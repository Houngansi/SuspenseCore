// SuspenseCoreEffect_SprintCost.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCoreEffect.h"
#include "SuspenseCoreEffect_SprintCost.generated.h"

/**
 * USuspenseCoreEffect_SprintCost
 *
 * Periodic stamina drain effect while sprinting.
 * Drains 10 stamina per second (1 per 0.1s tick).
 *
 * Migrated from: UGameplayEffect_SprintCost
 */
UCLASS()
class GAS_API USuspenseCoreEffect_SprintCost final : public USuspenseCoreEffect
{
	GENERATED_BODY()

public:
	USuspenseCoreEffect_SprintCost();
};
