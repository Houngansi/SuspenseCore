# Type Definitions — SuspenseCore

> **Версия:** 1.0
> **Naming Convention:** FSuspenseCore*, ESuspenseCore*

---

## 1. Обзор

Все типы данных в проекте следуют конвенции:
- **Structs:** `FSuspenseCore*`
- **Enums:** `ESuspenseCore*`
- **Delegates:** `FSuspenseCore*Delegate` / `FSuspenseCore*Callback`

---

## 2. BridgeSystem Types

### 2.1 Enums

#### ESuspenseCoreEventPriority

**Файл:** `BridgeSystem/Public/Core/SuspenseCoreTypes.h`

```cpp
/**
 * ESuspenseCoreEventPriority
 *
 * Приоритет обработки событий в EventBus.
 * Меньшее значение = выше приоритет.
 */
UENUM(BlueprintType)
enum class ESuspenseCoreEventPriority : uint8
{
    /** Системные события (обрабатываются первыми) */
    System = 0      UMETA(DisplayName = "System"),

    /** Высокий приоритет (GAS, боевая система) */
    High = 50       UMETA(DisplayName = "High"),

    /** Нормальный приоритет (по умолчанию) */
    Normal = 100    UMETA(DisplayName = "Normal"),

    /** Низкий приоритет (UI, визуальные эффекты) */
    Low = 150       UMETA(DisplayName = "Low"),

    /** Самый низкий (логирование, аналитика) */
    Lowest = 200    UMETA(DisplayName = "Lowest")
};
```

#### ESuspenseCoreEventContext

```cpp
/**
 * ESuspenseCoreEventContext
 *
 * Контекст выполнения события.
 */
UENUM(BlueprintType)
enum class ESuspenseCoreEventContext : uint8
{
    /** Выполнить в Game Thread (по умолчанию) */
    GameThread      UMETA(DisplayName = "Game Thread"),

    /** Может выполниться в любом потоке */
    AnyThread       UMETA(DisplayName = "Any Thread"),

    /** Отложить до конца кадра */
    Deferred        UMETA(DisplayName = "Deferred")
};
```

### 2.2 Structs

#### FSuspenseCoreSubscriptionHandle

```cpp
/**
 * FSuspenseCoreSubscriptionHandle
 *
 * Handle для управления подпиской на события.
 * Используется для отписки.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreSubscriptionHandle
{
    GENERATED_BODY()

    FSuspenseCoreSubscriptionHandle() : Id(0) {}
    explicit FSuspenseCoreSubscriptionHandle(uint64 InId) : Id(InId) {}

    /** Валиден ли handle */
    bool IsValid() const { return Id != 0; }

    /** Инвалидировать handle */
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
```

#### FSuspenseCoreEventData

