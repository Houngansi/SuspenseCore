// MedComCharacterScreen.cpp
// Copyright MedCom Team. All Rights Reserved.

#include "Widgets/Screens/MedComCharacterScreen.h"
#include "Widgets/Tabs/MedComUpperTabBar.h"
#include "Widgets/Inventory/MedComInventoryWidget.h"
#include "Widgets/Equipment/MedComEquipmentContainerWidget.h"
#include "Interfaces/Tabs/IMedComTabBarInterface.h"
#include "Interfaces/Screens/IMedComScreenInterface.h"
#include "Delegates/EventDelegateManager.h"
#include "GameFramework/PlayerController.h"

UMedComCharacterScreen::UMedComCharacterScreen(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Set default screen tag
    ScreenTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Screen.Character"));
    
    // Default to inventory tab
    DefaultTabTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Tab.Inventory"));
    
    bRememberLastTab = true;
    bIsActive = false;
}

void UMedComCharacterScreen::InitializeWidget_Implementation()
{
    Super::InitializeWidget_Implementation();

    if (!UpperTabBar)
    {
        UE_LOG(LogTemp, Error, TEXT("[CharacterScreen] UpperTabBar not bound!"));
        return;
    }

    // Initialize tab bar through interface
    if (IMedComTabBarInterface* TabBarInterface = Cast<IMedComTabBarInterface>(UpperTabBar))
    {
        // Subscribe to tab selection changes - это обычные C++ методы, не BlueprintNativeEvent
        if (FOnTabBarSelectionChanged* SelectionDelegate = TabBarInterface->GetOnTabSelectionChanged())
        {
            TabSelectionChangeHandle = SelectionDelegate->AddUObject(this, &UMedComCharacterScreen::OnTabSelectionChanged);
        }
        
        // Subscribe to tab bar close events
        if (FOnTabBarClosed* ClosedDelegate = TabBarInterface->GetOnTabBarClosed())
        {
            TabBarCloseHandle = ClosedDelegate->AddUObject(this, &UMedComCharacterScreen::OnTabBarClosed);
        }
    }

    // Verify tabs are properly configured
    int32 TabCount = IMedComTabBarInterface::Execute_GetTabCount(UpperTabBar);
    UE_LOG(LogTemp, Log, TEXT("[CharacterScreen] Tab bar initialized with %d tabs"), TabCount);
    
    // Log all tabs for debugging
    for (int32 i = 0; i < TabCount; i++)
    {
        if (UMedComUpperTabBar* TabBarWidget = Cast<UMedComUpperTabBar>(UpperTabBar))
        {
            FMedComTabConfig TabConfig = TabBarWidget->GetTabConfig(i);
            UE_LOG(LogTemp, Log, TEXT("[CharacterScreen] Tab[%d]: %s (%s)"), 
                i, *TabConfig.TabName.ToString(), *TabConfig.TabTag.ToString());
            
            // Verify content widgets
            UUserWidget* TabContent = IMedComTabBarInterface::Execute_GetTabContent(UpperTabBar, i);
            if (TabContent)
            {
                FString ContentType = TEXT("Unknown");
                
                // Check content type
                if (TabContent->IsA<UMedComInventoryWidget>())
                {
                    ContentType = TEXT("InventoryWidget");
                }
                else if (TabContent->IsA<UMedComEquipmentContainerWidget>())
                {
                    ContentType = TEXT("EquipmentWidget");
                }
                else if (TabContent->GetClass()->ImplementsInterface(UMedComScreenInterface::StaticClass()))
                {
                    FGameplayTag ContentScreenTag = IMedComScreenInterface::Execute_GetScreenTag(TabContent);
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
            IMedComTabBarInterface::Execute_SelectTabByIndex(UpperTabBar, 0);
        }
    }
    
    // Initialize screen state
    UpdateInputMode();
    
    UE_LOG(LogTemp, Log, TEXT("[CharacterScreen] Widget initialization completed"));
}

void UMedComCharacterScreen::UninitializeWidget_Implementation()
{
    // Unsubscribe from tab bar events
    if (UpperTabBar)
    {
        if (IMedComTabBarInterface* TabBarInterface = Cast<IMedComTabBarInterface>(UpperTabBar))
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

void UMedComCharacterScreen::OnScreenActivated_Implementation()
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

    // Notify event system
    if (UEventDelegateManager* EventManager = GetDelegateManager())
    {
        EventManager->NotifyScreenActivated(this, ScreenTag);
        
        FGameplayTag OpenedTag = FGameplayTag::RequestGameplayTag(TEXT("UI.CharacterScreen.Opened"));
        EventManager->NotifyUIEventGeneric(this, OpenedTag, TEXT(""));
        
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
                    FMedComTabConfig TabConfig = UpperTabBar->GetTabConfig(CurrentIndex);
                    if (UUserWidget* TabContent = UpperTabBar->GetTabContent_Implementation(CurrentIndex))
                    {
                        // Use interface to refresh content instead of direct class access
                        if (TabContent->GetClass()->ImplementsInterface(UMedComScreenInterface::StaticClass()))
                        {
                            IMedComScreenInterface::Execute_RefreshScreenContent(TabContent);
                        }
                    }
                }
            }
        }, 0.2f, false);
    }

    UE_LOG(LogTemp, Log, TEXT("[CharacterScreen] Activated"));
}

