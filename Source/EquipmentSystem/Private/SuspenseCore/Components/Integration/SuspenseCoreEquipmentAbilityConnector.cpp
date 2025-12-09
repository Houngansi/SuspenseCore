// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/Integration/SuspenseCoreEquipmentAbilityConnector.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "SuspenseCore/ItemSystem/SuspenseCoreItemManager.h"
#include "Types/Loadout/SuspenseCoreItemDataTable.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

// Logging category
DEFINE_LOG_CATEGORY_STATIC(LogAbilityConnector, Log, All);

//==================================================================
// Constructor and Lifecycle
//==================================================================

USuspenseCoreEquipmentAbilityConnector::USuspenseCoreEquipmentAbilityConnector()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(false);
}

void USuspenseCoreEquipmentAbilityConnector::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogAbilityConnector, Log, TEXT("[%s] AbilityConnector beginning play on %s"),
        *GetName(), GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));

    const bool bHasAuthority = (GetOwner() && GetOwner()->HasAuthority());
    if (!bHasAuthority)
    {
        bServerOnly = true;
        UE_LOG(LogAbilityConnector, Warning, TEXT("[%s] Running on client - GAS operations will be skipped"),
            *GetName());
    }
}

void USuspenseCoreEquipmentAbilityConnector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UE_LOG(LogAbilityConnector, Log, TEXT("[%s] AbilityConnector ending play - Reason: %s"),
        *GetName(), *UEnum::GetValueAsString(EndPlayReason));

    ClearAll();

    AbilitySystemComponent = nullptr;
    DataProvider.SetInterface(nullptr);
    DataProvider.SetObject(nullptr);

    LogStatistics();

    Super::EndPlay(EndPlayReason);
}

//==================================================================
// Initialization
//==================================================================

bool USuspenseCoreEquipmentAbilityConnector::Initialize(
    UAbilitySystemComponent* InASC,
    TScriptInterface<ISuspenseCoreEquipmentDataProvider> InDataProvider)
{
    if (!EnsureValidExecution(TEXT("Initialize")))
    {
        return false;
    }

    FScopeLock Lock(&ConnectorCriticalSection);

    if (!InASC)
    {
        UE_LOG(LogAbilityConnector, Error, TEXT("[%s] Initialize failed - ASC is null"),
            *GetName());
        FailedGrantOperations++;
        return false;
    }

    if (bIsInitialized && AbilitySystemComponent)
    {
        UE_LOG(LogAbilityConnector, Warning, TEXT("[%s] Already initialized with ASC: %s"),
            *GetName(), *AbilitySystemComponent->GetName());
        return true;
    }

    AbilitySystemComponent = InASC;
    DataProvider = InDataProvider;
    bIsInitialized = true;

    UE_LOG(LogAbilityConnector, Log, TEXT("[%s] Initialized successfully with ASC: %s"),
        *GetName(), *AbilitySystemComponent->GetName());

    return true;
}

//==================================================================
// ISuspenseCoreAbilityConnector Interface - Core Implementation
//==================================================================

TArray<FGameplayAbilitySpecHandle> USuspenseCoreEquipmentAbilityConnector::GrantEquipmentAbilities(
    const FSuspenseCoreInventoryItemInstance& ItemInstance)
{
    return GrantAbilitiesForSlot(INDEX_NONE, ItemInstance);
}

int32 USuspenseCoreEquipmentAbilityConnector::RemoveGrantedAbilities(const TArray<FGameplayAbilitySpecHandle>& Handles)
{
    if (!EnsureValidExecution(TEXT("RemoveGrantedAbilities")))
    {
        return 0;
    }

    FScopeLock Lock(&ConnectorCriticalSection);

    if (!AbilitySystemComponent)
    {
        return 0;
    }

    int32 RemovedCount = 0;

    for (const FGameplayAbilitySpecHandle& Handle : Handles)
    {
        if (!Handle.IsValid())
        {
            continue;
        }

        const FSuspenseCoreGrantedAbilityRecord* Found = GrantedAbilities.FindByPredicate(
            [Handle](const FSuspenseCoreGrantedAbilityRecord& R){ return R.AbilityHandle == Handle; });

        const TCHAR* AbilityClassName = TEXT("UnknownAbility");
        int32 SlotIndex = INDEX_NONE;
        const TCHAR* SourceName = TEXT("Unknown");

        if (Found)
        {
            AbilityClassName = Found->AbilityClass ? *Found->AbilityClass->GetName() : TEXT("UnknownAbility");
            SlotIndex = Found->SlotIndex;
            SourceName = Found->Source.Len() > 0 ? *Found->Source : TEXT("Unknown");
        }

        AbilitySystemComponent->CancelAbilityHandle(Handle);
        AbilitySystemComponent->ClearAbility(Handle);

        const int32 RemovedRecords = GrantedAbilities.RemoveAll([Handle](const FSuspenseCoreGrantedAbilityRecord& R)
        {
            return R.AbilityHandle == Handle;
        });

        if (RemovedRecords > 0)
        {
            RemovedCount++;
            UE_LOG(LogAbilityConnector, Verbose, TEXT("[%s] Removed ability %s (Slot %d, Source %s)"),
                *GetName(), AbilityClassName, SlotIndex, SourceName);
        }
    }

    return RemovedCount;
}


TArray<FActiveGameplayEffectHandle> USuspenseCoreEquipmentAbilityConnector::ApplyEquipmentEffects(
    const FSuspenseCoreInventoryItemInstance& ItemInstance)
{
    return ApplyEffectsForSlot(INDEX_NONE, ItemInstance);
}

