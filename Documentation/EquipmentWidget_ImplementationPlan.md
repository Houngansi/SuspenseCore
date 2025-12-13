# Equipment Widget Implementation Plan
## SuspenseCore - Tarkov-Style Equipment System with Multi-Cell Items

**Version:** 1.0
**Created:** December 2024
**Status:** Plan Ready for Review
**Module:** UISystem

---

## Table of Contents

1. [Overview](#1-overview)
2. [Architecture Analysis](#2-architecture-analysis)
3. [Files to Create](#3-files-to-create)
4. [Implementation Phases](#4-implementation-phases)
5. [Detailed Class Designs](#5-detailed-class-designs)
6. [Multi-Cell Item Support](#6-multi-cell-item-support)
7. [EventBus Integration](#7-eventbus-integration)
8. [GameplayTags to Add](#8-gameplaytags-to-add)
9. [Dependencies & Prerequisites](#9-dependencies--prerequisites)
10. [Testing Strategy](#10-testing-strategy)

---

## 1. Overview

### Goal
Create a Tarkov-style equipment widget system supporting 17 named equipment slots with multi-cell items (e.g., 2x3 rifles, 1x2 helmets).

### Key Requirements
- **Named Slots**: 17 equipment slots (weapons, armor, gear, quick slots)
- **Multi-Cell Items**: Items occupy visual size based on GridSize (e.g., rifle = 2x5)
- **Drag-Drop**: Full support for inventory ↔ equipment transfers
- **EventBus**: All communication via GameplayTags, no direct calls
- **SSOT**: Data from LoadoutManager + EquipmentComponent providers

### Architecture Compliance
Following patterns from:
- `SuspenseCoreDeveloperGuide.md` - Core architecture
- `UISystem_DeveloperGuide.md` - UI patterns

---

## 2. Architecture Analysis

### Current State
```
USuspenseCoreBaseContainerWidget
    └── USuspenseCoreInventoryWidget (grid-based, implemented)
    └── USuspenseCoreEquipmentWidget (named slots, TO CREATE)
```

### Data Flow
```
┌─────────────────────────────────────────────────────────────────────────┐
│                            DATA SOURCES (SSOT)                           │
├─────────────────────────────────────────────────────────────────────────┤
│  FLoadoutConfiguration (DataTable)     │  FSuspenseCoreUnifiedItemData   │
│  - EEquipmentSlotType (17 slots)       │  - GridSize (multi-cell)        │
│  - FEquipmentSlotConfig                │  - EquipmentSlot tag            │
│  - AllowedItemTypes                    │  - Icon, DisplayName            │
└─────────────────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│                       EQUIPMENT PROVIDER (EquipmentSystem)               │
├─────────────────────────────────────────────────────────────────────────┤
│  USuspenseCoreEquipmentDataStore : ISuspenseCoreUIDataProvider          │
│  - GetContainerUIData() → Named slots layout                            │
│  - GetSlotUIData(slot) → Equipment slot state                           │
│  - ValidateDrop() → Slot type + item type validation                    │
│  - OnUIDataChanged → Notify widget                                      │
└─────────────────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│                            UI WIDGETS (UISystem)                         │
├─────────────────────────────────────────────────────────────────────────┤
│  USuspenseCoreEquipmentWidget : USuspenseCoreBaseContainerWidget        │
│  - BindToProvider()                                                      │
│  - CreateSlotWidgets() → Named slots (not grid)                         │
│  - HandleDrop() → Via DragDropHandler                                   │
│      └── USuspenseCoreEquipmentSlotWidget[]                             │
│          - SetSlotConfig(FEquipmentSlotConfig)                          │
│          - UpdateSlot(SlotUIData, ItemUIData)                           │
│          - Multi-cell visual sizing                                      │
└─────────────────────────────────────────────────────────────────────────┘
```

### Key Differences: Inventory vs Equipment

| Aspect | Inventory | Equipment |
|--------|-----------|-----------|
| **Layout** | Grid (10x5 cells) | Named slots (17 fixed) |
| **Slot Indexing** | `Row * Width + Col` | `EEquipmentSlotType` enum → int |
| **Multi-Cell** | Items span grid cells | Slot visual size = item GridSize |
| **Slot Size** | Fixed cell size | Variable per-slot (based on item type) |
| **Rotation** | R key during drag | N/A (slots have fixed orientation) |
| **Item Filter** | Container-wide tags | Per-slot AllowedItemTypes |
| **Drop Target** | Any valid grid position | Specific slot matching item's EquipmentSlot |

---

## 3. Files to Create

### 3.1 Widget Files (UISystem)

```
Source/UISystem/
├── Public/SuspenseCore/Widgets/Equipment/
│   ├── SuspenseCoreEquipmentWidget.h           # Main container widget
│   ├── SuspenseCoreEquipmentSlotWidget.h       # Individual slot widget
│   └── SuspenseCoreEquipmentDragVisualWidget.h # Drag preview (optional)
│
└── Private/SuspenseCore/Widgets/Equipment/
    ├── SuspenseCoreEquipmentWidget.cpp
    ├── SuspenseCoreEquipmentSlotWidget.cpp
    └── SuspenseCoreEquipmentDragVisualWidget.cpp
```

### 3.2 Type Extensions (BridgeSystem)

```
Source/BridgeSystem/Public/SuspenseCore/
├── Types/UI/
│   └── SuspenseCoreEquipmentUITypes.h          # Equipment-specific UI types
│
└── Events/UI/
    └── SuspenseCoreEquipmentUIEvents.h         # Equipment UI event tags (extend)
```

### 3.3 DragDropHandler Extensions

```
Source/UISystem/Private/SuspenseCore/Subsystems/
└── SuspenseCoreDragDropHandler.cpp             # Add equipment routing methods
```

---

## 4. Implementation Phases

### Phase 1: Core Widget Structure
**Priority: P0**

1. Create `USuspenseCoreEquipmentWidget`
   - Inherit from `USuspenseCoreBaseContainerWidget`
   - Override `CreateSlotWidgets_Implementation()`
   - Override `UpdateSlotWidget_Implementation()`
   - Override `GetSlotAtLocalPosition()` for named slots

2. Create `USuspenseCoreEquipmentSlotWidget`
   - Implement `ISuspenseCoreSlotUI` interface
   - Implement `ISuspenseCoreDraggable` interface
   - Implement `ISuspenseCoreDropTarget` interface

### Phase 2: Multi-Cell Visual Support
**Priority: P1**

1. Slot size calculation based on item GridSize
2. Icon scaling/positioning within slot bounds
3. Empty slot silhouette icons per slot type

### Phase 3: Drag-Drop Integration
**Priority: P1**

1. Extend `USuspenseCoreDragDropHandler`:
   - `HandleInventoryToEquipment()`
   - `HandleEquipmentToInventory()`
   - `HandleEquipmentToEquipment()` (swap)

2. Validation logic:
   - Slot type compatibility
   - Item type allowed check
   - Conflict rules (e.g., tactical rig + body armor)

### Phase 4: EventBus Integration
**Priority: P0**

1. Add native GameplayTags for equipment events
2. Subscribe to equipment change events
3. Publish UI request events

### Phase 5: Blueprint Support
**Priority: P2**

1. Blueprint-exposed configuration
2. K2_On* events for Blueprint handling
3. WBP_Equipment widget creation

---

## 5. Detailed Class Designs

### 5.1 USuspenseCoreEquipmentWidget

```cpp
/**
 * USuspenseCoreEquipmentWidget
 *
 * Named-slot equipment container widget for Tarkov-style loadout.
 * Unlike InventoryWidget (grid), uses fixed named slots.
 *
 * FEATURES:
 * - 17 named equipment slots (weapons, armor, gear, quick slots)
 * - Multi-cell item visual sizing (items display at their GridSize)
 * - Per-slot item type filtering
 * - Quick-equip from inventory support
 *
 * LAYOUT:
 * - Slots positioned via Canvas or pre-defined layout
 * - Each slot sized for maximum expected item (e.g., Primary = 2x5)
 * - Slot widgets are USuspenseCoreEquipmentSlotWidget
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

    //==================================================================
    // ISuspenseCoreUIContainer Overrides
    //==================================================================
    virtual void RefreshFromProvider() override;
    virtual UWidget* GetSlotWidget(int32 SlotIndex) const override;
    virtual TArray<UWidget*> GetAllSlotWidgets() const override;
    virtual int32 GetSlotAtLocalPosition(const FVector2D& LocalPosition) const override;

    //==================================================================
    // Equipment-Specific API
    //==================================================================

    /**
     * Get slot widget by equipment type
     * @param SlotType Equipment slot type enum
     * @return Slot widget or nullptr
     */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Equipment")
    USuspenseCoreEquipmentSlotWidget* GetSlotWidgetByType(EEquipmentSlotType SlotType) const;

    /**
     * Get slot index from equipment type
     * Converts EEquipmentSlotType to linear slot index.
     */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Equipment")
    static int32 SlotTypeToIndex(EEquipmentSlotType SlotType);

    /**
     * Get equipment type from slot index
     */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Equipment")
    static EEquipmentSlotType IndexToSlotType(int32 SlotIndex);

    /**
     * Find best equipment slot for item
     * @param ItemEquipmentSlotTag Item's Equipment.Slot.* tag
     * @return Matching slot type or None
     */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Equipment")
    EEquipmentSlotType FindSlotForItem(const FGameplayTag& ItemEquipmentSlotTag) const;

protected:
    //==================================================================
    // Override Points from Base
    //==================================================================
    virtual void CreateSlotWidgets_Implementation() override;
    virtual void UpdateSlotWidget_Implementation(
        int32 SlotIndex,
        const FSuspenseCoreSlotUIData& SlotData,
        const FSuspenseCoreItemUIData& ItemData) override;
    virtual void ClearSlotWidgets_Implementation() override;

    //==================================================================
    // Widget References
    //==================================================================

    /** Canvas panel for positioned slot placement */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Widgets")
    TObjectPtr<UCanvasPanel> SlotCanvas;

    /** Character preview actor (for 3D model display) */
    UPROPERTY(BlueprintReadWrite, Category = "Preview")
    TWeakObjectPtr<class ASuspenseCoreCharacterPreviewActor> CharacterPreview;

    //==================================================================
    // Configuration
    //==================================================================

    /** Slot widget class to spawn */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
    TSubclassOf<USuspenseCoreEquipmentSlotWidget> SlotWidgetClass;

    /** Equipment slot configurations (from Loadout or custom) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    TArray<FSuspenseCoreEquipmentSlotUIConfig> SlotConfigs;

    /** Load slot configs from loadout manager */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    bool bUseLoadoutManagerConfig = true;

    /** Loadout ID to use (if bUseLoadoutManagerConfig) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration",
        meta = (EditCondition = "bUseLoadoutManagerConfig"))
    FName LoadoutID = TEXT("Default_PMC");

private:
    //==================================================================
    // Slot Widgets
    //==================================================================

    /** Map from slot type to widget */
    UPROPERTY(Transient)
    TMap<EEquipmentSlotType, TObjectPtr<USuspenseCoreEquipmentSlotWidget>> SlotWidgetMap;

    /** Array of all slot widgets (for iteration) */
    UPROPERTY(Transient)
    TArray<TObjectPtr<USuspenseCoreEquipmentSlotWidget>> SlotWidgets;

    //==================================================================
    // Internal Helpers
    //==================================================================

    /** Load configuration from LoadoutManager */
    void LoadConfigFromLoadoutManager();

    /** Create slot widget for config */
    USuspenseCoreEquipmentSlotWidget* CreateSlotWidgetForConfig(
        const FSuspenseCoreEquipmentSlotUIConfig& Config);

    /** Position slot widget in canvas */
    void PositionSlotWidget(
        USuspenseCoreEquipmentSlotWidget* SlotWidget,
        const FSuspenseCoreEquipmentSlotUIConfig& Config);
};
```

### 5.2 USuspenseCoreEquipmentSlotWidget

```cpp
/**
 * USuspenseCoreEquipmentSlotWidget
 *
 * Individual equipment slot widget.
 * Displays equipped item or empty slot icon.
 * Supports multi-cell item visual sizing.
 *
 * FEATURES:
 * - Variable slot size based on allowed item types
 * - Empty slot silhouette icon
 * - Rarity border coloring
 * - Drag source and drop target
 * - Tooltip on hover
 *
 * @see USuspenseCoreEquipmentWidget
 * @see ISuspenseCoreSlotUI
 */
UCLASS(BlueprintType, Blueprintable)
class UISYSTEM_API USuspenseCoreEquipmentSlotWidget : public UUserWidget
    , public ISuspenseCoreSlotUI
    , public ISuspenseCoreDraggable
    , public ISuspenseCoreDropTarget
{
    GENERATED_BODY()

public:
    USuspenseCoreEquipmentSlotWidget(const FObjectInitializer& ObjectInitializer);

    //==================================================================
    // UUserWidget Interface
    //==================================================================
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseEnter(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent,
        UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDrop(
        const FGeometry& InGeometry,
        const FDragDropEvent& InDragDropEvent,
        UDragDropOperation* InOperation) override;

    //==================================================================
    // ISuspenseCoreSlotUI Interface
    //==================================================================
    virtual void InitializeSlot_Implementation(
        const FSlotUIData& SlotData,
        const FItemUIData& ItemData) override;
    virtual void UpdateSlot_Implementation(
        const FSlotUIData& SlotData,
        const FItemUIData& ItemData) override;
    virtual void SetSelected_Implementation(bool bIsSelected) override;
    virtual void SetHighlighted_Implementation(
        bool bIsHighlighted,
        const FLinearColor& HighlightColor) override;
    virtual int32 GetSlotIndex_Implementation() const override;
    virtual FGuid GetItemInstanceID_Implementation() const override;
    virtual bool IsOccupied_Implementation() const override;
    virtual void SetLocked_Implementation(bool bIsLocked) override;

    //==================================================================
    // ISuspenseCoreDraggable Interface
    //==================================================================
    virtual bool CanStartDrag() const override;
    virtual FSuspenseCoreDragData GetDragData() const override;
    virtual UWidget* CreateDragVisual() const override;

    //==================================================================
    // ISuspenseCoreDropTarget Interface
    //==================================================================
    virtual bool CanAcceptDrop(const FSuspenseCoreDragData& DragData) const override;
    virtual FSuspenseCoreDropValidation ValidateDrop(
        const FSuspenseCoreDragData& DragData) const override;
    virtual void OnDragEnter(const FSuspenseCoreDragData& DragData) override;
    virtual void OnDragLeave() override;
    virtual bool HandleDrop(const FSuspenseCoreDragData& DragData) override;

    //==================================================================
    // Equipment Slot Specific
    //==================================================================

    /**
     * Configure slot from equipment slot config
     * @param Config Slot configuration from loadout
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Equipment")
    void SetSlotConfig(const FSuspenseCoreEquipmentSlotUIConfig& Config);

    /**
     * Get current slot configuration
     */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Equipment")
    const FSuspenseCoreEquipmentSlotUIConfig& GetSlotConfig() const { return SlotConfig; }

    /**
     * Check if item type can be equipped in this slot
     * @param ItemTypeTag Item's type tag
     * @return true if allowed
     */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Equipment")
    bool CanAcceptItemType(const FGameplayTag& ItemTypeTag) const;

    /**
     * Get equipment slot type
     */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Equipment")
    EEquipmentSlotType GetSlotType() const;

    /**
     * Set parent container reference
     */
    void SetOwningContainer(USuspenseCoreEquipmentWidget* InContainer);

protected:
    //==================================================================
    // Bound Widgets (UMG Designer)
    //==================================================================

    /** Root size box - controls slot visual size */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Widgets")
    TObjectPtr<USizeBox> RootSizeBox;

    /** Background border - slot background */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Widgets")
    TObjectPtr<UBorder> BackgroundBorder;

    /** Rarity border - colored by item rarity */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Widgets")
    TObjectPtr<UBorder> RarityBorder;

    /** Empty slot icon - silhouette when no item */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Widgets")
    TObjectPtr<UImage> EmptySlotIcon;

    /** Item icon - actual item image */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Widgets")
    TObjectPtr<UImage> ItemIcon;

    /** Highlight overlay - drag/select highlight */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Widgets")
    TObjectPtr<UImage> HighlightOverlay;

    /** Slot name text (optional) */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
    TObjectPtr<UTextBlock> SlotNameText;

    //==================================================================
    // Configuration
    //==================================================================

    /** Current slot configuration */
    UPROPERTY(BlueprintReadOnly, Category = "Configuration")
    FSuspenseCoreEquipmentSlotUIConfig SlotConfig;

    /** Base cell size for multi-cell calculations */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration")
    float BaseCellSize = 64.0f;

    /** Padding between icon and border */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration")
    float IconPadding = 4.0f;

    //==================================================================
    // State
    //==================================================================

    /** Current slot data */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    FSuspenseCoreSlotUIData CurrentSlotData;

    /** Current item data */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    FSuspenseCoreItemUIData CurrentItemData;

    /** Is slot selected */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    bool bIsSelected = false;

    /** Is slot highlighted */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    bool bIsHighlighted = false;

    /** Is slot locked */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    bool bIsLocked = false;

private:
    //==================================================================
    // Internal
    //==================================================================

    /** Owning container widget */
    TWeakObjectPtr<USuspenseCoreEquipmentWidget> OwningContainer;

    /** Update visual size based on config */
    void UpdateSlotVisualSize();

    /** Update icon display for current item */
    void UpdateItemIconDisplay();

    /** Update empty slot icon */
    void UpdateEmptySlotIcon();

    /** Apply rarity border color */
    void ApplyRarityBorder(const FGameplayTag& RarityTag);
};
```

---

## 6. Multi-Cell Item Support

### 6.1 Visual Sizing Strategy

Equipment slots have **variable visual sizes** based on maximum expected item:

| Slot Type | Max Item Size | Slot Visual Size |
|-----------|---------------|------------------|
| PrimaryWeapon | 2x5 (rifle) | 2x5 cells |
| SecondaryWeapon | 2x3 (SMG) | 2x3 cells |
| Holster | 1x2 (pistol) | 1x2 cells |
| Scabbard | 1x2 (knife) | 1x2 cells |
| Headwear | 2x2 (helmet) | 2x2 cells |
| Earpiece | 1x1 (comtac) | 1x1 cell |
| Eyewear | 1x1 (NVG) | 1x1 cell |
| FaceCover | 1x2 (mask) | 1x2 cells |
| BodyArmor | 3x3 (plate carrier) | 3x3 cells |
| TacticalRig | 2x3 (rig) | 2x3 cells |
| Backpack | 3x3 (backpack) | 3x3 cells |
| SecureContainer | 2x2 (gamma) | 2x2 cells |
| QuickSlot1-4 | 1x1 (consumable) | 1x1 cell |
| Armband | 1x1 (armband) | 1x1 cell |

### 6.2 Icon Sizing Within Slot

```cpp
void USuspenseCoreEquipmentSlotWidget::UpdateItemIconDisplay()
{
    if (!CurrentItemData.IsValid())
    {
        ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
        EmptySlotIcon->SetVisibility(ESlateVisibility::Visible);
        return;
    }

    // Show item icon, hide empty icon
    ItemIcon->SetVisibility(ESlateVisibility::Visible);
    EmptySlotIcon->SetVisibility(ESlateVisibility::Collapsed);

    // Calculate icon size based on item GridSize vs slot size
    FIntPoint ItemSize = CurrentItemData.GridSize;
    FIntPoint SlotSize = SlotConfig.SlotSize;

    // Scale item icon to fit within slot, maintaining aspect ratio
    float ItemAspect = (float)ItemSize.X / (float)ItemSize.Y;
    float SlotAspect = (float)SlotSize.X / (float)SlotSize.Y;

    FVector2D IconSize;
    float SlotPixelWidth = SlotSize.X * BaseCellSize - (IconPadding * 2);
    float SlotPixelHeight = SlotSize.Y * BaseCellSize - (IconPadding * 2);

    if (ItemAspect > SlotAspect)
    {
        // Item is wider - fit to width
        IconSize.X = SlotPixelWidth;
        IconSize.Y = SlotPixelWidth / ItemAspect;
    }
    else
    {
        // Item is taller - fit to height
        IconSize.Y = SlotPixelHeight;
        IconSize.X = SlotPixelHeight * ItemAspect;
    }

    // Apply size to image widget
    ItemIcon->SetDesiredSizeOverride(IconSize);
}
```

### 6.3 Slot Visual Size Calculation

```cpp
void USuspenseCoreEquipmentSlotWidget::UpdateSlotVisualSize()
{
    if (!RootSizeBox) return;

    // Calculate pixel size from cell count
    float Width = SlotConfig.SlotSize.X * BaseCellSize;
    float Height = SlotConfig.SlotSize.Y * BaseCellSize;

    RootSizeBox->SetWidthOverride(Width);
    RootSizeBox->SetHeightOverride(Height);
}
```

---

## 7. EventBus Integration

### 7.1 Event Flow

```
┌───────────────────────────────────────────────────────────────────────────┐
│                         EQUIPMENT UI EVENTS                                │
├───────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│  [UI Request Events] → Widget → EventBus → Equipment System               │
│  ─────────────────────────────────────────────────────────                │
│  SuspenseCore.Event.UIRequest.EquipItem                                   │
│  SuspenseCore.Event.UIRequest.UnequipItem                                 │
│  SuspenseCore.Event.UIRequest.SwapEquipment                               │
│  SuspenseCore.Event.UIRequest.QuickEquip                                  │
│                                                                           │
│  [System Events] → Equipment System → EventBus → Widget                   │
│  ─────────────────────────────────────────────────────────                │
│  SuspenseCore.Event.Equipment.ItemEquipped                                │
│  SuspenseCore.Event.Equipment.ItemUnequipped                              │
│  SuspenseCore.Event.Equipment.SlotChanged                                 │
│  SuspenseCore.Event.Equipment.ValidationFailed                            │
│                                                                           │
│  [UI State Events] → Widget → EventBus → Other Widgets                    │
│  ─────────────────────────────────────────────────────────                │
│  SuspenseCore.Event.UIContainer.Equipment.Opened                          │
│  SuspenseCore.Event.UIContainer.Equipment.Closed                          │
│  SuspenseCore.Event.UIContainer.Equipment.SlotHovered                     │
│  SuspenseCore.Event.UIContainer.Equipment.SlotSelected                    │
│                                                                           │
└───────────────────────────────────────────────────────────────────────────┘
```

### 7.2 Widget Event Subscription

```cpp
void USuspenseCoreEquipmentWidget::SetupEventSubscriptions()
{
    USuspenseCoreEventBus* EventBus = GetEventBus();
    if (!EventBus) return;

    // Subscribe to equipment change events
    FSuspenseCoreSubscriptionHandle Handle;

    // Equipment item changed
    Handle = EventBus->SubscribeNative(
        TAG_SuspenseCore_Event_Equipment_ItemEquipped,
        this,
        FSuspenseCoreNativeEventCallback::CreateUObject(
            this, &USuspenseCoreEquipmentWidget::OnEquipmentItemEquipped),
        ESuspenseCoreEventPriority::Normal
    );
    SubscriptionHandles.Add(Handle);

    // Equipment item unequipped
    Handle = EventBus->SubscribeNative(
        TAG_SuspenseCore_Event_Equipment_ItemUnequipped,
        this,
        FSuspenseCoreNativeEventCallback::CreateUObject(
            this, &USuspenseCoreEquipmentWidget::OnEquipmentItemUnequipped),
        ESuspenseCoreEventPriority::Normal
    );
    SubscriptionHandles.Add(Handle);

    // Validation failed (for error feedback)
    Handle = EventBus->SubscribeNative(
        TAG_SuspenseCore_Event_Equipment_ValidationFailed,
        this,
        FSuspenseCoreNativeEventCallback::CreateUObject(
            this, &USuspenseCoreEquipmentWidget::OnEquipmentValidationFailed),
        ESuspenseCoreEventPriority::Normal
    );
    SubscriptionHandles.Add(Handle);
}

void USuspenseCoreEquipmentWidget::OnEquipmentItemEquipped(
    FGameplayTag EventTag,
    const FSuspenseCoreEventData& EventData)
{
    // Extract slot info
    EEquipmentSlotType SlotType = static_cast<EEquipmentSlotType>(
        EventData.GetInt(TEXT("SlotType")));

    // Refresh specific slot
    int32 SlotIndex = SlotTypeToIndex(SlotType);
    RefreshSlot(SlotIndex);
}
```

---

## 8. GameplayTags to Add

### 8.1 Config/DefaultGameplayTags.ini

```ini
; =============================================================================
; EQUIPMENT UI EVENTS
; =============================================================================

; UI Request Events (Widget → System)
+GameplayTagList=(Tag="SuspenseCore.Event.UIRequest.EquipItem",DevComment="Request to equip item from inventory")
+GameplayTagList=(Tag="SuspenseCore.Event.UIRequest.UnequipItem",DevComment="Request to unequip item to inventory")
+GameplayTagList=(Tag="SuspenseCore.Event.UIRequest.SwapEquipment",DevComment="Request to swap two equipped items")
+GameplayTagList=(Tag="SuspenseCore.Event.UIRequest.QuickEquip",DevComment="Request quick-equip to best slot")

; Equipment System Events (System → Widget)
+GameplayTagList=(Tag="SuspenseCore.Event.Equipment.ItemEquipped",DevComment="Item was equipped successfully")
+GameplayTagList=(Tag="SuspenseCore.Event.Equipment.ItemUnequipped",DevComment="Item was unequipped successfully")
+GameplayTagList=(Tag="SuspenseCore.Event.Equipment.SlotChanged",DevComment="Equipment slot state changed")
+GameplayTagList=(Tag="SuspenseCore.Event.Equipment.ValidationFailed",DevComment="Equipment validation failed")

; UI Container Events
+GameplayTagList=(Tag="SuspenseCore.Event.UIContainer.Equipment.Opened",DevComment="Equipment panel opened")
+GameplayTagList=(Tag="SuspenseCore.Event.UIContainer.Equipment.Closed",DevComment="Equipment panel closed")
+GameplayTagList=(Tag="SuspenseCore.Event.UIContainer.Equipment.SlotHovered",DevComment="Equipment slot hovered")
+GameplayTagList=(Tag="SuspenseCore.Event.UIContainer.Equipment.SlotSelected",DevComment="Equipment slot selected")
```

### 8.2 Native Tags Header

Create/extend: `Source/BridgeSystem/Public/SuspenseCore/Events/UI/SuspenseCoreEquipmentUIEvents.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

// UI Request Events
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIRequest_EquipItem);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIRequest_UnequipItem);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIRequest_SwapEquipment);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIRequest_QuickEquip);

// Equipment System Events
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_Equipment_ItemEquipped);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_Equipment_ItemUnequipped);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_Equipment_SlotChanged);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_Equipment_ValidationFailed);

// UI Container Events
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIContainer_Equipment_Opened);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIContainer_Equipment_Closed);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIContainer_Equipment_SlotHovered);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIContainer_Equipment_SlotSelected);
```

---

## 9. Dependencies & Prerequisites

### 9.1 Module Dependencies

**UISystem.Build.cs** - Ensure dependencies:
```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core",
    "CoreUObject",
    "Engine",
    "UMG",
    "Slate",
    "SlateCore",
    "BridgeSystem",      // Interfaces, Types, EventBus
    "GAS",               // Optional: for attribute display
    "InputCore"          // For keyboard shortcuts
});
```

### 9.2 Required Files (Must Exist)

| File | Purpose | Status |
|------|---------|--------|
| `SuspenseCoreBaseContainerWidget.h/cpp` | Base class | ✅ Exists |
| `ISuspenseCoreUIDataProvider.h` | Provider interface | ✅ Exists |
| `ISuspenseCoreSlotUI.h` | Slot interface | ✅ Exists |
| `ISuspenseCoreDraggable.h` | Drag interface | ✅ Exists |
| `ISuspenseCoreDropTarget.h` | Drop interface | ✅ Exists |
| `SuspenseCoreUIContainerTypes.h` | Container types | ✅ Exists |
| `SuspenseCoreLoadoutSettings.h` | Loadout config | ✅ Exists |
| `SuspenseCoreDragDropHandler.h/cpp` | DragDrop subsystem | ✅ Exists |
| `SuspenseCoreEquipmentDataStore.h` | Equipment provider | ✅ Exists in EquipmentSystem |

### 9.3 Provider Implementation (EquipmentSystem)

The Equipment widget requires `ISuspenseCoreUIDataProvider` implementation in EquipmentSystem.

**Required methods in `USuspenseCoreEquipmentDataStore`:**
- `GetContainerUIData()` - Return equipment container with named slots
- `GetSlotUIData(int32)` - Return slot data by type index
- `GetItemUIDataAtSlot(int32)` - Return equipped item data
- `ValidateDrop()` - Validate equipment compatibility
- `OnUIDataChanged()` - Notify widget of changes

---

## 10. Testing Strategy

### 10.1 Unit Tests

1. **Slot Type Conversion**
   - `SlotTypeToIndex()` / `IndexToSlotType()` round-trip
   - Edge cases (None, MAX)

2. **Drop Validation**
   - Item type matches slot allowed types
   - Item type in disallowed list rejected
   - Conflict rules (tactical rig + body armor)

3. **Multi-Cell Sizing**
   - Icon sizing calculations
   - Slot visual size matches config

### 10.2 Integration Tests

1. **Provider Binding**
   - Bind to equipment provider
   - Receive data change notifications
   - Refresh displays correct items

2. **Drag-Drop Operations**
   - Inventory → Equipment (equip)
   - Equipment → Inventory (unequip)
   - Equipment → Equipment (swap)

3. **EventBus Integration**
   - Request events sent correctly
   - System events received and processed

### 10.3 Blueprint Widget Test

1. Create `WBP_EquipmentWidget` from `USuspenseCoreEquipmentWidget`
2. Configure slot positions in UMG Designer
3. Test with mock provider data

---

## Summary

### Files to Create (Priority Order)

1. **P0 - Core Structure**
   - `SuspenseCoreEquipmentWidget.h/cpp`
   - `SuspenseCoreEquipmentSlotWidget.h/cpp`
   - `SuspenseCoreEquipmentUIEvents.h/cpp` (native tags)

2. **P1 - Integration**
   - Extend `SuspenseCoreDragDropHandler.cpp`
   - Add equipment routing methods

3. **P2 - Polish**
   - `SuspenseCoreEquipmentDragVisualWidget.h/cpp`
   - Blueprint widget (WBP_Equipment)

### Key Architecture Points

| Point | Implementation |
|-------|----------------|
| **SSOT** | LoadoutManager for slot configs, EquipmentDataStore for runtime |
| **EventBus** | Native tags for all events, no RequestGameplayTag() |
| **Interfaces** | ISuspenseCoreUIDataProvider, ISuspenseCoreSlotUI |
| **Multi-Cell** | Variable slot sizes, icon scaling within bounds |
| **Naming** | `USuspenseCore*`, `FSuspenseCore*`, `ESuspenseCore*` |

---

**Document Version:** 1.0
**Ready for Implementation:** Yes
**Estimated Complexity:** Medium-High
