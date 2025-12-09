// SuspenseCorePickupItem.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SuspenseCore/Interfaces/Interaction/ISuspenseCoreInteractable.h"
#include "SuspenseCore/Interfaces/Interaction/ISuspenseCorePickup.h"
#include "SuspenseCore/Types/Items/SuspenseCoreItemTypes.h"
#include "SuspenseCore/SuspenseCoreInterfaces.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCorePickupItem.generated.h"

// Forward declarations
class USphereComponent;
class UStaticMeshComponent;
class USuspenseCoreDataManager;
class UDataTable;
class USuspenseCoreEventBus;
class UNiagaraComponent;
class UAudioComponent;
class APlayerController;

/**
 * FSuspenseCorePresetProperty
 *
 * Structure for storing key-value property pairs for replication.
 * Used instead of TMap for network replication support.
 */
USTRUCT(BlueprintType)
struct FSuspenseCorePresetProperty
{
	GENERATED_BODY()

	/** Property name (e.g., "Durability", "Ammo", "Charge") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
	FName PropertyName;

	/** Property value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
	float PropertyValue;

	FSuspenseCorePresetProperty()
		: PropertyName(NAME_None)
		, PropertyValue(0.0f)
	{
	}

	FSuspenseCorePresetProperty(FName InName, float InValue)
		: PropertyName(InName)
		, PropertyValue(InValue)
	{
	}

	bool operator==(const FSuspenseCorePresetProperty& Other) const
	{
		return PropertyName == Other.PropertyName;
	}
};

/**
 * ASuspenseCorePickupItem
 *
 * Base pickup actor class with EventBus integration.
 * Works exclusively with unified DataTable system.
 *
 * EVENTBUS INTEGRATION:
 * - Implements ISuspenseCoreEventEmitter for event publishing
 * - Broadcasts SuspenseCore.Event.Pickup.Spawned on BeginPlay
 * - Broadcasts SuspenseCore.Event.Pickup.Collected on successful pickup
 * - Uses FSuspenseCoreEventData for typed payloads
 *
 * ARCHITECTURE:
 * - Single source of truth: FSuspenseCoreItemData in DataTable
 * - ItemID is the only reference to static data
 * - Runtime state stored as FSuspenseCoreItemInstance
 * - Preset properties applied to created item instance
 * - Uses TArray for replication instead of TMap
 * - NO dependency on legacy types (FSuspenseCoreInventoryItemInstance)
 */
UCLASS(Blueprintable)
class INTERACTIONSYSTEM_API ASuspenseCorePickupItem
	: public AActor
	, public ISuspenseCoreInteractable
	, public ISuspenseCorePickup
	, public ISuspenseCoreEventEmitter
{
	GENERATED_BODY()

public:
	ASuspenseCorePickupItem();

	//==================================================================
	// AActor Interface
	//==================================================================

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;

	//==================================================================
	// ISuspenseCoreEventEmitter Interface
	//==================================================================

	virtual void EmitEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& Data) override;
	virtual USuspenseCoreEventBus* GetEventBus() const override;

	//==================================================================
	// ISuspenseCoreInteractable Interface
	//==================================================================

	virtual bool CanInteract_Implementation(APlayerController* InstigatingController) const override;
	virtual bool Interact_Implementation(APlayerController* InstigatingController) override;
	virtual FGameplayTag GetInteractionType_Implementation() const override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual int32 GetInteractionPriority_Implementation() const override;
	virtual float GetInteractionDistance_Implementation() const override;
	virtual void OnFocusGained_Implementation(APlayerController* InstigatingController) override;
	virtual void OnFocusLost_Implementation(APlayerController* InstigatingController) override;

	//==================================================================
	// ISuspenseCorePickup Interface
	//==================================================================

	virtual FName GetItemID_Implementation() const override;
	virtual void SetItemID_Implementation(FName NewItemID) override;
	virtual int32 GetQuantity_Implementation() const override;
	virtual void SetQuantity_Implementation(int32 NewQuantity) override;
	virtual bool CanPickup_Implementation(AActor* InstigatorActor) const override;
	virtual bool ExecutePickup_Implementation(AActor* InstigatorActor) override;
	virtual bool CreateInventoryInstance_Implementation(FSuspenseCoreItemInstance& OutInstance) const override;
	virtual bool HasAmmoState_Implementation() const override;
	virtual bool GetAmmoState_Implementation(float& OutCurrentAmmo, float& OutReserveAmmo) const override;
	virtual void SetAmmoState_Implementation(float CurrentAmmo, float ReserveAmmo) override;
	virtual FGameplayTag GetItemType_Implementation() const override;
	virtual FGameplayTag GetItemRarity_Implementation() const override;
	virtual FText GetDisplayName_Implementation() const override;
	virtual bool IsStackable_Implementation() const override;
	virtual float GetWeight_Implementation() const override;

