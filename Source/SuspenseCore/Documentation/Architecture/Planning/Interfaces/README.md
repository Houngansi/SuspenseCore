# Interfaces Specification — SuspenseCore

> **Версия:** 1.0
> **Naming Convention:** ISuspenseCore*

---

## 1. Обзор

Все интерфейсы в проекте следуют конвенции `ISuspenseCore*`.
Интерфейсы обеспечивают loose coupling между модулями.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         INTERFACES HIERARCHY                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   BridgeSystem                                                               │
│   ├── ISuspenseCoreEventSubscriber                                          │
│   ├── ISuspenseCoreEventEmitter                                             │
│   └── ISuspenseCoreService                                                  │
│                                                                              │
│   GAS                                                                        │
│   └── ISuspenseCoreAbilityInterface                                         │
│                                                                              │
│   PlayerCore                                                                 │
│   └── ISuspenseCoreCharacterInterface                                       │
│                                                                              │
│   InteractionSystem (Фаза 3)                                                │
│   ├── ISuspenseCoreInteractable                                             │
│   └── ISuspenseCoreInteractor                                               │
│                                                                              │
│   InventorySystem (Фаза 3)                                                  │
│   └── ISuspenseCoreInventoryHolder                                          │
│                                                                              │
│   EquipmentSystem (Фаза 3)                                                  │
│   └── ISuspenseCoreEquipable                                                │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. BridgeSystem Interfaces

### 2.1 ISuspenseCoreEventSubscriber

**Файл:** `BridgeSystem/Public/Core/SuspenseCoreInterfaces.h`

```cpp
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreEventSubscriber : public UInterface
{
    GENERATED_BODY()
};

/**
 * ISuspenseCoreEventSubscriber
 *
 * Интерфейс для объектов, подписывающихся на события EventBus.
 *
 * Использование:
 * - Реализуйте в классах, которые слушают события
 * - SetupEventSubscriptions() вызывается при инициализации
 * - TeardownEventSubscriptions() вызывается при деинициализации
 */
class BRIDGESYSTEM_API ISuspenseCoreEventSubscriber
{
    GENERATED_BODY()

public:
    /**
     * Настройка подписок на события.
     * Вызывается при инициализации объекта.
     *
     * @param EventBus - шина событий для подписки
     */
    virtual void SetupEventSubscriptions(USuspenseCoreEventBus* EventBus) = 0;

    /**
     * Отписка от событий.
     * Вызывается при уничтожении объекта.
     *
     * @param EventBus - шина событий
     */
    virtual void TeardownEventSubscriptions(USuspenseCoreEventBus* EventBus) = 0;

    /**
     * Получить все активные handles подписок.
     * Используется для отладки и автоматической отписки.
     */
    virtual TArray<FSuspenseCoreSubscriptionHandle> GetSubscriptionHandles() const = 0;
};
```

**Пример реализации:**

```cpp
class PLAYERCORE_API ASuspenseCoreCharacter : public ACharacter,
    public ISuspenseCoreEventSubscriber
{
public:
    virtual void SetupEventSubscriptions(USuspenseCoreEventBus* EventBus) override
    {
        SubscriptionHandles.Add(EventBus->Subscribe(
            SUSPENSE_CORE_EVENT_GAS_ATTRIBUTE_HEALTH,
            FSuspenseCoreEventCallback::CreateUObject(this, &ASuspenseCoreCharacter::OnHealthChanged)
        ));
    }

    virtual void TeardownEventSubscriptions(USuspenseCoreEventBus* EventBus) override
    {
        for (auto& Handle : SubscriptionHandles)
        {
            EventBus->Unsubscribe(Handle);
        }
        SubscriptionHandles.Empty();
    }

    virtual TArray<FSuspenseCoreSubscriptionHandle> GetSubscriptionHandles() const override
    {
        return SubscriptionHandles;
    }

protected:
    TArray<FSuspenseCoreSubscriptionHandle> SubscriptionHandles;
};
```

---

### 2.2 ISuspenseCoreEventEmitter

**Файл:** `BridgeSystem/Public/Core/SuspenseCoreInterfaces.h`