```cpp
/**
 * FSuspenseCoreEventData
 *
 * Данные события. Содержит источник, временную метку и payload.
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

    // ═══════════════════════════════════════════════════════════════════════
    // CORE FIELDS
    // ═══════════════════════════════════════════════════════════════════════

    /** Источник события (Actor, Component, etc.) */
    UPROPERTY(BlueprintReadWrite, Category = "Event")
    TObjectPtr<UObject> Source;

    /** Временная метка (FPlatformTime::Seconds) */
    UPROPERTY(BlueprintReadOnly, Category = "Event")
    double Timestamp;

    /** Приоритет обработки */
    UPROPERTY(BlueprintReadWrite, Category = "Event")
    ESuspenseCoreEventPriority Priority;

    // ═══════════════════════════════════════════════════════════════════════
    // PAYLOAD
    // ═══════════════════════════════════════════════════════════════════════

    /** Строковые данные */
    UPROPERTY(BlueprintReadWrite, Category = "Payload")
    TMap<FName, FString> StringPayload;

    /** Числовые данные (float) */
    UPROPERTY(BlueprintReadWrite, Category = "Payload")
    TMap<FName, float> FloatPayload;

    /** Целочисленные данные */
    UPROPERTY(BlueprintReadWrite, Category = "Payload")
    TMap<FName, int32> IntPayload;

    /** Булевые данные */
    UPROPERTY(BlueprintReadWrite, Category = "Payload")
    TMap<FName, bool> BoolPayload;

    /** Объекты */
    UPROPERTY(BlueprintReadWrite, Category = "Payload")
    TMap<FName, TObjectPtr<UObject>> ObjectPayload;

    /** Дополнительные теги */
    UPROPERTY(BlueprintReadWrite, Category = "Payload")
    FGameplayTagContainer Tags;

    // ═══════════════════════════════════════════════════════════════════════
    // GETTERS
    // ═══════════════════════════════════════════════════════════════════════

    /** Получить строку */
    FString GetString(FName Key, const FString& Default = TEXT("")) const
    {
        const FString* Value = StringPayload.Find(Key);
        return Value ? *Value : Default;
    }

    /** Получить float */
    float GetFloat(FName Key, float Default = 0.0f) const
    {
        const float* Value = FloatPayload.Find(Key);
        return Value ? *Value : Default;
    }

    /** Получить int */
    int32 GetInt(FName Key, int32 Default = 0) const
    {
        const int32* Value = IntPayload.Find(Key);
        return Value ? *Value : Default;
    }

    /** Получить bool */
    bool GetBool(FName Key, bool Default = false) const
    {
        const bool* Value = BoolPayload.Find(Key);
        return Value ? *Value : Default;
    }

    /** Получить объект с cast */
    template<typename T>
    T* GetObject(FName Key) const
    {
        const TObjectPtr<UObject>* Value = ObjectPayload.Find(Key);
        return Value ? Cast<T>(Value->Get()) : nullptr;
    }

    /** Проверить наличие ключа */
    bool HasKey(FName Key) const
    {
        return StringPayload.Contains(Key)
            || FloatPayload.Contains(Key)
            || IntPayload.Contains(Key)
            || BoolPayload.Contains(Key)
            || ObjectPayload.Contains(Key);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // SETTERS (Fluent API)
    // ═══════════════════════════════════════════════════════════════════════

    FSuspenseCoreEventData& SetString(FName Key, const FString& Value)
    {
        StringPayload.Add(Key, Value);
        return *this;
    }

    FSuspenseCoreEventData& SetFloat(FName Key, float Value)
    {
        FloatPayload.Add(Key, Value);
        return *this;
    }

    FSuspenseCoreEventData& SetInt(FName Key, int32 Value)
    {
        IntPayload.Add(Key, Value);
        return *this;
    }

    FSuspenseCoreEventData& SetBool(FName Key, bool Value)
    {
        BoolPayload.Add(Key, Value);
        return *this;
    }

    FSuspenseCoreEventData& SetObject(FName Key, UObject* Value)
    {
        ObjectPayload.Add(Key, Value);
        return *this;
    }

    FSuspenseCoreEventData& AddTag(FGameplayTag Tag)
    {
        Tags.AddTag(Tag);
        return *this;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // FACTORY
    // ═══════════════════════════════════════════════════════════════════════

    /** Создать EventData с источником */
    static FSuspenseCoreEventData Create(UObject* InSource)
    {
        FSuspenseCoreEventData Data;
        Data.Source = InSource;
        Data.Timestamp = FPlatformTime::Seconds();
        return Data;
    }

    /** Создать EventData с источником и приоритетом */
    static FSuspenseCoreEventData Create(UObject* InSource, ESuspenseCoreEventPriority InPriority)
    {
        FSuspenseCoreEventData Data;
        Data.Source = InSource;
        Data.Timestamp = FPlatformTime::Seconds();
        Data.Priority = InPriority;
        return Data;
    }
};
```

#### FSuspenseCoreSubscription (Internal)

