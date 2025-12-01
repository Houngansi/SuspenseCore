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
#include "Components/SceneCaptureComponent2D.h"
#include "Components/CapsuleComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetRenderingLibrary.h"

// ═══════════════════════════════════════════════════════════════════════════════
// CONSTRUCTOR
// ═══════════════════════════════════════════════════════════════════════════════

ASuspenseCoreCharacter::ASuspenseCoreCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Setup capsule size (matching legacy SuspenseCharacter)
	GetCapsuleComponent()->InitCapsuleSize(34.0f, 96.0f);

	// Configure third person mesh (seen by other players AND by capture camera)
	GetMesh()->SetOwnerNoSee(true);  // Owner doesn't see this mesh in gameplay
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

	// Shadow settings for third person mesh
	GetMesh()->SetCastShadow(true);
	GetMesh()->bCastDynamicShadow = true;
	GetMesh()->bCastStaticShadow = false;
	GetMesh()->bCastHiddenShadow = true;

	// Camera boom (attached to capsule, not mesh)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->TargetArmLength = 0.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = false;

	// Camera
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	CameraComponent->SetupAttachment(CameraBoom);
	CameraComponent->bUsePawnControlRotation = false;

	// First person mesh (arms) - directly attached to the main mesh (matching legacy)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh1P"));
	Mesh1P->SetupAttachment(GetMesh());
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetCollisionProfileName(FName("NoCollision"));
	Mesh1P->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
	Mesh1P->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));

	// ═══════════════════════════════════════════════════════════════════════════════
	// RENDER TARGET SETUP (Character Preview for UI)
	// ═══════════════════════════════════════════════════════════════════════════════

	// Capture camera boom - positioned to view the character from front
	CaptureCameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CaptureCameraBoom"));
	CaptureCameraBoom->SetupAttachment(GetMesh());
	CaptureCameraBoom->TargetArmLength = CaptureDistance;
	CaptureCameraBoom->bUsePawnControlRotation = false;
	CaptureCameraBoom->bDoCollisionTest = false;
	CaptureCameraBoom->bInheritPitch = false;
	CaptureCameraBoom->bInheritYaw = false;
	CaptureCameraBoom->bInheritRoll = false;
	CaptureCameraBoom->SetRelativeLocation(FVector(0.f, 0.f, CaptureHeightOffset));
	CaptureCameraBoom->SetRelativeRotation(FRotator(0.f, 180.f, 0.f)); // Face the character

	// Scene capture component for render target
	CharacterCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("CharacterCapture"));
	CharacterCaptureComponent->SetupAttachment(CaptureCameraBoom, USpringArmComponent::SocketName);
	CharacterCaptureComponent->FOVAngle = CaptureFOV;
	CharacterCaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	CharacterCaptureComponent->bCaptureEveryFrame = false; // Controlled manually for performance
	CharacterCaptureComponent->bCaptureOnMovement = false;
	CharacterCaptureComponent->bAlwaysPersistRenderingState = true;
	CharacterCaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;

	// Configure what the capture camera should see
	// CRITICAL: The capture camera needs to see the third person mesh (GetMesh())
	// but NOT the first person mesh (Mesh1P)
	CharacterCaptureComponent->ShowOnlyActors.Empty();
	CharacterCaptureComponent->HiddenActors.Empty();

	// Disable by default for performance (enabled when needed in UI)
	CharacterCaptureComponent->SetActive(false);

	// Movement settings
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->MaxWalkSpeed = BaseWalkSpeed;
		CMC->bOrientRotationToMovement = false;
		CMC->bUseControllerDesiredRotation = true;
		CMC->NavAgentProps.bCanCrouch = true;
		CMC->bCanWalkOffLedgesWhenCrouching = true;
		CMC->SetCrouchedHalfHeight(40.0f);
		CMC->MaxWalkSpeedCrouched = 150.0f;
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

	// Update capture camera with UPROPERTY values (not available in constructor)
	UpdateCaptureSettings();

	// Initialize render target for character preview
	InitializeRenderTarget();

	// Subscribe to UI capture requests
	SetupCaptureEventSubscription();

	PublishCharacterEvent(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.Spawned")),
		TEXT("{}")
	);
}

void ASuspenseCoreCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TeardownCaptureEventSubscription();

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

