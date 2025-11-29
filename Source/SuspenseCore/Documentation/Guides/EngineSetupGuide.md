# SuspenseCore - Engine Setup Guide

> Руководство по настройке SuspenseCore в Unreal Engine
> Версия: 1.0 | Дата: 2025-11-29

---

## 1. Подготовка проекта

### 1.1 Зависимости модулей

Убедитесь, что в вашем `.uproject` или `Build.cs` подключены модули:

```csharp
// YourGame.Build.cs
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core",
    "CoreUObject",
    "Engine",
    "InputCore",
    "GameplayAbilities",
    "GameplayTags",
    "GameplayTasks",

    // SuspenseCore модули
    "BridgeSystem",
    "PlayerCore",
    "GAS",
    "UISystem"
});
```

### 1.2 GameplayTags

**SSOT (Single Source of Truth):** Все теги игры хранятся в одном файле:

```
Source/SuspenseCore/Documentation/SuspenseCoreGameplayTags.ini
```

**ВАЖНО:** При создании новых файлов с тегами - сначала добавьте теги в SSOT файл!

**Подключение тегов:**

1. Скопируйте содержимое `SuspenseCoreGameplayTags.ini` в `Config/DefaultGameplayTags.ini`
2. Или импортируйте через **Project Settings** → **GameplayTags** → **Import Tags from File**

**Основные категории тегов:**

| Категория | Префикс | Описание |
|-----------|---------|----------|
| События SuspenseCore | `SuspenseCore.Event.*` | Новая архитектура |
| GAS Events | `Event.GAS.*` | Здоровье, щит, атрибуты |
| Progression | `Event.Progression.*` | Level up, XP, валюта |
| UI Events | `Event.UI.*` | Виджеты, регистрация |
| Equipment | `Equipment.*` | Legacy система экипировки |
| Items | `Item.*` | Предметы, оружие |
| Services | `Service.*` | Сервисы |

---

## 2. Создание GameMode

### 2.1 Blueprint GameMode

1. **Content Browser** → Right Click → **Blueprint Class**
2. Parent Class: `AGameModeBase` или ваш кастомный
3. Назовите: `BP_SuspenseCoreGameMode`

### 2.2 Настройка классов в GameMode

Откройте `BP_SuspenseCoreGameMode` → **Class Defaults**:

| Параметр | Значение |
|----------|----------|
| Player Controller Class | `BP_SuspenseCorePlayerController` |
| Player State Class | `BP_SuspenseCorePlayerState` |
| Default Pawn Class | `BP_SuspenseCoreCharacter` |

### 2.3 C++ GameMode (опционально)

```cpp
// MyGameMode.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MyGameMode.generated.h"

UCLASS()
class MYGAME_API AMyGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AMyGameMode();
};

// MyGameMode.cpp
#include "MyGameMode.h"
#include "SuspenseCore/Core/SuspenseCorePlayerController.h"
#include "SuspenseCore/Core/SuspenseCorePlayerState.h"
#include "SuspenseCore/Core/SuspenseCoreCharacter.h"

AMyGameMode::AMyGameMode()
{
    PlayerControllerClass = ASuspenseCorePlayerController::StaticClass();
    PlayerStateClass = ASuspenseCorePlayerState::StaticClass();
    DefaultPawnClass = ASuspenseCoreCharacter::StaticClass();
}
```

---

## 3. Создание Blueprint классов

### 3.1 BP_SuspenseCorePlayerState

1. **Content Browser** → Right Click → **Blueprint Class**
2. Parent: `ASuspenseCorePlayerState`
3. Назовите: `BP_SuspenseCorePlayerState`

**Настройка AttributeSets:**

В **Class Defaults**:

```
AttributeSetClass: USuspenseCoreAttributeSet

AdditionalAttributeSets:
  [0] USuspenseCoreShieldAttributeSet
  [1] USuspenseCoreMovementAttributeSet
  [2] USuspenseCoreProgressionAttributeSet
```

