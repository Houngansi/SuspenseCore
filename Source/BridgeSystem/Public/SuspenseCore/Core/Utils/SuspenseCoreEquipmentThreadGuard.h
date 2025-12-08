// Copyright Suspense Team. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

// Lock type for RAII guard
enum class ELockType : uint8
{
	Read,
	Write
};

/**
 * Exported RW-lock (implementation in .cpp).
 * Implementation: two FCriticalSection — one to protect reader counter, one to block writers.
 */
class BRIDGESYSTEM_API FEquipmentRWLock
{
public:
	FEquipmentRWLock();
	~FEquipmentRWLock();

	FEquipmentRWLock(const FEquipmentRWLock&) = delete;
	FEquipmentRWLock& operator=(const FEquipmentRWLock&) = delete;

	// operations
	void AcquireRead();
	void ReleaseRead();
	void AcquireWrite();
	void ReleaseWrite();

	bool TryAcquireRead();
	bool TryAcquireWrite();

private:
	FCriticalSection ReaderCountMutex;
	FCriticalSection WriterMutex;
	int32 ReaderCount = 0;
};

/**
 * RAII guard for read/write (implementation in .cpp)
 */
class BRIDGESYSTEM_API FEquipmentRWGuard
{
public:
	FEquipmentRWGuard(FEquipmentRWLock& InLock, ELockType InType);
	~FEquipmentRWGuard();

	FEquipmentRWGuard(const FEquipmentRWGuard&) = delete;
	FEquipmentRWGuard& operator=(const FEquipmentRWGuard&) = delete;

private:
	FEquipmentRWLock* LockPtr = nullptr;
	ELockType Type = ELockType::Read;
	bool bLocked = false;
};

/**
 * Scope lock for FCriticalSection only.
 * NOTE: NO default arguments in .h to avoid "Redefinition of default argument".
 */
class BRIDGESYSTEM_API FEquipmentScopeLock
{
public:
	explicit FEquipmentScopeLock(FCriticalSection& InCS, const TCHAR* Label);
	~FEquipmentScopeLock();

	FEquipmentScopeLock(const FEquipmentScopeLock&) = delete;
	FEquipmentScopeLock& operator=(const FEquipmentScopeLock&) = delete;

private:
	FCriticalSection* CS = nullptr;
};

// ---- local name-join helpers (no dependency on PreprocessorHelpers.h) ----
#ifndef EQUIPMENT_JOIN_IMPL
#define EQUIPMENT_JOIN_IMPL(a,b) a##b
#endif
#ifndef EQUIPMENT_JOIN
#define EQUIPMENT_JOIN(a,b) EQUIPMENT_JOIN_IMPL(a,b)
#endif

// Guard macros (protected against redefinition)
#ifndef EQUIPMENT_READ_LOCK
#define EQUIPMENT_READ_LOCK(RW)  FEquipmentRWGuard EQUIPMENT_JOIN(_EqReadLock__, __LINE__)(RW, ELockType::Read)
#endif

#ifndef EQUIPMENT_WRITE_LOCK
#define EQUIPMENT_WRITE_LOCK(RW) FEquipmentRWGuard EQUIPMENT_JOIN(_EqWriteLock__, __LINE__)(RW, ELockType::Write)
#endif

#ifndef EQUIPMENT_SCOPE_LOCK
#define EQUIPMENT_SCOPE_LOCK(CS) FEquipmentScopeLock EQUIPMENT_JOIN(_EqScopeLock__, __LINE__)(CS, nullptr)
#endif
