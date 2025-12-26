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
		// Debug: Log once when stance component missing
		static bool bLoggedOnce = false;
		if (!bLoggedOnce)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] No CachedStanceComponent! Check if character has USuspenseCoreWeaponStanceComponent"));
			bLoggedOnce = true;
		}
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
		// Legacy transforms for AnimGraph
		RHTransform = FTransform::Identity;
		LHTransform = FTransform::Identity;
		// DT update flags - bNeedsDTUpdate stays true for initial load
		bWeaponTypeChanged = false;
		return;
	}

	USuspenseCoreWeaponStanceComponent* StanceComp = CachedStanceComponent.Get();

	// Cache weapon actor for socket transform access (used in UpdateIKData)
	CachedWeaponActor = StanceComp->GetTrackedEquipmentActor();

	// Debug: Log if weapon actor is valid
	static bool bLoggedWeaponActor = false;
	if (!bLoggedWeaponActor || (CachedWeaponActor.IsValid() != bLoggedWeaponActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] CachedWeaponActor: %s"),
			CachedWeaponActor.IsValid() ? *CachedWeaponActor->GetName() : TEXT("NULL"));
		bLoggedWeaponActor = CachedWeaponActor.IsValid();
	}

	// Get complete stance snapshot from component (includes all combat states)
	const FSuspenseCoreWeaponStanceSnapshot Snapshot = StanceComp->GetStanceSnapshot();

	// Weapon identity
	FGameplayTag PreviousWeaponType = CurrentWeaponType;
	CurrentWeaponType = Snapshot.WeaponType;
	bHasWeaponEquipped = CurrentWeaponType.IsValid();
	bIsWeaponDrawn = Snapshot.bIsDrawn;

	// ═══════════════════════════════════════════════════════════════════════════════
	// WEAPON TYPE CHANGE DETECTION (for Blueprint DT loading)
	// bWeaponTypeChanged: true when weapon changed OR first frame
	// bNeedsDTUpdate: stays true until Blueprint calls MarkDTInitialized()
	// ═══════════════════════════════════════════════════════════════════════════════
	bWeaponTypeChanged = (CurrentWeaponType != PreviousWeaponType);

	// Also trigger DT update if weapon changed
	if (bWeaponTypeChanged)
	{
		bNeedsDTUpdate = true;
		UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] WeaponType changed: %s -> %s (bNeedsDTUpdate=true)"),
			*PreviousWeaponType.ToString(),
			*CurrentWeaponType.ToString());
	}

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

	// ═══════════════════════════════════════════════════════════════════════════════
	// DT LEGACY SUPPORT
	// Per documentation: StanceComponent is SSOT. DT* variables are legacy Blueprint support.
	// Priority: StanceComponent (via Snapshot) > Blueprint DT* variables
	//
	// If WeaponActor doesn't set values, StanceComponent will have defaults (0, false).
	// Blueprint can set DT* variables which will be used as fallback.
	// ═══════════════════════════════════════════════════════════════════════════════

	// Apply DT pose flags ONLY if StanceComponent didn't set them
	// Note: We check bModifyGrip/bCreateAimPose because these are explicitly set by WeaponActor
	if (!Snapshot.bModifyGrip)
	{
		bModifyGrip = DTModifyGrip;
	}
	if (!Snapshot.bCreateAimPose)
	{
		bCreateAimPose = DTCreateAimPose;
	}

	// DT pose indices are used as defaults ONLY if Blueprint sets them AND weapon doesn't override
	// Blueprint should set these via StanceComponent API for proper SSOT architecture
	// This is legacy support for existing Blueprint animations

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

	// Get animation data from DataTable (C++ SSOT path)
	const FAnimationStateData* AnimData = GetAnimationDataForWeaponType(CurrentWeaponType);
	if (!AnimData)
	{
		// C++ path returned null - either no table, struct mismatch, or row not found
		// Blueprint path should handle this via bNeedsDTUpdate flag
		static bool bLoggedNullAnimData = false;
		if (!bLoggedNullAnimData && bWeaponTypeChanged)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] UpdateAnimationAssets: C++ path returned NULL for weapon '%s'. Blueprint path should handle via Active DT macro."),
				*CurrentWeaponType.ToString());
			bLoggedNullAnimData = true;
		}
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

	// Log success
	if (bWeaponTypeChanged)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] UpdateAnimationAssets: SUCCESS! Loaded C++ animation data for '%s'. Stance=%s"),
			*CurrentWeaponType.ToString(),
			CurrentStanceBlendSpace ? *CurrentStanceBlendSpace->GetName() : TEXT("NULL"));
	}
}

