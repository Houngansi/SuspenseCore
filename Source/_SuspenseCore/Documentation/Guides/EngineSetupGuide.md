# SuspenseCore - Engine Setup Guide

> Руководство по настройке SuspenseCore в Unreal Engine
> Версия: 1.1 | Дата: 2025-11-29

---

## Связанная документация

| Документ | Описание |
|----------|----------|
| **[UI/](UI/)** | **Полная документация по UI системе** |
| [UI/MainMenuSetup.md](UI/MainMenuSetup.md) | Настройка главного меню |
| [UI/PauseMenuSetup.md](UI/PauseMenuSetup.md) | Настройка меню паузы |
| [UI/SaveLoadMenuSetup.md](UI/SaveLoadMenuSetup.md) | Настройка Save/Load меню |
| [UI/InputModeHandling.md](UI/InputModeHandling.md) | **КРИТИЧНО:** Input Mode при переходах |
| [UI/WidgetBindings.md](UI/WidgetBindings.md) | Справочник BindWidget имён |

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

Виджет регистрации персонажа с выбором класса.

**Создание:**
1. **Content Browser** → Right Click → **User Interface** → **Widget Blueprint**
2. Parent Class: `USuspenseCoreRegistrationWidget`
3. Назовите: `WBP_SuspenseCoreRegistration`

---

#### 5.1.1 Таблица биндингов (BindWidget)

| Имя виджета | Тип | Обязательность | Описание |
|-------------|-----|----------------|----------|
| `DisplayNameInput` | UEditableTextBox | **REQUIRED** | Поле ввода имени персонажа |
| `CreateButton` | UButton | **REQUIRED** | Кнопка создания персонажа |
| `StatusText` | UTextBlock | Optional | Текст ошибок/успеха |
| `TitleText` | UTextBlock | Optional | Заголовок формы |
| `AdditionalFieldsContainer` | UVerticalBox | Optional | Контейнер для доп. полей |

**Выбор класса (все Optional):**

| Имя виджета | Тип | Описание |
|-------------|-----|----------|
| `ClassSelectionContainer` | UHorizontalBox | Контейнер кнопок классов |
| `AssaultClassButton` | UButton | Кнопка класса Штурмовик |
| `MedicClassButton` | UButton | Кнопка класса Медик |
| `SniperClassButton` | UButton | Кнопка класса Снайпер |
| `SelectedClassNameText` | UTextBlock | Название выбранного класса |
| `SelectedClassDescriptionText` | UTextBlock | Описание выбранного класса |

---

#### 5.1.2 Твоя текущая структура виджета

```
WBP_SuspenseCoreRegistration (USuspenseCoreRegistrationWidget)
├── [Canvas Panel]
│   └── [Border] ← AdditionalFieldsContainer (опционально)
│       │
│       ├── [Text Block] "TitleText"
│       │       Text: "Create Your Character"
│       │       Font Size: 24
│       │
│       ├── [Spacer] Height: 20
│       │
│       ├── [Editable Text Box] "DisplayNameInput" ← REQUIRED
│       │       Hint Text: "Введите имя персонажа..."
│       │       Is Variable: ✓
│       │
│       ├── [Spacer] Height: 15
│       │
│       ├── [Size Box]
│       │   └── [Button] "CreateButton" ← REQUIRED
│       │       │   Is Variable: ✓
│       │       │   Style: Ваш стиль кнопки
│       │       └── [Text Block] "TextBlock_Create"
│       │               Text: "CREATE CHARACTER"
│       │
│       ├── [Spacer] Height: 10
│       │
│       └── [Text Block] "StatusText"
│               Is Variable: ✓
│               Text: "" (пустой)
│               Visibility: Collapsed (по умолчанию)
```

---

#### 5.1.3 Добавление выбора класса персонажа

Чтобы добавить выбор класса, расширь структуру виджета:

