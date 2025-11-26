// Copyright Suspense Team Team. All Rights Reserved.

#include "Tooltip/SuspenseTooltipManager.h"
#include "Widgets/Tooltip/SuspenseItemTooltipWidget.h"
#include "Interfaces/UI/ISuspenseTooltip.h"
#include "Interfaces/UI/ISuspenseTooltipSource.h"
#include "Delegates/EventDelegateManager.h"
#include "Engine/World.h"
#include "Components/PanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

// ========================================
// Subsystem Lifecycle Implementation
// ========================================

void USuspenseTooltipManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    // Initialize event manager first
    CachedEventManager = GetEventManager();
    if (!CachedEventManager)
    {
        UE_LOG(LogTemp, Error, TEXT("[TooltipManager] Failed to get EventDelegateManager! Tooltip system will not function."));
        return;
    }
    
    // Validate configuration
    if (!Configuration.DefaultTooltipClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TooltipManager] No default tooltip class configured! "
            "Please set DefaultTooltipClass in TooltipManager settings or blueprint. "
            "Tooltips will not work until a default class is set."));
    }
    
    // Subscribe to tooltip events
    TooltipRequestHandle = CachedEventManager->OnTooltipRequestedNative.AddUObject(
        this, &USuspenseTooltipManager::OnTooltipRequested);
        
    TooltipHideHandle = CachedEventManager->OnTooltipHideRequestedNative.AddUObject(
        this, &USuspenseTooltipManager::OnTooltipHideRequested);
        
    TooltipUpdateHandle = CachedEventManager->OnTooltipUpdatePositionNative.AddUObject(
        this, &USuspenseTooltipManager::OnTooltipUpdatePosition);
    
    // Pre-create pool for default class if specified
    if (Configuration.DefaultTooltipClass && Configuration.MaxPooledTooltipsPerClass > 0)
    {
        RegisterTooltipClass(Configuration.DefaultTooltipClass, Configuration.MaxPooledTooltipsPerClass);
    }
    
    UE_LOG(LogTemp, Log, TEXT("[TooltipManager] === Tooltip System Initialized ==="));
    UE_LOG(LogTemp, Log, TEXT("[TooltipManager] Default Class: %s"), 
        Configuration.DefaultTooltipClass ? *Configuration.DefaultTooltipClass->GetName() : TEXT("None"));
    UE_LOG(LogTemp, Log, TEXT("[TooltipManager] Z-Order: %d"), Configuration.TooltipZOrder);
    UE_LOG(LogTemp, Log, TEXT("[TooltipManager] Pool Size Per Class: %d"), Configuration.MaxPooledTooltipsPerClass);
    UE_LOG(LogTemp, Log, TEXT("[TooltipManager] Multiple Classes: %s"), 
        Configuration.bAllowMultipleTooltipClasses ? TEXT("Enabled") : TEXT("Disabled"));
}

void USuspenseTooltipManager::Deinitialize()
{
    // Unsubscribe from all events
    if (CachedEventManager)
    {
        if (TooltipRequestHandle.IsValid())
        {
            CachedEventManager->OnTooltipRequestedNative.Remove(TooltipRequestHandle);
        }
        if (TooltipHideHandle.IsValid())
        {
            CachedEventManager->OnTooltipHideRequestedNative.Remove(TooltipHideHandle);
        }
        if (TooltipUpdateHandle.IsValid())
        {
            CachedEventManager->OnTooltipUpdatePositionNative.Remove(TooltipUpdateHandle);
        }
    }
    
    // Force hide any active tooltip
    ForceHideTooltip();
    
    // Clean up all pools
    CleanupAllPools();
    
    // Clear references
    ActiveTooltip = nullptr;
    ActiveTooltipClass = nullptr;
    CurrentSourceWidget.Reset();
    CachedEventManager = nullptr;
    
    UE_LOG(LogTemp, Log, TEXT("[TooltipManager] Tooltip system deinitialized"));
    
    Super::Deinitialize();
}

