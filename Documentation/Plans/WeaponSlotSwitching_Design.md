# Weapon Slot Switching System Design

## Overview

Design document for implementing weapon slot switching ability in SuspenseCore.
Based on existing QuickSlot ability architecture.

**Author:** Claude Code
**Date:** 2026-01-08
**Status:** PLANNING (Ready for implementation)
**Related:** `TarkovStyle_Ammo_System_Design.md`

---

## 1. Key Bindings (CORRECTED)

### Current Layout

| Key | Current Use | New Use |
|-----|-------------|---------|
| `1` | FREE | **Primary Weapon** (Slot 0) |
| `2` | FREE | **Secondary Weapon** (Slot 1) |
| `3` | FREE | **Sidearm/Holster** (Slot 2) |
| `4` | QuickSlot 1 | QuickSlot 1 (magazines) |
| `5` | QuickSlot 2 | QuickSlot 2 (medkits) |
| `6` | QuickSlot 3 | QuickSlot 3 (grenades) |
| `7` | QuickSlot 4 | QuickSlot 4 (misc) |
| `V` | FREE | **Melee/Knife** (Slot 3) |

### Weapon Slots Summary

| Slot Index | Type | Key | Socket |
|------------|------|-----|--------|
| 0 | PrimaryWeapon | `1` | weapon_r |
| 1 | SecondaryWeapon | `2` | spine_03 |
| 2 | Holster (Sidearm) | `3` | spine_03 |
| 3 | Scabbard (Melee) | `V` | spine_03 |

---

## 2. Architecture: Following QuickSlot Pattern

### 2.1 QuickSlot Ability Pipeline (Reference)

```
┌─────────────────────────────────────────────────────────────────────┐
│                    QUICKSLOT ABILITY PIPELINE                        │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  1. INPUT                                                           │
│     Key 4 pressed → ESuspenseCoreAbilityInputID::QuickSlot1         │
│                                                                     │
│  2. GAS ACTIVATION                                                  │
│     ASC->TryActivateAbility(UGA_QuickSlot1Use)                     │
│     - AbilityInputID = QuickSlot1                                   │
│     - AbilityTags = SuspenseCoreTags::Ability::QuickSlot::Slot1     │
│                                                                     │
│  3. INTERFACE LOOKUP                                                │
│     GetQuickSlotProvider() → ISuspenseCoreQuickSlotProvider*        │
│     - Check AvatarActor implements interface                        │
│     - Check components implement interface                          │
│                                                                     │
│  4. CAN ACTIVATE                                                    │
│     ISuspenseCoreQuickSlotProvider::Execute_IsSlotReady(SlotIndex)  │
│                                                                     │
│  5. ACTIVATE                                                        │
│     ISuspenseCoreQuickSlotProvider::Execute_UseQuickSlot(SlotIndex) │
│                                                                     │
│  6. END ABILITY (instant)                                           │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 2.2 Weapon Switch Pipeline (NEW - Same Pattern)

```
┌─────────────────────────────────────────────────────────────────────┐
│                    WEAPON SWITCH ABILITY PIPELINE                    │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  1. INPUT                                                           │
│     Key 1 pressed → ESuspenseCoreAbilityInputID::WeaponSlot1        │
│     Key V pressed → ESuspenseCoreAbilityInputID::MeleeWeapon        │
│                                                                     │
│  2. GAS ACTIVATION                                                  │
│     ASC->TryActivateAbility(UGA_WeaponSwitch_Primary)               │
│     - AbilityInputID = WeaponSlot1                                  │
│     - AbilityTags = SuspenseCoreTags::Ability::WeaponSlot::Primary  │
│                                                                     │
│  3. INTERFACE LOOKUP                                                │
│     GetEquipmentDataProvider() → ISuspenseCoreEquipmentDataProvider*│
│     - Check PlayerState for DataStore                               │
│     - Or check AvatarActor components                               │
│                                                                     │
│  4. CAN ACTIVATE                                                    │
│     - Check slot has weapon: IsSlotOccupied(TargetSlot)             │
│     - Check not already active: GetActiveWeaponSlot() != TargetSlot │
│     - Check not blocked: !HasMatchingGameplayTag(State.Reloading)   │
│                                                                     │
│  5. ACTIVATE                                                        │
│     ISuspenseCoreEquipmentDataProvider::SetActiveWeaponSlot(Target) │
│     EventBus->Publish(TAG_Equipment_Event_WeaponSlotSwitched)       │
│                                                                     │
│  6. END ABILITY (instant for now, animation later)                  │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 3. Files to Create

