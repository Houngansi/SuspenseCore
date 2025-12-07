// Copyright Suspense Team. All Rights Reserved.

#include "Components/SuspenseCoreEquipmentComponentBase.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameFramework/PlayerState.h"
#include "GameplayAbilitySpec.h"
#include "Delegates/SuspenseCoreEventManager.h"
#include "ItemSystem/SuspenseCoreItemManager.h"
#include "Interfaces/Equipment/ISuspenseCoreEquipment.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY(LogMedComEquipment);

USuspenseCoreEquipmentComponentBase::USuspenseCoreEquipmentComponentBase()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);

    bIsInitialized = false;
    ComponentVersion = 1; // Current component version
    EquippedItemInstance = FSuspenseCoreInventoryItemInstance();
    CachedASC = nullptr;
    EquipmentCycleCounter = 0;
    BroadcastEventCounter = 0;
    NextPredictionKey = 1;
    LastCacheValidationTime = 0.0f;
}

void USuspenseCoreEquipmentComponentBase::BeginPlay()
{
    Super::BeginPlay();

    // Initialize core references on begin play
    InitializeCoreReferences();

    // Start periodic prediction cleanup on clients
    if (!GetOwner()->HasAuthority() && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            *new FTimerHandle(),
            this,
            &USuspenseCoreEquipmentComponentBase::CleanupExpiredPredictions,
            1.0f,
            true
        );
    }
}

void USuspenseCoreEquipmentComponentBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clean up any active equipment
    if (bIsInitialized)
    {
        Cleanup();
    }

    // Clear cached references with thread safety
    {
        FScopeLock Lock(&CacheCriticalSection);
        CachedASC = nullptr;
        CachedItemManager.Reset();
        CachedDelegateManager.Reset();
    }

    Super::EndPlay(EndPlayReason);
}

void USuspenseCoreEquipmentComponentBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(USuspenseCoreEquipmentComponentBase, bIsInitialized);
    DOREPLIFETIME(USuspenseCoreEquipmentComponentBase, EquippedItemInstance);
    DOREPLIFETIME(USuspenseCoreEquipmentComponentBase, ComponentVersion);
    DOREPLIFETIME(USuspenseCoreEquipmentComponentBase, EquipmentCycleCounter);
}

void USuspenseCoreEquipmentComponentBase::Initialize(AActor* InOwner, UAbilitySystemComponent* InASC)
{
    if (!InOwner || !InASC)
    {
        EQUIPMENT_LOG(Error, TEXT("Initialize called with invalid parameters - Owner: %s, ASC: %s"),
            InOwner ? TEXT("Valid") : TEXT("NULL"),
            InASC ? TEXT("Valid") : TEXT("NULL"));
        return;
    }

    if (bIsInitialized)
    {
        EQUIPMENT_LOG(Warning, TEXT("Already initialized, cleaning up first"));
        Cleanup();
    }

    // Проверяем тип владельца для диагностики
    FString OwnerType = TEXT("Unknown");
    if (Cast<APlayerState>(InOwner))
    {
        OwnerType = TEXT("PlayerState");
    }
    else if (Cast<APawn>(InOwner))
    {
        OwnerType = TEXT("Pawn");
    }
    else if (Cast<AController>(InOwner))
    {
        OwnerType = TEXT("Controller");
    }

    EQUIPMENT_LOG(Log, TEXT("Initialize: Owner=%s (Type: %s), ASC=%s"),
        *InOwner->GetName(),
        *OwnerType,
        *InASC->GetName());

    EquipmentCycleCounter++;
    CachedASC = InASC;
    bIsInitialized = true;

    // Ensure core references are initialized
    InitializeCoreReferences();

    EQUIPMENT_LOG(Log, TEXT("Initialized (Cycle: %d, Version: %d)"), EquipmentCycleCounter, ComponentVersion);

    OnEquipmentInitialized();

    // Mark for replication
    if (InOwner->HasAuthority())
    {
        InOwner->ForceNetUpdate();
    }
}

