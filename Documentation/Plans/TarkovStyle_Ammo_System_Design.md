# Tarkov-Style Ammo System Design

## Overview

This document describes the design for a realistic ammunition system inspired by Escape from Tarkov.
The system treats ammo as physical inventory items with detailed ballistic properties, supports
magazine-based reloading, and integrates with QuickSlots for fast tactical access.

**Author:** Claude Code
**Date:** 2026-01-04
**Status:** Phase 5 Complete (SwapMagazineFromQuickSlot + AmmoLoadingService)
**Related:** `SSOT_AttributeSet_DataTable_Integration.md`

---

## Implementation Status

### Completed Phases

| Phase | Status | Description |
|-------|--------|-------------|
| Phase 1 | DONE | Core Data Structures (Magazine types, Ammo stacks) |
| Phase 2 | DONE | MagazineComponent with full magazine operations |
| Phase 3 | DONE | QuickSlotComponent for fast magazine access |
| Phase 4 | DONE | GAS Reload Abilities (GA_Reload, GA_QuickSlot) |
| Phase 5 | DONE | SwapMagazineFromQuickSlot + AmmoLoadingService |
| Phase 6 | DONE | UI Integration (HUD, Inventory) |

### Key Files Created

```
Source/EquipmentSystem/
├── Public/SuspenseCore/
│   ├── Types/Weapon/
│   │   ├── SuspenseCoreMagazineTypes.h         # Magazine data structures
│   │   └── SuspenseCoreAmmoStackTypes.h        # Ammo stack structures
│   ├── Components/
│   │   ├── SuspenseCoreMagazineComponent.h     # Magazine management
│   │   └── SuspenseCoreQuickSlotComponent.h    # Quick access slots
│   └── Interfaces/
│       ├── ISuspenseCoreMagazineProvider.h     # Magazine interface
│       └── ISuspenseCoreQuickSlotProvider.h    # QuickSlot interface
├── Private/SuspenseCore/
│   └── Components/
│       ├── SuspenseCoreMagazineComponent.cpp
│       └── SuspenseCoreQuickSlotComponent.cpp

Source/GAS/
├── Public/SuspenseCore/Abilities/
│   ├── GA_Reload.h                              # Reload ability
│   └── GA_QuickSlot.h                           # QuickSlot ability
└── Private/SuspenseCore/Abilities/
    ├── GA_Reload.cpp
    └── GA_QuickSlot.cpp
```

### Git Commits (Phase 5)

```
09f8e58 fix(Magazine): Resolve SUSPENSECORE_QUICKSLOT_COUNT compile error
20baf51 feat(Magazine): Implement SwapMagazineFromQuickSlot with full Tarkov-style logic
```

### Phase 5 New Files

```
Source/EquipmentSystem/
├── Public/SuspenseCore/
│   └── Services/
│       └── SuspenseCoreAmmoLoadingService.h      # Ammo loading into magazines
├── Private/SuspenseCore/
│   └── Services/
│       └── SuspenseCoreAmmoLoadingService.cpp
└── Tags/
    └── SuspenseCoreEquipmentNativeTags.h/.cpp    # New ammo loading tags
```

### Phase 5 Key Changes

1. **SwapMagazineFromQuickSlot()** in MagazineComponent:
   - Full implementation with QuickSlot integration
   - Server RPC for multiplayer sync
   - EventBus publication (`TAG_Equipment_Event_Magazine_Swapped`)
   - Caliber compatibility check
   - Automatic chambering after swap

2. **USuspenseCoreAmmoLoadingService**:
   - Centralized service for loading ammo into magazines
   - Support for Drag&Drop, Quick Load (double-click)
   - Time-based loading with interruptible operations
   - EventBus integration for UI feedback

