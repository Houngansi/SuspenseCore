// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayEffect_CrouchDebuff.generated.h"

/**
 * Эффект уменьшения скорости при приседании
 * Уменьшает скорость движения на 50% и добавляет тег State.Crouching
 * ВАЖНО: Этот класс служит только как база для Blueprint!
 * Дополнительная настройка может происходить в Blueprint редакторе
 */
UCLASS(BlueprintType)
class GAS_API UGameplayEffect_CrouchDebuff : public UGameplayEffect
{
	GENERATED_BODY()
    
public:
	UGameplayEffect_CrouchDebuff();
};