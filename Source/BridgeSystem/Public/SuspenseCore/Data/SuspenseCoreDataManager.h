// SuspenseCoreDataManager.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "SuspenseCoreDataManager.generated.h"

class UDataTable;
class UDataAsset;
class USuspenseCoreEventBus;
class USuspenseCoreSettings;
struct FSuspenseCoreEventData;

/**
 * USuspenseCoreDataManager
 *
 * Central data management subsystem for SuspenseCore.
 * Loads, caches, and provides access to all game data with EventBus integration.
 *
 * ARCHITECTURE NOTES:
 * ═══════════════════════════════════════════════════════════════════════════
 * This subsystem is the RUNTIME interface for all SuspenseCore data.
 * Configuration comes from USuspenseCoreSettings (Project Settings).
 * All operations broadcast events through SuspenseCoreEventBus.
 *
 * LIFECYCLE:
 * ═══════════════════════════════════════════════════════════════════════════
 * 1. GameInstance creates subsystem
 * 2. Initialize() loads data from USuspenseCoreSettings
 * 3. Caches data for fast runtime access
 * 4. Broadcasts SuspenseCore.Event.Data.Initialized
 * 5. Provides data access throughout game session
 * 6. Deinitialize() cleans up on shutdown
 *
 * EVENTBUS EVENTS:
 * ═══════════════════════════════════════════════════════════════════════════
 * - SuspenseCore.Event.Data.Initialized      - Data manager ready
 * - SuspenseCore.Event.Data.ItemLoaded       - Item data loaded from cache
 * - SuspenseCore.Event.Data.ItemNotFound     - Item ID not in database
 * - SuspenseCore.Event.Data.ValidationFailed - Validation error occurred
 * - SuspenseCore.Event.Data.ValidationPassed - All validations passed
 *
 * USAGE:
 * ═══════════════════════════════════════════════════════════════════════════
 * C++:
 *   USuspenseCoreDataManager* DataManager = USuspenseCoreDataManager::Get(WorldContextObject);
 *   if (DataManager)
 *   {
 *       FSuspenseUnifiedItemData ItemData;
 *       if (DataManager->GetItemData(ItemID, ItemData))
 *       {
 *           // Use item data
 *       }
 *   }
 *
 * Blueprint:
 *   Use "Get SuspenseCore Data Manager" node
 *
 * @see USuspenseCoreSettings - Configuration source
 * @see USuspenseCoreEventManager - EventBus provider
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreDataManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//========================================================================
	// Static Access
	//========================================================================

	/**
	 * Get DataManager from world context
	 * @param WorldContextObject Any object with valid world context
	 * @return DataManager or nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data", meta = (WorldContext = "WorldContextObject", DisplayName = "Get SuspenseCore Data Manager"))
	static USuspenseCoreDataManager* Get(const UObject* WorldContextObject);

	//========================================================================
	// USubsystem Interface
	//========================================================================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	//========================================================================
	// Initialization Status
	//========================================================================

	/** Check if data manager is fully initialized and ready */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data")
	bool IsInitialized() const { return bIsInitialized; }

	/** Check if item system is ready */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data")
	bool IsItemSystemReady() const { return bItemSystemReady; }

	//========================================================================
	// Item Data Access
	//========================================================================

	/**
	 * Get unified item data by ID
	 * Broadcasts SuspenseCore.Event.Data.ItemLoaded or ItemNotFound
	 *
	 * @param ItemID The item identifier
	 * @param OutItemData Output item data structure
	 * @return true if item was found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Data")
	bool GetItemData(FName ItemID, FSuspenseUnifiedItemData& OutItemData) const;

	/**
	 * Check if item exists in database
	 * @param ItemID The item identifier
	 * @return true if item exists
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data")
	bool HasItem(FName ItemID) const;

	/**
	 * Get all cached item IDs
	 * @return Array of all item IDs in cache
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data")
	TArray<FName> GetAllItemIDs() const;

	/**
	 * Get count of cached items
	 * @return Number of items in cache
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data")
	int32 GetCachedItemCount() const { return ItemCache.Num(); }

	/**
	 * Create inventory item instance from item ID
	 * @param ItemID The item identifier
	 * @param Quantity Number of items
	 * @param OutInstance Output inventory instance
	 * @return true if instance created successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Data")
	bool CreateItemInstance(FName ItemID, int32 Quantity, FSuspenseInventoryItemInstance& OutInstance) const;

	//========================================================================
	// Item Validation
	//========================================================================

	/**
	 * Validate single item configuration
	 * @param ItemID Item to validate
	 * @param OutErrors Output validation errors
	 * @return true if item is valid
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Data")
	bool ValidateItem(FName ItemID, TArray<FString>& OutErrors) const;

	/**
	 * Validate all items in cache
	 * @param OutErrors Output validation errors
	 * @return Number of items with errors
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Data")
	int32 ValidateAllItems(TArray<FString>& OutErrors) const;

	//========================================================================
	// Character Data Access
	//========================================================================

	/**
	 * Get character classes data asset
	 * @return Character classes data asset or nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data")
	UDataAsset* GetCharacterClassesDataAsset() const;

	/**
	 * Get default character class tag
	 * @return Default character class gameplay tag
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data")
	FGameplayTag GetDefaultCharacterClass() const;

	//========================================================================
	// Loadout Data Access (Future)
	//========================================================================

	/**
	 * Get loadout DataTable
	 * @return Loadout configurations DataTable or nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data")
	UDataTable* GetLoadoutDataTable() const;

	/**
	 * Get default loadout ID
	 * @return Default loadout identifier
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data")
	FName GetDefaultLoadoutID() const;

	//========================================================================
	// EventBus Integration
	//========================================================================

	/**
	 * Get cached EventBus reference
	 * @return EventBus or nullptr
	 */
	USuspenseCoreEventBus* GetEventBus() const;

