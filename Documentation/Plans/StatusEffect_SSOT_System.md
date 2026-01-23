# StatusEffect SSOT System Design

**Version:** 1.0
**Date:** 2026-01-23
**Author:** SuspenseCore Team
**Status:** Implemented

---

## 1. Overview

The StatusEffect SSOT (Single Source of Truth) system provides a centralized DataTable-driven approach for managing all buffs and debuffs in the game. This system replaces hardcoded effect definitions with a flexible, designer-friendly data-driven architecture.

### 1.1 Key Features

- **Single Source of Truth**: All status effect data defined in one DataTable
- **Designer-Friendly**: Edit effects via JSON without code changes
- **Full Visual Configuration**: Icons, colors, VFX, audio per effect
- **GAS Integration**: Direct GameplayEffect and tag support
- **UI Integration**: W_DebuffIcon widget queries SSOT automatically
- **Tarkov-Style Medical System**: Support for cure requirements, surgery, etc.

### 1.2 Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        StatusEffect SSOT Architecture                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────────┐        ┌────────────────────────────────┐      │
│  │ JSON Data File      │        │ SuspenseCoreProjectSettings     │      │
│  │ SuspenseCoreStatus  │───────▶│ StatusEffectAttributesDataTable │      │
│  │ Effects.json        │        └───────────────┬────────────────┘      │
│  └─────────────────────┘                        │                        │
│                                                  │ Load on Init           │
│                                                  ▼                        │
│  ┌───────────────────────────────────────────────────────────────┐      │
│  │                  SuspenseCoreDataManager                       │      │
│  │  ┌─────────────────────────────────────────────────────────┐  │      │
│  │  │ StatusEffectAttributesCache                              │  │      │
│  │  │ TMap<FName, FSuspenseCoreStatusEffectAttributeRow>       │  │      │
│  │  └─────────────────────────────────────────────────────────┘  │      │
│  │  ┌─────────────────────────────────────────────────────────┐  │      │
│  │  │ StatusEffectTagToIDMap                                   │  │      │
│  │  │ TMap<FGameplayTag, FName>                                │  │      │
│  │  └─────────────────────────────────────────────────────────┘  │      │
│  └──────────────────────────┬────────────────────────────────────┘      │
│                              │                                           │
│              ┌───────────────┼───────────────┐                          │
│              ▼               ▼               ▼                          │
│  ┌──────────────────┐ ┌──────────────┐ ┌──────────────────┐            │
│  │  DoTService      │ │ W_DebuffIcon │ │ Medical System   │            │
│  │  (Apply/Remove)  │ │ (Display)    │ │ (Cure Logic)     │            │
│  └──────────────────┘ └──────────────┘ └──────────────────┘            │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Data Structure

### 2.1 FSuspenseCoreStatusEffectAttributeRow

The primary SSOT structure for status effects. Defined in `SuspenseCoreGASAttributeRows.h`.

