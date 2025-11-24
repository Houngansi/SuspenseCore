// Copyright MedCom Team. All Rights Reserved.

#include "Base/EquipmentActorBase.h"

#include "Components/EquipmentMeshComponent.h"
#include "Components/EquipmentAttributeComponent.h"
#include "Components/EquipmentAttachmentComponent.h"
#include "ItemSystem/MedComItemManager.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"

// ==============================
// Local tag cache (perf/stability)
// ==============================

namespace
{
    struct FEquipmentTagCache
    {
        // Equipment states
        FGameplayTag State_Inactive;
        FGameplayTag State_Equipped;
        FGameplayTag State_Ready;

        // Events
        FGameplayTag Event_Equipped;
        FGameplayTag Event_Unequipped;
        FGameplayTag Event_PropertyChanged;
        FGameplayTag UI_DataReady;

        // Slots
        FGameplayTag Slot_None;
        FGameplayTag Slot_PrimaryWeapon;
        FGameplayTag Slot_SecondaryWeapon;
        FGameplayTag Slot_Holster;
        FGameplayTag Slot_Scabbard;
        FGameplayTag Slot_Headwear;
        FGameplayTag Slot_Earpiece;
        FGameplayTag Slot_Eyewear;
        FGameplayTag Slot_FaceCover;
        FGameplayTag Slot_BodyArmor;
        FGameplayTag Slot_TacticalRig;
        FGameplayTag Slot_Backpack;
        FGameplayTag Slot_SecureContainer;
        FGameplayTag Slot_Quick1;
        FGameplayTag Slot_Quick2;
        FGameplayTag Slot_Quick3;
        FGameplayTag Slot_Quick4;
        FGameplayTag Slot_Armband;

        FEquipmentTagCache()
        {
            // states
            State_Inactive = FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Inactive"));
            State_Equipped = FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Equipped"));
            State_Ready    = FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Ready"));

            // events
            Event_Equipped        = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.Equipped"));
            Event_Unequipped      = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.Unequipped"));
            Event_PropertyChanged = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.PropertyChanged"));
            UI_DataReady          = FGameplayTag::RequestGameplayTag(TEXT("UI.Equipment.DataReady"));

            // slots
            Slot_None            = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.None"));
            Slot_PrimaryWeapon   = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.PrimaryWeapon"));
            Slot_SecondaryWeapon = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.SecondaryWeapon"));
            Slot_Holster         = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Holster"));
            Slot_Scabbard        = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Scabbard"));
            Slot_Headwear        = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Headwear"));
            Slot_Earpiece        = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Earpiece"));
            Slot_Eyewear         = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Eyewear"));
            Slot_FaceCover       = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.FaceCover"));
            Slot_BodyArmor       = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.BodyArmor"));
            Slot_TacticalRig     = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.TacticalRig"));
            Slot_Backpack        = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Backpack"));
            Slot_SecureContainer = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.SecureContainer"));
            Slot_Quick1          = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.QuickSlot1"));
            Slot_Quick2          = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.QuickSlot2"));
            Slot_Quick3          = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.QuickSlot3"));
            Slot_Quick4          = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.QuickSlot4"));
            Slot_Armband         = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Armband"));
        }
    };

    // renamed to avoid clash with AActor::Tags field
    static const FEquipmentTagCache& EqTags()
    {
        static FEquipmentTagCache S;
        return S;
    }
}

// ==============================
// Construction / Replication
// ==============================

AEquipmentActorBase::AEquipmentActorBase()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bNetUseOwnerRelevancy = true;

    MeshComponent       = CreateDefaultSubobject<UEquipmentMeshComponent>(TEXT("MeshComponent"));
    RootComponent       = MeshComponent;

    AttributeComponent  = CreateDefaultSubobject<UEquipmentAttributeComponent>(TEXT("AttributeComponent"));
    AttachmentComponent = CreateDefaultSubobject<UEquipmentAttachmentComponent>(TEXT("AttachmentComponent"));

    CurrentState     = EqTags().State_Inactive;
    EquipmentSlotTag = EqTags().Slot_None;
}

void AEquipmentActorBase::BeginPlay()
{
    Super::BeginPlay();
    SetReplicateMovement(false);
    bIsInitialized = true;
}

void AEquipmentActorBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AEquipmentActorBase, OwnerActor);
    DOREPLIFETIME(AEquipmentActorBase, CurrentState);

    DOREPLIFETIME_CONDITION(AEquipmentActorBase, ReplicatedItemID,        COND_SkipOwner);
    DOREPLIFETIME_CONDITION(AEquipmentActorBase, ReplicatedItemQuantity,  COND_SkipOwner);
    DOREPLIFETIME_CONDITION(AEquipmentActorBase, ReplicatedItemCondition, COND_SkipOwner);
}

// ==============================
// Public: GAS cache
// ==============================

void AEquipmentActorBase::SetCachedASC(UAbilitySystemComponent* InASC)
{
    if (!InASC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] SetCachedASC: null ASC"), *GetName());
        return;
    }

    CachedASC = InASC;
    if (PendingInit.bHasOwnerData)
    {
        PendingInit.PendingASC = InASC;
    }
}

// ==============================
// Interface: lifecycle
// ==============================

void AEquipmentActorBase::OnEquipped_Implementation(AActor* NewOwner)
{
    if (!CheckAuthority(TEXT("OnEquipped"))) { return; }
    if (!NewOwner) { UE_LOG(LogTemp, Error, TEXT("OnEquipped: Owner is null")); return; }

    EquipmentCycleCounter++;

    SetOwner(NewOwner);
    OwnerActor = NewOwner;

    if (!CachedASC)
    {
        if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(NewOwner))
        {
            CachedASC = ASI->GetAbilitySystemComponent();
        }
    }

    PendingInit.Reset();
    PendingInit.PendingOwner   = NewOwner;
    PendingInit.PendingASC     = CachedASC;
    PendingInit.bHasOwnerData  = true;

    SetEquipmentStateInternal(EqTags().State_Equipped);
    // Передаём текущий инстанс, если он уже валиден, иначе nullptr
    NotifyEquipmentEvent(EqTags().Event_Equipped, EquippedItemInstance.IsValid() ? &EquippedItemInstance : nullptr);
}

void AEquipmentActorBase::OnUnequipped_Implementation()
{
    if (!CheckAuthority(TEXT("OnUnequipped"))) { return; }

    // Сначала уведомим слушателей, пока инстанс ещё сохранён
    NotifyEquipmentEvent(EqTags().Event_Unequipped, EquippedItemInstance.IsValid() ? &EquippedItemInstance : nullptr);

    RemoveGrantedAbilities();
    RemoveAppliedEffects();

    if (AttributeComponent)  { AttributeComponent->Cleanup(); }
    if (AttachmentComponent) { AttachmentComponent->Cleanup(); }

    SetEquipmentStateInternal(EqTags().State_Inactive);

    OwnerActor   = nullptr;
    CachedASC    = nullptr;
    bFullyInitialized = false;
    PendingInit.Reset();
}

void AEquipmentActorBase::OnItemInstanceEquipped_Implementation(const FInventoryItemInstance& ItemInstance)
{
    if (!ItemInstance.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] OnItemInstanceEquipped: invalid instance"), *GetName());
        return;
    }

    EquippedItemInstance            = ItemInstance;
    PendingInit.PendingItemInstance = ItemInstance;
    PendingInit.bHasItemData        = true;

    if (HasAuthority())
    {
        ReplicatedItemID        = ItemInstance.ItemID;
        ReplicatedItemQuantity  = ItemInstance.Quantity;
        ReplicatedItemCondition = ItemInstance.GetRuntimeProperty(TEXT("Durability"), 100.f);
    }

    if (PendingInit.IsReadyToInitialize())
    {
        bFullyInitialized = false;

        InitializeEquipmentComponents(EquippedItemInstance);

        // Событие для UI/сервисов с валидным payload
        NotifyEquipmentEvent(EqTags().UI_DataReady, &EquippedItemInstance);

        bFullyInitialized = true;
        PendingInit.Reset();
    }
}

void AEquipmentActorBase::OnItemInstanceUnequipped_Implementation(const FInventoryItemInstance& ItemInstance)
{
    if (EquippedItemInstance.IsValid())
    {
        for (const auto& KV : EquippedItemInstance.RuntimeProperties)
        {
            const_cast<FInventoryItemInstance&>(ItemInstance).SetRuntimeProperty(KV.Key, KV.Value);
        }
    }

    EquippedItemInstance = FInventoryItemInstance();
}

