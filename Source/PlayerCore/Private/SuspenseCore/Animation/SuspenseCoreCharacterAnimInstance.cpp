// SuspenseCoreCharacterAnimInstance.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Animation/SuspenseCoreCharacterAnimInstance.h"
#include "SuspenseCore/Characters/SuspenseCoreCharacter.h"
#include "SuspenseCore/Settings/SuspenseCoreSettings.h"
#include "SuspenseCore/Attributes/SuspenseCoreMovementAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/DataTable.h"
#include "Kismet/KismetMathLibrary.h"

#if WITH_EQUIPMENT_SYSTEM
#include "SuspenseCore/Components/SuspenseCoreWeaponStanceComponent.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeaponAnimation.h"
#endif

USuspenseCoreCharacterAnimInstance::USuspenseCoreCharacterAnimInstance()
{
	// Default values are already set in header
}

void USuspenseCoreCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// Initial cache update
	UpdateCachedReferences();

	// Load DataTable from settings
	LoadWeaponAnimationsTable();
}

void USuspenseCoreCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Skip if no valid pawn
	if (!TryGetPawnOwner())
	{
		return;
	}

	// Periodically update cached references
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if ((CurrentTime - LastCacheUpdateTime) > CacheUpdateInterval)
	{
		UpdateCachedReferences();
		LastCacheUpdateTime = CurrentTime;
	}

	// Update all animation data
	UpdateMovementData(DeltaSeconds);
	UpdateVelocityData(DeltaSeconds);
	UpdateWeaponData(DeltaSeconds);
	UpdateAnimationAssets();
	UpdateIKData(DeltaSeconds);
	UpdateAimOffsetData(DeltaSeconds);
	UpdatePoseStates(DeltaSeconds);
	UpdateGASAttributes();
}

void USuspenseCoreCharacterAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	// Thread-safe calculations can go here if needed
	// Currently all updates are in NativeUpdateAnimation
}

void USuspenseCoreCharacterAnimInstance::UpdateCachedReferences()
{
	APawn* OwnerPawn = TryGetPawnOwner();
	if (!OwnerPawn)
	{
		return;
	}

	// Cache character
	CachedCharacter = Cast<ASuspenseCoreCharacter>(OwnerPawn);

	// Cache movement component
	if (ACharacter* Character = Cast<ACharacter>(OwnerPawn))
	{
		CachedMovementComponent = Character->GetCharacterMovement();
	}

#if WITH_EQUIPMENT_SYSTEM
	// Cache stance component
	CachedStanceComponent = OwnerPawn->FindComponentByClass<USuspenseCoreWeaponStanceComponent>();
#endif

	// Cache ASC (from PlayerState or Pawn)
	if (CachedCharacter.IsValid())
	{
		CachedASC = CachedCharacter->GetAbilitySystemComponent();
	}
	else if (IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(OwnerPawn))
	{
		CachedASC = ASCInterface->GetAbilitySystemComponent();
	}

	// Cache movement attributes
	if (CachedASC.IsValid())
	{
		CachedMovementAttributes = CachedASC->GetSet<USuspenseCoreMovementAttributeSet>();
	}
}

void USuspenseCoreCharacterAnimInstance::UpdateMovementData(float DeltaSeconds)
{
	if (!CachedCharacter.IsValid())
	{
		return;
	}

	ASuspenseCoreCharacter* Character = CachedCharacter.Get();

	// Get movement state from character
	MovementState = Character->GetMovementState();

	// Derive boolean flags from movement state
	bIsSprinting = Character->IsSprinting();
	bIsCrouching = (MovementState == ESuspenseCoreMovementState::Crouching);
	bIsJumping = (MovementState == ESuspenseCoreMovementState::Jumping);
	bIsFalling = (MovementState == ESuspenseCoreMovementState::Falling);
	bIsInAir = bIsJumping || bIsFalling;
	bIsOnGround = !bIsInAir;
	bHasMovementInput = Character->HasMovementInput();

	// Get animation values (already interpolated in character)
	MoveForward = Character->GetAnimationForwardValue();
	MoveRight = Character->GetAnimationRightValue();

	// Calculate Movement for State Machine transitions (matches example blueprint)
	// Movement = Clamp(ABS(Forward) + ABS(Right), 0, Max)
	// Max = 1.0 (walk), 2.0 (sprint)
	const float MaxMovement = bIsSprinting ? 2.0f : 1.0f;
	Movement = FMath::Clamp(FMath::Abs(MoveForward) + FMath::Abs(MoveRight), 0.0f, MaxMovement);
}

