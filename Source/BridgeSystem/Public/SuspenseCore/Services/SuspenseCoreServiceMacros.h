// SuspenseCoreServiceMacros.h
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "SuspenseCore/Services/SuspenseCoreServiceProvider.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"

/**
 * SuspenseCore Service Access Macros
 *
 * Convenience macros for accessing services through ServiceProvider.
 * Use these for concise, consistent service access across the codebase.
 *
 * USAGE:
 * ═══════════════════════════════════════════════════════════════════════════
 *   // Get EventBus
 *   SUSPENSE_GET_EVENTBUS(this, EventBus);
 *   if (EventBus)
 *   {
 *       EventBus->Publish(Tag, Data);
 *   }
 *
 *   // Quick event publish
 *   SUSPENSE_PUBLISH_EVENT(this, "SuspenseCore.Event.MyEvent", Data);
 *
 *   // Require service (asserts in development builds)
 *   SUSPENSE_REQUIRE_SERVICE(this, USuspenseCoreEventBus);
 */

// ═══════════════════════════════════════════════════════════════════════════════
// SERVICE PROVIDER ACCESS
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * SUSPENSE_GET_PROVIDER
 * Get ServiceProvider from world context.
 *
 * @param WorldContext Object with world context (this, Actor, etc.)
 * @param OutVar Variable name for the provider
 *
 * Usage:
 *   SUSPENSE_GET_PROVIDER(this, Provider);
 *   if (Provider) { ... }
 */
#define SUSPENSE_GET_PROVIDER(WorldContext, OutVar) \
	USuspenseCoreServiceProvider* OutVar = USuspenseCoreServiceProvider::Get(WorldContext)

// ═══════════════════════════════════════════════════════════════════════════════
// CORE SERVICE ACCESS
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * SUSPENSE_GET_EVENTBUS
 * Get EventBus from ServiceProvider.
 *
 * @param WorldContext Object with world context
 * @param OutVar Variable name for EventBus
 *
 * Usage:
 *   SUSPENSE_GET_EVENTBUS(this, EventBus);
 *   if (EventBus)
 *   {
 *       EventBus->Publish(Tag, Data);
 *   }
 */
#define SUSPENSE_GET_EVENTBUS(WorldContext, OutVar) \
	USuspenseCoreEventBus* OutVar = nullptr; \
	do { \
		if (USuspenseCoreServiceProvider* _Provider = USuspenseCoreServiceProvider::Get(WorldContext)) \
		{ \
			OutVar = _Provider->GetEventBus(); \
		} \
	} while(0)

/**
 * SUSPENSE_GET_DATAMANAGER
 * Get DataManager from ServiceProvider.
 *
 * @param WorldContext Object with world context
 * @param OutVar Variable name for DataManager
 *
 * Usage:
 *   SUSPENSE_GET_DATAMANAGER(this, DataManager);
 *   if (DataManager)
 *   {
 *       FSuspenseCoreItemData ItemData;
 *       DataManager->GetItemData(ItemID, ItemData);
 *   }
 */
#define SUSPENSE_GET_DATAMANAGER(WorldContext, OutVar) \
	USuspenseCoreDataManager* OutVar = nullptr; \
	do { \
		if (USuspenseCoreServiceProvider* _Provider = USuspenseCoreServiceProvider::Get(WorldContext)) \
		{ \
			OutVar = _Provider->GetDataManager(); \
		} \
	} while(0)

/**
 * SUSPENSE_GET_SERVICE
 * Get typed service from ServiceProvider.
 *
 * @param WorldContext Object with world context
 * @param ServiceClass Class type of the service
 * @param OutVar Variable name for the service
 *
 * Usage:
 *   SUSPENSE_GET_SERVICE(this, UMyCustomService, MyService);
 *   if (MyService) { ... }
 */
#define SUSPENSE_GET_SERVICE(WorldContext, ServiceClass, OutVar) \
	ServiceClass* OutVar = nullptr; \
	do { \
		if (USuspenseCoreServiceProvider* _Provider = USuspenseCoreServiceProvider::Get(WorldContext)) \
		{ \
			OutVar = _Provider->GetService<ServiceClass>(); \
		} \
	} while(0)

