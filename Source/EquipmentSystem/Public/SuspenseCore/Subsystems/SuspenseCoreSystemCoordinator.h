// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreSystemCoordinator.generated.h"

class USuspenseCoreServiceLocator;
class USuspenseCoreEventBus;
class USuspenseCoreEventManager;

/**
 * USuspenseCoreSystemCoordinator
 *
 * GameInstance-level subsystem that coordinates all SuspenseCore services and components.
 * Provides centralized service management, lifecycle control, and dependency injection.
 *
 * ARCHITECTURE:
 * - Owns and manages service lifecycle
 * - Integrates with EventBus for system-wide event communication
 * - Provides ServiceLocator functionality for dependency injection
 * - Survives seamless/non-seamless map transitions
 * - Ensures proper initialization order and cleanup
 *
 * LIFECYCLE:
 * 1. Initialize() - Create subsystems, register core services
 * 2. RegisterService<T>() - Register services during initialization
 * 3. GetService<T>() - Retrieve services for use
 * 4. Deinitialize() - Clean shutdown, release resources
 *
 * THREAD SAFETY:
 * All methods are GameThread-only (checked via check(IsInGameThread()))
 *
 * USAGE:
 *   auto* Coordinator = USuspenseCoreSystemCoordinator::Get(WorldContext);
 *   Coordinator->RegisterService<UMyService>(MyServiceInstance);
 *   auto* Service = Coordinator->GetService<UMyService>();
 *   Coordinator->BroadcastSystemEvent(EventTag, EventData);
 */
UCLASS(DisplayName="SuspenseCore System Coordinator", meta=(Comment="Coordinates all SuspenseCore services and manages their lifecycle"))
class EQUIPMENTSYSTEM_API USuspenseCoreSystemCoordinator : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	USuspenseCoreSystemCoordinator();

	//========================================
	// USubsystem Interface
	//========================================

	/** Called to determine if subsystem should be created for this outer */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Initialize subsystem - create service locator, register services */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Cleanup subsystem - shutdown services, release resources */
	virtual void Deinitialize() override;

	//========================================
	// Static Accessor
	//========================================

	/**
	 * Get coordinator instance from world context
	 * @param WorldContextObject - World context object
	 * @return Coordinator instance or nullptr if not available
	 */
	UFUNCTION(BlueprintCallable, Category="SuspenseCore|System", meta=(WorldContext="WorldContextObject"))
	static USuspenseCoreSystemCoordinator* Get(const UObject* WorldContextObject);

	//========================================
	// Service Management API
	//========================================

	/**
	 * Register a service instance (C++ template version)
	 * @param ServiceInstance - Service instance to register
	 */
	template<typename T>
	void RegisterService(T* ServiceInstance)
	{
		static_assert(TIsDerivedFrom<T, UObject>::Value, "Service must derive from UObject");
		check(IsInGameThread());

		if (ServiceLocator)
		{
			ServiceLocator->RegisterService<T>(ServiceInstance);
			UE_LOG(LogTemp, Verbose, TEXT("[SuspenseCoreSystemCoordinator] Registered service: %s"),
				*T::StaticClass()->GetName());
		}
	}

	/**
	 * Register a service by name (Blueprint version)
	 * @param ServiceName - Name to register service under
	 * @param ServiceInstance - Service instance to register
	 */
	UFUNCTION(BlueprintCallable, Category="SuspenseCore|Services")
	void RegisterServiceByName(FName ServiceName, UObject* ServiceInstance);

	/**
	 * Unregister a service (C++ template version)
	 */
	template<typename T>
	void UnregisterService()
	{
		check(IsInGameThread());

		if (ServiceLocator)
		{
			ServiceLocator->UnregisterService(T::StaticClass()->GetFName());
			UE_LOG(LogTemp, Verbose, TEXT("[SuspenseCoreSystemCoordinator] Unregistered service: %s"),
				*T::StaticClass()->GetName());
		}
	}

	/**
	 * Unregister a service by name (Blueprint version)
	 * @param ServiceName - Name of service to unregister
	 */
	UFUNCTION(BlueprintCallable, Category="SuspenseCore|Services")
	void UnregisterServiceByName(FName ServiceName);

	/**
	 * Get a service instance (C++ template version)
	 * @return Service instance or nullptr if not registered
	 */
	template<typename T>
	T* GetService() const
	{
		check(IsInGameThread());

		if (ServiceLocator)
		{
			return ServiceLocator->GetService<T>();
		}

		return nullptr;
	}

	/**
	 * Get a service by name (Blueprint version)
	 * @param ServiceName - Name of service to retrieve
	 * @return Service instance or nullptr if not registered
	 */
	UFUNCTION(BlueprintCallable, Category="SuspenseCore|Services")
	UObject* GetServiceByName(FName ServiceName) const;

	/**
	 * Check if a service is registered
	 * @param ServiceName - Name of service to check
	 * @return true if service is registered
	 */
	UFUNCTION(BlueprintCallable, Category="SuspenseCore|Services")
	bool HasService(FName ServiceName) const;

	//========================================
	// EventBus Integration
	//========================================

	/**
	 * Get the EventBus for publishing/subscribing to system events
	 * @return EventBus instance or nullptr if not initialized
	 */
	UFUNCTION(BlueprintCallable, Category="SuspenseCore|Events")
	USuspenseCoreEventBus* GetEventBus() const;

	/**
	 * Broadcast a system event through the EventBus
	 * @param EventTag - Gameplay tag identifying the event
	 * @param Source - Source object that triggered the event
	 */
	UFUNCTION(BlueprintCallable, Category="SuspenseCore|Events")
	void BroadcastSystemEvent(FGameplayTag EventTag, UObject* Source);

	/**
	 * Subscribe to a system event
	 * @param EventTag - Gameplay tag identifying the event
	 * @param Subscriber - Object subscribing to the event
	 * @param Callback - Callback to invoke when event is published
	 */
	UFUNCTION(BlueprintCallable, Category="SuspenseCore|Events")
	void SubscribeToSystemEvent(FGameplayTag EventTag, UObject* Subscriber);

	//========================================
	// Service Locator Access
	//========================================

	/**
	 * Get the ServiceLocator instance (read-only)
	 * @return ServiceLocator or nullptr if not initialized
	 */
	UFUNCTION(BlueprintPure, Category="SuspenseCore|Services")
	USuspenseCoreServiceLocator* GetServiceLocator() const { return ServiceLocator; }

	//========================================
	// Status API
	//========================================

	/**
	 * Check if coordinator is fully initialized and ready
	 * @return true if all systems are initialized and operational
	 */
	UFUNCTION(BlueprintPure, Category="SuspenseCore|System")
	bool IsReady() const { return bIsInitialized; }

	/**
	 * Get count of registered services
	 * @return Number of registered services
	 */
	UFUNCTION(BlueprintCallable, Category="SuspenseCore|Services")
	int32 GetServiceCount() const;

	/**
	 * Get list of all registered service names
	 * @return Array of service names
	 */
	UFUNCTION(BlueprintCallable, Category="SuspenseCore|Services|Debug")
	TArray<FName> GetRegisteredServiceNames() const;

	//========================================
	// Debug Commands
	//========================================

	/**
	 * Dump current system state to log
	 * Usage: ~ DebugDumpSystemState
	 */
	UFUNCTION(Exec, Category="SuspenseCore|Debug")
	void DebugDumpSystemState();

	/**
	 * Dump all registered services to log
	 * Usage: ~ DebugDumpServices
	 */
	UFUNCTION(Exec, Category="SuspenseCore|Debug")
	void DebugDumpServices();