```cpp
USTRUCT(BlueprintType)
struct FSuspenseCoreStatusEffectAttributeRow : public FTableRowBase
{
    // ═══════════════════════════════════════════════════════════════════
    // Identity
    // ═══════════════════════════════════════════════════════════════════

    FName EffectID;                              // Unique ID (BleedingLight, Burning)
    FText DisplayName;                           // Localized name
    FText Description;                           // Tooltip description
    FGameplayTag EffectTypeTag;                  // State.Health.Bleeding.Light
    ESuspenseCoreStatusEffectCategory Category;  // Buff, Debuff, Neutral
    int32 DisplayPriority;                       // UI sort order (higher = first)

    // ═══════════════════════════════════════════════════════════════════
    // Duration & Stacking
    // ═══════════════════════════════════════════════════════════════════

    float DefaultDuration;                       // -1 for infinite
    bool bIsInfinite;                            // Requires cure to remove
    int32 MaxStacks;                             // Stack limit
    ESuspenseCoreStackBehavior StackBehavior;    // Additive, RefreshDuration, etc.

    // ═══════════════════════════════════════════════════════════════════
    // Damage Over Time (DoT)
    // ═══════════════════════════════════════════════════════════════════

    float DamagePerTick;                         // 0 = no DoT
    float TickInterval;                          // Seconds between ticks
    FGameplayTag DamageTypeTag;                  // Damage.Type.Bleeding
    bool bBypassArmor;                           // Ignore armor
    float StackDamageMultiplier;                 // Per-stack damage mult

    // ═══════════════════════════════════════════════════════════════════
    // Healing Over Time (HoT)
    // ═══════════════════════════════════════════════════════════════════

    float HealPerTick;                           // 0 = no HoT
    float HealTickInterval;                      // Seconds between heals

    // ═══════════════════════════════════════════════════════════════════
    // Attribute Modifiers
    // ═══════════════════════════════════════════════════════════════════

    TArray<FSuspenseCoreStatusEffectModifier> AttributeModifiers;
    // - AttributeTag: Attribute.MoveSpeed
    // - FlatModifier: +/-N
    // - PercentModifier: 0.85 = -15%

    // ═══════════════════════════════════════════════════════════════════
    // Cure/Removal Requirements
    // ═══════════════════════════════════════════════════════════════════

    TArray<FName> CureItemIDs;                   // Items that cure this
    FGameplayTagContainer CureEffectTags;        // Effects that cure this
    bool bCuredByBandage;
    bool bCuredByMedkit;
    bool bRequiresSurgery;

    // ═══════════════════════════════════════════════════════════════════
    // Visual - Icons
    // ═══════════════════════════════════════════════════════════════════

    TSoftObjectPtr<UTexture2D> Icon;
    FLinearColor IconTint;                       // Normal state color
    FLinearColor CriticalIconTint;               // Low health color
    float IconScale;

    // ═══════════════════════════════════════════════════════════════════
    // Visual - VFX
    // ═══════════════════════════════════════════════════════════════════

    TSoftObjectPtr<UNiagaraSystem> CharacterVFX;
    TSoftObjectPtr<UParticleSystem> CharacterVFXLegacy;
    FName VFXAttachSocket;
    TSoftObjectPtr<UMaterialInterface> ScreenOverlayMaterial;
    TSoftObjectPtr<UMaterialInterface> PostProcessMaterial;

    // ═══════════════════════════════════════════════════════════════════
    // Audio
    // ═══════════════════════════════════════════════════════════════════

    TSoftObjectPtr<USoundBase> ApplicationSound;
    TSoftObjectPtr<USoundBase> TickSound;
    TSoftObjectPtr<USoundBase> RemovalSound;
    TSoftObjectPtr<USoundBase> AmbientLoop;

    // ═══════════════════════════════════════════════════════════════════
    // Animation
    // ═══════════════════════════════════════════════════════════════════

    TSoftObjectPtr<UAnimMontage> ApplicationMontage;
    bool bPreventsSprinting;
    bool bPreventsADS;
    bool bCausesLimp;

    // ═══════════════════════════════════════════════════════════════════
    // GAS Integration
    // ═══════════════════════════════════════════════════════════════════

    TSoftClassPtr<UGameplayEffect> GameplayEffectClass;
    FGameplayTagContainer GrantedTags;           // Tags added while active
    FGameplayTagContainer BlockedByTags;         // Tags that block application
    FGameplayTagContainer RequiredTags;          // Tags required for application
};
```

### 2.2 Stack Behavior Enum

```cpp
UENUM(BlueprintType)
enum class ESuspenseCoreStackBehavior : uint8
{
    Additive,        // Each stack adds to total effect
    StrongestWins,   // Only strongest stack applies
    RefreshDuration, // Duration resets on new application
    ExtendDuration,  // Duration extends on new application
    NoStack          // Cannot stack - new application ignored
};
```

### 2.3 Effect Category Enum