int32 USuspenseCoreEquipmentAbilityConnector::RemoveAppliedEffects(const TArray<FActiveGameplayEffectHandle>& Handles)
{
    if (!EnsureValidExecution(TEXT("RemoveAppliedEffects")))
    {
        return 0;
    }

    FScopeLock Lock(&ConnectorCriticalSection);

    if (!AbilitySystemComponent)
    {
        return 0;
    }

    int32 RemovedCount = 0;

    for (const FActiveGameplayEffectHandle& Handle : Handles)
    {
        if (!Handle.IsValid())
        {
            continue;
        }

        const FSuspenseCoreAppliedEffectRecord* Found = AppliedEffects.FindByPredicate(
            [Handle](const FSuspenseCoreAppliedEffectRecord& R){ return R.EffectHandle == Handle; });

        const TCHAR* EffectClassName = TEXT("UnknownEffect");
        int32 SlotIndex = INDEX_NONE;
        const TCHAR* SourceName = TEXT("Unknown");

        if (Found)
        {
            EffectClassName = Found->EffectClass ? *Found->EffectClass->GetName() : TEXT("UnknownEffect");
            SlotIndex = Found->SlotIndex;
            SourceName = Found->Source.Len() > 0 ? *Found->Source : TEXT("Unknown");
        }

        const bool bRemoved = AbilitySystemComponent->RemoveActiveGameplayEffect(Handle);

        if (bRemoved)
        {
            AppliedEffects.RemoveAll([Handle](const FSuspenseCoreAppliedEffectRecord& R)
            {
                return R.EffectHandle == Handle;
            });

            RemovedCount++;
            UE_LOG(LogAbilityConnector, Verbose, TEXT("[%s] Removed effect %s (Slot %d, Source %s)"),
                *GetName(), EffectClassName, SlotIndex, SourceName);
        }
    }

    return RemovedCount;
}


bool USuspenseCoreEquipmentAbilityConnector::UpdateEquipmentAttributes(
    const FSuspenseCoreInventoryItemInstance& ItemInstance)
{
    if (!EnsureValidExecution(TEXT("UpdateEquipmentAttributes")))
    {
        return false;
    }

    FScopeLock Lock(&ConnectorCriticalSection);

    if (!AbilitySystemComponent)
    {
        FailedApplyOperations++;
        return false;
    }

    // Get item data from single source of truth
    USuspenseCoreItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        FailedApplyOperations++;
        return false;
    }

    FSuspenseCoreUnifiedItemData ItemData;
    if (!ItemManager->GetUnifiedItemData(ItemInstance.ItemID, ItemData))
    {
        FailedApplyOperations++;
        return false;
    }

    // Check if this item has an attribute set
    TSubclassOf<UAttributeSet> AttributeClass = ItemData.GetPrimaryAttributeSet();
    if (!AttributeClass)
    {
        UE_LOG(LogAbilityConnector, Verbose, TEXT("[%s] Item %s has no attribute set"),
            *GetName(), *ItemInstance.ItemID.ToString());
        return true; // Not an error - item just doesn't have attributes
    }

    // Check if we already have an attribute set for this item
    UAttributeSet* ExistingSet = nullptr;
    int32 ExistingSlotIndex = INDEX_NONE;

    for (FSuspenseCoreManagedAttributeSet& ManagedSet : ManagedAttributeSets)
    {
        if (ManagedSet.ItemInstanceId == ItemInstance.InstanceID)
        {
            ExistingSet = ManagedSet.AttributeSet;
            ExistingSlotIndex = ManagedSet.SlotIndex;
            break;
        }
    }

    // Determine slot index
    int32 SlotIndex = ItemInstance.AnchorIndex != INDEX_NONE ? ItemInstance.AnchorIndex : INDEX_NONE;

    // Create new attribute set if needed
    if (!ExistingSet)
    {
        ExistingSet = CreateAttributeSetFromItemData(ItemData, ItemInstance, SlotIndex);
        if (!ExistingSet)
        {
            UE_LOG(LogAbilityConnector, Error, TEXT("[%s] Failed to create attribute set for item: %s"),
                *GetName(), *ItemInstance.ItemID.ToString());
            FailedApplyOperations++;
            return false;
        }
    }
    else if (ExistingSlotIndex != SlotIndex)
    {
        // Update slot index if changed
        for (FSuspenseCoreManagedAttributeSet& ManagedSet : ManagedAttributeSets)
        {
            if (ManagedSet.AttributeSet == ExistingSet)
            {
                ManagedSet.SlotIndex = SlotIndex;
                break;
            }
        }
    }

    // Apply initialization effect (ONLY here)
    TSubclassOf<UGameplayEffect> InitEffect = nullptr;
    if (ItemData.bIsWeapon)
    {
        InitEffect = ItemData.WeaponInitialization.WeaponInitEffect;
    }
    else if (ItemData.bIsArmor)
    {
        InitEffect = ItemData.ArmorInitialization.ArmorInitEffect;
    }
    else if (ItemData.bIsEquippable)
    {
        InitEffect = ItemData.EquipmentInitEffect;
    }

    if (InitEffect)
    {
        return InitializeAttributeSet(ExistingSet, InitEffect, ItemInstance);
    }

    return true;
}

UAttributeSet* USuspenseCoreEquipmentAbilityConnector::GetEquipmentAttributeSet(int32 SlotIndex) const
{
    for (const FSuspenseCoreManagedAttributeSet& ManagedSet : ManagedAttributeSets)
    {
        if (ManagedSet.SlotIndex == SlotIndex)
        {
            return ManagedSet.AttributeSet;
        }
    }

    return nullptr;
}