protected:
	//========================================
	// Internal Methods
	//========================================

	/**
	 * Create core subsystems (ServiceLocator, EventBus integration)
	 */
	void CreateSubsystems();

	/**
	 * Register core SuspenseCore services
	 * Override in subclasses to add custom services
	 */
	virtual void RegisterCoreServices();

	/**
	 * Initialize services after registration
	 * Calls initialization methods on registered services if they implement ISuspenseCoreService
	 */
	void InitializeServices();

	/**
	 * Shutdown all services before cleanup
	 */
	void ShutdownServices();

	/**
	 * Validate that all required services are registered
	 * @param OutErrors - Array to receive validation error messages
	 * @return true if all required services are present
	 */
	bool ValidateServices(TArray<FText>& OutErrors) const;

	/**
	 * Get EventManager from GameInstance
	 * @return EventManager instance or nullptr
	 */
	USuspenseCoreEventManager* GetEventManager() const;

private:
	//========================================
	// Owned Objects
	//========================================

	/**
	 * ServiceLocator instance - registry of all services
	 * Created with GameInstance as outer for persistence
	 */
	UPROPERTY(Transient)
	TObjectPtr<USuspenseCoreServiceLocator> ServiceLocator = nullptr;

	/**
	 * Cached reference to EventManager
	 * Retrieved from GameInstance subsystems
	 */
	UPROPERTY(Transient)
	TWeakObjectPtr<USuspenseCoreEventManager> CachedEventManager = nullptr;

	//========================================
	// State Flags
	//========================================

	/** Coordinator has been fully initialized */
	bool bIsInitialized = false;

	/** Services have been registered */
	bool bServicesRegistered = false;

	/** Services have been initialized */
	bool bServicesInitialized = false;
};