### 3.2 BP_SuspenseCorePlayerController

1. Parent: `ASuspenseCorePlayerController`
2. Назовите: `BP_SuspenseCorePlayerController`

### 3.3 BP_SuspenseCoreCharacter

1. Parent: `ASuspenseCoreCharacter`
2. Назовите: `BP_SuspenseCoreCharacter`
3. Настройте Mesh, Capsule, Movement по вкусу

---

## 4. Настройка Level Blueprint

### 4.1 Показ Registration Widget при старте

```
Event BeginPlay
    │
    ▼
Get Game Instance
    │
    ▼
Get Subsystem (USuspenseCoreEventManager)
    │
    ▼
[Branch] Is Valid?
    │
    ├─[True]─► Create Widget (WBP_SuspenseCoreRegistration)
    │              │
    │              ▼
    │         Add to Viewport
    │              │
    │              ▼
    │         Set Input Mode UI Only
    │              │
    │              ▼
    │         Show Mouse Cursor = true
    │
    └─[False]─► Print String ("EventManager not found!")
```

### 4.2 Blueprint код (нода)

```
// BeginPlay
USuspenseCoreEventManager* EventManager = GetGameInstance()->GetSubsystem<USuspenseCoreEventManager>();

if (EventManager)
{
    // Widget создастся и покажет форму регистрации
    UUserWidget* Widget = CreateWidget<USuspenseCoreRegistrationWidget>(GetWorld(), RegistrationWidgetClass);
    Widget->AddToViewport();

    // UI mode
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    PC->SetInputMode(FInputModeUIOnly());
    PC->bShowMouseCursor = true;
}
```

---

## 5. Создание UI Widget Blueprints

### 5.1 WBP_SuspenseCoreRegistration

1. **Content Browser** → Right Click → **User Interface** → **Widget Blueprint**
2. Parent: `USuspenseCoreRegistrationWidget`
3. Назовите: `WBP_SuspenseCoreRegistration`

**Designer разметка:**

```
[Canvas Panel]
├── [Vertical Box] (Centered)
│   ├── [Text Block] "Create Your Character"
│   │       Font Size: 24
│   │
│   ├── [Spacer] Height: 20
│   │
│   ├── [Editable Text Box]  ← Name: "DisplayNameInput"
│   │       Hint Text: "Enter display name..."
│   │       Is Variable: ✓
│   │
│   ├── [Spacer] Height: 10
│   │
│   ├── [Button]  ← Name: "CreateButton"
│   │   │   Is Variable: ✓
│   │   └── [Text Block] "Create Character"
│   │
│   ├── [Spacer] Height: 10
│   │
│   └── [Text Block]  ← Name: "StatusText"
│           Is Variable: ✓
│           Text: ""
│           Color: Red/Green (по состоянию)
```

**ВАЖНО:** Имена виджетов должны точно совпадать с `BindWidget` в C++:
- `DisplayNameInput` (UEditableTextBox)
- `CreateButton` (UButton)
- `StatusText` (UTextBlock)

### 5.2 WBP_SuspenseCorePlayerInfo

1. Parent: `USuspenseCorePlayerInfoWidget`
2. Назовите: `WBP_SuspenseCorePlayerInfo`

**Designer разметка:**

```
[Canvas Panel]
├── [Vertical Box]
│   ├── [Horizontal Box]
│   │   ├── [Text Block] "DisplayNameText"
│   │   └── [Text Block] "PlayerIdText"
│   │
│   ├── [Text Block] "LevelText"
│   │
│   ├── [Progress Bar] "XPProgressBar"
│   │
│   ├── [Text Block] "XPText"
│   │
│   ├── [Horizontal Box] Currency
│   │   ├── [Text Block] "SoftCurrencyText"
│   │   └── [Text Block] "HardCurrencyText"
│   │
│   ├── [Horizontal Box] Stats
│   │   ├── [Text Block] "KillsText"
│   │   ├── [Text Block] "DeathsText"
│   │   └── [Text Block] "KDRatioText"
│   │
│   ├── [Horizontal Box]
│   │   ├── [Text Block] "WinsText"
│   │   └── [Text Block] "MatchesText"
│   │
│   ├── [Text Block] "PlaytimeText"
│   │
│   └── [Button] "RefreshButton"
│       └── [Text Block] "Refresh"
```

