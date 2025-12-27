# Movement Abilities Audit Report

**Date:** 2025-12-27
**Auditor:** Claude Code
**Scope:** Sprint, Jump, Crouch abilities + Stamina UI connection

---

## Executive Summary

Audit of movement abilities revealed **1 critical issue** and **3 compliance issues** that need to be addressed.

| Severity | Count | Description |
|----------|-------|-------------|
| 🔴 CRITICAL | 1 | EventBus data key mismatch breaks stamina UI |
| 🟡 COMPLIANCE | 3 | Abilities use deprecated tag patterns |

---

## 🔴 CRITICAL ISSUE: Stamina UI Connection Broken

### Description
The stamina UI widget cannot receive correct stamina values from the EventBus due to a key mismatch between the publisher (ASC) and subscriber (Widget).

### Location
- **Publisher:** `SuspenseCoreAbilitySystemComponent.cpp:93-96`
- **Subscriber:** `SuspenseHealthStaminaWidget.cpp:487-488`

### Issue Details

**ASC publishes:**
```cpp
// SuspenseCoreAbilitySystemComponent.cpp:93-96
Data.SetString(TEXT("AttributeName"), Attribute.GetName());
Data.SetFloat(TEXT("OldValue"), OldValue);
Data.SetFloat(TEXT("NewValue"), NewValue);  // ❌ Key = "NewValue"
Data.SetFloat(TEXT("Delta"), NewValue - OldValue);
// ❌ Missing: MaxValue
```

**Widget expects:**
```cpp
// SuspenseHealthStaminaWidget.cpp:487-488
float Current = EventData.GetFloat(FName("Value"), 100.0f);    // ❌ Looks for "Value"
float Max = EventData.GetFloat(FName("MaxValue"), 100.0f);     // ❌ Looks for "MaxValue"
```

### Impact
- Stamina bar always shows default value (100%)
- Sprint stamina drain not visible to player
- Jump stamina cost not reflected in UI
- Player cannot see stamina status

### Recommended Fix
Update ASC to publish with correct keys:
```cpp
Data.SetFloat(TEXT("Value"), NewValue);
Data.SetFloat(TEXT("MaxValue"), GetMaxAttributeValue(Attribute));
Data.SetFloat(TEXT("OldValue"), OldValue);
Data.SetFloat(TEXT("NewValue"), NewValue);  // Keep for backwards compatibility
Data.SetFloat(TEXT("Delta"), NewValue - OldValue);
```

---

## 🟡 COMPLIANCE ISSUE: Sprint Ability - String-Based Tags

### Location
`SuspenseCoreCharacterSprintAbility.cpp:27-45`

### Current Code
```cpp
FGameplayTagContainer AbilityTagContainer;
AbilityTagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Ability.Sprint")));
AbilityTagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Movement.Sprint")));
SetAssetTags(AbilityTagContainer);

ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Sprinting")));

ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Stunned")));
ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Disabled")));
ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Reloading")));
ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Aiming")));
ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Crouching")));
```

### Documentation Standard (WeaponAbilities_DeveloperGuide.md)
```cpp
// Native Tags with compile-time safety
#include "SuspenseCore/Tags/SuspenseCoreTags.h"

AbilityTagContainer.AddTag(SuspenseCoreTags::Ability_Movement_Sprint);
ActivationOwnedTags.AddTag(SuspenseCoreTags::State_Sprinting);
ActivationBlockedTags.AddTag(SuspenseCoreTags::State_Dead);
// etc.
```

### Impact
- No compile-time tag validation
- Typos in tag names cause silent failures
- Inconsistent with documented patterns

---

## 🟡 COMPLIANCE ISSUE: Jump Ability - String-Based Tags

### Location
`SuspenseCoreCharacterJumpAbility.cpp:21-33`

### Current Code
```cpp
FGameplayTagContainer AbilityTagContainer;
AbilityTagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Ability.Jump")));
AbilityTagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Movement.Jump")));
SetAssetTags(AbilityTagContainer);

ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Jumping")));

ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Stunned")));
ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Disabled")));
```

