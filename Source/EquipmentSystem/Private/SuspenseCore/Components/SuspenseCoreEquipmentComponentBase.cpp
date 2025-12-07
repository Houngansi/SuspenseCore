// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreEquipmentComponentBase.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameFramework/PlayerState.h"
#include "GameplayAbilitySpec.h"
#include "Delegates/SuspenseEventManager.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "Interfaces/Equipment/ISuspenseEquipment.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY(LogSuspenseCoreEquipment);

USuspenseCoreEquipmentComponentBase::USuspenseCoreEquipmentComponentBase()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);

    bIsInitialized = false;
    ComponentVersion = 1;
    EquippedItemInstance = FSuspenseInventoryItemInstance();
    CachedASC = nullptr;
    EquipmentCycleCounter = 0;
    BroadcastEventCounter = 0;
    NextPredictionKey = 1;
    LastCacheValidationTime = 0.0f;
}

void USuspenseCoreEquipmentComponentBase::BeginPlay()
{
    Super::BeginPlay();
    InitializeCoreReferences();

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
    if (bIsInitialized) { Cleanup(); }

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
        SUSPENSECORE_LOG(Error, TEXT("Initialize called with invalid parameters"));
        return;
    }

    if (bIsInitialized) { Cleanup(); }

    EquipmentCycleCounter++;
    CachedASC = InASC;
    bIsInitialized = true;

    InitializeCoreReferences();
    OnEquipmentInitialized();

    if (InOwner->HasAuthority()) { InOwner->ForceNetUpdate(); }
}

void USuspenseCoreEquipmentComponentBase::InitializeWithItemInstance(AActor* InOwner, UAbilitySystemComponent* InASC, const FSuspenseInventoryItemInstance& ItemInstance)
{
    Initialize(InOwner, InASC);
    if (bIsInitialized) { SetEquippedItemInstance(ItemInstance); }
}

void USuspenseCoreEquipmentComponentBase::Cleanup()
{
    if (!bIsInitialized) { return; }

    FSuspenseInventoryItemInstance OldItem = EquippedItemInstance;
    EquippedItemInstance = FSuspenseInventoryItemInstance();
    ActivePredictions.Empty();

    if (OldItem.IsValid()) { OnEquippedItemChanged(OldItem, EquippedItemInstance); }

    bIsInitialized = false;
    CachedASC = nullptr;

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        if (AActor* Owner = GetOwner()) { Owner->ForceNetUpdate(); }
    }
}

void USuspenseCoreEquipmentComponentBase::UpdateEquippedItem(const FSuspenseInventoryItemInstance& NewItemInstance)
{
    if (!bIsInitialized || !NewItemInstance.IsValid()) { return; }

    FSuspenseInventoryItemInstance OldItem = EquippedItemInstance;
    EquippedItemInstance = NewItemInstance;
    OnEquippedItemChanged(OldItem, NewItemInstance);
    BroadcastEquipmentUpdated();

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        if (AActor* Owner = GetOwner()) { Owner->ForceNetUpdate(); }
    }
}

void USuspenseCoreEquipmentComponentBase::SetEquippedItemInstance(const FSuspenseInventoryItemInstance& ItemInstance)
{
    FSuspenseInventoryItemInstance OldItem = EquippedItemInstance;
    EquippedItemInstance = ItemInstance;
    OnEquippedItemChanged(OldItem, ItemInstance);

    if (ItemInstance.IsValid())
    {
        FSuspenseUnifiedItemData ItemData;
        if (GetEquippedItemData(ItemData)) { BroadcastItemEquipped(ItemInstance, ItemData.EquipmentSlot); }
    }
    else if (OldItem.IsValid())
    {
        FSuspenseUnifiedItemData OldItemData;
        if (GetItemManager() && GetItemManager()->GetUnifiedItemData(OldItem.ItemID, OldItemData))
        {
            BroadcastItemUnequipped(OldItem, OldItemData.EquipmentSlot);
        }
    }

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        if (AActor* Owner = GetOwner()) { Owner->ForceNetUpdate(); }
    }
}

