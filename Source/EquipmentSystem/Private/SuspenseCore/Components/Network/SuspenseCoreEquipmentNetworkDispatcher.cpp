// MedComEquipmentNetworkDispatcher.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/Network/SuspenseCoreEquipmentNetworkDispatcher.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentNetworkService.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Misc/SecureHash.h"

// ----------------------------------------------------
// Helpers: DTO <-> Domain (локальные, без сторонних зависимостей)
// ----------------------------------------------------
FSuspenseCoreInventoryItemInstanceNet USuspenseCoreEquipmentNetworkDispatcher::ToNet(const FSuspenseCoreInventoryItemInstance& In)
{
	FSuspenseCoreInventoryItemInstanceNet Out;
	Out.ItemID       = In.ItemID;
	Out.InstanceID   = In.InstanceID;
	Out.Quantity     = In.Quantity;
	Out.AnchorIndex  = In.AnchorIndex;
	Out.bIsRotated   = In.bIsRotated;
	Out.LastUsedTime = In.LastUsedTime;
	return Out;
}

FSuspenseCoreInventoryItemInstance USuspenseCoreEquipmentNetworkDispatcher::FromNet(const FSuspenseCoreInventoryItemInstanceNet& In)
{
	FSuspenseCoreInventoryItemInstance Out;
	Out.ItemID       = In.ItemID;
	Out.InstanceID   = In.InstanceID;
	Out.Quantity     = In.Quantity;
	Out.AnchorIndex  = In.AnchorIndex;
	Out.bIsRotated   = In.bIsRotated;
	Out.LastUsedTime = In.LastUsedTime;
	// RuntimeProperties умышленно не передаём по сети через RPC
	return Out;
}

FNetworkOperationRequestNet USuspenseCoreEquipmentNetworkDispatcher::ToNet(const FNetworkOperationRequest& In)
{
	FNetworkOperationRequestNet Out;
	Out.RequestId                 = In.RequestId;
	Out.Timestamp                 = In.Timestamp;
	Out.Operation.OperationType   = In.Operation.OperationType;
	Out.Operation.SourceSlotIndex = In.Operation.SourceSlotIndex;
	Out.Operation.TargetSlotIndex = In.Operation.TargetSlotIndex;
	Out.Operation.ItemInstance    = ToNet(In.Operation.ItemInstance);
	Out.Operation.Priority        = In.Priority;
	return Out;
}

FNetworkOperationRequest USuspenseCoreEquipmentNetworkDispatcher::FromNet(const FNetworkOperationRequestNet& In)
{
	FNetworkOperationRequest Out; // дефолтная инициализация
	Out.RequestId  = In.RequestId;
	Out.Timestamp  = In.Timestamp;
	Out.Priority   = In.Operation.Priority;

	Out.Operation.OperationType    = In.Operation.OperationType;
	Out.Operation.SourceSlotIndex  = In.Operation.SourceSlotIndex;
	Out.Operation.TargetSlotIndex  = In.Operation.TargetSlotIndex;
	Out.Operation.ItemInstance     = FromNet(In.Operation.ItemInstance);
	return Out;
}

// ----------------------------------------------------

USuspenseCoreEquipmentNetworkDispatcher::USuspenseCoreEquipmentNetworkDispatcher()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.05f;
	SetIsReplicatedByDefault(true);
	CurrentTimeout = DefaultTimeout;
}

void USuspenseCoreEquipmentNetworkDispatcher::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogEquipmentNetwork, Log, TEXT("NetworkDispatcher: Initialized for %s with role %s"),
		*GetNameSafe(GetOwner()),
		*UEnum::GetValueAsString(GetOwnerRole()));
}

void USuspenseCoreEquipmentNetworkDispatcher::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	{
		FScopeLock Lock(&QueueLock);
		for (const FOperationQueueEntry& Entry : OperationQueue)
		{
			OnOperationTimeout.Broadcast(Entry.Request.RequestId);
		}
		OperationQueue.Empty();
		ActiveBatches.Empty();
	}
	{
		FScopeLock Lock(&IdempotencyLock);
		IdempotencyCache.Empty();
	}
	ResponseHandlers.Empty();

	Super::EndPlay(EndPlayReason);
}