void USuspenseCoreEquipmentComponentBase::InitializeWithItemInstance(AActor* InOwner, UAbilitySystemComponent* InASC, const FSuspenseCoreInventoryItemInstance& ItemInstance)
{
    // First do basic initialization
    Initialize(InOwner, InASC);

    if (!bIsInitialized)
    {
        EQUIPMENT_LOG(Error, TEXT("Failed to initialize base component"));
        return;
    }

    // Set the equipped item
    SetEquippedItemInstance(ItemInstance);
}

void USuspenseCoreEquipmentComponentBase::Cleanup()
{
    if (!bIsInitialized)
    {
        return;
    }

    EQUIPMENT_LOG(Log, TEXT("Cleaning up (Cycle: %d)"), EquipmentCycleCounter);

    // Store old item for event
    FSuspenseCoreInventoryItemInstance OldItem = EquippedItemInstance;

    // Clear equipped item
    EquippedItemInstance = FSuspenseCoreInventoryItemInstance();

    // Clear predictions
    ActivePredictions.Empty();

    // Notify about change
    if (OldItem.IsValid())
    {
        OnEquippedItemChanged(OldItem, EquippedItemInstance);
    }

    bIsInitialized = false;
    CachedASC = nullptr;

    // Mark for replication
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        // Force net update on the owner actor
        if (AActor* Owner = GetOwner())
        {
            Owner->ForceNetUpdate();
        }
    }
}

void USuspenseCoreEquipmentComponentBase::UpdateEquippedItem(const FSuspenseCoreInventoryItemInstance& NewItemInstance)
{
    if (!bIsInitialized)
    {
        EQUIPMENT_LOG(Warning, TEXT("Cannot update equipped item - not initialized"));
        return;
    }

    if (!NewItemInstance.IsValid())
    {
        EQUIPMENT_LOG(Warning, TEXT("Cannot update with invalid item instance"));
        return;
    }

    FSuspenseCoreInventoryItemInstance OldItem = EquippedItemInstance;
    EquippedItemInstance = NewItemInstance;

    OnEquippedItemChanged(OldItem, NewItemInstance);

    BroadcastEquipmentUpdated();

    // Mark for replication
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        // Force net update on the owner actor
        if (AActor* Owner = GetOwner())
        {
            Owner->ForceNetUpdate();
        }
    }
}

void USuspenseCoreEquipmentComponentBase::SetEquippedItemInstance(const FSuspenseCoreInventoryItemInstance& ItemInstance)
{
    FSuspenseCoreInventoryItemInstance OldItem = EquippedItemInstance;
    EquippedItemInstance = ItemInstance;

    // Notify about change
    OnEquippedItemChanged(OldItem, ItemInstance);

    // Broadcast appropriate event
    if (ItemInstance.IsValid())
    {
        // Get slot type from item data
        FSuspenseCoreUnifiedItemData ItemData;
        if (GetEquippedItemData(ItemData))
        {
            BroadcastItemEquipped(ItemInstance, ItemData.EquipmentSlot);
        }
    }
    else if (OldItem.IsValid())
    {
        // Get slot type from old item data
        FSuspenseCoreUnifiedItemData OldItemData;
        if (GetItemManager() && GetItemManager()->GetUnifiedItemData(OldItem.ItemID, OldItemData))
        {
            BroadcastItemUnequipped(OldItem, OldItemData.EquipmentSlot);
        }
    }

    // Mark for replication
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        // Force net update on the owner actor
        if (AActor* Owner = GetOwner())
        {
            Owner->ForceNetUpdate();
        }
    }
}

//================================================
// Client Prediction Implementation
//================================================

