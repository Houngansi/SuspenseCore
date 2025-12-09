# ЭТАП 3: PlayerCore — Детальный план

> **Статус:** Не начат
> **Приоритет:** P0 (КРИТИЧЕСКИЙ)
> **Зависимости:** BridgeSystem (Этап 1), GAS (Этап 2)
> **Предыдущий этап:** [GAS](../GAS/README.md)

---

## 1. Обзор

PlayerCore — ядро игрока. Минимальный, чистый модуль.
Создаём с нуля следующие классы:

| Класс | Legacy референс | Описание |
|-------|-----------------|----------|
| `ASuspenseCoreCharacter` | `ASuspenseCharacter` | Базовый персонаж |
| `ASuspenseCorePlayerController` | `ASuspensePlayerController` | Контроллер игрока |
| `USuspenseCorePlayerState` | `USuspensePlayerState` | Состояние игрока |
| `ISuspenseCoreCharacterInterface` | — | Интерфейс персонажа |
| `USuspenseCoreMovementComponent` | — | Компонент движения (опционально) |

---

## 2. Архитектура PlayerCore модуля

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     PLAYERCORE MODULE ARCHITECTURE                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│                    ┌─────────────────────────────────┐                       │
│                    │  USuspenseCoreEventManager     │                       │
│                    │      (from BridgeSystem)       │                       │
│                    └───────────────┬─────────────────┘                       │
│                                    │                                         │
│                                    ▼                                         │
│    ┌───────────────────────────────────────────────────────────────────┐    │
│    │                   ASuspenseCoreCharacter                          │    │
│    │  ═══════════════════════════════════════════════════════════════  │    │
│    │  implements: ISuspenseCoreCharacterInterface                      │    │
│    │             ISuspenseCoreAbilityInterface                         │    │
│    │             ISuspenseCoreEventSubscriber                          │    │
│    │                                                                   │    │
│    │  ┌─────────────────────────────────────────────────────────────┐  │    │
│    │  │ Components:                                                 │  │    │
│    │  │ • USuspenseCoreAbilitySystemComponent                       │  │    │
│    │  │ • USkeletalMeshComponent                                    │  │    │
│    │  │ • UCameraComponent (optional)                               │  │    │
│    │  └─────────────────────────────────────────────────────────────┘  │    │
│    └───────────────────────────────┬───────────────────────────────────┘    │
│                                    │                                         │
│              ┌─────────────────────┴─────────────────────┐                  │
│              │                                           │                  │
│              ▼                                           ▼                  │
│   ┌──────────────────────────┐            ┌──────────────────────────┐     │
│   │ ASuspenseCore            │            │ USuspenseCore            │     │
│   │ PlayerController         │◄──────────►│ PlayerState              │     │
│   │ ════════════════════════ │            │ ════════════════════════ │     │
│   │ • Input handling         │            │ • Player data            │     │
│   │ • Camera management      │            │ • Team info              │     │
│   │ • UI coordination        │            │ • Score/Stats            │     │
│   └──────────────────────────┘            └──────────────────────────┘     │
│                                                                              │
│                         ▼                                                    │
│              ┌───────────────────┐                                          │
│              │   EventBus        │                                          │
│              │ (публикация       │                                          │
│              │  событий)         │                                          │
│              └───────────────────┘                                          │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Ключевые принципы

### 3.1 Минимальные зависимости

```
PlayerCore зависит ТОЛЬКО от:
├── Core, CoreUObject, Engine (UE базовые)
├── EnhancedInput (ввод)
├── BridgeSystem (события)
└── GAS (система способностей)

PlayerCore НЕ зависит от:
├── InventorySystem  ❌
├── EquipmentSystem  ❌
├── UISystem         ❌
└── InteractionSystem ❌
```

### 3.2 Коммуникация только через EventBus

