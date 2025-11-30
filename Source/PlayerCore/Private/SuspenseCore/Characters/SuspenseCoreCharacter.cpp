// SuspenseCoreCharacter.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Characters/SuspenseCoreCharacter.h"
#include "SuspenseCore/Core/SuspenseCorePlayerState.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"

// ═══════════════════════════════════════════════════════════════════════════════
// CONSTRUCTOR
// ═══════════════════════════════════════════════════════════════════════════════

ASuspenseCoreCharacter::ASuspenseCoreCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Camera boom
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetMesh());
	CameraBoom->TargetArmLength = 0.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = false;

	// Camera
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	CameraComponent->SetupAttachment(CameraBoom);
	CameraComponent->bUsePawnControlRotation = false;

	// First person mesh
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh1P"));
	Mesh1P->SetupAttachment(CameraComponent);
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;

	// Hide third person mesh for owner
	GetMesh()->SetOwnerNoSee(true);

	// Movement settings
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->MaxWalkSpeed = BaseWalkSpeed;
		CMC->bOrientRotationToMovement = false;
		CMC->bUseControllerDesiredRotation = true;
		CMC->NavAgentProps.bCanCrouch = true;
	}

	// Controller rotation
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CHARACTER LIFECYCLE
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCoreCharacter::BeginPlay()
{
	Super::BeginPlay();

	UpdateMovementSpeed();

	PublishCharacterEvent(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.Spawned")),
		TEXT("{}")
	);
}

void ASuspenseCoreCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CachedEventBus.Reset();
	CachedPlayerState.Reset();

	Super::EndPlay(EndPlayReason);
}

void ASuspenseCoreCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateMovementState();
	UpdateAnimationValues(DeltaTime);
}

void ASuspenseCoreCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Cache player state
	CachedPlayerState = Cast<ASuspenseCorePlayerState>(GetPlayerState());

	// Initialize ASC with this character as avatar
	if (ASuspenseCorePlayerState* PS = GetSuspenseCorePlayerState())
	{
		if (UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent())
		{
			ASC->InitAbilityActorInfo(PS, this);
		}
	}

	PublishCharacterEvent(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.Possessed")),
		TEXT("{}")
	);
}

void ASuspenseCoreCharacter::UnPossessed()
{
	PublishCharacterEvent(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.UnPossessed")),
		TEXT("{}")
	);

	CachedPlayerState.Reset();

	Super::UnPossessed();
}

void ASuspenseCoreCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	CachedPlayerState = Cast<ASuspenseCorePlayerState>(GetPlayerState());

	// Reinitialize ASC on client
	if (ASuspenseCorePlayerState* PS = GetSuspenseCorePlayerState())
	{
		if (UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent())
		{
			ASC->InitAbilityActorInfo(PS, this);
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// IABILITYSYSTEMINTERFACE
// ═══════════════════════════════════════════════════════════════════════════════

UAbilitySystemComponent* ASuspenseCoreCharacter::GetAbilitySystemComponent() const
{
	if (ASuspenseCorePlayerState* PS = GetSuspenseCorePlayerState())
	{
		return PS->GetAbilitySystemComponent();
	}
	return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API - MOVEMENT
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCoreCharacter::Move(const FVector2D& MovementInput)
{
	TargetMoveForward = MovementInput.Y;
	TargetMoveRight = MovementInput.X;

	if (Controller)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MovementInput.Y);
		AddMovementInput(RightDirection, MovementInput.X);
	}
}

void ASuspenseCoreCharacter::Look(const FVector2D& LookInput)
{
	if (Controller)
	{
		AddControllerYawInput(LookInput.X);
		AddControllerPitchInput(LookInput.Y);
	}
}

void ASuspenseCoreCharacter::StartSprinting()
{
	if (!bIsSprinting)
	{
		bIsSprinting = true;
		UpdateMovementSpeed();

		PublishCharacterEvent(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.SprintStarted")),
			TEXT("{}")
		);
	}
}

void ASuspenseCoreCharacter::StopSprinting()
{
	if (bIsSprinting)
	{
		bIsSprinting = false;
		UpdateMovementSpeed();

		PublishCharacterEvent(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.SprintStopped")),
			TEXT("{}")
		);
	}
}

void ASuspenseCoreCharacter::ToggleCrouch()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
	UpdateMovementSpeed();
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API - STATE
// ═══════════════════════════════════════════════════════════════════════════════

bool ASuspenseCoreCharacter::HasMovementInput() const
{
	return !FMath::IsNearlyZero(TargetMoveForward) || !FMath::IsNearlyZero(TargetMoveRight);
}

ASuspenseCorePlayerState* ASuspenseCoreCharacter::GetSuspenseCorePlayerState() const
{
	if (CachedPlayerState.IsValid())
	{
		return CachedPlayerState.Get();
	}

	ASuspenseCorePlayerState* PS = Cast<ASuspenseCorePlayerState>(GetPlayerState());
	if (PS)
	{
		const_cast<ASuspenseCoreCharacter*>(this)->CachedPlayerState = PS;
	}

	return PS;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API - ANIMATION
// ═══════════════════════════════════════════════════════════════════════════════

float ASuspenseCoreCharacter::GetAnimationForwardValue() const
{
	const float SprintMultiplier = bIsSprinting ? 2.0f : 1.0f;
	return MoveForwardValue * SprintMultiplier;
}

float ASuspenseCoreCharacter::GetAnimationRightValue() const
{
	const float SprintMultiplier = bIsSprinting ? 2.0f : 1.0f;
	return MoveRightValue * SprintMultiplier;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API - WEAPON
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCoreCharacter::SetHasWeapon(bool bNewHasWeapon)
{
	if (bHasWeapon != bNewHasWeapon)
	{
		bHasWeapon = bNewHasWeapon;

		PublishCharacterEvent(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.WeaponStateChanged")),
			FString::Printf(TEXT("{\"hasWeapon\":%s}"), bHasWeapon ? TEXT("true") : TEXT("false"))
		);
	}
}

void ASuspenseCoreCharacter::SetCurrentWeaponActor(AActor* WeaponActor)
{
	if (CurrentWeaponActor.Get() != WeaponActor)
	{
		CurrentWeaponActor = WeaponActor;
		SetHasWeapon(CurrentWeaponActor.IsValid());

		PublishCharacterEvent(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.WeaponChanged")),
			FString::Printf(TEXT("{\"weapon\":\"%s\"}"),
				WeaponActor ? *WeaponActor->GetName() : TEXT("None"))
		);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCoreCharacter::UpdateMovementState()
{
	ESuspenseCoreMovementState NewState = ESuspenseCoreMovementState::Idle;

	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		if (CMC->IsFalling())
		{
			NewState = ESuspenseCoreMovementState::Falling;
		}
		else if (bIsCrouched)
		{
			NewState = ESuspenseCoreMovementState::Crouching;
		}
		else if (bIsSprinting && HasMovementInput())
		{
			NewState = ESuspenseCoreMovementState::Sprinting;
		}
		else if (HasMovementInput())
		{
			NewState = ESuspenseCoreMovementState::Walking;
		}
	}

	if (CurrentMovementState != NewState)
	{
		PreviousMovementState = CurrentMovementState;
		CurrentMovementState = NewState;

		PublishCharacterEvent(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.MovementStateChanged")),
			FString::Printf(TEXT("{\"state\":%d}"), static_cast<int32>(CurrentMovementState))
		);
	}
}

void ASuspenseCoreCharacter::UpdateAnimationValues(float DeltaTime)
{
	// Smooth interpolation for animation values
	MoveForwardValue = FMath::FInterpTo(MoveForwardValue, TargetMoveForward, DeltaTime, AnimationInterpSpeed);
	MoveRightValue = FMath::FInterpTo(MoveRightValue, TargetMoveRight, DeltaTime, AnimationInterpSpeed);

	// Clear targets if no input
	if (!HasMovementInput())
	{
		TargetMoveForward = 0.0f;
		TargetMoveRight = 0.0f;
	}
}

void ASuspenseCoreCharacter::UpdateMovementSpeed()
{
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		float NewSpeed = BaseWalkSpeed;

		if (bIsSprinting)
		{
			NewSpeed *= SprintSpeedMultiplier;
		}
		else if (bIsCrouched)
		{
			NewSpeed *= CrouchSpeedMultiplier;
		}

		CMC->MaxWalkSpeed = NewSpeed;
	}
}

void ASuspenseCoreCharacter::PublishCharacterEvent(const FGameplayTag& EventTag, const FString& Payload)
{
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(const_cast<ASuspenseCoreCharacter*>(this));
		if (!Payload.IsEmpty())
		{
			EventData.SetString(FName("Payload"), Payload);
		}
		EventBus->Publish(EventTag, EventData);
	}
}

USuspenseCoreEventBus* ASuspenseCoreCharacter::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	if (USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this))
	{
		USuspenseCoreEventBus* EventBus = Manager->GetEventBus();
		if (EventBus)
		{
			const_cast<ASuspenseCoreCharacter*>(this)->CachedEventBus = EventBus;
		}
		return EventBus;
	}

	return nullptr;
}
