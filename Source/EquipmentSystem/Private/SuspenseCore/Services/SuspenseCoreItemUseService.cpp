// SuspenseCoreItemUseService.cpp
// Item Use Service - SSOT for all item use operations
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreItemUseService.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Services/SuspenseCoreServiceProvider.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreQuickSlotProvider.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogItemUseService, Log, All);

#define ITEMUSE_LOG(Verbosity, Format, ...) \
	UE_LOG(LogItemUseService, Verbosity, TEXT("[ItemUseService] " Format), ##__VA_ARGS__)

//==================================================================
// Constructor
//==================================================================

USuspenseCoreItemUseService::USuspenseCoreItemUseService()
{
}

//==================================================================
// ISuspenseCoreEquipmentService Implementation
//==================================================================

bool USuspenseCoreItemUseService::InitializeService(const FSuspenseCoreServiceInitParams& Params)
{
	if (ServiceState != ESuspenseCoreServiceLifecycleState::Uninitialized)
	{
		ITEMUSE_LOG(Warning, TEXT("InitializeService: Already initialized"));
		return false;
	}

	ServiceState = ESuspenseCoreServiceLifecycleState::Initializing;
	InitializationTime = FDateTime::Now();

	ITEMUSE_LOG(Log, TEXT("InitializeService: Starting initialization..."));

	// Get EventBus from ServiceProvider (following SuspenseCore architecture)
	if (USuspenseCoreServiceProvider* Provider = USuspenseCoreServiceProvider::Get(this))
	{
		EventBus = Provider->GetEventBus();
		ServiceProvider = Provider;
		ITEMUSE_LOG(Log, TEXT("InitializeService: Got EventBus from ServiceProvider (%s)"),
			EventBus.IsValid() ? TEXT("Valid") : TEXT("NULL"));
	}
	else if (Params.ServiceLocator)
	{
		// Fallback: try to get from GameInstance
		if (UGameInstance* GI = Params.ServiceLocator->GetGameInstance())
		{
			if (USuspenseCoreServiceProvider* GIProvider = GI->GetSubsystem<USuspenseCoreServiceProvider>())
			{
				EventBus = GIProvider->GetEventBus();
				ServiceProvider = GIProvider;
				ITEMUSE_LOG(Log, TEXT("InitializeService: Got EventBus from GameInstance->ServiceProvider"));
			}
		}
	}

	ServiceState = ESuspenseCoreServiceLifecycleState::Ready;

	ITEMUSE_LOG(Log, TEXT("InitializeService: Service ready (EventBus=%s, Handlers=%d)"),
		EventBus.IsValid() ? TEXT("Valid") : TEXT("NULL"),
		Handlers.Num());

	return true;
}

bool USuspenseCoreItemUseService::ShutdownService(bool bForce)
{
	if (ServiceState == ESuspenseCoreServiceLifecycleState::Shutdown)
	{
		return true;
	}

	ServiceState = ESuspenseCoreServiceLifecycleState::Shutting;

	ITEMUSE_LOG(Log, TEXT("ShutdownService: Cancelling %d active operations"), ActiveOperations.Num());

	// Cancel all active operations
	for (auto& Pair : ActiveOperations)
	{
		OnItemUseCancelled.Broadcast(Pair.Key, FText::FromString(TEXT("Service shutdown")));
	}
	ActiveOperations.Empty();

	// Clear handlers
	Handlers.Empty();
	HandlerIndexByTag.Empty();

	EventBus.Reset();
	ServiceProvider.Reset();

	ServiceState = ESuspenseCoreServiceLifecycleState::Shutdown;

	ITEMUSE_LOG(Log, TEXT("ShutdownService: Complete. Stats: Processed=%d, Success=%d, Failed=%d, Cancelled=%d"),
		TotalRequestsProcessed, SuccessfulOperations, FailedOperations, CancelledOperations);

	return true;
}

FGameplayTag USuspenseCoreItemUseService::GetServiceTag() const
{
	return FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Service.Equipment.ItemUse"), false);
}

FGameplayTagContainer USuspenseCoreItemUseService::GetRequiredDependencies() const
{
	// No strict dependencies - EventBus is obtained from ServiceProvider
	return FGameplayTagContainer();
}

bool USuspenseCoreItemUseService::ValidateService(TArray<FText>& OutErrors) const
{
	bool bValid = true;

	if (!EventBus.IsValid())
	{
		OutErrors.Add(FText::FromString(TEXT("ItemUseService: EventBus not available")));
		bValid = false;
	}

	if (Handlers.Num() == 0)
	{
		OutErrors.Add(FText::FromString(TEXT("ItemUseService: No handlers registered")));
		// Not a fatal error - service can still accept registrations
	}

	return bValid;
}

void USuspenseCoreItemUseService::ResetService()
{
	// Cancel all operations
	for (auto& Pair : ActiveOperations)
	{
		OnItemUseCancelled.Broadcast(Pair.Key, FText::FromString(TEXT("Service reset")));
	}
	ActiveOperations.Empty();

	// Reset statistics
	TotalRequestsProcessed = 0;
	SuccessfulOperations = 0;
	FailedOperations = 0;
	CancelledOperations = 0;

	ITEMUSE_LOG(Log, TEXT("ResetService: Service reset complete"));
}

FString USuspenseCoreItemUseService::GetServiceStats() const
{
	return FString::Printf(
		TEXT("ItemUseService: Handlers=%d, ActiveOps=%d, Total=%d, Success=%d, Failed=%d, Cancelled=%d, State=%s"),
		Handlers.Num(),
		ActiveOperations.Num(),
		TotalRequestsProcessed,
		SuccessfulOperations,
		FailedOperations,
		CancelledOperations,
		*UEnum::GetValueAsString(ServiceState));
}

//==================================================================
// Legacy Initialization
//==================================================================

void USuspenseCoreItemUseService::Initialize(USuspenseCoreEventBus* InEventBus)
{
	EventBus = InEventBus;
	ServiceState = ESuspenseCoreServiceLifecycleState::Ready;
	InitializationTime = FDateTime::Now();

	ITEMUSE_LOG(Log, TEXT("Initialize (Legacy): EventBus=%s"),
		InEventBus ? TEXT("Valid") : TEXT("NULL"));
}

//==================================================================
// Handler Registration
//==================================================================

bool USuspenseCoreItemUseService::RegisterHandler(TScriptInterface<ISuspenseCoreItemUseHandler> Handler)
{
	if (!Handler.GetInterface())
	{
		ITEMUSE_LOG(Warning, TEXT("RegisterHandler: Invalid handler interface"));
		return false;
	}

	ISuspenseCoreItemUseHandler* HandlerInterface = Handler.GetInterface();
	FGameplayTag HandlerTag = HandlerInterface->GetHandlerTag();

	if (!HandlerTag.IsValid())
	{
		ITEMUSE_LOG(Warning, TEXT("RegisterHandler: Handler has invalid tag"));
		return false;
	}

	// Check for duplicate
	if (HandlerIndexByTag.Contains(HandlerTag))
	{
		ITEMUSE_LOG(Warning, TEXT("RegisterHandler: Handler '%s' already registered"), *HandlerTag.ToString());
		return false;
	}

	// Create entry
	FSuspenseCoreRegisteredHandler Entry;
	Entry.Handler = Handler;
	Entry.HandlerTag = HandlerTag;
	Entry.Priority = static_cast<uint8>(HandlerInterface->GetPriority());

	// Add and update index
	int32 Index = Handlers.Add(Entry);
	HandlerIndexByTag.Add(HandlerTag, Index);

	// Resort by priority
	SortHandlersByPriority();

	ITEMUSE_LOG(Log, TEXT("RegisterHandler: Registered '%s' (Priority=%d), Total handlers=%d"),
		*HandlerTag.ToString(),
		Entry.Priority,
		Handlers.Num());

	return true;
}

bool USuspenseCoreItemUseService::UnregisterHandler(FGameplayTag HandlerTag)
{
	int32* IndexPtr = HandlerIndexByTag.Find(HandlerTag);
	if (!IndexPtr)
	{
		ITEMUSE_LOG(Warning, TEXT("UnregisterHandler: Handler '%s' not found"), *HandlerTag.ToString());
		return false;
	}

	// Remove from array
	Handlers.RemoveAt(*IndexPtr);
	HandlerIndexByTag.Remove(HandlerTag);

	// Rebuild index map
	HandlerIndexByTag.Empty();
	for (int32 i = 0; i < Handlers.Num(); ++i)
	{
		HandlerIndexByTag.Add(Handlers[i].HandlerTag, i);
	}

	ITEMUSE_LOG(Log, TEXT("UnregisterHandler: Unregistered '%s', Remaining handlers=%d"),
		*HandlerTag.ToString(),
		Handlers.Num());

	return true;
}

TArray<FGameplayTag> USuspenseCoreItemUseService::GetRegisteredHandlers() const
{
	TArray<FGameplayTag> Result;
	Result.Reserve(Handlers.Num());

	for (const FSuspenseCoreRegisteredHandler& Entry : Handlers)
	{
		Result.Add(Entry.HandlerTag);
	}

	return Result;
}

bool USuspenseCoreItemUseService::IsHandlerRegistered(FGameplayTag HandlerTag) const
{
	return HandlerIndexByTag.Contains(HandlerTag);
}

void USuspenseCoreItemUseService::SortHandlersByPriority()
{
	// Sort by priority (higher values first via operator<)
	Handlers.Sort();

	// Rebuild index map
	HandlerIndexByTag.Empty();
	for (int32 i = 0; i < Handlers.Num(); ++i)
	{
		HandlerIndexByTag.Add(Handlers[i].HandlerTag, i);
	}
}

//==================================================================
// Validation
//==================================================================

bool USuspenseCoreItemUseService::CanUseItem(const FSuspenseCoreItemUseRequest& Request) const
{
	if (!Request.IsValid())
	{
		return false;
	}

	return FindHandler(Request) != nullptr;
}

FSuspenseCoreItemUseResponse USuspenseCoreItemUseService::ValidateUseRequest(
	const FSuspenseCoreItemUseRequest& Request) const
{
	if (!Request.IsValid())
	{
		return FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_NotUsable,
			FText::FromString(TEXT("Invalid request: no source item")));
	}

	ISuspenseCoreItemUseHandler* Handler = FindHandler(Request);
	if (!Handler)
	{
		return FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_NoHandler,
			FText::FromString(TEXT("No handler found for this item combination")));
	}

	FSuspenseCoreItemUseResponse ValidationResponse;
	if (!Handler->ValidateRequest(Request, ValidationResponse))
	{
		return ValidationResponse;
	}

	// Validation passed
	FSuspenseCoreItemUseResponse Response = FSuspenseCoreItemUseResponse::Success(Request.RequestID);
	Response.HandlerTag = Handler->GetHandlerTag();
	Response.Duration = Handler->GetDuration(Request);
	Response.Cooldown = Handler->GetCooldown(Request);

	return Response;
}