```
WBP_SuspenseCoreRegistration
├── [Canvas Panel]
│   └── [Border / Vertical Box]
│       │
│       ├── [Text Block] "TitleText"
│       │       Text: "Создание персонажа"
│       │
│       ├── [Spacer] Height: 20
│       │
│       ├── [Editable Text Box] "DisplayNameInput"
│       │       Hint Text: "Введите имя персонажа..."
│       │
│       ├── [Spacer] Height: 20
│       │
│       │   ╔══════════════════════════════════════════════════════╗
│       │   ║         СЕКЦИЯ ВЫБОРА КЛАССА (НОВОЕ)                 ║
│       │   ╚══════════════════════════════════════════════════════╝
│       │
│       ├── [Text Block] "Выберите класс:"
│       │       Font Size: 16
│       │
│       ├── [Spacer] Height: 10
│       │
│       ├── [Horizontal Box] "ClassSelectionContainer" ← НОВЫЙ
│       │   │   Is Variable: ✓
│       │   │
│       │   ├── [Button] "AssaultClassButton"  ← НОВЫЙ
│       │   │   │   Is Variable: ✓
│       │   │   │   Padding: (10, 5)
│       │   │   └── [Vertical Box]
│       │   │       ├── [Image] (иконка штурмовика)
│       │   │       └── [Text Block] "ШТУРМОВИК"
│       │   │
│       │   ├── [Spacer] Width: 10
│       │   │
│       │   ├── [Button] "MedicClassButton"  ← НОВЫЙ
│       │   │   │   Is Variable: ✓
│       │   │   └── [Vertical Box]
│       │   │       ├── [Image] (иконка медика)
│       │   │       └── [Text Block] "МЕДИК"
│       │   │
│       │   ├── [Spacer] Width: 10
│       │   │
│       │   └── [Button] "SniperClassButton"  ← НОВЫЙ
│       │       │   Is Variable: ✓
│       │       └── [Vertical Box]
│       │           ├── [Image] (иконка снайпера)
│       │           └── [Text Block] "СНАЙПЕР"
│       │
│       ├── [Spacer] Height: 15
│       │
│       ├── [Border] (панель описания класса)
│       │   │   Background: (0.05, 0.05, 0.1, 0.8)
│       │   │   Padding: 15
│       │   │
│       │   └── [Vertical Box]
│       │       ├── [Text Block] "SelectedClassNameText"  ← НОВЫЙ
│       │       │       Is Variable: ✓
│       │       │       Font Size: 20
│       │       │       Text: "Штурмовик" (по умолчанию)
│       │       │
│       │       └── [Text Block] "SelectedClassDescriptionText"  ← НОВЫЙ
│       │               Is Variable: ✓
│       │               Font Size: 14
│       │               Text: "Сбалансированный боец..."
│       │               Auto Wrap Text: ✓
│       │
│       │   ╔══════════════════════════════════════════════════════╗
│       │   ║         КОНЕЦ СЕКЦИИ ВЫБОРА КЛАССА                   ║
│       │   ╚══════════════════════════════════════════════════════╝
│       │
│       ├── [Spacer] Height: 20
│       │
│       ├── [Size Box]
│       │   └── [Button] "CreateButton"
│       │       └── [Text Block] "CREATE CHARACTER"
│       │
│       ├── [Spacer] Height: 10
│       │
│       └── [Text Block] "StatusText"
│               Text: ""
```

---

#### 5.1.4 Настройка стилей кнопок классов

**Обычное состояние:**
```
Background Color: (0.1, 0.1, 0.15, 1.0)
Border: 2px solid (0.3, 0.3, 0.4, 1.0)
```

**Выбранное состояние (Hovered/Selected):**
```
Background Color: (0.0, 0.4, 0.8, 1.0)  // Синий
Border: 2px solid (0.2, 0.6, 1.0, 1.0)
```

**Blueprint логика подсветки:**
C++ автоматически вызывает `UpdateClassSelectionUI()` при клике на кнопку класса.
Для визуальной подсветки используй Button Style или меняй цвет в Blueprint.

---

#### 5.1.5 Описания классов

| ClassId | Название | Описание (русский) |
|---------|----------|-------------------|
| `Assault` | Штурмовик | Сбалансированный боец передовой линии. +15% к урону, +10% к скорости перезарядки. |
| `Medic` | Медик | Поддержка команды. +30% регенерация HP, +15% регенерация щита, повышенная мобильность. |
| `Sniper` | Снайпер | Дальнобойный стрелок. +25% дальность, +20% точность, но низкая выживаемость. |

---

#### 5.1.6 Валидация перед созданием