void USuspenseCoreCharacterAnimInstance::UpdateVelocityData(float DeltaSeconds)
{
	APawn* OwnerPawn = TryGetPawnOwner();
	if (!OwnerPawn)
	{
		return;
	}

	// Get velocity
	const FVector Velocity = OwnerPawn->GetVelocity();
	Speed = Velocity.Size();

	// Ground speed (horizontal only)
	GroundSpeed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();

	// Vertical velocity for jump/fall
	VerticalVelocity = Velocity.Z;

	// Normalized speed
	if (MaxWalkSpeed > 0.0f)
	{
		NormalizedSpeed = FMath::Clamp(GroundSpeed / MaxWalkSpeed, 0.0f, 2.0f);
	}

	// Calculate movement direction angle
	if (GroundSpeed > 10.0f)
	{
		const FRotator ActorRotation = OwnerPawn->GetActorRotation();
		MovementDirection = UKismetMathLibrary::NormalizedDeltaRotator(
			Velocity.ToOrientationRotator(),
			ActorRotation
		).Yaw;
	}
	else
	{
		MovementDirection = 0.0f;
	}
}

void USuspenseCoreCharacterAnimInstance::UpdateWeaponData(float DeltaSeconds)
{
#if WITH_EQUIPMENT_SYSTEM
	if (!CachedStanceComponent.IsValid())
	{
		// No stance component - reset weapon data
		CurrentWeaponType = FGameplayTag();
		bHasWeaponEquipped = false;
		bIsWeaponDrawn = false;
		return;
	}

	USuspenseCoreWeaponStanceComponent* StanceComp = CachedStanceComponent.Get();

	// Get weapon data from stance component
	CurrentWeaponType = StanceComp->GetCurrentWeaponType();
	bHasWeaponEquipped = CurrentWeaponType.IsValid();
	bIsWeaponDrawn = StanceComp->IsWeaponDrawn();

	// TODO: Get aiming state from ability system or input
	// For now, interpolate aiming alpha
	const float TargetAimAlpha = bIsAiming ? 1.0f : 0.0f;
	AimingAlpha = FMath::FInterpTo(AimingAlpha, TargetAimAlpha, DeltaSeconds, AimInterpSpeed);

#else
	// Equipment system disabled - use character's basic weapon flag
	if (CachedCharacter.IsValid())
	{
		bHasWeaponEquipped = CachedCharacter->HasWeapon();
		bIsWeaponDrawn = bHasWeaponEquipped; // Simplified: if has weapon, it's drawn
	}
#endif
}

void USuspenseCoreCharacterAnimInstance::UpdateAnimationAssets()
{
	// Reset assets if no weapon
	if (!bHasWeaponEquipped || !CurrentWeaponType.IsValid())
	{
		CurrentStanceBlendSpace = nullptr;
		CurrentLocomotionBlendSpace = nullptr;
		CurrentIdleAnimation = nullptr;
		CurrentAimPose = nullptr;
		CurrentAnimationData = FAnimationStateData();
		return;
	}

	// Get animation data from DataTable
	const FAnimationStateData* AnimData = GetAnimationDataForWeaponType(CurrentWeaponType);
	if (!AnimData)
	{
		return;
	}

	// Store full animation data
	CurrentAnimationData = *AnimData;

	// Extract commonly used assets for quick access
	CurrentStanceBlendSpace = AnimData->Stance;
	CurrentLocomotionBlendSpace = AnimData->Locomotion;
	CurrentIdleAnimation = AnimData->Idle;

	// AimPose is an AnimComposite, need to check if we want to expose sequence
	if (AnimData->AimIdle)
	{
		CurrentAimPose = AnimData->AimIdle;
	}
}

void USuspenseCoreCharacterAnimInstance::UpdateIKData(float DeltaSeconds)
{
	// IK is active when weapon is drawn
	const float TargetIKAlpha = (bIsWeaponDrawn && bHasWeaponEquipped) ? 1.0f : 0.0f;

	// Smooth transition
	LeftHandIKAlpha = FMath::FInterpTo(LeftHandIKAlpha, TargetIKAlpha, DeltaSeconds, 10.0f);
	RightHandIKAlpha = FMath::FInterpTo(RightHandIKAlpha, TargetIKAlpha, DeltaSeconds, 10.0f);

	// Get transforms from animation data
	if (bHasWeaponEquipped && CurrentAnimationData.Stance != nullptr)
	{
		// Use base grip transform (index 0)
		LeftHandIKTransform = CurrentAnimationData.GetLeftHandGripTransform(0);
		RightHandIKTransform = CurrentAnimationData.RHTransform;
		WeaponTransform = CurrentAnimationData.WTransform;

		// If aiming, use aim grip transform (index 1) if available
		if (bIsAiming && CurrentAnimationData.LHGripTransform.Num() > 1)
		{
			const FTransform AimGripTransform = CurrentAnimationData.GetLeftHandGripTransform(1);
			LeftHandIKTransform = UKismetMathLibrary::TLerp(
				LeftHandIKTransform,
				AimGripTransform,
				AimingAlpha
			);
		}
	}
}

