# ServiceLocator Centralization Plan

**Модуль:** BridgeSystem
**Приоритет:** Высокий (Архитектурная чистота)
**Статус:** Планирование
**Дата:** 2025-12-05

---

## Обзор

### Текущее состояние

```cpp
// Сейчас: Прямой доступ через helpers в разных местах
USuspenseCoreEventBus* EventBus = USuspenseCoreHelpers::GetEventBus(this);
USuspenseCoreDataManager* DataManager = USuspenseCoreDataManager::Get(this);
USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
// ... каждый компонент сам ищет зависимости
```

**Проблемы:**
1. Scattered dependency resolution — каждый класс сам ищет
2. Tight coupling to concrete implementations
3. Сложно тестировать (нет mock injection)
4. Дублирование кода поиска в каждом компоненте

### Целевое состояние

```cpp
// Централизованный ServiceLocator
USuspenseCoreServiceProvider* Provider = USuspenseCoreServiceProvider::Get(this);

// Типизированный доступ
auto* EventBus = Provider->GetService<USuspenseCoreEventBus>();
auto* DataManager = Provider->GetService<USuspenseCoreDataManager>();

// Или через интерфейсы
auto* ItemProvider = Provider->GetServiceAs<ISuspenseCoreItemProvider>(TEXT("ItemProvider"));
```

---

## Архитектура

### Компоненты

```
┌─────────────────────────────────────────────────────────────────┐
│                 USuspenseCoreServiceProvider                     │
│                   (GameInstanceSubsystem)                        │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              USuspenseCoreServiceLocator                 │   │
│  │  ├── RegisterService<T>(Instance)                        │   │
│  │  ├── GetService<T>() → T*                                │   │
│  │  ├── GetServiceAs<I>(Name) → I*                          │   │
│  │  └── HasService(Name) → bool                             │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              USuspenseCoreServiceRegistry                │   │
│  │  ├── ServiceDefinitions (config)                         │   │
│  │  ├── InterfaceBindings                                   │   │
│  │  └── Lifecycle policies                                  │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│  Registered Services:                                            │
│  ├── EventBus (Singleton, Eager)                                │
│  ├── DataManager (Singleton, Eager)                             │
│  ├── EventManager (Singleton, Eager)                            │
│  ├── SaveManager (Singleton, Lazy)                              │
│  └── ReplicationGraph (Singleton, Lazy)                         │
└─────────────────────────────────────────────────────────────────┘
```

### Файловая структура

```
Source/BridgeSystem/
├── Public/SuspenseCore/Services/
│   ├── SuspenseCoreServiceProvider.h      ← GameInstanceSubsystem
│   ├── SuspenseCoreServiceLocator.h       ← Существует, расширить
│   ├── SuspenseCoreServiceRegistry.h      ← Конфигурация сервисов
│   ├── SuspenseCoreServiceInterfaces.h    ← Интерфейсы сервисов
│   └── SuspenseCoreServiceMacros.h        ← Convenience macros
├── Private/SuspenseCore/Services/
│   ├── SuspenseCoreServiceProvider.cpp
│   ├── SuspenseCoreServiceLocator.cpp     ← Существует
│   └── SuspenseCoreServiceRegistry.cpp
└── Documentation/
    └── SuspenseCoreArchitecture.md (обновить)
```

---

## Этапы реализации

### Этап 1: ServiceProvider Subsystem

**Задачи:**

1. Создать `USuspenseCoreServiceProvider` (GameInstanceSubsystem)
2. Интегрировать существующий `USuspenseCoreServiceLocator`
3. Автоматическая регистрация core сервисов

**Код:**

