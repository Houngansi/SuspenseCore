#include "Core/Enemy/FSM/EnemyBehaviorDataAsset.h"
#include "Core/Enemy/FSM/MedComEnemyState.h"
#include "UObject/ObjectSaveContext.h"
#if WITH_EDITOR
#include "Misc/ScopedSlowTask.h"
#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "EnemyBehaviorDataAsset"

// Определение категории лога
DEFINE_LOG_CATEGORY_STATIC(LogEnemyBehaviorAsset, Log, All);

UEnemyBehaviorDataAsset::UEnemyBehaviorDataAsset()
    : InitialState(NAME_None)
    , WeaponSocket(TEXT("GripPoint"))
    , IdleTime(5.0f)
    , LookIntervalTime(2.0f)
    , MaxLookAngle(60.0f)
    , LookRotationSpeed(2.0f)
    , PatrolSpeed(300.0f)
    , bLoopPatrol(true)
    , bUseRandomPatrol(false)
    , PatrolAcceptanceRadius(100.0f)
    , NumPatrolPoints(4)
    , MaxPatrolDistance(3000.0f)
    , RepathDistance(100.0f)
    , PatrolRotationRate(300.0f)
    , bLookAroundWhilePatrolling(false)
    , PatrolLookAroundInterval(3.0f)
    , PatrolLookAroundDuration(1.5f)
    , ChaseSpeed(600.0f)
    , ChaseUpdateInterval(0.5f)
    , LoseTargetTime(5.0f)
    , MinTargetDistance(500.0f)
    , ChaseRotationRate(600.0f)
    , AttackRange(1000.0f)
    , AttackInterval(1.5f)
    , AttackDamage(10.0f)
    , AttackRadius(50.0f)
    , AttackAngle(60.0f)
    , AttackDelay(0.3f)
    , SearchTime(10.0f)
{
}

#if WITH_EDITOR
void UEnemyBehaviorDataAsset::PostLoad()
{
    Super::PostLoad();
    
    // Проверка на ошибки в данных
    if (States.Num() == 0)
    {
        UE_LOG(LogEnemyBehaviorAsset, Warning, TEXT("BehaviorAsset '%s' has no states defined"), *GetName());
    }
    
    // Проверка на валидность InitialState
    if (!InitialState.IsNone())
    {
        bool bInitialStateValid = false;
        for (const FStateDescription& State : States)
        {
            if (State.StateName == InitialState)
            {
                bInitialStateValid = true;
                break;
            }
        }
        
        if (!bInitialStateValid)
        {
            UE_LOG(LogEnemyBehaviorAsset, Error, TEXT("BehaviorAsset '%s': Initial state '%s' not found in state list"), 
                   *GetName(), *InitialState.ToString());
        }
    }
    
    // Проверка на валидность переходов
    for (const FStateDescription& State : States)
    {
        if (!State.StateClass)
        {
            UE_LOG(LogEnemyBehaviorAsset, Error, TEXT("BehaviorAsset '%s': State '%s' has no class assigned"), 
                   *GetName(), *State.StateName.ToString());
            continue;
        }
        
        for (const FStateTransition& Transition : State.Transitions)
        {
            bool bTargetValid = false;
            for (const FStateDescription& TargetState : States)
            {
                if (TargetState.StateName == Transition.TargetState)
                {
                    bTargetValid = true;
                    break;
                }
            }
            
            if (!bTargetValid)
            {
                UE_LOG(LogEnemyBehaviorAsset, Error, 
                       TEXT("BehaviorAsset '%s': State '%s' has transition to non-existent state '%s'"), 
                       *GetName(), *State.StateName.ToString(), *Transition.TargetState.ToString());
            }
        }
    }
}

