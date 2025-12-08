// MedComCharacterScreen.h
// Copyright Suspense Team Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Base/SuspenseBaseWidget.h"
#include "SuspenseCore/Interfaces/Screens/ISuspenseCoreScreen.h"
#include "GameplayTagContainer.h"
#include "SuspenseCharacterScreen.generated.h"

// Forward declarations
class USuspenseUpperTabBar;
class USuspenseInventoryWidget;
class USuspenseEquipmentContainerWidget;

/**
 * Main character screen that contains the upper tab bar
 * This is the main container shown when player opens character menu
 */
UCLASS()
class UISYSTEM_API USuspenseCharacterScreen : public USuspenseBaseWidget, public ISuspenseScreen
{
    GENERATED_BODY()

public:
    USuspenseCharacterScreen(const FObjectInitializer& ObjectInitializer);

    //~ Begin USuspenseBaseWidget Interface
    virtual void InitializeWidget_Implementation() override;
    virtual void UninitializeWidget_Implementation() override;
    //~ End USuspenseBaseWidget Interface

    //~ Begin ISuspenseScreenInterface Interface
    virtual FGameplayTag GetScreenTag_Implementation() const override { return ScreenTag; }
    virtual void OnScreenActivated_Implementation() override;
    virtual void OnScreenDeactivated_Implementation() override;
    virtual bool IsScreenActive_Implementation() const override { return bIsActive; }
    virtual void UpdateScreen_Implementation(float DeltaTime) override;
    virtual void RefreshScreenContent_Implementation() override;
    virtual bool RequiresTick_Implementation() const override { return false; }
    //~ End ISuspenseScreenInterface Interface

    /**
     * Opens a specific tab by tag
     * @param TabTag Tag of the tab to open
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Character")
    void OpenTabByTag(const FGameplayTag& TabTag);

    /**
     * Opens a specific tab by index
     * @param TabIndex Index of the tab to open
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Character")
    void OpenTabByIndex(int32 TabIndex);

    /**
     * Gets the upper tab bar widget
     * @return Tab bar widget
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Character")
    USuspenseUpperTabBar* GetTabBar() const { return UpperTabBar; }

    /**
     * Selects a tab by its tag
     * @param TabTag Tag to select
     * @return True if successful
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Character")
    bool SelectTabByTag(const FGameplayTag& TabTag);

protected:
    //================================================
    // Bindable Widget Properties
    //================================================

    /** The upper tab bar with all tabs and content */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    USuspenseUpperTabBar* UpperTabBar;

    //================================================
    // Configuration Properties
    //================================================

    /** Screen identifier tag */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen")
    FGameplayTag ScreenTag;

    /** Default tab to open */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen")
    FGameplayTag DefaultTabTag;

    /** Whether to remember last opened tab */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen")
    bool bRememberLastTab;

    //================================================
    // Runtime Properties
    //================================================

    /** Whether screen is currently active */
    UPROPERTY(BlueprintReadOnly, Category = "Screen")
    bool bIsActive;

    /** Last opened tab tag */
    UPROPERTY(BlueprintReadOnly, Category = "Screen")
    FGameplayTag LastOpenedTab;

    //================================================
    // Blueprint Events
    //================================================

    /** Called when screen is shown */
    UFUNCTION(BlueprintImplementableEvent, Category = "UI|Character", DisplayName = "On Character Screen Opened")
    void K2_OnCharacterScreenOpened();

    /** Called when screen is hidden */
    UFUNCTION(BlueprintImplementableEvent, Category = "UI|Character", DisplayName = "On Character Screen Closed")
    void K2_OnCharacterScreenClosed();

private:
    /** Tab selection change subscription handle */
    FDelegateHandle TabSelectionChangeHandle;

    /** Tab bar close subscription handle */
    FDelegateHandle TabBarCloseHandle;

    /** Handle tab selection change */
    void OnTabSelectionChanged(UObject* TabBar, int32 OldIndex, int32 NewIndex);

    /** Handle tab bar close event */
    void OnTabBarClosed(UObject* TabBar);

    /** Find tab index by tag */
    int32 FindTabIndexByTag(const FGameplayTag& TabTag) const;

    /** Update input mode based on screen state */
    void UpdateInputMode();
};