int32 USuspenseCoreEquipmentComponentBase::StartClientPrediction(const FSuspenseCoreInventoryItemInstance& PredictedInstance)
{
    // Only allow predictions on clients
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        return 0;
    }

    // Limit concurrent predictions
    if (ActivePredictions.Num() >= MaxConcurrentPredictions)
    {
        EQUIPMENT_LOG(Warning, TEXT("Too many concurrent predictions (%d)"), ActivePredictions.Num());
        CleanupExpiredPredictions();

        if (ActivePredictions.Num() >= MaxConcurrentPredictions)
        {
            return 0;
        }
    }

    // Create new prediction
    FEquipmentComponentPredictionData NewPrediction;
    NewPrediction.PredictionKey = NextPredictionKey++;
    NewPrediction.PredictedItem = PredictedInstance;
    NewPrediction.PredictionTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    NewPrediction.bConfirmed = false;

    ActivePredictions.Add(NewPrediction);

    EQUIPMENT_LOG(Verbose, TEXT("Started client prediction %d for item %s"),
        NewPrediction.PredictionKey, *PredictedInstance.ItemID.ToString());

    return NewPrediction.PredictionKey;
}

void USuspenseCoreEquipmentComponentBase::ConfirmClientPrediction(int32 PredictionKey, bool bSuccess, const FSuspenseCoreInventoryItemInstance& ActualInstance)
{
    // Find prediction
    int32 PredictionIndex = ActivePredictions.IndexOfByPredicate(
        [PredictionKey](const FEquipmentComponentPredictionData& Data) { return Data.PredictionKey == PredictionKey; }
    );

    if (PredictionIndex == INDEX_NONE)
    {
        EQUIPMENT_LOG(VeryVerbose, TEXT("Prediction %d not found (may have expired)"), PredictionKey);
        return;
    }

    FEquipmentComponentPredictionData& Prediction = ActivePredictions[PredictionIndex];

    if (bSuccess)
    {
        Prediction.bConfirmed = true;
        EQUIPMENT_LOG(Verbose, TEXT("Prediction %d confirmed"), PredictionKey);
    }
    else
    {
        // Prediction failed - revert to actual state
        EQUIPMENT_LOG(Warning, TEXT("Prediction %d failed - reverting to server state"), PredictionKey);

        // Update to actual item
        if (ActualInstance.IsValid() || EquippedItemInstance.IsValid())
        {
            UpdateEquippedItem(ActualInstance);
        }
    }

    // Remove confirmed/failed prediction
    ActivePredictions.RemoveAt(PredictionIndex);
}

void USuspenseCoreEquipmentComponentBase::CleanupExpiredPredictions()
{
    if (!GetWorld())
    {
        return;
    }

    const float CurrentTime = GetWorld()->GetTimeSeconds();

    // Remove expired predictions
    ActivePredictions.RemoveAll([CurrentTime](const FEquipmentComponentPredictionData& Data)
    {
        return Data.IsExpired(CurrentTime, PredictionTimeoutSeconds);
    });
}

//================================================
// Thread-Safe Cache Access
//================================================

USuspenseCoreItemManager* USuspenseCoreEquipmentComponentBase::GetItemManager() const
{
    FScopeLock Lock(&CacheCriticalSection);

    // Validate cache periodically
    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    const bool bShouldValidate = (CurrentTime - LastCacheValidationTime) > 1.0f;

    if (!CachedItemManager.IsValid() || bShouldValidate)
    {
        LastCacheValidationTime = CurrentTime;

        if (UWorld* World = GetWorld())
        {
            if (UGameInstance* GameInstance = World->GetGameInstance())
            {
                CachedItemManager = GameInstance->GetSubsystem<USuspenseCoreItemManager>();
            }
        }
    }

    return CachedItemManager.Get();
}

USuspenseCoreEventManager* USuspenseCoreEquipmentComponentBase::GetDelegateManager() const
{
    FScopeLock Lock(&CacheCriticalSection);

    // Validate cache periodically
    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    const bool bShouldValidate = (CurrentTime - LastCacheValidationTime) > 1.0f;

    if (!CachedDelegateManager.IsValid() || bShouldValidate)
    {
        LastCacheValidationTime = CurrentTime;

        if (UWorld* World = GetWorld())
        {
            if (UGameInstance* GameInstance = World->GetGameInstance())
            {
                CachedDelegateManager = GameInstance->GetSubsystem<USuspenseCoreEventManager>();
            }
        }
    }

    return CachedDelegateManager.Get();
}

