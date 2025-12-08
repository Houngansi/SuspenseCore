// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreEquipmentValidationService.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "Core/Utils/SuspenseEquipmentThreadGuard.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceLocator.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceMacros.h"
#include "Core/Utils/SuspenseGlobalCacheRegistry.h"
#include "SuspenseCore/Components/Rules/SuspenseCoreRulesCoordinator.h"
#include "Interfaces/Equipment/ISuspenseEquipmentDataProvider.h"
#include "Types/Rules/SuspenseRulesTypes.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Engine/World.h"
#include "Engine/NetConnection.h"
#include "GameFramework/PlayerState.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "HAL/PlatformProcess.h"
#include "Online/CoreOnline.h"
#include "Misc/Crc.h"

//========================================
// Shadow Snapshot Implementation
//========================================

bool FSuspenseCoreShadowEquipmentSnapshot::ApplyOperation(const FEquipmentOperationRequest& Operation)
{
    switch (Operation.OperationType)
    {
        case EEquipmentOperationType::Equip:
        {
            if (SlotItems.Contains(Operation.TargetSlotIndex))
            {
                return false; // Target slot occupied
            }
            SlotItems.Add(Operation.TargetSlotIndex, Operation.ItemInstance);
            ItemQuantities.FindOrAdd(Operation.ItemInstance.ItemID) += Operation.ItemInstance.Quantity;

            // Weight
            const float ItemWeight = Operation.ItemInstance.GetRuntimeProperty(TEXT("Weight"), 1.0f);
            TotalWeight += ItemWeight * Operation.ItemInstance.Quantity;
            return true;
        }

        case EEquipmentOperationType::Unequip:
        {
            // Use SourceSlotIndex for unequip
            if (!SlotItems.Contains(Operation.SourceSlotIndex))
            {
                return false; // Source slot empty
            }

            const FSuspenseInventoryItemInstance RemovedItem = SlotItems[Operation.SourceSlotIndex];
            SlotItems.Remove(Operation.SourceSlotIndex);

            int32& Qty = ItemQuantities.FindOrAdd(RemovedItem.ItemID);
            Qty -= RemovedItem.Quantity;
            if (Qty <= 0)
            {
                ItemQuantities.Remove(RemovedItem.ItemID);
            }

            const float ItemWeight = RemovedItem.GetRuntimeProperty(TEXT("Weight"), 1.0f);
            TotalWeight -= ItemWeight * RemovedItem.Quantity;
            return true;
        }

        case EEquipmentOperationType::Move:
        {
            if (!SlotItems.Contains(Operation.SourceSlotIndex))
            {
                return false; // Source empty
            }
            if (SlotItems.Contains(Operation.TargetSlotIndex))
            {
                return false; // Target occupied
            }

            const FSuspenseInventoryItemInstance MovedItem = SlotItems[Operation.SourceSlotIndex];
            SlotItems.Remove(Operation.SourceSlotIndex);
            SlotItems.Add(Operation.TargetSlotIndex, MovedItem);
            return true;
        }

        case EEquipmentOperationType::Swap:
        {
            if (!SlotItems.Contains(Operation.SourceSlotIndex) ||
                !SlotItems.Contains(Operation.TargetSlotIndex))
            {
                return false; // Both must be occupied
            }

            const FSuspenseInventoryItemInstance Temp = SlotItems[Operation.SourceSlotIndex];
            SlotItems[Operation.SourceSlotIndex] = SlotItems[Operation.TargetSlotIndex];
            SlotItems[Operation.TargetSlotIndex] = Temp;
            return true;
        }

        default:
            return false;
    }
}

bool FSuspenseCoreShadowEquipmentSnapshot::IsSlotOccupied(int32 SlotIndex) const
{
    return SlotItems.Contains(SlotIndex);
}

FSuspenseInventoryItemInstance FSuspenseCoreShadowEquipmentSnapshot::GetItemAtSlot(int32 SlotIndex) const
{
    if (const FSuspenseInventoryItemInstance* Found = SlotItems.Find(SlotIndex))
    {
        return *Found;
    }
    return FSuspenseInventoryItemInstance();
}

//========================================
// Service Implementation
//========================================

USuspenseCoreEquipmentValidationService::USuspenseCoreEquipmentValidationService()
{
    // Initialize cache with appropriate size
    ResultCache = MakeShareable(new FSuspenseCoreEquipmentCacheManager<uint32, FSlotValidationResult>(500));
    InitializationTime = FDateTime::Now();
}

USuspenseCoreEquipmentValidationService::~USuspenseCoreEquipmentValidationService()
{
    ShutdownService(true);
}

//========================================
// Configuration Management
//========================================

void USuspenseCoreEquipmentValidationService::EnsureValidConfig()
{
    // Sanitize CacheTTL
    const float OldCacheTTL = CacheTTL;
    CacheTTL = FMath::Clamp(CacheTTL, 0.05f, 60.0f);

    // Sanitize ParallelBatchThreshold
    const int32 OldThreshold = ParallelBatchThreshold;
    ParallelBatchThreshold = FMath::Clamp(ParallelBatchThreshold, 1, 10000);

    // Sanitize MaxParallelThreads based on CPU cores
    const int32 NumCores = FPlatformMisc::NumberOfCores();
    const int32 OldMaxThreads = MaxParallelThreads;
    MaxParallelThreads = FMath::Clamp(MaxParallelThreads, 2, FMath::Min(16, NumCores - 1));

    UE_LOG(LogSuspenseCoreEquipmentValidation, Verbose,
        TEXT("EnsureValidConfig: Final configuration - CacheTTL: %.2fs (was %.2fs), ParallelBatchThreshold: %d (was %d), MaxParallelThreads: %d (was %d, CPU cores: %d)"),
        CacheTTL, OldCacheTTL,
        ParallelBatchThreshold, OldThreshold,
        MaxParallelThreads, OldMaxThreads, NumCores);
}

//========================================
// IEquipmentService Implementation
//========================================

bool USuspenseCoreEquipmentValidationService::InitializeService(const FSuspenseCoreServiceInitParams& Params)
{
    SCOPED_SERVICE_TIMER("Validation.InitializeService");

    ServiceLocatorRef = Params.ServiceLocator;

    if (!ServiceLocatorRef.IsValid())
    {
        UE_LOG(LogSuspenseCoreEquipmentValidation, Error,
            TEXT("InitializeService: ServiceLocator not provided in init params"));
        ServiceMetrics.RecordError();
        return false;
    }

    EnsureValidConfig();

    EQUIPMENT_WRITE_LOCK(CacheLock);

    if (ServiceState != ESuspenseCoreServiceLifecycleState::Uninitialized)
    {
        UE_LOG(LogSuspenseCoreEquipmentValidation, Warning,
            TEXT("InitializeService: Service already initialized (state: %s)"),
            *UEnum::GetValueAsString(ServiceState));
        ServiceMetrics.RecordError();
        return false;
    }

    ServiceState = ESuspenseCoreServiceLifecycleState::Initializing;

    if (!InitializeDependencies())
    {
        ServiceState = ESuspenseCoreServiceLifecycleState::Failed;
        UE_LOG(LogSuspenseCoreEquipmentValidation, Error, TEXT("InitializeService: Failed to initialize dependencies"));
        ServiceMetrics.RecordError();
        return false;
    }

    // ✅ КРИТИЧЕСКОЕ ИЗМЕНЕНИЕ: RulesCoordinator работает БЕЗ обязательного DataProvider
    USuspenseRulesCoordinator* Coordinator = NewObject<USuspenseRulesCoordinator>(
        this,
        USuspenseRulesCoordinator::StaticClass(),
        TEXT("RulesCoordinator"),
        RF_Transient
    );

    if (!Coordinator)
    {
        ServiceState = ESuspenseCoreServiceLifecycleState::Failed;
        UE_LOG(LogSuspenseCoreEquipmentValidation, Error, TEXT("InitializeService: Failed to create rules coordinator"));
        ServiceMetrics.RecordError();
        return false;
    }

    // ✅ ИЗМЕНЕНИЕ: Передаем DataProvider (может быть nullptr)
    // RulesCoordinator теперь корректно работает в stateless режиме
    if (!Coordinator->Initialize(DataProvider))
    {
        ServiceState = ESuspenseCoreServiceLifecycleState::Failed;
        UE_LOG(LogSuspenseCoreEquipmentValidation, Error, TEXT("InitializeService: Failed to initialize rules coordinator"));
        ServiceMetrics.RecordError();
        return false;
    }

    Rules.SetObject(Coordinator);
    Rules.SetInterface(Coordinator);

    SetupEventSubscriptions();

    if (ResultCache.IsValid())
    {
        FSuspenseGlobalCacheRegistry::Get().RegisterCache<uint32, FSlotValidationResult>(
            TEXT("EquipmentValidation.Results"),
            ResultCache.Get());
    }

    ServiceState = ESuspenseCoreServiceLifecycleState::Ready;

    UE_LOG(LogSuspenseCoreEquipmentValidation, Log,
        TEXT("EquipmentValidationService initialized successfully:"));
    UE_LOG(LogSuspenseCoreEquipmentValidation, Log,
        TEXT("  - Mode: %s"),
        DataProvider.GetInterface() ? TEXT("STATEFUL (with DataProvider)") : TEXT("STATELESS (no DataProvider)"));
    UE_LOG(LogSuspenseCoreEquipmentValidation, Log,
        TEXT("  - RulesCoordinator: Available and initialized"));
    UE_LOG(LogSuspenseCoreEquipmentValidation, Log,
        TEXT("  - CacheTTL: %.1fs, Parallel: %s, MaxThreads: %d"),
        CacheTTL,
        bEnableParallelValidation ? TEXT("Enabled") : TEXT("Disabled"),
        MaxParallelThreads);
    UE_LOG(LogSuspenseCoreEquipmentValidation, Log,
        TEXT("  - RulesEpoch: %u"),
        RulesEpoch.load());

    ServiceMetrics.RecordSuccess();
    RECORD_SERVICE_METRIC("Validation.Service.Initialized", 1);
    return true;
}