// ═══════════════════════════════════════════════════════════════════════════════
// EVENT PUBLISHING
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * SUSPENSE_PUBLISH_EVENT
 * Quick event publishing through EventBus.
 *
 * @param WorldContext Object with world context
 * @param EventTagName String name of event tag (e.g., "SuspenseCore.Event.MyEvent")
 * @param EventData FSuspenseCoreEventData to publish
 *
 * Usage:
 *   FSuspenseCoreEventData Data = FSuspenseCoreEventData::Create(this);
 *   Data.SetString(FName("Key"), TEXT("Value"));
 *   SUSPENSE_PUBLISH_EVENT(this, "SuspenseCore.Event.Something.Happened", Data);
 */
#define SUSPENSE_PUBLISH_EVENT(WorldContext, EventTagName, EventData) \
	do { \
		SUSPENSE_GET_EVENTBUS(WorldContext, _EventBus); \
		if (_EventBus) \
		{ \
			_EventBus->Publish(FGameplayTag::RequestGameplayTag(FName(TEXT(EventTagName))), EventData); \
		} \
	} while(0)

/**
 * SUSPENSE_PUBLISH_SIMPLE_EVENT
 * Quick simple event publishing (source only, no payload).
 *
 * @param WorldContext Object with world context
 * @param EventTagName String name of event tag
 * @param Source Source object for the event
 *
 * Usage:
 *   SUSPENSE_PUBLISH_SIMPLE_EVENT(this, "SuspenseCore.Event.Player.Spawned", PlayerActor);
 */
#define SUSPENSE_PUBLISH_SIMPLE_EVENT(WorldContext, EventTagName, Source) \
	do { \
		SUSPENSE_GET_EVENTBUS(WorldContext, _EventBus); \
		if (_EventBus) \
		{ \
			_EventBus->PublishSimple(FGameplayTag::RequestGameplayTag(FName(TEXT(EventTagName))), Source); \
		} \
	} while(0)

/**
 * SUSPENSE_PUBLISH_DEFERRED
 * Publish event at end of frame (deferred).
 *
 * @param WorldContext Object with world context
 * @param EventTagName String name of event tag
 * @param EventData FSuspenseCoreEventData to publish
 */
#define SUSPENSE_PUBLISH_DEFERRED(WorldContext, EventTagName, EventData) \
	do { \
		SUSPENSE_GET_EVENTBUS(WorldContext, _EventBus); \
		if (_EventBus) \
		{ \
			_EventBus->PublishDeferred(FGameplayTag::RequestGameplayTag(FName(TEXT(EventTagName))), EventData); \
		} \
	} while(0)

// ═══════════════════════════════════════════════════════════════════════════════
// SERVICE VALIDATION (Development builds)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * SUSPENSE_REQUIRE_SERVICE
 * Assert that a service is available (development builds only).
 *
 * @param WorldContext Object with world context
 * @param ServiceClass Class type of required service
 *
 * Usage:
 *   SUSPENSE_REQUIRE_SERVICE(this, USuspenseCoreEventBus);
 */
