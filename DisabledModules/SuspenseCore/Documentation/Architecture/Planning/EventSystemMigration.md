# Event System Migration Plan

**Статус:** Планирование
**Дата:** 2025-11-28
**Автор:** Tech Lead

---

## Часть 1: Анализ текущего состояния

### 1.1 SuspenseEventManager — Текущая система

**Файл:** `BridgeSystem/Public/Delegates/SuspenseEventManager.h`
**LOC:** ~1060 строк
**Тип:** UGameInstanceSubsystem

#### Архитектура

```
USuspenseEventManager (GameInstanceSubsystem)
├── UI System Delegates (~15 делегатов)
├── Equipment System Delegates (~12 делегатов)
├── Weapon System Delegates (~10 делегатов)
├── Movement System Delegates (~6 делегатов)
├── Loadout System Delegates (~4 делегатов)
├── Inventory UI Delegates (~6 делегатов)
├── Tooltip System Delegates (~3 делегата)
└── Generic Event System (~2 делегата)
```

#### Проблемы

| Проблема | Severity | Описание |
|----------|----------|----------|
| **God Object** | Critical | 50+ делегатов в одном классе, нарушает SRP |
| **Двойные делегаты** | High | Каждое событие имеет Dynamic + Native версию |
| **Compile Time** | High | Изменение одного делегата перекомпилирует всё |
| **Tight Coupling** | High | Все модули зависят от одного класса |
| **Merge Conflicts** | Medium | При параллельной разработке постоянные конфликты |
| **Memory Overhead** | Medium | Даже неиспользуемые делегаты занимают память |
| **No Filtering** | Medium | Нельзя подписаться на подмножество событий |
| **No Priority** | Medium | Порядок вызова не гарантирован |

#### Metrics

```
Делегатов Dynamic (Blueprint): ~30
Делегатов Native (C++): ~35
Notify методов: ~40
Subscribe методов: ~35
Общий размер объекта: ~2KB+
```

---

### 1.2 Новая инфраструктура — EventBus + ServiceLocator

#### SuspenseEquipmentServiceLocator

**Файл:** `BridgeSystem/Public/Core/Services/SuspenseEquipmentServiceLocator.h`
**Тип:** UGameInstanceSubsystem

**Сильные стороны:**
- Dependency Injection через callback
- Lazy initialization сервисов
- Topological sort для зависимостей
- Thread-safe per-service locks
- Lifecycle management (Uninitialized → Ready → Shutdown)
- Reference counting для cleanup

**Паттерн использования:**
```cpp
// Регистрация
ServiceLocator->RegisterServiceClass(
    FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Data")),
    UEquipmentDataService::StaticClass(),
    InitParams
);

// Получение
IEquipmentDataService* Service = ServiceLocator->GetServiceAs<IEquipmentDataService>(ServiceTag);
```

---

#### SuspenseEquipmentEventBus

**Файл:** `BridgeSystem/Public/Core/Utils/SuspenseEquipmentEventBus.h`
**Тип:** TSharedFromThis (singleton)

**Сильные стороны:**
- Tag-based event filtering
- Priority levels (Lowest → System)
- Execution contexts (Immediate, GameThread, AsyncTask, NextFrame)
- Event queue с batch processing
- Delayed events
- Per-owner subscription limits
- Automatic cleanup invalid subscriptions
- RAII scope через FEventSubscriptionScope
- Rich metadata support

**Структура события:**
```cpp
struct FSuspenseEquipmentEventData
{
    FGameplayTag EventType;           // Тег типа события
    TWeakObjectPtr<UObject> Source;   // Источник
    TWeakObjectPtr<UObject> Target;   // Цель
    FString Payload;                  // Строковые данные
    TMap<FString, FString> Metadata;  // Дополнительные данные
    float Timestamp;                  // Время
    EEventPriority Priority;          // Приоритет
    float NumericData;                // Числовые данные
    uint32 Flags;                     // Флаги
};
```

**Паттерн использования:**
```cpp
// Подписка
FEventSubscriptionHandle Handle = EventBus->Subscribe(
    FGameplayTag::RequestGameplayTag(TEXT("Equipment.Weapon.Fired")),
    FEventHandlerDelegate::CreateUObject(this, &UMyClass::OnWeaponFired),
    EEventPriority::High,
    EEventExecutionContext::Immediate,
    this  // Owner для auto-cleanup
);

// Broadcast
BROADCAST_EQUIPMENT_EVENT(WeaponFiredTag, WeaponActor, nullptr, TEXT("Primary"));
```

