# ЭТАП 1: BridgeSystem — Детальный план

> **Статус:** В РАБОТЕ 🔄
> **Приоритет:** P0 (КРИТИЧЕСКИЙ)
> **Зависимости:** Нет (это фундамент)

---

## 1. Обзор

BridgeSystem — это **фундамент всей архитектуры**. Все остальные модули зависят от него.
Создаём с нуля следующие классы:

| Класс | Описание |
|-------|----------|
| `USuspenseCoreEventBus` | Шина событий на GameplayTags |
| `USuspenseCoreServiceLocator` | DI контейнер для сервисов |
| `USuspenseCoreEventManager` | GameInstanceSubsystem — точка входа |
| `FSuspenseCoreEventData` | Структура данных события |
| `ESuspenseCoreEventPriority` | Приоритеты обработки |
| `ISuspenseCoreEventSubscriber` | Интерфейс подписчика |
| `ISuspenseCorePlayerRepository` | Интерфейс репозитория игрока |
| `USuspenseCoreFilePlayerRepository` | Файловая реализация БД (JSON/Binary) |
| `FSuspenseCorePlayerData` | Структура данных игрока |

---

## 1.1 PlayerDatabase (встроен в BridgeSystem)

```
╔══════════════════════════════════════════════════════════════════════════════╗
║                      PLAYER DATABASE ARCHITECTURE                             ║
╠══════════════════════════════════════════════════════════════════════════════╣
║                                                                              ║
║   Принцип: Repository Pattern — лёгкая миграция на любую БД                  ║
║                                                                              ║
║   ┌──────────────────────────────────────────────────────────────────────┐  ║
║   │              ISuspenseCorePlayerRepository (интерфейс)               │  ║
║   └────────────────────────────────┬─────────────────────────────────────┘  ║
║                                    │                                         ║
║          ┌─────────────────────────┼─────────────────────────────┐          ║
║          │                         │                             │          ║
║          ▼                         ▼                             ▼          ║
║   ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────────┐      ║
║   │ FileRepository  │   │ SQLRepository   │   │ CloudRepository     │      ║
║   │ (СЕЙЧАС)        │   │ (ПОТОМ)         │   │ (ПОТОМ)             │      ║
║   │                 │   │                 │   │                     │      ║
║   │ • JSON/Binary   │   │ • PostgreSQL    │   │ • PlayFab           │      ║
║   │ • Нет зависим.  │   │ • SQLite        │   │ • GameSparks        │      ║
║   │ • Dedicated OK  │   │                 │   │ • Custom Backend    │      ║
║   └─────────────────┘   └─────────────────┘   └─────────────────────┘      ║
║                                                                              ║
║   Миграция: Просто создать новую реализацию + сменить в конфиге             ║
║                                                                              ║
╚══════════════════════════════════════════════════════════════════════════════╝
```

### Данные игрока (FSuspenseCorePlayerData)

| Категория | Поля |
|-----------|------|
| **Идентификация** | PlayerId, DisplayName, AvatarId, CreatedAt, LastLoginAt |
| **Прогресс** | Level, ExperiencePoints, PrestigeLevel |
| **Статистика** | Kills, Deaths, Assists, DamageDealt, TimePlayed, MatchesPlayed |
| **Валюта** | SoftCurrency, HardCurrency |
| **Инвентарь** | UnlockedWeapons, UnlockedSkins, Items |
| **Loadouts** | Loadouts[], ActiveLoadoutIndex |
| **Настройки** | Sensitivity, FOV, KeyBindings, AudioSettings |
| **Достижения** | Achievements map с прогрессом |

---

## 2. Классы к созданию

### 2.1 USuspenseCoreEventBus

