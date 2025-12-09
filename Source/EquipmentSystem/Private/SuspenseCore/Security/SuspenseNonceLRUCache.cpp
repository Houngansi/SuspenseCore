// SuspenseNonceLRUCache.cpp
// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Security/SuspenseNonceLRUCache.h"

DEFINE_LOG_CATEGORY_STATIC(LogNonceCache, Log, All);

//========================================
// FSuspenseNonceLRUCache
//========================================

FSuspenseNonceLRUCache::FSuspenseNonceLRUCache(int32 InMaxCapacity, float InDefaultTTL)
    : MaxCapacity(FMath::Max(100, InMaxCapacity))
    , DefaultTTL(FMath::Max(1.0f, InDefaultTTL))
{
    // Pre-allocate for expected capacity
    NonceMap.Reserve(MaxCapacity);
    LRUOrder.Reserve(MaxCapacity);
}

FSuspenseNonceLRUCache::~FSuspenseNonceLRUCache()
{
    Clear();
}

bool FSuspenseNonceLRUCache::Contains(uint64 Nonce) const
{
    FScopeLock Lock(&CacheLock);

    const FSuspenseNonceEntry* Entry = NonceMap.Find(Nonce);
    if (Entry)
    {
        // SECURITY: Check if expired - expired nonces MUST be rejected
        // to prevent replay attacks (BestPractices.md Section 13)
        if (Entry->ExpiryTime < FPlatformTime::Seconds())
        {
            // Entry is expired - treat as NOT FOUND to prevent replay attacks
            // This is the secure approach: expired nonce = invalid nonce
            Stats.TotalExpired++;
            Stats.TotalMisses++;
            UE_LOG(LogNonceCache, Warning,
                TEXT("SECURITY: Rejected expired nonce %llu (potential replay attack)"), Nonce);
            return false;
        }

        Stats.TotalHits++;
        return true;
    }

    Stats.TotalMisses++;
    return false;
}

bool FSuspenseNonceLRUCache::AddPending(uint64 Nonce, float TTL)
{
    FScopeLock Lock(&CacheLock);

    // Check if already exists
    if (NonceMap.Contains(Nonce))
    {
        Stats.TotalHits++;
        return false;
    }

    // Ensure capacity
    while (NonceMap.Num() >= MaxCapacity)
    {
        if (!EvictLRU())
        {
            break;
        }
    }

    // Create new entry
    FSuspenseNonceEntry Entry;
    Entry.Nonce = Nonce;
    Entry.CreationTime = FPlatformTime::Seconds();
    Entry.ExpiryTime = Entry.CreationTime + (TTL > 0.0f ? TTL : DefaultTTL);
    Entry.bConfirmed = false;
    Entry.LRUIndex = LRUOrder.Num();

    // Add to map and LRU list
    NonceMap.Add(Nonce, Entry);
    LRUOrder.Add(Nonce);

    // Update stats
    Stats.TotalAdded++;
    UpdateStats();

    return true;
}

bool FSuspenseNonceLRUCache::Confirm(uint64 Nonce)
{
    FScopeLock Lock(&CacheLock);

    FSuspenseNonceEntry* Entry = NonceMap.Find(Nonce);
    if (!Entry)
    {
        return false;
    }

    if (Entry->bConfirmed)
    {
        // Already confirmed
        return true;
    }

    Entry->bConfirmed = true;

    // Touch LRU to mark as recently used
    TouchLRU(Nonce);

    return true;
}

bool FSuspenseNonceLRUCache::Reject(uint64 Nonce)
{
    FScopeLock Lock(&CacheLock);

    FSuspenseNonceEntry* Entry = NonceMap.Find(Nonce);
    if (!Entry)
    {
        return false;
    }

    // Only allow rejecting pending nonces
    if (Entry->bConfirmed)
    {
        return false;
    }

    RemoveFromLRU(Nonce);
    NonceMap.Remove(Nonce);
    UpdateStats();

    return true;
}