bool USuspenseCoreEquipmentValidationService::ShutdownService(bool bForce)
{
    SCOPED_SERVICE_TIMER("Validation.ShutdownService");
    EQUIPMENT_WRITE_LOCK(CacheLock);

    if (ServiceState == ESuspenseCoreServiceLifecycleState::Shutdown)
    {
        return true;
    }

    ServiceState = ESuspenseCoreServiceLifecycleState::Shutting;

    // Clear cache
    if (ResultCache.IsValid())
    {
        ResultCache->Clear();
    }

    // Clear custom validators
    {
        FScopeLock Lock(&ValidatorsLock);
        CustomValidators.Empty();
    }

    // Unregister from cache registry
    FSuspenseGlobalCacheRegistry::Get().UnregisterCache(TEXT("EquipmentValidation.Results"));

    // Clear event subscriptions
    EventScope.UnsubscribeAll();

    // Clean up rules interface (coordinator is UObject, not component)
    if (Rules.GetInterface())
    {
        Rules.SetObject(nullptr);
        Rules.SetInterface(nullptr);
    }

    // Clear dependencies
    DataProvider = nullptr;
    TransactionManager = nullptr;

    ServiceState = ESuspenseCoreServiceLifecycleState::Shutdown;

    // Log final statistics
    const FTimespan Uptime = FDateTime::Now() - InitializationTime;
    UE_LOG(LogSuspenseCoreEquipmentValidation, Log,
        TEXT("Service shutdown - Uptime: %.1fh, Validated: %d, Pass rate: %.1f%%, Parallel batches: %d, Shadow batches: %d"),
        Uptime.GetTotalHours(),
        TotalValidations.load(),
        TotalValidations.load() > 0 ? (float)ValidationsPassed.load() / TotalValidations.load() * 100.0f : 0.0f,
        ParallelBatches.load(),
        ShadowSnapshotBatches.load());

    ServiceMetrics.RecordSuccess();
    RECORD_SERVICE_METRIC("Validation.Service.Shutdown", 1);
    return true;
}

FGameplayTag USuspenseCoreEquipmentValidationService::GetServiceTag() const
{
    SCOPED_SERVICE_TIMER_CONST("Validation.GetServiceTag");
    return FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Validation"));
}

FGameplayTagContainer USuspenseCoreEquipmentValidationService::GetRequiredDependencies() const
{
    SCOPED_SERVICE_TIMER_CONST("Validation.GetRequiredDependencies");
    FGameplayTagContainer Dependencies;
    Dependencies.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Data")));
    return Dependencies;
}

bool USuspenseCoreEquipmentValidationService::ValidateService(TArray<FText>& OutErrors) const
{
    SCOPED_SERVICE_TIMER_CONST("Validation.ValidateService");
    EQUIPMENT_READ_LOCK(CacheLock);

    OutErrors.Empty();
    bool bIsValid = true;

    if (!Rules.GetInterface())
    {
        OutErrors.Add(FText::FromString(TEXT("Rules interface not initialized")));
        bIsValid = false;
    }

    if (!DataProvider.GetInterface())
    {
        OutErrors.Add(FText::FromString(TEXT("Data provider not available")));
        bIsValid = false;
    }

    // Check cache performance
    const float CacheHitRate = TotalValidations.load() > 0 ?
        (float)CacheHits.load() / TotalValidations.load() : 0.0f;

    if (CacheHitRate < 0.3f && TotalValidations.load() > 100)
    {
        OutErrors.Add(FText::Format(
            FText::FromString(TEXT("Low cache hit rate: {0}%")),
            FMath::RoundToInt(CacheHitRate * 100)
        ));
    }

    if (bIsValid)
    {
        ServiceMetrics.RecordSuccess();
    }
    else
    {
        ServiceMetrics.RecordError();
    }

    return bIsValid;
}

void USuspenseCoreEquipmentValidationService::ResetService()
{
    SCOPED_SERVICE_TIMER("Validation.ResetService");
    EQUIPMENT_WRITE_LOCK(CacheLock);
    FEquipmentRWGuard StatsWriteGuard(StatsLock, ELockType::Write);

    UE_LOG(LogSuspenseCoreEquipmentValidation, Log, TEXT("ResetService: Beginning service reset"));

    // Clear cache
    if (ResultCache.IsValid())
    {
        ResultCache->Clear();
    }

    // Reset atomic statistics
    TotalValidations.store(0);
    CacheHits.store(0);
    ValidationsPassed.store(0);
    ValidationsFailed.store(0);
    ParallelBatches.store(0);
    SequentialBatches.store(0);
    ShadowSnapshotBatches.store(0);

    // Reset timing statistics
    AverageValidationTime = 0.0f;
    PeakValidationTime = 0.0f;
    AverageParallelBatchTime = 0.0f;
    PeakParallelBatchTime = 0.0f;
    LastParallelBatchTimeMs = 0.0f;
    AverageShadowBatchTime = 0.0f;

    // Reset rules epoch
    RulesEpoch.store(1);

    // Reset rules interface statistics
    if (Rules.GetInterface())
    {
        Rules->ResetStatistics();
    }

    // Reset service metrics
    ServiceMetrics.Reset();

    UE_LOG(LogSuspenseCoreEquipmentValidation, Log, TEXT("ResetService: Service reset complete"));

    ServiceMetrics.RecordSuccess();
    RECORD_SERVICE_METRIC("Validation.Service.Reset", 1);
}