```
ASuspenseCoreCharacter
    │
    ├── Публикует: SuspenseCore.Event.Player.Spawned
    ├── Публикует: SuspenseCore.Event.Player.Died
    ├── Публикует: SuspenseCore.Event.Player.StateChanged
    │
    └── Подписывается: SuspenseCore.Event.GAS.Attribute.Health
                       (для обработки смерти)
```

---

## 4. Классы к созданию

### 4.1 ASuspenseCoreCharacter

**Файл:** `PlayerCore/Public/Core/SuspenseCoreCharacter.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "BridgeSystem/Public/Core/SuspenseCoreInterfaces.h"
#include "GAS/Public/Core/Interfaces/SuspenseCoreAbilityInterface.h"
#include "SuspenseCoreCharacter.generated.h"

class USuspenseCoreAbilitySystemComponent;
class USuspenseCoreEventBus;
struct FSuspenseCoreEventData;
struct FSuspenseCoreSubscriptionHandle;

/**
 * ASuspenseCoreCharacter
 *
 * Базовый класс персонажа. Минимальный и чистый.
 *
 * Legacy референс: ASuspenseCharacter
 *
 * Что есть:
 * - Интеграция с GAS (AbilitySystemComponent)
 * - Публикация событий через EventBus
 * - Базовые атрибуты (Health, etc.)
 *
 * Чего НЕТ (в отличие от legacy):
 * - Прямых ссылок на Inventory, Equipment, UI
 * - Делегатов для внешних систем
 * - Тяжёлой логики в Tick
 */
UCLASS(Abstract)
class PLAYERCORE_API ASuspenseCoreCharacter : public ACharacter,
    public IAbilitySystemInterface,
    public ISuspenseCoreAbilityInterface,
    public ISuspenseCoreEventSubscriber
{
    GENERATED_BODY()

public:
    ASuspenseCoreCharacter(const FObjectInitializer& ObjectInitializer);

    // ═══════════════════════════════════════════════════════════════════════
    // AActor Interface
    // ═══════════════════════════════════════════════════════════════════════

    virtual void PostInitializeComponents() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void UnPossessed() override;

    // ═══════════════════════════════════════════════════════════════════════
    // IAbilitySystemInterface
    // ═══════════════════════════════════════════════════════════════════════

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    // ═══════════════════════════════════════════════════════════════════════
    // ISuspenseCoreAbilityInterface
    // ═══════════════════════════════════════════════════════════════════════

    virtual USuspenseCoreAbilitySystemComponent* GetSuspenseCoreAbilitySystemComponent() const override;
    virtual bool IsAlive() const override;
    virtual int32 GetAbilityLevel() const override;

    // ═══════════════════════════════════════════════════════════════════════
    // ISuspenseCoreEventSubscriber
    // ═══════════════════════════════════════════════════════════════════════

    virtual void SetupEventSubscriptions(USuspenseCoreEventBus* EventBus) override;
    virtual void TeardownEventSubscriptions(USuspenseCoreEventBus* EventBus) override;
    virtual TArray<FSuspenseCoreSubscriptionHandle> GetSubscriptionHandles() const override;

    // ═══════════════════════════════════════════════════════════════════════
    // CHARACTER STATE
    // ═══════════════════════════════════════════════════════════════════════

    /** Жив ли персонаж */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Character")
    bool IsDead() const { return bIsDead; }

    /** Получить текущее здоровье (from AttributeSet) */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Character")
    float GetHealth() const;

    /** Получить максимальное здоровье */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Character")
    float GetMaxHealth() const;

    /** Получить процент здоровья (0-1) */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Character")
    float GetHealthPercent() const;

    // ═══════════════════════════════════════════════════════════════════════
    // EVENTS
    // ═══════════════════════════════════════════════════════════════════════

    /** Вызывается при смерти персонажа */
    UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|Character")
    void OnDeath(AActor* Killer);

    /** Вызывается при возрождении */
    UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|Character")
    void OnRespawn();

protected:
    // ═══════════════════════════════════════════════════════════════════════
    // COMPONENTS
    // ═══════════════════════════════════════════════════════════════════════

    /** Ability System Component */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USuspenseCoreAbilitySystemComponent> AbilitySystemComponent;

    // ═══════════════════════════════════════════════════════════════════════
    // STATE
    // ═══════════════════════════════════════════════════════════════════════

    /** Флаг смерти */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_IsDead, Category = "State")
    bool bIsDead;

    /** Уровень персонажа (для GAS) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
    int32 CharacterLevel;

    // ═══════════════════════════════════════════════════════════════════════
    // EVENT SUBSCRIPTIONS
    // ═══════════════════════════════════════════════════════════════════════

    /** Handles подписок */
    TArray<FSuspenseCoreSubscriptionHandle> EventSubscriptionHandles;

    // ═══════════════════════════════════════════════════════════════════════
    // INTERNAL
    // ═══════════════════════════════════════════════════════════════════════

    /** Инициализация GAS */
    virtual void InitializeAbilitySystem();

    /** Публикация события spawned */
    virtual void PublishSpawnedEvent();

    /** Публикация события died */
    virtual void PublishDiedEvent(AActor* Killer);

    /** Callback на изменение здоровья */
    UFUNCTION()
    void OnHealthChanged(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

    /** Репликация смерти */
    UFUNCTION()
    virtual void OnRep_IsDead();

    /** Получить EventBus */
    USuspenseCoreEventBus* GetEventBus() const;

private:
    /** Кэш EventBus */
    TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;
};
```

