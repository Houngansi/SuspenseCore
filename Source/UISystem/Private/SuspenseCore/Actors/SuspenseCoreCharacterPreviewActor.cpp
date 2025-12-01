// SuspenseCoreCharacterPreviewActor.cpp
// Separate actor for character preview - solves OwnerNoSee issues in UE5.7+
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Actors/SuspenseCoreCharacterPreviewActor.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Character.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Animation/AnimInstance.h"

ASuspenseCoreCharacterPreviewActor::ASuspenseCoreCharacterPreviewActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // Enable only when needed

	// Root component
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Preview mesh - this is what we're capturing
	PreviewMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewMesh"));
	PreviewMesh->SetupAttachment(Root);
	PreviewMesh->SetRelativeLocation(FVector::ZeroVector);
	PreviewMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f)); // Face camera
	PreviewMesh->SetCastShadow(false);
	PreviewMesh->bCastDynamicShadow = false;

	// Camera boom for positioning capture camera
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Root);
	CameraBoom->TargetArmLength = CameraDistance;
	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, CameraHeightOffset));
	CameraBoom->SetRelativeRotation(FRotator(0.f, 0.f, 0.f)); // Look at mesh
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->bInheritRoll = false;

	// Scene capture component - captures the mesh to render target
	CaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("CaptureComponent"));
	CaptureComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	CaptureComponent->FOVAngle = CameraFOV;
	CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	CaptureComponent->bCaptureEveryFrame = true; // For animation
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->bAlwaysPersistRenderingState = true;
	CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;

	// Lighting for the preview scene
	PreviewLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PreviewLight"));
	PreviewLight->SetupAttachment(Root);
	PreviewLight->SetRelativeLocation(FVector(200.f, 200.f, 300.f));
	PreviewLight->Intensity = LightIntensity;
	PreviewLight->SetCastShadows(false);
}

void ASuspenseCoreCharacterPreviewActor::BeginPlay()
{
	Super::BeginPlay();

	// Update boom settings with UPROPERTY values
	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = CameraDistance;
		CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, CameraHeightOffset));
	}

	if (CaptureComponent)
	{
		CaptureComponent->FOVAngle = CameraFOV;
	}

	if (PreviewLight)
	{
		PreviewLight->SetIntensity(LightIntensity);
	}

	// Create render target
	CreateRenderTarget();

	// Setup capture component
	SetupCaptureComponent();

	// Subscribe to events
	SetupEventSubscriptions();

	// Publish that render target is ready
	PublishRenderTargetReady();

	UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewActor] Spawned at %s"), *GetActorLocation().ToString());
}

void ASuspenseCoreCharacterPreviewActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TeardownEventSubscriptions();

	Super::EndPlay(EndPlayReason);
}

void ASuspenseCoreCharacterPreviewActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCoreCharacterPreviewActor::SetPreviewMesh(USkeletalMesh* Mesh, TSubclassOf<UAnimInstance> AnimInstance)
{
	if (!PreviewMesh || !Mesh)
	{
		return;
	}

	PreviewMesh->SetSkeletalMesh(Mesh);

	if (AnimInstance)
	{
		PreviewMesh->SetAnimInstanceClass(AnimInstance);
	}

	// Update show only list
	CaptureComponent->ShowOnlyComponents.Empty();
	CaptureComponent->ShowOnlyComponents.Add(PreviewMesh);

	UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewActor] Set preview mesh: %s"), *Mesh->GetName());
}

void ASuspenseCoreCharacterPreviewActor::CopyFromCharacter(ACharacter* SourceCharacter)
{
	if (!SourceCharacter || !PreviewMesh)
	{
		return;
	}

	USkeletalMeshComponent* SourceMesh = SourceCharacter->GetMesh();
	if (!SourceMesh)
	{
		return;
	}

	// Copy skeletal mesh
	if (USkeletalMesh* Mesh = SourceMesh->GetSkeletalMeshAsset())
	{
		PreviewMesh->SetSkeletalMesh(Mesh);
	}

	// Copy animation blueprint
	if (UClass* AnimClass = SourceMesh->GetAnimClass())
	{
		PreviewMesh->SetAnimInstanceClass(AnimClass);
	}

	// Update show only list
	CaptureComponent->ShowOnlyComponents.Empty();
	CaptureComponent->ShowOnlyComponents.Add(PreviewMesh);

	UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewActor] Copied mesh from character: %s"), *SourceCharacter->GetName());
}

