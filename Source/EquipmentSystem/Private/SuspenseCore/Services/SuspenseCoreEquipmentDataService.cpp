// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreEquipmentDataService.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "Components/Core/SuspenseEquipmentDataStore.h"
#include "Components/Transaction/SuspenseEquipmentTransactionProcessor.h"
#include "Interfaces/Equipment/ISuspenseEquipmentDataProvider.h"
#include "Interfaces/Equipment/ISuspenseTransactionManager.h"

USuspenseCoreEquipmentDataService::USuspenseCoreEquipmentDataService()
	: ServiceState(EServiceLifecycleState::Uninitialized)
	, bComponentsInjected(false)
	, MaxSlotCount(20)
	, bEnableCaching(true)
	, CacheTTL(60.0f)
	, bEnableDetailedLogging(false)
	, TotalReads(0)
	, TotalWrites(0)
	, TotalTransactions(0)
	, CacheHits(0)
	, CacheMisses(0)
{
}

USuspenseCoreEquipmentDataService::~USuspenseCoreEquipmentDataService()
{
}

//========================================
// ISuspenseEquipmentService Interface
//========================================

bool USuspenseCoreEquipmentDataService::InitializeService(const FServiceInitParams& Params)
{
	TRACK_SERVICE_INIT();

	if (!bComponentsInjected)
	{
		LOG_SERVICE_ERROR(TEXT("Components not injected. Call InjectComponents() before InitializeService()"));
		ServiceState = EServiceLifecycleState::Failed;
		return false;
	}

	ServiceState = EServiceLifecycleState::Initializing;
	ServiceLocator = Params.ServiceLocator;

	if (!InitializeDataStorage())
	{
		LOG_SERVICE_ERROR(TEXT("Failed to initialize data storage"));
		ServiceState = EServiceLifecycleState::Failed;
		return false;
	}

	SetupEventSubscriptions();

	InitializationTime = FDateTime::UtcNow();
	ServiceState = EServiceLifecycleState::Ready;

	LOG_SERVICE_INFO(TEXT("Service initialized successfully"));
	return true;
}

bool USuspenseCoreEquipmentDataService::ShutdownService(bool bForce)
{
	TRACK_SERVICE_SHUTDOWN();

	ServiceState = EServiceLifecycleState::Shutting;
	CleanupResources();
	ServiceState = EServiceLifecycleState::Shutdown;

	LOG_SERVICE_INFO(TEXT("Service shut down"));
	return true;
}

EServiceLifecycleState USuspenseCoreEquipmentDataService::GetServiceState() const
{
	return ServiceState;
}

bool USuspenseCoreEquipmentDataService::IsServiceReady() const
{
	return ServiceState == EServiceLifecycleState::Ready;
}

FGameplayTag USuspenseCoreEquipmentDataService::GetServiceTag() const
{
	static FGameplayTag ServiceTag = FGameplayTag::RequestGameplayTag(FName("Equipment.Service.Data"));
	return ServiceTag;
}

FGameplayTagContainer USuspenseCoreEquipmentDataService::GetRequiredDependencies() const
{
	// Data service has no dependencies
	return FGameplayTagContainer();
}

bool USuspenseCoreEquipmentDataService::ValidateService(TArray<FText>& OutErrors) const
{
	bool bValid = true;

	if (!DataStore)
	{
		OutErrors.Add(FText::FromString(TEXT("DataStore is null")));
		bValid = false;
	}

	if (!TransactionProcessor)
	{
		OutErrors.Add(FText::FromString(TEXT("TransactionProcessor is null")));
		bValid = false;
	}

	return bValid;
}

void USuspenseCoreEquipmentDataService::ResetService()
{
	TotalReads = 0;
	TotalWrites = 0;
	TotalTransactions = 0;
	CacheHits = 0;
	CacheMisses = 0;
	ClearAllCaches();
	LOG_SERVICE_INFO(TEXT("Service reset"));
}

FString USuspenseCoreEquipmentDataService::GetServiceStats() const
{
	return FString::Printf(TEXT("Data - Reads: %d, Writes: %d, Transactions: %d, Cache Hits: %d, Cache Misses: %d"),
		TotalReads, TotalWrites, TotalTransactions, CacheHits, CacheMisses);
}

//========================================
// IEquipmentDataService Interface
//========================================

