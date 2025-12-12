// MedComBaseSlotWidget.cpp - Optimized version with tooltip support through EventBus
#include "Widgets/Base/SuspenseBaseSlotWidget.h"
#include "Widgets/Base/SuspenseBaseContainerWidget.h"
#include "Widgets/DragDrop/SuspenseDragDropOperation.h"
#include "DragDrop/SuspenseDragDropHandler.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Blueprint/WidgetLayoutLibrary.h"

USuspenseBaseSlotWidget::USuspenseBaseSlotWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Initialize state
    bIsSelected = false;
    bIsHovered = false;
    bIsHighlighted = false;
    bIsLocked = false;
    bIsDragging = false;
    bIsPooled = false;
    bIsTooltipActive = false;

    OwningContainer = nullptr;
    CachedDragDropHandler = nullptr;
    CachedEventManager = nullptr;
    CurrentHighlightColor = FLinearColor::White;

    // Performance settings
    bNeedsVisualUpdate = false;
    LastVisualUpdateTime = 0.0f;
    bGeometryCached = false;
    GeometryCacheTime = 0.0f;

    // Cache colors
    CachedBackgroundColor = EmptySlotColor;
    CachedHighlightColor = FLinearColor::White;

    // Tooltip settings
    bEnableTooltip = true;
    TooltipDelay = 0.5f;

    // Disable tick by default - we'll use invalidation
    bHasScriptImplementedTick = false;
}

void USuspenseBaseSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Validate widget bindings
    if (!ValidateWidgetBindings())
    {
        return;
    }

    // Validate owning container
    if (!OwningContainer)
    {
        UE_LOG(LogTemp, Error, TEXT("[%s] OwningContainer not set!"), *GetName());
    }

    // Cache managers
    CachedDragDropHandler = GetDragDropHandler();
    CachedEventManager = GetEventManager();

    // Set initial size
    if (RootSizeBox)
    {
        RootSizeBox->SetWidthOverride(SlotSize);
        RootSizeBox->SetHeightOverride(SlotSize);
    }

    // Initialize visual state
    UpdateVisualState();

    // Make sure we can receive mouse events
    SetVisibility(ESlateVisibility::Visible);
}

void USuspenseBaseSlotWidget::NativeDestruct()
{
    // Clean up tooltip
    CancelTooltipTimer();
    HideTooltip();

    // Cancel any async loading
    if (IconStreamingHandle.IsValid())
    {
        IconStreamingHandle->CancelHandle();
        IconStreamingHandle.Reset();
    }

    CachedDragDropHandler = nullptr;
    CachedEventManager = nullptr;
    OwningContainer = nullptr;

    Super::NativeDestruct();
}

void USuspenseBaseSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Process pending visual updates
    if (bNeedsVisualUpdate)
    {
        ProcessPendingVisualUpdates();
    }

    // Update tooltip position if active
    if (bIsTooltipActive && bIsHovered)
    {
        UpdateTooltipPosition();
    }
}

void USuspenseBaseSlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

    // Update cached geometry
    UpdateCachedGeometry(InGeometry);

    bIsHovered = true;
    ScheduleVisualUpdate();

    // Start tooltip timer
    StartTooltipTimer();
}

void USuspenseBaseSlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseLeave(InMouseEvent);

    bIsHovered = false;
    ScheduleVisualUpdate();

    // Cancel tooltip
    CancelTooltipTimer();
    HideTooltip();
}

FReply USuspenseBaseSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bIsLocked)
    {
        return FReply::Handled();
    }

    // Update cached geometry
    UpdateCachedGeometry(InGeometry);

    // Hide tooltip on interaction
    HideTooltip();

    // Handle different mouse buttons
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        // Check if we can drag ONLY if occupied
        if (CurrentSlotData.bIsOccupied && CanBeDragged_Implementation())
        {
            return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
        }

        // Handle normal click
        HandleClick();
        return FReply::Handled();
    }
    else if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        HandleRightClick();
        return FReply::Handled();
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USuspenseBaseSlotWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bIsLocked)
    {
        return FReply::Handled();
    }

    // Hide tooltip on interaction
    HideTooltip();

    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        HandleDoubleClick();
        return FReply::Handled();
    }

    return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
}

void USuspenseBaseSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

    // Validate drag possibility
    if (!CanBeDragged_Implementation() || !CachedDragDropHandler)
    {
        OutOperation = nullptr;
        return;
    }

    // Delegate to handler to create operation
    OutOperation = CachedDragDropHandler->StartDragOperation(this, InMouseEvent);

    if (OutOperation)
    {
        // Create visual if needed
        USuspenseDragDropOperation* MedComOp = Cast<USuspenseDragDropOperation>(OutOperation);
        if (MedComOp && !MedComOp->DefaultDragVisual && OwningContainer)
        {
            const FDragDropUIData& DragData = MedComOp->GetDragData();
            if (USuspenseDragVisualWidget* DragVisual = OwningContainer->CreateDragVisualWidget(DragData))
            {
                MedComOp->DefaultDragVisual = DragVisual;
            }
        }

        // Notify drag started
        OnDragStarted_Implementation();
    }
}

bool USuspenseBaseSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    USuspenseDragDropOperation* DragOp = Cast<USuspenseDragDropOperation>(InOperation);
    if (!DragOp || !IsValid(OwningContainer))
    {
        return false;
    }

    // Delegate to container
    return OwningContainer->ProcessDropOnSlot(
        DragOp,
        this,
        InDragDropEvent.GetScreenSpacePosition(),
        InGeometry
    );
}

bool USuspenseBaseSlotWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    USuspenseDragDropOperation* DragOp = Cast<USuspenseDragDropOperation>(InOperation);
    if (!DragOp || !IsValid(OwningContainer))
    {
        return false;
    }

    // Delegate to container
    return OwningContainer->ProcessDragOverSlot(
        DragOp,
        this,
        InDragDropEvent.GetScreenSpacePosition(),
        InGeometry
    );
}

void USuspenseBaseSlotWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

    if (!IsValid(InOperation) || !IsValid(OwningContainer))
    {
        return;
    }

    USuspenseDragDropOperation* MedComDragOp = Cast<USuspenseDragDropOperation>(InOperation);
    if (MedComDragOp)
    {
        OwningContainer->ProcessDragEnterSlot(MedComDragOp, this);
    }
}

void USuspenseBaseSlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDragLeave(InDragDropEvent, InOperation);

    OnDragLeave_Implementation();
}

void USuspenseBaseSlotWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDragCancelled(InDragDropEvent, InOperation);

    if (bIsDragging)
    {
        OnDragEnded_Implementation(false);
    }
}

void USuspenseBaseSlotWidget::InitializeSlot_Implementation(const FSlotUIData& SlotData, const FItemUIData& ItemData)
{
    CurrentSlotData = SlotData;
    CurrentItemData = ItemData;

    // Clear any cached icon
    CachedIconTexture = nullptr;

    // Load icon if needed
    if (ItemData.IsValid() && !ItemData.IconAssetPath.IsEmpty())
    {
        LoadIconAsync(ItemData.IconAssetPath);
    }

    ScheduleVisualUpdate();
}

void USuspenseBaseSlotWidget::UpdateSlot_Implementation(const FSlotUIData& SlotData, const FItemUIData& ItemData)
{
    // Check if data actually changed
    bool bDataChanged = CurrentSlotData.SlotIndex != SlotData.SlotIndex ||
                       CurrentSlotData.bIsOccupied != SlotData.bIsOccupied ||
                       CurrentSlotData.bIsAnchor != SlotData.bIsAnchor ||
                       CurrentSlotData.bIsPartOfItem != SlotData.bIsPartOfItem ||
                       CurrentItemData.ItemInstanceID != ItemData.ItemInstanceID ||
                       CurrentItemData.Quantity != ItemData.Quantity;

    if (!bDataChanged)
    {
        return; // No update needed
    }

    CurrentSlotData = SlotData;
    CurrentItemData = ItemData;

    // Check if icon changed
    if (ItemData.IconAssetPath != CurrentItemData.IconAssetPath)
    {
        CachedIconTexture = nullptr;
        if (ItemData.IsValid() && !ItemData.IconAssetPath.IsEmpty())
        {
            LoadIconAsync(ItemData.IconAssetPath);
        }
    }

    ScheduleVisualUpdate();
}