**Файл:** `BridgeSystem/Public/Core/SuspenseCoreEventBus.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreTypes.h"
#include "SuspenseCoreEventBus.generated.h"

/**
 * USuspenseCoreEventBus
 *
 * Центральная шина событий. Все модули общаются ТОЛЬКО через неё.
 *
 * Ключевые особенности:
 * - События идентифицируются через GameplayTags
 * - Поддержка приоритетов обработки
 * - Фильтрация по источнику события
 * - Thread-safe операции
 * - Отложенные события (deferred)
 */
UCLASS(BlueprintType)
class BRIDGESYSTEM_API USuspenseCoreEventBus : public UObject
{
    GENERATED_BODY()

public:
    USuspenseCoreEventBus();

    // ═══════════════════════════════════════════════════════════════════════
    // ПУБЛИКАЦИЯ СОБЫТИЙ
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Публикует событие немедленно
     * @param EventTag - тег события (например Suspense.Event.Player.Spawned)
     * @param EventData - данные события
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
    void Publish(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

    /**
     * Публикует событие отложенно (в конце кадра)
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
    void PublishDeferred(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

    /**
     * Публикует событие с приоритетом
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
    void PublishWithPriority(
        FGameplayTag EventTag,
        const FSuspenseCoreEventData& EventData,
        ESuspenseCoreEventPriority Priority
    );

    // ═══════════════════════════════════════════════════════════════════════
    // ПОДПИСКА НА СОБЫТИЯ
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Подписка на событие
     * @param EventTag - тег события для подписки
     * @param Callback - функция обратного вызова
     * @return Handle для отписки
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
    FSuspenseCoreSubscriptionHandle Subscribe(
        FGameplayTag EventTag,
        const FSuspenseCoreEventCallback& Callback
    );

    /**
     * Подписка на группу событий (по родительскому тегу)
     * Например: Suspense.Event.Player подпишет на все Player.* события
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
    FSuspenseCoreSubscriptionHandle SubscribeToChildren(
        FGameplayTag ParentTag,
        const FSuspenseCoreEventCallback& Callback
    );

    /**
     * Подписка с фильтром по источнику
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
    FSuspenseCoreSubscriptionHandle SubscribeWithFilter(
        FGameplayTag EventTag,
        const FSuspenseCoreEventCallback& Callback,
        UObject* SourceFilter
    );

    /**
     * Отписка по handle
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
    void Unsubscribe(FSuspenseCoreSubscriptionHandle Handle);

    /**
     * Отписка всех подписок объекта
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
    void UnsubscribeAll(UObject* Subscriber);

    // ═══════════════════════════════════════════════════════════════════════
    // УТИЛИТЫ
    // ═══════════════════════════════════════════════════════════════════════

    /** Обработка отложенных событий (вызывается EventManager) */
    void ProcessDeferredEvents();

    /** Очистка невалидных подписок */
    void CleanupStaleSubscriptions();

    /** Статистика для отладки */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events|Debug")
    FSuspenseCoreEventBusStats GetStats() const;

protected:
    /** Карта подписок: Tag -> Array of Subscribers */
    TMap<FGameplayTag, TArray<FSuspenseCoreSubscription>> Subscriptions;

    /** Очередь отложенных событий */
    TArray<FSuspenseCoreQueuedEvent> DeferredEvents;

    /** Счётчик для генерации уникальных handle */
    uint64 NextSubscriptionId;

    /** Критическая секция для thread-safety */
    mutable FCriticalSection SubscriptionLock;

private:
    /** Внутренний метод публикации */
    void PublishInternal(
        FGameplayTag EventTag,
        const FSuspenseCoreEventData& EventData,
        ESuspenseCoreEventPriority Priority
    );

    /** Сортировка подписчиков по приоритету */
    void SortSubscriptionsByPriority(TArray<FSuspenseCoreSubscription>& Subs);
};
```

### 2.2 USuspenseCoreServiceLocator

