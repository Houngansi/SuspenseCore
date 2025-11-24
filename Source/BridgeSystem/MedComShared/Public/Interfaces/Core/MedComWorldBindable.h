// MedComWorldBindable.h
// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MedComWorldBindable.generated.h"

/**
 * Interface for services/components that must rebind to current UWorld
 * after map loads, seamless travel, or world transitions.
 * 
 * CRITICAL: This interface ensures services don't hold stale world references
 * after travel events, preventing crashes and ensuring correct subsystem access.
 * 
 * Implementation Notes:
 * - RebindWorld must be idempotent (safe to call multiple times)
 * - RebindWorld must be called on GameThread only
 * - Typical use: update cached World*, re-acquire WorldSubsystems, rebind event dispatchers
 * 
 * Usage Example:
 * @code
 * class UMyService : public UObject, public IMedComWorldBindable
 * {
 *     virtual void RebindWorld_Implementation(UWorld* NewWorld) override
 *     {
 *         check(IsInGameThread());
 *         CachedWorld = NewWorld;
 *         MySubsystem = NewWorld ? NewWorld->GetSubsystem<UMySubsystem>() : nullptr;
 *     }
 * };
 * @endcode
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UMedComWorldBindable : public UInterface
{
    GENERATED_BODY()
};

class MEDCOMSHARED_API IMedComWorldBindable
{
    GENERATED_BODY()

public:
    /**
     * Rebind internal world-dependent pointers/contexts to the provided World.
     * Must be idempotent and GameThread-safe.
     * 
     * @param NewWorld - The world to rebind to. Can be nullptr during shutdown.
     */
    UFUNCTION(BlueprintNativeEvent, Category="MedCom|World")
    void RebindWorld(UWorld* NewWorld);
    virtual void RebindWorld_Implementation(UWorld* NewWorld) PURE_VIRTUAL(IMedComWorldBindable::RebindWorld_Implementation,);

    /**
     * Optional readiness probe after rebind.
     * @return true if service is ready to operate with current world binding
     */
    UFUNCTION(BlueprintNativeEvent, Category="MedCom|World")
    bool IsWorldBoundReady() const;
    virtual bool IsWorldBoundReady_Implementation() const { return true; }
};