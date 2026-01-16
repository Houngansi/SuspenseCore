# Consumables & Throwables - Game Design Document

> **Version:** 1.0
> **Author:** Claude Code
> **Date:** 2026-01-16
> **Status:** DRAFT
> **Related Docs:**
> - `ItemUseSystem_Pipeline.md` (Implementation architecture)
> - `SuspenseCoreConsumableAttributes.json` (Data source)
> - `SuspenseCoreThrowableAttributes.json` (Data source)

---

## Executive Summary

This document defines the game design for consumable items (medical supplies, stimulants) and throwable items (grenades) in SuspenseCore. It covers mechanics, balancing, status effects, and integration with the GAS-based ItemUseSystem.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Medical Items](#2-medical-items)
3. [Throwable Items](#3-throwable-items)
4. [Status Effects](#4-status-effects)
5. [GameplayTags Taxonomy](#5-gameplaytags-taxonomy)
6. [GAS Integration](#6-gas-integration)
7. [UI/UX Requirements](#7-uiux-requirements)
8. [Balance Tables](#8-balance-tables)

---

## 1. Overview

### Design Philosophy

Following Escape from Tarkov's realistic survival mechanics:
- Medical items require time to use and can be interrupted
- Different items heal different conditions (bleeds, fractures, HP)
- Throwables have distinct tactical roles
- All item use is channeled (time-based) with cancellation support

### System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    ITEM USE FLOW                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  [QuickSlot Key 4-7]                                           │
│         │                                                       │
│         ▼                                                       │
│  GA_QuickSlotUse (GAS Ability)                                 │
│         │                                                       │
│         ▼                                                       │
│  ItemUseService.UseItem()  ─────► FindHandler()                │
│         │                           │                           │
│         │    ┌──────────────────────┼──────────────────────┐   │
│         │    │                      │                      │   │
│         │    ▼                      ▼                      ▼   │
│         │  MagazineSwap        MedicalUse           GrenadeUse │
│         │  Handler             Handler              Handler    │
│         │    │                      │                      │   │
│         └────┼──────────────────────┼──────────────────────┘   │
│              │                      │                          │
│              ▼                      ▼                          │
│         [Instant]            [Time-Based]                      │
│              │                      │                          │
│              │                      ├── Apply InProgress GE    │
│              │                      ├── Show Progress UI       │
│              │                      └── Wait Duration          │
│              │                             │                   │
│              ▼                             ▼                   │
│         Complete               OnOperationComplete             │
│              │                      │                          │
│              └──────────────────────┴──► EventBus.Publish     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. Medical Items

### 2.1 Categories

| Category | Tag | Description |
|----------|-----|-------------|
| **Bandage** | `Item.Consumable.Bandage` | Stops light bleeding only |
| **Medkit** | `Item.Consumable.Medkit` | Heals HP + stops all bleeding |
| **Painkiller** | `Item.Consumable.Painkiller` | Temporary pain suppression |
| **Stimulant** | `Item.Consumable.Stimulant` | Temporary stat boost |
| **Splint** | `Item.Consumable.Splint` | Fixes fractures |
| **Surgery** | `Item.Consumable.Surgery` | Heals all conditions |

### 2.2 Medical Item Specifications

#### Bandage
```
Item: Medical_Bandage
├── UseTime: 2.0s
├── MaxUses: 1 (single-use)
├── HealAmount: 0
├── Effects:
│   └── StopBleeding (Light only)
└── Tags: Consumable.Effect.StopBleeding
```

#### IFAK (Individual First Aid Kit)
```
Item: Medical_IFAK
├── UseTime: 5.0s
├── MaxUses: 3
├── HealAmount: 150 HP (total)
├── HealRate: 30 HP/use
├── Effects:
│   ├── Heal
│   └── StopBleeding (Light + Heavy)
└── Tags: Consumable.Effect.Heal, Consumable.Effect.StopBleeding
```

#### Salewa
```
Item: Medical_Salewa
├── UseTime: 4.0s
├── MaxUses: 5
├── HealAmount: 400 HP (total)
├── HealRate: 50 HP/use
├── Effects:
│   ├── Heal
│   └── StopBleeding (Light + Heavy)
└── Tags: Consumable.Effect.Heal, Consumable.Effect.StopBleeding
```

#### Car Medkit
```
Item: Medical_CarMedkit
├── UseTime: 6.0s
├── MaxUses: 5
├── HealAmount: 220 HP (total)
├── HealRate: 25 HP/use
├── Effects:
│   ├── Heal
│   └── StopBleeding (Light + Heavy)
└── Tags: Consumable.Effect.Heal, Consumable.Effect.StopBleeding
```

#### Grizzly (Surgery Kit)
```
Item: Medical_Grizzly
├── UseTime: 7.0s
├── MaxUses: 18
├── HealAmount: 1800 HP (total)
├── HealRate: 75 HP/use
├── Effects:
│   ├── Heal
│   ├── StopBleeding (All)
│   └── FixFracture
└── Tags: Consumable.Effect.Heal, Consumable.Effect.StopBleeding, Consumable.Effect.FixFracture
```

#### Splint
```
Item: Medical_Splint
├── UseTime: 5.0s
├── MaxUses: 1 (single-use)
├── HealAmount: 0
├── Effects:
│   └── FixFracture
└── Tags: Consumable.Effect.FixFracture
```

#### Morphine (Painkiller)
```
Item: Medical_Morphine
├── UseTime: 2.5s
├── MaxUses: 1 (single-use)
├── HealAmount: 0
├── Effects:
│   ├── Painkiller (240s duration)
│   ├── StaminaRestore: +30
│   ├── HydrationCost: -10
│   └── EnergyCost: -15
└── Tags: Consumable.Effect.Painkiller
```

### 2.3 Limb-Based Healing (Future)

> **NOTE:** Current implementation heals total HP. Future versions may implement limb-based health:

```
Character Health Model:
├── Head: 35 HP
├── Thorax: 85 HP
├── Stomach: 70 HP
├── Left Arm: 60 HP
├── Right Arm: 60 HP
├── Left Leg: 65 HP
└── Right Leg: 65 HP

Total: 440 HP base
```

Medical items would target specific limbs or require selection:
- Bandages: Apply to specific bleeding limb
- Medkits: Heal selected limb first, overflow to others
- Surgical kits: Required for destroyed limbs (0 HP)

---

## 3. Throwable Items

### 3.1 Categories

| Category | Tag | Primary Effect |
|----------|-----|----------------|
| **Fragmentation** | `Item.Throwable.Frag` | Explosive damage + fragments |
| **Smoke** | `Item.Throwable.Smoke` | Visual obscuration |
| **Flashbang** | `Item.Throwable.Flash` | Blind + Stun |
| **Incendiary** | `Item.Throwable.Incendiary` | Fire damage over time |
| **Impact** | `Item.Throwable.VOG` | Explodes on contact |

### 3.2 Throwable Item Specifications

#### F-1 Fragmentation Grenade
```
Item: Throwable_F1
├── Type: Frag
├── FuseTime: 3.5s
├── ThrowForce: 1200
├── ThrowArc: 45°
├── Explosion:
│   ├── Damage: 120
│   ├── Radius: 20m
│   ├── InnerRadius: 5m (full damage)
│   └── DamageFalloff: 0.8
├── Fragments:
│   ├── Count: 300
│   ├── Damage: 25 per fragment
│   ├── Spread: 360°
│   └── ArmorPen: 15
└── Physics:
    ├── BounceCount: 2
    └── BounceFriction: 0.5
```

#### RGD-5 Fragmentation Grenade
```
Item: Throwable_RGD5
├── Type: Frag
├── FuseTime: 3.5s
├── ThrowForce: 1350 (lighter, throws farther)
├── ThrowArc: 45°
├── Explosion:
│   ├── Damage: 100
│   ├── Radius: 15m
│   ├── InnerRadius: 4m
│   └── DamageFalloff: 0.75
├── Fragments:
│   ├── Count: 250
│   ├── Damage: 22 per fragment
│   └── ArmorPen: 12
└── Physics:
    ├── BounceCount: 3
    └── BounceFriction: 0.45
```

#### M67 Fragmentation Grenade
```
Item: Throwable_M67
├── Type: Frag
├── FuseTime: 4.0s (longer fuse)
├── ThrowForce: 1300
├── ThrowArc: 45°
├── Explosion:
│   ├── Damage: 110
│   ├── Radius: 18m
│   ├── InnerRadius: 4.5m
│   └── DamageFalloff: 0.78
├── Fragments:
│   ├── Count: 275
│   ├── Damage: 24 per fragment
│   └── ArmorPen: 14
└── Physics:
    ├── BounceCount: 2
    └── BounceFriction: 0.5
```

#### M18 Smoke Grenade
```
Item: Throwable_M18_Smoke
├── Type: Smoke
├── FuseTime: 2.0s
├── ThrowForce: 1100
├── ThrowArc: 40°
├── Smoke:
│   ├── Duration: 45s
│   └── Radius: 12m
└── Physics:
    ├── BounceCount: 1
    └── BounceFriction: 0.7
```

#### M84 Flashbang
```
Item: Throwable_M84_Flashbang
├── Type: Flashbang
├── FuseTime: 1.5s (short fuse!)
├── ThrowForce: 1250
├── ThrowArc: 45°
├── Effects:
│   ├── BlindDuration: 5.0s
│   ├── StunDuration: 3.0s
│   └── MinorDamage: 5
├── Radius: 8m (effect range)
│   └── InnerRadius: 3m (full effect)
└── Physics:
    ├── BounceCount: 2
    └── BounceFriction: 0.4
```

#### VOG-25 (Impact Grenade)
```
Item: Throwable_VOG25
├── Type: Impact (explodes on contact)
├── FuseTime: 0.0s (INSTANT)
├── ThrowForce: 0 (launched from GL)
├── Explosion:
│   ├── Damage: 85
│   ├── Radius: 10m
│   ├── InnerRadius: 3m
│   └── DamageFalloff: 0.85
├── Fragments:
│   ├── Count: 150
│   ├── Damage: 20 per fragment
│   └── ArmorPen: 18
└── Effects:
    └── StunDuration: 0.5s
```

### 3.3 Grenade Throw Mechanics

```
THROW PHASES:
1. [Prepare] - Pull pin, ready throw (0.5s default)
   └── Can be cancelled (pin goes back)

2. [Cook] - Hold to reduce fuse time (optional, risky)
   └── If fuse expires while holding = self-damage!

3. [Throw] - Release to throw
   ├── Force = ThrowForce * (1.0 + MovementPenalty)
   ├── Arc = ThrowArc degrees
   └── Inherits player velocity

4. [Flight] - Physics simulation
   ├── Gravity
   ├── Air resistance
   └── Bounce on surfaces

5. [Detonate] - After FuseTime (or on impact for VOG)
   ├── Apply explosion damage
   ├── Spawn fragments
   └── Apply status effects
```

### 3.4 Damage Calculation

```cpp
// Explosion damage with distance falloff
float CalculateExplosionDamage(float Distance)
{
    if (Distance <= InnerRadius)
        return ExplosionDamage; // Full damage

    if (Distance > ExplosionRadius)
        return 0.0f; // No damage

    // Linear falloff from InnerRadius to ExplosionRadius
    float T = (Distance - InnerRadius) / (ExplosionRadius - InnerRadius);
    return ExplosionDamage * (1.0f - T * DamageFalloff);
}

// Fragment damage (per fragment that hits)
float FragmentDamage = BaseDamage * (1.0f - ArmorReduction);
```

---

## 4. Status Effects

### 4.1 Negative Status Effects

| Effect | Source | Duration | Symptom |
|--------|--------|----------|---------|
| **LightBleed** | Bullets, fragments | Until healed | -2 HP/s, blood screen |
| **HeavyBleed** | Armor pen hits | Until healed | -5 HP/s, heavy blood screen |
| **Fracture** | Fall, explosion | Until healed | Limb unusable, -50% move if leg |
| **Pain** | Any damage | 30s | Screen blur, -10% accuracy |
| **Blind** | Flashbang | 5s | White screen, no vision |
| **Stun** | Flashbang | 3s | Cannot act |
| **Tremor** | Stimulant withdrawal | 60s | Weapon sway |

### 4.2 Positive Status Effects (from Consumables)

| Effect | Source | Duration | Benefit |
|--------|--------|----------|---------|
| **Painkiller** | Morphine | 240s | Ignore Pain, can run on broken legs |
| **Regeneration** | MULE stim | 60s | +3 HP/s |
| **MaxStamina** | SJ6 stim | 120s | +50 max stamina |
| **Strength** | SJ1 stim | 120s | +20% melee damage |

### 4.3 GAS Tags for Status Effects

```
State.Health.Bleeding.Light
State.Health.Bleeding.Heavy
State.Health.Fracture
State.Health.Pain
State.Debuff.Blind
State.Debuff.Stun
State.Debuff.Tremor
State.Buff.Painkiller
State.Buff.Regeneration
```

---

## 5. GameplayTags Taxonomy

### 5.1 Item Type Tags

```
Item
├── Category
│   ├── Medical
│   ├── Grenade
│   ├── Magazine
│   └── Ammo
├── Consumable
│   ├── Bandage
│   ├── Medkit
│   ├── Painkiller
│   ├── Stimulant
│   ├── Splint
│   └── Surgery
└── Throwable
    ├── Frag
    ├── Smoke
    ├── Flash
    ├── Incendiary
    └── VOG
```

### 5.2 Effect Tags

```
Consumable.Effect
├── Heal
├── StopBleeding
├── FixFracture
├── Painkiller
├── Stamina
└── Strength

Throwable.Effect
├── Explosion
├── Fragment
├── Smoke
├── Blind
├── Stun
└── Fire
```

### 5.3 ItemUse System Tags (existing)

```
ItemUse.Handler
├── AmmoToMagazine
├── MagazineSwap
├── Medical
└── Grenade

ItemUse.Event
├── Started
├── Completed
├── Cancelled
└── Failed

State.ItemUse.InProgress
```

---

## 6. GAS Integration

### 6.1 Handler Selection Flow

The `ItemUseService` routes requests to handlers based on item tags:

```
Request → FindHandler() iterates (sorted by priority):
  1. MagazineSwapHandler (HIGH) - checks IsMagazine()
  2. GrenadeHandler (HIGH) - checks Item.Throwable tags
  3. MedicalHandler (NORMAL) - checks Item.Medical/Item.Consumable tags
  4. AmmoToMagazineHandler (NORMAL) - checks Item.Ammo → Magazine
```

### 6.2 Required GameplayEffects

| Effect | Purpose | Duration |
|--------|---------|----------|
| `GE_ItemUse_InProgress` | Block other uses, show UI | SetByCaller |
| `GE_ItemUse_Cooldown` | Post-use delay | SetByCaller |
| `GE_Medical_Heal` | Apply healing | Instant |
| `GE_Medical_StopBleed` | Remove bleed tags | Instant |
| `GE_Medical_Painkiller` | Apply painkiller buff | Duration |
| `GE_Grenade_Blind` | Apply blind debuff | Duration |
| `GE_Grenade_Stun` | Apply stun debuff | Duration |

### 6.3 Ability Activation

```cpp
// GA_QuickSlotUse builds request from slot data
FSuspenseCoreItemUseRequest Request = BuildItemUseRequest(ActorInfo);

// ItemUseService finds handler by item tags
ISuspenseCoreItemUseHandler* Handler = FindHandler(Request);

// Handler executes and returns duration
FSuspenseCoreItemUseResponse Response = Handler->Execute(Request, OwnerActor);

// For time-based operations, GA_ItemUse manages timer
if (Response.IsInProgress())
{
    ApplyInProgressEffect(Response.Duration);
    StartDurationTimer(Response.Duration);
}
```

---

## 7. UI/UX Requirements

### 7.1 Progress Indicator

When using consumables/preparing grenades:
- Circular progress bar around crosshair
- Item icon in center
- Time remaining text
- Cancel hint (press key again or move)

### 7.2 QuickSlot Display

```
┌─────────────────────────────────────────┐
│  [4]      [5]      [6]      [7]        │
│  ┌───┐    ┌───┐    ┌───┐    ┌───┐      │
│  │ M │    │ B │    │ G │    │ F │      │
│  │ 3 │    │ 2 │    │ 5 │    │ 2 │      │
│  └───┘    └───┘    └───┘    └───┘      │
│  IFAK    Bandage  Grenade  Flashbang   │
│  [===]                                  │
│  ↑ Uses remaining                      │
└─────────────────────────────────────────┘
```

### 7.3 Sound Design Requirements

| Action | Sound |
|--------|-------|
| Start medical use | Packaging rustle, cap opening |
| Complete medical use | Relief sigh, item put away |
| Cancel medical use | Item fumbled/dropped |
| Grenade pin pull | Metal click |
| Grenade throw | Whoosh |
| Grenade bounce | Metallic clink |
| Explosion | BOOM (varies by type) |
| Flashbang | High-pitched ringing |

---

## 8. Balance Tables

### 8.1 Medical Items Comparison

| Item | UseTime | HP/Use | Total HP | Uses | Cost Tier |
|------|---------|--------|----------|------|-----------|
| Bandage | 2.0s | 0 | 0 | 1 | Cheap |
| IFAK | 5.0s | 50 | 150 | 3 | Medium |
| Salewa | 4.0s | 80 | 400 | 5 | Medium |
| Car Medkit | 6.0s | 44 | 220 | 5 | Cheap |
| Grizzly | 7.0s | 100 | 1800 | 18 | Expensive |
| Splint | 5.0s | 0 | 0 | 1 | Cheap |
| Morphine | 2.5s | 0 | 0 | 1 | Medium |

### 8.2 Grenades Comparison

| Grenade | Fuse | Damage | Radius | Frags | Use Case |
|---------|------|--------|--------|-------|----------|
| F-1 | 3.5s | 120 | 20m | 300 | Room clearing |
| RGD-5 | 3.5s | 100 | 15m | 250 | Long-range throw |
| M67 | 4.0s | 110 | 18m | 275 | Balanced |
| M18 | 2.0s | 0 | 12m | 0 | Cover/escape |
| M84 | 1.5s | 5 | 8m | 0 | Breach entry |
| VOG-25 | 0s | 85 | 10m | 150 | Direct fire |

### 8.3 Time-to-Effect Analysis

```
Scenario: Player takes heavy damage, has light bleed

Option A: IFAK (5.0s)
  → Heals 50 HP + stops bleed
  → Safe if behind cover

Option B: Bandage (2.0s) + Medkit (5.0s)
  → Stop bleed first (2s), then heal (5s)
  → Total: 7.0s but bleed stops faster

Option C: Morphine (2.5s) + Sprint to safety
  → Painkiller, run, heal later
  → Risky but mobile
```

---

## Appendix A: Implementation Checklist

### Phase 5 Tasks

- [ ] Fix `MagazineSwapHandler::CanHandle()` - должен проверять `IsMagazine()`
- [ ] Verify `MedicalUseHandler` receives correct item tags from DataManager
- [ ] Verify `GrenadeHandler` receives correct item tags from DataManager
- [ ] Test QuickSlot 4-7 with medical items
- [ ] Test QuickSlot 4-7 with grenades
- [ ] Create GameplayEffects for medical effects (GE_Medical_Heal, etc.)
- [ ] Create GameplayEffects for grenade effects (GE_Grenade_Blind, etc.)
- [ ] Implement progress UI widget
- [ ] Add sound effects

### Known Issues

**BUG: MagazineSwapHandler catches all QuickSlot requests**

Location: `SuspenseCoreMagazineSwapHandler.cpp:75-96`

```cpp
bool USuspenseCoreMagazineSwapHandler::CanHandle(const FSuspenseCoreItemUseRequest& Request) const
{
    // BUG: Returns true for ANY QuickSlot item, not just magazines!
    // Should check: if (!Request.SourceItem.IsMagazine()) return false;
    if (Request.Context != ESuspenseCoreItemUseContext::QuickSlot)
        return false;
    if (!Request.SourceItem.IsValid())
        return false;
    if (Request.QuickSlotIndex < 0 || Request.QuickSlotIndex > 3)
        return false;
    return true;  // ← This catches medical/grenade items too!
}
```

**FIX:** Add magazine check before returning true:
```cpp
// Check if item is actually a magazine
if (!Request.SourceItem.IsMagazine())
    return false;
```

---

## Appendix B: JSON Schema Reference

### ConsumableAttributes Schema
```json
{
  "ConsumableID": "string (FName)",
  "ConsumableName": "FText (localized)",
  "ConsumableType": "GameplayTag",
  "HealAmount": "float (total HP across all uses)",
  "HealRate": "float (HP per use)",
  "UseTime": "float (seconds)",
  "MaxUses": "int32",
  "bCanHealHeavyBleed": "bool",
  "bCanHealLightBleed": "bool",
  "bCanHealFracture": "bool",
  "PainkillerDuration": "float (seconds)",
  "StaminaRestore": "float",
  "HydrationCost": "float (negative = drains)",
  "EnergyCost": "float (negative = drains)",
  "EffectTags": "GameplayTagContainer"
}
```

### ThrowableAttributes Schema
```json
{
  "ThrowableID": "string (FName)",
  "ThrowableName": "FText (localized)",
  "ThrowableType": "GameplayTag",
  "ExplosionDamage": "float",
  "ExplosionRadius": "float (meters)",
  "InnerRadius": "float (full damage zone)",
  "DamageFalloff": "float (0-1)",
  "FragmentCount": "int32",
  "FragmentDamage": "float (per fragment)",
  "FragmentSpread": "float (degrees)",
  "FuseTime": "float (seconds)",
  "ThrowForce": "float",
  "ThrowArc": "float (degrees)",
  "BounceCount": "int32",
  "BounceFriction": "float (0-1)",
  "ArmorPenetration": "float",
  "StunDuration": "float (seconds)",
  "BlindDuration": "float (seconds)",
  "SmokeDuration": "float (seconds)",
  "SmokeRadius": "float (meters)",
  "IncendiaryDamage": "float",
  "IncendiaryDuration": "float (seconds)"
}
```

---

*Document generated based on analysis of existing codebase and JSON data files.*
