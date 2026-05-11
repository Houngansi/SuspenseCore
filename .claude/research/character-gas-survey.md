# Character + GAS Survey — SuspenseCore (UE5.7)

Read-only survey of `Source/PlayerCore` and `Source/GAS`. All claims cite `file:line`. No source files edited; no git commits.

Scope: `ASuspenseCoreCharacter`, `ASuspenseCorePlayerState`, `USuspenseCoreAbilitySystemComponent`, the 6 AttributeSets, the four "character-adjacent" components (split below), the 3 movement abilities (`GA_CharacterSprint/Crouch/Jump`), `ASuspenseCorePlayerController`, and repo-wide CMC scan.

Assumptions confirmed:
- ASC on PlayerState (`SuspenseCorePlayerState.cpp:49–53`).
- Character is vanilla `ACharacter` + stock `UCharacterMovementComponent` (`SuspenseCoreCharacter.h:96`).
- Component split is REAL — `RecoilConvergenceComponent` and `CameraShakeComponent` live in **PlayerCore** (`SuspenseCoreCharacter.cpp:22–23`); `WeaponStanceComponent` and `QuickSlotComponent` live in **EquipmentSystem** (`SuspenseCoreCharacter.cpp:17–20`, gated by `WITH_EQUIPMENT_SYSTEM=1`).
- 6 AttributeSet headers present in `Source/GAS/Public/SuspenseCore/Attributes/` (listed §A).
- No Iris / Mover.
- 2 `MARK_PROPERTY_DIRTY_FROM_NAME` usages, both on `USuspenseCoreEquipmentReplicationManager` (`SuspenseCoreEquipmentReplicationManager.cpp:219, 269`).

---

## §A. Replication map

### A.1 `ASuspenseCoreCharacter` (PlayerCore)

No `UPROPERTY(Replicated)` declared on Character. State flags `bIsSprinting`, `bHasWeapon`, `bIsInScopeView`, `CurrentMovementState`, `MoveForwardValue`, `MoveRightValue`, `CurrentCrouchAlpha`, `TargetCrouchAlpha`, `CameraShake` are all local-only.

Verified: no matches for `Replicated` in `SuspenseCoreCharacter.h` or `SuspenseCoreCharacter.cpp`.

| Property | Type | Condition | RepNotify | Setter? | Direct-mutation sites |
| --- | --- | --- | --- | --- | --- |
| (none) | — | — | — | — | — |

Note: Character RELIES on stock `ACharacter`/`UCharacterMovementComponent` replication (`bIsCrouched`, `Movement->Velocity`, `bPressedJump`, RootMotion, etc.) via engine-provided `GetLifetimeReplicatedProps`. Crouch is triggered via `Crouch()` / `UnCrouch()` (`SuspenseCoreCharacter.cpp:339, 343, 388, 399`) and replicates through the engine.

### A.2 `ASuspenseCorePlayerState` (PlayerCore)

Replication declared in `SuspenseCorePlayerState.cpp:192–215`. All properties use legacy (non push-model) replication.

| Property | Type | Condition | RepNotify | Setter? | Direct-mutation sites |
| --- | --- | --- | --- | --- | --- |
| `AbilitySystemComponent` | `USuspenseCoreAbilitySystemComponent*` | COND_None | none | n/a | `SuspenseCorePlayerState.cpp:49` (CreateDefaultSubobject) |
| `InventoryComponent` | `UActorComponent*` | COND_None | none | n/a | `SuspenseCorePlayerState.cpp:62` |
| `EquipmentDataStore` | `UActorComponent*` | COND_None | none | n/a | `SuspenseCorePlayerState.cpp:74` |
| `EquipmentTxnProcessor` | `UActorComponent*` | COND_None | none | n/a | `SuspenseCorePlayerState.cpp:78` |
| `EquipmentOps` | `UActorComponent*` | COND_None | none | n/a | `SuspenseCorePlayerState.cpp:82` |
| `EquipmentPrediction` | `UActorComponent*` | COND_None | none | n/a | `SuspenseCorePlayerState.cpp:86` |
| `EquipmentReplication` | `UActorComponent*` | COND_None | none | n/a | `SuspenseCorePlayerState.cpp:90` |
| `EquipmentNetworkDispatcher` | `UActorComponent*` | COND_None | none | n/a | `SuspenseCorePlayerState.cpp:94` |
| `EquipmentEventDispatcher` | `UActorComponent*` | COND_None | none | n/a | `SuspenseCorePlayerState.cpp:98` |
| `WeaponStateManager` | `UActorComponent*` | COND_None | none | n/a | `SuspenseCorePlayerState.cpp:102` |
| `EquipmentInventoryBridge` | `UActorComponent*` | COND_None | none | n/a | `SuspenseCorePlayerState.cpp:106` |
| `EquipmentUIProvider` | `UActorComponent*` | declared `Replicated` in header but **NOT in DOREPLIFETIME list** | none | n/a | `SuspenseCorePlayerState.cpp:111` |
| `PlayerLevel` | `int32` | COND_None, `REPNOTIFY_Always` | `OnRep_PlayerLevel` (`SuspenseCorePlayerState.cpp:461`) | `SetPlayerLevel` (`SuspenseCorePlayerState.cpp:282`) | `SuspenseCorePlayerState.cpp:290` |
| `TeamId` | `int32` | COND_None, `REPNOTIFY_Always` | `OnRep_TeamId` (`SuspenseCorePlayerState.cpp:473`) | `SetTeamId` (`SuspenseCorePlayerState.cpp:301`) | `SuspenseCorePlayerState.cpp:309` |
| `CharacterClassId` | `FName` | COND_None, `REPNOTIFY_Always` | `OnRep_CharacterClassId` (`SuspenseCorePlayerState.cpp:485`) | none | `SuspenseCorePlayerState.cpp:347, 625` |

DISCREPANCY: `EquipmentUIProvider` has `UPROPERTY(VisibleAnywhere, Replicated)` at `SuspenseCorePlayerState.h:312` but is NOT in `GetLifetimeReplicatedProps` (`SuspenseCorePlayerState.cpp:192–215`). `SystemCoordinator` is declared without `Replicated` (`SuspenseCorePlayerState.h:316`).

### A.3 `USuspenseCoreAbilitySystemComponent` (GAS)

| Property | Type | Condition | RepNotify | Setter? | Direct-mutation sites |
| --- | --- | --- | --- | --- | --- |
| `bPublishAttributeEvents` | `bool` (not replicated) | — | — | `SetAttributeEventsEnabled` (`SuspenseCoreAbilitySystemComponent.cpp:57`) | `SuspenseCoreAbilitySystemComponent.cpp:59` |
| `CachedEventBus` | `TWeakObjectPtr` (not replicated, transient) | — | — | — | `SuspenseCoreAbilitySystemComponent.cpp:155` |

`SetIsReplicatedByDefault(true)` (`SuspenseCoreAbilitySystemComponent.cpp:15`); `ReplicationMode = EGameplayEffectReplicationMode::Mixed` (`SuspenseCoreAbilitySystemComponent.cpp:16`). All GAS-internal replicated state (abilities, GE) inherited from `UAbilitySystemComponent`.