// ========================================
// Configuration Management
// ========================================

void USuspenseTooltipManager::UpdateConfiguration(const FTooltipConfiguration& NewConfig)
{
    Configuration = NewConfig;
    
    LogVerbose(TEXT("Configuration updated"));
    
    // If default class changed and we have active tooltip of old default class, hide it
    if (ActiveTooltip && ActiveTooltipClass == Configuration.DefaultTooltipClass)
    {
        ForceHideTooltip();
    }
    
    // Update pool sizes if needed
    for (auto& PoolPair : TooltipPools)
    {
        PoolPair.Value.MaxPoolSize = Configuration.MaxPooledTooltipsPerClass;
    }
}

void USuspenseTooltipManager::SetDefaultTooltipClass(TSubclassOf<USuspenseItemTooltipWidget> InTooltipClass)
{
    if (Configuration.DefaultTooltipClass != InTooltipClass)
    {
        Configuration.DefaultTooltipClass = InTooltipClass;
        
        UE_LOG(LogTemp, Log, TEXT("[TooltipManager] Default tooltip class changed to: %s"), 
            InTooltipClass ? *InTooltipClass->GetName() : TEXT("None"));
        
        // Pre-create pool for new default class
        if (InTooltipClass)
        {
            RegisterTooltipClass(InTooltipClass, Configuration.MaxPooledTooltipsPerClass);
        }
    }
}

void USuspenseTooltipManager::RegisterTooltipClass(TSubclassOf<USuspenseItemTooltipWidget> TooltipClass, int32 PoolSize)
{
    if (!TooltipClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TooltipManager] Cannot register null tooltip class"));
        return;
    }
    
    // Get or create pool for this class
    FTooltipPool& Pool = GetOrCreatePool(TooltipClass);
    Pool.MaxPoolSize = FMath::Max(1, PoolSize);
    
    // Pre-create widgets up to pool size
    int32 ToCreate = Pool.MaxPoolSize - Pool.AvailableWidgets.Num();
    
    for (int32 i = 0; i < ToCreate; ++i)
    {
        if (USuspenseItemTooltipWidget* NewTooltip = CreateTooltipWidget(TooltipClass))
        {
            NewTooltip->SetVisibility(ESlateVisibility::Collapsed);
            Pool.AvailableWidgets.Add(NewTooltip);
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("[TooltipManager] Registered tooltip class %s with pool size %d (created %d new widgets)"), 
        *TooltipClass->GetName(), Pool.MaxPoolSize, ToCreate);
}

// ========================================
// Event Handlers
// ========================================

void USuspenseTooltipManager::OnTooltipRequested(const FItemUIData& ItemData, const FVector2D& ScreenPosition)
{
    LogVerbose(FString::Printf(TEXT("Tooltip requested for item: %s"), *ItemData.DisplayName.ToString()));
    
    // Determine which tooltip class to use
    TSubclassOf<UUserWidget> TooltipClassToUse = DetermineTooltipClass(ItemData);
    
    if (!TooltipClassToUse)
    {
        UE_LOG(LogTemp, Error, TEXT("[TooltipManager] No tooltip class available for item: %s"), 
            *ItemData.ItemID.ToString());
        return;
    }
    
    // Process the request with determined class
    ProcessTooltipRequest(nullptr, ItemData, ScreenPosition, TooltipClassToUse);
}

void USuspenseTooltipManager::OnTooltipHideRequested()
{
    LogVerbose(TEXT("Tooltip hide requested via event"));
    ProcessTooltipHide(nullptr);
}

void USuspenseTooltipManager::OnTooltipUpdatePosition(const FVector2D& ScreenPosition)
{
    if (ActiveTooltip && ActiveTooltip->GetClass()->ImplementsInterface(USuspenseTooltip::StaticClass()))
    {
        ISuspenseTooltipInterface::Execute_UpdateTooltipPosition(ActiveTooltip, ScreenPosition);
    }
}

// ========================================
// Tooltip Request Processing
// ========================================

