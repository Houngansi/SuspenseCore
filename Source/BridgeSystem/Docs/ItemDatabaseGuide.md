# SuspenseCore Item Database Guide

## Обзор

Система предметов SuspenseCore использует JSON файлы как источник данных для заполнения UE5 DataTables. Это позволяет:
- Программно генерировать большие объёмы данных
- Версионировать базу предметов в Git
- Редактировать визуальные ассеты (иконки, меши) в редакторе после импорта

## Архитектура

```
JSON Files (SSOT)          →  UE5 DataTable  →  Runtime Data Manager
├── SuspenseCoreItemDatabase.json    FSuspenseCoreUnifiedItemData
├── SuspenseCoreAmmoAttributes.json  (атрибуты для GAS)
└── SuspenseCoreWeaponAttributes.json
```

## Расположение файлов

```
Content/Data/ItemDatabase/
├── SuspenseCoreItemDatabase.json      # Основная база предметов
├── SuspenseCoreAmmoAttributes.json    # Атрибуты патронов (GAS)
└── SuspenseCoreWeaponAttributes.json  # Атрибуты оружия (GAS)
```

---

## Структура FSuspenseCoreUnifiedItemData

Основной тип строки DataTable определён в:
`Source/BridgeSystem/Public/SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h`

### Обязательные поля

| Поле | Тип | Описание |
|------|-----|----------|
| `ItemID` | FName | Уникальный идентификатор (RowName в DataTable) |
| `DisplayName` | FText | Локализованное название |
| `Description` | FText | Локализованное описание |
| `ItemType` | FGameplayTag | Основной тип предмета |
| `GridSize` | FIntPoint | Размер в ячейках инвентаря (X,Y) |

### Классификация (GameplayTags)

```json
{
  "ItemType": "(TagName=\"Item.Weapon.AR\")",
  "Rarity": "(TagName=\"Item.Rarity.Rare\")",
  "ItemTags": "(GameplayTags=((TagName=\"Item.Weapon\"),(TagName=\"Weapon.Type.Ranged\")))"
}
```

### Инвентарные свойства

| Поле | Тип | По умолчанию | Описание |
|------|-----|--------------|----------|
| `GridSize` | FIntPoint | (1,1) | Размер в ячейках 64x64 |
| `MaxStackSize` | int32 | 1 | Максимальный стак (1 = не стакается) |
| `Weight` | float | 1.0 | Вес в кг |
| `BaseValue` | int32 | 0 | Базовая стоимость |

### Флаги поведения

| Поле | Тип | Описание |
|------|-----|----------|
| `bIsEquippable` | bool | Можно экипировать |
| `bIsConsumable` | bool | Расходуемый предмет |
| `bCanDrop` | bool | Можно выбросить |
| `bCanTrade` | bool | Можно обменять |
| `bIsWeapon` | bool | Это оружие |
| `bIsArmor` | bool | Это броня |
| `bIsAmmo` | bool | Это боеприпасы |

---

## GameplayTags Reference

### Слоты экипировки (Equipment.Slot.*)

Используйте теги из `SuspenseCoreEquipmentNativeTags.h`:

```cpp
Equipment.Slot.PrimaryWeapon    // AR, DMR, SR, Shotgun, LMG
Equipment.Slot.SecondaryWeapon  // SMG, Shotgun, PDW
Equipment.Slot.Holster          // Pistol, Revolver
Equipment.Slot.Scabbard         // Melee.Knife
Equipment.Slot.Headwear         // Helmet, Headwear
Equipment.Slot.Earpiece         // Earpiece
Equipment.Slot.Eyewear          // Eyewear
Equipment.Slot.FaceCover        // FaceCover
Equipment.Slot.BodyArmor        // BodyArmor
Equipment.Slot.TacticalRig      // TacticalRig
Equipment.Slot.Backpack         // Backpack
Equipment.Slot.SecureContainer  // SecureContainer
Equipment.Slot.Armband          // Armband
Equipment.Slot.QuickSlot1-4     // Consumable, Medical, Throwable, Ammo
```

