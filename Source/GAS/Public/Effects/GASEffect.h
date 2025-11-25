// Public/Effects/GASEffect.h
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GASEffect.generated.h"

/**
 * Базовый класс для всех GameplayEffect-ов проекта.
 * Добавлять общие теги, стоимости и т.п. будем при необходимости.
 */
UCLASS(Abstract, Blueprintable)
class GAS_API UGASEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// Конструктор по умолчанию
	UGASEffect();
	
	// Конструктор с FObjectInitializer для поддержки дочерних классов,
	// которые используют эту сигнатуру
	UGASEffect(const FObjectInitializer& ObjectInitializer);
};