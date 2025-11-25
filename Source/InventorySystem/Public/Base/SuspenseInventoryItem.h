// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Inventory/IMedComInventoryItemInterface.h"
#include "Types/Inventory/InventoryTypes.h"
#include "Types/Loadout/MedComItemDataTable.h"
#include "Engine/Texture2D.h"
#include "SuspenseInventoryItem.generated.h"

// Forward declarations для чистого разделения модулей
class UMedComItemManager;
struct FMedComUnifiedItemData;

/**
 * Actor-based inventory item implementation for DataTable architecture
 * 
 * АРХИТЕКТУРНЫЕ ПРИНЦИПЫ:
 * - Runtime состояние хранится в FInventoryItemInstance  
 * - Статические данные получаются из DataTable через ItemManager
 * - Полная интеграция с LoadoutSettings и новой системой предметов
 * - Оптимизация производительности через интеллектуальное кэширование
 * - Отсутствие legacy зависимостей - чистая современная архитектура
 * 
 * КЛЮЧЕВЫЕ ВОЗМОЖНОСТИ:
 * - Автоматическая инициализация runtime свойств на основе типа предмета
 * - Поддержка поворота предметов в инвентаре с правильным расчетом размеров
 * - Система валидации состояния для обнаружения проблем
 * - Расширенная отладочная информация для development
 * - Интеграция с GAS через runtime свойства (патроны, прочность)
 */
UCLASS(BlueprintType, Blueprintable)
class INVENTORYSYSTEM_API AMedComInventoryItem : public AActor, public IMedComInventoryItemInterface
{
    GENERATED_BODY()
    
public:
    //==================================================================
    // Constructor and Core Actor Lifecycle
    //==================================================================
    
    AMedComInventoryItem();

    //~ Begin AActor Interface
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    //~ End AActor Interface

    //==================================================================
    // IMedComInventoryItemInterface Implementation (Pure DataTable)
    //==================================================================

    // Core item identification and data access
    virtual FName GetItemID() const override { return ItemInstance.ItemID; }
    virtual bool GetItemData(FMedComUnifiedItemData& OutItemData) const override;
    virtual UMedComItemManager* GetItemManager() const override;
    
    // Quantity management with DataTable validation
    virtual int32 GetAmount() const override { return ItemInstance.Quantity; }
    virtual bool TrySetAmount(int32 NewAmount) override;
    
    // Runtime properties system for GAS integration
    virtual float GetRuntimeProperty(const FName& PropertyName, float DefaultValue = 0.0f) const override;
    virtual void SetRuntimeProperty(const FName& PropertyName, float Value) override;
    virtual bool HasRuntimeProperty(const FName& PropertyName) const override;
    virtual void ClearRuntimeProperty(const FName& PropertyName) override;
    
    // Grid size management - always from DataTable
    virtual FVector2D GetEffectiveGridSize() const override;
    virtual FVector2D GetBaseGridSize() const override;
    
    // Inventory positioning
    virtual int32 GetAnchorIndex() const override { return ItemInstance.AnchorIndex; }
    virtual void SetAnchorIndex(int32 InAnchorIndex) override { ItemInstance.AnchorIndex = InAnchorIndex; }
    
    // Item rotation in inventory
    virtual bool IsRotated() const override { return ItemInstance.bIsRotated; }
    virtual void SetRotated(bool bRotated) override;
    
    // Initialization and state management
    virtual bool IsInitialized() const override { return bIsInitialized; }
    virtual bool InitializeFromID(const FName& InItemID, int32 InAmount) override;
    
    // Weapon ammo state persistence (for equipped weapons)
    virtual float GetSavedCurrentAmmo() const override;
    virtual float GetSavedRemainingAmmo() const override;
    virtual bool HasSavedAmmoState() const override;
    virtual void SetSavedAmmoState(float CurrentAmmo, float RemainingAmmo) override;
    virtual void ClearSavedAmmoState() override;
    
    // Complete runtime instance access
    virtual const FInventoryItemInstance& GetItemInstance() const override { return ItemInstance; }
    virtual bool SetItemInstance(const FInventoryItemInstance& InInstance) override;
    virtual FGuid GetInstanceID() const override { return ItemInstance.InstanceID; }
    //~ End IMedComInventoryItemInterface

    //==================================================================
    // Extended Functionality for Gameplay Systems
    //==================================================================
    
    /**
     * Update item from DataTable source
     * Refreshes cached static data while preserving runtime state
     */
    UFUNCTION(BlueprintCallable, Category = "Item|DataTable")
    void RefreshFromDataTable();

    //==================================================================
    // Advanced Grid and Placement System
    //==================================================================