// ==============================
// Components init (SSOT)
// ==============================

void AEquipmentActorBase::InitializeEquipmentComponents(const FInventoryItemInstance& ItemInstance)
{
    if (!ItemInstance.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[%s] InitializeEquipmentComponents: invalid instance"), *GetName());
        return;
    }
    if (!CachedASC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] InitializeEquipmentComponents: ASC not set"), *GetName());
    }

    FMedComUnifiedItemData ItemData;
    if (!GetUnifiedItemData(ItemData))
    {
        UE_LOG(LogTemp, Error, TEXT("[%s] SSOT not found for %s"), *GetName(), *ItemInstance.ItemID.ToString());
        return;
    }

    EquipmentSlotTag = ItemData.EquipmentSlot;

    if (MeshComponent)
    {
        MeshComponent->InitializeFromItemInstance(ItemInstance);
    }
    if (AttributeComponent)
    {
        AttributeComponent->InitializeWithItemInstance(this, CachedASC, ItemInstance);
    }
    if (AttachmentComponent)
    {
        AttachmentComponent->InitializeWithItemInstance(this, CachedASC, ItemInstance);
    }

    SetupEquipmentMesh(ItemData);
}

void AEquipmentActorBase::SetupEquipmentMesh(const FMedComUnifiedItemData& ItemData)
{
    if (!MeshComponent) { return; }
    if (USkeletalMesh* SK = MeshComponent->GetSkeletalMeshAsset())
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("[%s] Mesh set to %s"), *GetName(), *SK->GetName());
    }
}

// ==============================
// Interface: properties / queries
// ==============================

FInventoryItemInstance AEquipmentActorBase::GetEquippedItemInstance_Implementation() const
{
    return EquippedItemInstance;
}

FEquipmentSlotConfig AEquipmentActorBase::GetSlotConfiguration_Implementation() const
{
    const FEquipmentSlotConfig* Cfg = GetSlotConfigurationPtr();
    return Cfg ? *Cfg : FEquipmentSlotConfig();
}

const FEquipmentSlotConfig* AEquipmentActorBase::GetSlotConfigurationPtr() const
{
    return nullptr; // base has no predefined slot profile
}

EEquipmentSlotType AEquipmentActorBase::GetEquipmentSlotType_Implementation() const
{
    const FGameplayTag& T = EquipmentSlotTag;

    if (T.MatchesTagExact(EqTags().Slot_PrimaryWeapon))    return EEquipmentSlotType::PrimaryWeapon;
    if (T.MatchesTagExact(EqTags().Slot_SecondaryWeapon))  return EEquipmentSlotType::SecondaryWeapon;
    if (T.MatchesTagExact(EqTags().Slot_Holster))          return EEquipmentSlotType::Holster;
    if (T.MatchesTagExact(EqTags().Slot_Scabbard))         return EEquipmentSlotType::Scabbard;

    if (T.MatchesTagExact(EqTags().Slot_Headwear))         return EEquipmentSlotType::Headwear;
    if (T.MatchesTagExact(EqTags().Slot_Earpiece))         return EEquipmentSlotType::Earpiece;
    if (T.MatchesTagExact(EqTags().Slot_Eyewear))          return EEquipmentSlotType::Eyewear;
    if (T.MatchesTagExact(EqTags().Slot_FaceCover))        return EEquipmentSlotType::FaceCover;

    if (T.MatchesTagExact(EqTags().Slot_BodyArmor))        return EEquipmentSlotType::BodyArmor;
    if (T.MatchesTagExact(EqTags().Slot_TacticalRig))      return EEquipmentSlotType::TacticalRig;

    if (T.MatchesTagExact(EqTags().Slot_Backpack))         return EEquipmentSlotType::Backpack;
    if (T.MatchesTagExact(EqTags().Slot_SecureContainer))  return EEquipmentSlotType::SecureContainer;

    if (T.MatchesTagExact(EqTags().Slot_Quick1))           return EEquipmentSlotType::QuickSlot1;
    if (T.MatchesTagExact(EqTags().Slot_Quick2))           return EEquipmentSlotType::QuickSlot2;
    if (T.MatchesTagExact(EqTags().Slot_Quick3))           return EEquipmentSlotType::QuickSlot3;
    if (T.MatchesTagExact(EqTags().Slot_Quick4))           return EEquipmentSlotType::QuickSlot4;

    if (T.MatchesTagExact(EqTags().Slot_Armband))          return EEquipmentSlotType::Armband;

    return EEquipmentSlotType::None;
}

