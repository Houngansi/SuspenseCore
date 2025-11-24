// Copyright MedCom Team. All Rights Reserved.

#include "Widgets/DragDrop/MedComDragVisualWidget.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Engine/Texture2D.h"
#include "Blueprint/WidgetTree.h"
#include "Animation/WidgetAnimation.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Components/InvalidationBox.h"

// Static members
TMap<FString, TWeakObjectPtr<UTexture2D>> UMedComDragVisualWidget::IconTextureCache;
FCriticalSection UMedComDragVisualWidget::IconCacheMutex;

UMedComDragVisualWidget::UMedComDragVisualWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Initialize default values
    GridCellSize = 48.0f;
    ValidDropColor = FLinearColor(0.0f, 1.0f, 0.0f, 0.5f);
    InvalidDropColor = FLinearColor(1.0f, 0.0f, 0.0f, 0.5f);
    SnapColor = FLinearColor(0.2f, 0.8f, 1.0f, 0.6f);
    PreviewOpacity = 0.5f;
    
    bIsInitialized = false;
    bWidgetsValidated = false;
    CurrentVisualMode = EDragVisualMode::Normal;
    bIsShowingRotationPreview = false;
    CurrentSnapTarget = FVector2D::ZeroVector;
    CurrentSnapStrength = 0.0f;
    SnapAnimationTime = 0.0f;
    RotationAnimationTime = 0.0f;
    
    bLowPerformanceMode = false;
    LastVisualUpdateTime = 0.0f;
    bNeedsVisualUpdate = false;
    
    // Initialize widget references to null for safety
    RootSizeBox = nullptr;
    BackgroundBorder = nullptr;
    ItemIcon = nullptr;
    QuantityText = nullptr;
    EffectsOverlay = nullptr;
    PreviewGhost = nullptr;
    SnapIndicator = nullptr;
    StackingText = nullptr;
    
    // Enable invalidation for performance
    bHasScriptImplementedTick = false;
}

void UMedComDragVisualWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    
    // Attempt automatic widget binding if not bound in Blueprint
    AutoBindWidgets();
    
    // Set pivot point to center for proper rotation and scaling
    if (RootSizeBox)
    {
        RootSizeBox->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
    }
}

void UMedComDragVisualWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Re-attempt auto-binding if PreConstruct failed
    if (!RootSizeBox || !BackgroundBorder || !ItemIcon || !QuantityText)
    {
        AutoBindWidgets();
    }
    
    // Validate widget bindings
    if (!ValidateWidgetBindings())
    {
        return;
    }
    
    bWidgetsValidated = true;
    
    // ВАЖНО: Проверяем, что цвета установлены
    if (ValidDropColor.A == 0.0f)
    {
        ValidDropColor = FLinearColor(0.0f, 1.0f, 0.0f, 0.5f);  // Зелёный полупрозрачный
    }
    
    if (InvalidDropColor.A == 0.0f)
    {
        InvalidDropColor = FLinearColor(1.0f, 0.0f, 0.0f, 0.5f);  // Красный полупрозрачный
    }
    
    // Set initial state
    UpdateValidState(true);
    SetVisualMode(EDragVisualMode::Normal);
    
    // Wrap in invalidation box for performance
    if (UPanelWidget* Parent = GetParent())
    {
        if (!Parent->IsA<UInvalidationBox>())
        {
            // Consider wrapping in invalidation box for better performance
            // This would need to be done at the parent level
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("[DragVisualWidget] NativeConstruct - ValidColor: (%.2f,%.2f,%.2f,%.2f), InvalidColor: (%.2f,%.2f,%.2f,%.2f)"),
        ValidDropColor.R, ValidDropColor.G, ValidDropColor.B, ValidDropColor.A,
        InvalidDropColor.R, InvalidDropColor.G, InvalidDropColor.B, InvalidDropColor.A);
}

void UMedComDragVisualWidget::NativeDestruct()
{
    // Cancel async loading
    if (IconStreamingHandle.IsValid())
    {
        IconStreamingHandle->CancelHandle();
        IconStreamingHandle.Reset();
    }
    
    // Call Blueprint event
    OnDragVisualDestroyed();
    
    // Reset state
    bIsInitialized = false;
    bWidgetsValidated = false;
    CurrentVisualMode = EDragVisualMode::Normal;
    
    Super::NativeDestruct();
}

void UMedComDragVisualWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    // Throttle visual updates in low performance mode
    if (bLowPerformanceMode)
    {
        float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        if (CurrentTime - LastVisualUpdateTime < VISUAL_UPDATE_THROTTLE)
        {
            return;
        }
        LastVisualUpdateTime = CurrentTime;
    }
    
    // Update animations only if initialized
    if (bIsInitialized)
    {
        UpdateAnimations(InDeltaTime);
        
        // Process pending visual updates
        if (bNeedsVisualUpdate)
        {
            UpdateVisualsInternal();
            bNeedsVisualUpdate = false;
        }
    }
}

void UMedComDragVisualWidget::AutoBindWidgets()
{
    if (!WidgetTree)
    {
        return;
    }
    
    // Get all widgets in tree
    TArray<UWidget*> AllWidgets;
    WidgetTree->GetAllWidgets(AllWidgets);
    
    for (UWidget* Widget : AllWidgets)
    {
        if (!Widget) continue;
        
        FString WidgetName = Widget->GetName();
        
        // Try to find widgets by name
        if (!RootSizeBox && WidgetName.Contains(TEXT("RootSizeBox")))
        {
            RootSizeBox = Cast<USizeBox>(Widget);
        }
        else if (!BackgroundBorder && WidgetName.Contains(TEXT("BackgroundBorder")))
        {
            BackgroundBorder = Cast<UBorder>(Widget);
        }
        else if (!ItemIcon && WidgetName.Contains(TEXT("ItemIcon")) && Cast<UImage>(Widget))
        {
            ItemIcon = Cast<UImage>(Widget);
        }
        else if (!QuantityText && WidgetName.Contains(TEXT("QuantityText")) && Cast<UTextBlock>(Widget))
        {
            QuantityText = Cast<UTextBlock>(Widget);
        }
        else if (!EffectsOverlay && WidgetName.Contains(TEXT("EffectsOverlay")))
        {
            EffectsOverlay = Cast<UOverlay>(Widget);
        }
        else if (!PreviewGhost && WidgetName.Contains(TEXT("PreviewGhost")))
        {
            PreviewGhost = Cast<UImage>(Widget);
        }
        else if (!SnapIndicator && WidgetName.Contains(TEXT("SnapIndicator")))
        {
            SnapIndicator = Cast<UImage>(Widget);
        }
        else if (!StackingText && WidgetName.Contains(TEXT("StackingText")))
        {
            StackingText = Cast<UTextBlock>(Widget);
        }
    }
}

bool UMedComDragVisualWidget::ValidateWidgetBindings() const
{
    bool bAllValid = true;
    
    if (!RootSizeBox)
    {
        bAllValid = false;
    }
    
    if (!BackgroundBorder)
    {
        bAllValid = false;
    }
    
    if (!ItemIcon)
    {
        bAllValid = false;
    }
    
    if (!QuantityText)
    {
        bAllValid = false;
    }
    
    return bAllValid;
}