void USuspenseCoreEquipmentNetworkDispatcher::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetWorld())
	{
		const float CurrentTime = GetWorld()->GetTimeSeconds();

		if (CurrentTime - LastProcessTime >= 0.05f)
		{
			ProcessQueue();
			LastProcessTime = CurrentTime;
		}

		if (CurrentTime - LastIdempotencyCleanup >= 10.0f)
		{
			CleanIdempotencyCache();
			LastIdempotencyCleanup = CurrentTime;
		}

		FScopeLock Lock(&QueueLock);
		for (FOperationQueueEntry& Entry : OperationQueue)
		{
			if (Entry.bInProgress && CurrentTime - Entry.LastAttemptTime > CurrentTimeout)
			{
				HandleTimeout(Entry.Request.RequestId);
			}
		}
	}
}

// ============================
// ISuspenseCoreNetworkDispatcher
// ============================

FGuid USuspenseCoreEquipmentNetworkDispatcher::SendOperationToServer(const FNetworkOperationRequest& Request)
{
	if (bSecurityEnabled && !ValidateRequestSecurity(Request))
	{
		Statistics.SecurityRejects++;
		UE_LOG(LogEquipmentNetwork, Warning, TEXT("SendOperationToServer: Security validation failed"));
		return FGuid();
	}

	if (Request.Operation.OperationType == EEquipmentOperationType::None)
	{
		UE_LOG(LogEquipmentNetwork, Warning, TEXT("SendOperationToServer: Invalid operation type"));
		return FGuid();
	}

	{
		FScopeLock Lock(&QueueLock);

		if (OperationQueue.Num() >= MaxQueueSize)
		{
			UE_LOG(LogEquipmentNetwork, Warning, TEXT("SendOperationToServer: Queue is full"));
			return FGuid();
		}

		FOperationQueueEntry Entry;
		Entry.Request     = Request;
		Entry.QueueTime   = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		Entry.RequestHash = CalculateRequestHash(Request);

		OperationQueue.Add(Entry);
		Statistics.CurrentQueueSize = OperationQueue.Num();
	}

	return Request.RequestId;
}

void USuspenseCoreEquipmentNetworkDispatcher::SendOperationToClients(const FNetworkOperationRequest& /*Request*/, const TArray<APlayerController*>& /*TargetClients*/)
{
	UE_LOG(LogEquipmentNetwork, Verbose, TEXT("SendOperationToClients: Not implemented at dispatcher level"));
}

void USuspenseCoreEquipmentNetworkDispatcher::HandleServerResponse(const FNetworkOperationResponse& Response)
{
	Statistics.TotalReceived++;
	if (Response.Latency > 0.0f) { UpdateResponseTimeStats(Response.Latency); }

	FOperationQueueEntry* Entry = FindQueueEntry(Response.RequestId);
	if (!Entry)
	{
		OnServerResponse.Broadcast(Response.RequestId, Response.Result);
		return;
	}

	Entry->bInProgress = false;

	if (Response.bSuccess)
	{
		StoreIdempotentResult(Entry->Request, Response.Result);
		OnOperationSuccess.Broadcast(Response.RequestId, Response.Result);
		OnServerResponse.Broadcast(Response.RequestId, Response.Result);

		FScopeLock Lock(&QueueLock);
		OperationQueue.RemoveAll([&](const FOperationQueueEntry& E){ return E.Request.RequestId == Response.RequestId; });
		Statistics.CurrentQueueSize = OperationQueue.Num();
	}
	else
	{
		if (Entry->RetryCount < MaxRetryAttempts)
		{
			Entry->RetryCount++;
			Statistics.TotalRetries++;
			Entry->bInProgress    = false;
			Entry->LastAttemptTime = 0.0f;

			// Перезапуск на следующий тик
			GetWorld()->GetTimerManager().SetTimerForNextTick([this, ReqId = Response.RequestId]()
			{
				RetryOperation(ReqId);
			});
		}
		else
		{
			OnOperationFailure.Broadcast(Response.RequestId, Response.Result.ErrorMessage);
			OnServerResponse.Broadcast(Response.RequestId, FEquipmentOperationResult::CreateFailure(
				Response.RequestId, Response.Result.ErrorMessage, Response.Result.FailureType));

			FScopeLock Lock(&QueueLock);
			OperationQueue.RemoveAll([&](const FOperationQueueEntry& E){ return E.Request.RequestId == Response.RequestId; });
			Statistics.CurrentQueueSize = OperationQueue.Num();
		}
	}
}

