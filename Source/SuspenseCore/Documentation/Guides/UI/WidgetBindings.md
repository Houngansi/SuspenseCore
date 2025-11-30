# Widget Bindings Reference

> Полный справочник по BindWidget именам для всех виджетов SuspenseCore
> Версия: 1.0 | Дата: 2025-11-29

---

## О BindWidget

`UPROPERTY(meta = (BindWidget))` связывает C++ переменную с виджетом в UMG Designer.

**Правила:**
- Имя виджета в Designer должно **точно** совпадать с именем переменной в C++
- Виджет должен иметь флаг **Is Variable** в Designer
- Тип виджета должен совпадать или быть наследником

**Пример:**
```cpp
// C++
UPROPERTY(meta = (BindWidget))
UButton* PlayButton;  // Имя: PlayButton

// Designer: создать Button с именем "PlayButton", включить "Is Variable"
```

---

## USuspenseCoreMainMenuWidget

**Blueprint:** `WBP_MainMenu`

| Имя | Тип | Required | Описание |
|-----|-----|----------|----------|
| `ScreenSwitcher` | `UWidgetSwitcher` | **✓** | Переключатель экранов (3 индекса) |
| `CharacterSelectWidget` | `USuspenseCoreCharacterSelectWidget` | **✓** | Index 0 |
| `RegistrationWidget` | `USuspenseCoreRegistrationWidget` | **✓** | Index 1 |
| `PlayerInfoWidget` | `USuspenseCorePlayerInfoWidget` | | Index 2 |
| `PlayButton` | `UButton` | **✓** | Кнопка "Играть" |
| `PlayButtonText` | `UTextBlock` | | Текст кнопки Play |
| `OperatorsButton` | `UButton` | | Кнопка операторов |
| `SettingsButton` | `UButton` | | Кнопка настроек |
| `QuitButton` | `UButton` | | Кнопка выхода |
| `QuitButtonText` | `UTextBlock` | | Текст кнопки Quit |
| `BackgroundImage` | `UImage` | | Фоновое изображение |
| `GameTitleText` | `UTextBlock` | | Название игры |
| `VersionText` | `UTextBlock` | | Версия игры |

---

## USuspenseCoreRegistrationWidget

**Blueprint:** `WBP_Registration`

| Имя | Тип | Required | Описание |
|-----|-----|----------|----------|
| `DisplayNameInput` | `UEditableTextBox` | **✓** | Поле ввода имени |
| `CreateButton` | `UButton` | **✓** | Кнопка создания |
| `StatusText` | `UTextBlock` | **✓** | Статус/ошибки |
| `TitleText` | `UTextBlock` | | Заголовок формы |
| `CreateButtonText` | `UTextBlock` | | Текст кнопки Create |
| `BackButton` | `UButton` | | Кнопка "Назад" |
| `BackButtonText` | `UTextBlock` | | Текст кнопки Back |

---

## USuspenseCoreCharacterSelectWidget

**Blueprint:** `WBP_CharacterSelect`

| Имя | Тип | Required | Описание |
|-----|-----|----------|----------|
| `CharacterListScrollBox` | `UScrollBox` | **✓** | Список персонажей |
| `PlayButton` | `UButton` | **✓** | Кнопка "Играть" |
| `CreateNewButton` | `UButton` | **✓** | Создать нового |
| `TitleText` | `UTextBlock` | | Заголовок |
| `StatusText` | `UTextBlock` | | Статус (если нет персонажей) |
| `DeleteButton` | `UButton` | | Удалить персонажа |
| `DeleteButtonText` | `UTextBlock` | | Текст кнопки Delete |
| `PlayButtonText` | `UTextBlock` | | Текст кнопки Play |
| `CreateNewButtonText` | `UTextBlock` | | Текст кнопки Create New |

**Class Defaults:**
- `CharacterEntryWidgetClass` = `WBP_CharacterEntry`

---

## USuspenseCoreCharacterEntryWidget

**Blueprint:** `WBP_CharacterEntry`

