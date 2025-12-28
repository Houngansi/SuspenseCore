# Weapon Abilities Developer Guide

> **Version:** 1.0
> **Last Updated:** December 2024
> **Target:** Unreal Engine 5.x | SuspenseCore GAS Architecture

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Module Dependencies](#module-dependencies)
3. [Creating a Weapon Ability](#creating-a-weapon-ability)
4. [Native Tags System](#native-tags-system)
5. [EventBus Integration](#eventbus-integration)
6. [SSOT Pattern](#ssot-pattern)
7. [Existing Abilities Reference](#existing-abilities-reference)
8. [Checklists](#checklists)
9. [Common Mistakes](#common-mistakes)

---

## Architecture Overview

Weapon abilities in SuspenseCore follow strict architectural patterns to avoid circular dependencies and maintain SOLID principles.

### Key Architectural Rules

| Rule | Implementation |
|------|----------------|
| **No Circular Dependencies** | GAS module uses interfaces, not concrete classes |
| **SSOT (Single Source of Truth)** | WeaponStanceComponent owns all combat state |
| **Interface Segregation** | ISuspenseCoreWeaponCombatState in BridgeSystem |
| **EventBus for Decoupling** | Camera, UI subscribe to events, not poll state |
| **Native Tags** | Compile-time tag validation with `UE_DECLARE_GAMEPLAY_TAG_EXTERN` |

### Data Flow Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    WEAPON ABILITY DATA FLOW                                  │
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                         INPUT (Player)                                  │ │
│  │  RMB Press → Enhanced Input → AbilitySystemComponent                   │ │
│  └─────────────────────────────────┬──────────────────────────────────────┘ │
│                                    │                                         │
│                                    ▼                                         │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                    GAS ABILITY (e.g., AimDownSight)                     │ │
│  │                                                                         │ │
│  │  1. CanActivateAbility():                                               │ │
│  │     ISuspenseCoreWeaponCombatState* CombatState = GetWeaponCombatState()│ │
│  │     if (!CombatState->IsWeaponDrawn()) return false;                   │ │
│  │     if (CombatState->IsReloading()) return false;                      │ │
│  │                                                                         │ │
│  │  2. ActivateAbility():                                                  │ │
│  │     CombatState->SetAiming(true);     // → WeaponStanceComponent       │ │
│  │     ApplyGameplayEffect(SpeedDebuff); // → ASC                         │ │
│  │     EventBus->Publish(Camera::FOVChanged); // → Camera subscribes      │ │
│  │                                                                         │ │
│  │  3. EndAbility():                                                       │ │
│  │     CombatState->SetAiming(false);                                      │ │
│  │     RemoveGameplayEffect(SpeedDebuff);                                  │ │
│  └─────────────────────────────────┬──────────────────────────────────────┘ │
│                                    │                                         │
│                                    ▼                                         │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │               WeaponStanceComponent (SSOT)                              │ │
│  │                                                                         │ │
│  │  SetAiming(true):                                                       │ │
│  │    bIsAiming = true;                    // Replicated                   │ │
│  │    TargetAimPoseAlpha = 1.0f;           // For interpolation            │ │
│  │    ForceNetUpdate();                                                    │ │
│  │    EventBus->Publish(AimStarted);       // Equipment events             │ │
│  └─────────────────────────────────┬──────────────────────────────────────┘ │
│                                    │                                         │
│                                    ▼                                         │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │               AnimInstance::UpdateWeaponData()                          │ │
│  │                                                                         │ │
│  │  Snapshot = StanceComp->GetStanceSnapshot();                            │ │
│  │  bIsAiming = Snapshot.bIsAiming;                                        │ │
│  │  AimingAlpha = Snapshot.AimPoseAlpha; // Already interpolated!         │ │
│  └─────────────────────────────────┬──────────────────────────────────────┘ │
│                                    │                                         │ │
│                                    ▼                                         │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │               Animation Blueprint                                        │ │
│  │                                                                         │ │
│  │  Blend Hip → ADS based on AimingAlpha                                   │ │
│  │  Apply LHGripTransform[0] ↔ [1] based on AimingAlpha                   │ │
│  └────────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Module Dependencies

### Correct Dependency Graph (NO CYCLES)

```
                    BridgeSystem
                   (shared interfaces)
                         │
        ┌────────────────┼────────────────┐
        │                │                │
        ▼                ▼                ▼
      GAS         EquipmentSystem    PlayerCore
   (abilities)     (components)    (animation)

   GAS.Build.cs:
   - BridgeSystem (ISuspenseCoreWeaponCombatState)
   - GameplayAbilities
   - GameplayTags

   EquipmentSystem.Build.cs:
   - BridgeSystem (same interface)
   - GAS (for AbilitySystemComponent access)

   PlayerCore.Build.cs:
   - BridgeSystem
   - EquipmentSystem (uses WeaponStanceComponent directly)
   - GAS
```

### Why GAS Cannot Depend on EquipmentSystem

```
WRONG (creates cycle):
  EquipmentSystem → GAS (for AbilitySystem)
  GAS → EquipmentSystem (for WeaponStanceComponent)

  Result: Linker error, compilation failure

CORRECT (no cycle):
  EquipmentSystem → GAS (for AbilitySystem)
  GAS → BridgeSystem (for ISuspenseCoreWeaponCombatState interface)
  EquipmentSystem → BridgeSystem (implements interface)

  Result: Compiles successfully, clean architecture
```

### Interface Location

**File:** `Source/BridgeSystem/Public/SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeaponCombatState.h`

```cpp
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreWeaponCombatState : public UInterface { ... };

class BRIDGESYSTEM_API ISuspenseCoreWeaponCombatState
{
    GENERATED_BODY()
public:
    // Queries (const - for CanActivateAbility)
    virtual bool IsWeaponDrawn() const = 0;
    virtual bool IsAiming() const = 0;
    virtual bool IsFiring() const = 0;
    virtual bool IsReloading() const = 0;
    virtual bool IsHoldingBreath() const = 0;
    virtual bool IsMontageActive() const = 0;
    virtual float GetAimPoseAlpha() const = 0;

    // Commands (for ActivateAbility/EndAbility)
    virtual void SetAiming(bool bNewAiming) = 0;
    virtual void SetFiring(bool bNewFiring) = 0;
    virtual void SetReloading(bool bNewReloading) = 0;
    virtual void SetHoldingBreath(bool bNewHoldingBreath) = 0;
    virtual void SetMontageActive(bool bNewMontageActive) = 0;
};
```

---

## Creating a Weapon Ability

### Step 1: Define Native Tags

**File:** `Source/BridgeSystem/Public/SuspenseCore/Tags/SuspenseCoreGameplayTags.h`

```cpp
namespace SuspenseCoreTags
{
    namespace State
    {
        // Add state tag for your ability
        BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(YourNewState);
    }

    namespace Ability::Weapon
    {
        // Add ability tag
        BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(YourNewAbility);
    }
}
```

**File:** `Source/BridgeSystem/Private/SuspenseCore/Tags/SuspenseCoreGameplayTags.cpp`

```cpp
namespace SuspenseCoreTags
{
    namespace State
    {
        UE_DEFINE_GAMEPLAY_TAG(YourNewState, "State.YourNewState");
    }

    namespace Ability::Weapon
    {
        UE_DEFINE_GAMEPLAY_TAG(YourNewAbility, "SuspenseCore.Ability.Weapon.YourNewAbility");
    }
}
```

### Step 2: Update DefaultGameplayTags.ini

**File:** `Config/DefaultGameplayTags.ini`

```ini
; --- State Tags ---
+GameplayTagList=(Tag="State.YourNewState",DevComment="Description")

; --- Ability Tags ---
+GameplayTagList=(Tag="SuspenseCore.Ability.Weapon.YourNewAbility",DevComment="Description")
```

### Step 3: Add Input ID (if new input)

**File:** `Source/BridgeSystem/Public/SuspenseCore/Input/SuspenseCoreAbilityInputID.h`

```cpp
UENUM(BlueprintType)
enum class ESuspenseCoreAbilityInputID : uint8
{
    // ... existing ...
    YourNewInput    UMETA(DisplayName="Your New Input"),
};
```

### Step 4: Create Ability Header

**File:** `Source/GAS/Public/SuspenseCore/Abilities/Weapon/SuspenseCoreYourAbility.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Abilities/Base/SuspenseCoreAbility.h"
#include "SuspenseCoreYourAbility.generated.h"

class ISuspenseCoreWeaponCombatState;  // Forward declare INTERFACE, not class

UCLASS()
class GAS_API USuspenseCoreYourAbility : public USuspenseCoreAbility
{
    GENERATED_BODY()

public:
    USuspenseCoreYourAbility();

    // Configuration
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|YourAbility")
    TSubclassOf<UGameplayEffect> YourEffectClass;

protected:
    virtual bool CanActivateAbility(...) const override;
    virtual void ActivateAbility(...) override;
    virtual void EndAbility(...) override;

private:
    // Use INTERFACE, not concrete class
    ISuspenseCoreWeaponCombatState* GetWeaponCombatState() const;

    FActiveGameplayEffectHandle EffectHandle;
};
```

### Step 5: Create Ability Implementation

**File:** `Source/GAS/Private/SuspenseCore/Abilities/Weapon/SuspenseCoreYourAbility.cpp`

```cpp
#include "SuspenseCore/Abilities/Weapon/SuspenseCoreYourAbility.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeaponCombatState.h"  // Interface!
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "AbilitySystemComponent.h"

USuspenseCoreYourAbility::USuspenseCoreYourAbility()
{
    // Input binding
    AbilityInputID = ESuspenseCoreAbilityInputID::YourNewInput;

    // Ability tag
    AbilityTags.AddTag(SuspenseCoreTags::Ability::Weapon::YourNewAbility);

    // State added when active
    ActivationOwnedTags.AddTag(SuspenseCoreTags::State::YourNewState);

    // Blocking tags
    ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Dead);
    ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Stunned);

    // Network config
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

bool USuspenseCoreYourAbility::CanActivateAbility(...) const
{
    if (!Super::CanActivateAbility(...)) return false;

    ISuspenseCoreWeaponCombatState* CombatState = GetWeaponCombatState();
    if (!CombatState) return false;
    if (!CombatState->IsWeaponDrawn()) return false;

    return true;
}

void USuspenseCoreYourAbility::ActivateAbility(...)
{
    Super::ActivateAbility(...);

    // 1. Update SSOT (WeaponStanceComponent via interface)
    if (ISuspenseCoreWeaponCombatState* CombatState = GetWeaponCombatState())
    {
        CombatState->SetYourState(true);  // If applicable
    }

    // 2. Apply GameplayEffect
    if (YourEffectClass)
    {
        FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(YourEffectClass);
        EffectHandle = ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, Spec);
    }

    // 3. Publish EventBus event (if needed for camera/UI)
    if (USuspenseCoreEventBus* EventBus = GetEventBus())
    {
        FSuspenseCoreEventData EventData;
        EventBus->Publish(SuspenseCoreTags::Event::YourEvent, EventData);
    }
}

void USuspenseCoreYourAbility::EndAbility(...)
{
    // 1. Clear SSOT
    if (ISuspenseCoreWeaponCombatState* CombatState = GetWeaponCombatState())
    {
        CombatState->SetYourState(false);
    }

    // 2. Remove effect
    if (EffectHandle.IsValid() && CurrentActorInfo->AbilitySystemComponent.IsValid())
    {
        CurrentActorInfo->AbilitySystemComponent->RemoveActiveGameplayEffect(EffectHandle);
        EffectHandle.Invalidate();
    }

    Super::EndAbility(...);
}

ISuspenseCoreWeaponCombatState* USuspenseCoreYourAbility::GetWeaponCombatState() const
{
    const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid()) return nullptr;

    AActor* Avatar = ActorInfo->AvatarActor.Get();
    TArray<UActorComponent*> Components;
    Avatar->GetComponents(Components);

    for (UActorComponent* Comp : Components)
    {
        if (ISuspenseCoreWeaponCombatState* CombatState = Cast<ISuspenseCoreWeaponCombatState>(Comp))
        {
            return CombatState;
        }
    }
    return nullptr;
}
```

---

## Native Tags System

### Why Native Tags?

| Benefit | Description |
|---------|-------------|
| **Compile-time validation** | Typos caught at compile time, not runtime |
| **Autocomplete** | IDE shows available tags with `SuspenseCoreTags::` |
| **Refactoring** | Rename in one place, compiler finds all usages |
| **Performance** | Pre-hashed, no runtime string operations |

### Tag Naming Conventions

```
State Tags:           State.Aiming, State.Firing, State.Reloading
Ability Tags:         SuspenseCore.Ability.Weapon.AimDownSight
Event Tags:           SuspenseCore.Event.Camera.FOVChanged
Equipment Event Tags: SuspenseCore.Event.Equipment.Weapon.Stance.AimStarted
```

### Declaration Pattern

```cpp
// Header (.h) - declaration
namespace SuspenseCoreTags::State
{
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Aiming);
}

// Source (.cpp) - definition
namespace SuspenseCoreTags::State
{
    UE_DEFINE_GAMEPLAY_TAG(Aiming, "State.Aiming");
}

// Usage in ability
AbilityTags.AddTag(SuspenseCoreTags::State::Aiming);
```

### Existing Weapon Tags

```cpp
// State Tags
namespace SuspenseCoreTags::State
{
    Aiming          // "State.Aiming"
    Firing          // "State.Firing"
    Reloading       // "State.Reloading"
    HoldingBreath   // "State.HoldingBreath"
}

// Ability Tags
namespace SuspenseCoreTags::Ability::Weapon
{
    AimDownSight    // "SuspenseCore.Ability.Weapon.AimDownSight"
    Fire            // "SuspenseCore.Ability.Weapon.Fire"
    Reload          // "SuspenseCore.Ability.Weapon.Reload"
    FireModeSwitch  // "SuspenseCore.Ability.Weapon.FireModeSwitch"
    Inspect         // "SuspenseCore.Ability.Weapon.Inspect"
    HoldBreath      // "SuspenseCore.Ability.Weapon.HoldBreath"
}
```

---

## EventBus Integration

### When to Use EventBus

| Use Case | Example |
|----------|---------|
| **Camera changes** | FOV, DOF when aiming |
| **UI notifications** | Ammo count, reload indicator |
| **Sound triggers** | Play ADS sound |
| **Decoupled systems** | Any system that shouldn't poll state |

### Publishing Events from Ability

```cpp
void USuspenseCoreAimDownSightAbility::NotifyCameraFOVChange(bool bAiming)
{
    USuspenseCoreEventBus* EventBus = GetEventBus();
    if (!EventBus) return;

    FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetAvatarActorFromActorInfo());
    EventData.SetFloat(FName("TargetFOV"), bAiming ? 65.0f : 90.0f);
    EventData.SetFloat(FName("TransitionSpeed"), 15.0f);
    EventData.SetBool(FName("IsAiming"), bAiming);

    EventBus->Publish(SuspenseCoreTags::Event::Camera::FOVChanged, EventData);
}
```

### EventBus vs Direct Calls

```
WRONG (tight coupling):
  ADS Ability → Camera Component::SetFOV()
  Problem: GAS now depends on camera system

CORRECT (loose coupling):
  ADS Ability → EventBus::Publish(FOVChanged)
  Camera → EventBus::Subscribe(FOVChanged) → SetFOV()

  Benefit: GAS knows nothing about camera implementation
```

---

## SSOT Pattern

### Single Source of Truth: WeaponStanceComponent

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         SSOT: WeaponStanceComponent                          │
│                                                                              │
│  WRITERS (via interface):               READERS (via snapshot):              │
│  ├── ADS Ability: SetAiming()           ├── AnimInstance: GetStanceSnapshot()│
│  ├── Fire Ability: SetFiring()          │   → bIsAiming                     │
│  ├── Reload Ability: SetReloading()     │   → bIsFiring                     │
│  └── HoldBreath Ability: SetHoldBreath()│   → AimPoseAlpha (interpolated)   │
│                                         │   → RecoilAlpha (with decay)      │
│  INTERNAL PROCESSING:                   │                                    │
│  ├── AimPoseAlpha interpolation in Tick │                                    │
│  ├── RecoilAlpha decay in Tick          │                                    │
│  ├── Network replication (bool states)  │                                    │
│  └── EventBus publishing                │                                    │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Why SSOT Matters

| Without SSOT | With SSOT |
|--------------|-----------|
| ADS Ability stores bIsAiming | StanceComponent stores bIsAiming |
| Fire Ability stores bIsFiring | StanceComponent stores bIsFiring |
| AnimInstance checks 3 places | AnimInstance checks 1 place |
| State can be inconsistent | State is always consistent |
| Replication is complex | Replication is centralized |

### Reading from SSOT (AnimInstance)

```cpp
// AnimInstance::UpdateWeaponData()
void USuspenseCoreCharacterAnimInstance::UpdateWeaponData(float DeltaSeconds)
{
    if (!CachedStanceComponent.IsValid()) return;

    // ONE call gets ALL data
    const FSuspenseCoreWeaponStanceSnapshot Snapshot =
        CachedStanceComponent->GetStanceSnapshot();

    // Copy to local variables for AnimBP
    bIsAiming = Snapshot.bIsAiming;
    bIsFiring = Snapshot.bIsFiring;
    bIsReloading = Snapshot.bIsReloading;
    AimingAlpha = Snapshot.AimPoseAlpha;  // Already interpolated!
    RecoilAlpha = Snapshot.RecoilAlpha;   // Already with decay!
}
```

---

## Existing Abilities Reference

### AimDownSight Ability

**Location:** `Source/GAS/Public/SuspenseCore/Abilities/Weapon/SuspenseCoreAimDownSightAbility.h`

**Behavior:** Hold-to-aim (RMB pressed = aiming)

**Key Features:**
- Uses `ISuspenseCoreWeaponCombatState::SetAiming()`
- Applies speed debuff GameplayEffect
- Uses `UAbilityTask_WaitInputRelease` for hold behavior
- Animation automatically handled via `bIsAiming` flag in AnimInstance

**Tags Used:**
```cpp
AbilityTags: SuspenseCoreTags::Ability::Weapon::AimDownSight
ActivationOwnedTags: SuspenseCoreTags::State::Aiming
ActivationBlockedTags: State::Sprinting, State::Reloading, State::Dead, State::Stunned
CancelAbilitiesWithTag: SuspenseCoreTags::Ability::Sprint
```

**Configuration:**
```cpp
UPROPERTY() TSubclassOf<UGameplayEffect> AimSpeedDebuffClass;
```

---

## Checklists

### Before Creating New Weapon Ability

- [ ] Define Native Tags in `SuspenseCoreGameplayTags.h/.cpp`
- [ ] Add tags to `DefaultGameplayTags.ini`
- [ ] Add Input ID if new input needed
- [ ] Verify no circular dependencies in module

### During Implementation

- [ ] Use `ISuspenseCoreWeaponCombatState` interface, NOT concrete class
- [ ] Include interface header from BridgeSystem
- [ ] Set proper AbilityInputID in constructor
- [ ] Configure AbilityTags, ActivationOwnedTags, ActivationBlockedTags
- [ ] Set InstancingPolicy and NetExecutionPolicy

### CanActivateAbility Checks

- [ ] Call `Super::CanActivateAbility()` first
- [ ] Get interface via `GetWeaponCombatState()`
- [ ] Check `IsWeaponDrawn()` if weapon must be equipped
- [ ] Check conflicting states (e.g., `IsReloading()` for ADS)

### ActivateAbility Implementation

- [ ] Call `Super::ActivateAbility()` first
- [ ] Update SSOT via interface (`SetAiming()`, etc.)
- [ ] Apply GameplayEffects
- [ ] Publish EventBus events for camera/UI
- [ ] Setup input tasks if needed (WaitInputRelease)

### EndAbility Implementation

- [ ] Clear SSOT state via interface
- [ ] Remove applied GameplayEffects
- [ ] Publish cleanup EventBus events
- [ ] Call `Super::EndAbility()` last

### After Implementation

- [ ] Verify compilation succeeds
- [ ] Test in editor (PIE)
- [ ] Test multiplayer replication
- [ ] Verify AnimBP receives state correctly
- [ ] Document any special behavior

---

## Common Mistakes

### 1. Direct Class Reference Instead of Interface

```cpp
// WRONG - creates circular dependency
#include "SuspenseCore/Components/SuspenseCoreWeaponStanceComponent.h"
USuspenseCoreWeaponStanceComponent* Comp = GetComponent<...>();

// CORRECT - uses interface
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeaponCombatState.h"
ISuspenseCoreWeaponCombatState* CombatState = Cast<ISuspenseCoreWeaponCombatState>(Comp);
```

### 2. Forgetting to Update DefaultGameplayTags.ini

```ini
; Tags MUST be in .ini for editor to recognize them
; Native tags provide compile-time safety
; .ini provides runtime tag database

+GameplayTagList=(Tag="State.YourNewState",DevComment="...")
+GameplayTagList=(Tag="SuspenseCore.Ability.Weapon.YourAbility",DevComment="...")
```

### 3. Modifying State Directly Instead of SSOT

```cpp
// WRONG - bypasses SSOT
OwnerCharacter->bIsAiming = true;  // Where is this even defined?

// CORRECT - uses SSOT
CombatState->SetAiming(true);  // WeaponStanceComponent handles everything
```

### 4. Polling State Instead of EventBus

```cpp
// WRONG - camera polls every frame
void UCameraComponent::Tick()
{
    if (Character->IsAiming()) SetFOV(65.0f);  // Expensive!
}

// CORRECT - camera subscribes to event
void UCameraComponent::OnAimingChanged(FGameplayTag Tag, const FSuspenseCoreEventData& Data)
{
    SetTargetFOV(Data.GetFloat("TargetFOV"));  // Called only when state changes
}
```

### 5. Missing Network Configuration

```cpp
// WRONG - no network config
USuspenseCoreAimAbility::USuspenseCoreAimAbility()
{
    // Missing: ability won't replicate properly
}

// CORRECT
USuspenseCoreAimAbility::USuspenseCoreAimAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}
```

### 6. Storing Ability State Instead of Using SSOT

```cpp
// WRONG - ability stores its own state
class USuspenseCoreAimAbility
{
    bool bIsCurrentlyAiming;  // DON'T DO THIS
};

// CORRECT - query SSOT when needed
bool USuspenseCoreAimAbility::IsAiming() const
{
    ISuspenseCoreWeaponCombatState* CombatState = GetWeaponCombatState();
    return CombatState ? CombatState->IsAiming() : false;
}
```

---

## Files Reference

| File | Module | Purpose |
|------|--------|---------|
| `ISuspenseCoreWeaponCombatState.h` | BridgeSystem | Interface for weapon combat state |
| `SuspenseCoreGameplayTags.h/cpp` | BridgeSystem | Native tag declarations |
| `SuspenseCoreAbilityInputID.h` | BridgeSystem | Input ID enum |
| `SuspenseCoreWeaponStanceComponent.h` | EquipmentSystem | SSOT implementation |
| `SuspenseCoreAimDownSightAbility.h/cpp` | GAS | ADS ability |
| `SuspenseCoreAbility.h` | GAS | Base ability class |
| `SuspenseCoreCharacterAnimInstance.h` | PlayerCore | Animation SSOT |
| `DefaultGameplayTags.ini` | Config | Runtime tag database |

---

## Quick Architecture Reference

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    WEAPON ABILITY ARCHITECTURE SUMMARY                       │
│                                                                              │
│  Input → GAS Ability → Interface → SSOT → Snapshot → AnimBP                │
│                            │                                                 │
│                            └──→ EventBus → Camera/UI/Sound                  │
│                                                                              │
│  Modules:                                                                    │
│    BridgeSystem: ISuspenseCoreWeaponCombatState, Native Tags                │
│    GAS: Weapon abilities (uses interface)                                   │
│    EquipmentSystem: WeaponStanceComponent (implements interface)            │
│    PlayerCore: AnimInstance (reads from SSOT)                               │
│                                                                              │
│  Rules:                                                                      │
│    1. GAS abilities use INTERFACE, not concrete class                       │
│    2. All combat state goes through WeaponStanceComponent (SSOT)            │
│    3. AnimInstance PULLS from SSOT, does not receive pushes                 │
│    4. Camera/UI SUBSCRIBE to EventBus, do not poll                          │
│    5. Native Tags for compile-time safety + DefaultGameplayTags.ini         │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

**Remember:** The interface exists to break circular dependencies. Always use `ISuspenseCoreWeaponCombatState` in GAS abilities, never `USuspenseCoreWeaponStanceComponent` directly.
