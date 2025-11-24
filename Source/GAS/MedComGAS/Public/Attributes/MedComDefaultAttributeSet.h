// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/MedComBaseAttributeSet.h"
#include "MedComDefaultAttributeSet.generated.h"

/**
 * Пресет атрибутов по умолчанию для персонажей МедКом
 * Содержит предустановленные значения для стандартного персонажа
 */
UCLASS()
class MEDCOMGAS_API UMedComDefaultAttributeSet : public UMedComBaseAttributeSet
{
	GENERATED_BODY()

public:
	UMedComDefaultAttributeSet();
};