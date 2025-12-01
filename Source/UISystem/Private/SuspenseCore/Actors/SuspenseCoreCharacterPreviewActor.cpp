// SuspenseCoreCharacterPreviewActor.cpp
// SuspenseCore - Clean Architecture UISystem
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Actors/SuspenseCoreCharacterPreviewActor.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Data/SuspenseCoreCharacterClassData.h"
#include "Components/SkeletalMeshComponent.h"

// ═══════════════════════════════════════════════════════════════════════════════
// CONSTRUCTOR
// ═══════════════════════════════════════════════════════════════════════════════

ASuspenseCoreCharacterPreviewActor::ASuspenseCoreCharacterPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create root scene component
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Create skeletal mesh component for character preview
	PreviewMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewMesh"));
	PreviewMesh->SetupAttachment(Root);
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->bCastDynamicShadow = true;
	PreviewMesh->CastShadow = true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// AACTOR INTERFACE
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCoreCharacterPreviewActor::BeginPlay()
{
	Super::BeginPlay();

	// Subscribe to EventBus events
	if (bAutoSubscribeToEvents)
	{
		SetupEventSubscriptions();
	}

	// Apply default class if set
	if (DefaultClassData)
	{
		SetCharacterClass(DefaultClassData);
	}

	UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewActor] BeginPlay - Ready for character preview"));
}

void ASuspenseCoreCharacterPreviewActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TeardownEventSubscriptions();

	Super::EndPlay(EndPlayReason);
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCoreCharacterPreviewActor::SetCharacterClass(USuspenseCoreCharacterClassData* ClassData)
{
	if (!ClassData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterPreviewActor] SetCharacterClass: ClassData is null"));
		return;
	}

	CurrentClassData = ClassData;

	// Load and apply skeletal mesh
	if (USkeletalMesh* Mesh = ClassData->CharacterMesh.LoadSynchronous())
	{
		PreviewMesh->SetSkeletalMesh(Mesh);
		UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewActor] Applied mesh: %s"), *Mesh->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterPreviewActor] Failed to load mesh for class: %s"), *ClassData->ClassID.ToString());
	}

	// Load and apply animation blueprint
	if (TSubclassOf<UAnimInstance> AnimClass = ClassData->AnimationBlueprint.LoadSynchronous())
	{
		PreviewMesh->SetAnimInstanceClass(AnimClass);
		UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewActor] Applied AnimBP: %s"), *AnimClass->GetName());
	}

	// Play idle animation if specified
	if (UAnimSequence* IdleAnim = ClassData->PreviewIdleAnimation.LoadSynchronous())
	{
		if (PreviewMesh->GetAnimInstance())
		{
			// Note: PlayAnimation requires single-mode, better to use AnimBP
			UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewActor] Idle animation available: %s"), *IdleAnim->GetName());
		}
	}

	// Notify Blueprint
	OnClassChanged(ClassData);

	UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewActor] Character class set: %s"), *ClassData->DisplayName.ToString());
}

void ASuspenseCoreCharacterPreviewActor::RotatePreview(float DeltaYaw)
{
	CurrentYaw += DeltaYaw * RotationSpeed;
	CurrentYaw = FMath::Fmod(CurrentYaw, 360.0f);

	if (PreviewMesh)
	{
		FRotator NewRotation = PreviewMesh->GetRelativeRotation();
		NewRotation.Yaw = CurrentYaw;
		PreviewMesh->SetRelativeRotation(NewRotation);
	}
}

void ASuspenseCoreCharacterPreviewActor::SetPreviewRotation(float Yaw)
{
	CurrentYaw = FMath::Fmod(Yaw, 360.0f);

	if (PreviewMesh)
	{
		FRotator NewRotation = PreviewMesh->GetRelativeRotation();
		NewRotation.Yaw = CurrentYaw;
		PreviewMesh->SetRelativeRotation(NewRotation);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL METHODS
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCoreCharacterPreviewActor::SetupEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterPreviewActor] Cannot setup event subscriptions - EventBus not available"));
		return;
	}

	// Subscribe to character class changed events
	ClassChangedEventHandle = EventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.CharacterClass.Changed")),
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &ASuspenseCoreCharacterPreviewActor::OnCharacterClassChanged),
		ESuspenseCoreEventPriority::Normal
	);

	UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewActor] Subscribed to CharacterClass.Changed events"));
}

void ASuspenseCoreCharacterPreviewActor::TeardownEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (EventBus && ClassChangedEventHandle.IsValid())
	{
		EventBus->Unsubscribe(ClassChangedEventHandle);
	}
}

USuspenseCoreEventBus* ASuspenseCoreCharacterPreviewActor::GetEventBus()
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
			CachedEventBus = EventBus;
		}
		return EventBus;
	}

	return nullptr;
}

void ASuspenseCoreCharacterPreviewActor::OnCharacterClassChanged(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Get class data from event
	USuspenseCoreCharacterClassData* ClassData = EventData.GetObject<USuspenseCoreCharacterClassData>(FName("ClassData"));

	if (ClassData)
	{
		SetCharacterClass(ClassData);
	}
	else
	{
		// Try to get class ID and load from subsystem
		FString ClassIdStr = EventData.GetString(FName("ClassId"));
		UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewActor] Received CharacterClass.Changed event for: %s (ClassData not in event)"), *ClassIdStr);
	}
}