3. **New GameplayTags** (SuspenseCoreEquipmentNativeTags):
   - `TAG_Equipment_Event_Ammo_LoadStarted`
   - `TAG_Equipment_Event_Ammo_LoadCompleted`
   - `TAG_Equipment_Event_Ammo_LoadCancelled`
   - `TAG_Equipment_Event_Ammo_UnloadStarted`
   - `TAG_Equipment_Event_Ammo_UnloadCompleted`

### Git Commits (Phase 4)

```
e6c65f2 fix(compile): Fix compilation errors in Magazine and QuickSlot components
cdf1acb fix(arch): Resolve BlueprintNativeEvent interface conflicts in components
e20b5ec fix(arch): Decouple GAS from EquipmentSystem via BridgeSystem interfaces
4db8dd3 feat(GAS): Add Reload and QuickSlot abilities (Phase 4)
ee21034 feat(QuickSlot): Add QuickSlotComponent for fast magazine access (Phase 3)
51b9f53 feat(core): Integrate Tarkov-style ammo components into base classes
```

---

## Architecture Compliance

The implementation follows project architecture patterns:

| Pattern | Implementation |
|---------|----------------|
| **EventBus** | Components publish events via `USuspenseCoreEventBus` |
| **Native Tags** | `FGameplayTag` for calibers, slots, weapon types |
| **GAS** | Abilities access components via `ISuspenseCoreMagazineProvider` interface |
| **BridgeSystem** | Interfaces in BridgeSystem decouple GAS from EquipmentSystem |
| **DI/ServiceLocator** | Components accessed via getters, created in constructors |
| **SOLID/SRP** | Each component has single responsibility |
| **Conditional Compilation** | `#if WITH_EQUIPMENT_SYSTEM` for module dependency |

### Module Dependencies

```
BridgeSystem (interfaces only, no concrete types)
    ↑
    ├── GAS (uses interfaces to access components)
    │
    └── EquipmentSystem (implements interfaces)
            ↓
        PlayerCore (integrates components into Character)
```

---

## Setup Instructions

### 1. Enable Equipment System Module

In your `.uproject` or `.uplugin` file, ensure EquipmentSystem is enabled:

```json
{
    "Modules": [
        {
            "Name": "EquipmentSystem",
            "Type": "Runtime",
            "LoadingPhase": "Default"
        }
    ]
}
```

### 2. Define WITH_EQUIPMENT_SYSTEM

In your `Target.cs` or module Build.cs:

```csharp
PublicDefinitions.Add("WITH_EQUIPMENT_SYSTEM=1");
```

### 3. Component Integration (Already Done in Code)

Components are automatically created in base classes:

**ASuspenseCoreWeaponActor** (`SuspenseCoreWeaponActor.cpp:constructor`):
```cpp
// MagazineComponent is auto-created
MagazineComponent = CreateDefaultSubobject<USuspenseCoreMagazineComponent>(TEXT("MagazineComponent"));
```

**ASuspenseCoreCharacter** (`SuspenseCoreCharacter.cpp:constructor`):
```cpp
#if WITH_EQUIPMENT_SYSTEM
    QuickSlotComponent = CreateDefaultSubobject<USuspenseCoreQuickSlotComponent>(TEXT("QuickSlotComponent"));
#endif
```

### 4. Blueprint Setup (Required)

#### 4.1 Weapon Blueprint Configuration

In your weapon Blueprint (e.g., `BP_Rifle_M4`):

1. **Initialize Magazine Component:**
   ```
   Event BeginPlay
   → Get MagazineComponent
   → Call InitializeFromWeapon(Self)
   → Call InsertMagazine (if starting with magazine)
   ```

2. **Configure Initial Magazine:**
   - Set `InitialMagazineID` property on MagazineComponent
   - Or insert magazine programmatically via `InsertMagazine()`

#### 4.2 Character Blueprint Configuration

In your character Blueprint (e.g., `BP_Character_Player`):

1. **Link QuickSlot to Magazine Component:**
   ```
   Event OnWeaponEquipped (or similar)
   → Get QuickSlotComponent
   → Get Weapon's MagazineComponent
   → Call SetMagazineComponent(MagazineComponent)
   ```