### 3.1 New Ability Header

**File:** `Source/GAS/Public/SuspenseCore/Abilities/Equipment/GA_WeaponSwitch.h`

```cpp
// GA_WeaponSwitch.h
// Weapon slot switching ability
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE:
// - Uses ISuspenseCoreEquipmentDataProvider interface (BridgeSystem)
// - No direct dependency on EquipmentSystem
// - Follows same pattern as SuspenseCoreQuickSlotAbility
//
// USAGE:
// Create concrete subclasses for each weapon slot:
// - GA_WeaponSwitch_Primary (Key 1 → Slot 0)
// - GA_WeaponSwitch_Secondary (Key 2 → Slot 1)
// - GA_WeaponSwitch_Sidearm (Key 3 → Slot 2)
// - GA_WeaponSwitch_Melee (Key V → Slot 3)

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Abilities/Base/SuspenseCoreAbility.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "GA_WeaponSwitch.generated.h"

class ISuspenseCoreEquipmentDataProvider;

/**
 * UGA_WeaponSwitch
 *
 * Base class for weapon slot switching abilities.
 * Triggers switch to assigned weapon slot.
 *
 * @see ISuspenseCoreEquipmentDataProvider
 */
UCLASS()
class GAS_API UGA_WeaponSwitch : public USuspenseCoreAbility
{
    GENERATED_BODY()

public:
    UGA_WeaponSwitch();

    /** Target weapon slot index (0-3) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|WeaponSwitch",
        meta = (ClampMin = "0", ClampMax = "3"))
    int32 TargetSlotIndex;

protected:
    virtual bool CanActivateAbility(...) const override;
    virtual void ActivateAbility(...) override;

private:
    ISuspenseCoreEquipmentDataProvider* GetEquipmentDataProvider() const;
};

//==================================================================
// Concrete Weapon Slot Abilities
//==================================================================

/** Primary Weapon (Key 1 → Slot 0) */
UCLASS()
class GAS_API UGA_WeaponSwitch_Primary : public UGA_WeaponSwitch
{
    GENERATED_BODY()
public:
    UGA_WeaponSwitch_Primary()
    {
        TargetSlotIndex = 0;
        AbilityInputID = ESuspenseCoreAbilityInputID::WeaponSlot1;
        FGameplayTagContainer Tags;
        Tags.AddTag(SuspenseCoreTags::Ability::WeaponSlot::Primary);
        SetAssetTags(Tags);
    }
};

/** Secondary Weapon (Key 2 → Slot 1) */
UCLASS()
class GAS_API UGA_WeaponSwitch_Secondary : public UGA_WeaponSwitch
{
    GENERATED_BODY()
public:
    UGA_WeaponSwitch_Secondary()
    {
        TargetSlotIndex = 1;
        AbilityInputID = ESuspenseCoreAbilityInputID::WeaponSlot2;
        FGameplayTagContainer Tags;
        Tags.AddTag(SuspenseCoreTags::Ability::WeaponSlot::Secondary);
        SetAssetTags(Tags);
    }
};

/** Sidearm (Key 3 → Slot 2) */
UCLASS()
class GAS_API UGA_WeaponSwitch_Sidearm : public UGA_WeaponSwitch
{
    GENERATED_BODY()
public:
    UGA_WeaponSwitch_Sidearm()
    {
        TargetSlotIndex = 2;
        AbilityInputID = ESuspenseCoreAbilityInputID::WeaponSlot3;
        FGameplayTagContainer Tags;
        Tags.AddTag(SuspenseCoreTags::Ability::WeaponSlot::Sidearm);
        SetAssetTags(Tags);
    }
};

/** Melee/Knife (Key V → Slot 3) */
UCLASS()
class GAS_API UGA_WeaponSwitch_Melee : public UGA_WeaponSwitch
{
    GENERATED_BODY()
public:
    UGA_WeaponSwitch_Melee()
    {
        TargetSlotIndex = 3;
        AbilityInputID = ESuspenseCoreAbilityInputID::MeleeWeapon; // NEW - need to add
        FGameplayTagContainer Tags;
        Tags.AddTag(SuspenseCoreTags::Ability::WeaponSlot::Melee);
        SetAssetTags(Tags);
    }
};
```

