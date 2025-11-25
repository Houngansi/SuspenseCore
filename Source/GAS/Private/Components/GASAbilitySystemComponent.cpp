// Copyright Suspense Team. All Rights Reserved.

#include "Components/GASAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

UGASAbilitySystemComponent::UGASAbilitySystemComponent()
{
	// По умолчанию используем репликацию эффектов в смешанном режиме
	ReplicationMode = EGameplayEffectReplicationMode::Mixed;
}

void UGASAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);
    
	// Вызываем глобальный инит, нужный некоторым системам GAS
	UAbilitySystemGlobals::Get().InitGlobalData();
}