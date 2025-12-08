// Copyright Suspense Team. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SuspenseInventoryTypeRegistry.generated.h"

/**
 * Structure representing a registered item type
 */
USTRUCT(BlueprintType)
struct FInventoryItemTypeInfo
{
 GENERATED_BODY()

 // Tag for this item type
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Types")
 FGameplayTag TypeTag;

 // Display name
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Types")
 FText DisplayName;

 // Description
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Types")
 FText Description;

 // Icon
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Types")
 UTexture2D* Icon = nullptr;

 // Default weight
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Types")
 float DefaultWeight = 1.0f;

 // Default grid size
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Types")
 FVector2D DefaultGridSize = FVector2D(1.0f, 1.0f);

 // Compatible slot types
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Types")
 FGameplayTagContainer CompatibleSlots;

 // Явный конструктор
 FInventoryItemTypeInfo()
 {
  DisplayName = FText::GetEmpty();
  Description = FText::GetEmpty();
  Icon = nullptr;
  DefaultWeight = 1.0f;
  DefaultGridSize = FVector2D(1.0f, 1.0f);
 }
};

/**
 * Registry for inventory item types
 * Manages type registration, validation, and compatibility
 */
UCLASS(BlueprintType, Blueprintable, Config=Game, defaultconfig)
class BRIDGESYSTEM_API UInventoryTypeRegistry : public UObject
{
    GENERATED_BODY()

public:
    UInventoryTypeRegistry();

    // Добавленные методы жизненного цикла UObject для исправления ошибок
    virtual void PostInitProperties() override;
    virtual void BeginDestroy() override;

    // Registry of known item types
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Inventory|Types")
    TArray<FInventoryItemTypeInfo> RegisteredTypes;

    /**
     * Registers a new item type
     * @param TypeInfo Type information
     * @return True if registration succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Types")
    bool RegisterItemType(const FInventoryItemTypeInfo& TypeInfo);

    /**
     * Checks if an item type is registered
     * @param TypeTag Tag to check
     * @return True if type is registered
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Types")
    bool IsTypeRegistered(const FGameplayTag& TypeTag) const;

    /**
     * Gets type info for a registered type
     * @param TypeTag Tag to look up
     * @param OutTypeInfo Output type information
     * @return True if type was found
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Types")
    bool GetTypeInfo(const FGameplayTag& TypeTag, FInventoryItemTypeInfo& OutTypeInfo) const;

    /**
     * Checks if two types are compatible
     * @param ItemType Item type
     * @param SlotType Slot type
     * @return True if compatible
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Types")
    bool AreTypesCompatible(const FGameplayTag& ItemType, const FGameplayTag& SlotType) const;

    /**
     * Gets default grid size for a type
     * @param TypeTag Type tag
     * @return Default grid size or (1,1) if not found
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Types")
    FVector2D GetDefaultGridSize(const FGameplayTag& TypeTag) const;

    /**
     * Gets default weight for a type
     * @param TypeTag Type tag
     * @return Default weight or 1.0 if not found
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Types")
    float GetDefaultWeight(const FGameplayTag& TypeTag) const;

    /**
     * Gets all registered types
     * @return Array of registered types
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Types")
    TArray<FInventoryItemTypeInfo> GetAllRegisteredTypes() const;

    /**
     * Gets all compatible slot types for an item type
     * @param ItemType Item type
     * @return Container of compatible slots
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Types")
    FGameplayTagContainer GetCompatibleSlots(const FGameplayTag& ItemType) const;

    /**
     * Gets instance of the registry
     * @return Singleton instance
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Types")
    static UInventoryTypeRegistry* GetInstance();

private:
    // Singleton instance
    static UInventoryTypeRegistry* Instance;

    /**
     * Initializes registry with default types
     */
    void InitializeDefaultTypes();
};


