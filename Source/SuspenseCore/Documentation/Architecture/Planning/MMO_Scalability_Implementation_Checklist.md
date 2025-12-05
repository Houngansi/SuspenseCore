# MMO Scalability Implementation Checklist

**Версия:** 1.0
**Дата:** 2025-12-05
**Статус:** Планирование

---

## Обзор

Этот документ объединяет планы реализации двух критических архитектурных улучшений для масштабирования SuspenseCore до MMO нагрузок:

1. **Replication Graph** — оптимизация сетевой репликации
2. **ServiceLocator Centralization** — унификация Dependency Injection

---

## Приоритеты реализации

| # | Задача | Приоритет | Зависимости | Estimated |
|---|--------|-----------|-------------|-----------|
| 1 | ServiceLocator Centralization | 🔴 Критический | Нет | 2-3 дня |
| 2 | Replication Graph базовая инфраструктура | 🔴 Критический | #1 | 2-3 дня |
| 3 | Replication Nodes | 🟠 Высокий | #2 | 3-4 дня |
| 4 | Per-Connection Nodes | 🟠 Высокий | #3 | 2-3 дня |
| 5 | Class Policies & Testing | 🟡 Средний | #4 | 2-3 дня |

**Рекомендуемый порядок:** ServiceLocator → ReplicationGraph (последовательно)

**Причина:** ServiceLocator упрощает тестирование и интеграцию ReplicationGraph с EventBus.

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
- [ ] `SuspenseCoreServiceProvider.h` создан
- [ ] `SuspenseCoreServiceProvider.cpp` создан
- [ ] Наследует `UGameInstanceSubsystem`
- [ ] `Get()` static метод работает
- [ ] `Initialize()` регистрирует core сервисы
- [ ] `GetEventBus()` возвращает валидный pointer
- [ ] `GetDataManager()` возвращает валидный pointer
- [ ] `GetEventManager()` возвращает валидный pointer
- [ ] Компилируется без ошибок

#### 1.2 Service Interfaces
- [ ] `SuspenseCoreServiceInterfaces.h` создан
- [ ] `ISuspenseCoreEventPublisher` определён
- [ ] `ISuspenseCoreItemProvider` определён
- [ ] `ISuspenseCoreServiceConsumer` определён

#### 1.3 Service Macros
- [ ] `SuspenseCoreServiceMacros.h` создан
- [ ] `SUSPENSE_GET_SERVICE` работает
- [ ] `SUSPENSE_GET_EVENTBUS` работает
- [ ] `SUSPENSE_PUBLISH_EVENT` работает

#### 1.4 Helpers Migration
- [ ] `SuspenseCoreHelpers::GetServiceProvider()` добавлен
- [ ] `GetEventBus()` делегирует в ServiceProvider
- [ ] `GetDataManager()` делегирует в ServiceProvider
- [ ] Legacy `GetItemManager()` помечен deprecated
- [ ] Компилируется без ошибок

#### 1.5 Testing
- [ ] Запуск PIE — ServiceProvider создаётся
- [ ] EventBus доступен через Provider
- [ ] DataManager доступен через Provider
- [ ] События публикуются корректно

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
- [ ] `SuspenseCoreReplicationGraphSettings.h` создан
- [ ] Наследует `UDeveloperSettings`
- [ ] `GetCategoryName()` возвращает "Game"
- [ ] `GetSectionName()` возвращает "SuspenseCore Replication"
- [ ] Spatial Grid настройки
- [ ] Frequency настройки
- [ ] Dormancy настройки
- [ ] Появляется в Project Settings

#### 2.2 ReplicationGraph Class
- [ ] `SuspenseCoreReplicationGraph.h` создан
- [ ] Наследует `UReplicationGraph`
- [ ] `InitGlobalActorClassSettings()` override
- [ ] `InitGlobalGraphNodes()` override
- [ ] `InitConnectionGraphNodes()` override
- [ ] `RouteAddNetworkActorToNodes()` override
- [ ] `RouteRemoveNetworkActorToNodes()` override

#### 2.3 Engine Configuration
- [ ] `DefaultEngine.ini` содержит `ReplicationDriverClassName`
- [ ] Путь к классу корректный

#### 2.4 Build.cs Update
- [ ] `BridgeSystem.Build.cs` содержит `"NetCore"`
- [ ] `BridgeSystem.Build.cs` содержит `"ReplicationGraph"`

#### 2.5 Basic Testing
- [ ] Компилируется без ошибок
- [ ] Dedicated Server запускается
- [ ] ReplicationGraph загружается (логи)

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
- [ ] Header создан
- [ ] Implementation создан
- [ ] `NotifyAddNetworkActor()` работает
- [ ] `NotifyRemoveNetworkActor()` работает
- [ ] `GatherActorListsForConnection()` возвращает всех

#### 3.2 PlayerStateFrequency Node
- [ ] Header создан
- [ ] Implementation создан
- [ ] Distance thresholds настраиваемы
- [ ] Frequency buckets работают
- [ ] `PrepareForReplication()` обновляет состояние

#### 3.3 SpatialGrid2D Node
- [ ] Header создан
- [ ] Implementation создан
- [ ] Grid инициализируется с настройками
- [ ] Акторы правильно распределяются по ячейкам
- [ ] Cull distance работает

#### 3.4 Integration Testing
- [ ] 2 клиента подключаются
- [ ] Акторы реплицируются
- [ ] Spatial culling работает (далёкие акторы не реплицируются)

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

#### 4.1 InventoryOwnerOnly Node
- [ ] Header создан
- [ ] Implementation создан
- [ ] Инвентарь реплицируется только владельцу
- [ ] Другие игроки не видят чужой инвентарь

#### 4.2 EquipmentDormancy Node
- [ ] Header создан
- [ ] Implementation создан
- [ ] Dormancy timeout работает
- [ ] Пробуждение при изменении состояния

#### 4.3 Per-Connection Testing
- [ ] Owner-only репликация проверена
- [ ] Dormancy проверена с 3+ клиентами

---

## Phase 5: Class Policies & Final Testing

### Checklist Phase 5:

#### 5.1 Class Policies
- [ ] `ASuspenseCorePlayerState` routing настроен
- [ ] `ASuspenseCoreCharacter` routing настроен
- [ ] `ASuspenseCorePickupItem` routing настроен
- [ ] Cull distances установлены

#### 5.2 EventBus Integration
- [ ] GameplayTags для Replication событий добавлены
- [ ] Events публикуются при добавлении/удалении акторов
- [ ] Debug logging работает

#### 5.3 Performance Testing
- [ ] 64+ ботов тест пройден
- [ ] CPU usage < 50% на сервере
- [ ] Bandwidth < 200 KB/s per connection
- [ ] Нет desync issues

#### 5.4 Documentation
- [ ] `SuspenseCoreArchitecture.md` обновлён
- [ ] `BestPractices.md` обновлён
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
