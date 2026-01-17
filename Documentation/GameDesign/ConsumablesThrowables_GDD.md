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

## Appendix C: Fragment System Technical Specification

### C.1 Architecture Overview

The fragment system uses a **Hybrid Batched Calculation** approach optimized for multiplayer performance. Instead of spawning 200-300 individual projectile actors, we calculate fragment hits mathematically on the server and replicate only the results.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    GRENADE DETONATION PIPELINE                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  [SERVER ONLY - Authoritative]                                              │
│  ════════════════════════════                                               │
│                                                                             │
│  1. EXPLOSION PHASE (Instant)                                               │
│     ├── SphereOverlapActors(InnerRadius) → Direct blast victims             │
│     ├── ApplyRadialDamage() with falloff                                    │
│     └── ApplyRadialImpulse() for physics                                    │
│                                                                             │
│  2. FRAGMENT PHASE (Batched Calculation)                                    │
│     ├── SphereOverlapActors(FragmentRadius) → Potential victims             │
│     ├── For each victim:                                                    │
│     │   ├── CalculateFragmentHits(distance, density, spread)                │
│     │   ├── LineTraceTest(grenade → victim) for LOS/cover                   │
│     │   ├── CalculateArmorPenetration(fragmentPen, victimArmor)             │
│     │   └── AccumulateDamage(hits * damagePerFrag * penMultiplier)          │
│     └── ApplyAccumulatedDamage() via GAS                                    │
│                                                                             │
│  3. MULTICAST TO CLIENTS                                                    │
│     └── Multicast_OnDetonation(Location, GrenadeType, HitResults[])         │
│                                                                             │
│  [ALL CLIENTS - Cosmetic]                                                   │
│  ═══════════════════════                                                    │
│                                                                             │
│  4. VFX/SFX PHASE                                                           │
│     ├── Spawn Niagara explosion effect                                      │
│     ├── Play explosion sound (3D attenuated)                                │
│     ├── Camera shake (distance-based intensity)                             │
│     ├── Decal spawn (scorch mark)                                           │
│     └── For each HitResult: spawn impact VFX/sound                          │
│                                                                             │
│  5. LOCAL EFFECTS (if local player hit)                                     │
│     ├── Screen shake                                                        │
│     ├── Tinnitus sound effect                                               │
│     └── Post-process (blood, blur)                                          │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### C.2 Fragment Hit Calculation Algorithm

```cpp
/**
 * Calculate how many fragments hit a target based on distance and geometry.
 * This is a MATHEMATICAL MODEL, not actual raycast per fragment.
 *
 * @param Distance - Distance from explosion to target center
 * @param TargetRadius - Approximate target collision radius
 * @param FragmentCount - Total fragments from grenade data
 * @param FragmentRadius - Maximum fragment travel distance
 * @param bHasLOS - Whether target has line of sight to explosion
 * @return Number of fragments that hit the target
 */
int32 CalculateFragmentHits(
    float Distance,
    float TargetRadius,
    int32 FragmentCount,
    float FragmentRadius,
    bool bHasLOS)
{
    // No hits beyond fragment range
    if (Distance > FragmentRadius)
        return 0;

    // Calculate solid angle subtended by target from explosion point
    // SolidAngle = π * r² / d² (approximate for small angles)
    float SolidAngle = PI * FMath::Square(TargetRadius) / FMath::Square(FMath::Max(Distance, 1.0f));

    // Fragments spread uniformly over 4π steradians (full sphere)
    // Expected hits = FragmentCount * (SolidAngle / 4π)
    float FragmentDensity = FragmentCount / (4.0f * PI);
    float ExpectedHits = FragmentDensity * SolidAngle;

    // Distance falloff (fragments lose energy / spread out)
    float DistanceFactor = 1.0f - FMath::Clamp(Distance / FragmentRadius, 0.0f, 1.0f);
    ExpectedHits *= DistanceFactor;

    // Cover reduction (partial LOS = reduced hits)
    if (!bHasLOS)
    {
        ExpectedHits *= 0.1f;  // 90% blocked by cover
    }

    // Add randomness (±30%)
    float Variance = FMath::FRandRange(0.7f, 1.3f);
    ExpectedHits *= Variance;

    return FMath::RoundToInt(FMath::Max(ExpectedHits, 0.0f));
}
```

