// SuspenseInventory/Validation/SuspenseInventoryConstraints.h
// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "Operations/InventoryResult.h"
#include "GameplayTagContainer.h"
#include "SuspenseInventoryConstraints.generated.h"

// Forward declarations for clean separation between modules
class USuspenseItemManager;
struct FSuspenseInventoryItemInstance;
struct FSuspenseUnifiedItemData;

/**
 * Fully updated class for inventory operation validation
 *
 * Architectural principles of new version:
 * - Integration with DataTable as source of truth for static data
 * - Support for FInventoryItemInstance for runtime validation
 * - Centralized data access through USuspenseItemManager
 * - Enhanced error reporting with detailed diagnostics
 * - Backward compatibility with legacy structures during migration
 * - Thread-safe operations for multiplayer environment
 */
UCLASS(BlueprintType)
class INVENTORYSYSTEM_API USuspenseInventoryConstraints : public UObject
{
    GENERATED_BODY()

public:
    //==================================================================
    // Lifecycle and Initialization
    //==================================================================

    USuspenseInventoryConstraints();

    /**
     * Initializes constraints with specific settings
     * Can work with legacy parameters or LoadoutSettings
     *
     * @param InMaxWeight Maximum weight limit
     * @param InAllowedTypes Container of allowed item types
     * @param InGridWidth Inventory grid width
     * @param InGridHeight Inventory grid height
     * @param InItemManager Optional reference to ItemManager for DataTable access
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Constraints")
    void Initialize(float InMaxWeight, const FGameplayTagContainer& InAllowedTypes, int32 InGridWidth, int32 InGridHeight, USuspenseItemManager* InItemManager = nullptr);

    /**
     * Initialize from LoadoutSettings (recommended method)
     * Automatically retrieves all necessary parameters from configuration
     *
     * @param LoadoutID Loadout configuration identifier
     * @param InventoryName Name of specific inventory in loadout
     * @param WorldContext Context for ItemManager subsystem access
     * @return true if initialization was successful
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Constraints")
    bool InitializeFromLoadout(const FName& LoadoutID, const FName& InventoryName, const UObject* WorldContext);

    /**
     * Checks if constraints are ready for use
     * @return true if constraints are fully initialized
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Constraints")
    bool IsInitialized() const { return bInitialized; }

    //==================================================================
    // Enhanced Unified Data Validation
    //==================================================================

    /**
     * Main validation method for unified item data from DataTable
     * Replaces legacy ValidateItemParams for new architecture
     *
     * @param ItemData Unified item data from DataTable
     * @param Amount Item quantity
     * @param FunctionName Context for logging
     * @return Validation result with detailed error information
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateUnifiedItemData(const FSuspenseUnifiedItemData& ItemData, int32 Amount, const FName& FunctionName) const;

    /**
     * Validation of unified data with type restriction checks
     * Includes all basic checks plus item type restrictions
     *
     * @param ItemData Unified item data
     * @param Amount Item quantity
     * @param FunctionName Context for logging
     * @return Validation result
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateUnifiedItemDataWithRestrictions(const FSuspenseUnifiedItemData& ItemData, int32 Amount, const FName& FunctionName) const;

    //==================================================================
    // Runtime Instance Validation
    //==================================================================

    /**
     * Validation of runtime item instance
     * Checks both static DataTable data and runtime state
     *
     * @param ItemInstance Runtime item instance
     * @param FunctionName Context for logging
     * @return Instance validation result
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateItemInstance(const FInventoryItemInstance& ItemInstance, const FName& FunctionName) const;

    /**
     * Validation of array of runtime instances
     * Useful for bulk operations and batch validation
     *
     * @param ItemInstances Array of instances to check
     * @param FunctionName Context for logging
     * @param OutFailedInstances Output array of instances that failed validation
     * @return Number of successfully validated instances
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    int32 ValidateItemInstances(const TArray<FInventoryItemInstance>& ItemInstances, const FName& FunctionName, TArray<FInventoryItemInstance>& OutFailedInstances) const;

    /**
     * Validation of instance runtime properties
     * Checks consistency and validity of dynamic properties
     *
     * @param ItemInstance Runtime instance to check
     * @param FunctionName Context for logging
     * @return Runtime properties validation result
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateRuntimeProperties(const FInventoryItemInstance& ItemInstance, const FName& FunctionName) const;

    //==================================================================
    // Grid and Spatial Validation
    //==================================================================

    /**
     * Slot index validation against grid bounds
     * @param SlotIndex Index to check
     * @param FunctionName Context for logging
     * @return Validation result
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateSlotIndex(int32 SlotIndex, const FName& FunctionName) const;

    /**
     * Grid bounds validation for unified data
     * Works with item sizes from DataTable considering rotation
     *
     * @param ItemData Unified item data
     * @param AnchorIndex Starting placement index
     * @param bIsRotated Is item rotated 90 degrees
     * @param FunctionName Context for logging
     * @return Bounds validation result
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateGridBoundsForUnified(const FSuspenseUnifiedItemData& ItemData, int32 AnchorIndex, bool bIsRotated, const FName& FunctionName) const;

    /**
     * Bounds validation for runtime instance
     * Gets sizes from DataTable automatically
     *
     * @param ItemInstance Runtime item instance
     * @param AnchorIndex Starting placement index
     * @param FunctionName Context for logging
     * @return Bounds validation result
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateGridBoundsForInstance(const FInventoryItemInstance& ItemInstance, int32 AnchorIndex, const FName& FunctionName) const;

    /**
     * Item placement validation with collision detection
     * Checks not only bounds but also collisions with other items
     *
     * @param ItemData Unified item data
     * @param AnchorIndex Placement position
     * @param bIsRotated Item rotation
     * @param OccupiedSlots Array of already occupied slots
     * @param FunctionName Context for logging
     * @return Placement validation result
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateItemPlacement(const FSuspenseUnifiedItemData& ItemData, int32 AnchorIndex, bool bIsRotated, const TArray<bool>& OccupiedSlots, const FName& FunctionName) const;

    //==================================================================
    // Weight Validation
    //==================================================================

    /**
     * Weight validation for unified data
     * Uses weight from DataTable considering quantity
     *
     * @param ItemData Unified item data
     * @param Amount Item quantity
     * @param CurrentWeight Current inventory weight
     * @param FunctionName Context for logging
     * @return Weight validation result
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateWeightForUnified(const FSuspenseUnifiedItemData& ItemData, int32 Amount, float CurrentWeight, const FName& FunctionName) const;

    /**
     * Weight validation for runtime instance
     * Gets weight from DataTable automatically
     *
     * @param ItemInstance Runtime item instance
     * @param CurrentWeight Current inventory weight
     * @param FunctionName Context for logging
     * @return Weight validation result
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateWeightForInstance(const FInventoryItemInstance& ItemInstance, float CurrentWeight, const FName& FunctionName) const;

    /**
     * Check if weight limit would be exceeded for unified data
     * @param ItemData Unified item data
     * @param Amount Item quantity
     * @param CurrentWeight Current weight
     * @return true if limit would be exceeded
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Validation")
    bool WouldExceedWeightLimitUnified(const FSuspenseUnifiedItemData& ItemData, int32 Amount, float CurrentWeight) const;

    //==================================================================
    // Object Validation
    //==================================================================

    /**
     * Item object validation for operations
     * Checks interface implementation and initialization
     *
     * @param ItemObject Item object to check
     * @param FunctionName Context for logging
     * @return Object validation result
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateItemForOperation(UObject* ItemObject, const FName& FunctionName) const;

    /**
     * Item compatibility validation with inventory
     * Comprehensive check of all restrictions for specific item
     *
     * @param ItemData Unified item data
     * @param Amount Item quantity
     * @param CurrentWeight Current inventory weight
     * @param FunctionName Context for logging
     * @return Comprehensive validation result
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateItemCompatibility(const FSuspenseUnifiedItemData& ItemData, int32 Amount, float CurrentWeight, const FName& FunctionName) const;

    //==================================================================
    // Type and Restriction Checking
    //==================================================================

    /**
     * Check if item type is allowed in constraints
     * Supports hierarchical tags for flexible configuration
     *
     * @param ItemType Item type tag to check
     * @return true if type is allowed
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Validation")
    bool IsItemTypeAllowed(const FGameplayTag& ItemType) const;

    /**
     * Check if item is allowed by multiple criteria
     * Considers type, tags, special restrictions
     *
     * @param ItemData Unified item data
     * @return true if item is allowed by all criteria
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Validation")
    bool IsItemAllowedByAllCriteria(const FSuspenseUnifiedItemData& ItemData) const;

    /**
     * Check if weight limit would be exceeded (legacy method)
     * @param CurrentWeight Current weight
     * @param ItemWeight Item weight
     * @param Amount Item quantity
     * @return true if limit would be exceeded
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Validation")
    bool WouldExceedWeightLimit(float CurrentWeight, float ItemWeight, int32 Amount) const;

    //==================================================================
    // Configuration Access and Modification
    //==================================================================

    /**
     * Get maximum weight limit
     * @return Maximum weight limit
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Configuration")
    float GetMaxWeight() const { return MaxWeight; }

    /**
     * Set new weight limit
     * @param NewMaxWeight New weight limit
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Configuration")
    void SetMaxWeight(float NewMaxWeight);

    /**
     * Get allowed item types
     * @return Allowed types container
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Configuration")
    const FGameplayTagContainer& GetAllowedItemTypes() const { return AllowedItemTypes; }

    /**
     * Set new allowed item types
     * @param NewAllowedTypes New allowed types
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Configuration")
    void SetAllowedItemTypes(const FGameplayTagContainer& NewAllowedTypes);

    /**
     * Get inventory grid dimensions
     * @return Grid size (width x height)
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Configuration")
    FVector2D GetGridSize() const { return FVector2D(GridWidth, GridHeight); }

    /**
     * Get total number of slots in grid
     * @return Total slot count
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Configuration")
    int32 GetTotalSlots() const { return GridWidth * GridHeight; }

    //==================================================================
    // Debug and Diagnostic Methods
    //==================================================================

    /**
     * Get detailed diagnostic information
     * Useful for debugging and performance monitoring
     *
     * @return String with detailed constraints information
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    FString GetDetailedDiagnosticInfo() const;

    /**
     * Validate all constraints settings
     * Checks internal configuration consistency
     *
     * @param OutErrors Output array of found errors
     * @return true if all settings are valid
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    bool ValidateConstraintsConfiguration(TArray<FString>& OutErrors) const;

private:
    //==================================================================
    // Internal Data Members
    //==================================================================

    /** Maximum weight inventory can contain */
    UPROPERTY()
    float MaxWeight;

