// SuspenseInventory/Validation/SuspenseInventoryConstraints.cpp
// Copyright SuspenseCore Team. All Rights Reserved.

#include "Validation/SuspenseInventoryConstraints.h"
#include "Base/SuspenseInventoryLogs.h"
#include "Interfaces/Inventory/ISuspenseInventoryItem.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

// Define log category for consistent logging throughout validation module
DEFINE_LOG_CATEGORY_STATIC(LogSuspenseConstraints, Log, All);

//==================================================================
// Lifecycle and Initialization Implementation
//==================================================================

USuspenseInventoryConstraints::USuspenseInventoryConstraints()
{
    // Initialize with safe default values
    MaxWeight = 100.0f;
    GridWidth = 0;
    GridHeight = 0;
    bInitialized = false;
    ItemManagerRef = nullptr;

    UE_LOG(LogSuspenseConstraints, VeryVerbose, TEXT("USuspenseInventoryConstraints: Default constructor called"));
}

void USuspenseInventoryConstraints::Initialize(float InMaxWeight, const FGameplayTagContainer& InAllowedTypes, int32 InGridWidth, int32 InGridHeight, USuspenseItemManager* InItemManager)
{
    // Sanitize input parameters
    MaxWeight = FMath::Max(0.0f, InMaxWeight);
    AllowedItemTypes = InAllowedTypes;
    GridWidth = FMath::Max(0, InGridWidth);
    GridHeight = FMath::Max(0, InGridHeight);

    // Set weak reference to ItemManager for DataTable access
    ItemManagerRef = InItemManager;

    bInitialized = true;

    UE_LOG(LogSuspenseConstraints, Log, TEXT("InventoryConstraints initialized: MaxWeight=%.1f, Grid=%dx%d, AllowedTypes=%s, ItemManager=%s"),
        MaxWeight, GridWidth, GridHeight, *AllowedItemTypes.ToStringSimple(),
        InItemManager ? TEXT("Available") : TEXT("None"));
}

bool USuspenseInventoryConstraints::InitializeFromLoadout(const FName& LoadoutID, const FName& InventoryName, const UObject* WorldContext)
{
    if (!WorldContext)
    {
        UE_LOG(LogSuspenseConstraints, Error, TEXT("InitializeFromLoadout: WorldContext is null"));
        return false;
    }

    if (LoadoutID.IsNone())
    {
        UE_LOG(LogSuspenseConstraints, Error, TEXT("InitializeFromLoadout: LoadoutID is None"));
        return false;
    }

    // Get ItemManager from world context
    UWorld* World = WorldContext->GetWorld();
    if (!World || !World->GetGameInstance())
    {
        UE_LOG(LogSuspenseConstraints, Error, TEXT("InitializeFromLoadout: Failed to get world or game instance"));
        return false;
    }

    USuspenseItemManager* ItemManager = World->GetGameInstance()->GetSubsystem<USuspenseItemManager>();
    if (!ItemManager)
    {
        UE_LOG(LogSuspenseConstraints, Error, TEXT("InitializeFromLoadout: ItemManager subsystem not found"));
        return false;
    }

    // TODO: Load real parameters from LoadoutSettings
    float LoadoutMaxWeight = 50.0f;
    FGameplayTagContainer LoadoutAllowedTypes;
    int32 LoadoutGridWidth = 10;
    int32 LoadoutGridHeight = 6;

    Initialize(LoadoutMaxWeight, LoadoutAllowedTypes, LoadoutGridWidth, LoadoutGridHeight, ItemManager);

    UE_LOG(LogSuspenseConstraints, Log, TEXT("InitializeFromLoadout: Successfully initialized from LoadoutID='%s'"), *LoadoutID.ToString());

    return true;
}

//==================================================================
// Enhanced Unified Data Validation Implementation
//==================================================================