void USuspenseCoreCharacterAnimInstance::UpdateIKData(float DeltaSeconds)
{
	// ═══════════════════════════════════════════════════════════════════════════════
	// LEGACY LOGIC FROM MPS ANIMBP EVENTGRAPH
	// Implements full interpolation and LH_Target socket logic
	// ═══════════════════════════════════════════════════════════════════════════════

	// IK is active when weapon is drawn
	const float TargetIKAlpha = (bIsWeaponDrawn && bHasWeaponEquipped) ? 1.0f : 0.0f;

	// Smooth transition for IK alpha
	LeftHandIKAlpha = FMath::FInterpTo(LeftHandIKAlpha, TargetIKAlpha, DeltaSeconds, 10.0f);
	RightHandIKAlpha = FMath::FInterpTo(RightHandIKAlpha, TargetIKAlpha, DeltaSeconds, 10.0f);

	// ═══════════════════════════════════════════════════════════════════════════════
	// ADDITIVE PITCH - Ease function (legacy: Ease Out, BlendExp 6.0)
	// ═══════════════════════════════════════════════════════════════════════════════
	// Legacy Blueprint: Ease(EaseOut, Alpha=DeltaTime, A=AdditivePitch, B=StoredRecoil, BlendExp=6.0)
	// This creates smooth easing from current AdditivePitch towards StoredRecoil
	{
		const float EaseAlpha = FMath::Clamp(DeltaSeconds * 10.0f, 0.0f, 1.0f); // Normalized alpha
		const float EasedAlpha = FMath::Pow(EaseAlpha, AdditivePitchBlendExp); // EaseOut effect
		InterpolatedAdditivePitch = FMath::Lerp(InterpolatedAdditivePitch, StoredRecoil, EasedAlpha);
		AdditivePitch = InterpolatedAdditivePitch;
	}

	// ═══════════════════════════════════════════════════════════════════════════════
	// BLOCK DISTANCE - Interpolation (legacy: InterpSpeed 10.0)
	// ═══════════════════════════════════════════════════════════════════════════════
	InterpolatedBlockDistance = FMath::FInterpTo(InterpolatedBlockDistance, BlockDistance, DeltaSeconds, BlockDistanceInterpSpeed);
	BlockDistance = InterpolatedBlockDistance;

	// Skip transform calculations if no weapon equipped
	if (!bHasWeaponEquipped)
	{
		return;
	}

	// ═══════════════════════════════════════════════════════════════════════════════
	// COMPUTE TARGET TRANSFORMS
	// Per documentation priority:
	// RH: StanceComponent.RightHandTransform > DT > CurrentAnimationData
	// LH: ComputeLHOffsetTransform() handles all cases per M LH Offset logic
	// ═══════════════════════════════════════════════════════════════════════════════

	FTransform TargetRHTransform = FTransform::Identity;
	FTransform TargetLHTransform = FTransform::Identity;

	// Get RH Transform target - priority: StanceComponent > DT > AnimationData
	if (!RightHandTransform.Equals(FTransform::Identity))
	{
		// From StanceComponent snapshot (weapon actor set this)
		TargetRHTransform = RightHandTransform;
	}
	else if (!DTRHTransform.Equals(FTransform::Identity))
	{
		// Legacy DT support
		TargetRHTransform = DTRHTransform;
	}
	else if (CurrentAnimationData.Stance != nullptr)
	{
		// Fallback to animation data from DataTable
		TargetRHTransform = CurrentAnimationData.RHTransform;
	}

	// Get LH Transform target using M LH Offset logic (handles all priority cases)
	TargetLHTransform = ComputeLHOffsetTransform();

	// ═══════════════════════════════════════════════════════════════════════════════
	// APPLY AIM BLEND TO LH TRANSFORM (before interpolation)
	// Per documentation: "Apply AimTransform from weapon actor if aiming"
	// This must happen BEFORE interpolation so the blend is smooth
	// ═══════════════════════════════════════════════════════════════════════════════
	if (bCreateAimPose && !AimTransform.Equals(FTransform::Identity))
	{
		// Blend between base LH transform and aim transform based on AimingAlpha
		TargetLHTransform = UKismetMathLibrary::TLerp(
			TargetLHTransform,
			AimTransform,
			AimingAlpha
		);
	}

	// ═══════════════════════════════════════════════════════════════════════════════
	// TINTERP TO - Smooth interpolation (legacy: InterpSpeed 8.0)
	// ═══════════════════════════════════════════════════════════════════════════════

	// RH Transform interpolation
	InterpolatedRHTransform = UKismetMathLibrary::TInterpTo(
		InterpolatedRHTransform,
		TargetRHTransform,
		DeltaSeconds,
		TransformInterpSpeed
	);
	RightHandIKTransform = InterpolatedRHTransform;

	// LH Transform interpolation
	InterpolatedLHTransform = UKismetMathLibrary::TInterpTo(
		InterpolatedLHTransform,
		TargetLHTransform,
		DeltaSeconds,
		TransformInterpSpeed
	);
	LeftHandIKTransform = InterpolatedLHTransform;

	// ═══════════════════════════════════════════════════════════════════════════════
	// WEAPON TRANSFORM - priority: StanceComponent > DT > AnimationData
	// ═══════════════════════════════════════════════════════════════════════════════
	if (!DTWTransform.Equals(FTransform::Identity))
	{
		WeaponTransform = DTWTransform;
	}
	else if (CurrentAnimationData.Stance != nullptr)
	{
		WeaponTransform = CurrentAnimationData.WTransform;
	}

	// ═══════════════════════════════════════════════════════════════════════════════
	// LEGACY VARIABLES FOR ANIMGRAPH
	// Set RH Transform / LH Transform for AnimGraph's Transform (Modify) Bone nodes
	// These match the legacy Blueprint variable names that AnimGraph expects
	//
	// IMPORTANT: If transforms are Identity, DON'T apply them (keep Alpha=0)
	// Otherwise the Transform (Modify) Bone will override the animation pose!
	// ═══════════════════════════════════════════════════════════════════════════════
	RHTransform = RightHandIKTransform;
	LHTransform = LeftHandIKTransform;

	// If transforms are Identity, disable IK to preserve animation pose
	const bool bRHTransformValid = !RHTransform.GetLocation().IsNearlyZero(0.1f) || !RHTransform.GetRotation().IsIdentity(0.01f);
	const bool bLHTransformValid = !LHTransform.GetLocation().IsNearlyZero(0.1f) || !LHTransform.GetRotation().IsIdentity(0.01f);

	if (!bRHTransformValid)
	{
		RightHandIKAlpha = 0.0f;
	}
	if (!bLHTransformValid)
	{
		LeftHandIKAlpha = 0.0f;
	}

	// Debug logging (every 60 frames)
	static int32 IKFrameCounter = 0;
	if (++IKFrameCounter % 60 == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnimInstance IK] bWeaponDrawn=%d, RH_Valid=%d, LH_Valid=%d, RHIKAlpha=%.2f, LHIKAlpha=%.2f"),
			bIsWeaponDrawn,
			bRHTransformValid,
			bLHTransformValid,
			RightHandIKAlpha,
			LeftHandIKAlpha);
		UE_LOG(LogTemp, Warning, TEXT("[AnimInstance IK] RHTransform Loc=(%.2f,%.2f,%.2f), LHTransform Loc=(%.2f,%.2f,%.2f)"),
			RHTransform.GetLocation().X, RHTransform.GetLocation().Y, RHTransform.GetLocation().Z,
			LHTransform.GetLocation().X, LHTransform.GetLocation().Y, LHTransform.GetLocation().Z);
	}
}

