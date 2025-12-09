# InventorySystem API Reference

**Модуль:** InventorySystem
**Версия:** 2.0.0
**Статус:** Полностью мигрирован, компилируется
**LOC:** ~27,862
**Последнее обновление:** 2025-11-28

---

## Обзор модуля

InventorySystem — модуль grid-based инвентаря с поддержкой DataTable как единого источника истины для данных предметов. Поддерживает transaction-based операции, репликацию, GAS интеграцию и UI binding.

### Архитектурная роль

InventorySystem реализует следующую функциональность:

1. **Grid-based Storage** — сетка ячеек с поддержкой предметов различных размеров
2. **Instance-based Architecture** — работа с FSuspenseInventoryItemInstance вместо UObject*
3. **Transaction Support** — атомарные операции с rollback
4. **DataTable Integration** — все статические данные из единого источника
5. **Replication** — полная поддержка multiplayer

### Зависимости модуля

```
InventorySystem
├── Core, CoreUObject, Engine
├── GameplayTags
├── BridgeSystem
└── InteractionSystem
```

**Private зависимости:** Slate, SlateCore, NetCore, GameplayAbilities, Json, JsonUtilities

---

## Ключевые компоненты

### 1. USuspenseInventoryComponent

**Файл:** `Public/Components/SuspenseInventoryComponent.h`
**Тип:** UActorComponent
**Назначение:** Главный компонент для управления инвентарём

```cpp
UCLASS(ClassGroup=(Suspense), meta=(BlueprintSpawnableComponent))
class INVENTORYSYSTEM_API USuspenseInventoryComponent
    : public UActorComponent
    , public ISuspenseInventory
```

#### Внутренние sub-components

```cpp
USuspenseInventoryStorage* StorageComponent;       // Хранение предметов
USuspenseInventoryValidator* ConstraintsComponent; // Валидация операций
USuspenseInventoryTransaction* TransactionComponent; // Транзакции
USuspenseInventoryReplicator* ReplicatorComponent; // Репликация
USuspenseInventoryEvents* EventsComponent;         // События
USuspenseInventorySerializer* SerializerComponent; // Сериализация
USuspenseInventoryUIConnector* UIAdapter;          // UI binding
USuspenseInventoryGASIntegration* GASIntegration;  // GAS связь
```

#### Инициализация

```cpp
// Из Loadout (рекомендуется)
bool InitializeFromLoadout(const FName& LoadoutID, const FName& InventoryName = NAME_None);

// Простая инициализация
bool InitializeWithSimpleSettings(int32 Width, int32 Height, float MaxWeightLimit,
    const FGameplayTagContainer& AllowedTypes = FGameplayTagContainer());

// Проверка инициализации
bool IsInventoryInitialized() const;
bool IsInitializedFromLoadout() const;
FName GetCurrentLoadoutID() const;
```

#### Core Operations

```cpp
// Добавление предметов
bool AddItemByID_Implementation(FName ItemID, int32 Quantity);
FSuspenseInventoryOperationResult AddItemInstance(const FSuspenseInventoryItemInstance& ItemInstance);
FSuspenseInventoryOperationResult AddItemInstanceToSlot(const FSuspenseInventoryItemInstance& ItemInstance, int32 TargetSlot);

// Удаление предметов
FSuspenseInventoryOperationResult RemoveItemByID(const FName& ItemID, int32 Amount);
FSuspenseInventoryOperationResult RemoveItemInstance(const FGuid& InstanceID);
bool RemoveItemFromSlot(int32 SlotIndex, FSuspenseInventoryItemInstance& OutRemovedInstance);

// Перемещение
bool MoveItemBySlots_Implementation(int32 FromSlot, int32 ToSlot, bool bMaintainRotation);
bool MoveItemInstance(const FGuid& InstanceID, int32 NewSlot, bool bAllowRotation = false);

// Операции со стеками
int32 ConsolidateStacks(const FName& ItemID = NAME_None);
FSuspenseInventoryOperationResult SplitStack(int32 SourceSlot, int32 SplitQuantity, int32 TargetSlot);
```

#### Grid Operations

