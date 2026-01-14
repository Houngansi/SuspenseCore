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
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Types/GAS/SuspenseCoreGASAttributeRows.h"
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
	, bBoundToWorldTick(false)
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

	// Initialize recoil state from weapon SSOT data (convergence, ergonomics, etc.)
	InitializeRecoilStateFromWeapon();

	// Bind to world tick for convergence updates
	BindToWorldTick();

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

	// DO NOT unbind convergence tick here - convergence should continue after ability ends
	// The timer will auto-unbind when convergence completes (HasOffset() returns false)
	// This allows the camera to smoothly return to aim point even after stopping fire

	// Clear non-convergence timers
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecoilResetTimerHandle);
		World->GetTimerManager().ClearTimer(ConvergenceDelayTimerHandle);
	}

	// Start recoil reset timer (resets shot counter after delay)
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

	// Note: We do NOT unbind from world tick here!
	// Convergence should continue even after firing stops.
	// The tick will automatically unbind when recoil reaches zero.

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

	// Get aim direction - bullets go where camera is looking
	// NO AimOffset applied - this caused issues when player manually adjusted aim
	// Visual/Aim separation is purely cosmetic (camera kick feels stronger)
	// but bullets always go to crosshair
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
// Recoil System (Tarkov-Style with Convergence)
// @see Documentation/Plans/TarkovStyle_Recoil_System_Design.md
//========================================================================