### A.4 `USuspenseCoreAttributeSet` (GAS)

`GetLifetimeReplicatedProps` at `SuspenseCoreAttributeSet.cpp:44–59`. All `COND_None, REPNOTIFY_Always`. Setters are generated by `SUSPENSECORE_ATTRIBUTE_ACCESSORS` macro (`SuspenseCoreAttributeSet.h:17–21`).

| Property | Type | Condition | RepNotify | Setter? | Direct-mutation sites |
| --- | --- | --- | --- | --- | --- |
| `Health` | `FGameplayAttributeData` | COND_None | `OnRep_Health` (`.cpp:211`) | macro | `SuspenseCoreAttributeSet.cpp:24, 93, 117`; `SuspenseCorePlayerState.cpp:382` |
| `MaxHealth` | `FGameplayAttributeData` | COND_None | `OnRep_MaxHealth` (`.cpp:217`) | macro | `SuspenseCoreAttributeSet.cpp:25` |
| `HealthRegen` | `FGameplayAttributeData` | COND_None | `OnRep_HealthRegen` (`.cpp:223`) | macro | `SuspenseCoreAttributeSet.cpp:26` |
| `Stamina` | `FGameplayAttributeData` | COND_None | `OnRep_Stamina` (`.cpp:229`) | macro | `SuspenseCoreAttributeSet.cpp:28, 143–159`; `SuspenseCorePlayerState.cpp:386` |
| `MaxStamina` | `FGameplayAttributeData` | COND_None | `OnRep_MaxStamina` (`.cpp:235`) | macro | `SuspenseCoreAttributeSet.cpp:29` |
| `StaminaRegen` | `FGameplayAttributeData` | COND_None | `OnRep_StaminaRegen` (`.cpp:241`) | macro | `SuspenseCoreAttributeSet.cpp:30` |
| `Armor` | `FGameplayAttributeData` | COND_None | `OnRep_Armor` (`.cpp:247`) | macro | `SuspenseCoreAttributeSet.cpp:32` |
| `AttackPower` | `FGameplayAttributeData` | COND_None | `OnRep_AttackPower` (`.cpp:253`) | macro | `SuspenseCoreAttributeSet.cpp:33` |
| `MovementSpeed` | `FGameplayAttributeData` | COND_None | `OnRep_MovementSpeed` (`.cpp:259`) | macro | `SuspenseCoreAttributeSet.cpp:34` |

`IncomingDamage` / `IncomingHealing` are meta-attributes — declared `UPROPERTY(BlueprintReadOnly)` (`SuspenseCoreAttributeSet.h:112, 117`), NOT replicated (not in `DOREPLIFETIME` list).

### A.5 `USuspenseCoreMovementAttributeSet` (GAS)

`GetLifetimeReplicatedProps` at `SuspenseCoreMovementAttributeSet.cpp:45–66`.

| Property | Type | Condition | RepNotify | Setter? | Direct-mutation sites |
| --- | --- | --- | --- | --- | --- |
| `WalkSpeed` | `FGameplayAttributeData` | COND_None | `OnRep_WalkSpeed` (`.cpp:163`) | macro | `SuspenseCoreMovementAttributeSet.cpp:15` |
| `SprintSpeed` | `FGameplayAttributeData` | COND_None | `OnRep_SprintSpeed` (`.cpp:169`) | macro | `SuspenseCoreMovementAttributeSet.cpp:16` |
| `CrouchSpeed` | `FGameplayAttributeData` | COND_None | `OnRep_CrouchSpeed` (`.cpp:174`) | macro | `SuspenseCoreMovementAttributeSet.cpp:17` |
| `ProneSpeed` | `FGameplayAttributeData` | COND_None | `OnRep_ProneSpeed` (`.cpp:180`) | macro | `SuspenseCoreMovementAttributeSet.cpp:18` |
| `AimSpeed` | `FGameplayAttributeData` | COND_None | `OnRep_AimSpeed` (`.cpp:185`) | macro | `SuspenseCoreMovementAttributeSet.cpp:19` |
| `BackwardSpeedMultiplier` | `FGameplayAttributeData` | COND_None | `OnRep_BackwardSpeedMultiplier` (`.cpp:190`) | macro | `SuspenseCoreMovementAttributeSet.cpp:22` |
| `StrafeSpeedMultiplier` | `FGameplayAttributeData` | COND_None | `OnRep_StrafeSpeedMultiplier` (`.cpp:195`) | macro | `SuspenseCoreMovementAttributeSet.cpp:23` |
| `JumpHeight` | `FGameplayAttributeData` | COND_None | `OnRep_JumpHeight` (`.cpp:200`) | macro | `SuspenseCoreMovementAttributeSet.cpp:26` |
| `MaxJumpCount` | `FGameplayAttributeData` | COND_None | `OnRep_MaxJumpCount` (`.cpp:206`) | macro | `SuspenseCoreMovementAttributeSet.cpp:27` |
| `AirControl` | `FGameplayAttributeData` | COND_None | `OnRep_AirControl` (`.cpp:211`) | macro | `SuspenseCoreMovementAttributeSet.cpp:28` |
| `TurnRate` | `FGameplayAttributeData` | COND_None | `OnRep_TurnRate` (`.cpp:217`) | macro | `SuspenseCoreMovementAttributeSet.cpp:31` |
| `AimTurnRateMultiplier` | `FGameplayAttributeData` | COND_None | `OnRep_AimTurnRateMultiplier` (`.cpp:222`) | macro | `SuspenseCoreMovementAttributeSet.cpp:32` |
| `GroundAcceleration` | `FGameplayAttributeData` | COND_None | `OnRep_GroundAcceleration` (`.cpp:227`) | macro | `SuspenseCoreMovementAttributeSet.cpp:35` |
| `GroundDeceleration` | `FGameplayAttributeData` | COND_None | `OnRep_GroundDeceleration` (`.cpp:233`) | macro | `SuspenseCoreMovementAttributeSet.cpp:36` |
| `AirAcceleration` | `FGameplayAttributeData` | COND_None | `OnRep_AirAcceleration` (`.cpp:239`) | macro | `SuspenseCoreMovementAttributeSet.cpp:37` |
| `CurrentWeight` | `FGameplayAttributeData` | COND_None | `OnRep_CurrentWeight` (`.cpp:244`) | macro | `SuspenseCoreMovementAttributeSet.cpp:40` |
| `MaxWeight` | `FGameplayAttributeData` | COND_None | `OnRep_MaxWeight` (`.cpp:250`) | macro | `SuspenseCoreMovementAttributeSet.cpp:41` |
| `WeightSpeedPenalty` | `FGameplayAttributeData` | NOT replicated (computed) | none | macro | `SuspenseCoreMovementAttributeSet.cpp:42, 267, 277, 284, 290` |

### A.6 `USuspenseCoreShieldAttributeSet` (GAS)

Declared in `SuspenseCoreShieldAttributeSet.h:38–198`. Replication via `DOREPLIFETIME` (handler list in `SuspenseCoreShieldAttributeSet.h:146–168`). All `COND_None`. Setters via macro.

