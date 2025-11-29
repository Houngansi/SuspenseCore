# ЭТАП 2: GAS (Gameplay Ability System) — Детальный план

> **Статус:** ✅ ВЫПОЛНЕНО (Phase 2 Core)
> **Приоритет:** P0 (КРИТИЧЕСКИЙ)
> **Зависимости:** BridgeSystem (Этап 1)
> **Предыдущий этап:** [BridgeSystem](../BridgeSystem/README.md)

---

## 1. Созданные классы (SuspenseCore*)

| Класс | Путь | Статус |
|-------|------|--------|
| `USuspenseCoreAbilitySystemComponent` | `GAS/Public/SuspenseCore/Components/` | ✅ Создан |
| `USuspenseCoreAttributeSet` | `GAS/Public/SuspenseCore/Attributes/` | ✅ Создан |
| `USuspenseCoreShieldAttributeSet` | `GAS/Public/SuspenseCore/Attributes/` | ✅ Создан |
| `USuspenseCoreMovementAttributeSet` | `GAS/Public/SuspenseCore/Attributes/` | ✅ Создан |
| `USuspenseCoreProgressionAttributeSet` | `GAS/Public/SuspenseCore/Attributes/` | ✅ Создан |

---

## 2. Архитектура AttributeSets

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    ATTRIBUTE SETS HIERARCHY                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│                    USuspenseCoreAbilitySystemComponent                      │
│                              │                                              │
│              ┌───────────────┼───────────────┬───────────────┐              │
│              │               │               │               │              │
│              ▼               ▼               ▼               ▼              │
│   ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │
│   │ SuspenseCore │  │ SuspenseCore │  │ SuspenseCore │  │ SuspenseCore │   │
│   │ AttributeSet │  │ ShieldAttr   │  │ MovementAttr │  │ Progression  │   │
│   │              │  │ Set          │  │ Set          │  │ AttributeSet │   │
│   │ ────────────│  │ ────────────│  │ ────────────│  │ ────────────│   │
│   │ Health      │  │ Shield      │  │ WalkSpeed   │  │ Level       │   │
│   │ MaxHealth   │  │ MaxShield   │  │ SprintSpeed │  │ Experience  │   │
│   │ HealthRegen │  │ ShieldRegen │  │ CrouchSpeed │  │ XPToNext    │   │
│   │ Stamina     │  │ RegenDelay  │  │ ProneSpeed  │  │ XPMultiplier│   │
│   │ MaxStamina  │  │ BreakCool   │  │ AimSpeed    │  │ Reputation  │   │
│   │ StaminaRegen│  │ DmgReduce   │  │ JumpHeight  │  │ SoftCurrency│   │
│   │ Armor       │  │ Overflow    │  │ AirControl  │  │ HardCurrency│   │
│   │ AttackPower │  │             │  │ TurnRate    │  │ SkillPoints │   │
│   │ MoveSpeed   │  │             │  │ Weight      │  │ AttrPoints  │   │
│   │ IncomingDmg │  │             │  │ MaxWeight   │  │ Prestige    │   │
│   │ IncomingHeal│  │             │  │             │  │ SeasonRank  │   │
│   └──────────────┘  └──────────────┘  └──────────────┘  └──────────────┘   │
│                                                                              │
│   ВСЕГО: 11 атр.     ВСЕГО: 10 атр.    ВСЕГО: 18 атр.    ВСЕГО: 14 атр.    │
│                                                                              │
│                         ИТОГО: 53 АТРИБУТА                                  │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Детальное описание AttributeSets

### 3.1 USuspenseCoreAttributeSet (Базовый)

**Путь:** `GAS/Public/SuspenseCore/Attributes/SuspenseCoreAttributeSet.h`

**11 атрибутов:**

| Атрибут | Тип | Описание | Репликация |
|---------|-----|----------|------------|
| `Health` | Primary | Текущее здоровье | ✅ |
| `MaxHealth` | Primary | Максимальное здоровье | ✅ |
| `HealthRegen` | Primary | Регенерация HP/сек | ✅ |
| `Stamina` | Primary | Текущая стамина | ✅ |
| `MaxStamina` | Primary | Максимальная стамина | ✅ |
| `StaminaRegen` | Primary | Регенерация стамины/сек | ✅ |
| `Armor` | Primary | Снижение урона | ✅ |
| `AttackPower` | Primary | Множитель урона | ✅ |
| `MovementSpeed` | Primary | Множитель скорости | ✅ |
| `IncomingDamage` | Meta | Для расчёта урона | ❌ |
| `IncomingHealing` | Meta | Для расчёта лечения | ❌ |

### 3.2 USuspenseCoreShieldAttributeSet

**Путь:** `GAS/Public/SuspenseCore/Attributes/SuspenseCoreShieldAttributeSet.h`

**10 атрибутов:**

