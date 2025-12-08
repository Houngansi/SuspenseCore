// SuspenseCoreSecurityTypes.h
// SuspenseCore - Security Infrastructure
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCoreSecurityTypes.generated.h"

/**
 * ESuspenseCoreSecurityResult
 *
 * Result codes for security validation operations.
 */
UENUM(BlueprintType)
enum class ESuspenseCoreSecurityResult : uint8
{
	/** Operation allowed */
	Allowed = 0				UMETA(DisplayName = "Allowed"),

	/** Client has no server authority */
	NoAuthority				UMETA(DisplayName = "No Authority"),

	/** RPC validation failed */
	ValidationFailed		UMETA(DisplayName = "Validation Failed"),

	/** Rate limit exceeded */
	RateLimited				UMETA(DisplayName = "Rate Limited"),

	/** Suspicious activity detected */
	SuspiciousActivity		UMETA(DisplayName = "Suspicious Activity"),

	/** Actor not found or invalid */
	InvalidActor			UMETA(DisplayName = "Invalid Actor"),

	/** Insufficient permissions */
	InsufficientPerms		UMETA(DisplayName = "Insufficient Permissions"),

	/** Component not initialized */
	NotInitialized			UMETA(DisplayName = "Not Initialized")
};

/**
 * ESuspenseCoreSecurityLevel
 *
 * Security sensitivity level for operations.
 * Higher levels require stricter validation.
 */
UENUM(BlueprintType)
enum class ESuspenseCoreSecurityLevel : uint8
{
	/** Low security - read operations, queries */
	Low = 0					UMETA(DisplayName = "Low"),

	/** Normal security - standard gameplay operations */
	Normal					UMETA(DisplayName = "Normal"),

	/** High security - currency, trading, persistent data */
	High					UMETA(DisplayName = "High"),

	/** Critical security - admin operations, bans */
	Critical				UMETA(DisplayName = "Critical")
};

/**
 * FSuspenseCoreSecurityViolation
 *
 * Record of a security violation for logging and analytics.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreSecurityViolation
{
	GENERATED_BODY()

	FSuspenseCoreSecurityViolation()
		: Result(ESuspenseCoreSecurityResult::Allowed)
		, Timestamp(0.0)
		, ViolationCount(0)
	{
	}

	FSuspenseCoreSecurityViolation(AActor* InViolator, const FString& InOperation,
		ESuspenseCoreSecurityResult InResult, const FString& InContext)
		: Violator(InViolator)
		, OperationName(InOperation)
		, Result(InResult)
		, Timestamp(FPlatformTime::Seconds())
		, Context(InContext)
		, ViolationCount(1)
	{
	}

	/** Player/Actor that caused violation */
	UPROPERTY(BlueprintReadOnly, Category = "Security")
	TWeakObjectPtr<AActor> Violator;

	/** Operation that was attempted */
	UPROPERTY(BlueprintReadOnly, Category = "Security")
	FString OperationName;

	/** Violation type */
	UPROPERTY(BlueprintReadOnly, Category = "Security")
	ESuspenseCoreSecurityResult Result;

	/** Timestamp (seconds since app start) */
	UPROPERTY(BlueprintReadOnly, Category = "Security")
	double Timestamp;

	/** Additional context information */
	UPROPERTY(BlueprintReadOnly, Category = "Security")
	FString Context;

	/** Cumulative violation count for this actor */
	UPROPERTY(BlueprintReadOnly, Category = "Security")
	int32 ViolationCount;

	/** Get violator name safely */
	FString GetViolatorName() const
	{
		if (AActor* Actor = Violator.Get())
		{
			return Actor->GetName();
		}
		return TEXT("Unknown");
	}

	/** Get formatted log string */
	FString ToLogString() const
	{
		return FString::Printf(TEXT("[SECURITY] %s: %s attempted %s - %s (Count: %d)"),
			*FDateTime::Now().ToString(),
			*GetViolatorName(),
			*OperationName,
			*UEnum::GetValueAsString(Result),
			ViolationCount);
	}
};

/**
 * FSuspenseCoreRateLimitEntry
 *
 * Tracks rate limit state for an operation.
 */
USTRUCT()
struct FSuspenseCoreRateLimitEntry
{
	GENERATED_BODY()

	FSuspenseCoreRateLimitEntry()
		: LastOperationTime(0.0)
		, OperationCount(0)
		, WindowStartTime(0.0)
	{
	}

	/** Time of last operation */
	double LastOperationTime;

	/** Operations in current window */
	int32 OperationCount;

	/** Start of current rate limit window */
	double WindowStartTime;

	/** Check if rate limited (returns true if BLOCKED) */
	bool IsRateLimited(float MaxPerSecond, double CurrentTime)
	{
		// Reset window if expired (1 second window)
		if (CurrentTime - WindowStartTime >= 1.0)
		{
			WindowStartTime = CurrentTime;
			OperationCount = 0;
		}

		// Check limit
		if (OperationCount >= FMath::CeilToInt(MaxPerSecond))
		{
			return true; // Blocked
		}

		// Allow and increment
		OperationCount++;
		LastOperationTime = CurrentTime;
		return false; // Allowed
	}

	void Reset()
	{
		LastOperationTime = 0.0;
		OperationCount = 0;
		WindowStartTime = 0.0;
	}
};