bool USuspenseCoreEquipmentComponentBase::GetEquippedItemData(FSuspenseCoreUnifiedItemData& OutItemData) const
{
    if (!EquippedItemInstance.IsValid())
    {
        return false;
    }

    USuspenseCoreItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        EQUIPMENT_LOG(Warning, TEXT("ItemManager not available"));
        return false;
    }

    return ItemManager->GetUnifiedItemData(EquippedItemInstance.ItemID, OutItemData);
}

float USuspenseCoreEquipmentComponentBase::GetEquippedItemProperty(const FName& PropertyName, float DefaultValue) const
{
    if (!EquippedItemInstance.IsValid())
    {
        return DefaultValue;
    }

    return EquippedItemInstance.GetRuntimeProperty(PropertyName, DefaultValue);
}

void USuspenseCoreEquipmentComponentBase::SetEquippedItemProperty(const FName& PropertyName, float Value)
{
    if (!EquippedItemInstance.IsValid())
    {
        EQUIPMENT_LOG(Warning, TEXT("Cannot set property - no item equipped"));
        return;
    }

    float OldValue = EquippedItemInstance.GetRuntimeProperty(PropertyName, 0.0f);
    EquippedItemInstance.SetRuntimeProperty(PropertyName, Value);

    // Broadcast property change
    BroadcastEquipmentPropertyChanged(PropertyName, OldValue, Value);

    // Mark for replication
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        // Force net update on the owner actor
        if (AActor* Owner = GetOwner())
        {
            Owner->ForceNetUpdate();
        }
    }
}

void USuspenseCoreEquipmentComponentBase::InitializeCoreReferences()
{
    // Pre-cache commonly used subsystems
    GetItemManager();
    GetDelegateManager();
}

bool USuspenseCoreEquipmentComponentBase::ValidateSystemReferences() const
{
    bool bValid = true;

    if (!GetItemManager())
    {
        EQUIPMENT_LOG(Error, TEXT("ItemManager subsystem not available"));
        bValid = false;
    }

    if (!GetDelegateManager())
    {
        EQUIPMENT_LOG(Error, TEXT("EventDelegateManager subsystem not available"));
        bValid = false;
    }

    return bValid;
}

bool USuspenseCoreEquipmentComponentBase::ValidateDelegateManager() const
{
    USuspenseCoreEventManager* Manager = GetDelegateManager();
    return IsValid(Manager);
}

void USuspenseCoreEquipmentComponentBase::OnEquipmentInitialized()
{
    // Base implementation - override in derived classes
}

void USuspenseCoreEquipmentComponentBase::OnEquippedItemChanged(const FSuspenseCoreInventoryItemInstance& OldItem, const FSuspenseCoreInventoryItemInstance& NewItem)
{
    // Base implementation - override in derived classes
    EQUIPMENT_LOG(Verbose, TEXT("Equipped item changed from %s to %s"),
        OldItem.IsValid() ? *OldItem.ItemID.ToString() : TEXT("None"),
        NewItem.IsValid() ? *NewItem.ItemID.ToString() : TEXT("None"));
}

bool USuspenseCoreEquipmentComponentBase::ExecuteOnServer(const FString& FuncName, TFunction<void()> ServerCode)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        EQUIPMENT_LOG(Warning, TEXT("%s: Must be called on server"), *FuncName);
        return false;
    }

    ServerCode();
    return true;
}

void USuspenseCoreEquipmentComponentBase::LogEventBroadcast(const FString& EventName, bool bSuccess) const
{
    BroadcastEventCounter++;

    if (bSuccess)
    {
        EQUIPMENT_LOG(VeryVerbose, TEXT("Event broadcast: %s (Total: %d)"),
            *EventName, BroadcastEventCounter);
    }
    else
    {
        EQUIPMENT_LOG(Warning, TEXT("Failed to broadcast event: %s"), *EventName);
    }
}

//================================================
// Replication Callbacks
//================================================

