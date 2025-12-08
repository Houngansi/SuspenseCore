// SuspenseCoreServiceInterfaces.h
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreServiceInterfaces.generated.h"

// Forward declarations
struct FSuspenseCoreEventData;
struct FSuspenseCoreItemData;
class USuspenseCoreServiceProvider;
class USuspenseCoreEventBus;

// ═══════════════════════════════════════════════════════════════════════════════
// SERVICE CONSUMER INTERFACE
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * USuspenseCoreServiceConsumer
 *
 * Interface for objects that consume services and need dependency injection.
 * Implement this to receive automatic service injection and validation.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreServiceConsumer : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreServiceConsumer
 *
 * Interface for components/actors that require services.
 *
 * USAGE:
 * ═══════════════════════════════════════════════════════════════════════════
 * 1. Implement this interface on your component
 * 2. Override GetRequiredServices() to declare dependencies
 * 3. Override OnServicesInjected() to cache service references
 * 4. In BeginPlay, call ValidateAndInjectServices()
 *
 * EXAMPLE:
 * ═══════════════════════════════════════════════════════════════════════════
 *   class UMyComponent : public UActorComponent, public ISuspenseCoreServiceConsumer
 *   {
 *       virtual TArray<FName> GetRequiredServices() const override
 *       {
 *           return { USuspenseCoreEventBus::StaticClass()->GetFName() };
 *       }
 *
 *       virtual void OnServicesInjected(USuspenseCoreServiceProvider* Provider) override
 *       {
 *           CachedEventBus = Provider->GetEventBus();
 *       }
 *   };
 */
class BRIDGESYSTEM_API ISuspenseCoreServiceConsumer
{
	GENERATED_BODY()

public:
	/**
	 * Called after services are injected and validated.
	 * Use this to cache service references.
	 * @param Provider The ServiceProvider with all services
	 */
	virtual void OnServicesInjected(USuspenseCoreServiceProvider* Provider) = 0;

	/**
	 * Get list of required service names for validation.
	 * Return empty array if no strict requirements.
	 * @return Array of required service class names
	 */
	virtual TArray<FName> GetRequiredServices() const { return TArray<FName>(); }

	/**
	 * Called when a required service is missing.
	 * Default implementation logs error.
	 * @param ServiceName Name of missing service
	 */
	virtual void OnServiceMissing(FName ServiceName)
	{
		// Default: log error (implementations can override for custom handling)
	}

	/**
	 * Validate that all required services are available and inject them.
	 * Call this in BeginPlay.
	 * @param WorldContextObject Object with world context
	 * @return true if all required services available
	 */
	bool ValidateAndInjectServices(const UObject* WorldContextObject);
};

// ═══════════════════════════════════════════════════════════════════════════════
// EVENT PUBLISHER INTERFACE
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * USuspenseCoreEventPublisher
 *
 * Interface for services that can publish events.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreEventPublisher : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreEventPublisher
 *
 * Standard interface for objects that publish events through EventBus.
 * Provides consistent event publishing API.
 */
class BRIDGESYSTEM_API ISuspenseCoreEventPublisher
{
	GENERATED_BODY()

public:
	/**
	 * Publish event through EventBus.
	 * @param EventTag GameplayTag identifying the event
	 * @param EventData Event payload
	 */
	virtual void PublishEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData) = 0;

	/**
	 * Publish simple event with just a source.
	 * @param EventTag GameplayTag identifying the event
	 * @param Source Source object of the event
	 */
	virtual void PublishSimpleEvent(FGameplayTag EventTag, UObject* Source) = 0;

	/**
	 * Get EventBus used for publishing.
	 * @return EventBus or nullptr
	 */
	virtual USuspenseCoreEventBus* GetPublisherEventBus() const = 0;
};

// ═══════════════════════════════════════════════════════════════════════════════
// ITEM PROVIDER INTERFACE
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * USuspenseCoreItemProvider
 *
 * Interface for services that provide item data.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreItemProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreItemProvider
 *
 * Interface for accessing item data.
 * Allows mocking DataManager in tests.
 */
class BRIDGESYSTEM_API ISuspenseCoreItemProvider
{
	GENERATED_BODY()

public:
	/**
	 * Get item data by ID.
	 * @param ItemID Item identifier
	 * @param OutData Output item data
	 * @return true if item found
	 */
	virtual bool GetItemData(FName ItemID, FSuspenseCoreItemData& OutData) const = 0;

	/**
	 * Check if item exists.
	 * @param ItemID Item identifier
	 * @return true if item exists in database
	 */
	virtual bool HasItem(FName ItemID) const = 0;

	/**
	 * Get all available item IDs.
	 * @return Array of all item IDs
	 */
	virtual TArray<FName> GetAllItemIDs() const = 0;
};

// ═══════════════════════════════════════════════════════════════════════════════
// SAVEABLE SERVICE INTERFACE
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * USuspenseCoreSaveableService
 *
 * Interface for services that need to save/load state.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreSaveableService : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreSaveableService
 *
 * Interface for services with persistent state.
 */
class BRIDGESYSTEM_API ISuspenseCoreSaveableService
{
	GENERATED_BODY()

public:
	/**
	 * Get unique identifier for save data.
	 * @return Unique save key
	 */
	virtual FName GetSaveKey() const = 0;

	/**
	 * Serialize service state to JSON.
	 * @return JSON string of service state
	 */
	virtual FString SerializeToJson() const = 0;

	/**
	 * Deserialize service state from JSON.
	 * @param JsonString JSON string to deserialize
	 * @return true if successful
	 */
	virtual bool DeserializeFromJson(const FString& JsonString) = 0;

	/**
	 * Check if service has unsaved changes.
	 * @return true if dirty
	 */
	virtual bool IsDirty() const = 0;

	/**
	 * Mark service as clean (after save).
	 */
	virtual void MarkClean() = 0;
};

// ═══════════════════════════════════════════════════════════════════════════════
// NETWORK SERVICE INTERFACE
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * USuspenseCoreNetworkService
 *
 * Interface for services with network requirements.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreNetworkService : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreNetworkService
 *
 * Interface for network-aware services.
 */
class BRIDGESYSTEM_API ISuspenseCoreNetworkService
{
	GENERATED_BODY()

public:
	/**
	 * Check if this service should only run on server.
	 * @return true if server-only
	 */
	virtual bool IsServerOnly() const = 0;

	/**
	 * Check if this service requires network authority.
	 * @return true if requires authority
	 */
	virtual bool RequiresAuthority() const = 0;

	/**
	 * Called when network role changes.
	 * @param bIsServer true if now server
	 * @param bHasAuthority true if has authority
	 */
	virtual void OnNetworkRoleChanged(bool bIsServer, bool bHasAuthority) = 0;
};

