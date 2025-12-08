// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreEquipmentSecurityService.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTLS.h"
#include "Misc/SecureHash.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"

DEFINE_LOG_CATEGORY(LogSuspenseCoreEquipmentSecurity);

//========================================
// FSecurityServiceConfig
//========================================

FSecurityServiceConfig FSecurityServiceConfig::LoadFromConfig(const FString& ConfigSection)
{
    FSecurityServiceConfig OutConfig;

    if (!GConfig) return OutConfig;

    GConfig->GetFloat(*ConfigSection, TEXT("PacketAgeLimit"), OutConfig.PacketAgeLimit, GGameIni);
    GConfig->GetFloat(*ConfigSection, TEXT("NonceLifetime"), OutConfig.NonceLifetime, GGameIni);
    GConfig->GetInt(*ConfigSection, TEXT("MaxOperationsPerSecond"), OutConfig.MaxOperationsPerSecond, GGameIni);
    GConfig->GetInt(*ConfigSection, TEXT("MaxOperationsPerMinute"), OutConfig.MaxOperationsPerMinute, GGameIni);
    GConfig->GetFloat(*ConfigSection, TEXT("MinOperationInterval"), OutConfig.MinOperationInterval, GGameIni);
    GConfig->GetInt(*ConfigSection, TEXT("MaxSuspiciousActivities"), OutConfig.MaxSuspiciousActivities, GGameIni);
    GConfig->GetFloat(*ConfigSection, TEXT("TemporaryBanDuration"), OutConfig.TemporaryBanDuration, GGameIni);
    GConfig->GetInt(*ConfigSection, TEXT("MaxViolationsBeforeBan"), OutConfig.MaxViolationsBeforeBan, GGameIni);
    GConfig->GetBool(*ConfigSection, TEXT("bEnableStrictSecurity"), OutConfig.bEnableStrictSecurity, GGameIni);
    GConfig->GetBool(*ConfigSection, TEXT("bLogSuspiciousActivity"), OutConfig.bLogSuspiciousActivity, GGameIni);
    GConfig->GetBool(*ConfigSection, TEXT("bRequireHMACForCritical"), OutConfig.bRequireHMACForCritical, GGameIni);
    GConfig->GetBool(*ConfigSection, TEXT("bEnableIPRateLimit"), OutConfig.bEnableIPRateLimit, GGameIni);
    GConfig->GetInt(*ConfigSection, TEXT("MaxOperationsPerIPPerMinute"), OutConfig.MaxOperationsPerIPPerMinute, GGameIni);
    GConfig->GetInt(*ConfigSection, TEXT("NonceCacheCapacity"), OutConfig.NonceCacheCapacity, GGameIni);

    return OutConfig;
}

//========================================
// FSecurityServiceMetrics
//========================================

FString FSecurityServiceMetrics::ToString() const
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
        TEXT("Avg Processing: %llu us\n")
        TEXT("Peak Processing: %llu us"),
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

