// SuspenseCoreReloadAbility.cpp
// Tarkov-style reload ability with magazine management
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Weapon/SuspenseCoreReloadAbility.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreMagazineProvider.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreQuickSlotProvider.h"
#include "SuspenseCore/Interfaces/Inventory/ISuspenseCoreInventory.h"
#include "SuspenseCore/Types/Items/SuspenseCoreItemTypes.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"

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
    RELOAD_LOG(Warning, TEXT("═══════════════════════════════════════════════════════════════"));
    RELOAD_LOG(Warning, TEXT("CanActivateAbility CHECK STARTED"));

    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        RELOAD_LOG(Warning, TEXT("  ❌ FAILED: Super::CanActivateAbility returned false (check blocking tags)"));
        if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
        {
            FGameplayTagContainer OwnedTags;
            ActorInfo->AbilitySystemComponent->GetOwnedGameplayTags(OwnedTags);
            RELOAD_LOG(Warning, TEXT("  ASC Owned Tags: %s"), *OwnedTags.ToString());
        }
        return false;
    }
    RELOAD_LOG(Warning, TEXT("  ✓ Super::CanActivateAbility passed"));

    // Check if already reloading
    if (bIsReloading)
    {
        RELOAD_LOG(Warning, TEXT("  ❌ FAILED: Already reloading (bIsReloading=true)"));
        return false;
    }
    RELOAD_LOG(Warning, TEXT("  ✓ Not currently reloading"));

    // Get magazine provider interface
    ISuspenseCoreMagazineProvider* MagProvider = const_cast<USuspenseCoreReloadAbility*>(this)->GetMagazineProvider();
    if (!MagProvider)
    {
        RELOAD_LOG(Warning, TEXT("  ❌ FAILED: No MagazineProvider found!"));
        RELOAD_LOG(Warning, TEXT("    Avatar: %s"), *GetNameSafe(GetAvatarActorFromActorInfo()));
        RELOAD_LOG(Warning, TEXT("    Owner: %s"), *GetNameSafe(GetOwningActorFromActorInfo()));
        return false;
    }
    RELOAD_LOG(Warning, TEXT("  ✓ MagazineProvider found: %s"), *GetNameSafe(Cast<UObject>(MagProvider)));

    // Determine what kind of reload is possible
    ESuspenseCoreReloadType PossibleReload = DetermineReloadType();
    if (PossibleReload == ESuspenseCoreReloadType::None)
    {
        RELOAD_LOG(Warning, TEXT("  ❌ FAILED: No valid reload type available"));
        return false;
    }
    RELOAD_LOG(Warning, TEXT("  ✓ Reload type determined: %d"), static_cast<int32>(PossibleReload));
    RELOAD_LOG(Warning, TEXT("CanActivateAbility CHECK PASSED"));
    RELOAD_LOG(Warning, TEXT("═══════════════════════════════════════════════════════════════"));

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

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

//==================================================================
// Reload Logic
//==================================================================

