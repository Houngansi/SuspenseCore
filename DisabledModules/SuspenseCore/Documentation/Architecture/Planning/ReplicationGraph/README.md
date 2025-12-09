# Replication Graph Integration Plan

**Модуль:** BridgeSystem
**Приоритет:** Критический (MMO масштабирование)
**Статус:** Планирование
**Дата:** 2025-12-05

---

## Обзор

Replication Graph — система Unreal Engine для оптимизации сетевой репликации при большом количестве игроков (64+). Без неё сервер тратит O(N²) на проверку relevancy каждого актора для каждого соединения.

### Текущее состояние

```
Проблема:
┌─────────────────────────────────────────────────────────┐
│ Server Tick                                             │
│ ├── ForEachConnection (N players)                       │
│ │   └── ForEachActor (M actors)                         │
│ │       └── IsNetRelevantFor() ← O(N × M) каждый тик!  │
│ └── Bandwidth: Все акторы → Все клиенты                │
└─────────────────────────────────────────────────────────┘

При 100 игроках и 5000 акторах = 500,000 проверок/тик
```

### Целевое состояние

```
С Replication Graph:
┌─────────────────────────────────────────────────────────┐
│ Server Tick                                             │
│ ├── ReplicationGraph.ServerReplicateActors()            │
│ │   ├── SpatialGrid.GetActorsInRadius() ← O(1)         │
│ │   ├── AlwaysRelevant.GetAll() ← O(constant)          │
│ │   └── PerConnection.GetOwned() ← O(1)                │
│ └── Bandwidth: Только relevant акторы → Клиент          │
└─────────────────────────────────────────────────────────┘

При 100 игроках = ~100 проверок/тик (константа)
```

---

## Архитектура

### Иерархия классов

```
UReplicationGraph (Engine)
└── USuspenseCoreReplicationGraph
    ├── GlobalActorReplicationInfo     ← Настройки по классам
    ├── ConnectionManager              ← Per-connection данные
    └── Nodes:
        ├── USuspenseCoreRepNode_AlwaysRelevant
        ├── USuspenseCoreRepNode_PlayerStateFrequency
        ├── USuspenseCoreRepNode_SpatialGrid2D
        ├── USuspenseCoreRepNode_EquipmentDormancy
        └── USuspenseCoreRepNode_InventoryOwnerOnly
```

### Файловая структура

```
Source/BridgeSystem/
├── Public/SuspenseCore/Replication/
│   ├── SuspenseCoreReplicationGraph.h
│   ├── SuspenseCoreReplicationGraphSettings.h
│   ├── Nodes/
│   │   ├── SuspenseCoreRepNode_AlwaysRelevant.h
│   │   ├── SuspenseCoreRepNode_PlayerStateFrequency.h
│   │   ├── SuspenseCoreRepNode_SpatialGrid2D.h
│   │   ├── SuspenseCoreRepNode_EquipmentDormancy.h
│   │   └── SuspenseCoreRepNode_InventoryOwnerOnly.h
│   └── Policies/
│       ├── SuspenseCoreClassReplicationPolicy.h
│       └── SuspenseCorePriorityPolicy.h
├── Private/SuspenseCore/Replication/
│   ├── SuspenseCoreReplicationGraph.cpp
│   ├── Nodes/*.cpp
│   └── Policies/*.cpp
└── Documentation/
    └── SuspenseCoreArchitecture.md (обновить)
```

---

## Этапы реализации

### Этап 1: Базовая инфраструктура

**Задачи:**

1. Создать `USuspenseCoreReplicationGraph`
2. Создать `USuspenseCoreReplicationGraphSettings` (UDeveloperSettings)
3. Настроить регистрацию в `DefaultEngine.ini`

**Код:**