void USuspenseBaseSlotWidget::SetSelected_Implementation(bool bInIsSelected)
{
    if (bIsSelected == bInIsSelected)
    {
        return;
    }

    bIsSelected = bInIsSelected;
    ScheduleVisualUpdate();

    // Notify container
    if (IsValid(OwningContainer))
    {
        OwningContainer->OnSlotSelectionChanged(CurrentSlotData.SlotIndex, bIsSelected);
    }
}

void USuspenseBaseSlotWidget::SetHighlighted_Implementation(bool bInIsHighlighted, const FLinearColor& HighlightColor)
{
    if (bIsHighlighted == bInIsHighlighted && CurrentHighlightColor == HighlightColor)
    {
        return;
    }

    bIsHighlighted = bInIsHighlighted;
    CurrentHighlightColor = HighlightColor;
    CachedHighlightColor = HighlightColor;

    // Apply highlight immediately
    UpdateHighlightVisual();

    UE_LOG(LogTemp, VeryVerbose, TEXT("[Slot %d] SetHighlighted: %s, Color=(%.2f,%.2f,%.2f,%.2f)"),
        CurrentSlotData.SlotIndex,
        bIsHighlighted ? TEXT("ON") : TEXT("OFF"),
        HighlightColor.R, HighlightColor.G, HighlightColor.B, HighlightColor.A);
}

void USuspenseBaseSlotWidget::SetLocked_Implementation(bool bInIsLocked)
{
    if (bIsLocked == bInIsLocked)
    {
        return;
    }

    bIsLocked = bInIsLocked;
    ScheduleVisualUpdate();
}

bool USuspenseBaseSlotWidget::CanBeDragged_Implementation() const
{
    return !bIsLocked && !bIsPooled && CurrentSlotData.bIsOccupied && CurrentSlotData.bIsAnchor;
}

FDragDropUIData USuspenseBaseSlotWidget::GetDragData_Implementation() const
{
    FGameplayTag SourceContainerType = FGameplayTag::RequestGameplayTag(TEXT("Container.Inventory"));
    if (IsValid(OwningContainer))
    {
        SourceContainerType = ISuspenseCoreContainerUI::Execute_GetContainerType(OwningContainer);
    }

    return FDragDropUIData::CreateValidated(
        CurrentItemData,
        SourceContainerType,
        CurrentSlotData.SlotIndex
    );
}

void USuspenseBaseSlotWidget::OnDragStarted_Implementation()
{
    bIsDragging = true;

    // Hide tooltip
    HideTooltip();

    // Make icon semi-transparent
    if (ItemIcon)
    {
        ItemIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.5f));
    }

    ScheduleVisualUpdate();
}

void USuspenseBaseSlotWidget::OnDragEnded_Implementation(bool bWasDropped)
{
    bIsDragging = false;

    // Restore icon opacity
    if (ItemIcon)
    {
        ItemIcon->SetColorAndOpacity(FLinearColor::White);
    }

    ScheduleVisualUpdate();
}

void USuspenseBaseSlotWidget::UpdateDragVisual_Implementation(bool bIsValidTarget)
{
    // Visual feedback handled by drag visual widget and handler
}

FSlotValidationResult USuspenseBaseSlotWidget::CanAcceptDrop_Implementation(const UDragDropOperation* DragOperation) const
{
    if (!IsValid(DragOperation))
    {
        return FSlotValidationResult::Failure(FText::FromString(TEXT("Invalid drag operation")));
    }

    if (bIsLocked)
    {
        return FSlotValidationResult::Failure(FText::FromString(TEXT("Slot is locked")));
    }

    if (!IsValid(OwningContainer))
    {
        return FSlotValidationResult::Failure(FText::FromString(TEXT("No owning container")));
    }

    // Делегируем контейнеру (он возвращает FSlotValidationResult)
    return ISuspenseCoreContainerUI::Execute_CanAcceptDrop(
        OwningContainer,
        DragOperation,
        CurrentSlotData.SlotIndex
    );
}

bool USuspenseBaseSlotWidget::HandleDrop_Implementation(UDragDropOperation* DragOperation)
{
    // This should not be called directly - handled by container
    return false;
}

void USuspenseBaseSlotWidget::OnDragEnter_Implementation(UDragDropOperation* DragOperation)
{
    // Handled in NativeOnDragEnter
}

void USuspenseBaseSlotWidget::OnDragLeave_Implementation()
{
    if (IsValid(OwningContainer))
    {
        OwningContainer->ClearSlotHighlights();
    }
}

