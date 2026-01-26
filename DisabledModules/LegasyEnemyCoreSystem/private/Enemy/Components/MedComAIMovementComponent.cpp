#include "Core/Enemy/Components/MedComAIMovementComponent.h"
#include "Core/Enemy/MedComEnemyCharacter.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Equipment/Components/EquipmentMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Core/Enemy/StatesAI/MedComEnemyDetectionHelper.h"
#include "Equipment/Base/WeaponActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIMovement, Log, All);

UMedComAIMovementComponent::UMedComAIMovementComponent()  : OwnerEnemy(nullptr), AIController()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UMedComAIMovementComponent::BeginPlay()
{
    Super::BeginPlay();
    UpdateCache();
}

void UMedComAIMovementComponent::UpdateCache()
{
    OwnerEnemy = Cast<AMedComEnemyCharacter>(GetOwner());
    if (OwnerEnemy)
    {
        AIController = Cast<AAIController>(OwnerEnemy->GetController());
    }
}

void UMedComAIMovementComponent::InitializeParams(const FMedComRepositioningParams& Params)
{
    RepositioningParams = Params;
}

bool UMedComAIMovementComponent::NeedsRepositioning(float DistanceToTarget) const
{
    if (!OwnerEnemy)
    {
        UE_LOG(LogAIMovement, Error, TEXT("NeedsRepositioning: Invalid OwnerEnemy!"));
        return false;
    }
    bool bTooClose = DistanceToTarget < RepositioningParams.MinTargetDistance;
    bool bTooCloseToOthers = IsTooCloseToOtherEnemies(RepositioningParams.MinEnemyDistance);
    return bTooClose || bTooCloseToOthers;
}

