// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "MedComGameplayEffect_CrouchDebuff.generated.h"

/**
 * Эффект уменьшения скорости при приседании
 * Уменьшает скорость движения на 50% и добавляет тег State.Crouching
 * ВАЖНО: Этот класс служит только как база для Blueprint!
 * Дополнительная настройка может происходить в Blueprint редакторе
 */
UCLASS(BlueprintType)
class MEDCOMGAS_API UMedComGameplayEffect_CrouchDebuff : public UGameplayEffect
{
	GENERATED_BODY()
    
public:
	UMedComGameplayEffect_CrouchDebuff();
};