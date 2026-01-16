// SuspenseCoreOptimisticUIManager.cpp
// SuspenseCore - Optimistic UI Manager Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Subsystems/SuspenseCoreOptimisticUIManager.h"
#include "SuspenseCore/Subsystems/SuspenseCoreUIManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/UI/SuspenseCoreUIEvents.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIContainer.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIDataProvider.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogOptimisticUI, Log, All);

//==================================================================
// Static Access
//==================================================================

USuspenseCoreOptimisticUIManager* USuspenseCoreOptimisticUIManager::Get(const UObject* WorldContext)
{
	if (!WorldContext)
	{
		return nullptr;
	}

	UWorld* World = WorldContext->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return nullptr;
	}

	return GI->GetSubsystem<USuspenseCoreOptimisticUIManager>();
}

//==================================================================
// Lifecycle
//==================================================================

void USuspenseCoreOptimisticUIManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	NextPredictionKey = 1;
	PendingPredictions.Empty();

	// Start timeout check timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TimeoutCheckHandle,
			this,
			&USuspenseCoreOptimisticUIManager::CheckPredictionTimeouts,
			TIMEOUT_CHECK_INTERVAL,
			true // Looping
		);
	}

	UE_LOG(LogOptimisticUI, Log, TEXT("USuspenseCoreOptimisticUIManager: Initialized (AAA-Level Optimistic UI)"));
}

void USuspenseCoreOptimisticUIManager::Deinitialize()
{
	// Clear timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimeoutCheckHandle);
	}

	// Rollback any pending predictions before shutdown
	for (auto& Pair : PendingPredictions)
	{
		if (Pair.Value.State == ESuspenseCoreUIPredictionState::Pending)
		{
			UE_LOG(LogOptimisticUI, Warning, TEXT("Rolling back pending prediction %d on shutdown"), Pair.Key);
			ApplyRollback(Pair.Value);
		}
	}

	PendingPredictions.Empty();
	CachedEventBus.Reset();

	Super::Deinitialize();

	UE_LOG(LogOptimisticUI, Log, TEXT("USuspenseCoreOptimisticUIManager: Deinitialized"));
}

//==================================================================
// Prediction Management
//==================================================================

int32 USuspenseCoreOptimisticUIManager::GeneratePredictionKey()
{
	return NextPredictionKey++;
}

bool USuspenseCoreOptimisticUIManager::CreatePrediction(const FSuspenseCoreUIPrediction& Prediction)
{
	if (!Prediction.IsValid())
	{
		UE_LOG(LogOptimisticUI, Warning, TEXT("CreatePrediction: Invalid prediction data"));
		return false;
	}

	if (PendingPredictions.Num() >= MAX_PENDING_PREDICTIONS)
	{
		UE_LOG(LogOptimisticUI, Warning, TEXT("CreatePrediction: Max pending predictions reached (%d)"), MAX_PENDING_PREDICTIONS);
		return false;
	}

	if (PendingPredictions.Contains(Prediction.PredictionKey))
	{
		UE_LOG(LogOptimisticUI, Warning, TEXT("CreatePrediction: Prediction key %d already exists"), Prediction.PredictionKey);
		return false;
	}

	PendingPredictions.Add(Prediction.PredictionKey, Prediction);

	UE_LOG(LogOptimisticUI, Log, TEXT("CreatePrediction: Created prediction %d (type=%d, slots affected=%d)"),
		Prediction.PredictionKey,
		static_cast<int32>(Prediction.OperationType),
		Prediction.AffectedSlotSnapshots.Num());

	BroadcastStateChange(Prediction.PredictionKey, ESuspenseCoreUIPredictionState::Pending);

	return true;
}

