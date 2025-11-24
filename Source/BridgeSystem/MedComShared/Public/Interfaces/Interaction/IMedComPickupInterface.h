// IMedComPickupInterface.h
// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "Types/Inventory/InventoryTypes.h"
#include "Types/Loadout/MedComItemDataTable.h"
#include "IMedComPickupInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UMedComPickupInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for pickup actors in the world
 * Fully integrated with unified DataTable system
 * 
 * Architecture principles:
 * - Single source of truth: FMedComUnifiedItemData in DataTable
 * - ItemID is the only reference to item data
 * - Runtime state (amount, ammo) stored separately
 * - No data duplication or legacy structures
 */
class MEDCOMSHARED_API IMedComPickupInterface
{
    GENERATED_BODY()

public:
    //==================================================================
    // Core Pickup Properties - DataTable Based
    //==================================================================
    
    /**
     * Get item ID for DataTable lookup
     * This is the primary identifier linking to FMedComUnifiedItemData
     * @return Item identifier from DataTable
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickup")
    FName GetItemID() const;
    
    /**
     * Set item ID for DataTable reference
     * @param NewItemID Item identifier to set
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickup")
    void SetItemID(FName NewItemID);
    
    /**
     * Get unified item data from DataTable
     * @param OutItemData Output structure with all static item data
     * @return True if data was successfully retrieved
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickup")
    bool GetUnifiedItemData(FMedComUnifiedItemData& OutItemData) const;
    
    //==================================================================
    // Runtime Properties
    //==================================================================
    
    /**
     * Get item amount in this pickup
     * @return Number of items in the pickup
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickup")
    int32 GetItemAmount() const;
    
    /**
     * Set item amount
     * @param NewAmount New quantity to set
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickup")
    void SetAmount(int32 NewAmount);
    
    /**
     * Create runtime item instance for inventory
     * @param OutInstance Output runtime instance with all properties
     * @return True if instance created successfully
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickup")
    bool CreateItemInstance(FInventoryItemInstance& OutInstance) const;
    
    //==================================================================
    // Weapon State Persistence
    //==================================================================
    
    /**
     * Check if pickup has saved ammo state (for dropped weapons)
     * @return True if ammo state is stored
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickup|Weapon")
    bool HasSavedAmmoState() const;
    
    /**
     * Get saved ammo state for weapons
     * @param OutCurrentAmmo Current ammo in magazine
     * @param OutRemainingAmmo Remaining ammo in reserves
     * @return True if ammo state was retrieved
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickup|Weapon")
    bool GetSavedAmmoState(float& OutCurrentAmmo, float& OutRemainingAmmo) const;
    
    /**
     * Set saved ammo state for weapons
     * @param CurrentAmmo Current ammo in magazine
     * @param RemainingAmmo Remaining ammo in reserves
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickup|Weapon")
    void SetSavedAmmoState(float CurrentAmmo, float RemainingAmmo);
    
    //==================================================================
    // Pickup Behavior
    //==================================================================
    
    /**
     * Handle being picked up by an actor
     * @param InstigatorActor Actor attempting to pick up this item
     * @return True if pickup was successful
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickup")
    bool HandlePickedUp(AActor* InstigatorActor);
    
    /**
     * Check if this pickup can be collected by specific actor
     * @param InstigatorActor Actor to check
     * @return True if actor can pick up this item
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickup")
    bool CanBePickedUpBy(AActor* InstigatorActor) const;
    
    //==================================================================
    // Item Properties from DataTable
    //==================================================================
    
    /**
     * Get item type from DataTable
     * @return Effective item type gameplay tag
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickup")
    FGameplayTag GetItemType() const;
    
    /**
     * Get item rarity from DataTable
     * @return Item rarity gameplay tag
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickup")
    FGameplayTag GetItemRarity() const;
    
    /**
     * Get display name from DataTable
     * @return Localized display name
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickup")
    FText GetDisplayName() const;
    
    /**
     * Check if item is stackable
     * @return True if max stack size > 1
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickup")
    bool IsStackable() const;
    
    /**
     * Get item weight per unit
     * @return Weight from DataTable
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickup")
    float GetItemWeight() const;
};