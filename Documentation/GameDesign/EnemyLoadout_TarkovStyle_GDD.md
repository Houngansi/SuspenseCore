# Enemy Loadout System - Tarkov Style
# Game Design Document

> **Version:** 1.0
> **Status:** APPROVED FOR IMPLEMENTATION
> **Author:** Claude AI
> **Date:** 2026-01-27

---

## 1. Overview

### 1.1 Концепция
Система экипировки врагов в стиле Escape from Tarkov:
- Враг имеет **реальный инвентарь** с реальными предметами
- При обыске трупа игрок видит **фактическую экипировку** врага
- Патроны имеют **реальное количество** (не рандом из лут-таблицы)
- Магазины заряжены **конкретными патронами**

### 1.2 Отличия от классической системы
| Классический подход | Tarkov-стиль |
|---------------------|--------------|
| Loot Table → Random Items | Реальный инвентарь врага |
| Random ammo count | Точное количество патронов в магазинах |
| Abstract "weapon drop" | Конкретное оружие с модификациями |
| RNG-based rewards | What You See Is What You Get |

---

## 2. Архитектура SSOT

### 2.1 Иерархия данных
```
Project Settings (SuspenseCoreSettings)
    └── EnemyPresetsDataTable (DT_EnemyPresets)
            └── FSuspenseCoreEnemyPresetRow
                    ├── Attributes (HP, Speed, etc.)
                    ├── LoadoutPreset (FEnemyLoadoutPreset)
                    │       ├── WeaponLoadout[]
                    │       ├── ArmorLoadout
                    │       ├── RigLoadout
                    │       ├── BackpackContents[]
                    │       └── PocketContents[]
                    └── GAS Config (Abilities, Effects)
```

### 2.2 Data Flow
```
1. Spawn Enemy
       ↓
2. Load Preset from DataTable (by PresetRowName)
       ↓
3. Initialize Loadout (create actual items)
       ↓
4. Populate Inventory Component
       ↓
5. Equip Weapons/Armor
       ↓
6. On Death → Corpse has REAL inventory for looting
```

---

## 3. Структуры данных

### 3.1 FEnemyWeaponLoadout - Конфигурация оружия
```cpp
USTRUCT(BlueprintType)
struct FEnemyWeaponLoadout
{
    /** Item ID оружия (из DT_Items) */
    FName WeaponItemID;

    /** Модификации оружия (опционально) */
    TArray<FName> AttachmentItemIDs;

    /** Конфигурация магазинов */
    TArray<FEnemyMagazineConfig> Magazines;

    /** Запасные патроны (россыпью) */
    FEnemyAmmoConfig LooseAmmo;

    /** Слот экипировки */
    EEquipmentSlotType EquipSlot = EEquipmentSlotType::PrimaryWeapon;
};
```

### 3.2 FEnemyMagazineConfig - Конфигурация магазина
```cpp
USTRUCT(BlueprintType)
struct FEnemyMagazineConfig
{
    /** Item ID магазина */
    FName MagazineItemID;

    /** Item ID патронов */
    FName AmmoItemID;

    /** Количество патронов в магазине */
    int32 LoadedAmmoCount = 30;

    /** Где хранится магазин (в оружии / риге / карманах) */
    EEnemyMagazineLocation Location = EEnemyMagazineLocation::InWeapon;
};
```

### 3.3 FEnemyArmorLoadout - Конфигурация брони
```cpp
USTRUCT(BlueprintType)
struct FEnemyArmorLoadout
{
    /** Item ID бронежилета */
    FName BodyArmorItemID;

    /** Item ID шлема */
    FName HelmetItemID;

    /** Item ID разгрузки/рига */
    FName RigItemID;

    /** Текущая прочность брони (0-1, 1 = новая) */
    float ArmorDurabilityPercent = 1.0f;

    /** Текущая прочность шлема */
    float HelmetDurabilityPercent = 1.0f;
};
```

### 3.4 FEnemyInventoryItem - Предмет в инвентаре
```cpp
USTRUCT(BlueprintType)
struct FEnemyInventoryItem
{
    /** Item ID из базы предметов */
    FName ItemID;

    /** Количество */
    int32 Quantity = 1;

    /** Шанс появления (0-1) */
    float SpawnChance = 1.0f;

    /** Min-Max количество (если рандом) */
    int32 MinQuantity = 1;
    int32 MaxQuantity = 1;
};
```