### 3.2 New Ability Implementation

**File:** `Source/GAS/Private/SuspenseCore/Abilities/Equipment/GA_WeaponSwitch.cpp`

```cpp
// GA_WeaponSwitch.cpp
#include "SuspenseCore/Abilities/Equipment/GA_WeaponSwitch.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "GameFramework/PlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeaponSwitch, Log, All);

UGA_WeaponSwitch::UGA_WeaponSwitch()
{
    TargetSlotIndex = 0;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::NonInstanced;

    // Blocking tags
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Stunned")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Reloading")));

    bPublishAbilityEvents = true;
}

bool UGA_WeaponSwitch::CanActivateAbility(...) const
{
    if (!Super::CanActivateAbility(...))
    {
        return false;
    }

    ISuspenseCoreEquipmentDataProvider* Provider =
        const_cast<UGA_WeaponSwitch*>(this)->GetEquipmentDataProvider();

    if (!Provider)
    {
        return false;
    }

    // Check slot has weapon
    if (!Provider->IsSlotOccupied(TargetSlotIndex))
    {
        return false;
    }

    // Check not already active
    if (Provider->GetActiveWeaponSlot() == TargetSlotIndex)
    {
        return false;
    }

    return true;
}

void UGA_WeaponSwitch::ActivateAbility(...)
{
    if (!CommitAbility(...))
    {
        EndAbility(..., true, true);
        return;
    }

    ISuspenseCoreEquipmentDataProvider* Provider = GetEquipmentDataProvider();
    if (Provider)
    {
        int32 PreviousSlot = Provider->GetActiveWeaponSlot();
        bool bSuccess = Provider->SetActiveWeaponSlot(TargetSlotIndex);

        if (bSuccess)
        {
            // Publish EventBus event
            if (USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(GetAvatarActorFromActorInfo()))
            {
                if (USuspenseCoreEventBus* EventBus = EventManager->GetEventBus())
                {
                    FSuspenseCoreEventData EventData;
                    EventData.SetInt(TEXT("PreviousSlot"), PreviousSlot);
                    EventData.SetInt(TEXT("NewSlot"), TargetSlotIndex);
                    EventBus->Publish(TAG_Equipment_Event_WeaponSlot_Switched, EventData);
                }
            }
        }

        UE_LOG(LogWeaponSwitch, Log, TEXT("Weapon switch to slot %d: %s"),
            TargetSlotIndex, bSuccess ? TEXT("Success") : TEXT("Failed"));
    }

    EndAbility(..., true, false);
}

ISuspenseCoreEquipmentDataProvider* UGA_WeaponSwitch::GetEquipmentDataProvider() const
{
    AActor* AvatarActor = GetAvatarActorFromActorInfo();
    if (!AvatarActor)
    {
        return nullptr;
    }

    // DataStore is on PlayerState
    if (APawn* Pawn = Cast<APawn>(AvatarActor))
    {
        if (APlayerState* PS = Pawn->GetPlayerState())
        {
            TArray<UActorComponent*> Components;
            PS->GetComponents(Components);
            for (UActorComponent* Comp : Components)
            {
                if (Comp && Comp->GetClass()->ImplementsInterface(
                    USuspenseCoreEquipmentDataProvider::StaticClass()))
                {
                    return Cast<ISuspenseCoreEquipmentDataProvider>(Comp);
                }
            }
        }
    }

    // Fallback: check AvatarActor components
    TArray<UActorComponent*> Components;
    AvatarActor->GetComponents(Components);
    for (UActorComponent* Comp : Components)
    {
        if (Comp && Comp->GetClass()->ImplementsInterface(
            USuspenseCoreEquipmentDataProvider::StaticClass()))
        {
            return Cast<ISuspenseCoreEquipmentDataProvider>(Comp);
        }
    }

    return nullptr;
}
```

