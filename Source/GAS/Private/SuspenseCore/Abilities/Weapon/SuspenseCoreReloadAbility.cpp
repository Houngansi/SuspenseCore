// SuspenseCoreReloadAbility.cpp
// Tarkov-style reload ability with magazine management
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Weapon/SuspenseCoreReloadAbility.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreMagazineProvider.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreQuickSlotProvider.h"
#include "SuspenseCore/Interfaces/Inventory/ISuspenseCoreInventory.h"
#include "SuspenseCore/Types/Items/SuspenseCoreItemTypes.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreReload, Log, All);

#define RELOAD_LOG(Verbosity, Format, ...) \
    UE_LOG(LogSuspenseCoreReload, Verbosity, TEXT("[%s] " Format), *GetNameSafe(GetOwningActorFromActorInfo()), ##__VA_ARGS__)

//==================================================================
// Constructor
//==================================================================

USuspenseCoreReloadAbility::USuspenseCoreReloadAbility()
{
    // Input binding
    AbilityInputID = ESuspenseCoreAbilityInputID::Reload;

    // CRITICAL: AbilityTags for activation via TryActivateAbilitiesByTag()
    // Use SetAssetTags() instead of deprecated AbilityTags.AddTag()
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(SuspenseCoreTags::Ability::Weapon::Reload);
    SetAssetTags(AssetTags);

    // Default timing
    BaseTacticalReloadTime = 2.0f;
    BaseEmptyReloadTime = 2.5f;
    EmergencyReloadTimeMultiplier = 0.8f;
    ChamberOnlyTime = 0.8f;

    // Initialize runtime state
    CurrentReloadType = ESuspenseCoreReloadType::None;
    ReloadDuration = 0.0f;
    ReloadStartTime = 0.0f;
    bIsReloading = false;
    NewMagazineQuickSlotIndex = -1;

    // Ability configuration
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    bRetriggerInstancedAbility = false;

    // Network configuration (server-authoritative reload)
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;

    // Blocking tags - can't reload while doing these (using native tags for performance)
    ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Sprinting);
    ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Firing);
    ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Dead);
    ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Stunned);
    ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Disabled);

    // Tags applied while reloading
    ActivationOwnedTags.AddTag(SuspenseCoreTags::State::Reloading);

    // Cancel these abilities when reloading
    CancelAbilitiesWithTag.AddTag(SuspenseCoreTags::Ability::Sprint);
    CancelAbilitiesWithTag.AddTag(SuspenseCoreTags::Ability::Weapon::AimDownSight);

    // EventBus configuration
    bPublishAbilityEvents = true;
}

//==================================================================
// Runtime Accessors
//==================================================================

float USuspenseCoreReloadAbility::GetReloadProgress() const
{
    if (!bIsReloading || ReloadDuration <= 0.0f)
    {
        return 0.0f;
    }

    float ElapsedTime = GetWorld()->GetTimeSeconds() - ReloadStartTime;
    return FMath::Clamp(ElapsedTime / ReloadDuration, 0.0f, 1.0f);
}

//==================================================================
// GameplayAbility Interface
//==================================================================

bool USuspenseCoreReloadAbility::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    RELOAD_LOG(Verbose, TEXT("CanActivateAbility: Starting validation"));

    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        RELOAD_LOG(Verbose, TEXT("CanActivateAbility: Super check failed (blocking tags)"));
        return false;
    }

    // Check if already reloading
    if (bIsReloading)
    {
        RELOAD_LOG(Verbose, TEXT("CanActivateAbility: Already reloading"));
        return false;
    }

    // Get magazine provider interface
    ISuspenseCoreMagazineProvider* MagProvider = GetMagazineProvider();
    if (!MagProvider)
    {
        RELOAD_LOG(Warning, TEXT("CanActivateAbility: No MagazineProvider found"));
        return false;
    }

    // Determine what kind of reload is possible
    ESuspenseCoreReloadType PossibleReload = DetermineReloadType();
    if (PossibleReload == ESuspenseCoreReloadType::None)
    {
        RELOAD_LOG(Verbose, TEXT("CanActivateAbility: No valid reload type available"));
        return false;
    }

    RELOAD_LOG(Verbose, TEXT("CanActivateAbility: Passed, ReloadType=%d"), static_cast<int32>(PossibleReload));
    return true;
}

void USuspenseCoreReloadAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Cache for later use
    CachedActorInfo = ActorInfo;
    CachedSpecHandle = Handle;
    CachedActivationInfo = ActivationInfo;

    // Determine reload type
    CurrentReloadType = DetermineReloadType();
    if (CurrentReloadType == ESuspenseCoreReloadType::None)
    {
        RELOAD_LOG(Warning, TEXT("ActivateAbility: No valid reload type"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Find magazine to use
    if (CurrentReloadType != ESuspenseCoreReloadType::ChamberOnly)
    {
        if (!FindBestMagazine(NewMagazineQuickSlotIndex, NewMagazine))
        {
            RELOAD_LOG(Warning, TEXT("ActivateAbility: No magazine available for reload"));
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }
    }

    // Calculate reload duration
    ReloadDuration = CalculateReloadDuration(CurrentReloadType);
    ReloadStartTime = GetWorld()->GetTimeSeconds();
    bIsReloading = true;

    // Apply effects
    ApplyReloadEffects(ActorInfo);

    // Play montage
    if (!PlayReloadMontage())
    {
        RELOAD_LOG(Warning, TEXT("ActivateAbility: Failed to play reload montage"));
        RemoveReloadEffects(ActorInfo);
        bIsReloading = false;
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Broadcast reload started
    BroadcastReloadStarted();

    RELOAD_LOG(Log, TEXT("Reload started: Type=%d, Duration=%.2f"),
        static_cast<int32>(CurrentReloadType), ReloadDuration);

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void USuspenseCoreReloadAbility::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    // Clean up
    RemoveReloadEffects(ActorInfo);
    StopReloadMontage();

    if (bWasCancelled && bIsReloading)
    {
        BroadcastReloadCancelled();
        RELOAD_LOG(Log, TEXT("Reload cancelled"));
    }

    bIsReloading = false;
    CurrentReloadType = ESuspenseCoreReloadType::None;

    // Clear cached providers (weapon may change between reloads)
    CachedMagazineProvider.Reset();
    CachedQuickSlotProvider.Reset();
    CachedInventoryProvider.Reset();

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

//==================================================================
// Reload Logic
//==================================================================

ESuspenseCoreReloadType USuspenseCoreReloadAbility::DetermineReloadType() const
{
    ISuspenseCoreMagazineProvider* MagProvider = GetMagazineProvider();
    if (!MagProvider)
    {
        return ESuspenseCoreReloadType::None;
    }

    // Get current weapon state via interface
    FSuspenseCoreWeaponAmmoState AmmoState = ISuspenseCoreMagazineProvider::Execute_GetAmmoState(Cast<UObject>(MagProvider));

    RELOAD_LOG(Verbose, TEXT("DetermineReloadType: HasMag=%d, Chambered=%d, MagAmmo=%d/%d"),
        AmmoState.bHasMagazine,
        AmmoState.ChamberedRound.IsChambered(),
        AmmoState.InsertedMagazine.CurrentRoundCount,
        AmmoState.InsertedMagazine.MaxCapacity);

    // Check if we need to chamber only
    if (!AmmoState.IsReadyToFire() && AmmoState.bHasMagazine && !AmmoState.IsMagazineEmpty())
    {
        return ESuspenseCoreReloadType::ChamberOnly;
    }

    // Check if we need to reload at all
    if (AmmoState.bHasMagazine && !AmmoState.IsMagazineEmpty())
    {
        // Magazine has ammo, might want tactical reload
        // Only allow if magazine is not full
        if (AmmoState.InsertedMagazine.IsFull())
        {
            return ESuspenseCoreReloadType::None;
        }
        return ESuspenseCoreReloadType::Tactical;
    }

    // Empty magazine or no magazine
    if (AmmoState.IsReadyToFire())
    {
        // Round in chamber, tactical reload
        return ESuspenseCoreReloadType::Tactical;
    }

    // No round in chamber, full reload
    return ESuspenseCoreReloadType::Empty;
}

float USuspenseCoreReloadAbility::CalculateReloadDuration(ESuspenseCoreReloadType ReloadType) const
{
    // Get MagazineProvider to calculate duration with proper modifiers from SSOT
    ISuspenseCoreMagazineProvider* MagProvider = GetMagazineProvider();
    if (MagProvider)
    {
        UObject* ProviderObj = Cast<UObject>(MagProvider);
        if (ProviderObj)
        {
            // Use MagazineProvider to get reload duration with:
            // - WeaponAttributeSet.TacticalReloadTime / FullReloadTime (from SSOT DataTable)
            // - MagazineData.ReloadTimeModifier (from magazine DataTable)
            float Duration = ISuspenseCoreMagazineProvider::Execute_CalculateReloadDuration(
                ProviderObj, ReloadType, NewMagazine);

            if (Duration > 0.0f)
            {
                RELOAD_LOG(Verbose, TEXT("CalculateReloadDuration via Provider: Type=%d, Duration=%.2f"),
                    static_cast<int32>(ReloadType), Duration);
                return Duration;
            }
        }
    }

    // Fallback to hardcoded defaults if MagazineProvider unavailable
    float BaseTime = 0.0f;

    switch (ReloadType)
    {
        case ESuspenseCoreReloadType::Tactical:
            BaseTime = BaseTacticalReloadTime;
            break;

        case ESuspenseCoreReloadType::Empty:
            BaseTime = BaseEmptyReloadTime;
            break;

        case ESuspenseCoreReloadType::Emergency:
            BaseTime = BaseTacticalReloadTime * EmergencyReloadTimeMultiplier;
            break;

        case ESuspenseCoreReloadType::ChamberOnly:
            BaseTime = ChamberOnlyTime;
            break;

        default:
            return 0.0f;
    }

    RELOAD_LOG(Verbose, TEXT("CalculateReloadDuration: Fallback, Type=%d, Duration=%.2f"),
        static_cast<int32>(ReloadType), BaseTime);

    return BaseTime;
}

UAnimMontage* USuspenseCoreReloadAbility::GetMontageForReloadType(ESuspenseCoreReloadType ReloadType) const
{
    UAnimMontage* Montage = nullptr;
    const TCHAR* MontageName = TEXT("Unknown");

    switch (ReloadType)
    {
        case ESuspenseCoreReloadType::Tactical:
            Montage = TacticalReloadMontage.Get();
            MontageName = TEXT("TacticalReloadMontage");
            break;

        case ESuspenseCoreReloadType::Empty:
            Montage = EmptyReloadMontage.Get();
            MontageName = TEXT("EmptyReloadMontage");
            break;

        case ESuspenseCoreReloadType::Emergency:
            Montage = EmergencyReloadMontage.Get();
            MontageName = TEXT("EmergencyReloadMontage");
            break;

        case ESuspenseCoreReloadType::ChamberOnly:
            Montage = ChamberOnlyMontage.Get();
            MontageName = TEXT("ChamberOnlyMontage");
            break;

        default:
            RELOAD_LOG(Warning, TEXT("GetMontageForReloadType: Unknown reload type %d"), static_cast<int32>(ReloadType));
            return nullptr;
    }

    if (!Montage)
    {
        RELOAD_LOG(Warning, TEXT("GetMontageForReloadType: '%s' is NOT SET in Blueprint! Set it in GA_ReloadAbility_C defaults."),
            MontageName);
    }

    return Montage;
}

bool USuspenseCoreReloadAbility::FindBestMagazine(int32& OutQuickSlotIndex, FSuspenseCoreMagazineInstance& OutMagazine) const
{
    // Get MagazineProvider for compatibility checking
    ISuspenseCoreMagazineProvider* MagProvider = GetMagazineProvider();
    UObject* MagProviderObj = MagProvider ? Cast<UObject>(MagProvider) : nullptr;

    // Reset inventory instance ID
    NewMagazineInventoryInstanceID = FGuid();

    //========================================================================
    // STEP 1: Search INVENTORY FIRST for compatible magazines with ammo
    // Per user request: "СПЕРВА ПРОВЕРЯЕТ ЧТО ЕСТЬ МАГАЗИНЫ НУЖНОГО ТИПА С ПАТРОНАМИ В ИНВЕНТАРЕ"
    //========================================================================
    ISuspenseCoreInventory* InventoryProvider = GetInventoryProvider();
    if (InventoryProvider)
    {
        // Get all item instances from inventory
        TArray<FSuspenseCoreItemInstance> AllItems = InventoryProvider->GetAllItemInstances();

        // Find best magazine in inventory (most ammo first)
        FSuspenseCoreItemInstance BestMagazineItem;
        int32 BestAmmoCount = 0;

        for (const FSuspenseCoreItemInstance& Item : AllItems)
        {
            // Check if this item is a magazine with ammo
            if (!Item.IsMagazine() || Item.MagazineData.CurrentRoundCount <= 0)
            {
                continue;
            }

            // Check if magazine is compatible with weapon caliber
            if (MagProviderObj)
            {
                const bool bCompatible = ISuspenseCoreMagazineProvider::Execute_IsMagazineCompatible(
                    MagProviderObj, Item.MagazineData);
                if (!bCompatible)
                {
                    RELOAD_LOG(Verbose, TEXT("Magazine in inventory (%s) not compatible with weapon caliber, skipping"),
                        *Item.MagazineData.MagazineID.ToString());
                    continue;
                }
            }

            // Prefer magazine with most ammo
            if (Item.MagazineData.CurrentRoundCount > BestAmmoCount)
            {
                BestAmmoCount = Item.MagazineData.CurrentRoundCount;
                BestMagazineItem = Item;
            }
        }

        // If found a compatible magazine in inventory, use it
        if (BestMagazineItem.IsValid() && BestMagazineItem.IsMagazine())
        {
            OutQuickSlotIndex = -1;  // -1 indicates from inventory, not QuickSlot
            OutMagazine = BestMagazineItem.MagazineData;
            NewMagazineInventoryInstanceID = BestMagazineItem.UniqueInstanceID;

            RELOAD_LOG(Log, TEXT("Found compatible magazine in INVENTORY: %s, %d rounds, InstanceID=%s"),
                *OutMagazine.MagazineID.ToString(),
                OutMagazine.CurrentRoundCount,
                *BestMagazineItem.UniqueInstanceID.ToString());
            return true;
        }
    }

    //========================================================================
    // STEP 2: If no magazine in inventory, fall back to QuickSlots
    // Per user request: "ЕСЛИ НЕТ ТАКИХ МАГАЗИНОВ ТАМ ТО ДЕЛАЕТ ПЕРЕЗАРЯДКУ ИЗ КВИК СЛОТОВ"
    //========================================================================
    ISuspenseCoreQuickSlotProvider* QuickSlotProvider = GetQuickSlotProvider();
    if (QuickSlotProvider)
    {
        UObject* ProviderObj = Cast<UObject>(QuickSlotProvider);

        // Look for first available COMPATIBLE magazine with ammo
        // CRITICAL: Check caliber compatibility per TarkovStyle_Ammo_System_Design.md
        for (int32 i = 0; i < 4; ++i)
        {
            FSuspenseCoreMagazineInstance Mag;
            if (ISuspenseCoreQuickSlotProvider::Execute_GetMagazineFromSlot(ProviderObj, i, Mag) && Mag.HasAmmo())
            {
                // Check if magazine is compatible with weapon caliber
                if (MagProviderObj)
                {
                    const bool bCompatible = ISuspenseCoreMagazineProvider::Execute_IsMagazineCompatible(MagProviderObj, Mag);
                    if (!bCompatible)
                    {
                        RELOAD_LOG(Verbose, TEXT("Magazine in QuickSlot %d (%s) not compatible with weapon caliber, skipping"),
                            i, *Mag.MagazineID.ToString());
                        continue;  // Skip incompatible magazines
                    }
                }

                OutQuickSlotIndex = i;
                OutMagazine = Mag;
                RELOAD_LOG(Log, TEXT("Found compatible magazine in QuickSlot %d: %s, %d rounds"),
                    i, *Mag.MagazineID.ToString(), Mag.CurrentRoundCount);
                return true;
            }
        }
    }

    OutQuickSlotIndex = -1;
    RELOAD_LOG(Verbose, TEXT("No suitable compatible magazine found in inventory or QuickSlots"));
    return false;
}

//==================================================================
// Animation Notify Handlers
//==================================================================

void USuspenseCoreReloadAbility::OnMagOutNotify()
{
    RELOAD_LOG(Verbose, TEXT("MagOut notify fired"));

    // Play magazine out sound
    PlayReloadSound(MagOutSound);

    ISuspenseCoreMagazineProvider* MagProvider = GetMagazineProvider();
    if (!MagProvider)
    {
        return;
    }

    UObject* ProviderObj = Cast<UObject>(MagProvider);

    // Eject current magazine
    bool bDropToGround = (CurrentReloadType == ESuspenseCoreReloadType::Emergency);
    EjectedMagazine = ISuspenseCoreMagazineProvider::Execute_EjectMagazine(ProviderObj, bDropToGround);

    // CRITICAL FIX: Don't store ejected magazine here!
    // Store it in OnMagInNotify AFTER ClearSlot() so it goes into the same slot
    // where the new magazine came from. This prevents magazine duplication.
    // See TarkovStyle_Ammo_System_Design.md Phase 7.
    if (bDropToGround)
    {
        // Emergency drop - magazine falls to ground, don't store
        EjectedMagazine = FSuspenseCoreMagazineInstance();
    }
}

void USuspenseCoreReloadAbility::OnMagInNotify()
{
    RELOAD_LOG(Verbose, TEXT("MagIn notify fired"));

    // Play magazine insert sound
    PlayReloadSound(MagInSound);

    ISuspenseCoreMagazineProvider* MagProvider = GetMagazineProvider();
    if (!MagProvider)
    {
        return;
    }

    UObject* ProviderObj = Cast<UObject>(MagProvider);
    ISuspenseCoreQuickSlotProvider* QuickSlotProvider = GetQuickSlotProvider();
    ISuspenseCoreInventory* InventoryProvider = GetInventoryProvider();

    // Insert new magazine
    if (NewMagazine.IsValid())
    {
        ISuspenseCoreMagazineProvider::Execute_InsertMagazine(ProviderObj, NewMagazine);

        // Remove from source: QuickSlot or Inventory
        if (NewMagazineQuickSlotIndex >= 0 && QuickSlotProvider)
        {
            // Magazine came from QuickSlot - clear that slot
            ISuspenseCoreQuickSlotProvider::Execute_ClearSlot(
                Cast<UObject>(QuickSlotProvider), NewMagazineQuickSlotIndex);
            RELOAD_LOG(Verbose, TEXT("Cleared magazine from QuickSlot %d"), NewMagazineQuickSlotIndex);
        }
        else if (NewMagazineInventoryInstanceID.IsValid() && InventoryProvider)
        {
            // Magazine came from Inventory - remove by instance ID
            bool bRemoved = InventoryProvider->RemoveItemInstance(NewMagazineInventoryInstanceID);
            RELOAD_LOG(Log, TEXT("Removed magazine from inventory (InstanceID=%s): %s"),
                *NewMagazineInventoryInstanceID.ToString(),
                bRemoved ? TEXT("SUCCESS") : TEXT("FAILED"));

            // Clear the inventory instance ID
            NewMagazineInventoryInstanceID = FGuid();
        }
    }

    // CRITICAL FIX: Store ejected magazine AFTER removing the source magazine
    // This ensures the ejected magazine goes into the same slot where new magazine came from
    // Prevents magazine duplication bug. See TarkovStyle_Ammo_System_Design.md Phase 7.
    if (EjectedMagazine.IsValid())
    {
        // Try to store in QuickSlot first (if came from QuickSlot)
        if (NewMagazineQuickSlotIndex >= 0 && QuickSlotProvider)
        {
            int32 StoredSlotIndex;
            ISuspenseCoreQuickSlotProvider::Execute_StoreEjectedMagazine(
                Cast<UObject>(QuickSlotProvider), EjectedMagazine, StoredSlotIndex);

            RELOAD_LOG(Log, TEXT("Stored ejected magazine (%d rounds) in QuickSlot %d"),
                EjectedMagazine.CurrentRoundCount, StoredSlotIndex);
        }
        // If came from inventory, try to add ejected magazine back to inventory
        else if (InventoryProvider)
        {
            // Create an item instance for the ejected magazine
            FSuspenseCoreItemInstance EjectedMagItem;
            EjectedMagItem.UniqueInstanceID = FGuid::NewGuid();
            EjectedMagItem.ItemID = EjectedMagazine.MagazineID;
            EjectedMagItem.Quantity = 1;
            EjectedMagItem.MagazineData = EjectedMagazine;

            bool bAdded = ISuspenseCoreInventory::Execute_AddItemInstance(
                Cast<UObject>(InventoryProvider), EjectedMagItem);

            RELOAD_LOG(Log, TEXT("Added ejected magazine (%d rounds) to inventory: %s"),
                EjectedMagazine.CurrentRoundCount,
                bAdded ? TEXT("SUCCESS") : TEXT("FAILED - inventory might be full"));
        }

        // Clear reference
        EjectedMagazine = FSuspenseCoreMagazineInstance();
    }
}

void USuspenseCoreReloadAbility::OnRackStartNotify()
{
    RELOAD_LOG(Verbose, TEXT("RackStart notify fired"));

    // Play chamber/rack sound at start of chambering
    PlayReloadSound(ChamberSound);
}

void USuspenseCoreReloadAbility::OnRackEndNotify()
{
    RELOAD_LOG(Verbose, TEXT("RackEnd notify fired"));

    ISuspenseCoreMagazineProvider* MagProvider = GetMagazineProvider();
    if (!MagProvider)
    {
        return;
    }

    // Chamber a round
    ISuspenseCoreMagazineProvider::Execute_ChamberRound(Cast<UObject>(MagProvider));
}

void USuspenseCoreReloadAbility::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (bInterrupted)
    {
        // Reload was interrupted
        EndAbility(CachedSpecHandle, CachedActorInfo, CachedActivationInfo, true, true);
    }
    else
    {
        // Reload completed successfully
        // CRITICAL FIX: Execute magazine swap directly if AnimNotifies didn't fire
        // This ensures magazine insertion happens even without MagOut/MagIn notifies in montage
        ExecuteReloadOnMontageComplete();

        BroadcastReloadCompleted();
        RELOAD_LOG(Log, TEXT("Reload completed successfully"));
        EndAbility(CachedSpecHandle, CachedActorInfo, CachedActivationInfo, true, false);
    }
}

void USuspenseCoreReloadAbility::ExecuteReloadOnMontageComplete()
{
    // Skip if NewMagazine was already consumed by OnMagInNotify
    if (!NewMagazine.IsValid())
    {
        RELOAD_LOG(Verbose, TEXT("ExecuteReloadOnMontageComplete: NewMagazine already consumed by AnimNotify"));
        return;
    }

    ISuspenseCoreMagazineProvider* MagProvider = GetMagazineProvider();
    if (!MagProvider)
    {
        RELOAD_LOG(Warning, TEXT("ExecuteReloadOnMontageComplete: No MagazineProvider!"));
        return;
    }

    UObject* ProviderObj = Cast<UObject>(MagProvider);
    ISuspenseCoreQuickSlotProvider* QuickSlotProvider = GetQuickSlotProvider();
    ISuspenseCoreInventory* InventoryProvider = GetInventoryProvider();

    RELOAD_LOG(Log, TEXT("ExecuteReloadOnMontageComplete: AnimNotifies didn't fire, executing magazine swap directly"));

    // Step 1: Eject current magazine (if not ChamberOnly reload)
    if (CurrentReloadType != ESuspenseCoreReloadType::ChamberOnly)
    {
        bool bDropToGround = (CurrentReloadType == ESuspenseCoreReloadType::Emergency);
        EjectedMagazine = ISuspenseCoreMagazineProvider::Execute_EjectMagazine(ProviderObj, bDropToGround);

        if (bDropToGround)
        {
            EjectedMagazine = FSuspenseCoreMagazineInstance();
        }
    }

    // Step 2: Insert new magazine
    ISuspenseCoreMagazineProvider::Execute_InsertMagazine(ProviderObj, NewMagazine);
    RELOAD_LOG(Log, TEXT("ExecuteReloadOnMontageComplete: Inserted magazine %s with %d rounds"),
        *NewMagazine.MagazineID.ToString(), NewMagazine.CurrentRoundCount);

    // Step 3: Remove magazine from source (QuickSlot or Inventory)
    if (NewMagazineQuickSlotIndex >= 0 && QuickSlotProvider)
    {
        ISuspenseCoreQuickSlotProvider::Execute_ClearSlot(
            Cast<UObject>(QuickSlotProvider), NewMagazineQuickSlotIndex);
        RELOAD_LOG(Log, TEXT("ExecuteReloadOnMontageComplete: Cleared QuickSlot %d"), NewMagazineQuickSlotIndex);
    }
    else if (NewMagazineInventoryInstanceID.IsValid() && InventoryProvider)
    {
        bool bRemoved = InventoryProvider->RemoveItemInstance(NewMagazineInventoryInstanceID);
        RELOAD_LOG(Log, TEXT("ExecuteReloadOnMontageComplete: Removed from inventory (ID=%s): %s"),
            *NewMagazineInventoryInstanceID.ToString(), bRemoved ? TEXT("SUCCESS") : TEXT("FAILED"));
        NewMagazineInventoryInstanceID = FGuid();
    }

    // Step 4: Store ejected magazine
    if (EjectedMagazine.IsValid())
    {
        if (NewMagazineQuickSlotIndex >= 0 && QuickSlotProvider)
        {
            int32 StoredSlotIndex;
            ISuspenseCoreQuickSlotProvider::Execute_StoreEjectedMagazine(
                Cast<UObject>(QuickSlotProvider), EjectedMagazine, StoredSlotIndex);
            RELOAD_LOG(Log, TEXT("ExecuteReloadOnMontageComplete: Stored ejected mag in QuickSlot %d"), StoredSlotIndex);
        }
        else if (InventoryProvider)
        {
            FSuspenseCoreItemInstance EjectedMagItem;
            EjectedMagItem.UniqueInstanceID = FGuid::NewGuid();
            EjectedMagItem.ItemID = EjectedMagazine.MagazineID;
            EjectedMagItem.Quantity = 1;
            EjectedMagItem.MagazineData = EjectedMagazine;

            ISuspenseCoreInventory::Execute_AddItemInstance(
                Cast<UObject>(InventoryProvider), EjectedMagItem);
        }
        EjectedMagazine = FSuspenseCoreMagazineInstance();
    }

    // Step 5: Chamber a round (for non-tactical reloads or if not already chambered)
    FSuspenseCoreWeaponAmmoState AmmoState = ISuspenseCoreMagazineProvider::Execute_GetAmmoState(ProviderObj);
    if (!AmmoState.ChamberedRound.IsChambered() && !AmmoState.IsMagazineEmpty())
    {
        ISuspenseCoreMagazineProvider::Execute_ChamberRound(ProviderObj);
        RELOAD_LOG(Log, TEXT("ExecuteReloadOnMontageComplete: Chambered a round"));
    }

    // Step 6: Publish ammo changed event for UI update
    USuspenseCoreEventBus* EventBus = GetEventBus();
    if (EventBus)
    {
        // Get fresh ammo state after reload
        FSuspenseCoreWeaponAmmoState FinalState = ISuspenseCoreMagazineProvider::Execute_GetAmmoState(ProviderObj);

        FSuspenseCoreEventData EventData;
        EventData.SetInt(TEXT("CurrentRounds"), FinalState.InsertedMagazine.CurrentRoundCount);
        EventData.SetBool(TEXT("HasChamberedRound"), FinalState.ChamberedRound.IsChambered());
        EventData.SetInt(TEXT("MagazineCapacity"), FinalState.InsertedMagazine.MaxCapacity);
        EventData.SetBool(TEXT("HasMagazine"), FinalState.bHasMagazine);

        EventBus->Publish(SuspenseCoreTags::Event::Weapon::AmmoChanged, EventData);
        RELOAD_LOG(Log, TEXT("ExecuteReloadOnMontageComplete: Published AmmoChanged event (Rounds=%d, Chambered=%s)"),
            FinalState.InsertedMagazine.CurrentRoundCount,
            FinalState.ChamberedRound.IsChambered() ? TEXT("YES") : TEXT("NO"));
    }

    // Mark magazine as consumed
    NewMagazine = FSuspenseCoreMagazineInstance();
}

void USuspenseCoreReloadAbility::OnMontageBlendOut(UAnimMontage* Montage, bool bInterrupted)
{
    // Optional: Handle blend out if needed
}

//==================================================================
// Internal Methods
//==================================================================

ISuspenseCoreMagazineProvider* USuspenseCoreReloadAbility::GetMagazineProvider() const
{
    // Use cached provider if valid
    if (CachedMagazineProvider.IsValid())
    {
        return Cast<ISuspenseCoreMagazineProvider>(CachedMagazineProvider.Get());
    }

    AActor* AvatarActor = GetAvatarActorFromActorInfo();
    if (AvatarActor)
    {
        // Check if avatar implements interface
        if (AvatarActor->GetClass()->ImplementsInterface(USuspenseCoreMagazineProvider::StaticClass()))
        {
            CachedMagazineProvider = AvatarActor;
            return Cast<ISuspenseCoreMagazineProvider>(AvatarActor);
        }

        // Check components on avatar
        TArray<UActorComponent*> Components;
        AvatarActor->GetComponents(Components);
        for (UActorComponent* Comp : Components)
        {
            if (Comp && Comp->GetClass()->ImplementsInterface(USuspenseCoreMagazineProvider::StaticClass()))
            {
                CachedMagazineProvider = Comp;
                return Cast<ISuspenseCoreMagazineProvider>(Comp);
            }
        }

        // Check attached actors (weapons!)
        TArray<AActor*> AttachedActors;
        AvatarActor->GetAttachedActors(AttachedActors);
        for (AActor* Attached : AttachedActors)
        {
            if (!Attached)
            {
                continue;
            }

            // Check actor itself
            if (Attached->GetClass()->ImplementsInterface(USuspenseCoreMagazineProvider::StaticClass()))
            {
                CachedMagazineProvider = Attached;
                return Cast<ISuspenseCoreMagazineProvider>(Attached);
            }

            // Check its components
            TArray<UActorComponent*> AttachedComps;
            Attached->GetComponents(AttachedComps);
            for (UActorComponent* Comp : AttachedComps)
            {
                if (Comp && Comp->GetClass()->ImplementsInterface(USuspenseCoreMagazineProvider::StaticClass()))
                {
                    CachedMagazineProvider = Comp;
                    return Cast<ISuspenseCoreMagazineProvider>(Comp);
                }
            }
        }
    }

    // Check owner actor
    AActor* OwnerActor = GetOwningActorFromActorInfo();
    if (OwnerActor && OwnerActor != AvatarActor)
    {
        TArray<UActorComponent*> Components;
        OwnerActor->GetComponents(Components);
        for (UActorComponent* Comp : Components)
        {
            if (Comp && Comp->GetClass()->ImplementsInterface(USuspenseCoreMagazineProvider::StaticClass()))
            {
                CachedMagazineProvider = Comp;
                return Cast<ISuspenseCoreMagazineProvider>(Comp);
            }
        }
    }

    RELOAD_LOG(Warning, TEXT("GetMagazineProvider: Not found"));
    return nullptr;
}

ISuspenseCoreQuickSlotProvider* USuspenseCoreReloadAbility::GetQuickSlotProvider() const
{
    // Use cached provider if valid
    if (CachedQuickSlotProvider.IsValid())
    {
        return Cast<ISuspenseCoreQuickSlotProvider>(CachedQuickSlotProvider.Get());
    }

    AActor* OwnerActor = GetOwningActorFromActorInfo();
    if (!OwnerActor)
    {
        return nullptr;
    }

    // Check if owner implements interface
    if (OwnerActor->GetClass()->ImplementsInterface(USuspenseCoreQuickSlotProvider::StaticClass()))
    {
        CachedQuickSlotProvider = OwnerActor;
        return Cast<ISuspenseCoreQuickSlotProvider>(OwnerActor);
    }

    // Check components
    TArray<UActorComponent*> Components;
    OwnerActor->GetComponents(Components);
    for (UActorComponent* Comp : Components)
    {
        if (Comp && Comp->GetClass()->ImplementsInterface(USuspenseCoreQuickSlotProvider::StaticClass()))
        {
            CachedQuickSlotProvider = Comp;
            return Cast<ISuspenseCoreQuickSlotProvider>(Comp);
        }
    }

    return nullptr;
}

ISuspenseCoreInventory* USuspenseCoreReloadAbility::GetInventoryProvider() const
{
    // Use cached provider if valid
    if (CachedInventoryProvider.IsValid())
    {
        return Cast<ISuspenseCoreInventory>(CachedInventoryProvider.Get());
    }

    AActor* OwnerActor = GetOwningActorFromActorInfo();
    if (!OwnerActor)
    {
        return nullptr;
    }

    // Check if owner implements interface
    if (OwnerActor->GetClass()->ImplementsInterface(USuspenseCoreInventory::StaticClass()))
    {
        CachedInventoryProvider = OwnerActor;
        return Cast<ISuspenseCoreInventory>(OwnerActor);
    }

    // Check components
    TArray<UActorComponent*> Components;
    OwnerActor->GetComponents(Components);
    for (UActorComponent* Comp : Components)
    {
        if (Comp && Comp->GetClass()->ImplementsInterface(USuspenseCoreInventory::StaticClass()))
        {
            CachedInventoryProvider = Comp;
            return Cast<ISuspenseCoreInventory>(Comp);
        }
    }

    return nullptr;
}

void USuspenseCoreReloadAbility::ApplyReloadEffects(const FGameplayAbilityActorInfo* ActorInfo)
{
    if (ReloadSpeedDebuffClass && ActorInfo->AbilitySystemComponent.IsValid())
    {
        FGameplayEffectContextHandle EffectContext = ActorInfo->AbilitySystemComponent->MakeEffectContext();
        EffectContext.AddSourceObject(this);

        FGameplayEffectSpecHandle SpecHandle = ActorInfo->AbilitySystemComponent->MakeOutgoingSpec(
            ReloadSpeedDebuffClass, 1.0f, EffectContext);

        if (SpecHandle.IsValid())
        {
            ReloadSpeedEffectHandle = ActorInfo->AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }
    }
}

void USuspenseCoreReloadAbility::RemoveReloadEffects(const FGameplayAbilityActorInfo* ActorInfo)
{
    if (ReloadSpeedEffectHandle.IsValid() && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        ActorInfo->AbilitySystemComponent->RemoveActiveGameplayEffect(ReloadSpeedEffectHandle);
        ReloadSpeedEffectHandle.Invalidate();
    }
}

bool USuspenseCoreReloadAbility::PlayReloadMontage()
{
    UAnimMontage* Montage = GetMontageForReloadType(CurrentReloadType);
    if (!Montage)
    {
        RELOAD_LOG(Warning, TEXT("PlayReloadMontage: No montage for reload type %d"), static_cast<int32>(CurrentReloadType));
        return false;
    }

    RELOAD_LOG(Log, TEXT("PlayReloadMontage: Got montage '%s' for reload type %d"),
        *Montage->GetName(), static_cast<int32>(CurrentReloadType));

    AActor* AvatarActor = GetAvatarActorFromActorInfo();
    ACharacter* Character = Cast<ACharacter>(AvatarActor);
    if (!Character)
    {
        RELOAD_LOG(Warning, TEXT("PlayReloadMontage: AvatarActor '%s' is not ACharacter!"),
            AvatarActor ? *AvatarActor->GetName() : TEXT("NULL"));
        return false;
    }

    USkeletalMeshComponent* MeshComp = Character->GetMesh();
    if (!MeshComp)
    {
        RELOAD_LOG(Warning, TEXT("PlayReloadMontage: Character->GetMesh() returned NULL!"));
        return false;
    }

    UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
    if (!AnimInstance)
    {
        // Try to find AnimInstance on other skeletal mesh components (MetaHuman Body)
        RELOAD_LOG(Warning, TEXT("PlayReloadMontage: Primary mesh '%s' has no AnimInstance, searching other components..."),
            *MeshComp->GetName());

        TArray<USkeletalMeshComponent*> SkeletalMeshes;
        Character->GetComponents<USkeletalMeshComponent>(SkeletalMeshes);

        for (USkeletalMeshComponent* SMC : SkeletalMeshes)
        {
            if (SMC && SMC != MeshComp && SMC->GetAnimInstance())
            {
                AnimInstance = SMC->GetAnimInstance();
                RELOAD_LOG(Log, TEXT("PlayReloadMontage: Found AnimInstance on component '%s'"), *SMC->GetName());
                break;
            }
        }

        if (!AnimInstance)
        {
            RELOAD_LOG(Warning, TEXT("PlayReloadMontage: No AnimInstance found on any skeletal mesh!"));
            return false;
        }
    }

    // Calculate play rate to match desired duration
    float MontageLength = Montage->GetPlayLength();
    float PlayRate = (ReloadDuration > 0.0f) ? (MontageLength / ReloadDuration) : 1.0f;

    RELOAD_LOG(Log, TEXT("PlayReloadMontage: Playing montage. Length=%.2f, Duration=%.2f, PlayRate=%.2f"),
        MontageLength, ReloadDuration, PlayRate);

    // Play montage
    float Duration = AnimInstance->Montage_Play(Montage, PlayRate);
    if (Duration <= 0.0f)
    {
        RELOAD_LOG(Warning, TEXT("PlayReloadMontage: Montage_Play returned %.2f (failed). AnimInstance='%s', Montage='%s'"),
            Duration, *AnimInstance->GetClass()->GetName(), *Montage->GetName());
        return false;
    }

    RELOAD_LOG(Log, TEXT("PlayReloadMontage: SUCCESS! Duration=%.2f"), Duration);

    // Bind to montage end
    FOnMontageEnded EndDelegate;
    EndDelegate.BindUObject(this, &USuspenseCoreReloadAbility::OnMontageEnded);
    AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);

    // Bind to blend out
    FOnMontageBlendingOutStarted BlendOutDelegate;
    BlendOutDelegate.BindUObject(this, &USuspenseCoreReloadAbility::OnMontageBlendOut);
    AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, Montage);

    return true;
}

void USuspenseCoreReloadAbility::StopReloadMontage()
{
    ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!Character)
    {
        return;
    }

    UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
    if (!AnimInstance)
    {
        return;
    }

    UAnimMontage* Montage = GetMontageForReloadType(CurrentReloadType);
    if (Montage && AnimInstance->Montage_IsPlaying(Montage))
    {
        AnimInstance->Montage_Stop(0.2f, Montage);
    }
}