void USuspenseCoreEquipmentComponentBase::OnRep_EquippedItemInstance(const FSuspenseCoreInventoryItemInstance& OldInstance)
{
    // Handle item change on clients
    OnEquippedItemChanged(OldInstance, EquippedItemInstance);

    // Broadcast update
    BroadcastEquipmentUpdated();

    EQUIPMENT_LOG(Verbose, TEXT("OnRep_EquippedItemInstance: %s -> %s"),
        OldInstance.IsValid() ? *OldInstance.ItemID.ToString() : TEXT("None"),
        EquippedItemInstance.IsValid() ? *EquippedItemInstance.ItemID.ToString() : TEXT("None"));
}

void USuspenseCoreEquipmentComponentBase::OnRep_ComponentState()
{
    EQUIPMENT_LOG(Verbose, TEXT("OnRep_ComponentState: Initialized=%s, Cycle=%d"),
        bIsInitialized ? TEXT("true") : TEXT("false"), EquipmentCycleCounter);
}

//================================================
// Enhanced Broadcast Methods Implementation
//================================================

void USuspenseCoreEquipmentComponentBase::BroadcastItemEquipped(const FSuspenseCoreInventoryItemInstance& ItemInstance, const FGameplayTag& SlotType)
{
    if (!ValidateDelegateManager())
    {
        LogEventBroadcast(TEXT("ItemEquipped"), false);
        return;
    }

    USuspenseCoreEventManager* Manager = GetDelegateManager();

    // Create comprehensive event data
    FString EventData = FString::Printf(
        TEXT("ItemID:%s,Quantity:%d,Slot:%s,InstanceID:%s"),
        *ItemInstance.ItemID.ToString(),
        ItemInstance.Quantity,
        *SlotType.ToString(),
        *ItemInstance.InstanceID.ToString()
    );

    Manager->NotifyEquipmentEvent(
        GetOwner(),
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.ItemEquipped")),
        EventData
    );

    LogEventBroadcast(TEXT("ItemEquipped"), true);
}

void USuspenseCoreEquipmentComponentBase::BroadcastItemUnequipped(const FSuspenseCoreInventoryItemInstance& ItemInstance, const FGameplayTag& SlotType)
{
    if (!ValidateDelegateManager())
    {
        LogEventBroadcast(TEXT("ItemUnequipped"), false);
        return;
    }

    USuspenseCoreEventManager* Manager = GetDelegateManager();

    FString EventData = FString::Printf(
        TEXT("ItemID:%s,Quantity:%d,Slot:%s,InstanceID:%s"),
        *ItemInstance.ItemID.ToString(),
        ItemInstance.Quantity,
        *SlotType.ToString(),
        *ItemInstance.InstanceID.ToString()
    );

    Manager->NotifyEquipmentEvent(
        GetOwner(),
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.ItemUnequipped")),
        EventData
    );

    LogEventBroadcast(TEXT("ItemUnequipped"), true);
}

void USuspenseCoreEquipmentComponentBase::BroadcastEquipmentPropertyChanged(const FName& PropertyName, float OldValue, float NewValue)
{
    if (!ValidateDelegateManager())
    {
        LogEventBroadcast(FString::Printf(TEXT("PropertyChanged:%s"), *PropertyName.ToString()), false);
        return;
    }

    ISuspenseCoreEquipment::BroadcastEquipmentPropertyChanged(
        this, PropertyName, OldValue, NewValue
    );

    LogEventBroadcast(FString::Printf(TEXT("PropertyChanged:%s"), *PropertyName.ToString()), true);
}

void USuspenseCoreEquipmentComponentBase::BroadcastEquipmentStateChanged(const FGameplayTag& OldState, const FGameplayTag& NewState, bool bInterrupted)
{
    if (!ValidateDelegateManager())
    {
        LogEventBroadcast(TEXT("StateChanged"), false);
        return;
    }

    USuspenseCoreEventManager* Manager = GetDelegateManager();
    Manager->NotifyEquipmentStateChanged(OldState, NewState, bInterrupted);
    LogEventBroadcast(TEXT("StateChanged"), true);
}

void USuspenseCoreEquipmentComponentBase::BroadcastEquipmentEvent(const FGameplayTag& EventTag, const FString& EventData)
{
    if (!ValidateDelegateManager())
    {
        LogEventBroadcast(EventTag.ToString(), false);
        return;
    }

    USuspenseCoreEventManager* Manager = GetDelegateManager();
    Manager->NotifyEquipmentEvent(GetOwner(), EventTag, EventData);
    LogEventBroadcast(EventTag.ToString(), true);
}

