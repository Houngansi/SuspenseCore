// SuspenseCoreEffect_SprintBuff.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCoreEffect.h"
#include "SuspenseCoreEffect_SprintBuff.generated.h"

/**
 * USuspenseCoreEffect_SprintBuff
 *
 * Speed buff effect while sprinting.
 * Increases movement speed by 50%.
 *
 * Migrated from: UGameplayEffect_SprintBuff
 */
UCLASS(BlueprintType)
class GAS_API USuspenseCoreEffect_SprintBuff : public USuspenseCoreEffect
{
	GENERATED_BODY()

public:
	USuspenseCoreEffect_SprintBuff();
};