void ASuspenseCoreCharacterPreviewActor::RotatePreview(float DeltaYaw)
{
	CurrentYaw += DeltaYaw;
	CurrentYaw = FMath::Fmod(CurrentYaw, 360.0f);

	if (PreviewMesh)
	{
		FRotator NewRotation = PreviewMesh->GetRelativeRotation();
		NewRotation.Yaw = -90.0f + CurrentYaw; // -90 is default facing camera
		PreviewMesh->SetRelativeRotation(NewRotation);
	}
}

void ASuspenseCoreCharacterPreviewActor::SetPreviewRotation(float Yaw)
{
	CurrentYaw = FMath::Fmod(Yaw, 360.0f);

	if (PreviewMesh)
	{
		FRotator NewRotation = PreviewMesh->GetRelativeRotation();
		NewRotation.Yaw = -90.0f + CurrentYaw;
		PreviewMesh->SetRelativeRotation(NewRotation);
	}
}

void ASuspenseCoreCharacterPreviewActor::PlayAnimation(UAnimationAsset* Animation, bool bLooping)
{
	if (PreviewMesh && Animation)
	{
		PreviewMesh->PlayAnimation(Animation, bLooping);
	}
}

void ASuspenseCoreCharacterPreviewActor::RefreshCapture()
{
	if (CaptureComponent && RenderTarget)
	{
		CaptureComponent->CaptureScene();
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCoreCharacterPreviewActor::CreateRenderTarget()
{
	RenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("PreviewRenderTarget"));
	if (RenderTarget)
	{
		RenderTarget->InitAutoFormat(RenderTargetWidth, RenderTargetHeight);
		RenderTarget->ClearColor = BackgroundColor;
		RenderTarget->bAutoGenerateMips = false;
		RenderTarget->UpdateResourceImmediate();

		UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewActor] Created render target %dx%d"),
			RenderTargetWidth, RenderTargetHeight);
	}
}

void ASuspenseCoreCharacterPreviewActor::SetupCaptureComponent()
{
	if (!CaptureComponent || !RenderTarget)
	{
		return;
	}

	CaptureComponent->TextureTarget = RenderTarget;

	// Only capture the preview mesh (not the whole scene)
	CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureComponent->ShowOnlyComponents.Empty();
	CaptureComponent->ShowOnlyComponents.Add(PreviewMesh);

	// Disable post process effects for clean capture
	CaptureComponent->PostProcessSettings.bOverride_AmbientOcclusionIntensity = true;
	CaptureComponent->PostProcessSettings.AmbientOcclusionIntensity = 0.0f;
	CaptureComponent->PostProcessSettings.bOverride_MotionBlurAmount = true;
	CaptureComponent->PostProcessSettings.MotionBlurAmount = 0.0f;
	CaptureComponent->PostProcessSettings.bOverride_BloomIntensity = true;
	CaptureComponent->PostProcessSettings.BloomIntensity = 0.0f;

	UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewActor] Capture component configured"));
}

void ASuspenseCoreCharacterPreviewActor::SetupEventSubscriptions()
{
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (!Manager)
	{
		return;
	}

	CachedEventBus = Manager->GetEventBus();
	if (!CachedEventBus.IsValid())
	{
		return;
	}

	// Subscribe to rotation requests
	RotationEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.CharacterPreview.RequestRotation")),
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &ASuspenseCoreCharacterPreviewActor::OnRotationRequested),
		ESuspenseCoreEventPriority::Normal
	);

	UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewActor] EventBus subscriptions established"));
}

void ASuspenseCoreCharacterPreviewActor::TeardownEventSubscriptions()
{
	if (CachedEventBus.IsValid() && RotationEventHandle.IsValid())
	{
		CachedEventBus->Unsubscribe(RotationEventHandle);
	}
}

void ASuspenseCoreCharacterPreviewActor::PublishRenderTargetReady()
{
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (Manager && Manager->GetEventBus() && RenderTarget)
	{
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
		EventData.SetObject(FName("RenderTarget"), RenderTarget);
		Manager->GetEventBus()->Publish(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.RenderTargetReady")),
			EventData
		);

		UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewActor] Published RenderTargetReady event"));
	}
}

void ASuspenseCoreCharacterPreviewActor::OnRotationRequested(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	float DeltaYaw = EventData.GetFloat(FName("DeltaYaw"));
	RotatePreview(DeltaYaw);
}
