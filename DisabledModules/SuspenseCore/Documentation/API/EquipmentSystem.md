# EquipmentSystem API Reference

**Модуль:** EquipmentSystem
**Версия:** 2.0.0
**Статус:** Полностью мигрирован, компилируется
**LOC:** ~54,213 (крупнейший модуль)
**Последнее обновление:** 2025-11-28

---

## Обзор модуля

EquipmentSystem — модуль управления экипировкой и оружием. Реализует слотовую систему экипировки, state machine оружия, transaction-based операции и полную сетевую репликацию.

### Архитектурная роль

EquipmentSystem — наиболее сложный модуль проекта с многоуровневой архитектурой:

1. **Slot Management** — конфигурируемые слоты экипировки (Primary, Secondary, Holster, Scabbard)
2. **Weapon FSM** — finite state machine для состояний оружия
3. **Transaction System** — ACID-транзакции для операций экипировки
4. **Rules Engine** — модульные правила валидации (совместимость, вес, конфликты)
5. **Prediction System** — client-side prediction для сетевых операций
6. **Visual System** — управление визуализацией экипировки

### Зависимости модуля

```
EquipmentSystem
├── Core, CoreUObject, Engine
├── GameplayAbilities
├── GameplayTags
├── GameplayTasks
├── BridgeSystem
└── GAS
```

**Private зависимости:** Slate, SlateCore, InputCore, NetCore, IrisCore, Niagara, Json, JsonUtilities, OnlineSubsystem, OnlineSubsystemUtils

---

## Архитектурные слои

```
┌──────────────────────────────────────────────────────────────┐
│                     Service Layer                             │
│  OperationService, ValidationService, NetworkService, etc.   │
├──────────────────────────────────────────────────────────────┤
│                    Component Layer                            │
│  DataStore, TransactionProcessor, OperationExecutor, etc.    │
├──────────────────────────────────────────────────────────────┤
│                      Rules Layer                              │
│  CompatibilityRules, WeightRules, ConflictRules, etc.        │
├──────────────────────────────────────────────────────────────┤
│                     Actor Layer                               │
│  WeaponActor, EquipmentActor, AttachmentSystem                │
└──────────────────────────────────────────────────────────────┘
```

---

## Ключевые компоненты

### 1. USuspenseEquipmentDataStore

**Файл:** `Public/Components/Core/SuspenseEquipmentDataStore.h`
**Тип:** UActorComponent
**Назначение:** Pure data storage без бизнес-логики

```cpp
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseEquipmentDataStore
    : public UActorComponent
    , public ISuspenseEquipmentDataProvider
```

#### Философия дизайна

DataStore — "тупой" контейнер данных:
- **Без бизнес-логики** — только хранение и извлечение
- **Thread-safe** — все операции защищены critical section
- **Immutable interface** — геттеры возвращают копии
- **Event-driven** — никогда не broadcast под lock

#### Data Access API

```cpp
// Read-only доступ
FSuspenseCoreInventoryItemInstance GetSlotItem(int32 SlotIndex) const;
FEquipmentSlotConfig GetSlotConfiguration(int32 SlotIndex) const;
TArray<FEquipmentSlotConfig> GetAllSlotConfigurations() const;
TMap<int32, FSuspenseCoreInventoryItemInstance> GetAllEquippedItems() const;
int32 GetSlotCount() const;
bool IsValidSlotIndex(int32 SlotIndex) const;
bool IsSlotOccupied(int32 SlotIndex) const;

// Modification (без валидации!)
bool SetSlotItem(int32 SlotIndex, const FSuspenseCoreInventoryItemInstance& Item, bool bNotify = true);
FSuspenseCoreInventoryItemInstance ClearSlot(int32 SlotIndex, bool bNotify = true);
bool InitializeSlots(const TArray<FEquipmentSlotConfig>& Configurations);
```

#### State Management

```cpp
int32 GetActiveWeaponSlot() const;
bool SetActiveWeaponSlot(int32 SlotIndex);
FGameplayTag GetCurrentEquipmentState() const;
bool SetEquipmentState(const FGameplayTag& NewState);
```

#### Snapshot System

```cpp
FEquipmentStateSnapshot CreateSnapshot() const;
bool RestoreSnapshot(const FEquipmentStateSnapshot& Snapshot);
FEquipmentSlotSnapshot CreateSlotSnapshot(int32 SlotIndex) const;
```

#### Transaction Support

