# BridgeSystem API Reference

**Модуль:** BridgeSystem
**Версия:** 2.0.0
**Статус:** Полностью мигрирован, компилируется
**LOC:** ~26,680
**Последнее обновление:** 2025-11-28

---

## Обзор модуля

BridgeSystem — фундаментальный модуль архитектуры SuspenseCore, отвечающий за межмодульную коммуникацию и предоставляющий инфраструктуру для всех остальных модулей проекта.

### Архитектурная роль

BridgeSystem выполняет несколько критических функций:

1. **Service Locator** — централизованная регистрация и доступ к сервисам через GameplayTags
2. **Event Bus** — центральная шина событий для всей игры (Blueprint + C++)
3. **Shared Types** — единое хранилище типов данных, используемых несколькими модулями
4. **Interface Contracts** — определение интерфейсов для loose coupling между системами
5. **Item Management** — централизованный менеджер данных предметов

### Зависимости модуля

```
BridgeSystem
├── Core, CoreUObject, Engine (базовые UE)
├── GameplayAbilities (GAS интеграция)
├── GameplayTags (система тегов)
├── GameplayTasks (асинхронные таски)
├── UMG (UI виджеты)
├── Niagara (VFX)
└── PhysicsCore (физические типы)
```

**Private зависимости:** Slate, SlateCore, Json, JsonUtilities

---

## Ключевые компоненты

### 1. USuspenseEventManager

**Файл:** `Public/Delegates/SuspenseEventManager.h`
**Тип:** UGameInstanceSubsystem
**Назначение:** Централизованная шина событий для всего проекта

```cpp
UCLASS(BlueprintType)
class BRIDGESYSTEM_API USuspenseEventManager : public UGameInstanceSubsystem
```

#### Архитектурная философия

EventManager — это центральный хаб маршрутизации событий. Он **НЕ** содержит бизнес-логики, только маршрутизирует события между публикаторами и подписчиками.

**Ключевые принципы:**
- Единый источник истины для игровых событий
- Отсутствие циклических зависимостей между модулями
- Thread-safe диспетчеризация событий
- Поддержка Blueprint и C++ подписчиков

#### Категории событий

| Категория | Делегаты | Назначение |
|-----------|----------|------------|
| UI System | `OnUIWidgetCreated`, `OnHealthUpdated`, `OnCrosshairUpdated` | Обновление интерфейса |
| Equipment | `OnEquipmentUpdated`, `OnActiveWeaponChanged`, `OnEquipmentStateChanged` | Экипировка и оружие |
| Weapon | `OnAmmoChanged`, `OnWeaponFired`, `OnWeaponReloadStart` | Система оружия |
| Movement | `OnMovementSpeedChanged`, `OnJumpStateChanged`, `OnLanded` | Перемещение персонажа |
| Loadout | `OnLoadoutTableLoaded`, `OnLoadoutChanged`, `OnLoadoutApplied` | Лоадауты |
| Inventory | `OnUIItemDropped`, `OnInventoryItemMoved` | Инвентарь и drag-drop |
| Tooltip | `OnTooltipRequested`, `OnTooltipHideRequested` | Тултипы |

#### Паттерн использования

**Публикация события:**
```cpp
if (USuspenseEventManager* EventMgr = USuspenseEventManager::Get(this))
{
    EventMgr->NotifyHealthUpdated(CurrentHealth, MaxHealth, HealthPercent);
}
```

**Подписка (C++ Lambda):**
```cpp
EventMgr->SubscribeToHealthUpdated([this](float Current, float Max, float Percent) {
    UpdateHealthBar(Current, Max, Percent);
});
```

**Подписка (UObject Method):**
```cpp
EventMgr->SubscribeToAmmoChangedUObject(this, &UMyWidget::HandleAmmoChanged);
```

**Подписка (Blueprint):**
```cpp
UPROPERTY(BlueprintAssignable, Category = "UI|Attributes")
FOnHealthUpdatedEvent OnHealthUpdated;
```

#### Двойная система делегатов

EventManager предоставляет две версии каждого делегата:
- **Dynamic (Blueprint):** `FOnHealthUpdatedEvent` — для Blueprint подписчиков
- **Native (C++):** `FOnHealthUpdatedNative` — для высокопроизводительных C++ подписчиков

---

### 2. USuspenseEquipmentServiceLocator

**Файл:** `Public/Core/Services/SuspenseEquipmentServiceLocator.h`
**Тип:** UGameInstanceSubsystem
**Назначение:** Dependency Injection контейнер для сервисов

