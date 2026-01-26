#include "Core/Enemy/FSM/States/MedComChaseState.h"
#include "Core/Enemy/MedComEnemyCharacter.h"
#include "Core/Enemy/FSM/MedComEnemyFSMComponent.h"
#include "Core/Enemy/CrowdManagerSubsystem.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Core/MedComPlayerController.h"
#include "ProfilingDebugging/CsvProfiler.h"

DEFINE_LOG_CATEGORY_STATIC(LogMedComChase, Log, All);
CSV_DEFINE_CATEGORY(EnemyChase, true);

static APawn* FindNearestPlayerPawn(const FVector& From,UWorld* World)
{
	float BestSq = BIG_NUMBER; APawn* Result = nullptr;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* P = It->IsValid()? It->Get()->GetPawn() : nullptr;
		if (!P) continue;
		const float D = FVector::DistSquared(From,P->GetActorLocation());
		if (D < BestSq){ BestSq=D; Result=P; }
	}
	return Result;
}

/*-------------------------------- ENTER --------------------------------*/
UMedComChaseState::UMedComChaseState()
{
	StateTag = FGameplayTag::RequestGameplayTag(TEXT("State.Chase"));
}

void UMedComChaseState::OnEnter(AMedComEnemyCharacter* Owner)
{
	Super::OnEnter(Owner);
	if (!Owner) return;

	/* — параметры из StateParams / DataAsset — */
	ChaseSpeed       = GetStateParamFloat(TEXT("ChaseSpeed"),       ChaseSpeed);
	AcceptanceRadius = GetStateParamFloat(TEXT("AcceptanceRadius"), AcceptanceRadius);
	RepathInterval   = GetStateParamFloat(TEXT("UpdateInterval"),   RepathInterval);
	LoseSightTime    = GetStateParamFloat(TEXT("LoseTargetTime"),   LoseSightTime);
	AttackRange      = GetStateParamFloat(TEXT("AttackRange"),      AttackRange);

	Controller  = Cast<AAIController>(Owner->GetController());
	Target      = FindNearestPlayerPawn(Owner->GetActorLocation(),Owner->GetWorld());
	LastRepathTime = -RepathInterval;      // форс-старт
	TimeSinceLost  = 0.f;

	ConfigureMovement(Owner);
	StartMoveToTarget(Owner);
}

/*-------------------------------- EXIT ---------------------------------*/
void UMedComChaseState::OnExit(AMedComEnemyCharacter* Owner)
{
	if (Controller.IsValid()) Controller->StopMovement();
	Super::OnExit(Owner);
}

/*------------------------------ TICK -----------------------------------*/
void UMedComChaseState::ProcessTick(AMedComEnemyCharacter* Owner,float Dt)
{
	if (!Owner) return;
	CSV_SCOPED_TIMING_STAT(EnemyChase, Tick);

	/* 1) валидность цели -------------------------------------------------*/
	if (!Target.IsValid())
	{
		Target = FindNearestPlayerPawn(Owner->GetActorLocation(),Owner->GetWorld());
		if (!Target.IsValid()){ FSMComponent->ProcessFSMEvent(EEnemyEvent::PlayerLost); return; }
	}

	/* 2) видимость / потеря ---------------------------------------------*/
	if (CanSeePawn(Owner,Target.Get()))	TimeSinceLost = 0.f;
	else								TimeSinceLost += Dt;

	if (TimeSinceLost > LoseSightTime)
	{
		FSMComponent->ProcessFSMEvent(EEnemyEvent::PlayerLost); return;
	}

	/* 3) repath ----------------------------------------------------------*/
	const float Now = Owner->GetWorld()->GetTimeSeconds();
	if (NeedRepath(Now)) StartMoveToTarget(Owner);

	/* 4) дистанция до атаки ---------------------------------------------*/
	if (FVector::DistSquared(Owner->GetActorLocation(),
							 Target->GetActorLocation()) <= FMath::Square(AttackRange))
	{
		FSMComponent->ProcessFSMEvent(EEnemyEvent::ReachedTarget,Target.Get());
	}
}