| Property | Type | Condition | RepNotify | Setter? | Direct-mutation sites |
| --- | --- | --- | --- | --- | --- |
| `Shield` | `FGameplayAttributeData` | COND_None | `OnRep_Shield` (`.h:147`) | macro | (constructor, see .cpp — not deep-dived) |
| `MaxShield` | `FGameplayAttributeData` | COND_None | `OnRep_MaxShield` (`.h:150`) | macro | (constructor) |
| `ShieldRegen` | `FGameplayAttributeData` | COND_None | `OnRep_ShieldRegen` (`.h:153`) | macro | (constructor) |
| `ShieldRegenDelay` | `FGameplayAttributeData` | COND_None | `OnRep_ShieldRegenDelay` (`.h:156`) | macro | (constructor) |
| `ShieldBreakCooldown` | `FGameplayAttributeData` | COND_None | `OnRep_ShieldBreakCooldown` (`.h:159`) | macro | (constructor) |
| `ShieldBreakRecoveryPercent` | `FGameplayAttributeData` | COND_None | `OnRep_ShieldBreakRecoveryPercent` (`.h:162`) | macro | (constructor) |
| `ShieldDamageReduction` | `FGameplayAttributeData` | COND_None | `OnRep_ShieldDamageReduction` (`.h:165`) | macro | (constructor) |
| `ShieldOverflowDamage` | `FGameplayAttributeData` | COND_None | `OnRep_ShieldOverflowDamage` (`.h:168`) | macro | (constructor) |
| `IncomingShieldDamage` | `FGameplayAttributeData` | NOT replicated (meta) | — | macro | meta-only (`.h:103`) |
| `IncomingShieldHealing` | `FGameplayAttributeData` | NOT replicated (meta) | — | macro | meta-only (`.h:108`) |

### A.7 `USuspenseCoreWeaponAttributeSet` (GAS)

Declared at `SuspenseCoreWeaponAttributeSet.h:28–207`. RepNotifies declared at `.h:180–202`.

| Property | Type | Condition | RepNotify | Setter? | Direct-mutation sites |
| --- | --- | --- | --- | --- | --- |
| `BaseDamage` | `FGameplayAttributeData` | COND_None | `OnRep_BaseDamage` (`.h:180`) | macro | constructor / `InitializeFromData` (`.h:49`) — BROAD MUTATION |
| `RateOfFire` | `FGameplayAttributeData` | COND_None | `OnRep_RateOfFire` (`.h:181`) | macro | `InitializeFromData` |
| `EffectiveRange` | `FGameplayAttributeData` | COND_None | `OnRep_EffectiveRange` (`.h:182`) | macro | `InitializeFromData` |
| `MaxRange` | `FGameplayAttributeData` | COND_None | `OnRep_MaxRange` (`.h:183`) | macro | `InitializeFromData` |
| `MagazineSize` | `FGameplayAttributeData` | COND_None | `OnRep_MagazineSize` (`.h:184`) | macro | `InitializeFromData` |
| `TacticalReloadTime` | `FGameplayAttributeData` | COND_None | `OnRep_TacticalReloadTime` (`.h:185`) | macro | `InitializeFromData` |
| `FullReloadTime` | `FGameplayAttributeData` | COND_None | `OnRep_FullReloadTime` (`.h:186`) | macro | `InitializeFromData` |
| `MOA` | `FGameplayAttributeData` | COND_None | `OnRep_MOA` (`.h:187`) | macro | `InitializeFromData` |
| `HipFireSpread` | `FGameplayAttributeData` | COND_None | `OnRep_HipFireSpread` (`.h:188`) | macro | `InitializeFromData` |
| `AimSpread` | `FGameplayAttributeData` | COND_None | `OnRep_AimSpread` (`.h:189`) | macro | `InitializeFromData` |
| `VerticalRecoil` | `FGameplayAttributeData` | COND_None | `OnRep_VerticalRecoil` (`.h:190`) | macro | `InitializeFromData` |
| `HorizontalRecoil` | `FGameplayAttributeData` | COND_None | `OnRep_HorizontalRecoil` (`.h:191`) | macro | `InitializeFromData` |
| `ConvergenceSpeed` | `FGameplayAttributeData` | COND_None | `OnRep_ConvergenceSpeed` (`.h:192`) | macro | `InitializeFromData` |
| `ConvergenceDelay` | `FGameplayAttributeData` | COND_None | `OnRep_ConvergenceDelay` (`.h:193`) | macro | `InitializeFromData` |
| `RecoilAngleBias` | `FGameplayAttributeData` | COND_None | `OnRep_RecoilAngleBias` (`.h:194`) | macro | `InitializeFromData` |
| `RecoilPatternStrength` | `FGameplayAttributeData` | COND_None | `OnRep_RecoilPatternStrength` (`.h:195`) | macro | `InitializeFromData` |
| `Durability` | `FGameplayAttributeData` | COND_None | `OnRep_Durability` (`.h:196`) | macro | `InitializeFromData` |
| `MaxDurability` | `FGameplayAttributeData` | COND_None | `OnRep_MaxDurability` (`.h:197`) | macro | `InitializeFromData` |
| `MisfireChance` | `FGameplayAttributeData` | COND_None | `OnRep_MisfireChance` (`.h:198`) | macro | `InitializeFromData` |
| `JamChance` | `FGameplayAttributeData` | COND_None | `OnRep_JamChance` (`.h:199`) | macro | `InitializeFromData` |
| `Ergonomics` | `FGameplayAttributeData` | COND_None | `OnRep_Ergonomics` (`.h:200`) | macro | `InitializeFromData` |
| `AimDownSightTime` | `FGameplayAttributeData` | COND_None | `OnRep_AimDownSightTime` (`.h:201`) | macro | `InitializeFromData` |
| `WeaponWeight` | `FGameplayAttributeData` | COND_None | `OnRep_WeaponWeight` (`.h:202`) | macro | `InitializeFromData` |

### A.8 `USuspenseCoreAmmoAttributeSet` (GAS)

Declared at `SuspenseCoreAmmoAttributeSet.h:28–162`. 15 replicated attributes (`OnRep_*` at `.h:143–157`). All `COND_None`. All initialized by `InitializeFromData(FSuspenseCoreAmmoAttributeRow)` (`.h:49`).

Attributes: `BaseDamage`, `ArmorPenetration`, `StoppingPower`, `FragmentationChance`, `MuzzleVelocity`, `DragCoefficient`, `BulletMass`, `EffectiveRange`, `AccuracyModifier`, `RecoilModifier`, `RicochetChance`, `TracerVisibility`, `IncendiaryDamage`, `WeaponDegradationRate`, `MisfireChance` — see `.h:62–136`. All carry standard RepNotify pattern; no direct C++ mutators outside `InitializeFromData`.

### A.9 `USuspenseCoreProgressionAttributeSet` (GAS)

Declared at `SuspenseCoreProgressionAttributeSet.h:37–264`. 14 replicated attributes (`OnRep_*` at `.h:194–234`):

`Level`, `MaxLevel`, `Experience`, `ExperienceToNextLevel`, `ExperienceMultiplier`, `Reputation`, `ReputationMultiplier`, `SoftCurrency`, `HardCurrency`, `SkillPoints`, `AttributePoints`, `PrestigeLevel`, `SeasonRank`, `SeasonExperience` — see `.h:49–141`. All `COND_None`. `IncomingExperience` is meta-only (`.h:79`).