int32 USuspenseCoreOptimisticUIManager::CreateMoveItemPrediction(
	TScriptInterface<ISuspenseCoreUIContainer> Container,
	int32 SourceSlot,
	int32 TargetSlot,
	bool bIsRotated)
{
	if (!Container.GetInterface())
	{
		UE_LOG(LogOptimisticUI, Warning, TEXT("CreateMoveItemPrediction: Invalid container"));
		return INDEX_NONE;
	}

	// Get provider for data access
	TScriptInterface<ISuspenseCoreUIDataProvider> Provider = Container->GetBoundProvider();
	if (!Provider.GetInterface())
	{
		UE_LOG(LogOptimisticUI, Warning, TEXT("CreateMoveItemPrediction: Container has no provider"));
		return INDEX_NONE;
	}

	// Get item data from source slot
	FSuspenseCoreItemUIData SourceItemData;
	if (!Provider->GetItemUIDataAtSlot(SourceSlot, SourceItemData))
	{
		UE_LOG(LogOptimisticUI, Warning, TEXT("CreateMoveItemPrediction: No item at source slot %d"), SourceSlot);
		return INDEX_NONE;
	}

	// Generate prediction key
	int32 PredictionKey = GeneratePredictionKey();

	// Create prediction
	FSuspenseCoreUIPrediction Prediction = FSuspenseCoreUIPrediction::CreateMoveItem(
		PredictionKey,
		Provider->GetProviderID(),
		SourceSlot,
		TargetSlot,
		SourceItemData.InstanceID,
		bIsRotated
	);

	// Create snapshots for affected slots
	FIntPoint ItemSize = SourceItemData.GridSize;
	if (bIsRotated)
	{
		Swap(ItemSize.X, ItemSize.Y);
	}

	FIntPoint GridSize = Provider->GetGridSize();

	// Snapshot source slots
	int32 SourceCol = SourceSlot % GridSize.X;
	int32 SourceRow = SourceSlot / GridSize.X;

	for (int32 Y = 0; Y < SourceItemData.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < SourceItemData.GridSize.X; ++X)
		{
			int32 SlotIndex = (SourceRow + Y) * GridSize.X + (SourceCol + X);
			if (SlotIndex >= 0 && SlotIndex < GridSize.X * GridSize.Y)
			{
				FSuspenseCoreSlotUIData SlotData;
				FSuspenseCoreItemUIData ItemData;
				Provider->GetSlotUIData(SlotIndex, SlotData);
				Provider->GetItemUIDataAtSlot(SlotIndex, ItemData);

				Prediction.AddSlotSnapshot(FSuspenseCoreSlotSnapshot::Create(SlotIndex, SlotData, ItemData));
			}
		}
	}

	// Snapshot target slots
	int32 TargetCol = TargetSlot % GridSize.X;
	int32 TargetRow = TargetSlot / GridSize.X;

	for (int32 Y = 0; Y < ItemSize.Y; ++Y)
	{
		for (int32 X = 0; X < ItemSize.X; ++X)
		{
			int32 SlotIndex = (TargetRow + Y) * GridSize.X + (TargetCol + X);
			if (SlotIndex >= 0 && SlotIndex < GridSize.X * GridSize.Y)
			{
				// Only add if not already in snapshot list
				if (!Prediction.FindSlotSnapshot(SlotIndex))
				{
					FSuspenseCoreSlotUIData SlotData;
					FSuspenseCoreItemUIData ItemData;
					Provider->GetSlotUIData(SlotIndex, SlotData);
					Provider->GetItemUIDataAtSlot(SlotIndex, ItemData);

					Prediction.AddSlotSnapshot(FSuspenseCoreSlotSnapshot::Create(SlotIndex, SlotData, ItemData));
				}
			}
		}
	}

	// Store prediction
	if (!CreatePrediction(Prediction))
	{
		return INDEX_NONE;
	}

	return PredictionKey;
}

