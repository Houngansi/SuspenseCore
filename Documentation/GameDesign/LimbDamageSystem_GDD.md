# Limb Damage System - Game Design Document

**Document Version:** 1.0
**Last Updated:** 2026-01-17
**Author:** SuspenseCore Team
**Status:** Technical Specification

---

## Table of Contents

1. [Overview](#1-overview)
2. [Design Philosophy](#2-design-philosophy)
3. [Limb Health Model](#3-limb-health-model)
4. [Hit Detection & Damage Routing](#4-hit-detection--damage-routing)
5. [Damage Types & Armor Interaction](#5-damage-types--armor-interaction)
6. [Status Effects System](#6-status-effects-system)
7. [Limb Penalties](#7-limb-penalties)
8. [Fall Damage System](#8-fall-damage-system)
9. [Healing & Recovery](#9-healing--recovery)
10. [GAS Integration](#10-gas-integration)
11. [EventBus Integration](#11-eventbus-integration)
12. [Network Replication](#12-network-replication)
13. [UI Integration](#13-ui-integration)
14. [Performance Considerations](#14-performance-considerations)

---

## 1. Overview

### 1.1 Purpose

The Limb Damage System provides granular body-part damage tracking for realistic combat simulation. Players receive damage to specific limbs based on hit location, with cascading effects on movement, combat effectiveness, and survival.

### 1.2 Core Features

- **7 Body Zones:** Head, Thorax, Stomach, Left Arm, Right Arm, Left Leg, Right Leg
- **Zone-Specific Health Pools:** Independent HP tracking per limb
- **Damage Overflow:** Critical zones spread excess damage to adjacent zones
- **Status Effects:** Fractures, bleedings, destruction states
- **Dynamic Penalties:** Real-time debuffs based on limb condition
- **Fall Damage:** Height-based leg damage with fracture chance

### 1.3 Reference Games

- Escape from Tarkov (primary reference)
- SCUM
- DayZ
- Arma 3 ACE Medical

---

## 2. Design Philosophy

### 2.1 Core Principles

1. **Tactical Depth:** Limb targeting creates meaningful combat decisions
2. **Risk/Reward:** Exposed limbs vs protected vitals
3. **Progressive Degradation:** Gradual combat effectiveness loss
4. **Recovery Options:** Healing items restore specific limb functionality
5. **Network Efficiency:** Minimal bandwidth for MMO scale

### 2.2 Player Experience Goals

| Scenario | Expected Outcome |
|----------|------------------|
| Leg shot while sprinting | Immediate slowdown, potential fracture |
| Arm damage mid-reload | Slower reload completion |
| Head shot without helmet | Instant death or critical damage |
| Fall from 3m height | Leg damage + fracture chance |
| Destroyed leg | Cannot sprint, severe movement penalty |

### 2.3 Balance Considerations

- Arm damage affects combat but doesn't immobilize
- Leg damage restricts movement but allows fighting
- Stomach damage causes rapid HP drain (bleeding)
- Thorax/Head damage is immediately lethal at high values

---

## 3. Limb Health Model

### 3.1 Body Zone Definitions

```
                    ┌─────────┐
                    │  HEAD   │
                    │  35 HP  │
                    └────┬────┘
                         │
              ┌──────────┼──────────┐
              │          │          │
         ┌────┴────┐┌────┴────┐┌────┴────┐
         │LEFT ARM ││ THORAX  ││RIGHT ARM│
         │  60 HP  ││  85 HP  ││  60 HP  │
         └────┬────┘└────┬────┘└────┬────┘
              │          │          │
              │     ┌────┴────┐     │
              │     │ STOMACH │     │
              │     │  70 HP  │     │
              │     └────┬────┘     │
              │          │          │
         ┌────┴────┐     │     ┌────┴────┐
         │LEFT LEG │     │     │RIGHT LEG│
         │  65 HP  │     │     │  65 HP  │
         └─────────┘     │     └─────────┘
                         │
                    Total: 440 HP
```

### 3.2 Zone Properties

| Zone | Base HP | Critical | Overflow Target | Death Threshold |
|------|---------|----------|-----------------|-----------------|
| Head | 35 | Yes | Thorax | 0 HP = Death |
| Thorax | 85 | Yes | Stomach | 0 HP = Death |
| Stomach | 70 | No | Thorax (50%) | Spreads damage |
| Left Arm | 60 | No | Thorax (25%) | Destroyed state |
| Right Arm | 60 | No | Thorax (25%) | Destroyed state |
| Left Leg | 65 | No | Stomach (25%) | Destroyed state |
| Right Leg | 65 | No | Stomach (25%) | Destroyed state |

### 3.3 Damage Overflow Mechanics

When a non-critical limb reaches 0 HP:

1. **Limb enters "Destroyed" state**
2. **Excess damage overflows** to target zone
3. **Overflow multiplier:** 0.7x (30% damage reduction)

```cpp
// Pseudocode for damage overflow
void ApplyDamageToLimb(ELimbType Limb, float Damage)
{
    float CurrentHP = GetLimbHealth(Limb);
    float NewHP = CurrentHP - Damage;

    if (NewHP <= 0.0f && !IsCriticalLimb(Limb))
    {
        SetLimbHealth(Limb, 0.0f);
        SetLimbDestroyed(Limb, true);

        float OverflowDamage = FMath::Abs(NewHP) * OverflowMultiplier; // 0.7
        ELimbType OverflowTarget = GetOverflowTarget(Limb);
        ApplyDamageToLimb(OverflowTarget, OverflowDamage);
    }
    else
    {
        SetLimbHealth(Limb, FMath::Max(0.0f, NewHP));
    }
}
```

### 3.4 Critical Zone Rules

**Head (35 HP):**
- Instant death if damage ≥ 35 in single hit (without helmet)
- No destroyed state - death on 0 HP

**Thorax (85 HP):**
- Primary vital zone
- Receives overflow from all other zones
- Death on 0 HP

---

## 4. Hit Detection & Damage Routing

### 4.1 Bone-to-Limb Mapping

The system maps skeletal mesh bones to limb zones using a configuration table:

```cpp
USTRUCT(BlueprintType)
struct FBoneLimbMapping
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly)
    FName BoneName;

    UPROPERTY(EditDefaultsOnly)
    ELimbType LimbZone;

    UPROPERTY(EditDefaultsOnly)
    float DamageMultiplier = 1.0f;
};
```

### 4.2 Standard Bone Mapping Table

| Bone Pattern | Limb Zone | Multiplier | Notes |
|--------------|-----------|------------|-------|
| `head`, `neck_01` | Head | 1.0 | Headshots |
| `spine_03`, `clavicle_*` | Thorax | 1.0 | Upper chest |
| `spine_01`, `spine_02`, `pelvis` | Stomach | 1.0 | Lower torso |
| `upperarm_l`, `lowerarm_l`, `hand_l` | Left Arm | 1.0 | Full arm |
| `upperarm_r`, `lowerarm_r`, `hand_r` | Right Arm | 1.0 | Full arm |
| `thigh_l`, `calf_l`, `foot_l` | Left Leg | 1.0 | Full leg |
| `thigh_r`, `calf_r`, `foot_r` | Right Leg | 1.0 | Full leg |

### 4.3 Hit Processing Pipeline

```
[Projectile/Hitscan Impact]
           │
           ▼
┌─────────────────────────┐
│  Get Hit Bone Name      │
│  (FHitResult.BoneName)  │
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│  Lookup Limb Zone       │
│  (BoneLimbMapping)      │
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│  Check Armor Coverage   │
│  (Per-zone armor check) │
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│  Calculate Final Damage │
│  (Pen, armor, mult)     │
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│  Apply to Limb Health   │
│  (With overflow logic)  │
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│  Process Status Effects │
│  (Fracture, bleed, etc) │
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│  Broadcast EventBus     │
│  (UI, sound, effects)   │
└─────────────────────────┘
```

### 4.4 Fallback Zone Detection

When bone name is not found in mapping:

```cpp
ELimbType GetLimbFromHitLocation(const FVector& HitLocation, const AActor* Target)
{
    // Fallback: use relative height on character
    FVector LocalHit = Target->GetActorTransform().InverseTransformPosition(HitLocation);
    float RelativeHeight = LocalHit.Z / CharacterHeight;

    if (RelativeHeight > 0.85f) return ELimbType::Head;
    if (RelativeHeight > 0.55f) return ELimbType::Thorax;
    if (RelativeHeight > 0.40f) return ELimbType::Stomach;
    return ELimbType::Legs; // Further refined by X position
}
```

---

## 5. Damage Types & Armor Interaction

### 5.1 Damage Types

```cpp
UENUM(BlueprintType)
enum class EDamageCategory : uint8
{
    Ballistic,      // Bullets, fragments
    Blunt,          // Melee, fall damage
    Explosive,      // Grenade blast (non-fragment)
    Fire,           // Burns
    Environmental,  // Radiation, cold, etc.
};
```

### 5.2 Per-Limb Armor Coverage

Armor items protect specific zones:

| Armor Type | Protected Zones | Coverage % |
|------------|-----------------|------------|
| Helmet | Head | 80-100% |
| Body Armor | Thorax, Stomach | 85-95% |
| Arm Guards | Arms | 70-90% |
| Leg Armor | Legs | 70-90% |

### 5.3 Armor Penetration Calculation

```cpp
float CalculateArmorDamage(float BaseDamage, float Penetration, const FArmorData& Armor, ELimbType Limb)
{
    // Check if limb is covered
    if (!Armor.CoversLimb(Limb))
    {
        return BaseDamage; // Full damage
    }

    // Coverage roll
    if (FMath::FRand() > Armor.GetCoverage(Limb))
    {
        return BaseDamage; // Hit uncovered area
    }

    // Penetration vs Armor Class
    float PenChance = CalculatePenetrationChance(Penetration, Armor.ArmorClass);

    if (FMath::FRand() < PenChance)
    {
        // Penetrated - reduced damage
        float PenDamage = BaseDamage * (Penetration / (Penetration + Armor.ArmorClass));
        Armor.TakeDurabilityDamage(BaseDamage * 0.1f);
        return PenDamage;
    }
    else
    {
        // Blocked - blunt damage only
        float BluntDamage = BaseDamage * Armor.BluntThroughput;
        Armor.TakeDurabilityDamage(BaseDamage * 0.2f);
        return BluntDamage;
    }
}
```

---

## 6. Status Effects System

### 6.1 Status Effect Types

| Effect | Trigger | Duration | Cure |
|--------|---------|----------|------|
| Light Bleeding | Any limb damage (30% chance) | Until healed | Bandage, any medical |
| Heavy Bleeding | High damage hit (15% chance) | Until healed | Tourniquet, IFAK+ |
| Fracture | Blunt trauma / fall / destroyed limb | Until healed | Splint, Surgical Kit |
| Pain | Any significant damage | 60-300s | Painkillers, Morphine |
| Tremor | Low stamina + pain | While conditions met | Rest + painkillers |
| Destroyed | Limb HP = 0 | Until surgical repair | Surgical Kit only |

### 6.2 Bleeding System

```cpp
USTRUCT(BlueprintType)
struct FBleedingEffect
{
    GENERATED_BODY()

    UPROPERTY()
    ELimbType SourceLimb;

    UPROPERTY()
    EBleedingLevel Level; // Light, Heavy

    UPROPERTY()
    float DamagePerSecond; // Light: 0.3/s, Heavy: 1.2/s

    UPROPERTY()
    float TimeAccumulator;
};
```

**Bleeding Tick Logic:**

```cpp
void ProcessBleeding(float DeltaTime)
{
    for (FBleedingEffect& Bleed : ActiveBleeds)
    {
        Bleed.TimeAccumulator += DeltaTime;

        if (Bleed.TimeAccumulator >= 1.0f)
        {
            Bleed.TimeAccumulator -= 1.0f;

            // Apply damage to source limb
            ApplyDamageToLimb(Bleed.SourceLimb, Bleed.DamagePerSecond);

            // Broadcast for UI blood effect
            EventBus->Publish(FSuspenseCoreBleedTickEvent(OwnerActor, Bleed));
        }
    }
}
```

### 6.3 Fracture System

**Fracture Triggers:**

| Cause | Fracture Chance | Notes |
|-------|-----------------|-------|
| Fall damage ≥ 20 | 50% per leg | Height dependent |
| Fall damage ≥ 40 | 100% per leg | Severe fall |
| Blunt damage ≥ 30 | 25% | Limb specific |
| Limb destroyed | 100% | Automatic |
| Explosive nearby | 15% legs | Blast wave |

**Fracture State:**

```cpp
USTRUCT(BlueprintType)
struct FLimbFractureState
{
    GENERATED_BODY()

    UPROPERTY()
    ELimbType Limb;

    UPROPERTY()
    bool bIsFractured = false;

    UPROPERTY()
    bool bIsSplinted = false; // Temporary fix

    // Splint reduces penalty by 50% but doesn't cure
    float GetPenaltyMultiplier() const
    {
        if (!bIsFractured) return 1.0f;
        return bIsSplinted ? 0.75f : 0.5f;
    }
};
```

### 6.4 Pain System

Pain accumulates from damage and affects combat effectiveness:

```cpp
// Pain calculation
float CalculatePainFromDamage(float Damage, ELimbType Limb)
{
    float BasePain = Damage * 0.5f;

    // Head/Thorax cause more pain
    if (Limb == ELimbType::Head) BasePain *= 2.0f;
    if (Limb == ELimbType::Thorax) BasePain *= 1.5f;

    return BasePain;
}

// Pain decay
void TickPain(float DeltaTime)
{
    CurrentPain = FMath::Max(0.0f, CurrentPain - PainDecayRate * DeltaTime);
}
```

**Pain Thresholds:**

| Pain Level | Threshold | Effects |
|------------|-----------|---------|
| None | 0-20 | No effects |
| Light | 21-50 | Minor tremor |
| Moderate | 51-80 | Tremor + tunnel vision |
| Severe | 81-100 | All above + blurred vision |

---

## 7. Limb Penalties

### 7.1 Penalty Categories

```cpp
USTRUCT(BlueprintType)
struct FLimbPenalties
{
    GENERATED_BODY()

    // Movement
    UPROPERTY() float MovementSpeedMult = 1.0f;
    UPROPERTY() float SprintSpeedMult = 1.0f;
    UPROPERTY() bool bCanSprint = true;

    // Combat
    UPROPERTY() float ReloadSpeedMult = 1.0f;
    UPROPERTY() float AimSpeedMult = 1.0f;
    UPROPERTY() float WeaponSwayMult = 1.0f;
    UPROPERTY() float RecoilControlMult = 1.0f;

    // Interaction
    UPROPERTY() float InteractionSpeedMult = 1.0f;
    UPROPERTY() float HealingSpeedMult = 1.0f;
};
```

### 7.2 Leg Penalties

| Condition | Walk Speed | Sprint Speed | Sprint Allowed |
|-----------|------------|--------------|----------------|
| Healthy | 100% | 100% | Yes |
| Damaged (50% HP) | 90% | 85% | Yes |
| Damaged (25% HP) | 75% | 60% | Yes |
| Fractured | 60% | 40% | Yes (painful) |
| Fractured + Splint | 75% | 60% | Yes |
| Destroyed | 45% | N/A | No |
| Both legs damaged | Compound | Compound | Depends |

**Leg Penalty Calculation:**

```cpp
FLimbPenalties CalculateLegPenalties()
{
    FLimbPenalties Result;

    float LeftLegMod = CalculateLegModifier(ELimbType::LeftLeg);
    float RightLegMod = CalculateLegModifier(ELimbType::RightLeg);

    // Use worse leg for sprint, average for walk
    Result.MovementSpeedMult = (LeftLegMod + RightLegMod) / 2.0f;
    Result.SprintSpeedMult = FMath::Min(LeftLegMod, RightLegMod);

    // Can't sprint with destroyed leg
    Result.bCanSprint = !IsLimbDestroyed(ELimbType::LeftLeg) &&
                        !IsLimbDestroyed(ELimbType::RightLeg);

    return Result;
}

float CalculateLegModifier(ELimbType Leg)
{
    float HealthPercent = GetLimbHealthPercent(Leg);
    float Modifier = FMath::Lerp(0.45f, 1.0f, HealthPercent);

    // Apply fracture penalty
    Modifier *= GetFractureState(Leg).GetPenaltyMultiplier();

    // Destroyed limb override
    if (IsLimbDestroyed(Leg))
    {
        Modifier = 0.45f;
    }

    return Modifier;
}
```

### 7.3 Arm Penalties

| Condition | Reload Speed | Aim Speed | Weapon Sway | Recoil Control |
|-----------|--------------|-----------|-------------|----------------|
| Healthy | 100% | 100% | 100% | 100% |
| Damaged (50% HP) | 90% | 95% | 110% | 95% |
| Damaged (25% HP) | 75% | 85% | 130% | 85% |
| Fractured | 60% | 70% | 150% | 70% |
| Destroyed | 40% | 50% | 200% | 50% |

**Dominant Hand Consideration:**

```cpp
float CalculateArmPenalty(EArmPenaltyType Type)
{
    float LeftMod = GetArmModifier(ELimbType::LeftArm, Type);
    float RightMod = GetArmModifier(ELimbType::RightArm, Type);

    // Right arm (dominant) has more impact
    // 60% right arm, 40% left arm
    return (RightMod * 0.6f) + (LeftMod * 0.4f);
}
```

### 7.4 Torso Penalties (Stomach)

| Condition | Stamina Regen | Hold Breath Duration |
|-----------|---------------|---------------------|
| Healthy | 100% | 100% |
| Damaged (50% HP) | 70% | 80% |
| Damaged (25% HP) | 40% | 50% |
| Destroyed | 20% | 25% |

### 7.5 Combined Penalty Aggregation

```cpp
FLimbPenalties AggregatePenalties()
{
    FLimbPenalties Total;

    // Leg penalties (movement)
    FLimbPenalties LegPenalties = CalculateLegPenalties();
    Total.MovementSpeedMult = LegPenalties.MovementSpeedMult;
    Total.SprintSpeedMult = LegPenalties.SprintSpeedMult;
    Total.bCanSprint = LegPenalties.bCanSprint;

    // Arm penalties (combat)
    Total.ReloadSpeedMult = CalculateArmPenalty(EArmPenaltyType::Reload);
    Total.AimSpeedMult = CalculateArmPenalty(EArmPenaltyType::Aim);
    Total.WeaponSwayMult = 1.0f / CalculateArmPenalty(EArmPenaltyType::Stability);
    Total.RecoilControlMult = CalculateArmPenalty(EArmPenaltyType::Recoil);

    // Pain overlay
    float PainModifier = 1.0f - (CurrentPain / 100.0f) * 0.3f; // Max 30% penalty
    Total.AimSpeedMult *= PainModifier;
    Total.WeaponSwayMult /= PainModifier;

    return Total;
}
```

---

## 8. Fall Damage System

### 8.1 Fall Damage Calculation

```cpp
USTRUCT(BlueprintType)
struct FFallDamageConfig
{
    GENERATED_BODY()

    // Heights in centimeters
    UPROPERTY(EditDefaultsOnly)
    float SafeFallHeight = 200.0f; // 2m - no damage

    UPROPERTY(EditDefaultsOnly)
    float MinDamageHeight = 300.0f; // 3m - damage starts

    UPROPERTY(EditDefaultsOnly)
    float LethalHeight = 1500.0f; // 15m - instant death

    UPROPERTY(EditDefaultsOnly)
    float DamagePerMeter = 8.0f; // Base damage per meter over threshold

    UPROPERTY(EditDefaultsOnly)
    float FractureThreshold = 20.0f; // Damage threshold for fracture roll
};
```

### 8.2 Fall Damage Pipeline

```
[Character Lands]
       │
       ▼
┌──────────────────┐
│ Get Fall Height  │
│ (Z velocity)     │
└────────┬─────────┘
         │
         ▼
    Height < 2m?
    ┌────┴────┐
   Yes       No
    │         │
    ▼         ▼
  [None]  Calculate
          Damage
              │
              ▼
      ┌───────────────┐
      │ Apply to Legs │
      │ (50% each)    │
      └───────┬───────┘
              │
              ▼
      ┌───────────────┐
      │ Fracture Roll │
      │ (if > 20 dmg) │
      └───────┬───────┘
              │
              ▼
      ┌───────────────┐
      │ EventBus      │
      │ Broadcast     │
      └───────────────┘
```

### 8.3 Fall Damage Implementation

```cpp
void ProcessFallDamage(float FallHeight)
{
    // Height in cm
    if (FallHeight < FallConfig.SafeFallHeight)
    {
        return; // No damage
    }

    // Check lethal fall
    if (FallHeight >= FallConfig.LethalHeight)
    {
        // Instant death
        ApplyDamageToLimb(ELimbType::Thorax, 9999.0f);
        return;
    }

    // Calculate damage
    float DamageableHeight = FallHeight - FallConfig.SafeFallHeight;
    float MetersOver = DamageableHeight / 100.0f;
    float TotalDamage = MetersOver * FallConfig.DamagePerMeter;

    // Exponential scaling for high falls
    if (MetersOver > 5.0f)
    {
        TotalDamage *= FMath::Pow(1.15f, MetersOver - 5.0f);
    }

    // Split damage between legs
    float PerLegDamage = TotalDamage / 2.0f;

    ApplyDamageToLimb(ELimbType::LeftLeg, PerLegDamage);
    ApplyDamageToLimb(ELimbType::RightLeg, PerLegDamage);

    // Fracture rolls
    if (PerLegDamage >= FallConfig.FractureThreshold)
    {
        float FractureChance = FMath::GetMappedRangeValueClamped(
            FVector2D(20.0f, 40.0f),
            FVector2D(0.5f, 1.0f),
            PerLegDamage
        );

        if (FMath::FRand() < FractureChance)
        {
            ApplyFracture(ELimbType::LeftLeg);
        }
        if (FMath::FRand() < FractureChance)
        {
            ApplyFracture(ELimbType::RightLeg);
        }
    }

    // Broadcast event
    EventBus->Publish(FSuspenseCoreFallDamageEvent(
        OwnerActor,
        FallHeight,
        TotalDamage
    ));
}
```

### 8.4 Fall Damage Table

| Fall Height | Damage per Leg | Fracture Chance | Notes |
|-------------|----------------|-----------------|-------|
| 0-2m | 0 | 0% | Safe |
| 3m | 4 | 0% | Minor |
| 4m | 8 | 0% | Noticeable |
| 5m | 12 | 0% | Significant |
| 6m | 16 | 0% | Painful |
| 7m | 22 | 55% | Fracture likely |
| 8m | 28 | 70% | High risk |
| 10m | 44 | 100% | Guaranteed fracture |
| 15m+ | Death | - | Lethal |

### 8.5 Fall Damage Modifiers

```cpp
float GetFallDamageModifier(ACharacter* Character)
{
    float Modifier = 1.0f;

    // Soft landing (crouch during landing)
    if (Character->bIsCrouched)
    {
        Modifier *= 0.7f; // 30% reduction
    }

    // Boot armor (if implemented)
    if (HasBootArmor(Character))
    {
        Modifier *= 0.85f; // 15% reduction
    }

    // Stamina exhaustion increases damage
    float StaminaPercent = GetStaminaPercent(Character);
    if (StaminaPercent < 0.2f)
    {
        Modifier *= 1.25f; // 25% more damage
    }

    return Modifier;
}
```

---

## 9. Healing & Recovery

### 9.1 Medical Items for Limb System

| Item | Heals HP | Stops Light Bleed | Stops Heavy Bleed | Cures Fracture | Restores Destroyed |
|------|----------|-------------------|-------------------|----------------|-------------------|
| Bandage | 0 | Yes | No | No | No |
| Tourniquet | 0 | Yes | Yes | No | No |
| AI-2 | 50 | No | No | No | No |
| CAR | 50 | Yes | No | No | No |
| IFAK | 150 | Yes | Yes | No | No |
| AFAK | 250 | Yes | Yes | No | No |
| Salewa | 300 | Yes | Yes | No | No |
| Grizzly | 600 | Yes | Yes | No | No |
| Splint | 0 | No | No | Splints* | No |
| Surgical Kit | 0 | No | No | Yes | Yes |

*Splint provides temporary fracture relief, not full cure.

### 9.2 Limb-Specific Healing

```cpp
void HealLimb(ELimbType Limb, float Amount, const FMedicalItemData& Item)
{
    // Check if item can heal this limb
    if (!Item.CanHealLimb(Limb))
    {
        return;
    }

    // Get current state
    float CurrentHP = GetLimbHealth(Limb);
    float MaxHP = GetLimbMaxHealth(Limb);

    // Can't heal destroyed limb with normal meds
    if (IsLimbDestroyed(Limb) && !Item.bCanRestoreDestroyed)
    {
        return;
    }

    // Restore destroyed state first (surgical kit)
    if (IsLimbDestroyed(Limb) && Item.bCanRestoreDestroyed)
    {
        SetLimbDestroyed(Limb, false);
        SetLimbHealth(Limb, MaxHP * 0.1f); // Restore to 10%
    }

    // Apply healing
    float NewHP = FMath::Min(MaxHP, CurrentHP + Amount);
    SetLimbHealth(Limb, NewHP);

    // Stop bleedings
    if (Item.bStopsLightBleeding)
    {
        RemoveBleedingFromLimb(Limb, EBleedingLevel::Light);
    }
    if (Item.bStopsHeavyBleeding)
    {
        RemoveBleedingFromLimb(Limb, EBleedingLevel::Heavy);
    }

    // Fracture handling
    if (Item.bCuresFracture)
    {
        CureFracture(Limb);
    }
    else if (Item.bSplintsFracture && HasFracture(Limb))
    {
        ApplySplint(Limb);
    }

    // Broadcast
    EventBus->Publish(FSuspenseCoreLimbHealedEvent(OwnerActor, Limb, Amount));
}
```

### 9.3 Healing Priority System

When using medical item without targeting specific limb:

```cpp
ELimbType GetPriorityHealTarget(const FMedicalItemData& Item)
{
    // Priority 1: Critical zones with low HP
    if (GetLimbHealthPercent(ELimbType::Thorax) < 0.3f)
        return ELimbType::Thorax;
    if (GetLimbHealthPercent(ELimbType::Head) < 0.5f)
        return ELimbType::Head;

    // Priority 2: Heavy bleeding
    for (ELimbType Limb : GetLimbsWithHeavyBleeding())
    {
        if (Item.bStopsHeavyBleeding)
            return Limb;
    }

    // Priority 3: Destroyed limbs
    for (ELimbType Limb : GetDestroyedLimbs())
    {
        if (Item.bCanRestoreDestroyed)
            return Limb;
    }

    // Priority 4: Lowest HP limb
    return GetLowestHealthLimb();
}
```

---

## 10. GAS Integration

### 10.1 Limb Health AttributeSet

```cpp
UCLASS()
class USuspenseCoreLimbAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    // === Health Attributes ===
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_HeadHealth)
    FGameplayAttributeData HeadHealth;
    ATTRIBUTE_ACCESSORS(USuspenseCoreLimbAttributeSet, HeadHealth)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ThoraxHealth)
    FGameplayAttributeData ThoraxHealth;
    ATTRIBUTE_ACCESSORS(USuspenseCoreLimbAttributeSet, ThoraxHealth)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_StomachHealth)
    FGameplayAttributeData StomachHealth;
    ATTRIBUTE_ACCESSORS(USuspenseCoreLimbAttributeSet, StomachHealth)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_LeftArmHealth)
    FGameplayAttributeData LeftArmHealth;
    ATTRIBUTE_ACCESSORS(USuspenseCoreLimbAttributeSet, LeftArmHealth)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_RightArmHealth)
    FGameplayAttributeData RightArmHealth;
    ATTRIBUTE_ACCESSORS(USuspenseCoreLimbAttributeSet, RightArmHealth)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_LeftLegHealth)
    FGameplayAttributeData LeftLegHealth;
    ATTRIBUTE_ACCESSORS(USuspenseCoreLimbAttributeSet, LeftLegHealth)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_RightLegHealth)
    FGameplayAttributeData RightLegHealth;
    ATTRIBUTE_ACCESSORS(USuspenseCoreLimbAttributeSet, RightLegHealth)

    // === Max Health (for percentage calculations) ===
    UPROPERTY(BlueprintReadOnly, Category = "Max Health")
    FGameplayAttributeData MaxHeadHealth;
    ATTRIBUTE_ACCESSORS(USuspenseCoreLimbAttributeSet, MaxHeadHealth)

    // ... (similar for all limbs)

    // === Penalty Modifiers (applied by GameplayEffects) ===
    UPROPERTY(BlueprintReadOnly, Category = "Penalties")
    FGameplayAttributeData MovementSpeedPenalty;
    ATTRIBUTE_ACCESSORS(USuspenseCoreLimbAttributeSet, MovementSpeedPenalty)

    UPROPERTY(BlueprintReadOnly, Category = "Penalties")
    FGameplayAttributeData ReloadSpeedPenalty;
    ATTRIBUTE_ACCESSORS(USuspenseCoreLimbAttributeSet, ReloadSpeedPenalty)

    UPROPERTY(BlueprintReadOnly, Category = "Penalties")
    FGameplayAttributeData AimSpeedPenalty;
    ATTRIBUTE_ACCESSORS(USuspenseCoreLimbAttributeSet, AimSpeedPenalty)

    UPROPERTY(BlueprintReadOnly, Category = "Penalties")
    FGameplayAttributeData WeaponSwayMultiplier;
    ATTRIBUTE_ACCESSORS(USuspenseCoreLimbAttributeSet, WeaponSwayMultiplier)

    // === Status Flags (replicated as tags, not attributes) ===
    // Fractures, bleedings, destroyed states use GameplayTags

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};
```

### 10.2 GameplayTags for Limb States

```cpp
// Native GameplayTags declaration
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Limb_Head_Destroyed);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Limb_Thorax_Critical);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Limb_LeftArm_Destroyed);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Limb_LeftArm_Fractured);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Limb_RightArm_Destroyed);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Limb_RightArm_Fractured);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Limb_LeftLeg_Destroyed);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Limb_LeftLeg_Fractured);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Limb_RightLeg_Destroyed);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Limb_RightLeg_Fractured);

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Bleeding_Light);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Bleeding_Heavy);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Pain_Light);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Pain_Moderate);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Pain_Severe);
```

### 10.3 Damage Application via Gameplay Effect

```cpp
UCLASS()
class UGE_LimbDamage : public UGameplayEffect
{
    // Configured in Blueprint/DataAsset
    // Duration: Instant
    // Modifiers:
    //   - Attribute: LimbAttributeSet.XXXHealth
    //   - Operation: Additive
    //   - Magnitude: SetByCaller (Tag: Damage.Limb.Amount)

    // Executions:
    //   - ULimbDamageExecution (handles overflow, status effects)
};

// Damage Execution Calculation
UCLASS()
class ULimbDamageExecution : public UGameplayEffectExecutionCalculation
{
    GENERATED_BODY()

public:
    virtual void Execute_Implementation(
        const FGameplayEffectCustomExecutionParameters& ExecutionParams,
        FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override
    {
        // Get target limb from effect context
        ELimbType TargetLimb = GetLimbFromEffectContext(ExecutionParams);

        // Get damage amount
        float Damage = 0.0f;
        ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
            DamageStatics().DamageAmountDef,
            EvaluationParameters,
            Damage
        );

        // Apply to appropriate attribute
        FGameplayAttribute LimbAttribute = GetLimbHealthAttribute(TargetLimb);
        OutExecutionOutput.AddOutputModifier(
            FGameplayModifierEvaluatedData(
                LimbAttribute,
                EGameplayModOp::Additive,
                -Damage
            )
        );

        // Trigger status effect checks in PostGameplayEffectExecute
    }
};
```

### 10.4 Penalty Application Effects

```cpp
// Example: Leg Fracture Effect
UCLASS()
class UGE_LegFracture : public UGameplayEffect
{
    // Duration: Infinite (until removed)
    // Period: None

    // Modifiers:
    //   - MovementSpeedPenalty: Multiply 0.6 (40% reduction)
    //   - SprintSpeedPenalty: Multiply 0.4 (60% reduction)

    // Granted Tags:
    //   - TAG_State_Limb_XXXLeg_Fractured

    // Gameplay Cues:
    //   - GameplayCue.Limb.Fracture (for pain grunt, limp animation)
};

// Example: Heavy Bleeding Effect
UCLASS()
class UGE_HeavyBleeding : public UGameplayEffect
{
    // Duration: Infinite
    // Period: 1.0s

    // Periodic Execution:
    //   - Apply 1.2 damage to source limb each period

    // Granted Tags:
    //   - TAG_State_Bleeding_Heavy

    // Gameplay Cues:
    //   - GameplayCue.Status.Bleeding.Heavy (blood drip VFX)
};
```

### 10.5 Ability Modifications Based on Limb State

```cpp
// In Reload Ability
void UGA_Reload::ActivateAbility(...)
{
    // Get reload speed modifier from attribute
    float ReloadSpeedMod = AbilitySystemComponent->GetNumericAttribute(
        USuspenseCoreLimbAttributeSet::GetReloadSpeedPenaltyAttribute()
    );

    // Apply to montage playrate
    float PlayRate = BaseReloadSpeed * ReloadSpeedMod;
    PlayMontage(ReloadMontage, PlayRate);
}

// In Movement Component
void USuspenseCoreMovementComponent::GetMaxSpeed()
{
    float BaseSpeed = Super::GetMaxSpeed();

    // Query penalty attribute
    if (AbilitySystemComponent)
    {
        float MovementPenalty = AbilitySystemComponent->GetNumericAttribute(
            USuspenseCoreLimbAttributeSet::GetMovementSpeedPenaltyAttribute()
        );
        return BaseSpeed * MovementPenalty;
    }

    return BaseSpeed;
}
```

---

## 11. EventBus Integration

### 11.1 Limb Damage Events

```cpp
// Base limb event
USTRUCT(BlueprintType)
struct FSuspenseCoreLimbDamageEvent : public FSuspenseCoreEventBase
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<AActor> VictimActor;

    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<AActor> InstigatorActor;

    UPROPERTY(BlueprintReadOnly)
    ELimbType DamagedLimb;

    UPROPERTY(BlueprintReadOnly)
    float DamageAmount;

    UPROPERTY(BlueprintReadOnly)
    float RemainingHealth;

    UPROPERTY(BlueprintReadOnly)
    float HealthPercent;

    UPROPERTY(BlueprintReadOnly)
    bool bLimbDestroyed;

    UPROPERTY(BlueprintReadOnly)
    EDamageCategory DamageType;
};
```

### 11.2 Status Effect Events

```cpp
USTRUCT(BlueprintType)
struct FSuspenseCoreBleedingStartEvent : public FSuspenseCoreEventBase
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<AActor> AffectedActor;

    UPROPERTY(BlueprintReadOnly)
    ELimbType SourceLimb;

    UPROPERTY(BlueprintReadOnly)
    EBleedingLevel BleedingLevel;
};

USTRUCT(BlueprintType)
struct FSuspenseCoreFractureEvent : public FSuspenseCoreEventBase
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<AActor> AffectedActor;

    UPROPERTY(BlueprintReadOnly)
    ELimbType FracturedLimb;

    UPROPERTY(BlueprintReadOnly)
    bool bFromFall;

    UPROPERTY(BlueprintReadOnly)
    float FallHeight; // 0 if not from fall
};
```

### 11.3 Healing Events

```cpp
USTRUCT(BlueprintType)
struct FSuspenseCoreLimbHealedEvent : public FSuspenseCoreEventBase
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<AActor> HealedActor;

    UPROPERTY(BlueprintReadOnly)
    ELimbType HealedLimb;

    UPROPERTY(BlueprintReadOnly)
    float HealAmount;

    UPROPERTY(BlueprintReadOnly)
    float NewHealth;

    UPROPERTY(BlueprintReadOnly)
    bool bLimbRestored; // Was destroyed, now functional
};

USTRUCT(BlueprintType)
struct FSuspenseCoreStatusCuredEvent : public FSuspenseCoreEventBase
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<AActor> CuredActor;

    UPROPERTY(BlueprintReadOnly)
    ELimbType AffectedLimb;

    UPROPERTY(BlueprintReadOnly)
    FGameplayTag CuredStatus; // TAG_State_Bleeding_Heavy, etc.
};
```

### 11.4 Fall Damage Event

```cpp
USTRUCT(BlueprintType)
struct FSuspenseCoreFallDamageEvent : public FSuspenseCoreEventBase
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<AActor> FallenActor;

    UPROPERTY(BlueprintReadOnly)
    float FallHeight; // In cm

    UPROPERTY(BlueprintReadOnly)
    float TotalDamage;

    UPROPERTY(BlueprintReadOnly)
    bool bLeftLegFractured;

    UPROPERTY(BlueprintReadOnly)
    bool bRightLegFractured;

    UPROPERTY(BlueprintReadOnly)
    bool bLethalFall;
};
```

### 11.5 Event Publishing Examples

```cpp
// After applying limb damage
void PublishLimbDamageEvent(AActor* Victim, AActor* Instigator, ELimbType Limb, float Damage)
{
    FSuspenseCoreLimbDamageEvent Event;
    Event.VictimActor = Victim;
    Event.InstigatorActor = Instigator;
    Event.DamagedLimb = Limb;
    Event.DamageAmount = Damage;
    Event.RemainingHealth = GetLimbHealth(Limb);
    Event.HealthPercent = GetLimbHealthPercent(Limb);
    Event.bLimbDestroyed = IsLimbDestroyed(Limb);

    if (USuspenseCoreEventBus* EventBus = GetEventBus())
    {
        EventBus->Publish(Event);
    }
}

// UI widget subscribing to events
void ULimbStatusWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (USuspenseCoreEventBus* EventBus = GetEventBus())
    {
        EventBus->Subscribe<FSuspenseCoreLimbDamageEvent>(
            this,
            &ULimbStatusWidget::OnLimbDamage
        );

        EventBus->Subscribe<FSuspenseCoreBleedingStartEvent>(
            this,
            &ULimbStatusWidget::OnBleedingStart
        );
    }
}
```

---

## 12. Network Replication

### 12.1 Replication Strategy

**Server Authority:**
- All damage calculations
- Status effect application
- Death determination
- Healing validation

**Replicated to Clients:**
- Limb health values (via AttributeSet)
- Status effect tags (via ASC)
- Visual/audio cues (via GameplayCues)

### 12.2 Compact Limb State Replication

```cpp
USTRUCT()
struct FReplicatedLimbState
{
    GENERATED_BODY()

    // Pack all 7 limb health values into bytes (0-255 mapped to 0-100%)
    UPROPERTY()
    uint8 HeadHealthPct;      // 1 byte

    UPROPERTY()
    uint8 ThoraxHealthPct;    // 1 byte

    UPROPERTY()
    uint8 StomachHealthPct;   // 1 byte

    UPROPERTY()
    uint8 LeftArmHealthPct;   // 1 byte

    UPROPERTY()
    uint8 RightArmHealthPct;  // 1 byte

    UPROPERTY()
    uint8 LeftLegHealthPct;   // 1 byte

    UPROPERTY()
    uint8 RightLegHealthPct;  // 1 byte

    // Status flags packed into bitfield
    UPROPERTY()
    uint16 StatusFlags;       // 2 bytes
    // Bit 0: LeftArm Fractured
    // Bit 1: RightArm Fractured
    // Bit 2: LeftLeg Fractured
    // Bit 3: RightLeg Fractured
    // Bit 4: LeftArm Destroyed
    // Bit 5: RightArm Destroyed
    // Bit 6: LeftLeg Destroyed
    // Bit 7: RightLeg Destroyed
    // Bit 8-10: Active bleed count (0-7)
    // Bit 11: Has Heavy Bleed
    // Bit 12-15: Pain level (0-15)

    // Total: 9 bytes per character
};
```

### 12.3 Damage Event Replication

```cpp
// Server -> Client damage notification (unreliable)
UFUNCTION(NetMulticast, Unreliable)
void Multicast_OnLimbDamaged(ELimbType Limb, uint8 DamageAmount, bool bDestroyed);

// Implementation
void Multicast_OnLimbDamaged_Implementation(ELimbType Limb, uint8 DamageAmount, bool bDestroyed)
{
    // Client-side only: play effects
    if (!HasAuthority())
    {
        // Trigger hit reaction animation
        PlayLimbHitReaction(Limb, DamageAmount);

        // Blood VFX
        SpawnBloodEffect(Limb);

        // Pain grunt
        if (DamageAmount > 15)
        {
            PlayPainSound(DamageAmount);
        }
    }
}
```

### 12.4 Bandwidth Analysis

**Per Character (replicated state):**
- Base limb state: 9 bytes
- AttributeSet replication: ~28 bytes (7 floats, delta compressed)
- GameplayTags: Variable, typically 4-8 bytes for active status

**Per Damage Event:**
- Multicast call: ~8 bytes
- Sent only when damage occurs

**For 64 players:**
- Static state: 64 × 9 = 576 bytes/tick (worst case)
- With delta compression: ~50-100 bytes/tick typical
- Damage events: ~8 bytes per hit (unreliable, can drop)

### 12.5 Relevancy Optimization

```cpp
// Only replicate full limb state to nearby players
bool ASuspenseCoreCharacter::IsNetRelevantFor(
    const AActor* RealViewer,
    const AActor* ViewTarget,
    const FVector& SrcLocation) const
{
    // Full limb state relevancy: 50m radius
    float DistSq = FVector::DistSquared(GetActorLocation(), SrcLocation);

    if (DistSq > FMath::Square(5000.0f)) // 50m
    {
        // Only replicate critical state (alive/dead)
        return false;
    }

    return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}
```

---

## 13. UI Integration

### 13.1 Health Display Widget

```
┌─────────────────────────────────────┐
│           HEALTH STATUS             │
├─────────────────────────────────────┤
│                                     │
│              ┌───┐                  │
│              │ H │ ← Head (100%)    │
│              └─┬─┘                  │
│         ┌─────┼─────┐               │
│      ┌──┴──┐┌─┴─┐┌──┴──┐            │
│      │ LA ││ T ││ RA │              │
│      │75% ││85%││60% │              │
│      └──┬──┘└─┬─┘└──┬──┘            │
│         │  ┌─┴─┐   │                │
│         │  │ S │   │                │
│         │  │70%│   │                │
│         │  └─┬─┘   │                │
│      ┌──┴──┐ │ ┌──┴──┐              │
│      │ LL │ │ │ RL │               │
│      │ ⚠️ │   │ ✓  │               │
│      │45% │   │65% │               │
│      └────┘   └────┘               │
│                                     │
│  ⚠️ = Fractured  🩸 = Bleeding      │
│  ❌ = Destroyed                     │
└─────────────────────────────────────┘
```

### 13.2 Widget Color Coding

```cpp
FLinearColor GetLimbHealthColor(float HealthPercent, bool bDestroyed, bool bFractured)
{
    if (bDestroyed)
    {
        return FLinearColor(0.2f, 0.2f, 0.2f); // Dark gray
    }

    if (bFractured)
    {
        // Pulsing orange
        float Pulse = (FMath::Sin(GetWorld()->GetTimeSeconds() * 4.0f) + 1.0f) / 2.0f;
        return FLinearColor::LerpUsingHSV(
            FLinearColor(1.0f, 0.5f, 0.0f),
            FLinearColor(1.0f, 0.3f, 0.0f),
            Pulse
        );
    }

    // Health gradient: Green -> Yellow -> Red
    if (HealthPercent > 0.6f)
    {
        return FLinearColor::Green;
    }
    else if (HealthPercent > 0.3f)
    {
        float T = (HealthPercent - 0.3f) / 0.3f;
        return FLinearColor::LerpUsingHSV(FLinearColor::Yellow, FLinearColor::Green, T);
    }
    else
    {
        float T = HealthPercent / 0.3f;
        return FLinearColor::LerpUsingHSV(FLinearColor::Red, FLinearColor::Yellow, T);
    }
}
```

### 13.3 Status Effect Indicators

```cpp
UENUM(BlueprintType)
enum class ELimbStatusIcon : uint8
{
    None,
    Fractured,      // Bone icon
    Bleeding_Light, // Single blood drop
    Bleeding_Heavy, // Multiple blood drops
    Destroyed,      // X icon
    Splinted,       // Bandage icon
};

// Widget displays icons overlaid on limb diagram
void ULimbStatusWidget::UpdateLimbIcons(ELimbType Limb)
{
    TArray<ELimbStatusIcon> Icons;

    if (IsLimbDestroyed(Limb))
    {
        Icons.Add(ELimbStatusIcon::Destroyed);
    }
    else
    {
        if (HasFracture(Limb))
        {
            Icons.Add(HasSplint(Limb) ?
                ELimbStatusIcon::Splinted :
                ELimbStatusIcon::Fractured);
        }

        if (HasHeavyBleeding(Limb))
        {
            Icons.Add(ELimbStatusIcon::Bleeding_Heavy);
        }
        else if (HasLightBleeding(Limb))
        {
            Icons.Add(ELimbStatusIcon::Bleeding_Light);
        }
    }

    SetLimbIcons(Limb, Icons);
}
```

### 13.4 Damage Feedback

```cpp
// Screen effects for limb damage
void ULimbDamageHUDComponent::OnLimbDamaged(const FSuspenseCoreLimbDamageEvent& Event)
{
    // Only for local player
    if (!IsLocalPlayer(Event.VictimActor.Get()))
        return;

    // Direction indicator (where damage came from)
    ShowDamageDirection(Event.InstigatorActor.Get());

    // Limb-specific screen effect
    switch (Event.DamagedLimb)
    {
        case ELimbType::Head:
            // Screen shake + tinnitus
            PlayCameraShake(HeadDamageShake, Event.DamageAmount / 35.0f);
            PlayTinnitusEffect(Event.DamageAmount / 35.0f);
            break;

        case ELimbType::LeftLeg:
        case ELimbType::RightLeg:
            // Screen tilt + stumble
            if (Event.DamageAmount > 20)
            {
                TriggerStumbleEffect();
            }
            break;

        case ELimbType::LeftArm:
        case ELimbType::RightArm:
            // Weapon sway spike
            TriggerWeaponSwaySpike(Event.DamageAmount);
            break;
    }

    // Blood vignette
    if (Event.HealthPercent < 0.3f)
    {
        ShowBloodVignette(1.0f - Event.HealthPercent);
    }
}
```

---

## 14. Performance Considerations

### 14.1 Optimization Strategies

**Attribute Updates:**
- Batch multiple limb updates in single frame
- Use dirty flags to minimize recalculation
- Cache penalty values, recalculate only on change

```cpp
void USuspenseCoreLimbComponent::TickComponent(float DeltaTime, ...)
{
    // Only recalculate penalties if limb state changed
    if (bPenaltiesDirty)
    {
        RecalculatePenalties();
        bPenaltiesDirty = false;
    }

    // Bleeding tick (already rate-limited to 1/sec)
    ProcessBleedings(DeltaTime);
}
```

### 14.2 Memory Layout

```cpp
// Cache-friendly limb data structure
USTRUCT()
struct FLimbHealthData
{
    // Hot data (accessed every frame)
    float CurrentHealth[7];     // 28 bytes
    float MaxHealth[7];         // 28 bytes
    uint8 StatusFlags;          // 1 byte (bitfield)
    uint8 Padding[3];           // 3 bytes alignment

    // Cold data (accessed on damage/heal)
    FLimbFractureState Fractures[4]; // Arms + Legs only
    TArray<FBleedingEffect> Bleedings;

    // Total hot path: 60 bytes (fits in cache line)
};
```

### 14.3 Event Throttling

```cpp
// Throttle UI updates for rapid damage
class FLimbDamageThrottler
{
    float LastBroadcastTime[7] = {0};
    float AccumulatedDamage[7] = {0};
    const float ThrottleInterval = 0.1f; // 100ms

public:
    void QueueDamageEvent(ELimbType Limb, float Damage)
    {
        int32 Index = static_cast<int32>(Limb);
        AccumulatedDamage[Index] += Damage;

        float CurrentTime = GetWorld()->GetTimeSeconds();
        if (CurrentTime - LastBroadcastTime[Index] >= ThrottleInterval)
        {
            // Broadcast accumulated damage
            BroadcastLimbDamage(Limb, AccumulatedDamage[Index]);
            AccumulatedDamage[Index] = 0;
            LastBroadcastTime[Index] = CurrentTime;
        }
    }
};
```

### 14.4 Server Performance Budget

| Operation | Budget | Frequency |
|-----------|--------|-----------|
| Damage calculation | 0.1ms | Per hit |
| Penalty recalculation | 0.05ms | On state change |
| Bleeding tick (all players) | 0.5ms | Per second |
| Attribute replication | 0.02ms/player | On change |

**Target:** <2ms total for 64 players under heavy combat

---

## Appendix A: Data Tables

### A.1 Limb Configuration DataTable

```cpp
USTRUCT(BlueprintType)
struct FLimbConfigRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    ELimbType LimbType;

    UPROPERTY(EditAnywhere)
    float BaseHealth = 60.0f;

    UPROPERTY(EditAnywhere)
    bool bIsCritical = false;

    UPROPERTY(EditAnywhere)
    ELimbType OverflowTarget;

    UPROPERTY(EditAnywhere)
    float OverflowMultiplier = 0.7f;

    UPROPERTY(EditAnywhere)
    TArray<FName> AssociatedBones;

    UPROPERTY(EditAnywhere)
    float FractureChanceMultiplier = 1.0f;

    UPROPERTY(EditAnywhere)
    float BleedChanceMultiplier = 1.0f;
};
```

### A.2 Penalty Configuration DataTable

```cpp
USTRUCT(BlueprintType)
struct FLimbPenaltyRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    ELimbType LimbType;

    UPROPERTY(EditAnywhere)
    EPenaltyCondition Condition; // Damaged, Fractured, Destroyed

    UPROPERTY(EditAnywhere)
    float HealthThreshold = 0.5f; // For Damaged condition

    UPROPERTY(EditAnywhere)
    FLimbPenalties Penalties;
};
```

---

## Appendix B: Integration Checklist

### B.1 Required Components

- [ ] `USuspenseCoreLimbComponent` - Main limb tracking component
- [ ] `USuspenseCoreLimbAttributeSet` - GAS attributes
- [ ] `ULimbDamageExecution` - GAS damage execution
- [ ] `UGE_LimbDamage` - Base damage effect
- [ ] `UGE_Fracture_XXX` - Per-limb fracture effects
- [ ] `UGE_Bleeding_XXX` - Bleeding status effects
- [ ] Bone-to-limb mapping DataTable
- [ ] Penalty configuration DataTable

### B.2 EventBus Events to Implement

- [ ] `FSuspenseCoreLimbDamageEvent`
- [ ] `FSuspenseCoreLimbHealedEvent`
- [ ] `FSuspenseCoreBleedingStartEvent`
- [ ] `FSuspenseCoreBleedingStopEvent`
- [ ] `FSuspenseCoreFractureEvent`
- [ ] `FSuspenseCoreFractureCuredEvent`
- [ ] `FSuspenseCoreLimbDestroyedEvent`
- [ ] `FSuspenseCoreLimbRestoredEvent`
- [ ] `FSuspenseCoreFallDamageEvent`
- [ ] `FSuspenseCorePainLevelChangedEvent`

### B.3 UI Widgets

- [ ] Limb health diagram (body outline)
- [ ] Status effect icons
- [ ] Damage direction indicator
- [ ] Blood vignette overlay
- [ ] Pain/tremor screen effects

---

## Appendix C: Testing Scenarios

### C.1 Unit Tests

```cpp
// Test: Damage overflow from destroyed limb
TEST(LimbDamage, OverflowOnDestruction)
{
    // Setup: Left arm at 10 HP
    SetLimbHealth(ELimbType::LeftArm, 10.0f);

    // Action: Apply 30 damage
    ApplyDamage(ELimbType::LeftArm, 30.0f);

    // Assert: Arm destroyed, overflow to thorax
    EXPECT_EQ(GetLimbHealth(ELimbType::LeftArm), 0.0f);
    EXPECT_TRUE(IsLimbDestroyed(ELimbType::LeftArm));

    // Overflow: (30-10) * 0.7 = 14 damage to thorax
    EXPECT_NEAR(GetLimbHealth(ELimbType::Thorax), 85.0f - 14.0f, 0.1f);
}

// Test: Fall damage fracture
TEST(FallDamage, FractureChance)
{
    // Setup: 8m fall height
    float FallHeight = 800.0f;

    // Action: Process fall damage 1000 times
    int FractureCount = 0;
    for (int i = 0; i < 1000; i++)
    {
        ResetLimbState();
        ProcessFallDamage(FallHeight);
        if (HasFracture(ELimbType::LeftLeg))
            FractureCount++;
    }

    // Assert: ~70% fracture rate (±5%)
    float FractureRate = FractureCount / 1000.0f;
    EXPECT_NEAR(FractureRate, 0.7f, 0.05f);
}
```

### C.2 Integration Test Scenarios

1. **Full combat scenario:** Player takes multiple hits to various limbs, accumulates status effects, uses medical items to recover
2. **Fall damage ladder:** Test fall damage at 1m increments from 2m to 15m
3. **Bleeding to death:** Verify heavy bleed drains HP correctly over time
4. **Penalty stacking:** Verify multiple limb injuries compound correctly
5. **Network sync:** Verify client limb state matches server after 100 damage events

---

## Appendix D: Complete Implementation Guide

This appendix provides step-by-step implementation details for integrating the Limb Damage System with GAS, EventBus, and dependent game systems.

---

### D.1 System Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           HIT REGISTRATION LAYER                            │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐                      │
│  │ Projectile  │    │  Hitscan    │    │   Melee     │                      │
│  │ Component   │    │   Trace     │    │   Trace     │                      │
│  └──────┬──────┘    └──────┬──────┘    └──────┬──────┘                      │
│         │                  │                  │                             │
│         └──────────────────┼──────────────────┘                             │
│                            ▼                                                │
│                   ┌────────────────┐                                        │
│                   │  FHitResult    │                                        │
│                   │  (BoneName)    │                                        │
│                   └────────┬───────┘                                        │
└────────────────────────────┼────────────────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                         DAMAGE PROCESSING LAYER                             │
│                   ┌────────────────────┐                                    │
│                   │ USuspenseCoreDamage│                                    │
│                   │ ProcessorSubsystem │                                    │
│                   └─────────┬──────────┘                                    │
│                             │                                               │
│         ┌───────────────────┼───────────────────┐                           │
│         ▼                   ▼                   ▼                           │
│  ┌──────────────┐   ┌──────────────┐   ┌──────────────┐                     │
│  │ BoneToLimb   │   │   Armor      │   │  Damage      │                     │
│  │ Resolver     │   │   Check      │   │  Calculation │                     │
│  └──────┬───────┘   └──────┬───────┘   └──────┬───────┘                     │
│         │                  │                  │                             │
│         └──────────────────┼──────────────────┘                             │
│                            ▼                                                │
│                   ┌────────────────┐                                        │
│                   │FLimbDamageInfo │                                        │
│                   └────────┬───────┘                                        │
└────────────────────────────┼────────────────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                              GAS LAYER                                      │
│                   ┌────────────────────┐                                    │
│                   │  Apply Gameplay    │                                    │
│                   │  Effect to Target  │                                    │
│                   └─────────┬──────────┘                                    │
│                             │                                               │
│         ┌───────────────────┼───────────────────┐                           │
│         ▼                   ▼                   ▼                           │
│  ┌──────────────┐   ┌──────────────┐   ┌──────────────┐                     │
│  │LimbAttribute │   │  Status      │   │  Penalty     │                     │
│  │Set Modified  │   │  Effect GE   │   │  Effect GE   │                     │
│  └──────┬───────┘   └──────┬───────┘   └──────┬───────┘                     │
│         │                  │                  │                             │
│         │                  ▼                  ▼                             │
│         │         ┌────────────────┐  ┌────────────────┐                    │
│         │         │ GameplayTags   │  │ Attribute     │                     │
│         │         │ Granted        │  │ Modifiers     │                     │
│         │         └────────┬───────┘  └────────┬───────┘                    │
│         │                  │                   │                            │
└─────────┼──────────────────┼───────────────────┼────────────────────────────┘
          │                  │                   │
          ▼                  ▼                   ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                            EVENTBUS LAYER                                   │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                    FSuspenseCoreEventBus                             │   │
│  │  Publish: LimbDamageEvent, FractureEvent, BleedingEvent, etc.        │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                             │                                               │
│         ┌───────────────────┼───────────────────┐                           │
│         ▼                   ▼                   ▼                           │
│  ┌──────────────┐   ┌──────────────┐   ┌──────────────┐                     │
│  │  UI Widget   │   │   Audio      │   │   VFX        │                     │
│  │  Subscriber  │   │   Manager    │   │   Manager    │                     │
│  └──────────────┘   └──────────────┘   └──────────────┘                     │
└─────────────────────────────────────────────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                         CONSUMER SYSTEMS                                    │
│  ┌──────────────┐   ┌──────────────┐   ┌──────────────┐   ┌──────────────┐  │
│  │ Movement     │   │  Weapon      │   │  Animation   │   │   Camera     │  │
│  │ Component    │   │  Component   │   │  Blueprint   │   │   Manager    │  │
│  │ (Speed/Jump) │   │ (Reload/Aim) │   │  (Limp/Pain) │   │   (Shake)    │  │
│  └──────────────┘   └──────────────┘   └──────────────┘   └──────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### D.2 Hit Registration Implementation

#### D.2.1 Projectile Hit Detection

```cpp
// SuspenseCoreProjectile.cpp

void ASuspenseCoreProjectile::OnProjectileHit(
    UPrimitiveComponent* HitComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    FVector NormalImpulse,
    const FHitResult& Hit)
{
    // Validate hit target
    if (!OtherActor || OtherActor == GetInstigator())
    {
        return;
    }

    // Check if target is damageable character
    ASuspenseCoreCharacter* TargetCharacter = Cast<ASuspenseCoreCharacter>(OtherActor);
    if (!TargetCharacter)
    {
        // Hit world geometry - spawn decal/effect
        HandleEnvironmentHit(Hit);
        return;
    }

    // === CRITICAL: Extract bone name from hit ===
    FName HitBoneName = Hit.BoneName;

    // Build damage info struct
    FSuspenseCoreDamageInfo DamageInfo;
    DamageInfo.Instigator = GetInstigator();
    DamageInfo.DamageCauser = this;
    DamageInfo.BaseDamage = ProjectileData.BaseDamage;
    DamageInfo.Penetration = ProjectileData.Penetration;
    DamageInfo.DamageType = EDamageCategory::Ballistic;
    DamageInfo.HitLocation = Hit.ImpactPoint;
    DamageInfo.HitNormal = Hit.ImpactNormal;
    DamageInfo.HitBoneName = HitBoneName;  // <-- Key for limb detection
    DamageInfo.HitResult = Hit;

    // Send to damage processor subsystem (server-side)
    if (HasAuthority())
    {
        USuspenseCoreDamageProcessorSubsystem* DamageProcessor =
            GetWorld()->GetSubsystem<USuspenseCoreDamageProcessorSubsystem>();

        if (DamageProcessor)
        {
            DamageProcessor->ProcessDamage(TargetCharacter, DamageInfo);
        }
    }

    // Destroy projectile
    Destroy();
}
```

#### D.2.2 Hitscan Weapon Hit Detection

```cpp
// SuspenseCoreWeaponComponent.cpp

void USuspenseCoreWeaponComponent::FireHitscan()
{
    // Get muzzle transform
    FVector MuzzleLocation = GetMuzzleLocation();
    FVector AimDirection = GetAimDirection();

    // Trace parameters
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(GetOwner());
    QueryParams.bReturnPhysicalMaterial = true;
    QueryParams.bTraceComplex = true;  // Required for accurate bone detection

    FHitResult Hit;
    FVector TraceEnd = MuzzleLocation + (AimDirection * WeaponData.MaxRange);

    bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit,
        MuzzleLocation,
        TraceEnd,
        ECC_WeaponTrace,  // Custom collision channel
        QueryParams
    );

    if (bHit && Hit.GetActor())
    {
        ASuspenseCoreCharacter* TargetCharacter = Cast<ASuspenseCoreCharacter>(Hit.GetActor());

        if (TargetCharacter)
        {
            // === Build damage info with bone name ===
            FSuspenseCoreDamageInfo DamageInfo;
            DamageInfo.Instigator = GetOwner();
            DamageInfo.DamageCauser = GetOwner();
            DamageInfo.BaseDamage = WeaponData.BaseDamage;
            DamageInfo.Penetration = WeaponData.Penetration;
            DamageInfo.DamageType = EDamageCategory::Ballistic;
            DamageInfo.HitLocation = Hit.ImpactPoint;
            DamageInfo.HitNormal = Hit.ImpactNormal;
            DamageInfo.HitBoneName = Hit.BoneName;  // <-- From skeletal mesh
            DamageInfo.HitResult = Hit;

            // Server processes damage
            if (GetOwner()->HasAuthority())
            {
                ProcessHitscanDamage(TargetCharacter, DamageInfo);
            }
            else
            {
                // Client sends to server
                Server_ProcessHitscanDamage(TargetCharacter, DamageInfo);
            }
        }
    }
}
```

#### D.2.3 Bone-to-Limb Resolution Service

```cpp
// SuspenseCoreBoneLimbResolver.h

UCLASS()
class USuspenseCoreBoneLimbResolver : public UObject
{
    GENERATED_BODY()

public:
    // Singleton access
    static USuspenseCoreBoneLimbResolver* Get(UWorld* World);

    // Main resolution function
    ELimbType ResolveBoneToLimb(FName BoneName) const;

    // Get damage multiplier for specific bone
    float GetBoneDamageMultiplier(FName BoneName) const;

private:
    // Loaded from DataTable
    UPROPERTY()
    TMap<FName, FBoneLimbMappingData> BoneMappings;

    void LoadMappingsFromDataTable();
};

// SuspenseCoreBoneLimbResolver.cpp

ELimbType USuspenseCoreBoneLimbResolver::ResolveBoneToLimb(FName BoneName) const
{
    // Direct lookup first
    if (const FBoneLimbMappingData* Mapping = BoneMappings.Find(BoneName))
    {
        return Mapping->LimbType;
    }

    // Fallback: Pattern matching for common bone naming conventions
    FString BoneString = BoneName.ToString().ToLower();

    // Head detection
    if (BoneString.Contains(TEXT("head")) || BoneString.Contains(TEXT("neck")))
    {
        return ELimbType::Head;
    }

    // Arm detection (check side first)
    if (BoneString.Contains(TEXT("arm")) || BoneString.Contains(TEXT("hand")) ||
        BoneString.Contains(TEXT("shoulder")) || BoneString.Contains(TEXT("clavicle")))
    {
        if (BoneString.Contains(TEXT("_l")) || BoneString.Contains(TEXT("left")))
        {
            return ELimbType::LeftArm;
        }
        return ELimbType::RightArm;  // Default to right
    }

    // Leg detection
    if (BoneString.Contains(TEXT("leg")) || BoneString.Contains(TEXT("thigh")) ||
        BoneString.Contains(TEXT("calf")) || BoneString.Contains(TEXT("foot")))
    {
        if (BoneString.Contains(TEXT("_l")) || BoneString.Contains(TEXT("left")))
        {
            return ELimbType::LeftLeg;
        }
        return ELimbType::RightLeg;
    }

    // Torso detection
    if (BoneString.Contains(TEXT("spine_03")) || BoneString.Contains(TEXT("chest")))
    {
        return ELimbType::Thorax;
    }

    if (BoneString.Contains(TEXT("spine")) || BoneString.Contains(TEXT("pelvis")))
    {
        return ELimbType::Stomach;
    }

    // Ultimate fallback - thorax (center mass)
    UE_LOG(LogSuspenseCore, Warning,
        TEXT("Unknown bone '%s' - defaulting to Thorax"), *BoneName.ToString());
    return ELimbType::Thorax;
}
```

---

### D.3 Damage Processor Subsystem

#### D.3.1 Main Damage Processing Flow

```cpp
// SuspenseCoreDamageProcessorSubsystem.h

UCLASS()
class USuspenseCoreDamageProcessorSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // Main entry point for all damage
    void ProcessDamage(ASuspenseCoreCharacter* Target, const FSuspenseCoreDamageInfo& DamageInfo);

private:
    // Step 1: Resolve bone to limb
    ELimbType ResolveLimbFromHit(const FSuspenseCoreDamageInfo& DamageInfo);

    // Step 2: Check armor
    float ProcessArmorInteraction(ASuspenseCoreCharacter* Target, ELimbType Limb,
                                   const FSuspenseCoreDamageInfo& DamageInfo);

    // Step 3: Apply damage via GAS
    void ApplyLimbDamageViaGAS(ASuspenseCoreCharacter* Target, ELimbType Limb,
                                float FinalDamage, const FSuspenseCoreDamageInfo& DamageInfo);

    // Step 4: Check and apply status effects
    void ProcessStatusEffects(ASuspenseCoreCharacter* Target, ELimbType Limb,
                               float Damage, const FSuspenseCoreDamageInfo& DamageInfo);
};

// SuspenseCoreDamageProcessorSubsystem.cpp

void USuspenseCoreDamageProcessorSubsystem::ProcessDamage(
    ASuspenseCoreCharacter* Target,
    const FSuspenseCoreDamageInfo& DamageInfo)
{
    if (!Target || !Target->HasAuthority())
    {
        return;  // Server only
    }

    // === Step 1: Resolve limb from bone hit ===
    ELimbType TargetLimb = ResolveLimbFromHit(DamageInfo);

    UE_LOG(LogSuspenseCore, Verbose,
        TEXT("Processing damage: Bone=%s -> Limb=%s, BaseDamage=%.1f"),
        *DamageInfo.HitBoneName.ToString(),
        *UEnum::GetValueAsString(TargetLimb),
        DamageInfo.BaseDamage);

    // === Step 2: Armor interaction ===
    float DamageAfterArmor = ProcessArmorInteraction(Target, TargetLimb, DamageInfo);

    if (DamageAfterArmor <= 0.0f)
    {
        // Fully blocked by armor
        BroadcastArmorBlockEvent(Target, TargetLimb, DamageInfo);
        return;
    }

    // === Step 3: Apply damage to limb via GAS ===
    ApplyLimbDamageViaGAS(Target, TargetLimb, DamageAfterArmor, DamageInfo);

    // === Step 4: Process status effects (bleeding, fracture chances) ===
    ProcessStatusEffects(Target, TargetLimb, DamageAfterArmor, DamageInfo);
}

ELimbType USuspenseCoreDamageProcessorSubsystem::ResolveLimbFromHit(
    const FSuspenseCoreDamageInfo& DamageInfo)
{
    USuspenseCoreBoneLimbResolver* Resolver = USuspenseCoreBoneLimbResolver::Get(GetWorld());

    if (Resolver && DamageInfo.HitBoneName != NAME_None)
    {
        return Resolver->ResolveBoneToLimb(DamageInfo.HitBoneName);
    }

    // Fallback: Use hit location height
    // ... (see Section 4.4 in main document)

    return ELimbType::Thorax;
}
```

---

### D.4 GAS Damage Application

#### D.4.1 Applying Damage via Gameplay Effect

```cpp
void USuspenseCoreDamageProcessorSubsystem::ApplyLimbDamageViaGAS(
    ASuspenseCoreCharacter* Target,
    ELimbType Limb,
    float FinalDamage,
    const FSuspenseCoreDamageInfo& DamageInfo)
{
    UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
    if (!TargetASC)
    {
        UE_LOG(LogSuspenseCore, Error, TEXT("Target has no ASC!"));
        return;
    }

    // Get the damage effect class (configured in project settings or data asset)
    TSubclassOf<UGameplayEffect> DamageEffectClass = GetLimbDamageEffectClass();
    if (!DamageEffectClass)
    {
        return;
    }

    // Create effect context with custom data
    FGameplayEffectContextHandle ContextHandle = TargetASC->MakeEffectContext();
    ContextHandle.AddSourceObject(DamageInfo.DamageCauser);
    ContextHandle.AddInstigator(DamageInfo.Instigator, DamageInfo.DamageCauser);
    ContextHandle.AddHitResult(DamageInfo.HitResult);

    // === Store limb info in context ===
    // Option A: Custom context class
    FSuspenseCoreDamageEffectContext* CustomContext =
        static_cast<FSuspenseCoreDamageEffectContext*>(ContextHandle.Get());
    if (CustomContext)
    {
        CustomContext->TargetLimb = Limb;
        CustomContext->DamageCategory = DamageInfo.DamageType;
    }

    // Create effect spec
    FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(
        DamageEffectClass,
        1.0f,  // Level
        ContextHandle
    );

    if (SpecHandle.IsValid())
    {
        // === Set damage amount via SetByCaller ===
        FGameplayTag DamageAmountTag = FGameplayTag::RequestGameplayTag(
            FName("SetByCaller.Damage.Amount"));
        SpecHandle.Data->SetSetByCallerMagnitude(DamageAmountTag, FinalDamage);

        // === Set target limb via SetByCaller (for execution calculation) ===
        FGameplayTag LimbTag = FGameplayTag::RequestGameplayTag(
            FName("SetByCaller.Damage.Limb"));
        SpecHandle.Data->SetSetByCallerMagnitude(LimbTag, static_cast<float>(Limb));

        // Apply effect
        FActiveGameplayEffectHandle ActiveHandle = TargetASC->ApplyGameplayEffectSpecToSelf(
            *SpecHandle.Data.Get());

        if (!ActiveHandle.IsValid())
        {
            UE_LOG(LogSuspenseCore, Warning, TEXT("Failed to apply limb damage effect"));
        }
    }
}
```

#### D.4.2 Limb Damage Execution Calculation

```cpp
// SuspenseCoreLimbDamageExecution.h

UCLASS()
class ULimbDamageExecutionCalculation : public UGameplayEffectExecutionCalculation
{
    GENERATED_BODY()

public:
    ULimbDamageExecutionCalculation();

    virtual void Execute_Implementation(
        const FGameplayEffectCustomExecutionParameters& ExecutionParams,
        FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

private:
    // Captured attributes for reading
    FGameplayEffectAttributeCaptureDefinition HeadHealthDef;
    FGameplayEffectAttributeCaptureDefinition ThoraxHealthDef;
    FGameplayEffectAttributeCaptureDefinition StomachHealthDef;
    FGameplayEffectAttributeCaptureDefinition LeftArmHealthDef;
    FGameplayEffectAttributeCaptureDefinition RightArmHealthDef;
    FGameplayEffectAttributeCaptureDefinition LeftLegHealthDef;
    FGameplayEffectAttributeCaptureDefinition RightLegHealthDef;
};

// SuspenseCoreLimbDamageExecution.cpp

ULimbDamageExecutionCalculation::ULimbDamageExecutionCalculation()
{
    // Capture all limb health attributes
    HeadHealthDef.AttributeToCapture = USuspenseCoreLimbAttributeSet::GetHeadHealthAttribute();
    HeadHealthDef.AttributeSource = EGameplayEffectAttributeCaptureSource::Target;
    HeadHealthDef.bSnapshot = false;

    ThoraxHealthDef.AttributeToCapture = USuspenseCoreLimbAttributeSet::GetThoraxHealthAttribute();
    ThoraxHealthDef.AttributeSource = EGameplayEffectAttributeCaptureSource::Target;
    ThoraxHealthDef.bSnapshot = false;

    // ... (similar for all limbs)

    RelevantAttributesToCapture.Add(HeadHealthDef);
    RelevantAttributesToCapture.Add(ThoraxHealthDef);
    RelevantAttributesToCapture.Add(StomachHealthDef);
    RelevantAttributesToCapture.Add(LeftArmHealthDef);
    RelevantAttributesToCapture.Add(RightArmHealthDef);
    RelevantAttributesToCapture.Add(LeftLegHealthDef);
    RelevantAttributesToCapture.Add(RightLegHealthDef);
}

void ULimbDamageExecutionCalculation::Execute_Implementation(
    const FGameplayEffectCustomExecutionParameters& ExecutionParams,
    FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
    UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
    if (!TargetASC)
    {
        return;
    }

    const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

    // === Get damage amount from SetByCaller ===
    float DamageAmount = Spec.GetSetByCallerMagnitude(
        FGameplayTag::RequestGameplayTag(FName("SetByCaller.Damage.Amount")),
        false, 0.0f);

    // === Get target limb from SetByCaller ===
    float LimbFloat = Spec.GetSetByCallerMagnitude(
        FGameplayTag::RequestGameplayTag(FName("SetByCaller.Damage.Limb")),
        false, static_cast<float>(ELimbType::Thorax));
    ELimbType TargetLimb = static_cast<ELimbType>(FMath::RoundToInt(LimbFloat));

    if (DamageAmount <= 0.0f)
    {
        return;
    }

    // === Get current limb health ===
    float CurrentHealth = 0.0f;
    float MaxHealth = 0.0f;
    FGameplayAttribute TargetAttribute;

    switch (TargetLimb)
    {
        case ELimbType::Head:
            ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
                HeadHealthDef, FAggregatorEvaluateParameters(), CurrentHealth);
            TargetAttribute = USuspenseCoreLimbAttributeSet::GetHeadHealthAttribute();
            MaxHealth = 35.0f;
            break;

        case ELimbType::Thorax:
            ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
                ThoraxHealthDef, FAggregatorEvaluateParameters(), CurrentHealth);
            TargetAttribute = USuspenseCoreLimbAttributeSet::GetThoraxHealthAttribute();
            MaxHealth = 85.0f;
            break;

        case ELimbType::Stomach:
            ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
                StomachHealthDef, FAggregatorEvaluateParameters(), CurrentHealth);
            TargetAttribute = USuspenseCoreLimbAttributeSet::GetStomachHealthAttribute();
            MaxHealth = 70.0f;
            break;

        case ELimbType::LeftArm:
            ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
                LeftArmHealthDef, FAggregatorEvaluateParameters(), CurrentHealth);
            TargetAttribute = USuspenseCoreLimbAttributeSet::GetLeftArmHealthAttribute();
            MaxHealth = 60.0f;
            break;

        case ELimbType::RightArm:
            ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
                RightArmHealthDef, FAggregatorEvaluateParameters(), CurrentHealth);
            TargetAttribute = USuspenseCoreLimbAttributeSet::GetRightArmHealthAttribute();
            MaxHealth = 60.0f;
            break;

        case ELimbType::LeftLeg:
            ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
                LeftLegHealthDef, FAggregatorEvaluateParameters(), CurrentHealth);
            TargetAttribute = USuspenseCoreLimbAttributeSet::GetLeftLegHealthAttribute();
            MaxHealth = 65.0f;
            break;

        case ELimbType::RightLeg:
            ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
                RightLegHealthDef, FAggregatorEvaluateParameters(), CurrentHealth);
            TargetAttribute = USuspenseCoreLimbAttributeSet::GetRightLegHealthAttribute();
            MaxHealth = 65.0f;
            break;
    }

    // === Calculate damage with overflow ===
    float NewHealth = CurrentHealth - DamageAmount;
    float OverflowDamage = 0.0f;
    bool bLimbDestroyed = false;

    if (NewHealth < 0.0f)
    {
        // Limb goes to zero
        OverflowDamage = FMath::Abs(NewHealth) * 0.7f;  // 70% overflow
        NewHealth = 0.0f;
        bLimbDestroyed = !IsCriticalLimb(TargetLimb);
    }

    // === Output modifier for target limb ===
    OutExecutionOutput.AddOutputModifier(
        FGameplayModifierEvaluatedData(
            TargetAttribute,
            EGameplayModOp::Override,  // Set directly to new value
            NewHealth
        )
    );

    // === Handle overflow to secondary target ===
    if (OverflowDamage > 0.0f && !IsCriticalLimb(TargetLimb))
    {
        ELimbType OverflowTarget = GetOverflowTarget(TargetLimb);
        FGameplayAttribute OverflowAttribute = GetAttributeForLimb(OverflowTarget);

        float OverflowTargetHealth = GetCurrentHealth(ExecutionParams, OverflowTarget);
        float NewOverflowHealth = FMath::Max(0.0f, OverflowTargetHealth - OverflowDamage);

        OutExecutionOutput.AddOutputModifier(
            FGameplayModifierEvaluatedData(
                OverflowAttribute,
                EGameplayModOp::Override,
                NewOverflowHealth
            )
        );
    }

    // Store results for post-execution processing (status effects, events)
    // This is handled in PostGameplayEffectExecute on the AttributeSet
}
```

#### D.4.3 AttributeSet Post-Effect Processing

```cpp
// SuspenseCoreLimbAttributeSet.cpp

void USuspenseCoreLimbAttributeSet::PostGameplayEffectExecute(
    const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    // Determine which limb was affected
    ELimbType AffectedLimb = ELimbType::None;
    float NewHealth = 0.0f;
    float MaxHealth = 0.0f;

    if (Data.EvaluatedData.Attribute == GetHeadHealthAttribute())
    {
        AffectedLimb = ELimbType::Head;
        NewHealth = GetHeadHealth();
        MaxHealth = GetMaxHeadHealth();
    }
    else if (Data.EvaluatedData.Attribute == GetThoraxHealthAttribute())
    {
        AffectedLimb = ELimbType::Thorax;
        NewHealth = GetThoraxHealth();
        MaxHealth = GetMaxThoraxHealth();
    }
    // ... (similar for all limbs)

    if (AffectedLimb == ELimbType::None)
    {
        return;
    }

    // === Check for death (critical limbs) ===
    if (IsCriticalLimb(AffectedLimb) && NewHealth <= 0.0f)
    {
        HandleCharacterDeath(Data);
        return;
    }

    // === Check for limb destruction (non-critical) ===
    if (!IsCriticalLimb(AffectedLimb) && NewHealth <= 0.0f)
    {
        HandleLimbDestruction(AffectedLimb, Data);
    }

    // === Update penalty GameplayEffects based on new health ===
    UpdateLimbPenalties(AffectedLimb, NewHealth, MaxHealth);

    // === Broadcast EventBus event ===
    BroadcastLimbDamageEvent(AffectedLimb, Data);
}

void USuspenseCoreLimbAttributeSet::UpdateLimbPenalties(
    ELimbType Limb,
    float CurrentHealth,
    float MaxHealth)
{
    UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
    if (!ASC)
    {
        return;
    }

    float HealthPercent = CurrentHealth / MaxHealth;

    // === Remove existing penalty effect for this limb ===
    FGameplayTagContainer LimbPenaltyTags;
    LimbPenaltyTags.AddTag(GetPenaltyTagForLimb(Limb));
    ASC->RemoveActiveEffectsWithGrantedTags(LimbPenaltyTags);

    // === Apply new penalty effect based on health threshold ===
    TSubclassOf<UGameplayEffect> PenaltyEffect = nullptr;

    if (IsLimbDestroyed(Limb))
    {
        PenaltyEffect = GetDestroyedPenaltyEffect(Limb);
    }
    else if (HasFracture(Limb))
    {
        PenaltyEffect = GetFracturePenaltyEffect(Limb);
    }
    else if (HealthPercent <= 0.25f)
    {
        PenaltyEffect = GetSevereDamagePenaltyEffect(Limb);
    }
    else if (HealthPercent <= 0.50f)
    {
        PenaltyEffect = GetModerateDamagePenaltyEffect(Limb);
    }

    if (PenaltyEffect)
    {
        FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
        ASC->ApplyGameplayEffectToSelf(PenaltyEffect.GetDefaultObject(), 1.0f, Context);
    }
}
```

---

### D.5 GameplayTags Native Declaration

```cpp
// SuspenseCoreGameplayTags.h

#pragma once

#include "NativeGameplayTags.h"

namespace SuspenseCoreTags
{
    // === Limb State Tags ===
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Limb_Head_Destroyed);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Limb_Thorax_Critical);

    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Limb_LeftArm_Destroyed);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Limb_LeftArm_Fractured);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Limb_LeftArm_Splinted);

    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Limb_RightArm_Destroyed);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Limb_RightArm_Fractured);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Limb_RightArm_Splinted);

    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Limb_LeftLeg_Destroyed);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Limb_LeftLeg_Fractured);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Limb_LeftLeg_Splinted);

    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Limb_RightLeg_Destroyed);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Limb_RightLeg_Fractured);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Limb_RightLeg_Splinted);

    // === Status Effect Tags ===
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Status_Bleeding_Light);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Status_Bleeding_Heavy);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Status_Pain_Light);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Status_Pain_Moderate);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Status_Pain_Severe);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Status_Tremor);

    // === Penalty Tags (for effect identification) ===
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Penalty_Limb_LeftArm);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Penalty_Limb_RightArm);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Penalty_Limb_LeftLeg);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Penalty_Limb_RightLeg);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Penalty_Limb_Stomach);

    // === SetByCaller Tags ===
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage_Amount);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage_Limb);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Heal_Amount);

    // === Ability Block Tags ===
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Block_Sprint);  // Applied by destroyed leg
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Block_Jump);    // Applied by both legs fractured
}

// SuspenseCoreGameplayTags.cpp

#include "SuspenseCoreGameplayTags.h"

namespace SuspenseCoreTags
{
    UE_DEFINE_GAMEPLAY_TAG(State_Limb_Head_Destroyed, "State.Limb.Head.Destroyed");
    UE_DEFINE_GAMEPLAY_TAG(State_Limb_Thorax_Critical, "State.Limb.Thorax.Critical");

    UE_DEFINE_GAMEPLAY_TAG(State_Limb_LeftArm_Destroyed, "State.Limb.LeftArm.Destroyed");
    UE_DEFINE_GAMEPLAY_TAG(State_Limb_LeftArm_Fractured, "State.Limb.LeftArm.Fractured");
    UE_DEFINE_GAMEPLAY_TAG(State_Limb_LeftArm_Splinted, "State.Limb.LeftArm.Splinted");

    UE_DEFINE_GAMEPLAY_TAG(State_Limb_RightArm_Destroyed, "State.Limb.RightArm.Destroyed");
    UE_DEFINE_GAMEPLAY_TAG(State_Limb_RightArm_Fractured, "State.Limb.RightArm.Fractured");
    UE_DEFINE_GAMEPLAY_TAG(State_Limb_RightArm_Splinted, "State.Limb.RightArm.Splinted");

    UE_DEFINE_GAMEPLAY_TAG(State_Limb_LeftLeg_Destroyed, "State.Limb.LeftLeg.Destroyed");
    UE_DEFINE_GAMEPLAY_TAG(State_Limb_LeftLeg_Fractured, "State.Limb.LeftLeg.Fractured");
    UE_DEFINE_GAMEPLAY_TAG(State_Limb_LeftLeg_Splinted, "State.Limb.LeftLeg.Splinted");

    UE_DEFINE_GAMEPLAY_TAG(State_Limb_RightLeg_Destroyed, "State.Limb.RightLeg.Destroyed");
    UE_DEFINE_GAMEPLAY_TAG(State_Limb_RightLeg_Fractured, "State.Limb.RightLeg.Fractured");
    UE_DEFINE_GAMEPLAY_TAG(State_Limb_RightLeg_Splinted, "State.Limb.RightLeg.Splinted");

    UE_DEFINE_GAMEPLAY_TAG(State_Status_Bleeding_Light, "State.Status.Bleeding.Light");
    UE_DEFINE_GAMEPLAY_TAG(State_Status_Bleeding_Heavy, "State.Status.Bleeding.Heavy");
    UE_DEFINE_GAMEPLAY_TAG(State_Status_Pain_Light, "State.Status.Pain.Light");
    UE_DEFINE_GAMEPLAY_TAG(State_Status_Pain_Moderate, "State.Status.Pain.Moderate");
    UE_DEFINE_GAMEPLAY_TAG(State_Status_Pain_Severe, "State.Status.Pain.Severe");
    UE_DEFINE_GAMEPLAY_TAG(State_Status_Tremor, "State.Status.Tremor");

    UE_DEFINE_GAMEPLAY_TAG(Penalty_Limb_LeftArm, "Penalty.Limb.LeftArm");
    UE_DEFINE_GAMEPLAY_TAG(Penalty_Limb_RightArm, "Penalty.Limb.RightArm");
    UE_DEFINE_GAMEPLAY_TAG(Penalty_Limb_LeftLeg, "Penalty.Limb.LeftLeg");
    UE_DEFINE_GAMEPLAY_TAG(Penalty_Limb_RightLeg, "Penalty.Limb.RightLeg");
    UE_DEFINE_GAMEPLAY_TAG(Penalty_Limb_Stomach, "Penalty.Limb.Stomach");

    UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Damage_Amount, "SetByCaller.Damage.Amount");
    UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Damage_Limb, "SetByCaller.Damage.Limb");
    UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Heal_Amount, "SetByCaller.Heal.Amount");

    UE_DEFINE_GAMEPLAY_TAG(Ability_Block_Sprint, "Ability.Block.Sprint");
    UE_DEFINE_GAMEPLAY_TAG(Ability_Block_Jump, "Ability.Block.Jump");
}
```

---

### D.6 Consumer System Integration

#### D.6.1 Movement Component Integration

```cpp
// SuspenseCoreCharacterMovementComponent.h

UCLASS()
class USuspenseCoreCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    virtual float GetMaxSpeed() const override;
    virtual float GetMaxJumpHeight() const;
    virtual bool CanAttemptJump() const override;

protected:
    // Cached penalty values (updated on attribute change)
    float CachedMovementSpeedMultiplier = 1.0f;
    float CachedSprintSpeedMultiplier = 1.0f;
    float CachedJumpHeightMultiplier = 1.0f;

    // Subscribe to attribute changes
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnMovementPenaltyChanged(const FOnAttributeChangeData& Data);
};

// SuspenseCoreCharacterMovementComponent.cpp

void USuspenseCoreCharacterMovementComponent::BeginPlay()
{
    Super::BeginPlay();

    // Subscribe to penalty attribute changes via ASC
    if (ASuspenseCoreCharacter* Owner = Cast<ASuspenseCoreCharacter>(GetOwner()))
    {
        if (UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent())
        {
            // Subscribe to movement penalty attribute
            ASC->GetGameplayAttributeValueChangeDelegate(
                USuspenseCoreLimbAttributeSet::GetMovementSpeedPenaltyAttribute()
            ).AddUObject(this, &USuspenseCoreCharacterMovementComponent::OnMovementPenaltyChanged);

            // Subscribe to sprint penalty attribute
            ASC->GetGameplayAttributeValueChangeDelegate(
                USuspenseCoreLimbAttributeSet::GetSprintSpeedPenaltyAttribute()
            ).AddUObject(this, &USuspenseCoreCharacterMovementComponent::OnSprintPenaltyChanged);

            // Subscribe to jump penalty attribute
            ASC->GetGameplayAttributeValueChangeDelegate(
                USuspenseCoreLimbAttributeSet::GetJumpHeightPenaltyAttribute()
            ).AddUObject(this, &USuspenseCoreCharacterMovementComponent::OnJumpPenaltyChanged);
        }
    }
}

void USuspenseCoreCharacterMovementComponent::OnMovementPenaltyChanged(
    const FOnAttributeChangeData& Data)
{
    CachedMovementSpeedMultiplier = Data.NewValue;

    // Log for debugging
    UE_LOG(LogSuspenseCore, Verbose,
        TEXT("Movement speed penalty changed: %.2f"), Data.NewValue);
}

float USuspenseCoreCharacterMovementComponent::GetMaxSpeed() const
{
    float BaseSpeed = Super::GetMaxSpeed();

    // Apply limb penalty
    float ModifiedSpeed = BaseSpeed * CachedMovementSpeedMultiplier;

    // Check if sprinting and apply sprint penalty
    if (IsSprinting())
    {
        // Sprint uses separate multiplier
        ModifiedSpeed = GetSprintSpeed() * CachedSprintSpeedMultiplier;

        // Check for sprint block tag
        if (ASuspenseCoreCharacter* Owner = Cast<ASuspenseCoreCharacter>(GetOwner()))
        {
            if (UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent())
            {
                if (ASC->HasMatchingGameplayTag(SuspenseCoreTags::Ability_Block_Sprint))
                {
                    // Cannot sprint - fallback to walk speed
                    ModifiedSpeed = BaseSpeed * CachedMovementSpeedMultiplier;
                }
            }
        }
    }

    return ModifiedSpeed;
}

bool USuspenseCoreCharacterMovementComponent::CanAttemptJump() const
{
    // Check base conditions
    if (!Super::CanAttemptJump())
    {
        return false;
    }

    // Check for jump block tag (both legs fractured)
    if (ASuspenseCoreCharacter* Owner = Cast<ASuspenseCoreCharacter>(GetOwner()))
    {
        if (UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent())
        {
            if (ASC->HasMatchingGameplayTag(SuspenseCoreTags::Ability_Block_Jump))
            {
                return false;
            }
        }
    }

    return true;
}

float USuspenseCoreCharacterMovementComponent::GetMaxJumpHeight() const
{
    float BaseJumpHeight = JumpZVelocity;

    // Apply limb penalty
    return BaseJumpHeight * CachedJumpHeightMultiplier;
}
```

#### D.6.2 Weapon Component Integration

```cpp
// SuspenseCoreWeaponComponent.cpp - Reload Speed

float USuspenseCoreWeaponComponent::GetReloadSpeedMultiplier() const
{
    if (ASuspenseCoreCharacter* Owner = Cast<ASuspenseCoreCharacter>(GetOwner()))
    {
        if (UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent())
        {
            // Get reload penalty from limb attribute set
            float ReloadPenalty = ASC->GetNumericAttribute(
                USuspenseCoreLimbAttributeSet::GetReloadSpeedPenaltyAttribute());

            return FMath::Max(0.1f, ReloadPenalty);  // Minimum 10% speed
        }
    }
    return 1.0f;
}

// In reload ability
void UGA_Reload::ActivateAbility(...)
{
    // ... setup code ...

    // Get speed multiplier considering arm damage
    float ReloadSpeedMult = WeaponComponent->GetReloadSpeedMultiplier();

    // Apply to montage
    UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this,
        NAME_None,
        ReloadMontage,
        ReloadSpeedMult  // Play rate affected by arm damage
    );

    MontageTask->ReadyForActivation();
}
```

#### D.6.3 Aim/Weapon Sway Integration

```cpp
// SuspenseCoreWeaponComponent.cpp - Weapon Sway

FRotator USuspenseCoreWeaponComponent::CalculateWeaponSway(float DeltaTime) const
{
    FRotator BaseSway = CalculateBaseWeaponSway(DeltaTime);

    // Get sway multiplier from limb system
    float SwayMultiplier = 1.0f;
    if (ASuspenseCoreCharacter* Owner = Cast<ASuspenseCoreCharacter>(GetOwner()))
    {
        if (UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent())
        {
            SwayMultiplier = ASC->GetNumericAttribute(
                USuspenseCoreLimbAttributeSet::GetWeaponSwayMultiplierAttribute());
        }
    }

    // Apply multiplier (higher = more sway)
    return BaseSway * SwayMultiplier;
}

// Aim down sights speed
float USuspenseCoreWeaponComponent::GetADSSpeed() const
{
    float BaseADSSpeed = WeaponData.ADSSpeed;

    if (ASuspenseCoreCharacter* Owner = Cast<ASuspenseCoreCharacter>(GetOwner()))
    {
        if (UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent())
        {
            float AimPenalty = ASC->GetNumericAttribute(
                USuspenseCoreLimbAttributeSet::GetAimSpeedPenaltyAttribute());

            return BaseADSSpeed * AimPenalty;
        }
    }

    return BaseADSSpeed;
}
```

#### D.6.4 Animation Blueprint Integration

```cpp
// SuspenseCoreAnimInstance.h

UCLASS()
class USuspenseCoreAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
    // Exposed to AnimBP
    UPROPERTY(BlueprintReadOnly, Category = "Limb State")
    bool bLeftLegFractured = false;

    UPROPERTY(BlueprintReadOnly, Category = "Limb State")
    bool bRightLegFractured = false;

    UPROPERTY(BlueprintReadOnly, Category = "Limb State")
    bool bLeftArmDestroyed = false;

    UPROPERTY(BlueprintReadOnly, Category = "Limb State")
    bool bRightArmDestroyed = false;

    UPROPERTY(BlueprintReadOnly, Category = "Limb State")
    float LimpIntensity = 0.0f;  // 0-1 for blend

    UPROPERTY(BlueprintReadOnly, Category = "Limb State")
    float PainLevel = 0.0f;  // 0-100

private:
    void UpdateLimbStateFromTags();
};

// SuspenseCoreAnimInstance.cpp

void USuspenseCoreAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    UpdateLimbStateFromTags();
}

void USuspenseCoreAnimInstance::UpdateLimbStateFromTags()
{
    APawn* Owner = TryGetPawnOwner();
    if (!Owner)
    {
        return;
    }

    ASuspenseCoreCharacter* Character = Cast<ASuspenseCoreCharacter>(Owner);
    if (!Character)
    {
        return;
    }

    UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
    if (!ASC)
    {
        return;
    }

    // === Query GameplayTags for limb states ===
    bLeftLegFractured = ASC->HasMatchingGameplayTag(SuspenseCoreTags::State_Limb_LeftLeg_Fractured);
    bRightLegFractured = ASC->HasMatchingGameplayTag(SuspenseCoreTags::State_Limb_RightLeg_Fractured);
    bLeftArmDestroyed = ASC->HasMatchingGameplayTag(SuspenseCoreTags::State_Limb_LeftArm_Destroyed);
    bRightArmDestroyed = ASC->HasMatchingGameplayTag(SuspenseCoreTags::State_Limb_RightArm_Destroyed);

    // === Calculate limp intensity ===
    // Based on leg health and fracture state
    float LeftLegHealth = ASC->GetNumericAttribute(
        USuspenseCoreLimbAttributeSet::GetLeftLegHealthAttribute());
    float RightLegHealth = ASC->GetNumericAttribute(
        USuspenseCoreLimbAttributeSet::GetRightLegHealthAttribute());

    float WorstLegPercent = FMath::Min(LeftLegHealth / 65.0f, RightLegHealth / 65.0f);

    LimpIntensity = 1.0f - WorstLegPercent;  // Higher = more limp

    // Fracture increases limp
    if (bLeftLegFractured || bRightLegFractured)
    {
        LimpIntensity = FMath::Min(1.0f, LimpIntensity + 0.3f);
    }

    // === Get pain level ===
    if (ASC->HasMatchingGameplayTag(SuspenseCoreTags::State_Status_Pain_Severe))
    {
        PainLevel = 90.0f;
    }
    else if (ASC->HasMatchingGameplayTag(SuspenseCoreTags::State_Status_Pain_Moderate))
    {
        PainLevel = 60.0f;
    }
    else if (ASC->HasMatchingGameplayTag(SuspenseCoreTags::State_Status_Pain_Light))
    {
        PainLevel = 30.0f;
    }
    else
    {
        PainLevel = 0.0f;
    }
}
```

**Animation Blueprint Usage:**
```
In AnimGraph:
- Use LimpIntensity to blend between normal walk and limp animation
- Use PainLevel to add procedural camera sway/breathing
- Use bLeftArmDestroyed to modify weapon holding pose
- Use bLeftLegFractured/bRightLegFractured for specific leg limp blends
```

---

### D.7 EventBus Integration

#### D.7.1 Publishing Events from AttributeSet

```cpp
// SuspenseCoreLimbAttributeSet.cpp

void USuspenseCoreLimbAttributeSet::BroadcastLimbDamageEvent(
    ELimbType Limb,
    const FGameplayEffectModCallbackData& Data)
{
    // Get EventBus
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    USuspenseCoreEventBus* EventBus = World->GetSubsystem<USuspenseCoreEventBus>();
    if (!EventBus)
    {
        return;
    }

    // Build event
    FSuspenseCoreLimbDamageEvent Event;
    Event.VictimActor = GetOwningActor();
    Event.DamagedLimb = Limb;
    Event.DamageAmount = FMath::Abs(Data.EvaluatedData.Magnitude);
    Event.RemainingHealth = GetLimbHealth(Limb);
    Event.HealthPercent = GetLimbHealthPercent(Limb);
    Event.bLimbDestroyed = IsLimbDestroyed(Limb);

    // Get instigator from effect context
    if (Data.EffectSpec.GetContext().IsValid())
    {
        Event.InstigatorActor = Data.EffectSpec.GetContext().GetInstigator();
    }

    // Publish
    EventBus->Publish(Event);
}

void USuspenseCoreLimbAttributeSet::HandleLimbDestruction(
    ELimbType Limb,
    const FGameplayEffectModCallbackData& Data)
{
    // Grant destroyed tag
    UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
    if (ASC)
    {
        FGameplayTag DestroyedTag = GetDestroyedTagForLimb(Limb);
        ASC->AddLooseGameplayTag(DestroyedTag);

        // Apply fracture if limb can be fractured
        if (CanLimbBeFractured(Limb))
        {
            FGameplayTag FractureTag = GetFractureTagForLimb(Limb);
            ASC->AddLooseGameplayTag(FractureTag);

            // Publish fracture event
            PublishFractureEvent(Limb, false);  // false = not from fall
        }

        // Check if sprint should be blocked
        if (Limb == ELimbType::LeftLeg || Limb == ELimbType::RightLeg)
        {
            ASC->AddLooseGameplayTag(SuspenseCoreTags::Ability_Block_Sprint);

            // Check if both legs fractured - block jump too
            if (ASC->HasMatchingGameplayTag(SuspenseCoreTags::State_Limb_LeftLeg_Fractured) &&
                ASC->HasMatchingGameplayTag(SuspenseCoreTags::State_Limb_RightLeg_Fractured))
            {
                ASC->AddLooseGameplayTag(SuspenseCoreTags::Ability_Block_Jump);
            }
        }
    }

    // Publish destruction event
    if (USuspenseCoreEventBus* EventBus = GetWorld()->GetSubsystem<USuspenseCoreEventBus>())
    {
        FSuspenseCoreLimbDestroyedEvent Event;
        Event.AffectedActor = GetOwningActor();
        Event.DestroyedLimb = Limb;
        EventBus->Publish(Event);
    }
}
```

#### D.7.2 UI Widget Subscription

```cpp
// LimbStatusWidget.cpp

void ULimbStatusWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Get EventBus
    if (UWorld* World = GetWorld())
    {
        if (USuspenseCoreEventBus* EventBus = World->GetSubsystem<USuspenseCoreEventBus>())
        {
            // Subscribe to limb events
            EventBus->Subscribe<FSuspenseCoreLimbDamageEvent>(
                this, &ULimbStatusWidget::OnLimbDamageReceived);

            EventBus->Subscribe<FSuspenseCoreLimbHealedEvent>(
                this, &ULimbStatusWidget::OnLimbHealed);

            EventBus->Subscribe<FSuspenseCoreBleedingStartEvent>(
                this, &ULimbStatusWidget::OnBleedingStarted);

            EventBus->Subscribe<FSuspenseCoreBleedingStopEvent>(
                this, &ULimbStatusWidget::OnBleedingStopped);

            EventBus->Subscribe<FSuspenseCoreFractureEvent>(
                this, &ULimbStatusWidget::OnFractureReceived);
        }
    }
}

void ULimbStatusWidget::OnLimbDamageReceived(const FSuspenseCoreLimbDamageEvent& Event)
{
    // Only process events for local player
    APawn* LocalPawn = GetOwningPlayerPawn();
    if (!LocalPawn || Event.VictimActor.Get() != LocalPawn)
    {
        return;
    }

    // Update limb display
    UpdateLimbHealthDisplay(Event.DamagedLimb, Event.HealthPercent);

    // Flash damage indicator
    PlayDamageFlash(Event.DamagedLimb);

    // Update destroyed state if needed
    if (Event.bLimbDestroyed)
    {
        SetLimbDestroyedVisual(Event.DamagedLimb, true);
    }
}

void ULimbStatusWidget::OnFractureReceived(const FSuspenseCoreFractureEvent& Event)
{
    APawn* LocalPawn = GetOwningPlayerPawn();
    if (!LocalPawn || Event.AffectedActor.Get() != LocalPawn)
    {
        return;
    }

    // Show fracture icon on limb
    SetLimbFracturedVisual(Event.FracturedLimb, true);

    // Play fracture sound/notification
    PlayFractureNotification(Event.FracturedLimb);
}
```

---

### D.8 GameplayEffect Blueprint Setup

#### D.8.1 Penalty Effect Configuration (Data Asset/Blueprint)

```
GE_LegFracturePenalty (GameplayEffect Blueprint)
├── Duration Policy: Infinite
├── Period: None
│
├── Modifiers:
│   ├── Attribute: LimbAttributeSet.MovementSpeedPenalty
│   │   └── Modifier Op: Multiply
│   │   └── Magnitude: Scalable Float = 0.6
│   │
│   ├── Attribute: LimbAttributeSet.SprintSpeedPenalty
│   │   └── Modifier Op: Multiply
│   │   └── Magnitude: Scalable Float = 0.4
│   │
│   └── Attribute: LimbAttributeSet.JumpHeightPenalty
│       └── Modifier Op: Multiply
│       └── Magnitude: Scalable Float = 0.5
│
├── Granted Tags:
│   └── State.Limb.XXXLeg.Fractured
│
├── Asset Tags:
│   └── Penalty.Limb.XXXLeg
│
└── Gameplay Cues:
    └── GameplayCue.Status.Fracture.Leg
```

#### D.8.2 Bleeding Effect Configuration

```
GE_HeavyBleeding (GameplayEffect Blueprint)
├── Duration Policy: Infinite
├── Period: 1.0 seconds
│
├── Periodic Execution:
│   └── UBleedingDamageExecution
│       └── Applies 1.2 damage to source limb per tick
│
├── Granted Tags:
│   └── State.Status.Bleeding.Heavy
│
├── Ongoing Tag Requirements:
│   └── None (persists until cured)
│
└── Gameplay Cues:
    ├── GameplayCue.Status.Bleeding.Heavy.Start (OnApply)
    ├── GameplayCue.Status.Bleeding.Heavy.Tick (OnTick)
    └── GameplayCue.Status.Bleeding.Heavy.End (OnRemove)
```

---

### D.9 Implementation Checklist with Order

```
Phase 1: Foundation
├── [ ] 1.1 Create ELimbType enum
├── [ ] 1.2 Create USuspenseCoreLimbAttributeSet with all limb attributes
├── [ ] 1.3 Register AttributeSet on character
├── [ ] 1.4 Define all native GameplayTags in SuspenseCoreGameplayTags.h/cpp
└── [ ] 1.5 Create bone-to-limb mapping DataTable

Phase 2: Damage Pipeline
├── [ ] 2.1 Create USuspenseCoreBoneLimbResolver
├── [ ] 2.2 Create FSuspenseCoreDamageInfo struct
├── [ ] 2.3 Create USuspenseCoreDamageProcessorSubsystem
├── [ ] 2.4 Create ULimbDamageExecutionCalculation
├── [ ] 2.5 Create GE_LimbDamage base effect
├── [ ] 2.6 Update projectile OnHit to extract bone name
└── [ ] 2.7 Update hitscan trace to extract bone name

Phase 3: Status Effects
├── [ ] 3.1 Create bleeding GameplayEffects (Light, Heavy)
├── [ ] 3.2 Create fracture GameplayEffects per limb
├── [ ] 3.3 Create penalty GameplayEffects per severity level
├── [ ] 3.4 Implement UBleedingDamageExecution
└── [ ] 3.5 Add status effect chance rolls in ProcessStatusEffects()

Phase 4: Consumer System Integration
├── [ ] 4.1 Update MovementComponent to read penalty attributes
├── [ ] 4.2 Add tag checks for sprint/jump blocking
├── [ ] 4.3 Update WeaponComponent for reload/aim speed penalties
├── [ ] 4.4 Update weapon sway calculation
├── [ ] 4.5 Add AnimInstance limb state variables
└── [ ] 4.6 Update AnimBP with limp/pain blends

Phase 5: EventBus & UI
├── [ ] 5.1 Define all limb-related event structs
├── [ ] 5.2 Add event publishing in AttributeSet
├── [ ] 5.3 Create limb status widget
├── [ ] 5.4 Subscribe widget to EventBus
├── [ ] 5.5 Add damage direction indicator
└── [ ] 5.6 Add screen effects (vignette, shake)

Phase 6: Fall Damage
├── [ ] 6.1 Hook into CharacterMovementComponent::ProcessLanded
├── [ ] 6.2 Implement fall height calculation
├── [ ] 6.3 Apply damage to legs via GAS
├── [ ] 6.4 Add fracture chance calculation
└── [ ] 6.5 Publish fall damage event

Phase 7: Healing Integration
├── [ ] 7.1 Update medical item handlers to target limbs
├── [ ] 7.2 Add healing execution calculation
├── [ ] 7.3 Add status effect removal logic
├── [ ] 7.4 Add limb restoration (surgical kit)
└── [ ] 7.5 Add splint application logic

Phase 8: Network & Polish
├── [ ] 8.1 Configure attribute replication
├── [ ] 8.2 Add GameplayCues for effects
├── [ ] 8.3 Test with multiple clients
├── [ ] 8.4 Profile and optimize
└── [ ] 8.5 Add debug visualization
```

---

## Appendix E: SuspenseCore Architecture Integration

This appendix describes how to integrate the Limb Damage System with the **existing** SuspenseCore architecture. **EXTEND the existing SuspenseCoreAttributeSet** - do NOT create a separate LimbAttributeSet.

---

### E.1 Architecture Decision: Unified AttributeSet

**Why NOT create a separate LimbAttributeSet:**

1. **Single Source of Truth:** Health and limb health are fundamentally linked
2. **Death Logic:** `Health <= 0` already triggers death - Health = ThoraxHealth
3. **No Redundancy:** Avoid two separate health tracking systems
4. **Simpler Integration:** One AttributeSet to query, one place for damage processing

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     UNIFIED HEALTH ARCHITECTURE                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  USuspenseCoreAttributeSet (EXTENDED)                                       │
│  ├── Health          ← EXISTING = ThoraxHealth (alias for compatibility)   │
│  ├── MaxHealth       ← EXISTING = MaxThoraxHealth                          │
│  ├── Stamina         ← EXISTING (unchanged)                                │
│  ├── Armor           ← EXISTING (unchanged)                                │
│  │                                                                          │
│  ├── HeadHealth      ← NEW (35 HP, CRITICAL - death if <= 0)               │
│  ├── StomachHealth   ← NEW (70 HP, overflow to Thorax)                     │
│  ├── LeftArmHealth   ← NEW (60 HP, overflow to Thorax)                     │
│  ├── RightArmHealth  ← NEW (60 HP, overflow to Thorax)                     │
│  ├── LeftLegHealth   ← NEW (65 HP, overflow to Stomach)                    │
│  ├── RightLegHealth  ← NEW (65 HP, overflow to Stomach)                    │
│  │                                                                          │
│  ├── LimbMovementPenalty   ← NEW (0.0 - 1.0, stacks with weight penalty)   │
│  ├── LimbReloadPenalty     ← NEW (0.0 - 1.0)                               │
│  ├── LimbAimPenalty        ← NEW (0.0 - 1.0)                               │
│  ├── WeaponSwayMultiplier  ← NEW (1.0 = normal, 2.0 = double)              │
│  │                                                                          │
│  ├── IncomingLimbDamage    ← NEW META (for limb damage routing)            │
│  └── TargetLimbIndex       ← NEW META (ELimbType as float)                 │
│                                                                             │
│  DEATH CONDITION: Health <= 0 OR HeadHealth <= 0                            │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### E.2 Key Files to Modify

| Purpose | Existing File | Action |
|---------|---------------|--------|
| Limb Attributes | `SuspenseCoreAttributeSet.h/cpp` | **EXTEND** with limb health + penalties |
| Attribute Defaults | `SuspenseCoreAttributeDefaults.h` | **ADD** limb health defaults |
| GameplayTags | `SuspenseCoreGameplayTags.h/cpp` | **EXTEND** with limb state tags |
| Movement Penalties | `SuspenseCoreMovementAttributeSet.cpp` | **MODIFY** to read LimbMovementPenalty |
| Damage Processing | `SuspenseCoreAttributeSet.cpp` | **EXTEND** PostGameplayEffectExecute |

**NO new AttributeSet files needed.**

---

### E.3 Health-Limb Relationship

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         DAMAGE FLOW DIAGRAM                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  CRITICAL ZONES (Death on 0 HP):                                            │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │  HeadHealth = 35 HP                                                 │    │
│  │  └── HeadHealth <= 0 → DEATH                                        │    │
│  │                                                                     │    │
│  │  Health (ThoraxHealth) = 85 HP                                      │    │
│  │  └── Health <= 0 → DEATH                                            │    │
│  │  └── Receives overflow from: Stomach, Arms                          │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
│  NON-CRITICAL ZONES (Destroyed state, overflow damage):                     │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │  StomachHealth = 70 HP                                              │    │
│  │  └── Overflow target: Thorax (Health)                               │    │
│  │  └── Receives overflow from: Legs                                   │    │
│  │                                                                     │    │
│  │  LeftArmHealth / RightArmHealth = 60 HP each                        │    │
│  │  └── Overflow target: Thorax (Health)                               │    │
│  │                                                                     │    │
│  │  LeftLegHealth / RightLegHealth = 65 HP each                        │    │
│  │  └── Overflow target: Stomach                                       │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
│  OVERFLOW CHAIN EXAMPLE:                                                    │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │  [Hit Leg for 100 damage, Leg has 65 HP]                            │    │
│  │         │                                                           │    │
│  │         ▼                                                           │    │
│  │  LegHealth: 65 - 100 = -35 → SET TO 0 (Destroyed)                   │    │
│  │  Overflow: 35 × 0.7 = 24.5 → Stomach                                │    │
│  │         │                                                           │    │
│  │         ▼                                                           │    │
│  │  StomachHealth: 70 - 24.5 = 45.5 HP                                 │    │
│  │  (Stomach survives, no further overflow)                            │    │
│  │         │                                                           │    │
│  │         ▼                                                           │    │
│  │  Health (Thorax): UNCHANGED                                         │    │
│  │  → Player ALIVE but leg destroyed, cannot sprint                    │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### E.4 Extending SuspenseCoreAttributeSet

```cpp
// ============================================================================
// SuspenseCoreAttributeSet.h - ADD TO EXISTING CLASS
// ============================================================================

// Forward declare
enum class ELimbType : uint8;

UCLASS()
class GAS_API USuspenseCoreAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    // ========================================================================
    // EXISTING ATTRIBUTES (KEEP AS-IS)
    // ========================================================================

    // Health = ThoraxHealth (kept for backward compatibility)
    // Any system reading "Health" continues to work
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Health")
    FGameplayAttributeData Health;  // Default: 85 HP (was 100, now = Thorax)
    ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, Health)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Health")
    FGameplayAttributeData MaxHealth;  // Default: 85 HP
    ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, MaxHealth)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "Stamina")
    FGameplayAttributeData Stamina;
    ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, Stamina)

    // ... other existing attributes ...

    // ========================================================================
    // NEW: LIMB HEALTH ATTRIBUTES
    // ========================================================================

    // Head - CRITICAL (death if <= 0)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HeadHealth, Category = "Limb Health")
    FGameplayAttributeData HeadHealth;  // Default: 35 HP
    ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, HeadHealth)

    UPROPERTY(BlueprintReadOnly, Category = "Limb Health|Max")
    FGameplayAttributeData MaxHeadHealth;  // Default: 35 HP
    ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, MaxHeadHealth)

    // Stomach - overflow to Thorax
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_StomachHealth, Category = "Limb Health")
    FGameplayAttributeData StomachHealth;  // Default: 70 HP
    ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, StomachHealth)

    UPROPERTY(BlueprintReadOnly, Category = "Limb Health|Max")
    FGameplayAttributeData MaxStomachHealth;  // Default: 70 HP
    ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, MaxStomachHealth)

    // Arms - overflow to Thorax
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_LeftArmHealth, Category = "Limb Health")
    FGameplayAttributeData LeftArmHealth;  // Default: 60 HP
    ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, LeftArmHealth)

    UPROPERTY(BlueprintReadOnly, Category = "Limb Health|Max")
    FGameplayAttributeData MaxLeftArmHealth;  // Default: 60 HP
    ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, MaxLeftArmHealth)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RightArmHealth, Category = "Limb Health")
    FGameplayAttributeData RightArmHealth;  // Default: 60 HP
    ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, RightArmHealth)

    UPROPERTY(BlueprintReadOnly, Category = "Limb Health|Max")
    FGameplayAttributeData MaxRightArmHealth;  // Default: 60 HP
    ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, MaxRightArmHealth)

    // Legs - overflow to Stomach
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_LeftLegHealth, Category = "Limb Health")
    FGameplayAttributeData LeftLegHealth;  // Default: 65 HP
    ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, LeftLegHealth)

    UPROPERTY(BlueprintReadOnly, Category = "Limb Health|Max")
    FGameplayAttributeData MaxLeftLegHealth;  // Default: 65 HP
    ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, MaxLeftLegHealth)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RightLegHealth, Category = "Limb Health")
    FGameplayAttributeData RightLegHealth;  // Default: 65 HP
    ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, RightLegHealth)

    UPROPERTY(BlueprintReadOnly, Category = "Limb Health|Max")
    FGameplayAttributeData MaxRightLegHealth;  // Default: 65 HP
    ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, MaxRightLegHealth)

    // ========================================================================
    // NEW: LIMB PENALTY ATTRIBUTES
    // ========================================================================

    // Movement penalty from leg damage (stacks with weight penalty)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_LimbMovementPenalty, Category = "Limb Penalty")
    FGameplayAttributeData LimbMovementPenalty;  // 0.0 = no penalty, 0.5 = 50% slower
    ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, LimbMovementPenalty)

    // Reload speed penalty from arm damage
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_LimbReloadPenalty, Category = "Limb Penalty")
    FGameplayAttributeData LimbReloadPenalty;  // 0.0 = no penalty, 0.5 = 50% slower
    ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, LimbReloadPenalty)

    // Aim speed penalty from arm damage
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_LimbAimPenalty, Category = "Limb Penalty")
    FGameplayAttributeData LimbAimPenalty;  // 0.0 = no penalty
    ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, LimbAimPenalty)

    // Weapon sway multiplier from arm damage
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_WeaponSwayMultiplier, Category = "Limb Penalty")
    FGameplayAttributeData WeaponSwayMultiplier;  // 1.0 = normal, 2.0 = double sway
    ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, WeaponSwayMultiplier)

    // ========================================================================
    // NEW: META ATTRIBUTES (for damage routing, not replicated)
    // ========================================================================

    // Incoming limb damage amount (processed in PostGameplayEffectExecute)
    UPROPERTY(BlueprintReadOnly, Category = "Meta")
    FGameplayAttributeData IncomingLimbDamage;
    ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, IncomingLimbDamage)

    // Target limb index (ELimbType cast to float)
    UPROPERTY(BlueprintReadOnly, Category = "Meta")
    FGameplayAttributeData TargetLimbIndex;
    ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, TargetLimbIndex)

    // ========================================================================
    // NEW: LIMB HELPER FUNCTIONS
    // ========================================================================

    // Get health for specific limb
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Limb")
    float GetLimbHealth(ELimbType Limb) const;

    // Set health for specific limb
    void SetLimbHealth(ELimbType Limb, float NewValue);

    // Get max health for specific limb
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Limb")
    float GetLimbMaxHealth(ELimbType Limb) const;

    // Get health percentage (0.0 - 1.0)
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Limb")
    float GetLimbHealthPercent(ELimbType Limb) const;

    // Check if limb is destroyed (HP <= 0)
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Limb")
    bool IsLimbDestroyed(ELimbType Limb) const;

    // Check if limb is critical (Head or Thorax)
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Limb")
    static bool IsCriticalLimb(ELimbType Limb);

    // Get overflow target for limb
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Limb")
    static ELimbType GetOverflowTarget(ELimbType Limb);

    // Alias: GetThoraxHealth() = GetHealth()
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Limb")
    float GetThoraxHealth() const { return GetHealth(); }

    float GetMaxThoraxHealth() const { return GetMaxHealth(); }

protected:
    // EXISTING
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

    // NEW: Replication callbacks for limbs
    UFUNCTION()
    void OnRep_HeadHealth(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_StomachHealth(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_LeftArmHealth(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_RightArmHealth(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_LeftLegHealth(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_RightLegHealth(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_LimbMovementPenalty(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_LimbReloadPenalty(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_LimbAimPenalty(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_WeaponSwayMultiplier(const FGameplayAttributeData& OldValue);

private:
    // NEW: Limb damage processing
    void ProcessLimbDamage(float Damage, ELimbType TargetLimb, const FGameplayEffectModCallbackData& Data);

    // NEW: Apply damage with overflow
    void ApplyDamageToLimb(ELimbType Limb, float Damage, const FGameplayEffectModCallbackData& Data);

    // NEW: Recalculate all penalties based on limb health
    void RecalculateLimbPenalties();

    // NEW: Check death condition (Health <= 0 OR HeadHealth <= 0)
    void CheckDeathCondition(const FGameplayEffectModCallbackData& Data);

    // NEW: Mark limb as destroyed (add tag, apply effects)
    void MarkLimbDestroyed(ELimbType Limb);

    // NEW: Broadcast limb damage event via EventBus
    void BroadcastLimbDamageEvent(ELimbType Limb, float Damage, float RemainingHealth);
};
```

---

### E.5 Death Logic Implementation

```cpp
// ============================================================================
// SuspenseCoreAttributeSet.cpp - MODIFY PostGameplayEffectExecute
// ============================================================================

void USuspenseCoreAttributeSet::PostGameplayEffectExecute(
    const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    // ========================================================================
    // EXISTING: Direct damage processing (IncomingDamage → Health)
    // ========================================================================
    if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
    {
        const float LocalDamage = GetIncomingDamage();
        SetIncomingDamage(0.0f);

        // Apply armor, then to Health (Thorax)
        const float DamageAfterArmor = FMath::Max(LocalDamage - GetArmor(), 0.0f);
        const float NewHealth = FMath::Clamp(GetHealth() - DamageAfterArmor, 0.0f, GetMaxHealth());
        SetHealth(NewHealth);

        // Recalculate penalties (Thorax damage doesn't directly affect movement)
        RecalculateLimbPenalties();
    }

    // ========================================================================
    // NEW: Limb damage processing (IncomingLimbDamage → specific limb)
    // ========================================================================
    if (Data.EvaluatedData.Attribute == GetIncomingLimbDamageAttribute())
    {
        const float Damage = GetIncomingLimbDamage();
        SetIncomingLimbDamage(0.0f);

        const ELimbType TargetLimb = static_cast<ELimbType>(
            FMath::RoundToInt(GetTargetLimbIndex()));
        SetTargetLimbIndex(0.0f);

        if (Damage > 0.0f)
        {
            ProcessLimbDamage(Damage, TargetLimb, Data);
        }
    }

    // ========================================================================
    // MODIFIED: Death check (now includes HeadHealth)
    // ========================================================================
    CheckDeathCondition(Data);
}

void USuspenseCoreAttributeSet::CheckDeathCondition(
    const FGameplayEffectModCallbackData& Data)
{
    bool bShouldDie = false;
    FString DeathReason;

    // Check Thorax (Health)
    if (GetHealth() <= 0.0f)
    {
        bShouldDie = true;
        DeathReason = TEXT("Thorax destroyed");
    }

    // Check Head
    if (GetHeadHealth() <= 0.0f)
    {
        bShouldDie = true;
        DeathReason = TEXT("Head destroyed");
    }

    if (bShouldDie)
    {
        UE_LOG(LogSuspenseCore, Log, TEXT("Death: %s"), *DeathReason);

        // Get instigator from effect context
        AActor* Instigator = nullptr;
        AActor* Causer = nullptr;
        if (Data.EffectSpec.GetContext().IsValid())
        {
            Instigator = Data.EffectSpec.GetContext().GetInstigator();
            Causer = Data.EffectSpec.GetContext().GetEffectCauser();
        }

        HandleDeath(Instigator, Causer);
    }
}

void USuspenseCoreAttributeSet::ProcessLimbDamage(
    float Damage,
    ELimbType TargetLimb,
    const FGameplayEffectModCallbackData& Data)
{
    // Route to specific limb, handling Thorax specially
    if (TargetLimb == ELimbType::Thorax)
    {
        // Thorax = Health attribute
        const float OldHealth = GetHealth();
        const float NewHealth = FMath::Max(0.0f, OldHealth - Damage);
        SetHealth(NewHealth);

        BroadcastLimbDamageEvent(ELimbType::Thorax, Damage, NewHealth);
    }
    else
    {
        ApplyDamageToLimb(TargetLimb, Damage, Data);
    }

    RecalculateLimbPenalties();
}

void USuspenseCoreAttributeSet::ApplyDamageToLimb(
    ELimbType Limb,
    float Damage,
    const FGameplayEffectModCallbackData& Data)
{
    const float CurrentHealth = GetLimbHealth(Limb);
    float NewHealth = CurrentHealth - Damage;

    // Check for overflow (non-critical limbs only)
    if (NewHealth < 0.0f && !IsCriticalLimb(Limb))
    {
        // Limb destroyed
        SetLimbHealth(Limb, 0.0f);
        MarkLimbDestroyed(Limb);

        // Calculate overflow damage (70% passes through)
        const float OverflowDamage = FMath::Abs(NewHealth) * 0.7f;
        const ELimbType OverflowTarget = GetOverflowTarget(Limb);

        UE_LOG(LogSuspenseCore, Verbose,
            TEXT("Limb %d destroyed, overflow %.1f to limb %d"),
            static_cast<int32>(Limb), OverflowDamage, static_cast<int32>(OverflowTarget));

        // Recursive apply to overflow target
        if (OverflowTarget == ELimbType::Thorax)
        {
            // Thorax = Health
            const float OldHealth = GetHealth();
            const float ThoraxNewHealth = FMath::Max(0.0f, OldHealth - OverflowDamage);
            SetHealth(ThoraxNewHealth);
            BroadcastLimbDamageEvent(ELimbType::Thorax, OverflowDamage, ThoraxNewHealth);
        }
        else
        {
            ApplyDamageToLimb(OverflowTarget, OverflowDamage, Data);
        }

        BroadcastLimbDamageEvent(Limb, Damage, 0.0f);
    }
    else
    {
        // Normal damage or critical limb
        NewHealth = FMath::Max(0.0f, NewHealth);
        SetLimbHealth(Limb, NewHealth);
        BroadcastLimbDamageEvent(Limb, Damage, NewHealth);
    }
}

bool USuspenseCoreAttributeSet::IsCriticalLimb(ELimbType Limb)
{
    return Limb == ELimbType::Head || Limb == ELimbType::Thorax;
}

ELimbType USuspenseCoreAttributeSet::GetOverflowTarget(ELimbType Limb)
{
    switch (Limb)
    {
        case ELimbType::Stomach:    return ELimbType::Thorax;
        case ELimbType::LeftArm:    return ELimbType::Thorax;
        case ELimbType::RightArm:   return ELimbType::Thorax;
        case ELimbType::LeftLeg:    return ELimbType::Stomach;
        case ELimbType::RightLeg:   return ELimbType::Stomach;
        default:                    return ELimbType::Thorax;
    }
}
```

---

### E.6 Penalty Recalculation

```cpp
void USuspenseCoreAttributeSet::RecalculateLimbPenalties()
{
    // ========================================================================
    // LEG PENALTIES → Movement
    // ========================================================================
    const float LeftLegPercent = GetLimbHealthPercent(ELimbType::LeftLeg);
    const float RightLegPercent = GetLimbHealthPercent(ELimbType::RightLeg);

    // Use worst leg for movement penalty
    const float WorstLegPercent = FMath::Min(LeftLegPercent, RightLegPercent);

    // 0% HP → 0.55 penalty (45% speed), 100% HP → 0 penalty
    float MovementPenalty = (1.0f - WorstLegPercent) * 0.55f;

    // Check for fractures (add extra penalty)
    UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
    if (ASC)
    {
        if (ASC->HasMatchingGameplayTag(SuspenseCoreTags::State::Limb::LeftLeg::Fractured) ||
            ASC->HasMatchingGameplayTag(SuspenseCoreTags::State::Limb::RightLeg::Fractured))
        {
            MovementPenalty = FMath::Min(1.0f, MovementPenalty + 0.2f);
        }
    }

    SetLimbMovementPenalty(MovementPenalty);

    // ========================================================================
    // ARM PENALTIES → Reload, Aim, Sway
    // ========================================================================
    const float LeftArmPercent = GetLimbHealthPercent(ELimbType::LeftArm);
    const float RightArmPercent = GetLimbHealthPercent(ELimbType::RightArm);

    // Right arm (dominant) has more impact: 60% right, 40% left
    const float ArmHealthWeighted = (RightArmPercent * 0.6f) + (LeftArmPercent * 0.4f);

    // Reload penalty: 0% arms → 0.6 penalty (40% speed)
    const float ReloadPenalty = (1.0f - ArmHealthWeighted) * 0.6f;
    SetLimbReloadPenalty(ReloadPenalty);

    // Aim penalty: 0% arms → 0.5 penalty (50% ADS speed)
    const float AimPenalty = (1.0f - ArmHealthWeighted) * 0.5f;
    SetLimbAimPenalty(AimPenalty);

    // Weapon sway: 0% arms → 2.0x sway
    const float SwayMultiplier = 1.0f + (1.0f - ArmHealthWeighted);
    SetWeaponSwayMultiplier(SwayMultiplier);

    // ========================================================================
    // UPDATE MOVEMENT COMPONENT
    // ========================================================================
    if (AActor* Owner = GetOwningActor())
    {
        if (ACharacter* Character = Cast<ACharacter>(Owner))
        {
            // Notify movement component to refresh speeds
            if (USuspenseCoreCharacterMovementComponent* Movement =
                Cast<USuspenseCoreCharacterMovementComponent>(Character->GetCharacterMovement()))
            {
                Movement->OnLimbPenaltyChanged();
            }
        }
    }
}
```

---

### E.7 Extending GameplayTags

```cpp
// In SuspenseCoreGameplayTags.h - ADD to existing namespace structure

namespace SuspenseCoreTags
{
    // === EXISTING TAGS (don't modify) ===
    namespace State { /* Dead, Stunned, etc. */ }
    namespace Ability { /* Movement, Weapon, etc. */ }

    // === ADD: Limb State Tags ===
    namespace State::Limb
    {
        namespace Head
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Destroyed);  // State.Limb.Head.Destroyed
        }

        namespace Thorax
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Critical);   // State.Limb.Thorax.Critical
        }

        namespace LeftArm
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Destroyed);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fractured);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Splinted);
        }

        namespace RightArm
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Destroyed);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fractured);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Splinted);
        }

        namespace LeftLeg
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Destroyed);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fractured);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Splinted);
        }

        namespace RightLeg
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Destroyed);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fractured);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Splinted);
        }
    }

    // === ADD: Status Effect Tags ===
    namespace State::Status
    {
        namespace Bleeding
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Light);      // State.Status.Bleeding.Light
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Heavy);      // State.Status.Bleeding.Heavy
        }

        namespace Pain
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Light);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Moderate);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Severe);
        }

        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tremor);
    }

    // === ADD: Ability Block Tags (for limb restrictions) ===
    namespace Ability::Block
    {
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sprint);         // Ability.Block.Sprint
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Jump);           // Ability.Block.Jump
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(ADS);            // Ability.Block.ADS
    }

    // === ADD: SetByCaller Tags for Limb Damage ===
    namespace Data::Limb
    {
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage);         // Data.Limb.Damage
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(TargetIndex);    // Data.Limb.TargetIndex
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(HealAmount);     // Data.Limb.HealAmount
    }
}
```

---

### E.8 Integrating with Existing MovementAttributeSet

**Key Pattern:** Read `LimbMovementPenalty` from the SAME `SuspenseCoreAttributeSet`.

```cpp
// In SuspenseCoreMovementAttributeSet.h - ADD limb penalty integration

// === ADD: Limb Penalty Integration ===
// Called from SuspenseCoreAttributeSet when penalties change
UFUNCTION()
void OnLimbPenaltyChanged();

// === MODIFY: GetEffectiveWalkSpeed() ===
float USuspenseCoreMovementAttributeSet::GetEffectiveWalkSpeed() const
{
    float Speed = GetWalkSpeed();

    // Existing weight penalty
    Speed *= (1.0f - GetWeightSpeedPenalty());

    // ADD: Limb penalty from SuspenseCoreAttributeSet (NOT separate LimbAttributeSet)
    if (USuspenseCoreAttributeSet* AttrSet = GetSuspenseCoreAttributeSet())
    {
        Speed *= (1.0f - AttrSet->GetLimbMovementPenalty());
    }

    return Speed;
}

// === MODIFY: GetEffectiveSprintSpeed() ===
float USuspenseCoreMovementAttributeSet::GetEffectiveSprintSpeed() const
{
    // Check if sprint is blocked by limb damage
    if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
    {
        if (ASC->HasMatchingGameplayTag(SuspenseCoreTags::Ability::Block::Sprint))
        {
            return 0.0f;  // Cannot sprint
        }
    }

    float Speed = GetSprintSpeed();
    Speed *= (1.0f - GetWeightSpeedPenalty() * 1.5f);

    // Limb penalty applies MORE to sprint (from same AttributeSet)
    if (USuspenseCoreAttributeSet* AttrSet = GetSuspenseCoreAttributeSet())
    {
        Speed *= (1.0f - AttrSet->GetLimbMovementPenalty() * 1.5f);
    }

    return Speed;
}

// === ADD: Helper to get SuspenseCoreAttributeSet ===
USuspenseCoreAttributeSet* USuspenseCoreMovementAttributeSet::GetSuspenseCoreAttributeSet() const
{
    if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
    {
        return ASC->GetSet<USuspenseCoreAttributeSet>();
    }
    return nullptr;
}
```

---

### E.9 Service & Component Registration

#### E.7.1 BoneLimbResolver as Subsystem

```cpp
// Source/GAS/Public/SuspenseCore/Subsystems/SuspenseCoreBoneLimbResolverSubsystem.h

UCLASS()
class GAS_API USuspenseCoreBoneLimbResolverSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // Access point
    static USuspenseCoreBoneLimbResolverSubsystem* Get(const UWorld* World)
    {
        return World ? World->GetSubsystem<USuspenseCoreBoneLimbResolverSubsystem>() : nullptr;
    }

    // Main function
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Limb")
    ELimbType ResolveBoneToLimb(FName BoneName) const;

    // DataTable-based mapping
    UPROPERTY(EditDefaultsOnly, Category = "Config")
    UDataTable* BoneMappingTable;

protected:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    // Cached mappings from DataTable
    TMap<FName, ELimbType> CachedBoneMappings;

    void LoadBoneMappings();
};
```

**Registration:** Automatic - `UWorldSubsystem` registers itself when world is created.

#### E.7.2 LimbDamageProcessor as Subsystem

```cpp
// Source/GAS/Public/SuspenseCore/Subsystems/SuspenseCoreLimbDamageSubsystem.h

UCLASS()
class GAS_API USuspenseCoreLimbDamageSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    static USuspenseCoreLimbDamageSubsystem* Get(const UWorld* World);

    // Main entry point - called from weapon/projectile code
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Damage")
    void ProcessLimbDamage(
        AActor* Target,
        AActor* Instigator,
        float BaseDamage,
        FName HitBoneName,
        const FHitResult& HitResult
    );

    // Fall damage entry point
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Damage")
    void ProcessFallDamage(AActor* Target, float FallHeight);

protected:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

private:
    void ApplyDamageViaGAS(
        UAbilitySystemComponent* TargetASC,
        ELimbType Limb,
        float FinalDamage,
        AActor* Instigator
    );

    void ProcessStatusEffectChances(
        UAbilitySystemComponent* TargetASC,
        ELimbType Limb,
        float Damage
    );
};
```

**Registration:** Automatic via `UWorldSubsystem`.

**Usage from Weapon:**

```cpp
// In existing weapon/projectile code
void ASuspenseCoreProjectile::OnHit(const FHitResult& Hit)
{
    // Get subsystem
    USuspenseCoreLimbDamageSubsystem* LimbDamage =
        USuspenseCoreLimbDamageSubsystem::Get(GetWorld());

    if (LimbDamage && Hit.GetActor())
    {
        LimbDamage->ProcessLimbDamage(
            Hit.GetActor(),
            GetInstigator(),
            ProjectileData.BaseDamage,
            Hit.BoneName,
            Hit
        );
    }
}
```

---

### E.8 EventBus Integration (Existing Pattern)

The project already has EventBus integration in `SuspenseCoreAbilitySystemComponent`:

```cpp
// EXISTING in SuspenseCoreAbilitySystemComponent
void PublishAttributeChangeEvent(
    const FGameplayAttribute& Attribute,
    float OldValue,
    float NewValue
);

// ADD: Limb-specific events
void PublishLimbDamageEvent(
    ELimbType Limb,
    float Damage,
    float RemainingHealth,
    AActor* Instigator
);

void PublishStatusEffectEvent(
    FGameplayTag StatusTag,
    bool bApplied  // true = started, false = ended
);
```

**Event Structs (add to existing Events folder):**

```cpp
// Source/BridgeSystem/Public/SuspenseCore/Events/SuspenseCoreLimbEvents.h

USTRUCT(BlueprintType)
struct FSuspenseCoreLimbDamageEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<AActor> Victim;

    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<AActor> Instigator;

    UPROPERTY(BlueprintReadOnly)
    ELimbType Limb = ELimbType::Thorax;

    UPROPERTY(BlueprintReadOnly)
    float Damage = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float RemainingHealth = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float HealthPercent = 1.0f;

    UPROPERTY(BlueprintReadOnly)
    bool bLimbDestroyed = false;
};

// Similar structs for Fracture, Bleeding, FallDamage events
```

---

### E.11 Folder Structure (Unified Approach)

```
Source/GAS/
├── Public/SuspenseCore/
│   ├── Attributes/
│   │   ├── SuspenseCoreAttributeSet.h          ← MODIFY (add limb attributes)
│   │   ├── SuspenseCoreAttributeDefaults.h     ← MODIFY (add limb defaults)
│   │   └── SuspenseCoreMovementAttributeSet.h  ← MODIFY (read limb penalty)
│   │
│   ├── Subsystems/
│   │   ├── SuspenseCoreBoneLimbResolverSubsystem.h  ← NEW
│   │   └── SuspenseCoreLimbDamageSubsystem.h        ← NEW
│   │
│   └── Types/
│       └── SuspenseCoreLimbTypes.h             ← NEW (ELimbType enum)
│
├── Private/SuspenseCore/
│   ├── Attributes/
│   │   ├── SuspenseCoreAttributeSet.cpp        ← MODIFY (limb damage logic)
│   │   └── SuspenseCoreMovementAttributeSet.cpp ← MODIFY
│   │
│   └── Subsystems/
│       ├── SuspenseCoreBoneLimbResolverSubsystem.cpp ← NEW
│       └── SuspenseCoreLimbDamageSubsystem.cpp      ← NEW
│
Source/BridgeSystem/
├── Public/SuspenseCore/
│   ├── Tags/
│   │   └── SuspenseCoreGameplayTags.h          ← MODIFY (add limb tags)
│   │
│   └── Events/
│       └── SuspenseCoreLimbEvents.h            ← NEW

NO SEPARATE LimbAttributeSet.h/cpp - all limb attributes in SuspenseCoreAttributeSet
```

---

### E.12 Updated Implementation Checklist (Unified Approach)

```
Phase 1: Foundation (EXTEND existing SuspenseCoreAttributeSet)
├── [ ] 1.1 Create ELimbType enum in SuspenseCoreLimbTypes.h
├── [ ] 1.2 ADD limb health attributes to SuspenseCoreAttributeSet.h
│         ├── HeadHealth, StomachHealth
│         ├── LeftArmHealth, RightArmHealth
│         └── LeftLegHealth, RightLegHealth
├── [ ] 1.3 ADD penalty attributes to SuspenseCoreAttributeSet.h
│         ├── LimbMovementPenalty
│         ├── LimbReloadPenalty, LimbAimPenalty
│         └── WeaponSwayMultiplier
├── [ ] 1.4 ADD meta attributes: IncomingLimbDamage, TargetLimbIndex
├── [ ] 1.5 UPDATE Health default from 100 → 85 (= ThoraxHealth)
├── [ ] 1.6 ADD limb defaults to SuspenseCoreAttributeDefaults.h
├── [ ] 1.7 ADD limb tags to SuspenseCoreGameplayTags.h/cpp
├── [ ] 1.8 ADD replication for new attributes (GetLifetimeReplicatedProps)
└── [ ] 1.9 Create bone-to-limb mapping DataTable

Phase 2: Damage Processing (in SuspenseCoreAttributeSet.cpp)
├── [ ] 2.1 ADD ProcessLimbDamage() method
├── [ ] 2.2 ADD ApplyDamageToLimb() with overflow logic
├── [ ] 2.3 MODIFY CheckDeathCondition() - add HeadHealth check
├── [ ] 2.4 ADD RecalculateLimbPenalties()
├── [ ] 2.5 ADD MarkLimbDestroyed() - applies tags
├── [ ] 2.6 ADD BroadcastLimbDamageEvent()
└── [ ] 2.7 MODIFY PostGameplayEffectExecute() for IncomingLimbDamage

Phase 3: Subsystems
├── [ ] 3.1 Create SuspenseCoreBoneLimbResolverSubsystem
├── [ ] 3.2 Create SuspenseCoreLimbDamageSubsystem
├── [ ] 3.3 Update projectile OnHit to call LimbDamageSubsystem
└── [ ] 3.4 Update hitscan to call LimbDamageSubsystem

Phase 4: Penalty Integration
├── [ ] 4.1 MODIFY MovementAttributeSet::GetEffectiveWalkSpeed()
├── [ ] 4.2 MODIFY MovementAttributeSet::GetEffectiveSprintSpeed()
├── [ ] 4.3 ADD Ability.Block.Sprint/Jump tag checks in abilities
├── [ ] 4.4 Update WeaponComponent for reload/aim penalties
└── [ ] 4.5 Update weapon sway calculation

Phase 5: Status Effects
├── [ ] 5.1 Create GE_LightBleeding, GE_HeavyBleeding effects
├── [ ] 5.2 Create GE_XXXFracture effects per limb
├── [ ] 5.3 Add bleeding tick damage logic
├── [ ] 5.4 Add fracture chance rolls in ProcessLimbDamage
└── [ ] 5.5 Add status effect removal on healing

Phase 6: EventBus & UI
├── [ ] 6.1 Create SuspenseCoreLimbEvents.h
├── [ ] 6.2 Implement BroadcastLimbDamageEvent() via EventBus
├── [ ] 6.3 Create limb status widget
├── [ ] 6.4 Subscribe widget to EventBus
└── [ ] 6.5 Add damage direction indicator

Phase 7: Fall Damage
├── [ ] 7.1 Add ProcessFallDamage to LimbDamageSubsystem
├── [ ] 7.2 Hook MovementComponent::ProcessLanded
├── [ ] 7.3 Apply leg damage via IncomingLimbDamage
└── [ ] 7.4 Add fracture chance on fall

Phase 8: Healing Integration
├── [ ] 8.1 Update medical handlers for limb targeting
├── [ ] 8.2 Add limb healing via GameplayEffect
├── [ ] 8.3 Add status effect removal
└── [ ] 8.4 Add surgical kit limb restoration
```

---

### E.13 Integration Points Summary (Unified Approach)

| System | Integration Point | How to Integrate |
|--------|-------------------|------------------|
| **AttributeSet** | `SuspenseCoreAttributeSet.h/cpp` | ADD limb health + penalty attributes (NO separate file) |
| **Movement** | `MovementAttributeSet::GetEffective*()` | Read `LimbMovementPenalty` from `SuspenseCoreAttributeSet` |
| **Weapon** | `WeaponComponent` | Query `LimbAimPenalty`, `LimbReloadPenalty` from same AttributeSet |
| **Animation** | `AnimInstance` | Query limb state tags via ASC |
| **UI** | Widget | Subscribe to EventBus `FSuspenseCoreLimbDamageEvent` |
| **Damage** | Projectile/Weapon | Call `LimbDamageSubsystem::ProcessLimbDamage()` |
| **Death** | `SuspenseCoreAttributeSet::CheckDeathCondition()` | `Health <= 0 OR HeadHealth <= 0` |
| **Tags** | `SuspenseCoreGameplayTags.h/cpp` | Add `State.Limb.XXX.Destroyed/Fractured` |
| **Defaults** | `SuspenseCoreAttributeDefaults.h` | Add limb HP defaults (Head=35, Thorax=85, etc.) |

**Key Principle:** All limb attributes live in `SuspenseCoreAttributeSet`.
Health = ThoraxHealth for backward compatibility.

---

*Document End*