FInventoryOperationResult USuspenseInventoryConstraints::ValidateUnifiedItemData(const FSuspenseUnifiedItemData& ItemData, int32 Amount, const FName& FunctionName) const
{
    FInventoryOperationResult BasicResult = ValidateUnifiedDataBasics(ItemData, Amount, FunctionName);
    if (!BasicResult.IsSuccess())
    {
        LogValidationResult(BasicResult, FString::Printf(TEXT("Basic unified data validation for item '%s'"), *ItemData.ItemID.ToString()));
        return BasicResult;
    }

    UE_LOG(LogSuspenseConstraints, VeryVerbose, TEXT("ValidateUnifiedItemData: Item '%s' (x%d) passed basic validation"),
        *ItemData.ItemID.ToString(), Amount);

    return FInventoryOperationResult::Success(FunctionName);
}

FInventoryOperationResult USuspenseInventoryConstraints::ValidateUnifiedItemDataWithRestrictions(const FSuspenseUnifiedItemData& ItemData, int32 Amount, const FName& FunctionName) const
{
    FInventoryOperationResult BasicResult = ValidateUnifiedItemData(ItemData, Amount, FunctionName);
    if (!BasicResult.IsSuccess())
    {
        return BasicResult;
    }

    // Check type restrictions
    if (AllowedItemTypes.Num() > 0 && ItemData.ItemType.IsValid())
    {
        if (!IsItemTypeAllowed(ItemData.ItemType))
        {
            FInventoryOperationResult TypeResult = FInventoryOperationResult::Failure(
                ESuspenseInventoryErrorCode::InvalidItem,
                FText::Format(
                    NSLOCTEXT("SuspenseInventory", "ItemTypeNotAllowed", "Item type '{0}' is not allowed in this inventory"),
                    FText::FromString(ItemData.ItemType.ToString())
                ),
                FunctionName
            );

            LogValidationResult(TypeResult, FString::Printf(TEXT("Type restriction check for '%s'"), *ItemData.ItemID.ToString()));
            return TypeResult;
        }
    }

    return FInventoryOperationResult::Success(FunctionName);
}

//==================================================================
// Runtime Instance Validation Implementation
//==================================================================

FInventoryOperationResult USuspenseInventoryConstraints::ValidateItemInstance(const FSuspenseInventoryItemInstance& ItemInstance, const FName& FunctionName) const
{
    if (!ItemInstance.IsValid())
    {
        return FInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InvalidItem,
            NSLOCTEXT("SuspenseInventory", "InvalidItemInstance", "Item instance is not valid"),
            FunctionName
        );
    }

    FSuspenseUnifiedItemData UnifiedData;
    if (!GetUnifiedDataForInstance(ItemInstance, UnifiedData))
    {
        return FInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::ItemNotFound,
            FText::Format(
                NSLOCTEXT("SuspenseInventory", "ItemNotFoundInDataTable", "Item '{0}' not found in DataTable"),
                FText::FromName(ItemInstance.ItemID)
            ),
            FunctionName
        );
    }

    FInventoryOperationResult UnifiedResult = ValidateUnifiedItemDataWithRestrictions(UnifiedData, ItemInstance.Quantity, FunctionName);
    if (!UnifiedResult.IsSuccess())
    {
        return UnifiedResult;
    }

    FInventoryOperationResult RuntimeResult = ValidateRuntimeProperties(ItemInstance, FunctionName);
    if (!RuntimeResult.IsSuccess())
    {
        return RuntimeResult;
    }

    UE_LOG(LogSuspenseConstraints, VeryVerbose, TEXT("ValidateItemInstance: Instance '%s' [%s] passed validation"),
        *ItemInstance.ItemID.ToString(), *ItemInstance.InstanceID.ToString().Left(8));

    return FInventoryOperationResult::Success(FunctionName);
}

