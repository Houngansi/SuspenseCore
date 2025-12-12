// MedComBaseContainerWidget.cpp - Optimized version
#include "Widgets/Base/SuspenseBaseContainerWidget.h"
#include "Widgets/Base/SuspenseBaseSlotWidget.h"
#include "Components/TextBlock.h"
#include "Widgets/DragDrop/SuspenseDragDropOperation.h"
#include "DragDrop/SuspenseDragDropHandler.h"
#include "Components/PanelWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

// =====================================================
// FSlotWidgetPool Implementation
// =====================================================

USuspenseBaseSlotWidget* FSlotWidgetPool::AcquireSlot(UUserWidget* Outer, TSubclassOf<USuspenseBaseSlotWidget> SlotClass)
{
    // Try to reuse from pool
    if (AvailableSlots.Num() > 0)
    {
        USuspenseBaseSlotWidget* PooledSlot = AvailableSlots.Pop();
        if (IsValid(PooledSlot))
        {
            PooledSlot->SetPooled(false);
            return PooledSlot;
        }
    }

    // Create new slot - используем правильный тип Outer
    USuspenseBaseSlotWidget* NewSlot = CreateWidget<USuspenseBaseSlotWidget>(Outer, SlotClass);
    if (NewSlot)
    {
        AllSlots.Add(NewSlot);
    }
    return NewSlot;
}

void FSlotWidgetPool::ReleaseSlot(USuspenseBaseSlotWidget* Slot)
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
    for (USuspenseBaseSlotWidget* Slot : AllSlots)
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
// USuspenseBaseContainerWidget Implementation
// =====================================================

USuspenseBaseContainerWidget::USuspenseBaseContainerWidget(const FObjectInitializer& ObjectInitializer)
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

void USuspenseBaseContainerWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Ensure visibility
    SetVisibility(ESlateVisibility::Visible);

    // Initialize widget through interface
    if (GetClass()->ImplementsInterface(USuspenseCoreUIWidget::StaticClass()))
    {
        ISuspenseCoreUIWidget::Execute_InitializeWidget(this);
    }
}

void USuspenseBaseContainerWidget::NativeDestruct()
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
    ISuspenseCoreUIWidget::Execute_UninitializeWidget(this);

    Super::NativeDestruct();
}

void USuspenseBaseContainerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Process any immediate updates if needed
    if (PendingSlotUpdates.Num() > 0 && !UpdateBatchTimer.IsValid())
    {
        ProcessBatchedUpdates();
    }
}

void USuspenseBaseContainerWidget::InitializeWidget_Implementation()
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

void USuspenseBaseContainerWidget::UninitializeWidget_Implementation()
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

void USuspenseBaseContainerWidget::UpdateWidget_Implementation(float DeltaTime)
{
    // Base implementation - derived classes can override
}

FGameplayTag USuspenseBaseContainerWidget::GetWidgetTag_Implementation() const
{
   return ContainerType;
}

USuspenseCoreEventManager* USuspenseBaseContainerWidget::GetDelegateManager() const
{
   if (CachedDelegateManager)
   {
       return CachedDelegateManager;
   }

   return ISuspenseCoreUIWidget::GetDelegateManagerStatic(this);
}

void USuspenseBaseContainerWidget::InitializeContainer_Implementation(const FContainerUIData& ContainerData)
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

void USuspenseBaseContainerWidget::UpdateContainer_Implementation(const FContainerUIData& ContainerData)
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

void USuspenseBaseContainerWidget::RequestDataRefresh_Implementation()
{
   if (CachedDelegateManager)
   {
       ISuspenseCoreContainerUI::BroadcastContainerUpdateRequest(this, ContainerType);
   }
}