```cpp
// Размещение
int32 FindFreeSpaceForItem(const FVector2D& ItemSize, bool bAllowRotation = true) const;
bool CanPlaceItemAtSlot(const FVector2D& ItemSize, int32 SlotIndex, bool bIgnoreRotation = false) const;
bool PlaceItemInstanceAtSlot(const FSuspenseInventoryItemInstance& ItemInstance, int32 SlotIndex, bool bForcePlace = false);
bool TryAutoPlaceItemInstance(const FSuspenseInventoryItemInstance& ItemInstance);

// Swap и Rotate
bool SwapItemsInSlots(int32 Slot1, int32 Slot2, ESuspenseInventoryErrorCode& OutErrorCode);
bool CanSwapSlots(int32 Slot1, int32 Slot2) const;
bool RotateItemAtSlot(int32 SlotIndex);
bool CanRotateItemAtSlot(int32 SlotIndex) const;

// Координаты
FVector2D GetInventorySize() const;
bool GetInventoryCoordinates(int32 Index, int32& OutX, int32& OutY) const;
int32 GetIndexFromCoordinates(int32 X, int32 Y) const;
TArray<int32> GetOccupiedSlots(int32 AnchorIndex, const FVector2D& ItemSize, bool bIsRotated) const;
```

#### Weight Management

```cpp
float GetCurrentWeight_Implementation() const;
float GetMaxWeight_Implementation() const;
float GetRemainingWeight_Implementation() const;
bool HasWeightCapacity_Implementation(float RequiredWeight) const;
void SetMaxWeight(float NewMaxWeight);
```

#### Queries

```cpp
bool GetItemInstanceAtSlot(int32 SlotIndex, FSuspenseInventoryItemInstance& OutInstance) const;
TArray<FSuspenseInventoryItemInstance> GetAllItemInstances() const;
int32 GetItemCountByID(const FName& ItemID) const;
TArray<FSuspenseInventoryItemInstance> FindItemInstancesByType(const FGameplayTag& ItemType) const;
int32 GetTotalItemCount() const;
bool HasItem(const FName& ItemID, int32 Amount = 1) const;
```

#### Transaction Support

```cpp
void BeginTransaction();
void CommitTransaction();
void RollbackTransaction();
bool IsTransactionActive() const;
```

#### Replication

```cpp
// Replicated Properties
FVector2D ReplicatedGridSize;       // Grid size для клиентов
bool bIsInitializedReplicated;      // Состояние инициализации
float MaxWeight;                    // Лимит веса
float CurrentWeight;                // Текущий вес
FGameplayTagContainer AllowedItemTypes; // Разрешённые типы
FName CurrentLoadoutID;             // ID лоадаута

// RPCs
void Server_AddItemByID(const FName& ItemID, int32 Amount);
void Server_RemoveItem(const FName& ItemID, int32 Amount);
void Server_UpdateInventoryState();
```

---

### 2. USuspenseInventoryStorage

**Файл:** `Public/Storage/SuspenseInventoryStorage.h`
**Тип:** UActorComponent
**Назначение:** Низкоуровневое хранилище предметов в grid-структуре

```cpp
UCLASS(ClassGroup=(Suspense), meta=(BlueprintSpawnableComponent))
class INVENTORYSYSTEM_API USuspenseInventoryStorage : public UActorComponent
```

#### Grid Management

```cpp
bool InitializeGrid(int32 Width, int32 Height, float MaxWeight = 0.0f);
bool IsInitialized() const;
FVector2D GetGridSize() const;
int32 GetTotalCells() const;
int32 GetFreeCellCount() const;
```

#### Item Instance Operations

```cpp
bool AddItemInstance(const FSuspenseInventoryItemInstance& ItemInstance, bool bAllowRotation = true);
bool RemoveItemInstance(const FGuid& InstanceID);
bool GetItemInstance(const FGuid& InstanceID, FSuspenseInventoryItemInstance& OutInstance) const;
TArray<FSuspenseInventoryItemInstance> GetAllItemInstances() const;
bool UpdateItemInstance(const FSuspenseInventoryItemInstance& UpdatedInstance);
```

#### Placement

```cpp
int32 FindFreeSpace(const FName& ItemID, bool bAllowRotation = true, bool bOptimizeFragmentation = true) const;
bool AreCellsFreeForItem(int32 StartIndex, const FName& ItemID, bool bIsRotated = false) const;
bool PlaceItemInstance(const FSuspenseInventoryItemInstance& ItemInstance, int32 AnchorIndex);
bool MoveItem(const FGuid& InstanceID, int32 NewAnchorIndex, bool bAllowRotation = false);
```

#### Maintenance

```cpp
void ClearAllItems();
bool ValidateStorageIntegrity(TArray<FString>& OutErrors, bool bAutoFix = false);
int32 DefragmentStorage();
FString GetStorageDebugInfo() const;
```

#### Internal Structure