**Файл:** `BridgeSystem/Public/Core/SuspenseCoreServiceLocator.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "SuspenseCoreServiceLocator.generated.h"

/**
 * USuspenseCoreServiceLocator
 *
 * Простой Service Locator для Dependency Injection.
 * Позволяет регистрировать и получать сервисы по типу.
 *
 * Пример использования:
 *   ServiceLocator->RegisterService<IInventoryService>(MyInventoryService);
 *   auto* Service = ServiceLocator->GetService<IInventoryService>();
 */
UCLASS(BlueprintType)
class BRIDGESYSTEM_API USuspenseCoreServiceLocator : public UObject
{
    GENERATED_BODY()

public:
    USuspenseCoreServiceLocator();

    // ═══════════════════════════════════════════════════════════════════════
    // РЕГИСТРАЦИЯ СЕРВИСОВ
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Регистрирует сервис по имени класса
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Services")
    void RegisterServiceByName(FName ServiceName, UObject* ServiceInstance);

    /**
     * Регистрирует сервис (C++ template версия)
     */
    template<typename T>
    void RegisterService(T* ServiceInstance)
    {
        RegisterServiceByName(T::StaticClass()->GetFName(), ServiceInstance);
    }

    /**
     * Регистрирует сервис с интерфейсом (C++ template версия)
     */
    template<typename InterfaceType, typename ImplementationType>
    void RegisterServiceAs(ImplementationType* ServiceInstance)
    {
        static_assert(TIsDerivedFrom<ImplementationType, InterfaceType>::Value,
            "Implementation must derive from Interface");
        RegisterServiceByName(InterfaceType::UClassType::StaticClass()->GetFName(), ServiceInstance);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // ПОЛУЧЕНИЕ СЕРВИСОВ
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Получает сервис по имени
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Services")
    UObject* GetServiceByName(FName ServiceName) const;

    /**
     * Получает сервис (C++ template версия)
     */
    template<typename T>
    T* GetService() const
    {
        return Cast<T>(GetServiceByName(T::StaticClass()->GetFName()));
    }

    /**
     * Проверяет наличие сервиса
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Services")
    bool HasService(FName ServiceName) const;

    // ═══════════════════════════════════════════════════════════════════════
    // УПРАВЛЕНИЕ
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Удаляет сервис
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Services")
    void UnregisterService(FName ServiceName);

    /**
     * Очищает все сервисы
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Services")
    void ClearAllServices();

    /**
     * Получает список зарегистрированных сервисов (для отладки)
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Services|Debug")
    TArray<FName> GetRegisteredServiceNames() const;

protected:
    /** Карта сервисов: ServiceName -> Instance */
    UPROPERTY()
    TMap<FName, TObjectPtr<UObject>> Services;

    /** Критическая секция для thread-safety */
    mutable FCriticalSection ServiceLock;
};
```

### 2.3 USuspenseCoreEventManager

**Файл:** `BridgeSystem/Public/Core/SuspenseCoreEventManager.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SuspenseCoreEventBus.h"
#include "SuspenseCoreServiceLocator.h"
#include "SuspenseCoreEventManager.generated.h"

/**
 * USuspenseCoreEventManager
 *
 * Главный менеджер — точка входа в систему событий.
 * GameInstanceSubsystem — живёт всё время игры.
 *
 * Использование:
 *   auto* Manager = USuspenseCoreEventManager::Get(WorldContext);
 *   Manager->GetEventBus()->Publish(...);
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreEventManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ═══════════════════════════════════════════════════════════════════════
    // USubsystem Interface
    // ═══════════════════════════════════════════════════════════════════════

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

    // ═══════════════════════════════════════════════════════════════════════
    // СТАТИЧЕСКИЙ ДОСТУП
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Получить EventManager из любого места
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore", meta = (WorldContext = "WorldContextObject"))
    static USuspenseCoreEventManager* Get(const UObject* WorldContextObject);

    // ═══════════════════════════════════════════════════════════════════════
    // ДОСТУП К ПОДСИСТЕМАМ
    // ═══════════════════════════════════════════════════════════════════════

    /** EventBus для публикации/подписки */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
    USuspenseCoreEventBus* GetEventBus() const { return EventBus; }

    /** ServiceLocator для DI */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Services")
    USuspenseCoreServiceLocator* GetServiceLocator() const { return ServiceLocator; }

    // ═══════════════════════════════════════════════════════════════════════
    // БЫСТРЫЕ ХЕЛПЕРЫ
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Быстрая публикация события
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
    void PublishEvent(
        FGameplayTag EventTag,
        UObject* Source,
        const TMap<FName, FString>& Payload
    );

    /**
     * Быстрая подписка
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
    FSuspenseCoreSubscriptionHandle SubscribeToEvent(
        FGameplayTag EventTag,
        const FSuspenseCoreEventCallback& Callback
    );

protected:
    /** EventBus instance */
    UPROPERTY()
    TObjectPtr<USuspenseCoreEventBus> EventBus;

    /** ServiceLocator instance */
    UPROPERTY()
    TObjectPtr<USuspenseCoreServiceLocator> ServiceLocator;

    /** Таймер для обработки отложенных событий */
    FTimerHandle DeferredEventsTimerHandle;

private:
    /** Создание подсистем */
    void CreateSubsystems();

    /** Регистрация базовых сервисов */
    void RegisterCoreServices();

    /** Callback для обработки отложенных событий */
    void OnProcessDeferredEvents();
};
```