void USuspenseBaseContainerWidget::OnSlotClicked_Implementation(int32 SlotIndex, const FGuid& ItemInstanceID)
{
   // Update selection
   if (SelectedSlotIndex != SlotIndex)
   {
       // Deselect previous
       if (SelectedSlotIndex != INDEX_NONE)
       {
           if (USuspenseBaseSlotWidget* PrevSlot = GetSlotWidget(SelectedSlotIndex))
           {
               if (PrevSlot->GetClass()->ImplementsInterface(USuspenseCoreSlotUI::StaticClass()))
               {
                   ISuspenseCoreSlotUI::Execute_SetSelected(PrevSlot, false);
               }
           }
       }

       // Select new
       SelectedSlotIndex = SlotIndex;
       if (USuspenseBaseSlotWidget* NewSlot = GetSlotWidget(SlotIndex))
       {
           if (NewSlot->GetClass()->ImplementsInterface(USuspenseCoreSlotUI::StaticClass()))
           {
               ISuspenseCoreSlotUI::Execute_SetSelected(NewSlot, true);
           }
       }
   }

   // Notify through EventBus
   if (USuspenseCoreEventBus* EventBus = GetEventBus())
   {
       FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
       EventData.SetObject(TEXT("Container"), this);
       EventData.SetInt(TEXT("SlotIndex"), SlotIndex);
       EventData.SetString(TEXT("ContainerType"), ContainerType.ToString());
       EventData.SetString(TEXT("InteractionType"), TEXT("Click"));

       FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.UI.Slot.Interaction"));
       EventBus->Publish(EventTag, EventData);
   }
}

void USuspenseBaseContainerWidget::OnSlotDoubleClicked_Implementation(int32 SlotIndex, const FGuid& ItemInstanceID)
{
   FGameplayTag InteractionType = FGameplayTag::RequestGameplayTag(TEXT("UI.Interaction.DoubleClick"));
   ISuspenseCoreContainerUI::BroadcastSlotInteraction(this, SlotIndex, InteractionType);
}

void USuspenseBaseContainerWidget::OnSlotRightClicked_Implementation(int32 SlotIndex, const FGuid& ItemInstanceID)
{
   FGameplayTag InteractionType = FGameplayTag::RequestGameplayTag(TEXT("UI.Interaction.RightClick"));
   ISuspenseCoreContainerUI::BroadcastSlotInteraction(this, SlotIndex, InteractionType);
}