FVector UMedComAIMovementComponent::CalculateRepositionTarget(ACharacter* TargetPlayer, float CurrentDistance, FGameplayTag CurrentFireMode)
{
    // Проверка входных параметров
    if (!TargetPlayer)
    {
        if (OwnerEnemy && OwnerEnemy->GetWorld())
        {
            TargetPlayer = UGameplayStatics::GetPlayerCharacter(OwnerEnemy->GetWorld(), 0);
        }
        if (!TargetPlayer)
        {
            UE_LOG(LogAIMovement, Error, TEXT("CalculateRepositionTarget: TargetPlayer is invalid!"));
            return FVector::ZeroVector;
        }
    }

    // Проверка владельца
    if (!OwnerEnemy)
    {
        UpdateCache();
        if (!OwnerEnemy)
        {
            UE_LOG(LogAIMovement, Error, TEXT("CalculateRepositionTarget: OwnerEnemy is invalid!"));
            return FVector::ZeroVector;
        }
    }

    // Базовые расчеты
    FVector EnemyLocation = OwnerEnemy->GetActorLocation();
    FVector PlayerLocation = TargetPlayer->GetActorLocation();
    FVector DirectionToPlayer = (PlayerLocation - EnemyLocation).GetSafeNormal();
    DirectionToPlayer.Z = 0.0f;
    FVector RightVector = FVector::CrossProduct(DirectionToPlayer, FVector::UpVector);

    // Базовое расстояние для перемещения
    float MoveDistance = 200.0f;

    // Определяем тип перемещения на основе дистанции и режима оружия
    int32 MoveTypeValue = 0; // 0 = Forward, 1 = Backward, 2 = Left, 3 = Right
    bool bHasClearLOS = HasClearLineOfSight(TargetPlayer);

    // Простой выбор направления движения на основе ситуации
    if (CurrentDistance < RepositioningParams.MinTargetDistance)
    {
        // Слишком близко - отступаем
        MoveTypeValue = 1; // Backward
    }
    else if (CurrentDistance > RepositioningParams.MaxTargetDistance)
    {
        // Слишком далеко - приближаемся
        MoveTypeValue = 0; // Forward
    }
    else
    {
        // Подбираем тактику в зависимости от режима стрельбы
        if (CurrentFireMode == FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single"))
        {
            // Одиночные выстрелы - предпочитаем стабильную позицию,
            // иногда боковое смещение для непредсказуемости
            MoveTypeValue = (FMath::RandBool()) ? 2 : 3; // Left или Right
            if (!bHasClearLOS || FMath::RandBool()) 
            {
                MoveTypeValue = 0; // Forward, если нет прямой видимости или случайно
            }
        }
        else if (CurrentFireMode == FGameplayTag::RequestGameplayTag("Weapon.FireMode.Burst"))
        {
            // Режим очередями - активное боковое перемещение
            MoveTypeValue = (FMath::RandBool()) ? 2 : 3; // Left или Right
            if (!bHasClearLOS) 
            {
                MoveTypeValue = 0; // Forward, если нет прямой видимости
            }
        }
        else // Auto
        {
            // Автоматический огонь - агрессивное маневрирование
            if (CurrentDistance < RepositioningParams.AutoFireDistance)
            {
                // На близкой дистанции при авто - отступаем или двигаемся в сторону
                MoveTypeValue = FMath::RandRange(1, 3); // Backward, Left или Right
            }
            else
            {
                // На дальней дистанции при авто - сближаемся или боковое движение
                MoveTypeValue = FMath::RandRange(0, 3); // Любое направление
                if (!bHasClearLOS) 
                {
                    MoveTypeValue = 0; // Forward, если нет прямой видимости
                }
            }
        }
    }

    // Вычисляем смещение в зависимости от типа движения
    FVector MoveOffset;
    switch (MoveTypeValue)
    {
    case 0: // Forward
        MoveOffset = DirectionToPlayer * MoveDistance;
        break;
    case 1: // Backward
        MoveOffset = -DirectionToPlayer * MoveDistance;
        break;
    case 2: // Left
        MoveOffset = -RightVector * MoveDistance;
        break;
    case 3: // Right
        MoveOffset = RightVector * MoveDistance;
        break;
    default:
        MoveOffset = FVector::ZeroVector;
        break;
    }

    // Добавляем небольшое случайное смещение для непредсказуемости
    float RandomAngle = FMath::FRandRange(-30.0f, 30.0f) * (PI / 180.0f); // +/- 30 градусов
    FVector RandomDirection(
        FMath::Cos(RandomAngle) * MoveOffset.X - FMath::Sin(RandomAngle) * MoveOffset.Y,
        FMath::Sin(RandomAngle) * MoveOffset.X + FMath::Cos(RandomAngle) * MoveOffset.Y,
        0.0f
    );
    MoveOffset = RandomDirection.GetSafeNormal() * MoveDistance;

    // Вычисляем конечную позицию
    FVector CandidatePosition = EnemyLocation + MoveOffset;

    // Проецируем на NavMesh
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(OwnerEnemy->GetWorld());
    if (NavSys)
    {
        CandidatePosition = ProjectPointToNavigation(NavSys, CandidatePosition);
    }

    // Пробуем несколько направлений, если текущее не дает видимости цели
    if (!CanSeeTargetFromPosition(TargetPlayer, CandidatePosition))
    {
        const int32 MaxAttempts = 4;
        for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
        {
            int32 NewDirection = FMath::RandRange(0, 3);
            FVector NewOffset;
            
            switch (NewDirection)
            {
            case 0: // Forward
                NewOffset = DirectionToPlayer * MoveDistance;
                break;
            case 1: // Backward
                NewOffset = -DirectionToPlayer * MoveDistance;
                break;
            case 2: // Left
                NewOffset = -RightVector * MoveDistance;
                break;
            case 3: // Right
                NewOffset = RightVector * MoveDistance;
                break;
            }
            
            // Варьируем расстояние для улучшения поиска
            float DistanceMultiplier = FMath::FRandRange(0.7f, 1.3f);
            FVector TestPosition = EnemyLocation + (NewOffset * DistanceMultiplier);
            
            if (NavSys)
            {
                TestPosition = ProjectPointToNavigation(NavSys, TestPosition);
            }
            
            if (CanSeeTargetFromPosition(TargetPlayer, TestPosition))
            {
                CandidatePosition = TestPosition;
                break;
            }
        }
    }

    // Если все попытки не дали результата, используем позицию игрока как запасной вариант
    if (!CanSeeTargetFromPosition(TargetPlayer, CandidatePosition))
    {
        // Берем полукруг вокруг игрока
        float OptimalDistance = FMath::Clamp(
            CurrentDistance * 0.8f,
            RepositioningParams.MinTargetDistance + 1.0f,
            RepositioningParams.MaxTargetDistance - 1.0f
        );
        
        // Выбираем случайную позицию на оптимальном расстоянии
        float Angle = FMath::FRandRange(-60.0f, 60.0f) * (PI / 180.0f);
        FRotator Rotation(0.0f, Angle * (180.0f / PI), 0.0f);
        FVector Direction = Rotation.RotateVector(-DirectionToPlayer);
        
        CandidatePosition = PlayerLocation + Direction * (OptimalDistance * 100.0f); // Конвертируем в см
        
        if (NavSys)
        {
            CandidatePosition = ProjectPointToNavigation(NavSys, CandidatePosition);
        }
    }

    // Обеспечиваем отталкивание от других врагов
    CandidatePosition = FindPositionAwayFromOtherEnemies(CandidatePosition, RepositioningParams.MinEnemyDistance);

    UE_LOG(LogAIMovement, Log, TEXT("%s: Calculated reposition target at %s (MoveType: %d)"), 
           *OwnerEnemy->GetName(), *CandidatePosition.ToString(), MoveTypeValue);

    return CandidatePosition;
}

FVector UMedComAIMovementComponent::CalculateAttackPosition(const ACharacter* TargetPlayer) const
{
    if (!OwnerEnemy || !TargetPlayer)
    {
        UE_LOG(LogAIMovement, Error, TEXT("CalculateAttackPosition: Invalid parameters!"));
        return FVector::ZeroVector;
    }
    
    float Distance = FVector::Distance(OwnerEnemy->GetActorLocation(), TargetPlayer->GetActorLocation()) / CM_TO_M_CONVERSION;
    FVector ResultPosition = OwnerEnemy->GetActorLocation();
    
    if (Distance < RepositioningParams.MinTargetDistance)
    {
        FVector DirFromPlayer = (OwnerEnemy->GetActorLocation() - TargetPlayer->GetActorLocation()).GetSafeNormal();
        DirFromPlayer.Z = 0;
        ResultPosition = TargetPlayer->GetActorLocation() + DirFromPlayer * ((RepositioningParams.MinTargetDistance * M_TO_CM_CONVERSION) + 50.0f);
    }
    
    return FindPositionAwayFromOtherEnemies(ResultPosition, RepositioningParams.MinEnemyDistance);
}

void UMedComAIMovementComponent::RepositionForAttack(const ACharacter* TargetPlayer)
{
    if (!OwnerEnemy || !TargetPlayer || !OwnerEnemy->HasAuthority())
    {
        UE_LOG(LogAIMovement, Error, TEXT("RepositionForAttack: Invalid parameters or not authority."));
        return;
    }
    
    if (!AIController)
    {
        UpdateCache();
    }
    
    FVector OptimalPosition = CalculateAttackPosition(TargetPlayer);
    FVector DirToPlayer = (TargetPlayer->GetActorLocation() - OwnerEnemy->GetActorLocation()).GetSafeNormal();
    DirToPlayer.Z = 0;
    FRotator TargetRotation = DirToPlayer.Rotation();
    OwnerEnemy->SetActorRotation(TargetRotation);
    
    if (AIController)
    {
        AIController->MoveToLocation(OptimalPosition, 50.0f);
        if (UCharacterMovementComponent* MoveComp = OwnerEnemy->GetCharacterMovement())
        {
            MoveComp->bOrientRotationToMovement = false;
        }
        AIController->SetControlRotation(TargetRotation);
        UE_LOG(LogAIMovement, Log, TEXT("%s: Repositioning for attack to %s"), *OwnerEnemy->GetName(), *OptimalPosition.ToString());
    }
}

bool UMedComAIMovementComponent::HasClearLineOfFire(const ACharacter* Target) const
{
    if (!OwnerEnemy || !Target || !OwnerEnemy->GetWorld())
    {
        UE_LOG(LogAIMovement, Error, TEXT("HasClearLineOfFire: Invalid parameters!"));
        return false;
    }
    
    FVector WeaponLocation = OwnerEnemy->GetActorLocation() + FVector(0, 0, 60);
    if (AWeaponActor* Weapon = OwnerEnemy->GetCurrentWeapon())
    {
        if (Weapon->GetMeshComponent())
        {
            WeaponLocation = Weapon->GetMeshComponent()->GetSocketLocation("Muzzle");
        }
    }
    
    FVector TargetLocation = Target->GetActorLocation();
    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerEnemy);
    
    bool bHit = OwnerEnemy->GetWorld()->LineTraceSingleByChannel(HitResult, WeaponLocation, TargetLocation, ECC_Visibility, Params);
    return (!bHit || HitResult.GetActor() == Target);
}


