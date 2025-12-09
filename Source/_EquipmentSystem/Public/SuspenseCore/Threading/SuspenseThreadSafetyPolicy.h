// SuspenseThreadSafetyPolicy.h
// Copyright SuspenseCore Team. All Rights Reserved.
//
// Threading policy and lock ordering documentation for Equipment System.
// Prevents deadlocks through documented lock acquisition order.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * SUSPENSECORE LOCK ORDERING POLICY
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * CRITICAL: When acquiring multiple locks, ALWAYS follow this order to prevent deadlocks:
 *
 * LEVEL 1 (Highest priority - acquire first):
 *   1. ServiceLocator::ServiceLock
 *   2. NetworkService::SecurityLock
 *
 * LEVEL 2 (Service-level locks):
 *   3. DataService::DataLock
 *   4. DataService::CacheLock
 *   5. DataService::DeltaLock
 *   6. OperationService::ExecutorLock
 *   7. OperationService::QueueLock
 *   8. OperationService::HistoryLock
 *   9. OperationService::StatsLock
 *
 * LEVEL 3 (Validation & Visualization):
 *  10. ValidationService::ValidatorsLock
 *  11. VisualizationService::VisualLock
 *
 * LEVEL 4 (Component-level locks):
 *  12. Component::CacheCriticalSection (per-component)
 *  13. Component::StateLock (per-component)
 *
 * LEVEL 5 (Utility locks - lowest priority):
 *  14. ObjectPool locks
 *  15. Metrics locks
 *
 * RULES:
 * - Never hold a higher-level lock while acquiring a lower-level lock
 * - Prefer FRWLock over FCriticalSection for read-heavy operations
 * - Keep lock scope as small as possible
 * - Release locks in reverse order of acquisition
 * - Use RAII (FScopeLock, FRWScopeLock) exclusively - never manual lock/unlock
 *
 * ═══════════════════════════════════════════════════════════════════════════════
 */

/**
 * Lock level enumeration for compile-time lock ordering verification
 */
enum class ESuspenseLockLevel : uint8
{
    // Level 1 - Highest priority
    ServiceLocator = 10,
    NetworkSecurity = 11,

    // Level 2 - Service data locks
    DataService_Data = 20,
    DataService_Cache = 21,
    DataService_Delta = 22,
    OperationService_Executor = 23,
    OperationService_Queue = 24,
    OperationService_History = 25,
    OperationService_Stats = 26,

    // Level 3 - Subsystem locks
    ValidationService = 30,
    VisualizationService = 31,

    // Level 4 - Component locks
    Component_Cache = 40,
    Component_State = 41,

    // Level 5 - Utility locks
    ObjectPool = 50,
    Metrics = 51,

    // Special
    None = 255
};

/**
 * Thread-safe lock acquisition tracker for debug builds.
 * Validates lock ordering at runtime.
 */
#if !UE_BUILD_SHIPPING
class EQUIPMENTSYSTEM_API FSuspenseLockOrderValidator
{
public:
    static FSuspenseLockOrderValidator& Get()
    {
        static FSuspenseLockOrderValidator Instance;
        return Instance;
    }

    /**
     * Called before acquiring a lock. Validates ordering.
     * @param LockLevel The level of lock being acquired
     * @param LockName Name for debugging
     */
    void OnLockAcquiring(ESuspenseLockLevel LockLevel, const TCHAR* LockName);

    /**
     * Called after releasing a lock.
     * @param LockLevel The level of lock being released
     */
    void OnLockReleased(ESuspenseLockLevel LockLevel);

    /**
     * Check if a lock at given level can be safely acquired
     */
    bool CanAcquireLock(ESuspenseLockLevel LockLevel) const;

private:
    FSuspenseLockOrderValidator() = default;

    // Per-thread lock stack (using TLS)
    // In production: use platform-specific TLS
};
#endif

/**
 * Scoped lock with level tracking for debug builds.
 * In shipping builds, this is a simple FScopeLock wrapper.
 */
template<ESuspenseLockLevel Level>
class TSuspenseScopeLock
{
public:
    explicit TSuspenseScopeLock(FCriticalSection& InCriticalSection, const TCHAR* InLockName = TEXT("Unknown"))
        : CriticalSection(InCriticalSection)
#if !UE_BUILD_SHIPPING
        , LockName(InLockName)
#endif
    {
#if !UE_BUILD_SHIPPING
        FSuspenseLockOrderValidator::Get().OnLockAcquiring(Level, LockName);
#endif
        CriticalSection.Lock();
    }

    ~TSuspenseScopeLock()
    {
        CriticalSection.Unlock();
#if !UE_BUILD_SHIPPING
        FSuspenseLockOrderValidator::Get().OnLockReleased(Level);
#endif
    }

    // Non-copyable
    TSuspenseScopeLock(const TSuspenseScopeLock&) = delete;
    TSuspenseScopeLock& operator=(const TSuspenseScopeLock&) = delete;

private:
    FCriticalSection& CriticalSection;
#if !UE_BUILD_SHIPPING
    const TCHAR* LockName;
#endif
};

/**
 * Scoped read-write lock with level tracking.
 */