### Same Issue
Uses `FGameplayTag::RequestGameplayTag()` instead of Native Tags.

---

## 🟡 COMPLIANCE ISSUE: Crouch Ability - String-Based Tags

### Location
`SuspenseCoreCharacterCrouchAbility.cpp:29-42`

### Current Code
```cpp
FGameplayTagContainer AbilityTagContainer;
AbilityTagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Ability.Crouch")));
AbilityTagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Movement.Crouch")));
SetAssetTags(AbilityTagContainer);

ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Crouching")));

ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Stunned")));
ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Sprinting")));
```

### Same Issue
Uses `FGameplayTag::RequestGameplayTag()` instead of Native Tags.

---

## ✅ Positive Findings

### Stamina Consumption Logic - Correct
| Ability | Stamina Cost | Method | Verdict |
|---------|--------------|--------|---------|
| Sprint | Periodic drain | `SprintStaminaDrainEffectClass` | ✅ Correct |
| Jump | Instant cost | `JumpStaminaCostEffectClass` | ✅ Correct |
| Crouch | No cost | N/A | ✅ Correct (by design) |

### EventBus Event Publishing - Correct
- `OnRep_Stamina()` calls `BroadcastAttributeChange()` - ✅
- ASC publishes to `SuspenseCore.Event.GAS.Attribute.Stamina` - ✅
- Widget subscribes to same tag - ✅

### Blocking Tag Logic - Correct
- Sprint blocked by Aiming, Reloading, Crouching - ✅
- Jump blocked by Dead, Stunned, Disabled - ✅
- Crouch blocked by Dead, Stunned, Sprinting - ✅

### Stamina Check Before Activation - Correct
- Sprint checks `GetStamina() <= 0` before activation - ✅
- Jump checks `GetStamina() >= JumpStaminaCost` - ✅

---

## Recommended Action Items

### Priority 1 (Critical)
- [ ] Fix EventBus data keys in `SuspenseCoreAbilitySystemComponent::PublishAttributeChangeEvent()`
  - Add `"Value"` key with current value
  - Add `"MaxValue"` key with max attribute value

### Priority 2 (Compliance)
- [ ] Create `SuspenseCoreTags.h` with Native Tag definitions for movement abilities
- [ ] Update `SuspenseCoreCharacterSprintAbility.cpp` to use Native Tags
- [ ] Update `SuspenseCoreCharacterJumpAbility.cpp` to use Native Tags
- [ ] Update `SuspenseCoreCharacterCrouchAbility.cpp` to use Native Tags

### Priority 3 (Consistency)
- [ ] Add Native Tags to `SuspenseCoreTags.cpp` for:
  - `SuspenseCoreTags::Ability_Movement_Sprint`
  - `SuspenseCoreTags::Ability_Movement_Jump`
  - `SuspenseCoreTags::Ability_Movement_Crouch`
  - `SuspenseCoreTags::State_Sprinting`
  - `SuspenseCoreTags::State_Jumping`
  - `SuspenseCoreTags::State_Crouching`

---

## Files Audited

| File | Lines | Status |
|------|-------|--------|
| `GAS/Private/.../SuspenseCoreCharacterSprintAbility.cpp` | 268 | 🟡 Needs update |
| `GAS/Private/.../SuspenseCoreCharacterJumpAbility.cpp` | ~180 | 🟡 Needs update |
| `GAS/Private/.../SuspenseCoreCharacterCrouchAbility.cpp` | 275 | 🟡 Needs update |
| `GAS/Private/.../SuspenseCoreAbilitySystemComponent.cpp` | ~130 | 🔴 Critical fix |
| `GAS/Private/.../SuspenseCoreAttributeSet.cpp` | 316 | ✅ Correct |
| `UISystem/Private/.../SuspenseHealthStaminaWidget.cpp` | 532 | ✅ Correct (receiver) |

---

*End of Audit Report*