void USuspenseCoreEquipmentComponentBase::BroadcastEquipmentUpdated()
{
    if (!ValidateDelegateManager())
    {
        LogEventBroadcast(TEXT("EquipmentUpdated"), false);
        return;
    }

    USuspenseCoreEventManager* Manager = GetDelegateManager();
    Manager->NotifyEquipmentUpdated();
    LogEventBroadcast(TEXT("EquipmentUpdated"), true);
}

void USuspenseCoreEquipmentComponentBase::BroadcastAmmoChanged(float CurrentAmmo, float RemainingAmmo, float MagazineSize)
{
    if (!ValidateDelegateManager())
    {
        LogEventBroadcast(TEXT("AmmoChanged"), false);
        return;
    }

    USuspenseCoreEventManager* Manager = GetDelegateManager();
    Manager->NotifyAmmoChanged(CurrentAmmo, RemainingAmmo, MagazineSize);
    LogEventBroadcast(TEXT("AmmoChanged"), true);
}

void USuspenseCoreEquipmentComponentBase::BroadcastWeaponFired(const FVector& Origin, const FVector& Impact, bool bSuccess, const FGameplayTag& FireMode)
{
    if (!ValidateDelegateManager())
    {
        LogEventBroadcast(TEXT("WeaponFired"), false);
        return;
    }

    USuspenseCoreEventManager* Manager = GetDelegateManager();

    // Enhanced with fire mode information
    FString EventData = FString::Printf(
        TEXT("Origin:%s,Impact:%s,Success:%s,FireMode:%s"),
        *Origin.ToString(),
        *Impact.ToString(),
        bSuccess ? TEXT("true") : TEXT("false"),
        *FireMode.ToString()
    );

    Manager->NotifyEquipmentEvent(
        GetOwner(),
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.WeaponFired")),
        EventData
    );

    Manager->NotifyWeaponFired(Origin, Impact, bSuccess, FireMode.GetTagName());
    LogEventBroadcast(TEXT("WeaponFired"), true);
}

void USuspenseCoreEquipmentComponentBase::BroadcastFireModeChanged(const FGameplayTag& NewFireMode, const FText& FireModeDisplayName)
{
    if (!ValidateDelegateManager())
    {
        LogEventBroadcast(TEXT("FireModeChanged"), false);
        return;
    }

    USuspenseCoreEventManager* Manager = GetDelegateManager();

    // Get current spread from weapon data if available
    float CurrentSpread = 0.0f;
    if (EquippedItemInstance.IsValid())
    {
        CurrentSpread = GetEquippedItemProperty(TEXT("CurrentSpread"), 0.0f);
    }

    Manager->NotifyFireModeChanged(NewFireMode, CurrentSpread);

    // Also send detailed event
    FString EventData = FString::Printf(
        TEXT("FireMode:%s,DisplayName:%s,Spread:%.3f"),
        *NewFireMode.ToString(),
        *FireModeDisplayName.ToString(),
        CurrentSpread
    );

    Manager->NotifyEquipmentEvent(
        GetOwner(),
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.FireModeChanged")),
        EventData
    );

    LogEventBroadcast(TEXT("FireModeChanged"), true);
}

void USuspenseCoreEquipmentComponentBase::BroadcastWeaponReload(bool bStarted, float ReloadDuration)
{
    if (!ValidateDelegateManager())
    {
        LogEventBroadcast(TEXT("WeaponReload"), false);
        return;
    }

    USuspenseCoreEventManager* Manager = GetDelegateManager();

    if (bStarted)
    {
        Manager->NotifyWeaponReloadStart();

        // Send detailed event with duration
        FString EventData = FString::Printf(TEXT("Duration:%.2f"), ReloadDuration);
        Manager->NotifyEquipmentEvent(
            GetOwner(),
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.ReloadStart")),
            EventData
        );
    }
    else
    {
        Manager->NotifyWeaponReloadEnd();
        Manager->NotifyEquipmentEvent(
            GetOwner(),
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.ReloadEnd")),
            TEXT("")
        );
    }

    LogEventBroadcast(TEXT("WeaponReload"), true);
}

