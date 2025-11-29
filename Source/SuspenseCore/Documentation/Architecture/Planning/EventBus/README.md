# EventBus System — Полная спецификация

> **Модуль:** BridgeSystem
> **Класс:** USuspenseCoreEventBus
> **Статус:** Спецификация

---

## 1. Обзор системы

EventBus — центральная шина событий проекта. **Все модули** общаются друг с другом **только через EventBus**.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         EVENT BUS ARCHITECTURE                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│    ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐               │
│    │ PlayerCore│   │   GAS    │   │Inventory │   │ UISystem │               │
│    └─────┬────┘   └─────┬────┘   └─────┬────┘   └─────┬────┘               │
│          │              │              │              │                      │
│          │   Publish    │   Publish    │  Subscribe   │  Subscribe          │
│          ▼              ▼              ▼              ▼                      │
│    ╔═════════════════════════════════════════════════════════════════╗      │
│    ║                    USuspenseCoreEventBus                        ║      │
│    ║  ═══════════════════════════════════════════════════════════    ║      │
│    ║                                                                 ║      │
│    ║   Events Queue     Subscriptions Map     Deferred Queue         ║      │
│    ║   ┌─────────┐      ┌───────────────┐     ┌─────────────┐       ║      │
│    ║   │ Event 1 │      │ Tag1 → [A,B]  │     │ Deferred 1  │       ║      │
│    ║   │ Event 2 │      │ Tag2 → [C]    │     │ Deferred 2  │       ║      │
│    ║   │ Event 3 │      │ Tag3 → [D,E,F]│     │ Deferred 3  │       ║      │
│    ║   └─────────┘      └───────────────┘     └─────────────┘       ║      │
│    ║                                                                 ║      │
│    ╚═════════════════════════════════════════════════════════════════╝      │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Ключевые концепции

### 2.1 GameplayTags для событий

Вместо строковых идентификаторов используем **GameplayTags**:

```cpp
// Плохо ❌
EventBus->Publish("PlayerDied", Data);

// Хорошо ✅
EventBus->Publish(
    FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Player.Died")),
    Data
);
```

**Преимущества:**
- Типобезопасность (опечатки ловятся на этапе компиляции)
- Иерархия (можно подписаться на `Player.*`)
- Инструменты UE (визуальный редактор тегов)
- Производительность (сравнение по хешу)

### 2.2 Приоритеты обработки

События обрабатываются в порядке приоритета:

| Приоритет | Значение | Использование |
|-----------|----------|---------------|
| System | 0 | Критические системные события |
| High | 50 | GAS, боевая система |
| Normal | 100 | Большинство событий (по умолчанию) |
| Low | 150 | UI обновления |
| Lowest | 200 | Логирование, аналитика |

```cpp
// Подписка с высоким приоритетом
EventBus->Subscribe(
    EventTag,
    Callback,
    ESuspenseCoreEventPriority::High
);
```

### 2.3 Фильтрация по источнику

Можно подписаться только на события от конкретного объекта:

```cpp
// Подписка только на события от MyCharacter
EventBus->SubscribeWithFilter(
    FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.GAS.Attribute.Health")),
    Callback,
    MyCharacter  // Фильтр
);
```

### 2.4 Отложенные события (Deferred)

Для событий, которые не должны прерывать текущий код:

```cpp
// Обработается в конце кадра
EventBus->PublishDeferred(EventTag, Data);
```

---

## 3. API Reference

### 3.1 Публикация событий

```cpp
/**
 * Публикует событие немедленно
 * @param EventTag - GameplayTag события
 * @param EventData - Данные события
 */
void Publish(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

/**
 * Публикует событие отложенно (в конце кадра)
 */
void PublishDeferred(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

/**
 * Публикует событие с указанным приоритетом
 */
void PublishWithPriority(
    FGameplayTag EventTag,
    const FSuspenseCoreEventData& EventData,
    ESuspenseCoreEventPriority Priority
);
```

### 3.2 Подписка на события

