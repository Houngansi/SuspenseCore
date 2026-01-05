# Item Use System Pipeline

> **Version:** 1.1
> **Author:** Claude Code
> **Date:** 2026-01-05
> **Status:** APPROVED
> **Related Docs:**
> - `SuspenseCoreArchitecture.md` (BridgeSystem architecture)
> - `TarkovStyle_Ammo_System_Design.md` (existing ammo/magazine system)
> - `WeaponAbilities_DeveloperGuide.md` (GAS integration patterns)

---

## Executive Summary

This document defines the implementation pipeline for the **Item Use System** - a unified GAS-based ability system for handling all item interactions (QuickSlots, drag&drop, double-click usage, etc.).

### Key Design Decisions

| Decision | Rationale | Reference |
|----------|-----------|-----------|
| **Types/Interfaces in BridgeSystem** | Avoids circular dependencies between GAS ↔ EquipmentSystem | `SuspenseCoreArchitecture.md` §Module Dependencies |
| **Service via ServiceProvider** | Centralized DI pattern for service access | `SuspenseCoreArchitecture.md` §USuspenseCoreServiceProvider |
| **EventBus for communication** | Decoupled systems, no direct cross-module calls | `SuspenseCoreArchitecture.md` §EventBus |
| **Native Tags** | Compile-time validation, no runtime tag errors | `WeaponAbilities_DeveloperGuide.md` §Native Tags |
| **GAS Cooldown System** | Replaces tick-based reloading with proper ability cooldowns | This document |
| **Handler Pattern** | Extensible item type support without modifying core ability | `SuspenseCoreArchitecture.md` §SOLID |

---

## Table of Contents

