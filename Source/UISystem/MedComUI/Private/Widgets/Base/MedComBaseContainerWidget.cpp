// MedComBaseContainerWidget.cpp - Optimized version
#include "Widgets/Base/MedComBaseContainerWidget.h"
#include "Widgets/Base/MedComBaseSlotWidget.h"
#include "Components/TextBlock.h"
#include "Widgets/DragDrop/MedComDragDropOperation.h"
#include "DragDrop/MedComDragDropHandler.h"
#include "Components/PanelWidget.h"
#include "Delegates/EventDelegateManager.h"
#include "Engine/World.h"
#include "TimerManager.h"

// =====================================================
// FSlotWidgetPool Implementation
// =====================================================

UMedComBaseSlotWidget* FSlotWidgetPool::AcquireSlot(UUserWidget* Outer, TSubclassOf<UMedComBaseSlotWidget> SlotClass)
{
    // Try to reuse from pool
    if (AvailableSlots.Num() > 0)
    {
        UMedComBaseSlotWidget* PooledSlot = AvailableSlots.Pop();
        if (IsValid(PooledSlot))
        {
            PooledSlot->SetPooled(false);
            return PooledSlot;
        }
    }
    
    // Create new slot - используем правильный тип Outer
    UMedComBaseSlotWidget* NewSlot = CreateWidget<UMedComBaseSlotWidget>(Outer, SlotClass);
    if (NewSlot)
    {
        AllSlots.Add(NewSlot);
    }
    return NewSlot;
}

void FSlotWidgetPool::ReleaseSlot(UMedComBaseSlotWidget* Slot)
{
    if (!IsValid(Slot) || !Slot->CanBePooled())
    {
        return;
    }
    
    // Reset slot for reuse
    Slot->ResetForPool();
    Slot->SetPooled(true);
    
    // Add to available pool
    AvailableSlots.AddUnique(Slot);
}

void FSlotWidgetPool::Clear()
{
    // Clear all slots
    for (UMedComBaseSlotWidget* Slot : AllSlots)
    {
        if (IsValid(Slot))
        {
            Slot->RemoveFromParent();
        }
    }
    
    AvailableSlots.Empty();
    AllSlots.Empty();
}

// =====================================================
// UMedComBaseContainerWidget Implementation
// =====================================================

UMedComBaseContainerWidget::UMedComBaseContainerWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bIsInitialized = false;
    SelectedSlotIndex = INDEX_NONE;
    CachedDragDropHandler = nullptr;
    CachedDelegateManager = nullptr;
    
    // Performance settings
    bEnableSlotPooling = true;
    MaxPooledSlots = 200;
    UpdateBatchDelay = 0.033f; // ~30 FPS
    LastUpdateTime = 0.0f;
    
    // Disable tick by default
    bHasScriptImplementedTick = false;
}

void UMedComBaseContainerWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Ensure visibility
    SetVisibility(ESlateVisibility::Visible);
    
    // Initialize widget through interface
    if (GetClass()->ImplementsInterface(UMedComUIWidgetInterface::StaticClass()))
    {
        IMedComUIWidgetInterface::Execute_InitializeWidget(this);
    }
}

void UMedComBaseContainerWidget::NativeDestruct()
{
    // Cancel pending updates
    if (UWorld* World = GetWorld())
    {
        if (UpdateBatchTimer.IsValid())
        {
            World->GetTimerManager().ClearTimer(UpdateBatchTimer);
        }
    }
    
    // Clear slot pool
    SlotPool.Clear();
    
    // Uninitialize through interface
    IMedComUIWidgetInterface::Execute_UninitializeWidget(this);
    
    Super::NativeDestruct();
}

void UMedComBaseContainerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    // Process any immediate updates if needed
    if (PendingSlotUpdates.Num() > 0 && !UpdateBatchTimer.IsValid())
    {
        ProcessBatchedUpdates();
    }
}

