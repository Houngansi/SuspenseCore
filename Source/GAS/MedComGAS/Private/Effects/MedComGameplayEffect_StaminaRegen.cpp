#include "Effects/MedComGameplayEffect_StaminaRegen.h"
#include "Attributes/MedComBaseAttributeSet.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"

UMedComGameplayEffect_StaminaRegen::UMedComGameplayEffect_StaminaRegen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period         = 0.1f;   // 10 тиков/с

	/* +1 STA за тик ⇒ +10 STA/с */
	{
		FGameplayModifierInfo Mod;
		// Используем статический метод получения атрибута
		Mod.Attribute = UMedComBaseAttributeSet::GetStaminaAttribute();
		Mod.ModifierOp        = EGameplayModOp::Additive;
		Mod.ModifierMagnitude = FScalableFloat(1.f);
		Modifiers.Add(Mod);
	}

	auto* TagReq = ObjectInitializer.CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(
	   this, TEXT("StaminaRegenTagReq"));

	TagReq->OngoingTagRequirements.IgnoreTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Sprinting")));
	TagReq->OngoingTagRequirements.IgnoreTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Dead")));

	GEComponents.Add(TagReq);
}