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

const FName USuspenseCoreCharacterAnimInstance::LHTargetSocketName = TEXT("LH_Target");

USuspenseCoreCharacterAnimInstance::USuspenseCoreCharacterAnimInstance()
{
}

void USuspenseCoreCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	UE_LOG(LogTemp, Warning, TEXT("╔══════════════════════════════════════════════════════════════╗"));
	UE_LOG(LogTemp, Warning, TEXT("║ [AnimInstance] NativeInitializeAnimation CALLED!            ║"));
	UE_LOG(LogTemp, Warning, TEXT("║ Owner: %s"), TryGetPawnOwner() ? *TryGetPawnOwner()->GetName() : TEXT("NULL"));
	UE_LOG(LogTemp, Warning, TEXT("╚══════════════════════════════════════════════════════════════╝"));

	UpdateCachedReferences();
	LoadWeaponAnimationsTable();
}

void USuspenseCoreCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

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

	UpdateMovementData(DeltaSeconds);
	UpdateVelocityData(DeltaSeconds);
	UpdateWeaponData(DeltaSeconds);
	UpdateAnimationAssets();
	UpdateIKData(DeltaSeconds);
	UpdateAimOffsetData(DeltaSeconds);
	UpdatePoseStates(DeltaSeconds);
	UpdateGASAttributes();

	// DEBUG: Log key variables every 3 seconds when weapon equipped
	static float LastKeyVarLogTime = 0.0f;
	if (bHasWeaponEquipped && (CurrentTime - LastKeyVarLogTime) > 3.0f)
	{
		LastKeyVarLogTime = CurrentTime;
		UE_LOG(LogTemp, Warning, TEXT("═══════════════════ ANIM STATE DEBUG ═══════════════════"));
		UE_LOG(LogTemp, Warning, TEXT("Movement: MoveForward=%.2f, MoveRight=%.2f, Speed=%.2f, Movement=%.2f"),
			MoveForward, MoveRight, Speed, Movement);
		UE_LOG(LogTemp, Warning, TEXT("Weapon: bHasWeapon=%d, bIsDrawn=%d, bIsHolstered=%d, WeaponType=%s"),
			bHasWeaponEquipped, bIsWeaponDrawn, bIsHolstered, *CurrentWeaponType.ToString());
		UE_LOG(LogTemp, Warning, TEXT("States: bIsAiming=%d, bIsFiring=%d, bIsReloading=%d, AimingAlpha=%.2f"),
			bIsAiming, bIsFiring, bIsReloading, AimingAlpha);
		UE_LOG(LogTemp, Warning, TEXT("AnimData: Stance=%s, Idle=%s, AimPose=%s"),
			CurrentAnimationData.Stance ? *CurrentAnimationData.Stance->GetName() : TEXT("NULL"),
			CurrentAnimationData.Idle ? *CurrentAnimationData.Idle->GetName() : TEXT("NULL"),
			CurrentAnimationData.AimPose ? *CurrentAnimationData.AimPose->GetName() : TEXT("NULL"));
		UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════════════════════"));
	}
}

void USuspenseCoreCharacterAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);
}