void USuspenseCoreBaseFireAbility::ApplyRecoil()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	// Get player controller for camera shake and view punch
	APawn* Pawn = Cast<APawn>(Avatar);
	if (!Pawn)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC)
	{
		return;
	}

	// Calculate recoil strength based on consecutive shots
	const float RecoilMultiplier = GetCurrentRecoilMultiplier();

	// Apply ADS reduction if aiming
	float ADSMultiplier = 1.0f;
	if (ISuspenseCoreWeaponCombatState* CombatState = GetWeaponCombatState())
	{
		if (CombatState->IsAiming())
		{
			ADSMultiplier = RecoilConfig.ADSMultiplier;
		}
	}

	// Get weapon and ammo attributes for recoil calculation
	const USuspenseCoreWeaponAttributeSet* WeaponAttrs = GetWeaponAttributes();
	const USuspenseCoreAmmoAttributeSet* AmmoAttrs = nullptr;

	// Try to get ammo attributes from ASC (affects recoil based on ammo type)
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		AmmoAttrs = ActorInfo->AbilitySystemComponent->GetSet<USuspenseCoreAmmoAttributeSet>();
	}

	// Calculate final recoil values
	float VerticalRecoil = 0.0f;
	float HorizontalRecoil = 0.0f;

	// ===================================================================================
	// TARKOV-STYLE RECOIL CALCULATION
	// ===================================================================================
	// Formula: FinalRecoil = Base × Ammo × Attachments × PointsToDegrees × Progressive × ADS
	//
	// DataTable stores recoil as "Tarkov-style points" in range 0-500:
	//   - Low recoil: ~50-100 (MP5: 52)
	//   - Medium: ~100-200 (AK-74M: 145, M4A1: 165)
	//   - High: ~200-400 (AKM: 280, SA-58: 350)
	//
	// Conversion: RecoilPoints × 0.002 = Degrees
	//   - 145 points → 0.29° per shot (AK-74M base)
	//   - With PBS-1 suppressor (0.85): 145 × 0.85 × 0.002 = 0.25°
	//   - With full mods (0.727): 145 × 0.727 × 0.002 = 0.21°
	// ===================================================================================
	constexpr float RecoilPointsToDegrees = 0.002f;

	if (WeaponAttrs)
	{
		// Get base recoil values from weapon SSOT
		float BaseVertical = WeaponAttrs->GetVerticalRecoil();
		float BaseHorizontal = WeaponAttrs->GetHorizontalRecoil();

		// Apply ammo modifier (0.5 subsonic - 2.0 hot loads)
		float AmmoModifier = 1.0f;
		if (AmmoAttrs)
		{
			AmmoModifier = AmmoAttrs->GetRecoilModifier();
		}

		// Apply attachment modifiers (multiplicative stack)
		// e.g., Muzzle brake 0.85 × Stock 0.90 × Grip 0.95 = 0.727 total
		float AttachmentModifier = CalculateAttachmentRecoilModifier();

		// Calculate final recoil with all modifiers
		// Formula: Base × AmmoMod × AttachMod × PointsToDegrees × ProgressiveMod × ADSMod
		VerticalRecoil = BaseVertical * AmmoModifier * AttachmentModifier * RecoilPointsToDegrees * RecoilMultiplier * ADSMultiplier;
		HorizontalRecoil = BaseHorizontal * AmmoModifier * AttachmentModifier * RecoilPointsToDegrees * RecoilMultiplier * ADSMultiplier;

		// ===================================================================================
		// RECOIL PATTERN SYSTEM (Phase 6)
		// ===================================================================================
		// Blend between pattern-based recoil and random recoil based on PatternStrength
		// PatternStrength 0.0: Pure random (unpredictable)
		// PatternStrength 0.5: 50% pattern, 50% random (semi-predictable)
		// PatternStrength 1.0: Pure pattern (fully learnable like CS:GO)
		//
		// The pattern provides a predictable sequence that skilled players can learn
		// to compensate, creating a skill ceiling while still being accessible
		// ===================================================================================
		float PatternStrength = RecoilState.CachedPatternStrength;

		if (PatternStrength > 0.01f && RecoilPattern.Points.Num() > 0)
		{
			// Get pattern point for current shot (0-indexed, so use ConsecutiveShotsCount-1)
			int32 ShotIndex = FMath::Max(0, ConsecutiveShotsCount - 1);
			FSuspenseCoreRecoilPatternPoint PatternPoint = RecoilPattern.GetPointForShot(ShotIndex);

			// Pattern recoil: use pattern multipliers on base values
			float PatternVertical = VerticalRecoil * PatternPoint.PitchOffset;
			float PatternHorizontal = BaseHorizontal * AmmoModifier * AttachmentModifier *
				RecoilPointsToDegrees * RecoilMultiplier * ADSMultiplier * PatternPoint.YawOffset;

			// Random recoil: add variance for unpredictability
			float RandomVertical = VerticalRecoil * (1.0f + FMath::FRandRange(-0.1f, 0.1f));
			float RandomHorizontal = HorizontalRecoil;

			// Apply horizontal recoil bias from weapon (some weapons kick left/right consistently)
			float Bias = RecoilState.CachedRecoilBias;
			if (FMath::Abs(Bias) > 0.01f)
			{
				float RandomComponent = FMath::FRandRange(-1.0f, 1.0f);
				float BiasedComponent = (Bias > 0.0f) ? FMath::FRandRange(0.0f, 1.0f) : FMath::FRandRange(-1.0f, 0.0f);
				RandomHorizontal *= FMath::Lerp(RandomComponent, BiasedComponent, FMath::Abs(Bias));
			}
			else
			{
				RandomHorizontal *= FMath::FRandRange(-1.0f, 1.0f);
			}

			// Blend pattern and random based on PatternStrength
			VerticalRecoil = FMath::Lerp(RandomVertical, PatternVertical, PatternStrength);
			HorizontalRecoil = FMath::Lerp(RandomHorizontal, PatternHorizontal, PatternStrength);
		}
		else
		{
			// Pure random recoil (PatternStrength = 0 or no pattern defined)
			// Apply horizontal recoil bias from weapon (some weapons kick left/right consistently)
			float Bias = RecoilState.CachedRecoilBias;
			if (FMath::Abs(Bias) > 0.01f)
			{
				// Blend between full random and biased direction
				float RandomComponent = FMath::FRandRange(-1.0f, 1.0f);
				float BiasedComponent = (Bias > 0.0f) ? FMath::FRandRange(0.0f, 1.0f) : FMath::FRandRange(-1.0f, 0.0f);
				HorizontalRecoil *= FMath::Lerp(RandomComponent, BiasedComponent, FMath::Abs(Bias));
			}
			else
			{
				// Pure random horizontal direction
				HorizontalRecoil *= FMath::FRandRange(-1.0f, 1.0f);
			}
		}
	}
	else
	{
		// Fallback defaults (already in degrees, no conversion needed)
		VerticalRecoil = 0.3f * RecoilMultiplier * ADSMultiplier;
		HorizontalRecoil = FMath::FRandRange(-0.1f, 0.1f) * RecoilMultiplier * ADSMultiplier;
	}

	// ===================================================================================
	// VISUAL VS AIM RECOIL SEPARATION (Phase 5)
	// ===================================================================================
	// Visual recoil: What player SEES (camera kick, stronger for feel)
	// Aim recoil: Where bullets GO (actual aim offset, more stable)
	//
	// VisualRecoilMultiplier (default 1.5) makes camera kick feel 50% stronger
	// But actual bullet spread (AimRecoil) is the base recoil value
	// This creates the Tarkov "feel" - dramatic visual feedback but stable aim
	// ===================================================================================

	// Calculate visual recoil (stronger for dramatic effect)
	float VisualVertical = VerticalRecoil * RecoilConfig.VisualRecoilMultiplier;
	float VisualHorizontal = HorizontalRecoil * RecoilConfig.VisualRecoilMultiplier;

	// Aim recoil stays at base value (where bullets actually go)
	float AimVertical = VerticalRecoil;
	float AimHorizontal = HorizontalRecoil;

	// ===================================================================================
	// APPLY VISUAL RECOIL TO CAMERA
	// ===================================================================================
	// Apply view punch immediately (camera rotation - the dramatic visual effect)
	// In UE: AddPitchInput with NEGATIVE value = look UP (muzzle rise)
	PC->AddPitchInput(-VisualVertical);
	PC->AddYawInput(VisualHorizontal);

	// ===================================================================================
	// CONVERGENCE TRACKING
	// ===================================================================================
	// Track visual offset (what camera shows) - positive = kicked up
	RecoilState.VisualPitch += VisualVertical;
	RecoilState.VisualYaw += VisualHorizontal;

	// NOTE: AimOffset is NOT used anymore - bullets go where camera looks
	// Visual/Aim separation is purely cosmetic (stronger camera kick feel)
	// AimPitch/AimYaw are kept at 0 for HasOffset() check to work correctly

	// Legacy compatibility (deprecated but maintained for existing code)
	RecoilState.AccumulatedPitch += VerticalRecoil;
	RecoilState.AccumulatedYaw += HorizontalRecoil;

	RecoilState.TimeSinceLastShot = 0.0f;
	RecoilState.bWaitingForConvergence = true;
	RecoilState.bIsConverging = false;

	// Start convergence delay timer
	StartConvergenceTimer();

	// Play camera shake if configured
	if (RecoilCameraShake)
	{
		PC->ClientStartCameraShake(RecoilCameraShake, RecoilMultiplier * ADSMultiplier);
	}
}

