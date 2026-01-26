#include "Core/Enemy/CrowdManagerSubsystem.h"
#include "Core/Enemy/MedComEnemyCharacter.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Async/AsyncWork.h"
#include "Core/Enemy/FSM/MedComEnemyFSMComponent.h"

// Определение категории лога
DEFINE_LOG_CATEGORY_STATIC(LogCrowdManager, Log, All);

// Асинхронная задача для обработки путей
class FProcessPathRequestTask : public FNonAbandonableTask
{
public:
    FProcessPathRequestTask(UNavigationSystemV1* InNavSys, const FVector& InStartPos, const FVector& InEndPos)
        : NavSys(InNavSys), StartPos(InStartPos), EndPos(InEndPos), ResultLocation(FVector::ZeroVector), bHasPath(false)
    {
    }

    void DoWork()
    {
        if (NavSys)
        {
            FNavLocation ProjectedLocation;
            bHasPath = NavSys->ProjectPointToNavigation(
                EndPos,
                ProjectedLocation,
                FVector(300.0f, 300.0f, 100.0f));
                
            if (bHasPath)
            {
                ResultLocation = ProjectedLocation.Location;
            }
        }
    }

    FORCEINLINE TStatId GetStatId() const
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(FProcessPathRequestTask, STATGROUP_ThreadPoolAsyncTasks);
    }
    
    UNavigationSystemV1* NavSys;
    FVector StartPos;
    FVector EndPos;
    FVector ResultLocation;
    bool bHasPath;
};

void UCrowdManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    // Инициализируем аккумулированное дельта-время
    AccumulatedDeltaTime = 0.0f;
    
    #if !UE_BUILD_SHIPPING
    UE_LOG(LogCrowdManager, Log, TEXT("Crowd Manager initialized - using per-frame updates"));
    #endif
}

void UCrowdManagerSubsystem::Deinitialize()
{
    // Очищаем карту агентов
    AgentDataMap.Empty();
    PendingPathRequests.Empty();
    SpatialGrid.Clear();
    
    Super::Deinitialize();
}

void UCrowdManagerSubsystem::Tick(float DeltaTime)
{
    // Вызываем обновление толпы с реальным DeltaTime
    CrowdUpdateTick(DeltaTime);
}

TStatId UCrowdManagerSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UCrowdManagerSubsystem, STATGROUP_Tickables);
}

void UCrowdManagerSubsystem::RegisterAgent(AMedComEnemyCharacter* Agent)
{
    if (!Agent)
        return;
    
    // Создаем запись агента если не существует
    if (!AgentDataMap.Contains(Agent))
    {
        FCrowdAgentData NewData;
        NewData.Agent = Agent;
        NewData.Speed = 300.0f;
        AgentDataMap.Add(Agent, NewData);
        
        #if !UE_BUILD_SHIPPING
        UE_LOG(LogCrowdManager, Verbose, TEXT("Agent registered: %s"), *Agent->GetName());
        #endif
    }
}

void UCrowdManagerSubsystem::UnregisterAgent(AMedComEnemyCharacter* Agent)
{
    if (!Agent)
        return;
    
    // Удаляем агента из системы
    AgentDataMap.Remove(Agent);
    PendingPathRequests.Remove(Agent);
    
    #if !UE_BUILD_SHIPPING
    UE_LOG(LogCrowdManager, Verbose, TEXT("Agent unregistered: %s"), *Agent->GetName());
    #endif
}

void UCrowdManagerSubsystem::RequestAgentMove(AMedComEnemyCharacter* Agent, const FVector& Destination)
{
    if (!Agent)
        return;
    
    // Добавляем или обновляем запись агента
    FCrowdAgentData& AgentData = AgentDataMap.FindOrAdd(Agent);
    AgentData.Agent = Agent;
    AgentData.TargetDestination = Destination;
    AgentData.bHasPathRequest = true;
    
    // Добавляем в список ожидающих запрос пути
    if (!PendingPathRequests.Contains(Agent))
    {
        PendingPathRequests.Add(Agent);
    }
    
    #if !UE_BUILD_SHIPPING
    UE_LOG(LogCrowdManager, Verbose, TEXT("Agent %s requested move to %s"), 
           *Agent->GetName(), *Destination.ToString());
    #endif
}

FVector UCrowdManagerSubsystem::GetAgentVelocity(AMedComEnemyCharacter* Agent)
{
    if (!Agent)
        return FVector::ZeroVector;
    
    // Получаем текущую скорость из записи
    FCrowdAgentData* AgentData = AgentDataMap.Find(Agent);
    if (AgentData)
    {
        return AgentData->CurrentVelocity;
    }
    
    return FVector::ZeroVector;
}

void UCrowdManagerSubsystem::UpdateCrowdMovement()
{
    // Устаревший публичный метод - теперь используем Tick()
    // Оставлен для обратной совместимости
    float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
    CrowdUpdateTick(DeltaTime);
}

