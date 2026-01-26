// MedComPatrolState.cpp
#include "Core/Enemy/FSM/States/MedComPatrolState.h"
#include "Core/Enemy/MedComEnemyCharacter.h"
#include "Core/Enemy/FSM/MedComEnemyFSMComponent.h"
#include "AIController.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogMedComPatrol, Log, All);
FVector UMedComPatrolState::LastPatrolPoint = FVector::ZeroVector;

UMedComPatrolState::UMedComPatrolState()  : TargetLookRotation(), LastPathGoal()
{
    StateTag = FGameplayTag::RequestGameplayTag(TEXT("State.Patrol"));

    // Дефолтные значения (могут быть переопределены в DataAsset)
    PatrolSpeed = 300.f;
    AcceptanceRadius = 100.f;
    bLoopPatrol = true;
    bUseRandomPatrol = false;
    MaxPatrolDistance = 1000.f;
    RepathDistance = 100.f;
    PatrolRotationRate = 300.f;
    bLookAroundWhilePatrolling = false;
    PatrolLookAroundInterval = 3.0f;
    PatrolLookAroundDuration = 1.5f;
    NumPatrolPoints = 4;
    
    CurrentPointIndex = 0;
    PatrolDirection = 1;
    LastMoveRequestTime = 0.f;
    LastLookAroundTime = 0.f;
    bIsLookingAround = false;
}

void UMedComPatrolState::OnEnter(AMedComEnemyCharacter* Owner)
{
    Super::OnEnter(Owner);
    if (!Owner) return;

    // Извлекаем параметры из DataAsset
    PatrolSpeed = GetStateParamFloat(TEXT("PatrolSpeed"), PatrolSpeed);
    AcceptanceRadius = GetStateParamFloat(TEXT("AcceptanceRadius"), AcceptanceRadius);
    bLoopPatrol = GetStateParamBool(TEXT("LoopPatrol"), bLoopPatrol);
    bUseRandomPatrol = GetStateParamBool(TEXT("UseRandomPatrol"), bUseRandomPatrol);
    MaxPatrolDistance = GetStateParamFloat(TEXT("MaxPatrolDistance"), MaxPatrolDistance);
    RepathDistance = GetStateParamFloat(TEXT("RepathDistance"), RepathDistance);
    PatrolRotationRate = GetStateParamFloat(TEXT("PatrolRotationRate"), PatrolRotationRate);
    bLookAroundWhilePatrolling = GetStateParamBool(TEXT("LookAroundWhilePatrolling"), bLookAroundWhilePatrolling);
    PatrolLookAroundInterval = GetStateParamFloat(TEXT("PatrolLookAroundInterval"), PatrolLookAroundInterval);
    PatrolLookAroundDuration = GetStateParamFloat(TEXT("PatrolLookAroundDuration"), PatrolLookAroundDuration);
    NumPatrolPoints = FMath::RoundToInt(GetStateParamFloat(TEXT("NumPatrolPoints"), 4.0f));

    // Настраиваем движение
    SetupMovementComponent(Owner);

    // Сохраняем контроллер
    CachedController = Cast<AAIController>(Owner->GetController());
    if (CachedController.IsValid())
    {
        // Привязываем делегат завершения движения
        CachedController->ReceiveMoveCompleted.RemoveDynamic(this, &UMedComPatrolState::OnMoveCompleted);
        CachedController->ReceiveMoveCompleted.AddDynamic(this, &UMedComPatrolState::OnMoveCompleted);
    }

    // Генерируем точки патрулирования
    PatrolPoints = GeneratePatrolPoints(Owner);
    
    // Сбрасываем индекс точки и начинаем патрулирование
    CurrentPointIndex = 0;
    PatrolDirection = 1;
    LastMoveRequestTime = 0.f;
    LastLookAroundTime = 0.f;
    bIsLookingAround = false;
    
    // Начинаем движение к первой точке
    MoveToNextPoint(Owner);
    
    UE_LOG(LogMedComPatrol, Log, TEXT("[%s] Started patrol with %d points"), 
           *Owner->GetName(), PatrolPoints.Num());
}

