// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreEquipmentRulesService.h"
#include "SuspenseCore/Components/Rules/SuspenseCoreRulesCoordinator.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"

DEFINE_LOG_CATEGORY(LogSuspenseCoreEquipmentRules);

//========================================
// FRulesServiceConfig
//========================================

FRulesServiceConfig FRulesServiceConfig::LoadFromConfig(const FString& ConfigSection)
{
    FRulesServiceConfig OutConfig;

    if (!GConfig) return OutConfig;

    GConfig->GetBool(*ConfigSection, TEXT("bEnableCaching"), OutConfig.bEnableCaching, GGameIni);
    GConfig->GetFloat(*ConfigSection, TEXT("CacheTTLSeconds"), OutConfig.CacheTTLSeconds, GGameIni);
    GConfig->GetInt(*ConfigSection, TEXT("MaxCacheEntries"), OutConfig.MaxCacheEntries, GGameIni);
    GConfig->GetBool(*ConfigSection, TEXT("bEnableParallelEvaluation"), OutConfig.bEnableParallelEvaluation, GGameIni);
    GConfig->GetBool(*ConfigSection, TEXT("bBroadcastValidationEvents"), OutConfig.bBroadcastValidationEvents, GGameIni);
    GConfig->GetBool(*ConfigSection, TEXT("bLogDetailedResults"), OutConfig.bLogDetailedResults, GGameIni);

    return OutConfig;
}

//========================================
// FRulesServiceMetrics
//========================================

FString FRulesServiceMetrics::ToString() const
{
    return FString::Printf(
        TEXT("=== Rules Service Metrics ===\n")
        TEXT("Total Evaluations: %llu\n")
        TEXT("Cache Hits: %llu (%.1f%%)\n")
        TEXT("Cache Misses: %llu\n")
        TEXT("Passed: %llu\n")
        TEXT("Failed: %llu\n")
        TEXT("Avg Time: %llu us\n")
        TEXT("Peak Time: %llu us"),
        TotalEvaluations.Load(),
        CacheHits.Load(),
        GetCacheHitRate(),
        CacheMisses.Load(),
        ValidationsPassed.Load(),
        ValidationsFailed.Load(),
        AverageEvaluationTimeUs.Load(),
        PeakEvaluationTimeUs.Load()
    );
}

void FRulesServiceMetrics::Reset()
{
    TotalEvaluations.Store(0);
    CacheHits.Store(0);
    CacheMisses.Store(0);
    ValidationsPassed.Store(0);
    ValidationsFailed.Store(0);
    AverageEvaluationTimeUs.Store(0);
    PeakEvaluationTimeUs.Store(0);
}

//========================================
// USuspenseCoreEquipmentRulesService
//========================================

USuspenseCoreEquipmentRulesService::USuspenseCoreEquipmentRulesService()
{
    Config = FRulesServiceConfig::LoadFromConfig();
}

USuspenseCoreEquipmentRulesService::~USuspenseCoreEquipmentRulesService()
{
    ShutdownService(true);
}

//========================================
// IEquipmentService Implementation
//========================================

bool USuspenseCoreEquipmentRulesService::InitializeService(const FServiceInitParams& Params)
{
    SCOPED_SERVICE_TIMER("RulesService::Initialize");

    if (ServiceState != EServiceLifecycleState::Uninitialized)
    {
        UE_LOG(LogSuspenseCoreEquipmentRules, Warning, TEXT("Service already initialized"));
        return ServiceState == EServiceLifecycleState::Ready;
    }

    ServiceState = EServiceLifecycleState::Initializing;
    ServiceParams = Params;

    UE_LOG(LogSuspenseCoreEquipmentRules, Log, TEXT(">>> RulesService: Initializing..."));

    // Load configuration
    Config = FRulesServiceConfig::LoadFromConfig();

    // Create and initialize RulesCoordinator
    RulesCoordinator = NewObject<USuspenseCoreRulesCoordinator>(this, TEXT("RulesCoordinator"));
    if (!RulesCoordinator)
    {
        UE_LOG(LogSuspenseCoreEquipmentRules, Error, TEXT("Failed to create RulesCoordinator"));
        ServiceState = EServiceLifecycleState::Error;
        return false;
    }

    // Initialize coordinator (DataProvider is optional)
    if (!RulesCoordinator->Initialize(nullptr))
    {
        UE_LOG(LogSuspenseCoreEquipmentRules, Warning, TEXT("RulesCoordinator initialized without DataProvider"));
    }

    // Setup EventBus integration
    SetupEventBus();

    ServiceState = EServiceLifecycleState::Ready;
    UE_LOG(LogSuspenseCoreEquipmentRules, Log,
        TEXT("<<< RulesService: Initialized (Cache=%s, Events=%s)"),
        Config.bEnableCaching ? TEXT("ON") : TEXT("OFF"),
        Config.bBroadcastValidationEvents ? TEXT("ON") : TEXT("OFF"));

    return true;
}

