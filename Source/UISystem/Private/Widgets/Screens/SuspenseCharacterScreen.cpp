// MedComCharacterScreen.cpp
// Copyright Suspense Team Team. All Rights Reserved.

#include "Widgets/Screens/SuspenseCharacterScreen.h"
#include "Widgets/Tabs/SuspenseUpperTabBar.h"
#include "SuspenseCore/Widgets/Inventory/SuspenseCoreInventoryWidget.h"
// TODO: Create SuspenseEquipmentContainerWidget - Equipment widget is not yet implemented
#include "SuspenseCore/Interfaces/Tabs/ISuspenseCoreTabBar.h"
#include "SuspenseCore/Interfaces/Screens/ISuspenseCoreScreen.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"

USuspenseCharacterScreen::USuspenseCharacterScreen(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Set default screen tag using native tags
    ScreenTag = SuspenseCoreTags::UI::Screen::Character;

    // Default to inventory tab
    DefaultTabTag = SuspenseCoreTags::UI::Tab::Inventory;

    bRememberLastTab = true;
    bIsActive = false;
}

void USuspenseCharacterScreen::InitializeWidget_Implementation()
{
    Super::InitializeWidget_Implementation();

    if (!UpperTabBar)
    {
        UE_LOG(LogTemp, Error, TEXT("[CharacterScreen] UpperTabBar not bound!"));
        return;
    }

    // Initialize tab bar through interface
    if (ISuspenseCoreTabBar* TabBarInterface = Cast<ISuspenseCoreTabBar>(UpperTabBar))
    {
        // Subscribe to tab selection changes - это обычные C++ методы, не BlueprintNativeEvent
        if (FOnTabBarSelectionChanged* SelectionDelegate = TabBarInterface->GetOnTabSelectionChanged())
        {
            TabSelectionChangeHandle = SelectionDelegate->AddUObject(this, &USuspenseCharacterScreen::OnTabSelectionChanged);
        }

        // Subscribe to tab bar close events
        if (FOnTabBarClosed* ClosedDelegate = TabBarInterface->GetOnTabBarClosed())
        {
            TabBarCloseHandle = ClosedDelegate->AddUObject(this, &USuspenseCharacterScreen::OnTabBarClosed);
        }
    }

    // Verify tabs are properly configured
    int32 TabCount = ISuspenseCoreTabBar::Execute_GetTabCount(UpperTabBar);
    UE_LOG(LogTemp, Log, TEXT("[CharacterScreen] Tab bar initialized with %d tabs"), TabCount);

    // Log all tabs for debugging
    for (int32 i = 0; i < TabCount; i++)
    {
        if (USuspenseUpperTabBar* TabBarWidget = Cast<USuspenseUpperTabBar>(UpperTabBar))
        {
            FSuspenseTabConfig TabConfig = TabBarWidget->GetTabConfig(i);
            UE_LOG(LogTemp, Log, TEXT("[CharacterScreen] Tab[%d]: %s (%s)"),
                i, *TabConfig.TabName.ToString(), *TabConfig.TabTag.ToString());

            // Verify content widgets
            UUserWidget* TabContent = ISuspenseCoreTabBar::Execute_GetTabContent(UpperTabBar, i);
            if (TabContent)
            {
                FString ContentType = TEXT("Unknown");
                FString ClassName = TabContent->GetClass()->GetName();

                // Check content type by class name
                if (ClassName.Contains(TEXT("Inventory")))
                {
                    ContentType = TEXT("InventoryWidget");
                }
                else if (ClassName.Contains(TEXT("Equipment")))
                {
                    ContentType = TEXT("EquipmentWidget");
                }
                else if (TabContent->GetClass()->ImplementsInterface(USuspenseCoreScreen::StaticClass()))
                {
                    FGameplayTag ContentScreenTag = ISuspenseCoreScreen::Execute_GetScreenTag(TabContent);
                    ContentType = FString::Printf(TEXT("Screen: %s"), *ContentScreenTag.ToString());
                }

                UE_LOG(LogTemp, Log, TEXT("[CharacterScreen] Tab[%d] content: %s (%s)"),
                    i, *TabContent->GetClass()->GetName(), *ContentType);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[CharacterScreen] Tab[%d] has no content widget!"), i);
            }
        }
    }

    // Select default tab if specified
    if (DefaultTabTag.IsValid())
    {
        SelectTabByTag(DefaultTabTag);
    }
    else
    {
        // Select first tab by default
        if (TabCount > 0)
        {
            ISuspenseCoreTabBar::Execute_SelectTabByIndex(UpperTabBar, 0);
        }
    }

    // Initialize screen state
    UpdateInputMode();

    UE_LOG(LogTemp, Log, TEXT("[CharacterScreen] Widget initialization completed"));
}

