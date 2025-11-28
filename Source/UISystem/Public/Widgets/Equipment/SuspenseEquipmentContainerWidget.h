// MedComEquipmentContainerWidget.h
// Copyright Suspense Team Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Base/SuspenseBaseContainerWidget.h"
#include "Types/UI/SuspenseEquipmentUITypes.h"
#include "Components/CanvasPanel.h"
#include "Interfaces/UI/ISuspenseEquipmentUIBridge.h"
#include "Types/Loadout/SuspenseLoadoutSettings.h"
#include "SuspenseEquipmentContainerWidget.generated.h"

// Forward declarations
class UCanvasPanel;
class UGridPanel;
class UBorder;
class UTextBlock;
class USuspenseEquipmentSlotWidget;
class USuspenseEquipmentUIBridge;
class USuspenseDragDropOperation;
class USuspenseLoadoutManager;

/**
 * Equipment slot container info
 * Holds grid panel and slot widgets for a specific equipment slot type
 */
USTRUCT()
struct FEquipmentSlotContainer
{
    GENERATED_BODY()

    /** Grid panel for this equipment slot type */
    UPROPERTY()
    UGridPanel* GridPanel = nullptr;

    /** Equipment slot configuration from LoadoutSettings */
    FEquipmentSlotConfig SlotConfig;

    /** Created slot widgets for this container */
    UPROPERTY()
    TArray<USuspenseEquipmentSlotWidget*> SlotWidgets;

    /** Base slot index for this container (global index in container space) */
    int32 BaseSlotIndex = 0;

    /** Cached slot states for efficient updates */
    TMap<int32, bool> CachedSlotStates;

    FEquipmentSlotContainer()
    {
        BaseSlotIndex = 0;
    }
};

/**
 * Equipment Container Widget - SIMPLIFIED ARCHITECTURE
 *
 * NEW DATA FLOW (5 steps):
 *  1. DataStore changes slot data
 *  2. UIBridge receives HandleDataStoreSlotChanged
 *  3. Bridge updates CachedUIData and broadcasts OnEquipmentUIDataChanged
 *  4. Container receives HandleEquipmentDataChanged with ready data
 *  5. Container calls UpdateAllEquipmentSlots and widgets update
 *
 * OLD DATA FLOW (13 steps) - REMOVED:
 *  - No more UIConnector component
 *  - No more UIPort interface
 *  - No more EventDelegateManager::NotifyInventoryUIRefreshRequested
 *  - No more RequestDataRefresh with debounce
 *  - No more GetEquipmentSlotsUIData calls
 *
 * Key improvements:
 *  - Direct subscription to UIBridge delegate (no global events)
 *  - Receives pre-built FEquipmentSlotUIData (no conversion needed)
 *  - Immediate updates (no debounce delay unless desired)
 *  - Simpler code path (easier to debug)
 *  - Better performance (fewer allocations, no extra copies)
 *
 * Responsibilities:
 *  - Manage equipment slot layout based on LoadoutManager config
 *  - Subscribe directly to UIBridge for data updates
 *  - Update slot widgets when data changes
 *  - Handle user interactions (drag-drop) through Bridge
 *  - Manage slot visibility based on active loadout
 */
UCLASS(Abstract)
class UISYSTEM_API USuspenseEquipmentContainerWidget : public USuspenseBaseContainerWidget
{
    GENERATED_BODY()

public:
    USuspenseEquipmentContainerWidget(const FObjectInitializer& ObjectInitializer);

    //~ Begin UUserWidget
    virtual void NativeConstruct() override;
    virtual void NativePreConstruct() override;
    virtual void NativeDestruct() override;
    //~ End UUserWidget

    //~ Begin USuspenseBaseContainerWidget
    virtual UPanelWidget* GetSlotsPanel() const override { return EquipmentCanvas; }
    virtual bool ValidateSlotsPanel() const override;
    virtual void CreateSlots() override;
    virtual void ClearSlots() override;
    virtual void UpdateSlotWidget(int32 SlotIndex, const FSlotUIData& SlotData, const FItemUIData& ItemData) override;
    virtual float GetCellSize() const override { return CellSize; }
    virtual bool CalculateOccupiedSlots(int32 TargetSlot, FIntPoint ItemSize, bool bIsRotated, TArray<int32>& OutOccupiedSlots) const override;
    virtual FSmartDropZone FindBestDropZone(const FVector2D& ScreenPosition, const FIntPoint& ItemSize, bool bIsRotated) const override;
    //~ End USuspenseBaseContainerWidget

    //~ Begin ISuspenseUIWidgetInterface
    virtual void InitializeWidget_Implementation() override;
    //~ End ISuspenseUIWidgetInterface

    //~ Begin ISuspenseContainerUIInterface
    virtual void InitializeContainer_Implementation(const FContainerUIData& ContainerData) override;
    virtual void UpdateContainer_Implementation(const FContainerUIData& ContainerData) override;
    virtual FSlotValidationResult CanAcceptDrop_Implementation(const UDragDropOperation* DragOperation, int32 TargetSlotIndex) const override;
    virtual void RequestDataRefresh_Implementation() override;
    //~ End ISuspenseContainerUIInterface