```cpp
UENUM(BlueprintType)
enum class ESuspenseCoreStatusEffectCategory : uint8
{
    Debuff,   // Negative effect
    Buff,     // Positive effect
    Neutral   // Neither (markers, reveals)
};
```

---

## 3. Configuration

### 3.1 Project Settings

Navigate to **Project Settings → Game → SuspenseCore** and configure:

```
GAS Attributes:
└── Status Effect Attributes DataTable: DT_StatusEffects
```

### 3.2 DataTable Creation

1. **Create DataTable:**
   - Content Browser → Miscellaneous → Data Table
   - Row Structure: `FSuspenseCoreStatusEffectAttributeRow`
   - Name: `DT_StatusEffects`

2. **Import JSON:**
   - File → Import
   - Select `Content/Data/StatusEffects/SuspenseCoreStatusEffects.json`

### 3.3 JSON Format Example

```json
[
  {
    "Name": "BleedingLight",
    "EffectID": "BleedingLight",
    "DisplayName": "NSLOCTEXT(\"StatusEffect\", \"BleedingLight\", \"Light Bleeding\")",
    "Description": "NSLOCTEXT(\"StatusEffect\", \"BleedingLightDesc\", \"Use a bandage to stop.\")",
    "EffectTypeTag": "(TagName=\"State.Health.Bleeding.Light\")",
    "Category": "Debuff",
    "DisplayPriority": 90,
    "DefaultDuration": -1.0,
    "bIsInfinite": true,
    "MaxStacks": 3,
    "StackBehavior": "Additive",
    "DamagePerTick": 3.0,
    "TickInterval": 1.0,
    "DamageTypeTag": "(TagName=\"Damage.Type.Bleeding\")",
    "bBypassArmor": true,
    "bCuredByBandage": true,
    "Icon": "/Game/UI/Icons/StatusEffects/T_Icon_Bleeding_Light",
    "IconTint": "(R=1.0, G=0.2, B=0.2, A=1.0)",
    "GrantedTags": "(GameplayTags=((TagName=\"State.Health.Bleeding\")))"
  }
]
```

---

## 4. API Reference

### 4.1 DataManager Methods

```cpp
// Get effect by ID
bool GetStatusEffectAttributes(FName EffectKey, FSuspenseCoreStatusEffectAttributeRow& OutAttributes);

// Get effect by tag
bool GetStatusEffectByTag(FGameplayTag EffectTag, FSuspenseCoreStatusEffectAttributeRow& OutAttributes);

// Get all effects by category
TArray<FName> GetStatusEffectsByCategory(ESuspenseCoreStatusEffectCategory Category);

// Get all debuff IDs
TArray<FName> GetAllDebuffIDs();

// Get all buff IDs
TArray<FName> GetAllBuffIDs();

// Check if effect exists
bool HasStatusEffect(FName EffectKey);

// Get all effect keys
TArray<FName> GetAllStatusEffectKeys();

// Get cached count
int32 GetCachedStatusEffectCount();

// Check if system ready
bool IsStatusEffectSystemReady();
```

### 4.2 C++ Usage Examples

```cpp
// Query effect by ID
USuspenseCoreDataManager* DataManager = USuspenseCoreDataManager::Get(this);
FSuspenseCoreStatusEffectAttributeRow EffectData;
if (DataManager->GetStatusEffectAttributes("BleedingLight", EffectData))
{
    float Damage = EffectData.DamagePerTick;
    bool bInfinite = EffectData.bIsInfinite;
}

// Query effect by tag
FGameplayTag BleedTag = FGameplayTag::RequestGameplayTag("State.Health.Bleeding.Light");
if (DataManager->GetStatusEffectByTag(BleedTag, EffectData))
{
    // Use effect data
}

// Get all debuffs
TArray<FName> AllDebuffs = DataManager->GetAllDebuffIDs();
for (const FName& DebuffID : AllDebuffs)
{
    // Process each debuff
}
```

