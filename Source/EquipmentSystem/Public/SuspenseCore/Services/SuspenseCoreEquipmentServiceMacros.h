// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

//========================================
// Service Identification Macros
//========================================

/**
 * Define a service tag for equipment services
 * Usage: DEFINE_EQUIPMENT_SERVICE_TAG(Data, "Equipment.Service.Data")
 */
#define DEFINE_EQUIPMENT_SERVICE_TAG(ServiceName, TagString) \
	static FGameplayTag Get##ServiceName##ServiceTag() \
	{ \
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TagString)); \
		return Tag; \
	}

/**
 * Define required dependencies for a service
 * Usage: DEFINE_SERVICE_DEPENDENCIES(MyService, "Equipment.Service.Data", "Equipment.Service.Network")
 */
#define DEFINE_SERVICE_DEPENDENCIES(...) \
	virtual FGameplayTagContainer GetRequiredDependencies() const override \
	{ \
		FGameplayTagContainer Container; \
		TArray<FString> Deps = {__VA_ARGS__}; \
		for (const FString& Dep : Deps) \
		{ \
			Container.AddTag(FGameplayTag::RequestGameplayTag(FName(*Dep))); \
		} \
		return Container; \
	}

//========================================
// Service State Validation Macros
//========================================

/**
 * Check if service is in Ready state before executing
 */
#define CHECK_SERVICE_READY() \
	if (!IsServiceReady()) \
	{ \
		UE_LOG(LogTemp, Warning, TEXT("%s: Service not ready. Current state: %d"), \
			*FString(__FUNCTION__), static_cast<int32>(ServiceState)); \
		return {}; \
	}

/**
 * Check if service is in Ready state (returns bool)
 */
#define CHECK_SERVICE_READY_BOOL() \
	if (!IsServiceReady()) \
	{ \
		UE_LOG(LogTemp, Warning, TEXT("%s: Service not ready. Current state: %d"), \
			*FString(__FUNCTION__), static_cast<int32>(ServiceState)); \
		return false; \
	}

/**
 * Validate service state matches expected state
 */
#define VALIDATE_SERVICE_STATE(ExpectedState) \
	if (ServiceState != ExpectedState) \
	{ \
		UE_LOG(LogTemp, Warning, TEXT("%s: Invalid service state. Expected: %d, Current: %d"), \
			*FString(__FUNCTION__), static_cast<int32>(ExpectedState), static_cast<int32>(ServiceState)); \
		return {}; \
	}

//========================================
// Event Bus Integration Macros
//========================================

/**
 * Publish event through EventBus
 * Requires ServiceLocator to be available
 */
#define PUBLISH_SERVICE_EVENT(EventTag, EventData) \
	if (ServiceLocator.IsValid()) \
	{ \
		if (auto* EventBus = ServiceLocator->GetService<USuspenseCoreEventBus>()) \
		{ \
			EventBus->Publish(EventTag, EventData); \
		} \
	}

/**
 * Subscribe to event through EventBus
 */
#define SUBSCRIBE_SERVICE_EVENT(EventTag, Callback) \
	if (ServiceLocator.IsValid()) \
	{ \
		if (auto* EventBus = ServiceLocator->GetService<USuspenseCoreEventBus>()) \
		{ \
			EventBus->Subscribe(EventTag, Callback); \
		} \
	}

//========================================
// Dependency Injection Macros
//========================================

/**
 * Get service from ServiceLocator with validation
 */
#define GET_SERVICE_FROM_LOCATOR(ServiceType, OutVariable) \
	ServiceType* OutVariable = nullptr; \
	if (ServiceLocator.IsValid()) \
	{ \
		OutVariable = ServiceLocator->GetService<ServiceType>(); \
	}

/**
 * Get service from ServiceLocator and check validity
 */
#define GET_SERVICE_CHECKED(ServiceType, OutVariable) \
	GET_SERVICE_FROM_LOCATOR(ServiceType, OutVariable) \
	if (!OutVariable) \
	{ \
		UE_LOG(LogTemp, Error, TEXT("%s: Failed to get service: %s"), \
			*FString(__FUNCTION__), TEXT(#ServiceType)); \
		return {}; \
	}

//========================================
// Service Logging Macros
//========================================

DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseCoreEquipmentService, Log, All);

#define LOG_SERVICE_INFO(Format, ...) \
	UE_LOG(LogSuspenseCoreEquipmentService, Log, TEXT("%s: ") Format, *FString(__FUNCTION__), ##__VA_ARGS__)

#define LOG_SERVICE_WARNING(Format, ...) \
	UE_LOG(LogSuspenseCoreEquipmentService, Warning, TEXT("%s: ") Format, *FString(__FUNCTION__), ##__VA_ARGS__)

#define LOG_SERVICE_ERROR(Format, ...) \
	UE_LOG(LogSuspenseCoreEquipmentService, Error, TEXT("%s: ") Format, *FString(__FUNCTION__), ##__VA_ARGS__)

#define LOG_SERVICE_VERBOSE(Format, ...) \
	UE_LOG(LogSuspenseCoreEquipmentService, Verbose, TEXT("%s: ") Format, *FString(__FUNCTION__), ##__VA_ARGS__)

//========================================
// Service Metrics Macros
//========================================

/**
 * Record service operation metric
 */
#define RECORD_SERVICE_OPERATION(OperationName) \
	const double StartTime_##OperationName = FPlatformTime::Seconds(); \
	ON_SCOPE_EXIT \
	{ \
		const double Duration = (FPlatformTime::Seconds() - StartTime_##OperationName) * 1000.0; \
		LOG_SERVICE_VERBOSE(TEXT("%s completed in %.2f ms"), TEXT(#OperationName), Duration); \
	};

/**
 * Track service initialization time
 */
#define TRACK_SERVICE_INIT() \
	RECORD_SERVICE_OPERATION(InitializeService)

/**
 * Track service shutdown time
 */
#define TRACK_SERVICE_SHUTDOWN() \
	RECORD_SERVICE_OPERATION(ShutdownService)