bool USuspenseCoreCharacterAnimInstance::GetWeaponLHTargetTransform(FTransform& OutTransform) const
{
	// Get LH_Target socket transform from weapon mesh and convert to CHARACTER's Component Space
	// This is the key to making left hand follow weapon rotation!

	if (!CachedWeaponActor.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] GetWeaponLHTargetTransform: CachedWeaponActor is INVALID!"));
		return false;
	}

	if (!CachedCharacter.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] GetWeaponLHTargetTransform: CachedCharacter is INVALID!"));
		return false;
	}

	AActor* WeaponActor = CachedWeaponActor.Get();

	// Get character mesh for coordinate conversion
	USkeletalMeshComponent* CharacterMesh = CachedCharacter->GetMesh();
	if (!CharacterMesh)
	{
		return false;
	}

	// Try to get skeletal mesh component from weapon
	USkeletalMeshComponent* WeaponMesh = WeaponActor->FindComponentByClass<USkeletalMeshComponent>();
	if (!WeaponMesh)
	{
		// Fallback to static mesh if no skeletal mesh
		UStaticMeshComponent* StaticMesh = WeaponActor->FindComponentByClass<UStaticMeshComponent>();
		if (StaticMesh && StaticMesh->DoesSocketExist(LHTargetSocketName))
		{
			// Get socket in WORLD space, then convert to CHARACTER's Component Space
			const FTransform SocketWorldTransform = StaticMesh->GetSocketTransform(LHTargetSocketName, RTS_World);
			OutTransform = SocketWorldTransform.GetRelativeTransform(CharacterMesh->GetComponentTransform());
			return true;
		}
		return false;
	}

	// Check if socket exists
	if (!WeaponMesh->DoesSocketExist(LHTargetSocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] GetWeaponLHTargetTransform: Socket '%s' NOT FOUND on weapon mesh!"), *LHTargetSocketName.ToString());
		return false;
	}

	// Get socket transform in WORLD Space first
	const FTransform SocketWorldTransform = WeaponMesh->GetSocketTransform(LHTargetSocketName, RTS_World);

	// Convert from World Space to CHARACTER's Component Space
	// This makes the transform relative to the character mesh, which is what AnimGraph expects
	OutTransform = SocketWorldTransform.GetRelativeTransform(CharacterMesh->GetComponentTransform());

	UE_LOG(LogTemp, Verbose, TEXT("[AnimInstance] GetWeaponLHTargetTransform: SUCCESS! Socket Loc=(%.2f,%.2f,%.2f)"),
		OutTransform.GetLocation().X, OutTransform.GetLocation().Y, OutTransform.GetLocation().Z);

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

	// Debug: log which path is taken
	static int32 LHDebugCounter = 0;
	const bool bShouldLog = (++LHDebugCounter % 60 == 0);

	// During montages, don't apply IK offset (let montage control hands)
	if (bIsWeaponMontageActive)
	{
		if (bShouldLog) UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] ComputeLHOffset: MONTAGE ACTIVE - returning Identity"));
		return FTransform::Identity;
	}

	// If Modify Grip is true, use DataTable grip transforms
	if (bModifyGrip)
	{
		if (bShouldLog) UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] ComputeLHOffset: bModifyGrip=true, DTLHGripTransform.Num=%d"), DTLHGripTransform.Num());
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
	if (bShouldLog) UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] ComputeLHOffset: Trying to get socket transform..."));

	FTransform SocketTransform;
	if (GetWeaponLHTargetTransform(SocketTransform))
	{
		if (bShouldLog) UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] ComputeLHOffset: GOT SOCKET! Loc=(%.2f,%.2f,%.2f)"),
			SocketTransform.GetLocation().X, SocketTransform.GetLocation().Y, SocketTransform.GetLocation().Z);
		return SocketTransform;
	}

	if (bShouldLog) UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] ComputeLHOffset: Socket FAILED, using fallback..."));

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

	if (bShouldLog) UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] ComputeLHOffset: ALL FALLBACKS FAILED - returning Identity!"));
	return FTransform::Identity;
}

