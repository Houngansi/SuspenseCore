// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreEquipmentVisualizationService.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"

USuspenseCoreEquipmentVisualizationService::USuspenseCoreEquipmentVisualizationService()
	: ServiceState(EServiceLifecycleState::Uninitialized)
	, DefaultAttachSocket(NAME_None)
	, bEnableActorPooling(false)
	, ActorPoolSize(10)
	, bEnableLODManagement(true)
	, bEnableDetailedLogging(false)
	, TotalActorsSpawned(0)
	, TotalActorsDespawned(0)
	, ActiveActorCount(0)
	, PoolHits(0)
	, PoolMisses(0)
{
}

USuspenseCoreEquipmentVisualizationService::~USuspenseCoreEquipmentVisualizationService()
{
}

//========================================
// ISuspenseEquipmentService Interface
//========================================

bool USuspenseCoreEquipmentVisualizationService::InitializeService(const FServiceInitParams& Params)
{
	TRACK_SERVICE_INIT();

	ServiceState = EServiceLifecycleState::Initializing;
	ServiceLocator = Params.ServiceLocator;

	if (!InitializeVisualProviders())
	{
		LOG_SERVICE_ERROR(TEXT("Failed to initialize visual providers"));
		ServiceState = EServiceLifecycleState::Failed;
		return false;
	}

	SetupEventSubscriptions();

	InitializationTime = FDateTime::UtcNow();
	ServiceState = EServiceLifecycleState::Ready;

	LOG_SERVICE_INFO(TEXT("Service initialized successfully"));
	return true;
}

bool USuspenseCoreEquipmentVisualizationService::ShutdownService(bool bForce)
{
	TRACK_SERVICE_SHUTDOWN();

	ServiceState = EServiceLifecycleState::Shutting;
	CleanupSpawnedActors();
	ServiceState = EServiceLifecycleState::Shutdown;

	LOG_SERVICE_INFO(TEXT("Service shut down"));
	return true;
}

EServiceLifecycleState USuspenseCoreEquipmentVisualizationService::GetServiceState() const
{
	return ServiceState;
}

bool USuspenseCoreEquipmentVisualizationService::IsServiceReady() const
{
	return ServiceState == EServiceLifecycleState::Ready;
}

FGameplayTag USuspenseCoreEquipmentVisualizationService::GetServiceTag() const
{
	static FGameplayTag ServiceTag = FGameplayTag::RequestGameplayTag(FName("Equipment.Service.Visualization"));
	return ServiceTag;
}

FGameplayTagContainer USuspenseCoreEquipmentVisualizationService::GetRequiredDependencies() const
{
	FGameplayTagContainer Dependencies;
	Dependencies.AddTag(FGameplayTag::RequestGameplayTag(FName("Equipment.Service.Data")));
	return Dependencies;
}

bool USuspenseCoreEquipmentVisualizationService::ValidateService(TArray<FText>& OutErrors) const
{
	return true;
}

void USuspenseCoreEquipmentVisualizationService::ResetService()
{
	CleanupSpawnedActors();
	TotalActorsSpawned = 0;
	TotalActorsDespawned = 0;
	ActiveActorCount = 0;
	PoolHits = 0;
	PoolMisses = 0;
	LOG_SERVICE_INFO(TEXT("Service reset"));
}

FString USuspenseCoreEquipmentVisualizationService::GetServiceStats() const
{
	return FString::Printf(TEXT("Visualization - Spawned: %d, Despawned: %d, Active: %d, Pool Hits: %d, Pool Misses: %d"),
		TotalActorsSpawned, TotalActorsDespawned, ActiveActorCount, PoolHits, PoolMisses);
}

//========================================
// IEquipmentVisualizationService Interface
//========================================

ISuspenseVisualProvider* USuspenseCoreEquipmentVisualizationService::GetVisualProvider()
{
	return nullptr;
}

ISuspenseActorFactory* USuspenseCoreEquipmentVisualizationService::GetActorFactory()
{
	return nullptr;
}

ISuspenseAttachmentProvider* USuspenseCoreEquipmentVisualizationService::GetAttachmentProvider()
{
	return nullptr;
}

//========================================
// Actor Management
//========================================