float USuspenseCoreItemUseService::GetUseDuration(const FSuspenseCoreItemUseRequest& Request) const
{
	ISuspenseCoreItemUseHandler* Handler = FindHandler(Request);
	if (!Handler)
	{
		return 0.0f;
	}

	return Handler->GetDuration(Request);
}

float USuspenseCoreItemUseService::GetUseCooldown(const FSuspenseCoreItemUseRequest& Request) const
{
	ISuspenseCoreItemUseHandler* Handler = FindHandler(Request);
	if (!Handler)
	{
		return 0.0f;
	}

	return Handler->GetCooldown(Request);
}

//==================================================================
// Execution
//==================================================================

FSuspenseCoreItemUseResponse USuspenseCoreItemUseService::UseItem(
	const FSuspenseCoreItemUseRequest& Request,
	AActor* OwnerActor)
{
	TotalRequestsProcessed++;

	ITEMUSE_LOG(Verbose, TEXT("UseItem: %s"), *Request.ToString());

	// Validate request
	FSuspenseCoreItemUseResponse ValidationResponse = ValidateUseRequest(Request);
	if (ValidationResponse.IsFailed())
	{
		FailedOperations++;
		PublishEvent(SuspenseCoreItemUseTags::Event::TAG_ItemUse_Event_Failed, Request, ValidationResponse, OwnerActor);
		return ValidationResponse;
	}

	// Find handler (we know it exists from validation)
	ISuspenseCoreItemUseHandler* Handler = GetHandlerByTag(ValidationResponse.HandlerTag);
	if (!Handler)
	{
		FailedOperations++;
		return FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_SystemError,
			FText::FromString(TEXT("Handler disappeared between validation and execution")));
	}

	// Execute
	FSuspenseCoreItemUseResponse Response = Handler->Execute(Request, OwnerActor);
	Response.HandlerTag = Handler->GetHandlerTag();

	// Handle response
	if (Response.IsInProgress())
	{
		// Time-based operation - track it
		FSuspenseCoreActiveOperation Operation;
		Operation.Request = Request;
		Operation.HandlerTag = Response.HandlerTag;
		Operation.StartTime = GetWorldTimeSeconds();
		Operation.Duration = Response.Duration;
		Operation.OwnerActor = OwnerActor;

		ActiveOperations.Add(Request.RequestID, Operation);

		// Publish started event
		PublishEvent(SuspenseCoreItemUseTags::Event::TAG_ItemUse_Event_Started, Request, Response, OwnerActor);
		OnItemUseStarted.Broadcast(Request, Response.Duration);

		ITEMUSE_LOG(Log, TEXT("UseItem: Started time-based operation '%s' (Duration=%.2fs)"),
			*Request.RequestID.ToString().Left(8),
			Response.Duration);
	}
	else if (Response.IsSuccess())
	{
		// Instant success
		SuccessfulOperations++;

		// Publish completed event
		PublishEvent(SuspenseCoreItemUseTags::Event::TAG_ItemUse_Event_Completed, Request, Response, OwnerActor);
		OnItemUseCompleted.Broadcast(Request.RequestID, Response);

		ITEMUSE_LOG(Log, TEXT("UseItem: Instant success for '%s'"),
			*Request.RequestID.ToString().Left(8));
	}
	else
	{
		// Failure
		FailedOperations++;

		PublishEvent(SuspenseCoreItemUseTags::Event::TAG_ItemUse_Event_Failed, Request, Response, OwnerActor);

		ITEMUSE_LOG(Log, TEXT("UseItem: Failed for '%s': %s"),
			*Request.RequestID.ToString().Left(8),
			*Response.Message.ToString());
	}

	return Response;
}