void UEnemyBehaviorDataAsset::PreSave(FObjectPreSaveContext SaveContext)
{
    Super::PreSave(SaveContext);
    
    // Исправляем некоторые ошибки перед сохранением
    
    // Если не указано начальное состояние, но есть состояния - берем первое
    if (InitialState.IsNone() && States.Num() > 0)
    {
        InitialState = States[0].StateName;
        
        UE_LOG(LogEnemyBehaviorAsset, Warning, 
               TEXT("BehaviorAsset '%s': Setting initial state to '%s' since none was specified"), 
               *GetName(), *InitialState.ToString());
    }
    
    // Проверяем на дубликаты имен состояний
    TSet<FName> StateNames;
    for (int32 i = 0; i < States.Num(); ++i)
    {
        if (States[i].StateName.IsNone())
        {
            // Генерируем имя, если пустое
            FString NewName = FString::Printf(TEXT("State_%d"), i);
            States[i].StateName = FName(*NewName);
            UE_LOG(LogEnemyBehaviorAsset, Warning, 
                   TEXT("BehaviorAsset '%s': Generated name '%s' for unnamed state"), 
                   *GetName(), *NewName);
        }
        
        if (StateNames.Contains(States[i].StateName))
        {
            // Исправляем дубликат имени
            FString NewName = FString::Printf(TEXT("%s_%d"), *States[i].StateName.ToString(), i);
            FName OldName = States[i].StateName;
            States[i].StateName = FName(*NewName);
            
            // Также исправляем все переходы, указывающие на старое имя
            for (int32 j = 0; j < States.Num(); ++j)
            {
                for (int32 k = 0; k < States[j].Transitions.Num(); ++k)
                {
                    if (States[j].Transitions[k].TargetState == OldName)
                    {
                        States[j].Transitions[k].TargetState = States[i].StateName;
                    }
                }
            }
            
            // Если начальное состояние совпадает с дубликатом - обновляем его тоже
            if (InitialState == OldName)
            {
                InitialState = States[i].StateName;
            }
            
            UE_LOG(LogEnemyBehaviorAsset, Warning, 
                   TEXT("BehaviorAsset '%s': Renamed duplicate state from '%s' to '%s'"), 
                   *GetName(), *OldName.ToString(), *NewName);
        }
        
        StateNames.Add(States[i].StateName);
    }
    
    // Проверяем, что каждое состояние имеет класс
    for (int32 i = 0; i < States.Num(); ++i)
    {
        if (!States[i].StateClass)
        {
            UE_LOG(LogEnemyBehaviorAsset, Error, 
                   TEXT("BehaviorAsset '%s': State '%s' has no assigned class!"),
                   *GetName(), *States[i].StateName.ToString());
        }
    }
    
    // Дополнительная проверка на валидность переходов
    for (const FStateDescription& State : States)
    {
        for (const FStateTransition& Transition : State.Transitions)
        {
            if (Transition.TargetState.IsNone())
            {
                UE_LOG(LogEnemyBehaviorAsset, Warning, 
                       TEXT("BehaviorAsset '%s': State '%s' has transition with empty target state"),
                       *GetName(), *State.StateName.ToString());
            }
            else
            {
                bool bTargetExists = false;
                for (const FStateDescription& TargetState : States)
                {
                    if (TargetState.StateName == Transition.TargetState)
                    {
                        bTargetExists = true;
                        break;
                    }
                }
                
                if (!bTargetExists)
                {
                    UE_LOG(LogEnemyBehaviorAsset, Warning, 
                           TEXT("BehaviorAsset '%s': State '%s' has transition to non-existent state '%s'"),
                           *GetName(), *State.StateName.ToString(), *Transition.TargetState.ToString());
                }
            }
        }
    }
}

void UEnemyBehaviorDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    
    // Уведомляем об изменении данных для пересоздания FSM в редакторе
    FName PropertyName = PropertyChangedEvent.GetPropertyName();
    FName MemberPropertyName = PropertyChangedEvent.GetMemberPropertyName();
    
    // Если изменилось что-то важное для FSM, отмечаем это
    if (PropertyName == GET_MEMBER_NAME_CHECKED(UEnemyBehaviorDataAsset, States) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(UEnemyBehaviorDataAsset, InitialState))
    {
        UE_LOG(LogEnemyBehaviorAsset, Verbose, TEXT("BehaviorAsset '%s': FSM structure changed"), *GetName());
        
        // Валидация данных при изменении
        // Проверка на валидность InitialState
        if (!InitialState.IsNone())
        {
            bool bInitialStateValid = false;
            for (const FStateDescription& State : States)
            {
                if (State.StateName == InitialState)
                {
                    bInitialStateValid = true;
                    break;
                }
            }
            
            if (!bInitialStateValid)
            {
                UE_LOG(LogEnemyBehaviorAsset, Warning, 
                       TEXT("BehaviorAsset '%s': Initial state '%s' is invalid after property change"), 
                       *GetName(), *InitialState.ToString());
            }
        }
    }
}

// Реализация редакторных утилит
namespace EnemyBehaviorEditorUtility
{
    void VisualizeStateMachine(UEnemyBehaviorDataAsset* BehaviorAsset)
    {
        if (!BehaviorAsset || !GEditor) return;

        UWorld* World = GEditor->PlayWorld ? GEditor->PlayWorld.Get()
                                           : GEditor->GetEditorWorldContext().World();
        if (!World) return;

        for (const FStateDescription& State : BehaviorAsset->States)
        {
            // здесь можно нарисовать узлы / линии переходов
            // FVector Pos = ComputeNodePosition(State.StateName);
            // DrawDebugSphere(World, Pos, 25.f, 12, FColor::White, false, 5.f);
        }
    }
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE