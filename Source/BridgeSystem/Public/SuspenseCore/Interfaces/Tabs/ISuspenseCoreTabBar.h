// Copyright Suspense Team. All Rights Reserved.

#ifndef SUSPENSECORE_INTERFACES_TABS_ISUSPENSECORETABBAR_H
#define SUSPENSECORE_INTERFACES_TABS_ISUSPENSECORETABBAR_H

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "ISuspenseTabBar.generated.h"

// Forward declarations
class UUserWidget;

/**
 * Delegate for tab selection events
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnTabBarSelectionChanged, UObject* /*TabBar*/, int32 /*OldIndex*/, int32 /*NewIndex*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTabBarClosed, UObject* /*TabBar*/);

UINTERFACE(MinimalAPI, BlueprintType)
class USuspenseTabBar : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for tab bar widgets
 * Provides standardized methods for tab management
 */
class BRIDGESYSTEM_API ISuspenseTabBar
{
    GENERATED_BODY()

public:
    /**
     * Gets number of tabs
     * @return Tab count
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|TabBar")
    int32 GetTabCount() const;

    /**
     * Selects tab by index
     * @param TabIndex Tab to select
     * @return True if selected
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|TabBar")
    bool SelectTabByIndex(int32 TabIndex);

    /**
     * Gets currently selected tab index
     * @return Selected index or -1
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|TabBar")
    int32 GetSelectedTabIndex() const;

    /**
     * Gets content widget for tab
     * @param TabIndex Tab index
     * @return Content widget or nullptr
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|TabBar")
    UUserWidget* GetTabContent(int32 TabIndex) const;

    /**
     * Sets tab enabled state
     * @param TabIndex Tab index
     * @param bEnabled New state
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|TabBar")
    void SetTabEnabled(int32 TabIndex, bool bEnabled);

    /**
     * Checks if tab is enabled
     * @param TabIndex Tab index
     * @return True if enabled
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|TabBar")
    bool IsTabEnabled(int32 TabIndex) const;

    /**
     * Gets tab bar gameplay tag
     * @return Tab bar identifier tag
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|TabBar")
    FGameplayTag GetTabBarTag() const;

    /**
     * Native delegates for C++ binding
     */
    virtual FOnTabBarSelectionChanged* GetOnTabSelectionChanged() = 0;
    virtual FOnTabBarClosed* GetOnTabBarClosed() = 0;
};

#endif // SUSPENSECORE_INTERFACES_TABS_ISUSPENSECORETABBAR_H