void UMedComBaseContainerWidget::InitializeWidget_Implementation()
{
    if (bIsInitialized)
    {
        return;
    }
    
    if (!ValidateSlotsPanel())
    {
        return;
    }
    
    if (!SlotWidgetClass)
    {
        return;
    }
    
    // Cache managers
    CachedDelegateManager = GetDelegateManager();
    CachedDragDropHandler = GetDragDropHandler();
    
    // Subscribe to events
    SubscribeToEvents();
    
    bIsInitialized = true;
}

void UMedComBaseContainerWidget::UninitializeWidget_Implementation()
{
    // Cancel pending updates
    if (UWorld* World = GetWorld())
    {
        if (UpdateBatchTimer.IsValid())
        {
            World->GetTimerManager().ClearTimer(UpdateBatchTimer);
        }
    }
    
    // Clear all slots
    ClearSlots();
    
    // Unsubscribe from events
    UnsubscribeFromEvents();
    
    bIsInitialized = false;
    CachedDelegateManager = nullptr;
    CachedDragDropHandler = nullptr;
}

void UMedComBaseContainerWidget::UpdateWidget_Implementation(float DeltaTime)
{
    // Base implementation - derived classes can override
}

FGameplayTag UMedComBaseContainerWidget::GetWidgetTag_Implementation() const
{
   return ContainerType;
}

UEventDelegateManager* UMedComBaseContainerWidget::GetDelegateManager() const
{
   if (CachedDelegateManager)
   {
       return CachedDelegateManager;
   }
   
   return IMedComUIWidgetInterface::GetDelegateManagerStatic(this);
}

void UMedComBaseContainerWidget::InitializeContainer_Implementation(const FContainerUIData& ContainerData)
{
   if (!bIsInitialized)
   {
       return;
   }
   
   CurrentContainerData = ContainerData;
   ContainerType = ContainerData.ContainerType;
   
   // Create slots
   CreateSlots();
   
   // Update all slots with data
   for (const FSlotUIData& SlotData : ContainerData.Slots)
   {
       FItemUIData ItemData;
       for (const FItemUIData& Item : ContainerData.Items)
       {
           if (Item.AnchorSlotIndex == SlotData.SlotIndex)
           {
               ItemData = Item;
               break;
           }
       }
       
       // Use batched update for performance
       ScheduleSlotUpdate(SlotData.SlotIndex, SlotData, ItemData);
   }
   
   // Process initial batch immediately
   ProcessBatchedUpdates();
}

void UMedComBaseContainerWidget::UpdateContainer_Implementation(const FContainerUIData& ContainerData)
{
   if (!bIsInitialized)
   {
       return;
   }
   
   CurrentContainerData = ContainerData;
   
   // Batch updates for performance
   for (const FSlotUIData& SlotData : ContainerData.Slots)
   {
       FItemUIData ItemData;
       for (const FItemUIData& Item : ContainerData.Items)
       {
           if (Item.AnchorSlotIndex == SlotData.SlotIndex)
           {
               ItemData = Item;
               break;
           }
       }
       
       ScheduleSlotUpdate(SlotData.SlotIndex, SlotData, ItemData);
   }
}

void UMedComBaseContainerWidget::RequestDataRefresh_Implementation()
{
   if (CachedDelegateManager)
   {
       IMedComContainerUIInterface::BroadcastContainerUpdateRequest(this, ContainerType);
   }
}

void UMedComBaseContainerWidget::OnSlotClicked_Implementation(int32 SlotIndex, const FGuid& ItemInstanceID)
{
   // Update selection
   if (SelectedSlotIndex != SlotIndex)
   {
       // Deselect previous
       if (SelectedSlotIndex != INDEX_NONE)
       {
           if (UMedComBaseSlotWidget* PrevSlot = GetSlotWidget(SelectedSlotIndex))
           {
               if (PrevSlot->GetClass()->ImplementsInterface(UMedComSlotUIInterface::StaticClass()))
               {
                   IMedComSlotUIInterface::Execute_SetSelected(PrevSlot, false);
               }
           }
       }
       
       // Select new
       SelectedSlotIndex = SlotIndex;
       if (UMedComBaseSlotWidget* NewSlot = GetSlotWidget(SlotIndex))
       {
           if (NewSlot->GetClass()->ImplementsInterface(UMedComSlotUIInterface::StaticClass()))
           {
               IMedComSlotUIInterface::Execute_SetSelected(NewSlot, true);
           }
       }
   }
   
   // Notify through event system
   if (UEventDelegateManager* Manager = GetDelegateManager())
   {
       FGameplayTag InteractionType = FGameplayTag::RequestGameplayTag(TEXT("UI.Interaction.Click"));
       Manager->OnUISlotInteraction.Broadcast(this, SlotIndex, InteractionType);
   }
}