---

## 3. Типы данных (SuspenseCoreTypes.h)

**Файл:** `BridgeSystem/Public/Core/SuspenseCoreTypes.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreTypes.generated.h"

// ═══════════════════════════════════════════════════════════════════════════
// ENUMS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Приоритет обработки события
 */
UENUM(BlueprintType)
enum class ESuspenseCoreEventPriority : uint8
{
    /** Системные события (обрабатываются первыми) */
    System = 0      UMETA(DisplayName = "System"),

    /** Высокий приоритет (GAS, критическая логика) */
    High = 50       UMETA(DisplayName = "High"),

    /** Нормальный приоритет (большинство событий) */
    Normal = 100    UMETA(DisplayName = "Normal"),

    /** Низкий приоритет (UI, логирование) */
    Low = 150       UMETA(DisplayName = "Low"),

    /** Самый низкий (аналитика, отладка) */
    Lowest = 200    UMETA(DisplayName = "Lowest")
};

/**
 * Контекст выполнения события
 */
UENUM(BlueprintType)
enum class ESuspenseCoreEventContext : uint8
{
    /** Выполнить в GameThread */
    GameThread      UMETA(DisplayName = "Game Thread"),

    /** Выполнить в любом потоке */
    AnyThread       UMETA(DisplayName = "Any Thread"),

    /** Отложить до конца кадра */
    Deferred        UMETA(DisplayName = "Deferred")
};

// ═══════════════════════════════════════════════════════════════════════════
// STRUCTS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Handle для управления подпиской
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreSubscriptionHandle
{
    GENERATED_BODY()

    FSuspenseCoreSubscriptionHandle() : Id(0) {}
    explicit FSuspenseCoreSubscriptionHandle(uint64 InId) : Id(InId) {}

    bool IsValid() const { return Id != 0; }
    void Invalidate() { Id = 0; }

    bool operator==(const FSuspenseCoreSubscriptionHandle& Other) const { return Id == Other.Id; }
    bool operator!=(const FSuspenseCoreSubscriptionHandle& Other) const { return Id != Other.Id; }

    friend uint32 GetTypeHash(const FSuspenseCoreSubscriptionHandle& Handle)
    {
        return GetTypeHash(Handle.Id);
    }

private:
    UPROPERTY()
    uint64 Id;
};

/**
 * Данные события
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreEventData
{
    GENERATED_BODY()

    FSuspenseCoreEventData()
        : Source(nullptr)
        , Timestamp(0.0)
        , Priority(ESuspenseCoreEventPriority::Normal)
    {}

    /** Источник события */
    UPROPERTY(BlueprintReadWrite, Category = "Event")
    TObjectPtr<UObject> Source;

    /** Временная метка */
    UPROPERTY(BlueprintReadOnly, Category = "Event")
    double Timestamp;

    /** Приоритет */
    UPROPERTY(BlueprintReadWrite, Category = "Event")
    ESuspenseCoreEventPriority Priority;

    /** Произвольные данные (строковые) */
    UPROPERTY(BlueprintReadWrite, Category = "Event")
    TMap<FName, FString> StringPayload;

    /** Произвольные данные (числовые) */
    UPROPERTY(BlueprintReadWrite, Category = "Event")
    TMap<FName, float> FloatPayload;

    /** Произвольные данные (объекты) */
    UPROPERTY(BlueprintReadWrite, Category = "Event")
    TMap<FName, TObjectPtr<UObject>> ObjectPayload;

    /** GameplayTags для фильтрации */
    UPROPERTY(BlueprintReadWrite, Category = "Event")
    FGameplayTagContainer Tags;

    // ─────────────────────────────────────────────────────────────────────
    // ХЕЛПЕРЫ
    // ─────────────────────────────────────────────────────────────────────

    /** Получить строковое значение */
    FString GetString(FName Key, const FString& Default = TEXT("")) const
    {
        const FString* Value = StringPayload.Find(Key);
        return Value ? *Value : Default;
    }

    /** Получить float значение */
    float GetFloat(FName Key, float Default = 0.0f) const
    {
        const float* Value = FloatPayload.Find(Key);
        return Value ? *Value : Default;
    }

    /** Получить объект */
    template<typename T>
    T* GetObject(FName Key) const
    {
        const TObjectPtr<UObject>* Value = ObjectPayload.Find(Key);
        return Value ? Cast<T>(Value->Get()) : nullptr;
    }

    /** Установить строковое значение */
    FSuspenseCoreEventData& SetString(FName Key, const FString& Value)
    {
        StringPayload.Add(Key, Value);
        return *this;
    }

    /** Установить float значение */
    FSuspenseCoreEventData& SetFloat(FName Key, float Value)
    {
        FloatPayload.Add(Key, Value);
        return *this;
    }

    /** Установить объект */
    FSuspenseCoreEventData& SetObject(FName Key, UObject* Value)
    {
        ObjectPayload.Add(Key, Value);
        return *this;
    }

    /** Статический фабричный метод */
    static FSuspenseCoreEventData Create(UObject* InSource)
    {
        FSuspenseCoreEventData Data;
        Data.Source = InSource;
        Data.Timestamp = FPlatformTime::Seconds();
        return Data;
    }
};

/**
 * Внутренняя структура подписки
 */
USTRUCT()
struct FSuspenseCoreSubscription
{
    GENERATED_BODY()

    /** Уникальный ID подписки */
    UPROPERTY()
    uint64 Id = 0;

    /** Слабая ссылка на подписчика */
    TWeakObjectPtr<UObject> Subscriber;

    /** Приоритет обработки */
    ESuspenseCoreEventPriority Priority = ESuspenseCoreEventPriority::Normal;

    /** Фильтр по источнику (опционально) */
    TWeakObjectPtr<UObject> SourceFilter;

    /** Callback (не UPROPERTY - храним как делегат) */
    // FSuspenseCoreEventCallback Callback; // Определяется отдельно

    bool IsValid() const
    {
        return Id != 0 && Subscriber.IsValid();
    }
};

/**
 * Событие в очереди отложенных
 */
USTRUCT()
struct FSuspenseCoreQueuedEvent
{
    GENERATED_BODY()

    UPROPERTY()
    FGameplayTag EventTag;

    UPROPERTY()
    FSuspenseCoreEventData EventData;

    UPROPERTY()
    ESuspenseCoreEventPriority Priority = ESuspenseCoreEventPriority::Normal;
};

/**
 * Статистика EventBus (для отладки)
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreEventBusStats
{
    GENERATED_BODY()

    /** Количество активных подписок */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 ActiveSubscriptions = 0;

    /** Количество уникальных тегов */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 UniqueEventTags = 0;

    /** Событий опубликовано за сессию */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int64 TotalEventsPublished = 0;

    /** Событий в очереди отложенных */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 DeferredEventsQueued = 0;
};

// ═══════════════════════════════════════════════════════════════════════════
// ДЕЛЕГАТЫ
// ═══════════════════════════════════════════════════════════════════════════

/** Callback при получении события */
DECLARE_DYNAMIC_DELEGATE_TwoParams(
    FSuspenseCoreEventCallback,
    FGameplayTag, EventTag,
    const FSuspenseCoreEventData&, EventData
);

/** Мультикаст делегат для C++ */
DECLARE_MULTICAST_DELEGATE_TwoParams(
    FSuspenseCoreEventMulticast,
    FGameplayTag /*EventTag*/,
    const FSuspenseCoreEventData& /*EventData*/
);
```

