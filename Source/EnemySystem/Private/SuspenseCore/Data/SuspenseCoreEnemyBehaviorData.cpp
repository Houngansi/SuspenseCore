#include "SuspenseCore/Data/SuspenseCoreEnemyBehaviorData.h"
#include "SuspenseCore/Tags/SuspenseCoreEnemyTags.h"
#include "SuspenseCore/FSM/States/SuspenseCoreEnemyIdleState.h"
#include "SuspenseCore/FSM/States/SuspenseCoreEnemyPatrolState.h"
#include "SuspenseCore/FSM/States/SuspenseCoreEnemyChaseState.h"
#include "SuspenseCore/FSM/States/SuspenseCoreEnemyAttackState.h"
#include "SuspenseCore/FSM/States/SuspenseCoreEnemyDeathState.h"

USuspenseCoreEnemyBehaviorData::USuspenseCoreEnemyBehaviorData()
{
    BehaviorID = FName(TEXT("DefaultEnemy"));
    DisplayName = FText::FromString(TEXT("Default Enemy"));
    EnemyTypeTag = SuspenseCoreEnemyTags::Type::Scav;
    InitialState = SuspenseCoreEnemyTags::State::Idle;

    MaxHealth = 100.0f;
    AttackPower = 25.0f;
    Armor = 0.0f;
    WalkSpeed = 200.0f;
    RunSpeed = 500.0f;
    SightRange = 2000.0f;
    HearingRange = 1500.0f;

    FSuspenseCoreEnemyStateConfig IdleConfig;
    IdleConfig.StateTag = SuspenseCoreEnemyTags::State::Idle;
    IdleConfig.StateClass = USuspenseCoreEnemyIdleState::StaticClass();
    States.Add(IdleConfig);

    FSuspenseCoreEnemyStateConfig PatrolConfig;
    PatrolConfig.StateTag = SuspenseCoreEnemyTags::State::Patrol;
    PatrolConfig.StateClass = USuspenseCoreEnemyPatrolState::StaticClass();
    States.Add(PatrolConfig);

    FSuspenseCoreEnemyStateConfig ChaseConfig;
    ChaseConfig.StateTag = SuspenseCoreEnemyTags::State::Chase;
    ChaseConfig.StateClass = USuspenseCoreEnemyChaseState::StaticClass();
    States.Add(ChaseConfig);

    FSuspenseCoreEnemyStateConfig AttackConfig;
    AttackConfig.StateTag = SuspenseCoreEnemyTags::State::Attack;
    AttackConfig.StateClass = USuspenseCoreEnemyAttackState::StaticClass();
    States.Add(AttackConfig);

    FSuspenseCoreEnemyStateConfig DeathConfig;
    DeathConfig.StateTag = SuspenseCoreEnemyTags::State::Death;
    DeathConfig.StateClass = USuspenseCoreEnemyDeathState::StaticClass();
    States.Add(DeathConfig);

    FSuspenseCoreEnemyTransitionConfig IdleToPatrol;
    IdleToPatrol.FromState = SuspenseCoreEnemyTags::State::Idle;
    IdleToPatrol.OnEvent = SuspenseCoreEnemyTags::Event::IdleTimeout;
    IdleToPatrol.ToState = SuspenseCoreEnemyTags::State::Patrol;
    Transitions.Add(IdleToPatrol);

    FSuspenseCoreEnemyTransitionConfig IdleToChase;
    IdleToChase.FromState = SuspenseCoreEnemyTags::State::Idle;
    IdleToChase.OnEvent = SuspenseCoreEnemyTags::Event::PlayerDetected;
    IdleToChase.ToState = SuspenseCoreEnemyTags::State::Chase;
    Transitions.Add(IdleToChase);

    FSuspenseCoreEnemyTransitionConfig PatrolToChase;
    PatrolToChase.FromState = SuspenseCoreEnemyTags::State::Patrol;
    PatrolToChase.OnEvent = SuspenseCoreEnemyTags::Event::PlayerDetected;
    PatrolToChase.ToState = SuspenseCoreEnemyTags::State::Chase;
    Transitions.Add(PatrolToChase);

    FSuspenseCoreEnemyTransitionConfig ChaseToAttack;
    ChaseToAttack.FromState = SuspenseCoreEnemyTags::State::Chase;
    ChaseToAttack.OnEvent = SuspenseCoreEnemyTags::Event::TargetInRange;
    ChaseToAttack.ToState = SuspenseCoreEnemyTags::State::Attack;
    Transitions.Add(ChaseToAttack);

    FSuspenseCoreEnemyTransitionConfig ChaseToIdle;
    ChaseToIdle.FromState = SuspenseCoreEnemyTags::State::Chase;
    ChaseToIdle.OnEvent = SuspenseCoreEnemyTags::Event::PlayerLost;
    ChaseToIdle.ToState = SuspenseCoreEnemyTags::State::Idle;
    Transitions.Add(ChaseToIdle);

    FSuspenseCoreEnemyTransitionConfig AttackToChase;
    AttackToChase.FromState = SuspenseCoreEnemyTags::State::Attack;
    AttackToChase.OnEvent = SuspenseCoreEnemyTags::Event::TargetOutOfRange;
    AttackToChase.ToState = SuspenseCoreEnemyTags::State::Chase;
    Transitions.Add(AttackToChase);

    FSuspenseCoreEnemyTransitionConfig AttackToIdle;
    AttackToIdle.FromState = SuspenseCoreEnemyTags::State::Attack;
    AttackToIdle.OnEvent = SuspenseCoreEnemyTags::Event::PlayerLost;
    AttackToIdle.ToState = SuspenseCoreEnemyTags::State::Idle;
    Transitions.Add(AttackToIdle);
}
