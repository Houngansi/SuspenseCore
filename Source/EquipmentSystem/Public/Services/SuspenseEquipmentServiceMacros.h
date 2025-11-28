// SuspenseEquipmentServiceMacros.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformAtomics.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeExit.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include <atomic>
#include <climits>  // LLONG_MAX/LLONG_MIN

// ВАЖНО: используем ваш канонический треад-гвард и RW-лок,
// чтобы не дублировать типы/макросы и не ловить redefinition.
#include "Core/Utils/SuspenseEquipmentThreadGuard.h"

//========================================
// Logging Categories
//========================================

DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseEquipmentData, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseEquipmentNetwork, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseEquipmentOperation, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseEquipmentVisualization, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseEquipmentValidation, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseEquipmentAbility, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseEquipmentPrediction, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseEquipmentReplication, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseEquipmentDelta, Log, All);  // New category for delta logging

// Short aliases for convenience (used by legacy code)
DECLARE_LOG_CATEGORY_EXTERN(LogEquipmentPrediction, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogEquipmentNetwork, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogEquipmentValidation, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogEquipmentOperation, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogEquipmentReplication, Log, All);

//========================================
/* Thread Safety Macros */
//========================================

// НЕ объявляем псевдоним типа RW-лок (раньше было: using FEquipmentRWLock = FRWLock;)
// У вас уже есть класс FEquipmentRWLock в FSuspenseEquipmentThreadGuard.h — конфликт решаем удалением alias.

// Переиспользуем ваши макросы, но если где-то этот header подключается раньше
// FSuspenseEquipmentThreadGuard.h — дадим безопасный fallback. В обычном пути
// (мы выше инклюдим FSuspenseEquipmentThreadGuard.h) эти #ifndef просто не сработают.

#ifndef EQUIPMENT_READ_LOCK
    #define EQUIPMENT_READ_LOCK(RWLockObj) \
        FEquipmentRWGuard ScopeGuard_##RWLockObj(RWLockObj, ELockType::Read)
#endif

#ifndef EQUIPMENT_WRITE_LOCK
    #define EQUIPMENT_WRITE_LOCK(RWLockObj) \
        FEquipmentRWGuard ScopeGuard_##RWLockObj(RWLockObj, ELockType::Write)
#endif

#ifndef EQUIPMENT_CRITICAL_LOCK
    #define EQUIPMENT_CRITICAL_LOCK(CS) \
        FScopeLock ScopeLock_##CS(&(CS))
#endif

//========================================
// Service Metrics System
//========================================

/**
 * Thread-safe accumulator for a single metric
 * (добавлены move-ctor/assign, запрет copy — чтобы тип можно было хранить в UE-контейнерах)
 */
struct FMetricAccumulator
{
    std::atomic<int64> Count{0};
    std::atomic<int64> Sum{0};      // Sum of values (e.g., duration in ms)
    std::atomic<int64> Min{LLONG_MAX};
    std::atomic<int64> Max{LLONG_MIN};

    // --- Move support for UE containers (TMap/TArray need move-constructible values) ---
    FMetricAccumulator() = default;

    FMetricAccumulator(FMetricAccumulator&& Other) noexcept
    {
        Count.store(Other.Count.load(std::memory_order_relaxed), std::memory_order_relaxed);
        Sum  .store(Other.Sum  .load(std::memory_order_relaxed), std::memory_order_relaxed);
        Min  .store(Other.Min  .load(std::memory_order_relaxed), std::memory_order_relaxed);
        Max  .store(Other.Max  .load(std::memory_order_relaxed), std::memory_order_relaxed);
        // оставить Other как есть: атомики пригодны к повторному использованию
    }

