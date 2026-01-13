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
#include "Engine/CollisionProfile.h"

//==================================================================
// Collision Profile Configuration
// To create the "Weapon" profile in your game project:
// Project Settings -> Collision -> New Profile -> Name: "Weapon"
// Set it to block Pawn, WorldStatic, WorldDynamic, PhysicsBody
// Ignore Visibility, Camera, Vehicle traces
//==================================================================
namespace SuspenseCoreCollision
{
	// Primary weapon trace profile - create this in Project Settings -> Collision
	static const FName WeaponTraceProfile = FName("Weapon");

	// Fallback profile if Weapon not configured
	static const FName FallbackProfile = FName("BlockAllDynamic");

	// Check if collision profile exists in UE5
	inline bool DoesProfileExist(const FName& ProfileName)
	{
		// Get collision response template for the profile - returns false if not found
		FCollisionResponseTemplate ProfileTemplate;
		return UCollisionProfile::Get()->GetProfileTemplate(ProfileName, ProfileTemplate);
	}

	// Get weapon trace profile with fallback
	inline FName GetWeaponTraceProfile()
	{
		if (DoesProfileExist(WeaponTraceProfile))
		{
			return WeaponTraceProfile;
		}

		// Log warning only once per session
		static bool bWarnedOnce = false;
		if (!bWarnedOnce)
		{
			UE_LOG(LogTemp, Warning, TEXT("SuspenseCore: 'Weapon' collision profile not found. Using 'BlockAllDynamic' as fallback. "
				"Create 'Weapon' profile in Project Settings -> Collision for optimal weapon tracing."));
			bWarnedOnce = true;
		}

		return FallbackProfile;
	}
}

USuspenseCoreBaseFireAbility::USuspenseCoreBaseFireAbility()
	: bDebugTraces(false)
	, ConsecutiveShotsCount(0)
	, LastShotTime(0.0f)
{
	// Network configuration
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;

	// Tag configuration - use SetAssetTags() instead of deprecated AbilityTags.AddTag()
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(SuspenseCoreTags::Ability::Weapon::Fire);
	SetAssetTags(AssetTags);

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
	// Check if ability is already active (prevents double-fire)
	if (IsActive())
	{
		return false;
	}

	// Base class check: tags, cooldowns, costs
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// Get combat state interface from character
	ISuspenseCoreWeaponCombatState* CombatState = const_cast<USuspenseCoreBaseFireAbility*>(this)->GetWeaponCombatState();

	// If no combat state interface, allow fire (weapon component may not be equipped yet)
	if (!CombatState)
	{
		return true;
	}

	// Must have weapon drawn to fire
	if (!CombatState->IsWeaponDrawn())
	{
		return false;
	}

	// Cannot fire while reloading (also handled by blocking tags, but explicit check is clearer)
	if (CombatState->IsReloading())
	{
		return false;
	}

	// CRITICAL: Must have ammo to fire
	// Check via ISuspenseCoreWeapon interface for proper Tarkov-style ammo state
	if (!HasAmmo())
	{
		return false;
	}

	return true;
}

void USuspenseCoreBaseFireAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Set firing state via interface (for blocking other abilities)
	if (ISuspenseCoreWeaponCombatState* CombatState = GetWeaponCombatState())
	{
		CombatState->SetFiring(true);
	}

	// Fire first shot - children implement FireNextShot()
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

	// Perform trace using weapon collision profile with automatic fallback
	USuspenseCoreTraceUtils::PerformLineTrace(
		GetAvatarActorFromActorInfo(),
		ShotRequest.StartLocation,
		TraceEnd,
		SuspenseCoreCollision::GetWeaponTraceProfile(),
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
		return;
	}

	ACharacter* Character = Cast<ACharacter>(Avatar);

	// Play fire animation montage
	if (FireMontage && Character)
	{
		if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_Play(FireMontage);
		}
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
	// COMPLETELY DISABLED - need to find what actually causes the jerk
	// If jerk persists with recoil disabled, the problem is elsewhere
	UE_LOG(LogTemp, Warning, TEXT("[FIRE] ApplyRecoil SKIPPED (disabled for debugging)"));
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

	// Get current ammo to verify we have enough
	float CurrentAmmo = Weapon->Execute_GetCurrentAmmo(Cast<UObject>(Weapon));
	if (CurrentAmmo < Amount)
	{
		return false;
	}

	// Actually consume ammo via weapon interface
	// This calls WeaponActor::Fire_Implementation() which calls AmmoComponent::ConsumeAmmo()
	FWeaponFireParams FireParams;
	bool bConsumed = Weapon->Execute_Fire(Cast<UObject>(Weapon), FireParams);

	if (bConsumed)
	{
		// Publish ammo changed event for UI
		PublishAmmoChangedEvent();
	}

	return bConsumed;
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
		// Get MagazineProvider for proper Tarkov-style ammo state
		ISuspenseCoreMagazineProvider* MagProvider = GetMagazineProvider();
		if (!MagProvider)
		{
			return;
		}

		// Get ammo state from MagazineProvider (SSOT)
		FSuspenseCoreWeaponAmmoState AmmoState = ISuspenseCoreMagazineProvider::Execute_GetAmmoState(Cast<UObject>(MagProvider));

		// Build event data in format expected by AmmoCounterWidget
		// Use standard fields that UI widgets expect
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetAvatarActorFromActorInfo());
		EventData.SetInt(TEXT("CurrentRounds"), AmmoState.InsertedMagazine.CurrentRoundCount);
		EventData.SetInt(TEXT("MaxCapacity"), AmmoState.InsertedMagazine.MaxCapacity);
		EventData.SetBool(TEXT("HasChamberedRound"), AmmoState.ChamberedRound.IsChambered());
		EventData.SetString(TEXT("LoadedAmmoType"), AmmoState.InsertedMagazine.LoadedAmmoID.ToString());

		// Publish on BridgeSystem tag - AmmoCounterWidget subscribes to both this
		// and TAG_Equipment_Event_Weapon_AmmoChanged for cross-module compatibility
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
