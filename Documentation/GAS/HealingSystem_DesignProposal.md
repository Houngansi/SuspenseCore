# Healing System with Heal-over-Time (HoT) - Design Proposal

> **Version:** 1.0
> **Date:** 2026-01-22
> **Author:** Claude Code
> **Module:** GAS (GameplayAbilitySystem)
> **Status:** PROPOSAL (Pending Implementation)

---

## Table of Contents

1. [Overview](#1-overview)
2. [Healing Mechanics](#2-healing-mechanics)
3. [GAS Effect Design](#3-gas-effect-design)
4. [SSOT Integration](#4-ssot-integration)
5. [Implementation Guide](#5-implementation-guide)
6. [Balance Reference](#6-balance-reference)

---

## 1. Overview

### Design Philosophy

Following the established DoT (Damage-over-Time) pattern for grenades, this proposal outlines a complementary HoT (Heal-over-Time) system for medical items. This creates symmetry in the game mechanics:

| System | Effect Type | Duration | Application |
|--------|-------------|----------|-------------|
| **DoT (Grenades)** | Damage over time | Fixed/Infinite | Combat debuff |
| **HoT (Medical)** | Healing over time | Fixed | Recovery buff |

### Core Principle

```
INSTANT HEAL:  One-time health restoration (current system)
               → Simple, predictable, immediate feedback

HoT HEAL:      Gradual health restoration over time
               → Creates tactical decisions (safe to heal vs combat)
               → Encourages risk/reward gameplay
               → Interruptible by damage (optional)
```

### Use Cases

1. **Medkits (IFAK, Salewa)** - Instant heal + HoT regen after
2. **Stims (Propital)** - Pure HoT, no instant heal
3. **Regeneration Buff** - Low HoT from skills/perks

---

## 2. Healing Mechanics

### 2.1 Two-Phase Healing (Proposed)

Most medical items will use a two-phase healing model:

```
┌─────────────────────────────────────────────────────────────────┐
│                    MEDKIT HEALING FLOW                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Player Uses Medkit                                             │
│         │                                                       │
│         ▼                                                       │
│  ┌──────────────────┐                                          │
│  │  PHASE 1: INSTANT │  ← Immediate health boost               │
│  │  +30 HP          │                                          │
│  └────────┬─────────┘                                          │
│           │                                                     │
│           ▼                                                     │
│  ┌──────────────────┐                                          │
│  │  PHASE 2: HoT    │  ← Gradual recovery                      │
│  │  +50 HP over 10s │                                          │
│  │  (5 HP/sec)      │                                          │
│  └────────┬─────────┘                                          │
│           │                                                     │
│           ▼                                                     │
│  Total Healing: 80 HP                                           │
│  (30 instant + 50 over time)                                    │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 HoT Interruption (Optional Feature)

For tactical depth, HoT can be interrupted by taking damage:

```cpp
// Option A: Damage cancels HoT entirely
if (IncomingDamage > 0)
{
    RemoveActiveEffectsWithGrantedTags(Tag_State_Regenerating);
}

// Option B: Damage reduces remaining HoT
// More forgiving, still punishes taking hits
if (IncomingDamage > 0)
{
    float RemainingHeal = GetRemainingHoT();
    float PenaltyPercent = 0.5f;  // Lose 50% of remaining heal
    SetRemainingHoT(RemainingHeal * (1.0f - PenaltyPercent));
}

// Option C: HoT continues regardless (simplest)
// Fire-and-forget healing
```

### 2.3 Stacking Behavior

| Scenario | Behavior |
|----------|----------|
| Same item type twice | Refresh duration (don't stack heal rate) |
| Different item types | Stack HoT rates (up to cap) |
| Max HoT stacks | 2-3 depending on balance |
| HoT + Bleed | Both tick simultaneously |

---

## 3. GAS Effect Design

### 3.1 Effect Classes (Proposed)

```cpp
// ═══════════════════════════════════════════════════════════════════
// GE_HealOverTime - Base HoT effect
// ═══════════════════════════════════════════════════════════════════

UCLASS()
class GAS_API UGE_HealOverTime : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UGE_HealOverTime()
    {
        // Duration from SetByCaller
        DurationPolicy = EGameplayEffectDurationType::HasDuration;

        FSetByCallerFloat SetByCallerDuration;
        SetByCallerDuration.DataTag =
            FGameplayTag::RequestGameplayTag(FName("Data.Medical.HoTDuration"));
        DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDuration);

        // Tick every 1 second
        Period = 1.0f;
        bExecutePeriodicEffectOnApplication = true;

        // Heal modifier using IncomingHealing meta-attribute
        FGameplayModifierInfo HealModifier;
        HealModifier.Attribute =
            USuspenseCoreAttributeSet::GetIncomingHealingAttribute();
        HealModifier.ModifierOp = EGameplayModOp::Additive;

        FSetByCallerFloat SetByCallerHeal;
        SetByCallerHeal.DataTag =
            FGameplayTag::RequestGameplayTag(FName("Data.Medical.HealPerTick"));
        HealModifier.ModifierMagnitude =
            FGameplayEffectModifierMagnitude(SetByCallerHeal);

        Modifiers.Add(HealModifier);

        // Stacking: refresh duration, don't stack rate
        StackingType = EGameplayEffectStackingType::AggregateByTarget;
        StackLimitCount = 1;
        StackDurationRefreshPolicy =
            EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;

        // Granted tag for UI
        InheritableOwnedTagsContainer.AddTag(
            FGameplayTag::RequestGameplayTag(FName("State.Health.Regenerating")));

        // Asset tags
        InheritableGameplayEffectTags.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Effect.Healing")));
        InheritableGameplayEffectTags.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Effect.HoT")));
    }
};

// ═══════════════════════════════════════════════════════════════════
// GE_HealOverTime_Propital - Stim-specific HoT (stackable)
// ═══════════════════════════════════════════════════════════════════

UCLASS()
class GAS_API UGE_HealOverTime_Propital : public UGameplayEffect
{
    // Similar but:
    // - Higher heal rate (15 HP/tick)
    // - Shorter duration (20s)
    // - Grants State.Buff.Stimmed tag
    // - Stacking: AggregateBySource, limit 2
};

// ═══════════════════════════════════════════════════════════════════
// GE_InstantHeal - Instant portion of medkit
// ═══════════════════════════════════════════════════════════════════

UCLASS()
class GAS_API UGE_InstantHeal : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UGE_InstantHeal()
    {
        DurationPolicy = EGameplayEffectDurationType::Instant;

        FGameplayModifierInfo HealModifier;
        HealModifier.Attribute =
            USuspenseCoreAttributeSet::GetIncomingHealingAttribute();
        HealModifier.ModifierOp = EGameplayModOp::Additive;

        FSetByCallerFloat SetByCallerHeal;
        SetByCallerHeal.DataTag =
            FGameplayTag::RequestGameplayTag(FName("Data.Medical.InstantHeal"));
        HealModifier.ModifierMagnitude =
            FGameplayEffectModifierMagnitude(SetByCallerHeal);

        Modifiers.Add(HealModifier);
    }
};
```

### 3.2 SetByCaller Tags

| Tag | Effect | Value Type | Example |
|-----|--------|------------|---------|
| `Data.Medical.InstantHeal` | GE_InstantHeal | Positive HP | 30.0f |
| `Data.Medical.HealPerTick` | GE_HealOverTime | Positive HP/tick | 5.0f |
| `Data.Medical.HoTDuration` | GE_HealOverTime | Duration (sec) | 10.0f |

### 3.3 State Tags

| Tag | Meaning |
|-----|---------|
| `State.Health.Regenerating` | HoT is active |
| `State.Buff.Stimmed` | Stim HoT active (Propital) |
| `State.Medical.Healing` | During medkit use (animation) |

---

## 4. SSOT Integration

### 4.1 Extended ConsumableAttributeRow

```cpp
USTRUCT(BlueprintType)
struct FSuspenseCoreConsumableAttributeRow : public FTableRowBase
{
    GENERATED_BODY()

    // === EXISTING FIELDS ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ConsumableID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HealAmount;  // Renamed: Total heal (instant + HoT)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float UseTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxUses;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCanHealLightBleed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCanHealHeavyBleed;

    // === NEW FIELDS FOR HoT ===

    /** Instant heal on use completion */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HoT")
    float InstantHealAmount = 0.0f;

    /** Heal per second during HoT phase */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HoT")
    float HoTHealPerSecond = 0.0f;

    /** Duration of HoT effect in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HoT")
    float HoTDuration = 0.0f;

    /** Whether damage interrupts HoT */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HoT")
    bool bHoTInterruptedByDamage = false;
};
```

### 4.2 Sample DataTable Entries

```json
[
  {
    "ConsumableID": "Medical_IFAK",
    "HealAmount": 150,
    "InstantHealAmount": 30,
    "HoTHealPerSecond": 5,
    "HoTDuration": 24,
    "UseTime": 5.0,
    "MaxUses": 3,
    "bCanHealLightBleed": true,
    "bCanHealHeavyBleed": true,
    "bHoTInterruptedByDamage": true
  },
  {
    "ConsumableID": "Medical_Salewa",
    "HealAmount": 400,
    "InstantHealAmount": 50,
    "HoTHealPerSecond": 8,
    "HoTDuration": 44,
    "UseTime": 4.0,
    "MaxUses": 5,
    "bCanHealLightBleed": true,
    "bCanHealHeavyBleed": true,
    "bHoTInterruptedByDamage": true
  },
  {
    "ConsumableID": "Medical_Propital",
    "HealAmount": 300,
    "InstantHealAmount": 0,
    "HoTHealPerSecond": 15,
    "HoTDuration": 20,
    "UseTime": 2.5,
    "MaxUses": 1,
    "bCanHealLightBleed": false,
    "bCanHealHeavyBleed": false,
    "bHoTInterruptedByDamage": false
  }
]
```

### 4.3 Calculation Note

```
Total Heal = InstantHealAmount + (HoTHealPerSecond × HoTDuration)

IFAK:    30 + (5 × 24) = 30 + 120 = 150 HP total
Salewa:  50 + (8 × 44) = 50 + 352 ≈ 400 HP total
Propital: 0 + (15 × 20) = 0 + 300 = 300 HP total
```

---

## 5. Implementation Guide

### 5.1 MedicalHandler Changes

```cpp
void USuspenseCoreMedicalHandler::ApplyMedicalEffect(
    AActor* Target,
    const FSuspenseCoreConsumableAttributeRow& MedicalData)
{
    UAbilitySystemComponent* TargetASC =
        UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
    if (!TargetASC) return;

    FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
    Context.AddSourceObject(this);

    // ═══════════════════════════════════════════════════════════════════
    // PHASE 1: Instant Heal
    // ═══════════════════════════════════════════════════════════════════
    if (MedicalData.InstantHealAmount > 0.0f)
    {
        FGameplayEffectSpecHandle InstantSpec = TargetASC->MakeOutgoingSpec(
            UGE_InstantHeal::StaticClass(), 1.0f, Context);

        if (InstantSpec.IsValid())
        {
            InstantSpec.Data->SetSetByCallerMagnitude(
                FGameplayTag::RequestGameplayTag(FName("Data.Medical.InstantHeal")),
                MedicalData.InstantHealAmount);

            TargetASC->ApplyGameplayEffectSpecToSelf(*InstantSpec.Data.Get());
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // PHASE 2: Heal over Time
    // ═══════════════════════════════════════════════════════════════════
    if (MedicalData.HoTHealPerSecond > 0.0f && MedicalData.HoTDuration > 0.0f)
    {
        FGameplayEffectSpecHandle HoTSpec = TargetASC->MakeOutgoingSpec(
            UGE_HealOverTime::StaticClass(), 1.0f, Context);

        if (HoTSpec.IsValid())
        {
            HoTSpec.Data->SetSetByCallerMagnitude(
                FGameplayTag::RequestGameplayTag(FName("Data.Medical.HealPerTick")),
                MedicalData.HoTHealPerSecond);

            HoTSpec.Data->SetSetByCallerMagnitude(
                FGameplayTag::RequestGameplayTag(FName("Data.Medical.HoTDuration")),
                MedicalData.HoTDuration);

            TargetASC->ApplyGameplayEffectSpecToSelf(*HoTSpec.Data.Get());
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // Bleed Removal (existing code)
    // ═══════════════════════════════════════════════════════════════════
    if (MedicalData.bCanHealLightBleed)
    {
        // ... remove light bleed ...
    }
    if (MedicalData.bCanHealHeavyBleed)
    {
        // ... remove heavy bleed ...
    }
}
```

### 5.2 AttributeSet Changes

Add handling for `IncomingHealing` in `PostGameplayEffectExecute`:

```cpp
void USuspenseCoreAttributeSet::PostGameplayEffectExecute(
    const FGameplayEffectModCallbackData& Data)
{
    // ... existing IncomingDamage handling ...

    // Handle IncomingHealing
    if (Data.EvaluatedData.Attribute == GetIncomingHealingAttribute())
    {
        float HealAmount = GetIncomingHealing();
        if (HealAmount > 0.0f)
        {
            float CurrentHealth = GetHealth();
            float MaxHealthVal = GetMaxHealth();

            // Apply healing (clamped to max)
            float NewHealth = FMath::Clamp(
                CurrentHealth + HealAmount,
                0.0f,
                MaxHealthVal);

            SetHealth(NewHealth);

            // Publish heal event
            BroadcastAttributeChange(
                GetHealthAttribute(),
                CurrentHealth,
                NewHealth);
        }

        // Reset meta-attribute
        SetIncomingHealing(0.0f);
    }
}
```

### 5.3 HoT Interruption (Optional)

If `bHoTInterruptedByDamage` is enabled, add to damage handling:

```cpp
void USuspenseCoreAttributeSet::PostGameplayEffectExecute(
    const FGameplayEffectModCallbackData& Data)
{
    // After processing IncomingDamage
    if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
    {
        // ... damage application ...

        // Check if we should interrupt HoT
        if (DamageAfterArmor > 0.0f)
        {
            // Get ASC
            if (UAbilitySystemComponent* ASC = GetSuspenseCoreASC())
            {
                // Check if any active HoT effects have interrupt flag
                // This requires custom logic or effect tags
                FGameplayTagContainer HoTTags;
                HoTTags.AddTag(FGameplayTag::RequestGameplayTag(
                    FName("Effect.HoT.Interruptible")));

                // Remove interruptible HoT effects
                ASC->RemoveActiveEffectsWithTags(HoTTags);
            }
        }
    }
}
```

---

## 6. Balance Reference

### 6.1 Medical Item Comparison

| Item | Instant | HoT/sec | Duration | Total | Use Time | Interrupt |
|------|---------|---------|----------|-------|----------|-----------|
| Bandage | 0 | 0 | 0 | 0 | 2.0s | N/A |
| IFAK | 30 | 5 | 24s | 150 | 5.0s | Yes |
| Salewa | 50 | 8 | 44s | 400 | 4.0s | Yes |
| Car Medkit | 25 | 4 | 50s | 220 | 6.0s | Yes |
| Grizzly | 100 | 10 | 170s | 1800 | 7.0s | No |
| Propital | 0 | 15 | 20s | 300 | 2.5s | No |

### 6.2 Tactical Considerations

**Instant + HoT (IFAK, Salewa):**
- Immediate survivability boost
- Must stay safe for full benefit
- Punishes aggressive play during HoT

**Pure HoT (Propital):**
- Best for pre-fight preparation
- Use before engagement
- Not interrupted by damage

**High Instant (Grizzly):**
- Emergency healing in combat
- Expensive and heavy
- Intended for rare use

### 6.3 HoT vs DoT Interaction

When player has both Bleeding and Regenerating:

```
Example: Light Bleed (2 HP/tick) vs HoT (5 HP/tick)
Net result: +3 HP/tick

Example: Heavy Bleed (5 HP/tick) vs HoT (5 HP/tick)
Net result: 0 HP/tick (stable but not recovering)

Example: Burning (10 HP/tick) vs HoT (5 HP/tick)
Net result: -5 HP/tick (still dying, just slower)
```

---

## Appendix A: Required GameplayTags

```ini
; === Data Tags ===
+GameplayTags=(Tag="Data.Medical.InstantHeal",DevComment="Instant heal amount")
+GameplayTags=(Tag="Data.Medical.HealPerTick",DevComment="HoT heal per second")
+GameplayTags=(Tag="Data.Medical.HoTDuration",DevComment="HoT duration in seconds")

; === State Tags ===
+GameplayTags=(Tag="State.Health.Regenerating",DevComment="HoT is active")
+GameplayTags=(Tag="State.Buff.Stimmed",DevComment="Stim HoT active")
+GameplayTags=(Tag="State.Medical.Healing",DevComment="Using medical item")

; === Effect Tags ===
+GameplayTags=(Tag="Effect.Healing",DevComment="Any healing effect")
+GameplayTags=(Tag="Effect.HoT",DevComment="Heal over time effect")
+GameplayTags=(Tag="Effect.HoT.Interruptible",DevComment="HoT that damage cancels")
```

---

## Appendix B: Proposed File Structure

```
Source/GAS/
├── Public/SuspenseCore/Effects/Medical/
│   ├── GE_InstantHeal.h
│   ├── GE_HealOverTime.h
│   └── GE_HealOverTime_Propital.h
│
└── Private/SuspenseCore/Effects/Medical/
    ├── GE_InstantHeal.cpp
    ├── GE_HealOverTime.cpp
    └── GE_HealOverTime_Propital.cpp
```

---

## Appendix C: Implementation Checklist

- [ ] Add new GameplayTags to DefaultGameplayTags.ini
- [ ] Extend FSuspenseCoreConsumableAttributeRow with HoT fields
- [ ] Create GE_InstantHeal GameplayEffect
- [ ] Create GE_HealOverTime GameplayEffect
- [ ] Create GE_HealOverTime_Propital GameplayEffect (optional)
- [ ] Update SuspenseCoreAttributeSet::PostGameplayEffectExecute for IncomingHealing
- [ ] Update MedicalHandler to apply two-phase healing
- [ ] Add HoT interruption logic (optional)
- [ ] Update ConsumableAttributes DataTable with new fields
- [ ] Create UI indicator for regeneration state
- [ ] Create GameplayCue for HoT visual effect

---

**Document End**

*This is a design proposal. Await approval before implementation.*