void UMedComBaseContainerWidget::OnSlotDoubleClicked_Implementation(int32 SlotIndex, const FGuid& ItemInstanceID)
{
   FGameplayTag InteractionType = FGameplayTag::RequestGameplayTag(TEXT("UI.Interaction.DoubleClick"));
   IMedComContainerUIInterface::BroadcastSlotInteraction(this, SlotIndex, InteractionType);
}

void UMedComBaseContainerWidget::OnSlotRightClicked_Implementation(int32 SlotIndex, const FGuid& ItemInstanceID)
{
   FGameplayTag InteractionType = FGameplayTag::RequestGameplayTag(TEXT("UI.Interaction.RightClick"));
   IMedComContainerUIInterface::BroadcastSlotInteraction(this, SlotIndex, InteractionType);
}

FSlotValidationResult UMedComBaseContainerWidget::CanAcceptDrop_Implementation(
    const UDragDropOperation* DragOperation,
    int32 TargetSlotIndex) const
{
    // Basic validation
    if (!IsValid(DragOperation))
    {
        return FSlotValidationResult::Failure(FText::FromString(TEXT("Invalid drag operation")));
    }

    const UMedComDragDropOperation* MedComDragOp = Cast<UMedComDragDropOperation>(DragOperation);
    if (!MedComDragOp || !MedComDragOp->IsValidOperation())
    {
        return FSlotValidationResult::Failure(FText::FromString(TEXT("Invalid drag operation type")));
    }

    // Check slot exists
    if (!SlotWidgets.Contains(TargetSlotIndex))
    {
        return FSlotValidationResult::Failure(FText::FromString(TEXT("Invalid slot index")));
    }

    // Дальнейшая проверка — на стороне игровой логики/сервисов
    return FSlotValidationResult::Success();
}


void UMedComBaseContainerWidget::HandleItemDropped_Implementation(
   UDragDropOperation* DragOperation, 
   int32 TargetSlotIndex)
{
   // This is now just a notification - actual drop handling is in handler
   if (CachedDelegateManager)
   {
       FGameplayTag InteractionType = FGameplayTag::RequestGameplayTag(TEXT("UI.Interaction.Drop"));
       IMedComContainerUIInterface::BroadcastSlotInteraction(this, TargetSlotIndex, InteractionType);
   }
}

bool UMedComBaseContainerWidget::ProcessDropOnSlot(
   UMedComDragDropOperation* DragOperation,
   UMedComBaseSlotWidget* SlotWidget,
   const FVector2D& ScreenPosition,
   const FGeometry& SlotGeometry)
{
   // Delegate to handler
   if (CachedDragDropHandler)
   {
       return CachedDragDropHandler->ProcessContainerDrop(
           this,
           DragOperation,
           SlotWidget,
           ScreenPosition
       );
   }
   
   return false;
}

bool UMedComBaseContainerWidget::ProcessDragOverSlot(
   UMedComDragDropOperation* DragOperation,
   UMedComBaseSlotWidget* SlotWidget,
   const FVector2D& ScreenPosition,
   const FGeometry& SlotGeometry)
{
    if (!CachedDragDropHandler || !DragOperation || !SlotWidget)
    {
        return false;
    }
   
    // Get target slot
    int32 TargetSlot = INDEX_NONE;
    if (SlotWidget->GetClass()->ImplementsInterface(UMedComSlotUIInterface::StaticClass()))
    {
        TargetSlot = IMedComSlotUIInterface::Execute_GetSlotIndex(SlotWidget);
    }
   
    if (TargetSlot == INDEX_NONE)
    {
        return false;
    }
   
    // Let the handler handle all visual updates
    // Don't do any additional validation here
    return true;
}

