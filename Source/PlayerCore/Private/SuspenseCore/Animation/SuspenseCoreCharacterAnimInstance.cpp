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
#include "Animation/AnimComposite.h"

#if WITH_EQUIPMENT_SYSTEM
#include "SuspenseCore/Components/SuspenseCoreWeaponStanceComponent.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeaponAnimation.h"
#endif

// Static socket name for left hand target on weapon mesh
const FName USuspenseCoreCharacterAnimInstance::LHTargetSocketName = TEXT("LH_Target");

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

	// ═══════════════════════════════════════════════════════════════════════════════
	// Calculate MoveForward/MoveRight from VELOCITY relative to CONTROL rotation
	// When bUseControllerRotationYaw=true, actor rotates to face camera direction,
	// so we need to calculate direction relative to where the camera is looking,
	// not where the actor is facing (they're nearly the same but with lag)
	// ═══════════════════════════════════════════════════════════════════════════════
	const FVector Velocity = Character->GetVelocity();
	const float HorizontalSpeed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();

	if (HorizontalSpeed > 10.0f)
	{
		// Get velocity direction relative to CONTROL rotation (camera direction)
		// This is what the player considers "forward" for movement
		FRotator ReferenceRotation = Character->GetActorRotation();

		// If character uses controller rotation, use controller rotation as reference
		// This gives us the direction relative to where the player is looking
		if (AController* Controller = Character->GetController())
		{
			ReferenceRotation = FRotator(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
		}

		const FRotator VelocityRotation = Velocity.ToOrientationRotator();
		const FRotator DeltaRotation = UKismetMathLibrary::NormalizedDeltaRotator(VelocityRotation, ReferenceRotation);

		// Convert angle to Forward/Right components
		// 0 degrees = forward, 90 = right, -90 = left, 180 = backward
		const float AngleRad = FMath::DegreesToRadians(DeltaRotation.Yaw);
		float TargetForward = FMath::Cos(AngleRad);
		float TargetRight = FMath::Sin(AngleRad);

		// Apply sprint multiplier (2.0 for sprint, 1.0 for walk)
		const float SpeedMultiplier = bIsSprinting ? 2.0f : 1.0f;
		TargetForward *= SpeedMultiplier;
		TargetRight *= SpeedMultiplier;

		// Smooth interpolation
		MoveForward = FMath::FInterpTo(MoveForward, TargetForward, DeltaSeconds, 10.0f);
		MoveRight = FMath::FInterpTo(MoveRight, TargetRight, DeltaSeconds, 10.0f);
	}
	else
	{
		// No significant movement - interpolate to zero
		MoveForward = FMath::FInterpTo(MoveForward, 0.0f, DeltaSeconds, 10.0f);
		MoveRight = FMath::FInterpTo(MoveRight, 0.0f, DeltaSeconds, 10.0f);
	}

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
		bIsAiming = false;
		bIsFiring = false;
		bIsReloading = false;
		bIsHoldingBreath = false;
		bIsWeaponMontageActive = false;
		AimingAlpha = 0.0f;
		GripModifier = 0.0f;
		WeaponLoweredAlpha = 0.0f;
		RecoilAlpha = 0.0f;
		WeaponSwayMultiplier = 1.0f;
		StoredRecoil = 0.0f;
		AdditivePitch = 0.0f;
		BlockDistance = 0.0f;
		// Compatibility flags
		bIsHolstered = true;
		bModifyGrip = false;
		bCreateAimPose = false;
		// Aim target
		SightDistance = 200.0f;
		// Pose indices
		AimPose = 0;
		StoredPose = 0;
		GripID = 0;
		// IK Transforms
		AimTransform = FTransform::Identity;
		RightHandTransform = FTransform::Identity;
		LeftHandTransform = FTransform::Identity;
		// Reset legacy transforms
		RHTransform = FTransform::Identity;
		LHTransform = FTransform::Identity;
		WTransform = FTransform::Identity;
		return;
	}

	USuspenseCoreWeaponStanceComponent* StanceComp = CachedStanceComponent.Get();

	// Cache weapon actor for socket transform access (used in UpdateIKData)
	CachedWeaponActor = StanceComp->GetTrackedEquipmentActor();

	// Get complete stance snapshot from component (includes all combat states)
	const FSuspenseCoreWeaponStanceSnapshot Snapshot = StanceComp->GetStanceSnapshot();

	// Weapon identity
	CurrentWeaponType = Snapshot.WeaponType;
	bHasWeaponEquipped = CurrentWeaponType.IsValid();
	bIsWeaponDrawn = Snapshot.bIsDrawn;

	// Combat states from snapshot
	bIsAiming = Snapshot.bIsAiming;
	bIsFiring = Snapshot.bIsFiring;
	bIsReloading = Snapshot.bIsReloading;
	bIsHoldingBreath = Snapshot.bIsHoldingBreath;
	bIsWeaponMontageActive = Snapshot.bIsMontageActive;

	// Compatibility flags
	bIsHolstered = Snapshot.bIsHolstered;
	bModifyGrip = Snapshot.bModifyGrip;
	bCreateAimPose = Snapshot.bCreateAimPose;

	// Aim target - SightDistance from snapshot or default
	SightDistance = Snapshot.SightDistance;

	// Pose indices
	AimPose = Snapshot.AimPose;
	StoredPose = Snapshot.StoredPose;
	GripID = Snapshot.GripID;

	// Pose modifiers from snapshot (already interpolated in component)
	AimingAlpha = Snapshot.AimPoseAlpha;
	GripModifier = Snapshot.GripModifier;
	WeaponLoweredAlpha = Snapshot.WeaponLoweredAlpha;

	// IK Transforms
	AimTransform = Snapshot.AimTransform;
	RightHandTransform = Snapshot.RightHandTransform;
	LeftHandTransform = Snapshot.LeftHandTransform;

	// Procedural animation from snapshot
	RecoilAlpha = Snapshot.RecoilAlpha;
	WeaponSwayMultiplier = Snapshot.SwayMultiplier;
	StoredRecoil = Snapshot.StoredRecoil;
	AdditivePitch = Snapshot.AdditivePitch;
	BlockDistance = Snapshot.BlockDistance;

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
	// ═══════════════════════════════════════════════════════════════════════════════
	// LEGACY LOGIC FROM MPS ANIMBP EVENTGRAPH
	// Обновляет только 3 переменные: RHTransform, LHTransform, WTransform
	// ═══════════════════════════════════════════════════════════════════════════════

	// IK активен когда оружие достано
	const float TargetIKAlpha = (bIsWeaponDrawn && bHasWeaponEquipped) ? 1.0f : 0.0f;
	LeftHandIKAlpha = FMath::FInterpTo(LeftHandIKAlpha, TargetIKAlpha, DeltaSeconds, 10.0f);
	RightHandIKAlpha = FMath::FInterpTo(RightHandIKAlpha, TargetIKAlpha, DeltaSeconds, 10.0f);

	// ═══════════════════════════════════════════════════════════════════════════════
	// ADDITIVE PITCH - Ease функция (legacy: BlendExp 6.0)
	// ═══════════════════════════════════════════════════════════════════════════════
	{
		const float EaseAlpha = FMath::Clamp(DeltaSeconds * 10.0f, 0.0f, 1.0f);
		const float EasedAlpha = FMath::Pow(EaseAlpha, AdditivePitchBlendExp);
		InterpolatedAdditivePitch = FMath::Lerp(InterpolatedAdditivePitch, StoredRecoil, EasedAlpha);
		AdditivePitch = InterpolatedAdditivePitch;
	}

	// ═══════════════════════════════════════════════════════════════════════════════
	// BLOCK DISTANCE - Интерполяция (legacy: InterpSpeed 10.0)
	// ═══════════════════════════════════════════════════════════════════════════════
	InterpolatedBlockDistance = FMath::FInterpTo(InterpolatedBlockDistance, BlockDistance, DeltaSeconds, BlockDistanceInterpSpeed);
	BlockDistance = InterpolatedBlockDistance;

	if (!bHasWeaponEquipped)
	{
		return;
	}

	// ═══════════════════════════════════════════════════════════════════════════════
	// TARGET TRANSFORMS
	// ═══════════════════════════════════════════════════════════════════════════════

	FTransform TargetRHTransform = FTransform::Identity;
	FTransform TargetLHTransform = FTransform::Identity;
	FTransform TargetWTransform = FTransform::Identity;

	// RH Transform: DT → Snapshot → AnimData
	if (!DTRHTransform.Equals(FTransform::Identity))
	{
		TargetRHTransform = DTRHTransform;
	}
	else if (!RightHandTransform.Equals(FTransform::Identity))
	{
		TargetRHTransform = RightHandTransform;
	}
	else if (CurrentAnimationData.Stance != nullptr)
	{
		TargetRHTransform = CurrentAnimationData.RHTransform;
	}

	// LH Transform: M LH Offset логика (socket / grip transforms)
	TargetLHTransform = ComputeLHOffsetTransform();

	// W Transform: DT → AnimData
	if (!DTWTransform.Equals(FTransform::Identity))
	{
		TargetWTransform = DTWTransform;
	}
	else if (CurrentAnimationData.Stance != nullptr)
	{
		TargetWTransform = CurrentAnimationData.WTransform;
	}

	// ═══════════════════════════════════════════════════════════════════════════════
	// TINTERP TO - Плавная интерполяция (legacy: InterpSpeed 8.0)
	// ═══════════════════════════════════════════════════════════════════════════════

	InterpolatedRHTransform = UKismetMathLibrary::TInterpTo(
		InterpolatedRHTransform, TargetRHTransform, DeltaSeconds, TransformInterpSpeed);

	InterpolatedLHTransform = UKismetMathLibrary::TInterpTo(
		InterpolatedLHTransform, TargetLHTransform, DeltaSeconds, TransformInterpSpeed);

	// ═══════════════════════════════════════════════════════════════════════════════
	// ФИНАЛЬНЫЕ ЗНАЧЕНИЯ ДЛЯ ANIMGRAPH
	// Только эти 3 переменные используются в AnimGraph!
	// ═══════════════════════════════════════════════════════════════════════════════

	RHTransform = InterpolatedRHTransform;
	LHTransform = InterpolatedLHTransform;
	WTransform = TargetWTransform;

	// Override LH при прицеливании с кастомной позой
	if (bIsAiming && bCreateAimPose && !AimTransform.Equals(FTransform::Identity))
	{
		LHTransform = UKismetMathLibrary::TLerp(LHTransform, AimTransform, AimingAlpha);
	}
}

