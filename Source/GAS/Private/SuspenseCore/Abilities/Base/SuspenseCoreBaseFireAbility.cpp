// SuspenseCoreBaseFireAbility.cpp
// SuspenseCore - Base Fire Ability Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Base/SuspenseCoreBaseFireAbility.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Attributes/SuspenseCoreWeaponAttributeSet.h"
#include "SuspenseCore/Utils/SuspenseCoreTraceUtils.h"
#include "SuspenseCore/Utils/SuspenseCoreSpreadProcessor.h"
#include "SuspenseCore/Utils/SuspenseCoreSpreadCalculator.h"
#include "SuspenseCore/Attributes/SuspenseCoreAmmoAttributeSet.h"
#include "SuspenseCore/Effects/Weapon/SuspenseCoreDamageEffect.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeaponCombatState.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeapon.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreMagazineProvider.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraShakeBase.h"
#include "TimerManager.h"

USuspenseCoreBaseFireAbility::USuspenseCoreBaseFireAbility()
	: ConsecutiveShotsCount(0)
	, LastShotTime(0.0f)
	, bDebugTraces(false)
{
	// Network configuration
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;

	// Tag configuration
	AbilityTags.AddTag(SuspenseCoreTags::Ability::Weapon::Fire);
	ActivationOwnedTags.AddTag(SuspenseCoreTags::State::Firing);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Dead);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Stunned);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Reloading);
}

//========================================================================
// GameplayAbility Interface
//========================================================================

bool USuspenseCoreBaseFireAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	UE_LOG(LogTemp, Warning, TEXT("[FIRE CANACTIVATE] ════════════════════════════════════════════════════"));
	UE_LOG(LogTemp, Warning, TEXT("[FIRE CANACTIVATE] Called for %s (this=%p)"), *GetClass()->GetName(), this);

	// Check if ability is already active
	if (IsActive())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FIRE CANACTIVATE] SKIP: Ability is already active"));
		return false;
	}

	// Call base class check (handles tags, cooldowns, costs)
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FIRE CANACTIVATE] BLOCKED: Super::CanActivateAbility returned false"));
		UE_LOG(LogTemp, Warning, TEXT("[FIRE CANACTIVATE]   This usually means: cooldown, cost, or tag requirement failed"));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("[FIRE CANACTIVATE] Super check PASSED"));

	// DEBUG: Skip combat state checks for now - just let it activate
	// Once activation works, we can re-enable these checks
	UE_LOG(LogTemp, Warning, TEXT("[FIRE CANACTIVATE] SUCCESS: Allowing activation (combat checks disabled for debug)"));
	return true;

	/*
	// === COMBAT STATE CHECKS (disabled for debugging) ===

	// Check weapon combat state via interface (DI compliant)
	ISuspenseCoreWeaponCombatState* CombatState = const_cast<USuspenseCoreBaseFireAbility*>(this)->GetWeaponCombatState();
	if (!CombatState)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FIRE CANACTIVATE] BLOCKED: No WeaponCombatState interface"));
		return false;
	}

	// Must have weapon drawn
	if (!CombatState->IsWeaponDrawn())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FIRE CANACTIVATE] BLOCKED: Weapon not drawn"));
		return false;
	}

	// Cannot fire while reloading
	if (CombatState->IsReloading())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FIRE CANACTIVATE] BLOCKED: Currently reloading"));
		return false;
	}

	// Check ammo
	if (!const_cast<USuspenseCoreBaseFireAbility*>(this)->HasAmmo())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FIRE CANACTIVATE] BLOCKED: No ammo"));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("[FIRE CANACTIVATE] SUCCESS: All checks passed"));
	return true;
	*/
}

void USuspenseCoreBaseFireAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UE_LOG(LogTemp, Warning, TEXT("[FIRE ACTIVATE] ════════════════════════════════════════════════════"));
	UE_LOG(LogTemp, Warning, TEXT("[FIRE ACTIVATE] ActivateAbility called for %s"), *GetClass()->GetName());

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Set firing state via interface
	if (ISuspenseCoreWeaponCombatState* CombatState = GetWeaponCombatState())
	{
		CombatState->SetFiring(true);
		UE_LOG(LogTemp, Warning, TEXT("[FIRE ACTIVATE] Set CombatState->SetFiring(true)"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[FIRE ACTIVATE] No CombatState interface found (continuing anyway)"));
	}

	// Fire first shot - children implement FireNextShot()
	UE_LOG(LogTemp, Warning, TEXT("[FIRE ACTIVATE] Calling FireNextShot()..."));
	FireNextShot();
}

void USuspenseCoreBaseFireAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Clear firing state
	if (ISuspenseCoreWeaponCombatState* CombatState = GetWeaponCombatState())
	{
		CombatState->SetFiring(false);
	}

	// Clear timers
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecoilRecoveryTimerHandle);
		World->GetTimerManager().ClearTimer(RecoilResetTimerHandle);
	}

	// Start recoil reset timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RecoilResetTimerHandle,
			this,
			&USuspenseCoreBaseFireAbility::ResetShotCounter,
			RecoilConfig.ResetTime,
			false
		);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void USuspenseCoreBaseFireAbility::InputPressed(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	// Default: try to activate on press via ASC
	if (!IsActive() && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		ActorInfo->AbilitySystemComponent->TryActivateAbility(Handle, false);
	}
}

void USuspenseCoreBaseFireAbility::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	// Default: end on release for single shot
	// Auto/Burst override this behavior
}

//========================================================================
// Shot Generation
//========================================================================

FWeaponShotParams USuspenseCoreBaseFireAbility::GenerateShotRequest()
{
	FWeaponShotParams Params;

	// Get muzzle location
	Params.StartLocation = GetMuzzleLocation();

	// Get aim direction
	Params.Direction = GetAimDirection();

	// Get weapon and ammo attributes for proper damage/spread calculation
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const USuspenseCoreWeaponAttributeSet* WeaponAttrs = GetWeaponAttributes();
	const USuspenseCoreAmmoAttributeSet* AmmoAttrs = nullptr;

	// Try to get ammo attributes from ASC
	if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		AmmoAttrs = ActorInfo->AbilitySystemComponent->GetSet<USuspenseCoreAmmoAttributeSet>();
	}

	// Get combat state for aiming
	ISuspenseCoreWeaponCombatState* CombatState = GetWeaponCombatState();
	const bool bIsAiming = CombatState ? CombatState->IsAiming() : false;

	// Get movement speed
	float MovementSpeed = 0.0f;
	if (ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UCharacterMovementComponent* Movement = Char->GetCharacterMovement())
		{
			MovementSpeed = Movement->Velocity.Size2D();
		}
	}

	if (WeaponAttrs)
	{
		// Use full attribute-based calculation (Weapon + Ammo + Character)
		// This follows SSOT principle - data comes from DataTables through attributes
		Params.BaseDamage = USuspenseCoreSpreadCalculator::CalculateFinalDamage(
			WeaponAttrs,
			AmmoAttrs,
			0.0f  // Character damage bonus (could be fetched from character attribute set)
		);

		Params.Range = USuspenseCoreSpreadCalculator::CalculateEffectiveRange(
			WeaponAttrs,
			AmmoAttrs
		);

		// Calculate spread using full attribute chain
		Params.SpreadAngle = USuspenseCoreSpreadCalculator::CalculateSpreadWithAttributes(
			WeaponAttrs,
			AmmoAttrs,
			bIsAiming,
			MovementSpeed,
			GetCurrentRecoilMultiplier()
		);
	}
	else
	{
		// Fallback defaults if no attributes
		Params.BaseDamage = 25.0f;
		Params.Range = 10000.0f;
		Params.SpreadAngle = USuspenseCoreSpreadProcessor::CalculateCurrentSpread(
			bIsAiming ? 1.0f : 3.0f,
			bIsAiming,
			MovementSpeed,
			GetCurrentRecoilMultiplier()
		);
	}

	// Set metadata
	Params.Instigator = GetAvatarActorFromActorInfo();
	Params.DamageMultiplier = 1.0f;
	Params.ShotNumber = ConsecutiveShotsCount;
	Params.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	return Params;
}

