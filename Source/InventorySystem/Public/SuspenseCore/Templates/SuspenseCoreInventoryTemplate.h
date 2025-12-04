// SuspenseCoreInventoryTemplate.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTemplateTypes.h"
#include "SuspenseCore/Types/Items/SuspenseCoreItemTypes.h"
#include "SuspenseCoreInventoryTemplate.generated.h"

// Forward declarations
class USuspenseCoreInventoryComponent;
class USuspenseCoreDataManager;
class UDataTable;

/**
 * USuspenseCoreInventoryTemplateManager
 *
 * Manages inventory templates and loadouts.
 * Applies templates to inventory components.
 *
 * ARCHITECTURE:
 * - Loads templates from DataTables
 * - Supports loadouts, loot tables, containers
 * - Integrates with USuspenseCoreDataManager
 *
 * USAGE:
 * TemplateManager->ApplyTemplate(Inventory, "DefaultLoadout");
 * TemplateManager->GenerateLoot(Inventory, "Tier3Loot");
 */
UCLASS(BlueprintType)
class INVENTORYSYSTEM_API USuspenseCoreInventoryTemplateManager : public UObject
{
	GENERATED_BODY()

public:
	USuspenseCoreInventoryTemplateManager();

	//==================================================================
	// Initialization
	//==================================================================

	/**
	 * Initialize with template DataTable.
	 * @param TemplateTable DataTable with FSuspenseCoreInventoryTemplate rows
	 * @param LoadoutTable Optional DataTable with FSuspenseCoreTemplateLoadout rows
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Templates")
	void Initialize(UDataTable* TemplateTable, UDataTable* LoadoutTable = nullptr);

	/**
	 * Set data manager for item creation.
	 * @param DataManager Data manager instance
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Templates")
	void SetDataManager(USuspenseCoreDataManager* DataManager);

	//==================================================================
	// Template Application
	//==================================================================

	/**
	 * Apply template to inventory.
	 * @param Inventory Target inventory
	 * @param TemplateID Template to apply
	 * @param bClearFirst Clear inventory before applying
	 * @return true if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Templates")
	bool ApplyTemplate(
		USuspenseCoreInventoryComponent* Inventory,
		FName TemplateID,
		bool bClearFirst = true
	);

	/**
	 * Apply template struct directly.
	 * @param Inventory Target inventory
	 * @param Template Template data
	 * @param bClearFirst Clear inventory before applying
	 * @return true if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Templates")
	bool ApplyTemplateStruct(
		USuspenseCoreInventoryComponent* Inventory,
		const FSuspenseCoreInventoryTemplate& Template,
		bool bClearFirst = true
	);

	/**
	 * Generate loot from loot table template.
	 * @param Inventory Target inventory
	 * @param LootTemplateID Loot table template
	 * @return Number of items generated
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Templates")
	int32 GenerateLoot(
		USuspenseCoreInventoryComponent* Inventory,
		FName LootTemplateID
	);

	//==================================================================
	// Loadout Management
	//==================================================================

	/**
	 * Apply loadout to character.
	 * @param Inventory Character's inventory
	 * @param LoadoutID Loadout to apply
	 * @return true if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Templates")
	bool ApplyLoadout(
		USuspenseCoreInventoryComponent* Inventory,
		FName LoadoutID
	);

	/**
	 * Get default loadout for character class.
	 * @param CharacterClass Character class tag
	 * @param OutLoadout Loadout data
	 * @return true if found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Templates")
	bool GetDefaultLoadout(
		FGameplayTag CharacterClass,
		FSuspenseCoreTemplateLoadout& OutLoadout
	) const;

	/**
	 * Get all loadouts for character class.
	 * @param CharacterClass Character class tag
	 * @return Array of matching loadouts
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Templates")
	TArray<FSuspenseCoreTemplateLoadout> GetLoadoutsForClass(FGameplayTag CharacterClass) const;

	//==================================================================
	// Template Query
	//==================================================================

	/**
	 * Get template by ID.
	 * @param TemplateID Template identifier
	 * @param OutTemplate Template data
	 * @return true if found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Templates")
	bool GetTemplate(FName TemplateID, FSuspenseCoreInventoryTemplate& OutTemplate) const;

	/**
	 * Get loadout by ID.
	 * @param LoadoutID Loadout identifier
	 * @param OutLoadout Loadout data
	 * @return true if found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Templates")
	bool GetLoadout(FName LoadoutID, FSuspenseCoreTemplateLoadout& OutLoadout) const;

	/**
	 * Get all templates of type.
	 * @param Type Template type
	 * @return Array of matching templates
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Templates")
	TArray<FSuspenseCoreInventoryTemplate> GetTemplatesByType(ESuspenseCoreTemplateType Type) const;

	/**
	 * Get templates by tag.
	 * @param Tag Tag to filter by
	 * @return Array of matching templates
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Templates")
	TArray<FSuspenseCoreInventoryTemplate> GetTemplatesByTag(FGameplayTag Tag) const;

	/**
	 * Get all template IDs.
	 * @return Array of template IDs
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Templates")
	TArray<FName> GetAllTemplateIDs() const;

	/**
	 * Check if template exists.
	 * @param TemplateID Template to check
	 * @return true if exists
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Templates")
	bool HasTemplate(FName TemplateID) const;

	//==================================================================
	// Item Generation
	//==================================================================

	/**
	 * Create item instance from template item.
	 * @param TemplateItem Template item definition
	 * @param OutInstance Created instance
	 * @return true if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Templates")
	bool CreateItemFromTemplate(
		const FSuspenseCoreTemplateItem& TemplateItem,
		FSuspenseCoreItemInstance& OutInstance
	);

	/**
	 * Roll loot items from template.
	 * @param Template Loot table template
	 * @param OutItems Generated items
	 * @return Number of items generated
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Templates")
	int32 RollLootItems(
		const FSuspenseCoreInventoryTemplate& Template,
		TArray<FSuspenseCoreItemInstance>& OutItems
	);

protected:
	/** Cached templates from DataTable */
	UPROPERTY()
	TMap<FName, FSuspenseCoreInventoryTemplate> CachedTemplates;

	/** Cached loadouts from DataTable */
	UPROPERTY()
	TMap<FName, FSuspenseCoreTemplateLoadout> CachedLoadouts;

	/** Data manager reference */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreDataManager> DataManagerRef;

	/** Template DataTable */
	UPROPERTY()
	TWeakObjectPtr<UDataTable> TemplateTableRef;

	/** Loadout DataTable */
	UPROPERTY()
	TWeakObjectPtr<UDataTable> LoadoutTableRef;

	/** Load templates from DataTable */
	void LoadTemplates();

	/** Load loadouts from DataTable */
	void LoadLoadouts();
};