```cpp
// SuspenseCoreReplicationGraph.h
#pragma once

#include "CoreMinimal.h"
#include "ReplicationGraph.h"
#include "SuspenseCoreReplicationGraph.generated.h"

class USuspenseCoreReplicationGraphSettings;
class USuspenseCoreEventBus;

/**
 * USuspenseCoreReplicationGraph
 *
 * Custom Replication Graph для MMO шутера.
 * Оптимизирует репликацию для 100+ игроков.
 *
 * Интеграция с SuspenseCore:
 * - Публикует события через EventBus
 * - Использует SuspenseCoreSettings для конфигурации
 * - Поддерживает dynamic dormancy для Equipment
 */
UCLASS(Transient, Config = Engine)
class BRIDGESYSTEM_API USuspenseCoreReplicationGraph : public UReplicationGraph
{
    GENERATED_BODY()

public:
    USuspenseCoreReplicationGraph();

    //~ UReplicationGraph Interface
    virtual void InitGlobalActorClassSettings() override;
    virtual void InitGlobalGraphNodes() override;
    virtual void InitConnectionGraphNodes(UNetReplicationGraphConnection* RepGraphConnection) override;
    virtual void RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalInfo) override;
    virtual void RouteRemoveNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo) override;

    //==========================================================================
    // SuspenseCore Integration
    //==========================================================================

    /** Get settings from Project Settings */
    const USuspenseCoreReplicationGraphSettings* GetSuspenseCoreSettings() const;

    /** Publish replication event to EventBus */
    void PublishReplicationEvent(FGameplayTag EventTag, AActor* Actor);

protected:
    //==========================================================================
    // Global Nodes (shared across all connections)
    //==========================================================================

    /** Always relevant actors (GameState, WorldSettings) */
    UPROPERTY()
    TObjectPtr<USuspenseCoreRepNode_AlwaysRelevant> AlwaysRelevantNode;

    /** PlayerStates with adaptive frequency */
    UPROPERTY()
    TObjectPtr<USuspenseCoreRepNode_PlayerStateFrequency> PlayerStateNode;

    /** Spatial grid for world actors */
    UPROPERTY()
    TObjectPtr<USuspenseCoreRepNode_SpatialGrid2D> SpatialGridNode;

    //==========================================================================
    // Class Policies
    //==========================================================================

    /** Configure class-specific replication policies */
    void ConfigureClassPolicies();

    /** Setup routing for SuspenseCore classes */
    void SetupSuspenseCoreClassRouting();

private:
    /** Cached EventBus */
    UPROPERTY()
    TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

    /** Get or find EventBus */
    USuspenseCoreEventBus* GetEventBus() const;
};
```

```cpp
// SuspenseCoreReplicationGraphSettings.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SuspenseCoreReplicationGraphSettings.generated.h"

/**
 * USuspenseCoreReplicationGraphSettings
 *
 * Project Settings → Game → SuspenseCore → Replication
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "SuspenseCore Replication"))
class BRIDGESYSTEM_API USuspenseCoreReplicationGraphSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    USuspenseCoreReplicationGraphSettings();

    //==========================================================================
    // Spatial Grid
    //==========================================================================

    /** Grid cell size in Unreal units (default: 10000 = 100m) */
    UPROPERTY(Config, EditAnywhere, Category = "Spatial Grid", meta = (ClampMin = "1000", ClampMax = "50000"))
    float SpatialGridCellSize = 10000.0f;

    /** Grid extent (half-size of world) */
    UPROPERTY(Config, EditAnywhere, Category = "Spatial Grid", meta = (ClampMin = "100000"))
    float SpatialGridExtent = 500000.0f;

    /** Enable spatial grid for Characters */
    UPROPERTY(Config, EditAnywhere, Category = "Spatial Grid")
    bool bUseSpatialGridForCharacters = true;

    /** Enable spatial grid for Pickups */
    UPROPERTY(Config, EditAnywhere, Category = "Spatial Grid")
    bool bUseSpatialGridForPickups = true;

    //==========================================================================
    // Frequency Buckets
    //==========================================================================

    /** Near distance threshold (full frequency) */
    UPROPERTY(Config, EditAnywhere, Category = "Frequency", meta = (ClampMin = "500"))
    float NearDistanceThreshold = 2000.0f;

    /** Mid distance threshold (reduced frequency) */
    UPROPERTY(Config, EditAnywhere, Category = "Frequency", meta = (ClampMin = "1000"))
    float MidDistanceThreshold = 5000.0f;

    /** Far distance threshold (minimal frequency) */
    UPROPERTY(Config, EditAnywhere, Category = "Frequency", meta = (ClampMin = "2000"))
    float FarDistanceThreshold = 15000.0f;

    //==========================================================================
    // Dormancy
    //==========================================================================

    /** Enable dormancy for Equipment actors */
    UPROPERTY(Config, EditAnywhere, Category = "Dormancy")
    bool bEnableEquipmentDormancy = true;

    /** Dormancy timeout for inactive equipment (seconds) */
    UPROPERTY(Config, EditAnywhere, Category = "Dormancy", meta = (ClampMin = "1.0"))
    float EquipmentDormancyTimeout = 5.0f;

    //==========================================================================
    // Debugging
    //==========================================================================

    /** Enable replication graph debug drawing */
    UPROPERTY(Config, EditAnywhere, Category = "Debug")
    bool bEnableDebugVisualization = false;

    /** Log replication decisions */
    UPROPERTY(Config, EditAnywhere, Category = "Debug")
    bool bLogReplicationDecisions = false;

    //==========================================================================
    // Access
    //==========================================================================

    static const USuspenseCoreReplicationGraphSettings* Get()
    {
        return GetDefault<USuspenseCoreReplicationGraphSettings>();
    }

    virtual FName GetCategoryName() const override { return FName("Game"); }
    virtual FName GetSectionName() const override { return FName("SuspenseCore Replication"); }
};
```