```cpp
void SetActiveTransaction(const FGuid& TransactionId);
void ClearActiveTransaction();
FGuid GetActiveTransaction() const;
void OnTransactionDelta(const TArray<FEquipmentDelta>& Deltas);
```

#### Events

```cpp
FOnSlotDataChanged& OnSlotDataChanged();
FOnSlotConfigurationChanged& OnSlotConfigurationChanged();
FOnDataStoreReset& OnDataStoreReset();
FOnEquipmentDelta& OnEquipmentDelta();
```

---

### 2. USuspenseEquipmentOperationExecutor

**Файл:** `Public/Components/Core/SuspenseEquipmentOperationExecutor.h`
**Тип:** UActorComponent
**Назначение:** Детерминированный planning без side effects

```cpp
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseEquipmentOperationExecutor
    : public UActorComponent
    , public ISuspenseEquipmentOperations
```

#### Transaction Plans

```cpp
USTRUCT(BlueprintType)
struct FTransactionPlan
{
    FGuid PlanId;                   // Unique ID
    bool bAtomic = true;            // Все или ничего
    bool bReversible = true;        // Можно создать savepoint
    bool bIdempotent = false;       // Безопасно для retry
    TArray<FTransactionPlanStep> Steps;
    FString DebugLabel;
    float EstimatedExecutionTimeMs;
    TMap<FString, FString> Metadata;
};
```

#### Planning API

```cpp
// Pure functions — без side effects
bool BuildPlan(const FEquipmentOperationRequest& Request, FTransactionPlan& OutPlan, FText& OutError) const;
bool ValidatePlan(const FTransactionPlan& Plan, FText& OutError) const;
float EstimatePlanExecutionTime(const FTransactionPlan& Plan) const;
bool IsPlanIdempotent(const FTransactionPlan& Plan) const;
```

#### Operations

```cpp
FEquipmentOperationResult ExecuteOperation(const FEquipmentOperationRequest& Request);
FSlotValidationResult ValidateOperation(const FEquipmentOperationRequest& Request) const;
FEquipmentOperationResult EquipItem(const FSuspenseCoreInventoryItemInstance& Item, int32 SlotIndex);
FEquipmentOperationResult UnequipItem(int32 SlotIndex);
FEquipmentOperationResult SwapItems(int32 SlotA, int32 SlotB);
FEquipmentOperationResult MoveItem(int32 SourceSlot, int32 TargetSlot);
FEquipmentOperationResult DropItem(int32 SlotIndex);
FEquipmentOperationResult QuickSwitchWeapon();
```

#### Pre-validation

```cpp
// Для Bridge — проверка ДО удаления из инвентаря
FSlotValidationResult CanEquipItemToSlot(
    const FSuspenseCoreInventoryItemInstance& ItemInstance,
    int32 TargetSlotIndex) const;
```

---

### 3. USuspenseWeaponStateManager

**Файл:** `Public/Components/Core/SuspenseWeaponStateManager.h`
**Тип:** UActorComponent
**Назначение:** FSM для состояний оружия

```cpp
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseWeaponStateManager
    : public UActorComponent
    , public ISuspenseWeaponStateProvider
```

#### State Machine

```cpp
USTRUCT()
struct FWeaponStateMachine
{
    int32 SlotIndex;
    FGameplayTag CurrentState;
    FGameplayTag PreviousState;
    bool bIsTransitioning;
    float TransitionStartTime;
    float TransitionDuration;
    FWeaponStateTransitionRequest ActiveTransition;
};
```

#### State Transitions

```cpp
FGameplayTag GetWeaponState(int32 SlotIndex = -1) const;
FWeaponStateTransitionResult RequestStateTransition(const FWeaponStateTransitionRequest& Request);
bool CanTransitionTo(const FGameplayTag& FromState, const FGameplayTag& ToState) const;
TArray<FGameplayTag> GetValidTransitions(const FGameplayTag& CurrentState) const;
bool ForceState(const FGameplayTag& NewState, int32 SlotIndex = -1);
float GetTransitionDuration(const FGameplayTag& From, const FGameplayTag& To) const;
bool IsTransitioning(int32 SlotIndex = -1) const;
float GetTransitionProgress(int32 SlotIndex = -1) const;
bool AbortTransition(int32 SlotIndex = -1);
```

#### Transition Definition

```cpp
USTRUCT()
struct FStateTransitionDef
{
    FGameplayTag FromState;
    FGameplayTag ToState;
    float Duration = 0.2f;
    bool bInterruptible = false;
    FGameplayTagContainer RequiredTags;
};
```

