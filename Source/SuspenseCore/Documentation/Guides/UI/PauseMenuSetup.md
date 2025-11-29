# Pause Menu Setup Guide

> Руководство по настройке игрового меню паузы (Q/Escape)
> Версия: 1.0 | Дата: 2025-11-29

---

## Содержание

1. [Обзор](#1-обзор)
2. [Создание WBP_PauseMenu](#2-создание-wbp_pausemenu)
3. [Настройка Input Actions](#3-настройка-input-actions)
4. [Настройка PlayerController](#4-настройка-playercontroller)
5. [Интеграция с Save/Load](#5-интеграция-с-saveload)
6. [Возврат в Main Menu](#6-возврат-в-main-menu)
7. [Troubleshooting](#7-troubleshooting)

---

## 1. Обзор

### 1.1 Что это?

PauseMenu - меню паузы, вызываемое во время игры:
- **Q** или **Escape** - открыть/закрыть
- **F5** - Quick Save
- **F9** - Quick Load

### 1.2 Функционал

| Кнопка | Действие |
|--------|----------|
| Continue | Закрыть меню, продолжить игру |
| Save Game | Открыть меню сохранения |
| Load Game | Открыть меню загрузки |
| Exit to Lobby | Вернуться в MainMenu |
| Quit Game | Выход из игры |

### 1.3 Архитектура

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           PAUSE MENU SYSTEM                                  │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                      PlayerController                                │    │
│  │                                                                     │    │
│  │  Input Actions:                                                     │    │
│  │  ├── IA_PauseGame (Q/Escape) ──► HandlePauseGame()                 │    │
│  │  ├── IA_QuickSave (F5) ────────► HandleQuickSave()                 │    │
│  │  └── IA_QuickLoad (F9) ────────► HandleQuickLoad()                 │    │
│  │                                                                     │    │
│  │  PauseMenuWidget:                                                   │    │
│  │  └── WBP_PauseMenu (Z-Order: 100)                                  │    │
│  │                                                                     │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                    │                                        │
│                                    ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                      USuspenseCorePauseMenuWidget                    │    │
│  │                                                                     │    │
│  │  [Continue]     ──► ClosePauseMenu()                               │    │
│  │  [Save Game]    ──► ShowSaveLoadMenu(Save)                         │    │
│  │  [Load Game]    ──► ShowSaveLoadMenu(Load)                         │    │
│  │  [Exit to Lobby]──► TransitionToMainMenu()                         │    │
│  │  [Quit Game]    ──► QuitGame()                                     │    │
│  │                                                                     │    │
│  │  SaveLoadMenuWidget (создаётся по требованию):                     │    │
│  │  └── WBP_SaveLoadMenu (Z-Order: 101)                               │    │
│  │                                                                     │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Создание WBP_PauseMenu

### 2.1 Создание Blueprint

1. **Content Browser** → Right Click → **User Interface** → **Widget Blueprint**
2. **Parent**: `USuspenseCorePauseMenuWidget`
3. **Name**: `WBP_PauseMenu`

### 2.2 Designer структура

```
[Canvas Panel] (Full Screen)
│
├── [Border] "BackgroundOverlay"              ← Затемнение фона
│       Brush Color: (0, 0, 0, 0.7)
│       Anchors: Full Stretch
│       Is Variable: ✓
│
└── [Border] "MenuContainer"                  ← Контейнер меню
    │   Brush Color: (0.05, 0.05, 0.08, 0.95)
    │   Anchors: Center
    │   Size: 400x450
    │   Padding: 30
    │   Is Variable: ✓
    │
    └── [Vertical Box]
        │
        ├── [Text Block] "TitleText"          ← "PAUSED"
        │       Text: "PAUSED"
        │       Font Size: 48
        │       Font: Bold
        │       Alignment: Center
        │       Is Variable: ✓
        │
        ├── [Spacer] Height: 40
        │
        ├── [Button] "ContinueButton"         ← Продолжить
        │   │   Style: Primary
        │   │   Min Width: 300
        │   │   Is Variable: ✓
        │   │
        │   └── [Text Block] "ContinueButtonText"
        │           Text: "CONTINUE"
        │           Font Size: 18
        │           Is Variable: ✓
        │
        ├── [Spacer] Height: 15
        │
        ├── [Button] "SaveButton"             ← Сохранить
        │   │   Is Variable: ✓
        │   │
        │   └── [Text Block] "SaveButtonText"
        │           Text: "SAVE GAME"
        │           Is Variable: ✓
        │
        ├── [Spacer] Height: 15
        │
        ├── [Button] "LoadButton"             ← Загрузить
        │   │   Is Variable: ✓
        │   │
        │   └── [Text Block] "LoadButtonText"
        │           Text: "LOAD GAME"
        │           Is Variable: ✓
        │
        ├── [Spacer] Height: 15
        │
        ├── [Button] "ExitToLobbyButton"      ← Выход в лобби
        │   │   Style: Secondary
        │   │   Is Variable: ✓
        │   │
        │   └── [Text Block] "ExitToLobbyButtonText"
        │           Text: "EXIT TO LOBBY"
        │           Is Variable: ✓
        │
        ├── [Spacer] Height: 15
        │
        ├── [Button] "QuitButton"             ← Выход из игры
        │   │   Style: Danger/Red
        │   │   Is Variable: ✓
        │   │
        │   └── [Text Block] "QuitButtonText"
        │           Text: "QUIT GAME"
        │           Is Variable: ✓
        │
        ├── [Spacer] Height: 20
        │
        └── [Text Block] "SaveStatusText"     ← Статус операций
                Text: ""
                Visibility: Collapsed
                Color: Yellow
                Is Variable: ✓
```

### 2.3 Обязательные BindWidget

| Имя | Тип | Обязательный | Описание |
|-----|-----|--------------|----------|
| `ContinueButton` | UButton | **Да** | Продолжить игру |
| `SaveButton` | UButton | **Да** | Открыть Save меню |
| `LoadButton` | UButton | **Да** | Открыть Load меню |
| `ExitToLobbyButton` | UButton | **Да** | Выход в MainMenu |
| `QuitButton` | UButton | Нет | Выход из игры |
| `TitleText` | UTextBlock | Нет | Заголовок |
| `SaveStatusText` | UTextBlock | Нет | Статус сохранения |
| `BackgroundOverlay` | UBorder/UImage | Нет | Затемнение |

### 2.4 Class Defaults

```
PauseMenu | Config:
├── MainMenuMapName:         "MainMenuMap"
├── bPauseGameWhenOpened:    true
├── bShowCursorWhenOpened:   true

PauseMenu | Config | SaveLoad:
└── SaveLoadMenuWidgetClass: WBP_SaveLoadMenu     # Для полного меню

PauseMenu | Text:
├── TitleDefaultText:        "PAUSED"
├── SaveSuccessText:         "Game Saved!"
├── SaveFailedText:          "Save Failed"
├── LoadingText:             "Loading..."
└── ExitConfirmText:         "Unsaved progress will be lost. Continue?"
```

---

## 3. Настройка Input Actions

### 3.1 Создание Input Actions

В **Content Browser** создайте 3 Input Action:

**Right Click** → **Input** → **Input Action**

| Asset Name | Value Type | Описание |
|------------|------------|----------|
| `IA_PauseGame` | Digital (bool) | Пауза (Q/Escape) |
| `IA_QuickSave` | Digital (bool) | Быстрое сохранение (F5) |
| `IA_QuickLoad` | Digital (bool) | Быстрая загрузка (F9) |

### 3.2 Настройка Input Mapping Context

Откройте или создайте `IMC_GameplayDefault`:

**Right Click** → **Input** → **Input Mapping Context**

Добавьте маппинги:

| Input Action | Key 1 | Key 2 | Triggers |
|--------------|-------|-------|----------|
| `IA_PauseGame` | Q | Escape | Down |
| `IA_QuickSave` | F5 | - | Down |
| `IA_QuickLoad` | F9 | - | Down |

### 3.3 Структура файлов

```
Content/Input/
├── Actions/
│   ├── IA_Move.uasset
│   ├── IA_Look.uasset
│   ├── IA_Jump.uasset
│   ├── IA_PauseGame.uasset      # Q/Escape
│   ├── IA_QuickSave.uasset      # F5
│   └── IA_QuickLoad.uasset      # F9
│
└── Contexts/
    ├── IMC_GameplayDefault.uasset
    └── IMC_UIOnly.uasset
```

---

## 4. Настройка PlayerController

### 4.1 Настройка BP_SuspenseCorePlayerController

Откройте `BP_SuspenseCorePlayerController` → **Class Defaults**:

```
Input | UI:
├── IA_PauseGame:         IA_PauseGame
├── IA_QuickSave:         IA_QuickSave
└── IA_QuickLoad:         IA_QuickLoad

Input | Mapping:
└── DefaultMappingContext: IMC_GameplayDefault

UI:
└── PauseMenuWidgetClass: WBP_PauseMenu
```

### 4.2 Как это работает (C++ код)

```cpp
// SuspenseCorePlayerController.cpp

void ASuspenseCorePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (UEnhancedInputComponent* EnhancedInput =
        Cast<UEnhancedInputComponent>(InputComponent))
    {
        // Bind pause
        if (IA_PauseGame)
        {
            EnhancedInput->BindAction(IA_PauseGame, ETriggerEvent::Started,
                this, &ASuspenseCorePlayerController::HandlePauseGame);
        }

        // Bind quick save
        if (IA_QuickSave)
        {
            EnhancedInput->BindAction(IA_QuickSave, ETriggerEvent::Started,
                this, &ASuspenseCorePlayerController::HandleQuickSave);
        }

        // Bind quick load
        if (IA_QuickLoad)
        {
            EnhancedInput->BindAction(IA_QuickLoad, ETriggerEvent::Started,
                this, &ASuspenseCorePlayerController::HandleQuickLoad);
        }
    }
}

void ASuspenseCorePlayerController::HandlePauseGame()
{
    if (PauseMenuWidget)
    {
        if (PauseMenuWidget->IsVisible())
        {
            PauseMenuWidget->ClosePauseMenu();
        }
        else
        {
            PauseMenuWidget->OpenPauseMenu();
        }
    }
}

void ASuspenseCorePlayerController::HandleQuickSave()
{
    if (USuspenseCoreSaveManager* SaveMgr = USuspenseCoreSaveManager::Get(this))
    {
        SaveMgr->QuickSave();
        // UI уведомление будет через delegate
    }
}

void ASuspenseCorePlayerController::HandleQuickLoad()
{
    if (USuspenseCoreSaveManager* SaveMgr = USuspenseCoreSaveManager::Get(this))
    {
        SaveMgr->QuickLoad();
    }
}
```

### 4.3 Создание PauseMenu в BeginPlay

```cpp
void ASuspenseCorePlayerController::BeginPlay()
{
    Super::BeginPlay();

    // КРИТИЧНО: Установить GameOnly input mode
    if (IsLocalController())
    {
        FInputModeGameOnly InputMode;
        SetInputMode(InputMode);
        bShowMouseCursor = false;
    }

    // Создать PauseMenu
    if (PauseMenuWidgetClass && IsLocalController())
    {
        PauseMenuWidget = CreateWidget<USuspenseCorePauseMenuWidget>(
            this, PauseMenuWidgetClass);

        if (PauseMenuWidget)
        {
            PauseMenuWidget->AddToViewport(100);
            PauseMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}
```

---

## 5. Интеграция с Save/Load

### 5.1 Настройка в WBP_PauseMenu

В **Class Defaults**:

```
PauseMenu | Config | SaveLoad:
└── SaveLoadMenuWidgetClass: WBP_SaveLoadMenu
```

### 5.2 Поведение кнопок

| Кнопка | SaveLoadMenuWidgetClass SET | SaveLoadMenuWidgetClass NOT SET |
|--------|------------------------------|--------------------------------|
| Save | Открывает SaveLoadMenu в режиме Save | QuickSave (F5) |
| Load | Открывает SaveLoadMenu в режиме Load | QuickLoad (F9) |

### 5.3 Диаграмма потока

```
┌─────────────────┐
│   PauseMenu     │
│                 │
│  [Save Game] ───┼─────┐
│                 │     │
│  [Load Game] ───┼─────┤
└─────────────────┘     │
                        ▼
        ┌───────────────────────────────┐
        │  SaveLoadMenuWidgetClass SET? │
        └───────────┬───────────────────┘
                    │
       ┌────────────┴────────────┐
       │ YES                     │ NO
       ▼                         ▼
┌──────────────────┐    ┌──────────────────┐
│  SaveLoadMenu    │    │   Quick Action   │
│                  │    │                  │
│  - Slot list     │    │  Save: QuickSave │
│  - Confirmation  │    │  Load: QuickLoad │
│  - Delete        │    │                  │
└──────────────────┘    └──────────────────┘
```

---

## 6. Возврат в Main Menu

### 6.1 Как работает Exit to Lobby

```cpp
void USuspenseCorePauseMenuWidget::OnExitToLobbyButtonClicked()
{
    // Опционально: показать подтверждение
    // ShowConfirmationDialog(ExitConfirmText, ...);

    // Использовать MapTransitionSubsystem
    if (USuspenseCoreMapTransitionSubsystem* Transition =
        USuspenseCoreMapTransitionSubsystem::Get(this))
    {
        Transition->TransitionToMainMenu(MainMenuMapName);
    }
    else
    {
        // Fallback: прямой переход
        UGameplayStatics::OpenLevel(this, FName(*MainMenuMapName));
    }
}
```

### 6.2 Важно: Input Mode

При возврате в MainMenu автоматически установится `UIOnly` режим через `MenuPlayerController`.

При повторном входе в игру `SuspenseCorePlayerController` должен установить `GameOnly` режим. См. [InputModeHandling.md](InputModeHandling.md).

---

## 7. Troubleshooting

### Проблема: Q/Escape не открывает меню

**Причины и решения:**

1. **Input Actions не назначены**
   - Проверьте `IA_PauseGame` в Class Defaults PlayerController

2. **Input Mapping Context не добавлен**
   - Проверьте что `IMC_GameplayDefault` содержит `IA_PauseGame`
   - Проверьте что контекст добавляется в `BeginPlay`

3. **Input Mode = UIOnly**
   - После перехода из MainMenu режим остаётся UIOnly
   - Добавьте в `BeginPlay`: `SetInputMode(FInputModeGameOnly())`

4. **PauseMenuWidgetClass не установлен**
   - В Class Defaults PlayerController установите `WBP_PauseMenu`

---

### Проблема: Меню открывается, но кнопки не работают

**Причины:**
1. BindWidget имена не совпадают
2. Кнопки не имеют флаг `Is Variable`

**Решение:**
- Проверьте имена виджетов в Designer
- Включите `Is Variable` для всех кнопок

---

### Проблема: Игра не ставится на паузу

**Решение:**
В Class Defaults `WBP_PauseMenu`:
```
bPauseGameWhenOpened: true
```

---

### Проблема: Курсор не появляется

**Решение:**
В Class Defaults `WBP_PauseMenu`:
```
bShowCursorWhenOpened: true
```

---

### Проблема: После закрытия меню input не работает

**Причина:** Input Mode не восстановлен

**Решение:** Это автоматически в `ClosePauseMenu()`:

```cpp
void USuspenseCorePauseMenuWidget::ClosePauseMenu()
{
    SetVisibility(ESlateVisibility::Collapsed);

    APlayerController* PC = GetOwningPlayer();
    if (PC)
    {
        // Восстановить Game input mode
        FInputModeGameOnly InputMode;
        PC->SetInputMode(InputMode);
        PC->bShowMouseCursor = false;

        // Снять паузу
        if (bPauseGameWhenOpened)
        {
            PC->SetPause(false);
        }
    }
}
```

---

### Проблема: Exit to Lobby не работает

**Причины:**
1. `MainMenuMapName` не установлен
2. Карта не существует

**Решение:**
- В Class Defaults установите `MainMenuMapName = "MainMenuMap"`
- Убедитесь что карта существует в проекте

---

## Чеклист настройки

- [ ] Создан `WBP_PauseMenu` (Parent: `USuspenseCorePauseMenuWidget`)
- [ ] Все BindWidget элементы названы корректно
- [ ] Созданы Input Actions: `IA_PauseGame`, `IA_QuickSave`, `IA_QuickLoad`
- [ ] Input Actions добавлены в `IMC_GameplayDefault`
- [ ] Input Actions назначены в `BP_SuspenseCorePlayerController`
- [ ] `PauseMenuWidgetClass` установлен в PlayerController
- [ ] `SaveLoadMenuWidgetClass` установлен в `WBP_PauseMenu` (опционально)
- [ ] `MainMenuMapName` установлен в `WBP_PauseMenu`
- [ ] Input Mode = GameOnly в BeginPlay PlayerController
- [ ] Протестирован вызов меню по Q/Escape
- [ ] Протестированы все кнопки меню

---

*Документация создана: 2025-11-29*
*SuspenseCore Clean Architecture*
