// SuspenseCoreEquipmentServiceLocator.h
// SuspenseCore - EventBus Architecture
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "HAL/CriticalSection.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentService.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreEquipmentServiceLocator.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseCoreServiceLocator, Log, All);

// Forward declarations
class USuspenseCoreEventBus;
class USuspenseCoreEventManager;

/**
 * Dependency injection callback signature
 * Called BEFORE InitializeService() to inject service dependencies
 */
DECLARE_DELEGATE_TwoParams(FSuspenseCoreServiceInjectionDelegate, UObject* /*ServiceInstance*/, class USuspenseCoreEquipmentServiceLocator* /*Locator*/);

/**
 * FSuspenseCoreServiceRegistration
 *
 * Single registration record in the service registry.
 * Uses SuspenseCore types and EventBus architecture.
 */
USTRUCT()
struct BRIDGESYSTEM_API FSuspenseCoreServiceRegistration
{
	GENERATED_BODY()

	/** Stable identifier of the service (GameplayTag) */
	UPROPERTY()
	FGameplayTag ServiceTag;

	/** Current service instance (UObject that implements ISuspenseCoreEquipmentService) */
	UPROPERTY()
	TObjectPtr<UObject> ServiceInstance = nullptr;

	/** Class used to create the instance if Factory is not set */
	UPROPERTY()
	TSubclassOf<UObject> ServiceClass = nullptr;

	/** Initialization params to be passed to InitializeService() */
	UPROPERTY()
	FSuspenseCoreServiceInitParams InitParams;

	/** Current lifecycle state */
	UPROPERTY()
	ESuspenseCoreServiceLifecycleState State = ESuspenseCoreServiceLifecycleState::Uninitialized;

	/** Registration telemetry */
	UPROPERTY()
	float RegistrationTime = 0.f;

	/** External references holder (for opt-in cleanup) */
	UPROPERTY()
	int32 ReferenceCount = 0;

	/** Optional factory (takes Outer), overrides ServiceClass if set */
	TFunction<UObject*(UObject* /*Outer*/)> Factory;

	/** Dependency injection callback - called BEFORE InitializeService() */
	FSuspenseCoreServiceInjectionDelegate InjectionCallback;

	/** Per-service lock */
	TSharedPtr<FCriticalSection> ServiceLock;

	FSuspenseCoreServiceRegistration()
	{
		ServiceLock = MakeShareable(new FCriticalSection());
	}
};