void USuspenseCoreCharacterAnimInstance::UpdateAimOffsetData(float DeltaSeconds)
{
	APawn* OwnerPawn = TryGetPawnOwner();
	if (!OwnerPawn)
	{
		return;
	}

	// ═══════════════════════════════════════════════════════════════════════════════
	// AIM OFFSET CALCULATION
	// Using GetBaseAimRotation() - the standard UE method that:
	// - For locally controlled pawns: returns Controller rotation
	// - For simulated proxies: uses RemoteViewPitch (replicated)
	// - Handles all edge cases (no controller, AI, etc.)
	// ═══════════════════════════════════════════════════════════════════════════════

	// GetBaseAimRotation() returns the rotation to use for aim offset
	// This properly handles both local and remote characters
	const FRotator AimRotation = OwnerPawn->GetBaseAimRotation();
	const FRotator ActorRotation = OwnerPawn->GetActorRotation();

	// Calculate delta between aim rotation and actor rotation
	// This gives us how far the character is looking relative to their body
	const FRotator DeltaRotation = UKismetMathLibrary::NormalizedDeltaRotator(
		AimRotation,
		ActorRotation
	);

	// Clamp values
	AimYaw = FMath::Clamp(DeltaRotation.Yaw, -180.0f, 180.0f);
	AimPitch = FMath::Clamp(DeltaRotation.Pitch, -90.0f, 90.0f);

	// Debug logging (every 60 frames ~ 1 second at 60fps)
	static int32 FrameCounter = 0;
	if (++FrameCounter % 60 == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] AimOffset: AimRotation(Y=%.2f,P=%.2f) - ActorRot(Y=%.2f,P=%.2f) => AimYaw=%.2f, AimPitch=%.2f"),
			AimRotation.Yaw, AimRotation.Pitch,
			ActorRotation.Yaw, ActorRotation.Pitch,
			AimYaw, AimPitch);
	}

	// Turn in place detection
	const float TurnThreshold = 70.0f;
	bShouldTurnInPlace = FMath::Abs(AimYaw) > TurnThreshold && GroundSpeed < 10.0f;
	TurnInPlaceAngle = AimYaw;
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
		UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] LoadWeaponAnimationsTable: Loaded '%s' from Project Settings"),
			WeaponAnimationsTable ? *WeaponAnimationsTable->GetName() : TEXT("NULL"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] LoadWeaponAnimationsTable: WeaponAnimationsTable NOT SET in Project Settings! Path: Project Settings → Game → SuspenseCore → Animation System"));
	}
}

