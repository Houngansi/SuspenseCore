#pragma once

#include "CoreMinimal.h"
#include "MedComGameplayEffect.h"
#include "MedComGameplayEffect_StaminaRegen.generated.h"

/** Периодическая регенерация стамины (+10 STA/с) */
UCLASS()
class MEDCOMGAS_API UMedComGameplayEffect_StaminaRegen : public UMedComGameplayEffect
{
	GENERATED_BODY()

public:
	UMedComGameplayEffect_StaminaRegen(const FObjectInitializer& ObjectInitializer);
};
