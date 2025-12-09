// SuspenseNonceLRUCache.h
// Copyright SuspenseCore Team. All Rights Reserved.
//
// LRU (Least Recently Used) cache for nonces with TTL (Time-To-Live) support.
// Prevents unbounded memory growth while maintaining replay attack protection.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformTime.h"

/**
 * Entry in the nonce cache containing timestamp and usage info
 */
struct FSuspenseNonceEntry
{
    /** The nonce value */
    uint64 Nonce = 0;

    /** Time when nonce was added */
    double CreationTime = 0.0;

    /** Time when nonce expires (CreationTime + TTL) */
    double ExpiryTime = 0.0;

    /** Index in the LRU order list */
    int32 LRUIndex = INDEX_NONE;

    /** Whether this nonce has been confirmed (vs pending) */
    bool bConfirmed = false;
};

/**
 * Statistics for monitoring cache performance
 */
struct FSuspenseNonceCacheStats
{
    /** Total nonces added to cache */
    uint64 TotalAdded = 0;

    /** Total successful lookups (replay attacks blocked) */
    uint64 TotalHits = 0;

    /** Total misses (new nonces) */
    uint64 TotalMisses = 0;

    /** Total nonces evicted due to capacity */
    uint64 TotalEvictions = 0;

    /** Total nonces expired by TTL */
    uint64 TotalExpired = 0;

    /** Current cache size */
    int32 CurrentSize = 0;

    /** Peak cache size */
    int32 PeakSize = 0;

    FString ToString() const
    {
        return FString::Printf(
            TEXT("NonceCacheStats: Added=%llu, Hits=%llu, Misses=%llu, Evictions=%llu, Expired=%llu, Current=%d, Peak=%d"),
            TotalAdded, TotalHits, TotalMisses, TotalEvictions, TotalExpired, CurrentSize, PeakSize
        );
    }
};

/**
 * Thread-safe LRU cache for nonces with TTL expiration.
 *
 * Features:
 * - O(1) lookup for replay detection
 * - O(1) insertion with LRU ordering
 * - Automatic TTL-based expiration
 * - Configurable max capacity with LRU eviction
 * - Separate tracking for pending vs confirmed nonces
 * - Comprehensive statistics for monitoring
 */
class EQUIPMENTSYSTEM_API FSuspenseNonceLRUCache
{
public:
    /**
     * Constructor
     * @param InMaxCapacity Maximum number of nonces to store
     * @param InDefaultTTL Default time-to-live in seconds
     */
    explicit FSuspenseNonceLRUCache(int32 InMaxCapacity = 10000, float InDefaultTTL = 300.0f);

    ~FSuspenseNonceLRUCache();

    // Non-copyable
    FSuspenseNonceLRUCache(const FSuspenseNonceLRUCache&) = delete;
    FSuspenseNonceLRUCache& operator=(const FSuspenseNonceLRUCache&) = delete;

    /**
     * Check if a nonce exists in the cache (processed or pending)
     * @param Nonce The nonce to check
     * @return true if nonce exists (replay attack!)
     */
    bool Contains(uint64 Nonce) const;

    /**
     * Add a nonce as pending (awaiting confirmation)
     * @param Nonce The nonce to add
     * @param TTL Optional custom TTL (uses default if <= 0)
     * @return true if added successfully, false if already exists
     */
    bool AddPending(uint64 Nonce, float TTL = -1.0f);

    /**
     * Confirm a pending nonce (moves to processed state)
     * @param Nonce The nonce to confirm
     * @return true if confirmed, false if not found in pending
     */
    bool Confirm(uint64 Nonce);

    /**
     * Reject/remove a pending nonce
     * @param Nonce The nonce to reject
     * @return true if removed, false if not found
     */
    bool Reject(uint64 Nonce);

    /**
     * Add a nonce directly as confirmed (for server-side processing)
     * @param Nonce The nonce to add
     * @param TTL Optional custom TTL
     * @return true if added, false if already exists
     */
    bool AddConfirmed(uint64 Nonce, float TTL = -1.0f);

    /**
     * Remove expired entries from the cache
     * @return Number of entries removed
     */
    int32 CleanExpired();

    /**
     * Clear all entries from the cache
     */
    void Clear();

    /**
     * Get current cache statistics
     */
    FSuspenseNonceCacheStats GetStats() const;

    /**
     * Reset statistics counters
     */
    void ResetStats();

    /**
     * Get current number of entries
     */
    int32 GetSize() const;

    /**
     * Get maximum capacity
     */
    int32 GetMaxCapacity() const { return MaxCapacity; }

    /**
     * Set maximum capacity (may trigger evictions)
     */
    void SetMaxCapacity(int32 NewCapacity);

    /**
     * Get default TTL in seconds
     */
    float GetDefaultTTL() const { return DefaultTTL; }

    /**
     * Set default TTL for new entries
     */
    void SetDefaultTTL(float NewTTL);

    /**
     * Check if a nonce is in pending state
     */
    bool IsPending(uint64 Nonce) const;

    /**
     * Check if a nonce is confirmed
     */
    bool IsConfirmed(uint64 Nonce) const;

    /**
     * Get number of pending nonces
     */
    int32 GetPendingCount() const;

    /**
     * Get number of confirmed nonces
     */
    int32 GetConfirmedCount() const;

private:
    /** Maximum number of entries */
    int32 MaxCapacity;

    /** Default TTL in seconds */
    float DefaultTTL;

    /** Main hash map for O(1) lookup */
    TMap<uint64, FSuspenseNonceEntry> NonceMap;

    /** LRU order tracking (most recent at back) */
    TArray<uint64> LRUOrder;

    /** Statistics */
    mutable FSuspenseNonceCacheStats Stats;

    /** Thread safety */
    mutable FCriticalSection CacheLock;

    /**
     * Move a nonce to the back of the LRU list (mark as recently used)
     */
    void TouchLRU(uint64 Nonce);

    /**
     * Evict the least recently used entry
     * @return true if an entry was evicted
     */
    bool EvictLRU();

    /**
     * Remove a nonce from the LRU list
     */
    void RemoveFromLRU(uint64 Nonce);

    /**
     * Update statistics
     */
    void UpdateStats() const;
};

/**
 * RAII helper for automatic expired nonce cleanup
 */
class EQUIPMENTSYSTEM_API FScopedNonceCacheCleanup
{
public:
    explicit FScopedNonceCacheCleanup(FSuspenseNonceLRUCache& InCache);
    ~FScopedNonceCacheCleanup();

    /** Get number of entries cleaned */
    int32 GetCleanedCount() const { return CleanedCount; }

private:
    FSuspenseNonceLRUCache& Cache;
    int32 CleanedCount;
};