```cpp
// SuspenseCoreServiceProvider.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SuspenseCoreServiceProvider.generated.h"

class USuspenseCoreServiceLocator;
class USuspenseCoreEventBus;
class USuspenseCoreDataManager;
class USuspenseCoreEventManager;

/**
 * USuspenseCoreServiceProvider
 *
 * Central access point for all SuspenseCore services.
 * GameInstanceSubsystem ensures proper lifecycle management.
 *
 * DESIGN PRINCIPLES:
 * - Single point of service resolution
 * - Automatic registration of core services
 * - Support for interface-based lookup
 * - Lazy initialization where appropriate
 * - Thread-safe access
 *
 * USAGE:
 * ```cpp
 * // Get provider
 * auto* Provider = USuspenseCoreServiceProvider::Get(WorldContext);
 *
 * // Get typed service
 * auto* EventBus = Provider->GetEventBus();
 *
 * // Or generic access
 * auto* Service = Provider->GetService<USomeService>();
 * ```
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreServiceProvider : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    //==========================================================================
    // Static Access
    //==========================================================================

    /** Get provider from world context */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Services",
        meta = (WorldContext = "WorldContextObject", DisplayName = "Get Service Provider"))
    static USuspenseCoreServiceProvider* Get(const UObject* WorldContextObject);

    //==========================================================================
    // USubsystem Interface
    //==========================================================================

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

    //==========================================================================
    // Core Service Accessors (Typed, cached)
    //==========================================================================

    /** Get EventBus (always available after init) */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Services")
    USuspenseCoreEventBus* GetEventBus() const;

    /** Get DataManager (always available after init) */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Services")
    USuspenseCoreDataManager* GetDataManager() const;

    /** Get EventManager (always available after init) */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Services")
    USuspenseCoreEventManager* GetEventManager() const;

    //==========================================================================
    // Generic Service Access
    //==========================================================================

    /** Get service by class */
    template<typename T>
    T* GetService() const
    {
        return ServiceLocator ? ServiceLocator->GetService<T>() : nullptr;
    }

    /** Get service by interface name */
    template<typename T>
    T* GetServiceAs(FName InterfaceName) const
    {
        return ServiceLocator ? ServiceLocator->GetServiceAs<T>(InterfaceName) : nullptr;
    }

    /** Check if service is registered */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Services")
    bool HasService(FName ServiceName) const;

    //==========================================================================
    // Service Registration (for modules)
    //==========================================================================

    /** Register a service (called by modules during init) */
    template<typename T>
    void RegisterService(T* ServiceInstance)
    {
        if (ServiceLocator)
        {
            ServiceLocator->RegisterService<T>(ServiceInstance);
        }
    }

    /** Register service as interface */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Services")
    void RegisterServiceByName(FName ServiceName, UObject* ServiceInstance);

    /** Unregister a service */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Services")
    void UnregisterService(FName ServiceName);

    //==========================================================================
    // Initialization Status
    //==========================================================================

    /** Check if provider is fully initialized */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Services")
    bool IsInitialized() const { return bIsInitialized; }

    /** Get list of registered services (debug) */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Services|Debug")
    TArray<FName> GetRegisteredServiceNames() const;

protected:
    /** Register core SuspenseCore services */
    void RegisterCoreServices();

    /** Broadcast initialization event */
    void BroadcastInitialized();

private:
    /** Core service locator */
    UPROPERTY()
    TObjectPtr<USuspenseCoreServiceLocator> ServiceLocator;

    /** Cached core services for fast access */
    UPROPERTY()
    TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

    UPROPERTY()
    TWeakObjectPtr<USuspenseCoreDataManager> CachedDataManager;

    UPROPERTY()
    TWeakObjectPtr<USuspenseCoreEventManager> CachedEventManager;

    /** Initialization flag */
    bool bIsInitialized = false;
};
```

**Implementation:**