protected:
	//========================================================================
	// Initialization Helpers
	//========================================================================

	/** Load item data from settings */
	bool InitializeItemSystem();

	/** Build item cache from DataTable */
	bool BuildItemCache(UDataTable* DataTable);

	/** Load character data from settings */
	bool InitializeCharacterSystem();

	/** Load loadout data from settings */
	bool InitializeLoadoutSystem();

	//========================================================================
	// EventBus Broadcasting
	//========================================================================

	/** Broadcast initialization complete event */
	void BroadcastInitialized();

	/** Broadcast item loaded event */
	void BroadcastItemLoaded(FName ItemID, const FSuspenseUnifiedItemData& ItemData) const;

	/** Broadcast item not found event */
	void BroadcastItemNotFound(FName ItemID) const;

	/** Broadcast validation result */
	void BroadcastValidationResult(bool bPassed, const TArray<FString>& Errors) const;

private:
	//========================================================================
	// Cached Data
	//========================================================================

	/** Item data cache: ItemID -> UnifiedItemData */
	UPROPERTY()
	TMap<FName, FSuspenseUnifiedItemData> ItemCache;

	/** Loaded item DataTable reference */
	UPROPERTY()
	TObjectPtr<UDataTable> LoadedItemDataTable;

	/** Loaded character classes data asset */
	UPROPERTY()
	TObjectPtr<UDataAsset> LoadedCharacterClassesDataAsset;

	/** Loaded loadout DataTable reference */
	UPROPERTY()
	TObjectPtr<UDataTable> LoadedLoadoutDataTable;

	/** Cached EventBus reference */
	mutable TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	//========================================================================
	// Status Flags
	//========================================================================

	/** Overall initialization status */
	bool bIsInitialized = false;

	/** Item system ready flag */
	bool bItemSystemReady = false;

	/** Character system ready flag */
	bool bCharacterSystemReady = false;

	/** Loadout system ready flag */
	bool bLoadoutSystemReady = false;
};