```cpp
/**
 * FSuspenseCoreSubscription
 *
 * Внутренняя структура для хранения подписки.
 * Не экспортируется в Blueprint.
 */
USTRUCT()
struct FSuspenseCoreSubscription
{
    GENERATED_BODY()

    /** Уникальный ID */
    uint64 Id = 0;

    /** Слабая ссылка на подписчика (для cleanup) */
    TWeakObjectPtr<UObject> Subscriber;

    /** Приоритет */
    ESuspenseCoreEventPriority Priority = ESuspenseCoreEventPriority::Normal;

    /** Фильтр по источнику */
    TWeakObjectPtr<UObject> SourceFilter;

    /** Callback */
    FSuspenseCoreEventCallback Callback;

    /** Валидна ли подписка */
    bool IsValid() const
    {
        return Id != 0 && Subscriber.IsValid() && Callback.IsBound();
    }
};
```

#### FSuspenseCoreQueuedEvent (Internal)

```cpp
/**
 * FSuspenseCoreQueuedEvent
 *
 * Событие в очереди отложенных.
 */
USTRUCT()
struct FSuspenseCoreQueuedEvent
{
    GENERATED_BODY()

    UPROPERTY()
    FGameplayTag EventTag;

    UPROPERTY()
    FSuspenseCoreEventData EventData;

    double QueuedTime = 0.0;
};
```

#### FSuspenseCoreEventBusStats

```cpp
/**
 * FSuspenseCoreEventBusStats
 *
 * Статистика EventBus для мониторинга.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreEventBusStats
{
    GENERATED_BODY()

    /** Активные подписки */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 ActiveSubscriptions = 0;

    /** Уникальные теги */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 UniqueEventTags = 0;

    /** Событий опубликовано (за сессию) */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int64 TotalEventsPublished = 0;

    /** Событий в очереди отложенных */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 DeferredEventsQueued = 0;

    /** Среднее время обработки события (ms) */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float AverageProcessingTimeMs = 0.0f;
};
```

### 2.3 Delegates

```cpp
/**
 * Callback при получении события.
 * Blueprint-compatible.
 */
DECLARE_DYNAMIC_DELEGATE_TwoParams(
    FSuspenseCoreEventCallback,
    FGameplayTag, EventTag,
    const FSuspenseCoreEventData&, EventData
);

/**
 * Multicast версия для C++.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(
    FSuspenseCoreEventMulticastDelegate,
    FGameplayTag /*EventTag*/,
    const FSuspenseCoreEventData& /*EventData*/
);

/**
 * Callback при регистрации сервиса.
 */
DECLARE_DYNAMIC_DELEGATE_OneParam(
    FSuspenseCoreServiceRegisteredCallback,
    FName, ServiceName
);
```

---

## 3. GAS Types

### 3.1 Structs

#### FSuspenseCoreAbilityInfo

```cpp
/**
 * FSuspenseCoreAbilityInfo
 *
 * Информация о способности для событий.
 */
USTRUCT(BlueprintType)
struct GAS_API FSuspenseCoreAbilityInfo
{
    GENERATED_BODY()

    /** Класс способности */
    UPROPERTY(BlueprintReadOnly, Category = "Ability")
    TSubclassOf<UGameplayAbility> AbilityClass;

    /** Уровень */
    UPROPERTY(BlueprintReadOnly, Category = "Ability")
    int32 AbilityLevel = 1;

    /** Handle */
    UPROPERTY(BlueprintReadOnly, Category = "Ability")
    FGameplayAbilitySpecHandle Handle;

    /** Теги способности */
    UPROPERTY(BlueprintReadOnly, Category = "Ability")
    FGameplayTagContainer AbilityTags;
};
```

#### FSuspenseCoreAttributeChangeInfo

