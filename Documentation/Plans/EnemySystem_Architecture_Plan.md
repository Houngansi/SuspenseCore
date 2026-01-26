# EnemySystem Architecture Plan

> **Version:** 1.0
> **Author:** Claude (Technical Lead)
> **Date:** 2026-01-26
> **Target:** Unreal Engine 5.7 | AAA MMO FPS (Tarkov-Style)
> **Status:** PLANNING

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Architecture Overview](#architecture-overview)
3. [Module Structure](#module-structure)
4. [FSM State Machine Design](#fsm-state-machine-design)
5. [StateTree Integration (UE 5.7)](#statetree-integration-ue-57)
6. [Class Hierarchy](#class-hierarchy)
7. [SSOT Data Architecture](#ssot-data-architecture)
8. [GAS Integration](#gas-integration)
9. [EventBus Communication](#eventbus-communication)
10. [Perception System](#perception-system)
11. [Implementation Phases](#implementation-phases)
12. [File Structure](#file-structure)
13. [Code Templates](#code-templates)

---

## Executive Summary

### Objectives

Создать современную AI систему для врагов (ботов), полностью интегрированную с архитектурой SuspenseCore:

- **FSM State Machine** - состояния AI через GameplayTags
- **StateTree (UE 5.7)** - визуальный редактор поведения (замена BehaviorTree)
- **SSOT** - все данные врагов через DataAssets/DataManager
- **GAS** - AbilitySystemComponent на EnemyState
- **EventBus** - коммуникация через GameplayTag события

### Key Features (Tarkov-Style AI)

| Feature | Description |
|---------|-------------|
| **Tactical AI** | Укрытия, фланги, подавление |
| **Sound Awareness** | Реакция на выстрелы, шаги, голос |
| **Group Behavior** | Координация squad, callouts |
| **Threat Assessment** | Приоритизация целей по угрозе |
| **Scav/PMC Types** | Разные типы AI с различным поведением |
| **Boss AI** | Продвинутые паттерны для боссов |

---

## Architecture Overview

### System Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         SUSPENSECORE ENEMYSYSTEM                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    USuspenseCoreEnemySubsystem                       │    │
│  │  (GameInstanceSubsystem - Global enemy management)                   │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                    │                                         │
│                    ┌───────────────┼───────────────┐                        │
│                    ▼               ▼               ▼                        │
│  ┌─────────────────────┐ ┌─────────────────┐ ┌─────────────────────┐       │
│  │ USuspenseCore       │ │ USuspenseCore   │ │ USuspenseCore       │       │
│  │ EnemySpawnService   │ │ EnemyAIService  │ │ EnemyDataManager    │       │
│  │ (Spawn/Despawn)     │ │ (AI Logic)      │ │ (SSOT)              │       │
│  └─────────────────────┘ └─────────────────┘ └─────────────────────┘       │
│                                    │                                         │
│  ═══════════════════════════════════════════════════════════════════════    │
│                              EventBus Layer                                  │
│  ═══════════════════════════════════════════════════════════════════════    │
│                                    │                                         │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    ASuspenseCoreEnemy (Character)                    │    │
│  │  ┌─────────────────────────────────────────────────────────────┐    │    │
│  │  │ Components:                                                   │    │    │
│  │  │  • USuspenseCoreEnemyAIComponent (FSM Controller)            │    │    │
│  │  │  • USuspenseCoreEnemyPerceptionComponent (Sight/Sound)       │    │    │
│  │  │  • USuspenseCoreEnemyBehaviorComponent (StateTree Interface) │    │    │
│  │  │  • USuspenseCoreEnemySquadComponent (Group Behavior)         │    │    │
│  │  └─────────────────────────────────────────────────────────────┘    │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                    │                                         │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │               ASuspenseCoreEnemyAIController (AIController)          │    │
│  │  • StateTree Execution                                               │    │
│  │  • Blackboard Management                                             │    │
│  │  • Navigation/Movement                                               │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                    │                                         │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │               ASuspenseCoreEnemyState (PlayerState analog)           │    │
│  │  • UAbilitySystemComponent (GAS)                                     │    │
│  │  • USuspenseCoreEnemyAttributeSet                                    │    │
│  │  • Startup Abilities                                                 │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Module Dependencies

```
BridgeSystem (base)
    ↑
    ├── GAS
    │     ↑
    │     └── EnemySystem (NEW)
    │           ↑
    │           └── (Optional: LevelStreaming for spawn zones)
```

---

## Module Structure

### EnemySystem Module

```cpp
// EnemySystem.Build.cs
public class EnemySystem : ModuleRules
{
    public EnemySystem(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",

                // AI Modules (UE 5.7)
                "AIModule",
                "GameplayTasks",
                "NavigationSystem",
                "StateTreeModule",        // NEW: StateTree (UE 5.4+)
                "GameplayStateTreeModule", // NEW: Gameplay-specific StateTree

                // GAS
                "GameplayAbilities",
                "GameplayTags",

                // SuspenseCore
                "BridgeSystem",
                "GAS"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore",
                "UMG"
            }
        );
    }
}
```

---

## FSM State Machine Design

### State Definition via GameplayTags

```
State.Enemy.Idle          - Патруль/ожидание
State.Enemy.Alert         - Подозрение (слышал звук)
State.Enemy.Investigate   - Проверка подозрительного места
State.Enemy.Combat        - Активный бой
State.Enemy.Chase         - Преследование цели
State.Enemy.Cover         - В укрытии
State.Enemy.Flank         - Обход противника
State.Enemy.Retreat       - Отступление
State.Enemy.Reload        - Перезарядка
State.Enemy.Heal          - Самолечение
State.Enemy.Dead          - Мёртв
State.Enemy.Stunned       - Оглушён
State.Enemy.Suppressed    - Под подавлением
```

### State Transition Diagram

```
                              ┌────────────────┐
                              │   State.Idle   │◄─────────────────┐
                              └───────┬────────┘                  │
                                      │ Perception Event          │ No Threat
                                      ▼                           │
                              ┌────────────────┐                  │
                         ┌────│  State.Alert   │────┐             │
                         │    └───────┬────────┘    │             │
              No confirm │            │ Confirmed   │ Timeout     │
                         │            ▼             │             │
                         │    ┌────────────────┐    │             │
                         └───►│State.Investigate│───┴─────────────┤
                              └───────┬────────┘                  │
                                      │ Target Acquired           │
                                      ▼                           │
┌─────────────┐           ┌────────────────────────┐              │
│State.Retreat│◄──────────│     State.Combat       │──────────────┤
└──────┬──────┘ Low HP    │  ┌──────────────────┐  │ Target Lost  │
       │                  │  │ Sub-states:      │  │              │
       │                  │  │ • Cover          │  │              │
       │                  │  │ • Flank          │  │              │
       │                  │  │ • Suppress       │  │              │
       │                  │  │ • Reload         │  │              │
       │                  │  └──────────────────┘  │              │
       │                  └───────────┬────────────┘              │
       │                              │ HP = 0                    │
       │                              ▼                           │
       │                      ┌────────────────┐                  │
       └─────────────────────►│   State.Dead   │──────────────────┘
                              └────────────────┘
```

### FSM Implementation Pattern

```cpp
// USuspenseCoreEnemyAIComponent.h
UCLASS(ClassGroup = "SuspenseCore", meta = (BlueprintSpawnableComponent))
class ENEMYSYSTEM_API USuspenseCoreEnemyAIComponent : public UActorComponent,
    public ISuspenseCoreEventSubscriber
{
    GENERATED_BODY()

public:
    USuspenseCoreEnemyAIComponent();

    // ═══════════════════════════════════════════════════════════════════
    // FSM STATE MANAGEMENT
    // ═══════════════════════════════════════════════════════════════════

    /** Get current AI state */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Enemy|FSM")
    FGameplayTag GetCurrentState() const { return CurrentState; }

    /** Request state transition */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Enemy|FSM")
    bool RequestStateChange(const FGameplayTag& NewState);

    /** Force state (bypasses validation) */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Enemy|FSM")
    void ForceState(const FGameplayTag& NewState);

    /** Check if transition is valid */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Enemy|FSM")
    bool CanTransitionTo(const FGameplayTag& TargetState) const;

    // ═══════════════════════════════════════════════════════════════════
    // STATE CALLBACKS (override in subclasses)
    // ═══════════════════════════════════════════════════════════════════

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
        FOnStateChanged,
        FGameplayTag, OldState,
        FGameplayTag, NewState
    );

    UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Enemy|FSM")
    FOnStateChanged OnStateChanged;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    // ═══════════════════════════════════════════════════════════════════
    // STATE HANDLERS (implement state-specific logic)
    // ═══════════════════════════════════════════════════════════════════

    virtual void EnterState(const FGameplayTag& NewState);
    virtual void ExitState(const FGameplayTag& OldState);
    virtual void TickState(const FGameplayTag& CurrentState, float DeltaTime);

    // State-specific enter handlers
    virtual void EnterIdleState();
    virtual void EnterAlertState();
    virtual void EnterInvestigateState();
    virtual void EnterCombatState();
    virtual void EnterChaseState();
    virtual void EnterCoverState();
    virtual void EnterRetreatState();
    virtual void EnterDeadState();

    // State-specific tick handlers
    virtual void TickIdleState(float DeltaTime);
    virtual void TickAlertState(float DeltaTime);
    virtual void TickCombatState(float DeltaTime);

private:
    UPROPERTY()
    FGameplayTag CurrentState;

    UPROPERTY()
    FGameplayTag PreviousState;

    /** State transition rules map */
    TMap<FGameplayTag, TArray<FGameplayTag>> ValidTransitions;

    void InitializeTransitionRules();
    void BroadcastStateChange(const FGameplayTag& OldState, const FGameplayTag& NewState);
};
```

---

## StateTree Integration (UE 5.7)

### Why StateTree over BehaviorTree?

| Feature | BehaviorTree | StateTree (UE 5.4+) |
|---------|--------------|---------------------|
| **Debugging** | Limited | Rich visual debugging |
| **States** | Implicit | First-class citizens |
| **Transitions** | Complex | Simple, visual |
| **Performance** | Good | Excellent (optimized) |
| **UE 5.7 Support** | Legacy | Native, recommended |
| **Mass AI** | Limited | Full MassEntity support |

### StateTree Asset Structure

```
Content/EnemySystem/AI/
├── StateTree/
│   ├── ST_Enemy_Base.uasset           # Base enemy behavior
│   ├── ST_Enemy_Scav.uasset           # Scav-specific (cautious)
│   ├── ST_Enemy_PMC.uasset            # PMC-specific (tactical)
│   ├── ST_Enemy_Boss.uasset           # Boss-specific (aggressive)
│   └── ST_Enemy_Sniper.uasset         # Sniper-specific (static)
├── Tasks/
│   ├── STTask_FindCover.cpp           # Find nearest cover
│   ├── STTask_MoveToCover.cpp         # Navigate to cover
│   ├── STTask_Attack.cpp              # Execute attack ability
│   ├── STTask_Investigate.cpp         # Check suspicious location
│   └── STTask_CallReinforcements.cpp  # Alert squad members
├── Conditions/
│   ├── STCondition_CanSeeTarget.cpp   # Line of sight check
│   ├── STCondition_InCombatRange.cpp  # Distance check
│   ├── STCondition_LowHealth.cpp      # Health threshold
│   └── STCondition_HasAmmo.cpp        # Ammo check
└── Evaluators/
    ├── STEval_ThreatLevel.cpp         # Calculate threat
    └── STEval_BestCover.cpp           # Evaluate cover quality
```

### StateTree Task Example

```cpp
// STTask_SuspenseCoreAttack.h
USTRUCT()
struct ENEMYSYSTEM_API FSTTask_SuspenseCoreAttack : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FSTTask_SuspenseCoreAttackInstanceData;

    // Task parameters (configured in editor)
    UPROPERTY(EditAnywhere, Category = "Attack")
    FGameplayTag AttackAbilityTag = SuspenseCoreEnemyTags::Ability::Enemy::Attack;

    UPROPERTY(EditAnywhere, Category = "Attack")
    float MinAttackInterval = 0.5f;

protected:
    virtual EStateTreeRunStatus EnterState(
        FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) const override;

    virtual EStateTreeRunStatus Tick(
        FStateTreeExecutionContext& Context,
        const float DeltaTime) const override;

    virtual void ExitState(
        FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) const override;
};

// Instance data (per-enemy runtime state)
USTRUCT()
struct FSTTask_SuspenseCoreAttackInstanceData
{
    GENERATED_BODY()

    UPROPERTY()
    float TimeSinceLastAttack = 0.0f;

    UPROPERTY()
    TWeakObjectPtr<AActor> CurrentTarget;
};
```

---

## Class Hierarchy

### Enemy Character

```cpp
// ASuspenseCoreEnemy.h
UCLASS()
class ENEMYSYSTEM_API ASuspenseCoreEnemy : public ACharacter,
    public IAbilitySystemInterface,
    public ISuspenseCoreCharacter,
    public ISuspenseCoreEnemy
{
    GENERATED_BODY()

public:
    ASuspenseCoreEnemy();

    // ═══════════════════════════════════════════════════════════════════
    // IAbilitySystemInterface
    // ═══════════════════════════════════════════════════════════════════

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    // ═══════════════════════════════════════════════════════════════════
    // ISuspenseCoreCharacter
    // ═══════════════════════════════════════════════════════════════════

    virtual UAbilitySystemComponent* GetASC() const override;
    virtual float GetCharacterLevel() const override;
    virtual bool IsAlive() const override;
    virtual int32 GetTeamId() const override { return 1; } // Enemies = Team 1

    // ═══════════════════════════════════════════════════════════════════
    // ISuspenseCoreEnemy
    // ═══════════════════════════════════════════════════════════════════

    virtual void SetCurrentWeapon_Implementation(AActor* WeaponActor) override;
    virtual AActor* GetCurrentWeapon_Implementation() const override;
    virtual FGameplayTag GetAIState_Implementation() const override;
    virtual void SetAIState_Implementation(const FGameplayTag& NewState) override;
    virtual bool CanAttack_Implementation() const override;
    virtual float GetPreferredCombatRange_Implementation() const override;

    // ═══════════════════════════════════════════════════════════════════
    // PUBLIC API
    // ═══════════════════════════════════════════════════════════════════

    /** Initialize from DataAsset */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Enemy")
    void InitializeFromData(const USuspenseCoreEnemyClassData* EnemyData);

    /** Handle death */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Enemy")
    void HandleDeath(AActor* DamageInstigator, AActor* DamageCauser);

    /** Get enemy class ID */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Enemy")
    FName GetEnemyClassID() const { return EnemyClassID; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void PossessedBy(AController* NewController) override;

    // ═══════════════════════════════════════════════════════════════════
    // COMPONENTS
    // ═══════════════════════════════════════════════════════════════════

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USuspenseCoreEnemyAIComponent> AIComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USuspenseCoreEnemyPerceptionComponent> PerceptionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USuspenseCoreEnemyBehaviorComponent> BehaviorComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USuspenseCoreEnemySquadComponent> SquadComponent;

    // ═══════════════════════════════════════════════════════════════════
    // STATE
    // ═══════════════════════════════════════════════════════════════════

    UPROPERTY(BlueprintReadOnly, Category = "Enemy")
    FName EnemyClassID;

    UPROPERTY()
    TObjectPtr<ASuspenseCoreEnemyState> EnemyState;

    UPROPERTY()
    TObjectPtr<AActor> CurrentWeapon;

    bool bIsDead = false;

private:
    void SetupAbilitySystem();
    void GiveStartupAbilities();
};
```

### Enemy AI Controller

```cpp
// ASuspenseCoreEnemyAIController.h
UCLASS()
class ENEMYSYSTEM_API ASuspenseCoreEnemyAIController : public AAIController,
    public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ASuspenseCoreEnemyAIController();

    // ═══════════════════════════════════════════════════════════════════
    // IAbilitySystemInterface
    // ═══════════════════════════════════════════════════════════════════

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    // ═══════════════════════════════════════════════════════════════════
    // STATETREE API
    // ═══════════════════════════════════════════════════════════════════

    /** Start StateTree execution */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Enemy|AI")
    void StartStateTree(UStateTree* StateTreeAsset);

    /** Stop StateTree */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Enemy|AI")
    void StopStateTree();

    /** Get controlled enemy */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Enemy|AI")
    ASuspenseCoreEnemy* GetControlledEnemy() const;

    // ═══════════════════════════════════════════════════════════════════
    // BLACKBOARD API
    // ═══════════════════════════════════════════════════════════════════

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Enemy|AI")
    void SetTargetActor(AActor* Target);

    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Enemy|AI")
    AActor* GetTargetActor() const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Enemy|AI")
    void SetLastKnownLocation(const FVector& Location);

protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void BeginPlay() override;

    // ═══════════════════════════════════════════════════════════════════
    // COMPONENTS
    // ═══════════════════════════════════════════════════════════════════

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStateTreeComponent> StateTreeComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UAIPerceptionComponent> AIPerceptionComponent;

    // ═══════════════════════════════════════════════════════════════════
    // PERCEPTION HANDLERS
    // ═══════════════════════════════════════════════════════════════════

    UFUNCTION()
    void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

private:
    UPROPERTY()
    TWeakObjectPtr<ASuspenseCoreEnemy> ControlledEnemy;

    // Blackboard keys
    static const FName BB_TargetActor;
    static const FName BB_LastKnownLocation;
    static const FName BB_CurrentState;
    static const FName BB_ThreatLevel;
};
```

---

## SSOT Data Architecture

### Enemy Class Data Asset

```cpp
// USuspenseCoreEnemyClassData.h
UCLASS(BlueprintType)
class ENEMYSYSTEM_API USuspenseCoreEnemyClassData : public UDataAsset
{
    GENERATED_BODY()

public:
    // ═══════════════════════════════════════════════════════════════════
    // IDENTITY
    // ═══════════════════════════════════════════════════════════════════

    /** Unique class identifier */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FName ClassID;

    /** Display name (localized) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FText DisplayName;

    /** Enemy type tag (Scav, PMC, Boss, etc.) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FGameplayTag EnemyTypeTag;

    // ═══════════════════════════════════════════════════════════════════
    // COMBAT STATS
    // ═══════════════════════════════════════════════════════════════════

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    float MaxHealth = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    float MaxStamina = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    float Armor = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    float AttackPower = 25.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    float AccuracyModifier = 1.0f;

    // ═══════════════════════════════════════════════════════════════════
    // MOVEMENT
    // ═══════════════════════════════════════════════════════════════════

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
    float WalkSpeed = 200.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
    float RunSpeed = 500.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
    float SprintSpeed = 700.0f;

    // ═══════════════════════════════════════════════════════════════════
    // AI BEHAVIOR
    // ═══════════════════════════════════════════════════════════════════

    /** StateTree asset for AI behavior */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
    TSoftObjectPtr<UStateTree> StateTreeAsset;

    /** Perception radius (sight) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Perception")
    float SightRadius = 3000.0f;

    /** Peripheral vision angle (half-angle in degrees) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Perception")
    float PeripheralVisionAngle = 90.0f;

    /** Hearing range */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Perception")
    float HearingRange = 2000.0f;

    /** Preferred combat distance */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat")
    float PreferredCombatRange = 1500.0f;

    /** Aggression level (0-1, affects behavior) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat", meta = (ClampMin = 0.0, ClampMax = 1.0))
    float AggressionLevel = 0.5f;

    /** Health threshold for retreat (0-1) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat", meta = (ClampMin = 0.0, ClampMax = 1.0))
    float RetreatHealthThreshold = 0.25f;

    // ═══════════════════════════════════════════════════════════════════
    // GAS ABILITIES
    // ═══════════════════════════════════════════════════════════════════

    /** Startup abilities granted on spawn */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
    TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

    /** Default gameplay effects applied on spawn */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
    TArray<TSubclassOf<UGameplayEffect>> StartupEffects;

    // ═══════════════════════════════════════════════════════════════════
    // VISUAL
    // ═══════════════════════════════════════════════════════════════════

    /** Skeletal mesh */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
    TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

    /** Animation Blueprint */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
    TSubclassOf<UAnimInstance> AnimBlueprintClass;

    // ═══════════════════════════════════════════════════════════════════
    // LOOT
    // ═══════════════════════════════════════════════════════════════════

    /** Loot table ID */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
    FName LootTableID;

    /** Experience reward */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
    int32 ExperienceReward = 100;
};
```

### DataTable Row Structure

```cpp
// FSuspenseCoreEnemyClassRow.h
USTRUCT(BlueprintType)
struct ENEMYSYSTEM_API FSuspenseCoreEnemyClassRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ClassID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag EnemyTypeTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float MaxHealth = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float MaxStamina = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float Armor = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float AttackPower = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float SightRadius = 3000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float HearingRange = 2000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float AggressionLevel = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UStateTree> StateTreeAsset;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName LootTableID;
};
```

---

## GAS Integration

### Enemy Attribute Set

```cpp
// USuspenseCoreEnemyAttributeSet.h
UCLASS()
class ENEMYSYSTEM_API USuspenseCoreEnemyAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    USuspenseCoreEnemyAttributeSet();

    // ═══════════════════════════════════════════════════════════════════
    // HEALTH ATTRIBUTES
    // ═══════════════════════════════════════════════════════════════════

    UPROPERTY(BlueprintReadOnly, Category = "Health", ReplicatedUsing = OnRep_Health)
    FGameplayAttributeData Health;
    SUSPENSECORE_ATTRIBUTE_ACCESSORS(USuspenseCoreEnemyAttributeSet, Health)

    UPROPERTY(BlueprintReadOnly, Category = "Health", ReplicatedUsing = OnRep_MaxHealth)
    FGameplayAttributeData MaxHealth;
    SUSPENSECORE_ATTRIBUTE_ACCESSORS(USuspenseCoreEnemyAttributeSet, MaxHealth)

    // ═══════════════════════════════════════════════════════════════════
    // STAMINA ATTRIBUTES
    // ═══════════════════════════════════════════════════════════════════

    UPROPERTY(BlueprintReadOnly, Category = "Stamina", ReplicatedUsing = OnRep_Stamina)
    FGameplayAttributeData Stamina;
    SUSPENSECORE_ATTRIBUTE_ACCESSORS(USuspenseCoreEnemyAttributeSet, Stamina)

    UPROPERTY(BlueprintReadOnly, Category = "Stamina", ReplicatedUsing = OnRep_MaxStamina)
    FGameplayAttributeData MaxStamina;
    SUSPENSECORE_ATTRIBUTE_ACCESSORS(USuspenseCoreEnemyAttributeSet, MaxStamina)

    // ═══════════════════════════════════════════════════════════════════
    // COMBAT ATTRIBUTES
    // ═══════════════════════════════════════════════════════════════════

    UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_Armor)
    FGameplayAttributeData Armor;
    SUSPENSECORE_ATTRIBUTE_ACCESSORS(USuspenseCoreEnemyAttributeSet, Armor)

    UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_AttackPower)
    FGameplayAttributeData AttackPower;
    SUSPENSECORE_ATTRIBUTE_ACCESSORS(USuspenseCoreEnemyAttributeSet, AttackPower)

    // ═══════════════════════════════════════════════════════════════════
    // META ATTRIBUTES
    // ═══════════════════════════════════════════════════════════════════

    UPROPERTY(BlueprintReadOnly, Category = "Meta")
    FGameplayAttributeData IncomingDamage;
    SUSPENSECORE_ATTRIBUTE_ACCESSORS(USuspenseCoreEnemyAttributeSet, IncomingDamage)

    // ═══════════════════════════════════════════════════════════════════
    // OVERRIDES
    // ═══════════════════════════════════════════════════════════════════

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

protected:
    UFUNCTION()
    void OnRep_Health(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_Stamina(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_Armor(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_AttackPower(const FGameplayAttributeData& OldValue);

private:
    void HandleDeath();
    bool bIsDead = false;
};
```

---

## EventBus Communication

### Native Tags Definition

```cpp
// SuspenseCoreEnemyNativeTags.h
#pragma once

#include "NativeGameplayTags.h"

namespace SuspenseCoreEnemyTags
{
    // ═══════════════════════════════════════════════════════════════════
    // EVENTS
    // ═══════════════════════════════════════════════════════════════════

    namespace Event
    {
        namespace Enemy
        {
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Spawned);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Died);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(StateChanged);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TargetAcquired);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TargetLost);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AttackStarted);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AttackEnded);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageTaken);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Alerted);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(LostInterest);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(CalledReinforcements);
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // STATES
    // ═══════════════════════════════════════════════════════════════════

    namespace State
    {
        namespace Enemy
        {
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Idle);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Patrol);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Alert);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Investigate);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combat);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Chase);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cover);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Flank);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Retreat);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Reload);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Heal);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Dead);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stunned);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Suppressed);
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // ABILITIES
    // ═══════════════════════════════════════════════════════════════════

    namespace Ability
    {
        namespace Enemy
        {
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attack);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(MeleeAttack);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ThrowGrenade);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Reload);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Heal);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sprint);
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // TYPES
    // ═══════════════════════════════════════════════════════════════════

    namespace Type
    {
        namespace Enemy
        {
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Scav);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(PMC);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Boss);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Raider);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Guard);
            ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sniper);
        }
    }
}
```

### Event Publishing Example

```cpp
void ASuspenseCoreEnemy::HandleDeath(AActor* DamageInstigator, AActor* DamageCauser)
{
    if (bIsDead) return;
    bIsDead = true;

    // Publish death event
    if (USuspenseCoreEventBus* EventBus = USuspenseCoreHelpers::GetEventBus(this))
    {
        FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
        EventData.SetObject(TEXT("Instigator"), DamageInstigator);
        EventData.SetObject(TEXT("Causer"), DamageCauser);
        EventData.SetString(TEXT("EnemyClassID"), EnemyClassID.ToString());
        EventData.SetVector(TEXT("DeathLocation"), GetActorLocation());

        EventBus->Publish(SuspenseCoreEnemyTags::Event::Enemy::Died, EventData);
    }

    // Force dead state
    if (AIComponent)
    {
        AIComponent->ForceState(SuspenseCoreEnemyTags::State::Enemy::Dead);
    }
}
```

---

## Perception System

### Perception Component

```cpp
// USuspenseCoreEnemyPerceptionComponent.h
UCLASS(ClassGroup = "SuspenseCore", meta = (BlueprintSpawnableComponent))
class ENEMYSYSTEM_API USuspenseCoreEnemyPerceptionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USuspenseCoreEnemyPerceptionComponent();

    // ═══════════════════════════════════════════════════════════════════
    // PERCEPTION API
    // ═══════════════════════════════════════════════════════════════════

    /** Check if can see specific actor */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Enemy|Perception")
    bool CanSeeActor(AActor* Target) const;

    /** Check if heard sound from location */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Enemy|Perception")
    bool HeardSoundAt(const FVector& Location) const;

    /** Get current primary target */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Enemy|Perception")
    AActor* GetPrimaryTarget() const { return PrimaryTarget.Get(); }

    /** Get all perceived hostile actors */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Enemy|Perception")
    TArray<AActor*> GetPerceivedHostiles() const;

    /** Calculate threat level for actor (0-1) */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Enemy|Perception")
    float CalculateThreatLevel(AActor* Target) const;

    // ═══════════════════════════════════════════════════════════════════
    // EVENTS
    // ═══════════════════════════════════════════════════════════════════

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
        FOnTargetPerceived,
        AActor*, Target,
        float, ThreatLevel
    );

    UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Enemy|Perception")
    FOnTargetPerceived OnTargetPerceived;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
        FOnTargetLost,
        AActor*, Target
    );

    UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Enemy|Perception")
    FOnTargetLost OnTargetLost;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    // ═══════════════════════════════════════════════════════════════════
    // CONFIGURATION
    // ═══════════════════════════════════════════════════════════════════

    /** Sight range in cm */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception|Sight")
    float SightRadius = 3000.0f;

    /** Peripheral vision half-angle (degrees) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception|Sight")
    float PeripheralVisionAngle = 90.0f;

    /** How long target stays in memory after losing sight (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception|Sight")
    float SightMemoryDuration = 5.0f;

    /** Hearing range in cm */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception|Hearing")
    float HearingRange = 2000.0f;

    /** Minimum sound loudness to perceive (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception|Hearing")
    float HearingThreshold = 0.3f;

private:
    UPROPERTY()
    TWeakObjectPtr<AActor> PrimaryTarget;

    /** Map of perceived actors to their last known info */
    TMap<TWeakObjectPtr<AActor>, FSuspenseCorePerceptionInfo> PerceivedActors;

    void UpdatePerception(float DeltaTime);
    void EvaluatePrimaryTarget();
    bool PerformLineOfSightCheck(AActor* Target) const;
};
```

---

## Implementation Phases

### Phase 1: Core Infrastructure (Week 1-2)

| Task | Priority | Files |
|------|----------|-------|
| Create EnemySystem module | Critical | EnemySystem.Build.cs |
| Define native GameplayTags | Critical | SuspenseCoreEnemyNativeTags.h/cpp |
| Create ISuspenseCoreEnemy interface (extend) | Critical | ISuspenseCoreEnemy.h |
| Create USuspenseCoreEnemyClassData | High | SuspenseCoreEnemyClassData.h |
| Create FSuspenseCoreEnemyClassRow | High | SuspenseCoreEnemyDataTypes.h |

### Phase 2: Character Classes (Week 2-3)

| Task | Priority | Files |
|------|----------|-------|
| Create ASuspenseCoreEnemy | Critical | SuspenseCoreEnemy.h/cpp |
| Create ASuspenseCoreEnemyState | Critical | SuspenseCoreEnemyState.h/cpp |
| Create ASuspenseCoreEnemyAIController | Critical | SuspenseCoreEnemyAIController.h/cpp |
| Create USuspenseCoreEnemyAttributeSet | High | SuspenseCoreEnemyAttributeSet.h/cpp |

### Phase 3: AI Components (Week 3-4)

| Task | Priority | Files |
|------|----------|-------|
| Create USuspenseCoreEnemyAIComponent (FSM) | Critical | SuspenseCoreEnemyAIComponent.h/cpp |
| Create USuspenseCoreEnemyPerceptionComponent | Critical | SuspenseCoreEnemyPerceptionComponent.h/cpp |
| Create USuspenseCoreEnemyBehaviorComponent | High | SuspenseCoreEnemyBehaviorComponent.h/cpp |
| Create USuspenseCoreEnemySquadComponent | Medium | SuspenseCoreEnemySquadComponent.h/cpp |

### Phase 4: StateTree Integration (Week 4-5)

| Task | Priority | Files |
|------|----------|-------|
| Create base StateTree tasks | Critical | STTask_*.h/cpp |
| Create StateTree conditions | High | STCondition_*.h/cpp |
| Create StateTree evaluators | Medium | STEval_*.h/cpp |
| Create ST_Enemy_Base.uasset | Critical | Content/EnemySystem/AI/StateTree/ |

### Phase 5: Services & Subsystems (Week 5-6)

| Task | Priority | Files |
|------|----------|-------|
| Create USuspenseCoreEnemySubsystem | High | SuspenseCoreEnemySubsystem.h/cpp |
| Create USuspenseCoreEnemySpawnService | High | SuspenseCoreEnemySpawnService.h/cpp |
| Create USuspenseCoreEnemyAIService | Medium | SuspenseCoreEnemyAIService.h/cpp |
| Integrate with DataManager | High | DataManager extension |

### Phase 6: GAS Abilities (Week 6-7)

| Task | Priority | Files |
|------|----------|-------|
| Create GA_Enemy_Attack | Critical | GA_Enemy_Attack.h/cpp |
| Create GA_Enemy_Reload | High | GA_Enemy_Reload.h/cpp |
| Create GA_Enemy_ThrowGrenade | Medium | GA_Enemy_ThrowGrenade.h/cpp |
| Create GA_Enemy_Heal | Medium | GA_Enemy_Heal.h/cpp |

### Phase 7: Testing & Polish (Week 7-8)

| Task | Priority | Files |
|------|----------|-------|
| Unit tests for FSM | High | Tests/ |
| Integration tests | High | Tests/ |
| Debug visualization | Medium | Debug/ |
| Documentation | Medium | Documentation/ |

---

## File Structure

```
Source/EnemySystem/
├── Public/SuspenseCore/
│   ├── Characters/
│   │   ├── SuspenseCoreEnemy.h
│   │   └── SuspenseCoreEnemyState.h
│   ├── Controllers/
│   │   └── SuspenseCoreEnemyAIController.h
│   ├── Components/
│   │   ├── SuspenseCoreEnemyAIComponent.h
│   │   ├── SuspenseCoreEnemyPerceptionComponent.h
│   │   ├── SuspenseCoreEnemyBehaviorComponent.h
│   │   └── SuspenseCoreEnemySquadComponent.h
│   ├── Attributes/
│   │   └── SuspenseCoreEnemyAttributeSet.h
│   ├── Abilities/
│   │   ├── GA_Enemy_Attack.h
│   │   ├── GA_Enemy_Reload.h
│   │   ├── GA_Enemy_ThrowGrenade.h
│   │   └── GA_Enemy_Heal.h
│   ├── Data/
│   │   ├── SuspenseCoreEnemyClassData.h
│   │   └── SuspenseCoreEnemyDataTypes.h
│   ├── Services/
│   │   ├── SuspenseCoreEnemySpawnService.h
│   │   └── SuspenseCoreEnemyAIService.h
│   ├── Subsystems/
│   │   └── SuspenseCoreEnemySubsystem.h
│   ├── StateTree/
│   │   ├── Tasks/
│   │   │   ├── STTask_SuspenseCoreFindCover.h
│   │   │   ├── STTask_SuspenseCoreMoveTo.h
│   │   │   ├── STTask_SuspenseCoreAttack.h
│   │   │   ├── STTask_SuspenseCoreInvestigate.h
│   │   │   └── STTask_SuspenseCorePatrol.h
│   │   ├── Conditions/
│   │   │   ├── STCondition_SuspenseCoreCanSeeTarget.h
│   │   │   ├── STCondition_SuspenseCoreInRange.h
│   │   │   ├── STCondition_SuspenseCoreLowHealth.h
│   │   │   └── STCondition_SuspenseCoreHasAmmo.h
│   │   └── Evaluators/
│   │       ├── STEval_SuspenseCoreThreatLevel.h
│   │       └── STEval_SuspenseCoreBestCover.h
│   └── Tags/
│       └── SuspenseCoreEnemyNativeTags.h
├── Private/SuspenseCore/
│   ├── Characters/
│   │   ├── SuspenseCoreEnemy.cpp
│   │   └── SuspenseCoreEnemyState.cpp
│   ├── Controllers/
│   │   └── SuspenseCoreEnemyAIController.cpp
│   ├── Components/
│   │   ├── SuspenseCoreEnemyAIComponent.cpp
│   │   ├── SuspenseCoreEnemyPerceptionComponent.cpp
│   │   ├── SuspenseCoreEnemyBehaviorComponent.cpp
│   │   └── SuspenseCoreEnemySquadComponent.cpp
│   ├── Attributes/
│   │   └── SuspenseCoreEnemyAttributeSet.cpp
│   ├── Abilities/
│   │   ├── GA_Enemy_Attack.cpp
│   │   ├── GA_Enemy_Reload.cpp
│   │   ├── GA_Enemy_ThrowGrenade.cpp
│   │   └── GA_Enemy_Heal.cpp
│   ├── Data/
│   │   └── SuspenseCoreEnemyClassData.cpp
│   ├── Services/
│   │   ├── SuspenseCoreEnemySpawnService.cpp
│   │   └── SuspenseCoreEnemyAIService.cpp
│   ├── Subsystems/
│   │   └── SuspenseCoreEnemySubsystem.cpp
│   ├── StateTree/
│   │   ├── Tasks/
│   │   │   ├── STTask_SuspenseCoreFindCover.cpp
│   │   │   ├── STTask_SuspenseCoreMoveTo.cpp
│   │   │   ├── STTask_SuspenseCoreAttack.cpp
│   │   │   ├── STTask_SuspenseCoreInvestigate.cpp
│   │   │   └── STTask_SuspenseCorePatrol.cpp
│   │   ├── Conditions/
│   │   │   ├── STCondition_SuspenseCoreCanSeeTarget.cpp
│   │   │   ├── STCondition_SuspenseCoreInRange.cpp
│   │   │   ├── STCondition_SuspenseCoreLowHealth.cpp
│   │   │   └── STCondition_SuspenseCoreHasAmmo.cpp
│   │   └── Evaluators/
│   │       ├── STEval_SuspenseCoreThreatLevel.cpp
│   │       └── STEval_SuspenseCoreBestCover.cpp
│   └── Tags/
│       └── SuspenseCoreEnemyNativeTags.cpp
└── EnemySystem.Build.cs

Content/EnemySystem/
├── AI/
│   ├── StateTree/
│   │   ├── ST_Enemy_Base.uasset
│   │   ├── ST_Enemy_Scav.uasset
│   │   ├── ST_Enemy_PMC.uasset
│   │   ├── ST_Enemy_Boss.uasset
│   │   └── ST_Enemy_Sniper.uasset
│   └── Blackboard/
│       └── BB_Enemy_Base.uasset
├── Data/
│   ├── DataTables/
│   │   └── DT_EnemyClasses.uasset
│   └── DataAssets/
│       ├── DA_Enemy_Scav_Basic.uasset
│       ├── DA_Enemy_Scav_Armed.uasset
│       ├── DA_Enemy_PMC_USEC.uasset
│       ├── DA_Enemy_PMC_BEAR.uasset
│       └── DA_Enemy_Boss_Killa.uasset
├── Abilities/
│   └── Effects/
│       ├── GE_Enemy_Damage.uasset
│       └── GE_Enemy_Initialize.uasset
└── Blueprints/
    ├── BP_Enemy_Scav.uasset
    ├── BP_Enemy_PMC.uasset
    └── BP_Enemy_Boss.uasset
```

---

## Code Templates

### Module Registration

```cpp
// EnemySystem.cpp
#include "EnemySystem.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FEnemySystemModule"

void FEnemySystemModule::StartupModule()
{
    UE_LOG(LogTemp, Log, TEXT("EnemySystem Module Started"));
}

void FEnemySystemModule::ShutdownModule()
{
    UE_LOG(LogTemp, Log, TEXT("EnemySystem Module Shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FEnemySystemModule, EnemySystem)
```

### Native Tags Implementation

```cpp
// SuspenseCoreEnemyNativeTags.cpp
#include "SuspenseCore/Tags/SuspenseCoreEnemyNativeTags.h"

namespace SuspenseCoreEnemyTags
{
    namespace Event
    {
        namespace Enemy
        {
            UE_DEFINE_GAMEPLAY_TAG(Spawned, "SuspenseCore.Event.Enemy.Spawned");
            UE_DEFINE_GAMEPLAY_TAG(Died, "SuspenseCore.Event.Enemy.Died");
            UE_DEFINE_GAMEPLAY_TAG(StateChanged, "SuspenseCore.Event.Enemy.StateChanged");
            UE_DEFINE_GAMEPLAY_TAG(TargetAcquired, "SuspenseCore.Event.Enemy.TargetAcquired");
            UE_DEFINE_GAMEPLAY_TAG(TargetLost, "SuspenseCore.Event.Enemy.TargetLost");
            UE_DEFINE_GAMEPLAY_TAG(AttackStarted, "SuspenseCore.Event.Enemy.AttackStarted");
            UE_DEFINE_GAMEPLAY_TAG(AttackEnded, "SuspenseCore.Event.Enemy.AttackEnded");
            UE_DEFINE_GAMEPLAY_TAG(DamageTaken, "SuspenseCore.Event.Enemy.DamageTaken");
            UE_DEFINE_GAMEPLAY_TAG(Alerted, "SuspenseCore.Event.Enemy.Alerted");
            UE_DEFINE_GAMEPLAY_TAG(LostInterest, "SuspenseCore.Event.Enemy.LostInterest");
            UE_DEFINE_GAMEPLAY_TAG(CalledReinforcements, "SuspenseCore.Event.Enemy.CalledReinforcements");
        }
    }

    namespace State
    {
        namespace Enemy
        {
            UE_DEFINE_GAMEPLAY_TAG(Idle, "State.Enemy.Idle");
            UE_DEFINE_GAMEPLAY_TAG(Patrol, "State.Enemy.Patrol");
            UE_DEFINE_GAMEPLAY_TAG(Alert, "State.Enemy.Alert");
            UE_DEFINE_GAMEPLAY_TAG(Investigate, "State.Enemy.Investigate");
            UE_DEFINE_GAMEPLAY_TAG(Combat, "State.Enemy.Combat");
            UE_DEFINE_GAMEPLAY_TAG(Chase, "State.Enemy.Chase");
            UE_DEFINE_GAMEPLAY_TAG(Cover, "State.Enemy.Cover");
            UE_DEFINE_GAMEPLAY_TAG(Flank, "State.Enemy.Flank");
            UE_DEFINE_GAMEPLAY_TAG(Retreat, "State.Enemy.Retreat");
            UE_DEFINE_GAMEPLAY_TAG(Reload, "State.Enemy.Reload");
            UE_DEFINE_GAMEPLAY_TAG(Heal, "State.Enemy.Heal");
            UE_DEFINE_GAMEPLAY_TAG(Dead, "State.Enemy.Dead");
            UE_DEFINE_GAMEPLAY_TAG(Stunned, "State.Enemy.Stunned");
            UE_DEFINE_GAMEPLAY_TAG(Suppressed, "State.Enemy.Suppressed");
        }
    }

    namespace Ability
    {
        namespace Enemy
        {
            UE_DEFINE_GAMEPLAY_TAG(Attack, "SuspenseCore.Ability.Enemy.Attack");
            UE_DEFINE_GAMEPLAY_TAG(MeleeAttack, "SuspenseCore.Ability.Enemy.MeleeAttack");
            UE_DEFINE_GAMEPLAY_TAG(ThrowGrenade, "SuspenseCore.Ability.Enemy.ThrowGrenade");
            UE_DEFINE_GAMEPLAY_TAG(Reload, "SuspenseCore.Ability.Enemy.Reload");
            UE_DEFINE_GAMEPLAY_TAG(Heal, "SuspenseCore.Ability.Enemy.Heal");
            UE_DEFINE_GAMEPLAY_TAG(Sprint, "SuspenseCore.Ability.Enemy.Sprint");
        }
    }

    namespace Type
    {
        namespace Enemy
        {
            UE_DEFINE_GAMEPLAY_TAG(Scav, "Type.Enemy.Scav");
            UE_DEFINE_GAMEPLAY_TAG(PMC, "Type.Enemy.PMC");
            UE_DEFINE_GAMEPLAY_TAG(Boss, "Type.Enemy.Boss");
            UE_DEFINE_GAMEPLAY_TAG(Raider, "Type.Enemy.Raider");
            UE_DEFINE_GAMEPLAY_TAG(Guard, "Type.Enemy.Guard");
            UE_DEFINE_GAMEPLAY_TAG(Sniper, "Type.Enemy.Sniper");
        }
    }
}
```

---

## Summary

### Key Architecture Decisions

| Decision | Rationale |
|----------|-----------|
| **StateTree over BehaviorTree** | UE 5.7 native, better debugging, MassEntity support |
| **FSM via GameplayTags** | Consistent with project patterns, EventBus integration |
| **ASC on EnemyState** | Same pattern as PlayerState, replication ready |
| **Component-based AI** | Modular, testable, reusable |
| **SSOT via DataAssets** | Centralized data, designer-friendly |

### Critical Patterns to Follow

1. **Naming**: `USuspenseCoreEnemy*`, `ASuspenseCoreEnemy*`
2. **Tags**: Native declarations (`UE_DECLARE_GAMEPLAY_TAG_EXTERN`)
3. **Communication**: EventBus with GameplayTags
4. **Data**: DataAssets + DataManager
5. **GAS**: ASC on State class, not Character

### Next Steps

1. Утвердить архитектуру с командой
2. Создать модуль EnemySystem
3. Начать Phase 1: Core Infrastructure
4. Параллельно создавать Content assets

---

**Document Status:** Ready for Review

**Estimated Implementation Time:** 6-8 weeks (1 developer)

**Dependencies:**
- BridgeSystem module (EventBus, Interfaces)
- GAS module (Abilities, Attributes)
- Navigation System (NavMesh)