bool UMedComAIMovementComponent::UpdateRepositioning(ACharacter* Target, float DeltaTime, float& InOutRepositionTimer)
{
    if (!OwnerEnemy || !Target || !OwnerEnemy->GetWorld())
    {
        UE_LOG(LogAIMovement, Error, TEXT("UpdateRepositioning: Invalid parameters!"));
        return false;
    }
    
    if (!bRepositioning)
    {
        return false;
    }
    
    // Decrease repositioning timer
    InOutRepositionTimer -= DeltaTime;
    
    // CRITICAL: Force bot to look at target even during repositioning
    RotateTowardsTarget(Target, DeltaTime, 10.0f);
    
    float DistToTarget = FVector::Distance(OwnerEnemy->GetActorLocation(), TargetPosition);
    if (DistToTarget < 100.0f || InOutRepositionTimer <= 0.0f)
    {
        bRepositioning = false;
        LastRepositionTime = 0.0f;
        ShotsSinceReposition = 0;
        if (AIController)
        {
            AIController->StopMovement();
        }
        
        // Additional alignment after repositioning completes
        RotateTowardsTarget(Target, DeltaTime, 10.0f);
        
        // IMPORTANT: Keep bOrientRotationToMovement false to continue facing player
        if (UCharacterMovementComponent* MoveComp = OwnerEnemy->GetCharacterMovement())
        {
            MoveComp->bOrientRotationToMovement = false;
        }
        
        UE_LOG(LogAIMovement, Log, TEXT("%s: Repositioning complete."), *OwnerEnemy->GetName());
        return false;
    }
    
    return true;
}


