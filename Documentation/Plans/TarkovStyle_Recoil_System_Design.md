# Tarkov-Style Recoil System Design

## Overview

This document describes the design for a realistic weapon recoil system inspired by Escape from Tarkov.
The current implementation is a simplified linear conversion; this plan outlines the full Tarkov-style
system with convergence, skill-based control, and attachment modifiers.

**Author:** Claude Code
**Date:** 2026-01-14
**Status:** All Phases Complete (Phase 1-6)
**Related:**
- `TarkovStyle_Ammo_System_Design.md` - Ammo/Magazine system
- `SSOT_AttributeSet_DataTable_Integration.md` - Data architecture
- `SuspenseCoreGASAttributeRows.h` - SSOT structures

---

## Table of Contents

1. [Current Implementation](#1-current-implementation)
2. [Tarkov Recoil Analysis](#2-tarkov-recoil-analysis)
3. [Gap Analysis](#3-gap-analysis)
4. [Target Architecture](#4-target-architecture)
5. [SSOT Data Structures](#5-ssot-data-structures)
6. [Implementation Plan](#6-implementation-plan)
7. [Technical Specifications](#7-technical-specifications)
8. [References](#8-references)

---

## 1. Current Implementation

### 1.1 What We Have (Phase 1)

**File:** `Source/GAS/Private/SuspenseCore/Abilities/Base/SuspenseCoreBaseFireAbility.cpp:604-717`

```cpp
// Current formula:
FinalRecoil = BaseRecoil × AmmoModifier × PointsToDegrees × ProgressiveMultiplier × ADSMultiplier

// Where:
// - BaseRecoil: From FSuspenseCoreWeaponAttributeRow (0-500 points)
// - AmmoModifier: From FSuspenseCoreAmmoAttributeRow (0.5-2.0)
// - PointsToDegrees: 0.002 (converts points to camera degrees)
// - ProgressiveMultiplier: 1.0 + (ShotCount-1) × 0.2, max 3.0
// - ADSMultiplier: 0.5 when aiming, 1.0 otherwise
```

### 1.2 Current SSOT Values

| Attribute | Location | Range | Example |
|-----------|----------|-------|---------|
| VerticalRecoil | WeaponAttributeRow | 0-500 | 145 (AK-74M) |
| HorizontalRecoil | WeaponAttributeRow | 0-500 | 280 (default) |
| RecoilModifier | AmmoAttributeRow | 0.5-2.0 | 1.0 (standard) |
| Ergonomics | WeaponAttributeRow | 1-100 | 42 (default) |

### 1.3 Current Flow Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                 CURRENT RECOIL CALCULATION                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  [SHOT FIRED]                                                   │
│       │                                                         │
│       ▼                                                         │
│  ┌─────────────────┐      ┌─────────────────┐                  │
│  │ WeaponAttrs     │      │ AmmoAttrs       │                  │
│  │ VerticalRecoil  │──┬───│ RecoilModifier  │                  │
│  │ HorizontalRecoil│  │   └─────────────────┘                  │
│  └─────────────────┘  │                                         │
│                       ▼                                         │
│               ┌───────────────┐                                 │
│               │ × 0.002       │  ← Linear conversion            │
│               │ × AmmoMod     │                                 │
│               │ × Progressive │                                 │
│               │ × ADS         │                                 │
│               └───────┬───────┘                                 │
│                       │                                         │
│                       ▼                                         │
│               ┌───────────────┐                                 │
│               │ AddPitchInput │  ← Direct camera rotation       │
│               │ AddYawInput   │  ← NO convergence!              │
│               └───────────────┘                                 │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 1.4 Problems with Current Implementation

| Problem | Impact | Priority |
|---------|--------|----------|
| No convergence | Camera stays rotated after shot | HIGH |
| No ergonomics influence | All weapons feel same | MEDIUM |
| No attachment modifiers | Mods don't affect recoil | HIGH |
| No skill-based control | No progression feeling | MEDIUM |
| Linear conversion | Doesn't match Tarkov feel | LOW |
| No separate visual/aim recoil | Less realistic | LOW |

---

## 2. Tarkov Recoil Analysis

### 2.1 Tarkov Recoil Components

Escape from Tarkov uses a complex multi-layered recoil system:

```
┌─────────────────────────────────────────────────────────────────┐
│                    TARKOV RECOIL MODEL                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                   VISUAL RECOIL                          │   │
│  │  - Camera shake                                          │   │
│  │  - Weapon model kick animation                           │   │
│  │  - Purely cosmetic, doesn't affect aim                   │   │
│  └─────────────────────────────────────────────────────────┘   │
│                           +                                     │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                    AIM RECOIL                            │   │
│  │  - Actual crosshair/point of impact movement             │   │
│  │  - Vertical impulse (main)                               │   │
│  │  - Horizontal deviation (random)                         │   │
│  │  - AUTO-CONVERGENCE back to original aim point           │   │
│  └─────────────────────────────────────────────────────────┘   │
│                           +                                     │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                  CAMERA RECOIL                           │   │
│  │  - Player view rotation (what player sees)               │   │
│  │  - Compensated by "Recoil Control" skill                 │   │
│  │  - Higher skill = less camera movement                   │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Tarkov Weapon Stats

Real Tarkov weapon values (from game data):

| Weapon | Recoil Sum | Ergonomics | Convergence | Vertical | Horizontal |
|--------|------------|------------|-------------|----------|------------|
| MP5 | 52 | 70 | Fast | Low | Low |
| AK-74M | 145 | 42 | Medium | Medium | Medium |
| M4A1 | 165 | 62 | Fast | Medium | Low |
| AKM | 280 | 38 | Slow | High | High |
| SA-58 | 350 | 35 | Very Slow | Very High | Medium |
| RSASS | 105 | 55 | Fast | Medium | Very Low |

### 2.3 Tarkov Modifiers

| Modifier Type | Examples | Effect on Recoil |
|---------------|----------|------------------|
| **Muzzle Devices** | PBS-1 Suppressor, DTK-1 | -5% to -25% |
| **Stocks** | Zhukov-S, MOE | -10% to -20% |
| **Grips** | RK-1, Fortis Shift | -2% to -8% |
| **Handguards** | SAG MK1, VS-24 | -3% to -10% |
| **Buffer Tubes** | Advanced tube | -5% to -10% |
| **Skill: Recoil Control** | Level 0-51 | Up to -25% camera recoil |
| **Skill: Assault Rifles** | Level 0-51 | Up to -10% |

### 2.4 Convergence Mechanic

The KEY Tarkov feature: camera auto-returns to original aim point.

```
TIME →
     ┌─────────────────────────────────────────────────────────┐
     │                                                         │
     │  Aim Point    ●────────●────────●────────●────────●    │
     │               │        │        │        │        │    │
     │               │        │        │        │        │    │
     │  Camera       │  ╱╲    │   ╱╲   │  ╱╲    │   ╱╲   │    │
     │  Position     │ ╱  ╲   │  ╱  ╲  │ ╱  ╲   │  ╱  ╲  │    │
     │               │╱    ╲──│─╱    ╲─│╱    ╲──│─╱    ╲─│    │
     │                                                         │
     │               Shot1   Shot2   Shot3   Shot4   Shot5    │
     │                                                         │
     │  Legend:                                                │
     │  ●──● = Aim point (stays mostly stable)                │
     │  /\ = Camera kick and return (convergence)             │
     │                                                         │
     └─────────────────────────────────────────────────────────┘
```

**Convergence Speed** depends on:
- Base weapon convergence stat
- Ergonomics (higher = faster return)
- Attachments (grips especially)
- Character's "Recoil Control" skill

---

## 3. Gap Analysis

### 3.1 Missing Components

| Component | Tarkov Has | We Have | Priority |
|-----------|------------|---------|----------|
| **Convergence System** | Auto-return to aim | None | P0 |
| **Attachment Modifiers SSOT** | Per-attachment stats | Sockets only | P0 |
| **Ergonomics → Recoil Link** | Affects recovery | Not connected | P1 |
| **Visual vs Aim Recoil** | Separate systems | Combined | P2 |
| **Recoil Control Skill** | Character skill | No skill system | P2 |
| **Per-weapon Convergence** | Stat per weapon | Not in SSOT | P1 |
| **Recoil Patterns** | Semi-predictable | Pure random | P3 |

### 3.2 SSOT Gaps

**Missing from `FSuspenseCoreWeaponAttributeRow`:**
```cpp
// Convergence (auto-return to aim)
float ConvergenceSpeed = 5.0f;     // Degrees per second of recovery
float ConvergenceDelay = 0.1f;     // Delay before convergence starts

// Recoil pattern
float RecoilAngleBias = 0.0f;      // -1.0 (left) to 1.0 (right) tendency
float RecoilRandomness = 0.3f;     // How random vs predictable (0-1)
```

**Missing structure: `FSuspenseCoreAttachmentAttributeRow`:**
```cpp
USTRUCT(BlueprintType)
struct FSuspenseCoreAttachmentAttributeRow : public FTableRowBase
{
    // Identity
    FName AttachmentID;
    FText DisplayName;
    FGameplayTag AttachmentType;     // Muzzle, Stock, Grip, etc.

    // Stat modifiers (multiplicative)
    float RecoilModifier = 1.0f;     // 0.75 = -25% recoil
    float ErgonomicsModifier = 0.0f; // +5 = adds 5 ergonomics
    float AccuracyModifier = 1.0f;   // 0.9 = -10% spread

    // Weight
    float Weight = 0.0f;             // kg

    // Compatibility
    FGameplayTagContainer CompatibleSlots;
    FGameplayTagContainer CompatibleWeapons;
};
```

---

## 4. Target Architecture

### 4.1 Target Recoil Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│                    TARGET RECOIL ARCHITECTURE                        │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  [SHOT FIRED]                                                       │
│       │                                                             │
│       ▼                                                             │
│  ┌───────────────────────────────────────────────────────────────┐ │
│  │                   RECOIL CALCULATOR                            │ │
│  │                                                                │ │
│  │  Inputs:                                                       │ │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐              │ │
│  │  │ Weapon SSOT │ │ Ammo SSOT   │ │ Attachments │              │ │
│  │  │ Recoil: 145 │ │ Modifier:1.0│ │ Muzzle:-15% │              │ │
│  │  │ Ergo: 42    │ │             │ │ Stock: -10% │              │ │
│  │  │ Conv: 5.0   │ │             │ │ Grip: -5%   │              │ │
│  │  └─────────────┘ └─────────────┘ └──────┬──────┘              │ │
│  │                                         │                      │ │
│  │                    ┌────────────────────┘                      │ │
│  │                    ▼                                           │ │
│  │  ┌─────────────────────────────────────────────────────┐      │ │
│  │  │  TotalRecoilMod = Muzzle × Stock × Grip × Ammo      │      │ │
│  │  │                 = 0.85 × 0.90 × 0.95 × 1.0           │      │ │
│  │  │                 = 0.727 (27% reduction!)             │      │ │
│  │  └─────────────────────────────────────────────────────┘      │ │
│  │                                                                │ │
│  └───────────────────────────────────────────────────────────────┘ │
│                       │                                             │
│                       ▼                                             │
│  ┌───────────────────────────────────────────────────────────────┐ │
│  │                   RECOIL IMPULSE                               │ │
│  │                                                                │ │
│  │  ImpulseV = BaseV × TotalMod × ADS × Progressive              │ │
│  │           = 145 × 0.727 × 0.5 × 1.2                           │ │
│  │           = 63.3 recoil points                                 │ │
│  │                                                                │ │
│  │  ImpulseDegrees = 63.3 × 0.002 = 0.127°                       │ │
│  │                                                                │ │
│  └───────────────────────────────────────────────────────────────┘ │
│                       │                                             │
│           ┌───────────┴───────────┐                                 │
│           ▼                       ▼                                 │
│  ┌─────────────────┐     ┌─────────────────┐                       │
│  │  CAMERA RECOIL  │     │  AIM RECOIL     │                       │
│  │  (Visual)       │     │  (Actual)       │                       │
│  │  AddPitchInput  │     │  AimOffset      │                       │
│  │  + CameraShake  │     │  (for bullet)   │                       │
│  └────────┬────────┘     └────────┬────────┘                       │
│           │                       │                                 │
│           ▼                       ▼                                 │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                   CONVERGENCE SYSTEM                         │   │
│  │                                                              │   │
│  │  Every Tick:                                                 │   │
│  │    if (TimeSinceShot > ConvergenceDelay)                    │   │
│  │      CameraOffset = Lerp(CameraOffset, 0, ConvergenceSpeed) │   │
│  │      AimOffset = Lerp(AimOffset, 0, ConvergenceSpeed)       │   │
│  │                                                              │   │
│  │  Ergonomics bonus:                                           │   │
│  │    ConvergenceSpeed *= (1 + Ergonomics/100)                 │   │
│  │                                                              │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 4.2 Component Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    COMPONENT STRUCTURE                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  USuspenseCoreRecoilComponent (NEW)                             │
│  ├── FSuspenseCoreRecoilState (runtime state)                   │
│  │   ├── CurrentCameraOffset: FRotator                          │
│  │   ├── CurrentAimOffset: FVector2D                            │
│  │   ├── TimeSinceLastShot: float                               │
│  │   ├── ConsecutiveShotsCount: int32                           │
│  │   └── bIsConverging: bool                                    │
│  │                                                              │
│  ├── FSuspenseCoreRecoilConfig (from weapon SSOT)               │
│  │   ├── BaseVerticalRecoil: float                              │
│  │   ├── BaseHorizontalRecoil: float                            │
│  │   ├── ConvergenceSpeed: float                                │
│  │   ├── ConvergenceDelay: float                                │
│  │   └── RecoilAngleBias: float                                 │
│  │                                                              │
│  └── Methods:                                                   │
│      ├── ApplyRecoilImpulse()                                   │
│      ├── TickConvergence(DeltaTime)                             │
│      ├── CalculateTotalModifier()                               │
│      └── GetEffectiveRecoil()                                   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 5. SSOT Data Structures

### 5.1 Existing Structures to Modify

#### FSuspenseCoreWeaponAttributeRow (additions)

**File:** `Source/BridgeSystem/Public/SuspenseCore/Types/GAS/SuspenseCoreGASAttributeRows.h`

```cpp
// Add to existing structure (after HorizontalRecoil):

//========================================================================
// Recoil Dynamics (NEW)
//========================================================================

/** Convergence speed - how fast camera returns to aim point (degrees/second) */
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil",
    meta = (ClampMin = "1.0", ClampMax = "20.0", ToolTip = "Camera return speed"))
float ConvergenceSpeed = 5.0f;

/** Delay before convergence starts after shot (seconds) */
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil",
    meta = (ClampMin = "0.0", ClampMax = "0.5", ToolTip = "Convergence delay"))
float ConvergenceDelay = 0.1f;

/** Horizontal bias: -1.0 (always left) to 1.0 (always right), 0 = random */
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil",
    meta = (ClampMin = "-1.0", ClampMax = "1.0", ToolTip = "Horizontal recoil tendency"))
float RecoilAngleBias = 0.0f;

/** Recoil predictability: 0.0 (fully random) to 1.0 (learnable pattern) */
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil",
    meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Pattern predictability"))
float RecoilPatternStrength = 0.3f;
```

### 5.2 New Structure: Attachment Attributes

**File:** `Source/BridgeSystem/Public/SuspenseCore/Types/GAS/SuspenseCoreGASAttributeRows.h`

```cpp
/**
 * FSuspenseCoreAttachmentAttributeRow
 *
 * SSOT DataTable row for weapon attachment/modification stats.
 * Used for muzzle devices, stocks, grips, handguards, optics.
 * Modifiers are multiplicative (0.85 = -15%, 1.1 = +10%).
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreAttachmentAttributeRow : public FTableRowBase
{
    GENERATED_BODY()

    //========================================================================
    // Identity
    //========================================================================

    /** Unique attachment ID (e.g., "PBS1_Suppressor") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FName AttachmentID;

    /** Display name for UI */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FText DisplayName;

    /** Attachment slot type (Muzzle, Stock, Grip, Sight, Handguard) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity",
        meta = (Categories = "Equipment.Slot"))
    FGameplayTag SlotType;

    //========================================================================
    // Stat Modifiers (Multiplicative, 1.0 = no change)
    //========================================================================

    /** Recoil modifier: 0.75 = -25% recoil, 1.2 = +20% recoil */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifiers",
        meta = (ClampMin = "0.5", ClampMax = "1.5", ToolTip = "Recoil multiplier"))
    float RecoilModifier = 1.0f;

    /** Accuracy modifier: 0.9 = -10% spread, 1.1 = +10% spread */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifiers",
        meta = (ClampMin = "0.5", ClampMax = "1.5", ToolTip = "Accuracy multiplier"))
    float AccuracyModifier = 1.0f;

    /** Muzzle velocity modifier: affects bullet speed */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifiers",
        meta = (ClampMin = "0.8", ClampMax = "1.2", ToolTip = "Velocity multiplier"))
    float VelocityModifier = 1.0f;

    //========================================================================
    // Stat Additions (Additive)
    //========================================================================

    /** Ergonomics addition: +5 = adds 5 ergonomics points */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Additions",
        meta = (ClampMin = "-30", ClampMax = "30", ToolTip = "Ergonomics bonus"))
    float ErgonomicsBonus = 0.0f;

    /** Weight in kg */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical",
        meta = (ClampMin = "0.0", ClampMax = "5.0", ToolTip = "Weight in kg"))
    float Weight = 0.1f;

    //========================================================================
    // Special Effects
    //========================================================================

    /** Suppresses sound (for suppressors) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    bool bSuppressesSound = false;

    /** Sound reduction percentage */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects",
        meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bSuppressesSound"))
    float SoundReduction = 0.0f;

    /** Hides muzzle flash (for flash hiders) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    bool bHidesMuzzleFlash = false;

    //========================================================================
    // Compatibility
    //========================================================================

    /** Weapon types this attachment works with */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compatibility",
        meta = (Categories = "Weapon.Type"))
    FGameplayTagContainer CompatibleWeaponTypes;

    /** Specific weapons this works with (if empty, uses CompatibleWeaponTypes) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compatibility")
    TArray<FName> CompatibleWeaponIDs;

    //========================================================================
    // Visuals
    //========================================================================

    /** Static mesh for world display */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    TSoftObjectPtr<UStaticMesh> AttachmentMesh;

    /** Icon for inventory */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    TSoftObjectPtr<UTexture2D> Icon;
};
```

### 5.3 Runtime Attachment Instance

**File:** `Source/BridgeSystem/Public/SuspenseCore/Types/Equipment/SuspenseCoreAttachmentInstance.h` (NEW)

```cpp
/**
 * FSuspenseCoreAttachmentInstance
 *
 * Runtime instance of an attachment on a weapon.
 * Links to SSOT via AttachmentID.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreAttachmentInstance
{
    GENERATED_BODY()

    /** SSOT link - row name in DT_AttachmentAttributes */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName AttachmentID;

    /** Unique runtime instance ID */
    UPROPERTY(BlueprintReadOnly)
    FGuid InstanceGuid;

    /** Current durability (affects reliability) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CurrentDurability = 100.0f;

    /** Slot this is attached to */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        meta = (Categories = "Equipment.Slot"))
    FGameplayTag AttachedSlot;

    /** Is currently installed on a weapon */
    UPROPERTY(BlueprintReadOnly)
    bool bIsInstalled = false;

    // Helpers
    bool IsValid() const { return !AttachmentID.IsNone(); }
};
```

---

## 6. Implementation Plan

### Phase 1: Basic Recoil with Ammo Modifier ✅ DONE

**Commit:** `02f866f`

**Changes:**
- Added RecoilPointsToDegrees conversion (0.002)
- Integrated AmmoRecoilModifier into calculation
- Documented conversion factor

### Phase 2: Add Convergence System ✅ DONE

**Commit:** Current session (2026-01-14)

**Changes:**
- Added `FSuspenseCoreRecoilState` for runtime tracking
- Implemented `TickConvergence()` with world tick binding
- Added ConvergenceSpeed, ConvergenceDelay, RecoilAngleBias, RecoilPatternStrength to SSOT
- Camera auto-returns to aim point after shots

**Files Modified:**
- `SuspenseCoreGASAttributeRows.h` - Added convergence fields to FSuspenseCoreWeaponAttributeRow
- `SuspenseCoreBaseFireAbility.h` - Added FSuspenseCoreRecoilState, convergence methods
- `SuspenseCoreBaseFireAbility.cpp` - Implemented convergence system
- `SuspenseCoreWeaponAttributeSet.h/.cpp` - Added convergence attributes

### Phase 3: Attachment Modifier System ✅ DONE

**Commit:** 2026-01-14

**Changes:**
- Created `FSuspenseCoreAttachmentAttributeRow` SSOT in `SuspenseCoreGASAttributeRows.h`
- Created `FSuspenseCoreAttachmentInstance` runtime struct
- Added `AttachmentAttributesDataTable` to Settings and DataManager
- Implemented full SSOT lookup in `CalculateAttachmentRecoilModifier()`
- Added attachment storage to `SuspenseCoreWeaponActor`

**Files Created:**
- `Source/BridgeSystem/Public/SuspenseCore/Types/Equipment/SuspenseCoreAttachmentInstance.h`
- `Content/Data/ItemDatabase/SuspenseCoreAttachmentAttributes.json`

**Files Modified:**
- `SuspenseCoreSettings.h` - Added AttachmentAttributesDataTable
- `SuspenseCoreDataManager.h/.cpp` - Added GetAttachmentAttributes, BuildAttachmentAttributesCache
- `SuspenseCoreBaseFireAbility.cpp` - Full SSOT lookup in CalculateAttachmentRecoilModifier
- `ISuspenseCoreWeapon.h` - Added attachment interface methods
- `SuspenseCoreWeaponActor.h/.cpp` - Added InstalledAttachments storage

### Phase 4: Ergonomics Integration ✅ DONE

**Commit:** Current session (2026-01-14)

**Changes:**
- Ergonomics affects convergence speed: `Speed × (1 + Ergonomics/100)`
- 42 ergonomics = 1.42× faster recovery
- 70 ergonomics = 1.70× faster recovery
- Cached in RecoilState for performance

### Phase 5: Visual vs Aim Recoil Separation ✅ DONE

**Commit:** 2026-01-14

**Changes:**
- Separated visual recoil (camera kick) from aim recoil (bullet direction)
- Visual recoil is amplified by `VisualRecoilMultiplier` (default 1.5x)
- Aim recoil stays at base value for accurate bullet placement
- Visual converges faster (VisualConvergenceMultiplier) for stable "feel"
- Added `GetAimOffsetRotator()` to apply aim offset to shot direction

**Key Features:**
- **VisualPitch/VisualYaw**: What player sees (stronger, dramatic)
- **AimPitch/AimYaw**: Where bullets go (more stable)
- Separate convergence rates for visual and aim
- Aim offset applied in `GenerateShotRequest()` for bullet trajectory

**Files Modified:**
- `SuspenseCoreBaseFireAbility.h` - Added visual/aim to FSuspenseCoreRecoilState, VisualRecoilMultiplier to FSuspenseCoreRecoilConfig
- `SuspenseCoreBaseFireAbility.cpp` - Separate visual/aim in ApplyRecoil(), dual convergence in TickConvergence(), aim offset in GenerateShotRequest()

### Phase 6: Recoil Patterns ✅ DONE

**Commit:** 2026-01-14

**Changes:**
- Added `FSuspenseCoreRecoilPatternPoint` for individual pattern points
- Added `FSuspenseCoreRecoilPattern` with configurable spray patterns
- Patterns blend with random recoil based on `RecoilPatternStrength`
- Pattern loops with scaled-down values for sustained fire
- Default 8-shot pattern with characteristic up-left-right drift

**Key Features:**
- **PatternStrength 0.0**: Pure random (unpredictable like CoD)
- **PatternStrength 0.5**: Semi-predictable (like Tarkov)
- **PatternStrength 1.0**: Fully learnable (like CS:GO)
- Pattern multipliers affect base recoil values per shot
- LoopScaleFactor reduces pattern intensity on subsequent loops (0.7x)

**Files Modified:**
- `SuspenseCoreBaseFireAbility.h` - Added FSuspenseCoreRecoilPatternPoint, FSuspenseCoreRecoilPattern, CachedPatternStrength
- `SuspenseCoreBaseFireAbility.cpp` - Pattern blending in ApplyRecoil(), InitializeRecoilStateFromWeapon() caches PatternStrength

---

## 7. Technical Specifications

### 7.1 Recoil Calculation Formula (Target)

```cpp
float CalculateFinalRecoil(
    const USuspenseCoreWeaponAttributeSet* WeaponAttrs,
    const USuspenseCoreAmmoAttributeSet* AmmoAttrs,
    const TArray<FSuspenseCoreAttachmentInstance>& Attachments,
    bool bIsAiming,
    int32 ConsecutiveShots)
{
    // 1. Get base recoil from weapon SSOT
    float BaseRecoil = WeaponAttrs->GetVerticalRecoil();

    // 2. Apply ammo modifier (0.5 - 2.0)
    float AmmoMod = AmmoAttrs ? AmmoAttrs->GetRecoilModifier() : 1.0f;

    // 3. Calculate total attachment modifier
    float AttachmentMod = 1.0f;
    for (const auto& Attachment : Attachments)
    {
        if (const FSuspenseCoreAttachmentAttributeRow* Row = GetAttachmentData(Attachment.AttachmentID))
        {
            AttachmentMod *= Row->RecoilModifier;
        }
    }

    // 4. ADS modifier
    float ADSMod = bIsAiming ? 0.5f : 1.0f;

    // 5. Progressive modifier (increases with consecutive shots)
    float ProgressiveMod = 1.0f + FMath::Min((ConsecutiveShots - 1) * 0.2f, 2.0f);

    // 6. Convert to degrees
    constexpr float PointsToDegrees = 0.002f;

    // Final calculation
    float FinalRecoil = BaseRecoil
                      * AmmoMod
                      * AttachmentMod
                      * ADSMod
                      * ProgressiveMod
                      * PointsToDegrees;

    return FinalRecoil;
}
```

### 7.2 Convergence Algorithm

```cpp
void TickConvergence(float DeltaTime)
{
    // Skip if recently fired
    if (TimeSinceLastShot < ConvergenceDelay)
    {
        TimeSinceLastShot += DeltaTime;
        return;
    }

    // Calculate convergence rate
    // Base: 5.0 deg/sec, boosted by ergonomics
    float ErgoBonus = 1.0f + (Ergonomics / 100.0f); // 42 ergo = 1.42x speed
    float ConvergenceRate = ConvergenceSpeed * ErgoBonus * DeltaTime;

    // Interpolate camera offset toward zero
    CurrentCameraOffset = FMath::Lerp(
        CurrentCameraOffset,
        FRotator::ZeroRotator,
        FMath::Clamp(ConvergenceRate / CurrentCameraOffset.Size(), 0.0f, 1.0f)
    );

    // Check if fully converged
    if (CurrentCameraOffset.IsNearlyZero(0.01f))
    {
        CurrentCameraOffset = FRotator::ZeroRotator;
        bIsConverging = false;
    }
}
```

### 7.3 Example Attachment Data

```cpp
// DT_AttachmentAttributes examples