---

## 6. Настройка Project Settings

### 6.1 Maps & Modes

**Project Settings** → **Maps & Modes**:

| Параметр | Значение |
|----------|----------|
| Default GameMode | `BP_SuspenseCoreGameMode` |
| Editor Startup Map | Ваша тестовая карта |
| Game Default Map | Ваша тестовая карта |

### 6.2 Game Instance (опционально)

Если нужен кастомный GameInstance:

**Project Settings** → **Maps & Modes** → **Game Instance Class**

```cpp
// MyGameInstance.h
UCLASS()
class UMyGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override
    {
        Super::Init();

        // USuspenseCoreEventManager создаётся автоматически
        // как GameInstanceSubsystem
    }
};
```

---

## 7. UI Flow - Меню и переходы

### 7.1 Путь сохранения данных

**Сохранения игроков хранятся в:**
```
[Project]/Saved/Players/[PlayerId].json
```

Например: `MyProject/Saved/Players/Player_ABC123.json`

### 7.2 Архитектура UI Flow (3 экрана)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           MAIN MENU MAP                                  │
│                                                                         │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                        START                                       │  │
│  │                          │                                         │  │
│  │         ┌────────────────┼────────────────┐                        │  │
│  │         │ No saves       │ Has saves      │                        │  │
│  │         ▼                ▼                │                        │  │
│  │  ┌──────────────┐  ┌──────────────────┐   │                        │  │
│  │  │ Registration │  │ Character Select │   │                        │  │
│  │  │  (Index 1)   │  │    (Index 0)     │   │                        │  │
│  │  │              │  │                  │   │                        │  │
│  │  │ [Name Input] │  │ [Player_1 Lv.5]◄─┼───┼─ Select                │  │
│  │  │ [Create]─────┼──│ [Player_2 Lv.2] │   │                        │  │
│  │  │              │  │ [+ Create New]──┼───┘                        │  │
│  │  └──────┬───────┘  └────────┬────────┘                            │  │
│  │         │ Success           │ Select                               │  │
│  │         └─────────┬─────────┘                                      │  │
│  │                   ▼                                                │  │
│  │         ┌─────────────────────────────┐                            │  │
│  │         │       Main Menu             │                            │  │
│  │         │        (Index 2)            │                            │  │
│  │         │  ┌───────────────────────┐  │                            │  │
│  │         │  │   Player Info Widget  │  │                            │  │
│  │         │  │   Name: Player_X      │  │                            │  │
│  │         │  │   Level: 5            │  │                            │  │
│  │         │  └───────────────────────┘  │                            │  │
│  │         │                              │                            │  │
│  │         │  [PLAY] ─────────────────────┼────────────► GAME MAP     │  │
│  │         │  [OPERATORS] (disabled)      │                            │  │
│  │         │  [SETTINGS] (disabled)       │                            │  │
│  │         │  [QUIT]                      │                            │  │
│  │         └─────────────────────────────┘                            │  │
│  └───────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────┘
```

**Логика:**
1. **Старт** → проверка `[Project]/Saved/Players/` на наличие `.json` файлов
2. **Есть сохранения** → Character Select (Index 0) - список персонажей + "Создать нового"
3. **Нет сохранений** → Registration (Index 1) - создание первого персонажа
4. **После регистрации** → Main Menu (Index 2) с данными игрока
5. **После выбора персонажа** → Main Menu (Index 2) с данными выбранного игрока

### 7.3 Карты игры

| Карта | GameMode | Описание |
|-------|----------|----------|
| `MainMenuMap` | `BP_MenuGameMode` | Главное меню, регистрация, выбор персонажа |
| `CharacterSelectMap` | `BP_MenuGameMode` | Выбор оперативника (future - отдельная карта) |
| `GameMap` | `BP_SuspenseCoreGameMode` | Игровой процесс |

### 7.4 Настройка Menu GameMode

**1. Создайте Blueprint:**
- Parent: `ASuspenseCoreMenuGameMode`
- Name: `BP_MenuGameMode`

**2. Настройки в Class Defaults:**

| Параметр | Значение |
|----------|----------|
| Main Menu Widget Class | `WBP_MainMenu` |
| Main Menu Map Name | `MainMenuMap` |
| Default Game Map Name | `GameMap` |
| Auto Create Main Menu | ✓ |

### 7.5 Настройка Menu PlayerController

**1. Создайте Blueprint:**
- Parent: `ASuspenseCoreMenuPlayerController`
- Name: `BP_MenuPlayerController`

**2. Настройки:**
- Show Cursor On Start: ✓
- UI Only Mode On Start: ✓
- Main Menu Map Name: `MainMenuMap`

### 7.6 Создание WBP_MainMenu (3 экрана)

**1. Создайте Widget Blueprint:**
- Parent: `USuspenseCoreMainMenuWidget`
- Name: `WBP_MainMenu`

**2. Designer структура (3 экрана):**

```
[Canvas Panel] (Full Screen)
├── [Image] "BackgroundImage"
│       Brush: Your background image
│       Size To Content: false
│       Anchors: Full stretch
│
├── [Vertical Box] (Centered)
│   │
│   ├── [Text Block] "GameTitleText"
│   │       Text: "SUSPENSE"
│   │       Font Size: 72
│   │
│   ├── [Spacer] Height: 50
│   │
│   ├── [Widget Switcher] "ScreenSwitcher"
│   │   │
│   │   │── [Index 0] Panel_CharacterSelect
│   │   │   └── WBP_CharacterSelect "CharacterSelectWidget"
│   │   │       (Parent: USuspenseCoreCharacterSelectWidget)
│   │   │
│   │   │── [Index 1] Panel_Registration
│   │   │   └── WBP_Registration "RegistrationWidget"
│   │   │       (Parent: USuspenseCoreRegistrationWidget)
│   │   │
│   │   └── [Index 2] Panel_MainMenu
│   │       │
│   │       ├── WBP_PlayerInfo "PlayerInfoWidget"
│   │       │   (Parent: USuspenseCorePlayerInfoWidget)
│   │       │
│   │       ├── [Spacer] Height: 30
│   │       │
│   │       └── [Vertical Box] Menu Buttons
│   │           ├── [Button] "PlayButton"
│   │           │   └── [Text Block] "PlayButtonText" = "PLAY"
│   │           │
│   │           ├── [Button] "OperatorsButton"
│   │           │   └── [Text Block] "OPERATORS"
│   │           │
│   │           ├── [Button] "SettingsButton"
│   │           │   └── [Text Block] "SETTINGS"
│   │           │
│   │           └── [Button] "QuitButton"
│   │               └── [Text Block] "QUIT"
│   │
│   └── [Spacer] Height: 20
│
└── [Text Block] "VersionText"
        Text: "v0.1.0 Alpha"
        Anchors: Bottom-Right