```cpp
/**
 * Базовая подписка
 * @return Handle для отписки
 */
FSuspenseCoreSubscriptionHandle Subscribe(
    FGameplayTag EventTag,
    const FSuspenseCoreEventCallback& Callback
);

/**
 * Подписка на все дочерние теги
 * Например: "Player" подпишет на "Player.Spawned", "Player.Died", etc.
 */
FSuspenseCoreSubscriptionHandle SubscribeToChildren(
    FGameplayTag ParentTag,
    const FSuspenseCoreEventCallback& Callback
);

/**
 * Подписка с фильтром по источнику
 */
FSuspenseCoreSubscriptionHandle SubscribeWithFilter(
    FGameplayTag EventTag,
    const FSuspenseCoreEventCallback& Callback,
    UObject* SourceFilter
);

/**
 * Подписка с приоритетом
 */
FSuspenseCoreSubscriptionHandle SubscribeWithPriority(
    FGameplayTag EventTag,
    const FSuspenseCoreEventCallback& Callback,
    ESuspenseCoreEventPriority Priority
);
```

### 3.3 Отписка

```cpp
/**
 * Отписка по handle
 */
void Unsubscribe(FSuspenseCoreSubscriptionHandle Handle);

/**
 * Отписка всех подписок объекта
 */
void UnsubscribeAll(UObject* Subscriber);
```

---

## 4. Иерархия GameplayTags

### 4.1 Полная структура тегов

```
SuspenseCore.Event
├── System
│   ├── Initialized
│   ├── Shutdown
│   └── Error
│
├── Player
│   ├── Spawned
│   ├── Died
│   ├── Respawned
│   ├── StateChanged
│   └── Controller
│       ├── Connected
│       └── Disconnected
│
├── GAS
│   ├── Ability
│   │   ├── Activated
│   │   ├── Ended
│   │   └── Cancelled
│   ├── Attribute
│   │   ├── Changed
│   │   ├── Health
│   │   ├── MaxHealth
│   │   ├── Stamina
│   │   └── Ammo
│   └── Effect
│       ├── Applied
│       └── Removed
│
├── Weapon
│   ├── Equipped
│   ├── Unequipped
│   ├── Fired
│   ├── Reloaded
│   └── AmmoChanged
│
├── Inventory
│   ├── ItemAdded
│   ├── ItemRemoved
│   ├── ItemMoved
│   └── SlotChanged
│
├── Equipment
│   ├── SlotChanged
│   └── LoadoutChanged
│
├── Interaction
│   ├── Started
│   ├── Completed
│   ├── Cancelled
│   ├── Available
│   └── Unavailable
│
└── UI
    ├── WidgetOpened
    ├── WidgetClosed
    ├── Notification
    └── HUD
        └── Update
```

### 4.2 Макросы для тегов

**Файл:** `BridgeSystem/Public/Core/SuspenseCoreEventTags.h`