int32 USuspenseCoreEquipmentComponentBase::StartClientPrediction(const FSuspenseInventoryItemInstance& PredictedInstance)
{
    if (GetOwner() && GetOwner()->HasAuthority()) { return 0; }
    if (ActivePredictions.Num() >= MaxConcurrentPredictions) { CleanupExpiredPredictions(); }
    if (ActivePredictions.Num() >= MaxConcurrentPredictions) { return 0; }

    FSuspenseCorePredictionData NewPrediction;
    NewPrediction.PredictionKey = NextPredictionKey++;
    NewPrediction.PredictedItem = PredictedInstance;
    NewPrediction.PredictionTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    ActivePredictions.Add(NewPrediction);
    return NewPrediction.PredictionKey;
}

void USuspenseCoreEquipmentComponentBase::ConfirmClientPrediction(int32 PredictionKey, bool bSuccess, const FSuspenseInventoryItemInstance& ActualInstance)
{
    int32 Index = ActivePredictions.IndexOfByPredicate([PredictionKey](const FSuspenseCorePredictionData& D) { return D.PredictionKey == PredictionKey; });
    if (Index == INDEX_NONE) { return; }

    if (!bSuccess && (ActualInstance.IsValid() || EquippedItemInstance.IsValid()))
    {
        UpdateEquippedItem(ActualInstance);
    }
    ActivePredictions.RemoveAt(Index);
}

void USuspenseCoreEquipmentComponentBase::CleanupExpiredPredictions()
{
    if (!GetWorld()) { return; }
    const float CurrentTime = GetWorld()->GetTimeSeconds();
    ActivePredictions.RemoveAll([CurrentTime](const FSuspenseCorePredictionData& D) { return D.IsExpired(CurrentTime, PredictionTimeoutSeconds); });
}

USuspenseItemManager* USuspenseCoreEquipmentComponentBase::GetItemManager() const
{
    FScopeLock Lock(&CacheCriticalSection);
    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    if (!CachedItemManager.IsValid() || (CurrentTime - LastCacheValidationTime) > 1.0f)
    {
        LastCacheValidationTime = CurrentTime;
        if (UWorld* World = GetWorld())
        {
            if (UGameInstance* GI = World->GetGameInstance())
            {
                CachedItemManager = GI->GetSubsystem<USuspenseItemManager>();
            }
        }
    }
    return CachedItemManager.Get();
}

USuspenseEventManager* USuspenseCoreEquipmentComponentBase::GetDelegateManager() const
{
    FScopeLock Lock(&CacheCriticalSection);
    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    if (!CachedDelegateManager.IsValid() || (CurrentTime - LastCacheValidationTime) > 1.0f)
    {
        LastCacheValidationTime = CurrentTime;
        if (UWorld* World = GetWorld())
        {
            if (UGameInstance* GI = World->GetGameInstance())
            {
                CachedDelegateManager = GI->GetSubsystem<USuspenseEventManager>();
            }
        }
    }
    return CachedDelegateManager.Get();
}

bool USuspenseCoreEquipmentComponentBase::GetEquippedItemData(FSuspenseUnifiedItemData& OutItemData) const
{
    if (!EquippedItemInstance.IsValid()) { return false; }
    USuspenseItemManager* IM = GetItemManager();
    return IM ? IM->GetUnifiedItemData(EquippedItemInstance.ItemID, OutItemData) : false;
}

float USuspenseCoreEquipmentComponentBase::GetEquippedItemProperty(const FName& PropertyName, float DefaultValue) const
{
    return EquippedItemInstance.IsValid() ? EquippedItemInstance.GetRuntimeProperty(PropertyName, DefaultValue) : DefaultValue;
}