### A.10 `USuspenseCoreQuickSlotComponent` (EquipmentSystem; nested in Character via `WITH_EQUIPMENT_SYSTEM`)

Per scope: replication surface only. `GetLifetimeReplicatedProps` at `SuspenseCoreQuickSlotComponent.cpp:63–64`.

| Property | Type | Condition | RepNotify | Setter? | Direct-mutation sites |
| --- | --- | --- | --- | --- | --- |
| `QuickSlots` | `TArray<FSuspenseCoreQuickSlot>` | COND_None | `OnRep_QuickSlots` (`.h:280`) | n/a (mutated via Server RPCs §B) | BROAD MUTATION (not deep-dived per scope) |
| `StoredMagazines` | `TArray<FSuspenseCoreMagazineInstance>` | COND_None | none | n/a | BROAD MUTATION (not deep-dived per scope) |

### A.11 `USuspenseCoreWeaponStanceComponent` (EquipmentSystem; nested in Character via `WITH_EQUIPMENT_SYSTEM`)

`GetLifetimeReplicatedProps` at `SuspenseCoreWeaponStanceComponent.cpp:50–59`. All `DOREPLIFETIME_CONDITION_NOTIFY(..., COND_None, REPNOTIFY_Always)`.

| Property | Type | Condition | RepNotify | Setter? | Direct-mutation sites |
| --- | --- | --- | --- | --- | --- |
| `CurrentWeaponType` | `FGameplayTag` | COND_None | `OnRep_WeaponType` (`.h:347`) | `SetWeaponStance` / `ClearWeaponStance` (`.h:177, 180`) | not deep-dived per scope |
| `bWeaponDrawn` | `bool` | COND_None | `OnRep_DrawnState` (`.h:351`) | `SetWeaponDrawnState` (`.h:183`) | not deep-dived per scope |
| `bIsAiming` | `bool` | COND_None | `OnRep_CombatState` (`.h:353`) | `SetAiming` (`.h:192`) | not deep-dived per scope |
| `bIsFiring` | `bool` | COND_None | `OnRep_CombatState` | `SetFiring` (`.h:196`) | not deep-dived per scope |
| `bIsReloading` | `bool` | COND_None | `OnRep_CombatState` | `SetReloading` (`.h:200`) | not deep-dived per scope |
| `bIsHoldingBreath` | `bool` | COND_None | `OnRep_CombatState` | `SetHoldingBreath` (`.h:204`) | not deep-dived per scope |
| `bIsWeaponBlocked` | `bool` | COND_None | `OnRep_CombatState` | `SetWeaponBlocked` (`.h:212`) | not deep-dived per scope |

Note: 5 different bools share `OnRep_CombatState` callback.

### A.12 `USuspenseCoreRecoilConvergenceComponent` (PlayerCore)

No `UPROPERTY(Replicated)` declared.

`AccumulatedPitch`, `AccumulatedYaw` are `UPROPERTY(VisibleAnywhere)` (`SuspenseCoreRecoilConvergenceComponent.h:114, 118`) — debug-only, local. Component operates entirely from EventBus subscription on each client (`SuspenseCoreRecoilConvergenceComponent.cpp:107`).

### A.13 `USuspenseCoreCameraShakeComponent` (PlayerCore)

No `UPROPERTY(Replicated)` declared. All shake state is local — shakes triggered via EventBus on each client (`SuspenseCoreCameraShakeComponent.cpp:92–113`).

### A.14 `ASuspenseCorePlayerController` (PlayerCore)

No `UPROPERTY(Replicated)` declared. Verified via grep.

---

## §B. RPC inventory

Verified: `grep -rn "UFUNCTION.*Server\|UFUNCTION.*Client\|UFUNCTION.*NetMulticast" Source/PlayerCore Source/GAS`.

### B.1 PlayerCore

**No RPCs declared.** Character/PlayerState/PlayerController do not define `UFUNCTION(Server|Client|NetMulticast)`. Mutations of replicated state on `ASuspenseCorePlayerState` (e.g. `SetPlayerLevel`, `SetTeamId`) check `HasAuthority()` and assume server-only callers (`SuspenseCorePlayerState.cpp:284, 303, 322, 369`).

### B.2 GAS

| Class | Function | Signature | Reliable | _Validate | file:line |
| --- | --- | --- | --- | --- | --- |
| `USuspenseCoreBaseFireAbility` | `ServerFireShot` | `(const FWeaponShotParams&)` | Server, Reliable | YES (`_Validate` at `.h:621`) | `SuspenseCoreBaseFireAbility.h:619` |
| `USuspenseCoreBaseFireAbility` | `ClientReceiveShotResult` | `(const FSuspenseCoreShotResult&)` | Client, Reliable | n/a (no Validate for Client RPC) | `SuspenseCoreBaseFireAbility.h:627` |
| `USuspenseCoreInteractAbility` | `ServerPerformInteraction` | `(AActor* TargetActor)` | Server, Reliable | NO (no `WithValidation`) | `SuspenseCoreInteractAbility.h:112` |
| `USuspenseCoreInteractAbility` | `ClientInteractionResult` | `(bool bSuccess, AActor* TargetActor)` | Client, Reliable | n/a | `SuspenseCoreInteractAbility.h:116` |
| `USuspenseCoreReloadAbility` | `Server_ExecuteReload` | `(ESuspenseCoreReloadType, int32 MagazineSlotIndex, const FSuspenseCoreMagazineInstance&)` | Server, Reliable, WithValidation | YES (`_Validate` at `.h:214`) | `SuspenseCoreReloadAbility.h:212` |

### B.3 Character-adjacent components

`USuspenseCoreQuickSlotComponent` (EquipmentSystem; nested in Character) — replication surface only per scope:

| Function | Signature | Reliable | _Validate | file:line |
| --- | --- | --- | --- | --- |
| `Server_AssignItemToSlot` | `(int32 SlotIndex, const FGuid&, FName ItemID)` | Server, Reliable, WithValidation | YES | `SuspenseCoreQuickSlotComponent.h:263` |
| `Server_ClearSlot` | `(int32 SlotIndex)` | Server, Reliable, WithValidation | YES | `SuspenseCoreQuickSlotComponent.h:266` |
| `Server_UseQuickSlot` | `(int32 SlotIndex)` | Server, Reliable, WithValidation | YES | `SuspenseCoreQuickSlotComponent.h:269` |
| `Server_QuickSwapMagazine` | `(int32 SlotIndex, bool bEmergencyDrop)` | Server, Reliable, WithValidation | YES | `SuspenseCoreQuickSlotComponent.h:272` |

`USuspenseCoreWeaponStanceComponent`: no RPCs declared (state changes via authority-guarded direct setters or replication only — not deep-dived).

`USuspenseCoreRecoilConvergenceComponent`, `USuspenseCoreCameraShakeComponent`: no RPCs declared.

---

## §C. CMC dependency map

Verified: `grep -rn "GetCharacterMovement()" Source/`.

### C.1 `GetCharacterMovement()` call sites (PlayerCore + GAS + adjacent)