### 3.5 FEnemyLoadoutPreset - Полный пресет экипировки
```cpp
USTRUCT(BlueprintType)
struct FEnemyLoadoutPreset
{
    /** Конфигурация оружия */
    TArray<FEnemyWeaponLoadout> Weapons;

    /** Конфигурация брони */
    FEnemyArmorLoadout Armor;

    /** Содержимое рига (магазины, гранаты, аптечки) */
    TArray<FEnemyInventoryItem> RigContents;

    /** Содержимое рюкзака */
    TArray<FEnemyInventoryItem> BackpackContents;

    /** Содержимое карманов */
    TArray<FEnemyInventoryItem> PocketContents;

    /** Деньги (рубли/доллары/евро) */
    TArray<FEnemyInventoryItem> Currency;

    /** Квестовые предметы (отдельная категория) */
    TArray<FEnemyInventoryItem> QuestItems;
};
```

---

## 4. Примеры пресетов

### 4.1 Scav_Grunt (Базовый скавенджер)
```json
{
  "PresetID": "Scav_Grunt",
  "LoadoutPreset": {
    "Weapons": [
      {
        "WeaponItemID": "Weapon_PM_Makarov",
        "Magazines": [
          {
            "MagazineItemID": "Mag_PM_8rnd",
            "AmmoItemID": "Ammo_9x18_PM_Pst",
            "LoadedAmmoCount": 8,
            "Location": "InWeapon"
          },
          {
            "MagazineItemID": "Mag_PM_8rnd",
            "AmmoItemID": "Ammo_9x18_PM_Pst",
            "LoadedAmmoCount": 8,
            "Location": "InRig"
          }
        ],
        "LooseAmmo": {
          "AmmoItemID": "Ammo_9x18_PM_Pst",
          "MinCount": 8,
          "MaxCount": 24
        }
      }
    ],
    "Armor": {
      "BodyArmorItemID": "",
      "HelmetItemID": "",
      "RigItemID": "Rig_Scav_Vest"
    },
    "RigContents": [
      { "ItemID": "Consumable_Bandage", "Quantity": 1, "SpawnChance": 0.7 },
      { "ItemID": "Food_Crackers", "Quantity": 1, "SpawnChance": 0.3 }
    ],
    "PocketContents": [
      { "ItemID": "Misc_Cigarettes", "Quantity": 1, "SpawnChance": 0.5 }
    ],
    "Currency": [
      { "ItemID": "Currency_Roubles", "MinQuantity": 500, "MaxQuantity": 5000, "SpawnChance": 0.8 }
    ]
  }
}
```

### 4.2 Raider_Elite (Элитный рейдер)
```json
{
  "PresetID": "Raider_Elite",
  "LoadoutPreset": {
    "Weapons": [
      {
        "WeaponItemID": "Weapon_AK74M",
        "AttachmentItemIDs": ["Optic_EKP_Cobra", "Grip_RK1", "Muzzle_PBS4"],
        "Magazines": [
          {
            "MagazineItemID": "Mag_AK74_30rnd",
            "AmmoItemID": "Ammo_545x39_BT",
            "LoadedAmmoCount": 30,
            "Location": "InWeapon"
          },
          {
            "MagazineItemID": "Mag_AK74_30rnd",
            "AmmoItemID": "Ammo_545x39_BT",
            "LoadedAmmoCount": 30,
            "Location": "InRig"
          },
          {
            "MagazineItemID": "Mag_AK74_30rnd",
            "AmmoItemID": "Ammo_545x39_BT",
            "LoadedAmmoCount": 30,
            "Location": "InRig"
          },
          {
            "MagazineItemID": "Mag_AK74_30rnd",
            "AmmoItemID": "Ammo_545x39_BT",
            "LoadedAmmoCount": 30,
            "Location": "InRig"
          }
        ]
      },
      {
        "WeaponItemID": "Weapon_Glock17",
        "Magazines": [
          {
            "MagazineItemID": "Mag_Glock_17rnd",
            "AmmoItemID": "Ammo_9x19_PST",
            "LoadedAmmoCount": 17,
            "Location": "InWeapon"
          }
        ],
        "EquipSlot": "SecondaryWeapon"
      }
    ],
    "Armor": {
      "BodyArmorItemID": "Armor_6B13",
      "HelmetItemID": "Helmet_LZSh",
      "RigItemID": "Rig_Triton",
      "ArmorDurabilityPercent": 0.85,
      "HelmetDurabilityPercent": 0.9
    },
    "RigContents": [
      { "ItemID": "Grenade_F1", "Quantity": 2 },
      { "ItemID": "Medical_IFAK", "Quantity": 1 },
      { "ItemID": "Medical_Morphine", "Quantity": 1, "SpawnChance": 0.5 }
    ],
    "BackpackContents": [
      { "ItemID": "Medical_Salewa", "Quantity": 1 },
      { "ItemID": "Food_MRE", "Quantity": 1, "SpawnChance": 0.6 }
    ],
    "Currency": [
      { "ItemID": "Currency_Dollars", "MinQuantity": 50, "MaxQuantity": 200, "SpawnChance": 0.7 }
    ]
  }
}
```

