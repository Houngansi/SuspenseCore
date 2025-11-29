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

Создайте файл тегов `Config/DefaultGameplayTags.ini`:

```ini
[/Script/GameplayTags.GameplayTagsSettings]
+GameplayTagList=(Tag="Event.GAS.Health.Death",DevComment="")
+GameplayTagList=(Tag="Event.GAS.Health.Low",DevComment="")
+GameplayTagList=(Tag="Event.GAS.Health.Changed",DevComment="")
+GameplayTagList=(Tag="Event.GAS.Shield.Broken",DevComment="")
+GameplayTagList=(Tag="Event.GAS.Shield.Low",DevComment="")
+GameplayTagList=(Tag="Event.GAS.Shield.Changed",DevComment="")
+GameplayTagList=(Tag="Event.GAS.Attribute.Changed",DevComment="")
+GameplayTagList=(Tag="Event.Progression.LevelUp",DevComment="")
+GameplayTagList=(Tag="Event.Progression.XPGained",DevComment="")
+GameplayTagList=(Tag="Event.UI.Registration.Success",DevComment="")
+GameplayTagList=(Tag="Event.UI.Registration.Failed",DevComment="")
```

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

## 7. Тестирование

### 7.1 Быстрый тест регистрации

1. Откройте тестовую карту
2. Нажмите **Play**
3. Должна появиться форма регистрации
4. Введите имя (3-32 символа, буквы/цифры/_/-)
5. Нажмите "Create Character"
6. Проверьте `Saved/PlayerData/` на наличие JSON файла

### 7.2 Проверка EventManager

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

### 7.3 Проверка AttributeSets

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

## 8. Debugging

### 8.1 Console Commands

```
// Показать статистику EventBus
SuspenseCore.EventBus.Stats

// Список зарегистрированных сервисов
SuspenseCore.Services.List

// Атрибуты игрока
showdebug abilitysystem
```

### 8.2 Логирование

```cpp
// Включить подробные логи
UE_LOG(LogSuspenseCore, Log, TEXT("..."));

// В DefaultEngine.ini
[Core.Log]
LogSuspenseCore=Verbose
```

### 8.3 Типичные проблемы

| Проблема | Решение |
|----------|---------|
| Widget не связывается | Проверьте имена виджетов (точное совпадение с BindWidget) |
| EventManager = nullptr | Убедитесь, что BridgeSystem модуль подключён |
| AttributeSet не найден | Добавьте в AdditionalAttributeSets в PlayerState |
| Репликация не работает | Проверьте GetLifetimeReplicatedProps |

---

## 9. Следующие шаги

После успешной настройки:

1. **Создайте GameplayEffects** для изменения атрибутов
2. **Создайте GameplayAbilities** для способностей
3. **Настройте Input** для управления
4. **Создайте HUD** для отображения атрибутов в игре
5. **Тестируйте multiplayer** для проверки репликации

---

## 10. Структура файлов после настройки

```
Content/
├── Blueprints/
│   ├── Core/
│   │   ├── BP_SuspenseCoreGameMode.uasset
│   │   ├── BP_SuspenseCorePlayerState.uasset
│   │   ├── BP_SuspenseCorePlayerController.uasset
│   │   └── BP_SuspenseCoreCharacter.uasset
│   │
│   └── UI/
│       ├── WBP_SuspenseCoreRegistration.uasset
│       └── WBP_SuspenseCorePlayerInfo.uasset
│
└── Maps/
    └── TestMap.umap
```

---

*Документ создан: 2025-11-29*
*SuspenseCore Clean Architecture*
