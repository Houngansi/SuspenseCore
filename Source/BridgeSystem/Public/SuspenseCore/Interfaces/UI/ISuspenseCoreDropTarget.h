// ISuspenseCoreDropTarget.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Types/Equipment/SuspenseCoreEquipmentTypes.h"
#include "UObject/Interface.h"
#include "SuspenseCore/Types/UI/SuspenseCoreContainerUITypes.h"
#include "ISuspenseCoreDropTarget.generated.h"

// Forward declarations
class UDragDropOperation; // Используем базовый класс

UINTERFACE(MinimalAPI, BlueprintType)
class USuspenseCoreDropTarget : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for UI elements that can act as drop targets for drag-and-drop operations.
 * ОБНОВЛЕНО: Используем базовый UDragDropOperation для избежания циклических зависимостей
 */
class BRIDGESYSTEM_API ISuspenseCoreDropTarget
{
    GENERATED_BODY()

public:
    /**
     * Determines if the target can accept the dragged item.
     * @param DragOperation Current drag operation (базовый класс)
     * @return Validation result indicating whether the drop is allowed.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|DragDrop")
    FSlotValidationResult CanAcceptDrop(const UDragDropOperation* DragOperation) const;

    /**
     * Handles the drop of an item onto this target.
     * @param DragOperation Drag operation with item data (базовый класс)
     * @return True if the drop was successfully handled, false otherwise.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|DragDrop")
    bool HandleDrop(UDragDropOperation* DragOperation);

    /**
     * Called when a drag operation enters this target's bounds.
     * ВАЖНО: Убираем const для корректной работы с Blueprint
     * @param DragOperation Current drag operation (базовый класс)
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|DragDrop")
    void OnDragEnter(UDragDropOperation* DragOperation);

    /**
     * Called when a drag operation leaves this target's bounds.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|DragDrop")
    void OnDragLeave();

    /**
     * Retrieves the slot index or identifier for this drop target.
     * @return The target slot index, or a special value if applicable (e.g., -1 for invalid).
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|DragDrop")
    int32 GetDropTargetSlot() const;
};

