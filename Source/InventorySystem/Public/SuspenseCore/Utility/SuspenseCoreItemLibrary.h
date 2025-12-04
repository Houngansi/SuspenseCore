// SuspenseCoreItemLibrary.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/Items/SuspenseCoreItemTypes.h"
#include "SuspenseCoreItemLibrary.generated.h"

// Forward declarations
class USuspenseCoreDataManager;

/**
 * USuspenseCoreItemLibrary
 *
 * Blueprint function library for item operations.
 * Provides utilities for working with FSuspenseCoreItemData and FSuspenseCoreItemInstance.
 */
UCLASS()
class INVENTORYSYSTEM_API USuspenseCoreItemLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//==================================================================
	// Item Instance Creation
	//==================================================================

	/**
	 * Create item instance from ID.
	 * @param WorldContextObject World context
	 * @param ItemID DataTable row name
	 * @param Quantity Initial quantity
	 * @param OutInstance Created instance
	 * @return true if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Items",
		meta = (WorldContext = "WorldContextObject"))
	static bool CreateItemInstance(
		UObject* WorldContextObject,
		FName ItemID,
		int32 Quantity,
		FSuspenseCoreItemInstance& OutInstance
	);

	/**
	 * Create item instance with custom properties.
	 * @param ItemID DataTable row name
	 * @param Quantity Initial quantity
	 * @param Properties Initial properties
	 * @return Created instance
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Items")
	static FSuspenseCoreItemInstance MakeItemInstance(
		FName ItemID,
		int32 Quantity,
		const TMap<FName, float>& Properties
	);

	/**
	 * Clone item instance.
	 * @param Source Instance to clone
	 * @param bNewInstanceID Generate new unique ID
	 * @return Cloned instance
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Items")
	static FSuspenseCoreItemInstance CloneItemInstance(
		const FSuspenseCoreItemInstance& Source,
		bool bNewInstanceID = true
	);

	//==================================================================
	// Item Data Query
	//==================================================================

	/**
	 * Get item data from DataManager.
	 * @param WorldContextObject World context
	 * @param ItemID Item to lookup
	 * @param OutData Item data
	 * @return true if found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Items",
		meta = (WorldContext = "WorldContextObject"))
	static bool GetItemData(
		UObject* WorldContextObject,
		FName ItemID,
		FSuspenseCoreItemData& OutData
	);

	/**
	 * Check if item exists in DataManager.
	 * @param WorldContextObject World context
	 * @param ItemID Item to check
	 * @return true if exists
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Items",
		meta = (WorldContext = "WorldContextObject"))
	static bool ItemExists(UObject* WorldContextObject, FName ItemID);

	/**
	 * Get all items of type.
	 * @param WorldContextObject World context
	 * @param ItemType Type tag to filter
	 * @param OutItemIDs Matching item IDs
	 * @return Number of items found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Items",
		meta = (WorldContext = "WorldContextObject"))
	static int32 GetItemsOfType(
		UObject* WorldContextObject,
		FGameplayTag ItemType,
		TArray<FName>& OutItemIDs
	);

	//==================================================================
	// Item Instance Utilities
	//==================================================================

	/**
	 * Check if instance is valid.
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Items")
	static bool IsInstanceValid(const FSuspenseCoreItemInstance& Instance);

	/**
	 * Check if instances are same item type.
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Items")
	static bool AreSameItemType(
		const FSuspenseCoreItemInstance& A,
		const FSuspenseCoreItemInstance& B
	);

	/**
	 * Check if instances can stack.
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Items")
	static bool CanInstancesStack(
		const FSuspenseCoreItemInstance& A,
		const FSuspenseCoreItemInstance& B
	);

	/**
	 * Get instance property.
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Items")
	static float GetInstanceProperty(
		const FSuspenseCoreItemInstance& Instance,
		FName PropertyName,
		float DefaultValue = 0.0f
	);

	/**
	 * Set instance property.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Items")
	static void SetInstanceProperty(
		UPARAM(ref) FSuspenseCoreItemInstance& Instance,
		FName PropertyName,
		float Value
	);

	/**
	 * Get all properties as map.
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Items")
	static TMap<FName, float> GetAllProperties(const FSuspenseCoreItemInstance& Instance);

	//==================================================================
	// Weapon Utilities
	//==================================================================

	/**
	 * Check if instance is weapon.
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Items|Weapon")
	static bool IsWeaponInstance(const FSuspenseCoreItemInstance& Instance);

	/**
	 * Get weapon ammo state.
	 * @param Instance Weapon instance
	 * @param OutCurrentAmmo Current magazine ammo
	 * @param OutReserveAmmo Reserve ammo
	 * @return true if has weapon state
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Items|Weapon")
	static bool GetWeaponAmmo(
		const FSuspenseCoreItemInstance& Instance,
		int32& OutCurrentAmmo,
		int32& OutReserveAmmo
	);

	/**
	 * Set weapon ammo state.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Items|Weapon")
	static void SetWeaponAmmo(
		UPARAM(ref) FSuspenseCoreItemInstance& Instance,
		int32 CurrentAmmo,
		int32 ReserveAmmo
	);

	/**
	 * Reload weapon.
	 * @param Instance Weapon instance
	 * @param MagazineSize Magazine capacity
	 * @return Ammo consumed from reserve
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Items|Weapon")
	static int32 ReloadWeapon(
		UPARAM(ref) FSuspenseCoreItemInstance& Instance,
		int32 MagazineSize
	);

	//==================================================================
	// Durability Utilities
	//==================================================================

	/**
	 * Get durability (0-100).
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Items|Durability")
	static float GetDurability(const FSuspenseCoreItemInstance& Instance);

	/**
	 * Set durability (0-100).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Items|Durability")
	static void SetDurability(
		UPARAM(ref) FSuspenseCoreItemInstance& Instance,
		float Durability
	);

	/**
	 * Apply durability damage.
	 * @param Instance Item instance
	 * @param Damage Damage amount
	 * @return true if item broke (durability <= 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Items|Durability")
	static bool ApplyDurabilityDamage(
		UPARAM(ref) FSuspenseCoreItemInstance& Instance,
		float Damage
	);

	/**
	 * Repair item.
	 * @param Instance Item instance
	 * @param Amount Repair amount
	 * @return New durability
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Items|Durability")
	static float RepairItem(
		UPARAM(ref) FSuspenseCoreItemInstance& Instance,
		float Amount
	);

	/**
	 * Check if item is broken.
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Items|Durability")
	static bool IsBroken(const FSuspenseCoreItemInstance& Instance);

	//==================================================================
	// Grid Utilities
	//==================================================================

	/**
	 * Get grid size from item data.
	 * @param WorldContextObject World context
	 * @param ItemID Item to lookup
	 * @return Grid size (1,1) if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Items|Grid",
		meta = (WorldContext = "WorldContextObject"))
	static FIntPoint GetItemGridSize(UObject* WorldContextObject, FName ItemID);

	/**
	 * Get rotated grid size for instance.
	 * @param WorldContextObject World context
	 * @param Instance Item instance
	 * @return Rotated grid size
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Items|Grid",
		meta = (WorldContext = "WorldContextObject"))
	static FIntPoint GetRotatedGridSize(
		UObject* WorldContextObject,
		const FSuspenseCoreItemInstance& Instance
	);

	/**
	 * Rotate instance by 90 degrees.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Items|Grid")
	static void RotateInstance(UPARAM(ref) FSuspenseCoreItemInstance& Instance);

	//==================================================================
	// Value Utilities
	//==================================================================

	/**
	 * Calculate item value.
	 * @param WorldContextObject World context
	 * @param Instance Item instance
	 * @param bIncludeDurability Factor in durability
	 * @return Calculated value
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Items|Value",
		meta = (WorldContext = "WorldContextObject"))
	static int32 CalculateItemValue(
		UObject* WorldContextObject,
		const FSuspenseCoreItemInstance& Instance,
		bool bIncludeDurability = true
	);

	/**
	 * Calculate stack value.
	 * @param WorldContextObject World context
	 * @param Instance Item instance
	 * @return Total value (unit value * quantity)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Items|Value",
		meta = (WorldContext = "WorldContextObject"))
	static int32 CalculateStackValue(
		UObject* WorldContextObject,
		const FSuspenseCoreItemInstance& Instance
	);

	//==================================================================
	// Debug Utilities
	//==================================================================

	/**
	 * Get debug string for instance.
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Items|Debug")
	static FString GetInstanceDebugString(const FSuspenseCoreItemInstance& Instance);

	/**
	 * Get debug string for item data.
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Items|Debug")
	static FString GetItemDataDebugString(const FSuspenseCoreItemData& ItemData);
};