ESuspenseCoreReloadType USuspenseCoreReloadAbility::DetermineReloadType() const
{
    RELOAD_LOG(Warning, TEXT("  [DetermineReloadType] Analyzing weapon state..."));

    ISuspenseCoreMagazineProvider* MagProvider = const_cast<USuspenseCoreReloadAbility*>(this)->GetMagazineProvider();
    if (!MagProvider)
    {
        RELOAD_LOG(Warning, TEXT("    ❌ No MagazineProvider - returning None"));
        return ESuspenseCoreReloadType::None;
    }

    // Get current weapon state via interface
    FSuspenseCoreWeaponAmmoState AmmoState = ISuspenseCoreMagazineProvider::Execute_GetAmmoState(Cast<UObject>(MagProvider));

    RELOAD_LOG(Warning, TEXT("    AmmoState:"));
    RELOAD_LOG(Warning, TEXT("      bHasMagazine: %s"), AmmoState.bHasMagazine ? TEXT("YES") : TEXT("NO"));
    RELOAD_LOG(Warning, TEXT("      IsChambered: %s"), AmmoState.ChamberedRound.IsChambered() ? TEXT("YES") : TEXT("NO"));
    RELOAD_LOG(Warning, TEXT("      IsReadyToFire: %s"), AmmoState.IsReadyToFire() ? TEXT("YES") : TEXT("NO"));
    RELOAD_LOG(Warning, TEXT("      IsMagazineEmpty: %s"), AmmoState.IsMagazineEmpty() ? TEXT("YES") : TEXT("NO"));
    RELOAD_LOG(Warning, TEXT("      Magazine Ammo: %d / %d"), AmmoState.InsertedMagazine.CurrentRoundCount, AmmoState.InsertedMagazine.MaxCapacity);

    // Check if we need to chamber only
    if (!AmmoState.IsReadyToFire() && AmmoState.bHasMagazine && !AmmoState.IsMagazineEmpty())
    {
        RELOAD_LOG(Warning, TEXT("    → Result: ChamberOnly (mag has ammo but not chambered)"));
        return ESuspenseCoreReloadType::ChamberOnly;
    }

    // Check if we need to reload at all
    if (AmmoState.bHasMagazine && !AmmoState.IsMagazineEmpty())
    {
        // Magazine has ammo, might want tactical reload
        // Only allow if magazine is not full
        if (AmmoState.InsertedMagazine.IsFull())
        {
            RELOAD_LOG(Warning, TEXT("    → Result: None (magazine is FULL - no reload needed)"));
            return ESuspenseCoreReloadType::None;
        }

        RELOAD_LOG(Warning, TEXT("    → Result: Tactical (mag has ammo but not full)"));
        return ESuspenseCoreReloadType::Tactical;
    }

    // Empty magazine or no magazine
    if (AmmoState.IsReadyToFire())
    {
        // Round in chamber, tactical reload
        RELOAD_LOG(Warning, TEXT("    → Result: Tactical (empty mag but round chambered)"));
        return ESuspenseCoreReloadType::Tactical;
    }
    else
    {
        // No round in chamber, full reload
        RELOAD_LOG(Warning, TEXT("    → Result: Empty (no mag/empty + not chambered)"));
        return ESuspenseCoreReloadType::Empty;
    }
}

float USuspenseCoreReloadAbility::CalculateReloadDuration(ESuspenseCoreReloadType ReloadType) const
{
    // Get MagazineProvider to calculate duration with proper modifiers from SSOT
    ISuspenseCoreMagazineProvider* MagProvider = const_cast<USuspenseCoreReloadAbility*>(this)->GetMagazineProvider();
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

    RELOAD_LOG(Warning, TEXT("CalculateReloadDuration: Using fallback defaults (no MagazineProvider), Type=%d, Duration=%.2f"),
        static_cast<int32>(ReloadType), BaseTime);

    return BaseTime;
}

UAnimMontage* USuspenseCoreReloadAbility::GetMontageForReloadType(ESuspenseCoreReloadType ReloadType) const
{
    switch (ReloadType)
    {
        case ESuspenseCoreReloadType::Tactical:
            return TacticalReloadMontage.Get();

        case ESuspenseCoreReloadType::Empty:
            return EmptyReloadMontage.Get();

        case ESuspenseCoreReloadType::Emergency:
            return EmergencyReloadMontage.Get();

        case ESuspenseCoreReloadType::ChamberOnly:
            return ChamberOnlyMontage.Get();

        default:
            return nullptr;
    }
}

