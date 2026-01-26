#include "Core/Enemy/FSM/States/MedComAttackState.h"
#include "Core/Enemy/FSM/MedComEnemyFSMComponent.h"
#include "Core/Enemy/MedComEnemyCharacter.h"
#include "Engine/World.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogMedComAttack, Log, All);

/*------------------------------------------------------------------*/
UMedComAttackState::UMedComAttackState()
{
	StateTag = FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"));
}

/*------------------------------------------------------------------*/
void UMedComAttackState::OnEnter(AMedComEnemyCharacter* Owner)
{
	Super::OnEnter(Owner);
	if (!Owner) return;

	// кэшируем контроллер & цель
	CachedController = Cast<AAIController>(Owner->GetController());
	CachedTarget     = UGameplayStatics::GetPlayerPawn(Owner, 0);

	// движение: бот стоит и поворачивается сам
	if (UCharacterMovementComponent* CM = Owner->GetCharacterMovement())
	{
		CM->bOrientRotationToMovement = false;
		CM->StopMovementImmediately();
	}

	TimeSinceLastSight  = 0.f;
	NextAllowedShotTime = Owner->GetWorld()->GetTimeSeconds() + 0.2f;	// маленькая задержка перед первым выстрелом

	UE_LOG(LogMedComAttack, Log, TEXT("[%s] → Attack (target=%s)"),
	       *Owner->GetName(), *GetNameSafe(CachedTarget.Get()));
}

/*------------------------------------------------------------------*/
void UMedComAttackState::OnExit(AMedComEnemyCharacter* Owner)
{
	Super::OnExit(Owner);
	if (!Owner) return;

	// убираем тег состояния
	Owner->RemoveGameplayTag(StateTag);

	// возвращаем ориентацию-к-движению
	if (UCharacterMovementComponent* CM = Owner->GetCharacterMovement())
	{
		CM->bOrientRotationToMovement = true;
	}
}

/*------------------------------------------------------------------*/
void UMedComAttackState::ProcessTick(AMedComEnemyCharacter* Owner, float DeltaTime)
{
	if (!Owner) return;

	/* 1. цель жива / валидна? */
	if (!HasValidTarget(Owner))
	{
		if (FSMComponent) FSMComponent->ProcessFSMEvent(EEnemyEvent::PlayerLost);
		return;
	}

	/* 2. line-of-sight? */
	if (HasLineOfSight(Owner))
	{
		TimeSinceLastSight = 0.f;
	}
	else
	{
		TimeSinceLastSight += DeltaTime;
		if (TimeSinceLastSight > SightGraceTime)
		{
			if (FSMComponent) FSMComponent->ProcessFSMEvent(EEnemyEvent::PlayerLost);
			return;
		}
	}

	/* 3. дистанция */
	const float Dist = FVector::Dist(Owner->GetActorLocation(),
	                                 CachedTarget->GetActorLocation()) * 0.01f; // → метры

	if (Dist > AttackRange + 200.f)      // за пределами зоны
	{
		if (FSMComponent) FSMComponent->ProcessFSMEvent(EEnemyEvent::TargetOutOfRange);
		return;
	}

	/* 4. поворачиваемся к цели */
	ForceRotationToTarget(Owner, DeltaTime);

	/* 5. выбираем и проставляем (в TAG’ах) режим огня */
	SelectFireMode(Owner, Dist);

	/* 6. «стреляем» */
	const float Now = Owner->GetWorld()->GetTimeSeconds();
	if (Now >= NextAllowedShotTime)
	{
		FireWeapon(Owner);
		NextAllowedShotTime = Now + FireCooldown;
	}
}

/*------------------------------------------------------------------*/
void UMedComAttackState::OnEvent(AMedComEnemyCharacter* Owner,
                                 EEnemyEvent            Event,
                                 AActor*                Instigator)
{
	Super::OnEvent(Owner, Event, Instigator);

	if (Event == EEnemyEvent::PlayerSeen && Instigator)
	{
		CachedTarget = Cast<APawn>(Instigator);
		TimeSinceLastSight = 0.f;
	}
}

/* === helpers =====================================================*/
bool UMedComAttackState::HasValidTarget(AMedComEnemyCharacter* /*Owner*/) const
{
	return CachedTarget.IsValid();
}

bool UMedComAttackState::HasLineOfSight(AMedComEnemyCharacter* Owner) const
{
	if (!CachedController.IsValid() || !CachedTarget.IsValid()) return false;

	// FVector(0) — значит «используй точку глаз контроллера»
	return CachedController->LineOfSightTo(CachedTarget.Get(), FVector::ZeroVector, true);
}

void UMedComAttackState::SelectFireMode(AMedComEnemyCharacter* Owner, float DistM)
{
	// TODO: считать пороги из DataAsset, выдавать нужные тэги и вызывать WeaponHandler->SetFireMode()
	if (!Owner) return;

	FString Mode = TEXT("Single");
	if (DistM < 2.f)       Mode = TEXT("Auto");
	else if (DistM < 10.f) Mode = TEXT("Burst");

	UE_LOG(LogMedComAttack, Verbose, TEXT("[%s] FireMode = %s (D=%.1f m)"),
	       *Owner->GetName(), *Mode, DistM);

	// пример: Owner->AddGameplayTag(TagForMode);
}

void UMedComAttackState::FireWeapon(AMedComEnemyCharacter* Owner)
{
	if (!Owner) return;

	// TODO: подключить WeaponHandler / GAS умения.
	// Сейчас просто логируем действие.
	UE_LOG(LogMedComAttack, Log, TEXT("[%s] *BANG* (fake shot)"), *Owner->GetName());

	// Example:
	// if (UMedComWeaponHandlerComponent* WH = Owner->GetWeaponHandlerComponent())
	// {
	// 	 WH->TryFireWeapon();
	// }
}

void UMedComAttackState::ForceRotationToTarget(AMedComEnemyCharacter* Owner, float DeltaTime) const
{
	if (!Owner || !CachedTarget.IsValid()) return;

	FVector Dir = (CachedTarget->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
	if (!Dir.IsNearlyZero())
	{
		const FRotator Wanted = Dir.Rotation();
		Owner->SetActorRotation(FMath::RInterpTo(Owner->GetActorRotation(), Wanted, DeltaTime, 6.f));
	}
}
