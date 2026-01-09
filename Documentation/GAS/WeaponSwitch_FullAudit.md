# Weapon Switch System - Full Audit Report

> **Дата:** 2026-01-09
> **Версия:** 1.0
> **Цель:** Полный пайплайн правок переключения активного оружия в слотах экипировки

---

## Содержание

1. [Executive Summary](#executive-summary)
2. [Архитектура Проекта](#архитектура-проекта)
3. [GAS Ability System](#gas-ability-system)
4. [Пайплайн Переключения Оружия](#пайплайн-переключения-оружия)
5. [UI и Данные о Патронах](#ui-и-данные-о-патронах)
6. [Native Tags Registry](#native-tags-registry)
7. [Текущие Проблемы и Gaps](#текущие-проблемы-и-gaps)
8. [Чеклист Реализации](#чеклист-реализации)

---

## Executive Summary

### Ключевые Компоненты Системы

| Компонент | Путь | Роль |
|-----------|------|------|
| `UGA_WeaponSwitch` | `Source/GAS/*/Abilities/Equipment/GA_WeaponSwitch.h/cpp` | GAS Ability для переключения слотов (0-3) |
| `ISuspenseCoreEquipmentDataProvider` | `Source/BridgeSystem/*/Interfaces/Equipment/` | Интерфейс доступа к данным экипировки |
| `USuspenseCoreEquipmentDataStore` | `Source/EquipmentSystem/*/Components/Core/` | SSOT для хранения слотов экипировки |
| `USuspenseCoreAmmoCounterWidget` | `Source/UISystem/*/Widgets/HUD/` | UI виджет патронов (Tarkov-style) |
| `USuspenseCoreWeaponStanceComponent` | `Source/EquipmentSystem/*/Components/` | SSOT для боевых состояний оружия |

### Паттерны Проекта (Обязательно Соблюдать)

1. **SSOT (Single Source of Truth)** - DataManager для item data, DataStore для equipment
2. **EventBus** - Вся коммуникация через события, НЕ прямые вызовы
3. **Native Tags** - Обязательно использовать `UE_DECLARE_GAMEPLAY_TAG_EXTERN`, НЕ `RequestGameplayTag()`
4. **Interface Segregation** - GAS использует интерфейсы, не конкретные классы
5. **Server Authority** - Все мутации валидируются на сервере

---

## Архитектура Проекта

### Модульная Структура (Без Циклических Зависимостей)

```
┌─────────────────────────────────────────────────────────────────────┐
│                        BridgeSystem (PreDefault)                     │
│  - Interfaces: ISuspenseCoreEquipmentDataProvider                   │
│  - Interfaces: ISuspenseCoreWeaponCombatState                       │
│  - Tags: SuspenseCoreGameplayTags.h                                 │
│  - Tags: SuspenseCoreEquipmentNativeTags.h (EquipmentSystem)        │
│  - Types: FSuspenseCoreEventData, FSuspenseCoreInventoryItemInstance│
│  - EventBus: USuspenseCoreEventBus                                  │
└─────────────────────────────────────────────────────────────────────┘
                         │
         ┌───────────────┼───────────────┐
         │               │               │
         ▼               ▼               ▼
┌──────────────┐ ┌──────────────┐ ┌──────────────┐
│     GAS      │ │EquipmentSys │ │  PlayerCore  │
│  (PreDefault)│ │  (Default)   │ │  (Default)   │
│              │ │              │ │              │
│ Uses:        │ │ Implements:  │ │ Uses:        │
│ IEquipData.. │ │ IEquipData.. │ │ Concrete     │
│ Provider     │ │ Provider     │ │ Components   │
└──────────────┘ └──────────────┘ └──────────────┘
```

### Слоты Экипировки (17 Canonical Slots)

```
Index  Slot Tag                          Тип
─────────────────────────────────────────────────────────
0      Equipment.Slot.PrimaryWeapon      Основное оружие
1      Equipment.Slot.SecondaryWeapon    Вторичное оружие
2      Equipment.Slot.Holster            Пистолет
3      Equipment.Slot.Scabbard           Нож/Ближний бой
4      Equipment.Slot.Headwear           Головной убор
5      Equipment.Slot.Earpiece           Гарнитура
6      Equipment.Slot.Eyewear            Очки
7      Equipment.Slot.FaceCover          Маска
8      Equipment.Slot.BodyArmor          Бронежилет
9      Equipment.Slot.TacticalRig        Разгрузка
10     Equipment.Slot.Backpack           Рюкзак
11     Equipment.Slot.SecureContainer    Безопасный контейнер
12     Equipment.Slot.Armband            Нарукавная повязка
13     Equipment.Slot.QuickSlot1         Быстрый слот 1
14     Equipment.Slot.QuickSlot2         Быстрый слот 2
15     Equipment.Slot.QuickSlot3         Быстрый слот 3
16     Equipment.Slot.QuickSlot4         Быстрый слот 4
```

**Weapon Slots для переключения:** 0-3 (Primary, Secondary, Holster, Scabbard)

---

## GAS Ability System

### UGA_WeaponSwitch - Базовый Класс

**Расположение:** `Source/GAS/Public/SuspenseCore/Abilities/Equipment/GA_WeaponSwitch.h`

```cpp
UCLASS(Abstract)
class GAS_API UGA_WeaponSwitch : public USuspenseCoreAbility
{
    // Target slot index (0-3)
    UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", ClampMax = "3"))
    int32 TargetSlotIndex;

protected:
    // Checks: slot occupied && not already active && no blocking tags
    virtual bool CanActivateAbility(...) const override;

    // Calls SetActiveWeaponSlot() + publishes EventBus event
    virtual void ActivateAbility(...) override;

private:
    // Finds ISuspenseCoreEquipmentDataProvider on PlayerState
    ISuspenseCoreEquipmentDataProvider* GetEquipmentDataProvider() const;
};
```

### Конкретные Реализации

| Класс | Key | Slot | Ability Tag |
|-------|-----|------|-------------|
| `UGA_WeaponSwitch_Primary` | 1 | 0 | `SuspenseCore.Ability.WeaponSlot.Primary` |
| `UGA_WeaponSwitch_Secondary` | 2 | 1 | `SuspenseCore.Ability.WeaponSlot.Secondary` |
| `UGA_WeaponSwitch_Sidearm` | 3 | 2 | `SuspenseCore.Ability.WeaponSlot.Sidearm` |
| `UGA_WeaponSwitch_Melee` | V | 3 | `SuspenseCore.Ability.WeaponSlot.Melee` |

### Blocking Tags (Нельзя Переключать)

```cpp
ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Stunned")));
ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Disabled")));
ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Reloading")));
```

### Input IDs

```cpp
// ESuspenseCoreAbilityInputID (в SuspenseCoreAbilityInputID.h)
WeaponSlot1,    // Key 1 → Primary
WeaponSlot2,    // Key 2 → Secondary
WeaponSlot3,    // Key 3 → Sidearm
MeleeWeapon,    // Key V → Melee
```

---

## Пайплайн Переключения Оружия

### Полный Flow (от Input до UI Update)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ 1. INPUT LAYER                                                               │
├─────────────────────────────────────────────────────────────────────────────┤
│   Player Press Key 1/2/3/V                                                  │
│          │                                                                   │
│          ▼                                                                   │
│   EnhancedInput → InputAction → AbilityInputID                              │
│          │                                                                   │
│          ▼                                                                   │
│   ASC->TryActivateAbilitiesByTag(SuspenseCoreTags::Ability::WeaponSlot::*)  │
└─────────────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ 2. GAS ABILITY ACTIVATION                                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│   UGA_WeaponSwitch::CanActivateAbility()                                    │
│   ├── Check blocking tags (Dead, Stunned, Reloading)                        │
│   ├── Get ISuspenseCoreEquipmentDataProvider from PlayerState               │
│   ├── IsSlotOccupied(TargetSlotIndex)?                                      │
│   └── GetActiveWeaponSlot() != TargetSlotIndex?                             │
│          │                                                                   │
│          ▼ (if all pass)                                                     │
│   UGA_WeaponSwitch::ActivateAbility()                                       │
│   ├── CommitAbility()                                                        │
│   ├── Provider->SetActiveWeaponSlot(TargetSlotIndex)                        │
│   ├── Publish EventBus: SuspenseCoreTags::Event::Equipment::WeaponSlotSwitched
│   │     EventData: { PreviousSlot, NewSlot, Target }                        │
│   └── EndAbility() (instant)                                                 │
└─────────────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ 3. DATA STORE UPDATE                                                         │
├─────────────────────────────────────────────────────────────────────────────┤
│   USuspenseCoreEquipmentDataStore::SetActiveWeaponSlot(int32 SlotIndex)     │
│   ├── FScopeLock(&DataCriticalSection)                                       │
│   ├── DataStorage.ActiveWeaponSlot = SlotIndex                              │
│   ├── IncrementVersion()                                                     │
│   ├── Collect pending events                                                 │
│   └── BroadcastPendingEvents() [after lock release]                         │
│          │                                                                   │
│          ▼                                                                   │
│   OnSlotDataChanged delegate fired                                           │
└─────────────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ 4. EVENTBUS PROPAGATION                                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│   EventBus->Publish(TAG_Equipment_Event_WeaponSlot_Switched, EventData)     │
│          │                                                                   │
│          ├──────────────────────┬──────────────────────┬───────────────────│
│          ▼                      ▼                      ▼                    │
│   UI Widgets               Animation System       Sound System              │
│   - AmmoCounterWidget      - AnimInstance          - Equip SFX              │
│   - EquipmentWidget        - StanceComponent       - Holster SFX            │
│   - WeaponInfoWidget       - IK Updates                                     │
└─────────────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ 5. UI UPDATE (Ammo Counter Widget)                                           │
├─────────────────────────────────────────────────────────────────────────────┤
│   USuspenseCoreAmmoCounterWidget::OnActiveWeaponChangedEvent()              │
│   ├── Extract NewSlot from EventData                                         │
│   ├── Get weapon actor from slot                                             │
│   ├── Get weapon data from DataManager                                       │
│   ├── UpdateWeaponUI()      → WeaponNameText, WeaponIcon                    │
│   ├── UpdateMagazineUI()    → MagazineRoundsText, ChamberIndicatorText      │
│   ├── UpdateReserveUI()     → ReserveRoundsText, AvailableMagazinesText     │
│   ├── UpdateFireModeUI()    → FireModeText, FireModeIcon                    │
│   ├── UpdateAmmoTypeUI()    → AmmoTypeText, AmmoTypeIcon                    │
│   └── OnWeaponChanged(NewWeaponActor) [BlueprintImplementableEvent]         │
└─────────────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ 6. ANIMATION UPDATE                                                          │
├─────────────────────────────────────────────────────────────────────────────┤
│   USuspenseCoreWeaponStanceComponent                                         │
│   ├── SetCurrentWeaponType(NewWeaponArchetype)                              │
│   ├── SetWeaponDrawn(true) [triggers Draw montage]                          │
│   └── EventBus: TAG_Equipment_Event_Weapon_Stance_Drawn                     │
│          │                                                                   │
│          ▼                                                                   │
│   USuspenseCoreCharacterAnimInstance::NativeUpdateAnimation()               │
│   ├── UpdateWeaponData() → Get StanceSnapshot                               │
│   ├── CurrentWeaponType = Snapshot.WeaponArchetypeTag                       │
│   ├── GetAnimationDataForWeaponType() → DataTable lookup                    │
│   └── Apply new Stance BlendSpace, IK transforms                            │
└─────────────────────────────────────────────────────────────────────────────┘
```

### EventBus Events для Weapon Switch

| Event Tag | Когда | Payload |
|-----------|-------|---------|
| `SuspenseCoreTags::Event::Equipment::WeaponSlotSwitched` | При переключении слота | PreviousSlot, NewSlot, Target |
| `TAG_Equipment_Event_WeaponSlot_Switched` | То же (Equipment module) | SlotIndex, WeaponData |
| `TAG_Equipment_Event_ItemEquipped` | Когда оружие экипировано визуально | SlotIndex, ItemInstance |
| `TAG_Equipment_Event_Weapon_Stance_Drawn` | Оружие достато | WeaponType |
| `TAG_Equipment_Event_Weapon_Stance_Holstered` | Оружие убрано | WeaponType |

---

## UI и Данные о Патронах

### USuspenseCoreAmmoCounterWidget

**Layout (Tarkov-style):**
```
┌──────────────────────────────────────┐
│  [ICON]  AK-74M                      │  ← WeaponIcon + WeaponNameText
│  30+1 / 30    [5.45 PS]              │  ← MagRounds+Chamber / Capacity [AmmoType]
│  ░░░░░░░░░░   AUTO                   │  ← MagazineFillBar, FireModeText
│  Reserve: 90  Mags: 3                │  ← ReserveRoundsText, AvailableMagazinesText
└──────────────────────────────────────┘
```

### Widget Bindings (ВСЕ ОБЯЗАТЕЛЬНЫЕ)

```cpp
// Weapon Info
UPROPERTY(meta = (BindWidget))
TObjectPtr<UTextBlock> WeaponNameText;
TObjectPtr<UImage> WeaponIcon;

// Magazine Display
TObjectPtr<UTextBlock> MagazineRoundsText;     // "30"
TObjectPtr<UTextBlock> ChamberIndicatorText;   // "+1"
TObjectPtr<UTextBlock> MagazineCapacityText;   // "/30"
TObjectPtr<UProgressBar> MagazineFillBar;

// Ammo Type
TObjectPtr<UTextBlock> AmmoTypeText;           // "5.45 PS"
TObjectPtr<UImage> AmmoTypeIcon;

// Reserve Info
TObjectPtr<UTextBlock> ReserveRoundsText;      // "90"
TObjectPtr<UTextBlock> AvailableMagazinesText; // "3"

// Fire Mode
TObjectPtr<UTextBlock> FireModeText;           // "AUTO"
TObjectPtr<UImage> FireModeIcon;
```

### EventBus Subscriptions (Push-Based)

```cpp
// В NativeConstruct():
SetupEventSubscriptions();

// Subscribed events:
TAG_Equipment_Event_Magazine_Inserted
TAG_Equipment_Event_Magazine_Ejected
TAG_Equipment_Event_Magazine_RoundsChanged
TAG_Equipment_Event_Weapon_AmmoChanged
TAG_Equipment_Event_Weapon_FireModeChanged
TAG_Equipment_Event_ItemEquipped          // Active weapon change!
TAG_Equipment_Event_Weapon_ReloadStart
TAG_Equipment_Event_Weapon_ReloadEnd
```

### Update Flow при Смене Оружия

```cpp
void OnActiveWeaponChangedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& Data)
{
    // 1. Get new weapon info
    int32 NewSlot = Data.GetInt(FName("NewSlot"));
    AActor* NewWeapon = Data.GetObject(FName("WeaponActor"));

    // 2. Cache weapon actor
    CachedWeaponActor = NewWeapon;

    // 3. Update all UI elements
    UpdateWeaponUI();      // Name, Icon
    UpdateMagazineUI();    // Rounds, Chamber, Capacity, FillBar
    UpdateReserveUI();     // Reserve, Available Mags
    UpdateFireModeUI();    // Mode text and icon
    UpdateAmmoTypeUI();    // Ammo type

    // 4. Fire Blueprint event
    OnWeaponChanged(NewWeapon);
}
```

---

## Native Tags Registry

### SuspenseCoreGameplayTags.h (BridgeSystem)

```cpp
namespace SuspenseCoreTags
{
    namespace Event::Equipment
    {
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Initialized);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(SlotChanged);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(LoadoutChanged);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(WeaponSlotSwitched);  // ← Ключевой тег!
    }

    namespace Ability::WeaponSlot
    {
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Primary);    // Key 1
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Secondary);  // Key 2
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sidearm);    // Key 3
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Melee);      // Key V
    }

    namespace State
    {
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Dead);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stunned);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Disabled);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Reloading);  // Blocks weapon switch
    }
}
```

### SuspenseCoreEquipmentNativeTags.h (EquipmentSystem)

```cpp
namespace SuspenseCoreEquipmentTags
{
    namespace Event
    {
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_WeaponSlot_Switched);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_ItemEquipped);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_ItemUnequipped);

        // Magazine events (for UI)
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Magazine_Inserted);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Magazine_Ejected);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Magazine_RoundsChanged);

        // Weapon events
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_AmmoChanged);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_FireModeChanged);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_ReloadStart);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_ReloadEnd);

        // Stance events
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_Stance_Drawn);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_Stance_Holstered);
    }
}
```

---

## Текущие Проблемы и Gaps

### 1. DEPRECATION WARNING в GA_WeaponSwitch.cpp

```cpp
// Строки 207-209, 217-219, 227-229, 237-239
PRAGMA_DISABLE_DEPRECATION_WARNINGS
AbilityTags.AddTag(SuspenseCoreTags::Ability::WeaponSlot::Primary);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
```

**Проблема:** Используется deprecated `AbilityTags.AddTag()` вместо `SetAssetTags()`.

**Решение:**
```cpp
// Заменить на:
SetAssetTags(FGameplayTagContainer(SuspenseCoreTags::Ability::WeaponSlot::Primary));
```

### 2. RequestGameplayTag в Blocking Tags

```cpp
// GA_WeaponSwitch.cpp:27-30
ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
```

**Проблема:** Используется `RequestGameplayTag()` вместо native tags.

**Решение:**
```cpp
// Заменить на native tags:
ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Dead);
ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Stunned);
ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Disabled);
ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Reloading);
```

### 3. Отсутствует Анимация Переключения

**Текущее состояние:**
```cpp
// GA_WeaponSwitch.cpp:133-134
// Instant ability - end immediately
// Future: Add animation montage wait here
EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
```

**Требуется:**
- Добавить Holster montage для текущего оружия
- Добавить Draw montage для нового оружия
- Wait for montages completion перед EndAbility

### 4. Визуальное Обновление Оружия

**Текущий flow не включает:**
- Spawn/Attach нового WeaponActor
- Destroy/Detach старого WeaponActor
- Обновление WeaponStanceComponent

**Требуется интеграция с:**
- `USuspenseCoreEquipmentVisualizationService`
- `USuspenseCoreEquipmentAttachmentComponent`

### 5. Обновление UI изображения оружия

**AmmoCounterWidget подписан на:**
- `TAG_Equipment_Event_ItemEquipped`

**Но не ясно:**
- Кто публикует этот event при weapon switch?
- Как получить WeaponIcon texture из DataManager?

---

## Чеклист Реализации

### Phase 1: Исправление Текущего Кода

- [ ] Заменить `AbilityTags.AddTag()` на `SetAssetTags()` в GA_WeaponSwitch конструкторах
- [ ] Заменить `RequestGameplayTag()` на native tags в blocking tags
- [ ] Добавить proper logging с категорией `LogWeaponSwitch`

### Phase 2: Визуальное Переключение

- [ ] Интегрировать GA_WeaponSwitch с EquipmentVisualizationService
- [ ] Добавить Holster montage для старого оружия
- [ ] Добавить Draw montage для нового оружия
- [ ] Обновить WeaponStanceComponent при переключении

### Phase 3: UI Обновление

- [ ] Убедиться что TAG_Equipment_Event_ItemEquipped публикуется
- [ ] Добавить WeaponIcon в FSuspenseCoreEventData payload
- [ ] Реализовать UpdateWeaponUI() с загрузкой иконки из DataManager
- [ ] Тестировать обновление MagazineUI при смене оружия

### Phase 4: Ammo Data Flow

- [ ] При смене оружия - получить ammo data из нового WeaponActor
- [ ] Обновить CachedAmmoData в AmmoCounterWidget
- [ ] Проверить FireMode и AmmoType updates

### Phase 5: Animation Integration

- [ ] Обновить WeaponStanceComponent.CurrentWeaponType
- [ ] Trigger correct animation data from DataTable
- [ ] Apply IK transforms для нового оружия
- [ ] Test aim/fire/reload с новым оружием

### Phase 6: Replication

- [ ] Проверить репликацию ActiveWeaponSlot
- [ ] Проверить визуальную синхронизацию на клиентах
- [ ] Проверить UI sync на всех клиентах

---

## Файлы для Изменений

### Обязательные

1. `Source/GAS/Private/SuspenseCore/Abilities/Equipment/GA_WeaponSwitch.cpp`
   - Fix deprecation warnings
   - Use native tags for blocking

2. `Source/GAS/Public/SuspenseCore/Abilities/Equipment/GA_WeaponSwitch.h`
   - Add montage references (optional for Phase 2)

### Вероятные

3. `Source/EquipmentSystem/Private/SuspenseCore/Services/SuspenseCoreEquipmentVisualizationService.cpp`
   - Integration with weapon switch

4. `Source/EquipmentSystem/Private/SuspenseCore/Components/SuspenseCoreWeaponStanceComponent.cpp`
   - Update on weapon switch

5. `Source/UISystem/Private/SuspenseCore/Widgets/HUD/SuspenseCoreAmmoCounterWidget.cpp`
   - Handle weapon switch event properly

---

## Рекомендации

### DO (Делать)

✅ Использовать Native Tags везде (`SuspenseCoreTags::*`, `SuspenseCoreEquipmentTags::*`)
✅ Публиковать events через EventBus для decoupling
✅ Следовать SSOT паттерну (DataStore → EventBus → UI)
✅ Использовать ISuspenseCoreEquipmentDataProvider интерфейс
✅ Server authority для SetActiveWeaponSlot

### DON'T (Не делать)

❌ Использовать `RequestGameplayTag()` для критических тегов
❌ Прямые вызовы между модулями (GAS → EquipmentSystem)
❌ Polling state вместо EventBus subscription
❌ Модификация state без authority check
❌ `AbilityTags.AddTag()` (deprecated)

---

*Документ создан: 2026-01-09*
*Для: Pipeline правок переключения активного оружия*