```cpp
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreEventEmitter : public UInterface
{
    GENERATED_BODY()
};

/**
 * ISuspenseCoreEventEmitter
 *
 * Интерфейс для объектов, публикующих события.
 *
 * Использование:
 * - Реализуйте для стандартизации публикации событий
 * - Полезно для тестирования (можно замокать)
 */
class BRIDGESYSTEM_API ISuspenseCoreEventEmitter
{
    GENERATED_BODY()

public:
    /**
     * Публикует событие.
     *
     * @param EventTag - тег события
     * @param Data - данные события
     */
    virtual void EmitEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& Data) = 0;

    /**
     * Получить EventBus для публикации.
     */
    virtual USuspenseCoreEventBus* GetEventBus() const = 0;
};
```

**Пример реализации:**

```cpp
class PLAYERCORE_API ASuspenseCorePlayerState : public APlayerState,
    public ISuspenseCoreEventEmitter
{
public:
    virtual void EmitEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& Data) override
    {
        if (USuspenseCoreEventBus* EventBus = GetEventBus())
        {
            EventBus->Publish(EventTag, Data);
        }
    }

    virtual USuspenseCoreEventBus* GetEventBus() const override
    {
        if (auto* Manager = USuspenseCoreEventManager::Get(this))
        {
            return Manager->GetEventBus();
        }
        return nullptr;
    }
};
```

---

### 2.3 ISuspenseCoreService

**Файл:** `BridgeSystem/Public/Core/SuspenseCoreInterfaces.h`

```cpp
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreService : public UInterface
{
    GENERATED_BODY()
};

/**
 * ISuspenseCoreService
 *
 * Базовый интерфейс для сервисов, регистрируемых в ServiceLocator.
 *
 * Использование:
 * - Реализуйте для любого сервиса с lifecycle
 * - InitializeService() вызывается при регистрации
 * - ShutdownService() вызывается при выгрузке
 */
class BRIDGESYSTEM_API ISuspenseCoreService
{
    GENERATED_BODY()

public:
    /**
     * Инициализация сервиса.
     * Вызывается после регистрации в ServiceLocator.
     */
    virtual void InitializeService() = 0;

    /**
     * Остановка сервиса.
     * Вызывается перед удалением из ServiceLocator.
     */
    virtual void ShutdownService() = 0;

    /**
     * Имя сервиса для отладки.
     */
    virtual FName GetServiceName() const = 0;

    /**
     * Готов ли сервис к использованию.
     */
    virtual bool IsServiceReady() const { return true; }
};
```

---

## 3. GAS Interfaces

### 3.1 ISuspenseCoreAbilityInterface

**Файл:** `GAS/Public/Core/Interfaces/SuspenseCoreAbilityInterface.h`

```cpp
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreAbilityInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * ISuspenseCoreAbilityInterface
 *
 * Интерфейс для Actor'ов с AbilitySystemComponent.
 * Расширение стандартного IAbilitySystemInterface.
 *
 * Использование:
 * - Реализуйте вместе с IAbilitySystemInterface
 * - Добавляет методы специфичные для SuspenseCore
 */
class GAS_API ISuspenseCoreAbilityInterface
{
    GENERATED_BODY()

public:
    /**
     * Получить SuspenseCore версию ASC.
     */
    virtual USuspenseCoreAbilitySystemComponent* GetSuspenseCoreAbilitySystemComponent() const = 0;

    /**
     * Жив ли Actor (для GAS расчётов).
     */
    virtual bool IsAlive() const { return true; }

    /**
     * Уровень для скейлинга эффектов.
     */
    virtual int32 GetAbilityLevel() const { return 1; }

    /**
     * Можно ли активировать способности.
     */
    virtual bool CanActivateAbilities() const { return IsAlive(); }

    /**
     * Получить GameplayTags для фильтрации способностей.
     */
    virtual FGameplayTagContainer GetAbilityTags() const { return FGameplayTagContainer(); }
};
```

---

## 4. PlayerCore Interfaces

### 4.1 ISuspenseCoreCharacterInterface

**Файл:** `PlayerCore/Public/Core/Interfaces/SuspenseCoreCharacterInterface.h`

