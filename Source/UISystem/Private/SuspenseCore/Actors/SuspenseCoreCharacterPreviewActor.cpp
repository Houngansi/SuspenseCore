// SuspenseCoreCharacterPreviewActor.cpp
// SuspenseCore - Clean Architecture UISystem
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Actors/SuspenseCoreCharacterPreviewActor.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Data/SuspenseCoreCharacterClassData.h"
#include "SuspenseCore/Subsystems/SuspenseCoreCharacterClassSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCorePreview, Log, All);

// Static map to track active preview actors by ID (prevents duplicates)
static TMap<FName, TWeakObjectPtr<ASuspenseCoreCharacterPreviewActor>> ActivePreviewActors;

// ═══════════════════════════════════════════════════════════════════════════════
// CONSTRUCTOR
// ═══════════════════════════════════════════════════════════════════════════════

ASuspenseCoreCharacterPreviewActor::ASuspenseCoreCharacterPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create root scene component for positioning
	PreviewRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PreviewRoot"));
	SetRootComponent(PreviewRoot);

	SpawnedPreviewActor = nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════════
// AACTOR INTERFACE
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCoreCharacterPreviewActor::BeginPlay()
{
	Super::BeginPlay();

	// Check for duplicate preview actors with same ID
	if (!PreviewActorId.IsNone())
	{
		if (TWeakObjectPtr<ASuspenseCoreCharacterPreviewActor>* ExistingActor = ActivePreviewActors.Find(PreviewActorId))
		{
			if (ExistingActor->IsValid() && ExistingActor->Get() != this)
			{
				UE_LOG(LogSuspenseCorePreview, Warning,
					TEXT("[CharacterPreviewActor] Duplicate preview actor with ID '%s' detected! Disabling this instance: %s"),
					*PreviewActorId.ToString(), *GetName());
				// Don't setup subscriptions or spawn for this duplicate
				return;
			}
		}

		// Register this actor as active
		ActivePreviewActors.Add(PreviewActorId, this);
		UE_LOG(LogSuspenseCorePreview, Log,
			TEXT("[CharacterPreviewActor] Registered preview actor with ID '%s': %s"),
			*PreviewActorId.ToString(), *GetName());
	}

	// Subscribe to EventBus events
	if (bAutoSubscribeToEvents)
	{
		SetupEventSubscriptions();
	}

	// Apply default class if set (only if we spawn actors)
	if (bSpawnsPreviewActor && DefaultClassData)
	{
		SetCharacterClass(DefaultClassData);
	}

	UE_LOG(LogSuspenseCorePreview, Log, TEXT("[CharacterPreviewActor] BeginPlay - Ready for character preview (SpawnsActor: %s)"),
		bSpawnsPreviewActor ? TEXT("true") : TEXT("false"));
}

void ASuspenseCoreCharacterPreviewActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unregister from active preview actors
	if (!PreviewActorId.IsNone())
	{
		if (TWeakObjectPtr<ASuspenseCoreCharacterPreviewActor>* ExistingActor = ActivePreviewActors.Find(PreviewActorId))
		{
			if (ExistingActor->Get() == this)
			{
				ActivePreviewActors.Remove(PreviewActorId);
				UE_LOG(LogSuspenseCorePreview, Log,
					TEXT("[CharacterPreviewActor] Unregistered preview actor with ID '%s'"),
					*PreviewActorId.ToString());
			}
		}
	}

	TeardownEventSubscriptions();
	DestroyPreviewActor();

	Super::EndPlay(EndPlayReason);
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCoreCharacterPreviewActor::SetCharacterClass(USuspenseCoreCharacterClassData* ClassData)
{
	if (!ClassData)
	{
		UE_LOG(LogSuspenseCorePreview, Warning, TEXT("[CharacterPreviewActor] SetCharacterClass: ClassData is null"));
		return;
	}

	CurrentClassData = ClassData;

	// Spawn new preview actor
	SpawnPreviewActor(ClassData);

	// Notify Blueprint
	OnClassChanged(ClassData);

	UE_LOG(LogSuspenseCorePreview, Log, TEXT("[CharacterPreviewActor] Character class set: %s"),
		*ClassData->DisplayName.ToString());
}

