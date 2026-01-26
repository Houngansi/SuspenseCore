// MedComAsyncTaskRepositionCalculation.cpp
#include "Core/Enemy/StatesAI/StateTasks/MedComAsyncTaskRepositionCalculation.h"
#include "Core/Enemy/MedComEnemyCharacter.h"
#include "Core/Enemy/Components/MedComAIMovementComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Async/TaskGraphInterfaces.h"
#include "DrawDebugHelpers.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRepositioning, Log, All);

// Константы для преобразования единиц измерения
#define CM_TO_M_CONVERSION 100.0f
#define M_TO_CM_CONVERSION 100.0f
#define DEFAULT_NAV_EXTENT 500.0f
#define EYE_HEIGHT_OFFSET 60.0f

UMedComAsyncTaskRepositionCalculation* UMedComAsyncTaskRepositionCalculation::CalculateRepositionTargetAsync(
    AMedComEnemyCharacter* Enemy, 
    ACharacter* Target, 
    float CurrentDistance, 
    FGameplayTag FireMode,
    bool DebugMode)
{
    UMedComAsyncTaskRepositionCalculation* Task = NewObject<UMedComAsyncTaskRepositionCalculation>();
    Task->Initialize(Enemy, Target, CurrentDistance, FireMode, DebugMode);
    return Task;
}