#if !UE_BUILD_SHIPPING
#define SUSPENSE_REQUIRE_SERVICE(WorldContext, ServiceClass) \
	do { \
		SUSPENSE_GET_SERVICE(WorldContext, ServiceClass, _RequiredService); \
		checkf(_RequiredService != nullptr, \
			TEXT("Required service %s not available. Ensure ServiceProvider is initialized."), \
			TEXT(#ServiceClass)); \
	} while(0)
#else
#define SUSPENSE_REQUIRE_SERVICE(WorldContext, ServiceClass) ((void)0)

/**
 * SUSPENSE_REQUIRE_EVENTBUS
 * Assert that EventBus is available.
 */
#if !UE_BUILD_SHIPPING
#define SUSPENSE_REQUIRE_EVENTBUS(WorldContext) \
	do { \
		SUSPENSE_GET_EVENTBUS(WorldContext, _RequiredEventBus); \
		checkf(_RequiredEventBus != nullptr, \
			TEXT("EventBus not available. Ensure ServiceProvider is initialized.")); \
	} while(0)
#else
#define SUSPENSE_REQUIRE_EVENTBUS(WorldContext) ((void)0)

// ═══════════════════════════════════════════════════════════════════════════════
// CONDITIONAL SERVICE ACCESS
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * SUSPENSE_WITH_EVENTBUS
 * Execute block if EventBus is available.
 *
 * @param WorldContext Object with world context
 * @param VarName Variable name for EventBus in block
 *
 * Usage:
 *   SUSPENSE_WITH_EVENTBUS(this, EventBus)
 *   {
 *       EventBus->Publish(Tag, Data);
 *   }
 */
#define SUSPENSE_WITH_EVENTBUS(WorldContext, VarName) \
	if (USuspenseCoreEventBus* VarName = []() -> USuspenseCoreEventBus* { \
		if (USuspenseCoreServiceProvider* _P = USuspenseCoreServiceProvider::Get(WorldContext)) \
		{ \
			return _P->GetEventBus(); \
		} \
		return nullptr; \
	}())

/**
 * SUSPENSE_WITH_DATAMANAGER
 * Execute block if DataManager is available.
 *
 * @param WorldContext Object with world context
 * @param VarName Variable name for DataManager in block
 *
 * Usage:
 *   SUSPENSE_WITH_DATAMANAGER(this, DM)
 *   {
 *       DM->GetItemData(ItemID, OutData);
 *   }
 */
#define SUSPENSE_WITH_DATAMANAGER(WorldContext, VarName) \
	if (USuspenseCoreDataManager* VarName = []() -> USuspenseCoreDataManager* { \
		if (USuspenseCoreServiceProvider* _P = USuspenseCoreServiceProvider::Get(WorldContext)) \
		{ \
			return _P->GetDataManager(); \
		} \
		return nullptr; \
	}())

/**
 * SUSPENSE_WITH_PROVIDER
 * Execute block if ServiceProvider is available.
 *
 * @param WorldContext Object with world context
 * @param VarName Variable name for ServiceProvider in block
 */
#define SUSPENSE_WITH_PROVIDER(WorldContext, VarName) \
	if (USuspenseCoreServiceProvider* VarName = USuspenseCoreServiceProvider::Get(WorldContext))

// ═══════════════════════════════════════════════════════════════════════════════
// SERVICE REGISTRATION
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * SUSPENSE_REGISTER_SERVICE
 * Register a service in ServiceProvider.
 *
 * @param WorldContext Object with world context
 * @param ServiceClass Class type of service
 * @param ServiceInstance Instance to register
 */
#define SUSPENSE_REGISTER_SERVICE(WorldContext, ServiceClass, ServiceInstance) \
	do { \
		if (USuspenseCoreServiceProvider* _Provider = USuspenseCoreServiceProvider::Get(WorldContext)) \
		{ \
			_Provider->RegisterService<ServiceClass>(ServiceInstance); \
		} \
	} while(0)

/**
 * SUSPENSE_UNREGISTER_SERVICE
 * Unregister a service from ServiceProvider.
 *
 * @param WorldContext Object with world context
 * @param ServiceName FName of service to unregister
 */
#define SUSPENSE_UNREGISTER_SERVICE(WorldContext, ServiceName) \
	do { \
		if (USuspenseCoreServiceProvider* _Provider = USuspenseCoreServiceProvider::Get(WorldContext)) \
		{ \
			_Provider->UnregisterService(ServiceName); \
		} \
	} while(0)

// ═══════════════════════════════════════════════════════════════════════════════
// QUICK EVENT DATA CREATION
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * SUSPENSE_CREATE_EVENT
 * Create FSuspenseCoreEventData with source.
 *
 * @param Source Source object for event
 * @param OutVar Variable name for event data
 *
 * Usage:
 *   SUSPENSE_CREATE_EVENT(this, EventData);
 *   EventData.SetString(FName("Key"), TEXT("Value"));
 */
#define SUSPENSE_CREATE_EVENT(Source, OutVar) \
	FSuspenseCoreEventData OutVar = FSuspenseCoreEventData::Create(Source)

/**
 * SUSPENSE_CREATE_EVENT_PRIORITY
 * Create FSuspenseCoreEventData with source and priority.
 *
 * @param Source Source object
 * @param Priority ESuspenseCoreEventPriority value
 * @param OutVar Variable name
 */
#define SUSPENSE_CREATE_EVENT_PRIORITY(Source, Priority, OutVar) \
	FSuspenseCoreEventData OutVar = FSuspenseCoreEventData::Create(Source, Priority)