bool USuspenseCoreEquipmentAbilityConnector::ActivateEquipmentAbility(
    const FGameplayAbilitySpecHandle& AbilityHandle)
{
    if (!EnsureValidExecution(TEXT("ActivateEquipmentAbility")))
    {
        return false;
    }

    if (!AbilitySystemComponent || !AbilityHandle.IsValid())
    {
        FailedActivateOperations++;
        return false;
    }

    bool bSuccess = AbilitySystemComponent->TryActivateAbility(AbilityHandle);

    if (bSuccess)
    {
        TotalActivations++;

        const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(AbilityHandle);
        if (AbilitySpec && AbilitySpec->Ability)
        {
            UE_LOG(LogAbilityConnector, Verbose, TEXT("[%s] Activated ability: %s"),
                *GetName(), *AbilitySpec->Ability->GetName());
        }
        else
        {
            UE_LOG(LogAbilityConnector, Verbose, TEXT("[%s] Activated ability handle: %p"),
                *GetName(), &AbilityHandle);
        }
    }
    else
    {
        FailedActivateOperations++;

        const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(AbilityHandle);
        if (AbilitySpec && AbilitySpec->Ability)
        {
            UE_LOG(LogAbilityConnector, Warning, TEXT("[%s] Failed to activate ability: %s"),
                *GetName(), *AbilitySpec->Ability->GetName());
        }
        else
        {
            UE_LOG(LogAbilityConnector, Warning, TEXT("[%s] Failed to activate ability handle: %p"),
                *GetName(), &AbilityHandle);
        }
    }

    return bSuccess;
}

//==================================================================
// Slot-Based Operations
//==================================================================

TArray<FGameplayAbilitySpecHandle> USuspenseCoreEquipmentAbilityConnector::GrantAbilitiesForSlot(
    int32 SlotIndex, const FSuspenseCoreInventoryItemInstance& ItemInstance)
{
    TArray<FGameplayAbilitySpecHandle> GrantedHandles;

    if (!EnsureValidExecution(TEXT("GrantAbilitiesForSlot")))
    {
        return GrantedHandles;
    }

    FScopeLock Lock(&ConnectorCriticalSection);

    if (!bIsInitialized || !AbilitySystemComponent)
    {
        UE_LOG(LogAbilityConnector, Error, TEXT("[%s] GrantAbilitiesForSlot - Not initialized"),
            *GetName());
        FailedGrantOperations++;
        return GrantedHandles;
    }

    if (!ItemInstance.IsValid())
    {
        UE_LOG(LogAbilityConnector, Warning, TEXT("[%s] GrantAbilitiesForSlot - Invalid item instance"),
            *GetName());
        FailedGrantOperations++;
        return GrantedHandles;
    }

    USuspenseCoreItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        UE_LOG(LogAbilityConnector, Error, TEXT("[%s] GrantAbilitiesForSlot - ItemManager not available"),
            *GetName());
        FailedGrantOperations++;
        return GrantedHandles;
    }

    FSuspenseCoreUnifiedItemData ItemData;
    if (!ItemManager->GetUnifiedItemData(ItemInstance.ItemID, ItemData))
    {
        UE_LOG(LogAbilityConnector, Warning, TEXT("[%s] GrantAbilitiesForSlot - No data for item: %s"),
            *GetName(), *ItemInstance.ItemID.ToString());
        FailedGrantOperations++;
        return GrantedHandles;
    }

    GrantedHandles = GrantAbilitiesFromItemData(ItemData, ItemInstance, SlotIndex);

    UE_LOG(LogAbilityConnector, Log, TEXT("[%s] Granted %d abilities for item: %s in slot %d"),
        *GetName(), GrantedHandles.Num(), *ItemInstance.ItemID.ToString(), SlotIndex);

    return GrantedHandles;
}

int32 USuspenseCoreEquipmentAbilityConnector::RemoveAbilitiesForSlot(int32 SlotIndex)
{
    TArray<FGameplayAbilitySpecHandle> HandlesToRemove;

    for (const FSuspenseCoreGrantedAbilityRecord& Record : GrantedAbilities)
    {
        if (Record.SlotIndex == SlotIndex)
        {
            HandlesToRemove.Add(Record.AbilityHandle);
        }
    }

    return RemoveGrantedAbilities(HandlesToRemove);
}

TArray<FActiveGameplayEffectHandle> USuspenseCoreEquipmentAbilityConnector::ApplyEffectsForSlot(
    int32 SlotIndex, const FSuspenseCoreInventoryItemInstance& ItemInstance)
{
    TArray<FActiveGameplayEffectHandle> AppliedHandles;

    if (!EnsureValidExecution(TEXT("ApplyEffectsForSlot")))
    {
        return AppliedHandles;
    }

    FScopeLock Lock(&ConnectorCriticalSection);

    if (!bIsInitialized || !AbilitySystemComponent)
    {
        UE_LOG(LogAbilityConnector, Error, TEXT("[%s] ApplyEffectsForSlot - Not initialized"),
            *GetName());
        FailedApplyOperations++;
        return AppliedHandles;
    }

    USuspenseCoreItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        UE_LOG(LogAbilityConnector, Error, TEXT("[%s] ApplyEffectsForSlot - ItemManager not available"),
            *GetName());
        FailedApplyOperations++;
        return AppliedHandles;
    }

    FSuspenseCoreUnifiedItemData ItemData;
    if (!ItemManager->GetUnifiedItemData(ItemInstance.ItemID, ItemData))
    {
        UE_LOG(LogAbilityConnector, Warning, TEXT("[%s] ApplyEffectsForSlot - No data for item: %s"),
            *GetName(), *ItemInstance.ItemID.ToString());
        FailedApplyOperations++;
        return AppliedHandles;
    }

    // Apply ONLY passive effects (init effects are handled in UpdateEquipmentAttributes)
    AppliedHandles = ApplyEffectsFromItemData(ItemData, ItemInstance, SlotIndex);

    UE_LOG(LogAbilityConnector, Log, TEXT("[%s] Applied %d effects for item: %s in slot %d"),
        *GetName(), AppliedHandles.Num(), *ItemInstance.ItemID.ToString(), SlotIndex);

    return AppliedHandles;
}

