// SuspenseCoreCharacter.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Characters/SuspenseCoreCharacter.h"
#include "SuspenseCore/Core/SuspenseCorePlayerState.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Data/SuspenseCoreCharacterClassData.h"
#include "SuspenseCore/Subsystems/SuspenseCoreCharacterSelectionSubsystem.h"

#if WITH_INTERACTION_SYSTEM
#include "SuspenseCore/Components/SuspenseCoreInteractionComponent.h"
#endif
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "CineCameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"

// ═══════════════════════════════════════════════════════════════════════════════
// CONSTRUCTOR
// ═══════════════════════════════════════════════════════════════════════════════

ASuspenseCoreCharacter::ASuspenseCoreCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Setup capsule size (matching legacy SuspenseCharacter)
	GetCapsuleComponent()->InitCapsuleSize(34.0f, 96.0f);

	// Configure third person mesh
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

	// Shadow settings for third person mesh
	GetMesh()->SetCastShadow(true);
	GetMesh()->bCastDynamicShadow = true;
	GetMesh()->bCastStaticShadow = false;
	GetMesh()->bCastHiddenShadow = true;

	// First person mesh (arms) - directly attached to the main mesh (optional for MetaHuman)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh1P"));
	Mesh1P->SetupAttachment(GetMesh());
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetCollisionProfileName(FName("NoCollision"));
	Mesh1P->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
	Mesh1P->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));

	// Camera boom for optional camera lag/smoothing (attached to capsule)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->TargetArmLength = 0.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = bEnableCameraLag;
	CameraBoom->CameraLagSpeed = CameraLagSpeed;
	CameraBoom->CameraLagMaxDistance = CameraLagMaxDistance;
	CameraBoom->bEnableCameraRotationLag = bEnableCameraRotationLag;
	CameraBoom->CameraRotationLagSpeed = CameraRotationLagSpeed;

	// Create cinematic camera - attached to CameraBoom by default (stable FPS)
	// Attachment can be changed in BeginPlay based on CameraAttachMode
	Camera = CreateDefaultSubobject<UCineCameraComponent>(TEXT("FirstPersonCamera"));
	Camera->SetupAttachment(CameraBoom);
	Camera->bUsePawnControlRotation = false; // CameraBoom handles rotation

	// Configure cinematic camera settings
	Camera->SetFieldOfView(CinematicFieldOfView);
	Camera->SetCurrentFocalLength(CurrentFocalLength);
	Camera->SetCurrentAperture(CurrentAperture);

	// Setup lens settings
	Camera->LensSettings.MaxFocalLength = 1000.0f;
	Camera->LensSettings.MinFocalLength = 4.0f;
	Camera->LensSettings.MaxFStop = 32.0f;
	Camera->LensSettings.MinFStop = 0.7f;
	Camera->LensSettings.DiaphragmBladeCount = DiaphragmBladeCount;

	// Configure depth of field
	Camera->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
	Camera->FocusSettings.ManualFocusDistance = ManualFocusDistance;
	Camera->FocusSettings.bDrawDebugFocusPlane = false;
	Camera->FocusSettings.bSmoothFocusChanges = bSmoothFocusChanges;
	Camera->FocusSettings.FocusSmoothingInterpSpeed = FocusSmoothingSpeed;

	// Filmback settings for sensor size (affects FOV and DOF)
	Camera->Filmback.SensorWidth = SensorWidth;
	Camera->Filmback.SensorHeight = SensorHeight;
	Camera->Filmback.SensorAspectRatio = SensorWidth / SensorHeight;

	// Post process settings for FPS games
	Camera->PostProcessSettings.bOverride_MotionBlurAmount = true;
	Camera->PostProcessSettings.MotionBlurAmount = 0.1f;
	Camera->PostProcessSettings.bOverride_SceneFringeIntensity = true;
	Camera->PostProcessSettings.SceneFringeIntensity = 0.0f;

	// Initialize focus distance
	Camera->CurrentFocusDistance = ManualFocusDistance;

#if WITH_INTERACTION_SYSTEM
	// Create interaction component for world object interaction
	InteractionComponent = CreateDefaultSubobject<USuspenseCoreInteractionComponent>(TEXT("InteractionComponent"));
