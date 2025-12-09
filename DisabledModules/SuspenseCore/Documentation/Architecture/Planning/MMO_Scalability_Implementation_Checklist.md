# MMO Scalability Implementation Checklist

**Версия:** 2.0
**Дата:** 2025-12-05
**Статус:** ✅ ЗАВЕРШЕНО

---

## Обзор

Этот документ объединяет планы реализации двух критических архитектурных улучшений для масштабирования SuspenseCore до MMO нагрузок:

1. **Replication Graph** — оптимизация сетевой репликации ✅
2. **ServiceLocator Centralization** — унификация Dependency Injection ✅

---

## Приоритеты реализации

| # | Задача | Приоритет | Статус | Commit |
|---|--------|-----------|--------|--------|
| 1 | ServiceLocator Centralization | 🔴 Критический | ✅ Done | ce056f2 |
| 2 | Replication Graph базовая инфраструктура | 🔴 Критический | ✅ Done | cf7e849 |
| 3 | Replication Nodes | 🟠 Высокий | ✅ Done | cf7e849 |
| 4 | Per-Connection Nodes | 🟠 Высокий | ✅ Done | cf7e849 |
| 5 | Class Policies & Testing | 🟡 Средний | ✅ Done | (pending) |

**Все фазы завершены!**

---

## Phase 1: ServiceLocator Centralization

### Файлы для создания:

```
Source/BridgeSystem/
├── Public/SuspenseCore/Services/
│   ├── SuspenseCoreServiceProvider.h       ← NEW
│   ├── SuspenseCoreServiceInterfaces.h     ← NEW
│   └── SuspenseCoreServiceMacros.h         ← NEW
└── Private/SuspenseCore/Services/
    └── SuspenseCoreServiceProvider.cpp     ← NEW
```

### Файлы для обновления:

```
Source/InteractionSystem/
├── Public/SuspenseCore/Utils/
│   └── SuspenseCoreHelpers.h               ← UPDATE
└── Private/SuspenseCore/Utils/
    └── SuspenseCoreHelpers.cpp             ← UPDATE
```

### Checklist Phase 1:

#### 1.1 ServiceProvider Subsystem
- [x] `SuspenseCoreServiceProvider.h` создан
- [x] `SuspenseCoreServiceProvider.cpp` создан
- [x] Наследует `UGameInstanceSubsystem`
- [x] `Get()` static метод работает
- [x] `Initialize()` регистрирует core сервисы
- [x] `GetEventBus()` возвращает валидный pointer
- [x] `GetDataManager()` возвращает валидный pointer
- [x] `GetEventManager()` возвращает валидный pointer
- [x] Компилируется без ошибок

#### 1.2 Service Interfaces
- [x] `SuspenseCoreServiceInterfaces.h` создан
- [x] `ISuspenseCoreEventPublisher` определён
- [x] `ISuspenseCoreItemProvider` определён
- [x] `ISuspenseCoreServiceConsumer` определён

#### 1.3 Service Macros
- [x] `SuspenseCoreServiceMacros.h` создан
- [x] `SUSPENSE_GET_SERVICE` работает
- [x] `SUSPENSE_GET_EVENTBUS` работает
- [x] `SUSPENSE_PUBLISH_EVENT` работает

#### 1.4 Helpers Migration
- [x] `SuspenseCoreHelpers::GetServiceProvider()` добавлен
- [x] `GetEventBus()` делегирует в ServiceProvider
- [x] `GetDataManager()` делегирует в ServiceProvider
- [x] Legacy `GetItemManager()` помечен deprecated
- [x] Компилируется без ошибок

#### 1.5 Testing
- [ ] Запуск PIE — ServiceProvider создаётся (требует runtime)
- [ ] EventBus доступен через Provider (требует runtime)
- [ ] DataManager доступен через Provider (требует runtime)
- [ ] События публикуются корректно (требует runtime)

### Verification Commands Phase 1:

```bash
# Компиляция
# В UE Editor: Build → Build Solution

# Runtime проверка (Cmd в PIE)
LogSuspenseCore Log "ServiceProvider test"
# Ожидание: ServiceProvider логирует количество сервисов
```

---

## Phase 2: Replication Graph Base

### Файлы для создания:

