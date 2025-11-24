// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MedComGameplayEffect.h"
#include "MedComInitialAttributesEffect.generated.h"

/**
 * Эффект инициализации атрибутов персонажа
 * Используется для установки начальных значений атрибутов
 */
UCLASS()
class MEDCOMGAS_API UMedComInitialAttributesEffect : public UMedComGameplayEffect
{
	GENERATED_BODY()
    
public:
	UMedComInitialAttributesEffect();
};