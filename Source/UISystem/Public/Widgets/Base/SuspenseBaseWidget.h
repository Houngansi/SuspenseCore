// Copyright Suspense Team Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseBaseWidget.generated.h"

// Forward declarations
class USuspenseCoreEventManager;

/**
 * Base class for all UI widgets in MedCom game
 *
 * Key responsibilities:
 * - Implements ISuspenseCoreUIWidget interface for standardized widget behavior
 * - Provides lifecycle management (initialization, updates, cleanup)
 * - Handles animation support for show/hide operations
 * - Manages event system integration through EventDelegateManager
 *
 * Architecture notes:
 * - No dependencies on game modules (only MedComShared)
 * - All game data comes through interfaces or events
 * - Follows Single Responsibility Principle - only base widget functionality
 */
UCLASS(Abstract)
class UISYSTEM_API USuspenseBaseWidget : public UUserWidget, public ISuspenseCoreUIWidget
{
    GENERATED_BODY()

public:
    USuspenseBaseWidget(const FObjectInitializer& ObjectInitializer);

    //================================================
    // UUserWidget Interface
    //================================================

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void SetVisibility(ESlateVisibility InVisibility) override;

    //================================================
    // ISuspenseCoreUIWidget Interface Implementation
    //================================================

    virtual void InitializeWidget_Implementation() override;
    virtual void UninitializeWidget_Implementation() override;
    virtual void UpdateWidget_Implementation(float DeltaTime) override;
    virtual FGameplayTag GetWidgetTag_Implementation() const override { return WidgetTag; }
    virtual void SetWidgetTag_Implementation(const FGameplayTag& NewTag) override { WidgetTag = NewTag; }
    virtual bool IsInitialized_Implementation() const override { return bIsInitialized; }
    virtual void ShowWidget_Implementation(bool bAnimate) override;
    virtual void HideWidget_Implementation(bool bAnimate) override;
    virtual void OnVisibilityChanged_Implementation(bool bIsVisible) override;
    virtual USuspenseCoreEventManager* GetDelegateManager() const override;

    //================================================
    // Animation Support
    //================================================

    UFUNCTION(BlueprintCallable, Category = "UI|Animation")
    void PlayShowAnimation();

    UFUNCTION(BlueprintCallable, Category = "UI|Animation")
    void PlayHideAnimation();

    UFUNCTION(BlueprintCallable, Category = "UI|Animation")
    virtual void OnShowAnimationFinished();

    UFUNCTION(BlueprintCallable, Category = "UI|Animation")
    virtual void OnHideAnimationFinished();

    /** Unique tag identifying this widget instance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widget")
    FGameplayTag WidgetTag;
protected:
    //================================================
    // Properties
    //================================================



    /** Whether this widget should tick */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Widget")
    bool bEnableTick = false;

    /** Optional show animation */
    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    UWidgetAnimation* ShowAnimation;

    /** Optional hide animation */
    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    UWidgetAnimation* HideAnimation;

    /** Current visibility state for animation purposes */
    UPROPERTY(BlueprintReadOnly, Category = "Widget")
    bool bIsShowing = true;

    //================================================
    // Internal State
    //================================================

    /** Whether widget has been properly initialized */
    bool bIsInitialized = false;

    /** Cached event manager reference for performance */
    UPROPERTY()
    mutable USuspenseCoreEventManager* CachedEventManager = nullptr;

    //================================================
    // Helper Methods
    //================================================

    /** Log widget lifecycle events for debugging */
    void LogLifecycleEvent(const FString& EventName) const;

    /** Get the owning player index (useful for split-screen) */
    UFUNCTION(BlueprintCallable, Category = "Widget")
    int32 GetOwningPlayerIndex() const;
};
