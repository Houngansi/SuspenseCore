// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreEquipmentAttributeComponent.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "AttributeSet.h"
#include "SuspenseCore/ItemSystem/SuspenseCoreItemManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Attributes/SuspenseCoreWeaponAttributeSet.h"
#include "SuspenseCore/Attributes/SuspenseCoreAmmoAttributeSet.h"
#include "SuspenseCore/Types/GAS/SuspenseCoreGASAttributeRows.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "GameplayTagsManager.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystemGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogEquipmentAttributeComponent, Log, All);

USuspenseCoreEquipmentAttributeComponent::USuspenseCoreEquipmentAttributeComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);

    CurrentAttributeSet = nullptr;
    WeaponAttributeSet = nullptr;
    ArmorAttributeSet = nullptr;
    AmmoAttributeSet = nullptr;
    AttributeReplicationVersion = 0;
    NextAttributePredictionKey = 1;
}

void USuspenseCoreEquipmentAttributeComponent::BeginPlay()
{
    Super::BeginPlay();

    // Start periodic attribute collection on server
    if (GetOwner() && GetOwner()->HasAuthority() && GetWorld())
    {
        FTimerHandle TimerHandle;
        GetWorld()->GetTimerManager().SetTimer(
            TimerHandle,
            this,
            &USuspenseCoreEquipmentAttributeComponent::CollectReplicatedAttributes,
            0.1f, // Update 10 times per second
            true
        );
    }
}

void USuspenseCoreEquipmentAttributeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(USuspenseCoreEquipmentAttributeComponent, ReplicatedAttributes);
    DOREPLIFETIME(USuspenseCoreEquipmentAttributeComponent, ReplicatedAttributeSetClasses);
    DOREPLIFETIME(USuspenseCoreEquipmentAttributeComponent, AttributeReplicationVersion);
}

void USuspenseCoreEquipmentAttributeComponent::InitializeWithItemInstance(AActor* InOwner, UAbilitySystemComponent* InASC, const FSuspenseCoreInventoryItemInstance& ItemInstance)
{
    // Call base initialization first
    Super::InitializeWithItemInstance(InOwner, InASC, ItemInstance);

    if (!IsInitialized())
    {
        EQUIPMENT_LOG(Error, TEXT("Failed to initialize base component"));
        return;
    }

    // Get item data from DataTable - this is our source of truth
    FSuspenseCoreUnifiedItemData ItemData;
    if (!GetEquippedItemData(ItemData))
    {
        EQUIPMENT_LOG(Error, TEXT("Failed to get item data for: %s"), *ItemInstance.ItemID.ToString());
        return;
    }

    // Create attribute sets based on item configuration in DataTable
    CreateAttributeSetsForItem(ItemData);

    // Apply all item effects including passive effects and granted abilities
    ApplyItemEffects(ItemData);

    // Force initial replication to sync clients
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        CollectReplicatedAttributes();
        AttributeReplicationVersion++;

        if (AActor* Owner = GetOwner())
        {
            Owner->ForceNetUpdate();
        }
    }

    EQUIPMENT_LOG(Log, TEXT("Initialized attributes for item: %s"), *ItemInstance.ItemID.ToString());
}

void USuspenseCoreEquipmentAttributeComponent::Cleanup()
{
    // Remove all effects first before destroying AttributeSets
    RemoveItemEffects();

    // Clean up attribute sets
    CleanupAttributeSets();

    // Clear all cached and replicated data
    ReplicatedAttributes.Empty();
    ReplicatedAttributeSetClasses.Empty();
    ActiveAttributePredictions.Empty();
    AttributePropertyCache.Empty();

    // Call base cleanup last
    Super::Cleanup();
}

void USuspenseCoreEquipmentAttributeComponent::UpdateEquippedItem(const FSuspenseCoreInventoryItemInstance& ItemInstance)
{
    // Update base item
    Super::UpdateEquippedItem(ItemInstance);

    // Reinitialize with new item if valid
    if (ItemInstance.IsValid())
    {
        FSuspenseCoreUnifiedItemData ItemData;
        if (GetEquippedItemData(ItemData))
        {
            // Clean up old AttributeSets
            CleanupAttributeSets();
            RemoveItemEffects();

            // Create new AttributeSets for new item
            CreateAttributeSetsForItem(ItemData);
            ApplyItemEffects(ItemData);

            // Force replication update
            if (GetOwner() && GetOwner()->HasAuthority())
            {
                CollectReplicatedAttributes();
                AttributeReplicationVersion++;

                if (AActor* Owner = GetOwner())
                {
                    Owner->ForceNetUpdate();
                }
            }
        }
    }
}

void USuspenseCoreEquipmentAttributeComponent::OnEquipmentInitialized()
{
    Super::OnEquipmentInitialized();

    EQUIPMENT_LOG(Log, TEXT("Equipment attributes component initialized"));
}

void USuspenseCoreEquipmentAttributeComponent::OnEquippedItemChanged(const FSuspenseCoreInventoryItemInstance& OldItem, const FSuspenseCoreInventoryItemInstance& NewItem)
{
    Super::OnEquippedItemChanged(OldItem, NewItem);

    // Additional handling if needed when item changes
}

