// SuspenseCoreWeaponAsyncTask_PerformTrace.cpp
// SuspenseCore - Async Weapon Trace Task Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Tasks/SuspenseCoreWeaponAsyncTask_PerformTrace.h"
#include "SuspenseCore/Utils/SuspenseCoreTraceUtils.h"
#include "SuspenseCore/Utils/SuspenseCoreSpreadCalculator.h"
#include "SuspenseCore/Attributes/SuspenseCoreWeaponAttributeSet.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeapon.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeaponCombatState.h"
#include "SuspenseCore/Core/SuspenseCoreUnits.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

USuspenseCoreWeaponAsyncTask_PerformTrace::USuspenseCoreWeaponAsyncTask_PerformTrace()
	: bUseRequestMode(false)
{
}

//========================================================================
// Task Creation
//========================================================================

USuspenseCoreWeaponAsyncTask_PerformTrace* USuspenseCoreWeaponAsyncTask_PerformTrace::PerformWeaponTrace(
	UGameplayAbility* OwningAbility,
	const FSuspenseCoreWeaponTraceConfig& InConfig)
{
	USuspenseCoreWeaponAsyncTask_PerformTrace* Task = NewAbilityTask<USuspenseCoreWeaponAsyncTask_PerformTrace>(
		OwningAbility, FName("WeaponTrace"));

	if (Task)
	{
		Task->Config = InConfig;
		Task->bUseRequestMode = false;
	}

	return Task;
}

USuspenseCoreWeaponAsyncTask_PerformTrace* USuspenseCoreWeaponAsyncTask_PerformTrace::PerformWeaponTraceFromRequest(
	UGameplayAbility* OwningAbility,
	const FWeaponShotParams& InShotRequest,
	bool bDebug)
{
	USuspenseCoreWeaponAsyncTask_PerformTrace* Task = NewAbilityTask<USuspenseCoreWeaponAsyncTask_PerformTrace>(
		OwningAbility, FName("WeaponTraceFromRequest"));

	if (Task)
	{
		Task->ShotRequest = InShotRequest;
		Task->bUseRequestMode = true;
		Task->Config.bDebug = bDebug;
	}

	return Task;
}

//========================================================================
// Task Interface
//========================================================================

void USuspenseCoreWeaponAsyncTask_PerformTrace::Activate()
{
	if (bUseRequestMode)
	{
		ExecuteTraceFromRequest();
	}
	else
	{
		ExecuteTrace();
	}
}

FString USuspenseCoreWeaponAsyncTask_PerformTrace::GetDebugString() const
{
	return FString::Printf(TEXT("WeaponTrace (Mode: %s)"),
		bUseRequestMode ? TEXT("Request") : TEXT("Config"));
}

//========================================================================
// Trace Execution
//========================================================================