int32 USuspenseInventoryConstraints::ValidateItemInstances(const TArray<FSuspenseInventoryItemInstance>& ItemInstances, const FName& FunctionName, TArray<FSuspenseInventoryItemInstance>& OutFailedInstances) const
{
    OutFailedInstances.Empty();
    OutFailedInstances.Reserve(ItemInstances.Num() / 4);

    int32 SuccessfulCount = 0;

    for (const FSuspenseInventoryItemInstance& Instance : ItemInstances)
    {
        FInventoryOperationResult ValidationResult = ValidateItemInstance(Instance, FunctionName);
        if (ValidationResult.IsSuccess())
        {
            SuccessfulCount++;
        }
        else
        {
            OutFailedInstances.Add(Instance);
            UE_LOG(LogSuspenseConstraints, Warning, TEXT("ValidateItemInstances: Instance '%s' [%s] failed validation: %s"),
                *Instance.ItemID.ToString(),
                *Instance.InstanceID.ToString().Left(8),
                *ValidationResult.ErrorMessage.ToString());
        }
    }

    UE_LOG(LogSuspenseConstraints, Log, TEXT("ValidateItemInstances: %d/%d instances passed batch validation (%d failed)"),
        SuccessfulCount, ItemInstances.Num(), OutFailedInstances.Num());

    return SuccessfulCount;
}

FInventoryOperationResult USuspenseInventoryConstraints::ValidateRuntimeProperties(const FSuspenseInventoryItemInstance& ItemInstance, const FName& FunctionName) const
{
    for (const auto& PropertyPair : ItemInstance.RuntimeProperties)
    {
        if (PropertyPair.Key.IsNone())
        {
            return FInventoryOperationResult::Failure(
                ESuspenseInventoryErrorCode::InvalidItem,
                NSLOCTEXT("SuspenseInventory", "EmptyPropertyName", "Found runtime property with empty name"),
                FunctionName
            );
        }

        if (!FMath::IsFinite(PropertyPair.Value))
        {
            return FInventoryOperationResult::Failure(
                ESuspenseInventoryErrorCode::InvalidItem,
                FText::Format(
                    NSLOCTEXT("SuspenseInventory", "InvalidPropertyValue", "Invalid value for runtime property '{0}': {1}"),
                    FText::FromName(PropertyPair.Key),
                    FText::AsNumber(PropertyPair.Value)
                ),
                FunctionName
            );
        }

        FString PropertyName = PropertyPair.Key.ToString();
        if (PropertyName == TEXT("Durability") || PropertyName == TEXT("MaxDurability"))
        {
            if (PropertyPair.Value < 0.0f)
            {
                return FInventoryOperationResult::Failure(
                    ESuspenseInventoryErrorCode::InvalidItem,
                    FText::Format(
                        NSLOCTEXT("SuspenseInventory", "NegativeDurability", "Durability cannot be negative: {0}"),
                        FText::AsNumber(PropertyPair.Value)
                    ),
                    FunctionName
                );
            }
        }
    }

    if (ItemInstance.HasRuntimeProperty(TEXT("Durability")) && ItemInstance.HasRuntimeProperty(TEXT("MaxDurability")))
    {
        float CurrentDurability = ItemInstance.GetRuntimeProperty(TEXT("Durability"), 0.0f);
        float MaxDurability = ItemInstance.GetRuntimeProperty(TEXT("MaxDurability"), 100.0f);

        if (CurrentDurability > MaxDurability)
        {
            return FInventoryOperationResult::Failure(
                ESuspenseInventoryErrorCode::InvalidItem,
                FText::Format(
                    NSLOCTEXT("SuspenseInventory", "DurabilityExceedsMax", "Current durability ({0}) exceeds maximum ({1})"),
                    FText::AsNumber(CurrentDurability),
                    FText::AsNumber(MaxDurability)
                ),
                FunctionName
            );
        }
    }

    return FInventoryOperationResult::Success(FunctionName);
}

//==================================================================
// Grid and Spatial Validation Implementation
//==================================================================