| Атрибут | Категория | Описание |
|---------|-----------|----------|
| `Shield` | Shield | Текущий щит |
| `MaxShield` | Shield | Максимальный щит |
| `ShieldRegen` | Shield | Регенерация/сек |
| `ShieldRegenDelay` | Shield | Задержка перед регеном (сек) |
| `ShieldBreakCooldown` | Break | Кулдаун после разрушения |
| `ShieldBreakRecoveryPercent` | Break | % восстановления после разрушения |
| `ShieldDamageReduction` | Damage | Снижение урона щитом (0-1) |
| `ShieldOverflowDamage` | Damage | Пробитие в HP при разрушении |
| `IncomingShieldDamage` | Meta | Входящий урон |
| `IncomingShieldHealing` | Meta | Входящее восстановление |

**Механики:**
- Щит поглощает урон до здоровья (Halo/Apex style)
- Регенерация с задержкой после получения урона
- Событие `Event.GAS.Shield.Broken` при разрушении
- Событие `Event.GAS.Shield.Low` при 25%

### 3.3 USuspenseCoreMovementAttributeSet

**Путь:** `GAS/Public/SuspenseCore/Attributes/SuspenseCoreMovementAttributeSet.h`

**18 атрибутов:**

| Атрибут | Категория | Описание |
|---------|-----------|----------|
| `WalkSpeed` | Speed | Базовая скорость ходьбы |
| `SprintSpeed` | Speed | Скорость спринта |
| `CrouchSpeed` | Speed | Скорость в присяде |
| `ProneSpeed` | Speed | Скорость ползком |
| `AimSpeed` | Speed | Скорость в прицеле |
| `BackwardSpeedMultiplier` | Speed | Множитель назад |
| `StrafeSpeedMultiplier` | Speed | Множитель боком |
| `JumpHeight` | Jump | Высота прыжка |
| `MaxJumpCount` | Jump | Макс. прыжков (double jump) |
| `AirControl` | Jump | Управление в воздухе |
| `TurnRate` | Rotation | Скорость поворота |
| `AimTurnRateMultiplier` | Rotation | Множитель в прицеле |
| `GroundAcceleration` | Acceleration | Ускорение на земле |
| `GroundDeceleration` | Acceleration | Торможение на земле |
| `AirAcceleration` | Acceleration | Ускорение в воздухе |
| `CurrentWeight` | Weight | Текущий вес |
| `MaxWeight` | Weight | Максимальный вес |
| `WeightSpeedPenalty` | Weight | Штраф от веса (авто) |

**Механики:**
- Автоматический пересчёт штрафа от веса
- Применение скоростей к CharacterMovementComponent
- Штраф начинается после 50% загрузки
- Максимальный штраф 80% при перегрузе

### 3.4 USuspenseCoreProgressionAttributeSet

**Путь:** `GAS/Public/SuspenseCore/Attributes/SuspenseCoreProgressionAttributeSet.h`

**14 атрибутов:**

| Атрибут | Категория | Описание |
|---------|-----------|----------|
| `Level` | Level | Текущий уровень |
| `MaxLevel` | Level | Максимальный уровень |
| `Experience` | Experience | Текущий опыт |
| `ExperienceToNextLevel` | Experience | XP до следующего уровня |
| `ExperienceMultiplier` | Experience | Множитель получаемого XP |
| `IncomingExperience` | Meta | Входящий опыт |
| `Reputation` | Reputation | Репутация (0-100) |
| `ReputationMultiplier` | Reputation | Множитель репутации |
| `SoftCurrency` | Currency | Мягкая валюта |
| `HardCurrency` | Currency | Твёрдая валюта |
| `SkillPoints` | Skills | Очки навыков |
| `AttributePoints` | Skills | Очки атрибутов |
| `PrestigeLevel` | Prestige | Уровень престижа |
| `SeasonRank` | Season | Сезонный ранг |
| `SeasonExperience` | Season | Сезонный опыт |

**Механики:**
- Экспоненциальная кривая опыта: `BaseXP * (1.15 ^ (Level - 2))`
- Автоматический level up с выдачей очков
- +1 SkillPoint и +3 AttributePoints за уровень
- Событие `Event.Progression.LevelUp`

---

## 4. Интеграция с EventBus

### 4.1 События атрибутов

```cpp
// Все AttributeSets публикуют через ASC
USuspenseCoreAbilitySystemComponent* ASC = GetSuspenseCoreASC();
if (ASC)
{
    ASC->PublishAttributeChangeEvent(Attribute, OldValue, NewValue);
}
```

### 4.2 Критические события

| Событие | Tag | Когда публикуется |
|---------|-----|-------------------|
| Смерть | `Event.GAS.Health.Death` | Health <= 0 |
| Низкое HP | `Event.GAS.Health.Low` | Health <= 25% |
| Щит разрушен | `Event.GAS.Shield.Broken` | Shield = 0 (was > 0) |
| Level Up | `Event.Progression.LevelUp` | Level increased |