void UMedComBaseContainerWidget::ProcessDragEnterSlot(
   const UMedComDragDropOperation* DragOperation,
   UMedComBaseSlotWidget* SlotWidget)
{
   // Simple notification - handler manages visual feedback
}

void UMedComBaseContainerWidget::ClearSlotHighlights()
{
   // Delegate to handler
   if (CachedDragDropHandler)
   {
       CachedDragDropHandler->ClearAllVisualFeedback();
   }
}

UMedComBaseSlotWidget* UMedComBaseContainerWidget::GetSlotWidget(int32 SlotIndex) const
{
   if (const auto* SlotWidget = SlotWidgets.Find(SlotIndex))
   {
       return *SlotWidget;
   }
   return nullptr;
}

TArray<UMedComBaseSlotWidget*> UMedComBaseContainerWidget::GetAllSlotWidgets() const
{
   TArray<UMedComBaseSlotWidget*> AllSlots;
   AllSlots.Reserve(SlotWidgets.Num());
   
   for (const auto& Pair : SlotWidgets)
   {
       if (IsValid(Pair.Value))
       {
           AllSlots.Add(Pair.Value);
       }
   }
   
   return AllSlots;
}

UMedComBaseSlotWidget* UMedComBaseContainerWidget::GetSlotAtScreenPosition(const FVector2D& ScreenPosition) const
{
   for (const auto& Pair : SlotWidgets)
   {
       if (Pair.Value && Pair.Value->GetCachedGeometry().IsUnderLocation(ScreenPosition))
       {
           return Pair.Value;
       }
   }
   return nullptr;
}

TArray<UMedComBaseSlotWidget*> UMedComBaseContainerWidget::GetSlotsInRegion(const FVector2D& Center, float Radius) const
{
   TArray<UMedComBaseSlotWidget*> Result;
   float RadiusSq = Radius * Radius;
   
   for (const auto& Pair : SlotWidgets)
   {
       if (!IsValid(Pair.Value))
       {
           continue;
       }
       
       FVector2D SlotCenter = Pair.Value->GetCachedGeometry().GetAbsolutePosition() + 
                             Pair.Value->GetCachedGeometry().GetLocalSize() * 0.5f;
       
       if (FVector2D::DistSquared(Center, SlotCenter) <= RadiusSq)
       {
           Result.Add(Pair.Value);
       }
   }
   
   return Result;
}

void UMedComBaseContainerWidget::OnSlotSelectionChanged(int32 SlotIndex, bool bIsSelected)
{
   if (bIsSelected)
   {
       // Deselect other slots
       if (SelectedSlotIndex != INDEX_NONE && SelectedSlotIndex != SlotIndex)
       {
           if (UMedComBaseSlotWidget* PrevSlot = GetSlotWidget(SelectedSlotIndex))
           {
               if (PrevSlot->GetClass()->ImplementsInterface(UMedComSlotUIInterface::StaticClass()))
               {
                   IMedComSlotUIInterface::Execute_SetSelected(PrevSlot, false);
               }
           }
       }
       SelectedSlotIndex = SlotIndex;
   }
   else if (SelectedSlotIndex == SlotIndex)
   {
       SelectedSlotIndex = INDEX_NONE;
   }
}

bool UMedComBaseContainerWidget::CalculateOccupiedSlots(
    int32 TargetSlot, 
    FIntPoint ItemSize, 
    bool bIsRotated, 
    TArray<int32>& OutOccupiedSlots) const
{
    OutOccupiedSlots.Empty();
    
    // For grid-based containers, this should be overridden
    // Base implementation assumes single slot
    if (TargetSlot >= 0 && SlotWidgets.Contains(TargetSlot))
    {
        OutOccupiedSlots.Add(TargetSlot);
        return true;
    }
    
    return false;
}