---

#### SuspenseEquipmentCacheManager

**Файл:** `BridgeSystem/Public/Core/Utils/SuspenseEquipmentCacheManager.h`
**Тип:** Template class

**Сильные стороны:**
- LRU eviction policy
- TTL (time-to-live) per entry
- Data integrity verification (hash)
- Cache poisoning protection
- Rate limiting (updates per second)
- Hit/miss statistics
- Thread-safe operations

---

#### SuspenseEquipmentThreadGuard

**Файл:** `BridgeSystem/Public/Core/Utils/SuspenseEquipmentThreadGuard.h`

**Сильные стороны:**
- Read-Write locks (multiple readers OR single writer)
- RAII guards
- Macros для удобства

---

#### SuspenseGlobalCacheRegistry

**Файл:** `BridgeSystem/Public/Core/Utils/SuspenseGlobalCacheRegistry.h`
**Тип:** Singleton

**Сильные стороны:**
- Централизованная регистрация всех кешей
- Глобальная инвалидация
- Сводная статистика
- Security audit hook

---

## Часть 2: Лучшие практики для MMO Shooters

### 2.1 Event System Requirements для MMO

| Требование | Приоритет | Описание |
|------------|-----------|----------|
| **Масштабируемость** | Critical | 64+ игроков, 1000+ событий/сек |
| **Latency** | Critical | < 1ms на dispatch для UI/gameplay |
| **Network-aware** | Critical | Разделение local/replicated events |
| **Thread Safety** | High | Параллельные операции без deadlocks |
| **Memory Efficiency** | High | Pooling, минимум аллокаций |
| **Filtering** | High | Подписка по тегам, приоритетам |
| **Debugging** | Medium | Tracing, profiling, logging |
| **Hot Reload** | Medium | Изменения без перезапуска |

### 2.2 Рекомендуемая архитектура

```
┌─────────────────────────────────────────────────────────────┐
│                    Event Distribution Layer                  │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌─────────────────┐    ┌─────────────────┐                 │
│  │ Domain EventBus │    │ Domain EventBus │    ...          │
│  │   (Gameplay)    │    │      (UI)       │                 │
│  └────────┬────────┘    └────────┬────────┘                 │
│           │                      │                           │
│           └──────────┬───────────┘                           │
│                      ▼                                       │
│           ┌─────────────────────┐                            │
│           │   Event Router      │                            │
│           │ (Cross-domain)      │                            │
│           └─────────────────────┘                            │
│                      │                                       │
├──────────────────────┼───────────────────────────────────────┤
│                      ▼                                       │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │              Service Locator (DI Container)              │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                               │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐        │
│  │ Service  │ │ Service  │ │ Service  │ │ Service  │        │
│  │  (GAS)   │ │(Equip)   │ │  (Inv)   │ │  (UI)    │        │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘        │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

### 2.3 Domain EventBus vs Global EventManager

| Критерий | Global EventManager | Domain EventBus |
|----------|---------------------|-----------------|
| Coupling | Высокий | Низкий |
| Compile Time | Медленный | Быстрый |
| Memory | Всё в одном объекте | Распределённо |
| Filtering | Нет | По тегам |
| Priority | Нет | Есть |
| Debugging | Сложно | Изолированно |
| Scalability | Плохая | Хорошая |

**Вывод:** Для MMO шутера domain-based EventBus + ServiceLocator — правильный выбор.

---

## Часть 3: Стратегия миграции

### 3.1 Принципы миграции

1. **Incremental** — модуль за модулем, не Big Bang
2. **Backward Compatible** — старый API работает пока нужен
3. **Feature Flags** — переключение между системами
4. **Parallel Running** — обе системы работают одновременно при переходе
5. **Measurable** — метрики до/после каждого этапа

### 3.2 Этапы миграции

```
Этап 0: Подготовка инфраструктуры
         │
         ▼
Этап 1: Core (PlayerCore + GAS + BridgeSystem)
         │
         ▼
Этап 2: InteractionSystem
         │
         ▼
Этап 3: InventorySystem
         │
         ▼
Этап 4: UISystem
         │
         ▼
Этап 5: EquipmentSystem
         │
         ▼