### 4.2 ASuspenseCorePlayerController

**Файл:** `PlayerCore/Public/Core/SuspenseCorePlayerController.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BridgeSystem/Public/Core/SuspenseCoreInterfaces.h"
#include "SuspenseCorePlayerController.generated.h"

class USuspenseCoreEventBus;
struct FSuspenseCoreEventData;
struct FSuspenseCoreSubscriptionHandle;

/**
 * ASuspenseCorePlayerController
 *
 * Контроллер игрока. Управляет вводом и камерой.
 *
 * Legacy референс: ASuspensePlayerController
 *
 * Что есть:
 * - Enhanced Input интеграция
 * - Публикация событий ввода
 * - Управление камерой
 *
 * Чего НЕТ:
 * - Прямых ссылок на UI виджеты
 * - Управления инвентарём
 */
UCLASS()
class PLAYERCORE_API ASuspenseCorePlayerController : public APlayerController,
    public ISuspenseCoreEventSubscriber
{
    GENERATED_BODY()

public:
    ASuspenseCorePlayerController();

    // ═══════════════════════════════════════════════════════════════════════
    // APlayerController Interface
    // ═══════════════════════════════════════════════════════════════════════

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupInputComponent() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;

    // ═══════════════════════════════════════════════════════════════════════
    // ISuspenseCoreEventSubscriber
    // ═══════════════════════════════════════════════════════════════════════

    virtual void SetupEventSubscriptions(USuspenseCoreEventBus* EventBus) override;
    virtual void TeardownEventSubscriptions(USuspenseCoreEventBus* EventBus) override;
    virtual TArray<FSuspenseCoreSubscriptionHandle> GetSubscriptionHandles() const override;

    // ═══════════════════════════════════════════════════════════════════════
    // INPUT
    // ═══════════════════════════════════════════════════════════════════════

    /** Получить текущий Input Component (Enhanced) */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Input")
    UEnhancedInputComponent* GetEnhancedInputComponent() const;

    // ═══════════════════════════════════════════════════════════════════════
    // EVENTS
    // ═══════════════════════════════════════════════════════════════════════

    /** Публикует событие ввода */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
    void PublishInputEvent(FGameplayTag InputTag, float Value);

protected:
    // ═══════════════════════════════════════════════════════════════════════
    // INPUT MAPPINGS
    // ═══════════════════════════════════════════════════════════════════════

    /** Default Input Mapping Context */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<class UInputMappingContext> DefaultMappingContext;

    /** Priority для маппинга */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    int32 DefaultMappingPriority;

    // ═══════════════════════════════════════════════════════════════════════
    // EVENT SUBSCRIPTIONS
    // ═══════════════════════════════════════════════════════════════════════

    TArray<FSuspenseCoreSubscriptionHandle> EventSubscriptionHandles;

    // ═══════════════════════════════════════════════════════════════════════
    // INTERNAL
    // ═══════════════════════════════════════════════════════════════════════

    /** Настройка Enhanced Input */
    virtual void SetupEnhancedInput();

    /** Callback на смерть персонажа */
    UFUNCTION()
    void OnPlayerDied(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

    /** Получить EventBus */
    USuspenseCoreEventBus* GetEventBus() const;

private:
    TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;
};
```