void USuspenseCoreEquipmentAttributeComponent::CreateAttributeSetsForItem(const FSuspenseCoreUnifiedItemData& ItemData)
{
    if (!CachedASC)
    {
        EQUIPMENT_LOG(Error, TEXT("Cannot create attribute sets - no ASC"));
        return;
    }

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        EQUIPMENT_LOG(Error, TEXT("Cannot create attribute sets - no owner"));
        return;
    }

    // Clean up any existing sets first
    CleanupAttributeSets();

    // Clear replicated classes list
    ReplicatedAttributeSetClasses.Empty();

    // Get DataManager for SSOT initialization
    USuspenseCoreDataManager* DataManager = nullptr;
    const bool bShouldUseSSOT = ItemData.ShouldUseSSOTInitialization();

    UE_LOG(LogEquipmentAttributeComponent, Log,
        TEXT("CreateAttributeSets for %s: bIsWeapon=%s, ShouldUseSSOT=%s, WeaponAttributesRowName=%s"),
        *ItemData.DisplayName.ToString(),
        ItemData.bIsWeapon ? TEXT("Yes") : TEXT("No"),
        bShouldUseSSOT ? TEXT("Yes") : TEXT("No"),
        *ItemData.WeaponAttributesRowName.ToString());

    if (bShouldUseSSOT)
    {
        DataManager = USuspenseCoreDataManager::Get(this);
        if (!DataManager)
        {
            UE_LOG(LogEquipmentAttributeComponent, Warning,
                TEXT("SSOT initialization requested but DataManager not available, falling back to legacy"));
        }
        else
        {
            UE_LOG(LogEquipmentAttributeComponent, Log,
                TEXT("DataManager available: WeaponAttrReady=%s, AmmoAttrReady=%s"),
                DataManager->IsWeaponAttributesSystemReady() ? TEXT("Yes") : TEXT("No"),
                DataManager->IsAmmoAttributesSystemReady() ? TEXT("Yes") : TEXT("No"));
        }
    }

    // Create weapon-specific attributes
    if (ItemData.bIsWeapon)
    {
        bool bSSOTInitialized = false;

        // Try SSOT initialization first
        if (DataManager && DataManager->IsWeaponAttributesSystemReady())
        {
            FName AttributeKey = ItemData.GetWeaponAttributesKey();
            UE_LOG(LogEquipmentAttributeComponent, Log,
                TEXT("SSOT lookup: AttributeKey=%s"), *AttributeKey.ToString());
            FSuspenseCoreWeaponAttributeRow WeaponRowData;

            if (DataManager->GetWeaponAttributes(AttributeKey, WeaponRowData))
            {
                // Create weapon AttributeSet using SSOT class
                USuspenseCoreWeaponAttributeSet* TypedWeaponSet = NewObject<USuspenseCoreWeaponAttributeSet>(Owner);
                TypedWeaponSet->InitializeFromData(WeaponRowData);

                WeaponAttributeSet = TypedWeaponSet;
                CachedASC->AddAttributeSetSubobject(WeaponAttributeSet);
                AttributeSetsByType.Add(SuspenseCoreEquipmentTags::AttributeSet::TAG_AttributeSet_Weapon, WeaponAttributeSet);
                CurrentAttributeSet = WeaponAttributeSet;
                ReplicatedAttributeSetClasses.Add(USuspenseCoreWeaponAttributeSet::StaticClass());

                bSSOTInitialized = true;

                UE_LOG(LogEquipmentAttributeComponent, Log,
                    TEXT("SSOT: Created weapon attributes for %s from DataTable row %s"),
                    *ItemData.DisplayName.ToString(), *AttributeKey.ToString());
            }
            else
            {
                UE_LOG(LogEquipmentAttributeComponent, Warning,
                    TEXT("SSOT: WeaponAttributes row not found: %s, falling back to legacy"),
                    *AttributeKey.ToString());
            }
        }

        // Fallback to legacy initialization
        if (!bSSOTInitialized && ItemData.WeaponInitialization.WeaponAttributeSetClass)
        {
            WeaponAttributeSet = NewObject<UAttributeSet>(Owner, ItemData.WeaponInitialization.WeaponAttributeSetClass);
            CachedASC->AddAttributeSetSubobject(WeaponAttributeSet);
            AttributeSetsByType.Add(SuspenseCoreEquipmentTags::AttributeSet::TAG_AttributeSet_Weapon, WeaponAttributeSet);
            CurrentAttributeSet = WeaponAttributeSet;
            ReplicatedAttributeSetClasses.Add(ItemData.WeaponInitialization.WeaponAttributeSetClass);

            if (ItemData.WeaponInitialization.WeaponInitEffect)
            {
                ApplyInitializationEffect(WeaponAttributeSet, ItemData.WeaponInitialization.WeaponInitEffect, ItemData);
            }

            UE_LOG(LogEquipmentAttributeComponent, Log,
                TEXT("Legacy: Created weapon attributes for %s using AttributeSetClass"),
                *ItemData.DisplayName.ToString());
        }

        // Warning if no weapon attributes were created
        if (!bSSOTInitialized && !WeaponAttributeSet)
        {
            UE_LOG(LogEquipmentAttributeComponent, Warning,
                TEXT("NO WEAPON ATTRIBUTES CREATED for %s! SSOT row: %s, Legacy class: %s"),
                *ItemData.DisplayName.ToString(),
                *ItemData.WeaponAttributesRowName.ToString(),
                ItemData.WeaponInitialization.WeaponAttributeSetClass ? TEXT("Valid") : TEXT("None"));
        }

        // Create ammo attributes if specified (SSOT or legacy)
        if (ItemData.AmmoAttributeSet)
        {
            AmmoAttributeSet = NewObject<UAttributeSet>(Owner, ItemData.AmmoAttributeSet);
            CachedASC->AddAttributeSetSubobject(AmmoAttributeSet);
            AttributeSetsByType.Add(SuspenseCoreEquipmentTags::AttributeSet::TAG_AttributeSet_Ammo, AmmoAttributeSet);
            ReplicatedAttributeSetClasses.Add(ItemData.AmmoAttributeSet);
        }

        EQUIPMENT_LOG(Log, TEXT("Created weapon attribute sets for: %s"), *ItemData.DisplayName.ToString());
    }
    // Create armor-specific attributes
    else if (ItemData.bIsArmor)
    {
        bool bSSOTInitialized = false;

        // Try SSOT initialization first
        if (DataManager && DataManager->IsArmorAttributesSystemReady())
        {
            FName AttributeKey = ItemData.GetArmorAttributesKey();
            FSuspenseCoreArmorAttributeRow ArmorRowData;

            if (DataManager->GetArmorAttributes(AttributeKey, ArmorRowData))
            {
                // TODO: Create armor AttributeSet using SSOT class
                // USuspenseCoreArmorAttributeSet* TypedArmorSet = NewObject<USuspenseCoreArmorAttributeSet>(Owner);
                // TypedArmorSet->InitializeFromData(ArmorRowData);
                // For now, log that SSOT armor is not yet implemented

                UE_LOG(LogEquipmentAttributeComponent, Log,
                    TEXT("SSOT: Armor attributes found for %s, but InitializeFromData not yet implemented"),
                    *ItemData.DisplayName.ToString());
            }
        }

        // Fallback to legacy initialization
        if (!bSSOTInitialized && ItemData.ArmorInitialization.ArmorAttributeSetClass)
        {
            ArmorAttributeSet = NewObject<UAttributeSet>(Owner, ItemData.ArmorInitialization.ArmorAttributeSetClass);
            CachedASC->AddAttributeSetSubobject(ArmorAttributeSet);
            AttributeSetsByType.Add(SuspenseCoreEquipmentTags::AttributeSet::TAG_AttributeSet_Armor, ArmorAttributeSet);
            CurrentAttributeSet = ArmorAttributeSet;
            ReplicatedAttributeSetClasses.Add(ItemData.ArmorInitialization.ArmorAttributeSetClass);

            if (ItemData.ArmorInitialization.ArmorInitEffect)
            {
                ApplyInitializationEffect(ArmorAttributeSet, ItemData.ArmorInitialization.ArmorInitEffect, ItemData);
            }

            EQUIPMENT_LOG(Log, TEXT("Legacy: Created armor attribute sets for: %s"), *ItemData.DisplayName.ToString());
        }
    }
    // Create ammo-specific attributes (for ammo items, not weapon ammo)
    else if (ItemData.bIsAmmo)
    {
        bool bSSOTInitialized = false;

        // Try SSOT initialization first
        if (DataManager && DataManager->IsAmmoAttributesSystemReady())
        {
            FName AttributeKey = ItemData.GetAmmoAttributesKey();
            FSuspenseCoreAmmoAttributeRow AmmoRowData;

            if (DataManager->GetAmmoAttributes(AttributeKey, AmmoRowData))
            {
                USuspenseCoreAmmoAttributeSet* TypedAmmoSet = NewObject<USuspenseCoreAmmoAttributeSet>(Owner);
                TypedAmmoSet->InitializeFromData(AmmoRowData);

                AmmoAttributeSet = TypedAmmoSet;
                CachedASC->AddAttributeSetSubobject(AmmoAttributeSet);
                AttributeSetsByType.Add(SuspenseCoreEquipmentTags::AttributeSet::TAG_AttributeSet_Ammo, AmmoAttributeSet);
                CurrentAttributeSet = AmmoAttributeSet;
                ReplicatedAttributeSetClasses.Add(USuspenseCoreAmmoAttributeSet::StaticClass());

                bSSOTInitialized = true;

                UE_LOG(LogEquipmentAttributeComponent, Log,
                    TEXT("SSOT: Created ammo attributes for %s from DataTable row %s"),
                    *ItemData.DisplayName.ToString(), *AttributeKey.ToString());
            }
        }

        // Fallback to legacy initialization
        if (!bSSOTInitialized && ItemData.AmmoInitialization.AmmoAttributeSetClass)
        {
            AmmoAttributeSet = NewObject<UAttributeSet>(Owner, ItemData.AmmoInitialization.AmmoAttributeSetClass);
            CachedASC->AddAttributeSetSubobject(AmmoAttributeSet);
            AttributeSetsByType.Add(SuspenseCoreEquipmentTags::AttributeSet::TAG_AttributeSet_Ammo, AmmoAttributeSet);
            CurrentAttributeSet = AmmoAttributeSet;
            ReplicatedAttributeSetClasses.Add(ItemData.AmmoInitialization.AmmoAttributeSetClass);

            if (ItemData.AmmoInitialization.AmmoInitEffect)
            {
                ApplyInitializationEffect(AmmoAttributeSet, ItemData.AmmoInitialization.AmmoInitEffect, ItemData);
            }

            EQUIPMENT_LOG(Log, TEXT("Legacy: Created ammo attribute sets for: %s"), *ItemData.DisplayName.ToString());
        }
    }
    // Create general equipment attributes
    else if (ItemData.bIsEquippable && ItemData.EquipmentAttributeSet)
    {
        CurrentAttributeSet = NewObject<UAttributeSet>(Owner, ItemData.EquipmentAttributeSet);
        CachedASC->AddAttributeSetSubobject(CurrentAttributeSet);
        AttributeSetsByType.Add(SuspenseCoreEquipmentTags::AttributeSet::TAG_AttributeSet_Equipment, CurrentAttributeSet);
        ReplicatedAttributeSetClasses.Add(ItemData.EquipmentAttributeSet);

        if (ItemData.EquipmentInitEffect)
        {
            ApplyInitializationEffect(CurrentAttributeSet, ItemData.EquipmentInitEffect, ItemData);
        }

        EQUIPMENT_LOG(Log, TEXT("Created equipment attribute sets for: %s"), *ItemData.DisplayName.ToString());
    }

    // Force attribute system to update
    if (CachedASC)
    {
        CachedASC->ForceReplication();
    }
}