void USuspenseCoreCharacterAnimInstance::UpdateCachedReferences()
{
	APawn* OwnerPawn = TryGetPawnOwner();
	if (!OwnerPawn)
	{
		return;
	}

	CachedCharacter = Cast<ASuspenseCoreCharacter>(OwnerPawn);

	if (ACharacter* Character = Cast<ACharacter>(OwnerPawn))
	{
		CachedMovementComponent = Character->GetCharacterMovement();
	}

#if WITH_EQUIPMENT_SYSTEM
	CachedStanceComponent = OwnerPawn->FindComponentByClass<USuspenseCoreWeaponStanceComponent>();
#endif

	if (CachedCharacter.IsValid())
	{
		CachedASC = CachedCharacter->GetAbilitySystemComponent();
	}
	else if (IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(OwnerPawn))
	{
		CachedASC = ASCInterface->GetAbilitySystemComponent();
	}

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

	MovementState = Character->GetMovementState();
	bIsSprinting = Character->IsSprinting();
	bIsCrouching = (MovementState == ESuspenseCoreMovementState::Crouching);
	bIsJumping = (MovementState == ESuspenseCoreMovementState::Jumping);
	bIsFalling = (MovementState == ESuspenseCoreMovementState::Falling);
	bIsInAir = bIsJumping || bIsFalling;
	bIsOnGround = !bIsInAir;
	bHasMovementInput = Character->HasMovementInput();

	const FVector Velocity = Character->GetVelocity();
	const float HorizontalSpeed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();

	if (HorizontalSpeed > 10.0f)
	{
		FRotator ReferenceRotation = Character->GetActorRotation();
		if (AController* Controller = Character->GetController())
		{
			ReferenceRotation = FRotator(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
		}

		const FRotator VelocityRotation = Velocity.ToOrientationRotator();
		const FRotator DeltaRotation = UKismetMathLibrary::NormalizedDeltaRotator(VelocityRotation, ReferenceRotation);

		const float AngleRad = FMath::DegreesToRadians(DeltaRotation.Yaw);
		float TargetForward = FMath::Cos(AngleRad);
		float TargetRight = FMath::Sin(AngleRad);

		const float SpeedMultiplier = bIsSprinting ? 2.0f : 1.0f;
		TargetForward *= SpeedMultiplier;
		TargetRight *= SpeedMultiplier;

		MoveForward = FMath::FInterpTo(MoveForward, TargetForward, DeltaSeconds, 10.0f);
		MoveRight = FMath::FInterpTo(MoveRight, TargetRight, DeltaSeconds, 10.0f);
	}
	else
	{
		MoveForward = FMath::FInterpTo(MoveForward, 0.0f, DeltaSeconds, 10.0f);
		MoveRight = FMath::FInterpTo(MoveRight, 0.0f, DeltaSeconds, 10.0f);
	}

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

	const FVector Velocity = OwnerPawn->GetVelocity();
	Speed = Velocity.Size();
	GroundSpeed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();
	VerticalVelocity = Velocity.Z;

	if (MaxWalkSpeed > 0.0f)
	{
		NormalizedSpeed = FMath::Clamp(GroundSpeed / MaxWalkSpeed, 0.0f, 2.0f);
	}

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
		// Reset weapon data
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
		bIsHolstered = true;
		bModifyGrip = false;
		bCreateAimPose = false;
		SightDistance = 200.0f;
		AimPose = 0;
		StoredPose = 0;
		GripID = 0;
		bWeaponTypeChanged = false;
		return;
	}

	USuspenseCoreWeaponStanceComponent* StanceComp = CachedStanceComponent.Get();
	CachedWeaponActor = StanceComp->GetTrackedEquipmentActor();

	const FSuspenseCoreWeaponStanceSnapshot Snapshot = StanceComp->GetStanceSnapshot();

	// Detect weapon type change
	bWeaponTypeChanged = (CurrentWeaponType != Snapshot.WeaponType);
	if (bWeaponTypeChanged)
	{
		PreviousWeaponType = CurrentWeaponType;
		UE_LOG(LogTemp, Log, TEXT("[AnimInstance] WeaponType changed: %s -> %s"),
			*PreviousWeaponType.ToString(), *Snapshot.WeaponType.ToString());
	}

	CurrentWeaponType = Snapshot.WeaponType;
	bHasWeaponEquipped = CurrentWeaponType.IsValid();
	bIsWeaponDrawn = Snapshot.bIsDrawn;

	// Combat states
	bIsAiming = Snapshot.bIsAiming;
	bIsFiring = Snapshot.bIsFiring;
	bIsReloading = Snapshot.bIsReloading;
	bIsHoldingBreath = Snapshot.bIsHoldingBreath;
	bIsWeaponMontageActive = Snapshot.bIsMontageActive;
	bIsHolstered = Snapshot.bIsHolstered;
	bModifyGrip = Snapshot.bModifyGrip;
	bCreateAimPose = Snapshot.bCreateAimPose;
	SightDistance = Snapshot.SightDistance;

	// Pose indices
	AimPose = Snapshot.AimPose;
	StoredPose = Snapshot.StoredPose;
	GripID = Snapshot.GripID;

	// Pose modifiers
	AimingAlpha = Snapshot.AimPoseAlpha;
	GripModifier = Snapshot.GripModifier;
	WeaponLoweredAlpha = Snapshot.WeaponLoweredAlpha;

	// Procedural animation
	RecoilAlpha = Snapshot.RecoilAlpha;
	WeaponSwayMultiplier = Snapshot.SwayMultiplier;
	StoredRecoil = Snapshot.StoredRecoil;
	AdditivePitch = Snapshot.AdditivePitch;
	BlockDistance = Snapshot.BlockDistance;

#else
	if (CachedCharacter.IsValid())
	{
		bHasWeaponEquipped = CachedCharacter->HasWeapon();
		bIsWeaponDrawn = bHasWeaponEquipped;
	}
#endif
}

