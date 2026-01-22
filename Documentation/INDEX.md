# SuspenseCore Documentation Index

## Quick Navigation

> **For Devlog:** See [Completed Features](#completed-features) for ready-to-publish content.

**Last Updated:** 2026-01-22

---

## Table of Contents

1. [Completed Features](#completed-features) - Ready for devlog
2. [Active Plans](#active-plans) - In progress
3. [Developer Guides](#developer-guides) - How-to documentation
4. [Technical References](#technical-references) - API & data references
5. [Game Design](#game-design) - GDD documents

---

## Completed Features

### Tarkov-Style Ammo System (8 Phases Complete)

**File:** [Completed/TarkovStyle_Ammo_System_Design.md](Completed/TarkovStyle_Ammo_System_Design.md)

**Devlog Summary:**
> Implemented a realistic ammunition system inspired by Escape from Tarkov. Magazines are physical
> inventory items with individual round tracking. QuickSlots (1-4) provide fast tactical access
> to spare magazines. The system supports mixed ammo types in magazines (AP rounds on top for
> first shots, tracers every 5th round, etc.).

**Key Features:**
- Physical magazines with per-round tracking
- QuickSlot system for fast magazine swaps
- MagazineComponent on weapons (insert/eject/chamber)
- QuickSlotComponent on characters (4 slots)
- GAS abilities: GA_Reload, GA_QuickSlot
- Full UI integration (inspection widget, HUD counter)
- EventBus sync between DataStore and QuickSlots

**Created Files:**
```
Source/EquipmentSystem/
├── Components/SuspenseCoreMagazineComponent.h/.cpp
├── Components/SuspenseCoreQuickSlotComponent.h/.cpp
└── Services/SuspenseCoreAmmoLoadingService.h/.cpp

Source/GAS/Abilities/
├── GA_Reload.h/.cpp
└── GA_QuickSlot.h/.cpp
```

---

### Weapon Fire System Migration (Complete)

**File:** [Completed/WeaponGASMigrationProgress.md](Completed/WeaponGASMigrationProgress.md)

**Devlog Summary:**
> Migrated the entire weapon fire system from legacy MedCom module to SuspenseCore architecture.
> All 11 weapon-related classes now follow SOLID principles with proper GAS integration.

**Migrated Classes:**
| Original | New | Module |
|----------|-----|--------|
| MedComBaseFireAbility | SuspenseCoreBaseFireAbility | GAS |
| MedComSingleShotAbility | SuspenseCoreSingleShotAbility | GAS |
| MedComAutoFireAbility | SuspenseCoreAutoFireAbility | GAS |
| MedComBurstFireAbility | SuspenseCoreBurstFireAbility | GAS |
| MedComTraceUtils | SuspenseCoreTraceUtils | BridgeSystem |
| MedComSpreadProcessor | SuspenseCoreSpreadProcessor | BridgeSystem |

---

### Movement Abilities Audit (All Issues Fixed)

**File:** [Completed/MovementAbilities_AuditReport.md](Completed/MovementAbilities_AuditReport.md)

**Devlog Summary:**
> Complete audit and fix of all movement abilities: Sprint, Crouch, Prone, Jump, Vault.
> All abilities now properly integrate with GAS AttributeSets and stamina system.

**Fixed Issues:**
- Stamina consumption now uses GAS GameplayEffects
- Movement speed modifiers properly replicated
- Prone/crouch state transitions validated
- Jump height linked to character attributes

---

## Active Plans

### Tarkov-Style Recoil System (Phase 4 of 6)

**File:** [Plans/TarkovStyle_Recoil_System_Design.md](Plans/TarkovStyle_Recoil_System_Design.md)
**Status:** Phase 4 Complete (Convergence + Ergonomics)

**Devlog Preview:**
> Implemented Tarkov-style recoil with convergence! Camera now kicks up on each shot and
> automatically returns to the original aim point. Ergonomics affects recovery speed
> (42 ergo = 1.42× faster). Attachment modifier SSOT is ready for integration.

**Completed:**
- [x] Phase 1: Basic recoil with AmmoRecoilModifier
- [x] Phase 2: Convergence system (camera auto-returns to aim)
- [x] Phase 3: Attachment SSOT (FSuspenseCoreAttachmentAttributeRow)
- [x] Phase 4: Ergonomics integration (affects convergence speed)

**Remaining:**
- [ ] Phase 3: Attachment integration (read from weapon, multiply modifiers)
- [ ] Phase 5: Visual vs Aim recoil separation
- [ ] Phase 6: Recoil patterns

---

### Weapon Slot Switching

**File:** [Plans/WeaponSlotSwitching_Design.md](Plans/WeaponSlotSwitching_Design.md)
**Status:** PLANNING (Ready for implementation)

**Summary:** Design for switching between primary/secondary weapons using number keys (1-3).

---

### SSOT AttributeSet DataTable Integration

**File:** [Plans/SSOT_AttributeSet_DataTable_Integration.md](Plans/SSOT_AttributeSet_DataTable_Integration.md)
**Status:** Planning

**Summary:** Architecture for Single Source of Truth data flow from JSON → DataTable → AttributeSet.

---

### Instance Zone System

**File:** [Plans/InstanceZone_System_Design.md](Plans/InstanceZone_System_Design.md)
**Status:** Planning

**Summary:** Level streaming and zone-based instance management for extraction gameplay.

---

### Item Use System Pipeline

**File:** [Plans/ItemUseSystem_Pipeline.md](Plans/ItemUseSystem_Pipeline.md)
**Status:** APPROVED

**Summary:** Pipeline for using consumable items (medkits, food, etc.) with GAS integration.

---

### DoT System (Bleeding/Burning) - NEW

**File:** [Plans/DoT_System_ImplementationPlan.md](Plans/DoT_System_ImplementationPlan.md)
**Status:** Phase 1-3 Complete, Phase 4-5 In Progress

**Devlog Preview:**
> Implemented Tarkov-style Damage-over-Time effects for grenades. Fragmentation grenades cause
> bleeding wounds (only when armor = 0), while incendiary grenades cause burning that bypasses
> armor entirely. DoTService with EventBus integration provides real-time effect tracking for UI.

**Completed:**
- [x] Phase 1: Native GameplayTags (State::Health::Bleeding, Event::DoT, etc.)
- [x] Phase 2: DoTService (EventBus integration, query API)
- [x] Phase 3: GrenadeProjectile DoT application

**Remaining:**
- [ ] Phase 4: Debuff Widget system (W_DebuffIcon, W_DebuffContainer)
- [ ] Phase 5: Documentation finalization

**Related:**
- [GAS/GrenadeDoT_DesignDocument.md](GAS/GrenadeDoT_DesignDocument.md) - Technical design
- [Plans/DebuffWidget_System_Plan.md](Plans/DebuffWidget_System_Plan.md) - UI widget plan

---

## Developer Guides

### Core Guides

| Document | Description |
|----------|-------------|
| [SuspenseCoreDeveloperGuide.md](SuspenseCoreDeveloperGuide.md) | Project architecture overview |
| [AnimationSystem_DeveloperGuide.md](AnimationSystem_DeveloperGuide.md) | Animation Blueprint setup |
| [UISystem_DeveloperGuide.md](UISystem_DeveloperGuide.md) | UI widget architecture |
| [WeaponAbilities_DeveloperGuide.md](WeaponAbilities_DeveloperGuide.md) | Weapon ability creation |

### GAS (Gameplay Ability System)

| Document | Description |
|----------|-------------|
| [GAS/AttributeSystem_DeveloperGuide.md](GAS/AttributeSystem_DeveloperGuide.md) | AttributeSet creation |
| [GAS/MovementAbilities_SetupGuide.md](GAS/MovementAbilities_SetupGuide.md) | Movement ability setup |
| [GAS/WeaponAbilityDevelopmentGuide.md](GAS/WeaponAbilityDevelopmentGuide.md) | Fire ability development |
| [GAS/WeaponAbilityArchitecture_TechnicalAnalysis.md](GAS/WeaponAbilityArchitecture_TechnicalAnalysis.md) | Technical deep-dive |
| [GAS/WeaponSwitch_FullAudit.md](GAS/WeaponSwitch_FullAudit.md) | Weapon switching analysis |
| [GAS/FireAbilityChecklist.md](GAS/FireAbilityChecklist.md) | Fire ability checklist |
| [GAS/GrenadeDoT_DesignDocument.md](GAS/GrenadeDoT_DesignDocument.md) | **NEW** DoT effects (Bleeding/Burning) |
| [GAS/HealingSystem_DesignProposal.md](GAS/HealingSystem_DesignProposal.md) | **NEW** HoT effects (Medical items) |

### Setup Guides

| Document | Description |
|----------|-------------|
| [Guides/MagazineInspectionWidget_Setup_Guide.md](Guides/MagazineInspectionWidget_Setup_Guide.md) | Magazine UI widget setup |

---

## Technical References

### Data References

| Document | Description |
|----------|-------------|
| [Data/WeaponSSOTReference.md](Data/WeaponSSOTReference.md) | Weapon DataTable structure |

### Checklists

| Document | Description |
|----------|-------------|
| [EquipmentModule_ReviewChecklist.md](EquipmentModule_ReviewChecklist.md) | Equipment module review |
| [EquipmentWidget_ImplementationPlan.md](EquipmentWidget_ImplementationPlan.md) | Equipment UI implementation |

---

## Game Design

| Document | Description |
|----------|-------------|
| [GameDesign/ExtractionPvE_DesignDocument.md](GameDesign/ExtractionPvE_DesignDocument.md) | Core game loop design |
| [GameDesign/Narrative_Design.md](GameDesign/Narrative_Design.md) | Story and lore |

---

## Devlog Snippets

### Ready-to-Use Excerpts

#### Tarkov-Style Ammo System

```
We implemented a realistic ammunition system inspired by Escape from Tarkov:

- Magazines are physical inventory items that take up grid space
- Each magazine tracks individual rounds (not just a counter)
- Players can mix ammo types: AP rounds on top for armor, tracers every 5th round
- QuickSlots (keys 1-4) provide fast tactical magazine access mid-combat
- Full Tarkov-style reload: tactical (fast swap) vs empty reload (rack slide)

Technical highlights:
- MagazineComponent manages weapon's loaded magazine
- QuickSlotComponent on character for 4 quick-access slots
- EventBus pattern keeps UI, DataStore, and Components in sync
- GAS abilities (GA_Reload, GA_QuickSlot) handle multiplayer replication
```

#### Weapon Fire System

```
Complete weapon fire system with Gameplay Ability System (GAS):

- Single shot, burst fire, and full-auto modes
- Spread calculation based on stance, movement, and consecutive shots
- Recoil system with per-ammo modifiers (subsonic = less kick)
- Async line traces for performance
- Full multiplayer replication via GAS

All classes follow SOLID principles with clear separation:
- BaseFireAbility: Common fire logic
- SingleShotAbility: Semi-auto weapons
- AutoFireAbility: Full-auto with heat buildup
- BurstFireAbility: 3-round burst mode
```

#### Movement System

```
GAS-powered movement abilities with stamina integration:

- Sprint: Drains stamina, affects weapon accuracy
- Crouch: Reduces spread, slower movement
- Prone: Minimal spread, very slow, vulnerable
- Jump: Stamina cost, height based on encumbrance
- Vault: Context-aware obstacle traversal

All movement states properly replicated in multiplayer.
Stamina managed through GAS GameplayEffects.
```

#### Grenade DoT System (NEW)

```
Tarkov-style Damage-over-Time effects for grenades:

BLEEDING (Fragmentation Grenades):
- Only applies when target armor = 0 (shrapnel blocked by armor)
- Light Bleed: 1-2 HP/sec (bandages can heal)
- Heavy Bleed: 3-5 HP/sec (requires medkit/surgery)
- Infinite duration - must be healed

BURNING (Incendiary Grenades):
- BYPASSES armor entirely (fire burns through everything)
- Damages BOTH Armor AND Health simultaneously
- Fixed duration (5-10 seconds, will expire)

Technical highlights:
- DoTService tracks active effects with EventBus integration
- Native GameplayTags for all DoT states/events
- UI widgets subscribe to DoT.Applied/Removed events
- Full GAS integration with SetByCaller magnitudes
```

---

## Directory Structure

```
Documentation/
├── INDEX.md                              ← YOU ARE HERE
├── Completed/                            ← Finished features
│   ├── TarkovStyle_Ammo_System_Design.md
│   ├── WeaponGASMigrationProgress.md
│   └── MovementAbilities_AuditReport.md
├── Plans/                                ← Active development
│   ├── TarkovStyle_Recoil_System_Design.md
│   ├── WeaponSlotSwitching_Design.md
│   ├── SSOT_AttributeSet_DataTable_Integration.md
│   ├── InstanceZone_System_Design.md
│   ├── ItemUseSystem_Pipeline.md
│   ├── DoT_System_ImplementationPlan.md       ← NEW
│   └── DebuffWidget_System_Plan.md            ← NEW
├── GAS/                                  ← GAS-specific docs
│   ├── AttributeSystem_DeveloperGuide.md
│   ├── MovementAbilities_SetupGuide.md
│   ├── WeaponAbilityDevelopmentGuide.md
│   └── ...
├── GameDesign/                           ← GDD documents
│   ├── ExtractionPvE_DesignDocument.md
│   └── Narrative_Design.md
├── Guides/                               ← Setup guides
│   └── MagazineInspectionWidget_Setup_Guide.md
├── Data/                                 ← Data references
│   └── WeaponSSOTReference.md
└── [Root guides]                         ← Core documentation
    ├── SuspenseCoreDeveloperGuide.md
    ├── AnimationSystem_DeveloperGuide.md
    ├── UISystem_DeveloperGuide.md
    └── WeaponAbilities_DeveloperGuide.md
```

---

## Recent Commits (Documentation)

```
2818b97 feat(GAS): Add AmmoRecoilModifier and document Tarkov-style recoil system
a12c719 fix(GAS): Replace all RequestGameplayTag() with native tags
fd5b567 Merge: Add weapon documentation
97bb699 Merge: GAS documentation audit
85119ba docs: Update migration progress with compilation fixes
429a893 feat(GAS): Complete Weapon Fire System migration
2e73ed5 docs(GAS): Add weapon ability development documentation
d3cc168 docs(GAS): Add comprehensive weapon switch system audit
c735b2b docs: Add Phase 8 DataStore EventBus sync + WeaponSlotSwitching design
667e8e0 docs(GAS): Add AttributeSystem guide and update Movement docs
27b0092 docs(GameDesign): Add detailed level design and UE 5.5 optimization
4ca4730 docs(GameDesign): Add Extraction PvE concept document
```

---

**End of Index**