void UCrowdManagerSubsystem::CrowdUpdateTick(float DeltaTime)
{
    if (DeltaTime <= 0.0f)
    {
        return;
    }

    // Обрабатываем запросы поиска пути каждый кадр
    BatchProcessPathRequests();
    
    // Обновляем движение агентов
    TArray<TWeakObjectPtr<AMedComEnemyCharacter>> ToRemove;
    for (auto& Pair : AgentDataMap)
    {
        TWeakObjectPtr<AMedComEnemyCharacter> AgentPtr = Pair.Key;
        FCrowdAgentData& Data = Pair.Value;

        if (!AgentPtr.IsValid())
        {
            ToRemove.Add(AgentPtr);
            continue;
        }

        AMedComEnemyCharacter* Agent = AgentPtr.Get();

        // Если бот мёртв или ушёл в Sleep LOD — игнорируем движение
        if (!Agent->IsAlive() || Agent->GetCurrentDetailLevel() == EAIDetailLevel::Sleep)
        {
            Data.bIsMoving = false;
            Data.CurrentVelocity = FVector::ZeroVector;
            continue;
        }

        if (Data.bIsMoving)
        {
            FVector CurrentPos = Agent->GetActorLocation();
            FVector Dir = (Data.TargetDestination - CurrentPos).GetSafeNormal();
            float DistToTarget = FVector::Distance(CurrentPos, Data.TargetDestination);
            Data.DistanceToTarget = DistToTarget;

            if (DistToTarget <= 100.0f)
            {
                // Цель достигнута
                Data.bIsMoving = false;
                Data.CurrentVelocity = FVector::ZeroVector;

                if (UFloatingPawnMovement* FloatComp = Agent->GetFloatingMovementComponent())
                {
                    if (FloatComp->IsComponentTickEnabled())
                    {
                        FloatComp->StopMovementImmediately();
                    }
                }
                
                if (Agent->GetFsmComponent())
                {
                    Agent->GetFsmComponent()->ProcessFSMEvent(EEnemyEvent::ReachedTarget);
                }
                
                #if !UE_BUILD_SHIPPING
                UE_LOG(LogCrowdManager, Verbose, TEXT("Agent %s reached destination"), *Agent->GetName());
                #endif
                
                continue;
            }

            Data.CurrentVelocity = Dir * Data.Speed;

            if (UFloatingPawnMovement* FloatComp = Agent->GetFloatingMovementComponent())
            {
                if (FloatComp->IsComponentTickEnabled())
                {
                    // Используем реальную скорость вместо адаптивной
                    FloatComp->MaxSpeed = Data.Speed;
                    FloatComp->AddInputVector(Dir);

                    // Плавный поворот в направлении движения
                    FRotator TargetRot = Dir.Rotation();
                    Agent->SetActorRotation(FMath::RInterpTo(
                        Agent->GetActorRotation(), TargetRot, DeltaTime, 5.0f));
                }
            }
        }
    }

    // Удаляем невалидных агентов
    for (auto& Rm : ToRemove)
    {
        AgentDataMap.Remove(Rm);
        PendingPathRequests.Remove(Rm);
    }

    // Обновляем сетку и разрешаем коллизии с ограниченной частотой
    AccumulatedDeltaTime += DeltaTime;
    if (AccumulatedDeltaTime >= CollisionCheckInterval)
    {
        BuildSpatialGrid();
        ResolveAgentCollisions(AccumulatedDeltaTime);
        AccumulatedDeltaTime = 0.0f;
    }
}