bool FSuspenseNonceLRUCache::AddConfirmed(uint64 Nonce, float TTL)
{
    FScopeLock Lock(&CacheLock);

    // Check if already exists
    if (NonceMap.Contains(Nonce))
    {
        Stats.TotalHits++;
        return false;
    }

    // Ensure capacity
    while (NonceMap.Num() >= MaxCapacity)
    {
        if (!EvictLRU())
        {
            break;
        }
    }

    // Create confirmed entry
    FSuspenseNonceEntry Entry;
    Entry.Nonce = Nonce;
    Entry.CreationTime = FPlatformTime::Seconds();
    Entry.ExpiryTime = Entry.CreationTime + (TTL > 0.0f ? TTL : DefaultTTL);
    Entry.bConfirmed = true;
    Entry.LRUIndex = LRUOrder.Num();

    NonceMap.Add(Nonce, Entry);
    LRUOrder.Add(Nonce);

    Stats.TotalAdded++;
    UpdateStats();

    return true;
}

int32 FSuspenseNonceLRUCache::CleanExpired()
{
    FScopeLock Lock(&CacheLock);

    double CurrentTime = FPlatformTime::Seconds();
    TArray<uint64> ExpiredNonces;

    // Find expired entries
    for (const auto& Pair : NonceMap)
    {
        if (Pair.Value.ExpiryTime < CurrentTime)
        {
            ExpiredNonces.Add(Pair.Key);
        }
    }

    // Remove expired entries
    for (uint64 Nonce : ExpiredNonces)
    {
        RemoveFromLRU(Nonce);
        NonceMap.Remove(Nonce);
        Stats.TotalExpired++;
    }

    if (ExpiredNonces.Num() > 0)
    {
        UpdateStats();
        UE_LOG(LogNonceCache, Verbose, TEXT("Cleaned %d expired nonces, %d remaining"),
            ExpiredNonces.Num(), NonceMap.Num());
    }

    return ExpiredNonces.Num();
}

void FSuspenseNonceLRUCache::Clear()
{
    FScopeLock Lock(&CacheLock);

    NonceMap.Empty();
    LRUOrder.Empty();
    UpdateStats();

    UE_LOG(LogNonceCache, Log, TEXT("Nonce cache cleared"));
}

FSuspenseNonceCacheStats FSuspenseNonceLRUCache::GetStats() const
{
    FScopeLock Lock(&CacheLock);
    UpdateStats();
    return Stats;
}

void FSuspenseNonceLRUCache::ResetStats()
{
    FScopeLock Lock(&CacheLock);

    Stats.TotalAdded = 0;
    Stats.TotalHits = 0;
    Stats.TotalMisses = 0;
    Stats.TotalEvictions = 0;
    Stats.TotalExpired = 0;
    // Keep CurrentSize and PeakSize as-is
}

int32 FSuspenseNonceLRUCache::GetSize() const
{
    FScopeLock Lock(&CacheLock);
    return NonceMap.Num();
}

void FSuspenseNonceLRUCache::SetMaxCapacity(int32 NewCapacity)
{
    FScopeLock Lock(&CacheLock);

    MaxCapacity = FMath::Max(100, NewCapacity);

    // Evict if over new capacity
    while (NonceMap.Num() > MaxCapacity)
    {
        EvictLRU();
    }

    UpdateStats();
}

void FSuspenseNonceLRUCache::SetDefaultTTL(float NewTTL)
{
    FScopeLock Lock(&CacheLock);
    DefaultTTL = FMath::Max(1.0f, NewTTL);
}

bool FSuspenseNonceLRUCache::IsPending(uint64 Nonce) const
{
    FScopeLock Lock(&CacheLock);

    const FSuspenseNonceEntry* Entry = NonceMap.Find(Nonce);
    return Entry && !Entry->bConfirmed;
}

bool FSuspenseNonceLRUCache::IsConfirmed(uint64 Nonce) const
{
    FScopeLock Lock(&CacheLock);

    const FSuspenseNonceEntry* Entry = NonceMap.Find(Nonce);
    return Entry && Entry->bConfirmed;
}

int32 FSuspenseNonceLRUCache::GetPendingCount() const
{
    FScopeLock Lock(&CacheLock);

    int32 Count = 0;
    for (const auto& Pair : NonceMap)
    {
        if (!Pair.Value.bConfirmed)
        {
            Count++;
        }
    }
    return Count;
}