void USuspenseCoreBaseFireAbility::TickConvergence(float DeltaTime)
{
	// Skip if no offset to converge
	if (!RecoilState.HasOffset())
	{
		// Convergence complete - unbind from tick to save performance
		if (bBoundToWorldTick)
		{
			UE_LOG(LogTemp, Log, TEXT("Convergence: Complete, unbinding. VisualPitch=%.3f"), RecoilState.VisualPitch);
			UnbindFromWorldTick();
		}
		return;
	}

	// Update time since last shot
	RecoilState.TimeSinceLastShot += DeltaTime;

	// Wait for convergence delay before starting recovery
	if (RecoilState.bWaitingForConvergence)
	{
		if (RecoilState.TimeSinceLastShot >= RecoilState.CachedConvergenceDelay)
		{
			UE_LOG(LogTemp, Log, TEXT("Convergence: Delay complete, starting recovery. VisualPitch=%.3f"), RecoilState.VisualPitch);
			RecoilState.bWaitingForConvergence = false;
			RecoilState.bIsConverging = true;
		}
		return;
	}

	// Get player controller
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(Avatar);
	if (!Pawn)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC)
	{
		return;
	}

	// ===================================================================================
	// VISUAL VS AIM CONVERGENCE (Phase 5)
	// ===================================================================================
	// Visual recoil converges FASTER (VisualConvergenceMultiplier, default 1.2×)
	// This makes camera feel stable quickly while aim still drifts
	//
	// Aim recoil converges at base speed - this affects actual bullet accuracy
	// The separation creates the Tarkov "feel" where gun kicks visually but
	// aim recovery is more gradual
	// ===================================================================================

	float EffectiveSpeed = RecoilState.GetEffectiveConvergenceSpeed();
	float BaseConvergenceRate = EffectiveSpeed * DeltaTime;

	// Visual recoil converges faster than aim recoil
	float VisualConvergenceRate = BaseConvergenceRate * RecoilConfig.VisualConvergenceMultiplier;
	float AimConvergenceRate = BaseConvergenceRate;

	// ===================================================================================
	// VISUAL RECOIL RECOVERY (Camera feel)
	// ===================================================================================
	float VisualPitchRecovery = 0.0f;
	float VisualYawRecovery = 0.0f;

	if (RecoilState.HasVisualOffset())
	{
		// Converge visual toward zero
		if (FMath::Abs(RecoilState.VisualPitch) > 0.01f)
		{
			VisualPitchRecovery = -FMath::Sign(RecoilState.VisualPitch) *
				FMath::Min(VisualConvergenceRate, FMath::Abs(RecoilState.VisualPitch));
		}

		if (FMath::Abs(RecoilState.VisualYaw) > 0.01f)
		{
			VisualYawRecovery = -FMath::Sign(RecoilState.VisualYaw) *
				FMath::Min(VisualConvergenceRate, FMath::Abs(RecoilState.VisualYaw));
		}

		// Apply visual recovery to camera
		if (!FMath::IsNearlyZero(VisualPitchRecovery) || !FMath::IsNearlyZero(VisualYawRecovery))
		{
			// VisualPitch is positive (camera kicked UP from recoil)
			// VisualPitchRecovery is negative (need to return camera DOWN toward center)
			// Since recoil used AddPitchInput(-VisualVertical) to go UP,
			// recovery needs AddPitchInput(+value) to go DOWN
			// VisualPitchRecovery is already negative, so we negate it to get positive
			UE_LOG(LogTemp, Verbose, TEXT("Convergence: Applying recovery. PitchRecovery=%.4f, VisualPitch=%.3f"),
				VisualPitchRecovery, RecoilState.VisualPitch);
			PC->AddPitchInput(-VisualPitchRecovery);
			PC->AddYawInput(VisualYawRecovery);

			// Update visual offset state
			RecoilState.VisualPitch += VisualPitchRecovery;
			RecoilState.VisualYaw += VisualYawRecovery;
		}

		// Snap to zero if very small
		if (FMath::IsNearlyZero(RecoilState.VisualPitch, 0.01f))
		{
			RecoilState.VisualPitch = 0.0f;
		}
		if (FMath::IsNearlyZero(RecoilState.VisualYaw, 0.01f))
		{
			RecoilState.VisualYaw = 0.0f;
		}
	}

	// NOTE: AimOffset recovery removed - AimOffset no longer used for bullet direction
	// Bullets go where camera looks, so only visual convergence matters

	// ===================================================================================
	// LEGACY COMPATIBILITY (maintain AccumulatedPitch/Yaw for existing code)
	// ===================================================================================
	float PitchDelta = RecoilState.TargetPitch - RecoilState.AccumulatedPitch;
	float YawDelta = RecoilState.TargetYaw - RecoilState.AccumulatedYaw;

	if (FMath::Abs(PitchDelta) > 0.01f)
	{
		float PitchRecovery = FMath::Sign(PitchDelta) *
			FMath::Min(BaseConvergenceRate, FMath::Abs(PitchDelta));
		RecoilState.AccumulatedPitch += PitchRecovery;
	}

	if (FMath::Abs(YawDelta) > 0.01f)
	{
		float YawRecovery = FMath::Sign(YawDelta) *
			FMath::Min(BaseConvergenceRate, FMath::Abs(YawDelta));
		RecoilState.AccumulatedYaw += YawRecovery;
	}

	// Snap legacy values to zero if very small
	if (FMath::IsNearlyZero(RecoilState.AccumulatedPitch, 0.01f))
	{
		RecoilState.AccumulatedPitch = 0.0f;
	}
	if (FMath::IsNearlyZero(RecoilState.AccumulatedYaw, 0.01f))
	{
		RecoilState.AccumulatedYaw = 0.0f;
	}

	// Check if convergence is complete
	if (!RecoilState.HasOffset())
	{
		RecoilState.bIsConverging = false;
		RecoilState.Reset();
	}
}

