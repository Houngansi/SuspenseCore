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
#include "CineCameraComponent.h"

#if WITH_EQUIPMENT_SYSTEM
#include "SuspenseCore/Components/SuspenseCoreWeaponStanceComponent.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeaponAnimation.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeapon.h"
#endif

const FName USuspenseCoreCharacterAnimInstance::LHTargetSocketName = TEXT("LH_Target");

USuspenseCoreCharacterAnimInstance::USuspenseCoreCharacterAnimInstance()
{
}

void USuspenseCoreCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

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
	UpdateLeftHandSocket();
	UpdateADSData(DeltaSeconds);
	UpdateAimOffsetData(DeltaSeconds);
	UpdatePoseStates(DeltaSeconds);
	UpdateGASAttributes();
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
		bIsWeaponBlocked = false;
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
	}

	CurrentWeaponType = Snapshot.WeaponType;
	bHasWeaponEquipped = CurrentWeaponType.IsValid();
	bIsWeaponDrawn = Snapshot.bIsDrawn;

	// Combat states
	bIsAiming = Snapshot.bIsAiming;
	bIsFiring = Snapshot.bIsFiring;
	bIsReloading = Snapshot.bIsReloading;
	bIsHoldingBreath = Snapshot.bIsHoldingBreath;
	bIsWeaponBlocked = Snapshot.bIsWeaponBlocked;
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
	// Debug: Log every frame when weapon type changes
	static FGameplayTag LastLoggedType;
	if (CurrentWeaponType != LastLoggedType)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] UpdateAnimationAssets: WeaponType=%s, HasWeapon=%s, IsDrawn=%s"),
			*CurrentWeaponType.ToString(),
			bHasWeaponEquipped ? TEXT("YES") : TEXT("NO"),
			bIsWeaponDrawn ? TEXT("YES") : TEXT("NO"));
		LastLoggedType = CurrentWeaponType;
	}

	if (!bHasWeaponEquipped || !CurrentWeaponType.IsValid())
	{
		CurrentAnimationData = FSuspenseCoreAnimationData();
		return;
	}

	const FSuspenseCoreAnimationData* AnimData = GetAnimationDataForWeaponType(CurrentWeaponType);
	if (!AnimData)
	{
		UE_LOG(LogTemp, Error, TEXT("[AnimInstance] GetAnimationDataForWeaponType returned NULL for: %s"), *CurrentWeaponType.ToString());
		return;
	}

	// Log when animation data is loaded
	static FGameplayTag LastLoadedType;
	if (CurrentWeaponType != LastLoadedType)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] Animation data loaded: Stance=%s, Locomotion=%s"),
			AnimData->Stance ? TEXT("VALID") : TEXT("NULL"),
			AnimData->Locomotion ? TEXT("VALID") : TEXT("NULL"));
		LastLoadedType = CurrentWeaponType;
	}

	CurrentAnimationData = *AnimData;
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

	// BlockDistance is already interpolated in StanceComponent, no need to double-interpolate

	if (!bHasWeaponEquipped)
	{
		// Reset interpolated transforms when no weapon
		InterpolatedRHTransform = FTransform::Identity;
		InterpolatedLHTransform = FTransform::Identity;
		return;
	}

	// ═══════════════════════════════════════════════════════════════════════════════
	// RIGHT HAND TRANSFORM (from DataTable RHTransform)
	// ═══════════════════════════════════════════════════════════════════════════════
	FTransform TargetRHTransform = CurrentAnimationData.RHTransform;

	// ═══════════════════════════════════════════════════════════════════════════════
	// LEFT HAND TRANSFORM (from DataTable LHGripTransform - SSOT)
	// ═══════════════════════════════════════════════════════════════════════════════
	FTransform TargetLHTransform = ComputeLHOffsetTransform();

	// ═══════════════════════════════════════════════════════════════════════════════
	// INTERPOLATE TRANSFORMS (like legacy TInterpTo nodes)
	// ═══════════════════════════════════════════════════════════════════════════════
	InterpolatedRHTransform = UKismetMathLibrary::TInterpTo(InterpolatedRHTransform, TargetRHTransform, DeltaSeconds, TransformInterpSpeed);
	RightHandIKTransform = InterpolatedRHTransform;

	InterpolatedLHTransform = UKismetMathLibrary::TInterpTo(InterpolatedLHTransform, TargetLHTransform, DeltaSeconds, TransformInterpSpeed);
	LeftHandIKTransform = InterpolatedLHTransform;

	// Weapon transform (direct assignment, no interpolation needed)
	WeaponTransform = CurrentAnimationData.WTransform;

	// ═══════════════════════════════════════════════════════════════════════════════
	// IK VALIDITY CHECK
	// Only disable IK if transforms are truly Identity (0,0,0 location + identity rotation)
	// Don't disable for large transform values!
	// ═══════════════════════════════════════════════════════════════════════════════
	const bool bRHIsIdentity = RightHandIKTransform.GetLocation().IsNearlyZero(0.01f) &&
	                           RightHandIKTransform.GetRotation().IsIdentity(0.001f);
	const bool bLHIsIdentity = LeftHandIKTransform.GetLocation().IsNearlyZero(0.01f) &&
	                           LeftHandIKTransform.GetRotation().IsIdentity(0.001f);

	// Keep IK enabled if transforms have ANY data
	if (bRHIsIdentity) RightHandIKAlpha = 0.0f;
	if (bLHIsIdentity) LeftHandIKAlpha = 0.0f;
}

