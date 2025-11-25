// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GASEffect.h"
#include "GameplayEffect_JumpCost.generated.h"

/**
 * Эффект единоразового расхода стамины при прыжке.
 * 
 * Использует SetByCaller для динамической установки расхода стамины,
 * что позволяет разным способностям или модификаторам влиять на стоимость прыжка.
 */
UCLASS()
class GAS_API UGameplayEffect_JumpCost final : public UGASEffect
{
	GENERATED_BODY()

public:
	UGameplayEffect_JumpCost();
};