FString FSecurityServiceMetrics::ToCSV() const
{
    return FString::Printf(
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

void FSecurityServiceMetrics::Reset()
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

//========================================
// FRateLimitEntry
//========================================

bool FRateLimitEntry::IsOperationAllowed(float CurrentTime, int32 MaxPerSecond, int32 MaxPerMinute)
{
    // Check temporary ban
    if (bIsTemporarilyBanned)
    {
        if (CurrentTime < BanExpiryTime)
        {
            return false;
        }
        // Ban expired
        bIsTemporarilyBanned = false;
        ViolationCount = 0;
    }

    // Clean expired timestamps
    ClearExpiredTimestamps(CurrentTime);

    // Count operations in last second
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

void FRateLimitEntry::RecordOperation(float CurrentTime)
{
    OperationTimestamps.Add(CurrentTime);
    LastOperationTime = CurrentTime;
}

void FRateLimitEntry::RecordViolation(float CurrentTime, float BanDuration, int32 MaxViolations)
{
    ViolationCount++;
    if (ViolationCount >= MaxViolations)
    {
        bIsTemporarilyBanned = true;
        BanExpiryTime = CurrentTime + BanDuration;
    }
}

void FRateLimitEntry::ClearExpiredTimestamps(float CurrentTime)
{
    OperationTimestamps.RemoveAll([CurrentTime](float Time)
    {
        return (CurrentTime - Time) > 60.0f;
    });
}

//========================================
// USuspenseCoreEquipmentSecurityService
//========================================

USuspenseCoreEquipmentSecurityService::USuspenseCoreEquipmentSecurityService()
{
    Config = FSecurityServiceConfig::LoadFromConfig();
}

USuspenseCoreEquipmentSecurityService::~USuspenseCoreEquipmentSecurityService()
{
    ShutdownService(true);
}

//========================================
// IEquipmentService Implementation
//========================================

bool USuspenseCoreEquipmentSecurityService::InitializeService(const FSuspenseCoreServiceInitParams& Params)
{
    SCOPED_SERVICE_TIMER("SecurityService::Initialize");

    if (ServiceState != ESuspenseCoreServiceLifecycleState::Uninitialized)
    {
        UE_LOG(LogSuspenseCoreEquipmentSecurity, Warning, TEXT("Service already initialized"));
        return ServiceState == ESuspenseCoreServiceLifecycleState::Ready;
    }

    ServiceState = ESuspenseCoreServiceLifecycleState::Initializing;
    ServiceParams = Params;

    UE_LOG(LogSuspenseCoreEquipmentSecurity, Log, TEXT(">>> SecurityService: Initializing..."));

    // Load configuration
    Config = FSecurityServiceConfig::LoadFromConfig();

    // Initialize secure storage (HMAC keys, nonce cache)
    if (!InitializeSecureStorage())
    {
        UE_LOG(LogSuspenseCoreEquipmentSecurity, Error, TEXT("Failed to initialize secure storage"));
        ServiceState = ESuspenseCoreServiceLifecycleState::Failed;
        return false;
    }

    // Setup cleanup timer
    if (UWorld* World = Params.WorldContext.IsValid() ? Params.WorldContext.Get() : nullptr)
    {
        World->GetTimerManager().SetTimer(
            CleanupTimerHandle,
            FTimerDelegate::CreateUObject(this, &USuspenseCoreEquipmentSecurityService::CleanupExpiredData),
            60.0f, // Every minute
            true   // Looping
        );

        // Periodic metrics export (every 5 minutes)
        World->GetTimerManager().SetTimer(
            MetricsExportTimerHandle,
            FTimerDelegate::CreateUObject(this, &USuspenseCoreEquipmentSecurityService::ExportMetricsPeriodically),
            300.0f,
            true
        );
    }

    ServiceState = ESuspenseCoreServiceLifecycleState::Ready;
    UE_LOG(LogSuspenseCoreEquipmentSecurity, Log,
        TEXT("<<< SecurityService: Initialized (Cache=%d, StrictMode=%s)"),
        Config.NonceCacheCapacity,
        Config.bEnableStrictSecurity ? TEXT("ON") : TEXT("OFF"));

    return true;
}

bool USuspenseCoreEquipmentSecurityService::ShutdownService(bool bForce)
{
    if (ServiceState == ESuspenseCoreServiceLifecycleState::Shutdown)
    {
        return true;
    }

    UE_LOG(LogSuspenseCoreEquipmentSecurity, Log, TEXT(">>> SecurityService: Shutting down..."));

    // Clear timers
    if (CleanupTimerHandle.IsValid())
    {
        if (UWorld* World = ServiceParams.WorldContext.Get())
        {
            World->GetTimerManager().ClearTimer(CleanupTimerHandle);
            World->GetTimerManager().ClearTimer(MetricsExportTimerHandle);
        }
    }

    // Export final metrics before shutdown
    if (!bForce && Config.bLogSuspiciousActivity)
    {
        const FString MetricsPath = FPaths::ProjectLogDir() / TEXT("SecurityMetrics_Final.csv");
        ExportMetrics(MetricsPath);
    }

    // Shutdown secure storage
    ShutdownSecureStorage();

    // Clear rate limit data
    {
        FRWScopeLock Lock(SecurityLock, SLT_Write);
        RateLimitPerPlayer.Empty();
        RateLimitPerIP.Empty();
        SuspiciousActivityCount.Empty();
    }

    ServiceState = ESuspenseCoreServiceLifecycleState::Shutdown;
    UE_LOG(LogSuspenseCoreEquipmentSecurity, Log, TEXT("<<< SecurityService: Shutdown complete"));

    return true;
}

FGameplayTag USuspenseCoreEquipmentSecurityService::GetServiceTag() const
{
    using namespace SuspenseCoreEquipmentTags;
    return Service::TAG_Service_Equipment_Security;
}

FGameplayTagContainer USuspenseCoreEquipmentSecurityService::GetRequiredDependencies() const
{
    // SecurityService has no dependencies - it's a leaf service
    return FGameplayTagContainer();
}

bool USuspenseCoreEquipmentSecurityService::ValidateService(TArray<FText>& OutErrors) const
{
    bool bValid = true;

    if (!NonceCache.IsValid())
    {
        OutErrors.Add(FText::FromString(TEXT("NonceCache not initialized")));
        bValid = false;
    }

    if (!SecureKeyStorage.IsValid())
    {
        OutErrors.Add(FText::FromString(TEXT("SecureKeyStorage not initialized")));
        bValid = false;
    }

    return bValid;
}

void USuspenseCoreEquipmentSecurityService::ResetService()
{
    FRWScopeLock Lock(SecurityLock, SLT_Write);

    RateLimitPerPlayer.Empty();
    RateLimitPerIP.Empty();
    SuspiciousActivityCount.Empty();
    Metrics.Reset();

    if (NonceCache.IsValid())
    {
        NonceCache->Clear();
    }

    UE_LOG(LogSuspenseCoreEquipmentSecurity, Log, TEXT("SecurityService: Reset complete"));
}

FString USuspenseCoreEquipmentSecurityService::GetServiceStats() const
{
    FRWScopeLock Lock(SecurityLock, SLT_ReadOnly);

    return FString::Printf(
        TEXT("SecurityService Stats:\n")
        TEXT("  Players Tracked: %d\n")
        TEXT("  IPs Tracked: %d\n")
        TEXT("  Suspicious Players: %d\n")
        TEXT("  Nonce Cache: %s\n")
        TEXT("%s"),
        RateLimitPerPlayer.Num(),
        RateLimitPerIP.Num(),
        SuspiciousActivityCount.Num(),
        NonceCache.IsValid() ? *NonceCache->GetStatistics() : TEXT("N/A"),
        *Metrics.ToString()
    );
}

//========================================
// ISuspenseCoreSecurityService Implementation
//========================================

FSecurityValidationResponse USuspenseCoreEquipmentSecurityService::ValidateRequest(
    const FGuid& PlayerGuid,
    APlayerController* PlayerController,
    uint64 Nonce,
    bool bIsCritical)
{
    // SECURITY: Validation must only run on server (authoritative side)
    UWorld* World = ServiceParams.WorldContext.Get();
    if (World && World->GetNetMode() == NM_Client)
    {
        FSecurityValidationResponse ClientResponse;
        ClientResponse.Result = ESecurityValidationResult::ServiceUnavailable;
        ClientResponse.ErrorMessage = TEXT("Security validation is server-only");
        UE_LOG(LogSuspenseCoreEquipmentSecurity, Warning,
            TEXT("ValidateRequest rejected - security validation is server authoritative only"));
        return ClientResponse;
    }

    const double StartTime = FPlatformTime::Seconds();
    FSecurityValidationResponse Response;

    Metrics.TotalRequestsProcessed++;

    // 1. Check player rate limit
    if (!CheckPlayerRateLimit(PlayerGuid))
    {
        Response.Result = ESecurityValidationResult::RateLimitExceeded;
        Response.ErrorMessage = TEXT("Player rate limit exceeded");
        Response.bShouldLogSuspicious = true;
        Metrics.RequestsRejectedRateLimit++;
        UpdateMetrics(StartTime);
        return Response;
    }

    // 2. Check IP rate limit (if enabled)
    if (Config.bEnableIPRateLimit && PlayerController)
    {
        const FString IPAddress = GetIPAddress(PlayerController);
        if (!IPAddress.IsEmpty() && !CheckIPRateLimit(IPAddress))
        {
            Response.Result = ESecurityValidationResult::IPRateLimitExceeded;
            Response.ErrorMessage = TEXT("IP rate limit exceeded");
            Response.bShouldLogSuspicious = true;
            Metrics.RequestsRejectedIP++;
            UpdateMetrics(StartTime);
            return Response;
        }
    }

    // 3. Check replay attack (nonce)
    if (IsNonceUsed(Nonce))
    {
        Response.Result = ESecurityValidationResult::ReplayAttackDetected;
        Response.ErrorMessage = TEXT("Replay attack detected - nonce already used");
        Response.bShouldLogSuspicious = true;
        Metrics.RequestsRejectedReplay++;
        UpdateMetrics(StartTime);
        return Response;
    }

    // Mark nonce as pending
    if (!MarkNoncePending(Nonce))
    {
        Response.Result = ESecurityValidationResult::ReplayAttackDetected;
        Response.ErrorMessage = TEXT("Failed to register nonce");
        UpdateMetrics(StartTime);
        return Response;
    }

    // Track critical operations
    if (bIsCritical)
    {
        Metrics.CriticalOperationsProcessed++;
    }

    // Record successful operation in rate limiter
    {
        FRWScopeLock Lock(SecurityLock, SLT_Write);
        const float CurrentTime = FPlatformTime::Seconds();

        FRateLimitEntry& PlayerEntry = RateLimitPerPlayer.FindOrAdd(PlayerGuid);
        PlayerEntry.RecordOperation(CurrentTime);

        if (Config.bEnableIPRateLimit && PlayerController)
        {
            const FString IPAddress = GetIPAddress(PlayerController);
            if (!IPAddress.IsEmpty())
            {
                FRateLimitEntry& IPEntry = RateLimitPerIP.FindOrAdd(IPAddress);
                IPEntry.RecordOperation(CurrentTime);
            }
        }
    }

    Response.Result = ESecurityValidationResult::Valid;
    UpdateMetrics(StartTime);
    return Response;
}

uint64 USuspenseCoreEquipmentSecurityService::GenerateNonce()
{
    // SECURITY: Generate cryptographically strong nonce using multiple entropy sources
    // Following NIST SP 800-90B recommendations for entropy collection

    uint64 Nonce = 0;

    // Source 1: High-resolution timer (unpredictable timing)
    const uint64 CycleEntropy = FPlatformTime::Cycles64();

    // Source 2: Generate fresh GUID (uses platform CSPRNG internally)
    // FGuid::NewGuid() on Windows uses UuidCreate (RPC) or CryptGenRandom
    // On Linux uses /dev/urandom or getrandom()
    const FGuid Guid1 = FGuid::NewGuid();
    const FGuid Guid2 = FGuid::NewGuid();

    // Combine GUID components (128 bits of platform CSPRNG entropy)
    const uint64 GuidHigh = (static_cast<uint64>(Guid1.A) << 32) | Guid1.B;
    const uint64 GuidLow = (static_cast<uint64>(Guid2.C) << 16) | Guid2.D;

    // Source 3: Process/Thread IDs for additional unpredictability
    const uint64 ProcessEntropy = static_cast<uint64>(FPlatformProcess::GetCurrentProcessId());
    const uint64 ThreadEntropy = static_cast<uint64>(FPlatformTLS::GetCurrentThreadId());

    // Mix all entropy sources using XOR and rotation
    // This ensures even if one source is weak, others provide security
    Nonce = GuidHigh;
    Nonce ^= FMath::RotateLeft64(GuidLow, 17);
    Nonce ^= FMath::RotateLeft64(CycleEntropy, 31);
    Nonce ^= FMath::RotateLeft64(ProcessEntropy ^ ThreadEntropy, 47);

    // Final mixing step - multiply by large prime and XOR high/low
    constexpr uint64 MixPrime = 0x9E3779B97F4A7C15ULL; // Golden ratio prime
    Nonce *= MixPrime;
    Nonce ^= (Nonce >> 33);

    return Nonce;
}

bool USuspenseCoreEquipmentSecurityService::MarkNonceUsed(uint64 Nonce)
{
    if (!NonceCache.IsValid())
    {
        return false;
    }

    // Confirm the pending nonce as used
    ConfirmNonce(Nonce);
    return true;
}

FString USuspenseCoreEquipmentSecurityService::GenerateHMAC(const FNetworkOperationRequest& Request) const
{
    if (!SecureKeyStorage.IsValid())
    {
        UE_LOG(LogSuspenseCoreEquipmentSecurity, Warning, TEXT("HMAC generation failed - no key storage"));
        return FString();
    }

    // Build canonical string from request (deterministic ordering)
    const FString CanonicalString = FString::Printf(
        TEXT("%s|%llu|%s|%d|%d"),
        *Request.OperationId.ToString(),
        Request.Timestamp,
        *Request.Request.ItemInstance.ItemID.ToString(),
        static_cast<int32>(Request.Request.OperationType),
        Request.Request.TargetSlotIndex
    );

    // Get key bytes
    TArray<uint8> KeyBytes;
    SecureKeyStorage->GetKeyBytes(KeyBytes);

    if (KeyBytes.Num() == 0)
    {
        return FString();
    }

    // Convert message to UTF-8 bytes
    const FTCHARToUTF8 Utf8Converter(*CanonicalString);
    const uint8* MessageData = reinterpret_cast<const uint8*>(Utf8Converter.Get());
    const int32 MessageLen = Utf8Converter.Length();

    // ========================================
    // RFC 2104 HMAC-SHA256 Implementation
    // HMAC(K, m) = H((K' ⊕ opad) || H((K' ⊕ ipad) || m))
    // ========================================

    constexpr int32 BlockSize = 64;  // SHA-256 block size in bytes
    constexpr int32 HashSize = 32;   // SHA-256 output size in bytes
    constexpr uint8 IPAD = 0x36;
    constexpr uint8 OPAD = 0x5C;

    // Step 1: Prepare the key (K')
    // If key > BlockSize, hash it first. If key < BlockSize, pad with zeros.
    TArray<uint8> KeyPrime;
    KeyPrime.SetNumZeroed(BlockSize);

    if (KeyBytes.Num() > BlockSize)
    {
        // Hash the key if too long
        FSHA256Signature KeyHash;
        FSHA256::HashBuffer(KeyBytes.GetData(), KeyBytes.Num(), KeyHash.Signature);
        FMemory::Memcpy(KeyPrime.GetData(), KeyHash.Signature, HashSize);
    }
    else
    {
        // Use key directly (already zero-padded)
        FMemory::Memcpy(KeyPrime.GetData(), KeyBytes.GetData(), KeyBytes.Num());
    }

    // Step 2: Compute inner hash: H((K' ⊕ ipad) || message)
    TArray<uint8> InnerData;
    InnerData.SetNumUninitialized(BlockSize + MessageLen);

    // K' XOR ipad
    for (int32 i = 0; i < BlockSize; ++i)
    {
        InnerData[i] = KeyPrime[i] ^ IPAD;
    }
    // Append message
    FMemory::Memcpy(InnerData.GetData() + BlockSize, MessageData, MessageLen);

    FSHA256Signature InnerHash;
    FSHA256::HashBuffer(InnerData.GetData(), InnerData.Num(), InnerHash.Signature);

    // Step 3: Compute outer hash: H((K' ⊕ opad) || inner_hash)
    TArray<uint8> OuterData;
    OuterData.SetNumUninitialized(BlockSize + HashSize);

    // K' XOR opad
    for (int32 i = 0; i < BlockSize; ++i)
    {
        OuterData[i] = KeyPrime[i] ^ OPAD;
    }
    // Append inner hash
    FMemory::Memcpy(OuterData.GetData() + BlockSize, InnerHash.Signature, HashSize);

    FSHA256Signature FinalHash;
    FSHA256::HashBuffer(OuterData.GetData(), OuterData.Num(), FinalHash.Signature);

    // Secure cleanup of sensitive data
    FMemory::Memzero(KeyPrime.GetData(), KeyPrime.Num());
    FMemory::Memzero(InnerData.GetData(), InnerData.Num());
    FMemory::Memzero(OuterData.GetData(), OuterData.Num());

    return BytesToHex(FinalHash.Signature, HashSize);
}

bool USuspenseCoreEquipmentSecurityService::VerifyHMAC(const FNetworkOperationRequest& Request) const
{
    if (!Config.bRequireHMACForCritical)
    {
        return true; // HMAC not required
    }

    const FString ExpectedHMAC = GenerateHMAC(Request);
    if (ExpectedHMAC.IsEmpty())
    {
        return false;
    }

    // Constant-time comparison to prevent timing attacks
    if (ExpectedHMAC.Len() != Request.HMAC.Len())
    {
        Metrics.RequestsRejectedHMAC++;
        return false;
    }

    int32 Result = 0;
    for (int32 i = 0; i < ExpectedHMAC.Len(); i++)
    {
        Result |= ExpectedHMAC[i] ^ Request.HMAC[i];
    }

    if (Result != 0)
    {
        Metrics.RequestsRejectedHMAC++;
    }

    return Result == 0;
}

void USuspenseCoreEquipmentSecurityService::ReportSuspiciousActivity(
    APlayerController* PlayerController,
    const FString& Reason,
    const FString& Details)
{
    Metrics.SuspiciousActivitiesDetected++;

    const FString PlayerIdentifier = GetPlayerIdentifier(PlayerController);

    {
        FRWScopeLock Lock(SecurityLock, SLT_Write);

        int32& ActivityCount = SuspiciousActivityCount.FindOrAdd(PlayerIdentifier);
        ActivityCount++;

        if (ActivityCount >= Config.MaxSuspiciousActivities)
        {
            // Ban player
            if (FRateLimitEntry* Entry = RateLimitPerPlayer.Find(FGuid())) // Need to get GUID
            {
                Entry->bIsTemporarilyBanned = true;
                Entry->BanExpiryTime = FPlatformTime::Seconds() + Config.TemporaryBanDuration;
                Metrics.PlayersTemporarilyBanned++;
            }
        }
    }

    LogSecurityEvent(TEXT("SuspiciousActivity"), FString::Printf(
        TEXT("Player=%s, Reason=%s, Details=%s"),
        *PlayerIdentifier, *Reason, *Details));
}

bool USuspenseCoreEquipmentSecurityService::ExportMetrics(const FString& FilePath) const
{
    const FString CSVHeader = TEXT("Timestamp,TotalProcessed,RateLimit,Replay,Integrity,HMAC,IPLimit,Suspicious,PlayersBanned,IPsBanned,Critical,AvgTime,PeakTime\n");
    const FString CSVData = Metrics.ToCSV();

    FString Content;
    if (FFileHelper::LoadFileToString(Content, *FilePath))
    {
        // Append to existing file
        Content += TEXT("\n") + CSVData;
    }
    else
    {
        // New file with header
        Content = CSVHeader + CSVData;
    }

    return FFileHelper::SaveStringToFile(Content, *FilePath);
}

void USuspenseCoreEquipmentSecurityService::ReloadConfiguration()
{
    Config = FSecurityServiceConfig::LoadFromConfig();

    UE_LOG(LogSuspenseCoreEquipmentSecurity, Log,
        TEXT("SecurityService: Configuration reloaded (MaxOps/s=%d, StrictMode=%s)"),
        Config.MaxOperationsPerSecond,
        Config.bEnableStrictSecurity ? TEXT("ON") : TEXT("OFF"));
}

//========================================
// Private Helpers
//========================================

bool USuspenseCoreEquipmentSecurityService::CheckPlayerRateLimit(const FGuid& PlayerGuid)
{
    FRWScopeLock Lock(SecurityLock, SLT_ReadOnly);

    const FRateLimitEntry* Entry = RateLimitPerPlayer.Find(PlayerGuid);
    if (!Entry)
    {
        return true; // No history = allowed
    }

    const float CurrentTime = FPlatformTime::Seconds();
    // Need to cast away const for the check (or make a copy)
    FRateLimitEntry EntryCopy = *Entry;
    return EntryCopy.IsOperationAllowed(CurrentTime, Config.MaxOperationsPerSecond, Config.MaxOperationsPerMinute);
}

bool USuspenseCoreEquipmentSecurityService::CheckIPRateLimit(const FString& IPAddress)
{
    FRWScopeLock Lock(SecurityLock, SLT_ReadOnly);

    const FRateLimitEntry* Entry = RateLimitPerIP.Find(IPAddress);
    if (!Entry)
    {
        return true;
    }

    const float CurrentTime = FPlatformTime::Seconds();
    FRateLimitEntry EntryCopy = *Entry;
    return EntryCopy.IsOperationAllowed(CurrentTime, Config.MaxOperationsPerIPPerMinute, Config.MaxOperationsPerIPPerMinute * 10);
}

bool USuspenseCoreEquipmentSecurityService::IsNonceUsed(uint64 Nonce) const
{
    if (!NonceCache.IsValid())
    {
        return false;
    }
    return NonceCache->Contains(Nonce);
}

bool USuspenseCoreEquipmentSecurityService::MarkNoncePending(uint64 Nonce)
{
    if (!NonceCache.IsValid())
    {
        return false;
    }
    return NonceCache->Insert(Nonce, ENonceState::Pending);
}

void USuspenseCoreEquipmentSecurityService::ConfirmNonce(uint64 Nonce)
{
    if (NonceCache.IsValid())
    {
        NonceCache->Confirm(Nonce);
    }
}

void USuspenseCoreEquipmentSecurityService::RejectNonce(uint64 Nonce)
{
    if (NonceCache.IsValid())
    {
        NonceCache->Remove(Nonce);
    }
}

FString USuspenseCoreEquipmentSecurityService::GetPlayerIdentifier(APlayerController* PlayerController) const
{
    if (!PlayerController)
    {
        return TEXT("Unknown");
    }

    if (APlayerState* PS = PlayerController->GetPlayerState<APlayerState>())
    {
        return PS->GetPlayerName();
    }

    return FString::Printf(TEXT("Controller_%p"), PlayerController);
}

FString USuspenseCoreEquipmentSecurityService::GetIPAddress(APlayerController* PlayerController) const
{
    if (!PlayerController)
    {
        return FString();
    }

    if (UNetConnection* Connection = PlayerController->GetNetConnection())
    {
        return Connection->LowLevelGetRemoteAddress(true);
    }

    return FString();
}

void USuspenseCoreEquipmentSecurityService::CleanupExpiredData()
{
    FRWScopeLock Lock(SecurityLock, SLT_Write);

    const float CurrentTime = FPlatformTime::Seconds();

    // Cleanup player rate limits
    for (auto It = RateLimitPerPlayer.CreateIterator(); It; ++It)
    {
        It.Value().ClearExpiredTimestamps(CurrentTime);
        if (It.Value().OperationTimestamps.Num() == 0 && !It.Value().bIsTemporarilyBanned)
        {
            It.RemoveCurrent();
        }
    }

    // Cleanup IP rate limits
    for (auto It = RateLimitPerIP.CreateIterator(); It; ++It)
    {
        It.Value().ClearExpiredTimestamps(CurrentTime);
        if (It.Value().OperationTimestamps.Num() == 0 && !It.Value().bIsTemporarilyBanned)
        {
            It.RemoveCurrent();
        }
    }

    // Cleanup nonce cache (LRU handles TTL internally)
    if (NonceCache.IsValid())
    {
        NonceCache->CleanupExpired();
    }

    UE_LOG(LogSuspenseCoreEquipmentSecurity, Verbose,
        TEXT("SecurityService: Cleanup complete (Players=%d, IPs=%d)"),
        RateLimitPerPlayer.Num(), RateLimitPerIP.Num());
}

void USuspenseCoreEquipmentSecurityService::UpdateMetrics(double ProcessingStartTime)
{
    const double EndTime = FPlatformTime::Seconds();
    const uint64 ProcessingTimeUs = static_cast<uint64>((EndTime - ProcessingStartTime) * 1000000.0);

    // Update average (simple moving average)
    const uint64 CurrentAvg = Metrics.AverageProcessingTimeUs.Load();
    const uint64 NewAvg = (CurrentAvg * 9 + ProcessingTimeUs) / 10;
    Metrics.AverageProcessingTimeUs.Store(NewAvg);

    // Update peak
    uint64 CurrentPeak = Metrics.PeakProcessingTimeUs.Load();
    while (ProcessingTimeUs > CurrentPeak)
    {
        if (Metrics.PeakProcessingTimeUs.CompareExchange(CurrentPeak, ProcessingTimeUs))
        {
            break;
        }
        CurrentPeak = Metrics.PeakProcessingTimeUs.Load();
    }
}

void USuspenseCoreEquipmentSecurityService::ExportMetricsPeriodically()
{
    if (!Config.bLogSuspiciousActivity)
    {
        return;
    }

    const FString MetricsPath = FPaths::ProjectLogDir() / TEXT("SecurityMetrics.csv");
    ExportMetrics(MetricsPath);
}

bool USuspenseCoreEquipmentSecurityService::InitializeSecureStorage()
{
    // Initialize nonce cache
    NonceCache = MakeUnique<FSuspenseNonceLRUCache>(
        Config.NonceCacheCapacity,
        Config.NonceLifetime
    );

    // Initialize secure key storage
    SecureKeyStorage = MakeUnique<FSuspenseSecureKeyStorage>();

    // Load or generate HMAC key
    return LoadOrGenerateHMACKey();
}

bool USuspenseCoreEquipmentSecurityService::LoadOrGenerateHMACKey()
{
    if (!SecureKeyStorage.IsValid())
    {
        return false;
    }

    // Try to load existing key
    const FString KeyPath = FPaths::ProjectSavedDir() / TEXT("Security") / TEXT("equipment.key");

    if (FPaths::FileExists(KeyPath))
    {
        if (SecureKeyStorage->LoadFromFile(KeyPath))
        {
            UE_LOG(LogSuspenseCoreEquipmentSecurity, Log, TEXT("HMAC key loaded from file"));
            return true;
        }
    }

    // Generate new key
    if (SecureKeyStorage->GenerateKey(32))
    {
        // Ensure directory exists
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(KeyPath), true);

        if (SecureKeyStorage->SaveToFile(KeyPath))
        {
            UE_LOG(LogSuspenseCoreEquipmentSecurity, Log, TEXT("New HMAC key generated and saved"));
            return true;
        }
    }

    UE_LOG(LogSuspenseCoreEquipmentSecurity, Warning, TEXT("Failed to initialize HMAC key - using runtime key"));
    return SecureKeyStorage->GenerateKey(32);
}

void USuspenseCoreEquipmentSecurityService::ShutdownSecureStorage()
{
    if (NonceCache.IsValid())
    {
        NonceCache->Clear();
        NonceCache.Reset();
    }

    if (SecureKeyStorage.IsValid())
    {
        SecureKeyStorage->ClearKey();
        SecureKeyStorage.Reset();
    }
}

void USuspenseCoreEquipmentSecurityService::LogSecurityEvent(const FString& EventType, const FString& Details) const
{
    if (!Config.bLogSuspiciousActivity)
    {
        return;
    }

    UE_LOG(LogSuspenseCoreEquipmentSecurity, Warning,
        TEXT("[SECURITY] %s: %s"), *EventType, *Details);

    // Also write to security log file
    const FString LogPath = FPaths::ProjectLogDir() / TEXT("SecurityEvents.log");
    const FString LogEntry = FString::Printf(
        TEXT("[%s] %s: %s\n"),
        *FDateTime::Now().ToString(),
        *EventType,
        *Details
    );

    FFileHelper::SaveStringToFile(
        LogEntry,
        *LogPath,
        FFileHelper::EEncodingOptions::AutoDetect,
        &IFileManager::Get(),
        EFileWrite::FILEWRITE_Append
    );
}
