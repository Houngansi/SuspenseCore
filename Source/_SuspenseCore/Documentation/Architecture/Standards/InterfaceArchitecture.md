# Interface Architecture Standards - ОБЯЗАТЕЛЬНО К ИСПОЛНЕНИЮ!

**КРИТИЧЕСКИ ВАЖНО**: Эти правила ОБЯЗАТЕЛЬНЫ для всех SuspenseCore разработок!

---

## ⚠️ GOLDEN RULE: Интерфейсы SuspenseCore

> **ВСЕ новые интерфейсы ДОЛЖНЫ создаваться в `BridgeSystem/Public/SuspenseCore/Interfaces/`**
>
> **НИКОГДА не используйте legacy интерфейсы (`ISuspenseInteract`, `ISuspensePickup`, etc.) в новом SuspenseCore коде!**

---

## ❌ ГРУБАЯ ОШИБКА (НЕ ДЕЛАЙ ТАК!)

```cpp
// ❌ НЕПРАВИЛЬНО: Использование legacy интерфейсов
class ASuspenseCorePickupItem
    : public AActor
    , public ISuspenseInteract       // ❌ LEGACY - НЕ ИСПОЛЬЗОВАТЬ!
    , public ISuspensePickup         // ❌ LEGACY - НЕ ИСПОЛЬЗОВАТЬ!
    , public ISuspenseCoreEventEmitter
{
    // ...
};
```

**Проблемы:**
1. Смешение legacy и SuspenseCore архитектур
2. Зависимость от устаревших структур данных
3. Нарушение принципа единой ответственности
4. Препятствие миграции

---

## ✅ ПРАВИЛЬНЫЙ ПОДХОД

```cpp
// ✅ ПРАВИЛЬНО: Только SuspenseCore интерфейсы
class ASuspenseCorePickupItem
    : public AActor
    , public ISuspenseCoreInteractable   // ✅ НОВЫЙ интерфейс
    , public ISuspenseCorePickup         // ✅ НОВЫЙ интерфейс
    , public ISuspenseCoreEventEmitter   // ✅ EventBus интерфейс
{
    // ...
};
```

---

## 📁 Структура Интерфейсов

### Legacy Интерфейсы (НЕ ИСПОЛЬЗОВАТЬ в новом коде!)

Расположение: `BridgeSystem/Public/Interfaces/`

```
BridgeSystem/Public/Interfaces/
├── Interaction/
│   ├── ISuspenseInteract.h          ❌ LEGACY
│   ├── ISuspensePickup.h            ❌ LEGACY
│   └── ISuspenseItemFactoryInterface.h  ❌ LEGACY
├── Inventory/
│   ├── ISuspenseInventory.h         ❌ LEGACY
│   └── ISuspenseInventoryItem.h     ❌ LEGACY
├── Equipment/
│   └── (22 legacy interfaces)       ❌ LEGACY
└── ...
```

### SuspenseCore Интерфейсы (ИСПОЛЬЗОВАТЬ!)

Расположение: `BridgeSystem/Public/SuspenseCore/Interfaces/`

```
BridgeSystem/Public/SuspenseCore/Interfaces/
├── Interaction/
│   ├── ISuspenseCoreInteractable.h  ✅ НОВЫЙ
│   └── ISuspenseCorePickup.h        ✅ НОВЫЙ
├── Inventory/
│   ├── ISuspenseCoreInventory.h     ✅ НОВЫЙ
│   └── ISuspenseCoreInventoryItem.h ✅ НОВЫЙ
├── Factory/
│   └── ISuspenseCoreItemFactory.h   ✅ НОВЫЙ
└── SuspenseCoreUIController.h       ✅ (уже существует)
```

---

## 📐 Правила Создания Интерфейсов

### 1. Префикс именования

```cpp
// ✅ ПРАВИЛЬНО: Префикс ISuspenseCore
class ISuspenseCoreInteractable {};
class ISuspenseCoreInventory {};
class ISuspenseCorePickup {};

// ❌ НЕПРАВИЛЬНО: Legacy префикс
class ISuspenseInteract {};  // Без "Core" = legacy
```