void USuspenseCoreCharacterAnimInstance::UpdateAnimationAssets()
{
	// Debug: Log state every time weapon is equipped but no data loaded
	static float LastDebugLogTime = 0.0f;
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const bool bShouldLogDebug = (CurrentTime - LastDebugLogTime) > 2.0f; // Log every 2 seconds max

	if (!bHasWeaponEquipped || !CurrentWeaponType.IsValid())
	{
		CurrentAnimationData = FSuspenseCoreAnimationData();
		return;
	}

	const FSuspenseCoreAnimationData* AnimData = GetAnimationDataForWeaponType(CurrentWeaponType);
	if (!AnimData)
	{
		if (bWeaponTypeChanged || bShouldLogDebug)
		{
			LastDebugLogTime = CurrentTime;
			UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] Failed to load animation data for weapon '%s'."),
				*CurrentWeaponType.ToString());
			UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] DataTable=%s, bHasWeapon=%d, WeaponType=%s"),
				WeaponAnimationsTable ? *WeaponAnimationsTable->GetName() : TEXT("NULL"),
				bHasWeaponEquipped,
				*CurrentWeaponType.ToString());

			// Log the expected row name
			const FName ExpectedRowName = FSuspenseCoreAnimationHelpers::GetRowNameFromWeaponArchetype(CurrentWeaponType);
			UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] Expected RowName: '%s'"), *ExpectedRowName.ToString());

			// Log all available rows in DataTable
			if (WeaponAnimationsTable)
			{
				TArray<FName> RowNames = WeaponAnimationsTable->GetRowNames();
				FString RowNamesStr;
				for (const FName& Name : RowNames)
				{
					RowNamesStr += Name.ToString() + TEXT(", ");
				}
				UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] Available rows: [%s]"), *RowNamesStr);
			}
		}
		return;
	}

	CurrentAnimationData = *AnimData;

	if (bWeaponTypeChanged)
	{
		UE_LOG(LogTemp, Log, TEXT("[AnimInstance] SUCCESS! Loaded animation data for '%s'. Stance=%s"),
			*CurrentWeaponType.ToString(),
			CurrentAnimationData.Stance ? *CurrentAnimationData.Stance->GetName() : TEXT("NULL"));
	}
}

void USuspenseCoreCharacterAnimInstance::UpdateIKData(float DeltaSeconds)
{
	// IK is active when weapon is drawn
	const float TargetIKAlpha = (bIsWeaponDrawn && bHasWeaponEquipped) ? 1.0f : 0.0f;
	LeftHandIKAlpha = FMath::FInterpTo(LeftHandIKAlpha, TargetIKAlpha, DeltaSeconds, 10.0f);
	RightHandIKAlpha = FMath::FInterpTo(RightHandIKAlpha, TargetIKAlpha, DeltaSeconds, 10.0f);

	// Additive pitch interpolation
	{
		const float EaseAlpha = FMath::Clamp(DeltaSeconds * 10.0f, 0.0f, 1.0f);
		const float EasedAlpha = FMath::Pow(EaseAlpha, AdditivePitchBlendExp);
		InterpolatedAdditivePitch = FMath::Lerp(InterpolatedAdditivePitch, StoredRecoil, EasedAlpha);
		AdditivePitch = InterpolatedAdditivePitch;
	}

	// Block distance interpolation
	InterpolatedBlockDistance = FMath::FInterpTo(InterpolatedBlockDistance, BlockDistance, DeltaSeconds, BlockDistanceInterpSpeed);
	BlockDistance = InterpolatedBlockDistance;

	if (!bHasWeaponEquipped)
	{
		return;
	}

	// Get RH Transform from CurrentAnimationData
	FTransform TargetRHTransform = CurrentAnimationData.RHTransform;

	// Get LH Transform
	FTransform TargetLHTransform = ComputeLHOffsetTransform();

	// Apply aim blend to LH Transform
	if (bCreateAimPose && CurrentAnimationData.LHGripTransform.Num() > 1)
	{
		const int32 AimGripIndex = FMath::Clamp(AimPose, 0, CurrentAnimationData.LHGripTransform.Num() - 1);
		TargetLHTransform = UKismetMathLibrary::TLerp(
			TargetLHTransform,
			CurrentAnimationData.LHGripTransform[AimGripIndex],
			AimingAlpha
		);
	}

	// Interpolate transforms
	InterpolatedRHTransform = UKismetMathLibrary::TInterpTo(InterpolatedRHTransform, TargetRHTransform, DeltaSeconds, TransformInterpSpeed);
	RightHandIKTransform = InterpolatedRHTransform;

	InterpolatedLHTransform = UKismetMathLibrary::TInterpTo(InterpolatedLHTransform, TargetLHTransform, DeltaSeconds, TransformInterpSpeed);
	LeftHandIKTransform = InterpolatedLHTransform;

	// Weapon transform
	WeaponTransform = CurrentAnimationData.WTransform;

	// Disable IK if transforms are Identity
	const bool bRHValid = !RightHandIKTransform.GetLocation().IsNearlyZero(0.1f) || !RightHandIKTransform.GetRotation().IsIdentity(0.01f);
	const bool bLHValid = !LeftHandIKTransform.GetLocation().IsNearlyZero(0.1f) || !LeftHandIKTransform.GetRotation().IsIdentity(0.01f);

	if (!bRHValid) RightHandIKAlpha = 0.0f;
	if (!bLHValid) LeftHandIKAlpha = 0.0f;
}

