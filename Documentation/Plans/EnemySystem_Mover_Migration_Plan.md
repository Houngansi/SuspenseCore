# EnemySystem Mover Migration Plan

## Overview

Plan for migrating EnemySystem from CharacterMovementComponent (CMC) to Mover Plugin for UE 5.7+.

**Status:** Planning Phase
**Target UE Version:** 5.8+ (when Mover AI integration is stable)
**Risk Level:** High (experimental plugin)

---

## Current Architecture (CMC)

```
ASuspenseCoreEnemyCharacter : ACharacter
    ├── UCharacterMovementComponent (inherited)
    ├── UAbilitySystemComponent
    ├── USuspenseCoreEnemyFSMComponent
    ├── USuspenseCoreEnemyAnimInstance
    └── UCapsuleComponent
```

### Key Dependencies on CMC
- `MaxWalkSpeed` - used in FSM states (Patrol, Chase)
- `bOrientRotationToMovement` - critical for walk animation
- `SetMovementMode(MOVE_Walking/NavWalking)` - movement activation
- `RotationRate` - turn speed
- `GetVelocity()` - AnimInstance reads this

---

## Target Architecture (Mover)

```
ASuspenseCoreEnemyMoverActor : AActor (or APawn)
    ├── UMoverComponent
    │   ├── Walking Movement Mode
    │   ├── Chase Movement Mode (custom)
    │   └── NavMover integration
    ├── UAbilitySystemComponent
    ├── USuspenseCoreEnemyFSMComponent
    ├── USuspenseCoreEnemyMoverAnimInstance
    └── USkeletalMeshComponent
```

---

## Key Architectural Differences

| Aspect | CMC | Mover |
|--------|-----|-------|
| Actor Base | ACharacter only | Any Actor/Pawn |
| State Access | Direct (`Velocity`, `MaxWalkSpeed`) | Via Modes/Effects only |
| Networking | Client sends state+inputs | Shared timeline, inputs only |
| Modularity | Monolithic | Movement Modes as objects |
| Root | Capsule required | Any SceneComponent |
| Orientation | Z always up | Arbitrary |

---

## Migration Phases

### Phase 1: Abstraction Layer (PRE-MIGRATION)

Create interface to abstract movement operations:

```cpp
// IMoverCharacterInterface.h
UINTERFACE(MinimalAPI)
class UMoverCharacterInterface : public UInterface
{
    GENERATED_BODY()
};

class IMoverCharacterInterface
{
    GENERATED_BODY()

public:
    virtual void SetMovementSpeed(float Speed) = 0;
    virtual float GetMovementSpeed() const = 0;
    virtual void SetOrientToMovement(bool bOrient) = 0;
    virtual FVector GetCurrentVelocity() const = 0;
    virtual void StartMoveTo(const FVector& Location) = 0;
    virtual void StopMovement() = 0;
};
```

**Files to modify:**
- `SuspenseCoreEnemyCharacter.h/cpp` - implement interface
- `SuspenseCoreEnemyPatrolState.cpp` - use interface
- `SuspenseCoreEnemyChaseState.cpp` - use interface
- `SuspenseCoreEnemyAttackState.cpp` - use interface
- `SuspenseCoreEnemyAnimInstance.cpp` - get velocity via interface

### Phase 2: Create Mover Character (PARALLEL)

Create new actor class using Mover:

```cpp
// SuspenseCoreEnemyMoverCharacter.h
UCLASS()
class ASuspenseCoreEnemyMoverCharacter : public APawn,
                                         public IMoverCharacterInterface
{
    UPROPERTY()
    UMoverComponent* MoverComponent;

    UPROPERTY()
    UAbilitySystemComponent* AbilitySystemComponent;

    UPROPERTY()
    USuspenseCoreEnemyFSMComponent* FSMComponent;
};
```

**New files:**
- `SuspenseCoreEnemyMoverCharacter.h/cpp`
- `SuspenseCoreEnemyMoverAnimInstance.h/cpp`
- `SuspenseCoreEnemyWalkingMode.h/cpp` (custom movement mode)
- `SuspenseCoreEnemyChaseMode.h/cpp` (custom movement mode)