FSmartDropZone UMedComBaseContainerWidget::FindBestDropZone(
   const FVector2D& ScreenPosition,
   const FIntPoint& ItemSize,
   bool bIsRotated) const
{
   FSmartDropZone Result;
   
   // Simple implementation - just find slot at position
   if (UMedComBaseSlotWidget* SlotWidget = GetSlotAtScreenPosition(ScreenPosition))
   {
       if (SlotWidget->GetClass()->ImplementsInterface(UMedComSlotUIInterface::StaticClass()))
       {
           Result.SlotIndex = IMedComSlotUIInterface::Execute_GetSlotIndex(SlotWidget);
           Result.bIsValid = true;
           Result.FeedbackPosition = SlotWidget->GetCachedGeometry().GetAbsolutePosition() + 
                                    SlotWidget->GetCachedGeometry().GetLocalSize() * 0.5f;
       }
   }
   
   return Result;
}

void UMedComBaseContainerWidget::CreateSlots()
{
   UPanelWidget* Panel = GetSlotsPanel();
   if (!Panel || !SlotWidgetClass)
   {
       return;
   }
   
   // Clear existing
   ClearSlots();
   
   // Create new slots
   SlotWidgets.Reserve(CurrentContainerData.Slots.Num());
   
   for (const FSlotUIData& SlotData : CurrentContainerData.Slots)
   {
       UMedComBaseSlotWidget* SlotWidget = CreateOrAcquireSlot();
       if (!SlotWidget)
       {
           continue;
       }
       
       // Set owning container
       SlotWidget->SetOwningContainer(this);
       
       // Initialize slot
       FItemUIData EmptyItem;
       IMedComSlotUIInterface::Execute_InitializeSlot(SlotWidget, SlotData, EmptyItem);
       
       // Add to panel
       Panel->AddChild(SlotWidget);
       
       // Store reference
       SlotWidgets.Add(SlotData.SlotIndex, SlotWidget);
   }
}

void UMedComBaseContainerWidget::ClearSlots()
{
    if (UPanelWidget* Panel = GetSlotsPanel())
    {
        Panel->ClearChildren();
    }
    
    // Return slots to pool if enabled
    if (bEnableSlotPooling)
    {
        for (const auto& Pair : SlotWidgets)
        {
            if (IsValid(Pair.Value))
            {
                ReleaseSlot(Pair.Value); // Используем ReleaseSlot вместо прямого вызова
            }
        }
    }
    
    SlotWidgets.Empty();
    SelectedSlotIndex = INDEX_NONE;
}

void UMedComBaseContainerWidget::UpdateSlotWidget(int32 SlotIndex, const FSlotUIData& SlotData, const FItemUIData& ItemData)
{
   // Use batched update system
   ScheduleSlotUpdate(SlotIndex, SlotData, ItemData);
}

void UMedComBaseContainerWidget::ScheduleSlotUpdate(int32 SlotIndex, const FSlotUIData& SlotData, const FItemUIData& ItemData)
{
   // Add to pending updates
   PendingSlotUpdates.Add(SlotIndex, TPair<FSlotUIData, FItemUIData>(SlotData, ItemData));
   
   // Schedule batch processing
   if (!UpdateBatchTimer.IsValid() && UpdateBatchDelay > 0.0f)
   {
       if (UWorld* World = GetWorld())
       {
           World->GetTimerManager().SetTimer(
               UpdateBatchTimer,
               this,
               &UMedComBaseContainerWidget::ProcessBatchedUpdates,
               UpdateBatchDelay,
               false
           );
       }
   }
}