bool USuspenseCoreItemUseService::CancelUse(const FGuid& RequestID)
{
	FSuspenseCoreActiveOperation* Operation = ActiveOperations.Find(RequestID);
	if (!Operation)
	{
		ITEMUSE_LOG(Warning, TEXT("CancelUse: Operation '%s' not found"), *RequestID.ToString().Left(8));
		return false;
	}

	// Try to cancel via handler
	ISuspenseCoreItemUseHandler* Handler = GetHandlerByTag(Operation->HandlerTag);
	if (Handler && Handler->IsCancellable())
	{
		Handler->CancelOperation(RequestID);
	}

	// Build cancelled response
	FSuspenseCoreItemUseResponse CancelledResponse;
	CancelledResponse.Result = ESuspenseCoreItemUseResult::Cancelled;
	CancelledResponse.RequestID = RequestID;
	CancelledResponse.HandlerTag = Operation->HandlerTag;
	CancelledResponse.Message = FText::FromString(TEXT("Operation cancelled"));

	// Publish cancelled event
	PublishEvent(SuspenseCoreItemUseTags::Event::TAG_ItemUse_Event_Cancelled,
		Operation->Request, CancelledResponse, Operation->OwnerActor.Get());

	FText CancelReason = FText::FromString(TEXT("Cancelled by user"));
	OnItemUseCancelled.Broadcast(RequestID, CancelReason);

	CancelledOperations++;
	ActiveOperations.Remove(RequestID);

	ITEMUSE_LOG(Log, TEXT("CancelUse: Cancelled operation '%s'"), *RequestID.ToString().Left(8));

	return true;
}

