# Grenade Damage-over-Time (DoT) Effects - Technical Design Document

> **Version:** 1.0
> **Date:** 2026-01-22
> **Author:** Claude Code
> **Module:** GAS (GameplayAbilitySystem)
> **Status:** Implementation Complete

---

## Table of Contents

1. [Overview](#1-overview)
2. [Bleeding Effect (Shrapnel)](#2-bleeding-effect-shrapnel)
3. [Burning Effect (Incendiary)](#3-burning-effect-incendiary)
4. [GAS Integration](#4-gas-integration)
5. [Application Logic](#5-application-logic)
6. [Removal / Healing](#6-removal--healing)
7. [UI Requirements](#7-ui-requirements)
8. [Balance Reference](#8-balance-reference)

---

## 1. Overview

### Design Philosophy

SuspenseCore implements two distinct Damage-over-Time (DoT) effects for grenades, each with unique mechanics that create tactical depth:

| Effect | Source | Duration | Armor Interaction | Treatment |
|--------|--------|----------|-------------------|-----------|
| **Bleeding** | Frag grenade shrapnel | Infinite (until healed) | Only applies if Armor = 0 | Bandage / Medkit |
| **Burning** | Incendiary grenade | Fixed duration | **BYPASSES** armor | Water / Extinguish |

### Core Principle

```
BLEEDING: Shrapnel must penetrate armor to cause bleeding
          → Check armor BEFORE applying bleed effect
          → Infinite duration creates urgency to heal

BURNING:  Fire doesn't care about armor
          → Damages BOTH Armor AND Health simultaneously
          → Fixed duration (will end naturally)
```

---

## 2. Bleeding Effect (Shrapnel)

### 2.1 Mechanics

**Fragmentation grenades** disperse shrapnel that can cause bleeding wounds. However, bleeding only occurs when shrapnel penetrates:

```
┌─────────────────────────────────────────────────────────────────┐
│                    BLEEDING APPLICATION FLOW                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Frag Grenade Explodes                                          │
│         │                                                       │
│         ▼                                                       │
│  Calculate Shrapnel Hits (distance, LOS)                        │
│         │                                                       │
│         ▼                                                       │
│  Check Target Armor                                             │
│         │                                                       │
│    ┌────┴────┐                                                  │
│    │         │                                                  │
│    ▼         ▼                                                  │
│ Armor > 0  Armor = 0                                            │
│    │         │                                                  │
│    ▼         ▼                                                  │
│ NO BLEED   APPLY BLEEDING                                       │
│ (shrapnel  └── Light Bleed: 1-2 HP/tick                         │
│  blocked)  └── Heavy Bleed: 3-5 HP/tick (multiple hits)         │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Effect Classes

#### UGE_BleedingEffect_Light

| Property | Value | Notes |
|----------|-------|-------|
| Duration | Infinite | Until healed |
| Period | 1.0s | Damage tick interval |
| Damage/Tick | 1-2 HP | SetByCaller: `Data.Damage.Bleed` |
| Granted Tag | `State.Health.Bleeding.Light` | For UI indication |
| Stacking | 1 max | Refresh on reapply |
| Removal | Bandage, Medkit, Surgery | Any bleed-healing item |

#### UGE_BleedingEffect_Heavy

| Property | Value | Notes |
|----------|-------|-------|
| Duration | Infinite | Until healed |
| Period | 1.0s | Damage tick interval |
| Damage/Tick | 3-5 HP | SetByCaller: `Data.Damage.Bleed` |
| Granted Tag | `State.Health.Bleeding.Heavy` | For UI indication |
| Stacking | 1 max | Refresh on reapply |
| Removal | Medkit (with `bCanHealHeavyBleed`), Surgery | NOT bandages |

### 2.3 Source Files

```
Source/GAS/Public/SuspenseCore/Effects/Grenade/GE_BleedingEffect.h
Source/GAS/Private/SuspenseCore/Effects/Grenade/GE_BleedingEffect.cpp
```

---

## 3. Burning Effect (Incendiary)

### 3.1 Mechanics

**Incendiary grenades** create fire that burns through everything. The key mechanic is **armor bypass**:

```
┌─────────────────────────────────────────────────────────────────┐
│                    BURNING DAMAGE FLOW                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Incendiary Grenade Explodes                                    │
│         │                                                       │
│         ▼                                                       │
│  Apply UGE_IncendiaryEffect_ArmorBypass                         │
│         │                                                       │
│         ├─────────────────────────────────────────────────────┐ │
│         │                                                     │ │
│  ┌──────┴──────┐                                ┌─────────────┴─┤
│  │ MODIFIER 1  │                                │  MODIFIER 2   │
│  │ Armor Damage│                                │ Health Damage │
│  │ -5 per tick │                                │ -5 per tick   │
│  │ (direct)    │                                │ (BYPASSES     │
│  │             │                                │  IncomingDmg) │
│  └──────┬──────┘                                └─────────────┬─┤
│         │                                                     │ │
│         └─────────────────────────────────────────────────────┘ │
│                           │                                     │
│                           ▼                                     │
│  RESULT: Target loses 5 Armor + 5 Health SIMULTANEOUSLY        │
│          Total: 10 HP equivalent per tick                       │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 Effect Classes

#### UGE_IncendiaryEffect (Standard)

Uses `IncomingDamage` meta-attribute, so armor reduces damage.

| Property | Value | Notes |
|----------|-------|-------|
| Duration | SetByCaller | `Data.Grenade.BurnDuration` |
| Period | 0.5s | Faster ticks than bleed |
| Damage/Tick | 5-15 HP | SetByCaller: `Data.Damage.Burn` |
| Granted Tag | `State.Burning` | For VFX/sound |
| Stacking | 3 max (by source) | Multiple fire sources |
| Armor | **REDUCED** by armor | Standard damage pipeline |

#### UGE_IncendiaryEffect_ArmorBypass (New)

Directly modifies BOTH Armor AND Health attributes, bypassing the damage pipeline.

| Property | Value | Notes |
|----------|-------|-------|
| Duration | SetByCaller | `Data.Grenade.BurnDuration` |
| Period | 0.5s | Same as standard |
| Armor Damage/Tick | -5 | SetByCaller: `Data.Damage.Burn.Armor` (NEGATIVE) |
| Health Damage/Tick | -5 | SetByCaller: `Data.Damage.Burn.Health` (NEGATIVE) |
| Granted Tag | `State.Burning` | Same visual effect |
| Stacking | 3 max (by source) | Multiple fire sources |
| Armor | **BYPASSED** | Fire burns through everything |

### 3.3 Critical Implementation Note

For `UGE_IncendiaryEffect_ArmorBypass`, the SetByCaller values must be **NEGATIVE**:

```cpp
// CORRECT - Apply burning damage
Spec.Data->SetSetByCallerMagnitude(
    FGameplayTag::RequestGameplayTag(FName("Data.Damage.Burn.Armor")),
    -5.0f);  // NEGATIVE: reduces armor

Spec.Data->SetSetByCallerMagnitude(
    FGameplayTag::RequestGameplayTag(FName("Data.Damage.Burn.Health")),
    -5.0f);  // NEGATIVE: reduces health directly
```

### 3.4 Source Files

```
Source/GAS/Public/SuspenseCore/Effects/Grenade/GE_IncendiaryEffect.h
Source/GAS/Private/SuspenseCore/Effects/Grenade/GE_IncendiaryEffect.cpp
```

---

## 4. GAS Integration

### 4.1 Required GameplayTags

Add to `SuspenseCoreGameplayTags.h` or GameplayTags ini:

```cpp
// DoT Data Tags
Data.Damage.Bleed              // Bleed damage per tick
Data.Damage.Burn               // Standard burn damage
Data.Damage.Burn.Armor         // Armor bypass - armor damage
Data.Damage.Burn.Health        // Armor bypass - health damage
Data.Grenade.BurnDuration      // Burn effect duration

// State Tags
State.Health.Bleeding.Light    // Light bleed indicator
State.Health.Bleeding.Heavy    // Heavy bleed indicator
State.Burning                  // Burning indicator

// Effect Tags
Effect.Damage.Bleed            // Bleed effect category
Effect.Damage.Bleed.Light      // Light bleed specific
Effect.Damage.Bleed.Heavy      // Heavy bleed specific
Effect.Damage.Burn             // Burn effect category
Effect.DoT                     // All DoT effects
Effect.Grenade.Shrapnel        // Shrapnel damage source
Effect.Grenade.Incendiary      // Incendiary damage source
Effect.Grenade.Incendiary.ArmorBypass  // Armor bypass variant
```

### 4.2 Attribute Requirements

From `USuspenseCoreAttributeSet`:

```cpp
// Required attributes for DoT effects
UPROPERTY(BlueprintReadOnly, Category = "Health")
FGameplayAttributeData Health;          // Burn.Health modifies directly

UPROPERTY(BlueprintReadOnly, Category = "Combat")
FGameplayAttributeData Armor;           // Burn.Armor modifies directly

UPROPERTY(BlueprintReadOnly, Category = "Meta")
FGameplayAttributeData IncomingDamage;  // Standard bleed uses this
```

---

## 5. Application Logic

### 5.1 Bleeding Application (GrenadeProjectile)

```cpp
void ASuspenseCoreGrenadeProjectile::ApplyExplosionDamage()
{
    // ... overlap query ...

    for (AActor* Target : OverlappedActors)
    {
        UAbilitySystemComponent* TargetASC =
            UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
        if (!TargetASC) continue;

        // Check if shrapnel can penetrate
        if (GrenadeType == ESuspenseCoreGrenadeType::Fragmentation)
        {
            // Get target armor
            float TargetArmor = TargetASC->GetNumericAttribute(
                USuspenseCoreAttributeSet::GetArmorAttribute());

            // CRITICAL: Bleeding only applies when armor = 0
            if (TargetArmor <= 0.0f)
            {
                // Determine bleed severity based on fragment hits
                int32 FragmentHits = CalculateFragmentHits(Target, Distance);
                TSubclassOf<UGameplayEffect> BleedClass =
                    (FragmentHits >= 5) ? UGE_BleedingEffect_Heavy::StaticClass()
                                        : UGE_BleedingEffect_Light::StaticClass();

                // Apply bleeding
                FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
                Context.AddSourceObject(this);
                FGameplayEffectSpecHandle BleedSpec =
                    TargetASC->MakeOutgoingSpec(BleedClass, 1.0f, Context);

                if (BleedSpec.IsValid())
                {
                    float DamagePerTick = (FragmentHits >= 5) ? 4.0f : 1.5f;
                    BleedSpec.Data->SetSetByCallerMagnitude(
                        FGameplayTag::RequestGameplayTag(FName("Data.Damage.Bleed")),
                        DamagePerTick);

                    TargetASC->ApplyGameplayEffectSpecToSelf(*BleedSpec.Data.Get());
                }
            }
        }
    }
}
```

### 5.2 Burning Application (GrenadeProjectile)

```cpp
void ASuspenseCoreGrenadeProjectile::ApplyExplosionDamage()
{
    // ... overlap query ...

    for (AActor* Target : OverlappedActors)
    {
        UAbilitySystemComponent* TargetASC =
            UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
        if (!TargetASC) continue;

        if (GrenadeType == ESuspenseCoreGrenadeType::Incendiary)
        {
            // Use ArmorBypass variant for fire damage
            FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
            Context.AddSourceObject(this);
            FGameplayEffectSpecHandle BurnSpec = TargetASC->MakeOutgoingSpec(
                UGE_IncendiaryEffect_ArmorBypass::StaticClass(), 1.0f, Context);

            if (BurnSpec.IsValid())
            {
                // CRITICAL: Pass NEGATIVE values for direct attribute reduction
                BurnSpec.Data->SetSetByCallerMagnitude(
                    FGameplayTag::RequestGameplayTag(FName("Data.Damage.Burn.Armor")),
                    -5.0f);  // Reduces armor by 5 per tick

                BurnSpec.Data->SetSetByCallerMagnitude(
                    FGameplayTag::RequestGameplayTag(FName("Data.Damage.Burn.Health")),
                    -5.0f);  // Reduces health by 5 per tick

                BurnSpec.Data->SetSetByCallerMagnitude(
                    FGameplayTag::RequestGameplayTag(FName("Data.Grenade.BurnDuration")),
                    IncendiaryDuration);  // 5-10 seconds

                TargetASC->ApplyGameplayEffectSpecToSelf(*BurnSpec.Data.Get());
            }
        }
    }
}
```

---

## 6. Removal / Healing

### 6.1 Bleeding Removal

Bleeding effects are removed when medical items with appropriate capabilities are used:

```cpp
void USuspenseCoreMedicalHandler::ApplyMedicalEffect(AActor* Target,
    const FSuspenseCoreConsumableAttributeRow& MedicalData)
{
    UAbilitySystemComponent* TargetASC =
        UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
    if (!TargetASC) return;

    // Remove light bleeding
    if (MedicalData.bCanHealLightBleed)
    {
        FGameplayTagContainer BleedTags;
        BleedTags.AddTag(FGameplayTag::RequestGameplayTag(
            FName("State.Health.Bleeding.Light")));
        TargetASC->RemoveActiveEffectsWithGrantedTags(BleedTags);
    }

    // Remove heavy bleeding (requires surgery/medkit)
    if (MedicalData.bCanHealHeavyBleed)
    {
        FGameplayTagContainer HeavyBleedTags;
        HeavyBleedTags.AddTag(FGameplayTag::RequestGameplayTag(
            FName("State.Health.Bleeding.Heavy")));
        TargetASC->RemoveActiveEffectsWithGrantedTags(HeavyBleedTags);
    }
}
```

### 6.2 Burning Removal

Burning has a fixed duration and will expire naturally. However, it can be removed early:

```cpp
// Remove burning effect (e.g., water, fire extinguisher)
void ExtinguishBurning(UAbilitySystemComponent* TargetASC)
{
    FGameplayTagContainer BurnTags;
    BurnTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Burning")));
    TargetASC->RemoveActiveEffectsWithGrantedTags(BurnTags);
}
```

### 6.3 Medical Item Capabilities

| Item | Light Bleed | Heavy Bleed | Fire |
|------|-------------|-------------|------|
| Bandage | ✓ | ✗ | ✗ |
| IFAK | ✓ | ✓ | ✗ |
| Salewa | ✓ | ✓ | ✗ |
| Grizzly | ✓ | ✓ | ✗ |
| Water | ✗ | ✗ | ✓ |

---

## 7. UI Requirements

### 7.1 Status Indicators

```
┌─────────────────────────────────────────────────────────────────┐
│                      HEALTH BAR UI                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  [████████████████░░░░░░░░░]  HP: 75/100                       │
│                                                                 │
│  STATUS ICONS:                                                  │
│  ┌───┐ ┌───┐                                                   │
│  │🩸│ │🔥│  ← Active DoT indicators                           │
│  └───┘ └───┘                                                   │
│  Light  Burn                                                    │
│  Bleed  (5s)                                                    │
│                                                                 │
│  • Red pulse effect on screen for bleeding                      │
│  • Orange vignette for burning                                  │
│  • Countdown timer for burning duration                         │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 7.2 GameplayCues

| Cue Tag | Purpose | Assets |
|---------|---------|--------|
| `GameplayCue.Grenade.Bleed.Light` | Light blood screen effect | Blood_Light_PP, Heartbeat_Fast |
| `GameplayCue.Grenade.Bleed.Heavy` | Heavy blood screen + trail | Blood_Heavy_PP, BloodTrail_NS |
| `GameplayCue.Grenade.Burn` | Fire VFX on character | CharacterFire_NS, BurnScream_SFX |

---

## 8. Balance Reference

### 8.1 Damage Comparison

| Effect | DPS | Duration | Total Damage |
|--------|-----|----------|--------------|
| Light Bleed | 1.5 HP/s | Infinite | ∞ (until healed) |
| Heavy Bleed | 4 HP/s | Infinite | ∞ (until healed) |
| Burning (Standard) | 10-20 HP/s | 5-10s | 50-200 HP |
| Burning (ArmorBypass) | 10 HP/s (5+5) | 5-10s | 50-100 HP + Armor |

### 8.2 Tactical Considerations

**Bleeding:**
- Creates long-term attrition if not treated
- Forces players to carry medical supplies
- Encourages armor usage (prevents bleed when armor > 0)
- Punishes players who lose armor in combat

**Burning:**
- Immediate high damage threat
- Counters armor-stacking strategies
- Fixed duration provides window to escape
- Encourages movement (don't stand in fire)

### 8.3 Grenade Type Summary

| Grenade | Instant Damage | DoT Type | DoT Condition |
|---------|----------------|----------|---------------|
| Fragmentation | High (explosion + shrapnel) | Bleeding | Armor = 0 |
| Incendiary | Low (initial) | Burning | Always (bypasses armor) |
| Smoke | None | None | N/A |
| Flashbang | Low | None | N/A |
| Impact | High (explosion) | None | N/A |

---

## Appendix A: SetByCaller Tag Reference

| Tag | Used By | Value Type | Notes |
|-----|---------|------------|-------|
| `Data.Damage.Bleed` | GE_BleedingEffect_* | POSITIVE float | Damage per tick |
| `Data.Damage.Burn` | GE_IncendiaryEffect | POSITIVE float | Goes through armor calc |
| `Data.Damage.Burn.Armor` | GE_IncendiaryEffect_ArmorBypass | NEGATIVE float | Direct armor reduction |
| `Data.Damage.Burn.Health` | GE_IncendiaryEffect_ArmorBypass | NEGATIVE float | Direct health reduction |
| `Data.Grenade.BurnDuration` | GE_IncendiaryEffect_* | POSITIVE float | Duration in seconds |

---

## Appendix B: File Reference

### Core Implementation

| File | Purpose |
|------|---------|
| `GE_BleedingEffect.h/cpp` | Bleeding DoT effects (Light/Heavy) |
| `GE_IncendiaryEffect.h/cpp` | Burning DoT effects (Standard/Zone/ArmorBypass) |
| `SuspenseCoreGrenadeProjectile.h/cpp` | DoT application logic (ApplyDoTEffects, ApplyBleedingEffect, ApplyBurningEffect) |
| `SuspenseCoreMedicalHandler.cpp` | Bleeding removal |
| `SuspenseCoreThrowableAttributeRow` | SSOT for incendiary damage/duration |
| `SuspenseCoreConsumableAttributeRow` | Medical item bleed healing capabilities |

### Service Layer (NEW)

| File | Purpose |
|------|---------|
| `SuspenseCoreDoTService.h/cpp` | Central DoT tracking service with EventBus integration |
| `SuspenseCoreGameplayTags.h/cpp` | Native GameplayTags for DoT (State::Health, Event::DoT, Data::DoT) |

### UI Widgets (Planned)

| File | Purpose |
|------|---------|
| `W_DebuffIcon.h/cpp` | Individual debuff icon widget |
| `W_DebuffContainer.h/cpp` | Container with EventBus subscription for procedural icon loading |

---

## Appendix C: DoTService Integration

### EventBus Events Published

| Event Tag | Payload | When Published |
|-----------|---------|----------------|
| `SuspenseCore.Event.DoT.Applied` | FSuspenseCoreDoTEventPayload | When DoT effect first applied |
| `SuspenseCore.Event.DoT.Tick` | FSuspenseCoreDoTEventPayload + DamageDealt | Each damage tick |
| `SuspenseCore.Event.DoT.Expired` | FSuspenseCoreDoTEventPayload | When timed DoT expires naturally |
| `SuspenseCore.Event.DoT.Removed` | FSuspenseCoreDoTEventPayload | When DoT healed/removed early |

### Service Query API

```cpp
// Get DoT Service instance
USuspenseCoreDoTService* Service = USuspenseCoreDoTService::Get(WorldContextObject);

// Query active DoTs
TArray<FSuspenseCoreActiveDoT> DoTs = Service->GetActiveDoTs(TargetActor);
bool bBleeding = Service->HasActiveBleeding(TargetActor);
bool bBurning = Service->HasActiveBurning(TargetActor);
float BleedDPS = Service->GetBleedDamagePerSecond(TargetActor);
float BurnRemaining = Service->GetBurnTimeRemaining(TargetActor);
```

---

**Document End**

*For questions or updates, refer to the SuspenseCore development team.*