int32 USuspenseCoreEquipmentAbilityConnector::RemoveEffectsForSlot(int32 SlotIndex)
{
    TArray<FActiveGameplayEffectHandle> HandlesToRemove;

    for (const FSuspenseCoreAppliedEffectRecord& Record : AppliedEffects)
    {
        if (Record.SlotIndex == SlotIndex)
        {
            HandlesToRemove.Add(Record.EffectHandle);
        }
    }

    return RemoveAppliedEffects(HandlesToRemove);
}

//==================================================================
// Cleanup
//==================================================================

void USuspenseCoreEquipmentAbilityConnector::ClearAll()
{
    if (!EnsureValidExecution(TEXT("ClearAll")))
    {
        return;
    }

    FScopeLock Lock(&ConnectorCriticalSection);

    UE_LOG(LogAbilityConnector, Log, TEXT("[%s] Clearing all abilities and effects"),
        *GetName());

    if (AbilitySystemComponent)
    {
        for (const FSuspenseCoreGrantedAbilityRecord& Record : GrantedAbilities)
        {
            if (Record.AbilityHandle.IsValid())
            {
                AbilitySystemComponent->CancelAbilityHandle(Record.AbilityHandle);
                AbilitySystemComponent->ClearAbility(Record.AbilityHandle);
            }
        }

        for (const FSuspenseCoreAppliedEffectRecord& Record : AppliedEffects)
        {
            if (Record.EffectHandle.IsValid())
            {
                AbilitySystemComponent->RemoveActiveGameplayEffect(Record.EffectHandle);
            }
        }

        // AttributeSets: не снимаем вручную — в UE5 это небезопасно.
    }

    int32 ClearedAbilities = GrantedAbilities.Num();
    int32 ClearedEffects = AppliedEffects.Num();
    int32 ClearedAttributes = ManagedAttributeSets.Num();

    GrantedAbilities.Empty();
    AppliedEffects.Empty();
    ManagedAttributeSets.Empty();

    UE_LOG(LogAbilityConnector, Log, TEXT("[%s] Cleared %d abilities, %d effects, %d attribute sets"),
        *GetName(), ClearedAbilities, ClearedEffects, ClearedAttributes);
}

int32 USuspenseCoreEquipmentAbilityConnector::CleanupInvalidHandles()
{
    if (!EnsureValidExecution(TEXT("CleanupInvalidHandles")))
    {
        return 0;
    }

    FScopeLock Lock(&ConnectorCriticalSection);

    if (!AbilitySystemComponent)
    {
        return 0;
    }

    int32 CleanedCount = 0;

    for (int32 i = GrantedAbilities.Num() - 1; i >= 0; i--)
    {
        if (!AbilitySystemComponent->FindAbilitySpecFromHandle(GrantedAbilities[i].AbilityHandle))
        {
            GrantedAbilities.RemoveAt(i);
            CleanedCount++;
        }
    }

    for (int32 i = AppliedEffects.Num() - 1; i >= 0; i--)
    {
        const FActiveGameplayEffect* ActiveEffect = AbilitySystemComponent->GetActiveGameplayEffect(AppliedEffects[i].EffectHandle);
        if (!ActiveEffect)
        {
            AppliedEffects.RemoveAt(i);
            CleanedCount++;
        }
    }

    if (CleanedCount > 0)
    {
        UE_LOG(LogAbilityConnector, Log, TEXT("[%s] Cleaned %d invalid handles"),
            *GetName(), CleanedCount);
    }

    return CleanedCount;
}

//==================================================================
// Debug and Statistics
//==================================================================

bool USuspenseCoreEquipmentAbilityConnector::ValidateConnector(TArray<FString>& OutErrors) const
{
    OutErrors.Empty();
    bool bIsValid = true;

    if (!bIsInitialized)
    {
        OutErrors.Add(TEXT("Connector not initialized"));
        bIsValid = false;
    }

    if (!AbilitySystemComponent)
    {
        OutErrors.Add(TEXT("No AbilitySystemComponent set"));
        bIsValid = false;
    }

    if (AbilitySystemComponent)
    {
        int32 OrphanedAbilities = 0;
        for (const FSuspenseCoreGrantedAbilityRecord& Record : GrantedAbilities)
        {
            if (!AbilitySystemComponent->FindAbilitySpecFromHandle(Record.AbilityHandle))
            {
                OrphanedAbilities++;
            }
        }

        if (OrphanedAbilities > 0)
        {
            OutErrors.Add(FString::Printf(TEXT("%d orphaned ability records"), OrphanedAbilities));
            bIsValid = false;
        }

        int32 OrphanedEffects = 0;
        for (const FSuspenseCoreAppliedEffectRecord& Record : AppliedEffects)
        {
            const FActiveGameplayEffect* ActiveEffect = AbilitySystemComponent->GetActiveGameplayEffect(Record.EffectHandle);
            if (!ActiveEffect)
            {
                OrphanedEffects++;
            }
        }

        if (OrphanedEffects > 0)
        {
            OutErrors.Add(FString::Printf(TEXT("%d orphaned effect records"), OrphanedEffects));
            bIsValid = false;
        }
    }

    return bIsValid;
}

