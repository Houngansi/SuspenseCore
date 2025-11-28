# Руководство по миграции

> **✅ МИГРАЦИЯ ЗАВЕРШЕНА (2025-11-28)**
>
> Этот документ использовался во время миграции с MedCom на Suspense.
> Миграция успешно завершена - весь код компилируется без ошибок.
> Документ сохранён как справочник для понимания процесса миграции и для будущих проектов.

Детальное руководство по переносу кода из старого проекта в SuspenseCore.

---

## Статус миграции: ✅ ЗАВЕРШЕНА

| Волна | Модуль | Статус |
|-------|--------|--------|
| Wave 1 | MedComShared → BridgeSystem | ✅ Завершено |
| Wave 2 | MedComInteraction → InteractionSystem | ✅ Завершено |
| Wave 2 | MedComCore → PlayerCore | ✅ Завершено |
| Wave 2 | MedComGAS → GAS | ✅ Завершено |
| Wave 3 | MedComInventory → InventorySystem | ✅ Завершено |
| Wave 4 | MedComEquipment → EquipmentSystem | ✅ Завершено |
| Wave 5 | MedComUI → UISystem | ✅ Завершено |

**Итого:** ~155,000 LOC успешно мигрировано.

---

## Общая стратегия (Reference)

Миграция проводилась **постепенно** с обновлением стандартов кода и архитектуры:

1. **Анализ** → 2. **Планирование** → 3. **Миграция** → 4. **Тестирование** → 5. **Документация**

---

## Чек-лист миграции файла

Используйте этот чеклист для каждого переносимого файла:

### Подготовка

- [ ] Определен целевой модуль для файла
- [ ] Проверены зависимости файла
- [ ] Понятна текущая функциональность
- [ ] Нет дублирования с существующим кодом

### Перенос