1. [Problem Statement](#1-problem-statement)
2. [Architecture Overview](#2-architecture-overview)
3. [Module Dependency Analysis](#3-module-dependency-analysis)
4. [File Structure Plan](#4-file-structure-plan)
5. [Implementation Phases](#5-implementation-phases)
6. [Native Tags Specification](#6-native-tags-specification)
7. [Type Definitions](#7-type-definitions)
8. [Interface Specifications](#8-interface-specifications)
9. [Service Implementation](#9-service-implementation)
10. [GAS Ability Implementation](#10-gas-ability-implementation)
11. [Handler Implementations](#11-handler-implementations)
12. [Migration from Tick-Based to GAS Cooldowns](#12-migration-from-tick-based-to-gas-cooldowns)
13. [Integration Points](#13-integration-points)
14. [Testing Strategy](#14-testing-strategy)
15. [Checklist](#15-checklist)

---

## 1. Problem Statement

### Current Issues

1. **Fragmented Item Use Logic**
   - QuickSlot hotkeys (4-5-6-7) in HUD Master → hardcoded logic
   - Equipment slots (13-14-15-16) → separate QuickSlotComponent logic
   - Double-click usage → handled in UI directly
   - Drag&Drop operations → scattered across multiple components

2. **Tick-Based Reloading (Anti-Pattern)**
   - `AmmoLoadingService` uses `TickComponent()` for reload timing
   - Not integrated with GAS ability system
   - No cooldown visualization in UI via GAS
   - Cannot be interrupted via GAS tags

3. **No SSOT for Item Usage**
   - Multiple entry points for same operation
   - Inconsistent validation across paths
   - Difficult to add new item types

### Solution: Unified Item Use System

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    UNIFIED ITEM USE SYSTEM                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ALL INPUT SOURCES              SINGLE ABILITY          HANDLER REGISTRY    │
│  ─────────────────              ──────────────          ────────────────    │
│                                                                             │
│  [4][5][6][7] Keys ────┐       ┌──────────────┐        ┌────────────────┐  │
│                        │       │              │        │ AmmoToMagazine │  │
│  [Double-Click] ───────┼──────►│  GA_ItemUse  │◄──────►│ MagazineSwap   │  │
│                        │       │  (GAS Ability)│       │ Medical        │  │
│  [Drag & Drop] ────────┤       │              │        │ Grenade        │  │
│                        │       │  Cooldowns   │        │ Container      │  │
│  [Context Menu] ───────┘       │  via GAS     │        │ [Extensible]   │  │
│                                └──────────────┘        └────────────────┘  │
│                                       │                                     │
│                                       ▼                                     │
│                                ┌──────────────┐                             │
│                                │   EventBus   │                             │
│                                │   Publish    │                             │
│                                └──────────────┘                             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Architecture Overview

### Compliance with SuspenseCore Architecture

Per `SuspenseCoreArchitecture.md`, all systems must follow:

| Principle | Implementation in ItemUse System |
|-----------|----------------------------------|
| **SSOT** | `ItemUseService` is single entry point for all item usage |
| **EventBus** | All notifications via `USuspenseCoreEventBus` |
| **ServiceProvider** | Service registered with `USuspenseCoreServiceProvider` |
| **Native Tags** | All tags in `SuspenseCoreItemUseNativeTags.h` |
| **No Circular Dependencies** | Interfaces in BridgeSystem only |
| **Server Authority** | Validation via `USuspenseCoreSecurityValidator` |

### System Components

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         COMPONENT ARCHITECTURE                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  BRIDGESYSTEM (Foundation - No Dependencies)                                │
│  ─────────────────────────────────────────────                              │
│  ├── Types/ItemUse/                                                         │
│  │   └── SuspenseCoreItemUseTypes.h           # All structs, enums         │
│  ├── Interfaces/ItemUse/                                                    │
│  │   ├── ISuspenseCoreItemUseHandler.h        # Handler interface          │
│  │   └── ISuspenseCoreItemUseService.h        # Service interface          │
│  └── Tags/                                                                  │
│      └── SuspenseCoreItemUseNativeTags.h      # Native GameplayTags        │
│                                                                             │
│  EQUIPMENTSYSTEM (Depends on: BridgeSystem)                                 │
│  ───────────────────────────────────────────                                │
│  ├── Services/                                                              │
│  │   └── SuspenseCoreItemUseService.h/cpp     # SSOT Implementation        │
│  └── Handlers/                                                              │
│      ├── SuspenseCoreAmmoToMagazineHandler.h/cpp                           │
│      ├── SuspenseCoreMagazineSwapHandler.h/cpp                             │
│      ├── SuspenseCoreMedicalUseHandler.h/cpp                               │
│      └── SuspenseCoreGrenadeHandler.h/cpp                                  │
│                                                                             │
│  GAS (Depends on: BridgeSystem)                                             │
│  ───────────────────────────────                                            │
│  ├── Abilities/                                                             │
│  │   ├── GA_ItemUse.h/cpp                     # Base item use ability      │
│  │   └── GA_QuickSlotUse.h/cpp                # QuickSlot specific         │
│  └── Effects/                                                               │
│      ├── GE_ItemUse_InProgress.uasset         # In-progress state          │
│      └── GE_ItemUse_Cooldown.uasset           # Cooldown effect            │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Module Dependency Analysis

### Current Module Graph (from `SuspenseCoreArchitecture.md`)

```
                    BridgeSystem
                   (shared interfaces)
                         │
        ┌────────────────┼────────────────┐
        │                │                │
        ▼                ▼                ▼
      GAS         EquipmentSystem    PlayerCore
   (abilities)     (components)    (animation)
```

### Why Types/Interfaces MUST Be in BridgeSystem

**WRONG Architecture (creates cycle):**
```
EquipmentSystem
├── SuspenseCoreItemUseTypes.h
└── ISuspenseCoreItemUseHandler.h
        ↑ includes
        GAS (needs to know Request/Response types)
        ↓ includes
EquipmentSystem (needs AbilitySystemComponent)

RESULT: ❌ LINKER ERROR - Circular dependency
```

**CORRECT Architecture:**
```
BridgeSystem (foundation)
├── SuspenseCoreItemUseTypes.h     ← Shared types
├── ISuspenseCoreItemUseHandler.h  ← Handler interface
└── ISuspenseCoreItemUseService.h  ← Service interface
        │
        │ no dependency
        ▼
┌───────────────────────────────────────────┐
│                                           │
▼                                           ▼
GAS                              EquipmentSystem
├── GA_ItemUse.h                 ├── SuspenseCoreItemUseService.h
│   (uses interface)             │   (implements interface)
└── Uses types from              └── Handlers use types from
    BridgeSystem                     BridgeSystem

RESULT: ✅ NO CYCLES - Clean compilation
```

### Module Dependencies (Updated)

```cpp
// BridgeSystem.Build.cs - NO CHANGES (base module)
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "GameplayTags",
    "GameplayAbilities"  // For FGameplayAbilitySpecHandle in types
});

// EquipmentSystem.Build.cs - ADD BridgeSystem dependency
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "BridgeSystem",       // For interfaces and types
    "GameplayTags",
    "GameplayAbilities"
});

// GAS.Build.cs - ADD BridgeSystem dependency
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "BridgeSystem",       // For interfaces and types
    "GameplayTags",
    "GameplayAbilities"
});
```

---

## 4. File Structure Plan

### Phase-Ordered File Creation

```
═══════════════════════════════════════════════════════════════════════════════
PHASE 1: BRIDGESYSTEM (Foundation - No Dependencies)
═══════════════════════════════════════════════════════════════════════════════

Source/BridgeSystem/
├── Public/SuspenseCore/
│   ├── Types/ItemUse/
│   │   └── SuspenseCoreItemUseTypes.h           [NEW] All types/enums
│   │
│   ├── Interfaces/ItemUse/
│   │   ├── ISuspenseCoreItemUseHandler.h        [NEW] Handler interface
│   │   └── ISuspenseCoreItemUseService.h        [NEW] Service interface
│   │
│   └── Tags/
│       └── SuspenseCoreItemUseNativeTags.h      [NEW] Native tags declaration
│
├── Private/SuspenseCore/
│   └── Tags/
│       └── SuspenseCoreItemUseNativeTags.cpp    [NEW] Native tags definition

═══════════════════════════════════════════════════════════════════════════════
PHASE 2: EQUIPMENTSYSTEM (Service Implementation)
═══════════════════════════════════════════════════════════════════════════════

Source/EquipmentSystem/
├── Public/SuspenseCore/
│   └── Services/
│       └── SuspenseCoreItemUseService.h         [NEW] Service header
│
├── Private/SuspenseCore/
│   └── Services/
│       └── SuspenseCoreItemUseService.cpp       [NEW] Service impl

═══════════════════════════════════════════════════════════════════════════════
PHASE 3: GAS (Abilities, Effects, Input Integration)
═══════════════════════════════════════════════════════════════════════════════

Source/GAS/
├── Public/SuspenseCore/
│   ├── Abilities/ItemUse/
│   │   ├── GA_ItemUse.h                         [NEW] Base ability
│   │   └── GA_QuickSlotUse.h                    [NEW] QuickSlot ability
│   │
│   └── Effects/ItemUse/
│       ├── GE_ItemUse_InProgress.h              [NEW] C++ base for in-progress
│       └── GE_ItemUse_Cooldown.h                [NEW] C++ base for cooldown
│
├── Private/SuspenseCore/
│   ├── Abilities/ItemUse/
│   │   ├── GA_ItemUse.cpp                       [NEW]
│   │   └── GA_QuickSlotUse.cpp                  [NEW]
│   │
│   └── Effects/ItemUse/
│       ├── GE_ItemUse_InProgress.cpp            [NEW]
│       └── GE_ItemUse_Cooldown.cpp              [NEW]

Source/PlayerCore/
├── Public/SuspenseCore/
│   └── Input/
│       └── SuspenseCoreItemUseInputConfig.h     [NEW] Input actions config
│
├── Private/SuspenseCore/
│   └── Input/
│       └── SuspenseCoreItemUseInputConfig.cpp   [NEW]

[MODIFY] Existing files:
├── SuspenseCorePlayerController.h/cpp           [MODIFY] Add QuickSlot input bindings
└── SuspenseCoreInputComponent.h/cpp             [MODIFY] Bind IA_QuickSlot1-4

═══════════════════════════════════════════════════════════════════════════════
PHASE 4: HANDLERS (Extensible Item Type Support)
═══════════════════════════════════════════════════════════════════════════════

Source/EquipmentSystem/
├── Public/SuspenseCore/
│   └── Handlers/ItemUse/
│       ├── SuspenseCoreAmmoToMagazineHandler.h  [NEW]
│       ├── SuspenseCoreMagazineSwapHandler.h    [NEW]
│       ├── SuspenseCoreMedicalUseHandler.h      [NEW]
│       └── SuspenseCoreGrenadeHandler.h         [NEW]
│
├── Private/SuspenseCore/
│   └── Handlers/ItemUse/
│       ├── SuspenseCoreAmmoToMagazineHandler.cpp
│       ├── SuspenseCoreMagazineSwapHandler.cpp
│       ├── SuspenseCoreMedicalUseHandler.cpp
│       └── SuspenseCoreGrenadeHandler.cpp

═══════════════════════════════════════════════════════════════════════════════
PHASE 5: INTEGRATION & MIGRATION
═══════════════════════════════════════════════════════════════════════════════

Config/
└── DefaultGameplayTags.ini                      [MODIFY] Add new tags

Source/EquipmentSystem/
├── SuspenseCoreAmmoLoadingService.h/cpp         [MODIFY] Remove tick logic
├── SuspenseCoreQuickSlotComponent.h/cpp         [MODIFY] Use ItemUseService

Content/GAS/Effects/
├── GE_ItemUse_InProgress.uasset                 [NEW] Blueprint GE
└── GE_ItemUse_Cooldown.uasset                   [NEW] Blueprint GE

Documentation/
└── ItemUseSystem_DeveloperGuide.md              [NEW] Usage documentation
```

---

## 5. Implementation Phases

### Phase 1: BridgeSystem Foundation (P0 - Critical)

**Goal:** Create shared types, interfaces, and native tags.

**Files:**
1. `SuspenseCoreItemUseTypes.h` - All enums, structs
2. `ISuspenseCoreItemUseHandler.h` - Handler interface
3. `ISuspenseCoreItemUseService.h` - Service interface
4. `SuspenseCoreItemUseNativeTags.h/.cpp` - Native tags

**Validation:**
- [ ] Compiles independently
- [ ] No module dependencies added
- [ ] All types are `BRIDGESYSTEM_API` exported

---

### Phase 2: Service Implementation (P0 - Critical)

**Goal:** Create SSOT service for item usage.

**Files:**
1. `SuspenseCoreItemUseService.h/.cpp`

**Key Features:**
- Implements `ISuspenseCoreItemUseService`
- Registers with `USuspenseCoreServiceProvider`
- Handler registry with priority sorting
- EventBus integration

**Validation:**
- [ ] Service accessible via `ServiceProvider->GetItemUseService()`
- [ ] EventBus events publish correctly
- [ ] No dependency on GAS module

---

### Phase 3: GAS Abilities, Effects & Input Integration (P0 - Critical)

**Goal:** Create GAS abilities, C++ GameplayEffects, and integrate input bindings.

**Files (Abilities):**
1. `GA_ItemUse.h/.cpp` - Base item use ability
2. `GA_QuickSlotUse.h/.cpp` - QuickSlot specialization (keys 4-7)

**Files (GameplayEffects - C++ Base Classes):**
3. `GE_ItemUse_InProgress.h/.cpp` - In-progress state effect
   - Adds `State.ItemUse.InProgress` tag
   - Duration modifier via SetByCaller
   - Blueprint child: `BP_GE_ItemUse_InProgress`
4. `GE_ItemUse_Cooldown.h/.cpp` - Cooldown effect
   - Adds `State.ItemUse.Cooldown` tag
   - Duration modifier via SetByCaller
   - Blueprint child: `BP_GE_ItemUse_Cooldown`

**Files (Input Integration):**
5. `SuspenseCoreItemUseInputConfig.h/.cpp` - Input action definitions
6. Modify `SuspenseCorePlayerController` - Add QuickSlot input bindings
7. Modify `SuspenseCoreInputComponent` - Bind IA_QuickSlot1-4 actions

**Key Features:**
- Event-triggered activation via AbilitySystemComponent
- Cooldown via C++ GameplayEffects (BP children for configuration)
- Cancellation via damage/movement tags
- Progress reporting for UI via EventBus
- Input bindings: Keys 4-5-6-7 → GA_QuickSlotUse with slot index

**Input Mapping:**
```
IA_QuickSlot1 (Key 4) → GA_QuickSlotUse (SlotIndex=0)
IA_QuickSlot2 (Key 5) → GA_QuickSlotUse (SlotIndex=1)
IA_QuickSlot3 (Key 6) → GA_QuickSlotUse (SlotIndex=2)
IA_QuickSlot4 (Key 7) → GA_QuickSlotUse (SlotIndex=3)
```

**Validation:**
- [ ] GA_ItemUse activates correctly via event
- [ ] GA_QuickSlotUse activates via hotkeys 4-5-6-7
- [ ] GE_ItemUse_InProgress applies State tag correctly
- [ ] GE_ItemUse_Cooldown applies and displays in UI
- [ ] Cancellation works (damage interrupts)
- [ ] Input bindings work in PlayerController

---

### Phase 4: Handlers (P1 - High)

**Goal:** Implement item type handlers.

**Files:**
1. `SuspenseCoreAmmoToMagazineHandler.h/.cpp`
2. `SuspenseCoreMagazineSwapHandler.h/.cpp`
3. `SuspenseCoreMedicalUseHandler.h/.cpp`
4. `SuspenseCoreGrenadeHandler.h/.cpp`

**Validation:**
- [ ] Each handler registers correctly
- [ ] Handler priority resolves conflicts
- [ ] Ammo loading works via handler

---

### Phase 5: Integration & Migration (P1 - High)

**Goal:** Integrate with existing systems, migrate from tick-based.

**Changes:**
1. Remove tick from `AmmoLoadingService`
2. Update `QuickSlotComponent` to use `ItemUseService`
3. Update UI to use GAS cooldown events
4. Create developer documentation

**Validation:**
- [ ] Old ammo loading path removed
- [ ] QuickSlot keys work via new system
- [ ] UI shows progress/cooldown correctly

---

## 6. Native Tags Specification

### Tag Hierarchy

```ini
; ═══════════════════════════════════════════════════════════════════════════
; Config/DefaultGameplayTags.ini - ADD THESE TAGS
; ═══════════════════════════════════════════════════════════════════════════

; --- Handler Tags ---
+GameplayTagList=(Tag="ItemUse.Handler",DevComment="Base handler tag")
+GameplayTagList=(Tag="ItemUse.Handler.AmmoToMagazine",DevComment="Ammo -> Magazine loading")
+GameplayTagList=(Tag="ItemUse.Handler.MagazineSwap",DevComment="QuickSlot magazine swap")
+GameplayTagList=(Tag="ItemUse.Handler.Medical",DevComment="Medical item usage")
+GameplayTagList=(Tag="ItemUse.Handler.Grenade",DevComment="Grenade prepare/throw")
+GameplayTagList=(Tag="ItemUse.Handler.Container",DevComment="Container open/close")

; --- Event Tags ---
+GameplayTagList=(Tag="SuspenseCore.Event.ItemUse.Started",DevComment="Item use started")
+GameplayTagList=(Tag="SuspenseCore.Event.ItemUse.Progress",DevComment="Item use progress update")
+GameplayTagList=(Tag="SuspenseCore.Event.ItemUse.Completed",DevComment="Item use completed")
+GameplayTagList=(Tag="SuspenseCore.Event.ItemUse.Cancelled",DevComment="Item use cancelled")
+GameplayTagList=(Tag="SuspenseCore.Event.ItemUse.Failed",DevComment="Item use failed")

; --- State Tags ---
+GameplayTagList=(Tag="State.ItemUse.InProgress",DevComment="Currently using item")
+GameplayTagList=(Tag="State.ItemUse.Cooldown",DevComment="Item use on cooldown")

; --- Ability Tags ---
+GameplayTagList=(Tag="SuspenseCore.Ability.ItemUse",DevComment="Base item use ability")
+GameplayTagList=(Tag="SuspenseCore.Ability.QuickSlotUse",DevComment="QuickSlot use ability")

; --- Data Tags (for SetByCaller) ---
+GameplayTagList=(Tag="Data.ItemUse.Duration",DevComment="Duration for timed use")
+GameplayTagList=(Tag="Data.ItemUse.Cooldown",DevComment="Cooldown after use")
+GameplayTagList=(Tag="Data.ItemUse.Progress",DevComment="Current progress 0-1")
```

### Native Tags Header

**File:** `Source/BridgeSystem/Public/SuspenseCore/Tags/SuspenseCoreItemUseNativeTags.h`

```cpp
// SuspenseCoreItemUseNativeTags.h
// Native GameplayTags for Item Use System
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

namespace SuspenseCoreItemUseTags
{
    //==================================================================
    // Handler Tags
    //==================================================================

    namespace Handler
    {
        BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Handler);
        BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Handler_AmmoToMagazine);
        BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Handler_MagazineSwap);
        BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Handler_Medical);
        BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Handler_Grenade);
        BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Handler_Container);
    }

    //==================================================================
    // Event Tags
    //==================================================================

    namespace Event
    {
        BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Event_Started);
        BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Event_Progress);
        BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Event_Completed);
        BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Event_Cancelled);
        BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Event_Failed);
    }

    //==================================================================
    // State Tags
    //==================================================================

    namespace State
    {
        BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_ItemUse_InProgress);
        BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_ItemUse_Cooldown);
    }

    //==================================================================
    // Ability Tags
    //==================================================================

    namespace Ability
    {
        BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_ItemUse);
        BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_QuickSlotUse);
    }

    //==================================================================
    // Data Tags (SetByCaller)
    //==================================================================

    namespace Data
    {
        BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_ItemUse_Duration);
        BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_ItemUse_Cooldown);
        BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_ItemUse_Progress);
    }
}
```

---

## 7. Type Definitions

### File: `SuspenseCoreItemUseTypes.h`

```cpp
// SuspenseCoreItemUseTypes.h
// Item Use System - Core Types
// Copyright Suspense Team. All Rights Reserved.
//
// LOCATION: BridgeSystem (shared types - no module dependencies)
// USAGE: Used by GAS abilities and EquipmentSystem handlers
//
// This file is in BridgeSystem to avoid circular dependencies:
//   - GAS needs these types to define abilities
//   - EquipmentSystem needs these types for handlers
//   - Neither module depends on the other

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/Items/SuspenseCoreItemTypes.h"
#include "SuspenseCoreItemUseTypes.generated.h"

/**
 * Result of item use operation
 */
UENUM(BlueprintType)
enum class ESuspenseCoreItemUseResult : uint8
{
    /** Operation completed successfully */
    Success                     UMETA(DisplayName = "Success"),

    /** Operation started, will complete async (time-based) */
    InProgress                  UMETA(DisplayName = "In Progress"),

    /** Items are not compatible for this operation */
    Failed_IncompatibleItems    UMETA(DisplayName = "Incompatible Items"),

    /** Target is full (magazine, container) */
    Failed_TargetFull           UMETA(DisplayName = "Target Full"),

    /** Item/slot is on cooldown */
    Failed_OnCooldown           UMETA(DisplayName = "On Cooldown"),

    /** Item cannot be used (wrong context) */
    Failed_NotUsable            UMETA(DisplayName = "Not Usable"),

    /** Missing requirement (ammo type, health condition) */
    Failed_MissingRequirement   UMETA(DisplayName = "Missing Requirement"),

    /** Operation was cancelled by user or damage */
    Cancelled                   UMETA(DisplayName = "Cancelled"),

    /** No handler found for this item combination */
    Failed_NoHandler            UMETA(DisplayName = "No Handler"),

    /** Security validation failed */
    Failed_SecurityDenied       UMETA(DisplayName = "Security Denied"),

    /** System error */
    Failed_SystemError          UMETA(DisplayName = "System Error")
};

/**
 * Context that triggered the item use
 */
UENUM(BlueprintType)
enum class ESuspenseCoreItemUseContext : uint8
{
    /** Double-click on item (consume, open) */
    DoubleClick     UMETA(DisplayName = "Double Click"),

    /** Drag item A onto item B */
    DragDrop        UMETA(DisplayName = "Drag & Drop"),

    /** Use from QuickSlot (keys 4-7) */
    QuickSlot       UMETA(DisplayName = "QuickSlot Hotkey"),

    /** Direct hotkey (F for interact) */
    Hotkey          UMETA(DisplayName = "Direct Hotkey"),

    /** Right-click context menu */
    ContextMenu     UMETA(DisplayName = "Context Menu"),

    /** Programmatic use (script/AI) */
    Programmatic    UMETA(DisplayName = "Programmatic")
};

/**
 * Handler priority for resolving conflicts
 */
UENUM(BlueprintType)
enum class ESuspenseCoreHandlerPriority : uint8
{
    Low = 0,
    Normal = 50,
    High = 100,
    Critical = 200
};

/**
 * Item use request - input to ItemUseService
 *
 * This struct encapsulates ALL information needed to execute an item use:
 * - Source item (what is being used)
 * - Target item (for drag&drop operations)
 * - Context (how the use was triggered)
 * - Additional metadata
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreItemUseRequest
{
    GENERATED_BODY()

    //==================================================================
    // Source Item (required)
    //==================================================================

    /** Item being used (source) - REQUIRED */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemUse|Source")
    FSuspenseCoreItemInstance SourceItem;

    /** Source location index (inventory slot, equipment slot) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemUse|Source")
    int32 SourceSlotIndex = INDEX_NONE;

    /** Source container type tag */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemUse|Source",
        meta = (Categories = "Container"))
    FGameplayTag SourceContainerTag;

    //==================================================================
    // Target Item (optional - for DragDrop)
    //==================================================================

    /** Target item (for drag&drop operations) - OPTIONAL */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemUse|Target")
    FSuspenseCoreItemInstance TargetItem;

    /** Target location index */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemUse|Target")
    int32 TargetSlotIndex = INDEX_NONE;

    /** Target container type tag */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemUse|Target",
        meta = (Categories = "Container"))
    FGameplayTag TargetContainerTag;

    //==================================================================
    // Context
    //==================================================================

    /** How this use was triggered */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemUse|Context")
    ESuspenseCoreItemUseContext Context = ESuspenseCoreItemUseContext::DoubleClick;

    /** Quantity to use (for stackable items) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemUse|Context",
        meta = (ClampMin = "1"))
    int32 Quantity = 1;

    /** QuickSlot index if Context == QuickSlot (0-3) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemUse|Context")
    int32 QuickSlotIndex = INDEX_NONE;

    /** Request timestamp (for cooldown/debounce) */
    UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Context")
    float RequestTime = 0.0f;

    /** Unique request ID for tracking async operations */
    UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Context")
    FGuid RequestID;

    /** Requesting actor (player controller or pawn) */
    UPROPERTY(BlueprintReadWrite, Category = "ItemUse|Context")
    TWeakObjectPtr<AActor> RequestingActor;

    //==================================================================
    // Methods
    //==================================================================

    FSuspenseCoreItemUseRequest()
    {
        RequestID = FGuid::NewGuid();
        RequestTime = 0.0f;
    }

    /** Is this a DragDrop operation with target? */
    bool HasTarget() const { return TargetItem.IsValid(); }

    /** Is this from QuickSlot? */
    bool IsFromQuickSlot() const { return Context == ESuspenseCoreItemUseContext::QuickSlot; }

    /** Is this from UI (double-click, drag&drop, context menu)? */
    bool IsFromUI() const
    {
        return Context == ESuspenseCoreItemUseContext::DoubleClick ||
               Context == ESuspenseCoreItemUseContext::DragDrop ||
               Context == ESuspenseCoreItemUseContext::ContextMenu;
    }

    /** Debug string */
    FString ToString() const
    {
        return FString::Printf(TEXT("UseRequest[%s]: Source=%s, Target=%s, Context=%d, QuickSlot=%d"),
            *RequestID.ToString().Left(8),
            *SourceItem.ItemID.ToString(),
            HasTarget() ? *TargetItem.ItemID.ToString() : TEXT("None"),
            static_cast<int32>(Context),
            QuickSlotIndex);
    }
};

/**
 * Item use response - output from ItemUseService
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreItemUseResponse
{
    GENERATED_BODY()

    /** Result of the operation */
    UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Response")
    ESuspenseCoreItemUseResult Result = ESuspenseCoreItemUseResult::Failed_SystemError;

    /** Request ID this response is for */
    UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Response")
    FGuid RequestID;

    /** Human-readable error/status message */
    UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Response")
    FText Message;

    /** Duration for time-based operations (0 = instant) */
    UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Response")
    float Duration = 0.0f;

    /** Cooldown to apply after completion */
    UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Response")
    float Cooldown = 0.0f;

    /** Progress (0.0 - 1.0) for in-progress operations */
    UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Response")
    float Progress = 0.0f;

    /** Handler tag that processed this request */
    UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Response",
        meta = (Categories = "ItemUse.Handler"))
    FGameplayTag HandlerTag;

    /** Modified source item (updated state after use) */
    UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Response")
    FSuspenseCoreItemInstance ModifiedSourceItem;

    /** Modified target item (if applicable) */
    UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Response")
    FSuspenseCoreItemInstance ModifiedTargetItem;

    /** Additional data (handler-specific) */
    UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Response")
    TMap<FString, FString> Metadata;

    //==================================================================
    // Factory Methods
    //==================================================================

    static FSuspenseCoreItemUseResponse Success(const FGuid& InRequestID, float InDuration = 0.0f)
    {
        FSuspenseCoreItemUseResponse Response;
        Response.Result = InDuration > 0.0f ?
            ESuspenseCoreItemUseResult::InProgress :
            ESuspenseCoreItemUseResult::Success;
        Response.RequestID = InRequestID;
        Response.Duration = InDuration;
        Response.Progress = InDuration > 0.0f ? 0.0f : 1.0f;
        return Response;
    }

    static FSuspenseCoreItemUseResponse Failure(
        const FGuid& InRequestID,
        ESuspenseCoreItemUseResult InResult,
        const FText& InMessage)
    {
        FSuspenseCoreItemUseResponse Response;
        Response.Result = InResult;
        Response.RequestID = InRequestID;
        Response.Message = InMessage;
        return Response;
    }

    bool IsSuccess() const
    {
        return Result == ESuspenseCoreItemUseResult::Success ||
               Result == ESuspenseCoreItemUseResult::InProgress;
    }

    bool IsInProgress() const
    {
        return Result == ESuspenseCoreItemUseResult::InProgress;
    }
};

/**
 * EventBus event data for item use events
 * Sent via EventBus with tags from SuspenseCoreItemUseTags::Event
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreItemUseEventData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Event")
    FSuspenseCoreItemUseRequest Request;

    UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Event")
    FSuspenseCoreItemUseResponse Response;

    UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Event")
    TWeakObjectPtr<AActor> OwnerActor;
};
```

---

## 8. Interface Specifications

### ISuspenseCoreItemUseHandler

**File:** `Source/BridgeSystem/Public/SuspenseCore/Interfaces/ItemUse/ISuspenseCoreItemUseHandler.h`

```cpp
// ISuspenseCoreItemUseHandler.h
// Handler interface for Item Use operations
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE NOTE:
// This interface lives in BridgeSystem so both GAS and EquipmentSystem
// can reference it without creating circular dependencies.
//
// Handlers are registered with ItemUseService and process specific
// item type combinations (e.g., Ammo→Magazine, Medical use).

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SuspenseCore/Types/ItemUse/SuspenseCoreItemUseTypes.h"
#include "ISuspenseCoreItemUseHandler.generated.h"

UINTERFACE(MinimalAPI, BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class USuspenseCoreItemUseHandler : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for item use handlers
 *
 * IMPLEMENTATION RULES (per SuspenseCoreArchitecture.md):
 * 1. Handlers MUST be stateless (no member variables that persist between calls)
 * 2. All state goes to ItemUseService or GAS ability
 * 3. Validation MUST be idempotent (same input = same output)
 * 4. Execute MUST publish events to EventBus on completion
 * 5. Time-based operations return InProgress and let ability handle timing
 */
class BRIDGESYSTEM_API ISuspenseCoreItemUseHandler
{
    GENERATED_BODY()

public:
    //==================================================================
    // Handler Identity
    //==================================================================

    /**
     * Get unique handler tag
     * Format: ItemUse.Handler.{HandlerName}
     * Example: ItemUse.Handler.AmmoToMagazine
     */
    virtual FGameplayTag GetHandlerTag() const = 0;

    /**
     * Get handler priority for conflict resolution
     * Higher priority handlers are checked first
     * Default: Normal (50)
     */
    virtual ESuspenseCoreHandlerPriority GetPriority() const
    {
        return ESuspenseCoreHandlerPriority::Normal;
    }

    /**
     * Get display name for UI/debugging
     */
    virtual FText GetDisplayName() const
    {
        return FText::FromString(GetHandlerTag().ToString());
    }

    //==================================================================
    // Supported Item Types
    //==================================================================

    /**
     * Get item type tags this handler supports as SOURCE
     * e.g., Item.Ammo for AmmoToMagazineHandler
     * Return empty = handler doesn't filter by source type
     */
    virtual FGameplayTagContainer GetSupportedSourceTags() const = 0;

    /**
     * Get item type tags this handler supports as TARGET (for DragDrop)
     * Empty = handler doesn't require target (DoubleClick use)
     * Return empty for single-item operations
     */
    virtual FGameplayTagContainer GetSupportedTargetTags() const
    {
        return FGameplayTagContainer();
    }

    /**
     * Get supported use contexts
     * Default: DoubleClick only
     * Override for handlers that support multiple contexts
     */
    virtual TArray<ESuspenseCoreItemUseContext> GetSupportedContexts() const
    {
        return { ESuspenseCoreItemUseContext::DoubleClick };
    }

    //==================================================================
    // Validation (MUST be idempotent)
    //==================================================================

    /**
     * Quick check if this handler CAN process the request
     * Called by ItemUseService to find appropriate handler
     * MUST be fast - no complex validation here
     *
     * @param Request The use request to check
     * @return true if this handler might be able to process this request
     */
    virtual bool CanHandle(const FSuspenseCoreItemUseRequest& Request) const = 0;

    /**
     * Full validation before execution
     * Called after CanHandle returns true
     *
     * @param Request The use request
     * @param OutResponse Pre-filled with error if validation fails
     * @return true if request is valid for execution
     */
    virtual bool ValidateRequest(
        const FSuspenseCoreItemUseRequest& Request,
        FSuspenseCoreItemUseResponse& OutResponse) const = 0;

    //==================================================================
    // Execution
    //==================================================================

    /**
     * Execute the item use operation
     * Called after ValidateRequest returns true
     *
     * For instant operations: Return Success, modify items immediately
     * For time-based operations: Return InProgress with Duration set
     *
     * @param Request The validated use request
     * @param OwnerActor Actor performing the use (for GAS/components)
     * @return Response with result and modified items
     */
    virtual FSuspenseCoreItemUseResponse Execute(
        const FSuspenseCoreItemUseRequest& Request,
        AActor* OwnerActor) = 0;

    /**
     * Get expected duration for time-based operations
     * Called before Execute to show UI feedback
     *
     * @param Request The use request
     * @return Duration in seconds (0 = instant)
     */
    virtual float GetDuration(const FSuspenseCoreItemUseRequest& Request) const
    {
        return 0.0f;
    }

    /**
     * Get cooldown to apply after completion
     * Used by GAS ability to apply cooldown effect
     *
     * @param Request The use request
     * @return Cooldown in seconds (0 = no cooldown)
     */
    virtual float GetCooldown(const FSuspenseCoreItemUseRequest& Request) const
    {
        return 0.0f;
    }

    /**
     * Called when operation is cancelled mid-progress
     * Only called for time-based operations (InProgress)
     *
     * @param RequestID The request to cancel
     * @return true if cancelled, false if already completed or not found
     */
    virtual bool CancelOperation(const FGuid& RequestID)
    {
        return false; // Default: not cancellable or instant
    }

    /**
     * Is this handler's operations cancellable?
     * If true, user can interrupt via damage/movement
     */
    virtual bool IsCancellable() const { return false; }
};
```

### ISuspenseCoreItemUseService

**File:** `Source/BridgeSystem/Public/SuspenseCore/Interfaces/ItemUse/ISuspenseCoreItemUseService.h`

```cpp
// ISuspenseCoreItemUseService.h
// Service interface for Item Use System
// Copyright Suspense Team. All Rights Reserved.
//
// SSOT (Single Source of Truth) for all item use operations
// Per SuspenseCoreArchitecture.md §SSOT Pattern

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SuspenseCore/Types/ItemUse/SuspenseCoreItemUseTypes.h"
#include "ISuspenseCoreItemUseService.generated.h"

class ISuspenseCoreItemUseHandler;

UINTERFACE(MinimalAPI, BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class USuspenseCoreItemUseService : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for Item Use Service
 *
 * This is the SSOT for all item use operations.
 * All item use requests MUST go through this service.
 *
 * Access via ServiceProvider:
 *   USuspenseCoreServiceProvider* Provider = USuspenseCoreServiceProvider::Get(this);
 *   ISuspenseCoreItemUseService* ItemUseService = Provider->GetItemUseService();
 */
class BRIDGESYSTEM_API ISuspenseCoreItemUseService
{
    GENERATED_BODY()

public:
    //==================================================================
    // Handler Registration
    //==================================================================

    /**
     * Register a handler
     * @param Handler Handler instance
     * @return true if registered successfully
     */
    virtual bool RegisterHandler(TScriptInterface<ISuspenseCoreItemUseHandler> Handler) = 0;

    /**
     * Unregister a handler by tag
     */
    virtual bool UnregisterHandler(FGameplayTag HandlerTag) = 0;

    /**
     * Get all registered handler tags
     */
    virtual TArray<FGameplayTag> GetRegisteredHandlers() const = 0;

    //==================================================================
    // Validation
    //==================================================================

    /**
     * Check if an item can be used
     * Use before showing "Use" option in UI
     */
    virtual bool CanUseItem(const FSuspenseCoreItemUseRequest& Request) const = 0;

    /**
     * Get detailed validation result
     * Returns failure reason if item cannot be used
     */
    virtual FSuspenseCoreItemUseResponse ValidateUseRequest(
        const FSuspenseCoreItemUseRequest& Request) const = 0;

    /**
     * Get expected duration for an operation
     * Returns 0 for instant operations
     */
    virtual float GetUseDuration(const FSuspenseCoreItemUseRequest& Request) const = 0;

    //==================================================================
    // Execution
    //==================================================================

    /**
     * Execute item use operation
     *
     * This is the main entry point for all item usage.
     * Routes to appropriate handler based on item types.
     *
     * @param Request The use request
     * @param OwnerActor Actor performing the use
     * @return Response with result
     */
    virtual FSuspenseCoreItemUseResponse UseItem(
        const FSuspenseCoreItemUseRequest& Request,
        AActor* OwnerActor) = 0;

    /**
     * Cancel an in-progress operation
     */
    virtual bool CancelUse(const FGuid& RequestID) = 0;

    /**
     * Check if an operation is in progress
     */
    virtual bool IsOperationInProgress(const FGuid& RequestID) const = 0;

    //==================================================================
    // QuickSlot Helpers
    //==================================================================

    /**
     * Use item from QuickSlot by index
     * Builds request automatically from QuickSlot data
     *
     * @param QuickSlotIndex Slot index (0-3)
     * @param OwnerActor Actor with QuickSlotComponent
     * @return Response with result
     */
    virtual FSuspenseCoreItemUseResponse UseQuickSlot(int32 QuickSlotIndex, AActor* OwnerActor) = 0;
};
```

---

## 9. Service Implementation

### SuspenseCoreItemUseService

**Location:** `Source/EquipmentSystem/Public/SuspenseCore/Services/SuspenseCoreItemUseService.h`

```cpp
// SuspenseCoreItemUseService.h
// SSOT Service for all item use operations
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE COMPLIANCE:
// - Implements ISuspenseCoreItemUseService from BridgeSystem
// - Registers with USuspenseCoreServiceProvider
// - Publishes events to EventBus
// - Uses Native Tags from SuspenseCoreItemUseNativeTags.h
// - Security validation via USuspenseCoreSecurityValidator

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SuspenseCore/Interfaces/ItemUse/ISuspenseCoreItemUseService.h"
#include "SuspenseCore/Types/ItemUse/SuspenseCoreItemUseTypes.h"
#include "SuspenseCoreItemUseService.generated.h"

class ISuspenseCoreItemUseHandler;
class USuspenseCoreEventBus;
class USuspenseCoreSecurityValidator;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnItemUseStarted,
    const FSuspenseCoreItemUseRequest&, Request,
    float, Duration
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnItemUseCompleted,
    const FSuspenseCoreItemUseRequest&, Request,
    const FSuspenseCoreItemUseResponse&, Response
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnItemUseCancelled,
    const FGuid&, RequestID,
    const FText&, Reason
);

/**
 * USuspenseCoreItemUseService
 *
 * SINGLE SOURCE OF TRUTH for all item use operations.
 *
 * Per SuspenseCoreArchitecture.md:
 * - All item use MUST go through this service
 * - Handlers register here and are called by routing logic
 * - Events published to EventBus for UI/camera/sound
 * - Security validated via USuspenseCoreSecurityValidator
 *
 * Usage:
 *   // Via ServiceProvider (recommended)
 *   USuspenseCoreServiceProvider* Provider = USuspenseCoreServiceProvider::Get(this);
 *   ISuspenseCoreItemUseService* Service = Provider->GetItemUseService();
 *
 *   // Or via static Get()
 *   USuspenseCoreItemUseService* Service = USuspenseCoreItemUseService::Get(this);
 */
UCLASS()
class EQUIPMENTSYSTEM_API USuspenseCoreItemUseService
    : public UGameInstanceSubsystem
    , public ISuspenseCoreItemUseService
{
    GENERATED_BODY()

public:
    //==================================================================
    // Subsystem Interface
    //==================================================================

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Get service instance (static accessor) */
    static USuspenseCoreItemUseService* Get(const UObject* WorldContext);

    //==================================================================
    // ISuspenseCoreItemUseService Interface
    //==================================================================

    // Handler Registration
    virtual bool RegisterHandler(TScriptInterface<ISuspenseCoreItemUseHandler> Handler) override;
    virtual bool UnregisterHandler(FGameplayTag HandlerTag) override;
    virtual TArray<FGameplayTag> GetRegisteredHandlers() const override;

    // Validation
    virtual bool CanUseItem(const FSuspenseCoreItemUseRequest& Request) const override;
    virtual FSuspenseCoreItemUseResponse ValidateUseRequest(
        const FSuspenseCoreItemUseRequest& Request) const override;
    virtual float GetUseDuration(const FSuspenseCoreItemUseRequest& Request) const override;

    // Execution
    virtual FSuspenseCoreItemUseResponse UseItem(
        const FSuspenseCoreItemUseRequest& Request,
        AActor* OwnerActor) override;
    virtual bool CancelUse(const FGuid& RequestID) override;
    virtual bool IsOperationInProgress(const FGuid& RequestID) const override;

    // QuickSlot Helpers
    virtual FSuspenseCoreItemUseResponse UseQuickSlot(int32 QuickSlotIndex, AActor* OwnerActor) override;

    //==================================================================
    // Events (Blueprint Bindable)
    //==================================================================

    UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|ItemUse|Events")
    FOnItemUseStarted OnItemUseStarted;

    UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|ItemUse|Events")
    FOnItemUseCompleted OnItemUseCompleted;

    UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|ItemUse|Events")
    FOnItemUseCancelled OnItemUseCancelled;

protected:
    //==================================================================
    // Internal Methods
    //==================================================================

    /** Auto-register built-in handlers on Initialize */
    void AutoRegisterHandlers();

    /** Find handler for request (sorted by priority) */
    ISuspenseCoreItemUseHandler* FindHandler(const FSuspenseCoreItemUseRequest& Request) const;

    /** Sort handlers by priority (highest first) */
    void SortHandlersByPriority();

    /** Publish event to EventBus */
    void PublishEventBusEvent(
        FGameplayTag EventTag,
        const FSuspenseCoreItemUseRequest& Request,
        const FSuspenseCoreItemUseResponse* Response = nullptr);

    /** Get EventBus via ServiceProvider */
    USuspenseCoreEventBus* GetEventBus() const;

    /** Get SecurityValidator via ServiceProvider */
    USuspenseCoreSecurityValidator* GetSecurityValidator() const;

    /** Validate security (authority, rate limit) */
    bool ValidateSecurity(AActor* OwnerActor, const FString& OperationName) const;

private:
    /** Registered handlers (sorted by priority, highest first) */
    UPROPERTY()
    TArray<TScriptInterface<ISuspenseCoreItemUseHandler>> Handlers;

    /** Active operations map (RequestID -> Handler) */
    TMap<FGuid, ISuspenseCoreItemUseHandler*> ActiveOperations;

    /** Pending operations (for async completion tracking) */
    TMap<FGuid, FSuspenseCoreItemUseRequest> PendingRequests;
};
```

---

## 10. GAS Ability Implementation

### GA_ItemUse (Base Ability)

**Key Features:**
- Event-triggered (receives `FSuspenseCoreItemUseRequest` via EventData)
- Uses GameplayEffects for cooldown (not tick-based!)
- Publishes progress to EventBus for UI
- Cancellable via tags (State.Dead, State.Stunned)

```cpp
// GA_ItemUse.h (header outline)
UCLASS(Abstract)
class GAS_API UGA_ItemUse : public USuspenseCoreGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_ItemUse();

protected:
    //==================================================================
    // Configuration (Blueprint Editable)
    //==================================================================

    /** GameplayEffect for in-progress state */
    UPROPERTY(EditDefaultsOnly, Category = "ItemUse|Effects")
    TSubclassOf<UGameplayEffect> InProgressEffectClass;

    /** GameplayEffect for cooldown */
    UPROPERTY(EditDefaultsOnly, Category = "ItemUse|Effects")
    TSubclassOf<UGameplayEffect> CooldownEffectClass;

    /** Tags that block this ability */
    UPROPERTY(EditDefaultsOnly, Category = "ItemUse|Tags")
    FGameplayTagContainer BlockedByTags;

    /** Tags that cancel this ability when applied */
    UPROPERTY(EditDefaultsOnly, Category = "ItemUse|Tags")
    FGameplayTagContainer CancelledByTags;

    /** Should damage cancel this ability? */
    UPROPERTY(EditDefaultsOnly, Category = "ItemUse|Config")
    bool bCancelOnDamage = true;

    //==================================================================
    // GAS Overrides
    //==================================================================

    virtual bool CanActivateAbility(...) const override;
    virtual void ActivateAbility(...) override;
    virtual void EndAbility(...) override;
    virtual void CancelAbility(...) override;

    //==================================================================
    // Execution Flow
    //==================================================================

    /** Parse FSuspenseCoreItemUseRequest from EventData */
    bool ParseRequestFromEventData(const FGameplayEventData* EventData);

    /** Execute instant use (no duration) */
    void ExecuteInstantUse();

    /** Execute timed use (starts wait task) */
    void ExecuteTimedUse(float Duration);

    /** Called when timed use completes */
    UFUNCTION()
    void OnTimedUseComplete();

    /** Apply cooldown effect */
    void ApplyCooldown(float CooldownDuration);

    /** Report progress to EventBus (for UI) */
    void ReportProgress(float Progress);

    //==================================================================
    // State
    //==================================================================

    UPROPERTY()
    FSuspenseCoreItemUseRequest CurrentRequest;

    UPROPERTY()
    FSuspenseCoreItemUseResponse CurrentResponse;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitDelay> WaitTask;

    FActiveGameplayEffectHandle InProgressEffectHandle;
};
```

### GA_QuickSlotUse (QuickSlot Specialization)

```cpp
// GA_QuickSlotUse.h (outline)
// Triggered by keys 4-5-6-7
// Builds FSuspenseCoreItemUseRequest from QuickSlotComponent data

UCLASS()
class GAS_API UGA_QuickSlotUse : public UGA_ItemUse
{
    GENERATED_BODY()

public:
    UGA_QuickSlotUse();

protected:
    /** Parse slot index from EventData magnitude */
    int32 GetSlotIndexFromEventData(const FGameplayEventData* EventData) const;

    /** Build request from QuickSlotComponent */
    bool BuildRequestFromQuickSlot(int32 SlotIndex);

    virtual void ActivateAbility(...) override;
};
```

---

## 11. Handler Implementations

### Handler: AmmoToMagazine

**Purpose:** Load ammo into magazine via drag&drop

```cpp
// SuspenseCoreAmmoToMagazineHandler.h (outline)
// Replaces tick-based AmmoLoadingService logic

class USuspenseCoreAmmoToMagazineHandler
    : public UObject
    , public ISuspenseCoreItemUseHandler
{
    // Handler Implementation
    virtual FGameplayTag GetHandlerTag() const override
    {
        return SuspenseCoreItemUseTags::Handler::TAG_ItemUse_Handler_AmmoToMagazine;
    }

    virtual FGameplayTagContainer GetSupportedSourceTags() const override
    {
        // Item.Ammo.* tags
        FGameplayTagContainer Tags;
        Tags.AddTag(FGameplayTag::RequestGameplayTag("Item.Ammo"));
        return Tags;
    }

    virtual FGameplayTagContainer GetSupportedTargetTags() const override
    {
        // Item.Magazine tags
        FGameplayTagContainer Tags;
        Tags.AddTag(FGameplayTag::RequestGameplayTag("Item.Category.Magazine"));
        return Tags;
    }

    virtual TArray<ESuspenseCoreItemUseContext> GetSupportedContexts() const override
    {
        return { ESuspenseCoreItemUseContext::DragDrop };
    }

    virtual float GetDuration(const FSuspenseCoreItemUseRequest& Request) const override
    {
        // TimePerRound * RoundsToLoad
        // Get from MagazineData in DataManager
        return CalculateLoadDuration(Request);
    }

    // CanHandle, ValidateRequest, Execute...
};
```

### Handler: MagazineSwap

**Purpose:** Swap magazine via QuickSlot (keys 4-7)

```cpp
// SuspenseCoreMagazineSwapHandler.h (outline)
// Uses existing SwapMagazineFromQuickSlot() from MagazineComponent

class USuspenseCoreMagazineSwapHandler
    : public UObject
    , public ISuspenseCoreItemUseHandler
{
    virtual FGameplayTag GetHandlerTag() const override
    {
        return SuspenseCoreItemUseTags::Handler::TAG_ItemUse_Handler_MagazineSwap;
    }

    virtual TArray<ESuspenseCoreItemUseContext> GetSupportedContexts() const override
    {
        return { ESuspenseCoreItemUseContext::QuickSlot };
    }

    virtual FGameplayTagContainer GetSupportedSourceTags() const override
    {
        FGameplayTagContainer Tags;
        Tags.AddTag(FGameplayTag::RequestGameplayTag("Item.Category.Magazine"));
        return Tags;
    }

    // Execute calls MagazineComponent->SwapMagazineFromQuickSlot()
};
```

---

## 12. Migration from Tick-Based to GAS Cooldowns

### Current Implementation (Tick-Based)

**File:** `SuspenseCoreAmmoLoadingService.cpp`

```cpp
// CURRENT (ANTI-PATTERN):
void USuspenseCoreAmmoLoadingService::TickComponent(float DeltaTime, ...)
{
    if (bIsLoading)
    {
        LoadProgress += DeltaTime;
        if (LoadProgress >= TimePerRound)
        {
            LoadNextRound();
            LoadProgress = 0.0f;
        }
    }
}
```

### New Implementation (GAS-Based)

```cpp
// NEW (GAS ABILITY):
void UGA_ItemUse::ExecuteTimedUse(float Duration)
{
    // 1. Apply in-progress GameplayEffect (adds State.ItemUse.InProgress tag)
    ApplyInProgressEffect();

    // 2. Start wait task
    WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, Duration);
    WaitTask->OnFinish.AddDynamic(this, &UGA_ItemUse::OnTimedUseComplete);
    WaitTask->ReadyForActivation();

    // 3. Progress reporting task (optional, for UI)
    StartProgressReportingTask(Duration);
}

void UGA_ItemUse::OnTimedUseComplete()
{
    // 1. Execute handler completion
    ISuspenseCoreItemUseService* Service = GetItemUseService();
    // Handler's Execute() already called at start, this just signals completion

    // 2. Apply cooldown GameplayEffect
    ApplyCooldown(CurrentResponse.Cooldown);

    // 3. Publish completion event
    PublishEventBusEvent(SuspenseCoreItemUseTags::Event::TAG_ItemUse_Event_Completed);

    // 4. End ability
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
```

### UI Integration with GAS Cooldowns

```cpp
// UI subscribes to EventBus instead of polling tick:
void UMyWidget::SetupEventSubscriptions()
{
    EventBus->Subscribe(
        SuspenseCoreItemUseTags::Event::TAG_ItemUse_Event_Started,
        FOnSuspenseCoreEvent::CreateUObject(this, &UMyWidget::OnItemUseStarted)
    );

    EventBus->Subscribe(
        SuspenseCoreItemUseTags::Event::TAG_ItemUse_Event_Progress,
        FOnSuspenseCoreEvent::CreateUObject(this, &UMyWidget::OnItemUseProgress)
    );
}

void UMyWidget::OnItemUseProgress(FGameplayTag Tag, const FSuspenseCoreEventData& Data)
{
    float Progress = Data.GetFloat(TEXT("Progress"));
    UpdateProgressBar(Progress);
}
```

---

## 13. Integration Points

### QuickSlotComponent Integration

**Modify:** `SuspenseCoreQuickSlotComponent.cpp`

```cpp
// BEFORE:
bool USuspenseCoreQuickSlotComponent::UseQuickSlot(int32 SlotIndex)
{
    // Direct logic here...
}

// AFTER:
bool USuspenseCoreQuickSlotComponent::UseQuickSlot(int32 SlotIndex)
{
    // Route through ItemUseService
    ISuspenseCoreItemUseService* Service = GetItemUseService();
    if (!Service) return false;

    FSuspenseCoreItemUseResponse Response = Service->UseQuickSlot(SlotIndex, GetOwner());
    return Response.IsSuccess();
}
```

### HUD QuickSlot Binding

**Modify:** HUD Blueprint to trigger `GA_QuickSlotUse` ability instead of direct calls

```
Input Key 4 → TryActivateAbilityByClass(GA_QuickSlotUse, SlotIndex=0)
Input Key 5 → TryActivateAbilityByClass(GA_QuickSlotUse, SlotIndex=1)
Input Key 6 → TryActivateAbilityByClass(GA_QuickSlotUse, SlotIndex=2)
Input Key 7 → TryActivateAbilityByClass(GA_QuickSlotUse, SlotIndex=3)
```

### ServiceProvider Registration

**Modify:** `USuspenseCoreServiceProvider::Initialize()`

```cpp
void USuspenseCoreServiceProvider::Initialize(FSubsystemCollectionBase& Collection)
{
    // Existing services...

    // Register ItemUseService
    ItemUseService = NewObject<USuspenseCoreItemUseService>(this);
    ItemUseService->Initialize(Collection);
}

ISuspenseCoreItemUseService* USuspenseCoreServiceProvider::GetItemUseService() const
{
    return ItemUseService;
}
```

---

## 14. Testing Strategy

### Unit Tests

| Test Case | Expected Result |
|-----------|-----------------|
| Handler registration | Handler appears in GetRegisteredHandlers() |
| Handler priority sorting | Higher priority handlers checked first |
| CanUseItem validation | Returns false for invalid requests |
| UseItem routing | Correct handler receives request |
| Cooldown application | GE applied after completion |
| Cancellation | InProgress operation cancelled, event published |

### Integration Tests

| Test Case | Expected Result |
|-----------|-----------------|
| QuickSlot 4 key press | Magazine swap executed via GA_QuickSlotUse |
| Drag ammo to magazine | AmmoToMagazineHandler processes, rounds loaded |
| Damage during reload | Ability cancelled, State.ItemUse.InProgress removed |
| UI progress bar | Updates correctly during timed operation |

### Multiplayer Tests

| Test Case | Expected Result |
|-----------|-----------------|
| Client requests use | Server validates via SecurityValidator |
| Server authority | Client cannot bypass to direct component calls |
| Replication | Modified items replicate to all clients |

---

## 15. Checklist

### Phase 1: BridgeSystem Foundation
- [x] Create `SuspenseCoreItemUseTypes.h`
- [x] Create `ISuspenseCoreItemUseHandler.h`
- [x] Create `ISuspenseCoreItemUseService.h`
- [x] Create `SuspenseCoreItemUseNativeTags.h/.cpp`
- [x] Update `DefaultGameplayTags.ini`
- [x] Verify BridgeSystem compiles

### Phase 2: Service Implementation
- [x] Create `SuspenseCoreItemUseService.h/.cpp`
- [x] Register with ServiceProvider (via GetService<> pattern)
- [x] Implement handler registration
- [x] Implement request routing
- [x] Implement EventBus publishing
- [ ] Verify EquipmentSystem compiles

### Phase 3: GAS Abilities, Effects & Input Integration
- [ ] Create `GA_ItemUse.h/.cpp` - Base item use ability
- [ ] Create `GA_QuickSlotUse.h/.cpp` - QuickSlot ability (keys 4-7)
- [ ] Create `GE_ItemUse_InProgress.h/.cpp` - C++ GameplayEffect base
- [ ] Create `GE_ItemUse_Cooldown.h/.cpp` - C++ GameplayEffect base
- [ ] Create `SuspenseCoreItemUseInputConfig.h/.cpp` - Input actions
- [ ] Modify `SuspenseCorePlayerController` - Add QuickSlot input bindings
- [ ] Modify `SuspenseCoreInputComponent` - Bind IA_QuickSlot1-4 actions
- [ ] Test input: Keys 4-5-6-7 trigger GA_QuickSlotUse
- [ ] Verify GAS module compiles
- [ ] Verify PlayerCore input integration works

### Phase 4: Handlers
- [ ] Create `SuspenseCoreAmmoToMagazineHandler`
- [ ] Create `SuspenseCoreMagazineSwapHandler`
- [ ] Create `SuspenseCoreMedicalUseHandler`
- [ ] Create `SuspenseCoreGrenadeHandler`
- [ ] Verify all handlers register correctly

### Phase 5: Integration
- [ ] Modify `QuickSlotComponent` to use service
- [ ] Modify HUD input bindings
- [ ] Remove tick from `AmmoLoadingService`
- [ ] Update UI to use EventBus
- [ ] Create developer documentation
- [ ] Full system test

---

## Document Approval

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Technical Lead | | | |
| Systems Developer | | | |
| GAS Developer | | | |
| UI Developer | | | |

---

**END OF DOCUMENT**

---

## Appendix A: Quick Reference

### Getting ItemUseService

```cpp
// Via ServiceProvider (recommended)
USuspenseCoreServiceProvider* Provider = USuspenseCoreServiceProvider::Get(this);
ISuspenseCoreItemUseService* Service = Provider->GetItemUseService();

// Via static Get()
USuspenseCoreItemUseService* Service = USuspenseCoreItemUseService::Get(this);
```

### Creating a Use Request

```cpp
FSuspenseCoreItemUseRequest Request;
Request.SourceItem = MyItem;
Request.Context = ESuspenseCoreItemUseContext::QuickSlot;
Request.QuickSlotIndex = 0;
Request.RequestingActor = GetOwner();

FSuspenseCoreItemUseResponse Response = Service->UseItem(Request, GetOwner());
```

### Subscribing to Events

```cpp
EventBus->Subscribe(
    SuspenseCoreItemUseTags::Event::TAG_ItemUse_Event_Completed,
    FOnSuspenseCoreEvent::CreateUObject(this, &MyClass::OnItemUseCompleted)
);
```

### Creating a Custom Handler

```cpp
class UMyCustomHandler : public UObject, public ISuspenseCoreItemUseHandler
{
    virtual FGameplayTag GetHandlerTag() const override
    {
        return FGameplayTag::RequestGameplayTag("ItemUse.Handler.MyCustom");
    }

    virtual FGameplayTagContainer GetSupportedSourceTags() const override
    {
        FGameplayTagContainer Tags;
        Tags.AddTag(FGameplayTag::RequestGameplayTag("Item.MyType"));
        return Tags;
    }

    virtual bool CanHandle(const FSuspenseCoreItemUseRequest& Request) const override
    {
        return Request.SourceItem.ItemTags.HasTag(
            FGameplayTag::RequestGameplayTag("Item.MyType"));
    }

    virtual bool ValidateRequest(...) const override { /* ... */ }
    virtual FSuspenseCoreItemUseResponse Execute(...) override { /* ... */ }
};
```