```

**ВАЖНО:** Имена виджетов должны совпадать с `BindWidget`:
- `ScreenSwitcher` (UWidgetSwitcher)
- `BackgroundImage` (UImage)
- `GameTitleText` (UTextBlock)
- `CharacterSelectWidget` (USuspenseCoreCharacterSelectWidget) - Index 0
- `RegistrationWidget` (USuspenseCoreRegistrationWidget) - Index 1
- `PlayerInfoWidget` (USuspenseCorePlayerInfoWidget) - Index 2
- `PlayButton`, `OperatorsButton`, `SettingsButton`, `QuitButton` (UButton)
- `PlayButtonText` (UTextBlock)
- `VersionText` (UTextBlock)

### 7.7 Создание WBP_CharacterSelect

**1. Создайте Widget Blueprint:**
- Parent: `USuspenseCoreCharacterSelectWidget`
- Name: `WBP_CharacterSelect`

**2. Designer структура:**

```
[Canvas Panel]
├── [Text Block] "TitleText"
│       Text: "SELECT CHARACTER"
│       Font Size: 32
│
├── [Scroll Box] "CharacterListScrollBox"
│       (динамически заполняется кнопками персонажей)
│       или
│   [Vertical Box] "CharacterListBox"
│
├── [Text Block] "StatusText"
│       Text: "" (показывается если нет персонажей)
│
└── [Button] "CreateNewButton"
    └── [Text Block] "CreateNewButtonText" = "CREATE NEW CHARACTER"
