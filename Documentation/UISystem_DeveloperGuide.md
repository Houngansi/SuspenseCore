# UISystem Developer Guide
## SuspenseCore AAA MMO FPS - EventBus Architecture

**Version:** 1.0
**Last Updated:** December 2024
**Module:** UISystem
**Status:** Phase 4 - UI System Integration

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Single Source of Truth (SSOT)](#2-single-source-of-truth-ssot)
3. [Module Structure](#3-module-structure)
4. [Data Flow Architecture](#4-data-flow-architecture)
5. [EventBus Integration](#5-eventbus-integration)
6. [Interfaces Reference](#6-interfaces-reference)
7. [Widget Implementation Guide](#7-widget-implementation-guide)
8. [Equipment Widget Requirements](#8-equipment-widget-requirements)
9. [Task List & Issues](#9-task-list--issues)
10. [Code Examples](#10-code-examples)
11. [Magazine System Widgets](#11-magazine-system-widgets-tarkov-style)
12. [HUD Widgets (Weapon System)](#12-hud-widgets-weapon-system)
13. [Ammo Loading System (Testing Configuration)](#13-ammo-loading-system-testing-configuration)

---

## 1. Architecture Overview

### System Hierarchy

```
┌─────────────────────────────────────────────────────────────────────┐
│                        BridgeSystem (Foundation)                     │
│  ┌───────────────┐  ┌───────────────┐  ┌───────────────────────────┐│
│  │  EventBus     │  │  Interfaces   │  │  Types & Tags             ││
│  │  (Pub/Sub)    │  │  (Contracts)  │  │  (SSOT Data)              ││
│  └───────────────┘  └───────────────┘  └───────────────────────────┘│
└─────────────────────────────────────────────────────────────────────┘
                                │
        ┌───────────────────────┼───────────────────────┐
        ▼                       ▼                       ▼
┌───────────────┐      ┌───────────────┐      ┌───────────────┐
│InventorySystem│      │EquipmentSystem│      │   UISystem    │
│  (Provider)   │      │   (Provider)  │      │   (Consumer)  │
└───────────────┘      └───────────────┘      └───────────────┘
```

### Key Principles

| Principle | Implementation |
|-----------|----------------|
| **SSOT** | DataTable `FSuspenseCoreUnifiedItemData` + `FLoadoutConfiguration` |
| **EventBus** | All cross-system communication via GameplayTags |
| **Interfaces** | UISystem knows only about BridgeSystem interfaces |
| **ServiceLocator** | Static `Get()` methods on all Subsystems |

---

## 2. Single Source of Truth (SSOT)

### Item Data (What items exist)

**File:** `Source/BridgeSystem/Public/SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h`

```cpp
FSuspenseCoreUnifiedItemData : public FTableRowBase
├── Identity (ItemID, DisplayName, Description, Icon)
├── Classification (ItemType tag, Rarity tag, ItemTags)
├── Inventory Properties (GridSize, MaxStackSize, Weight)
├── Usage Flags (bIsEquippable, bIsConsumable, bCanDrop)
├── Equipment Config (EquipmentSlot tag, ActorClass, Sockets)
├── GAS Integration (AttributeSets, Effects, Abilities)
├── Weapon Config (FireModes, AmmoType, WeaponArchetype)
├── Armor Config (ArmorType, ArmorInitialization)
└── Ammo Config (AmmoCaliber, CompatibleWeapons)
```

**Usage:** All item data comes from this DataTable. NEVER hardcode item properties.

### Loadout Configuration (How slots/grids are configured)

**File:** `Source/BridgeSystem/Public/SuspenseCore/Types/Loadout/SuspenseCoreLoadoutSettings.h`

```cpp
FLoadoutConfiguration : public FTableRowBase
├── Identity (LoadoutID, LoadoutName, Description)
├── MainInventory (FSuspenseCoreLoadoutInventoryConfig)
│   ├── Width, Height (grid dimensions)
│   ├── MaxWeight
│   ├── AllowedItemTypes / DisallowedItemTypes
│   └── StartingItems
├── AdditionalInventories (TMap<FName, FSuspenseCoreLoadoutInventoryConfig>)
├── EquipmentSlots (TArray<FEquipmentSlotConfig>)
│   ├── SlotType (EEquipmentSlotType enum)
│   ├── SlotTag (FGameplayTag)
│   ├── AllowedItemTypes / DisallowedItemTypes
│   ├── AttachmentSocket
│   └── DefaultItemID
├── StartingEquipment (TMap<EEquipmentSlotType, FName>)
└── Weight Limits (MaxTotalWeight, OverweightThreshold)
```

### Equipment Slot Types (17 slots Tarkov-style)

```cpp
enum class EEquipmentSlotType : uint8
{
    // WEAPONS (4)
    PrimaryWeapon,      // AR, DMR, SR, Shotgun, LMG
    SecondaryWeapon,    // SMG, Shotgun, PDW
    Holster,            // Pistol, Revolver
    Scabbard,           // Melee Knife

    // HEAD GEAR (4)
    Headwear,           // Helmets, Hats
    Earpiece,           // Comtacs
    Eyewear,            // Glasses, NVG
    FaceCover,          // Masks, Balaclavas

    // BODY GEAR (2)
    BodyArmor,          // Plate carriers
    TacticalRig,        // Chest rigs

    // STORAGE (2)
    Backpack,           // Backpacks
    SecureContainer,    // Gamma/Kappa

    // QUICK ACCESS (4)
    QuickSlot1-4,       // Consumables, Meds, Grenades

    // SPECIAL (1)
    Armband             // Team identification
};
```

### Loadout Manager (Runtime access)

**File:** `Source/BridgeSystem/Public/SuspenseCore/Services/SuspenseCoreLoadoutManager.h`

```cpp
USuspenseCoreLoadoutManager : public UGameInstanceSubsystem
├── LoadLoadoutTable(UDataTable*)
├── GetLoadoutConfig(FName LoadoutID) -> FLoadoutConfiguration*
├── GetInventoryConfig(FName LoadoutID, FName InventoryName)
├── GetEquipmentSlots(FName LoadoutID) -> TArray<FEquipmentSlotConfig>
├── ApplyLoadoutToInventory(UObject*, FName LoadoutID)
├── ApplyLoadoutToEquipment(UObject*, FName LoadoutID)
└── RegisterDefaultLoadout(FName LoadoutID)
```

---

## 3. Module Structure

```
Source/UISystem/
├── UISystem.Build.cs                    # Dependencies: BridgeSystem, GAS, UMG
├── Public/
│   ├── UISystem.h                       # Module interface
│   └── SuspenseCore/
│       ├── Subsystems/
│       │   ├── SuspenseCoreUIManager.h              # [P0] Needs EventBus fix
│       │   └── SuspenseCoreDragDropHandler.h        # [P1] Equipment routing
│       ├── Actors/
│       │   └── SuspenseCoreCharacterPreviewActor.h  # [OK] EventBus correct
│       └── Widgets/
│           ├── Base/
│           │   └── SuspenseCoreBaseContainerWidget.h   # [P1] Native tags
│           ├── Common/
│           │   ├── SuspenseCoreButtonWidget.h
│           │   ├── SuspenseCoreMenuWidget.h
│           │   └── SuspenseCoreMenuButtonConfig.h
│           ├── ContextMenu/
│           │   └── SuspenseCoreContextMenuWidget.h     # [P2] EventBus promised
│           ├── DragDrop/
│           │   ├── SuspenseCoreDragDropOperation.h     # [P1] EventBus events
│           │   └── SuspenseCoreDragVisualWidget.h
│           ├── Inventory/
│           │   ├── SuspenseCoreInventoryWidget.h       # [P0] Rotation incomplete
│           │   └── SuspenseCoreInventorySlotWidget.h   # [P1] ISuspenseCoreSlotUI
│           ├── Layout/
│           │   ├── SuspenseCoreContainerScreenWidget.h # [P1] Native tags
│           │   ├── SuspenseCorePanelWidget.h           # [P2] Validation
│           │   └── SuspenseCorePanelSwitcherWidget.h   # [P0] Refresh empty
│           ├── Tooltip/
│           │   └── SuspenseCoreTooltipWidget.h
│           └── [Specialized Widgets]
│               ├── SuspenseCoreHUDWidget.h             # [P0] ISuspenseCoreHUDWidget
│               ├── SuspenseCoreAttributesWidget.h      # [OK]
│               ├── SuspenseCoreLevelWidget.h           # [OK]
│               ├── SuspenseCoreMainMenuWidget.h        # [OK]
│               ├── SuspenseCorePauseMenuWidget.h       # [P1] No EventBus
│               ├── SuspenseCoreSaveLoadMenuWidget.h    # [P1] No EventBus
│               └── [+8 more widgets]
└── Private/
    └── SuspenseCore/
        └── [Mirror structure with .cpp files]
```

---

## 4. Data Flow Architecture

### Inventory/Equipment → UI Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              DATA LAYER (SSOT)                              │
├─────────────────────────────────────────────────────────────────────────────┤
│  UDataTable<FSuspenseCoreUnifiedItemData>    (Item definitions)             │
│  UDataTable<FLoadoutConfiguration>           (Loadout configurations)       │
│                            │                                                │
│                            ▼                                                │
│              USuspenseCoreLoadoutManager (GameInstanceSubsystem)            │
│              USuspenseCoreDataManager (GameInstanceSubsystem)               │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                    ┌───────────────┴───────────────┐
                    ▼                               ▼
┌───────────────────────────────┐   ┌───────────────────────────────┐
│     PROVIDER LAYER            │   │     PROVIDER LAYER            │
├───────────────────────────────┤   ├───────────────────────────────┤
│ USuspenseCoreInventoryComponent│   │ USuspenseCoreEquipmentComponent│
│ Implements:                   │   │ Implements:                   │
│  - ISuspenseCoreInventory     │   │  - ISuspenseCoreEquipment     │
│  - ISuspenseCoreUIDataProvider│   │  - ISuspenseCoreUIDataProvider│
│                               │   │                               │
│ Location: InventorySystem     │   │ Location: EquipmentSystem     │
└───────────────────────────────┘   └───────────────────────────────┘
                    │                               │
                    └───────────────┬───────────────┘
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           UI LAYER (UISystem)                               │
├─────────────────────────────────────────────────────────────────────────────┤
│  USuspenseCoreUIManager (GameInstanceSubsystem)                             │
│    - FindProviderOnActor(Actor, ContainerType)                              │
│    - ShowContainerScreen(PC, PanelTag)                                      │
│                            │                                                │
│                            ▼                                                │
│  USuspenseCoreContainerScreenWidget                                         │
│    └── USuspenseCorePanelSwitcherWidget (tabs)                              │
│        └── USuspenseCorePanelWidget (container layout)                      │
│            ├── USuspenseCoreInventoryWidget      ← BindToProvider()         │
│            │   └── USuspenseCoreInventorySlotWidget[]                       │
│            └── USuspenseCoreEquipmentWidget      ← BindToProvider()  [NEW]  │
│                └── USuspenseCoreEquipmentSlotWidget[]                [NEW]  │
└─────────────────────────────────────────────────────────────────────────────┘
```

### EventBus Communication Flow

```
[InventoryComponent]                    [EquipmentComponent]
       │                                        │
       │  Publish:                              │  Publish:
       │  SuspenseCore.Event.Inventory.*        │  SuspenseCore.Event.Equipment.*
       │                                        │
       └────────────────┬───────────────────────┘
                        ▼
              ┌─────────────────────┐
              │  USuspenseCoreEventBus │
              │  (GameInstanceSubsystem) │
              └─────────────────────┘
                        │
        ┌───────────────┼───────────────┐
        ▼               ▼               ▼
   [UIManager]    [HUDWidget]    [ContainerWidget]
   Subscribe:     Subscribe:      Subscribe:
   UIFeedback.*   GAS.Attribute.* UIProvider.DataChanged
```

---

## 5. EventBus Integration

### Required Pattern for UI Widgets

```cpp
// Header
class UISYSTEM_API USuspenseCoreMyWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Lifecycle
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

protected:
    // EventBus Setup
    void SetupEventSubscriptions();
    void TeardownEventSubscriptions();
    USuspenseCoreEventBus* GetEventBus() const;

    // Event Handlers
    void OnMyEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

private:
    // Subscription Storage - MUST use correct type!
    TArray<FSuspenseCoreSubscriptionHandle> SubscriptionHandles;

    // Cached EventBus
    mutable TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;
};
```

```cpp
// Implementation
void USuspenseCoreMyWidget::SetupEventSubscriptions()
{
    USuspenseCoreEventBus* EventBus = GetEventBus();
    if (!EventBus) return;

    // Use NATIVE TAGS - never RequestGameplayTag() in runtime code
    FSuspenseCoreSubscriptionHandle Handle = EventBus->SubscribeNative(
        TAG_SuspenseCore_Event_UIProvider_DataChanged,  // ← Native tag
        this,
        FSuspenseCoreNativeEventCallback::CreateUObject(
            this, &USuspenseCoreMyWidget::OnMyEvent),
        ESuspenseCoreEventPriority::Normal
    );
    SubscriptionHandles.Add(Handle);
}

void USuspenseCoreMyWidget::TeardownEventSubscriptions()
{
    USuspenseCoreEventBus* EventBus = GetEventBus();
    if (!EventBus) return;

    for (const FSuspenseCoreSubscriptionHandle& Handle : SubscriptionHandles)
    {
        EventBus->Unsubscribe(Handle);
    }
    SubscriptionHandles.Empty();
}
```

### Native Tags Location

**File:** `Source/BridgeSystem/Public/SuspenseCore/Events/UI/SuspenseCoreUIEvents.h`

```cpp
// UI Provider Events
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIProvider);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIProvider_Registered);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIProvider_DataChanged);

// UI Container Events
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIContainer_Opened);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIContainer_Closed);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIContainer_DragStarted);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIContainer_DropCompleted);

// UI Request Events (UI → Game Logic)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIRequest_MoveItem);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIRequest_TransferItem);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIRequest_EquipItem);
```

### WRONG vs RIGHT

```cpp
// ❌ WRONG - Runtime string lookup
FGameplayTag Tag = FGameplayTag::RequestGameplayTag(
    FName("SuspenseCore.Event.UIContainer.Opened"));

// ✅ RIGHT - Native tag (compile-time)
#include "SuspenseCore/Events/UI/SuspenseCoreUIEvents.h"
FGameplayTag Tag = TAG_SuspenseCore_Event_UIContainer_Opened;
```

---

## 6. Interfaces Reference

### UI Interfaces (BridgeSystem)

| Interface | Location | Purpose | Implementers |
|-----------|----------|---------|--------------|
| `ISuspenseCoreUIDataProvider` | `Interfaces/UI/` | Data source for widgets | InventoryComponent, EquipmentComponent |
| `ISuspenseCoreUIContainer` | `Interfaces/UI/` | Container widget contract | BaseContainerWidget, InventoryWidget |
| `ISuspenseCoreUIWidget` | `Interfaces/UI/` | Base widget contract | SuspenseBaseWidget |
| `ISuspenseCoreSlotUI` | `Interfaces/UI/` | Slot widget contract | **MISSING in InventorySlotWidget** |
| `ISuspenseCoreDraggable` | `Interfaces/UI/` | Drag source | SuspenseBaseSlotWidget |
| `ISuspenseCoreDropTarget` | `Interfaces/UI/` | Drop target | SuspenseBaseSlotWidget |
| `ISuspenseCoreHUDWidget` | `Interfaces/UI/` | HUD coordinator | **MISSING in HUDWidget** |
| `ISuspenseCoreTooltip` | `Interfaces/UI/` | Tooltip display | SuspenseItemTooltipWidget |

### Key Interface: ISuspenseCoreUIDataProvider

```cpp
// Provider → Widget data flow
class ISuspenseCoreUIDataProvider
{
public:
    // Identity
    virtual FGuid GetProviderID() const = 0;
    virtual ESuspenseCoreContainerType GetContainerType() const = 0;
    virtual FGameplayTag GetContainerTypeTag() const = 0;

    // Container Data
    virtual FSuspenseCoreContainerUIData GetContainerUIData() const = 0;
    virtual FIntPoint GetGridSize() const = 0;
    virtual int32 GetSlotCount() const = 0;

    // Item Data
    virtual TArray<FSuspenseCoreItemUIData> GetAllItemUIData() const = 0;
    virtual bool GetItemUIDataAtSlot(int32 SlotIndex, FSuspenseCoreItemUIData& OutItem) const = 0;

    // Operations (routed to actual system)
    virtual bool RequestMoveItem(int32 FromSlot, int32 ToSlot, bool bRotate = false) = 0;
    virtual bool RequestTransferItem(int32 SlotIndex, const FGuid& TargetProviderID,
                                     int32 TargetSlot = INDEX_NONE, int32 Quantity = 0) = 0;

    // Validation
    virtual FSuspenseCoreDropValidation ValidateDrop(const FSuspenseCoreDragData& DragData,
                                                     int32 TargetSlot, bool bRotated = false) const = 0;

    // Change Notification
    virtual FOnSuspenseCoreUIDataChanged& OnUIDataChanged() = 0;
};
```

---

## 7. Widget Implementation Guide

### Container Widget Hierarchy

```
UUserWidget
└── USuspenseCoreBaseContainerWidget (implements ISuspenseCoreUIContainer)
    ├── USuspenseCoreInventoryWidget     (grid-based, multi-cell items)
    └── USuspenseCoreEquipmentWidget     [TO CREATE] (named slots)
```

### Base Container Widget Key Methods

```cpp
// Provider Binding (call when panel initializes)
void BindToProvider(TScriptInterface<ISuspenseCoreUIDataProvider> Provider);
void UnbindFromProvider();

// Refresh (called automatically on OnUIDataChanged)
void RefreshFromProvider();           // Full refresh
void RefreshSlot(int32 SlotIndex);    // Single slot
void RefreshItem(const FGuid& ID);    // Single item

// Drag-Drop (override in subclasses)
bool StartDragFromSlot(int32 SlotIndex, bool bSplitStack = false);
bool HandleDrop(const FSuspenseCoreDragData& DragData, int32 TargetSlot);

// Blueprint Events
K2_OnProviderBound();
K2_OnRefresh();
K2_OnSlotSelected(int32 SlotIndex);
K2_OnDropReceived(FSuspenseCoreDragData, int32 TargetSlot, bool bSuccess);
```

---

## 8. Equipment Widget Requirements

### Files to Create

```
Source/UISystem/Public/SuspenseCore/Widgets/Equipment/
├── SuspenseCoreEquipmentWidget.h
├── SuspenseCoreEquipmentSlotWidget.h
└── SuspenseCoreEquipmentDragVisualWidget.h

Source/UISystem/Private/SuspenseCore/Widgets/Equipment/
├── SuspenseCoreEquipmentWidget.cpp
├── SuspenseCoreEquipmentSlotWidget.cpp
└── SuspenseCoreEquipmentDragVisualWidget.cpp
```

### USuspenseCoreEquipmentWidget Design

```cpp
UCLASS(BlueprintType, Blueprintable)
class UISYSTEM_API USuspenseCoreEquipmentWidget : public USuspenseCoreBaseContainerWidget
{
    GENERATED_BODY()

public:
    // Named slots instead of grid
    virtual int32 GetSlotAtLocalPosition(const FVector2D& LocalPosition) const override;

protected:
    // Override for equipment-specific behavior
    virtual void CreateSlotWidgets_Implementation() override;
    virtual void UpdateSlotWidget_Implementation(int32 SlotIndex,
        const FSuspenseCoreSlotUIData& SlotData,
        const FSuspenseCoreItemUIData& ItemData) override;

    // Equipment slot configuration (from Loadout)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
    TArray<FEquipmentSlotConfig> SlotConfigs;

    // Slot widget instances (keyed by SlotType)
    UPROPERTY()
    TMap<EEquipmentSlotType, USuspenseCoreEquipmentSlotWidget*> SlotWidgetMap;
};
```

### Key Differences from Inventory

| Aspect | Inventory | Equipment |
|--------|-----------|-----------|
| Layout | Grid (10x5 cells) | Named slots (17 fixed) |
| Slot Indexing | `Row * Width + Col` | `EEquipmentSlotType` enum |
| Multi-cell | Yes (items span cells) | Yes (some slots larger) |
| Rotation | Toggle with R key | Per-slot configuration |
| Item Filter | Tag-based | Slot-specific AllowedItemTypes |
| Drag-to | Any valid slot | Slot matching item's EquipmentSlot tag |

### Equipment Slot Widget

```cpp
UCLASS(BlueprintType, Blueprintable)
class UISYSTEM_API USuspenseCoreEquipmentSlotWidget : public UUserWidget
    , public ISuspenseCoreSlotUI
    , public ISuspenseCoreDraggable
    , public ISuspenseCoreDropTarget
{
    GENERATED_BODY()

public:
    // ISuspenseCoreSlotUI
    virtual void InitializeSlot(int32 InSlotIndex) override;
    virtual void UpdateSlot(const FSuspenseCoreSlotUIData& SlotData,
                           const FSuspenseCoreItemUIData& ItemData) override;

    // Equipment-specific
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void SetSlotConfig(const FEquipmentSlotConfig& Config);

    UFUNCTION(BlueprintPure, Category = "Equipment")
    bool CanAcceptItem(const FGameplayTag& ItemEquipmentSlot) const;

protected:
    UPROPERTY(BlueprintReadOnly, Category = "Equipment")
    FEquipmentSlotConfig SlotConfig;

    UPROPERTY(meta = (BindWidget))
    UImage* ItemIcon;

    UPROPERTY(meta = (BindWidget))
    UImage* EmptySlotIcon;

    UPROPERTY(meta = (BindWidget))
    UBorder* RarityBorder;
};
```

### DragDropHandler Extension

**Add to:** `SuspenseCoreDragDropHandler.cpp`

```cpp
FSuspenseCoreDropResult USuspenseCoreDragDropHandler::HandleEquipmentToEquipment(
    const FSuspenseCoreDropRequest& Request)
{
    // 1. Get source and target providers
    auto SourceProvider = FindProviderByID(Request.SourceProviderID);
    auto TargetProvider = FindProviderByID(Request.TargetProviderID);

    // 2. If same provider - this is a SWAP within equipment
    if (Request.SourceProviderID == Request.TargetProviderID)
    {
        // Check if target slot is occupied
        FSuspenseCoreItemUIData TargetItem;
        if (TargetProvider->GetItemUIDataAtSlot(Request.TargetSlot, TargetItem))
        {
            // Swap items
            return ExecuteEquipmentSwap(Request);
        }
        else
        {
            // Move to empty slot
            return ExecuteEquipmentMove(Request);
        }
    }

    // 3. Cross-provider transfer
    return ExecuteDrop(Request);
}
```

---

## 8.5. Critical Implementation Notes

### ⚠️ CRITICAL: UUserWidget Method Name Conflicts

When implementing `ISuspenseCoreDropTarget` interface on widgets that inherit from `UUserWidget`, **DO NOT** use method names that conflict with UUserWidget's built-in Blueprint events.

#### The Problem

`UUserWidget` already defines these Blueprint events:
- `OnDragEnter(FGeometry, FDragDropEvent)`
- `OnDragLeave(FDragDropEvent)`
- `OnDrop(FGeometry, FDragDropEvent, UDragDropOperation*)`

If your interface defines methods with the **same names but different signatures**:
```cpp
// ❌ WRONG - Conflicts with UUserWidget::OnDragEnter
void OnDragEnter(UDragDropOperation* DragOperation);
```

The Blueprint system will **incorrectly marshal parameters**, causing:
- `FGeometry` data (slot coordinates like 64.0, 64.0) passed as `UDragDropOperation*`
- Crash with `EXCEPTION_ACCESS_VIOLATION reading address 0xFFFFFFFFFFFFFFFF`
- Garbage pointer values like `0x4280000042800000` (two floats)

#### The Solution

**Rename interface methods to avoid conflicts:**

```cpp
// ✅ CORRECT - ISuspenseCoreDropTarget.h
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|DragDrop")
void NotifyDragEnter(UDragDropOperation* DragOperation);  // ← NOT OnDragEnter!

UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|DragDrop")
void NotifyDragLeave();  // ← NOT OnDragLeave!
```

#### Implementation Pattern

```cpp
// Header
virtual void NotifyDragEnter_Implementation(UDragDropOperation* DragOperation) override;
virtual void NotifyDragLeave_Implementation() override;

// Implementation
void USuspenseCoreInventorySlotWidget::NotifyDragEnter_Implementation(UDragDropOperation* DragOperation)
{
    if (!DragOperation)
    {
        SetHighlightState(ESuspenseCoreUISlotState::DropTargetInvalid);
        return;
    }

    FSlotValidationResult Validation = CanAcceptDrop_Implementation(DragOperation);
    SetHighlightState(Validation.bIsValid
        ? ESuspenseCoreUISlotState::DropTargetValid
        : ESuspenseCoreUISlotState::DropTargetInvalid);
}
```

#### Affected Files

| File | Method | Status |
|------|--------|--------|
| `ISuspenseCoreDropTarget.h` | `NotifyDragEnter`, `NotifyDragLeave` | ✅ Fixed |
| `SuspenseCoreInventorySlotWidget.cpp` | `NotifyDragEnter_Implementation` | ✅ Fixed |
| `SuspenseCoreEquipmentSlotWidget.cpp` | `NotifyDragEnter_Implementation` | ✅ Fixed |
| `SuspenseBaseSlotWidget.cpp` | `NotifyDragEnter_Implementation` | ✅ Fixed |

### Safe Method Names for UUserWidget Interfaces

**Avoid these names (UUserWidget conflicts):**
- `OnDragEnter`, `OnDragLeave`, `OnDrop`
- `OnMouseEnter`, `OnMouseLeave`
- `OnFocusReceived`, `OnFocusLost`

**Use these naming patterns instead:**
- `NotifyDragEnter`, `NotifyDragLeave`
- `HandleDropTarget`, `ProcessDragEnter`
- `OnSlotDragEnter`, `OnSlotDragLeave`

---

## 9. Task List & Issues

### Priority 0 - Blockers (Must Fix)

| # | File | Line | Issue | Action |
|---|------|------|-------|--------|
| 1 | SuspenseCoreUIManager.cpp | 656 | SubscribeToEvents() EMPTY | Implement EventBus subscriptions |
| 2 | SuspenseCoreUIManager.h | 404 | FDelegateHandle wrong type | Change to FSuspenseCoreSubscriptionHandle |
| 3 | SuspenseCoreInventoryWidget.cpp | 878 | HandleRotationInput() not working | Implement rotation toggle |
| 4 | SuspenseCorePanelSwitcherWidget.cpp | 380 | RefreshActiveTabContent() empty | Implement refresh logic |
| 5 | SuspenseCoreHUDWidget.h | - | Missing ISuspenseCoreHUDWidget | Add interface + implement |

### Priority 1 - High Priority

| # | File | Issue | Action |
|---|------|-------|--------|
| 6 | SuspenseCoreBaseContainerWidget.cpp:463 | RequestGameplayTag() | Use native tag |
| 7 | SuspenseCoreBaseContainerWidget.cpp:499 | GetContainerTypeTag() returns empty | Implement conversion |
| 8 | SuspenseCoreInventorySlotWidget.h | Missing ISuspenseCoreSlotUI | Add interface |
| 9 | SuspenseCorePauseMenuWidget | No EventBus | Add pause state events |
| 10 | SuspenseCoreSaveLoadMenuWidget | No EventBus | Add save/load events |
| 11 | DragDropOperation | Only delegates | Add EventBus events |
| 12 | SuspenseCoreInventoryWidget.cpp:184-220 | No anchor resolution | Use SlotToAnchorMap |

### Priority 2 - Medium Priority

| # | File | Issue |
|---|------|-------|
| 13 | SuspenseCorePanelWidget.cpp:252 | No ContainerTypes validation |
| 14 | SuspenseCorePanelSwitcherWidget.cpp:425-430 | AddTab missing ContentWidgetClass |
| 15 | SuspenseCoreContextMenuWidget | EventBus documented but not used |
| 16 | ContainerScreenWidget/PanelSwitcherWidget | Hardcoded tag strings |

### Priority 0 - NEW CODE (Equipment)

| # | Task | Dependencies |
|---|------|--------------|
| E1 | Create USuspenseCoreEquipmentWidget | P1 issues fixed |
| E2 | Create USuspenseCoreEquipmentSlotWidget | P1 issues fixed |
| E3 | Extend DragDropHandler for Equipment | E1, E2 |
| E4 | Create USuspenseCoreEquipmentDragVisualWidget | E1 |
| E5 | Add Equipment GameplayTags | BridgeSystem |

---

## 10. Code Examples

### Getting Loadout Configuration

```cpp
// In any class with world context
USuspenseCoreLoadoutManager* LoadoutManager =
    USuspenseCoreLoadoutManager::Get(GetWorld());

if (LoadoutManager)
{
    // Get loadout config
    const FLoadoutConfiguration* Config =
        LoadoutManager->GetLoadoutConfig(TEXT("Default_Soldier"));

    if (Config)
    {
        // Get inventory settings
        const FSuspenseCoreLoadoutInventoryConfig& MainInv = Config->MainInventory;
        FIntPoint GridSize(MainInv.Width, MainInv.Height);

        // Get equipment slots
        for (const FEquipmentSlotConfig& SlotConfig : Config->EquipmentSlots)
        {
            UE_LOG(LogTemp, Log, TEXT("Slot: %s, Tag: %s"),
                *SlotConfig.DisplayName.ToString(),
                *SlotConfig.SlotTag.ToString());
        }
    }
}
```

### Binding Container Widget to Provider

```cpp
void USuspenseCorePanelWidget::BindToPlayerProviders()
{
    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this);
    if (!UIManager) return;

    for (USuspenseCoreBaseContainerWidget* Container : ContainerWidgets)
    {
        ESuspenseCoreContainerType ExpectedType = Container->GetExpectedContainerType();

        TScriptInterface<ISuspenseCoreUIDataProvider> Provider;

        switch (ExpectedType)
        {
            case ESuspenseCoreContainerType::Inventory:
                Provider = UIManager->GetPlayerInventoryProvider(PC);
                break;
            case ESuspenseCoreContainerType::Equipment:
                Provider = UIManager->GetPlayerEquipmentProvider(PC);
                break;
        }

        if (Provider)
        {
            Container->BindToProvider(Provider);
        }
    }
}
```

### Publishing EventBus Event

```cpp
void USuspenseCoreContainerScreenWidget::PublishScreenEvent(FGameplayTag EventTag)
{
    USuspenseCoreEventBus* EventBus = GetEventBus();
    if (!EventBus) return;

    FSuspenseCoreEventData EventData;
    EventData.Source = this;
    EventData.SetObject(TEXT("OwningPlayer"), GetOwningPlayer());

    EventBus->Publish(EventTag, EventData);
}
```

---

## 11. Magazine System Widgets (Tarkov-Style)

### Widget Hierarchy

```
UUserWidget
├── USuspenseCoreTooltipWidget
│   └── USuspenseCoreMagazineTooltipWidget   # Inherits base tooltip for animations
└── USuspenseCoreMagazineInspectionWidget    # Standalone inspection panel
```

### Magazine Tooltip (USuspenseCoreMagazineTooltipWidget)

Specialized tooltip for magazine items, inheriting from `USuspenseCoreTooltipWidget` for consistent animations.

**Key Features:**
- Inherits base tooltip show/hide animations
- Displays magazine capacity, current rounds, caliber
- Visual fill bar for quick capacity overview
- Rounds list preview

**Usage via UIManager:**
```cpp
// Show magazine tooltip
FSuspenseCoreMagazineTooltipData MagData;
MagData.DisplayName = FText::FromString("STANAG 30-Round");
MagData.CurrentRounds = 28;
MagData.MaxCapacity = 30;
// ... fill other data

USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this);
UIManager->ShowMagazineTooltip(MagData, ScreenPosition);
```

### Magazine Inspection (USuspenseCoreMagazineInspectionWidget)

Tarkov-style detailed magazine view with per-round slot visualization.

**BindWidget Requirements (ALL MANDATORY):**
- `CloseButton` - Close inspection panel
- `MagazineNameText` - Magazine display name
- `MagazineIcon` - Magazine icon image
- `CaliberText` - Caliber display text
- `RoundSlotsContainer` - WrapBox for round slot grid
- `RoundsCountText` - "27/30" format text
- `FillProgressBar` - Fill percentage bar
- `HintText` - "Drag ammo here to load"
- `DropZoneBorder` - Drop zone highlight border
- `LoadingProgressBar` - Loading operation progress
- `LoadingStatusText` - "Loading round 15..."

**EventBus Subscriptions:**
- `TAG_Equipment_Event_Ammo_LoadStarted` - Start loading animation
- `TAG_Equipment_Event_Ammo_RoundLoaded` - Complete slot loading
- `TAG_Equipment_Event_Ammo_LoadCompleted` - Finish loading operation
- `TAG_Equipment_Event_Ammo_LoadCancelled` - Cancel and reset

**Blueprint Events:**
```cpp
// Called when inspection opens/closes
UFUNCTION(BlueprintImplementableEvent)
void OnInspectionOpened();
void OnInspectionClosed();

// Called during loading/unloading
void OnRoundLoadingStarted(int32 SlotIndex);
void OnRoundLoadingCompleted(int32 SlotIndex);

// Called when user clicks a round slot
void OnRoundClicked(int32 SlotIndex);
```

### Creating Blueprint Child Widgets

1. **WBP_MagazineTooltip:**
   - Parent: `USuspenseCoreMagazineTooltipWidget`
   - Add visual design with BindWidget components
   - Configure animations in Blueprint

2. **WBP_MagazineInspector:**
   - Parent: `USuspenseCoreMagazineInspectionWidget`
   - Set `RoundSlotWidgetClass` to WBP_RoundSlot
   - Design layout matching Tarkov-style inspection

3. **WBP_RoundSlot:**
   - Simple UUserWidget with:
     - `RoundImage` (UImage) - Round icon
     - `SlotBorder` (UBorder) - Slot background/highlight

---

## Quick Reference

### File Locations Quick Lookup

| Need | File |
|------|------|
| Item Data Definition | `BridgeSystem/.../Types/Loadout/SuspenseCoreItemDataTable.h` |
| Loadout Configuration | `BridgeSystem/.../Types/Loadout/SuspenseCoreLoadoutSettings.h` |
| Loadout Manager | `BridgeSystem/.../Services/SuspenseCoreLoadoutManager.h` |
| UI Types | `BridgeSystem/.../Types/UI/SuspenseCoreUITypes.h` |
| UI Container Types | `BridgeSystem/.../Types/UI/SuspenseCoreUIContainerTypes.h` |
| UI Events Tags | `BridgeSystem/.../Events/UI/SuspenseCoreUIEvents.h` |
| UI Data Provider Interface | `BridgeSystem/.../Interfaces/UI/ISuspenseCoreUIDataProvider.h` |
| UI Container Interface | `BridgeSystem/.../Interfaces/UI/ISuspenseCoreUIContainer.h` |
| Magazine Inspection Interface | `BridgeSystem/.../Interfaces/UI/ISuspenseCoreMagazineInspectionWidget.h` |
| UI Manager | `UISystem/.../Subsystems/SuspenseCoreUIManager.h` |
| Base Container Widget | `UISystem/.../Widgets/Base/SuspenseCoreBaseContainerWidget.h` |
| Inventory Widget | `UISystem/.../Widgets/Inventory/SuspenseCoreInventoryWidget.h` |
| Magazine Tooltip Widget | `UISystem/.../Widgets/Tooltip/SuspenseCoreMagazineTooltipWidget.h` |
| Magazine Inspection Widget | `UISystem/.../Widgets/HUD/SuspenseCoreMagazineInspectionWidget.h` |
| DragDrop Handler | `UISystem/.../Subsystems/SuspenseCoreDragDropHandler.h` |
| Ammo Loading Native Tags | `EquipmentSystem/.../Tags/SuspenseCoreEquipmentNativeTags.h` |

### Service Locator Access

```cpp
// EventBus
USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(WorldContext);
USuspenseCoreEventBus* EventBus = EventManager->GetEventBus();

// UI Manager
USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(WorldContext);

// Loadout Manager
USuspenseCoreLoadoutManager* LoadoutManager = USuspenseCoreLoadoutManager::Get(WorldContext);

// Data Manager
USuspenseCoreDataManager* DataManager = USuspenseCoreDataManager::Get(WorldContext);

// DragDrop Handler
USuspenseCoreDragDropHandler* DragHandler = USuspenseCoreDragDropHandler::Get(WorldContext);
```

---

## 12. HUD Widgets (Weapon System)

### Widget Overview

```
Source/UISystem/
└── Widgets/HUD/
    ├── SuspenseCoreAmmoCounterWidget        # Ammo display + fire mode
    ├── SuspenseCoreReloadProgressWidget     # Reload progress + phase indicators
    └── SuspenseCoreMasterHUDWidget          # Main HUD coordinator
```

### AmmoCounterWidget (USuspenseCoreAmmoCounterWidget)

Tarkov-style ammo counter displaying magazine state, fire mode, and reserve ammo.

**Key Features:**
- Magazine rounds with fill bar (material-based progress)
- Chamber indicator (+1)
- Fire mode display (Auto/Semi/Burst)
- Reserve ammo count

#### CRITICAL: Dual EventBus Subscription Pattern

The widget must subscribe to **BOTH** EquipmentSystem and BridgeSystem tags for the same event type. This is required because:

1. **GAS abilities** (like `SwitchFireModeAbility`) publish on BridgeSystem tags
2. **EquipmentSystem components** publish on EquipmentSystem tags
3. **GAS cannot depend on EquipmentSystem** (DI principle)

```cpp
// SetupEventSubscriptions() - CORRECT pattern
void USuspenseCoreAmmoCounterWidget::SetupEventSubscriptions()
{
    // ═══════════════════════════════════════════════════════════════
    // WEAPON AMMO EVENTS - Dual subscription required!
    // ═══════════════════════════════════════════════════════════════

    // 1. EquipmentSystem tag (from MagazineComponent, EquipmentComponent)
    WeaponAmmoChangedHandle = EventBus->SubscribeNative(
        TAG_Equipment_Event_Weapon_AmmoChanged,
        this,
        FSuspenseCoreNativeEventCallback::CreateUObject(this, &ThisClass::OnWeaponAmmoChangedEvent),
        ESuspenseCoreEventPriority::Normal
    );

    // 2. BridgeSystem tag (from GAS fire abilities - ConsumeAmmo())
    WeaponAmmoChangedGASHandle = EventBus->SubscribeNative(
        SuspenseCoreTags::Event::Weapon::AmmoChanged,
        this,
        FSuspenseCoreNativeEventCallback::CreateUObject(this, &ThisClass::OnWeaponAmmoChangedEvent),
        ESuspenseCoreEventPriority::Normal
    );

    // ═══════════════════════════════════════════════════════════════
    // FIRE MODE EVENTS - Dual subscription required!
    // ═══════════════════════════════════════════════════════════════

    // 1. EquipmentSystem tag
    FireModeChangedHandle = EventBus->SubscribeNative(
        TAG_Equipment_Event_Weapon_FireModeChanged,
        this,
        FSuspenseCoreNativeEventCallback::CreateUObject(this, &ThisClass::OnFireModeChangedEvent),
        ESuspenseCoreEventPriority::Normal
    );

    // 2. BridgeSystem tag (from GAS SwitchFireModeAbility)
    FireModeChangedGASHandle = EventBus->SubscribeNative(
        SuspenseCoreTags::Event::Weapon::FireModeChanged,
        this,
        FSuspenseCoreNativeEventCallback::CreateUObject(this, &ThisClass::OnFireModeChangedEvent),
        ESuspenseCoreEventPriority::Normal
    );
}
```

**Tag Mapping Reference:**

| Event | EquipmentSystem Tag | BridgeSystem Tag |
|-------|---------------------|------------------|
| Ammo Changed | `SuspenseCore.Event.Equipment.Weapon.AmmoChanged` | `SuspenseCore.Event.Weapon.AmmoChanged` |
| Fire Mode Changed | `SuspenseCore.Event.Equipment.Weapon.FireModeChanged` | `SuspenseCore.Event.Weapon.FireModeChanged` |
| Reload Started | `SuspenseCore.Event.Equipment.Weapon.ReloadStart` | `SuspenseCore.Event.Weapon.ReloadStarted` |

#### Initial State Reading Pattern

When initializing with a weapon, read current state directly from components - don't wait for events:

```cpp
void USuspenseCoreAmmoCounterWidget::InitializeWithWeapon_Implementation(AActor* WeaponActor)
{
    // 1. Read ammo state from MagazineComponent
    if (USuspenseCoreMagazineComponent* MagComp = WeaponActor->FindComponentByClass<USuspenseCoreMagazineComponent>())
    {
        const FSuspenseCoreWeaponAmmoState& AmmoState = MagComp->GetWeaponAmmoState();
        CachedAmmoData.MagazineRounds = AmmoState.InsertedMagazine.CurrentRoundCount;
        CachedAmmoData.MagazineCapacity = AmmoState.InsertedMagazine.MaxCapacity;
        CachedAmmoData.bHasChamberedRound = AmmoState.ChamberedRound.IsChambered();
    }

    // 2. Read fire mode from weapon interface
    if (WeaponActor->Implements<USuspenseCoreWeapon>())
    {
        FGameplayTag CurrentFireMode = ISuspenseCoreWeapon::Execute_GetCurrentFireMode(WeaponActor);
        if (CurrentFireMode.IsValid())
        {
            CachedAmmoData.FireModeTag = CurrentFireMode;
            // Extract display name from tag (last segment after '.')
            CachedAmmoData.FireModeText = ExtractDisplayNameFromTag(CurrentFireMode);
        }
    }

    // 3. Now setup event subscriptions for future updates
    SetupEventSubscriptions();
    RefreshDisplay();
}
```

#### EventData Field Names

When handling fire mode events, use correct field names from broadcasters:

| Field | Source | Used By |
|-------|--------|---------|
| `FireModeTag` | All broadcasters | Tag string like "Weapon.FireMode.Auto" |
| `FireModeName` | SwitchFireModeAbility | Display name like "Auto" |
| `CurrentSpread` | FireModeProvider | Spread value (float) |

```cpp
// ❌ WRONG - "FireMode" field doesn't exist
FString FireModeStr = EventData.GetString(TEXT("FireMode"));

// ✅ CORRECT - Use actual field names
FString FireModeTagStr = EventData.GetString(TEXT("FireModeTag"));
FString FireModeDisplayName = EventData.GetString(TEXT("FireModeName"));
```

### ReloadProgressWidget (USuspenseCoreReloadProgressWidget)

Reload progress bar with three-phase indicators (Eject/Insert/Chamber).

**BindWidget Requirements:**
- `ProgressBar` - Main reload progress (material-based)
- `EjectPhaseIndicator` - Phase 1 indicator
- `InsertPhaseIndicator` - Phase 2 indicator
- `ChamberPhaseIndicator` - Phase 3 indicator

#### AnimNotify Integration for Phase Indicators

Phase indicators are driven by montage AnimNotify events, not timers. This ensures visual sync with animation.

**Ability Side (SuspenseCoreReloadAbility):**

```cpp
void USuspenseCoreReloadAbility::PlayReloadMontage()
{
    UAnimInstance* AnimInstance = GetAnimInstance();
    if (!AnimInstance) return;

    // Cache for cleanup
    CachedAnimInstance = AnimInstance;

    // Bind to montage AnimNotify events
    AnimInstance->OnPlayMontageNotifyBegin.AddDynamic(
        this, &USuspenseCoreReloadAbility::OnAnimNotifyBegin);

    // Play montage
    AnimInstance->Montage_Play(ReloadMontage);
}

void USuspenseCoreReloadAbility::OnAnimNotifyBegin(
    FName NotifyName,
    const FBranchingPointNotifyPayload& BranchingPointPayload)
{
    // Phase 1: Magazine Eject
    if (NotifyName == FName("Continue") || NotifyName == FName("MagOut"))
    {
        OnMagOutNotify();  // Broadcasts TAG_Equipment_Event_Reload_Phase1
    }
    // Phase 2: Magazine Insert
    else if (NotifyName == FName("ClipIn") || NotifyName == FName("MagIn"))
    {
        OnMagInNotify();   // Broadcasts TAG_Equipment_Event_Reload_Phase2
    }
    // Phase 3: Chamber
    else if (NotifyName == FName("Finalize") || NotifyName == FName("RackEnd"))
    {
        OnRackEndNotify(); // Broadcasts TAG_Equipment_Event_Reload_Phase3
    }
}

void USuspenseCoreReloadAbility::StopReloadMontage()
{
    // CRITICAL: Unbind to prevent dangling delegate
    if (CachedAnimInstance.IsValid())
    {
        CachedAnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(
            this, &USuspenseCoreReloadAbility::OnAnimNotifyBegin);
    }
    CachedAnimInstance.Reset();
}
```

**CRITICAL: Dynamic Delegate Pattern**

`OnPlayMontageNotifyBegin` is a `DECLARE_DYNAMIC_MULTICAST_DELEGATE` - use `AddDynamic`/`RemoveDynamic`:

```cpp
// ❌ WRONG - Compile error C2039
AnimInstance->OnPlayMontageNotifyBegin.AddUObject(this, &ThisClass::Handler);

// ✅ CORRECT - Dynamic delegates use AddDynamic
AnimInstance->OnPlayMontageNotifyBegin.AddDynamic(this, &ThisClass::OnAnimNotifyBegin);
```

**AnimNotify → Phase Mapping:**

| AnimNotify Name | Phase | Description |
|-----------------|-------|-------------|
| `Continue`, `MagOut`, `Eject` | Phase 1 | Magazine ejected |
| `ClipIn`, `MagIn`, `Insert` | Phase 2 | New magazine inserted |
| `Finalize`, `RackEnd`, `Chamber` | Phase 3 | Round chambered |

### Removed Widget: ReloadTimerWidget

`SuspenseCoreReloadTimerWidget` was removed as it duplicated `ReloadProgressWidget` functionality without phase indicators. All references cleaned from `MasterHUDWidget`.

### MasterHUDWidget Configuration

`SuspenseCoreMasterHUDWidget` coordinates all HUD elements with auto-hide config:

```cpp
// Config flags (EditDefaultsOnly)
bool bAutoHideAmmoCounter = true;      // Show only when weapon equipped
bool bAutoHideReloadProgress = true;   // Show only during reload
```

---

## 13. Ammo Loading System (Testing Configuration)

### Load Time Configuration

Magazine load/unload times are configured in `FSuspenseCoreMagazineData`:

**File:** `Source/BridgeSystem/Public/SuspenseCore/Types/Weapon/SuspenseCoreMagazineTypes.h`

```cpp
USTRUCT(BlueprintType)
struct FSuspenseCoreMagazineData
{
    // Time to load one round into magazine (seconds)
    // TODO: Restore to 0.5f for production - currently 0 for testing instant load
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine|Stats",
        meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float LoadTimePerRound = 0.0f;  // Production: 0.5f

    // Time to unload one round from magazine (seconds)
    // TODO: Restore to 0.3f for production - currently 0 for testing instant unload
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine|Stats",
        meta = (ClampMin = "0.0", ClampMax = "3.0"))
    float UnloadTimePerRound = 0.0f;  // Production: 0.3f
};
```

**Production Values:**
| Parameter | Testing | Production |
|-----------|---------|------------|
| `LoadTimePerRound` | 0.0f | 0.5f |
| `UnloadTimePerRound` | 0.0f | 0.3f |

Search for `TODO: Restore to` to find all testing overrides before release.

---

**Document maintained by:** SuspenseCore Development Team
**Architecture:** EventBus + DI + GameplayTags + GAS
**Last Updated:** January 2026