bool USuspenseCoreCharacterAnimInstance::GetWeaponLHTargetTransform(FTransform& OutTransform) const
{
	// Get LH_Target socket transform from weapon mesh (Component Space)
	// This is the key to making left hand follow weapon rotation!

	if (!CachedWeaponActor.IsValid())
	{
		return false;
	}

	AActor* WeaponActor = CachedWeaponActor.Get();

	// Try to get skeletal mesh component from weapon
	USkeletalMeshComponent* WeaponMesh = WeaponActor->FindComponentByClass<USkeletalMeshComponent>();
	if (!WeaponMesh)
	{
		// Fallback to static mesh if no skeletal mesh
		UStaticMeshComponent* StaticMesh = WeaponActor->FindComponentByClass<UStaticMeshComponent>();
		if (StaticMesh && StaticMesh->DoesSocketExist(LHTargetSocketName))
		{
			OutTransform = StaticMesh->GetSocketTransform(LHTargetSocketName, RTS_Component);
			return true;
		}
		return false;
	}

	// Check if socket exists
	if (!WeaponMesh->DoesSocketExist(LHTargetSocketName))
	{
		return false;
	}

	// Get socket transform in Component Space (RTS_Component)
	// This makes the transform relative to the weapon mesh, so it rotates WITH the weapon
	OutTransform = WeaponMesh->GetSocketTransform(LHTargetSocketName, RTS_Component);
	return true;
}