bool USuspenseCoreReloadAbility::FindBestMagazine(int32& OutQuickSlotIndex, FSuspenseCoreMagazineInstance& OutMagazine) const
{
    // Get MagazineProvider for compatibility checking
    ISuspenseCoreMagazineProvider* MagProvider = const_cast<USuspenseCoreReloadAbility*>(this)->GetMagazineProvider();
    UObject* MagProviderObj = MagProvider ? Cast<UObject>(MagProvider) : nullptr;

    // Reset inventory instance ID
    const_cast<USuspenseCoreReloadAbility*>(this)->NewMagazineInventoryInstanceID = FGuid();

    //========================================================================
    // STEP 1: Search INVENTORY FIRST for compatible magazines with ammo
    // Per user request: "СПЕРВА ПРОВЕРЯЕТ ЧТО ЕСТЬ МАГАЗИНЫ НУЖНОГО ТИПА С ПАТРОНАМИ В ИНВЕНТАРЕ"
    //========================================================================
    ISuspenseCoreInventory* InventoryProvider = const_cast<USuspenseCoreReloadAbility*>(this)->GetInventoryProvider();
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
            const_cast<USuspenseCoreReloadAbility*>(this)->NewMagazineInventoryInstanceID = BestMagazineItem.UniqueInstanceID;

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
    ISuspenseCoreQuickSlotProvider* QuickSlotProvider = const_cast<USuspenseCoreReloadAbility*>(this)->GetQuickSlotProvider();
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
    // Visual/audio feedback here if needed
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
        BroadcastReloadCompleted();
        RELOAD_LOG(Log, TEXT("Reload completed successfully"));
        EndAbility(CachedSpecHandle, CachedActorInfo, CachedActivationInfo, true, false);
    }
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
    RELOAD_LOG(Warning, TEXT("  [GetMagazineProvider] Searching for ISuspenseCoreMagazineProvider..."));

    AActor* AvatarActor = GetAvatarActorFromActorInfo();
    RELOAD_LOG(Warning, TEXT("    AvatarActor: %s"), *GetNameSafe(AvatarActor));

    if (AvatarActor)
    {
        // Check if avatar implements interface
        if (AvatarActor->GetClass()->ImplementsInterface(USuspenseCoreMagazineProvider::StaticClass()))
        {
            RELOAD_LOG(Warning, TEXT("    ✓ Found on AvatarActor directly"));
            return Cast<ISuspenseCoreMagazineProvider>(AvatarActor);
        }

        // Check components
        TArray<UActorComponent*> Components;
        AvatarActor->GetComponents(Components);
        RELOAD_LOG(Warning, TEXT("    Searching %d components on Avatar..."), Components.Num());
        for (UActorComponent* Comp : Components)
        {
            if (Comp && Comp->GetClass()->ImplementsInterface(USuspenseCoreMagazineProvider::StaticClass()))
            {
                RELOAD_LOG(Warning, TEXT("    ✓ Found on Avatar component: %s"), *Comp->GetName());
                return Cast<ISuspenseCoreMagazineProvider>(Comp);
            }
        }

        // Check attached actors (weapons!)
        TArray<AActor*> AttachedActors;
        AvatarActor->GetAttachedActors(AttachedActors);
        RELOAD_LOG(Warning, TEXT("    Searching %d attached actors on Avatar..."), AttachedActors.Num());
        for (AActor* Attached : AttachedActors)
        {
            if (Attached)
            {
                RELOAD_LOG(Warning, TEXT("      Checking attached: %s"), *Attached->GetName());
                // Check actor itself
                if (Attached->GetClass()->ImplementsInterface(USuspenseCoreMagazineProvider::StaticClass()))
                {
                    RELOAD_LOG(Warning, TEXT("    ✓ Found on attached actor: %s"), *Attached->GetName());
                    return Cast<ISuspenseCoreMagazineProvider>(Attached);
                }
                // Check its components
                TArray<UActorComponent*> AttachedComps;
                Attached->GetComponents(AttachedComps);
                for (UActorComponent* Comp : AttachedComps)
                {
                    if (Comp && Comp->GetClass()->ImplementsInterface(USuspenseCoreMagazineProvider::StaticClass()))
                    {
                        RELOAD_LOG(Warning, TEXT("    ✓ Found on attached actor component: %s.%s"), *Attached->GetName(), *Comp->GetName());
                        return Cast<ISuspenseCoreMagazineProvider>(Comp);
                    }
                }
            }
        }
    }

    // Check owner actor
    AActor* OwnerActor = GetOwningActorFromActorInfo();
    RELOAD_LOG(Warning, TEXT("    OwnerActor: %s"), *GetNameSafe(OwnerActor));

    if (OwnerActor && OwnerActor != AvatarActor)
    {
        TArray<UActorComponent*> Components;
        OwnerActor->GetComponents(Components);
        RELOAD_LOG(Warning, TEXT("    Searching %d components on Owner..."), Components.Num());
        for (UActorComponent* Comp : Components)
        {
            if (Comp && Comp->GetClass()->ImplementsInterface(USuspenseCoreMagazineProvider::StaticClass()))
            {
                RELOAD_LOG(Warning, TEXT("    ✓ Found on Owner component: %s"), *Comp->GetName());
                return Cast<ISuspenseCoreMagazineProvider>(Comp);
            }
        }
    }

    RELOAD_LOG(Warning, TEXT("    ❌ MagazineProvider NOT FOUND anywhere!"));
    return nullptr;
}

