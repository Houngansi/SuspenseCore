// Copyright Suspense Team. All Rights Reserved.

#include "Interfaces/Interaction/ISuspenseInteract.h"
#include "Delegates/SuspenseEventManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

// NOTE: Interface classes cannot provide default implementations for BlueprintNativeEvent methods
// These must be implemented in the classes that implement this interface

USuspenseEventManager* ISuspenseInteract::GetDelegateManagerStatic(const UObject* WorldContextObject)
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

void ISuspenseInteract::BroadcastInteractionStarted(
    const UObject* Interactable,
    APlayerController* Instigator,
    const FGameplayTag& InteractionType)
{
    if (!Interactable || !Instigator)
    {
        return;
    }
    
    USuspenseEventManager* Manager = GetDelegateManagerStatic(Interactable);
    if (Manager)
    {
        // Create event data for interaction start
        FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("Interaction.Event.Started"));
        FString EventData = FString::Printf(TEXT("Type:%s,Instigator:%s"), 
            *InteractionType.ToString(),
            *Instigator->GetName());
            
        Manager->NotifyEquipmentEvent(const_cast<UObject*>(Interactable), EventTag, EventData);
    }
}

void ISuspenseInteract::BroadcastInteractionCompleted(
    const UObject* Interactable,
    APlayerController* Instigator,
    bool bSuccess)
{
    if (!Interactable || !Instigator)
    {
        return;
    }
    
    USuspenseEventManager* Manager = GetDelegateManagerStatic(Interactable);
    if (Manager)
    {
        // Create event data for interaction completion
        FGameplayTag EventTag = bSuccess 
            ? FGameplayTag::RequestGameplayTag(TEXT("Interaction.Event.Success"))
            : FGameplayTag::RequestGameplayTag(TEXT("Interaction.Event.Failed"));
            
        FString EventData = FString::Printf(TEXT("Instigator:%s,Result:%s"), 
            *Instigator->GetName(),
            bSuccess ? TEXT("Success") : TEXT("Failed"));
            
        Manager->NotifyEquipmentEvent(const_cast<UObject*>(Interactable), EventTag, EventData);
    }
}

void ISuspenseInteract::BroadcastInteractionFocusChanged(
    const UObject* Interactable,
    APlayerController* Instigator,
    bool bGainedFocus)
{
    if (!Interactable || !Instigator)
    {
        return;
    }
    
    USuspenseEventManager* Manager = GetDelegateManagerStatic(Interactable);
    if (Manager)
    {
        // Create event data for focus change
        FGameplayTag EventTag = bGainedFocus
            ? FGameplayTag::RequestGameplayTag(TEXT("Interaction.Event.FocusGained"))
            : FGameplayTag::RequestGameplayTag(TEXT("Interaction.Event.FocusLost"));
            
        FString EventData = FString::Printf(TEXT("Instigator:%s"), *Instigator->GetName());
            
        Manager->NotifyEquipmentEvent(const_cast<UObject*>(Interactable), EventTag, EventData);
    }
}