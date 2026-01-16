// SuspenseCoreEquipmentWidget.h
// SuspenseCore - Equipment Container Widget
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Widgets/Base/SuspenseCoreBaseContainerWidget.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreLoadoutSettings.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUIContainerTypes.h"
#include "SuspenseCore/Types/UI/SuspenseCoreOptimisticUITypes.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Events/UI/SuspenseCoreUIEvents.h"
#include "SuspenseCoreEquipmentWidget.generated.h"

// Forward declarations
class USuspenseCoreEquipmentSlotWidget;
class UCanvasPanel;
class UOverlay;
class USuspenseCoreEventBus;
class USuspenseCoreOptimisticUIManager;
class ASuspenseCoreCharacterPreviewActor;

/**
 * USuspenseCoreEquipmentWidget
 *
 * Container widget for equipment slots (named slots, not grid).
 * Displays weapon, armor, and accessory slots with free-form positioning.
 *
 * ARCHITECTURE:
 * - Inherits from USuspenseCoreBaseContainerWidget for provider binding
 * - Uses Named slot layout (not Grid)
 * - Slots identified by EEquipmentSlotType / FGameplayTag
 * - Positioned via FSuspenseCoreEquipmentSlotUIConfig
 *
 * BINDWIDGET COMPONENTS (create in Blueprint):
 * - SlotContainer: UCanvasPanel or UOverlay - Parent for slot widgets
 *
 * SLOT CREATION:
 * - Slots are created from FLoadoutConfiguration::EquipmentSlots
 * - Each slot widget is USuspenseCoreEquipmentSlotWidget
 * - Position each slot in Blueprint or via SlotUIConfigs
 *
 * @see USuspenseCoreBaseContainerWidget
 * @see USuspenseCoreEquipmentSlotWidget
 */