void USuspenseCharacterScreen::UninitializeWidget_Implementation()
{
    // Unsubscribe from tab bar events
    if (UpperTabBar)
    {
        if (ISuspenseCoreTabBar* TabBarInterface = Cast<ISuspenseCoreTabBar>(UpperTabBar))
        {
            if (FOnTabBarSelectionChanged* SelectionDelegate = TabBarInterface->GetOnTabSelectionChanged())
            {
                SelectionDelegate->Remove(TabSelectionChangeHandle);
            }

            if (FOnTabBarClosed* CloseDelegate = TabBarInterface->GetOnTabBarClosed())
            {
                CloseDelegate->Remove(TabBarCloseHandle);
            }
        }
    }

    Super::UninitializeWidget_Implementation();
}

void USuspenseCharacterScreen::OnScreenActivated_Implementation()
{
    if (bIsActive)
    {
        return;
    }

    bIsActive = true;

    // Determine which tab to open
    FGameplayTag TabToOpen = DefaultTabTag;

    if (bRememberLastTab && LastOpenedTab.IsValid())
    {
        TabToOpen = LastOpenedTab;
    }

    // Open the tab
    if (TabToOpen.IsValid())
    {
        OpenTabByTag(TabToOpen);
    }

    // Call Blueprint event
    K2_OnCharacterScreenOpened();

    // Notify event system through EventBus
    if (USuspenseCoreEventBus* EventBus = GetEventBus())
    {
        // Publish screen activated event
        FSuspenseCoreEventData ActivatedData = FSuspenseCoreEventData::Create(this);
        ActivatedData.SetObject(TEXT("Screen"), this);
        ActivatedData.SetString(TEXT("ScreenTag"), ScreenTag.ToString());

        FGameplayTag ScreenActivatedTag = FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.UI.Screen.Activated"));
        EventBus->Publish(ScreenActivatedTag, ActivatedData);

        // Publish character screen opened event
        FSuspenseCoreEventData OpenedData = FSuspenseCoreEventData::Create(this);
        FGameplayTag OpenedTag = FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.UI.CharacterScreen.Opened"));
        EventBus->Publish(OpenedTag, OpenedData);

        // Force refresh active tab content after a short delay
        FTimerHandle RefreshHandle;
        GetWorld()->GetTimerManager().SetTimer(RefreshHandle, [this]()
        {
            if (UpperTabBar)
            {
                UpperTabBar->RefreshActiveTabContent();

                // Refresh content of active tab
                int32 CurrentIndex = UpperTabBar->GetSelectedTabIndex_Implementation();
                if (CurrentIndex >= 0)
                {
                    FSuspenseTabConfig TabConfig = UpperTabBar->GetTabConfig(CurrentIndex);
                    if (UUserWidget* TabContent = UpperTabBar->GetTabContent_Implementation(CurrentIndex))
                    {
                        // Use interface to refresh content instead of direct class access
                        if (TabContent->GetClass()->ImplementsInterface(USuspenseCoreScreen::StaticClass()))
                        {
                            ISuspenseCoreScreen::Execute_RefreshScreenContent(TabContent);
                        }
                    }
                }
            }
        }, 0.2f, false);
    }

    UE_LOG(LogTemp, Log, TEXT("[CharacterScreen] Activated"));
}

void USuspenseCharacterScreen::OnScreenDeactivated_Implementation()
{
    if (!bIsActive)
    {
        return;
    }

    bIsActive = false;

    // Remember current tab if enabled
    if (bRememberLastTab && UpperTabBar)
    {
        int32 CurrentIndex = UpperTabBar->GetSelectedTabIndex_Implementation();
        FSuspenseTabConfig TabConfig = UpperTabBar->GetTabConfig(CurrentIndex);
        if (TabConfig.TabTag.IsValid())
        {
            LastOpenedTab = TabConfig.TabTag;
        }
    }

    // Call Blueprint event
    K2_OnCharacterScreenClosed();

    // Notify event system through EventBus
    if (USuspenseCoreEventBus* EventBus = GetEventBus())
    {
        // Publish screen deactivated event
        FSuspenseCoreEventData DeactivatedData = FSuspenseCoreEventData::Create(this);
        DeactivatedData.SetObject(TEXT("Screen"), this);
        DeactivatedData.SetString(TEXT("ScreenTag"), ScreenTag.ToString());

        FGameplayTag ScreenDeactivatedTag = FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.UI.Screen.Deactivated"));
        EventBus->Publish(ScreenDeactivatedTag, DeactivatedData);

        // Publish character screen closed event
        FSuspenseCoreEventData ClosedData = FSuspenseCoreEventData::Create(this);
        FGameplayTag ClosedTag = FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.UI.CharacterScreen.Closed"));
        EventBus->Publish(ClosedTag, ClosedData);
    }

    UE_LOG(LogTemp, Log, TEXT("[CharacterScreen] Deactivated"));
}

