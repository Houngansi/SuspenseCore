// Copyright Suspense Team. All Rights Reserved.
// SuspenseCore - Clean Architecture Foundation
// Threading primitives implementation

#include "SuspenseCore/Core/Utils/SuspenseCoreEquipmentThreadGuard.h"

DEFINE_LOG_CATEGORY_STATIC(LogEquipmentThreadGuard, Log, All);

// ═══════════════════════════════════════════════════════════════════════════════
// FEquipmentRWLock - Native FRWLock Composition
// ═══════════════════════════════════════════════════════════════════════════════

FEquipmentRWLock::FEquipmentRWLock()
{
	// FRWLock is default-constructed and ready to use
}

FEquipmentRWLock::~FEquipmentRWLock()
{
	// FRWLock handles cleanup automatically
}

void FEquipmentRWLock::AcquireRead()
{
	NativeLock.ReadLock();
}

void FEquipmentRWLock::ReleaseRead()
{
	NativeLock.ReadUnlock();
}

void FEquipmentRWLock::AcquireWrite()
{
	NativeLock.WriteLock();
}

void FEquipmentRWLock::ReleaseWrite()
{
	NativeLock.WriteUnlock();
}

bool FEquipmentRWLock::TryAcquireRead()
{
	return NativeLock.TryReadLock();
}

bool FEquipmentRWLock::TryAcquireWrite()
{
	return NativeLock.TryWriteLock();
}

// ═══════════════════════════════════════════════════════════════════════════════
// FEquipmentRWGuard - RAII Guard
// ═══════════════════════════════════════════════════════════════════════════════

FEquipmentRWGuard::FEquipmentRWGuard(FEquipmentRWLock& InLock, ELockType InType)
	: LockPtr(&InLock)
	, Type(InType)
	, bLocked(true)
{
	if (Type == ELockType::Read)
	{
		LockPtr->AcquireRead();
	}
	else
	{
		LockPtr->AcquireWrite();
	}
}

FEquipmentRWGuard::~FEquipmentRWGuard()
{
	if (LockPtr && bLocked)
	{
		if (Type == ELockType::Read)
		{
			LockPtr->ReleaseRead();
		}
		else
		{
			LockPtr->ReleaseWrite();
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// FEquipmentScopeLock - FCriticalSection Guard
// ═══════════════════════════════════════════════════════════════════════════════

FEquipmentScopeLock::FEquipmentScopeLock(FCriticalSection& InCS, const TCHAR* /*Label*/)
	: CS(&InCS)
{
	CS->Lock();
}

FEquipmentScopeLock::~FEquipmentScopeLock()
{
	if (CS)
	{
		CS->Unlock();
	}
}
