# Input Mode Handling - CRITICAL GUIDE

> **КРИТИЧЕСКИ ВАЖНАЯ** информация об обработке Input Mode при переходах между картами
> Версия: 1.0 | Дата: 2025-11-29

---

## ВАЖНО - ПРОЧИТАЙТЕ ЭТО

Этот документ описывает **критическую проблему**, которая может привести к тому, что:
- Q/Escape не открывает меню паузы
- WASD не работает для движения
- F5/F9 не работают для Quick Save/Load
- Весь игровой ввод не работает после перехода из MainMenu

---

## Содержание

1. [Проблема](#1-проблема)
2. [Причина](#2-причина)
3. [Решение](#3-решение)
4. [Детальное объяснение](#4-детальное-объяснение)
5. [Проверка](#5-проверка)
6. [Частые ошибки](#6-частые-ошибки)

---

## 1. Проблема

### Симптомы

После перехода из MainMenu в GameMap:
- Нажатие Q или Escape ничего не делает
- WASD не перемещает персонажа
- F5/F9 не работают
- Любой игровой ввод игнорируется

### Когда проявляется

```
MainMenuMap ──► GameMap
     │              │
     │              └── Input не работает!
     │
     └── Input работает (UI mode)
```

### Важное наблюдение

Если запустить игру **напрямую на GameMap** (минуя MainMenu):
- Всё работает корректно!
- Q открывает паузу
- WASD перемещает персонажа

Проблема возникает **только** при переходе MainMenu → GameMap.

---

## 2. Причина

### Input Mode в Unreal Engine

Unreal Engine имеет 3 режима ввода:

| Режим | Описание | Использование |
|-------|----------|---------------|
| `GameOnly` | Только игровой ввод | Геймплей |
| `UIOnly` | Только UI ввод | Меню |
| `GameAndUI` | Оба режима | Инвентарь в игре |

### Что происходит

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        INPUT MODE PERSISTENCE BUG                            │
│                                                                             │
│  MainMenuMap                                                                │
│  ┌───────────────────────────────────────────────────────────────────┐      │
│  │  MenuPlayerController::BeginPlay()                                 │      │
│  │  {                                                                 │      │
│  │      SetInputMode(FInputModeUIOnly());  ◄── Устанавливает UIOnly  │      │
│  │      bShowMouseCursor = true;                                      │      │
│  │  }                                                                 │      │
│  └───────────────────────────────────────────────────────────────────┘      │
│                               │                                             │
│                               │ OpenLevel("GameMap")                        │
│                               ▼                                             │
│  GameMap                                                                    │
│  ┌───────────────────────────────────────────────────────────────────┐      │
│  │  SuspenseCorePlayerController::BeginPlay()                         │      │
│  │  {                                                                 │      │
│  │      Super::BeginPlay();                                           │      │
│  │      // Input Mode НЕ СБРАСЫВАЕТСЯ!                               │      │
│  │      // Остаётся UIOnly от MenuPlayerController!                  │      │
│  │  }                                                                 │      │
│  │                                                                    │      │
│  │  InputMode = UIOnly  ◄── ПРОБЛЕМА! Должен быть GameOnly           │      │
│  └───────────────────────────────────────────────────────────────────┘      │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Почему это происходит?

1. `MenuPlayerController` устанавливает `UIOnly` режим
2. При `OpenLevel()` создаётся новый `PlayerController`
3. Unreal Engine **НЕ сбрасывает** Input Mode автоматически
4. Новый `SuspenseCorePlayerController` наследует состояние
5. Input Mode остаётся `UIOnly` → игровой ввод не работает

---

## 3. Решение

### Обязательный код в PlayerController

В `ASuspenseCorePlayerController::BeginPlay()` **ОБЯЗАТЕЛЬНО** добавьте:

```cpp
void ASuspenseCorePlayerController::BeginPlay()
{
    Super::BeginPlay();

    // ═══════════════════════════════════════════════════════════════════════
    // CRITICAL: Explicitly set Input Mode to GameOnly
    // This is REQUIRED after transitioning from MainMenu (which uses UIOnly)
    // Without this, Q-pause, WASD movement, and all game input will not work!
    // ═══════════════════════════════════════════════════════════════════════
    if (IsLocalController())
    {
        FInputModeGameOnly InputMode;
        SetInputMode(InputMode);
        bShowMouseCursor = false;

        UE_LOG(LogSuspenseCoreController, Log,
            TEXT("BeginPlay: Set InputMode to GameOnly"));
    }

    // Rest of initialization...
    SetupEnhancedInput();
    CreatePauseMenu();
}
```

### Полный пример

```cpp
// SuspenseCorePlayerController.cpp

#include "SuspenseCore/Core/SuspenseCorePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"

void ASuspenseCorePlayerController::BeginPlay()
{
    Super::BeginPlay();

    // ═══════════════════════════════════════════════════════════════
    // CRITICAL: Set Input Mode to GameOnly
    // Required after map transition from MainMenu
    // ═══════════════════════════════════════════════════════════════
    if (IsLocalController())
    {
        FInputModeGameOnly InputMode;
        SetInputMode(InputMode);
        bShowMouseCursor = false;
    }

    // Setup Enhanced Input
    SetupEnhancedInput();

    // Create Pause Menu
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

void ASuspenseCorePlayerController::SetupEnhancedInput()
{
    if (!IsLocalController()) return;

    // Add Input Mapping Context
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        if (DefaultMappingContext)
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }
}
```

---

## 4. Детальное объяснение

### 4.1 Input Mode и Map Transitions

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      CORRECT INPUT MODE HANDLING                             │
│                                                                             │
│  MainMenuMap                          GameMap                               │
│  ┌─────────────────────┐              ┌─────────────────────┐               │
│  │ MenuPlayerController│              │ SuspenseCorePC      │               │
│  │                     │              │                     │               │
│  │ BeginPlay:          │              │ BeginPlay:          │               │
│  │ ┌─────────────────┐ │  OpenLevel   │ ┌─────────────────┐ │               │
│  │ │ SetInputMode    │ │ ──────────►  │ │ SetInputMode    │ │               │
│  │ │ (UIOnly)        │ │              │ │ (GameOnly)      │ │ ◄── ВАЖНО!   │
│  │ └─────────────────┘ │              │ └─────────────────┘ │               │
│  │                     │              │                     │               │
│  │ InputMode: UIOnly   │              │ InputMode: GameOnly │               │
│  └─────────────────────┘              └─────────────────────┘               │
│                                                                             │
│  UI работает ✓                        Game input работает ✓                 │
│  Game input blocked ✓                 Q-pause работает ✓                    │
│                                       WASD работает ✓                       │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Почему `IsLocalController()`?

```cpp
if (IsLocalController())
{
    FInputModeGameOnly InputMode;
    SetInputMode(InputMode);
}
```

- В мультиплеере `BeginPlay` вызывается и на сервере, и на клиенте
- Input Mode должен устанавливаться **только** на локальном клиенте
- `IsLocalController()` проверяет что это локальный контроллер

### 4.3 Порядок инициализации

```
BeginPlay()
    │
    ├── 1. Super::BeginPlay()
    │
    ├── 2. SetInputMode(GameOnly)     ◄── ПЕРВЫМ делом!
    │
    ├── 3. SetupEnhancedInput()       ◄── После Input Mode
    │
    └── 4. CreatePauseMenu()          ◄── В конце
```

Важно устанавливать Input Mode **до** настройки Enhanced Input!

---

## 5. Проверка

### 5.1 Логирование

Добавьте логи для отладки:

```cpp
void ASuspenseCorePlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (IsLocalController())
    {
        UE_LOG(LogTemp, Warning, TEXT("=== SuspenseCorePC BeginPlay ==="));
        UE_LOG(LogTemp, Warning, TEXT("Setting InputMode to GameOnly"));

        FInputModeGameOnly InputMode;
        SetInputMode(InputMode);
        bShowMouseCursor = false;

        UE_LOG(LogTemp, Warning, TEXT("InputMode set successfully"));
    }
}
```

### 5.2 Тестирование

1. **Тест 1: Прямой запуск на GameMap**
   - PIE на GameMap
   - Нажать Q - должна открыться пауза
   - ✓ Работает? → Базовая настройка верна

2. **Тест 2: Переход из MainMenu**
   - PIE на MainMenuMap
   - Создать/выбрать персонажа
   - Нажать Play
   - На GameMap нажать Q
   - ✓ Работает? → Input Mode обрабатывается корректно

3. **Тест 3: Возврат и повторный вход**
   - Войти в игру
   - Exit to Lobby
   - Снова войти в игру
   - Проверить Q-паузу
   - ✓ Работает? → Полный цикл работает

### 5.3 Проверка Input Mode в Runtime

```cpp
// Console command для проверки
void ASuspenseCorePlayerController::DebugInputMode()
{
    // К сожалению, нет прямого способа получить текущий Input Mode
    // Но можно проверить через поведение:

    // Если bShowMouseCursor = true и игровой ввод не работает → UIOnly
    // Если bShowMouseCursor = false и игровой ввод работает → GameOnly

    UE_LOG(LogTemp, Warning, TEXT("ShowMouseCursor: %s"),
        bShowMouseCursor ? TEXT("true") : TEXT("false"));
}
```

---

## 6. Частые ошибки

### Ошибка 1: Забыли добавить SetInputMode

```cpp
// НЕПРАВИЛЬНО
void ASuspenseCorePlayerController::BeginPlay()
{
    Super::BeginPlay();
    SetupEnhancedInput();  // Input не будет работать после MainMenu!
}

// ПРАВИЛЬНО
void ASuspenseCorePlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (IsLocalController())
    {
        SetInputMode(FInputModeGameOnly());  // ОБЯЗАТЕЛЬНО!
        bShowMouseCursor = false;
    }

    SetupEnhancedInput();
}
```

### Ошибка 2: SetInputMode без IsLocalController

```cpp
// НЕПРАВИЛЬНО - может вызвать проблемы в мультиплеере
void ASuspenseCorePlayerController::BeginPlay()
{
    Super::BeginPlay();
    SetInputMode(FInputModeGameOnly());
}

// ПРАВИЛЬНО
void ASuspenseCorePlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (IsLocalController())  // Только для локального игрока!
    {
        SetInputMode(FInputModeGameOnly());
    }
}
```

### Ошибка 3: SetInputMode после SetupEnhancedInput

```cpp
// МОЖЕТ РАБОТАТЬ, НО ЛУЧШЕ ИЗБЕГАТЬ
void ASuspenseCorePlayerController::BeginPlay()
{
    Super::BeginPlay();
    SetupEnhancedInput();  // Сначала настраиваем input

    if (IsLocalController())
    {
        SetInputMode(FInputModeGameOnly());  // Потом режим
    }
}

// РЕКОМЕНДУЕТСЯ - сначала режим, потом настройка
void ASuspenseCorePlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (IsLocalController())
    {
        SetInputMode(FInputModeGameOnly());  // Сначала режим
    }

    SetupEnhancedInput();  // Потом настраиваем input
}
```

### Ошибка 4: Использование GetInputMode()

```cpp
// НЕПРАВИЛЬНО - GetInputMode() не существует!
void ASuspenseCorePlayerController::BeginPlay()
{
    auto Mode = GetInputMode();  // ОШИБКА КОМПИЛЯЦИИ!
}
```

`APlayerController` не имеет метода `GetInputMode()`. Input Mode можно только **устанавливать**, но не читать напрямую.

---

## Сводка

### Обязательный код

```cpp
void ASuspenseCorePlayerController::BeginPlay()
{
    Super::BeginPlay();

    // CRITICAL: Set Input Mode after map transition
    if (IsLocalController())
    {
        FInputModeGameOnly InputMode;
        SetInputMode(InputMode);
        bShowMouseCursor = false;
    }

    // ... rest of initialization
}
```

### Чеклист

- [ ] `SetInputMode(FInputModeGameOnly())` добавлен в `BeginPlay()`
- [ ] Проверка `IsLocalController()` перед `SetInputMode()`
- [ ] `bShowMouseCursor = false` установлен
- [ ] Протестирован переход MainMenu → GameMap
- [ ] Q-пауза работает после перехода
- [ ] WASD работает после перехода

---

*Документация создана: 2025-11-29*
*SuspenseCore Clean Architecture*

**Этот документ описывает критическую проблему. Если вы столкнулись с неработающим input после перехода между картами - проверьте этот код!**
