# Extraction PvE: "City 17 Salvage"

## Концепция

Игрок — сталкер-мусорщик в оккупированном мире Half-Life 2. Задача: проникнуть в опасные зоны, собрать ценные ресурсы и выбраться живым. Каждый рейд — баланс между жадностью и выживанием.

---

## Core Loop (Основной Цикл)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           HIDEOUT (Убежище)                             │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐    │
│  │   Stash     │  │  Workbench  │  │   Trader    │  │   Mission   │    │
│  │  (Схрон)    │  │  (Верстак)  │  │ (Торговец)  │  │   Board     │    │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘    │
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
│  - Combine Patrols                  - Alien Technology                  │
│  - Headcrabs/Zombies               - Combine Weapons                    │
│  - Environmental Hazards            - Medical Supplies                  │
│  - Equipment Degradation            - Upgrade Materials                 │
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
| Успешный спринт побег | Mobility | 8 |
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
│ Lvl 31-40:  Hack Combine terminals, craft advanced items      │
│ Lvl 41-50:  Disable turrets, craft alien tech                 │
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

### GAS Implementation

```cpp
// В SuspenseCoreAttributeSet добавить:
UPROPERTY(BlueprintReadOnly, Category = "Skills")
FGameplayAttributeData CombatSkillXP;

UPROPERTY(BlueprintReadOnly, Category = "Skills")
FGameplayAttributeData CombatSkillLevel;

// GameplayEffect для добавления XP
// GE_AddCombatXP - SetByCaller(Data.Skill.XPAmount)

// GameplayCue для level up
// GC_SkillLevelUp - Visual/Audio feedback
```

---

## Weapon System

### Weapon Degradation (Износ оружия)

```
┌──────────────────────────────────────────────────────────────┐
│ WEAPON DURABILITY: 100% ──────────────────────────────► 0%   │
├──────────────────────────────────────────────────────────────┤
│ 100-75%  │ Full performance                                  │
│  75-50%  │ -10% accuracy, occasional jam (5%)               │
│  50-25%  │ -20% accuracy, frequent jam (15%), -10% damage   │
│  25-10%  │ -35% accuracy, constant jam (30%), -25% damage   │
│  10-0%   │ BROKEN - Cannot fire, must repair                │
└──────────────────────────────────────────────────────────────┘
```

### Degradation Rate

| Оружие | Durability Loss/Shot | Shots to 0% |
|--------|---------------------|-------------|
| Pistol | 0.05% | 2000 |
| SMG | 0.08% | 1250 |
| Shotgun | 0.15% | 666 |
| Rifle | 0.10% | 1000 |
| Crossbow | 0.03% | 3333 |

### Weapon Upgrades

```
┌─────────────────────────────────────────────────────────────────┐
│                     WEAPON UPGRADE TREE                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  BASE WEAPON                                                     │
│       │                                                          │
│       ├──► [Barrel] ──► Accuracy +15% / Damage +10% / Silencer  │
│       │                                                          │
│       ├──► [Stock] ──► Recoil -20% / Stability +15%             │
│       │                                                          │
│       ├──► [Magazine] ──► Capacity +50% / Reload -25%           │
│       │                                                          │
│       ├──► [Sights] ──► Zoom 2x / Red Dot / Thermal             │
│       │                                                          │
│       └──► [Frame] ──► Durability +30% / Weight -20%            │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Weapon XP (Mastery)

Каждое оружие имеет свой уровень мастерства:

```
Weapon Mastery Levels:
├── Lvl 1 (0 XP):     Base stats
├── Lvl 2 (500 XP):   -5% recoil
├── Lvl 3 (1500 XP):  +5% damage
├── Lvl 4 (3500 XP):  -10% reload time
├── Lvl 5 (7000 XP):  Unlock special ammo types
└── Lvl 6 (15000 XP): Weapon skin + title
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
│            ├── Combine raid in progress                        │
│            ├── Strider patrol                                  │
│            ├── Resistance cache                                │
│            └── Anomaly zone                                    │
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
│  RESISTANCE                    TRADERS                          │
│  ├── Reputation: -100 to +100  ├── Reputation: -100 to +100    │
│  ├── +Rep: Kill Combine        ├── +Rep: Sell items, complete  │
│  ├── -Rep: Steal supplies      │        deliveries             │
│  └── Rewards:                  ├── -Rep: Steal, attack traders │
│      ├── Safe houses           └── Rewards:                    │
│      ├── Intel on raids            ├── Better prices           │
│      └── Unique weapons            ├── Rare items              │
│                                    └── Special orders          │
│                                                                 │
│  STALKERS (Other Players/NPCs)                                 │
│  ├── Reputation: Personal per stalker                          │
│  ├── +Rep: Trade fairly, help in combat                        │
│  ├── -Rep: Betray, steal                                       │
│  └── Rewards: Alliance, shared intel, backup                   │
│                                                                 │
└────────────────────────────────────────────────────────────────┘
```

### 4. Seasonal Content

```
Season Structure (8-12 weeks):
├── Week 1-2:  New zone unlocks
├── Week 3-4:  Special event (Combine crackdown)
├── Week 5-6:  New weapon/upgrade tier
├── Week 7-8:  Boss event
├── Week 9-10: Double XP / Rare loot event
└── Week 11-12: Season finale + rewards