### 4.3 USuspenseCorePlayerState

**Файл:** `PlayerCore/Public/Core/SuspenseCorePlayerState.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "BridgeSystem/Public/Core/SuspenseCoreInterfaces.h"
#include "SuspenseCorePlayerState.generated.h"

class USuspenseCoreEventBus;
struct FSuspenseCoreEventData;
struct FSuspenseCoreSubscriptionHandle;

/**
 * USuspenseCorePlayerState
 *
 * Состояние игрока. Хранит данные, реплицируемые всем.
 *
 * Legacy референс: USuspensePlayerState
 *
 * Что хранит:
 * - Имя игрока
 * - Команда
 * - Счёт/Статистика
 * - ID для матчинга событий
 */
UCLASS()
class PLAYERCORE_API ASuspenseCorePlayerState : public APlayerState,
    public ISuspenseCoreEventEmitter
{
    GENERATED_BODY()

public:
    ASuspenseCorePlayerState();

    // ═══════════════════════════════════════════════════════════════════════
    // APlayerState Interface
    // ═══════════════════════════════════════════════════════════════════════

    virtual void BeginPlay() override;
    virtual void CopyProperties(APlayerState* PlayerState) override;

    // ═══════════════════════════════════════════════════════════════════════
    // ISuspenseCoreEventEmitter
    // ═══════════════════════════════════════════════════════════════════════

    virtual void EmitEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& Data) override;
    virtual USuspenseCoreEventBus* GetEventBus() const override;

    // ═══════════════════════════════════════════════════════════════════════
    // PLAYER DATA
    // ═══════════════════════════════════════════════════════════════════════

    /** Получить ID игрока (уникальный для сессии) */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Player")
    int32 GetSuspenseCorePlayerId() const { return SuspenseCorePlayerId; }

    /** Получить команду */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Player")
    int32 GetTeamId() const { return TeamId; }

    /** Установить команду */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Player")
    void SetTeamId(int32 NewTeamId);

    // ═══════════════════════════════════════════════════════════════════════
    // STATISTICS
    // ═══════════════════════════════════════════════════════════════════════

    /** Получить убийства */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Stats")
    int32 GetKills() const { return Kills; }

    /** Получить смерти */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Stats")
    int32 GetDeaths() const { return Deaths; }

    /** Получить K/D ratio */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Stats")
    float GetKDRatio() const;

    /** Добавить убийство */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Stats")
    void AddKill();

    /** Добавить смерть */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Stats")
    void AddDeath();

protected:
    // ═══════════════════════════════════════════════════════════════════════
    // REPLICATED DATA
    // ═══════════════════════════════════════════════════════════════════════

    /** Уникальный ID игрока */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player")
    int32 SuspenseCorePlayerId;

    /** ID команды */
    UPROPERTY(ReplicatedUsing = OnRep_TeamId, BlueprintReadOnly, Category = "Team")
    int32 TeamId;

    /** Количество убийств */
    UPROPERTY(ReplicatedUsing = OnRep_Kills, BlueprintReadOnly, Category = "Stats")
    int32 Kills;

    /** Количество смертей */
    UPROPERTY(ReplicatedUsing = OnRep_Deaths, BlueprintReadOnly, Category = "Stats")
    int32 Deaths;

    // ═══════════════════════════════════════════════════════════════════════
    // REPLICATION CALLBACKS
    // ═══════════════════════════════════════════════════════════════════════

    UFUNCTION()
    void OnRep_TeamId();

    UFUNCTION()
    void OnRep_Kills();

    UFUNCTION()
    void OnRep_Deaths();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    /** Статический счётчик для генерации ID */
    static int32 NextPlayerId;
};
```