AActor* USuspenseCoreEquipmentVisualizationService::SpawnEquipmentActor(const FSuspenseInventoryItemInstance& Item, AActor* Owner)
{
	CHECK_SERVICE_READY();

	AActor* SpawnedActor = SpawnActorInternal(Item, Owner);

	if (SpawnedActor)
	{
		TotalActorsSpawned++;
		ActiveActorCount++;
		LOG_SERVICE_VERBOSE(TEXT("Equipment actor spawned for slot"));
	}

	return SpawnedActor;
}

bool USuspenseCoreEquipmentVisualizationService::DespawnEquipmentActor(AActor* EquipmentActor)
{
	CHECK_SERVICE_READY_BOOL();

	if (!EquipmentActor)
	{
		return false;
	}

	EquipmentActor->Destroy();
	TotalActorsDespawned++;
	ActiveActorCount--;

	LOG_SERVICE_VERBOSE(TEXT("Equipment actor despawned"));
	return true;
}

AActor* USuspenseCoreEquipmentVisualizationService::GetActorForSlot(int32 SlotIndex) const
{
	if (const TObjectPtr<AActor>* ActorPtr = SpawnedActors.Find(SlotIndex))
	{
		return *ActorPtr;
	}

	return nullptr;
}

TArray<AActor*> USuspenseCoreEquipmentVisualizationService::GetAllEquipmentActors() const
{
	TArray<AActor*> Actors;
	SpawnedActors.GenerateValueArray(Actors);
	return Actors;
}

//========================================
// Attachment Management
//========================================

bool USuspenseCoreEquipmentVisualizationService::AttachToSocket(AActor* EquipmentActor, USkeletalMeshComponent* ParentMesh, FName SocketName)
{
	CHECK_SERVICE_READY_BOOL();

	if (!EquipmentActor || !ParentMesh)
	{
		return false;
	}

	EquipmentActor->AttachToComponent(ParentMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
	LOG_SERVICE_VERBOSE(TEXT("Actor attached to socket: %s"), *SocketName.ToString());
	return true;
}

bool USuspenseCoreEquipmentVisualizationService::DetachEquipmentActor(AActor* EquipmentActor)
{
	CHECK_SERVICE_READY_BOOL();

	if (!EquipmentActor)
	{
		return false;
	}

	EquipmentActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	LOG_SERVICE_VERBOSE(TEXT("Actor detached"));
	return true;
}

FName USuspenseCoreEquipmentVisualizationService::GetSocketForSlot(int32 SlotIndex) const
{
	if (const FName* SocketName = SlotSocketMap.Find(SlotIndex))
	{
		return *SocketName;
	}

	return DefaultAttachSocket;
}

bool USuspenseCoreEquipmentVisualizationService::UpdateAttachmentTransform(AActor* EquipmentActor, const FTransform& RelativeTransform)
{
	CHECK_SERVICE_READY_BOOL();

	if (!EquipmentActor)
	{
		return false;
	}

	EquipmentActor->SetActorRelativeTransform(RelativeTransform);
	return true;
}

//========================================
// Visual State Management
//========================================

void USuspenseCoreEquipmentVisualizationService::UpdateSlotVisuals(int32 SlotIndex)
{
	CHECK_SERVICE_READY();

	LOG_SERVICE_VERBOSE(TEXT("Updating visuals for slot %d"), SlotIndex);
	PublishVisualUpdated(SlotIndex);
}

void USuspenseCoreEquipmentVisualizationService::SetEquipmentVisibility(int32 SlotIndex, bool bVisible)
{
	CHECK_SERVICE_READY();

	AActor* Actor = GetActorForSlot(SlotIndex);
	if (Actor)
	{
		Actor->SetActorHiddenInGame(!bVisible);
	}
}

void USuspenseCoreEquipmentVisualizationService::ApplyVisualCustomization(int32 SlotIndex, const FEquipmentVisualData& VisualData)
{
	CHECK_SERVICE_READY();

	// Apply visual customization logic
}

void USuspenseCoreEquipmentVisualizationService::RefreshAllVisuals()
{
	CHECK_SERVICE_READY();

	for (const auto& Pair : SpawnedActors)
	{
		UpdateSlotVisuals(Pair.Key);
	}

	LOG_SERVICE_INFO(TEXT("All visuals refreshed"));
}

//========================================
// Material Management
//========================================

void USuspenseCoreEquipmentVisualizationService::UpdateEquipmentMaterials(AActor* EquipmentActor, const TArray<UMaterialInterface*>& Materials)
{
	CHECK_SERVICE_READY();

	if (!EquipmentActor)
	{
		return;
	}

	// Update materials logic
}

void USuspenseCoreEquipmentVisualizationService::SetMaterialParameter(AActor* EquipmentActor, FName ParameterName, float Value)
{
	CHECK_SERVICE_READY();

	if (!EquipmentActor)
	{
		return;
	}

	// Set material parameter logic
}

void USuspenseCoreEquipmentVisualizationService::SetMaterialVectorParameter(AActor* EquipmentActor, FName ParameterName, FLinearColor Value)
{
	CHECK_SERVICE_READY();

	if (!EquipmentActor)
	{
		return;
	}

	// Set material vector parameter logic
}

//========================================
// Animation Integration
//========================================

void USuspenseCoreEquipmentVisualizationService::NotifyAnimationSystemChanged(int32 SlotIndex)
{
	CHECK_SERVICE_READY();

	// Notify animation system
}

UAnimationAsset* USuspenseCoreEquipmentVisualizationService::GetEquipmentAnimationSet(int32 SlotIndex) const
{
	CHECK_SERVICE_READY();

	return nullptr;
}

//========================================
// Event Publishing
//========================================

void USuspenseCoreEquipmentVisualizationService::PublishVisualSpawned(int32 SlotIndex, AActor* SpawnedActor)
{
	PUBLISH_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Visual.Spawned")),
		FSuspenseEquipmentEventData()
	);
}

