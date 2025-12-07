// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/Equipment/ISuspenseEquipmentService.h"
#include "Interfaces/Equipment/ISuspenseNetworkDispatcher.h"
#include "Interfaces/Equipment/ISuspensePredictionManager.h"
#include "Interfaces/Equipment/ISuspenseReplicationProvider.h"
#include "Components/Network/SuspenseEquipmentReplicationManager.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceMacros.h"
#include "SuspenseCore/Security/SuspenseSecureKeyStorage.h"
#include "SuspenseCore/Security/SuspenseNonceLRUCache.h"
#include "Types/Network/SuspenseNetworkTypes.h"
#include "Types/Equipment/SuspenseEquipmentTypes.h"
#include "HAL/CriticalSection.h"
#include "HAL/RWLock.h"
#include "Templates/SharedPointer.h"
#include "Containers/Queue.h"
#include "HAL/ThreadSafeBool.h"
#include "UObject/Interface.h"
#include "SuspenseCoreEquipmentNetworkService.generated.h"

// Forward declarations for dependency resolution
class USuspenseCoreEquipmentNetworkDispatcher;
class USuspenseCoreEquipmentPredictionSystem;
class USuspenseCoreEquipmentReplicationManager;
class USuspenseCoreEquipmentServiceLocator;
class ISuspenseEquipmentDataProvider;
class ISuspenseEquipmentOperations;
struct FUniqueNetIdRepl;

/**
 * Network security configuration loaded from config files
 * Provides runtime-configurable security parameters
 */
USTRUCT()
struct FNetworkSecurityConfig
{
    GENERATED_BODY()

    /** Maximum age for valid packets in seconds */
    UPROPERTY(Config)
    float PacketAgeLimit = 30.0f;

    /** Lifetime of nonces in seconds before cleanup */
    UPROPERTY(Config)
    float NonceLifetime = 300.0f;

    /** Maximum operations allowed per second per player */
    UPROPERTY(Config)
    int32 MaxOperationsPerSecond = 10;

    /** Maximum operations allowed per minute per player */
    UPROPERTY(Config)
    int32 MaxOperationsPerMinute = 200;

    /** Minimum interval between operations in seconds */
    UPROPERTY(Config)
    float MinOperationInterval = 0.05f;

    /** Maximum suspicious activities before permanent action */
    UPROPERTY(Config)
    int32 MaxSuspiciousActivities = 10;

    /** Duration of temporary ban in seconds */
    UPROPERTY(Config)
    float TemporaryBanDuration = 60.0f;

    /** Maximum violations before temporary ban */
    UPROPERTY(Config)
    int32 MaxViolationsBeforeBan = 3;

    /** Enable strict security checks */
    UPROPERTY(Config)
    bool bEnableStrictSecurity = true;

    /** Log suspicious activities to file */
    UPROPERTY(Config)
    bool bLogSuspiciousActivity = true;

    /** Require HMAC for critical operations */
    UPROPERTY(Config)
    bool bRequireHMACForCritical = true;

    /** Enable IP-based rate limiting */
    UPROPERTY(Config)
    bool bEnableIPRateLimit = true;

    /** Maximum operations per IP per minute */
    UPROPERTY(Config)
    int32 MaxOperationsPerIPPerMinute = 500;