void USuspenseCoreEquipmentComponentBase::BroadcastWeaponSpreadUpdated(float NewSpread, float MaxSpread)
{
    if (!ValidateDelegateManager())
    {
        LogEventBroadcast(TEXT("SpreadUpdated"), false);
        return;
    }

    USuspenseCoreEventManager* Manager = GetDelegateManager();

    Manager->NotifyWeaponSpreadUpdated(NewSpread);

    // Send detailed event
    FString EventData = FString::Printf(
        TEXT("CurrentSpread:%.3f,MaxSpread:%.3f,Percentage:%.1f"),
        NewSpread,
        MaxSpread,
        MaxSpread > 0.0f ? (NewSpread / MaxSpread * 100.0f) : 0.0f
    );

    Manager->NotifyEquipmentEvent(
        GetOwner(),
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.SpreadUpdated")),
        EventData
    );

    LogEventBroadcast(TEXT("SpreadUpdated"), true);
}

//================================================
// ISuspenseCoreAbilityProvider Implementation
//================================================

void USuspenseCoreEquipmentComponentBase::InitializeAbilityProvider_Implementation(UAbilitySystemComponent* InASC)
{
    CachedASC = InASC;
    EQUIPMENT_LOG(Log, TEXT("Ability provider initialized"));
}

FGameplayAbilitySpecHandle USuspenseCoreEquipmentComponentBase::GrantAbility_Implementation(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level, int32 InputID)
{
    if (!CachedASC || !AbilityClass)
    {
        EQUIPMENT_LOG(Warning, TEXT("Cannot grant ability - ASC or ability class invalid"));
        return FGameplayAbilitySpecHandle();
    }

    FGameplayAbilitySpec AbilitySpec(AbilityClass, Level, InputID, this);
    FGameplayAbilitySpecHandle Handle = CachedASC->GiveAbility(AbilitySpec);

    EQUIPMENT_LOG(Log, TEXT("Granted ability: %s (Level: %d, InputID: %d)"),
        *GetNameSafe(AbilityClass), Level, InputID);

    return Handle;
}

void USuspenseCoreEquipmentComponentBase::RemoveAbility_Implementation(FGameplayAbilitySpecHandle AbilityHandle)
{
    if (!CachedASC || !AbilityHandle.IsValid())
    {
        return;
    }

    CachedASC->ClearAbility(AbilityHandle);
    EQUIPMENT_LOG(Log, TEXT("Removed ability handle"));
}

FActiveGameplayEffectHandle USuspenseCoreEquipmentComponentBase::ApplyEffectToSelf_Implementation(TSubclassOf<UGameplayEffect> EffectClass, float Level)
{
    if (!CachedASC || !EffectClass)
    {
        EQUIPMENT_LOG(Warning, TEXT("Cannot apply effect - ASC or effect class invalid"));
        return FActiveGameplayEffectHandle();
    }

    FGameplayEffectContextHandle Context = CachedASC->MakeEffectContext();
    Context.AddSourceObject(this);

    FGameplayEffectSpecHandle Spec = CachedASC->MakeOutgoingSpec(EffectClass, Level, Context);
    if (Spec.IsValid())
    {
        FActiveGameplayEffectHandle Handle = CachedASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
        EQUIPMENT_LOG(Log, TEXT("Applied effect: %s (Level: %.1f)"),
            *GetNameSafe(EffectClass), Level);
        return Handle;
    }

    return FActiveGameplayEffectHandle();
}

void USuspenseCoreEquipmentComponentBase::RemoveEffect_Implementation(FActiveGameplayEffectHandle EffectHandle)
{
    if (!CachedASC || !EffectHandle.IsValid())
    {
        return;
    }

    CachedASC->RemoveActiveGameplayEffect(EffectHandle);
    EQUIPMENT_LOG(Log, TEXT("Removed effect handle"));
}