void UMedComAsyncTaskRepositionCalculation::Initialize(
    AMedComEnemyCharacter* InEnemy,
    ACharacter* InTarget,
    float InCurrentDistance,
    FGameplayTag InFireMode,
    bool InDebugMode)
{
    EnemyCharacter = InEnemy;
    TargetCharacter = InTarget;
    CurrentDistance = InCurrentDistance;
    CurrentFireMode = InFireMode;
    bDebugMode = InDebugMode;

    // Предварительно получаем компонент и параметры
    CachedAIMovementComponent = GetAIMovementComponent();

    // Заполняем параметры расчета из AI Movement Component или используем значения по умолчанию
    CalculationParams.OptimalDistance = 10.0f;
    CalculationParams.DistanceVariation = 2.0f;
    CalculationParams.MinTargetDistance = 1.0f;
    CalculationParams.MaxTargetDistance = 30.0f;
    CalculationParams.MinEnemyDistance = 250.0f;
    CalculationParams.RepositionAttemptsCount = 8;
    CalculationParams.CoverDetectionRadius = 200.0f;
    CalculationParams.bPreferCoverPositions = true;

    // Если есть компонент AI Movement, берем параметры из него
    if (CachedAIMovementComponent)
    {
        const auto& Params = CachedAIMovementComponent->RepositioningParams;
        CalculationParams.MinTargetDistance = Params.MinTargetDistance;
        CalculationParams.MaxTargetDistance = Params.MaxTargetDistance;
        CalculationParams.MinEnemyDistance = Params.MinEnemyDistance;
    }

    // Настраиваем оптимальную дистанцию в зависимости от режима огня
    if (CachedAIMovementComponent)
    {
        if (CurrentFireMode == FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single"))
        {
            CalculationParams.OptimalDistance = 20.0f; // Дальняя дистанция для одиночных выстрелов
            CalculationParams.DistanceVariation = 3.0f;
        }
        else if (CurrentFireMode == FGameplayTag::RequestGameplayTag("Weapon.FireMode.Burst"))
        {
            CalculationParams.OptimalDistance = CachedAIMovementComponent->RepositioningParams.BurstFireDistance;
            CalculationParams.DistanceVariation = 2.5f;
        }
        else if (CurrentFireMode == FGameplayTag::RequestGameplayTag("Weapon.FireMode.Auto"))
        {
            CalculationParams.OptimalDistance = CachedAIMovementComponent->RepositioningParams.AutoFireDistance;
            CalculationParams.DistanceVariation = 2.0f;
        }
    }
    else
    {
        // Если нет компонента, используем базовые значения
        if (CurrentFireMode == FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single"))
        {
            CalculationParams.OptimalDistance = 20.0f;
            CalculationParams.DistanceVariation = 3.0f;
        }
        else if (CurrentFireMode == FGameplayTag::RequestGameplayTag("Weapon.FireMode.Burst"))
        {
            CalculationParams.OptimalDistance = 12.0f;
            CalculationParams.DistanceVariation = 2.5f;
        }
        else if (CurrentFireMode == FGameplayTag::RequestGameplayTag("Weapon.FireMode.Auto"))
        {
            CalculationParams.OptimalDistance = 8.0f;
            CalculationParams.DistanceVariation = 2.0f;
        }
    }
    
    // НОВОЕ: Собираем данные Game Thread здесь при инициализации
    CollectGameThreadData();
}

// НОВЫЙ МЕТОД: Собираем все данные, для которых нужен Game Thread
void UMedComAsyncTaskRepositionCalculation::CollectGameThreadData()
{
    // Собираем позиции всех ботов здесь, в Game Thread
    if (EnemyCharacter && EnemyCharacter->GetWorld())
    {
        // Очищаем предыдущие данные
        EnemyPositions.Reset();
        
        // Получаем всех ботов в Game Thread (безопасно)
        TArray<AActor*> AllEnemies;
        UGameplayStatics::GetAllActorsOfClass(EnemyCharacter->GetWorld(), AMedComEnemyCharacter::StaticClass(), AllEnemies);
        
        // Сохраняем только позиции, которые безопасно использовать в любом потоке
        for (AActor* Actor : AllEnemies)
        {
            AMedComEnemyCharacter* OtherEnemy = Cast<AMedComEnemyCharacter>(Actor);
            if (OtherEnemy && OtherEnemy != EnemyCharacter)
            {
                EnemyPositions.Add(OtherEnemy->GetActorLocation());
            }
        }
        
        UE_LOG(LogAIRepositioning, Verbose, TEXT("CollectGameThreadData: Cached %d enemy positions"), EnemyPositions.Num());
    }
}

void UMedComAsyncTaskRepositionCalculation::Activate()
{
    if (!EnemyCharacter || !TargetCharacter)
    {
        FRepositionCalculationResult FailResult;
        FailResult.bSuccess = false;
        FailResult.DebugInfo = TEXT("Invalid enemy or target character");
        OnCalculationFailed.Broadcast(FailResult);
        return;
    }

    // Запускаем расчет в отдельной асинхронной задаче
    // Используем MediumPriority, чтобы не перегружать систему
    FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady(
        [this]()
        {
            ExecuteCalculation();
        },
        TStatId(),
        nullptr,
        ENamedThreads::AnyBackgroundThreadNormalTask
    );

    // Зависимость для callback'а после завершения расчета (выполняется в Game Thread)
    FTaskGraphInterface::Get().WaitUntilTaskCompletes(Task, ENamedThreads::GameThread);
}

void UMedComAsyncTaskRepositionCalculation::ExplicitCancel()
{
    FRepositionCalculationResult CancelResult;
    CancelResult.bSuccess = false;
    CancelResult.DebugInfo = TEXT("Task cancelled");
    FinishCalculation(CancelResult);
}

void UMedComAsyncTaskRepositionCalculation::ExecuteCalculation()
{
    if (!EnemyCharacter || !TargetCharacter || !EnemyCharacter->GetWorld())
    {
        FRepositionCalculationResult FailResult;
        FailResult.bSuccess = false;
        FailResult.DebugInfo = TEXT("Invalid parameters during calculation");
        FinishCalculation(FailResult);
        return;
    }

    // Базовые расчеты
    FVector EnemyLocation = EnemyCharacter->GetActorLocation();
    FVector PlayerLocation = TargetCharacter->GetActorLocation();
    FVector DirectionToPlayer = (PlayerLocation - EnemyLocation).GetSafeNormal();
    DirectionToPlayer.Z = 0.0f;

    // Добавляем случайное отклонение для вариативности
    float OptimalDistance = CalculationParams.OptimalDistance + FMath::RandRange(-CalculationParams.DistanceVariation, CalculationParams.DistanceVariation);

    // Ограничиваем количество попыток для экономии ресурсов
    int32 MaxAttempts = FMath::Min(CalculationParams.RepositionAttemptsCount, 8); // Увеличено с 4 до 8 для лучших результатов
    TArray<FVector> PotentialPositions;
    TArray<float> PositionScores;

    // Система для поиска угловых секторов
    TArray<float> AngleSectors;
    const int32 NumSectors = 8;
    for (int32 i = 0; i < NumSectors; ++i)
    {
        AngleSectors.Add(i * 360.0f / NumSectors);
    }

    // Перемешиваем секторы для случайности
    for (int32 i = 0; i < AngleSectors.Num(); ++i)
    {
        int32 SwapIdx = FMath::RandRange(i, AngleSectors.Num() - 1);
        if (i != SwapIdx)
        {
            float Temp = AngleSectors[i];
            AngleSectors[i] = AngleSectors[SwapIdx];
            AngleSectors[SwapIdx] = Temp;
        }
    }

    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(EnemyCharacter->GetWorld());

    // Общие счетчики для отладки
    int32 ValidPositionsFound = 0;
    int32 PositionsWithLoS = 0;
    int32 PositionsNearCover = 0;

    FString DebugInfo = FString::Printf(TEXT("Calculating with OptimalDistance=%.1f, FireMode=%s, Attempts=%d"),
        OptimalDistance, *CurrentFireMode.ToString(), MaxAttempts);

    for (int32 i = 0; i < MaxAttempts; ++i)
    {
        // Используем секторы для более равномерного распределения поиска
        int32 SectorIndex = i % AngleSectors.Num();

        // Базовый угол из сектора + небольшое случайное отклонение
        float BaseAngle = AngleSectors[SectorIndex];
        float RandomOffset = FMath::RandRange(-15.0f, 15.0f);
        float RandomAngle = FMath::DegreesToRadians(BaseAngle + RandomOffset);

        // Вычисляем направление и потенциальную позицию
        FVector Direction(FMath::Cos(RandomAngle), FMath::Sin(RandomAngle), 0.0f);

        // Добавляем вертикальное отклонение для многоуровневых сред (уменьшено количество, чтобы не тратить расчеты)
        if (FMath::RandBool() && i % 4 == 0)
        {
            float HeightVariation = FMath::RandRange(-100.0f, 100.0f);
            Direction.Z = HeightVariation / (OptimalDistance * M_TO_CM_CONVERSION);
        }

        // Вычисляем позицию с некоторой вариацией дистанции
        float DistanceMod = FMath::RandRange(0.8f, 1.2f);
        FVector PotentialPos = PlayerLocation + Direction * OptimalDistance * M_TO_CM_CONVERSION * DistanceMod;

        // Выполняем навигационный запрос для проверки доступности точки (только если есть навигационная система)
        if (NavSys)
        {
            FNavLocation NavLocation;
            if (NavSys->ProjectPointToNavigation(PotentialPos, NavLocation, FVector(DEFAULT_NAV_EXTENT, DEFAULT_NAV_EXTENT, DEFAULT_NAV_EXTENT)))
            {
                PotentialPos = ProjectPointToNavigation(NavSys, NavLocation.Location, EnemyLocation);
                ValidPositionsFound++;

                // Упрощенная проверка линии видимости
                bool bHasLineOfSight = CanSeeTargetFromPosition(TargetCharacter, PotentialPos);
                if (bHasLineOfSight)
                {
                    PositionsWithLoS++;
                }

                float Score = 0.0f;
                if (bHasLineOfSight)
                {
                    Score += 50.0f;
                }

                // Штраф за отклонение от оптимальной дистанции
                float DistanceDiff = FMath::Abs(FVector::Distance(PotentialPos, PlayerLocation) / CM_TO_M_CONVERSION - OptimalDistance);
                Score -= DistanceDiff * 5.0f;

                // Бонус за укрытие рядом (если опция включена)
                if (CalculationParams.bPreferCoverPositions && IsPositionNearCover(PotentialPos, CalculationParams.CoverDetectionRadius))
                {
                    Score += 30.0f;
                    PositionsNearCover++;
                    if (bHasLineOfSight)
                    {
                        Score += 20.0f;
                    }
                }

                // Бонус за позицию, противоположную текущему направлению
                FVector CurrentDirection = (EnemyLocation - PlayerLocation).GetSafeNormal();
                FVector NewDirection = (PotentialPos - PlayerLocation).GetSafeNormal();
                float DotProduct = FVector::DotProduct(CurrentDirection, NewDirection);
                if (DotProduct < 0.0f)
                {
                    Score += 15.0f * FMath::Abs(DotProduct);
                }

                PotentialPositions.Add(PotentialPos);
                PositionScores.Add(Score);
            }
        }
        else
        {
            // Fallback если нет навигационной системы
            PotentialPos = PotentialPos + FVector(0, 0, 50.0f);
            PotentialPositions.Add(PotentialPos);
            PositionScores.Add(0.0f); // Нейтральная оценка
        }
    }

    // Дополняем отладочную информацию
    DebugInfo += FString::Printf(TEXT(", Valid=%d, WithLoS=%d, NearCover=%d"),
        ValidPositionsFound, PositionsWithLoS, PositionsNearCover);

    // Выбираем лучшую позицию из найденных
    FRepositionCalculationResult Result;
    if (PotentialPositions.Num() > 0)
    {
        int32 BestIndex = 0;
        float BestScore = PositionScores[0];

        for (int32 i = 1; i < PositionScores.Num(); ++i)
        {
            if (PositionScores[i] > BestScore)
            {
                BestScore = PositionScores[i];
                BestIndex = i;
            }
        }

        Result.TargetPosition = PotentialPositions[BestIndex];
        Result.bSuccess = true;
        Result.DebugInfo = DebugInfo + FString::Printf(TEXT(", Selected pos with score %.1f"), BestScore);

        // Дополнительная корректировка для избегания других врагов (используя предварительно собранные данные)
        Result.TargetPosition = FindPositionAwayFromOtherEnemies(Result.TargetPosition, CalculationParams.MinEnemyDistance);
    }
    else
    {
        // Если не нашли подходящих позиций - случайное направление на оптимальной дистанции
        FVector RandomDir = FMath::VRand();
        RandomDir.Z = 0.0f;
        RandomDir.Normalize();

        Result.TargetPosition = PlayerLocation + RandomDir * OptimalDistance * M_TO_CM_CONVERSION;
        Result.bSuccess = true;
        Result.DebugInfo = DebugInfo + TEXT(", Using fallback random position");

        // Дополнительная корректировка для избегания других врагов (используя предварительно собранные данные)
        Result.TargetPosition = FindPositionAwayFromOtherEnemies(Result.TargetPosition, CalculationParams.MinEnemyDistance);
    }

    // Завершаем задачу с полученным результатом
    FinishCalculation(Result);
}

void UMedComAsyncTaskRepositionCalculation::FinishCalculation(const FRepositionCalculationResult& Result)
{
    // Используем GameThread для вызова делегатов, так как они могут содержать UObject ссылки
    FFunctionGraphTask::CreateAndDispatchWhenReady(
        [this, Result]()
        {
            if (Result.bSuccess)
            {
                OnCalculationComplete.Broadcast(Result);
            }
            else
            {
                OnCalculationFailed.Broadcast(Result);
            }
            SetReadyToDestroy();
        },
        TStatId(),
        nullptr,
        ENamedThreads::GameThread
    );
}

UMedComAIMovementComponent* UMedComAsyncTaskRepositionCalculation::GetAIMovementComponent() const
{
    if (!EnemyCharacter)
    {
        return nullptr;
    }
    return EnemyCharacter->FindComponentByClass<UMedComAIMovementComponent>();
}

bool UMedComAsyncTaskRepositionCalculation::IsPositionNearCover(const FVector& Position, float CoverRadius)
{
    if (!EnemyCharacter || !EnemyCharacter->GetWorld())
    {
        return false;
    }

    // Оптимизированная версия - проверяем только 4 основных направления
    TArray<FVector> Directions = {
        FVector(1, 0, 0),
        FVector(-1, 0, 0),
        FVector(0, 1, 0),
        FVector(0, -1, 0)
    };

    for (const FVector& Dir : Directions)
    {
        FHitResult HitResult;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(EnemyCharacter);

        if (EnemyCharacter->GetWorld()->LineTraceSingleByChannel(HitResult,
            Position + FVector(0, 0, 60.0f),
            Position + FVector(0, 0, 60.0f) + Dir * CoverRadius,
            ECC_Visibility, Params))
        {
            if (HitResult.GetActor() && !HitResult.GetActor()->IsA<APawn>())
            {
                FVector Extent = HitResult.GetActor()->GetComponentsBoundingBox().GetExtent();
                if (Extent.Z > 80.0f && Extent.Size() > 100.0f)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

bool UMedComAsyncTaskRepositionCalculation::CanSeeTargetFromPosition(const ACharacter* Target, const FVector& Position)
{
    if (!EnemyCharacter || !Target || !EnemyCharacter->GetWorld())
    {
        return false;
    }

    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(EnemyCharacter);

    FVector EyePosition = Position + FVector(0, 0, EYE_HEIGHT_OFFSET);
    FVector TargetPosition = Target->GetActorLocation() + FVector(0, 0, EYE_HEIGHT_OFFSET);

    bool bHit = EnemyCharacter->GetWorld()->LineTraceSingleByChannel(HitResult, EyePosition, TargetPosition, ECC_Visibility, Params);
    return (!bHit || HitResult.GetActor() == Target);
}

FVector UMedComAsyncTaskRepositionCalculation::ProjectPointToNavigation(UNavigationSystemV1* NavSys, const FVector& Point, const FVector& OwnerLocation)
{
    if (!NavSys)
    {
        return Point;
    }

    FNavLocation NavLoc;
    FVector Extent(DEFAULT_NAV_EXTENT, DEFAULT_NAV_EXTENT, DEFAULT_NAV_EXTENT);
    if (NavSys->ProjectPointToNavigation(Point, NavLoc, Extent))
    {
        // Если высота полученной точки ниже, чем у владельца (с небольшим отступом), поднимаем её
        float OwnerZ = OwnerLocation.Z;
        if (NavLoc.Location.Z < OwnerZ + 50.0f)
        {
            NavLoc.Location.Z = OwnerZ + 50.0f;
        }
        return NavLoc.Location;
    }
    return Point;
}

// ОБНОВЛЕННАЯ ВЕРСИЯ: Теперь использует предварительно собранные позиции врагов из Game Thread
FVector UMedComAsyncTaskRepositionCalculation::FindPositionAwayFromOtherEnemies(const FVector& BasePosition, float MinDistance)
{
    // Не нужна проверка на IsInGameThread(), используем предварительно собранные данные
    FVector RepulsionVector = FVector::ZeroVector;
    
    // Вместо GetAllActorsOfClass используем предварительно кэшированные позиции
    for (const FVector& EnemyPosition : EnemyPositions)
    {
        FVector Direction = BasePosition - EnemyPosition;
        float Distance = Direction.Size();
        if (Distance < MinDistance && Distance > 0.0f)
        {
            Direction.Normalize();
            float RepulsionStrength = 1.0f - (Distance / MinDistance);
            RepulsionVector += Direction * RepulsionStrength;
        }
    }
    
    if (!RepulsionVector.IsNearlyZero())
    {
        RepulsionVector.Normalize();
        FVector NewPosition = BasePosition + RepulsionVector * (MinDistance * 0.3f);
        
        // Проверка безопасная, так как NavSys был получен в начале ExecuteCalculation
        UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(EnemyCharacter->GetWorld());
        if (NavSys)
        {
            return ProjectPointToNavigation(NavSys, NewPosition, EnemyCharacter->GetActorLocation());
        }
        return NewPosition;
    }
    
    return BasePosition;
}