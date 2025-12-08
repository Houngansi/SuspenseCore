// SuspenseCoreServiceProvider.h
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "SuspenseCoreServiceProvider.generated.h"

class USuspenseCoreEventBus;
class USuspenseCoreEventManager;
class USuspenseCoreDataManager;

DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseCoreServiceProvider, Log, All);

/**
 * USuspenseCoreServiceProvider
 *
 * Central access point for all SuspenseCore services.
 * GameInstanceSubsystem ensures proper lifecycle management.
 *
 * DESIGN PRINCIPLES:
 * ═══════════════════════════════════════════════════════════════════════════
 * - Single point of service resolution (Dependency Injection hub)
 * - Automatic registration of core services on initialization
 * - Support for interface-based lookup for testability
 * - Lazy initialization where appropriate
 * - Thread-safe access via ServiceLocator
 *
 * LIFECYCLE:
 * ═══════════════════════════════════════════════════════════════════════════
 * 1. GameInstance creates subsystem
 * 2. Initialize() ensures EventManager and DataManager are ready
 * 3. Registers all core services in ServiceLocator
 * 4. Broadcasts SuspenseCore.Event.Services.Initialized
 * 5. Components access services via Get() static method
 * 6. Deinitialize() cleans up on shutdown
 *
 * USAGE:
 * ═══════════════════════════════════════════════════════════════════════════
 * C++:
 *   // Get provider
 *   auto* Provider = USuspenseCoreServiceProvider::Get(this);
 *
 *   // Get typed service (preferred for core services)
 *   auto* EventBus = Provider->GetEventBus();
 *   auto* DataManager = Provider->GetDataManager();
 *
 *   // Generic access for custom services
 *   auto* MyService = Provider->GetService<UMyCustomService>();
 *
 * Blueprint:
 *   Use "Get Service Provider" node, then call GetEventBus, etc.
 *
 * @see USuspenseCoreServiceLocator - Internal service registry
 * @see USuspenseCoreEventManager - EventBus provider
 * @see USuspenseCoreDataManager - Data access
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreServiceProvider : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════════════════════════════════
	// STATIC ACCESS
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Get ServiceProvider from world context.
	 * This is the PRIMARY entry point for service access.
	 *
	 * @param WorldContextObject Any object with valid world context
	 * @return ServiceProvider or nullptr if not available
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Services",
		meta = (WorldContext = "WorldContextObject", DisplayName = "Get Service Provider"))
	static USuspenseCoreServiceProvider* Get(const UObject* WorldContextObject);

	// ═══════════════════════════════════════════════════════════════════════════
	// USUBSYSTEM INTERFACE
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// ═══════════════════════════════════════════════════════════════════════════
	// CORE SERVICE ACCESSORS (Typed, cached for performance)
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Get EventBus for event publishing/subscribing.
	 * Always available after initialization.
	 * @return EventBus instance
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Services")
	USuspenseCoreEventBus* GetEventBus() const;

	/**
	 * Get DataManager for item/character data access.
	 * Always available after initialization.
	 * @return DataManager instance
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Services")
	USuspenseCoreDataManager* GetDataManager() const;

	/**
	 * Get EventManager (owns EventBus and ServiceLocator).
	 * @return EventManager instance
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Services")
	USuspenseCoreEventManager* GetEventManager() const;

	/**
	 * Get ServiceLocator for custom service access.
	 * @return ServiceLocator instance
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Services")
	USuspenseCoreServiceLocator* GetServiceLocator() const;

	// ═══════════════════════════════════════════════════════════════════════════
	// GENERIC SERVICE ACCESS (C++ Templates)
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Get service by class type.
	 * @tparam T Service class type (must derive from UObject)
	 * @return Service instance or nullptr if not registered
	 */
	template<typename T>
	T* GetService() const
	{
		if (CachedServiceLocator.IsValid())
		{
			return CachedServiceLocator->template GetService<T>();
		}
		return nullptr;
	}

	/**
	 * Get service by interface name.
	 * Useful for interface-based access and mocking.
	 * @tparam T Expected return type
	 * @param InterfaceName Name of the interface/service
	 * @return Service instance or nullptr
	 */
	template<typename T>
	T* GetServiceAs(FName InterfaceName) const
	{
		if (CachedServiceLocator.IsValid())
		{
			return CachedServiceLocator->template GetServiceAs<T>(InterfaceName);
		}
		return nullptr;
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// SERVICE REGISTRATION (For modules to register custom services)
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Register a service by class type.
	 * @tparam T Service class type
	 * @param ServiceInstance Instance to register
	 */
	template<typename T>
	void RegisterService(T* ServiceInstance)
	{
		if (CachedServiceLocator.IsValid())
		{
			CachedServiceLocator->template RegisterService<T>(ServiceInstance);
			BroadcastServiceRegistered(T::StaticClass()->GetFName());
		}
	}

	/**
	 * Register service by name (Blueprint callable).
	 * @param ServiceName Name to register under
	 * @param ServiceInstance Instance to register
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Services")
	void RegisterServiceByName(FName ServiceName, UObject* ServiceInstance);

	/**
	 * Unregister a service by name.
	 * @param ServiceName Name of service to unregister
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Services")
	void UnregisterService(FName ServiceName);

	/**
	 * Check if a service is registered.
	 * @param ServiceName Name to check
	 * @return true if service is registered
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Services")
	bool HasService(FName ServiceName) const;

	// ═══════════════════════════════════════════════════════════════════════════
	// INITIALIZATION STATUS
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Check if provider is fully initialized and all core services available.
	 * @return true if ready for use
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Services")
	bool IsInitialized() const { return bIsInitialized; }

	/**
	 * Get list of all registered service names (for debugging).
	 * @return Array of service names
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Services|Debug")
	TArray<FName> GetRegisteredServiceNames() const;

	/**
	 * Get count of registered services.
	 * @return Number of services
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Services|Debug")
	int32 GetServiceCount() const;

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// INITIALIZATION HELPERS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Cache references to dependent subsystems */
	void CacheSubsystemReferences(FSubsystemCollectionBase& Collection);

	/** Register core SuspenseCore services in ServiceLocator */
	void RegisterCoreServices();

	/** Broadcast initialization complete event */
	void BroadcastInitialized();

	/** Broadcast service registered event */
	void BroadcastServiceRegistered(FName ServiceName);

	/** Broadcast service unregistered event */
	void BroadcastServiceUnregistered(FName ServiceName);

private:
	// ═══════════════════════════════════════════════════════════════════════════
	// CACHED REFERENCES
	// ═══════════════════════════════════════════════════════════════════════════

	/** Cached EventManager reference */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventManager> CachedEventManager;

	/** Cached DataManager reference */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreDataManager> CachedDataManager;

	/** Cached EventBus reference (from EventManager) */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** Cached ServiceLocator reference (from EventManager) */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreServiceLocator> CachedServiceLocator;

	// ═══════════════════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════════════════

	/** Initialization flag */
	bool bIsInitialized = false;
};