| Имя | Тип | Required | Описание |
|-----|-----|----------|----------|
| `DisplayNameText` | `UTextBlock` | **✓** | Имя персонажа |
| `EntryBorder` | `UBorder` | | Рамка для подсветки |
| `SelectButton` | `UButton` | | Кнопка выбора |
| `AvatarImage` | `UImage` | | Аватар персонажа |
| `LevelText` | `UTextBlock` | | Уровень персонажа |

---

## USuspenseCorePlayerInfoWidget

**Blueprint:** `WBP_PlayerInfo`

| Имя | Тип | Required | Описание |
|-----|-----|----------|----------|
| `DisplayNameText` | `UTextBlock` | **✓** | Имя игрока |
| `PlayerIdText` | `UTextBlock` | | ID игрока |
| `LevelText` | `UTextBlock` | | Уровень |
| `XPProgressBar` | `UProgressBar` | | Прогресс XP |
| `XPText` | `UTextBlock` | | "X / Y XP" |
| `SoftCurrencyText` | `UTextBlock` | | Мягкая валюта |
| `HardCurrencyText` | `UTextBlock` | | Жёсткая валюта |
| `KillsText` | `UTextBlock` | | Убийства |
| `DeathsText` | `UTextBlock` | | Смерти |
| `KDRatioText` | `UTextBlock` | | K/D Ratio |
| `WinsText` | `UTextBlock` | | Победы |
| `MatchesText` | `UTextBlock` | | Матчи |
| `PlaytimeText` | `UTextBlock` | | Время игры |
| `RefreshButton` | `UButton` | | Обновить данные |

---

## USuspenseCorePauseMenuWidget

**Blueprint:** `WBP_PauseMenu`

| Имя | Тип | Required | Описание |
|-----|-----|----------|----------|
| `ContinueButton` | `UButton` | **✓** | Продолжить |
| `SaveButton` | `UButton` | **✓** | Сохранить |
| `LoadButton` | `UButton` | **✓** | Загрузить |
| `ExitToLobbyButton` | `UButton` | **✓** | Выход в лобби |
| `QuitButton` | `UButton` | | Выход из игры |
| `TitleText` | `UTextBlock` | | Заголовок "PAUSED" |
| `ContinueButtonText` | `UTextBlock` | | Текст Continue |
| `SaveButtonText` | `UTextBlock` | | Текст Save |
| `LoadButtonText` | `UTextBlock` | | Текст Load |
| `ExitToLobbyButtonText` | `UTextBlock` | | Текст Exit |
| `QuitButtonText` | `UTextBlock` | | Текст Quit |
| `SaveStatusText` | `UTextBlock` | | Статус сохранения |
| `BackgroundOverlay` | `UBorder/UImage` | | Затемнение фона |
| `MenuContainer` | `UBorder` | | Контейнер меню |

**Class Defaults:**
- `SaveLoadMenuWidgetClass` = `WBP_SaveLoadMenu` (опционально)

---

## USuspenseCoreSaveLoadMenuWidget

**Blueprint:** `WBP_SaveLoadMenu`

| Имя | Тип | Required | Описание |
|-----|-----|----------|----------|
| `SlotsContainer` | `UVerticalBox` | **✓** | Контейнер для слотов (ОБЯЗАТЕЛЬНО!) |
| `TitleText` | `UTextBlock` | | "SAVE GAME" / "LOAD GAME" |
| `SlotsScrollBox` | `UScrollBox` | | ScrollBox для скролла |
| `CloseButton` | `UButton` | | Кнопка закрытия |
| `ActionButton` | `UButton` | | Кнопка Save/Load |
| `ActionButtonText` | `UTextBlock` | | Текст кнопки |
| `DeleteButton` | `UButton` | | Кнопка удаления |
| `StatusText` | `UTextBlock` | | Статус операции |
| `CloseButtonText` | `UTextBlock` | | Текст кнопки Close |
| `BackgroundOverlay` | `UBorder` | | Затемнение фона |
| `MenuContainer` | `UBorder` | | Контейнер меню |

**Class Defaults:**
- `SaveSlotWidgetClass` = `WBP_SaveSlot` (**ОБЯЗАТЕЛЬНО!**)

