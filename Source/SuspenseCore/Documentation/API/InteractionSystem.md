# InteractionSystem API Reference

**Модуль:** InteractionSystem
**Версия:** 2.0.0
**Статус:** Полностью мигрирован, компилируется
**LOC:** ~3,486
**Последнее обновление:** 2025-11-28

---

## Обзор модуля

InteractionSystem — компактный модуль для взаимодействия с объектами мира через GAS. Включает компонент взаимодействия и систему pickup предметов.

### Архитектурная роль

InteractionSystem предоставляет:

1. **Interaction Component** — trace-based поиск и взаимодействие через GAS
2. **Pickup System** — подбираемые предметы с DataTable интеграцией
3. **Interface Contracts** — ISuspenseInteract, ISuspensePickup

### Зависимости модуля

```
InteractionSystem
├── Core
├── Niagara (VFX)
├── GameplayAbilities
├── GameplayTags
├── DeveloperSettings
└── BridgeSystem
```

**Private зависимости:** CoreUObject, Engine, Slate, SlateCore

---

## Ключевые компоненты

### 1. USuspenseInteractionComponent

**Файл:** `Public/Components/SuspenseInteractionComponent.h`
**Тип:** UActorComponent
**Назначение:** Компонент для trace-based взаимодействия через GAS

```cpp
UCLASS(ClassGroup = (Suspense), meta = (BlueprintSpawnableComponent))
class INTERACTIONSYSTEM_API USuspenseInteractionComponent : public UActorComponent
```

#### Основное API

```cpp
// Начать взаимодействие с текущим объектом
void StartInteraction();

// Проверить возможность взаимодействия
bool CanInteractNow() const;

// Trace для UI (highlight, prompt)
AActor* PerformUIInteractionTrace() const;

// Получить текущий focused объект
AActor* GetFocusedInteractable() const;
```

#### Settings

```cpp
// Дистанция trace
float TraceDistance = 300.0f;

// Collision channel для trace
TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

// Debug визуализация
bool bEnableDebugTrace = false;

// Cooldown между взаимодействиями
float InteractionCooldown = 0.5f;
```

#### GameplayTags

```cpp
// Тег способности взаимодействия
FGameplayTag InteractAbilityTag;

// Тег успешного взаимодействия
FGameplayTag InteractSuccessTag;

// Тег неудачного взаимодействия
FGameplayTag InteractFailedTag;

// Теги, блокирующие взаимодействие
FGameplayTagContainer BlockingTags;
```

#### Events

```cpp
// Успешное взаимодействие
UPROPERTY(BlueprintAssignable)
FInteractionSucceededDelegate OnInteractionSucceeded;

// Неудачное взаимодействие
UPROPERTY(BlueprintAssignable)
FInteractionFailedDelegate OnInteractionFailed;

// Изменился тип взаимодействия (для UI)
UPROPERTY(BlueprintAssignable)
FInteractionTypeChangedDelegate OnInteractionTypeChanged;
```

#### Паттерн использования

```cpp
// На Character в BeginPlay
InteractionComponent = CreateDefaultSubobject<USuspenseInteractionComponent>(TEXT("Interaction"));
InteractionComponent->InteractAbilityTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Interact"));
InteractionComponent->TraceDistance = 250.0f;

// В Input Handler
void AMyCharacter::OnInteractPressed()
{
    if (InteractionComponent && InteractionComponent->CanInteractNow())
    {
        InteractionComponent->StartInteraction();
    }
}

// Подписка на события
InteractionComponent->OnInteractionSucceeded.AddDynamic(this, &AMyCharacter::HandleInteractionSuccess);
```

---

### 2. ASuspensePickupItem

**Файл:** `Public/Pickup/SuspensePickupItem.h`
**Тип:** AActor
**Назначение:** Базовый pickup actor для подбираемых предметов

```cpp
UCLASS(Blueprintable)
class INTERACTIONSYSTEM_API ASuspensePickupItem
    : public AActor
    , public ISuspenseInteract
    , public ISuspensePickup
```

