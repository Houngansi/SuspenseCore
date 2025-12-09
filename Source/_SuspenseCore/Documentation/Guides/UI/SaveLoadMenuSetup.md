# Save/Load Menu Setup Guide

> Детальное руководство по созданию и настройке меню сохранения/загрузки
> Версия: 1.0 | Дата: 2025-11-29

---

## Содержание

1. [Обзор системы](#1-обзор-системы)
2. [Архитектура компонентов](#2-архитектура-компонентов)
3. [Создание WBP_SaveSlot](#3-создание-wbp_saveslot)
4. [Создание WBP_SaveLoadMenu](#4-создание-wbp_saveloadmenu)
5. [Интеграция с PauseMenu](#5-интеграция-с-pausemenu)
6. [Настройка слотов](#6-настройка-слотов)
7. [API SaveManager](#7-api-savemanager)
8. [Тестирование](#8-тестирование)
9. [Troubleshooting](#9-troubleshooting)

---

## 1. Обзор системы

### Что это?

Save/Load Menu - это полноценное меню для управления игровыми сохранениями:
- Отображение списка слотов сохранения
- Создание новых сохранений
- Загрузка существующих сохранений
- Удаление сохранений
- Поддержка QuickSave и AutoSave

### Структура слотов

| Индекс | Тип | Описание |
|--------|-----|----------|
| 0-9 | Manual | Ручные сохранения пользователя |
| 100 | AutoSave | Автоматические сохранения (чекпоинты) |
| 101 | QuickSave | Быстрое сохранение (F5) |

### Путь сохранений

```
[Project]/Saved/SaveGames/[PlayerId]/
├── Slot_0.sav          # Ручной слот 0
├── Slot_1.sav          # Ручной слот 1
├── ...
├── Slot_100.sav        # AutoSave
└── Slot_101.sav        # QuickSave
```

---

## 2. Архитектура компонентов

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        SAVE/LOAD MENU ARCHITECTURE                           │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    USuspenseCoreSaveLoadMenuWidget                   │    │
│  │                                                                     │    │
│  │  ┌─────────────────────────────────────────────────────────────┐   │    │
│  │  │ Header                                                       │   │    │
│  │  │  TitleText: "SAVE GAME" / "LOAD GAME"                       │   │    │
│  │  └─────────────────────────────────────────────────────────────┘   │    │
│  │                                                                     │    │
│  │  ┌─────────────────────────────────────────────────────────────┐   │    │
│  │  │ SlotListScrollBox                                            │   │    │
│  │  │  ┌───────────────────────────────────────────────────────┐  │   │    │
│  │  │  │ USuspenseCoreSaveSlotWidget [QuickSave - Slot 101]    │  │   │    │
│  │  │  │  - SlotNameText: "Quick Save"                         │  │   │    │
│  │  │  │  - InfoText: "Lv.15 | Forest | 2:30:45"              │  │   │    │
│  │  │  │  - TimestampText: "2025-11-29 14:32"                  │  │   │    │
│  │  │  │  - DeleteButton                                        │  │   │    │
│  │  │  └───────────────────────────────────────────────────────┘  │   │    │
│  │  │  ┌───────────────────────────────────────────────────────┐  │   │    │
│  │  │  │ USuspenseCoreSaveSlotWidget [AutoSave - Slot 100]     │  │   │    │
│  │  │  │  - SlotNameText: "Auto Save"                          │  │   │    │
│  │  │  │  - InfoText: "Lv.15 | Cave | 2:45:12"                │  │   │    │
│  │  │  │  - TimestampText: "2025-11-29 14:45"                  │  │   │    │
│  │  │  │  - [No Delete Button for AutoSave]                    │  │   │    │
│  │  │  └───────────────────────────────────────────────────────┘  │   │    │
│  │  │  ┌───────────────────────────────────────────────────────┐  │   │    │
│  │  │  │ USuspenseCoreSaveSlotWidget [Slot 0]                  │  │   │    │
│  │  │  │  - SlotNameText: "My Save 1" / "Empty Slot"           │  │   │    │
│  │  │  │  - InfoText / EmptySlotText                           │  │   │    │
│  │  │  └───────────────────────────────────────────────────────┘  │   │    │
│  │  │  ... (Slots 1-9)                                            │   │    │
│  │  └─────────────────────────────────────────────────────────────┘   │    │
│  │                                                                     │    │
│  │  ┌─────────────────────────────────────────────────────────────┐   │    │
│  │  │ Footer                                                       │   │    │
│  │  │  StatusText: "Select a slot" / "Saving..." / "Loaded!"      │   │    │
│  │  │  [CloseButton: "BACK"]                                       │   │    │
│  │  └─────────────────────────────────────────────────────────────┘   │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Классы

| Класс | Файл | Назначение |
|-------|------|------------|
| `USuspenseCoreSaveSlotWidget` | `SuspenseCoreSaveSlotWidget.h/cpp` | Виджет одного слота |
| `USuspenseCoreSaveLoadMenuWidget` | `SuspenseCoreSaveLoadMenuWidget.h/cpp` | Полное меню |
| `USuspenseCoreSaveManager` | `SuspenseCoreSaveManager.h/cpp` | Логика сохранений |

---

## 3. Создание WBP_SaveSlot

### 3.1 Создание Blueprint

1. **Content Browser** → Right Click → **User Interface** → **Widget Blueprint**
2. **Parent Class**: `USuspenseCoreSaveSlotWidget`
3. **Name**: `WBP_SaveSlot`

### 3.2 Designer структура

**КРИТИЧНО:** Вся структура должна быть внутри `SlotButton`!

```
[Button] "SlotButton"                        ← ОБЯЗАТЕЛЬНО! Главная кнопка
│   Style: Transparent/None
│   Is Variable: ✓
│
└── [Border] "SlotBorder"                    ← Подсветка при выборе
    │   Brush Color: (0.1, 0.1, 0.1, 0.8)
    │   Padding: 10
    │   Is Variable: ✓
    │
    └── [Horizontal Box]
        │
        ├── [Image] "ThumbnailImage"         ← Превью (опционально)
        │       Size: 80x60
        │       Visibility: Collapsed
        │       Is Variable: ✓
        │
        ├── [Spacer] Width: 10
        │
        ├── [Vertical Box] (Expand: Fill)
        │   │
        │   ├── [Horizontal Box]
        │   │   │
        │   │   ├── [Text Block] "SlotNameText"      ← "Slot 1" / "Quick Save"
        │   │   │       Font Size: 16, Bold
        │   │   │       Is Variable: ✓
        │   │   │
        │   │   └── [Text Block] "TimestampText"     ← "Nov 29, 2025 14:32"
        │   │           Font Size: 12, Gray
        │   │           Alignment: Right
        │   │           Is Variable: ✓
        │   │
        │   ├── [Spacer] Height: 5
        │   │
        │   ├── [Horizontal Box]                     ← Данные сохранения
        │   │   │
        │   │   ├── [Text Block] "CharacterNameText" ← "Player Name"
        │   │   │       Font Size: 14
        │   │   │       Is Variable: ✓
        │   │   │
        │   │   ├── [Text Block] "LevelText"         ← "Lv. 15"
        │   │   │       Font Size: 14, Gray
        │   │   │       Padding Left: 10
        │   │   │       Is Variable: ✓
        │   │   │
        │   │   ├── [Text Block] "LocationText"      ← "Forest"
        │   │   │       Font Size: 14, Gray
        │   │   │       Padding Left: 10
        │   │   │       Is Variable: ✓
        │   │   │
        │   │   └── [Text Block] "PlaytimeText"      ← "2h 30m"
        │   │           Font Size: 14, Gray
        │   │           Padding Left: 10
        │   │           Is Variable: ✓
        │   │
        │   └── [Text Block] "EmptyText"             ← "- Empty Slot -"
        │           Font Size: 14
        │           Color: Dark Gray
        │           Visibility: Collapsed
        │           Is Variable: ✓
        │
        └── (опционально) [Button] "DeleteButton"   ← Можно не добавлять!
```

### 3.2.1 Упрощённая структура (без DeleteButton в слоте)

Если не нужна кнопка удаления прямо в слоте:

```
[Button] "SlotButton"                        ← ОБЯЗАТЕЛЬНО!
│
└── [Border] "SlotBorder"
    │
    └── [Vertical Box]
        │
        ├── [Text Block] "SlotNameText"       ← "Slot 1"
        ├── [Text Block] "CharacterNameText"  ← "Player"
        ├── [Text Block] "TimestampText"      ← "Nov 29, 2025"
        └── [Text Block] "EmptyText"          ← "- Empty -"
```

> **Примечание:** Удаление можно делать через `DeleteButton` в главном меню `WBP_SaveLoadMenu`.

### 3.3 BindWidget имена для SaveSlot

| Имя | Тип | Важность | Описание |
|-----|-----|----------|----------|
| `SlotButton` | UButton | **ОБЯЗАТЕЛЬНО** | Главная кнопка - без неё клики не работают! |
| `SlotBorder` | UBorder | Опционально | Рамка для подсветки |
| `SlotNameText` | UTextBlock | Опционально | "Slot 1" / "Quick Save" / "Auto Save" |
| `CharacterNameText` | UTextBlock | Опционально | Имя персонажа |
| `LevelText` | UTextBlock | Опционально | "Lv. 15" |
| `LocationText` | UTextBlock | Опционально | Название локации |
| `TimestampText` | UTextBlock | Опционально | Дата/время сохранения |
| `PlaytimeText` | UTextBlock | Опционально | Время игры "2h 30m" |
| `EmptyText` | UTextBlock | Опционально | Текст для пустых слотов |
| `DeleteButton` | UButton | Опционально | Удаление прямо из слота (можно не добавлять!) |
| `ThumbnailImage` | UImage | Опционально | Превью скриншот |

> **ВАЖНО:** `InfoText` НЕ существует! Используй отдельные поля (CharacterNameText, LevelText и т.д.)

### 3.4 Настройка в Class Defaults

```
Slot Widget | Appearance:
├── NormalBorderColor:    (0.1, 0.1, 0.1, 0.8)   # Обычное состояние
├── SelectedBorderColor:  (0.2, 0.5, 1.0, 1.0)   # Выбран (синий)
├── HoveredBorderColor:   (0.3, 0.3, 0.3, 1.0)   # Наведение
├── EmptySlotColor:       (0.5, 0.5, 0.5, 1.0)   # Пустой слот (серый)
└── OccupiedSlotColor:    (1.0, 1.0, 1.0, 1.0)   # Занятый слот (белый)

Slot Widget | Text:
├── EmptySlotDisplayText: "Empty Slot"
├── QuickSaveDisplayText: "Quick Save"
└── AutoSaveDisplayText:  "Auto Save"
```

### 3.5 Как работают события слота

```cpp
// NativeConstruct привязывает события к кнопкам
void USuspenseCoreSaveSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // SlotButton - ГЛАВНАЯ КНОПКА для выбора слота!
    if (SlotButton)
    {
        SlotButton->OnClicked.AddDynamic(this, &USuspenseCoreSaveSlotWidget::OnSlotButtonClicked);
        SlotButton->OnHovered.AddDynamic(this, &USuspenseCoreSaveSlotWidget::OnSlotButtonHovered);
        SlotButton->OnUnhovered.AddDynamic(this, &USuspenseCoreSaveSlotWidget::OnSlotButtonUnhovered);
    }

    // DeleteButton - кнопка удаления
    if (DeleteButton)
    {
        DeleteButton->OnClicked.AddDynamic(this, &USuspenseCoreSaveSlotWidget::OnDeleteButtonClicked);
    }
}

// При клике на слот вызывается делегат
void USuspenseCoreSaveSlotWidget::OnSlotButtonClicked()
{
    // Вызывает события для родительского меню
    OnSlotSelectedEvent(SlotIndex, bIsEmpty);
    OnSlotSelected.Broadcast(SlotIndex, bIsEmpty);
}

// При клике на DeleteButton
void USuspenseCoreSaveSlotWidget::OnDeleteButtonClicked()
{
    OnDeleteRequestedEvent(SlotIndex);
    OnDeleteRequested.Broadcast(SlotIndex);
}
```

### 3.6 Логика отображения UpdateDisplay()

```cpp
void USuspenseCoreSaveSlotWidget::UpdateDisplay()
{
    // Имя слота: "Slot 1" / "Quick Save" / "Auto Save"
    if (SlotNameText)
    {
        SlotNameText->SetText(FText::FromString(GetSlotDisplayName(SlotIndex)));
    }

    if (bIsEmpty)
    {
        // Показать текст пустого слота
        if (EmptyText) EmptyText->SetVisibility(ESlateVisibility::Visible);

        // Скрыть все поля данных
        if (CharacterNameText) CharacterNameText->SetVisibility(ESlateVisibility::Collapsed);
        if (LevelText) LevelText->SetVisibility(ESlateVisibility::Collapsed);
        if (LocationText) LocationText->SetVisibility(ESlateVisibility::Collapsed);
        if (TimestampText) TimestampText->SetVisibility(ESlateVisibility::Collapsed);
        if (PlaytimeText) PlaytimeText->SetVisibility(ESlateVisibility::Collapsed);
        if (DeleteButton) DeleteButton->SetVisibility(ESlateVisibility::Collapsed);
    }
    else
    {
        // Скрыть текст пустого слота
        if (EmptyText) EmptyText->SetVisibility(ESlateVisibility::Collapsed);

        // Показать данные сохранения
        if (CharacterNameText) CharacterNameText->SetText(FText::FromString(CachedHeader.CharacterName));
        if (LevelText) LevelText->SetText(FText::Format("Lv. {0}", CachedHeader.CharacterLevel));
        if (LocationText) LocationText->SetText(FText::FromString(CachedHeader.LocationName));
        if (TimestampText) TimestampText->SetText(FText::FromString(FormatTimestamp(CachedHeader.SaveTimestamp)));
        if (PlaytimeText) PlaytimeText->SetText(FText::FromString(FormatPlaytime(CachedHeader.TotalPlayTimeSeconds)));

        // Показать кнопку удаления (кроме AutoSave)
        if (DeleteButton && SlotIndex != AUTOSAVE_SLOT)
        {
            DeleteButton->SetVisibility(ESlateVisibility::Visible);
        }
    }
}
```

---

## 4. Создание WBP_SaveLoadMenu

### 4.1 Создание Blueprint

1. **Content Browser** → Right Click → **User Interface** → **Widget Blueprint**
2. **Parent Class**: `USuspenseCoreSaveLoadMenuWidget`
3. **Name**: `WBP_SaveLoadMenu`

### 4.2 Designer структура

```
[Canvas Panel] (Full Screen)
│
├── [Border] "BackgroundOverlay"             ← Затемнение фона
│       Brush Color: (0, 0, 0, 0.7)
│       Anchors: Full Stretch
│
└── [Border] "MenuContainer"                 ← Контейнер меню
    │   Brush Color: (0.05, 0.05, 0.05, 0.95)
    │   Anchors: Center
    │   Size: 600x500
    │   Padding: 20
    │
    └── [Vertical Box]
        │
        ├── [Text Block] "TitleText"         ← "SAVE GAME" / "LOAD GAME"
        │       Font Size: 28
        │       Font: Bold
        │       Alignment: Center
        │       Is Variable: ✓
        │
        ├── [Spacer] Height: 20
        │
        ├── [Scroll Box] "SlotsScrollBox"      ← Опционально, для скролла
        │   │   Orientation: Vertical
        │   │   Size: Fill
        │   │   Is Variable: ✓
        │   │
        │   └── [Vertical Box] "SlotsContainer" ← ОБЯЗАТЕЛЬНО! Контейнер слотов
        │           Is Variable: ✓
        │           (Динамически заполняется WBP_SaveSlot виджетами)
        │
        ├── [Spacer] Height: 15
        │
        ├── [Text Block] "StatusText"        ← Статус операции
        │       Font Size: 14
        │       Color: Yellow
        │       Alignment: Center
        │       Is Variable: ✓
        │
        ├── [Spacer] Height: 15
        │
        └── [Horizontal Box]                  ← КНОПКИ ДЕЙСТВИЙ
            │
            ├── [Button] "ActionButton"       ← **ОБЯЗАТЕЛЬНО!** Save/Load кнопка
            │   │   Style: Primary/Accent
            │   │   Is Variable: ✓
            │   │
            │   └── [Text Block] "ActionButtonText"
            │           Text: "SAVE" (меняется автоматически)
            │           Is Variable: ✓
            │
            ├── [Spacer] Width: 10
            │
            ├── [Button] "DeleteButton"       ← Опционально: удаление слота
            │   │   Style: Danger/Red
            │   │   Is Variable: ✓
            │   │   Visibility: Collapsed (показывается когда можно удалить)
            │   │
            │   └── [Text Block] "DeleteButtonText"
            │           Text: "DELETE"
            │
            ├── [Spacer] (Fill)
            │
            └── [Button] "CloseButton"        ← Кнопка "BACK"
                │   Style: Secondary
                │   Is Variable: ✓
                │
                └── [Text Block] "CloseButtonText"
                        Text: "BACK"
                        Is Variable: ✓
```

### 4.2.1 Панель подтверждения (опционально)

Для подтверждения перезаписи/удаления/загрузки:

```
[Widget] "ConfirmationOverlay"               ← Overlay для подтверждения
│   Visibility: Collapsed (по умолчанию)
│   Is Variable: ✓
│
└── [Border]
    │   Brush Color: (0, 0, 0, 0.8)
    │   Anchors: Full Stretch
    │
    └── [Vertical Box]
        │   Alignment: Center
        │
        ├── [Text Block] "ConfirmationText"   ← "Overwrite existing save?"
        │       Is Variable: ✓
        │
        ├── [Spacer] Height: 20
        │
        └── [Horizontal Box]
            │
            ├── [Button] "ConfirmButton"      ← "YES" / "CONFIRM"
            │       Is Variable: ✓
            │
            ├── [Spacer] Width: 20
            │
            └── [Button] "CancelButton"       ← "NO" / "CANCEL"
                    Is Variable: ✓
```

### 4.3 BindWidget имена

| Имя | Тип | Важность | Описание |
|-----|-----|----------|----------|
| `SlotsContainer` | UVerticalBox | **ОБЯЗАТЕЛЬНО** | Контейнер для слотов |
| `ActionButton` | UButton | **РЕКОМЕНДУЕТСЯ** | Кнопка Save/Load - без неё нельзя сохранять! |
| `ActionButtonText` | UTextBlock | Рекомендуется | Текст (меняется: "SAVE"/"LOAD") |
| `TitleText` | UTextBlock | Опционально | Заголовок ("SAVE GAME"/"LOAD GAME") |
| `SlotsScrollBox` | UScrollBox | Опционально | Для скролла |
| `StatusText` | UTextBlock | Опционально | "Game saved!" / "Loading..." |
| `DeleteButton` | UButton | Опционально | Удаление выбранного слота |
| `CloseButton` | UButton | Опционально | Кнопка "BACK" |
| `CloseButtonText` | UTextBlock | Опционально | Текст кнопки Close |
| `ConfirmationOverlay` | UWidget | Опционально | Панель подтверждения |
| `ConfirmationText` | UTextBlock | Опционально | Текст подтверждения |
| `ConfirmButton` | UButton | Опционально | Кнопка "YES" |
| `CancelButton` | UButton | Опционально | Кнопка "NO" |

**ВАЖНО:**
- `SlotsContainer` = **UVerticalBox** (не ScrollBox!)
- Без `ActionButton` сохранение/загрузка не будет работать!

### 4.4 Настройка в Class Defaults (КРИТИЧЕСКИ ВАЖНО!)

**Шаги для настройки SaveSlotWidgetClass:**

1. Откройте Blueprint `WBP_SaveLoadMenu` в редакторе
2. Нажмите **Class Defaults** в тулбаре (или Alt+Enter)
3. В панели **Details** найдите категорию **"SuspenseCore | Config"**
4. Найдите параметр **"Save Slot Widget Class"**
5. Выберите из выпадающего списка **WBP_SaveSlot**

```
┌─────────────────────────────────────────────────────────────┐
│  WBP_SaveLoadMenu - Class Defaults                          │
├─────────────────────────────────────────────────────────────┤
│  ▼ SuspenseCore | Config                                    │
│    ┌─────────────────────────────────────────────────────┐  │
│    │ Save Slot Widget Class: [WBP_SaveSlot        ▼]    │  │ ← УСТАНОВИТЬ!
│    │ Save Mode Title:        "SAVE GAME"                │  │
│    │ Load Mode Title:        "LOAD GAME"                │  │
│    │ Num Manual Slots:       10                         │  │
│    │ Show Quick Save Slot:   ✓                          │  │
│    │ Show Auto Save Slot:    ✓                          │  │
│    └─────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

**⚠️ БЕЗ ЭТОЙ НАСТРОЙКИ СЛОТЫ НЕ БУДУТ ОТОБРАЖАТЬСЯ!**

**Полные настройки:**

```
SaveLoad Menu | Config:
├── SaveSlotWidgetClass:    WBP_SaveSlot     # ОБЯЗАТЕЛЬНО!
├── MaxManualSlots:         10               # Слоты 0-9
├── bShowQuickSaveSlot:     true             # Показать QuickSave
├── bShowAutoSaveSlot:      true             # Показать AutoSave

SaveLoad Menu | Text:
├── SaveModeTitle:          "SAVE GAME"
├── LoadModeTitle:          "LOAD GAME"
├── SelectSlotText:         "Select a slot"
├── SavingText:             "Saving..."
├── LoadingText:            "Loading..."
├── SaveSuccessText:        "Game Saved!"
├── LoadSuccessText:        "Game Loaded!"
├── OperationFailedText:    "Operation failed"
├── ConfirmOverwriteText:   "Overwrite existing save?"
├── ConfirmDeleteText:      "Delete this save?"
```

### 4.5 Режимы работы

```cpp
// Enum режимов
UENUM(BlueprintType)
enum class ESuspenseCoreSaveLoadMode : uint8
{
    Save,   // Режим сохранения
    Load    // Режим загрузки
};

// Открытие меню в нужном режиме
void USuspenseCoreSaveLoadMenuWidget::ShowMenu(ESuspenseCoreSaveLoadMode Mode)
{
    CurrentMode = Mode;

    // Установить заголовок
    if (TitleText)
    {
        TitleText->SetText(Mode == ESuspenseCoreSaveLoadMode::Save
            ? SaveModeTitle
            : LoadModeTitle);
    }

    // Обновить список слотов
    RefreshSlots();

    // Показать меню
    SetVisibility(ESlateVisibility::Visible);
    SetUIInputMode();
}
```

---

## 5. Интеграция с PauseMenu

### 5.1 Настройка WBP_PauseMenu

В **Class Defaults** вашего `WBP_PauseMenu`:

```
PauseMenu | Config | SaveLoad:
└── SaveLoadMenuWidgetClass: WBP_SaveLoadMenu
```

### 5.2 Как это работает

Кнопки Save/Load в PauseMenu автоматически открывают SaveLoadMenu:

```cpp
// В SuspenseCorePauseMenuWidget.cpp

void USuspenseCorePauseMenuWidget::OnSaveButtonClicked()
{
    if (SaveLoadMenuWidgetClass)
    {
        ShowSaveLoadMenu(ESuspenseCoreSaveLoadMode::Save);
    }
    else
    {
        // Fallback: Quick Save
        if (USuspenseCoreSaveManager* SaveMgr = GetSaveManager())
        {
            SaveMgr->QuickSave();
            ShowStatus(TEXT("Quick Saved!"));
        }
    }
}

void USuspenseCorePauseMenuWidget::OnLoadButtonClicked()
{
    if (SaveLoadMenuWidgetClass)
    {
        ShowSaveLoadMenu(ESuspenseCoreSaveLoadMode::Load);
    }
    else
    {
        // Fallback: Quick Load
        if (USuspenseCoreSaveManager* SaveMgr = GetSaveManager())
        {
            SaveMgr->QuickLoad();
        }
    }
}

void USuspenseCorePauseMenuWidget::ShowSaveLoadMenu(ESuspenseCoreSaveLoadMode Mode)
{
    if (!SaveLoadMenuWidget)
    {
        CreateSaveLoadMenu();
    }

    if (SaveLoadMenuWidget)
    {
        SaveLoadMenuWidget->ShowMenu(Mode);
    }
}
```

### 5.3 Диаграмма потока

```
┌─────────────────┐
│   PauseMenu     │
│                 │
│  [Save Game] ───┼──► ShowSaveLoadMenu(Save)
│                 │           │
│  [Load Game] ───┼──► ShowSaveLoadMenu(Load)
│                 │           │
└─────────────────┘           │
                              ▼
                    ┌─────────────────────┐
                    │   SaveLoadMenu      │
                    │                     │
                    │  ┌───────────────┐  │
                    │  │ Quick Save    │◄─┼── Slot 101
                    │  ├───────────────┤  │
                    │  │ Auto Save     │◄─┼── Slot 100
                    │  ├───────────────┤  │
                    │  │ Slot 0        │  │
                    │  ├───────────────┤  │
                    │  │ Slot 1        │  │
                    │  ├───────────────┤  │
                    │  │ ...           │  │
                    │  └───────────────┘  │
                    │                     │
                    │  [BACK] ────────────┼──► CloseMenu()
                    └─────────────────────┘         │
                                                   │
                              ┌─────────────────────┘
                              ▼
                    ┌─────────────────┐
                    │   PauseMenu     │
                    │  (восстановлен) │
                    └─────────────────┘
```

---

## 6. Настройка слотов

### 6.1 Количество слотов

```cpp
// В Class Defaults SaveLoadMenuWidget
MaxManualSlots = 10;  // Слоты 0-9

// Специальные слоты (константы)
static constexpr int32 AUTOSAVE_SLOT = 100;
static constexpr int32 QUICKSAVE_SLOT = 101;
```

### 6.2 Показ/скрытие специальных слотов

```cpp
// В Class Defaults
bShowQuickSaveSlot = true;   // Показать Quick Save слот
bShowAutoSaveSlot = true;    // Показать Auto Save слот
```

### 6.3 Кастомизация отображения

```cpp
// В WBP_SaveSlot Blueprint или C++
// Переопределите FormatSlotInfo для кастомного формата

FText USuspenseCoreSaveSlotWidget::FormatSlotInfo(const FSuspenseCoreSaveHeader& Header)
{
    // Формат по умолчанию: "Lv.15 | Forest | 2:30:45"

    // Ваш кастомный формат:
    return FText::Format(
        "{0} - Level {1} - {2}",
        Header.CharacterName,
        Header.CharacterLevel,
        Header.MapDisplayName
    );
}
```

---

## 7. API SaveManager

### 7.1 Основные методы

```cpp
// Получение SaveManager
USuspenseCoreSaveManager* SaveMgr = USuspenseCoreSaveManager::Get(this);

// Установить текущего игрока
SaveMgr->SetCurrentPlayer(PlayerId);

// Quick Save/Load
SaveMgr->QuickSave();
SaveMgr->QuickLoad();
bool bHasQuickSave = SaveMgr->HasQuickSave();

// Сохранение в слот
SaveMgr->SaveToSlot(SlotIndex, SlotName);
SaveMgr->LoadFromSlot(SlotIndex);
SaveMgr->DeleteSlot(SlotIndex);

// Проверки
bool bExists = SaveMgr->SlotExists(SlotIndex);
int32 MaxSlots = SaveMgr->GetMaxSlots();

// Получить все заголовки для UI
TArray<FSuspenseCoreSaveHeader> Headers = SaveMgr->GetAllSlotHeaders();

// Auto-Save
SaveMgr->SetAutoSaveEnabled(true);
SaveMgr->SetAutoSaveInterval(300.0f);  // 5 минут
SaveMgr->TriggerAutoSave();            // Ручной триггер
bool bHasAutoSave = SaveMgr->HasAutoSave();

// Статус
bool bIsSaving = SaveMgr->IsSaving();
bool bIsLoading = SaveMgr->IsLoading();
```

### 7.2 События (Delegates)

```cpp
// Подписка на события
SaveMgr->OnSaveStarted.AddDynamic(this, &UMyWidget::HandleSaveStarted);
SaveMgr->OnSaveCompleted.AddDynamic(this, &UMyWidget::HandleSaveCompleted);
SaveMgr->OnLoadStarted.AddDynamic(this, &UMyWidget::HandleLoadStarted);
SaveMgr->OnLoadCompleted.AddDynamic(this, &UMyWidget::HandleLoadCompleted);

// Обработчики
void UMyWidget::HandleSaveCompleted(bool bSuccess, const FString& ErrorMessage)
{
    if (bSuccess)
    {
        ShowStatus(TEXT("Game Saved!"));
    }
    else
    {
        ShowStatus(FString::Printf(TEXT("Save failed: %s"), *ErrorMessage));
    }
}
```

### 7.3 Структура FSuspenseCoreSaveHeader

```cpp
USTRUCT(BlueprintType)
struct FSuspenseCoreSaveHeader
{
    GENERATED_BODY()

    // Идентификация
    UPROPERTY() int32 SlotIndex;
    UPROPERTY() FString SlotName;
    UPROPERTY() FString PlayerId;

    // Информация о персонаже
    UPROPERTY() FString CharacterName;
    UPROPERTY() int32 CharacterLevel;
    UPROPERTY() float HealthPercent;

    // Мир
    UPROPERTY() FName MapName;
    UPROPERTY() FString MapDisplayName;
    UPROPERTY() FVector PlayerLocation;

    // Время
    UPROPERTY() FDateTime SaveTimestamp;
    UPROPERTY() int64 TotalPlayTime;  // В секундах

    // Метаданные
    UPROPERTY() FString SaveVersion;
    UPROPERTY() bool bIsAutoSave;
    UPROPERTY() bool bIsQuickSave;
};
```

---

## 8. Тестирование

### 8.1 Быстрый тест

1. Запустите игру на GameMap
2. Нажмите **Q** или **Escape** для паузы
3. Нажмите **Save Game**
4. Должно открыться меню со слотами
5. Выберите слот - должно сохраниться
6. Проверьте `[Project]/Saved/SaveGames/[PlayerId]/`

### 8.2 Тест загрузки

1. Сохраните игру в слот
2. Переместите персонажа в другое место
3. Откройте **Load Game**
4. Выберите сохранённый слот
5. Персонаж должен вернуться на сохранённую позицию

### 8.3 Тест Quick Save/Load

1. Нажмите **F5** (Quick Save)
2. Должно появиться сообщение "Quick Saved!"
3. Переместитесь
4. Нажмите **F9** (Quick Load)
5. Должны вернуться на позицию Quick Save

### 8.4 Логи для отладки

```cpp
// Включить подробные логи в DefaultEngine.ini
[Core.Log]
LogSuspenseCoreSaveLoadMenu=Verbose
LogSuspenseCoreSaveSlot=Verbose
LogSuspenseCoreSaveManager=Verbose
```

---

## 9. Troubleshooting

### Проблема: Меню не открывается

**Причина:** `SaveLoadMenuWidgetClass` не установлен в PauseMenu

**Решение:**
1. Откройте `WBP_PauseMenu`
2. В Class Defaults найдите `SaveLoadMenuWidgetClass`
3. Установите `WBP_SaveLoadMenu`

---

### Проблема: Слоты не отображаются

**Причина:** `SaveSlotWidgetClass` не установлен в SaveLoadMenu

**Решение:**
1. Откройте `WBP_SaveLoadMenu`
2. В Class Defaults найдите `SaveSlotWidgetClass`
3. Установите `WBP_SaveSlot`

---

### Проблема: Сохранение не создаётся

**Причина:** CurrentPlayer не установлен

**Решение:**
```cpp
// В вашем GameMode или при входе в игру:
if (USuspenseCoreSaveManager* SaveMgr = USuspenseCoreSaveManager::Get(this))
{
    SaveMgr->SetCurrentPlayer(PlayerId);
}
```

---

### Проблема: Пустые данные в слоте

**Причина:** CharacterState не собирается

**Решение:** Убедитесь, что ваш Character реализует сбор состояния:

```cpp
// В вашем Character:
FSuspenseCoreCharacterState ASuspenseCoreCharacter::CollectCharacterState()
{
    FSuspenseCoreCharacterState State;
    State.Transform = GetActorTransform();
    State.Health = GetHealth();
    // ... другие данные
    return State;
}
```

---

### Проблема: Input не работает после закрытия меню

**Причина:** Input Mode не восстановлен

**Решение:** Это автоматически обрабатывается в `OnMenuClosed`, но проверьте:

```cpp
void USuspenseCoreSaveLoadMenuWidget::CloseMenu()
{
    SetVisibility(ESlateVisibility::Collapsed);
    RestoreGameInputMode();  // Восстанавливает GameOnly режим
    OnMenuClosed.Broadcast();
}
```

---

## Чеклист настройки

- [ ] Создан `WBP_SaveSlot` (Parent: `USuspenseCoreSaveSlotWidget`)
- [ ] Все BindWidget элементы названы корректно в `WBP_SaveSlot`
- [ ] Создан `WBP_SaveLoadMenu` (Parent: `USuspenseCoreSaveLoadMenuWidget`)
- [ ] Все BindWidget элементы названы корректно в `WBP_SaveLoadMenu`
- [ ] `SaveSlotWidgetClass` установлен в `WBP_SaveLoadMenu` → Class Defaults
- [ ] `SaveLoadMenuWidgetClass` установлен в `WBP_PauseMenu` → Class Defaults
- [ ] `SetCurrentPlayer()` вызывается при старте игры
- [ ] Input Actions (F5, F9) настроены для Quick Save/Load
- [ ] Протестировано сохранение в слот
- [ ] Протестирована загрузка из слота
- [ ] Протестирован Quick Save/Load

---

*Документация создана: 2025-11-29*
*SuspenseCore Clean Architecture*