int32 FSuspenseNonceLRUCache::GetConfirmedCount() const
{
    FScopeLock Lock(&CacheLock);

    int32 Count = 0;
    for (const auto& Pair : NonceMap)
    {
        if (Pair.Value.bConfirmed)
        {
            Count++;
        }
    }
    return Count;
}

void FSuspenseNonceLRUCache::TouchLRU(uint64 Nonce)
{
    // Find current position
    FSuspenseNonceEntry* Entry = NonceMap.Find(Nonce);
    if (!Entry || Entry->LRUIndex == INDEX_NONE)
    {
        return;
    }

    // If already at back (most recent), nothing to do
    if (Entry->LRUIndex == LRUOrder.Num() - 1)
    {
        return;
    }

    // Remove from current position
    int32 OldIndex = Entry->LRUIndex;
    LRUOrder.RemoveAt(OldIndex);

    // Update indices for all entries after the removed one
    for (int32 i = OldIndex; i < LRUOrder.Num(); ++i)
    {
        if (FSuspenseNonceEntry* Other = NonceMap.Find(LRUOrder[i]))
        {
            Other->LRUIndex = i;
        }
    }

    // Add to back (most recent)
    Entry->LRUIndex = LRUOrder.Num();
    LRUOrder.Add(Nonce);
}

bool FSuspenseNonceLRUCache::EvictLRU()
{
    if (LRUOrder.Num() == 0)
    {
        return false;
    }

    // Evict from front (least recently used)
    // But skip pending nonces if possible - prefer evicting confirmed ones
    int32 EvictIndex = INDEX_NONE;

    // First pass: try to find a confirmed entry to evict
    for (int32 i = 0; i < LRUOrder.Num(); ++i)
    {
        const FSuspenseNonceEntry* Entry = NonceMap.Find(LRUOrder[i]);
        if (Entry && Entry->bConfirmed)
        {
            EvictIndex = i;
            break;
        }
    }

    // If no confirmed entries, evict the oldest (even if pending)
    if (EvictIndex == INDEX_NONE)
    {
        EvictIndex = 0;
    }

    uint64 NonceToEvict = LRUOrder[EvictIndex];

    // Remove from map
    NonceMap.Remove(NonceToEvict);

    // Remove from LRU list
    LRUOrder.RemoveAt(EvictIndex);

    // Update indices for remaining entries
    for (int32 i = EvictIndex; i < LRUOrder.Num(); ++i)
    {
        if (FSuspenseNonceEntry* Entry = NonceMap.Find(LRUOrder[i]))
        {
            Entry->LRUIndex = i;
        }
    }

    Stats.TotalEvictions++;

    UE_LOG(LogNonceCache, Verbose, TEXT("Evicted nonce %llu from LRU cache"), NonceToEvict);

    return true;
}

void FSuspenseNonceLRUCache::RemoveFromLRU(uint64 Nonce)
{
    FSuspenseNonceEntry* Entry = NonceMap.Find(Nonce);
    if (!Entry || Entry->LRUIndex == INDEX_NONE)
    {
        return;
    }

    int32 Index = Entry->LRUIndex;

    // Sanity check
    if (Index >= 0 && Index < LRUOrder.Num() && LRUOrder[Index] == Nonce)
    {
        LRUOrder.RemoveAt(Index);

        // Update indices for remaining entries
        for (int32 i = Index; i < LRUOrder.Num(); ++i)
        {
            if (FSuspenseNonceEntry* Other = NonceMap.Find(LRUOrder[i]))
            {
                Other->LRUIndex = i;
            }
        }
    }

    Entry->LRUIndex = INDEX_NONE;
}

void FSuspenseNonceLRUCache::UpdateStats() const
{
    Stats.CurrentSize = NonceMap.Num();
    if (Stats.CurrentSize > Stats.PeakSize)
    {
        Stats.PeakSize = Stats.CurrentSize;
    }
}

//========================================
// FScopedNonceCacheCleanup
//========================================

FScopedNonceCacheCleanup::FScopedNonceCacheCleanup(FSuspenseNonceLRUCache& InCache)
    : Cache(InCache)
    , CleanedCount(0)
{
    CleanedCount = Cache.CleanExpired();
}

FScopedNonceCacheCleanup::~FScopedNonceCacheCleanup()
{
    // Could do additional cleanup here if needed
}