bool USuspenseCoreCharacterAnimInstance::GetWeaponLHTargetTransform(FTransform& OutTransform) const
{
	if (!CachedWeaponActor.IsValid() || !CachedCharacter.IsValid())
	{
		return false;
	}

	USkeletalMeshComponent* CharacterMesh = CachedCharacter->GetMesh();
	if (!CharacterMesh)
	{
		return false;
	}

	AActor* WeaponActor = CachedWeaponActor.Get();

	// Try skeletal mesh first
	if (USkeletalMeshComponent* WeaponMesh = WeaponActor->FindComponentByClass<USkeletalMeshComponent>())
	{
		if (WeaponMesh->DoesSocketExist(LHTargetSocketName))
		{
			const FTransform SocketWorldTransform = WeaponMesh->GetSocketTransform(LHTargetSocketName, RTS_World);
			OutTransform = SocketWorldTransform.GetRelativeTransform(CharacterMesh->GetComponentTransform());
			return true;
		}
	}

	// Fallback to static mesh
	if (UStaticMeshComponent* StaticMesh = WeaponActor->FindComponentByClass<UStaticMeshComponent>())
	{
		if (StaticMesh->DoesSocketExist(LHTargetSocketName))
		{
			const FTransform SocketWorldTransform = StaticMesh->GetSocketTransform(LHTargetSocketName, RTS_World);
			OutTransform = SocketWorldTransform.GetRelativeTransform(CharacterMesh->GetComponentTransform());
			return true;
		}
	}

	return false;
}