#### Components

```cpp
USphereComponent* SphereCollision;      // Collision для взаимодействия
UStaticMeshComponent* MeshComponent;    // Визуальный меш
UNiagaraComponent* SpawnVFXComponent;   // VFX при спавне
UAudioComponent* AudioComponent;        // Звуки
```

#### Item Reference

```cpp
// Единственная ссылка на статические данные
FName ItemID;                           // ID для DataTable lookup
int32 Amount;                           // Количество

// Полный runtime instance (для dropped items)
FSuspenseInventoryItemInstance RuntimeInstance;
bool bUseRuntimeInstance = false;
```

#### Preset Properties

Система предустановленных свойств для спавна:

```cpp
USTRUCT(BlueprintType)
struct FPresetPropertyPair
{
    FName PropertyName;     // e.g., "Durability", "Ammo"
    float PropertyValue;
};

// Массив свойств (TArray для репликации)
TArray<FPresetPropertyPair> PresetRuntimeProperties;
```

**Preset API:**
```cpp
float GetPresetProperty(FName PropertyName, float DefaultValue = 0.0f) const;
void SetPresetProperty(FName PropertyName, float Value);
bool HasPresetProperty(FName PropertyName) const;
bool RemovePresetProperty(FName PropertyName);
TMap<FName, float> GetPresetPropertiesAsMap() const;
void SetPresetPropertiesFromMap(const TMap<FName, float>& NewProperties);
```

#### Weapon State

```cpp
bool bHasSavedAmmoState;    // Есть ли сохранённое состояние боеприпасов
float SavedCurrentAmmo;     // Патроны в магазине
float SavedRemainingAmmo;   // Оставшиеся патроны
```

#### ISuspenseInteract Implementation

```cpp
bool CanInteract_Implementation(APlayerController* Instigator) const;
bool Interact_Implementation(APlayerController* Instigator);
FGameplayTag GetInteractionType_Implementation() const;
FText GetInteractionText_Implementation() const;
int32 GetInteractionPriority_Implementation() const;
float GetInteractionDistance_Implementation() const;
void OnInteractionFocusGained_Implementation(APlayerController* Instigator);
void OnInteractionFocusLost_Implementation(APlayerController* Instigator);
```

#### ISuspensePickup Implementation

```cpp
FName GetItemID_Implementation() const;
void SetItemID_Implementation(FName NewItemID);
bool GetUnifiedItemData_Implementation(FSuspenseUnifiedItemData& OutData) const;
int32 GetItemAmount_Implementation() const;
void SetAmount_Implementation(int32 NewAmount);
bool HasSavedAmmoState_Implementation() const;
bool GetSavedAmmoState_Implementation(float& OutCurrent, float& OutRemaining) const;
void SetSavedAmmoState_Implementation(float Current, float Remaining);
bool HandlePickedUp_Implementation(AActor* Instigator);
bool CanBePickedUpBy_Implementation(AActor* Instigator) const;
FGameplayTag GetItemType_Implementation() const;
bool CreateItemInstance_Implementation(FSuspenseInventoryItemInstance& OutInstance) const;
FGameplayTag GetItemRarity_Implementation() const;
FText GetDisplayName_Implementation() const;
bool IsStackable_Implementation() const;
float GetItemWeight_Implementation() const;
```

#### Инициализация

```cpp
// Из полного runtime instance
void InitializeFromInstance(const FSuspenseInventoryItemInstance& Instance);

// Из spawn data
void InitializeFromSpawnData(const FSuspensePickupSpawnData& SpawnData);

// Установка ammo state
void SetAmmoState(bool bHasState, float CurrentAmmo, float RemainingAmmo);
```

---

## Интерфейсы

### ISuspenseInteract

**Файл:** `BridgeSystem/Public/Interfaces/Interaction/ISuspenseInteract.h`

Контракт для интерактивных объектов:

