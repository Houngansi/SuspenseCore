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
class USuspenseCoreEventBus;
class USuspenseCoreLoadoutManager;

/**
 * USuspenseCoreEquipmentUIProvider
 *
 * Adapter that wraps equipment data to provide ISuspenseCoreUIDataProvider interface for UI widgets.
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
	 * @param InOwningActor Actor that owns the equipment
	 * @param InLoadoutID Loadout ID for slot configuration
	 * @return True if initialized successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Provider")
	bool Initialize(AActor* InOwningActor, FName InLoadoutID = NAME_None);

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
	// ISuspenseCoreUIDataProvider Interface - Identity
	//==================================================================

	virtual FGuid GetProviderID() const override { return ProviderID; }
	virtual ESuspenseCoreContainerType GetContainerType() const override { return ESuspenseCoreContainerType::Equipment; }
	virtual FGameplayTag GetContainerTypeTag() const override;
	virtual AActor* GetOwningActor() const override { return OwningActor.Get(); }

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

	/** Owning actor */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> OwningActor;

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
	mutable TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** Cached LoadoutManager reference */
	mutable TWeakObjectPtr<USuspenseCoreLoadoutManager> CachedLoadoutManager;

private:
	/** Get LoadoutManager */
	USuspenseCoreLoadoutManager* GetLoadoutManager() const;

	/** Map slot type to index for quick lookup */
	TMap<EEquipmentSlotType, int32> SlotTypeToIndex;
};