void UMedComBaseContainerWidget::ProcessBatchedUpdates()
{
   // Clear timer
   if (UWorld* World = GetWorld())
   {
       if (UpdateBatchTimer.IsValid())
       {
           World->GetTimerManager().ClearTimer(UpdateBatchTimer);
       }
   }
   
   // Process all pending updates
   for (const auto& Update : PendingSlotUpdates)
   {
       int32 SlotIndex = Update.Key;
       const FSlotUIData& SlotData = Update.Value.Key;
       const FItemUIData& ItemData = Update.Value.Value;
       
       if (UMedComBaseSlotWidget* SlotWidget = GetSlotWidget(SlotIndex))
       {
           // Ensure owning container is set
           if (!SlotWidget->GetOwningContainer())
           {
               SlotWidget->SetOwningContainer(this);
           }
           
           IMedComSlotUIInterface::Execute_UpdateSlot(SlotWidget, SlotData, ItemData);
       }
   }
   
   // Clear pending updates
   PendingSlotUpdates.Empty();
   
   // Update timestamp
   LastUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

UMedComBaseSlotWidget* UMedComBaseContainerWidget::CreateOrAcquireSlot()
{
    if (bEnableSlotPooling)
    {
        // Передаем this как UUserWidget*
        return SlotPool.AcquireSlot(this, SlotWidgetClass);
    }
    else
    {
        return CreateWidget<UMedComBaseSlotWidget>(this, SlotWidgetClass);
    }
}

void UMedComBaseContainerWidget::ReleaseSlot(UMedComBaseSlotWidget* SlotWidget)
{
    if (!IsValid(SlotWidget))
    {
        return;
    }
    
    if (bEnableSlotPooling && SlotPool.AvailableSlots.Num() < MaxPooledSlots)
    {
        SlotPool.ReleaseSlot(SlotWidget);
    }
    else
    {
        SlotWidget->RemoveFromParent();
    }
}

bool UMedComBaseContainerWidget::ValidateSlotsPanel() const
{
   UPanelWidget* Panel = GetSlotsPanel();
   return Panel != nullptr;
}

const FSlotUIData* UMedComBaseContainerWidget::FindSlotData(int32 SlotIndex) const
{
   for (const FSlotUIData& SlotData : CurrentContainerData.Slots)
   {
       if (SlotData.SlotIndex == SlotIndex)
       {
           return &SlotData;
       }
   }
   return nullptr;
}

const FItemUIData* UMedComBaseContainerWidget::FindItemDataForSlot(int32 SlotIndex) const
{
   for (const FItemUIData& Item : CurrentContainerData.Items)
   {
       if (Item.AnchorSlotIndex == SlotIndex)
       {
           return &Item;
       }
   }
   return nullptr;
}

void UMedComBaseContainerWidget::SubscribeToEvents()
{
   // Base implementation - derived classes override
}

void UMedComBaseContainerWidget::UnsubscribeFromEvents()
{
   // Base implementation - derived classes override
}

UMedComDragDropHandler* UMedComBaseContainerWidget::GetDragDropHandler() const
{
   if (CachedDragDropHandler)
   {
       return CachedDragDropHandler;
   }
   
   return UMedComDragDropHandler::Get(this);
}

TSubclassOf<UMedComDragVisualWidget> UMedComBaseContainerWidget::GetDragVisualWidgetClass() const
{
   // Return configured class or nullptr
   return DragVisualWidgetClass;
}

UMedComDragVisualWidget* UMedComBaseContainerWidget::CreateDragVisualWidget(const FDragDropUIData& DragData)
{
    // Get the class to use
    TSubclassOf<UMedComDragVisualWidget> VisualClass = GetDragVisualWidgetClass();
    
    if (!VisualClass)
    {
        return nullptr;
    }
    
    // Create the widget
    UMedComDragVisualWidget* DragVisual = CreateWidget<UMedComDragVisualWidget>(this, VisualClass);
    if (!DragVisual)
    {
        return nullptr;
    }
    
    // Initialize with data
    float CellSizeWidget = GetDragVisualCellSize();
    if (!DragVisual->InitializeDragVisual(DragData, CellSizeWidget))
    {
        DragVisual->RemoveFromParent();
        return nullptr;
    }
    
    // Apply container-specific settings
    // Вместо прямого доступа к QuantityText используем метод
    if (!bShowQuantityOnDrag)
    {
        DragVisual->SetQuantityTextVisible(false);
    }
    
    // Enable low performance mode if needed
    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    if (CurrentTime - LastUpdateTime < 0.016f) // If running below 60 FPS
        {
        DragVisual->SetLowPerformanceMode(true);
        }
    
    return DragVisual;
}

float UMedComBaseContainerWidget::GetDragVisualCellSize() const
{
   // Base implementation returns default
   // Derived classes can override to provide actual cell size
   return DefaultDragVisualCellSize;
}