### 4.4 ISuspenseCoreCharacterInterface

**Файл:** `PlayerCore/Public/Core/Interfaces/SuspenseCoreCharacterInterface.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SuspenseCoreCharacterInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreCharacterInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * ISuspenseCoreCharacterInterface
 *
 * Интерфейс для персонажей.
 * Используется для безопасного приведения типов.
 */
class PLAYERCORE_API ISuspenseCoreCharacterInterface
{
    GENERATED_BODY()

public:
    // ═══════════════════════════════════════════════════════════════════════
    // STATE
    // ═══════════════════════════════════════════════════════════════════════

    /** Жив ли персонаж */
    virtual bool IsCharacterAlive() const = 0;

    /** Может ли быть целью */
    virtual bool CanBeTargeted() const { return IsCharacterAlive(); }

    /** Является ли AI */
    virtual bool IsAI() const { return false; }

    // ═══════════════════════════════════════════════════════════════════════
    // TEAM
    // ═══════════════════════════════════════════════════════════════════════

    /** Получить ID команды */
    virtual int32 GetCharacterTeamId() const = 0;

    /** Проверить враг ли (по отношению к Other) */
    virtual bool IsEnemyOf(const ISuspenseCoreCharacterInterface* Other) const
    {
        if (!Other) return true;
        return GetCharacterTeamId() != Other->GetCharacterTeamId();
    }

    /** Проверить союзник ли */
    virtual bool IsAllyOf(const ISuspenseCoreCharacterInterface* Other) const
    {
        return !IsEnemyOf(Other);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // IDENTIFICATION
    // ═══════════════════════════════════════════════════════════════════════

    /** Получить отображаемое имя */
    virtual FString GetCharacterDisplayName() const = 0;

    /** Получить локацию (для UI, аудио, etc.) */
    virtual FVector GetCharacterLocation() const = 0;
};
```

---

## 5. Публикуемые события

| Событие | Tag | Когда | Payload |
|---------|-----|-------|---------|
| Персонаж заспавнился | `SuspenseCore.Event.Player.Spawned` | BeginPlay | PlayerId, Location, TeamId |
| Персонаж умер | `SuspenseCore.Event.Player.Died` | OnDeath | PlayerId, Killer, DamageType |
| Персонаж возродился | `SuspenseCore.Event.Player.Respawned` | OnRespawn | PlayerId, Location |
| Состояние изменилось | `SuspenseCore.Event.Player.StateChanged` | SetTeamId, etc. | PlayerId, StateName, NewValue |
| Контроллер подключился | `SuspenseCore.Event.Player.Controller.Connected` | OnPossess | PlayerId, ControllerId |
| Kill зафиксирован | `SuspenseCore.Event.Player.Kill` | AddKill | KillerId, VictimId |

---

## 6. Зависимости модуля

### 6.1 PlayerCore.Build.cs (ЦЕЛЕВОЙ)

```csharp
PublicDependencyModuleNames.AddRange(
    new string[]
    {
        "Core",
        "CoreUObject",
        "Engine",
        "EnhancedInput",        // Enhanced Input System
        "BridgeSystem",         // EventBus, ServiceLocator
        "GAS"                   // AbilitySystemComponent
    }
);

PrivateDependencyModuleNames.AddRange(
    new string[]
    {
        "NetCore",
        "GameplayAbilities",
        "GameplayTags"
    }
);

// УБРАТЬ:
// - InventorySystem    ❌
// - EquipmentSystem    ❌
// - UISystem           ❌
// - CinematicCamera    ❌ (если не используется)
```

---