template<ESuspenseLockLevel Level>
class TSuspenseRWScopeLock
{
public:
    TSuspenseRWScopeLock(FRWLock& InLock, FRWScopeLockType InLockType, const TCHAR* InLockName = TEXT("Unknown"))
        : Lock(InLock)
        , LockType(InLockType)
#if !UE_BUILD_SHIPPING
        , LockName(InLockName)
#endif
    {
#if !UE_BUILD_SHIPPING
        FSuspenseLockOrderValidator::Get().OnLockAcquiring(Level, LockName);
#endif
        if (LockType == SLT_ReadOnly)
        {
            Lock.ReadLock();
        }
        else
        {
            Lock.WriteLock();
        }
    }

    ~TSuspenseRWScopeLock()
    {
        if (LockType == SLT_ReadOnly)
        {
            Lock.ReadUnlock();
        }
        else
        {
            Lock.WriteUnlock();
        }
#if !UE_BUILD_SHIPPING
        FSuspenseLockOrderValidator::Get().OnLockReleased(Level);
#endif
    }

    // Non-copyable
    TSuspenseRWScopeLock(const TSuspenseRWScopeLock&) = delete;
    TSuspenseRWScopeLock& operator=(const TSuspenseRWScopeLock&) = delete;

private:
    FRWLock& Lock;
    FRWScopeLockType LockType;
#if !UE_BUILD_SHIPPING
    const TCHAR* LockName;
#endif
};

// Convenience typedefs for each service
using FServiceLocatorScopeLock = TSuspenseScopeLock<ESuspenseLockLevel::ServiceLocator>;
using FNetworkSecurityScopeLock = TSuspenseScopeLock<ESuspenseLockLevel::NetworkSecurity>;
using FValidationScopeLock = TSuspenseScopeLock<ESuspenseLockLevel::ValidationService>;
using FVisualizationScopeLock = TSuspenseScopeLock<ESuspenseLockLevel::VisualizationService>;

// Read-write lock typedefs
using FNetworkSecurityRWLock = TSuspenseRWScopeLock<ESuspenseLockLevel::NetworkSecurity>;
using FValidationRWLock = TSuspenseRWScopeLock<ESuspenseLockLevel::ValidationService>;
using FVisualizationRWLock = TSuspenseRWScopeLock<ESuspenseLockLevel::VisualizationService>;

/**
 * Helper macros for common lock patterns
 */

// Simple scoped lock with name
#define SUSPENSE_SCOPED_LOCK(LockLevel, CriticalSection) \
    TSuspenseScopeLock<ESuspenseLockLevel::LockLevel> ScopedLock_##CriticalSection(CriticalSection, TEXT(#CriticalSection))

// Read lock
#define SUSPENSE_READ_LOCK(LockLevel, RWLock) \
    TSuspenseRWScopeLock<ESuspenseLockLevel::LockLevel> ReadLock_##RWLock(RWLock, SLT_ReadOnly, TEXT(#RWLock))

// Write lock
#define SUSPENSE_WRITE_LOCK(LockLevel, RWLock) \
    TSuspenseRWScopeLock<ESuspenseLockLevel::LockLevel> WriteLock_##RWLock(RWLock, SLT_Write, TEXT(#RWLock))

/**
 * Read-preferring lock wrapper.
 * Use this when reads significantly outnumber writes.
 */
class EQUIPMENTSYSTEM_API FSuspenseReadPreferringLock
{
public:
    FSuspenseReadPreferringLock() = default;

    FORCEINLINE void ReadLock() const
    {
        Lock.ReadLock();
    }

    FORCEINLINE void ReadUnlock() const
    {
        Lock.ReadUnlock();
    }

    FORCEINLINE void WriteLock()
    {
        Lock.WriteLock();
    }

    FORCEINLINE void WriteUnlock()
    {
        Lock.WriteUnlock();
    }

    FRWLock& GetLock() { return Lock; }
    const FRWLock& GetLock() const { return Lock; }

private:
    mutable FRWLock Lock;
};

/**
 * Spin lock for very short critical sections.
 * Use only when lock is held for < 1000 cycles.
 */
class EQUIPMENTSYSTEM_API FSuspenseSpinLock
{
public:
    FSuspenseSpinLock()
        : LockFlag(0)
    {
    }

    FORCEINLINE void Lock()
    {
        while (FPlatformAtomics::InterlockedCompareExchange(&LockFlag, 1, 0) != 0)
        {
            FPlatformProcess::Yield();
        }
    }

    FORCEINLINE void Unlock()
    {
        FPlatformAtomics::InterlockedExchange(&LockFlag, 0);
    }

    FORCEINLINE bool TryLock()
    {
        return FPlatformAtomics::InterlockedCompareExchange(&LockFlag, 1, 0) == 0;
    }

private:
    // Note: volatile is NOT needed here because FPlatformAtomics functions
    // provide full memory barriers. In modern C++, volatile does NOT imply
    // atomicity or memory ordering - use std::atomic for that.
    // FPlatformAtomics::Interlocked* functions use platform intrinsics that
    // guarantee proper memory ordering without volatile.
    int32 LockFlag;
};

/**
 * Scoped spin lock
 */
class EQUIPMENTSYSTEM_API FSuspenseScopedSpinLock
{
public:
    explicit FSuspenseScopedSpinLock(FSuspenseSpinLock& InLock)
        : SpinLock(InLock)
    {
        SpinLock.Lock();
    }

    ~FSuspenseScopedSpinLock()
    {
        SpinLock.Unlock();
    }

    FSuspenseScopedSpinLock(const FSuspenseScopedSpinLock&) = delete;
    FSuspenseScopedSpinLock& operator=(const FSuspenseScopedSpinLock&) = delete;

private:
    FSuspenseSpinLock& SpinLock;
};
