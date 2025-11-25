// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GASEffect.h"
#include "InitialAttributesEffect.generated.h"

/**
 * Эффект инициализации атрибутов персонажа
 * Используется для установки начальных значений атрибутов
 */
UCLASS()
class GAS_API UInitialAttributesEffect : public UMedComGameplayEffect
{
	GENERATED_BODY()
    
public:
	UInitialAttributesEffect();
};