## 7. Чеклист выполнения

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    ЧЕКЛИСТ ЭТАПА 3: PlayerCore                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ ПОДГОТОВКА                                                                   │
│ ☐ Создать папку PlayerCore/Public/Core/                                     │
│ ☐ Создать папку PlayerCore/Public/Core/Interfaces/                          │
│ ☐ Создать папку PlayerCore/Private/Core/                                    │
│                                                                              │
│ CHARACTER                                                                    │
│ ☐ Создать SuspenseCoreCharacter.h                                           │
│ ☐ Создать SuspenseCoreCharacter.cpp                                         │
│   ☐ PostInitializeComponents (создание ASC)                                 │
│   ☐ BeginPlay (публикация Spawned)                                          │
│   ☐ EndPlay (отписка от событий)                                            │
│   ☐ PossessedBy/UnPossessed                                                 │
│   ☐ GetAbilitySystemComponent                                               │
│   ☐ IsAlive, GetHealth, GetMaxHealth                                        │
│   ☐ OnDeath, OnRespawn                                                      │
│   ☐ SetupEventSubscriptions                                                 │
│   ☐ TeardownEventSubscriptions                                              │
│   ☐ Репликация bIsDead                                                      │
│                                                                              │
│ PLAYER CONTROLLER                                                            │
│ ☐ Создать SuspenseCorePlayerController.h                                    │
│ ☐ Создать SuspenseCorePlayerController.cpp                                  │
│   ☐ BeginPlay/EndPlay                                                       │
│   ☐ SetupInputComponent                                                     │
│   ☐ OnPossess/OnUnPossess                                                   │
│   ☐ SetupEnhancedInput                                                      │
│   ☐ PublishInputEvent                                                       │
│   ☐ Event subscriptions                                                     │
│                                                                              │
│ PLAYER STATE                                                                 │
│ ☐ Создать SuspenseCorePlayerState.h                                         │
│ ☐ Создать SuspenseCorePlayerState.cpp                                       │
│   ☐ BeginPlay                                                               │
│   ☐ CopyProperties                                                          │
│   ☐ GetTeamId, SetTeamId                                                    │
│   ☐ GetKills, GetDeaths, GetKDRatio                                         │
│   ☐ AddKill, AddDeath                                                       │
│   ☐ EmitEvent                                                               │
│   ☐ Репликация всех свойств                                                 │
│                                                                              │
│ INTERFACES                                                                   │
│ ☐ Создать SuspenseCoreCharacterInterface.h                                  │
│   ☐ IsCharacterAlive                                                        │
│   ☐ CanBeTargeted                                                           │
│   ☐ GetCharacterTeamId                                                      │
│   ☐ IsEnemyOf, IsAllyOf                                                     │
│   ☐ GetCharacterDisplayName                                                 │
│   ☐ GetCharacterLocation                                                    │
│                                                                              │
│ ЗАВИСИМОСТИ                                                                  │
│ ☐ Обновить PlayerCore.Build.cs                                              │
│ ☐ Убрать InventorySystem                                                    │
│ ☐ Убрать EquipmentSystem                                                    │
│ ☐ Убрать UISystem                                                           │
│ ☐ Добавить BridgeSystem                                                     │
│ ☐ Добавить GAS                                                              │
│                                                                              │
│ ТЕСТИРОВАНИЕ                                                                 │
│ ☐ Unit тест: Character spawn публикует событие                              │
│ ☐ Unit тест: Character death публикует событие                              │
│ ☐ Unit тест: PlayerState репликация                                         │
│ ☐ Integration тест: Character + GAS + EventBus                              │
│ ☐ Multiplayer тест: Репликация работает                                     │
│                                                                              │
│ ФИНАЛИЗАЦИЯ                                                                  │
│ ☐ Компиляция без ошибок                                                     │
│ ☐ Компиляция без warnings                                                   │
│ ☐ Изолированная компиляция модуля                                           │
│ ☐ Code review пройден                                                       │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 8. Примеры использования

### 8.1 Спавн персонажа