### Типы предметов (Item.*)

```cpp
// Оружие
Item.Weapon.AR          // Автоматы (5x2)
Item.Weapon.DMR         // Марксманские винтовки (5x2)
Item.Weapon.SR          // Снайперские винтовки (5x2)
Item.Weapon.LMG         // Пулемёты (5x2)
Item.Weapon.Shotgun     // Дробовики (4x2 или 5x2)
Item.Weapon.SMG         // Пистолеты-пулемёты (4x2)
Item.Weapon.PDW         // PDW (3x2)
Item.Weapon.Pistol      // Пистолеты (2x2)
Item.Weapon.Revolver    // Револьверы (2x2)
Item.Weapon.Melee.Knife // Ножи (1x2)

// Броня
Item.Armor.Helmet       // Шлемы (2x2)
Item.Armor.BodyArmor    // Бронежилеты (3x3)

// Снаряжение
Item.Gear.Headwear      // Головные уборы (2x2)
Item.Gear.Earpiece      // Наушники (1x1)
Item.Gear.Eyewear       // Очки (1x1)
Item.Gear.FaceCover     // Маски (1x1)
Item.Gear.TacticalRig   // Разгрузки (2x3)
Item.Gear.Backpack      // Рюкзаки (3x3)
Item.Gear.SecureContainer // Контейнеры (2x2)
Item.Gear.Armband       // Повязки (1x1)

// Расходники
Item.Consumable         // Общий тег расходников
Item.Consumable.Medical // Медикаменты
Item.Medical            // Алиас для медикаментов
Item.Throwable          // Гранаты

// Патроны
Item.Ammo               // Общий тег патронов
Item.Ammo.Rifle         // Винтовочные
Item.Ammo.Pistol        // Пистолетные
Item.Ammo.Shotgun       // Дробовые
```

### Редкость (Item.Rarity.*)

```cpp
Item.Rarity.Common      // Белый
Item.Rarity.Uncommon    // Зелёный
Item.Rarity.Rare        // Синий
Item.Rarity.Epic        // Фиолетовый
Item.Rarity.Legendary   // Оранжевый
Item.Rarity.Mythic      // Красный
```

---

## Примеры JSON записей

### Оружие (Primary Weapon)

```json
{
  "Name": "AK74M",
  "ItemID": "Weapon_AK74M",
  "DisplayName": "АК-74М",
  "Description": "Модернизированный автомат Калашникова...",
  "ItemType": "(TagName=\"Item.Weapon.AR\")",
  "Rarity": "(TagName=\"Item.Rarity.Rare\")",
  "ItemTags": "(GameplayTags=((TagName=\"Item.Weapon\"),(TagName=\"Weapon.Type.Ranged\")))",
  "GridSize": "(X=5,Y=2)",
  "MaxStackSize": 1,
  "Weight": 3.4,
  "BaseValue": 38000,
  "bIsEquippable": true,
  "EquipmentSlot": "(TagName=\"Equipment.Slot.PrimaryWeapon\")",
  "bCanDrop": true,
  "bCanTrade": true,
  "bIsWeapon": true,
  "WeaponArchetype": "(TagName=\"Weapon.Type.Ranged\")",
  "AmmoType": "(TagName=\"Item.Ammo.Rifle\")",
  "AttachmentSocket": "weapon_r",
  "UnequippedSocket": "spine_03"
}
```

### Броня (Body Armor)

```json
{
  "Name": "6B13Armor",
  "ItemID": "Armor_6B13",
  "DisplayName": "Бронежилет 6Б13",
  "Description": "Общевойсковой бронежилет 4-го класса защиты...",
  "ItemType": "(TagName=\"Item.Armor.BodyArmor\")",
  "Rarity": "(TagName=\"Item.Rarity.Rare\")",
  "ItemTags": "(GameplayTags=((TagName=\"Item.Armor\")))",
  "GridSize": "(X=3,Y=3)",
  "MaxStackSize": 1,
  "Weight": 8.5,
  "BaseValue": 85000,
  "bIsEquippable": true,
  "EquipmentSlot": "(TagName=\"Equipment.Slot.BodyArmor\")",
  "bCanDrop": true,
  "bCanTrade": true,
  "bIsArmor": true,
  "ArmorType": "(TagName=\"Armor.Type.Body\")",
  "AttachmentSocket": "spine_03"
}
```