```cpp
// Grid cells с информацией о размещении
TArray<FInventoryCell> Cells;

// Runtime instances в storage
TArray<FSuspenseInventoryItemInstance> StoredInstances;

// Bitmap для быстрого поиска свободных ячеек
TBitArray<> FreeCellsBitmap;

// Transaction для atomic операций
FSuspenseStorageTransaction ActiveTransaction;
```

---

### 3. USuspenseInventoryValidator

**Файл:** `Public/Validation/SuspenseInventoryValidator.h`
**Тип:** UObject
**Назначение:** Валидация всех операций инвентаря

```cpp
UCLASS(BlueprintType)
class INVENTORYSYSTEM_API USuspenseInventoryValidator : public UObject
```

#### Инициализация

```cpp
void Initialize(float InMaxWeight, const FGameplayTagContainer& InAllowedTypes,
    int32 InGridWidth, int32 InGridHeight, USuspenseItemManager* InItemManager = nullptr);

bool InitializeFromLoadout(const FName& LoadoutID, const FName& InventoryName, const UObject* WorldContext);
```

#### Unified Data Validation

```cpp
FSuspenseInventoryOperationResult ValidateUnifiedItemData(
    const FSuspenseUnifiedItemData& ItemData, int32 Amount, const FName& FunctionName) const;

FSuspenseInventoryOperationResult ValidateUnifiedItemDataWithRestrictions(
    const FSuspenseUnifiedItemData& ItemData, int32 Amount, const FName& FunctionName) const;
```

#### Instance Validation

```cpp
FSuspenseInventoryOperationResult ValidateItemInstance(
    const FSuspenseInventoryItemInstance& ItemInstance, const FName& FunctionName) const;

int32 ValidateItemInstances(const TArray<FSuspenseInventoryItemInstance>& ItemInstances,
    const FName& FunctionName, TArray<FSuspenseInventoryItemInstance>& OutFailedInstances) const;

FSuspenseInventoryOperationResult ValidateRuntimeProperties(
    const FSuspenseInventoryItemInstance& ItemInstance, const FName& FunctionName) const;
```

#### Spatial Validation

```cpp
FSuspenseInventoryOperationResult ValidateSlotIndex(int32 SlotIndex, const FName& FunctionName) const;

FSuspenseInventoryOperationResult ValidateGridBoundsForUnified(
    const FSuspenseUnifiedItemData& ItemData, int32 AnchorIndex, bool bIsRotated, const FName& FunctionName) const;

FSuspenseInventoryOperationResult ValidateItemPlacement(
    const FSuspenseUnifiedItemData& ItemData, int32 AnchorIndex, bool bIsRotated,
    const TArray<bool>& OccupiedSlots, const FName& FunctionName) const;
```

#### Weight Validation

```cpp
FSuspenseInventoryOperationResult ValidateWeightForUnified(
    const FSuspenseUnifiedItemData& ItemData, int32 Amount, float CurrentWeight, const FName& FunctionName) const;

bool WouldExceedWeightLimitUnified(
    const FSuspenseUnifiedItemData& ItemData, int32 Amount, float CurrentWeight) const;
```

#### Type Checking

```cpp
bool IsItemTypeAllowed(const FGameplayTag& ItemType) const;
bool IsItemAllowedByAllCriteria(const FSuspenseUnifiedItemData& ItemData) const;
```

---

### 4. USuspenseInventoryEvents

**Файл:** `Public/Events/SuspenseInventoryEvents.h`
**Тип:** UActorComponent
**Назначение:** Event система для inventory operations

```cpp
UCLASS(ClassGroup=(Suspense), meta=(BlueprintSpawnableComponent))
class INVENTORYSYSTEM_API USuspenseInventoryEvents : public UActorComponent
```

#### События

| Делегат | Параметры | Описание |
|---------|-----------|----------|
| `OnInventoryInitialized` | — | Инвентарь инициализирован |
| `OnWeightChanged` | `float NewWeight` | Изменился вес |
| `OnLockStateChanged` | `bool bLocked` | Изменился lock state |
| `OnItemAdded` | `FName ItemID, int32 Amount` | Добавлен предмет |
| `OnItemRemoved` | `FName ItemID, int32 Amount` | Удалён предмет |
| `OnItemMoved` | `FGuid InstanceID, FName ItemID, int32 FromSlot, int32 ToSlot` | Перемещён предмет |
| `OnItemStacked` | `FGuid SourceID, FGuid TargetID, int32 Amount` | Стеки объединены |
| `OnItemSplit` | `FGuid SourceID, FGuid NewID, int32 Amount, int32 NewSlot` | Стек разделён |
| `OnItemSwapped` | `FGuid FirstID, FGuid SecondID, int32 FirstSlot, int32 SecondSlot` | Предметы поменялись местами |
| `OnItemRotated` | `FGuid InstanceID, int32 SlotIndex, bool bRotated` | Предмет повёрнут |