FTransform USuspenseCoreCharacterAnimInstance::ComputeLHOffsetTransform() const
{
	// During montages, don't apply IK offset
	if (bIsWeaponMontageActive)
	{
		return FTransform::Identity;
	}

	// If ModifyGrip, use grip transforms from DataTable
	if (bModifyGrip && CurrentAnimationData.LHGripTransform.Num() > 0)
	{
		const int32 GripIndex = FMath::Clamp(GripID, 0, CurrentAnimationData.LHGripTransform.Num() - 1);
		FTransform BaseGrip = CurrentAnimationData.LHGripTransform[GripIndex];

		// Blend to aim grip if aiming
		if (bIsAiming)
		{
			const int32 AimGripIndex = FMath::Clamp((AimPose > 0) ? AimPose : 1, 0, CurrentAnimationData.LHGripTransform.Num() - 1);
			BaseGrip = UKismetMathLibrary::TLerp(BaseGrip, CurrentAnimationData.LHGripTransform[AimGripIndex], AimingAlpha);
		}
		return BaseGrip;
	}

	// Default: Get socket transform from weapon
	FTransform SocketTransform;
	if (GetWeaponLHTargetTransform(SocketTransform))
	{
		return SocketTransform;
	}

	// Fallback to DataTable LHTransform
	if (!CurrentAnimationData.LHTransform.Equals(FTransform::Identity))
	{
		return CurrentAnimationData.LHTransform;
	}

	// Fallback to grip transform array
	if (CurrentAnimationData.LHGripTransform.Num() > 0)
	{
		const int32 GripIndex = FMath::Clamp(GripID, 0, CurrentAnimationData.LHGripTransform.Num() - 1);
		return CurrentAnimationData.LHGripTransform[GripIndex];
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

	const FRotator AimRotation = OwnerPawn->GetBaseAimRotation();
	const FRotator ActorRotation = OwnerPawn->GetActorRotation();
	const FRotator DeltaRotation = UKismetMathLibrary::NormalizedDeltaRotator(AimRotation, ActorRotation);

	AimYaw = FMath::Clamp(DeltaRotation.Yaw, -180.0f, 180.0f);
	AimPitch = FMath::Clamp(DeltaRotation.Pitch, -90.0f, 90.0f);

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

	bIsSliding = false;

	if (CachedCharacter.IsValid())
	{
		TargetLean = MoveRight * 15.0f;
	}
	Lean = FMath::FInterpTo(Lean, TargetLean, DeltaSeconds, LeanInterpSpeed);

	BodyPitch = -AimPitch;

	const FRotator ActorRotation = OwnerPawn->GetActorRotation();

	LastTickYaw = CurrentYaw;
	CurrentYaw = ActorRotation.Yaw;

	const bool bShouldAccumulateYaw = (Movement <= 0.0f) && !bIsInAir;

	if (bShouldAccumulateYaw)
	{
		float YawDelta = FMath::UnwindDegrees(LastTickYaw - CurrentYaw);
		YawOffset += YawDelta;
		YawOffset = FMath::Clamp(FMath::UnwindDegrees(YawOffset), -MaxYawOffset, MaxYawOffset);
	}
	else
	{
		YawOffset = FMath::FInterpTo(YawOffset, 0.0f, DeltaSeconds, YawOffsetInterpSpeed);
	}

	IsTurningCurve = GetCurveValue(FName("IsTurning"));
	RotationCurve = GetCurveValue(FName("Rotation"));

	if (FMath::IsNearlyEqual(IsTurningCurve, 1.0f, 0.01f))
	{
		const float TargetYawFromCurve = RotationCurve - YawOffset;
		YawOffset = FMath::FInterpTo(YawOffset, TargetYawFromCurve, DeltaSeconds, YawOffsetCurveInterpSpeed);
	}
}

void USuspenseCoreCharacterAnimInstance::UpdateGASAttributes()
{
	if (!CachedMovementAttributes.IsValid())
	{
		if (CachedMovementComponent.IsValid())
		{
			MaxWalkSpeed = CachedMovementComponent->MaxWalkSpeed;
			MaxCrouchSpeed = CachedMovementComponent->MaxWalkSpeedCrouched;
			MaxSprintSpeed = MaxWalkSpeed * 1.5f;
			MaxAimSpeed = MaxWalkSpeed * 0.6f;
			JumpHeight = CachedMovementComponent->JumpZVelocity;
		}
		return;
	}

	const USuspenseCoreMovementAttributeSet* MovementAttrs = CachedMovementAttributes.Get();
	MaxWalkSpeed = MovementAttrs->GetWalkSpeed();
	MaxSprintSpeed = MovementAttrs->GetSprintSpeed();
	MaxCrouchSpeed = MovementAttrs->GetCrouchSpeed();
	MaxAimSpeed = MovementAttrs->GetAimSpeed();
	JumpHeight = MovementAttrs->GetJumpHeight();
}

void USuspenseCoreCharacterAnimInstance::LoadWeaponAnimationsTable()
{
	UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] LoadWeaponAnimationsTable() called"));

	const USuspenseCoreSettings* Settings = GetDefault<USuspenseCoreSettings>();
	if (!Settings)
	{
		UE_LOG(LogTemp, Error, TEXT("[AnimInstance] ERROR: USuspenseCoreSettings is NULL!"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] Settings found. WeaponAnimationsTable.IsNull() = %s"),
		Settings->WeaponAnimationsTable.IsNull() ? TEXT("TRUE (not set!)") : TEXT("FALSE (set)"));

	if (!Settings->WeaponAnimationsTable.IsNull())
	{
		WeaponAnimationsTable = Settings->WeaponAnimationsTable.LoadSynchronous();
		UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] ✓ Loaded WeaponAnimationsTable: %s"),
			WeaponAnimationsTable ? *WeaponAnimationsTable->GetName() : TEXT("NULL"));

		if (WeaponAnimationsTable)
		{
			TArray<FName> RowNames = WeaponAnimationsTable->GetRowNames();
			UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] DataTable has %d rows"), RowNames.Num());
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[AnimInstance] ERROR: WeaponAnimationsTable NOT SET in Project Settings!"));
	}
}