```cpp
#pragma once

#include "GameplayTagContainer.h"

// ═══════════════════════════════════════════════════════════════════════════
// SUSPENSE CORE EVENT TAG MACROS
// ═══════════════════════════════════════════════════════════════════════════

/** Получить тег события по пути */
#define SUSPENSE_CORE_EVENT_TAG(Path) \
    FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event." #Path)))

/** Получить тег события (с кэшированием) */
#define SUSPENSE_CORE_EVENT_TAG_STATIC(Path) \
    []() -> FGameplayTag { \
        static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event." #Path))); \
        return Tag; \
    }()

// ───────────────────────────────────────────────────────────────────────────
// PLAYER EVENTS
// ───────────────────────────────────────────────────────────────────────────
#define SUSPENSE_CORE_EVENT_PLAYER_SPAWNED      SUSPENSE_CORE_EVENT_TAG_STATIC(Player.Spawned)
#define SUSPENSE_CORE_EVENT_PLAYER_DIED         SUSPENSE_CORE_EVENT_TAG_STATIC(Player.Died)
#define SUSPENSE_CORE_EVENT_PLAYER_RESPAWNED    SUSPENSE_CORE_EVENT_TAG_STATIC(Player.Respawned)
#define SUSPENSE_CORE_EVENT_PLAYER_STATE        SUSPENSE_CORE_EVENT_TAG_STATIC(Player.StateChanged)

// ───────────────────────────────────────────────────────────────────────────
// GAS EVENTS
// ───────────────────────────────────────────────────────────────────────────
#define SUSPENSE_CORE_EVENT_GAS_ABILITY_ACTIVATED   SUSPENSE_CORE_EVENT_TAG_STATIC(GAS.Ability.Activated)
#define SUSPENSE_CORE_EVENT_GAS_ABILITY_ENDED       SUSPENSE_CORE_EVENT_TAG_STATIC(GAS.Ability.Ended)
#define SUSPENSE_CORE_EVENT_GAS_ATTRIBUTE_CHANGED   SUSPENSE_CORE_EVENT_TAG_STATIC(GAS.Attribute.Changed)
#define SUSPENSE_CORE_EVENT_GAS_ATTRIBUTE_HEALTH    SUSPENSE_CORE_EVENT_TAG_STATIC(GAS.Attribute.Health)
#define SUSPENSE_CORE_EVENT_GAS_EFFECT_APPLIED      SUSPENSE_CORE_EVENT_TAG_STATIC(GAS.Effect.Applied)
#define SUSPENSE_CORE_EVENT_GAS_EFFECT_REMOVED      SUSPENSE_CORE_EVENT_TAG_STATIC(GAS.Effect.Removed)

// ───────────────────────────────────────────────────────────────────────────
// WEAPON EVENTS
// ───────────────────────────────────────────────────────────────────────────
#define SUSPENSE_CORE_EVENT_WEAPON_EQUIPPED     SUSPENSE_CORE_EVENT_TAG_STATIC(Weapon.Equipped)
#define SUSPENSE_CORE_EVENT_WEAPON_UNEQUIPPED   SUSPENSE_CORE_EVENT_TAG_STATIC(Weapon.Unequipped)
#define SUSPENSE_CORE_EVENT_WEAPON_FIRED        SUSPENSE_CORE_EVENT_TAG_STATIC(Weapon.Fired)
#define SUSPENSE_CORE_EVENT_WEAPON_RELOADED     SUSPENSE_CORE_EVENT_TAG_STATIC(Weapon.Reloaded)
#define SUSPENSE_CORE_EVENT_WEAPON_AMMO         SUSPENSE_CORE_EVENT_TAG_STATIC(Weapon.AmmoChanged)

// ───────────────────────────────────────────────────────────────────────────
// INVENTORY EVENTS
// ───────────────────────────────────────────────────────────────────────────
#define SUSPENSE_CORE_EVENT_INVENTORY_ITEM_ADDED    SUSPENSE_CORE_EVENT_TAG_STATIC(Inventory.ItemAdded)
#define SUSPENSE_CORE_EVENT_INVENTORY_ITEM_REMOVED  SUSPENSE_CORE_EVENT_TAG_STATIC(Inventory.ItemRemoved)
#define SUSPENSE_CORE_EVENT_INVENTORY_SLOT_CHANGED  SUSPENSE_CORE_EVENT_TAG_STATIC(Inventory.SlotChanged)

// ───────────────────────────────────────────────────────────────────────────
// INTERACTION EVENTS
// ───────────────────────────────────────────────────────────────────────────
#define SUSPENSE_CORE_EVENT_INTERACTION_STARTED     SUSPENSE_CORE_EVENT_TAG_STATIC(Interaction.Started)
#define SUSPENSE_CORE_EVENT_INTERACTION_COMPLETED   SUSPENSE_CORE_EVENT_TAG_STATIC(Interaction.Completed)
#define SUSPENSE_CORE_EVENT_INTERACTION_CANCELLED   SUSPENSE_CORE_EVENT_TAG_STATIC(Interaction.Cancelled)

// ───────────────────────────────────────────────────────────────────────────
// UI EVENTS
// ───────────────────────────────────────────────────────────────────────────
#define SUSPENSE_CORE_EVENT_UI_WIDGET_OPENED    SUSPENSE_CORE_EVENT_TAG_STATIC(UI.WidgetOpened)
#define SUSPENSE_CORE_EVENT_UI_WIDGET_CLOSED    SUSPENSE_CORE_EVENT_TAG_STATIC(UI.WidgetClosed)
#define SUSPENSE_CORE_EVENT_UI_NOTIFICATION     SUSPENSE_CORE_EVENT_TAG_STATIC(UI.Notification)
```

---

## 5. Паттерны использования

### 5.1 Паттерн: Публикация события

```cpp
void ASuspenseCoreCharacter::PublishDiedEvent(AActor* Killer)
{
    USuspenseCoreEventBus* EventBus = GetEventBus();
    if (!EventBus) return;

    // Создаём данные события
    FSuspenseCoreEventData Data = FSuspenseCoreEventData::Create(this);

    // Заполняем payload
    Data.SetFloat(TEXT("PlayerId"), GetPlayerId());
    Data.SetObject(TEXT("Killer"), Killer);
    Data.SetString(TEXT("DamageType"), LastDamageType.ToString());
    Data.Tags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Death.Combat")));

    // Публикуем
    EventBus->Publish(SUSPENSE_CORE_EVENT_PLAYER_DIED, Data);
}
```

### 5.2 Паттерн: Подписка в BeginPlay

