// SuspenseCoreEffect.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "SuspenseCoreEffect.generated.h"

/**
 * USuspenseCoreEffect
 *
 * Base class for all SuspenseCore GameplayEffects.
 * Uses USuspenseCoreAttributeSet instead of legacy UGASAttributeSet.
 *
 * This is the NEW SuspenseCore effect base class.
 * Legacy: UGASEffect (not touched)
 *
 * FEATURES:
 * - EventBus-ready effect architecture
 * - Uses SuspenseCore attribute accessors
 * - Common tags and configurations
 */
UCLASS(Abstract, Blueprintable)
class GAS_API USuspenseCoreEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USuspenseCoreEffect();
	USuspenseCoreEffect(const FObjectInitializer& ObjectInitializer);
};
