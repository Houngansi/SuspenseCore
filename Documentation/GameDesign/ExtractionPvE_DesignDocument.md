# Extraction PvE: "Зона Отчуждения"

## Концепция

Игрок — сталкер-выживальщик в постсоветском дистопийном мире. Действие происходит в заброшенных промышленных районах, разрушенных городах бывшего СССР после неизвестной катастрофы. Задача: проникнуть в опасные зоны, собрать ценные ресурсы и выбраться живым. Каждый рейд — баланс между жадностью и выживанием.

**Сеттинг**: Пост-советская антиутопия. Разрушенные заводы, заброшенные НИИ, мёртвые колхозы, полуразваленные хрущёвки. Атмосфера Сталкера/Тарков с элементами советской эстетики.

---

## Core Loop (Основной Цикл)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           УБЕЖИЩЕ (Hideout)                             │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐         │
│  │     Схрон       │  │    Верстак      │  │    Торговец     │         │
│  │    (Stash)      │  │   (Workbench)   │  │    (Trader)     │         │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘         │
└───────────────────────────────┬─────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         DEPLOYMENT (Выход на рейд)                      │
│  - Выбор локации (риск/награда)                                         │
│  - Подготовка снаряжения                                                │
│  - Выбор точки входа                                                    │
└───────────────────────────────┬─────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                            RAID (Рейд)                                  │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │ EXPLORATION ──► COMBAT ──► LOOT ──► EXTRACTION                   │  │
│  │      │            │          │           │                        │  │
│  │      ▼            ▼          ▼           ▼                        │  │
│  │  Skill XP    Weapon XP   Resources   Survival Bonus              │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│  THREATS:                           REWARDS:                            │
│  - Мародёры и бандиты               - Редкое оружие                    │
│  - Мутанты                          - Модификации                       │
│  - Аномалии                         - Медикаменты                       │
│  - Радиация                         - Материалы для крафта              │
└───────────────────────────────┬─────────────────────────────────────────┘
                                │
                    ┌───────────┴───────────┐
                    ▼                       ▼
            ┌─────────────┐         ┌─────────────┐
            │  EXTRACTED  │         │    DEAD     │
            │  (Выжил)    │         │  (Погиб)    │
            ├─────────────┤         ├─────────────┤
            │ Keep Loot   │         │ Lose Loot   │
            │ Keep XP     │         │ Keep XP*    │
            │ Gear Damage │         │ Lose Gear   │
            └─────────────┘         └─────────────┘
                    │                       │
                    └───────────┬───────────┘
                                ▼
                        [Return to Hideout]