void UMedComPatrolState::OnExit(AMedComEnemyCharacter* Owner)
{
    // Отвязываем делегат и останавливаем движение
    if (CachedController.IsValid())
    {
        CachedController->ReceiveMoveCompleted.RemoveDynamic(this, &UMedComPatrolState::OnMoveCompleted);
        CachedController->StopMovement();
    }
    
    // Восстанавливаем настройки ориентации, если были изменены
    if (Owner)
    {
        Owner->bUseControllerRotationYaw = false;
        if (UCharacterMovementComponent* MoveComp = Owner->GetCharacterMovement())
        {
            MoveComp->bOrientRotationToMovement = true;
        }
    }
    
    Super::OnExit(Owner);
}

void UMedComPatrolState::ProcessTick(AMedComEnemyCharacter* Owner, float DeltaTime)
{
    Super::ProcessTick(Owner, DeltaTime);
    
    if (!Owner || !CachedController.IsValid() || PatrolPoints.Num() == 0)
        return;

    // Если нужно обновить путь - делаем это
    if (NeedsPathRefresh(Owner))
    {
        MoveToNextPoint(Owner);
    }
    
    // Обрабатываем "осмотр по сторонам" при патрулировании
    if (bLookAroundWhilePatrolling)
    {
        LookAround(Owner, DeltaTime);
    }
    
    // Отладочный вывод
    #if !UE_BUILD_SHIPPING
    UPathFollowingComponent* Path = CachedController->GetPathFollowingComponent();
    const FVector Vel = Owner->GetVelocity();
    
    UE_LOG(LogMedComPatrol, Verbose,
           TEXT("[Patrol] Point %d/%d Vel=%.0f PathSt=%s"),
           CurrentPointIndex + 1, PatrolPoints.Num(),
           Vel.Size(),
           Path ? *UEnum::GetValueAsString(Path->GetStatus()) : TEXT("NULL"));
    
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            /*Key*/2, /*Time*/0.f, FColor::Green,
            FString::Printf(TEXT("[Patrol] Point %d/%d v%.0f"), 
                           CurrentPointIndex + 1, PatrolPoints.Num(), Vel.Size()));
    }
    #endif
}

void UMedComPatrolState::OnEvent(AMedComEnemyCharacter* Owner, EEnemyEvent Event, AActor* Instigator)
{
    Super::OnEvent(Owner, Event, Instigator);
    
    if (Event == EEnemyEvent::PlayerSeen && Instigator)
    {
        // Поворачиваемся к игроку
        if (CachedController.IsValid())
        {
            CachedController->SetFocus(Instigator);
            UE_LOG(LogMedComPatrol, Log, TEXT("[%s] Spotted player: %s"), 
                   *Owner->GetName(), *Instigator->GetName());
        }
    }
}

