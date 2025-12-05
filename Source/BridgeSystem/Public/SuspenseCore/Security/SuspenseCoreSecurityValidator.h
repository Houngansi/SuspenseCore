// SuspenseCoreSecurityValidator.h
// SuspenseCore - Centralized Security Validation
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreSecurityTypes.h"
#include "SuspenseCoreSecurityValidator.generated.h"

// Forward declarations
class USuspenseCoreEventBus;
class APlayerController;

// Log category
DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseCoreSecurity, Log, All);

/**
 * USuspenseCoreSecurityValidator
 *
 * Centralized security validation for all SuspenseCore operations.
 * Implements AAA-standard anti-cheat patterns.
 *
 * FEATURES:
 * - Authority checking (server-side validation)
 * - Rate limiting (prevent spam/DoS)
 * - Suspicious activity detection
 * - Violation logging for analytics
 * - EventBus integration for security events
 *
 * ARCHITECTURE:
 * - GameInstanceSubsystem for global access
 * - Thread-safe rate limiting
 * - Configurable thresholds
 * - EventBus notifications
 *
 * USAGE:
 * ```cpp
 * USuspenseCoreSecurityValidator* Security = USuspenseCoreSecurityValidator::Get(this);
 * if (!Security->CheckAuthority(GetOwner(), TEXT("AddItem")))
 * {
 *     return; // Client blocked, Server RPC should be called instead
 * }
 * ```
 *
 * @see SuspenseCoreSecurityMacros.h for convenient macros
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreSecurityValidator : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	USuspenseCoreSecurityValidator();

	//==================================================================
	// Static Access
	//==================================================================

	/**
	 * Get validator instance from any world context.
	 * @param WorldContextObject Any UObject with world context
	 * @return Validator instance or nullptr
	 */
	static USuspenseCoreSecurityValidator* Get(const UObject* WorldContextObject);

	//==================================================================
	// USubsystem Interface
	//==================================================================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//==================================================================
	// Authority Checking
	//==================================================================

	/**
	 * Check if actor has server authority.
	 * Primary security check for all operations.
	 *
	 * @param Actor Actor to check
	 * @param OperationName Name for logging
	 * @return true if has authority (server), false if client
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Security")
	bool CheckAuthority(AActor* Actor, const FString& OperationName) const;

	/**
	 * Check if actor has authority with detailed result.
	 *
	 * @param Actor Actor to check
	 * @param OperationName Name for logging
	 * @param OutResult Detailed result code
	 * @return true if has authority
	 */
	bool CheckAuthorityWithResult(AActor* Actor, const FString& OperationName,
		ESuspenseCoreSecurityResult& OutResult) const;

	/**
	 * Check if component's owner has authority.
	 *
	 * @param Component Component to check
	 * @param OperationName Name for logging
	 * @return true if has authority
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Security")
	bool CheckComponentAuthority(UActorComponent* Component, const FString& OperationName) const;

	/**
	 * Check authority for UObject (finds outer actor).
	 *
	 * @param Object Object to check
	 * @param OperationName Name for logging
	 * @return true if has authority
	 */
	bool CheckObjectAuthority(UObject* Object, const FString& OperationName) const;

	//==================================================================
	// Rate Limiting
	//==================================================================

	/**
	 * Check if operation is rate limited.
	 * Prevents spam and potential DoS attacks.
	 *
	 * @param Actor Actor performing operation
	 * @param OperationName Operation identifier
	 * @param MaxPerSecond Max operations per second (default: 10)
	 * @return true if allowed (not rate limited)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Security")
	bool CheckRateLimit(AActor* Actor, const FString& OperationName, float MaxPerSecond = 10.0f);

	/**
	 * Reset rate limit for actor.
	 * Call when actor respawns or reconnects.
	 *
	 * @param Actor Actor to reset
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Security")
	void ResetRateLimit(AActor* Actor);

	/**
	 * Reset all rate limits.
	 * Use with caution (admin only).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Security")
	void ResetAllRateLimits();

	//==================================================================
	// RPC Validation Helpers
	//==================================================================

	/**
	 * Validate item ID and quantity for Server RPC.
	 *
	 * @param ItemID Item ID parameter
	 * @param Quantity Quantity parameter
	 * @param MaxQuantity Maximum allowed quantity (default: 9999)
	 * @return true if valid
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Security")
	bool ValidateItemRPC(FName ItemID, int32 Quantity, int32 MaxQuantity = 9999) const;

	/**
	 * Validate GUID parameter.
	 *
	 * @param InstanceID GUID to validate
	 * @return true if valid
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Security")
	bool ValidateGUID(const FGuid& InstanceID) const;

	/**
	 * Validate slot index.
	 *
	 * @param SlotIndex Slot to validate
	 * @param MaxSlots Maximum slots in inventory
	 * @return true if valid
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Security")
	bool ValidateSlotIndex(int32 SlotIndex, int32 MaxSlots) const;

	/**
	 * Validate quantity for stack operations.
	 *
	 * @param Quantity Quantity to validate
	 * @param MinQuantity Minimum (inclusive)
	 * @param MaxQuantity Maximum (inclusive)
	 * @return true if valid
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Security")
	bool ValidateQuantity(int32 Quantity, int32 MinQuantity = 1, int32 MaxQuantity = 9999) const;

	//==================================================================
	// Suspicious Activity Detection
	//==================================================================

	/**
	 * Check for suspicious activity patterns.
	 * Analyzes operation frequency and patterns.
	 *
	 * @param Actor Actor to check
	 * @param OperationName Operation being performed
	 * @param Level Security level for operation
	 * @return true if activity is normal (not suspicious)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Security")
	bool CheckSuspiciousActivity(AActor* Actor, const FString& OperationName,
		ESuspenseCoreSecurityLevel Level = ESuspenseCoreSecurityLevel::Normal);

	/**
	 * Report suspicious activity manually.
	 *
	 * @param Actor Suspicious actor
	 * @param Reason Description of suspicious behavior
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Security")
	void ReportSuspiciousActivity(AActor* Actor, const FString& Reason);

	//==================================================================
	// Violation Tracking
	//==================================================================

	/**
	 * Log security violation.
	 *
	 * @param Violation Violation data
	 */
	void LogViolation(const FSuspenseCoreSecurityViolation& Violation);

	/**
	 * Get violation count for actor.
	 *
	 * @param Actor Actor to check
	 * @return Number of violations in current session
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Security")
	int32 GetViolationCount(AActor* Actor) const;

	/**
	 * Get all violations (for admin tools).
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Security")
	const TArray<FSuspenseCoreSecurityViolation>& GetAllViolations() const { return ViolationLog; }

	/**
	 * Clear violations for actor.
	 *
	 * @param Actor Actor to clear
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Security")
	void ClearViolations(AActor* Actor);

	/**
	 * Clear all violations.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Security")
	void ClearAllViolations();

	//==================================================================
	// EventBus Integration
	//==================================================================

	/**
	 * Broadcast security event via EventBus.
	 *
	 * @param EventTag Event tag (SuspenseCore.Event.Security.*)
	 * @param Actor Related actor
	 * @param Context Additional context
	 */
	void BroadcastSecurityEvent(FGameplayTag EventTag, AActor* Actor, const FString& Context);

	//==================================================================
	// Configuration
	//==================================================================

	/** Enable/disable rate limiting globally */
	UPROPERTY(EditAnywhere, Category = "Configuration")
	bool bEnableRateLimiting = true;

	/** Enable/disable suspicious activity detection */
	UPROPERTY(EditAnywhere, Category = "Configuration")
	bool bEnableSuspiciousActivityDetection = true;

	/** Enable/disable violation logging */
	UPROPERTY(EditAnywhere, Category = "Configuration")
	bool bEnableViolationLogging = true;

	/** Max violations before auto-kick (0 = never kick) */
	UPROPERTY(EditAnywhere, Category = "Configuration")
	int32 MaxViolationsBeforeKick = 10;

	/** Violation decay time in seconds (violations older than this are forgotten) */
	UPROPERTY(EditAnywhere, Category = "Configuration")
	float ViolationDecayTime = 300.0f;

	/** Default rate limit (operations per second) */
	UPROPERTY(EditAnywhere, Category = "Configuration")
	float DefaultRateLimit = 10.0f;

	//==================================================================
	// Statistics
	//==================================================================

	/** Get total violations logged */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Security")
	int32 GetTotalViolationCount() const { return ViolationLog.Num(); }

	/** Get unique violators count */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Security")
	int32 GetUniqueViolatorsCount() const { return ViolationCounts.Num(); }

protected:
	//==================================================================
	// Internal Data
	//==================================================================

	/** Rate limit tracking: ActorHash -> (OperationName -> Entry) */
	TMap<uint32, TMap<FString, FSuspenseCoreRateLimitEntry>> RateLimitMap;

	/** Violation count per actor hash */
	TMap<uint32, int32> ViolationCounts;

	/** Full violation log */
	UPROPERTY()
	TArray<FSuspenseCoreSecurityViolation> ViolationLog;

	/** Cached EventBus */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	//==================================================================
	// Internal Methods
	//==================================================================

	/** Get consistent hash for actor */
	uint32 GetActorHash(AActor* Actor) const;

	/** Handle violation (log, broadcast, potentially kick) */
	void HandleViolation(AActor* Actor, const FString& OperationName,
		ESuspenseCoreSecurityResult Result, const FString& Context);

	/** Attempt to kick player for too many violations */
	void TryKickPlayer(AActor* Actor, const FString& Reason);

	/** Decay old violations */
	void DecayViolations();

	/** Get or create EventBus */
	USuspenseCoreEventBus* GetEventBus();
};
