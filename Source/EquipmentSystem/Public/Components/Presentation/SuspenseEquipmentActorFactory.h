// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/Equipment/ISuspenseActorFactory.h"
#include "Core/Utils/SuspenseEquipmentCacheManager.h"
#include "Core/Utils/SuspenseEquipmentThreadGuard.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "Engine/StreamableManager.h"
#include "SuspenseEquipmentActorFactory.generated.h"

/**
 * Actor pool entry for object pooling
 */
USTRUCT()
struct FActorPoolEntry
{
    GENERATED_BODY()

    UPROPERTY()
    AActor* Actor = nullptr;

    UPROPERTY()
    TSubclassOf<AActor> ActorClass;

    UPROPERTY()
    bool bInUse = false;

    UPROPERTY()
    float LastUsedTime = 0.0f;

    UPROPERTY()
    FName ItemId;
};

/**
 * Factory configuration
 */
USTRUCT(BlueprintType)
struct FActorFactoryConfig
{
    GENERATED_BODY()

    /** Maximum actors in pool per class */
    UPROPERTY(EditAnywhere, Category = "Pool")
    int32 MaxPoolSizePerClass = 5;

    /** Pool cleanup interval in seconds */
    UPROPERTY(EditAnywhere, Category = "Pool")
    float PoolCleanupInterval = 30.0f;

    /** Actor idle time before cleanup */
    UPROPERTY(EditAnywhere, Category = "Pool")
    float ActorIdleTimeout = 60.0f;

    /** Enable async loading of actor classes */
    UPROPERTY(EditAnywhere, Category = "Loading")
    bool bEnableAsyncLoading = true;

    /** Preload priority classes */
    UPROPERTY(EditAnywhere, Category = "Loading")
    TArray<FName> PriorityPreloadItems;
};

/**
 * Equipment Actor Factory Component
 *
 * Pure factory pattern implementation for equipment actors.
 * Responsible ONLY for creation, configuration, and lifecycle management.
 * Uses object pooling and caching for performance optimization.
 *
 * Key principles:
 * - Single Responsibility: Only handles actor creation and pooling
 * - No attachment logic, no visual effects, no slot management
 * - Thread-safe operations through FEquipmentThreadGuard
 * - Efficient caching through FEquipmentCacheManager
 */
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseEquipmentActorFactory : public UActorComponent, public ISuspenseActorFactory
{
    GENERATED_BODY()

public:
    USuspenseEquipmentActorFactory();

    //~ Begin UActorComponent Interface
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    //~ End UActorComponent Interface

    //~ Begin ISuspenseActorFactory Interface
    virtual FEquipmentActorSpawnResult SpawnEquipmentActor(const FEquipmentActorSpawnParams& Params) override;
    virtual bool DestroyEquipmentActor(AActor* Actor, bool bImmediate = false) override;
    virtual bool ConfigureEquipmentActor(AActor* Actor, const FSuspenseInventoryItemInstance& ItemInstance) override;
    virtual bool RecycleActor(AActor* Actor) override;
    virtual AActor* GetPooledActor(TSubclassOf<AActor> ActorClass) override;
    virtual bool PreloadActorClass(const FName& ItemId) override;
    virtual FTransform GetSpawnTransformForSlot(int32 SlotIndex, AActor* Owner) const override;
    virtual bool RegisterSpawnedActor(AActor* Actor, int32 SlotIndex) override;
    virtual bool UnregisterActor(AActor* Actor) override;
    virtual TMap<int32, AActor*> GetAllSpawnedActors() const override;
    virtual void ClearAllActors(bool bDestroy = true) override;
    //~ End ISuspenseActorFactory Interface

    /** Set factory configuration */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Factory")
    void SetFactoryConfiguration(const FActorFactoryConfig& NewConfig);

    /** Get pool statistics */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Factory")
    void GetPoolStatistics(int32& TotalActors, int32& ActiveActors, int32& PooledActors) const;

    /** Preload multiple item classes */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Factory")
    void PreloadItemClasses(const TArray<FName>& ItemIds);

protected:
    /** Configuration */
    UPROPERTY(EditAnywhere, Category = "Factory Config")
    FActorFactoryConfig FactoryConfig;

    /** Actor pool */
    UPROPERTY()
    TArray<FActorPoolEntry> ActorPool;

    /** Registry of spawned actors by slot */
    UPROPERTY()
    TMap<int32, AActor*> SpawnedActorRegistry;

    /** Cache manager for actor classes */
    FSuspenseEquipmentCacheManager<FName, TSubclassOf<AActor>> ActorClassCache;

    /** Streamable manager for async loading */
    FStreamableManager StreamableManager;

    /** Async loading handles */
    TMap<FName, TSharedPtr<struct FStreamableHandle>> LoadingHandles;

    /** Thread guards for thread-safe operations */
    mutable FCriticalSection PoolLock;
    mutable FCriticalSection RegistryLock;

    /** Pool cleanup timer handle */
    FTimerHandle PoolCleanupTimerHandle;

private:
    /** Internal spawn logic */
    AActor* SpawnActorInternal(TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform, AActor* Owner);

    /** Internal destroy logic */
    void DestroyActorInternal(AActor* Actor, bool bImmediate);

    /** Get actor class for item */
    TSubclassOf<AActor> GetActorClassForItem(const FName& ItemId);

    /** Find pool entry for actor */
    FActorPoolEntry* FindPoolEntry(AActor* Actor);

    /** Find available pool entry for class */
    FActorPoolEntry* FindAvailablePoolEntry(TSubclassOf<AActor> ActorClass);

    /** Create new pool entry */
    FActorPoolEntry* CreatePoolEntry(TSubclassOf<AActor> ActorClass);

    /** Clean up expired pool entries */
    void CleanupPool();

    /** Async load complete callback */
    void OnAsyncLoadComplete(FName ItemId);

    /** Validate actor for operations */
    bool IsActorValid(AActor* Actor) const;

    /** Log factory operation */
    void LogFactoryOperation(const FString& Operation, const FString& Details) const;
};