Этап 6: Cleanup & Deprecation
```

---

## Часть 4: Детальный план этапов

### Этап 0: Подготовка инфраструктуры

**Цель:** Создать unified event infrastructure в BridgeSystem

#### 0.1 Переименование и унификация

```
Текущее имя                          → Новое имя
──────────────────────────────────────────────────────────
SuspenseEquipmentEventBus            → SuspenseEventBus
SuspenseEquipmentEventData           → SuspenseEventData
SuspenseEquipmentServiceLocator      → SuspenseServiceLocator
SuspenseEquipmentCacheManager        → SuspenseCacheManager
SuspenseEquipmentThreadGuard         → SuspenseThreadGuard
```

**Обоснование:** Убираем "Equipment" из названий — это общая инфраструктура для всего проекта.

#### 0.2 Domain Event Tags

Создать иерархию GameplayTags для событий:

```ini
; DefaultGameplayTags.ini

+GameplayTagList=(Tag="Event.Core.System")
+GameplayTagList=(Tag="Event.Core.Lifecycle")

+GameplayTagList=(Tag="Event.GAS.Attribute")
+GameplayTagList=(Tag="Event.GAS.Ability")
+GameplayTagList=(Tag="Event.GAS.Effect")

+GameplayTagList=(Tag="Event.Movement.Speed")
+GameplayTagList=(Tag="Event.Movement.State")
+GameplayTagList=(Tag="Event.Movement.Jump")
+GameplayTagList=(Tag="Event.Movement.Crouch")

+GameplayTagList=(Tag="Event.Weapon.Fire")
+GameplayTagList=(Tag="Event.Weapon.Reload")
+GameplayTagList=(Tag="Event.Weapon.State")
+GameplayTagList=(Tag="Event.Weapon.Ammo")

+GameplayTagList=(Tag="Event.Equipment.Slot")
+GameplayTagList=(Tag="Event.Equipment.Operation")
+GameplayTagList=(Tag="Event.Equipment.State")

+GameplayTagList=(Tag="Event.Inventory.Item")
+GameplayTagList=(Tag="Event.Inventory.Slot")
+GameplayTagList=(Tag="Event.Inventory.Weight")

+GameplayTagList=(Tag="Event.UI.Widget")
+GameplayTagList=(Tag="Event.UI.HUD")
+GameplayTagList=(Tag="Event.UI.Screen")
+GameplayTagList=(Tag="Event.UI.Tooltip")

+GameplayTagList=(Tag="Event.Interaction.Focus")
+GameplayTagList=(Tag="Event.Interaction.Pickup")
```

#### 0.3 Event Adapters

Создать adapter между старым EventManager и новым EventBus:

```cpp
// SuspenseEventAdapter.h
class BRIDGESYSTEM_API USuspenseEventAdapter : public UGameInstanceSubsystem
{
    // Bridges old delegates to new EventBus
    void ForwardHealthUpdated(float Current, float Max, float Percent);
    void ForwardAmmoChanged(float Current, float Remaining, float Magazine);
    // ...

    // Временно подписывается на оба
    bool bUseNewEventBus = false;  // Feature flag
};
```

---

### Этап 1: Core (PlayerCore + GAS + BridgeSystem)

**Цель:** Перевести базовые события движения, здоровья, выносливости на новый EventBus

#### 1.1 События для миграции

| Событие | Старый делегат | Новый тег |
|---------|---------------|-----------|
| Health Updated | `OnHealthUpdated` | `Event.GAS.Attribute.Health` |
| Stamina Updated | `OnStaminaUpdated` | `Event.GAS.Attribute.Stamina` |
| Movement Speed | `OnMovementSpeedChanged` | `Event.Movement.Speed` |
| Movement State | `OnMovementStateChanged` | `Event.Movement.State` |
| Jump State | `OnJumpStateChanged` | `Event.Movement.Jump` |
| Crouch State | `OnCrouchStateChanged` | `Event.Movement.Crouch` |
| Landed | `OnLanded` | `Event.Movement.Landed` |

#### 1.2 Изменения в UGASAttributeSet

```cpp
// Before
void UGASAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
    if (USuspenseEventManager* EventMgr = USuspenseEventManager::Get(GetOwningActor()))
    {
        EventMgr->NotifyHealthUpdated(Health.GetCurrentValue(), MaxHealth.GetCurrentValue(), Percent);
    }
}

