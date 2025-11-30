# SuspenseCore UI Widgets Guide

This guide covers the core UI widgets for player registration, character creation, and class selection in SuspenseCore.

## Table of Contents

1. [Widget Architecture](#widget-architecture)
2. [USuspenseCoreRegistrationWidget](#ususpensecoreregistrationwidget)
3. [USuspenseCoreCharacterSelectWidget](#ususpensecorecharacterselectwidget)
4. [Blueprint Setup](#blueprint-setup)
5. [EventBus Integration](#eventbus-integration)
6. [Customization](#customization)
7. [USuspenseCoreHUDWidget](#ususpensecorehudwidget)

---

## Widget Architecture

SuspenseCore UI follows these principles:

- **Pure C++ widgets** with Blueprint bindings via `BindWidget` meta
- **EventBus integration** for decoupled communication
- **Repository pattern** for data persistence
- **Class-based character system** with GAS integration

### UI Flow

```
┌─────────────────────┐
│  Character Select   │
│  (existing chars)   │
└─────────┬───────────┘
          │
    ┌─────┴─────┐
    │           │
    ▼           ▼
┌───────┐  ┌────────────┐
│ Play  │  │ Create New │
│ Game  │  │ Character  │
└───────┘  └─────┬──────┘
                 │
                 ▼
         ┌───────────────┐
         │  Registration │
         │  + Class Pick │
         └───────────────┘
```

---

## USuspenseCoreRegistrationWidget

Widget for creating new player characters with class selection.

### Header Location

```
Source/UISystem/Public/SuspenseCore/Widgets/SuspenseCoreRegistrationWidget.h
```

### Required Bindings (BindWidget)

These elements **must** exist in your Blueprint widget:

| Widget Name | Type | Description |
|-------------|------|-------------|
| `DisplayNameInput` | `UEditableTextBox` | Player name input field |
| `CreateButton` | `UButton` | Submit button |

### Optional Bindings (BindWidgetOptional)

These elements are optional but recommended:

| Widget Name | Type | Description |
|-------------|------|-------------|
| `StatusText` | `UTextBlock` | Error/success messages |
| `TitleText` | `UTextBlock` | Widget title |
| `AdditionalFieldsContainer` | `UVerticalBox` | Container for extra fields |

### Class Selection Bindings

| Widget Name | Type | Description |
|-------------|------|-------------|
| `ClassSelectionContainer` | `UHorizontalBox` | Container for class buttons |
| `AssaultClassButton` | `UButton` | Select Assault class |
| `MedicClassButton` | `UButton` | Select Medic class |
| `SniperClassButton` | `UButton` | Select Sniper class |
| `SelectedClassNameText` | `UTextBlock` | Shows selected class name |
| `SelectedClassDescriptionText` | `UTextBlock` | Shows class description |

### Blueprint Widget Hierarchy Example

```
WBP_Registration (USuspenseCoreRegistrationWidget)
├── CanvasPanel
│   ├── Border (Background)
│   │   └── VerticalBox
│   │       ├── TitleText [TextBlock] - "Create Character"
│   │       │
│   │       ├── HorizontalBox (Name Input Row)
│   │       │   ├── TextBlock - "Name:"
│   │       │   └── DisplayNameInput [EditableTextBox]
│   │       │
│   │       ├── Spacer
│   │       │
│   │       ├── TextBlock - "Select Class:"
│   │       ├── ClassSelectionContainer [HorizontalBox]
│   │       │   ├── AssaultClassButton [Button]
│   │       │   │   └── TextBlock - "Assault"
│   │       │   ├── MedicClassButton [Button]
│   │       │   │   └── TextBlock - "Medic"
│   │       │   └── SniperClassButton [Button]
│   │       │       └── TextBlock - "Sniper"
│   │       │
│   │       ├── Border (Class Info Panel)
│   │       │   └── VerticalBox
│   │       │       ├── SelectedClassNameText [TextBlock]
│   │       │       └── SelectedClassDescriptionText [TextBlock]
│   │       │
│   │       ├── StatusText [TextBlock] - Error/Success messages
│   │       │
│   │       └── CreateButton [Button]
│   │           └── TextBlock - "CREATE CHARACTER"
│   │
│   └── AdditionalFieldsContainer [VerticalBox] (optional)
```

### Configuration Properties

```cpp
// Set in Blueprint Details panel under "SuspenseCore|Config"

/** Minimum display name length (default: 3) */
int32 MinDisplayNameLength = 3;

/** Maximum display name length (default: 32) */
int32 MaxDisplayNameLength = 32;

/** Auto-close after success (default: false) */
bool bAutoCloseOnSuccess = false;

/** Delay before auto-close in seconds (default: 2.0) */
float AutoCloseDelay = 2.0f;
```

### Public API

```cpp
// Attempt to create player with current inputs
UFUNCTION(BlueprintCallable)
void AttemptCreatePlayer();

// Check if inputs are valid
UFUNCTION(BlueprintCallable)
bool ValidateInput() const;

// Get entered name
UFUNCTION(BlueprintCallable)
FString GetEnteredDisplayName() const;

// Get selected class ID ("Assault", "Medic", "Sniper")
UFUNCTION(BlueprintCallable)
FString GetSelectedClassId() const;

// Programmatically select a class
UFUNCTION(BlueprintCallable)
void SelectClass(const FString& ClassId);

// Show error message
UFUNCTION(BlueprintCallable)
void ShowError(const FString& Message);

// Show success message
UFUNCTION(BlueprintCallable)
void ShowSuccess(const FString& Message);

// Clear all fields
UFUNCTION(BlueprintCallable)
void ClearInputFields();
```

### Events (Delegates)

```cpp
// Called on successful registration
UPROPERTY(BlueprintAssignable)
FSuspenseCoreOnRegistrationComplete OnRegistrationComplete;

// Called on registration failure
UPROPERTY(BlueprintAssignable)
FSuspenseCoreOnRegistrationError OnRegistrationError;
```

### Usage in Blueprint

```cpp
// In your HUD or GameMode Blueprint:

// 1. Create widget
UUserWidget* Widget = CreateWidget<USuspenseCoreRegistrationWidget>(
    GetOwningPlayer(),
    WBP_Registration_Class
);

// 2. Bind to events
USuspenseCoreRegistrationWidget* RegWidget = Cast<USuspenseCoreRegistrationWidget>(Widget);
RegWidget->OnRegistrationComplete.AddDynamic(this, &AMyHUD::HandleRegistrationComplete);
RegWidget->OnRegistrationError.AddDynamic(this, &AMyHUD::HandleRegistrationError);

// 3. Add to viewport
Widget->AddToViewport();
```

---

## USuspenseCoreCharacterSelectWidget

Widget for selecting existing characters or navigating to character creation.

### Header Location

```
Source/UISystem/Public/SuspenseCore/Widgets/SuspenseCoreCharacterSelectWidget.h
```

### Bindings

| Widget Name | Type | Required | Description |
|-------------|------|----------|-------------|
| `CharacterListScrollBox` | `UScrollBox` | Optional | Scrollable character list |
| `CharacterListBox` | `UVerticalBox` | Optional | Alternative list container |
| `CreateNewButton` | `UButton` | Optional | Create new character button |
| `CreateNewButtonText` | `UTextBlock` | Optional | Text for create button |
| `TitleText` | `UTextBlock` | Optional | Widget title |
| `StatusText` | `UTextBlock` | Optional | Status messages |
| `PlayButton` | `UButton` | Optional | Start game button |
| `PlayButtonText` | `UTextBlock` | Optional | Text for play button |
| `DeleteButton` | `UButton` | Optional | Delete character button |
| `DeleteButtonText` | `UTextBlock` | Optional | Text for delete button |

### Blueprint Widget Hierarchy Example

```
WBP_CharacterSelect (USuspenseCoreCharacterSelectWidget)
├── CanvasPanel
│   └── Border (Background)
│       └── VerticalBox
│           ├── TitleText [TextBlock] - "SELECT CHARACTER"
│           │
│           ├── CharacterListScrollBox [ScrollBox]
│           │   └── CharacterListBox [VerticalBox]
│           │       └── (Character entries added dynamically)
│           │
│           ├── StatusText [TextBlock] - "No characters" / info
│           │
│           └── HorizontalBox (Buttons)
│               ├── CreateNewButton [Button]
│               │   └── CreateNewButtonText [TextBlock]
│               ├── PlayButton [Button]
│               │   └── PlayButtonText [TextBlock]
│               └── DeleteButton [Button]
│                   └── DeleteButtonText [TextBlock]
```

### Configuration

```cpp
// Class for entry widgets (create in Blueprint)
UPROPERTY(EditAnywhere)
TSubclassOf<UUserWidget> CharacterEntryWidgetClass;

// Configurable text strings
FText Title = "SELECT CHARACTER";
FText CreateNewText = "CREATE NEW CHARACTER";
FText NoCharactersText = "No characters found...";
FText PlayText = "PLAY";
FText SelectCharacterText = "Select a character";
FText DeleteText = "DELETE";
```

### Public API

```cpp
// Reload character list from repository
UFUNCTION(BlueprintCallable)
void RefreshCharacterList();

// Get all character entries
UFUNCTION(BlueprintCallable)
TArray<FSuspenseCoreCharacterEntry> GetCharacterEntries() const;

// Select (confirm) a character - triggers OnCharacterSelected
UFUNCTION(BlueprintCallable)
void SelectCharacter(const FString& PlayerId);

// Highlight a character (visual selection)
UFUNCTION(BlueprintCallable)
void HighlightCharacter(const FString& PlayerId);

// Play with currently highlighted character
UFUNCTION(BlueprintCallable)
void PlayWithHighlightedCharacter();

// Delete a character
UFUNCTION(BlueprintCallable)
void DeleteCharacter(const FString& PlayerId);

// Request to create new character
UFUNCTION(BlueprintCallable)
void RequestCreateNewCharacter();
```

### Events

```cpp
// C++ delegates
UPROPERTY(BlueprintAssignable)
FOnCharacterSelectedDelegate OnCharacterSelectedDelegate;

UPROPERTY(BlueprintAssignable)
FOnCharacterHighlightedDelegate OnCharacterHighlightedDelegate;

UPROPERTY(BlueprintAssignable)
FOnCreateNewRequestedDelegate OnCreateNewRequestedDelegate;

UPROPERTY(BlueprintAssignable)
FOnCharacterDeletedDelegate OnCharacterDeletedDelegate;

// Blueprint implementable events
UFUNCTION(BlueprintImplementableEvent)
void OnCharacterSelected(const FString& PlayerId, const FSuspenseCoreCharacterEntry& Entry);

UFUNCTION(BlueprintImplementableEvent)
void OnCreateNewRequested();

UFUNCTION(BlueprintImplementableEvent)
void OnCharacterDeleted(const FString& PlayerId);

UFUNCTION(BlueprintImplementableEvent)
void OnCharacterListRefreshed(int32 CharacterCount);
```

---

## Blueprint Setup

### Step 1: Create Registration Widget Blueprint

1. **Content Browser** → Right-click → **User Interface** → **Widget Blueprint**
2. Name it `WBP_SuspenseCoreRegistration`
3. Open and go to **Graph** → **Class Settings**
4. Set **Parent Class** to `USuspenseCoreRegistrationWidget`
5. In **Designer**, add required widgets with exact names:
   - `DisplayNameInput` (EditableTextBox)
   - `CreateButton` (Button)

### Step 2: Create Character Select Widget Blueprint

1. Create Widget Blueprint named `WBP_SuspenseCoreCharacterSelect`
2. Set **Parent Class** to `USuspenseCoreCharacterSelectWidget`
3. Add widgets with matching names from bindings table

### Step 3: Create Character Entry Widget (Optional)

For custom character list entries:

1. Create Widget Blueprint named `WBP_CharacterEntry`
2. Set **Parent Class** to `USuspenseCoreCharacterEntryWidget`
3. Add required bindings:
   - `DisplayNameText` (TextBlock)
   - `LevelText` (TextBlock)
   - `SelectButton` (Button) or `EntryButton` (Button)

### Step 4: Configure in Character Select

1. Open `WBP_SuspenseCoreCharacterSelect`
2. In **Details** panel, set `CharacterEntryWidgetClass` to `WBP_CharacterEntry`

---

## EventBus Integration

Registration and character selection publish events via EventBus.

### Published Events

| Event Tag | When Published | Data |
|-----------|---------------|------|
| `SuspenseCore.Event.UI.Registration.Success` | Registration successful | PlayerData |
| `SuspenseCore.Event.UI.Registration.Failed` | Registration failed | ErrorMessage |
| `SuspenseCore.Event.UI.CharacterSelect.Selected` | Character confirmed | PlayerId |
| `SuspenseCore.Event.UI.CharacterSelect.Highlighted` | Character highlighted | PlayerId |
| `SuspenseCore.Event.UI.CharacterSelect.Deleted` | Character deleted | PlayerId |
| `SuspenseCore.Event.UI.CharacterSelect.CreateNew` | Create new requested | - |

### Subscribing to Events

```cpp
// In your C++ class
void AMyHUD::BeginPlay()
{
    Super::BeginPlay();

    if (USuspenseCoreEventBus* EventBus = GetEventBus())
    {
        // Subscribe to registration success
        EventBus->Subscribe(
            FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.Registration.Success")),
            FSuspenseCoreEventCallback::CreateUObject(this, &AMyHUD::OnRegistrationSuccess)
        );
    }
}

void AMyHUD::OnRegistrationSuccess(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
    // Handle successful registration
    UE_LOG(LogTemp, Log, TEXT("Player registered: %s"), *EventData.GetString("PlayerId"));
}
```

---

## Customization

### Custom Validation

Override in C++:

```cpp
// In your derived class
bool UMyRegistrationWidget::ValidateInput() const
{
    // Call parent validation
    if (!Super::ValidateInput())
    {
        return false;
    }

    // Add custom validation
    FString Name = GetEnteredDisplayName();
    if (Name.Contains(TEXT("admin"), ESearchCase::IgnoreCase))
    {
        return false;
    }

    return true;
}
```

### Custom Class Selection UI

For more than 3 classes, create dynamic buttons:

```cpp
void UMyRegistrationWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Get all available classes
    if (USuspenseCoreCharacterClassSubsystem* ClassSys = GetClassSubsystem())
    {
        TArray<USuspenseCoreCharacterClassData*> Classes = ClassSys->GetAllClasses();

        for (USuspenseCoreCharacterClassData* ClassData : Classes)
        {
            UButton* ClassButton = CreateClassButton(ClassData);
            ClassSelectionContainer->AddChild(ClassButton);
        }
    }
}
```

### Styling

Style widgets in Blueprint Designer:

1. Select widget in hierarchy
2. Modify **Appearance** section in Details
3. Use **Style** properties for buttons
4. Consider creating a **Widget Style** asset for consistency

---

## USuspenseCoreHUDWidget

In-game HUD widget displaying player vital stats (Health, Shield, Stamina).

### Header Location

```
Source/UISystem/Public/SuspenseCore/Widgets/SuspenseCoreHUDWidget.h
```

### Features

- Real-time GAS attribute updates via AttributeSet binding
- Smooth progress bar interpolation
- Color coding (critical health = red)
- EventBus integration for attribute changes
- Auto-bind to local player on construct

### Health Bindings

| Widget Name | Type | Description |
|-------------|------|-------------|
| `HealthProgressBar` | `UProgressBar` | Health progress bar |
| `HealthValueText` | `UTextBlock` | Current health (e.g., "75") |
| `MaxHealthValueText` | `UTextBlock` | Max health (e.g., "100") |
| `HealthText` | `UTextBlock` | Combined text (e.g., "75 / 100") |
| `HealthIcon` | `UImage` | Health icon |

### Shield Bindings

| Widget Name | Type | Description |
|-------------|------|-------------|
| `ShieldProgressBar` | `UProgressBar` | Shield progress bar |
| `ShieldValueText` | `UTextBlock` | Current shield |
| `MaxShieldValueText` | `UTextBlock` | Max shield |
| `ShieldText` | `UTextBlock` | Combined text |
| `ShieldIcon` | `UImage` | Shield icon |

### Stamina Bindings

| Widget Name | Type | Description |
|-------------|------|-------------|
| `StaminaProgressBar` | `UProgressBar` | Stamina progress bar |
| `StaminaValueText` | `UTextBlock` | Current stamina |
| `MaxStaminaValueText` | `UTextBlock` | Max stamina |
| `StaminaText` | `UTextBlock` | Combined text |
| `StaminaIcon` | `UImage` | Stamina icon |

### Blueprint Widget Hierarchy Example

```
WBP_SuspenseCoreHUD (USuspenseCoreHUDWidget)
├── CanvasPanel
│   └── VerticalBox (Bottom-Left Anchor)
│       │
│       ├── HorizontalBox (Health Row)
│       │   ├── HealthIcon [Image] - Heart icon
│       │   ├── HealthProgressBar [ProgressBar] - Green/Red
│       │   └── HealthText [TextBlock] - "75 / 100"
│       │
│       ├── HorizontalBox (Shield Row)
│       │   ├── ShieldIcon [Image] - Shield icon
│       │   ├── ShieldProgressBar [ProgressBar] - Blue
│       │   └── ShieldText [TextBlock] - "50 / 100"
│       │
│       └── HorizontalBox (Stamina Row)
│           ├── StaminaIcon [Image] - Lightning icon
│           ├── StaminaProgressBar [ProgressBar] - Yellow
│           └── StaminaText [TextBlock] - "80 / 100"
```

### Configuration Properties

```cpp
// Set in Blueprint Details panel under "SuspenseCore|HUD|Config"

/** Auto-bind to local player on construct (default: true) */
bool bAutoBindToLocalPlayer = true;

/** Enable smooth progress bar interpolation (default: true) */
bool bSmoothProgressBars = true;

/** Progress bar interpolation speed (default: 10.0) */
float ProgressBarInterpSpeed = 10.0f;

/** Threshold for critical health (default: 0.25) */
float CriticalHealthThreshold = 0.25f;

// Colors
FLinearColor HealthColorNormal = FLinearColor(0.2f, 0.8f, 0.2f, 1.0f);   // Green
FLinearColor HealthColorCritical = FLinearColor(0.9f, 0.1f, 0.1f, 1.0f); // Red
FLinearColor ShieldColor = FLinearColor(0.2f, 0.6f, 1.0f, 1.0f);         // Blue
FLinearColor StaminaColor = FLinearColor(1.0f, 0.8f, 0.2f, 1.0f);        // Yellow
```

### Public API

```cpp
// Bind to specific actor with ASC
UFUNCTION(BlueprintCallable)
void BindToActor(AActor* Actor);

// Unbind from current actor
UFUNCTION(BlueprintCallable)
void UnbindFromActor();

// Bind to local player's pawn
UFUNCTION(BlueprintCallable)
void BindToLocalPlayer();

// Force refresh all values
UFUNCTION(BlueprintCallable)
void RefreshAllValues();

// Getters
float GetCurrentHealth() const;
float GetMaxHealth() const;
float GetHealthPercent() const;
float GetCurrentShield() const;
float GetMaxShield() const;
float GetShieldPercent() const;
float GetCurrentStamina() const;
float GetMaxStamina() const;
float GetStaminaPercent() const;
```

### Blueprint Events

```cpp
// Called when health changes
UFUNCTION(BlueprintImplementableEvent)
void OnHealthChanged(float NewHealth, float MaxHealth, float OldHealth);

// Called when shield changes
UFUNCTION(BlueprintImplementableEvent)
void OnShieldChanged(float NewShield, float MaxShield, float OldShield);

// Called when stamina changes
UFUNCTION(BlueprintImplementableEvent)
void OnStaminaChanged(float NewStamina, float MaxStamina, float OldStamina);

// Called when health becomes critical (<25%)
UFUNCTION(BlueprintImplementableEvent)
void OnHealthCritical();

// Called when shield is broken (reaches 0)
UFUNCTION(BlueprintImplementableEvent)
void OnShieldBroken();
```

### Usage: Adding to PlayerController HUD

```cpp
// In your PlayerController or HUD Blueprint:

// 1. Create widget
UUserWidget* Widget = CreateWidget<USuspenseCoreHUDWidget>(
    this,  // PlayerController
    WBP_SuspenseCoreHUD_Class
);

// 2. Widget auto-binds to local player if bAutoBindToLocalPlayer=true
// Or manually bind:
USuspenseCoreHUDWidget* HUDWidget = Cast<USuspenseCoreHUDWidget>(Widget);
HUDWidget->BindToLocalPlayer();

// 3. Add to viewport
Widget->AddToViewport();
```

### Blueprint Setup Steps

1. **Create Widget Blueprint**:
   - Right-click → User Interface → Widget Blueprint
   - Name: `WBP_SuspenseCoreHUD`

2. **Set Parent Class**:
   - Graph → Class Settings → Parent Class: `USuspenseCoreHUDWidget`

3. **Add Progress Bars** (Designer):
   - Add 3 ProgressBars named: `HealthProgressBar`, `ShieldProgressBar`, `StaminaProgressBar`
   - Set Fill Color for each

4. **Add Text Blocks**:
   - `HealthText`, `ShieldText`, `StaminaText` for "current / max" display

5. **Configure in Details Panel**:
   - Set colors, interpolation speed, critical threshold

6. **Assign to HUD**:
   - In PlayerController BP, add to viewport on BeginPlay

---

## Character Classes Reference

Default classes provided by `USuspenseCoreCharacterClassSubsystem`:

| Class ID | Name | Role | Description |
|----------|------|------|-------------|
| `Assault` | Assault | Assault | Frontline fighter. +15% damage, +10% reload speed |
| `Medic` | Medic | Support | Team healer. +30% health regen, +15% shield regen |
| `Sniper` | Sniper | Recon | Long-range. +25% range, +20% accuracy |

Classes are defined in `DefaultGameplayTags.ini`:

```ini
+GameplayTagList=(Tag="SuspenseCore.Class.Assault",DevComment="Assault class")
+GameplayTagList=(Tag="SuspenseCore.Class.Medic",DevComment="Medic class")
+GameplayTagList=(Tag="SuspenseCore.Class.Sniper",DevComment="Sniper class")
```

---

## Troubleshooting

### Widget bindings not working

- Ensure widget names match exactly (case-sensitive)
- Check parent class is set correctly
- Verify widgets are direct children or in hierarchy

### Class selection not updating

- Check `ClassSelectionContainer` is bound
- Verify class buttons have correct names
- Check logs for EventBus errors

### Characters not loading

- Verify repository is configured
- Check `Saved/Players/` directory exists
- Review logs for file I/O errors

### EventBus events not firing

- Ensure EventManager subsystem is initialized
- Check event tags match exactly
- Verify subscription is bound before events fire