2. **Initialize QuickSlots:**
   ```
   Event BeginPlay
   → Get QuickSlotComponent
   → Call InitializeSlots()
   ```

### 5. Input Binding

Add Enhanced Input actions for QuickSlots:

```cpp
// In your InputConfig DataAsset or InputMappingContext:
// Key 1 → IA_QuickSlot_1
// Key 2 → IA_QuickSlot_2
// Key 3 → IA_QuickSlot_3
// Key 4 → IA_QuickSlot_4

// In PlayerController or InputComponent setup:
EnhancedInputComponent->BindAction(IA_QuickSlot_1, ETriggerEvent::Started, this, &AMyController::OnQuickSlot1);
```

### 6. DataTable Configuration

#### 6.1 Create Magazine DataTable

Create `DT_Magazines` with row structure `FSuspenseCoreMagazineData`:

| MagazineID | Caliber | MaxCapacity | ReloadModifier | Weight |
|------------|---------|-------------|----------------|--------|
| Mag_STANAG_30 | Item.Ammo.556x45 | 30 | 1.0 | 0.13 |
| Mag_PMAG_40 | Item.Ammo.556x45 | 40 | 0.95 | 0.17 |
| Mag_Glock_17 | Item.Ammo.9x19 | 17 | 0.9 | 0.07 |

#### 6.2 Extend Ammo DataTable

Ensure `DT_AmmoAttributes` includes caliber tags:

| AmmoID | Caliber | Damage | Penetration | IsTracer |
|--------|---------|--------|-------------|----------|
| Ammo_556_M855 | Item.Ammo.556x45 | 40 | 25 | false |
| Ammo_556_M855A1 | Item.Ammo.556x45 | 42 | 38 | false |
| Ammo_9x19_FMJ | Item.Ammo.9x19 | 32 | 18 | false |

### 7. GAS Ability Setup

Grant reload abilities to your AbilitySystemComponent:

```cpp
// In character or weapon initialization:
if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
{
    ASC->GiveAbility(FGameplayAbilitySpec(UGA_Reload::StaticClass(), 1));
    ASC->GiveAbility(FGameplayAbilitySpec(UGA_QuickSlot::StaticClass(), 1));
}
```

### 8. GameplayTags Required

Ensure these tags exist in your GameplayTags configuration:

```ini
; Equipment/QuickSlots
+GameplayTags=(Tag="Equipment.QuickSlot.1")
+GameplayTags=(Tag="Equipment.QuickSlot.2")
+GameplayTags=(Tag="Equipment.QuickSlot.3")
+GameplayTags=(Tag="Equipment.QuickSlot.4")

; Item Categories
+GameplayTags=(Tag="Item.Category.Magazine")
+GameplayTags=(Tag="Item.Category.Ammo")

; Ammo Calibers
+GameplayTags=(Tag="Item.Ammo.556x45")
+GameplayTags=(Tag="Item.Ammo.9x19")
+GameplayTags=(Tag="Item.Ammo.762x39")
; ... add more as needed

; Weapon Types (for magazine compatibility)
+GameplayTags=(Tag="Weapon.Type.AssaultRifle")
+GameplayTags=(Tag="Weapon.Type.Pistol")
+GameplayTags=(Tag="Weapon.Type.SMG")
```

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

### 3.1 Magazine Component (IMPLEMENTED)

Located at: `Source/EquipmentSystem/Public/SuspenseCore/Components/SuspenseCoreMagazineComponent.h`