// === Tooltip Interface Implementation ===

FItemUIData USuspenseBaseSlotWidget::GetTooltipData_Implementation() const
{
    return CurrentItemData;
}

bool USuspenseBaseSlotWidget::CanShowTooltip_Implementation() const
{
    return bEnableTooltip &&
           CurrentSlotData.bIsOccupied &&
           CurrentSlotData.bIsAnchor &&
           CurrentItemData.IsValid() &&
           !bIsDragging;
}

float USuspenseBaseSlotWidget::GetTooltipDelay_Implementation() const
{
    return TooltipDelay;
}

void USuspenseBaseSlotWidget::OnTooltipShown_Implementation()
{
    bIsTooltipActive = true;
}

void USuspenseBaseSlotWidget::OnTooltipHidden_Implementation()
{
    bIsTooltipActive = false;
}

// === Tooltip Management ===

void USuspenseBaseSlotWidget::StartTooltipTimer()
{
    if (!CanShowTooltip_Implementation())
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            TooltipShowTimer,
            this,
            &USuspenseBaseSlotWidget::ShowTooltipDelayed,
            TooltipDelay,
            false
        );
    }
}

void USuspenseBaseSlotWidget::CancelTooltipTimer()
{
    if (UWorld* World = GetWorld())
    {
        if (TooltipShowTimer.IsValid())
        {
            World->GetTimerManager().ClearTimer(TooltipShowTimer);
        }
    }
}

void USuspenseBaseSlotWidget::ShowTooltipDelayed()
{
    if (!bIsHovered || !CanShowTooltip_Implementation())
    {
        return;
    }

    USuspenseCoreEventBus* EventBus = GetEventBus();
    if (!EventBus)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Slot %d] No EventBus available for tooltip"),
            CurrentSlotData.SlotIndex);
        return;
    }

    // Get current mouse position directly from PlayerController
    APlayerController* PC = GetOwningPlayer();
    if (!PC)
    {
        return;
    }

    // Get raw mouse position
    float MouseX, MouseY;
    if (!PC->GetMousePosition(MouseX, MouseY))
    {
        // Fallback to scaled mouse position
        if (!UWidgetLayoutLibrary::GetMousePositionScaledByDPI(PC, MouseX, MouseY))
        {
            return;
        }
    }

    // Convert to viewport space (no additional scaling needed for raw mouse position)
    FVector2D ViewportPosition = FVector2D(MouseX, MouseY);

    // Create event data for tooltip request
    FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
    EventData.SetObject(TEXT("SourceWidget"), this);
    EventData.SetString(TEXT("ItemInstanceID"), CurrentItemData.ItemInstanceID.ToString());
    EventData.SetString(TEXT("DisplayName"), CurrentItemData.DisplayName.ToString());
    EventData.SetString(TEXT("Description"), CurrentItemData.Description.ToString());
    EventData.SetString(TEXT("Rarity"), UEnum::GetValueAsString(CurrentItemData.Rarity));
    EventData.SetFloat(TEXT("PositionX"), ViewportPosition.X);
    EventData.SetFloat(TEXT("PositionY"), ViewportPosition.Y);
    EventData.SetInt(TEXT("Quantity"), CurrentItemData.Quantity);

    if (CustomTooltipClass)
    {
        EventData.SetObject(TEXT("CustomTooltipClass"), CustomTooltipClass.GetDefaultObject());
    }

    // Publish tooltip request event
    FGameplayTag TooltipTag = FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.UI.Tooltip.Requested"));
    EventBus->Publish(TooltipTag, EventData);

    // Mark as active
    bIsTooltipActive = true;

    UE_LOG(LogTemp, Verbose, TEXT("[Slot %d] Requested tooltip at mouse position: (%.1f, %.1f) for item: %s%s"),
        CurrentSlotData.SlotIndex,
        ViewportPosition.X,
        ViewportPosition.Y,
        *CurrentItemData.DisplayName.ToString(),
        CustomTooltipClass ? TEXT(" (with custom class)") : TEXT(""));
}

