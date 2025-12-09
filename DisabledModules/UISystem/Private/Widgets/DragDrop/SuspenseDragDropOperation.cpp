// Copyright Suspense Team Team. All Rights Reserved.

#include "Widgets/DragDrop/SuspenseDragDropOperation.h"
#include "Widgets/Base/SuspenseBaseSlotWidget.h"
#include "DragDrop/SuspenseDragDropHandler.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreDraggable.h"
#include "SuspenseCore/Operations/SuspenseCoreInventoryResult.h"
#include "Engine/World.h"

USuspenseDragDropOperation::USuspenseDragDropOperation()
{
    bWasSuccessful = false;
}

bool USuspenseDragDropOperation::InitializeOperation(
    const FDragDropUIData& InDragData,
    USuspenseBaseSlotWidget* InSourceWidget,
    const FVector2D& InDragOffset,
    USuspenseDragDropHandler* InHandler)
{
    // Validate input parameters
    if (!InDragData.IsValidDragData())
    {
        UE_LOG(LogTemp, Error, TEXT("[DragDropOperation] Invalid drag data provided"));
        return false;
    }

    if (!IsValid(InSourceWidget))
    {
        UE_LOG(LogTemp, Error, TEXT("[DragDropOperation] Invalid source widget provided"));
        return false;
    }

    if (!InHandler)
    {
        UE_LOG(LogTemp, Error, TEXT("[DragDropOperation] Invalid handler provided"));
        return false;
    }

    // Store data
    DragData = InDragData;
    DragData.DragOffset = InDragOffset; // Store offset in drag data
    SourceWidget = InSourceWidget;
    Handler = InHandler;

    UE_LOG(LogTemp, Log, TEXT("[DragDropOperation] Initialized with item: %s, offset: (%.2f, %.2f)"),
        *DragData.ItemData.ItemID.ToString(),
        DragData.DragOffset.X,
        DragData.DragOffset.Y);

    return true;
}

bool USuspenseDragDropOperation::IsValidOperation() const
{
    return DragData.IsValidDragData() &&
           SourceWidget.IsValid() &&
           Handler.IsValid();
}

void USuspenseDragDropOperation::Drop_Implementation(const FPointerEvent& PointerEvent)
{
    FVector2D ScreenPos = PointerEvent.GetScreenSpacePosition();
    UE_LOG(LogTemp, Log, TEXT("[DragDropOperation] Drop at screen position: (%.1f, %.1f)"),
        ScreenPos.X, ScreenPos.Y);

    if (!IsValidOperation())
    {
        UE_LOG(LogTemp, Warning, TEXT("[DragDropOperation] Drop called on invalid operation"));
        bWasSuccessful = false;

        // Refresh source container even on invalid operation
        if (Handler.IsValid())
        {
            if (USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(Handler.Get()))
            {
                EventManager->NotifyInventoryUIRefreshRequested(DragData.SourceContainerType);
            }
        }
    }
    else if (Handler.IsValid())
    {
        // Delegate to handler
        FSuspenseCoreInventoryOperationResult Result = Handler->ProcessDrop(this, ScreenPos);
        bWasSuccessful = Result.IsSuccess();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[DragDropOperation] No handler available for drop"));
        bWasSuccessful = false;
    }

    // Notify source about completion
    if (SourceWidget.IsValid() &&
        SourceWidget->GetClass()->ImplementsInterface(USuspenseCoreDraggable::StaticClass()))
    {
        ISuspenseCoreDraggable::Execute_OnDragEnded(SourceWidget.Get(), bWasSuccessful);
    }

    Super::Drop_Implementation(PointerEvent);
}

void USuspenseDragDropOperation::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
    UE_LOG(LogTemp, Log, TEXT("[DragDropOperation] Drag operation cancelled"));

    bWasSuccessful = false;

    // Clear visual feedback through handler
    if (Handler.IsValid())
    {
        Handler->ClearAllVisualFeedback();

        // Ensure source container refreshes to show item in original position
        if (USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(Handler.Get()))
        {
            EventManager->NotifyInventoryUIRefreshRequested(DragData.SourceContainerType);

            UE_LOG(LogTemp, Log, TEXT("[DragDropOperation] Requested refresh for source container: %s"),
                *DragData.SourceContainerType.ToString());
        }
    }

    // Notify source
    if (SourceWidget.IsValid() &&
        SourceWidget->GetClass()->ImplementsInterface(USuspenseCoreDraggable::StaticClass()))
    {
        ISuspenseCoreDraggable::Execute_OnDragEnded(SourceWidget.Get(), false);
    }

    Super::DragCancelled_Implementation(PointerEvent);
}

void USuspenseDragDropOperation::Dragged_Implementation(const FPointerEvent& PointerEvent)
{
    Super::Dragged_Implementation(PointerEvent);

    // Получаем экранную позицию
    FVector2D ScreenPos = PointerEvent.GetScreenSpacePosition();

    // ВАЖНО: Добавляем детальное логирование
    UE_LOG(LogTemp, VeryVerbose, TEXT("[DragDropOperation] Dragged at screen pos: (%.1f, %.1f)"),
        ScreenPos.X, ScreenPos.Y);

    // Only delegate update to handler, don't do any visual updates here
    if (Handler.IsValid())
    {
        Handler->OnDraggedUpdate(this, ScreenPos);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[DragDropOperation] No handler available for drag update!"));
    }
}