void USuspenseCoreWeaponAsyncTask_PerformTrace::ExecuteTrace()
{
	if (!Ability)
	{
		EndTask();
		return;
	}

	// Get avatar actor
	AActor* AvatarActor = GetAvatarActor();
	if (!AvatarActor)
	{
		EndTask();
		return;
	}

	// Get weapon attributes
	const USuspenseCoreWeaponAttributeSet* WeaponAttrs = GetWeaponAttributes();

	// Determine trace parameters
	FVector MuzzleLocation = GetMuzzleLocation();
	FVector AimPoint;
	FVector CameraLocation;

	// Get aim point from screen center
	if (APlayerController* PC = Cast<APlayerController>(
		Cast<APawn>(AvatarActor)->GetController()))
	{
		USuspenseCoreTraceUtils::GetAimPoint(PC, DefaultMaxRange, CameraLocation, AimPoint);
	}
	else
	{
		// Fallback: use actor forward
		AimPoint = MuzzleLocation + (AvatarActor->GetActorForwardVector() * DefaultMaxRange);
	}

	// Calculate direction
	FVector AimDirection = (AimPoint - MuzzleLocation).GetSafeNormal();

	// Get spread parameters
	float BaseSpread = 1.0f;
	float MaxRange = DefaultMaxRange;
	int32 NumTraces = 1;

	if (WeaponAttrs)
	{
		BaseSpread = WeaponAttrs->GetHipFireSpread();

		// CRITICAL: Use CalculateMaxTraceRangeFromWeapon for proper unit conversion!
		// WeaponAttrs->GetMaxRange() returns METERS (DataTable units)
		// CalculateMaxTraceRangeFromWeapon converts to UE units (centimeters)
		// Example: 600m → 60000 UE units
		//
		// @see SuspenseCoreUnits::ConvertRangeToUnits()
		// @see Documentation/GAS/UnitConversionSystem.md
		MaxRange = USuspenseCoreSpreadCalculator::CalculateMaxTraceRangeFromWeapon(WeaponAttrs);
	}

	// Apply overrides
	if (Config.OverrideSpreadAngle >= 0.0f)
	{
		BaseSpread = Config.OverrideSpreadAngle;
	}
	if (Config.OverrideMaxRange >= 0.0f)
	{
		MaxRange = Config.OverrideMaxRange;
	}
	if (Config.OverrideNumTraces > 0)
	{
		NumTraces = Config.OverrideNumTraces;
	}

	// Apply spread modifiers
	const float SpreadModifier = CalculateSpreadModifier();
	const float FinalSpread = BaseSpread * SpreadModifier;

	// Prepare result
	FSuspenseCoreWeaponTraceResult Result;
	Result.MuzzleLocation = MuzzleLocation;
	Result.AimDirection = AimDirection;
	Result.AppliedSpreadAngle = FinalSpread;
	Result.NumTraces = NumTraces;

	// Actors to ignore
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(AvatarActor);

	// Perform traces
	FRandomStream RandomStream;
	RandomStream.GenerateNewSeed();

	for (int32 i = 0; i < NumTraces; ++i)
	{
		// Apply spread
		FVector TraceDirection = USuspenseCoreTraceUtils::ApplySpreadToDirection(
			AimDirection, FinalSpread, RandomStream.RandRange(0, INT32_MAX));

		// Calculate end point
		FVector TraceEnd = USuspenseCoreTraceUtils::CalculateTraceEndPoint(
			MuzzleLocation, TraceDirection, MaxRange);

		// Perform trace
		TArray<FHitResult> TraceHits;
		bool bHadHit = USuspenseCoreTraceUtils::PerformLineTrace(
			AvatarActor,
			MuzzleLocation,
			TraceEnd,
			Config.TraceProfile,
			ActorsToIgnore,
			Config.bDebug,
			Config.DebugDrawTime,
			TraceHits
		);

		// Collect results
		Result.HitResults.Append(TraceHits);
		if (bHadHit)
		{
			Result.bHadBlockingHit = true;
		}
	}

	// Broadcast result
	OnCompleted.Broadcast(Result);
	EndTask();
}

void USuspenseCoreWeaponAsyncTask_PerformTrace::ExecuteTraceFromRequest()
{
	if (!Ability)
	{
		EndTask();
		return;
	}

	AActor* AvatarActor = GetAvatarActor();
	if (!AvatarActor)
	{
		EndTask();
		return;
	}

	// Prepare result
	FSuspenseCoreWeaponTraceResult Result;
	Result.MuzzleLocation = ShotRequest.StartLocation;
	Result.AimDirection = ShotRequest.Direction;
	Result.AppliedSpreadAngle = ShotRequest.SpreadAngle;
	Result.NumTraces = FMath::Max(1, ShotRequest.ShotNumber); // ShotNumber used as trace count for shotguns

	// Actors to ignore
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(AvatarActor);
	if (ShotRequest.Instigator && ShotRequest.Instigator != AvatarActor)
	{
		ActorsToIgnore.Add(ShotRequest.Instigator);
	}

	// Use deterministic random stream from shot parameters
	FRandomStream RandomStream(static_cast<int32>(ShotRequest.Timestamp * 1000.0f));

	const int32 NumTraces = Result.NumTraces;
	for (int32 i = 0; i < NumTraces; ++i)
	{
		// Apply spread using deterministic seed
		FVector TraceDirection = USuspenseCoreTraceUtils::ApplySpreadToDirection(
			ShotRequest.Direction,
			ShotRequest.SpreadAngle,
			RandomStream.RandRange(0, INT32_MAX)
		);

		// Calculate end point
		FVector TraceEnd = USuspenseCoreTraceUtils::CalculateTraceEndPoint(
			ShotRequest.StartLocation,
			TraceDirection,
			ShotRequest.Range
		);

		// Perform trace
		TArray<FHitResult> TraceHits;
		bool bHadHit = USuspenseCoreTraceUtils::PerformLineTrace(
			AvatarActor,
			ShotRequest.StartLocation,
			TraceEnd,
			Config.TraceProfile,
			ActorsToIgnore,
			Config.bDebug,
			Config.DebugDrawTime,
			TraceHits
		);

		Result.HitResults.Append(TraceHits);
		if (bHadHit)
		{
			Result.bHadBlockingHit = true;
		}
	}

	// Broadcast result
	OnCompleted.Broadcast(Result);
	EndTask();
}