C++ автоматически проверяет:
- Имя: 3-32 символа (настраивается в Class Defaults)
- Разрешённые символы: буквы, цифры, пробелы, `_`, `-`
- Класс должен быть выбран (если кнопки присутствуют)

При ошибке текст появится в `StatusText` красным цветом.

---

#### 5.1.7 События (Delegates)

Подпишись на события в Blueprint или C++:

```cpp
// C++ пример
RegistrationWidget->OnRegistrationComplete.AddDynamic(
    this, &AMyHUD::HandleRegistrationComplete);

RegistrationWidget->OnRegistrationError.AddDynamic(
    this, &AMyHUD::HandleRegistrationError);
```

```
// Blueprint пример
Event OnRegistrationComplete (PlayerData)
    → Show Success Message
    → Switch to Main Menu Screen

Event OnRegistrationError (ErrorMessage)
    → Show Error in StatusText
```

---

#### 5.1.8 Class Defaults (настройки)

В Details панели виджета (Class Defaults):

| Параметр | Значение по умолчанию | Описание |
|----------|----------------------|----------|
| Min Display Name Length | 3 | Минимальная длина имени |
| Max Display Name Length | 32 | Максимальная длина имени |
| Auto Close On Success | false | Закрыть виджет после успеха |
| Auto Close Delay | 2.0 | Задержка перед закрытием (сек) |

---

#### 5.1.9 EventBus события

При создании персонажа публикуются:

| Событие | Когда | Данные |
|---------|-------|--------|
| `SuspenseCore.Event.UI.Registration.Success` | Успешное создание | PlayerId, DisplayName, ClassId |
| `SuspenseCore.Event.UI.Registration.Failed` | Ошибка | ErrorMessage |

**ВАЖНО:** Имена виджетов РЕГИСТРОЗАВИСИМЫ и должны совпадать ТОЧНО!

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
│       (динамически заполняется карточками персонажей)
│       Size: Fill, Min Height: 200
│
├── [Text Block] "StatusText"
│       Text: "" (показывается если нет персонажей)
│       Visibility: Collapsed (по умолчанию)
│
├── [Horizontal Box] Buttons
│   ├── [Button] "PlayButton"
│   │   └── [Text Block] "PlayButtonText" = "PLAY"
│   │       (активна только при выбранном персонаже)
│   │
│   └── [Button] "CreateNewButton"
│       └── [Text Block] "CreateNewButtonText" = "CREATE NEW"
```

**ВАЖНО:** Имена виджетов для BindWidget:
- `TitleText` (UTextBlock)
- `CharacterListScrollBox` (UScrollBox) или `CharacterListBox` (UVerticalBox)
- `StatusText` (UTextBlock)
- `PlayButton` (UButton) - **Кнопка для начала игры**
- `PlayButtonText` (UTextBlock)
- `DeleteButton` (UButton) - **Кнопка удаления персонажа**
- `DeleteButtonText` (UTextBlock)
- `CreateNewButton` (UButton)
- `CreateNewButtonText` (UTextBlock)

**3. В Class Defaults установите:**
- `CharacterEntryWidgetClass` = `WBP_CharacterEntry`

**4. Логика кнопок:**
- **Play**: активна только когда выбран персонаж → переход в Main Menu
- **Delete**: активна только когда выбран персонаж → удаление и обновление списка
- **Create New**: всегда активна → переход на Registration

### 7.7.1 Создание WBP_CharacterEntry (карточка персонажа)

**1. Создайте Widget Blueprint:**
- Parent: `USuspenseCoreCharacterEntryWidget`
- Name: `WBP_CharacterEntry`

**2. Designer структура:**

```
[Border] "EntryBorder"
│       Brush Color: (0.1, 0.1, 0.1, 1) - меняется при выборе
│       Padding: 10
│
└── [Horizontal Box]
    ├── [Image] "AvatarImage"
    │       Size: 64x64
    │       Brush: Default avatar
    │
    ├── [Spacer] Width: 10
    │
    ├── [Vertical Box]
    │   ├── [Text Block] "DisplayNameText"
    │   │       Font Size: 18
    │   │       Color: White
    │   │
    │   └── [Text Block] "LevelText"
    │           Text: "Level X"
    │           Font Size: 14
    │           Color: Gray
    │
    └── [Button] "SelectButton" (опционально)
            (весь виджет кликабелен)
