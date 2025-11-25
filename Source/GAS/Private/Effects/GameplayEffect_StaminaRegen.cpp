#include "Effects/GameplayEffect_StaminaRegen.h"
#include "Attributes/GASAttributeSet.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"

UGameplayEffect_StaminaRegen::UGameplayEffect_StaminaRegen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period         = 0.1f;   // 10 тиков/с

	/* +1 STA за тик ⇒ +10 STA/с */
	{
		FGameplayModifierInfo Mod;
		// Используем статический метод получения атрибута
		Mod.Attribute = UGASAttributeSet::GetStaminaAttribute();
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