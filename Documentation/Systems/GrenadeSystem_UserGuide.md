# SuspenseCore Grenade System - Complete User Guide

> **Version:** 2.0 AAA Release
> **Last Updated:** 2026-01-19
> **Module:** EquipmentSystem + GAS
> **Status:** Production Ready

---

## Table of Contents

1. [Overview](#1-overview)
2. [Architecture](#2-architecture)
3. [Quick Start](#3-quick-start)
4. [Components Reference](#4-components-reference)
5. [Configuration Guide](#5-configuration-guide)
6. [Integration Guide](#6-integration-guide)
7. [Animation Setup](#7-animation-setup)
8. [Blueprint Examples](#8-blueprint-examples)
9. [Network Replication](#9-network-replication)
10. [Troubleshooting](#10-troubleshooting)

---

## 1. Overview

### What is the Grenade System?

The SuspenseCore Grenade System is a AAA-quality implementation for throwable weapons in MMO FPS games. It provides:

- **Tarkov-style cooking** - Hold to reduce fuse time
- **Three throw types** - Overhand, Underhand, Roll
- **Animation-driven phases** - Pin pull, Ready, Release via AnimNotify
- **Server-authoritative damage** - Prevents cheating
- **Client-predicted visuals** - Responsive feel
- **Full GAS integration** - Uses GameplayEffects for damage

### Key Features

| Feature | Description |
|---------|-------------|
| **Cooking Mechanic** | Hold grenade to reduce fuse time (risky!) |
| **Cancel Support** | Release before pin pull to cancel safely |
| **Impact Grenades** | Explode on contact with surfaces |
| **Multiple Types** | Frag, Smoke, Flashbang, Incendiary, Impact |
| **QuickSlot Integration** | Keys 4-7 for instant grenade access |
| **EventBus Events** | Full event system for UI/AI/Sound |

### Supported Grenade Types

| Type | Damage | Effect |
|------|--------|--------|
| **Fragmentation** | High | Area damage with shrapnel |
| **Smoke** | None | Visual obstruction |
| **Flashbang** | Low | Blind + Deafen effect |
| **Incendiary** | DoT | Fire damage over time |
| **Impact** | High | Explodes on hit (no fuse) |

---

## 2. Architecture

### System Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│                        GRENADE SYSTEM FLOW                          │
└─────────────────────────────────────────────────────────────────────┘

┌──────────────┐     ┌────────────────┐     ┌─────────────────────┐
│   Player     │────▶│ QuickSlot (4-7)│────▶│  ItemUseService     │
│   Input      │     │ or Hotkey (G)  │     │                     │
└──────────────┘     └────────────────┘     └──────────┬──────────┘
                                                       │
                                                       ▼
                                            ┌─────────────────────┐
                                            │  GrenadeHandler     │
                                            │  (Validate + Route) │
                                            └──────────┬──────────┘
                                                       │
                                           TryActivateAbilitiesByTag
                                                       │
                                                       ▼
                                            ┌─────────────────────┐
                                            │ GrenadeThrowAbility │
                                            │    (GAS Ability)    │
                                            └──────────┬──────────┘
                                                       │
                                              Play Montage with
                                              AnimNotify phases
                                                       │
                     ┌─────────────────────────────────┼─────────────────────────────────┐
                     │                                 │                                 │
                     ▼                                 ▼                                 ▼
          ┌─────────────────┐              ┌─────────────────┐              ┌─────────────────┐
          │  AN: PinPull    │              │   AN: Ready     │              │  AN: Release    │
          │  Arm grenade    │              │  Start cooking  │              │  Throw grenade  │
          └─────────────────┘              └─────────────────┘              └────────┬────────┘
                                                                                     │
                                                                          EventBus.Publish
                                                                          (SpawnRequested)
                                                                                     │
                                                                                     ▼
                                                                          ┌─────────────────┐
                                                                          │ GrenadeHandler  │
                                                                          │ OnSpawnRequested│
                                                                          └────────┬────────┘
                                                                                   │
                                                                            Spawn Actor
                                                                                   │
                                                                                   ▼
                                                                          ┌─────────────────┐
                                                                          │GrenadeProjectile│
                                                                          │  (Physics +     │
                                                                          │   Fuse Timer)   │
                                                                          └────────┬────────┘
                                                                                   │
                                                                             Fuse Expires
                                                                                   │
                                                                                   ▼
                                                                          ┌─────────────────┐
                                                                          │    Explode()    │
                                                                          │ Apply Damage    │
                                                                          │ Spawn Effects   │
                                                                          └─────────────────┘
```

### Core Classes

| Class | Module | Purpose |
|-------|--------|---------|
| `USuspenseCoreGrenadeThrowAbility` | GAS | GAS ability for throw animation/phases |
| `USuspenseCoreGrenadeHandler` | EquipmentSystem | Validates requests, spawns grenades |
| `ASuspenseCoreGrenadeProjectile` | EquipmentSystem | Physics projectile with fuse/damage |
| `USuspenseCoreQuickSlotComponent` | EquipmentSystem | Stores grenades in QuickSlots 0-3 |

### EventBus Events

| Event Tag | When Fired | Payload |
|-----------|------------|---------|
| `Event.Throwable.PrepareStarted` | Ability activated | GrenadeID, ThrowType |
| `Event.Throwable.PinPulled` | Pin pulled (armed) | GrenadeID, PinPulled=true |
| `Event.Throwable.CookingStarted` | Ready to throw | GrenadeID, CookTime |
| `Event.Throwable.Thrown` | Grenade released | GrenadeID, ThrowForce |
| `Event.Throwable.Cancelled` | Cancelled (pin not pulled) | GrenadeID |
| `Event.Throwable.SpawnRequested` | Request actor spawn | Location, Direction, Force |

---

## 3. Quick Start

### Step 1: Grant the Ability

In your PlayerState or Character class:

```cpp
void AMyPlayerState::GrantStartupAbilities()
{
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        // Grant GrenadeThrowAbility
        FGameplayAbilitySpec Spec(
            USuspenseCoreGrenadeThrowAbility::StaticClass(),
            1,  // Level
            INDEX_NONE,  // No input binding (uses TryActivateAbilitiesByTag)
            this);
        ASC->GiveAbility(Spec);
    }
}
```

### Step 2: Register the Handler

In your ItemUseService setup:

```cpp
void UMyGameInstance::SetupItemUseService()
{
    USuspenseCoreItemUseService* Service = GetItemUseService();

    // Create and register GrenadeHandler
    USuspenseCoreGrenadeHandler* GrenadeHandler = NewObject<USuspenseCoreGrenadeHandler>();
    GrenadeHandler->Initialize(GetDataManager(), GetEventBus());

    Service->RegisterHandler(GrenadeHandler);
}
```

### Step 3: Add Grenade to QuickSlot

```cpp
// Add grenade to QuickSlot index 0 (Key 4)
QuickSlotComponent->AssignItemToSlot(0, FName("Throwable_F1"));
```

### Step 4: Create Animation Montage

Create a montage with these AnimNotify names:
- `PinPull` - At pin pull moment
- `Ready` - When grenade is in throw position
- `Release` - At throw release moment

### Step 5: Configure Blueprint

1. Create `BP_GrenadeThrowAbility` inheriting from `USuspenseCoreGrenadeThrowAbility`
2. Set montage references (Overhand, Underhand, Roll)
3. Configure sounds (PinPull, Throw, Cancel)
4. Set physics parameters (throw forces)

---

## 4. Components Reference

### USuspenseCoreGrenadeThrowAbility

**Header:** `Source/GAS/Public/SuspenseCore/Abilities/Throwable/SuspenseCoreGrenadeThrowAbility.h`

#### Configuration Properties

```cpp
// Montages
UPROPERTY(EditDefaultsOnly, Category = "Grenade|Montages")
TObjectPtr<UAnimMontage> OverhandThrowMontage;

UPROPERTY(EditDefaultsOnly, Category = "Grenade|Montages")
TObjectPtr<UAnimMontage> UnderhandThrowMontage;

UPROPERTY(EditDefaultsOnly, Category = "Grenade|Montages")
TObjectPtr<UAnimMontage> RollThrowMontage;

// Timing
UPROPERTY(EditDefaultsOnly, Category = "Grenade|Timing")
float PrepareTime = 0.5f;

UPROPERTY(EditDefaultsOnly, Category = "Grenade|Timing")
float MaxCookTime = 5.0f;

// Physics
UPROPERTY(EditDefaultsOnly, Category = "Grenade|Physics")
float OverhandThrowForce = 1500.0f;

UPROPERTY(EditDefaultsOnly, Category = "Grenade|Physics")
float UnderhandThrowForce = 800.0f;

UPROPERTY(EditDefaultsOnly, Category = "Grenade|Physics")
float RollThrowForce = 500.0f;

// Sounds
UPROPERTY(EditDefaultsOnly, Category = "Grenade|Sounds")
TObjectPtr<USoundBase> PinPullSound;

UPROPERTY(EditDefaultsOnly, Category = "Grenade|Sounds")
TObjectPtr<USoundBase> ThrowSound;

UPROPERTY(EditDefaultsOnly, Category = "Grenade|Sounds")
TObjectPtr<USoundBase> CancelSound;
```

#### Blueprint Events

```cpp
// Override these in Blueprint for custom behavior
UFUNCTION(BlueprintImplementableEvent)
void OnPrepareStarted();

UFUNCTION(BlueprintImplementableEvent)
void OnGrenadePinPulled();

UFUNCTION(BlueprintImplementableEvent)
void OnGrenadeThrown();

UFUNCTION(BlueprintImplementableEvent)
void OnThrowCancelled();
```

### ASuspenseCoreGrenadeProjectile

**Header:** `Source/EquipmentSystem/Public/SuspenseCore/Actors/SuspenseCoreGrenadeProjectile.h`

#### Configuration Properties

```cpp
// Timing
UPROPERTY(EditDefaultsOnly, Category = "Grenade|Timing")
float FuseTime = 3.5f;

UPROPERTY(EditDefaultsOnly, Category = "Grenade|Timing")
float MinFuseTime = 0.5f;

// Damage
UPROPERTY(EditDefaultsOnly, Category = "Grenade|Damage")
float BaseDamage = 250.0f;

UPROPERTY(EditDefaultsOnly, Category = "Grenade|Damage")
float InnerRadius = 200.0f;

UPROPERTY(EditDefaultsOnly, Category = "Grenade|Damage")
float OuterRadius = 500.0f;

UPROPERTY(EditDefaultsOnly, Category = "Grenade|Damage")
float DamageFalloff = 1.0f;

UPROPERTY(EditDefaultsOnly, Category = "Grenade|Damage")
bool bDamageInstigator = true;

// Effects
UPROPERTY(EditDefaultsOnly, Category = "Grenade|Effects")
TObjectPtr<USoundBase> ExplosionSound;

UPROPERTY(EditDefaultsOnly, Category = "Grenade|Effects")
TObjectPtr<UNiagaraSystem> ExplosionEffect;

// GameplayEffects
UPROPERTY(EditDefaultsOnly, Category = "Grenade|Damage")
TSubclassOf<UGameplayEffect> DamageEffectClass;

UPROPERTY(EditDefaultsOnly, Category = "Grenade|Damage")
TSubclassOf<UGameplayEffect> FlashbangEffectClass;

UPROPERTY(EditDefaultsOnly, Category = "Grenade|Damage")
TSubclassOf<UGameplayEffect> IncendiaryEffectClass;
```

---

## 5. Configuration Guide

### Creating a New Grenade Type

#### Step 1: Add to ItemDatabase

In `Content/Data/ItemDatabase/SuspenseCoreItemDatabase.json`:

```json
{
  "ItemID": "Throwable_M67",
  "DisplayName": "M67 Fragmentation Grenade",
  "Description": "Standard US military fragmentation grenade",
  "ItemType": "Item.Throwable",
  "bIsThrowable": true,
  "ThrowableType": "Item.Grenade.Fragmentation",
  "EquipmentActorClass": "/Game/Blueprints/Grenades/BP_Grenade_M67.BP_Grenade_M67_C",
  "ItemTags": ["Item.Grenade", "Item.Throwable", "Item.Grenade.Fragmentation"],
  "Weight": 0.4,
  "Size": { "Width": 1, "Height": 1 }
}
```

#### Step 2: Add Throwable Attributes

In `Content/Data/ItemDatabase/SuspenseCoreThrowableAttributes.json`:

```json
{
  "ThrowableID": "Throwable_M67",
  "DisplayName": "M67 Grenade",
  "FuseTime": 4.0,
  "ExplosionRadius": 450.0,
  "Damage": 200.0,
  "ThrowForce": 1400.0,
  "GrenadeClass": "/Game/Blueprints/Grenades/BP_Grenade_M67.BP_Grenade_M67_C"
}
```

#### Step 3: Create Blueprint

1. Create `BP_Grenade_M67` inheriting from `ASuspenseCoreGrenadeProjectile`
2. Configure:
   - Set mesh (StaticMesh for grenade model)
   - Set FuseTime = 4.0
   - Set BaseDamage = 200.0
   - Set InnerRadius = 150.0, OuterRadius = 450.0
   - Set explosion sound and particle
   - Set DamageEffectClass

### Grenade Presets

#### F-1 (Russian Fragmentation)
```
FuseTime: 3.5s
BaseDamage: 250
InnerRadius: 200
OuterRadius: 500
Bounciness: 0.3
```

#### M67 (US Fragmentation)
```
FuseTime: 4.0s
BaseDamage: 200
InnerRadius: 150
OuterRadius: 450
Bounciness: 0.35
```

#### Smoke (M18)
```
FuseTime: 2.0s
BaseDamage: 0
SmokeRadius: 500
SmokeDuration: 30s
```

#### Flashbang (M84)
```
FuseTime: 1.5s
BaseDamage: 10
FlashRadius: 300
FlashDuration: 5s
DeafenDuration: 3s
```

---

## 6. Integration Guide

### With Inventory System

```cpp
// When player picks up grenade
void UInventoryComponent::OnItemAdded(const FSuspenseCoreItemInstance& Item)
{
    // Check if this is a throwable
    if (Item.HasTag(SuspenseCoreTags::Item::Throwable))
    {
        // Auto-assign to first empty QuickSlot if available
        if (QuickSlotComponent)
        {
            int32 EmptySlot = QuickSlotComponent->FindFirstEmptySlot();
            if (EmptySlot != INDEX_NONE)
            {
                QuickSlotComponent->AssignItemToSlot(EmptySlot, Item.ItemID);
            }
        }
    }
}
```

### With UI System

```cpp
// Subscribe to grenade events for HUD
void UGrenadeHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (USuspenseCoreEventBus* EventBus = GetEventBus())
    {
        EventBus->SubscribeNative(
            SuspenseCoreTags::Event::Throwable::PrepareStarted,
            this,
            FSuspenseCoreNativeEventCallback::CreateUObject(
                this, &UGrenadeHUDWidget::OnPrepareStarted),
            ESuspenseCoreEventPriority::Normal);

        EventBus->SubscribeNative(
            SuspenseCoreTags::Event::Throwable::CookingStarted,
            this,
            FSuspenseCoreNativeEventCallback::CreateUObject(
                this, &UGrenadeHUDWidget::OnCookingStarted),
            ESuspenseCoreEventPriority::Normal);
    }
}

void UGrenadeHUDWidget::OnCookingStarted(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
    // Show cooking timer in HUD
    FString GrenadeID = EventData.GetString(TEXT("GrenadeID"));
    float CookTime = EventData.GetFloat(TEXT("CookTime"));

    ShowCookingIndicator(GrenadeID, CookTime);
}
```

### With AI System

```cpp
// AI perception of grenades
void UAIPerceptionComponent::OnGrenadeThrown(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
    FVector ThrowLocation = EventData.GetVector(TEXT("ThrowLocation"));
    FVector ThrowDirection = EventData.GetVector(TEXT("ThrowDirection"));
    float ThrowForce = EventData.GetFloat(TEXT("ThrowForce"));

    // Predict landing position
    FVector PredictedLanding = PredictGrenadeLanding(ThrowLocation, ThrowDirection, ThrowForce);

    // Notify AI to take cover
    if (AAIController* AIController = GetAIController())
    {
        AIController->HandleGrenadeWarning(PredictedLanding);
    }
}
```

---

## 7. Animation Setup

### Required AnimNotify Points

Your throw montage **must** include these AnimNotify events:

| Notify Name | Timing | Purpose |
|-------------|--------|---------|
| `PinPull` | ~0.2s | Pin removed, grenade is now armed |
| `Ready` | ~0.5s | Grenade in throw position, cooking starts |
| `Release` | ~0.8s | Throw the grenade |

### Alternative Notify Names

The system also recognizes these alternative names:

| Primary | Alternatives |
|---------|--------------|
| `PinPull` | `Continue`, `Arm` |
| `Ready` | `ClipIn`, `Armed` |
| `Release` | `Finalize`, `Throw` |

### Montage Setup Steps

1. **Import throw animation** (FBX)
2. **Create AnimMontage** from animation
3. **Add AnimNotify_PlayMontageNotify** at key frames:
   - Add at pin pull frame → Name: `PinPull`
   - Add at ready position → Name: `Ready`
   - Add at release frame → Name: `Release`
4. **Set DefaultSlot** to appropriate slot (FullBody or UpperBody)

### Example Timeline

```
[Start] ----[PinPull]----[Ready]----[Release]----[End]
  0.0s        0.2s        0.5s        0.8s       1.2s
   |           |           |           |           |
   V           V           V           V           V
 Begin     Pin Out    In Position   Throw      Blend Out
```

---

## 8. Blueprint Examples

### Custom Grenade Ability Blueprint

```
BP_CustomGrenadeAbility (Parent: USuspenseCoreGrenadeThrowAbility)
├── Event: OnPrepareStarted
│   └── Play camera effect (slight zoom)
├── Event: OnGrenadePinPulled
│   └── Play UI sound
│   └── Start cooking timer widget
├── Event: OnGrenadeThrown
│   └── Stop cooking timer widget
│   └── Camera shake
└── Event: OnThrowCancelled
    └── Play cancel sound
    └── Hide cooking timer
```

### Custom Grenade Projectile Blueprint

```
BP_CustomGrenadeProjectile (Parent: ASuspenseCoreGrenadeProjectile)
├── Event: OnGrenadeInitialized
│   └── Start trail particle
│   └── Play throw sound
├── Event: OnPreExplosion
│   └── Prepare explosion decal
│   └── Play warning sound
└── Event: OnPostExplosion
    └── Spawn debris actors
    └── Apply screen shake
    └── Create crater decal
```

---

## 9. Network Replication

### Authority Model

| Component | Authority | Prediction |
|-----------|-----------|------------|
| **Ability activation** | Server | Client predicted |
| **Animation** | Local | Replicated via montage |
| **Projectile spawn** | Server only | - |
| **Physics** | Server | Interpolated on clients |
| **Damage** | Server only | - |
| **Effects** | Multicast | All clients |

### Replication Flow

```
CLIENT                          SERVER                          OTHER CLIENTS
   │                               │                                 │
   │──TryActivateAbility──────────▶│                                 │
   │  (predicted)                  │                                 │
   │                               │──Validate──────────────────────▶│
   │                               │                                 │
   │◀─────────────Confirm──────────│                                 │
   │                               │                                 │
   │  Play Montage (local)         │  Play Montage (replicated)      │
   │                               │                                 │
   │──SpawnRequested (EventBus)───▶│                                 │
   │                               │                                 │
   │                               │──SpawnGrenade─────────────────▶│
   │                               │  (Replicated Actor)             │
   │                               │                                 │
   │◀────────Replicate Actor───────│─────────Replicate Actor────────▶│
   │                               │                                 │
   │  (Physics interpolated)       │  (Physics authority)            │
   │                               │                                 │
   │                               │──Explode()                      │
   │                               │  ApplyDamage (server only)      │
   │                               │                                 │
   │◀──Multicast_SpawnEffects──────│──Multicast_SpawnEffects────────▶│
```

### Lag Compensation

The system handles network lag by:

1. **Client prediction** - Ability activates immediately on client
2. **Server validation** - Server confirms/denies activation
3. **Projectile replication** - Server spawns, clients receive replicated actor
4. **Effect multicast** - Visual effects sent to all clients simultaneously

---

## 10. Troubleshooting

### Common Issues

#### "Grenade not throwing"

**Symptoms:** Ability activates but no grenade spawns

**Solutions:**
1. Check GrenadeHandler is registered with ItemUseService
2. Verify EventBus subscription for `SpawnRequested`
3. Check DataManager has valid `EquipmentActorClass` for grenade ID
4. Ensure grenade Blueprint class is valid

#### "AnimNotify not firing"

**Symptoms:** Montage plays but phases don't trigger

**Solutions:**
1. Verify AnimNotify names match expected: `PinPull`, `Ready`, `Release`
2. Use `AnimNotify_PlayMontageNotify` type (not `AnimNotify` or `AnimNotifyState`)
3. Check montage is playing on correct slot
4. Enable verbose logging: `LogSuspenseCoreGrenade Verbose`

#### "Damage not applying"

**Symptoms:** Explosion effects play but no damage

**Solutions:**
1. Ensure `DamageEffectClass` is set on projectile Blueprint
2. Verify targets have `AbilitySystemComponent`
3. Check `SuspenseCoreTags::Data::Damage` SetByCaller tag exists
4. Ensure explosion runs on server (HasAuthority() check)

#### "Cooking timer not working"

**Symptoms:** Grenade doesn't reduce fuse time when held

**Solutions:**
1. Check `Ready` AnimNotify is firing (starts cooking)
2. Verify `CookStartTime` is being set
3. Ensure `GetCookTime()` returns correct value
4. Check `EffectiveFuseTime` calculation: `FuseTime - CookTime`

### Debug Commands

```cpp
// Enable verbose logging
LogSuspenseCoreGrenade Verbose
LogGrenadeHandler Verbose
LogGrenadeProjectile Verbose

// Show grenade debug spheres
ShowFlag.Collision 1

// Test explosion damage
GrenadeProjectile.ForceExplode
```

### Log Messages

| Log | Meaning |
|-----|---------|
| `Grenade throw started` | Ability activated successfully |
| `AnimNotify received: 'PinPull'` | Pin pull phase triggered |
| `Published SpawnRequested event` | Grenade ready to spawn |
| `Spawned grenade at...` | Projectile actor created |
| `Applied X damage to Y` | Damage calculation complete |

---

## Appendix A: Module Dependencies

### EquipmentSystem.Build.cs Dependencies

The GrenadeProjectile uses the CameraShake system from `PlayerCore` module. Ensure these dependencies are in your `EquipmentSystem.Build.cs`:

```csharp
PublicDependencyModuleNames.AddRange(
    new string[]
    {
        "Core",
        "CoreUObject",
        "Engine",
        "GameplayAbilities",  // GAS integration
        "GameplayTags",       // Tag system
        "GameplayTasks",      // Task system

        // Suspense modules
        "BridgeSystem",
        "GAS",
        "PlayerCore"  // For CameraShake system (required for grenade explosions)
    }
);

PrivateDependencyModuleNames.AddRange(
    new string[]
    {
        "Slate",
        "SlateCore",
        "InputCore",
        "NetCore",
        "Niagara",  // For particle effects
        "Json",
        "JsonUtilities",
        "OnlineSubsystem",
        "OnlineSubsystemUtils"
    }
);
```

### Required Includes for GrenadeProjectile.cpp

```cpp
// Grenade header
#include "SuspenseCore/Actors/SuspenseCoreGrenadeProjectile.h"

// EventBus - use EventManager pattern
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"

// CameraShake from PlayerCore module (requires PlayerCore dependency)
#include "SuspenseCore/CameraShake/SuspenseCoreExplosionCameraShake.h"

// Engine includes for damage/overlap
#include "Engine/OverlapResult.h"
#include "Engine/DamageEvents.h"
```

### EventBus Access Pattern

Always use the project-standard `USuspenseCoreEventManager` pattern:

```cpp
// CORRECT - Project standard pattern
USuspenseCoreEventBus* GetEventBus()
{
    if (!EventBus.IsValid())
    {
        if (USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this))
        {
            EventBus = EventManager->GetEventBus();
        }
    }
    return EventBus.Get();
}

// WRONG - Do NOT use TActorIterator
// TActorIterator<ASuspenseCoreEventBus> It(GetWorld()); // DON'T DO THIS
```

---

## Appendix B: Complete Configuration Checklist

- [ ] GrenadeThrowAbility granted to player
- [ ] GrenadeHandler registered with ItemUseService
- [ ] GrenadeHandler subscribed to EventBus SpawnRequested
- [ ] Animation montages created with AnimNotify points
- [ ] Grenade projectile Blueprint configured
- [ ] GameplayEffect for damage created
- [ ] Grenade data added to ItemDatabase
- [ ] QuickSlot component on player character
- [ ] Sounds and particle effects assigned
- [ ] Network replication tested

---

## Appendix C: Performance Considerations

| Optimization | Implementation |
|--------------|----------------|
| **Native Tags** | Uses UE_DECLARE_GAMEPLAY_TAG_EXTERN for zero runtime lookup |
| **Cached Providers** | QuickSlotProvider cached per ability activation |
| **Server Authority** | Damage calculation only on server |
| **Effect Pooling** | Niagara effects use AutoRelease pooling |
| **Overlap Batching** | Single OverlapMulti for all targets |

---

**Document End**

*For questions or issues, refer to the SuspenseCore GitHub repository or contact the development team.*