    /**
     * Load configuration from ini file
     * @param ConfigSection Section name in config file
     * @return Loaded configuration
     */
    static FNetworkSecurityConfig LoadFromConfig(const FString& ConfigSection = TEXT("NetworkSecurity"))
    {
        FNetworkSecurityConfig Config;

        if (!GConfig) return Config;

        GConfig->GetFloat(*ConfigSection, TEXT("PacketAgeLimit"), Config.PacketAgeLimit, GGameIni);
        GConfig->GetFloat(*ConfigSection, TEXT("NonceLifetime"), Config.NonceLifetime, GGameIni);
        GConfig->GetInt(*ConfigSection, TEXT("MaxOperationsPerSecond"), Config.MaxOperationsPerSecond, GGameIni);
        GConfig->GetInt(*ConfigSection, TEXT("MaxOperationsPerMinute"), Config.MaxOperationsPerMinute, GGameIni);
        GConfig->GetFloat(*ConfigSection, TEXT("MinOperationInterval"), Config.MinOperationInterval, GGameIni);
        GConfig->GetInt(*ConfigSection, TEXT("MaxSuspiciousActivities"), Config.MaxSuspiciousActivities, GGameIni);
        GConfig->GetFloat(*ConfigSection, TEXT("TemporaryBanDuration"), Config.TemporaryBanDuration, GGameIni);
        GConfig->GetInt(*ConfigSection, TEXT("MaxViolationsBeforeBan"), Config.MaxViolationsBeforeBan, GGameIni);
        GConfig->GetBool(*ConfigSection, TEXT("bEnableStrictSecurity"), Config.bEnableStrictSecurity, GGameIni);
        GConfig->GetBool(*ConfigSection, TEXT("bLogSuspiciousActivity"), Config.bLogSuspiciousActivity, GGameIni);
        GConfig->GetBool(*ConfigSection, TEXT("bRequireHMACForCritical"), Config.bRequireHMACForCritical, GGameIni);
        GConfig->GetBool(*ConfigSection, TEXT("bEnableIPRateLimit"), Config.bEnableIPRateLimit, GGameIni);
        GConfig->GetInt(*ConfigSection, TEXT("MaxOperationsPerIPPerMinute"), Config.MaxOperationsPerIPPerMinute, GGameIni);

        return Config;
    }
};

/**
 * Enhanced network security metrics for monitoring
 * Provides comprehensive tracking of all security events
 */
struct FNetworkSecurityMetrics
{
    // Use atomic counters for thread-safe operations
    TAtomic<uint64> TotalRequestsProcessed{0};
    TAtomic<uint64> RequestsRejectedRateLimit{0};
    TAtomic<uint64> RequestsRejectedReplay{0};
    TAtomic<uint64> RequestsRejectedIntegrity{0};
    TAtomic<uint64> RequestsRejectedHMAC{0};
    TAtomic<uint64> RequestsRejectedIP{0};
    TAtomic<uint64> SuspiciousActivitiesDetected{0};
    TAtomic<uint64> PlayersTemporarilyBanned{0};
    TAtomic<uint64> IPsTemporarilyBanned{0};
    TAtomic<uint64> CriticalOperationsProcessed{0};

    // Performance metrics
    TAtomic<uint64> AverageProcessingTimeUs{0};
    TAtomic<uint64> PeakProcessingTimeUs{0};

    FString ToString() const
    {
        return FString::Printf(
            TEXT("=== Security Metrics ===\n")
            TEXT("Total Processed: %llu\n")
            TEXT("Rate Limit Rejects: %llu\n")
            TEXT("Replay Attack Blocks: %llu\n")
            TEXT("Integrity Failures: %llu\n")
            TEXT("HMAC Failures: %llu\n")
            TEXT("IP Rate Limit Rejects: %llu\n")
            TEXT("Suspicious Activities: %llu\n")
            TEXT("Players Banned: %llu\n")
            TEXT("IPs Banned: %llu\n")
            TEXT("Critical Operations: %llu\n")
            TEXT("Avg Processing: %llu µs\n")
            TEXT("Peak Processing: %llu µs"),
            TotalRequestsProcessed.Load(),
            RequestsRejectedRateLimit.Load(),
            RequestsRejectedReplay.Load(),
            RequestsRejectedIntegrity.Load(),
            RequestsRejectedHMAC.Load(),
            RequestsRejectedIP.Load(),
            SuspiciousActivitiesDetected.Load(),
            PlayersTemporarilyBanned.Load(),
            IPsTemporarilyBanned.Load(),
            CriticalOperationsProcessed.Load(),
            AverageProcessingTimeUs.Load(),
            PeakProcessingTimeUs.Load()
        );
    }

