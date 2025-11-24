// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "MedComAbilitySystemComponent.generated.h"

/**
 * Собственный компонент AbilitySystem для проекта MedCom
 * Добавляет дополнительный функционал к стандартному UAbilitySystemComponent
 */
UCLASS()
class MEDCOMGAS_API UMedComAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UMedComAbilitySystemComponent();

	// Вызывается для инициализации компонента
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;
    
	// Переопределение метода для проверки активации способности
	virtual bool ShouldDoServerAbilityRPCBatch() const override { return true; }
};