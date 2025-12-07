// SuspenseCoreEquipmentNetworkDispatcher.h
// Copyright Suspense Team. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Delegates/Delegate.h"
#include "Interfaces/Equipment/ISuspenseCoreNetworkDispatcher.h"
#include "Interfaces/Equipment/ISuspenseCoreEquipmentOperations.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "Types/Network/SuspenseCoreNetworkTypes.h"
#include "SuspenseCoreEquipmentNetworkDispatcher.generated.h"

class USuspenseCoreEquipmentNetworkService;

// -----------------------------
// Локальные net-DTO (без TMap)
// -----------------------------

/** Облегчённая версия ItemInstance для RPC (без RuntimeProperties) */
USTRUCT()
struct FSuspenseCoreInventoryItemInstanceNet
{
	GENERATED_BODY()

	UPROPERTY() FName  ItemID = NAME_None;
	UPROPERTY() FGuid  InstanceID;
	UPROPERTY() int32  Quantity = 1;
	UPROPERTY() int32  AnchorIndex = INDEX_NONE;
	UPROPERTY() bool   bIsRotated = false;
	UPROPERTY() float  LastUsedTime = 0.f;
};

/** Облегчённая операция экипировки для RPC */
USTRUCT()
struct FSuspenseCoreEquipmentOperationNet
{
	GENERATED_BODY()

	UPROPERTY() EEquipmentOperationType OperationType = EEquipmentOperationType::None;
	UPROPERTY() int32 SourceSlotIndex = INDEX_NONE;
	UPROPERTY() int32 TargetSlotIndex = INDEX_NONE;
	UPROPERTY() FSuspenseCoreInventoryItemInstanceNet ItemInstance;
	UPROPERTY() ENetworkOperationPriority Priority = ENetworkOperationPriority::Normal;
};

/** Облегчённый запрос для RPC */
USTRUCT()
struct FSuspenseCoreNetworkOperationRequestNet
{
	GENERATED_BODY()

	UPROPERTY() FGuid RequestId;
	UPROPERTY() FSuspenseCoreEquipmentOperationNet Operation;
	UPROPERTY() float Timestamp = 0.f;
};

// -----------------------------
// Очередь/батчи/статистика
// -----------------------------

USTRUCT()
struct FSuspenseCoreOperationQueueEntry
{
	GENERATED_BODY()
	UPROPERTY() FNetworkOperationRequest Request;
	UPROPERTY() int32  RetryCount = 0;
	UPROPERTY() float  QueueTime = 0.0f;
	UPROPERTY() float  LastAttemptTime = 0.0f;
	UPROPERTY() bool   bInProgress = false;

	FDelegateHandle   TimeoutHandle;

	UPROPERTY() bool   bSecurityValidated = false;
	UPROPERTY() float  BackoffMultiplier  = 1.0f;
	UPROPERTY() uint64 RequestHash        = 0;
};

USTRUCT()
struct FSuspenseCoreIdempotencyEntry
{
	GENERATED_BODY()
	UPROPERTY() FGuid                     RequestId;
	UPROPERTY() uint64                    RequestHash = 0;
	UPROPERTY() FEquipmentOperationResult CachedResult;
	UPROPERTY() float                     Timestamp   = 0.0f;
	UPROPERTY() bool                      bProcessed  = false;
};

USTRUCT()
struct FSuspenseCoreOperationBatch
{
	GENERATED_BODY()
	UPROPERTY() FGuid                          BatchId;
	UPROPERTY() TArray<FNetworkOperationRequest> Operations;
	UPROPERTY() float                          CreationTime = 0.0f;
	UPROPERTY() bool                           bSent        = false;
};

USTRUCT(BlueprintType)
struct FSuspenseCoreNetworkDispatcherStats
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32  TotalSent = 0;
	UPROPERTY(BlueprintReadOnly) int32  TotalReceived = 0;
	UPROPERTY(BlueprintReadOnly) int32  TotalFailed = 0;
	UPROPERTY(BlueprintReadOnly) int32  TotalRetries = 0;
	UPROPERTY(BlueprintReadOnly) int32  CurrentQueueSize = 0;
	UPROPERTY(BlueprintReadOnly) float  AverageResponseTime = 0.0f;
	UPROPERTY(BlueprintReadOnly) int32  SecurityRejects = 0;
	UPROPERTY(BlueprintReadOnly) int32  IdempotentHits = 0;
};

UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentNetworkDispatcher
	: public UActorComponent
	, public ISuspenseCoreNetworkDispatcher
{
	GENERATED_BODY()
public:
	USuspenseCoreEquipmentNetworkDispatcher();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ISuspenseCoreNetworkDispatcher
	virtual FGuid SendOperationToServer(const FNetworkOperationRequest& Request) override;
	virtual void SendOperationToClients(const FNetworkOperationRequest& Request, const TArray<APlayerController*>& TargetClients) override;
	virtual void HandleServerResponse(const FNetworkOperationResponse& Response) override;
	virtual FGuid BatchOperations(const TArray<FNetworkOperationRequest>& Operations) override;
	virtual bool CancelOperation(const FGuid& RequestId) override;
	virtual bool RetryOperation(const FGuid& RequestId) override;
	virtual TArray<FNetworkOperationRequest> GetPendingOperations() const override;
	virtual void FlushPendingOperations(bool bForce=false) override;
	virtual void SetOperationTimeout(float Seconds) override;
	virtual FString GetNetworkStatistics() const override;
	virtual bool IsOperationPending(const FGuid& RequestId) const override;

	// Wiring / Config
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Network|Security")
	void SetSecurityService(USuspenseCoreEquipmentNetworkService* InSecurityService);
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Network|Security")
	void SetSecurityEnabled(bool bEnabled){ bSecurityEnabled = bEnabled; }

	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Network")
	void SetOperationExecutor(TScriptInterface<ISuspenseCoreEquipmentOperations> InExecutor);
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Network")
	void ConfigureRetryPolicy(int32 MaxRetries, float RetryDelay, float BackoffFactor = 2.0f, float MaxJitter = 0.5f);
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Network")
	void ConfigureBatching(int32 BatchSize, float BatchInterval);
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Network")
	void ConfigureIdempotency(int32 CacheSize, float EntryLifetime);
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Network")
	FSuspenseCoreNetworkDispatcherStats GetStats() const { return Statistics; }

	// Delegates
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnOperationSuccess, const FGuid&, const FEquipmentOperationResult&);
	FOnOperationSuccess OnOperationSuccess;
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnOperationFailure, const FGuid&, const FText&);
	FOnOperationFailure OnOperationFailure;
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnOperationTimeout, const FGuid&);
	FOnOperationTimeout OnOperationTimeout;
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnServerResponse, const FGuid&, const FEquipmentOperationResult&);
	FOnServerResponse OnServerResponse;

protected:
	// -----------------------------
	// RPC (исправлено):
	// 1) НЕ передаём FNetworkOperationRequest (внутри TMap) в RPC.
	// 2) Все FString/TArray — по const&.
	// -----------------------------
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerExecuteOperation(const FSuspenseCoreNetworkOperationRequestNet& RequestNet);
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerExecuteBatch(const FGuid& BatchId, const TArray<FSuspenseCoreNetworkOperationRequestNet>& RequestsNet);
	UFUNCTION(Server, Unreliable, WithValidation)
	void ServerExecuteLowPriority(const FSuspenseCoreNetworkOperationRequestNet& RequestNet);

	UFUNCTION(Client, Reliable)
	void ClientReceiveResponse(const FGuid& OperationId, bool bSuccess, const FString& ErrorMessage, const FString& ResultData);
	UFUNCTION(Client, Reliable)
	void ClientReceiveBatchResponse(const FGuid& BatchId, const TArray<FGuid>& OperationIds, const TArray<bool>& Results);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastOperationResult(const FGuid& OperationId, bool bSuccess);
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastLowPriorityResult(const FGuid& OperationId, bool bSuccess);