FString USuspenseCoreEquipmentAbilityConnector::GetDebugInfo() const
{
    FString DebugInfo;

    DebugInfo += TEXT("=== Equipment Ability Connector Debug ===\n");
    DebugInfo += FString::Printf(TEXT("Component: %s\n"), *GetName());
    DebugInfo += FString::Printf(TEXT("Owner: %s\n"),
        GetOwner() ? *GetOwner()->GetName() : TEXT("None"));
    DebugInfo += FString::Printf(TEXT("Initialized: %s\n"),
        bIsInitialized ? TEXT("Yes") : TEXT("No"));
    DebugInfo += FString::Printf(TEXT("ASC: %s\n"),
        AbilitySystemComponent ? *AbilitySystemComponent->GetName() : TEXT("None"));
    DebugInfo += FString::Printf(TEXT("Server Only: %s\n"),
        bServerOnly ? TEXT("Yes") : TEXT("No"));

    DebugInfo += TEXT("\n--- Granted Abilities ---\n");
    DebugInfo += FString::Printf(TEXT("Total: %d\n"), GrantedAbilities.Num());
    for (const FSuspenseCoreGrantedAbilityRecord& Record : GrantedAbilities)
    {
        DebugInfo += FString::Printf(TEXT("  [%s] %s (Level %d, Slot %d, Source: %s)\n"),
            *Record.ItemInstanceId.ToString().Left(8),
            Record.AbilityClass ? *Record.AbilityClass->GetName() : TEXT("Invalid"),
            Record.AbilityLevel,
            Record.SlotIndex,
            *Record.Source);
    }

    DebugInfo += TEXT("\n--- Applied Effects ---\n");
    DebugInfo += FString::Printf(TEXT("Total: %d\n"), AppliedEffects.Num());
    for (const FSuspenseCoreAppliedEffectRecord& Record : AppliedEffects)
    {
        DebugInfo += FString::Printf(TEXT("  [%s] %s (Duration: %.1f, Slot %d, Source: %s)\n"),
            *Record.ItemInstanceId.ToString().Left(8),
            Record.EffectClass ? *Record.EffectClass->GetName() : TEXT("Invalid"),
            Record.Duration,
            Record.SlotIndex,
            *Record.Source);
    }

    DebugInfo += TEXT("\n--- Managed Attributes ---\n");
    DebugInfo += FString::Printf(TEXT("Total: %d\n"), ManagedAttributeSets.Num());
    for (const FSuspenseCoreManagedAttributeSet& ManagedSet : ManagedAttributeSets)
    {
        DebugInfo += FString::Printf(TEXT("  [%s] %s (Slot %d, Type: %s, Init: %s)\n"),
            *ManagedSet.ItemInstanceId.ToString().Left(8),
            ManagedSet.AttributeClass ? *ManagedSet.AttributeClass->GetName() : TEXT("Invalid"),
            ManagedSet.SlotIndex,
            *ManagedSet.AttributeType,
            ManagedSet.bIsInitialized ? TEXT("Yes") : TEXT("No"));
    }

    DebugInfo += TEXT("\n--- Statistics ---\n");
    DebugInfo += FString::Printf(TEXT("Total Abilities Granted: %d\n"), TotalAbilitiesGranted);
    DebugInfo += FString::Printf(TEXT("Total Effects Applied: %d\n"), TotalEffectsApplied);
    DebugInfo += FString::Printf(TEXT("Total Attributes Created: %d\n"), TotalAttributeSetsCreated);
    DebugInfo += FString::Printf(TEXT("Total Activations: %d\n"), TotalActivations);
    DebugInfo += FString::Printf(TEXT("Failed Grant Operations: %d\n"), FailedGrantOperations);
    DebugInfo += FString::Printf(TEXT("Failed Apply Operations: %d\n"), FailedApplyOperations);
    DebugInfo += FString::Printf(TEXT("Failed Activate Operations: %d\n"), FailedActivateOperations);

    if (TotalAbilitiesGranted > 0 || FailedGrantOperations > 0)
    {
        float GrantSuccessRate = (float)TotalAbilitiesGranted / (TotalAbilitiesGranted + FailedGrantOperations) * 100.0f;
        DebugInfo += FString::Printf(TEXT("Grant Success Rate: %.1f%%\n"), GrantSuccessRate);
    }

    if (TotalActivations > 0 || FailedActivateOperations > 0)
    {
        float ActivateSuccessRate = (float)TotalActivations / (TotalActivations + FailedActivateOperations) * 100.0f;
        DebugInfo += FString::Printf(TEXT("Activate Success Rate: %.1f%%\n"), ActivateSuccessRate);
    }

    return DebugInfo;
}

