// SuspenseCoreEffect_StaminaRegen.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCoreEffect.h"
#include "SuspenseCoreEffect_StaminaRegen.generated.h"

/**
 * USuspenseCoreEffect_StaminaRegen
 *
 * Periodic stamina regeneration effect.
 * Regenerates +10 STA per second (1 per 0.1s tick).
 * Blocked while sprinting or dead.
 *
 * Migrated from: UGameplayEffect_StaminaRegen
 */
UCLASS()
class GAS_API USuspenseCoreEffect_StaminaRegen : public USuspenseCoreEffect
{
	GENERATED_BODY()

public:
	USuspenseCoreEffect_StaminaRegen(const FObjectInitializer& ObjectInitializer);
};