FString USuspenseCoreEquipmentValidationService::GetServiceStats() const
{
    SCOPED_SERVICE_TIMER_CONST("Validation.GetServiceStats");
    EQUIPMENT_READ_LOCK(StatsLock);

    FString Stats = TEXT("=== Equipment Validation Service Statistics ===\n");
    Stats += FString::Printf(TEXT("Service State: %s\n"),
        *UEnum::GetValueAsString(ServiceState));

    const FTimespan Uptime = FDateTime::Now() - InitializationTime;
    Stats += FString::Printf(TEXT("Uptime: %d hours, %d minutes\n"),
        Uptime.GetHours(), Uptime.GetMinutes() % 60);

    Stats += FString::Printf(TEXT("Rules Epoch: %u\n"), RulesEpoch.load());
    Stats += FString::Printf(TEXT("Rules Implementation: %s\n"),
        Rules.GetInterface() ? TEXT("Coordinator") : TEXT("None"));

    Stats += FString::Printf(TEXT("\n--- Validations ---\n"));
    Stats += FString::Printf(TEXT("Total: %d\n"), TotalValidations.load());
    Stats += FString::Printf(TEXT("Passed: %d (%.1f%%)\n"),
        ValidationsPassed.load(),
        TotalValidations.load() > 0 ?
            (float)ValidationsPassed.load() / TotalValidations.load() * 100.0f : 0.0f);
    Stats += FString::Printf(TEXT("Failed: %d\n"), ValidationsFailed.load());

    Stats += FString::Printf(TEXT("\n--- Batch Processing ---\n"));
    Stats += FString::Printf(TEXT("Parallel Batches: %d\n"), ParallelBatches.load());
    Stats += FString::Printf(TEXT("Sequential Batches: %d\n"), SequentialBatches.load());
    Stats += FString::Printf(TEXT("Shadow Snapshot Batches: %d\n"), ShadowSnapshotBatches.load());
    Stats += FString::Printf(TEXT("Parallel Threshold: %d requests\n"), ParallelBatchThreshold);
    Stats += FString::Printf(TEXT("Max Parallel Threads: %d\n"), MaxParallelThreads);
    Stats += FString::Printf(TEXT("Shadow Snapshot Enabled: %s\n"), bUseShadowSnapshot ? TEXT("Yes") : TEXT("No"));

    Stats += FString::Printf(TEXT("\n--- Performance ---\n"));
    Stats += FString::Printf(TEXT("Cache Hit Rate: %.1f%%\n"),
        TotalValidations.load() > 0 ?
            (float)CacheHits.load() / TotalValidations.load() * 100.0f : 0.0f);
    Stats += FString::Printf(TEXT("Avg Validation Time: %.3f ms\n"), AverageValidationTime);
    Stats += FString::Printf(TEXT("Peak Validation Time: %.3f ms\n"), PeakValidationTime);
    Stats += FString::Printf(TEXT("Avg Parallel Batch Time: %.3f ms\n"), AverageParallelBatchTime);
    Stats += FString::Printf(TEXT("Peak Parallel Batch Time: %.3f ms\n"), PeakParallelBatchTime);
    Stats += FString::Printf(TEXT("Last Parallel Batch: %.3f ms\n"), LastParallelBatchTimeMs);
    Stats += FString::Printf(TEXT("Avg Shadow Batch Time: %.3f ms\n"), AverageShadowBatchTime);

    if (ResultCache.IsValid())
    {
        Stats += TEXT("\n--- Cache ---\n");
        Stats += ResultCache->DumpStats();
    }

    Stats += FString::Printf(TEXT("\n--- Custom Validators ---\n"));
    Stats += FString::Printf(TEXT("Registered: %d\n"), CustomValidators.Num());

    // Add service metrics
    Stats += ServiceMetrics.ToString(TEXT("EquipmentValidationService"));

    return Stats;
}

//========================================
// IEquipmentValidationService Implementation
//========================================

ISuspenseEquipmentRules* USuspenseCoreEquipmentValidationService::GetRulesEngine()
{
    SCOPED_SERVICE_TIMER("Validation.GetRulesEngine");
    return Rules.GetInterface();
}

bool USuspenseCoreEquipmentValidationService::RegisterValidator(const FGameplayTag& ValidatorTag, TFunction<bool(const void*)> Validator)
{
    SCOPED_SERVICE_TIMER("Validation.RegisterValidator");

    if (!Rules.GetInterface() || !Validator)
    {
        UE_LOG(LogSuspenseCoreEquipmentValidation, Warning,
            TEXT("RegisterValidator: Rules interface not available or invalid validator"));
        ServiceMetrics.RecordError();
        return false;
    }

    // Store custom validator
    {
        FScopeLock Lock(&ValidatorsLock);
        CustomValidators.Add(ValidatorTag, Validator);
    }

    // Create rule that will use the custom validator
    FEquipmentRule Rule;
    Rule.RuleTag = ValidatorTag;
    Rule.RuleExpression = FString::Printf(TEXT("CustomValidator_%s"), *ValidatorTag.ToString());
    Rule.Priority = 50;
    Rule.bIsStrict = true;
    Rule.Description = FText::Format(
        FText::FromString(TEXT("Custom validator: {0}")),
        FText::FromString(ValidatorTag.ToString())
    );

    // Register rule in rules interface
    const bool bSuccess = Rules->RegisterRule(Rule);

    if (bSuccess)
    {
        UE_LOG(LogSuspenseCoreEquipmentValidation, Log,
            TEXT("RegisterValidator: Registered custom validator for %s"),
            *ValidatorTag.ToString());
        ServiceMetrics.RecordSuccess();
        RECORD_SERVICE_METRIC("Validation.CustomValidators.Registered", 1);
    }
    else
    {
        ServiceMetrics.RecordError();
    }

    return bSuccess;
}

void USuspenseCoreEquipmentValidationService::ClearValidationCache()
{
    SCOPED_SERVICE_TIMER("Validation.ClearValidationCache");
    EQUIPMENT_WRITE_LOCK(CacheLock);

    if (ResultCache.IsValid())
    {
        ResultCache->Clear();
        RECORD_SERVICE_METRIC("Validation.Cache.Cleared", 1);
    }

    // Also clear rule cache if supported
    if (Rules.GetInterface())
    {
        Rules->ClearRuleCache();
    }

    UE_LOG(LogSuspenseCoreEquipmentValidation, Verbose, TEXT("Validation cache cleared"));
}

//========================================
// Coordination API
//========================================

FSlotValidationResult USuspenseCoreEquipmentValidationService::ValidateEquipmentOperation(const FEquipmentOperationRequest& Request)
{
    SCOPED_SERVICE_TIMER("Validation.ValidateEquipmentOperation");
    return ValidateSingleRequest(Request, true);
}