void USuspenseTooltipManager::ProcessTooltipRequest(
    UUserWidget* SourceWidget, 
    const FItemUIData& ItemData, 
    const FVector2D& ScreenPosition,
    TSubclassOf<UUserWidget> TooltipClass)
{
    // Validate input
    if (!ItemData.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[TooltipManager] ProcessTooltipRequest called with invalid item data"));
        return;
    }
    
    if (!TooltipClass)
    {
        UE_LOG(LogTemp, Error, TEXT("[TooltipManager] No tooltip class specified for processing"));
        return;
    }
    
    // Check if source widget allows tooltips
    if (SourceWidget)
    {
        bool bCanShow = true;
        if (SourceWidget->GetClass()->ImplementsInterface(USuspenseTooltipSource::StaticClass()))
        {
            bCanShow = ISuspenseTooltipSourceInterface::Execute_CanShowTooltip(SourceWidget);
        }
        
        if (!bCanShow)
        {
            LogVerbose(TEXT("Source widget does not allow tooltips"));
            return;
        }
    }
    
    // If switching tooltip classes, hide current tooltip first
    if (ActiveTooltip && ActiveTooltipClass != TooltipClass)
    {
        LogVerbose(TEXT("Switching tooltip classes, hiding current tooltip"));
        ProcessTooltipHide(CurrentSourceWidget.Get());
    }
    
    // Update source widget reference
    CurrentSourceWidget = SourceWidget;
    
    // Acquire tooltip widget of requested class
    ActiveTooltip = AcquireTooltipWidget(TooltipClass);
    if (!ActiveTooltip)
    {
        UE_LOG(LogTemp, Error, TEXT("[TooltipManager] Failed to acquire tooltip widget of class: %s"), 
            *TooltipClass->GetName());
        return;
    }
    
    ActiveTooltipClass = TooltipClass;
    
    // Show tooltip with data
    if (ActiveTooltip->GetClass()->ImplementsInterface(USuspenseTooltip::StaticClass()))
    {
        ISuspenseTooltipInterface::Execute_ShowTooltip(ActiveTooltip, ItemData, ScreenPosition);
        
        ActiveTooltip->SetRenderOpacity(1.0f);
        ActiveTooltip->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
    
        LogVerbose(FString::Printf(TEXT("Showing tooltip for item: %s with class: %s"), 
            *ItemData.DisplayName.ToString(), *TooltipClass->GetName()));
    }
    
    // Notify source widget if available
    if (SourceWidget && SourceWidget->GetClass()->ImplementsInterface(USuspenseTooltipSource::StaticClass()))
    {
        ISuspenseTooltipSourceInterface::Execute_OnTooltipShown(SourceWidget);
    }
}

void USuspenseTooltipManager::ProcessTooltipHide(UUserWidget* SourceWidget)
{
    // If source widget provided, only hide if it owns the current tooltip
    if (SourceWidget && CurrentSourceWidget.IsValid() && CurrentSourceWidget.Get() != SourceWidget)
    {
        LogVerbose(TEXT("Hide request from non-owner widget, ignoring"));
        return;
    }
    
    if (!ActiveTooltip)
    {
        return;
    }
    
    // Hide the tooltip
    if (ActiveTooltip->GetClass()->ImplementsInterface(USuspenseTooltip::StaticClass()))
    {
        ISuspenseTooltipInterface::Execute_HideTooltip(ActiveTooltip);
    }
    
    // Notify source widget
    if (CurrentSourceWidget.IsValid())
    {
        if (UUserWidget* Widget = CurrentSourceWidget.Get())
        {
            if (Widget->GetClass()->ImplementsInterface(USuspenseTooltipSource::StaticClass()))
            {
                ISuspenseTooltipSourceInterface::Execute_OnTooltipHidden(Widget);
            }
        }
    }
    
    // Return tooltip to pool
    if (ActiveTooltip && ActiveTooltipClass)
    {
        ReleaseTooltipWidget(ActiveTooltip, ActiveTooltipClass);
    }
    
    // Clear references
    ActiveTooltip = nullptr;
    ActiveTooltipClass = nullptr;
    CurrentSourceWidget.Reset();
    
    LogVerbose(TEXT("Tooltip hidden"));
}