FGuid USuspenseCoreEquipmentNetworkDispatcher::BatchOperations(const TArray<FNetworkOperationRequest>& Operations)
{
	if (Operations.Num() == 0) { return FGuid(); }

	const FGuid BatchId = FGuid::NewGuid();
	{
		FScopeLock Lock(&QueueLock);
		FOperationBatch Batch;
		Batch.BatchId     = BatchId;
		Batch.CreationTime= GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

		for (const FNetworkOperationRequest& R : Operations)
		{
			if (bSecurityEnabled && !ValidateRequestSecurity(R))
			{
				Statistics.SecurityRejects++;
				UE_LOG(LogEquipmentNetwork, Warning, TEXT("BatchOperations: security failed for %s"), *R.RequestId.ToString());
				continue;
			}
			if (OperationQueue.Num() >= MaxQueueSize)
			{
				UE_LOG(LogEquipmentNetwork, Warning, TEXT("BatchOperations: queue full"));
				break;
			}

			FOperationQueueEntry Entry;
			Entry.Request     = R;
			Entry.QueueTime   = Batch.CreationTime;
			Entry.RequestHash = CalculateRequestHash(R);

			OperationQueue.Add(Entry);
			Batch.Operations.Add(R);
		}

		if (Batch.Operations.Num() > 0) { ActiveBatches.Add(Batch); }
		Statistics.CurrentQueueSize = OperationQueue.Num();
	}
	return BatchId;
}

bool USuspenseCoreEquipmentNetworkDispatcher::CancelOperation(const FGuid& RequestId)
{
	FScopeLock Lock(&QueueLock);
	const int32 Removed = OperationQueue.RemoveAll([&](const FOperationQueueEntry& E){ return E.Request.RequestId == RequestId && !E.bInProgress; });
	if (Removed > 0)
	{
		Statistics.CurrentQueueSize = OperationQueue.Num();
		return true;
	}
	return false;
}

bool USuspenseCoreEquipmentNetworkDispatcher::RetryOperation(const FGuid& RequestId)
{
	FOperationQueueEntry* Entry = FindQueueEntry(RequestId);
	if (!Entry) { return false; }
	if (Entry->RetryCount >= MaxRetryAttempts) { return false; }
	Entry->RetryCount++; Entry->bInProgress = false; Entry->LastAttemptTime = 0.0f;
	return true;
}

TArray<FNetworkOperationRequest> USuspenseCoreEquipmentNetworkDispatcher::GetPendingOperations() const
{
	FScopeLock Lock(&QueueLock);
	TArray<FNetworkOperationRequest> Out;
	Out.Reserve(OperationQueue.Num());
	for (const FOperationQueueEntry& E : OperationQueue) { Out.Add(E.Request); }
	return Out;
}

void USuspenseCoreEquipmentNetworkDispatcher::FlushPendingOperations(bool /*bForce*/)
{
	ProcessQueue();
}

void USuspenseCoreEquipmentNetworkDispatcher::SetOperationTimeout(float Seconds)
{
	CurrentTimeout = FMath::Max(0.1f, Seconds);
}

FString USuspenseCoreEquipmentNetworkDispatcher::GetNetworkStatistics() const
{
	FScopeLock Lock(&StatsLock);
	return FString::Printf(TEXT("Sent=%d Received=%d Failed=%d Retries=%d AvgRT=%.2fms Queue=%d SecurityRejects=%d IdemHits=%d"),
		Statistics.TotalSent,
		Statistics.TotalReceived,
		Statistics.TotalFailed,
		Statistics.TotalRetries,
		Statistics.AverageResponseTime * 1000.0f,
		Statistics.CurrentQueueSize,
		Statistics.SecurityRejects,
		Statistics.IdempotentHits);
}

bool USuspenseCoreEquipmentNetworkDispatcher::IsOperationPending(const FGuid& RequestId) const
{
	FScopeLock Lock(&QueueLock);
	return OperationQueue.ContainsByPredicate([&](const FOperationQueueEntry& E){ return E.Request.RequestId == RequestId; });
}

