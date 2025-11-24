#pragma once

#include "CoreMinimal.h"
#include "MedComGameplayEffect.h"
#include "MedComGameplayEffect_HealthRegen.generated.h"

/** Периодическая регенерация здоровья (+5 HP/с) */
UCLASS()
class MEDCOMGAS_API UMedComGameplayEffect_HealthRegen : public UMedComGameplayEffect
{
	GENERATED_BODY()

public:
	UMedComGameplayEffect_HealthRegen(const FObjectInitializer& ObjectInitializer);
};