```cpp
UCLASS()
class BRIDGESYSTEM_API USuspenseEquipmentServiceLocator : public UGameInstanceSubsystem
```

#### Функциональность

| Метод | Описание |
|-------|----------|
| `RegisterServiceClass()` | Регистрация сервиса по классу (lazy создание) |
| `RegisterServiceInstance()` | Регистрация уже созданного экземпляра |
| `RegisterServiceFactory()` | Регистрация через фабрику |
| `GetService()` | Получение сервиса с lazy инициализацией |
| `TryGetService()` | Получение только если готов (без lazy) |
| `InitializeAllServices()` | Топологическая сортировка и инициализация |
| `ShutdownAllServices()` | Graceful shutdown в обратном порядке |

#### Lifecycle сервисов

```cpp
enum class EServiceLifecycleState : uint8
{
    Uninitialized,  // Зарегистрирован, не инициализирован
    Initializing,   // В процессе инициализации
    Ready,          // Готов к использованию
    Shutting,       // В процессе shutdown
    Shutdown,       // Завершен
    Failed          // Ошибка инициализации
};
```

#### Dependency Injection

ServiceLocator поддерживает injection callback для внедрения зависимостей:

```cpp
bool RegisterServiceClassWithInjection(
    const FGameplayTag& ServiceTag,
    TSubclassOf<UObject> ServiceClass,
    const FServiceInitParams& InitParams,
    const FServiceInjectionDelegate& InjectionCallback
);
```

**Порядок инициализации:**
1. ServiceLocator создает экземпляр сервиса
2. Вызывается `InjectionCallback` для внедрения зависимостей
3. Вызывается `InitializeService()` на сервисе

#### Thread Safety

Каждая регистрация сервиса имеет собственный mutex (`TSharedPtr<FCriticalSection> ServiceLock`), обеспечивая thread-safe доступ в многопоточной среде.

---

### 3. USuspenseCoreItemManager

**Файл:** `Public/ItemSystem/SuspenseItemManager.h`
**Тип:** UGameInstanceSubsystem
**Назначение:** Централизованный доступ к данным предметов

```cpp
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreItemManager : public UGameInstanceSubsystem
```

#### Архитектурные принципы

- **Single Source of Truth:** DataTable как единственный источник данных
- **Explicit Configuration:** Нет скрытых зависимостей на hardcoded пути
- **Fail-fast Validation:** Критические предметы валидируются при загрузке
- **Backward Compatibility:** Fallback на дефолтный путь с предупреждением

#### Основное API

```cpp
// Загрузка таблицы данных (PRIMARY)
bool LoadItemDataTable(UDataTable* ItemDataTable, bool bStrictValidation = false);

// Получение данных предмета
bool GetUnifiedItemData(const FName& ItemID, FSuspenseCoreUnifiedItemData& OutItemData) const;

// Создание экземпляра предмета
bool CreateItemInstance(const FName& ItemID, int32 Quantity,
                        FSuspenseCoreInventoryItemInstance& OutInstance) const;

// Поиск по критериям
TArray<FName> GetItemsByType(const FGameplayTag& ItemType) const;
TArray<FName> GetEquippableItemsForSlot(const FGameplayTag& SlotType) const;
TArray<FName> GetCompatibleAmmoForWeapon(const FName& WeaponItemID) const;
```

#### Кеширование

ItemManager строит внутренний кеш для быстрого доступа:
```cpp
TMap<FName, FSuspenseCoreUnifiedItemData> UnifiedItemCache;
```

Статистика кеша доступна через `GetCacheStatistics()`.

---

### 4. Интерфейсы

BridgeSystem определяет контракты для loose coupling между модулями.

#### ISuspenseCharacterInterface

**Файл:** `Public/Interfaces/Core/ISuspenseCharacter.h`

Унифицированный интерфейс персонажа:
```cpp
// Управление оружием
void SetHasWeapon(bool bHasWeapon);
void SetCurrentWeaponActor(AActor* WeaponActor);
AActor* GetCurrentWeaponActor() const;
bool HasWeapon() const;

// GAS доступ
UAbilitySystemComponent* GetASC() const;
float GetCharacterLevel() const;

// Состояние
bool IsAlive() const;
int32 GetTeamId() const;

// Event System
USuspenseEventManager* GetDelegateManager() const;
```

#### ISuspenseEquipmentService

**Файл:** `Public/Interfaces/Equipment/ISuspenseEquipmentService.h`