```cpp
/**
 * FSuspenseCoreAttributeChangeInfo
 *
 * Информация об изменении атрибута.
 */
USTRUCT(BlueprintType)
struct GAS_API FSuspenseCoreAttributeChangeInfo
{
    GENERATED_BODY()

    /** Атрибут */
    UPROPERTY(BlueprintReadOnly, Category = "Attribute")
    FGameplayAttribute Attribute;

    /** Старое значение */
    UPROPERTY(BlueprintReadOnly, Category = "Attribute")
    float OldValue = 0.0f;

    /** Новое значение */
    UPROPERTY(BlueprintReadOnly, Category = "Attribute")
    float NewValue = 0.0f;

    /** Источник изменения */
    UPROPERTY(BlueprintReadOnly, Category = "Attribute")
    TObjectPtr<AActor> Instigator;

    /** Дельта */
    float GetDelta() const { return NewValue - OldValue; }

    /** Процент изменения */
    float GetPercentChange() const
    {
        return OldValue != 0.0f ? (NewValue - OldValue) / OldValue : 0.0f;
    }
};
```

---

## 4. PlayerCore Types

### 4.1 Enums

#### ESuspenseCorePlayerState

```cpp
/**
 * ESuspenseCorePlayerState
 *
 * Состояние игрока.
 */
UENUM(BlueprintType)
enum class ESuspenseCorePlayerState : uint8
{
    /** Не инициализирован */
    None            UMETA(DisplayName = "None"),

    /** В лобби */
    InLobby         UMETA(DisplayName = "In Lobby"),

    /** Спавнится */
    Spawning        UMETA(DisplayName = "Spawning"),

    /** Жив и активен */
    Alive           UMETA(DisplayName = "Alive"),

    /** Мёртв */
    Dead            UMETA(DisplayName = "Dead"),

    /** Наблюдает */
    Spectating      UMETA(DisplayName = "Spectating")
};
```

#### ESuspenseCoreTeam

```cpp
/**
 * ESuspenseCoreTeam
 *
 * Команда игрока.
 */
UENUM(BlueprintType)
enum class ESuspenseCoreTeam : uint8
{
    /** Нет команды */
    None = 0        UMETA(DisplayName = "None"),

    /** Команда 1 */
    Team1 = 1       UMETA(DisplayName = "Team 1"),

    /** Команда 2 */
    Team2 = 2       UMETA(DisplayName = "Team 2"),

    /** Команда 3 */
    Team3 = 3       UMETA(DisplayName = "Team 3"),

    /** Команда 4 */
    Team4 = 4       UMETA(DisplayName = "Team 4")
};
```

### 4.2 Structs

#### FSuspenseCorePlayerInfo

```cpp
/**
 * FSuspenseCorePlayerInfo
 *
 * Информация об игроке для передачи в событиях.
 */
USTRUCT(BlueprintType)
struct PLAYERCORE_API FSuspenseCorePlayerInfo
{
    GENERATED_BODY()

    /** ID игрока */
    UPROPERTY(BlueprintReadOnly, Category = "Player")
    int32 PlayerId = 0;

    /** Отображаемое имя */
    UPROPERTY(BlueprintReadOnly, Category = "Player")
    FString DisplayName;

    /** ID команды */
    UPROPERTY(BlueprintReadOnly, Category = "Player")
    int32 TeamId = 0;

    /** Текущее состояние */
    UPROPERTY(BlueprintReadOnly, Category = "Player")
    ESuspenseCorePlayerState State = ESuspenseCorePlayerState::None;

    /** Ссылка на PlayerState */
    UPROPERTY(BlueprintReadOnly, Category = "Player")
    TWeakObjectPtr<APlayerState> PlayerState;

    /** Создать из PlayerState */
    static FSuspenseCorePlayerInfo FromPlayerState(const ASuspenseCorePlayerState* PS);
};
```

#### FSuspenseCorePlayerStats

```cpp
/**
 * FSuspenseCorePlayerStats
 *
 * Статистика игрока.
 */
USTRUCT(BlueprintType)
struct PLAYERCORE_API FSuspenseCorePlayerStats
{
    GENERATED_BODY()

    /** Убийства */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 Kills = 0;

    /** Смерти */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 Deaths = 0;

    /** Assists */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 Assists = 0;

    /** Урон нанесено */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float DamageDealt = 0.0f;

    /** Урон получено */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float DamageTaken = 0.0f;

    /** K/D Ratio */
    float GetKDRatio() const
    {
        return Deaths > 0 ? static_cast<float>(Kills) / Deaths : Kills;
    }

    /** KDA Ratio */
    float GetKDARatio() const
    {
        return Deaths > 0 ? static_cast<float>(Kills + Assists) / Deaths : Kills + Assists;
    }
};
```