FSlotValidationResult USuspenseCoreEquipmentValidationService::ValidateSingleRequest(
    const FEquipmentOperationRequest& Request,
    bool bBroadcastEvents)
{
    SCOPED_SERVICE_TIMER("Validation.ValidateSingleRequest");
    const float StartTime = FPlatformTime::Seconds();

    TotalValidations.fetch_add(1);
    RECORD_SERVICE_METRIC("Validation.Requests.Total", 1);

    // Notify validation started (only on game thread)
    if (bBroadcastEvents)
    {
        if (IsInGameThread())
        {
            OnValidationStarted.Broadcast(Request);
        }
        else
        {
            // Buffer event for later dispatch
            const FEquipmentOperationRequest RequestCopy = Request;
            AsyncTask(ENamedThreads::GameThread, [this, RequestCopy]()
            {
                OnValidationStarted.Broadcast(RequestCopy);
            });
        }
    }

    // Generate cache key
    const uint32 CacheKey = GenerateCacheKey(Request);

    // Check cache with read lock
    FSlotValidationResult CachedResult;
    {
        EQUIPMENT_READ_LOCK(CacheLock);
        if (ResultCache.IsValid() && ResultCache->Get(CacheKey, CachedResult))
        {
            CacheHits.fetch_add(1);
            RECORD_SERVICE_METRIC("Validation.Cache.Hits", 1);

            // Publish event for cached result
            if (bBroadcastEvents)
            {
                const FSlotValidationResult ResultCopy = CachedResult;
                const FEquipmentOperationRequest RequestCopy = Request;

                if (IsInGameThread())
                {
                    PublishValidationEvent(
                        ResultCopy.bIsValid ?
                            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Validation.Passed")) :
                            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Validation.Failed")),
                        RequestCopy,
                        ResultCopy
                    );

                    OnValidationCompleted.Broadcast(ResultCopy);

                    if (!ResultCopy.bIsValid)
                    {
                        OnValidationFailed.Broadcast(RequestCopy, ResultCopy.ErrorMessage);
                    }
                }
                else
                {
                    AsyncTask(ENamedThreads::GameThread, [this, RequestCopy, ResultCopy]()
                    {
                        PublishValidationEvent(
                            ResultCopy.bIsValid ?
                                FGameplayTag::RequestGameplayTag(TEXT("Equipment.Validation.Passed")) :
                                FGameplayTag::RequestGameplayTag(TEXT("Equipment.Validation.Failed")),
                            RequestCopy,
                            ResultCopy
                        );

                        OnValidationCompleted.Broadcast(ResultCopy);

                        if (!ResultCopy.bIsValid)
                        {
                            OnValidationFailed.Broadcast(RequestCopy, ResultCopy.ErrorMessage);
                        }
                    });
                }
            }

            return CachedResult;
        }
    }

    RECORD_SERVICE_METRIC("Validation.Cache.Misses", 1);

    // Delegate validation to rules interface (coordinator)
    FSlotValidationResult Result;

    if (!Rules.GetInterface())
    {
        Result = FSlotValidationResult::Failure(
            FText::FromString(TEXT("Rules interface not available")),
            EEquipmentValidationFailure::SystemError
        );
        ServiceMetrics.RecordError();
    }
    else
    {
        // Check custom validators first
        bool bCustomValidationPassed = true;
        {
            FScopeLock Lock(&ValidatorsLock);
            for (const auto& ValidatorPair : CustomValidators)
            {
                if (ValidatorPair.Value)
                {
                    if (!ValidatorPair.Value(&Request))
                    {
                        bCustomValidationPassed = false;
                        Result = FSlotValidationResult::Failure(
                            FText::Format(
                                FText::FromString(TEXT("Custom validation failed: {0}")),
                                FText::FromString(ValidatorPair.Key.ToString())
                            ),
                            EEquipmentValidationFailure::RequirementsNotMet
                        );
                        RECORD_SERVICE_METRIC("Validation.CustomValidators.Failed", 1);
                        break;
                    }
                }
            }
        }

        // If custom validation passed, evaluate rules through coordinator
        if (bCustomValidationPassed)
        {
            const FRuleEvaluationResult RuleResult = Rules->EvaluateRules(Request);

            // Convert FRuleEvaluationResult to FSlotValidationResult
            Result.bIsValid = RuleResult.bPassed;
            Result.ErrorMessage = RuleResult.FailureReason;
            Result.ConfidenceScore = RuleResult.ConfidenceScore;
            Result.bCanOverride = false;

            // Fill context from details
            for (int32 i = 0; i < RuleResult.Details.Num(); i += 2)
            {
                if (i + 1 < RuleResult.Details.Num())
                {
                    Result.Context.Add(RuleResult.Details[i], RuleResult.Details[i + 1]);
                }
            }

            // Add rule type to context
            if (RuleResult.RuleType.IsValid())
            {
                Result.Context.Add(TEXT("RuleType"), RuleResult.RuleType.ToString());
            }

            // Determine failure type
            if (!RuleResult.bPassed)
            {
                Result.FailureType = DetermineFailureType(RuleResult);
                Result.ErrorTag = RuleResult.RuleType;
            }
        }
    }

    // Update statistics
    const float ValidationTime = (FPlatformTime::Seconds() - StartTime) * 1000.0f;

    {
        EQUIPMENT_WRITE_LOCK(StatsLock);

        if (Result.bIsValid)
        {
            ValidationsPassed.fetch_add(1);
            ServiceMetrics.RecordSuccess();
            RECORD_SERVICE_METRIC("Validation.Results.Passed", 1);
        }
        else
        {
            ValidationsFailed.fetch_add(1);
            ServiceMetrics.RecordError();
            RECORD_SERVICE_METRIC("Validation.Results.Failed", 1);
        }

        // Update average time using exponential moving average
        AverageValidationTime = AverageValidationTime * 0.9f + ValidationTime * 0.1f;
        PeakValidationTime = FMath::Max(PeakValidationTime, ValidationTime);
    }

    // Cache result with write lock
    {
        EQUIPMENT_WRITE_LOCK(CacheLock);
        if (ResultCache.IsValid())
        {
            ResultCache->Set(CacheKey, Result, CacheTTL);
        }
    }

    // Publish validation event
    if (bBroadcastEvents)
    {
        const FSlotValidationResult ResultCopy = Result;
        const FEquipmentOperationRequest RequestCopy = Request;

        if (IsInGameThread())
        {
            PublishValidationEvent(
                ResultCopy.bIsValid ?
                    FGameplayTag::RequestGameplayTag(TEXT("Equipment.Validation.Passed")) :
                    FGameplayTag::RequestGameplayTag(TEXT("Equipment.Validation.Failed")),
                RequestCopy,
                ResultCopy
            );

            OnValidationCompleted.Broadcast(ResultCopy);

            if (!ResultCopy.bIsValid)
            {
                OnValidationFailed.Broadcast(RequestCopy, ResultCopy.ErrorMessage);
            }
        }
        else
        {
            AsyncTask(ENamedThreads::GameThread, [this, RequestCopy, ResultCopy]()
            {
                PublishValidationEvent(
                    ResultCopy.bIsValid ?
                        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Validation.Passed")) :
                        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Validation.Failed")),
                    RequestCopy,
                    ResultCopy
                );

                OnValidationCompleted.Broadcast(ResultCopy);

                if (!ResultCopy.bIsValid)
                {
                    OnValidationFailed.Broadcast(RequestCopy, ResultCopy.ErrorMessage);
                }
            });
        }
    }

    if (bEnableDetailedLogging)
    {
        UE_LOG(LogSuspenseCoreEquipmentValidation, Verbose,
            TEXT("Validation completed in %.3f ms (Result: %s, Confidence: %.2f)"),
            ValidationTime,
            Result.bIsValid ? TEXT("PASS") : TEXT("FAIL"),
            Result.ConfidenceScore);
    }

    return Result;
}

FSlotValidationResult USuspenseCoreEquipmentValidationService::ValidateSingleRequestParallel(
    const FEquipmentOperationRequest& Request)
{
    // Thread-safe validation without events
    return ValidateSingleRequest(Request, false);
}

FSlotValidationResult USuspenseCoreEquipmentValidationService::ValidateInTransaction(
    const FEquipmentOperationRequest& Request,
    const FGuid& TransactionId)
{
    SCOPED_SERVICE_TIMER("Validation.ValidateInTransaction");

    // Validation service is read-only, no transaction management here
    // Just perform validation
    return ValidateSingleRequest(Request, true);
}



FSlotValidationBatchResult USuspenseCoreEquipmentValidationService::BatchValidateEx(
    const TArray<FEquipmentOperationRequest>& Operations,
    bool /*bFastPath*/,
    bool /*bServerAuthoritative*/,
    bool /*bStopOnFailure*/)
{
    // Reuse existing internal pipeline
    return ProcessBatchWithShadowSnapshot(Operations);
}

TArray<FSlotValidationResult> USuspenseCoreEquipmentValidationService::BatchValidate(
    const TArray<FEquipmentOperationRequest>& Requests,
    bool bAtomic)
{
    SCOPED_SERVICE_TIMER("Validation.BatchValidate");
    RECORD_SERVICE_METRIC("Validation.BatchValidate.Requests", Requests.Num());

    if (Requests.Num() == 0)
    {
        return TArray<FSlotValidationResult>();
    }

    const float BatchStartTime = FPlatformTime::Seconds();
    TArray<FSlotValidationResult> Results;

    // Primary path: use shadow snapshot for sequential validation
    if (bUseShadowSnapshot && Requests.Num() > 1)
    {
        const FSuspenseCoreBatchValidationReport Report = ProcessBatchWithShadowSnapshot(Requests);
        Results = Report.Results;

        // Handle atomic batch result
        if (bAtomic && !Report.bAllPassed)
        {
            // Mark all results as failed due to atomic rollback
            for (FSlotValidationResult& Result : Results)
            {
                if (Result.bIsValid)
                {
                    Result.bIsValid = false;
                    Result.ErrorMessage = FText::FromString(TEXT("Batch validation failed (atomic rollback)"));
                    Result.FailureType = EEquipmentValidationFailure::TransactionActive;
                }
            }
        }

        const float BatchTime = (FPlatformTime::Seconds() - BatchStartTime) * 1000.0f;
        RECORD_SERVICE_METRIC("Validation.BatchShadowAppliedMs", (int64)BatchTime);

        return Results;
    }

    // Fallback: check if we should use parallel processing
    const bool bUseParallel = bEnableParallelValidation &&
                       !bAtomic &&
                       Requests.Num() > ParallelBatchThreshold;

    if (bUseParallel)
    {
        RECORD_SERVICE_METRIC("Validation.BatchValidate.Parallel", 1);

        // Process batch in parallel
        ParallelBatches.fetch_add(1);
        Results = ProcessParallelBatch(Requests);

        // Update parallel batch timing
        const float BatchTime = (FPlatformTime::Seconds() - BatchStartTime) * 1000.0f;

        {
            EQUIPMENT_WRITE_LOCK(StatsLock);
            LastParallelBatchTimeMs = BatchTime;
            AverageParallelBatchTime = AverageParallelBatchTime * 0.9f + BatchTime * 0.1f;
            PeakParallelBatchTime = FMath::Max(PeakParallelBatchTime, BatchTime);
        }

        RECORD_SERVICE_METRIC("Validation.BatchValidate.ParallelTimeMs", (int64)BatchTime);

        if (bEnableDetailedLogging)
        {
            UE_LOG(LogSuspenseCoreEquipmentValidation, Log,
                TEXT("Parallel batch validation: %d requests in %.3f ms (%.2f req/ms)"),
                Requests.Num(), BatchTime, BatchTime > 0 ? Requests.Num() / BatchTime : 0.0f);
        }
    }
    else
    {
        RECORD_SERVICE_METRIC("Validation.BatchValidate.Sequential", 1);

        // Process batch sequentially
        SequentialBatches.fetch_add(1);
        Results = ProcessSequentialBatch(Requests, bAtomic);

        // Handle atomic batch result (without transactions in validation layer)
        if (bAtomic)
        {
            bool bAllPassed = true;
            for (const FSlotValidationResult& Result : Results)
            {
                if (!Result.bIsValid)
                {
                    bAllPassed = false;
                    break;
                }
            }

            if (!bAllPassed)
            {
                // Mark all results as failed due to atomic rollback
                for (FSlotValidationResult& Result : Results)
                {
                    if (Result.bIsValid)
                    {
                        Result.bIsValid = false;
                        Result.ErrorMessage = FText::FromString(TEXT("Batch validation failed (atomic rollback)"));
                        Result.FailureType = EEquipmentValidationFailure::TransactionActive;
                    }
                }
            }
        }
    }

    ServiceMetrics.RecordSuccess();
    return Results;
}

