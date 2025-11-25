// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GASAbilitySystemComponent.generated.h"

/**
 * Собственный компонент AbilitySystem для GAS
 * Добавляет дополнительный функционал к стандартному UAbilitySystemComponent
 */
UCLASS()
class GAS_API UGASAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UGASAbilitySystemComponent();

	// Вызывается для инициализации компонента
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;
    
	// Переопределение метода для проверки активации способности
	virtual bool ShouldDoServerAbilityRPCBatch() const override { return true; }
};