```cpp
// SuspenseCoreServiceProvider.cpp
#include "SuspenseCore/Services/SuspenseCoreServiceProvider.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "Engine/GameInstance.h"

USuspenseCoreServiceProvider* USuspenseCoreServiceProvider::Get(const UObject* WorldContextObject)
{
    if (!WorldContextObject)
    {
        return nullptr;
    }

    const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        return nullptr;
    }

    UGameInstance* GI = World->GetGameInstance();
    if (!GI)
    {
        return nullptr;
    }

    return GI->GetSubsystem<USuspenseCoreServiceProvider>();
}

void USuspenseCoreServiceProvider::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Create service locator
    ServiceLocator = NewObject<USuspenseCoreServiceLocator>(this, TEXT("ServiceLocator"));

    // Ensure dependent subsystems are initialized first
    Collection.InitializeDependency<USuspenseCoreEventManager>();
    Collection.InitializeDependency<USuspenseCoreDataManager>();

    // Register core services
    RegisterCoreServices();

    bIsInitialized = true;

    UE_LOG(LogTemp, Log, TEXT("SuspenseCoreServiceProvider: Initialized with %d services"),
        ServiceLocator->GetServiceCount());

    BroadcastInitialized();
}

void USuspenseCoreServiceProvider::RegisterCoreServices()
{
    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        return;
    }

    // EventManager (provides EventBus)
    if (USuspenseCoreEventManager* EventManager = GI->GetSubsystem<USuspenseCoreEventManager>())
    {
        ServiceLocator->RegisterService<USuspenseCoreEventManager>(EventManager);
        CachedEventManager = EventManager;

        // Also register EventBus directly for convenience
        if (USuspenseCoreEventBus* EventBus = EventManager->GetEventBus())
        {
            ServiceLocator->RegisterService<USuspenseCoreEventBus>(EventBus);
            CachedEventBus = EventBus;
        }
    }

    // DataManager
    if (USuspenseCoreDataManager* DataManager = GI->GetSubsystem<USuspenseCoreDataManager>())
    {
        ServiceLocator->RegisterService<USuspenseCoreDataManager>(DataManager);
        CachedDataManager = DataManager;
    }
}

USuspenseCoreEventBus* USuspenseCoreServiceProvider::GetEventBus() const
{
    if (CachedEventBus.IsValid())
    {
        return CachedEventBus.Get();
    }

    // Fallback to locator
    return GetService<USuspenseCoreEventBus>();
}

USuspenseCoreDataManager* USuspenseCoreServiceProvider::GetDataManager() const
{
    if (CachedDataManager.IsValid())
    {
        return CachedDataManager.Get();
    }

    return GetService<USuspenseCoreDataManager>();
}

USuspenseCoreEventManager* USuspenseCoreServiceProvider::GetEventManager() const
{
    if (CachedEventManager.IsValid())
    {
        return CachedEventManager.Get();
    }

    return GetService<USuspenseCoreEventManager>();
}

void USuspenseCoreServiceProvider::BroadcastInitialized()
{
    if (USuspenseCoreEventBus* EventBus = GetEventBus())
    {
        FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
        EventData.SetInt32(FName("ServiceCount"), ServiceLocator->GetServiceCount());
        EventBus->Publish(
            FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Services.Initialized")),
            EventData
        );
    }
}
```

---

### Этап 2: Service Interfaces

**Задачи:**

1. Создать интерфейсы для абстракции сервисов
2. Позволить подмену реализаций (для тестов)

**Код:**

```cpp
// SuspenseCoreServiceInterfaces.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreServiceInterfaces.generated.h"

struct FSuspenseCoreEventData;
struct FSuspenseCoreItemData;

/**
 * ISuspenseCoreEventPublisher
 * Interface for services that can publish events.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreEventPublisher : public UInterface
{
    GENERATED_BODY()
};

class BRIDGESYSTEM_API ISuspenseCoreEventPublisher
{
    GENERATED_BODY()

public:
    /** Publish event through EventBus */
    virtual void PublishEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData) = 0;
};

/**
 * ISuspenseCoreItemProvider
 * Interface for services that provide item data.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreItemProvider : public UInterface
{
    GENERATED_BODY()
};

class BRIDGESYSTEM_API ISuspenseCoreItemProvider
{
    GENERATED_BODY()

public:
    /** Get item data by ID */
    virtual bool GetItemData(FName ItemID, FSuspenseCoreItemData& OutData) const = 0;

    /** Check if item exists */
    virtual bool HasItem(FName ItemID) const = 0;
};

/**
 * ISuspenseCoreServiceConsumer
 * Interface for objects that consume services (for auto-injection).
 */
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreServiceConsumer : public UInterface
{
    GENERATED_BODY()
};

class BRIDGESYSTEM_API ISuspenseCoreServiceConsumer
{
    GENERATED_BODY()

public:
    /** Called after services are injected */
    virtual void OnServicesInjected(class USuspenseCoreServiceProvider* Provider) = 0;

    /** Get required service names for validation */
    virtual TArray<FName> GetRequiredServices() const { return TArray<FName>(); }
};
```

