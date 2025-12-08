// Copyright Suspense Team. All Rights Reserved.

#ifndef SUSPENSECORE_TYPES_UI_SUSPENSECORECONTAINERUITYPES_H
#define SUSPENSECORE_TYPES_UI_SUSPENSECORECONTAINERUITYPES_H

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/Texture2D.h"
#include "SuspenseContainerUITypes.generated.h"

// Forward declaration для избежания циклических зависимостей
class UUserWidget;

/**
 * UI data for a single slot in a container
 * This is a display-only structure, no game logic
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSlotUIData
{
    GENERATED_BODY()

    /** Unique index of this slot in the container */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Slot")
    int32 SlotIndex;

    /** Grid position X (for grid-based containers) */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Slot")
    int32 GridX;

    /** Grid position Y (for grid-based containers) */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Slot")
    int32 GridY;

    /** Whether this slot is currently occupied */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Slot")
    bool bIsOccupied;

    /** Whether this slot is the anchor point for an item */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Slot")
    bool bIsAnchor;

    /** Whether this slot is part of a larger item */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Slot")
    bool bIsPartOfItem;

    /** Allowed item types for this slot (for equipment slots) */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Slot")
    FGameplayTagContainer AllowedItemTypes;

    /** Slot type tag (e.g., Equipment.Slot.Weapon) */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Slot")
    FGameplayTag SlotType;

    FSlotUIData()
    {
        SlotIndex = INDEX_NONE;
        GridX = 0;
        GridY = 0;
        bIsOccupied = false;
        bIsAnchor = false;
        bIsPartOfItem = false;
    }
};

/**
 * UI data for an item displayed in a container
 * Contains only visual information needed for display
 * CRITICAL: Fixed for safe copying during drag operations
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FItemUIData
{
    GENERATED_BODY()

    /** Unique identifier for the item instance */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Item")
    FGuid ItemInstanceID;

    /** Item definition ID for looking up static data */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Item")
    FName ItemID;

    /** Display name of the item */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Item")
    FText DisplayName;

    /** Item description */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Item")
    FText Description;

    /** Item icon texture path for safe copying */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Item")
    FString IconAssetPath;

    /** Current quantity in stack */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Item")
    int32 Quantity;

    /** Maximum stack size */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Item")
    int32 MaxStackSize;

    /** Anchor slot index where item is placed */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Item")
    int32 AnchorSlotIndex;

    /** Item size in grid cells */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Item")
    FIntPoint GridSize;

    /** Whether the item is currently rotated */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Item")
    bool bIsRotated;

    /** Item type tag */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Item")
    FGameplayTag ItemType;

    /** Equipment slot type if equippable */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Item")
    FGameplayTag EquipmentSlotType;

    /** Whether item can be equipped */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Item")
    bool bIsEquippable;

    /** Whether item can be used */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Item")
    bool bIsUsable;

    /** Item weight for display */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Item")
    float Weight;

    /** Ammo display data (optional) */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Item")
    FText AmmoText;

    /** Whether item has ammo to display */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Item")
    bool bHasAmmo;

    /** Custom tooltip class for this item (optional) */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Item", SkipSerialization)
    TSubclassOf<UUserWidget> PreferredTooltipClass;

private:
    /** Cached icon texture - not serialized to avoid copy issues */
    UPROPERTY(Transient, SkipSerialization)
    mutable TSoftObjectPtr<UTexture2D> CachedIcon;

    /** Whether icon has been cached */
    UPROPERTY(Transient, SkipSerialization)
    mutable bool bIconCached;