FInventoryOperationResult USuspenseInventoryConstraints::ValidateSlotIndex(int32 SlotIndex, const FName& FunctionName) const
{
    if (!bInitialized)
    {
        return FInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::NotInitialized,
            NSLOCTEXT("SuspenseInventory", "ConstraintsNotInitialized", "Inventory constraints not initialized"),
            FunctionName
        );
    }

    int32 TotalSlots = GridWidth * GridHeight;
    if (SlotIndex < 0 || SlotIndex >= TotalSlots)
    {
        return FInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InvalidSlot,
            FText::Format(
                NSLOCTEXT("SuspenseInventory", "InvalidSlotIndex", "Invalid slot index: {0}. Valid range: 0-{1} (Grid: {2}x{3})"),
                FText::AsNumber(SlotIndex),
                FText::AsNumber(TotalSlots - 1),
                FText::AsNumber(GridWidth),
                FText::AsNumber(GridHeight)
            ),
            FunctionName
        );
    }

    return FInventoryOperationResult::Success(FunctionName);
}

FInventoryOperationResult USuspenseInventoryConstraints::ValidateGridBoundsForUnified(const FSuspenseUnifiedItemData& ItemData, int32 AnchorIndex, bool bIsRotated, const FName& FunctionName) const
{
    if (!bInitialized)
    {
        return FInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::NotInitialized,
            NSLOCTEXT("SuspenseInventory", "ConstraintsNotInitialized", "Inventory constraints not initialized"),
            FunctionName
        );
    }

    FInventoryOperationResult IndexResult = ValidateSlotIndex(AnchorIndex, FunctionName);
    if (!IndexResult.IsSuccess())
    {
        return IndexResult;
    }

    FVector2D EffectiveSize = CalculateEffectiveItemSize(ItemData.GridSize, bIsRotated);

    if (EffectiveSize.X <= 0 || EffectiveSize.Y <= 0)
    {
        return FInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InvalidItem,
            FText::Format(
                NSLOCTEXT("SuspenseInventory", "InvalidItemSize", "Invalid item size for '{0}': {1}x{2}"),
                FText::FromName(ItemData.ItemID),
                FText::AsNumber(ItemData.GridSize.X),
                FText::AsNumber(ItemData.GridSize.Y)
            ),
            FunctionName
        );
    }

    int32 AnchorX = AnchorIndex % GridWidth;
    int32 AnchorY = AnchorIndex / GridWidth;

    if (AnchorX + FMath::CeilToInt(EffectiveSize.X) > GridWidth)
    {
        return FInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InvalidSlot,
            FText::Format(
                NSLOCTEXT("SuspenseInventory", "OutOfBoundsX", "Item '{0}' extends beyond horizontal boundary"),
                FText::FromName(ItemData.ItemID)
            ),
            FunctionName
        );
    }

    if (AnchorY + FMath::CeilToInt(EffectiveSize.Y) > GridHeight)
    {
        return FInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InvalidSlot,
            FText::Format(
                NSLOCTEXT("SuspenseInventory", "OutOfBoundsY", "Item '{0}' extends beyond vertical boundary"),
                FText::FromName(ItemData.ItemID)
            ),
            FunctionName
        );
    }

    return FInventoryOperationResult::Success(FunctionName);
}

FInventoryOperationResult USuspenseInventoryConstraints::ValidateGridBoundsForInstance(const FSuspenseInventoryItemInstance& ItemInstance, int32 AnchorIndex, const FName& FunctionName) const
{
    FSuspenseUnifiedItemData UnifiedData;
    if (!GetUnifiedDataForInstance(ItemInstance, UnifiedData))
    {
        return FInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::ItemNotFound,
            FText::Format(
                NSLOCTEXT("SuspenseInventory", "ItemNotFoundForBounds", "Cannot validate bounds: item '{0}' not found in DataTable"),
                FText::FromName(ItemInstance.ItemID)
            ),
            FunctionName
        );
    }

    return ValidateGridBoundsForUnified(UnifiedData, AnchorIndex, ItemInstance.bIsRotated, FunctionName);
}