```cpp
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreCharacterInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * ISuspenseCoreCharacterInterface
 *
 * Интерфейс для персонажей.
 * Позволяет работать с персонажами без знания конкретного класса.
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

    /** Является ли AI-контролируемым */
    virtual bool IsAI() const { return false; }

    /** Получить текущее состояние (для FSM) */
    virtual FName GetCurrentState() const { return NAME_None; }

    // ═══════════════════════════════════════════════════════════════════════
    // TEAM
    // ═══════════════════════════════════════════════════════════════════════

    /** Получить ID команды */
    virtual int32 GetCharacterTeamId() const = 0;

    /** Проверить враг ли */
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

    /** Уникальный ID персонажа */
    virtual int32 GetCharacterId() const = 0;

    /** Отображаемое имя */
    virtual FString GetCharacterDisplayName() const = 0;

    /** Текущая локация */
    virtual FVector GetCharacterLocation() const = 0;

    /** Forward vector */
    virtual FVector GetCharacterForward() const = 0;

    // ═══════════════════════════════════════════════════════════════════════
    // COMBAT
    // ═══════════════════════════════════════════════════════════════════════

    /** Получить текущее здоровье (0-1) */
    virtual float GetHealthPercent() const { return 1.0f; }

    /** Может ли атаковать */
    virtual bool CanAttack() const { return IsCharacterAlive(); }

    /** Может ли получить урон */
    virtual bool CanTakeDamage() const { return IsCharacterAlive(); }
};
```

---

## 5. InteractionSystem Interfaces (Фаза 3)

### 5.1 ISuspenseCoreInteractable

```cpp
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreInteractable : public UInterface
{
    GENERATED_BODY()
};

/**
 * ISuspenseCoreInteractable
 *
 * Интерфейс для объектов, с которыми можно взаимодействовать.
 */
class INTERACTIONSYSTEM_API ISuspenseCoreInteractable
{
    GENERATED_BODY()

public:
    // ═══════════════════════════════════════════════════════════════════════
    // AVAILABILITY
    // ═══════════════════════════════════════════════════════════════════════

    /** Можно ли взаимодействовать */
    virtual bool CanInteract(const AActor* Interactor) const = 0;

    /** Получить приоритет (для выбора из нескольких) */
    virtual int32 GetInteractionPriority() const { return 0; }

    /** Дистанция взаимодействия */
    virtual float GetInteractionDistance() const { return 200.0f; }

    // ═══════════════════════════════════════════════════════════════════════
    // INTERACTION
    // ═══════════════════════════════════════════════════════════════════════

    /** Начать взаимодействие */
    virtual bool StartInteraction(AActor* Interactor) = 0;

    /** Завершить взаимодействие */
    virtual void EndInteraction(AActor* Interactor, bool bWasCancelled) = 0;

    /** Продолжающееся взаимодействие (Hold) */
    virtual void TickInteraction(AActor* Interactor, float DeltaTime) {}

    // ═══════════════════════════════════════════════════════════════════════
    // UI
    // ═══════════════════════════════════════════════════════════════════════

    /** Текст подсказки */
    virtual FText GetInteractionPrompt() const { return FText::GetEmpty(); }

    /** Иконка */
    virtual TSoftObjectPtr<UTexture2D> GetInteractionIcon() const { return nullptr; }

    /** Время удержания (0 = instant) */
    virtual float GetHoldDuration() const { return 0.0f; }

    // ═══════════════════════════════════════════════════════════════════════
    // FOCUS
    // ═══════════════════════════════════════════════════════════════════════

    /** Вызывается когда игрок смотрит на объект */
    virtual void OnFocused(AActor* Interactor) {}

    /** Вызывается когда игрок отвёл взгляд */
    virtual void OnUnfocused(AActor* Interactor) {}
};
```

### 5.2 ISuspenseCoreInteractor

```cpp
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreInteractor : public UInterface
{
    GENERATED_BODY()
};

/**
 * ISuspenseCoreInteractor
 *
 * Интерфейс для Actor'ов, способных взаимодействовать.
 */
class INTERACTIONSYSTEM_API ISuspenseCoreInteractor
{
    GENERATED_BODY()

public:
    /** Может ли сейчас взаимодействовать */
    virtual bool CanPerformInteraction() const = 0;

    /** Получить текущий фокус */
    virtual AActor* GetFocusedInteractable() const = 0;

    /** Дистанция детекции */
    virtual float GetInteractionTraceDistance() const { return 300.0f; }

    /** Радиус трейса */
    virtual float GetInteractionTraceRadius() const { return 30.0f; }
};
```

