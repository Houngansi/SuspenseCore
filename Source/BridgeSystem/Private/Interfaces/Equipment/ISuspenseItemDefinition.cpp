// Copyright Suspense Team. All Rights Reserved.

#include "Interfaces/Equipment/ISuspenseItemDefinition.h"
#include "Delegates/SuspenseEventManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

USuspenseEventManager* ISuspenseItemDefinition::GetDelegateManagerStatic(const UObject* WorldContextObject)
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