---

### Этап 3: Service Macros

**Задачи:**

1. Упростить доступ к сервисам через макросы
2. Унифицировать паттерн во всём коде

**Код:**

```cpp
// SuspenseCoreServiceMacros.h
#pragma once

#include "SuspenseCore/Services/SuspenseCoreServiceProvider.h"

/**
 * SUSPENSE_GET_SERVICE
 * Macro to get typed service from ServiceProvider.
 *
 * Usage:
 *   SUSPENSE_GET_SERVICE(this, USuspenseCoreEventBus, EventBus);
 *   if (EventBus) { ... }
 */
#define SUSPENSE_GET_SERVICE(WorldContext, ServiceClass, OutVar) \
    ServiceClass* OutVar = nullptr; \
    if (USuspenseCoreServiceProvider* _Provider = USuspenseCoreServiceProvider::Get(WorldContext)) \
    { \
        OutVar = _Provider->GetService<ServiceClass>(); \
    }

/**
 * SUSPENSE_GET_EVENTBUS
 * Shortcut for EventBus (most common case).
 */
#define SUSPENSE_GET_EVENTBUS(WorldContext, OutVar) \
    USuspenseCoreEventBus* OutVar = nullptr; \
    if (USuspenseCoreServiceProvider* _Provider = USuspenseCoreServiceProvider::Get(WorldContext)) \
    { \
        OutVar = _Provider->GetEventBus(); \
    }

/**
 * SUSPENSE_GET_DATAMANAGER
 * Shortcut for DataManager.
 */
#define SUSPENSE_GET_DATAMANAGER(WorldContext, OutVar) \
    USuspenseCoreDataManager* OutVar = nullptr; \
    if (USuspenseCoreServiceProvider* _Provider = USuspenseCoreServiceProvider::Get(WorldContext)) \
    { \
        OutVar = _Provider->GetDataManager(); \
    }

/**
 * SUSPENSE_PUBLISH_EVENT
 * Quick event publishing.
 *
 * Usage:
 *   SUSPENSE_PUBLISH_EVENT(this, "SuspenseCore.Event.MyEvent", EventData);
 */
#define SUSPENSE_PUBLISH_EVENT(WorldContext, EventTagName, EventData) \
    do { \
        SUSPENSE_GET_EVENTBUS(WorldContext, _EB); \
        if (_EB) { \
            _EB->Publish(FGameplayTag::RequestGameplayTag(FName(EventTagName)), EventData); \
        } \
    } while(0)

/**
 * SUSPENSE_REQUIRE_SERVICE
 * Assert service is available (development builds).
 */
#define SUSPENSE_REQUIRE_SERVICE(WorldContext, ServiceClass) \
    do { \
        SUSPENSE_GET_SERVICE(WorldContext, ServiceClass, _Svc); \
        checkf(_Svc != nullptr, TEXT("Required service %s not available"), TEXT(#ServiceClass)); \
    } while(0)
```

---

### Этап 4: Миграция существующего кода

**Задачи:**

1. Заменить прямые вызовы на ServiceProvider
2. Обновить SuspenseCoreHelpers
3. Не ломать Legacy код

**Код миграции для SuspenseCoreHelpers:**

```cpp
// SuspenseCoreHelpers.h (обновлённый)
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SuspenseCoreHelpers.generated.h"

class USuspenseCoreEventBus;
class USuspenseCoreDataManager;
class USuspenseCoreServiceProvider;

/**
 * USuspenseCoreHelpers
 *
 * Blueprint function library for SuspenseCore services.
 * Now delegates to ServiceProvider for centralized access.
 */
UCLASS()
class INTERACTIONSYSTEM_API USuspenseCoreHelpers : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    //==========================================================================
    // Service Access (NEW - Preferred)
    //==========================================================================

    /** Get ServiceProvider (central access point) */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Services",
        meta = (WorldContext = "WorldContextObject", DisplayName = "Get Service Provider"))
    static USuspenseCoreServiceProvider* GetServiceProvider(const UObject* WorldContextObject);

    //==========================================================================
    // Convenience Accessors (Delegates to ServiceProvider)
    //==========================================================================

    /** Get EventBus from ServiceProvider */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Events",
        meta = (WorldContext = "WorldContextObject", DisplayName = "Get EventBus"))
    static USuspenseCoreEventBus* GetEventBus(const UObject* WorldContextObject);

    /** Get DataManager from ServiceProvider */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data",
        meta = (WorldContext = "WorldContextObject", DisplayName = "Get Data Manager"))
    static USuspenseCoreDataManager* GetDataManager(const UObject* WorldContextObject);

    //==========================================================================
    // Legacy Support (Redirects to new system)
    //==========================================================================

    /** @deprecated Use GetDataManager instead */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Legacy",
        meta = (WorldContext = "WorldContextObject", DeprecatedFunction,
                DeprecationMessage = "Use GetDataManager instead"))
    static class USuspenseCoreItemManager* GetItemManager(const UObject* WorldContextObject);
};
```

