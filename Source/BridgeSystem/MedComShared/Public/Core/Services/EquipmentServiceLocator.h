// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "HAL/CriticalSection.h"
#include "Interfaces/Equipment/IEquipmentService.h"
#include "EquipmentServiceLocator.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogServiceLocator, Log, All);

/**
 * Dependency injection callback signature
 * Called BEFORE InitializeService() to inject service dependencies
 */
DECLARE_DELEGATE_TwoParams(FServiceInjectionDelegate, UObject* /*ServiceInstance*/, UEquipmentServiceLocator* /*Locator*/);

/**
 * Single registration record in the service registry.
 */
USTRUCT()
struct FServiceRegistration
{
    GENERATED_BODY()

    /** Stable identifier of the service (provided by IEquipmentService::GetServiceTag on the instance). */
    UPROPERTY() FGameplayTag ServiceTag;

    /** Current service instance (UObject that implements UEquipmentService). */
    UPROPERTY() UObject* ServiceInstance = nullptr;

    /** Class used to create the instance if Factory is not set. */
    UPROPERTY() TSubclassOf<UObject> ServiceClass = nullptr;

    /** Initialization params to be passed to InitializeService(). */
    UPROPERTY() FServiceInitParams InitParams;

    /** Current lifecycle. */
    UPROPERTY() EServiceLifecycleState State = EServiceLifecycleState::Uninitialized;

    /** Registration telemetry. */
    UPROPERTY() float RegistrationTime = 0.f;

    /** External references holder (for opt-in cleanup). */
    UPROPERTY() int32 ReferenceCount = 0;

    /** Optional factory (takes Outer), overrides ServiceClass if set. */
    TFunction<UObject*(UObject* /*Outer*/)> Factory;
    
    /** Dependency injection callback - called BEFORE InitializeService() */
    FServiceInjectionDelegate InjectionCallback;

    /** Per-service lock. */
    TSharedPtr<FCriticalSection> ServiceLock;

    FServiceRegistration()
    {
        ServiceLock = MakeShareable(new FCriticalSection());
    }
};

/**
 * Centralized Service Locator (Shared module).
 * - No compile-time references to gameplay/eqp implementations.
 * - Works purely via IEquipmentService and gameplay tags.
 * - Supports dependency injection via callbacks before initialization
 */