FGameplayTag AEquipmentActorBase::GetEquipmentSlotTag_Implementation() const
{
    return EquipmentSlotTag;
}

bool AEquipmentActorBase::IsEquipped_Implementation() const
{
    return (OwnerActor != nullptr) && EquippedItemInstance.IsValid();
}

bool AEquipmentActorBase::IsRequiredSlot_Implementation() const
{
    const FEquipmentSlotConfig* Cfg = GetSlotConfigurationPtr();
    return Cfg ? Cfg->bIsRequired : false;
}

FText AEquipmentActorBase::GetSlotDisplayName_Implementation() const
{
    const FEquipmentSlotConfig* Cfg = GetSlotConfigurationPtr();
    return Cfg ? Cfg->DisplayName : FText::GetEmpty();
}

FName AEquipmentActorBase::GetAttachmentSocket_Implementation() const
{
    if (!EquippedItemInstance.IsValid()) { return NAME_None; }
    FMedComUnifiedItemData ItemData;
    return GetUnifiedItemData(ItemData) ? ItemData.AttachmentSocket : NAME_None;
}

FTransform AEquipmentActorBase::GetAttachmentOffset_Implementation() const
{
    if (!EquippedItemInstance.IsValid()) { return FTransform::Identity; }
    FMedComUnifiedItemData ItemData;
    return GetUnifiedItemData(ItemData) ? ItemData.AttachmentOffset : FTransform::Identity;
}

bool AEquipmentActorBase::CanEquipItemInstance_Implementation(const FInventoryItemInstance& ItemInstance) const
{
    if (!ItemInstance.IsValid()) { return false; }

    if (const UMedComItemManager* IM = GetItemManager())
    {
        FMedComUnifiedItemData Data;
        if (!IM->GetUnifiedItemData(ItemInstance.ItemID, Data)) { return false; }
        if (!Data.bIsEquippable) { return false; }
        if (!Data.EquipmentSlot.MatchesTag(EquipmentSlotTag)) { return false; }
        return true;
    }
    return false;
}

FGameplayTagContainer AEquipmentActorBase::GetAllowedItemTypes_Implementation() const
{
    const FEquipmentSlotConfig* Cfg = GetSlotConfigurationPtr();
    return Cfg ? Cfg->AllowedItemTypes : FGameplayTagContainer();
}

bool AEquipmentActorBase::ValidateEquipmentRequirements_Implementation(TArray<FString>& OutErrors) const
{
    OutErrors.Empty();

    if (!EquippedItemInstance.IsValid())
    {
        if (IsRequiredSlot_Implementation())
        {
            OutErrors.Add(FString::Printf(TEXT("Required slot %s is empty"), *EquipmentSlotTag.ToString()));
            return false;
        }
        return true;
    }

    FMedComUnifiedItemData Data;
    if (!GetUnifiedItemData(Data))
    {
        OutErrors.Add(TEXT("Failed to load SSOT row"));
        return false;
    }
    if (!Data.EquipmentSlot.MatchesTag(EquipmentSlotTag))
    {
        OutErrors.Add(FString::Printf(TEXT("Slot mismatch: expected %s, got %s"),
                           *EquipmentSlotTag.ToString(), *Data.EquipmentSlot.ToString()));
        return false;
    }
    return true;
}

// ==============================
// Interface: operations
// ==============================