```cpp
UCLASS(ClassGroup=(SuspenseCore), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreMagazineComponent : public UActorComponent,
                                                           public ISuspenseCoreMagazineProvider
{
    GENERATED_BODY()

public:
    // Initialization
    UFUNCTION(BlueprintCallable, Category="Magazine")
    bool InitializeFromWeapon(TScriptInterface<ISuspenseCoreWeapon> WeaponInterface);

    // Magazine operations
    UFUNCTION(BlueprintCallable, Category="Magazine")
    bool InsertMagazine(const FSuspenseCoreMagazineInstance& Magazine);

    UFUNCTION(BlueprintCallable, Category="Magazine")
    FSuspenseCoreMagazineInstance EjectMagazine();

    UFUNCTION(BlueprintCallable, Category="Magazine")
    bool ChamberNextRound();

    // Queries
    UFUNCTION(BlueprintPure, Category="Magazine")
    int32 GetMagazineCount() const;

    UFUNCTION(BlueprintPure, Category="Magazine")
    bool HasMagazine() const;

    UFUNCTION(BlueprintPure, Category="Magazine")
    bool IsReadyToFire() const;

    // ISuspenseCoreMagazineProvider interface
    virtual bool CanReload_Implementation() const override;
    virtual bool StartReload_Implementation() override;
    virtual bool FinishReload_Implementation() override;
    virtual void CancelReload_Implementation() override;

protected:
    UPROPERTY(Replicated)
    FSuspenseCoreMagazineInstance CurrentMagazine;

    UPROPERTY(Replicated)
    FSuspenseCoreLoadedRound ChamberedRound;

    UPROPERTY(Replicated)
    bool bHasChamberedRound = false;

    UPROPERTY(EditDefaultsOnly, Category="Magazine")
    FName InitialMagazineID;
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

### 4.1 QuickSlot System Overview (IMPLEMENTED)

Located at: `Source/EquipmentSystem/Public/SuspenseCore/Components/SuspenseCoreQuickSlotComponent.h`

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

### 4.2 QuickSlot Component (IMPLEMENTED)

```cpp
UCLASS(ClassGroup=(SuspenseCore), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreQuickSlotComponent : public UActorComponent,
                                                            public ISuspenseCoreQuickSlotProvider
{
    GENERATED_BODY()

public:
    // Initialization
    UFUNCTION(BlueprintCallable, Category="QuickSlot")
    void InitializeSlots();

    UFUNCTION(BlueprintCallable, Category="QuickSlot")
    void SetMagazineComponent(USuspenseCoreMagazineComponent* InMagazineComponent);

    // Slot operations
    UFUNCTION(BlueprintCallable, Category="QuickSlot")
    bool AssignMagazineToSlot(int32 SlotIndex, const FSuspenseCoreMagazineInstance& Magazine);

    UFUNCTION(BlueprintCallable, Category="QuickSlot")
    FSuspenseCoreMagazineInstance GetMagazineFromSlot(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category="QuickSlot")
    bool SwapMagazineFromSlot(int32 SlotIndex);

    // ISuspenseCoreQuickSlotProvider interface
    virtual bool UseQuickSlot_Implementation(int32 SlotIndex) override;
    virtual bool IsSlotAvailable_Implementation(int32 SlotIndex) const override;

protected:
    UPROPERTY(Replicated)
    TArray<FSuspenseCoreQuickSlot> QuickSlots;

    UPROPERTY(Replicated)
    TArray<FSuspenseCoreMagazineInstance> StoredMagazines;

    UPROPERTY()
    TWeakObjectPtr<USuspenseCoreMagazineComponent> MagazineComponent;
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

### 5.3 Reload Ability (GAS) - IMPLEMENTED

Located at: `Source/GAS/Public/SuspenseCore/Abilities/GA_Reload.h`

```cpp
UCLASS()
class GAS_API UGA_Reload : public USuspenseCoreGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_Reload();

    virtual bool CanActivateAbility(...) const override;
    virtual void ActivateAbility(...) override;
    virtual void EndAbility(...) override;

protected:
    // Get magazine provider via BridgeSystem interface
    TScriptInterface<ISuspenseCoreMagazineProvider> GetMagazineProvider() const;

    UPROPERTY()
    EReloadType CurrentReloadType;

    UPROPERTY()
    float ReloadDuration;

    UFUNCTION()
    void OnReloadMontageNotify(FName NotifyName);
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

### 6.1 BridgeSystem Interfaces (IMPLEMENTED)

The key architectural decision is using BridgeSystem interfaces to decouple GAS from EquipmentSystem:

```cpp
// Located at: Source/BridgeSystem/Public/SuspenseCore/Interfaces/

// ISuspenseCoreMagazineProvider.h
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreMagazineProvider : public UInterface
{
    GENERATED_BODY()
};

class BRIDGESYSTEM_API ISuspenseCoreMagazineProvider
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Magazine")
    bool CanReload() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Magazine")
    bool StartReload();

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Magazine")
    bool FinishReload();

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Magazine")
    void CancelReload();

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Magazine")
    int32 GetCurrentMagazineCount() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Magazine")
    int32 GetMagazineCapacity() const;
};

// ISuspenseCoreQuickSlotProvider.h
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreQuickSlotProvider : public UInterface
{
    GENERATED_BODY()
};

class BRIDGESYSTEM_API ISuspenseCoreQuickSlotProvider
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="QuickSlot")
    bool UseQuickSlot(int32 SlotIndex);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="QuickSlot")
    bool IsSlotAvailable(int32 SlotIndex) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="QuickSlot")
    int32 GetAvailableSlotCount() const;
};
```

### 6.2 Ammo-Related Attributes (Already in FSuspenseCoreAmmoAttributeRow)

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

### 6.3 Weapon Ammo Attributes (Runtime)

```cpp
// In USuspenseCoreWeaponAttributeSet - already exists:
UPROPERTY() FGameplayAttributeData CurrentAmmoInMag;
UPROPERTY() FGameplayAttributeData MagazineSize;
UPROPERTY() FGameplayAttributeData TotalReserveAmmo;