	//==================================================================
	// Data Access
	//==================================================================

	/** Get item data from DataManager cache */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Pickup")
	bool GetItemData(FSuspenseCoreItemData& OutItemData) const;

	//==================================================================
	// Component Access
	//==================================================================

	/** Get mesh component */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Pickup")
	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }

	//==================================================================
	// Initialization
	//==================================================================

	/**
	 * Initialize pickup from complete runtime instance.
	 * Preserves all runtime properties like durability, modifications, etc.
	 * @param Instance Runtime item instance with all properties
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Pickup")
	void InitializeFromInstance(const FSuspenseCoreItemInstance& Instance);

	/**
	 * Initialize pickup from ItemID and quantity.
	 * @param InItemID Item identifier
	 * @param InQuantity Item quantity
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Pickup")
	void InitializeFromItemID(FName InItemID, int32 InQuantity = 1);

	/**
	 * Set ammo state for weapon pickups (internal use).
	 * @param bHasState Whether ammo state is set
	 * @param CurrentAmmo Ammo in magazine
	 * @param RemainingAmmo Reserve ammo
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Pickup|Weapon")
	void SetPickupAmmoState(bool bHasState, float CurrentAmmo, float RemainingAmmo);

	//==================================================================
	// Preset Property Management
	//==================================================================

	/**
	 * Get preset property value by name.
	 * @param PropertyName Property name to find
	 * @param DefaultValue Default value if not found
	 * @return Property value or DefaultValue
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Pickup|Preset")
	float GetPresetProperty(FName PropertyName, float DefaultValue = 0.0f) const;

	/**
	 * Set preset property value.
	 * @param PropertyName Property name
	 * @param Value New value
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "SuspenseCore|Pickup|Preset")
	void SetPresetProperty(FName PropertyName, float Value);

	/**
	 * Check if preset property exists.
	 * @param PropertyName Property name to check
	 * @return true if property exists
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Pickup|Preset")
	bool HasPresetProperty(FName PropertyName) const;

	/**
	 * Remove preset property.
	 * @param PropertyName Property name to remove
	 * @return true if property was removed
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "SuspenseCore|Pickup|Preset")
	bool RemovePresetProperty(FName PropertyName);

	/**
	 * Get all preset properties as TMap.
	 * @return Map with all properties
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Pickup|Preset")
	TMap<FName, float> GetPresetPropertiesAsMap() const;

	/**
	 * Set preset properties from TMap.
	 * @param NewProperties Map with new properties
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "SuspenseCore|Pickup|Preset")
	void SetPresetPropertiesFromMap(const TMap<FName, float>& NewProperties);

protected:
	//==================================================================
	// Components
	//==================================================================

	/** Interaction collision sphere */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* SphereCollision;

	/** Visual mesh component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent;

	/** VFX component for spawn effect */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UNiagaraComponent* SpawnVFXComponent;

	/** Audio component for ambient sounds */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAudioComponent* AudioComponent;

	//==================================================================
	// Item Reference - Single Source of Truth
	//==================================================================

	/** Item identifier for DataTable lookup */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Pickup|Item")
	FName ItemID;

	/** Quantity of items in this pickup */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Pickup|Item",
		meta = (ClampMin = "1"))
	int32 Amount;

	//==================================================================
	// Runtime State
	//==================================================================

	/**
	 * Complete runtime instance data.
	 * Used when pickup represents dropped equipped item with full state.
	 * NOTE: RuntimeProperties replicated via PresetRuntimeProperties (TArray).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Pickup|Runtime")
	FSuspenseCoreItemInstance RuntimeInstance;

	/**
	 * Whether this pickup uses full runtime instance.
	 * If true, RuntimeInstance is used instead of simple ItemID/Amount.
	 */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Pickup|Runtime")
	bool bUseRuntimeInstance;

	/**
	 * Preset properties for applying to created item.
	 * Uses TArray for replication support.
	 */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Pickup|Preset",
		meta = (TitleProperty = "PropertyName"))
	TArray<FSuspenseCorePresetProperty> PresetRuntimeProperties;

	//==================================================================
	// Weapon State
	//==================================================================

	/** Whether this pickup has saved ammo state */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Pickup|Weapon")
	bool bHasSavedAmmoState;

	/** Current ammo in magazine */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Pickup|Weapon",
		meta = (EditCondition = "bHasSavedAmmoState"))
	float SavedCurrentAmmo;

	/** Remaining ammo */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Pickup|Weapon",
		meta = (EditCondition = "bHasSavedAmmoState"))
	float SavedRemainingAmmo;

	//==================================================================
	// Interaction Settings
	//==================================================================

	/** Delay before destroying actor after pickup */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Pickup|Settings")
	float DestroyDelay;

	/** Interaction priority for overlapping pickups */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Pickup|Settings")
	int32 InteractionPriority;

	/** Custom interaction distance (0 = use default) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Pickup|Settings")
	float InteractionDistanceOverride;

	//==================================================================
	// Runtime Cache
	//==================================================================

	/** Cached item data from DataManager */
	UPROPERTY(Transient)
	mutable FSuspenseCoreItemData CachedItemData;

	/** Whether item data has been loaded */
	UPROPERTY(Transient)
	mutable bool bDataCached;

	/** Cached EventBus reference */
	mutable TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	//==================================================================
	// Event Handlers
	//==================================================================

	/**
	 * Called when pickup is successfully collected.
	 * @param InstigatorActor Actor that collected the pickup
	 * @return true if handled successfully
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|Pickup")
	bool OnPickedUp(AActor* InstigatorActor);
	virtual bool OnPickedUp_Implementation(AActor* InstigatorActor);

	//==================================================================
	// Data Management
	//==================================================================

	/**
	 * Load item data from DataTable through ItemManager.
	 * @return true if data was loaded successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Pickup")
	bool LoadItemData() const;

	/** Apply visual properties from item data */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Pickup|Visuals")
	virtual void ApplyItemVisuals();

	/** Apply audio properties from item data */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Pickup|Audio")
	virtual void ApplyItemAudio();

	/** Apply VFX from item data */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Pickup|VFX")
	virtual void ApplyItemVFX();

	//==================================================================
	// Blueprint Events
	//==================================================================

	/** Blueprint event for custom visual setup */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Pickup|Visuals")
	void OnVisualsApplied();

	/** Blueprint event for weapon-specific setup */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Pickup|Weapon")
	void OnWeaponPickupSetup();

	/** Blueprint event for armor-specific setup */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Pickup|Armor")
	void OnArmorPickupSetup();

	//==================================================================
	// Utility Methods
	//==================================================================

	/**
	 * Try to add item to actor's inventory.
	 * @param InstigatorActor Actor to receive the item
	 * @return true if item was added
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Pickup")
	bool TryAddToInventory(AActor* InstigatorActor);

	/**
	 * Get data manager subsystem.
	 * @return DataManager or nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Pickup")
	USuspenseCoreDataManager* GetDataManager() const;

	/** Handle visual feedback for interaction focus */
	virtual void HandleInteractionFeedback(bool bGainedFocus);

	//==================================================================
	// EventBus Broadcasting
	//==================================================================

	/**
	 * Broadcast interaction started event through EventBus.
	 * @param InstigatingController Controller that started interaction
	 */
	void BroadcastInteractionStarted(APlayerController* InstigatingController);

	/**
	 * Broadcast interaction completed event through EventBus.
	 * @param InstigatingController Controller that completed interaction
	 * @param bSuccess Whether interaction was successful
	 */
	void BroadcastInteractionCompleted(APlayerController* InstigatingController, bool bSuccess);

	/**
	 * Broadcast focus changed event through EventBus.
	 * @param InstigatingController Controller whose focus changed
	 * @param bGainedFocus True if focus gained, false if lost
	 */
	void BroadcastFocusChanged(APlayerController* InstigatingController, bool bGainedFocus);

	/** Broadcast pickup spawned event */
	void BroadcastPickupSpawned();

	/**
	 * Broadcast pickup collected event.
	 * @param Collector Actor that collected the pickup
	 */
	void BroadcastPickupCollected(AActor* Collector);

private:
	//==================================================================
	// Internal Helpers
	//==================================================================

	/** Find preset property in array */
	FSuspenseCorePresetProperty* FindPresetProperty(FName PropertyName);
	const FSuspenseCorePresetProperty* FindPresetProperty(FName PropertyName) const;
};
