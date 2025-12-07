// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreEquipmentValidationService.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"

USuspenseCoreEquipmentValidationService::USuspenseCoreEquipmentValidationService()
	: ServiceState(EServiceLifecycleState::Uninitialized)
	, bEnableCaching(true)
	, CacheTTL(5.0f)
	, bEnableDetailedLogging(false)
	, bStrictValidation(true)
	, TotalValidations(0)
	, TotalValidationsPassed(0)
	, TotalValidationsFailed(0)
	, CacheHits(0)
	, CacheMisses(0)
{
}

USuspenseCoreEquipmentValidationService::~USuspenseCoreEquipmentValidationService()
{
}

//========================================
// ISuspenseEquipmentService Interface
//========================================

bool USuspenseCoreEquipmentValidationService::InitializeService(const FServiceInitParams& Params)
{
	TRACK_SERVICE_INIT();

	ServiceState = EServiceLifecycleState::Initializing;
	ServiceLocator = Params.ServiceLocator;

	if (!InitializeValidationRules())
	{
		LOG_SERVICE_ERROR(TEXT("Failed to initialize validation rules"));
		ServiceState = EServiceLifecycleState::Failed;
		return false;
	}

	SetupEventSubscriptions();

	InitializationTime = FDateTime::UtcNow();
	ServiceState = EServiceLifecycleState::Ready;

	LOG_SERVICE_INFO(TEXT("Service initialized successfully"));
	return true;
}

bool USuspenseCoreEquipmentValidationService::ShutdownService(bool bForce)
{
	TRACK_SERVICE_SHUTDOWN();

	ServiceState = EServiceLifecycleState::Shutting;
	CleanupResources();
	ServiceState = EServiceLifecycleState::Shutdown;

	LOG_SERVICE_INFO(TEXT("Service shut down"));
	return true;
}

EServiceLifecycleState USuspenseCoreEquipmentValidationService::GetServiceState() const
{
	return ServiceState;
}

bool USuspenseCoreEquipmentValidationService::IsServiceReady() const
{
	return ServiceState == EServiceLifecycleState::Ready;
}

FGameplayTag USuspenseCoreEquipmentValidationService::GetServiceTag() const
{
	static FGameplayTag ServiceTag = FGameplayTag::RequestGameplayTag(FName("Equipment.Service.Validation"));
	return ServiceTag;
}

FGameplayTagContainer USuspenseCoreEquipmentValidationService::GetRequiredDependencies() const
{
	FGameplayTagContainer Dependencies;
	Dependencies.AddTag(FGameplayTag::RequestGameplayTag(FName("Equipment.Service.Data")));
	return Dependencies;
}

bool USuspenseCoreEquipmentValidationService::ValidateService(TArray<FText>& OutErrors) const
{
	return true;
}

void USuspenseCoreEquipmentValidationService::ResetService()
{
	TotalValidations = 0;
	TotalValidationsPassed = 0;
	TotalValidationsFailed = 0;
	CacheHits = 0;
	CacheMisses = 0;
	ClearValidationCache();
	LOG_SERVICE_INFO(TEXT("Service reset"));
}

FString USuspenseCoreEquipmentValidationService::GetServiceStats() const
{
	float PassRate = (TotalValidations > 0) ?
		(float)TotalValidationsPassed / (float)TotalValidations * 100.0f : 0.0f;

	return FString::Printf(TEXT("Validation - Total: %d, Passed: %d, Failed: %d, Pass Rate: %.2f%%"),
		TotalValidations, TotalValidationsPassed, TotalValidationsFailed, PassRate);
}

//========================================
// IEquipmentValidationService Interface
//========================================

ISuspenseEquipmentRules* USuspenseCoreEquipmentValidationService::GetRulesEngine()
{
	return nullptr;
}

bool USuspenseCoreEquipmentValidationService::RegisterValidator(const FGameplayTag& ValidatorTag, TFunction<bool(const void*)> Validator)
{
	return true;
}

void USuspenseCoreEquipmentValidationService::ClearValidationCache()
{
	FRWScopeLock ScopeLock(CacheLock, SLT_Write);
	ValidationCache.Empty();
	CacheTimestamps.Empty();
	LOG_SERVICE_INFO(TEXT("Validation cache cleared"));
}

//========================================
// Validation Operations
//========================================

FSlotValidationResult USuspenseCoreEquipmentValidationService::ValidateOperation(const FEquipmentOperationRequest& Request)
{
	CHECK_SERVICE_READY();
	RECORD_SERVICE_OPERATION(ValidateOperation);

	TotalValidations++;

	FSlotValidationResult Result;
	Result.bIsValid = true;

	FText Error;
	if (!ExecuteValidationRules(Request, Error))
	{
		Result.bIsValid = false;
		Result.ErrorMessage = Error;
		TotalValidationsFailed++;
		PublishValidationFailed(Request, Error);
	}
	else
	{
		TotalValidationsPassed++;
		PublishValidationSucceeded(Request);
	}

	return Result;
}

bool USuspenseCoreEquipmentValidationService::ValidateSlotCompatibility(int32 SlotIndex, const FSuspenseInventoryItemInstance& Item, FText& OutReason) const
{
	CHECK_SERVICE_READY_BOOL();

	// Slot compatibility validation logic
	return true;
}

bool USuspenseCoreEquipmentValidationService::ValidateWeightLimit(const FSuspenseInventoryItemInstance& Item, FText& OutReason) const
{
	CHECK_SERVICE_READY_BOOL();

	// Weight limit validation logic
	return true;
}

bool USuspenseCoreEquipmentValidationService::ValidateRequirements(const FSuspenseInventoryItemInstance& Item, FText& OutReason) const
{
	CHECK_SERVICE_READY_BOOL();

	// Requirements validation logic
	return true;
}

