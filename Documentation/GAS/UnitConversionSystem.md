# Unit Conversion System

> **Version:** 1.0
> **Created:** 2026-01-15
> **Status:** Implemented
> **Author:** Technical Lead
> **Target:** SuspenseCore GAS Architecture

---

## Table of Contents

1. [Overview](#1-overview)
2. [Problem Statement](#2-problem-statement)
3. [Solution Architecture](#3-solution-architecture)
4. [Implementation Details](#4-implementation-details)
5. [Usage Guide](#5-usage-guide)
6. [Data Flow](#6-data-flow)
7. [Files Modified](#7-files-modified)
8. [Testing](#8-testing)

---

## 1. Overview

### 1.1 Purpose

This document describes the unit conversion system implemented in SuspenseCore to properly handle the difference between **real-world units** (used in DataTables/JSON for human-readability) and **Unreal Engine units** (used for gameplay calculations).

### 1.2 Key Principle

```
DataTables store REAL-WORLD values (meters, m/s, kg)
↓
Gameplay code converts TO UE units at point of use
↓
Never store converted values back to DataTables
```

### 1.3 Unreal Engine Unit System

| UE Unit | Real World Equivalent |
|---------|----------------------|
| 1 UE unit | 1 centimeter |
| 100 UE units | 1 meter |
| 100,000 UE units | 1 kilometer |

---

## 2. Problem Statement

### 2.1 The Bug

Weapon traces were too short because:

1. **JSON/DataTable values** stored range in **meters** (e.g., `MaxRange: 600`)
2. **Code used values directly** as UE units (treating 600 as 600cm = 6 meters!)
3. **Result:** AK-74M with 600m max range only traced 6 meters

### 2.2 Before Fix

```cpp
// WRONG - treating meters as centimeters!
Params.Range = WeaponAttrs->GetMaxRange(); // 600 (meters) used as 600 UE units
// Trace only goes 6 meters!
```

### 2.3 After Fix

```cpp
// CORRECT - proper unit conversion
Params.Range = USuspenseCoreSpreadCalculator::CalculateMaxTraceRange(
    WeaponAttrs, AmmoAttrs
);
// 600 meters → 60000 UE units → Trace goes 600 meters!
```

---

## 3. Solution Architecture

### 3.1 SSOT Header: SuspenseCoreUnits.h

**Location:** `Source/BridgeSystem/Public/SuspenseCore/Core/SuspenseCoreUnits.h`

This header is the **Single Source of Truth** for all unit conversions:

```cpp
namespace SuspenseCoreUnits
{
    // Distance conversions
    constexpr float MetersToUnits = 100.0f;
    constexpr float UnitsToMeters = 0.01f;

    // Velocity conversions
    constexpr float MetersPerSecToUnitsPerSec = 100.0f;

    // Helper functions
    float ConvertRangeToUnits(float RangeMeters);
    float ConvertVelocityToUnits(float VelocityMS);
    float ConvertDistanceToMeters(float DistanceUnits);
}
```

### 3.2 Conversion Philosophy

| Layer | Unit System | Example |
|-------|-------------|---------|
| **JSON/DataTable** | Real-world (meters) | `MaxRange: 600` |
| **AttributeSet** | Real-world (meters) | `GetMaxRange() → 600.0f` |
| **SpreadCalculator** | UE units (cm) | `CalculateMaxTraceRange() → 60000.0f` |
| **LineTrace** | UE units (cm) | `TraceEnd = Start + Dir * 60000` |

---

## 4. Implementation Details

### 4.1 CalculateMaxTraceRange vs CalculateEffectiveRange

| Function | Purpose | Returns |
|----------|---------|---------|
| `CalculateEffectiveRange()` | Damage falloff distance | **Meters** (no conversion) |
| `CalculateMaxTraceRange()` | Trace endpoint distance | **UE Units** (converted) |

**Rule:** Use `CalculateMaxTraceRange()` for ALL trace distances!

### 4.2 Key Constants

```cpp
namespace SuspenseCoreUnits
{
    // Conversion factors
    constexpr float MetersToUnits = 100.0f;

    // Game limits
    constexpr float MaxGameRangeMeters = 1500.0f;
    constexpr float MaxGameRangeUnits = 150000.0f;

    // Fallbacks
    constexpr float DefaultTraceRangeUnits = 1000000.0f; // 10km
    constexpr float MinTraceRangeUnits = 100.0f;         // 1m
}
```

### 4.3 Tarkov-Realistic Values (from JSON)

| Weapon | EffectiveRange (m) | MaxRange (m) | Trace Range (UE) |
|--------|-------------------|--------------|------------------|
| АК-74М | 350 | 600 | 60,000 |
| АК-103 | 400 | 650 | 65,000 |
| M4A1 | 400 | 600 | 60,000 |
| СВД | 800 | 1200 | 120,000 |
| MP5SD | 100 | 200 | 20,000 |
| Glock 17 | 50 | 100 | 10,000 |
| MP7A2 | 150 | 250 | 25,000 |

---

## 5. Usage Guide

### 5.1 Getting Trace Range

```cpp
#include "SuspenseCore/Utils/SuspenseCoreSpreadCalculator.h"

// In fire ability or trace task:
float TraceRange = USuspenseCoreSpreadCalculator::CalculateMaxTraceRange(
    WeaponAttrs,
    AmmoAttrs
);
// TraceRange is in UE units, ready for LineTrace!
```

### 5.2 Manual Conversion (if needed)

```cpp
#include "SuspenseCore/Core/SuspenseCoreUnits.h"

// Convert meters to UE units
float RangeMeters = WeaponAttrs->GetMaxRange();
float RangeUnits = SuspenseCoreUnits::ConvertRangeToUnits(RangeMeters);

// Or use the constant directly
float RangeUnits = RangeMeters * SuspenseCoreUnits::MetersToUnits;
```

### 5.3 Displaying Distance to Player

```cpp
// After trace hit, display distance in meters
float DistanceUnits = HitResult.Distance;
FString DisplayText = SuspenseCoreUnits::GetDistanceString(DistanceUnits);
// Returns "350m" or "1.2km"
```

---

## 6. Data Flow

### 6.1 Complete Flow Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           DATA FLOW                                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  JSON (SSOT - Human Readable)                                               │
│  ├── SuspenseCoreWeaponAttributes.json                                      │
│  │   └── "MaxRange": 600  ← METERS (Tarkov-realistic)                       │
│  │                                                                           │
│  ▼                                                                           │
│  DataTable (UE Editor)                                                       │
│  └── DT_WeaponAttributes                                                    │
│      └── MaxRange = 600.0f  ← METERS (as loaded)                            │
│                                                                              │
│  ▼                                                                           │
│  USuspenseCoreWeaponAttributeSet (Runtime)                                  │
│  └── GetMaxRange() → 600.0f  ← METERS (raw attribute value)                 │
│                                                                              │
│  ▼                                                                           │
│  USuspenseCoreSpreadCalculator::CalculateMaxTraceRange()                    │
│  └── 600.0f × 100 = 60000.0f  ← UE UNITS (centimeters)                     │
│                                                                              │
│  ▼                                                                           │
│  FWeaponShotParams.Range = 60000.0f                                         │
│  │                                                                           │
│  ▼                                                                           │
│  LineTraceMultiByProfile(Start, Start + Dir * 60000, ...)                   │
│  └── Traces 600 meters correctly!                                           │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 6.2 Fire Ability Flow

```
1. GenerateShotRequest()
   ├── Get WeaponAttrs from ASC
   ├── Call CalculateMaxTraceRange(WeaponAttrs, AmmoAttrs)
   │   └── Returns 60000.0f (UE units)
   └── Params.Range = 60000.0f

2. ServerProcessShotTrace()
   ├── TraceEnd = Start + Direction * Params.Range
   └── PerformLineTrace(Start, TraceEnd, ...)
       └── Traces 600 meters!
```

---

## 7. Files Modified

### 7.1 New Files

| File | Purpose |
|------|---------|
| `Source/BridgeSystem/Public/SuspenseCore/Core/SuspenseCoreUnits.h` | SSOT for unit conversion constants |
| `Documentation/GAS/UnitConversionSystem.md` | This documentation |

### 7.2 Modified Files

| File | Changes |
|------|---------|
| `SuspenseCoreSpreadCalculator.h` | Added `CalculateMaxTraceRange()`, `CalculateMaxTraceRangeFromWeapon()` |
| `SuspenseCoreSpreadCalculator.cpp` | Implemented new functions with unit conversion |
| `SuspenseCoreBaseFireAbility.cpp` | Changed to use `CalculateMaxTraceRange()` |
| `SuspenseCoreWeaponAsyncTask_PerformTrace.h` | Updated `DefaultMaxRange` to use `SuspenseCoreUnits` |
| `SuspenseCoreWeaponAsyncTask_PerformTrace.cpp` | Added unit conversion for MaxRange |

### 7.3 Unchanged Files (Already Correct)

| File | Status |
|------|--------|
| `SuspenseCoreWeaponAttributes.json` | Values already in meters (correct) |
| `SuspenseCoreAmmoAttributes.json` | Values already in meters (correct) |
| `SuspenseCoreWeaponAttributeSet.h/cpp` | Stores raw meter values (correct) |

---

## 8. Testing

### 8.1 Verification Checklist

- [ ] АК-74М traces 600 meters (60000 UE units)
- [ ] СВД traces 1200 meters (120000 UE units)
- [ ] Glock 17 traces 100 meters (10000 UE units)
- [ ] Debug draw shows correct trace length
- [ ] `showdebug abilitysystem` shows correct MaxRange value

### 8.2 Debug Commands

```cpp
// In-game console
showdebug abilitysystem  // Check weapon attribute values

// C++ Debug logging
UE_LOG(LogSuspenseCore, Log, TEXT("MaxRange: %f meters, Trace: %f UE units"),
    WeaponAttrs->GetMaxRange(),
    USuspenseCoreSpreadCalculator::CalculateMaxTraceRange(WeaponAttrs, nullptr)
);
```

### 8.3 Visual Debug

Enable trace debug visualization:

```cpp
FSuspenseCoreWeaponTraceConfig Config;
Config.bDebug = true;
Config.DebugDrawTime = 5.0f;

// Trace will draw:
// - Green line: Clean shot (no hit)
// - Red line: Blocking hit
// - Orange line: Penetration hit
```

---

## 9. Summary

### 9.1 Key Takeaways

1. **Always use `CalculateMaxTraceRange()`** for trace distances
2. **Never use `GetMaxRange()` directly** for traces (it's in meters!)
3. **JSON values are in meters** - this is intentional for human readability
4. **Conversion happens at point of use** - not at data loading

### 9.2 Quick Reference

```cpp
// For trace distance
float TraceRange = USuspenseCoreSpreadCalculator::CalculateMaxTraceRange(WeaponAttrs, AmmoAttrs);

// For damage falloff
float EffectiveRange = USuspenseCoreSpreadCalculator::CalculateEffectiveRange(WeaponAttrs, AmmoAttrs);

// Manual conversion
float UEUnits = Meters * SuspenseCoreUnits::MetersToUnits;
```

---

**Document Status:** Complete
**Related Documents:**
- `SSOT_AttributeSet_DataTable_Integration.md`
- `WeaponAbilityDevelopmentGuide.md`
- `SuspenseCoreWeaponAttributes.json`