```cpp
void ASuspenseCoreCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Настройка подписок
    if (auto* EventBus = GetEventBus())
    {
        SetupEventSubscriptions(EventBus);
    }

    // Публикация события спавна
    PublishSpawnedEvent();
}

void ASuspenseCoreCharacter::PublishSpawnedEvent()
{
    if (auto* EventBus = GetEventBus())
    {
        FSuspenseCoreEventData Data = FSuspenseCoreEventData::Create(this);
        Data.SetFloat(TEXT("PlayerId"), GetPlayerState<ASuspenseCorePlayerState>()->GetSuspenseCorePlayerId());
        Data.SetObject(TEXT("Location"), /* FVector не объект, используем StringPayload */);
        Data.SetString(TEXT("LocationX"), FString::SanitizeFloat(GetActorLocation().X));
        Data.SetString(TEXT("LocationY"), FString::SanitizeFloat(GetActorLocation().Y));
        Data.SetString(TEXT("LocationZ"), FString::SanitizeFloat(GetActorLocation().Z));

        EventBus->Publish(
            FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Player.Spawned")),
            Data
        );
    }
}
```

### 8.2 Обработка смерти

```cpp
void ASuspenseCoreCharacter::OnDeath_Implementation(AActor* Killer)
{
    bIsDead = true;

    // Публикуем событие смерти
    PublishDiedEvent(Killer);

    // Обновляем статистику
    if (auto* PS = GetPlayerState<ASuspenseCorePlayerState>())
    {
        PS->AddDeath();
    }

    // Если есть убийца — ему kill
    if (auto* KillerCharacter = Cast<ASuspenseCoreCharacter>(Killer))
    {
        if (auto* KillerPS = KillerCharacter->GetPlayerState<ASuspenseCorePlayerState>())
        {
            KillerPS->AddKill();
        }
    }
}
```

### 8.3 Подписка UI на события игрока

```cpp
// В UI виджете (из другого модуля!)
void UPlayerHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (auto* Manager = USuspenseCoreEventManager::Get(this))
    {
        // Подписываемся на события игрока
        Manager->GetEventBus()->Subscribe(
            FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Player.Died")),
            FSuspenseCoreEventCallback::CreateUObject(this, &UPlayerHUDWidget::OnPlayerDied)
        );
    }
}

void UPlayerHUDWidget::OnPlayerDied(FGameplayTag EventTag, const FSuspenseCoreEventData& Data)
{
    // Показываем UI смерти
    ShowDeathScreen();
}
```

---

## 9. CHECKPOINT: После этапа 3

После завершения этапов 1-3 (BridgeSystem, GAS, PlayerCore):

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              CHECKPOINT                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ ПРОВЕРКИ:                                                                    │
│ ☐ Все 3 модуля компилируются                                                │
│ ☐ Каждый модуль компилируется изолированно                                  │
│ ☐ Персонаж спавнится и публикует события                                    │
│ ☐ GAS атрибуты работают                                                     │
│ ☐ События доходят через EventBus                                           │
│ ☐ Репликация работает                                                       │
│ ☐ Нет зависимостей от InventorySystem, EquipmentSystem, UISystem            │
│                                                                              │
│ ТЕСТЫ:                                                                       │
│ ☐ Полный цикл: Spawn → Damage → Death → Respawn                             │
│ ☐ Multiplayer: 2+ игрока, репликация событий                                │
│ ☐ Performance: Нет деградации FPS                                           │
│                                                                              │
│ ДОКУМЕНТАЦИЯ:                                                                │
│ ☐ API документация обновлена                                                │
│ ☐ Примеры использования добавлены                                           │
│ ☐ CHANGELOG обновлён                                                        │
│                                                                              │
│ ПОСЛЕ CHECKPOINT → Начинаем Фазу 3 (InteractionSystem, InventorySystem, etc.)│
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

*Документ создан: 2025-11-29*
*Этап: 3 из 7*
*Зависимости: BridgeSystem, GAS*