### C.3 Performance Specifications

| Metric | Target | Notes |
|--------|--------|-------|
| **Max Overlap Actors** | 32 | `SphereOverlapActors` limit |
| **LOS Traces per Detonation** | ≤32 | One per potential victim |
| **Total Traces** | ≤33 | 1 ground + 32 victims |
| **Detonation Time Budget** | <2ms | Server frame time |
| **Network Payload** | <512 bytes | Multicast RPC size |
| **Client VFX Budget** | <1ms | Niagara + sound spawn |

### C.4 Network Replication Strategy

```cpp
// ═══════════════════════════════════════════════════════════════════════
// REPLICATED STRUCTURES (minimal bandwidth)
// ═══════════════════════════════════════════════════════════════════════

/** Compact hit result for replication */
USTRUCT()
struct FReplicatedFragmentHit
{
    UPROPERTY()
    uint8 VictimIndex;      // Index into overlap results (not full actor ref)

    UPROPERTY()
    uint8 FragmentHits;     // 0-255 fragments

    UPROPERTY()
    uint8 HitBoneIndex;     // Limb hit (0=torso, 1=head, etc.)

    // Total: 3 bytes per victim
};

/** Detonation event for multicast */
USTRUCT()
struct FReplicatedDetonation
{
    UPROPERTY()
    FVector_NetQuantize Location;   // 6 bytes (quantized)

    UPROPERTY()
    uint8 GrenadeTypeIndex;         // 1 byte (enum)

    UPROPERTY()
    TArray<FReplicatedFragmentHit> Hits;  // 3 bytes × N victims

    // Typical total: 6 + 1 + (3 × 8 victims) = 31 bytes
};

// ═══════════════════════════════════════════════════════════════════════
// SERVER RPC (damage application)
// ═══════════════════════════════════════════════════════════════════════

UFUNCTION(Server, Reliable)
void Server_RequestDetonation(FVector Location, uint8 GrenadeTypeIndex);

// ═══════════════════════════════════════════════════════════════════════
// MULTICAST RPC (visual effects only)
// ═══════════════════════════════════════════════════════════════════════

UFUNCTION(NetMulticast, Unreliable)  // Unreliable = less bandwidth
void Multicast_OnDetonation(const FReplicatedDetonation& Detonation);
```

### C.5 Line-of-Sight / Cover System

```cpp
/**
 * Check if target has cover from explosion.
 * Uses single optimized trace with collision presets.
 */
bool HasLineOfSight(const FVector& ExplosionLocation, AActor* Target)
{
    FVector TargetLocation = Target->GetActorLocation();

    // Trace from explosion to target center mass
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Target);
    Params.bTraceComplex = false;  // Simple collision only

    // Use dedicated collision channel for grenade LOS
    bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit,
        ExplosionLocation,
        TargetLocation,
        ECC_GrenadeTrace,  // Custom channel, ignores small debris
        Params
    );

    // If we hit something before reaching target = no LOS
    return !bHit;
}

/**
 * Collision channel configuration:
 * - ECC_GrenadeTrace responds to:
 *   - WorldStatic (walls, floors)
 *   - WorldDynamic (doors, destructibles)
 *   - Vehicle
 * - ECC_GrenadeTrace ignores:
 *   - Pawn (handled separately)
 *   - PhysicsBody (small props)
 *   - Projectile
 */
```

### C.6 Armor Penetration Integration

```cpp
/**
 * Calculate damage after armor penetration.
 * Integrates with existing armor system via GAS.
 */
float CalculateFragmentDamage(
    int32 FragmentHits,
    float DamagePerFragment,
    float ArmorPenetration,
    AActor* Victim,
    FName HitBone)
{
    if (FragmentHits <= 0)
        return 0.0f;

    float BaseDamage = FragmentHits * DamagePerFragment;

    // Get victim's armor value for hit location
    UAbilitySystemComponent* VictimASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Victim);
    if (!VictimASC)
        return BaseDamage;  // No armor system = full damage

    // Query armor attribute for hit bone/limb
    float ArmorValue = 0.0f;
    FGameplayAttribute ArmorAttr = GetArmorAttributeForBone(HitBone);
    if (ArmorAttr.IsValid())
    {
        ArmorValue = VictimASC->GetNumericAttribute(ArmorAttr);
    }

    // Penetration calculation (Tarkov-style)
    // If ArmorPen >= ArmorValue: full damage
    // If ArmorPen < ArmorValue: reduced damage
    float PenetrationChance = FMath::Clamp(ArmorPenetration / FMath::Max(ArmorValue, 1.0f), 0.0f, 1.0f);
    float DamageMultiplier = FMath::Lerp(0.1f, 1.0f, PenetrationChance);

    return BaseDamage * DamageMultiplier;
}
```

