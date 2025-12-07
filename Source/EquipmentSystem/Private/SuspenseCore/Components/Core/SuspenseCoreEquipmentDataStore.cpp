// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Components/Core/SuspenseCoreEquipmentDataStore.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreDataStore, Log, All);

#define DATASTORE_LOG(Verbosity, Format, ...) \
	UE_LOG(LogSuspenseCoreDataStore, Verbosity, TEXT("%s: " Format), *GetNameSafe(this), ##__VA_ARGS__)

USuspenseCoreEquipmentDataStore::USuspenseCoreEquipmentDataStore()
{
	bIsInitialized = false;
	DataVersion = 0;
	TotalReads = 0;
	TotalWrites = 0;
	CacheHits = 0;
	CacheMisses = 0;
	LastCacheCleanupTime = 0.0f;

	// Configuration defaults
	CacheMaxAge = 60.0f; // 60 seconds
	bEnableAutoCacheCleanup = true;
	CacheCleanupInterval = 30.0f; // 30 seconds
}

bool USuspenseCoreEquipmentDataStore::Initialize(USuspenseCoreServiceLocator* InServiceLocator)
{
	if (!InServiceLocator)
	{
		DATASTORE_LOG(Error, TEXT("Initialize: Invalid ServiceLocator"));
		return false;
	}

	ServiceLocator = InServiceLocator;

	// Get EventBus from ServiceLocator
	EventBus = ServiceLocator->GetService<USuspenseCoreEventBus>();
	if (!EventBus.IsValid())
	{
		DATASTORE_LOG(Warning, TEXT("Initialize: EventBus not found in ServiceLocator"));
	}

	bIsInitialized = true;
	DATASTORE_LOG(Log, TEXT("Initialize: Success"));
	return true;
}

void USuspenseCoreEquipmentDataStore::Shutdown()
{
	DATASTORE_LOG(Log, TEXT("Shutdown"));

	FScopeLock Lock(&DataCriticalSection);

	// Clear all data
	StoredItems.Empty();
	CacheEntries.Empty();

	// Cleanup
	ServiceLocator.Reset();
	EventBus.Reset();

	bIsInitialized = false;
}

bool USuspenseCoreEquipmentDataStore::StoreItemData(
	int32 SlotIndex,
	const FSuspenseInventoryItemInstance& ItemInstance)
{
	if (!bIsInitialized)
	{
		DATASTORE_LOG(Error, TEXT("StoreItemData: DataStore not initialized"));
		return false;
	}

	FScopeLock Lock(&DataCriticalSection);

	const bool bAlreadyExists = StoredItems.Contains(SlotIndex);

	// Store item
	StoredItems.Add(SlotIndex, ItemInstance);

	// Update cache
	UpdateCache(SlotIndex, ItemInstance);

	// Increment version
	DataVersion++;
	TotalWrites++;

	DATASTORE_LOG(Verbose, TEXT("StoreItemData: Slot %d"), SlotIndex);

	// Publish data changed event
	PublishDataChanged(SlotIndex, !bAlreadyExists);

	return true;
}

bool USuspenseCoreEquipmentDataStore::RetrieveItemData(
	int32 SlotIndex,
	FSuspenseInventoryItemInstance& OutItem) const
{
	FScopeLock Lock(&DataCriticalSection);

	// Check cache first
	if (IsCacheValid(SlotIndex))
	{
		const FSuspenseCoreEquipmentCacheEntry* CacheEntry = CacheEntries.Find(SlotIndex);
		if (CacheEntry && CacheEntry->bIsValid)
		{
			OutItem = CacheEntry->ItemInstance;
			const_cast<USuspenseCoreEquipmentDataStore*>(this)->CacheHits++;
			const_cast<USuspenseCoreEquipmentDataStore*>(this)->TotalReads++;
			return true;
		}
	}

	// Cache miss - retrieve from storage
	const FSuspenseInventoryItemInstance* Found = StoredItems.Find(SlotIndex);
	if (Found)
	{
		OutItem = *Found;
		const_cast<USuspenseCoreEquipmentDataStore*>(this)->UpdateCache(SlotIndex, *Found);
		const_cast<USuspenseCoreEquipmentDataStore*>(this)->CacheMisses++;
		const_cast<USuspenseCoreEquipmentDataStore*>(this)->TotalReads++;
		return true;
	}

	const_cast<USuspenseCoreEquipmentDataStore*>(this)->CacheMisses++;
	const_cast<USuspenseCoreEquipmentDataStore*>(this)->TotalReads++;
	return false;
}

bool USuspenseCoreEquipmentDataStore::RemoveItemData(int32 SlotIndex)
{
	FScopeLock Lock(&DataCriticalSection);

	if (!StoredItems.Contains(SlotIndex))
	{
		return false;
	}

	StoredItems.Remove(SlotIndex);
	InvalidateCache(SlotIndex);

	DataVersion++;
	TotalWrites++;

	DATASTORE_LOG(Verbose, TEXT("RemoveItemData: Slot %d"), SlotIndex);

	PublishDataChanged(SlotIndex, false);

	return true;
}

bool USuspenseCoreEquipmentDataStore::HasItemData(int32 SlotIndex) const
{
	FScopeLock Lock(&DataCriticalSection);
	return StoredItems.Contains(SlotIndex);
}

TArray<int32> USuspenseCoreEquipmentDataStore::GetStoredSlots() const
{
	FScopeLock Lock(&DataCriticalSection);

	TArray<int32> Slots;
	StoredItems.GetKeys(Slots);
	return Slots;
}

void USuspenseCoreEquipmentDataStore::ClearAllData()
{
	FScopeLock Lock(&DataCriticalSection);

	DATASTORE_LOG(Log, TEXT("ClearAllData: Clearing %d items"), StoredItems.Num());

	StoredItems.Empty();
	CacheEntries.Empty();
	DataVersion++;
}

void USuspenseCoreEquipmentDataStore::UpdateCache(
	int32 SlotIndex,
	const FSuspenseInventoryItemInstance& ItemInstance)
{
	FScopeLock Lock(&DataCriticalSection);

	FSuspenseCoreEquipmentCacheEntry& Entry = GetOrCreateCacheEntry(SlotIndex);
	Entry.ItemInstance = ItemInstance;
	Entry.CacheTime = 0.0f; // TODO: Get actual game time
	Entry.Version = DataVersion;
	Entry.bIsValid = true;

	DATASTORE_LOG(Verbose, TEXT("UpdateCache: Slot %d"), SlotIndex);
}

void USuspenseCoreEquipmentDataStore::InvalidateCache(int32 SlotIndex)
{
	FScopeLock Lock(&DataCriticalSection);

	FSuspenseCoreEquipmentCacheEntry* Entry = CacheEntries.Find(SlotIndex);
	if (Entry)
	{
		Entry->Invalidate();
		DATASTORE_LOG(Verbose, TEXT("InvalidateCache: Slot %d"), SlotIndex);

		PublishCacheInvalidated(SlotIndex);
	}
}

void USuspenseCoreEquipmentDataStore::InvalidateAllCaches()
{
	FScopeLock Lock(&DataCriticalSection);

	DATASTORE_LOG(Log, TEXT("InvalidateAllCaches"));

	for (auto& Pair : CacheEntries)
	{
		Pair.Value.Invalidate();
	}
}

float USuspenseCoreEquipmentDataStore::GetCacheHitRate() const
{
	const int32 TotalCacheAccesses = CacheHits + CacheMisses;
	if (TotalCacheAccesses == 0)
	{
		return 0.0f;
	}

	return static_cast<float>(CacheHits) / static_cast<float>(TotalCacheAccesses);
}

void USuspenseCoreEquipmentDataStore::CleanExpiredCaches()
{
	FScopeLock Lock(&DataCriticalSection);

	const float CurrentTime = 0.0f; // TODO: Get actual game time
	int32 CleanedCount = 0;

	for (auto It = CacheEntries.CreateIterator(); It; ++It)
	{
		if (It->Value.IsExpired(CurrentTime, CacheMaxAge))
		{
			It->Value.Invalidate();
			CleanedCount++;
		}
	}

	LastCacheCleanupTime = CurrentTime;

	if (CleanedCount > 0)
	{
		DATASTORE_LOG(Verbose, TEXT("CleanExpiredCaches: Cleaned %d entries"), CleanedCount);
	}
}

FSuspenseCoreEquipmentSnapshot USuspenseCoreEquipmentDataStore::CreateSnapshot()
{
	FScopeLock Lock(&DataCriticalSection);

	FSuspenseCoreEquipmentSnapshot Snapshot = FSuspenseCoreEquipmentSnapshot::Create();

	// Copy all stored items
	Snapshot.EquippedItems = StoredItems;

	DATASTORE_LOG(Log, TEXT("CreateSnapshot: %d items"), StoredItems.Num());

	PublishSnapshotCreated(Snapshot);

	return Snapshot;
}

bool USuspenseCoreEquipmentDataStore::RestoreSnapshot(const FSuspenseCoreEquipmentSnapshot& Snapshot)
{
	FScopeLock Lock(&DataCriticalSection);

	DATASTORE_LOG(Log, TEXT("RestoreSnapshot: %d items"), Snapshot.EquippedItems.Num());

	// Clear existing data
	StoredItems.Empty();
	InvalidateAllCaches();

	// Restore items
	StoredItems = Snapshot.EquippedItems;

	// Update cache for all restored items
	for (const auto& Pair : StoredItems)
	{
		UpdateCache(Pair.Key, Pair.Value);
	}

	DataVersion++;

	return true;
}

bool USuspenseCoreEquipmentDataStore::SaveSnapshot(
	const FSuspenseCoreEquipmentSnapshot& Snapshot,
	const FString& SaveName)
{
	// TODO: Implement persistent storage
	DATASTORE_LOG(Log, TEXT("SaveSnapshot: %s"), *SaveName);
	return false;
}

bool USuspenseCoreEquipmentDataStore::LoadSnapshot(
	const FString& SaveName,
	FSuspenseCoreEquipmentSnapshot& OutSnapshot)
{
	// TODO: Implement persistent storage
	DATASTORE_LOG(Log, TEXT("LoadSnapshot: %s"), *SaveName);
	return false;
}

TMap<int32, FSuspenseInventoryItemInstance> USuspenseCoreEquipmentDataStore::FindItemsByTag(
	FGameplayTag Tag) const
{
	FScopeLock Lock(&DataCriticalSection);

	TMap<int32, FSuspenseInventoryItemInstance> FoundItems;

	for (const auto& Pair : StoredItems)
	{
		// TODO: Check if item has tag
		// if (Pair.Value.HasTag(Tag))
		// {
		//     FoundItems.Add(Pair.Key, Pair.Value);
		// }
	}

	return FoundItems;
}

TMap<int32, FSuspenseInventoryItemInstance> USuspenseCoreEquipmentDataStore::FindItemsByType(
	FName ItemType) const
{
	FScopeLock Lock(&DataCriticalSection);

	TMap<int32, FSuspenseInventoryItemInstance> FoundItems;

	for (const auto& Pair : StoredItems)
	{
		// TODO: Check if item type matches
		// if (Pair.Value.ItemType == ItemType)
		// {
		//     FoundItems.Add(Pair.Key, Pair.Value);
		// }
	}

	return FoundItems;
}

void USuspenseCoreEquipmentDataStore::ResetStatistics()
{
	TotalReads = 0;
	TotalWrites = 0;
	CacheHits = 0;
	CacheMisses = 0;
	DATASTORE_LOG(Log, TEXT("ResetStatistics"));
}

FString USuspenseCoreEquipmentDataStore::GetDataStoreStats() const
{
	return FString::Printf(
		TEXT("Items: %d, Reads: %d, Writes: %d, CacheHitRate: %.2f%%"),
		StoredItems.Num(),
		TotalReads,
		TotalWrites,
		GetCacheHitRate() * 100.0f);
}

void USuspenseCoreEquipmentDataStore::PublishDataChanged(int32 SlotIndex, bool bAdded)
{
	if (!EventBus.IsValid())
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetInt(TEXT("SlotIndex"), SlotIndex);
	EventData.SetBool(TEXT("Added"), bAdded);

	EventBus->Publish(
		FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.DataStore.Changed"))),
		EventData);
}

void USuspenseCoreEquipmentDataStore::PublishCacheInvalidated(int32 SlotIndex)
{
	if (!EventBus.IsValid())
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetInt(TEXT("SlotIndex"), SlotIndex);

	EventBus->Publish(
		FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.DataStore.CacheInvalidated"))),
		EventData);
}

void USuspenseCoreEquipmentDataStore::PublishSnapshotCreated(
	const FSuspenseCoreEquipmentSnapshot& Snapshot)
{
	if (!EventBus.IsValid())
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetString(TEXT("SnapshotId"), Snapshot.SnapshotId.ToString());
	EventData.SetInt(TEXT("ItemCount"), Snapshot.EquippedItems.Num());

	EventBus->Publish(
		FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.DataStore.SnapshotCreated"))),
		EventData);
}

FSuspenseCoreEquipmentCacheEntry& USuspenseCoreEquipmentDataStore::GetOrCreateCacheEntry(int32 SlotIndex)
{
	FSuspenseCoreEquipmentCacheEntry* Entry = CacheEntries.Find(SlotIndex);
	if (!Entry)
	{
		Entry = &CacheEntries.Add(SlotIndex);
	}
	return *Entry;
}

bool USuspenseCoreEquipmentDataStore::IsCacheValid(int32 SlotIndex) const
{
	const FSuspenseCoreEquipmentCacheEntry* Entry = CacheEntries.Find(SlotIndex);
	if (!Entry || !Entry->bIsValid)
	{
		return false;
	}

	const float CurrentTime = 0.0f; // TODO: Get actual game time
	return !Entry->IsExpired(CurrentTime, CacheMaxAge);
}

void USuspenseCoreEquipmentDataStore::UpdateCacheStats(bool bHit)
{
	if (bHit)
	{
		CacheHits++;
	}
	else
	{
		CacheMisses++;
	}
}
