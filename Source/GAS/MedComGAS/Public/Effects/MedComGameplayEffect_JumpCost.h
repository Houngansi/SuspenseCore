// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MedComGameplayEffect.h"
#include "MedComGameplayEffect_JumpCost.generated.h"

/**
 * Эффект единоразового расхода стамины при прыжке.
 * 
 * Использует SetByCaller для динамической установки расхода стамины,
 * что позволяет разным способностям или модификаторам влиять на стоимость прыжка.
 */
UCLASS()
class MEDCOMGAS_API UMedComGameplayEffect_JumpCost final : public UMedComGameplayEffect
{
	GENERATED_BODY()

public:
	UMedComGameplayEffect_JumpCost();
};