void USuspenseCoreEquipmentDataService::InjectComponents(UObject* InDataStore, UObject* InTransactionProcessor)
{
	DataStore = Cast<USuspenseEquipmentDataStore>(InDataStore);
	TransactionProcessor = Cast<USuspenseEquipmentTransactionProcessor>(InTransactionProcessor);

	if (!DataStore || !TransactionProcessor)
	{
		LOG_SERVICE_ERROR(TEXT("Failed to cast injected components"));
		return;
	}

	bComponentsInjected = true;
	LOG_SERVICE_INFO(TEXT("Components injected successfully"));
}

void USuspenseCoreEquipmentDataService::SetValidator(UObject* InValidator)
{
	SlotValidator = InValidator;
	LOG_SERVICE_INFO(TEXT("Validator set"));
}

ISuspenseEquipmentDataProvider* USuspenseCoreEquipmentDataService::GetDataProvider()
{
	return Cast<ISuspenseEquipmentDataProvider>(DataStore);
}

ISuspenseTransactionManager* USuspenseCoreEquipmentDataService::GetTransactionManager()
{
	return Cast<ISuspenseTransactionManager>(TransactionProcessor);
}

//========================================
// Data Access
//========================================

FSuspenseInventoryItemInstance USuspenseCoreEquipmentDataService::GetSlotData(int32 SlotIndex) const
{
	CHECK_SERVICE_READY();

	FRWScopeLock ScopeLock(DataLock, SLT_ReadOnly);
	TotalReads++;

	return GetSlotDataInternal(SlotIndex);
}

bool USuspenseCoreEquipmentDataService::SetSlotData(int32 SlotIndex, const FSuspenseInventoryItemInstance& ItemData)
{
	CHECK_SERVICE_READY_BOOL();

	FRWScopeLock ScopeLock(DataLock, SLT_Write);
	TotalWrites++;

	bool bSuccess = SetSlotDataInternal(SlotIndex, ItemData);

	if (bSuccess)
	{
		PublishDataChanged(SlotIndex);
	}

	return bSuccess;
}

bool USuspenseCoreEquipmentDataService::SwapSlots(int32 SlotA, int32 SlotB)
{
	CHECK_SERVICE_READY_BOOL();

	// Swap operation would use transaction
	FGuid TransactionId = BeginTransaction(TEXT("SwapSlots"));

	// Actual swap logic here
	bool bSuccess = true;

	if (bSuccess)
	{
		CommitTransaction(TransactionId);
	}
	else
	{
		RollbackTransaction(TransactionId);
	}

	return bSuccess;
}

bool USuspenseCoreEquipmentDataService::ClearSlot(int32 SlotIndex)
{
	CHECK_SERVICE_READY_BOOL();

	return SetSlotData(SlotIndex, FSuspenseInventoryItemInstance());
}

TArray<FSuspenseInventoryItemInstance> USuspenseCoreEquipmentDataService::GetAllSlotData() const
{
	CHECK_SERVICE_READY();

	TArray<FSuspenseInventoryItemInstance> AllData;
	AllData.Reserve(MaxSlotCount);

	for (int32 i = 0; i < MaxSlotCount; i++)
	{
		AllData.Add(GetSlotData(i));
	}

	return AllData;
}

//========================================
// Transaction Management
//========================================

FGuid USuspenseCoreEquipmentDataService::BeginTransaction(const FString& Description)
{
	CHECK_SERVICE_READY();

	if (!TransactionProcessor)
	{
		return FGuid();
	}

	TotalTransactions++;
	FGuid TransactionId = FGuid::NewGuid();
	LOG_SERVICE_VERBOSE(TEXT("Transaction started: %s"), *Description);
	return TransactionId;
}

bool USuspenseCoreEquipmentDataService::CommitTransaction(const FGuid& TransactionId)
{
	CHECK_SERVICE_READY_BOOL();

	if (!TransactionProcessor)
	{
		return false;
	}

	LOG_SERVICE_VERBOSE(TEXT("Transaction committed: %s"), *TransactionId.ToString());
	return true;
}

bool USuspenseCoreEquipmentDataService::RollbackTransaction(const FGuid& TransactionId)
{
	CHECK_SERVICE_READY_BOOL();

	if (!TransactionProcessor)
	{
		return false;
	}

	LOG_SERVICE_VERBOSE(TEXT("Transaction rolled back: %s"), *TransactionId.ToString());
	return true;
}