bool USuspenseCoreItemUseService::IsOperationInProgress(const FGuid& RequestID) const
{
	return ActiveOperations.Contains(RequestID);
}

bool USuspenseCoreItemUseService::GetOperationProgress(const FGuid& RequestID, float& OutProgress) const
{
	const FSuspenseCoreActiveOperation* Operation = ActiveOperations.Find(RequestID);
	if (!Operation)
	{
		OutProgress = 0.0f;
		return false;
	}

	OutProgress = Operation->GetProgress(GetWorldTimeSeconds());
	return true;
}

FSuspenseCoreItemUseResponse USuspenseCoreItemUseService::CompleteOperation(const FGuid& RequestID)
{
	FSuspenseCoreActiveOperation* Operation = ActiveOperations.Find(RequestID);
	if (!Operation)
	{
		ITEMUSE_LOG(Warning, TEXT("CompleteOperation: Operation '%s' not found"), *RequestID.ToString().Left(8));
		return FSuspenseCoreItemUseResponse::Failure(
			RequestID,
			ESuspenseCoreItemUseResult::Failed_SystemError,
			FText::FromString(TEXT("Operation not found")));
	}

	// Get handler to finalize
	ISuspenseCoreItemUseHandler* Handler = GetHandlerByTag(Operation->HandlerTag);
	FSuspenseCoreItemUseResponse Response;

	if (Handler)
	{
		Response = Handler->OnOperationComplete(Operation->Request, Operation->OwnerActor.Get());
	}
	else
	{
		Response = FSuspenseCoreItemUseResponse::Success(RequestID);
	}

	Response.HandlerTag = Operation->HandlerTag;
	Response.Progress = 1.0f;

	// Publish completed event
	PublishEvent(SuspenseCoreItemUseTags::Event::TAG_ItemUse_Event_Completed,
		Operation->Request, Response, Operation->OwnerActor.Get());
	OnItemUseCompleted.Broadcast(RequestID, Response);

	SuccessfulOperations++;
	ActiveOperations.Remove(RequestID);

	ITEMUSE_LOG(Log, TEXT("CompleteOperation: Completed '%s'"), *RequestID.ToString().Left(8));

	return Response;
}

