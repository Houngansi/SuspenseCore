// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreActorFactory.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentService.h"
#include "SuspenseCore/Core/Utils/SuspenseCoreEquipmentCacheManager.h"
#include "SuspenseCore/Core/Utils/SuspenseCoreEquipmentThreadGuard.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
#include "Engine/StreamableManager.h"
#include "SuspenseCore/Components/Coordination/SuspenseCoreEquipmentEventDispatcher.h"
#include "SuspenseCoreEquipmentActorFactory.generated.h"

/**
 * Actor pool entry for object pooling
 */
USTRUCT()
struct FSuspenseCoreActorPoolEntry
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
struct FSuspenseCoreActorFactoryConfig
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
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentActorFactory : public UActorComponent, public ISuspenseCoreActorFactory, public ISuspenseCoreEquipmentService
{
    GENERATED_BODY()

public:
    USuspenseCoreEquipmentActorFactory();

    //~ Begin UActorComponent Interface
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    //~ End UActorComponent Interface

    //~ Begin ISuspenseCoreEquipmentService Interface
    virtual bool InitializeService(const FSuspenseCoreServiceInitParams& Params) override;
    virtual bool ShutdownService(bool bForce = false) override;
    virtual ESuspenseCoreServiceLifecycleState GetServiceState() const override;
    virtual bool IsServiceReady() const override;
    virtual FGameplayTag GetServiceTag() const override;
    virtual FGameplayTagContainer GetRequiredDependencies() const override;
    virtual bool ValidateService(TArray<FText>& OutErrors) const override;
    virtual void ResetService() override;
    virtual FString GetServiceStats() const override;
    //~ End ISuspenseCoreEquipmentService Interface

    //~ Begin ISuspenseActorFactory Interface
    virtual FEquipmentActorSpawnResult SpawnEquipmentActor(const FEquipmentActorSpawnParams& Params) override;
    virtual bool DestroyEquipmentActor(AActor* Actor, bool bImmediate = false) override;
    virtual bool ConfigureEquipmentActor(AActor* Actor, const FSuspenseCoreInventoryItemInstance& ItemInstance) override;
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
    void SetFactoryConfiguration(const FSuspenseCoreActorFactoryConfig& NewConfig);

    /** Get pool statistics */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Factory")
    void GetPoolStatistics(int32& TotalActors, int32& ActiveActors, int32& PooledActors) const;

    /** Preload multiple item classes */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Factory")
    void PreloadItemClasses(const TArray<FName>& ItemIds);

protected:
    /** Configuration */
    UPROPERTY(EditAnywhere, Category = "Factory Config")
    FSuspenseCoreActorFactoryConfig FactoryConfig;

    /** Actor pool */
    UPROPERTY()
    TArray<FSuspenseCoreActorPoolEntry> ActorPool;

    /** Registry of spawned actors by slot */
    UPROPERTY()
    TMap<int32, AActor*> SpawnedActorRegistry;

    /** Cache manager for actor classes */
    FSuspenseCoreEquipmentCacheManager<FName, TSubclassOf<AActor>> ActorClassCache;

    /** Streamable manager for async loading */
    FStreamableManager StreamableManager;

    /** Async loading handles */
    TMap<FName, TSharedPtr<struct FStreamableHandle>> LoadingHandles;

    /** Thread guards for thread-safe operations */
    mutable FCriticalSection PoolLock;
    mutable FCriticalSection RegistryLock;

    /** Pool cleanup timer handle */
    FTimerHandle PoolCleanupTimerHandle;

    // ---- EventBus Integration (SuspenseCore architecture) ----

    /** EventBus for decoupled inter-component communication */
    UPROPERTY(Transient)
    TObjectPtr<USuspenseCoreEventBus> EventBus = nullptr;

    /** Event tags using native compile-time tags */
    FGameplayTag Tag_Visual_Spawned;
    FGameplayTag Tag_Visual_Destroyed;

    /** Service lifecycle state for ISuspenseCoreEquipmentService */
    ESuspenseCoreServiceLifecycleState ServiceState = ESuspenseCoreServiceLifecycleState::Uninitialized;

    /** Cached service tag */
    FGameplayTag CachedServiceTag;

private:
    /** Internal spawn logic */
    AActor* SpawnActorInternal(TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform, AActor* Owner);

    // ---- EventBus Helpers ----

    /** Initialize EventBus connection */
    void SetupEventBus();

    /** Broadcast actor spawned event with full data for AbilityService */
    void BroadcastActorSpawned(AActor* SpawnedActor, AActor* OwnerActor, const FSuspenseCoreInventoryItemInstance& ItemInstance, int32 SlotIndex);

    /** Broadcast actor destroyed event */
    void BroadcastActorDestroyed(AActor* Actor, const FName& ItemId);

    /** Internal destroy logic */
    void DestroyActorInternal(AActor* Actor, bool bImmediate);

    /** Get actor class for item */
    TSubclassOf<AActor> GetActorClassForItem(const FName& ItemId);

    /** Find pool entry for actor */
    FSuspenseCoreActorPoolEntry* FindPoolEntry(AActor* Actor);

    /** Find available pool entry for class */
    FSuspenseCoreActorPoolEntry* FindAvailablePoolEntry(TSubclassOf<AActor> ActorClass);

    /** Create new pool entry */
    FSuspenseCoreActorPoolEntry* CreatePoolEntry(TSubclassOf<AActor> ActorClass);

    /** Clean up expired pool entries */
    void CleanupPool();

    /** Async load complete callback */
    void OnAsyncLoadComplete(FName ItemId);

    /** Validate actor for operations */
    bool IsActorValid(AActor* Actor) const;

    /** Log factory operation */
    void LogFactoryOperation(const FString& Operation, const FString& Details) const;
};
