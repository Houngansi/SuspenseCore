// Copyright Suspense Team Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Types/UI/SuspenseContainerUITypes.h"
#include "SuspenseDragDropOperation.generated.h"

// Forward declarations
class USuspenseBaseSlotWidget;
class USuspenseDragVisualWidget;
class USuspenseDragDropHandler;

/**
 * Simplified drag drop operation - pure data holder
 * All logic delegated to USuspenseDragDropHandler
 */
UCLASS()
class UISYSTEM_API USuspenseDragDropOperation : public UDragDropOperation
{
    GENERATED_BODY()

public:
    USuspenseDragDropOperation();

    //================================================
    // Initialization
    //================================================

    /**
     * Initialize drag operation with data
     * @param InDragData Data to be dragged
     * @param InSourceWidget Source widget that started the drag
     * @param InDragOffset Normalized offset (0-1) where the drag started within the item
     * @param InHandler Handler that manages this operation
     * @return true if initialization successful
     */
    UFUNCTION(BlueprintCallable, Category = "DragDrop")
    bool InitializeOperation(const FDragDropUIData& InDragData,
                           USuspenseBaseSlotWidget* InSourceWidget,
                           const FVector2D& InDragOffset,
                           USuspenseDragDropHandler* InHandler);

    //================================================
    // Data Access
    //================================================

    /**
     * Get the drag data directly
     * @return Drag data stored in this operation
     */
    UFUNCTION(BlueprintPure, Category = "DragDrop")
    const FDragDropUIData& GetDragData() const { return DragData; }

    /**
     * Check if operation is valid
     * @return true if operation has valid data
     */
    UFUNCTION(BlueprintPure, Category = "DragDrop")
    bool IsValidOperation() const;

    /**
     * Get the size of dragged item
     * @return Item size in grid cells
     */
    UFUNCTION(BlueprintPure, Category = "DragDrop")
    FIntPoint GetDraggedItemSize() const { return DragData.GetEffectiveSize(); }

    /**
     * Get normalized drag offset
     * @return Offset where drag started (0-1 range)
     */
    UFUNCTION(BlueprintPure, Category = "DragDrop")
    FVector2D GetDragOffset() const { return DragData.DragOffset; }

    /**
     * Get source widget
     * @return Source widget or nullptr
     */
    UFUNCTION(BlueprintPure, Category = "DragDrop")
    USuspenseBaseSlotWidget* GetSourceWidget() const { return SourceWidget.Get(); }

    //================================================
    // Operation State
    //================================================

    /** Flag indicating if drop was successful */
    UPROPERTY(BlueprintReadOnly, Category = "DragDrop")
    bool bWasSuccessful;

protected:
    //================================================
    // UDragDropOperation Interface
    //================================================

    virtual void Drop_Implementation(const FPointerEvent& PointerEvent) override;
    virtual void DragCancelled_Implementation(const FPointerEvent& PointerEvent) override;
    virtual void Dragged_Implementation(const FPointerEvent& PointerEvent) override;

private:
    friend class USuspenseDragDropHandler; // Only handler can create/manage

    /** Complete drag data stored directly */
    UPROPERTY()
    FDragDropUIData DragData;

    /** Source widget that initiated the drag */
    UPROPERTY()
    TWeakObjectPtr<USuspenseBaseSlotWidget> SourceWidget;

    /** Handler managing this operation */
    UPROPERTY()
    TWeakObjectPtr<USuspenseDragDropHandler> Handler;
};
