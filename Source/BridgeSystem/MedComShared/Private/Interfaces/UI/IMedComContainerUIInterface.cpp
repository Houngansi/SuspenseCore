// Copyright MedCom Team. All Rights Reserved.

#include "Interfaces/UI/IMedComContainerUIInterface.h"
#include "Delegates/EventDelegateManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Blueprint/UserWidget.h" 

UEventDelegateManager* IMedComContainerUIInterface::GetDelegateManagerStatic(const UObject* WorldContextObject)
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

void IMedComContainerUIInterface::BroadcastContainerUpdateRequest(
    const UObject* Container,
    const FGameplayTag& ContainerType)
{
    if (!Container)
    {
        return;
    }

    UEventDelegateManager* Manager = GetDelegateManagerStatic(Container);
    if (!Manager)
    {
        return;
    }

    // Attempt to cast the UObject* to UUserWidget*
    UUserWidget* WidgetContainer = Cast<UUserWidget>(const_cast<UObject*>(Container));
    if (WidgetContainer)
    {
        // Уведомляем о запросе обновления контейнера, если Container является UUserWidget
        Manager->NotifyUIContainerUpdateRequested(WidgetContainer, ContainerType);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("BroadcastContainerUpdateRequest: Container '%s' is not a UUserWidget. NotifyUIContainerUpdateRequested skipped."), *Container->GetName());
    }
}

void IMedComContainerUIInterface::BroadcastSlotInteraction(
    const UObject* Container,
    int32 SlotIndex,
    const FGameplayTag& InteractionType)
{
    if (!Container)
    {
        return;
    }

    UEventDelegateManager* Manager = GetDelegateManagerStatic(Container);
    if (!Manager)
    {
        return;
    }

    // Attempt to cast the UObject* to UUserWidget* for UI-specific notifications
    UUserWidget* WidgetContainer = Cast<UUserWidget>(const_cast<UObject*>(Container));

    // ИСПРАВЛЕНИЕ: Используем существующие теги из конфигурации (This logic seems fine as per your code)
    FGameplayTag EventTag;
    
    if (InteractionType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Interaction.DoubleClick"))))
    {
        EventTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Interaction.DoubleClick"), false);
    }
    else if (InteractionType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Interaction.RightClick"))))
    {
        EventTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Interaction.RightClick"), false);
    }
    else if (InteractionType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Interaction.Drag"))))
    {
        EventTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Interaction.Drag"), false);
    }
    else if (InteractionType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Interaction.Drop"))))
    {
        EventTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Interaction.Drop"), false);
    }
    else
    {
        EventTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Interaction.Click"), false);
    }
    
    if (!EventTag.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("BroadcastSlotInteraction: No valid event tag found for interaction type %s. Using fallback."), 
            *InteractionType.ToString());
        EventTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Event.ContainerUpdated"), false); // Fallback
        
        if (!EventTag.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("BroadcastSlotInteraction: Failed to find any valid UI event tag! Aborting notification."));
            return;
        }
    }

    FString EventData = FString::Printf(
        TEXT("Container:%s,Slot:%d,Interaction:%s"), 
        *Container->GetName(),
        SlotIndex,
        *InteractionType.ToString()
    );
    
    if (WidgetContainer)
    {
        // Отправляем уведомление через менеджер делегатов, если Container является UUserWidget
        Manager->NotifyUISlotInteraction(WidgetContainer, SlotIndex, InteractionType);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("BroadcastSlotInteraction: Container '%s' is not a UUserWidget. NotifyUISlotInteraction skipped."), *Container->GetName());
    }
    
    // Предполагаем, что NotifyUIEvent может принимать UObject* или имеет другую перегрузку,
    // так как он не был указан в контексте ошибки преобразования к UUserWidget*.
    // Если NotifyUIEvent также требует UUserWidget*, примените аналогичный cast.
    Manager->NotifyUIEvent(const_cast<UObject*>(Container), EventTag, EventData);
    
    UE_LOG(LogTemp, Verbose, TEXT("BroadcastSlotInteraction: Container=%s, Slot=%d, Type=%s, EventTag=%s"), 
        *Container->GetName(), SlotIndex, *InteractionType.ToString(), *EventTag.ToString());
}