void USuspenseCoreEquipmentAttributeComponent::CleanupAttributeSets()
{
    if (!CachedASC)
    {
        return;
    }

    // Remove all attribute sets from ASC
    if (CurrentAttributeSet)
    {
        CachedASC->RemoveSpawnedAttribute(CurrentAttributeSet);
        CurrentAttributeSet = nullptr;
    }

    if (WeaponAttributeSet)
    {
        CachedASC->RemoveSpawnedAttribute(WeaponAttributeSet);
        WeaponAttributeSet = nullptr;
    }

    if (ArmorAttributeSet)
    {
        CachedASC->RemoveSpawnedAttribute(ArmorAttributeSet);
        ArmorAttributeSet = nullptr;
    }

    if (AmmoAttributeSet)
    {
        CachedASC->RemoveSpawnedAttribute(AmmoAttributeSet);
        AmmoAttributeSet = nullptr;
    }

    AttributeSetsByType.Empty();
    AttributePropertyCache.Empty();

    EQUIPMENT_LOG(Log, TEXT("Cleaned up all attribute sets"));
}

void USuspenseCoreEquipmentAttributeComponent::ApplyInitializationEffect(UAttributeSet* AttributeSet, TSubclassOf<UGameplayEffect> InitEffect, const FSuspenseCoreUnifiedItemData& ItemData)
{
    if (!AttributeSet || !InitEffect || !CachedASC)
    {
        return;
    }

    // Create effect context with item data
    FGameplayEffectContextHandle Context = CachedASC->MakeEffectContext();
    Context.AddSourceObject(this);

    // Create effect spec
    FGameplayEffectSpecHandle Spec = CachedASC->MakeOutgoingSpec(InitEffect, 1.0f, Context);

    if (Spec.IsValid())
    {
        // Add item-specific data to the effect spec if needed
        // For example, item level, rarity, etc. could modify initial values

        // Apply the initialization effect
        FActiveGameplayEffectHandle Handle = CachedASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);

        if (Handle.IsValid())
        {
            // Initialization effects are instant, so they don't need to be tracked
            EQUIPMENT_LOG(Log, TEXT("Applied initialization effect %s to AttributeSet %s"),
                *GetNameSafe(InitEffect),
                *GetNameSafe(AttributeSet->GetClass()));
        }
    }
}