void USuspenseCoreEquipmentAbilityConnector::LogStatistics() const
{
    UE_LOG(LogAbilityConnector, Log, TEXT("=== Ability Connector Statistics for %s ==="), *GetName());
    UE_LOG(LogAbilityConnector, Log, TEXT("  Granted: %d abilities (Failed: %d)"),
        TotalAbilitiesGranted, FailedGrantOperations);
    UE_LOG(LogAbilityConnector, Log, TEXT("  Applied: %d effects (Failed: %d)"),
        TotalEffectsApplied, FailedApplyOperations);
    UE_LOG(LogAbilityConnector, Log, TEXT("  Created: %d attribute sets"),
        TotalAttributeSetsCreated);
    UE_LOG(LogAbilityConnector, Log, TEXT("  Activated: %d abilities (Failed: %d)"),
        TotalActivations, FailedActivateOperations);

    if (TotalAbilitiesGranted > 0 || FailedGrantOperations > 0)
    {
        float SuccessRate = (float)TotalAbilitiesGranted / (TotalAbilitiesGranted + FailedGrantOperations) * 100.0f;
        UE_LOG(LogAbilityConnector, Log, TEXT("  Grant Success Rate: %.1f%%"), SuccessRate);
    }

    if (TotalActivations > 0 || FailedActivateOperations > 0)
    {
        float SuccessRate = (float)TotalActivations / (TotalActivations + FailedActivateOperations) * 100.0f;
        UE_LOG(LogAbilityConnector, Log, TEXT("  Activation Success Rate: %.1f%%"), SuccessRate);
    }
}

//==================================================================
// Protected - Internal GAS Operations
//==================================================================

TArray<FGameplayAbilitySpecHandle> USuspenseCoreEquipmentAbilityConnector::GrantAbilitiesFromItemData(
    const FSuspenseCoreUnifiedItemData& ItemData,
    const FSuspenseCoreInventoryItemInstance& ItemInstance,
    int32 SlotIndex)
{
    TArray<FGameplayAbilitySpecHandle> GrantedHandles;

    // Base abilities
    for (const FGrantedAbilityData& AbilityData : ItemData.GrantedAbilities)
    {
        if (!AbilityData.AbilityClass)
        {
            UE_LOG(LogAbilityConnector, Warning, TEXT("[%s] Null ability class in item data for: %s"),
                *GetName(), *ItemData.ItemID.ToString());
            continue;
        }

        FGameplayAbilitySpecHandle Handle = GrantSingleAbility(
            AbilityData.AbilityClass,
            AbilityData.AbilityLevel,
            AbilityData.InputTag,
            this,
            TEXT("Base")
        );

        if (Handle.IsValid())
        {
            GrantedHandles.Add(Handle);

            FSuspenseCoreGrantedAbilityRecord Record;
            Record.ItemInstanceId = ItemInstance.InstanceID;
            Record.SlotIndex = SlotIndex;
            Record.AbilityHandle = Handle;
            Record.AbilityClass = AbilityData.AbilityClass;
            Record.AbilityLevel = AbilityData.AbilityLevel;
            Record.InputTag = AbilityData.InputTag;
            Record.GrantTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
            Record.Source = TEXT("Base");

            GrantedAbilities.Add(Record);
            TotalAbilitiesGranted++;

            UE_LOG(LogAbilityConnector, Verbose, TEXT("[%s] Granted base ability: %s for slot %d"),
                *GetName(), *AbilityData.AbilityClass->GetName(), SlotIndex);
        }
    }

    // Fire modes
    if (ItemData.bIsWeapon)
    {
        for (const FWeaponFireModeData& FireMode : ItemData.FireModes)
        {
            if (!FireMode.FireModeAbility || !FireMode.bEnabled)
            {
                continue;
            }

            FGameplayAbilitySpecHandle Handle = GrantSingleAbility(
                FireMode.FireModeAbility,
                1,
                FireMode.FireModeTag,
                this,
                FString::Printf(TEXT("FireMode_%s"), *FireMode.DisplayName.ToString())
            );

            if (Handle.IsValid())
            {
                GrantedHandles.Add(Handle);

                FSuspenseCoreGrantedAbilityRecord Record;
                Record.ItemInstanceId = ItemInstance.InstanceID;
                Record.SlotIndex = SlotIndex;
                Record.AbilityHandle = Handle;
                Record.AbilityClass = FireMode.FireModeAbility;
                Record.AbilityLevel = 1;
                Record.InputTag = FireMode.FireModeTag;
                Record.GrantTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
                Record.Source = FString::Printf(TEXT("FireMode_%s"), *FireMode.DisplayName.ToString());

                GrantedAbilities.Add(Record);
                TotalAbilitiesGranted++;

                UE_LOG(LogAbilityConnector, Verbose, TEXT("[%s] Granted fire mode: %s for slot %d"),
                    *GetName(), *FireMode.DisplayName.ToString(), SlotIndex);
            }
        }
    }

    return GrantedHandles;
}