---

### 4. ASuspenseWeaponActor

**Файл:** `Public/Base/SuspenseWeaponActor.h`
**Тип:** AActor (ASuspenseEquipmentActor)
**Назначение:** Thin facade для weapon систем

```cpp
UCLASS()
class EQUIPMENTSYSTEM_API ASuspenseWeaponActor
    : public ASuspenseEquipmentActor
    , public ISuspenseWeapon
    , public ISuspenseFireModeProvider
```

#### Философия

WeaponActor — тонкий фасад:
- **Без GA/GE** — никаких прямых GameplayAbility
- **Без visual hacks** — только проксирует вызовы
- **SSOT** — инициализация из единого источника данных

#### Components

```cpp
USuspenseWeaponAmmoComponent* AmmoComponent;      // Боеприпасы
USuspenseWeaponFireModeComponent* FireModeComponent; // Режимы огня
UCameraComponent* ScopeCamera;                     // Опциональная камера прицела
```

#### ISuspenseWeapon

```cpp
// Инициализация
FWeaponInitializationResult InitializeFromItemData_Implementation(const FSuspenseCoreInventoryItemInstance& Item);
FSuspenseCoreInventoryItemInstance GetItemInstance_Implementation() const;

// Действия
bool Fire_Implementation(const FWeaponFireParams& Params);
void StopFire_Implementation();
bool Reload_Implementation(bool bForce);
void CancelReload_Implementation();

// Идентификация
FGameplayTag GetWeaponArchetype_Implementation() const;
FGameplayTag GetWeaponType_Implementation() const;
FGameplayTag GetAmmoType_Implementation() const;

// Сокеты
FName GetMuzzleSocketName_Implementation() const;
FName GetSightSocketName_Implementation() const;
FName GetMagazineSocketName_Implementation() const;

// Атрибуты (read-only)
float GetWeaponDamage_Implementation() const;
float GetFireRate_Implementation() const;
float GetReloadTime_Implementation() const;
float GetRecoil_Implementation() const;
float GetRange_Implementation() const;

// Боеприпасы
float GetCurrentAmmo_Implementation() const;
float GetRemainingAmmo_Implementation() const;
float GetMagazineSize_Implementation() const;
FSuspenseCoreInventoryAmmoState GetAmmoState_Implementation() const;
void SetAmmoState_Implementation(const FSuspenseCoreInventoryAmmoState& State);
bool CanReload_Implementation() const;
```

---

### 5. Rules Engine

Модульная система правил валидации.

#### USuspenseRulesCoordinator

**Файл:** `Public/Components/Rules/SuspenseRulesCoordinator.h`

Координирует все rules engines:
```cpp
// Registered engines
USuspenseCompatibilityRulesEngine* CompatibilityEngine;
USuspenseWeightRulesEngine* WeightEngine;
USuspenseConflictRulesEngine* ConflictEngine;
USuspenseRequirementRulesEngine* RequirementEngine;
```

#### USuspenseCompatibilityRulesEngine

Проверка совместимости item ↔ slot:
```cpp
bool IsItemCompatibleWithSlot(const FSuspenseCoreInventoryItemInstance& Item, int32 SlotIndex) const;
TArray<int32> FindCompatibleSlots(const FSuspenseCoreInventoryItemInstance& Item) const;
```

#### USuspenseWeightRulesEngine

Проверка весовых ограничений:
```cpp
bool CanAddWeight(float AdditionalWeight) const;
float GetCurrentTotalWeight() const;
float GetMaxAllowedWeight() const;
```

#### USuspenseConflictRulesEngine

Проверка конфликтов между предметами:
```cpp
bool HasConflict(const FSuspenseCoreInventoryItemInstance& ItemA, const FSuspenseCoreInventoryItemInstance& ItemB) const;
TArray<FEquipmentConflict> GetAllConflicts() const;
```

---

### 6. Network Components

#### USuspenseEquipmentPredictionSystem

**Файл:** `Public/Components/Network/SuspenseEquipmentPredictionSystem.h`

Client-side prediction:
```cpp
FGuid CreatePrediction(const FEquipmentOperationRequest& Request);
void ConfirmPrediction(const FGuid& PredictionId);
void RejectPrediction(const FGuid& PredictionId, const FEquipmentStateSnapshot& ServerState);
bool HasPendingPredictions() const;
```

#### USuspenseEquipmentReplicationManager

**Файл:** `Public/Components/Network/SuspenseEquipmentReplicationManager.h`

