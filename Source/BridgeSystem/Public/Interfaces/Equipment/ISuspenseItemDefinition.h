// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "Engine/Texture2D.h"
#include "ISuspenseItemDefinition.generated.h"

// Forward declarations
class USuspenseEventManager;
class UGameplayEffect;
class UAttributeSet;

UINTERFACE(MinimalAPI, BlueprintType, meta=(NotBlueprintable))
class USuspenseItemDefinitionInterface : public UInterface
{
    GENERATED_BODY()
};

class BRIDGESYSTEM_API ISuspenseItemDefinition
{
    GENERATED_BODY()

public:
    //================================================
    // Core Item Identity Methods
    //================================================
    
    virtual FName GetItemID() const = 0;
    virtual FText GetItemName() const = 0;
    virtual FText GetItemDescription() const = 0;
    virtual TSoftObjectPtr<UTexture2D> GetItemIcon() const = 0;

    //================================================
    // Item Classification Methods
    //================================================
    
    virtual FGameplayTag GetItemType() const = 0;
    virtual bool IsEquippable() const = 0;
    virtual FGameplayTag GetEquipmentSlotType() const = 0;

    //================================================
    // Item Properties Methods
    //================================================
    
    virtual int32 GetMaxStackSize() const = 0;
    virtual FVector2D GetGridSize() const = 0;
    virtual float GetWeight() const = 0;

    //================================================
    // Item Type Detection Methods
    //================================================
    
    virtual bool IsWeapon() const = 0;
    virtual bool IsArmor() const = 0;
    virtual bool IsHelmet() const = 0;
    virtual bool IsAmmo() const = 0;
    virtual bool IsConsumable() const = 0;

    //================================================
    // Weapon Definition Methods
    //================================================
    
    virtual bool GetWeaponAmmoTemplate(float& OutDefaultAmmo, float& OutMaxAmmo) const = 0;
    virtual float GetWeaponBaseDamage() const = 0;
    virtual FGameplayTag GetWeaponAmmoType() const = 0;

    //================================================
    // Equipment Integration Methods
    //================================================
    
    virtual TSubclassOf<UGameplayEffect> GetEquipmentEffect() const = 0;
    virtual TSubclassOf<UAttributeSet> GetItemAttributeSet() const = 0;

    //================================================
    // Factory and Creation Methods
    //================================================
    
    virtual TSoftClassPtr<AActor> GetPickupActorClass() const = 0;
    virtual struct FMCInventoryItemData ToInventoryItemData(int32 Quantity = 1) const = 0;

    //================================================
    // Validation and Utility Methods
    //================================================
    
    virtual bool IsValidDefinition() const = 0;
    virtual bool IsCompatibleWithSlot(const FGameplayTag& SlotTag) const = 0;
    virtual int32 GetSortPriority() const = 0;

    //================================================
    // Event System Integration
    //================================================
    
    /**
     * Gets access to the central delegate manager
     * @return Pointer to delegate manager or nullptr
     */
    virtual USuspenseEventManager* GetDelegateManager() const = 0;
    
    /**
     * Static helper to get delegate manager from world context
     * @param WorldContextObject Any object with valid world context
     * @return Delegate manager or nullptr
     */
    static USuspenseEventManager* GetDelegateManagerStatic(const UObject* WorldContextObject);
};