```cpp
void UMyComponent::BeginPlay()
{
    Super::BeginPlay();

    if (auto* Manager = USuspenseCoreEventManager::Get(this))
    {
        USuspenseCoreEventBus* EventBus = Manager->GetEventBus();

        // Сохраняем handles для отписки
        Handles.Add(EventBus->Subscribe(
            SUSPENSE_CORE_EVENT_PLAYER_DIED,
            FSuspenseCoreEventCallback::CreateUObject(this, &UMyComponent::OnPlayerDied)
        ));

        Handles.Add(EventBus->Subscribe(
            SUSPENSE_CORE_EVENT_WEAPON_FIRED,
            FSuspenseCoreEventCallback::CreateUObject(this, &UMyComponent::OnWeaponFired)
        ));
    }
}

void UMyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (auto* Manager = USuspenseCoreEventManager::Get(this))
    {
        USuspenseCoreEventBus* EventBus = Manager->GetEventBus();

        for (auto& Handle : Handles)
        {
            EventBus->Unsubscribe(Handle);
        }
        Handles.Empty();
    }

    Super::EndPlay(EndPlayReason);
}
```

### 5.3 Паттерн: Реализация ISuspenseCoreEventSubscriber

```cpp
// Header
class UMyComponent : public UActorComponent, public ISuspenseCoreEventSubscriber
{
    // ...
    virtual void SetupEventSubscriptions(USuspenseCoreEventBus* EventBus) override;
    virtual void TeardownEventSubscriptions(USuspenseCoreEventBus* EventBus) override;
    virtual TArray<FSuspenseCoreSubscriptionHandle> GetSubscriptionHandles() const override;

protected:
    TArray<FSuspenseCoreSubscriptionHandle> SubscriptionHandles;
};

// Implementation
void UMyComponent::SetupEventSubscriptions(USuspenseCoreEventBus* EventBus)
{
    SubscriptionHandles.Add(EventBus->Subscribe(
        SUSPENSE_CORE_EVENT_PLAYER_SPAWNED,
        FSuspenseCoreEventCallback::CreateUObject(this, &UMyComponent::OnPlayerSpawned)
    ));
}

void UMyComponent::TeardownEventSubscriptions(USuspenseCoreEventBus* EventBus)
{
    for (auto& Handle : SubscriptionHandles)
    {
        EventBus->Unsubscribe(Handle);
    }
    SubscriptionHandles.Empty();
}

TArray<FSuspenseCoreSubscriptionHandle> UMyComponent::GetSubscriptionHandles() const
{
    return SubscriptionHandles;
}
```

### 5.4 Паттерн: Фильтрация в callback

```cpp
void UPlayerHUD::OnHealthChanged(FGameplayTag EventTag, const FSuspenseCoreEventData& Data)
{
    // Проверяем что это наш персонаж
    if (Data.Source != OwnerCharacter)
    {
        return;
    }

    // Обрабатываем
    float NewHealth = Data.GetFloat(TEXT("NewValue"));
    float MaxHealth = Data.GetFloat(TEXT("MaxValue"));

    UpdateHealthBar(NewHealth, MaxHealth);
}
```

### 5.5 Паттерн: Подписка на группу событий

```cpp
// Подписаться на ВСЕ события GAS
EventBus->SubscribeToChildren(
    FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.GAS")),
    FSuspenseCoreEventCallback::CreateUObject(this, &UAnalytics::OnAnyGASEvent)
);

void UAnalytics::OnAnyGASEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& Data)
{
    // Получаем конкретный тег
    FString TagString = EventTag.ToString();

    // Логируем
    UE_LOG(LogAnalytics, Log, TEXT("GAS Event: %s from %s"),
        *TagString,
        *GetNameSafe(Data.Source.Get())
    );
}
```

---

## 6. Thread Safety

### 6.1 Правила потокобезопасности

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         THREAD SAFETY RULES                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ БЕЗОПАСНО в любом потоке:                                                   │
│ ✅ Publish() - использует lock                                              │
│ ✅ PublishDeferred() - добавляет в очередь с lock                           │
│                                                                              │
│ ТОЛЬКО GameThread:                                                           │
│ ⚠️ Subscribe() - модификация подписок                                       │
│ ⚠️ Unsubscribe() - модификация подписок                                     │
│ ⚠️ ProcessDeferredEvents() - вызов callbacks                                │
│                                                                              │
│ CALLBACKS всегда в GameThread:                                               │
│ ✅ Все callbacks вызываются в GameThread                                     │
│ ✅ Deferred events обрабатываются в конце кадра                              │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 6.2 Реализация thread safety