FSuspenseCoreBatchValidationReport USuspenseCoreEquipmentValidationService::BatchValidateWithReport(
    const TArray<FEquipmentOperationRequest>& Requests,
    bool bAtomic)
{
    SCOPED_SERVICE_TIMER("Validation.BatchValidateWithReport");

    FSuspenseCoreBatchValidationReport Report = ProcessBatchWithShadowSnapshot(Requests);

    if (bAtomic && !Report.bAllPassed)
    {
        // Add hard error for atomic failure
        Report.HardErrors.Add(FText::FromString(TEXT("Atomic batch validation failed - all operations rolled back")));

        // Mark all successful results as failed
        for (FSlotValidationResult& Result : Report.Results)
        {
            if (Result.bIsValid)
            {
                Result.bIsValid = false;
                Result.ErrorMessage = FText::FromString(TEXT("Rolled back due to atomic batch failure"));
                Result.FailureType = EEquipmentValidationFailure::TransactionActive;
            }
        }
    }

    return Report;
}

TArray<FSlotValidationResult> USuspenseCoreEquipmentValidationService::ProcessParallelBatch(
    const TArray<FEquipmentOperationRequest>& Requests)
{
    SCOPED_SERVICE_TIMER("Validation.ProcessParallelBatch");

    const int32 NumRequests = Requests.Num();
    TArray<FSlotValidationResult> Results;
    Results.SetNum(NumRequests);

    // Buffer for events to dispatch on game thread
    TArray<FSuspenseCoreBufferedValidationEvent> BufferedEvents;
    BufferedEvents.Reserve(NumRequests * 3);
    FCriticalSection EventBufferLock;

    // Use atomic counters for progress tracking
    std::atomic<int32> ProcessedCount{0};
    std::atomic<int32> PassedCount{0};
    std::atomic<int32> FailedCount{0};

    // Use safe overload of ParallelFor (background priority)
    ParallelFor(NumRequests, [&](int32 Index)
    {
        Results[Index] = ValidateSingleRequestParallel(Requests[Index]);

        {
            FScopeLock Lock(&EventBufferLock);

            FSuspenseCoreBufferedValidationEvent StartEvent;
            StartEvent.Type    = FSuspenseCoreBufferedValidationEvent::Started;
            StartEvent.Request = Requests[Index];
            BufferedEvents.Add(StartEvent);

            FSuspenseCoreBufferedValidationEvent CompleteEvent;
            CompleteEvent.Type    = FSuspenseCoreBufferedValidationEvent::Completed;
            CompleteEvent.Request = Requests[Index];
            CompleteEvent.Result  = Results[Index];
            BufferedEvents.Add(CompleteEvent);

            if (!Results[Index].bIsValid)
            {
                FSuspenseCoreBufferedValidationEvent FailEvent;
                FailEvent.Type    = FSuspenseCoreBufferedValidationEvent::Failed;
                FailEvent.Request = Requests[Index];
                FailEvent.Result  = Results[Index];
                BufferedEvents.Add(FailEvent);
            }
        }

        ProcessedCount.fetch_add(1);
        if (Results[Index].bIsValid) { PassedCount.fetch_add(1); }
        else                          { FailedCount.fetch_add(1); }

        if (bEnableDetailedLogging && ProcessedCount.load() % 100 == 0)
        {
            const double Now = FPlatformTime::Seconds();
            double Last = LastProgressLogTime.load(std::memory_order_relaxed);
            if (Now - Last > ProgressLogInterval)
            {
                if (LastProgressLogTime.compare_exchange_strong(Last, Now, std::memory_order_relaxed))
                {
                    UE_LOG(LogSuspenseCoreEquipmentValidation, Verbose,
                           TEXT("Parallel batch progress: %d/%d"), ProcessedCount.load(), NumRequests);
                }
            }
        }
    }, EParallelForFlags::BackgroundPriority);

    // Dispatch buffered events on game thread with move semantics to avoid copy
    if (BufferedEvents.Num() > 0)
    {
        AsyncTask(ENamedThreads::GameThread, [this, BufferedEventsToMove = MoveTemp(BufferedEvents)]()
        {
            DispatchBufferedEvents(BufferedEventsToMove);
        });
    }

    RECORD_SERVICE_METRIC("Validation.ProcessParallelBatch.Passed", PassedCount.load());
    RECORD_SERVICE_METRIC("Validation.ProcessParallelBatch.Failed", FailedCount.load());

    if (bEnableDetailedLogging)
    {
        UE_LOG(LogSuspenseCoreEquipmentValidation, Log,
            TEXT("Parallel batch complete: %d passed, %d failed"),
            PassedCount.load(), FailedCount.load());
    }

    return Results;
}

TArray<FSlotValidationResult> USuspenseCoreEquipmentValidationService::ProcessSequentialBatch(
    const TArray<FEquipmentOperationRequest>& Requests,
    bool bStopOnFailure)
{
    SCOPED_SERVICE_TIMER("Validation.ProcessSequentialBatch");

    TArray<FSlotValidationResult> Results;
    Results.Reserve(Requests.Num());

    for (const FEquipmentOperationRequest& Request : Requests)
    {
        const FSlotValidationResult Result = ValidateSingleRequest(Request, true);
        Results.Add(Result);

        if (!Result.bIsValid && bStopOnFailure)
        {
            RECORD_SERVICE_METRIC("Validation.ProcessSequentialBatch.StoppedEarly", 1);
            break;
        }
    }

    // Fill remaining results with failure if we stopped early
    if (bStopOnFailure && Results.Num() < Requests.Num())
    {
        const FSlotValidationResult SkippedResult = FSlotValidationResult::Failure(
            FText::FromString(TEXT("Validation skipped due to previous failure")),
            EEquipmentValidationFailure::TransactionActive
        );

        const int32 SkippedCount = Requests.Num() - Results.Num();
        RECORD_SERVICE_METRIC("Validation.ProcessSequentialBatch.Skipped", SkippedCount);

        while (Results.Num() < Requests.Num())
        {
            Results.Add(SkippedResult);
        }
    }

    return Results;
}