```

**ВАЖНО:** Имена виджетов:
- `TitleText` (UTextBlock)
- `CharacterListScrollBox` (UScrollBox) или `CharacterListBox` (UVerticalBox)
- `StatusText` (UTextBlock)
- `CreateNewButton` (UButton)
- `CreateNewButtonText` (UTextBlock)

### 7.8 Встраивание виджетов в WidgetSwitcher

**Шаги:**

1. В ScreenSwitcher создайте 3 слота
2. **Index 0:** Добавьте WBP_CharacterSelect, переименуйте в "CharacterSelectWidget"
3. **Index 1:** Добавьте WBP_Registration, переименуйте в "RegistrationWidget"
4. **Index 2:** Добавьте Panel с WBP_PlayerInfo и кнопками меню

**Важно:** Порядок слотов критичен! C++ код использует индексы:
- `CharacterSelectScreenIndex = 0`
- `RegistrationScreenIndex = 1`
- `MainMenuScreenIndex = 2`

### 7.9 Настройка MainMenuMap

**1. Создайте карту:**
- File → New Level → Empty Level
- Save as: `MainMenuMap`

**2. World Settings:**

| Параметр | Значение |
|----------|----------|
| GameMode Override | `BP_MenuGameMode` |

**3. Level Blueprint:**

Обычно не нужен - MenuGameMode сам создаёт виджеты.
Но если нужна кастомная логика:

```
Event BeginPlay
    │
    ▼
Get Game Mode (Cast to BP_MenuGameMode)
    │
    ▼
Get Main Menu Widget
    │
    ▼
[Your custom logic]
```

### 7.10 Переход из игры в меню

**В любом месте игры (например, Pause Menu):**

```cpp
// C++
ASuspenseCoreMenuGameMode::ReturnToMainMenu(this);

// Blueprint
Call "Return To Main Menu" on MenuPlayerController
// или
Open Level "MainMenuMap"
```

### 7.11 Input Action для Escape

**1. Добавьте в Project Settings → Input:**

```
Action Mappings:
  Escape
    - Keyboard: Escape
  Back
    - Gamepad: Special Right (Menu button)