#### Broadcast Methods

```cpp
void BroadcastInitialized();
void BroadcastWeightChanged(float NewWeight);
void BroadcastItemAdded(FName ItemID, int32 Amount);
void BroadcastItemMoved(const FGuid& InstanceID, const FName& ItemID, int32 FromSlot, int32 ToSlot);
void BroadcastItemStacked(const FGuid& SourceInstanceID, const FGuid& TargetInstanceID, int32 TransferredAmount);
void BroadcastItemSplit(const FGuid& SourceInstanceID, const FGuid& NewInstanceID, int32 SplitAmount, int32 NewSlot);
```

---

## Ключевые типы данных

### FSuspenseInventoryItemInstance

**Файл:** `BridgeSystem/Public/Types/Inventory/SuspenseInventoryTypes.h`

```cpp
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseInventoryItemInstance
{
    FName ItemID;                           // ID для DataTable lookup
    FGuid InstanceID;                       // Уникальный runtime ID
    int32 Quantity;                         // Количество в стеке
    TMap<FName, float> RuntimeProperties;  // Динамические свойства
    int32 AnchorIndex;                      // Позиция в инвентаре
    bool bIsRotated;                        // Повёрнут ли
    float LastUsedTime;                     // Время последнего использования
};
```

### ESuspenseInventoryErrorCode

```cpp
enum class ESuspenseInventoryErrorCode : uint8
{
    Success,
    NoSpace,
    WeightLimit,
    InvalidItem,
    ItemNotFound,
    InsufficientQuantity,
    InvalidSlot,
    SlotOccupied,
    TransactionActive,
    NotInitialized,
    NetworkError,
    UnknownError
};
```

### FSuspenseInventoryOperationResult

```cpp
USTRUCT(BlueprintType)
struct FSuspenseInventoryOperationResult
{
    bool bSuccess;
    ESuspenseInventoryErrorCode ErrorCode;
    FText ErrorMessage;
    FGuid AffectedInstanceID;
    int32 AffectedSlot;
    int32 AffectedQuantity;
};
```

---

## Паттерны использования

### Добавление предмета в инвентарь

```cpp
// Простой способ - по ID из DataTable
if (InventoryComponent->AddItemByID_Implementation(TEXT("Weapon_Rifle"), 1))
{
    UE_LOG(LogTemp, Log, TEXT("Item added successfully"));
}

// С детальной информацией об ошибке
FSuspenseInventoryOperationResult Result = InventoryComponent->AddItemInstance(ItemInstance);
if (!Result.bSuccess)
{
    UE_LOG(LogTemp, Warning, TEXT("Failed to add item: %s"), *Result.ErrorMessage.ToString());
}
```

### Перемещение предмета

```cpp
// Перемещение между слотами
if (InventoryComponent->MoveItemBySlots_Implementation(FromSlot, ToSlot, false))
{
    // Успешно перемещено
}

// Перемещение по InstanceID
if (InventoryComponent->MoveItemInstance(InstanceID, NewSlot, true))
{
    // Перемещено с возможной ротацией
}
```

### Транзакции

```cpp
// Начало транзакции
InventoryComponent->BeginTransaction();

// Серия операций
bool bSuccess = true;
bSuccess &= InventoryComponent->MoveItemBySlots_Implementation(0, 5, false);
bSuccess &= InventoryComponent->MoveItemBySlots_Implementation(1, 6, false);
bSuccess &= InventoryComponent->MoveItemBySlots_Implementation(2, 7, false);

if (bSuccess)
{
    InventoryComponent->CommitTransaction();
}
else
{
    InventoryComponent->RollbackTransaction();
}
```

### Инициализация из Loadout

```cpp
// В PlayerState::BeginPlay()
if (InventoryComponent && HasAuthority())
{
    if (InventoryComponent->InitializeFromLoadout(TEXT("Default_Soldier"), TEXT("MainInventory")))
    {
        UE_LOG(LogTemp, Log, TEXT("Inventory initialized from loadout"));
    }
}
```

### Подписка на события

```cpp
// Blueprint Assignable
InventoryComponent->GetEventsComponent()->OnItemAdded.AddDynamic(this, &UMyWidget::HandleItemAdded);

// Или напрямую
InventoryComponent->BindToInventoryUpdates(FOnInventoryUpdated::FDelegate::CreateUObject(
    this, &UMyClass::OnInventoryChanged));
```