---

## 4. Интерфейсы (SuspenseCoreInterfaces.h)

**Файл:** `BridgeSystem/Public/Core/SuspenseCoreInterfaces.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SuspenseCoreTypes.h"
#include "SuspenseCoreInterfaces.generated.h"

// ═══════════════════════════════════════════════════════════════════════════
// ISuspenseCoreEventSubscriber
// ═══════════════════════════════════════════════════════════════════════════

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreEventSubscriber : public UInterface
{
    GENERATED_BODY()
};

/**
 * ISuspenseCoreEventSubscriber
 *
 * Интерфейс для объектов, которые подписываются на события.
 * Опционален — можно подписываться и без него.
 * Но полезен для автоматической отписки при уничтожении.
 */
class BRIDGESYSTEM_API ISuspenseCoreEventSubscriber
{
    GENERATED_BODY()

public:
    /**
     * Вызывается при инициализации — здесь настраиваем подписки
     */
    virtual void SetupEventSubscriptions(USuspenseCoreEventBus* EventBus) = 0;

    /**
     * Вызывается при деинициализации — отписываемся
     */
    virtual void TeardownEventSubscriptions(USuspenseCoreEventBus* EventBus) = 0;

    /**
     * Получить все активные handles подписок
     */
    virtual TArray<FSuspenseCoreSubscriptionHandle> GetSubscriptionHandles() const = 0;
};

// ═══════════════════════════════════════════════════════════════════════════
// ISuspenseCoreService
// ═══════════════════════════════════════════════════════════════════════════

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreService : public UInterface
{
    GENERATED_BODY()
};

/**
 * ISuspenseCoreService
 *
 * Базовый интерфейс для сервисов, регистрируемых в ServiceLocator.
 */
class BRIDGESYSTEM_API ISuspenseCoreService
{
    GENERATED_BODY()

public:
    /**
     * Инициализация сервиса
     */
    virtual void InitializeService() = 0;

    /**
     * Деинициализация сервиса
     */
    virtual void ShutdownService() = 0;

    /**
     * Получить имя сервиса для отладки
     */
    virtual FName GetServiceName() const = 0;
};

// ═══════════════════════════════════════════════════════════════════════════
// ISuspenseCoreEventEmitter
// ═══════════════════════════════════════════════════════════════════════════

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreEventEmitter : public UInterface
{
    GENERATED_BODY()
};

/**
 * ISuspenseCoreEventEmitter
 *
 * Интерфейс для объектов, публикующих события.
 * Помогает стандартизировать способ публикации.
 */
class BRIDGESYSTEM_API ISuspenseCoreEventEmitter
{
    GENERATED_BODY()

public:
    /**
     * Публикует событие через EventBus
     */
    virtual void EmitEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& Data) = 0;

    /**
     * Получить EventBus (для внутреннего использования)
     */
    virtual USuspenseCoreEventBus* GetEventBus() const = 0;
};
```