void USuspenseCoreEquipmentComponentBase::SetEquippedItemProperty(const FName& PropertyName, float Value)
{
    if (!EquippedItemInstance.IsValid()) { return; }
    float OldValue = EquippedItemInstance.GetRuntimeProperty(PropertyName, 0.0f);
    EquippedItemInstance.SetRuntimeProperty(PropertyName, Value);
    BroadcastEquipmentPropertyChanged(PropertyName, OldValue, Value);
    if (GetOwner() && GetOwner()->HasAuthority()) { if (AActor* Owner = GetOwner()) { Owner->ForceNetUpdate(); } }
}

void USuspenseCoreEquipmentComponentBase::InitializeCoreReferences() { GetItemManager(); GetDelegateManager(); }
bool USuspenseCoreEquipmentComponentBase::ValidateSystemReferences() const { return GetItemManager() && GetDelegateManager(); }
bool USuspenseCoreEquipmentComponentBase::ValidateDelegateManager() const { return IsValid(GetDelegateManager()); }
void USuspenseCoreEquipmentComponentBase::OnEquipmentInitialized() {}
void USuspenseCoreEquipmentComponentBase::OnEquippedItemChanged(const FSuspenseInventoryItemInstance& OldItem, const FSuspenseInventoryItemInstance& NewItem) {}

bool USuspenseCoreEquipmentComponentBase::ExecuteOnServer(const FString& FuncName, TFunction<void()> ServerCode)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) { return false; }
    ServerCode();
    return true;
}

void USuspenseCoreEquipmentComponentBase::LogEventBroadcast(const FString& EventName, bool bSuccess) const { BroadcastEventCounter++; }

void USuspenseCoreEquipmentComponentBase::OnRep_EquippedItemInstance(const FSuspenseInventoryItemInstance& OldInstance)
{
    OnEquippedItemChanged(OldInstance, EquippedItemInstance);
    BroadcastEquipmentUpdated();
}

void USuspenseCoreEquipmentComponentBase::OnRep_ComponentState() {}

