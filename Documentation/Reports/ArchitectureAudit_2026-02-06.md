# SuspenseCore - Architecture Audit Report

**Date:** 2026-02-06
**Auditor:** Senior Tech Lead
**Engine:** Unreal Engine 5.7
**Project Type:** Extraction PvE MMO Shooter (Tarkov-style)
**Codebase:** 627+ C++ files, 9 active modules, 46 docs

---

## Executive Summary

| Category | Grade | Critical | High | Medium | Low |
|---|---|---|---|---|---|
| **Architecture (Service Locator / EventBus)** | A- | 0 | 2 | 3 | 2 |
| **GAS (Gameplay Ability System)** | A | 0 | 1 | 2 | 1 |
| **Networking / MMO** | B+ | 1 | 2 | 2 | 1 |
| **SSOT Compliance** | A | 0 | 0 | 2 | 1 |
| **SRP / OOP** | B+ | 0 | 3 | 4 | 2 |
| **Enemy System (FSM)** | B+ | 0 | 2 | 3 | 2 |
| **Equipment System** | A- | 0 | 1 | 2 | 3 |
| **Security** | A | 0 | 0 | 1 | 1 |
| **Code Quality / Style** | B+ | 0 | 1 | 3 | 3 |
| **2026 Pattern Relevance** | B | 1 | 2 | 2 | 0 |
| **Total** | **B+/A-** | **2** | **14** | **24** | **16** |

**Overall Verdict:** Проект демонстрирует высокий уровень архитектурной зрелости. Clean Architecture, EventBus, SSOT, DI через ServiceLocator --- всё реализовано корректно. Основные риски сосредоточены в области сетевого стека (необходимость миграции на Iris) и нарушениях SRP в нескольких god-object классах.

---

## Table of Contents

