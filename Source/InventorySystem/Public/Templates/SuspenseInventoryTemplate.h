// SuspenseInventoryTemplate.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Loadout/LoadoutSettings.h"
#include "GameplayTagContainer.h"
#include "SuspenseInventoryTemplate.generated.h"

// Forward declarations
class USuspenseInventoryComponent;
class USuspenseItemManager;
struct FSuspenseUnifiedItemData;

/**
 * Inventory template configuration focused solely on inventory setup
 * Acts as a helper class for creating inventory configurations
 */
USTRUCT(BlueprintType)
struct INVENTORYSYSTEM_API FInventoryTemplateConfig
{
    GENERATED_BODY()
    
    /** Unique name for this inventory */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    FName InventoryName = TEXT("MainInventory");
    
    /** Grid dimensions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Grid", meta = (ClampMin = "1", ClampMax = "50"))
    int32 GridWidth = 10;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Grid", meta = (ClampMin = "1", ClampMax = "50"))
    int32 GridHeight = 5;
    
    /** Maximum weight capacity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Constraints", meta = (ClampMin = "0.0"))
    float MaxWeight = 100.0f;
    
    /** Allowed item types */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Filters", meta = (Categories = "Item.Type"))
    FGameplayTagContainer AllowedItemTypes;
    
    /** Disallowed item types (takes priority) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Filters", meta = (Categories = "Item.Type"))
    FGameplayTagContainer DisallowedItemTypes;

    /** Starting items for this inventory */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Items")
    TArray<FSuspensePickupSpawnData> StartingItems;
    
    /** Auto-place starting items optimally */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Behavior")
    bool bAutoPlaceStartingItems = true;
    
    /** Allow item rotation for optimal placement */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Behavior")
    bool bAllowItemRotation = true;
    
    /** Priority for auto-placement */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Behavior", meta = (ClampMin = "0"))
    int32 PlacementPriority = 0;
    
    /** Constructor with defaults */
    FInventoryTemplateConfig()
    {
        InventoryName = TEXT("MainInventory");
        GridWidth = 10;
        GridHeight = 5;
        MaxWeight = 100.0f;
        bAutoPlaceStartingItems = true;
        bAllowItemRotation = true;
        PlacementPriority = 0;
    }
    
    /** Get total grid cells */
    int32 GetTotalCells() const { return GridWidth * GridHeight; }
    
    /** Get estimated UI size */
    FVector2D GetEstimatedUISize(float CellSize = 64.0f) const 
    { 
        return FVector2D(GridWidth * CellSize, GridHeight * CellSize); 
    }
    
    /** Validate configuration */
    bool IsValid() const
    {
        return GridWidth > 0 && GridHeight > 0 && MaxWeight >= 0.0f && !InventoryName.IsNone();
    }
    
    /**
     * Convert to FInventoryConfig for LoadoutManager
     * @return Converted configuration
     */
    FInventoryConfig ToInventoryConfig() const
    {
        FInventoryConfig Config;
        Config.InventoryName = FText::FromName(InventoryName);
        Config.Width = GridWidth;
        Config.Height = GridHeight;
        Config.MaxWeight = MaxWeight;
        Config.AllowedItemTypes = AllowedItemTypes;
        Config.DisallowedItemTypes = DisallowedItemTypes;
        Config.StartingItems = StartingItems;
        return Config;
    }
};

/**
 * Template analysis result for inventory requirements
 */
USTRUCT(BlueprintType)
struct INVENTORYSYSTEM_API FInventoryTemplateAnalysis
{
    GENERATED_BODY()
    
    /** Total grid cells across all inventories */
    UPROPERTY(BlueprintReadOnly)
    int32 TotalGridCells = 0;
    
    /** Total maximum weight capacity */
    UPROPERTY(BlueprintReadOnly)
    float TotalMaxWeight = 0.0f;
    
    /** Number of inventories */
    UPROPERTY(BlueprintReadOnly)
    int32 InventoryCount = 0;
    
    /** Starting item count */
    UPROPERTY(BlueprintReadOnly)
    int32 StartingItemCount = 0;
    
    /** Estimated starting weight */
    UPROPERTY(BlueprintReadOnly)
    float EstimatedStartingWeight = 0.0f;
    
    /** Unique item types in starting items */
    UPROPERTY(BlueprintReadOnly)
    TArray<FGameplayTag> UniqueItemTypes;
    
    /** Warnings about potential issues */
    UPROPERTY(BlueprintReadOnly)
    TArray<FString> Warnings;
    
    /** Whether template is complex */
    UPROPERTY(BlueprintReadOnly)
    bool bIsComplex = false;
};

/**
 * Inventory template helper class for creating inventory configurations
 * 
 * KEY FEATURES:
 * - Focused solely on inventory configuration
 * - Acts as a helper for LoadoutManager integration
 * - Provides pre-built templates for common setups
 * - Validates against DataTable
 * - No equipment or loadout responsibility
 * 
 * USAGE:
 * Templates are used to quickly configure inventories with
 * validated settings and starting items. They export configurations
 * that can be used by LoadoutManager.
 */