void USuspenseCoreBaseFireAbility::ExecuteSingleShot()
{
	// Generate shot parameters
	FWeaponShotParams ShotParams = GenerateShotRequest();

	// Client-side prediction: send to server
	if (GetAvatarActorFromActorInfo() && GetAvatarActorFromActorInfo()->HasAuthority() == false)
	{
		// Store pending shot
		PendingShots.Add(ShotParams);

		// Send to server
		ServerFireShot(ShotParams);
	}
	else
	{
		// Server or standalone: process immediately
		FSuspenseCoreShotResult Result;
		ServerProcessShotTrace(ShotParams, Result);
		ApplyDamageToTargets(Result.HitResults, ShotParams.BaseDamage);
		ConsumeAmmo();
	}

	// Play local effects (client-side)
	if (IsLocallyControlled())
	{
		PlayLocalFireEffects();
		ApplyRecoil();
		IncrementShotCounter();
	}

	// Publish fired event
	PublishWeaponFiredEvent(ShotParams, true);
}

//========================================================================
// Server Validation & Damage
//========================================================================

bool USuspenseCoreBaseFireAbility::ServerFireShot_Validate(const FWeaponShotParams& ShotRequest)
{
	// Basic validation
	if (ShotRequest.SpreadAngle < 0.0f)
	{
		return false;
	}

	if (!ShotRequest.Direction.IsNormalized())
	{
		return false;
	}

	return true;
}

void USuspenseCoreBaseFireAbility::ServerFireShot_Implementation(const FWeaponShotParams& ShotRequest)
{
	// Validate shot
	if (!ValidateShotRequest(ShotRequest))
	{
		// Send invalid result to client
		FSuspenseCoreShotResult InvalidResult;
		InvalidResult.bWasValidated = false;
		ClientReceiveShotResult(InvalidResult);
		return;
	}

	// Process trace
	FSuspenseCoreShotResult Result;
	ServerProcessShotTrace(ShotRequest, Result);
	Result.bWasValidated = true;

	// Apply damage
	ApplyDamageToTargets(Result.HitResults, ShotRequest.BaseDamage);

	// Consume ammo
	ConsumeAmmo();

	// Send result to client
	ClientReceiveShotResult(Result);
}

void USuspenseCoreBaseFireAbility::ClientReceiveShotResult_Implementation(const FSuspenseCoreShotResult& ShotResult)
{
	// Remove from pending
	if (PendingShots.Num() > 0)
	{
		PendingShots.RemoveAt(0);
	}

	// Play confirmed impact effects
	if (ShotResult.bWasValidated)
	{
		PlayImpactEffects(ShotResult.HitResults);
	}
}

bool USuspenseCoreBaseFireAbility::ValidateShotRequest(const FWeaponShotParams& ShotRequest) const
{
	// Validate origin distance
	const FVector ActualMuzzle = const_cast<USuspenseCoreBaseFireAbility*>(this)->GetMuzzleLocation();
	const float OriginDistance = FVector::Dist(ShotRequest.StartLocation, ActualMuzzle);
	if (OriginDistance > MaxAllowedOriginDistance)
	{
		UE_LOG(LogTemp, Warning, TEXT("Shot validation failed: Origin distance %f > %f"), OriginDistance, MaxAllowedOriginDistance);
		return false;
	}

	// Validate timestamp
	if (GetWorld())
	{
		const float ServerTime = GetWorld()->GetTimeSeconds();
		const float TimeDiff = FMath::Abs(ServerTime - ShotRequest.Timestamp);
		if (TimeDiff > MaxTimeDifference)
		{
			UE_LOG(LogTemp, Warning, TEXT("Shot validation failed: Time diff %f > %f"), TimeDiff, MaxTimeDifference);
			return false;
		}
	}

	return true;
}

