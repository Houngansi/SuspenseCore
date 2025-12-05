// SuspenseCoreEffect_HealthRegen.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCoreEffect.h"
#include "SuspenseCoreEffect_HealthRegen.generated.h"

/**
 * USuspenseCoreEffect_HealthRegen
 *
 * Periodic health regeneration effect.
 * Regenerates +5 HP per second (0.5 per 0.1s tick).
 * Blocked while sprinting or dead.
 *
 * Migrated from: UGameplayEffect_HealthRegen
 */
UCLASS()
class GAS_API USuspenseCoreEffect_HealthRegen : public USuspenseCoreEffect
{
	GENERATED_BODY()

public:
	USuspenseCoreEffect_HealthRegen(const FObjectInitializer& ObjectInitializer);
};
