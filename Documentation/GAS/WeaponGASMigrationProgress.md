# Weapon GAS System Migration Progress

## Overview
Migration of legacy MedCom weapon GAS system to SuspenseCore architecture.

**Branch:** `claude/add-weapon-docs-nl4wW`
**Started:** 2026-01-11
**Status:** IN PROGRESS

---

## Migration Map

| # | Legacy File | SuspenseCore File | Module | Status |
|---|-------------|-------------------|--------|--------|
| 0 | - | GameplayTags additions | BridgeSystem | DONE |
| 1 | MedComTraceUtils | SuspenseCoreTraceUtils | BridgeSystem | DONE |
| 2 | MedComSpreadProcessor | SuspenseCoreSpreadProcessor | BridgeSystem | DONE |
| 3 | MedComWeaponCameraShake | SuspenseCoreWeaponCameraShake | PlayerCore | DONE |
| 4 | MedComAbilityTask_PlayMontageAndWaitForEvent | SuspenseCoreAbilityTask_PlayMontageAndWaitForEvent | GAS | DONE |
| 5 | MedComWeaponAsyncTask_PerformTrace | SuspenseCoreWeaponAsyncTask_PerformTrace | GAS | DONE |
| 6 | MedComDamageEffect | SuspenseCoreDamageEffect | GAS | DONE |
| 7 | MedComBaseFireAbility | SuspenseCoreBaseFireAbility | GAS | DONE |
| 8 | MedComSingleShotAbility | SuspenseCoreSingleShotAbility | GAS | DONE |
| 9 | MedComAutoFireAbility | SuspenseCoreAutoFireAbility | GAS | DONE |
| 10 | MedComBurstFireAbility | SuspenseCoreBurstFireAbility | GAS | DONE |
| 11 | MedComSwitchFireModeAbility | SuspenseCoreSwitchFireModeAbility | GAS | DONE |

---

## Architecture Changes Summary

### DI Pattern (CRITICAL)
```
Legacy:  Ability → WeaponActor → EquipmentComponent (direct)
New:     Ability → ISuspenseCoreWeaponCombatState (interface)
                 → ISuspenseCoreWeapon (interface)
                 → ISuspenseCoreMagazineProvider (interface)
```

### SSOT Pattern
```
Legacy:  State stored in abilities (bIsFiring, etc.)
New:     State via WeaponStanceComponent through interface
         CombatState->SetFiring(true) / CombatState->IsFiring()
```

### EventBus Integration
```
Legacy:  Direct calls (Weapon->NotifyAmmoChanged())
New:     EventBus->Publish(SuspenseCoreTags::Event::Weapon::Fired, EventData)
```

### Native Tags
```
Legacy:  FGameplayTag::RequestGameplayTag("...")
New:     SuspenseCoreTags::State::Firing (compile-time safe)
```

---

## File Locations

### GAS Module
```
Source/GAS/
├── Public/SuspenseCore/
│   ├── Abilities/
│   │   ├── Base/
│   │   │   └── SuspenseCoreBaseFireAbility.h
│   │   └── Weapon/
│   │       ├── SuspenseCoreSingleShotAbility.h
│   │       ├── SuspenseCoreAutoFireAbility.h
│   │       ├── SuspenseCoreBurstFireAbility.h
│   │       └── SuspenseCoreSwitchFireModeAbility.h
│   ├── Effects/
│   │   └── Weapon/
│   │       └── SuspenseCoreDamageEffect.h
│   └── Tasks/
│       ├── SuspenseCoreAbilityTask_PlayMontageAndWaitForEvent.h
│       └── SuspenseCoreWeaponAsyncTask_PerformTrace.h
└── Private/SuspenseCore/
    └── [mirrors Public structure]
```

### BridgeSystem Module
```
Source/BridgeSystem/
├── Public/SuspenseCore/
│   └── Utils/
│       ├── SuspenseCoreSpreadProcessor.h
│       └── SuspenseCoreTraceUtils.h
└── Private/SuspenseCore/
    └── Utils/
        └── [implementation files]
```

### PlayerCore Module
```
Source/PlayerCore/
├── Public/SuspenseCore/
│   └── CameraShake/
│       └── SuspenseCoreWeaponCameraShake.h
└── Private/SuspenseCore/
    └── CameraShake/
        └── SuspenseCoreWeaponCameraShake.cpp
```

---

## New GameplayTags Added

### State Tags
- `State.BurstActive` - Burst fire in progress (blocks re-activation)
- `State.AutoFireActive` - Auto fire active (blocks re-activation)

### Data Tags (SetByCaller)
- `Data.Damage` - Damage value for GameplayEffects

### Ability Tags
- (existing tags used: Fire, Reload, AimDownSight, etc.)

---

## Key Types Used