const FSuspenseCoreAnimationData* USuspenseCoreCharacterAnimInstance::GetAnimationDataForWeaponType(const FGameplayTag& WeaponType) const
{
	if (!WeaponAnimationsTable || !WeaponType.IsValid())
	{
		return nullptr;
	}

	// Verify DataTable uses correct row structure
	const UScriptStruct* RowStruct = WeaponAnimationsTable->GetRowStruct();
	if (!RowStruct || RowStruct != FSuspenseCoreAnimationData::StaticStruct())
	{
		static bool bLoggedOnce = false;
		if (!bLoggedOnce)
		{
			UE_LOG(LogTemp, Error, TEXT("[AnimInstance] DataTable '%s' uses wrong row struct '%s'. Expected 'FSuspenseCoreAnimationData'!"),
				*WeaponAnimationsTable->GetName(),
				RowStruct ? *RowStruct->GetName() : TEXT("NULL"));
			bLoggedOnce = true;
		}
		return nullptr;
	}

	const FName RowName = FSuspenseCoreAnimationHelpers::GetRowNameFromWeaponArchetype(WeaponType);
	if (RowName == NAME_None)
	{
		return nullptr;
	}

	const FSuspenseCoreAnimationData* Result = WeaponAnimationsTable->FindRow<FSuspenseCoreAnimationData>(RowName, TEXT("GetAnimationDataForWeaponType"));
	if (!Result)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] Row '%s' not found for weapon '%s'"), *RowName.ToString(), *WeaponType.ToString());
	}

	return Result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ANIMATION ASSET GETTERS (с логированием для отладки)
// ═══════════════════════════════════════════════════════════════════════════════

UBlendSpace* USuspenseCoreCharacterAnimInstance::GetStance() const
{
	UBlendSpace* Result = CurrentAnimationData.Stance;

	// Debug logging (раз в 2 секунды)
	static float LastLogTime = 0.0f;
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if ((CurrentTime - LastLogTime) > 2.0f && bHasWeaponEquipped)
	{
		LastLogTime = CurrentTime;
		UE_LOG(LogTemp, Warning, TEXT("[GetStance] Called! Returning: %s (bHasWeapon=%d, WeaponType=%s)"),
			Result ? *Result->GetName() : TEXT("NULL"),
			bHasWeaponEquipped,
			*CurrentWeaponType.ToString());
	}

	return Result;
}

UBlendSpace1D* USuspenseCoreCharacterAnimInstance::GetLocomotion() const
{
	return CurrentAnimationData.Locomotion;
}

UAnimSequence* USuspenseCoreCharacterAnimInstance::GetIdle() const
{
	return CurrentAnimationData.Idle;
}

UAnimComposite* USuspenseCoreCharacterAnimInstance::GetGripPoses() const
{
	UAnimComposite* Result = CurrentAnimationData.GripPoses;

	// Debug logging (раз в 3 секунды)
	static float LastLogTime = 0.0f;
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if ((CurrentTime - LastLogTime) > 3.0f && bHasWeaponEquipped)
	{
		LastLogTime = CurrentTime;
		UE_LOG(LogTemp, Warning, TEXT("[GetGripPoses] Returning: %s, GripID=%d, ModifyGrip=%d"),
			Result ? *Result->GetName() : TEXT("NULL"),
			GripID,
			bModifyGrip);
	}

	return Result;
}

UAnimSequenceBase* USuspenseCoreCharacterAnimInstance::GetGripPoseByIndex(int32 PoseIndex) const
{
	if (!CurrentAnimationData.GripPoses)
	{
		return nullptr;
	}

	const TArray<FAnimSegment>& Segments = CurrentAnimationData.GripPoses->AnimationTrack.AnimSegments;

	if (Segments.IsValidIndex(PoseIndex))
	{
		return Segments[PoseIndex].GetAnimReference();
	}

	if (Segments.Num() > 0)
	{
		return Segments[0].GetAnimReference();
	}

	return nullptr;
}

UAnimSequenceBase* USuspenseCoreCharacterAnimInstance::GetActiveGripPose() const
{
	int32 PoseIndex = GripID;

	if (bIsReloading && GripID == 0)
	{
		PoseIndex = 2;
	}
	else if (bIsAiming && bModifyGrip && AimPose > 0)
	{
		PoseIndex = AimPose;
	}
	else if (bIsAiming && GripID == 0)
	{
		PoseIndex = 1;
	}

	return GetGripPoseByIndex(PoseIndex);
}

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