FInventoryOperationResult AEquipmentActorBase::EquipItemInstance_Implementation(const FInventoryItemInstance& ItemInstance, bool bForceEquip)
{
    FInventoryOperationResult R;

    if (!ItemInstance.IsValid())
    {
        R.bSuccess = false; R.ErrorCode = EInventoryErrorCode::InvalidItem;
        R.ErrorMessage = FText::FromString(TEXT("Invalid item instance"));
        return R;
    }

    if (!bForceEquip && !CanEquipItemInstance_Implementation(ItemInstance))
    {
        R.bSuccess = false; R.ErrorCode = EInventoryErrorCode::InvalidSlot;
        R.ErrorMessage = FText::FromString(TEXT("Item cannot be equipped in this slot"));
        return R;
    }

    if (EquippedItemInstance.IsValid())
    {
        FInventoryItemInstance Tmp;
        UnequipItem_Implementation(Tmp);
    }

    OnItemInstanceEquipped_Implementation(ItemInstance);

    R.bSuccess = true;
    R.ErrorCode = EInventoryErrorCode::Success;
    R.AffectedItems.Add(ItemInstance);
    return R;
}

FInventoryOperationResult AEquipmentActorBase::UnequipItem_Implementation(FInventoryItemInstance& OutUnequippedInstance)
{
    FInventoryOperationResult R;

    if (!EquippedItemInstance.IsValid())
    {
        R.bSuccess = false; R.ErrorCode = EInventoryErrorCode::ItemNotFound;
        R.ErrorMessage = FText::FromString(TEXT("No item equipped"));
        return R;
    }

    OutUnequippedInstance = EquippedItemInstance;
    OnItemInstanceUnequipped_Implementation(OutUnequippedInstance);

    R.bSuccess = true;
    R.ErrorCode = EInventoryErrorCode::Success;
    R.AffectedItems.Add(OutUnequippedInstance);
    return R;
}

FInventoryOperationResult AEquipmentActorBase::SwapEquipmentWith_Implementation(const TScriptInterface<IMedComEquipmentInterface>& OtherEquipment)
{
    FInventoryOperationResult R;

    if (!OtherEquipment)
    {
        R.bSuccess = false; R.ErrorCode = EInventoryErrorCode::InvalidSlot;
        R.ErrorMessage = FText::FromString(TEXT("Invalid target equipment"));
        return R;
    }

    const FInventoryItemInstance ThisItem  = EquippedItemInstance;
    const FInventoryItemInstance OtherItem = IMedComEquipmentInterface::Execute_GetEquippedItemInstance(OtherEquipment.GetObject());

    const bool bThisCanEquipOther  = !OtherItem.IsValid() || CanEquipItemInstance_Implementation(OtherItem);
    const bool bOtherCanEquipThis  = !ThisItem.IsValid() || IMedComEquipmentInterface::Execute_CanEquipItemInstance(OtherEquipment.GetObject(), ThisItem);

    if (!bThisCanEquipOther || !bOtherCanEquipThis)
    {
        R.bSuccess = false; R.ErrorCode = EInventoryErrorCode::InvalidSlot;
        R.ErrorMessage = FText::FromString(TEXT("Items cannot be swapped between these slots"));
        return R;
    }

    FInventoryItemInstance Tmp;
    if (ThisItem.IsValid())
    {
        UnequipItem_Implementation(Tmp);
        R.AffectedItems.Add(Tmp);
    }
    if (OtherItem.IsValid())
    {
        FInventoryItemInstance OtherUnequipped;
        IMedComEquipmentInterface::Execute_UnequipItem(OtherEquipment.GetObject(), OtherUnequipped);
        EquipItemInstance_Implementation(OtherUnequipped, false);
        R.AffectedItems.Add(OtherUnequipped);
    }
    if (ThisItem.IsValid())
    {
        IMedComEquipmentInterface::Execute_EquipItemInstance(OtherEquipment.GetObject(), Tmp, false);
    }

    R.bSuccess = true;
    R.ErrorCode = EInventoryErrorCode::Success;
    return R;
}

// ==============================
// GAS (proxy)
// ==============================

UAbilitySystemComponent* AEquipmentActorBase::GetAbilitySystemComponent_Implementation() const
{
    return CachedASC;
}

UAttributeSet* AEquipmentActorBase::GetEquipmentAttributeSet_Implementation() const
{
    return AttributeComponent ? AttributeComponent->GetAttributeSet() : nullptr;
}

TArray<TSubclassOf<UGameplayAbility>> AEquipmentActorBase::GetGrantedAbilities_Implementation() const
{
    TArray<TSubclassOf<UGameplayAbility>> Abilities;
    if (!EquippedItemInstance.IsValid()) { return Abilities; }

    FMedComUnifiedItemData Data;
    if (GetUnifiedItemData(Data))
    {
        for (const FGrantedAbilityData& GA : Data.GrantedAbilities)
        {
            if (GA.AbilityClass) { Abilities.Add(GA.AbilityClass); }
        }
    }
    return Abilities;
}