FSuspenseCoreBatchValidationReport USuspenseCoreEquipmentValidationService::ProcessBatchWithShadowSnapshot(
    const TArray<FEquipmentOperationRequest>& Requests)
{
    SCOPED_SERVICE_TIMER("Validation.ProcessBatchWithShadowSnapshot");

    const float StartTime = FPlatformTime::Seconds();
    ShadowSnapshotBatches.fetch_add(1);

    FSuspenseCoreBatchValidationReport Report;
    Report.TotalOperations = Requests.Num();
    Report.Results.Reserve(Requests.Num());

    // Initialize shadow snapshot from current equipment state
    FSuspenseCoreShadowEquipmentSnapshot ShadowSnapshot;
    if (DataProvider.GetInterface())
    {
        const TMap<int32, FSuspenseInventoryItemInstance> CurrentEquipment = DataProvider->GetAllEquippedItems();
        ShadowSnapshot.SlotItems = CurrentEquipment;

        // Compute initial weight
        for (const auto& SlotPair : CurrentEquipment)
        {
            const float ItemWeight = SlotPair.Value.GetRuntimeProperty(TEXT("Weight"), 1.0f);
            ShadowSnapshot.TotalWeight += ItemWeight * SlotPair.Value.Quantity;
            ShadowSnapshot.ItemQuantities.FindOrAdd(SlotPair.Value.ItemID) += SlotPair.Value.Quantity;
        }
    }

    // Process each operation sequentially against shadow state
    for (const FEquipmentOperationRequest& Request : Requests)
    {
        // Validate against current shadow state
        FSlotValidationResult Result = ValidateAgainstShadowSnapshot(Request, ShadowSnapshot);

        if (Result.bIsValid)
        {
            // Apply operation to shadow state
            if (!ShadowSnapshot.ApplyOperation(Request))
            {
                // Could not apply to shadow state (inconsistency)
                Result.bIsValid = false;
                Result.ErrorMessage = FText::FromString(TEXT("Failed to apply operation to validation snapshot"));
                Result.FailureType = EEquipmentValidationFailure::SystemError;
                Report.HardErrors.Add(FText::Format(
                    FText::FromString(TEXT("Operation {0}: snapshot apply failed")),
                    FText::AsNumber(Report.Results.Num())
                ));
            }
        }

        Report.Results.Add(Result);

        if (Result.bIsValid)
        {
            Report.PassedOperations++;
        }
        else
        {
            Report.FailedOperations++;
            Report.bAllPassed = false;

            // Classify error
            if (Result.FailureType == EEquipmentValidationFailure::SystemError ||
                Result.FailureType == EEquipmentValidationFailure::NetworkError)
            {
                Report.HardErrors.Add(Result.ErrorMessage);
            }
            else
            {
                Report.Warnings.Add(Result.ErrorMessage);
            }
        }
    }

    // Update timing stats
    const float BatchTime = (FPlatformTime::Seconds() - StartTime) * 1000.0f;
    {
        EQUIPMENT_WRITE_LOCK(StatsLock);
        AverageShadowBatchTime = AverageShadowBatchTime * 0.9f + BatchTime * 0.1f;
    }

    RECORD_SERVICE_METRIC("Validation.BatchShadowAppliedMs", (int64)BatchTime);

    if (bEnableDetailedLogging)
    {
        UE_LOG(LogSuspenseCoreEquipmentValidation, Log,
            TEXT("Shadow snapshot batch finished: %d/%d passed in %.3f ms"),
            Report.PassedOperations, Report.TotalOperations, BatchTime);
    }

    return Report;
}

FSlotValidationResult USuspenseCoreEquipmentValidationService::ValidateAgainstShadowSnapshot(
    const FEquipmentOperationRequest& Request,
    const FSuspenseCoreShadowEquipmentSnapshot& Snapshot)
{
    SCOPED_SERVICE_TIMER("Validation.ValidateAgainstShadowSnapshot");

    // Basic slot checks
    switch (Request.OperationType)
    {
        case EEquipmentOperationType::Equip:
        {
            if (Snapshot.IsSlotOccupied(Request.TargetSlotIndex))
            {
                return FSlotValidationResult::Failure(
                    FText::FromString(TEXT("Target slot occupied")),
                    EEquipmentValidationFailure::SlotOccupied);
            }
            break;
        }
        case EEquipmentOperationType::Unequip:
        {
            if (!Snapshot.IsSlotOccupied(Request.SourceSlotIndex))
            {
                return FSlotValidationResult::Failure(
                    FText::FromString(TEXT("Source slot empty")),
                    EEquipmentValidationFailure::InvalidSlot);
            }
            break;
        }
        case EEquipmentOperationType::Move:
        {
            if (!Snapshot.IsSlotOccupied(Request.SourceSlotIndex))
            {
                return FSlotValidationResult::Failure(
                    FText::FromString(TEXT("Source slot empty")),
                    EEquipmentValidationFailure::InvalidSlot);
            }
            if (Snapshot.IsSlotOccupied(Request.TargetSlotIndex))
            {
                return FSlotValidationResult::Failure(
                    FText::FromString(TEXT("Target slot occupied")),
                    EEquipmentValidationFailure::SlotOccupied);
            }
            break;
        }
        case EEquipmentOperationType::Swap:
        {
            if (!Snapshot.IsSlotOccupied(Request.SourceSlotIndex) ||
                !Snapshot.IsSlotOccupied(Request.TargetSlotIndex))
            {
                return FSlotValidationResult::Failure(
                    FText::FromString(TEXT("Both slots must be occupied to swap")),
                    EEquipmentValidationFailure::InvalidSlot);
            }
            break;
        }
        default:
            break;
    }

    // Build explicit rule context from snapshot
    FSuspenseRuleContext Ctx;
    Ctx.Character        = Request.Instigator.Get();
    Ctx.ItemInstance     = Request.ItemInstance;
    Ctx.TargetSlotIndex  = Request.TargetSlotIndex;
    Ctx.bForceOperation  = Request.bForceOperation;
    Ctx.CurrentItems.Reserve(Snapshot.SlotItems.Num());

    for (const auto& KV : Snapshot.SlotItems)
    {
        Ctx.CurrentItems.Add(KV.Value);
    }

    // Delegate to rules with explicit context (no live provider reads)
    if (Rules.GetInterface())
    {
        const FRuleEvaluationResult RuleRes = Rules->EvaluateRulesWithContext(Request, Ctx);

        FSlotValidationResult Out;
        Out.bIsValid         = RuleRes.bPassed;
        Out.ErrorMessage     = RuleRes.FailureReason;
        Out.ConfidenceScore  = RuleRes.ConfidenceScore;

        if (!RuleRes.bPassed)
        {
            Out.FailureType = DetermineFailureType(RuleRes);
            Out.ErrorTag    = RuleRes.RuleType;
        }

        // Transfer details into context map
        for (int32 i = 0; i + 1 < RuleRes.Details.Num(); i += 2)
        {
            Out.Context.Add(RuleRes.Details[i], RuleRes.Details[i + 1]);
        }
        return Out;
    }

    // No rules -> basic checks already done
    return FSlotValidationResult::Success();
}

void USuspenseCoreEquipmentValidationService::DispatchBufferedEvents(const TArray<FSuspenseCoreBufferedValidationEvent>& Events)
{
    if (!IsInGameThread())
    {
        UE_LOG(LogSuspenseCoreEquipmentValidation, Error, TEXT("DispatchBufferedEvents called from non-game thread"));
        return;
    }

    for (const FSuspenseCoreBufferedValidationEvent& Event : Events)
    {
        switch (Event.Type)
        {
            case FSuspenseCoreBufferedValidationEvent::Started:
                OnValidationStarted.Broadcast(Event.Request);
                break;

            case FSuspenseCoreBufferedValidationEvent::Completed:
                OnValidationCompleted.Broadcast(Event.Result);
                PublishValidationEvent(
                    Event.Result.bIsValid ?
                        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Validation.Passed")) :
                        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Validation.Failed")),
                    Event.Request,
                    Event.Result
                );
                break;

            case FSuspenseCoreBufferedValidationEvent::Failed:
                OnValidationFailed.Broadcast(Event.Request, Event.Result.ErrorMessage);
                break;

            case FSuspenseCoreBufferedValidationEvent::Custom:
                if (Event.CustomEventTag.IsValid())
                {
                    PublishValidationEvent(Event.CustomEventTag, Event.Request, Event.Result);
                }
                break;
        }
    }
}

void USuspenseCoreEquipmentValidationService::ValidateAsync(
    const FEquipmentOperationRequest& Request,
    TFunction<void(const FSlotValidationResult&)> Callback)
{
    SCOPED_SERVICE_TIMER("Validation.ValidateAsync");

    if (!Callback)
    {
        return;
    }

    // Execute validation in thread pool
    Async(EAsyncExecution::ThreadPool, [this, Request, Callback]()
    {
        const FSlotValidationResult Result = ValidateSingleRequestParallel(Request);

        // Call callback on game thread
        AsyncTask(ENamedThreads::GameThread, [Callback, Result]()
        {
            Callback(Result);
        });
    });

    ServiceMetrics.RecordSuccess();
}