// ============================
// Security / Wiring
// ============================

void USuspenseCoreEquipmentNetworkDispatcher::SetSecurityService(USuspenseCoreEquipmentNetworkService* InSecurityService)
{
	SecurityService = InSecurityService;
}

void USuspenseCoreEquipmentNetworkDispatcher::SetOperationExecutor(TScriptInterface<ISuspenseCoreEquipmentOperations> InExecutor)
{
	OperationExecutor = InExecutor;
}

void USuspenseCoreEquipmentNetworkDispatcher::ConfigureRetryPolicy(int32 MaxRetries, float RetryDelay, float InBackoffFactor, float InMaxJitter)
{
	MaxRetryAttempts = FMath::Max(0, MaxRetries);
	BaseRetryDelay   = FMath::Max(0.0f, RetryDelay);
	BackoffFactor    = FMath::Max(1.0f, InBackoffFactor);
	MaxJitter        = FMath::Max(0.0f, InMaxJitter);
}

void USuspenseCoreEquipmentNetworkDispatcher::ConfigureBatching(int32 BatchSize, float BatchInterval)
{
	MaxBatchSize  = FMath::Max(1, BatchSize);
	BatchWaitTime = FMath::Max(0.0f, BatchInterval);
}

void USuspenseCoreEquipmentNetworkDispatcher::ConfigureIdempotency(int32 CacheSize, float EntryLifetime)
{
	MaxIdempotencyCacheSize = FMath::Max(1, CacheSize);
	IdempotencyLifetime     = FMath::Max(1.0f, EntryLifetime);
}

bool USuspenseCoreEquipmentNetworkDispatcher::ValidateRequestSecurity(const FNetworkOperationRequest& Request)
{
	if (!bSecurityEnabled || !SecurityService) { return true; }

	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC && GetOwner()) { PC = Cast<APlayerController>(GetOwner()->GetInstigatorController()); }
	if (!PC)
	{
		UE_LOG(LogEquipmentNetwork, Warning, TEXT("ValidateRequestSecurity: No PlayerController found"));
		return false;
	}

	return SecurityService->ReceiveEquipmentOperation(Request, PC);
}

bool USuspenseCoreEquipmentNetworkDispatcher::VerifyServerSecurity(const FNetworkOperationRequest& Request, APlayerController* SendingPlayer)
{
	if (!SecurityService) { return true; }
	return SecurityService->ReceiveEquipmentOperation(Request, SendingPlayer);
}

// ============================
// Internal
// ============================

void USuspenseCoreEquipmentNetworkDispatcher::ProcessQueue()
{
	TArray<FOperationQueueEntry*> Pending;
	{
		FScopeLock Lock(&QueueLock);
		for (FOperationQueueEntry& Entry : OperationQueue)
		{
			if (!Entry.bInProgress)
			{
				Pending.Add(&Entry);
			}
		}
	}

	for (FOperationQueueEntry* EntryPtr : Pending)
	{
		if (EntryPtr) { SendOperation(*EntryPtr); }
	}

	{
		FScopeLock Lock(&QueueLock);
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

		for (FOperationBatch& Batch : ActiveBatches)
		{
			if (!Batch.bSent && Now - Batch.CreationTime >= BatchWaitTime)
			{
				// Security check
				bool bAllValid = true;
				if (bSecurityEnabled)
				{
					for (const FNetworkOperationRequest& R : Batch.Operations)
					{
						if (!ValidateRequestSecurity(R))
						{
							bAllValid = false;
							Statistics.SecurityRejects++;
							UE_LOG(LogEquipmentNetwork, Warning, TEXT("ProcessQueue: Security validation failed for batched request %s"), *R.RequestId.ToString());
						}
					}
				}
				if (!bAllValid) { continue; }

				// Сбор net-DTO и отправка
				TArray<FNetworkOperationRequestNet> NetList;
				NetList.Reserve(Batch.Operations.Num());
				for (const FNetworkOperationRequest& R : Batch.Operations)
				{
					NetList.Add(ToNet(R));
				}

				ServerExecuteBatch(Batch.BatchId, NetList);
				Batch.bSent = true;
			}
		}
		ActiveBatches.RemoveAll([](const FOperationBatch& B){ return B.bSent; });
	}
}