Базовый интерфейс для всех сервисов экипировки:
```cpp
virtual bool InitializeService(const FServiceInitParams& Params) = 0;
virtual bool ShutdownService(bool bForce = false) = 0;
virtual EServiceLifecycleState GetServiceState() const = 0;
virtual FGameplayTag GetServiceTag() const = 0;
virtual FGameplayTagContainer GetRequiredDependencies() const = 0;
```

**Производные интерфейсы:**
- `IEquipmentDataService` — данные экипировки + dependency injection
- `IEquipmentOperationService` — операции экипировки
- `IEquipmentValidationService` — валидация правил
- `IEquipmentVisualizationService` — визуализация
- `IEquipmentNetworkService` — сетевой слой

---

### 5. Типы данных (Types)

#### FSuspenseCoreInventoryItemInstance

**Файл:** `Public/Types/Inventory/SuspenseInventoryTypes.h`

Основная runtime структура для экземпляров предметов:

```cpp
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreInventoryItemInstance
{
    FName ItemID;                           // Связь с DataTable
    FGuid InstanceID;                       // Уникальный ID экземпляра
    int32 Quantity;                         // Количество в стеке
    TMap<FName, float> RuntimeProperties;   // Динамические свойства
    int32 AnchorIndex;                      // Позиция в инвентаре
    bool bIsRotated;                        // Повернут ли в сетке
    float LastUsedTime;                     // Время последнего использования
};
```

**Factory методы:**
```cpp
static FSuspenseCoreInventoryItemInstance Create();
static FSuspenseCoreInventoryItemInstance Create(const FName& InItemID, int32 InQuantity = 1);
static FSuspenseCoreInventoryItemInstance CreateWithID(const FName& InItemID, const FGuid& InInstanceID, int32 InQuantity = 1);
```

**Runtime Properties API:**
```cpp
float GetRuntimeProperty(const FName& PropertyName, float DefaultValue = 0.0f) const;
void SetRuntimeProperty(const FName& PropertyName, float Value);
void RemoveRuntimeProperty(const FName& PropertyName);

// Convenience методы
float GetCurrentDurability() const;
void SetCurrentDurability(float Durability);
int32 GetCurrentAmmo() const;
void SetCurrentAmmo(int32 AmmoCount);
bool IsOnCooldown(float CurrentTime) const;
```

#### FEquipmentOperationRequest

**Файл:** `Public/Types/Equipment/SuspenseEquipmentTypes.h`

Запрос на операцию с экипировкой:

```cpp
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FEquipmentOperationRequest
{
    FGuid OperationId;                              // Уникальный ID операции
    EEquipmentOperationType OperationType;          // Тип операции
    EEquipmentOperationPriority Priority;           // Приоритет
    FSuspenseCoreInventoryItemInstance ItemInstance;    // Предмет операции
    int32 SourceSlotIndex;                          // Исходный слот
    int32 TargetSlotIndex;                          // Целевой слот
    float Timestamp;                                // Время запроса
    TWeakObjectPtr<AActor> Instigator;              // Инициатор
    TMap<FString, FString> Parameters;              // Дополнительные параметры
    bool bForceOperation;                           // Принудительное выполнение
    bool bIsSimulated;                              // Симуляция (prediction)
};
```

**Типы операций:**
```cpp
enum class EEquipmentOperationType : uint8
{
    None, Equip, Unequip, Swap, Move, Drop,
    Transfer, QuickSwitch, Reload, Inspect,
    Repair, Upgrade, Modify, Combine, Split
};
```

#### FEquipmentOperationResult

Результат операции с экипировкой:

```cpp
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FEquipmentOperationResult
{
    bool bSuccess;
    FText ErrorMessage;
    EEquipmentValidationFailure FailureType;
    FGuid OperationId;
    FGuid TransactionId;
    TArray<int32> AffectedSlots;
    TArray<FSuspenseCoreInventoryItemInstance> AffectedItems;
    TMap<FString, FString> ResultMetadata;
    float ExecutionTime;
    TArray<FText> Warnings;
};
```

#### FEquipmentTransaction

ACID-транзакция для операций экипировки:

```cpp
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FEquipmentTransaction
{
    FGuid TransactionId;
    ETransactionState State;                    // None, Active, Committing, Committed, RollingBack, RolledBack, Failed
    TArray<FEquipmentOperationRequest> Operations;
    FEquipmentStateSnapshot StateBefore;        // Snapshot до транзакции
    FEquipmentStateSnapshot StateAfter;         // Snapshot после транзакции
    FDateTime StartTime;
    FDateTime EndTime;
    bool bIsCommitted;
    bool bIsRolledBack;
    bool bIsNested;                             // Вложенная транзакция
    FGuid ParentTransactionId;
};
```

---

## Паттерны использования

