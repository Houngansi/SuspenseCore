// SuspenseInventory/Operations/SuspenseInventoryOperation.h
// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/Inventory/InventoryTypes.h"
#include "SuspenseInventoryOperation.generated.h"

class USuspenseInventoryComponent;

/**
 * Enumeration of inventory operation types.
 * Used for identifying and filtering operations in history.
 */
UENUM(BlueprintType)
enum class ESuspenseInventoryOperationType : uint8
{
    None        UMETA(DisplayName = "None"),
    Rotate      UMETA(DisplayName = "Rotate"),
    Move        UMETA(DisplayName = "Move"),
    Stack       UMETA(DisplayName = "Stack"),
    Split       UMETA(DisplayName = "Split"),
    Add         UMETA(DisplayName = "Add"),
    Remove      UMETA(DisplayName = "Remove"),
    Swap        UMETA(DisplayName = "Swap"),
    Equip       UMETA(DisplayName = "Equip"),
    Unequip     UMETA(DisplayName = "Unequip"),
    Use         UMETA(DisplayName = "Use"),
    Custom      UMETA(DisplayName = "Custom")
};

/**
 * Base structure for tracking inventory operations.
 * Used for implementing undo and redo operations, as well as logging.
 */
USTRUCT()
struct INVENTORYSYSTEM_API FSuspenseInventoryOperation
{
    GENERATED_BODY()

    /** Operation type */
    ESuspenseInventoryOperationType OperationType = ESuspenseInventoryOperationType::None;

    /** Flag indicating successful operation execution */
    bool bSuccess = false;

    /** Error code on failure */
    EInventoryErrorCode ErrorCode = EInventoryErrorCode::Success;

    /** Pointer to inventory component */
    USuspenseInventoryComponent* InventoryComponent = nullptr;

    /** Default constructor */
    FSuspenseInventoryOperation() = default;

    /**
     * Constructor with type and component
     * @param InOperationType Operation type
     * @param InInventoryComponent Inventory component
     */
    FSuspenseInventoryOperation(ESuspenseInventoryOperationType InOperationType, USuspenseInventoryComponent* InInventoryComponent)
        : OperationType(InOperationType)
        , bSuccess(false)
        , ErrorCode(EInventoryErrorCode::Success)
        , InventoryComponent(InInventoryComponent)
    {
    }

    /**
     * Checks if operation can be undone
     * @return true if operation can be undone
     */
    virtual bool CanUndo() const
    {
        return bSuccess && InventoryComponent != nullptr;
    }

    /**
     * Undoes the operation
     * @return true if operation was successfully undone
     */
    virtual bool Undo()
    {
        // Base implementation does nothing
        return false;
    }

    /**
     * Checks if operation can be redone
     * @return true if operation can be redone
     */
    virtual bool CanRedo() const
    {
        return InventoryComponent != nullptr;
    }

    /**
     * Redoes the operation
     * @return true if operation was successfully redone
     */
    virtual bool Redo()
    {
        // Base implementation does nothing
        return false;
    }

    /**
     * Returns information string about operation state
     * @return Operation description
     */
    virtual FString ToString() const
    {
        return FString::Printf(TEXT("Operation[Type=%d, Success=%s, Error=%d]"),
            (int32)OperationType,
            bSuccess ? TEXT("Yes") : TEXT("No"),
            (int32)ErrorCode);
    }
};

/**
 * Move operation for tracking item movement
 */
USTRUCT()
struct INVENTORYSYSTEM_API FSuspenseMoveOperation : public FSuspenseInventoryOperation
{
    GENERATED_BODY()

    /** Instance ID of item being moved */
    FGuid InstanceID;

    /** Source anchor index */
    int32 SourceIndex = INDEX_NONE;

    /** Target anchor index */
    int32 TargetIndex = INDEX_NONE;

    /** Original rotation state */
    bool bWasRotated = false;

    /** New rotation state */
    bool bIsRotated = false;

    FSuspenseMoveOperation()
    {
        OperationType = ESuspenseInventoryOperationType::Move;
    }