    FString ToCSV() const
    {
        return FString::Printf(
            TEXT("Timestamp,TotalProcessed,RateLimit,Replay,Integrity,HMAC,IPLimit,Suspicious,PlayersBanned,IPsBanned,Critical,AvgTime,PeakTime\n")
            TEXT("%s,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu"),
            *FDateTime::Now().ToString(),
            TotalRequestsProcessed.Load(),
            RequestsRejectedRateLimit.Load(),
            RequestsRejectedReplay.Load(),
            RequestsRejectedIntegrity.Load(),
            RequestsRejectedHMAC.Load(),
            RequestsRejectedIP.Load(),
            SuspiciousActivitiesDetected.Load(),
            PlayersTemporarilyBanned.Load(),
            IPsTemporarilyBanned.Load(),
            CriticalOperationsProcessed.Load(),
            AverageProcessingTimeUs.Load(),
            PeakProcessingTimeUs.Load()
        );
    }

    void Reset()
    {
        TotalRequestsProcessed.Store(0);
        RequestsRejectedRateLimit.Store(0);
        RequestsRejectedReplay.Store(0);
        RequestsRejectedIntegrity.Store(0);
        RequestsRejectedHMAC.Store(0);
        RequestsRejectedIP.Store(0);
        SuspiciousActivitiesDetected.Store(0);
        PlayersTemporarilyBanned.Store(0);
        IPsTemporarilyBanned.Store(0);
        CriticalOperationsProcessed.Store(0);
        AverageProcessingTimeUs.Store(0);
        PeakProcessingTimeUs.Store(0);
    }
};

/**
 * Rate limiting data for player operations
 */
struct FRateLimitData
{
    TArray<float> OperationTimestamps;
    float LastOperationTime = 0.0f;
    int32 ViolationCount = 0;
    FString PlayerIdentifier;
    bool bIsTemporarilyBanned = false;
    float BanExpiryTime = 0.0f;

    bool IsOperationAllowed(float CurrentTime, int32 MaxPerSecond, int32 MaxPerMinute, float BanDuration = 60.0f)
    {
        if (bIsTemporarilyBanned && CurrentTime < BanExpiryTime)
        {
            return false;
        }
        else if (bIsTemporarilyBanned)
        {
            bIsTemporarilyBanned = false;
            ViolationCount = 0;
        }

        OperationTimestamps.RemoveAll([CurrentTime](float Time)
        {
            return (CurrentTime - Time) > 60.0f;
        });

        int32 OpsInLastSecond = 0;
        for (float Time : OperationTimestamps)
        {
            if ((CurrentTime - Time) <= 1.0f)
            {
                OpsInLastSecond++;
            }
        }

        if (OpsInLastSecond >= MaxPerSecond)
        {
            return false;
        }

        if (OperationTimestamps.Num() >= MaxPerMinute)
        {
            return false;
        }

        return true;
    }

    void RecordOperation(float CurrentTime)
    {
        OperationTimestamps.Add(CurrentTime);
        LastOperationTime = CurrentTime;
    }

    void RecordViolation(float CurrentTime, float BanDuration = 60.0f, int32 MaxViolations = 3)
    {
        ViolationCount++;
        if (ViolationCount >= MaxViolations)
        {
            bIsTemporarilyBanned = true;
            BanExpiryTime = CurrentTime + BanDuration;
        }
    }
};

/**
 * Equipment Network Service Implementation
 */