---

## Репликация

### Стратегия репликации

1. **Server Authority** — все изменения происходят на сервере
2. **State Replication** — grid size, weight, allowed types реплицируются
3. **Delta Updates** — через ReplicatorComponent передаются только изменения
4. **Client Prediction** — не поддерживается (server-authoritative)

### RPC Flow

```
Client                           Server
  │                                │
  │─────► Server_AddItemByID ─────►│
  │                                │
  │◄──── OnRep_InventoryState ◄────│
  │                                │
  │◄──── Replicator Updates ◄──────│
```

---

## Риски и потенциальные проблемы

### 1. Большое количество sub-components

**Проблема:** USuspenseInventoryComponent содержит 8 внутренних компонентов.

**Последствия:**
- Высокий memory footprint
- Сложная инициализация
- Потенциальные race conditions между компонентами

**Рекомендации:**
- Рассмотреть lazy initialization для редко используемых компонентов
- Документировать порядок инициализации
- Добавить dependency graph между компонентами

### 2. Transaction Timeout

**Проблема:** FSuspenseStorageTransaction имеет timeout 30 секунд (hardcoded).

**Последствия:**
- Длительные операции могут fail
- Нет конфигурируемости таймаута
- Потенциальные утечки при crash во время транзакции

**Рекомендации:**
- Сделать timeout конфигурируемым
- Добавить автоматический cleanup orphaned транзакций
- Логировать длительные транзакции

### 3. TBitArray для FreeCellsBitmap

**Проблема:** FreeCellsBitmap пересоздаётся при каждом изменении.

**Последствия:**
- O(n) для обновления при каждой операции
- Потенциальные allocation spikes

**Рекомендации:**
- Рассмотреть инкрементальное обновление bitmap
- Профилировать влияние на performance при большом инвентаре

### 4. Отсутствие Item Pooling

**Проблема:** FSuspenseInventoryItemInstance создаётся каждый раз заново.

**Последствия:**
- Memory churn при частых операциях
- Потенциальная фрагментация памяти

**Рекомендации:**
- Рассмотреть object pool для часто создаваемых instances
- Предаллоцировать массивы для известного количества слотов

### 5. Coupling с USuspenseItemManager

**Проблема:** Validator и Storage требуют ItemManager для работы.

**Последствия:**
- Невозможность работы без GameInstance subsystem
- Сложность unit-тестирования

**Рекомендации:**
- Добавить mock ItemManager для тестов
- Рассмотреть интерфейс IItemDataProvider

### 6. Нет undo/redo stack

**Проблема:** История операций не сохраняется после commit.

**Последствия:**
- Невозможность отмены действий игрока
- Нет audit trail для debugging

**Рекомендации:**
- Добавить опциональный history ring buffer
- Логировать критичные операции

### 7. JSON Serialization в Private Dependencies

**Проблема:** Json/JsonUtilities в private зависимостях.

**Последствия:**
- Невозможность использовать serializer из других модулей
- Потенциальные проблемы с forward declarations

**Рекомендации:**
- Вынести serializer в public interface
- Или создать отдельный модуль для сериализации

---

## Метрики модуля

| Метрика | Значение |
|---------|----------|
| Файлов (.h) | 24 |
| Компонентов | 8+ |
| Делегатов событий | 10 |
| Операций | 30+ |
| LOC (приблизительно) | 27,862 |

---

## Связь с другими модулями

```
┌──────────────────┐
│    PlayerCore    │
│  (PlayerState)   │
└────────┬─────────┘
         │ owns
         ▼
┌──────────────────┐         ┌─────────────────┐
│InventorySystem  │◄────────│  BridgeSystem   │
│ (Component)     │         │ (Types, Events) │
└────────┬─────────┘         └─────────────────┘
         │ uses                      ▲
         ▼                           │
┌──────────────────┐                 │
│InteractionSystem │─────────────────┘
│  (Pickups)       │
└──────────────────┘
         │
         ▼
┌──────────────────┐
│EquipmentSystem  │
│ (Slots, Bridge)  │
└──────────────────┘
```

**InventorySystem зависит от:**
- **BridgeSystem** — типы данных, EventManager
- **InteractionSystem** — pickup actors

**От InventorySystem зависят:**
- **PlayerCore** — владеет InventoryComponent
- **EquipmentSystem** — использует items из инвентаря

---

**Дата создания:** 2025-11-28
**Автор:** Tech Lead
**Версия документа:** 1.0