FInventoryOperationResult USuspenseInventoryConstraints::ValidateItemPlacement(const FSuspenseUnifiedItemData& ItemData, int32 AnchorIndex, bool bIsRotated, const TArray<bool>& OccupiedSlots, const FName& FunctionName) const
{
    FInventoryOperationResult BoundsResult = ValidateGridBoundsForUnified(ItemData, AnchorIndex, bIsRotated, FunctionName);
    if (!BoundsResult.IsSuccess())
    {
        return BoundsResult;
    }

    FVector2D EffectiveSize = CalculateEffectiveItemSize(ItemData.GridSize, bIsRotated);
    TArray<int32> RequiredSlots = GetOccupiedSlots(AnchorIndex, EffectiveSize);

    int32 TotalSlots = GridWidth * GridHeight;
    if (OccupiedSlots.Num() != TotalSlots)
    {
        return FInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InvalidSlot,
            FText::Format(
                NSLOCTEXT("SuspenseInventory", "InvalidOccupiedSlotsSize", "OccupiedSlots array size mismatch: expected {0}, got {1}"),
                FText::AsNumber(TotalSlots),
                FText::AsNumber(OccupiedSlots.Num())
            ),
            FunctionName
        );
    }

    for (int32 SlotIndex : RequiredSlots)
    {
        if (SlotIndex >= 0 && SlotIndex < OccupiedSlots.Num() && OccupiedSlots[SlotIndex])
        {
            int32 SlotX = SlotIndex % GridWidth;
            int32 SlotY = SlotIndex / GridWidth;

            return FInventoryOperationResult::Failure(
                ESuspenseInventoryErrorCode::SlotOccupied,
                FText::Format(
                    NSLOCTEXT("SuspenseInventory", "SlotOccupied", "Cannot place item '{0}': slot ({1},{2}) is already occupied"),
                    FText::FromName(ItemData.ItemID),
                    FText::AsNumber(SlotX),
                    FText::AsNumber(SlotY)
                ),
                FunctionName
            );
        }
    }

    return FInventoryOperationResult::Success(FunctionName);
}

//==================================================================
// Weight Validation Implementation
//==================================================================

FInventoryOperationResult USuspenseInventoryConstraints::ValidateWeightForUnified(const FSuspenseUnifiedItemData& ItemData, int32 Amount, float CurrentWeight, const FName& FunctionName) const
{
    if (MaxWeight <= 0.0f)
    {
        return FInventoryOperationResult::Success(FunctionName);
    }

    float AddedWeight = ItemData.Weight * Amount;
    float TotalWeight = CurrentWeight + AddedWeight;

    if (TotalWeight > MaxWeight)
    {
        return FInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::WeightLimit,
            FText::Format(
                NSLOCTEXT("SuspenseInventory", "WeightLimitExceeded", "Adding '{0}' (x{1}) would exceed weight limit. Current: {2}kg, Adding: {3}kg, Maximum: {4}kg"),
                FText::FromString(ItemData.DisplayName.ToString()),
                FText::AsNumber(Amount),
                FText::AsNumber(CurrentWeight),
                FText::AsNumber(AddedWeight),
                FText::AsNumber(MaxWeight)
            ),
            FunctionName
        );
    }

    return FInventoryOperationResult::Success(FunctionName);
}

FInventoryOperationResult USuspenseInventoryConstraints::ValidateWeightForInstance(const FSuspenseInventoryItemInstance& ItemInstance, float CurrentWeight, const FName& FunctionName) const
{
    FSuspenseUnifiedItemData UnifiedData;
    if (!GetUnifiedDataForInstance(ItemInstance, UnifiedData))
    {
        return FInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::ItemNotFound,
            FText::Format(
                NSLOCTEXT("SuspenseInventory", "ItemNotFoundForWeight", "Cannot validate weight: item '{0}' not found in DataTable"),
                FText::FromName(ItemInstance.ItemID)
            ),
            FunctionName
        );
    }

    return ValidateWeightForUnified(UnifiedData, ItemInstance.Quantity, CurrentWeight, FunctionName);
}