void USuspenseCoreCharacterAnimInstance::UpdateAimOffsetData(float DeltaSeconds)
{
	APawn* OwnerPawn = TryGetPawnOwner();
	if (!OwnerPawn)
	{
		return;
	}

	// Get control rotation for aim offset
	if (AController* Controller = OwnerPawn->GetController())
	{
		const FRotator ControlRotation = Controller->GetControlRotation();
		const FRotator ActorRotation = OwnerPawn->GetActorRotation();

		// Calculate delta between control and actor rotation
		const FRotator DeltaRotation = UKismetMathLibrary::NormalizedDeltaRotator(
			ControlRotation,
			ActorRotation
		);

		// Clamp values
		AimYaw = FMath::Clamp(DeltaRotation.Yaw, -180.0f, 180.0f);
		AimPitch = FMath::Clamp(DeltaRotation.Pitch, -90.0f, 90.0f);

		// Turn in place detection
		const float TurnThreshold = 70.0f;
		bShouldTurnInPlace = FMath::Abs(AimYaw) > TurnThreshold && GroundSpeed < 10.0f;
		TurnInPlaceAngle = AimYaw;
	}
}

void USuspenseCoreCharacterAnimInstance::UpdatePoseStates(float DeltaSeconds)
{
	APawn* OwnerPawn = TryGetPawnOwner();
	if (!OwnerPawn)
	{
		return;
	}

	// ═══════════════════════════════════════════════════════════════════════════════
	// SLIDING STATE
	// ═══════════════════════════════════════════════════════════════════════════════
	// TODO: Get sliding state from Character when implemented
	// For now, sliding is false (can be extended later)
	bIsSliding = false;

	// ═══════════════════════════════════════════════════════════════════════════════
	// LEAN (ROLL) - наклон при поворотах
	// ═══════════════════════════════════════════════════════════════════════════════
	// In example: MPS->Lean, interpolated with speed 10.0
	// Calculate lean from lateral velocity or yaw delta
	if (CachedCharacter.IsValid())
	{
		// Get lean from character if available, otherwise calculate from movement
		// For now, calculate based on strafe movement
		TargetLean = MoveRight * 15.0f; // Max 15 degrees lean
	}
	Lean = FMath::FInterpTo(Lean, TargetLean, DeltaSeconds, LeanInterpSpeed);

	// ═══════════════════════════════════════════════════════════════════════════════
	// BODY PITCH - наклон корпуса
	// ═══════════════════════════════════════════════════════════════════════════════
	// Use AimPitch (delta between Control and Actor rotation) for body lean
	// AimPitch is calculated in UpdateAimOffsetData and represents where player is looking
	BodyPitch = AimPitch;

	// Get actor rotation for yaw calculations
	const FRotator ActorRotation = OwnerPawn->GetActorRotation();

	// ═══════════════════════════════════════════════════════════════════════════════
	// YAW OFFSET - Turn In Place logic (DefineYaw from example)
	// ═══════════════════════════════════════════════════════════════════════════════
	// Logic from example:
	// 1. Save LastTickYaw, update CurrentYaw
	// 2. Accumulate: YawOffset += (LastTickYaw - CurrentYaw)
	// 3. Normalize and clamp to -120..120
	// 4. If moving or in air: interpolate YawOffset toward 0
	// 5. If turning animation: use RotationCurve to drive YawOffset

	// Save last yaw and get current
	LastTickYaw = CurrentYaw;
	CurrentYaw = ActorRotation.Yaw;

	// Only accumulate when stationary and on ground
	const bool bShouldAccumulateYaw = (Movement <= 0.0f) && !bIsInAir;

	if (bShouldAccumulateYaw)
	{
		// Accumulate yaw delta
		float YawDelta = LastTickYaw - CurrentYaw;

		// Normalize delta to -180..180
		YawDelta = FMath::UnwindDegrees(YawDelta);

		YawOffset += YawDelta;

		// Normalize and clamp
		YawOffset = FMath::UnwindDegrees(YawOffset);
		YawOffset = FMath::Clamp(YawOffset, -MaxYawOffset, MaxYawOffset);
	}
	else
	{
		// When moving or in air, interpolate YawOffset toward 0
		YawOffset = FMath::FInterpTo(YawOffset, 0.0f, DeltaSeconds, YawOffsetInterpSpeed);
	}

	// ═══════════════════════════════════════════════════════════════════════════════
	// ANIMATION CURVES - IsTurning and Rotation
	// ═══════════════════════════════════════════════════════════════════════════════
	// Get curve values from currently playing animations
	IsTurningCurve = GetCurveValue(FName("IsTurning"));
	RotationCurve = GetCurveValue(FName("Rotation"));

	// If turning animation is playing, use rotation curve to modify YawOffset
	const bool bIsTurningAnimation = FMath::IsNearlyEqual(IsTurningCurve, 1.0f, 0.01f);
	if (bIsTurningAnimation)
	{
		// Interpolate YawOffset based on RotationCurve
		const float TargetYawFromCurve = RotationCurve - YawOffset;
		YawOffset = FMath::FInterpTo(YawOffset, TargetYawFromCurve, DeltaSeconds, YawOffsetCurveInterpSpeed);
	}
}