TArray<FVector> UMedComPatrolState::GeneratePatrolPoints(AMedComEnemyCharacter* Owner)
{
    TArray<FVector> Result;
    if (!Owner || !Owner->GetWorld())
        return Result;
    
    const FVector InitialPos = Owner->GetActorLocation();
    Result.Add(InitialPos); // Первая точка - текущая позиция
    
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Owner->GetWorld());
    
    if (!NavSys)
    {
        // Без навигационной системы используем простой паттерн
        const float PatrolRadius = FMath::Min(MaxPatrolDistance * 0.5f, 500.0f);
        
        // Создаем квадратный маршрут
        FVector PointsToCheck[] = {
            InitialPos + FVector(PatrolRadius, PatrolRadius, 0),
            InitialPos + FVector(-PatrolRadius, PatrolRadius, 0),
            InitialPos + FVector(-PatrolRadius, -PatrolRadius, 0),
            InitialPos + FVector(PatrolRadius, -PatrolRadius, 0)
        };
        
        for (const FVector& Point : PointsToCheck)
        {
            Result.Add(Point);
        }
    }
    else if (bUseRandomPatrol)
    {
        // Генерируем случайные точки с проверкой на досягаемость
        for (int32 i = 0; i < FMath::Min(NumPatrolPoints, 12); ++i)
        {
            FNavLocation NavLocation;
            if (NavSys->GetRandomReachablePointInRadius(
                InitialPos,
                MaxPatrolDistance,
                NavLocation))
            {
                Result.Add(NavLocation.Location);
            }
        }
    }
    else
    {
        // Создаем маршрут с проекцией на NavMesh
        const float PatrolRadius = FMath::Min(MaxPatrolDistance * 0.5f, 500.0f);
        const int32 NumPoints = FMath::Min(NumPatrolPoints, 12);
        
        // Равномерно распределяем точки по кругу
        for (int32 i = 0; i < NumPoints; ++i)
        {
            const float Angle = 2.0f * PI * static_cast<float>(i) / static_cast<float>(NumPoints);
            const FVector Offset = FVector(
                FMath::Cos(Angle) * PatrolRadius,
                FMath::Sin(Angle) * PatrolRadius,
                0.0f
            );
            const FVector PointLocation = InitialPos + Offset;
            
            FNavLocation ProjectedLocation;
            if (NavSys->ProjectPointToNavigation(
                PointLocation, 
                ProjectedLocation, 
                FVector(300.0f, 300.0f, 100.0f)))
            {
                Result.Add(ProjectedLocation.Location);
            }
            else
            {
                // Если проекция не удалась, используем оригинальную точку
                Result.Add(PointLocation);
            }
        }
    }
    
    // Если точек недостаточно, добавляем хотя бы ещё одну
    if (Result.Num() < 2)
    {
        Result.Add(InitialPos + Owner->GetActorForwardVector() * 300.0f);
    }
    
    return Result;
}

void UMedComPatrolState::MoveToNextPoint(AMedComEnemyCharacter* Owner)
{
    if (!Owner || !CachedController.IsValid() || PatrolPoints.Num() == 0)
        return;
    
    // Если последняя точка была достигнута, обрабатываем завершение маршрута
    if (CurrentPointIndex >= PatrolPoints.Num() - 1 && PatrolDirection > 0)
    {
        if (bLoopPatrol)
        {
            // Зацикливаем маршрут
            CurrentPointIndex = 0;
        }
        else
        {
            // Меняем направление
            PatrolDirection = -1;
            CurrentPointIndex = PatrolPoints.Num() - 2;
            if (CurrentPointIndex < 0) CurrentPointIndex = 0;
        }
    }
    else if (CurrentPointIndex <= 0 && PatrolDirection < 0)
    {
        if (bLoopPatrol)
        {
            // Зацикливаем маршрут в обратном направлении
            CurrentPointIndex = PatrolPoints.Num() - 1;
        }
        else
        {
            // Меняем направление
            PatrolDirection = 1;
            CurrentPointIndex = 1;
            if (CurrentPointIndex >= PatrolPoints.Num()) CurrentPointIndex = PatrolPoints.Num() - 1;
        }
    }
    else
    {
        // Переходим к следующей точке
        CurrentPointIndex += PatrolDirection;
    }
    
    // Проверяем валидность индекса
    if (CurrentPointIndex < 0 || CurrentPointIndex >= PatrolPoints.Num())
    {
        CurrentPointIndex = 0;
    }
    
    // Получаем целевую точку
    FVector TargetPoint = PatrolPoints[CurrentPointIndex];
    
    // Останавливаем предыдущее движение
    CachedController->StopMovement();
    
    // Запрашиваем движение к точке
    EPathFollowingRequestResult::Type MoveResult = CachedController->MoveToLocation(
        TargetPoint,
        AcceptanceRadius,
        /* bStopOnOverlap */ true,
        /* bUsePathfinding */ true,
        /* bProjectDestinationToNavigation */ true,
        /* bCanStrafe */ false
    );
    
    LastPathGoal = TargetPoint;
    LastMoveRequestTime = Owner->GetWorld()->GetTimeSeconds();
    
    bool bSuccess = (MoveResult == EPathFollowingRequestResult::RequestSuccessful || 
                    MoveResult == EPathFollowingRequestResult::AlreadyAtGoal);
    
    if (bSuccess)
    {
        UE_LOG(LogMedComPatrol, Verbose, TEXT("[%s] Moving to patrol point %d/%d"), 
               *Owner->GetName(), CurrentPointIndex + 1, PatrolPoints.Num());
    }
    else
    {
        UE_LOG(LogMedComPatrol, Warning, TEXT("[%s] Failed to move to point %d/%d"), 
               *Owner->GetName(), CurrentPointIndex + 1, PatrolPoints.Num());
    }
    
    #if !UE_BUILD_SHIPPING
    // Отладочная визуализация маршрута
    DrawDebugSphere(Owner->GetWorld(), TargetPoint, 20.0f, 8, FColor::Blue, false, 1.0f);
    #endif
}

