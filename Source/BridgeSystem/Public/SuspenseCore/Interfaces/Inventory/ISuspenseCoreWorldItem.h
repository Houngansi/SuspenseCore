// ISuspenseCoreWorldItem.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "ISuspenseCoreWorldItem.generated.h"

// Forward declarations
struct FSuspenseCoreItemData;
struct FSuspenseCoreItemInstance;
class USuspenseCoreEventBus;
class AActor;

/**
 * USuspenseCoreWorldItem
 *
 * UInterface for SuspenseCore world item representation.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreWorldItem : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreWorldItem
 *
 * Interface for actors/objects that represent inventory items in the world.
 * Used for world pickups, equipped items, and dropped items.
 *
 * ARCHITECTURE:
 * - Wraps FSuspenseCoreItemInstance for world representation
 * - Integrates with USuspenseCoreEventBus for notifications
 * - Works with USuspenseCoreDataManager for item data
 *
 * USAGE:
 * Implement on:
 * - Pickup actors (ASuspenseCorePickup)
 * - Equipped weapon actors
 * - Dropped item actors
 * - Container actors
 */
class BRIDGESYSTEM_API ISuspenseCoreWorldItem
{
	GENERATED_BODY()

public:
	//==================================================================
	// Item Instance
	//==================================================================

	/**
	 * Get the item instance data.
	 * @return Reference to item instance
	 */
	virtual const FSuspenseCoreItemInstance& GetItemInstance() const = 0;

	/**
	 * Get mutable item instance.
	 * @return Mutable reference to item instance
	 */
	virtual FSuspenseCoreItemInstance& GetMutableItemInstance() = 0;

	/**
	 * Set the item instance.
	 * @param NewInstance New instance data
	 */
	virtual void SetItemInstance(const FSuspenseCoreItemInstance& NewInstance) = 0;

	/**
	 * Initialize from item ID.
	 * Creates new instance with given quantity.
	 * @param ItemID DataTable row name
	 * @param Quantity Initial quantity
	 * @return true if successful
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem")
	bool InitializeFromItemID(FName ItemID, int32 Quantity);

	//==================================================================
	// Item Data
	//==================================================================

	/**
	 * Get item ID.
	 * @return Item DataTable row name
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem")
	FName GetItemID() const;

	/**
	 * Get unique instance ID.
	 * @return Unique runtime instance ID
	 */
	virtual FGuid GetInstanceID() const = 0;