void USuspenseCoreCharacterAnimInstance::UpdateGASAttributes()
{
	if (!CachedMovementAttributes.IsValid())
	{
		// Use defaults from character movement
		if (CachedMovementComponent.IsValid())
		{
			MaxWalkSpeed = CachedMovementComponent->MaxWalkSpeed;
			MaxCrouchSpeed = CachedMovementComponent->MaxWalkSpeedCrouched;
			// Estimate sprint speed
			MaxSprintSpeed = MaxWalkSpeed * 1.5f;
			MaxAimSpeed = MaxWalkSpeed * 0.6f;
			// Default jump height from CMC
			JumpHeight = CachedMovementComponent->JumpZVelocity;
		}
		return;
	}

	const USuspenseCoreMovementAttributeSet* MovementAttrs = CachedMovementAttributes.Get();

	// Get speeds from GAS attributes
	MaxWalkSpeed = MovementAttrs->GetWalkSpeed();
	MaxSprintSpeed = MovementAttrs->GetSprintSpeed();
	MaxCrouchSpeed = MovementAttrs->GetCrouchSpeed();
	MaxAimSpeed = MovementAttrs->GetAimSpeed();
	JumpHeight = MovementAttrs->GetJumpHeight();
}

void USuspenseCoreCharacterAnimInstance::LoadWeaponAnimationsTable()
{
	// Get DataTable from project settings
	const USuspenseCoreSettings* Settings = GetDefault<USuspenseCoreSettings>();
	if (Settings && !Settings->WeaponAnimationsTable.IsNull())
	{
		WeaponAnimationsTable = Settings->WeaponAnimationsTable.LoadSynchronous();
	}
}

const FAnimationStateData* USuspenseCoreCharacterAnimInstance::GetAnimationDataForWeaponType(const FGameplayTag& WeaponType) const
{
	if (!WeaponAnimationsTable || !WeaponType.IsValid())
	{
		return nullptr;
	}

	// Row name is the tag string
	const FString RowName = WeaponType.ToString();
	return WeaponAnimationsTable->FindRow<FAnimationStateData>(*RowName, TEXT("GetAnimationDataForWeaponType"));
}

// ═══════════════════════════════════════════════════════════════════════════════
// HELPER METHODS
// ═══════════════════════════════════════════════════════════════════════════════

ASuspenseCoreCharacter* USuspenseCoreCharacterAnimInstance::GetSuspenseCoreCharacter() const
{
	return CachedCharacter.Get();
}

bool USuspenseCoreCharacterAnimInstance::HasValidAnimationData() const
{
	return bHasWeaponEquipped && CurrentAnimationData.Stance != nullptr;
}

UAnimMontage* USuspenseCoreCharacterAnimInstance::GetDrawMontage(bool bFirstDraw) const
{
	if (bFirstDraw && CurrentAnimationData.FirstDraw)
	{
		return CurrentAnimationData.FirstDraw;
	}
	return CurrentAnimationData.Draw;
}

UAnimMontage* USuspenseCoreCharacterAnimInstance::GetHolsterMontage() const
{
	return CurrentAnimationData.Holster;
}

UAnimMontage* USuspenseCoreCharacterAnimInstance::GetReloadMontage(bool bTactical) const
{
	if (bTactical && CurrentAnimationData.ReloadShort)
	{
		return CurrentAnimationData.ReloadShort;
	}
	return CurrentAnimationData.ReloadLong;
}

UAnimMontage* USuspenseCoreCharacterAnimInstance::GetFireMontage(bool bAiming) const
{
	if (bAiming && CurrentAnimationData.AimShoot)
	{
		return CurrentAnimationData.AimShoot;
	}
	return CurrentAnimationData.Shoot;
}