void USuspenseCoreBaseFireAbility::InitializeRecoilStateFromWeapon()
{
	const USuspenseCoreWeaponAttributeSet* WeaponAttrs = GetWeaponAttributes();

	if (WeaponAttrs)
	{
		// Cache values from weapon SSOT for performance
		RecoilState.CachedConvergenceSpeed = WeaponAttrs->GetConvergenceSpeed();
		RecoilState.CachedConvergenceDelay = WeaponAttrs->GetConvergenceDelay();
		RecoilState.CachedErgonomics = WeaponAttrs->GetErgonomics();
		RecoilState.CachedRecoilBias = WeaponAttrs->GetRecoilAngleBias();
		RecoilState.CachedPatternStrength = WeaponAttrs->GetRecoilPatternStrength();
	}
	else
	{
		// Use defaults if no weapon attributes available
		RecoilState.CachedConvergenceSpeed = 5.0f;
		RecoilState.CachedConvergenceDelay = 0.1f;
		RecoilState.CachedErgonomics = 42.0f;
		RecoilState.CachedRecoilBias = 0.0f;
		RecoilState.CachedPatternStrength = 0.3f; // Default: 30% pattern, 70% random
	}
}

float USuspenseCoreBaseFireAbility::CalculateAttachmentRecoilModifier() const
{
	// Get weapon interface to access installed attachments
	ISuspenseCoreWeapon* Weapon = const_cast<USuspenseCoreBaseFireAbility*>(this)->GetWeaponInterface();
	if (!Weapon)
	{
		return 1.0f;
	}

	// Get installed attachments from weapon
	FSuspenseCoreInstalledAttachments InstalledAttachments =
		ISuspenseCoreWeapon::Execute_GetInstalledAttachments(Cast<UObject>(Weapon));

	// If no attachments installed, return 1.0 (no modification)
	if (!InstalledAttachments.HasAnyAttachments())
	{
		return 1.0f;
	}

	// Get DataManager to lookup attachment SSOT data
	USuspenseCoreDataManager* DataManager = USuspenseCoreDataManager::Get(GetAvatarActorFromActorInfo());
	if (!DataManager)
	{
		return 1.0f;
	}

	// Multiply all attachment recoil modifiers (Tarkov-style)
	// e.g., Muzzle brake (0.85) × Stock (0.90) × Grip (0.95) = 0.727 total
	// @see Documentation/Plans/TarkovStyle_Recoil_System_Design.md Section 5.2
	float TotalModifier = 1.0f;

	for (const FSuspenseCoreAttachmentInstance& Attachment : InstalledAttachments.Attachments)
	{
		if (!Attachment.IsInstalled())
		{
			continue;
		}

		// Lookup attachment SSOT data from AttachmentAttributesDataTable
		FSuspenseCoreAttachmentAttributeRow AttachmentData;
		if (DataManager->GetAttachmentAttributes(Attachment.AttachmentID, AttachmentData))
		{
			// Apply recoil modifier from SSOT (multiplicative stacking)
			// RecoilModifier: 0.85 = -15% recoil, 1.0 = no change, 1.2 = +20% recoil
			if (AttachmentData.AffectsRecoil())
			{
				TotalModifier *= AttachmentData.RecoilModifier;

				UE_LOG(LogTemp, Verbose, TEXT("Attachment '%s' recoil modifier: %.2f (total: %.3f)"),
					*Attachment.AttachmentID.ToString(), AttachmentData.RecoilModifier, TotalModifier);
			}
		}
		else
		{
			// Fallback: if SSOT data not found, log warning but continue
			// This allows the system to work even without full DataTable configuration
			UE_LOG(LogTemp, Warning, TEXT("Attachment '%s' not found in SSOT - using default modifier 1.0"),
				*Attachment.AttachmentID.ToString());
		}
	}

	return TotalModifier;
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

void USuspenseCoreBaseFireAbility::StartConvergenceTimer()
{
	// Clear any existing convergence delay timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ConvergenceDelayTimerHandle);
	}

	// Convergence is now handled by world tick, not timer
	// The delay is managed in TickConvergence via TimeSinceLastShot
}