```

**ВАЖНО:** Имена виджетов для BindWidget:
- `EntryBorder` (UBorder) - для подсветки выбора
- `AvatarImage` (UImage) - аватар персонажа
- `DisplayNameText` (UTextBlock) - имя
- `LevelText` (UTextBlock) - уровень
- `SelectButton` или `EntryButton` (UButton) - опционально

**3. Цвета границы (настраиваются в Class Defaults):**
- `NormalBorderColor`: (0.1, 0.1, 0.1, 1) - обычное состояние
- `SelectedBorderColor`: (0.0, 0.5, 1.0, 1) - выбран (синий)
- `HoveredBorderColor`: (0.3, 0.3, 0.3, 1) - наведение

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

| Карта | GameMode | PlayerController |
|-------|----------|------------------|
| MainMenuMap | BP_MenuGameMode | BP_MenuPlayerController |
| CharacterSelectMap | BP_MenuGameMode | BP_MenuPlayerController |
| GameMap | BP_SuspenseCoreGameGameMode | BP_SuspenseCorePlayerController |

---

## 7.11 Map Transition System

### 7.11.1 Архитектура переходов между картами

SuspenseCore использует `USuspenseCoreMapTransitionSubsystem` для корректного переключения между картами с разными GameMode.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        MAP TRANSITION ARCHITECTURE                           │
│                                                                             │
│  ┌─────────────────────┐         ┌─────────────────────┐                    │
│  │   MAIN MENU MAP     │         │      GAME MAP       │                    │
│  │                     │         │                     │                    │
│  │  GameMode:          │         │  GameMode:          │                    │
│  │  MenuGameMode       │         │  GameGameMode       │                    │
│  │                     │         │                     │                    │
│  │  PlayerController:  │         │  PlayerController:  │                    │
│  │  MenuPlayerController│        │  SuspenseCorePC     │                    │
│  │                     │  Play   │                     │                    │
│  │  Features:          │ ──────► │  Features:          │                    │
│  │  - UI Only Mode     │         │  - Enhanced Input   │                    │
│  │  - No Pawn          │         │  - GAS Integration  │                    │
│  │  - Mouse Cursor     │ ◄────── │  - Pause Menu       │                    │
│  │                     │ Exit    │  - Save System      │                    │
│  └─────────────────────┘ ToLobby └─────────────────────┘                    │
│                                                                             │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                 MapTransitionSubsystem (GameInstance)                 │  │
│  │  - Persists across map changes                                        │  │
│  │  - Stores: PlayerId, SourceMap, TargetMap, CustomData                │  │
│  │  - Handles: TransitionToGameMap(), TransitionToMainMenu()            │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 7.11.2 GameMode классы

| Класс | Использование | PlayerController | PlayerState |
|-------|---------------|------------------|-------------|
| `ASuspenseCoreMenuGameMode` | Меню (MainMenu, CharSelect) | MenuPlayerController | nullptr |
| `ASuspenseCoreGameGameMode` | Игровой процесс | SuspenseCorePlayerController | SuspenseCorePlayerState |

### 7.11.3 Создание BP_SuspenseCoreGameGameMode

**1. Создайте Blueprint:**
- Parent: `ASuspenseCoreGameGameMode`
- Name: `BP_SuspenseCoreGameGameMode`

**2. В Class Defaults настройте:**

| Параметр | Значение |
|----------|----------|
| Main Menu Map Name | `MainMenuMap` |
| Auto Start Save Manager | ✓ |
| Auto Save Interval | 300.0 (5 минут) |
| Default Pawn Class | `BP_SuspenseCoreCharacter` |

### 7.11.4 Использование MapTransitionSubsystem

```cpp
// Переход из меню в игру (автоматически в MainMenuWidget)
if (USuspenseCoreMapTransitionSubsystem* TransitionSubsystem =
    USuspenseCoreMapTransitionSubsystem::Get(this))
{
    TransitionSubsystem->TransitionToGameMap(CurrentPlayerId, GameMapName);
}

// Возврат в меню из игры (автоматически в PauseMenuWidget)
if (USuspenseCoreMapTransitionSubsystem* TransitionSubsystem =
    USuspenseCoreMapTransitionSubsystem::Get(this))
{
    TransitionSubsystem->TransitionToMainMenu(MainMenuMapName);
}