---

## 4. Native Tags to Add

### 4.1 SuspenseCoreGameplayTags.h

**File:** `Source/BridgeSystem/Public/SuspenseCore/Tags/SuspenseCoreGameplayTags.h`

Add in `namespace Ability`:

```cpp
// Weapon slot switching abilities (Keys 1-3, V)
namespace WeaponSlot
{
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Primary);    // Key 1
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Secondary);  // Key 2
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sidearm);    // Key 3
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Melee);      // Key V
}
```

### 4.2 SuspenseCoreGameplayTags.cpp

**File:** `Source/BridgeSystem/Private/SuspenseCore/Tags/SuspenseCoreGameplayTags.cpp`

```cpp
// Weapon slot abilities
UE_DEFINE_GAMEPLAY_TAG(SuspenseCoreTags::Ability::WeaponSlot::Primary,
    "Ability.WeaponSlot.Primary");
UE_DEFINE_GAMEPLAY_TAG(SuspenseCoreTags::Ability::WeaponSlot::Secondary,
    "Ability.WeaponSlot.Secondary");
UE_DEFINE_GAMEPLAY_TAG(SuspenseCoreTags::Ability::WeaponSlot::Sidearm,
    "Ability.WeaponSlot.Sidearm");
UE_DEFINE_GAMEPLAY_TAG(SuspenseCoreTags::Ability::WeaponSlot::Melee,
    "Ability.WeaponSlot.Melee");
```

### 4.3 SuspenseCoreEquipmentNativeTags.h

**File:** `Source/EquipmentSystem/Public/SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h`

Add new event tag:

```cpp
// Weapon slot switched event
EQUIPMENTSYSTEM_API extern FGameplayTag TAG_Equipment_Event_WeaponSlot_Switched;
```

### 4.4 SuspenseCoreEquipmentNativeTags.cpp

```cpp
FGameplayTag TAG_Equipment_Event_WeaponSlot_Switched = FGameplayTag::RequestGameplayTag(
    FName(TEXT("SuspenseCore.Event.Equipment.WeaponSlot.Switched")));
```

---

## 5. Input ID to Add

### 5.1 SuspenseCoreAbilityInputID.h

**File:** `Source/BridgeSystem/Public/SuspenseCore/Input/SuspenseCoreAbilityInputID.h`

Add new enum value:

```cpp
// Already exists:
WeaponSlot1   UMETA(DisplayName="Weapon Slot 1"),  // Key 1
WeaponSlot2   UMETA(DisplayName="Weapon Slot 2"),  // Key 2
WeaponSlot3   UMETA(DisplayName="Weapon Slot 3"),  // Key 3

// ADD NEW:
MeleeWeapon   UMETA(DisplayName="Melee Weapon"),   // Key V
```

---

## 6. Grant Abilities to Character

### 6.1 Where to Grant