FTransform USuspenseCoreCharacterAnimInstance::ComputeLHOffsetTransform() const
{
	// ═══════════════════════════════════════════════════════════════════════════════
	// M LH OFFSET LOGIC (from legacy MPS Blueprint)
	//
	// Select logic:
	// 1. If Is Montage Active? → return Identity (skip IK during montages)
	// 2. If Modify Grip? → use DT LH Grip Transform[Grip ID]
	// 3. Else → use Get Socket Transform("LH_Target") from weapon
	// ═══════════════════════════════════════════════════════════════════════════════

	// During montages, don't apply IK offset (let montage control hands)
	if (bIsWeaponMontageActive)
	{
		return FTransform::Identity;
	}

	// If Modify Grip is true, use DataTable grip transforms
	if (bModifyGrip)
	{
		// Use DT LH Grip Transform array with GripID
		if (DTLHGripTransform.Num() > 0)
		{
			const int32 GripIndex = FMath::Clamp(GripID, 0, DTLHGripTransform.Num() - 1);
			FTransform BaseGrip = DTLHGripTransform[GripIndex];

			// Blend to aim grip if aiming
			if (bIsAiming)
			{
				const int32 AimGripIndex = (AimPose > 0) ? AimPose : 1;
				if (DTLHGripTransform.IsValidIndex(AimGripIndex))
				{
					BaseGrip = UKismetMathLibrary::TLerp(
						BaseGrip,
						DTLHGripTransform[AimGripIndex],
						AimingAlpha
					);
				}
			}
			return BaseGrip;
		}
		else if (!DTLHTransform.Equals(FTransform::Identity))
		{
			return DTLHTransform;
		}
		else if (CurrentAnimationData.LHGripTransform.Num() > 0)
		{
			const int32 GripIndex = FMath::Clamp(GripID, 0, CurrentAnimationData.LHGripTransform.Num() - 1);
			return CurrentAnimationData.GetLeftHandGripTransform(GripIndex);
		}
	}

	// Default: Get socket transform from weapon (LH_Target socket)
	// This is the KEY - the socket moves with the weapon, so hand follows weapon rotation!
	FTransform SocketTransform;
	if (GetWeaponLHTargetTransform(SocketTransform))
	{
		return SocketTransform;
	}

	// Fallback to DT transforms if socket not found
	if (DTLHGripTransform.Num() > 0)
	{
		const int32 GripIndex = FMath::Clamp(GripID, 0, DTLHGripTransform.Num() - 1);
		return DTLHGripTransform[GripIndex];
	}
	else if (!DTLHTransform.Equals(FTransform::Identity))
	{
		return DTLHTransform;
	}
	else if (CurrentAnimationData.LHGripTransform.Num() > 0)
	{
		const int32 GripIndex = FMath::Clamp(GripID, 0, CurrentAnimationData.LHGripTransform.Num() - 1);
		return CurrentAnimationData.GetLeftHandGripTransform(GripIndex);
	}

	return FTransform::Identity;
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
	// Inverted because positive AimPitch (looking up) should lean body backward (negative rotation)
	BodyPitch = -AimPitch;

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

	// Verify DataTable uses correct row structure (FAnimationStateData)
	// This prevents errors when table uses different struct (e.g., MPS struct)
	const UScriptStruct* RowStruct = WeaponAnimationsTable->GetRowStruct();
	if (!RowStruct || RowStruct != FAnimationStateData::StaticStruct())
	{
		// Table uses different struct - skip C++ path, let Blueprint handle it
		return nullptr;
	}

	// Row name is the tag string
	const FString RowName = WeaponType.ToString();
	return WeaponAnimationsTable->FindRow<FAnimationStateData>(*RowName, TEXT("GetAnimationDataForWeaponType"));
}