// ========================================
// Pool Management
// ========================================

FTooltipPool& USuspenseTooltipManager::GetOrCreatePool(UClass* TooltipClass)
{
    // Теперь FTooltipPool - это не вложенная структура, а обычная структура
    if (!TooltipPools.Contains(TooltipClass))
    {
        FTooltipPool NewPool;
        NewPool.MaxPoolSize = Configuration.MaxPooledTooltipsPerClass;
        TooltipPools.Add(TooltipClass, NewPool);
    }
    
    return TooltipPools[TooltipClass];
}

USuspenseItemTooltipWidget* USuspenseTooltipManager::AcquireTooltipWidget(TSubclassOf<UUserWidget> TooltipClass)
{
    if (!TooltipClass)
    {
        return nullptr;
    }
    
    FTooltipPool& Pool = GetOrCreatePool(TooltipClass);
    
    USuspenseItemTooltipWidget* TooltipWidget = nullptr;
    
    // Try to get from pool first
    if (Pool.AvailableWidgets.Num() > 0)
    {
        TooltipWidget = Pool.AvailableWidgets.Pop();
        
        if (IsValid(TooltipWidget))
        {
            // Reset widget state
            TooltipWidget->SetRenderOpacity(1.0f);
            TooltipWidget->SetColorAndOpacity(FLinearColor::White);
            TooltipWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            
            LogVerbose(FString::Printf(TEXT("Acquired tooltip from pool for class: %s"), 
                *TooltipClass->GetName()));
        }
        else
        {
            // Invalid widget in pool, remove it
            TooltipWidget = nullptr;
        }
    }
    
    // Create new widget if needed
    if (!TooltipWidget)
    {
        TooltipWidget = CreateTooltipWidget(TooltipClass);
        LogVerbose(FString::Printf(TEXT("Created new tooltip for class: %s"), 
            *TooltipClass->GetName()));
    }
    
    // Track as in use
    if (TooltipWidget)
    {
        Pool.InUseWidgets.Add(TooltipWidget);
    }
    
    return TooltipWidget;
}

void USuspenseTooltipManager::ReleaseTooltipWidget(USuspenseItemTooltipWidget* Tooltip, UClass* TooltipClass)
{
    if (!IsValid(Tooltip) || !TooltipClass)
    {
        return;
    }
    
    FTooltipPool* Pool = TooltipPools.Find(TooltipClass);
    if (!Pool)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TooltipManager] No pool found for tooltip class: %s"), 
            *TooltipClass->GetName());
        return;
    }
    
    // Remove from in-use list
    Pool->InUseWidgets.Remove(Tooltip);
    
    // Hide the tooltip
    Tooltip->SetVisibility(ESlateVisibility::Collapsed);
    
    // Return to pool if not at capacity
    if (Pool->AvailableWidgets.Num() < Pool->MaxPoolSize)
    {
        Pool->AvailableWidgets.Add(Tooltip);
        LogVerbose(FString::Printf(TEXT("Returned tooltip to pool for class: %s"), 
            *TooltipClass->GetName()));
    }
    else
    {
        // Pool is full, destroy the widget
        Tooltip->RemoveFromParent();
        Tooltip->ConditionalBeginDestroy();
        LogVerbose(FString::Printf(TEXT("Destroyed excess tooltip for class: %s"), 
            *TooltipClass->GetName()));
    }
}