void USuspenseCoreEquipmentAttributeComponent::ApplyItemEffects(const FSuspenseCoreUnifiedItemData& ItemData)
{
    if (!CachedASC)
    {
        EQUIPMENT_LOG(Warning, TEXT("Cannot apply effects - no ASC"));
        return;
    }

    // Apply passive effects for equipped items
    if (ItemData.bIsEquippable)
    {
        ApplyPassiveEffects(ItemData);
    }

    // Apply ammo projectile effects
    if (ItemData.bIsAmmo)
    {
        for (const TSubclassOf<UGameplayEffect>& EffectClass : ItemData.ProjectileEffects)
        {
            if (EffectClass)
            {
                FActiveGameplayEffectHandle Handle = ApplyEffectToSelf(EffectClass, 1.0f);
                if (Handle.IsValid())
                {
                    AppliedEffectHandles.Add(Handle);
                }
            }
        }
    }

    // Grant abilities from equipped item
    ApplyGrantedAbilities(ItemData);

    EQUIPMENT_LOG(Log, TEXT("Applied %d effects and granted %d abilities from item %s"),
        AppliedEffectHandles.Num(),
        GrantedAbilityHandles.Num(),
        *ItemData.DisplayName.ToString());
}

void USuspenseCoreEquipmentAttributeComponent::RemoveItemEffects()
{
    if (!CachedASC)
    {
        return;
    }

    // Remove all applied effects
    for (const FActiveGameplayEffectHandle& Handle : AppliedEffectHandles)
    {
        if (Handle.IsValid())
        {
            RemoveEffect(Handle);
        }
    }
    AppliedEffectHandles.Empty();

    // Remove all granted abilities
    for (const FGameplayAbilitySpecHandle& Handle : GrantedAbilityHandles)
    {
        if (Handle.IsValid())
        {
            RemoveAbility(Handle);
        }
    }
    GrantedAbilityHandles.Empty();

    EQUIPMENT_LOG(Log, TEXT("Removed all item effects and abilities"));
}

