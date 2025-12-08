// ISuspenseCorePickup.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "ISuspenseCorePickup.generated.h"

// Forward declarations
class AActor;
struct FSuspenseCoreItemInstance;

/**
 * USuspenseCorePickup
 *
 * UInterface definition for SuspenseCore pickup items.
 *
 * IMPORTANT: This is a SuspenseCore interface.
 * DO NOT use legacy ISuspensePickup in new SuspenseCore code!
 */
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCorePickup : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCorePickup
 *
 * Interface for pickup actors in SuspenseCore architecture.
 * Works with unified DataTable system through ItemID references.
 *
 * ARCHITECTURE PRINCIPLES:
 * - Single source of truth: ItemID references DataTable
 * - No legacy FSuspenseUnifiedItemData dependency in interface
 * - Runtime state (amount, ammo) managed separately
 * - Events broadcast through EventBus
 *
 * DATA FLOW:
 * 1. Pickup stores ItemID (reference to DataTable row)
 * 2. Runtime properties stored in PresetProperties
 * 3. On pickup, creates FSuspenseCoreItemInstance for inventory
 * 4. Item data loaded on-demand through DataManager
 *
 * MIGRATION FROM LEGACY:
 * - Replace ISuspensePickup with ISuspenseCorePickup
 * - Remove GetUnifiedItemData() - use DataManager::GetItemData(ItemID)
 * - Use FSuspenseCoreItemInstance instead of FSuspenseInventoryItemInstance
 * - Use EventBus for pickup events
 *
 * @see ISuspensePickup (Legacy - DO NOT USE in new code)
 * @see USuspenseCoreDataManager
 */
class BRIDGESYSTEM_API ISuspenseCorePickup
{
	GENERATED_BODY()

public:
	//==================================================================
	// Item Identity - DataTable Reference
	//==================================================================

	/**
	 * Get item identifier for DataTable lookup.
	 * This is the primary link to static item data.
	 * @return ItemID from DataTable row name
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Pickup")
	FName GetItemID() const;

	/**
	 * Set item identifier.
	 * @param NewItemID New ItemID to set
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Pickup")
	void SetItemID(FName NewItemID);

	//==================================================================
	// Quantity Management
	//==================================================================

	/**
	 * Get current item quantity in this pickup.
	 * @return Number of items
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Pickup")
	int32 GetQuantity() const;

	/**
	 * Set item quantity.
	 * @param NewQuantity New amount to set (clamped to valid range)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Pickup")
	void SetQuantity(int32 NewQuantity);

	//==================================================================
	// Pickup Behavior
	//==================================================================

	/**
	 * Check if actor can pick up this item.
	 * Validates inventory space, weight, restrictions, etc.
	 * @param InstigatorActor Actor attempting pickup
	 * @return true if pickup is allowed
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Pickup")
	bool CanPickup(AActor* InstigatorActor) const;

	/**
	 * Execute pickup logic.
	 * Should add item to inventory and destroy/disable pickup actor.
	 * Broadcasts pickup events through EventBus.
	 * @param InstigatorActor Actor performing pickup
	 * @return true if pickup was successful
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Pickup")
	bool ExecutePickup(AActor* InstigatorActor);

	//==================================================================
	// Item Instance Creation
	//==================================================================

	/**
	 * Create runtime item instance for inventory system.
	 * Combines ItemID reference with runtime state.
	 * @param OutInstance Output SuspenseCore item instance
	 * @return true if instance created successfully
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Pickup")
	bool CreateInventoryInstance(FSuspenseCoreItemInstance& OutInstance) const;

	//==================================================================
	// Weapon State (Optional)
	//==================================================================

	/**
	 * Check if pickup has preserved weapon ammo state.
	 * Used for dropped weapons to maintain ammo count.
	 * @return true if ammo state is stored
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Pickup|Weapon")
	bool HasAmmoState() const;

	/**
	 * Get preserved ammo state.
	 * @param OutCurrentAmmo Ammo in magazine
	 * @param OutReserveAmmo Reserve ammunition
	 * @return true if ammo state was retrieved
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Pickup|Weapon")
	bool GetAmmoState(float& OutCurrentAmmo, float& OutReserveAmmo) const;

	/**
	 * Set ammo state for weapon pickup.
	 * @param CurrentAmmo Ammo in magazine
	 * @param ReserveAmmo Reserve ammunition
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Pickup|Weapon")
	void SetAmmoState(float CurrentAmmo, float ReserveAmmo);

	//==================================================================
	// Quick Access Properties (Cached from DataTable)
	//==================================================================

	/**
	 * Get item type tag.
	 * @return GameplayTag for item type classification
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Pickup|Info")
	FGameplayTag GetItemType() const;

	/**
	 * Get item rarity tag.
	 * @return GameplayTag for item rarity
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Pickup|Info")
	FGameplayTag GetItemRarity() const;

	/**
	 * Get localized display name.
	 * @return Item display name for UI
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Pickup|Info")
	FText GetDisplayName() const;

	/**
	 * Check if item can be stacked.
	 * @return true if MaxStackSize > 1
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Pickup|Info")
	bool IsStackable() const;

	/**
	 * Get item weight per unit.
	 * @return Weight value from DataTable
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Pickup|Info")
	float GetWeight() const;
};