bool USuspenseCoreEquipmentRulesService::ShutdownService(bool bForce)
{
    if (ServiceState == EServiceLifecycleState::Shutdown)
    {
        return true;
    }

    UE_LOG(LogSuspenseCoreEquipmentRules, Log, TEXT(">>> RulesService: Shutting down..."));

    // Teardown EventBus
    TeardownEventBus();

    // Clear cache
    {
        FScopeLock Lock(&CacheLock);
        ResultCache.Empty();
    }

    // RulesCoordinator will be cleaned up by GC (UPROPERTY)
    RulesCoordinator = nullptr;

    ServiceState = EServiceLifecycleState::Shutdown;
    UE_LOG(LogSuspenseCoreEquipmentRules, Log, TEXT("<<< RulesService: Shutdown complete"));

    return true;
}

FGameplayTag USuspenseCoreEquipmentRulesService::GetServiceTag() const
{
    using namespace SuspenseCoreEquipmentTags;
    return Service::TAG_Service_Equipment_Rules;
}

FGameplayTagContainer USuspenseCoreEquipmentRulesService::GetRequiredDependencies() const
{
    // RulesService has no dependencies - it's a leaf service
    return FGameplayTagContainer();
}

bool USuspenseCoreEquipmentRulesService::ValidateService(TArray<FText>& OutErrors) const
{
    bool bValid = true;

    if (!RulesCoordinator)
    {
        OutErrors.Add(FText::FromString(TEXT("RulesCoordinator not created")));
        bValid = false;
    }

    return bValid;
}

void USuspenseCoreEquipmentRulesService::ResetService()
{
    // Clear cache
    {
        FScopeLock Lock(&CacheLock);
        ResultCache.Empty();
    }

    // Reset coordinator statistics
    if (RulesCoordinator)
    {
        RulesCoordinator->ResetStatistics();
    }

    // Reset metrics
    Metrics.Reset();

    UE_LOG(LogSuspenseCoreEquipmentRules, Log, TEXT("RulesService: Reset complete"));
}

FString USuspenseCoreEquipmentRulesService::GetServiceStats() const
{
    FString CoordinatorStats;
    if (RulesCoordinator)
    {
        const TMap<FString, FString> Stats = RulesCoordinator->GetExecutionStatistics();
        for (const auto& Pair : Stats)
        {
            CoordinatorStats += FString::Printf(TEXT("  %s: %s\n"), *Pair.Key, *Pair.Value);
        }
    }

    return FString::Printf(
        TEXT("RulesService Stats:\n")
        TEXT("  Cache Entries: %d\n")
        TEXT("%s\n")
        TEXT("Coordinator:\n%s"),
        ResultCache.Num(),
        *Metrics.ToString(),
        *CoordinatorStats
    );
}

//========================================
// ISuspenseCoreEquipmentRules Implementation (Delegated)
//========================================