bool USuspenseCoreOptimisticUIManager::ConfirmPrediction(int32 PredictionKey)
{
	FSuspenseCoreUIPrediction* Prediction = PendingPredictions.Find(PredictionKey);
	if (!Prediction)
	{
		UE_LOG(LogOptimisticUI, Verbose, TEXT("ConfirmPrediction: Prediction %d not found (may have already been processed)"), PredictionKey);
		return false;
	}

	if (Prediction->State != ESuspenseCoreUIPredictionState::Pending)
	{
		UE_LOG(LogOptimisticUI, Warning, TEXT("ConfirmPrediction: Prediction %d is not pending (state=%d)"),
			PredictionKey, static_cast<int32>(Prediction->State));
		return false;
	}

	// Mark as confirmed
	Prediction->State = ESuspenseCoreUIPredictionState::Confirmed;

	UE_LOG(LogOptimisticUI, Log, TEXT("ConfirmPrediction: Prediction %d confirmed (visual state already correct)"), PredictionKey);

	// Broadcast state change
	BroadcastStateChange(PredictionKey, ESuspenseCoreUIPredictionState::Confirmed);

	// Publish success feedback
	PublishFeedbackEvent(true, FText::GetEmpty());

	// Broadcast result
	FSuspenseCoreUIPredictionResult Result = FSuspenseCoreUIPredictionResult::Success(PredictionKey);
	OnPredictionResult.Broadcast(Result);

	// Remove from pending (we don't need it anymore)
	PendingPredictions.Remove(PredictionKey);

	return true;
}

bool USuspenseCoreOptimisticUIManager::RollbackPrediction(int32 PredictionKey, const FText& ErrorMessage)
{
	FSuspenseCoreUIPrediction* Prediction = PendingPredictions.Find(PredictionKey);
	if (!Prediction)
	{
		UE_LOG(LogOptimisticUI, Warning, TEXT("RollbackPrediction: Prediction %d not found"), PredictionKey);
		return false;
	}

	if (Prediction->State != ESuspenseCoreUIPredictionState::Pending)
	{
		UE_LOG(LogOptimisticUI, Warning, TEXT("RollbackPrediction: Prediction %d is not pending (state=%d)"),
			PredictionKey, static_cast<int32>(Prediction->State));
		return false;
	}

	// Mark as rolling back
	Prediction->State = ESuspenseCoreUIPredictionState::RolledBack;

	UE_LOG(LogOptimisticUI, Log, TEXT("RollbackPrediction: Rolling back prediction %d (%d slots to restore)"),
		PredictionKey, Prediction->AffectedSlotSnapshots.Num());

	// Apply rollback
	ApplyRollback(*Prediction);

	// Broadcast state change
	BroadcastStateChange(PredictionKey, ESuspenseCoreUIPredictionState::RolledBack);

	// Publish error feedback
	FText Message = ErrorMessage.IsEmpty()
		? NSLOCTEXT("SuspenseCore", "PredictionFailed", "Action failed")
		: ErrorMessage;
	PublishFeedbackEvent(false, Message);

	// Broadcast result
	FSuspenseCoreUIPredictionResult Result = FSuspenseCoreUIPredictionResult::Failure(PredictionKey, Message);
	OnPredictionResult.Broadcast(Result);

	// Remove from pending
	PendingPredictions.Remove(PredictionKey);

	return true;
}

void USuspenseCoreOptimisticUIManager::ProcessPredictionResult(const FSuspenseCoreUIPredictionResult& Result)
{
	if (Result.bSuccess)
	{
		ConfirmPrediction(Result.PredictionKey);
	}
	else
	{
		RollbackPrediction(Result.PredictionKey, Result.ErrorMessage);
	}
}

//==================================================================
// State Queries
//==================================================================

bool USuspenseCoreOptimisticUIManager::HasPendingPredictionForSlot(const FGuid& ContainerID, int32 SlotIndex) const
{
	for (const auto& Pair : PendingPredictions)
	{
		const FSuspenseCoreUIPrediction& Prediction = Pair.Value;

		if (Prediction.State != ESuspenseCoreUIPredictionState::Pending)
		{
			continue;
		}

		// Check if this prediction affects the container
		if (Prediction.SourceContainerID != ContainerID && Prediction.TargetContainerID != ContainerID)
		{
			continue;
		}

		// Check if any snapshot includes this slot
		for (const FSuspenseCoreSlotSnapshot& Snapshot : Prediction.AffectedSlotSnapshots)
		{
			if (Snapshot.SlotIndex == SlotIndex)
			{
				return true;
			}
		}
	}

	return false;
}

const FSuspenseCoreUIPrediction* USuspenseCoreOptimisticUIManager::GetPrediction(int32 PredictionKey) const
{
	return PendingPredictions.Find(PredictionKey);
}

