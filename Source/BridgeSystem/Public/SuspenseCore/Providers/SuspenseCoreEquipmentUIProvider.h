// SuspenseCoreEquipmentUIProvider.h
// SuspenseCore - UI Data Provider Component for Equipment System
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIDataProvider.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreLoadoutSettings.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUITypes.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUIContainerTypes.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryBaseTypes.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCoreEquipmentUIProvider.generated.h"

// Forward declarations
class USuspenseCoreEventBus;
class USuspenseCoreLoadoutManager;
class USuspenseCoreDataManager;

struct FSuspenseCoreEventData;

/**
 * USuspenseCoreEquipmentUIProvider
 *
 * ActorComponent that provides ISuspenseCoreUIDataProvider interface for Equipment UI.
 * Auto-discovered by UIManager::FindAllProvidersOnActor() when attached to PlayerState.
 *
 * PURPOSE:
 * - Bridges EquipmentSystem data layer with UISystem widgets
 * - Converts equipment data to UI-friendly formats
 * - Publishes UI update events when equipment changes
 * - Does NOT modify equipment state (read-only for UI)
 *
 * ARCHITECTURE:
 * - Created in PlayerState constructor (alongside EquipmentDataStore)
 * - Initialized in PlayerState::BeginPlay after equipment wiring
 * - Auto-discovered by UIManager when ShowContainerScreen is called
 * - Bound to EquipmentWidget via BindToProvider()
 *
 * @see ISuspenseCoreUIDataProvider
 * @see USuspenseCoreEquipmentWidget
 * @see ASuspenseCorePlayerState
 */
UCLASS(ClassGroup=(SuspenseCore), meta=(BlueprintSpawnableComponent))
class BRIDGESYSTEM_API USuspenseCoreEquipmentUIProvider : public UActorComponent, public ISuspenseCoreUIDataProvider
{
	GENERATED_BODY()

public:
	USuspenseCoreEquipmentUIProvider();

	//==================================================================
	// UActorComponent Interface
	//==================================================================

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//==================================================================
	// Initialization
	//==================================================================

	/**
	 * Initialize provider with loadout configuration.
	 * Called automatically in BeginPlay, but can be called manually for custom setup.
	 * @param InLoadoutID Loadout ID for slot configuration (uses owner's DefaultLoadoutID if empty)
	 * @return True if initialized successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Provider")
	bool InitializeProvider(FName InLoadoutID = NAME_None);

	/**
	 * Check if provider is initialized
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Provider")
	bool IsInitialized() const { return bIsInitialized; }

	/**
	 * Shutdown provider and clear cached data
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Provider")
	void Shutdown();

	//==================================================================
	// ISuspenseCoreUIDataProvider Interface - Identity
	//==================================================================

	virtual FGuid GetProviderID() const override { return ProviderID; }
	virtual ESuspenseCoreContainerType GetContainerType() const override { return ESuspenseCoreContainerType::Equipment; }
	virtual FGameplayTag GetContainerTypeTag() const override;
	virtual AActor* GetOwningActor() const override { return GetOwner(); }

	//==================================================================
	// ISuspenseCoreUIDataProvider Interface - Container Data
	//==================================================================

	virtual FSuspenseCoreContainerUIData GetContainerUIData() const override;
	virtual FIntPoint GetGridSize() const override;
	virtual int32 GetSlotCount() const override { return SlotConfigs.Num(); }

	//==================================================================
	// ISuspenseCoreUIDataProvider Interface - Slot Data
	//==================================================================

	virtual TArray<FSuspenseCoreSlotUIData> GetAllSlotUIData() const override;
	virtual FSuspenseCoreSlotUIData GetSlotUIData(int32 SlotIndex) const override;
	virtual bool IsSlotValid(int32 SlotIndex) const override;

	//==================================================================
	// ISuspenseCoreUIDataProvider Interface - Item Data
	//==================================================================

	virtual TArray<FSuspenseCoreItemUIData> GetAllItemUIData() const override;
	virtual bool GetItemUIDataAtSlot(int32 SlotIndex, FSuspenseCoreItemUIData& OutItem) const override;
	virtual bool FindItemUIData(const FGuid& InstanceID, FSuspenseCoreItemUIData& OutItem) const override;
	virtual int32 GetItemCount() const override;

	//==================================================================
	// ISuspenseCoreUIDataProvider Interface - Weight System
	//==================================================================

	virtual bool HasWeightLimit() const override { return false; }
	virtual float GetCurrentWeight() const override { return 0.0f; }
	virtual float GetMaxWeight() const override { return 0.0f; }

	//==================================================================
	// ISuspenseCoreUIDataProvider Interface - Validation
	//==================================================================

	virtual FSuspenseCoreDropValidation ValidateDrop(
		const FSuspenseCoreDragData& DragData,
		int32 TargetSlot,
		bool bRotated = false) const override;

	virtual bool CanAcceptItemType(const FGameplayTag& ItemType) const override;
	virtual int32 FindBestSlotForItem(FIntPoint ItemSize, bool bAllowRotation = true) const override;

	//==================================================================
	// ISuspenseCoreUIDataProvider Interface - Grid Position Calculations
	//==================================================================

	virtual int32 GetSlotAtLocalPosition(const FVector2D& LocalPos, float CellSize, float CellGap) const override;
	virtual TArray<int32> GetOccupiedSlotsForItem(const FGuid& ItemInstanceID) const override;
	virtual int32 GetAnchorSlotForPosition(int32 AnySlotIndex) const override;
	virtual bool CanPlaceItemAtSlot(const FGuid& ItemID, int32 SlotIndex, bool bRotated) const override;

	//==================================================================
	// ISuspenseCoreUIDataProvider Interface - Operations
	//==================================================================

	virtual bool RequestMoveItem(int32 FromSlot, int32 ToSlot, bool bRotate = false) override;
	virtual bool RequestRotateItem(int32 SlotIndex) override;
	virtual bool RequestUseItem(int32 SlotIndex) override;
	virtual bool RequestDropItem(int32 SlotIndex, int32 Quantity = 0) override;
	virtual bool RequestSplitStack(int32 SlotIndex, int32 SplitQuantity, int32 TargetSlot = INDEX_NONE) override;
	virtual bool RequestTransferItem(int32 SlotIndex, const FGuid& TargetProviderID, int32 TargetSlot = INDEX_NONE, int32 Quantity = 0) override;

	//==================================================================
	// ISuspenseCoreUIDataProvider Interface - Context Menu
	//==================================================================

	virtual TArray<FGameplayTag> GetItemContextActions(int32 SlotIndex) const override;
	virtual bool ExecuteContextAction(int32 SlotIndex, const FGameplayTag& ActionTag) override;

	//==================================================================
	// ISuspenseCoreUIDataProvider Interface - Delegates & EventBus
	//==================================================================

	virtual FOnSuspenseCoreUIDataChanged& OnUIDataChanged() override { return UIDataChangedDelegate; }
	virtual USuspenseCoreEventBus* GetEventBus() const override;

	//==================================================================
	// Equipment-Specific API
	//==================================================================

	/**
	 * Get slot data by equipment slot type
	 * @param SlotType Equipment slot type enum
	 * @return Slot UI data
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Provider")
	FSuspenseCoreSlotUIData GetSlotDataByType(EEquipmentSlotType SlotType) const;

	/**
	 * Get all equipment slot configurations
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Provider")
	TArray<FEquipmentSlotConfig> GetSlotConfigs() const { return SlotConfigs; }

	/**
	 * Force refresh all slot data
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Provider")
	void RefreshAllSlots();

protected:
	//==================================================================
	// Data Conversion
	//==================================================================

	/** Convert equipment slot state to UI slot data */
	virtual FSuspenseCoreSlotUIData ConvertToSlotUIData(EEquipmentSlotType SlotType, int32 SlotIndex) const;

