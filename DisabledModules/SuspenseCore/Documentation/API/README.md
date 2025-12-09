# API Documentation

API Reference для всех модулей SuspenseCore plugin.

**Статус проекта:** ✅ Миграция завершена, код компилируется
**Дата обновления:** 2025-11-28

---

## Статус документации

| Модуль | Код | API Docs | Документ |
|--------|-----|----------|----------|
| SuspenseCore | ✅ Готов | 🔄 В разработке | [SuspenseCore.md](SuspenseCore.md) |
| BridgeSystem | ✅ Готов | 🔄 В разработке | [BridgeSystem.md](BridgeSystem.md) |
| GAS | ✅ Готов | 🔄 В разработке | [GAS.md](GAS.md) |
| PlayerCore | ✅ Готов | 🔄 В разработке | [PlayerCore.md](PlayerCore.md) |
| InventorySystem | ✅ Готов | 🔄 В разработке | [InventorySystem.md](InventorySystem.md) |
| EquipmentSystem | ✅ Готов | 🔄 В разработке | [EquipmentSystem.md](EquipmentSystem.md) |
| InteractionSystem | ✅ Готов | 🔄 В разработке | [InteractionSystem.md](InteractionSystem.md) |
| UISystem | ✅ Готов | 🔄 В разработке | [UISystem.md](UISystem.md) |

**Легенда:**
- ✅ Готово/Компилируется
- 🔄 В разработке
- ⏳ Планируется

> **Примечание:** Весь код успешно мигрирован и компилируется.
> API документация будет дополняться по мере детального описания классов.

---

## Навигация по API

### SuspenseCore (Ядро)

Базовые интерфейсы, утилиты, logging.

**Основные классы:**
- `FSuspenseCoreModule` - Main plugin module
- Core interfaces и базовые типы

[→ Полная документация](SuspenseCore.md)

---

### BridgeSystem (Межмодульная коммуникация)

Service Locator, Event Bus, Dependency Injection.

**Основные классы:**
- `UModuleBridge` - Service locator
- `UEventBus` - Event messaging system
- `USuspenseServiceRegistry` - Service registration

[→ Полная документация](BridgeSystem.md)

---

### GAS (Gameplay Ability System)

Интеграция и расширения для Gameplay Ability System.

**Основные классы:**
- `USuspenseAbilitySystemComponent` - Custom ASC
- `USuspenseAttributeSet` - Base attributes
- `USuspenseGameplayAbility` - Base ability class
- GameplayTags structure

[→ Полная документация](GAS.md)

---

### PlayerCore (Системы игрока)

Character, Controller, Camera, Input.

**Основные классы:**
- `APlayerCharacterBase` - Base character
- `APlayerControllerBase` - Base controller
- Camera management components

[→ Полная документация](PlayerCore.md)

---

### InventorySystem (Инвентарь)

Item management, stacking, replication.

**Основные классы:**
- `UItemDefinition` - Item data asset
- `UItemInstance` - Runtime item instance
- `UInventoryComponent` - Inventory management

[→ Полная документация](InventorySystem.md)

---

### EquipmentSystem (Экипировка)

Weapon and equipment management.

**Основные классы:**
- `UEquipmentDefinition` - Equipment data
- `UEquipmentManagerComponent` - Equipment slots
- `AWeaponBase` - Base weapon actor

[→ Полная документация](EquipmentSystem.md)

---

### InteractionSystem (Взаимодействия)

Object interaction framework.

**Основные классы:**
- `IInteractable` - Interaction interface
- `UInteractionComponent` - Detection component
- `AInteractableActor` - Base interactable

[→ Полная документация](InteractionSystem.md)

---

### UISystem (UI)

User interface framework.

**Основные классы:**
- `USuspenseHUD` - Main HUD widget
- `UInventoryWidget` - Inventory UI
- `UEquipmentWidget` - Equipment UI

[→ Полная документация](UISystem.md)

---

## Быстрый поиск

### По категориям

- **Replication**: См. все модули (все системы replicated)
- **GAS Integration**: GAS, PlayerCore, EquipmentSystem
- **UI Binding**: UISystem, InventorySystem, EquipmentSystem
- **Networking**: All modules (server-authoritative)

### По Use Cases

**Создание персонажа:**
→ PlayerCore: `APlayerCharacterBase`

**Добавление способностей:**
→ GAS: `USuspenseGameplayAbility`

**Работа с инвентарем:**
→ InventorySystem: `UInventoryComponent`

**Экипировка оружия:**
→ EquipmentSystem: `UEquipmentManagerComponent`

**Взаимодействие с миром:**
→ InteractionSystem: `IInteractable`

---

## Соглашения об именовании

### Классы

```cpp
U* - UObject-derived class
    UInventoryComponent
    UItemInstance

A* - AActor-derived class
    APlayerCharacterBase
    AWeaponActor

F* - Struct/POD type
    FItemData
    FEquipmentSlotInfo

E* - Enum (old style) / enum class (new)
    enum class EEquipmentSlot : uint8

I* - Interface
    IInteractable
    IEquippable
```

### Functions

```cpp
// PascalCase (UE standard)
void AddItemToInventory();
bool CanEquipWeapon() const;

// Blueprint callable: usually match C++ name
UFUNCTION(BlueprintCallable)
void EquipWeapon(AWeapon* Weapon);
```

### Variables

```cpp
// Member variables: descriptive PascalCase
int32 MaxHealthPoints;
float MovementSpeed;

// Booleans: 'b' prefix
bool bIsEquipped;
bool bCanInteract;

// Pointers: no prefix (UE5 style with TObjectPtr)
TObjectPtr<UInventoryComponent> Inventory;
TObjectPtr<AActor> CachedActor;
```

---

## Генерация документации

### Автоматическая генерация (планируется)

Будет использоваться Doxygen для генерации API reference из комментариев в коде.

**Конфигурация:**
```bash
# В корне проекта
doxygen Doxyfile
```

**Output:** HTML documentation в `Docs/Generated/`

### Ручное обновление

При добавлении новых публичных классов:

1. Добавить описание в соответствующий модуль .md файл
2. Обновить таблицу статуса в этом README
3. Добавить примеры использования
4. Обновить Changelog.md

---

**Последнее обновление:** 2025-11-28
**Статус миграции:** ✅ Завершена
