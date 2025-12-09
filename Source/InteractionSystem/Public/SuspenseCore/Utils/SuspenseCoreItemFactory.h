// SuspenseCoreItemFactory.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Interfaces/Interaction/ISuspenseCoreItemFactory.h"
#include "SuspenseCore/SuspenseCoreInterfaces.h"
#include "SuspenseCore/ItemSystem/SuspenseCoreItemManager.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreItemFactory.generated.h"

// Forward declarations
class USuspenseCoreEventBus;
class USuspenseCoreItemManager;
struct FSuspenseCoreUnifiedItemData;

/**
 * USuspenseCoreItemFactorySubsystem
 *
 * Item factory subsystem for creating pickup actors with EventBus integration.
 * Works with unified DataTable system and broadcasts creation events.
 *
 * EVENTBUS INTEGRATION:
 * - Implements ISuspenseCoreEventEmitter for event publishing
 * - Broadcasts SuspenseCore.Event.Factory.ItemCreated on successful spawn
 * - Broadcasts SuspenseCore.Event.Factory.SpawnFailed on failure
 * - Uses FSuspenseCoreEventData for typed payloads
 *
 * Note: Renamed from USuspenseCoreItemFactory to avoid conflict with USuspenseCoreItemFactory interface.
 *
 * Migrated from legacy USuspenseItemFactory
 */
UCLASS()
class INTERACTIONSYSTEM_API USuspenseCoreItemFactorySubsystem
	: public UGameInstanceSubsystem
	, public ISuspenseCoreItemFactory
	, public ISuspenseCoreEventEmitter
{
	GENERATED_BODY()

public:
	//==================================================================
	// USubsystem Interface
	//==================================================================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//==================================================================
	// ISuspenseCoreEventEmitter Interface
	//==================================================================

	/** Publish event through EventBus */
	virtual void EmitEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& Data) override;

	/** Get EventBus for publishing */
	virtual USuspenseCoreEventBus* GetEventBus() const override;

	//==================================================================
	// ISuspenseCoreItemFactory Implementation
	//==================================================================

	/**
	 * Create pickup from item ID.
	 * Broadcasts SuspenseCore.Event.Factory.ItemCreated on success.
	 * @param ItemID Item identifier from DataTable
	 * @param World World for spawning
	 * @param Transform Spawn transform
	 * @param Quantity Item amount
	 * @return Spawned pickup actor
	 */
	virtual AActor* CreatePickupFromItemID_Implementation(
		FName ItemID,
		UWorld* World,
		const FTransform& Transform,
		int32 Quantity
	) override;

	/**
	 * Get default pickup class.
	 * @return Default pickup actor class
	 */
	virtual TSubclassOf<AActor> GetDefaultPickupClass_Implementation() const override;

	//==================================================================
	// Extended Factory Methods
	//==================================================================

	/**
	 * Create pickup with custom properties.
	 * Supports weapon ammo state and preset runtime properties.
	 * @param ItemID Item identifier
	 * @param World World for spawning
	 * @param Transform Spawn transform
	 * @param Quantity Item amount
	 * @param bWithAmmoState Whether to include ammo state
	 * @param CurrentAmmo Current ammo count
	 * @param RemainingAmmo Remaining ammo count
	 * @return Spawned pickup actor
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Factory")
	AActor* CreatePickupWithAmmo(
		FName ItemID,
		UWorld* World,
		const FTransform& Transform,
		int32 Quantity,
		bool bWithAmmoState,
		float CurrentAmmo,
		float RemainingAmmo
	);

	/**
	 * Create pickup with preset properties.
	 * @param ItemID Item identifier
	 * @param World World for spawning
	 * @param Transform Spawn transform
	 * @param Quantity Item amount
	 * @param PresetProperties Map of preset properties to apply
	 * @return Spawned pickup actor
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Factory")
	AActor* CreatePickupWithProperties(
		FName ItemID,
		UWorld* World,
		const FTransform& Transform,
		int32 Quantity,
		const TMap<FName, float>& PresetProperties
	);

	/**
	 * Set default pickup class.
	 * @param NewDefaultClass New default class
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Factory")
	void SetDefaultPickupClass(TSubclassOf<AActor> NewDefaultClass);

	/**
	 * Get factory statistics.
	 * @return Number of pickups created this session
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Factory")
	int32 GetTotalPickupsCreated() const { return TotalPickupsCreated; }

protected:
	//==================================================================
	// Settings
	//==================================================================

	/** Default pickup actor class */
	UPROPERTY(EditDefaultsOnly, Category = "Factory Settings")
	TSubclassOf<AActor> DefaultPickupClass;

	//==================================================================
	// Cached References
	//==================================================================

	/** Cached EventBus reference */
	UPROPERTY(Transient)
	mutable TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** Cached ItemManager reference */
	UPROPERTY(Transient)
	TWeakObjectPtr<USuspenseCoreItemManager> CachedItemManager;

	//==================================================================
	// Statistics
	//==================================================================

	/** Total pickups created this session */
	int32 TotalPickupsCreated;

	//==================================================================
	// Internal Methods
	//==================================================================

	/** Get item manager subsystem */
	USuspenseCoreItemManager* GetItemManager() const;

	/**
	 * Configure spawned pickup with item data.
	 * @param PickupActor Actor to configure
	 * @param ItemData Item data from DataTable
	 * @param Quantity Item quantity
	 */
	void ConfigurePickup(
		AActor* PickupActor,
		const FSuspenseCoreUnifiedItemData& ItemData,
		int32 Quantity
	);

	/**
	 * Configure weapon pickup with ammo state.
	 * @param PickupActor Actor to configure
	 * @param ItemData Item data from DataTable
	 * @param bWithAmmoState Whether to set ammo state
	 * @param CurrentAmmo Current ammo in magazine
	 * @param RemainingAmmo Remaining ammo
	 */
	void ConfigureWeaponPickup(
		AActor* PickupActor,
		const FSuspenseCoreUnifiedItemData& ItemData,
		bool bWithAmmoState,
		float CurrentAmmo,
		float RemainingAmmo
	);

	/**
	 * Apply preset properties to pickup.
	 * @param PickupActor Actor to configure
	 * @param PresetProperties Properties to apply
	 */
	void ApplyPresetProperties(
		AActor* PickupActor,
		const TMap<FName, float>& PresetProperties
	);

	//==================================================================
	// EventBus Broadcasting
	//==================================================================

	/**
	 * Broadcast item creation event.
	 * @param CreatedActor Created pickup actor
	 * @param ItemID Item identifier
	 * @param Quantity Item quantity
	 * @param Transform Spawn transform
	 */
	void BroadcastItemCreated(
		AActor* CreatedActor,
		FName ItemID,
		int32 Quantity,
		const FTransform& Transform
	);

	/**
	 * Broadcast spawn failure event.
	 * @param ItemID Item identifier that failed to spawn
	 * @param Reason Failure reason
	 */
	void BroadcastSpawnFailed(
		FName ItemID,
		const FString& Reason
	);
};