TArray<FGuid> USuspenseCoreItemUseService::GetActiveOperationsForActor(AActor* OwnerActor) const
{
	TArray<FGuid> Result;

	for (const auto& Pair : ActiveOperations)
	{
		if (Pair.Value.OwnerActor.Get() == OwnerActor)
		{
			Result.Add(Pair.Key);
		}
	}

	return Result;
}

void USuspenseCoreItemUseService::CancelAllOperationsForActor(AActor* OwnerActor, const FText& Reason)
{
	TArray<FGuid> ToCancel = GetActiveOperationsForActor(OwnerActor);

	for (const FGuid& RequestID : ToCancel)
	{
		CancelUse(RequestID);
	}

	if (ToCancel.Num() > 0)
	{
		ITEMUSE_LOG(Log, TEXT("CancelAllOperationsForActor: Cancelled %d operations. Reason: %s"),
			ToCancel.Num(),
			*Reason.ToString());
	}
}

//==================================================================
// QuickSlot Helpers
//==================================================================

FSuspenseCoreItemUseResponse USuspenseCoreItemUseService::UseQuickSlot(int32 QuickSlotIndex, AActor* OwnerActor)
{
	ITEMUSE_LOG(Verbose, TEXT("UseQuickSlot: SlotIndex=%d, Actor=%s"),
		QuickSlotIndex,
		OwnerActor ? *OwnerActor->GetName() : TEXT("NULL"));

	// Build request from QuickSlot data
	FSuspenseCoreItemUseRequest Request = BuildQuickSlotRequest(QuickSlotIndex, OwnerActor);

	if (!Request.IsValid())
	{
		return FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_NotUsable,
			FText::FromString(TEXT("QuickSlot is empty or invalid")));
	}

	return UseItem(Request, OwnerActor);
}

bool USuspenseCoreItemUseService::CanUseQuickSlot(int32 QuickSlotIndex, AActor* OwnerActor) const
{
	FSuspenseCoreItemUseRequest Request = BuildQuickSlotRequest(QuickSlotIndex, OwnerActor);
	return CanUseItem(Request);
}

FSuspenseCoreItemUseRequest USuspenseCoreItemUseService::BuildQuickSlotRequest(
	int32 QuickSlotIndex,
	AActor* OwnerActor) const
{
	FSuspenseCoreItemUseRequest Request;
	Request.Context = ESuspenseCoreItemUseContext::QuickSlot;
	Request.QuickSlotIndex = QuickSlotIndex;
	Request.RequestingActor = OwnerActor;
	Request.RequestTime = GetWorldTimeSeconds();

	if (!OwnerActor)
	{
		return Request;
	}

	// Find QuickSlotProvider component on actor
	TArray<UActorComponent*> Components;
	OwnerActor->GetComponents(USuspenseCoreQuickSlotProvider::StaticClass(), Components);

	for (UActorComponent* Component : Components)
	{
		if (Component && Component->Implements<USuspenseCoreQuickSlotProvider>())
		{
			// Get slot data
			FSuspenseCoreQuickSlot SlotData = ISuspenseCoreQuickSlotProvider::Execute_GetQuickSlot(Component, QuickSlotIndex);

			if (SlotData.AssignedItemInstanceID.IsValid())
			{
				// Build item instance for request
				Request.SourceItem.UniqueInstanceID = SlotData.AssignedItemInstanceID;
				Request.SourceItem.ItemID = SlotData.AssignedItemID;
				Request.SourceSlotIndex = QuickSlotIndex;
				Request.SourceContainerTag = SlotData.SlotTag;
				break;
			}
		}
	}

	return Request;
}

