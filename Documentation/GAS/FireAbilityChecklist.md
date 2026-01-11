# Fire Ability - Чеклист разработки

## Перед началом работы

### 1. Изучить существующий код

```
Source/GAS/Private/SuspenseCore/Abilities/Weapon/
├── SuspenseCoreAimDownSightAbility.cpp   ✅ Изучить паттерны
├── GA_WeaponSwitch.cpp                    ✅ EventBus публикация
└── [Legacy Fire код - будет предоставлен]
```

### 2. Проверить компоненты

| Компонент | Файл | Что делает |
|-----------|------|------------|
| AmmoComponent | `SuspenseCoreAmmoComponent.h` | Счётчик патронов |
| MagazineComponent | `SuspenseCoreMagazineComponent.h` | Калибр, магазин |
| FireModeComponent | `SuspenseCoreFireModeComponent.h` | Режимы стрельбы |
| WeaponStanceComponent | `SuspenseCoreWeaponStanceComponent.h` | Состояние (firing, reloading) |

### 3. Проверить интерфейсы (BridgeSystem)

```cpp
// Source/BridgeSystem/Public/SuspenseCore/Interfaces/Weapon/
ISuspenseCoreWeapon              // Данные оружия
ISuspenseCoreWeaponCombatState   // Состояние (SetFiring, IsFiring)
ISuspenseCoreAmmoProvider        // Доступ к патронам
```

---

## Архитектура Fire Ability

### Основной флоу

```
Input (Fire) → CanActivateAbility() → ActivateAbility()
                    ↓                        ↓
              [Проверки]              [Начало стрельбы]
                    ↓                        ↓
              - HasAmmo?              - SetFiring(true)
              - IsReloading?          - FireShot()
              - IsBlocked?            - ConsumeAmmo()
                                      - ApplyRecoil()
                                      - Publish Event
                                            ↓
                                      [UI обновляется]
```

### Fire Modes

```
Single Mode:
  Input Press → FireShot() → EndAbility()

Burst Mode:
  Input Press → FireShot() x3 (с задержкой) → EndAbility()

Auto Mode:
  Input Press → StartAutoFire() → FireShot() каждые N ms
  Input Release → StopAutoFire() → EndAbility()
```

---

## Критические точки

### 1. Получение WeaponActor

```cpp
// ✅ ПРАВИЛЬНО
AActor* WeaponActor = ISuspenseCoreCharacterInterface::Execute_GetCurrentWeaponActor(AvatarActor);

// ❌ НЕПРАВИЛЬНО - компоненты на WeaponActor, не на Character!
// USuspenseCoreAmmoComponent* = AvatarActor->FindComponentByClass<...>();
```

### 2. Обновление состояния

```cpp
// Состояние оружия через WeaponStanceComponent (на Character)
if (USuspenseCoreWeaponStanceComponent* StanceComp = Character->FindComponentByClass<USuspenseCoreWeaponStanceComponent>())
{
    StanceComp->SetFiring(true);   // При начале стрельбы
    StanceComp->SetFiring(false);  // При окончании
}
```

### 3. EventBus события

```cpp
// Обязательные события для UI:
SuspenseCoreTags::Event::Equipment::WeaponFired   // После каждого выстрела
SuspenseCoreTags::Event::Equipment::AmmoChanged   // При изменении патронов
SuspenseCoreTags::Event::Equipment::AmmoEmpty     // Когда закончились
```

### 4. DI Compliance

```cpp
// ✅ Разрешённые include для GAS модуля:
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"           // BridgeSystem
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeapon.h"   // BridgeSystem
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"             // BridgeSystem

// ❌ ЗАПРЕЩЁННЫЕ include:
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"    // EquipmentSystem!
#include "SuspenseCore/Components/SuspenseCoreAmmoComponent.h"    // EquipmentSystem!
```

**НО**: Можно использовать `FindComponentByClass<>()` для получения компонентов - это не создаёт compile-time зависимость, только runtime.

---

## Данные для EventData

### WeaponFired Event

```cpp
FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(AvatarActor);
EventData.SetInt(FName("CurrentAmmo"), AmmoComp->GetCurrentAmmo());
EventData.SetInt(FName("MaxAmmo"), AmmoComp->GetMaxAmmo());
EventData.SetInt(FName("SlotIndex"), SlotIndex);
EventData.SetString(FName("FireMode"), FireModeToString(CurrentFireMode));
EventData.SetBool(FName("IsLastShot"), AmmoComp->GetCurrentAmmo() == 0);
```

### AmmoChanged Event

```cpp
EventData.SetInt(FName("CurrentAmmo"), NewAmmo);
EventData.SetInt(FName("MaxAmmo"), MaxAmmo);
EventData.SetInt(FName("Delta"), -1);  // Сколько изменилось
EventData.SetObject(FName("Target"), AvatarActor);
```

---

## Теги для конфигурации

```cpp
// Ability Tags
AbilityTags.AddTag(SuspenseCoreTags::Ability::Weapon::Fire);

// Activation Owned Tags (добавляются при активации)
ActivationOwnedTags.AddTag(SuspenseCoreTags::State::Firing);

// Blocking Tags (блокируют активацию)
ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Reloading);
ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Dead);
ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Stunned);
ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Disabled);
ActivationBlockedTags.AddTag(SuspenseCoreTags::State::WeaponBlocked);
ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Sprinting);  // Опционально
```

---

## Последовательность реализации

### Phase 1: Базовая структура
- [ ] Создать `SuspenseCoreFireAbility.h/cpp`
- [ ] Настроить теги и input binding
- [ ] Реализовать `CanActivateAbility()` с проверками
- [ ] Реализовать базовый `ActivateAbility()` → `FireShot()` → `EndAbility()`

### Phase 2: Fire Modes
- [ ] Добавить поддержку Single mode
- [ ] Добавить поддержку Burst mode (таймер)
- [ ] Добавить поддержку Auto mode (hold input)

### Phase 3: EventBus интеграция
- [ ] Публиковать WeaponFired event
- [ ] Публиковать AmmoChanged event
- [ ] Публиковать AmmoEmpty event
- [ ] Проверить что UI обновляется

### Phase 4: Сетевая репликация
- [ ] Настроить `NetExecutionPolicy`
- [ ] Добавить серверную валидацию
- [ ] Проверить предсказание на клиенте

### Phase 5: Полировка
- [ ] Отдача (recoil)
- [ ] Spread (разброс)
- [ ] Звуки и эффекты
- [ ] Интеграция с Damage System

---

## Связанные документы

- [WeaponAbilityDevelopmentGuide.md](./WeaponAbilityDevelopmentGuide.md) - Полная документация
- [WeaponSSOTReference.md](../Data/WeaponSSOTReference.md) - DataTable и калибры
- Существующие Abilities для референса:
  - `SuspenseCoreAimDownSightAbility.cpp`
  - `GA_WeaponSwitch.cpp`