bool UMedComDragVisualWidget::InitializeDragVisual(const FDragDropUIData& InDragData, float CellSize)
{
    // Validate input data
    if (!InDragData.IsValidDragData())
    {
        return false;
    }
    
    if (CellSize <= 0.0f)
    {
        CellSize = 64.0f;
    }
    
    // Ensure widgets are validated
    if (!bWidgetsValidated)
    {
        AutoBindWidgets();
        
        if (!ValidateWidgetBindings())
        {
            return false;
        }
        bWidgetsValidated = true;
    }
    
    // Store data
    DragData = InDragData;
    GridCellSize = CellSize;
    
    // Update visuals
    if (!UpdateVisuals())
    {
        return false;
    }
    
    // ВАЖНО: Устанавливаем начальное визуальное состояние
    // По умолчанию устанавливаем как невалидное (красное), потому что:
    // 1. В начале перетаскивания курсор обычно находится над исходным слотом
    // 2. Нельзя бросить предмет на то же место, откуда взяли
    // 3. Это даёт визуальную обратную связь, что операция началась
    UpdateValidState(false);
    
    // Убедимся, что BackgroundBorder действительно получил цвет
    if (BackgroundBorder)
    {
        // Принудительно применяем начальный цвет, если UpdateValidState не сработал
        BackgroundBorder->SetBrushColor(InvalidDropColor);
        BackgroundBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    
    // Mark as initialized
    bIsInitialized = true;
    
    // Call Blueprint event
    OnDragVisualCreated();
    
    UE_LOG(LogTemp, Log, TEXT("[DragVisualWidget] Initialized with invalid state (red) for item %s"), 
        *InDragData.ItemData.ItemID.ToString());
    
    return true;
}

void UMedComDragVisualWidget::SetDragData(const FDragDropUIData& InDragData)
{
    // Validate input
    if (!InDragData.IsValidDragData())
    {
        return;
    }
    
    // Update data
    DragData = InDragData;
    
    // Schedule visual update
    InvalidateVisual();
}

void UMedComDragVisualWidget::UpdateValidState(bool bIsValid)
{
    if (!BackgroundBorder)
    {
        UE_LOG(LogTemp, Error, TEXT("[DragVisualWidget] BackgroundBorder is null!"));
        return;
    }
    
    // Всегда применяем цвет, даже если состояние не изменилось
    FLinearColor TargetColor = bIsValid ? ValidDropColor : InvalidDropColor;
    BackgroundBorder->SetBrushColor(TargetColor);
    
    // Update visual mode
    SetVisualMode(bIsValid ? EDragVisualMode::ValidTarget : EDragVisualMode::InvalidTarget);
    
    // Log для отладки
    UE_LOG(LogTemp, Log, TEXT("[DragVisualWidget] UpdateValidState: %s, Color: (%.2f, %.2f, %.2f, %.2f)"), 
        bIsValid ? TEXT("VALID") : TEXT("INVALID"),
        TargetColor.R, TargetColor.G, TargetColor.B, TargetColor.A);
}

void UMedComDragVisualWidget::SetCellSize(float CellSize)
{
    if (CellSize <= 0.0f)
    {
        return;
    }
    
    GridCellSize = CellSize;
    
    // Schedule visual update
    InvalidateVisual();
}

void UMedComDragVisualWidget::ShowPlacementPreview(const FVector2D& ScreenPosition, bool bIsValid)
{
    if (!PreviewGhost)
    {
        return;
    }
    
    // Update preview visual
    PreviewGhost->SetVisibility(ESlateVisibility::HitTestInvisible);
    
    // Set color based on validity
    FLinearColor PreviewColor = bIsValid ? ValidDropColor : InvalidDropColor;
    PreviewColor.A = PreviewOpacity;
    PreviewGhost->SetColorAndOpacity(PreviewColor);
}

void UMedComDragVisualWidget::AnimateSnapFeedback(const FVector2D& TargetPosition, float SnapStrength)
{
    CurrentSnapTarget = TargetPosition;
    CurrentSnapStrength = FMath::Clamp(SnapStrength, 0.0f, 1.0f);
    SnapAnimationTime = 0.0f;
    
    SetVisualMode(EDragVisualMode::Snapping);
    
    // Update snap indicator
    if (SnapIndicator)
    {
        SnapIndicator->SetVisibility(ESlateVisibility::HitTestInvisible);
        SnapIndicator->SetColorAndOpacity(FLinearColor(SnapColor.R, SnapColor.G, SnapColor.B, SnapColor.A * CurrentSnapStrength));
    }
    
    // Play snap animation if available
    if (SnapAnimation && !bLowPerformanceMode)
    {
        PlayAnimation(SnapAnimation, 0.0f, 0, EUMGSequencePlayMode::Forward, CurrentSnapStrength);
    }
}

void UMedComDragVisualWidget::PreviewRotation(bool bShowRotated)
{
    bIsShowingRotationPreview = bShowRotated;
    RotationAnimationTime = 0.0f;
    
    SetVisualMode(EDragVisualMode::Rotating);
    
    // Play rotation animation if available
    if (RotationAnimation && !bLowPerformanceMode)
    {
        PlayAnimation(RotationAnimation);
    }
}

void UMedComDragVisualWidget::UpdateStackingFeedback(int32 StackCount, int32 MaxStack)
{
    if (!StackingText)
    {
        return;
    }
    
    if (StackCount > 0 && MaxStack > 0)
    {
        FText StackText = FText::Format(
            NSLOCTEXT("DragVisual", "StackingFormat", "+{0}/{1}"),
            FText::AsNumber(StackCount),
            FText::AsNumber(MaxStack)
        );
        
        StackingText->SetText(StackText);
        StackingText->SetVisibility(ESlateVisibility::HitTestInvisible);
        
        SetVisualMode(EDragVisualMode::Stacking);
        
        // Play stacking animation if available
        if (StackingAnimation && !bLowPerformanceMode)
        {
            PlayAnimation(StackingAnimation);
        }
    }
    else
    {
        StackingText->SetVisibility(ESlateVisibility::Hidden);
    }
}

void UMedComDragVisualWidget::SetVisualMode(EDragVisualMode NewMode)
{
    if (CurrentVisualMode != NewMode)
    {
        CurrentVisualMode = NewMode;
        ApplyVisualMode();
        OnVisualModeChanged(NewMode);
    }
}

void UMedComDragVisualWidget::SetLowPerformanceMode(bool bEnable)
{
    bLowPerformanceMode = bEnable;
    
    if (bEnable)
    {
        // Stop all animations
        StopAllAnimations();
    }
}

bool UMedComDragVisualWidget::UpdateVisuals()
{
    // Schedule update instead of immediate
    InvalidateVisual();
    return true;
}

void UMedComDragVisualWidget::InvalidateVisual()
{
    bNeedsVisualUpdate = true;
    
    // In low performance mode, update immediately
    if (bLowPerformanceMode)
    {
        UpdateVisualsInternal();
        bNeedsVisualUpdate = false;
    }
}

void UMedComDragVisualWidget::UpdateVisualsInternal()
{
    // Validate state
    if (!bWidgetsValidated || !DragData.IsValidDragData())
    {
        return;
    }
    
    // Update size based on item grid size
    if (RootSizeBox)
    {
        FIntPoint EffectiveSize = DragData.GetEffectiveSize();
        float Width = EffectiveSize.X * GridCellSize;
        float Height = EffectiveSize.Y * GridCellSize;
        
        if (Width > 0.0f && Height > 0.0f)
        {
            RootSizeBox->SetWidthOverride(Width);
            RootSizeBox->SetHeightOverride(Height);
            RootSizeBox->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
        }
    }
    
    // Update icon
    if (ItemIcon)
    {
        if (!DragData.ItemData.IconAssetPath.IsEmpty())
        {
            // Try to get from cache first
            LoadIconAsync(DragData.ItemData.IconAssetPath);
        }
        else
        {
            ItemIcon->SetVisibility(ESlateVisibility::Hidden);
        }
        
        // Handle rotation
        float TargetRotation = (DragData.ItemData.bIsRotated || bIsShowingRotationPreview) ? 90.0f : 0.0f;
        ItemIcon->SetRenderTransformAngle(TargetRotation);
        ItemIcon->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
    }
    
    // Update quantity text
    if (QuantityText)
    {
        bool bShouldShowQuantity = false;
        int32 DisplayQuantity = 1;
        
        if (DragData.bIsSplitStack && DragData.DraggedQuantity > 0)
        {
            DisplayQuantity = DragData.DraggedQuantity;
            bShouldShowQuantity = true;
        }
        else if (DragData.ItemData.Quantity > 1)
        {
            DisplayQuantity = DragData.ItemData.Quantity;
            bShouldShowQuantity = true;
        }
        
        if (bShouldShowQuantity)
        {
            QuantityText->SetText(FText::AsNumber(DisplayQuantity));
            QuantityText->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            QuantityText->SetVisibility(ESlateVisibility::Hidden);
        }
    }
}

void UMedComDragVisualWidget::LoadIconAsync(const FString& IconPath)
{
    // Check cache first
    {
        FScopeLock Lock(&IconCacheMutex);
        if (auto* CachedTexture = IconTextureCache.Find(IconPath))
        {
            if (CachedTexture->IsValid())
            {
                // Use cached texture immediately
                ItemIcon->SetBrushFromTexture(CachedTexture->Get());
                ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
                return;
            }
        }
    }
    
    // Load texture asynchronously
    FSoftObjectPath SoftPath(IconPath);
    if (!SoftPath.IsValid())
    {
        return;
    }
    
    // Cancel previous loading
    if (IconStreamingHandle.IsValid())
    {
        IconStreamingHandle->CancelHandle();
    }
    
    // Start async load
    FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
    PendingIconTexture = TSoftObjectPtr<UTexture2D>(SoftPath);
    
    IconStreamingHandle = StreamableManager.RequestAsyncLoad(
        SoftPath,
        FStreamableDelegate::CreateUObject(this, &UMedComDragVisualWidget::OnIconLoaded),
        FStreamableManager::AsyncLoadHighPriority
    );
}

void UMedComDragVisualWidget::OnIconLoaded()
{
    if (!ItemIcon || !PendingIconTexture.IsValid())
    {
        return;
    }
    
    if (UTexture2D* LoadedTexture = PendingIconTexture.Get())
    {
        // Cache the texture
        {
            FScopeLock Lock(&IconCacheMutex);
            IconTextureCache.Add(DragData.ItemData.IconAssetPath, LoadedTexture);
        }
        
        // Apply texture
        ItemIcon->SetBrushFromTexture(LoadedTexture);
        ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    
    IconStreamingHandle.Reset();
}

void UMedComDragVisualWidget::ResetVisual()
{
    // Cancel async operations
    if (IconStreamingHandle.IsValid())
    {
        IconStreamingHandle->CancelHandle();
        IconStreamingHandle.Reset();
    }
    
    // Reset size
    if (RootSizeBox)
    {
        RootSizeBox->SetWidthOverride(GridCellSize);
        RootSizeBox->SetHeightOverride(GridCellSize);
    }
    
    // Hide all elements
    if (ItemIcon)
    {
        ItemIcon->SetVisibility(ESlateVisibility::Hidden);
        ItemIcon->SetRenderTransformAngle(0.0f);
    }
    
    if (QuantityText)
    {
        QuantityText->SetVisibility(ESlateVisibility::Hidden);
    }
    
    if (PreviewGhost)
    {
        PreviewGhost->SetVisibility(ESlateVisibility::Hidden);
    }
    
    if (SnapIndicator)
    {
        SnapIndicator->SetVisibility(ESlateVisibility::Hidden);
    }
    
    if (StackingText)
    {
        StackingText->SetVisibility(ESlateVisibility::Hidden);
    }
    
    // Reset state
    SetVisualMode(EDragVisualMode::Normal);
    UpdateValidState(true);
    
    DragData = FDragDropUIData();
    bIsInitialized = false;
    bIsShowingRotationPreview = false;
    CurrentSnapStrength = 0.0f;
    bNeedsVisualUpdate = false;
}

void UMedComDragVisualWidget::UpdateAnimations(float DeltaTime)
{
    // Skip animations in low performance mode
    if (bLowPerformanceMode)
    {
        return;
    }
    
    // Update snap animation
    if (CurrentVisualMode == EDragVisualMode::Snapping && CurrentSnapStrength > 0.0f)
    {
        SnapAnimationTime += DeltaTime;
        
        // Pulsing effect for snap indicator
        if (SnapIndicator)
        {
            float PulseValue = (FMath::Sin(SnapAnimationTime * 4.0f) + 1.0f) * 0.5f;
            float Alpha = SnapColor.A * CurrentSnapStrength * (0.5f + PulseValue * 0.5f);
            SnapIndicator->SetColorAndOpacity(FLinearColor(SnapColor.R, SnapColor.G, SnapColor.B, Alpha));
        }
    }
    
    // Update rotation animation
    if (CurrentVisualMode == EDragVisualMode::Rotating)
    {
        RotationAnimationTime += DeltaTime;
        
        if (ItemIcon)
        {
            float TargetAngle = bIsShowingRotationPreview ? 90.0f : 0.0f;
            // Используем GetRenderTransformAngle() вместо прямого доступа
            float CurrentAngle = ItemIcon->GetRenderTransformAngle();
            float NewAngle = FMath::FInterpTo(CurrentAngle, TargetAngle, DeltaTime, 10.0f);
            ItemIcon->SetRenderTransformAngle(NewAngle);
        }
    }
}

void UMedComDragVisualWidget::ApplyVisualMode()
{
    // Apply visual changes based on mode
    switch (CurrentVisualMode)
    {
    case EDragVisualMode::Normal:
        if (BackgroundBorder)
        {
            BackgroundBorder->SetBrushColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.8f));
        }
        break;
            
    case EDragVisualMode::ValidTarget:
        if (BackgroundBorder)
        {
            // Ensure we're using the valid (green) color
            BackgroundBorder->SetBrushColor(ValidDropColor);
            
            // Also update any highlight border if it exists
            if (UBorder* HighlightBorder = GetHighlightBorder())
            {
                HighlightBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
                HighlightBorder->SetBrushColor(ValidDropColor);
            }
        }
        break;
            
    case EDragVisualMode::InvalidTarget:
        if (BackgroundBorder)
        {
            // Ensure we're using the invalid (red) color
            BackgroundBorder->SetBrushColor(InvalidDropColor);
            
            // Also update any highlight border if it exists
            if (UBorder* HighlightBorder = GetHighlightBorder())
            {
                HighlightBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
                HighlightBorder->SetBrushColor(InvalidDropColor);
            }
        }
        if (InvalidAnimation && !bLowPerformanceMode)
        {
            PlayAnimation(InvalidAnimation);
        }
        break;
            
    case EDragVisualMode::Snapping:
        if (BackgroundBorder)
        {
            BackgroundBorder->SetBrushColor(SnapColor);
        }
        break;
            
    case EDragVisualMode::Stacking:
        if (BackgroundBorder)
        {
            BackgroundBorder->SetBrushColor(FLinearColor(0.8f, 0.6f, 0.2f, 0.6f));
        }
        break;
            
    case EDragVisualMode::Rotating:
        if (BackgroundBorder)
        {
            BackgroundBorder->SetBrushColor(FLinearColor(0.6f, 0.4f, 0.8f, 0.6f));
        }
        break;
    }
    
    if (!bLowPerformanceMode)
    {
        PlayModeAnimation(CurrentVisualMode);
    }
}