bool USuspenseCoreEquipmentValidationService::ExportMetricsToCSV(const FString& AbsoluteFilePath) const
{
    SCOPED_SERVICE_TIMER_CONST("Validation.ExportMetricsToCSV");
    return ServiceMetrics.ExportToCSV(AbsoluteFilePath, TEXT("EquipmentValidationService"));
}

//========================================
// Protected Methods
//========================================

bool USuspenseCoreEquipmentValidationService::InitializeDependencies()
{
    SCOPED_SERVICE_TIMER("Validation.InitializeDependencies");

    // ✅ Используем сохранённую ссылку на ServiceLocator
    USuspenseCoreEquipmentServiceLocator* ServiceLocator = ServiceLocatorRef.Get();

    if (!ServiceLocator)
    {
        UE_LOG(LogSuspenseCoreEquipmentValidation, Error,
            TEXT("InitializeDependencies: ServiceLocator not available"));
        return false;
    }

    UE_LOG(LogSuspenseCoreEquipmentValidation, Verbose,
        TEXT("InitializeDependencies: ServiceLocator reference valid, resolving dependencies..."));

    // Get DataService from ServiceLocator
    UObject* DataServiceObject = ServiceLocator->GetService(
        FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Data"))
    );

    if (!DataServiceObject)
    {
        UE_LOG(LogSuspenseCoreEquipmentValidation, Error,
            TEXT("InitializeDependencies: DataService not available in ServiceLocator"));
        return false;
    }

    UE_LOG(LogSuspenseCoreEquipmentValidation, Verbose,
        TEXT("InitializeDependencies: DataService retrieved: %s (class: %s)"),
        *DataServiceObject->GetName(),
        *DataServiceObject->GetClass()->GetName());

    // Cast to IEquipmentDataService
    if (!DataServiceObject->GetClass()->ImplementsInterface(UEquipmentDataService::StaticClass()))
    {
        UE_LOG(LogSuspenseCoreEquipmentValidation, Error,
            TEXT("InitializeDependencies: DataService does not implement IEquipmentDataService"));
        return false;
    }

    IEquipmentDataService* DataServiceInterface = Cast<IEquipmentDataService>(DataServiceObject);
    if (!DataServiceInterface)
    {
        UE_LOG(LogSuspenseCoreEquipmentValidation, Error,
            TEXT("InitializeDependencies: Failed to cast DataService to IEquipmentDataService"));
        return false;
    }

    UE_LOG(LogSuspenseCoreEquipmentValidation, Verbose,
        TEXT("InitializeDependencies: Successfully cast to IEquipmentDataService"));

    // ✅ КРИТИЧЕСКОЕ ИЗМЕНЕНИЕ: DataProvider теперь ОПЦИОНАЛЬНЫЙ
    // В stateless режиме DataService не имеет компонентов и GetDataProvider() вернёт null
    ISuspenseEquipmentDataProvider* RawDataProvider = DataServiceInterface->GetDataProvider();

    if (RawDataProvider)
    {
        UObject* DataProviderObject = Cast<UObject>(RawDataProvider);
        if (DataProviderObject)
        {
            DataProvider.SetObject(DataProviderObject);
            DataProvider.SetInterface(RawDataProvider);

            UE_LOG(LogSuspenseCoreEquipmentValidation, Log,
                TEXT("InitializeDependencies: DataProvider available (STATEFUL mode)"));
            UE_LOG(LogSuspenseCoreEquipmentValidation, Log,
                TEXT("  DataProvider: %s"), *DataProviderObject->GetName());
        }
        else
        {
            UE_LOG(LogSuspenseCoreEquipmentValidation, Warning,
                TEXT("InitializeDependencies: DataProvider is not a UObject (ignoring)"));
        }
    }
    else
    {
        // ✅ DataProvider отсутствует - это НормАльно для stateless режима
        UE_LOG(LogSuspenseCoreEquipmentValidation, Warning,
            TEXT("InitializeDependencies: DataProvider not available (STATELESS mode)"));
        UE_LOG(LogSuspenseCoreEquipmentValidation, Warning,
            TEXT("  ValidationService will work in stateless mode"));
        UE_LOG(LogSuspenseCoreEquipmentValidation, Warning,
            TEXT("  Context-based validation will be limited"));
    }

    // Try to get TransactionManager (optional)
    ISuspenseTransactionManager* RawTransactionManager = DataServiceInterface->GetTransactionManager();
    if (RawTransactionManager)
    {
        UObject* TransactionManagerObject = Cast<UObject>(RawTransactionManager);
        if (TransactionManagerObject)
        {
            TransactionManager.SetObject(TransactionManagerObject);
            TransactionManager.SetInterface(RawTransactionManager);

            UE_LOG(LogSuspenseCoreEquipmentValidation, Log,
                TEXT("InitializeDependencies: TransactionManager available"));
        }
    }
    else
    {
        UE_LOG(LogSuspenseCoreEquipmentValidation, Verbose,
            TEXT("InitializeDependencies: TransactionManager not available (optional)"));
    }

    // ✅ SUCCESS: ValidationService может работать БЕЗ DataProvider
    // Зависимость от DataService достаточна (для получения ItemManager и т.д.)
    UE_LOG(LogSuspenseCoreEquipmentValidation, Log,
        TEXT("InitializeDependencies: ✅ Dependencies initialized successfully"));
    UE_LOG(LogSuspenseCoreEquipmentValidation, Log,
        TEXT("  - Mode: %s"),
        DataProvider.GetInterface() ? TEXT("STATEFUL (with DataProvider)") : TEXT("STATELESS (no DataProvider)"));
    UE_LOG(LogSuspenseCoreEquipmentValidation, Log,
        TEXT("  - DataProvider: %s"),
        DataProvider.GetInterface() ? TEXT("Available") : TEXT("Not Available"));
    UE_LOG(LogSuspenseCoreEquipmentValidation, Log,
        TEXT("  - TransactionManager: %s"),
        TransactionManager.GetInterface() ? TEXT("Available") : TEXT("Not Available"));

    return true; // ✅ SUCCESS даже без DataProvider
}

void USuspenseCoreEquipmentValidationService::SetupEventSubscriptions()
{
    auto EventBus = FSuspenseCoreEquipmentEventBus::Get();
    if (!EventBus.IsValid())
    {
        UE_LOG(LogSuspenseCoreEquipmentValidation, Warning, TEXT("SetupEventSubscriptions: EventBus not available"));
        return;
    }

    // Subscribe to cache invalidation events
    EventScope.Subscribe(
        FGameplayTag::RequestGameplayTag(TEXT("Cache.Invalidate.Validation")),
        FEventHandlerDelegate::CreateLambda([this](const FSuspenseCoreEquipmentEventData& EventData)
        {
            ClearValidationCache();
        })
    );

    // Subscribe to rules change events
    EventScope.Subscribe(
        FGameplayTag::RequestGameplayTag(TEXT("Rules.Changed")),
        FEventHandlerDelegate::CreateLambda([this](const FSuspenseCoreEquipmentEventData& EventData)
        {
            OnRulesOrConfigChanged();
        })
    );

    // Subscribe to configuration changes
    EventScope.Subscribe(
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Configuration.Changed")),
        FEventHandlerDelegate::CreateLambda([this](const FSuspenseCoreEquipmentEventData& EventData)
        {
            OnRulesOrConfigChanged();

            if (EventData.HasMetadata(TEXT("ReloadRules")) && Rules.GetInterface())
            {
                Rules->ClearRuleCache();
            }
        })
    );

    // Subscribe to transaction events (for cache invalidation only)
    EventScope.Subscribe(
        FGameplayTag::RequestGameplayTag(TEXT("Transaction.RolledBack")),
        FEventHandlerDelegate::CreateLambda([this](const FSuspenseCoreEquipmentEventData& EventData)
        {
            ClearValidationCache();
        })
    );

    UE_LOG(LogSuspenseCoreEquipmentValidation, Log, TEXT("SetupEventSubscriptions: Event handlers registered"));
}

void USuspenseCoreEquipmentValidationService::OnRulesOrConfigChanged()
{
    // Increment rules epoch
    const uint32 NewEpoch = RulesEpoch.fetch_add(1) + 1;

    // Clear cache
    ClearValidationCache();

    UE_LOG(LogSuspenseCoreEquipmentValidation, Log,
        TEXT("OnRulesOrConfigChanged: Rules epoch incremented to %u, cache cleared"),
        NewEpoch);

    RECORD_SERVICE_METRIC("Validation.RulesEpoch.Incremented", 1);
}

