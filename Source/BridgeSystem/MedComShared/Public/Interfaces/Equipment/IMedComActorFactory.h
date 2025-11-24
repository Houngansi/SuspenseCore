// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Types/Inventory/InventoryTypes.h"
#include "IMedComActorFactory.generated.h"

/**
 * Actor spawn parameters
 */
USTRUCT(BlueprintType)
struct FEquipmentActorSpawnParams
{
    GENERATED_BODY()
    
    UPROPERTY()
    FInventoryItemInstance ItemInstance;
    
    UPROPERTY()
    FTransform SpawnTransform;
    
    UPROPERTY()
    AActor* Owner = nullptr;
    
    UPROPERTY()
    APawn* Instigator = nullptr;
    
    UPROPERTY()
    bool bDeferredSpawn = false;
    
    UPROPERTY()
    bool bNoCollisionFail = true;
    
    UPROPERTY()
    TMap<FString, FString> CustomParameters;
};

/**
 * Actor spawn result
 */
USTRUCT(BlueprintType)
struct FEquipmentActorSpawnResult
{
    GENERATED_BODY()
    
    UPROPERTY()
    bool bSuccess = false;
    
    UPROPERTY()
    AActor* SpawnedActor = nullptr;
    
    UPROPERTY()
    FText FailureReason;
    
    UPROPERTY()
    float SpawnTime = 0.0f;
};

UINTERFACE(MinimalAPI, Blueprintable)
class UMedComActorFactory : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for equipment actor creation
 * 
 * Philosophy: Factory pattern for equipment actors.
 * Handles spawning, configuration, and lifecycle.
 */
class MEDCOMSHARED_API IMedComActorFactory
{
    GENERATED_BODY()

public:
    /**
     * Spawn equipment actor
     * @param Params Spawn parameters
     * @return Spawn result
     */
    virtual FEquipmentActorSpawnResult SpawnEquipmentActor(const FEquipmentActorSpawnParams& Params) = 0;
    
    /**
     * Destroy equipment actor
     * @param Actor Actor to destroy
     * @param bImmediate Destroy immediately or deferred
     * @return True if destroyed
     */
    virtual bool DestroyEquipmentActor(AActor* Actor, bool bImmediate = false) = 0;
    
    /**
     * Configure spawned actor with item data
     * @param Actor Actor to configure
     * @param ItemInstance Item data
     * @return True if configured
     */
    virtual bool ConfigureEquipmentActor(AActor* Actor, const FInventoryItemInstance& ItemInstance) = 0;
    
    /**
     * Recycle actor to pool
     * @param Actor Actor to recycle
     * @return True if recycled
     */
    virtual bool RecycleActor(AActor* Actor) = 0;
    
    /**
     * Get actor from pool
     * @param ActorClass Class to get
     * @return Actor or nullptr
     */
    virtual AActor* GetPooledActor(TSubclassOf<AActor> ActorClass) = 0;
    
    /**
     * Preload actor class
     * @param ItemId Item to preload for
     * @return True if preloaded
     */
    virtual bool PreloadActorClass(const FName& ItemId) = 0;
    
    /**
     * Get spawn transform for slot
     * @param SlotIndex Equipment slot
     * @param Owner Owner actor
     * @return Spawn transform
     */
    virtual FTransform GetSpawnTransformForSlot(int32 SlotIndex, AActor* Owner) const = 0;
    
    /**
     * Register spawned actor
     * @param Actor Actor to register
     * @param SlotIndex Associated slot
     * @return True if registered
     */
    virtual bool RegisterSpawnedActor(AActor* Actor, int32 SlotIndex) = 0;
    
    /**
     * Unregister actor
     * @param Actor Actor to unregister
     * @return True if unregistered
     */
    virtual bool UnregisterActor(AActor* Actor) = 0;
    
    /**
     * Get all spawned actors
     * @return Map of slot to actor
     */
    virtual TMap<int32, AActor*> GetAllSpawnedActors() const = 0;
    
    /**
     * Clear all spawned actors
     * @param bDestroy Destroy actors or just unregister
     */
    virtual void ClearAllActors(bool bDestroy = true) = 0;
};