// SuspenseCoreSecurityValidator.cpp
// SuspenseCore - Centralized Security Validation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Security/SuspenseCoreSecurityValidator.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

DEFINE_LOG_CATEGORY(LogSuspenseCoreSecurity);

// ═══════════════════════════════════════════════════════════════════════════════
// CONSTRUCTION
// ═══════════════════════════════════════════════════════════════════════════════

USuspenseCoreSecurityValidator::USuspenseCoreSecurityValidator()
	: bEnableRateLimiting(true)
	, bEnableSuspiciousActivityDetection(true)
	, bEnableViolationLogging(true)
	, MaxViolationsBeforeKick(10)
	, ViolationDecayTime(300.0f)
	, DefaultRateLimit(10.0f)
{
}

// ═══════════════════════════════════════════════════════════════════════════════
// STATIC ACCESS
// ═══════════════════════════════════════════════════════════════════════════════

USuspenseCoreSecurityValidator* USuspenseCoreSecurityValidator::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	return GameInstance->GetSubsystem<USuspenseCoreSecurityValidator>();
}

// ═══════════════════════════════════════════════════════════════════════════════
// SUBSYSTEM INTERFACE
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreSecurityValidator::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogSuspenseCoreSecurity, Log, TEXT("SecurityValidator initialized"));
	UE_LOG(LogSuspenseCoreSecurity, Log, TEXT("  RateLimiting: %s"),
		bEnableRateLimiting ? TEXT("Enabled") : TEXT("Disabled"));
	UE_LOG(LogSuspenseCoreSecurity, Log, TEXT("  SuspiciousActivityDetection: %s"),
		bEnableSuspiciousActivityDetection ? TEXT("Enabled") : TEXT("Disabled"));
	UE_LOG(LogSuspenseCoreSecurity, Log, TEXT("  MaxViolationsBeforeKick: %d"),
		MaxViolationsBeforeKick);
}

void USuspenseCoreSecurityValidator::Deinitialize()
{
	// Log final stats
	UE_LOG(LogSuspenseCoreSecurity, Log, TEXT("SecurityValidator deinitializing"));
	UE_LOG(LogSuspenseCoreSecurity, Log, TEXT("  Total violations logged: %d"), ViolationLog.Num());
	UE_LOG(LogSuspenseCoreSecurity, Log, TEXT("  Unique violators: %d"), ViolationCounts.Num());

	// Clear data
	RateLimitMap.Empty();
	ViolationCounts.Empty();
	ViolationLog.Empty();
	CachedEventBus.Reset();

	Super::Deinitialize();
}

// ═══════════════════════════════════════════════════════════════════════════════
// AUTHORITY CHECKING
// ═══════════════════════════════════════════════════════════════════════════════

bool USuspenseCoreSecurityValidator::CheckAuthority(AActor* Actor, const FString& OperationName) const
{
	if (!Actor)
	{
		UE_LOG(LogSuspenseCoreSecurity, Verbose, TEXT("%s: Actor is null"), *OperationName);
		return false;
	}

	if (!Actor->HasAuthority())
	{
		UE_LOG(LogSuspenseCoreSecurity, Verbose, TEXT("%s: %s has no authority"),
			*OperationName, *Actor->GetName());
		return false;
	}

	return true;
}

bool USuspenseCoreSecurityValidator::CheckAuthorityWithResult(AActor* Actor, const FString& OperationName,
	ESuspenseCoreSecurityResult& OutResult) const
{
	if (!Actor)
	{
		OutResult = ESuspenseCoreSecurityResult::InvalidActor;
		return false;
	}

	if (!Actor->HasAuthority())
	{
		OutResult = ESuspenseCoreSecurityResult::NoAuthority;
		return false;
	}

	OutResult = ESuspenseCoreSecurityResult::Allowed;
	return true;
}

bool USuspenseCoreSecurityValidator::CheckComponentAuthority(UActorComponent* Component,
	const FString& OperationName) const
{
	if (!Component)
	{
		UE_LOG(LogSuspenseCoreSecurity, Verbose, TEXT("%s: Component is null"), *OperationName);
		return false;
	}

	return CheckAuthority(Component->GetOwner(), OperationName);
}

