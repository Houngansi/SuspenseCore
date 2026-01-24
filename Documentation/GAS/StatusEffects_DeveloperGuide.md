# Status Effects System - Developer Guide

**Version:** 1.0
**Date:** 2026-01-24
**Author:** Claude Code
**Status:** PRODUCTION

---

## Table of Contents

1. [Overview](#1-overview)
2. [Architecture](#2-architecture)
3. [GameplayEffect Configuration](#3-gameplayeffect-configuration)
4. [DataTable/JSON Configuration](#4-datatablejson-configuration)
5. [UI Widgets](#5-ui-widgets)
6. [Critical Bug Fixes & Lessons Learned](#6-critical-bug-fixes--lessons-learned)
7. [Adding New Effects](#7-adding-new-effects)
8. [Healing System Integration](#8-healing-system-integration)
9. [File Reference](#9-file-reference)
10. [Troubleshooting](#10-troubleshooting)

---

## 1. Overview

### 1.1 System Purpose

Система статус эффектов (баффов/дебаффов) в стиле Escape from Tarkov.
Эффекты применяются через GameplayAbilitySystem и отображаются в UI.

### 1.2 Key Principles

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        SEPARATION OF CONCERNS                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   GAMEPLAY DATA (C++ GameplayEffect)      VISUAL DATA (DataTable/JSON)     │
│   ══════════════════════════════════      ════════════════════════════     │
│   • Duration, Tick Interval               • DisplayName, Description       │
│   • Damage/Heal per tick                  • Icon texture, IconTint         │
│   • Stacking policy (StackLimitCount)     • VFX, Audio references          │
│   • Attribute modifiers                   • CureItemIDs                    │
│   • Granted/Blocked tags                  • Category (Buff/Debuff)         │
│                                                                             │
│   GE_BleedingEffect.cpp                   DT_StatusEffects.uasset          │
│   (C++ класс)                             (DataTable из JSON)              │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 1.3 Data Flow

```
┌──────────────────┐    ┌─────────────────┐    ┌────────────────────┐
│ GrenadeProjectile │───►│   DoTService    │───►│     EventBus       │
│ ApplyDoTEffects() │    │ PublishDoTEvent │    │ Publish(DoT.Event) │
└──────────────────┘    └─────────────────┘    └─────────┬──────────┘
                                                         │
                        ┌────────────────────────────────┘
                        ▼
┌──────────────────┐    ┌─────────────────┐    ┌────────────────────┐
│DataManager(SSOT) │◄───│W_DebuffContainer│◄───│   EventBus Sub     │
│GetVisualsByTag() │    │OnDoTApplied()   │    │ OnDoTApplied/Removed│
└────────┬─────────┘    └────────┬────────┘    └────────────────────┘
         │                       │
         ▼                       ▼
┌──────────────────┐    ┌─────────────────┐
│ DT_StatusEffects │    │   W_DebuffIcon  │
│ (Visuals SSOT)   │    │ SetDebuffData() │
└──────────────────┘    └─────────────────┘
```

---

## 2. Architecture

### 2.1 Component Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           STATUS EFFECT SYSTEM                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐       │
│  │ GameplayEffect  │     │   DoTService    │     │   DataManager   │       │
│  │    C++ Classes  │────►│  (Tracking)     │◄───►│  (SSOT Cache)   │       │
│  │                 │     │                 │     │                 │       │
│  │ • GE_Bleeding_* │     │ • OnEffectAdd   │     │ • GetVisuals()  │       │
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
│           │              │ • DoT.Stacked   │              │                 │
│           │              └────────┬────────┘              │                 │
│           │                       │                       │                 │
│           ▼                       ▼                       ▼                 │
│  ┌─────────────────────────────────────────────────────────────────┐       │
│  │                      ASC (AbilitySystemComponent)                │       │
│  │                                                                  │       │
│  │  • ApplyGameplayEffectToSelf()                                  │       │
│  │  • SetSetByCallerMagnitude() - для передачи урона               │       │
│  │  • RemoveActiveGameplayEffectByTag() - для лечения              │       │
│  └──────────────────────────────────────────────────────────────────┘       │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────┐       │
│  │                           UI LAYER                               │       │
│  │                                                                  │       │
│  │  W_DebuffContainer ──────────────────────────────────────────┐  │       │
│  │  │                                                           │  │       │
│  │  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐         │  │       │
│  │  │  │W_DebuffIcon │ │W_DebuffIcon │ │W_DebuffIcon │  ...    │  │       │
│  │  │  │             │ │             │ │             │         │  │       │
│  │  │  │ [Bleeding]  │ │ [Burning]   │ │[Suppressed] │         │  │       │
│  │  │  │  x3 stacks  │ │  4.5s left  │ │  2.1s left  │         │  │       │
│  │  │  └─────────────┘ └─────────────┘ └─────────────┘         │  │       │
│  │  └───────────────────────────────────────────────────────────┘  │       │
│  └──────────────────────────────────────────────────────────────────┘       │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Class Responsibilities

| Class | Location | Responsibility |
|-------|----------|---------------|
| `UGE_BleedingEffect_Light` | GE_BleedingEffect.cpp | DoT gameplay: duration, damage, stacking |
| `UGE_BleedingEffect_Heavy` | GE_BleedingEffect.cpp | Heavy DoT gameplay: higher damage |
| `USuspenseCoreDoTService` | SuspenseCoreDoTService.cpp | Tracks active effects, publishes EventBus |
| `USuspenseCoreDataManager` | SuspenseCoreDataManager.cpp | Caches DataTable, provides GetVisuals() API |
| `UW_DebuffContainer` | W_DebuffContainer.cpp | Creates/destroys W_DebuffIcon widgets |
| `UW_DebuffIcon` | W_DebuffIcon.cpp | Displays single effect: icon, timer, stacks |

---

## 3. GameplayEffect Configuration

### 3.1 Bleeding Effects (C++)

**Location:** `Source/GAS/Private/SuspenseCore/Effects/Grenade/GE_BleedingEffect.cpp`

#### Light Bleeding

```cpp
UGE_BleedingEffect_Light::UGE_BleedingEffect_Light()
{
    // Duration: INFINITE (requires cure)
    DurationPolicy = EGameplayEffectDurationType::Infinite;

    // Tick every 1 second
    Period = 1.0f;
    bExecutePeriodicEffectOnApplication = false;

    // STACKING: Tarkov-style - 3 bleeds = 3x damage
    StackingType = EGameplayEffectStackingType::AggregateByTarget;
    StackLimitCount = 5;  // Max 5 stacks

    // Damage via SetByCaller
    FGameplayModifierInfo DamageMod;
    DamageMod.Attribute = USuspenseCoreAttributeSet::GetHealthAttribute();
    DamageMod.ModifierOp = EGameplayModOp::Additive;
    DamageMod.ModifierMagnitude = FSetByCallerFloat();  // Tag: Data.DoT.Bleed
    Modifiers.Add(DamageMod);

    // Granted tag for UI tracking
    InheritableOwnedTagsContainer.AddTag(
        FGameplayTag::RequestGameplayTag("State.Health.Bleeding.Light"));
}
```

#### Heavy Bleeding

```cpp
UGE_BleedingEffect_Heavy::UGE_BleedingEffect_Heavy()
{
    // Same as Light, but:
    // - Higher damage (1.5 DPS vs 0.5 DPS)
    // - Max 5 stacks
    StackLimitCount = 5;

    // Granted tag
    InheritableOwnedTagsContainer.AddTag(
        FGameplayTag::RequestGameplayTag("State.Health.Bleeding.Heavy"));
}
```

### 3.2 Damage Values (Grenade Projectile)

**Location:** `Source/GAS/Private/SuspenseCore/Projectile/SuspenseCoreGrenadeProjectile.cpp`

```cpp
// Balanced damage values (Tarkov-style)
float BleedDamagePerTickLight = 0.5f;  // Light: 0.5 DPS per stack
float BleedDamagePerTickHeavy = 1.5f;  // Heavy: 1.5 DPS per stack

// Apply with SetByCaller
float DamagePerTick = bIsHeavy ? BleedDamagePerTickHeavy : BleedDamagePerTickLight;
SpecHandle.Data->SetSetByCallerMagnitude(SuspenseCoreTags::Data::DoT::Bleed, -DamagePerTick);
```

### 3.3 Stacking Behavior

| Effect | StackLimitCount | Result |
|--------|-----------------|--------|
| Light Bleeding | 5 | 1 stack = 0.5 DPS, 3 stacks = 1.5 DPS, 5 stacks = 2.5 DPS |
| Heavy Bleeding | 5 | 1 stack = 1.5 DPS, 3 stacks = 4.5 DPS, 5 stacks = 7.5 DPS |

---

## 4. DataTable/JSON Configuration

### 4.1 Project Settings

**Location:** Project Settings → Game → SuspenseCore → GAS Attributes

| Setting | DataTable | Purpose |
|---------|-----------|---------|
| StatusEffectAttributesDataTable | DT_StatusEffects | Effect metadata (cure items, category) |
| StatusEffectVisualsDataTable | DT_StatusEffects | Visual data (icons, tints) |

### 4.2 JSON Format

**Location:** `Content/Data/StatusEffects/SuspenseCoreStatusEffects.json`

```json
[
  {
    "Name": "BleedingLight",
    "EffectID": "BleedingLight",
    "EffectTypeTag": "(TagName=\"State.Health.Bleeding.Light\")",
    "DisplayName": "NSLOCTEXT(\"StatusEffect\", \"BleedingLight\", \"Light Bleeding\")",
    "Description": "NSLOCTEXT(\"StatusEffect\", \"BleedingLightDesc\", \"Use a bandage.\")",
    "Category": "Debuff",
    "DisplayPriority": 90,
    "MaxStacks": 5,
    "DamagePerTick": 0.5,
    "TickInterval": 1.0,
    "bCuredByBandage": true,
    "bCuredByMedkit": true,
    "Icon": "/Game/UI/Icons/StatusEffects/T_Icon_Bleeding_Light",
    "IconTint": "(R=1.0, G=0.2, B=0.2, A=1.0)"
  },
  {
    "Name": "BleedingHeavy",
    "EffectID": "BleedingHeavy",
    "EffectTypeTag": "(TagName=\"State.Health.Bleeding.Heavy\")",
    "DisplayName": "NSLOCTEXT(\"StatusEffect\", \"BleedingHeavy\", \"Heavy Bleeding\")",
    "Category": "Debuff",
    "DisplayPriority": 95,
    "MaxStacks": 5,
    "DamagePerTick": 1.5,
    "TickInterval": 1.0,
    "bCuredByBandage": false,
    "bCuredByMedkit": true,
    "bRequiresSurgery": false,
    "Icon": "/Game/UI/Icons/StatusEffects/T_Icon_Bleeding_Heavy",
    "IconTint": "(R=0.8, G=0.0, B=0.0, A=1.0)"
  }
]
```

### 4.3 DataTable Import

1. Right-click in Content Browser → Miscellaneous → Data Table
2. Row Structure: `FSuspenseCoreStatusEffectAttributeRow`
3. Name: `DT_StatusEffects`
4. Right-click DataTable → Reimport → Select JSON file

---

## 5. UI Widgets

### 5.1 Widget Hierarchy

```
WBP_MasterHUD
└── RootCanvas
    ├── VitalsWidget
    ├── AmmoCounterWidget
    └── DebuffContainerWidget (W_DebuffContainer)
        └── DebuffBox (UHorizontalBox)
            ├── W_DebuffIcon [Bleeding x3]
            ├── W_DebuffIcon [Burning 5s]
            └── ...
```

### 5.2 W_DebuffIcon Blueprint Structure

```
WBP_DebuffIcon
└── SizeContainer (SizeBox) ← Min/Max: 64x64
    └── [Overlay]
        ├── DebuffImage (UImage) ← BindWidget
        ├── TimerText (UTextBlock) ← BindWidget
        ├── StackText (UTextBlock) ← BindWidgetOptional
        └── DurationBar (UProgressBar) ← BindWidgetOptional
```

### 5.3 W_DebuffContainer Blueprint Structure

```
WBP_DebuffContainer
└── DebuffBox (UHorizontalBox) ← BindWidget
    └── [Dynamic children - W_DebuffIcon instances]
```

---

## 6. Critical Bug Fixes & Lessons Learned

### 6.1 Icon Visibility Bug (CRITICAL)

**Problem:** Icons created but not visible (visibility = Collapsed)

**Root Cause:** Initialization order issue:
```
❌ WRONG ORDER:
1. CreateWidget<UW_DebuffIcon>()
2. SetDebuffData() → Sets visibility to HitTestInvisible
3. AddChildToHorizontalBox() → Triggers NativeConstruct() → Sets visibility to Collapsed
```

**Fix:**
```cpp
// ✅ CORRECT ORDER in W_DebuffContainer.cpp:
// 1. FIRST add to parent (triggers NativeConstruct → Collapsed)
UHorizontalBoxSlot* IconSlot = DebuffBox->AddChildToHorizontalBox(NewIcon);
IconSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

// 2. THEN setup data (sets HitTestInvisible)
NewIcon->SetDebuffData(DoTType, Duration, StackCount);

// 3. Force layout update
DebuffBox->InvalidateLayoutAndVolatility();
```

**Commit:** `fix(DebuffContainer): Add icon to parent BEFORE calling SetDebuffData`

---

### 6.2 StatusEffectVisuals Not Loading

**Problem:** `StatusEffectVisuals not ready for tag State.Health.Bleeding.Heavy`

**Root Cause:** `InitializeStatusEffectVisualsSystem()` was never called

**Fix in SuspenseCoreDataManager.cpp:**
```cpp
void USuspenseCoreDataManager::Initialize()
{
    // ... other initialization ...

    // ✅ ADD THIS: Status Effect Visuals initialization
    bool bVisualsReady = InitializeStatusEffectVisualsSystem();
    if (bVisualsReady && bStatusEffectVisualsReady)
    {
        UE_LOG(LogSuspenseCoreData, Log, TEXT("Status Effect Visuals: READY (%d visuals cached)"),
            StatusEffectVisualsCache.Num());
    }
}
```

**Fix in SuspenseCoreSettings.h:**
```cpp
// ✅ ADD DataTable property
UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "GAS Attributes")
TSoftObjectPtr<UDataTable> StatusEffectVisualsDataTable;
```

---

### 6.3 Stacks Not Multiplying Damage

**Problem:** 3 stacks showed x3 but damage was still 0.5, not 1.5

**Root Cause:** `StackLimitCount = 1` in GameplayEffect

**Fix in GE_BleedingEffect.cpp:**
```cpp
// ❌ WRONG:
StackLimitCount = 1;  // Only 1 stack allowed

// ✅ CORRECT (Tarkov-style):
StackLimitCount = 5;  // Allow up to 5 stacks
```

**Commit:** `feat(Bleeding): Enable Tarkov-style stacking (up to 5x damage)`

---

### 6.4 Damage Too High

**Problem:** Players dying too fast from bleeding (5.0 DPS)

**Root Cause:** Single `BleedDamagePerTick = 5.0f` for both Light and Heavy

**Fix in SuspenseCoreGrenadeProjectile.h:**
```cpp
// ❌ WRONG:
float BleedDamagePerTick = 5.0f;  // Too high!

// ✅ CORRECT (balanced):
float BleedDamagePerTickLight = 0.5f;  // Light: 0.5 DPS
float BleedDamagePerTickHeavy = 1.5f;  // Heavy: 1.5 DPS
```

**Commit:** `balance(Bleeding): Separate Light/Heavy bleeding damage, reduce by 10x`

---

### 6.5 Lessons Summary

| Issue | Symptom | Root Cause | Fix |
|-------|---------|------------|-----|
| Icons invisible | visibility = 1 (Collapsed) | NativeConstruct order | Add to parent FIRST, then SetDebuffData |
| Visuals not loading | "not ready" log | Init function not called | Call InitializeStatusEffectVisualsSystem() |
| Stacks not working | x3 shows, damage = 1x | StackLimitCount = 1 | Set StackLimitCount = 5 |
| Dying too fast | 5.0 DPS | Single damage value | Separate Light (0.5) / Heavy (1.5) |

---

## 7. Adding New Effects

### 7.1 Checklist for New Effect

1. **GameplayEffect C++ Class:**
   - [ ] Create class in `GE_*Effect.cpp`
   - [ ] Set DurationPolicy (Infinite/HasDuration)
   - [ ] Set Period and tick behavior
   - [ ] Set StackingType and StackLimitCount
   - [ ] Add SetByCaller modifier for damage/heal
   - [ ] Add Granted Tag (State.Health.*)

2. **DataTable JSON:**
   - [ ] Add row to `SuspenseCoreStatusEffects.json`
   - [ ] Set EffectTypeTag matching Granted Tag
   - [ ] Set Icon path and IconTint
   - [ ] Set CureItemIDs if applicable
   - [ ] Reimport DataTable in Editor

3. **GameplayTags:**
   - [ ] Add native tag in `SuspenseCoreGameplayTags.h/cpp`
   - [ ] Add Data tag for SetByCaller

4. **Application Code:**
   - [ ] Update source (Grenade, Ability, etc.) to apply effect
   - [ ] Use SetSetByCallerMagnitude for dynamic values

### 7.2 Example: Adding Poisoned Effect

**GE_PoisonedEffect.cpp:**
```cpp
UGE_PoisonedEffect::UGE_PoisonedEffect()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FScalableFloat(30.0f);  // 30 seconds
    Period = 2.0f;  // Tick every 2 seconds

    StackingType = EGameplayEffectStackingType::AggregateByTarget;
    StackLimitCount = 3;

    // Damage modifier
    FGameplayModifierInfo DamageMod;
    DamageMod.Attribute = USuspenseCoreAttributeSet::GetHealthAttribute();
    DamageMod.ModifierOp = EGameplayModOp::Additive;
    DamageMod.ModifierMagnitude = FSetByCallerFloat(SuspenseCoreTags::Data::DoT::Poison);
    Modifiers.Add(DamageMod);

    // Granted tag
    InheritableOwnedTagsContainer.AddTag(
        FGameplayTag::RequestGameplayTag("State.Health.Poisoned"));
}
```

**JSON Entry:**
```json
{
  "Name": "Poisoned",
  "EffectID": "Poisoned",
  "EffectTypeTag": "(TagName=\"State.Health.Poisoned\")",
  "DisplayName": "NSLOCTEXT(\"StatusEffect\", \"Poisoned\", \"Poisoned\")",
  "Category": "Debuff",
  "DisplayPriority": 85,
  "MaxStacks": 3,
  "DamagePerTick": 2.0,
  "TickInterval": 2.0,
  "Icon": "/Game/UI/Icons/StatusEffects/T_Icon_Poisoned",
  "IconTint": "(R=0.2, G=0.8, B=0.2, A=1.0)",
  "CureItemIDs": ["Item_Antidote"]
}
```

---

## 8. Healing System Integration

### 8.1 Cure Effects via Items

```cpp
// In GA_UseConsumable or similar ability:
void UGA_UseConsumable::CureBleedingEffect(UAbilitySystemComponent* ASC, bool bHeavyOnly)
{
    FGameplayTagContainer TagsToRemove;

    if (bHeavyOnly)
    {
        // Medkit: cure heavy bleeding
        TagsToRemove.AddTag(SuspenseCoreTags::State::Health::BleedingHeavy);
    }
    else
    {
        // Bandage: cure light bleeding only
        TagsToRemove.AddTag(SuspenseCoreTags::State::Health::BleedingLight);
    }

    // Remove ALL stacks of the effect
    ASC->RemoveActiveEffectsWithTags(TagsToRemove);
}
```

### 8.2 Query Cure Items from SSOT

```cpp
USuspenseCoreDataManager* DataManager = USuspenseCoreDataManager::Get(this);

// Check if bandage can cure light bleeding
FGameplayTag LightBleedTag = SuspenseCoreTags::State::Health::BleedingLight;
if (DataManager->CanItemCureEffect(FName("Item_Bandage"), LightBleedTag))
{
    // Allow use
}

// Get all items that cure this effect
TArray<FName> CureItems = DataManager->GetCureItemsForEffect(LightBleedTag);
```

### 8.3 Healing Over Time (HoT) Effects

```cpp
UGE_Regenerating::UGE_Regenerating()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FScalableFloat(10.0f);  // 10 seconds
    Period = 1.0f;

    StackingType = EGameplayEffectStackingType::None;  // No stacking
    StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;

    // Heal modifier (POSITIVE value)
    FGameplayModifierInfo HealMod;
    HealMod.Attribute = USuspenseCoreAttributeSet::GetHealthAttribute();
    HealMod.ModifierOp = EGameplayModOp::Additive;
    HealMod.ModifierMagnitude = FScalableFloat(5.0f);  // +5 HP per tick
    Modifiers.Add(HealMod);

    // Granted tag
    InheritableOwnedTagsContainer.AddTag(
        FGameplayTag::RequestGameplayTag("State.Health.Regenerating"));
}
```

---

## 9. File Reference

### 9.1 Core Files

| File | Purpose |
|------|---------|
| `Source/GAS/Private/SuspenseCore/Effects/Grenade/GE_BleedingEffect.cpp` | Bleeding GameplayEffects |
| `Source/GAS/Private/SuspenseCore/Projectile/SuspenseCoreGrenadeProjectile.cpp` | Grenade DoT application |
| `Source/BridgeSystem/Private/SuspenseCore/Data/SuspenseCoreDataManager.cpp` | SSOT data loading |
| `Source/BridgeSystem/Public/SuspenseCore/Settings/SuspenseCoreSettings.h` | DataTable references |

### 9.2 UI Files

| File | Purpose |
|------|---------|
| `Source/UISystem/Private/SuspenseCore/Widgets/HUD/W_DebuffIcon.cpp` | Individual icon widget |
| `Source/UISystem/Private/SuspenseCore/Widgets/HUD/W_DebuffContainer.cpp` | Icon container/manager |

### 9.3 Data Files

| File | Purpose |
|------|---------|
| `Content/Data/StatusEffects/SuspenseCoreStatusEffects.json` | Effect definitions |
| `Content/UI/Icons/StatusEffects/` | Icon textures |

### 9.4 Documentation

| File | Purpose |
|------|---------|
| `Documentation/GAS/StatusEffects_DeveloperGuide.md` | This file |
| `Documentation/Plans/StatusEffect_SSOT_System.md` | SSOT architecture |
| `Documentation/Plans/DebuffWidget_System_Plan.md` | Widget implementation |
| `Documentation/GameDesign/StatusEffect_System_GDD.md` | Game design spec |

---

## 10. Troubleshooting

### 10.1 Icons Not Appearing

```
LogDebuffContainer: AddOrUpdateDebuff: Type=State.Health.Bleeding.Heavy
LogDebuffIcon: SetDebuffData: Type=State.Health.Bleeding.Heavy
LogDebuffIcon: Icon visibility: 1  ← PROBLEM! Should be 3
```

**Solution:** Check W_DebuffContainer.cpp initialization order (Section 6.1)

### 10.2 "StatusEffectVisuals not ready"

```
LogSuspenseCoreData: Warning: StatusEffectVisuals not ready for tag State.Health.Bleeding.Heavy
```

**Solution:**
1. Check Project Settings → SuspenseCore → StatusEffectVisualsDataTable
2. Verify InitializeStatusEffectVisualsSystem() is called in DataManager::Initialize()

### 10.3 Damage Not Scaling with Stacks

**Symptom:** x3 stacks, but damage = 0.5 instead of 1.5

**Solution:** Check StackLimitCount in GameplayEffect (should be > 1)

### 10.4 Debug Logging

Enable verbose logging:
```cpp
// In DefaultEngine.ini or via console
LogSuspenseCoreGAS=VeryVerbose
LogDebuffContainer=VeryVerbose
LogDebuffIcon=VeryVerbose
```

---

## Commit History (Recent Fixes)

| Commit | Description |
|--------|-------------|
| `feat(Bleeding): Enable Tarkov-style stacking (up to 5x damage)` | StackLimitCount 1→5 |
| `balance(Bleeding): Separate Light/Heavy bleeding damage, reduce by 10x` | 5.0→0.5/1.5 DPS |
| `fix(DebuffContainer): Add icon to parent BEFORE calling SetDebuffData` | Visibility fix |
| `debug(DebuffIcon): Add widget binding validation logging` | Debug logging |

---

**Document End**