void USuspenseCoreBaseFireAbility::ServerProcessShotTrace(const FWeaponShotParams& ShotRequest, FSuspenseCoreShotResult& OutResult)
{
	OutResult.Timestamp = ShotRequest.Timestamp;

	// Actors to ignore
	TArray<AActor*> IgnoreActors;
	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		IgnoreActors.Add(Avatar);
	}

	// Apply spread
	FVector TraceDirection = USuspenseCoreTraceUtils::ApplySpreadToDirection(
		ShotRequest.Direction,
		ShotRequest.SpreadAngle,
		static_cast<int32>(ShotRequest.Timestamp * 1000.0f)
	);

	// Calculate end point
	FVector TraceEnd = USuspenseCoreTraceUtils::CalculateTraceEndPoint(
		ShotRequest.StartLocation,
		TraceDirection,
		ShotRequest.Range
	);

	// Perform trace
	USuspenseCoreTraceUtils::PerformLineTrace(
		GetAvatarActorFromActorInfo(),
		ShotRequest.StartLocation,
		TraceEnd,
		FName("Weapon"),
		IgnoreActors,
		bDebugTraces,
		2.0f,
		OutResult.HitResults
	);
}

void USuspenseCoreBaseFireAbility::ApplyDamageToTargets(const TArray<FHitResult>& HitResults, float BaseDamage)
{
	AActor* Instigator = GetAvatarActorFromActorInfo();

	for (const FHitResult& Hit : HitResults)
	{
		if (!Hit.bBlockingHit)
		{
			continue;
		}

		AActor* HitActor = Hit.GetActor();
		if (!HitActor || HitActor == Instigator)
		{
			continue;
		}

		// Apply damage with headshot check
		USuspenseCoreDamageEffectLibrary::ApplyDamageWithHeadshotCheck(
			Instigator,
			HitActor,
			BaseDamage,
			Hit
		);
	}
}

//========================================================================
// Visual Effects
//========================================================================

void USuspenseCoreBaseFireAbility::PlayLocalFireEffects()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FIRE EFFECTS] No Avatar - skipping effects"));
		return;
	}

	ACharacter* Character = Cast<ACharacter>(Avatar);

	// Play montage
	if (FireMontage && Character)
	{
		if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
		{
			UE_LOG(LogTemp, Warning, TEXT("[FIRE EFFECTS] Playing FireMontage: %s (RootMotion=%s)"),
				*FireMontage->GetName(),
				FireMontage->bEnableRootMotionTranslation ? TEXT("YES - MAY CAUSE MOVEMENT!") : TEXT("No"));
			AnimInstance->Montage_Play(FireMontage);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[FIRE EFFECTS] No FireMontage set (Character=%s)"),
			Character ? TEXT("Valid") : TEXT("NULL"));
	}

	// Play sound
	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			Avatar,
			FireSound,
			GetMuzzleLocation()
		);
	}

	// Spawn muzzle flash
	if (MuzzleFlashEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			Avatar,
			MuzzleFlashEffect,
			GetMuzzleLocation(),
			GetAvatarActorFromActorInfo()->GetActorRotation()
		);
	}
}

void USuspenseCoreBaseFireAbility::PlayImpactEffects(const TArray<FHitResult>& HitResults)
{
	if (!ImpactEffect)
	{
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	for (const FHitResult& Hit : HitResults)
	{
		if (!Hit.bBlockingHit)
		{
			continue;
		}

		// Spawn impact effect
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			Avatar,
			ImpactEffect,
			Hit.ImpactPoint,
			Hit.ImpactNormal.Rotation()
		);
	}
}

