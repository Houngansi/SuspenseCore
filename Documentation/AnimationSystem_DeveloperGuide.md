# Animation System Developer Guide

> **Version:** 1.0
> **Last Updated:** December 2024
> **Target:** Unreal Engine 5.x | AAA Project Standards

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [AnimInstance SSOT](#animinstance-ssot)
3. [Basic Locomotion Setup](#basic-locomotion-setup)
4. [GAS Movement Abilities](#gas-movement-abilities)
5. [Animation Blueprint Guide](#animation-blueprint-guide)
6. [DataTable Configuration](#datatable-configuration)
7. [Weapon Animation Setup](#weapon-animation-setup)
8. [EventBus Integration](#eventbus-integration)
9. [Checklist for New Animations](#checklist-for-new-animations)

---

## Architecture Overview

The Animation System follows **SSOT (Single Source of Truth)** architecture where `USuspenseCoreCharacterAnimInstance` aggregates all animation data from multiple sources.

### Core Principles

| Principle | Implementation |
|-----------|----------------|
| **Single Source of Truth** | AnimInstance aggregates ALL animation variables |
| **Data-Driven Animations** | Weapon animations from DataTable |
| **GAS Integration** | Movement speeds from AttributeSet |
| **Pull Model** | AnimInstance pulls data each frame (not push) |

### Data Flow Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      DATA SOURCES                                            │
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────────────────────┐ │
│  │ Character      │  │ StanceComponent│  │ GAS MovementAttributeSet      │ │
│  │ - MovementState│  │ - WeaponType   │  │ - WalkSpeed, SprintSpeed      │ │
│  │ - IsSprinting  │  │ - IsDrawn      │  │ - CrouchSpeed, AimSpeed       │ │
│  │ - IsCrouching  │  │ - AnimData     │  └────────────────────────────────┘ │
│  └───────┬────────┘  └───────┬────────┘                   │                 │
│          │                   │                            │                 │
└──────────┼───────────────────┼────────────────────────────┼─────────────────┘
           │                   │                            │
           ▼                   ▼                            ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│              USuspenseCoreCharacterAnimInstance (SSOT)                       │
│                                                                              │
│  NativeUpdateAnimation() каждый кадр:                                       │
│  ├── UpdateMovementData()     → bIsSprinting, bIsCrouching, MovementState   │
│  ├── UpdateVelocityData()     → Speed, MoveForward, MoveRight               │
│  ├── UpdateWeaponData()       → CurrentWeaponType, bIsWeaponDrawn           │
│  ├── UpdateAnimationAssets()  → CurrentStanceBlendSpace, Montages           │
│  ├── UpdateIKData()           → LeftHandIKTransform, WeaponTransform        │
│  ├── UpdateAimOffsetData()    → AimYaw, AimPitch                            │
│  └── UpdateGASAttributes()    → MaxWalkSpeed, MaxSprintSpeed                │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
           │
           ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                    ANIMATION BLUEPRINT                                       │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │ AnimGraph:                                                              │ │
│  │ - State Machine использует MovementState                               │ │
│  │ - BlendSpaces используют MoveForward, MoveRight, Speed                 │ │
│  │ - Montages берутся из CurrentAnimationData                             │ │
│  └────────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Module Location

```
Source/PlayerCore/
├── Public/SuspenseCore/Animation/
│   └── SuspenseCoreCharacterAnimInstance.h    ← SSOT AnimInstance
└── Private/SuspenseCore/Animation/
    └── SuspenseCoreCharacterAnimInstance.cpp
```

---

## AnimInstance SSOT

### Class Overview

`USuspenseCoreCharacterAnimInstance` - единственный источник истины для Animation Blueprint.

**Файл:** `Source/PlayerCore/Public/SuspenseCore/Animation/SuspenseCoreCharacterAnimInstance.h`

### Key Variables

#### Movement State (из Character)

| Variable | Type | Description |
|----------|------|-------------|
| `MovementState` | `ESuspenseCoreMovementState` | Idle, Walking, Sprinting, Crouching, Jumping, Falling |
| `bIsSprinting` | `bool` | Sprint ability активна |
| `bIsCrouching` | `bool` | Crouch ability активна |
| `bIsInAir` | `bool` | В воздухе (Jump или Fall) |
| `bIsJumping` | `bool` | Активный прыжок |
| `bIsFalling` | `bool` | Падение (без прыжка) |
| `bIsOnGround` | `bool` | На земле |
| `bHasMovementInput` | `bool` | Игрок нажимает WASD |

#### Velocity & Direction (для BlendSpaces)

| Variable | Type | Description |
|----------|------|-------------|
| `Speed` | `float` | Полная скорость (длина Velocity) |
| `GroundSpeed` | `float` | Горизонтальная скорость |
| `NormalizedSpeed` | `float` | 0-1 относительно MaxWalkSpeed |
| `MoveForward` | `float` | -1..1 для BlendSpace |
| `MoveRight` | `float` | -1..1 для BlendSpace |
| `MovementDirection` | `float` | -180..180 градусов |
| `VerticalVelocity` | `float` | Для Jump/Fall бленда |

#### GAS Attributes (скорости)

| Variable | Type | Source |
|----------|------|--------|
| `MaxWalkSpeed` | `float` | `USuspenseCoreMovementAttributeSet` |
| `MaxSprintSpeed` | `float` | `USuspenseCoreMovementAttributeSet` |
| `MaxCrouchSpeed` | `float` | `USuspenseCoreMovementAttributeSet` |
| `MaxAimSpeed` | `float` | `USuspenseCoreMovementAttributeSet` |

### Usage in Animation Blueprint

```cpp
// AnimInstance автоматически доступен в ABP как "this"
// Все переменные BlueprintReadOnly

// State Machine Transition Rules:
// Walking → Sprinting: bIsSprinting == true
// Walking → Crouching: bIsCrouching == true
// Any → InAir:         bIsInAir == true

// BlendSpace Inputs:
// Locomotion BS: Speed (или NormalizedSpeed)
// Directional BS: MoveForward, MoveRight
```

---

## Basic Locomotion Setup

### Movement States

`ESuspenseCoreMovementState` enum определен в `SuspenseCoreCharacter.h`:

```cpp
UENUM(BlueprintType)
enum class ESuspenseCoreMovementState : uint8
{
    Idle        UMETA(DisplayName = "Idle"),
    Walking     UMETA(DisplayName = "Walking"),
    Sprinting   UMETA(DisplayName = "Sprinting"),
    Crouching   UMETA(DisplayName = "Crouching"),
    Jumping     UMETA(DisplayName = "Jumping"),
    Falling     UMETA(DisplayName = "Falling")
};
```

### State Machine Structure (Unarmed)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         LOCOMOTION STATE MACHINE                             │
│                                                                              │
│    ┌──────────┐     bHasMovementInput     ┌──────────┐                      │
│    │   IDLE   │ ──────────────────────────│  WALKING │                      │
│    │ (Idle BS)│ ◄────────────────────────│(Loco BS) │                      │
│    └────┬─────┘    !bHasMovementInput     └────┬─────┘                      │
│         │                                       │                            │
│         │ bIsSprinting                          │ bIsSprinting               │
│         ▼                                       ▼                            │
│    ┌──────────────────────────────────────────────┐                         │
│    │              SPRINTING                        │                         │
│    │         (Sprint BlendSpace)                   │                         │
│    │   Speed = MaxSprintSpeed (~600)               │                         │
│    └──────────────────────────────────────────────┘                         │
│                                                                              │
│    ┌──────────┐                                                              │
│    │ CROUCHING│ ◄── bIsCrouching                                            │
│    │(Crouch BS)│    Speed = MaxCrouchSpeed (~200)                           │
│    └──────────┘                                                              │
│                                                                              │
│    ┌──────────────────────────────────────────────┐                         │
│    │              IN AIR                           │                         │
│    │  ┌─────────┐            ┌──────────┐         │                         │
│    │  │ JUMPING │ ──────────│  FALLING │         │ ◄── bIsInAir            │
│    │  │(Jump BS)│ VelZ < 0  │ (Fall BS)│         │                         │
│    │  └─────────┘            └──────────┘         │                         │
│    └──────────────────────────────────────────────┘                         │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Transition Rules

| From State | To State | Condition |
|------------|----------|-----------|
| Idle | Walking | `bHasMovementInput == true` |
| Walking | Idle | `bHasMovementInput == false && GroundSpeed < 10` |
| Walking | Sprinting | `bIsSprinting == true` |
| Sprinting | Walking | `bIsSprinting == false` |
| Any Ground | Crouching | `bIsCrouching == true` |
| Crouching | Previous | `bIsCrouching == false` |
| Any Ground | InAir | `bIsInAir == true` |
| InAir | Idle/Walking | `bIsOnGround == true` |
| Jumping | Falling | `VerticalVelocity < 0` |

### BlendSpace Configuration

#### Idle/Walking BlendSpace1D

- **Horizontal Axis:** `Speed` (0 → MaxWalkSpeed)
- **Animations:**
  - 0: Idle
  - 150: Walk
  - 400: Jog

#### Directional Locomotion BlendSpace

- **Horizontal Axis:** `MoveRight` (-1 → 1)
- **Vertical Axis:** `MoveForward` (-1 → 1)
- **Animations:** 8-directional movement

#### Sprint BlendSpace1D

- **Horizontal Axis:** `Speed` (MaxWalkSpeed → MaxSprintSpeed)
- **Animations:**
  - 400: Fast Jog
  - 600: Sprint

---

## GAS Movement Abilities

Система использует GAS для управления движением с интеграцией в анимации.

### Sprint Ability

**Файл:** `Source/GAS/Public/SuspenseCore/Abilities/Movement/SuspenseCoreCharacterSprintAbility.h`

```cpp
// Поведение: Hold-to-Sprint
// - Удерживание кнопки активирует спринт
// - Отпускание завершает

// GameplayEffects:
UPROPERTY()
TSubclassOf<UGameplayEffect> SprintBuffEffectClass;    // +50% Speed
TSubclassOf<UGameplayEffect> SprintCostEffectClass;    // Stamina drain

// EventBus Events:
// SuspenseCore.Event.Ability.CharacterSprint.Activated
// SuspenseCore.Event.Ability.CharacterSprint.Ended
```

**Integration with AnimInstance:**

```cpp
// В AnimInstance переменная bIsSprinting обновляется из Character:
bIsSprinting = Character->IsSprinting();

// Character получает состояние из GAS ability через GameplayTag:
// State.Sprinting - добавляется SprintBuffEffect
```

### Crouch Ability

**Файл:** `Source/GAS/Public/SuspenseCore/Abilities/Movement/SuspenseCoreCharacterCrouchAbility.h`

```cpp
// Поведение: Hold-to-Crouch или Toggle
UPROPERTY()
bool bToggleMode;    // true = toggle, false = hold

// GameplayEffects:
TSubclassOf<UGameplayEffect> CrouchDebuffEffectClass;    // -50% Speed

// EventBus Events:
// SuspenseCore.Event.Ability.CharacterCrouch.Activated
// SuspenseCore.Event.Ability.CharacterCrouch.Ended
```

**Integration with AnimInstance:**

```cpp
// В AnimInstance:
bIsCrouching = (MovementState == ESuspenseCoreMovementState::Crouching);

// Также обновляется MaxCrouchSpeed из GAS AttributeSet
MaxCrouchSpeed = MovementAttrs->GetCrouchSpeed();
```

### Jump Ability

**Файл:** `Source/GAS/Public/SuspenseCore/Abilities/Movement/SuspenseCoreCharacterJumpAbility.h`

```cpp
// Поведение: Variable Height Jump
UPROPERTY()
float JumpPowerMultiplier;         // Множитель силы прыжка
float StaminaCostPerJump;          // Стоимость в стамине
TSubclassOf<UGameplayEffect> JumpStaminaCostEffectClass;

// Safety:
float MaxJumpDuration;             // Таймаут
float GroundCheckInterval;         // Интервал проверки приземления

// EventBus Events:
// SuspenseCore.Event.Ability.CharacterJump.Activated
// SuspenseCore.Event.Ability.CharacterJump.Ended
// SuspenseCore.Event.Ability.CharacterJump.Landed
```

**Integration with AnimInstance:**

```cpp
// В AnimInstance обновляется из Character:
bIsJumping = (MovementState == ESuspenseCoreMovementState::Jumping);
bIsFalling = (MovementState == ESuspenseCoreMovementState::Falling);
bIsInAir = bIsJumping || bIsFalling;

// Для бленда прыжка/падения:
VerticalVelocity = Velocity.Z;
```

### Movement AttributeSet

**Файл:** `Source/GAS/Public/SuspenseCore/Attributes/SuspenseCoreMovementAttributeSet.h`

```cpp
UCLASS()
class USuspenseCoreMovementAttributeSet : public UAttributeSet
{
    // Базовые скорости
    UPROPERTY(BlueprintReadOnly)
    FGameplayAttributeData WalkSpeed;      // Default: 400

    UPROPERTY(BlueprintReadOnly)
    FGameplayAttributeData SprintSpeed;    // Default: 600

    UPROPERTY(BlueprintReadOnly)
    FGameplayAttributeData CrouchSpeed;    // Default: 200

    UPROPERTY(BlueprintReadOnly)
    FGameplayAttributeData AimSpeed;       // Default: 250

    // Getters
    float GetWalkSpeed() const;
    float GetSprintSpeed() const;
    float GetCrouchSpeed() const;
    float GetAimSpeed() const;
};
```

**GameplayEffects модифицируют эти атрибуты:**

- `GE_SprintBuff`: SprintSpeed modifier +50%
- `GE_CrouchDebuff`: CrouchSpeed modifier -50%

---

## Animation Blueprint Guide

### Step 1: Create Animation Blueprint

1. Content Browser → Animation → Animation Blueprint
2. Parent Class: `USuspenseCoreCharacterAnimInstance`
3. Skeleton: Your character skeleton
4. Name: `ABP_SuspenseCoreCharacter`

### Step 2: Configure Parent Class

**Blueprints/Animation/ABP_SuspenseCoreCharacter.uasset**

После создания ABP, все переменные из `USuspenseCoreCharacterAnimInstance` доступны автоматически.

### Step 3: Create State Machine

```
AnimGraph:
├── Output Pose
│   └── Layered Blend Per Bone (для Upper Body оружия)
│       ├── Base: Locomotion State Machine
│       │   ├── Idle
│       │   ├── Walking (BlendSpace)
│       │   ├── Sprinting (BlendSpace)
│       │   ├── Crouching (BlendSpace)
│       │   └── InAir
│       │       ├── Jumping
│       │       └── Falling
│       └── Upper Body: Weapon Layer (optional)
│
└── Event Graph: (пустой - все в NativeUpdate)
```

### Step 4: Setup Transitions

**Idle → Walking:**
```
// Transition Rule
Return bHasMovementInput && !bIsCrouching && !bIsSprinting
```

**Walking → Sprinting:**
```
// Transition Rule
Return bIsSprinting
```

**Any → Crouching:**
```
// Transition Rule
Return bIsCrouching
```

**Ground → InAir:**
```
// Transition Rule
Return bIsInAir
```

### Step 5: Configure BlendSpaces

**Walking State → BlendSpace1D:**
- Asset: `BS_Unarmed_Locomotion`
- Input: `Speed`

**Sprinting State → BlendSpace1D:**
- Asset: `BS_Unarmed_Sprint`
- Input: `Speed`

**InAir → Blend:**
```
// Используем VerticalVelocity для бленда
Blend(JumpAnim, FallAnim, MapRangeClamped(VerticalVelocity, 100, -100, 0, 1))
```

---

## DataTable Configuration

### Weapon Animations DataTable

**Location:** Configured in `USuspenseCoreSettings::WeaponAnimationsTable`

**Row Structure:** `FAnimationStateData`

```cpp
USTRUCT(BlueprintType)
struct FAnimationStateData : public FTableRowBase
{
    // Core BlendSpaces
    UBlendSpace* Stance;           // Стойка с оружием
    UBlendSpace1D* Locomotion;     // Локомоция с оружием

    // Idle/Aim
    UAnimSequence* Idle;
    UAnimSequence* AimIdle;

    // Action Montages
    UAnimMontage* Draw;            // Достать оружие
    UAnimMontage* FirstDraw;       // Первое доставание (optional)
    UAnimMontage* Holster;         // Убрать оружие
    UAnimMontage* Shoot;           // Выстрел Hip
    UAnimMontage* AimShoot;        // Выстрел ADS
    UAnimMontage* ReloadLong;      // Полная перезарядка
    UAnimMontage* ReloadShort;     // Тактическая перезарядка

    // IK Transforms
    TArray<FTransform> LHGripTransform;    // [0]=Hip, [1]=Aim
    FTransform RHTransform;
    FTransform WTransform;
};
```

### DataTable Row Names

Row names должны соответствовать GameplayTag оружия:

| Row Name | GameplayTag |
|----------|-------------|
| `Weapon.Rifle.AR` | `Weapon.Rifle.AR` |
| `Weapon.Pistol.Glock` | `Weapon.Pistol.Glock` |
| `Weapon.SMG.MP5` | `Weapon.SMG.MP5` |

### Accessing in AnimInstance

```cpp
// AnimInstance автоматически загружает DataTable из SuspenseCoreSettings
void USuspenseCoreCharacterAnimInstance::LoadWeaponAnimationsTable()
{
    const USuspenseCoreSettings* Settings = GetDefault<USuspenseCoreSettings>();
    if (Settings && !Settings->WeaponAnimationsTable.IsNull())
    {
        WeaponAnimationsTable = Settings->WeaponAnimationsTable.LoadSynchronous();
    }
}

// Получение данных по WeaponType:
const FAnimationStateData* AnimData = GetAnimationDataForWeaponType(CurrentWeaponType);
if (AnimData)
{
    CurrentStanceBlendSpace = AnimData->Stance;
    CurrentLocomotionBlendSpace = AnimData->Locomotion;
}
```

---

## Weapon Animation Setup

### Overview

Weapon animation system использует `USuspenseCoreWeaponStanceComponent` как SSOT для всех боевых состояний оружия. Компонент автоматически добавляется к Character и предоставляет данные в AnimInstance.

### Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      WEAPON ANIMATION ARCHITECTURE                          │
│                                                                             │
│  ┌─────────────────┐    ┌─────────────────────────────────────────────────┐│
│  │ GAS Abilities   │───▶│         USuspenseCoreWeaponStanceComponent     ││
│  │ - AimAbility    │    │                     (SSOT)                      ││
│  │ - ReloadAbility │    │                                                 ││
│  │ - HoldBreath    │    │  Combat States (Replicated):                    ││
│  └─────────────────┘    │  ├── bIsAiming          ← SetAiming()           ││
│                         │  ├── bIsFiring          ← SetFiring()           ││
│  ┌─────────────────┐    │  ├── bIsReloading       ← SetReloading()        ││
│  │ Weapon Actor    │───▶│  ├── bIsHoldingBreath   ← SetHoldingBreath()    ││
│  │ - Fire()        │    │  │                                              ││
│  │ - AddRecoil()   │    │  Pose Modifiers (Local):                        ││
│  │ - WallDetect()  │    │  ├── AimPoseAlpha       ← interpolated          ││
│  └─────────────────┘    │  ├── GripModifier       ← SetGripModifier()     ││
│                         │  ├── WeaponLoweredAlpha ← SetWeaponLowered()    ││
│  ┌─────────────────┐    │  │                                              ││
│  │ Equipment Sys   │───▶│  Procedural:                                    ││
│  │ - Equip/Unequip │    │  ├── RecoilAlpha        ← AddRecoil() + decay   ││
│  │ - WeaponType    │    │  └── SwayMultiplier     ← SetSwayMultiplier()   ││
│  └─────────────────┘    │                                                 ││
│                         │  GetStanceSnapshot() ──────────────────────────▶││
│                         └─────────────────────────────────────────────────┘│
│                                          │                                  │
│                                          ▼                                  │
│                         ┌─────────────────────────────────────────────────┐│
│                         │    USuspenseCoreCharacterAnimInstance           ││
│                         │                                                 ││
│                         │  UpdateWeaponData():                            ││
│                         │  ├── bIsAiming          (from snapshot)         ││
│                         │  ├── bIsFiring          (from snapshot)         ││
│                         │  ├── bIsReloading       (from snapshot)         ││
│                         │  ├── bIsHoldingBreath   (from snapshot)         ││
│                         │  ├── AimingAlpha        (from snapshot)         ││
│                         │  ├── GripModifier       (from snapshot)         ││
│                         │  ├── WeaponLoweredAlpha (from snapshot)         ││
│                         │  ├── RecoilAlpha        (from snapshot)         ││
│                         │  └── WeaponSwayMultiplier (from snapshot)       ││
│                         └─────────────────────────────────────────────────┘│
│                                          │                                  │
│                                          ▼                                  │
│                         ┌─────────────────────────────────────────────────┐│
│                         │            Animation Blueprint                  ││
│                         │  - Use variables directly in State Machine      ││
│                         │  - Blend poses based on AimingAlpha             ││
│                         │  - Apply recoil via additive layer              ││
│                         └─────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────────────────┘
```

### WeaponStanceComponent API

**Location:** `Source/EquipmentSystem/Public/SuspenseCore/Components/SuspenseCoreWeaponStanceComponent.h`

#### Combat State Setters (called by GAS/Weapon)

| Method | Description | Called By |
|--------|-------------|-----------|
| `SetAiming(bool)` | Начать/закончить прицеливание | ADS Ability |
| `SetFiring(bool)` | Начать/закончить стрельбу | Weapon Actor |
| `SetReloading(bool)` | Начать/закончить перезарядку | Reload Ability / WeaponStateManager |
| `SetHoldingBreath(bool)` | Задержка дыхания (снайпер) | Sniper Ability |
| `SetMontageActive(bool)` | Монтаж оружия играет | AnimInstance callback |

#### Pose Modifier Setters

| Method | Description | Called By |
|--------|-------------|-----------|
| `SetTargetAimPose(float)` | Целевой ADS alpha (0-1) | ADS Ability |
| `SetGripModifier(float)` | Модификатор хвата (0-1) | Weapon Actor / Attachment |
| `SetWeaponLowered(float)` | Опустить оружие (0-1) | Wall Detection / Obstacle |

#### Procedural Animation Setters

| Method | Description | Called By |
|--------|-------------|-----------|
| `AddRecoil(float)` | Добавить отдачу (decays автоматически) | Weapon Actor on Fire |
| `SetSwayMultiplier(float)` | Множитель раскачивания | Stamina System / Movement |

#### Snapshot API (called by AnimInstance)

```cpp
// В AnimInstance::UpdateWeaponData():
USuspenseCoreWeaponStanceComponent* StanceComp = CachedStanceComponent.Get();
const FSuspenseCoreWeaponStanceSnapshot Snapshot = StanceComp->GetStanceSnapshot();

// Snapshot содержит ВСЕ данные за один вызов:
bIsAiming = Snapshot.bIsAiming;
bIsFiring = Snapshot.bIsFiring;
bIsReloading = Snapshot.bIsReloading;
AimingAlpha = Snapshot.AimPoseAlpha;  // Уже интерполировано!
RecoilAlpha = Snapshot.RecoilAlpha;   // Уже с decay!
// ...
```

### AnimInstance Weapon Variables

#### Combat States (из Snapshot)

| Variable | Type | Description |
|----------|------|-------------|
| `bIsAiming` | `bool` | Прицеливание (ADS) активно |
| `bIsFiring` | `bool` | Стрельба активна |
| `bIsReloading` | `bool` | Перезарядка активна |
| `bIsHoldingBreath` | `bool` | Задержка дыхания |
| `bIsWeaponMontageActive` | `bool` | Монтаж оружия играет |

#### Pose Modifiers (из Snapshot)

| Variable | Type | Range | Description |
|----------|------|-------|-------------|
| `AimingAlpha` | `float` | 0-1 | Бленд Hip→ADS (автоматически интерполируется) |
| `GripModifier` | `float` | 0-1 | 0=обычный хват, 1=тактический |
| `WeaponLoweredAlpha` | `float` | 0-1 | 0=нормально, 1=полностью опущено |

#### Procedural Animation (из Snapshot)

| Variable | Type | Description |
|----------|------|-------------|
| `RecoilAlpha` | `float` | 0-1, текущий уровень отдачи (decay автоматический) |
| `WeaponSwayMultiplier` | `float` | Множитель раскачивания (стамина, движение) |

### Integration Examples

#### 1. Aiming (ADS) Ability

```cpp
// В вашей GAS Ability для прицеливания:
void USuspenseCoreAimAbility::ActivateAbility(...)
{
    Super::ActivateAbility(...);

    // Получаем StanceComponent
    if (USuspenseCoreWeaponStanceComponent* Stance =
        GetAvatarActor()->FindComponentByClass<USuspenseCoreWeaponStanceComponent>())
    {
        Stance->SetAiming(true);
    }
}

void USuspenseCoreAimAbility::EndAbility(...)
{
    if (USuspenseCoreWeaponStanceComponent* Stance = ...)
    {
        Stance->SetAiming(false);
    }
    Super::EndAbility(...);
}
```

#### 2. Weapon Fire (Recoil)

```cpp
// В вашем Weapon Actor при выстреле:
void ASuspenseCoreWeaponBase::Fire()
{
    // Логика выстрела...

    // Добавляем отдачу в анимацию
    if (USuspenseCoreWeaponStanceComponent* Stance =
        OwnerCharacter->FindComponentByClass<USuspenseCoreWeaponStanceComponent>())
    {
        Stance->SetFiring(true);
        Stance->AddRecoil(WeaponData.RecoilStrength);  // например 0.3f
    }
}

void ASuspenseCoreWeaponBase::StopFire()
{
    if (USuspenseCoreWeaponStanceComponent* Stance = ...)
    {
        Stance->SetFiring(false);
    }
}
```

#### 3. Wall Detection (Weapon Lowering)

```cpp
// В Weapon Actor или специальном компоненте:
void ASuspenseCoreWeaponBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Трейс вперед для обнаружения стен
    FHitResult Hit;
    FVector Start = GetMuzzleLocation();
    FVector End = Start + GetActorForwardVector() * 50.0f;

    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic))
    {
        // Рядом препятствие - опускаем оружие
        float LoweredAlpha = 1.0f - (Hit.Distance / 50.0f);
        if (USuspenseCoreWeaponStanceComponent* Stance = ...)
        {
            Stance->SetWeaponLowered(LoweredAlpha);
        }
    }
    else
    {
        if (USuspenseCoreWeaponStanceComponent* Stance = ...)
        {
            Stance->SetWeaponLowered(0.0f);
        }
    }
}
```

### Animation Blueprint Usage

#### State Machine Transitions

```
// Weapon State Machine

[Idle] ──── bIsWeaponDrawn ────▶ [Ready]
                                    │
          ┌─────────────────────────┼─────────────────────────┐
          │                         │                         │
          ▼                         ▼                         ▼
     [Aiming]                  [Reloading]               [Firing]
     bIsAiming                 bIsReloading              bIsFiring
```

#### Aim Pose Blending

```
// В AnimGraph:
Output Pose
└── Blend Poses by Float
    ├── A: Hip Fire Pose
    ├── B: ADS Pose
    └── Alpha: AimingAlpha    ← автоматически интерполируется!
```

#### Additive Recoil Layer

```
// В AnimGraph:
Output Pose
└── Apply Additive
    ├── Base: Main Pose
    └── Additive: Recoil Animation
        └── Alpha: RecoilAlpha    ← decay автоматический!
```

#### Weapon Lowered Blend

```
// В AnimGraph:
Output Pose
└── Blend Poses by Float
    ├── A: Normal Weapon Pose
    ├── B: Lowered Weapon Pose
    └── Alpha: WeaponLoweredAlpha
```

### Replication

WeaponStanceComponent автоматически реплицирует боевые состояния:

| Property | Replication |
|----------|-------------|
| `CurrentWeaponType` | Yes (OnRep) |
| `bWeaponDrawn` | Yes (OnRep) |
| `bIsAiming` | Yes (OnRep) |
| `bIsFiring` | Yes (OnRep) |
| `bIsReloading` | Yes (OnRep) |
| `bIsHoldingBreath` | Yes (OnRep) |
| `AimPoseAlpha` | No (local interp) |
| `RecoilAlpha` | No (local visual) |
| `WeaponLoweredAlpha` | No (local visual) |

**Важно:** Визуальные модификаторы (Alpha значения) НЕ реплицируются - они вычисляются локально на каждом клиенте на основе реплицированных bool состояний.

### Configuration

#### WeaponStanceComponent Settings

```cpp
// В Blueprint или C++:
UPROPERTY(EditDefaultsOnly, Category="Weapon|Config")
float AimInterpSpeed = 15.0f;  // Скорость интерполяции AimPoseAlpha

UPROPERTY(EditDefaultsOnly, Category="Weapon|Config")
float RecoilRecoverySpeed = 8.0f;  // Скорость восстановления после отдачи
```

### Troubleshooting

#### Problem: AimingAlpha не интерполируется плавно

**Причина:** Вызов `SetAiming()` без использования `GetStanceSnapshot()`

**Решение:** Убедитесь что AnimInstance использует `GetStanceSnapshot()` - он возвращает уже интерполированные значения.

#### Problem: Recoil не затухает

**Причина:** Компонент не тикает

**Решение:** Проверьте что `PrimaryComponentTick.bCanEverTick = true` в конструкторе (установлено по умолчанию).

#### Problem: Состояния не синхронизируются в мультиплеере

**Причина:** Setters вызываются на клиенте

**Решение:** Вызывайте `SetAiming()`, `SetFiring()` и т.д. только на сервере. Компонент автоматически реплицирует изменения.

---

## EventBus Integration

### Movement Ability Events

Все movement abilities публикуют события в EventBus:

```cpp
// Sprint Events
SuspenseCore.Event.Ability.CharacterSprint.Activated
SuspenseCore.Event.Ability.CharacterSprint.Ended

// Crouch Events
SuspenseCore.Event.Ability.CharacterCrouch.Activated
SuspenseCore.Event.Ability.CharacterCrouch.Ended

// Jump Events
SuspenseCore.Event.Ability.CharacterJump.Activated
SuspenseCore.Event.Ability.CharacterJump.Ended
SuspenseCore.Event.Ability.CharacterJump.Landed
```

### Subscribing to Animation Events

```cpp
// Пример подписки на события для UI или звуков
void UMyComponent::SetupEventSubscriptions(USuspenseCoreEventBus* EventBus)
{
    if (!EventBus) return;

    // Подписка на приземление
    EventBus->Subscribe(
        FGameplayTag::RequestGameplayTag("SuspenseCore.Event.Ability.CharacterJump.Landed"),
        FSuspenseCoreEventDelegate::CreateUObject(this, &UMyComponent::OnCharacterLanded)
    );
}

void UMyComponent::OnCharacterLanded(FGameplayTag EventTag, const FSuspenseCoreEventData& Data)
{
    // Проиграть звук приземления, VFX и т.д.
}
```

---

## Checklist for New Animations

### Before Adding New Animation States

- [ ] Movement state enum обновлен (`ESuspenseCoreMovementState`)
- [ ] AnimInstance содержит необходимые переменные
- [ ] Character обновляет MovementState корректно
- [ ] GAS ability (если нужна) создана и тестирована

### Before Adding New Weapon Animations

- [ ] DataTable row создан с правильным именем (GameplayTag)
- [ ] BlendSpaces настроены и указаны в DataTable
- [ ] Montages импортированы и добавлены в DataTable
- [ ] IK transforms заполнены

### Animation Blueprint Checklist

- [ ] Parent class: `USuspenseCoreCharacterAnimInstance`
- [ ] State Machine создан для всех состояний
- [ ] Transitions используют переменные из AnimInstance
- [ ] BlendSpaces подключены к правильным переменным
- [ ] НЕ используется EventGraph для обновления (всё в NativeUpdate)

### Code Review Checklist

- [ ] Никаких hardcoded значений в AnimInstance
- [ ] Все данные берутся из SSOT (Character, StanceComponent, GAS)
- [ ] Кэширование используется для производительности
- [ ] Fallback значения для отсутствующих компонентов

---

## Quick Reference

### Common Includes

```cpp
// AnimInstance
#include "SuspenseCore/Animation/SuspenseCoreCharacterAnimInstance.h"

// Animation Data
#include "SuspenseCore/Types/Animation/SuspenseCoreAnimationState.h"

// Character
#include "SuspenseCore/Characters/SuspenseCoreCharacter.h"

// GAS Attributes
#include "SuspenseCore/Attributes/SuspenseCoreMovementAttributeSet.h"
```

### Variable Quick Reference

```cpp
// Movement (use in State Machine transitions)
MovementState    // Enum state
bIsSprinting     // Sprint active
bIsCrouching     // Crouch active
bIsInAir         // Jumping or Falling
bIsOnGround      // On ground

// Velocity (use in BlendSpaces)
Speed            // Full velocity
GroundSpeed      // Horizontal only
NormalizedSpeed  // 0-1 relative to MaxWalkSpeed
MoveForward      // -1..1
MoveRight        // -1..1

// GAS (for NormalizedSpeed calculation)
MaxWalkSpeed
MaxSprintSpeed
MaxCrouchSpeed
MaxAimSpeed
```

---

## Files Reference

| File | Module | Purpose |
|------|--------|---------|
| `SuspenseCoreCharacterAnimInstance.h/cpp` | PlayerCore | SSOT AnimInstance |
| `SuspenseCoreCharacter.h` | PlayerCore | MovementState enum |
| `SuspenseCoreMovementAttributeSet.h` | GAS | Speed attributes |
| `SuspenseCoreCharacterSprintAbility.h` | GAS | Sprint ability |
| `SuspenseCoreCharacterCrouchAbility.h` | GAS | Crouch ability |
| `SuspenseCoreCharacterJumpAbility.h` | GAS | Jump ability |
| `SuspenseCoreSettings.h` | BridgeSystem | WeaponAnimationsTable reference |
| `SuspenseCoreAnimationState.h` | BridgeSystem | FAnimationStateData struct |

---

**Remember:** AnimInstance - единственный источник истины. Не добавляй логику в EventGraph ABP. Все данные должны обновляться в `NativeUpdateAnimation()`.