| file:line | Context |
| --- | --- |
| `SuspenseCoreCharacter.cpp:146` | constructor — sets `MaxWalkSpeed`, `MaxWalkSpeedCrouched`, `bOrientRotationToMovement`, `bUseControllerDesiredRotation`, `NavAgentProps.bCanCrouch`, `bCanWalkOffLedgesWhenCrouching`, `SetCrouchedHalfHeight(40.0f)` |
| `SuspenseCoreCharacter.cpp:355` | `MovementCanSprint_Implementation` — reads `IsFalling()` |
| `SuspenseCoreCharacter.cpp:374` | `MovementCanCrouch_Implementation` — reads `CanEverCrouch()`, `IsFalling()` |
| `SuspenseCoreCharacter.cpp:410` | `MovementIsInAir_Implementation` — reads `IsFalling()` |
| `SuspenseCoreCharacter.cpp:564` | `UpdateMovementState` — reads `IsFalling()` |
| `SuspenseCoreCharacter.cpp:612` | `UpdateMovementSpeed` — writes `MaxWalkSpeed` |
| `SuspenseCoreCharacter.cpp:645` | `InitializeCrouchHeights` — reads `GetCrouchedHalfHeight()` |
| `SuspenseCoreCharacterAnimInstance.cpp:82` | `UpdateCachedReferences` — caches `CachedMovementComponent` |
| `SuspenseCoreCharacterAnimInstance.cpp:753–757` | reads `MaxWalkSpeed`, `MaxWalkSpeedCrouched`, `JumpZVelocity` |
| `SuspenseCoreCharacterSprintAbility.cpp:76` | reads `IsFalling()` |
| `SuspenseCoreCharacterCrouchAbility.cpp:70` | reads `CanEverCrouch()`, `IsFalling()` |
| `SuspenseCoreCharacterJumpAbility.cpp:79` | `CanActivateAbility` — reads `IsFalling()` |
| `SuspenseCoreCharacterJumpAbility.cpp:221` | `IsCharacterGrounded` — reads `IsFalling()` |
| `SuspenseCoreCharacterJumpAbility.cpp:284` | `PerformJump` — reads + writes `JumpZVelocity` (temp swap) |
| `SuspenseCoreAttributeSet.cpp:132` | `PostGameplayEffectExecute` (MovementSpeed branch) — writes `MaxWalkSpeed = BaseWalkSpeed * GetMovementSpeed()` |
| `SuspenseCoreAttributeSet.cpp:352` | `HandleDeath` — `DisableMovement()`, `StopMovementImmediately()` |
| `SuspenseCoreMovementAttributeSet.cpp:147` | `ApplySpeedsToCharacter` — writes `MaxWalkSpeed`, `MaxWalkSpeedCrouched`, `JumpZVelocity`, `AirControl`, `MaxAcceleration`, `BrakingDecelerationWalking` |
| `SuspenseCoreBaseFireAbility.cpp:278` | reads movement state (`IsFalling()` or similar) — confirmed call site |
| `SuspenseCoreWeaponAsyncTask_PerformTrace.cpp:367` | weapon task — reads movement state |

Also (out-of-scope but listed by repo grep): `Source/EnemySystem/*` 9 hits; one `SuspenseCoreCharacterAnimInstance.cpp:82` (already listed).

### C.2 CMC property reads/writes (in scope)

| Property | Read sites | Write sites |
| --- | --- | --- |
| `MaxWalkSpeed` | `SuspenseCoreCharacterAnimInstance.cpp:170, 172, 753` | `SuspenseCoreCharacter.cpp:148, 630`; `SuspenseCoreAttributeSet.cpp:134`; `SuspenseCoreMovementAttributeSet.cpp:151` |
| `MaxWalkSpeedCrouched` | `SuspenseCoreCharacterAnimInstance.cpp:754` | `SuspenseCoreCharacter.cpp:149`; `SuspenseCoreMovementAttributeSet.cpp:152` |
| `MaxAcceleration` | — | `SuspenseCoreMovementAttributeSet.cpp:155` |
| `AirControl` | — | `SuspenseCoreMovementAttributeSet.cpp:154` |
| `GroundFriction` | — | none (in scope) — only `Source/EnemySystem/.../SuspenseCoreEnemyCharacter.cpp:41` (out of scope) |
| `JumpZVelocity` | `SuspenseCoreCharacterAnimInstance.cpp:757` | `SuspenseCoreCharacterJumpAbility.cpp:289, 290, 292` (temp set/restore); `SuspenseCoreMovementAttributeSet.cpp:153` |
| `GravityScale` | — | none in PlayerCore/GAS (only `Source/EquipmentSystem/Private/.../SuspenseCoreGrenadeProjectile.cpp:80` `ProjectileGravityScale` — different property, projectile) |
| `RotationRate` | — | none in PlayerCore/GAS (only `Source/EnemySystem/*` out of scope) |
| `BrakingDecelerationWalking` | — | `SuspenseCoreMovementAttributeSet.cpp:156` |

### C.3 CMC override sites

Verified: `grep -rn "UpdateFromCompressedFlags\|GetCompressedFlags\|OnMovementModeChanged" Source/` returns **zero hits**. No custom `UCharacterMovementComponent` subclass exists. No custom `FSavedMove`. Stock UE CMC only.

---

## §D. AnimBP contract

Variables on `ASuspenseCoreCharacter` that are `BlueprintReadOnly` / `BlueprintReadWrite` and animation-facing:

| Variable | Access | Writer | Source | file:line |
| --- | --- | --- | --- | --- |
| `CurrentMovementState` | `BlueprintReadOnly` | `UpdateMovementState()` | called from `Tick` | declared `SuspenseCoreCharacter.h:528`; written `SuspenseCoreCharacter.cpp:587`; tick caller `SuspenseCoreCharacter.cpp:208` |
| `bIsSprinting` | `BlueprintReadOnly` | `StartSprinting()` / `StopSprinting()` | input event (Sprint ability) | declared `SuspenseCoreCharacter.h:531`; written `SuspenseCoreCharacter.cpp:311, 325` |
| `bHasWeapon` | `BlueprintReadOnly` | `SetHasWeapon_Implementation()` | weapon attach event | declared `SuspenseCoreCharacter.h:535`; written `SuspenseCoreCharacter.cpp:468` |
| `bIsInScopeView` | `BlueprintReadOnly` | `SwitchToScopeCamera()` | ADS interface | declared `SuspenseCoreCharacter.h:538`; written `SuspenseCoreCharacter.cpp:1391, 1409, 1456` |
| `CurrentCrouchAlpha` | `BlueprintReadOnly` | `UpdateSmoothCrouch()` | called from `Tick` | declared `SuspenseCoreCharacter.h:550`; written `SuspenseCoreCharacter.cpp:665, 670`; tick caller `SuspenseCoreCharacter.cpp:210` |
| `TargetCrouchAlpha` | `BlueprintReadOnly` | `MovementStartCrouch_Implementation()` / `MovementStopCrouch_Implementation()` | crouch ability via interface | declared `SuspenseCoreCharacter.h:554`; written `SuspenseCoreCharacter.cpp:384, 395` |
| `MoveForwardValue` | (public field, no UPROPERTY) | `UpdateAnimationValues()` | called from `Tick` | declared `SuspenseCoreCharacter.h:575`; written `SuspenseCoreCharacter.cpp:599`; tick caller `SuspenseCoreCharacter.cpp:209` |
| `MoveRightValue` | (public field, no UPROPERTY) | `UpdateAnimationValues()` | called from `Tick` | declared `SuspenseCoreCharacter.h:576`; written `SuspenseCoreCharacter.cpp:600`; tick caller `SuspenseCoreCharacter.cpp:209` |
| `CameraShake` | `BlueprintReadWrite` | (BP-set or external) | comment says "from Blueprint" (`.h:582`) | declared `SuspenseCoreCharacter.h:585` — no C++ writer found |