Delta-based репликация:
```cpp
void QueueDelta(const FEquipmentDelta& Delta);
TArray<FEquipmentDelta> GetPendingDeltas() const;
void ClearPendingDeltas();
void ApplyServerSnapshot(const FEquipmentStateSnapshot& Snapshot);
```

#### USuspenseEquipmentNetworkDispatcher

**Файл:** `Public/Components/Network/SuspenseEquipmentNetworkDispatcher.h`

RPC queue и network operations:
```cpp
void EnqueueRequest(const FEquipmentOperationRequest& Request);
void ProcessQueue();
bool HasPendingRequests() const;
```

---

## Ключевые типы данных

### EEquipmentSlotType

```cpp
enum class EEquipmentSlotType : uint8
{
    None,
    Primary,      // Основное оружие
    Secondary,    // Вторичное оружие
    Holster,      // Кобура (пистолет)
    Scabbard,     // Ножны (холодное оружие)
    Head,
    Chest,
    Hands,
    Legs,
    Feet,
    Accessory,
    Backpack
};
```

### FEquipmentSlotConfig

```cpp
USTRUCT(BlueprintType)
struct FEquipmentSlotConfig
{
    int32 SlotIndex;
    EEquipmentSlotType SlotType;
    FGameplayTag SlotTag;
    FGameplayTagContainer AllowedItemTypes;
    float MaxWeight;
    bool bIsWeaponSlot;
    bool bIsPrimaryWeaponSlot;
    FName AttachSocketName;
};
```

### FEquipmentDelta

```cpp
USTRUCT(BlueprintType)
struct FEquipmentDelta
{
    FGuid DeltaId;
    FGameplayTag ChangeType;
    int32 SlotIndex;
    FSuspenseCoreInventoryItemInstance Before;
    FSuspenseCoreInventoryItemInstance After;
    FGameplayTag Reason;
    float Timestamp;
};
```

### FEquipmentStateSnapshot

```cpp
USTRUCT(BlueprintType)
struct FEquipmentStateSnapshot
{
    FGuid SnapshotId;
    TArray<FEquipmentSlotSnapshot> Slots;
    int32 ActiveWeaponSlot;
    FGameplayTag EquipmentState;
    float Timestamp;
    uint32 DataVersion;
};
```

---

## Паттерны использования

### Экипировка предмета

```cpp
// Через OperationExecutor
FEquipmentOperationRequest Request;
Request.OperationType = EEquipmentOperationType::Equip;
Request.ItemInstance = ItemFromInventory;
Request.TargetSlotIndex = 0; // Primary weapon

FEquipmentOperationResult Result = OperationExecutor->ExecuteOperation(Request);
if (!Result.bSuccess)
{
    UE_LOG(LogEquipment, Warning, TEXT("Failed: %s"), *Result.ErrorMessage.ToString());
}
```

### Pre-validation для Bridge

```cpp
// В EquipmentInventoryBridge перед удалением из инвентаря
FSlotValidationResult Validation = OperationExecutor->CanEquipItemToSlot(ItemInstance, TargetSlot);
if (!Validation.bIsValid)
{
    // Не удаляем из инвентаря, возвращаем ошибку
    return false;
}

// Теперь безопасно удалить и экипировать
InventoryComponent->RemoveItemInstance(ItemInstance.InstanceID);
OperationExecutor->EquipItem(ItemInstance, TargetSlot);
```

### Работа с Weapon State

```cpp
// Запрос перехода состояния
FWeaponStateTransitionRequest Request;
Request.FromState = FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Idle"));
Request.ToState = FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Firing"));
Request.SlotIndex = ActiveWeaponSlot;

FWeaponStateTransitionResult Result = WeaponStateManager->RequestStateTransition(Request);
if (Result.bSuccess)
{
    // Ждём завершения transition
    float Duration = Result.TransitionDuration;
}
```

### Transaction Flow

```cpp
// 1. Build plan
FTransactionPlan Plan;
FText Error;
if (!OperationExecutor->BuildPlan(Request, Plan, Error))
{
    UE_LOG(LogEquipment, Error, TEXT("Planning failed: %s"), *Error.ToString());
    return;
}

// 2. Validate plan
if (!OperationExecutor->ValidatePlan(Plan, Error))
{
    UE_LOG(LogEquipment, Warning, TEXT("Validation failed: %s"), *Error.ToString());
    return;
}

// 3. Execute via TransactionProcessor
TransactionProcessor->BeginTransaction(Plan.PlanId);
for (const FTransactionPlanStep& Step : Plan.Steps)
{
    TransactionProcessor->ExecuteStep(Step);
}
TransactionProcessor->CommitTransaction();
```

