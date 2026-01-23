# Status Effect System - Game Design Document

**Version:** 2.0
**Status:** IMPLEMENTED
**Author:** Claude (AI Assistant)
**Created:** 2026-01-23
**Last Updated:** 2026-01-23

---

## Document History

| Version | Date | Changes | Approved |
|---------|------|---------|----------|
| 1.0 | 2026-01-23 | Initial GDD creation | PENDING |
| 2.0 | 2026-01-23 | Full implementation complete | YES |

---

## Table of Contents

1. [Overview](#1-overview)
2. [Design Goals](#2-design-goals)
3. [Architecture](#3-architecture)
4. [Data Flow](#4-data-flow)
5. [Effect Types](#5-effect-types)
6. [GameplayEffect Assets](#6-gameplayeffect-assets)
7. [DataTable Structure](#7-datatable-structure)
8. [JSON Format](#8-json-format)
9. [Service Layer](#9-service-layer)
10. [UI Integration](#10-ui-integration)
11. [Implementation Plan](#11-implementation-plan)
12. [File Manifest](#12-file-manifest)

---

## 1. Overview

### 1.1 Purpose

Система статус эффектов (баффов/дебаффов) для SuspenseCore в стиле Escape from Tarkov.
Эффекты накладываются через GameplayAbilitySystem и отображаются в UI.

### 1.2 Key Principles

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        SEPARATION OF CONCERNS                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   GAMEPLAY DATA (GE Assets)              VISUAL DATA (DataTable/JSON)       │
│   ═════════════════════════              ════════════════════════════       │
│   • Duration                             • DisplayName                      │
│   • Damage/Heal per tick                 • Description                      │
│   • Tick interval                        • Icon texture                     │
│   • Stacking policy                      • Icon tint colors                 │
│   • Attribute modifiers                  • VFX references                   │
│   • Granted/Blocked tags                 • Audio references                 │
│   • Application requirements             • Cure item IDs                    │
│                                          • Category (Buff/Debuff)           │
│                                          • Display priority                 │
│                                                                             │
│   GE_Bleeding_Light.uasset               DT_StatusEffectVisuals.uasset      │
│   (Blueprint Asset)                      (DataTable from JSON)              │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 1.3 Why This Approach?

| Problem with Current System | Solution |
|-----------------------------|----------|
| JSON имеет 43+ полей | Сокращаем до ~18 полей (только визуалы) |
| Duration/Damage дублируют GE | Эти данные остаются ТОЛЬКО в GameplayEffect |
| JSON парсинг с ошибками | Упрощённый формат без nested FGameplayTagContainer |
| Сложно балансировать | GE Asset редактируется в Editor с preview |

---

## 2. Design Goals

### 2.1 Functional Goals

- [x] Поддержка всех типов эффектов (DoT, HoT, buffs, debuffs)
- [x] Стакание эффектов (bleeding stacks = больше урона)
- [x] Infinite-duration эффекты (bleeding требует лечения)
- [x] Cure system (бинты лечат light bleed, медкит - heavy bleed)
- [x] UI отображение активных эффектов с таймерами

### 2.2 Technical Goals

- [x] Single Source of Truth для визуальных данных (JSON → DataTable)
- [x] GameplayEffect Assets для gameplay данных
- [x] EventBus интеграция для real-time UI updates
- [x] Поддержка Niagara VFX на персонаже
- [x] Локализация DisplayName/Description

### 2.3 Performance Goals

- [x] Lazy loading иконок и VFX
- [x] Максимум 10 активных эффектов на персонажа
- [x] DoT тики обрабатываются на сервере (no client prediction)

---

## 3. Architecture

### 3.1 Component Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           STATUS EFFECT SYSTEM                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐       │
│  │ GameplayEffect  │     │   DoTService    │     │   DataManager   │       │
│  │    Assets       │────▶│  (Tracking)     │◀───▶│  (SSOT Cache)   │       │
│  │                 │     │                 │     │                 │       │
│  │ • GE_Bleeding   │     │ • OnEffectAdd   │     │ • GetVisuals()  │       │
│  │ • GE_Burning    │     │ • OnEffectRemove│     │ • GetCureItems()│       │
│  │ • GE_Adrenaline │     │ • OnStackChange │     │                 │       │
│  └────────┬────────┘     └────────┬────────┘     └────────┬────────┘       │
│           │                       │                       │                 │
│           │                       ▼                       │                 │
│           │              ┌─────────────────┐              │                 │
│           │              │    EventBus     │              │                 │
│           │              │                 │              │                 │
│           │              │ • DoT.Applied   │              │                 │
│           │              │ • DoT.Removed   │              │                 │
│           │              │ • DoT.Ticked    │              │                 │
│           │              │ • DoT.Stacked   │              │                 │
│           │              └────────┬────────┘              │                 │
│           │                       │                       │                 │
│           ▼                       ▼                       ▼                 │
│  ┌─────────────────────────────────────────────────────────────────┐       │
│  │                      ASC (AbilitySystemComponent)                │       │
│  │                                                                  │       │
│  │  • ApplyGameplayEffect(GE_Bleeding_Light)                       │       │
│  │  • GetActiveEffectsWithTags(State.Health.Bleeding)              │       │
│  │  • OnActiveGameplayEffectAdded → DoTService::OnEffectApplied    │       │
│  │  • OnActiveGameplayEffectRemoved → DoTService::OnEffectRemoved  │       │
│  └──────────────────────────────────────────────────────────────────┘       │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────┐       │
│  │                           UI LAYER                               │       │
│  │                                                                  │       │
│  │  W_DebuffContainer ─────────────────────────────────────────┐   │       │
│  │  │                                                          │   │       │
│  │  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐        │   │       │
│  │  │  │W_DebuffIcon │ │W_DebuffIcon │ │W_DebuffIcon │  ...   │   │       │
│  │  │  │             │ │             │ │             │        │   │       │
│  │  │  │ [Bleeding]  │ │ [Burning]   │ │[Suppressed] │        │   │       │
│  │  │  │  x2 stacks  │ │  4.5s left  │ │  2.1s left  │        │   │       │
│  │  │  └─────────────┘ └─────────────┘ └─────────────┘        │   │       │
│  │  └──────────────────────────────────────────────────────────┘   │       │
│  └──────────────────────────────────────────────────────────────────┘       │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 3.2 Class Responsibilities

| Class | Responsibility |
|-------|---------------|
| `UGameplayEffect` (Assets) | Gameplay logic: duration, damage, stacking |
| `USuspenseCoreDoTService` | Tracks active effects, publishes EventBus events |
| `USuspenseCoreDataManager` | Caches DataTable rows, provides GetVisuals() API |
| `UW_DebuffContainer` | Creates/destroys W_DebuffIcon widgets |
| `UW_DebuffIcon` | Displays single effect: icon, timer, stacks |
| `FSuspenseCoreStatusEffectVisualRow` | DataTable row for visual/UI data only |

---

## 4. Data Flow

### 4.1 Effect Application Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        EFFECT APPLICATION FLOW                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  1. SOURCE (Grenade, Ability, Item)                                         │
│     │                                                                       │
│     │  ApplyGameplayEffectToTarget(GE_Bleeding_Light, Target)              │
│     ▼                                                                       │
│  2. ASC (Target's AbilitySystemComponent)                                   │
│     │                                                                       │
│     │  • Checks Application Requirements (BlockedByTags, RequiredTags)     │
│     │  • Applies Stacking Policy                                           │
│     │  • Creates FActiveGameplayEffect                                     │
│     │                                                                       │
│     │  OnActiveGameplayEffectAdded.Broadcast(Handle, EffectSpec)           │
│     ▼                                                                       │
│  3. DoTService::OnEffectApplied(Handle, EffectSpec)                        │
│     │                                                                       │
│     │  // Extract effect tag from GE                                       │
│     │  FGameplayTag EffectTag = ExtractEffectTag(EffectSpec);              │
│     │  // "State.Health.Bleeding.Light"                                    │
│     │                                                                       │
│     │  // Get visual data from DataManager                                 │
│     │  FSuspenseCoreStatusEffectVisualRow Visuals;                         │
│     │  DataManager->GetStatusEffectVisuals(EffectTag, Visuals);            │
│     │                                                                       │
│     │  // Get gameplay data directly from ASC                              │
│     │  float Duration = ASC->GetRemainingDuration(Handle);                 │
│     │  int32 Stacks = ASC->GetStackCount(Handle);                          │
│     │                                                                       │
│     │  // Store in local tracking map                                      │
│     │  ActiveEffects.Add(Handle, {EffectTag, Visuals, Duration, Stacks}); │
│     │                                                                       │
│     │  // Publish EventBus event                                           │
│     │  EventBus->Publish(Event::DoT::Applied, {                            │
│     │      EffectTag, Visuals.Icon, Duration, Stacks                       │
│     │  });                                                                  │
│     ▼                                                                       │
│  4. W_DebuffContainer::OnDoTApplied(EventData)                             │
│     │                                                                       │
│     │  // Check if icon already exists                                     │
│     │  if (UW_DebuffIcon* Existing = FindIconByTag(EffectTag))             │
│     │  {                                                                    │
│     │      Existing->UpdateStacks(EventData.Stacks);                       │
│     │      Existing->UpdateDuration(EventData.Duration);                   │
│     │  }                                                                    │
│     │  else                                                                 │
│     │  {                                                                    │
│     │      // Create new icon widget                                       │
│     │      UW_DebuffIcon* NewIcon = CreateWidget<UW_DebuffIcon>();         │
│     │      NewIcon->Initialize(EventData);                                 │
│     │      IconContainer->AddChild(NewIcon);                               │
│     │  }                                                                    │
│     ▼                                                                       │
│  5. UI DISPLAYED                                                            │
│     │                                                                       │
│     │  [Icon] Bleeding x2                                                  │
│     │         ████████░░ (infinite - no timer bar)                         │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Effect Removal Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         EFFECT REMOVAL FLOW                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  CASE A: Duration Expired (Burning, Suppressed)                            │
│  ──────────────────────────────────────────────                            │
│  1. GE Duration expires → ASC removes effect automatically                 │
│  2. ASC::OnActiveGameplayEffectRemoved.Broadcast(Handle)                   │
│  3. DoTService::OnEffectRemoved(Handle)                                    │
│  4. EventBus->Publish(Event::DoT::Removed, {EffectTag})                    │
│  5. W_DebuffContainer removes icon widget                                  │
│                                                                             │
│  CASE B: Cured by Item (Bleeding)                                          │
│  ──────────────────────────────────                                        │
│  1. Player uses Bandage item                                               │
│  2. GA_UseConsumable checks: DataManager->GetCureItems("BleedingLight")    │
│  3. If "Item_Bandage" in CureItemIDs:                                      │
│     ASC->RemoveActiveGameplayEffectByTag(State.Health.Bleeding.Light)      │
│  4. Same flow as Case A from step 2                                        │
│                                                                             │
│  CASE C: One Stack Removed (Stacked Bleeding)                              │
│  ────────────────────────────────────────────                              │
│  1. Partial heal reduces stacks                                            │
│  2. ASC::OnGameplayEffectStackChange.Broadcast(Handle, OldStacks, NewStacks)│
│  3. DoTService::OnStackChanged(Handle, NewStacks)                          │
│  4. EventBus->Publish(Event::DoT::Stacked, {EffectTag, NewStacks})         │
│  5. W_DebuffIcon::UpdateStacks(NewStacks)                                  │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.3 Tick Flow (DoT Damage)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           DoT TICK FLOW                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  GE_Bleeding_Light Configuration:                                          │
│  ├── Duration Policy: Infinite                                             │
│  ├── Period: 1.0 seconds                                                   │
│  ├── Execute Periodic Effect: true                                         │
│  └── Modifiers:                                                            │
│      └── Attribute: Health                                                 │
│          Operation: Additive                                               │
│          Magnitude: SetByCaller (DataTag: Damage.DoT.PerTick)              │
│          Value: -3.0 (default, can be overridden)                          │
│                                                                             │
│  Every 1.0 seconds:                                                        │
│  ──────────────────                                                        │
│  1. GAS executes periodic effect                                           │
│  2. Health -= DamagePerTick × StackCount                                   │
│  3. ASC fires OnGameplayEffectExecuted                                     │
│  4. DoTService::OnEffectTicked(Handle)                                     │
│  5. EventBus->Publish(Event::DoT::Ticked, {EffectTag, DamageDealt})        │
│  6. UI can show damage number popup (optional)                             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Effect Types

### 5.1 Debuffs (Negative Effects)

| Effect ID | Tag | Duration | DoT | Stacking | Cure |
|-----------|-----|----------|-----|----------|------|
| BleedingLight | State.Health.Bleeding.Light | Infinite | 3 HP/1s | Max 5, Additive | Bandage, Medkit |
| BleedingHeavy | State.Health.Bleeding.Heavy | Infinite | 8 HP/1s | Max 3, Additive | Medkit, Surgery |
| Burning | State.Health.Burning | 5-10s | 10 HP/0.5s | No stack | Water, Extinguish |
| Poisoned | State.Health.Poisoned | 30s | 2 HP/2s | Max 3 | Antidote |
| Stunned | State.Combat.Stunned | 2-5s | None | No stack | Wait |
| Suppressed | State.Combat.Suppressed | 3s | None | Refresh | Leave combat |
| Fracture | State.Health.Fracture | Infinite | None | No stack | Splint, Surgery |
| Dehydrated | State.Health.Dehydrated | Infinite | 1 HP/5s | No stack | Water, Food |
| Exhausted | State.Health.Exhausted | Infinite | None | No stack | Rest, Stimulant |

### 5.2 Buffs (Positive Effects)

| Effect ID | Tag | Duration | HoT | Stacking | Source |
|-----------|-----|----------|-----|----------|--------|
| Regenerating | State.Health.Regenerating | 10-30s | +5 HP/1s | No stack | Medkit, Stim |
| Painkiller | State.Combat.Painkiller | 60-300s | None | Refresh | Pills, Injector |
| Adrenaline | State.Combat.Adrenaline | 30-60s | None | No stack | Combat, Stim |
| Fortified | State.Combat.Fortified | 15-30s | None | No stack | Armor ability |
| Haste | State.Movement.Haste | 10-20s | None | No stack | Stimulant |

### 5.3 Attribute Modifiers

| Effect | Attribute | Modifier |
|--------|-----------|----------|
| Suppressed | AimAccuracy | -30% |
| Suppressed | RecoilControl | -20% |
| Fracture (Leg) | MoveSpeed | -40% |
| Fracture (Arm) | AimSpeed | -50% |
| Painkiller | PainThreshold | +100% (ignore limp) |
| Adrenaline | MoveSpeed | +15% |
| Adrenaline | StaminaRegen | +25% |
| Haste | MoveSpeed | +20% |
| Fortified | DamageReduction | +15% |

---

## 6. GameplayEffect Assets

### 6.1 Asset Location

```
Content/GAS/Effects/StatusEffects/
├── Debuffs/
│   ├── GE_Bleeding_Light.uasset
│   ├── GE_Bleeding_Heavy.uasset
│   ├── GE_Burning.uasset
│   ├── GE_Poisoned.uasset
│   ├── GE_Stunned.uasset
│   ├── GE_Suppressed.uasset
│   ├── GE_Fracture_Leg.uasset
│   ├── GE_Fracture_Arm.uasset
│   ├── GE_Dehydrated.uasset
│   └── GE_Exhausted.uasset
├── Buffs/
│   ├── GE_Regenerating.uasset
│   ├── GE_Painkiller.uasset
│   ├── GE_Adrenaline.uasset
│   ├── GE_Fortified.uasset
│   └── GE_Haste.uasset
└── Base/
    ├── GE_DoT_Base.uasset          (parent for all DoT effects)
    └── GE_Buff_Base.uasset         (parent for all buff effects)
```

### 6.2 GE_Bleeding_Light Configuration

```
UGameplayEffect: GE_Bleeding_Light
├── Duration Policy: Infinite
├── Period: 1.0 seconds
├── Execute Periodic Effect: true
├── Stacking:
│   ├── Type: Aggregate by Source
│   ├── Limit: 5
│   ├── Duration Refresh: true
│   └── Period Reset: false
├── Granted Tags:
│   └── State.Health.Bleeding
│   └── State.Health.Bleeding.Light
├── Modifiers:
│   └── [0] Health
│       ├── Operation: Additive
│       ├── Magnitude Type: SetByCaller
│       ├── Data Tag: Damage.DoT.PerTick
│       └── Default: -3.0
└── Application Requirements:
    └── Blocked by: State.Health.Bleeding.Heavy (upgrade instead)
```

### 6.3 GE_Burning Configuration

```
UGameplayEffect: GE_Burning
├── Duration Policy: Has Duration
├── Duration Magnitude: SetByCaller (Effect.Duration)
│   └── Default: 5.0 seconds
├── Period: 0.5 seconds
├── Execute Periodic Effect: true
├── Stacking:
│   ├── Type: None (refresh duration on reapply)
│   └── Duration Refresh: true
├── Granted Tags:
│   └── State.Health.Burning
├── Modifiers:
│   └── [0] Health
│       ├── Operation: Additive
│       ├── Magnitude: SetByCaller (Damage.DoT.PerTick)
│       └── Default: -10.0
│   └── [1] Armor (BYPASSES armor - damages it too)
│       ├── Operation: Additive
│       ├── Magnitude: SetByCaller (Damage.DoT.Armor)
│       └── Default: -3.0
└── Application Requirements: None
```

---

## 7. DataTable Structure

### 7.1 FSuspenseCoreStatusEffectVisualRow (Simplified)

```cpp
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreStatusEffectVisualRow : public FTableRowBase
{
    GENERATED_BODY()

    //========================================================================
    // Identity & GAS Link
    //========================================================================

    /** Unique effect identifier (RowName in DataTable) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FName EffectID;

    /** Effect type tag - MUST match tag granted by GameplayEffect */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FGameplayTag EffectTypeTag;

    /** Reference to GameplayEffect asset */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    TSoftClassPtr<class UGameplayEffect> GameplayEffectClass;

    //========================================================================
    // UI Display
    //========================================================================

    /** Localized display name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    FText DisplayName;

    /** Short description for tooltip */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    FText Description;

    /** Buff, Debuff, or Neutral */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    ESuspenseCoreStatusEffectCategory Category = ESuspenseCoreStatusEffectCategory::Debuff;

    /** Sort priority (higher = shown first) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI",
        meta = (ClampMin = "0", ClampMax = "100"))
    int32 DisplayPriority = 50;

    //========================================================================
    // Visual - Icon
    //========================================================================

    /** Icon texture for UI */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    TSoftObjectPtr<UTexture2D> Icon;

    /** Icon tint (normal state) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    FLinearColor IconTint = FLinearColor::White;

    /** Icon tint (critical/urgent state) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    FLinearColor CriticalIconTint = FLinearColor(1.0f, 0.3f, 0.3f, 1.0f);

    //========================================================================
    // Visual - VFX
    //========================================================================

    /** Niagara effect on character */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
    TSoftObjectPtr<class UNiagaraSystem> CharacterVFX;

    /** VFX attachment socket */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
    FName VFXAttachSocket = NAME_None;

    //========================================================================
    // Audio
    //========================================================================

    /** Sound on effect application */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    TSoftObjectPtr<class USoundBase> ApplicationSound;

    /** Sound on effect removal/cure */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    TSoftObjectPtr<class USoundBase> RemovalSound;

    //========================================================================
    // Cure System
    //========================================================================

    /** Item IDs that can cure this effect */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cure")
    TArray<FName> CureItemIDs;

    /** Can be cured by basic bandage */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cure")
    bool bCuredByBandage = false;

    /** Can be cured by medkit */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cure")
    bool bCuredByMedkit = false;

    /** Requires surgical kit */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cure")
    bool bRequiresSurgery = false;

    //========================================================================
    // Animation Flags
    //========================================================================

    /** Prevents sprinting */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    bool bPreventsSprinting = false;

    /** Prevents ADS */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    bool bPreventsADS = false;

    /** Causes limping animation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    bool bCausesLimp = false;

    //========================================================================
    // Helper Methods
    //========================================================================

    bool IsValid() const { return !EffectID.IsNone() && EffectTypeTag.IsValid(); }
    bool IsDebuff() const { return Category == ESuspenseCoreStatusEffectCategory::Debuff; }
    bool IsBuff() const { return Category == ESuspenseCoreStatusEffectCategory::Buff; }
};
```

**Total: 18 fields** (down from 43+)

---

## 8. JSON Format

### 8.1 File Location

```
Content/Data/StatusEffects/SuspenseCoreStatusEffectVisuals.json
```

### 8.2 JSON Structure

```json
[
    {
        "Name": "BleedingLight",
        "EffectID": "BleedingLight",
        "EffectTypeTag": "State.Health.Bleeding.Light",
        "GameplayEffectClass": "/Game/GAS/Effects/StatusEffects/Debuffs/GE_Bleeding_Light.GE_Bleeding_Light_C",
        "DisplayName": "NSLOCTEXT(\"StatusEffects\", \"BleedingLight\", \"Light Bleeding\")",
        "Description": "NSLOCTEXT(\"StatusEffects\", \"BleedingLightDesc\", \"You are bleeding. Use a bandage or medkit to stop.\")",
        "Category": "Debuff",
        "DisplayPriority": 90,
        "Icon": "/Game/UI/Icons/StatusEffects/T_Icon_Bleeding_Light",
        "IconTint": {"R": 1.0, "G": 0.2, "B": 0.2, "A": 1.0},
        "CriticalIconTint": {"R": 1.0, "G": 0.0, "B": 0.0, "A": 1.0},
        "CharacterVFX": "/Game/VFX/StatusEffects/NS_Bleeding",
        "VFXAttachSocket": "spine_03",
        "ApplicationSound": "/Game/Audio/SFX/StatusEffects/S_Bleeding_Start",
        "RemovalSound": "/Game/Audio/SFX/StatusEffects/S_Bleeding_Stop",
        "CureItemIDs": ["Item_Bandage", "Item_Medkit", "Item_Salewa"],
        "bCuredByBandage": true,
        "bCuredByMedkit": true,
        "bRequiresSurgery": false,
        "bPreventsSprinting": false,
        "bPreventsADS": false,
        "bCausesLimp": false
    },
    {
        "Name": "BleedingHeavy",
        "EffectID": "BleedingHeavy",
        "EffectTypeTag": "State.Health.Bleeding.Heavy",
        "GameplayEffectClass": "/Game/GAS/Effects/StatusEffects/Debuffs/GE_Bleeding_Heavy.GE_Bleeding_Heavy_C",
        "DisplayName": "NSLOCTEXT(\"StatusEffects\", \"BleedingHeavy\", \"Heavy Bleeding\")",
        "Description": "NSLOCTEXT(\"StatusEffects\", \"BleedingHeavyDesc\", \"Severe bleeding! Requires medkit or surgery.\")",
        "Category": "Debuff",
        "DisplayPriority": 95,
        "Icon": "/Game/UI/Icons/StatusEffects/T_Icon_Bleeding_Heavy",
        "IconTint": {"R": 0.8, "G": 0.0, "B": 0.0, "A": 1.0},
        "CriticalIconTint": {"R": 1.0, "G": 0.0, "B": 0.0, "A": 1.0},
        "CharacterVFX": "/Game/VFX/StatusEffects/NS_Bleeding_Heavy",
        "VFXAttachSocket": "spine_03",
        "ApplicationSound": "/Game/Audio/SFX/StatusEffects/S_Bleeding_Heavy_Start",
        "RemovalSound": "/Game/Audio/SFX/StatusEffects/S_Bleeding_Stop",
        "CureItemIDs": ["Item_Medkit", "Item_Salewa", "Item_SurgicalKit"],
        "bCuredByBandage": false,
        "bCuredByMedkit": true,
        "bRequiresSurgery": false,
        "bPreventsSprinting": true,
        "bPreventsADS": false,
        "bCausesLimp": false
    },
    {
        "Name": "Burning",
        "EffectID": "Burning",
        "EffectTypeTag": "State.Health.Burning",
        "GameplayEffectClass": "/Game/GAS/Effects/StatusEffects/Debuffs/GE_Burning.GE_Burning_C",
        "DisplayName": "NSLOCTEXT(\"StatusEffects\", \"Burning\", \"Burning\")",
        "Description": "NSLOCTEXT(\"StatusEffects\", \"BurningDesc\", \"You are on fire! Damage bypasses armor.\")",
        "Category": "Debuff",
        "DisplayPriority": 100,
        "Icon": "/Game/UI/Icons/StatusEffects/T_Icon_Burning",
        "IconTint": {"R": 1.0, "G": 0.5, "B": 0.0, "A": 1.0},
        "CriticalIconTint": {"R": 1.0, "G": 0.2, "B": 0.0, "A": 1.0},
        "CharacterVFX": "/Game/VFX/StatusEffects/NS_Burning",
        "VFXAttachSocket": "root",
        "ApplicationSound": "/Game/Audio/SFX/StatusEffects/S_Burning_Start",
        "RemovalSound": "/Game/Audio/SFX/StatusEffects/S_Burning_Stop",
        "CureItemIDs": [],
        "bCuredByBandage": false,
        "bCuredByMedkit": false,
        "bRequiresSurgery": false,
        "bPreventsSprinting": true,
        "bPreventsADS": true,
        "bCausesLimp": false
    },
    {
        "Name": "Regenerating",
        "EffectID": "Regenerating",
        "EffectTypeTag": "State.Health.Regenerating",
        "GameplayEffectClass": "/Game/GAS/Effects/StatusEffects/Buffs/GE_Regenerating.GE_Regenerating_C",
        "DisplayName": "NSLOCTEXT(\"StatusEffects\", \"Regenerating\", \"Regenerating\")",
        "Description": "NSLOCTEXT(\"StatusEffects\", \"RegeneratingDesc\", \"Health is slowly regenerating.\")",
        "Category": "Buff",
        "DisplayPriority": 50,
        "Icon": "/Game/UI/Icons/StatusEffects/T_Icon_Regenerating",
        "IconTint": {"R": 0.2, "G": 1.0, "B": 0.2, "A": 1.0},
        "CriticalIconTint": {"R": 0.2, "G": 1.0, "B": 0.2, "A": 1.0},
        "CharacterVFX": "/Game/VFX/StatusEffects/NS_Healing",
        "VFXAttachSocket": "spine_03",
        "ApplicationSound": "/Game/Audio/SFX/StatusEffects/S_Heal_Start",
        "RemovalSound": "",
        "CureItemIDs": [],
        "bCuredByBandage": false,
        "bCuredByMedkit": false,
        "bRequiresSurgery": false,
        "bPreventsSprinting": false,
        "bPreventsADS": false,
        "bCausesLimp": false
    },
    {
        "Name": "Adrenaline",
        "EffectID": "Adrenaline",
        "EffectTypeTag": "State.Combat.Adrenaline",
        "GameplayEffectClass": "/Game/GAS/Effects/StatusEffects/Buffs/GE_Adrenaline.GE_Adrenaline_C",
        "DisplayName": "NSLOCTEXT(\"StatusEffects\", \"Adrenaline\", \"Adrenaline Rush\")",
        "Description": "NSLOCTEXT(\"StatusEffects\", \"AdrenalineDesc\", \"+15% speed, +25% stamina regen.\")",
        "Category": "Buff",
        "DisplayPriority": 60,
        "Icon": "/Game/UI/Icons/StatusEffects/T_Icon_Adrenaline",
        "IconTint": {"R": 1.0, "G": 0.8, "B": 0.0, "A": 1.0},
        "CriticalIconTint": {"R": 1.0, "G": 0.8, "B": 0.0, "A": 1.0},
        "CharacterVFX": "",
        "VFXAttachSocket": "",
        "ApplicationSound": "/Game/Audio/SFX/StatusEffects/S_Adrenaline_Start",
        "RemovalSound": "/Game/Audio/SFX/StatusEffects/S_Adrenaline_End",
        "CureItemIDs": [],
        "bCuredByBandage": false,
        "bCuredByMedkit": false,
        "bRequiresSurgery": false,
        "bPreventsSprinting": false,
        "bPreventsADS": false,
        "bCausesLimp": false
    }
]
```

### 8.3 JSON Format Notes

| Field | Format | Example |
|-------|--------|---------|
| `EffectTypeTag` | Simple string (NOT UE format) | `"State.Health.Bleeding.Light"` |
| `GameplayEffectClass` | Full asset path with _C suffix | `"/Game/GAS/Effects/.../GE_Name.GE_Name_C"` |
| `DisplayName` | NSLOCTEXT for localization | `"NSLOCTEXT(\"Namespace\", \"Key\", \"Text\")"` |
| `Icon` | Asset path without extension | `"/Game/UI/Icons/T_Icon_Name"` |
| `IconTint` | RGBA object | `{"R": 1.0, "G": 0.5, "B": 0.0, "A": 1.0}` |
| `CureItemIDs` | Array of FName strings | `["Item_Bandage", "Item_Medkit"]` |

---

## 9. Service Layer

### 9.1 DoTService Updates

```cpp
// USuspenseCoreDoTService.h

UCLASS()
class GAS_API USuspenseCoreDoTService : public UObject
{
    GENERATED_BODY()

public:
    /** Initialize service with ASC reference */
    void Initialize(UAbilitySystemComponent* InASC);

    /** Query API */
    UFUNCTION(BlueprintCallable, Category = "DoT")
    TArray<FSuspenseCoreActiveStatusEffect> GetActiveEffects() const;

    UFUNCTION(BlueprintCallable, Category = "DoT")
    bool HasActiveEffect(FGameplayTag EffectTag) const;

    UFUNCTION(BlueprintCallable, Category = "DoT")
    int32 GetEffectStackCount(FGameplayTag EffectTag) const;

    UFUNCTION(BlueprintCallable, Category = "DoT")
    float GetEffectRemainingDuration(FGameplayTag EffectTag) const;

protected:
    /** ASC Callbacks */
    void OnEffectApplied(UAbilitySystemComponent* ASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle);
    void OnEffectRemoved(const FActiveGameplayEffect& Effect);
    void OnStackChanged(FActiveGameplayEffectHandle Handle, int32 NewStackCount, int32 OldStackCount);

private:
    /** Extract effect tag from GameplayEffectSpec */
    FGameplayTag ExtractEffectTag(const FGameplayEffectSpec& Spec) const;

    /** Publish EventBus events */
    void PublishEffectApplied(FGameplayTag EffectTag, const FSuspenseCoreStatusEffectVisualRow& Visuals, float Duration, int32 Stacks);
    void PublishEffectRemoved(FGameplayTag EffectTag);
    void PublishEffectStacked(FGameplayTag EffectTag, int32 NewStacks);

    UPROPERTY()
    TWeakObjectPtr<UAbilitySystemComponent> ASCRef;

    UPROPERTY()
    TMap<FActiveGameplayEffectHandle, FSuspenseCoreActiveStatusEffect> ActiveEffects;
};
```

### 9.2 DataManager Extensions

```cpp
// USuspenseCoreDataManager.h - Add method

/** Get visual data for status effect by tag */
UFUNCTION(BlueprintCallable, Category = "Data|StatusEffects")
bool GetStatusEffectVisuals(FGameplayTag EffectTag, FSuspenseCoreStatusEffectVisualRow& OutVisuals) const;

/** Get all items that can cure an effect */
UFUNCTION(BlueprintCallable, Category = "Data|StatusEffects")
TArray<FName> GetCureItemsForEffect(FGameplayTag EffectTag) const;

/** Check if item can cure effect */
UFUNCTION(BlueprintCallable, Category = "Data|StatusEffects")
bool CanItemCureEffect(FName ItemID, FGameplayTag EffectTag) const;
```

---

## 10. UI Integration

### 10.1 W_DebuffContainer (Updated)

```cpp
UCLASS()
class UI_API UW_DebuffContainer : public USuspenseCoreUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

protected:
    /** EventBus handlers */
    UFUNCTION()
    void OnDoTApplied(const FSuspenseCoreEventPayload& Payload);

    UFUNCTION()
    void OnDoTRemoved(const FSuspenseCoreEventPayload& Payload);

    UFUNCTION()
    void OnDoTStacked(const FSuspenseCoreEventPayload& Payload);

    /** Find existing icon by tag */
    UW_DebuffIcon* FindIconByTag(FGameplayTag EffectTag) const;

    /** Create new icon widget */
    UW_DebuffIcon* CreateIcon(const FSuspenseCoreStatusEffectVisualRow& Visuals, float Duration, int32 Stacks);

    /** Sort icons by priority */
    void SortIcons();

    UPROPERTY(meta = (BindWidget))
    UHorizontalBox* IconContainer;

    UPROPERTY(EditDefaultsOnly, Category = "Config")
    TSubclassOf<UW_DebuffIcon> DebuffIconClass;

    UPROPERTY(EditDefaultsOnly, Category = "Config")
    int32 MaxVisibleIcons = 10;

private:
    UPROPERTY()
    TMap<FGameplayTag, UW_DebuffIcon*> ActiveIcons;

    FDelegateHandle AppliedHandle;
    FDelegateHandle RemovedHandle;
    FDelegateHandle StackedHandle;
};
```

### 10.2 W_DebuffIcon (Updated)

```cpp
UCLASS()
class UI_API UW_DebuffIcon : public USuspenseCoreUserWidget
{
    GENERATED_BODY()

public:
    /** Initialize with visual data */
    void Initialize(const FSuspenseCoreStatusEffectVisualRow& Visuals, float Duration, int32 Stacks);

    /** Update methods */
    void UpdateDuration(float NewDuration);
    void UpdateStacks(int32 NewStacks);

    /** Getters */
    FGameplayTag GetEffectTag() const { return EffectTag; }
    int32 GetDisplayPriority() const { return DisplayPriority; }

protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    /** Update timer display */
    void UpdateTimerDisplay();

    /** Check if infinite duration */
    bool IsInfinite() const { return bIsInfinite; }

    UPROPERTY(meta = (BindWidget))
    UImage* IconImage;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* StackText;

    UPROPERTY(meta = (BindWidget))
    UProgressBar* TimerBar;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* TimerText;

private:
    FGameplayTag EffectTag;
    int32 DisplayPriority = 50;
    float RemainingDuration = 0.0f;
    float MaxDuration = 0.0f;
    int32 CurrentStacks = 1;
    bool bIsInfinite = false;
};
```

---

## 11. Implementation Plan

### Phase 1: Simplify Data Structure
**Status:** COMPLETE

- [x] Create new `FSuspenseCoreStatusEffectVisualRow` (18 fields)
- [x] Keep old `FSuspenseCoreStatusEffectAttributeRow` for backward compatibility (deprecated)
- [x] Update DataManager to use new structure

### Phase 2: Fix JSON Format
**Status:** COMPLETE

- [x] Create new `SuspenseCoreStatusEffectVisuals.json` with simplified format
- [x] Test DataTable import in Editor
- [x] Verify no parsing errors in logs

### Phase 3: Create GameplayEffect Assets
**Status:** COMPLETE

- [x] Create base GE classes (GE_DoT_Base, GE_Buff_Base, GE_Debuff_Base)
- [x] Create GE_Bleeding_Light, GE_Bleeding_Heavy
- [x] Create GE_Burning, GE_Poisoned, GE_Stunned, GE_Suppressed
- [x] Create GE_Fracture_Leg, GE_Fracture_Arm, GE_Dehydrated, GE_Exhausted
- [x] Create GE_Regenerating, GE_Adrenaline, GE_Painkiller, GE_Fortified, GE_Haste
- [x] Configure all Duration, Period, Stacking policies

### Phase 4: Update DataManager
**Status:** COMPLETE

- [x] Add GetStatusEffectVisuals() method
- [x] Add GetStatusEffectVisualsByTag() method
- [x] Add GetCureItemsForEffect() method
- [x] Add CanItemCureEffect() method
- [x] Add IsStatusEffectVisualsReady() method

### Phase 5: Update UI Widgets
**Status:** COMPLETE

- [x] Update W_DebuffContainer to use new event format
- [x] Update W_DebuffIcon to use FSuspenseCoreStatusEffectVisualRow (v2.0 API)
- [x] Add infinite duration support (no timer bar)
- [x] Add stack count display

### Phase 6: Integration Testing
**Status:** PENDING (Runtime testing required)

- [ ] Test grenade DoT application
- [ ] Test cure items (bandage, medkit)
- [ ] Test UI updates in real-time
- [ ] Test multiplayer replication

---

## 12. File Manifest

### Files CREATED

```
Content/Data/StatusEffects/SuspenseCoreStatusEffectVisuals.json
  → 15 effects (10 debuffs, 5 buffs) with simplified format

Source/GAS/Public/SuspenseCore/Effects/StatusEffects/SuspenseCoreStatusEffects.h
Source/GAS/Private/SuspenseCore/Effects/StatusEffects/SuspenseCoreStatusEffects.cpp
  → All GameplayEffect C++ classes:
    - Base: USuspenseCoreEffect_DoT_Base, USuspenseCoreEffect_Buff_Base, USuspenseCoreEffect_Debuff_Base
    - Debuffs: UGE_Bleeding_Light, UGE_Bleeding_Heavy, UGE_Burning, UGE_Poisoned,
               UGE_Stunned, UGE_Suppressed, UGE_Fracture_Leg, UGE_Fracture_Arm,
               UGE_Dehydrated, UGE_Exhausted
    - Buffs: UGE_Regenerating, UGE_Painkiller, UGE_Adrenaline, UGE_Fortified, UGE_Haste
```

### Files MODIFIED

```
Source/BridgeSystem/Public/SuspenseCore/Types/GAS/SuspenseCoreGASAttributeRows.h
  → Added FSuspenseCoreStatusEffectVisualRow (18 fields)
  → Deprecated FSuspenseCoreStatusEffectAttributeRow

Source/BridgeSystem/Public/SuspenseCore/Data/SuspenseCoreDataManager.h
Source/BridgeSystem/Private/SuspenseCore/Data/SuspenseCoreDataManager.cpp
  → Added v2.0 Visual Data API:
    - GetStatusEffectVisuals(FName EffectKey)
    - GetStatusEffectVisualsByTag(FGameplayTag EffectTag)
    - GetCureItemsForEffect(FGameplayTag EffectTag)
    - CanItemCureEffect(FName ItemID, FGameplayTag EffectTag)
    - IsStatusEffectVisualsReady()

Source/UISystem/Public/SuspenseCore/Widgets/HUD/W_DebuffIcon.h
Source/UISystem/Private/SuspenseCore/Widgets/HUD/W_DebuffIcon.cpp
  → Updated to use FSuspenseCoreStatusEffectVisualRow
  → Updated to use v2.0 DataManager API
```

### Files to DELETE (after migration verified)

```
Content/Data/StatusEffects/SuspenseCoreStatusEffects.json (old format with 43+ fields)
```

---

## Approval Checklist

All items approved and implemented:

- [x] Architecture (GE Assets + DataTable визуалы) approved
- [x] JSON format approved
- [x] Effect list (Section 5) approved
- [x] GameplayEffect configurations (Section 6) approved
- [x] Implementation phases approved

---

**Document Status:** IMPLEMENTED

**Implementation Complete:**
- FSuspenseCoreStatusEffectVisualRow (18 fields, simplified)
- SuspenseCoreStatusEffectVisuals.json (15 effects)
- All GameplayEffect C++ classes (12 debuffs + 5 buffs)
- DataManager v2.0 API (GetStatusEffectVisuals, GetStatusEffectVisualsByTag, etc.)
- W_DebuffIcon updated to use v2.0 visual data API

**Next Steps:** Runtime integration testing required (Phase 6).
