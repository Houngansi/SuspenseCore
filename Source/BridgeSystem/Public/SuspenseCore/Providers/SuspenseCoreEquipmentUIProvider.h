// SuspenseCoreEquipmentUIProvider.h
// SuspenseCore - UI Data Provider Adapter for Equipment System
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIDataProvider.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreLoadoutSettings.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUITypes.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUIContainerTypes.h"
#include "SuspenseCoreEquipmentUIProvider.generated.h"

// Forward declarations
class USuspenseCoreEquipmentDataStore;
class USuspenseCoreEventBus;
class USuspenseCoreLoadoutManager;

/**
 * USuspenseCoreEquipmentUIProvider
 *
 * Adapter that wraps USuspenseCoreEquipmentDataStore to provide
 * ISuspenseCoreUIDataProvider interface for UI widgets.
 *
 * PURPOSE:
 * - Bridges EquipmentSystem data layer with UISystem widgets
 * - Converts equipment data to UI-friendly formats
 * - Publishes UI update events when equipment changes
 * - Does NOT modify equipment state (read-only for UI)
 *
 * USAGE:
 * 1. Create instance and call Initialize() with EquipmentDataStore
 * 2. Bind USuspenseCoreEquipmentWidget to this provider
 * 3. Provider broadcasts OnUIDataChanged when equipment changes
 * 4. Widget refreshes automatically
 *
 * @see ISuspenseCoreUIDataProvider
 * @see USuspenseCoreEquipmentDataStore
 * @see USuspenseCoreEquipmentWidget
 */
UCLASS(BlueprintType, Blueprintable)
class BRIDGESYSTEM_API USuspenseCoreEquipmentUIProvider : public UObject, public ISuspenseCoreUIDataProvider
{
	GENERATED_BODY()

public:
	USuspenseCoreEquipmentUIProvider();

	//==================================================================
	// Initialization
	//==================================================================

	/**
	 * Initialize provider with equipment data store
	 * @param InDataStore Equipment data store component
	 * @param InLoadoutID Loadout ID for slot configuration
	 * @return True if initialized successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Provider")
	bool Initialize(UActorComponent* InDataStore, FName InLoadoutID = NAME_None);

	/**
	 * Check if provider is initialized
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Provider")
	bool IsInitialized() const { return bIsInitialized; }

	/**
	 * Shutdown provider and unbind from data store
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Provider")
	void Shutdown();

	//==================================================================
	// ISuspenseCoreUIDataProvider Interface
	//==================================================================

	virtual FGuid GetProviderID() const override { return ProviderID; }
	virtual ESuspenseCoreContainerType GetContainerType() const override { return ESuspenseCoreContainerType::Equipment; }
	virtual FGameplayTag GetContainerTypeTag() const override;

	virtual FSuspenseCoreContainerUIData GetContainerData() const override;
	virtual FSuspenseCoreSlotUIData GetSlotData(int32 SlotIndex) const override;
	virtual FSuspenseCoreItemUIData GetItemData(const FGuid& InstanceID) const override;
	virtual TArray<FSuspenseCoreSlotUIData> GetAllSlotData() const override;

	virtual bool CanPerformAction(const FGameplayTag& ActionTag, int32 SlotIndex) const override;
	virtual bool RequestAction(const FGameplayTag& ActionTag, int32 SlotIndex, const FSuspenseCoreDragData* DragData = nullptr) override;

	virtual FOnSuspenseCoreUIDataChanged& OnUIDataChanged() override { return UIDataChangedDelegate; }

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
	 * Get slot data by slot tag
	 * @param SlotTag GameplayTag (Equipment.Slot.*)
	 * @return Slot UI data
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Provider")
	FSuspenseCoreSlotUIData GetSlotDataByTag(const FGameplayTag& SlotTag) const;

	/**
	 * Get equipped item data by slot type
	 * @param SlotType Equipment slot type
	 * @return Item UI data (invalid if slot empty)
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Provider")
	FSuspenseCoreItemUIData GetEquippedItemByType(EEquipmentSlotType SlotType) const;

	/**
	 * Get all equipment slot configurations
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Provider")
	TArray<FEquipmentSlotConfig> GetSlotConfigs() const { return SlotConfigs; }

	/**
	 * Get number of equipment slots
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Provider")
	int32 GetSlotCount() const { return SlotConfigs.Num(); }

	/**
	 * Force refresh all slot data
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Provider")
	void RefreshAllSlots();

protected:
	//==================================================================
	// Data Store Binding
	//==================================================================

	/** Subscribe to equipment data store events */
	virtual void BindToDataStore();

	/** Unsubscribe from equipment data store events */
	virtual void UnbindFromDataStore();

	/** Handle equipment slot changed event */
	UFUNCTION()
	void OnEquipmentSlotChanged(EEquipmentSlotType SlotType, const FGuid& ItemInstanceID);

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

	/** Bound equipment data store */
	UPROPERTY(Transient)
	TWeakObjectPtr<UActorComponent> BoundDataStore;

	/** Loadout ID for configuration */
	UPROPERTY()
	FName LoadoutID;

	/** Cached slot configurations */
	UPROPERTY()
	TArray<FEquipmentSlotConfig> SlotConfigs;

	/** Is provider initialized */
	bool bIsInitialized;

	/** UI data changed delegate */
	FOnSuspenseCoreUIDataChanged UIDataChangedDelegate;

	/** Cached EventBus reference */
	UPROPERTY(Transient)
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** Cached LoadoutManager reference */
	UPROPERTY(Transient)
	TWeakObjectPtr<USuspenseCoreLoadoutManager> CachedLoadoutManager;

private:
	/** Get EventBus */
	USuspenseCoreEventBus* GetEventBus() const;

	/** Get LoadoutManager */
	USuspenseCoreLoadoutManager* GetLoadoutManager() const;

	/** Map slot type to index for quick lookup */
	TMap<EEquipmentSlotType, int32> SlotTypeToIndex;
};