---

## Риски и потенциальные проблемы

### 1. Сложность модуля

**Проблема:** 54k+ LOC, 40+ классов, многоуровневая архитектура.

**Последствия:**
- Высокий порог входа для новых разработчиков
- Сложность отладки cross-layer issues
- Риск архитектурной деградации

**Рекомендации:**
- Создать архитектурные диаграммы для каждого слоя
- Добавить integration tests между слоями
- Рассмотреть разбиение на sub-modules

### 2. Over-engineering Rules Engine

**Проблема:** 6 отдельных Rules Engines для валидации.

**Последствия:**
- Overhead на координацию между engines
- Потенциальные конфликты правил
- Сложность добавления новых правил

**Рекомендации:**
- Документировать приоритеты правил
- Добавить dry-run режим для тестирования
- Рассмотреть объединение simple engines

### 3. Tight Coupling с GAS

**Проблема:** WeaponActor и Services тесно связаны с GameplayAbilities.

**Последствия:**
- Невозможность использования без GAS
- Сложность мокирования для тестов

**Рекомендации:**
- Создать абстракцию IAbilityProvider
- Добавить fallback для non-GAS режима

### 4. State Machine Complexity

**Проблема:** WeaponStateManager поддерживает per-slot state machines.

**Последствия:**
- N state machines для N слотов
- Потенциальные sync issues между ними
- Memory overhead

**Рекомендации:**
- Рассмотреть lazy creation для редко используемых слотов
- Добавить state machine pooling

### 5. Network Prediction без Reconciliation

**Проблема:** PredictionSystem создает predictions но reconciliation manual.

**Последствия:**
- Потенциальные desync при высоком ping
- Сложность отладки prediction errors

**Рекомендации:**
- Добавить автоматический reconciliation
- Логировать prediction accuracy metrics

### 6. IrisCore зависимость

**Проблема:** Модуль зависит от IrisCore (replication).

**Последствия:**
- Неясная связь с UE replication
- Потенциальные конфликты при обновлении UE

**Рекомендации:**
- Документировать роль IrisCore
- Рассмотреть миграцию на стандартный UE replication

### 7. Отсутствие Weapon Pooling

**Проблема:** WeaponActor создаётся/уничтожается при equip/unequip.

**Последствия:**
- GC spikes при быстрой смене оружия
- Потенциальные stalls

**Рекомендации:**
- Реализовать actor pool для weapon actors
- Hide/show вместо create/destroy

### 8. Монолитный Build.cs

**Проблема:** 14 зависимостей, включая OnlineSubsystem.

**Последствия:**
- Увеличенное время компиляции
- Unnecessary dependencies для offline режима

**Рекомендации:**
- Разделить на EquipmentCore и EquipmentNetwork
- Сделать OnlineSubsystem optional

---

## Метрики модуля

| Метрика | Значение |
|---------|----------|
| Файлов (.h) | 40 |
| Components | 25+ |
| Services | 6 |
| Rules Engines | 6 |
| Actors | 3 |
| LOC (приблизительно) | 54,213 |

---

## Связь с другими модулями

```
┌────────────────────────────────────────────────────┐
│                   PlayerCore                        │
│              (PlayerState owns components)          │
└───────────────────────┬────────────────────────────┘
                        │
          ┌─────────────┼─────────────┐
          │             │             │
          ▼             ▼             ▼
┌─────────────┐  ┌───────────────┐  ┌─────────────┐
│InventorySys │◄─│EquipmentSys  │──►│     GAS     │
│  (Items)    │  │(Slots,Weapons)│  │ (Attributes)│
└─────────────┘  └───────┬───────┘  └─────────────┘
                         │
              ┌──────────┴──────────┐
              │                     │
              ▼                     ▼
      ┌─────────────┐       ┌─────────────┐
      │BridgeSystem │       │  UISystem   │
      │  (Events)   │       │ (Equipment  │
      └─────────────┘       │   Widget)   │
                            └─────────────┘
```

**EquipmentSystem зависит от:**
- **BridgeSystem** — типы, события, EventManager
- **GAS** — атрибуты оружия

**От EquipmentSystem зависят:**
- **PlayerCore** — владеет equipment components
- **UISystem** — отображает экипировку

---

**Дата создания:** 2025-11-28
**Автор:** Tech Lead
**Версия документа:** 1.0