void UMedComPatrolState::OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
    // Получаем владельца из FSM
    AMedComEnemyCharacter* Owner = FSMComponent ? FSMComponent->GetOwnerEnemy() : nullptr;
    if (!Owner || !IsValid(Owner))
    {
        return;
    }
    
    // Сохраняем последнюю достигнутую точку
    if (Result == EPathFollowingResult::Success && Owner)
    {
        SetLastPatrolPoint(Owner->GetActorLocation());
    }
    
    UE_LOG(LogMedComPatrol, Log, TEXT("[%s] Move completed with result: %d"), 
           *Owner->GetName(), static_cast<int32>(Result));
    
    switch (Result)
    {
    case EPathFollowingResult::Success:
        // Достигли точки, переходим к следующей
        MoveToNextPoint(Owner);
        break;
            
    case EPathFollowingResult::Blocked:
    case EPathFollowingResult::Invalid:
    case EPathFollowingResult::OffPath:
    case EPathFollowingResult::Aborted:
        // В случае проблем, пытаемся перестроить путь с задержкой
        if (Owner->GetWorld() && IsValid(this))
        {
            TWeakObjectPtr<UMedComPatrolState> WeakThis(this);
            TWeakObjectPtr<AMedComEnemyCharacter> WeakOwner(Owner);
            
            Owner->GetWorld()->GetTimerManager().SetTimer(
                RetryMoveTimerHandle, 
                [WeakThis, WeakOwner]() { 
                    if (WeakThis.IsValid() && WeakOwner.IsValid()) 
                        WeakThis->MoveToNextPoint(WeakOwner.Get()); 
                },
                0.5f,
                false
            );
        }
        break;
        
    default:
        // Обработка остальных случаев перечисления
        UE_LOG(LogMedComPatrol, Warning, TEXT("[%s] Unhandled path following result: %d"), 
               *Owner->GetName(), static_cast<int32>(Result));
        // По умолчанию также пробуем перейти к следующей точке
        if (Owner->GetWorld() && IsValid(this))
        {
            TWeakObjectPtr<UMedComPatrolState> WeakThis(this);
            TWeakObjectPtr<AMedComEnemyCharacter> WeakOwner(Owner);
            
            Owner->GetWorld()->GetTimerManager().SetTimer(
                RetryMoveTimerHandle, 
                [WeakThis, WeakOwner]() { 
                    if (WeakThis.IsValid() && WeakOwner.IsValid()) 
                        WeakThis->MoveToNextPoint(WeakOwner.Get()); 
                },
                1.0f,  // Больший таймаут для неизвестных ошибок
                false
            );
        }
        break;
    }
    
    // Проверяем, не пора ли сообщить о завершении маршрута
    if (bLoopPatrol && CurrentPointIndex == 0 && PatrolDirection > 0)
    {
        if (FSMComponent && IsValid(FSMComponent))
        {
            UE_LOG(LogMedComPatrol, Log, TEXT("[%s] Patrol route completed"), *Owner->GetName());
            FSMComponent->ProcessFSMEvent(EEnemyEvent::PatrolComplete);
        }
    }
}