// New attributes to add:
UPROPERTY() FGameplayAttributeData ChamberedRoundDamage;      // From current chambered round
UPROPERTY() FGameplayAttributeData ChamberedRoundPenetration;
```

---

## 7. UI Requirements

### 7.1 HUD Elements (DONE - Phase 6)

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

### 7.2 Inventory Magazine Inspection (DONE - Phase 6)

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

### Phase 1: Core Data Structures - DONE

**Files created:**
- `SuspenseCoreMagazineTypes.h` - Magazine data structures
- `SuspenseCoreAmmoStackTypes.h` - Ammo stack structures

### Phase 2: Magazine Component - DONE

**Files created:**
- `SuspenseCoreMagazineComponent.h/.cpp`

### Phase 3: QuickSlot System - DONE

**Files created:**
- `SuspenseCoreQuickSlotComponent.h/.cpp`

**GameplayTags added:**
```cpp
Equipment.QuickSlot.1
Equipment.QuickSlot.2
Equipment.QuickSlot.3
Equipment.QuickSlot.4
Item.Category.Magazine
Item.Category.Consumable
Item.Category.Throwable
```

### Phase 4: Reload Abilities - DONE

**Files created:**
- `GA_Reload.h/.cpp` - Main reload ability
- `GA_QuickSlot.h/.cpp` - QuickSlot usage ability

**Interfaces created (BridgeSystem):**
- `ISuspenseCoreMagazineProvider.h`
- `ISuspenseCoreQuickSlotProvider.h`

### Phase 5: Base Class Integration - DONE

**Files modified:**
- `SuspenseCoreWeaponActor.h/.cpp` - Added MagazineComponent
- `SuspenseCoreCharacter.h/.cpp` - Added QuickSlotComponent

### Phase 6: UI Integration - DONE

**C++ Widgets created:**
- `USuspenseCoreMagazineInspectionWidget` - Tarkov-style magazine inspection with per-round slots
- `USuspenseCoreMagazineTooltipWidget` - Magazine tooltip (inherits from SuspenseCoreTooltipWidget)

**Interfaces created (BridgeSystem):**
- `ISuspenseCoreMagazineInspectionWidgetInterface` - Interface for magazine inspection widget

**Native GameplayTags added:**
- `TAG_Equipment_Event_Ammo_RoundLoaded` - Single round loaded (for UI animation)
- `TAG_Equipment_Event_Ammo_RoundUnloaded` - Single round unloaded

**UIManager integration:**
- `ShowMagazineTooltip()` - Shows specialized magazine tooltip
- `IsMagazineTooltipVisible()` - Checks magazine tooltip visibility
- Magazine tooltip auto-selection based on item type

**Blueprint widgets to create (Designer task):**
- `WBP_AmmoStatus` - HUD ammo display (use SuspenseCoreAmmoCounterWidget)
- `WBP_QuickSlotBar` - QuickSlot display
- `WBP_MagazineInspector` - Blueprint child of USuspenseCoreMagazineInspectionWidget
- `WBP_MagazineTooltip` - Blueprint child of USuspenseCoreMagazineTooltipWidget
- `WBP_RoundSlot` - Individual round slot for inspection grid

### Phase 7: Testing & Polish - PENDING

- Test all reload paths
- Verify replication in multiplayer
- Balance reload times and magazine weights
- Polish animations and UI feedback

---

## Troubleshooting

### Common Issues

#### 1. "GetWeaponData is not a member of ISuspenseCoreWeapon"

**Solution:** Use the correct interface method:
```cpp
// Wrong:
GetWeaponData(...)