void UMedComAIMovementComponent::StartRepositioning(ACharacter* Target, float CurrentDistance, FGameplayTag CurrentFireMode)
{
    if (!OwnerEnemy || !Target || !OwnerEnemy->GetWorld())
    {
        UE_LOG(LogAIMovement, Error, TEXT("StartRepositioning: Invalid parameters!"));
        return;
    }
    
    FVector NewPosition = CalculateRepositionTarget(Target, CurrentDistance, CurrentFireMode);
    TargetPosition = NewPosition;
    bRepositioning = true;
    RepositionTimer = RepositioningParams.MaxRepositionTime;
    
    if (!AIController)
    {
        UpdateCache();
    }
    
    if (AIController)
    {
        // IMPORTANT: Use same parameters as in UMedComEnemyStateAttack::StartRepositioning
        AIController->MoveToLocation(TargetPosition, 50.0f, true, true, true, false);
        
        // CRITICAL: Disable orientation to movement direction
        if (UCharacterMovementComponent* MoveComp = OwnerEnemy->GetCharacterMovement())
        {
            MoveComp->bOrientRotationToMovement = false;
        }
        
        UE_LOG(LogAIMovement, Log, TEXT("%s: Started repositioning to %s"), *OwnerEnemy->GetName(), *TargetPosition.ToString());
    }
    else
    {
        UE_LOG(LogAIMovement, Error, TEXT("%s: Failed to start repositioning - no AIController!"), *OwnerEnemy->GetName());
        bRepositioning = false;
    }
}

bool UMedComAIMovementComponent::IsInAttackRange(const ACharacter* Target, float MinDistance, float MaxDistance) const
{
    if (!OwnerEnemy || !Target || !OwnerEnemy->GetWorld())
    {
        UE_LOG(LogAIMovement, Error, TEXT("IsInAttackRange: Invalid parameters!"));
        return false;
    }
    
    float Distance = FVector::Distance(OwnerEnemy->GetActorLocation(), Target->GetActorLocation()) / CM_TO_M_CONVERSION;
    bool bInRange = (Distance >= MinDistance) && (Distance <= MaxDistance);
    
    if (bInRange)
    {
        bool bHasLOS = HasClearLineOfSight(Target);
        UE_LOG(LogAIMovement, Verbose, TEXT("IsInAttackRange: %s -> dist=%.1fm, LOS=%s"), *OwnerEnemy->GetName(), Distance, bHasLOS ? TEXT("YES") : TEXT("NO"));
        return bHasLOS;
    }
    
    return false;
}

bool UMedComAIMovementComponent::HasClearLineOfSight(const AActor* Target) const
{
    if (!OwnerEnemy || !Target || !OwnerEnemy->GetWorld())
    {
        return false;
    }
    
    FVector EyeLocation = OwnerEnemy->GetActorLocation() + FVector(0, 0, 70.0f);
    FVector TargetLocation = Target->GetActorLocation() + FVector(0, 0, 70.0f);
    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerEnemy);
    QueryParams.bTraceComplex = false;
    
    bool bHit = OwnerEnemy->GetWorld()->LineTraceSingleByChannel(HitResult, EyeLocation, TargetLocation, ECC_Visibility, QueryParams);
    return (!bHit || HitResult.GetActor() == Target);
}