### C.7 Complete Detonation Implementation

```cpp
void AGrenade::Detonate()
{
    // ═══════════════════════════════════════════════════════════════════
    // SERVER ONLY - All damage calculation is authoritative
    // ═══════════════════════════════════════════════════════════════════
    if (!HasAuthority())
    {
        Server_RequestDetonation(GetActorLocation(), GrenadeTypeIndex);
        return;
    }

    const FGrenadeData& Data = GetGrenadeData();
    FVector Location = GetActorLocation();
    FReplicatedDetonation Detonation;
    Detonation.Location = Location;
    Detonation.GrenadeTypeIndex = GrenadeTypeIndex;

    // ───────────────────────────────────────────────────────────────────
    // PHASE 1: Explosion Damage (radial)
    // ───────────────────────────────────────────────────────────────────
    TArray<AActor*> IgnoredActors;
    IgnoredActors.Add(this);
    IgnoredActors.Add(GetInstigator());  // Optional: don't damage thrower

    UGameplayStatics::ApplyRadialDamageWithFalloff(
        GetWorld(),
        Data.ExplosionDamage,
        Data.ExplosionDamage * 0.1f,  // MinDamage at edge
        Location,
        Data.InnerRadius,
        Data.ExplosionRadius,
        Data.DamageFalloff,
        UDamageType_Explosion::StaticClass(),
        IgnoredActors,
        this,
        GetInstigatorController()
    );

    // ───────────────────────────────────────────────────────────────────
    // PHASE 2: Fragment Damage (batched calculation)
    // ───────────────────────────────────────────────────────────────────
    TArray<FOverlapResult> Overlaps;
    FCollisionShape Sphere = FCollisionShape::MakeSphere(Data.FragmentRadius);

    GetWorld()->OverlapMultiByChannel(
        Overlaps,
        Location,
        FQuat::Identity,
        ECC_Pawn,
        Sphere
    );

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Victim = Overlap.GetActor();
        if (!Victim || Victim == this || !Victim->CanBeDamaged())
            continue;

        APawn* VictimPawn = Cast<APawn>(Victim);
        if (!VictimPawn)
            continue;

        float Distance = FVector::Dist(Location, Victim->GetActorLocation());
        bool bHasLOS = HasLineOfSight(Location, Victim);

        // Calculate fragment hits
        int32 FragmentHits = CalculateFragmentHits(
            Distance,
            VictimPawn->GetSimpleCollisionRadius(),
            Data.FragmentCount,
            Data.FragmentRadius,
            bHasLOS
        );

        if (FragmentHits <= 0)
            continue;

        // Determine hit bone (random weighted by exposure)
        FName HitBone = DetermineHitBone(Location, VictimPawn);

        // Calculate final damage with armor
        float Damage = CalculateFragmentDamage(
            FragmentHits,
            Data.FragmentDamage,
            Data.ArmorPenetration,
            Victim,
            HitBone
        );

        // Apply damage via GAS
        ApplyFragmentDamageToTarget(VictimPawn, Damage, HitBone);

        // Record for replication
        FReplicatedFragmentHit Hit;
        Hit.VictimIndex = Overlaps.IndexOfByKey(Overlap);
        Hit.FragmentHits = FMath::Clamp(FragmentHits, 0, 255);
        Hit.HitBoneIndex = GetBoneIndex(HitBone);
        Detonation.Hits.Add(Hit);
    }

    // ───────────────────────────────────────────────────────────────────
    // PHASE 3: Multicast visual effects
    // ───────────────────────────────────────────────────────────────────
    Multicast_OnDetonation(Detonation);

    // Destroy grenade actor
    Destroy();
}

void AGrenade::Multicast_OnDetonation_Implementation(const FReplicatedDetonation& Detonation)
{
    // ═══════════════════════════════════════════════════════════════════
    // ALL CLIENTS - Cosmetic effects only
    // ═══════════════════════════════════════════════════════════════════

    const FGrenadeData& Data = GetGrenadeData(Detonation.GrenadeTypeIndex);

    // Spawn explosion VFX
    UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        GetWorld(),
        Data.ExplosionVFX,
        Detonation.Location,
        FRotator::ZeroRotator,
        FVector(Data.ExplosionRadius / 10.0f)  // Scale based on radius
    );

    // Play explosion sound
    UGameplayStatics::PlaySoundAtLocation(
        GetWorld(),
        Data.ExplosionSound,
        Detonation.Location,
        1.0f,
        1.0f,
        0.0f,
        Data.SoundAttenuation
    );

    // Camera shake for local player
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PC)
    {
        float Distance = FVector::Dist(Detonation.Location, PC->GetPawn()->GetActorLocation());
        float ShakeIntensity = FMath::Clamp(1.0f - (Distance / Data.ExplosionRadius), 0.0f, 1.0f);

        if (ShakeIntensity > 0.0f)
        {
            PC->ClientStartCameraShake(Data.CameraShakeClass, ShakeIntensity);
        }
    }

    // Spawn ground decal
    UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(
        GetWorld(),
        Data.ScorchDecal,
        FVector(Data.InnerRadius * 100.0f),
        Detonation.Location,
        FRotator(-90.0f, 0.0f, 0.0f),
        30.0f  // Lifetime
    );

    // Local player hit effects
    APawn* LocalPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (LocalPawn)
    {
        for (const FReplicatedFragmentHit& Hit : Detonation.Hits)
        {
            // Check if local player was hit
            // Apply tinnitus, screen effects, etc.
        }
    }
}
```

