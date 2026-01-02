# SSOT AttributeSet DataTable Integration Plan

> **Version:** 1.0
> **Created:** 2026-01-02
> **Status:** Planning
> **Author:** Technical Lead
> **Target:** Unreal Engine 5.x | SuspenseCore GAS Architecture

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Reference Documentation](#2-reference-documentation)
3. [Current Architecture Analysis](#3-current-architecture-analysis)
4. [Target Architecture](#4-target-architecture)
5. [Implementation Steps](#5-implementation-steps)
6. [GameplayTags Reference](#6-gameplaytags-reference)
7. [UI Compatibility Checklist](#7-ui-compatibility-checklist)
8. [Testing Plan](#8-testing-plan)
9. [Rollback Strategy](#9-rollback-strategy)

---

## 1. Executive Summary

### 1.1 Problem Statement

Текущая реализация AttributeSet для оружия и патронов использует **legacy поля с прямыми ссылками на классы** вместо DataTable lookup:

```cpp
// LEGACY (текущее состояние) - SuspenseCoreItemDataTable.h:121-129
struct FWeaponInitializationData
{
    TSubclassOf<UAttributeSet> WeaponAttributeSetClass;  // ← Прямая ссылка
    TSubclassOf<UGameplayEffect> WeaponInitEffect;       // ← Требует GE на каждое оружие
};
```

**Последствия:**
- AttributeSet инициализируется с hardcoded дефолтами из конструктора
- JSON данные (`SuspenseCoreWeaponAttributes.json`) не используются в runtime
- Атрибуты оружия НЕ отображаются в `showdebug abilitysystem`
- Дублирование данных: JSON, конструктор, GameplayEffect

### 1.2 Solution

Заменить legacy поля на **FName ссылки на DataTable** + создать метод `InitializeFromData()`:

```cpp
// TARGET (целевое состояние)
struct FSuspenseCoreUnifiedItemData
{
    // DEPRECATED legacy fields
    UPROPERTY(meta = (DeprecatedProperty))
    FWeaponInitializationData WeaponInitialization;

    // NEW: DataTable row reference (SSOT)
    UPROPERTY(EditAnywhere, Category = "Item|Weapon|SSOT")
    FName WeaponAttributesRowName;  // → lookup в WeaponAttributesDataTable

    UPROPERTY(EditAnywhere, Category = "Item|Ammo|SSOT")
    FName AmmoAttributesRowName;    // → lookup в AmmoAttributesDataTable
};
```

---

## 2. Reference Documentation

### 2.1 ОБЯЗАТЕЛЬНЫЕ документы для изучения перед реализацией

| Document | Path | Purpose |
|----------|------|---------|
| **SuspenseCore Architecture** | `Source/BridgeSystem/Documentation/SuspenseCoreArchitecture.md` | SSOT, DataManager, EventBus patterns |
| **Item Database Guide** | `Source/BridgeSystem/Docs/ItemDatabaseGuide.md` | JSON structure, AttributeSet fields mapping |
| **Weapon Abilities Guide** | `Documentation/WeaponAbilities_DeveloperGuide.md` | GAS integration, interface patterns |
| **Developer Guide** | `Documentation/SuspenseCoreDeveloperGuide.md` | Naming conventions, EventBus usage |
| **Equipment Checklist** | `Documentation/EquipmentModule_ReviewChecklist.md` | 17 slots, validation, SOLID principles |

### 2.2 Ключевые файлы кода

| File | Path | Role |
|------|------|------|
| **Settings** | `Source/BridgeSystem/Public/SuspenseCore/Settings/SuspenseCoreSettings.h` | DataTable references |
| **DataManager** | `Source/BridgeSystem/Public/SuspenseCore/Data/SuspenseCoreDataManager.h` | SSOT cache |
| **UnifiedItemData** | `Source/BridgeSystem/Public/SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h` | Item structure with legacy fields |
| **WeaponAttributeSet** | `Source/GAS/Public/SuspenseCore/Attributes/SuspenseCoreWeaponAttributeSet.h` | 19 weapon attributes |
| **AmmoAttributeSet** | `Source/GAS/Public/SuspenseCore/Attributes/SuspenseCoreAmmoAttributeSet.h` | 15 ammo attributes |
| **EquipmentAttrComp** | `Source/EquipmentSystem/Public/SuspenseCore/Components/SuspenseCoreEquipmentAttributeComponent.h` | AttributeSet creation |
| **WeaponAmmoComp** | `Source/EquipmentSystem/Public/SuspenseCore/Components/SuspenseCoreWeaponAmmoComponent.h` | Ammo management |
| **Native Tags** | `Source/EquipmentSystem/Public/SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h` | Equipment native tags |
| **Gameplay Tags** | `Source/BridgeSystem/Public/SuspenseCore/Tags/SuspenseCoreGameplayTags.h` | Core native tags |

### 2.3 JSON Data Sources (загружаются в DataTable)

| JSON File | Path | Target DataTable |
|-----------|------|------------------|
| Item Database | `Content/Data/ItemDatabase/SuspenseCoreItemDatabase.json` | DT_ItemDatabase |
| Weapon Attributes | `Content/Data/ItemDatabase/SuspenseCoreWeaponAttributes.json` | DT_WeaponAttributes (NEW) |
| Ammo Attributes | `Content/Data/ItemDatabase/SuspenseCoreAmmoAttributes.json` | DT_AmmoAttributes (NEW) |

---

## 3. Current Architecture Analysis

### 3.1 Legacy Fields Location

```
SuspenseCoreItemDataTable.h (lines 113-204)
├── FWeaponInitializationData (lines 113-132)
│   ├── TSubclassOf<UAttributeSet> WeaponAttributeSetClass  ← LEGACY
│   └── TSubclassOf<UGameplayEffect> WeaponInitEffect       ← LEGACY
│
├── FAmmoInitializationData (lines 137-156)
│   ├── TSubclassOf<UAttributeSet> AmmoAttributeSetClass    ← LEGACY
│   └── TSubclassOf<UGameplayEffect> AmmoInitEffect         ← LEGACY
│
└── FArmorInitializationData (lines 161-180)
    ├── TSubclassOf<UAttributeSet> ArmorAttributeSetClass   ← LEGACY
    └── TSubclassOf<UGameplayEffect> ArmorInitEffect        ← LEGACY
```

### 3.2 Current AttributeSet Initialization Flow

```
1. FSuspenseCoreUnifiedItemData loaded from ItemDataTable
   └── WeaponInitialization.WeaponAttributeSetClass = USuspenseCoreWeaponAttributeSet

2. EquipmentAttributeComponent::CreateAttributeSetsForItem()
   ├── File: SuspenseCoreEquipmentAttributeComponent.cpp:182-194
   ├── NewObject<UAttributeSet>(Owner, WeaponInitialization.WeaponAttributeSetClass)
   │   └── Constructor called: USuspenseCoreWeaponAttributeSet()
   │       └── InitBaseDamage(100.0f);  // ← HARDCODED, NOT FROM JSON!
   │       └── InitRateOfFire(600.0f);  // ← WRONG VALUES
   │
   └── ApplyInitializationEffect(WeaponAttributeSet, WeaponInitEffect, ItemData)
       └── GameplayEffect SHOULD modify attributes...
           └── But requires manual GE creation for EACH weapon!

3. showdebug abilitysystem → NO weapon attributes visible!
   └── Because AttributeSet has wrong values / not properly added to ASC
```

### 3.3 Files Using Legacy Fields

```cpp
// Files that need modification:
SuspenseCoreDataManager.cpp:413-414           // Uses WeaponInitialization
SuspenseCoreEquipmentAttributeComponent.cpp:182-194  // Creates AttributeSet from class
SuspenseCoreEquipmentAbilityConnector.cpp:337,1154-1156  // Uses WeaponInitEffect
SuspenseCoreItemDataTable.cpp:53-56,123,240-247,417  // Validation of legacy fields
SuspenseCoreWeaponActor.cpp:257-259           // Uses FWeaponInitializationResult
DisabledModules/UISystem/.../SuspenseItemTooltipWidget.cpp:381-639  // UI tooltip
```

---

## 4. Target Architecture

### 4.1 Data Flow Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         ЕДИНЫЙ ИСТОЧНИК ИСТИНЫ (SSOT)                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  Content/Data/ItemDatabase/                                                  │
│  ├── SuspenseCoreWeaponAttributes.json ─────┐                               │
│  └── SuspenseCoreAmmoAttributes.json ───────┼── Imported via Editor         │
│                                             │                                │
│  Content/Data/ItemDatabase/                 ▼                                │
│  ├── DT_WeaponAttributes.uasset ◄───────────┘  (FSuspenseCoreWeaponAttrRow) │
│  └── DT_AmmoAttributes.uasset ◄─────────────── (FSuspenseCoreAmmoAttrRow)   │
│                                             │                                │
│  USuspenseCoreSettings (Project Settings)   │                                │
│  ├── ItemDataTable ──────────────────────── │ ─→ DT_ItemDatabase            │
│  ├── WeaponAttributesDataTable ◄────────────┘ ─→ DT_WeaponAttributes (NEW)  │
│  └── AmmoAttributesDataTable ◄──────────────── ─→ DT_AmmoAttributes (NEW)   │
│                                             │                                │
│  USuspenseCoreDataManager (SSOT)            │                                │
│  ├── UnifiedItemCache<ItemID, Data>         │                                │
│  ├── WeaponAttributesCache<ID, AttrRow> ◄───┘ NEW                           │
│  └── AmmoAttributesCache<ID, AttrRow> ◄─────── NEW                          │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
                                     │
                                     ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                    EQUIPMENT FLOW (при экипировке)                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  EquipmentComponent::EquipItem(ItemID)                                       │
│  └─→ DataManager->GetUnifiedItemData(ItemID) → FSuspenseCoreUnifiedItemData │
│                                                                              │
│  EquipmentAttributeComponent::CreateAttributeSetsForItem()                   │
│  │                                                                           │
│  │  if (ItemData.bIsWeapon)                                                 │
│  │  {                                                                        │
│  │      // 1. Get row key (NEW field or fallback to ItemID)                 │
│  │      FName RowKey = ItemData.WeaponAttributesRowName.IsNone()            │
│  │                   ? ItemData.ItemID                                       │
│  │                   : ItemData.WeaponAttributesRowName;                     │
│  │                                                                           │
│  │      // 2. Get attributes from DataManager (SSOT)                        │
│  │      FSuspenseCoreWeaponAttributeRow WeaponAttrs;                        │
│  │      DataManager->GetWeaponAttributes(RowKey, WeaponAttrs);              │
│  │                                                                           │
│  │      // 3. Create AttributeSet (fixed class, not from legacy field)     │
│  │      WeaponAttributeSet = NewObject<USuspenseCoreWeaponAttributeSet>();  │
│  │                                                                           │
│  │      // 4. Initialize from DataTable data (NEW method)                   │
│  │      WeaponAttributeSet->InitializeFromData(WeaponAttrs);                │
│  │                                                                           │
│  │      // 5. Add to ASC → showdebug abilitysystem shows attributes!        │
│  │      CachedASC->AddAttributeSetSubobject(WeaponAttributeSet);            │
│  │  }                                                                        │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.2 New Structures

#### FSuspenseCoreWeaponAttributeRow

```cpp
// NEW FILE: Source/BridgeSystem/Public/SuspenseCore/Types/GAS/SuspenseCoreGASAttributeRows.h

USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreWeaponAttributeRow : public FTableRowBase
{
    GENERATED_BODY()

    //================================================
    // Identity (связь с ItemDataTable)
    //================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FName WeaponID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FText WeaponName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity",
        meta = (Categories = "Weapon.Type"))
    FGameplayTag WeaponType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity",
        meta = (Categories = "Item.Ammo"))
    FGameplayTag Caliber;

    //================================================
    // Combat Attributes (1:1 mapping to USuspenseCoreWeaponAttributeSet)
    //================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat",
        meta = (ClampMin = "20", ClampMax = "100"))
    float BaseDamage = 42.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat",
        meta = (ClampMin = "80", ClampMax = "1200"))
    float RateOfFire = 650.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat",
        meta = (ClampMin = "50", ClampMax = "1000"))
    float EffectiveRange = 350.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat",
        meta = (ClampMin = "100", ClampMax = "1500"))
    float MaxRange = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat",
        meta = (ClampMin = "1", ClampMax = "100"))
    float MagazineSize = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat",
        meta = (ClampMin = "1", ClampMax = "5"))
    float TacticalReloadTime = 2.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat",
        meta = (ClampMin = "2", ClampMax = "7"))
    float FullReloadTime = 2.8f;

    //================================================
    // Accuracy Attributes
    //================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy",
        meta = (ClampMin = "0.5", ClampMax = "5"))
    float MOA = 2.9f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy",
        meta = (ClampMin = "0.05", ClampMax = "0.3"))
    float HipFireSpread = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy",
        meta = (ClampMin = "0.01", ClampMax = "0.05"))
    float AimSpread = 0.025f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy",
        meta = (ClampMin = "50", ClampMax = "300"))
    float VerticalRecoil = 145.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy",
        meta = (ClampMin = "100", ClampMax = "400"))
    float HorizontalRecoil = 280.0f;

    //================================================
    // Reliability Attributes
    //================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reliability",
        meta = (ClampMin = "0", ClampMax = "100"))
    float Durability = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reliability")
    float MaxDurability = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reliability",
        meta = (ClampMin = "0", ClampMax = "0.01"))
    float MisfireChance = 0.001f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reliability",
        meta = (ClampMin = "0", ClampMax = "0.02"))
    float JamChance = 0.002f;

    //================================================
    // Ergonomics Attributes
    //================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ergonomics",
        meta = (ClampMin = "20", ClampMax = "100"))
    float Ergonomics = 42.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ergonomics",
        meta = (ClampMin = "0.1", ClampMax = "0.6"))
    float AimDownSightTime = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ergonomics",
        meta = (ClampMin = "0.3", ClampMax = "10"))
    float WeaponWeight = 3.4f;

    //================================================
    // Fire Modes (not GAS attributes, but useful metadata)
    //================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireModes",
        meta = (Categories = "Weapon.FireMode"))
    TArray<FGameplayTag> FireModes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireModes",
        meta = (Categories = "Weapon.FireMode"))
    FGameplayTag DefaultFireMode;
};
```

#### FSuspenseCoreAmmoAttributeRow

```cpp
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreAmmoAttributeRow : public FTableRowBase
{
    GENERATED_BODY()

    //================================================
    // Identity
    //================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FName AmmoID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FText AmmoName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity",
        meta = (Categories = "Item.Ammo"))
    FGameplayTag Caliber;

    //================================================
    // Damage Attributes (1:1 mapping to USuspenseCoreAmmoAttributeSet)
    //================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    float BaseDamage = 42.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    float ArmorPenetration = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    float StoppingPower = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    float FragmentationChance = 0.40f;

    //================================================
    // Ballistics Attributes
    //================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistics")
    float MuzzleVelocity = 890.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistics")
    float DragCoefficient = 0.168f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistics")
    float BulletMass = 3.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistics")
    float EffectiveRange = 350.0f;

    //================================================
    // Accuracy Modifiers
    //================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy")
    float AccuracyModifier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy")
    float RecoilModifier = 1.0f;

    //================================================
    // Special Effects
    //================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Special")
    float RicochetChance = 0.30f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Special")
    float TracerVisibility = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Special")
    float IncendiaryDamage = 0.0f;

    //================================================
    // Weapon Effects
    //================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float WeaponDegradationRate = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float MisfireChance = 0.001f;
};
```

---

## 5. Implementation Steps

### Phase 1: Create DataTable Row Structures

| Step | Task | File | Priority |
|------|------|------|----------|
| 1.1 | Create `FSuspenseCoreWeaponAttributeRow` | `BridgeSystem/Types/GAS/SuspenseCoreGASAttributeRows.h` | HIGH |
| 1.2 | Create `FSuspenseCoreAmmoAttributeRow` | Same file | HIGH |
| 1.3 | Register types in BridgeSystem module | `BridgeSystem.Build.cs` | HIGH |

### Phase 2: Add DataTables to Settings

| Step | Task | File | Priority |
|------|------|------|----------|
| 2.1 | Add `WeaponAttributesDataTable` to Settings | `SuspenseCoreSettings.h` | HIGH |
| 2.2 | Add `AmmoAttributesDataTable` to Settings | Same file | HIGH |
| 2.3 | Create DataTable assets in Editor | `Content/Data/ItemDatabase/` | HIGH |
| 2.4 | Import JSON into DataTables | Editor workflow | HIGH |

### Phase 3: Extend DataManager (SSOT)

| Step | Task | File | Priority |
|------|------|------|----------|
| 3.1 | Add `WeaponAttributesCache` TMap | `SuspenseCoreDataManager.h` | HIGH |
| 3.2 | Add `AmmoAttributesCache` TMap | Same file | HIGH |
| 3.3 | Add `GetWeaponAttributes(ItemID)` method | `SuspenseCoreDataManager.h/cpp` | HIGH |
| 3.4 | Add `GetAmmoAttributes(ItemID)` method | Same file | HIGH |
| 3.5 | Implement `InitializeWeaponAttributesSystem()` | `SuspenseCoreDataManager.cpp` | HIGH |
| 3.6 | Implement `InitializeAmmoAttributesSystem()` | Same file | HIGH |
| 3.7 | Call init in `Initialize()` | `SuspenseCoreDataManager.cpp` | HIGH |

### Phase 4: Add InitializeFromData Methods

| Step | Task | File | Priority |
|------|------|------|----------|
| 4.1 | Add `InitializeFromData(Row)` to WeaponAttributeSet | `SuspenseCoreWeaponAttributeSet.h/cpp` | HIGH |
| 4.2 | Add `InitializeFromData(Row)` to AmmoAttributeSet | `SuspenseCoreAmmoAttributeSet.h/cpp` | HIGH |
| 4.3 | Remove hardcoded defaults from constructors | Both files | MEDIUM |

### Phase 5: Replace Legacy Fields in UnifiedItemData

| Step | Task | File | Priority |
|------|------|------|----------|
| 5.1 | Add `WeaponAttributesRowName` field | `SuspenseCoreItemDataTable.h` | HIGH |
| 5.2 | Add `AmmoAttributesRowName` field | Same file | HIGH |
| 5.3 | Mark `WeaponInitialization` as DEPRECATED | Same file | MEDIUM |
| 5.4 | Add helper `GetWeaponAttributesKey()` | Same file | MEDIUM |
| 5.5 | Update validation in `GetValidationErrors()` | `SuspenseCoreItemDataTable.cpp` | MEDIUM |

### Phase 6: Modify EquipmentAttributeComponent

| Step | Task | File | Priority |
|------|------|------|----------|
| 6.1 | Update `CreateAttributeSetsForItem()` | `SuspenseCoreEquipmentAttributeComponent.cpp` | HIGH |
| 6.2 | Use fixed class instead of legacy field | Same file | HIGH |
| 6.3 | Call `InitializeFromData()` | Same file | HIGH |
| 6.4 | Ensure ASC `AddAttributeSetSubobject()` | Same file | HIGH |
| 6.5 | Add EventBus broadcast for attributes applied | Same file | MEDIUM |

### Phase 7: Update Dependent Files

| Step | Task | File | Priority |
|------|------|------|----------|
| 7.1 | Update `SuspenseCoreEquipmentAbilityConnector` | `.cpp:337,1154-1156` | MEDIUM |
| 7.2 | Update tooltip widget (if enabled) | `SuspenseItemTooltipWidget.cpp` | LOW |
| 7.3 | Update any other consumers | Search for usage | MEDIUM |

### Phase 8: Testing & Verification

| Step | Task | Priority |
|------|------|----------|
| 8.1 | Verify `showdebug abilitysystem` shows weapon attributes | HIGH |
| 8.2 | Verify UI slots still display correctly | HIGH |
| 8.3 | Verify attribute values match JSON data | HIGH |
| 8.4 | Test reload with ammo attributes | HIGH |
| 8.5 | Network replication test | HIGH |

---

## 6. GameplayTags Reference

### 6.1 Equipment Slot Tags (DO NOT MODIFY)

These tags are used by UI and must remain unchanged:

```cpp
// From SuspenseCoreEquipmentNativeTags.h - Slot namespace (lines 258-302)

namespace Slot
{
    // Weapons (indices 0-3)
    TAG_Equipment_Slot_PrimaryWeapon   // Index 0
    TAG_Equipment_Slot_SecondaryWeapon // Index 1
    TAG_Equipment_Slot_Holster         // Index 2
    TAG_Equipment_Slot_Scabbard        // Index 3

    // Head gear (indices 4-7)
    TAG_Equipment_Slot_Headwear        // Index 4
    TAG_Equipment_Slot_Earpiece        // Index 5
    TAG_Equipment_Slot_Eyewear         // Index 6
    TAG_Equipment_Slot_FaceCover       // Index 7

    // Body gear (indices 8-9)
    TAG_Equipment_Slot_BodyArmor       // Index 8
    TAG_Equipment_Slot_TacticalRig     // Index 9

    // Storage (indices 10-12)
    TAG_Equipment_Slot_Backpack        // Index 10
    TAG_Equipment_Slot_SecureContainer // Index 11
    TAG_Equipment_Slot_Armband         // Index 12

    // Quick slots (indices 13-16) - Used for ammo/magazines!
    TAG_Equipment_Slot_QuickSlot1      // Index 13
    TAG_Equipment_Slot_QuickSlot2      // Index 14
    TAG_Equipment_Slot_QuickSlot3      // Index 15
    TAG_Equipment_Slot_QuickSlot4      // Index 16
}
```

### 6.2 Item Type Tags (Reference)

```cpp
// From SuspenseCoreEquipmentNativeTags.h - Item namespace (lines 351-393)

namespace Item
{
    TAG_Item_Weapon
    TAG_Item_Weapon_AR           // → PrimaryWeapon slot
    TAG_Item_Weapon_DMR          // → PrimaryWeapon slot
    TAG_Item_Weapon_SR           // → PrimaryWeapon slot
    TAG_Item_Weapon_LMG          // → PrimaryWeapon slot
    TAG_Item_Weapon_SMG          // → SecondaryWeapon slot
    TAG_Item_Weapon_Shotgun      // → Primary or Secondary
    TAG_Item_Weapon_PDW          // → SecondaryWeapon slot
    TAG_Item_Weapon_Pistol       // → Holster slot
    TAG_Item_Weapon_Revolver     // → Holster slot
    TAG_Item_Weapon_Melee_Knife  // → Scabbard slot

    TAG_Item_Armor
    TAG_Item_Armor_Helmet        // → Headwear slot
    TAG_Item_Armor_BodyArmor     // → BodyArmor slot

    TAG_Item_Consumable          // → QuickSlots
    TAG_Item_Medical             // → QuickSlots
    TAG_Item_Throwable           // → QuickSlots
    TAG_Item_Ammo                // → QuickSlots or Inventory
}
```

### 6.3 Weapon Archetype Tags (For Animation)

```cpp
// From SuspenseCoreEquipmentNativeTags.h - Archetype namespace (lines 163-203)

namespace Archetype
{
    TAG_Weapon_Rifle_Assault
    TAG_Weapon_Rifle_Sniper
    TAG_Weapon_Rifle_DMR
    TAG_Weapon_Pistol_Semi
    TAG_Weapon_Pistol_Revolver
    TAG_Weapon_SMG_Compact
    TAG_Weapon_SMG_Full
    TAG_Weapon_Shotgun_Pump
    TAG_Weapon_Shotgun_Auto
    TAG_Weapon_Heavy_LMG
    TAG_Weapon_Melee_Knife
}
```

---

## 7. UI Compatibility Checklist

### 7.1 Files That Use Equipment Tags

| File | Tags Used | Impact |
|------|-----------|--------|
| `SuspenseCoreEquipmentWidget.cpp` | Slot tags for rendering | NONE - read only |
| `SuspenseCoreEquipmentSlotValidator.cpp` | Item type → Slot validation | NONE - uses tag matching |
| `SuspenseCoreEquipmentDataService.cpp` | Slot events | NONE - uses native tags |
| `SuspenseCoreLoadoutManager.cpp` | QuickSlot1-4 for loadouts | NONE - unchanged |

### 7.2 Compatibility Rules

1. **DO NOT modify tag names** in:
   - `SuspenseCoreEquipmentNativeTags.h`
   - `SuspenseCoreGameplayTags.h`
   - `Config/DefaultGameplayTags.ini`

2. **DO NOT change slot indices** (0-16 mapping)

3. **DO NOT modify** `FSuspenseCoreUnifiedItemData` fields used by UI:
   - `ItemID`
   - `DisplayName`
   - `Icon`
   - `ItemType`
   - `EquipmentSlot`
   - `GridSize`

4. **ADD fields, don't replace**:
   - `WeaponAttributesRowName` is NEW field
   - `WeaponInitialization` remains for backward compatibility

### 7.3 Testing Checklist for UI

- [ ] Equipment slots render correctly in Equipment Widget
- [ ] Items can be dragged to correct slots
- [ ] Slot validation works (wrong item type rejected)
- [ ] QuickSlots 1-4 accept consumables/ammo
- [ ] Tooltip shows correct item data
- [ ] Icon displays properly
- [ ] Rarity colors work

---

## 8. Testing Plan

### 8.1 Unit Tests

```cpp
// Test AttributeSet initialization from DataTable row
TEST_CASE("WeaponAttributeSet initializes from DataTable")
{
    FSuspenseCoreWeaponAttributeRow Row;
    Row.WeaponID = "Weapon_AK74M";
    Row.BaseDamage = 42.0f;
    Row.RateOfFire = 650.0f;

    USuspenseCoreWeaponAttributeSet* AttrSet = NewObject<USuspenseCoreWeaponAttributeSet>();
    AttrSet->InitializeFromData(Row);

    CHECK(AttrSet->GetBaseDamage() == 42.0f);
    CHECK(AttrSet->GetRateOfFire() == 650.0f);
}

// Test DataManager lookup
TEST_CASE("DataManager returns weapon attributes by ItemID")
{
    USuspenseCoreDataManager* DM = USuspenseCoreDataManager::Get(World);

    FSuspenseCoreWeaponAttributeRow Attrs;
    bool bFound = DM->GetWeaponAttributes("Weapon_AK74M", Attrs);

    CHECK(bFound == true);
    CHECK(Attrs.BaseDamage == 42.0f);
}
```

### 8.2 Integration Tests

1. **showdebug abilitysystem Test:**
   ```
   1. Equip Weapon_AK74M to PrimaryWeapon slot
   2. Open console: showdebug abilitysystem
   3. Verify USuspenseCoreWeaponAttributeSet visible
   4. Verify BaseDamage = 42.0, RateOfFire = 650.0, etc.
   ```

2. **Reload with Ammo Test:**
   ```
   1. Equip Weapon_AK74M (uses 5.45x39mm)
   2. Add Ammo_545x39_BP to inventory
   3. Press Reload
   4. Verify AmmoAttributeSet applied
   5. Verify ArmorPenetration = 45.0
   ```

3. **UI Slot Test:**
   ```
   1. Open Equipment screen
   2. Verify 17 slots render correctly
   3. Drag AR to PrimaryWeapon - should accept
   4. Drag AR to Holster - should reject
   5. Drag Pistol to Holster - should accept
   ```

### 8.3 Network Test

```
1. Start dedicated server
2. Connect 2 clients
3. Client1 equips Weapon_AK74M
4. Verify Client2 sees correct weapon visuals
5. Verify server has correct AttributeSet values
6. Verify replication of attribute changes
```

---

## 9. Rollback Strategy

### 9.1 If Issues Arise

1. **Revert to legacy flow:**
   - Keep `WeaponInitialization` fields functional
   - Add flag `bUseLegacyAttributeInit` in Settings
   - Default to legacy until verified

2. **Gradual migration:**
   ```cpp
   if (Settings->bUseSSOTAttributes && !ItemData.WeaponAttributesRowName.IsNone())
   {
       // New SSOT path
       DataManager->GetWeaponAttributes(RowKey, WeaponAttrs);
       WeaponAttributeSet->InitializeFromData(WeaponAttrs);
   }
   else
   {
       // Legacy path
       NewObject<UAttributeSet>(Owner, WeaponInitialization.WeaponAttributeSetClass);
       ApplyInitializationEffect(...);
   }
   ```

### 9.2 Backup Points

- Git branch before each phase
- DataTable backups before import
- Settings backup before modification

---

## 10. Summary

### What We Use

| Component | Purpose | Location |
|-----------|---------|----------|
| **USuspenseCoreDataManager** | SSOT for all item/attribute data | BridgeSystem module |
| **USuspenseCoreSettings** | DataTable references in Project Settings | BridgeSystem module |
| **EventBus** | Decoupled communication | `SuspenseCoreEventBus` |
| **Native GameplayTags** | Compile-time tag validation | `SuspenseCoreEquipmentNativeTags.h` |
| **ServiceProvider** | Dependency injection | `SuspenseCoreServiceProvider` |
| **FSuspenseCoreUnifiedItemData** | Item SSOT structure | `SuspenseCoreItemDataTable.h` |

### What We Add

| Component | Purpose |
|-----------|---------|
| `FSuspenseCoreWeaponAttributeRow` | DataTable row for weapon attributes |
| `FSuspenseCoreAmmoAttributeRow` | DataTable row for ammo attributes |
| `WeaponAttributesDataTable` in Settings | Reference to weapon attributes DT |
| `AmmoAttributesDataTable` in Settings | Reference to ammo attributes DT |
| `WeaponAttributesCache` in DataManager | Runtime cache |
| `GetWeaponAttributes()` method | SSOT lookup |
| `InitializeFromData()` methods | AttributeSet initialization |
| `WeaponAttributesRowName` field | New SSOT reference in UnifiedItemData |

### What We Deprecate (but keep for compatibility)

| Component | Reason |
|-----------|--------|
| `FWeaponInitializationData.WeaponAttributeSetClass` | Direct class reference |
| `FWeaponInitializationData.WeaponInitEffect` | Requires GE per weapon |
| Same for Ammo/Armor | Same reasons |

---

## 11. SSOT Architecture Verification (AAA-Level)

> **Added:** 2026-01-02
> **Status:** VERIFIED

### 11.1 Architecture Overview

The SSOT architecture follows AAA patterns with clear separation of concerns:

| Layer | Purpose | Source |
|-------|---------|--------|
| **ItemDatabase** | Inventory/UI layer (identity, grid, weight, assets) | `SuspenseCoreItemDatabase.json` |
| **SSOT DataTables** | Gameplay/Combat layer (damage, ballistics, attributes) | `*Attributes.json` files |

### 11.2 SSOT RowName References Verified

All `*AttributesRowName` fields are correctly set and reference valid SSOT rows:

| ItemDatabase Item | RowName Field | SSOT DataTable | Status |
|-------------------|---------------|----------------|--------|
| Weapon_AK74M | `WeaponAttributesRowName` | Weapon_AK74M | ✓ |
| Weapon_AK103 | `WeaponAttributesRowName` | Weapon_AK103 | ✓ |
| Weapon_M4A1 | `WeaponAttributesRowName` | Weapon_M4A1 | ✓ |
| Ammo_545x39_PS | `AmmoAttributesRowName` | Ammo_545x39_PS | ✓ |
| Ammo_556x45_M855A1 | `AmmoAttributesRowName` | Ammo_556x45_M855A1 | ✓ |
| Armor_6B13 | `ArmorAttributesRowName` | Armor_6B13 | ✓ |
| Helmet_Altyn | `ArmorAttributesRowName` | Helmet_Altyn | ✓ |

### 11.3 Data Consistency Verified

Data between ItemDatabase and SSOT is consistent where overlap exists:

| Field | ItemDatabase | SSOT | Match |
|-------|--------------|------|-------|
| Weapon Weight | `Weight` | `WeaponWeight` | ✓ |
| Weapon AmmoType | `AmmoType` tag | `Caliber` tag | ✓ |
| Ammo Caliber | `AmmoCaliber` tag | `Caliber` tag | ✓ |
| Fire Mode Default | `DefaultFireMode` | `DefaultFireMode` | ✓ |

### 11.4 Field Purpose Clarification

Fields that appear duplicated serve different purposes (intentional denormalization):

| ItemDatabase Field | Purpose | SSOT Field | Purpose |
|--------------------|---------|------------|---------|
| `AmmoCaliber` | UI/inventory matching (quick lookup) | `Caliber` | Authoritative source for gameplay |
| `WeaponArchetype` | Animation stance selection | `WeaponType` | Weapon category for combat |
| `FireModes` (TArray\<FWeaponFireModeData\>) | Ability bindings with InputID | `FireModes` (TArray\<FGameplayTag\>) | Available mode tags only |
| `Weight` | Inventory encumbrance calculation | `WeaponWeight` | Handling/sway (same value, different semantic) |

### 11.5 Deprecated Fields Status

Legacy initialization structs are properly defaulted:

| Field | Status | Current Value |
|-------|--------|---------------|
| `WeaponInitialization` | DEPRECATED | `"()"` (empty) |
| `AmmoInitialization` | DEPRECATED | `"()"` (empty) |
| `ArmorInitialization` | DEPRECATED | `"()"` (empty) |

The system uses `ShouldUseSSOTInitialization()` to determine data source at runtime.

### 11.6 SSOT DataTable Content Summary

| DataTable | Rows | Key Attributes |
|-----------|------|----------------|
| **WeaponAttributes** | 9 weapons | BaseDamage, RateOfFire, Caliber, FireModes, Ergonomics, Recoil, Durability |
| **AmmoAttributes** | 15 ammo types | BaseDamage, ArmorPenetration, MuzzleVelocity, FragmentationChance, RicochetChance |
| **ArmorAttributes** | 18 armor items | ArmorClass, Durability, SpeedPenalty, TurnSpeedPenalty, ErgonomicsPenalty |

### 11.7 Key Architecture Points

1. **ItemDatabase = UI/Inventory layer**
   - Core Identity: ItemID, DisplayName, Description, Icon
   - Inventory Properties: GridSize, MaxStackSize, Weight, BaseValue
   - Usage Configuration: bIsEquippable, EquipmentSlot, bIsConsumable
   - Visual/Audio Assets: WorldMesh, sounds, VFX
   - SSOT Row References: `*AttributesRowName` fields

2. **SSOT DataTables = Gameplay/Combat layer**
   - All gameplay-affecting attributes (damage, penetration, etc.)
   - Combat stats (rate of fire, reload times)
   - Physical properties (weight, durability)
   - Ballistic properties (velocity, drag, fragmentation)

3. **Runtime Flow**
   ```
   EquipItem(ItemID)
   └── DataManager->GetUnifiedItemData(ItemID)
       └── ItemData.WeaponAttributesRowName → SSOT Lookup
           └── WeaponAttributeSet->InitializeFromData(SSOTRow)
   ```

---

**Document Status:** Ready for Implementation
**Next Step:** Begin Phase 1 - Create DataTable Row Structures

---

## 12. Critical Bugs & Fixes (2026-01-02)

> **Added:** 2026-01-02
> **Status:** FIXED - Document lessons learned to prevent future issues

### 12.1 BUG: Using Deprecated ItemManager Instead of DataManager

**Symptoms:**
```
LogSuspenseCore: Error: InitializeFromItemInstance: Failed to get item data for item 'Weapon_AK74M'
LogSuspenseCore: Error: ConfigureEquipmentActor failed: Invalid ItemData
```

**Root Cause:**
Components still referenced the deprecated `USuspenseCoreItemManager` instead of the new `USuspenseCoreDataManager` (SSOT).

**Files Affected:**
- `SuspenseCoreEquipmentMeshComponent.cpp`
- `SuspenseCoreEquipmentVisualizationService.cpp`
- `SuspenseCoreEquipmentActorFactory.cpp`

**Fix Pattern:**
```cpp
// ❌ WRONG - ItemManager is DEPRECATED!
#include "SuspenseCore/Services/SuspenseCoreItemManager.h"
USuspenseCoreItemManager* IM = GetGameInstance()->GetSubsystem<USuspenseCoreItemManager>();
const FSuspenseCoreUnifiedItemData* ItemData = IM->GetItemData(ItemID);

// ✅ CORRECT - Use DataManager (SSOT)
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
USuspenseCoreDataManager* DM = USuspenseCoreDataManager::Get(GetWorld());
const FSuspenseCoreUnifiedItemData* ItemData = DM->GetUnifiedItemData(ItemID);
```

**Prevention Rule:**
- Search for `ItemManager` in new code - should NOT appear
- Always use `USuspenseCoreDataManager::Get(World)` for item data
- DataManager is the Single Source of Truth (SSOT)

### 12.2 BUG: Using RequestGameplayTag() for EventBus Events

**Symptoms:**
```
LogGameplayTags: Error: Requested Gameplay Tag Equipment.Event.RequestVisualSync was not found
Assertion failed: Tag.IsValid() [File:...SuspenseCoreEquipmentMeshComponent.cpp] [Line:363]
```

**Root Cause:**
Using `FGameplayTag::RequestGameplayTag()` for tags that are NOT registered in `DefaultGameplayTags.ini` or native tags.

**Fix Pattern:**
```cpp
// ❌ WRONG - Tag doesn't exist at runtime, causes ensure crash!
Manager->PublishEvent(
    FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.RequestVisualSync")),
    GetOwner()
);

// ✅ CORRECT - Use native tag from namespace
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"

Manager->PublishEvent(
    SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Visual_RequestSync,
    GetOwner()
);
```

**Native Tags Added for Visual Events:**
```cpp
// SuspenseCoreEquipmentNativeTags.h - Event namespace
namespace SuspenseCoreEquipmentTags::Event
{
    TAG_Equipment_Event_Visual_StateChanged   // "SuspenseCore.Event.Equipment.Visual.StateChanged"
    TAG_Equipment_Event_Visual_RequestSync    // "SuspenseCore.Event.Equipment.Visual.RequestSync"
    TAG_Equipment_Event_Visual_Effect         // "SuspenseCore.Event.Equipment.Visual.Effect"
}
```

**Prevention Rule:**
- ALWAYS use native tag constants for EventBus operations
- If tag doesn't exist in namespace, ADD it to native tags first
- `RequestGameplayTag()` is only safe for runtime-known tags from DataTables

### 12.3 BUG: AttributeSet Not Removed on Unequip

**Symptoms:**
- AttributeSet still present after weapon unequip
- Previous weapon stats affect new weapon
- `showdebug abilitysystem` shows stale attributes

**Root Cause:**
`OnItemInstanceUnequipped()` didn't clean up AttributeSets from Character's ASC.

**Fix Applied in:**
- `SuspenseCoreEquipmentVisualizationService.cpp` - `OnItemInstanceUnequipped()`

**Fix Pattern:**
```cpp
void USuspenseCoreEquipmentVisualizationService::OnItemInstanceUnequipped(
    AActor* OwnerActor,
    FName ItemID,
    FGameplayTag SlotTag)
{
    // 1. Get Character's ASC (AttributeSets live on CHARACTER, not weapon actor)
    ACharacter* Character = Cast<ACharacter>(OwnerActor);
    UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character);

    // 2. Remove AttributeSet by tag
    if (ASC && SlotTag.MatchesTagExact(SuspenseCoreEquipmentTags::Slot::TAG_Equipment_Slot_PrimaryWeapon))
    {
        // Find and remove weapon attribute set
        for (UAttributeSet* AttrSet : ASC->GetSpawnedAttributes_Mutable())
        {
            if (AttrSet && AttrSet->IsA<USuspenseCoreWeaponAttributeSet>())
            {
                ASC->RemoveSpawnedAttribute(AttrSet);
                break;
            }
        }
    }

    // 3. Now destroy the weapon actor
    if (ASuspenseCoreEquipmentActor* EquipActor = FindEquipmentActor(OwnerActor, SlotTag))
    {
        EquipActor->Destroy();
    }
}
```

**Prevention Rule:**
- AttributeSets are added to CHARACTER's ASC, not weapon actor
- Must be removed BEFORE destroying weapon actor
- Use native tags for slot matching

### 12.4 Summary Table

| Bug | Symptom | Root Cause | Fix |
|-----|---------|------------|-----|
| ItemManager usage | "Failed to get item data" | Using deprecated ItemManager | Use DataManager (SSOT) |
| RequestGameplayTag() | "Tag not found" + ensure crash | Tag not registered | Use native tags |
| AttributeSet leak | Stale attributes on unequip | No cleanup in OnUnequipped | Remove AttributeSet before destroy |

### 12.5 Code Review Checklist (New Additions)

When reviewing EquipmentSystem code, verify:

- [ ] **No ItemManager references** - Search for `ItemManager` should return 0 results
- [ ] **All EventBus tags are native** - No `RequestGameplayTag()` for event publishing
- [ ] **AttributeSet cleanup** - `OnUnequip` removes AttributeSets from Character ASC
- [ ] **DataManager usage** - All item data lookups use `USuspenseCoreDataManager::Get()`
- [ ] **Native tag includes** - `#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"`
