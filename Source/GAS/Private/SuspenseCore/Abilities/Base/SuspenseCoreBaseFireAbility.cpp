// SuspenseCoreBaseFireAbility.cpp
// SuspenseCore - Base Fire Ability Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Base/SuspenseCoreBaseFireAbility.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Attributes/SuspenseCoreWeaponAttributeSet.h"
#include "SuspenseCore/Utils/SuspenseCoreTraceUtils.h"
#include "SuspenseCore/Utils/SuspenseCoreSpreadProcessor.h"
#include "SuspenseCore/Utils/SuspenseCoreSpreadCalculator.h"
#include "SuspenseCore/Core/SuspenseCoreUnits.h"
#include "SuspenseCore/Attributes/SuspenseCoreAmmoAttributeSet.h"
#include "SuspenseCore/Effects/Weapon/SuspenseCoreDamageEffect.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeaponCombatState.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeapon.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreMagazineProvider.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Types/GAS/SuspenseCoreGASAttributeRows.h"
// NOTE: Convergence component is in PlayerCore module
// Fire ability communicates via EventBus (decoupled)
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraShakeBase.h"
#include "TimerManager.h"
#include "Engine/CollisionProfile.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"

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
		// Play empty magazine click sound
		const_cast<USuspenseCoreBaseFireAbility*>(this)->PlayEmptySound();
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

	// Fire first shot - children implement FireNextShot()
	// NOTE: Convergence is handled by USuspenseCoreRecoilConvergenceComponent on Character
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

	// Clear recoil reset timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecoilResetTimerHandle);
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

	// NOTE: Convergence continues via USuspenseCoreRecoilConvergenceComponent
	// Component lives on Character and is independent of ability lifecycle

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

		// CRITICAL: Use CalculateMaxTraceRange() for trace distance!
		// This function:
		// 1. Uses MaxRange (maximum bullet travel), NOT EffectiveRange (damage falloff)
		// 2. Converts from meters (DataTable) to UE units (trace)
		// Example: MaxRange 600m → 60000 UE units
		//
		// @see SuspenseCoreUnits::ConvertRangeToUnits()
		// @see Documentation/GAS/UnitConversionSystem.md
		Params.Range = USuspenseCoreSpreadCalculator::CalculateMaxTraceRange(
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
		// Using SuspenseCoreUnits constants for consistency
		Params.BaseDamage = 25.0f;
		Params.Range = SuspenseCoreUnits::DefaultTraceRangeUnits; // 10km in UE units
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

		// Publish camera shake event for weapon fire
		if (USuspenseCoreEventBus* EventBus = GetEventBus())
		{
			FSuspenseCoreEventData ShakeData = FSuspenseCoreEventData::Create(
				GetAvatarActorFromActorInfo(),
				ESuspenseCoreEventPriority::Normal);
			ShakeData.SetString(TEXT("Type"), TEXT("Rifle"));  // Default weapon type
			ShakeData.SetFloat(TEXT("Scale"), 1.0f);
			EventBus->Publish(SuspenseCoreTags::Event::Camera::ShakeWeapon, ShakeData);
		}

		// Publish spread change for crosshair widget
		PublishSpreadChangedEvent(ShotParams.SpreadAngle);
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

	// Play fire sound at muzzle location
	if (FireSound)
	{
		FVector MuzzleLocation = GetWeaponSocketLocation(MuzzleSocketName);
		UGameplayStatics::PlaySoundAtLocation(
			Avatar,
			FireSound,
			MuzzleLocation
		);
	}

	// Spawn muzzle flash - Niagara (preferred)
	if (MuzzleFlashEffect)
	{
		FTransform MuzzleTransform = GetWeaponSocketTransform(MuzzleSocketName);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			Avatar,
			MuzzleFlashEffect,
			MuzzleTransform.GetLocation(),
			MuzzleTransform.GetRotation().Rotator()
		);
	}
	// Spawn muzzle flash - Cascade (fallback/alternative)
	else if (MuzzleFlashCascade)
	{
		SpawnMuzzleFlashCascade();
	}

	// Schedule shell casing sound with slight delay (realistic timing)
	if (ShellSound)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				ShellSoundTimerHandle,
				this,
				&USuspenseCoreBaseFireAbility::PlayShellSound,
				0.1f,  // 100ms delay for shell to eject and hit ground
				false
			);
		}
	}
}