**DefaultEngine.ini:**

```ini
[/Script/OnlineSubsystemUtils.IpNetDriver]
ReplicationDriverClassName="/Script/BridgeSystem.SuspenseCoreReplicationGraph"
```

---

### Этап 2: Replication Nodes

**Задачи:**

1. `USuspenseCoreRepNode_AlwaysRelevant` — GameState, WorldSettings
2. `USuspenseCoreRepNode_PlayerStateFrequency` — адаптивная частота PlayerState
3. `USuspenseCoreRepNode_SpatialGrid2D` — пространственное разбиение

**Код:**

```cpp
// SuspenseCoreRepNode_AlwaysRelevant.h
#pragma once

#include "ReplicationGraph.h"
#include "SuspenseCoreRepNode_AlwaysRelevant.generated.h"

/**
 * Node for actors that are ALWAYS relevant to ALL connections.
 * Examples: GameState, GameMode proxies, World settings
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreRepNode_AlwaysRelevant : public UReplicationGraphNode
{
    GENERATED_BODY()

public:
    virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& ActorInfo) override;
    virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& ActorInfo, bool bWarnIfNotFound = true) override;
    virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;

protected:
    UPROPERTY()
    FActorRepListRefView ReplicationActorList;
};
```

```cpp
// SuspenseCoreRepNode_PlayerStateFrequency.h
#pragma once

#include "ReplicationGraph.h"
#include "SuspenseCoreRepNode_PlayerStateFrequency.generated.h"

/**
 * Node for PlayerStates with distance-based frequency adjustment.
 *
 * Near players: 60 Hz
 * Mid-range: 30 Hz
 * Far players: 10 Hz
 * Very far: 5 Hz
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreRepNode_PlayerStateFrequency : public UReplicationGraphNode
{
    GENERATED_BODY()

public:
    virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& ActorInfo) override;
    virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& ActorInfo, bool bWarnIfNotFound = true) override;
    virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;
    virtual void PrepareForReplication() override;

    /** Set distance thresholds */
    void SetDistanceThresholds(float Near, float Mid, float Far);

protected:
    UPROPERTY()
    TArray<TObjectPtr<APlayerState>> AllPlayerStates;

    /** Distance thresholds squared (for fast comparison) */
    float NearDistSq = 4000000.0f;   // 2000^2
    float MidDistSq = 25000000.0f;   // 5000^2
    float FarDistSq = 225000000.0f;  // 15000^2

    /** Frequency buckets */
    uint32 FrameCounterNear = 0;
    uint32 FrameCounterMid = 0;
    uint32 FrameCounterFar = 0;

    /** Should replicate this frame for bucket */
    bool ShouldReplicateNear() const;
    bool ShouldReplicateMid() const;
    bool ShouldReplicateFar() const;
};
```