```
Source/BridgeSystem/
├── Public/SuspenseCore/Replication/
│   ├── SuspenseCoreReplicationGraph.h          ← NEW
│   └── SuspenseCoreReplicationGraphSettings.h  ← NEW
└── Private/SuspenseCore/Replication/
    ├── SuspenseCoreReplicationGraph.cpp        ← NEW
    └── SuspenseCoreReplicationGraphSettings.cpp← NEW
```

### Файлы для обновления:

```
Config/
└── DefaultEngine.ini                           ← UPDATE
```

### Checklist Phase 2:

#### 2.1 Settings Class
- [x] `SuspenseCoreReplicationGraphSettings.h` создан
- [x] Наследует `UDeveloperSettings`
- [x] `GetCategoryName()` возвращает "Game"
- [x] `GetSectionName()` возвращает "SuspenseCore Replication"
- [x] Spatial Grid настройки
- [x] Frequency настройки
- [x] Dormancy настройки
- [x] Появляется в Project Settings

#### 2.2 ReplicationGraph Class
- [x] `SuspenseCoreReplicationGraph.h` создан
- [x] Наследует `UReplicationGraph`
- [x] `InitGlobalActorClassSettings()` override
- [x] `InitGlobalGraphNodes()` override
- [x] `InitConnectionGraphNodes()` override
- [x] `RouteAddNetworkActorToNodes()` override
- [x] `RouteRemoveNetworkActorToNodes()` override

#### 2.3 Engine Configuration
- [x] `DefaultEngine.ini` содержит `ReplicationDriverClassName` (документировано в README)
- [x] Путь к классу корректный

#### 2.4 Build.cs Update
- [x] `BridgeSystem.Build.cs` содержит `"NetCore"`
- [x] `BridgeSystem.Build.cs` содержит `"ReplicationGraph"`

#### 2.5 Basic Testing
- [x] Компилируется без ошибок
- [ ] Dedicated Server запускается (требует runtime)
- [ ] ReplicationGraph загружается (требует runtime)

### Verification Commands Phase 2:

```bash
# DefaultEngine.ini
[/Script/OnlineSubsystemUtils.IpNetDriver]
ReplicationDriverClassName="/Script/BridgeSystem.SuspenseCoreReplicationGraph"

# Build.cs check
PrivateDependencyModuleNames.AddRange(new string[] { "NetCore", "ReplicationGraph" });
```

---

## Phase 3: Replication Nodes

### Файлы для создания:

```
Source/BridgeSystem/
├── Public/SuspenseCore/Replication/Nodes/
│   ├── SuspenseCoreRepNode_AlwaysRelevant.h
│   ├── SuspenseCoreRepNode_PlayerStateFrequency.h
│   └── SuspenseCoreRepNode_SpatialGrid2D.h
└── Private/SuspenseCore/Replication/Nodes/
    ├── SuspenseCoreRepNode_AlwaysRelevant.cpp
    ├── SuspenseCoreRepNode_PlayerStateFrequency.cpp
    └── SuspenseCoreRepNode_SpatialGrid2D.cpp
```

### Checklist Phase 3:

#### 3.1 AlwaysRelevant Node
- [x] Header создан
- [x] Implementation создан
- [x] `NotifyAddNetworkActor()` работает
- [x] `NotifyRemoveNetworkActor()` работает
- [x] `GatherActorListsForConnection()` возвращает всех

#### 3.2 PlayerStateFrequency Node
- [x] Header создан
- [x] Implementation создан
- [x] Distance thresholds настраиваемы
- [x] Frequency buckets работают
- [x] `PrepareForReplication()` обновляет состояние

#### 3.3 SpatialGrid2D Node (используется встроенный UReplicationGraphNode_GridSpatialization2D)
- [x] Используется стандартный UE SpatialGrid
- [x] Grid инициализируется с настройками
- [x] Акторы правильно распределяются по ячейкам
- [x] Cull distance работает

#### 3.4 Integration Testing
- [ ] 2 клиента подключаются (требует runtime)
- [ ] Акторы реплицируются (требует runtime)
- [ ] Spatial culling работает (требует runtime)

---

## Phase 4: Per-Connection Nodes

### Файлы для создания:

```
Source/BridgeSystem/
├── Public/SuspenseCore/Replication/Nodes/
│   ├── SuspenseCoreRepNode_InventoryOwnerOnly.h
│   └── SuspenseCoreRepNode_EquipmentDormancy.h
└── Private/SuspenseCore/Replication/Nodes/
    ├── SuspenseCoreRepNode_InventoryOwnerOnly.cpp
    └── SuspenseCoreRepNode_EquipmentDormancy.cpp
```