void USuspenseCoreEquipmentAttributeComponent::ApplyPassiveEffects(const FSuspenseCoreUnifiedItemData& ItemData)
{
    for (const TSubclassOf<UGameplayEffect>& EffectClass : ItemData.PassiveEffects)
    {
        if (EffectClass)
        {
            FActiveGameplayEffectHandle Handle = ApplyEffectToSelf(EffectClass, 1.0f);
            if (Handle.IsValid())
            {
                AppliedEffectHandles.Add(Handle);
                EQUIPMENT_LOG(Log, TEXT("Applied passive effect: %s"), *GetNameSafe(EffectClass));
            }
        }
    }
}

void USuspenseCoreEquipmentAttributeComponent::ApplyGrantedAbilities(const FSuspenseCoreUnifiedItemData& ItemData)
{
    for (const FGrantedAbilityData& AbilityData : ItemData.GrantedAbilities)
    {
        if (AbilityData.AbilityClass)
        {
            FGameplayAbilitySpecHandle Handle = GrantAbility(
                AbilityData.AbilityClass,
                AbilityData.AbilityLevel,
                INDEX_NONE
            );

            if (Handle.IsValid())
            {
                GrantedAbilityHandles.Add(Handle);

                // Add activation required tags if specified
                if (!AbilityData.ActivationRequiredTags.IsEmpty())
                {
                    if (FGameplayAbilitySpec* Spec = CachedASC->FindAbilitySpecFromHandle(Handle))
                    {
                        Spec->GetDynamicSpecSourceTags().AppendTags(AbilityData.ActivationRequiredTags);

                        EQUIPMENT_LOG(Verbose, TEXT("Added activation tags to ability %s: %s"),
                            *AbilityData.AbilityClass->GetName(),
                            *AbilityData.ActivationRequiredTags.ToString());
                    }
                }
            }
        }
    }
}

//================================================
// Client Prediction Implementation
//================================================

int32 USuspenseCoreEquipmentAttributeComponent::PredictAttributeChange(const FString& AttributeName, float NewValue)
{
    // Only allow predictions on clients
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        return 0;
    }

    // Get current value
    float CurrentValue = 0.0f;
    if (!GetAttributeValue(AttributeName, CurrentValue))
    {
        EQUIPMENT_LOG(Warning, TEXT("Cannot predict unknown attribute: %s"), *AttributeName);
        return 0;
    }

    // Create prediction
    FSuspenseCoreAttributePredictionData Prediction;
    Prediction.PredictionKey = NextAttributePredictionKey++;
    Prediction.AttributeName = AttributeName;
    Prediction.PredictedValue = NewValue;
    Prediction.OriginalValue = CurrentValue;
    Prediction.PredictionTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    ActiveAttributePredictions.Add(Prediction);

    // Apply prediction locally
    SetAttributeValue(AttributeName, NewValue, false);

    EQUIPMENT_LOG(Verbose, TEXT("Predicted attribute %s: %.2f -> %.2f (Key: %d)"),
        *AttributeName, CurrentValue, NewValue, Prediction.PredictionKey);

    return Prediction.PredictionKey;
}

void USuspenseCoreEquipmentAttributeComponent::ConfirmAttributePrediction(int32 PredictionKey, bool bSuccess, float ActualValue)
{
    int32 PredictionIndex = ActiveAttributePredictions.IndexOfByPredicate(
        [PredictionKey](const FSuspenseCoreAttributePredictionData& Data) { return Data.PredictionKey == PredictionKey; }
    );

    if (PredictionIndex == INDEX_NONE)
    {
        return;
    }

    FSuspenseCoreAttributePredictionData& Prediction = ActiveAttributePredictions[PredictionIndex];

    if (!bSuccess)
    {
        // Revert to actual value
        SetAttributeValue(Prediction.AttributeName, ActualValue, false);
        EQUIPMENT_LOG(Warning, TEXT("Attribute prediction failed for %s - reverting to %.2f"),
            *Prediction.AttributeName, ActualValue);
    }

    ActiveAttributePredictions.RemoveAt(PredictionIndex);
}

//================================================
// Attribute Access Methods
//================================================

