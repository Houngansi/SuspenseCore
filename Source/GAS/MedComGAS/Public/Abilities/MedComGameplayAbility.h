#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Input/MCAbilityInputID.h"
#include "MedComGameplayAbility.generated.h"

UCLASS(Abstract, Blueprintable)
class MEDCOMGAS_API UMedComGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UMedComGameplayAbility();

	/** Клавиша / действие, которое активирует способность (см. MCAbilityInputID) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MedCom|Ability")
	EMCAbilityInputID AbilityInputID = EMCAbilityInputID::None;
};