### Phase 3: NavMover Integration

Integrate with NavMover for AI pathfinding:

```cpp
// NavMover usage (experimental API as of UE 5.7)
UPROPERTY()
UNavMoverComponent* NavMoverComponent;

void MoveToLocation(const FVector& Target)
{
    // NavMover handles pathfinding internally
    NavMoverComponent->RequestMoveTo(Target);
}
```

**Considerations:**
- NavMover API may change between UE versions
- Test with complex NavMesh geometries
- Verify performance vs CMC

### Phase 4: AnimInstance Adaptation

Create Mover-compatible AnimInstance:

```cpp
void USuspenseCoreEnemyMoverAnimInstance::UpdateVelocityData(float DeltaSeconds)
{
    if (IMoverCharacterInterface* MoverChar = GetMoverCharacter())
    {
        // Get velocity from Mover instead of CMC
        const FVector Velocity = MoverChar->GetCurrentVelocity();
        GroundSpeed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();
        // ... rest unchanged
    }
}
```

### Phase 5: Testing & Validation

**Test cases:**
- [ ] Basic movement (walk, run)
- [ ] NavMesh pathfinding
- [ ] Obstacle avoidance
- [ ] Animation blending
- [ ] State transitions (Idle → Patrol → Chase → Attack)
- [ ] Multiplayer replication
- [ ] Performance comparison vs CMC

### Phase 6: Feature Flag Toggle

Add runtime toggle between CMC and Mover:

```cpp
// In Project Settings or Data Asset
UPROPERTY()
bool bUseMoverPlugin = false;

// Factory method
ASuspenseCoreEnemyBase* SpawnEnemy(...)
{
    if (bUseMoverPlugin)
        return SpawnMoverEnemy(...);
    else
        return SpawnCMCEnemy(...);
}
```

---

## Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Mover API changes | High | High | Abstract via interface |
| NavMover bugs | Medium | High | Keep CMC fallback |
| Animation issues | Medium | Medium | Test extensively |
| Performance regression | Low | Medium | Profile both systems |
| Network desyncs | Medium | High | Test multiplayer early |

---

## Timeline Recommendation

| Phase | Estimated Effort | Dependencies |
|-------|------------------|--------------|
| Phase 1 | 2-3 days | None |
| Phase 2 | 1 week | Phase 1 |
| Phase 3 | 1 week | Phase 2, NavMover docs |
| Phase 4 | 2-3 days | Phase 2 |
| Phase 5 | 1 week | Phases 1-4 |
| Phase 6 | 1 day | All phases |

**Total:** ~4 weeks

---

## Wait Conditions

Do NOT start migration until:
- [ ] UE 5.8 releases with stable Mover AI integration
- [ ] NavMover has official documentation
- [ ] Community has validated Mover for AI use cases
- [ ] Epic confirms Mover as CMC successor (not just experiment)

---

## References

- [Mover Features and Concepts](https://dev.epicgames.com/documentation/en-us/unreal-engine/mover-features-and-concepts-in-unreal-engine)
- [Comparing Mover and CMC](https://dev.epicgames.com/documentation/en-us/unreal-engine/comparing-mover-and-character-movement-component-in-unreal-engine)
- [GASP Updates in UE 5.7](https://80.lv/articles/game-animation-sample-project-updated-for-unreal-engine-5-7)
- [Mover Deep Dive (Community)](https://vorixo.github.io/devtricks/mover-show/)

---

## Appendix: Quick Reference

### CMC → Mover Mapping

| CMC Property/Method | Mover Equivalent |
|---------------------|------------------|
| `MaxWalkSpeed` | Movement Mode parameter |
| `Velocity` | Query from MoverComponent |
| `SetMovementMode()` | Switch active Movement Mode |
| `bOrientRotationToMovement` | Rotation in Movement Mode |
| `MoveToLocation()` | NavMover request or Layered Move |
| `StopMovement()` | Instant Effect or Mode transition |

---

*Document created: 2026-01-30*
*Last updated: 2026-01-30*
