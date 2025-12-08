// ISuspenseItemFactoryInterface.h
// SuspenseCore Item Factory Interface
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ISuspenseItemFactoryInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class USuspenseItemFactoryInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * ISuspenseItemFactoryInterface
 *
 * Interface for item factory implementations.
 * Provides methods for creating pickup actors from item data.
 */
class INTERACTIONSYSTEM_API ISuspenseItemFactoryInterface
{
    GENERATED_BODY()

public:
    /**
     * Create pickup actor from item ID
     * @param ItemID Item identifier from DataTable
     * @param World World for spawning
     * @param Transform Spawn transform
     * @param Quantity Item amount
     * @return Spawned pickup actor or nullptr
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item Factory")
    AActor* CreatePickupFromItemID(FName ItemID, UWorld* World, const FTransform& Transform, int32 Quantity);
    virtual AActor* CreatePickupFromItemID_Implementation(FName ItemID, UWorld* World, const FTransform& Transform, int32 Quantity) { return nullptr; }

    /**
     * Get default pickup actor class
     * @return Default pickup class
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item Factory")
    TSubclassOf<AActor> GetDefaultPickupClass() const;
    virtual TSubclassOf<AActor> GetDefaultPickupClass_Implementation() const { return nullptr; }

    /**
     * Check if factory can create item
     * @param ItemID Item to check
     * @return True if item can be created
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item Factory")
    bool CanCreateItem(FName ItemID) const;
    virtual bool CanCreateItem_Implementation(FName ItemID) const { return true; }

    /**
     * Get item data table
     * @return DataTable with item definitions
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item Factory")
    UDataTable* GetItemDataTable() const;
    virtual UDataTable* GetItemDataTable_Implementation() const { return nullptr; }
};
