// SuspenseCoreEffect_InitialAttributes.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCoreEffect.h"
#include "SuspenseCoreEffect_InitialAttributes.generated.h"

/**
 * USuspenseCoreEffect_InitialAttributes
 *
 * Character attribute initialization effect.
 * Sets initial values for all SuspenseCore attributes.
 *
 * CRITICAL: This is the ONLY place where initial attribute values should be set!
 * Do not set initial values in AttributeSet constructor.
 *
 * Migrated from: UInitialAttributesEffect
 */
UCLASS()
class GAS_API USuspenseCoreEffect_InitialAttributes : public USuspenseCoreEffect
{
	GENERATED_BODY()

public:
	USuspenseCoreEffect_InitialAttributes();
};