ISuspenseCoreQuickSlotProvider* USuspenseCoreReloadAbility::GetQuickSlotProvider() const
{
    AActor* OwnerActor = GetOwningActorFromActorInfo();
    if (!OwnerActor)
    {
        return nullptr;
    }

    // Check if owner implements interface
    if (OwnerActor->GetClass()->ImplementsInterface(USuspenseCoreQuickSlotProvider::StaticClass()))
    {
        return Cast<ISuspenseCoreQuickSlotProvider>(OwnerActor);
    }

    // Check components
    TArray<UActorComponent*> Components;
    OwnerActor->GetComponents(Components);
    for (UActorComponent* Comp : Components)
    {
        if (Comp && Comp->GetClass()->ImplementsInterface(USuspenseCoreQuickSlotProvider::StaticClass()))
        {
            return Cast<ISuspenseCoreQuickSlotProvider>(Comp);
        }
    }

    return nullptr;
}

ISuspenseCoreInventory* USuspenseCoreReloadAbility::GetInventoryProvider() const
{
    AActor* OwnerActor = GetOwningActorFromActorInfo();
    if (!OwnerActor)
    {
        return nullptr;
    }

    // Check if owner implements interface
    if (OwnerActor->GetClass()->ImplementsInterface(USuspenseCoreInventory::StaticClass()))
    {
        return Cast<ISuspenseCoreInventory>(OwnerActor);
    }

    // Check components
    TArray<UActorComponent*> Components;
    OwnerActor->GetComponents(Components);
    for (UActorComponent* Comp : Components)
    {
        if (Comp && Comp->GetClass()->ImplementsInterface(USuspenseCoreInventory::StaticClass()))
        {
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
        RELOAD_LOG(Warning, TEXT("No montage for reload type %d"), static_cast<int32>(CurrentReloadType));
        return false;
    }

    ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!Character)
    {
        return false;
    }

    UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
    if (!AnimInstance)
    {
        return false;
    }

    // Calculate play rate to match desired duration
    float MontageLength = Montage->GetPlayLength();
    float PlayRate = (ReloadDuration > 0.0f) ? (MontageLength / ReloadDuration) : 1.0f;

    // Play montage
    float Duration = AnimInstance->Montage_Play(Montage, PlayRate);
    if (Duration <= 0.0f)
    {
        RELOAD_LOG(Warning, TEXT("Failed to play reload montage"));
        return false;
    }

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
    ISuspenseCoreMagazineProvider* MagProvider = GetMagazineProvider();
    if (MagProvider)
    {
        ISuspenseCoreMagazineProvider::Execute_NotifyReloadStateChanged(
            Cast<UObject>(MagProvider), false, CurrentReloadType, 0.0f);
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

#undef RELOAD_LOG