public:
    FItemUIData()
    {
        ItemInstanceID = FGuid();
        ItemID = NAME_None;
        DisplayName = FText::GetEmpty();
        Description = FText::GetEmpty();
        AmmoText = FText::GetEmpty();
        IconAssetPath.Empty();
        Quantity = 1;
        MaxStackSize = 1;
        AnchorSlotIndex = INDEX_NONE;
        GridSize = FIntPoint(1, 1);
        Weight = 0.0f;
        bIsRotated = false;
        bIsEquippable = false;
        bIsUsable = false;
        bHasAmmo = false;
        bIconCached = false;
    }

    /** Safe icon getter that handles caching */
    UTexture2D* GetIcon() const
    {
        // Быстрый путь: иконка уже закеширована
        if (bIconCached && CachedIcon.IsValid())
        {
            return CachedIcon.Get();
        }

        // Медленный путь: загружаем из пути
        if (!IconAssetPath.IsEmpty())
        {
            // Создаем soft object path
            FSoftObjectPath SoftPath(IconAssetPath);
            if (SoftPath.IsValid())
            {
                // КРИТИЧНО: Используем LoadSynchronous для гарантированной загрузки
                UTexture2D* LoadedIcon = Cast<UTexture2D>(SoftPath.TryLoad());

                if (LoadedIcon)
                {
                    // Кешируем результат
                    CachedIcon = LoadedIcon;
                    bIconCached = true;
                    return LoadedIcon;
                }
            }
        }

        // Финальный fallback
        return CachedIcon.IsValid() ? CachedIcon.Get() : nullptr;
    }

    /** Safe icon setter that stores path */
    void SetIcon(UTexture2D* InIcon)
    {
        if (InIcon)
        {
            IconAssetPath = InIcon->GetPathName();
            CachedIcon = InIcon;
            bIconCached = true;
        }
        else
        {
            IconAssetPath.Empty();
            CachedIcon.Reset();
            bIconCached = false;
        }
    }

    /**
     * Create a safe copy for drag operations
     * This method reconstructs FText members to ensure their validity
     */
    FItemUIData CreateDragCopy() const
    {
        FItemUIData Copy;

        // КРИТИЧЕСКАЯ ПРОВЕРКА: Валидация перед копированием
        if (!IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("CreateDragCopy: Attempting to copy invalid item data!"));
            // Возвращаем пустую, но валидную структуру
            Copy.ItemID = FName(TEXT("InvalidItem"));
            Copy.DisplayName = FText::FromString(TEXT("Invalid"));
            Copy.Description = FText::GetEmpty();
            Copy.Quantity = 1;
            Copy.MaxStackSize = 1;
            Copy.GridSize = FIntPoint(1, 1);
            return Copy;
        }

        // 1. Копируем простые типы (безопасно)
        Copy.ItemInstanceID = ItemInstanceID;
        Copy.ItemID = ItemID;
        Copy.Quantity = Quantity;
        Copy.MaxStackSize = MaxStackSize;
        Copy.AnchorSlotIndex = AnchorSlotIndex;
        Copy.GridSize = GridSize;
        Copy.bIsRotated = bIsRotated;
        Copy.ItemType = ItemType;
        Copy.EquipmentSlotType = EquipmentSlotType;
        Copy.bIsEquippable = bIsEquippable;
        Copy.bIsUsable = bIsUsable;
        Copy.Weight = Weight;
        Copy.bHasAmmo = bHasAmmo;
        Copy.IconAssetPath = IconAssetPath;

        // 2. ИСПРАВЛЕНИЕ: Безопасное копирование FText без ToString()
        // DisplayName
        if (DisplayName.IsEmpty())
        {
            Copy.DisplayName = FText::FromName(ItemID);
        }
        else
        {
            // Создаем новый FText через конструктор копирования
            Copy.DisplayName = FText(DisplayName);
        }

        // Description
        if (Description.IsEmpty())
        {
            Copy.Description = FText::GetEmpty();
        }
        else
        {
            // Создаем новый FText через конструктор копирования
            Copy.Description = FText(Description);
        }

        // AmmoText
        if (bHasAmmo)
        {
            if (AmmoText.IsEmpty())
            {
                Copy.AmmoText = FText::FromString(TEXT("0/0"));
            }
            else
            {
                // Создаем новый FText через конструктор копирования
                Copy.AmmoText = FText(AmmoText);
            }
        }
        else
        {
            Copy.AmmoText = FText::GetEmpty();
        }

        // 3. Сброс кешированных данных
        Copy.CachedIcon.Reset();
        Copy.bIconCached = false;

        // 4. Финальная валидация
        if (!Copy.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("CreateDragCopy: Copy validation failed, using fallback values"));
            Copy.ItemID = FName(TEXT("UnknownItem"));
            Copy.Quantity = 1;
            Copy.GridSize = FIntPoint(1, 1);
            Copy.MaxStackSize = 1;
            Copy.DisplayName = FText::FromName(Copy.ItemID);
        }

        return Copy;
    }

    /** Validate item data integrity */
    bool IsValid() const
    {
        return ItemID != NAME_None &&
               GridSize.X > 0 &&
               GridSize.Y > 0 &&
               Quantity > 0 &&
               MaxStackSize > 0;
    }

    /** Get effective item size considering rotation */
    FIntPoint GetEffectiveSize() const
    {
        if (!IsValid())
        {
            return FIntPoint(1, 1); // Возвращаем минимальный размер для невалидных данных
        }

        return bIsRotated ? FIntPoint(GridSize.Y, GridSize.X) : GridSize;
    }
};

