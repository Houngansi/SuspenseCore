// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Types/Inventory/InventoryTypes.h"
#include "Types/Equipment/EquipmentTypes.h"
#include "GameplayTagContainer.h"
#include "IMedComSlotValidator.generated.h"

/**
 * Validation result with detailed error information
 */
USTRUCT(BlueprintType)
struct FEquipmentSlotValidationResult
{
    GENERATED_BODY()
    
    UPROPERTY()
    bool bIsValid = false;
    
    UPROPERTY()
    FText ErrorMessage;
    
    UPROPERTY()
    FGameplayTag ErrorType;
    
    UPROPERTY()
    TArray<FString> ValidationDetails;
    
    static FEquipmentSlotValidationResult Success()
    {
        FEquipmentSlotValidationResult Result;
        Result.bIsValid = true;
        return Result;
    }
    
    static FEquipmentSlotValidationResult Failure(const FText& Error, const FGameplayTag& Type = FGameplayTag())
    {
        FEquipmentSlotValidationResult Result;
        Result.bIsValid = false;
        Result.ErrorMessage = Error;
        Result.ErrorType = Type;
        return Result;
    }
};

UINTERFACE(MinimalAPI, Blueprintable)
class UMedComSlotValidator : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for slot validation logic
 * 
 * Philosophy: Centralized validation rules for slot operations.
 * Ensures data integrity and enforces game rules.
 */
class MEDCOMSHARED_API IMedComSlotValidator
{
    GENERATED_BODY()

public:
    /**
     * Validate if item can be placed in slot
     * @param SlotConfig Slot configuration
     * @param ItemInstance Item to validate
     * @return Validation result
     */
    virtual FSlotValidationResult CanPlaceItemInSlot(
        const FEquipmentSlotConfig& SlotConfig,
        const FInventoryItemInstance& ItemInstance) const = 0;
    
    /**
     * Validate if items can be swapped between slots
     * @param SlotConfigA First slot configuration
     * @param ItemA Item in first slot
     * @param SlotConfigB Second slot configuration
     * @param ItemB Item in second slot
     * @return Validation result
     */
    virtual FSlotValidationResult CanSwapItems(
        const FEquipmentSlotConfig& SlotConfigA,
        const FInventoryItemInstance& ItemA,
        const FEquipmentSlotConfig& SlotConfigB,
        const FInventoryItemInstance& ItemB) const = 0;
    
    /**
     * Validate slot configuration integrity
     * @param SlotConfig Configuration to validate
     * @return Validation result
     */
    virtual FSlotValidationResult ValidateSlotConfiguration(
        const FEquipmentSlotConfig& SlotConfig) const = 0;
    
    /**
     * Check if slot satisfies requirements
     * @param SlotConfig Slot configuration
     * @param Requirements Requirements to check
     * @return Validation result
     */
    virtual FSlotValidationResult CheckSlotRequirements(
        const FEquipmentSlotConfig& SlotConfig,
        const FGameplayTagContainer& Requirements) const = 0;
    
    /**
     * Validate item compatibility with slot type
     * @param ItemType Type of item
     * @param SlotType Type of slot
     * @return True if compatible
     */
    virtual bool IsItemTypeCompatibleWithSlot(
        const FGameplayTag& ItemType,
        EEquipmentSlotType SlotType) const = 0;
};