// Correct:
FSuspenseCoreUnifiedItemData WeaponData;
ISuspenseCoreWeapon::Execute_GetWeaponItemData(WeaponObject, WeaponData);
```

#### 2. "TWeakObjectPtr assignment error with USuspenseCoreInventoryComponent"

**Solution:** InventorySystem module is not in dependencies. Remove inventory references or add module dependency.

#### 3. "INVENTORYSYSTEM_API not defined"

**Solution:** Same as above - the InventorySystem module is not linked. Either add it to Build.cs or remove references.

#### 4. QuickSlotComponent is nullptr

**Solution:** Ensure `WITH_EQUIPMENT_SYSTEM=1` is defined in your build configuration.

#### 5. MagazineComponent not initialized

**Solution:** Call `InitializeFromWeapon()` in weapon's BeginPlay:
```cpp
void AMyWeapon::BeginPlay()
{
    Super::BeginPlay();

    if (MagazineComponent)
    {
        MagazineComponent->InitializeFromWeapon(this);
    }
}
```

---

## Future Enhancements

1. **Ammo Crafting** - Combine components to create ammunition
2. **Magazine Modifications** - Extended mags, drum mags, speed loaders
3. **Ammo Degradation** - Condition affects reliability and accuracy
4. **Ballistic Simulation** - Full trajectory with wind, drop, penetration
5. **Inventory Grid System** - Physical magazine storage in inventory

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

### Key Implementation Files

| File | Purpose |
|------|---------|
| `SuspenseCoreMagazineComponent.h/.cpp` | Magazine management |
| `SuspenseCoreQuickSlotComponent.h/.cpp` | QuickSlot system |
| `ISuspenseCoreMagazineProvider.h` | BridgeSystem interface |
| `ISuspenseCoreQuickSlotProvider.h` | BridgeSystem interface |
| `GA_Reload.h/.cpp` | Reload GAS ability |
| `GA_QuickSlot.h/.cpp` | QuickSlot GAS ability |
| `SuspenseCoreWeaponActor.h/.cpp` | Weapon base (has MagazineComponent) |
| `SuspenseCoreCharacter.h/.cpp` | Character base (has QuickSlotComponent) |

### Existing Files to Integrate With

| File | Purpose |
|------|---------|
| `SuspenseCoreUnifiedItemData` | Base item data |
| `SuspenseCoreInventoryItemInstance` | Runtime item instance |
| `SuspenseCoreAmmoAttributeSet` | GAS ammo attributes |
| `SuspenseCoreWeaponAttributeSet` | GAS weapon attributes |
| `SuspenseCoreEquipmentSlotComponent` | Equipment management |
| `SuspenseCoreWeaponAmmoComponent` | Existing ammo component |

---

**End of Document**

---

## Appendix C: Ammo Loading Methods (Tarkov-Style)

### Available Loading Methods

| Method | Description | Implementation |
|--------|-------------|----------------|
| **Drag & Drop** | Drag ammo stack onto magazine in inventory | Via UI → AmmoLoadingService |
| **Quick Load** | Double-click ammo to auto-load into best magazine | AmmoLoadingService::QuickLoadAmmo() |
| **Context Menu** | RKM on magazine → "Load Ammo" → select type | Via UI → AmmoLoadingService |
| **Inspect & Load** | Magazine inspection window with load buttons | USuspenseCoreMagazineInspectionWidget (Phase 6 - DONE) |

### AmmoLoadingService Usage

```cpp
// Get service (usually from GameMode or WorldSubsystem)
USuspenseCoreAmmoLoadingService* LoadingService = GetAmmoLoadingService();