    /**
     * Get item grid size for specific rotation
     * Useful for UI preview and placement validation
     * @param bForRotated Calculate size for rotated orientation
     * @return Grid size in cells for the specified orientation
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Grid")
    FVector2D GetGridSizeForRotation(bool bForRotated) const;
    
    /**
     * Check if item can fit in grid with specified dimensions
     * @param GridWidth Width of target grid
     * @param GridHeight Height of target grid  
     * @param bCheckRotated Test rotated orientation instead of current
     * @return True if item fits in the specified grid
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Grid")
    bool CanFitInGrid(int32 GridWidth, int32 GridHeight, bool bCheckRotated = false) const;
    
    /**
     * Get optimal rotation for placement in specific grid
     * Automatically determines best orientation to save space
     * @param GridWidth Width of target grid
     * @param GridHeight Height of target grid
     * @return True if item should be rotated for optimal placement
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Grid")
    bool GetOptimalRotationForGrid(int32 GridWidth, int32 GridHeight) const;

    //==================================================================
    // Item Type and Properties Query System
    //==================================================================

    /**
     * Check if this item is equippable
     * @return True if item can be equipped
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Properties")
    bool IsEquippable() const;
    
    /**
     * Check if this item is a weapon
     * @return True if item is classified as weapon in DataTable
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Properties")
    bool IsWeapon() const;
    
    /**
     * Check if this item is armor
     * @return True if item is classified as armor in DataTable
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Properties")
    bool IsArmor() const;
    
    /**
     * Check if this item is consumable
     * @return True if item can be consumed/used
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Properties")
    bool IsConsumable() const;
    
    /**
     * Check if this item is ammunition
     * @return True if item is classified as ammo in DataTable
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Properties")
    bool IsAmmo() const;
    
    /**
     * Get item weight from DataTable
     * @return Weight of single item unit
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Properties")
    float GetItemWeight() const;
    
    /**
     * Get total weight of this item stack
     * @return Total weight (item weight * quantity)
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Properties")
    float GetTotalWeight() const;
    
    /**
     * Get maximum stack size from DataTable
     * @return Maximum number of items that can stack together
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Properties")
    int32 GetMaxStackSize() const;

    //==================================================================
    // Enhanced Runtime Properties for Gameplay Systems
    //==================================================================

    /**
     * Get current durability of item
     * @return Current durability value or 0 if not applicable
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Durability")
    float GetCurrentDurability() const;
    
    /**
     * Get maximum durability of item
     * @return Maximum durability value or 0 if not applicable
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Durability")
    float GetMaxDurability() const;
    
    /**
     * Get durability as percentage (0.0 to 1.0)
     * @return Durability percentage or 1.0 if no durability system
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Durability")
    float GetDurabilityPercent() const;
    
    /**
     * Set current durability with automatic clamping
     * @param NewDurability New durability value (will be clamped to max)
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Durability")
    void SetCurrentDurability(float NewDurability);
    
    /**
     * Get current ammo count for weapons
     * @return Current ammo in weapon or 0 if not applicable
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Weapon")
    int32 GetCurrentAmmo() const;
    
    /**
     * Get maximum ammo capacity for weapons
     * @return Maximum ammo capacity or 0 if not applicable
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Weapon")
    int32 GetMaxAmmo() const;
    
    /**
     * Set current ammo with automatic clamping
     * @param NewAmmo New ammo count (will be clamped to max)
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Weapon")
    void SetCurrentAmmo(int32 NewAmmo);

    //==================================================================
    // Validation and Debug Support
    //==================================================================

    /**
     * Comprehensive validation of item state
     * Checks consistency between runtime instance and DataTable
     * @param OutErrors Array to receive detailed validation errors
     * @return True if item is in completely valid state
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Debug")
    bool ValidateItemState(TArray<FString>& OutErrors) const;
    
    /**
     * Get detailed debug information about item state
     * Useful for development and troubleshooting
     * @return Comprehensive debug string with all item details
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Debug")
    FString GetItemStateDebugInfo() const;
    
    /**
     * Check if item data is currently cached
     * @return True if static data is cached and fresh
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Debug")
    bool IsDataCached() const;
    
    /**
     * Get cache age in seconds
     * @return How long current cache has been valid
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Debug")
    float GetCacheAge() const;

protected:
    //==================================================================
    // Core Data Members
    //==================================================================
    
    /** 
     * Runtime item instance containing all dynamic data
     * This is the core data structure that holds:
     * - ItemID (link to DataTable)
     * - Quantity (stack size)
     * - Position data (AnchorIndex, bIsRotated)
     * - Runtime properties (durability, ammo, etc.)
     * - Unique GUID for multiplayer tracking
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Core")
    FInventoryItemInstance ItemInstance;
    
    /** 
     * Whether item has been properly initialized
     * Guards against using uninitialized item data
     */
    UPROPERTY(BlueprintReadOnly, Category = "Item|State")
    bool bIsInitialized;

private:
    //==================================================================
    // Performance Optimization - Caching System
    //==================================================================
    
    /** 
     * Cached reference to ItemManager subsystem
     * Avoids repeated subsystem lookups for better performance
     */
    UPROPERTY()
    mutable UMedComItemManager* CachedItemManager;
    
    /** 
     * Cached static item data from DataTable
     * Reduces DataTable queries for frequently accessed data
     */
    mutable TOptional<FMedComUnifiedItemData> CachedItemData;
    
    /** 
     * Timestamp when cache was last updated
     * Used to determine when cache refresh is needed
     */
    mutable FDateTime CacheTime;
    
    /** Cache duration in seconds before refresh is required */
    static constexpr float CACHE_DURATION = 5.0f;
    
    //==================================================================
    // Internal Helper Methods
    //==================================================================
    
    /** Update cached data if needed (checks cache age) */
    void UpdateCachedData() const;
    
    /** Force clear all cached data */
    void ClearCachedData();
    
    /** Internal validation with detailed error reporting */
    bool ValidateItemStateInternal(TArray<FString>& OutErrors) const;
    
    /** Get cached item data with automatic refresh */
    const FMedComUnifiedItemData* GetCachedUnifiedData() const;
};