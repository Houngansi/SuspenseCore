// SuspenseCoreCharacterPreviewActor.cpp
// SuspenseCore - Clean Architecture UISystem
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Actors/SuspenseCoreCharacterPreviewActor.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Data/SuspenseCoreCharacterClassData.h"
#include "SuspenseCore/Subsystems/SuspenseCoreCharacterClassSubsystem.h"
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

	// Load and apply skeletal mesh (third person only - no FirstPersonArmsMesh for preview)
	if (USkeletalMesh* Mesh = ClassData->CharacterMesh.LoadSynchronous())
	{
		// Stop any existing animation before changing mesh
		PreviewMesh->Stop();

		// Set animation mode to AnimationBlueprint before setting mesh
		PreviewMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);

		PreviewMesh->SetSkeletalMesh(Mesh);
		UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewActor] Applied mesh: %s"), *Mesh->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterPreviewActor] Failed to load mesh for class: %s"), *ClassData->ClassID.ToString());
	}

	// Load and apply animation blueprint from ClassData (IMPORTANT: this overrides any mesh default)
	TSubclassOf<UAnimInstance> AnimClass = ClassData->AnimationBlueprint.LoadSynchronous();
	if (AnimClass)
	{
		// Force set the anim class - this overrides any default from the mesh
		PreviewMesh->SetAnimInstanceClass(AnimClass);

		// Reinitialize animation to ensure the new AnimBP takes effect immediately
		PreviewMesh->InitAnim(true);

		UE_LOG(LogTemp, Warning, TEXT("[CharacterPreviewActor] Applied AnimBP from ClassData: %s (Class: %s)"),
			*AnimClass->GetName(), *ClassData->ClassID.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterPreviewActor] AnimationBlueprint NOT SET in ClassData for: %s - check your Data Asset!"),
			*ClassData->ClassID.ToString());
	}

	// Play preview idle animation if specified (optional - overrides AnimBP default pose)
	if (UAnimSequence* IdleAnim = ClassData->PreviewIdleAnimation.LoadSynchronous())
	{
		// Switch to animation asset mode to play specific animation
		PreviewMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		PreviewMesh->PlayAnimation(IdleAnim, true);
		UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewActor] Playing preview idle animation: %s"), *IdleAnim->GetName());
	}

	// Notify Blueprint
	OnClassChanged(ClassData);

	UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewActor] Character class set: %s (Mesh: %s, AnimBP: %s)"),
		*ClassData->DisplayName.ToString(),
		ClassData->CharacterMesh.IsNull() ? TEXT("NULL") : *ClassData->CharacterMesh.GetAssetName(),
		ClassData->AnimationBlueprint.IsNull() ? TEXT("NULL") : *ClassData->AnimationBlueprint.GetAssetName());
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
		UE_LOG(LogTemp, Error, TEXT("[CharacterPreviewActor] Cannot setup event subscriptions - EventBus not available! Check EventManager initialization."));
		return;
	}

	// Subscribe to character class changed events
	FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.CharacterClass.Changed"));
	UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewActor] Subscribing to tag: %s"), *EventTag.ToString());

	ClassChangedEventHandle = EventBus->SubscribeNative(
		EventTag,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &ASuspenseCoreCharacterPreviewActor::OnCharacterClassChanged),
		ESuspenseCoreEventPriority::Normal
	);

	if (ClassChangedEventHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterPreviewActor] Successfully subscribed to CharacterClass.Changed events"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterPreviewActor] Failed to subscribe to CharacterClass.Changed events!"));
	}
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
	UE_LOG(LogTemp, Warning, TEXT("[CharacterPreviewActor] >>> OnCharacterClassChanged EVENT RECEIVED! Tag: %s <<<"), *EventTag.ToString());

	// Get class data from event
	USuspenseCoreCharacterClassData* ClassData = EventData.GetObject<USuspenseCoreCharacterClassData>(FName("ClassData"));

	if (ClassData)
	{
		UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewActor] ClassData found in event: %s"), *ClassData->ClassID.ToString());
		SetCharacterClass(ClassData);
	}
	else
	{
		// Try to get class ID and load from subsystem
		FString ClassIdStr = EventData.GetString(FName("ClassId"));
		UE_LOG(LogTemp, Warning, TEXT("[CharacterPreviewActor] ClassData NOT in event, ClassId: %s - trying to load from subsystem"), *ClassIdStr);

		// Try to get class data from CharacterClassSubsystem
		if (!ClassIdStr.IsEmpty())
		{
			if (USuspenseCoreCharacterClassSubsystem* ClassSubsystem = USuspenseCoreCharacterClassSubsystem::Get(this))
			{
				USuspenseCoreCharacterClassData* LoadedClassData = ClassSubsystem->GetClassById(FName(*ClassIdStr));
				if (LoadedClassData)
				{
					UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewActor] Loaded ClassData from subsystem for: %s"), *ClassIdStr);
					SetCharacterClass(LoadedClassData);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[CharacterPreviewActor] Failed to load ClassData for: %s"), *ClassIdStr);
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[CharacterPreviewActor] CharacterClassSubsystem not available!"));
			}
		}
	}
}