bool USuspenseCoreOptimisticUIManager::HasPrediction(int32 PredictionKey) const
{
	return PendingPredictions.Contains(PredictionKey);
}

//==================================================================
// Internal Methods
//==================================================================

void USuspenseCoreOptimisticUIManager::ApplyRollback(const FSuspenseCoreUIPrediction& Prediction)
{
	// Get UIManager to find provider
	USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this);
	if (!UIManager)
	{
		UE_LOG(LogOptimisticUI, Error, TEXT("ApplyRollback: UIManager not available"));
		return;
	}

	// Find source provider
	TScriptInterface<ISuspenseCoreUIDataProvider> SourceProvider =
		UIManager->FindProviderByID(Prediction.SourceContainerID);

	TScriptInterface<ISuspenseCoreUIDataProvider> TargetProvider =
		UIManager->FindProviderByID(Prediction.TargetContainerID);

	// Restore snapshots
	for (const FSuspenseCoreSlotSnapshot& Snapshot : Prediction.AffectedSlotSnapshots)
	{
		// Determine which provider owns this slot
		TScriptInterface<ISuspenseCoreUIDataProvider> Provider;

		// Check if slot is in source range or target range
		// For simplicity, we notify both providers to refresh
		if (SourceProvider.GetInterface())
		{
			SourceProvider->NotifySlotChanged(Snapshot.SlotIndex);
		}

		if (TargetProvider.GetInterface() && Prediction.TargetContainerID != Prediction.SourceContainerID)
		{
			TargetProvider->NotifySlotChanged(Snapshot.SlotIndex);
		}

		UE_LOG(LogOptimisticUI, Verbose, TEXT("ApplyRollback: Restored slot %d"), Snapshot.SlotIndex);
	}

	UE_LOG(LogOptimisticUI, Log, TEXT("ApplyRollback: Completed rollback for prediction %d"), Prediction.PredictionKey);
}

void USuspenseCoreOptimisticUIManager::CheckPredictionTimeouts()
{
	TArray<int32> ExpiredKeys;

	for (auto& Pair : PendingPredictions)
	{
		if (Pair.Value.IsExpired())
		{
			ExpiredKeys.Add(Pair.Key);
		}
	}

	for (int32 Key : ExpiredKeys)
	{
		UE_LOG(LogOptimisticUI, Warning, TEXT("CheckPredictionTimeouts: Prediction %d expired - rolling back"), Key);
		RollbackPrediction(Key, NSLOCTEXT("SuspenseCore", "PredictionTimeout", "Operation timed out"));
	}
}

void USuspenseCoreOptimisticUIManager::BroadcastStateChange(int32 PredictionKey, ESuspenseCoreUIPredictionState NewState)
{
	OnPredictionStateChanged.Broadcast(PredictionKey, NewState);

	// Also publish via EventBus for cross-widget awareness
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData EventData;
		EventData.SetInt(FName("PredictionKey"), PredictionKey);
		EventData.SetInt(FName("State"), static_cast<int32>(NewState));

		// Use UIFeedback tag for prediction events
		EventBus->Publish(TAG_SuspenseCore_Event_UIFeedback, EventData);
	}
}

void USuspenseCoreOptimisticUIManager::PublishFeedbackEvent(bool bSuccess, const FText& Message)
{
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData EventData;
		EventData.SetBool(FName("Success"), bSuccess);
		if (!Message.IsEmpty())
		{
			EventData.SetString(FName("Message"), Message.ToString());
		}

		FGameplayTag FeedbackTag = bSuccess
			? TAG_SuspenseCore_Event_UIFeedback_Success
			: TAG_SuspenseCore_Event_UIFeedback_Error;

		EventBus->Publish(FeedbackTag, EventData);
	}
}

USuspenseCoreEventBus* USuspenseCoreOptimisticUIManager::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (USuspenseCoreEventManager* EventManager = GI->GetSubsystem<USuspenseCoreEventManager>())
		{
			CachedEventBus = EventManager->GetEventBus();
			return CachedEventBus.Get();
		}
	}

	return nullptr;
}