### Расходник (Medical)

```json
{
  "Name": "IFAK",
  "ItemID": "Medical_IFAK",
  "DisplayName": "IFAK",
  "Description": "Individual First Aid Kit...",
  "ItemType": "(TagName=\"Item.Consumable.Medical\")",
  "Rarity": "(TagName=\"Item.Rarity.Uncommon\")",
  "ItemTags": "(GameplayTags=((TagName=\"Item.Consumable\"),(TagName=\"Item.Medical\")))",
  "GridSize": "(X=1,Y=2)",
  "MaxStackSize": 1,
  "Weight": 0.3,
  "BaseValue": 15000,
  "bIsEquippable": true,
  "EquipmentSlot": "(TagName=\"Equipment.Slot.QuickSlot1\")",
  "bCanDrop": true,
  "bCanTrade": true,
  "bIsConsumable": true,
  "UseTime": 5.0
}
```

### Патроны (Ammo)

```json
{
  "Name": "Ammo_545x39_BP",
  "ItemID": "Ammo_545x39_BP",
  "DisplayName": "5.45x39 мм БП",
  "Description": "Бронебойный патрон с термоупрочнённым сердечником...",
  "ItemType": "(TagName=\"Item.Ammo.Rifle\")",
  "Rarity": "(TagName=\"Item.Rarity.Rare\")",
  "ItemTags": "(GameplayTags=((TagName=\"Item.Ammo\")))",
  "GridSize": "(X=1,Y=1)",
  "MaxStackSize": 60,
  "Weight": 0.01,
  "BaseValue": 350,
  "bIsEquippable": false,
  "bCanDrop": true,
  "bCanTrade": true,
  "bIsAmmo": true,
  "AmmoCaliber": "(TagName=\"Item.Ammo.Rifle\")"
}
```

---

## Атрибуты GAS

### SuspenseCoreAmmoAttributeSet

Файл: `SuspenseCoreAmmoAttributes.json`

```json
{
  "AmmoID": "Ammo_545x39_BP",
  "Caliber": "5.45x39mm",
  "AmmoName": "5.45x39 БП",
  "Attributes": {
    "BaseDamage": 37.0,
    "ArmorPenetration": 45.0,
    "StoppingPower": 0.25,
    "FragmentationChance": 0.17,
    "MuzzleVelocity": 890.0,
    "DragCoefficient": 0.175,
    "BulletMass": 3.7,
    "EffectiveRange": 400.0,
    "AccuracyModifier": 0.98,
    "RecoilModifier": 1.05,
    "RicochetChance": 0.35,
    "TracerVisibility": 0.0,
    "IncendiaryDamage": 0.0,
    "WeaponDegradationRate": 1.15,
    "MisfireChance": 0.001
  }
}
```

| Атрибут | Диапазон | Описание |
|---------|----------|----------|
| BaseDamage | 20-100 | Базовый урон пули |
| ArmorPenetration | 0-70 | Пробитие брони (vs ArmorClass) |
| StoppingPower | 0-1 | Останавливающее действие |
| FragmentationChance | 0-1 | Шанс фрагментации (доп. урон) |
| MuzzleVelocity | 300-1000 | Начальная скорость м/с |
| DragCoefficient | 0.1-0.3 | Коэффициент сопротивления |
| BulletMass | 2-15 | Масса пули в граммах |
| EffectiveRange | 30-1000 | Эффективная дальность м |
| AccuracyModifier | 0.8-1.1 | Модификатор точности |
| RecoilModifier | 0.8-1.2 | Модификатор отдачи |
| RicochetChance | 0-0.5 | Шанс рикошета |
| TracerVisibility | 0 или 1 | Трассер (видимость) |
| IncendiaryDamage | 0-20 | Урон от воспламенения |
| WeaponDegradationRate | 0.8-1.5 | Износ оружия |
| MisfireChance | 0-0.01 | Шанс осечки |