USuspenseItemTooltipWidget* USuspenseTooltipManager::CreateTooltipWidget(TSubclassOf<UUserWidget> TooltipClass)
{
    if (!TooltipClass)
    {
        return nullptr;
    }
    
    APlayerController* PC = GetOwningPlayerController();
    if (!PC)
    {
        UE_LOG(LogTemp, Error, TEXT("[TooltipManager] No player controller available for widget creation"));
        return nullptr;
    }
    
    // Create the widget
    UUserWidget* NewWidget = CreateWidget<UUserWidget>(PC, TooltipClass);
    if (!NewWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("[TooltipManager] Failed to create widget of class: %s"), 
            *TooltipClass->GetName());
        return nullptr;
    }
    
    // Ensure it's the correct type
    USuspenseItemTooltipWidget* TooltipWidget = Cast<USuspenseItemTooltipWidget>(NewWidget);
    if (!TooltipWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("[TooltipManager] Created widget is not derived from USuspenseItemTooltipWidget"));
        NewWidget->ConditionalBeginDestroy();
        return nullptr;
    }
    
    // КРИТИЧНО: Устанавливаем ВСЕ свойства ДО добавления к viewport
    // 1. Сначала полная непрозрачность
    TooltipWidget->SetRenderOpacity(1.0f);
    TooltipWidget->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
    
    // 2. НЕ отключаем виджет! Вместо этого делаем его HitTestInvisible
    TooltipWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
    TooltipWidget->SetIsEnabled(true); // ВАЖНО: Оставляем включенным!
    TooltipWidget->SetIsFocusable(false);
    
    // 3. Убеждаемся что нет никаких модификаторов рендеринга
    TooltipWidget->SetRenderTransform(FWidgetTransform());
    TooltipWidget->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
    
    // 4. ТОЛЬКО ТЕПЕРЬ добавляем к viewport с максимальным Z-order
    const int32 TOOLTIP_MAX_ZORDER = 999999; // Используем экстремально высокое значение
    TooltipWidget->AddToViewport(TOOLTIP_MAX_ZORDER);
    
    // 5. КРИТИЧНО: Еще раз форсируем opacity ПОСЛЕ добавления к viewport
    TooltipWidget->SetRenderOpacity(1.0f);
    TooltipWidget->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
    
    // 6. Проверяем родительскую иерархию и исправляем если нужно
    if (UPanelWidget* ParentPanel = TooltipWidget->GetParent())
    {
        UE_LOG(LogTemp, Warning, TEXT("[TooltipManager] Tooltip has parent: %s, checking opacity..."), 
            *ParentPanel->GetName());
            
        // Проходим по всей иерархии родителей
        UWidget* CurrentParent = ParentPanel;
        while (CurrentParent)
        {
            if (UUserWidget* ParentUserWidget = Cast<UUserWidget>(CurrentParent))
            {
                float ParentOpacity = ParentUserWidget->GetRenderOpacity();
                if (ParentOpacity < 1.0f)
                {
                    UE_LOG(LogTemp, Error, TEXT("[TooltipManager] FOUND PROBLEM! Parent %s has opacity %.2f"), 
                        *ParentUserWidget->GetName(), ParentOpacity);
                    ParentUserWidget->SetRenderOpacity(1.0f);
                }
            }
            CurrentParent = CurrentParent->GetParent();
        }
    }
    
    // Update pool statistics
    FTooltipPool* Pool = TooltipPools.Find(TooltipClass);
    if (Pool)
    {
        Pool->TotalCreated++;
    }
    
    // ДИАГНОСТИКА: Финальная проверка
    float FinalOpacity = TooltipWidget->GetRenderOpacity();
    FLinearColor FinalColor = TooltipWidget->GetColorAndOpacity();
    
    UE_LOG(LogTemp, Warning, TEXT("[TooltipManager] Tooltip created with:"));
    UE_LOG(LogTemp, Warning, TEXT("  - Z-Order: %d"), TOOLTIP_MAX_ZORDER);
    UE_LOG(LogTemp, Warning, TEXT("  - Render Opacity: %.2f"), FinalOpacity);
    UE_LOG(LogTemp, Warning, TEXT("  - Color: R=%.2f G=%.2f B=%.2f A=%.2f"), 
        FinalColor.R, FinalColor.G, FinalColor.B, FinalColor.A);
    UE_LOG(LogTemp, Warning, TEXT("  - IsEnabled: %s"), TooltipWidget->GetIsEnabled() ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTemp, Warning, TEXT("  - Visibility: %s"), *UEnum::GetValueAsString(TooltipWidget->GetVisibility()));
    
    return TooltipWidget;
}