FRuleEvaluationResult USuspenseCoreEquipmentRulesService::EvaluateRules(const FEquipmentOperationRequest& Operation) const
{
    if (!RulesCoordinator || !IsServiceReady())
    {
        FRuleEvaluationResult ErrorResult;
        ErrorResult.bPassed = false;
        ErrorResult.FailureReason = TEXT("RulesService not ready");
        return ErrorResult;
    }

    const double StartTime = FPlatformTime::Seconds();
    Metrics.TotalEvaluations++;

    // Broadcast start event
    if (Config.bBroadcastValidationEvents)
    {
        BroadcastValidationStarted(Operation);
    }

    // Check cache
    if (Config.bEnableCaching)
    {
        const uint32 RequestHash = ComputeRequestHash(Operation);
        FRuleEvaluationResult CachedResult;
        if (TryGetCachedResult(RequestHash, CachedResult))
        {
            Metrics.CacheHits++;
            return CachedResult;
        }
        Metrics.CacheMisses++;
    }

    // Delegate to coordinator
    FRuleEvaluationResult Result = RulesCoordinator->EvaluateRules(Operation);

    // Cache result
    if (Config.bEnableCaching)
    {
        const uint32 RequestHash = ComputeRequestHash(Operation);
        CacheResult(RequestHash, Result);
    }

    // Update metrics
    UpdateMetrics(StartTime, Result.bPassed);

    // Broadcast result event
    if (Config.bBroadcastValidationEvents)
    {
        BroadcastValidationResult(Operation, Result);
    }

    // Log if configured
    if (Config.bLogDetailedResults)
    {
        UE_LOG(LogSuspenseCoreEquipmentRules, Log,
            TEXT("EvaluateRules: %s - %s"),
            Result.bPassed ? TEXT("PASSED") : TEXT("FAILED"),
            *Result.FailureReason);
    }

    return Result;
}

FRuleEvaluationResult USuspenseCoreEquipmentRulesService::EvaluateRulesWithContext(
    const FEquipmentOperationRequest& Operation,
    const FSuspenseCoreRuleContext& Context) const
{
    if (!RulesCoordinator || !IsServiceReady())
    {
        FRuleEvaluationResult ErrorResult;
        ErrorResult.bPassed = false;
        ErrorResult.FailureReason = TEXT("RulesService not ready");
        return ErrorResult;
    }

    const double StartTime = FPlatformTime::Seconds();
    Metrics.TotalEvaluations++;

    // Note: No caching for context-based evaluation (context may vary)
    FRuleEvaluationResult Result = RulesCoordinator->EvaluateRulesWithContext(Operation, Context);

    UpdateMetrics(StartTime, Result.bPassed);

    return Result;
}

FRuleEvaluationResult USuspenseCoreEquipmentRulesService::CheckItemCompatibility(
    const FSuspenseCoreInventoryItemInstance& ItemInstance,
    const FEquipmentSlotConfig& SlotConfig) const
{
    if (!RulesCoordinator) return FRuleEvaluationResult();
    return RulesCoordinator->CheckItemCompatibility(ItemInstance, SlotConfig);
}

FRuleEvaluationResult USuspenseCoreEquipmentRulesService::CheckCharacterRequirements(
    const AActor* Character,
    const FSuspenseCoreInventoryItemInstance& ItemInstance) const
{
    if (!RulesCoordinator) return FRuleEvaluationResult();
    return RulesCoordinator->CheckCharacterRequirements(Character, ItemInstance);
}

FRuleEvaluationResult USuspenseCoreEquipmentRulesService::CheckWeightLimit(
    float CurrentWeight,
    float AdditionalWeight) const
{
    if (!RulesCoordinator) return FRuleEvaluationResult();
    return RulesCoordinator->CheckWeightLimit(CurrentWeight, AdditionalWeight);
}

FRuleEvaluationResult USuspenseCoreEquipmentRulesService::CheckConflictingEquipment(
    const TArray<FSuspenseCoreInventoryItemInstance>& ExistingItems,
    const FSuspenseCoreInventoryItemInstance& NewItem) const
{
    if (!RulesCoordinator) return FRuleEvaluationResult();
    return RulesCoordinator->CheckConflictingEquipment(ExistingItems, NewItem);
}

TArray<FEquipmentRule> USuspenseCoreEquipmentRulesService::GetActiveRules() const
{
    if (!RulesCoordinator) return TArray<FEquipmentRule>();
    return RulesCoordinator->GetActiveRules();
}

bool USuspenseCoreEquipmentRulesService::RegisterRule(const FEquipmentRule& Rule)
{
    if (!RulesCoordinator) return false;
    InvalidateCache();
    return RulesCoordinator->RegisterRule(Rule);
}