---

## 6. InventorySystem Interfaces (Фаза 3)

### 6.1 ISuspenseCoreInventoryHolder

```cpp
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreInventoryHolder : public UInterface
{
    GENERATED_BODY()
};

/**
 * ISuspenseCoreInventoryHolder
 *
 * Интерфейс для объектов с инвентарём.
 */
class INVENTORYSYSTEM_API ISuspenseCoreInventoryHolder
{
    GENERATED_BODY()

public:
    /** Получить компонент инвентаря */
    virtual class USuspenseCoreInventoryComponent* GetInventoryComponent() const = 0;

    /** Может ли добавить предмет */
    virtual bool CanAddItem(const class USuspenseCoreItemDefinition* Item) const = 0;

    /** Максимальный вес */
    virtual float GetMaxCarryWeight() const { return 100.0f; }

    /** Текущий вес */
    virtual float GetCurrentCarryWeight() const = 0;
};
```

---

## 7. EquipmentSystem Interfaces (Фаза 3)

### 7.1 ISuspenseCoreEquipable

```cpp
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreEquipable : public UInterface
{
    GENERATED_BODY()
};

/**
 * ISuspenseCoreEquipable
 *
 * Интерфейс для экипируемых предметов.
 */
class EQUIPMENTSYSTEM_API ISuspenseCoreEquipable
{
    GENERATED_BODY()

public:
    /** Получить слот экипировки */
    virtual FGameplayTag GetEquipmentSlot() const = 0;

    /** Можно ли экипировать */
    virtual bool CanBeEquipped(const AActor* Owner) const = 0;

    /** Вызывается при экипировке */
    virtual void OnEquipped(AActor* Owner) = 0;

    /** Вызывается при снятии */
    virtual void OnUnequipped(AActor* Owner) = 0;

    /** Получить AttributeSet для применения */
    virtual TSubclassOf<UAttributeSet> GetGrantedAttributeSet() const { return nullptr; }

    /** Получить Abilities для выдачи */
    virtual TArray<TSubclassOf<UGameplayAbility>> GetGrantedAbilities() const { return {}; }
};
```

---

## 8. Полный список интерфейсов

| Интерфейс | Модуль | Описание |
|-----------|--------|----------|
| `ISuspenseCoreEventSubscriber` | BridgeSystem | Подписчик на события |
| `ISuspenseCoreEventEmitter` | BridgeSystem | Публикатор событий |
| `ISuspenseCoreService` | BridgeSystem | Сервис с lifecycle |
| `ISuspenseCoreAbilityInterface` | GAS | Владелец ASC |
| `ISuspenseCoreCharacterInterface` | PlayerCore | Персонаж |
| `ISuspenseCoreInteractable` | InteractionSystem | Интерактивный объект |
| `ISuspenseCoreInteractor` | InteractionSystem | Тот кто взаимодействует |
| `ISuspenseCoreInventoryHolder` | InventorySystem | Владелец инвентаря |
| `ISuspenseCoreEquipable` | EquipmentSystem | Экипируемый предмет |

---

## 9. Best Practices

### 9.1 DO (Делать)

```
✅ Используйте интерфейсы для cross-module коммуникации
✅ Проверяйте интерфейс через Cast<> или Implements<>
✅ Документируйте контракт каждого метода
✅ Используйте значения по умолчанию где возможно
✅ Объявляйте pure virtual только необходимые методы
```

### 9.2 DON'T (Не делать)

```
❌ Не создавайте "толстые" интерфейсы с множеством методов
❌ Не добавляйте состояние в интерфейсы
❌ Не делайте интерфейсы зависимыми от конкретных классов
❌ Не используйте интерфейсы там где достаточно прямой ссылки
```

---

*Документ создан: 2025-11-29*
*Naming Convention: ISuspenseCore**