// After
void UGASAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
    // Broadcast через новый EventBus
    FSuspenseEventData EventData;
    EventData.EventType = FGameplayTag::RequestGameplayTag(TEXT("Event.GAS.Attribute.Health"));
    EventData.Source = GetOwningActor();
    EventData.NumericData = Health.GetCurrentValue();
    EventData.AddMetadata(TEXT("Max"), FString::SanitizeFloat(MaxHealth.GetCurrentValue()));
    EventData.AddMetadata(TEXT("Percent"), FString::SanitizeFloat(Percent));

    SuspenseEventBus::Get()->Broadcast(EventData);

    // TODO: Удалить после полной миграции UI
    if (USuspenseEventManager* EventMgr = USuspenseEventManager::Get(GetOwningActor()))
    {
        EventMgr->NotifyHealthUpdated(Health.GetCurrentValue(), MaxHealth.GetCurrentValue(), Percent);
    }
}
```

#### 1.3 Изменения в USuspenseCharacterMovementComponent

```cpp
// Before
void USuspenseCharacterMovementComponent::OnSprintStateChanged(bool bIsSprinting)
{
    if (USuspenseEventManager* EventMgr = USuspenseEventManager::Get(GetOwner()))
    {
        EventMgr->NotifyMovementStateChanged(SprintTag, false);
    }
}

// After
void USuspenseCharacterMovementComponent::OnSprintStateChanged(bool bIsSprinting)
{
    BROADCAST_EVENT(
        FGameplayTag::RequestGameplayTag(TEXT("Event.Movement.State")),
        GetOwner(),
        nullptr,
        bIsSprinting ? TEXT("Sprint.Start") : TEXT("Sprint.End")
    );
}
```

#### 1.4 Тестирование Этапа 1

**Чеклист:**
- [ ] Компиляция без ошибок
- [ ] Health bar обновляется
- [ ] Stamina bar обновляется
- [ ] Crouch работает
- [ ] Sprint работает
- [ ] Jump/land работает
- [ ] Все unit tests проходят
- [ ] Performance: latency < 1ms

---

### Этап 2: InteractionSystem

**Цель:** Перевести события взаимодействия и pickup

#### 2.1 События для миграции

| Событие | Старый делегат | Новый тег |
|---------|---------------|-----------|
| Interaction Succeeded | `OnInteractionSucceeded` | `Event.Interaction.Succeeded` |
| Interaction Failed | `OnInteractionFailed` | `Event.Interaction.Failed` |
| Focus Gained | local delegate | `Event.Interaction.Focus.Gained` |
| Focus Lost | local delegate | `Event.Interaction.Focus.Lost` |
| Pickup Spawned | local | `Event.Interaction.Pickup.Spawned` |
| Pickup Collected | local | `Event.Interaction.Pickup.Collected` |

#### 2.2 Изменения в USuspenseInteractionComponent

```cpp
// После успешного взаимодействия
void USuspenseInteractionComponent::HandleInteractionSuccess(...)
{
    BROADCAST_EVENT(
        FGameplayTag::RequestGameplayTag(TEXT("Event.Interaction.Succeeded")),
        GetOwner(),
        LastInteractableActor.Get(),
        TEXT("")
    );
}
```

---

### Этап 3: InventorySystem

**Цель:** Перевести события инвентаря

#### 3.1 События для миграции

| Событие | Новый тег |
|---------|-----------|
| Item Added | `Event.Inventory.Item.Added` |
| Item Removed | `Event.Inventory.Item.Removed` |
| Item Moved | `Event.Inventory.Item.Moved` |
| Item Stacked | `Event.Inventory.Item.Stacked` |
| Item Split | `Event.Inventory.Item.Split` |
| Weight Changed | `Event.Inventory.Weight.Changed` |
| Inventory Initialized | `Event.Inventory.Initialized` |

#### 3.2 Регистрация сервиса

```cpp
// В GameInstance или PlayerState
ServiceLocator->RegisterServiceClass(
    FGameplayTag::RequestGameplayTag(TEXT("Service.Inventory")),
    USuspenseInventoryService::StaticClass(),
    InitParams
);
```

---

### Этап 4: UISystem

**Цель:** Подключить UI к новому EventBus

#### 4.1 События для миграции

| Событие | Новый тег |
|---------|-----------|
| Widget Created | `Event.UI.Widget.Created` |
| Widget Destroyed | `Event.UI.Widget.Destroyed` |
| Visibility Changed | `Event.UI.Widget.Visibility` |
| Screen Opened | `Event.UI.Screen.Opened` |
| Screen Closed | `Event.UI.Screen.Closed` |
| Tab Selected | `Event.UI.Tab.Selected` |
| Tooltip Requested | `Event.UI.Tooltip.Show` |
| Tooltip Hidden | `Event.UI.Tooltip.Hide` |

#### 4.2 RAII Subscriptions в виджетах

```cpp
class USuspenseBaseWidget : public UUserWidget
{
protected:
    // RAII - автоотписка при уничтожении виджета
    FEventSubscriptionScope EventScope;