bool USuspenseCoreEquipmentNetworkDispatcher::SendOperation(FOperationQueueEntry& Entry)
{
	if (Entry.bInProgress) { return false; }

	if (!Entry.bSecurityValidated && bSecurityEnabled)
	{
		if (!ValidateRequestSecurity(Entry.Request))
		{
			Statistics.SecurityRejects++;
			OnOperationFailure.Broadcast(Entry.Request.RequestId, NSLOCTEXT("EquipmentNetwork", "SecurityFail", "Security validation failed"));
			return false;
		}
		Entry.bSecurityValidated = true;
	}

	Entry.bInProgress     = true;
	Entry.LastAttemptTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	// Idempotency (клиентская сторона)
	FEquipmentOperationResult Cached;
	if (CheckIdempotency(Entry.Request, Cached))
	{
		Statistics.IdempotentHits++;
		OnOperationSuccess.Broadcast(Entry.Request.RequestId, Cached);
		OnServerResponse.Broadcast(Entry.Request.RequestId, Cached);

		FScopeLock Lock(&QueueLock);
		OperationQueue.RemoveAll([&](const FOperationQueueEntry& E){ return E.Request.RequestId == Entry.Request.RequestId; });
		Statistics.CurrentQueueSize = OperationQueue.Num();
		return true;
	}

	SendWithPriority(Entry.Request.Priority, Entry.Request);
	Statistics.TotalSent++;
	return true;
}

float USuspenseCoreEquipmentNetworkDispatcher::CalculateRetryDelay(int32 RetryCount) const
{
	const float Base   = BaseRetryDelay * FMath::Pow(BackoffFactor, FMath::Clamp(RetryCount - 1, 0, 10));
	const float Jitter = FMath::FRandRange(0.0f, MaxJitter);
	return Base + Jitter;
}

void USuspenseCoreEquipmentNetworkDispatcher::SendWithPriority(ENetworkOperationPriority Priority, const FNetworkOperationRequest& Request)
{
	switch (Priority)
	{
	case ENetworkOperationPriority::High:
	case ENetworkOperationPriority::Critical:
		ServerExecuteOperation(ToNet(Request));
		break;

	case ENetworkOperationPriority::Normal:
		if (ShouldBatchOperation(Priority))
		{
			FScopeLock Lock(&QueueLock);
			FOperationBatch* Existing = ActiveBatches.FindByPredicate([](const FOperationBatch& B){ return !B.bSent; });
			if (!Existing)
			{
				FOperationBatch NewBatch;
				NewBatch.BatchId      = FGuid::NewGuid();
				NewBatch.CreationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
				ActiveBatches.Add(NewBatch);
				Existing = &ActiveBatches.Last();
			}
			Existing->Operations.Add(Request);
			if (Existing->Operations.Num() >= MaxBatchSize)
			{
				TArray<FNetworkOperationRequestNet> NetList;
				NetList.Reserve(Existing->Operations.Num());
				for (const FNetworkOperationRequest& R : Existing->Operations) { NetList.Add(ToNet(R)); }

				ServerExecuteBatch(Existing->BatchId, NetList);
				Existing->bSent = true;
			}
		}
		else
		{
			ServerExecuteOperation(ToNet(Request));
		}
		break;

	case ENetworkOperationPriority::Low:
	default:
		ServerExecuteLowPriority(ToNet(Request));
		break;
	}
}

void USuspenseCoreEquipmentNetworkDispatcher::HandleTimeout(const FGuid& OperationId)
{
	FScopeLock Lock(&QueueLock);
	for (FOperationQueueEntry& Entry : OperationQueue)
	{
		if (Entry.Request.RequestId == OperationId && Entry.bInProgress)
		{
			if (Entry.RetryCount < MaxRetryAttempts)
			{
				Entry.RetryCount++;
				Entry.bInProgress     = false;
				Entry.LastAttemptTime = 0.0f;
				Statistics.TotalRetries++;
			}
			else
			{
				Statistics.TotalFailed++;
				OnOperationTimeout.Broadcast(OperationId);
				OnOperationFailure.Broadcast(OperationId, NSLOCTEXT("EquipmentNetwork", "Timeout", "Operation timed out"));

				OperationQueue.RemoveAll([&](const FOperationQueueEntry& E){ return E.Request.RequestId == OperationId; });
				Statistics.CurrentQueueSize = OperationQueue.Num();
			}
			break;
		}
	}
}