---

## 5. GameplayTags иерархия

**Файл:** `Config/DefaultGameplayTags.ini` (добавить)

```ini
; ═══════════════════════════════════════════════════════════════════════════
; SUSPENSECORE EVENT TAGS
; ═══════════════════════════════════════════════════════════════════════════

; Корневой тег
+GameplayTags=(Tag="SuspenseCore.Event")

; ───────────────────────────────────────────────────────────────────────────
; SYSTEM EVENTS
; ───────────────────────────────────────────────────────────────────────────
+GameplayTags=(Tag="SuspenseCore.Event.System")
+GameplayTags=(Tag="SuspenseCore.Event.System.Initialized")
+GameplayTags=(Tag="SuspenseCore.Event.System.Shutdown")

; ───────────────────────────────────────────────────────────────────────────
; PLAYER EVENTS
; ───────────────────────────────────────────────────────────────────────────
+GameplayTags=(Tag="SuspenseCore.Event.Player")
+GameplayTags=(Tag="SuspenseCore.Event.Player.Spawned")
+GameplayTags=(Tag="SuspenseCore.Event.Player.Died")
+GameplayTags=(Tag="SuspenseCore.Event.Player.Respawned")
+GameplayTags=(Tag="SuspenseCore.Event.Player.StateChanged")

; ───────────────────────────────────────────────────────────────────────────
; GAS EVENTS
; ───────────────────────────────────────────────────────────────────────────
+GameplayTags=(Tag="SuspenseCore.Event.GAS")
+GameplayTags=(Tag="SuspenseCore.Event.GAS.Ability.Activated")
+GameplayTags=(Tag="SuspenseCore.Event.GAS.Ability.Ended")
+GameplayTags=(Tag="SuspenseCore.Event.GAS.Ability.Cancelled")
+GameplayTags=(Tag="SuspenseCore.Event.GAS.Attribute.Changed")
+GameplayTags=(Tag="SuspenseCore.Event.GAS.Attribute.Health")
+GameplayTags=(Tag="SuspenseCore.Event.GAS.Attribute.Stamina")
+GameplayTags=(Tag="SuspenseCore.Event.GAS.Attribute.Ammo")
+GameplayTags=(Tag="SuspenseCore.Event.GAS.Effect.Applied")
+GameplayTags=(Tag="SuspenseCore.Event.GAS.Effect.Removed")

; ───────────────────────────────────────────────────────────────────────────
; WEAPON EVENTS
; ───────────────────────────────────────────────────────────────────────────
+GameplayTags=(Tag="SuspenseCore.Event.Weapon")
+GameplayTags=(Tag="SuspenseCore.Event.Weapon.Equipped")
+GameplayTags=(Tag="SuspenseCore.Event.Weapon.Unequipped")
+GameplayTags=(Tag="SuspenseCore.Event.Weapon.Fired")
+GameplayTags=(Tag="SuspenseCore.Event.Weapon.Reloaded")
+GameplayTags=(Tag="SuspenseCore.Event.Weapon.AmmoChanged")

; ───────────────────────────────────────────────────────────────────────────
; INVENTORY EVENTS
; ───────────────────────────────────────────────────────────────────────────
+GameplayTags=(Tag="SuspenseCore.Event.Inventory")
+GameplayTags=(Tag="SuspenseCore.Event.Inventory.ItemAdded")
+GameplayTags=(Tag="SuspenseCore.Event.Inventory.ItemRemoved")
+GameplayTags=(Tag="SuspenseCore.Event.Inventory.ItemMoved")
+GameplayTags=(Tag="SuspenseCore.Event.Inventory.SlotChanged")

; ───────────────────────────────────────────────────────────────────────────
; EQUIPMENT EVENTS
; ───────────────────────────────────────────────────────────────────────────
+GameplayTags=(Tag="SuspenseCore.Event.Equipment")
+GameplayTags=(Tag="SuspenseCore.Event.Equipment.SlotChanged")
+GameplayTags=(Tag="SuspenseCore.Event.Equipment.LoadoutChanged")

; ───────────────────────────────────────────────────────────────────────────
; INTERACTION EVENTS
; ───────────────────────────────────────────────────────────────────────────
+GameplayTags=(Tag="SuspenseCore.Event.Interaction")
+GameplayTags=(Tag="SuspenseCore.Event.Interaction.Started")
+GameplayTags=(Tag="SuspenseCore.Event.Interaction.Completed")
+GameplayTags=(Tag="SuspenseCore.Event.Interaction.Cancelled")
+GameplayTags=(Tag="SuspenseCore.Event.Interaction.Available")
+GameplayTags=(Tag="SuspenseCore.Event.Interaction.Unavailable")

; ───────────────────────────────────────────────────────────────────────────
; UI EVENTS
; ───────────────────────────────────────────────────────────────────────────
+GameplayTags=(Tag="SuspenseCore.Event.UI")
+GameplayTags=(Tag="SuspenseCore.Event.UI.WidgetOpened")
+GameplayTags=(Tag="SuspenseCore.Event.UI.WidgetClosed")
+GameplayTags=(Tag="SuspenseCore.Event.UI.Notification")
+GameplayTags=(Tag="SuspenseCore.Event.UI.HUD.Update")
```

