// ISuspenseItemFactory.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "ISuspenseItemFactory.generated.h"

// Forward declarations
struct FSuspenseInventoryItemInstance;
struct FSuspensePickupSpawnData;

UINTERFACE(MinimalAPI, BlueprintType, meta=(NotBlueprintable))
class USuspenseItemFactoryInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for item factory - creates pickups without circular dependencies
 * Provides multiple methods for different pickup creation scenarios
 */
class BRIDGESYSTEM_API ISuspenseItemFactory
{
    GENERATED_BODY()

public:
    /**
     * Create basic pickup actor by ItemID
     * @param ItemID Item identifier from DataTable
     * @param World World for spawning
     * @param Transform Spawn transform
     * @param Quantity Number of items
     * @return Spawned pickup actor or nullptr
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Items")
    AActor* CreatePickupFromItemID(
        FName ItemID, 
        UWorld* World, 
        const FTransform& Transform, 
        int32 Quantity
    );
    
    /**
     * Create pickup from runtime item instance (for dropping equipped items)
     * Preserves runtime state like weapon ammo, durability, etc.
     * @param ItemInstance Runtime instance with all properties
     * @param World World for spawning
     * @param Transform Spawn transform
     * @return Spawned pickup actor or nullptr
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Items")
    AActor* CreatePickupFromInstance(
        const FSuspenseInventoryItemInstance& ItemInstance,
        UWorld* World,
        const FTransform& Transform
    );
    
    /**
     * Create pickup with custom spawn data
     * Supports preset runtime properties for special cases
     * @param SpawnData Spawn configuration with preset properties
     * @param World World for spawning
     * @param Transform Spawn transform
     * @return Spawned pickup actor or nullptr
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Items")
    AActor* CreatePickupFromSpawnData(
        const FSuspensePickupSpawnData& SpawnData,
        UWorld* World,
        const FTransform& Transform
    );
    
    /**
     * Get default pickup actor class for customization
     * @return Default pickup class or nullptr
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Items")
    TSubclassOf<AActor> GetDefaultPickupClass() const;
};