    /** Allowed item types for this inventory */
    UPROPERTY()
    FGameplayTagContainer AllowedItemTypes;

    /** Grid width for bounds checking */
    UPROPERTY()
    int32 GridWidth;

    /** Grid height for bounds checking */
    UPROPERTY()
    int32 GridHeight;

    /** Whether constraints object is initialized */
    UPROPERTY()
    bool bInitialized;

    /** Weak reference to ItemManager for DataTable access */
    UPROPERTY()
    TWeakObjectPtr<USuspenseItemManager> ItemManagerRef;

    //==================================================================
    // Internal Helper Methods
    //==================================================================

    /**
     * Get validated ItemManager for DataTable access
     * Centralized logic with proper error handling
     *
     * @return Valid ItemManager or nullptr
     */
    USuspenseItemManager* GetValidatedItemManager() const;

    /**
     * Get unified data for runtime instance
     * Internal helper for getting DataTable data from instance
     *
     * @param ItemInstance Runtime instance
     * @param OutUnifiedData Output unified data
     * @return true if data was retrieved successfully
     */
    bool GetUnifiedDataForInstance(const FInventoryItemInstance& ItemInstance, FSuspenseUnifiedItemData& OutUnifiedData) const;

    /**
     * Validate basic parameters of unified data
     * Internal helper for checking fundamental properties
     *
     * @param ItemData Unified data to check
     * @param Amount Item quantity
     * @param FunctionName Context for logging
     * @return Basic validation result
     */
    FInventoryOperationResult ValidateUnifiedDataBasics(const FSuspenseUnifiedItemData& ItemData, int32 Amount, const FName& FunctionName) const;

    /**
     * Calculate effective item size considering rotation
     * @param BaseSize Base size from DataTable
     * @param bIsRotated Is item rotated
     * @return Effective size for placement
     */
    FVector2D CalculateEffectiveItemSize(const FIntPoint& BaseSize, bool bIsRotated) const;

    /**
     * Get all slots that item will occupy
     * @param AnchorIndex Anchor index
     * @param ItemSize Item size
     * @return Array of all occupied slots
     */
    TArray<int32> GetOccupiedSlots(int32 AnchorIndex, const FVector2D& ItemSize) const;

    /**
     * Log validation result for debugging
     * @param Result Validation result
     * @param Context Additional context
     */
    void LogValidationResult(const FInventoryOperationResult& Result, const FString& Context) const;
};
