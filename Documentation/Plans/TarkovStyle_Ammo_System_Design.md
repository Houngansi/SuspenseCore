# Tarkov-Style Ammo System Design

## Overview

This document describes the design for a realistic ammunition system inspired by Escape from Tarkov.
The system treats ammo as physical inventory items with detailed ballistic properties, supports
magazine-based reloading, and integrates with QuickSlots for fast tactical access.

**Author:** Claude Code
**Date:** 2026-01-02
**Status:** Design Phase
**Related:** `SSOT_AttributeSet_DataTable_Integration.md`

---

## Table of Contents

1. [Core Concepts](#1-core-concepts)
2. [Data Structures](#2-data-structures)
3. [Magazine System](#3-magazine-system)
4. [QuickSlots Integration](#4-quickslots-integration)
5. [Reload Mechanics](#5-reload-mechanics)
6. [GAS Integration](#6-gas-integration)
7. [UI Requirements](#7-ui-requirements)
8. [Implementation Plan](#8-implementation-plan)

---

## 1. Core Concepts

### 1.1 Physical Ammo Model

Unlike simplified FPS games where ammo is just a counter, Tarkov-style ammo:
- Occupies inventory space (grid-based)
- Has physical weight
- Can be different types within same caliber (AP, HP, tracer, subsonic)
- Degrades/can be inspected
- Is tradeable/droppable

### 1.2 Key Differences from Simple Systems

| Feature | Simple System | Tarkov-Style |
|---------|---------------|--------------|
| Ammo Storage | Counter on weapon | Physical items in inventory |
| Magazine | Infinite reserve | Physical item, limited capacity |
| Reload Speed | Fixed time | Depends on magazine swap vs chamber load |
| Ammo Types | One per caliber | Multiple with different stats |
| Mixed Ammo | N/A | Can mix types in magazine |

### 1.3 Terminology

- **Round/Cartridge**: Single ammunition unit
- **Magazine**: Container holding multiple rounds
- **Caliber**: Ammunition size/type compatibility (e.g., 5.56x45mm, 9x19mm)
- **Chamber**: Single round loaded in weapon, ready to fire
- **Tactical Reload**: Swap magazine while one round is chambered
- **Empty Reload**: Insert magazine into empty weapon, chamber first round

---

## 2. Data Structures

### 2.1 Ammo Item Data (extends FSuspenseCoreUnifiedItemData)

```cpp
// Already exists in AmmoAttributes - these are the key fields:
UPROPERTY() FGameplayTag AmmoCaliber;           // Item.Ammo.556x45, Item.Ammo.9x19
UPROPERTY() FGameplayTagContainer CompatibleWeapons;
UPROPERTY() FName AmmoAttributesRowName;        // SSOT link to DT_AmmoAttributes
```

### 2.2 Ammo Stack Instance

```cpp
USTRUCT(BlueprintType)
struct FSuspenseCoreAmmoStackInstance
{
    GENERATED_BODY()

    // Item identity
    UPROPERTY() FName AmmoID;                   // e.g., "Ammo_556x45_M855A1"
    UPROPERTY() FGameplayTag Caliber;           // e.g., Item.Ammo.556x45

    // Stack data
    UPROPERTY() int32 CurrentCount;             // Rounds in this stack
    UPROPERTY() int32 MaxStackSize;             // From DataTable (usually 30-60)

    // Runtime state
    UPROPERTY() FGuid InstanceGuid;             // Unique instance ID
    UPROPERTY() float AverageCondition;         // 0.0-1.0, affects reliability
};
```

### 2.3 Magazine Item Data

```cpp
USTRUCT(BlueprintType)
struct FSuspenseCoreMagazineData : public FTableRowBase
{
    GENERATED_BODY()

    // Identity
    UPROPERTY() FName MagazineID;               // e.g., "Mag_STANAG_30"
    UPROPERTY() FText DisplayName;

    // Compatibility
    UPROPERTY() FGameplayTag Caliber;           // Item.Ammo.556x45
    UPROPERTY() FGameplayTagContainer CompatibleWeapons;

    // Capacity
    UPROPERTY() int32 MaxCapacity = 30;         // Max rounds
    UPROPERTY() int32 LoadPenalty = 0;          // Ergonomics penalty when full

    // Stats
    UPROPERTY() float LoadTimePerRound = 0.5f;  // Seconds to load one round
    UPROPERTY() float UnloadTimePerRound = 0.3f;
    UPROPERTY() float ReloadModifier = 1.0f;    // Multiplier on weapon reload time

    // Reliability
    UPROPERTY() float FeedReliability = 0.999f; // Chance of proper feed per shot
    UPROPERTY() float Durability = 100.0f;

    // Inventory
    UPROPERTY() FIntPoint GridSize = FIntPoint(1, 2);
    UPROPERTY() float Weight = 0.1f;            // Empty weight in kg
    UPROPERTY() float WeightPerRound = 0.012f;  // Additional weight per round
};
```

### 2.4 Magazine Instance (Runtime)

```cpp
USTRUCT(BlueprintType)
struct FSuspenseCoreMagazineInstance
{
    GENERATED_BODY()

    // Base data link
    UPROPERTY() FName MagazineID;
    UPROPERTY() FGuid InstanceGuid;

    // Current contents - ordered array (bottom to top of magazine)
    UPROPERTY() TArray<FSuspenseCoreLoadedRound> LoadedRounds;

    // State
    UPROPERTY() float CurrentDurability;
    UPROPERTY() bool bIsInsertedInWeapon;

    // Helper methods
    int32 GetCurrentCount() const { return LoadedRounds.Num(); }
    bool IsFull() const;
    bool IsEmpty() const { return LoadedRounds.IsEmpty(); }
    FName GetTopRoundType() const;              // Next round to feed
    float GetTotalWeight() const;
};

USTRUCT(BlueprintType)
struct FSuspenseCoreLoadedRound
{
    GENERATED_BODY()

    UPROPERTY() FName AmmoID;                   // Type of this round
    UPROPERTY() float Condition;                // Individual round condition
};
```

---

## 3. Magazine System

### 3.1 Magazine Component

```cpp
UCLASS()
class USuspenseCoreMagazineComponent : public UActorComponent
{
    // Current magazine state
    UPROPERTY(Replicated)
    FSuspenseCoreMagazineInstance CurrentMagazine;

    // Chambered round (separate from magazine)
    UPROPERTY(Replicated)
    FSuspenseCoreLoadedRound ChamberedRound;

    UPROPERTY(Replicated)
    bool bHasChamberedRound;

public:
    // Magazine operations
    UFUNCTION(BlueprintCallable)
    bool InsertMagazine(const FSuspenseCoreMagazineInstance& Magazine);

    UFUNCTION(BlueprintCallable)
    FSuspenseCoreMagazineInstance EjectMagazine();

    UFUNCTION(BlueprintCallable)
    bool ChamberNextRound();                    // Feeds from magazine

    UFUNCTION(BlueprintCallable)
    FSuspenseCoreLoadedRound EjectChamberedRound();

    // Ammo management in magazine
    UFUNCTION(BlueprintCallable)
    bool LoadRound(const FName& AmmoID, float Condition = 1.0f);

    UFUNCTION(BlueprintCallable)
    FSuspenseCoreLoadedRound UnloadTopRound();

    // Queries
    UFUNCTION(BlueprintPure)
    int32 GetMagazineCount() const;

    UFUNCTION(BlueprintPure)
    int32 GetMagazineCapacity() const;

    UFUNCTION(BlueprintPure)
    bool HasMagazine() const;

    UFUNCTION(BlueprintPure)
    bool IsReadyToFire() const { return bHasChamberedRound; }

    UFUNCTION(BlueprintPure)
    FName GetChamberedAmmoType() const;
};
```

### 3.2 Magazine Loading UI Flow

```
┌─────────────────────────────────────────────────────────────┐
│                    MAGAZINE LOADING                         │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐         ┌─────────────────────────────┐   │
│  │  Inventory  │         │      Selected Magazine      │   │
│  │  ─────────  │         │      ─────────────────      │   │
│  │ [5.56 M855] │ ──────► │  ████████████░░░░░░░  23/30 │   │
│  │ [5.56 M855] │  Load   │  ────────────────────       │   │
│  │ [5.56 M995] │         │  Contents (top to bottom):  │   │
│  │ [5.56 Trcr] │         │  1. M855A1                  │   │
│  └─────────────┘         │  2. M855A1                  │   │
│                          │  ...                        │   │
│  [Load All Same Type]    │  23. M855                   │   │
│  [Unload All]            └─────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### 3.3 Mixed Ammo Handling

When firing, the weapon uses the **top round** in the magazine (last loaded):
- Player can strategically load AP rounds on top for initial engagement
- Tracer rounds can be loaded at specific positions (every 5th round)
- Subsonic rounds for suppressed shooting

```cpp
// Example: Strategic magazine loading
// Magazine capacity: 30
// Load order (bottom to top):
//   Rounds 1-25:  M855 (standard)
//   Rounds 26-29: M855A1 (better penetration)
//   Round 30:     M995 (AP - first shot is AP)
```

---

## 4. QuickSlots Integration

### 4.1 QuickSlot System Overview

QuickSlots 1-4 provide fast access to magazines during combat:

```
┌────────────────────────────────────────────────────┐
│  QUICKSLOTS (Bottom of Screen)                     │
├────────────────────────────────────────────────────┤
│                                                    │
│  [1]           [2]           [3]           [4]     │
│  ┌───┐        ┌───┐        ┌───┐        ┌───┐    │
│  │MAG│        │MAG│        │MED│        │GRN│    │
│  │30 │        │ 0 │        │   │        │   │    │
│  └───┘        └───┘        └───┘        └───┘    │
│  STANAG       STANAG       Bandage     Grenade   │
│  30/30        0/30                               │
│                                                    │
└────────────────────────────────────────────────────┘
```

### 4.2 QuickSlot Data Structure

```cpp
USTRUCT(BlueprintType)
struct FSuspenseCoreQuickSlot
{
    GENERATED_BODY()

    UPROPERTY() int32 SlotIndex;                // 0-3 for slots 1-4
    UPROPERTY() FGameplayTag SlotType;          // Equipment.QuickSlot.1, etc.

    // Item reference (can be magazine, consumable, grenade, etc.)
    UPROPERTY() FSuspenseCoreInventoryItemInstance AssignedItem;

    // Quick access state
    UPROPERTY() bool bIsAvailable;              // Has item and can use
    UPROPERTY() float CooldownRemaining;        // If on cooldown
};

UCLASS()
class USuspenseCoreQuickSlotComponent : public UActorComponent
{
    UPROPERTY(Replicated)
    TArray<FSuspenseCoreQuickSlot> QuickSlots;  // Fixed size: 4

public:
    // Assignment
    UFUNCTION(BlueprintCallable)
    bool AssignItemToSlot(int32 SlotIndex, const FSuspenseCoreInventoryItemInstance& Item);

    UFUNCTION(BlueprintCallable)
    void ClearSlot(int32 SlotIndex);

    // Usage
    UFUNCTION(BlueprintCallable)
    bool UseQuickSlot(int32 SlotIndex);         // Triggers use (reload, heal, throw)

    // Magazine-specific
    UFUNCTION(BlueprintCallable)
    bool SwapToMagazineInSlot(int32 SlotIndex); // Fast magazine swap

    // Queries
    UFUNCTION(BlueprintPure)
    FSuspenseCoreQuickSlot GetQuickSlot(int32 SlotIndex) const;

    UFUNCTION(BlueprintPure)
    TArray<FSuspenseCoreQuickSlot> GetMagazineSlots() const; // All slots with mags
};
```

### 4.3 Input Binding

```cpp
// Native GameplayTags for QuickSlot input
UE_DEFINE_GAMEPLAY_TAG(TAG_Input_QuickSlot_1, "Input.Action.QuickSlot.1");
UE_DEFINE_GAMEPLAY_TAG(TAG_Input_QuickSlot_2, "Input.Action.QuickSlot.2");
UE_DEFINE_GAMEPLAY_TAG(TAG_Input_QuickSlot_3, "Input.Action.QuickSlot.3");
UE_DEFINE_GAMEPLAY_TAG(TAG_Input_QuickSlot_4, "Input.Action.QuickSlot.4");

// Enhanced Input Action mapping
// Key 1-4 → QuickSlot.1-4
// Double-tap → Quick use without aiming
// Hold → Open radial menu for that category
```

---

## 5. Reload Mechanics

### 5.1 Reload Types

| Reload Type | Condition | Speed | Animation |
|-------------|-----------|-------|-----------|
| **Tactical** | Magazine inserted, round chambered | Fast | Swap mag, no rack |
| **Empty** | No magazine or empty mag | Medium | Swap mag, rack slide |
| **Emergency** | Any state, drops current mag | Fastest | Drop mag, insert, rack |
| **Chamber Load** | Empty chamber, has mag with rounds | Slow | Rack only |

### 5.2 Reload State Machine

```
                    ┌──────────────┐
                    │    IDLE      │
                    │  Ready/Empty │
                    └──────┬───────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
              ▼            ▼            ▼
        ┌──────────┐ ┌──────────┐ ┌──────────┐
        │ TACTICAL │ │  EMPTY   │ │EMERGENCY │
        │  RELOAD  │ │  RELOAD  │ │  RELOAD  │
        └────┬─────┘ └────┬─────┘ └────┬─────┘
             │            │            │
             │      ┌─────┴─────┐      │
             │      ▼           │      │
             │ ┌─────────┐      │      │
             │ │ RACKING │◄─────┼──────┘
             │ │ CHAMBER │      │
             │ └────┬────┘      │
             │      │           │
             ▼      ▼           │
        ┌─────────────────┐     │
        │     READY       │◄────┘
        │ (Round Chambered)│
        └─────────────────┘
```

### 5.3 Reload Ability (GAS)

```cpp
UCLASS()
class UGA_Reload : public USuspenseCoreGameplayAbility
{
    // Reload type determined on activation
    UPROPERTY()
    EReloadType CurrentReloadType;

    // Timing (from weapon + magazine modifiers)
    UPROPERTY()
    float ReloadDuration;

    // Callbacks
    UFUNCTION()
    void OnReloadMontageNotify(FName NotifyName);

    // Notify points:
    // - "MagOut" - old magazine detached
    // - "MagIn" - new magazine inserted
    // - "RackStart" - begin chambering
    // - "RackEnd" - round chambered
};
```

### 5.4 Reload Time Calculation

```cpp
float CalculateReloadTime(
    const FSuspenseCoreWeaponAttributeRow& WeaponData,
    const FSuspenseCoreMagazineData& MagazineData,
    EReloadType ReloadType,
    float ErgonomicsModifier)
{
    float BaseTime = 0.0f;

    switch (ReloadType)
    {
        case EReloadType::Tactical:
            BaseTime = WeaponData.TacticalReloadTime;
            break;
        case EReloadType::Empty:
            BaseTime = WeaponData.FullReloadTime;
            break;
        case EReloadType::Emergency:
            BaseTime = WeaponData.TacticalReloadTime * 0.8f; // 20% faster but drops mag
            break;
    }

    // Apply magazine modifier
    BaseTime *= MagazineData.ReloadModifier;

    // Apply ergonomics (from attachments, skill, etc.)
    BaseTime *= (2.0f - ErgonomicsModifier); // Lower ergo = slower

    return BaseTime;
}
```

---

## 6. GAS Integration

### 6.1 Ammo-Related Attributes (Already in FSuspenseCoreAmmoAttributeRow)

```cpp
// Ballistics
float BaseDamage;
float ArmorPenetration;
float ArmorDamage;
float FragmentationChance;
float RicochetChance;

// Projectile physics
float MuzzleVelocity;
float BallisticCoefficient;
float ProjectileMass;

// Effects
float BleedChance;
float StunChance;
bool bIsTracer;
bool bIsSubsonic;
```

### 6.2 Weapon Ammo Attributes (Runtime)

```cpp
// In USuspenseCoreWeaponAttributeSet - already exists:
UPROPERTY() FGameplayAttributeData CurrentAmmoInMag;
UPROPERTY() FGameplayAttributeData MagazineSize;
UPROPERTY() FGameplayAttributeData TotalReserveAmmo;

// New attributes to add:
UPROPERTY() FGameplayAttributeData ChamberedRoundDamage;      // From current chambered round
UPROPERTY() FGameplayAttributeData ChamberedRoundPenetration;
```

### 6.3 Ammo Selection Effect

When chambering a round, apply a temporary effect that sets weapon's damage modifiers:

```cpp
// GE_AmmoLoaded - Applied when round is chambered
// Sets damage/penetration based on ammo type
// Duration: Until next round chambered or weapon unequipped

Modifiers:
- Attribute: Weapon.ChamberedRoundDamage
  ModifierOp: Override
  ModifierMagnitude: Calculated from AmmoAttributeRow.BaseDamage

- Attribute: Weapon.ChamberedRoundPenetration
  ModifierOp: Override
  ModifierMagnitude: Calculated from AmmoAttributeRow.ArmorPenetration
```

---

## 7. UI Requirements

### 7.1 HUD Elements

```
┌─────────────────────────────────────────────────────────────────┐
│                         GAMEPLAY HUD                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  [Health/Armor UI]                              [Compass]       │
│                                                                 │
│                                                                 │
│                        [Crosshair]                              │
│                                                                 │
│                                                                 │
│                                                                 │
│  ┌──────────────────┐                    ┌─────────────────┐   │
│  │   AMMO STATUS    │                    │   FIRE MODE     │   │
│  │  ═══════════════ │                    │   [AUTO]        │   │
│  │   30 │ 90        │                    └─────────────────┘   │
│  │  MAG │ RESERVE   │                                          │
│  │  ───────────     │     [1]  [2]  [3]  [4]                   │
│  │  M855A1 ██████   │     30   15   Med  Grn                   │
│  └──────────────────┘     QUICKSLOTS                           │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 7.2 Inventory Magazine Inspection

```
┌─────────────────────────────────────────────────────────────────┐
│                    MAGAZINE INSPECTION                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  STANAG 30-Round Magazine                     Condition: 95%   │
│  ═══════════════════════                                       │
│                                                                 │
│  Contents: 28/30 rounds                                        │
│                                                                 │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │ TOP (Next to fire)                                        │ │
│  │ ┌─┐┌─┐┌─┐┌─┐┌─┐┌─┐┌─┐┌─┐┌─┐┌─┐                           │ │
│  │ │●││●││●││●││●││○││●││●││●││●│  ● M855A1 (22)            │ │
│  │ └─┘└─┘└─┘└─┘└─┘└─┘└─┘└─┘└─┘└─┘  ○ M995 AP (4)            │ │
│  │ ┌─┐┌─┐┌─┐┌─┐┌─┐┌─┐┌─┐┌─┐┌─┐┌─┐  ◐ Tracer (2)             │ │
│  │ │●││●││●││◐││●││●││●││●││◐││●│                           │ │
│  │ └─┘└─┘└─┘└─┘└─┘└─┘└─┘└─┘└─┘└─┘                           │ │
│  │ ┌─┐┌─┐┌─┐┌─┐┌─┐┌─┐┌─┐┌─┐┌─┐┌─┐                           │ │
│  │ │●││●││●││●││●││●││●││●││ ││ │                           │ │
│  │ └─┘└─┘└─┘└─┘└─┘└─┘└─┘└─┘└─┘└─┘                           │ │
│  │ BOTTOM (Last to fire)                                     │ │
│  └───────────────────────────────────────────────────────────┘ │
│                                                                 │
│  [Load Ammo]  [Unload All]  [Assign to QuickSlot]              │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 8. Implementation Plan

### Phase 1: Core Data Structures

**Files to create:**
- `SuspenseCoreMagazineTypes.h` - Magazine data structures
- `SuspenseCoreAmmoStackTypes.h` - Ammo stack structures

**Files to modify:**
- `SuspenseCoreItemDataTable.h` - Add magazine fields

### Phase 2: Magazine Component

**Files to create:**
- `SuspenseCoreMagazineComponent.h/.cpp`
- `SuspenseCoreWeaponAmmoComponent.h/.cpp` (extends existing)

### Phase 3: QuickSlot System

**Files to create:**
- `SuspenseCoreQuickSlotComponent.h/.cpp`
- `SuspenseCoreQuickSlotTypes.h`

**GameplayTags to add:**
```cpp
// Equipment slots
Equipment.QuickSlot.1
Equipment.QuickSlot.2
Equipment.QuickSlot.3
Equipment.QuickSlot.4

// Item categories for QuickSlots
Item.Category.Magazine
Item.Category.Consumable
Item.Category.Throwable
```

### Phase 4: Reload Abilities

**Files to create:**
- `GA_ReloadTactical.h/.cpp`
- `GA_ReloadEmpty.h/.cpp`
- `GA_ReloadEmergency.h/.cpp`
- `GA_ChamberRound.h/.cpp`

### Phase 5: UI Integration

**Widgets to create/modify:**
- `WBP_AmmoStatus` - HUD ammo display
- `WBP_QuickSlotBar` - QuickSlot display
- `WBP_MagazineInspector` - Inventory magazine view
- `WBP_AmmoLoadingUI` - Magazine loading interface

### Phase 6: Testing & Polish

- Test all reload paths
- Verify replication in multiplayer
- Balance reload times and magazine weights
- Polish animations and UI feedback

---

## Appendix A: DataTable Examples

### DT_Magazines

| MagazineID | Caliber | MaxCapacity | ReloadModifier | Weight |
|------------|---------|-------------|----------------|--------|
| Mag_STANAG_30 | 556x45 | 30 | 1.0 | 0.13 |
| Mag_STANAG_60 | 556x45 | 60 | 1.15 | 0.23 |
| Mag_PMAG_40 | 556x45 | 40 | 0.95 | 0.17 |
| Mag_Glock_17 | 9x19 | 17 | 0.9 | 0.07 |
| Mag_Glock_33 | 9x19 | 33 | 1.1 | 0.12 |

### DT_AmmoAttributes (Extended)

| AmmoID | Caliber | Damage | Penetration | Velocity | IsTracer | IsSubsonic |
|--------|---------|--------|-------------|----------|----------|------------|
| Ammo_556_M855 | 556x45 | 40 | 25 | 940 | false | false |
| Ammo_556_M855A1 | 556x45 | 42 | 38 | 945 | false | false |
| Ammo_556_M995 | 556x45 | 35 | 53 | 950 | false | false |
| Ammo_556_Tracer | 556x45 | 38 | 20 | 940 | true | false |
| Ammo_9x19_FMJ | 9x19 | 32 | 18 | 360 | false | false |
| Ammo_9x19_AP | 9x19 | 28 | 30 | 370 | false | false |

---

## Appendix B: Related Files Reference

### Existing Files to Integrate With

| File | Purpose |
|------|---------|
| `SuspenseCoreUnifiedItemData` | Base item data |
| `SuspenseCoreInventoryItemInstance` | Runtime item instance |
| `SuspenseCoreAmmoAttributeSet` | GAS ammo attributes |
| `SuspenseCoreWeaponAttributeSet` | GAS weapon attributes |
| `SuspenseCoreEquipmentSlotComponent` | Equipment management |
| `SuspenseCoreWeaponAmmoComponent` | Existing ammo component |

### GameplayTags Already Defined

```cpp
// From SuspenseCoreEquipmentGameplayTags.h
Equipment.Slot.Primary
Equipment.Slot.Secondary
Equipment.Slot.Sidearm
// ... (17 total slots)

// Need to add for ammo system:
Equipment.QuickSlot.1
Equipment.QuickSlot.2
Equipment.QuickSlot.3
Equipment.QuickSlot.4
```

---

**End of Document**
