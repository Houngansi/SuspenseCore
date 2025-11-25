// Copyright Suspense Team. All Rights Reserved.

#include "Interfaces/Abilities/ISuspenseAbilityProvider.h"
#include "Delegates/SuspenseEventManager.h"
#include "GameplayEffect.h"
#include "GameplayAbilities/Public/AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

USuspenseEventManager* ISuspenseAbilityProvider::GetDelegateManagerStatic(const UObject* WorldContextObject)
{
    if (!WorldContextObject)
    {
        return nullptr;
    }
    
    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        return nullptr;
    }
    
    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance)
    {
        return nullptr;
    }
    
    return GameInstance->GetSubsystem<USuspenseEventManager>();
}

void ISuspenseAbilityProvider::BroadcastAbilityGranted(const UObject* Provider, FGameplayAbilitySpecHandle AbilityHandle, TSubclassOf<UGameplayAbility> AbilityClass)
{
    if (!Provider || !AbilityHandle.IsValid() || !AbilityClass)
    {
        return;
    }
    
    USuspenseEventManager* Manager = GetDelegateManagerStatic(Provider);
    if (Manager)
    {
        // Notify about ability granted - using class name instead of private handle
        FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("AbilitySystem.Event.AbilityGranted"));
        FString EventData = FString::Printf(TEXT("Ability:%s,Valid:%s"), 
            *AbilityClass->GetName(), 
            AbilityHandle.IsValid() ? TEXT("true") : TEXT("false"));
            
        Manager->NotifyEquipmentEvent(const_cast<UObject*>(Provider), EventTag, EventData);
    }
}

void ISuspenseAbilityProvider::BroadcastEffectApplied(const UObject* Provider, FActiveGameplayEffectHandle EffectHandle, TSubclassOf<UGameplayEffect> EffectClass)
{
    if (!Provider || !EffectHandle.IsValid() || !EffectClass)
    {
        return;
    }
    
    USuspenseEventManager* Manager = GetDelegateManagerStatic(Provider);
    if (Manager)
    {
        // Notify about effect applied - using class name instead of private handle
        FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("AbilitySystem.Event.EffectApplied"));
        FString EventData = FString::Printf(TEXT("Effect:%s,Valid:%s"), 
            *EffectClass->GetName(), 
            EffectHandle.IsValid() ? TEXT("true") : TEXT("false"));
            
        Manager->NotifyEquipmentEvent(const_cast<UObject*>(Provider), EventTag, EventData);
    }
}