```

---

## Equipment System (Система экипировки)

### Слоты снаряжения (Tarkov-style)

Система использует 17 слотов экипировки, определённых в `EEquipmentSlotType`:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    EQUIPMENT SLOTS LAYOUT                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────┐    ┌───────────┐ ┌────┐                            │
│  │ PRIMARY WEAPON  │    │ HEADWEAR  │ │EAR │   ← Шлем + Наушники        │
│  │   (5x2 cells)   │    │  (2x2)    │ │1x1 │                            │
│  │ AR/DMR/SR/LMG   │    └───────────┘ └────┘                            │
│  └─────────────────┘    ┌────┐ ┌────┐                                   │
│                         │EYE │ │FACE│       ← Очки + Балаклава          │
│  ┌──────────────┐       │1x1 │ │1x1 │                                   │
│  │SECONDARY WPN │       └────┘ └────┘                                   │
│  │  (4x2 cells) │                                                       │
│  │ SMG/PDW/SG   │       ┌──────────────┐ ┌─────────┐                    │
│  └──────────────┘       │  BODY ARMOR  │ │TACT.RIG │  ← Броня + Разгруз │
│                         │    (3x3)     │ │  (2x3)  │                    │
│  ┌────────┐ ┌────┐      └──────────────┘ └─────────┘                    │
│  │HOLSTER │ │SCAB│                                                      │
│  │ (2x2)  │ │1x2 │      ┌──────────────┐ ┌─────────┐ ┌────┐             │
│  │Pistol  │ │Knf │      │   BACKPACK   │ │ SECURE  │ │ARM │             │
│  └────────┘ └────┘      │    (3x3)     │ │  (2x2)  │ │BAND│             │
│                         └──────────────┘ └─────────┘ └────┘             │
│                                                                          │
│  ┌────┐ ┌────┐ ┌────┐ ┌────┐                                            │
│  │ Q1 │ │ Q2 │ │ Q3 │ │ Q4 │  ← Quick Slots (Consumable/Medical/Ammo)   │
│  └────┘ └────┘ └────┘ └────┘                                            │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### Типы слотов и допустимые предметы

| Слот | Размер | Допустимые типы |
|------|--------|-----------------|
| **PrimaryWeapon** | 5x2 | AR, DMR, SR, Shotgun, LMG |
| **SecondaryWeapon** | 4x2 | SMG, PDW, Shotgun |
| **Holster** | 2x2 | Pistol, Revolver |
| **Scabbard** | 1x2 | Melee/Knife |
| **Headwear** | 2x2 | Helmet, Headwear |
| **Earpiece** | 1x1 | Earpiece (тактические наушники) |
| **Eyewear** | 1x1 | Eyewear (очки, визоры) |
| **FaceCover** | 1x1 | FaceCover (балаклава, респиратор) |
| **BodyArmor** | 3x3 | BodyArmor |
| **TacticalRig** | 2x3 | TacticalRig |
| **Backpack** | 3x3 | Backpack |
| **SecureContainer** | 2x2 | SecureContainer (сохраняется при смерти) |
| **QuickSlot 1-4** | 1x1 | Consumable, Medical, Throwable, Ammo |
| **Armband** | 1x1 | Armband (фракционная метка) |

---

## Weapon System (Система оружия)

### Доступное оружие

На основе `SuspenseCoreWeaponAttributes.json`:

| ID | Название | Тип | Калибр | Урон | ROF | Магазин |
|----|----------|-----|--------|------|-----|---------|
| `Weapon_AK74M` | **АК-74М** | Assault Rifle | 5.45x39mm | 42 | 650 | 30 |
| `Weapon_AK103` | **АК-103** | Assault Rifle | 7.62x39mm | 55 | 600 | 30 |
| `Weapon_SVD` | **СВД** | DMR | 7.62x54mmR | 86 | 120 | 10 |
| `Weapon_MP5SD` | **MP5SD** | SMG | 9x19mm | 35 | 800 | 30 |
| `Weapon_MP7A2` | **MP7A2** | PDW | 4.6x30mm | 30 | 950 | 40 |
| `Weapon_Glock17` | **Glock 17** | Pistol | 9x19mm | 35 | 450 | 17 |
| `Weapon_M9A3` | **Beretta M9A3** | Pistol | 9x19mm | 36 | 400 | 17 |
| `Weapon_M9Bayonet` | **Штык-нож M9** | Melee | - | 45 | 80 | - |

### Характеристики оружия

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    WEAPON ATTRIBUTES                                      │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│  BaseDamage         - Базовый урон                                       │
│  RateOfFire         - Скорострельность (выстр/мин)                       │
│  EffectiveRange     - Эффективная дальность (м)                          │
│  MaxRange           - Максимальная дальность (м)                         │
│  MagazineSize       - Ёмкость магазина                                   │
│  TacticalReloadTime - Перезарядка с патроном в патроннике (сек)          │
│  FullReloadTime     - Полная перезарядка (сек)                           │
│  MOA                - Точность (меньше = лучше)                          │
│  HipFireSpread      - Разброс при стрельбе от бедра                      │
│  AimSpread          - Разброс при прицеливании                           │
│  VerticalRecoil     - Вертикальная отдача                                │
│  HorizontalRecoil   - Горизонтальная отдача                              │
│  Durability         - Текущая прочность (0-100)                          │
│  MaxDurability      - Максимальная прочность                             │
│  MisfireChance      - Шанс осечки                                        │
│  JamChance          - Шанс заклинивания                                  │
│  Ergonomics         - Эргономика (влияет на ADS speed)                   │
│  AimDownSightTime   - Время прицеливания (сек)                           │
│  WeaponWeight       - Вес оружия (кг)                                    │
│                                                                           │
└──────────────────────────────────────────────────────────────────────────┘
```

### Режимы огня (FireModes)

| Оружие | Режимы огня |
|--------|-------------|
| АК-74М | Single, Auto |
| АК-103 | Single, Auto |
| СВД | Single |
| MP5SD | Single, Burst, Auto |
| MP7A2 | Single, Auto |
| Glock 17 | Single |
| M9A3 | Single |

### Weapon Degradation (Износ оружия)

```
┌──────────────────────────────────────────────────────────────────┐
│ WEAPON DURABILITY: 100% ──────────────────────────────────► 0%   │
├──────────────────────────────────────────────────────────────────┤
│ 100-75%  │ Full performance                                      │
│  75-50%  │ -10% accuracy, occasional jam (5%)                    │
│  50-25%  │ -20% accuracy, frequent jam (15%), -10% damage        │
│  25-10%  │ -35% accuracy, constant jam (30%), -25% damage        │
│  10-0%   │ BROKEN - Cannot fire, must repair                     │
└──────────────────────────────────────────────────────────────────┘
```