// ========================================
// Public Control Methods
// ========================================

void USuspenseTooltipManager::ForceHideTooltip()
{
    if (ActiveTooltip)
    {
        ProcessTooltipHide(CurrentSourceWidget.Get());
    }
}

bool USuspenseTooltipManager::IsTooltipActive() const
{
    if (ActiveTooltip && ActiveTooltip->GetClass()->ImplementsInterface(USuspenseTooltip::StaticClass()))
    {
        return ISuspenseTooltipInterface::Execute_IsTooltipVisible(ActiveTooltip);
    }
    
    return false;
}

// ========================================
// Statistics and Debug
// ========================================

int32 USuspenseTooltipManager::GetTotalTooltipCount() const
{
    int32 Total = 0;
    
    for (const auto& PoolPair : TooltipPools)
    {
        Total += PoolPair.Value.AvailableWidgets.Num();
        Total += PoolPair.Value.InUseWidgets.Num();
    }
    
    return Total;
}

int32 USuspenseTooltipManager::GetActiveTooltipCount() const
{
    int32 Active = 0;
    
    for (const auto& PoolPair : TooltipPools)
    {
        Active += PoolPair.Value.InUseWidgets.Num();
    }
    
    return Active;
}

void USuspenseTooltipManager::GetPoolStats(TSubclassOf<USuspenseItemTooltipWidget> TooltipClass, 
    int32& OutPoolSize, int32& OutInUse) const
{
    OutPoolSize = 0;
    OutInUse = 0;
    
    if (!TooltipClass)
    {
        return;
    }
    
    const FTooltipPool* Pool = TooltipPools.Find(TooltipClass);
    if (Pool)
    {
        OutPoolSize = Pool->AvailableWidgets.Num();
        OutInUse = Pool->InUseWidgets.Num();
    }
}

void USuspenseTooltipManager::ClearAllPools()
{
    UE_LOG(LogTemp, Log, TEXT("[TooltipManager] Clearing all tooltip pools"));
    
    ForceHideTooltip();
    CleanupAllPools();
}