---

## 5. Common Payload Keys

Стандартные ключи для FSuspenseCoreEventData payload:

### 5.1 Player Events

| Key | Type | Description |
|-----|------|-------------|
| `PlayerId` | Int | ID игрока |
| `PlayerName` | String | Имя игрока |
| `TeamId` | Int | ID команды |
| `Location` | Object (FVector*) | Позиция |
| `Killer` | Object (AActor*) | Убийца |
| `DamageType` | String | Тип урона |

### 5.2 GAS Events

| Key | Type | Description |
|-----|------|-------------|
| `AttributeName` | String | Имя атрибута |
| `OldValue` | Float | Старое значение |
| `NewValue` | Float | Новое значение |
| `MaxValue` | Float | Максимальное значение |
| `Instigator` | Object (AActor*) | Источник изменения |
| `AbilityClass` | String | Имя класса способности |
| `AbilityLevel` | Int | Уровень способности |
| `WasCancelled` | Bool | Была ли отменена |

### 5.3 Weapon Events

| Key | Type | Description |
|-----|------|-------------|
| `WeaponClass` | String | Класс оружия |
| `CurrentAmmo` | Int | Текущие патроны |
| `MaxAmmo` | Int | Максимум в магазине |
| `ReserveAmmo` | Int | Запас |
| `HitLocation` | Object (FVector*) | Точка попадания |
| `HitActor` | Object (AActor*) | Поражённый объект |

### 5.4 Inventory Events

| Key | Type | Description |
|-----|------|-------------|
| `ItemId` | String | ID предмета |
| `ItemClass` | String | Класс предмета |
| `Quantity` | Int | Количество |
| `SlotIndex` | Int | Индекс слота |
| `FromSlot` | Int | Откуда |
| `ToSlot` | Int | Куда |

---

## 6. Type Helpers

### 6.1 FSuspenseCoreEventDataBuilder

```cpp
/**
 * Builder для удобного создания EventData.
 */
class BRIDGESYSTEM_API FSuspenseCoreEventDataBuilder
{
public:
    FSuspenseCoreEventDataBuilder(UObject* Source)
    {
        Data = FSuspenseCoreEventData::Create(Source);
    }

    FSuspenseCoreEventDataBuilder& Priority(ESuspenseCoreEventPriority P)
    {
        Data.Priority = P;
        return *this;
    }

    FSuspenseCoreEventDataBuilder& String(FName Key, const FString& Value)
    {
        Data.SetString(Key, Value);
        return *this;
    }

    FSuspenseCoreEventDataBuilder& Float(FName Key, float Value)
    {
        Data.SetFloat(Key, Value);
        return *this;
    }

    FSuspenseCoreEventDataBuilder& Int(FName Key, int32 Value)
    {
        Data.SetInt(Key, Value);
        return *this;
    }

    FSuspenseCoreEventDataBuilder& Bool(FName Key, bool Value)
    {
        Data.SetBool(Key, Value);
        return *this;
    }

    FSuspenseCoreEventDataBuilder& Object(FName Key, UObject* Value)
    {
        Data.SetObject(Key, Value);
        return *this;
    }

    FSuspenseCoreEventDataBuilder& Tag(FGameplayTag T)
    {
        Data.AddTag(T);
        return *this;
    }

    FSuspenseCoreEventData Build() { return Data; }

private:
    FSuspenseCoreEventData Data;
};

// Usage:
// auto EventData = FSuspenseCoreEventDataBuilder(this)
//     .Priority(ESuspenseCoreEventPriority::High)
//     .Float(TEXT("Damage"), 100.0f)
//     .Object(TEXT("Instigator"), AttackingActor)
//     .Build();
```

---

*Документ создан: 2025-11-29*
*Naming Conventions: FSuspenseCore*, ESuspenseCore**