void UMedComAIMovementComponent::RotateTowardsTarget(const ACharacter* Target, float DeltaTime, float RotationSpeed)
{
    if (!OwnerEnemy || !Target || !OwnerEnemy->GetWorld())
    {
        return;
    }
    
    FVector DirectionToTarget = (Target->GetActorLocation() - OwnerEnemy->GetActorLocation()).GetSafeNormal();
    DirectionToTarget.Z = 0;
    
    if (!DirectionToTarget.IsNearlyZero())
    {
        FRotator TargetRotation = DirectionToTarget.Rotation();
        OwnerEnemy->SetActorRotation(FMath::RInterpTo(OwnerEnemy->GetActorRotation(), TargetRotation, DeltaTime, RotationSpeed));
        
        // Also update control rotation if we have AI Controller (helps with some animation systems)
        if (AIController)
        {
            AIController->SetControlRotation(FMath::RInterpTo(AIController->GetControlRotation(), TargetRotation, DeltaTime, RotationSpeed));
        }
    }
}

FVector UMedComAIMovementComponent::ProjectPointToNavigation(UNavigationSystemV1* NavSys, const FVector& Point) const
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
        float OwnerZ = OwnerEnemy->GetActorLocation().Z;
        if (NavLoc.Location.Z < OwnerZ + 50.0f) // 50 см отступ можно корректировать
        {
            NavLoc.Location.Z = OwnerZ + 50.0f;
        }
        return NavLoc.Location;
    }
    
    return Point;
}


bool UMedComAIMovementComponent::CanSeeTargetFromPosition(const ACharacter* Target, const FVector& Position) const
{
    if (!OwnerEnemy || !Target || !OwnerEnemy->GetWorld())
    {
        return false;
    }
    
    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerEnemy);
    
    FVector EyePosition = Position + FVector(0, 0, EYE_HEIGHT_OFFSET);
    FVector LocalTargetPosition = Target->GetActorLocation() + FVector(0, 0, EYE_HEIGHT_OFFSET);
    
    bool bHit = OwnerEnemy->GetWorld()->LineTraceSingleByChannel(HitResult, EyePosition, LocalTargetPosition, ECC_Visibility, Params);
    return (!bHit || HitResult.GetActor() == Target);
}

FVector UMedComAIMovementComponent::FindPositionAwayFromOtherEnemies(const FVector& BasePosition, float MinDistance) const
{
    if (!OwnerEnemy || !OwnerEnemy->GetWorld())
    {
        return BasePosition;
    }
    
    TArray<AActor*> AllEnemies;
    UGameplayStatics::GetAllActorsOfClass(OwnerEnemy->GetWorld(), AMedComEnemyCharacter::StaticClass(), AllEnemies);
    
    FVector RepulsionVector = FVector::ZeroVector;
    for (AActor* Actor : AllEnemies)
    {
        AMedComEnemyCharacter* OtherEnemy = Cast<AMedComEnemyCharacter>(Actor);
        if (OtherEnemy && OtherEnemy != OwnerEnemy)
        {
            FVector Direction = BasePosition - OtherEnemy->GetActorLocation();
            float Distance = Direction.Size();
            if (Distance < MinDistance && Distance > 0.0f)
            {
                Direction.Normalize();
                float RepulsionStrength = 1.0f - (Distance / MinDistance);
                RepulsionVector += Direction * RepulsionStrength;
            }
        }
    }
    
    if (!RepulsionVector.IsNearlyZero())
    {
        RepulsionVector.Normalize();
        FVector NewPosition = BasePosition + RepulsionVector * (MinDistance * 0.3f);
        if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(OwnerEnemy->GetWorld()))
        {
            return ProjectPointToNavigation(NavSys, NewPosition);
        }
        return NewPosition;
    }
    
    return BasePosition;
}

bool UMedComAIMovementComponent::IsTooCloseToOtherEnemies(float MinDistance) const
{
    if (!OwnerEnemy || !OwnerEnemy->GetWorld())
    {
        return false;
    }
    
    TArray<AActor*> AllEnemies;
    UGameplayStatics::GetAllActorsOfClass(OwnerEnemy->GetWorld(), AMedComEnemyCharacter::StaticClass(), AllEnemies);
    
    for (AActor* Actor : AllEnemies)
    {
        AMedComEnemyCharacter* OtherEnemy = Cast<AMedComEnemyCharacter>(Actor);
        if (OtherEnemy && OtherEnemy != OwnerEnemy)
        {
            float Distance = FVector::Dist(OwnerEnemy->GetActorLocation(), OtherEnemy->GetActorLocation());
            if (Distance < MinDistance)
            {
                return true;
            }
        }
    }
    
    return false;
}