UCLASS()
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentNetworkService : public UObject,
    public IEquipmentNetworkService
{
    GENERATED_BODY()

public:
    USuspenseCoreEquipmentNetworkService();
    virtual ~USuspenseCoreEquipmentNetworkService();

    //~ Begin IEquipmentService Interface
    virtual bool InitializeService(const FServiceInitParams& Params) override;
    virtual bool ShutdownService(bool bForce = false) override;
    virtual EServiceLifecycleState GetServiceState() const override { return ServiceState; }
    virtual bool IsServiceReady() const override { return ServiceState == EServiceLifecycleState::Ready; }
    virtual FGameplayTag GetServiceTag() const override;
    virtual FGameplayTagContainer GetRequiredDependencies() const override;
    virtual bool ValidateService(TArray<FText>& OutErrors) const override;
    virtual void ResetService() override;
    virtual FString GetServiceStats() const override;
    //~ End IEquipmentService Interface

    //~ Begin IEquipmentNetworkService Interface
    virtual ISuspenseNetworkDispatcher* GetNetworkDispatcher() override
    {
        return NetworkDispatcher.GetInterface();
    }

    virtual ISuspensePredictionManager* GetPredictionManager() override
    {
        return PredictionManager.GetInterface();
    }

    virtual ISuspenseReplicationProvider* GetReplicationProvider() override
    {
        if (!ReplicationProvider) return nullptr;
        if (ReplicationProvider->GetClass()->ImplementsInterface(USuspenseReplicationProvider::StaticClass()))
        {
            return Cast<ISuspenseReplicationProvider>(ReplicationProvider);
        }
        return nullptr;
    }
    //~ End IEquipmentNetworkService Interface

    UFUNCTION(BlueprintCallable, Category = "Network|Operations")
    FGuid SendEquipmentOperation(const FEquipmentOperationRequest& Request, APlayerController* PlayerController);

    bool ReceiveEquipmentOperation(const FNetworkOperationRequest& NetworkRequest, APlayerController* SendingPlayer);

    UFUNCTION(BlueprintCallable, Category = "Network|Quality")
    void SetNetworkQuality(float Quality);

    //UFUNCTION(BlueprintCallable, Category = "Network|Metrics")
    FLatencyCompensationData GetNetworkMetrics() const;

    UFUNCTION(BlueprintCallable, Category = "Network|Sync")
    void ForceSynchronization(APlayerController* PlayerController);

    const FNetworkSecurityMetrics& GetSecurityMetrics() const { return SecurityMetrics; }
    bool ExportSecurityMetrics(const FString& FilePath) const;

    UFUNCTION(BlueprintCallable, Category = "Network|Metrics")
    bool ExportMetricsToCSV(const FString& FilePath) const;

    UFUNCTION(BlueprintCallable, Category = "Network|Security")
    void ReloadSecurityConfig();

private:
    // Service state
    EServiceLifecycleState ServiceState;
    FServiceInitParams ServiceParams;

    // Network components: store UE interfaces as TScriptInterface (GC-friendly), replication keeps raw UObject
    TScriptInterface<ISuspenseNetworkDispatcher> NetworkDispatcher;
    TScriptInterface<ISuspensePredictionManager>  PredictionManager;

    UPROPERTY(Transient)
    TObjectPtr<USuspenseCoreEquipmentReplicationManager> ReplicationProvider = nullptr;

    // Security configuration loaded from INI
    FNetworkSecurityConfig SecurityConfig;

    // Security: Rate limiting
    TMap<FGuid, FRateLimitData> RateLimitPerPlayer;
    TMap<FString, FRateLimitData> RateLimitPerIP;

    // Security: replay protection using LRU cache with TTL
    // Replaces unbounded TSet<uint64> ProcessedNonces
    TUniquePtr<FSuspenseNonceLRUCache> NonceCache;

    // Suspicious activity tracking
    TMap<FString, int32> SuspiciousActivityCount;

    // Security: HMAC secret key with memory obfuscation
    // Replaces plain FString HMACSecretKey
    TUniquePtr<FSuspenseSecureKeyStorage> SecureKeyStorage;

    // Thread safety - using FRWLock for read-heavy operations
    // Lock ordering: NetworkSecurity (Level 11) - see SuspenseThreadSafetyPolicy.h
    mutable FRWLock SecurityLock;

    // Metrics
    FNetworkSecurityMetrics SecurityMetrics;
    mutable FServiceMetrics ServiceMetrics;

    // Legacy metrics
    mutable float AverageLatency;
    mutable int32 TotalOperationsSent;
    mutable int32 TotalOperationsRejected;
    mutable int32 TotalReplayAttemptsBlocked;
    mutable int32 TotalIntegrityFailures;

    float NetworkQualityLevel;

    // Timers
    FTimerHandle NonceCleanupTimer;
    FTimerHandle MetricsUpdateTimer;
    FTimerHandle MetricsExportTimer;

    // Dispatcher delegate handles
    FDelegateHandle DispatcherSuccessHandle;
    FDelegateHandle DispatcherFailureHandle;
    FDelegateHandle DispatcherTimeoutHandle;
    /**
     * Internal non-virtual cleanup method safe to call from destructor
     * @param bForce Force immediate cleanup without metrics export
     * @param bFromDestructor True if called from destructor (minimal cleanup)
     */
    void InternalShutdown(bool bForce, bool bFromDestructor);

    // ---- helpers ----
    bool ResolveDependencies(
        UWorld* World,
        TScriptInterface<ISuspenseEquipmentDataProvider>& OutDataProvider,
        TScriptInterface<ISuspenseEquipmentOperations>& OutOperationExecutor);

    USuspenseCoreEquipmentNetworkDispatcher* CreateAndInitNetworkDispatcher(
        AActor* OwnerActor,
        const TScriptInterface<ISuspenseEquipmentOperations>& OperationExecutor);

    USuspenseCoreEquipmentPredictionSystem* CreateAndInitPredictionSystem(
        AActor* OwnerActor,
        const TScriptInterface<ISuspenseEquipmentDataProvider>& DataProvider,
        const TScriptInterface<ISuspenseEquipmentOperations>& OperationExecutor);

    USuspenseCoreEquipmentReplicationManager* CreateAndInitReplicationManager(
        AActor* OwnerActor,
        const TScriptInterface<ISuspenseEquipmentDataProvider>& DataProvider);

    void BindDispatcherToPrediction(
        USuspenseCoreEquipmentNetworkDispatcher* Dispatcher,
        ISuspensePredictionManager* Prediction);

    void StartMonitoringTimers(UWorld* World);

    FGuid CreatePlayerGuid(const FUniqueNetIdRepl& UniqueId) const;
    bool CheckRateLimit(const FGuid& PlayerGuid, APlayerController* PlayerController);
    bool CheckIPRateLimit(const FString& IPAddress);

    /**
     * Check if nonce exists in cache (replay attack detection)
     * Uses LRU cache instead of unbounded TSet
     */
    bool IsNonceUsed(uint64 Nonce) const;

    /**
     * Mark nonce as pending in LRU cache
     */
    bool MarkNonceAsPending(uint64 Nonce);

    /**
     * Confirm a pending nonce
     */
    bool ConfirmNonce(uint64 Nonce);

    /**
     * Reject/remove a pending nonce
     */
    void RejectNonce(uint64 Nonce);

    /**
     * Generate cryptographically secure nonce
     */
    uint64 GenerateSecureNonce();

    /**
     * Generate HMAC using secure key storage
     */
    FString GenerateRequestHMAC(const FNetworkOperationRequest& Request) const;

    /**
     * Verify HMAC using secure key storage
     */
    bool VerifyRequestHMAC(const FNetworkOperationRequest& Request) const;

    /**
     * Clean expired nonces from LRU cache
     */
    void CleanExpiredNonces();

    void LogSuspiciousActivity(APlayerController* PlayerController, const FString& Reason);
    FString GetPlayerIdentifier(APlayerController* PlayerController) const;
    FString GetIPAddress(APlayerController* PlayerController) const;
    void UpdateNetworkMetrics();
    void UpdateSecurityMetrics(double StartTime);
    void ExportMetricsPeriodically();
    void AdaptNetworkStrategies();

    /**
     * Initialize security subsystems (SecureKeyStorage, NonceCache)
     */
    void InitializeSecurity();

    /**
     * Shutdown security subsystems with secure memory cleanup
     */
    void ShutdownSecurity();

    /**
     * Load HMAC key using SecureKeyStorage
     * @return true if key loaded or generated successfully
     */
    bool LoadHMACKey();
};