---

## Ammo System (Система патронов)

### Калибр 5.45x39mm (для АК-74М)

На основе `SuspenseCoreAmmoAttributes.json`:

| ID | Название | Урон | Пробитие | Фрагментация |
|----|----------|------|----------|--------------|
| `Ammo_545x39_PS` | 5.45x39 ПС гс | 42 | 25 | 40% |
| `Ammo_545x39_BP` | 5.45x39 БП | 37 | 45 | 17% |
| `Ammo_545x39_BS` | 5.45x39 БС | 40 | 57 | 15% |
| `Ammo_545x39_T` | 5.45x39 Т (трассер) | 40 | 20 | 25% |
| `Ammo_545x39_HP` | 5.45x39 HP (экспанс.) | 68 | 8 | 85% |

### Калибр 9x19mm (для Glock/M9A3/MP5SD)

| ID | Название | Урон | Пробитие | Фрагментация |
|----|----------|------|----------|--------------|
| `Ammo_9x19_PST` | 9x19 ПСТ гж | 50 | 18 | 15% |
| `Ammo_9x19_AP` | 9x19 AP 6.3 | 45 | 30 | 5% |
| `Ammo_9x19_RIP` | 9x19 RIP | 102 | 2 | 100% |
| `Ammo_9x19_Luger` | 9x19 Luger CCI | 55 | 10 | 20% |

### Атрибуты патронов

```cpp
// Каждый тип патрона влияет на:
BaseDamage           // Базовый урон
ArmorPenetration     // Пробитие брони (0-100)
StoppingPower        // Останавливающая сила
FragmentationChance  // Шанс фрагментации (доп. урон)
MuzzleVelocity       // Начальная скорость пули (м/с)
DragCoefficient      // Коэффициент сопротивления воздуха
EffectiveRange       // Эффективная дальность
AccuracyModifier     // Модификатор точности
RecoilModifier       // Модификатор отдачи
RicochetChance       // Шанс рикошета
TracerVisibility     // Видимость трассера (0.0-1.0)
IncendiaryDamage     // Зажигательный урон
WeaponDegradationRate // Скорость износа оружия
MisfireChance        // Шанс осечки
```

---

## Skill Progression System (GAS-Based)

### Архитектура навыков

```cpp
// Категории навыков (AttributeSet)
UPROPERTY() float CombatSkill;      // Боевые навыки
UPROPERTY() float SurvivalSkill;    // Выживание
UPROPERTY() float TechnicalSkill;   // Техника
UPROPERTY() float MobilitySkill;    // Мобильность
```

### Прокачка через использование

| Действие | Навык | XP за действие |
|----------|-------|----------------|
| Убийство врага | Combat | 10-50 (по типу) |
| Попадание в голову | Combat | +15 бонус |
| Использование аптечки | Survival | 5 |
| Найден тайник | Survival | 20 |
| Взлом замка | Technical | 15 |
| Ремонт оружия | Technical | 10 |
| Успешный спринт-побег | Mobility | 8 |
| Прыжок через препятствие | Mobility | 3 |

### Уровни навыков и бонусы

```
┌────────────────────────────────────────────────────────────────┐
│ COMBAT SKILL                                                    │
├────────────────────────────────────────────────────────────────┤
│ Lvl 1-10:   Base damage                                        │
│ Lvl 11-20:  +5% damage, -3% recoil                            │
│ Lvl 21-30:  +10% damage, -6% recoil, faster reload            │
│ Lvl 31-40:  +15% damage, -10% recoil, +10% crit chance        │
│ Lvl 41-50:  +20% damage, -15% recoil, +15% crit, elite perks  │
└────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────┐
│ SURVIVAL SKILL                                                  │
├────────────────────────────────────────────────────────────────┤
│ Lvl 1-10:   Base healing                                       │
│ Lvl 11-20:  +15% healing efficiency                            │
│ Lvl 21-30:  Slower stamina drain, radiation resistance         │
│ Lvl 31-40:  Detect nearby loot, bonus HP                       │
│ Lvl 41-50:  Second chance (once per raid)                      │
└────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────┐
│ TECHNICAL SKILL                                                 │
├────────────────────────────────────────────────────────────────┤
│ Lvl 1-10:   Basic repairs                                      │
│ Lvl 11-20:  -20% repair cost, craft basic items               │
│ Lvl 21-30:  Access locked containers, weapon mods             │
│ Lvl 31-40:  Hack terminals, craft advanced items              │
│ Lvl 41-50:  Disable security systems, craft rare items        │
└────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────┐
│ MOBILITY SKILL                                                  │
├────────────────────────────────────────────────────────────────┤
│ Lvl 1-10:   Base movement speed                                │
│ Lvl 11-20:  +5% sprint speed, quieter footsteps               │
│ Lvl 21-30:  +10% sprint, -15% stamina cost                    │
│ Lvl 31-40:  Slide ability, longer sprint duration             │
│ Lvl 41-50:  Wall climb, silent movement                       │
└────────────────────────────────────────────────────────────────┘
```