void USuspenseCoreCharacterAnimInstance::UpdateLeftHandSocket()
{
	// ═══════════════════════════════════════════════════════════════════════════════
	// SIMPLE LEFT HAND SOCKET TRACKING
	// Просто берём world позицию сокета LH_Target на оружии каждый кадр
	// В AnimBP: Two Bone IK → Effector Location = LeftHandSocketLocation (World Space)
	// ═══════════════════════════════════════════════════════════════════════════════

	// Debug logging (throttled - once per second)
	static double LastLogTime = 0.0;
	const double CurrentTime = FPlatformTime::Seconds();
	const bool bShouldLog = (CurrentTime - LastLogTime) > 1.0;

	// Reset if no weapon
	if (!bHasWeaponEquipped || !bIsWeaponDrawn)
	{
		if (bShouldLog)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LH_IK] UpdateLeftHandSocket: No weapon equipped/drawn. bHasWeapon=%s, bIsDrawn=%s"),
				bHasWeaponEquipped ? TEXT("YES") : TEXT("NO"),
				bIsWeaponDrawn ? TEXT("YES") : TEXT("NO"));
			LastLogTime = CurrentTime;
		}
		bHasLeftHandSocket = false;
		LeftHandSocketLocation = FVector::ZeroVector;
		LeftHandSocketRotation = FRotator::ZeroRotator;
		return;
	}

#if WITH_EQUIPMENT_SYSTEM
	if (!CachedWeaponActor.IsValid())
	{
		if (bShouldLog)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LH_IK] UpdateLeftHandSocket: CachedWeaponActor is INVALID!"));
			LastLogTime = CurrentTime;
		}
		bHasLeftHandSocket = false;
		return;
	}

	AActor* WeaponActor = CachedWeaponActor.Get();
	if (bShouldLog)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LH_IK] UpdateLeftHandSocket: WeaponActor=%s, Looking for socket '%s'"),
			*GetNameSafe(WeaponActor), *LHTargetSocketName.ToString());
	}

	// Try skeletal mesh first
	if (USkeletalMeshComponent* WeaponMesh = WeaponActor->FindComponentByClass<USkeletalMeshComponent>())
	{
		if (bShouldLog)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LH_IK]   Found SkeletalMeshComponent, DoesSocketExist('%s') = %s"),
				*LHTargetSocketName.ToString(),
				WeaponMesh->DoesSocketExist(LHTargetSocketName) ? TEXT("YES") : TEXT("NO"));
		}

		if (WeaponMesh->DoesSocketExist(LHTargetSocketName))
		{
			const FTransform SocketWorldTransform = WeaponMesh->GetSocketTransform(LHTargetSocketName, RTS_World);
			LeftHandSocketLocation = SocketWorldTransform.GetLocation();
			LeftHandSocketRotation = SocketWorldTransform.GetRotation().Rotator();
			bHasLeftHandSocket = true;

			if (bShouldLog)
			{
				UE_LOG(LogTemp, Warning, TEXT("[LH_IK]   SUCCESS! Socket Location: %s, Rotation: %s"),
					*LeftHandSocketLocation.ToString(), *LeftHandSocketRotation.ToString());
				LastLogTime = CurrentTime;
			}
			return;
		}
	}

	// Fallback to static mesh
	if (UStaticMeshComponent* StaticMesh = WeaponActor->FindComponentByClass<UStaticMeshComponent>())
	{
		if (bShouldLog)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LH_IK]   Found StaticMeshComponent, DoesSocketExist('%s') = %s"),
				*LHTargetSocketName.ToString(),
				StaticMesh->DoesSocketExist(LHTargetSocketName) ? TEXT("YES") : TEXT("NO"));
		}

		if (StaticMesh->DoesSocketExist(LHTargetSocketName))
		{
			const FTransform SocketWorldTransform = StaticMesh->GetSocketTransform(LHTargetSocketName, RTS_World);
			LeftHandSocketLocation = SocketWorldTransform.GetLocation();
			LeftHandSocketRotation = SocketWorldTransform.GetRotation().Rotator();
			bHasLeftHandSocket = true;

			if (bShouldLog)
			{
				UE_LOG(LogTemp, Warning, TEXT("[LH_IK]   SUCCESS! Socket Location: %s, Rotation: %s"),
					*LeftHandSocketLocation.ToString(), *LeftHandSocketRotation.ToString());
				LastLogTime = CurrentTime;
			}
			return;
		}
	}

	// Socket not found
	if (bShouldLog)
	{
		UE_LOG(LogTemp, Error, TEXT("[LH_IK] UpdateLeftHandSocket: Socket '%s' NOT FOUND on weapon '%s'!"),
			*LHTargetSocketName.ToString(), *GetNameSafe(WeaponActor));
		LastLogTime = CurrentTime;
	}
	bHasLeftHandSocket = false;