UCLASS(BlueprintType, Blueprintable)
class UISYSTEM_API USuspenseCoreEquipmentWidget : public USuspenseCoreBaseContainerWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreEquipmentWidget(const FObjectInitializer& ObjectInitializer);

	//==================================================================
	// UUserWidget Interface
	//==================================================================

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Drag-Drop Handling (CRITICAL: Required for equipment slot drops AND drags)
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent, UDragDropOperation* Operation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	//==================================================================
	// ISuspenseCoreUIContainer Overrides
	//==================================================================

	virtual ESuspenseCoreContainerType GetContainerType() const override { return ESuspenseCoreContainerType::Equipment; }
	virtual UWidget* GetSlotWidget(int32 SlotIndex) const override;
	virtual TArray<UWidget*> GetAllSlotWidgets() const override;

	//==================================================================
	// Equipment-Specific API
	//==================================================================

	/**
	 * Initialize equipment slots from loadout configuration
	 * @param InSlotConfigs Array of slot configurations from FLoadoutConfiguration
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Equipment")
	void InitializeFromSlotConfigs(const TArray<FEquipmentSlotConfig>& InSlotConfigs);

	/**
	 * Initialize with UI layout configurations
	 * @param InSlotConfigs Slot configs from loadout
	 * @param InUIConfigs UI-specific configs for positioning
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Equipment")
	void InitializeWithUIConfig(
		const TArray<FEquipmentSlotConfig>& InSlotConfigs,
		const TArray<FSuspenseCoreEquipmentSlotUIConfig>& InUIConfigs);

	/**
	 * Get slot widget by slot type
	 * @param SlotType Equipment slot type enum
	 * @return Slot widget or nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Equipment")
	USuspenseCoreEquipmentSlotWidget* GetSlotByType(EEquipmentSlotType SlotType) const;

	/**
	 * Get slot widget by slot tag
	 * @param SlotTag GameplayTag (Equipment.Slot.*)
	 * @return Slot widget or nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Equipment")
	USuspenseCoreEquipmentSlotWidget* GetSlotByTag(const FGameplayTag& SlotTag) const;

	/**
	 * Update specific slot by type
	 * @param SlotType Equipment slot type
	 * @param SlotData New slot data
	 * @param ItemData New item data
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Equipment")
	void UpdateSlotByType(EEquipmentSlotType SlotType, const FSuspenseCoreSlotUIData& SlotData, const FSuspenseCoreItemUIData& ItemData);

	/**
	 * Update specific slot by tag
	 * @param SlotTag GameplayTag (Equipment.Slot.*)
	 * @param SlotData New slot data
	 * @param ItemData New item data
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Equipment")
	void UpdateSlotByTag(const FGameplayTag& SlotTag, const FSuspenseCoreSlotUIData& SlotData, const FSuspenseCoreItemUIData& ItemData);

	/**
	 * Get all slot types present in this widget
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Equipment")
	TArray<EEquipmentSlotType> GetAllSlotTypes() const;

	/**
	 * Check if slot exists
	 * @param SlotType Equipment slot type
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Equipment")
	bool HasSlot(EEquipmentSlotType SlotType) const;

	//==================================================================
	// Blueprint Events
	//==================================================================

	/** Called when all slots are initialized */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Equipment", meta = (DisplayName = "On Slots Initialized"))
	void K2_OnSlotsInitialized(int32 SlotCount);

	/** Called when a slot receives an equip request (before processing) */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Equipment", meta = (DisplayName = "On Equip Requested"))
	void K2_OnEquipRequested(EEquipmentSlotType SlotType, const FSuspenseCoreItemUIData& ItemData);

	/** Called when a slot receives an unequip request */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Equipment", meta = (DisplayName = "On Unequip Requested"))
	void K2_OnUnequipRequested(EEquipmentSlotType SlotType);

	/** Called when prediction is rolled back (server rejected) */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Equipment", meta = (DisplayName = "On Prediction Rolled Back"))
	void K2_OnPredictionRolledBack(int32 PredictionKey, const FText& ErrorMessage);

	//==================================================================
	// Optimistic UI API (AAA-Level Client Prediction)
	//==================================================================

	/**
	 * Request equip with optimistic visual update.
	 * Updates UI immediately, then confirms/rollbacks based on server response.
	 * Follows pattern from USuspenseCoreMagazineComponent::PredictStartReload.
	 *
	 * @param ItemInstanceID Item to equip
	 * @param TargetSlotIndex Target equipment slot index
	 * @param SourceContainerID Source container (inventory) ID
	 * @param SourceSlotIndex Source slot in inventory
	 * @return Prediction key for tracking, or INDEX_NONE if failed
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Equipment|OptimisticUI")
	int32 RequestEquipOptimistic(
		const FGuid& ItemInstanceID,
		int32 TargetSlotIndex,
		const FGuid& SourceContainerID,
		int32 SourceSlotIndex);

	/**
	 * Request unequip with optimistic visual update.
	 * @param SourceSlotIndex Equipment slot to unequip from
	 * @param TargetContainerID Target container (inventory) ID
	 * @param TargetSlotIndex Target slot in inventory
	 * @return Prediction key for tracking, or INDEX_NONE if failed
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Equipment|OptimisticUI")
	int32 RequestUnequipOptimistic(
		int32 SourceSlotIndex,
		const FGuid& TargetContainerID,
		int32 TargetSlotIndex);

	/**
	 * Confirm a pending prediction (server accepted).
	 * Removes prediction - visual state is already correct.
	 * @param PredictionKey Key from Request*Optimistic
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Equipment|OptimisticUI")
	void ConfirmPrediction(int32 PredictionKey);

	/**
	 * Rollback a pending prediction (server rejected).
	 * Restores visual state from snapshot.
	 * @param PredictionKey Key from Request*Optimistic
	 * @param ErrorMessage Error message for feedback
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Equipment|OptimisticUI")
	void RollbackPrediction(int32 PredictionKey, const FText& ErrorMessage);

	/**
	 * Check if there's a pending prediction for a slot
	 * @param SlotIndex Slot to check
	 * @return true if slot has pending prediction
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Equipment|OptimisticUI")
	bool HasPendingPredictionForSlot(int32 SlotIndex) const;

protected:
	//==================================================================
	// Override Points from Base
	//==================================================================

	virtual void CreateSlotWidgets_Implementation() override;
	virtual void UpdateSlotWidget_Implementation(int32 SlotIndex, const FSuspenseCoreSlotUIData& SlotData, const FSuspenseCoreItemUIData& ItemData) override;
	virtual void ClearSlotWidgets_Implementation() override;

	//==================================================================
	// Slot Creation
	//==================================================================

	/**
	 * Create a single slot widget
	 * @param SlotConfig Configuration for the slot
	 * @param UIConfig UI layout config (optional)
	 * @return Created slot widget
	 */
	virtual USuspenseCoreEquipmentSlotWidget* CreateSlotWidget(
		const FEquipmentSlotConfig& SlotConfig,
		const FSuspenseCoreEquipmentSlotUIConfig* UIConfig = nullptr);

	/**
	 * Position slot widget in container
	 * @param SlotWidget Widget to position
	 * @param UIConfig UI config with position
	 */
	virtual void PositionSlotWidget(USuspenseCoreEquipmentSlotWidget* SlotWidget, const FSuspenseCoreEquipmentSlotUIConfig& UIConfig);

	/**
	 * Position slot widget using FEquipmentSlotConfig (SSOT)
	 * Uses UIPosition and UISize directly from the config
	 * @param SlotWidget Widget to position
	 * @param Config Equipment slot config with UI layout data
	 */
	virtual void PositionSlotWidgetFromConfig(USuspenseCoreEquipmentSlotWidget* SlotWidget, const FEquipmentSlotConfig& Config);

	//==================================================================
	// Widget References (Bind in Blueprint) - REQUIRED components
	//==================================================================

	/**
	 * Container for slot widgets (CanvasPanel for absolute positioning)
	 * REQUIRED: Blueprint MUST have a CanvasPanel named 'SlotContainer'
	 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Widgets")
	TObjectPtr<UCanvasPanel> SlotContainer;

	//==================================================================
	// Character Preview (3D Model Display)
	//==================================================================

	/**
	 * Reference to character preview actor for 3D model display
	 * Set via Blueprint or spawned programmatically
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Preview")
	TWeakObjectPtr<ASuspenseCoreCharacterPreviewActor> CharacterPreview;

	/**
	 * Spawn and setup character preview actor
	 * @param PreviewActorClass Class to spawn for preview
	 * @return Spawned actor or nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Equipment")
	ASuspenseCoreCharacterPreviewActor* SpawnCharacterPreview(TSubclassOf<ASuspenseCoreCharacterPreviewActor> PreviewActorClass);

	/**
	 * Update character preview to show current equipment
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Equipment")
	void RefreshCharacterPreview();

	//==================================================================
	// Configuration
	//==================================================================

	/**
	 * If true, automatically load slot configs from SSOT on NativeConstruct.
	 * Reads from: Project Settings → Game → SuspenseCore → EquipmentSlotPresetsAsset
	 * No manual configuration needed in widget - just enable this flag.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|SSOT")
	bool bAutoInitializeFromSSoT = true;

	/** Slot widget class to instantiate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
	TSubclassOf<USuspenseCoreEquipmentSlotWidget> SlotWidgetClass;

	/** Default slot size for equipment slots */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
	FVector2D DefaultSlotSize;

	/** Slot configurations from loadout */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	TArray<FEquipmentSlotConfig> SlotConfigs;

	/** UI configurations for slot layout */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
	TArray<FSuspenseCoreEquipmentSlotUIConfig> SlotUIConfigs;