bool USuspenseInventoryConstraints::WouldExceedWeightLimitUnified(const FSuspenseUnifiedItemData& ItemData, int32 Amount, float CurrentWeight) const
{
    if (MaxWeight <= 0.0f)
    {
        return false;
    }

    float AddedWeight = ItemData.Weight * Amount;
    return (CurrentWeight + AddedWeight) > MaxWeight;
}

//==================================================================
// Object Validation Implementation
//==================================================================

FInventoryOperationResult USuspenseInventoryConstraints::ValidateItemForOperation(UObject* ItemObject, const FName& FunctionName) const
{
    if (!ItemObject)
    {
        return FInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InvalidItem,
            NSLOCTEXT("SuspenseInventory", "NullItemObject", "Item object is null"),
            FunctionName
        );
    }

    ISuspenseInventoryItemInterface* ItemInterface = Cast<ISuspenseInventoryItemInterface>(ItemObject);
    if (!ItemInterface)
    {
        return FInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InvalidItem,
            FText::Format(
                NSLOCTEXT("SuspenseInventory", "NoInventoryInterface", "Object '{0}' does not implement ISuspenseInventoryItemInterface"),
                FText::FromString(ItemObject->GetClass()->GetName())
            ),
            FunctionName
        );
    }

    if (!ItemInterface->IsInitialized())
    {
        return FInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::NotInitialized,
            NSLOCTEXT("SuspenseInventory", "ItemNotInitialized", "Item object is not properly initialized"),
            FunctionName
        );
    }

    return FInventoryOperationResult::Success(FunctionName);
}

FInventoryOperationResult USuspenseInventoryConstraints::ValidateItemCompatibility(const FSuspenseUnifiedItemData& ItemData, int32 Amount, float CurrentWeight, const FName& FunctionName) const
{
    FInventoryOperationResult UnifiedResult = ValidateUnifiedItemDataWithRestrictions(ItemData, Amount, FunctionName);
    if (!UnifiedResult.IsSuccess())
    {
        return UnifiedResult;
    }

    FInventoryOperationResult WeightResult = ValidateWeightForUnified(ItemData, Amount, CurrentWeight, FunctionName);
    if (!WeightResult.IsSuccess())
    {
        return WeightResult;
    }

    if (ItemData.bIsWeapon && !ItemData.WeaponArchetype.IsValid())
    {
        return FInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InvalidItem,
            FText::Format(
                NSLOCTEXT("SuspenseInventory", "InvalidWeaponArchetype", "Weapon '{0}' has invalid archetype"),
                FText::FromName(ItemData.ItemID)
            ),
            FunctionName
        );
    }

    if (ItemData.bIsAmmo && !ItemData.AmmoCaliber.IsValid())
    {
        return FInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InvalidItem,
            FText::Format(
                NSLOCTEXT("SuspenseInventory", "InvalidAmmoCaliber", "Ammo '{0}' has invalid caliber"),
                FText::FromName(ItemData.ItemID)
            ),
            FunctionName
        );
    }

    return FInventoryOperationResult::Success(FunctionName);
}

//==================================================================
// Type Checking Implementation
//==================================================================

bool USuspenseInventoryConstraints::IsItemTypeAllowed(const FGameplayTag& ItemType) const
{
    if (AllowedItemTypes.IsEmpty())
    {
        return true;
    }

    for (const FGameplayTag& AllowedTag : AllowedItemTypes)
    {
        if (ItemType == AllowedTag || ItemType.MatchesTag(AllowedTag))
        {
            return true;
        }
    }

    return false;
}

bool USuspenseInventoryConstraints::IsItemAllowedByAllCriteria(const FSuspenseUnifiedItemData& ItemData) const
{
    if (!IsItemTypeAllowed(ItemData.ItemType))
    {
        return false;
    }

    if (ItemData.bIsWeapon && ItemData.WeaponArchetype.IsValid())
    {
        if (!IsItemTypeAllowed(ItemData.WeaponArchetype))
        {
            return false;
        }
    }

    if (ItemData.bIsArmor && ItemData.ArmorType.IsValid())
    {
        if (!IsItemTypeAllowed(ItemData.ArmorType))
        {
            return false;
        }
    }

    return true;
}