void USuspenseCoreEquipmentNetworkDispatcher::UpdateResponseTimeStats(float ResponseTime)
{
	FScopeLock Lock(&StatsLock);
	ResponseTimeSamples.Add(ResponseTime);
	if (ResponseTimeSamples.Num() > MaxResponseSamples)
	{
		ResponseTimeSamples.RemoveAt(0);
	}
	float Sum = 0.f;
	for (float V : ResponseTimeSamples) { Sum += V; }
	Statistics.AverageResponseTime = ResponseTimeSamples.Num() ? Sum / (float)ResponseTimeSamples.Num() : 0.f;
}

bool USuspenseCoreEquipmentNetworkDispatcher::ShouldBatchOperation(ENetworkOperationPriority Priority) const
{
	return Priority == ENetworkOperationPriority::Normal;
}

FOperationQueueEntry* USuspenseCoreEquipmentNetworkDispatcher::FindQueueEntry(const FGuid& OperationId)
{
	FScopeLock Lock(&QueueLock);
	for (FOperationQueueEntry& E : OperationQueue)
	{
		if (E.Request.RequestId == OperationId) { return &E; }
	}
	return nullptr;
}

FString USuspenseCoreEquipmentNetworkDispatcher::SerializeResult(const FEquipmentOperationResult& Result) const
{
	const FString Err = Result.ErrorMessage.ToString();
	return FString::Printf(TEXT("%d|%s"), Result.bSuccess ? 1 : 0, *Err);
}

bool USuspenseCoreEquipmentNetworkDispatcher::DeserializeResult(const FString& Data, FEquipmentOperationResult& OutResult) const
{
	int32 SepIdx = INDEX_NONE;
	if (!Data.FindChar(TEXT('|'), SepIdx)) { return false; }
	const FString FlagStr = Data.Left(SepIdx);
	const FString ErrStr  = Data.Mid(SepIdx + 1);
	OutResult.bSuccess    = (FlagStr == TEXT("1"));
	OutResult.ErrorMessage= FText::FromString(ErrStr);
	return true;
}

uint64 USuspenseCoreEquipmentNetworkDispatcher::CalculateRequestHash(const FNetworkOperationRequest& Request) const
{
	FSHA1 Sha;
	Sha.Update((const uint8*)&Request.RequestId, sizeof(FGuid));
	const uint8 OpType = (uint8)Request.Operation.OperationType; Sha.Update(&OpType, 1);
	Sha.Update((const uint8*)&Request.Operation.SourceSlotIndex, sizeof(int32));
	Sha.Update((const uint8*)&Request.Operation.TargetSlotIndex, sizeof(int32));
	const uint32 ItemIdHash     = GetTypeHash(Request.Operation.ItemInstance.ItemID);
	Sha.Update((const uint8*)&ItemIdHash, sizeof(uint32));
	const uint32 InstanceIdHash = GetTypeHash(Request.Operation.ItemInstance.InstanceID);
	Sha.Update((const uint8*)&InstanceIdHash, sizeof(uint32));
	Sha.Update((const uint8*)&Request.Timestamp, sizeof(float));
	Sha.Final();
	uint8 Hash[20]; Sha.GetHash(Hash);
	uint64 Out = 0; FMemory::Memcpy(&Out, Hash, sizeof(uint64));
	return Out;
}

bool USuspenseCoreEquipmentNetworkDispatcher::CheckIdempotency(const FNetworkOperationRequest& Request, FEquipmentOperationResult& OutCachedResult)
{
	const uint64 H = CalculateRequestHash(Request);
	FScopeLock Lock(&IdempotencyLock);
	for (const FIdempotencyEntry& E : IdempotencyCache)
	{
		if (E.RequestId == Request.RequestId || E.RequestHash == H)
		{
			OutCachedResult = E.CachedResult;
			return true;
		}
	}
	return false;
}