void ASuspenseCoreCharacterPreviewActor::RotatePreview(float DeltaYaw)
{
	CurrentYaw += DeltaYaw * RotationSpeed;
	CurrentYaw = FMath::Fmod(CurrentYaw, 360.0f);

	// Get the actor to rotate
	AActor* ActorToRotate = bSpawnsPreviewActor ? SpawnedPreviewActor : this;

	if (ActorToRotate)
	{
		FRotator NewRotation = ActorToRotate->GetActorRotation();
		NewRotation.Yaw = CurrentYaw;
		ActorToRotate->SetActorRotation(NewRotation);
	}
}

void ASuspenseCoreCharacterPreviewActor::SetPreviewRotation(float Yaw)
{
	CurrentYaw = FMath::Fmod(Yaw, 360.0f);

	// Get the actor to rotate
	AActor* ActorToRotate = bSpawnsPreviewActor ? SpawnedPreviewActor : this;

	if (ActorToRotate)
	{
		FRotator NewRotation = ActorToRotate->GetActorRotation();
		NewRotation.Yaw = CurrentYaw;
		ActorToRotate->SetActorRotation(NewRotation);
	}
}

USkeletalMeshComponent* ASuspenseCoreCharacterPreviewActor::GetPreviewMesh() const
{
	// Get the actor that is the preview
	const AActor* PreviewActor = bSpawnsPreviewActor ? SpawnedPreviewActor : this;

	if (!PreviewActor)
	{
		return nullptr;
	}

	// Find first SkeletalMeshComponent in preview actor
	return PreviewActor->FindComponentByClass<USkeletalMeshComponent>();
}

// ═══════════════════════════════════════════════════════════════════════════════
// PREVIEW ACTOR MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCoreCharacterPreviewActor::SpawnPreviewActor(USuspenseCoreCharacterClassData* ClassData)
{
	if (!ClassData)
	{
		return;
	}

	// If bSpawnsPreviewActor is false, this actor doesn't spawn - it IS the preview
	if (!bSpawnsPreviewActor)
	{
		UE_LOG(LogSuspenseCorePreview, Verbose,
			TEXT("[CharacterPreviewActor] Skipping spawn - bSpawnsPreviewActor is false (this actor IS the preview)"));
		return;
	}

	// Destroy existing preview actor
	DestroyPreviewActor();

	// Load preview actor class
	UClass* ActorClass = ClassData->PreviewActorClass.LoadSynchronous();
	if (!ActorClass)
	{
		UE_LOG(LogSuspenseCorePreview, Warning, TEXT("[CharacterPreviewActor] PreviewActorClass not set for: %s"),
			*ClassData->ClassID.ToString());
		return;
	}

	// Spawn actor at our location
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	FTransform SpawnTransform = GetActorTransform();
	SpawnTransform.SetRotation(FQuat(FRotator(0.0f, CurrentYaw, 0.0f)));

	SpawnedPreviewActor = GetWorld()->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParams);

	if (SpawnedPreviewActor)
	{
		// Disable collision on spawned actor
		SpawnedPreviewActor->SetActorEnableCollision(false);

		// Apply custom AnimBP if specified
		ApplyAnimationBlueprint(ClassData);

		UE_LOG(LogSuspenseCorePreview, Log, TEXT("[CharacterPreviewActor] Spawned preview actor: %s for class: %s"),
			*ActorClass->GetName(), *ClassData->ClassID.ToString());
	}
	else
	{
		UE_LOG(LogSuspenseCorePreview, Error, TEXT("[CharacterPreviewActor] Failed to spawn preview actor for: %s"),
			*ClassData->ClassID.ToString());
	}
}

void ASuspenseCoreCharacterPreviewActor::DestroyPreviewActor()
{
	if (SpawnedPreviewActor)
	{
		SpawnedPreviewActor->Destroy();
		SpawnedPreviewActor = nullptr;
	}
}

