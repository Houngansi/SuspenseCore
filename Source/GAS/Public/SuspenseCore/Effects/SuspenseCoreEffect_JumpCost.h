// SuspenseCoreEffect_JumpCost.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCoreEffect.h"
#include "SuspenseCoreEffect_JumpCost.generated.h"

/**
 * USuspenseCoreEffect_JumpCost
 *
 * Instant stamina cost effect for jumping.
 * Uses SetByCaller for dynamic stamina cost.
 *
 * Migrated from: UGameplayEffect_JumpCost
 */
UCLASS()
class GAS_API USuspenseCoreEffect_JumpCost final : public USuspenseCoreEffect
{
	GENERATED_BODY()

public:
	USuspenseCoreEffect_JumpCost();
};
