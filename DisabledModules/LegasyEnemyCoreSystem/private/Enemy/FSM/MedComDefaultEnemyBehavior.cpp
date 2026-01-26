#include "Core/Enemy/FSM/MedComDefaultEnemyBehavior.h"
#include "Core/Enemy/FSM/States/MedComIdleState.h"
#include "Core/Enemy/FSM/States/MedComPatrolState.h"
#include "Core/Enemy/FSM/States/MedComChaseState.h"
#include "Core/Enemy/FSM/States/MedComAttackState.h"
#include "Core/Enemy/FSM/States/MedComReturnState.h"
#include "Core/Enemy/FSM/States/MedComDeathState.h"
#include "Core/AbilitySystem/Abilities/Enemy/MedComEnemyPatrolAbility.h"
#include "Core/AbilitySystem/Abilities/Enemy/MedComEnemyMoveAbility.h"
#include "UObject/ConstructorHelpers.h"
#include "GameplayEffect.h"

// Определение категории лога
DEFINE_LOG_CATEGORY_STATIC(LogDefaultEnemyBehavior, Log, All);

UMedComDefaultEnemyBehavior::UMedComDefaultEnemyBehavior()
{
    // Устанавливаем имя ассета
    FString DisplayName = FString::Printf(TEXT("DefaultEnemyBehavior_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Short));
    SetFlags(RF_Public);

    // Базовые настройки начального состояния
    InitialState = FName("Idle");
    
    // Настройка классов способностей GAS
#if WITH_EDITORONLY_DATA
    // Попытка найти классы способностей, если они уже созданы
    static ConstructorHelpers::FClassFinder<UGameplayAbility> PatrolAbilityFinder(TEXT("/Game/MedCom/GAS/Abilities/States/GA_EnemyPatrol"));
    if (PatrolAbilityFinder.Succeeded())
    {
        PatrolAbilityClass = PatrolAbilityFinder.Class;
    }
    else 
    {
        // Ищем в коде
        PatrolAbilityClass = UMedComEnemyPatrolAbility::StaticClass();
    }
    
    static ConstructorHelpers::FClassFinder<UGameplayAbility> MoveAbilityFinder(TEXT("/Game/MedCom/GAS/Abilities/States/GA_EnemyMove"));
    if (MoveAbilityFinder.Succeeded())
    {
        MoveAbilityClass = MoveAbilityFinder.Class;
    }
    else 
    {
        // Ищем в коде
        MoveAbilityClass = UMedComEnemyMoveAbility::StaticClass();
    }
    
    // Для состояния Return можно использовать ту же способность, что и для движения
    ReturnAbilityClass = MoveAbilityClass;
#endif
    
    // Инициализируем состояния по умолчанию
    InitializeDefaultStates();
}

void UMedComDefaultEnemyBehavior::InitializeDefaultStates()
{
    // Очищаем существующие состояния
    States.Empty();

    // Создаем базовые состояния
    FStateDescription IdleStateDesc;
    IdleStateDesc.StateName = FName("Idle");
    IdleStateDesc.StateClass = UMedComIdleState::StaticClass();
    SetupIdleState(IdleStateDesc);
    States.Add(IdleStateDesc);

    // Создаем состояние патрулирования
    FStateDescription PatrolStateDesc;
    PatrolStateDesc.StateName = FName("Patrol");
    PatrolStateDesc.StateClass = UMedComPatrolState::StaticClass();
    SetupPatrolState(PatrolStateDesc);
    States.Add(PatrolStateDesc);
    
    // Создаем состояние Chase
    FStateDescription ChaseStateDesc;
    ChaseStateDesc.StateName = FName("Chase");
    ChaseStateDesc.StateClass = UMedComChaseState::StaticClass();
    SetupChaseState(ChaseStateDesc);
    States.Add(ChaseStateDesc);
    
    // Создаем состояние Attack
    FStateDescription AttackStateDesc;
    AttackStateDesc.StateName = FName("Attack");
    AttackStateDesc.StateClass = UMedComAttackState::StaticClass();
    SetupAttackState(AttackStateDesc);
    States.Add(AttackStateDesc);
    
    // Создаем состояние Return
    FStateDescription ReturnStateDesc;
    ReturnStateDesc.StateName = FName("Return");
    ReturnStateDesc.StateClass = UMedComReturnState::StaticClass();
    SetupReturnState(ReturnStateDesc);
    States.Add(ReturnStateDesc);
    
    // Создаем состояние Death
    FStateDescription DeathStateDesc;
    DeathStateDesc.StateName = FName("Death");
    DeathStateDesc.StateClass = UMedComDeathState::StaticClass();
    SetupDeathState(DeathStateDesc);
    States.Add(DeathStateDesc);

    // Настройка основных переходов между состояниями
    SetupBasicTransitions();
    
    // Настраиваем специальные переходы для GAS интеграции, если нужно
    if (bUseGASForMovement)
    {
        SetupGASIntegration();
    }
    
    UE_LOG(LogDefaultEnemyBehavior, Log, TEXT("Initialized default enemy behavior with %d states (GAS Integration: %s)"), 
           States.Num(), bUseGASForMovement ? TEXT("Enabled") : TEXT("Disabled"));
}

void UMedComDefaultEnemyBehavior::SetupIdleState(FStateDescription& StateDesc)
{
    // Параметры для состояния Idle
    StateDesc.StateParams.Add(FName("IdleTime"), IdleTime);
    StateDesc.StateParams.Add(FName("LookInterval"), LookIntervalTime);
    StateDesc.StateParams.Add(FName("MaxLookAngle"), MaxLookAngle);
    StateDesc.StateParams.Add(FName("LookRotationSpeed"), LookRotationSpeed);

    // Экстра параметры для GAS интеграции
    if (bUseGASForMovement)
    {
        StateDesc.StateParams.Add(FName("UseGASForMovement"), 1.0f);
    }

    // Переходы для Idle состояния
    // 1. Переход в Patrol по истечению времени
    FStateTransition IdleTimeoutTransition;
    IdleTimeoutTransition.TriggerEvent = EEnemyEvent::IdleTimeout;
    IdleTimeoutTransition.TargetState = FName("Patrol");
    IdleTimeoutTransition.Delay = 0.0f;
    StateDesc.Transitions.Add(IdleTimeoutTransition);

    // 2. Переход в Chase при обнаружении игрока
    FStateTransition PlayerSeenTransition;
    PlayerSeenTransition.TriggerEvent = EEnemyEvent::PlayerSeen;
    PlayerSeenTransition.TargetState = FName("Chase");
    PlayerSeenTransition.Delay = 0.2f; // Небольшая задержка для анимации реакции
    StateDesc.Transitions.Add(PlayerSeenTransition);

    // 3. Переход в Chase при получении урона
    FStateTransition DamageTransition;
    DamageTransition.TriggerEvent = EEnemyEvent::TookDamage;
    DamageTransition.TargetState = FName("Chase");
    DamageTransition.Delay = 0.0f;
    StateDesc.Transitions.Add(DamageTransition);
    
    // 4. Переход в Death при смерти
    FStateTransition DeathTransition;
    DeathTransition.TriggerEvent = EEnemyEvent::Dead;
    DeathTransition.TargetState = FName("Death");
    DeathTransition.Delay = 0.0f;
    StateDesc.Transitions.Add(DeathTransition);
}

void UMedComDefaultEnemyBehavior::SetupPatrolState(FStateDescription& StateDesc)
{
    // Параметры для состояния Patrol
    StateDesc.StateParams.Add(FName("PatrolSpeed"), PatrolSpeed);
    StateDesc.StateParams.Add(FName("LoopPatrol"), bLoopPatrol ? 1.0f : 0.0f);
    StateDesc.StateParams.Add(FName("UseRandomPatrol"), bUseRandomPatrol ? 1.0f : 0.0f);
    StateDesc.StateParams.Add(FName("AcceptanceRadius"), PatrolAcceptanceRadius);
    StateDesc.StateParams.Add(FName("NumPatrolPoints"), (float)NumPatrolPoints);
    StateDesc.StateParams.Add(FName("MaxPatrolDistance"), MaxPatrolDistance);
    StateDesc.StateParams.Add(FName("RepathDistance"), RepathDistance);
    StateDesc.StateParams.Add(FName("PatrolRotationRate"), PatrolRotationRate);
    StateDesc.StateParams.Add(FName("LookAroundWhilePatrolling"), bLookAroundWhilePatrolling ? 1.0f : 0.0f);
    StateDesc.StateParams.Add(FName("PatrolLookAroundInterval"), PatrolLookAroundInterval);
    StateDesc.StateParams.Add(FName("PatrolLookAroundDuration"), PatrolLookAroundDuration);
    
    // Также добавляем параметр для GAS интеграции
    StateDesc.StateParams.Add(FName("UseGASForMovement"), bUseGASForMovement ? 1.0f : 0.0f);
    
    // Если задан класс GAS способности, сохраняем имя класса как строковый параметр
    if (bUseGASForMovement && PatrolAbilityClass)
    {
        // Для использования ExtendedParams добавляем имя класса как строковый параметр
        StateDesc.StateParams.Add(FName("AbilityClass"), (float)GetTypeHash(PatrolAbilityClass->GetFName()));
    }

    // Переходы для Patrol состояния
    // 1. Переход в Idle по завершении патрулирования
    FStateTransition PatrolCompleteTransition;
    PatrolCompleteTransition.TriggerEvent = EEnemyEvent::PatrolComplete;
    PatrolCompleteTransition.TargetState = FName("Idle");
    PatrolCompleteTransition.Delay = 0.0f;
    StateDesc.Transitions.Add(PatrolCompleteTransition);

    // 2. Переход в Chase при обнаружении игрока
    FStateTransition PlayerSeenTransition;
    PlayerSeenTransition.TriggerEvent = EEnemyEvent::PlayerSeen;
    PlayerSeenTransition.TargetState = FName("Chase");
    PlayerSeenTransition.Delay = 0.1f;
    StateDesc.Transitions.Add(PlayerSeenTransition);

    // 3. Переход в Chase при получении урона
    FStateTransition DamageTransition;
    DamageTransition.TriggerEvent = EEnemyEvent::TookDamage;
    DamageTransition.TargetState = FName("Chase");
    DamageTransition.Delay = 0.0f;
    StateDesc.Transitions.Add(DamageTransition);
    
    // 4. Переход в Death при смерти
    FStateTransition DeathTransition;
    DeathTransition.TriggerEvent = EEnemyEvent::Dead;
    DeathTransition.TargetState = FName("Death");
    DeathTransition.Delay = 0.0f;
    StateDesc.Transitions.Add(DeathTransition);
}

void UMedComDefaultEnemyBehavior::SetupChaseState(FStateDescription& StateDesc)
{
    // Параметры для состояния Chase
    StateDesc.StateParams.Add(FName("ChaseSpeed"), ChaseSpeed);
    StateDesc.StateParams.Add(FName("UpdateInterval"), ChaseUpdateInterval);
    StateDesc.StateParams.Add(FName("LoseTargetTime"), LoseTargetTime);
    StateDesc.StateParams.Add(FName("MinChaseDistance"), MinTargetDistance);
    StateDesc.StateParams.Add(FName("ChaseRotationRate"), ChaseRotationRate);
    
    // Интеграция с GAS
    StateDesc.StateParams.Add(FName("UseGASForMovement"), bUseGASForMovement ? 1.0f : 0.0f);
    
    if (bUseGASForMovement && MoveAbilityClass)
    {
        StateDesc.StateParams.Add(FName("AbilityClass"), (float)GetTypeHash(MoveAbilityClass->GetFName()));
    }
    
    // Переходы
    // 1. Переход в Return при потере цели
    FStateTransition LostSightTransition;
    LostSightTransition.TriggerEvent = EEnemyEvent::PlayerLost;
    LostSightTransition.TargetState = FName("Return");
    LostSightTransition.Delay = LoseTargetTime; // Задержка перед возвратом
    StateDesc.Transitions.Add(LostSightTransition);
    
    // 2. Переход в Attack при достижении цели
    FStateTransition ReachedTargetTransition;
    ReachedTargetTransition.TriggerEvent = EEnemyEvent::ReachedTarget;
    ReachedTargetTransition.TargetState = FName("Attack");
    ReachedTargetTransition.Delay = 0.0f;
    StateDesc.Transitions.Add(ReachedTargetTransition);
    
    // 3. Переход в Death при смерти
    FStateTransition DeathTransition;
    DeathTransition.TriggerEvent = EEnemyEvent::Dead;
    DeathTransition.TargetState = FName("Death");
    DeathTransition.Delay = 0.0f;
    StateDesc.Transitions.Add(DeathTransition);
}

void UMedComDefaultEnemyBehavior::SetupAttackState(FStateDescription& StateDesc)
{
    // Параметры для состояния Attack
    StateDesc.StateParams.Add(FName("AttackRange"), AttackRange);
    StateDesc.StateParams.Add(FName("AttackInterval"), AttackInterval);
    StateDesc.StateParams.Add(FName("AttackDamage"), AttackDamage);
    StateDesc.StateParams.Add(FName("AttackRadius"), AttackRadius);
    StateDesc.StateParams.Add(FName("AttackAngle"), AttackAngle);
    StateDesc.StateParams.Add(FName("AttackDelay"), AttackDelay);
    
    // Интеграция с GAS
    StateDesc.StateParams.Add(FName("UseGASForMovement"), bUseGASForMovement ? 1.0f : 0.0f);
    
    // Переходы
    // 1. Возврат в Chase если цель вышла из радиуса атаки
    FStateTransition TargetOutOfRangeTransition;
    TargetOutOfRangeTransition.TriggerEvent = EEnemyEvent::TargetOutOfRange;
    TargetOutOfRangeTransition.TargetState = FName("Chase");
    TargetOutOfRangeTransition.Delay = 0.0f;
    StateDesc.Transitions.Add(TargetOutOfRangeTransition);
    
    // 2. Переход в Return при потере цели
    FStateTransition PlayerLostTransition;
    PlayerLostTransition.TriggerEvent = EEnemyEvent::PlayerLost;
    PlayerLostTransition.TargetState = FName("Return");
    PlayerLostTransition.Delay = 0.0f;
    StateDesc.Transitions.Add(PlayerLostTransition);
    
    // 3. Переход в Death при смерти
    FStateTransition DeathTransition;
    DeathTransition.TriggerEvent = EEnemyEvent::Dead;
    DeathTransition.TargetState = FName("Death");
    DeathTransition.Delay = 0.0f;
    StateDesc.Transitions.Add(DeathTransition);
}

void UMedComDefaultEnemyBehavior::SetupReturnState(FStateDescription& StateDesc)
{
    // Параметры для состояния Return
    StateDesc.StateParams.Add(FName("ReturnSpeed"), ReturnSpeed);
    StateDesc.StateParams.Add(FName("PathUpdateInterval"), ReturnUpdateInterval);
    StateDesc.StateParams.Add(FName("AcceptanceRadius"), ReturnAcceptanceRadius);
    
    // Интеграция с GAS
    StateDesc.StateParams.Add(FName("UseGASForMovement"), bUseGASForMovement ? 1.0f : 0.0f);
    
    if (bUseGASForMovement && ReturnAbilityClass)
    {
        StateDesc.StateParams.Add(FName("AbilityClass"), (float)GetTypeHash(ReturnAbilityClass->GetFName()));
    }
    
    // Переходы
    // 1. Возврат в Idle при достижении точки возврата
    FStateTransition ReturnCompleteTransition;
    ReturnCompleteTransition.TriggerEvent = EEnemyEvent::ReturnComplete;
    ReturnCompleteTransition.TargetState = FName("Idle");
    ReturnCompleteTransition.Delay = 0.0f;
    StateDesc.Transitions.Add(ReturnCompleteTransition);
    
    // 2. Возврат в Chase при обнаружении игрока
    FStateTransition PlayerSeenTransition;
    PlayerSeenTransition.TriggerEvent = EEnemyEvent::PlayerSeen;
    PlayerSeenTransition.TargetState = FName("Chase");
    PlayerSeenTransition.Delay = 0.1f;
    StateDesc.Transitions.Add(PlayerSeenTransition);
    
    // 3. Возврат в Chase при получении урона
    FStateTransition DamageTransition;
    DamageTransition.TriggerEvent = EEnemyEvent::TookDamage;
    DamageTransition.TargetState = FName("Chase");
    DamageTransition.Delay = 0.0f;
    StateDesc.Transitions.Add(DamageTransition);
    
    // 4. Переход в Death при смерти
    FStateTransition DeathTransition;
    DeathTransition.TriggerEvent = EEnemyEvent::Dead;
    DeathTransition.TargetState = FName("Death");
    DeathTransition.Delay = 0.0f;
    StateDesc.Transitions.Add(DeathTransition);
}

void UMedComDefaultEnemyBehavior::SetupDeathState(FStateDescription& StateDesc)
{
    // Параметры для состояния Death
    StateDesc.StateParams.Add(FName("RagdollDelay"), RagdollDelay);
    StateDesc.StateParams.Add(FName("DespawnTime"), DespawnTime);
    StateDesc.StateParams.Add(FName("DestroyOnDeath"), bDestroyOnDeath ? 1.0f : 0.0f);
    
    // Состояние Death не имеет переходов, так как это конечное состояние
}

void UMedComDefaultEnemyBehavior::SetupBasicTransitions()
{
    // Все базовые переходы уже настроены в отдельных методах SetupXXXState
    // Этот метод оставлен для возможных дополнительных настроек или глобальных переходов
}

void UMedComDefaultEnemyBehavior::SetupGASIntegration()
{
    // Этот метод настраивает специальные параметры и переходы для интеграции FSM и GAS
    
    // Если у нас есть State-Ability маппинг, настраиваем его здесь
    for (FStateDescription& State : States)
    {
        if (State.StateName == FName("Patrol") && PatrolAbilityClass)
        {
            // Сохраняем путь к классу способности
            State.StateParams.Add(FName("AbilityClassPath"), (float)GetTypeHash(PatrolAbilityClass->GetPathName()));
        }
        else if (State.StateName == FName("Chase") && MoveAbilityClass)
        {
            // Аналогично для Chase состояния и MoveAbility
            State.StateParams.Add(FName("AbilityClassPath"), (float)GetTypeHash(MoveAbilityClass->GetPathName()));
        }
        else if (State.StateName == FName("Return") && ReturnAbilityClass)
        {
            // Для Return состояния
            State.StateParams.Add(FName("AbilityClassPath"), (float)GetTypeHash(ReturnAbilityClass->GetPathName()));
        }
    }
}

#if WITH_EDITOR
void UMedComDefaultEnemyBehavior::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    
    // Получаем имя измененного свойства
    FName PropertyName = PropertyChangedEvent.GetPropertyName();
    FName MemberPropertyName = PropertyChangedEvent.GetMemberPropertyName();
    
    // Если изменились параметры, которые влияют на FSM, пересоздаем состояния
    bool bShouldReinitializeStates = 
        PropertyName == GET_MEMBER_NAME_CHECKED(UMedComDefaultEnemyBehavior, bUseGASForMovement) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(UMedComDefaultEnemyBehavior, PatrolAbilityClass) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(UMedComDefaultEnemyBehavior, MoveAbilityClass) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(UMedComDefaultEnemyBehavior, ReturnAbilityClass) ||
        MemberPropertyName == GET_MEMBER_NAME_CHECKED(UMedComDefaultEnemyBehavior, bUseGASForMovement) ||
        MemberPropertyName == GET_MEMBER_NAME_CHECKED(UMedComDefaultEnemyBehavior, States);
    
    if (bShouldReinitializeStates)
    {
        // Пересоздаем состояния
        InitializeDefaultStates();
        
        UE_LOG(LogDefaultEnemyBehavior, Verbose, TEXT("Reinitialized states after property change: %s"), 
               *PropertyName.ToString());
    }
}
#endif