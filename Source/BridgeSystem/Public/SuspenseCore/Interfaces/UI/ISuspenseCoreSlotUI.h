// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SuspenseCore/Types/UI/SuspenseCoreContainerUITypes.h"
#include "ISuspenseCoreSlotUI.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class USuspenseSlotUI : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for individual slot widgets
 * Provides unified way to display and interact with container slots
 * 
 * IMPORTANT: Always use Execute_ methods when calling interface functions from C++
 * Never call interface methods directly!
 */
class BRIDGESYSTEM_API ISuspenseSlotUI
{
    GENERATED_BODY()

public:
    /**
     * Initialize slot with display data
     * @param SlotData Data for displaying the slot
     * @param ItemData Optional item data if slot is occupied
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Slot")
    void InitializeSlot(const FSlotUIData& SlotData, const FItemUIData& ItemData);

    /**
     * Update slot display
     * @param SlotData Updated slot data
     * @param ItemData Updated item data (empty struct if no item)
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Slot")
    void UpdateSlot(const FSlotUIData& SlotData, const FItemUIData& ItemData);

    /**
     * Set slot selection state
     * @param bIsSelected Whether slot is selected
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Slot")
    void SetSelected(bool bIsSelected);

    /**
     * Set slot highlight state
     * @param bIsHighlighted Whether slot is highlighted
     * @param HighlightColor Color to use for highlight
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Slot")
    void SetHighlighted(bool bIsHighlighted, const FLinearColor& HighlightColor);

    /**
     * Get slot index
     * @return Slot index in container
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Slot")
    int32 GetSlotIndex() const;

    /**
     * Get item instance ID if slot has item
     * @return Item instance ID or invalid GUID
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Slot")
    FGuid GetItemInstanceID() const;

    /**
     * Check if slot is occupied
     * @return True if slot has an item
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Slot")
    bool IsOccupied() const;

    /**
     * Set slot locked state
     * @param bIsLocked Whether slot is locked
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Slot")
    void SetLocked(bool bIsLocked);
    
protected:
    // Helper macro to prevent direct interface calls
    #define DIRECT_INTERFACE_CALL_NOT_ALLOWED() \
        checkf(0, TEXT("Do not call interface methods directly! Use Execute_ methods instead."))
};