private:
	//==================================================================
	// Slot Management
	//==================================================================

	/** Map of slot type to slot widget */
	UPROPERTY(Transient)
	TMap<EEquipmentSlotType, TObjectPtr<USuspenseCoreEquipmentSlotWidget>> SlotWidgetsByType;

	/** Map of slot tag to slot widget */
	TMap<FGameplayTag, TWeakObjectPtr<USuspenseCoreEquipmentSlotWidget>> SlotWidgetsByTag;

	/** Slot widgets in order (for index-based access) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USuspenseCoreEquipmentSlotWidget>> SlotWidgetsArray;

	/** Find UI config for slot type */
	const FSuspenseCoreEquipmentSlotUIConfig* FindUIConfigForSlot(EEquipmentSlotType SlotType) const;

	/** Get index for slot type (for compatibility with base class) */
	int32 GetSlotIndexForType(EEquipmentSlotType SlotType) const;

	/** Auto-initialize slot configs from LoadoutManager (SSOT) */
	void AutoInitializeFromLoadoutManager();

	//==================================================================
	// Drag-Drop Helpers
	//==================================================================

	/** Get slot widget under screen space position (more accurate) */
	USuspenseCoreEquipmentSlotWidget* GetSlotWidgetAtScreenPosition(const FVector2D& ScreenSpacePos) const;

	/** Get slot widget under local position (backward compat - uses screen method internally) */
	USuspenseCoreEquipmentSlotWidget* GetSlotWidgetAtPosition(const FVector2D& LocalPos) const;

	/** Clear all slot highlights */
	void ClearSlotHighlights();

	/** Currently hovered slot index for drag-drop */
	int32 HoveredSlotIndex = INDEX_NONE;

	/** Source slot for drag operation (tracks which slot we're dragging from) */
	int32 DragSourceSlot = INDEX_NONE;

	/** Mouse position when drag started (for calculating drag offset) */
	FVector2D DragStartMousePosition = FVector2D::ZeroVector;

	//==================================================================
	// EventBus Integration (REQUIRED pattern per documentation)
	//==================================================================

	/** Setup EventBus subscriptions - called in NativeConstruct */
	void SetupEventSubscriptions();

	/** Teardown EventBus subscriptions - called in NativeDestruct */
	void TeardownEventSubscriptions();

	/** Get EventBus instance (cached) */
	USuspenseCoreEventBus* GetEventBus() const;

	/** EventBus event handlers */
	void OnEquipmentItemEquipped(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnEquipmentItemUnequipped(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnProviderDataChanged(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnQuickSlotAssignedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnQuickSlotClearedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnPredictionRollbackEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Active EventBus subscriptions */
	TArray<FSuspenseCoreSubscriptionHandle> SubscriptionHandles;

	/** Cached EventBus reference */
	mutable TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	//==================================================================
	// Optimistic UI (AAA-Level Client Prediction)
	//==================================================================

	/**
	 * Apply optimistic equip visual.
	 * Called immediately on user action before server confirmation.
	 * @param SlotIndex Target slot
	 * @param ItemData Item to show
	 */
	void ApplyOptimisticEquip(int32 SlotIndex, const FSuspenseCoreItemUIData& ItemData);

	/**
	 * Apply optimistic unequip visual.
	 * @param SlotIndex Slot to clear
	 */
	void ApplyOptimisticUnequip(int32 SlotIndex);

	/**
	 * Restore slot from snapshot (rollback).
	 * @param Snapshot Slot snapshot to restore
	 */
	void RestoreSlotFromSnapshot(const FSuspenseCoreSlotSnapshot& Snapshot);

	/**
	 * Get Optimistic UI Manager.
	 * @return Manager or nullptr
	 */
	USuspenseCoreOptimisticUIManager* GetOptimisticUIManager() const;

	/** Cached OptimisticUIManager reference */
	mutable TWeakObjectPtr<USuspenseCoreOptimisticUIManager> CachedOptimisticUIManager;

	/** Map of prediction key to affected slot */
	TMap<int32, int32> PendingPredictionSlots;
};