void USuspenseBaseSlotWidget::HideTooltip()
{
    CancelTooltipTimer();

    if (bIsTooltipActive)
    {
        if (USuspenseCoreEventBus* EventBus = GetEventBus())
        {
            // Hide tooltip through EventBus
            FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);

            FGameplayTag HideTag = FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.UI.Tooltip.Hide"));
            EventBus->Publish(HideTag, EventData);

            UE_LOG(LogTemp, Verbose, TEXT("[Slot %d] Requested tooltip hide through EventBus"),
                CurrentSlotData.SlotIndex);
        }

        bIsTooltipActive = false;
    }
}

void USuspenseBaseSlotWidget::UpdateTooltipPosition()
{
    if (!bIsTooltipActive)
    {
        return;
    }

    USuspenseCoreEventBus* EventBus = GetEventBus();
    if (!EventBus)
    {
        return;
    }

    APlayerController* PC = GetOwningPlayer();
    if (!PC)
    {
        return;
    }

    // Get raw mouse position for accurate tracking
    float MouseX, MouseY;
    if (!PC->GetMousePosition(MouseX, MouseY))
    {
        // Fallback to scaled position
        if (!UWidgetLayoutLibrary::GetMousePositionScaledByDPI(PC, MouseX, MouseY))
        {
            return;
        }
    }

    FVector2D ViewportPosition = FVector2D(MouseX, MouseY);

    // Update position through EventBus
    FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
    EventData.SetFloat(TEXT("PositionX"), ViewportPosition.X);
    EventData.SetFloat(TEXT("PositionY"), ViewportPosition.Y);

    FGameplayTag UpdateTag = FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.UI.Tooltip.UpdatePosition"));
    EventBus->Publish(UpdateTag, EventData);
}

// === Other Methods ===

void USuspenseBaseSlotWidget::SetOwningContainer(USuspenseBaseContainerWidget* Container)
{
    OwningContainer = Container;
}

void USuspenseBaseSlotWidget::SetSnapTarget(bool bIsTarget, float SnapStrength)
{
    // Visual feedback for snapping
    if (bIsTarget && SnapStrength > 0.0f)
    {
        // Could apply visual effect based on snap strength
        if (BackgroundBorder)
        {
            FLinearColor SnapColor = FLinearColor::LerpUsingHSV(
                GetBackgroundColor(),
                FLinearColor(0.2f, 0.8f, 0.2f, 1.0f),
                SnapStrength * 0.6f
            );
            BackgroundBorder->SetBrushColor(SnapColor);
        }
    }
    else
    {
        UpdateBackgroundVisual();
    }
}

void USuspenseBaseSlotWidget::ResetForPool()
{
    // Reset all state
    bIsSelected = false;
    bIsHovered = false;
    bIsHighlighted = false;
    bIsLocked = false;
    bIsDragging = false;
    bIsTooltipActive = false;

    CurrentSlotData = FSlotUIData();
    CurrentItemData = FItemUIData();
    CurrentHighlightColor = FLinearColor::White;

    // Cancel any timers
    CancelTooltipTimer();

    // Cancel any async loading
    if (IconStreamingHandle.IsValid())
    {
        IconStreamingHandle->CancelHandle();
        IconStreamingHandle.Reset();
    }

    // Clear cached data
    CachedIconTexture = nullptr;
    bGeometryCached = false;

    // Reset visuals
    UpdateVisualState();

    // Mark as pooled
    bIsPooled = true;
}

bool USuspenseBaseSlotWidget::CanBePooled() const
{
    // Can be pooled if not dragging and no async operations
    return !bIsDragging && !IconStreamingHandle.IsValid() && !bIsTooltipActive;
}

void USuspenseBaseSlotWidget::ScheduleVisualUpdate()
{
    bNeedsVisualUpdate = true;

    // Enable tick only when needed
    if (!bHasScriptImplementedTick)
    {
        bNeedsVisualUpdate = true;
        bHasScriptImplementedTick = true;
    }
}

void USuspenseBaseSlotWidget::ProcessPendingVisualUpdates()
{
    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    // Throttle updates
    if (CurrentTime - LastVisualUpdateTime < VISUAL_UPDATE_THROTTLE)
    {
        return;
    }

    UpdateVisualState();

    LastVisualUpdateTime = CurrentTime;
    bNeedsVisualUpdate = false;

    // Disable tick when not needed
    if (bHasScriptImplementedTick)
    {
        bNeedsVisualUpdate = false;
        bHasScriptImplementedTick = false;
    }
}