### Получение EventManager

```cpp
// Из любого UObject с World context
USuspenseEventManager* EventMgr = USuspenseEventManager::Get(this);
if (EventMgr)
{
    // Публикация
    EventMgr->NotifyEquipmentUpdated();

    // Подписка
    FDelegateHandle Handle = EventMgr->SubscribeToEquipmentUpdated([]() {
        // Обработка события
    });
}
```

### Получение сервиса через ServiceLocator

```cpp
USuspenseEquipmentServiceLocator* Locator = USuspenseEquipmentServiceLocator::Get(this);
if (Locator)
{
    // Получение сервиса по тегу
    FGameplayTag ServiceTag = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Data"));
    UObject* Service = Locator->GetService(ServiceTag);

    // Type-safe получение через интерфейс
    IEquipmentDataService* DataService = Locator->GetServiceAs<IEquipmentDataService>(ServiceTag);
}
```

### Работа с предметами

```cpp
USuspenseCoreItemManager* ItemMgr = GetGameInstance()->GetSubsystem<USuspenseCoreItemManager>();
if (ItemMgr)
{
    // Создание экземпляра предмета
    FSuspenseCoreInventoryItemInstance NewItem;
    if (ItemMgr->CreateItemInstance(TEXT("Weapon_Rifle"), 1, NewItem))
    {
        // Установка runtime свойств
        NewItem.SetCurrentAmmo(30);
        NewItem.SetCurrentDurability(100.0f);
    }

    // Поиск предметов
    TArray<FName> Weapons = ItemMgr->GetItemsByType(
        FGameplayTag::RequestGameplayTag(TEXT("Item.Type.Weapon"))
    );
}
```

---

## Риски и потенциальные проблемы

### 1. Централизация EventManager

**Проблема:** USuspenseEventManager становится god object с огромным количеством делегатов (~50+ событий).

**Последствия:**
- Сложность поддержки и расширения
- Высокий риск merge conflicts при параллельной разработке
- Увеличение времени компиляции при изменениях в заголовочном файле

**Рекомендации:**
- Рассмотреть разделение на доменные EventManager'ы (UIEventManager, GameplayEventManager)
- Использовать forward declarations для типов делегатов
- Вынести делегаты в отдельные заголовочные файлы

### 2. Двойная система делегатов

**Проблема:** Каждое событие имеет два делегата (Dynamic + Native), что удваивает код.

**Последствия:**
- Дублирование кода в Notify методах
- Риск рассинхронизации между версиями
- Увеличение размера объекта в памяти

**Рекомендации:**
- Рассмотреть генерацию кода через макросы
- Документировать, какую версию использовать в каких случаях

### 3. Thread Safety ServiceLocator

**Проблема:** Хотя ServiceLocator имеет per-service locks, операции registry-wide (InitializeAllServices, TopoSort) используют один RegistryLock.

**Последствия:**
- Потенциальные блокировки при массовой инициализации
- Необходимость внимательной работы с порядком блокировок

**Рекомендации:**
- Проверить отсутствие deadlock'ов при nested dependency resolution
- Рассмотреть lock-free структуры для read-heavy операций

### 4. Fallback на hardcoded пути

**Проблема:** ItemManager имеет fallback на `DefaultItemTablePath = TEXT("/Game/MEDCOM/Data/DT_MedComItems")`.

**Последствия:**
- Скрытая зависимость на legacy путь
- Потенциальные проблемы при рефакторинге структуры проекта

**Рекомендации:**
- Удалить fallback после полной миграции
- Требовать explicit configuration в production builds

### 5. Большой размер типов

**Проблема:** FSuspenseCoreInventoryItemInstance содержит TMap для RuntimeProperties.

**Последствия:**
- Увеличенный overhead при репликации
- Потенциальные проблемы производительности при массовых операциях

**Рекомендации:**
- Профилировать сетевой трафик при репликации инвентаря
- Рассмотреть bitfield или fixed-size массив для частых свойств

### 6. Отсутствие версионирования типов

**Проблема:** Структуры не имеют явного версионирования для сериализации.

**Последствия:**
- Проблемы при обновлении формата данных
- Потенциальная несовместимость save files

**Рекомендации:**
- Добавить Version поле в критичные структуры
- Реализовать миграцию данных при загрузке

### 7. Циклические зависимости интерфейсов

**Проблема:** Forward declarations указывают на классы с префиксом MedCom (legacy naming).

**Последствия:**
- Несоответствие между объявлениями и реализациями
- Путаница при добавлении новых классов

**Рекомендации:**
- Провести ревизию всех forward declarations
- Обновить на Suspense naming convention