#endif

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

	// Setup camera attachment based on mode (MetaHuman support)
	SetupCameraAttachment();

	// Setup camera settings
	SetupCameraSettings();

	// Load character class from subsystem (selected in menu)
	LoadCharacterClassFromSubsystem();

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
// PUBLIC API - CHARACTER CLASS
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCoreCharacter::ApplyCharacterClass(USuspenseCoreCharacterClassData* ClassData)
{
	if (!ClassData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SuspenseCoreCharacter] ApplyCharacterClass: ClassData is null"));
		return;
	}

	AppliedClassData = ClassData;

	// NOTE: Visual configuration (mesh, animations) is now handled in the Character Blueprint itself.
	// Each class has its own BP_Character_[ClassName] with pre-configured visuals.
	// This function only applies gameplay-related class data (attributes, abilities via events).

	// Publish event for other systems (GAS, UI, etc.)
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
		EventData.SetString(FName("ClassId"), ClassData->ClassID.ToString());
		EventData.SetObject(FName("ClassData"), ClassData);

		EventBus->Publish(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.CharacterClass.Applied")),
			EventData
		);
	}

	UE_LOG(LogTemp, Log, TEXT("[SuspenseCoreCharacter] Character class applied: %s"), *ClassData->DisplayName.ToString());
}