```

**2. В MenuPlayerController:**

Escape автоматически вызывает `OnEscapePressedEvent` (Blueprint event)

### 7.10 Project Settings для Menu Flow

**Maps & Modes:**

| Параметр | Значение |
|----------|----------|
| Editor Startup Map | `MainMenuMap` |
| Game Default Map | `MainMenuMap` |
| Default GameMode | `BP_MenuGameMode` |

**Map Overrides (per-map):**

| Карта | GameMode |
|-------|----------|
| MainMenuMap | BP_MenuGameMode |
| CharacterSelectMap | BP_MenuGameMode |
| GameMap | BP_SuspenseCoreGameMode |

---

## 8. Тестирование

### 8.1 Быстрый тест регистрации

1. Откройте тестовую карту
2. Нажмите **Play**
3. Должна появиться форма регистрации
4. Введите имя (3-32 символа, буквы/цифры/_/-)
5. Нажмите "Create Character"
6. Проверьте `Saved/PlayerData/` на наличие JSON файла

### 8.2 Проверка EventManager

```cpp
// В любом Blueprint или C++
USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
if (Manager)
{
    USuspenseCoreEventBus* EventBus = Manager->GetEventBus();
    USuspenseCoreServiceLocator* Services = Manager->GetServiceLocator();

    // Всё работает!
}
```

### 8.3 Проверка AttributeSets

```cpp
// В PlayerState Blueprint
ASuspenseCorePlayerState* PS = Cast<ASuspenseCorePlayerState>(PlayerState);
if (PS && PS->GetAbilitySystemComponent())
{
    // Получить атрибуты
    const USuspenseCoreAttributeSet* BaseAttrs = PS->GetAttributeSet();
    float Health = BaseAttrs->GetHealth();

    // Shield
    const USuspenseCoreShieldAttributeSet* ShieldAttrs =
        PS->GetAbilitySystemComponent()->GetSet<USuspenseCoreShieldAttributeSet>();
    float Shield = ShieldAttrs->GetShield();
}
```

---

## 9. Debugging

### 9.1 Console Commands

```
// Показать статистику EventBus
SuspenseCore.EventBus.Stats

// Список зарегистрированных сервисов
SuspenseCore.Services.List

// Атрибуты игрока
showdebug abilitysystem
```

### 9.2 Логирование

```cpp
// Включить подробные логи
UE_LOG(LogSuspenseCore, Log, TEXT("..."));

// В DefaultEngine.ini
[Core.Log]
LogSuspenseCore=Verbose
```

### 9.3 Типичные проблемы

| Проблема | Решение |
|----------|---------|
| Widget не связывается | Проверьте имена виджетов (точное совпадение с BindWidget) |
| EventManager = nullptr | Убедитесь, что BridgeSystem модуль подключён |
| AttributeSet не найден | Добавьте в AdditionalAttributeSets в PlayerState |
| Репликация не работает | Проверьте GetLifetimeReplicatedProps |
| MainMenu не появляется | Проверьте GameMode Override в World Settings карты |
| Play button не работает | Проверьте что GameMapName в GameMode настроен |

---

## 10. Следующие шаги

После успешной настройки:

1. **Создайте GameplayEffects** для изменения атрибутов
2. **Создайте GameplayAbilities** для способностей
3. **Настройте Input** для управления
4. **Создайте HUD** для отображения атрибутов в игре
5. **Тестируйте multiplayer** для проверки репликации
6. **Реализуйте Character Select** для выбора оперативников

---

## 11. Структура файлов после настройки

```
Content/
├── Blueprints/
│   ├── Core/
│   │   ├── BP_SuspenseCoreGameMode.uasset      (для игрового процесса)
│   │   ├── BP_SuspenseCorePlayerState.uasset
│   │   ├── BP_SuspenseCorePlayerController.uasset
│   │   └── BP_SuspenseCoreCharacter.uasset
│   │
│   ├── Menu/
│   │   ├── BP_MenuGameMode.uasset              (для меню)
│   │   └── BP_MenuPlayerController.uasset
│   │
│   └── UI/
│       ├── WBP_MainMenu.uasset                 (главное меню)
│       ├── WBP_SuspenseCoreRegistration.uasset (регистрация)
│       └── WBP_SuspenseCorePlayerInfo.uasset   (инфо игрока)
│
└── Maps/
    ├── MainMenuMap.umap                        (карта меню)
    ├── CharacterSelectMap.umap                 (выбор персонажа - future)
    └── GameMap.umap                            (игровая карта)
```

---

*Документ создан: 2025-11-29*
*SuspenseCore Clean Architecture*