//==================================================================
// Handler Query
//==================================================================

FGameplayTag USuspenseCoreItemUseService::FindHandlerForRequest(const FSuspenseCoreItemUseRequest& Request) const
{
	ISuspenseCoreItemUseHandler* Handler = FindHandler(Request);
	if (Handler)
	{
		return Handler->GetHandlerTag();
	}
	return FGameplayTag();
}

ISuspenseCoreItemUseHandler* USuspenseCoreItemUseService::FindHandler(const FSuspenseCoreItemUseRequest& Request) const
{
	// Handlers are sorted by priority (highest first)
	for (const FSuspenseCoreRegisteredHandler& Entry : Handlers)
	{
		if (Entry.IsValid())
		{
			ISuspenseCoreItemUseHandler* Handler = Entry.Handler.GetInterface();
			if (Handler && Handler->CanHandle(Request))
			{
				return Handler;
			}
		}
	}

	return nullptr;
}

ISuspenseCoreItemUseHandler* USuspenseCoreItemUseService::GetHandlerByTag(FGameplayTag HandlerTag) const
{
	const int32* IndexPtr = HandlerIndexByTag.Find(HandlerTag);
	if (!IndexPtr)
	{
		return nullptr;
	}

	if (*IndexPtr >= 0 && *IndexPtr < Handlers.Num())
	{
		return Handlers[*IndexPtr].Handler.GetInterface();
	}

	return nullptr;
}

//==================================================================
// Internal Helpers
//==================================================================

void USuspenseCoreItemUseService::PublishEvent(
	const FGameplayTag& EventTag,
	const FSuspenseCoreItemUseRequest& Request,
	const FSuspenseCoreItemUseResponse& Response,
	AActor* OwnerActor)
{
	if (!EventBus.IsValid())
	{
		ITEMUSE_LOG(Warning, TEXT("PublishEvent: EventBus not available"));
		return;
	}

	FSuspenseCoreEventData EventData;
	EventData.Source = OwnerActor;
	EventData.Timestamp = FPlatformTime::Seconds();
	EventData.Priority = ESuspenseCoreEventPriority::High;

	// Core data
	EventData.StringPayload.Add(TEXT("RequestID"), Request.RequestID.ToString());
	EventData.StringPayload.Add(TEXT("SourceItemID"), Request.SourceItem.ItemID.ToString());
	EventData.IntPayload.Add(TEXT("Context"), static_cast<int32>(Request.Context));
	EventData.IntPayload.Add(TEXT("Result"), static_cast<int32>(Response.Result));
	EventData.IntPayload.Add(TEXT("QuickSlotIndex"), Request.QuickSlotIndex);

	// Handler info
	if (Response.HandlerTag.IsValid())
	{
		EventData.StringPayload.Add(TEXT("HandlerTag"), Response.HandlerTag.ToString());
	}

	// Time-based data
	EventData.FloatPayload.Add(TEXT("Duration"), Response.Duration);
	EventData.FloatPayload.Add(TEXT("Cooldown"), Response.Cooldown);
	EventData.FloatPayload.Add(TEXT("Progress"), Response.Progress);

	// Target (for DragDrop)
	if (Request.HasTarget())
	{
		EventData.StringPayload.Add(TEXT("TargetItemID"), Request.TargetItem.ItemID.ToString());
	}

	// Message
	if (!Response.Message.IsEmpty())
	{
		EventData.StringPayload.Add(TEXT("Message"), Response.Message.ToString());
	}

	EventBus->Publish(EventTag, EventData);

	ITEMUSE_LOG(Verbose, TEXT("PublishEvent: %s for RequestID=%s"),
		*EventTag.ToString(),
		*Request.RequestID.ToString().Left(8));
}

float USuspenseCoreItemUseService::GetWorldTimeSeconds() const
{
	UWorld* World = GetWorld();
	if (World)
	{
		return World->GetTimeSeconds();
	}

	// Fallback
	return static_cast<float>(FPlatformTime::Seconds());
}