1. [CRITICAL: Iris Replication System Migration](#1-critical-iris-replication-system-migration)
2. [CRITICAL: DeathState Dangling Pointer](#2-critical-deathstate-dangling-pointer)
3. [HIGH: SRP Violations - God Objects](#3-high-srp-violations---god-objects)
4. [HIGH: FCriticalSection Overuse](#4-high-fcriticalsection-overuse)
5. [HIGH: EventBus Type Safety](#5-high-eventbus-type-safety)
6. [HIGH: EnemySystem - Missing EventBus Integration](#6-high-enemysystem---missing-eventbus-integration)
7. [HIGH: GAS ASC Simulated Proxy Optimization](#7-high-gas-asc-simulated-proxy-optimization)
8. [HIGH: EnemySystem - Perception Config Hardcoded](#8-high-enemysystem---perception-config-hardcoded)
9. [MEDIUM: Findings](#9-medium-findings)
10. [LOW: Findings](#10-low-findings)
11. [Pattern Relevance for 2026](#11-pattern-relevance-for-2026)
12. [Module Dependency Analysis](#12-module-dependency-analysis)
13. [GDD Compliance Check](#13-gdd-compliance-check)
14. [Summary of Recommendations](#14-summary-of-recommendations)
15. [Appendix: Sources](#15-appendix-sources)

---

## 1. CRITICAL: Iris Replication System Migration

**Severity:** CRITICAL
**Files:** `Source/BridgeSystem/Public/SuspenseCore/Replication/SuspenseCoreReplicationGraph.h`
**Category:** Networking / 2026 Relevance

### Problem

Проект использует custom `UReplicationGraph` с 4 нодами (AlwaysRelevant, PlayerStateFrequency, OwnerOnly, Dormancy). В UE 5.7 (текущая версия движка на 2026 год) Epic представил **Iris** --- push-based replication system, разработанную на базе Fortnite Battle Royale.

**Iris и ReplicationGraph являются взаимоисключающими системами.** Network driver может использовать только одну из них. Iris заменяет концепцию нод ReplicationGraph на `Network Object Filters` и `Prioritizers`.

Бенчмарки сообщества на UE 5.7 Preview показывают:
- **+31% FPS** на сервере при 100 игроках
- **-24% frame time** по сравнению с legacy replication
- Push-based модель --- система не polling'ит объекты, а получает уведомления при изменении реплицируемых свойств

### Current Implementation Quality

Текущая реализация ReplicationGraph **качественная**:
- Frequency buckets для PlayerState (60/30/20/12 Hz по дистанции) --- корректно
- OwnerOnly ноды для инвентаря (99% сокращение bandwidth) --- корректно
- Dormancy для Equipment (80% экономия) --- корректно
- Spatial Grid 2D --- корректно

Однако всё это потребует переписывания при миграции на Iris.

### Recommendation

```
PHASE 1 (Сейчас): Подготовка
├── Изучить документацию Iris (dev.epicgames.com)
├── Создать Iris Proof-of-Concept branch
├── Маппинг текущих нод → Iris Filters/Prioritizers:
│   ├── AlwaysRelevant Node    → Iris AlwaysRelevant Filter (built-in)
│   ├── PlayerStateFrequency   → Iris Custom Prioritizer (distance-based)
│   ├── OwnerOnly Node         → Iris Owner-Only Filter (built-in)
│   └── Dormancy Node          → Iris Net Object Filter + Dormancy API
└── Оценить push-model replication для всех replicated properties

PHASE 2 (Следующий спринт): Миграция
├── Включить push-model для всех UPROPERTY(Replicated)
├── Реализовать Iris Filters
├── Реализовать Iris Prioritizers
├── A/B тестирование с legacy ReplicationGraph
└── Замерить bandwidth и server FPS

PHASE 3: Удаление legacy кода
├── Удалить SuspenseCoreReplicationGraph и все RepNodes
├── Обновить SuspenseCoreReplicationGraphSettings → Iris Settings
└── Обновить документацию
```

**Текущий статус Iris:** Experimental в UE 5.7. Для production рекомендуется дождаться стабилизации, но **начинать подготовку необходимо уже сейчас**, т.к. ReplicationGraph будет deprecated.

---

## 2. CRITICAL: DeathState Dangling Pointer

**Severity:** CRITICAL
**File:** `Source/EnemySystem/Private/SuspenseCore/FSM/States/SuspenseCoreEnemyDeathState.cpp:82-92`
**Category:** Memory Safety

### Problem

```cpp
// CURRENT CODE (DANGEROUS):
void USuspenseCoreEnemyDeathState::ScheduleDespawn(ASuspenseCoreEnemyCharacter* Enemy)
{
    FTimerHandle DespawnTimerHandle;
    Enemy->GetWorldTimerManager().SetTimer(
        DespawnTimerHandle,
        [Enemy]()  // <-- RAW POINTER CAPTURE!
        {
            if (Enemy && IsValid(Enemy))
            {
                Enemy->Destroy();
            }
        },
        DespawnDelay,
        false
    );
}
```

Lambda захватывает **raw pointer** `Enemy`. Если объект будет уничтожен GC или другой системой до срабатывания таймера, `Enemy` станет dangling pointer. `IsValid()` не спасёт, т.к. адрес может быть переиспользован другим объектом.

### Fix

```cpp
// CORRECTED:
void USuspenseCoreEnemyDeathState::ScheduleDespawn(ASuspenseCoreEnemyCharacter* Enemy)
{
    if (!Enemy || DespawnDelay <= 0.0f) return;

    TWeakObjectPtr<ASuspenseCoreEnemyCharacter> WeakEnemy(Enemy);
    FTimerHandle DespawnTimerHandle;
    Enemy->GetWorldTimerManager().SetTimer(
        DespawnTimerHandle,
        [WeakEnemy]()
        {
            if (ASuspenseCoreEnemyCharacter* StrongEnemy = WeakEnemy.Get())
            {
                StrongEnemy->Destroy();
            }
        },
        DespawnDelay,
        false
    );
}
```

---

## 3. HIGH: SRP Violations - God Objects

**Severity:** HIGH
**Category:** SRP / OOP

### 3.1 SuspenseCoreBaseFireAbility (882 + 1531 lines)

**File:** `Source/GAS/Public/SuspenseCore/Abilities/Base/SuspenseCoreBaseFireAbility.h`

Один класс отвечает за:
- Расчёт отдачи (recoil calculation)
- Расчёт разброса (spread calculation)
- Tarkov-style паттерны отдачи (recoil patterns)
- Визуальное и прицельное разделение (visual/aim separation)
- Трассировку (ray/line trace)
- Применение урона (damage application)
- Звуковые эффекты (audio FX)
- Визуальные эффекты (Niagara + Cascade VFX)
- Анимации (montage playback)
- Сетевая валидация (server validation)
- Модификаторы обвесов (attachment modifiers)

**Это минимум 5-6 отдельных ответственностей.**

#### Recommendation

```
SuspenseCoreBaseFireAbility (оркестратор, ~200 lines)
├── USuspenseCoreRecoilCalculator      (отдача + паттерны)
├── USuspenseCoreSpreadCalculator      (разброс) [уже существует частично]
├── USuspenseCoreWeaponTraceService    (трассировка + урон)
├── USuspenseCoreWeaponFXService       (звук + VFX + анимация)
└── USuspenseCoreWeaponNetValidator    (сетевая валидация)
```

### 3.2 SuspenseCoreDataManager (1058 + 2351 lines)

**File:** `Source/BridgeSystem/Public/SuspenseCore/Data/SuspenseCoreDataManager.h`

Менеджер данных обрабатывает ВСЕ типы данных:
- Оружие (weapons)
- Патроны (ammo)
- Броня (armor)
- Расходники (consumables)
- Статус-эффекты (status effects)
- Магазины (magazines)
- Загрузочные конфигурации (loadouts)

#### Recommendation

Сохранить DataManager как фасад, но вынести доменную логику в специализированные Data Providers:

```
USuspenseCoreDataManager (фасад, ~300 lines)
├── USuspenseCoreWeaponDataProvider
├── USuspenseCoreAmmoDataProvider
├── USuspenseCoreArmorDataProvider
├── USuspenseCoreConsumableDataProvider
├── USuspenseCoreStatusEffectDataProvider
└── USuspenseCoreMagazineDataProvider
```

Каждый провайдер регистрируется через ServiceLocator и реализует интерфейс `ISuspenseCoreDataProvider`.

### 3.3 SuspenseCoreEquipmentOperationService (495 + 36969+ tokens)

**File:** `Source/EquipmentSystem/Public/SuspenseCore/Services/SuspenseCoreEquipmentOperationService.h`

Файл реализации **превышает 36,000 tokens** --- это ~2000+ строк. Для одного сервиса это слишком много.

#### Recommendation

Разделить на:
```
SuspenseCoreEquipmentOperationService (координатор, ~500 lines)
├── SuspenseCoreEquipmentQueueProcessor   (очередь операций)
├── SuspenseCoreEquipmentHistoryService   (undo/redo)
├── SuspenseCoreEquipmentCacheService     (кэш результатов и валидации)
└── SuspenseCoreEquipmentBatchExecutor    (пакетные операции)
```

---

## 4. HIGH: FCriticalSection Overuse

**Severity:** HIGH
**Category:** Performance / Threading

### Problem

Проект использует `FCriticalSection` (exclusive lock) **везде**, включая read-heavy структуры данных:

| File | Data Structure | Read:Write Ratio |
|---|---|---|
| `SuspenseCoreEventBus.h` | `TMap<FGameplayTag, TArray<FSuspenseCoreSubscription>> Subscriptions` | ~100:1 |
| `SuspenseCoreServiceLocator.h` | `TMap<FName, TObjectPtr<UObject>> Services` | ~1000:1 |
| `SuspenseCoreDataManager.h` | Все кэши данных | ~1000:1 |
| `SuspenseCoreEquipmentDataStore.h` | Equipment state data | ~50:1 |

`FCriticalSection` блокирует **всех** читателей, даже когда данные не модифицируются. Для read-heavy workloads это приводит к unnecessary contention.

### Recommendation

Заменить на `FRWLock` для read-heavy структур:

```cpp
// BEFORE (FCriticalSection):
mutable FCriticalSection SubscriptionLock;

void Publish(...)
{
    TArray<FSuspenseCoreSubscription> SubsCopy;
    {
        FScopeLock Lock(&SubscriptionLock);     // BLOCKS all readers
        SubsCopy = Subscriptions.FindRef(Tag);
    }
    NotifySubscribers(SubsCopy, ...);
}

// AFTER (FRWLock):
mutable FRWLock SubscriptionLock;

void Publish(...)
{
    TArray<FSuspenseCoreSubscription> SubsCopy;
    {
        FRWScopeLock Lock(SubscriptionLock, SLT_ReadOnly);  // CONCURRENT reads
        SubsCopy = Subscriptions.FindRef(Tag);
    }
    NotifySubscribers(SubsCopy, ...);
}

FSuspenseCoreSubscriptionHandle Subscribe(...)
{
    FRWScopeLock Lock(SubscriptionLock, SLT_Write);  // Exclusive for writes
    // ...
}
```

**Приоритет замены:**
1. `EventBus` --- наибольший трафик чтения
2. `ServiceLocator` --- read-only после инициализации
3. `DataManager` кэши --- практически immutable после загрузки

---

## 5. HIGH: EventBus Type Safety

**Severity:** HIGH
**Category:** Architecture / Type Safety

### Problem

`FSuspenseCoreEventData` использует string-keyed maps для передачи данных:

```cpp
// CURRENT USAGE:
FSuspenseCoreEventData Data = FSuspenseCoreEventData::Create(this);
Data.SetFloat(TEXT("Damage"), 50.0f);        // string key - typo = silent bug
Data.SetFloat(TEXT("CurrentHealth"), HP);     // no compile-time validation
EventBus->Publish(DamageTag, Data);

// SUBSCRIBER:
float Damage = EventData.GetFloat(TEXT("Damge"));  // TYPO - returns 0.0, no error
```

Это антипаттерн в проекте с 700+ GameplayTags и десятками типов событий. Любая опечатка в ключе создаёт **silent bug**, который сложно отловить.

### Recommendation

Ввести типизированные payload structs для каждой группы событий:

```cpp
// RECOMMENDED:
USTRUCT(BlueprintType)
struct FSuspenseCoreDamageEventPayload
{
    GENERATED_BODY()

    UPROPERTY()
    float Damage = 0.0f;

    UPROPERTY()
    float CurrentHealth = 0.0f;

    UPROPERTY()
    TWeakObjectPtr<AActor> DamageSource;

    UPROPERTY()
    FGameplayTag DamageType;
};

// Usage (compile-time safety):
FSuspenseCoreDamageEventPayload Payload;
Payload.Damage = 50.0f;
Payload.CurrentHealth = HP;
EventBus->PublishTyped(DamageTag, Payload);
```

**Примечание:** `SuspenseCoreUIEvents.h` уже частично делает это (определяет helper-структуры), но EventBus API до сих пор принимает generic `FSuspenseCoreEventData`. Нужен единый подход.

---

## 6. HIGH: EnemySystem - Missing EventBus Integration

**Severity:** HIGH
**Category:** Architecture Consistency

### Problem

**Весь проект** построен на EventBus для межмодульной коммуникации. Однако EnemySystem **не использует EventBus** ни для одного из своих событий:

| Event | Current Mechanism | Expected (per architecture) |
|---|---|---|
| State changed | `OnStateChanged` UE delegate | EventBus publish |
| Player detected | Direct `FSMComponent->SendFSMEvent()` | EventBus publish |
| Enemy died | Direct state change | EventBus publish (для UI, progression, analytics) |
| Attack executed | Direct `Enemy->ExecuteAttack()` | EventBus publish (для audio, VFX, damage) |

FSM states напрямую вызывают методы `FSMComponent` и `Enemy`, что создаёт **tight coupling** и нарушает принцип Clean Architecture проекта.

### Recommendation

```cpp
// В OnEnterState Death:
SUSPENSE_PUBLISH_EVENT(SuspenseCoreEnemyTags::Event::Died, Enemy);

// В OnPerceptionUpdated:
FSuspenseCoreEventData Data = FSuspenseCoreEventData::Create(this);
Data.SetObject(TEXT("DetectedActor"), Actor);
Data.SetBool(TEXT("IsSensed"), bIsSensed);
SUSPENSE_PUBLISH_EVENT(SuspenseCoreEnemyTags::Event::PerceptionUpdated, Data);

// В Attack:
FSuspenseCoreEventData AttackData = FSuspenseCoreEventData::Create(this);
AttackData.SetObject(TEXT("Target"), CurrentTarget.Get());
SUSPENSE_PUBLISH_EVENT(SuspenseCoreEnemyTags::Event::AttackExecuted, AttackData);
```

Это позволит:
- UI подписаться на события врагов (kill feed, damage numbers)
- Progression системе считать убийства
- Audio/VFX реагировать на события без прямой зависимости
- Analytics логировать AI поведение

---

## 7. HIGH: GAS ASC Simulated Proxy Optimization

**Severity:** HIGH
**Category:** Networking / Performance

### Problem

Текущая реализация реплицирует полный `UAbilitySystemComponent` для **всех** proxies. В Fortnite (откуда GAS берёт начало), ASC **не реплицируется для simulated proxies**. Вместо этого, минимальный набор данных передаётся через структуру на Pawn.

При 100+ игроках в рейде, репликация полного ASC для каждого simulated proxy --- это значительная нагрузка на bandwidth.

**File:** `Source/GAS/Public/SuspenseCore/Components/SuspenseCoreAbilitySystemComponent.h`

### Recommendation

Реализовать `ReplicateSubobjects` override, который пропускает ASC для simulated proxies:

```cpp
// В Character или PlayerState:
bool ASuspenseCoreCharacter::ReplicateSubobjects(
    UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
    bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

    // Skip ASC replication for simulated proxies (Fortnite pattern)
    if (RepFlags->bIsSimulatedProxy)
    {
        // Replicate only minimal struct instead
        // Health, active visual effects, weapon state
        return bWroteSomething;
    }

    return bWroteSomething;
}
```

**Ожидаемый эффект:** ~60-80% сокращение bandwidth для ASC данных.

---

## 8. HIGH: EnemySystem - Perception Config Hardcoded

**Severity:** HIGH
**Category:** SRP / Data-Driven Design

### Problem

Perception параметры жёстко зашиты в конструкторе `SuspenseCoreEnemyAIController.cpp:15-30`:

```cpp
SightConfig->SightRadius = 2000.0f;           // hardcoded
SightConfig->LoseSightRadius = 2500.0f;        // hardcoded
SightConfig->PeripheralVisionAngleDegrees = 90.0f; // hardcoded
HearingConfig->HearingRange = 1500.0f;         // hardcoded
```

Но `SuspenseCoreEnemyBehaviorData` уже имеет поля `SightRange` и `HearingRange`. Они **не используются** для настройки AIPerception --- вместо этого BehaviorData значения применяются только для inline-атрибутов.

### Recommendation

Применять BehaviorData значения к AIPerception при `OnPossess`:

```cpp
void ASuspenseCoreEnemyAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    if (auto* Enemy = Cast<ASuspenseCoreEnemyCharacter>(InPawn))
    {
        if (auto* BehaviorData = Enemy->GetBehaviorData())
        {
            ConfigurePerception(BehaviorData);
        }
    }
}

void ASuspenseCoreEnemyAIController::ConfigurePerception(
    USuspenseCoreEnemyBehaviorData* BehaviorData)
{
    if (auto* SightConfig = GetSightConfig())
    {
        SightConfig->SightRadius = BehaviorData->GetEffectiveSightRange();
        SightConfig->LoseSightRadius = SightConfig->SightRadius * 1.25f;
    }
    if (auto* HearingConfig = GetHearingConfig())
    {
        HearingConfig->HearingRange = BehaviorData->GetEffectiveHearingRange();
    }
    AIPerceptionComponent->RequestStimuliListenerUpdate();
}
```

---

## 9. MEDIUM Findings

### 9.1 EventBus as UObject (Not Subsystem)

**File:** `Source/BridgeSystem/Public/SuspenseCore/Events/SuspenseCoreEventBus.h:80`

EventBus extends `UObject`, а не `UGameInstanceSubsystem`. Его lifecycle управляется вручную через `EventManager`, который сам является subsystem. Это дополнительный уровень индирекции без необходимости.

**Recommendation:** Рассмотреть слияние EventBus в EventManager (subsystem), или оставить как есть если требуется несколько инстансов EventBus (маловероятно).

### 9.2 IdleState Frame-Rate Dependent Rotation

**File:** `Source/EnemySystem/Private/SuspenseCore/FSM/States/SuspenseCoreEnemyIdleState.cpp:53-61`

```cpp
// PROBLEM: RInterpTo с DeltaTime внутри условия по TimeSinceLastLook
// Вызывается однократно при достижении LookAroundInterval,
// но DeltaTime в этом кадре может быть разным
Enemy->SetActorRotation(FMath::RInterpTo(
    Enemy->GetActorRotation(),
    NewRotation,
    DeltaTime,     // <-- problematic
    2.0f
));
```

Вызов `SetActorRotation` с `RInterpTo` происходит только в одном кадре (при сбросе таймера), а не каждый кадр. Это создаёт **резкий рывок** вместо плавного поворота.

**Fix:** Хранить `TargetLookRotation` и интерполировать каждый tick:

```cpp
void OnTickState(ASuspenseCoreEnemyCharacter* Enemy, float DeltaTime)
{
    TimeSinceLastLook += DeltaTime;
    if (TimeSinceLastLook >= LookAroundInterval)
    {
        TimeSinceLastLook = 0.0f;
        float RandomYaw = FMath::RandRange(-60.0f, 60.0f);
        TargetLookRotation = OriginalRotation;
        TargetLookRotation.Yaw += RandomYaw;
    }

    // Smooth interpolation EVERY frame
    FRotator Current = Enemy->GetActorRotation();
    FRotator Smoothed = FMath::RInterpTo(Current, TargetLookRotation, DeltaTime, 2.0f);
    Enemy->SetActorRotation(Smoothed);
}
```

### 9.3 PatrolState - OnMoveCompleted Not Cleaned On Destruction

**File:** `Source/EnemySystem/Private/SuspenseCore/FSM/States/SuspenseCoreEnemyPatrolState.cpp`

`ReceiveMoveCompleted.AddDynamic(this, &OnMoveCompleted)` привязан к UObject (`USuspenseCoreEnemyPatrolState`), но delegate unbind происходит только в `OnExitState`. Если State уничтожается без вызова OnExitState (например, при unload), delegate может стрелять в невалидный объект.

**Fix:** Добавить `BeginDestroy()` override с cleanup:

```cpp
void USuspenseCoreEnemyPatrolState::BeginDestroy()
{
    if (CachedController.IsValid())
    {
        CachedController->ReceiveMoveCompleted.RemoveDynamic(
            this, &USuspenseCoreEnemyPatrolState::OnMoveCompleted);
    }
    Super::BeginDestroy();
}
```

### 9.4 ChaseState - MoveToLocation Excessive Logging

**File:** `Source/EnemySystem/Private/SuspenseCore/FSM/States/SuspenseCoreEnemyChaseState.cpp:130-155`

`MoveToTarget()` логирует **каждый** вызов (каждые 0.5 секунды) с `UE_LOG(Log)`. При 50 врагах = 100 log messages/second. Это:
- Снижает performance (string formatting на каждый вызов)
- Спамит в Output Log

**Fix:** Использовать `UE_LOG(Verbose)` или `#if !UE_BUILD_SHIPPING`:

```cpp
UE_LOG(LogEnemySystem, Verbose, TEXT("[%s] ChaseState: MoveToTarget - ..."), ...);
```

### 9.5 SuspenseCoreCharacter - ADS Camera Approach

**File:** `Source/PlayerCore/Public/SuspenseCore/Characters/SuspenseCoreCharacter.h`

Персонаж использует `UCineCameraComponent` для ADS. Это нестандартный подход --- обычно ADS реализуется через Timeline lerp FOV основной камеры + сокет offset.

**Recommendation:** Это не ошибка, но стоит задокументировать причину выбора CineCamera (вероятно, для кинематографических эффектов / DoF / scope rendering). Убедиться, что нет performance overhead от CineCamera в gameplay.

### 9.6 DataManager - Synchronous Loading

**File:** `Source/BridgeSystem/Private/SuspenseCore/Data/SuspenseCoreDataManager.cpp`

DataManager использует `LoadSynchronous()` для DataTables. При большом количестве таблиц это может вызвать stutters при инициализации.

**Recommendation:** Использовать `FStreamableManager::RequestAsyncLoad` с callback:

```cpp
FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
Streamable.RequestAsyncLoad(
    SoftObjectPaths,
    FStreamableDelegate::CreateUObject(this, &USuspenseCoreDataManager::OnDataTablesLoaded)
);
```

### 9.7 EquipmentActor - Tag Caching in BeginPlay

**File:** `Source/EquipmentSystem/Private/SuspenseCore/Base/SuspenseCoreEquipmentActor.cpp`

Lazy tag caching --- хороший паттерн, но tag lookup при каждом вызове IsWeapon/GetArchetype добавляет overhead. Для hot path (каждый tick при firing), лучше кэшировать при InitializeFromItemInstance.

### 9.8 SecurityValidator - Missing Rate Limiting Per Operation Type

**File:** `Source/BridgeSystem/Public/SuspenseCore/Security/SuspenseCoreSecurityValidator.h`

Rate limiting существует, но не дифференцирован по типу операции. Equip и fire имеют разные нормальные частоты вызовов.

**Recommendation:** Добавить per-operation-type rate limits:

```cpp
TMap<FGameplayTag, FSuspenseCoreRateLimitConfig> OperationRateLimits;
// Fire: max 20/sec, Equip: max 5/sec, Move: max 10/sec
```

### 9.9 EnemySystem - ExecuteAttack() Empty Implementation

**File:** `Source/EnemySystem/Private/SuspenseCore/Characters/SuspenseCoreEnemyCharacter.cpp:160-165`

```cpp
void ASuspenseCoreEnemyCharacter::ExecuteAttack()
{
    UE_LOG(LogEnemySystem, Verbose, TEXT("[%s] Executing attack..."), ...);
    // NO ACTUAL IMPLEMENTATION
}
```

Метод только логирует. Отсутствует:
- GAS Ability activation
- Damage application
- Animation montage trigger

**Recommendation:** Реализовать через GAS:

```cpp
void ASuspenseCoreEnemyCharacter::ExecuteAttack()
{
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        ASC->TryActivateAbilityByClass(AttackAbilityClass);
    }
}
```

### 9.10 BridgeSystem - Niagara/UMG Dependencies

**File:** `Source/BridgeSystem/BridgeSystem.Build.cs`

BridgeSystem (core foundation) зависит от `Niagara` и `UMG`. Это нарушает принцип минимальных зависимостей для foundation модуля.

**Recommendation:** Вынести VFX и UI зависимости из BridgeSystem. Niagara нужен только для SpreadProcessor/TraceUtils --- вынести в GAS или отдельный утилитарный модуль.

### 9.11 SuspenseCoreAbilitySystemComponent - Mixed Owner/Avatar Issue

**File:** `Source/GAS/Public/SuspenseCore/Components/SuspenseCoreAbilitySystemComponent.h`

ASC инициализируется с `OwnerActor = PlayerState, AvatarActor = Character`. Это корректный паттерн (как в Fortnite/Lyra), но важно убедиться, что все проверки ролей используют Avatar role, а не Owner role.

**Potential issue:** `GetOwnerRole()` для PlayerState возвращает `ROLE_Authority` на сервере, даже для simulated proxies. Некоторые GAS функции используют Owner role вместо Avatar role.

**Recommendation:** Аудит всех мест где проверяется `HasAuthority()` / `GetOwnerRole()` в ability code.

---

## 10. LOW Findings

### 10.1 Mixed Language Comments

Russian и English комментарии смешаны по всей кодовой базе. Для международной команды это снижает maintainability.

**Recommendation:** Стандартизировать на English для кода, Russian допустим для GDD документов.

### 10.2 Copyright Year 2025

Многие файлы содержат `Copyright (c) 2025. All Rights Reserved.` Обновить до 2025-2026.

### 10.3 SuspenseCore Module - Minimal Implementation

**File:** `Source/SuspenseCore/SuspenseCore.Build.cs`

Модуль `SuspenseCore` --- практически пустой re-export модуль. Рассмотреть удаление, если re-export не требуется внешним потребителям.

### 10.4 DisabledModules - Legacy Code

3 отключённых модуля (`LegasyEnemyCoreSystem`, `LegasyWeaponGAS`, старый UISystem`) остаются в репозитории. Рекомендуется удалить полностью после подтверждения полной миграции.

**Note:** Typo в названии: `Legasy` → `Legacy`.

### 10.5 EquipmentOperationExecutor - Deprecated Methods

**File:** `Source/EquipmentSystem/Private/SuspenseCore/Components/Core/SuspenseCoreEquipmentOperationExecutor.cpp`

Содержит deprecated methods с комментариями "moved to service layer". Удалить мёртвый код.

### 10.6 Plugin Status - Beta

`SuspenseCore.uplugin` содержит `IsBetaVersion: true`. Если плагин используется в production, обновить статус.

---

## 11. Pattern Relevance for 2026

### Service Locator Pattern

| Aspect | Status | Notes |
|---|---|---|
| UE5 Subsystem alignment | **OK** | ServiceProvider как GameInstanceSubsystem --- корректно |
| Thread safety | **NEEDS UPGRADE** | FCriticalSection → FRWLock (см. #4) |
| Lifecycle management | **OK** | Equipment ServiceLocator с topological sort --- продвинуто |
| Alternative: DI Container | **CONSIDER** | UE5 не имеет встроенного DI. Service Locator --- стандарт в UE |

**Verdict:** Service Locator остаётся **актуальным** паттерном для UE5 в 2026. Нет предпосылок для замены.

### EventBus Pattern

| Aspect | Status | Notes |
|---|---|---|
| GameplayTag routing | **EXCELLENT** | Иерархические теги, child subscription --- best-in-class |
| Priority system | **OK** | 5 уровней --- достаточно |
| Thread safety | **NEEDS UPGRADE** | Copy-then-notify корректно, но locks --- см. #4 |
| Type safety | **NEEDS UPGRADE** | String-keyed payload --- см. #5 |
| Deferred events | **OK** | End-of-frame processing --- корректно |
| Async publish | **OK** | Game thread dispatch --- корректно |
| Alternative: UE5 Message Bus | **NO** | UE FMessageEndpoint --- для inter-process, не для in-game |

**Verdict:** EventBus остаётся **актуальным**. Паттерн широко используется в UE5 проектах. Рекомендации: type safety и RWLock.

### GAS (Gameplay Ability System)

| Aspect | Status | Notes |
|---|---|---|
| ASC on PlayerState | **OK** | Fortnite/Lyra pattern |
| Attribute Sets | **OK** | 7 sets, well-separated |
| Effects system | **OK** | 20+ effects, proper lifecycle |
| Ability Tasks | **OK** | Custom tasks for async operations |
| Networking | **NEEDS UPGRADE** | Simulated proxy optimization --- см. #7 |
| Prediction | **PARTIAL** | Equipment prediction есть, ability prediction --- стандартная GAS |
| Native Tags | **EXCELLENT** | UE_DECLARE_GAMEPLAY_TAG_EXTERN --- best practice |

**Verdict:** GAS usage **актуален**. GAS остаётся стандартом для AAA ability систем в UE5.7.

### Replication Architecture

| Aspect | Status | Notes |
|---|---|---|
| ReplicationGraph | **WILL BE DEPRECATED** | Iris заменяет ReplicationGraph в UE 5.7+ |
| Frequency buckets | **OK, NEEDS MIGRATION** | Нужно переводить на Iris Prioritizers |
| Push-model replication | **NOT IMPLEMENTED** | Iris требует push-model для оптимальной работы |
| Client-side prediction | **OK** | Equipment prediction system --- quality implementation |

**Verdict:** **Необходима миграция на Iris.** См. раздел #1.

### FSM для Enemy AI

| Aspect | Status | Notes |
|---|---|---|
| Component-based FSM | **OK** | Стандартный подход для UE5 |
| Data-driven transitions | **OK** | BehaviorData DataAsset --- хорошо |
| Alternative: Behavior Trees | **CONSIDER** | UE5 BT + EQS + StateTree мощнее для сложного AI |
| Alternative: StateTree | **RECOMMEND** | UE 5.7 StateTree --- рекомендуемая Epic альтернатива |

**Verdict:** Текущий FSM корректен для базового AI. Для development Scav/PMC/Boss AI с тактическим поведением (укрытия, фланки, координация отрядов) рекомендуется миграция на **StateTree** или **Behavior Tree + EQS**.

### SSOT Pattern

| Aspect | Status | Notes |
|---|---|---|
| DataManager as SSOT | **OK** | Единая точка для item data |
| DataTable-driven | **OK** | Стандарт UE5 |
| EventBus notifications | **OK** | Broadcast при изменении данных |

**Verdict:** SSOT **актуален и корректно реализован**.

---

## 12. Module Dependency Analysis

### Dependency Graph

```
BridgeSystem [PreDefault]
├── GAS [PreDefault]
│   ├── PlayerCore [Default]
│   │   ├── EquipmentSystem [Default] (conditional)
│   │   ├── InventorySystem [Default] (conditional)
│   │   └── UISystem [Default] (conditional)
│   ├── EquipmentSystem [Default]
│   ├── UISystem [Default]
│   └── EnemySystem [Default]
├── InteractionSystem [Default]
├── InventorySystem [Default]
│   └── InteractionSystem
└── SuspenseCore [Default] (re-export)
```

### Findings

| Issue | Severity | Details |
|---|---|---|
| **PlayerCore conditional deps** | LOW | `bWithEquipmentSystem`, `bWithUISystem` --- управляется compile flags. Корректно, но добавляет complexity |
| **UISystem → EquipmentSystem** | MEDIUM | Прямая зависимость. Лучше через interfaces из BridgeSystem |
| **BridgeSystem → Niagara** | MEDIUM | Foundation не должен зависеть от rendering |
| **EnemySystem → PlayerCore** | LOW | One-way, допустимо |
| **Circular deps prevention** | OK | EquipmentSystem намеренно не зависит от PlayerCore (задокументировано) |

### Load Order

```
Phase 1 (PreDefault): BridgeSystem → GAS
Phase 2 (Default):    PlayerCore → InteractionSystem → InventorySystem →
                      EquipmentSystem → SuspenseCore → UISystem → EnemySystem
```

**Verdict:** Граф зависимостей **корректный**, циклических зависимостей нет. Рекомендации по снижению coupling через interfaces.

---

## 13. GDD Compliance Check

### ExtractionPvE_DesignDocument.md

| GDD Requirement | Implementation Status | Notes |
|---|---|---|
| Hideout → Deployment → Raid → Extraction | **PARTIAL** | MapTransitionSubsystem + LevelStreaming Plan |
| Tarkov-style ammo system | **COMPLETE** | MagazineComponent + QuickSlot (8 phases done) |
| Limb damage system | **PLANNED** | GDD exists, no implementation |
| Consumables/Throwables | **COMPLETE** | ItemUse handlers + Grenade system |
| Realistic recoil | **COMPLETE** | Convergence system (Tarkov-style, 6 phases) |
| Enemy AI (Scav/PMC/Boss) | **BASIC** | FSM with 5 states, needs expansion |
| Status effects | **COMPLETE** | DoT service + DebuffWidget + StatusEffect GDD |
| Equipment system (17 slots) | **COMPLETE** | Full slot system with rules engines |
| Level streaming | **PLANNED** | Design exists, no implementation |
| Instance zones | **PLANNED** | Design exists, no implementation |

### Narrative_Design.md

| Requirement | Status |
|---|---|
| Alternative USSR setting | **DOCUMENTED** |
| S.T.A.L.K.E.R. atmosphere | **DOCUMENTED** |
| "Pochtalion-47" concept | **DOCUMENTED** |

### Key GDD Gaps

1. **Limb damage** --- GDD готов, имплементации нет
2. **Level streaming** --- план готов, кода нет
3. **Instance zones** --- дизайн готов, кода нет
4. **Extraction mechanics** --- нет реализации extraction points
5. **Enemy AI depth** --- только базовый FSM, нет тактического поведения

---

## 14. Summary of Recommendations

### Priority 1: CRITICAL (Fix Immediately)

| # | Issue | Action |
|---|---|---|
| 1 | DeathState dangling pointer | Replace raw ptr capture with `TWeakObjectPtr` |
| 2 | Iris migration preparation | Start research, create PoC branch |

### Priority 2: HIGH (Next Sprint)

| # | Issue | Action |
|---|---|---|
| 3 | SRP: BaseFireAbility | Extract to 5 sub-components |
| 4 | SRP: DataManager | Extract domain-specific providers |
| 5 | SRP: EquipmentOperationService | Split into 4 services |
| 6 | FCriticalSection → FRWLock | Upgrade read-heavy structures |
| 7 | EventBus type safety | Introduce typed payload structs |
| 8 | EnemySystem EventBus integration | Add EventBus publishing to FSM |
| 9 | GAS simulated proxy optimization | Skip ASC replication for sim proxies |
| 10 | Perception config from BehaviorData | Data-driven perception setup |

### Priority 3: MEDIUM (Backlog)

| # | Issue | Action |
|---|---|---|
| 11 | EventBus UObject → Subsystem | Consider merging into EventManager |
| 12 | IdleState rotation bug | Fix frame-rate dependent interpolation |
| 13 | PatrolState delegate cleanup | Add BeginDestroy override |
| 14 | ChaseState excessive logging | Change to Verbose |
| 15 | DataManager async loading | Use FStreamableManager |
| 16 | BridgeSystem Niagara/UMG deps | Move to appropriate modules |
| 17 | SecurityValidator rate limits | Per-operation-type rate limiting |
| 18 | ExecuteAttack implementation | Wire to GAS abilities |
| 19 | StateTree evaluation | Evaluate for complex AI behaviors |
| 20 | UISystem → Equipment coupling | Decouple via BridgeSystem interfaces |

### Priority 4: LOW (Cleanup)

| # | Issue | Action |
|---|---|---|
| 21 | Mixed language comments | Standardize to English |
| 22 | Copyright year | Update to 2025-2026 |
| 23 | DisabledModules | Delete legacy code |
| 24 | Legasy typo | Fix spelling |
| 25 | Plugin beta status | Update if production-ready |
| 26 | Deprecated methods | Remove dead code |

---

## 15. Appendix: Sources

### UE5 Networking & Iris
- [Iris Replication System - UE 5.7 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/iris-replication-system-in-unreal-engine)
- [Introduction to Iris](https://dev.epicgames.com/documentation/en-us/unreal-engine/introduction-to-iris-in-unreal-engine)
- [Migrate to Iris](https://dev.epicgames.com/documentation/en-us/unreal-engine/migrate-to-iris-in-unreal-engine)
- [Components of Iris](https://dev.epicgames.com/documentation/en-us/unreal-engine/components-of-iris-in-unreal-engine)
- [Iris: 100 Players Benchmark](https://bormor.dev/posts/iris-one-hundred-players/)
- [Networking Overview - UE 5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/networking-overview-for-unreal-engine)
- [Building Small-Scale MMO in UE5 (2026)](https://medium.com/@Jamesroha/building-a-small-scale-mmo-in-unreal-engine-5-the-solo-developers-practical-guide-e7c7ab17eaae)

### GAS Best Practices
- [Gameplay Attributes & Attribute Sets - UE 5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-attributes-and-attribute-sets-for-the-gameplay-ability-system-in-unreal-engine)
- [Understanding GAS - UE 5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/understanding-the-unreal-engine-gameplay-ability-system)
- [GAS Best Practices for Setup](https://dev.epicgames.com/community/learning/tutorials/DPpd/unreal-engine-gameplay-ability-system-best-practices-for-setup)
- [GAS Documentation (tranek)](https://github.com/tranek/GASDocumentation)
- [GAS Advanced Network Optimizations](https://vorixo.github.io/devtricks/gas-replication-proxy/)
- [GAS Prediction Overview](https://ikrima.dev/ue4guide/gameplay-programming/gameplay-ability-system/gas-networking/)

### Threading & Performance
- [Mastering Multithreading in UE5 C++](https://inoland.net/unreal-engine-5-multithreading/)
- [How to use mutex in UE](https://georgy.dev/posts/mutex/)
- [MultiThreading Guide (UE Wiki)](https://unrealcommunity.wiki/multithreading-and-synchronization-guide-9l0xyz17)
- [FRWScopeLock - UE Docs](https://docs.unrealengine.com/5.2/en-US/API/Runtime/Core/Misc/FRWScopeLock/)

### Event Bus Pattern
- [Unreal-EventBus (MrRobinOfficial)](https://github.com/MrRobinOfficial/Unreal-EventBus)

---

**End of Audit Report**
*Generated: 2026-02-06 | Auditor: Senior Tech Lead | SuspenseCore v1.0 Phase 4*