bool UMedComPatrolState::NeedsPathRefresh(const AMedComEnemyCharacter* Owner) const
{
    if (!Owner || !CachedController.IsValid())
        return false;
    
    const UPathFollowingComponent* PFC = CachedController->GetPathFollowingComponent();
    if (!PFC) return false;
    
    // Путь завершился или заблокирован
    if (PFC->GetStatus() == EPathFollowingStatus::Idle ||
        PFC->GetStatus() == EPathFollowingStatus::Waiting ||
        PFC->DidMoveReachGoal())
    {
        return true;
    }
    
    return false;
}

void UMedComPatrolState::SetupMovementComponent(AMedComEnemyCharacter* Owner) const
{
    if (!Owner)
        return;
    
    UCharacterMovementComponent* MoveComp = Owner->GetCharacterMovement();
    if (!MoveComp)
        return;
    
    // Активируем компонент
    MoveComp->SetComponentTickEnabled(true);
    
    // Устанавливаем режим движения
    MoveComp->SetMovementMode(MOVE_Walking);
    
    // Скорость патрулирования
    MoveComp->MaxWalkSpeed = PatrolSpeed;
    
    // Поворот в направлении движения
    MoveComp->bOrientRotationToMovement = true;
    MoveComp->RotationRate = FRotator(0.f, PatrolRotationRate, 0.f);
    
    // Отключаем повороты за контроллером
    Owner->bUseControllerRotationYaw = false;
}

void UMedComPatrolState::LookAround(AMedComEnemyCharacter* Owner, float DeltaTime)
{
    if (!Owner || !bLookAroundWhilePatrolling)
        return;
    
    const float CurrentTime = Owner->GetWorld()->GetTimeSeconds();
    
    // Если еще не пора "смотреть по сторонам" и не в процессе осмотра
    if (CurrentTime - LastLookAroundTime < PatrolLookAroundInterval && !bIsLookingAround)
        return;
    
    if (!bIsLookingAround)
    {
        // Начинаем новый взгляд
        bIsLookingAround = true;
        LastLookAroundTime = CurrentTime;
        
        // Генерируем случайное направление для обзора (+/- 80 градусов)
        const float RandomYaw = FMath::RandRange(-80.0f, 80.0f);
        TargetLookRotation = Owner->GetActorRotation();
        TargetLookRotation.Yaw += RandomYaw;
        
        // Сбрасываем ориентацию по движению временно
        if (UCharacterMovementComponent* MoveComp = Owner->GetCharacterMovement())
        {
            MoveComp->bOrientRotationToMovement = false;
        }
        
        // Включаем ориентацию головы/контроллера
        Owner->bUseControllerRotationYaw = true;
        
        // Останавливаем движение на время осмотра
        if (CachedController.IsValid())
        {
            CachedController->StopMovement();
        }
    }
    else
    {
        // Поворачиваем голову к целевому направлению
        FRotator CurrentRot = Owner->GetActorRotation();
        FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetLookRotation, DeltaTime, 2.0f);
        
        // Применяем только поворот по Yaw
        if (CachedController.IsValid())
        {
            CachedController->SetControlRotation(NewRot);
        }
        
        // Проверяем, завершили ли поворот (с точностью до 5 градусов)
        if (FMath::Abs(FMath::FindDeltaAngleDegrees(NewRot.Yaw, TargetLookRotation.Yaw)) < 5.0f)
        {
            // Задержка в новом направлении
            if (CurrentTime - LastLookAroundTime > PatrolLookAroundDuration)
            {
                // Завершаем обзор, возвращаемся к патрулированию
                bIsLookingAround = false;
                LastLookAroundTime = CurrentTime;
                
                // Восстанавливаем ориентацию по движению
                if (UCharacterMovementComponent* MoveComp = Owner->GetCharacterMovement())
                {
                    MoveComp->bOrientRotationToMovement = true;
                }
                
                // Отключаем ориентацию головы/контроллера
                Owner->bUseControllerRotationYaw = false;
                
                // Продолжаем движение к следующей точке
                MoveToNextPoint(Owner);
            }
        }
    }
}