// EventBus Broadcasts
void USuspenseCoreEquipmentComponentBase::BroadcastItemEquipped(const FSuspenseInventoryItemInstance& ItemInstance, const FGameplayTag& SlotType)
{
    if (!ValidateDelegateManager()) { return; }
    USuspenseEventManager* M = GetDelegateManager();
    FString EventData = FString::Printf(TEXT("ItemID:%s,Slot:%s"), *ItemInstance.ItemID.ToString(), *SlotType.ToString());
    M->NotifyEquipmentEvent(GetOwner(), FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.ItemEquipped")), EventData);
}

void USuspenseCoreEquipmentComponentBase::BroadcastItemUnequipped(const FSuspenseInventoryItemInstance& ItemInstance, const FGameplayTag& SlotType)
{
    if (!ValidateDelegateManager()) { return; }
    USuspenseEventManager* M = GetDelegateManager();
    FString EventData = FString::Printf(TEXT("ItemID:%s,Slot:%s"), *ItemInstance.ItemID.ToString(), *SlotType.ToString());
    M->NotifyEquipmentEvent(GetOwner(), FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.ItemUnequipped")), EventData);
}

void USuspenseCoreEquipmentComponentBase::BroadcastEquipmentPropertyChanged(const FName& PropertyName, float OldValue, float NewValue)
{
    if (!ValidateDelegateManager()) { return; }
    ISuspenseEquipment::BroadcastEquipmentPropertyChanged(this, PropertyName, OldValue, NewValue);
}

void USuspenseCoreEquipmentComponentBase::BroadcastEquipmentStateChanged(const FGameplayTag& OldState, const FGameplayTag& NewState, bool bInterrupted)
{
    if (!ValidateDelegateManager()) { return; }
    GetDelegateManager()->NotifyEquipmentStateChanged(OldState, NewState, bInterrupted);
}

void USuspenseCoreEquipmentComponentBase::BroadcastEquipmentEvent(const FGameplayTag& EventTag, const FString& EventData)
{
    if (!ValidateDelegateManager()) { return; }
    GetDelegateManager()->NotifyEquipmentEvent(GetOwner(), EventTag, EventData);
}

void USuspenseCoreEquipmentComponentBase::BroadcastEquipmentUpdated()
{
    if (!ValidateDelegateManager()) { return; }
    GetDelegateManager()->NotifyEquipmentUpdated();
}

void USuspenseCoreEquipmentComponentBase::BroadcastAmmoChanged(float CurrentAmmo, float RemainingAmmo, float MagazineSize)
{
    if (!ValidateDelegateManager()) { return; }
    GetDelegateManager()->NotifyAmmoChanged(CurrentAmmo, RemainingAmmo, MagazineSize);
}

void USuspenseCoreEquipmentComponentBase::BroadcastWeaponFired(const FVector& Origin, const FVector& Impact, bool bSuccess, const FGameplayTag& FireMode)
{
    if (!ValidateDelegateManager()) { return; }
    GetDelegateManager()->NotifyWeaponFired(Origin, Impact, bSuccess, FireMode.GetTagName());
}

void USuspenseCoreEquipmentComponentBase::BroadcastFireModeChanged(const FGameplayTag& NewFireMode, const FText& FireModeDisplayName)
{
    if (!ValidateDelegateManager()) { return; }
    GetDelegateManager()->NotifyFireModeChanged(NewFireMode, 0.0f);
}

void USuspenseCoreEquipmentComponentBase::BroadcastWeaponReload(bool bStarted, float ReloadDuration)
{
    if (!ValidateDelegateManager()) { return; }
    if (bStarted) { GetDelegateManager()->NotifyWeaponReloadStart(); }
    else { GetDelegateManager()->NotifyWeaponReloadEnd(); }
}

void USuspenseCoreEquipmentComponentBase::BroadcastWeaponSpreadUpdated(float NewSpread, float MaxSpread)
{
    if (!ValidateDelegateManager()) { return; }
    GetDelegateManager()->NotifyWeaponSpreadUpdated(NewSpread);
}

// ISuspenseAbilityProvider
void USuspenseCoreEquipmentComponentBase::InitializeAbilityProvider_Implementation(UAbilitySystemComponent* InASC) { CachedASC = InASC; }

FGameplayAbilitySpecHandle USuspenseCoreEquipmentComponentBase::GrantAbility_Implementation(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level, int32 InputID)
{
    if (!CachedASC || !AbilityClass) { return FGameplayAbilitySpecHandle(); }
    FGameplayAbilitySpec Spec(AbilityClass, Level, InputID, this);
    return CachedASC->GiveAbility(Spec);
}

void USuspenseCoreEquipmentComponentBase::RemoveAbility_Implementation(FGameplayAbilitySpecHandle AbilityHandle)
{
    if (CachedASC && AbilityHandle.IsValid()) { CachedASC->ClearAbility(AbilityHandle); }
}

FActiveGameplayEffectHandle USuspenseCoreEquipmentComponentBase::ApplyEffectToSelf_Implementation(TSubclassOf<UGameplayEffect> EffectClass, float Level)
{
    if (!CachedASC || !EffectClass) { return FActiveGameplayEffectHandle(); }
    FGameplayEffectContextHandle Ctx = CachedASC->MakeEffectContext();
    Ctx.AddSourceObject(this);
    FGameplayEffectSpecHandle Spec = CachedASC->MakeOutgoingSpec(EffectClass, Level, Ctx);
    return Spec.IsValid() ? CachedASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()) : FActiveGameplayEffectHandle();
}

void USuspenseCoreEquipmentComponentBase::RemoveEffect_Implementation(FActiveGameplayEffectHandle EffectHandle)
{
    if (CachedASC && EffectHandle.IsValid()) { CachedASC->RemoveActiveGameplayEffect(EffectHandle); }
}