    virtual void NativeConstruct() override
    {
        Super::NativeConstruct();

        // Подписка с автоматическим cleanup
        EventScope.Subscribe(
            FGameplayTag::RequestGameplayTag(TEXT("Event.GAS.Attribute.Health")),
            FEventHandlerDelegate::CreateUObject(this, &USuspenseBaseWidget::OnHealthEvent),
            EEventPriority::Normal
        );
    }

    // Деструктор EventScope автоматически отпишется
};
```

---

### Этап 5: EquipmentSystem

**Цель:** Полная интеграция EquipmentSystem с новой инфраструктурой

#### 5.1 События для миграции

| Событие | Новый тег |
|---------|-----------|
| Equipment Updated | `Event.Equipment.Updated` |
| Slot Changed | `Event.Equipment.Slot.Changed` |
| Weapon Changed | `Event.Equipment.Weapon.Active` |
| Weapon Fired | `Event.Weapon.Fired` |
| Weapon Reload Start | `Event.Weapon.Reload.Start` |
| Weapon Reload End | `Event.Weapon.Reload.End` |
| Ammo Changed | `Event.Weapon.Ammo.Changed` |
| Fire Mode Changed | `Event.Weapon.FireMode.Changed` |

#### 5.2 Service Registration

```cpp
// Регистрация всех equipment сервисов
ServiceLocator->RegisterServiceClass(TAG("Service.Equipment.Data"), UEquipmentDataService::StaticClass(), Params);
ServiceLocator->RegisterServiceClass(TAG("Service.Equipment.Operations"), UEquipmentOperationService::StaticClass(), Params);
ServiceLocator->RegisterServiceClass(TAG("Service.Equipment.Validation"), UEquipmentValidationService::StaticClass(), Params);
```

---

### Этап 6: Cleanup & Deprecation

**Цель:** Удаление legacy кода

#### 6.1 Deprecation Timeline

```
Версия 2.1: Добавить DEPRECATED_* макросы на старые методы EventManager
Версия 2.2: Логирование использования deprecated API
Версия 2.3: Compiler warnings
Версия 3.0: Удаление USuspenseEventManager
```

#### 6.2 Final Cleanup

- [ ] Удалить USuspenseEventManager
- [ ] Удалить двойные делегаты
- [ ] Удалить EventAdapter
- [ ] Обновить документацию
- [ ] Обновить примеры кода

---

## Часть 5: Метрики успеха

### 5.1 Performance KPIs

| Метрика | До миграции | Цель |
|---------|-------------|------|
| Event dispatch latency | ~0.5ms | < 0.2ms |
| Memory per event system | ~2KB | < 500B |
| Compile time (incremental) | ~15s | < 5s |
| Subscriber count per event | unlimited | limited (128) |

### 5.2 Code Quality KPIs

| Метрика | До | Цель |
|---------|-----|------|
| EventManager LOC | 1060 | 0 (deleted) |
| Coupling degree | High | Low |
| Test coverage | ? | > 80% |
| Circular dependencies | Some | 0 |

---

## Часть 6: Риски и митигация

| Риск | Вероятность | Impact | Митигация |
|------|-------------|--------|-----------|
| Регрессии в UI | High | Medium | Parallel running, feature flags |
| Performance degradation | Medium | High | Profiling на каждом этапе |
| Merge conflicts | Medium | Low | Работа в feature branches |
| Неполная миграция | Medium | Medium | Чеклисты, автотесты |
| Blueprint breakage | Low | High | Deprecated warnings заранее |

---

## Заключение

Переход на Domain EventBus + ServiceLocator — правильный архитектурный выбор для MMO шутера. Это обеспечит:

1. **Масштабируемость** — изолированные домены событий
2. **Performance** — приоритеты, фильтрация, очереди
3. **Maintainability** — SRP, loose coupling
4. **Testability** — изолированное тестирование модулей
5. **Developer Experience** — быстрая компиляция, меньше конфликтов

Рекомендуемый срок миграции: 5-7 итераций (по одному модулю за итерацию).

---

**Дата создания:** 2025-11-28
**Версия документа:** 1.0