### 2. Использование EventBus

Все SuspenseCore интерфейсы должны поддерживать EventBus:

```cpp
// ✅ ПРАВИЛЬНО: EventBus интеграция
class ISuspenseCoreInteractable
{
    // Используем EventBus вместо прямых делегатов
    virtual void EmitInteractionEvent(
        FGameplayTag EventTag,
        const FSuspenseCoreEventData& Data) = 0;
};
```

### 3. Зависимости только от SuspenseCore типов

```cpp
// ✅ ПРАВИЛЬНО: Зависимость от SuspenseCore типов
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Types/SuspenseCoreItemTypes.h"

// ❌ НЕПРАВИЛЬНО: Зависимость от legacy типов
#include "Types/Loadout/SuspenseItemDataTable.h"  // FSuspenseUnifiedItemData
```

### 4. Минимальный интерфейс

```cpp
// ✅ ПРАВИЛЬНО: Минимальный, focused интерфейс
class ISuspenseCoreInteractable
{
public:
    virtual bool CanInteract(APlayerController* Instigator) const = 0;
    virtual bool Interact(APlayerController* Instigator) = 0;
    virtual FGameplayTag GetInteractionType() const = 0;
    virtual FText GetInteractionPrompt() const = 0;
};

// ❌ НЕПРАВИЛЬНО: Раздутый интерфейс с множеством обязанностей
class ISuspenseInteract
{
    // 20+ методов...
    virtual USuspenseEventManager* GetDelegateManager() const = 0;  // Не нужно с EventBus!
};
```

---

## 🔄 Миграция с Legacy на SuspenseCore

### Шаг 1: Создать новый интерфейс

```cpp
// BridgeSystem/Public/SuspenseCore/Interfaces/Interaction/ISuspenseCoreInteractable.h
UINTERFACE(MinimalAPI, Blueprintable, meta = (CannotImplementInterfaceInBlueprint))
class USuspenseCoreInteractable : public UInterface
{
    GENERATED_BODY()
};

class BRIDGESYSTEM_API ISuspenseCoreInteractable
{
    GENERATED_BODY()
public:
    // Минимальный API
    virtual bool CanInteract(APlayerController* Instigator) const = 0;
    virtual bool Interact(APlayerController* Instigator) = 0;
    virtual FGameplayTag GetInteractionType() const = 0;
    virtual FText GetInteractionPrompt() const = 0;
};
```

### Шаг 2: Реализовать в SuspenseCore классе

```cpp
// InteractionSystem/Public/SuspenseCore/Pickup/SuspenseCorePickupItem.h
#include "SuspenseCore/Interfaces/Interaction/ISuspenseCoreInteractable.h"
#include "SuspenseCore/Interfaces/Interaction/ISuspenseCorePickup.h"

class ASuspenseCorePickupItem
    : public AActor
    , public ISuspenseCoreInteractable   // ✅ НОВЫЙ
    , public ISuspenseCorePickup         // ✅ НОВЫЙ
    , public ISuspenseCoreEventEmitter
{
    // ...
};
```

### Шаг 3: Удалить зависимости от legacy

```cpp
// ❌ Удалить эти include
// #include "Interfaces/Interaction/ISuspenseInteract.h"
// #include "Interfaces/Interaction/ISuspensePickup.h"

// ✅ Использовать только SuspenseCore
#include "SuspenseCore/Interfaces/Interaction/ISuspenseCoreInteractable.h"
#include "SuspenseCore/Interfaces/Interaction/ISuspenseCorePickup.h"
```

---

## 📋 Checklist при создании SuspenseCore компонентов

Перед написанием кода проверь:

- [ ] Использую **ТОЛЬКО** интерфейсы из `SuspenseCore/Interfaces/`
- [ ] **НЕ** использую legacy интерфейсы (`ISuspenseInteract`, `ISuspensePickup`, etc.)
- [ ] Все интерфейсы имеют префикс `ISuspenseCore`
- [ ] Интерфейсы расположены в `BridgeSystem/Public/SuspenseCore/Interfaces/`
- [ ] Использую `ISuspenseCoreEventEmitter` вместо `GetDelegateManager()`
- [ ] Использую `ISuspenseCoreEventSubscriber` для подписок
- [ ] Зависимости только от `SuspenseCore/Types/`

