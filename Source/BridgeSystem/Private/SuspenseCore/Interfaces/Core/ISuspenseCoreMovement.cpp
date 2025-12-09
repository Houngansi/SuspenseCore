// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Interfaces/Core/ISuspenseCoreMovement.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

void ISuspenseCoreMovement::NotifyMovementSpeedChanged(const UObject* Source, float OldSpeed, float NewSpeed, bool bIsSprinting)
{
    if (!Source)
    {
        return;
    }

    USuspenseCoreEventManager* Manager = GetDelegateManagerStatic(Source);
    if (Manager)
    {
        // TODO: Migrate to EventBus - old delegate system removed
        // Manager->NotifyMovementSpeedChanged(OldSpeed, NewSpeed, bIsSprinting);
    }
}

void ISuspenseCoreMovement::NotifyMovementStateChanged(const UObject* Source, FGameplayTag NewState, bool bIsTransitioning)
{
    if (!Source)
    {
        return;
    }

    USuspenseCoreEventManager* Manager = GetDelegateManagerStatic(Source);
    if (Manager)
    {
        // TODO: Migrate to EventBus - old delegate system removed
        // Manager->NotifyMovementStateChanged(NewState, bIsTransitioning);
    }
}

void ISuspenseCoreMovement::NotifyJumpStateChanged(const UObject* Source, bool bIsJumping)
{
    if (!Source)
    {
        return;
    }

    USuspenseCoreEventManager* Manager = GetDelegateManagerStatic(Source);
    if (Manager)
    {
        // TODO: Migrate to EventBus - old delegate system removed
        // Manager->NotifyJumpStateChanged(bIsJumping);
    }
}

void ISuspenseCoreMovement::NotifyCrouchStateChanged(const UObject* Source, bool bIsCrouching)
{
    if (!Source)
    {
        return;
    }

    USuspenseCoreEventManager* Manager = GetDelegateManagerStatic(Source);
    if (Manager)
    {
        // TODO: Migrate to EventBus - old delegate system removed
        // Manager->NotifyCrouchStateChanged(bIsCrouching);
    }
}

void ISuspenseCoreMovement::NotifyLanded(const UObject* Source, float ImpactVelocity)
{
    if (!Source)
    {
        return;
    }

    USuspenseCoreEventManager* Manager = GetDelegateManagerStatic(Source);
    if (Manager)
    {
        // TODO: Migrate to EventBus - old delegate system removed
        // Manager->NotifyLanded(ImpactVelocity);
    }
}

USuspenseCoreEventManager* ISuspenseCoreMovement::GetDelegateManagerStatic(const UObject* WorldContextObject)
{
    if (!WorldContextObject)
    {
        return nullptr;
    }

    return USuspenseCoreEventManager::Get(WorldContextObject);
}