bool USuspenseCoreEquipmentRulesService::UnregisterRule(const FGameplayTag& RuleTag)
{
    if (!RulesCoordinator) return false;
    InvalidateCache();
    return RulesCoordinator->UnregisterRule(RuleTag);
}

bool USuspenseCoreEquipmentRulesService::SetRuleEnabled(const FGameplayTag& RuleTag, bool bEnabled)
{
    if (!RulesCoordinator) return false;
    InvalidateCache();
    return RulesCoordinator->SetRuleEnabled(RuleTag, bEnabled);
}

FString USuspenseCoreEquipmentRulesService::GenerateComplianceReport(const FEquipmentStateSnapshot& CurrentState) const
{
    if (!RulesCoordinator) return TEXT("RulesService not available");
    return RulesCoordinator->GenerateComplianceReport(CurrentState);
}

void USuspenseCoreEquipmentRulesService::ClearRuleCache()
{
    InvalidateCache();
    if (RulesCoordinator)
    {
        RulesCoordinator->ClearRuleCache();
    }
}

bool USuspenseCoreEquipmentRulesService::Initialize(TScriptInterface<ISuspenseCoreEquipmentDataProvider> InDataProvider)
{
    if (!RulesCoordinator) return false;
    return RulesCoordinator->Initialize(InDataProvider);
}

void USuspenseCoreEquipmentRulesService::ResetStatistics()
{
    Metrics.Reset();
    if (RulesCoordinator)
    {
        RulesCoordinator->ResetStatistics();
    }
}

void USuspenseCoreEquipmentRulesService::InvalidateCache(EEquipmentOperationType OperationType)
{
    FScopeLock Lock(&CacheLock);

    if (OperationType == EEquipmentOperationType::None)
    {
        ResultCache.Empty();
        UE_LOG(LogSuspenseCoreEquipmentRules, Verbose, TEXT("Cache fully invalidated"));
    }
    else
    {
        // Selective invalidation could be implemented here
        ResultCache.Empty();
    }
}

//========================================
// Cache Helpers
//========================================

uint32 USuspenseCoreEquipmentRulesService::ComputeRequestHash(const FEquipmentOperationRequest& Request) const
{
    uint32 Hash = 0;
    Hash = HashCombine(Hash, GetTypeHash(Request.ItemInstance.ItemID));
    Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Request.OperationType)));
    Hash = HashCombine(Hash, GetTypeHash(Request.TargetSlotIndex));
    Hash = HashCombine(Hash, GetTypeHash(Request.SourceSlotIndex));
    return Hash;
}

bool USuspenseCoreEquipmentRulesService::TryGetCachedResult(uint32 Hash, FRuleEvaluationResult& OutResult) const
{
    FScopeLock Lock(&CacheLock);

    const FCachedResult* Cached = ResultCache.Find(Hash);
    if (Cached)
    {
        const double CurrentTime = FPlatformTime::Seconds();
        if ((CurrentTime - Cached->CacheTime) < Config.CacheTTLSeconds)
        {
            OutResult = Cached->Result;
            return true;
        }
        // Expired - will be cleaned up later
    }

    return false;
}

void USuspenseCoreEquipmentRulesService::CacheResult(uint32 Hash, const FRuleEvaluationResult& Result) const
{
    FScopeLock Lock(&CacheLock);

    // Cleanup if over limit
    if (ResultCache.Num() >= Config.MaxCacheEntries)
    {
        CleanupExpiredCache();

        // If still over limit, remove oldest
        if (ResultCache.Num() >= Config.MaxCacheEntries)
        {
            double OldestTime = TNumericLimits<double>::Max();
            uint32 OldestHash = 0;
            for (const auto& Pair : ResultCache)
            {
                if (Pair.Value.CacheTime < OldestTime)
                {
                    OldestTime = Pair.Value.CacheTime;
                    OldestHash = Pair.Key;
                }
            }
            ResultCache.Remove(OldestHash);
        }
    }

    FCachedResult& Entry = ResultCache.FindOrAdd(Hash);
    Entry.Result = Result;
    Entry.CacheTime = FPlatformTime::Seconds();
    Entry.RequestHash = Hash;
}