Note: `MoveForwardValue` / `MoveRightValue` lack `UPROPERTY` macros (`SuspenseCoreCharacter.h:575–578`) but are publicly accessible via `GetMoveForwardValue()`/`GetMoveRightValue()` BlueprintCallable getters (`.h:189, 192`) and `GetAnimationForwardValue()`/`GetAnimationRightValue()` (`.h:194, 197`).

The AnimInstance `USuspenseCoreCharacterAnimInstance` reads these via `Character->GetMovementState()`, `Character->IsSprinting()`, `Character->HasMovementInput()` etc. in `UpdateMovementData()` (`SuspenseCoreCharacterAnimInstance.cpp:111–155`). It also has its own duplicate `CameraShake` `UPROPERTY(BlueprintReadWrite)` (`SuspenseCoreCharacterAnimInstance.h:437`).

---

## §E. Movement ability surface

### E.1 `USuspenseCoreCharacterSprintAbility`

- File: `SuspenseCoreCharacterSprintAbility.h:35` / `.cpp:18`
- `NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted` (`.cpp:29`)
- `InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor` (`.cpp:28`)
- Activation entry: `ActivateAbility()` at `.cpp:100`
- CMC state touched:
  - `CanActivateAbility` reads `GetCharacterMovement()->IsFalling()` (`.cpp:76–80`)
  - Speed change NOT applied by ability — comment `.cpp:196–198`: speed handled by `Character->UpdateMovementSpeed()` setting `CMC->MaxWalkSpeed = SprintSpeed` when `bIsSprinting=true`. Ability calls `ISuspenseCoreMovementInterface::Execute_MovementStartSprint(Avatar)` (`.cpp:115`) which routes to `Character::StartSprinting()` (`SuspenseCoreCharacter.cpp:362–365`).
- Attribute consumed: `Stamina` via `SprintCostEffectClass` GE with `SetByCaller` magnitude `-StaminaCostPerSecond * 0.1f` (`.cpp:238–244`).
- Pre-check on `Stamina` in `CanActivateAbility` (`.cpp:86–95`).
- Stamina polling: `CheckStaminaDepletion` timer at 0.1s (`.cpp:127–135`).
- Client-predicted (LocalPredicted) — both sides run; cost effect applies via server replication.

### E.2 `USuspenseCoreCharacterCrouchAbility`

- File: `SuspenseCoreCharacterCrouchAbility.h:34` / `.cpp:16`
- `NetExecutionPolicy = LocalPredicted` (`.cpp:27`)
- `bRetriggerInstancedAbility = true` for toggle (`.cpp:28`)
- Activation entry: `ActivateAbility()` at `.cpp:85`
- CMC state touched:
  - `CanActivateAbility` reads `CanEverCrouch()` and `IsFalling()` (`.cpp:70–82`)
  - No direct CMC write — calls `ISuspenseCoreMovementInterface::Execute_MovementStartCrouch(Avatar)` -> `Character::MovementStartCrouch_Implementation()` -> `Crouch()` (engine; `SuspenseCoreCharacter.cpp:388`)
  - Comment `.cpp:97–98`: "CMC handles speed automatically via MaxWalkSpeedCrouched"
- Attribute consumed: none directly. `CrouchDebuffEffectClass` is OPTIONAL (comments `.cpp:183–211`).
- Client-predicted.

### E.3 `USuspenseCoreCharacterJumpAbility`

- File: `SuspenseCoreCharacterJumpAbility.h:35` / `.cpp:18`
- `NetExecutionPolicy = LocalPredicted` (`.cpp:31`)
- Activation entry: `ActivateAbility()` at `.cpp:105`
- CMC state touched:
  - `CanActivateAbility` reads `IsFalling()` (`.cpp:80`)
  - `PerformJump` reads `JumpZVelocity`, **writes** `JumpZVelocity *= JumpPowerMultiplier` then immediately restores it (`.cpp:289–292`) — temp mutation around `Character->Jump()`. Comment `.cpp:288`: "Note: In production, use a gameplay effect for this."
  - `IsCharacterGrounded` reads `IsFalling()` (`.cpp:221`)
- Attribute consumed: `Stamina` via `JumpStaminaCostEffectClass` GE; `SetByCaller(SuspenseCoreTags::Data::Cost::Stamina, -StaminaCostPerJump)` (`.cpp:260–263`). Pre-check at `.cpp:88–99`.
- Calls `Character->Jump()` / `Character->StopJumping()` (`.cpp:291, 297, 176, 197`) — replicates via engine.
- Polling timer for landing every `GroundCheckInterval=0.1s` (`.cpp:131–138`), safety timeout `MaxJumpDuration=3.0s` (`.cpp:141–147`).
- Client-predicted.

---

## §F. EventBus traffic for Character

Per scope: tags matching `Player.*`, `Movement.*`, `Character.*`, `Camera.*`, `Ability.Character*`.

