// SuspenseCoreMedicalVisualHandler.cpp
// Handles visual medical item spawning and attachment
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Handlers/Medical/SuspenseCoreMedicalVisualHandler.h"
#include "SuspenseCore/Actors/SuspenseCoreMedicalItemActor.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Tags/SuspenseCoreMedicalNativeTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogMedicalVisualHandler, Log, All);

#define HANDLER_LOG(Verbosity, Format, ...) \
	UE_LOG(LogMedicalVisualHandler, Verbosity, TEXT("[MedicalVisual] " Format), ##__VA_ARGS__)

//==================================================================
// Constructor / Destructor
//==================================================================

USuspenseCoreMedicalVisualHandler::USuspenseCoreMedicalVisualHandler()
{
	// Default socket alternatives (same as GrenadeHandler)
	AlternativeSocketNames = {
		FName("GripPoint"),
		FName("RightHandSocket"),
		FName("hand_r"),
		FName("hand_rSocket")
	};
}

USuspenseCoreMedicalVisualHandler::~USuspenseCoreMedicalVisualHandler()
{
	Shutdown();
}

//==================================================================
// Initialization
//==================================================================

void USuspenseCoreMedicalVisualHandler::Initialize(
	USuspenseCoreDataManager* InDataManager,
	USuspenseCoreEventBus* InEventBus)
{
	if (bInitialized)
	{
		HANDLER_LOG(Warning, TEXT("Initialize: Already initialized"));
		return;
	}

	DataManager = InDataManager;
	EventBus = InEventBus;

	if (!InEventBus)
	{
		HANDLER_LOG(Error, TEXT("Initialize: No EventBus provided"));
		return;
	}

	// Subscribe to medical events
	EquippedSubscriptionHandle = InEventBus->SubscribeNative(
		SuspenseCoreMedicalTags::Event::TAG_Event_Medical_Equipped,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(
			this, &USuspenseCoreMedicalVisualHandler::OnMedicalEquipped));

	UnequippedSubscriptionHandle = InEventBus->SubscribeNative(
		SuspenseCoreMedicalTags::Event::TAG_Event_Medical_Unequipped,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(
			this, &USuspenseCoreMedicalVisualHandler::OnMedicalUnequipped));

	bInitialized = true;

	HANDLER_LOG(Log, TEXT("Initialized with DataManager=%s, EventBus=%s"),
		InDataManager ? TEXT("Valid") : TEXT("NULL"),
		TEXT("Valid"));
}

void USuspenseCoreMedicalVisualHandler::Shutdown()
{
	if (!bInitialized)
	{
		return;
	}

	// Unsubscribe from events
	if (EventBus.IsValid())
	{
		EventBus->Unsubscribe(EquippedSubscriptionHandle);
		EventBus->Unsubscribe(UnequippedSubscriptionHandle);
	}

	// Destroy active visuals
	for (auto& Pair : ActiveVisuals)
	{
		if (ASuspenseCoreMedicalItemActor* Actor = Pair.Value.Get())
		{
			Actor->Destroy();
		}
	}
	ActiveVisuals.Empty();

	// Destroy pooled actors
	for (auto& WeakActor : ActorPool)
	{
		if (ASuspenseCoreMedicalItemActor* Actor = WeakActor.Get())
		{
			Actor->Destroy();
		}
	}
	ActorPool.Empty();

	bInitialized = false;

	HANDLER_LOG(Log, TEXT("Shutdown complete"));
}

//==================================================================
// Visual Management
//==================================================================

bool USuspenseCoreMedicalVisualHandler::SpawnVisualMedical(AActor* Character, FName MedicalItemID)
{
	if (!Character)
	{
		HANDLER_LOG(Warning, TEXT("SpawnVisualMedical: No character"));
		return false;
	}

	// Destroy existing visual for this character
	DestroyVisualMedical(Character);

	// Get actor from pool or spawn new
	ASuspenseCoreMedicalItemActor* Visual = AcquireFromPool();
	if (!Visual)
	{
		HANDLER_LOG(Warning, TEXT("SpawnVisualMedical: Failed to acquire actor"));
		return false;
	}

	// Initialize with item data
	Visual->InitializeFromItemID(MedicalItemID, DataManager.Get());

	// Attach to character hand
	if (!AttachToCharacterHand(Visual, Character))
	{
		HANDLER_LOG(Warning, TEXT("SpawnVisualMedical: Failed to attach to character"));
		ReleaseToPool(Visual);
		return false;
	}

	// Show the visual
	Visual->SetVisibility(true);

	// Track active visual
	ActiveVisuals.Add(Character, Visual);

	HANDLER_LOG(Log, TEXT("SpawnVisualMedical: Spawned %s for %s"),
		*MedicalItemID.ToString(), *Character->GetName());

	return true;
}

void USuspenseCoreMedicalVisualHandler::DestroyVisualMedical(AActor* Character)
{
	if (!Character)
	{
		return;
	}

	TWeakObjectPtr<ASuspenseCoreMedicalItemActor>* FoundVisual = ActiveVisuals.Find(Character);
	if (!FoundVisual)
	{
		return;
	}

	ASuspenseCoreMedicalItemActor* Visual = FoundVisual->Get();
	if (Visual)
	{
		// Return to pool instead of destroying
		ReleaseToPool(Visual);
		HANDLER_LOG(Log, TEXT("DestroyVisualMedical: Recycled visual for %s"), *Character->GetName());
	}

	ActiveVisuals.Remove(Character);
}

void USuspenseCoreMedicalVisualHandler::HideVisualMedical(AActor* Character)
{
	if (ASuspenseCoreMedicalItemActor* Visual = GetVisualMedical(Character))
	{
		Visual->SetVisibility(false);
		HANDLER_LOG(Log, TEXT("HideVisualMedical: Hidden for %s"), *Character->GetName());
	}
}

void USuspenseCoreMedicalVisualHandler::ShowVisualMedical(AActor* Character)
{
	if (ASuspenseCoreMedicalItemActor* Visual = GetVisualMedical(Character))
	{
		Visual->SetVisibility(true);
		HANDLER_LOG(Log, TEXT("ShowVisualMedical: Shown for %s"), *Character->GetName());
	}
}

ASuspenseCoreMedicalItemActor* USuspenseCoreMedicalVisualHandler::GetVisualMedical(AActor* Character) const
{
	if (!Character)
	{
		return nullptr;
	}

	const TWeakObjectPtr<ASuspenseCoreMedicalItemActor>* FoundVisual = ActiveVisuals.Find(Character);
	if (FoundVisual)
	{
		return FoundVisual->Get();
	}

	return nullptr;
}

//==================================================================
// EventBus Handlers
//==================================================================

void USuspenseCoreMedicalVisualHandler::OnMedicalEquipped(
	FGameplayTag EventTag,
	const FSuspenseCoreEventData& EventData)
{
	AActor* Character = Cast<AActor>(EventData.Source.Get());
	if (!Character)
	{
		HANDLER_LOG(Warning, TEXT("OnMedicalEquipped: No character in event"));
		return;
	}

	// Get medical item ID from event
	FName MedicalItemID = NAME_None;
	if (const FString* ItemIDStr = EventData.StringPayload.Find(TEXT("MedicalItemID")))
	{
		MedicalItemID = FName(**ItemIDStr);
	}

	HANDLER_LOG(Log, TEXT("OnMedicalEquipped: Character=%s, ItemID=%s"),
		*Character->GetName(), *MedicalItemID.ToString());

	if (!MedicalItemID.IsNone())
	{
		SpawnVisualMedical(Character, MedicalItemID);
	}
	else
	{
		HANDLER_LOG(Warning, TEXT("OnMedicalEquipped: No MedicalItemID in event"));
	}
}

void USuspenseCoreMedicalVisualHandler::OnMedicalUnequipped(
	FGameplayTag EventTag,
	const FSuspenseCoreEventData& EventData)
{
	AActor* Character = Cast<AActor>(EventData.Source.Get());
	if (!Character)
	{
		HANDLER_LOG(Warning, TEXT("OnMedicalUnequipped: No character in event"));
		return;
	}

	HANDLER_LOG(Log, TEXT("OnMedicalUnequipped: Character=%s"), *Character->GetName());

	DestroyVisualMedical(Character);
}

//==================================================================
// Internal Methods - Pooling
//==================================================================

ASuspenseCoreMedicalItemActor* USuspenseCoreMedicalVisualHandler::AcquireFromPool()
{
	// Try to reuse from pool
	while (ActorPool.Num() > 0)
	{
		TWeakObjectPtr<ASuspenseCoreMedicalItemActor> WeakActor = ActorPool.Pop();
		if (ASuspenseCoreMedicalItemActor* Actor = WeakActor.Get())
		{
			Actor->SetActorHiddenInGame(false);
			HANDLER_LOG(Verbose, TEXT("AcquireFromPool: Reused %s"), *Actor->GetName());
			return Actor;
		}
	}

	// Spawn new actor
	UWorld* World = GetWorld();
	if (!World)
	{
		HANDLER_LOG(Warning, TEXT("AcquireFromPool: No world"));
		return nullptr;
	}

	TSubclassOf<ASuspenseCoreMedicalItemActor> ClassToSpawn = MedicalItemActorClass;
	if (!ClassToSpawn)
	{
		ClassToSpawn = ASuspenseCoreMedicalItemActor::StaticClass();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASuspenseCoreMedicalItemActor* NewActor = World->SpawnActor<ASuspenseCoreMedicalItemActor>(
		ClassToSpawn,
		FTransform::Identity,
		SpawnParams);

	if (NewActor)
	{
		HANDLER_LOG(Log, TEXT("AcquireFromPool: Spawned new %s"), *NewActor->GetName());
	}

	return NewActor;
}

void USuspenseCoreMedicalVisualHandler::ReleaseToPool(ASuspenseCoreMedicalItemActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	// Reset actor state
	Actor->ResetForPool();

	// Add to pool if not at max
	if (ActorPool.Num() < MaxPoolSize)
	{
		ActorPool.Add(Actor);
		HANDLER_LOG(Verbose, TEXT("ReleaseToPool: %s (pool size: %d)"),
			*Actor->GetName(), ActorPool.Num());
	}
	else
	{
		// Pool full, destroy
		Actor->Destroy();
		HANDLER_LOG(Verbose, TEXT("ReleaseToPool: Pool full, destroyed %s"), *Actor->GetName());
	}
}

//==================================================================
// Internal Methods - Attachment
//==================================================================

bool USuspenseCoreMedicalVisualHandler::AttachToCharacterHand(
	ASuspenseCoreMedicalItemActor* Actor,
	AActor* Character)
{
	if (!Actor || !Character)
	{
		return false;
	}

	FName FinalSocketName;
	USkeletalMeshComponent* TargetMesh = FindAttachmentTarget(Character, FinalSocketName);

	if (!TargetMesh)
	{
		HANDLER_LOG(Warning, TEXT("AttachToCharacterHand: No suitable mesh found on %s"),
			*Character->GetName());
		return false;
	}

	// Detach first
	Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// Attach to socket
	FAttachmentTransformRules AttachRules(
		EAttachmentRule::SnapToTarget,
		EAttachmentRule::SnapToTarget,
		EAttachmentRule::SnapToTarget,
		false);

	Actor->AttachToComponent(
		TargetMesh,
		AttachRules,
		FinalSocketName);

	// Apply offset for this medical type
	FTransform Offset = Actor->GetAttachOffset();
	Actor->SetActorRelativeTransform(Offset);

	HANDLER_LOG(Log, TEXT("AttachToCharacterHand: Attached to %s socket '%s'"),
		*TargetMesh->GetName(), *FinalSocketName.ToString());

	return true;
}

USkeletalMeshComponent* USuspenseCoreMedicalVisualHandler::FindAttachmentTarget(
	AActor* Character,
	FName& OutSocketName) const
{
	if (!Character)
	{
		return nullptr;
	}

	// Build list of sockets to try
	TArray<FName> SocketsToTry;
	SocketsToTry.Add(AttachSocketName);
	SocketsToTry.Append(AlternativeSocketNames);

	// Get all skeletal mesh components
	TArray<USkeletalMeshComponent*> SkelMeshes;
	Character->GetComponents<USkeletalMeshComponent>(SkelMeshes);

	// First pass: Look for "Body" component with socket (MetaHuman pattern)
	for (USkeletalMeshComponent* SkelMesh : SkelMeshes)
	{
		if (!SkelMesh || !SkelMesh->GetSkeletalMeshAsset())
		{
			continue;
		}

		if (SkelMesh->GetName().Contains(TEXT("Body")))
		{
			for (const FName& Socket : SocketsToTry)
			{
				if (SkelMesh->DoesSocketExist(Socket))
				{
					OutSocketName = Socket;
					return SkelMesh;
				}
			}
		}
	}

	// Second pass: Any mesh with socket
	for (USkeletalMeshComponent* SkelMesh : SkelMeshes)
	{
		if (!SkelMesh || !SkelMesh->GetSkeletalMeshAsset())
		{
			continue;
		}

		for (const FName& Socket : SocketsToTry)
		{
			if (SkelMesh->DoesSocketExist(Socket))
			{
				OutSocketName = Socket;
				return SkelMesh;
			}
		}
	}

	// Fallback: Default character mesh
	if (ACharacter* AsCharacter = Cast<ACharacter>(Character))
	{
		USkeletalMeshComponent* CharMesh = AsCharacter->GetMesh();
		if (CharMesh)
		{
			OutSocketName = AttachSocketName;
			return CharMesh;
		}
	}

	return nullptr;
}

#undef HANDLER_LOG