// Option 1: Start loading specific ammo into magazine
FSuspenseCoreAmmoLoadRequest Request;
Request.MagazineInstanceID = MagazineGuid;
Request.AmmoID = FName("556x45_M855");
Request.RoundsToLoad = 30; // 0 = max possible
LoadingService->StartLoading(Request, OwnerActor);

// Option 2: Quick load (finds best magazine automatically)
FGuid SelectedMagazine;
LoadingService->QuickLoadAmmo(FName("556x45_M855"), 60, OwnerActor, SelectedMagazine);

// Option 3: Unload magazine
LoadingService->StartUnloading(MagazineGuid, 0, OwnerActor); // 0 = all rounds

// Cancel ongoing operation
LoadingService->CancelOperation(MagazineGuid);
```

### Loading Flow Diagram

```
┌──────────────────────────────────────────────────────────────────────┐
│                    AMMO LOADING FLOW                                 │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  UI Action                    AmmoLoadingService                     │
│  ─────────                    ──────────────────                     │
│                                                                      │
│  [Drag Ammo] ──────► StartLoading(Request) ───► Validate            │
│                                     │           ├── Caliber match   │
│  [Dbl-Click] ──────► QuickLoadAmmo()│           ├── Space check     │
│                                     │           └── Operation check │
│                                     ▼                                │
│                              ┌──────────────┐                        │
│                              │   LOADING    │ ◄── Tick() called     │
│                              │   STATE      │     by owner          │
│                              └──────┬───────┘                        │
│                                     │                                │
│                                     ├── TimePerRound elapsed         │
│                                     ▼                                │
│                              ┌──────────────┐                        │
│                              │ Process Round│ ───► Magazine.LoadRounds()
│                              │ (repeat)     │                        │
│                              └──────┬───────┘                        │
│                                     │                                │
│                                     │ All rounds processed           │
│                                     ▼                                │
│                              ┌──────────────┐                        │
│  OnLoadingCompleted ◄─────── │  COMPLETED   │ ───► EventBus.Publish  │
│  delegate                    └──────────────┘      TAG_Ammo_LoadCompleted
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

### Reload Priority Logic

**Reload is ONLY possible when:**
1. Character has magazines with ammo in QuickSlots OR inventory
2. Current magazine is not full (for tactical reload)
3. No blocking states (sprinting, firing, dead, etc.)

**Priority for magazine selection (FindBestMagazine):**
1. QuickSlots 1-4 (first with ammo wins)
2. Inventory (future: prioritize fuller magazines)

**If no magazines with ammo → Reload impossible**

---

## Appendix D: Key Files Reference (Phase 5)