/*----------------------------- EVENTS ----------------------------------*/
void UMedComChaseState::OnEvent(AMedComEnemyCharacter* Owner,EEnemyEvent Event,AActor* Instigator)
{
	Super::OnEvent(Owner,Event,Instigator);
	if (Event==EEnemyEvent::PlayerSeen && Instigator)
	{
		if (APawn* P=Cast<APawn>(Instigator)){ Target=P; StartMoveToTarget(Owner); TimeSinceLost=0.f; }
	}
}

/*============================ HELPERS ==================================*/
bool UMedComChaseState::CanSeePawn(const AMedComEnemyCharacter* Owner,
								   const APawn*                 TargetPawn) const
{
	if (!Owner || !TargetPawn) return false;

	/* 1) у бота есть AI-контроллер → используем встроенный LOS-тест  */
	if (Controller.IsValid())
	{
		const FVector ViewPoint = Owner->GetPawnViewLocation();   // позиция «глаз»
		return Controller->LineOfSightTo(TargetPawn, ViewPoint, true);

	}

	/* 2) fallback – простой line-trace (если по каким-то причинам контроллера нет) */
	FHitResult Hit;
	const FVector From = Owner->GetActorLocation() + FVector(0,0,60.f);
	const FVector To   = TargetPawn->GetActorLocation() + FVector(0,0,60.f);

	const bool bHit = Owner->GetWorld()->LineTraceSingleByChannel(
						  Hit, From, To, ECC_Visibility,
						  FCollisionQueryParams(TEXT("ChaseLOS"), /*bTraceComplex*/false, Owner));

	return !bHit || Hit.GetActor() == TargetPawn;     // видно, если ничего не задели
}


bool UMedComChaseState::NeedRepath(float Now) const
{
	if (Now - LastRepathTime >= RepathInterval)                        return true;
	if (!Target.IsValid())                                             return false;

	const float DistSq = FVector::DistSquared(Target->GetActorLocation(),LastPathGoal);
	return DistSq >= FMath::Square(RepathDistance);
}

void UMedComChaseState::ConfigureMovement(AMedComEnemyCharacter* Owner) const
{
	/* Full LOD → CharacterMovement; иначе – Floating/Crowd */
	if (Owner->GetCurrentDetailLevel()==EAIDetailLevel::Full)
	{
		UCharacterMovementComponent* CM = Owner->GetCharacterMovement();
		if (CM)
		{
			CM->SetComponentTickEnabled(true);
			CM->SetMovementMode(MOVE_NavWalking);
			CM->MaxWalkSpeed             = ChaseSpeed;
			CM->bOrientRotationToMovement= true;
		}
		/* выключаем Floating */
		if (UFloatingPawnMovement* FM = Owner->GetFloatingMovementComponent())
			FM->SetComponentTickEnabled(false);
	}
	else
	{
		if (UFloatingPawnMovement* FM = Owner->GetFloatingMovementComponent())
		{
			FM->SetComponentTickEnabled(true);
			FM->MaxSpeed   = ChaseSpeed;
			FM->Acceleration = ChaseSpeed*4.f;
		}
		if (UCharacterMovementComponent* CM = Owner->GetCharacterMovement())
			CM->SetComponentTickEnabled(false);
	}
}

void UMedComChaseState::StartMoveToTarget(AMedComEnemyCharacter* Owner)
{
	if (!Owner || !Target.IsValid()) return;

	LastPathGoal   = Target->GetActorLocation();
	LastRepathTime = Owner->GetWorld()->GetTimeSeconds();

	/* Reduced+ → CrowdManager (дёшево) ---------------------------------*/
	if (Owner->GetCurrentDetailLevel() >= EAIDetailLevel::Reduced)
	{
		if (auto* Crowd = Owner->GetWorld()->GetSubsystem<UCrowdManagerSubsystem>())
		{
			Crowd->RequestAgentMove(Owner,LastPathGoal);
			return;
		}
	}

	/* Full LOD → стандартный MoveTo ------------------------------------*/
	if (Controller.IsValid())
	{
		Controller->MoveToActor(Target.Get(),AcceptanceRadius,
								/*StopOnOverlap*/false,/*Pathfinding*/true,
								/*ProjectGoal*/true);
	}
}