---

## 5. Pipeline: От пресета к трупу

### 5.1 Spawn Flow
```
[1] SpawnEnemy(PresetRowName)
        │
        ▼
[2] DataManager->GetEnemyPreset(PresetRowName)
        │
        ▼
[3] CreateActualItems(LoadoutPreset)
        │   ├── Create Weapon Instances
        │   ├── Create Magazine Instances (with ammo)
        │   ├── Create Armor Instances (with durability)
        │   └── Create Inventory Items
        │
        ▼
[4] InventoryComponent->AddItems(CreatedItems)
        │
        ▼
[5] EquipmentComponent->EquipFromInventory()
        │   ├── Equip Primary Weapon
        │   ├── Equip Secondary Weapon
        │   ├── Equip Armor
        │   └── Equip Rig
        │
        ▼
[6] Enemy Ready (с реальным инвентарём)
```

### 5.2 Death & Loot Flow
```
[1] Enemy Dies
        │
        ▼
[2] DropEquippedWeapon() (optional - weapon on ground)
        │
        ▼
[3] Convert to Corpse/Loot Container
        │   └── Inventory remains INTACT
        │
        ▼
[4] Player Interacts with Corpse
        │
        ▼
[5] Open Loot UI
        │   ├── Show Equipped Items (Weapon, Armor, Rig)
        │   ├── Show Rig Contents (Magazines, Meds, etc.)
        │   ├── Show Backpack Contents
        │   └── Show Pockets
        │
        ▼
[6] Player Takes Items → Transfer to Player Inventory
```

---

## 6. Интеграция с существующими системами

### 6.1 С Equipment System
```cpp
// В SuspenseCoreEnemyCharacter::InitializeFromPreset()
void InitializeLoadout(const FEnemyLoadoutPreset& Loadout)
{
    // 1. Создать предметы оружия
    for (const FEnemyWeaponLoadout& WeaponConfig : Loadout.Weapons)
    {
        // Создать экземпляр оружия
        UItemInstance* WeaponItem = CreateItemInstance(WeaponConfig.WeaponItemID);

        // Добавить модификации
        for (const FName& AttachmentID : WeaponConfig.AttachmentItemIDs)
        {
            // AttachModification(WeaponItem, AttachmentID);
        }

        // Создать и зарядить магазины
        for (const FEnemyMagazineConfig& MagConfig : WeaponConfig.Magazines)
        {
            UItemInstance* Magazine = CreateMagazineWithAmmo(
                MagConfig.MagazineItemID,
                MagConfig.AmmoItemID,
                MagConfig.LoadedAmmoCount
            );

            if (MagConfig.Location == EEnemyMagazineLocation::InWeapon)
            {
                // Вставить магазин в оружие
                LoadMagazineIntoWeapon(WeaponItem, Magazine);
            }
            else
            {
                // Добавить в инвентарь (риг/карманы)
                InventoryComponent->AddItem(Magazine);
            }
        }

        // Экипировать оружие
        EquipmentComponent->EquipItem(WeaponItem, WeaponConfig.EquipSlot);
    }

    // 2. Создать броню
    // ...

    // 3. Заполнить инвентарь
    // ...
}
```