#else
	bHasLeftHandSocket = false;
#endif
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

	// Socket transform from weapon (LH_Target socket) - единственный источник данных
	FTransform SocketTransform;
	if (GetWeaponLHTargetTransform(SocketTransform))
	{
		return SocketTransform;
	}

	return FTransform::Identity;
}

void USuspenseCoreCharacterAnimInstance::UpdateADSData(float DeltaSeconds)
{
	// ═══════════════════════════════════════════════════════════════════════════════
	// ADS (AIM DOWN SIGHT) - PROCEDURAL WEAPON TO HEAD
	// Вычисляет offset для hand_r чтобы Sight_Socket оружия совпал с ADS_Target на голове
	// ═══════════════════════════════════════════════════════════════════════════════

	// Reset flags
	bHasADSTarget = false;
	bHasSightSocket = false;

	if (!CachedCharacter.IsValid())
	{
		ADSHandOffset = FVector::ZeroVector;
		ADSHandOffsetRaw = FVector::ZeroVector;
		return;
	}

	// ═══════════════════════════════════════════════════════════════════════════════
	// STEP 1: Get ADS_Target socket position on CHARACTER
	// Search ALL skeletal mesh components (MetaHuman has Body + Face meshes!)
	// ═══════════════════════════════════════════════════════════════════════════════
	USkeletalMeshComponent* MeshWithADSSocket = nullptr;

	TArray<USkeletalMeshComponent*> SkeletalMeshes;
	CachedCharacter->GetComponents<USkeletalMeshComponent>(SkeletalMeshes);

	for (USkeletalMeshComponent* Mesh : SkeletalMeshes)
	{
		if (Mesh && Mesh->DoesSocketExist(ADSTargetSocketName))
		{
			ADSTargetLocation = Mesh->GetSocketLocation(ADSTargetSocketName);
			MeshWithADSSocket = Mesh;
			bHasADSTarget = true;
			break;
		}
	}

	if (!bHasADSTarget)
	{
		ADSHandOffset = FVector::ZeroVector;
		ADSHandOffsetRaw = FVector::ZeroVector;
		return;
	}

	// ═══════════════════════════════════════════════════════════════════════════════
	// STEP 2: Get Sight_Socket position on WEAPON
	// ═══════════════════════════════════════════════════════════════════════════════
#if WITH_EQUIPMENT_SYSTEM
	if (CachedWeaponActor.IsValid() && bHasWeaponEquipped && bIsWeaponDrawn)
	{
		AActor* WeaponActor = CachedWeaponActor.Get();

		if (USkeletalMeshComponent* WeaponMesh = WeaponActor->FindComponentByClass<USkeletalMeshComponent>())
		{
			if (WeaponMesh->DoesSocketExist(ADSSightSocketName))
			{
				ADSSightLocation = WeaponMesh->GetSocketLocation(ADSSightSocketName);
				bHasSightSocket = true;
			}
		}
		else if (UStaticMeshComponent* StaticMesh = WeaponActor->FindComponentByClass<UStaticMeshComponent>())
		{
			if (StaticMesh->DoesSocketExist(ADSSightSocketName))
			{
				ADSSightLocation = StaticMesh->GetSocketLocation(ADSSightSocketName);
				bHasSightSocket = true;
			}
		}
	}
#endif

	if (!bHasSightSocket)
	{
		// No weapon or no sight socket - interpolate offset to zero
		ADSHandOffsetRaw = FMath::VInterpTo(ADSHandOffsetRaw, FVector::ZeroVector, DeltaSeconds, ADSInterpSpeed);
		ADSHandOffset = ADSHandOffsetRaw * AimingAlpha;
		return;
	}

	// ═══════════════════════════════════════════════════════════════════════════════
	// STEP 3: Calculate offset to move Sight_Socket to ADS_Target
	// ═══════════════════════════════════════════════════════════════════════════════

	// World space delta: where sight needs to go - where it currently is
	const FVector WorldDelta = ADSTargetLocation - ADSSightLocation;

	// Convert to Component Space using the MAIN character mesh (for AnimBP)
	USkeletalMeshComponent* MainMesh = CachedCharacter->GetMesh();
	if (!MainMesh)
	{
		ADSHandOffset = FVector::ZeroVector;
		return;
	}

	const FVector ComponentDelta = MainMesh->GetComponentTransform().InverseTransformVector(WorldDelta);

	// Add fine-tune offset
	const FVector TargetOffset = ComponentDelta + ADSFinetuneOffset;

	// Interpolate for smooth transition
	ADSHandOffsetRaw = FMath::VInterpTo(ADSHandOffsetRaw, TargetOffset, DeltaSeconds, ADSInterpSpeed);

	// Final offset multiplied by AimingAlpha (0 = hip fire, 1 = full ADS)
	ADSHandOffset = ADSHandOffsetRaw * AimingAlpha;
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
	const USuspenseCoreSettings* Settings = GetDefault<USuspenseCoreSettings>();
	if (!Settings)
	{
		return;
	}

	if (!Settings->WeaponAnimationsTable.IsNull())
	{
		WeaponAnimationsTable = Settings->WeaponAnimationsTable.LoadSynchronous();
	}
}