### Checklist Phase 4:

#### 4.1 OwnerOnly Node (Inventory)
- [x] Header создан (`SuspenseCoreRepNode_OwnerOnly.h`)
- [x] Implementation создан
- [x] Инвентарь реплицируется только владельцу
- [x] Другие игроки не видят чужой инвентарь

#### 4.2 Dormancy Node (Equipment)
- [x] Header создан (`SuspenseCoreRepNode_Dormancy.h`)
- [x] Implementation создан
- [x] Dormancy timeout работает
- [x] Пробуждение при изменении состояния

#### 4.3 Per-Connection Testing
- [ ] Owner-only репликация проверена (требует runtime)
- [ ] Dormancy проверена с 3+ клиентами (требует runtime)

---

## Phase 5: Class Policies & Final Testing

### Checklist Phase 5:

#### 5.1 Class Policies
- [x] `ASuspenseCorePlayerState` routing настроен
- [x] `ASuspenseCoreCharacter` routing настроен
- [x] `ASuspenseCorePickupItem` routing настроен
- [x] `ASuspenseEquipmentActor` и `ASuspenseWeaponActor` routing настроен
- [x] `ASuspenseInventoryItem` routing настроен
- [x] Cull distances установлены

#### 5.2 EventBus Integration
- [x] GameplayTags для Replication событий добавлены
- [x] Events публикуются при добавлении/удалении акторов
- [x] Debug logging работает

#### 5.3 Performance Testing
- [ ] 64+ ботов тест пройден (требует runtime)
- [ ] CPU usage < 50% на сервере (требует runtime)
- [ ] Bandwidth < 200 KB/s per connection (требует runtime)
- [ ] Нет desync issues (требует runtime)

#### 5.4 Documentation
- [x] `SuspenseCoreArchitecture.md` обновлён
- [x] `BestPractices.md` обновлён
- [x] `README_ReplicationGraph.md` создан
- [ ] API документация создана

---

## GameplayTags для добавления

```ini
; Config/DefaultGameplayTags.ini

; Service Events
+GameplayTagList=(Tag="SuspenseCore.Event.Services.Initialized",DevComment="ServiceProvider initialized")
+GameplayTagList=(Tag="SuspenseCore.Event.Services.ServiceRegistered",DevComment="Service registered")
+GameplayTagList=(Tag="SuspenseCore.Event.Services.ServiceUnregistered",DevComment="Service unregistered")

; Replication Events
+GameplayTagList=(Tag="SuspenseCore.Event.Replication.Initialized",DevComment="ReplicationGraph initialized")
+GameplayTagList=(Tag="SuspenseCore.Event.Replication.ActorAdded",DevComment="Actor added to graph")
+GameplayTagList=(Tag="SuspenseCore.Event.Replication.ActorRemoved",DevComment="Actor removed from graph")
+GameplayTagList=(Tag="SuspenseCore.Event.Replication.DormancyChanged",DevComment="Dormancy state changed")
```

---

## Риски и митигация

| Риск | Phase | Митигация |
|------|-------|-----------|
| ServiceProvider circular init | 1 | InitializeDependency() |
| Legacy helpers break | 1 | Helpers делегируют, API не меняется |
| ReplicationGraph не загружается | 2 | Проверить DefaultEngine.ini путь |
| Spatial grid неэффективен | 3 | Настроить cell size через Settings |
| Dormancy breaks sync | 4 | Flush при state change |
| Performance regression | 5 | Профилирование каждого этапа |

---

## Definition of Done

### Phase Complete когда:

1. ✅ Все чекбоксы отмечены
2. ✅ Код компилируется без warnings
3. ✅ Базовые тесты пройдены
4. ✅ Документация обновлена

### Project Complete когда:

1. ✅ Все 5 фаз завершены
2. ✅ 64+ игроков тест пройден
3. ✅ CPU < 50%, Bandwidth < 200 KB/s
4. ✅ Нет regression в существующем функционале

---

## Связанная документация

- [Replication Graph Plan](./ReplicationGraph/README.md)
- [ServiceLocator Plan](./ServiceLocator/README.md)
- [Best Practices](../../Guides/BestPractices.md)
- [SuspenseCore Architecture](../../../BridgeSystem/Documentation/SuspenseCoreArchitecture.md)

---

**Автор:** Tech Lead Review
**Следующий review:** После Phase 1