### SuspenseCoreWeaponAttributeSet

Файл: `SuspenseCoreWeaponAttributes.json`

```json
{
  "WeaponID": "Weapon_AK74M",
  "WeaponName": "АК-74М",
  "WeaponType": "AssaultRifle",
  "Caliber": "5.45x39mm",
  "Attributes": {
    "BaseDamage": 42.0,
    "RateOfFire": 650.0,
    "EffectiveRange": 350.0,
    "MaxRange": 600.0,
    "MagazineSize": 30.0,
    "TacticalReloadTime": 2.1,
    "FullReloadTime": 2.8,
    "MOA": 2.9,
    "HipFireSpread": 0.12,
    "AimSpread": 0.025,
    "VerticalRecoil": 145.0,
    "HorizontalRecoil": 280.0,
    "Durability": 100.0,
    "MaxDurability": 100.0,
    "MisfireChance": 0.001,
    "JamChance": 0.002,
    "Ergonomics": 42.0,
    "AimDownSightTime": 0.35,
    "WeaponWeight": 3.4
  },
  "FireModes": ["Single", "Auto"],
  "DefaultFireMode": "Auto"
}
```

| Атрибут | Диапазон | Описание |
|---------|----------|----------|
| BaseDamage | 20-100 | Базовый урон (до модификации патроном) |
| RateOfFire | 80-1200 | Скорострельность RPM |
| EffectiveRange | 50-1000 | Эффективная дальность м |
| MaxRange | 100-1500 | Максимальная дальность м |
| MagazineSize | 1-100 | Ёмкость магазина |
| TacticalReloadTime | 1-5 | Тактическая перезарядка сек |
| FullReloadTime | 2-7 | Полная перезарядка сек |
| MOA | 0.5-5 | Точность (Minutes of Angle) |
| HipFireSpread | 0.05-0.3 | Разброс от бедра |
| AimSpread | 0.01-0.05 | Разброс в прицеле |
| VerticalRecoil | 50-300 | Вертикальная отдача |
| HorizontalRecoil | 100-400 | Горизонтальная отдача |
| Durability | 0-100 | Текущая прочность |
| MaxDurability | 100 | Максимальная прочность |
| MisfireChance | 0-0.01 | Шанс осечки |
| JamChance | 0-0.02 | Шанс заклинивания |
| Ergonomics | 20-100 | Эргономика (ADS speed, etc) |
| AimDownSightTime | 0.1-0.6 | Время прицеливания сек |
| WeaponWeight | 0.3-10 | Вес в кг |

---

## Импорт в UE5 DataTable

### Создание DataTable

1. Content Browser → Right Click → Miscellaneous → Data Table
2. Выбрать Row Structure: `FSuspenseCoreUnifiedItemData`
3. Назвать: `DT_ItemDatabase`

### Импорт JSON

1. Выбрать DataTable в Content Browser
2. Right Click → Reimport (или Asset Actions → Reimport)
3. Выбрать JSON файл
4. Подтвердить импорт

### После импорта

Отредактируйте в редакторе:
- `Icon` — ссылка на текстуру иконки
- `WorldMesh` — меш для отображения в мире
- `EquipmentActorClass` — класс актора экипировки
- Звуки и VFX

---

## Naming Conventions

### ItemID

```
{Category}_{Name}

Weapon_AK74M
Weapon_Glock17
Armor_6B13
Armor_6B47Helmet
Gear_TV110Rig
Gear_BergenBackpack
Medical_IFAK
Medical_Bandage
Throwable_F1Grenade
Ammo_545x39_PS
Ammo_9x19_AP
```

### Категории

| Префикс | Описание |
|---------|----------|
| Weapon_ | Оружие |
| Armor_ | Броня |
| Gear_ | Снаряжение |
| Medical_ | Медикаменты |
| Throwable_ | Метательные |
| Ammo_ | Патроны |
| Key_ | Ключи |
| Quest_ | Квестовые |