bool USuspenseCoreSecurityValidator::CheckObjectAuthority(UObject* Object, const FString& OperationName) const
{
	if (!Object)
	{
		return false;
	}

	// Try to find outer actor
	AActor* Actor = Object->GetTypedOuter<AActor>();
	if (Actor)
	{
		return CheckAuthority(Actor, OperationName);
	}

	// Try component
	UActorComponent* Component = Cast<UActorComponent>(Object);
	if (Component)
	{
		return CheckComponentAuthority(Component, OperationName);
	}

	UE_LOG(LogSuspenseCoreSecurity, Warning, TEXT("%s: Cannot determine authority for %s"),
		*OperationName, *Object->GetName());
	return false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// RATE LIMITING
// ═══════════════════════════════════════════════════════════════════════════════

bool USuspenseCoreSecurityValidator::CheckRateLimit(AActor* Actor, const FString& OperationName,
	float MaxPerSecond)
{
	if (!bEnableRateLimiting || !Actor)
	{
		return true; // Allowed if disabled or no actor
	}

	const double CurrentTime = FPlatformTime::Seconds();
	const uint32 ActorHash = GetActorHash(Actor);

	// Get or create operation map for actor
	TMap<FString, FSuspenseCoreRateLimitEntry>& OperationMap = RateLimitMap.FindOrAdd(ActorHash);

	// Get or create entry for operation
	FSuspenseCoreRateLimitEntry& Entry = OperationMap.FindOrAdd(OperationName);

	// Check rate limit
	if (Entry.IsRateLimited(MaxPerSecond, CurrentTime))
	{
		// Rate limited - log violation
		HandleViolation(Actor, OperationName, ESuspenseCoreSecurityResult::RateLimited,
			FString::Printf(TEXT("Exceeded %.1f ops/sec"), MaxPerSecond));

		UE_LOG(LogSuspenseCoreSecurity, Warning, TEXT("Rate limit exceeded: %s performing %s (%.1f/sec limit)"),
			*Actor->GetName(), *OperationName, MaxPerSecond);

		return false; // Blocked
	}

	return true; // Allowed
}

void USuspenseCoreSecurityValidator::ResetRateLimit(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	const uint32 ActorHash = GetActorHash(Actor);
	RateLimitMap.Remove(ActorHash);

	UE_LOG(LogSuspenseCoreSecurity, Log, TEXT("Rate limit reset for %s"), *Actor->GetName());
}

void USuspenseCoreSecurityValidator::ResetAllRateLimits()
{
	RateLimitMap.Empty();
	UE_LOG(LogSuspenseCoreSecurity, Log, TEXT("All rate limits reset"));
}

// ═══════════════════════════════════════════════════════════════════════════════
// RPC VALIDATION
// ═══════════════════════════════════════════════════════════════════════════════

bool USuspenseCoreSecurityValidator::ValidateItemRPC(FName ItemID, int32 Quantity, int32 MaxQuantity) const
{
	if (ItemID.IsNone())
	{
		UE_LOG(LogSuspenseCoreSecurity, Warning, TEXT("ValidateItemRPC: ItemID is None"));
		return false;
	}

	if (Quantity <= 0)
	{
		UE_LOG(LogSuspenseCoreSecurity, Warning, TEXT("ValidateItemRPC: Quantity %d <= 0"), Quantity);
		return false;
	}

	if (Quantity > MaxQuantity)
	{
		UE_LOG(LogSuspenseCoreSecurity, Warning, TEXT("ValidateItemRPC: Quantity %d > MaxQuantity %d"),
			Quantity, MaxQuantity);
		return false;
	}

	return true;
}

bool USuspenseCoreSecurityValidator::ValidateGUID(const FGuid& InstanceID) const
{
	if (!InstanceID.IsValid())
	{
		UE_LOG(LogSuspenseCoreSecurity, Warning, TEXT("ValidateGUID: Invalid GUID"));
		return false;
	}

	return true;
}

bool USuspenseCoreSecurityValidator::ValidateSlotIndex(int32 SlotIndex, int32 MaxSlots) const
{
	if (SlotIndex < 0)
	{
		UE_LOG(LogSuspenseCoreSecurity, Warning, TEXT("ValidateSlotIndex: SlotIndex %d < 0"), SlotIndex);
		return false;
	}

	if (SlotIndex >= MaxSlots)
	{
		UE_LOG(LogSuspenseCoreSecurity, Warning, TEXT("ValidateSlotIndex: SlotIndex %d >= MaxSlots %d"),
			SlotIndex, MaxSlots);
		return false;
	}

	return true;
}

bool USuspenseCoreSecurityValidator::ValidateQuantity(int32 Quantity, int32 MinQuantity, int32 MaxQuantity) const
{
	if (Quantity < MinQuantity || Quantity > MaxQuantity)
	{
		UE_LOG(LogSuspenseCoreSecurity, Warning, TEXT("ValidateQuantity: %d not in range [%d, %d]"),
			Quantity, MinQuantity, MaxQuantity);
		return false;
	}

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// SUSPICIOUS ACTIVITY
// ═══════════════════════════════════════════════════════════════════════════════

bool USuspenseCoreSecurityValidator::CheckSuspiciousActivity(AActor* Actor, const FString& OperationName,
	ESuspenseCoreSecurityLevel Level)
{
	if (!bEnableSuspiciousActivityDetection || !Actor)
	{
		return true;
	}

	// Check violation count
	const int32 ViolationCount = GetViolationCount(Actor);

	// Higher security levels have stricter thresholds
	int32 MaxAllowedViolations = MaxViolationsBeforeKick;
	switch (Level)
	{
		case ESuspenseCoreSecurityLevel::Critical:
			MaxAllowedViolations = FMath::Max(1, MaxAllowedViolations / 4);
			break;
		case ESuspenseCoreSecurityLevel::High:
			MaxAllowedViolations = FMath::Max(2, MaxAllowedViolations / 2);
			break;
		case ESuspenseCoreSecurityLevel::Normal:
			// Default
			break;
		case ESuspenseCoreSecurityLevel::Low:
			MaxAllowedViolations = MaxAllowedViolations * 2;
			break;
	}

	if (ViolationCount >= MaxAllowedViolations)
	{
		HandleViolation(Actor, OperationName, ESuspenseCoreSecurityResult::SuspiciousActivity,
			FString::Printf(TEXT("Too many violations (%d)"), ViolationCount));
		return false;
	}

	return true;
}

void USuspenseCoreSecurityValidator::ReportSuspiciousActivity(AActor* Actor, const FString& Reason)
{
	if (!Actor)
	{
		return;
	}

	UE_LOG(LogSuspenseCoreSecurity, Warning, TEXT("Suspicious activity reported: %s - %s"),
		*Actor->GetName(), *Reason);

	HandleViolation(Actor, TEXT("SuspiciousActivity"), ESuspenseCoreSecurityResult::SuspiciousActivity, Reason);
}

// ═══════════════════════════════════════════════════════════════════════════════
// VIOLATION TRACKING
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreSecurityValidator::LogViolation(const FSuspenseCoreSecurityViolation& Violation)
{
	if (!bEnableViolationLogging)
	{
		return;
	}

	ViolationLog.Add(Violation);

	// Log to output
	UE_LOG(LogSuspenseCoreSecurity, Warning, TEXT("%s"), *Violation.ToLogString());

	// Broadcast event
	if (AActor* ViolatorActor = Violation.Violator.Get())
	{
		BroadcastSecurityEvent(
			FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Security.ViolationDetected")),
			ViolatorActor,
			Violation.Context);
	}
}

int32 USuspenseCoreSecurityValidator::GetViolationCount(AActor* Actor) const
{
	if (!Actor)
	{
		return 0;
	}

	const uint32 ActorHash = GetActorHash(const_cast<AActor*>(Actor));
	const int32* Count = ViolationCounts.Find(ActorHash);
	return Count ? *Count : 0;
}

void USuspenseCoreSecurityValidator::ClearViolations(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	const uint32 ActorHash = GetActorHash(Actor);
	ViolationCounts.Remove(ActorHash);

	// Remove from log
	ViolationLog.RemoveAll([Actor](const FSuspenseCoreSecurityViolation& V)
	{
		return V.Violator.Get() == Actor;
	});
}

void USuspenseCoreSecurityValidator::ClearAllViolations()
{
	ViolationCounts.Empty();
	ViolationLog.Empty();
}

// ═══════════════════════════════════════════════════════════════════════════════
// EVENTBUS INTEGRATION
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreSecurityValidator::BroadcastSecurityEvent(FGameplayTag EventTag, AActor* Actor,
	const FString& Context)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	// Build event data
	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(Actor, ESuspenseCoreEventPriority::High);
	EventData.SetString(TEXT("Context"), Context);
	EventData.SetObject(TEXT("Actor"), Actor);

	// Publish
	EventBus->Publish(EventTag, EventData);
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL METHODS
// ═══════════════════════════════════════════════════════════════════════════════

uint32 USuspenseCoreSecurityValidator::GetActorHash(AActor* Actor) const
{
	if (!Actor)
	{
		return 0;
	}

	// Use object ID for consistent hashing
	return GetTypeHash(Actor->GetUniqueID());
}

void USuspenseCoreSecurityValidator::HandleViolation(AActor* Actor, const FString& OperationName,
	ESuspenseCoreSecurityResult Result, const FString& Context)
{
	if (!Actor)
	{
		return;
	}

	// Increment violation count
	const uint32 ActorHash = GetActorHash(Actor);
	int32& Count = ViolationCounts.FindOrAdd(ActorHash);
	Count++;

	// Create violation record
	FSuspenseCoreSecurityViolation Violation(Actor, OperationName, Result, Context);
	Violation.ViolationCount = Count;

	// Log violation
	LogViolation(Violation);

	// Check for kick
	if (MaxViolationsBeforeKick > 0 && Count >= MaxViolationsBeforeKick)
	{
		TryKickPlayer(Actor, FString::Printf(TEXT("Too many security violations (%d)"), Count));
	}
}

void USuspenseCoreSecurityValidator::TryKickPlayer(AActor* Actor, const FString& Reason)
{
	if (!Actor)
	{
		return;
	}

	UE_LOG(LogSuspenseCoreSecurity, Warning, TEXT("Attempting to kick %s: %s"), *Actor->GetName(), *Reason);

	// Broadcast kick event
	BroadcastSecurityEvent(
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Security.PlayerKicked")),
		Actor,
		Reason);

	// Try to find and kick player controller
	APlayerController* PC = nullptr;

	// Check if actor is a pawn
	if (APawn* Pawn = Cast<APawn>(Actor))
	{
		PC = Cast<APlayerController>(Pawn->GetController());
	}
	// Check if actor is a player controller
	else if (APlayerController* DirectPC = Cast<APlayerController>(Actor))
	{
		PC = DirectPC;
	}
	// Check if actor is a player state
	else if (APlayerState* PS = Cast<APlayerState>(Actor))
	{
		PC = PS->GetPlayerController();
	}

	if (PC)
	{
		// Note: Actually kicking requires game-specific implementation
		// This broadcasts the event for the game to handle
		UE_LOG(LogSuspenseCoreSecurity, Warning, TEXT("Player kick requested for %s"),
			*PC->GetName());
	}
}

void USuspenseCoreSecurityValidator::DecayViolations()
{
	const double CurrentTime = FPlatformTime::Seconds();

	// Remove old violations
	ViolationLog.RemoveAll([this, CurrentTime](const FSuspenseCoreSecurityViolation& V)
	{
		return (CurrentTime - V.Timestamp) > ViolationDecayTime;
	});

	// Note: ViolationCounts would need periodic cleanup in a real implementation
}

USuspenseCoreEventBus* USuspenseCoreSecurityValidator::GetEventBus()
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	// Try to get EventBus from EventManager
	USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this);
	if (EventManager)
	{
		CachedEventBus = EventManager->GetEventBus();
	}

	return CachedEventBus.Get();
}