void USuspenseCoreEquipmentNetworkDispatcher::StoreIdempotentResult(const FNetworkOperationRequest& Request, const FEquipmentOperationResult& Result)
{
	FIdempotencyEntry NewEntry;
	NewEntry.RequestId   = Request.RequestId;
	NewEntry.RequestHash = CalculateRequestHash(Request);
	NewEntry.CachedResult= Result;
	NewEntry.Timestamp   = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	NewEntry.bProcessed  = true;

	FScopeLock Lock(&IdempotencyLock);
	IdempotencyCache.Add(NewEntry);
	if (IdempotencyCache.Num() > MaxIdempotencyCacheSize)
	{
		IdempotencyCache.RemoveAt(0);
	}
}

void USuspenseCoreEquipmentNetworkDispatcher::CleanIdempotencyCache()
{
	FScopeLock Lock(&IdempotencyLock);
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	IdempotencyCache.RemoveAll([&](const FIdempotencyEntry& E)
	{
		return (Now - E.Timestamp) > IdempotencyLifetime;
	});
}

// ============================
// RPC (исправленные сигнатуры)
// ============================

bool USuspenseCoreEquipmentNetworkDispatcher::ServerExecuteOperation_Validate(const FNetworkOperationRequestNet& RequestNet)
{
	return RequestNet.Operation.OperationType != EEquipmentOperationType::None;
}

void USuspenseCoreEquipmentNetworkDispatcher::ServerExecuteOperation_Implementation(const FNetworkOperationRequestNet& RequestNet)
{
	FNetworkOperationRequest Request = FromNet(RequestNet);

	APlayerController* Sender = Cast<APlayerController>(GetOwner());
	if (!VerifyServerSecurity(Request, Sender))
	{
		Statistics.TotalFailed++;
		OnOperationFailure.Broadcast(Request.RequestId, NSLOCTEXT("EquipmentNetwork","ServerSecurityFail","Server security verification failed"));
		return;
	}

	if (OperationExecutor.GetInterface())
	{
		const FEquipmentOperationResult Result = OperationExecutor->ExecuteOperation(Request.Operation);
		StoreIdempotentResult(Request, Result);
		ClientReceiveResponse(Request.RequestId, Result.bSuccess, Result.ErrorMessage.ToString(), SerializeResult(Result));
		MulticastOperationResult(Request.RequestId, Result.bSuccess);
		if (!Result.bSuccess) { Statistics.TotalFailed++; }
	}
}

bool USuspenseCoreEquipmentNetworkDispatcher::ServerExecuteBatch_Validate(const FGuid& /*BatchId*/, const TArray<FNetworkOperationRequestNet>& RequestsNet)
{
	return RequestsNet.Num() > 0 && RequestsNet.Num() <= MaxBatchSize;
}

void USuspenseCoreEquipmentNetworkDispatcher::ServerExecuteBatch_Implementation(const FGuid& BatchId, const TArray<FNetworkOperationRequestNet>& RequestsNet)
{
	APlayerController* Sender = Cast<APlayerController>(GetOwner());

	TArray<FGuid> OpIds;
	TArray<bool>  Results;
	OpIds.Reserve(RequestsNet.Num());
	Results.Reserve(RequestsNet.Num());

	for (const FNetworkOperationRequestNet& Net : RequestsNet)
	{
		FNetworkOperationRequest Req = FromNet(Net);

		if (!VerifyServerSecurity(Req, Sender))
		{
			Statistics.TotalFailed++;
			OnOperationFailure.Broadcast(Req.RequestId, NSLOCTEXT("EquipmentNetwork","ServerSecurityFailBatch","Server security verification failed (batch)"));
			OpIds.Add(Req.RequestId);
			Results.Add(false);
			continue;
		}

		bool bSuccess = false;
		if (OperationExecutor.GetInterface())
		{
			const FEquipmentOperationResult Res = OperationExecutor->ExecuteOperation(Req.Operation);
			StoreIdempotentResult(Req, Res);
			bSuccess = Res.bSuccess;
		}

		OpIds.Add(Req.RequestId);
		Results.Add(bSuccess);
		MulticastOperationResult(Req.RequestId, bSuccess);
		if (!bSuccess) { Statistics.TotalFailed++; }
	}

	ClientReceiveBatchResponse(BatchId, OpIds, Results);
}

bool USuspenseCoreEquipmentNetworkDispatcher::ServerExecuteLowPriority_Validate(const FNetworkOperationRequestNet& RequestNet)
{
	return RequestNet.Operation.OperationType != EEquipmentOperationType::None;
}