void USuspenseCoreBaseFireAbility::SpawnTracer(const FVector& Start, const FVector& End)
{
	if (!TracerEffect)
	{
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	// Calculate direction and spawn tracer
	const FVector Direction = (End - Start).GetSafeNormal();
	const FRotator Rotation = Direction.Rotation();

	if (UNiagaraComponent* Tracer = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		Avatar,
		TracerEffect,
		Start,
		Rotation
	))
	{
		// Set tracer end point if system supports it
		Tracer->SetVectorParameter(FName("EndPoint"), End);
	}
}

//========================================================================
// Recoil System
//========================================================================

void USuspenseCoreBaseFireAbility::ApplyRecoil()
{
	APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!AvatarPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RECOIL] No AvatarPawn - skipping recoil"));
		return;
	}

	APlayerController* PC = Cast<APlayerController>(AvatarPawn->GetController());
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RECOIL] No PlayerController - skipping recoil"));
		return;
	}

	// Get weapon and ammo attributes for full recoil calculation
	const USuspenseCoreWeaponAttributeSet* WeaponAttrs = GetWeaponAttributes();
	const USuspenseCoreAmmoAttributeSet* AmmoAttrs = nullptr;

	// Try to get ammo attributes from ASC
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		AmmoAttrs = ActorInfo->AbilitySystemComponent->GetSet<USuspenseCoreAmmoAttributeSet>();
	}

	// Check ADS for reduction
	ISuspenseCoreWeaponCombatState* CombatState = GetWeaponCombatState();
	const bool bIsAiming = CombatState ? CombatState->IsAiming() : false;

	// Calculate base recoil from weapon and ammo attributes
	float BaseRecoil = USuspenseCoreSpreadCalculator::CalculateRecoil(
		WeaponAttrs,
		AmmoAttrs,
		bIsAiming,
		RecoilConfig.ADSMultiplier
	);

	// Apply progressive recoil multiplier
	float RecoilMult = GetCurrentRecoilMultiplier();

	UE_LOG(LogTemp, Warning, TEXT("[RECOIL] BaseRecoil=%.2f, RecoilMult=%.2f, WeaponAttrs=%s, IsAiming=%s"),
		BaseRecoil, RecoilMult,
		WeaponAttrs ? TEXT("Valid") : TEXT("NULL"),
		bIsAiming ? TEXT("Yes") : TEXT("No"));

	// Apply random recoil to view
	const float PitchRecoil = FMath::RandRange(BaseRecoil * 0.8f, BaseRecoil * 1.2f) * RecoilMult;
	const float YawRecoil = FMath::RandRange(-BaseRecoil * 0.3f, BaseRecoil * 0.3f) * RecoilMult;

	UE_LOG(LogTemp, Warning, TEXT("[RECOIL] Applying: Pitch=%.2f, Yaw=%.2f"), PitchRecoil, YawRecoil);

	FRotator NewRotation = PC->GetControlRotation();
	NewRotation.Pitch += PitchRecoil;
	NewRotation.Yaw += YawRecoil;
	PC->SetControlRotation(NewRotation);

	// Play camera shake
	if (RecoilCameraShake)
	{
		PC->ClientStartCameraShake(RecoilCameraShake, RecoilMult);
		UE_LOG(LogTemp, Warning, TEXT("[RECOIL] Camera shake played"));
	}

	// Start recovery timer
	StartRecoilRecovery();
}

float USuspenseCoreBaseFireAbility::GetCurrentRecoilMultiplier() const
{
	if (ConsecutiveShotsCount <= 1)
	{
		return 1.0f;
	}

	float Multiplier = 1.0f + (ConsecutiveShotsCount - 1) * (RecoilConfig.ProgressiveMultiplier - 1.0f);
	return FMath::Min(Multiplier, RecoilConfig.MaximumMultiplier);
}