FSlotValidationResult USuspenseBaseContainerWidget::CanAcceptDrop_Implementation(
    const UDragDropOperation* DragOperation,
    int32 TargetSlotIndex) const
{
    // Basic validation
    if (!IsValid(DragOperation))
    {
        return FSlotValidationResult::Failure(FText::FromString(TEXT("Invalid drag operation")));
    }

    const USuspenseDragDropOperation* MedComDragOp = Cast<USuspenseDragDropOperation>(DragOperation);
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


void USuspenseBaseContainerWidget::HandleItemDropped_Implementation(
   UDragDropOperation* DragOperation,
   int32 TargetSlotIndex)
{
   // This is now just a notification - actual drop handling is in handler
   if (CachedDelegateManager)
   {
       FGameplayTag InteractionType = FGameplayTag::RequestGameplayTag(TEXT("UI.Interaction.Drop"));
       ISuspenseCoreContainerUI::BroadcastSlotInteraction(this, TargetSlotIndex, InteractionType);
   }
}

bool USuspenseBaseContainerWidget::ProcessDropOnSlot(
   USuspenseDragDropOperation* DragOperation,
   USuspenseBaseSlotWidget* SlotWidget,
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

bool USuspenseBaseContainerWidget::ProcessDragOverSlot(
   USuspenseDragDropOperation* DragOperation,
   USuspenseBaseSlotWidget* SlotWidget,
   const FVector2D& ScreenPosition,
   const FGeometry& SlotGeometry)
{
    if (!CachedDragDropHandler || !DragOperation || !SlotWidget)
    {
        return false;
    }

    // Get target slot
    int32 TargetSlot = INDEX_NONE;
    if (SlotWidget->GetClass()->ImplementsInterface(USuspenseCoreSlotUI::StaticClass()))
    {
        TargetSlot = ISuspenseCoreSlotUI::Execute_GetSlotIndex(SlotWidget);
    }

    if (TargetSlot == INDEX_NONE)
    {
        return false;
    }

    // Let the handler handle all visual updates
    // Don't do any additional validation here
    return true;
}

void USuspenseBaseContainerWidget::ProcessDragEnterSlot(
   const USuspenseDragDropOperation* DragOperation,
   USuspenseBaseSlotWidget* SlotWidget)
{
   // Simple notification - handler manages visual feedback
}

void USuspenseBaseContainerWidget::ClearSlotHighlights()
{
   // Delegate to handler
   if (CachedDragDropHandler)
   {
       CachedDragDropHandler->ClearAllVisualFeedback();
   }
}

USuspenseBaseSlotWidget* USuspenseBaseContainerWidget::GetSlotWidget(int32 SlotIndex) const
{
   if (const auto* SlotWidget = SlotWidgets.Find(SlotIndex))
   {
       return *SlotWidget;
   }
   return nullptr;
}

TArray<USuspenseBaseSlotWidget*> USuspenseBaseContainerWidget::GetAllSlotWidgets() const
{
   TArray<USuspenseBaseSlotWidget*> AllSlots;
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

USuspenseBaseSlotWidget* USuspenseBaseContainerWidget::GetSlotAtScreenPosition(const FVector2D& ScreenPosition) const
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

TArray<USuspenseBaseSlotWidget*> USuspenseBaseContainerWidget::GetSlotsInRegion(const FVector2D& Center, float Radius) const
{
   TArray<USuspenseBaseSlotWidget*> Result;
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

void USuspenseBaseContainerWidget::OnSlotSelectionChanged(int32 SlotIndex, bool bIsSelected)
{
   if (bIsSelected)
   {
       // Deselect other slots
       if (SelectedSlotIndex != INDEX_NONE && SelectedSlotIndex != SlotIndex)
       {
           if (USuspenseBaseSlotWidget* PrevSlot = GetSlotWidget(SelectedSlotIndex))
           {
               if (PrevSlot->GetClass()->ImplementsInterface(USuspenseCoreSlotUI::StaticClass()))
               {
                   ISuspenseCoreSlotUI::Execute_SetSelected(PrevSlot, false);
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

bool USuspenseBaseContainerWidget::CalculateOccupiedSlots(
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

FSmartDropZone USuspenseBaseContainerWidget::FindBestDropZone(
   const FVector2D& ScreenPosition,
   const FIntPoint& ItemSize,
   bool bIsRotated) const
{
   FSmartDropZone Result;

   // Simple implementation - just find slot at position
   if (USuspenseBaseSlotWidget* SlotWidget = GetSlotAtScreenPosition(ScreenPosition))
   {
       if (SlotWidget->GetClass()->ImplementsInterface(USuspenseCoreSlotUI::StaticClass()))
       {
           Result.SlotIndex = ISuspenseCoreSlotUI::Execute_GetSlotIndex(SlotWidget);
           Result.bIsValid = true;
           Result.FeedbackPosition = SlotWidget->GetCachedGeometry().GetAbsolutePosition() +
                                    SlotWidget->GetCachedGeometry().GetLocalSize() * 0.5f;
       }
   }

   return Result;
}

void USuspenseBaseContainerWidget::CreateSlots()
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
       USuspenseBaseSlotWidget* SlotWidget = CreateOrAcquireSlot();
       if (!SlotWidget)
       {
           continue;
       }

       // Set owning container
       SlotWidget->SetOwningContainer(this);

       // Initialize slot
       FItemUIData EmptyItem;
       ISuspenseCoreSlotUI::Execute_InitializeSlot(SlotWidget, SlotData, EmptyItem);

       // Add to panel
       Panel->AddChild(SlotWidget);

       // Store reference
       SlotWidgets.Add(SlotData.SlotIndex, SlotWidget);
   }
}

void USuspenseBaseContainerWidget::ClearSlots()
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

void USuspenseBaseContainerWidget::UpdateSlotWidget(int32 SlotIndex, const FSlotUIData& SlotData, const FItemUIData& ItemData)
{
   // Use batched update system
   ScheduleSlotUpdate(SlotIndex, SlotData, ItemData);
}

void USuspenseBaseContainerWidget::ScheduleSlotUpdate(int32 SlotIndex, const FSlotUIData& SlotData, const FItemUIData& ItemData)
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
               &USuspenseBaseContainerWidget::ProcessBatchedUpdates,
               UpdateBatchDelay,
               false
           );
       }
   }
}

void USuspenseBaseContainerWidget::ProcessBatchedUpdates()
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

       if (USuspenseBaseSlotWidget* SlotWidget = GetSlotWidget(SlotIndex))
       {
           // Ensure owning container is set
           if (!SlotWidget->GetOwningContainer())
           {
               SlotWidget->SetOwningContainer(this);
           }

           ISuspenseCoreSlotUI::Execute_UpdateSlot(SlotWidget, SlotData, ItemData);
       }
   }

   // Clear pending updates
   PendingSlotUpdates.Empty();

   // Update timestamp
   LastUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

USuspenseBaseSlotWidget* USuspenseBaseContainerWidget::CreateOrAcquireSlot()
{
    if (bEnableSlotPooling)
    {
        // Передаем this как UUserWidget*
        return SlotPool.AcquireSlot(this, SlotWidgetClass);
    }
    else
    {
        return CreateWidget<USuspenseBaseSlotWidget>(this, SlotWidgetClass);
    }
}

void USuspenseBaseContainerWidget::ReleaseSlot(USuspenseBaseSlotWidget* SlotWidget)
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

bool USuspenseBaseContainerWidget::ValidateSlotsPanel() const
{
   UPanelWidget* Panel = GetSlotsPanel();
   return Panel != nullptr;
}

const FSlotUIData* USuspenseBaseContainerWidget::FindSlotData(int32 SlotIndex) const
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

const FItemUIData* USuspenseBaseContainerWidget::FindItemDataForSlot(int32 SlotIndex) const
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

void USuspenseBaseContainerWidget::SubscribeToEvents()
{
   // Base implementation - derived classes override
}

void USuspenseBaseContainerWidget::UnsubscribeFromEvents()
{
   // Base implementation - derived classes override
}

USuspenseCoreEventBus* USuspenseBaseContainerWidget::GetEventBus() const
{
   if (CachedDelegateManager)
   {
       return CachedDelegateManager->GetEventBus();
   }

   if (USuspenseCoreEventManager* EventManager = GetDelegateManager())
   {
       return EventManager->GetEventBus();
   }

   return nullptr;
}

USuspenseDragDropHandler* USuspenseBaseContainerWidget::GetDragDropHandler() const
{
   if (CachedDragDropHandler)
   {
       return CachedDragDropHandler;
   }

   return USuspenseDragDropHandler::Get(this);
}

TSubclassOf<USuspenseDragVisualWidget> USuspenseBaseContainerWidget::GetDragVisualWidgetClass() const
{
   // Return configured class or nullptr
   return DragVisualWidgetClass;
}

USuspenseDragVisualWidget* USuspenseBaseContainerWidget::CreateDragVisualWidget(const FDragDropUIData& DragData)
{
    // Get the class to use
    TSubclassOf<USuspenseDragVisualWidget> VisualClass = GetDragVisualWidgetClass();

    if (!VisualClass)
    {
        return nullptr;
    }

    // Create the widget
    USuspenseDragVisualWidget* DragVisual = CreateWidget<USuspenseDragVisualWidget>(this, VisualClass);
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

float USuspenseBaseContainerWidget::GetDragVisualCellSize() const
{
   // Base implementation returns default
   // Derived classes can override to provide actual cell size
   return DefaultDragVisualCellSize;
}