    // ===== NEW: Direct UIBridge Integration =====

    /**
     * Set UIBridge reference for direct subscription
     * This replaces the old global event system approach
     * Container will subscribe to Bridge's OnEquipmentUIDataChanged delegate
     * @param InBridge - Equipment UI Bridge instance to bind to
     */
    UFUNCTION(BlueprintCallable, Category="Equipment")
    void SetUIBridge(USuspenseEquipmentUIBridge* InBridge);

    /**
     * Get current UIBridge reference
     * @return Current equipment UI bridge or nullptr
     */
    UFUNCTION(BlueprintCallable, Category="Equipment")
    USuspenseEquipmentUIBridge* GetUIBridge() const { return UIBridge; }

    // ===== Loadout Management =====

    /** Pull loadout config from LoadoutManager and rebuild UI */
    UFUNCTION(BlueprintCallable, Category="Equipment|Loadout")
    void RefreshFromLoadoutManager();

    /** Apply visibility according to current config */
    UFUNCTION(BlueprintCallable, Category="Equipment|Loadout")
    void UpdateSlotVisibilityFromConfig();

    /** Get slot config by enum type */
    UFUNCTION(BlueprintCallable, Category="Equipment|Loadout")
    bool GetSlotConfigByType(EEquipmentSlotType SlotType, FEquipmentSlotConfig& OutConfig) const;

    /** Is slot type present & visible in current loadout */
    UFUNCTION(BlueprintCallable, Category="Equipment|Loadout")
    bool IsSlotActiveInCurrentLoadout(EEquipmentSlotType SlotType) const;

    // ===== Equipment Display =====

    /** Update equipment visual state from bridge data */
    UFUNCTION(BlueprintCallable, Category="Equipment")
    void UpdateEquipmentDisplay(const FEquipmentContainerUIData& EquipmentData);

    /** Get equipment slot widget by slot tag + local index */
    UFUNCTION(BlueprintCallable, Category="Equipment")
    USuspenseEquipmentSlotWidget* GetEquipmentSlot(const FGameplayTag& SlotType, int32 LocalIndex) const;

    /** Get equipment slot widget by global index */
    UFUNCTION(BlueprintCallable, Category="Equipment")
    USuspenseEquipmentSlotWidget* GetEquipmentSlotByIndex(int32 GlobalIndex) const;

    /** Map global index -> slot type tag */
    UFUNCTION(BlueprintCallable, Category="Equipment")
    FGameplayTag GetSlotTypeForIndex(int32 GlobalIndex) const;

    /** Current loadout getters */
    UFUNCTION(BlueprintCallable, Category="Equipment|Loadout")
    const FLoadoutConfiguration& GetLoadoutConfiguration() const { return CurrentLoadoutConfig; }

    UFUNCTION(BlueprintCallable, Category="Equipment|Loadout")
    FName GetCurrentLoadoutID() const { return CurrentLoadoutID; }

protected:
    // ===== UI Bindings =====

    //~ Root panel
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
    UCanvasPanel* EquipmentCanvas;

    //~ Slot containers (bind in BP to Borders)
    // Weapons
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) UBorder* PrimaryWeaponSlotContainer;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) UBorder* SecondaryWeaponSlotContainer;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) UBorder* HolsterSlotContainer;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) UBorder* ScabbardSlotContainer;

    // Head
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) UBorder* HeadwearSlotContainer;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) UBorder* EarpieceSlotContainer;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) UBorder* EyewearSlotContainer;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) UBorder* FaceCoverSlotContainer;

    // Body
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) UBorder* BodyArmorSlotContainer;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) UBorder* TacticalRigSlotContainer;

    // Storage
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) UBorder* BackpackSlotContainer;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) UBorder* SecureContainerSlotContainer;

    // Quick access
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) UBorder* QuickSlot1Container;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) UBorder* QuickSlot2Container;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) UBorder* QuickSlot3Container;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) UBorder* QuickSlot4Container;

    // Special
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) UBorder* ArmbandSlotContainer;

    //~ Optional UI
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) UTextBlock* TitleText;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) UBorder* BackgroundBorder;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) UTextBlock* LoadoutNameText;

    // ===== Configuration =====

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment|Config")
    FName FallbackLoadoutID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment|Config")
    bool bAutoRefreshFromLoadoutManager;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment|Config")
    bool bHideUnusedSlots;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Layout")
    float CellPadding;

    // ===== State =====

    UPROPERTY(BlueprintReadOnly, Category="Equipment|State")
    FName CurrentLoadoutID;

    UPROPERTY(BlueprintReadOnly, Category="Equipment|State")
    FLoadoutConfiguration CurrentLoadoutConfig;

    UPROPERTY(BlueprintReadOnly, Category="Equipment|State")
    FEquipmentContainerUIData CurrentEquipmentData;

    UPROPERTY() USuspenseLoadoutManager* CachedLoadoutManager;

    // ===== Blueprint Events =====

    UFUNCTION(BlueprintImplementableEvent, Category="Equipment|Events", DisplayName="On Loadout Changed")
    void K2_OnLoadoutChanged(const FName& NewLoadoutID);

    UFUNCTION(BlueprintImplementableEvent, Category="Equipment|Events", DisplayName="On Slot Visibility Changed")
    void K2_OnSlotVisibilityChanged(EEquipmentSlotType SlotType, bool bIsVisible);

    UFUNCTION(BlueprintImplementableEvent, Category="Equipment|Events", DisplayName="On Loadout Refresh Failed")
    void K2_OnLoadoutRefreshFailed(const FString& Reason);

    // ===== Event Subscriptions =====

    virtual void SubscribeToEvents() override;
    virtual void UnsubscribeFromEvents() override;