const FSuspenseCoreAnimationData* USuspenseCoreCharacterAnimInstance::GetAnimationDataForWeaponType(const FGameplayTag& WeaponType) const
{
	if (!WeaponAnimationsTable)
	{
		UE_LOG(LogTemp, Error, TEXT("[AnimInstance] GetAnimationData FAIL: WeaponAnimationsTable is NULL! Check Project Settings -> SuspenseCore -> WeaponAnimationsTable"));
		return nullptr;
	}

	if (!WeaponType.IsValid())
	{
		return nullptr;
	}

	// Verify DataTable uses correct row structure
	const UScriptStruct* RowStruct = WeaponAnimationsTable->GetRowStruct();
	if (!RowStruct || RowStruct != FSuspenseCoreAnimationData::StaticStruct())
	{
		UE_LOG(LogTemp, Error, TEXT("[AnimInstance] GetAnimationData FAIL: DataTable row struct mismatch! Expected FSuspenseCoreAnimationData"));
		return nullptr;
	}

	const FName RowName = FSuspenseCoreAnimationHelpers::GetRowNameFromWeaponArchetype(WeaponType);
	if (RowName == NAME_None)
	{
		UE_LOG(LogTemp, Error, TEXT("[AnimInstance] GetAnimationData FAIL: GetRowNameFromWeaponArchetype returned None for: %s"), *WeaponType.ToString());
		return nullptr;
	}

	const FSuspenseCoreAnimationData* Result = WeaponAnimationsTable->FindRow<FSuspenseCoreAnimationData>(RowName, TEXT("GetAnimationDataForWeaponType"));
	if (!Result)
	{
		UE_LOG(LogTemp, Error, TEXT("[AnimInstance] GetAnimationData FAIL: Row '%s' not found in DataTable for WeaponType: %s"),
			*RowName.ToString(), *WeaponType.ToString());
	}

	return Result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ANIMATION ASSET GETTERS
// ═══════════════════════════════════════════════════════════════════════════════

UBlendSpace* USuspenseCoreCharacterAnimInstance::GetStance() const
{
	return CurrentAnimationData.Stance;
}

UBlendSpace1D* USuspenseCoreCharacterAnimInstance::GetLocomotion() const
{
	return CurrentAnimationData.Locomotion;
}

UAnimSequence* USuspenseCoreCharacterAnimInstance::GetIdle() const
{
	return CurrentAnimationData.Idle;
}

// ═══════════════════════════════════════════════════════════════════════════════
// AIM ANIMATION GETTERS
// ═══════════════════════════════════════════════════════════════════════════════

UAnimComposite* USuspenseCoreCharacterAnimInstance::GetAimPose() const
{
	return CurrentAnimationData.AimPose;
}

UAnimSequence* USuspenseCoreCharacterAnimInstance::GetAimIn() const
{
	return CurrentAnimationData.AimIn;
}

UAnimSequence* USuspenseCoreCharacterAnimInstance::GetAimIdle() const
{
	// Fallback to Idle if AimIdle is NULL to prevent Sequence Evaluator crash
	return CurrentAnimationData.AimIdle ? CurrentAnimationData.AimIdle : CurrentAnimationData.Idle;
}

UAnimSequence* USuspenseCoreCharacterAnimInstance::GetAimOut() const
{
	return CurrentAnimationData.AimOut;
}

UAnimComposite* USuspenseCoreCharacterAnimInstance::GetGripPoses() const
{
	return CurrentAnimationData.GripPoses;
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