---

## 5. Использование в PlayerState

### 5.1 Конфигурация Blueprint

В `BP_SuspenseCorePlayerState`:

```
AttributeSetClass: USuspenseCoreAttributeSet (или кастомный)

AdditionalAttributeSets:
  - USuspenseCoreShieldAttributeSet
  - USuspenseCoreMovementAttributeSet
  - USuspenseCoreProgressionAttributeSet
```

### 5.2 Код инициализации

```cpp
void ASuspenseCorePlayerState::InitializeAbilitySystem()
{
    // Базовый AttributeSet
    if (AttributeSetClass)
    {
        AttributeSet = NewObject<USuspenseCoreAttributeSet>(this, AttributeSetClass);
        AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet);
    }

    // Дополнительные AttributeSets
    ShieldAttributes = NewObject<USuspenseCoreShieldAttributeSet>(this);
    AbilitySystemComponent->AddAttributeSetSubobject(ShieldAttributes);

    MovementAttributes = NewObject<USuspenseCoreMovementAttributeSet>(this);
    AbilitySystemComponent->AddAttributeSetSubobject(MovementAttributes);

    ProgressionAttributes = NewObject<USuspenseCoreProgressionAttributeSet>(this);
    AbilitySystemComponent->AddAttributeSetSubobject(ProgressionAttributes);
}
```

---

## 6. Файловая структура

```
GAS/
├── Public/
│   ├── SuspenseCore/
│   │   ├── Components/
│   │   │   └── SuspenseCoreAbilitySystemComponent.h    ✅
│   │   └── Attributes/
│   │       ├── SuspenseCoreAttributeSet.h              ✅
│   │       ├── SuspenseCoreShieldAttributeSet.h        ✅ NEW
│   │       ├── SuspenseCoreMovementAttributeSet.h      ✅ NEW
│   │       └── SuspenseCoreProgressionAttributeSet.h   ✅ NEW
│   │
│   ├── Components/          (Legacy)
│   ├── Attributes/          (Legacy)
│   ├── Abilities/           (Legacy)
│   └── Effects/             (Legacy)
│
└── Private/
    └── SuspenseCore/
        ├── Components/
        │   └── SuspenseCoreAbilitySystemComponent.cpp  ✅
        └── Attributes/
            ├── SuspenseCoreAttributeSet.cpp            ✅
            ├── SuspenseCoreShieldAttributeSet.cpp      ✅ NEW
            ├── SuspenseCoreMovementAttributeSet.cpp    ✅ NEW
            └── SuspenseCoreProgressionAttributeSet.cpp ✅ NEW
```

---

## 7. Чеклист выполнения

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    ЧЕКЛИСТ ЭТАПА 2: GAS                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ ABILITY SYSTEM COMPONENT                                                     │
│ ✅ SuspenseCoreAbilitySystemComponent.h                                     │
│ ✅ SuspenseCoreAbilitySystemComponent.cpp                                   │
│ ✅ EventBus интеграция                                                      │
│ ✅ Attribute change publishing                                              │
│                                                                              │
│ BASE ATTRIBUTE SET                                                           │
│ ✅ SuspenseCoreAttributeSet.h                                               │
│ ✅ SuspenseCoreAttributeSet.cpp                                             │
│ ✅ 11 базовых атрибутов                                                     │
│ ✅ Meta атрибуты (IncomingDamage, IncomingHealing)                          │
│ ✅ Репликация                                                               │
│                                                                              │
│ SHIELD ATTRIBUTE SET                                                         │
│ ✅ SuspenseCoreShieldAttributeSet.h                                         │
│ ✅ SuspenseCoreShieldAttributeSet.cpp                                       │
│ ✅ 10 атрибутов щита                                                        │
│ ✅ Shield break механика                                                    │
│ ✅ Regen delay механика                                                     │
│                                                                              │
│ MOVEMENT ATTRIBUTE SET                                                       │
│ ✅ SuspenseCoreMovementAttributeSet.h                                       │
│ ✅ SuspenseCoreMovementAttributeSet.cpp                                     │
│ ✅ 18 атрибутов движения                                                    │
│ ✅ Weight/Encumbrance система                                               │
│ ✅ Интеграция с CharacterMovement                                           │
│                                                                              │
│ PROGRESSION ATTRIBUTE SET                                                    │
│ ✅ SuspenseCoreProgressionAttributeSet.h                                    │
│ ✅ SuspenseCoreProgressionAttributeSet.cpp                                  │
│ ✅ 14 атрибутов прогрессии                                                  │
│ ✅ Level up система                                                         │
│ ✅ Currency система                                                         │
│ ✅ Season/Prestige система                                                  │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

*Документ обновлён: 2025-11-29*
*Этап: 2 из 7*
*Статус: ✅ Core выполнен*
