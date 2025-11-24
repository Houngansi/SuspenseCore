// Copyright MedCom Team. All Rights Reserved.

#include "Widgets/Base/MedComBaseWidget.h" 
#include "Delegates/EventDelegateManager.h"
#include "Interfaces/UI/IMedComUIWidgetInterface.h"
#include "Animation/WidgetAnimation.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

UMedComBaseWidget::UMedComBaseWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Default settings
    bEnableTick = false;
}

void UMedComBaseWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    LogLifecycleEvent(TEXT("NativeConstruct"));
    
    // Initialize widget through interface
    Execute_InitializeWidget(this);
    
    // Notify event system about widget creation
    IMedComUIWidgetInterface::BroadcastWidgetCreated(this);
}

void UMedComBaseWidget::NativeDestruct()
{
    // Uninitialize widget through interface
    Execute_UninitializeWidget(this);
    
    // Notify event system about widget destruction
    IMedComUIWidgetInterface::BroadcastWidgetDestroyed(this);
    
    LogLifecycleEvent(TEXT("NativeDestruct"));
    
    Super::NativeDestruct();
}

void UMedComBaseWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    // Only tick if enabled and initialized
    if (bEnableTick && bIsInitialized)
    {
        Execute_UpdateWidget(this, InDeltaTime);
    }
}

void UMedComBaseWidget::SetVisibility(ESlateVisibility InVisibility)
{
    // Cache old visibility
    ESlateVisibility OldVisibility = GetVisibility();
    
    // Call parent implementation
    Super::SetVisibility(InVisibility);
    
    // Determine if visibility actually changed
    bool bWasVisible = (OldVisibility == ESlateVisibility::Visible || 
                       OldVisibility == ESlateVisibility::HitTestInvisible || 
                       OldVisibility == ESlateVisibility::SelfHitTestInvisible);
                       
    bool bIsNowVisible = (InVisibility == ESlateVisibility::Visible || 
                         InVisibility == ESlateVisibility::HitTestInvisible || 
                         InVisibility == ESlateVisibility::SelfHitTestInvisible);
    
    // If visibility changed, notify through interface
    if (bWasVisible != bIsNowVisible)
    {
        Execute_OnVisibilityChanged(this, bIsNowVisible);
        IMedComUIWidgetInterface::BroadcastVisibilityChanged(this, bIsNowVisible);
    }
}

void UMedComBaseWidget::InitializeWidget_Implementation()
{
    LogLifecycleEvent(TEXT("InitializeWidget"));
    
    bIsInitialized = true;
    
    // Note: UUserWidget doesn't have SetTickableWhenPaused or SetCanTick methods
    // Ticking is controlled by the Slate system automatically
}

void UMedComBaseWidget::UninitializeWidget_Implementation()
{
    LogLifecycleEvent(TEXT("UninitializeWidget"));
    
    bIsInitialized = false;
    
    // Clear cached event manager
    CachedEventManager = nullptr;
}

void UMedComBaseWidget::UpdateWidget_Implementation(float DeltaTime)
{
    // Base implementation does nothing
    // Derived classes override this for custom update logic
}

void UMedComBaseWidget::ShowWidget_Implementation(bool bAnimate)
{
    LogLifecycleEvent(TEXT("ShowWidget"));
    
    if (bAnimate && ShowAnimation)
    {
        PlayShowAnimation();
    }
    else
    {
        SetVisibility(ESlateVisibility::Visible);
        bIsShowing = true;
    }
}

void UMedComBaseWidget::HideWidget_Implementation(bool bAnimate)
{
    LogLifecycleEvent(TEXT("HideWidget"));
    
    if (bAnimate && HideAnimation)
    {
        PlayHideAnimation();
    }
    else
    {
        SetVisibility(ESlateVisibility::Collapsed);
        bIsShowing = false;
    }
}

void UMedComBaseWidget::OnVisibilityChanged_Implementation(bool bIsVisible)
{
    LogLifecycleEvent(FString::Printf(TEXT("OnVisibilityChanged: %s"), 
        bIsVisible ? TEXT("Visible") : TEXT("Hidden")));
}

UEventDelegateManager* UMedComBaseWidget::GetDelegateManager() const
{
    if (!CachedEventManager)
    {
        // Получаем из GameInstance - правильный способ для UGameInstanceSubsystem
        if (UGameInstance* GameInstance = GetGameInstance())
        {
            CachedEventManager = GameInstance->GetSubsystem<UEventDelegateManager>();
        }
        if (!CachedEventManager)
        {
            CachedEventManager = UEventDelegateManager::Get(this);
        }
    }
    return CachedEventManager;
}

void UMedComBaseWidget::PlayShowAnimation()
{
    if (ShowAnimation)
    {
        PlayAnimation(ShowAnimation);
        SetVisibility(ESlateVisibility::Visible);
        bIsShowing = true;
        
        // Bind animation finished callback
        FWidgetAnimationDynamicEvent AnimationFinished;
        AnimationFinished.BindDynamic(this, &UMedComBaseWidget::OnShowAnimationFinished);
        BindToAnimationFinished(ShowAnimation, AnimationFinished);
    }
}

void UMedComBaseWidget::PlayHideAnimation()
{
    if (HideAnimation)
    {
        PlayAnimation(HideAnimation);
        bIsShowing = false;
        
        // Bind animation finished callback
        FWidgetAnimationDynamicEvent AnimationFinished;
        AnimationFinished.BindDynamic(this, &UMedComBaseWidget::OnHideAnimationFinished);
        BindToAnimationFinished(HideAnimation, AnimationFinished);
    }
}

void UMedComBaseWidget::OnShowAnimationFinished()
{
    // Base implementation does nothing
    // Derived classes can override for custom behavior
}

void UMedComBaseWidget::OnHideAnimationFinished()
{
    // Hide widget after animation completes
    SetVisibility(ESlateVisibility::Collapsed);
}

void UMedComBaseWidget::LogLifecycleEvent(const FString& EventName) const
{
    UE_LOG(LogTemp, Verbose, TEXT("[%s] %s - Tag: %s"), 
        *GetClass()->GetName(), 
        *EventName, 
        *WidgetTag.ToString());
}

int32 UMedComBaseWidget::GetOwningPlayerIndex() const
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
        {
            return LocalPlayer->GetControllerId();
        }
    }
    return 0;
}