const FAnimationStateData* USuspenseCoreCharacterAnimInstance::GetAnimationDataForWeaponType(const FGameplayTag& WeaponType) const
{
	if (!WeaponAnimationsTable)
	{
		// Log once per session
		static bool bLoggedNoTable = false;
		if (!bLoggedNoTable)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] GetAnimationDataForWeaponType: WeaponAnimationsTable is NULL! Set it in Project Settings → Game → SuspenseCore"));
			bLoggedNoTable = true;
		}
		return nullptr;
	}

	if (!WeaponType.IsValid())
	{
		return nullptr;
	}

	// ═══════════════════════════════════════════════════════════════════════════════
	// SSOT: Use FSuspenseCoreAnimationHelpers to map WeaponArchetype → Row Name
	//
	// Mapping (defined in SuspenseCoreAnimationState.h):
	// - Weapon.Rifle.* (except Sniper) → SMG
	// - Weapon.SMG.* → SMG
	// - Weapon.Pistol.* → Pistol
	// - Weapon.Shotgun.* → Shotgun
	// - *Sniper* → Sniper
	// - Weapon.Melee.Knife → Knife
	// - Weapon.Melee.* → Special
	// - Weapon.Heavy.* → Special
	// - Weapon.Throwable.* → Frag
	// ═══════════════════════════════════════════════════════════════════════════════

	// Verify DataTable uses correct row structure
	const UScriptStruct* RowStruct = WeaponAnimationsTable->GetRowStruct();
	if (!RowStruct || RowStruct != FSuspenseCoreAnimationData::StaticStruct())
	{
		// Log once per session with helpful info
		static bool bLoggedStructMismatch = false;
		if (!bLoggedStructMismatch)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] GetAnimationDataForWeaponType: DataTable '%s' uses struct '%s', expected 'FSuspenseCoreAnimationData'. C++ path disabled, using Blueprint path."),
				*WeaponAnimationsTable->GetName(),
				RowStruct ? *RowStruct->GetName() : TEXT("NULL"));
			bLoggedStructMismatch = true;
		}
		return nullptr;
	}

	// Use SSOT helper for row name mapping
	const FName RowName = FSuspenseCoreAnimationHelpers::GetRowNameFromWeaponArchetype(WeaponType);

	if (RowName == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] GetAnimationDataForWeaponType: Invalid weapon type '%s'"), *WeaponType.ToString());
		return nullptr;
	}

	const FSuspenseCoreAnimationData* Result = WeaponAnimationsTable->FindRow<FSuspenseCoreAnimationData>(RowName, TEXT("GetAnimationDataForWeaponType"));

	if (!Result)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] GetAnimationDataForWeaponType: Row '%s' not found in DataTable for weapon '%s'"),
			*RowName.ToString(), *WeaponType.ToString());
	}

	return Result;
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
	const FName RowName = GetLegacyRowNameFromArchetypeTag(CurrentWeaponType);

	// Debug logging
	UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] GetLegacyRowNameFromArchetype: CurrentWeaponType='%s' => RowName='%s'"),
		*CurrentWeaponType.ToString(),
		*RowName.ToString());

	return RowName;
}

FName USuspenseCoreCharacterAnimInstance::GetLegacyRowNameFromArchetypeTag(const FGameplayTag& WeaponArchetype)
{
	// ═══════════════════════════════════════════════════════════════════════════════
	// SSOT: Delegate to FSuspenseCoreAnimationHelpers
	// This eliminates code duplication and ensures consistent mapping across codebase
	// ═══════════════════════════════════════════════════════════════════════════════

	FName RowName = FSuspenseCoreAnimationHelpers::GetRowNameFromWeaponArchetype(WeaponArchetype);

	// Helper returns NAME_None for invalid tags, but legacy expects "SMG" default
	if (RowName == NAME_None)
	{
		return FName("SMG");
	}

	return RowName;
}