void USuspenseCoreBaseFireAbility::BindToWorldTick()
{
	if (bBoundToWorldTick)
	{
		UE_LOG(LogTemp, Warning, TEXT("Convergence: BindToWorldTick skipped - already bound"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("Convergence: BindToWorldTick failed - no World"));
		return;
	}

	// Use looping timer for convergence updates (60Hz tick rate)
	// This replaces direct world tick delegate for better compatibility
	constexpr float ConvergenceTickRate = 1.0f / 60.0f; // 60 FPS tick rate

	World->GetTimerManager().SetTimer(
		ConvergenceTickTimerHandle,
		this,
		&USuspenseCoreBaseFireAbility::OnConvergenceTick,
		ConvergenceTickRate,
		true // bLoop
	);

	bBoundToWorldTick = true;
	UE_LOG(LogTemp, Log, TEXT("Convergence: Timer started (60Hz)"));
}

void USuspenseCoreBaseFireAbility::UnbindFromWorldTick()
{
	if (!bBoundToWorldTick)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(ConvergenceTickTimerHandle);
	}

	bBoundToWorldTick = false;
}

void USuspenseCoreBaseFireAbility::OnConvergenceTick()
{
	// Check if we should tick convergence
	// NOTE: Cannot use IsLocallyControlled() here - ability may be inactive after EndAbility
	// Instead, check the actor directly
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		UE_LOG(LogTemp, Warning, TEXT("Convergence: No Avatar, unbinding"));
		UnbindFromWorldTick();
		return;
	}

	APawn* Pawn = Cast<APawn>(Avatar);
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		UE_LOG(LogTemp, Warning, TEXT("Convergence: Not locally controlled"));
		return;
	}

	// Get delta time from timer rate (60Hz = ~0.0167 seconds)
	constexpr float DeltaTime = 1.0f / 60.0f;
	TickConvergence(DeltaTime);
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