---

## Метрики модуля

| Метрика | Значение |
|---------|----------|
| Файлов (.h) | 82 |
| Файлов (.cpp) | ~40 |
| Интерфейсов | 35+ |
| Структур | 20+ |
| Enums | 12 |
| LOC (приблизительно) | 26,680 |

---

## Связь с другими модулями

```
┌─────────────┐
│   UISystem  │──────┐
├─────────────┤      │
│ PlayerCore  │──────┼───▶ BridgeSystem
├─────────────┤      │     (Types, Events, Services)
│     GAS     │──────┤
├─────────────┤      │
│EquipmentSys│──────┤
├─────────────┤      │
│InventorySys│──────┤
├─────────────┤      │
│InteractionS│──────┘
└─────────────┘
```

**BridgeSystem не зависит ни от одного геймплейного модуля** — это критически важно для архитектуры. Все модули зависят от BridgeSystem, но не наоборот.

---

## SuspenseCore Save System

**Файлы:**
- `Public/SuspenseCore/Save/SuspenseCoreSaveManager.h`
- `Public/SuspenseCore/Save/SuspenseCoreSaveTypes.h`
- `Public/SuspenseCore/Save/SuspenseCoreSaveInterfaces.h`
- `Public/SuspenseCore/Save/SuspenseCoreFileSaveRepository.h`

### USuspenseCoreSaveManager

**Тип:** UGameInstanceSubsystem
**Назначение:** Централизованное управление сохранениями игры

#### Что сохраняется

| Категория | Данные | Статус |
|-----------|--------|--------|
| **Position** | WorldPosition, WorldRotation, CurrentMapName | ✅ |
| **GAS Attributes** | Health, MaxHealth, Stamina, MaxStamina, Armor, Shield | ✅ |
| **Active Effects** | EffectId, RemainingDuration, StackCount, Level | ✅ |
| **Profile Data** | DisplayName, Level, XP, Stats, Settings, Loadouts | ✅ |
| **Player Stats** | Kills, Deaths, Assists, DamageDealt, Accuracy, PlayTime | ✅ |
| **Settings** | MouseSensitivity, FOV, Volume, CrosshairColor | ✅ |
| **Movement State** | bIsCrouching, bIsSprinting | ✅ |

#### API

```cpp
// Quick Save/Load (F5/F9)
void QuickSave();
void QuickLoad();
bool HasQuickSave() const;

// Slot Management
void SaveToSlot(int32 SlotIndex, const FString& SlotName = TEXT(""));
void LoadFromSlot(int32 SlotIndex);
void DeleteSlot(int32 SlotIndex);
TArray<FSuspenseCoreSaveHeader> GetAllSlotHeaders();

// Auto-Save
void SetAutoSaveEnabled(bool bEnabled);
void SetAutoSaveInterval(float IntervalSeconds);  // Min 30s, default 300s
void TriggerAutoSave();  // Manual checkpoint trigger

// State Collection (internal)
FSuspenseCoreSaveData CollectCurrentGameState();
void ApplyLoadedState(const FSuspenseCoreSaveData& SaveData);
```

#### Events

```cpp
FOnSuspenseCoreSaveStarted OnSaveStarted;
FOnSuspenseCoreSaveCompleted OnSaveCompleted;  // (bool bSuccess, FString ErrorMessage)
FOnSuspenseCoreLoadStarted OnLoadStarted;
FOnSuspenseCoreLoadCompleted OnLoadCompleted;  // (bool bSuccess, FString ErrorMessage)
```

#### Save Data Structure

```cpp
FSuspenseCoreSaveData
├── FSuspenseCoreSaveHeader        // Metadata (version, timestamp, playtime, character info)
├── FSuspenseCorePlayerData        // Profile (ID, DisplayName, Level, XP, Stats, Settings)
├── FSuspenseCoreCharacterState    // Runtime state (position, attributes, effects)
├── FSuspenseCoreInventoryState    // Items, Currencies, InventorySize
└── FSuspenseCoreEquipmentState    // EquippedSlots, Loadout, QuickSlots, WeaponAmmo
```

#### Integration with GAS

Save system полностью интегрирована с Gameplay Ability System:
- `CollectCharacterState()` читает атрибуты через `IAbilitySystemInterface`
- `ApplyLoadedState()` восстанавливает атрибуты через `SetNumericAttributeBase()`
- Active GameplayEffects сохраняются и восстанавливаются с duration/stack count

---

**Дата создания:** 2025-11-28
**Автор:** Tech Lead
**Версия документа:** 1.1 (Save System documentation added)