void USuspenseCoreReloadAbility::BroadcastReloadStarted()
{
    // Play reload start sound
    PlayReloadSound(ReloadStartSound);

    ISuspenseCoreMagazineProvider* MagProvider = GetMagazineProvider();
    if (MagProvider)
    {
        ISuspenseCoreMagazineProvider::Execute_NotifyReloadStateChanged(
            Cast<UObject>(MagProvider), true, CurrentReloadType, ReloadDuration);
    }

    // EventBus broadcast (using native tags)
    PublishSimpleEvent(SuspenseCoreTags::Event::Weapon::ReloadStarted);
}

void USuspenseCoreReloadAbility::BroadcastReloadCompleted()
{
    // Play reload complete sound
    PlayReloadSound(ReloadCompleteSound);

    ISuspenseCoreMagazineProvider* MagProvider = GetMagazineProvider();
    if (MagProvider)
    {
        ISuspenseCoreMagazineProvider::Execute_NotifyReloadStateChanged(
            Cast<UObject>(MagProvider), false, CurrentReloadType, 0.0f);
    }

    // Publish EventBus events for UI and other systems
    if (USuspenseCoreEventBus* EventBus = GetEventBus())
    {
        // Broadcast reload completed event
        FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetAvatarActorFromActorInfo());
        EventData.SetInt(TEXT("ReloadType"), static_cast<int32>(CurrentReloadType));
        EventData.SetFloat(TEXT("Duration"), ReloadDuration);
        EventBus->Publish(SuspenseCoreTags::Event::Weapon::ReloadCompleted, EventData);

        // Broadcast spread reset after reload (weapon is now stable)
        // This allows UI to show reduced spread indicator and resets recoil accumulation
        FSuspenseCoreEventData SpreadEventData = FSuspenseCoreEventData::Create(GetAvatarActorFromActorInfo());
        SpreadEventData.SetFloat(TEXT("Spread"), 0.0f);
        SpreadEventData.SetBool(TEXT("IsReloadReset"), true);
        EventBus->Publish(SuspenseCoreTags::Event::Weapon::SpreadChanged, SpreadEventData);
    }
}