---

## Item Degradation (Анти-абуз система)

### Концепция

Все снаряжение изнашивается, предотвращая бесконечный гринд без риска:

```
┌────────────────────────────────────────────────────────────────┐
│                    EQUIPMENT LIFECYCLE                          │
├────────────────────────────────────────────────────────────────┤
│                                                                 │
│  [Purchase/Find] ──► [Use in Raids] ──► [Degrade] ──► [Repair] │
│        │                   │                │            │      │
│        │                   │                │            │      │
│     Credits              XP Gain         Durability    Credits  │
│                                          Loss                   │
│                                                                 │
│  BALANCE: XP gained < Cost to maintain equipment               │
│           Forces player to take risks for profit               │
│                                                                 │
└────────────────────────────────────────────────────────────────┘
```

### Degradation Types

| Item Type | Degrades By | Rate |
|-----------|-------------|------|
| Weapons | Shots fired | 0.05-0.15%/shot |
| Armor | Damage taken | 1% per 10 HP damage |
| Backpack | Weight carried | 0.1%/kg/minute |
| Tools | Uses | 5-10%/use |
| Medicine | Time (shelf life) | Expires after 5 raids |

### Repair Economy

```
Repair Cost Formula:
Cost = (MaxDurability - CurrentDurability) * BaseCost * QualityMultiplier

Quality Multipliers:
- Scrap:    0.5x cost, max repair to 60%
- Standard: 1.0x cost, max repair to 85%
- Premium:  2.0x cost, max repair to 100%
```

---

## Replayability Systems

### 1. Procedural Raid Generation

```
┌────────────────────────────────────────────────────────────────┐
│                    RAID GENERATION                              │
├────────────────────────────────────────────────────────────────┤
│                                                                 │
│  BASE MAP (Static Layout)                                       │
│       │                                                         │
│       ├──► Enemy Spawns (Randomized density/types)             │
│       │    └── Patrol routes change each raid                  │
│       │                                                         │
│       ├──► Loot Locations (70% fixed, 30% random)              │
│       │    └── Quality varies by "raid difficulty"             │
│       │                                                         │
│       ├──► Extraction Points (2-4 options, 1-2 active)         │
│       │    └── Active points revealed mid-raid                 │
│       │                                                         │
│       └──► Events (Random encounters)                          │
│            ├── Бандитский рейд в процессе                      │
│            ├── Военный патруль                                 │
│            ├── Тайник сталкеров                                │
│            └── Аномальная зона                                 │
│                                                                 │
└────────────────────────────────────────────────────────────────┘
```

### 2. Dynamic Difficulty

```cpp
// Difficulty scales with player progress
struct FRaidDifficulty
{
    float EnemyHealthMultiplier;    // 1.0 - 2.0
    float EnemyDamageMultiplier;    // 1.0 - 1.5
    float LootQualityMultiplier;    // 1.0 - 2.5
    int32 EliteEnemyChance;         // 0-30%
    int32 AmbushChance;             // 5-25%
};

// Based on:
// - Player skill levels
// - Equipment value brought
// - Previous raid success rate
// - Selected zone difficulty
```

### 3. Faction Reputation

```
┌────────────────────────────────────────────────────────────────┐
│                    FACTIONS                                     │
├────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ВОЛЬНЫЕ СТАЛКЕРЫ                ТОРГОВЦЫ                      │
│  ├── Reputation: -100 to +100    ├── Reputation: -100 to +100  │
│  ├── +Rep: Kill bandits          ├── +Rep: Sell items, complete│
│  ├── -Rep: Steal supplies        │        deliveries           │
│  └── Rewards:                    ├── -Rep: Steal, attack       │
│      ├── Safe houses             └── Rewards:                  │
│      ├── Intel on raids              ├── Better prices         │
│      └── Unique weapons              ├── Rare items            │
│                                      └── Special orders        │
│                                                                 │
│  MILITARY (Военные)              BANDITS (Бандиты)             │
│  ├── Reputation: -100 to +100    ├── Always hostile            │
│  ├── +Rep: Complete contracts    ├── Can be bribed             │
│  ├── -Rep: Trespass, kill       └── Control certain zones     │
│  └── Rewards: Military gear                                    │
│                                                                 │
└────────────────────────────────────────────────────────────────┘
```