void UCrowdManagerSubsystem::BatchProcessPathRequests()
{
    if (PendingPathRequests.Num() == 0)
        return;
    
    // Получаем навигационную систему
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!NavSys)
        return;
    
    // Обрабатываем ограниченное количество запросов за кадр
    int32 ProcessedRequests = 0;
    
    for (int32 i = 0; i < PendingPathRequests.Num() && ProcessedRequests < MaxPathRequestsPerFrame; ++i)
    {
        // Проверяем валидность агента
        if (!PendingPathRequests[i].IsValid())
            continue;
        
        AMedComEnemyCharacter* Agent = PendingPathRequests[i].Get();
        FCrowdAgentData* AgentData = AgentDataMap.Find(Agent);
        
        if (!AgentData || !AgentData->bHasPathRequest)
            continue;
        
        // Если агент в Sleep LOD, пропускаем его обработку
        if (Agent->GetCurrentDetailLevel() == EAIDetailLevel::Sleep)
        {
            AgentData->bHasPathRequest = false;
            continue;
        }
        
        // Обрабатываем запрос пути
        FVector CurrentPosition = Agent->GetActorLocation();
        FVector TargetPosition = AgentData->TargetDestination;
        
        // Выполняем проверку на доступность пути
        FNavLocation ProjectedLocation;
        bool bHasPath = NavSys->ProjectPointToNavigation(
            TargetPosition, 
            ProjectedLocation, 
            FVector(300.0f, 300.0f, 100.0f));
        
        if (bHasPath)
        {
            // Устанавливаем корректированную цель
            AgentData->TargetDestination = ProjectedLocation.Location;
            
            // Устанавливаем начальную скорость
            float Speed = 300.0f; // Значение по умолчанию
            
            // Пытаемся получить скорость из компонента движения
            if (UCharacterMovementComponent* CharMoveComp = Agent->GetCharacterMovement())
            {
                if (CharMoveComp->IsComponentTickEnabled())
                {
                    Speed = CharMoveComp->MaxWalkSpeed;
                }
            }
            
            if (UFloatingPawnMovement* FloatComp = Agent->GetFloatingMovementComponent())
            {
                if (FloatComp->IsComponentTickEnabled())
                {
                    Speed = FloatComp->MaxSpeed;
                }
            }
            
            // Обновляем параметры агента
            AgentData->Speed = Speed;
            AgentData->bIsMoving = true;
            AgentData->bHasPathRequest = false;
            
            #if !UE_BUILD_SHIPPING
            UE_LOG(LogCrowdManager, Verbose, TEXT("Path found for agent %s to %s"), 
                   *Agent->GetName(), *ProjectedLocation.Location.ToString());
            #endif
        }
        else
        {
            // Не смогли найти путь, останавливаем агента
            AgentData->bIsMoving = false;
            AgentData->bHasPathRequest = false;
            AgentData->CurrentVelocity = FVector::ZeroVector;
            
            #if !UE_BUILD_SHIPPING
            UE_LOG(LogCrowdManager, Warning, TEXT("No path found for agent %s to %s"), 
                   *Agent->GetName(), *TargetPosition.ToString());
            #endif
        }
        
        // Увеличиваем счетчик обработанных запросов
        ProcessedRequests++;
    }
    
    // Удаляем обработанные запросы
    for (int32 i = PendingPathRequests.Num() - 1; i >= 0; --i)
    {
        if (!PendingPathRequests[i].IsValid())
        {
            PendingPathRequests.RemoveAt(i);
            continue;
        }
        
        FCrowdAgentData* AgentData = AgentDataMap.Find(PendingPathRequests[i].Get());
        if (!AgentData || !AgentData->bHasPathRequest)
        {
            PendingPathRequests.RemoveAt(i);
        }
    }
}

void UCrowdManagerSubsystem::BuildSpatialGrid()
{
    // Очищаем текущую сетку
    SpatialGrid.Clear();
    
    // Добавляем всех движущихся агентов в сетку
    for (auto& Pair : AgentDataMap)
    {
        if (Pair.Key.IsValid())
        {
            AMedComEnemyCharacter* Agent = Pair.Key.Get();
            
            // Пропускаем ботов в режиме Sleep для оптимизации
            if (Agent->GetCurrentDetailLevel() == EAIDetailLevel::Sleep)
                continue;
                
            if (Pair.Value.bIsMoving)
            {
                SpatialGrid.AddAgent(Agent->GetActorLocation(), Agent);
            }
        }
    }
}

void UCrowdManagerSubsystem::ResolveAgentCollisions(float DeltaTime)
{
    // Обрабатываем столкновения для каждого агента с использованием пространственного хеширования
    for (auto& Pair : AgentDataMap)
    {
        if (!Pair.Key.IsValid() || !Pair.Value.bIsMoving)
            continue;
            
        AMedComEnemyCharacter* AgentA = Pair.Key.Get();
        
        // Пропускаем ботов в режиме Sleep
        if (AgentA->GetCurrentDetailLevel() == EAIDetailLevel::Sleep)
            continue;
            
        FCrowdAgentData& DataA = Pair.Value;
        FVector PositionA = AgentA->GetActorLocation();
        
        // Получаем всех агентов в соседних ячейках
        TArray<TWeakObjectPtr<AMedComEnemyCharacter>> Neighbors = 
            SpatialGrid.GetCellAgents(PositionA);
        
        FVector AvoidanceDir = FVector::ZeroVector;
        bool bCollide = false;
        
        for (auto& BPtr : Neighbors)
        {
            if (!BPtr.IsValid() || BPtr == AgentA)
                continue;
                
            AMedComEnemyCharacter* AgentB = BPtr.Get();
            
            // Пропускаем ботов в режиме Sleep при проверке коллизий
            if (AgentB->GetCurrentDetailLevel() == EAIDetailLevel::Sleep)
                continue;
                
            // Проверяем расстояние между агентами
            float Dist = FVector::Distance(PositionA, AgentB->GetActorLocation());
            
            if (Dist < CollisionRadius)
            {
                // Вычисляем направление уклонения
                FVector Dir = (PositionA - AgentB->GetActorLocation()).GetSafeNormal();
                AvoidanceDir += Dir * (1.0f - Dist / CollisionRadius);
                bCollide = true;
            }
        }
        
        // Если есть столкновения, корректируем вектор скорости
        if (bCollide && !AvoidanceDir.IsNearlyZero())
        {
            AvoidanceDir.Normalize();
            
            // Комбинируем текущее направление с направлением уклонения
            FVector CurrentDir = DataA.CurrentVelocity.GetSafeNormal();
            FVector NewDir = (CurrentDir + AvoidanceDir * AvoidanceStrength).GetSafeNormal();
            
            // Обновляем скорость агента
            DataA.CurrentVelocity = NewDir * DataA.Speed;
        }
    }
}