void USuspenseCoreEquipmentNetworkDispatcher::ServerExecuteLowPriority_Implementation(const FNetworkOperationRequestNet& RequestNet)
{
	FNetworkOperationRequest Request = FromNet(RequestNet);

	APlayerController* Sender = Cast<APlayerController>(GetOwner());
	if (!VerifyServerSecurity(Request, Sender))
	{
		Statistics.TotalFailed++;
		OnOperationFailure.Broadcast(Request.RequestId, NSLOCTEXT("EquipmentNetwork","ServerSecurityFailLow","Server security verification failed (low)"));
		return;
	}

	bool bSuccess = false;
	if (OperationExecutor.GetInterface())
	{
		const FEquipmentOperationResult Res = OperationExecutor->ExecuteOperation(Request.Operation);
		StoreIdempotentResult(Request, Res);
		bSuccess = Res.bSuccess;
	}

	ClientReceiveResponse(Request.RequestId, bSuccess, bSuccess ? TEXT("") : TEXT("Operation failed"), FString());
	MulticastLowPriorityResult(Request.RequestId, bSuccess);
	if (!bSuccess) { Statistics.TotalFailed++; }
}

// ============================
// Client RPCs (const& fixed)
// ============================

void USuspenseCoreEquipmentNetworkDispatcher::ClientReceiveResponse_Implementation(
	const FGuid& OperationId, bool bSuccess, const FString& ErrorMessage, const FString& ResultData)
{
	FNetworkOperationResponse Resp;
	Resp.RequestId       = OperationId;
	Resp.bSuccess        = bSuccess;
	Resp.ServerTimestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	Resp.Latency         = 0.0f;

	if (!DeserializeResult(ResultData, Resp.Result))
	{
		Resp.Result.bSuccess    = bSuccess;
		Resp.Result.ErrorMessage= FText::FromString(ErrorMessage);
		Resp.Result.OperationId = OperationId;
	}
	HandleServerResponse(Resp);
}

void USuspenseCoreEquipmentNetworkDispatcher::ClientReceiveBatchResponse_Implementation(
	const FGuid& BatchId, const TArray<FGuid>& OperationIds, const TArray<bool>& Results)
{
	const int32 N = FMath::Min(OperationIds.Num(), Results.Num());
	for (int32 i = 0; i < N; ++i)
	{
		FNetworkOperationResponse Resp;
		Resp.RequestId       = OperationIds[i];
		Resp.bSuccess        = Results[i];
		Resp.ServerTimestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		Resp.Latency         = 0.0f;

		// Минимальный результат для батча: статус + пустая ошибка
		Resp.Result.OperationId = Resp.RequestId;
		Resp.Result.bSuccess    = Resp.bSuccess;
		if (!Resp.bSuccess)
		{
			Resp.Result.ErrorMessage = NSLOCTEXT("EquipmentNetwork", "BatchOpFailed", "Batched operation failed");
		}

		HandleServerResponse(Resp);
	}

	UE_LOG(LogEquipmentNetwork, Verbose, TEXT("ClientReceiveBatchResponse: Batch %s, Ops=%d"),
		*BatchId.ToString(), N);
}

void USuspenseCoreEquipmentNetworkDispatcher::MulticastOperationResult_Implementation(const FGuid& OperationId, bool bSuccess)
{
	// Минимальная реакция на всех клиентах (и на сервере), без дублирования делегатов,
	// т.к. владеющий клиент получает полный результат через ClientReceiveResponse.
	UE_LOG(LogEquipmentNetwork, Verbose,
		TEXT("MulticastOperationResult: OpId=%s, Success=%s"),
		*OperationId.ToString(),
		bSuccess ? TEXT("true") : TEXT("false"));
}

void USuspenseCoreEquipmentNetworkDispatcher::MulticastLowPriorityResult_Implementation(const FGuid& OperationId, bool bSuccess)
{
	UE_LOG(LogEquipmentNetwork, Verbose,
		TEXT("MulticastLowPriorityResult: OpId=%s, Success=%s"),
		*OperationId.ToString(),
		bSuccess ? TEXT("true") : TEXT("false"));
}