/**
 * UI data for a container (inventory, equipment, etc.)
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FContainerUIData
{
    GENERATED_BODY()

    /** Container type identifier */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Container")
    FGameplayTag ContainerType;

    /** Display name of the container */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Container")
    FText DisplayName;

    /** Container dimensions (for grid-based) */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Container")
    FIntPoint GridSize;

    /** All slots in this container */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Container")
    TArray<FSlotUIData> Slots;

    /** Items currently in container */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Container")
    TArray<FItemUIData> Items;

    /** Allowed item types for this container */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Container")
    FGameplayTagContainer AllowedItemTypes;

    /** Current weight (if applicable) */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Container")
    float CurrentWeight;

    /** Maximum weight (if applicable) */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Container")
    float MaxWeight;

    /** Whether weight limit is enforced */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Container")
    bool bHasWeightLimit;

    /** Whether container is currently locked */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Container")
    bool bIsLocked;

    FContainerUIData()
    {
        DisplayName = FText::GetEmpty();
        GridSize = FIntPoint(1, 1);
        CurrentWeight = 0.0f;
        MaxWeight = 0.0f;
        bHasWeightLimit = false;
        bIsLocked = false;
    }

    /** Validate container data integrity */
    bool IsValid() const
    {
        return GridSize.X > 0 &&
               GridSize.Y > 0 &&
               (Slots.Num() == (GridSize.X * GridSize.Y)) &&
               CurrentWeight >= 0.0f &&
               MaxWeight >= 0.0f;
    }
};

/**
 * Data for drag and drop operations
 * CRITICAL: Redesigned for safe copying and crash prevention
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FDragDropUIData
{
    GENERATED_BODY()

    /** Item being dragged - safe copy */
    UPROPERTY(BlueprintReadOnly, Category = "UI|DragDrop")
    FItemUIData ItemData;

    /** Source container type */
    UPROPERTY(BlueprintReadOnly, Category = "UI|DragDrop")
    FGameplayTag SourceContainerType;

    /** Source slot index */
    UPROPERTY(BlueprintReadOnly, Category = "UI|DragDrop")
    int32 SourceSlotIndex;

    /** Target container type (filled when drop occurs) */
    UPROPERTY(BlueprintReadOnly, Category = "UI|DragDrop")
    FGameplayTag TargetContainerType;

    /** Target slot index (filled when drop occurs) */
    UPROPERTY(BlueprintReadOnly, Category = "UI|DragDrop")
    int32 TargetSlotIndex;

    /** Drag offset in grid cells */
    UPROPERTY(BlueprintReadOnly, Category = "UI|DragDrop")
    FVector2D DragOffset;

    /** Whether split stack mode is active */
    UPROPERTY(BlueprintReadOnly, Category = "UI|DragDrop")
    bool bIsSplitStack;

    /** Quantity being dragged (for split stack) */
    UPROPERTY(BlueprintReadOnly, Category = "UI|DragDrop")
    int32 DraggedQuantity;

    /** Validation flag to prevent invalid data usage */
    UPROPERTY(BlueprintReadOnly, Category = "UI|DragDrop")
    bool bIsValid;

    FDragDropUIData()
    {
        SourceSlotIndex = INDEX_NONE;
        TargetSlotIndex = INDEX_NONE;
        DragOffset = FVector2D::ZeroVector;
        bIsSplitStack = false;
        DraggedQuantity = 0;
        bIsValid = false;
    }

    /** Create a validated drag data from item */
    static FDragDropUIData CreateValidated(const FItemUIData& InItemData,
                                          const FGameplayTag& InSourceContainerType,
                                          int32 InSourceSlotIndex)
    {
        FDragDropUIData DragData;

        // КРИТИЧЕСКАЯ ПРОВЕРКА: Валидация входных данных
        if (!InItemData.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("CreateValidated: Invalid input item data!"));
            return DragData; // Возвращаем невалидные данные
        }

        if (!InSourceContainerType.IsValid() || InSourceSlotIndex < 0)
        {
            UE_LOG(LogTemp, Error, TEXT("CreateValidated: Invalid container type or slot index!"));
            return DragData;
        }

        // Безопасное копирование
        DragData.ItemData = InItemData.CreateDragCopy();

        // Validate the copy was successful
        if (!DragData.ItemData.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("CreateValidated: Failed to create valid item copy"));
            return DragData;
        }

        DragData.SourceContainerType = InSourceContainerType;
        DragData.SourceSlotIndex = InSourceSlotIndex;
        DragData.DraggedQuantity = InItemData.Quantity;
        DragData.bIsValid = true;

        UE_LOG(LogTemp, Verbose, TEXT("CreateValidated: Successfully created drag data for item %s"),
            *DragData.ItemData.ItemID.ToString());

        return DragData;
    }

    /** Validate drag data integrity */
    bool IsValidDragData() const
    {
        return bIsValid &&
               ItemData.IsValid() &&
               SourceSlotIndex >= 0 &&
               DraggedQuantity > 0 &&
               SourceContainerType.IsValid();
    }

    /** Get effective item size considering rotation */
    FIntPoint GetEffectiveSize() const
    {
        if (!IsValidDragData())
        {
            return FIntPoint(1, 1);
        }

        return ItemData.bIsRotated ?
            FIntPoint(ItemData.GridSize.Y, ItemData.GridSize.X) :
            ItemData.GridSize;
    }
};