```cpp
// SuspenseCoreHelpers.cpp
#include "SuspenseCore/Utils/SuspenseCoreHelpers.h"
#include "SuspenseCore/Services/SuspenseCoreServiceProvider.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"

USuspenseCoreServiceProvider* USuspenseCoreHelpers::GetServiceProvider(const UObject* WorldContextObject)
{
    return USuspenseCoreServiceProvider::Get(WorldContextObject);
}

USuspenseCoreEventBus* USuspenseCoreHelpers::GetEventBus(const UObject* WorldContextObject)
{
    if (USuspenseCoreServiceProvider* Provider = GetServiceProvider(WorldContextObject))
    {
        return Provider->GetEventBus();
    }
    return nullptr;
}

USuspenseCoreDataManager* USuspenseCoreHelpers::GetDataManager(const UObject* WorldContextObject)
{
    if (USuspenseCoreServiceProvider* Provider = GetServiceProvider(WorldContextObject))
    {
        return Provider->GetDataManager();
    }
    return nullptr;
}
```

---

### Этап 5: Component Auto-Injection

**Задачи:**

1. Автоматическая инъекция сервисов в компоненты
2. Валидация зависимостей при старте

**Код:**

```cpp
// В компонентах реализующих ISuspenseCoreServiceConsumer:

// SuspenseCoreInteractionComponent.h (обновлённый)
class USuspenseCoreInteractionComponent
    : public UActorComponent
    , public ISuspenseCoreEventSubscriber
    , public ISuspenseCoreEventEmitter
    , public ISuspenseCoreServiceConsumer  // ← Добавить
{
    // ...

    //~ ISuspenseCoreServiceConsumer
    virtual void OnServicesInjected(USuspenseCoreServiceProvider* Provider) override;
    virtual TArray<FName> GetRequiredServices() const override;
};

// Implementation
void USuspenseCoreInteractionComponent::BeginPlay()
{
    Super::BeginPlay();

    // Get services through provider
    if (USuspenseCoreServiceProvider* Provider = USuspenseCoreServiceProvider::Get(this))
    {
        // Validate required services
        for (const FName& ServiceName : GetRequiredServices())
        {
            if (!Provider->HasService(ServiceName))
            {
                UE_LOG(LogSuspenseCore, Error,
                    TEXT("InteractionComponent: Required service '%s' not available"),
                    *ServiceName.ToString());
            }
        }

        // Cache services
        CachedEventBus = Provider->GetEventBus();

        // Notify ready
        OnServicesInjected(Provider);
    }
}

TArray<FName> USuspenseCoreInteractionComponent::GetRequiredServices() const
{
    return {
        USuspenseCoreEventBus::StaticClass()->GetFName()
    };
}

void USuspenseCoreInteractionComponent::OnServicesInjected(USuspenseCoreServiceProvider* Provider)
{
    // Setup event subscriptions now that services are available
    if (USuspenseCoreEventBus* EventBus = Provider->GetEventBus())
    {
        SetupEventSubscriptions(EventBus);
    }
}
```

---

## Примеры использования

### До (текущий код):

```cpp
void USomeComponent::BeginPlay()
{
    Super::BeginPlay();

    // Прямые вызовы в разных местах
    EventBus = USuspenseCoreHelpers::GetEventBus(this);
    DataManager = USuspenseCoreDataManager::Get(this);

    // Иногда через EventManager
    if (USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this))
    {
        EventBus = Manager->GetEventBus();
    }
}
```

