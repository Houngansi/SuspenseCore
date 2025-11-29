# Main Menu Setup Guide

> Руководство по настройке главного меню: регистрация, выбор персонажа, основное меню
> Версия: 1.0 | Дата: 2025-11-29

---

## Содержание

1. [Обзор архитектуры](#1-обзор-архитектуры)
2. [Создание GameMode и PlayerController](#2-создание-gamemode-и-playercontroller)
3. [Создание WBP_MainMenu](#3-создание-wbp_mainmenu)
4. [Создание WBP_Registration](#4-создание-wbp_registration)
5. [Создание WBP_CharacterSelect](#5-создание-wbp_characterselect)
6. [Создание WBP_CharacterEntry](#6-создание-wbp_characterentry)
7. [Создание WBP_PlayerInfo](#7-создание-wbp_playerinfo)
8. [Настройка MainMenuMap](#8-настройка-mainmenumap)
9. [Переход в игру](#9-переход-в-игру)
10. [Troubleshooting](#10-troubleshooting)

---

## 1. Обзор архитектуры

### 1.1 Трёхэкранная структура

MainMenu использует `WidgetSwitcher` для переключения между экранами:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              MAIN MENU FLOW                                  │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                         START                                        │    │
│  │                           │                                          │    │
│  │         ┌─────────────────┼─────────────────┐                        │    │
│  │         │ No saves        │ Has saves       │                        │    │
│  │         ▼                 ▼                 │                        │    │
│  │  ┌──────────────┐  ┌──────────────────┐     │                        │    │
│  │  │ Registration │  │ Character Select │     │                        │    │
│  │  │  (Index 1)   │  │    (Index 0)     │     │                        │    │
│  │  │              │  │                  │     │                        │    │
│  │  │ [Name Input] │  │ [Player_1 Lv.5]◄─┼─────┼─ Select                │    │
│  │  │ [Create]─────┼──│ [Player_2 Lv.2] │     │                        │    │
│  │  │              │  │ [+ Create New]──┼─────┘                        │    │
│  │  └──────┬───────┘  └────────┬────────┘                              │    │
│  │         │ Success           │ Select                                 │    │
│  │         └─────────┬─────────┘                                        │    │
│  │                   ▼                                                  │    │
│  │         ┌─────────────────────────────┐                              │    │
│  │         │       Main Menu             │                              │    │
│  │         │        (Index 2)            │                              │    │
│  │         │  ┌───────────────────────┐  │                              │    │
│  │         │  │   Player Info Widget  │  │                              │    │
│  │         │  │   Name: Player_X      │  │                              │    │
│  │         │  │   Level: 5            │  │                              │    │
│  │         │  └───────────────────────┘  │                              │    │
│  │         │                             │                              │    │
│  │         │  [PLAY] ────────────────────┼────────► GAME MAP            │    │
│  │         │  [OPERATORS] (disabled)     │                              │    │
│  │         │  [SETTINGS] (disabled)      │                              │    │
│  │         │  [QUIT]                     │                              │    │
│  │         └─────────────────────────────┘                              │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 1.2 Индексы экранов

| Index | Экран | Назначение |
|-------|-------|------------|
| 0 | CharacterSelect | Выбор существующего персонажа |
| 1 | Registration | Создание нового персонажа |
| 2 | MainMenu | Основное меню с кнопками |

### 1.3 Хранение данных

```
[Project]/Saved/Players/
├── Player_ABC123.json    # Данные игрока 1
├── Player_DEF456.json    # Данные игрока 2
└── ...
```

---

## 2. Создание GameMode и PlayerController

### 2.1 BP_MenuGameMode

1. **Content Browser** → Right Click → **Blueprint Class**
2. **Parent**: `ASuspenseCoreMenuGameMode`
3. **Name**: `BP_MenuGameMode`

**Class Defaults:**

| Параметр | Значение |
|----------|----------|
| Main Menu Widget Class | `WBP_MainMenu` |
| Main Menu Map Name | `MainMenuMap` |
| Default Game Map Name | `GameMap` |
| Auto Create Main Menu | ✓ |
| Default Pawn Class | None |

### 2.2 BP_MenuPlayerController

1. **Content Browser** → Right Click → **Blueprint Class**
2. **Parent**: `ASuspenseCoreMenuPlayerController`
3. **Name**: `BP_MenuPlayerController`

**Class Defaults:**

| Параметр | Значение |
|----------|----------|
| Show Cursor On Start | ✓ |
| UI Only Mode On Start | ✓ |
| Main Menu Map Name | `MainMenuMap` |

---

## 3. Создание WBP_MainMenu

### 3.1 Создание Blueprint

1. **Content Browser** → Right Click → **User Interface** → **Widget Blueprint**
2. **Parent**: `USuspenseCoreMainMenuWidget`
3. **Name**: `WBP_MainMenu`

### 3.2 Designer структура

```
[Canvas Panel] (Full Screen)
│
├── [Image] "BackgroundImage"                 ← Фоновое изображение
│       Brush: YourBackgroundImage
│       Size To Content: false
│       Anchors: Full Stretch
│       Is Variable: ✓
│
├── [Vertical Box] (Centered)
│   │   Anchors: Center
│   │   Alignment: (0.5, 0.5)
│   │
│   ├── [Text Block] "GameTitleText"          ← Название игры
│   │       Text: "SUSPENSE"
│   │       Font Size: 72
│   │       Font: Bold
│   │       Is Variable: ✓
│   │
│   ├── [Spacer] Height: 50
│   │
│   ├── [Widget Switcher] "ScreenSwitcher"    ← Переключатель экранов
│   │   │   Is Variable: ✓
│   │   │
│   │   │── [Index 0] ─ Panel_CharacterSelect
│   │   │   └── WBP_CharacterSelect           ← "CharacterSelectWidget"
│   │   │       Is Variable: ✓
│   │   │
│   │   │── [Index 1] ─ Panel_Registration
│   │   │   └── WBP_Registration              ← "RegistrationWidget"
│   │   │       Is Variable: ✓
│   │   │
│   │   └── [Index 2] ─ Panel_MainMenu
│   │       │
│   │       ├── WBP_PlayerInfo                ← "PlayerInfoWidget"
│   │       │   Is Variable: ✓
│   │       │
│   │       ├── [Spacer] Height: 30
│   │       │
│   │       └── [Vertical Box] Menu Buttons
│   │           │
│   │           ├── [Button] "PlayButton"
│   │           │   └── [Text Block] "PlayButtonText" = "PLAY"
│   │           │       Is Variable: ✓
│   │           │
│   │           ├── [Spacer] Height: 10
│   │           │
│   │           ├── [Button] "OperatorsButton"
│   │           │   └── [Text Block] = "OPERATORS"
│   │           │   Is Enabled: false (future)
│   │           │
│   │           ├── [Spacer] Height: 10
│   │           │
│   │           ├── [Button] "SettingsButton"
│   │           │   └── [Text Block] = "SETTINGS"
│   │           │   Is Enabled: false (future)
│   │           │
│   │           ├── [Spacer] Height: 10
│   │           │
│   │           └── [Button] "QuitButton"
│   │               └── [Text Block] "QuitButtonText" = "QUIT"
│   │               Is Variable: ✓
│   │
│   └── [Spacer] Height: 20
│
└── [Text Block] "VersionText"                ← Версия игры
        Text: "v0.1.0 Alpha"
        Anchors: Bottom-Right
        Is Variable: ✓
```

### 3.3 Обязательные BindWidget имена

| Имя | Тип | Обязательный | Описание |
|-----|-----|--------------|----------|
| `ScreenSwitcher` | UWidgetSwitcher | **Да** | Переключатель экранов |
| `CharacterSelectWidget` | USuspenseCoreCharacterSelectWidget | **Да** | Виджет выбора персонажа |
| `RegistrationWidget` | USuspenseCoreRegistrationWidget | **Да** | Виджет регистрации |
| `PlayerInfoWidget` | USuspenseCorePlayerInfoWidget | Нет | Информация об игроке |
| `PlayButton` | UButton | **Да** | Кнопка "Играть" |
| `PlayButtonText` | UTextBlock | Нет | Текст кнопки |
| `QuitButton` | UButton | Нет | Кнопка выхода |
| `BackgroundImage` | UImage | Нет | Фон |
| `GameTitleText` | UTextBlock | Нет | Название игры |
| `VersionText` | UTextBlock | Нет | Версия |

### 3.4 Class Defaults

```
MainMenu | Config:
├── CharacterSelectScreenIndex: 0
├── RegistrationScreenIndex:    1
├── MainMenuScreenIndex:        2
├── GameMapName:                "GameMap"
├── GameGameModeClass:          BP_SuspenseCoreGameGameMode

MainMenu | Text:
├── PlayButtonDefaultText:      "PLAY"
├── PlayButtonLoadingText:      "LOADING..."
└── QuitConfirmText:            "Are you sure you want to quit?"
```

---

## 4. Создание WBP_Registration

### 4.1 Создание Blueprint

1. **Content Browser** → Right Click → **User Interface** → **Widget Blueprint**
2. **Parent**: `USuspenseCoreRegistrationWidget`
3. **Name**: `WBP_Registration`

### 4.2 Designer структура

```
[Vertical Box]
│   Size: 400x300
│   Alignment: Center
│
├── [Text Block] "TitleText"
│       Text: "CREATE CHARACTER"
│       Font Size: 24
│       Font: Bold
│       Is Variable: ✓
│
├── [Spacer] Height: 30
│
├── [Editable Text Box] "DisplayNameInput"    ← Поле ввода имени
│       Hint Text: "Enter character name..."
│       Font Size: 16
│       Min Width: 300
│       Is Variable: ✓
│
├── [Spacer] Height: 20
│
├── [Button] "CreateButton"                    ← Кнопка создания
│   │   Style: Primary/Accent
│   │   Is Variable: ✓
│   │
│   └── [Text Block] "CreateButtonText"
│           Text: "CREATE"
│           Is Variable: ✓
│
├── [Spacer] Height: 15
│
├── [Text Block] "StatusText"                  ← Статус/ошибки
│       Text: ""
│       Color: Red (для ошибок) / Green (для успеха)
│       Is Variable: ✓
│
├── [Spacer] Height: 20
│
└── [Button] "BackButton"                      ← Назад (если есть персонажи)
    │   Visibility: Collapsed (показать если есть персонажи)
    │   Is Variable: ✓
    │
    └── [Text Block]
            Text: "BACK"
```

### 4.3 Обязательные BindWidget

| Имя | Тип | Обязательный |
|-----|-----|--------------|
| `DisplayNameInput` | UEditableTextBox | **Да** |
| `CreateButton` | UButton | **Да** |
| `StatusText` | UTextBlock | **Да** |
| `TitleText` | UTextBlock | Нет |
| `CreateButtonText` | UTextBlock | Нет |
| `BackButton` | UButton | Нет |

### 4.4 Валидация имени

Имя должно соответствовать правилам:
- Длина: 3-32 символа
- Разрешённые символы: буквы, цифры, `_`, `-`
- Не должно содержать пробелы в начале/конце

```cpp
// Регулярное выражение для валидации
static const FRegexPattern NamePattern(TEXT("^[a-zA-Z0-9_-]{3,32}$"));
```

---

## 5. Создание WBP_CharacterSelect

### 5.1 Создание Blueprint

1. **Content Browser** → Right Click → **User Interface** → **Widget Blueprint**
2. **Parent**: `USuspenseCoreCharacterSelectWidget`
3. **Name**: `WBP_CharacterSelect`

### 5.2 Designer структура

```
[Vertical Box]
│   Size: 500x400
│
├── [Text Block] "TitleText"
│       Text: "SELECT CHARACTER"
│       Font Size: 24
│       Is Variable: ✓
│
├── [Spacer] Height: 20
│
├── [Scroll Box] "CharacterListScrollBox"     ← Список персонажей
│   │   Orientation: Vertical
│   │   Size: Fill
│   │   Min Height: 200
│   │   Is Variable: ✓
│   │
│   └── (Динамически заполняется WBP_CharacterEntry)
│
├── [Spacer] Height: 15
│
├── [Text Block] "StatusText"                  ← "No characters found"
│       Visibility: Collapsed
│       Is Variable: ✓
│
├── [Spacer] Height: 15
│
└── [Horizontal Box] Buttons
    │
    ├── [Button] "PlayButton"                  ← Играть выбранным
    │   │   Is Enabled: false (до выбора)
    │   │   Is Variable: ✓
    │   │
    │   └── [Text Block] "PlayButtonText"
    │           Text: "PLAY"
    │           Is Variable: ✓
    │
    ├── [Spacer] Width: 10
    │
    ├── [Button] "DeleteButton"                ← Удалить выбранного
    │   │   Is Enabled: false (до выбора)
    │   │   Style: Danger/Red
    │   │   Is Variable: ✓
    │   │
    │   └── [Text Block] "DeleteButtonText"
    │           Text: "DELETE"
    │           Is Variable: ✓
    │
    ├── [Spacer] Width: 10
    │
    └── [Button] "CreateNewButton"             ← Создать нового
        │   Is Variable: ✓
        │
        └── [Text Block] "CreateNewButtonText"
                Text: "CREATE NEW"
                Is Variable: ✓
```

### 5.3 Обязательные BindWidget

| Имя | Тип | Обязательный |
|-----|-----|--------------|
| `CharacterListScrollBox` | UScrollBox | **Да** |
| `PlayButton` | UButton | **Да** |
| `CreateNewButton` | UButton | **Да** |
| `TitleText` | UTextBlock | Нет |
| `StatusText` | UTextBlock | Нет |
| `DeleteButton` | UButton | Нет |

### 5.4 Class Defaults

```
CharacterSelect | Config:
└── CharacterEntryWidgetClass: WBP_CharacterEntry  # ОБЯЗАТЕЛЬНО!
```

---

## 6. Создание WBP_CharacterEntry

### 6.1 Создание Blueprint

1. **Content Browser** → Right Click → **User Interface** → **Widget Blueprint**
2. **Parent**: `USuspenseCoreCharacterEntryWidget`
3. **Name**: `WBP_CharacterEntry`

### 6.2 Designer структура

```
[Border] "EntryBorder"                         ← Рамка для подсветки
│   Brush Color: (0.1, 0.1, 0.1, 1.0)
│   Padding: 10
│   Is Variable: ✓
│
└── [Button] "SelectButton"                    ← Кликабельная область
    │   Style: Transparent
    │   Is Variable: ✓
    │
    └── [Horizontal Box]
        │
        ├── [Image] "AvatarImage"              ← Аватар (опционально)
        │       Size: 64x64
        │       Is Variable: ✓
        │
        ├── [Spacer] Width: 15
        │
        └── [Vertical Box]
            │
            ├── [Text Block] "DisplayNameText" ← Имя персонажа
            │       Font Size: 18
            │       Font: Bold
            │       Is Variable: ✓
            │
            └── [Text Block] "LevelText"       ← Уровень
                    Text: "Level X"
                    Font Size: 14
                    Color: Gray
                    Is Variable: ✓
```

### 6.3 Обязательные BindWidget

| Имя | Тип | Обязательный |
|-----|-----|--------------|
| `DisplayNameText` | UTextBlock | **Да** |
| `EntryBorder` | UBorder | Нет |
| `SelectButton` | UButton | Нет |
| `AvatarImage` | UImage | Нет |
| `LevelText` | UTextBlock | Нет |

### 6.4 Class Defaults - Цвета

```
CharacterEntry | Appearance:
├── NormalBorderColor:    (0.1, 0.1, 0.1, 1.0)  # Обычное
├── SelectedBorderColor:  (0.0, 0.5, 1.0, 1.0)  # Выбран (синий)
└── HoveredBorderColor:   (0.3, 0.3, 0.3, 1.0)  # Наведение
```

---

## 7. Создание WBP_PlayerInfo

### 7.1 Создание Blueprint

1. **Content Browser** → Right Click → **User Interface** → **Widget Blueprint**
2. **Parent**: `USuspenseCorePlayerInfoWidget`
3. **Name**: `WBP_PlayerInfo`

### 7.2 Designer структура

```
[Vertical Box]
│   Size: 350x250
│
├── [Horizontal Box]
│   ├── [Text Block] "DisplayNameText"
│   │       Font Size: 20
│   │       Font: Bold
│   │       Is Variable: ✓
│   │
│   └── [Text Block] "PlayerIdText"
│           Font Size: 12
│           Color: Gray
│           Is Variable: ✓
│
├── [Spacer] Height: 10
│
├── [Text Block] "LevelText"
│       Text: "Level X"
│       Font Size: 16
│       Is Variable: ✓
│
├── [Spacer] Height: 5
│
├── [Progress Bar] "XPProgressBar"
│       Percent: 0.0
│       Fill Color: Gold
│       Is Variable: ✓
│
├── [Text Block] "XPText"
│       Text: "0 / 1000 XP"
│       Font Size: 12
│       Is Variable: ✓
│
├── [Spacer] Height: 15
│
├── [Horizontal Box] Currency
│   ├── [Image] SoftCurrencyIcon
│   ├── [Text Block] "SoftCurrencyText"
│   │       Is Variable: ✓
│   ├── [Spacer] Width: 20
│   ├── [Image] HardCurrencyIcon
│   └── [Text Block] "HardCurrencyText"
│           Is Variable: ✓
│
├── [Spacer] Height: 10
│
├── [Horizontal Box] Stats
│   ├── [Text Block] "KillsText"
│   │       Is Variable: ✓
│   ├── [Text Block] "DeathsText"
│   │       Is Variable: ✓
│   └── [Text Block] "KDRatioText"
│           Is Variable: ✓
│
├── [Text Block] "PlaytimeText"
│       Is Variable: ✓
│
└── [Button] "RefreshButton" (опционально)
        Is Variable: ✓
```

### 7.3 Обязательные BindWidget

| Имя | Тип | Обязательный |
|-----|-----|--------------|
| `DisplayNameText` | UTextBlock | **Да** |
| `LevelText` | UTextBlock | Нет |
| `XPProgressBar` | UProgressBar | Нет |
| `XPText` | UTextBlock | Нет |
| `SoftCurrencyText` | UTextBlock | Нет |
| `HardCurrencyText` | UTextBlock | Нет |
| `KillsText` | UTextBlock | Нет |
| `DeathsText` | UTextBlock | Нет |
| `PlaytimeText` | UTextBlock | Нет |

---

## 8. Настройка MainMenuMap

### 8.1 Создание карты

1. **File** → **New Level** → **Empty Level**
2. **Save As**: `MainMenuMap`

### 8.2 World Settings

| Параметр | Значение |
|----------|----------|
| GameMode Override | `BP_MenuGameMode` |

### 8.3 Project Settings

**Maps & Modes:**

| Параметр | Значение |
|----------|----------|
| Editor Startup Map | `MainMenuMap` |
| Game Default Map | `MainMenuMap` |
| Default GameMode | `BP_MenuGameMode` |

---

## 9. Переход в игру

### 9.1 Архитектура перехода

```
MainMenuMap                    GameMap
    │                              │
    │  TransitionToGameMap()       │
    │  ──────────────────────────► │
    │                              │
    │  MapTransitionSubsystem      │
    │  сохраняет PlayerId          │
    │                              │
    │                        GameGameMode
    │                        получает PlayerId
    │                        инициализирует SaveManager
```

### 9.2 Код перехода

```cpp
// В MainMenuWidget при нажатии Play
void USuspenseCoreMainMenuWidget::OnPlayButtonClicked()
{
    if (USuspenseCoreMapTransitionSubsystem* Transition =
        USuspenseCoreMapTransitionSubsystem::Get(this))
    {
        // Передаём PlayerId и имя карты
        Transition->TransitionToGameMap(CurrentPlayerId, GameMapName, GameGameModeClass);
    }
}
```

### 9.3 Настройка GameMap

**World Settings:**

| Параметр | Значение |
|----------|----------|
| GameMode Override | `BP_SuspenseCoreGameGameMode` |

---

## 10. Troubleshooting

### Проблема: Меню не появляется

**Решение:**
1. Проверьте `BP_MenuGameMode` → `MainMenuWidgetClass` = `WBP_MainMenu`
2. Проверьте World Settings карты → GameMode Override = `BP_MenuGameMode`
3. Убедитесь что `AutoCreateMainMenu = true`

---

### Проблема: WidgetSwitcher показывает не тот экран

**Решение:**
1. Проверьте индексы в `ScreenSwitcher`
2. Index 0 = CharacterSelect
3. Index 1 = Registration
4. Index 2 = MainMenu

---

### Проблема: Персонажи не отображаются

**Решение:**
1. Проверьте `CharacterEntryWidgetClass` в `WBP_CharacterSelect`
2. Проверьте наличие файлов в `[Project]/Saved/Players/`
3. Проверьте логи на ошибки чтения

---

### Проблема: Кнопка Play не работает

**Решение:**
1. Проверьте `GameMapName` в Class Defaults `WBP_MainMenu`
2. Убедитесь что карта `GameMap` существует
3. Проверьте что `GameGameModeClass` установлен

---

### Проблема: После перехода нет управления

**Критично!** См. [InputModeHandling.md](InputModeHandling.md)

В `BP_SuspenseCorePlayerController` должен быть код:

```cpp
void ASuspenseCorePlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (IsLocalController())
    {
        FInputModeGameOnly InputMode;
        SetInputMode(InputMode);
        bShowMouseCursor = false;
    }
}
```

---

## Чеклист настройки

- [ ] Создан `BP_MenuGameMode` (Parent: `ASuspenseCoreMenuGameMode`)
- [ ] Создан `BP_MenuPlayerController` (Parent: `ASuspenseCoreMenuPlayerController`)
- [ ] Создан `WBP_MainMenu` (Parent: `USuspenseCoreMainMenuWidget`)
- [ ] Создан `WBP_Registration` (Parent: `USuspenseCoreRegistrationWidget`)
- [ ] Создан `WBP_CharacterSelect` (Parent: `USuspenseCoreCharacterSelectWidget`)
- [ ] Создан `WBP_CharacterEntry` (Parent: `USuspenseCoreCharacterEntryWidget`)
- [ ] Создан `WBP_PlayerInfo` (Parent: `USuspenseCorePlayerInfoWidget`)
- [ ] Виджеты встроены в `ScreenSwitcher` с правильными индексами
- [ ] `CharacterEntryWidgetClass` установлен в `WBP_CharacterSelect`
- [ ] `MainMenuWidgetClass` установлен в `BP_MenuGameMode`
- [ ] Карта `MainMenuMap` создана с правильным GameMode Override
- [ ] Project Settings настроены на `MainMenuMap` как стартовую

---

*Документация создана: 2025-11-29*
*SuspenseCore Clean Architecture*