TArray<FActiveGameplayEffectHandle> USuspenseCoreEquipmentAbilityConnector::ApplyEffectsFromItemData(
    const FSuspenseCoreUnifiedItemData& ItemData,
    const FSuspenseCoreInventoryItemInstance& ItemInstance,
    int32 SlotIndex)
{
    TArray<FActiveGameplayEffectHandle> AppliedHandles;

    for (TSubclassOf<UGameplayEffect> EffectClass : ItemData.PassiveEffects)
    {
        if (!EffectClass)
        {
            continue;
        }

        FActiveGameplayEffectHandle Handle = ApplySingleEffect(
            EffectClass,
            1.0f,
            this,
            TEXT("Passive")
        );

        if (Handle.IsValid())
        {
            AppliedHandles.Add(Handle);

            FSuspenseCoreAppliedEffectRecord Record;
            Record.ItemInstanceId = ItemInstance.InstanceID;
            Record.SlotIndex = SlotIndex;
            Record.EffectHandle = Handle;
            Record.EffectClass = EffectClass;
            Record.ApplicationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
            Record.Source = TEXT("Passive");

            const UGameplayEffect* CDO = EffectClass->GetDefaultObject<UGameplayEffect>();
            if (CDO)
            {
                if (CDO->DurationPolicy == EGameplayEffectDurationType::Infinite)
                {
                    Record.Duration = -1.0f;
                }
                else if (CDO->DurationPolicy == EGameplayEffectDurationType::HasDuration)
                {
                    FGameplayEffectContextHandle ContextHandle = AbilitySystemComponent->MakeEffectContext();
                    FGameplayEffectSpec TempSpec(CDO, ContextHandle, 1.0f);
                    float CalculatedDuration = 0.0f;
                    bool bSuccess = CDO->DurationMagnitude.AttemptCalculateMagnitude(
                        TempSpec, CalculatedDuration, true, 0.0f);
                    Record.Duration = bSuccess ? CalculatedDuration : 0.0f;
                    if (!bSuccess)
                    {
                        UE_LOG(LogAbilityConnector, Warning,
                            TEXT("[%s] Failed to calculate duration for effect %s - using 0"),
                            *GetName(), *EffectClass->GetName());
                    }
                }
                else
                {
                    Record.Duration = 0.0f;
                }
            }

            AppliedEffects.Add(Record);
            TotalEffectsApplied++;

            UE_LOG(LogAbilityConnector, Verbose, TEXT("[%s] Applied passive effect: %s for slot %d (Duration: %.2f)"),
                *GetName(), *EffectClass->GetName(), SlotIndex, Record.Duration);
        }
    }

    return AppliedHandles;
}

UAttributeSet* USuspenseCoreEquipmentAbilityConnector::CreateAttributeSetFromItemData(
    const FSuspenseCoreUnifiedItemData& ItemData,
    const FSuspenseCoreInventoryItemInstance& ItemInstance,
    int32 SlotIndex)
{
    if (!AbilitySystemComponent)
    {
        return nullptr;
    }

    TSubclassOf<UAttributeSet> AttributeClass = nullptr;
    FString AttributeType;

    if (ItemData.bIsWeapon && ItemData.WeaponInitialization.WeaponAttributeSetClass)
    {
        AttributeClass = ItemData.WeaponInitialization.WeaponAttributeSetClass;
        AttributeType = TEXT("Weapon");
    }
    else if (ItemData.bIsArmor && ItemData.ArmorInitialization.ArmorAttributeSetClass)
    {
        AttributeClass = ItemData.ArmorInitialization.ArmorAttributeSetClass;
        AttributeType = TEXT("Armor");
    }
    else if (ItemData.bIsEquippable && ItemData.EquipmentAttributeSet)
    {
        AttributeClass = ItemData.EquipmentAttributeSet;
        AttributeType = TEXT("Equipment");
    }

    if (!AttributeClass)
    {
        return nullptr;
    }

    // Reuse existing set for same instance + class
    if (ManagedAttributeSets.Num() > 0)
    {
        const FGuid& InstanceId = ItemInstance.InstanceID;
        const FSuspenseCoreManagedAttributeSet* Existing = ManagedAttributeSets.FindByPredicate(
            [&](const FSuspenseCoreManagedAttributeSet& M)
            {
                return M.ItemInstanceId == InstanceId && M.AttributeClass == AttributeClass && IsValid(M.AttributeSet);
            });

        if (Existing && Existing->AttributeSet)
        {
            UE_LOG(LogAbilityConnector, Verbose, TEXT("[%s] Reusing %s AttributeSet for item %s (Slot %d)"),
                *GetName(), *AttributeType, *ItemData.ItemID.ToString(), SlotIndex);
            return Existing->AttributeSet;
        }
    }

    UAttributeSet* NewAttributeSet = NewObject<UAttributeSet>(
        AbilitySystemComponent,
        AttributeClass,
        NAME_None,
        RF_Transient
    );

    if (!NewAttributeSet)
    {
        UE_LOG(LogAbilityConnector, Error, TEXT("[%s] Failed to create attribute set of class: %s"),
            *GetName(), *AttributeClass->GetName());
        return nullptr;
    }

    AbilitySystemComponent->AddAttributeSetSubobject(NewAttributeSet);

    FSuspenseCoreManagedAttributeSet ManagedSet;
    ManagedSet.SlotIndex = SlotIndex;
    ManagedSet.AttributeSet = NewAttributeSet;
    ManagedSet.AttributeClass = AttributeClass;
    ManagedSet.ItemInstanceId = ItemInstance.InstanceID;
    ManagedSet.bIsInitialized = false;
    ManagedSet.AttributeType = AttributeType;

    ManagedAttributeSets.Add(ManagedSet);
    TotalAttributeSetsCreated++;

    UE_LOG(LogAbilityConnector, Log, TEXT("[%s] Created %s AttributeSet for item %s (Slot %d)"),
        *GetName(), *AttributeType, *ItemData.ItemID.ToString(), SlotIndex);

    return NewAttributeSet;
}