---

## 6. Чеклист выполнения

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    ЧЕКЛИСТ ЭТАПА 1: BridgeSystem                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ ПОДГОТОВКА                                                                   │
│ ☐ Создать папку BridgeSystem/Public/Core/                                   │
│ ☐ Создать папку BridgeSystem/Private/Core/                                  │
│                                                                              │
│ ТИПЫ ДАННЫХ                                                                  │
│ ☐ Создать SuspenseCoreTypes.h                                               │
│   ☐ ESuspenseCoreEventPriority                                              │
│   ☐ ESuspenseCoreEventContext                                               │
│   ☐ FSuspenseCoreSubscriptionHandle                                         │
│   ☐ FSuspenseCoreEventData                                                  │
│   ☐ FSuspenseCoreSubscription                                               │
│   ☐ FSuspenseCoreQueuedEvent                                                │
│   ☐ FSuspenseCoreEventBusStats                                              │
│   ☐ FSuspenseCoreEventCallback (delegate)                                   │
│                                                                              │
│ ИНТЕРФЕЙСЫ                                                                   │
│ ☐ Создать SuspenseCoreInterfaces.h                                          │
│   ☐ ISuspenseCoreEventSubscriber                                            │
│   ☐ ISuspenseCoreService                                                    │
│   ☐ ISuspenseCoreEventEmitter                                               │
│                                                                              │
│ EVENTBUS                                                                     │
│ ☐ Создать SuspenseCoreEventBus.h                                            │
│ ☐ Создать SuspenseCoreEventBus.cpp                                          │
│   ☐ Publish()                                                               │
│   ☐ PublishDeferred()                                                       │
│   ☐ PublishWithPriority()                                                   │
│   ☐ Subscribe()                                                             │
│   ☐ SubscribeToChildren()                                                   │
│   ☐ SubscribeWithFilter()                                                   │
│   ☐ Unsubscribe()                                                           │
│   ☐ UnsubscribeAll()                                                        │
│   ☐ ProcessDeferredEvents()                                                 │
│   ☐ CleanupStaleSubscriptions()                                             │
│                                                                              │
│ SERVICE LOCATOR                                                              │
│ ☐ Создать SuspenseCoreServiceLocator.h                                      │
│ ☐ Создать SuspenseCoreServiceLocator.cpp                                    │
│   ☐ RegisterServiceByName()                                                 │
│   ☐ GetServiceByName()                                                      │
│   ☐ HasService()                                                            │
│   ☐ UnregisterService()                                                     │
│   ☐ ClearAllServices()                                                      │
│                                                                              │
│ EVENT MANAGER                                                                │
│ ☐ Создать SuspenseCoreEventManager.h                                        │
│ ☐ Создать SuspenseCoreEventManager.cpp                                      │
│   ☐ Initialize()                                                            │
│   ☐ Deinitialize()                                                          │
│   ☐ Get() static accessor                                                   │
│   ☐ GetEventBus()                                                           │
│   ☐ GetServiceLocator()                                                     │
│   ☐ PublishEvent() helper                                                   │
│   ☐ SubscribeToEvent() helper                                               │
│                                                                              │
│ GAMEPLAY TAGS                                                                │
│ ☐ Добавить теги в DefaultGameplayTags.ini                                   │
│ ☐ Создать макросы SUSPENSE_CORE_EVENT_TAG()                                 │
│                                                                              │
│ ТЕСТИРОВАНИЕ                                                                 │
│ ☐ Unit тест: EventBus Publish/Subscribe                                     │
│ ☐ Unit тест: EventBus Priority ordering                                     │
│ ☐ Unit тест: EventBus Deferred events                                       │
│ ☐ Unit тест: ServiceLocator Register/Get                                    │
│ ☐ Unit тест: EventManager lifecycle                                         │
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

