// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/Equipment/ISuspenseEquipmentService.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceMacros.h"
#include "SuspenseCore/Security/SuspenseSecureKeyStorage.h"
#include "SuspenseCore/Security/SuspenseNonceLRUCache.h"
#include "Types/Network/SuspenseNetworkTypes.h"
#include "HAL/RWLock.h"
#include "Templates/SharedPointer.h"
#include "SuspenseCoreEquipmentSecurityService.generated.h"

// Forward declarations
class APlayerController;
struct FUniqueNetIdRepl;

DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseCoreEquipmentSecurity, Log, All);

//========================================
// SECURITY CONFIGURATION
//========================================

/**
 * Network security configuration loaded from config files
 * Provides runtime-configurable security parameters
 */
USTRUCT(BlueprintType)
struct FSecurityServiceConfig
{
    GENERATED_BODY()

    /** Maximum age for valid packets in seconds */
    UPROPERTY(EditAnywhere, Config, Category = "Timing")
    float PacketAgeLimit = 30.0f;

    /** Lifetime of nonces in seconds before cleanup */
    UPROPERTY(EditAnywhere, Config, Category = "Timing")
    float NonceLifetime = 300.0f;

    /** Maximum operations allowed per second per player */
    UPROPERTY(EditAnywhere, Config, Category = "RateLimit")
    int32 MaxOperationsPerSecond = 10;

    /** Maximum operations allowed per minute per player */
    UPROPERTY(EditAnywhere, Config, Category = "RateLimit")
    int32 MaxOperationsPerMinute = 200;

    /** Minimum interval between operations in seconds */
    UPROPERTY(EditAnywhere, Config, Category = "RateLimit")
    float MinOperationInterval = 0.05f;

    /** Maximum suspicious activities before permanent action */
    UPROPERTY(EditAnywhere, Config, Category = "Violations")
    int32 MaxSuspiciousActivities = 10;

    /** Duration of temporary ban in seconds */
    UPROPERTY(EditAnywhere, Config, Category = "Violations")
    float TemporaryBanDuration = 60.0f;

    /** Maximum violations before temporary ban */
    UPROPERTY(EditAnywhere, Config, Category = "Violations")
    int32 MaxViolationsBeforeBan = 3;

    /** Enable strict security checks */
    UPROPERTY(EditAnywhere, Config, Category = "Security")
    bool bEnableStrictSecurity = true;

    /** Log suspicious activities to file */
    UPROPERTY(EditAnywhere, Config, Category = "Security")
    bool bLogSuspiciousActivity = true;

    /** Require HMAC for critical operations */
    UPROPERTY(EditAnywhere, Config, Category = "Security")
    bool bRequireHMACForCritical = true;

    /** Enable IP-based rate limiting */
    UPROPERTY(EditAnywhere, Config, Category = "RateLimit")
    bool bEnableIPRateLimit = true;

    /** Maximum operations per IP per minute */
    UPROPERTY(EditAnywhere, Config, Category = "RateLimit")
    int32 MaxOperationsPerIPPerMinute = 500;

    /** LRU cache capacity for nonces */
    UPROPERTY(EditAnywhere, Config, Category = "Cache")
    int32 NonceCacheCapacity = 10000;

    /**
     * Load configuration from ini file
     */
    static FSecurityServiceConfig LoadFromConfig(const FString& ConfigSection = TEXT("EquipmentSecurity"));
};

//========================================
// SECURITY METRICS
//========================================

/**
 * Thread-safe security metrics for monitoring
 */
struct EQUIPMENTSYSTEM_API FSecurityServiceMetrics
{
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
    TAtomic<uint64> AverageProcessingTimeUs{0};
    TAtomic<uint64> PeakProcessingTimeUs{0};

    FString ToString() const;
    FString ToCSV() const;
    void Reset();
};

//========================================
// RATE LIMIT DATA
//========================================

/**
 * Rate limiting data for player/IP operations
 */
struct FRateLimitEntry
{
    TArray<float> OperationTimestamps;
    float LastOperationTime = 0.0f;
    int32 ViolationCount = 0;
    FString Identifier;
    bool bIsTemporarilyBanned = false;
    float BanExpiryTime = 0.0f;

    bool IsOperationAllowed(float CurrentTime, int32 MaxPerSecond, int32 MaxPerMinute);
    void RecordOperation(float CurrentTime);
    void RecordViolation(float CurrentTime, float BanDuration, int32 MaxViolations);
    void ClearExpiredTimestamps(float CurrentTime);
};

//========================================
// SECURITY VALIDATION RESULT
//========================================

UENUM(BlueprintType)
enum class ESecurityValidationResult : uint8
{
    Valid,
    RateLimitExceeded,
    IPRateLimitExceeded,
    ReplayAttackDetected,
    IntegrityCheckFailed,
    HMACVerificationFailed,
    PlayerBanned,
    IPBanned,
    InvalidRequest
};

USTRUCT(BlueprintType)
struct FSecurityValidationResponse
{
    GENERATED_BODY()

    UPROPERTY()
    ESecurityValidationResult Result = ESecurityValidationResult::Valid;

    UPROPERTY()
    FString ErrorMessage;

    UPROPERTY()
    bool bShouldLogSuspicious = false;

    bool IsValid() const { return Result == ESecurityValidationResult::Valid; }
};