### 6.2 С GAS (Abilities)
```cpp
// Способности врага зависят от экипировки:
// - Есть оружие → GA_EnemyFire, GA_EnemyReload
// - Есть граната → GA_EnemyThrowGrenade
// - Есть аптечка → GA_EnemyHeal

void GrantAbilitiesFromLoadout(const FEnemyLoadoutPreset& Loadout)
{
    for (const FEnemyWeaponLoadout& Weapon : Loadout.Weapons)
    {
        // Дать способность стрельбы для этого типа оружия
        GrantWeaponAbilities(Weapon.WeaponItemID);
    }

    // Проверить наличие гранат в инвентаре
    if (HasItemType(Loadout, EItemType::Grenade))
    {
        ASC->GiveAbility(GA_EnemyThrowGrenade::StaticClass());
    }

    // Проверить наличие медикаментов
    if (HasItemType(Loadout, EItemType::Medical))
    {
        ASC->GiveAbility(GA_EnemyHeal::StaticClass());
    }
}
```

---

## 7. Файлы для создания/модификации

### 7.1 Новые файлы
```
Source/EnemySystem/Public/SuspenseCore/Types/
    └── SuspenseCoreEnemyLoadoutTypes.h    [NEW] - Структуры loadout

Source/EnemySystem/Private/SuspenseCore/Loadout/
    └── SuspenseCoreEnemyLoadoutHelper.cpp [NEW] - Хелперы создания items

Content/Data/EnemyDatabase/
    └── SuspenseCoreEnemyLoadouts.json     [NEW] - Пресеты loadout
```

### 7.2 Модификация существующих
```
Source/EnemySystem/Public/SuspenseCore/Types/
    └── SuspenseCoreEnemyTypes.h           [MODIFY] - Добавить FEnemyLoadoutPreset

Source/EnemySystem/Public/SuspenseCore/Characters/
    └── SuspenseCoreEnemyCharacter.h       [MODIFY] - Добавить InitializeLoadout()

Content/Data/EnemyDatabase/
    └── SuspenseCoreEnemyPresets.json      [MODIFY] - Добавить LoadoutPreset к каждому врагу
```

---

## 8. План реализации

### Phase 1: Структуры данных
- [ ] Создать `SuspenseCoreEnemyLoadoutTypes.h`
- [ ] Добавить новые структуры в `SuspenseCoreEnemyTypes.h`
- [ ] Обновить JSON schema

### Phase 2: Инициализация Loadout
- [ ] Реализовать `InitializeLoadout()` в EnemyCharacter
- [ ] Интеграция с InventoryComponent
- [ ] Интеграция с EquipmentComponent

### Phase 3: Смерть и Лут
- [ ] Создать Corpse/LootContainer систему
- [ ] UI для обыска трупа
- [ ] Transfer items to player

### Phase 4: Тестирование
- [ ] Spawn врага с loadout
- [ ] Проверить экипировку
- [ ] Проверить лут после смерти

---

## 9. Именования (Naming Conventions)

### DataTable Row Names
```
Enemy Presets:      Scav_Grunt, Scav_Heavy, Raider_Elite, Boss_Killa
Weapon Items:       Weapon_AK74M, Weapon_PM_Makarov, Weapon_M4A1
Magazine Items:     Mag_AK74_30rnd, Mag_PM_8rnd, Mag_M4_PMAG_30
Ammo Items:         Ammo_545x39_BT, Ammo_9x18_PM_Pst, Ammo_556x45_M855
Armor Items:        Armor_6B13, Armor_Paca, Armor_Slick
Helmet Items:       Helmet_LZSh, Helmet_Altyn, Helmet_ULACH
Rig Items:          Rig_Scav_Vest, Rig_Triton, Rig_AVS
```

### GameplayTags
```
Enemy.Faction.Scav
Enemy.Faction.Raider
Enemy.Faction.Boss
Enemy.Trait.Armored
Enemy.Trait.Elite
Enemy.Loadout.Light
Enemy.Loadout.Medium
Enemy.Loadout.Heavy
```

---

## 10. Резюме

**Ключевые принципы:**
1. **WYSIWYG** - What You See Is What You Get (реальный инвентарь)
2. **SSOT** - Все данные в DataTable, не хардкод
3. **Composition** - Loadout состоит из Item ID ссылок
4. **Deterministic** - Точные количества, не рандом везде
5. **Tarkov-style** - Обыск трупа показывает реальную экипировку