| file:line | Tag | Emitting side |
| --- | --- | --- |
| `SuspenseCoreCharacter.cpp:191` | `SuspenseCore.Event.Player.Spawned` | both (Character `BeginPlay`) |
| `SuspenseCoreCharacter.cpp:230` | `SuspenseCore.Event.Player.Possessed` | server-authoritative (`PossessedBy`) |
| `SuspenseCoreCharacter.cpp:238` | `SuspenseCore.Event.Player.UnPossessed` | server (`UnPossessed`) |
| `SuspenseCoreCharacter.cpp:315` | `SuspenseCore.Event.Player.SprintStarted` | client-predicted (`StartSprinting`, invoked from LocalPredicted ability) |
| `SuspenseCoreCharacter.cpp:329` | `SuspenseCore.Event.Player.SprintStopped` | client-predicted (`StopSprinting`) |
| `SuspenseCoreCharacter.cpp:471` | `SuspenseCore.Event.Player.WeaponStateChanged` | both (called via interface) |
| `SuspenseCoreCharacter.cpp:486` | `SuspenseCore.Event.Player.WeaponChanged` | both |
| `SuspenseCoreCharacter.cpp:519` | `SuspenseCore.Event.CharacterClass.Applied` | both (`ApplyCharacterClass`) |
| `SuspenseCoreCharacter.cpp:590` | `SuspenseCore.Event.Player.MovementStateChanged` | both (Tick) |
| `SuspenseCoreCharacter.cpp:1151` | `SuspenseCore.Event.Camera.FOVChanged` | client (SetCameraFOV; camera is local) |
| `SuspenseCoreCharacter.cpp:1169` | `SuspenseCore.Event.Camera.FocalLengthChanged` | client |
| `SuspenseCoreCharacter.cpp:1193` | `SuspenseCore.Event.Camera.ApertureChanged` | client |
| `SuspenseCoreCharacter.cpp:1217` | `SuspenseCore.Event.Camera.DOFChanged` | client |
| `SuspenseCoreCharacter.cpp:1232` | `SuspenseCore.Event.Camera.FocusDistanceChanged` | client |
| `SuspenseCoreCharacter.cpp:1255` | `SuspenseCore.Event.Camera.PresetApplied` | client |
| `SuspenseCoreCharacter.cpp:1293` | `SuspenseCore.Event.Camera.Reset` | client |
| `SuspenseCoreCharacter.cpp:1416` | `SuspenseCoreTags::Event::Camera::ADSEnter` (native tag) | client (ADS) |
| `SuspenseCoreCharacter.cpp:1463` | `SuspenseCoreTags::Event::Camera::ADSExit` (native tag) | client |
| `SuspenseCorePlayerState.cpp:185` | `SuspenseCore.Event.Player.Left` | both (EndPlay) |
| `SuspenseCorePlayerState.cpp:295` | `SuspenseCore.Event.Player.LevelChanged` | server (in `SetPlayerLevel`, gated by `HasAuthority`) |
| `SuspenseCorePlayerState.cpp:314` | `SuspenseCore.Event.Player.TeamChanged` | server |
| `SuspenseCorePlayerState.cpp:354` | `SuspenseCore.Event.Player.ClassChanged` | server (`ApplyCharacterClass`) |
| `SuspenseCorePlayerState.cpp:392` | `SuspenseCore.Event.Player.Respawned` | server (`RestoreVitals`) |
| `SuspenseCorePlayerState.cpp:467` | `SuspenseCore.Event.Player.LevelChanged` | client (`OnRep_PlayerLevel`) |
| `SuspenseCorePlayerState.cpp:479` | `SuspenseCore.Event.Player.TeamChanged` | client (`OnRep_TeamId`) |
| `SuspenseCorePlayerState.cpp:491` | `SuspenseCore.Event.Player.ClassChanged` | client (`OnRep_CharacterClassId`) |
| `SuspenseCorePlayerState.cpp:541` | `SuspenseCore.Event.Player.Initialized` | server (`InitializeAbilitySystem`) |
| `SuspenseCorePlayerComponent.cpp:148` | `SuspenseCore.Event.Player.IdentifierChanged` | both |
| `SuspenseCorePlayerComponent.cpp:210` | `SuspenseCore.Event.Player.ComponentReady` | both |
| `SuspenseCorePlayerController.cpp:237` (in `OnPossess`) | `SuspenseCoreTags::Event::Player::ControllerPossessed` | client (controller is local) |
| `SuspenseCorePlayerController.cpp:245` (`OnUnPossess`) | `SuspenseCoreTags::Event::Player::ControllerUnPossessed` | client |
| `SuspenseCorePlayerController.cpp:643` | `SuspenseCoreTags::Event::Input::AbilityActivated` | client (input handler) |
| `SuspenseCoreCharacterClassSubsystem.cpp:667` | `SuspenseCore.Event.Player.ClassChanged` | server |
| `SuspenseCoreAttributeSet.cpp:325` | `SuspenseCore.Event.Player.Died` | server (in `HandleDeath`) |
| `SuspenseCoreAttributeSet.cpp:393` | `SuspenseCore.Event.Player.LowHealth` | server (in `HandleLowHealth`) |
| `SuspenseCoreCharacterJumpAbility.cpp:353` | `SuspenseCoreTags::Event::Ability::CharacterJump::Landed` | both (LocalPredicted ability) |

No publishers found for tags `SuspenseCore.Event.Movement.*` or `SuspenseCore.Event.Character.*` outside what is listed; the `SuspenseCore.Event.Camera.Shake*` tags are SUBSCRIBED to by `CameraShakeComponent` (`SuspenseCoreCameraShakeComponent.cpp:92, 99, 106, 113`) and PUBLISHED by `SuspenseCoreBaseFireAbility.cpp:377` (`Camera::ShakeWeapon`, weapon fire), but no `ShakeMovement`/`ShakeDamage`/`ShakeExplosion` publisher in PlayerCore/GAS scope. Recoil-impulse publisher is `SuspenseCoreBaseFireAbility.cpp:885` (`Event::Weapon::RecoilImpulse`).

No `SuspenseCore.Event.Ability.CharacterSprint.Activated/Ended` or `CharacterCrouch.Activated/Ended` C++ publisher located — header comments at `SuspenseCoreCharacterSprintAbility.h:29–30` and `SuspenseCoreCharacterCrouchAbility.h:28–29` document them but only Jump's `Landed` is explicitly published in C++ (`SuspenseCoreCharacterJumpAbility.cpp:353`). Sprint/Crouch likely rely on base class `Super::ActivateAbility` activation event (`SuspenseCoreCharacterSprintAbility.cpp:122`; `SuspenseCoreCharacterCrouchAbility.cpp:109`) — base class `USuspenseCoreAbility` not deep-dived per scope.

---

## §G. Existing push-model patterns to mirror

Verified: `grep -rn "MARK_PROPERTY_DIRTY_FROM_NAME" Source/`.

Only 2 usages in entire repo:

| file:line | Setter idiom |
| --- | --- |
| `SuspenseCoreEquipmentReplicationManager.cpp:219` | `MARK_PROPERTY_DIRTY_FROM_NAME(USuspenseCoreEquipmentReplicationManager, CurrentVersion, this);` |
| `SuspenseCoreEquipmentReplicationManager.cpp:269` | `MARK_PROPERTY_DIRTY_FROM_NAME(USuspenseCoreEquipmentReplicationManager, CurrentVersion, this);` |

Both mark the same property `CurrentVersion` (in `EquipmentSystem`, OUT of scope per task). No setter wrapping idiom (guard / equality check / mark) is documented in PlayerCore or GAS — both call sites are bare in-function bumps with no `if (Old != New)` equality guard. No `DOREPLIFETIME_WITH_PARAMS_FAST` usages in repo.

No push-model patterns exist in PlayerCore or GAS to mirror.

---

## §H. Documented antipatterns / past pain

### H.1 Documentation hits

`Documentation/SuspenseCoreDeveloperGuide.md` is the only file containing pitfall-style guidance. Relevant quotes:

- `Documentation/SuspenseCoreDeveloperGuide.md:496` "## Common Pitfalls & Solutions"
- `Documentation/SuspenseCoreDeveloperGuide.md:498` "### EventBus Subscription Errors"
- `Documentation/SuspenseCoreDeveloperGuide.md:511–515` "Wrong: ... ERROR: FSuspenseCoreEventCallback is a DYNAMIC_DELEGATE - no CreateUObject"
- `Documentation/SuspenseCoreDeveloperGuide.md:528–543` "### EventData Type Casting ... `FSuspenseCoreEventData::Source` is `TObjectPtr<UObject>`, not `AActor*`."
- `Documentation/SuspenseCoreDeveloperGuide.md:547–569` "### Cross-Module Include Errors ... Rules: 1. GAS module should NOT include EquipmentSystem headers directly. 2. Use EventBus for cross-module communication."
- `Documentation/SuspenseCoreDeveloperGuide.md:573–585` "### GameplayTag Deprecation Warnings ... C4996 StackingType deprecated"
- `Documentation/SuspenseCoreDeveloperGuide.md:589–603` "### Modern UE5 GAS Components ... InheritableOwnedTagsContainer deprecated ... Solution: Use UTargetTagsGameplayEffectComponent"
- `Documentation/SuspenseCoreDeveloperGuide.md:607–618` "### Native Tags vs RequestGameplayTag ... Wrong - runtime string lookup, prone to typos ... Correct - compile-time checked, cached"

