// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Interfaces/UI/ISuspenseCoreContainerUI.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Blueprint/UserWidget.h" 

USuspenseCoreEventManager* ISuspenseCoreContainerUI::GetDelegateManagerStatic(const UObject* WorldContextObject)
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

    return GameInstance->GetSubsystem<USuspenseCoreEventManager>();
}

void ISuspenseCoreContainerUI::BroadcastContainerUpdateRequest(
    const UObject* Container,
    const FGameplayTag& ContainerType)
{
    if (!Container)
    {
        return;
    }

    USuspenseCoreEventManager* Manager = GetDelegateManagerStatic(Container);
    if (!Manager)
    {
        return;
    }

    // TODO: Migrate to EventBus - old delegate system removed
    // UUserWidget* WidgetContainer = Cast<UUserWidget>(const_cast<UObject*>(Container));
    // if (WidgetContainer)
    // {
    //     Manager->NotifyUIContainerUpdateRequested(WidgetContainer, ContainerType);
    // }
}

void ISuspenseCoreContainerUI::BroadcastSlotInteraction(
    const UObject* Container,
    int32 SlotIndex,
    const FGameplayTag& InteractionType)
{
    if (!Container)
    {
        return;
    }

    USuspenseCoreEventManager* Manager = GetDelegateManagerStatic(Container);
    if (!Manager)
    {
        return;
    }

    // TODO: Migrate to EventBus - old delegate system removed
    // UUserWidget* WidgetContainer = Cast<UUserWidget>(const_cast<UObject*>(Container));
    // if (WidgetContainer)
    // {
    //     Manager->NotifyUISlotInteraction(WidgetContainer, SlotIndex, InteractionType);
    // }
    // Manager->NotifyUIEvent(const_cast<UObject*>(Container), EventTag, EventData);
}