FGameplayAbilitySpecHandle USuspenseCoreEquipmentAbilityConnector::GrantSingleAbility(
    TSubclassOf<UGameplayAbility> AbilityClass,
    int32 Level,
    const FGameplayTag& InputTag,
    UObject* SourceObject,
    const FString& Source)
{
    if (!AbilitySystemComponent || !AbilityClass)
    {
        return FGameplayAbilitySpecHandle();
    }

    FGameplayAbilitySpec AbilitySpec(
        AbilityClass,
        Level,
        INDEX_NONE, // Input ID handled elsewhere
        SourceObject
    );

    if (InputTag.IsValid())
    {
        AbilitySpec.DynamicAbilityTags.AddTag(InputTag);
    }

    FGameplayAbilitySpecHandle Handle = AbilitySystemComponent->GiveAbility(AbilitySpec);

    if (Handle.IsValid())
    {
        UE_LOG(LogAbilityConnector, VeryVerbose, TEXT("[%s] Granted ability %s from source: %s"),
            *GetName(), *AbilityClass->GetName(), *Source);
    }

    return Handle;
}

FActiveGameplayEffectHandle USuspenseCoreEquipmentAbilityConnector::ApplySingleEffect(
    TSubclassOf<UGameplayEffect> EffectClass,
    float Level,
    UObject* SourceObject,
    const FString& Source)
{
    if (!AbilitySystemComponent || !EffectClass)
    {
        return FActiveGameplayEffectHandle();
    }

    FGameplayEffectContextHandle ContextHandle = AbilitySystemComponent->MakeEffectContext();
    ContextHandle.AddSourceObject(SourceObject);

    FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
        EffectClass,
        Level,
        ContextHandle
    );

    if (!SpecHandle.IsValid())
    {
        return FActiveGameplayEffectHandle();
    }

    FActiveGameplayEffectHandle Handle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
        *SpecHandle.Data.Get()
    );

    if (Handle.IsValid())
    {
        UE_LOG(LogAbilityConnector, VeryVerbose, TEXT("[%s] Applied effect %s from source: %s"),
            *GetName(), *EffectClass->GetName(), *Source);
    }

    return Handle;
}

bool USuspenseCoreEquipmentAbilityConnector::InitializeAttributeSet(
    UAttributeSet* AttributeSet,
    TSubclassOf<UGameplayEffect> InitEffect,
    const FSuspenseCoreInventoryItemInstance& ItemInstance)
{
    if (!AttributeSet || !AbilitySystemComponent || !InitEffect)
    {
        return false;
    }

    FGameplayEffectContextHandle ContextHandle = AbilitySystemComponent->MakeEffectContext();
    ContextHandle.AddSourceObject(this);

    FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
        InitEffect,
        1.0f,
        ContextHandle
    );

    if (!SpecHandle.IsValid())
    {
        return false;
    }

    // Runtime properties -> SetByCaller.<Tag>
    for (const auto& PropertyPair : ItemInstance.RuntimeProperties)
    {
        const FName TagName(*(TEXT("SetByCaller.") + PropertyPair.Key.ToString()));
        const FGameplayTag PropertyTag = FGameplayTag::RequestGameplayTag(TagName, false);

        if (PropertyTag.IsValid())
        {
            SpecHandle.Data->SetSetByCallerMagnitude(PropertyTag, PropertyPair.Value);
            UE_LOG(LogAbilityConnector, VeryVerbose, TEXT("[%s] Set runtime property %s = %.2f"),
                *GetName(), *PropertyTag.ToString(), PropertyPair.Value);
        }
        else
        {
            UE_LOG(LogAbilityConnector, Warning, TEXT("[%s] Runtime property tag not found: %s"),
                *GetName(), *TagName.ToString());
        }
    }

    FActiveGameplayEffectHandle Handle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
        *SpecHandle.Data.Get()
    );

    if (Handle.IsValid())
    {
        for (FSuspenseCoreManagedAttributeSet& ManagedSet : ManagedAttributeSets)
        {
            if (ManagedSet.AttributeSet == AttributeSet)
            {
                ManagedSet.bIsInitialized = true;
                break;
            }
        }

        UE_LOG(LogAbilityConnector, Log, TEXT("[%s] Initialized attribute set with effect: %s"),
            *GetName(), *InitEffect->GetName());

        return true;
    }

    return false;
}

//==================================================================
// Manager Access
//==================================================================

USuspenseCoreItemManager* USuspenseCoreEquipmentAbilityConnector::GetItemManager() const
{
    if (CachedItemManager.IsValid())
    {
        float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        if (CurrentTime - LastCacheTime < CacheLifetime)
        {
            return CachedItemManager.Get();
        }
    }

    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GameInstance = World->GetGameInstance())
        {
            CachedItemManager = GameInstance->GetSubsystem<USuspenseCoreItemManager>();
            LastCacheTime = World->GetTimeSeconds();

            if (!CachedItemManager.IsValid())
            {
                UE_LOG(LogAbilityConnector, Error, TEXT("[%s] ItemManager subsystem not found!"),
                    *GetName());
            }
        }
    }

    return CachedItemManager.Get();
}

bool USuspenseCoreEquipmentAbilityConnector::EnsureValidExecution(const FString& FunctionName) const
{
    if (!ensureAlways(IsInGameThread()))
    {
        UE_LOG(LogAbilityConnector, Error, TEXT("[%s] %s must be called on GameThread"),
            *GetName(), *FunctionName);
        return false;
    }

    AActor* OwnerActor = GetOwner();
    const bool bHasAuthority = (OwnerActor && OwnerActor->HasAuthority());

    if (bServerOnly && !bHasAuthority)
    {
        UE_LOG(LogAbilityConnector, VeryVerbose, TEXT("[%s] %s skipped on client"),
            *GetName(), *FunctionName);
        return false;
    }

    return true;
}