void USuspenseCoreReloadAbility::BroadcastReloadCancelled()
{
    ISuspenseCoreMagazineProvider* MagProvider = GetMagazineProvider();
    if (MagProvider)
    {
        ISuspenseCoreMagazineProvider::Execute_NotifyReloadStateChanged(
            Cast<UObject>(MagProvider), false, ESuspenseCoreReloadType::None, 0.0f);
    }
}

//==================================================================
// Network RPCs
//==================================================================

bool USuspenseCoreReloadAbility::Server_ExecuteReload_Validate(
    ESuspenseCoreReloadType ReloadType,
    int32 MagazineSlotIndex,
    const FSuspenseCoreMagazineInstance& Magazine)
{
    // Basic validation
    if (ReloadType == ESuspenseCoreReloadType::None)
    {
        return false;
    }

    // Validate magazine has ammo (unless chamber only)
    if (ReloadType != ESuspenseCoreReloadType::ChamberOnly && !Magazine.HasAmmo())
    {
        return false;
    }

    return true;
}

void USuspenseCoreReloadAbility::Server_ExecuteReload_Implementation(
    ESuspenseCoreReloadType ReloadType,
    int32 MagazineSlotIndex,
    const FSuspenseCoreMagazineInstance& Magazine)
{
    // Server validates and executes the reload
    ISuspenseCoreMagazineProvider* MagProvider = GetMagazineProvider();
    if (!MagProvider)
    {
        RELOAD_LOG(Warning, TEXT("Server_ExecuteReload: No MagazineProvider"));
        return;
    }

    UObject* ProviderObj = Cast<UObject>(MagProvider);

    // Execute magazine swap on server
    if (ReloadType != ESuspenseCoreReloadType::ChamberOnly)
    {
        // Eject current magazine
        bool bDropToGround = (ReloadType == ESuspenseCoreReloadType::Emergency);
        ISuspenseCoreMagazineProvider::Execute_EjectMagazine(ProviderObj, bDropToGround);

        // Insert new magazine
        ISuspenseCoreMagazineProvider::Execute_InsertMagazine(ProviderObj, Magazine);
    }

    // Chamber round if needed
    FSuspenseCoreWeaponAmmoState AmmoState = ISuspenseCoreMagazineProvider::Execute_GetAmmoState(ProviderObj);
    if (!AmmoState.ChamberedRound.IsChambered() && !AmmoState.IsMagazineEmpty())
    {
        ISuspenseCoreMagazineProvider::Execute_ChamberRound(ProviderObj);
    }

    // NOTE: Removed Multicast_PlayReloadEffects - GameplayAbilities don't replicate to Simulated Proxies.
    // Animation replication is handled automatically by UE's AnimMontage replication system.
    // Sounds play locally via notify handlers on the client running the ability.

    RELOAD_LOG(Log, TEXT("Server_ExecuteReload: Completed, Type=%d"), static_cast<int32>(ReloadType));
}

void USuspenseCoreReloadAbility::PlayReloadSound(USoundBase* Sound)
{
    if (!Sound)
    {
        return;
    }

    AActor* AvatarActor = GetAvatarActorFromActorInfo();
    if (!AvatarActor)
    {
        return;
    }

    // Play sound at character location (3D positional audio)
    UGameplayStatics::PlaySoundAtLocation(
        AvatarActor,
        Sound,
        AvatarActor->GetActorLocation(),
        AvatarActor->GetActorRotation(),
        1.0f,  // Volume multiplier
        1.0f,  // Pitch multiplier
        0.0f,  // Start time
        nullptr,  // Attenuation settings (use default from SoundBase)
        nullptr,  // Concurrency settings
        AvatarActor  // Owning actor
    );

    RELOAD_LOG(Verbose, TEXT("PlayReloadSound: %s"), *Sound->GetName());
}

#undef RELOAD_LOG