TArray<TSubclassOf<UGameplayEffect>> AEquipmentActorBase::GetPassiveEffects_Implementation() const
{
    TArray<TSubclassOf<UGameplayEffect>> Effects;
    if (!EquippedItemInstance.IsValid()) { return Effects; }

    FMedComUnifiedItemData Data;
    if (GetUnifiedItemData(Data))
    {
        Effects = Data.PassiveEffects;
    }
    return Effects;
}

// S3: NO-OPs (services will grant/remove on events)
void AEquipmentActorBase::ApplyEquipmentEffects_Implementation()      { ApplyInitializationEffects(); /* intentionally empty */ }
void AEquipmentActorBase::RemoveEquipmentEffects_Implementation()     { RemoveGrantedAbilities(); RemoveAppliedEffects(); }
void AEquipmentActorBase::GrantAbilitiesFromItemData()                { GrantedAbilityHandles.Reset(); }
void AEquipmentActorBase::ApplyPassiveEffectsFromItemData()           { AppliedEffectHandles.Reset(); }
void AEquipmentActorBase::ApplyInitializationEffects()                {}
void AEquipmentActorBase::RemoveGrantedAbilities()                    { GrantedAbilityHandles.Reset(); }
void AEquipmentActorBase::RemoveAppliedEffects()                      { AppliedEffectHandles.Reset(); }

// ==============================
// State
// ==============================

FGameplayTag AEquipmentActorBase::GetCurrentEquipmentState_Implementation() const
{
    return CurrentState;
}

bool AEquipmentActorBase::SetEquipmentState_Implementation(const FGameplayTag& NewState, bool bForceTransition)
{
    if (!bForceTransition && CurrentState == NewState) { return false; }
    CurrentState = NewState;
    NotifyEquipmentStateChanged(NewState, /*bIsRefresh*/false);
    return true;
}

bool AEquipmentActorBase::IsInEquipmentState_Implementation(const FGameplayTag& StateTag) const
{
    return CurrentState.MatchesTag(StateTag);
}

TArray<FGameplayTag> AEquipmentActorBase::GetAvailableStateTransitions_Implementation() const
{
    TArray<FGameplayTag> Out;
    if (CurrentState.MatchesTagExact(EqTags().State_Inactive))
    {
        Out.Add(EqTags().State_Equipped);
    }
    else if (CurrentState.MatchesTagExact(EqTags().State_Equipped))
    {
        Out.Add(EqTags().State_Ready);
        Out.Add(EqTags().State_Inactive);
    }
    else if (CurrentState.MatchesTagExact(EqTags().State_Ready))
    {
        Out.Add(EqTags().State_Equipped);
    }
    return Out;
}

void AEquipmentActorBase::SetEquipmentStateInternal(const FGameplayTag& NewState)
{
    SetEquipmentState_Implementation(NewState, false);
}

// ==============================
// Runtime properties
// ==============================

float AEquipmentActorBase::GetEquipmentRuntimeProperty_Implementation(const FName& PropertyName, float DefaultValue) const
{
    return EquippedItemInstance.IsValid()
        ? EquippedItemInstance.GetRuntimeProperty(PropertyName, DefaultValue)
        : DefaultValue;
}

void AEquipmentActorBase::SetEquipmentRuntimeProperty_Implementation(const FName& PropertyName, float Value)
{
    if (!EquippedItemInstance.IsValid()) { return; }
    const float Old = EquippedItemInstance.GetRuntimeProperty(PropertyName, 0.f);
    EquippedItemInstance.SetRuntimeProperty(PropertyName, Value);
    IMedComEquipmentInterface::BroadcastEquipmentPropertyChanged(this, PropertyName, Old, Value);
}

float AEquipmentActorBase::GetEquipmentConditionPercent_Implementation() const
{
    if (!EquippedItemInstance.IsValid()) { return 1.f; }
    const float MaxDur = EquippedItemInstance.GetRuntimeProperty(TEXT("MaxDurability"), 100.f);
    const float CurDur = EquippedItemInstance.GetRuntimeProperty(TEXT("Durability"),   MaxDur);
    return (MaxDur > 0.f) ? FMath::Clamp(CurDur / MaxDur, 0.f, 1.f) : 1.f;
}