	/**
	 * Get current quantity.
	 * @return Stack quantity
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem")
	int32 GetQuantity() const;

	/**
	 * Set quantity.
	 * @param NewQuantity New quantity
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem")
	void SetQuantity(int32 NewQuantity);

	/**
	 * Get item type tag.
	 * @return Primary item type GameplayTag
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem")
	FGameplayTag GetItemType() const;

	/**
	 * Check if item has tag.
	 * @param Tag Tag to check
	 * @return true if has tag
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem")
	bool HasItemTag(FGameplayTag Tag) const;

	//==================================================================
	// Runtime Properties
	//==================================================================

	/**
	 * Get runtime property value.
	 * @param PropertyName Name of property
	 * @param DefaultValue Value if not found
	 * @return Property value
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Properties")
	float GetProperty(FName PropertyName, float DefaultValue) const;

	/**
	 * Set runtime property value.
	 * @param PropertyName Name of property
	 * @param Value New value
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Properties")
	void SetProperty(FName PropertyName, float Value);

	/**
	 * Check if has property.
	 * @param PropertyName Name of property
	 * @return true if has property
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Properties")
	bool HasProperty(FName PropertyName) const;

	/**
	 * Get current durability (0-1).
	 * @return Durability ratio
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Properties")
	float GetDurability() const;

	/**
	 * Set current durability.
	 * @param NewDurability New durability (0-1)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Properties")
	void SetDurability(float NewDurability);

	//==================================================================
	// Weapon State
	//==================================================================

	/**
	 * Check if item is a weapon.
	 * @return true if weapon
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Weapon")
	bool IsWeapon() const;

	/**
	 * Get current ammo.
	 * @return Current magazine ammo
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Weapon")
	int32 GetCurrentAmmo() const;

	/**
	 * Get reserve ammo.
	 * @return Reserve ammo count
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Weapon")
	int32 GetReserveAmmo() const;

	/**
	 * Set ammo state.
	 * @param CurrentAmmo Current magazine
	 * @param ReserveAmmo Reserve pool
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Weapon")
	void SetAmmoState(int32 CurrentAmmo, int32 ReserveAmmo);

	//==================================================================
	// Behavior
	//==================================================================

	/**
	 * Check if item can stack with another.
	 * @param Other Other item interface
	 * @return true if can stack
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Behavior")
	bool CanStackWith(const TScriptInterface<ISuspenseCoreWorldItem>& Other) const;

	/**
	 * Check if item is stackable.
	 * @return true if max stack > 1
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Behavior")
	bool IsStackable() const;

	/**
	 * Get max stack size.
	 * @return Maximum quantity per stack
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Behavior")
	int32 GetMaxStackSize() const;

	/**
	 * Get item weight.
	 * @return Weight per unit
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Behavior")
	float GetWeight() const;

	/**
	 * Get total weight (weight * quantity).
	 * @return Total stack weight
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Behavior")
	float GetTotalWeight() const;

	/**
	 * Check if can be dropped.
	 * @return true if droppable
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Behavior")
	bool CanDrop() const;

	/**
	 * Check if can be traded.
	 * @return true if tradeable
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Behavior")
	bool CanTrade() const;

	/**
	 * Check if is quest item.
	 * @return true if quest item
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Behavior")
	bool IsQuestItem() const;

	//==================================================================
	// Actions
	//==================================================================

	/**
	 * Use/consume the item.
	 * @param User Actor using item
	 * @return true if successfully used
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Actions")
	bool UseItem(AActor* User);

	/**
	 * Drop item to world.
	 * @param DropLocation World location
	 * @param DropRotation World rotation
	 * @return Spawned pickup actor
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Actions")
	AActor* DropItem(FVector DropLocation, FRotator DropRotation);

	/**
	 * Split stack.
	 * @param SplitQuantity Amount to split off
	 * @return New item with split quantity (null if failed)
	 */
	virtual TScriptInterface<ISuspenseCoreWorldItem> SplitStack(int32 SplitQuantity) = 0;

	/**
	 * Merge with another stack.
	 * @param Other Item to merge from
	 * @param MaxMerge Maximum to merge (-1 for all)
	 * @return Quantity actually merged
	 */
	virtual int32 MergeWith(TScriptInterface<ISuspenseCoreWorldItem>& Other, int32 MaxMerge = -1) = 0;

	//==================================================================
	// Grid Inventory
	//==================================================================

	/**
	 * Get grid size.
	 * @return Item size in grid cells (width, height)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Grid")
	FIntPoint GetGridSize() const;

	/**
	 * Get current rotation.
	 * @return Rotation in degrees (0, 90, 180, 270)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Grid")
	int32 GetRotation() const;

	/**
	 * Set rotation.
	 * @param NewRotation Rotation in degrees
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Grid")
	void SetRotation(int32 NewRotation);

	/**
	 * Get effective grid size with rotation.
	 * @return Size after rotation applied
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|WorldItem|Grid")
	FIntPoint GetRotatedGridSize() const;

	//==================================================================
	// EventBus Integration
	//==================================================================

	/**
	 * Get EventBus for item events.
	 * @return EventBus instance
	 */
	virtual USuspenseCoreEventBus* GetItemEventBus() const = 0;

	/**
	 * Broadcast item modified event.
	 */
	virtual void BroadcastItemModified() = 0;

	//==================================================================
	// Debug
	//==================================================================

	/**
	 * Get debug string.
	 * @return Debug info
	 */
	virtual FString GetDebugString() const = 0;
};