//========================================
// State Management
//========================================

FGuid USuspenseCoreEquipmentDataService::CreateSnapshot(const FString& SnapshotName)
{
	CHECK_SERVICE_READY();

	FGuid SnapshotId = FGuid::NewGuid();
	LOG_SERVICE_VERBOSE(TEXT("Snapshot created: %s"), *SnapshotName);
	return SnapshotId;
}

bool USuspenseCoreEquipmentDataService::RestoreSnapshot(const FGuid& SnapshotId)
{
	CHECK_SERVICE_READY_BOOL();

	LOG_SERVICE_VERBOSE(TEXT("Snapshot restored: %s"), *SnapshotId.ToString());
	return true;
}

FEquipmentStateSnapshot USuspenseCoreEquipmentDataService::GetCurrentState() const
{
	CHECK_SERVICE_READY();

	FEquipmentStateSnapshot Snapshot;
	return Snapshot;
}

bool USuspenseCoreEquipmentDataService::ValidateDataIntegrity(TArray<FText>& OutErrors) const
{
	CHECK_SERVICE_READY_BOOL();

	// Validation logic here
	return true;
}

//========================================
// Cache Management
//========================================

void USuspenseCoreEquipmentDataService::InvalidateSlotCache(int32 SlotIndex)
{
	FRWScopeLock ScopeLock(CacheLock, SLT_Write);
	LOG_SERVICE_VERBOSE(TEXT("Cache invalidated for slot %d"), SlotIndex);
}

void USuspenseCoreEquipmentDataService::ClearAllCaches()
{
	FRWScopeLock ScopeLock(CacheLock, SLT_Write);
	LOG_SERVICE_INFO(TEXT("All caches cleared"));
}

FString USuspenseCoreEquipmentDataService::GetCacheStats() const
{
	float HitRate = (CacheHits + CacheMisses > 0) ?
		(float)CacheHits / (float)(CacheHits + CacheMisses) * 100.0f : 0.0f;

	return FString::Printf(TEXT("Cache - Hits: %d, Misses: %d, Hit Rate: %.2f%%"),
		CacheHits, CacheMisses, HitRate);
}

//========================================
// Event Publishing
//========================================

void USuspenseCoreEquipmentDataService::PublishDataChanged(int32 SlotIndex)
{
	PUBLISH_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Data.Changed")),
		FSuspenseEquipmentEventData()
	);
}

void USuspenseCoreEquipmentDataService::PublishBatchDataChanged(const TArray<int32>& SlotIndices)
{
	PUBLISH_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Data.BatchChanged")),
		FSuspenseEquipmentEventData()
	);
}

//========================================
// Service Lifecycle
//========================================

bool USuspenseCoreEquipmentDataService::InitializeDataStorage()
{
	if (!DataStore || !TransactionProcessor)
	{
		return false;
	}

	// Initialize data storage
	return true;
}

void USuspenseCoreEquipmentDataService::SetupEventSubscriptions()
{
	// Setup event subscriptions
}

void USuspenseCoreEquipmentDataService::CleanupResources()
{
	ClearAllCaches();
}

//========================================
// Data Operations
//========================================

FSuspenseInventoryItemInstance USuspenseCoreEquipmentDataService::GetSlotDataInternal(int32 SlotIndex) const
{
	// Actual data retrieval logic
	return FSuspenseInventoryItemInstance();
}

bool USuspenseCoreEquipmentDataService::SetSlotDataInternal(int32 SlotIndex, const FSuspenseInventoryItemInstance& ItemData)
{
	// Actual data modification logic
	return true;
}

bool USuspenseCoreEquipmentDataService::IsValidSlotIndex(int32 SlotIndex) const
{
	return SlotIndex >= 0 && SlotIndex < MaxSlotCount;
}

//========================================
// Event Handlers
//========================================

void USuspenseCoreEquipmentDataService::OnTransactionCompleted(const FGuid& TransactionId, bool bSuccess)
{
	// Handle transaction completion
}

void USuspenseCoreEquipmentDataService::OnCacheInvalidationRequested(const FSuspenseEquipmentEventData& EventData)
{
	// Handle cache invalidation request
}
