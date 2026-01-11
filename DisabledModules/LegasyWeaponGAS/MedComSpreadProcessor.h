#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Core/AbilitySystem/Attributes/MedComWeaponAttributeSet.h"
#include "MedComSpreadProcessor.generated.h"

/**
 * UMedComSpreadProcessor
 * Класс для расчета текущего разброса оружия.
 * Может учитывать базовый разброс, состояние прицеливания, скорость движения и отдачу.
 */
UCLASS(Blueprintable)
class MEDCOMCORE_API UMedComSpreadProcessor : public UObject
{
	GENERATED_BODY()

public:
	// Рассчитывает итоговый разброс
	UFUNCTION(BlueprintCallable, Category="Weapon|Spread")
	float CalculateCurrentSpread(const UMedComWeaponAttributeSet* WeaponAttributes, bool bIsAiming, float MovementSpeed, float RecoilModifier = 1.f) const;
};