None of the documented pitfalls explicitly target Character/CMC/replication/movement — they cover EventBus, modules, GAS Effects deprecations, and native tag usage.

### H.2 Git history flags

`git log --oneline -- Source/PlayerCore Source/GAS` returns 17 total commits (full history for these paths). Notable fix/revert commits:

| Hash | Subject |
| --- | --- |
| `24f2b7b` | `fix(GAS): Save CachedPreChangeStamina before SetNumericAttributeBase` |
| `f5c642e` | `fix(GAS): Restore SetNumericAttributeBase to prevent stamina overflow` |
| `493f9b0` | `fix(GAS): Fix stamina progress bar jumping at max value` |
| `b9cc0d1` | `feat(GAS): Implement character death system` |
| `701b231` | `fix(UI): Fix DoT stack display not clearing on cure` |
| `fbdf5bb` | `fix(Medical): Fix item consumption and stance tag extraction` |
| `08aec1e` | `fix(GAS/Equipment): Fix duplicate effects, Fire spam, and weapon hiding` |
| `b1fba2c` | `fix(GAS/Equipment): Fix native tag usage and GA_MedicalUse lookup` |
| `b5100f3` | `fix(GAS): Fix medical/grenade/weapon Fire input conflict and healing order` |
| `d20fb9e` | `fix(GAS): Use correct SuspenseCore prefix for Medical ability tags` |
| `8996fe5` | `fix(GAS): Remove unused FSuspenseCoreItemUseRequest in GA_MedicalUse` |
| `428479c` | `fix(GAS): Resolve compilation errors in medical ability system` |
| `79987bc` | `feat(Input): Route Fire input to GA_MedicalUse when medical item equipped` |
| `fa4da03` | `feat(GAS): Add animation-driven medical abilities (Tarkov-style flow)` |
| `46b10ea` | `fix(GAS): Suppress StackingType deprecation warnings in GE_BleedingEffect` |
| `f2ad698` | `refactor(GAS): Replace RequestGameplayTag with native SuspenseCoreTags` |
| `bd21e04` | merge commit |

Pattern observed: 3 sequential fixes around Stamina + `SetNumericAttributeBase` (`493f9b0`, `f5c642e`, `24f2b7b`) — see `SuspenseCoreAttributeSet.cpp:139–164` for the resulting code with `CachedPreChangeStamina` workaround. No reverts.

---

## §I. Validation surface

- Spec/automation tests in PlayerCore or GAS: **none found**. `find ... -name "*.spec.cpp" -o -name "*Spec*.cpp" -o -name "*Test*.cpp"` returns no hits in `Source/PlayerCore` or `Source/GAS` (or repo-wide).
- Smoke-test maps in `Content/`: **none**. `find Content -name "*Test*" -o -name "*Smoke*"` returns nothing. `Content/` contains only `Data/StatusEffects`, `Data/ItemDatabase`, `Data/EnemyDatabase` (DataTables, no maps).
- No automated coverage found.

---

## §J. Open questions for the user

1. **Push-model adoption boundary.** Only 2 `MARK_PROPERTY_DIRTY_FROM_NAME` usages exist, both on `EquipmentReplicationManager.CurrentVersion` (`SuspenseCoreEquipmentReplicationManager.cpp:219, 269`). For the Iris migration skill: is the goal to migrate all 6 AttributeSets + PlayerState to push-model, or just Character/PlayerState core state? Confirm scope.

2. **`EquipmentUIProvider` replication discrepancy.** `SuspenseCorePlayerState.h:312` declares it `UPROPERTY(VisibleAnywhere, Replicated)` but `SuspenseCorePlayerState.cpp:192–215` does NOT call `DOREPLIFETIME` for it. Same for `SystemCoordinator` (`.h:316`, declared without `Replicated`). Is this intentional (UI provider is local-only) or a bug? Affects skill's "current state" baseline.

3. **`SystemCoordinator` non-replication.** Declared `UPROPERTY(VisibleAnywhere)` without `Replicated` (`SuspenseCorePlayerState.h:316`). Is this intentional (server-only orchestrator)? Should it be flagged in the migration plan?

4. **Temp `JumpZVelocity` mutation in jump ability.** `SuspenseCoreCharacterJumpAbility.cpp:289–292` mutates and restores `CMC->JumpZVelocity` around `Character->Jump()` with comment "Note: In production, use a gameplay effect for this." Is this a known tech-debt item to address in the skill, or leave as-is?

5. **`Character::CameraShake` writer.** `SuspenseCoreCharacter.h:585` declares `CameraShake` as `UPROPERTY(BlueprintReadWrite)` with comment "Устанавливается из Blueprint" — no C++ writer found. Is the AnimBP contract assuming Blueprint will set this each tick, or is there an intended C++ writer that is missing?

6. **`MoveForwardValue` / `MoveRightValue` lacking `UPROPERTY`.** `SuspenseCoreCharacter.h:575–578` declares these as plain `float` fields, accessed via `BlueprintCallable` getters (`.h:189, 192`). Is the AnimBP reading via getters only, or does any subsystem need direct UPROPERTY-style reflection access (e.g. binding visualization)?

7. **5 bools shared `OnRep_CombatState` on `USuspenseCoreWeaponStanceComponent`.** Five distinct bools (`bIsAiming`, `bIsFiring`, `bIsReloading`, `bIsHoldingBreath`, `bIsWeaponBlocked`) share one rep callback (`SuspenseCoreWeaponStanceComponent.cpp:55–59`). Confirm this is intentional batching versus a per-property migration target.

8. **Sprint / Crouch ability event publication.** Headers document `SuspenseCore.Event.Ability.CharacterSprint.Activated/Ended` and `CharacterCrouch.Activated/Ended` (`SuspenseCoreCharacterSprintAbility.h:29–30`, `SuspenseCoreCharacterCrouchAbility.h:28–29`), but no explicit C++ `EventBus->Publish(...)` for these tags is in scope. They are presumably emitted by `USuspenseCoreAbility::ActivateAbility/EndAbility` base class (out of scope). Should §F be augmented after reading base class, or is the existing scope sufficient?

9. **No CMC subclass / no Mover.** Verified zero overrides of `UpdateFromCompressedFlags`/`GetCompressedFlags`/`OnMovementModeChanged` in any module. Confirm assumption that future skill output should NOT introduce a custom CMC subclass.

10. **Validation coverage gap.** No Spec tests, no automated test maps. For skill "Done when" criteria: should new code require manual PIE smoke only, or is the user planning to introduce automation in the same change?