bool USuspenseCoreEquipmentAttributeComponent::GetAttributeValue(const FString& AttributeName, float& OutValue) const
{
    FScopeLock Lock(&AttributeCacheCriticalSection);

    // Check predictions first
    for (const FSuspenseCoreAttributePredictionData& Prediction : ActiveAttributePredictions)
    {
        if (Prediction.AttributeName == AttributeName)
        {
            OutValue = Prediction.PredictedValue;
            return true;
        }
    }

    // Check cache
    if (AttributePropertyCache.Contains(AttributeName))
    {
        const TPair<UAttributeSet*, FProperty*>& CachedPair = AttributePropertyCache[AttributeName];
        if (CachedPair.Key && CachedPair.Value)
        {
            OutValue = GetAttributeValueFromProperty(CachedPair.Key, CachedPair.Value);
            return true;
        }
    }

    // Search all attribute sets
    TArray<UAttributeSet*> AllSets = { CurrentAttributeSet, WeaponAttributeSet, ArmorAttributeSet, AmmoAttributeSet };

    for (UAttributeSet* Set : AllSets)
    {
        if (!Set) continue;

        if (FProperty* Property = FindAttributeProperty(Set, AttributeName))
        {
            AttributePropertyCache.Add(AttributeName, TPair<UAttributeSet*, FProperty*>(Set, Property));
            OutValue = GetAttributeValueFromProperty(Set, Property);
            return true;
        }
    }

    return false;
}

void USuspenseCoreEquipmentAttributeComponent::SetAttributeValue(const FString& AttributeName, float NewValue, bool bForceReplication)
{
    FScopeLock Lock(&AttributeCacheCriticalSection);

    // Find attribute
    UAttributeSet* TargetSet = nullptr;
    FProperty* Property = nullptr;

    // Check cache
    if (AttributePropertyCache.Contains(AttributeName))
    {
        const TPair<UAttributeSet*, FProperty*>& CachedPair = AttributePropertyCache[AttributeName];
        TargetSet = CachedPair.Key;
        Property = CachedPair.Value;
    }
    else
    {
        // Search all attribute sets
        TArray<UAttributeSet*> AllSets = { CurrentAttributeSet, WeaponAttributeSet, ArmorAttributeSet, AmmoAttributeSet };

        for (UAttributeSet* Set : AllSets)
        {
            if (!Set) continue;

            if (FProperty* FoundProperty = FindAttributeProperty(Set, AttributeName))
            {
                TargetSet = Set;
                Property = FoundProperty;
                AttributePropertyCache.Add(AttributeName, TPair<UAttributeSet*, FProperty*>(Set, FoundProperty));
                break;
            }
        }
    }

    if (!TargetSet || !Property)
    {
        EQUIPMENT_LOG(Warning, TEXT("Attribute not found: %s"), *AttributeName);
        return;
    }

    // Get old value for broadcast
    float OldValue = GetAttributeValueFromProperty(TargetSet, Property);

    // Set new value through GAS for proper validation
    if (CachedASC)
    {
        // Create a gameplay effect to modify the attribute
        FGameplayAttribute Attribute = GetGameplayAttributeFromProperty(TargetSet, Property);
        if (Attribute.IsValid())
        {
            CachedASC->SetNumericAttributeBase(Attribute, NewValue);
        }
    }
    else
    {
        // Fallback to direct set
        SetAttributeValueToProperty(TargetSet, Property, NewValue);
    }

    // Broadcast change
    BroadcastEquipmentPropertyChanged(FName(*AttributeName), OldValue, NewValue);

    // Force replication if needed
    if (bForceReplication)
    {
        ForceAttributeReplication();
    }
}

TMap<FString, float> USuspenseCoreEquipmentAttributeComponent::GetAllAttributeValues() const
{
    TMap<FString, float> Result;

    TArray<UAttributeSet*> AllSets = { CurrentAttributeSet, WeaponAttributeSet, ArmorAttributeSet, AmmoAttributeSet };

    for (UAttributeSet* Set : AllSets)
    {
        if (!Set) continue;

        for (TFieldIterator<FProperty> It(Set->GetClass()); It; ++It)
        {
            FProperty* Property = *It;
            if (Property && Property->HasAnyPropertyFlags(CPF_BlueprintVisible))
            {
                FString PropertyName = Property->GetName();
                float Value = GetAttributeValueFromProperty(Set, Property);
                Result.Add(PropertyName, Value);
            }
        }
    }

    return Result;
}

bool USuspenseCoreEquipmentAttributeComponent::HasAttribute(const FString& AttributeName) const
{
    float DummyValue;
    return GetAttributeValue(AttributeName, DummyValue);
}

