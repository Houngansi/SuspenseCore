// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Components/Core/SuspenseCoreSystemCoordinatorComponent.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "AbilitySystemComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreCoordinator, Log, All);

#define COORDINATOR_LOG(Verbosity, Format, ...) \
	UE_LOG(LogSuspenseCoreCoordinator, Verbosity, TEXT("%s: " Format), *GetNameSafe(this), ##__VA_ARGS__)

USuspenseCoreSystemCoordinatorComponent::USuspenseCoreSystemCoordinatorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 1.0f; // Tick once per second for health monitoring
	SetIsReplicatedComponent(false);

	bIsInitialized = false;
	bIsActive = false;
	LastSyncTime = 0.0f;
	TotalCoordinationEvents = 0;
	TotalSubsystemErrors = 0;

	// Configuration defaults
	bEnableHealthMonitoring = true;
	HealthCheckInterval = 5.0f;
}

void USuspenseCoreSystemCoordinatorComponent::BeginPlay()
{
	Super::BeginPlay();
	COORDINATOR_LOG(Log, TEXT("BeginPlay"));
}

void USuspenseCoreSystemCoordinatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	COORDINATOR_LOG(Log, TEXT("EndPlay"));

	// Shutdown all subsystems
	Shutdown(true);

	Super::EndPlay(EndPlayReason);
}

void USuspenseCoreSystemCoordinatorComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsInitialized || !bEnableHealthMonitoring)
	{
		return;
	}

	// Perform health monitoring
	UpdateSubsystemStatuses();
}

bool USuspenseCoreSystemCoordinatorComponent::Initialize(
	USuspenseCoreServiceLocator* InServiceLocator,
	UAbilitySystemComponent* InASC)
{
	if (!InServiceLocator)
	{
		COORDINATOR_LOG(Error, TEXT("Initialize: Invalid ServiceLocator"));
		return false;
	}

	ServiceLocator = InServiceLocator;
	CachedASC = InASC;

	// Get EventBus from ServiceLocator
	EventBus = ServiceLocator->GetService<USuspenseCoreEventBus>();
	if (!EventBus.IsValid())
	{
		COORDINATOR_LOG(Warning, TEXT("Initialize: EventBus not found in ServiceLocator"));
	}

	// Setup event subscriptions
	SetupEventSubscriptions();

	bIsInitialized = true;
	COORDINATOR_LOG(Log, TEXT("Initialize: Success"));
	return true;
}

void USuspenseCoreSystemCoordinatorComponent::Shutdown(bool bForce)
{
	COORDINATOR_LOG(Log, TEXT("Shutdown: Force=%d"), bForce);

	FScopeLock Lock(&SubsystemCriticalSection);

	// Deactivate all subsystems
	DeactivateAllSubsystems();

	// Clear subsystems
	RegisteredSubsystems.Empty();
	SubsystemStatuses.Empty();

	// Cleanup
	ServiceLocator.Reset();
	EventBus.Reset();
	CachedASC.Reset();

	bIsInitialized = false;
	bIsActive = false;
}

bool USuspenseCoreSystemCoordinatorComponent::RegisterSubsystem(FGameplayTag SubsystemTag, UObject* Subsystem)
{
	if (!SubsystemTag.IsValid() || !Subsystem)
	{
		COORDINATOR_LOG(Error, TEXT("RegisterSubsystem: Invalid parameters"));
		return false;
	}

	FScopeLock Lock(&SubsystemCriticalSection);

	// Check if already registered
	if (RegisteredSubsystems.Contains(SubsystemTag))
	{
		COORDINATOR_LOG(Warning, TEXT("RegisterSubsystem: Subsystem %s already registered"), *SubsystemTag.ToString());
		return false;
	}

	// Register subsystem
	RegisteredSubsystems.Add(SubsystemTag, Subsystem);

	// Create status entry
	FSuspenseCoreSubsystemStatus Status;
	Status.SubsystemTag = SubsystemTag;
	Status.bIsInitialized = false;
	Status.bIsActive = false;
	Status.LastUpdateTime = 0.0f;
	SubsystemStatuses.Add(SubsystemTag, Status);

	COORDINATOR_LOG(Log, TEXT("RegisterSubsystem: Registered %s"), *SubsystemTag.ToString());
	return true;
}

