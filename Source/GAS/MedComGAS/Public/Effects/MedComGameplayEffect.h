// Public/Effects/MedComGameplayEffect.h
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "MedComGameplayEffect.generated.h"

/**
 * Базовый класс для всех GameplayEffect-ов проекта.
 * Добавлять общие теги, стоимости и т.п. будем при необходимости.
 */
UCLASS(Abstract, Blueprintable)
class MEDCOMGAS_API UMedComGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// Конструктор по умолчанию
	UMedComGameplayEffect();
	
	// Конструктор с FObjectInitializer для поддержки дочерних классов,
	// которые используют эту сигнатуру
	UMedComGameplayEffect(const FObjectInitializer& ObjectInitializer);
};