### C.8 Profiling & Optimization Notes

```cpp
// ═══════════════════════════════════════════════════════════════════════
// PERFORMANCE BEST PRACTICES
// ═══════════════════════════════════════════════════════════════════════

/*
 * 1. SPATIAL QUERIES
 *    - Use OverlapMultiByChannel, NOT OverlapMultiByObjectType
 *    - Limit results with FCollisionQueryParams::MaxTouchCount = 32
 *    - Use dedicated collision channel to skip irrelevant actors
 *
 * 2. LINE TRACES
 *    - bTraceComplex = false (use simple collision)
 *    - Cache collision query params (don't recreate each frame)
 *    - Consider async traces for multiple grenades
 *
 * 3. GAS DAMAGE APPLICATION
 *    - Batch GE applications where possible
 *    - Use FGameplayEffectSpec pooling
 *    - Avoid GetAbilitySystemComponentFromActor in hot paths (cache ASC refs)
 *
 * 4. REPLICATION
 *    - Multicast is Unreliable (cosmetics don't need guaranteed delivery)
 *    - Use quantized vectors (FVector_NetQuantize)
 *    - Compress arrays (uint8 indices instead of full object refs)
 *
 * 5. VFX
 *    - Use Niagara GPU particles
 *    - LOD system for distant explosions
 *    - Pool decal actors
 */

// Stat tracking for profiling
DECLARE_CYCLE_STAT(TEXT("Grenade Detonation"), STAT_GrenadeDetonation, STATGROUP_Grenade);
DECLARE_CYCLE_STAT(TEXT("Fragment Calculation"), STAT_FragmentCalc, STATGROUP_Grenade);
DECLARE_CYCLE_STAT(TEXT("LOS Traces"), STAT_LOSTraces, STATGROUP_Grenade);

void AGrenade::Detonate()
{
    SCOPE_CYCLE_COUNTER(STAT_GrenadeDetonation);
    // ... implementation
}
```

### C.9 Test Scenarios

| Scenario | Expected Result | Performance Target |
|----------|-----------------|-------------------|
| Single grenade, no victims | VFX only | <0.5ms server |
| Single grenade, 8 victims | 8 damage events, 8 LOS traces | <1.5ms server |
| Single grenade, 32 victims (max) | 32 damage events | <2.0ms server |
| 4 simultaneous grenades | All process same frame | <8ms total |
| Grenade behind full cover | No fragment damage | LOS trace blocks |
| Grenade vs armored target | Reduced fragment damage | Armor calc correct |

---

*Document generated based on analysis of existing codebase and JSON data files.*