void USuspenseCoreSystemCoordinatorComponent::UnregisterSubsystem(FGameplayTag SubsystemTag)
{
	FScopeLock Lock(&SubsystemCriticalSection);

	RegisteredSubsystems.Remove(SubsystemTag);
	SubsystemStatuses.Remove(SubsystemTag);

	COORDINATOR_LOG(Log, TEXT("UnregisterSubsystem: %s"), *SubsystemTag.ToString());
}

UObject* USuspenseCoreSystemCoordinatorComponent::GetSubsystem(FGameplayTag SubsystemTag) const
{
	FScopeLock Lock(&SubsystemCriticalSection);

	const TObjectPtr<UObject>* Found = RegisteredSubsystems.Find(SubsystemTag);
	return Found ? Found->Get() : nullptr;
}

FSuspenseCoreSubsystemStatus USuspenseCoreSystemCoordinatorComponent::GetSubsystemStatus(
	FGameplayTag SubsystemTag) const
{
	FScopeLock Lock(&SubsystemCriticalSection);

	const FSuspenseCoreSubsystemStatus* Found = SubsystemStatuses.Find(SubsystemTag);
	return Found ? *Found : FSuspenseCoreSubsystemStatus();
}

bool USuspenseCoreSystemCoordinatorComponent::IsSubsystemReady(FGameplayTag SubsystemTag) const
{
	FScopeLock Lock(&SubsystemCriticalSection);

	const FSuspenseCoreSubsystemStatus* Found = SubsystemStatuses.Find(SubsystemTag);
	return Found && Found->bIsInitialized && Found->bIsActive;
}

TArray<FGameplayTag> USuspenseCoreSystemCoordinatorComponent::GetRegisteredSubsystems() const
{
	FScopeLock Lock(&SubsystemCriticalSection);

	TArray<FGameplayTag> Subsystems;
	RegisteredSubsystems.GetKeys(Subsystems);
	return Subsystems;
}

bool USuspenseCoreSystemCoordinatorComponent::InitializeAllSubsystems()
{
	COORDINATOR_LOG(Log, TEXT("InitializeAllSubsystems: Initializing %d subsystems"), RegisteredSubsystems.Num());

	FScopeLock Lock(&SubsystemCriticalSection);

	// Initialize core subsystems first
	if (!InitializeCoreSubsystems())
	{
		COORDINATOR_LOG(Error, TEXT("InitializeAllSubsystems: Failed to initialize core subsystems"));
		return false;
	}

	// TODO: Initialize all registered subsystems in dependency order

	COORDINATOR_LOG(Log, TEXT("InitializeAllSubsystems: Success"));
	return true;
}

void USuspenseCoreSystemCoordinatorComponent::ActivateAllSubsystems()
{
	COORDINATOR_LOG(Log, TEXT("ActivateAllSubsystems"));

	FScopeLock Lock(&SubsystemCriticalSection);

	for (auto& Pair : SubsystemStatuses)
	{
		Pair.Value.bIsActive = true;
		Pair.Value.LastUpdateTime = GetWorld()->GetTimeSeconds();
	}

	bIsActive = true;
}

void USuspenseCoreSystemCoordinatorComponent::DeactivateAllSubsystems()
{
	COORDINATOR_LOG(Log, TEXT("DeactivateAllSubsystems"));

	FScopeLock Lock(&SubsystemCriticalSection);

	for (auto& Pair : SubsystemStatuses)
	{
		Pair.Value.bIsActive = false;
	}

	bIsActive = false;
}

