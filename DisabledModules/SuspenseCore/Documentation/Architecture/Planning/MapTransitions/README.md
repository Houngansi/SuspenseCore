# Map Transitions & Input Mode Management

## Critical Architecture Note

This document describes a **CRITICAL** aspect of SuspenseCore's map transition system that must be understood by all developers working on UI and game mode switching.

## The Problem

When transitioning between maps with different GameModes (e.g., MainMenu → GameMap), the **Input Mode does NOT automatically reset**. This leads to a common bug:

```
MainMenu (MenuPlayerController)
    │
    ├─► SetUIOnlyMode() ← Input Mode = UI Only
    │
    └─► Transition to GameMap
            │
            └─► SuspenseCorePlayerController::BeginPlay()
                    │
                    ├─► SetupEnhancedInput() ← MappingContext added ✓
                    ├─► CreatePauseMenu() ← Widget created ✓
                    │
                    └─► Input Mode = ??? ← STILL UI ONLY! ✗
                            │
                            └─► Game inputs don't work!
```

## The Solution

**ALWAYS explicitly set Input Mode in PlayerController::BeginPlay():**

```cpp
void ASuspenseCorePlayerController::BeginPlay()
{
    Super::BeginPlay();

    // CRITICAL: Set Input Mode to Game Only!
    // When transitioning from MainMenu (which uses UIOnly mode), the input mode
    // may still be in UI mode. We MUST explicitly set it to Game mode.
    if (IsLocalController())
    {
        FInputModeGameOnly InputMode;
        SetInputMode(InputMode);
        bShowMouseCursor = false;
    }

    SetupEnhancedInput();
    // ... rest of BeginPlay
}
```

## Implementation Details

### SuspenseCorePlayerController (Game Controller)

Located in: `Source/PlayerCore/Private/SuspenseCore/Core/SuspenseCorePlayerController.cpp`

The game controller MUST:
1. Set `FInputModeGameOnly` in BeginPlay
2. Hide mouse cursor (`bShowMouseCursor = false`)
3. Setup Enhanced Input with proper MappingContext
4. Create pause menu widget

### SuspenseCoreMenuPlayerController (Menu Controller)

Located in: `Source/PlayerCore/Private/SuspenseCore/Controllers/SuspenseCoreMenuPlayerController.cpp`

The menu controller:
1. Sets `FInputModeUIOnly` in BeginPlay (if `bUIOnlyModeOnStart` is true)
2. Shows mouse cursor
3. Handles ESC key for navigation

### GameMode Class References

When using `OpenLevel()` with `?game=` parameter, the path format is critical:

**WRONG (SoftObjectPath format):**
```
/Script/Engine.Blueprint'/Game/Blueprints/Core/BP_GameMode.BP_GameMode'
```

**CORRECT (Class path format):**
```
/Game/Blueprints/Core/BP_GameMode.BP_GameMode_C
```

The `NormalizeGameModeClassPath()` function in `SuspenseCoreMapTransitionSubsystem.cpp` handles this conversion automatically.

## Transition Flow

### MainMenu → GameMap

```
1. MainMenuWidget::TransitionToGame()
2. TransitionSubsystem->SetGameGameModePath(Path)
3. TransitionSubsystem->TransitionToGameMap(PlayerId, "GameMap")
4. OpenLevel("GameMap", true, "?PlayerId=XXX?game=/Game/.../BP_GameMode.BP_GameMode_C")
5. SuspenseCoreGameGameMode created
6. SuspenseCorePlayerController::BeginPlay()
   - Sets FInputModeGameOnly ← CRITICAL!
   - SetupEnhancedInput()
   - CreatePauseMenu()
7. Game inputs now work
```

### GameMap → MainMenu (via Pause Menu)

```
1. PauseMenuWidget::OnExitToLobbyButtonClicked()
2. SetGamePaused(false)
3. TransitionSubsystem->SetMenuGameModePath(Path)
4. TransitionSubsystem->TransitionToMainMenu("MainMenuMap")
5. OpenLevel("MainMenuMap", true, "?game=/Game/.../BP_MenuGameMode.BP_MenuGameMode_C")
6. SuspenseCoreMenuGameMode created
7. SuspenseCoreMenuPlayerController::BeginPlay()
   - Sets FInputModeUIOnly
   - Shows cursor
8. Menu UI works
```

## Common Pitfalls

### 1. Not Setting Input Mode

**Symptom:** Game inputs (WASD, Q for pause) don't work after transitioning from menu.

**Fix:** Always set `FInputModeGameOnly` in game controller's BeginPlay.

### 2. Wrong GameMode Path Format

**Symptom:** "Failed to find object 'Class /Game/...'" in logs.

**Fix:** Use `NormalizeGameModeClassPath()` or ensure path ends with `_C`.

### 3. MappingContext Not Added

**Symptom:** Enhanced Input bindings don't trigger.

**Fix:** Ensure `DefaultMappingContext` is set in Blueprint and `AddMappingContext()` is called.

### 4. UI Widgets Blocking Input

**Symptom:** Game input works sometimes but not always.

**Fix:** Check that UI widgets properly restore `FInputModeGameOnly` when hidden.

## Best Practices

1. **Always be explicit about Input Mode** - Never assume the mode will persist correctly.

2. **Use TransitionSubsystem** - Don't call `OpenLevel()` directly; use `TransitionSubsystem->TransitionToGameMap()` or `TransitionToMainMenu()`.

3. **Validate in BeginPlay** - Add logging to verify input mode and MappingContext state.

4. **Test both paths** - Always test:
   - Starting game directly on GameMap
   - Transitioning from MainMenu → GameMap
   - Transitioning back GameMap → MainMenu → GameMap

## Related Files

- `Source/PlayerCore/Private/SuspenseCore/Core/SuspenseCorePlayerController.cpp`
- `Source/PlayerCore/Private/SuspenseCore/Controllers/SuspenseCoreMenuPlayerController.cpp`
- `Source/BridgeSystem/Private/SuspenseCore/Subsystems/SuspenseCoreMapTransitionSubsystem.cpp`
- `Source/UISystem/Private/SuspenseCore/Widgets/SuspenseCorePauseMenuWidget.cpp`
- `Source/UISystem/Private/SuspenseCore/Widgets/SuspenseCoreMainMenuWidget.cpp`

## Debug Logging

When debugging input issues, look for these log messages:

```
=== SuspenseCorePlayerController::BeginPlay ===
=== Setting Input Mode to Game Only ===
=== SetupEnhancedInput ===
  VERIFIED: MappingContext is active in subsystem
=== HandlePauseGame called! ===
```

If `HandlePauseGame called!` is missing when pressing Q, the input isn't reaching the controller - check Input Mode.
