#include "Core/AbilitySystem/Abilities/WeaponAbilities/MedComSpreadProcessor.h"
#include "Core/AbilitySystem/Attributes/MedComWeaponAttributeSet.h"
#include "Math/UnrealMathUtility.h"

float UMedComSpreadProcessor::CalculateCurrentSpread(const UMedComWeaponAttributeSet* WeaponAttributes, bool bIsAiming, float MovementSpeed, float RecoilModifier) const
{
	if (!WeaponAttributes)
	{
		return 0.f;
	}
    
	// Базовый разброс из атрибутов
	float BaseSpread = WeaponAttributes->GetSpread();
    
	// Если оружие прицеливается – уменьшаем разброс
	float AimModifier = bIsAiming ? 0.5f : 1.f;
    
	// При движении разброс увеличивается; например, на 1% за каждую единицу скорости
	float MovementModifier = 1.f + (MovementSpeed * 0.01f);
    
	// Итоговый разброс с учетом отдачи
	float CurrentSpread = BaseSpread * AimModifier * MovementModifier * RecoilModifier;
    
	return CurrentSpread;
}