void USuspenseCharacterScreen::UpdateScreen_Implementation(float DeltaTime)
{
    // Character screen doesn't need tick updates by default
}

void USuspenseCharacterScreen::RefreshScreenContent_Implementation()
{
    // Refresh active tab content
    if (UpperTabBar)
    {
        UpperTabBar->RefreshActiveTabContent();
    }
}

void USuspenseCharacterScreen::OpenTabByTag(const FGameplayTag& TabTag)
{
    if (!UpperTabBar)
    {
        UE_LOG(LogTemp, Error, TEXT("[CharacterScreen] No UpperTabBar found"));
        return;
    }

    // Use SelectTabByTag directly
    bool bSuccess = UpperTabBar->SelectTabByTag(TabTag);

    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("[CharacterScreen] Successfully opened tab: %s"),
            *TabTag.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[CharacterScreen] Failed to open tab: %s"),
            *TabTag.ToString());

        // Fallback: try to select first tab
        if (UpperTabBar->GetTabCount_Implementation() > 0)
        {
            UpperTabBar->SelectTabByIndex_Implementation(0);
            UE_LOG(LogTemp, Log, TEXT("[CharacterScreen] Selected first tab as fallback"));
        }
    }
}

void USuspenseCharacterScreen::OpenTabByIndex(int32 TabIndex)
{
    if (UpperTabBar)
    {
        UpperTabBar->SelectTabByIndex_Implementation(TabIndex);
    }
}

bool USuspenseCharacterScreen::SelectTabByTag(const FGameplayTag& TabTag)
{
    if (!UpperTabBar)
    {
        return false;
    }

    return UpperTabBar->SelectTabByTag(TabTag);
}

void USuspenseCharacterScreen::OnTabSelectionChanged(UObject* TabBar, int32 OldIndex, int32 NewIndex)
{
    if (TabBar != UpperTabBar)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[CharacterScreen] Tab selection changed from %d to %d"), OldIndex, NewIndex);

    // Remember new tab
    if (bRememberLastTab && UpperTabBar && NewIndex >= 0)
    {
        FSuspenseTabConfig TabConfig = UpperTabBar->GetTabConfig(NewIndex);
        if (TabConfig.TabTag.IsValid())
        {
            LastOpenedTab = TabConfig.TabTag;
        }
    }
}

void USuspenseCharacterScreen::OnTabBarClosed(UObject* TabBar)
{
    if (TabBar == UpperTabBar)
    {
        // Hide the character screen
        SetVisibility(ESlateVisibility::Collapsed);

        // Notify about close through EventBus
        if (USuspenseCoreEventBus* EventBus = GetEventBus())
        {
            FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
            FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.UI.CharacterScreen.Closed"));
            EventBus->Publish(EventTag, EventData);
        }
    }
}

int32 USuspenseCharacterScreen::FindTabIndexByTag(const FGameplayTag& TabTag) const
{
    if (!UpperTabBar)
    {
        return -1;
    }

    for (int32 i = 0; i < UpperTabBar->GetTabCount_Implementation(); i++)
    {
        FSuspenseTabConfig Config = UpperTabBar->GetTabConfig(i);
        if (Config.TabTag.MatchesTagExact(TabTag))
        {
            return i;
        }
    }

    return -1;
}

void USuspenseCharacterScreen::UpdateInputMode()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (bIsActive)
        {
            // Set UI and game input mode
            FInputModeGameAndUI InputMode;
            InputMode.SetWidgetToFocus(TakeWidget());
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PC->SetInputMode(InputMode);
            PC->bShowMouseCursor = true;
        }
        else
        {
            // Return to game only mode
            FInputModeGameOnly InputMode;
            PC->SetInputMode(InputMode);
            PC->bShowMouseCursor = false;
        }
    }
}

USuspenseCoreEventBus* USuspenseCharacterScreen::GetEventBus() const
{
    if (USuspenseCoreEventManager* EventManager = GetDelegateManager())
    {
        return EventManager->GetEventBus();
    }
    return nullptr;
}