```cpp
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseInteract : public UInterface
{
    GENERATED_BODY()
};

class ISuspenseInteract
{
    GENERATED_BODY()

public:
    // Может ли быть выполнено взаимодействие
    virtual bool CanInteract_Implementation(APlayerController* Instigator) const = 0;

    // Выполнить взаимодействие
    virtual bool Interact_Implementation(APlayerController* Instigator) = 0;

    // Тип взаимодействия (для UI)
    virtual FGameplayTag GetInteractionType_Implementation() const = 0;

    // Текст подсказки
    virtual FText GetInteractionText_Implementation() const = 0;

    // Приоритет (при нескольких объектах)
    virtual int32 GetInteractionPriority_Implementation() const = 0;

    // Дистанция взаимодействия
    virtual float GetInteractionDistance_Implementation() const = 0;

    // Callbacks при фокусе
    virtual void OnInteractionFocusGained_Implementation(APlayerController* Instigator) = 0;
    virtual void OnInteractionFocusLost_Implementation(APlayerController* Instigator) = 0;
};
```

### ISuspensePickup

**Файл:** `BridgeSystem/Public/Interfaces/Interaction/ISuspensePickup.h`

Специализированный контракт для подбираемых предметов:

```cpp
class ISuspensePickup
{
    GENERATED_BODY()

public:
    // Item identity
    virtual FName GetItemID_Implementation() const = 0;
    virtual void SetItemID_Implementation(FName NewItemID) = 0;
    virtual bool GetUnifiedItemData_Implementation(FSuspenseUnifiedItemData& OutData) const = 0;

    // Quantity
    virtual int32 GetItemAmount_Implementation() const = 0;
    virtual void SetAmount_Implementation(int32 NewAmount) = 0;

    // Weapon state
    virtual bool HasSavedAmmoState_Implementation() const = 0;
    virtual bool GetSavedAmmoState_Implementation(float& OutCurrent, float& OutRemaining) const = 0;
    virtual void SetSavedAmmoState_Implementation(float Current, float Remaining) = 0;

    // Pickup handling
    virtual bool HandlePickedUp_Implementation(AActor* Instigator) = 0;
    virtual bool CanBePickedUpBy_Implementation(AActor* Instigator) const = 0;

    // Item properties
    virtual FGameplayTag GetItemType_Implementation() const = 0;
    virtual bool CreateItemInstance_Implementation(FSuspenseInventoryItemInstance& OutInstance) const = 0;
    virtual FGameplayTag GetItemRarity_Implementation() const = 0;
    virtual FText GetDisplayName_Implementation() const = 0;
    virtual bool IsStackable_Implementation() const = 0;
    virtual float GetItemWeight_Implementation() const = 0;
};
```

---

## Паттерны использования

### Создание pickup в мире

```cpp
// Blueprint-способ: разместить BP_PickupItem в уровне и настроить ItemID

// C++ способ
ASuspensePickupItem* Pickup = World->SpawnActor<ASuspensePickupItem>(PickupClass, Location, Rotation);
Pickup->SetItemID_Implementation(TEXT("Weapon_Rifle"));
Pickup->SetAmount_Implementation(1);

// С preset properties (поврежденное оружие)
Pickup->SetPresetProperty(TEXT("Durability"), 50.0f);

// С ammo state (не полный магазин)
Pickup->SetAmmoState(true, 15.0f, 60.0f);
```

### Инициализация из dropped item

```cpp
// При выбрасывании предмета из инвентаря
FSuspenseInventoryItemInstance DroppedItem = InventoryComponent->RemoveItemFromSlot(SlotIndex);

ASuspensePickupItem* Pickup = World->SpawnActor<ASuspensePickupItem>(PickupClass, DropLocation, FRotator::ZeroRotator);
Pickup->InitializeFromInstance(DroppedItem); // Сохраняет все runtime свойства
```

### Обработка подбора