- [ ] Файл помещен в правильный модуль (`Source/[Module]/Public` или `Private`)
- [ ] Обновлен copyright header (убрать Epic Games, добавить Houngansi)
- [ ] Имена классов соответствуют UE Coding Standard
- [ ] Префиксы классов корректны (U/A/F/E/I)
- [ ] MODULENAME_API макросы обновлены
- [ ] Include guards обновлены (#pragma once предпочтительнее)

### Зависимости

- [ ] Все зависимости добавлены в `[Module].Build.cs`
- [ ] Forward declarations использованы где возможно
- [ ] Циклические зависимости устранены
- [ ] Includes упорядочены по UE стандарту

### Code Style

- [ ] Форматирование соответствует UE Coding Standard
- [ ] Именование переменных корректно (bIsActive, MaxHealth, etc.)
- [ ] Комментарии на английском языке
- [ ] Удалены ненужные комментарии и TODO

### Replication (если применимо)

- [ ] Replicated properties объявлены с UPROPERTY(Replicated)
- [ ] GetLifetimeReplicatedProps() реализован
- [ ] OnRep_ функции созданы для важных свойств
- [ ] Replication conditions настроены (COND_OwnerOnly, etc.)

### GAS Integration (если применимо)

- [ ] GameplayTags созданы в конфигурации
- [ ] Abilities наследуются от базовых классов SuspenseCore
- [ ] AttributeSets правильно реплицируются
- [ ] GameplayEffects настроены корректно

### Тестирование

- [ ] Код компилируется без ошибок и предупреждений
- [ ] Функциональность протестирована в редакторе
- [ ] Replication протестирована (если применимо)
- [ ] Performance профилирован (для критичных систем)

### Документация

- [ ] API документация обновлена в `Docs/API/`
- [ ] README.md обновлен (если это значимое изменение)
- [ ] Changelog.md содержит запись о миграции
- [ ] Inline комментарии добавлены для сложной логики

---

## Mapping старых систем на новые модули

### Система → Модуль

| Старая система | Новый модуль | Примечания |
|----------------|--------------|------------|
| Character системы | PlayerCore | Base character, controller, camera |
| Ability System | GAS | Все GAS-related классы |
| Weapon/Equipment | EquipmentSystem | Оружие, броня, экипировка |
| Inventory | InventorySystem | Инвентарь, предметы, стаки |
| Object Interaction | InteractionSystem | Взаимодействие с миром |
| UI/HUD | UISystem | Весь UI код |
| Cross-system communication | BridgeSystem | Mediator, events |
| Shared utilities | SuspenseCore | Логгирование, configs, utils |

---

## Примеры миграции

### Пример 1: Миграция Character класса

#### Старый код (до миграции)

```cpp
// MyOldCharacter.h
#pragma once
#include "GameFramework/Character.h"
#include "MyOldCharacter.generated.h"

UCLASS()
class AMyOldCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AMyOldCharacter();

    // Health
    float Health;
    float MaxHealth;

    void TakeDamage(float Damage);
};
```

#### Новый код (после миграции)

```cpp
// PlayerCharacterBase.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "PlayerCharacterBase.generated.h"

class UAbilitySystemComponent;
class USuspenseAttributeSet;

/**
 * Base character class with GAS integration
 * Replicated for multiplayer, designed for FPS/MMO
 */
UCLASS(Abstract, Blueprintable)
class PLAYERCORE_API APlayerCharacterBase : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    APlayerCharacterBase(const FObjectInitializer& ObjectInitializer);

    // IAbilitySystemInterface
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    // Replication
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;

    // GAS Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", Replicated)
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(BlueprintReadOnly, Category = "Abilities")
    TObjectPtr<USuspenseAttributeSet> AttributeSet;

    // Initialize GAS
    virtual void InitializeAbilitySystem();
};
```

```cpp
// PlayerCharacterBase.cpp
#include "PlayerCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
// #include "GAS/Public/SuspenseAttributeSet.h" // TODO: when implemented

APlayerCharacterBase::APlayerCharacterBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Disable ticking if not needed
    PrimaryActorTick.bCanEverTick = false;

    // Create GAS components
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComp"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    // TODO: Create attribute set when SuspenseAttributeSet is implemented
    // AttributeSet = CreateDefaultSubobject<USuspenseAttributeSet>(TEXT("AttributeSet"));
}

void APlayerCharacterBase::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        InitializeAbilitySystem();
    }
}

UAbilitySystemComponent* APlayerCharacterBase::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void APlayerCharacterBase::InitializeAbilitySystem()
{
    if (!AbilitySystemComponent)
    {
        return;
    }

    // Init ASC with owner
    AbilitySystemComponent->InitAbilityActorInfo(this, this);

    // TODO: Grant default abilities
    // TODO: Initialize attributes
}

void APlayerCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(APlayerCharacterBase, AbilitySystemComponent);
}
```

**Изменения:**
- ✅ Health перенесен в GAS AttributeSet
- ✅ Добавлена репликация
- ✅ TakeDamage() будет реализован через GameplayEffect
- ✅ Следование UE Coding Standard
- ✅ Правильный copyright и module API macro

---

### Пример 2: Миграция Inventory Component

#### Старый код

```cpp
// OldInventoryComponent.h
#pragma once
#include "Components/ActorComponent.h"
#include "OldInventoryComponent.generated.h"

UCLASS()
class UOldInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    TArray<AActor*> Items;

    void AddItem(AActor* Item);
    void RemoveItem(AActor* Item);
};
```

#### Новый код

```cpp
// InventoryComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/UnrealNetwork.h"
#include "InventoryComponent.generated.h"

class UItemInstance;
class UItemDefinition;

/**
 * Replicated inventory component
 * Handles item storage, stacking, and management
 */
UCLASS(ClassGroup=(Inventory), meta=(BlueprintSpawnableComponent))
class INVENTORYSYSTEM_API UInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInventoryComponent();

    // Replication
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /**
     * Add item to inventory
     * @param ItemDef Item definition to add
     * @param Count Number of items to add
     * @return true if successfully added
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool AddItem(const UItemDefinition* ItemDef, int32 Count = 1);

    /**
     * Remove item from inventory
     * @param ItemInstance Instance to remove
     * @param Count Number to remove (0 = all)
     * @return Number actually removed
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    int32 RemoveItem(UItemInstance* ItemInstance, int32 Count = 0);

    /**
     * Get all items in inventory
     */
    UFUNCTION(BlueprintPure, Category = "Inventory")
    const TArray<UItemInstance*>& GetItems() const { return Items; }

protected:
    // Replicated array of item instances
    UPROPERTY(ReplicatedUsing = OnRep_Items)
    TArray<TObjectPtr<UItemInstance>> Items;

    UFUNCTION()
    void OnRep_Items();

    // Server-only: actually add item
    bool AddItem_Internal(const UItemDefinition* ItemDef, int32 Count);
};
```

**Изменения:**
- ✅ AActor* → UItemInstance* (более чистая архитектура)
- ✅ Добавлена репликация
- ✅ Type-safe item system
- ✅ Stack support через Count parameter
- ✅ Server authority pattern
- ✅ BlueprintCallable для designer accessibility

---

## Неизменяемые имена модулей (ВАЖНО!)

При миграции и рефакторинге **ЗАПРЕЩЕНО** изменять имена следующих модулей:

| Модуль | Имя модуля (неизменяемое) | API Macro |
|--------|---------------------------|-----------|
| Core | **SuspenseCore** | `SUSPENSECORE_API` |
| Player | **PlayerCore** | `PLAYERCORE_API` |
| Abilities | **GAS** | `GAS_API` |
| Equipment | **EquipmentSystem** | `EQUIPMENTSYSTEM_API` |
| Inventory | **InventorySystem** | `INVENTORYSYSTEM_API` |
| Interaction | **InteractionSystem** | `INTERACTIONSYSTEM_API` |
| Bridge | **BridgeSystem** | `BRIDGESYSTEM_API` |
| UI | **UISystem** | `UISYSTEM_API` |

### Правила именования классов

При миграции классов необходимо:

1. **Избегать конфликтов имен** между struct и class:
   - Struct `FSomeName` и Class `USomeName` имеют одинаковое engine name
   - Используйте уникальные суффиксы: `FStorageTransaction` vs `UInventoryTransaction`

2. **Избегать дублирования структур**:
   - Простые структуры для tracking/history: добавить суффикс `Record`
   - Детализированные структуры для операций: использовать полное имя

3. **Соответствие имени файла и .generated.h**:
   - Файл `SuspenseMoveOperation.h` → `SuspenseMoveOperation.generated.h`
   - **НЕ** `MoveOperation.generated.h` (это приведет к ошибкам UHT)

### Примеры корректного именования

```cpp
// ПРАВИЛЬНО: разные engine names
USTRUCT() struct FSuspenseStorageTransaction { };  // Engine name: SuspenseStorageTransaction
UCLASS() class USuspenseInventoryTransaction { };  // Engine name: SuspenseInventoryTransaction

// ПРАВИЛЬНО: tracking record vs operation
USTRUCT() struct FSuspenseMoveRecord { };          // Для истории операций
USTRUCT() struct FSuspenseMoveOperation { };       // Для выполнения операций

// НЕПРАВИЛЬНО: конфликт engine names
USTRUCT() struct FSuspenseInventoryTransaction { };  // Конфликт!
UCLASS() class USuspenseInventoryTransaction { };    // Конфликт!
```

---

## Обновление неймспейсов

### До (старый проект)

```cpp
namespace MyGame
{
    namespace Inventory
    {
        class ItemBase { };
    }
}
```

### После (SuspenseCore)

```cpp
// В UE не используются C++ namespaces, вместо этого:
// 1. Module prefix в API macro
// 2. Class name prefix для уникальности

// Item.h - в модуле InventorySystem
class INVENTORYSYSTEM_API UItemInstance : public UObject
{
    // ...
};
```

**Причина:** Unreal Engine использует reflection system (UHT), который плохо работает с namespaces. Вместо этого используются префиксы классов и модульные макросы.

---

## Обновление Build.cs

При миграции не забывайте обновлять зависимости:

```csharp
// EquipmentSystem.Build.cs
public class EquipmentSystem : ModuleRules
{
    public EquipmentSystem(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "SuspenseCore",          // Core utilities
            "InventorySystem",       // For item system
            "GAS",                   // For abilities
            "GameplayAbilities",
            "GameplayTags"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "CoreUObject",
            "Engine",
            "NetCore"                // For replication
        });
    }
}
```

---

## Типичные проблемы и решения

### Проблема: Циклические зависимости между модулями

**Решение:** Используйте BridgeSystem как mediator:

```cpp
// Instead of direct dependency PlayerCore → EquipmentSystem
// Use: PlayerCore → BridgeSystem ← EquipmentSystem

// In PlayerCore
class UModuleBridge; // Forward declaration

void APlayerCharacter::EquipWeapon(UItemInstance* Weapon)
{
    // Get equipment manager via bridge
    UEquipmentManager* EquipMgr = Bridge->GetService<UEquipmentManager>();
    if (EquipMgr)
    {
        EquipMgr->EquipItem(this, Weapon);
    }
}
```

### Проблема: Replication не работает после миграции

**Чеклист:**
1. ✅ Actor/Component имеет `SetIsReplicated(true)`?
2. ✅ Properties помечены `UPROPERTY(Replicated)`?
3. ✅ `GetLifetimeReplicatedProps()` реализован?
4. ✅ `DOREPLIFETIME()` макрос используется?
5. ✅ Тестируете с dedicated server?

### Проблема: GAS abilities не активируются

**Решение:**
1. Проверьте, что ASC инициализирован: `InitAbilityActorInfo()`
2. Ability granted: `GiveAbility()`
3. GameplayTags настроены корректно
4. Проверьте activation policies и costs

---

## Порядок миграции модулей (рекомендуется)

1. **SuspenseCore** → базовые утилиты и интерфейсы
2. **BridgeSystem** → коммуникация между модулями
3. **GAS** → базовые ability классы и attribute sets
4. **PlayerCore** → character и controller
5. **InventorySystem** → item system
6. **EquipmentSystem** → weapon и equipment (зависит от Inventory)
7. **InteractionSystem** → world interactions
8. **UISystem** → UI (последний, зависит от всех)

---

## Полезные команды для миграции

### Поиск всех включений старого header

```bash
# В корне проекта
grep -r "OldClassName.h" Source/
```

### Переименование класса во всех файлах

```bash
# Используйте IDE refactoring tools (Rider/VS)
# Или sed (осторожно!):
find Source/ -type f -name "*.cpp" -o -name "*.h" | xargs sed -i 's/OldClassName/NewClassName/g'
```

### Проверка compile после миграции

```bash
# Rebuild из командной строки
[UE_ROOT]/Engine/Build/BatchFiles/Build.bat YourProjectEditor Win64 Development -Project="[PATH]/YourProject.uproject"
```

---

## Когда создавать новый класс вместо миграции

Создавайте новый класс если:
- Старый код не соответствует современным стандартам UE
- Архитектура кардинально изменилась (например, переход на GAS)
- Старый класс имеет множество legacy зависимостей
- Проще написать заново, чем рефакторить

Мигрируйте существующий класс если:
- Логика сложная и хорошо протестирована
- Класс уже следует хорошим практикам
- Требуются минимальные изменения

---

**Последнее обновление:** 2025-11-23