---

## Размеры ячеек (GridSize)

Базовый размер ячейки: **64x64 пикселей**

| Тип предмета | GridSize | Пиксели |
|--------------|----------|---------|
| Автомат/Винтовка | 5x2 | 320x128 |
| SMG/Дробовик | 4x2 | 256x128 |
| Пистолет | 2x2 | 128x128 |
| Нож | 1x2 | 64x128 |
| Шлем | 2x2 | 128x128 |
| Бронежилет | 3x3 | 192x192 |
| Разгрузка | 2x3 | 128x192 |
| Рюкзак | 3x3 | 192x192 |
| Наушники/Очки | 1x1 | 64x64 |
| Аптечка | 1x2 | 64x128 |
| Граната | 1x1 | 64x64 |
| Патроны (стак) | 1x1 | 64x64 |

---

## Валидация

### Обязательные условия

1. `ItemID` уникален в базе
2. `ItemType` соответствует одному из зарегистрированных тегов
3. `EquipmentSlot` совместим с `ItemType` (см. SuspenseCoreEquipmentSlotPresets.cpp)
4. `GridSize` > 0 для обоих измерений
5. `bIsWeapon` → должны быть заполнены `WeaponArchetype`, `AmmoType`
6. `bIsArmor` → должен быть заполнен `ArmorType`
7. `bIsAmmo` → должен быть заполнен `AmmoCaliber`

### Проверка совместимости слотов

```cpp
// Из SuspenseCoreEquipmentSlotPresets.cpp
PrimaryWeapon: AR, DMR, SR, Shotgun, LMG
SecondaryWeapon: SMG, Shotgun, PDW
Holster: Pistol, Revolver
Scabbard: Melee.Knife
Headwear: Helmet, Headwear
QuickSlots: Consumable, Medical, Throwable, Ammo
```

---

## Расширение системы

### Добавление нового типа предмета

1. Добавить тег в `DefaultGameplayTags.ini`:
```ini
+GameplayTagList=(Tag="Item.NewType",DevComment="New item type")
```

2. Добавить native тег в `SuspenseCoreGameplayTags.h` (если частое использование):
```cpp
namespace Item
{
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(NewType);
}
```

3. Определить тег в `SuspenseCoreGameplayTags.cpp`:
```cpp
UE_DEFINE_GAMEPLAY_TAG(Item::NewType, "Item.NewType");
```

4. Обновить разрешённые типы в слотах (`SuspenseCoreEquipmentSlotPresets.cpp`)

### Добавление нового слота

1. Добавить enum в `EEquipmentSlotType`
2. Добавить тег в `SuspenseCoreEquipmentNativeTags.h`
3. Добавить пресет в `CreateDefaultPresets()`
4. Обновить UI виджет экипировки

---

## Troubleshooting

### Тег не найден при импорте

```
Warning: Tag "Item.Weapon.NewGun" not found
```

**Решение:** Добавьте тег в `DefaultGameplayTags.ini` и перезапустите редактор.

### Предмет не помещается в слот

Проверьте:
1. `ItemType` в списке `AllowedItemTypes` для слота
2. `EquipmentSlot` совпадает с целевым слотом
3. `bIsEquippable = true`

### GAS атрибуты не применяются

Проверьте:
1. `WeaponID`/`AmmoID` совпадает с `ItemID`
2. AttributeSet класс указан в DataTable
3. InitEffect применяется при экипировке

---

## См. также

- `SuspenseCoreItemTypes.h` — типы данных предметов
- `SuspenseCoreItemDataTable.h` — структура FSuspenseCoreUnifiedItemData
- `SuspenseCoreEquipmentNativeTags.h` — native теги экипировки
- `SuspenseCoreEquipmentSlotPresets.cpp` — конфигурация слотов
- `SuspenseCoreAmmoAttributeSet.h` — атрибуты патронов
- `SuspenseCoreWeaponAttributeSet.h` — атрибуты оружия