// Muzzle Devices
{ "PBS1_Suppressor",     "PBS-1 Suppressor",      Slot.Muzzle, RecoilMod=0.85, ErgoBonus=-8,  Weight=0.45, bSuppresses=true }
{ "DTK1_MuzzleBrake",    "DTK-1 Muzzle Brake",    Slot.Muzzle, RecoilMod=0.80, ErgoBonus=-2,  Weight=0.12 }
{ "PWS_FlashHider",      "PWS CQB Flash Hider",   Slot.Muzzle, RecoilMod=0.95, ErgoBonus=+1,  Weight=0.08, bHidesFlash=true }

// Stocks
{ "ZhukovS_Stock",       "Zhukov-S Stock",        Slot.Stock,  RecoilMod=0.88, ErgoBonus=+4,  Weight=0.32 }
{ "MOE_Stock",           "Magpul MOE Stock",      Slot.Stock,  RecoilMod=0.92, ErgoBonus=+6,  Weight=0.28 }
{ "Buffer_Advanced",     "Advanced Buffer Tube",  Slot.Stock,  RecoilMod=0.90, ErgoBonus=+2,  Weight=0.18 }

// Grips
{ "RK1_Foregrip",        "RK-1 Foregrip",         Slot.Grip,   RecoilMod=0.96, ErgoBonus=+3,  Weight=0.09 }
{ "Fortis_Shift",        "Fortis Shift Grip",     Slot.Grip,   RecoilMod=0.94, ErgoBonus=+5,  Weight=0.11 }
{ "RVG_Grip",            "Magpul RVG",            Slot.Grip,   RecoilMod=0.97, ErgoBonus=+2,  Weight=0.07 }
```

---

## 8. References

### 8.1 Project Files

| File | Purpose |
|------|---------|
| `Source/BridgeSystem/Public/SuspenseCore/Types/GAS/SuspenseCoreGASAttributeRows.h` | SSOT structures |
| `Source/GAS/Private/SuspenseCore/Abilities/Base/SuspenseCoreBaseFireAbility.cpp` | Recoil implementation |
| `Source/GAS/Public/SuspenseCore/Attributes/SuspenseCoreWeaponAttributeSet.h` | Weapon GAS attributes |
| `Source/GAS/Public/SuspenseCore/Attributes/SuspenseCoreAmmoAttributeSet.h` | Ammo GAS attributes |
| `Source/BridgeSystem/Public/SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h` | Item definitions |
| `Documentation/Plans/TarkovStyle_Ammo_System_Design.md` | Related ammo system |

### 8.2 Tarkov Reference Data

| Source | URL |
|--------|-----|
| Tarkov Wiki - Weapons | https://escapefromtarkov.fandom.com/wiki/Weapons |
| Tarkov Wiki - Weapon Mods | https://escapefromtarkov.fandom.com/wiki/Weapon_mods |
| Tarkov Wiki - Skills | https://escapefromtarkov.fandom.com/wiki/Skills |
| NoFoodAfterMidnight Ammo Chart | Community spreadsheet |

### 8.3 Industry Methods Comparison

| Method | Games | Pros | Cons |
|--------|-------|------|------|
| **Spray Patterns** | CS:GO, Valorant | Skill-based, learnable | Unintuitive for casuals |
| **Procedural Random** | Call of Duty | Easy to implement | Less skill expression |
| **Convergence** | Tarkov, Squad | Realistic feel | Complex implementation |
| **Physics-Based** | ARMA, Squad | Most realistic | Performance cost |
| **Hybrid (Ours)** | Target | Best of both | Development effort |

---

## Summary

### Implementation Complete ✅

All 6 phases of the Tarkov-style recoil system have been implemented:

| Phase | Feature | Status |
|-------|---------|--------|
| Phase 1 | Basic Recoil + Ammo Modifier | ✅ Done |
| Phase 2 | Convergence System | ✅ Done |
| Phase 3 | Attachment Modifiers SSOT | ✅ Done |
| Phase 4 | Ergonomics Integration | ✅ Done |
| Phase 5 | Visual vs Aim Separation | ✅ Done |
| Phase 6 | Recoil Patterns | ✅ Done |

### Key Features Implemented

1. **Convergence System**: Camera auto-returns to aim point after shots
2. **Attachment Modifiers**: Full SSOT chain from DataTable to FireAbility
3. **Ergonomics Integration**: Affects convergence speed (42 ergo = 1.42× speed)
4. **Visual/Aim Separation**: Visual kick stronger (1.5×), aim more stable
5. **Recoil Patterns**: Learnable patterns blend with random (0-100% configurable)

### Future Enhancements (Optional)
- Character skill integration ("Recoil Control" skill reduces camera recoil)
- Per-weapon custom patterns (load from DataTable)
- Network replication for recoil state
- Attachment durability affects recoil modifiers

---

**Last Updated:** 2026-01-13
**Version:** 1.0