### Existing (BridgeSystem)
- `FWeaponShotParams` - Shot parameters (origin, direction, spread, damage)
- `FWeaponHitData` - Hit result data
- `EWeaponStateMask` - Weapon state flags

### Interfaces Used
- `ISuspenseCoreWeaponCombatState` - Combat state queries/commands
- `ISuspenseCoreWeapon` - Weapon interface
- `ISuspenseCoreMagazineProvider` - Magazine/ammo access
- `ISuspenseCoreFireModeProvider` - Fire mode switching

---

## Configuration Constants

### Recoil System
```cpp
ProgressiveRecoilMultiplier = 1.2f   // Each shot +20%
MaximumRecoilMultiplier = 3.0f       // Cap at 3x
RecoilResetTime = 0.5f               // Reset after 0.5s idle
ADSRecoilMultiplier = 0.5f           // ADS = 50% recoil
```

### Spread Modifiers
```cpp
AimingMod = 0.3f      // -70% spread
CrouchingMod = 0.7f   // -30% spread
SprintingMod = 3.0f   // +200% spread
JumpingMod = 4.0f     // +300% spread
AutoFireMod = 2.0f    // +100% spread
BurstFireMod = 1.5f   // +50% spread
MovementMod = 1.5f    // +50% spread (if speed > 10)
```

### Fire Modes
```cpp
BurstCount = 3        // Shots per burst
BurstDelay = 0.15f    // Delay between burst shots
AutoFireRate = 0.1f   // 10 rounds/second default
```

### Validation
```cpp
MaxAllowedDistance = 300.0f    // Shot origin validation
MaxTimeDifference = 2.0f       // Client-server time diff
MaxAngleDifference = 45.0f     // Direction validation
```

---

## Implementation Notes

### Wave 1 (No Dependencies)
1. SuspenseCoreTraceUtils - Static utility library
2. SuspenseCoreSpreadProcessor - Spread calculation
3. SuspenseCoreWeaponCameraShake - Camera shake pattern

### Wave 2 (Depends on Wave 1)
4. SuspenseCoreAbilityTask_PlayMontageAndWaitForEvent
5. SuspenseCoreWeaponAsyncTask_PerformTrace
6. SuspenseCoreDamageEffect

### Wave 3 (Depends on Wave 2)
7. SuspenseCoreBaseFireAbility

### Wave 4 (Depends on Wave 3)
8. SuspenseCoreSingleShotAbility
9. SuspenseCoreAutoFireAbility
10. SuspenseCoreBurstFireAbility
11. SuspenseCoreSwitchFireModeAbility

---

## Checklist for Each File

- [ ] Correct module (GAS/BridgeSystem/PlayerCore)
- [ ] Correct API macro (GAS_API/BRIDGESYSTEM_API/PLAYERCORE_API)
- [ ] Include guards + #pragma once
- [ ] Copyright header
- [ ] Interface-only for cross-module deps
- [ ] Native Tags (not RequestGameplayTag)
- [ ] EventBus for events
- [ ] SSOT via interface for state
- [ ] Replicated properties configured
- [ ] GENERATED_BODY() in all USTRUCTs/UCLASSes

---

## Completed Tasks Log

### 2026-01-11 - Full Weapon GAS System Migration
- Added GameplayTags: State.BurstActive, State.AutoFireActive, Data.Damage
- Created SuspenseCoreTraceUtils (BridgeSystem) - line trace utilities
- Created SuspenseCoreSpreadProcessor (BridgeSystem) - spread calculation
- Created SuspenseCoreWeaponCameraShake (PlayerCore) - recoil camera shake
- Created SuspenseCoreAbilityTask_PlayMontageAndWaitForEvent (GAS)
- Created SuspenseCoreWeaponAsyncTask_PerformTrace (GAS)
- Created SuspenseCoreDamageEffect (GAS) - instant damage effect
- Created SuspenseCoreBaseFireAbility (GAS) - abstract base class
- Created SuspenseCoreSingleShotAbility (GAS) - semi-auto fire
- Created SuspenseCoreAutoFireAbility (GAS) - full-auto fire
- Created SuspenseCoreBurstFireAbility (GAS) - burst fire
- Created SuspenseCoreSwitchFireModeAbility (GAS) - fire mode cycling

All files follow DI architecture (interfaces from BridgeSystem), SSOT pattern,
EventBus integration, and native tags for compile-time safety.

---

## Known Issues / TODOs

1. Need to verify ISuspenseCoreWeapon interface has muzzle location getter
2. Need to check if ammo consumption goes through MagazineProvider
3. Camera shake may need PlayerCore module dependency setup

---

## How to Continue

1. Read this document to understand current state
2. Check the Migration Map for pending files
3. Follow Wave order (1 → 2 → 3 → 4)
4. Update status in Migration Map after each file
5. Log completed tasks with date

---
