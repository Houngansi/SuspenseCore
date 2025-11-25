#pragma once

#include "CoreMinimal.h"
#include "GASEffect.h"
#include "GameplayEffect_HealthRegen.generated.h"

/** Периодическая регенерация здоровья (+5 HP/с) */
UCLASS()
class GAS_API UGameplayEffect_HealthRegen : public UMedComGameplayEffect
{
	GENERATED_BODY()

public:
	UGameplayEffect_HealthRegen(const FObjectInitializer& ObjectInitializer);
};