uint32 USuspenseCoreEquipmentValidationService::GenerateCacheKey(const FEquipmentOperationRequest& Request) const
{
    uint32 Key = GetTypeHash(Request.ItemInstance.ItemID);
    Key = HashCombine(Key, static_cast<uint32>(Request.ItemInstance.Quantity));
    Key = HashCombine(Key, static_cast<uint32>(Request.TargetSlotIndex));
    Key = HashCombine(Key, static_cast<uint32>(Request.SourceSlotIndex));
    Key = HashCombine(Key, static_cast<uint32>(Request.OperationType));
    Key = HashCombine(Key, Request.bForceOperation ? 1u : 0u);

    // Add stable actor identifier instead of pointer
    if (Request.Instigator.IsValid())
    {
        Key = HashCombine(Key, GetStableActorIdentifier(Request.Instigator.Get()));
    }

    // Add sequence number for network operations
    Key = HashCombine(Key, Request.SequenceNumber);

    // Add priority as it may affect validation rules
    Key = HashCombine(Key, static_cast<uint32>(Request.Priority));

    // Add rules epoch for cache invalidation on rules change
    Key = HashCombine(Key, RulesEpoch.load());

    return Key;
}

uint32 USuspenseCoreEquipmentValidationService::GetStableActorIdentifier(AActor* Actor) const
{
    if (!Actor)
    {
        return 0u;
    }

    // Для пешек игроков используем байты сетевого идентификатора (стабильнее, чем ToString)
    if (const APawn* Pawn = Cast<APawn>(Actor))
    {
        if (const APlayerState* PS = Pawn->GetPlayerState())
        {
            const FUniqueNetIdRepl& IdRepl = PS->GetUniqueId();
            if (IdRepl.IsValid())
            {
                // Достаём «сырое» FUniqueNetId
                TSharedPtr<const FUniqueNetId> NetId = IdRepl.GetUniqueNetId();
                if (NetId.IsValid())
                {
                    const uint8* Bytes = NetId->GetBytes();
                    const int32  Size  = NetId->GetSize();

                    if (Bytes && Size > 0)
                    {
                        // CRC32 по байтам — быстрый, стабильный ключ
                        return FCrc::MemCrc32(Bytes, Size);
                    }
                }
            }

            // Доп. стабильный fallback для PS: используем имя игрока
            const FString& Name = PS->GetPlayerName();
            if (!Name.IsEmpty())
            {
                return GetTypeHash(Name);
            }
        }
    }

    // Общий fallback для любых акторов
    return static_cast<uint32>(Actor->GetUniqueID());
}

void USuspenseCoreEquipmentValidationService::PublishValidationEvent(
    const FGameplayTag& EventType,
    const FEquipmentOperationRequest& Request,
    const FSlotValidationResult& Result)
{
    if (!IsInGameThread())
    {
        UE_LOG(LogSuspenseCoreEquipmentValidation, Error, TEXT("PublishValidationEvent called from non-game thread"));
        return;
    }

    FSuspenseCoreEquipmentEventData EventData;
    EventData.EventType = EventType;
    EventData.Source = this;
    EventData.Target = Request.Instigator.Get();
    EventData.Timestamp = FPlatformTime::Seconds();
    EventData.Priority = Result.FailureType == EEquipmentValidationFailure::SystemError ?
        EEventPriority::High : EEventPriority::Normal;

    // Add metadata
    EventData.AddMetadata(TEXT("OperationId"), Request.OperationId.ToString());
    EventData.AddMetadata(TEXT("OperationType"), UEnum::GetValueAsString(Request.OperationType));
    EventData.AddMetadata(TEXT("ItemID"), Request.ItemInstance.ItemID.ToString());
    EventData.AddMetadata(TEXT("ValidationPassed"), Result.bIsValid ? TEXT("true") : TEXT("false"));
    EventData.AddMetadata(TEXT("Confidence"), FString::Printf(TEXT("%.2f"), Result.ConfidenceScore));
    EventData.AddMetadata(TEXT("RulesEpoch"), FString::Printf(TEXT("%u"), RulesEpoch.load()));

    if (!Result.bIsValid)
    {
        EventData.AddMetadata(TEXT("FailureType"), UEnum::GetValueAsString(Result.FailureType));
        EventData.AddMetadata(TEXT("ErrorMessage"), Result.ErrorMessage.ToString());
    }

    // Publish event
    if (auto EventBus = FSuspenseCoreEquipmentEventBus::Get())
    {
        EventBus->Broadcast(EventData);
    }
}

EEquipmentValidationFailure USuspenseCoreEquipmentValidationService::DetermineFailureType(const FRuleEvaluationResult& RuleResult) const
{
    // Check rule type via tag first
    if (RuleResult.RuleType.IsValid())
    {
        const FString RuleTypeString = RuleResult.RuleType.ToString();

        if (RuleTypeString.Contains(TEXT("Weight")))
        {
            return EEquipmentValidationFailure::WeightLimit;
        }
        else if (RuleTypeString.Contains(TEXT("Level")))
        {
            return EEquipmentValidationFailure::LevelRequirement;
        }
        else if (RuleTypeString.Contains(TEXT("Slot")))
        {
            return EEquipmentValidationFailure::InvalidSlot;
        }
        else if (RuleTypeString.Contains(TEXT("Class")))
        {
            return EEquipmentValidationFailure::ClassRestriction;
        }
        else if (RuleTypeString.Contains(TEXT("Unique")))
        {
            return EEquipmentValidationFailure::UniqueConstraint;
        }
        else if (RuleTypeString.Contains(TEXT("Conflict")))
        {
            return EEquipmentValidationFailure::ConflictingItem;
        }
        else if (RuleTypeString.Contains(TEXT("Cooldown")))
        {
            return EEquipmentValidationFailure::CooldownActive;
        }
        else if (RuleTypeString.Contains(TEXT("Transaction")))
        {
            return EEquipmentValidationFailure::TransactionActive;
        }
        else if (RuleTypeString.Contains(TEXT("Network")))
        {
            return EEquipmentValidationFailure::NetworkError;
        }
    }

    // Analyze error text if tag didn't help
    const FString FailureString = RuleResult.FailureReason.ToString();

    if (FailureString.Contains(TEXT("Weight")) || FailureString.Contains(TEXT("weight")))
    {
        return EEquipmentValidationFailure::WeightLimit;
    }
    else if (FailureString.Contains(TEXT("Level")) || FailureString.Contains(TEXT("level")))
    {
        return EEquipmentValidationFailure::LevelRequirement;
    }
    else if (FailureString.Contains(TEXT("Slot")) || FailureString.Contains(TEXT("slot")))
    {
        return EEquipmentValidationFailure::InvalidSlot;
    }
    else if (FailureString.Contains(TEXT("Requirements")) || FailureString.Contains(TEXT("requirements")))
    {
        return EEquipmentValidationFailure::RequirementsNotMet;
    }
    else if (FailureString.Contains(TEXT("Class")) || FailureString.Contains(TEXT("class")))
    {
        return EEquipmentValidationFailure::ClassRestriction;
    }
    else if (FailureString.Contains(TEXT("Conflict")) || FailureString.Contains(TEXT("conflict")))
    {
        return EEquipmentValidationFailure::ConflictingItem;
    }
    else if (FailureString.Contains(TEXT("Incompatible")) || FailureString.Contains(TEXT("incompatible")))
    {
        return EEquipmentValidationFailure::IncompatibleType;
    }
    else if (FailureString.Contains(TEXT("Occupied")) || FailureString.Contains(TEXT("occupied")))
    {
        return EEquipmentValidationFailure::SlotOccupied;
    }
    else if (FailureString.Contains(TEXT("Cooldown")) || FailureString.Contains(TEXT("cooldown")))
    {
        return EEquipmentValidationFailure::CooldownActive;
    }
    else if (FailureString.Contains(TEXT("Transaction")) || FailureString.Contains(TEXT("transaction")))
    {
        return EEquipmentValidationFailure::TransactionActive;
    }
    else if (FailureString.Contains(TEXT("Network")) || FailureString.Contains(TEXT("network")))
    {
        return EEquipmentValidationFailure::NetworkError;
    }

    // Default fallback
    return EEquipmentValidationFailure::RequirementsNotMet;
}
