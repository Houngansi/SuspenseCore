# Weapon Slot Switching System Design

## Overview

Design document for implementing weapon slot switching ability in SuspenseCore.
Inspired by Escape from Tarkov and modern AAA shooters.

**Author:** Claude Code
**Date:** 2026-01-08
**Status:** PLANNING
**Related:** `TarkovStyle_Ammo_System_Design.md`

---

## 1. Reference: How Major Studios & Tarkov Do It

### 1.1 Escape from Tarkov Mechanics

**Sources:**
- [Tarkov Keybindings](https://gist.github.com/TheDonDope/8327101ead7758b9bfc6e86b9a776f80)
- [GINX Quick Swap Guide](https://www.ginx.tv/en/tarkov/how-to-quick-swap)
- [GameSpot Controls Guide](https://www.gamespot.com/articles/escape-from-tarkov-controls-guide-hotkeys-and-keybindings/1100-6472601/)

| Input | Action | Notes |
|-------|--------|-------|
| `1` | Primary Weapon | Assault rifle, DMR, sniper |
| `2` | Secondary Weapon | SMG, shotgun on back |
| `3` | Sidearm (Pistol) | Quick draw from holster |
| `V` (double tap) | Quick Knife | Stab + return to previous weapon |
| `Mouse4` | Shoulder Swap | Switch weapon to left/right shoulder |
| `Tab + number` | Quick Slot Item | Use consumable/grenade |

**Tarkov's Quick Swap Feature (Patch 13.5):**
- Reduced time switching Primary → Sidearm (pistol only)
- Requires chambered round in pistol
- Critical for snipers needing CQB backup
- NOT instant - has animation time

**Key Tarkov Design Decisions:**
1. Weapon must be chambered to fire immediately after switch
2. Holster animations are realistic (not instant)
3. Weight/ergonomics affect switch speed
4. Sling attachment affects carry position

### 1.2 AAA Shooter Patterns

**Sources:**
- [80.lv UE5 Weapon Switching](https://80.lv/articles/smooth-weapon-switching-holster-system-made-for-a-ue5-shooter)
- [Advanced Dual Weapon System](https://github.com/unrealroshan/Advanced-Dual-Weapon-System-UE5)
- [GASShooter Sample](https://github.com/tranek/GASShooter)

**Common Patterns:**

| Game Style | Switching Method | Animation Time |
|------------|------------------|----------------|
| **Call of Duty** | Instant (Y/Triangle) | ~0.3-0.5s |
| **Battlefield** | Cycle (scroll) or direct (1-4) | ~0.5-0.8s |
| **Tarkov/ARMA** | Realistic animations | ~1.0-2.0s |
| **Counter-Strike** | Number keys + scroll | ~0.5s |

**Best Practices from AAA:**
1. **State Machine** for weapon states (Holstered → Drawing → Ready → Firing)
2. **Animation Notify Events** for equip/unequip timing
3. **GAS Integration** for ability-based switching with cooldowns/costs
4. **Prediction** for responsive feel in multiplayer
5. **Socket-based attachment** for visual positioning

---

## 2. Current SuspenseCore Architecture

### 2.1 Equipment Slot Configuration

```
Slot 0:  PrimaryWeapon    (AR, DMR, SR, Shotgun, LMG)  - socket "weapon_r"
Slot 1:  SecondaryWeapon  (SMG, Shotgun, LMG)          - socket "spine_03"
Slot 2:  Holster          (Pistols)                     - socket "spine_03"
Slot 3:  Scabbard         (Melee, Knives)               - socket "spine_03"
Slot 4-11: Gear/Armor
Slot 12-15: QuickSlots (consumables, magazines)
Slot 16: Armband
```

### 2.2 Key Files

| File | Purpose |
|------|---------|
| `SuspenseCoreEquipmentDataStore.h` | `ActiveWeaponSlot` storage, `SetActiveWeaponSlot()` |
| `SuspenseCoreWeaponStateManager.h` | Per-slot state machines |
| `SuspenseCoreEquipmentOperationExecutor.h` | `QuickSwitch` operation type |
| `SuspenseCoreAbilityInputID.h` | `WeaponSlot1-5`, `NextWeapon`, `PrevWeapon`, `QuickSwitch` |
| `SuspenseCoreQuickSlotAbility.h` | Template for slot-based abilities |
| `SuspenseCoreEquipmentNativeTags.h` | `Equipment.Event.SlotSwitched` |

### 2.3 Existing Input IDs (Ready to Use)

```cpp
// ESuspenseCoreAbilityInputID (SuspenseCoreAbilityInputID.h)
WeaponSlot1,    // Key 1 → Primary
WeaponSlot2,    // Key 2 → Secondary
WeaponSlot3,    // Key 3 → Sidearm
WeaponSlot4,    // Key 4 → Melee
WeaponSlot5,    // Key 5 → (reserved)
NextWeapon,     // Scroll Up / MouseWheel
PrevWeapon,     // Scroll Down
QuickSwitch,    // Q or similar - toggle last two weapons
```

### 2.4 Missing Components

- [ ] `GA_WeaponSwitch` ability (does NOT exist yet)
- [ ] Animation montages for draw/holster
- [ ] WeaponStateManager integration with abilities
- [ ] UI feedback for weapon switching

---

## 3. Proposed Implementation

### 3.1 GA_WeaponSwitch Ability

```cpp
// Source/GAS/Public/SuspenseCore/Abilities/Equipment/GA_WeaponSwitch.h

UCLASS()
class GAS_API UGA_WeaponSwitch : public USuspenseCoreGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_WeaponSwitch();

    // Target slot to switch to (set via InputID mapping)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 TargetSlotIndex = INDEX_NONE;

    // If true, toggle between last two weapons
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    bool bIsQuickSwitch = false;

protected:
    virtual bool CanActivateAbility(...) const override;
    virtual void ActivateAbility(...) override;
    virtual void EndAbility(...) override;

    // Animation support
    UPROPERTY(EditDefaultsOnly, Category = "Animation")
    UAnimMontage* HolsterMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Animation")
    UAnimMontage* DrawMontage;

private:
    void OnHolsterComplete();
    void OnDrawComplete();

    int32 PreviousSlotIndex = INDEX_NONE;
};
```

### 3.2 Concrete Ability Classes

```cpp
// GA_WeaponSwitch_Primary.h
UCLASS()
class UGA_WeaponSwitch_Primary : public UGA_WeaponSwitch
{
    UGA_WeaponSwitch_Primary() { TargetSlotIndex = 0; }
};

// GA_WeaponSwitch_Secondary.h
UCLASS()
class UGA_WeaponSwitch_Secondary : public UGA_WeaponSwitch
{
    UGA_WeaponSwitch_Secondary() { TargetSlotIndex = 1; }
};

// GA_WeaponSwitch_Sidearm.h
UCLASS()
class UGA_WeaponSwitch_Sidearm : public UGA_WeaponSwitch
{
    UGA_WeaponSwitch_Sidearm() { TargetSlotIndex = 2; }
};

// GA_WeaponSwitch_Melee.h
UCLASS()
class UGA_WeaponSwitch_Melee : public UGA_WeaponSwitch
{
    UGA_WeaponSwitch_Melee() { TargetSlotIndex = 3; }
};

// GA_WeaponSwitch_Quick.h (toggle last two)
UCLASS()
class UGA_WeaponSwitch_Quick : public UGA_WeaponSwitch
{
    UGA_WeaponSwitch_Quick() { bIsQuickSwitch = true; }
};
```

### 3.3 Activation Flow

```
┌──────────────────────────────────────────────────────────────────┐
│                    WEAPON SWITCH FLOW                             │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Input (Key 1-4)                                                 │
│       ↓                                                          │
│  GAS: TryActivateAbility(GA_WeaponSwitch_Primary)               │
│       ↓                                                          │
│  CanActivateAbility():                                           │
│    - Check not already switching                                 │
│    - Check target slot has weapon                                │
│    - Check not in blocking state (reloading, firing)             │
│       ↓                                                          │
│  ActivateAbility():                                              │
│    1. WeaponStateManager->RequestStateTransition(Holstering)     │
│    2. Play HolsterMontage                                        │
│    3. Wait for AnimNotify "HolsterComplete"                      │
│       ↓                                                          │
│  OnHolsterComplete():                                            │
│    1. DataStore->SetActiveWeaponSlot(TargetSlotIndex)            │
│    2. EventBus->Publish(SlotSwitched)                            │
│    3. Update weapon attachment (detach old, attach new)          │
│    4. Play DrawMontage                                           │
│    5. Wait for AnimNotify "DrawComplete"                         │
│       ↓                                                          │
│  OnDrawComplete():                                               │
│    1. WeaponStateManager->TransitionTo(Ready)                    │
│    2. EndAbility()                                               │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

### 3.4 EventBus Integration

```cpp
// New event for weapon switch
TAG_Equipment_Event_WeaponSwitchStarted   // When holster begins
TAG_Equipment_Event_WeaponSwitchCompleted // When draw completes
TAG_Equipment_Event_SlotSwitched          // Already exists - DataStore change

// Event data
EventData.SetInt("PreviousSlot", OldSlot);
EventData.SetInt("NewSlot", NewSlot);
EventData.SetString("WeaponID", NewWeaponItemID);
```

### 3.5 Animation Montage Requirements

| Montage | Duration | Notify Events |
|---------|----------|---------------|
| `AM_Holster_Rifle` | ~0.5s | `HolsterComplete` at end |
| `AM_Holster_Pistol` | ~0.3s | `HolsterComplete` at end |
| `AM_Draw_Rifle` | ~0.6s | `DrawComplete` at end, `WeaponReady` when usable |
| `AM_Draw_Pistol` | ~0.4s | `DrawComplete` at end |
| `AM_QuickSwitch_ToPistol` | ~0.8s | Combined holster+draw |

---

## 4. Input Binding Setup

### 4.1 Enhanced Input Actions

```cpp
// IA_WeaponSlot1 (Primary)
// IA_WeaponSlot2 (Secondary)
// IA_WeaponSlot3 (Sidearm)
// IA_WeaponSlot4 (Melee)
// IA_QuickSwitch (Toggle)
// IA_NextWeapon (Scroll up)
// IA_PrevWeapon (Scroll down)
```

### 4.2 Default Key Mappings

| Key | Action | Slot |
|-----|--------|------|
| `1` | Primary | 0 |
| `2` | Secondary | 1 |
| `3` | Sidearm | 2 |
| `4` | Melee | 3 |
| `Q` | QuickSwitch | Last used |
| `Scroll Up` | NextWeapon | +1 |
| `Scroll Down` | PrevWeapon | -1 |

---

## 5. Files to Create/Modify

### 5.1 New Files

| File | Description |
|------|-------------|
| `Source/GAS/Public/SuspenseCore/Abilities/Equipment/GA_WeaponSwitch.h` | Base weapon switch ability |
| `Source/GAS/Private/SuspenseCore/Abilities/Equipment/GA_WeaponSwitch.cpp` | Implementation |
| `Source/GAS/Public/SuspenseCore/Abilities/Equipment/GA_WeaponSwitch_Primary.h` | Slot 0 |
| `Source/GAS/Public/SuspenseCore/Abilities/Equipment/GA_WeaponSwitch_Secondary.h` | Slot 1 |
| `Source/GAS/Public/SuspenseCore/Abilities/Equipment/GA_WeaponSwitch_Sidearm.h` | Slot 2 |
| `Source/GAS/Public/SuspenseCore/Abilities/Equipment/GA_WeaponSwitch_Melee.h` | Slot 3 |
| `Source/GAS/Public/SuspenseCore/Abilities/Equipment/GA_WeaponSwitch_Quick.h` | Toggle |

### 5.2 Files to Modify

| File | Changes |
|------|---------|
| `SuspenseCoreEquipmentNativeTags.h/.cpp` | Add WeaponSwitch event tags |
| `SuspenseCorePlayerController.cpp` | Bind WeaponSlot inputs to abilities |
| `SuspenseCoreCharacter.cpp` | Grant weapon switch abilities on spawn |

### 5.3 DataTable/Config Files

| Asset | Purpose |
|-------|---------|
| `DT_WeaponSwitchTimes` | Per-weapon holster/draw times |
| `IMC_DefaultPlayer` | Input mapping context |

---

## 6. Implementation Phases

### Phase 1: Core Ability
- [ ] Create `GA_WeaponSwitch` base class
- [ ] Implement `CanActivateAbility` checks
- [ ] Implement `ActivateAbility` with DataStore update
- [ ] Add EventBus publication

### Phase 2: Animation Integration
- [ ] Add montage support
- [ ] Implement notify handlers
- [ ] Create placeholder montages

### Phase 3: Input Binding
- [ ] Setup Enhanced Input actions
- [ ] Bind in PlayerController
- [ ] Grant abilities to character

### Phase 4: WeaponStateManager Integration
- [ ] Hook into state transitions
- [ ] Add blocking during switch
- [ ] Handle interruptions

### Phase 5: Polish
- [ ] QuickSwitch (toggle) logic
- [ ] NextWeapon/PrevWeapon cycling
- [ ] UI weapon indicator updates
- [ ] Sound effects

---

## 7. Testing Checklist

- [ ] Press 1 → equips Primary weapon
- [ ] Press 2 → equips Secondary weapon
- [ ] Press 3 → equips Sidearm
- [ ] Press 4 → equips Melee
- [ ] Press Q → toggles between last two
- [ ] Cannot switch while reloading
- [ ] Cannot switch while firing
- [ ] Multiplayer replication works
- [ ] Animation plays correctly
- [ ] HUD updates weapon indicator

---

## 8. Architecture Notes

### SOLID Compliance

- **SRP**: GA_WeaponSwitch only handles switching, delegates to DataStore/StateManager
- **OCP**: Base class + concrete slots allows extension without modification
- **DI**: Uses interfaces (ISuspenseCoreEquipmentDataProvider) not concrete classes
- **EventBus**: All state changes published for loose coupling

### Threading

- DataStore access is thread-safe (critical section)
- Ability activation is on game thread
- Animation notifies are on game thread

---

**Last Updated:** 2026-01-08