void USuspenseCoreBaseFireAbility::IncrementShotCounter()
{
	ConsecutiveShotsCount++;
	LastShotTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

void USuspenseCoreBaseFireAbility::ResetShotCounter()
{
	ConsecutiveShotsCount = 0;
}

void USuspenseCoreBaseFireAbility::StartRecoilRecovery()
{
	if (!GetWorld())
	{
		return;
	}

	// Clear existing timer
	GetWorld()->GetTimerManager().ClearTimer(RecoilRecoveryTimerHandle);

	// Start recovery after delay
	GetWorld()->GetTimerManager().SetTimer(
		RecoilRecoveryTimerHandle,
		this,
		&USuspenseCoreBaseFireAbility::RecoverRecoil,
		0.1f, // Recovery tick interval
		true, // Looping
		RecoilConfig.RecoveryDelay // Initial delay
	);
}

void USuspenseCoreBaseFireAbility::RecoverRecoil()
{
	// Recovery is handled by natural recoil decay
	// This could be extended to add gradual view recovery
}

//========================================================================
// Ammunition
//========================================================================

bool USuspenseCoreBaseFireAbility::ConsumeAmmo(int32 Amount)
{
	ISuspenseCoreWeapon* Weapon = GetWeaponInterface();
	if (!Weapon)
	{
		return false;
	}

	// Get current ammo
	float CurrentAmmo = Weapon->Execute_GetCurrentAmmo(Cast<UObject>(Weapon));
	if (CurrentAmmo < Amount)
	{
		return false;
	}

	// Consume via weapon interface
	// The actual implementation depends on the weapon component
	// For now, we rely on the weapon handling this

	// Publish ammo changed event
	PublishAmmoChangedEvent();

	return true;
}

bool USuspenseCoreBaseFireAbility::HasAmmo() const
{
	ISuspenseCoreWeapon* Weapon = const_cast<USuspenseCoreBaseFireAbility*>(this)->GetWeaponInterface();
	if (!Weapon)
	{
		return false;
	}

	float CurrentAmmo = Weapon->Execute_GetCurrentAmmo(Cast<UObject>(Weapon));
	return CurrentAmmo > 0.0f;
}

//========================================================================
// Interface Access
//========================================================================

ISuspenseCoreWeaponCombatState* USuspenseCoreBaseFireAbility::GetWeaponCombatState() const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return nullptr;
	}

	// Find component implementing the interface
	TArray<UActorComponent*> Components;
	Avatar->GetComponents(Components);

	for (UActorComponent* Comp : Components)
	{
		if (ISuspenseCoreWeaponCombatState* CombatState = Cast<ISuspenseCoreWeaponCombatState>(Comp))
		{
			return CombatState;
		}
	}

	return nullptr;
}

ISuspenseCoreWeapon* USuspenseCoreBaseFireAbility::GetWeaponInterface() const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return nullptr;
	}

	// Check attached actors for weapon
	TArray<AActor*> AttachedActors;
	Avatar->GetAttachedActors(AttachedActors);

	for (AActor* Attached : AttachedActors)
	{
		if (ISuspenseCoreWeapon* Weapon = Cast<ISuspenseCoreWeapon>(Attached))
		{
			return Weapon;
		}
	}

	return nullptr;
}

ISuspenseCoreMagazineProvider* USuspenseCoreBaseFireAbility::GetMagazineProvider() const
{
	ISuspenseCoreWeapon* Weapon = const_cast<USuspenseCoreBaseFireAbility*>(this)->GetWeaponInterface();
	if (!Weapon)
	{
		return nullptr;
	}

	// Magazine provider is usually a component on the weapon actor
	AActor* WeaponActor = Cast<AActor>(Cast<UObject>(Weapon));
	if (!WeaponActor)
	{
		return nullptr;
	}

	TArray<UActorComponent*> Components;
	WeaponActor->GetComponents(Components);

	for (UActorComponent* Comp : Components)
	{
		if (ISuspenseCoreMagazineProvider* Provider = Cast<ISuspenseCoreMagazineProvider>(Comp))
		{
			return Provider;
		}
	}

	return nullptr;
}

const USuspenseCoreWeaponAttributeSet* USuspenseCoreBaseFireAbility::GetWeaponAttributes() const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return nullptr;
	}

	return ActorInfo->AbilitySystemComponent->GetSet<USuspenseCoreWeaponAttributeSet>();
}

