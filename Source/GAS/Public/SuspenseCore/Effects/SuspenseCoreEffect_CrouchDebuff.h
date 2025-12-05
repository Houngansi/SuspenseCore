// SuspenseCoreEffect_CrouchDebuff.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCoreEffect.h"
#include "SuspenseCoreEffect_CrouchDebuff.generated.h"

/**
 * USuspenseCoreEffect_CrouchDebuff
 *
 * Speed reduction effect while crouching.
 * Decreases movement speed by 50% and adds State.Crouching tag.
 *
 * Migrated from: UGameplayEffect_CrouchDebuff
 */
UCLASS(BlueprintType)
class GAS_API USuspenseCoreEffect_CrouchDebuff : public USuspenseCoreEffect
{
	GENERATED_BODY()

public:
	USuspenseCoreEffect_CrouchDebuff();
};