//========================================
// SECURITY SERVICE INTERFACE
//========================================

/**
 * Interface for security operations
 * Extracted from NetworkService for single responsibility
 */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class USuspenseCoreSecurityService : public UInterface
{
    GENERATED_BODY()
};

class EQUIPMENTSYSTEM_API ISuspenseCoreSecurityService
{
    GENERATED_BODY()

public:
    /** Validate a network request for security */
    virtual FSecurityValidationResponse ValidateRequest(
        const FGuid& PlayerGuid,
        APlayerController* PlayerController,
        uint64 Nonce,
        bool bIsCritical = false) = 0;

    /** Generate secure nonce for request */
    virtual uint64 GenerateNonce() = 0;

    /** Mark nonce as used (after successful validation) */
    virtual bool MarkNonceUsed(uint64 Nonce) = 0;

    /** Generate HMAC for request */
    virtual FString GenerateHMAC(const FNetworkOperationRequest& Request) const = 0;

    /** Verify HMAC for request */
    virtual bool VerifyHMAC(const FNetworkOperationRequest& Request) const = 0;

    /** Log suspicious activity */
    virtual void ReportSuspiciousActivity(
        APlayerController* PlayerController,
        const FString& Reason,
        const FString& Details = TEXT("")) = 0;

    /** Get security metrics */
    virtual const FSecurityServiceMetrics& GetMetrics() const = 0;

    /** Export metrics to file */
    virtual bool ExportMetrics(const FString& FilePath) const = 0;

    /** Reload configuration */
    virtual void ReloadConfiguration() = 0;
};

//========================================
// SECURITY SERVICE IMPLEMENTATION
//========================================

/**
 * Equipment Security Service
 *
 * Single Responsibility: All security-related operations
 * - Rate limiting (per player, per IP)
 * - Replay attack protection (nonce management)
 * - HMAC signing/verification
 * - Suspicious activity tracking
 * - Security metrics collection
 *
 * Extracted from NetworkService to follow SRP.
 */
UCLASS()
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentSecurityService : public UObject,
    public IEquipmentService,
    public ISuspenseCoreSecurityService
{
    GENERATED_BODY()

public:
    USuspenseCoreEquipmentSecurityService();
    virtual ~USuspenseCoreEquipmentSecurityService();

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

    //~ Begin ISuspenseCoreSecurityService Interface
    virtual FSecurityValidationResponse ValidateRequest(
        const FGuid& PlayerGuid,
        APlayerController* PlayerController,
        uint64 Nonce,
        bool bIsCritical = false) override;

    virtual uint64 GenerateNonce() override;
    virtual bool MarkNonceUsed(uint64 Nonce) override;
    virtual FString GenerateHMAC(const FNetworkOperationRequest& Request) const override;
    virtual bool VerifyHMAC(const FNetworkOperationRequest& Request) const override;

    virtual void ReportSuspiciousActivity(
        APlayerController* PlayerController,
        const FString& Reason,
        const FString& Details = TEXT("")) override;

    virtual const FSecurityServiceMetrics& GetMetrics() const override { return Metrics; }
    virtual bool ExportMetrics(const FString& FilePath) const override;
    virtual void ReloadConfiguration() override;
    //~ End ISuspenseCoreSecurityService Interface

    /** Get current configuration (read-only) */
    const FSecurityServiceConfig& GetConfiguration() const { return Config; }

protected:
    // Service state
    EServiceLifecycleState ServiceState = EServiceLifecycleState::Uninitialized;
    FServiceInitParams ServiceParams;

    // Configuration
    FSecurityServiceConfig Config;

    // Rate limiting - per player GUID
    TMap<FGuid, FRateLimitEntry> RateLimitPerPlayer;

    // Rate limiting - per IP address
    TMap<FString, FRateLimitEntry> RateLimitPerIP;

    // Suspicious activity tracking
    TMap<FString, int32> SuspiciousActivityCount;

    // Nonce cache for replay protection (LRU with TTL)
    TUniquePtr<FSuspenseNonceLRUCache> NonceCache;

    // Secure key storage for HMAC
    TUniquePtr<FSuspenseSecureKeyStorage> SecureKeyStorage;

    // Thread safety - using FRWLock for read-heavy operations
    mutable FRWLock SecurityLock;

    // Metrics
    FSecurityServiceMetrics Metrics;
    mutable FServiceMetrics ServiceMetrics;

    // Cleanup timer
    FTimerHandle CleanupTimerHandle;
    FTimerHandle MetricsExportTimerHandle;

private:
    // Internal helpers
    bool CheckPlayerRateLimit(const FGuid& PlayerGuid);
    bool CheckIPRateLimit(const FString& IPAddress);
    bool IsNonceUsed(uint64 Nonce) const;
    bool MarkNoncePending(uint64 Nonce);
    void ConfirmNonce(uint64 Nonce);
    void RejectNonce(uint64 Nonce);

    FString GetPlayerIdentifier(APlayerController* PlayerController) const;
    FString GetIPAddress(APlayerController* PlayerController) const;

    void CleanupExpiredData();
    void UpdateMetrics(double ProcessingStartTime);
    void ExportMetricsPeriodically();

    bool InitializeSecureStorage();
    bool LoadOrGenerateHMACKey();
    void ShutdownSecureStorage();

    void LogSecurityEvent(const FString& EventType, const FString& Details) const;
};
