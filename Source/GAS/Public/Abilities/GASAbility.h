#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Input/MCAbilityInputID.h"
#include "GASAbility.generated.h"

UCLASS(Abstract, Blueprintable)
class GAS_API UGASAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGASAbility();

	/** Клавиша / действие, которое активирует способность (см. MCAbilityInputID) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GAS|Ability")
	EMCAbilityInputID AbilityInputID = EMCAbilityInputID::None;
};