    FMetricAccumulator& operator=(FMetricAccumulator&& Other) noexcept
    {
        if (this != &Other)
        {
            Count.store(Other.Count.load(std::memory_order_relaxed), std::memory_order_relaxed);
            Sum  .store(Other.Sum  .load(std::memory_order_relaxed), std::memory_order_relaxed);
            Min  .store(Other.Min  .load(std::memory_order_relaxed), std::memory_order_relaxed);
            Max  .store(Other.Max  .load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        return *this;
    }

    FMetricAccumulator(const FMetricAccumulator&) = delete;
    FMetricAccumulator& operator=(const FMetricAccumulator&) = delete;
    // -------------------------------------------------------------------------------

    FORCEINLINE void Add(int64 Value)
    {
        Count.fetch_add(1, std::memory_order_relaxed);
        Sum.fetch_add(Value, std::memory_order_relaxed);

        // Safe but cheap: CAS for Min/Max
        int64 CurMin = Min.load(std::memory_order_relaxed);
        while (Value < CurMin && !Min.compare_exchange_weak(CurMin, Value, std::memory_order_relaxed)) {}

        int64 CurMax = Max.load(std::memory_order_relaxed);
        while (Value > CurMax && !Max.compare_exchange_weak(CurMax, Value, std::memory_order_relaxed)) {}
    }

    struct FSnapshot
    {
        int64 Count = 0;
        int64 Sum = 0;
        int64 Min = 0;
        int64 Max = 0;
        double Avg = 0.0; // Sum/Count
    };

    FSnapshot Snapshot() const
    {
        FSnapshot S;
        S.Count = Count.load(std::memory_order_relaxed);
        S.Sum = Sum.load(std::memory_order_relaxed);
        const int64 RawMin = Min.load(std::memory_order_relaxed);
        const int64 RawMax = Max.load(std::memory_order_relaxed);
        S.Min = (RawMin == LLONG_MAX) ? 0 : RawMin;
        S.Max = (RawMax == LLONG_MIN) ? 0 : RawMax;
        S.Avg = (S.Count > 0) ? double(S.Sum) / double(S.Count) : 0.0;
        return S;
    }

    void Reset()
    {
        Count.store(0, std::memory_order_seq_cst);
        Sum.store(0, std::memory_order_seq_cst);
        Min.store(LLONG_MAX, std::memory_order_seq_cst);
        Max.store(LLONG_MIN, std::memory_order_seq_cst);
    }
};

/**
 * Unified service metrics container
 */
struct FServiceMetrics
{
    // Global counters
    std::atomic<int64> TotalCalls{0};
    std::atomic<int64> TotalSuccess{0};
    std::atomic<int64> TotalErrors{0};
    std::atomic<int64> TotalDurationMs{0};
    std::atomic<int64> MinDurationMs{LLONG_MAX};
    std::atomic<int64> MaxDurationMs{LLONG_MIN};

    // Named metrics (methods, custom metrics)
    mutable FCriticalSection NamedLock;
    TMap<FName, FMetricAccumulator> Named; // Protected by NamedLock

    void RecordCallDuration(int64 DurationMs)
    {
        TotalCalls.fetch_add(1, std::memory_order_relaxed);
        TotalDurationMs.fetch_add(DurationMs, std::memory_order_relaxed);

        int64 CurMin = MinDurationMs.load(std::memory_order_relaxed);
        while (DurationMs < CurMin && !MinDurationMs.compare_exchange_weak(CurMin, DurationMs, std::memory_order_relaxed)) {}

        int64 CurMax = MaxDurationMs.load(std::memory_order_relaxed);
        while (DurationMs > CurMax && !MaxDurationMs.compare_exchange_weak(CurMax, DurationMs, std::memory_order_relaxed)) {}
    }

    void RecordSuccess() { TotalSuccess.fetch_add(1, std::memory_order_relaxed); }
    void RecordError() { TotalErrors.fetch_add(1, std::memory_order_relaxed); }

    void RecordValue(const FName& MetricName, int64 Value)
    {
        FScopeLock L(&NamedLock);
        FMetricAccumulator& Acc = Named.FindOrAdd(MetricName);
        Acc.Add(Value);
    }

    void RecordEvent(const FName& EventName, int64 Count = 1)
    {
        RecordValue(EventName, Count);
    }

    FORCEINLINE void Inc(const FName& MetricName, int64 Delta = 1) { RecordValue(MetricName, Delta); }
    FORCEINLINE void AddDurationMs(const FName& MetricName, int64 DurationMs) { RecordValue(MetricName, DurationMs); }

    void Reset()
    {
        TotalCalls.store(0, std::memory_order_seq_cst);
        TotalSuccess.store(0, std::memory_order_seq_cst);
        TotalErrors.store(0, std::memory_order_seq_cst);
        TotalDurationMs.store(0, std::memory_order_seq_cst);
        MinDurationMs.store(LLONG_MAX, std::memory_order_seq_cst);
        MaxDurationMs.store(LLONG_MIN, std::memory_order_seq_cst);

        FScopeLock L(&NamedLock);
        Named.Empty();
    }

    bool ExportToCSV(const FString& AbsoluteFilePath, const FString& ServiceName) const
    {
        FString Csv;
        Csv += TEXT("service,metric,count,sum,min,max,avg\n");

        {
            const int64 totalCalls = TotalCalls.load();
            const int64 totalDur = TotalDurationMs.load();
            const int64 minDur = (MinDurationMs.load() == LLONG_MAX) ? 0 : MinDurationMs.load();
            const int64 maxDur = (MaxDurationMs.load() == LLONG_MIN) ? 0 : MaxDurationMs.load();
            const double avgDur = (totalCalls > 0) ? double(totalDur) / double(totalCalls) : 0.0;

            Csv += FString::Printf(TEXT("%s,%s,%lld,%lld,%lld,%lld,%.3f\n"),
                *ServiceName, TEXT("public_method_duration_ms"),
                (long long)totalCalls, (long long)totalDur,
                (long long)minDur, (long long)maxDur, avgDur);
        }

        {
            Csv += FString::Printf(TEXT("%s,%s,%lld,%lld,%d,%d,%.3f\n"),
                *ServiceName, TEXT("success"),
                (long long)TotalSuccess.load(), (long long)TotalSuccess.load(),
                0, 0, 1.0);
            Csv += FString::Printf(TEXT("%s,%s,%lld,%lld,%d,%d,%.3f\n"),
                *ServiceName, TEXT("errors"),
                (long long)TotalErrors.load(), (long long)TotalErrors.load(),
                0, 0, 1.0);
        }

        {
            FScopeLock L(&NamedLock);
            for (const auto& KVP : Named)
            {
                const FName& Name = KVP.Key;
                const FMetricAccumulator::FSnapshot S = KVP.Value.Snapshot();
                Csv += FString::Printf(TEXT("%s,%s,%lld,%lld,%lld,%lld,%.3f\n"),
                    *ServiceName, *Name.ToString(),
                    (long long)S.Count, (long long)S.Sum,
                    (long long)S.Min, (long long)S.Max, S.Avg);
            }
        }

        return FFileHelper::SaveStringToFile(Csv, *AbsoluteFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    FString ToString(const FString& ServiceName) const
    {
        FString Out;
        const int64 Calls = TotalCalls.load();
        const int64 Dur = TotalDurationMs.load();
        const int64 Min = (MinDurationMs.load() == LLONG_MAX) ? 0 : MinDurationMs.load();
        const int64 Max = (MaxDurationMs.load() == LLONG_MIN) ? 0 : MaxDurationMs.load();
        const double Avg = (Calls > 0) ? double(Dur) / double(Calls) : 0.0;

        Out += FString::Printf(TEXT("\n--- Metrics (%s) ---\n"), *ServiceName);
        Out += FString::Printf(TEXT("Public calls: %lld | Success: %lld | Errors: %lld\n"),
            (long long)Calls, (long long)TotalSuccess.load(), (long long)TotalErrors.load());
        Out += FString::Printf(TEXT("Duration ms (sum/min/max/avg): %lld / %lld / %lld / %.3f\n"),
            (long long)Dur, (long long)Min, (long long)Max, Avg);

        FScopeLock L(&NamedLock);
        for (const auto& KVP : Named)
        {
            const FMetricAccumulator::FSnapshot S = KVP.Value.Snapshot();
            Out += FString::Printf(TEXT("%s => count=%lld, sum=%lld, min=%lld, max=%lld, avg=%.3f\n"),
                *KVP.Key.ToString(), (long long)S.Count, (long long)S.Sum,
                (long long)S.Min, (long long)S.Max, S.Avg);
        }
        return Out;
    }
};

//========================================
// Delta/DIFF Metrics (оставлено как было, тип Accumulator теперь мувится)
//========================================

struct FDeltaMetrics
{
    std::atomic<int64> TotalDeltas{0};
    std::atomic<int64> TotalBatchDeltas{0};
    std::atomic<int64> TotalDeltaProcessingMs{0};
    std::atomic<int64> MinDeltaProcessingMs{LLONG_MAX};
    std::atomic<int64> MaxDeltaProcessingMs{LLONG_MIN};

    mutable FCriticalSection DeltaTypeLock;
    TMap<FName, FMetricAccumulator> DeltasByType;   // Protected by DeltaTypeLock
    TMap<FName, FMetricAccumulator> DeltasBySource; // Protected by DeltaTypeLock
    TMap<FName, FMetricAccumulator> DeltaTiming;    // Protected by DeltaTypeLock

    void RecordDelta(const FName& DeltaType, int64 ProcessingTimeMs = 0)
    {
        TotalDeltas.fetch_add(1, std::memory_order_relaxed);

        if (ProcessingTimeMs > 0)
        {
            TotalDeltaProcessingMs.fetch_add(ProcessingTimeMs, std::memory_order_relaxed);

            int64 CurMin = MinDeltaProcessingMs.load(std::memory_order_relaxed);
            while (ProcessingTimeMs < CurMin && !MinDeltaProcessingMs.compare_exchange_weak(CurMin, ProcessingTimeMs, std::memory_order_relaxed)) {}

            int64 CurMax = MaxDeltaProcessingMs.load(std::memory_order_relaxed);
            while (ProcessingTimeMs > CurMax && !MaxDeltaProcessingMs.compare_exchange_weak(CurMax, ProcessingTimeMs, std::memory_order_relaxed)) {}
        }

        FScopeLock L(&DeltaTypeLock);
        FMetricAccumulator& TypeAcc = DeltasByType.FindOrAdd(DeltaType);
        TypeAcc.Add(ProcessingTimeMs > 0 ? ProcessingTimeMs : 1);
    }

    void RecordBatchDelta(const FName& DeltaType, int32 BatchSize, int64 ProcessingTimeMs = 0)
    {
        TotalBatchDeltas.fetch_add(1, std::memory_order_relaxed);
        TotalDeltas.fetch_add(BatchSize, std::memory_order_relaxed);

        if (ProcessingTimeMs > 0)
        {
            TotalDeltaProcessingMs.fetch_add(ProcessingTimeMs, std::memory_order_relaxed);
        }

        FScopeLock L(&DeltaTypeLock);
        FMetricAccumulator& TypeAcc = DeltasByType.FindOrAdd(DeltaType);
        TypeAcc.Add(BatchSize);

        FMetricAccumulator& TimingAcc = DeltaTiming.FindOrAdd(DeltaType);
        TimingAcc.Add(ProcessingTimeMs);
    }

    void RecordDeltaSource(const FName& Source, int64 Count = 1)
    {
        FScopeLock L(&DeltaTypeLock);
        FMetricAccumulator& SourceAcc = DeltasBySource.FindOrAdd(Source);
        SourceAcc.Add(Count);
    }

    void RecordDeltaTiming(const FName& OperationType, int64 DurationMs)
    {
        FScopeLock L(&DeltaTypeLock);
        FMetricAccumulator& TimingAcc = DeltaTiming.FindOrAdd(OperationType);
        TimingAcc.Add(DurationMs);
    }

    void RecordValue(const FName& MetricName, int64 Value)
    {
        FScopeLock L(&DeltaTypeLock);
        FMetricAccumulator& Acc = DeltasByType.FindOrAdd(MetricName);
        Acc.Add(Value);
    }

    void Reset()
    {
        TotalDeltas.store(0, std::memory_order_seq_cst);
        TotalBatchDeltas.store(0, std::memory_order_seq_cst);
        TotalDeltaProcessingMs.store(0, std::memory_order_seq_cst);
        MinDeltaProcessingMs.store(LLONG_MAX, std::memory_order_seq_cst);
        MaxDeltaProcessingMs.store(LLONG_MIN, std::memory_order_seq_cst);

        FScopeLock L(&DeltaTypeLock);
        DeltasByType.Empty();
        DeltasBySource.Empty();
        DeltaTiming.Empty();
    }

    bool ExportToCSV(const FString& AbsoluteFilePath, const FString& ServiceName) const
    {
        FString Csv;
        Csv += TEXT("service,category,metric,count,sum,min,max,avg\n");

        {
            const int64 totalDeltas = TotalDeltas.load();
            const int64 totalProcessing = TotalDeltaProcessingMs.load();
            const int64 minProcessing = (MinDeltaProcessingMs.load() == LLONG_MAX) ? 0 : MinDeltaProcessingMs.load();
            const int64 maxProcessing = (MaxDeltaProcessingMs.load() == LLONG_MIN) ? 0 : MaxDeltaProcessingMs.load();
            const double avgProcessing = (totalDeltas > 0) ? double(totalProcessing) / double(totalDeltas) : 0.0;

            Csv += FString::Printf(TEXT("%s,%s,%s,%lld,%lld,%lld,%lld,%.3f\n"),
                *ServiceName, TEXT("Delta"), TEXT("total_deltas"),
                (long long)totalDeltas, (long long)totalProcessing,
                (long long)minProcessing, (long long)maxProcessing, avgProcessing);

            Csv += FString::Printf(TEXT("%s,%s,%s,%lld,%lld,%d,%d,%.3f\n"),
                *ServiceName, TEXT("Delta"), TEXT("batch_deltas"),
                (long long)TotalBatchDeltas.load(), (long long)TotalBatchDeltas.load(),
                0, 0, 1.0);
        }

        {
            FScopeLock L(&DeltaTypeLock);

            for (const auto& KVP : DeltasByType)
            {
                const FMetricAccumulator::FSnapshot S = KVP.Value.Snapshot();
                Csv += FString::Printf(TEXT("%s,%s,%s,%lld,%lld,%lld,%lld,%.3f\n"),
                    *ServiceName, TEXT("DeltaType"), *KVP.Key.ToString(),
                    (long long)S.Count, (long long)S.Sum,
                    (long long)S.Min, (long long)S.Max, S.Avg);
            }

            for (const auto& KVP : DeltasBySource)
            {
                const FMetricAccumulator::FSnapshot S = KVP.Value.Snapshot();
                Csv += FString::Printf(TEXT("%s,%s,%s,%lld,%lld,%lld,%lld,%.3f\n"),
                    *ServiceName, TEXT("DeltaSource"), *KVP.Key.ToString(),
                    (long long)S.Count, (long long)S.Sum,
                    (long long)S.Min, (long long)S.Max, S.Avg);
            }

            for (const auto& KVP : DeltaTiming)
            {
                const FMetricAccumulator::FSnapshot S = KVP.Value.Snapshot();
                Csv += FString::Printf(TEXT("%s,%s,%s,%lld,%lld,%lld,%lld,%.3f\n"),
                    *ServiceName, TEXT("DeltaTiming"), *KVP.Key.ToString(),
                    (long long)S.Count, (long long)S.Sum,
                    (long long)S.Min, (long long)S.Max, S.Avg);
            }
        }

        return FFileHelper::SaveStringToFile(Csv, *AbsoluteFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    FString ToString(const FString& ServiceName) const
    {
        FString Out;
        const int64 Deltas = TotalDeltas.load();
        const int64 BatchDeltas = TotalBatchDeltas.load();
        const int64 Processing = TotalDeltaProcessingMs.load();
        const int64 MinProc = (MinDeltaProcessingMs.load() == LLONG_MAX) ? 0 : MinDeltaProcessingMs.load();
        const int64 MaxProc = (MaxDeltaProcessingMs.load() == LLONG_MIN) ? 0 : MaxDeltaProcessingMs.load();
        const double AvgProc = (Deltas > 0) ? double(Processing) / double(Deltas) : 0.0;

        Out += FString::Printf(TEXT("\n--- Delta Metrics (%s) ---\n"), *ServiceName);
        Out += FString::Printf(TEXT("Total Deltas: %lld | Batch Operations: %lld\n"),
            (long long)Deltas, (long long)BatchDeltas);
        Out += FString::Printf(TEXT("Processing ms (sum/min/max/avg): %lld / %lld / %lld / %.3f\n"),
            (long long)Processing, (long long)MinProc, (long long)MaxProc, AvgProc);

        FScopeLock L(&DeltaTypeLock);

        if (DeltasByType.Num() > 0)
        {
            Out += TEXT("\nDeltas by Type:\n");
            for (const auto& KVP : DeltasByType)
            {
                const FMetricAccumulator::FSnapshot S = KVP.Value.Snapshot();
                Out += FString::Printf(TEXT("  %s: count=%lld, avg=%.3f\n"),
                    *KVP.Key.ToString(), (long long)S.Count, S.Avg);
            }
        }

        if (DeltasBySource.Num() > 0)
        {
            Out += TEXT("\nDeltas by Source:\n");
            for (const auto& KVP : DeltasBySource)
            {
                const FMetricAccumulator::FSnapshot S = KVP.Value.Snapshot();
                Out += FString::Printf(TEXT("  %s: count=%lld\n"),
                    *KVP.Key.ToString(), (long long)S.Count);
            }
        }

        return Out;
    }
};

//========================================
// Scoped timers & legacy macros (без изменений функционально)
//========================================

class FScopedServiceTimer
{
public:
    FScopedServiceTimer(FServiceMetrics& InMetrics, const FName& InMethodMetricName)
        : Metrics(InMetrics)
        , MethodMetricName(InMethodMetricName)
        , StartSeconds(FPlatformTime::Seconds())
    {}

    ~FScopedServiceTimer()
    {
        const double End = FPlatformTime::Seconds();
        const int64 Ms = (int64)((End - StartSeconds) * 1000.0);
        Metrics.RecordCallDuration(Ms);
        Metrics.AddDurationMs(MethodMetricName, Ms);
    }

private:
    FServiceMetrics& Metrics;
    FName MethodMetricName;
    double StartSeconds = 0.0;
};

class FScopedDiffTimer
{
public:
    FScopedDiffTimer(FDeltaMetrics& InMetrics, const FName& InOperationType)
        : Metrics(InMetrics)
        , OperationType(InOperationType)
        , StartSeconds(FPlatformTime::Seconds())
    {}

    ~FScopedDiffTimer()
    {
        const double End = FPlatformTime::Seconds();
        const int64 Ms = (int64)((End - StartSeconds) * 1000.0);
        Metrics.RecordDeltaTiming(OperationType, Ms);
    }

private:
    FDeltaMetrics& Metrics;
    FName OperationType;
    double StartSeconds = 0.0;
};

//========================================
// Performance tracking (legacy) — без изменений
//========================================

class FScopedDurationTimer
{
public:
    FScopedDurationTimer(float& OutDuration)
        : Duration(OutDuration)
        , StartTime(FPlatformTime::Seconds())
    {
    }

    ~FScopedDurationTimer()
    {
        float ElapsedMs = (FPlatformTime::Seconds() - StartTime) * 1000.0f;

        if (Duration == 0.0f) { Duration = ElapsedMs; }
        else { Duration = Duration * 0.9f + ElapsedMs * 0.1f; }
    }

private:
    float& Duration;
    double StartTime;
};

#define TRACK_DATA_OPERATION_TIME(OpName) \
    FScopedDurationTimer Timer##OpName(AverageReadLatency); \
    if (bEnableDetailedLogging) { LogDataOperation(TEXT(#OpName)); }

#define TRACK_NETWORK_OPERATION_TIME(OpName) \
    FScopedDurationTimer Timer##OpName(AverageOperationLatency); \
    if (bDetailedMetrics) { LogNetworkOperation(TEXT(#OpName)); }

#define TRACK_VALIDATION_TIME(OpName) \
    FScopedDurationTimer Timer##OpName(AverageValidationTime); \
    if (bEnableMetrics) { LogValidationOperation(TEXT(#OpName)); }

//========================================
// Service Metrics Macros
//========================================

#define RECORD_SERVICE_METRIC(MetricName, Value) \
    do { ServiceMetrics.RecordValue(FName(TEXT(MetricName)), (int64)(Value)); } while(0)

#define SCOPED_SERVICE_TIMER(MetricName) \
    FScopedServiceTimer ANON_SVC_TIMER_##__LINE__(ServiceMetrics, FName(TEXT(MetricName)))

#define SCOPED_SERVICE_TIMER_CONST(MetricName) \
    FScopedServiceTimer ANON_SVC_TIMER_##__LINE__(const_cast<FServiceMetrics&>(ServiceMetrics), FName(TEXT(MetricName)))

//========================================
// Delta/DIFF Metrics Macros
//========================================

#define RECORD_DIFF_METRIC(MetricName, Value) \
    do { DeltaMetrics.RecordValue(FName(TEXT(MetricName)), (int64)(Value)); } while(0)

#define RECORD_DELTA_EVENT(DeltaType, ProcessingTimeMs) \
    do { DeltaMetrics.RecordDelta(FName(TEXT(DeltaType)), (int64)(ProcessingTimeMs)); } while(0)

#define RECORD_BATCH_DELTA(DeltaType, BatchSize, ProcessingTimeMs) \
    do { DeltaMetrics.RecordBatchDelta(FName(TEXT(DeltaType)), (int32)(BatchSize), (int64)(ProcessingTimeMs)); } while(0)

#define RECORD_DELTA_SOURCE(Source, Count) \
    do { DeltaMetrics.RecordDeltaSource(FName(TEXT(Source)), (int64)(Count)); } while(0)

#define SCOPED_DIFF_TIMER(OperationType) \
    FScopedDiffTimer ANON_DIFF_TIMER_##__LINE__(DeltaMetrics, FName(TEXT(OperationType)))

#define SCOPED_DIFF_TIMER_CONST(OperationType) \
    FScopedDiffTimer ANON_DIFF_TIMER_##__LINE__(const_cast<FDeltaMetrics&>(DeltaMetrics), FName(TEXT(OperationType)))

#define LOG_DELTA_OPERATION(DeltaType, SlotIndex) \
    UE_LOG(LogSuspenseEquipmentDelta, Verbose, TEXT("Delta[%s]: Slot %d"), TEXT(DeltaType), SlotIndex)

#define LOG_BATCH_DELTA(DeltaType, Count) \
    UE_LOG(LogSuspenseEquipmentDelta, Verbose, TEXT("BatchDelta[%s]: %d operations"), TEXT(DeltaType), Count)

//========================================
// Atomics helpers
//========================================

#define ATOMIC_INCREMENT(Counter) FPlatformAtomics::InterlockedIncrement(&Counter)
#define ATOMIC_DECREMENT(Counter) FPlatformAtomics::InterlockedDecrement(&Counter)
#define ATOMIC_ADD(Counter, Value) FPlatformAtomics::InterlockedAdd(&Counter, Value)
#define ATOMIC_EXCHANGE(Counter, Value) FPlatformAtomics::InterlockedExchange(&Counter, Value)
#define ATOMIC_COMPARE_EXCHANGE(Counter, Exchange, Comparand) FPlatformAtomics::InterlockedCompareExchange(&Counter, Exchange, Comparand)

//========================================
// Validation & misc (без изменений)
//========================================

#define VALIDATE_SERVICE_STATE(RequiredState) \
    if (ServiceState != RequiredState) \
    { \
        UE_LOG(LogSuspenseEquipmentOperation, Warning, \
            TEXT("%s: Invalid service state. Expected: %s, Current: %s"), \
            *FString(__FUNCTION__), \
            *UEnum::GetValueAsString(RequiredState), \
            *UEnum::GetValueAsString(ServiceState)); \
        return {}; \
    }

#define CHECK_COMPONENT_VALID(Component) \
    if (!Component) \
    { \
        UE_LOG(LogSuspenseEquipmentOperation, Error, \
            TEXT("%s: %s is null"), \
            *FString(__FUNCTION__), \
            TEXT(#Component)); \
        return {}; \
    }

#define CHECK_COMPONENT_VALID_BOOL(Component) \
    if (!Component) \
    { \
        UE_LOG(LogSuspenseEquipmentOperation, Error, \
            TEXT("%s: %s is null"), \
            *FString(__FUNCTION__), \
            TEXT(#Component)); \
        return false; \
    }

#define INVALIDATE_CACHE_ENTRY(Cache, Key) if (Cache) { Cache->Invalidate(Key); }
#define UPDATE_CACHE_ENTRY(Cache, Key, Value, TTL) if (Cache) { Cache->Set(Key, Value, TTL); }
#define GET_CACHED_VALUE(Cache, Key, OutValue, OnMiss) if (!Cache || !Cache->Get(Key, OutValue)) { OnMiss; }
#define CLEAR_CACHE(Cache) if (Cache) { Cache->Clear(); }
#define CACHE_HIT_METRIC() ATOMIC_INCREMENT(CacheHits); RECORD_SERVICE_METRIC("Cache.Hit", 1)
#define CACHE_MISS_METRIC() ATOMIC_INCREMENT(CacheMisses); RECORD_SERVICE_METRIC("Cache.Miss", 1)

#define BEGIN_TRANSACTION(Processor, Description) (Processor ? Processor->BeginTransaction(Description) : FGuid())
#define COMMIT_TRANSACTION(Processor, TransactionId) (Processor ? Processor->CommitTransaction(TransactionId) : false)
#define ROLLBACK_TRANSACTION(Processor, TransactionId) (Processor ? Processor->RollbackTransaction(TransactionId) : false)
#define RECORD_TRANSACTION_METRIC(Type) ATOMIC_INCREMENT(Total##Type##Transactions); RECORD_SERVICE_METRIC("Tx." #Type, 1)

#define LOG_AND_RETURN_ERROR(Category, Message, ReturnValue) do { UE_LOG(Category, Error, TEXT("%s"), Message); RECORD_SERVICE_METRIC("Error." #Category, 1); return ReturnValue; } while(0)
#define LOG_AND_CONTINUE(Category, Message) do { UE_LOG(Category, Warning, TEXT("%s"), Message); RECORD_SERVICE_METRIC("Warning." #Category, 1); } while(0)

#if WITH_EDITOR
    #define EQUIPMENT_DEBUG_LOG(Category, Verbosity, Format, ...) UE_LOG(Category, Verbosity, Format, ##__VA_ARGS__)
    #define EQUIPMENT_DEBUG_METRIC(MetricName, Value) RECORD_SERVICE_METRIC(MetricName, Value)
#else
    #define EQUIPMENT_DEBUG_LOG(Category, Verbosity, Format, ...)
    #define EQUIPMENT_DEBUG_METRIC(MetricName, Value)
#endif

#define LOG_IF_DETAILED(Condition, Category, Format, ...) if (Condition) { UE_LOG(Category, Verbose, Format, ##__VA_ARGS__); }

#define SCOPE_EXIT_METRIC(MetricName, Condition) ON_SCOPE_EXIT { if (Condition) RECORD_SERVICE_METRIC(MetricName, 1); }

#define TRACK_MEMORY_ALLOCATION(Size) RECORD_SERVICE_METRIC("Memory.Allocated", Size)
#define TRACK_MEMORY_DEALLOCATION(Size) RECORD_SERVICE_METRIC("Memory.Deallocated", Size)
#define TRACK_OBJECT_CREATION(ObjectType) RECORD_SERVICE_METRIC("Object.Created." #ObjectType, 1)
#define TRACK_OBJECT_DESTRUCTION(ObjectType) RECORD_SERVICE_METRIC("Object.Destroyed." #ObjectType, 1)

#define BEGIN_BATCH_OPERATION(Name) double Batch##Name##StartTime = FPlatformTime::Seconds(); int32 Batch##Name##Count = 0;
#define INCREMENT_BATCH_COUNT(Name) Batch##Name##Count++;
#define END_BATCH_OPERATION(Name) do { \
    double Batch##Name##Duration = (FPlatformTime::Seconds() - Batch##Name##StartTime) * 1000.0; \
    RECORD_SERVICE_METRIC("Batch." #Name ".Count", Batch##Name##Count); \
    RECORD_SERVICE_METRIC("Batch." #Name ".DurationMs", (int64)Batch##Name##Duration); \
    if (Batch##Name##Count > 0) { \
        RECORD_SERVICE_METRIC("Batch." #Name ".AvgMs", (int64)(Batch##Name##Duration / Batch##Name##Count)); \
    } \
} while(0)

#define BROADCAST_EVENT_SAFE(Delegate, ...) if (Delegate.IsBound()) { Delegate.Broadcast(__VA_ARGS__); }
#define EXECUTE_DELEGATE_SAFE(Delegate, ...) if (Delegate.IsBound()) { Delegate.Execute(__VA_ARGS__); }
#define SUBSCRIBE_TO_EVENT(EventSource, EventName, Handler) EventSource->EventName.AddUObject(this, Handler)
#define UNSUBSCRIBE_TO_EVENT(EventSource, EventName, Handle) EventSource->EventName.Remove(Handle)

#if DO_CHECK
    #define ASSERT_GAME_THREAD() check(IsInGameThread())
    #define ASSERT_NOT_GAME_THREAD() check(!IsInGameThread())
    #define ASSERT_LOCK_HELD(LockName) /* no-op for custom lock */
#else
    #define ASSERT_GAME_THREAD()
    #define ASSERT_NOT_GAME_THREAD()
    #define ASSERT_LOCK_HELD(LockName)
#endif

#define SAFE_ARRAY_ACCESS(Array, Index, DefaultValue) ((Array).IsValidIndex(Index) ? (Array)[Index] : (DefaultValue))
#define SAFE_PTR_ACCESS(Ptr, Member, DefaultValue) ((Ptr) ? (Ptr)->Member : (DefaultValue))
#define RETURN_IF_INVALID(Condition, ReturnValue) if (!(Condition)) { return ReturnValue; }
#define CONTINUE_IF_INVALID(Condition) if (!(Condition)) { continue; }
#define BREAK_IF_INVALID(Condition) if (!(Condition)) { break; }