```cpp
// SuspenseCoreRepNode_SpatialGrid2D.h
#pragma once

#include "ReplicationGraph.h"
#include "SuspenseCoreRepNode_SpatialGrid2D.generated.h"

/**
 * 2D spatial grid for efficient actor relevancy.
 * Actors only replicate to connections viewing nearby cells.
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreRepNode_SpatialGrid2D : public UReplicationGraphNode_GridSpatialization2D
{
    GENERATED_BODY()

public:
    USuspenseCoreRepNode_SpatialGrid2D();

    /** Initialize grid with settings */
    void InitializeGrid(float CellSize, float Extent);

    /** Add actor with cull distance override */
    void AddActorWithCullDistance(const FNewReplicatedActorInfo& ActorInfo, float CullDistance);

    //~ UReplicationGraphNode_GridSpatialization2D Interface
    virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;

protected:
    /** Get cull distance for actor class */
    float GetCullDistanceForClass(UClass* ActorClass) const;

    /** Class to cull distance mapping */
    TMap<TObjectPtr<UClass>, float> ClassCullDistances;
};
```

---

### Этап 3: Per-Connection Nodes

**Задачи:**

1. `USuspenseCoreRepNode_InventoryOwnerOnly` — инвентарь только владельцу
2. `USuspenseCoreRepNode_EquipmentDormancy` — dormancy для экипировки

**Код:**

```cpp
// SuspenseCoreRepNode_InventoryOwnerOnly.h
#pragma once

#include "ReplicationGraph.h"
#include "SuspenseCoreRepNode_InventoryOwnerOnly.generated.h"

/**
 * Per-connection node for owner-only actors.
 * Inventory details only replicate to the owning player.
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreRepNode_InventoryOwnerOnly : public UReplicationGraphNode
{
    GENERATED_BODY()

public:
    virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;

    /** Add actor that should only replicate to owner */
    void AddOwnerOnlyActor(AActor* Actor);

    /** Remove owner-only actor */
    bool RemoveOwnerOnlyActor(AActor* Actor);

protected:
    UPROPERTY()
    FActorRepListRefView OwnerOnlyActors;
};
```

---

### Этап 4: Class Policies

**Задачи:**

1. Настроить routing для всех SuspenseCore классов
2. Интегрировать с EventBus для мониторинга

**Код:**

```cpp
// В SuspenseCoreReplicationGraph.cpp

void USuspenseCoreReplicationGraph::SetupSuspenseCoreClassRouting()
{
    // PlayerState → PlayerStateFrequency node
    ClassRepNodePolicies.Set(ASuspenseCorePlayerState::StaticClass(),
        FClassReplicationInfo{
            .ReplicationPeriodFrame = 1,
            .DistancePriorityScale = 1.0f,
            .StarvationPriorityScale = 1.0f,
            .ActorChannelFrameTimeout = 0 // Never timeout
        });

    // Character → Spatial Grid
    ClassRepNodePolicies.Set(ASuspenseCoreCharacter::StaticClass(),
        FClassReplicationInfo{
            .CullDistanceSquared = 225000000.0f, // 15000^2
            .ReplicationPeriodFrame = 1,
            .DistancePriorityScale = 1.0f
        });

    // Pickup items → Spatial Grid with smaller cull distance
    // (Pickup classes from InteractionSystem)
    ClassRepNodePolicies.Set(ASuspenseCorePickupItem::StaticClass(),
        FClassReplicationInfo{
            .CullDistanceSquared = 100000000.0f, // 10000^2
            .ReplicationPeriodFrame = 2 // Every other frame
        });

    // Projectiles → Spatial Grid, high frequency
    // (When implemented)
}

void USuspenseCoreReplicationGraph::ConfigureClassPolicies()
{
    // Always Relevant classes
    auto& AlwaysRelevantClasses = GlobalActorReplicationInfoMap.Find(AGameStateBase::StaticClass())->Settings.bAlwaysRelevant;

    // Grid-based classes
    SpatialGridNode->AddSpatializationClass(ASuspenseCoreCharacter::StaticClass());
    SpatialGridNode->AddSpatializationClass(ASuspenseCorePickupItem::StaticClass());

    // Owner-only: handled per-connection
}
```

---

### Этап 5: EventBus интеграция

**Задачи:**

1. Публикация событий репликации
2. Подписка на системные события

**Код:**

```cpp
void USuspenseCoreReplicationGraph::PublishReplicationEvent(FGameplayTag EventTag, AActor* Actor)
{
    if (USuspenseCoreEventBus* EventBus = GetEventBus())
    {
        FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(Actor);
        EventData.SetString(FName("ActorClass"), Actor->GetClass()->GetName());
        EventBus->Publish(EventTag, EventData);
    }
}

// Events to add to DefaultGameplayTags.ini:
// +GameplayTagList=(Tag="SuspenseCore.Event.Replication.ActorAdded",DevComment="Actor added to replication graph")
// +GameplayTagList=(Tag="SuspenseCore.Event.Replication.ActorRemoved",DevComment="Actor removed from replication graph")
// +GameplayTagList=(Tag="SuspenseCore.Event.Replication.DormancyChanged",DevComment="Actor dormancy state changed")
// +GameplayTagList=(Tag="SuspenseCore.Event.Replication.FrequencyChanged",DevComment="Replication frequency changed")
```