```cpp
// В ISuspensePickup::HandlePickedUp_Implementation
bool ASuspensePickupItem::HandlePickedUp_Implementation(AActor* Instigator)
{
    // Создаём instance для инвентаря
    FSuspenseInventoryItemInstance NewInstance;
    if (!CreateItemInstance_Implementation(NewInstance))
    {
        return false;
    }

    // Добавляем в инвентарь
    if (!TryAddToInventory(Instigator))
    {
        return false;
    }

    // Broadcast event
    BroadcastPickupCollected(Instigator);

    // Уничтожаем pickup
    SetLifeSpan(DestroyDelay);
    return true;
}
```

---

## Репликация

### Replicated Properties

В ASuspensePickupItem:
```cpp
FName ItemID;                           // Replicated
int32 Amount;                           // Replicated
bool bUseRuntimeInstance;               // Replicated
TArray<FPresetPropertyPair> PresetRuntimeProperties; // Replicated
bool bHasSavedAmmoState;                // Replicated
float SavedCurrentAmmo;                 // Replicated
float SavedRemainingAmmo;               // Replicated
```

**Важно:** `FSuspenseInventoryItemInstance RuntimeInstance` НЕ реплицируется напрямую из-за TMap. Вместо этого используется `PresetRuntimeProperties` (TArray).

---

## Риски и потенциальные проблемы

### 1. TMap не реплицируется

**Проблема:** RuntimeInstance.RuntimeProperties (TMap) не реплицируется.

**Последствия:**
- При dropped items с custom properties клиент не видит полное состояние
- Workaround через TArray<FPresetPropertyPair>

**Рекомендации:**
- Документировать ограничение
- Рассмотреть полную замену TMap на TArray в FSuspenseInventoryItemInstance

### 2. Отсутствие Object Pooling

**Проблема:** Pickup actors создаются и уничтожаются постоянно.

**Последствия:**
- GC spikes при массовом лутинге
- Потенциальные memory allocations

**Рекомендации:**
- Реализовать pickup pool
- Hide/recycle вместо destroy

### 3. Trace каждый Tick (в UI режиме)

**Проблема:** PerformUIInteractionTrace() может вызываться каждый кадр для highlight.

**Последствия:**
- CPU overhead
- Потенциальные performance issues при большом количестве объектов

**Рекомендации:**
- Добавить rate limiting для trace
- Использовать overlap events вместо trace для близких объектов

### 4. Жёсткая связь с GAS

**Проблема:** InteractionComponent требует ASC для активации способностей.

**Последствия:**
- Невозможность использования без GAS
- Усложнённое тестирование

**Рекомендации:**
- Добавить fallback для non-GAS режима
- Создать mock ASC для тестов

### 5. Single Trace Channel

**Проблема:** Используется один TraceChannel для всех взаимодействий.

**Последствия:**
- Невозможность фильтрации по типу объекта
- Потенциальные false positives

**Рекомендации:**
- Добавить object type query
- Использовать multi-trace с фильтрацией

---

## Метрики модуля

| Метрика | Значение |
|---------|----------|
| Файлов (.h) | 6 |
| Компонентов | 1 |
| Actors | 1 |
| Интерфейсов | 2 |
| LOC (приблизительно) | 3,486 |

---

## Связь с другими модулями

```
┌─────────────┐       ┌─────────────────┐
│ PlayerCore  │       │InteractionSystem│
│ (Character) │──────►│  (Component)    │
└─────────────┘       └────────┬────────┘
                               │ uses
                               ▼
                      ┌─────────────────┐
                      │  BridgeSystem   │
                      │ (ISuspensePickup│
                      │  ISuspenseInter)│
                      └────────┬────────┘
                               │
                               ▼
                      ┌─────────────────┐
                      │InventorySystem │
                      │ (AddItem)       │
                      └─────────────────┘
```

**InteractionSystem зависит от:**
- **BridgeSystem** — интерфейсы, EventManager
- **GAS** — GameplayAbility для взаимодействия

**От InteractionSystem зависят:**
- **InventorySystem** — pickup вызывает AddItem

---

**Дата создания:** 2025-11-28
**Автор:** Tech Lead
**Версия документа:** 1.0