void USuspenseBaseSlotWidget::UpdateVisualState()
{
    UpdateBackgroundVisual();
    UpdateItemIcon();
    UpdateQuantityText();
    UpdateHighlightVisual();
    UpdateSelectionVisual();
}

void USuspenseBaseSlotWidget::UpdateBackgroundVisual()
{
    if (!BackgroundBorder)
    {
        return;
    }

    FLinearColor NewColor = GetBackgroundColor();
    BackgroundBorder->SetBrushColor(NewColor);
}

void USuspenseBaseSlotWidget::UpdateItemIcon()
{
    if (!ItemIcon)
    {
        return;
    }

    if (CurrentSlotData.bIsOccupied && CurrentSlotData.bIsAnchor && (CurrentItemData.GetIcon() || CachedIconTexture))
    {
        ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);

        // Use cached texture if available
        if (CachedIconTexture)
        {
            ItemIcon->SetBrushFromTexture(CachedIconTexture);
        }
        else if (CurrentItemData.GetIcon())
        {
            ItemIcon->SetBrushFromTexture(CurrentItemData.GetIcon());
        }

        // Handle rotation
        ItemIcon->SetRenderTransformAngle(CurrentItemData.bIsRotated ? 90.0f : 0.0f);

        // Apply drag opacity
        if (bIsDragging)
        {
            ItemIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.5f));
        }
        else
        {
            ItemIcon->SetColorAndOpacity(FLinearColor::White);
        }
    }
    else
    {
        ItemIcon->SetVisibility(ESlateVisibility::Hidden);
    }
}

void USuspenseBaseSlotWidget::UpdateQuantityText()
{
    if (!QuantityText)
    {
        return;
    }

    if (CurrentSlotData.bIsOccupied && CurrentSlotData.bIsAnchor && CurrentItemData.Quantity > 1)
    {
        QuantityText->SetVisibility(ESlateVisibility::HitTestInvisible);
        QuantityText->SetText(FText::AsNumber(CurrentItemData.Quantity));
    }
    else
    {
        QuantityText->SetVisibility(ESlateVisibility::Hidden);
    }
}

void USuspenseBaseSlotWidget::UpdateHighlightVisual()
{
    // If we have separate HighlightBorder
    if (HighlightBorder)
    {
        if (bIsHighlighted)
        {
            HighlightBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
            HighlightBorder->SetBrushColor(CurrentHighlightColor);

            UE_LOG(LogTemp, VeryVerbose, TEXT("[Slot %d] HighlightBorder visible with color: (%.2f, %.2f, %.2f, %.2f)"),
                CurrentSlotData.SlotIndex,
                CurrentHighlightColor.R, CurrentHighlightColor.G, CurrentHighlightColor.B, CurrentHighlightColor.A);
        }
        else
        {
            HighlightBorder->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    // If no separate HighlightBorder, use BackgroundBorder
    else if (BackgroundBorder)
    {
        if (bIsHighlighted)
        {
            BackgroundBorder->SetBrushColor(CurrentHighlightColor);

            UE_LOG(LogTemp, VeryVerbose, TEXT("[Slot %d] BackgroundBorder highlighted with color: (%.2f, %.2f, %.2f, %.2f)"),
                CurrentSlotData.SlotIndex,
                CurrentHighlightColor.R, CurrentHighlightColor.G, CurrentHighlightColor.B, CurrentHighlightColor.A);
        }
        else
        {
            BackgroundBorder->SetBrushColor(GetBackgroundColor());
        }
    }
}

void USuspenseBaseSlotWidget::UpdateSelectionVisual()
{
    if (!SelectionBorder)
    {
        return;
    }

    SelectionBorder->SetVisibility(bIsSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

FLinearColor USuspenseBaseSlotWidget::GetBackgroundColor() const
{
    // Priority: Locked > Selected > Hovered > Occupied/Empty
    if (bIsLocked)
    {
        return LockedColor;
    }

    if (bIsSelected)
    {
        return SelectedColor;
    }

    if (bIsHovered)
    {
        return HoveredColor;
    }

    return CurrentSlotData.bIsOccupied ? OccupiedSlotColor : EmptySlotColor;
}

void USuspenseBaseSlotWidget::HandleClick()
{
    if (IsValid(OwningContainer))
    {
        ISuspenseCoreContainerUI::Execute_OnSlotClicked(
            OwningContainer,
            CurrentSlotData.SlotIndex,
            CurrentItemData.ItemInstanceID
        );
    }
}

void USuspenseBaseSlotWidget::HandleDoubleClick()
{
    if (IsValid(OwningContainer))
    {
        ISuspenseCoreContainerUI::Execute_OnSlotDoubleClicked(
            OwningContainer,
            CurrentSlotData.SlotIndex,
            CurrentItemData.ItemInstanceID
        );
    }
}

void USuspenseBaseSlotWidget::HandleRightClick()
{
    if (IsValid(OwningContainer))
    {
        ISuspenseCoreContainerUI::Execute_OnSlotRightClicked(
            OwningContainer,
            CurrentSlotData.SlotIndex,
            CurrentItemData.ItemInstanceID
        );
    }
}

USuspenseDragDropHandler* USuspenseBaseSlotWidget::GetDragDropHandler() const
{
    if (CachedDragDropHandler)
    {
        return CachedDragDropHandler;
    }

    return USuspenseDragDropHandler::Get(this);
}

USuspenseCoreEventManager* USuspenseBaseSlotWidget::GetEventManager() const
{
    if (CachedEventManager)
    {
        return CachedEventManager;
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        return GameInstance->GetSubsystem<USuspenseCoreEventManager>();
    }

    return nullptr;
}

USuspenseCoreEventBus* USuspenseBaseSlotWidget::GetEventBus() const
{
    if (USuspenseCoreEventManager* EventManager = GetEventManager())
    {
        return EventManager->GetEventBus();
    }
    return nullptr;
}

bool USuspenseBaseSlotWidget::ValidateWidgetBindings() const
{
    bool bAllValid = true;

    if (!BackgroundBorder)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Slot] BackgroundBorder not bound"));
        bAllValid = false;
    }

    if (!ItemIcon)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Slot] ItemIcon not bound"));
        bAllValid = false;
    }

    if (!QuantityText)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Slot] QuantityText not bound"));
        bAllValid = false;
    }

    if (!RootSizeBox)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Slot] RootSizeBox not bound"));
        bAllValid = false;
    }

    // HighlightBorder is optional
    if (!HighlightBorder)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[Slot] HighlightBorder not bound (optional)"));
    }

    return bAllValid;
}