void ASuspenseCoreCharacterPreviewActor::ApplyAnimationBlueprint(USuspenseCoreCharacterClassData* ClassData)
{
	if (!SpawnedPreviewActor || !ClassData)
	{
		return;
	}

	// Check if custom AnimBP is specified
	if (ClassData->AnimationBlueprint.IsNull())
	{
		UE_LOG(LogSuspenseCorePreview, Verbose, TEXT("[CharacterPreviewActor] No custom AnimBP - using actor's default"));
		return;
	}

	// Load AnimBP class
	TSubclassOf<UAnimInstance> AnimClass = ClassData->AnimationBlueprint.LoadSynchronous();
	if (!AnimClass)
	{
		UE_LOG(LogSuspenseCorePreview, Warning, TEXT("[CharacterPreviewActor] Failed to load AnimBP for: %s"),
			*ClassData->ClassID.ToString());
		return;
	}

	// Find SkeletalMeshComponent and apply AnimBP
	USkeletalMeshComponent* MeshComp = GetPreviewMesh();
	if (MeshComp)
	{
		MeshComp->SetAnimInstanceClass(AnimClass);
		MeshComp->InitAnim(true);

		UE_LOG(LogSuspenseCorePreview, Log, TEXT("[CharacterPreviewActor] Applied AnimBP: %s"),
			*AnimClass->GetName());
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// EVENTBUS
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCoreCharacterPreviewActor::SetupEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		UE_LOG(LogSuspenseCorePreview, Error, TEXT("[CharacterPreviewActor] Cannot setup event subscriptions - EventBus not available!"));
		return;
	}

	// Subscribe to character class changed events
	FGameplayTag ClassChangedTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.CharacterClass.Changed"));
	ClassChangedEventHandle = EventBus->SubscribeNative(
		ClassChangedTag,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &ASuspenseCoreCharacterPreviewActor::OnCharacterClassChanged),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to preview rotation events (from PreviewRotationWidget drag)
	FGameplayTag RotateTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Preview.Rotate"));
	RotateEventHandle = EventBus->SubscribeNative(
		RotateTag,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &ASuspenseCoreCharacterPreviewActor::OnRotatePreviewEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to set rotation events (absolute rotation)
	FGameplayTag SetRotationTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Preview.SetRotation"));
	SetRotationEventHandle = EventBus->SubscribeNative(
		SetRotationTag,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &ASuspenseCoreCharacterPreviewActor::OnSetRotationEvent),
		ESuspenseCoreEventPriority::Normal
	);

	UE_LOG(LogSuspenseCorePreview, Log, TEXT("[CharacterPreviewActor] Subscribed to events: CharacterClass.Changed, Preview.Rotate, Preview.SetRotation"));
}

void ASuspenseCoreCharacterPreviewActor::TeardownEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (EventBus)
	{
		if (ClassChangedEventHandle.IsValid())
		{
			EventBus->Unsubscribe(ClassChangedEventHandle);
		}
		if (RotateEventHandle.IsValid())
		{
			EventBus->Unsubscribe(RotateEventHandle);
		}
		if (SetRotationEventHandle.IsValid())
		{
			EventBus->Unsubscribe(SetRotationEventHandle);
		}
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
	UE_LOG(LogSuspenseCorePreview, Log, TEXT("[CharacterPreviewActor] OnCharacterClassChanged event received"));

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

		if (!ClassIdStr.IsEmpty())
		{
			if (USuspenseCoreCharacterClassSubsystem* ClassSubsystem = USuspenseCoreCharacterClassSubsystem::Get(this))
			{
				USuspenseCoreCharacterClassData* LoadedClassData = ClassSubsystem->GetClassById(FName(*ClassIdStr));
				if (LoadedClassData)
				{
					SetCharacterClass(LoadedClassData);
				}
				else
				{
					UE_LOG(LogSuspenseCorePreview, Warning, TEXT("[CharacterPreviewActor] ClassData not found for: %s"), *ClassIdStr);
				}
			}
		}
	}
}

void ASuspenseCoreCharacterPreviewActor::OnRotatePreviewEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	float DeltaYaw = EventData.GetFloat(FName("DeltaYaw"));

	if (!FMath::IsNearlyZero(DeltaYaw))
	{
		RotatePreview(DeltaYaw);
	}
}

void ASuspenseCoreCharacterPreviewActor::OnSetRotationEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	float Yaw = EventData.GetFloat(FName("Yaw"));
	SetPreviewRotation(Yaw);

	UE_LOG(LogSuspenseCorePreview, Verbose, TEXT("[CharacterPreviewActor] Rotation set to: %.1f via event"), Yaw);
}
