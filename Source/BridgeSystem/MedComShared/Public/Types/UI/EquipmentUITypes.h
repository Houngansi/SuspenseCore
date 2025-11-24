// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/UI/ContainerUITypes.h"
#include "Interfaces/Equipment/IMedComEquipmentInterface.h"
#include "EquipmentUITypes.generated.h"

/**
 * UI representation of equipment slot
 */
USTRUCT(BlueprintType)
struct MEDCOMSHARED_API FEquipmentSlotUIData
{
    GENERATED_BODY()

    /** Slot index in equipment array */
    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    int32 SlotIndex = -1;

    /** Type of equipment slot */
    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    FGameplayTag SlotType;

    /** Display name of slot */
    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    FText SlotName;

    /** Allowed item types for this slot */
    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    FGameplayTagContainer AllowedItemTypes;

    /** Whether slot is occupied */
    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    bool bIsOccupied = false;

    /** Equipped item data if occupied */
    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    FItemUIData EquippedItem;

    /** ДОБАВЛЕНО: Полный экземпляр предмета для сохранения runtime данных */
    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    FInventoryItemInstance ItemInstance;

    /** ДОБАВЛЕНО: Интерфейс к слоту экипировки для прямого взаимодействия */
    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    TScriptInterface<IMedComEquipmentInterface> SlotInterface;

    /** Grid size for this slot (usually 1x1 for equipment) */
    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    FIntPoint GridSize = FIntPoint(1, 1);

    /** Icon to show when slot is empty */
    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    TSoftObjectPtr<UTexture2D> EmptySlotIcon;

    /** Whether this slot is locked */
    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    bool bIsLocked = false;

    /** Slot position in UI grid */
    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    FIntPoint GridPosition = FIntPoint(0, 0);

    /** Whether this slot is required for valid loadout */
    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    bool bIsRequired = false;

    FEquipmentSlotUIData()
    {
        SlotIndex = -1;
        bIsOccupied = false;
        bIsLocked = false;
        GridSize = FIntPoint(1, 1);
        GridPosition = FIntPoint(0, 0);
    }

    bool IsValid() const
    {
        return SlotIndex >= 0 && SlotType.IsValid();
    }
};

/**
 * Container data for equipment UI
 */
USTRUCT(BlueprintType)
struct MEDCOMSHARED_API FEquipmentContainerUIData
{
    GENERATED_BODY()

    /** All equipment slots */
    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    TArray<FEquipmentSlotUIData> Slots;

    /** Container display name */
    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    FText DisplayName;

    /** Total equipment weight */
    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    float TotalWeight = 0.0f;

    /** Total armor value */
    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    float TotalArmor = 0.0f;

    /** Container type tag */
    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    FGameplayTag ContainerType;

    FEquipmentContainerUIData()
    {
        DisplayName = NSLOCTEXT("Equipment", "EquipmentTitle", "Equipment");
        ContainerType = FGameplayTag::RequestGameplayTag(TEXT("Container.Equipment"));
        TotalWeight = 0.0f;
        TotalArmor = 0.0f;
    }
};