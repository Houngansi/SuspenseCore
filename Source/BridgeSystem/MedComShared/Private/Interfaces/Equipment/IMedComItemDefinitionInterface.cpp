// Copyright MedCom Team. All Rights Reserved.

#include "Interfaces/Equipment/IMedComItemDefinitionInterface.h"
#include "Delegates/EventDelegateManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

UEventDelegateManager* IMedComItemDefinitionInterface::GetDelegateManagerStatic(const UObject* WorldContextObject)
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
    
    return GameInstance->GetSubsystem<UEventDelegateManager>();
}