// SuspenseThreadSafetyPolicy.cpp
// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Threading/SuspenseThreadSafetyPolicy.h"
#include "HAL/PlatformTLS.h"
#include "Containers/Array.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseThreading, Log, All);

#if !UE_BUILD_SHIPPING

// Thread-local storage for lock stack
namespace SuspenseThreadingPrivate
{
    // Simple per-thread lock stack using TLS slot
    static uint32 LockStackTLSSlot = 0;
    static bool bTLSInitialized = false;

    struct FThreadLockStack
    {
        TArray<ESuspenseLockLevel> HeldLocks;
        ESuspenseLockLevel HighestHeldLevel = ESuspenseLockLevel::None;

        void Push(ESuspenseLockLevel Level)
        {
            HeldLocks.Add(Level);
            if (Level < HighestHeldLevel || HighestHeldLevel == ESuspenseLockLevel::None)
            {
                HighestHeldLevel = Level;
            }
        }

        void Pop(ESuspenseLockLevel Level)
        {
            // Find and remove the level (should be at the end for proper LIFO)
            int32 Index = HeldLocks.FindLast(Level);
            if (Index != INDEX_NONE)
            {
                HeldLocks.RemoveAt(Index);
            }

            // Recalculate highest
            HighestHeldLevel = ESuspenseLockLevel::None;
            for (ESuspenseLockLevel L : HeldLocks)
            {
                if (HighestHeldLevel == ESuspenseLockLevel::None || L < HighestHeldLevel)
                {
                    HighestHeldLevel = L;
                }
            }
        }

        bool CanAcquire(ESuspenseLockLevel Level) const
        {
            // Can acquire if no locks held, or if new level is lower priority (higher number)
            if (HighestHeldLevel == ESuspenseLockLevel::None)
            {
                return true;
            }
            return static_cast<uint8>(Level) > static_cast<uint8>(HighestHeldLevel);
        }
    };

    FThreadLockStack* GetThreadLockStack()
    {
        if (!bTLSInitialized)
        {
            LockStackTLSSlot = FPlatformTLS::AllocTlsSlot();
            bTLSInitialized = true;
        }

        FThreadLockStack* Stack = static_cast<FThreadLockStack*>(FPlatformTLS::GetTlsValue(LockStackTLSSlot));
        if (!Stack)
        {
            Stack = new FThreadLockStack();
            FPlatformTLS::SetTlsValue(LockStackTLSSlot, Stack);
        }
        return Stack;
    }
}

void FSuspenseLockOrderValidator::OnLockAcquiring(ESuspenseLockLevel LockLevel, const TCHAR* LockName)
{
    using namespace SuspenseThreadingPrivate;

    FThreadLockStack* Stack = GetThreadLockStack();
    if (!Stack->CanAcquire(LockLevel))
    {
        // Lock order violation!
        UE_LOG(LogSuspenseThreading, Error,
            TEXT("LOCK ORDER VIOLATION! Attempting to acquire '%s' (level %d) while holding lock at level %d. "
                 "This may cause deadlock! Check SuspenseThreadSafetyPolicy.h for correct ordering."),
            LockName,
            static_cast<int32>(LockLevel),
            static_cast<int32>(Stack->HighestHeldLevel));

        // In debug builds, we could assert here
        // For now, just log and continue (to avoid breaking existing code)
#if DO_CHECK
        // ensureAlwaysMsgf(false, TEXT("Lock order violation detected - see log for details"));
#endif
    }

    Stack->Push(LockLevel);
}

void FSuspenseLockOrderValidator::OnLockReleased(ESuspenseLockLevel LockLevel)
{
    using namespace SuspenseThreadingPrivate;

    FThreadLockStack* Stack = GetThreadLockStack();
    Stack->Pop(LockLevel);
}

bool FSuspenseLockOrderValidator::CanAcquireLock(ESuspenseLockLevel LockLevel) const
{
    using namespace SuspenseThreadingPrivate;

    FThreadLockStack* Stack = GetThreadLockStack();
    return Stack->CanAcquire(LockLevel);
}

#endif // !UE_BUILD_SHIPPING