### 4.3 Blueprint Usage

```
// Get DataManager
DataManager = Get SuspenseCore Data Manager

// Query effect
Get Status Effect Attributes (EffectKey: "BleedingLight") → OutAttributes

// Use data
OutAttributes.DamagePerTick
OutAttributes.Icon
OutAttributes.DisplayName
```

---

## 5. UI Integration

### 5.1 W_DebuffIcon SSOT Mode

The `UW_DebuffIcon` widget supports automatic SSOT data loading:

```cpp
// Enable SSOT mode (default: true)
UPROPERTY(EditDefaultsOnly, Category = "Debuff|Config")
bool bUseSSOTData = true;

// Initialize from SSOT by EffectID
UFUNCTION(BlueprintCallable)
void SetDebuffDataFromSSOT(FName EffectID, float InDuration = 0.0f, int32 InStackCount = 1);
```

When SSOT mode is enabled:
- Icons loaded from `StatusEffectAttributesDataTable.Icon`
- Tint colors from `IconTint` / `CriticalIconTint`
- Automatic fallback to local `DebuffIcons` map if SSOT fails

### 5.2 Blueprint Widget Setup

1. Create Widget Blueprint from `W_DebuffIcon`
2. Enable `bUseSSOTData` in defaults
3. Call `SetDebuffDataFromSSOT` with EffectID

---

## 6. Included Effects

### 6.1 Debuffs

| EffectID | Tag | DoT | Duration | Stacks |
|----------|-----|-----|----------|--------|
| BleedingLight | State.Health.Bleeding.Light | 3/s | Infinite | 3 |
| BleedingHeavy | State.Health.Bleeding.Heavy | 8/s | Infinite | 2 |
| Burning | State.Health.Burning | 10/0.5s | 5s | 1 |
| Poisoned | State.Health.Poisoned | 2/2s | 30s | 3 |
| Stunned | State.Combat.Stunned | - | 3s | 1 |
| Suppressed | State.Combat.Suppressed | - | 2s | 5 |
| Fracture | State.Health.Fracture | - | Infinite | 1 |
| Blinded | State.Combat.Blinded | - | 5s | 1 |
| Slowed | State.Movement.Slowed | - | 5s | 3 |
| Dehydrated | State.Survival.Dehydrated | 1/5s | Infinite | 1 |
| Exhausted | State.Survival.Exhausted | - | 30s | 1 |

### 6.2 Buffs

| EffectID | Tag | HoT | Duration | Effect |
|----------|-----|-----|----------|--------|
| Regenerating | State.Health.Regenerating | 5/s | 10s | Health regen |
| Painkiller | State.Health.Painkiller | - | 120s | Pain immunity |
| Adrenaline | State.Combat.Adrenaline | - | 30s | +15% speed, +25% stamina regen |
| Fortified | State.Combat.Fortified | - | 15s | +25% damage resistance |

---

## 7. Native GameplayTags

### 7.1 State Tags

```cpp
// Health states
SuspenseCoreTags::State::Health::BleedingLight   // State.Health.Bleeding.Light
SuspenseCoreTags::State::Health::BleedingHeavy   // State.Health.Bleeding.Heavy
SuspenseCoreTags::State::Health::Poisoned        // State.Health.Poisoned
SuspenseCoreTags::State::Health::Fracture        // State.Health.Fracture
SuspenseCoreTags::State::Health::Painkiller      // State.Health.Painkiller
SuspenseCoreTags::State::Health::Regenerating    // State.Health.Regenerating

// Combat states
SuspenseCoreTags::State::Combat::Blinded         // State.Combat.Blinded
SuspenseCoreTags::State::Combat::Suppressed      // State.Combat.Suppressed
SuspenseCoreTags::State::Combat::Adrenaline      // State.Combat.Adrenaline
SuspenseCoreTags::State::Combat::Fortified       // State.Combat.Fortified

// Movement states
SuspenseCoreTags::State::Movement::Slowed        // State.Movement.Slowed
SuspenseCoreTags::State::Movement::Disabled      // State.Movement.Disabled

// Survival states
SuspenseCoreTags::State::Survival::Dehydrated    // State.Survival.Dehydrated
SuspenseCoreTags::State::Survival::Exhausted     // State.Survival.Exhausted
SuspenseCoreTags::State::Survival::Hungry        // State.Survival.Hungry

// Immunity tags
SuspenseCoreTags::State::Immunity::Pain          // State.Immunity.Pain
SuspenseCoreTags::State::Immunity::Fire          // State.Immunity.Fire
SuspenseCoreTags::State::Immunity::Poison        // State.Immunity.Poison
SuspenseCoreTags::State::Immunity::Stun          // State.Immunity.Stun
SuspenseCoreTags::State::Immunity::Flash         // State.Immunity.Flash
SuspenseCoreTags::State::Immunity::Slow          // State.Immunity.Slow
```