//========================================================================
// Helper Functions
//========================================================================

const USuspenseCoreWeaponAttributeSet* USuspenseCoreWeaponAsyncTask_PerformTrace::GetWeaponAttributes() const
{
	if (!Ability)
	{
		return nullptr;
	}

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return nullptr;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	return ASC->GetSet<USuspenseCoreWeaponAttributeSet>();
}

FVector USuspenseCoreWeaponAsyncTask_PerformTrace::GetMuzzleLocation() const
{
	if (!Ability)
	{
		return FVector::ZeroVector;
	}

	AActor* AvatarActor = GetAvatarActor();
	if (!AvatarActor)
	{
		return FVector::ZeroVector;
	}

	// Try to get muzzle location from weapon interface
	// First check if avatar implements weapon interface directly
	if (ISuspenseCoreWeapon* Weapon = Cast<ISuspenseCoreWeapon>(AvatarActor))
	{
		// Interface would provide muzzle location
		// For now, return actor location as fallback
	}

	// Check owned actors for weapon
	TArray<AActor*> OwnedActors;
	AvatarActor->GetAttachedActors(OwnedActors);

	for (AActor* Attached : OwnedActors)
	{
		if (ISuspenseCoreWeapon* Weapon = Cast<ISuspenseCoreWeapon>(Attached))
		{
			// Would call Weapon->GetMuzzleLocation() if interface had that method
			// Return attached actor location for now
			return Attached->GetActorLocation();
		}
	}

	// Fallback: return actor location + forward offset
	return AvatarActor->GetActorLocation() + (AvatarActor->GetActorForwardVector() * 50.0f);
}

float USuspenseCoreWeaponAsyncTask_PerformTrace::CalculateSpreadModifier() const
{
	if (!Ability)
	{
		return 1.0f;
	}

	AActor* AvatarActor = GetAvatarActor();
	if (!AvatarActor)
	{
		return 1.0f;
	}

	float Modifier = 1.0f;

	// Get character for movement checks
	ACharacter* Character = Cast<ACharacter>(AvatarActor);
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;

	// Get ASC for tag checks
	UAbilitySystemComponent* ASC = nullptr;
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		ASC = ActorInfo->AbilitySystemComponent.Get();
	}

	// Check aiming state via tags
	if (ASC && ASC->HasMatchingGameplayTag(SuspenseCoreTags::State::Aiming))
	{
		Modifier *= AimingModifier;
	}

	// Check crouching
	if (ASC && ASC->HasMatchingGameplayTag(SuspenseCoreTags::State::Crouching))
	{
		Modifier *= CrouchingModifier;
	}

	// Check sprinting
	if (ASC && ASC->HasMatchingGameplayTag(SuspenseCoreTags::State::Sprinting))
	{
		Modifier *= SprintingModifier;
	}

	// Check jumping/in air
	if (Movement && Movement->IsFalling())
	{
		Modifier *= JumpingModifier;
	}
	else if (ASC && ASC->HasMatchingGameplayTag(SuspenseCoreTags::State::Jumping))
	{
		Modifier *= JumpingModifier;
	}

	// Check fire mode tags
	if (ASC && ASC->HasMatchingGameplayTag(SuspenseCoreTags::State::AutoFireActive))
	{
		Modifier *= AutoFireModifier;
	}
	else if (ASC && ASC->HasMatchingGameplayTag(SuspenseCoreTags::State::BurstActive))
	{
		Modifier *= BurstFireModifier;
	}

	// Check movement speed
	if (Movement)
	{
		const float Speed = Movement->Velocity.Size2D();
		if (Speed > MovementThreshold)
		{
			Modifier *= MovementModifier;
		}
	}

	return Modifier;
}