	/** Convert equipped item to UI item data */
	virtual FSuspenseCoreItemUIData ConvertToItemUIData(const FGuid& ItemInstanceID) const;

	/** Get slot index for slot type */
	int32 GetSlotIndexForType(EEquipmentSlotType SlotType) const;

	/** Get slot type for slot index */
	EEquipmentSlotType GetSlotTypeForIndex(int32 SlotIndex) const;

	//==================================================================
	// State
	//==================================================================

	/** Unique provider ID */
	UPROPERTY()
	FGuid ProviderID;

	/** Loadout ID for configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
	FName LoadoutID;

	/** Cached slot configurations */
	UPROPERTY()
	TArray<FEquipmentSlotConfig> SlotConfigs;

	/** Is provider initialized */
	bool bIsInitialized;

	/** UI data changed delegate */
	FOnSuspenseCoreUIDataChanged UIDataChangedDelegate;

	/** Cached EventBus reference */
	mutable TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** Cached LoadoutManager reference */
	mutable TWeakObjectPtr<USuspenseCoreLoadoutManager> CachedLoadoutManager;

	/** Cached DataManager reference */
	mutable TWeakObjectPtr<USuspenseCoreDataManager> CachedDataManager;

	/**
	 * Cached equipped items - pushed via EventBus, not pulled from DataStore
	 * Key: SlotIndex, Value: Item instance data
	 * This avoids circular dependency: BridgeSystem <- EquipmentSystem -> BridgeSystem
	 */
	UPROPERTY()
	TMap<int32, FSuspenseCoreInventoryItemInstance> CachedEquippedItems;

	/** EventBus subscription handles */
	TArray<FSuspenseCoreSubscriptionHandle> EventSubscriptions;

private:
	/** Get LoadoutManager */
	USuspenseCoreLoadoutManager* GetLoadoutManager() const;

	/** Get DataManager for item lookups */
	USuspenseCoreDataManager* GetDataManager() const;

	/** Map slot type to index for quick lookup */
	TMap<EEquipmentSlotType, int32> SlotTypeToIndex;

	//==================================================================
	// EventBus Handlers (Push-based data sync)
	//==================================================================

	/** Setup EventBus subscriptions */
	void SetupEventSubscriptions();

	/** Teardown EventBus subscriptions */
	void TeardownEventSubscriptions();

	/** Handle item equipped event - add to cache */
	void OnItemEquipped(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle item unequipped event - remove from cache */
	void OnItemUnequipped(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle slot updated event - update slot state */
	void OnSlotUpdated(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
};