private:
	// Конвертация DTO <-> доменная модель
	static FSuspenseCoreInventoryItemInstanceNet   ToNet(const FSuspenseCoreInventoryItemInstance& In);
	static FSuspenseCoreInventoryItemInstance      FromNet(const FSuspenseCoreInventoryItemInstanceNet& In);
	static FSuspenseCoreNetworkOperationRequestNet ToNet(const FNetworkOperationRequest& In);
	static FNetworkOperationRequest    FromNet(const FSuspenseCoreNetworkOperationRequestNet& In);

	// Security / Idempotency / Hashing
	bool   ValidateRequestSecurity(const FNetworkOperationRequest& Request);
	bool   VerifyServerSecurity(const FNetworkOperationRequest& Request, APlayerController* SendingPlayer);
	bool   CheckIdempotency(const FNetworkOperationRequest& Request, FEquipmentOperationResult& OutCachedResult);
	void   StoreIdempotentResult(const FNetworkOperationRequest& Request, const FEquipmentOperationResult& Result);
	uint64 CalculateRequestHash(const FNetworkOperationRequest& Request) const;
	void   CleanIdempotencyCache();

	// Queue / Sending
	void   ProcessQueue();
	bool   SendOperation(FOperationQueueEntry& Entry);
	float  CalculateRetryDelay(int32 RetryCount) const;
	void   SendWithPriority(ENetworkOperationPriority Priority, const FNetworkOperationRequest& Request);
	void   HandleTimeout(const FGuid& OperationId);
	void   UpdateResponseTimeStats(float ResponseTime);
	bool   ShouldBatchOperation(ENetworkOperationPriority Priority) const;
	FSuspenseCoreOperationQueueEntry* FindQueueEntry(const FGuid& OperationId);

	// Result (строковый формат оставил как был в вашем коде)
	FString SerializeResult(const FEquipmentOperationResult& Result) const;
	bool    DeserializeResult(const FString& Data, FEquipmentOperationResult& OutResult) const;

private:
	// Config
	UPROPERTY(EditDefaultsOnly, Category="Network|Config") int32 MaxQueueSize = 100;
	UPROPERTY(EditDefaultsOnly, Category="Network|Config") float DefaultTimeout = 5.0f;
	UPROPERTY(EditDefaultsOnly, Category="Network|Config") int32 MaxRetryAttempts = 3;
	UPROPERTY(EditDefaultsOnly, Category="Network|Config") float BaseRetryDelay = 0.5f;
	UPROPERTY(EditDefaultsOnly, Category="Network|Config") float BackoffFactor   = 2.0f;
	UPROPERTY(EditDefaultsOnly, Category="Network|Config") float MaxJitter      = 0.5f;
	UPROPERTY(EditDefaultsOnly, Category="Network|Config") int32 MaxBatchSize   = 10;
	UPROPERTY(EditDefaultsOnly, Category="Network|Config") float BatchWaitTime  = 0.1f;
	UPROPERTY(EditDefaultsOnly, Category="Network|Config") int32 MaxIdempotencyCacheSize = 100;
	UPROPERTY(EditDefaultsOnly, Category="Network|Config") float IdempotencyLifetime     = 60.0f;

	// Wiring
	UPROPERTY() USuspenseCoreEquipmentNetworkService* SecurityService = nullptr;
	UPROPERTY() TScriptInterface<ISuspenseCoreEquipmentOperations> OperationExecutor;

	// State
	UPROPERTY() TArray<FSuspenseCoreOperationQueueEntry> OperationQueue;
	UPROPERTY() TArray<FSuspenseCoreOperationBatch>      ActiveBatches;
	UPROPERTY() TArray<FSuspenseCoreIdempotencyEntry>    IdempotencyCache;

	TMap<FGuid, FDelegateHandle> ResponseHandlers;

	bool  bSecurityEnabled = true;
	float CurrentTimeout   = 0.0f;
	float LastProcessTime  = 0.0f;
	float LastBatchTime    = 0.0f;
	float LastIdempotencyCleanup = 0.0f;

	UPROPERTY() FSuspenseCoreNetworkDispatcherStats Statistics;
	TArray<float> ResponseTimeSamples;
	static constexpr int32 MaxResponseSamples = 100;

	mutable FCriticalSection QueueLock;
	mutable FCriticalSection StatsLock;
	mutable FCriticalSection IdempotencyLock;
};
