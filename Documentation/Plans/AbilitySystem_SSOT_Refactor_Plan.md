# Ability System SSOT Refactoring Plan

> **Version:** 1.0
> **Date:** 2026-01-26
> **Author:** Claude Code (Technical Lead)
> **Status:** PROPOSAL
> **Module:** GAS, PlayerCore

---

## Executive Summary

This document outlines the refactoring plan to implement a proper SSOT (Single Source of Truth) architecture for the character ability system. The goal is to separate **common abilities** (available to all classes) from **class-specific abilities** using Data Assets with inheritance support.

---

## Table of Contents

1. [Current Architecture Analysis](#1-current-architecture-analysis)
2. [Problem Statement](#2-problem-statement)
3. [Proposed Solution](#3-proposed-solution)
4. [Implementation Plan](#4-implementation-plan)
5. [File Changes](#5-file-changes)
6. [Migration Guide](#6-migration-guide)

---

## 1. Current Architecture Analysis

### 1.1 Current Files

| File | Purpose | Location |
|------|---------|----------|
| `SuspenseCorePlayerState.h` | Stores `StartupAbilities` (HARDCODED) | PlayerCore |
| `SuspenseCoreCharacterClassData.h` | Stores `ClassAbilities` (DATA-DRIVEN) | GAS/Data |
| `SuspenseCoreCharacterClassSubsystem.cpp` | Applies classes and grants abilities | GAS/Subsystems |

### 1.2 Current Ability Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         CURRENT ABILITY GRANTING FLOW                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  PlayerState::BeginPlay()                                                    │
│         │                                                                    │
│         ├──► InitializeAbilitySystem()                                       │
│         │                                                                    │
│         ├──► GrantStartupAbilities()  ◄─── FROM: PlayerState.StartupAbilities│
│         │    ┌─────────────────────────────────────────────────────┐        │
│         │    │  PROBLEM: Hardcoded in Blueprint defaults!          │        │
│         │    │  - Sprint, Crouch, Jump, Interact, ADS, etc.        │        │
│         │    │  - Must edit BP_PlayerState to change               │        │
│         │    │  - Not data-driven, not SSOT                        │        │
│         │    └─────────────────────────────────────────────────────┘        │
│         │                                                                    │
│         └──► ApplyDefaultCharacterClass()                                    │
│                    │                                                         │
│                    ▼                                                         │
│         CharacterClassSubsystem::ApplyClassToActor()                         │
│                    │                                                         │
│                    ├──► ApplyAttributeModifiers()                            │
│                    │                                                         │
│                    ├──► GrantClassAbilities()  ◄─── FROM: ClassData.ClassAbilities
│                    │    ┌─────────────────────────────────────────────────┐ │
│                    │    │  OK: Data-driven via Data Asset                 │ │
│                    │    │  - Class-specific abilities                     │ │
│                    │    │  - Medic heal, Engineer deploy, etc.            │ │
│                    │    └─────────────────────────────────────────────────┘ │
│                    │                                                         │
│                    └──► ApplyPassiveEffects()                                │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 1.3 Current Data Structures

**PlayerState (lines 335-337):**
```cpp
/** Abilities to grant on startup */
UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config|GAS")
TArray<FSuspenseCoreAbilityEntry> StartupAbilities;
```

**CharacterClassData (lines 262-264):**
```cpp
/** Abilities granted by this class */
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
TArray<FSuspenseCoreClassAbilitySlot> ClassAbilities;
```

---

## 2. Problem Statement

### 2.1 Issues with Current Architecture

| Issue | Impact | Severity |
|-------|--------|----------|
| **StartupAbilities hardcoded in PlayerState BP** | Cannot change common abilities without BP edit | HIGH |
| **No SSOT for common abilities** | Data not centralized, hard to audit | HIGH |
| **Duplicate abilities across classes** | Each class must manually add common abilities | MEDIUM |
| **No inheritance for ability sets** | Cannot create base + specialized sets | MEDIUM |
| **Two different structs for same concept** | `FSuspenseCoreAbilityEntry` vs `FSuspenseCoreClassAbilitySlot` | LOW |

### 2.2 Current Pain Points

1. **Designer Iteration Blocked**
   - Adding new common ability requires C++ recompile or BP edit
   - No central place to see all granted abilities

2. **Violation of SRP**
   - PlayerState knows about ability configuration (should be data-driven)
   - Class subsystem only handles class-specific abilities

3. **SOLID Violations**
   - **S (Single Responsibility)**: PlayerState handles both state AND ability config
   - **O (Open/Closed)**: Must modify BP to add abilities
   - **D (Dependency Inversion)**: High-level PlayerState depends on low-level ability details

---

## 3. Proposed Solution

### 3.1 Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         PROPOSED SSOT ARCHITECTURE                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    DA_BaseAbilitySet (NEW)                          │    │
│  │   USuspenseCoreBaseAbilitySetData : UPrimaryDataAsset               │    │
│  ├─────────────────────────────────────────────────────────────────────┤    │
│  │  FName SetID = "BaseAbilities"                                      │    │
│  │  TArray<FSuspenseCoreAbilitySlot> CommonAbilities:                  │    │
│  │    - GA_Sprint          (Input.Action.Sprint)                       │    │
│  │    - GA_Crouch          (Input.Action.Crouch)                       │    │
│  │    - GA_Jump            (Input.Action.Jump)                         │    │
│  │    - GA_Interact        (Input.Action.Interact)                     │    │
│  │    - GA_AimDownSight    (Input.Action.ADS)                          │    │
│  │    - GA_Reload          (Input.Action.Reload)                       │    │
│  │    - GA_QuickSlot       (Input.Action.QuickSlot)                    │    │
│  │    - GA_Fire            (Input.Action.Fire)                         │    │
│  │    - GA_MedicalEquip    (Input.Action.Medical)                      │    │
│  │    - GA_GrenadeEquip    (Input.Action.Grenade)                      │    │
│  │    etc.                                                             │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                              │                                               │
│                              │ Referenced by                                 │
│                              ▼                                               │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │               USuspenseCoreCharacterClassData (MODIFIED)            │    │
│  ├─────────────────────────────────────────────────────────────────────┤    │
│  │  // NEW: Reference to base ability set                              │    │
│  │  TSoftObjectPtr<USuspenseCoreBaseAbilitySetData> BaseAbilitySet;    │    │
│  │                                                                     │    │
│  │  // EXISTING: Class-specific abilities                              │    │
│  │  TArray<FSuspenseCoreClassAbilitySlot> ClassAbilities;              │    │
│  │    - GA_MedicHeal        (Medic only)                               │    │
│  │    - GA_EngineerDeploy   (Engineer only)                            │    │
│  │    - GA_ReconMarkTarget  (Recon only)                               │    │
│  │    etc.                                                             │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
│                    OR (Alternative: Project Settings)                        │
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │               USuspenseCoreProjectSettings (MODIFIED)               │    │
│  ├─────────────────────────────────────────────────────────────────────┤    │
│  │  // NEW: Global base ability set for all characters                 │    │
│  │  TSoftObjectPtr<USuspenseCoreBaseAbilitySetData> DefaultBaseAbilitySet;│  │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 3.2 New Ability Granting Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         NEW ABILITY GRANTING FLOW                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  PlayerState::BeginPlay()                                                    │
│         │                                                                    │
│         ├──► InitializeAbilitySystem()                                       │
│         │                                                                    │
│         └──► ApplyDefaultCharacterClass()                                    │
│                    │                                                         │
│                    ▼                                                         │
│         CharacterClassSubsystem::ApplyClassToActor()                         │
│                    │                                                         │
│                    ├──► ApplyAttributeModifiers()                            │
│                    │                                                         │
│                    ├──► GrantBaseAbilities()  ◄─── NEW: FROM SSOT Data Asset │
│                    │    ┌─────────────────────────────────────────────────┐ │
│                    │    │  Step 1: Load BaseAbilitySet from:              │ │
│                    │    │    - ClassData.BaseAbilitySet (if set)          │ │
│                    │    │    - OR ProjectSettings.DefaultBaseAbilitySet   │ │
│                    │    │  Step 2: Grant all CommonAbilities              │ │
│                    │    └─────────────────────────────────────────────────┘ │
│                    │                                                         │
│                    ├──► GrantClassAbilities()  ◄─── EXISTING: Class-specific │
│                    │                                                         │
│                    └──► ApplyPassiveEffects()                                │
│                                                                              │
│  Result: All abilities from SSOT, designer-editable, no BP changes needed   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 3.3 Design Principles Applied

| Principle | Implementation |
|-----------|---------------|
| **SOLID - S** | PlayerState only manages state, not ability configuration |
| **SOLID - O** | Add abilities by editing Data Asset, not code/BP |
| **SOLID - D** | PlayerState depends on abstraction (DataManager), not details |
| **DRY** | Common abilities defined once, inherited by all classes |
| **SSOT** | All ability data in centralized Data Assets |
| **Data-Driven** | Designers can iterate without code changes |

---

## 4. Implementation Plan

### Phase 1: Create BaseAbilitySetData (NEW FILE)

**File:** `Source/GAS/Public/SuspenseCore/Data/SuspenseCoreBaseAbilitySetData.h`

```cpp
// SuspenseCoreBaseAbilitySetData.h
// Base ability set data asset for common abilities

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreBaseAbilitySetData.generated.h"

class UGameplayAbility;
class UGameplayEffect;

/**
 * FSuspenseCoreAbilitySlot
 * Unified ability slot structure (replaces both FSuspenseCoreAbilityEntry and FSuspenseCoreClassAbilitySlot)
 */
USTRUCT(BlueprintType)
struct GAS_API FSuspenseCoreAbilitySlot
{
    GENERATED_BODY()

    /** The ability class to grant */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
    TSubclassOf<UGameplayAbility> AbilityClass;

    /** Input action tag for binding */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
    FGameplayTag InputTag;

    /** Ability level (default 1) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (ClampMin = "1", ClampMax = "10"))
    int32 AbilityLevel = 1;

    /** Level required to unlock (0 = available immediately) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (ClampMin = "0", ClampMax = "100"))
    int32 RequiredLevel = 0;

    /** Is this a passive ability (always active)? */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
    bool bIsPassive = false;

    /** Slot index for UI display */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (ClampMin = "0", ClampMax = "20"))
    int32 SlotIndex = 0;

    /** Optional: Category for grouping in UI */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
    FGameplayTag CategoryTag;
};

/**
 * USuspenseCoreBaseAbilitySetData
 *
 * Primary Data Asset containing common abilities available to all character classes.
 * This is the SSOT for base gameplay abilities (movement, interaction, combat basics).
 *
 * Usage:
 * 1. Create Data Asset: DA_BaseAbilitySet
 * 2. Configure in Project Settings or per-class CharacterClassData
 * 3. Abilities are granted automatically when class is applied
 *
 * Design:
 * - Separates common abilities from class-specific
 * - Enables inheritance (override per-class if needed)
 * - Designer-editable without code changes
 */
UCLASS(BlueprintType, Blueprintable)
class GAS_API USuspenseCoreBaseAbilitySetData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    USuspenseCoreBaseAbilitySetData();

    // ═══════════════════════════════════════════════════════════════════════════
    // IDENTITY
    // ═══════════════════════════════════════════════════════════════════════════

    /** Internal identifier for this ability set */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FName SetID;

    /** Display name for UI/debugging */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FText DisplayName;

    /** Description of what this set contains */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FText Description;

    // ═══════════════════════════════════════════════════════════════════════════
    // ABILITIES
    // ═══════════════════════════════════════════════════════════════════════════

    /** Common abilities granted to all characters using this set */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
    TArray<FSuspenseCoreAbilitySlot> CommonAbilities;

    // ═══════════════════════════════════════════════════════════════════════════
    // PASSIVE EFFECTS
    // ═══════════════════════════════════════════════════════════════════════════

    /** Common passive effects applied to all characters (e.g., stamina regen) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
    TArray<TSubclassOf<UGameplayEffect>> CommonPassiveEffects;

    // ═══════════════════════════════════════════════════════════════════════════
    // INHERITANCE
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * Parent ability set to inherit from.
     * Abilities from parent are granted first, then this set's abilities.
     * Allows creating hierarchy: Core → Combat → Class-specific
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inheritance")
    TSoftObjectPtr<USuspenseCoreBaseAbilitySetData> ParentAbilitySet;

    // ═══════════════════════════════════════════════════════════════════════════
    // UPrimaryDataAsset Interface
    // ═══════════════════════════════════════════════════════════════════════════

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

    // ═══════════════════════════════════════════════════════════════════════════
    // UTILITY
    // ═══════════════════════════════════════════════════════════════════════════

    /** Get all abilities (including inherited from parent) */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|AbilitySet")
    TArray<FSuspenseCoreAbilitySlot> GetAllAbilities() const;

    /** Get abilities available at given level (including inherited) */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|AbilitySet")
    TArray<FSuspenseCoreAbilitySlot> GetAbilitiesForLevel(int32 Level) const;

    /** Get all passive effects (including inherited) */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|AbilitySet")
    TArray<TSubclassOf<UGameplayEffect>> GetAllPassiveEffects() const;
};
```

### Phase 2: Modify CharacterClassData

**File:** `Source/GAS/Public/SuspenseCore/Data/SuspenseCoreCharacterClassData.h`

**Changes:**

```cpp
// ADD: Reference to base ability set
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
TSoftObjectPtr<USuspenseCoreBaseAbilitySetData> BaseAbilitySet;

// KEEP: Class-specific abilities (rename for clarity)
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
TArray<FSuspenseCoreClassAbilitySlot> ClassSpecificAbilities;  // Was: ClassAbilities
```

### Phase 3: Modify ProjectSettings

**File:** `Source/BridgeSystem/Public/SuspenseCore/Settings/SuspenseCoreProjectSettings.h`

**Changes:**

```cpp
// ADD: Default base ability set for all characters
UPROPERTY(EditDefaultsOnly, Config, Category = "GAS|Abilities")
TSoftObjectPtr<USuspenseCoreBaseAbilitySetData> DefaultBaseAbilitySet;
```

### Phase 4: Modify CharacterClassSubsystem

**File:** `Source/GAS/Private/SuspenseCore/Subsystems/SuspenseCoreCharacterClassSubsystem.cpp`

**Changes:**

```cpp
// ADD: New method to grant base abilities
void USuspenseCoreCharacterClassSubsystem::GrantBaseAbilities(
    UAbilitySystemComponent* ASC,
    const USuspenseCoreCharacterClassData* ClassData,
    int32 PlayerLevel)
{
    if (!ASC || !ClassData) return;

    // 1. Get base ability set (class-specific or default from settings)
    USuspenseCoreBaseAbilitySetData* BaseSet = nullptr;

    if (!ClassData->BaseAbilitySet.IsNull())
    {
        BaseSet = ClassData->BaseAbilitySet.LoadSynchronous();
    }

    if (!BaseSet)
    {
        // Fallback to project settings default
        const USuspenseCoreProjectSettings* Settings = USuspenseCoreProjectSettings::Get();
        if (Settings && !Settings->DefaultBaseAbilitySet.IsNull())
        {
            BaseSet = Settings->DefaultBaseAbilitySet.LoadSynchronous();
        }
    }

    if (!BaseSet)
    {
        UE_LOG(LogSuspenseCoreClass, Warning, TEXT("No BaseAbilitySet found for class or in settings"));
        return;
    }

    // 2. Get all abilities (including inherited)
    TArray<FSuspenseCoreAbilitySlot> AllAbilities = BaseSet->GetAbilitiesForLevel(PlayerLevel);

    // 3. Grant each ability
    for (const FSuspenseCoreAbilitySlot& Slot : AllAbilities)
    {
        if (Slot.AbilityClass && !ASC->FindAbilitySpecFromClass(Slot.AbilityClass))
        {
            FGameplayAbilitySpec Spec(Slot.AbilityClass, Slot.AbilityLevel, INDEX_NONE, ASC->GetOwner());

            // Store input tag in spec for later binding
            Spec.DynamicAbilityTags.AddTag(Slot.InputTag);

            ASC->GiveAbility(Spec);
            UE_LOG(LogSuspenseCoreClass, Log, TEXT("Granted base ability: %s"), *Slot.AbilityClass->GetName());
        }
    }

    // 4. Apply common passive effects
    for (const TSubclassOf<UGameplayEffect>& EffectClass : BaseSet->GetAllPassiveEffects())
    {
        if (EffectClass)
        {
            FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
            FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, 1.0f, Context);
            if (SpecHandle.IsValid())
            {
                ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
            }
        }
    }
}

// MODIFY: ApplyClassDataToActor to call GrantBaseAbilities
bool USuspenseCoreCharacterClassSubsystem::ApplyClassDataToActor(...)
{
    // ... existing code ...

    // 2. Apply attribute modifiers
    ApplyAttributeModifiers(ASC, ClassData);

    // 3. NEW: Grant base abilities first
    GrantBaseAbilities(ASC, ClassData, PlayerLevel);

    // 4. Grant class-specific abilities
    GrantClassAbilities(ASC, ClassData, PlayerLevel);

    // 5. Apply passive effects
    ApplyPassiveEffects(ASC, ClassData);

    // ... rest of code ...
}
```

### Phase 5: Modify PlayerState

**File:** `Source/PlayerCore/Public/SuspenseCore/Core/SuspenseCorePlayerState.h`

**Changes:**

```cpp
// DEPRECATE: StartupAbilities (keep for backwards compatibility)
UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config|GAS", meta = (DeprecatedProperty, DeprecationMessage = "Use BaseAbilitySetData instead"))
TArray<FSuspenseCoreAbilityEntry> StartupAbilities_DEPRECATED;

// REMOVE: GrantStartupAbilities() call from BeginPlay
// Base abilities now granted via CharacterClassSubsystem::GrantBaseAbilities()
```

---

## 5. File Changes

### 5.1 New Files

| File | Purpose |
|------|---------|
| `SuspenseCoreBaseAbilitySetData.h` | Base ability set data asset header |
| `SuspenseCoreBaseAbilitySetData.cpp` | Implementation |
| `DA_BaseAbilitySet.uasset` | Default base ability set data asset |

### 5.2 Modified Files

| File | Changes |
|------|---------|
| `SuspenseCoreCharacterClassData.h` | Add `BaseAbilitySet` reference |
| `SuspenseCoreProjectSettings.h` | Add `DefaultBaseAbilitySet` |
| `SuspenseCoreCharacterClassSubsystem.h` | Add `GrantBaseAbilities()` declaration |
| `SuspenseCoreCharacterClassSubsystem.cpp` | Implement `GrantBaseAbilities()`, modify flow |
| `SuspenseCorePlayerState.h` | Deprecate `StartupAbilities` |
| `SuspenseCorePlayerState.cpp` | Remove `GrantStartupAbilities()` call |

### 5.3 Data Assets to Create

```
Content/
└── Blueprints/
    └── Core/
        └── Data/
            └── AbilitySets/
                ├── DA_BaseAbilitySet_Core.uasset      ← Movement, Interaction
                ├── DA_BaseAbilitySet_Combat.uasset    ← Weapons, Medical, Grenades
                └── DA_BaseAbilitySet_Default.uasset   ← Inherits Core + Combat
```

---

## 6. Migration Guide

### 6.1 For Existing Projects

1. **Create BaseAbilitySetData Data Asset**
   - Copy abilities from PlayerState BP to new Data Asset
   - Set up inheritance chain if needed

2. **Update Project Settings**
   - Set `DefaultBaseAbilitySet` to new Data Asset

3. **Update CharacterClassData Assets**
   - Remove duplicated common abilities from `ClassAbilities`
   - Optionally override `BaseAbilitySet` per class

4. **Update PlayerState BP**
   - Clear `StartupAbilities` array (now deprecated)
   - Verify abilities still granted correctly

### 6.2 Backwards Compatibility

The deprecated `StartupAbilities` will still work:

```cpp
void ASuspenseCorePlayerState::GrantStartupAbilities()
{
    // Legacy support: grant from deprecated array if not empty
    if (StartupAbilities_DEPRECATED.Num() > 0)
    {
        UE_LOG(LogSuspenseCore, Warning,
            TEXT("StartupAbilities is deprecated. Migrate to BaseAbilitySetData."));
        // ... grant legacy abilities ...
    }
}
```

---

## 7. Proposed Data Asset Structure

### DA_BaseAbilitySet_Default

```
SetID: "Default"
DisplayName: "Default Base Abilities"
Description: "Core gameplay abilities for all characters"

CommonAbilities:
├── Movement
│   ├── GA_Sprint           (Input.Action.Sprint)
│   ├── GA_Crouch           (Input.Action.Crouch)
│   ├── GA_Jump             (Input.Action.Jump)
│   └── GA_Walk             (Passive)
│
├── Combat
│   ├── GA_Fire             (Input.Action.Fire)
│   ├── GA_AimDownSight     (Input.Action.ADS)
│   ├── GA_Reload           (Input.Action.Reload)
│   ├── GA_SwitchFireMode   (Input.Action.FireMode)
│   └── GA_MeleeAttack      (Input.Action.Melee)
│
├── Interaction
│   ├── GA_Interact         (Input.Action.Interact)
│   ├── GA_QuickSlot        (Input.Action.QuickSlot)
│   ├── GA_Inventory        (Input.Action.Inventory)
│   └── GA_InspectItem      (Input.Action.Inspect)
│
├── Medical
│   ├── GA_MedicalEquip     (Input.Action.Medical)
│   └── GA_MedicalUse       (Input.Action.Fire - context)
│
└── Throwable
    ├── GA_GrenadeEquip     (Input.Action.Grenade)
    └── GA_GrenadeThrow     (Input.Action.Fire - context)

CommonPassiveEffects:
├── GE_StaminaRegen
├── GE_HealthRegen (if any)
└── GE_BaseMovementSpeed
```

---

## 8. Benefits Summary

| Benefit | Description |
|---------|-------------|
| **SSOT Compliance** | All ability data in Data Assets, not code/BP |
| **Designer-Friendly** | Edit abilities without code changes |
| **Inheritance Support** | Base → Combat → Class-specific hierarchy |
| **Clean Separation** | Common vs class-specific abilities clearly separated |
| **Audit-Friendly** | Single place to review all granted abilities |
| **Backwards Compatible** | Deprecated API still works during migration |
| **SOLID Compliant** | PlayerState no longer knows about ability details |

---

## 9. Implementation Checklist

- [ ] Create `SuspenseCoreBaseAbilitySetData.h/cpp`
- [ ] Create `FSuspenseCoreAbilitySlot` unified struct
- [ ] Modify `SuspenseCoreCharacterClassData.h` (add BaseAbilitySet)
- [ ] Modify `SuspenseCoreProjectSettings.h` (add DefaultBaseAbilitySet)
- [ ] Implement `GrantBaseAbilities()` in CharacterClassSubsystem
- [ ] Modify `ApplyClassDataToActor()` flow
- [ ] Deprecate `StartupAbilities` in PlayerState
- [ ] Create `DA_BaseAbilitySet_Default.uasset`
- [ ] Migrate existing BP abilities to Data Asset
- [ ] Update all CharacterClassData assets
- [ ] Test ability granting flow
- [ ] Update documentation

---

**Document End**

*Awaiting approval before implementation.*
