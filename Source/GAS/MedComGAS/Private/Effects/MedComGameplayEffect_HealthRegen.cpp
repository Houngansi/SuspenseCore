#include "Effects/MedComGameplayEffect_HealthRegen.h"
#include "Attributes/MedComBaseAttributeSet.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"

UMedComGameplayEffect_HealthRegen::UMedComGameplayEffect_HealthRegen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	/* ∞-длительность, 10 тиков/с */
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period         = 0.1f;

	/* +0.5 HP за тик ⇒ +5 HP/с */
	{
		FGameplayModifierInfo Mod;
		// Используем статический метод получения атрибута
		Mod.Attribute = UMedComBaseAttributeSet::GetHealthAttribute();
		Mod.ModifierOp        = EGameplayModOp::Additive;
		Mod.ModifierMagnitude = FScalableFloat(0.5f);
		Modifiers.Add(Mod);
	}

	/* Создаём компонент-фильтр через CreateDefaultSubobject */
	auto* TagReq = ObjectInitializer.CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(
		this, TEXT("HealthRegenTagReq"));

	TagReq->OngoingTagRequirements.IgnoreTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Sprinting")));
	TagReq->OngoingTagRequirements.IgnoreTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Dead")));

	/* Обязательно зарегистрировать компонент вручную */
	GEComponents.Add(TagReq);
}