Season Pass Rewards:
├── Free Track: Cosmetics, resources
└── Premium Track: Unique weapons, stash space, cosmetics
```

### 5. Mastery Challenges

```
┌────────────────────────────────────────────────────────────────┐
│                    DAILY/WEEKLY CHALLENGES                      │
├────────────────────────────────────────────────────────────────┤
│                                                                 │
│  DAILY (Reset every 24h):                                      │
│  ├── Kill 10 Headcrabs                    [50 Credits]         │
│  ├── Extract with 5kg+ loot               [75 Credits]         │
│  └── Use pistol only for entire raid      [100 Credits]        │
│                                                                 │
│  WEEKLY (Reset every 7 days):                                  │
│  ├── Complete 10 raids                    [500 Credits]        │
│  ├── Reach extraction 5 times in a row    [Rare Item]         │
│  └── Kill Elite Combine soldier           [Weapon Skin]        │
│                                                                 │
│  MASTERY (Permanent):                                          │
│  ├── Kill 1000 enemies                    [Title: Veteran]     │
│  ├── Extract with 100kg total loot        [Backpack upgrade]  │
│  └── Max out Combat skill                 [Unique Ability]     │
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
│  ┌─────────┐     ┌─────────┐     ┌─────────┐                   │
│  │ OUTSKIRTS│────│ CANALS  │────│INDUSTRIAL│                   │
│  │ Tier 1   │    │ Tier 2  │    │ Tier 3   │                   │
│  │ Low risk │    │ Medium  │    │ High     │                   │
│  └─────────┘     └─────────┘     └─────────┘                   │
│       │               │               │                         │
│       │               │               │                         │
│  ┌─────────┐     ┌─────────┐     ┌─────────┐                   │
│  │  PLAZA  │────│ NEXUS   │────│  CITADEL │                   │
│  │ Tier 2  │    │ Tier 4  │    │ Tier 5   │                   │
│  │ Medium  │    │ V.High  │    │ Extreme  │                   │
│  └─────────┘     └─────────┘     └─────────┘                   │
│                                                                 │
│  Unlock Requirements:                                          │
│  ├── Tier 2: Complete 5 Tier 1 raids                          │
│  ├── Tier 3: Combat Skill 15+                                 │
│  ├── Tier 4: All skills 20+, Resistance rep 50+              │
│  └── Tier 5: Complete "Citadel Key" quest chain              │
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
- [ ] Single zone (Outskirts)
- [ ] 3 enemy types (Headcrab, Zombie, Combine Soldier)
- [ ] Basic loot system
- [ ] Single extraction point
- [ ] Hideout with stash only

### Phase 2: Progression
- [ ] Combat + Mobility skills
- [ ] Weapon durability
- [ ] Basic repairs
- [ ] 3 weapons (Pistol, SMG, Shotgun)

### Phase 3: Replayability
- [ ] Second zone (Canals)
- [ ] Procedural enemy spawns
- [ ] Daily challenges
- [ ] Trader NPC

---

*Document Version: 1.0*
*Created: 2025-12-27*
*Architecture: SuspenseCore GAS + EventBus*