// ==============================
// Weapon helpers (read-only)
// ==============================

bool AEquipmentActorBase::IsWeaponEquipment_Implementation() const
{
    if (!EquippedItemInstance.IsValid()) { return false; }
    FMedComUnifiedItemData Data;
    return GetUnifiedItemData(Data) ? Data.bIsWeapon : false;
}

FGameplayTag AEquipmentActorBase::GetWeaponArchetype_Implementation() const
{
    if (!EquippedItemInstance.IsValid()) { return FGameplayTag(); }
    FMedComUnifiedItemData Data;
    return GetUnifiedItemData(Data) ? Data.WeaponArchetype : FGameplayTag();
}

bool AEquipmentActorBase::CanFireWeapon_Implementation() const
{
    if (!IsWeaponEquipment_Implementation()) { return false; }
    return CurrentState.MatchesTag(EqTags().State_Ready);
}

// ==============================
// Spawn-side init helper
// ==============================

bool AEquipmentActorBase::InitializeFromItemInstance(const FInventoryItemInstance& ItemInstance)
{
    if (!ItemInstance.IsValid()) { return false; }

    EquippedItemInstance = ItemInstance;

    FMedComUnifiedItemData Data;
    if (!GetUnifiedItemData(Data)) { return false; }

    EquipmentSlotTag = Data.EquipmentSlot;

    InitializeEquipmentComponents(ItemInstance);
    SetupEquipmentMesh(Data);

    NotifyEquipmentEvent(EqTags().UI_DataReady, &EquippedItemInstance);
    return true;
}

// ==============================
// Replication notifications
// ==============================

void AEquipmentActorBase::OnRep_ItemData()
{
    if (HasAuthority()) { return; }
    if (ReplicatedItemID == NAME_None) { return; }

    if (!EquippedItemInstance.IsValid())
    {
        EquippedItemInstance.ItemID   = ReplicatedItemID;
        EquippedItemInstance.Quantity = ReplicatedItemQuantity;
        EquippedItemInstance.SetRuntimeProperty(TEXT("Durability"), ReplicatedItemCondition);
    }

    if (UMedComItemManager* IM = GetItemManager())
    {
        FMedComUnifiedItemData Data;
        if (IM->GetUnifiedItemData(ReplicatedItemID, Data))
        {
            SetupEquipmentMesh(Data);
            NotifyEquipmentEvent(EqTags().UI_DataReady, &EquippedItemInstance);
        }
    }
}

// ==============================
// Events / SSOT helpers
// ==============================

void AEquipmentActorBase::NotifyEquipmentEvent(const FGameplayTag& EventTag, const FInventoryItemInstance* Payload) const
{
    IMedComEquipmentInterface::BroadcastEquipmentOperationEvent(this, EventTag, Payload);
}

void AEquipmentActorBase::NotifyEquipmentStateChanged(const FGameplayTag& NewState, bool bIsRefresh) const
{
    // В текущей сигнатуре шлём только событие PropertyChanged с контекстом предмета,
    // слушатели читают текущее состояние через интерфейс актора.
    const FInventoryItemInstance* Payload = EquippedItemInstance.IsValid() ? &EquippedItemInstance : nullptr;
    IMedComEquipmentInterface::BroadcastEquipmentOperationEvent(this, EqTags().Event_PropertyChanged, Payload);
}

bool AEquipmentActorBase::GetUnifiedItemData(FMedComUnifiedItemData& OutData) const
{
    if (!EquippedItemInstance.IsValid()) { return false; }
    if (UMedComItemManager* IM = GetItemManager())
    {
        return IM->GetUnifiedItemData(EquippedItemInstance.ItemID, OutData);
    }
    return false;
}

UMedComItemManager* AEquipmentActorBase::GetItemManager() const
{
    if (const UWorld* W = GetWorld())
    {
        if (UGameInstance* GI = W->GetGameInstance())
        {
            return GI->GetSubsystem<UMedComItemManager>();
        }
    }
    return nullptr;
}