### После (новый код):

```cpp
void USomeComponent::BeginPlay()
{
    Super::BeginPlay();

    // Единая точка входа
    if (USuspenseCoreServiceProvider* Provider = USuspenseCoreServiceProvider::Get(this))
    {
        EventBus = Provider->GetEventBus();
        DataManager = Provider->GetDataManager();

        // Или через макросы
        // SUSPENSE_GET_EVENTBUS(this, EventBus);
    }
}
```

### Или с макросами:

```cpp
void USomeComponent::DoSomething()
{
    // Быстрый доступ через макросы
    SUSPENSE_GET_EVENTBUS(this, EventBus);
    if (EventBus)
    {
        FSuspenseCoreEventData Data = FSuspenseCoreEventData::Create(this);
        EventBus->Publish(MyEventTag, Data);
    }

    // Или ещё короче
    FSuspenseCoreEventData Data = FSuspenseCoreEventData::Create(this);
    SUSPENSE_PUBLISH_EVENT(this, "SuspenseCore.Event.Something.Happened", Data);
}
```

---

## GameplayTags для событий

Добавить в `DefaultGameplayTags.ini`:

```ini
; Service Events
+GameplayTagList=(Tag="SuspenseCore.Event.Services.Initialized",DevComment="ServiceProvider fully initialized")
+GameplayTagList=(Tag="SuspenseCore.Event.Services.ServiceRegistered",DevComment="New service registered")
+GameplayTagList=(Tag="SuspenseCore.Event.Services.ServiceUnregistered",DevComment="Service unregistered")
+GameplayTagList=(Tag="SuspenseCore.Event.Services.Error",DevComment="Service access error")
```

---

## Чеклист реализации

### Этап 1: ServiceProvider Subsystem
- [ ] Создать `SuspenseCoreServiceProvider.h/cpp`
- [ ] Интегрировать с существующим `SuspenseCoreServiceLocator`
- [ ] Автоматическая регистрация core сервисов
- [ ] Скомпилировать и протестировать

### Этап 2: Service Interfaces
- [ ] Создать `SuspenseCoreServiceInterfaces.h`
- [ ] `ISuspenseCoreEventPublisher`
- [ ] `ISuspenseCoreItemProvider`
- [ ] `ISuspenseCoreServiceConsumer`

### Этап 3: Service Macros
- [ ] Создать `SuspenseCoreServiceMacros.h`
- [ ] `SUSPENSE_GET_SERVICE`
- [ ] `SUSPENSE_GET_EVENTBUS`
- [ ] `SUSPENSE_PUBLISH_EVENT`

### Этап 4: Миграция кода
- [ ] Обновить `SuspenseCoreHelpers` (делегировать в ServiceProvider)
- [ ] Обновить `SuspenseCoreInteractionComponent`
- [ ] Обновить `SuspenseCoreInventoryComponent`
- [ ] Не ломать Legacy код

### Этап 5: Auto-Injection
- [ ] Реализовать `ISuspenseCoreServiceConsumer`
- [ ] Добавить валидацию зависимостей
- [ ] Документировать паттерн

### Этап 6: Документация
- [ ] Обновить `SuspenseCoreArchitecture.md`
- [ ] Обновить `BestPractices.md`
- [ ] Создать примеры использования

---

## Риски

| Риск | Вероятность | Влияние | Митигация |
|------|-------------|---------|-----------|
| Circular dependency при инициализации | Средняя | Высокое | InitializeDependency() в subsystem |
| Legacy код сломается | Низкая | Среднее | Helpers делегируют, не меняют API |
| Performance overhead | Низкая | Низкое | Кэширование, прямые методы для hot path |

---

## Метрики успеха

| Метрика | Текущее | Целевое |
|---------|---------|---------|
| Точек доступа к EventBus | 15+ разных | 1 (ServiceProvider) |
| Дублирование кода поиска | Высокое | Минимальное |
| Возможность mock injection | Нет | Да |
| Валидация зависимостей | Нет | Автоматическая |

---

**Автор:** Tech Lead Review
**Версия:** 1.0
**Последнее обновление:** 2025-12-05