    virtual bool CanUndo() const override
    {
        return FSuspenseInventoryOperation::CanUndo() && InstanceID.IsValid() && SourceIndex != INDEX_NONE;
    }

    virtual FString ToString() const override
    {
        return FString::Printf(TEXT("MoveOperation[Instance=%s, From=%d, To=%d, Rotated=%s]"),
            *InstanceID.ToString().Left(8),
            SourceIndex,
            TargetIndex,
            bIsRotated ? TEXT("Yes") : TEXT("No"));
    }
};

/**
 * Rotate operation for tracking item rotation
 */
USTRUCT()
struct INVENTORYSYSTEM_API FSuspenseRotateOperation : public FSuspenseInventoryOperation
{
    GENERATED_BODY()

    /** Instance ID of item being rotated */
    FGuid InstanceID;

    /** Slot index where rotation occurred */
    int32 SlotIndex = INDEX_NONE;

    /** Previous rotation state */
    bool bWasRotated = false;

    FSuspenseRotateOperation()
    {
        OperationType = ESuspenseInventoryOperationType::Rotate;
    }

    virtual bool CanUndo() const override
    {
        return FSuspenseInventoryOperation::CanUndo() && InstanceID.IsValid() && SlotIndex != INDEX_NONE;
    }

    virtual FString ToString() const override
    {
        return FString::Printf(TEXT("RotateOperation[Instance=%s, Slot=%d, WasRotated=%s]"),
            *InstanceID.ToString().Left(8),
            SlotIndex,
            bWasRotated ? TEXT("Yes") : TEXT("No"));
    }
};

/**
 * Stack operation for tracking item stacking
 */
USTRUCT()
struct INVENTORYSYSTEM_API FSuspenseStackOperation : public FSuspenseInventoryOperation
{
    GENERATED_BODY()

    /** Source instance ID */
    FGuid SourceInstanceID;

    /** Target instance ID */
    FGuid TargetInstanceID;

    /** Amount transferred */
    int32 TransferredAmount = 0;

    /** Source quantity before operation */
    int32 PreviousSourceQuantity = 0;

    /** Target quantity before operation */
    int32 PreviousTargetQuantity = 0;

    FSuspenseStackOperation()
    {
        OperationType = ESuspenseInventoryOperationType::Stack;
    }

    virtual bool CanUndo() const override
    {
        return FSuspenseInventoryOperation::CanUndo() &&
               SourceInstanceID.IsValid() &&
               TargetInstanceID.IsValid() &&
               TransferredAmount > 0;
    }

    virtual FString ToString() const override
    {
        return FString::Printf(TEXT("StackOperation[Source=%s, Target=%s, Amount=%d]"),
            *SourceInstanceID.ToString().Left(8),
            *TargetInstanceID.ToString().Left(8),
            TransferredAmount);
    }
};

/**
 * Split operation for tracking stack splitting
 */
USTRUCT()
struct INVENTORYSYSTEM_API FSuspenseSplitOperation : public FSuspenseInventoryOperation
{
    GENERATED_BODY()

    /** Source instance ID (original stack) */
    FGuid SourceInstanceID;

    /** New instance ID (created stack) */
    FGuid NewInstanceID;

    /** Amount split off */
    int32 SplitAmount = 0;

    /** Slot where new stack was placed */
    int32 NewSlotIndex = INDEX_NONE;

    FSuspenseSplitOperation()
    {
        OperationType = ESuspenseInventoryOperationType::Split;
    }

    virtual bool CanUndo() const override
    {
        return FSuspenseInventoryOperation::CanUndo() &&
               SourceInstanceID.IsValid() &&
               NewInstanceID.IsValid() &&
               SplitAmount > 0;
    }

    virtual FString ToString() const override
    {
        return FString::Printf(TEXT("SplitOperation[Source=%s, New=%s, Amount=%d, Slot=%d]"),
            *SourceInstanceID.ToString().Left(8),
            *NewInstanceID.ToString().Left(8),
            SplitAmount,
            NewSlotIndex);
    }
};