void ASuspenseCoreCharacter::LoadCharacterClassFromSubsystem()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	USuspenseCoreCharacterSelectionSubsystem* SelectionSubsystem = GI->GetSubsystem<USuspenseCoreCharacterSelectionSubsystem>();
	if (!SelectionSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SuspenseCoreCharacter] CharacterSelectionSubsystem not found"));
		return;
	}

	// GetSelectedClass returns UObject* (to avoid GAS dependency in BridgeSystem), cast to concrete type
	UObject* ClassDataObj = SelectionSubsystem->GetSelectedClass();
	USuspenseCoreCharacterClassData* ClassData = Cast<USuspenseCoreCharacterClassData>(ClassDataObj);

	if (ClassData)
	{
		ApplyCharacterClass(ClassData);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[SuspenseCoreCharacter] No character class selected in subsystem"));
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

// ═══════════════════════════════════════════════════════════════════════════════
// ISUSPENSECOREEVENTEMITTER INTERFACE
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCoreCharacter::EmitEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& Data)
{
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		EventBus->Publish(EventTag, Data);
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

// ═══════════════════════════════════════════════════════════════════════════════
// CAMERA ATTACHMENT
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCoreCharacter::SetupCameraAttachment()
{
	if (!Camera)
	{
		return;
	}

	USceneComponent* AttachComponent = nullptr;
	FName SocketToUse = CameraAttachSocketName;

	switch (CameraAttachMode)
	{
	case ESuspenseCoreCameraAttachMode::CameraBoom:
		// Attach to CameraBoom - stable FPS, no head bob
		Camera->AttachToComponent(CameraBoom, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Camera->SetRelativeLocation(FVector::ZeroVector);
		Camera->SetRelativeRotation(FRotator::ZeroRotator);
		Camera->bUsePawnControlRotation = false;
		UE_LOG(LogTemp, Log, TEXT("[SuspenseCoreCharacter] Camera attached to CameraBoom (stable FPS mode)"));
		return;

	case ESuspenseCoreCameraAttachMode::MetaHumanFace:
		AttachComponent = FindMetaHumanFaceComponent();
		if (!AttachComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("[SuspenseCoreCharacter] MetaHuman Face component not found!"));
		}
		break;

	case ESuspenseCoreCameraAttachMode::MetaHumanBody:
		AttachComponent = FindMetaHumanBodyComponent();
		if (!AttachComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("[SuspenseCoreCharacter] MetaHuman Body SkeletalMesh not found!"));
		}
		break;

	case ESuspenseCoreCameraAttachMode::ComponentByName:
		AttachComponent = FindComponentByName(CameraAttachComponentName);
		if (!AttachComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("[SuspenseCoreCharacter] Component with name '%s' not found!"),
				*CameraAttachComponentName.ToString());
		}
		break;

	case ESuspenseCoreCameraAttachMode::ComponentByTag:
		AttachComponent = FindComponentByTag(CameraAttachComponentTag);
		if (!AttachComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("[SuspenseCoreCharacter] Component with tag '%s' not found!"),
				*CameraAttachComponentTag.ToString());
		}
		break;

	case ESuspenseCoreCameraAttachMode::Mesh1P:
		AttachComponent = Mesh1P;
		break;
	}

	// Attach camera to found component
	if (AttachComponent)
	{
		Camera->AttachToComponent(AttachComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketToUse);
		Camera->SetRelativeLocation(CameraAttachOffset);
		Camera->SetRelativeRotation(CameraAttachRotation);
		Camera->bUsePawnControlRotation = true;

		UE_LOG(LogTemp, Log, TEXT("[SuspenseCoreCharacter] Camera attached to '%s' socket '%s'"),
			*AttachComponent->GetName(), *SocketToUse.ToString());
	}
	else
	{
		// Fallback to CameraBoom
		UE_LOG(LogTemp, Warning, TEXT("[SuspenseCoreCharacter] Falling back to CameraBoom attachment"));
		Camera->AttachToComponent(CameraBoom, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Camera->SetRelativeLocation(FVector::ZeroVector);
		Camera->SetRelativeRotation(FRotator::ZeroRotator);
		Camera->bUsePawnControlRotation = false;
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// EQUIPMENT ATTACHMENT
// ═══════════════════════════════════════════════════════════════════════════════

USkeletalMeshComponent* ASuspenseCoreCharacter::GetEquipmentAttachMesh_Implementation() const
{
	// For MetaHuman: search for Body SkeletalMesh component
	// This is the component that has the equipment sockets (weapon_r, spine_03, etc.)

	TArray<UActorComponent*> Components;
	GetComponents(USkeletalMeshComponent::StaticClass(), Components);

	// First: Look for SkeletalMesh under "Body" parent (MetaHuman structure)
	for (UActorComponent* Component : Components)
	{
		if (USkeletalMeshComponent* SkelMesh = Cast<USkeletalMeshComponent>(Component))
		{
			USceneComponent* Parent = SkelMesh->GetAttachParent();

			// Check if parent is "Body" (MetaHuman hierarchy)
			if (Parent && Parent->GetName().Contains(TEXT("Body")))
			{
				UE_LOG(LogTemp, Log, TEXT("[SuspenseCoreCharacter] GetEquipmentAttachMesh: Found MetaHuman Body mesh: %s"),
					*SkelMesh->GetName());
				return SkelMesh;
			}
		}
	}

	// Second: Look for any SkeletalMesh with equipment sockets
	for (UActorComponent* Component : Components)
	{
		if (USkeletalMeshComponent* SkelMesh = Cast<USkeletalMeshComponent>(Component))
		{
			// Check for common equipment socket names
			if (SkelMesh->DoesSocketExist(FName("weapon_r")) ||
				SkelMesh->DoesSocketExist(FName("hand_r")) ||
				SkelMesh->DoesSocketExist(FName("RightHand")) ||
				SkelMesh->DoesSocketExist(FName("spine_03")))
			{
				UE_LOG(LogTemp, Log, TEXT("[SuspenseCoreCharacter] GetEquipmentAttachMesh: Found mesh with equipment sockets: %s"),
					*SkelMesh->GetName());
				return SkelMesh;
			}
		}
	}

	// Fallback: Return standard character mesh (legacy support)
	UE_LOG(LogTemp, Log, TEXT("[SuspenseCoreCharacter] GetEquipmentAttachMesh: Using default GetMesh()"));
	return GetMesh();
}

USceneComponent* ASuspenseCoreCharacter::FindMetaHumanFaceComponent() const
{
	// MetaHuman Face component is named "Face" and is a SkeletalMeshComponent
	// Hierarchy: Root > Body > Face
	TArray<UActorComponent*> Components;
	GetComponents(USkeletalMeshComponent::StaticClass(), Components);

	for (UActorComponent* Component : Components)
	{
		FString CompName = Component->GetName();
		// Check for "Face" in component name
		if (CompName.Contains(TEXT("Face")))
		{
			UE_LOG(LogTemp, Log, TEXT("[SuspenseCoreCharacter] Found MetaHuman Face: %s"), *CompName);
			return Cast<USceneComponent>(Component);
		}
	}

	// Also search in hierarchy with partial name match
	for (UActorComponent* Component : Components)
	{
		if (USceneComponent* SceneComp = Cast<USceneComponent>(Component))
		{
			// Check parent hierarchy for "Face"
			USceneComponent* Parent = SceneComp->GetAttachParent();
			while (Parent)
			{
				if (Parent->GetName().Contains(TEXT("Face")))
				{
					return SceneComp;
				}
				Parent = Parent->GetAttachParent();
			}
		}
	}

	return nullptr;
}

USceneComponent* ASuspenseCoreCharacter::FindMetaHumanBodyComponent() const
{
	// MetaHuman Body has a SkeletalMesh child
	// Hierarchy: Root > Body > SkeletalMesh (this has the skeleton with "head" bone)
	TArray<UActorComponent*> Components;
	GetComponents(USkeletalMeshComponent::StaticClass(), Components);

	// First look for "SkeletalMesh" under "Body"
	for (UActorComponent* Component : Components)
	{
		if (USceneComponent* SceneComp = Cast<USceneComponent>(Component))
		{
			FString CompName = Component->GetName();
			USceneComponent* Parent = SceneComp->GetAttachParent();

			// Check if parent is "Body"
			if (Parent && Parent->GetName().Contains(TEXT("Body")))
			{
				UE_LOG(LogTemp, Log, TEXT("[SuspenseCoreCharacter] Found MetaHuman Body SkeletalMesh: %s (parent: %s)"),
					*CompName, *Parent->GetName());
				return SceneComp;
			}
		}
	}

	// Fallback: look for any SkeletalMesh with "head" bone
	for (UActorComponent* Component : Components)
	{
		if (USkeletalMeshComponent* SkelMesh = Cast<USkeletalMeshComponent>(Component))
		{
			if (SkelMesh->DoesSocketExist(FName("head")))
			{
				UE_LOG(LogTemp, Log, TEXT("[SuspenseCoreCharacter] Found SkeletalMesh with 'head' socket: %s"),
					*Component->GetName());
				return SkelMesh;
			}
		}
	}

	return nullptr;
}

USceneComponent* ASuspenseCoreCharacter::FindComponentByName(FName ComponentName) const
{
	TArray<UActorComponent*> Components;
	GetComponents(Components);

	FString SearchName = ComponentName.ToString();

	for (UActorComponent* Component : Components)
	{
		if (USceneComponent* SceneComp = Cast<USceneComponent>(Component))
		{
			if (SceneComp->GetName().Contains(SearchName))
			{
				UE_LOG(LogTemp, Log, TEXT("[SuspenseCoreCharacter] Found component by name: %s"), *SceneComp->GetName());
				return SceneComp;
			}
		}
	}

	return nullptr;
}

USceneComponent* ASuspenseCoreCharacter::FindComponentByTag(FName Tag) const
{
	TArray<UActorComponent*> Components;
	GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (USceneComponent* SceneComp = Cast<USceneComponent>(Component))
		{
			if (SceneComp->ComponentHasTag(Tag))
			{
				return SceneComp;
			}
		}
	}

	return nullptr;
}

void ASuspenseCoreCharacter::SetupCameraSettings()
{
	ApplyCameraLagSettings();
	ApplyCinematicCameraSettings();
}

void ASuspenseCoreCharacter::ApplyCameraLagSettings()
{
	if (CameraBoom)
	{
		CameraBoom->bEnableCameraLag = bEnableCameraLag;
		CameraBoom->CameraLagSpeed = CameraLagSpeed;
		CameraBoom->CameraLagMaxDistance = CameraLagMaxDistance;
		CameraBoom->bEnableCameraRotationLag = bEnableCameraRotationLag;
		CameraBoom->CameraRotationLagSpeed = CameraRotationLagSpeed;
	}
}

void ASuspenseCoreCharacter::ApplyCinematicCameraSettings()
{
	if (Camera)
	{
		// Apply DOF settings via post process
		Camera->PostProcessSettings.bOverride_DepthOfFieldFstop = bEnableDepthOfField;
		Camera->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = bEnableDepthOfField;
		Camera->PostProcessSettings.bOverride_DepthOfFieldDepthBlurAmount = bEnableDepthOfField;
		Camera->PostProcessSettings.bOverride_DepthOfFieldDepthBlurRadius = bEnableDepthOfField;

		if (bEnableDepthOfField)
		{
			Camera->PostProcessSettings.DepthOfFieldFstop = CurrentAperture;
			Camera->PostProcessSettings.DepthOfFieldFocalDistance = ManualFocusDistance;
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// CINEMATIC CAMERA CONTROL
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCoreCharacter::SetCameraFOV(float NewFOV)
{
	if (Camera)
	{
		float ClampedFOV = FMath::Clamp(NewFOV, 5.0f, 170.0f);
		Camera->SetFieldOfView(ClampedFOV);

		PublishCameraEvent(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Camera.FOVChanged")),
			ClampedFOV
		);
	}
}

void ASuspenseCoreCharacter::SetCameraFocalLength(float NewFocalLength)
{
	if (Camera)
	{
		float ClampedFocalLength = FMath::Clamp(NewFocalLength,
			Camera->LensSettings.MinFocalLength,
			Camera->LensSettings.MaxFocalLength);

		Camera->SetCurrentFocalLength(ClampedFocalLength);
		CurrentFocalLength = ClampedFocalLength;

		PublishCameraEvent(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Camera.FocalLengthChanged")),
			ClampedFocalLength
		);
	}
}

void ASuspenseCoreCharacter::SetCameraAperture(float NewAperture)
{
	if (Camera)
	{
		float ClampedAperture = FMath::Clamp(NewAperture,
			Camera->LensSettings.MinFStop,
			Camera->LensSettings.MaxFStop);

		Camera->SetCurrentAperture(ClampedAperture);

		if (bEnableDepthOfField)
		{
			Camera->PostProcessSettings.DepthOfFieldFstop = ClampedAperture;
		}

		CurrentAperture = ClampedAperture;

		PublishCameraEvent(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Camera.ApertureChanged")),
			ClampedAperture
		);
	}
}

void ASuspenseCoreCharacter::SetDepthOfFieldEnabled(bool bEnabled)
{
	if (Camera)
	{
		bEnableDepthOfField = bEnabled;

		Camera->PostProcessSettings.bOverride_DepthOfFieldFstop = bEnabled;
		Camera->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = bEnabled;
		Camera->PostProcessSettings.bOverride_DepthOfFieldDepthBlurAmount = bEnabled;
		Camera->PostProcessSettings.bOverride_DepthOfFieldDepthBlurRadius = bEnabled;

		if (bEnabled)
		{
			Camera->PostProcessSettings.DepthOfFieldFstop = CurrentAperture;
			Camera->PostProcessSettings.DepthOfFieldFocalDistance = ManualFocusDistance;
		}

		PublishCameraEvent(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Camera.DOFChanged")),
			bEnabled ? 1.0f : 0.0f
		);
	}
}

void ASuspenseCoreCharacter::SetCameraFocusDistance(float Distance)
{
	if (Camera)
	{
		ManualFocusDistance = Distance;
		Camera->FocusSettings.ManualFocusDistance = Distance;
		Camera->PostProcessSettings.DepthOfFieldFocalDistance = Distance;

		PublishCameraEvent(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Camera.FocusDistanceChanged")),
			Distance
		);
	}
}

void ASuspenseCoreCharacter::ApplyCinematicPreset(bool bEnableDOF, float Aperture, float FocusDistance)
{
	SetDepthOfFieldEnabled(bEnableDOF);
	SetCameraAperture(Aperture);
	SetCameraFocusDistance(FocusDistance);

	if (Camera && bEnableDOF)
	{
		// Enhanced cinematic settings
		Camera->PostProcessSettings.bOverride_MotionBlurAmount = true;
		Camera->PostProcessSettings.MotionBlurAmount = 0.5f;

		Camera->PostProcessSettings.bOverride_VignetteIntensity = true;
		Camera->PostProcessSettings.VignetteIntensity = 0.4f;
	}

	PublishCharacterEvent(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Camera.PresetApplied")),
		FString::Printf(TEXT("{\"dof\":%s,\"aperture\":%.2f,\"focus\":%.2f}"),
			bEnableDOF ? TEXT("true") : TEXT("false"), Aperture, FocusDistance)
	);
}

void ASuspenseCoreCharacter::ResetCameraToDefaults()
{
	if (Camera)
	{
		// Reset FOV
		Camera->SetFieldOfView(CinematicFieldOfView);

		// Reset focal length and aperture
		Camera->SetCurrentFocalLength(35.0f);
		Camera->SetCurrentAperture(2.8f);
		CurrentFocalLength = 35.0f;
		CurrentAperture = 2.8f;

		// Reset focus settings
		ManualFocusDistance = 1000.0f;
		Camera->FocusSettings.ManualFocusDistance = ManualFocusDistance;

		// Disable DOF
		SetDepthOfFieldEnabled(false);

		// Reset post process
		Camera->PostProcessSettings.bOverride_MotionBlurAmount = true;
		Camera->PostProcessSettings.MotionBlurAmount = 0.1f;
		Camera->PostProcessSettings.bOverride_VignetteIntensity = false;
		Camera->PostProcessSettings.bOverride_SceneFringeIntensity = true;
		Camera->PostProcessSettings.SceneFringeIntensity = 0.0f;
	}

	PublishCharacterEvent(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Camera.Reset")),
		TEXT("{}")
	);
}

void ASuspenseCoreCharacter::PublishCameraEvent(const FGameplayTag& EventTag, float Value)
{
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(const_cast<ASuspenseCoreCharacter*>(this));
		EventData.SetFloat(FName("Value"), Value);
		EventBus->Publish(EventTag, EventData);
	}
}