---

## 🎯 Интерфейсы которые нужно создать

### Приоритет 1 (Interaction System):
- [x] `ISuspenseCoreEventSubscriber` - уже существует
- [x] `ISuspenseCoreEventEmitter` - уже существует
- [ ] `ISuspenseCoreInteractable` - СОЗДАТЬ
- [ ] `ISuspenseCorePickup` - СОЗДАТЬ
- [ ] `ISuspenseCoreItemFactory` - СОЗДАТЬ

### Приоритет 2 (Inventory System):
- [ ] `ISuspenseCoreInventory` - СОЗДАТЬ
- [ ] `ISuspenseCoreInventoryItem` - СОЗДАТЬ
- [ ] `ISuspenseCoreInventoryGrid` - СОЗДАТЬ

### Приоритет 3 (Equipment System):
- [ ] `ISuspenseCoreEquipment` - СОЗДАТЬ
- [ ] `ISuspenseCoreWeapon` - СОЗДАТЬ

---

## 🗂️ DataTable Architecture

### Проблема: Монолитный FSuspenseUnifiedItemData

Текущий `FSuspenseUnifiedItemData` (689 строк) - это антипаттерн:

```cpp
// ❌ ПЛОХО: Монолитный struct с 50+ полями
USTRUCT()
struct FSuspenseUnifiedItemData : public FTableRowBase
{
    // Core Identity (5 полей)
    // Type Classification (3 поля)
    // Inventory Properties (4 поля)
    // Usage Configuration (7 полей)
    // Visual Assets (4 поля)
    // Audio Assets (3 поля)
    // Equipment Configuration (8 полей)
    // GAS Integration (4 поля)
    // Weapon Configuration (15+ полей)
    // Armor Configuration (3 поля)
    // Ammo Configuration (8 полей)
    // ... 689 строк!
};
```

### Решение: Композитная архитектура

```cpp
// ✅ ХОРОШО: Декомпозированные структуры
USTRUCT()
struct FSuspenseCoreItemIdentity
{
    FName ItemID;
    FText DisplayName;
    FText Description;
    TSoftObjectPtr<UTexture2D> Icon;
};

USTRUCT()
struct FSuspenseCoreItemClassification
{
    FGameplayTag ItemType;
    FGameplayTag Rarity;
    FGameplayTagContainer ItemTags;
};

USTRUCT()
struct FSuspenseCoreInventoryProperties
{
    FIntPoint GridSize;
    int32 MaxStackSize;
    float Weight;
    int32 BaseValue;
};

// ... другие специализированные структуры

// Основной struct - композиция
USTRUCT()
struct FSuspenseCoreItemData : public FTableRowBase
{
    FSuspenseCoreItemIdentity Identity;
    FSuspenseCoreItemClassification Classification;
    FSuspenseCoreInventoryProperties InventoryProps;
    FSuspenseCoreUsageConfig UsageConfig;
    FSuspenseCoreVisualAssets Visuals;
    FSuspenseCoreAudioAssets Audio;

    // Опциональные расширения (через указатели или optional)
    TOptional<FSuspenseCoreWeaponConfig> WeaponConfig;
    TOptional<FSuspenseCoreArmorConfig> ArmorConfig;
    TOptional<FSuspenseCoreAmmoConfig> AmmoConfig;
};
```

**Преимущества:**
1. Меньшие, focused структуры
2. Легче понять и поддерживать
3. Возможность переиспользования
4. Меньше памяти для не-оружия (нет пустых weapon полей)

---

## 📝 Версионирование

| Версия | Дата | Изменения |
|--------|------|-----------|
| 1.0 | 2025-12-04 | Initial document |

---

**ВСЕГДА** следуй этому документу при создании SuspenseCore компонентов!