void USuspenseBaseSlotWidget::LoadIconAsync(const FString& IconPath)
{
    // Cancel previous loading
    if (IconStreamingHandle.IsValid())
    {
        IconStreamingHandle->CancelHandle();
        IconStreamingHandle.Reset();
    }

    FSoftObjectPath SoftPath(IconPath);
    if (!SoftPath.IsValid())
    {
        return;
    }

    // Check if already loaded
    TSoftObjectPtr<UTexture2D> SoftTexture(SoftPath);
    if (UTexture2D* LoadedTexture = SoftTexture.Get())
    {
        CachedIconTexture = LoadedTexture;
        if (ItemIcon)
        {
            ItemIcon->SetBrushFromTexture(LoadedTexture);
        }
        return;
    }

    // Load asynchronously
    FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
    IconStreamingHandle = StreamableManager.RequestAsyncLoad(
        SoftPath,
        FStreamableDelegate::CreateUObject(this, &USuspenseBaseSlotWidget::OnIconLoaded),
        FStreamableManager::AsyncLoadHighPriority
    );
}

void USuspenseBaseSlotWidget::OnIconLoaded()
{
    if (!IconStreamingHandle.IsValid())
    {
        return;
    }

    if (UTexture2D* LoadedTexture = Cast<UTexture2D>(IconStreamingHandle->GetLoadedAsset()))
    {
        CachedIconTexture = LoadedTexture;
        ScheduleVisualUpdate();
    }

    IconStreamingHandle.Reset();
}

void USuspenseBaseSlotWidget::UpdateCachedGeometry(const FGeometry& NewGeometry)
{
    CachedGeometry = NewGeometry;
    bGeometryCached = true;
    GeometryCacheTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

const FGeometry& USuspenseBaseSlotWidget::GetCachedOrCurrentGeometry() const
{
    if (bGeometryCached)
    {
        float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        if (CurrentTime - GeometryCacheTime < GEOMETRY_CACHE_LIFETIME)
        {
            return CachedGeometry;
        }
    }

    // Return current if cache invalid
    return GetCachedGeometry();
}