bool USuspenseInventoryConstraints::WouldExceedWeightLimit(float CurrentWeight, float ItemWeight, int32 Amount) const
{
    if (MaxWeight <= 0.0f)
    {
        return false;
    }

    return (CurrentWeight + (ItemWeight * Amount)) > MaxWeight;
}

//==================================================================
// Configuration Implementation
//==================================================================

void USuspenseInventoryConstraints::SetMaxWeight(float NewMaxWeight)
{
    MaxWeight = FMath::Max(0.0f, NewMaxWeight);
    UE_LOG(LogSuspenseConstraints, Log, TEXT("InventoryConstraints: Max weight updated to %.1f"), MaxWeight);
}

void USuspenseInventoryConstraints::SetAllowedItemTypes(const FGameplayTagContainer& NewAllowedTypes)
{
    AllowedItemTypes = NewAllowedTypes;
    UE_LOG(LogSuspenseConstraints, Log, TEXT("InventoryConstraints: Allowed types updated to %s"), *AllowedItemTypes.ToStringSimple());
}

//==================================================================
// Debug Methods Implementation
//==================================================================

FString USuspenseInventoryConstraints::GetDetailedDiagnosticInfo() const
{
    FString DiagnosticInfo;
    DiagnosticInfo += TEXT("====== INVENTORY CONSTRAINTS DIAGNOSTIC ======\n");
    DiagnosticInfo += FString::Printf(TEXT("Initialized: %s\n"), bInitialized ? TEXT("Yes") : TEXT("No"));
    DiagnosticInfo += FString::Printf(TEXT("Max Weight: %.2f kg\n"), MaxWeight);
    DiagnosticInfo += FString::Printf(TEXT("Grid Size: %dx%d (%d total slots)\n"), GridWidth, GridHeight, GridWidth * GridHeight);
    DiagnosticInfo += FString::Printf(TEXT("ItemManager: %s\n"), ItemManagerRef.IsValid() ? TEXT("Available") : TEXT("None"));

    if (AllowedItemTypes.IsEmpty())
    {
        DiagnosticInfo += TEXT("Allowed Types: All types allowed\n");
    }
    else
    {
        DiagnosticInfo += FString::Printf(TEXT("Allowed Types: %s\n"), *AllowedItemTypes.ToStringSimple());
    }

    DiagnosticInfo += TEXT("===============================================");
    return DiagnosticInfo;
}

bool USuspenseInventoryConstraints::ValidateConstraintsConfiguration(TArray<FString>& OutErrors) const
{
    OutErrors.Empty();

    if (!bInitialized)
    {
        OutErrors.Add(TEXT("Constraints object is not initialized"));
    }

    if (MaxWeight < 0.0f)
    {
        OutErrors.Add(FString::Printf(TEXT("Invalid max weight: %.2f"), MaxWeight));
    }

    if (GridWidth <= 0 || GridHeight <= 0)
    {
        OutErrors.Add(FString::Printf(TEXT("Invalid grid dimensions: %dx%d"), GridWidth, GridHeight));
    }

    return OutErrors.Num() == 0;
}

//==================================================================
// Internal Helper Methods Implementation
//==================================================================

USuspenseItemManager* USuspenseInventoryConstraints::GetValidatedItemManager() const
{
    if (!ItemManagerRef.IsValid())
    {
        return nullptr;
    }

    return ItemManagerRef.Get();
}

bool USuspenseInventoryConstraints::GetUnifiedDataForInstance(const FSuspenseInventoryItemInstance& ItemInstance, FSuspenseUnifiedItemData& OutUnifiedData) const
{
    USuspenseItemManager* ItemManager = GetValidatedItemManager();
    if (!ItemManager)
    {
        return false;
    }

    return ItemManager->GetUnifiedItemData(ItemInstance.ItemID, OutUnifiedData);
}

