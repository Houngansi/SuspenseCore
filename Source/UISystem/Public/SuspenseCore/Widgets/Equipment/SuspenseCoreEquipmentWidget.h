// SuspenseCoreEquipmentWidget.h
// SuspenseCore - Equipment Container Widget
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Widgets/Base/SuspenseCoreBaseContainerWidget.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreLoadoutSettings.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUIContainerTypes.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Events/UI/SuspenseCoreUIEvents.h"
#include "SuspenseCoreEquipmentWidget.generated.h"

// Forward declarations
class USuspenseCoreEquipmentSlotWidget;
class UCanvasPanel;
class UOverlay;
class USuspenseCoreEventBus;
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

	// Drag-Drop Handling (CRITICAL: Required for equipment slot drops)
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

	/** Get slot widget under local position */
	USuspenseCoreEquipmentSlotWidget* GetSlotWidgetAtPosition(const FVector2D& LocalPos) const;

	/** Clear all slot highlights */
	void ClearSlotHighlights();

	/** Currently hovered slot index for drag-drop */
	int32 HoveredSlotIndex = INDEX_NONE;

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

	/** Active EventBus subscriptions */
	TArray<FSuspenseCoreSubscriptionHandle> SubscriptionHandles;

	/** Cached EventBus reference */
	mutable TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;
};
