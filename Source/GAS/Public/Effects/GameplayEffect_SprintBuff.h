// GameplayEffect_SprintBuff.h
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayEffect_SprintBuff.generated.h"

/**
 * Эффект увеличения скорости при спринте
 * ВАЖНО: Этот класс служит только как база для Blueprint!
 * Настройка компонентов должна происходить в Blueprint редакторе
 */
UCLASS(BlueprintType)
class GAS_API UGameplayEffect_SprintBuff : public UGameplayEffect
{
	GENERATED_BODY()
    
public:
	UGameplayEffect_SprintBuff();
};