## 7. Пример использования

### 7.1 Публикация события

```cpp
// В любом месте кода
if (auto* Manager = USuspenseCoreEventManager::Get(this))
{
    FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
    EventData.SetFloat(TEXT("DamageAmount"), 50.0f);
    EventData.SetObject(TEXT("Instigator"), DamageInstigator);

    Manager->GetEventBus()->Publish(
        FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Player.Died")),
        EventData
    );
}
```

### 7.2 Подписка на событие

```cpp
// В BeginPlay или SetupEventSubscriptions
void AMyActor::BeginPlay()
{
    Super::BeginPlay();

    if (auto* Manager = USuspenseCoreEventManager::Get(this))
    {
        SubscriptionHandle = Manager->GetEventBus()->Subscribe(
            FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Player.Died")),
            FSuspenseCoreEventCallback::CreateUObject(this, &AMyActor::OnPlayerDied)
        );
    }
}

void AMyActor::OnPlayerDied(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
    float Damage = EventData.GetFloat(TEXT("DamageAmount"));
    UE_LOG(LogTemp, Log, TEXT("Player died with damage: %f"), Damage);
}

void AMyActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (auto* Manager = USuspenseCoreEventManager::Get(this))
    {
        Manager->GetEventBus()->Unsubscribe(SubscriptionHandle);
    }

    Super::EndPlay(EndPlayReason);
}
```

---

*Документ создан: 2025-11-29*
*Этап: 1 из 7*
*Зависимости: Нет*
