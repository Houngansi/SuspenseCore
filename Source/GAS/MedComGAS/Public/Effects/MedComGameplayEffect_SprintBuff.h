// MedComGameplayEffect_SprintBuff.h
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "MedComGameplayEffect_SprintBuff.generated.h"

/**
 * Эффект увеличения скорости при спринте
 * ВАЖНО: Этот класс служит только как база для Blueprint!
 * Настройка компонентов должна происходить в Blueprint редакторе
 */
UCLASS(BlueprintType)
class MEDCOMGAS_API UMedComGameplayEffect_SprintBuff : public UGameplayEffect
{
	GENERATED_BODY()
    
public:
	UMedComGameplayEffect_SprintBuff();
};