/**
 * USuspenseCoreEquipmentServiceLocator
 *
 * Centralized Service Locator for Equipment System using SuspenseCore EventBus architecture.
 *
 * KEY FEATURES:
 * - Works via ISuspenseCoreEquipmentService and GameplayTags
 * - Broadcasts lifecycle events via EventBus:
 *   - SuspenseCore.Service.Registered
 *   - SuspenseCore.Service.Initialized
 *   - SuspenseCore.Service.Ready
 *   - SuspenseCore.Service.ShuttingDown
 *   - SuspenseCore.Service.Shutdown
 *   - SuspenseCore.Service.Failed
 * - Supports dependency injection via callbacks
 * - Thread-safe operations
 * - Topological sorting for dependency initialization
 *
 * This is NEW architecture - replaces legacy SuspenseEquipmentServiceLocator.
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreEquipmentServiceLocator : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ UGameInstanceSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	/** Static accessor */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|ServiceLocator", meta = (WorldContext = "WorldContext"))
	static USuspenseCoreEquipmentServiceLocator* Get(const UObject* WorldContext);

	// ═══════════════════════════════════════════════════════════════════════════
	// REGISTRATION API
	// ═══════════════════════════════════════════════════════════════════════════

	/** Register service by class (instance will be lazily created) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|ServiceLocator")
	bool RegisterServiceClass(
		const FGameplayTag& ServiceTag,
		TSubclassOf<UObject> ServiceClass,
		const FSuspenseCoreServiceInitParams& InitParams = FSuspenseCoreServiceInitParams());

	/** Register service with dependency injection callback */
	bool RegisterServiceClassWithInjection(
		const FGameplayTag& ServiceTag,
		TSubclassOf<UObject> ServiceClass,
		const FSuspenseCoreServiceInitParams& InitParams,
		const FSuspenseCoreServiceInjectionDelegate& InjectionCallback);

	/** Register already created instance */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|ServiceLocator")
	bool RegisterServiceInstance(
		const FGameplayTag& ServiceTag,
		UObject* ServiceInstance,
		const FSuspenseCoreServiceInitParams& InitParams = FSuspenseCoreServiceInitParams());

	/** Register via factory (overrides ServiceClass) */
	bool RegisterServiceFactory(
		const FGameplayTag& ServiceTag,
		TFunction<UObject*(UObject* /*Outer*/)> Factory,
		const FSuspenseCoreServiceInitParams& InitParams = FSuspenseCoreServiceInitParams());

	/** Unregister and shutdown (optional) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|ServiceLocator")
	bool UnregisterService(const FGameplayTag& ServiceTag, bool bForceShutdown = false);

	// ═══════════════════════════════════════════════════════════════════════════
	// ACCESS API
	// ═══════════════════════════════════════════════════════════════════════════

	/** Resolve service (lazy create + inject + initialize) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|ServiceLocator")
	UObject* GetService(const FGameplayTag& ServiceTag);

	/** Template accessor for C++ - returns interface pointer */
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

	/** Peek service if ready (no lazy work) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|ServiceLocator")
	UObject* TryGetService(const FGameplayTag& ServiceTag) const;

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|ServiceLocator", BlueprintPure)
	bool IsServiceRegistered(const FGameplayTag& ServiceTag) const;

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|ServiceLocator", BlueprintPure)
	bool IsServiceReady(const FGameplayTag& ServiceTag) const;

	// ═══════════════════════════════════════════════════════════════════════════
	// LIFECYCLE MANAGEMENT
	// ═══════════════════════════════════════════════════════════════════════════

	/** Initialize all currently registered but uninitialized services (topo-sorted by declared deps) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|ServiceLocator")
	int32 InitializeAllServices();

	/** Shutdown all ready services (reverse topo) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|ServiceLocator")
	int32 ShutdownAllServices(bool bForce = false);

	/** Reset all to Uninitialized (without unregister) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|ServiceLocator")
	void ResetAllServices();

	/** Current service state */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|ServiceLocator", BlueprintPure)
	ESuspenseCoreServiceLifecycleState GetServiceState(const FGameplayTag& ServiceTag) const;

	// ═══════════════════════════════════════════════════════════════════════════
	// DEBUG & VALIDATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Build a textual dependency graph (for logs) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|ServiceLocator|Debug")
	FString BuildDependencyGraph() const;

	/** Validate registered services (state, deps, instances) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|ServiceLocator|Debug")
	bool ValidateAllServices(TArray<FText>& OutErrors);

	/** List of registered tags */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|ServiceLocator|Debug")
	TArray<FGameplayTag> GetRegisteredServices() const;

	/** Get all registered service tags (for universal rebind) */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|ServiceLocator|Debug")
	TArray<FGameplayTag> GetAllRegisteredServiceTags() const;

	/** Verbose logging switch */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|ServiceLocator|Debug")
	void SetDetailedLogging(bool bEnable) { bDetailedLogging = bEnable; }

	// ═══════════════════════════════════════════════════════════════════════════
	// EVENTBUS INTEGRATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Get EventBus for service events */
	USuspenseCoreEventBus* GetEventBus() const;

	/** Subscribe to service lifecycle events */
	FSuspenseCoreSubscriptionHandle SubscribeToServiceEvents(
		const FGameplayTag& ServiceTag,
		const FSuspenseCoreNativeEventCallback& Callback);

protected:
	// Internal helpers
	bool InitializeService(FSuspenseCoreServiceRegistration& Reg);
	bool ShutdownService(FSuspenseCoreServiceRegistration& Reg, bool bForce);
	UObject* CreateServiceInstance(const FSuspenseCoreServiceRegistration& Reg);
	bool InjectServiceDependencies(FSuspenseCoreServiceRegistration& Reg);

	FSuspenseCoreServiceRegistration* FindReg(const FGameplayTag& Tag);
	const FSuspenseCoreServiceRegistration* FindReg(const FGameplayTag& Tag) const;

	FGameplayTagContainer GetRequiredDeps_NoLock(const FSuspenseCoreServiceRegistration& Reg) const;

	TArray<FGameplayTag> TopoSort(const TArray<FGameplayTag>& Services) const;
	bool HasCircular(const FGameplayTag& Tag, TSet<FGameplayTag>& Visited) const;

	void PerformAutomaticCleanup();
	int32 CleanupUnusedServices();
	bool ValidateServiceInstance(const UObject* ServiceInstance) const;

	// EventBus notification helpers
	void BroadcastServiceEvent(const FGameplayTag& EventTag, const FGameplayTag& ServiceTag, ESuspenseCoreServiceLifecycleState State);

private:
	// Registry
	UPROPERTY()
	TMap<FGameplayTag, FSuspenseCoreServiceRegistration> Registry;

	// Cached EventManager reference
	TWeakObjectPtr<USuspenseCoreEventManager> CachedEventManager;

	// Concurrency
	mutable FCriticalSection RegistryLock;

	// Config
	UPROPERTY()
	bool bDetailedLogging = false;

	UPROPERTY()
	float CleanupInterval = 60.f;

	// Stats
	mutable int32 TotalCreated = 0;
	mutable int32 TotalInited = 0;
	mutable int32 TotalFailed = 0;
	mutable float SumInitTime = 0.f;

	// Init guards
	TSet<FGameplayTag> Initializing;
	TSet<FGameplayTag> ReadySet;

	// Timer
	FTimerHandle CleanupTimer;

	// Depth guard
	static constexpr int32 MaxDepDepth = 16;
};