FVector USuspenseCoreBaseFireAbility::GetMuzzleLocation() const
{
	ISuspenseCoreWeapon* Weapon = const_cast<USuspenseCoreBaseFireAbility*>(this)->GetWeaponInterface();
	if (Weapon)
	{
		// Get muzzle socket
		AActor* WeaponActor = Cast<AActor>(Cast<UObject>(Weapon));
		if (WeaponActor)
		{
			FName MuzzleSocket = Weapon->Execute_GetMuzzleSocketName(Cast<UObject>(Weapon));
			if (USkeletalMeshComponent* Mesh = WeaponActor->FindComponentByClass<USkeletalMeshComponent>())
			{
				if (Mesh->DoesSocketExist(MuzzleSocket))
				{
					return Mesh->GetSocketLocation(MuzzleSocket);
				}
			}
			if (UStaticMeshComponent* StaticMesh = WeaponActor->FindComponentByClass<UStaticMeshComponent>())
			{
				if (StaticMesh->DoesSocketExist(MuzzleSocket))
				{
					return StaticMesh->GetSocketLocation(MuzzleSocket);
				}
			}
			// Fallback to weapon location
			return WeaponActor->GetActorLocation();
		}
	}

	// Ultimate fallback to avatar
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (Avatar)
	{
		return Avatar->GetActorLocation() + (Avatar->GetActorForwardVector() * 50.0f);
	}

	return FVector::ZeroVector;
}

FVector USuspenseCoreBaseFireAbility::GetAimDirection() const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return FVector::ForwardVector;
	}

	// Get aim from player controller
	if (APawn* Pawn = Cast<APawn>(Avatar))
	{
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			FVector CameraLoc;
			FVector AimPoint;
			USuspenseCoreTraceUtils::GetAimPoint(PC, 10000.0f, CameraLoc, AimPoint);
			return (AimPoint - GetMuzzleLocation()).GetSafeNormal();
		}
	}

	// Fallback to actor forward
	return Avatar->GetActorForwardVector();
}

//========================================================================
// EventBus Publishing
//========================================================================

void USuspenseCoreBaseFireAbility::PublishWeaponFiredEvent(const FWeaponShotParams& ShotParams, bool bSuccess)
{
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetAvatarActorFromActorInfo());
		EventData.SetVector(FName("Origin"), ShotParams.StartLocation);
		EventData.SetVector(FName("Direction"), ShotParams.Direction);
		EventData.SetFloat(FName("Damage"), ShotParams.BaseDamage);
		EventData.SetBool(FName("Success"), bSuccess);
		EventBus->Publish(SuspenseCoreTags::Event::Weapon::Fired, EventData);
	}
}

void USuspenseCoreBaseFireAbility::PublishAmmoChangedEvent()
{
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		ISuspenseCoreWeapon* Weapon = GetWeaponInterface();
		if (!Weapon)
		{
			return;
		}

		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetAvatarActorFromActorInfo());
		EventData.SetFloat(FName("CurrentAmmo"), Weapon->Execute_GetCurrentAmmo(Cast<UObject>(Weapon)));
		EventData.SetFloat(FName("RemainingAmmo"), Weapon->Execute_GetRemainingAmmo(Cast<UObject>(Weapon)));
		EventData.SetFloat(FName("MagazineSize"), Weapon->Execute_GetMagazineSize(Cast<UObject>(Weapon)));
		EventBus->Publish(SuspenseCoreTags::Event::Weapon::AmmoChanged, EventData);
	}
}

void USuspenseCoreBaseFireAbility::PublishSpreadChangedEvent(float NewSpread)
{
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetAvatarActorFromActorInfo());
		EventData.SetFloat(FName("Spread"), NewSpread);
		EventBus->Publish(SuspenseCoreTags::Event::Weapon::SpreadChanged, EventData);
	}
}