bool USuspenseCoreEquipmentValidationService::ValidateNoConflicts(const FSuspenseInventoryItemInstance& Item, FText& OutReason) const
{
	CHECK_SERVICE_READY_BOOL();

	// Conflict validation logic
	return true;
}

TArray<FSlotValidationResult> USuspenseCoreEquipmentValidationService::BatchValidateOperations(const TArray<FEquipmentOperationRequest>& Requests)
{
	CHECK_SERVICE_READY();

	TArray<FSlotValidationResult> Results;
	Results.Reserve(Requests.Num());

	for (const FEquipmentOperationRequest& Request : Requests)
	{
		Results.Add(ValidateOperation(Request));
	}

	return Results;
}

//========================================
// Rule Management
//========================================

bool USuspenseCoreEquipmentValidationService::RegisterValidationRule(FGameplayTag RuleTag, const FEquipmentValidationRule& Rule)
{
	FRWScopeLock ScopeLock(RulesLock, SLT_Write);
	ValidationRules.Add(RuleTag, Rule);
	RuleEnabledStates.Add(RuleTag, true);
	LOG_SERVICE_INFO(TEXT("Validation rule registered: %s"), *RuleTag.ToString());
	return true;
}

bool USuspenseCoreEquipmentValidationService::UnregisterValidationRule(FGameplayTag RuleTag)
{
	FRWScopeLock ScopeLock(RulesLock, SLT_Write);
	ValidationRules.Remove(RuleTag);
	RuleEnabledStates.Remove(RuleTag);
	LOG_SERVICE_INFO(TEXT("Validation rule unregistered: %s"), *RuleTag.ToString());
	return true;
}

void USuspenseCoreEquipmentValidationService::SetRuleEnabled(FGameplayTag RuleTag, bool bEnabled)
{
	FRWScopeLock ScopeLock(RulesLock, SLT_Write);
	RuleEnabledStates.Add(RuleTag, bEnabled);
	PublishRulesChanged(RuleTag);
}

bool USuspenseCoreEquipmentValidationService::IsRuleEnabled(FGameplayTag RuleTag) const
{
	FRWScopeLock ScopeLock(RulesLock, SLT_ReadOnly);
	const bool* bEnabled = RuleEnabledStates.Find(RuleTag);
	return bEnabled ? *bEnabled : false;
}

TArray<FGameplayTag> USuspenseCoreEquipmentValidationService::GetRegisteredRules() const
{
	FRWScopeLock ScopeLock(RulesLock, SLT_ReadOnly);
	TArray<FGameplayTag> Rules;
	ValidationRules.GetKeys(Rules);
	return Rules;
}

//========================================
// Cache Management
//========================================

void USuspenseCoreEquipmentValidationService::InvalidateCacheForRequest(const FEquipmentOperationRequest& Request)
{
	uint32 CacheKey = GenerateCacheKey(Request);

	FRWScopeLock ScopeLock(CacheLock, SLT_Write);
	ValidationCache.Remove(CacheKey);
	CacheTimestamps.Remove(CacheKey);
}

FString USuspenseCoreEquipmentValidationService::GetCacheStatistics() const
{
	float HitRate = (CacheHits + CacheMisses > 0) ?
		(float)CacheHits / (float)(CacheHits + CacheMisses) * 100.0f : 0.0f;

	return FString::Printf(TEXT("Cache - Hits: %d, Misses: %d, Hit Rate: %.2f%%"),
		CacheHits, CacheMisses, HitRate);
}

//========================================
// Event Publishing
//========================================

void USuspenseCoreEquipmentValidationService::PublishValidationFailed(const FEquipmentOperationRequest& Request, const FText& Reason)
{
	PUBLISH_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Validation.Failed")),
		FSuspenseEquipmentEventData()
	);
}

void USuspenseCoreEquipmentValidationService::PublishValidationSucceeded(const FEquipmentOperationRequest& Request)
{
	PUBLISH_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Validation.Succeeded")),
		FSuspenseEquipmentEventData()
	);
}

void USuspenseCoreEquipmentValidationService::PublishRulesChanged(FGameplayTag RuleTag)
{
	PUBLISH_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Validation.RulesChanged")),
		FSuspenseEquipmentEventData()
	);
}

//========================================
// Service Lifecycle
//========================================

bool USuspenseCoreEquipmentValidationService::InitializeValidationRules()
{
	// Initialize default validation rules
	return true;
}

void USuspenseCoreEquipmentValidationService::SetupEventSubscriptions()
{
	// Setup event subscriptions
}

void USuspenseCoreEquipmentValidationService::CleanupResources()
{
	ClearValidationCache();
	ValidationRules.Empty();
	RuleEnabledStates.Empty();
}

//========================================
// Validation Logic
//========================================

bool USuspenseCoreEquipmentValidationService::ExecuteValidationRules(const FEquipmentOperationRequest& Request, FText& OutReason)
{
	// Execute all enabled validation rules
	return true;
}

bool USuspenseCoreEquipmentValidationService::CheckRule(FGameplayTag RuleTag, const FEquipmentOperationRequest& Request, FText& OutReason) const
{
	// Check single validation rule
	return true;
}

uint32 USuspenseCoreEquipmentValidationService::GenerateCacheKey(const FEquipmentOperationRequest& Request) const
{
	// Generate cache key from request
	return 0;
}

//========================================
// Event Handlers
//========================================

void USuspenseCoreEquipmentValidationService::OnDataChanged(const FSuspenseEquipmentEventData& EventData)
{
	// Handle data changed - invalidate affected cache entries
	ClearValidationCache();
}

void USuspenseCoreEquipmentValidationService::OnConfigurationChanged(const FSuspenseEquipmentEventData& EventData)
{
	// Handle configuration changed
}
