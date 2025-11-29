# Save/Load UI System

## Overview

The Save/Load UI system provides a full-featured interface for managing game saves. It consists of two main widgets:

1. **SuspenseCoreSaveSlotWidget** - Displays a single save slot
2. **SuspenseCoreSaveLoadMenuWidget** - Full menu with slot list and operations

## Widget Architecture

```
SuspenseCoreSaveLoadMenuWidget
├── TitleText ("SAVE GAME" / "LOAD GAME")
├── SlotsScrollBox
│   └── SlotsContainer (VerticalBox)
│       ├── SuspenseCoreSaveSlotWidget (Quick Save - slot 101)
│       ├── SuspenseCoreSaveSlotWidget (Auto Save - slot 100)
│       ├── SuspenseCoreSaveSlotWidget (Slot 0)
│       ├── SuspenseCoreSaveSlotWidget (Slot 1)
│       └── ... (up to 10 manual slots)
├── ActionButton (SAVE/LOAD)
├── DeleteButton
├── CloseButton
├── StatusText
└── ConfirmationOverlay
    ├── ConfirmationText
    ├── ConfirmButton
    └── CancelButton
```

## Save Slot Structure

Each slot displays:
- **Slot Name** - "Quick Save", "Auto Save", or "Slot 1-10"
- **Character Name** - From save header
- **Level** - Character level
- **Location** - Current map/area name
- **Timestamp** - When the save was created
- **Playtime** - Total play time
- **Delete Button** - For non-auto save slots

### Special Slots

| Slot Index | Name | Notes |
|------------|------|-------|
| 100 | Auto Save | Created automatically, cannot be manually deleted |
| 101 | Quick Save | Created with F5, loaded with F9 |
| 0-9 | Manual Slots | User-controlled save slots |

## Usage

### In Pause Menu

```cpp
// In SuspenseCorePauseMenuWidget

UPROPERTY()
USuspenseCoreSaveLoadMenuWidget* SaveLoadMenu;

void OnSaveButtonClicked()
{
    if (SaveLoadMenu)
    {
        SaveLoadMenu->ShowMenu(ESuspenseCoreSaveLoadMode::Save);
    }
}

void OnLoadButtonClicked()
{
    if (SaveLoadMenu)
    {
        SaveLoadMenu->ShowMenu(ESuspenseCoreSaveLoadMode::Load);
    }
}
```

### Standalone

```cpp
// Create widget
USuspenseCoreSaveLoadMenuWidget* Menu = CreateWidget<USuspenseCoreSaveLoadMenuWidget>(
    this, SaveLoadMenuClass);
Menu->AddToViewport(100);

// Show in Save mode
Menu->ShowMenu(ESuspenseCoreSaveLoadMode::Save);

// Or Load mode
Menu->ShowMenu(ESuspenseCoreSaveLoadMode::Load);

// Toggle
Menu->ToggleMenu(ESuspenseCoreSaveLoadMode::Load);
```

## Input Mode Handling

**CRITICAL:** The SaveLoadMenuWidget handles input mode internally:

1. **ShowMenu()** calls `SetUIInputMode()`:
   - Sets `FInputModeUIOnly`
   - Shows mouse cursor
   - Focuses the widget

2. **HideMenu()** calls `RestoreGameInputMode()`:
   - Sets `FInputModeGameOnly`
   - Hides mouse cursor

This ensures game inputs work after closing the menu.

## Modes

### Save Mode

- Shows all slots (empty and filled)
- Can overwrite existing saves (with confirmation)
- Cannot save to Auto Save slot (index 100)
- Creates new saves with generated name

### Load Mode

- Shows all slots
- Can only load from non-empty slots
- Confirmation dialog before loading
- Menu closes after successful load

## Confirmation Dialogs

Three types of confirmations:

1. **Overwrite Save** - When saving to non-empty slot
2. **Load Game** - Warning about losing unsaved progress
3. **Delete Save** - Before permanently deleting a save

## Events & Delegates

### C++ Delegates

```cpp
// In SuspenseCoreSaveLoadMenuWidget.h

// Menu closed
UPROPERTY(BlueprintAssignable)
FOnSaveLoadMenuClosed OnMenuClosed;

// Operation completed
UPROPERTY(BlueprintAssignable)
FOnSaveLoadOperationCompleted OnOperationCompleted;
```

### Blueprint Events

```cpp
// Called when menu is shown
UFUNCTION(BlueprintImplementableEvent)
void OnMenuShown(ESuspenseCoreSaveLoadMode Mode);

// Called when menu is hidden
UFUNCTION(BlueprintImplementableEvent)
void OnMenuHidden();

// Called when a slot is selected
UFUNCTION(BlueprintImplementableEvent)
void OnSlotSelectedEvent(int32 SlotIndex, bool bIsEmpty);

// Called when operation succeeds
UFUNCTION(BlueprintImplementableEvent)
void OnOperationSucceeded(ESuspenseCoreSaveLoadMode Mode, int32 SlotIndex);

// Called when operation fails
UFUNCTION(BlueprintImplementableEvent)
void OnOperationFailed(ESuspenseCoreSaveLoadMode Mode, int32 SlotIndex, const FString& ErrorMessage);
```

