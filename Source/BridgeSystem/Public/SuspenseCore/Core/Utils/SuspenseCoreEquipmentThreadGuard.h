// Copyright Suspense Team. All Rights Reserved.
// SuspenseCore - Clean Architecture Foundation
// Threading primitives using native UE5 FRWLock for optimal performance

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"

// UE5 native Read-Write Lock - OS-optimized, no writer starvation
#include "HAL/PlatformProcess.h"

// Lock type for RAII guard
enum class ELockType : uint8
{
	Read,
	Write
};

/**
 * FEquipmentRWLock
 *
 * High-performance Read-Write lock using UE5 native FRWLock via composition.
 * Replaces custom implementation to avoid Writer Starvation issues.
 *
 * Key improvements over custom implementation:
 * - Uses OS-native primitives (SRWLock on Windows, pthread_rwlock on POSIX)
 * - Fair scheduling prevents writer starvation
 * - Better cache behavior and lower contention
 *
 * Thread Safety: All methods are thread-safe.
 *
 * Usage:
 *   FEquipmentRWLock Lock;
 *
 *   // For reading (multiple concurrent readers allowed):
 *   {
 *       EQUIPMENT_READ_LOCK(Lock);
 *       // ... read operations ...
 *   }
 *
 *   // For writing (exclusive access):
 *   {
 *       EQUIPMENT_WRITE_LOCK(Lock);
 *       // ... write operations ...
 *   }
 */
class BRIDGESYSTEM_API FEquipmentRWLock
{
public:
	FEquipmentRWLock();
	~FEquipmentRWLock();

	// Non-copyable
	FEquipmentRWLock(const FEquipmentRWLock&) = delete;
	FEquipmentRWLock& operator=(const FEquipmentRWLock&) = delete;

	// Core operations - delegate to native FRWLock
	void AcquireRead();
	void ReleaseRead();
	void AcquireWrite();
	void ReleaseWrite();

	// Try-lock variants for non-blocking scenarios
	bool TryAcquireRead();
	bool TryAcquireWrite();

private:
	// Composition with native UE5 FRWLock
	FRWLock NativeLock;
};

/**
 * FEquipmentRWGuard
 *
 * RAII guard for FEquipmentRWLock. Automatically acquires lock on construction
 * and releases on destruction. Exception-safe.
 *
 * Usage:
 *   FEquipmentRWLock Lock;
 *   {
 *       FEquipmentRWGuard Guard(Lock, ELockType::Read);
 *       // Lock is held here
 *   }
 *   // Lock automatically released
 */
class BRIDGESYSTEM_API FEquipmentRWGuard
{
public:
	FEquipmentRWGuard(FEquipmentRWLock& InLock, ELockType InType);
	~FEquipmentRWGuard();

	// Non-copyable
	FEquipmentRWGuard(const FEquipmentRWGuard&) = delete;
	FEquipmentRWGuard& operator=(const FEquipmentRWGuard&) = delete;

private:
	FEquipmentRWLock* LockPtr = nullptr;
	ELockType Type = ELockType::Read;
	bool bLocked = false;
};

/**
 * FEquipmentScopeLock
 *
 * RAII guard for FCriticalSection. Similar to FScopeLock but with optional label.
 */
class BRIDGESYSTEM_API FEquipmentScopeLock
{
public:
	explicit FEquipmentScopeLock(FCriticalSection& InCS, const TCHAR* Label = nullptr);
	~FEquipmentScopeLock();

	// Non-copyable
	FEquipmentScopeLock(const FEquipmentScopeLock&) = delete;
	FEquipmentScopeLock& operator=(const FEquipmentScopeLock&) = delete;

private:
	FCriticalSection* CS = nullptr;
};

// ═══════════════════════════════════════════════════════════════════════════════
// HELPER MACROS
// ═══════════════════════════════════════════════════════════════════════════════

// Local name-join helpers (no dependency on PreprocessorHelpers.h)
#ifndef EQUIPMENT_JOIN_IMPL
#define EQUIPMENT_JOIN_IMPL(a, b) a##b
#endif

#ifndef EQUIPMENT_JOIN
#define EQUIPMENT_JOIN(a, b) EQUIPMENT_JOIN_IMPL(a, b)
#endif

/**
 * EQUIPMENT_READ_LOCK(RWLock)
 *
 * Creates a scoped read lock. Multiple readers can hold the lock simultaneously.
 * Lock is automatically released when scope ends.
 */
#ifndef EQUIPMENT_READ_LOCK
#define EQUIPMENT_READ_LOCK(RW) \
	FEquipmentRWGuard EQUIPMENT_JOIN(_EqReadLock__, __LINE__)(RW, ELockType::Read)
#endif

/**
 * EQUIPMENT_WRITE_LOCK(RWLock)
 *
 * Creates a scoped write lock. Only one writer can hold the lock.
 * All readers are blocked while write lock is held.
 * Lock is automatically released when scope ends.
 */
#ifndef EQUIPMENT_WRITE_LOCK
#define EQUIPMENT_WRITE_LOCK(RW) \
	FEquipmentRWGuard EQUIPMENT_JOIN(_EqWriteLock__, __LINE__)(RW, ELockType::Write)
#endif

/**
 * EQUIPMENT_SCOPE_LOCK(CriticalSection)
 *
 * Creates a scoped lock for FCriticalSection.
 * Lock is automatically released when scope ends.
 */
#ifndef EQUIPMENT_SCOPE_LOCK
#define EQUIPMENT_SCOPE_LOCK(CS) \
	FEquipmentScopeLock EQUIPMENT_JOIN(_EqScopeLock__, __LINE__)(CS, nullptr)
#endif