UBorder* UMedComDragVisualWidget::GetHighlightBorder() const
{
    if (!WidgetTree)
    {
        return nullptr;
    }
    
    TArray<UWidget*> AllWidgets;
    WidgetTree->GetAllWidgets(AllWidgets);
    
    for (UWidget* Widget : AllWidgets)
    {
        if (Widget && Widget->GetName().Contains(TEXT("HighlightBorder")))
        {
            return Cast<UBorder>(Widget);
        }
    }
    
    return nullptr;
}

void UMedComDragVisualWidget::PlayModeAnimation(EDragVisualMode Mode)
{
    // Stop all animations first
    StopAllAnimations();
    
    // Play appropriate animation
    switch (Mode)
    {
        case EDragVisualMode::InvalidTarget:
            if (InvalidAnimation)
            {
                PlayAnimation(InvalidAnimation, 0.0f, 0, EUMGSequencePlayMode::PingPong);
            }
            break;
            
        case EDragVisualMode::Snapping:
            if (SnapAnimation)
            {
                PlayAnimation(SnapAnimation, 0.0f, 0, EUMGSequencePlayMode::Forward, CurrentSnapStrength);
            }
            break;
            
        case EDragVisualMode::Stacking:
            if (StackingAnimation)
            {
                PlayAnimation(StackingAnimation);
            }
            break;
            
        case EDragVisualMode::Rotating:
            if (RotationAnimation)
            {
                PlayAnimation(RotationAnimation);
            }
            break;
            
        default:
            break;
    }
}