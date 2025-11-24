// Copyright MedCom Team. All Rights Reserved.

#include "Components/MedComAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

UMedComAbilitySystemComponent::UMedComAbilitySystemComponent()
{
	// По умолчанию используем репликацию эффектов в смешанном режиме
	ReplicationMode = EGameplayEffectReplicationMode::Mixed;
}

void UMedComAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);
    
	// Вызываем глобальный инит, нужный некоторым системам GAS
	UAbilitySystemGlobals::Get().InitGlobalData();
}