void USuspenseCoreBaseFireAbility::PlayImpactEffects(const TArray<FHitResult>& HitResults)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	// Need at least one effect type
	if (!ImpactEffect && !ImpactCascade)
	{
		return;
	}

	for (const FHitResult& Hit : HitResults)
	{
		if (!Hit.bBlockingHit)
		{
			continue;
		}

		// Spawn impact effect - Niagara (preferred)
		if (ImpactEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				Avatar,
				ImpactEffect,
				Hit.ImpactPoint,
				Hit.ImpactNormal.Rotation()
			);
		}
		// Spawn impact effect - Cascade (fallback)
		else if (ImpactCascade)
		{
			SpawnImpactCascade(Hit.ImpactPoint, Hit.ImpactNormal);
		}
	}
}

void USuspenseCoreBaseFireAbility::SpawnTracer(const FVector& Start, const FVector& End)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	// Calculate direction and spawn tracer
	const FVector Direction = (End - Start).GetSafeNormal();
	const FRotator Rotation = Direction.Rotation();

	// Spawn tracer - Niagara (preferred)
	if (TracerEffect)
	{
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
	// Spawn tracer - Cascade (fallback)
	else if (TracerCascade)
	{
		SpawnTracerCascade(Start, End);
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
	// PUBLISH RECOIL EVENT VIA EVENTBUS
	// ===================================================================================
	// Convergence (camera return to aim point) is handled by component on Character
	// Component subscribes to Event::Weapon::RecoilImpulse - fully decoupled
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetAvatarActorFromActorInfo());
		EventData.SetFloat(TEXT("PitchImpulse"), VisualVertical);
		EventData.SetFloat(TEXT("YawImpulse"), VisualHorizontal);
		EventData.SetFloat(TEXT("ConvergenceDelay"), RecoilState.CachedConvergenceDelay);
		EventData.SetFloat(TEXT("ConvergenceSpeed"), RecoilState.CachedConvergenceSpeed);
		EventData.SetFloat(TEXT("Ergonomics"), RecoilState.CachedErgonomics);

		EventBus->Publish(SuspenseCoreTags::Event::Weapon::RecoilImpulse, EventData);

		UE_LOG(LogTemp, Log, TEXT("RecoilImpulse: Published via EventBus. Pitch=%.3f, Yaw=%.3f"),
			VisualVertical, VisualHorizontal);
	}

	// Track state for debugging/UI (not used for convergence)
	RecoilState.VisualPitch += VisualVertical;
	RecoilState.VisualYaw += VisualHorizontal;
	RecoilState.AccumulatedPitch += VerticalRecoil;
	RecoilState.AccumulatedYaw += HorizontalRecoil;

	// Play camera shake if configured
	if (RecoilCameraShake)
	{
		PC->ClientStartCameraShake(RecoilCameraShake, RecoilMultiplier * ADSMultiplier);
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
// New Audio/Visual Effect Methods
//========================================================================

void USuspenseCoreBaseFireAbility::PlayEmptySound()
{
	if (!EmptySound)
	{
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	// Play empty click at weapon location
	FVector MuzzleLocation = GetWeaponSocketLocation(MuzzleSocketName);
	UGameplayStatics::PlaySoundAtLocation(
		Avatar,
		EmptySound,
		MuzzleLocation,
		FRotator::ZeroRotator,
		1.0f,  // Volume
		1.0f,  // Pitch
		0.0f,  // Start time
		nullptr,
		nullptr,
		Avatar
	);
}

void USuspenseCoreBaseFireAbility::PlayShellSound()
{
	if (!ShellSound)
	{
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	// Play shell casing sound at Shells socket location
	FVector ShellLocation = GetWeaponSocketLocation(ShellSocketName);
	UGameplayStatics::PlaySoundAtLocation(
		Avatar,
		ShellSound,
		ShellLocation,
		FRotator::ZeroRotator,
		0.7f,  // Volume (quieter than gunshot)
		FMath::FRandRange(0.9f, 1.1f),  // Random pitch for variety
		0.0f,
		nullptr,
		nullptr,
		Avatar
	);
}

void USuspenseCoreBaseFireAbility::SpawnMuzzleFlashCascade()
{
	if (!MuzzleFlashCascade)
	{
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	// Get muzzle socket transform
	FTransform MuzzleTransform = GetWeaponSocketTransform(MuzzleSocketName);

	// Spawn Cascade particle at Muzzle socket
	UGameplayStatics::SpawnEmitterAtLocation(
		Avatar->GetWorld(),
		MuzzleFlashCascade,
		MuzzleTransform.GetLocation(),
		MuzzleTransform.GetRotation().Rotator(),
		MuzzleTransform.GetScale3D(),
		true,  // Auto destroy
		EPSCPoolMethod::AutoRelease
	);
}

void USuspenseCoreBaseFireAbility::SpawnTracerCascade(const FVector& Start, const FVector& End)
{
	if (!TracerCascade)
	{
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	// Calculate direction
	const FVector Direction = (End - Start).GetSafeNormal();
	const FRotator Rotation = Direction.Rotation();

	// Spawn Cascade tracer
	if (UParticleSystemComponent* TracerComp = UGameplayStatics::SpawnEmitterAtLocation(
		Avatar->GetWorld(),
		TracerCascade,
		Start,
		Rotation,
		FVector::OneVector,
		true,
		EPSCPoolMethod::AutoRelease
	))
	{
		// Set beam end point if supported
		TracerComp->SetBeamEndPoint(0, End);
	}
}

void USuspenseCoreBaseFireAbility::SpawnImpactCascade(const FVector& Location, const FVector& Normal)
{
	if (!ImpactCascade)
	{
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	// Spawn Cascade impact effect
	UGameplayStatics::SpawnEmitterAtLocation(
		Avatar->GetWorld(),
		ImpactCascade,
		Location,
		Normal.Rotation(),
		FVector::OneVector,
		true,
		EPSCPoolMethod::AutoRelease
	);
}

UMeshComponent* USuspenseCoreBaseFireAbility::GetWeaponMeshComponent() const
{
	ISuspenseCoreWeapon* Weapon = const_cast<USuspenseCoreBaseFireAbility*>(this)->GetWeaponInterface();
	if (!Weapon)
	{
		return nullptr;
	}

	AActor* WeaponActor = Cast<AActor>(Cast<UObject>(Weapon));
	if (!WeaponActor)
	{
		return nullptr;
	}

	// Try skeletal mesh first (most weapons)
	if (USkeletalMeshComponent* SkelMesh = WeaponActor->FindComponentByClass<USkeletalMeshComponent>())
	{
		return SkelMesh;
	}

	// Fallback to static mesh
	return WeaponActor->FindComponentByClass<UStaticMeshComponent>();
}

FVector USuspenseCoreBaseFireAbility::GetWeaponSocketLocation(FName SocketName) const
{
	if (UMeshComponent* Mesh = GetWeaponMeshComponent())
	{
		if (Mesh->DoesSocketExist(SocketName))
		{
			return Mesh->GetSocketLocation(SocketName);
		}
	}

	// Fallback to weapon actor location
	ISuspenseCoreWeapon* Weapon = const_cast<USuspenseCoreBaseFireAbility*>(this)->GetWeaponInterface();
	if (Weapon)
	{
		if (AActor* WeaponActor = Cast<AActor>(Cast<UObject>(Weapon)))
		{
			return WeaponActor->GetActorLocation();
		}
	}

	// Ultimate fallback - avatar forward
	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		return Avatar->GetActorLocation() + Avatar->GetActorForwardVector() * 50.0f;
	}

	return FVector::ZeroVector;
}

FTransform USuspenseCoreBaseFireAbility::GetWeaponSocketTransform(FName SocketName) const
{
	if (UMeshComponent* Mesh = GetWeaponMeshComponent())
	{
		if (Mesh->DoesSocketExist(SocketName))
		{
			return Mesh->GetSocketTransform(SocketName);
		}
	}

	// Fallback to weapon actor transform
	ISuspenseCoreWeapon* Weapon = const_cast<USuspenseCoreBaseFireAbility*>(this)->GetWeaponInterface();
	if (Weapon)
	{
		if (AActor* WeaponActor = Cast<AActor>(Cast<UObject>(Weapon)))
		{
			return WeaponActor->GetActorTransform();
		}
	}

	return FTransform::Identity;
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
		EventData.SetFloat(FName("Spread"), ShotParams.SpreadAngle);
		EventData.SetFloat(FName("RecoilKick"), 2.0f);  // Visual kick for crosshair
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