| File | Purpose |
|------|---------|
| `SuspenseCoreMagazineComponent.h/.cpp` | Magazine in weapon (swap, chamber, reload) |
| `SuspenseCoreQuickSlotComponent.h/.cpp` | QuickSlot management |
| `SuspenseCoreAmmoLoadingService.h/.cpp` | Ammo loading service |
| `SuspenseCoreEquipmentNativeTags.h/.cpp` | Native GameplayTags |
| `SuspenseCoreReloadAbility.h/.cpp` | GAS reload ability |

---

## Appendix E: Magazine Weight System

Magazines have dynamic weight that changes based on loaded rounds.

### Weight Calculation

```cpp
// FSuspenseCoreMagazineData (from DataTable)
float EmptyWeight = 0.1f;        // Base magazine weight (kg)
float WeightPerRound = 0.012f;   // Weight added per round (kg)

// Total weight calculation
float GetWeightWithRounds(int32 RoundCount) const
{
    return EmptyWeight + (WeightPerRound * FMath::Clamp(RoundCount, 0, MaxCapacity));
}
```

### Inventory Weight Integration

When rounds are loaded into a magazine:
1. **Remove ammo weight** from inventory (ammo stack decreases)
2. **Add WeightPerRound** to magazine (magazine becomes heavier)

When rounds are unloaded from a magazine:
1. **Subtract WeightPerRound** from magazine (magazine becomes lighter)
2. **Add ammo weight** back to inventory (if ammo stack is created)

### Implementation Files

| File | Function | Description |
|------|----------|-------------|
| `SuspenseCoreInventoryComponent.cpp` | `RecalculateWeight()` | Uses `GetWeightWithRounds()` for magazines |
| `SuspenseCoreInventoryComponent.cpp` | `OnMagazineRoundLoaded()` | Adds WeightPerRound on load |
| `SuspenseCoreInventoryComponent.cpp` | `OnMagazineRoundUnloaded()` | Subtracts WeightPerRound on unload |
| `SuspenseCoreMagazineTypes.h` | `FSuspenseCoreMagazineData` | EmptyWeight, WeightPerRound fields |

### Weight Flow Diagram

```
┌────────────────────────────────────────────────────────────────────┐
│                    MAGAZINE LOADING WEIGHT FLOW                     │
├────────────────────────────────────────────────────────────────────┤
│                                                                    │
│  Before Loading:                                                   │
│  ┌─────────────┐     ┌─────────────┐                              │
│  │ Ammo Stack  │     │  Magazine   │                              │
│  │ 50 rounds   │     │  0/30       │                              │
│  │ 1.25 kg     │     │  0.10 kg    │   Total: 1.35 kg             │
│  └─────────────┘     └─────────────┘                              │
│                                                                    │
│  After Loading 30 rounds:                                          │
│  ┌─────────────┐     ┌─────────────┐                              │
│  │ Ammo Stack  │     │  Magazine   │                              │
│  │ 20 rounds   │     │  30/30      │                              │
│  │ 0.50 kg     │     │  0.46 kg    │   Total: 0.96 kg             │
│  └─────────────┘     └─────────────┘   (EmptyWeight + 30*WeightPerRound)
│                                                                    │
│  Weight change per round:                                          │
│  - Ammo removed: -0.025 kg (ammo unit weight)                     │
│  - Magazine added: +0.012 kg (WeightPerRound)                     │
│  - Net change: -0.013 kg per round loaded                         │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘
```

### Example DataTable Values

| Magazine | EmptyWeight | WeightPerRound | MaxCapacity |
|----------|-------------|----------------|-------------|
| Mag_STANAG_30 | 0.10 kg | 0.012 kg | 30 |
| Mag_STANAG_60 | 0.18 kg | 0.012 kg | 60 |
| Mag_Glock_17 | 0.07 kg | 0.008 kg | 17 |

---

**Last Updated:** 2026-01-05
**Version:** 1.3 (Magazine Weight System)