```cpp
void USuspenseCoreEventBus::Publish(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
    // Thread-safe публикация
    FScopeLock Lock(&SubscriptionLock);

    // Находим подписчиков
    if (TArray<FSuspenseCoreSubscription>* Subs = Subscriptions.Find(EventTag))
    {
        // Сортируем по приоритету (один раз при изменении подписок)
        for (const FSuspenseCoreSubscription& Sub : *Subs)
        {
            if (Sub.IsValid())
            {
                // Если не в GameThread - откладываем
                if (!IsInGameThread())
                {
                    AsyncTask(ENamedThreads::GameThread, [=]()
                    {
                        Sub.Callback.ExecuteIfBound(EventTag, EventData);
                    });
                }
                else
                {
                    Sub.Callback.ExecuteIfBound(EventTag, EventData);
                }
            }
        }
    }
}
```

---

## 7. Performance

### 7.1 Оптимизации

| Оптимизация | Описание |
|-------------|----------|
| **Хеширование тегов** | GameplayTags используют хеши для быстрого сравнения |
| **Кэширование подписчиков** | Подписчики хранятся в TMap по тегу |
| **Lazy cleanup** | Невалидные подписки удаляются периодически, не при каждом событии |
| **Pre-sorted** | Подписчики сортируются по приоритету при добавлении |
| **Deferred batching** | Отложенные события обрабатываются пакетом |

### 7.2 Метрики для мониторинга

```cpp
FSuspenseCoreEventBusStats Stats = EventBus->GetStats();

UE_LOG(LogEventBus, Log, TEXT("EventBus Stats:"));
UE_LOG(LogEventBus, Log, TEXT("  Active Subscriptions: %d"), Stats.ActiveSubscriptions);
UE_LOG(LogEventBus, Log, TEXT("  Unique Event Tags: %d"), Stats.UniqueEventTags);
UE_LOG(LogEventBus, Log, TEXT("  Total Events Published: %lld"), Stats.TotalEventsPublished);
UE_LOG(LogEventBus, Log, TEXT("  Deferred Queue Size: %d"), Stats.DeferredEventsQueued);
```

---

## 8. Debugging

### 8.1 Логирование событий

```cpp
// Включить логирование всех событий (для отладки)
UCLASS()
class USuspenseCoreEventBusDebugger : public UObject
{
public:
    void EnableLogging(USuspenseCoreEventBus* EventBus)
    {
        // Подписываемся на корневой тег - получаем ВСЕ события
        EventBus->SubscribeToChildren(
            FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event")),
            FSuspenseCoreEventCallback::CreateUObject(this, &USuspenseCoreEventBusDebugger::LogEvent)
        );
    }

    void LogEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& Data)
    {
        UE_LOG(LogEventBus, Verbose, TEXT("[EVENT] %s | Source: %s | Time: %.3f"),
            *EventTag.ToString(),
            *GetNameSafe(Data.Source.Get()),
            Data.Timestamp
        );
    }
};
```

### 8.2 Console Commands

```cpp
// Регистрация консольных команд
static FAutoConsoleCommand CmdEventBusStats(
    TEXT("SuspenseCore.EventBus.Stats"),
    TEXT("Print EventBus statistics"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (GEngine && GEngine->GameViewport)
        {
            if (auto* Manager = USuspenseCoreEventManager::Get(GEngine->GameViewport->GetWorld()))
            {
                FSuspenseCoreEventBusStats Stats = Manager->GetEventBus()->GetStats();
                // Print stats...
            }
        }
    })
);
```

---

## 9. Best Practices

### 9.1 DO (Делать)

```
✅ Используйте макросы SUSPENSE_CORE_EVENT_* для тегов
✅ Всегда сохраняйте handles и отписывайтесь в EndPlay
✅ Проверяйте Source в callback если нужна фильтрация
✅ Используйте приоритеты для критичного кода
✅ Используйте Deferred для не-критичных обновлений
✅ Документируйте payload событий
```

### 9.2 DON'T (Не делать)

```
❌ Не используйте строковые теги напрямую
❌ Не забывайте отписываться
❌ Не выполняйте тяжёлые операции в callbacks
❌ Не модифицируйте Source объект в callback (может быть destroyed)
❌ Не создавайте циклические публикации
❌ Не полагайтесь на порядок вызова при одинаковом приоритете
```

---

*Документ создан: 2025-11-29*
*Модуль: BridgeSystem*
