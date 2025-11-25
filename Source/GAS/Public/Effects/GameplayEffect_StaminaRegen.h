#pragma once

#include "CoreMinimal.h"
#include "GASEffect.h"
#include "GameplayEffect_StaminaRegen.generated.h"

/** Периодическая регенерация стамины (+10 STA/с) */
UCLASS()
class GAS_API UGameplayEffect_StaminaRegen : public UGASEffect
{
	GENERATED_BODY()

public:
	UGameplayEffect_StaminaRegen(const FObjectInitializer& ObjectInitializer);
};
