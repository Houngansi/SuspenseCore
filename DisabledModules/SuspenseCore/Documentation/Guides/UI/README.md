# SuspenseCore UI System - Documentation

> Полное руководство по настройке UI системы SuspenseCore
> Версия: 1.0 | Дата: 2025-11-29

---

## Содержание

| Документ | Описание |
|----------|----------|
| [MainMenuSetup.md](MainMenuSetup.md) | Настройка главного меню, выбор персонажа, регистрация |
| [PauseMenuSetup.md](PauseMenuSetup.md) | Игровое меню паузы (Q/Escape) |
| [SaveLoadMenuSetup.md](SaveLoadMenuSetup.md) | Меню сохранения/загрузки со слотами |
| [InputModeHandling.md](InputModeHandling.md) | **КРИТИЧНО:** Обработка Input Mode при переходах |
| [WidgetBindings.md](WidgetBindings.md) | Справочник по BindWidget для всех виджетов |

---

## Архитектура UI

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          SUSPENSECORE UI ARCHITECTURE                        │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                         MAIN MENU MAP                                │    │
│  │                                                                     │    │
│  │  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐  │    │
│  │  │ CharacterSelect  │  │   Registration   │  │    MainMenu      │  │    │
│  │  │   (Index 0)      │  │    (Index 1)     │  │   (Index 2)      │  │    │
│  │  │                  │  │                  │  │                  │  │    │
│  │  │ - Player list    │  │ - Name input     │  │ - PlayerInfo     │  │    │
│  │  │ - Create new     │  │ - Validation     │  │ - Play button    │  │    │
│  │  │ - Delete         │  │ - Save to disk   │  │ - Settings       │  │    │
│  │  └────────┬─────────┘  └────────┬─────────┘  └────────┬─────────┘  │    │
│  │           │                     │                     │            │    │
│  │           └─────────────────────┼─────────────────────┘            │    │
│  │                                 │                                  │    │
│  │                    ┌────────────▼────────────┐                     │    │
│  │                    │    WidgetSwitcher       │                     │    │
│  │                    │  (ScreenSwitcher)       │                     │    │
│  │                    └─────────────────────────┘                     │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                    │                                        │
│                                    │ TransitionToGameMap()                  │
│                                    ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                           GAME MAP                                   │    │
│  │                                                                     │    │
│  │  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐  │    │
│  │  │    PauseMenu     │  │  SaveLoadMenu    │  │      HUD         │  │    │
│  │  │   (Q/Escape)     │  │  (from Pause)    │  │                  │  │    │
│  │  │                  │  │                  │  │  - Health bar    │  │    │
│  │  │ - Continue       │  │ - Save slots     │  │  - Minimap       │  │    │
│  │  │ - Save/Load      │  │ - Load slots     │  │  - Inventory     │  │    │
│  │  │ - Exit to Lobby  │  │ - Quick/Auto     │  │  - etc           │  │    │
│  │  └──────────────────┘  └──────────────────┘  └──────────────────┘  │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Иерархия классов

### Базовые виджеты

```
UUserWidget
├── USuspenseCoreRegistrationWidget    - Регистрация нового игрока
├── USuspenseCoreCharacterSelectWidget - Выбор персонажа
├── USuspenseCoreCharacterEntryWidget  - Карточка персонажа в списке
├── USuspenseCorePlayerInfoWidget      - Отображение данных игрока
├── USuspenseCoreMainMenuWidget        - Главное меню (WidgetSwitcher)
├── USuspenseCorePauseMenuWidget       - Меню паузы в игре
├── USuspenseCoreSaveLoadMenuWidget    - Меню выбора слотов сохранения
└── USuspenseCoreSaveSlotWidget        - Карточка слота сохранения
```

### Blueprint виджеты (создаются в Content Browser)

| Blueprint | Parent Class | Назначение |
|-----------|--------------|------------|
| `WBP_Registration` | `USuspenseCoreRegistrationWidget` | Форма регистрации |
| `WBP_CharacterSelect` | `USuspenseCoreCharacterSelectWidget` | Список персонажей |
| `WBP_CharacterEntry` | `USuspenseCoreCharacterEntryWidget` | Карточка в списке |
| `WBP_PlayerInfo` | `USuspenseCorePlayerInfoWidget` | Информация игрока |
| `WBP_MainMenu` | `USuspenseCoreMainMenuWidget` | Главное меню |
| `WBP_PauseMenu` | `USuspenseCorePauseMenuWidget` | Меню паузы |
| `WBP_SaveLoadMenu` | `USuspenseCoreSaveLoadMenuWidget` | Меню сохранений |
| `WBP_SaveSlot` | `USuspenseCoreSaveSlotWidget` | Карточка слота |

---

## Быстрый старт

### 1. Минимальная настройка (только меню)

```
1. Создайте BP_MenuGameMode (Parent: ASuspenseCoreMenuGameMode)
2. Создайте WBP_MainMenu (Parent: USuspenseCoreMainMenuWidget)
3. В BP_MenuGameMode → MainMenuWidgetClass = WBP_MainMenu
4. На карте MainMenuMap → GameMode Override = BP_MenuGameMode
5. Играйте!
```

### 2. Полная настройка (с Save/Load)

См. документы в этой директории в порядке:
1. [MainMenuSetup.md](MainMenuSetup.md)
2. [PauseMenuSetup.md](PauseMenuSetup.md)
3. [SaveLoadMenuSetup.md](SaveLoadMenuSetup.md)
4. [InputModeHandling.md](InputModeHandling.md) - **ОБЯЗАТЕЛЬНО ПРОЧИТАТЬ!**

---

## Критические замечания

### Input Mode при переходах между картами

**ПРОБЛЕМА:** При переходе MainMenu → GameMap Input Mode остаётся `UIOnly`, игровой ввод (WASD, Q-пауза) не работает!

**РЕШЕНИЕ:** Явно установить `GameOnly` в `BeginPlay` PlayerController'а игровой карты.

```cpp
void ASuspenseCorePlayerController::BeginPlay()
{
    Super::BeginPlay();

    // КРИТИЧНО! Установить Input Mode для игры
    if (IsLocalController())
    {
        FInputModeGameOnly InputMode;
        SetInputMode(InputMode);
        bShowMouseCursor = false;
    }
}
```

Подробнее: [InputModeHandling.md](InputModeHandling.md)

---

## GameplayTags для UI

Все теги UI определены в `SuspenseCoreGameplayTags.ini`:

```ini
; --- UI Events ---
+GameplayTagList=(Tag="Event.UI.PauseMenu.Opened")
+GameplayTagList=(Tag="Event.UI.PauseMenu.Closed")
+GameplayTagList=(Tag="Event.UI.SaveLoadMenu.Opened")
+GameplayTagList=(Tag="Event.UI.SaveLoadMenu.Closed")
+GameplayTagList=(Tag="Event.UI.SaveLoadMenu.SlotSelected")
+GameplayTagList=(Tag="Event.UI.SaveLoadMenu.SaveConfirmed")
+GameplayTagList=(Tag="Event.UI.SaveLoadMenu.LoadConfirmed")
+GameplayTagList=(Tag="Event.UI.SaveLoadMenu.DeleteConfirmed")
```

---

*Документация создана: 2025-11-29*
*SuspenseCore Clean Architecture*