---

## Конфигурация

### DefaultEngine.ini

```ini
[/Script/OnlineSubsystemUtils.IpNetDriver]
ReplicationDriverClassName="/Script/BridgeSystem.SuspenseCoreReplicationGraph"

[/Script/Engine.GameEngine]
!NetDriverDefinitions=ClearArray
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/OnlineSubsystemUtils.IpNetDriver",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")
```

### DefaultGame.ini (через UDeveloperSettings)

```ini
[/Script/BridgeSystem.SuspenseCoreReplicationGraphSettings]
SpatialGridCellSize=10000.0
SpatialGridExtent=500000.0
bUseSpatialGridForCharacters=True
bUseSpatialGridForPickups=True
NearDistanceThreshold=2000.0
MidDistanceThreshold=5000.0
FarDistanceThreshold=15000.0
bEnableEquipmentDormancy=True
EquipmentDormancyTimeout=5.0
```

---

## Метрики успеха

| Метрика | Текущее | Целевое |
|---------|---------|---------|
| Server CPU на 100 игроков | ~80% | <40% |
| Bandwidth per connection | ~500 KB/s | <100 KB/s |
| Relevancy checks per tick | O(N×M) | O(N) |
| Actor update latency | Variable | Consistent |

---

## Зависимости

| Компонент | Зависимость | Статус |
|-----------|-------------|--------|
| EventBus | Требуется | ✅ Готов |
| SuspenseCoreSettings | Требуется | ✅ Готов |
| SuspenseCoreCharacter | Рекомендуется | ✅ Готов |
| SuspenseCorePlayerState | Рекомендуется | ✅ Готов |
| SuspenseCorePickupItem | Рекомендуется | ✅ Готов |

---

## Чеклист реализации

### Этап 1: Базовая инфраструктура
- [ ] Создать `SuspenseCoreReplicationGraph.h/cpp`
- [ ] Создать `SuspenseCoreReplicationGraphSettings.h/cpp`
- [ ] Добавить в `BridgeSystem.Build.cs`: `"NetCore"`, `"ReplicationGraph"`
- [ ] Настроить `DefaultEngine.ini`
- [ ] Скомпилировать и протестировать загрузку

### Этап 2: Replication Nodes
- [ ] Реализовать `SuspenseCoreRepNode_AlwaysRelevant`
- [ ] Реализовать `SuspenseCoreRepNode_PlayerStateFrequency`
- [ ] Реализовать `SuspenseCoreRepNode_SpatialGrid2D`
- [ ] Протестировать с 2 клиентами

### Этап 3: Per-Connection Nodes
- [ ] Реализовать `SuspenseCoreRepNode_InventoryOwnerOnly`
- [ ] Реализовать `SuspenseCoreRepNode_EquipmentDormancy`
- [ ] Протестировать owner-only репликацию

### Этап 4: Class Policies
- [ ] Настроить routing для SuspenseCore классов
- [ ] Настроить cull distances
- [ ] Настроить frequency buckets

### Этап 5: EventBus интеграция
- [ ] Добавить GameplayTags для событий репликации
- [ ] Реализовать публикацию событий
- [ ] Добавить debug visualization

### Этап 6: Тестирование
- [ ] Тест с 64+ ботами
- [ ] Профилирование CPU
- [ ] Профилирование bandwidth
- [ ] Stress test

---

## Риски

| Риск | Вероятность | Влияние | Митигация |
|------|-------------|---------|-----------|
| Legacy код использует старую репликацию | Высокая | Среднее | Gradual migration, fallback |
| Spatial grid неэффективен для вертикальных карт | Средняя | Среднее | Рассмотреть 3D grid |
| Equipment dormancy breaks sync | Средняя | Высокое | Тщательное тестирование |

---

**Автор:** Tech Lead Review
**Версия:** 1.0
**Последнее обновление:** 2025-12-05