bool USuspenseCoreSystemCoordinatorComponent::ValidateAllSubsystems(TArray<FText>& OutErrors) const
{
	FScopeLock Lock(&SubsystemCriticalSection);

	bool bAllValid = true;

	for (const auto& Pair : SubsystemStatuses)
	{
		if (!Pair.Value.bIsInitialized)
		{
			OutErrors.Add(FText::FromString(FString::Printf(
				TEXT("Subsystem %s is not initialized"),
				*Pair.Key.ToString())));
			bAllValid = false;
		}

		if (!Pair.Value.ErrorMessage.IsEmpty())
		{
			OutErrors.Add(Pair.Value.ErrorMessage);
			bAllValid = false;
		}
	}

	return bAllValid;
}

void USuspenseCoreSystemCoordinatorComponent::BroadcastToSubsystems(
	FGameplayTag EventTag,
	const FSuspenseCoreEventData& EventData)
{
	if (!EventBus.IsValid())
	{
		return;
	}

	EventBus->Publish(EventTag, EventData);
	TotalCoordinationEvents++;

	COORDINATOR_LOG(Verbose, TEXT("BroadcastToSubsystems: %s"), *EventTag.ToString());
}

void USuspenseCoreSystemCoordinatorComponent::RequestSubsystemSync()
{
	COORDINATOR_LOG(Log, TEXT("RequestSubsystemSync"));

	LastSyncTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	// Publish sync event
	PublishCoordinatorEvent(
		FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.Coordinator.Sync"))),
		TEXT("Subsystem synchronization requested"));
}

FString USuspenseCoreSystemCoordinatorComponent::GetCoordinatorStats() const
{
	return FString::Printf(
		TEXT("Subsystems: %d, Events: %d, Errors: %d, Active: %d"),
		RegisteredSubsystems.Num(),
		TotalCoordinationEvents,
		TotalSubsystemErrors,
		bIsActive ? 1 : 0);
}

void USuspenseCoreSystemCoordinatorComponent::OnSubsystemStateChanged(
	FGameplayTag EventTag,
	const FSuspenseCoreEventData& EventData)
{
	COORDINATOR_LOG(Verbose, TEXT("OnSubsystemStateChanged: %s"), *EventTag.ToString());
	UpdateSubsystemStatuses();
}

void USuspenseCoreSystemCoordinatorComponent::OnSubsystemError(
	FGameplayTag EventTag,
	const FSuspenseCoreEventData& EventData)
{
	COORDINATOR_LOG(Warning, TEXT("OnSubsystemError: %s"), *EventTag.ToString());
	TotalSubsystemErrors++;
}

bool USuspenseCoreSystemCoordinatorComponent::InitializeCoreSubsystems()
{
	// TODO: Initialize core subsystems
	return true;
}

void USuspenseCoreSystemCoordinatorComponent::SetupEventSubscriptions()
{
	if (!EventBus.IsValid())
	{
		return;
	}

	// Subscribe to subsystem events
	EventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.Subsystem.StateChanged"))),
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(
			this,
			&USuspenseCoreSystemCoordinatorComponent::OnSubsystemStateChanged));

	EventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.Subsystem.Error"))),
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(
			this,
			&USuspenseCoreSystemCoordinatorComponent::OnSubsystemError));

	COORDINATOR_LOG(Log, TEXT("SetupEventSubscriptions: Complete"));
}

void USuspenseCoreSystemCoordinatorComponent::UpdateSubsystemStatuses()
{
	FScopeLock Lock(&SubsystemCriticalSection);

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	for (auto& Pair : SubsystemStatuses)
	{
		Pair.Value.LastUpdateTime = CurrentTime;
	}
}

void USuspenseCoreSystemCoordinatorComponent::PublishCoordinatorEvent(
	FGameplayTag EventTag,
	const FString& Message)
{
	if (!EventBus.IsValid())
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetString(TEXT("Message"), Message);

	EventBus->Publish(EventTag, EventData);

	COORDINATOR_LOG(Verbose, TEXT("PublishCoordinatorEvent: %s - %s"), *EventTag.ToString(), *Message);
}