// ═══════════════════════════════════════════════════════════════════════════════
// RENDER TARGET (Character Preview for UI)
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCoreCharacter::InitializeRenderTarget()
{
	if (bRenderTargetInitialized)
	{
		return;
	}

	// Create render target texture
	CharacterRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("CharacterRenderTarget"));
	if (CharacterRenderTarget)
	{
		CharacterRenderTarget->InitAutoFormat(RenderTargetWidth, RenderTargetHeight);
		CharacterRenderTarget->ClearColor = FLinearColor::Transparent;
		CharacterRenderTarget->bAutoGenerateMips = false;
		CharacterRenderTarget->UpdateResourceImmediate();

		UE_LOG(LogTemp, Log, TEXT("[SuspenseCoreCharacter] Created render target %dx%d"),
			RenderTargetWidth, RenderTargetHeight);
	}

	// Setup capture component
	SetupCaptureComponent();

	// Create material for UI display
	CreateRenderTargetMaterial();

	bRenderTargetInitialized = true;

	// Publish event for UI systems with render target reference
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
		EventData.SetObject(FName("RenderTarget"), CharacterRenderTarget);
		EventBus->Publish(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.RenderTargetReady")),
			EventData
		);
	}
}

void ASuspenseCoreCharacter::SetupCaptureComponent()
{
	if (!CharacterCaptureComponent || !CharacterRenderTarget)
	{
		return;
	}

	// Assign render target to capture component
	CharacterCaptureComponent->TextureTarget = CharacterRenderTarget;

	// Configure capture settings for character preview
	CharacterCaptureComponent->FOVAngle = CaptureFOV;
	CharacterCaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

	// CRITICAL: Configure visibility
	// The capture camera should ONLY see the third person mesh (GetMesh())
	// It should NOT see the first person arms (Mesh1P)
	CharacterCaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CharacterCaptureComponent->ShowOnlyComponents.Empty();

	// Add the third person skeletal mesh to the show only list
	if (GetMesh())
	{
		CharacterCaptureComponent->ShowOnlyComponents.Add(GetMesh());
	}

	// Post process settings for clean capture
	CharacterCaptureComponent->PostProcessSettings.bOverride_AmbientOcclusionIntensity = true;
	CharacterCaptureComponent->PostProcessSettings.AmbientOcclusionIntensity = 0.0f;
	CharacterCaptureComponent->PostProcessSettings.bOverride_MotionBlurAmount = true;
	CharacterCaptureComponent->PostProcessSettings.MotionBlurAmount = 0.0f;

	// Set capture to on-demand by default (controlled via SetCaptureEnabled)
	CharacterCaptureComponent->bCaptureEveryFrame = bContinuousCapture;
	CharacterCaptureComponent->bCaptureOnMovement = false;

	UE_LOG(LogTemp, Log, TEXT("[SuspenseCoreCharacter] Capture component configured with ShowOnlyComponents"));
}

void ASuspenseCoreCharacter::CreateRenderTargetMaterial()
{
	if (!CharacterRenderTarget)
	{
		return;
	}

	// Create dynamic material instance if base material is set
	if (RenderTargetBaseMaterial)
	{
		RenderTargetMaterialInstance = UMaterialInstanceDynamic::Create(RenderTargetBaseMaterial, this);
		if (RenderTargetMaterialInstance)
		{
			RenderTargetMaterialInstance->SetTextureParameterValue(FName("RenderTargetTexture"), CharacterRenderTarget);
			UE_LOG(LogTemp, Log, TEXT("[SuspenseCoreCharacter] Created render target material instance"));
		}
	}
	else
	{
		// No base material set - UI can use the render target directly
		UE_LOG(LogTemp, Log, TEXT("[SuspenseCoreCharacter] No base material set. UI should create material from render target."));
	}
}

void ASuspenseCoreCharacter::UpdateCaptureSettings()
{
	// Apply UPROPERTY values to capture components (not available in constructor)
	if (CaptureCameraBoom)
	{
		CaptureCameraBoom->TargetArmLength = CaptureDistance;
		CaptureCameraBoom->SetRelativeLocation(FVector(0.f, 0.f, CaptureHeightOffset));
		CaptureCameraBoom->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));

		UE_LOG(LogTemp, Log, TEXT("[SuspenseCoreCharacter] Capture settings updated: Distance=%.1f, Height=%.1f, FOV=%.1f"),
			CaptureDistance, CaptureHeightOffset, CaptureFOV);
	}

	if (CharacterCaptureComponent)
	{
		CharacterCaptureComponent->FOVAngle = CaptureFOV;
	}
}

void ASuspenseCoreCharacter::SetCaptureLocation(const FVector& RelativeLocation, const FRotator& RelativeRotation)
{
	if (CaptureCameraBoom)
	{
		CaptureCameraBoom->SetRelativeLocation(RelativeLocation);
		CaptureCameraBoom->SetRelativeRotation(RelativeRotation);

		// Refresh capture if enabled
		if (bCaptureEnabled)
		{
			RefreshCharacterCapture();
		}
	}
}