UCLASS(BlueprintType, Blueprintable)
class INVENTORYSYSTEM_API USuspenseInventoryTemplate : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    //==================================================================
    // Constructor and Basic Properties
    //==================================================================
    
    USuspenseInventoryTemplate();

    /** Display name for UI */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template|Identity")
    FText DisplayName;

    /** Detailed description */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template|Identity", meta = (MultiLine = "true"))
    FText Description;

    /** Icon for UI */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template|Identity")
    TSoftObjectPtr<UTexture2D> Icon;
    
    /** Template category for organization */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template|Identity", meta = (Categories = "Template.Category"))
    FGameplayTag TemplateCategory;
    
    /** Complexity level (1-10) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template|Identity", meta = (ClampMin = "1", ClampMax = "10"))
    int32 ComplexityLevel = 1;

    //==================================================================
    // Inventory Configuration
    //==================================================================
    
    /** Main inventory configuration */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template|Inventories")
    FInventoryTemplateConfig MainInventoryConfig;
    
    /** Additional inventories (backpacks, pouches, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template|Inventories")
    TMap<FName, FInventoryTemplateConfig> AdditionalInventories;
    
    /** Character classes compatible with this template */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template|Compatibility", meta = (Categories = "Character.Class"))
    FGameplayTagContainer CompatibleCharacterClasses;

    //==================================================================
    // Template Export and Application
    //==================================================================
    
    /**
     * Export inventory configurations for use with LoadoutManager
     * @return Map of inventory name to configuration
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Template")
    TMap<FName, FInventoryConfig> ExportInventoryConfigurations() const;
    
    /**
     * Apply template directly to inventory component
     * @param InventoryComponent Component to configure
     * @param InventoryName Specific inventory to apply (empty = main)
     * @return true if successfully applied
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Template")
    bool ApplyToInventoryComponent(USuspenseInventoryComponent* InventoryComponent, 
                                 const FName& InventoryName = NAME_None) const;
    
    /**
     * Create starting items in inventory
     * @param InventoryComponent Target inventory
     * @param InventoryName Specific inventory name
     * @return Number of items created
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Template")
    int32 CreateStartingItemsInInventory(USuspenseInventoryComponent* InventoryComponent,
                                       const FName& InventoryName = NAME_None) const;

    //==================================================================
    // Validation and Analysis
    //==================================================================
    
    /**
     * Validate template against DataTable
     * @param OutMissingItems Items not found in DataTable
     * @param OutValidationErrors Detailed validation errors
     * @return true if template is valid
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    bool ValidateTemplate(TArray<FName>& OutMissingItems, TArray<FString>& OutValidationErrors) const;
    
    /**
     * Analyze template requirements
     * @return Analysis results
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Analysis")
    FInventoryTemplateAnalysis AnalyzeTemplate() const;
    
    /**
     * Check compatibility with character class
     * @param CharacterClass Class to check
     * @return true if compatible
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Compatibility")
    bool IsCompatibleWithCharacterClass(const FGameplayTag& CharacterClass) const;

    //==================================================================
    // Static Template Factory Methods
    //==================================================================
    
    /**
     * Get all predefined templates
     * @param CategoryFilter Optional category filter
     * @return Array of templates
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Factory", CallInEditor)
    static TArray<USuspenseInventoryTemplate*> GetPredefinedTemplates(const FGameplayTag& CategoryFilter = FGameplayTag());

    /** Create minimal inventory template */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Factory", CallInEditor)
    static USuspenseInventoryTemplate* CreateMinimalTemplate();

    /** Create standard inventory template */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Factory", CallInEditor)
    static USuspenseInventoryTemplate* CreateStandardTemplate();

    /** Create expanded inventory template */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Factory", CallInEditor)
    static USuspenseInventoryTemplate* CreateExpandedTemplate();

    /** Create survival-focused template */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Factory", CallInEditor)
    static USuspenseInventoryTemplate* CreateSurvivalTemplate();

    /** Create specialized template for character class */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Factory", CallInEditor)
    static USuspenseInventoryTemplate* CreateClassSpecificTemplate(const FGameplayTag& CharacterClass);

    //==================================================================
    // Template Customization
    //==================================================================
    
    /**
     * Create customized copy with modifications
     * @param Modifications Map of property changes
     * @return New customized template
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Customization")
    USuspenseInventoryTemplate* CreateCustomizedCopy(const TMap<FString, FString>& Modifications) const;
    
    /**
     * Add additional inventory
     * @param InventoryName Unique name
     * @param Configuration Inventory configuration
     * @return true if added successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Customization")
    bool AddAdditionalInventory(const FName& InventoryName, const FInventoryTemplateConfig& Configuration);
    
    /**
     * Remove additional inventory
     * @param InventoryName Name to remove
     * @return true if removed
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Customization")
    bool RemoveAdditionalInventory(const FName& InventoryName);

    //==================================================================
    // Asset Management
    //==================================================================
    
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(TEXT("InventoryTemplate"), GetFName());
    }
    
    virtual void PostLoad() override;
    
#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(TArray<FText>& ValidationErrors) override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
    //==================================================================
    // Internal Helper Methods
    //==================================================================
    
    /**
     * Get ItemManager for validation
     */
    USuspenseItemManager* GetItemManager() const;
    
    /**
     * Validate single inventory configuration
     */
    bool ValidateInventoryConfig(const FInventoryTemplateConfig& Config, 
                               const FString& ConfigName,
                               TArray<FString>& OutErrors) const;
    
    /**
     * Validate starting items
     */
    bool ValidateStartingItems(const TArray<FSuspensePickupSpawnData>& StartingItems,
                             TArray<FName>& OutMissingItems,
                             TArray<FString>& OutErrors) const;
    
    /**
     * Get configuration by name
     */
    const FInventoryTemplateConfig* GetInventoryConfig(const FName& InventoryName) const;
};