// SuspenseCoreCharacterPreviewComponent.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "Components/SuspenseCoreCharacterPreviewComponent.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Subsystems/SuspenseCoreCharacterClassSubsystem.h"
#include "SuspenseCore/Data/SuspenseCoreCharacterClassData.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"

USuspenseCoreCharacterPreviewComponent::USuspenseCoreCharacterPreviewComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USuspenseCoreCharacterPreviewComponent::BeginPlay()
{
	Super::BeginPlay();

	// Find mesh component if not assigned
	FindMeshComponentIfNeeded();

	// Setup EventBus subscriptions
	if (bAutoSubscribeToEvents)
	{
		SetupEventSubscriptions();
	}

	// Set default class if specified
	if (!DefaultClassId.IsNone())
	{
		SetClassById(DefaultClassId);
	}
}

void USuspenseCoreCharacterPreviewComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TeardownEventSubscriptions();
	Super::EndPlay(EndPlayReason);
}

void USuspenseCoreCharacterPreviewComponent::SetupEventSubscriptions()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(World);
	if (!Manager)
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreCharacterPreviewComponent: EventManager not found"));
		return;
	}

	CachedEventBus = Manager->GetEventBus();
	if (!CachedEventBus.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreCharacterPreviewComponent: EventBus not found"));
		return;
	}

	// Subscribe to ClassPreview.Selected event
	ClassPreviewEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.ClassPreview.Selected")),
		const_cast<USuspenseCoreCharacterPreviewComponent*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreCharacterPreviewComponent::OnClassPreviewEvent),
		ESuspenseCoreEventPriority::Normal
	);

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreCharacterPreviewComponent: Subscribed to ClassPreview events"));
}

void USuspenseCoreCharacterPreviewComponent::TeardownEventSubscriptions()
{
	if (CachedEventBus.IsValid() && ClassPreviewEventHandle.IsValid())
	{
		CachedEventBus->Unsubscribe(ClassPreviewEventHandle);
	}
}

void USuspenseCoreCharacterPreviewComponent::FindMeshComponentIfNeeded()
{
	if (PreviewMeshComponent)
	{
		return; // Already assigned
	}

	// Try to find a skeletal mesh component on the owner
	AActor* Owner = GetOwner();
	if (Owner)
	{
		PreviewMeshComponent = Owner->FindComponentByClass<USkeletalMeshComponent>();
		if (!PreviewMeshComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreCharacterPreviewComponent: No SkeletalMeshComponent found on %s"), *Owner->GetName());
		}
	}
}

void USuspenseCoreCharacterPreviewComponent::SetClassById(FName ClassId)
{
	if (ClassId.IsNone())
	{
		ClearPreview();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Get CharacterClassSubsystem
	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	USuspenseCoreCharacterClassSubsystem* ClassSubsystem = GameInstance->GetSubsystem<USuspenseCoreCharacterClassSubsystem>();
	if (!ClassSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreCharacterPreviewComponent: CharacterClassSubsystem not found"));
		return;
	}

	// Get class data
	USuspenseCoreCharacterClassData* ClassData = ClassSubsystem->GetClassById(ClassId);
	if (ClassData)
	{
		SetClassData(ClassData);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreCharacterPreviewComponent: Class %s not found"), *ClassId.ToString());
	}
}

void USuspenseCoreCharacterPreviewComponent::SetClassData(USuspenseCoreCharacterClassData* ClassData)
{
	if (!ClassData)
	{
		ClearPreview();
		return;
	}

	CurrentClassId = ClassData->ClassID;
	CurrentClassData = ClassData;

	// Apply to mesh
	ApplyClassDataToMesh(ClassData);

	// Notify Blueprint
	OnClassPreviewChanged(CurrentClassId, ClassData);
}

void USuspenseCoreCharacterPreviewComponent::ClearPreview()
{
	CurrentClassId = NAME_None;
	CurrentClassData.Reset();

	if (PreviewMeshComponent)
	{
		PreviewMeshComponent->SetSkeletalMesh(nullptr);
		PreviewMeshComponent->SetVisibility(false);
	}
}

void USuspenseCoreCharacterPreviewComponent::PlayPreviewAnimation()
{
	if (!PreviewMeshComponent || !CurrentClassData.IsValid())
	{
		return;
	}

	USuspenseCoreCharacterClassData* ClassData = CurrentClassData.Get();
	if (ClassData->PreviewIdleAnimation.IsValid())
	{
		// Load and play animation
		UAnimSequence* Anim = ClassData->PreviewIdleAnimation.LoadSynchronous();
		if (Anim)
		{
			PreviewMeshComponent->PlayAnimation(Anim, true);
		}
	}
}

void USuspenseCoreCharacterPreviewComponent::OnClassPreviewEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Extract ClassId from EventData
	FName ClassId = NAME_None;

	// Try to get from StringValue first
	if (!EventData.StringValue.IsEmpty())
	{
		ClassId = FName(*EventData.StringValue);
	}
	// Try to get from payload if ClassData was passed directly
	else if (EventData.Payload.IsValid())
	{
		USuspenseCoreCharacterClassData* ClassData = Cast<USuspenseCoreCharacterClassData>(
			static_cast<UObject*>(const_cast<void*>(EventData.Payload.Get()))
		);
		if (ClassData)
		{
			SetClassData(ClassData);
			return;
		}
	}

	if (!ClassId.IsNone())
	{
		SetClassById(ClassId);
	}
}

void USuspenseCoreCharacterPreviewComponent::ApplyClassDataToMesh(USuspenseCoreCharacterClassData* ClassData)
{
	if (!PreviewMeshComponent || !ClassData)
	{
		return;
	}

	// Check if mesh is set
	if (ClassData->CharacterMesh.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreCharacterPreviewComponent: No CharacterMesh set for class %s"), *ClassData->ClassID.ToString());
		return;
	}

	// Load mesh asynchronously
	LoadAndApplyMesh(ClassData->CharacterMesh);

	// Set animation blueprint if specified
	if (!ClassData->AnimationBlueprint.IsNull())
	{
		TSubclassOf<UAnimInstance> AnimBP = ClassData->AnimationBlueprint.LoadSynchronous();
		if (AnimBP)
		{
			PreviewMeshComponent->SetAnimInstanceClass(AnimBP);
		}
	}
}

void USuspenseCoreCharacterPreviewComponent::LoadAndApplyMesh(TSoftObjectPtr<USkeletalMesh> MeshPtr)
{
	if (MeshPtr.IsNull())
	{
		return;
	}

	bIsLoadingMesh = true;
	OnMeshLoadingStarted();

	// Check if already loaded
	if (MeshPtr.IsValid())
	{
		PreviewMeshComponent->SetSkeletalMesh(MeshPtr.Get());
		PreviewMeshComponent->SetVisibility(true);
		OnMeshLoaded();
		return;
	}

	// Async load
	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	StreamableManager.RequestAsyncLoad(
		MeshPtr.ToSoftObjectPath(),
		FStreamableDelegate::CreateLambda([this, MeshPtr]()
		{
			if (PreviewMeshComponent && MeshPtr.IsValid())
			{
				PreviewMeshComponent->SetSkeletalMesh(MeshPtr.Get());
				PreviewMeshComponent->SetVisibility(true);
			}
			OnMeshLoaded();
		})
	);
}

void USuspenseCoreCharacterPreviewComponent::OnMeshLoaded()
{
	bIsLoadingMesh = false;
	OnMeshLoadingCompleted();

	// Play preview animation if configured
	if (bAutoPlayAnimation)
	{
		PlayPreviewAnimation();
	}
}