UAnimSequenceBase* USuspenseCoreCharacterAnimInstance::GetGripPoseByIndex(int32 PoseIndex) const
{
	// GripPoses is an AnimComposite containing multiple pose segments
	// Each segment in the composite represents a different grip pose
	// The pose index maps to segment index within the composite

	if (!CurrentAnimationData.GripPoses)
	{
		return nullptr;
	}

	// AnimComposite stores segments in AnimationTrack
	const TArray<FAnimSegment>& Segments = CurrentAnimationData.GripPoses->AnimationTrack.AnimSegments;

	if (Segments.IsValidIndex(PoseIndex))
	{
		return Segments[PoseIndex].GetAnimReference();
	}

	// Fallback to first segment if index invalid
	if (Segments.Num() > 0)
	{
		return Segments[0].GetAnimReference();
	}

	return nullptr;
}

UAnimSequenceBase* USuspenseCoreCharacterAnimInstance::GetActiveGripPose() const
{
	// Determine pose index based on combat state
	int32 PoseIndex = GripID; // Base grip ID from weapon

	// Override with state-specific poses
	if (bIsReloading && GripID == 0)
	{
		// Use reload pose (index 2) if currently reloading and no custom GripID
		PoseIndex = 2;
	}
	else if (bIsAiming && bModifyGrip && AimPose > 0)
	{
		// Use AimPose index for aiming if ModifyGrip is enabled
		PoseIndex = AimPose;
	}
	else if (bIsAiming && GripID == 0)
	{
		// Default to aim pose (index 1) when aiming
		PoseIndex = 1;
	}

	return GetGripPoseByIndex(PoseIndex);
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

FName USuspenseCoreCharacterAnimInstance::GetLegacyRowNameFromArchetype() const
{
	return GetLegacyRowNameFromArchetypeTag(CurrentWeaponType);
}

FName USuspenseCoreCharacterAnimInstance::GetLegacyRowNameFromArchetypeTag(const FGameplayTag& WeaponArchetype)
{
	if (!WeaponArchetype.IsValid())
	{
		return FName("SMG"); // Default
	}

	const FString TagString = WeaponArchetype.ToString();

	// Sniper check first (more specific)
	if (TagString.Contains(TEXT("Sniper")))
	{
		return FName("Sniper");
	}

	// Rifle (Assault, DMR) -> SMG (legacy naming)
	if (TagString.Contains(TEXT("Weapon.Rifle")))
	{
		return FName("SMG");
	}

	// SMG
	if (TagString.Contains(TEXT("Weapon.SMG")))
	{
		return FName("SMG");
	}

	// Pistol
	if (TagString.Contains(TEXT("Weapon.Pistol")))
	{
		return FName("Pistol");
	}

	// Shotgun
	if (TagString.Contains(TEXT("Weapon.Shotgun")))
	{
		return FName("Shotgun");
	}

	// Melee Knife
	if (TagString.Contains(TEXT("Weapon.Melee.Knife")))
	{
		return FName("Knife");
	}

	// Melee Blunt -> Special
	if (TagString.Contains(TEXT("Weapon.Melee")))
	{
		return FName("Special");
	}

	// Heavy -> Special
	if (TagString.Contains(TEXT("Weapon.Heavy")))
	{
		return FName("Special");
	}

	// Throwable -> Frag
	if (TagString.Contains(TEXT("Weapon.Throwable")))
	{
		return FName("Frag");
	}

	// Default fallback
	return FName("SMG");
}
