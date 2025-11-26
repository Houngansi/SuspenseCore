// Copyright Suspense Team. All Rights Reserved.
#include "Core/Utils/SuspenseEquipmentThreadGuard.h"

#include "Core/Utils/SuspenseEquipmentThreadGuard.h"

// =========================
// FEquipmentRWLock
// =========================

FEquipmentRWLock::FEquipmentRWLock() = default;
FEquipmentRWLock::~FEquipmentRWLock() = default;

void FEquipmentRWLock::AcquireRead()
{
	ReaderCountMutex.Lock();
	++ReaderCount;
	if (ReaderCount == 1)
	{
		// first reader blocks writers
		WriterMutex.Lock();
	}
	ReaderCountMutex.Unlock();
}

void FEquipmentRWLock::ReleaseRead()
{
	ReaderCountMutex.Lock();
	--ReaderCount;
	if (ReaderCount == 0)
	{
		// last reader releases writers
		WriterMutex.Unlock();
	}
	ReaderCountMutex.Unlock();
}

void FEquipmentRWLock::AcquireWrite()
{
	WriterMutex.Lock();
}

void FEquipmentRWLock::ReleaseWrite()
{
	WriterMutex.Unlock();
}

bool FEquipmentRWLock::TryAcquireRead()
{
	if (!ReaderCountMutex.TryLock())
	{
		return false;
	}

	bool bLockedWriters = true;
	if (ReaderCount == 0)
	{
		// first reader must non-blockingly lock writers
		bLockedWriters = WriterMutex.TryLock();
		if (!bLockedWriters)
		{
			ReaderCountMutex.Unlock();
			return false;
		}
	}

	++ReaderCount;
	ReaderCountMutex.Unlock();
	return true;
}

bool FEquipmentRWLock::TryAcquireWrite()
{
	return WriterMutex.TryLock();
}

// =========================
// FEquipmentRWGuard
// =========================

FEquipmentRWGuard::FEquipmentRWGuard(FEquipmentRWLock& InLock, ELockType InType)
	: LockPtr(&InLock)
	, Type(InType)
	, bLocked(true)
{
	if (Type == ELockType::Read)  { LockPtr->AcquireRead(); }
	else                          { LockPtr->AcquireWrite(); }
}

FEquipmentRWGuard::~FEquipmentRWGuard()
{
	if (!LockPtr || !bLocked) return;
	if (Type == ELockType::Read)  { LockPtr->ReleaseRead(); }
	else                          { LockPtr->ReleaseWrite(); }
	bLocked = false;
}

// =========================
// FEquipmentScopeLock
// =========================

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
