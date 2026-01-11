#include "Core/AbilitySystem/Abilities/WeaponAbilities/MedComDamageEffect.h"
#include "Core/AbilitySystem/Attributes/MedComBaseAttributeSet.h"
#include "Engine/DamageEvents.h"      // Для FDamageEvent
#include "GameplayEffectTypes.h"       // Для FGameplayEffectModifierMagnitude и FSetByCallerFloat

UMedComDamageEffect::UMedComDamageEffect()
{
	// Эффект мгновенный
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Создаем модификатор для атрибута Health
	FGameplayModifierInfo DamageModifier;

	// Находим свойство Health в UMedComBaseAttributeSet
	FProperty* HealthProperty = FindFProperty<FProperty>(
		UMedComBaseAttributeSet::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UMedComBaseAttributeSet, Health)
	);
	DamageModifier.Attribute = FGameplayAttribute(HealthProperty);
	DamageModifier.ModifierOp = EGameplayModOp::Additive; // Health = Health + X

	// Настраиваем модификатор через SetByCaller:
	// 1. Создаем экземпляр FSetByCallerFloat и задаем тег "Data.Damage"
	FSetByCallerFloat SetByCallerData;
	SetByCallerData.DataTag = FGameplayTag::RequestGameplayTag("Data.Damage");

	// 2. Создаем FGameplayEffectModifierMagnitude с типом SetByCaller, используя конструктор
	FGameplayEffectModifierMagnitude ModifierMagnitude(SetByCallerData);
	// Обратите внимание: чтобы вычитать урон, значение, передаваемое через этот тег в способности, должно быть отрицательным.

	DamageModifier.ModifierMagnitude = ModifierMagnitude;

	// Добавляем модификатор в список
	Modifiers.Add(DamageModifier);

	// Добавляем тег для идентификации эффекта (например, для использования в GameplayCues)
	// Убедитесь, что вашему активу назначен UAssetTagsGameplayEffectComponent в редакторе.
	GetMutableAssetTags().AddTag(FGameplayTag::RequestGameplayTag("Effect.Damage"));
}