---

## Zone Progression (Карта мира)

```
┌────────────────────────────────────────────────────────────────┐
│                      WORLD MAP                                  │
├────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────┐     ┌─────────────┐     ┌─────────────┐       │
│  │  ОКРАИНЫ    │────│  ПРОМЗОНА   │────│    ЗАВОД    │       │
│  │   Tier 1    │    │   Tier 2    │    │   Tier 3    │       │
│  │  Low risk   │    │   Medium    │    │    High     │       │
│  └─────────────┘     └─────────────┘     └─────────────┘       │
│       │               │               │                         │
│       │               │               │                         │
│  ┌─────────────┐     ┌─────────────┐     ┌─────────────┐       │
│  │   КОЛХОЗ    │────│    НИИ      │────│   БУНКЕР    │       │
│  │   Tier 2    │    │   Tier 4    │    │   Tier 5    │       │
│  │   Medium    │    │   V.High    │    │   Extreme   │       │
│  └─────────────┘     └─────────────┘     └─────────────┘       │
│                                                                 │
│  Unlock Requirements:                                          │
│  ├── Tier 2: Complete 5 Tier 1 raids                          │
│  ├── Tier 3: Combat Skill 15+                                 │
│  ├── Tier 4: All skills 20+, Stalker rep 50+                  │
│  └── Tier 5: Complete "Секретный бункер" quest chain          │
│                                                                 │
└────────────────────────────────────────────────────────────────┘
```

---

## Technical Implementation (GAS Integration)

### New Native Tags Needed

```cpp
namespace SuspenseCoreTags
{
    // Skills
    namespace Skill
    {
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combat);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Survival);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Technical);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mobility);
    }

    // Weapon States
    namespace Weapon
    {
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Jammed);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Broken);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Degraded);
    }

    // Raid States
    namespace Raid
    {
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(InProgress);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Extracted);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Failed);
    }

    // Events
    namespace Event::Skill
    {
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(XPGained);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(LevelUp);
    }
}
```

### New Attributes

```cpp
// SuspenseCoreSkillAttributeSet.h
UCLASS()
class USuspenseCoreSkillAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    // Combat
    UPROPERTY(BlueprintReadOnly)
    FGameplayAttributeData CombatXP;
    UPROPERTY(BlueprintReadOnly)
    FGameplayAttributeData CombatLevel;

    // Survival
    UPROPERTY(BlueprintReadOnly)
    FGameplayAttributeData SurvivalXP;
    UPROPERTY(BlueprintReadOnly)
    FGameplayAttributeData SurvivalLevel;

    // Technical
    UPROPERTY(BlueprintReadOnly)
    FGameplayAttributeData TechnicalXP;
    UPROPERTY(BlueprintReadOnly)
    FGameplayAttributeData TechnicalLevel;

    // Mobility
    UPROPERTY(BlueprintReadOnly)
    FGameplayAttributeData MobilityXP;
    UPROPERTY(BlueprintReadOnly)
    FGameplayAttributeData MobilityLevel;
};
```

### New GameplayEffects

```cpp
// GE_AddSkillXP - SetByCaller для XP amount
// GE_WeaponDegradation - Periodic, reduces durability
// GE_WeaponJam - Instant, applies Weapon.Jammed tag
// GE_SkillBonus_Combat - Passive, applies combat bonuses based on level
```

---

## MVP Scope (Demo Version)

### Phase 1: Core Loop
- [ ] Single zone (Окраины)
- [ ] 3 enemy types (Мародёр, Мутант, Бандит)
- [ ] Basic loot system
- [ ] Single extraction point
- [ ] Hideout with stash only

### Phase 2: Progression
- [ ] Combat + Mobility skills
- [ ] Weapon durability (using existing `MisfireChance`, `JamChance`)
- [ ] Basic repairs
- [ ] 4 weapons (АК-74М, Glock 17, MP5SD, Штык-нож)

### Phase 3: Replayability
- [ ] Second zone (Промзона)
- [ ] Procedural enemy spawns
- [ ] Daily challenges
- [ ] Trader NPC

---

*Document Version: 2.0*
*Created: 2025-12-27*
*Updated: 2025-12-27 - Aligned with actual project data*
*Architecture: SuspenseCore GAS + EventBus*
*Data Sources: SuspenseCoreWeaponAttributes.json, SuspenseCoreAmmoAttributes.json, SuspenseCoreLoadoutSettings.h*