// В GameMode получить PlayerId после перехода
void ASuspenseCoreGameGameMode::InitGame(...)
{
    // PlayerId автоматически извлекается из TransitionSubsystem
    FString PlayerId = GetCurrentPlayerId();
}
```

### 7.11.5 Настройка World Settings для карт

**MainMenuMap World Settings:**
- GameMode Override: `BP_MenuGameMode`

**GameMap World Settings:**
- GameMode Override: `BP_SuspenseCoreGameGameMode`

**ВАЖНО:** Каждая карта должна иметь правильный GameMode Override!

---

## 7.12 Save System

### 7.12.1 Обзор

SaveSystem позволяет сохранять и загружать:
- **Profile** - аккаунт, XP, валюта, статистика
- **Character** - HP, позиция, атрибуты, эффекты
- **Inventory** - предметы, экипировка

**Пути сохранения:**
```
[Project]/Saved/
├── Players/          # Профили (существующее)
│   └── [PlayerId].json
│
└── SaveGames/        # Игровые сохранения (новое)
    └── [PlayerId]/
        ├── Slot_0.sav
        ├── Slot_1.sav
        ├── AutoSave.sav
        └── QuickSave.sav
```

### 7.12.2 Использование SaveManager

```cpp
// Получение SaveManager
USuspenseCoreSaveManager* SaveMgr = USuspenseCoreSaveManager::Get(this);

// Установить текущего игрока (после логина/выбора персонажа)
SaveMgr->SetCurrentPlayer(PlayerId);
SaveMgr->SetProfileData(PlayerData);

// Quick Save/Load (F5/F9)
SaveMgr->QuickSave();
SaveMgr->QuickLoad();

// Сохранение в слот
SaveMgr->SaveToSlot(0, TEXT("My Save"));
SaveMgr->LoadFromSlot(0);

// Auto-Save
SaveMgr->SetAutoSaveEnabled(true);
SaveMgr->SetAutoSaveInterval(300.0f); // 5 минут
SaveMgr->TriggerAutoSave(); // На чекпоинте

// Получить список сохранений для UI
TArray<FSuspenseCoreSaveHeader> Headers = SaveMgr->GetAllSlotHeaders();
```

### 7.12.3 Слоты сохранений

| Слот | Описание |
|------|----------|
| 0-9 | Обычные слоты (ручное сохранение) |
| 100 | AutoSave (автоматическое) |
| 101 | QuickSave (F5) |

### 7.12.4 Интеграция с MainMenu

При выборе персонажа установите его в SaveManager:

```cpp
// В MainMenuWidget::ShowMainMenuScreen()
if (USuspenseCoreSaveManager* SaveMgr = USuspenseCoreSaveManager::Get(this))
{
    SaveMgr->SetCurrentPlayer(PlayerId);
    SaveMgr->SetProfileData(CachedPlayerData);
}
```

---

## 7.13 Pause Menu (In-Game)

### 7.13.1 Создание WBP_PauseMenu

**1. Создайте Widget Blueprint:**
- Parent: `USuspenseCorePauseMenuWidget`
- Name: `WBP_PauseMenu`

**2. Designer структура:**

```
[Canvas Panel] (Full Screen, с затемнением)
│
├── [Border] (Полупрозрачный фон)
│       Brush Color: (0, 0, 0, 0.7)
│
└── [Vertical Box] (Centered)
    │
    ├── [Text Block] "TitleText"
    │       Text: "PAUSED"
    │       Font Size: 48
    │
    ├── [Spacer] Height: 40
    │
    ├── [Button] "ContinueButton"
    │   └── [Text Block] "ContinueButtonText" - "CONTINUE"
    │
    ├── [Spacer] Height: 10
    │
    ├── [Button] "SaveButton"
    │   └── [Text Block] "SaveButtonText" - "SAVE GAME"
    │
    ├── [Spacer] Height: 10
    │
    ├── [Button] "LoadButton"
    │   └── [Text Block] "LoadButtonText" - "LOAD GAME"
    │
    ├── [Spacer] Height: 10
    │
    ├── [Button] "ExitToLobbyButton"
    │   └── [Text Block] "ExitToLobbyButtonText" - "EXIT TO LOBBY"
    │
    ├── [Spacer] Height: 10
    │
    ├── [Button] "QuitButton"
    │   └── [Text Block] "QuitButtonText" - "QUIT GAME"
    │
    └── [Text Block] "SaveStatusText" (Hidden by default)
            Text: "Saving..."