bool USuspenseCoreEquipmentAttributeComponent::GetAttributeByTag(const FGameplayTag& AttributeTag, float& OutValue) const
{
    // Map common tags to attribute names
    static TMap<FGameplayTag, FString> TagToAttributeMap = {
        // Weapon attributes
        { FGameplayTag::RequestGameplayTag(TEXT("Attribute.Weapon.Damage")), TEXT("BaseDamage") },
        { FGameplayTag::RequestGameplayTag(TEXT("Attribute.Weapon.FireRate")), TEXT("RateOfFire") },
        { FGameplayTag::RequestGameplayTag(TEXT("Attribute.Weapon.MagazineSize")), TEXT("MagazineSize") },
        { FGameplayTag::RequestGameplayTag(TEXT("Attribute.Weapon.ReloadTime")), TEXT("TacticalReloadTime") },
        { FGameplayTag::RequestGameplayTag(TEXT("Attribute.Weapon.Spread")), TEXT("HipFireSpread") },
        { FGameplayTag::RequestGameplayTag(TEXT("Attribute.Weapon.Recoil")), TEXT("VerticalRecoil") },

        // Armor attributes
        { FGameplayTag::RequestGameplayTag(TEXT("Attribute.Armor.Class")), TEXT("ArmorClass") },
        { FGameplayTag::RequestGameplayTag(TEXT("Attribute.Armor.Defense")), TEXT("BallisticDefense") },
        { FGameplayTag::RequestGameplayTag(TEXT("Attribute.Armor.Weight")), TEXT("ArmorWeight") },

        // Ammo attributes
        { FGameplayTag::RequestGameplayTag(TEXT("Attribute.Ammo.Damage")), TEXT("BaseDamage") },
        { FGameplayTag::RequestGameplayTag(TEXT("Attribute.Ammo.Penetration")), TEXT("ArmorPenetration") },
        { FGameplayTag::RequestGameplayTag(TEXT("Attribute.Ammo.Velocity")), TEXT("MuzzleVelocity") },

        // Common attributes
        { FGameplayTag::RequestGameplayTag(TEXT("Attribute.Durability")), TEXT("Durability") },
        { FGameplayTag::RequestGameplayTag(TEXT("Attribute.MaxDurability")), TEXT("MaxDurability") },
    };

    if (FString* AttributeName = TagToAttributeMap.Find(AttributeTag))
    {
        return GetAttributeValue(*AttributeName, OutValue);
    }

    return false;
}

void USuspenseCoreEquipmentAttributeComponent::ForceAttributeReplication()
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        CollectReplicatedAttributes();
        AttributeReplicationVersion++;

        if (AActor* Owner = GetOwner())
        {
            Owner->ForceNetUpdate();
        }
    }
}

//================================================
// Replication Implementation
//================================================

void USuspenseCoreEquipmentAttributeComponent::CollectReplicatedAttributes()
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    ReplicatedAttributes.Empty();

    TArray<UAttributeSet*> AllSets = { CurrentAttributeSet, WeaponAttributeSet, ArmorAttributeSet, AmmoAttributeSet };

    for (UAttributeSet* Set : AllSets)
    {
        if (!Set) continue;

        for (TFieldIterator<FProperty> It(Set->GetClass()); It; ++It)
        {
            FProperty* Property = *It;
            if (Property && Property->HasAnyPropertyFlags(CPF_BlueprintVisible))
            {
                FSuspenseCoreReplicatedAttributeData Data;
                Data.AttributeName = Property->GetName();
                Data.CurrentValue = GetAttributeValueFromProperty(Set, Property);
                Data.BaseValue = Data.CurrentValue; // Could be enhanced to track base vs current

                ReplicatedAttributes.Add(Data);
            }
        }
    }
}

void USuspenseCoreEquipmentAttributeComponent::ApplyReplicatedAttributes()
{
    for (const FSuspenseCoreReplicatedAttributeData& Data : ReplicatedAttributes)
    {
        SetAttributeValue(Data.AttributeName, Data.CurrentValue, false);
    }
}

void USuspenseCoreEquipmentAttributeComponent::OnRep_ReplicatedAttributes()
{
    // Apply replicated values to local attribute sets
    ApplyReplicatedAttributes();

    // Notify about updates
    BroadcastEquipmentUpdated();

    EQUIPMENT_LOG(Verbose, TEXT("Applied %d replicated attributes"), ReplicatedAttributes.Num());
}

void USuspenseCoreEquipmentAttributeComponent::OnRep_AttributeSetClasses()
{
    // Recreate attribute sets on clients based on replicated classes
    if (!CachedASC || !GetOwner())
    {
        return;
    }

    // Clean up existing sets
    CleanupAttributeSets();

    // Create new sets from replicated classes
    for (TSubclassOf<UAttributeSet> SetClass : ReplicatedAttributeSetClasses)
    {
        if (!SetClass) continue;

        UAttributeSet* NewSet = NewObject<UAttributeSet>(GetOwner(), SetClass);
        CachedASC->AddAttributeSetSubobject(NewSet);

        // Determine type and store
        FString ClassName = SetClass->GetName();
        if (ClassName.Contains(TEXT("Weapon")))
        {
            WeaponAttributeSet = NewSet;
            CurrentAttributeSet = NewSet;
            AttributeSetsByType.Add(SuspenseCoreEquipmentTags::AttributeSet::TAG_AttributeSet_Weapon, NewSet);
        }
        else if (ClassName.Contains(TEXT("Armor")))
        {
            ArmorAttributeSet = NewSet;
            CurrentAttributeSet = NewSet;
            AttributeSetsByType.Add(SuspenseCoreEquipmentTags::AttributeSet::TAG_AttributeSet_Armor, NewSet);
        }
        else if (ClassName.Contains(TEXT("Ammo")))
        {
            AmmoAttributeSet = NewSet;
            AttributeSetsByType.Add(SuspenseCoreEquipmentTags::AttributeSet::TAG_AttributeSet_Ammo, NewSet);
        }
        else if (!CurrentAttributeSet)
        {
            CurrentAttributeSet = NewSet;
            AttributeSetsByType.Add(SuspenseCoreEquipmentTags::AttributeSet::TAG_AttributeSet_Equipment, NewSet);
        }
    }

    // Apply replicated values
    ApplyReplicatedAttributes();

    EQUIPMENT_LOG(Log, TEXT("Created %d attribute sets from replicated classes"), ReplicatedAttributeSetClasses.Num());
}