void USuspenseCoreEquipmentVisualizationService::PublishVisualDespawned(int32 SlotIndex, AActor* DespawnedActor)
{
	PUBLISH_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Visual.Despawned")),
		FSuspenseEquipmentEventData()
	);
}

void USuspenseCoreEquipmentVisualizationService::PublishVisualUpdated(int32 SlotIndex)
{
	PUBLISH_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Visual.Updated")),
		FSuspenseEquipmentEventData()
	);
}

//========================================
// Service Lifecycle
//========================================

bool USuspenseCoreEquipmentVisualizationService::InitializeVisualProviders()
{
	// Initialize visual providers
	return true;
}

void USuspenseCoreEquipmentVisualizationService::SetupEventSubscriptions()
{
	// Subscribe to equipment events
	SUBSCRIBE_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Equipped")),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentVisualizationService::OnEquipmentEquipped)
	);

	SUBSCRIBE_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Unequipped")),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentVisualizationService::OnEquipmentUnequipped)
	);
}

void USuspenseCoreEquipmentVisualizationService::CleanupSpawnedActors()
{
	for (auto& Pair : SpawnedActors)
	{
		if (Pair.Value)
		{
			Pair.Value->Destroy();
		}
	}

	SpawnedActors.Empty();
	PendingDespawn.Empty();
	ActiveActorCount = 0;

	LOG_SERVICE_INFO(TEXT("All spawned actors cleaned up"));
}

//========================================
// Visual Operations
//========================================

AActor* USuspenseCoreEquipmentVisualizationService::SpawnActorInternal(const FSuspenseInventoryItemInstance& Item, AActor* Owner)
{
	// Actual actor spawning logic
	return nullptr;
}

TSubclassOf<AActor> USuspenseCoreEquipmentVisualizationService::GetActorClassForItem(const FSuspenseInventoryItemInstance& Item) const
{
	// Get actor class from item definition
	return nullptr;
}

void USuspenseCoreEquipmentVisualizationService::ConfigureSpawnedActor(AActor* Actor, const FSuspenseInventoryItemInstance& Item)
{
	// Configure actor after spawn
}

//========================================
// Event Handlers
//========================================

void USuspenseCoreEquipmentVisualizationService::OnEquipmentEquipped(const FSuspenseEquipmentEventData& EventData)
{
	// Handle equipment equipped event
}

void USuspenseCoreEquipmentVisualizationService::OnEquipmentUnequipped(const FSuspenseEquipmentEventData& EventData)
{
	// Handle equipment unequipped event
}

void USuspenseCoreEquipmentVisualizationService::OnEquipmentSwapped(const FSuspenseEquipmentEventData& EventData)
{
	// Handle equipment swapped event
}

void USuspenseCoreEquipmentVisualizationService::OnVisualSettingsChanged(const FSuspenseEquipmentEventData& EventData)
{
	// Handle visual settings changed event
	RefreshAllVisuals();
}