### 7.2 Effect Tags

```cpp
// Cure effects
SuspenseCoreTags::Effect::Cure::Bleeding         // Effect.Cure.Bleeding
SuspenseCoreTags::Effect::Cure::BleedingHeavy    // Effect.Cure.Bleeding.Heavy
SuspenseCoreTags::Effect::Cure::Fire             // Effect.Cure.Fire
SuspenseCoreTags::Effect::Cure::Poison           // Effect.Cure.Poison
SuspenseCoreTags::Effect::Cure::Fracture         // Effect.Cure.Fracture
SuspenseCoreTags::Effect::Cure::Dehydration      // Effect.Cure.Dehydration
SuspenseCoreTags::Effect::Cure::Exhaustion       // Effect.Cure.Exhaustion

// Buff effects
SuspenseCoreTags::Effect::Buff::Regenerating     // Effect.Buff.Regenerating
SuspenseCoreTags::Effect::Buff::Painkiller       // Effect.Buff.Painkiller
SuspenseCoreTags::Effect::Buff::Adrenaline       // Effect.Buff.Adrenaline
SuspenseCoreTags::Effect::Buff::Fortified        // Effect.Buff.Fortified
```

---

## 8. Files Modified/Created

### 8.1 New Files

- `Content/Data/StatusEffects/SuspenseCoreStatusEffects.json`
- `Documentation/Plans/StatusEffect_SSOT_System.md` (this file)

### 8.2 Modified Files

| File | Changes |
|------|---------|
| `SuspenseCoreGASAttributeRows.h` | Added `FSuspenseCoreStatusEffectAttributeRow`, enums |
| `SuspenseCoreSettings.h` | Added `StatusEffectAttributesDataTable` |
| `SuspenseCoreDataManager.h/cpp` | Added StatusEffect cache and accessor methods |
| `SuspenseCoreGameplayTags.h/cpp` | Added State::*, Effect::Cure::*, Effect::Buff::* tags |
| `W_DebuffIcon.h/cpp` | Added SSOT integration, `SetDebuffDataFromSSOT()` |

---

## 9. Future Enhancements

1. **Combo System**: Effects that trigger other effects (e.g., Burning + Poisoned = Toxic)
2. **Body Part Targeting**: Effects that only affect specific limbs
3. **Environmental Effects**: Weather/zone-based status effects
4. **Effect Immunity Scaling**: Partial immunity based on armor/traits
5. **Visual Effect Layering**: Multiple VFX stacking rules

---

## 10. Related Documentation

- [DebuffWidget_System_Plan.md](./DebuffWidget_System_Plan.md) - Widget implementation details
- [SSOT_AttributeSet_DataTable_Integration.md](./SSOT_AttributeSet_DataTable_Integration.md) - SSOT pattern reference
- [UISystem_DeveloperGuide.md](../UISystem_DeveloperGuide.md) - UI architecture
- [WeaponSSOTReference.md](../Data/WeaponSSOTReference.md) - Similar SSOT implementation