void UMedComCharacterScreen::OnScreenDeactivated_Implementation()
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
        FMedComTabConfig TabConfig = UpperTabBar->GetTabConfig(CurrentIndex);
        if (TabConfig.TabTag.IsValid())
        {
            LastOpenedTab = TabConfig.TabTag;
        }
    }

    // Call Blueprint event
    K2_OnCharacterScreenClosed();

    // Notify event system
    if (UEventDelegateManager* EventManager = GetDelegateManager())
    {
        EventManager->NotifyScreenDeactivated(this, ScreenTag);
        
        FGameplayTag ClosedTag = FGameplayTag::RequestGameplayTag(TEXT("UI.CharacterScreen.Closed"));
        EventManager->NotifyUIEventGeneric(this, ClosedTag, TEXT(""));
    }

    UE_LOG(LogTemp, Log, TEXT("[CharacterScreen] Deactivated"));
}

void UMedComCharacterScreen::UpdateScreen_Implementation(float DeltaTime)
{
    // Character screen doesn't need tick updates by default
}

void UMedComCharacterScreen::RefreshScreenContent_Implementation()
{
    // Refresh active tab content
    if (UpperTabBar)
    {
        UpperTabBar->RefreshActiveTabContent();
    }
}

void UMedComCharacterScreen::OpenTabByTag(const FGameplayTag& TabTag)
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

void UMedComCharacterScreen::OpenTabByIndex(int32 TabIndex)
{
    if (UpperTabBar)
    {
        UpperTabBar->SelectTabByIndex_Implementation(TabIndex);
    }
}

bool UMedComCharacterScreen::SelectTabByTag(const FGameplayTag& TabTag)
{
    if (!UpperTabBar)
    {
        return false;
    }
    
    return UpperTabBar->SelectTabByTag(TabTag);
}

void UMedComCharacterScreen::OnTabSelectionChanged(UObject* TabBar, int32 OldIndex, int32 NewIndex)
{
    if (TabBar != UpperTabBar)
    {
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("[CharacterScreen] Tab selection changed from %d to %d"), OldIndex, NewIndex);
    
    // Remember new tab
    if (bRememberLastTab && UpperTabBar && NewIndex >= 0)
    {
        FMedComTabConfig TabConfig = UpperTabBar->GetTabConfig(NewIndex);
        if (TabConfig.TabTag.IsValid())
        {
            LastOpenedTab = TabConfig.TabTag;
        }
    }
}

void UMedComCharacterScreen::OnTabBarClosed(UObject* TabBar)
{
    if (TabBar == UpperTabBar)
    {
        // Hide the character screen
        SetVisibility(ESlateVisibility::Collapsed);

        // Notify about close
        if (UEventDelegateManager* EventManager = GetDelegateManager())
        {
            FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("UI.CharacterScreen.Closed"));
            EventManager->NotifyUIEventGeneric(this, EventTag, TEXT(""));
        }
    }
}

int32 UMedComCharacterScreen::FindTabIndexByTag(const FGameplayTag& TabTag) const
{
    if (!UpperTabBar)
    {
        return -1;
    }

    for (int32 i = 0; i < UpperTabBar->GetTabCount_Implementation(); i++)
    {
        FMedComTabConfig Config = UpperTabBar->GetTabConfig(i);
        if (Config.TabTag.MatchesTagExact(TabTag))
        {
            return i;
        }
    }

    return -1;
}

void UMedComCharacterScreen::UpdateInputMode()
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