//================================================
// Server RPC Implementation
//================================================

void USuspenseCoreEquipmentAttributeComponent::ServerSetAttributeValue_Implementation(const FString& AttributeName, float NewValue)
{
    SetAttributeValue(AttributeName, NewValue, true);
}

bool USuspenseCoreEquipmentAttributeComponent::ServerSetAttributeValue_Validate(const FString& AttributeName, float NewValue)
{
    return !AttributeName.IsEmpty();
}

void USuspenseCoreEquipmentAttributeComponent::ServerApplyItemEffects_Implementation(const FName& ItemID)
{
    FSuspenseCoreUnifiedItemData ItemData;
    // Use DataManager (SSOT) instead of deprecated ItemManager
    USuspenseCoreDataManager* DataManager = USuspenseCoreDataManager::Get(this);
    if (DataManager && DataManager->GetUnifiedItemData(ItemID, ItemData))
    {
        ApplyItemEffects(ItemData);
    }
}

bool USuspenseCoreEquipmentAttributeComponent::ServerApplyItemEffects_Validate(const FName& ItemID)
{
    return !ItemID.IsNone();
}

void USuspenseCoreEquipmentAttributeComponent::ServerRemoveItemEffects_Implementation()
{
    RemoveItemEffects();
}

bool USuspenseCoreEquipmentAttributeComponent::ServerRemoveItemEffects_Validate()
{
    return true;
}

//================================================
// Helper Methods
//================================================

FProperty* USuspenseCoreEquipmentAttributeComponent::FindAttributeProperty(UAttributeSet* AttributeSet, const FString& AttributeName) const
{
    if (!AttributeSet)
    {
        return nullptr;
    }

    for (TFieldIterator<FProperty> It(AttributeSet->GetClass()); It; ++It)
    {
        FProperty* Property = *It;
        if (Property && Property->GetName() == AttributeName)
        {
            return Property;
        }
    }

    return nullptr;
}

float USuspenseCoreEquipmentAttributeComponent::GetAttributeValueFromProperty(UAttributeSet* AttributeSet, FProperty* Property) const
{
    if (!AttributeSet || !Property)
    {
        return 0.0f;
    }

    // Try to get value through reflection
    if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
    {
        return *FloatProp->ContainerPtrToValuePtr<float>(AttributeSet);
    }
    else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
    {
        return static_cast<float>(*DoubleProp->ContainerPtrToValuePtr<double>(AttributeSet));
    }
    else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
    {
        return static_cast<float>(*IntProp->ContainerPtrToValuePtr<int32>(AttributeSet));
    }

    // If property is FGameplayAttributeData, we need to access it differently
    if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        if (StructProp->Struct == FGameplayAttributeData::StaticStruct())
        {
            const FGameplayAttributeData* AttributeData = StructProp->ContainerPtrToValuePtr<FGameplayAttributeData>(AttributeSet);
            if (AttributeData)
            {
                return AttributeData->GetCurrentValue();
            }
        }
    }

    return 0.0f;
}

void USuspenseCoreEquipmentAttributeComponent::SetAttributeValueToProperty(UAttributeSet* AttributeSet, FProperty* Property, float Value)
{
    if (!AttributeSet || !Property)
    {
        return;
    }

    // Try to set value through reflection
    if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
    {
        *FloatProp->ContainerPtrToValuePtr<float>(AttributeSet) = Value;
    }
    else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
    {
        *DoubleProp->ContainerPtrToValuePtr<double>(AttributeSet) = static_cast<double>(Value);
    }
    else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
    {
        *IntProp->ContainerPtrToValuePtr<int32>(AttributeSet) = FMath::RoundToInt(Value);
    }
    else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        if (StructProp->Struct == FGameplayAttributeData::StaticStruct())
        {
            FGameplayAttributeData* AttributeData = StructProp->ContainerPtrToValuePtr<FGameplayAttributeData>(AttributeSet);
            if (AttributeData)
            {
                AttributeData->SetBaseValue(Value);
                AttributeData->SetCurrentValue(Value);
            }
        }
    }
}

FGameplayAttribute USuspenseCoreEquipmentAttributeComponent::GetGameplayAttributeFromProperty(UAttributeSet* AttributeSet, FProperty* Property) const
{
    if (!AttributeSet || !Property)
    {
        return FGameplayAttribute();
    }

    // Create GameplayAttribute from property
    return FGameplayAttribute(Property);
}