## GameplayTags

The following tags are used for UI events:

```ini
Event.UI.SaveLoadMenu.Opened
Event.UI.SaveLoadMenu.Closed
Event.UI.SaveLoadMenu.SlotSelected
Event.UI.SaveLoadMenu.SaveRequested
Event.UI.SaveLoadMenu.LoadRequested
Event.UI.SaveLoadMenu.DeleteRequested
Event.UI.SaveLoadMenu.ConfirmationShown
Event.UI.SaveLoadMenu.ConfirmationAccepted
Event.UI.SaveLoadMenu.ConfirmationCancelled
```

For save system events:

```ini
SuspenseCore.Event.Save.Started
SuspenseCore.Event.Save.Completed
SuspenseCore.Event.Save.Failed
SuspenseCore.Event.Load.Started
SuspenseCore.Event.Load.Completed
SuspenseCore.Event.Load.Failed
SuspenseCore.Event.Save.SlotDeleted
```

## Configuration (Blueprint)

In the Blueprint derived from `SuspenseCoreSaveLoadMenuWidget`:

| Property | Default | Description |
|----------|---------|-------------|
| SaveSlotWidgetClass | None | **REQUIRED:** Widget class for slot entries |
| SaveModeTitle | "SAVE GAME" | Title text in save mode |
| LoadModeTitle | "LOAD GAME" | Title text in load mode |
| bShowQuickSaveSlot | true | Show Quick Save slot in list |
| bShowAutoSaveSlot | true | Show Auto Save slot in list |
| NumManualSlots | 10 | Number of manual save slots |
| StatusMessageDuration | 3.0 | How long status messages display |

## Save Slot Widget Configuration

In the Blueprint derived from `SuspenseCoreSaveSlotWidget`:

| Property | Default | Description |
|----------|---------|-------------|
| EmptySlotText | "- Empty Slot -" | Text for empty slots |
| QuickSaveText | "Quick Save" | Name for quick save slot |
| AutoSaveText | "Auto Save" | Name for auto save slot |
| NormalColor | Dark gray | Background color normally |
| SelectedColor | Blue tint | Background when selected |
| HoveredColor | Light gray | Background on hover |

## File Locations

- **Headers:**
  - `Source/UISystem/Public/SuspenseCore/Widgets/SuspenseCoreSaveSlotWidget.h`
  - `Source/UISystem/Public/SuspenseCore/Widgets/SuspenseCoreSaveLoadMenuWidget.h`

- **Implementation:**
  - `Source/UISystem/Private/SuspenseCore/Widgets/SuspenseCoreSaveSlotWidget.cpp`
  - `Source/UISystem/Private/SuspenseCore/Widgets/SuspenseCoreSaveLoadMenuWidget.cpp`

- **Save System:**
  - `Source/BridgeSystem/Public/SuspenseCore/Save/SuspenseCoreSaveManager.h`
  - `Source/BridgeSystem/Public/SuspenseCore/Save/SuspenseCoreSaveTypes.h`

## Blueprint Setup

1. Create `WBP_SaveSlotWidget` extending `SuspenseCoreSaveSlotWidget`
   - Design the slot layout
   - Bind widgets: SlotButton, SlotNameText, CharacterNameText, etc.

2. Create `WBP_SaveLoadMenu` extending `SuspenseCoreSaveLoadMenuWidget`
   - Set `SaveSlotWidgetClass` to `WBP_SaveSlotWidget`
   - Design the menu layout
   - Bind widgets: TitleText, SlotsContainer, ActionButton, etc.

3. Reference in Pause Menu:
   - Add `WBP_SaveLoadMenu` to viewport or as child widget
   - Call `ShowMenu()` from Save/Load buttons

## Integration with PauseMenu

The PauseMenuWidget should:

1. Have buttons for Save and Load
2. Create/reference SaveLoadMenuWidget
3. Call appropriate ShowMenu() on button click

Example flow:
```
PauseMenu visible
    │
    ├─► User clicks "Save"
    │       │
    │       └─► SaveLoadMenu->ShowMenu(Save)
    │               │
    │               └─► PauseMenu stays visible but blocked
    │                       │
    │                       └─► User selects slot, saves
    │                               │
    │                               └─► SaveLoadMenu hides
    │                                       │
    │                                       └─► PauseMenu regains focus
    │
    └─► User clicks "Load"
            │
            └─► Same flow but Load mode
                    │
                    └─► After successful load, both menus close
```

## Best Practices

1. **Always set SaveSlotWidgetClass** - The menu won't work without it

2. **Test all slot states** - Empty, filled, special slots

3. **Handle save failures gracefully** - Show user-friendly messages

4. **Preserve input mode** - Menu handles this automatically, but verify

5. **Use confirmation dialogs** - Especially for destructive operations

6. **Subscribe to completion events** - To update other UI elements
