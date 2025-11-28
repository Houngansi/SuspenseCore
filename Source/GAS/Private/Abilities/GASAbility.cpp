#include "Abilities/GASAbility.h"

UGASAbility::UGASAbility()
{
	// Создаём по-актерному экземпляру: так безопаснее с переменными состояния
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// По умолчанию предсказываем локально; при необходимости переопределяйте в потомках
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// Никакой клавише не привязано, пока дизайнер не задаст в БП-потомке
	AbilityInputID = ESuspenseAbilityInputID::None;
}