void USuspenseTooltipManager::LogTooltipSystemState() const
{
    UE_LOG(LogTemp, Warning, TEXT("[TooltipManager] === Tooltip System State ==="));
    UE_LOG(LogTemp, Warning, TEXT("Configuration:"));
    UE_LOG(LogTemp, Warning, TEXT("  Default Class: %s"), 
        Configuration.DefaultTooltipClass ? *Configuration.DefaultTooltipClass->GetName() : TEXT("None"));
    UE_LOG(LogTemp, Warning, TEXT("  Z-Order: %d"), Configuration.TooltipZOrder);
    UE_LOG(LogTemp, Warning, TEXT("  Max Pool Size: %d"), Configuration.MaxPooledTooltipsPerClass);
    UE_LOG(LogTemp, Warning, TEXT("  Multiple Classes: %s"), 
        Configuration.bAllowMultipleTooltipClasses ? TEXT("Yes") : TEXT("No"));
    
    UE_LOG(LogTemp, Warning, TEXT("Active Tooltip:"));
    UE_LOG(LogTemp, Warning, TEXT("  Active: %s"), ActiveTooltip ? TEXT("Yes") : TEXT("No"));
    UE_LOG(LogTemp, Warning, TEXT("  Class: %s"), 
        ActiveTooltipClass ? *ActiveTooltipClass->GetName() : TEXT("None"));
    
    UE_LOG(LogTemp, Warning, TEXT("Pools (%d total):"), TooltipPools.Num());
    
    for (const auto& PoolPair : TooltipPools)
    {
        const FTooltipPool& Pool = PoolPair.Value;
        UE_LOG(LogTemp, Warning, TEXT("  %s:"), *PoolPair.Key->GetName());
        UE_LOG(LogTemp, Warning, TEXT("    Available: %d"), Pool.AvailableWidgets.Num());
        UE_LOG(LogTemp, Warning, TEXT("    In Use: %d"), Pool.InUseWidgets.Num());
        UE_LOG(LogTemp, Warning, TEXT("    Total Created: %d"), Pool.TotalCreated);
        UE_LOG(LogTemp, Warning, TEXT("    Max Size: %d"), Pool.MaxPoolSize);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Summary:"));
    UE_LOG(LogTemp, Warning, TEXT("  Total Widgets: %d"), GetTotalTooltipCount());
    UE_LOG(LogTemp, Warning, TEXT("  Active Widgets: %d"), GetActiveTooltipCount());
    UE_LOG(LogTemp, Warning, TEXT("========================="));
}

// ========================================
// Private Helper Methods
// ========================================

void USuspenseTooltipManager::CleanupTooltipPool(UClass* TooltipClass)
{
    FTooltipPool* Pool = TooltipPools.Find(TooltipClass);
    if (!Pool)
    {
        return;
    }
    
    // Destroy all widgets in pool
    for (USuspenseItemTooltipWidget* Widget : Pool->AvailableWidgets)
    {
        if (IsValid(Widget))
        {
            Widget->RemoveFromParent();
            Widget->ConditionalBeginDestroy();
        }
    }
    
    for (USuspenseItemTooltipWidget* Widget : Pool->InUseWidgets)
    {
        if (IsValid(Widget))
        {
            Widget->RemoveFromParent();
            Widget->ConditionalBeginDestroy();
        }
    }
    
    Pool->AvailableWidgets.Empty();
    Pool->InUseWidgets.Empty();
}

void USuspenseTooltipManager::CleanupAllPools()
{
    for (auto& PoolPair : TooltipPools)
    {
        CleanupTooltipPool(PoolPair.Key);
    }
    
    TooltipPools.Empty();
}

APlayerController* USuspenseTooltipManager::GetOwningPlayerController() const
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UWorld* World = GameInstance->GetWorld())
        {
            return World->GetFirstPlayerController();
        }
    }
    
    return nullptr;
}

UEventDelegateManager* USuspenseTooltipManager::GetEventManager() const
{
    if (CachedEventManager)
    {
        return CachedEventManager;
    }
    
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        return GameInstance->GetSubsystem<UEventDelegateManager>();
    }
    
    return nullptr;
}

TSubclassOf<UUserWidget> USuspenseTooltipManager::DetermineTooltipClass(const FItemUIData& ItemData) const
{
    // Priority 1: Custom class from item data (if set by slot)
    if (ItemData.PreferredTooltipClass && Configuration.bAllowMultipleTooltipClasses)
    {
        // Verify it's derived from our base tooltip class
        if (ItemData.PreferredTooltipClass->IsChildOf(USuspenseItemTooltipWidget::StaticClass()))
        {
            LogVerbose(FString::Printf(TEXT("Using custom tooltip class from item: %s"), 
                *ItemData.PreferredTooltipClass->GetName()));
            return ItemData.PreferredTooltipClass;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[TooltipManager] PreferredTooltipClass %s is not derived from USuspenseItemTooltipWidget"), 
                *ItemData.PreferredTooltipClass->GetName());
        }
    }
    
    // Priority 2: Default configured class
    if (Configuration.DefaultTooltipClass)
    {
        LogVerbose(FString::Printf(TEXT("Using default tooltip class: %s"), 
            *Configuration.DefaultTooltipClass->GetName()));
        return Configuration.DefaultTooltipClass;
    }
    
    return nullptr;
}

void USuspenseTooltipManager::LogVerbose(const FString& Message) const
{
    if (Configuration.bEnableDetailedLogging)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[TooltipManager] %s"), *Message);
    }
}