/**
 * UI-specific slot validation result
 * RENAMED from FSlotValidationResult to avoid conflict with SuspenseEquipmentTypes.h
 * This is specifically for UI validation, not gameplay validation
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FUISlotValidationResult
{
    GENERATED_BODY()

    /** Whether the slot is valid for the UI operation */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Validation")
    bool bIsValid;

    /** UI-friendly reason if not valid */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Validation")
    FText Reason;

    /** Suggested alternative slot index for UI hint */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Validation")
    int32 AlternativeSlotIndex;

    /** Visual feedback type for UI */
    UPROPERTY(BlueprintReadOnly, Category = "UI|Validation")
    FGameplayTag FeedbackType;

    FUISlotValidationResult()
    {
        bIsValid = false;
        Reason = FText::GetEmpty();
        AlternativeSlotIndex = INDEX_NONE;
    }

    /** Create success result for UI */
    static FUISlotValidationResult CreateSuccess()
    {
        FUISlotValidationResult Result;
        Result.bIsValid = true;
        return Result;
    }

    /** Create failure result with UI-friendly reason */
    static FUISlotValidationResult CreateFailure(const FText& InReason, int32 InAlternativeSlot = INDEX_NONE)
    {
        FUISlotValidationResult Result;
        Result.bIsValid = false;
        Result.Reason = InReason;
        Result.AlternativeSlotIndex = InAlternativeSlot;
        return Result;
    }

    /** Create result with visual feedback hint */
    static FUISlotValidationResult CreateWithFeedback(bool bValid, const FText& InReason, const FGameplayTag& InFeedbackType)
    {
        FUISlotValidationResult Result;
        Result.bIsValid = bValid;
        Result.Reason = InReason;
        Result.FeedbackType = InFeedbackType;
        return Result;
    }
};

/**
 * Smart drop zone detection for magnetic snapping
 * Used by container widgets to find optimal drop positions
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSmartDropZone
{
    GENERATED_BODY()

    /** Target slot index where item would be placed */
    UPROPERTY(BlueprintReadOnly, Category = "UI|DragDrop")
    int32 SlotIndex = INDEX_NONE;

    /** Distance from cursor to drop zone center in pixels */
    UPROPERTY(BlueprintReadOnly, Category = "UI|DragDrop")
    float Distance = 0.0f;

    /** Snap strength (0-1) based on distance and configuration */
    UPROPERTY(BlueprintReadOnly, Category = "UI|DragDrop")
    float SnapStrength = 0.0f;

    /** Visual feedback position in screen space */
    UPROPERTY(BlueprintReadOnly, Category = "UI|DragDrop")
    FVector2D FeedbackPosition = FVector2D::ZeroVector;

    /** Whether this is a valid drop target for the current item */
    UPROPERTY(BlueprintReadOnly, Category = "UI|DragDrop")
    bool bIsValid = false;

    FSmartDropZone()
    {
        SlotIndex = INDEX_NONE;
        Distance = 0.0f;
        SnapStrength = 0.0f;
        FeedbackPosition = FVector2D::ZeroVector;
        bIsValid = false;
    }
};

/**
 * Result of a drag drop operation
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FDragDropResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "UI|DragDrop")
    bool bSuccess;

    UPROPERTY(BlueprintReadOnly, Category = "UI|DragDrop")
    FText Message;

    FDragDropResult()
    {
        bSuccess = false;
        Message = FText::GetEmpty();
    }

    FDragDropResult(bool InSuccess, const FText& InMessage)
    {
        bSuccess = InSuccess;
        Message = InMessage;
    }
};


#endif // SUSPENSECORE_TYPES_UI_SUSPENSECORECONTAINERUITYPES_H