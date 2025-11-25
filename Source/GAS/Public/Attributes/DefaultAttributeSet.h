// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/GASAttributeSet.h"
#include "DefaultAttributeSet.generated.h"

/**
 * Пресет атрибутов по умолчанию для персонажей МедКом
 * Содержит предустановленные значения для стандартного персонажа
 */
UCLASS()
class GAS_API UDefaultAttributeSet : public UGASAttributeSet
{
	GENERATED_BODY()

public:
	UDefaultAttributeSet();
};