```

**3. Биндинги (BindWidget):**
- `TitleText` (UTextBlock)
- `ContinueButton`, `ContinueButtonText`
- `SaveButton`, `SaveButtonText`
- `LoadButton`, `LoadButtonText`
- `ExitToLobbyButton`, `ExitToLobbyButtonText`
- `QuitButton`, `QuitButtonText`
- `SaveStatusText` (UTextBlock) - показывает "Saving..." / "Saved!"

### 7.13.2 Настройка в GameMode

**Для игровой карты (GameMap):**

```cpp
// В вашем игровом GameMode
void AGameGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Создать Pause Menu
    if (PauseMenuWidgetClass)
    {
        APlayerController* PC = GetWorld()->GetFirstPlayerController();
        if (PC)
        {
            PauseMenuWidget = CreateWidget<USuspenseCorePauseMenuWidget>(PC, PauseMenuWidgetClass);
            if (PauseMenuWidget)
            {
                PauseMenuWidget->AddToViewport(100); // Высокий Z-order
            }
        }
    }
}
```

### 7.13.3 Input Bindings (Enhanced Input System - UE5)

SuspenseCore использует Enhanced Input System (современный подход UE5) вместо legacy Input.

**1. Создайте Input Actions:**

В Content Browser создайте 3 Data Asset:
- Right Click → Input → **Input Action**

| Asset Name | Value Type | Trigger |
|------------|------------|---------|
| `IA_PauseGame` | Digital (bool) | Down |
| `IA_QuickSave` | Digital (bool) | Down |
| `IA_QuickLoad` | Digital (bool) | Down |

**2. Создайте/обновите Input Mapping Context:**

- Right Click → Input → **Input Mapping Context**
- Name: `IMC_GameplayDefault` (или существующий)

**3. Добавьте маппинги в IMC:**

| Input Action | Key | Triggers |
|--------------|-----|----------|
| `IA_PauseGame` | Escape | Down |
| `IA_QuickSave` | F5 | Down |
| `IA_QuickLoad` | F9 | Down |

**4. Настройте BP_SuspenseCorePlayerController:**

В Class Defaults вашего PlayerController Blueprint:

```
Input | UI:
├── IA_PauseGame = IA_PauseGame
├── IA_QuickSave = IA_QuickSave
└── IA_QuickLoad = IA_QuickLoad

UI:
├── PauseMenuWidgetClass = WBP_PauseMenu
```

**5. Код уже реализован в C++:**

```cpp
// SuspenseCorePlayerController.h - Input Actions (уже есть)
UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|UI")
UInputAction* IA_PauseGame;

UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|UI")
UInputAction* IA_QuickSave;

UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|UI")
UInputAction* IA_QuickLoad;

// В SetupInputComponent() уже забиндено:
if (IA_PauseGame)
{
    EnhancedInput->BindAction(IA_PauseGame, ETriggerEvent::Started,
        this, &ASuspenseCorePlayerController::HandlePauseGame);
}
// ... аналогично для QuickSave/QuickLoad
```

**6. Структура файлов Input:**

```
Content/
└── Input/
    ├── Actions/
    │   ├── IA_Move.uasset
    │   ├── IA_Look.uasset
    │   ├── IA_Jump.uasset
    │   ├── IA_PauseGame.uasset      # Escape
    │   ├── IA_QuickSave.uasset      # F5
    │   └── IA_QuickLoad.uasset      # F9
    │
    └── Contexts/
        ├── IMC_GameplayDefault.uasset    # Основной контекст
        └── IMC_UIOnly.uasset             # Для меню (без movement)
```

### 7.13.4 Функционал кнопок

| Кнопка | Действие |
|--------|----------|
| **Continue** | Скрыть меню, продолжить игру |
| **Save Game** | QuickSave, показать "Saved!" |
| **Load Game** | Загрузить QuickSave |
| **Exit to Lobby** | Открыть MainMenuMap |
| **Quit Game** | Выход из игры |

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