Check existing pattern in `SuspenseCoreCharacter.cpp` or `SuspenseCorePlayerState.cpp`:

```cpp
// Grant weapon switch abilities
if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
{
    ASC->GiveAbility(FGameplayAbilitySpec(UGA_WeaponSwitch_Primary::StaticClass(), 1));
    ASC->GiveAbility(FGameplayAbilitySpec(UGA_WeaponSwitch_Secondary::StaticClass(), 1));
    ASC->GiveAbility(FGameplayAbilitySpec(UGA_WeaponSwitch_Sidearm::StaticClass(), 1));
    ASC->GiveAbility(FGameplayAbilitySpec(UGA_WeaponSwitch_Melee::StaticClass(), 1));
}
```

---

## 7. Implementation Checklist

### Phase 1: Tags & Input
- [ ] Add `WeaponSlot` namespace to `SuspenseCoreGameplayTags.h/.cpp`
- [ ] Add `TAG_Equipment_Event_WeaponSlot_Switched` to native tags
- [ ] Add `MeleeWeapon` to `ESuspenseCoreAbilityInputID`

### Phase 2: Ability Class
- [ ] Create `GA_WeaponSwitch.h` with base class + 4 concrete classes
- [ ] Create `GA_WeaponSwitch.cpp` with implementation

### Phase 3: Integration
- [ ] Grant abilities in Character/PlayerState
- [ ] Setup input bindings (1, 2, 3, V)

### Phase 4: Testing
- [ ] Press 1 → switches to Primary (slot 0)
- [ ] Press 2 → switches to Secondary (slot 1)
- [ ] Press 3 → switches to Sidearm (slot 2)
- [ ] Press V → switches to Melee (slot 3)
- [ ] Cannot switch while reloading
- [ ] EventBus publishes WeaponSlot.Switched

---

## 8. Key Differences from QuickSlot

| Aspect | QuickSlot | WeaponSwitch |
|--------|-----------|--------------|
| Interface | `ISuspenseCoreQuickSlotProvider` | `ISuspenseCoreEquipmentDataProvider` |
| Location | Character component | PlayerState (DataStore) |
| Keys | 4-7 | 1-3, V |
| Action | UseQuickSlot() | SetActiveWeaponSlot() |
| Slots | 12-15 (DataStore) | 0-3 (DataStore) |

---

## 9. Files Summary

### Files to CREATE

| File | Description |
|------|-------------|
| `Source/GAS/Public/SuspenseCore/Abilities/Equipment/GA_WeaponSwitch.h` | Base + concrete abilities |
| `Source/GAS/Private/SuspenseCore/Abilities/Equipment/GA_WeaponSwitch.cpp` | Implementation |

### Files to MODIFY

| File | Changes |
|------|---------|
| `SuspenseCoreGameplayTags.h` | Add `Ability::WeaponSlot` namespace |
| `SuspenseCoreGameplayTags.cpp` | Define weapon slot tags |
| `SuspenseCoreEquipmentNativeTags.h` | Add `TAG_Equipment_Event_WeaponSlot_Switched` |
| `SuspenseCoreEquipmentNativeTags.cpp` | Define the tag |
| `SuspenseCoreAbilityInputID.h` | Add `MeleeWeapon` enum value |
| `SuspenseCoreCharacter.cpp` (or PlayerState) | Grant abilities |

---

## 10. Architecture Compliance

| Pattern | Implementation |
|---------|----------------|
| **SOLID/SRP** | Ability only handles switching, DataStore handles state |
| **Interface-based** | Uses `ISuspenseCoreEquipmentDataProvider`, not concrete `DataStore` |
| **EventBus** | Publishes `WeaponSlot.Switched` for UI/systems |
| **Native Tags** | All tags defined in central locations |
| **Same pattern as QuickSlot** | Easy to understand and maintain |

---

**Last Updated:** 2026-01-08
**Version:** 2.0 (Corrected bindings, full pipeline)