**ВАЖНО:** `SlotsContainer` - это `UVerticalBox` внутри `SlotsScrollBox`!

---

## USuspenseCoreSaveSlotWidget

**Blueprint:** `WBP_SaveSlot`

| Имя | Тип | Required | Описание |
|-----|-----|----------|----------|
| `SlotButton` | `UButton` | **✓** | **ГЛАВНАЯ КНОПКА!** Без неё клики не работают |
| `SlotBorder` | `UBorder` | | Рамка для подсветки выбора |
| `SlotNameText` | `UTextBlock` | | "Slot 1" / "Quick Save" / "Auto Save" |
| `CharacterNameText` | `UTextBlock` | | Имя персонажа |
| `LevelText` | `UTextBlock` | | "Lv. 15" |
| `LocationText` | `UTextBlock` | | Название локации |
| `TimestampText` | `UTextBlock` | | "Nov 29, 2025 14:32" |
| `PlaytimeText` | `UTextBlock` | | "2h 30m" |
| `EmptyText` | `UTextBlock` | | "- Empty Slot -" (для пустых слотов) |
| `DeleteButton` | `UButton` | | Кнопка удаления |
| `ThumbnailImage` | `UImage` | | Превью скриншот |

**КРИТИЧНО:** `SlotButton` должен оборачивать весь контент слота!

---

## Быстрая проверка

### Критически важные виджеты

```
WBP_MainMenu:
  ✓ ScreenSwitcher (UWidgetSwitcher)
  ✓ CharacterSelectWidget
  ✓ RegistrationWidget
  ✓ PlayButton

WBP_Registration:
  ✓ DisplayNameInput
  ✓ CreateButton
  ✓ StatusText

WBP_CharacterSelect:
  ✓ CharacterListScrollBox
  ✓ PlayButton
  ✓ CreateNewButton

WBP_CharacterEntry:
  ✓ DisplayNameText

WBP_PlayerInfo:
  ✓ DisplayNameText

WBP_PauseMenu:
  ✓ ContinueButton
  ✓ SaveButton
  ✓ LoadButton
  ✓ ExitToLobbyButton

WBP_SaveLoadMenu:
  ✓ SlotsContainer (UVerticalBox) ← ОБЯЗАТЕЛЬНО!
  + TitleText (опционально)
  + CloseButton (опционально)

WBP_SaveSlot:
  ✓ SlotButton (UButton) ← ОБЯЗАТЕЛЬНО! Главная кнопка
  + SlotBorder (опционально)
  + SlotNameText (опционально)
  + CharacterNameText (опционально)
  + LevelText (опционально)
  + LocationText (опционально)
  + TimestampText (опционально)
  + PlaytimeText (опционально)
  + EmptyText (опционально)
  + DeleteButton (опционально)
```

**ВАЖНО:** Без `SlotButton` клики на слоты не будут работать!

---

## Типичные ошибки

### 1. Имя не совпадает

```
C++:     UButton* PlayButton;
Designer: "Play_Button"  ← НЕПРАВИЛЬНО! Должно быть "PlayButton"
```

### 2. Нет флага Is Variable

```
C++:     UButton* PlayButton;
Designer: "PlayButton" но Is Variable = false  ← НЕПРАВИЛЬНО!
```

### 3. Неверный тип

```
C++:     UButton* PlayButton;
Designer: "PlayButton" как TextBlock  ← НЕПРАВИЛЬНО! Должен быть Button
```

### 4. Виджет в неправильном месте

```
C++:     USuspenseCoreCharacterSelectWidget* CharacterSelectWidget;
Designer: CharacterSelectWidget НЕ в ScreenSwitcher Index 0  ← НЕПРАВИЛЬНО!
```

---

## Отладка

Если виджет не связывается:

1. **Проверьте логи** при запуске - будет ошибка BindWidget
2. **Проверьте имя** - точное совпадение, включая регистр
3. **Проверьте Is Variable** - должен быть включён
4. **Проверьте тип** - должен совпадать с C++
5. **Перекомпилируйте** - Blueprint может требовать перекомпиляции

---

*Документация создана: 2025-11-29*
*SuspenseCore Clean Architecture*