void ASuspenseCoreCharacter::RotateCharacterPreview(float DeltaYaw)
{
	CharacterPreviewYaw += DeltaYaw;
	CharacterPreviewYaw = FMath::Fmod(CharacterPreviewYaw, 360.0f);

	// Rotate the capture camera boom around the character
	if (CaptureCameraBoom)
	{
		FRotator NewRotation = CaptureCameraBoom->GetRelativeRotation();
		NewRotation.Yaw = 180.0f + CharacterPreviewYaw; // 180 is default front view
		CaptureCameraBoom->SetRelativeRotation(NewRotation);

		// Refresh capture to show updated rotation
		if (bCaptureEnabled)
		{
			RefreshCharacterCapture();
		}
	}
}

void ASuspenseCoreCharacter::SetCharacterPreviewRotation(float Yaw)
{
	CharacterPreviewYaw = FMath::Fmod(Yaw, 360.0f);

	if (CaptureCameraBoom)
	{
		FRotator NewRotation = CaptureCameraBoom->GetRelativeRotation();
		NewRotation.Yaw = 180.0f + CharacterPreviewYaw;
		CaptureCameraBoom->SetRelativeRotation(NewRotation);

		if (bCaptureEnabled)
		{
			RefreshCharacterCapture();
		}
	}
}

void ASuspenseCoreCharacter::RefreshCharacterCapture()
{
	if (CharacterCaptureComponent && CharacterRenderTarget)
	{
		// Ensure third person mesh is visible to capture (temporarily override owner visibility)
		USkeletalMeshComponent* ThirdPersonMesh = GetMesh();
		if (ThirdPersonMesh)
		{
			// Force capture even if mesh is set to OwnerNoSee
			CharacterCaptureComponent->CaptureScene();
		}
	}
}

void ASuspenseCoreCharacter::SetCaptureEnabled(bool bEnabled)
{
	if (bCaptureEnabled == bEnabled)
	{
		return;
	}

	bCaptureEnabled = bEnabled;

	if (CharacterCaptureComponent)
	{
		CharacterCaptureComponent->SetActive(bEnabled);
		CharacterCaptureComponent->bCaptureEveryFrame = bEnabled && bContinuousCapture;

		if (bEnabled)
		{
			// Initialize if not done yet
			if (!bRenderTargetInitialized)
			{
				InitializeRenderTarget();
			}

			// Immediate capture on enable
			RefreshCharacterCapture();

			UE_LOG(LogTemp, Log, TEXT("[SuspenseCoreCharacter] Character capture enabled"));
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[SuspenseCoreCharacter] Character capture disabled"));
		}
	}

	// Publish state change event
	PublishCharacterEvent(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.CaptureStateChanged")),
		FString::Printf(TEXT("{\"enabled\":%s}"), bEnabled ? TEXT("true") : TEXT("false"))
	);
}

bool ASuspenseCoreCharacter::IsCaptureEnabled() const
{
	return bCaptureEnabled;
}

// ═══════════════════════════════════════════════════════════════════════════════
// EVENTBUS SUBSCRIPTION (UI Capture Requests)
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCoreCharacter::SetupCaptureEventSubscription()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	// Subscribe to capture requests from UI
	CaptureRequestEventHandle = EventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.CharacterPreview.RequestCapture")),
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &ASuspenseCoreCharacter::OnCaptureRequested),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to rotation requests from UI (mouse drag)
	RotationRequestEventHandle = EventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.CharacterPreview.RequestRotation")),
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &ASuspenseCoreCharacter::OnRotationRequested),
		ESuspenseCoreEventPriority::Normal
	);

	UE_LOG(LogTemp, Log, TEXT("[SuspenseCoreCharacter] Subscribed to UI capture/rotation requests"));
}

void ASuspenseCoreCharacter::TeardownCaptureEventSubscription()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (EventBus)
	{
		if (CaptureRequestEventHandle.IsValid())
		{
			EventBus->Unsubscribe(CaptureRequestEventHandle);
		}
		if (RotationRequestEventHandle.IsValid())
		{
			EventBus->Unsubscribe(RotationRequestEventHandle);
		}
	}
}

void ASuspenseCoreCharacter::OnCaptureRequested(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	bool bEnable = EventData.GetBool(FName("Enabled"));
	SetCaptureEnabled(bEnable);

	UE_LOG(LogTemp, Log, TEXT("[SuspenseCoreCharacter] Capture request received: %s"),
		bEnable ? TEXT("Enable") : TEXT("Disable"));
}

void ASuspenseCoreCharacter::OnRotationRequested(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	float DeltaYaw = EventData.GetFloat(FName("DeltaYaw"));
	RotateCharacterPreview(DeltaYaw);
}