FInventoryOperationResult USuspenseInventoryConstraints::ValidateUnifiedDataBasics(const FSuspenseUnifiedItemData& ItemData, int32 Amount, const FName& FunctionName) const
{
    if (ItemData.ItemID.IsNone())
    {
        return FInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InvalidItem,
            NSLOCTEXT("SuspenseInventory", "MissingItemID", "Item ID is required"),
            FunctionName
        );
    }

    if (Amount <= 0)
    {
        return FInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InvalidItem,
            FText::Format(
                NSLOCTEXT("SuspenseInventory", "InvalidAmount", "Invalid amount: {0}"),
                FText::AsNumber(Amount)
            ),
            FunctionName
        );
    }

    if (Amount > ItemData.MaxStackSize)
    {
        return FInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InvalidItem,
            FText::Format(
                NSLOCTEXT("SuspenseInventory", "ExceedsMaxStack", "Amount {0} exceeds maximum stack size {1}"),
                FText::AsNumber(Amount),
                FText::AsNumber(ItemData.MaxStackSize)
            ),
            FunctionName
        );
    }

    if (ItemData.GridSize.X <= 0 || ItemData.GridSize.Y <= 0)
    {
        return FInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InvalidItem,
            FText::Format(
                NSLOCTEXT("SuspenseInventory", "InvalidGridSize", "Invalid grid size: {0}x{1}"),
                FText::AsNumber(ItemData.GridSize.X),
                FText::AsNumber(ItemData.GridSize.Y)
            ),
            FunctionName
        );
    }

    if (ItemData.Weight < 0.0f)
    {
        return FInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InvalidItem,
            FText::Format(
                NSLOCTEXT("SuspenseInventory", "NegativeWeight", "Item has negative weight: {0}"),
                FText::AsNumber(ItemData.Weight)
            ),
            FunctionName
        );
    }

    return FInventoryOperationResult::Success(FunctionName);
}

FVector2D USuspenseInventoryConstraints::CalculateEffectiveItemSize(const FIntPoint& BaseSize, bool bIsRotated) const
{
    if (bIsRotated)
    {
        return FVector2D(BaseSize.Y, BaseSize.X);
    }
    else
    {
        return FVector2D(BaseSize.X, BaseSize.Y);
    }
}

TArray<int32> USuspenseInventoryConstraints::GetOccupiedSlots(int32 AnchorIndex, const FVector2D& ItemSize) const
{
    TArray<int32> OccupiedSlotsArray;

    if (AnchorIndex < 0 || GridWidth <= 0)
    {
        return OccupiedSlotsArray;
    }

    int32 AnchorX = AnchorIndex % GridWidth;
    int32 AnchorY = AnchorIndex / GridWidth;

    int32 ItemWidth = FMath::CeilToInt(ItemSize.X);
    int32 ItemHeight = FMath::CeilToInt(ItemSize.Y);

    for (int32 Y = 0; Y < ItemHeight; ++Y)
    {
        for (int32 X = 0; X < ItemWidth; ++X)
        {
            int32 SlotX = AnchorX + X;
            int32 SlotY = AnchorY + Y;

            if (SlotX >= 0 && SlotX < GridWidth && SlotY >= 0 && SlotY < GridHeight)
            {
                int32 SlotIndex = SlotY * GridWidth + SlotX;
                OccupiedSlotsArray.Add(SlotIndex);
            }
        }
    }

    return OccupiedSlotsArray;
}

void USuspenseInventoryConstraints::LogValidationResult(const FInventoryOperationResult& Result, const FString& Context) const
{
    if (Result.IsSuccess())
    {
        UE_LOG(LogSuspenseConstraints, VeryVerbose, TEXT("ValidationResult [%s]: SUCCESS"), *Context);
    }
    else
    {
        UE_LOG(LogSuspenseConstraints, Warning, TEXT("ValidationResult [%s]: FAILURE - %s (ErrorCode: %s, Context: %s)"),
            *Context,
            *Result.ErrorMessage.ToString(),
            *UEnum::GetValueAsString(Result.ErrorCode),
            *Result.Context.ToString());
    }
}