void USuspenseCoreEquipmentRulesService::CleanupExpiredCache() const
{
    const double CurrentTime = FPlatformTime::Seconds();
    const double ExpiryThreshold = CurrentTime - Config.CacheTTLSeconds;

    for (auto It = ResultCache.CreateIterator(); It; ++It)
    {
        if (It.Value().CacheTime < ExpiryThreshold)
        {
            It.RemoveCurrent();
        }
    }
}

//========================================
// EventBus Helpers
//========================================

void USuspenseCoreEquipmentRulesService::SetupEventBus()
{
    EventBus = FSuspenseCoreEquipmentEventBus::Get();

    using namespace SuspenseCoreEquipmentTags;
    Tag_Validation_Started = Event::TAG_Equipment_Event_Validation_Started;
    Tag_Validation_Passed = Event::TAG_Equipment_Event_Validation_Passed;
    Tag_Validation_Failed = Event::TAG_Equipment_Event_Validation_Failed;
}

void USuspenseCoreEquipmentRulesService::TeardownEventBus()
{
    auto Bus = EventBus.Pin();
    if (Bus.IsValid())
    {
        for (const FEventSubscriptionHandle& Handle : EventSubscriptions)
        {
            Bus->Unsubscribe(Handle);
        }
    }
    EventSubscriptions.Empty();
}

void USuspenseCoreEquipmentRulesService::BroadcastValidationStarted(const FEquipmentOperationRequest& Request) const
{
    auto Bus = EventBus.Pin();
    if (!Bus.IsValid() || !Tag_Validation_Started.IsValid()) return;

    FSuspenseCoreEquipmentEventData EventData;
    EventData.EventType = Tag_Validation_Started;
    EventData.AddMetadata(TEXT("ItemId"), Request.ItemInstance.ItemID.ToString());
    EventData.AddMetadata(TEXT("OperationType"), FString::FromInt(static_cast<int32>(Request.OperationType)));
    EventData.AddMetadata(TEXT("TargetSlot"), FString::FromInt(Request.TargetSlotIndex));

    Bus->Broadcast(EventData);
}

void USuspenseCoreEquipmentRulesService::BroadcastValidationResult(
    const FEquipmentOperationRequest& Request,
    const FRuleEvaluationResult& Result) const
{
    auto Bus = EventBus.Pin();
    if (!Bus.IsValid()) return;

    const FGameplayTag ResultTag = Result.bPassed ? Tag_Validation_Passed : Tag_Validation_Failed;
    if (!ResultTag.IsValid()) return;

    FSuspenseCoreEquipmentEventData EventData;
    EventData.EventType = ResultTag;
    EventData.AddMetadata(TEXT("ItemId"), Request.ItemInstance.ItemID.ToString());
    EventData.AddMetadata(TEXT("OperationType"), FString::FromInt(static_cast<int32>(Request.OperationType)));
    EventData.AddMetadata(TEXT("Passed"), Result.bPassed ? TEXT("true") : TEXT("false"));
    if (!Result.bPassed)
    {
        EventData.AddMetadata(TEXT("FailureReason"), Result.FailureReason);
    }

    Bus->Broadcast(EventData);
}

void USuspenseCoreEquipmentRulesService::UpdateMetrics(double EvaluationStartTime, bool bPassed) const
{
    const double EndTime = FPlatformTime::Seconds();
    const uint64 EvaluationTimeUs = static_cast<uint64>((EndTime - EvaluationStartTime) * 1000000.0);

    // Update pass/fail counters
    if (bPassed)
    {
        Metrics.ValidationsPassed++;
    }
    else
    {
        Metrics.ValidationsFailed++;
    }

    // Update average (simple moving average)
    const uint64 CurrentAvg = Metrics.AverageEvaluationTimeUs.Load();
    const uint64 NewAvg = (CurrentAvg * 9 + EvaluationTimeUs) / 10;
    Metrics.AverageEvaluationTimeUs.Store(NewAvg);

    // Update peak
    uint64 CurrentPeak = Metrics.PeakEvaluationTimeUs.Load();
    while (EvaluationTimeUs > CurrentPeak)
    {
        if (Metrics.PeakEvaluationTimeUs.CompareExchange(CurrentPeak, EvaluationTimeUs))
        {
            break;
        }
        CurrentPeak = Metrics.PeakEvaluationTimeUs.Load();
    }
}