private:
    // ===== NEW: Direct UIBridge Reference =====

    /**
     * Direct reference to UIBridge (replaces TScriptInterface)
     * This is the single source of equipment data for this widget
     * Container subscribes to Bridge's OnEquipmentUIDataChanged delegate
     */
    UPROPERTY()
    USuspenseEquipmentUIBridge* UIBridge = nullptr;

    /**
     * Delegate handle for UIBridge subscription
     * Must be properly removed in NativeDestruct to prevent crashes
     */
    FDelegateHandle DataChangedHandle;

    /**
     * NEW: Direct handler for equipment data changes from Bridge
     * Receives pre-built FEquipmentSlotUIData array ready for display
     * No conversion or additional queries needed
     * @param FreshData - Complete array of equipment slot UI data
     */
    void HandleEquipmentDataChanged(const TArray<FEquipmentSlotUIData>& FreshData);

    // ===== Container Management =====

    /** Map of equipment slot containers by slot tag */
    UPROPERTY() TMap<FGameplayTag, FEquipmentSlotContainer> EquipmentContainers;

    /** Map of slot type tags to Border containers (active ones) */
    TMap<FGameplayTag, UBorder*> SlotContainerMap;

    /** All possible slot containers (by enum) */
    TMap<EEquipmentSlotType, UBorder*> AllSlotContainers;

    /** Active slot types in current loadout */
    TArray<EEquipmentSlotType> ActiveSlotTypes;

    /** All available slot types (taxonomy) */
    TArray<EEquipmentSlotType> AllAvailableSlotTypes;

    /** Whether event subscriptions are active */
    bool bEventSubscriptionsActive = false;

    // ===== Legacy Event Handles (kept for backward compatibility) =====

    /**
     * NOTE: These native event handles are KEPT for potential future use
     * They are NOT used in the new simplified architecture
     * EventDelegateManager native events are replaced by direct Bridge subscription
     * Keeping them in code in case project needs them for other purposes
     */
    FDelegateHandle EquipmentUIRefreshHandle;
    FDelegateHandle EquipmentSlotHandle;
    FDelegateHandle EquipmentUpdatedHandle;

    // ===== Internal Helpers =====

    bool ValidateAllBorderBindings() const;
    void ShowAllSlotsForDesign();
    void UseDefaultLoadoutForTesting();

    USuspenseLoadoutManager* GetLoadoutManager() const;
    FName GetCurrentLoadoutIDFromContext() const;

    void ApplyLoadoutConfigurationInternal(const FLoadoutConfiguration& LoadoutConfig);
    void InitializeAllSlotContainers();
    void InitializeSlotContainerMap();

    FGameplayTag GetSlotTypeTag(EEquipmentSlotType SlotType) const;
    bool ValidateSlotContainers() const;
    void UpdateAllSlotVisibility();
    void SetSlotVisibility(EEquipmentSlotType SlotType, bool bVisible);

    UGridPanel* CreateGridPanelInContainer(UBorder* Container, const FEquipmentSlotConfig& SlotConfig);
    void CreateEquipmentContainers();
    void CreateSlotsForContainer(FGameplayTag SlotType, FEquipmentSlotContainer& Container);

    int32 CalculateGlobalIndex(const FGameplayTag& SlotType, int32 LocalIndex) const;
    bool GetContainerFromGlobalIndex(int32 GlobalIndex, FGameplayTag& OutSlotType, int32& OutLocalIndex) const;

    void UpdateAllEquipmentSlots(const FEquipmentContainerUIData& EquipmentData);
    bool CalculateOccupiedSlotsInContainer(const FGameplayTag& SlotType, int32 LocalIndex, FIntPoint ItemSize, TArray<int32>& OutGlobalIndices) const;
    bool IsItemTypeAllowedInSlot(const FGameplayTag& ItemType, const FGameplayTag& SlotType) const;

    ISuspenseEquipmentUIBridgeInterface* GetOrCreateEquipmentBridge();
    bool ProcessEquipmentOperationThroughBridge(const FDragDropUIData& DragData, int32 TargetSlotIndex);

    /** Event handlers for legacy compatibility */
    UFUNCTION() void OnEquipmentSlotUpdated(int32 SlotIndex, const FGameplayTag& SlotType, bool bIsOccupied);
    UFUNCTION() void OnEquipmentUIRefreshRequested(UUserWidget* Widget);
    UFUNCTION() void OnLoadoutChanged(const FName& LoadoutID, APlayerState* PlayerState, bool bSuccess);
};