UCLASS()
class MEDCOMSHARED_API UEquipmentServiceLocator : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    //~ UGameInstanceSubsystem
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

    /** Accessor. */
    static UEquipmentServiceLocator* Get(const UObject* WorldContext);

    // -------- Registration API (interfaces only) --------

    /** Register service by class (instance will be lazily created). */
    UFUNCTION(BlueprintCallable, Category="ServiceLocator")
    bool RegisterServiceClass(const FGameplayTag& ServiceTag,
                              TSubclassOf<UObject> ServiceClass,
                              const FServiceInitParams& InitParams = FServiceInitParams());

    /** Register service with dependency injection callback */
    bool RegisterServiceClassWithInjection(const FGameplayTag& ServiceTag,
                                          TSubclassOf<UObject> ServiceClass,
                                          const FServiceInitParams& InitParams,
                                          const FServiceInjectionDelegate& InjectionCallback);

    /** Register already created instance. */
    UFUNCTION(BlueprintCallable, Category="ServiceLocator")
    bool RegisterServiceInstance(const FGameplayTag& ServiceTag,
                                 UObject* ServiceInstance,
                                 const FServiceInitParams& InitParams = FServiceInitParams());

    /** Register via factory (overrides ServiceClass). */
    bool RegisterServiceFactory(const FGameplayTag& ServiceTag,
                                TFunction<UObject*(UObject* /*Outer*/)> Factory,
                                const FServiceInitParams& InitParams = FServiceInitParams());

    /** Unregister and shutdown (optional). */
    UFUNCTION(BlueprintCallable, Category="ServiceLocator")
    bool UnregisterService(const FGameplayTag& ServiceTag, bool bForceShutdown = false);

    // -------- Access API --------

    /** Resolve service (lazy create + inject + initialize). */
    UFUNCTION(BlueprintCallable, Category="ServiceLocator")
    UObject* GetService(const FGameplayTag& ServiceTag);

    template<typename IInterface>
    IInterface* GetServiceAs(const FGameplayTag& ServiceTag)
    {
        UObject* Obj = GetService(ServiceTag);
        if (!Obj) return nullptr;
        if (!Obj->GetClass()->ImplementsInterface(IInterface::UClassType::StaticClass()))
        {
            return nullptr;
        }
        return static_cast<IInterface*>(Obj->GetInterfaceAddress(IInterface::UClassType::StaticClass()));
    }

    /** Peek service if ready (no lazy work). */
    UFUNCTION(BlueprintCallable, Category="ServiceLocator")
    UObject* TryGetService(const FGameplayTag& ServiceTag) const;

    UFUNCTION(BlueprintCallable, Category="ServiceLocator", BlueprintPure)
    bool IsServiceRegistered(const FGameplayTag& ServiceTag) const;

    UFUNCTION(BlueprintCallable, Category="ServiceLocator", BlueprintPure)
    bool IsServiceReady(const FGameplayTag& ServiceTag) const;

    // -------- Lifecycle & deps --------

    /** Initialize all currently registered but uninitialized services (topo-sorted by declared deps). */
    UFUNCTION(BlueprintCallable, Category="ServiceLocator")
    int32 InitializeAllServices();

    /** Shutdown all ready services (reverse topo). */
    UFUNCTION(BlueprintCallable, Category="ServiceLocator")
    int32 ShutdownAllServices(bool bForce = false);

    /** Reset all to Uninitialized (without unregister). */
    UFUNCTION(BlueprintCallable, Category="ServiceLocator")
    void ResetAllServices();

    /** Current service state. */
    UFUNCTION(BlueprintCallable, Category="ServiceLocator", BlueprintPure)
    EServiceLifecycleState GetServiceState(const FGameplayTag& ServiceTag) const;

    /** Build a textual dependency graph (for logs). */
    UFUNCTION(BlueprintCallable, Category="ServiceLocator|Debug")
    FString BuildDependencyGraph() const;

    /** Validate registered services (state, deps, instances). */
    UFUNCTION(BlueprintCallable, Category="ServiceLocator|Debug")
    bool ValidateAllServices(TArray<FText>& OutErrors);

    /** List of registered tags. */
    UFUNCTION(BlueprintCallable, Category="ServiceLocator|Debug")
    TArray<FGameplayTag> GetRegisteredServices() const;
    
    /**
     * Get all registered service tags (for universal rebind without hardcoded lists)
     * Used by SystemCoordinatorSubsystem to iterate all services for RebindWorld
     * @return Array of all registered service tags
     */
    UFUNCTION(BlueprintPure, Category="ServiceLocator|Debug")
    TArray<FGameplayTag> GetAllRegisteredServiceTags() const;

    /** Verbose logging switch. */
    UFUNCTION(BlueprintCallable, Category="ServiceLocator|Debug")
    void SetDetailedLogging(bool bEnable) { bDetailedLogging = bEnable; }

protected:
    // Internal helpers
    bool InitializeService(FServiceRegistration& Reg);
    bool ShutdownService(FServiceRegistration& Reg, bool bForce);
    UObject* CreateServiceInstance(const FServiceRegistration& Reg);
    bool InjectServiceDependencies(FServiceRegistration& Reg);

    FServiceRegistration* FindReg(const FGameplayTag& Tag);
    const FServiceRegistration* FindReg(const FGameplayTag& Tag) const;

    FGameplayTagContainer GetRequiredDeps_NoLock(const FServiceRegistration& Reg) const;

    TArray<FGameplayTag> TopoSort(const TArray<FGameplayTag>& Services) const;
    bool HasCircular(const FGameplayTag& Tag, TSet<FGameplayTag>& Visited) const;

    void PerformAutomaticCleanup();
    int32 CleanupUnusedServices();
    bool ValidateServiceInstance(const UObject* ServiceInstance) const;

private:
    // Registry
    UPROPERTY() TMap<FGameplayTag, FServiceRegistration> Registry;

    // Concurrency
    mutable FCriticalSection RegistryLock;

    // Config
    UPROPERTY() bool  bDetailedLogging = false;
    UPROPERTY() float CleanupInterval  = 60.f;

    // Stats
    mutable int32 TotalCreated = 0;
    mutable int32 TotalInited  = 0;
    mutable int32 TotalFailed  = 0;
    mutable float SumInitTime  = 0.f;

    // Init guards
    TSet<FGameplayTag> Initializing;
    TSet<FGameplayTag> ReadySet;

    // Timer
    FTimerHandle CleanupTimer;

    // Depth guard
    static constexpr int32 MaxDepDepth = 16;
};