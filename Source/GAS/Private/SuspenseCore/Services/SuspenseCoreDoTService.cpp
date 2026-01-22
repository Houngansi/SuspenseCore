// SuspenseCoreDoTService.cpp
// Service for tracking and managing Damage-over-Time effects
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreDoTService.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

// Log category
DEFINE_LOG_CATEGORY_STATIC(LogDoTService, Log, All);

#define DOT_LOG(Verbosity, Format, ...) \
	UE_LOG(LogDoTService, Verbosity, TEXT("[DoTService] ") Format, ##__VA_ARGS__)

//========================================================================
// FSuspenseCoreActiveDoT Implementation
//========================================================================

bool FSuspenseCoreActiveDoT::IsBleeding() const
{
	return DoTType.MatchesTag(FGameplayTag::RequestGameplayTag(FName("State.Health.Bleeding")));
}

bool FSuspenseCoreActiveDoT::IsBurning() const
{
	return DoTType.MatchesTag(FGameplayTag::RequestGameplayTag(FName("State.Burning")));
}

FText FSuspenseCoreActiveDoT::GetDisplayName() const
{
	if (DoTType == SuspenseCoreTags::State::Health::BleedingLight)
	{
		return NSLOCTEXT("DoT", "BleedLight", "Bleeding");
	}
	else if (DoTType == SuspenseCoreTags::State::Health::BleedingHeavy)
	{
		return NSLOCTEXT("DoT", "BleedHeavy", "Heavy Bleeding");
	}
	else if (DoTType == SuspenseCoreTags::State::Burning)
	{
		return NSLOCTEXT("DoT", "Burning", "Burning");
	}

	return FText::FromString(DoTType.ToString());
}

FString FSuspenseCoreActiveDoT::GetIconPath() const
{
	if (IsBleeding())
	{
		return TEXT("/Game/UI/Icons/Debuffs/T_Icon_Bleeding");
	}
	else if (IsBurning())
	{
		return TEXT("/Game/UI/Icons/Debuffs/T_Icon_Burning");
	}

	return TEXT("/Game/UI/Icons/Debuffs/T_Icon_Generic");
}

//========================================================================
// FSuspenseCoreDoTEventPayload Implementation
//========================================================================

FSuspenseCoreEventData FSuspenseCoreDoTEventPayload::ToEventData() const
{
	FSuspenseCoreEventData Data = FSuspenseCoreEventData::Create(AffectedActor.Get());

	Data.SetObject(TEXT("AffectedActor"), AffectedActor.Get());
	Data.SetTag(TEXT("DoTType"), DoTType);
	Data.SetFloat(TEXT("DamagePerTick"), DoTData.DamagePerTick);
	Data.SetFloat(TEXT("TickInterval"), DoTData.TickInterval);
	Data.SetFloat(TEXT("RemainingDuration"), DoTData.RemainingDuration);
	Data.SetInt(TEXT("StackCount"), DoTData.StackCount);
	Data.SetFloat(TEXT("DamageDealt"), DamageDealt);

	return Data;
}

FSuspenseCoreDoTEventPayload FSuspenseCoreDoTEventPayload::FromEventData(const FSuspenseCoreEventData& EventData)
{
	FSuspenseCoreDoTEventPayload Payload;

	Payload.AffectedActor = Cast<AActor>(EventData.GetObject(TEXT("AffectedActor")));
	Payload.DoTType = EventData.GetTag(TEXT("DoTType"));
	Payload.DoTData.DamagePerTick = EventData.GetFloat(TEXT("DamagePerTick"));
	Payload.DoTData.TickInterval = EventData.GetFloat(TEXT("TickInterval"));
	Payload.DoTData.RemainingDuration = EventData.GetFloat(TEXT("RemainingDuration"));
	Payload.DoTData.StackCount = EventData.GetInt(TEXT("StackCount"));
	Payload.DamageDealt = EventData.GetFloat(TEXT("DamageDealt"));

	return Payload;
}

//========================================================================
// USuspenseCoreDoTService Implementation
//========================================================================

void USuspenseCoreDoTService::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	DOT_LOG(Log, TEXT("Initializing DoT Service..."));

	// Initialize EventBus connection (deferred to first use if not ready)
	InitializeEventBus();

	// Start duration update timer (every 0.5 seconds)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DurationUpdateTimerHandle,
			this,
			&USuspenseCoreDoTService::OnDurationUpdateTimer,
			0.5f,  // Update every 0.5 seconds
			true   // Looping
		);
	}

	DOT_LOG(Log, TEXT("DoT Service initialized successfully"));
}

void USuspenseCoreDoTService::Deinitialize()
{
	DOT_LOG(Log, TEXT("Shutting down DoT Service..."));

	// Clear timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DurationUpdateTimerHandle);
	}

	// Clear all tracked DoTs
	{
		FScopeLock Lock(&DoTLock);
		ActiveDoTs.Empty();
	}

	EventBus.Reset();

	Super::Deinitialize();
}

USuspenseCoreDoTService* USuspenseCoreDoTService::Get(const UObject* WorldContextObject)
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

	return GameInstance->GetSubsystem<USuspenseCoreDoTService>();
}

//========================================================================
// Query API
//========================================================================

TArray<FSuspenseCoreActiveDoT> USuspenseCoreDoTService::GetActiveDoTs(AActor* Target) const
{
	if (!Target)
	{
		return TArray<FSuspenseCoreActiveDoT>();
	}

	FScopeLock Lock(&DoTLock);

	const TArray<FSuspenseCoreActiveDoT>* DoTs = ActiveDoTs.Find(Target);
	if (DoTs)
	{
		return *DoTs;
	}

	return TArray<FSuspenseCoreActiveDoT>();
}

bool USuspenseCoreDoTService::HasActiveBleeding(AActor* Target) const
{
	if (!Target)
	{
		return false;
	}

	FScopeLock Lock(&DoTLock);

	const TArray<FSuspenseCoreActiveDoT>* DoTs = ActiveDoTs.Find(Target);
	if (!DoTs)
	{
		return false;
	}

	for (const FSuspenseCoreActiveDoT& DoT : *DoTs)
	{
		if (DoT.IsBleeding())
		{
			return true;
		}
	}

	return false;
}

bool USuspenseCoreDoTService::HasActiveBurning(AActor* Target) const
{
	if (!Target)
	{
		return false;
	}

	FScopeLock Lock(&DoTLock);

	const TArray<FSuspenseCoreActiveDoT>* DoTs = ActiveDoTs.Find(Target);
	if (!DoTs)
	{
		return false;
	}

	for (const FSuspenseCoreActiveDoT& DoT : *DoTs)
	{
		if (DoT.IsBurning())
		{
			return true;
		}
	}

	return false;
}

float USuspenseCoreDoTService::GetBleedDamagePerSecond(AActor* Target) const
{
	if (!Target)
	{
		return 0.0f;
	}

	FScopeLock Lock(&DoTLock);

	const TArray<FSuspenseCoreActiveDoT>* DoTs = ActiveDoTs.Find(Target);
	if (!DoTs)
	{
		return 0.0f;
	}

	float TotalDPS = 0.0f;
	for (const FSuspenseCoreActiveDoT& DoT : *DoTs)
	{
		if (DoT.IsBleeding() && DoT.TickInterval > 0.0f)
		{
			TotalDPS += DoT.DamagePerTick / DoT.TickInterval;
		}
	}

	return TotalDPS;
}

float USuspenseCoreDoTService::GetBurnTimeRemaining(AActor* Target) const
{
	if (!Target)
	{
		return -1.0f;
	}

	FScopeLock Lock(&DoTLock);

	const TArray<FSuspenseCoreActiveDoT>* DoTs = ActiveDoTs.Find(Target);
	if (!DoTs)
	{
		return -1.0f;
	}

	float ShortestRemaining = -1.0f;
	for (const FSuspenseCoreActiveDoT& DoT : *DoTs)
	{
		if (DoT.IsBurning() && !DoT.IsInfinite())
		{
			if (ShortestRemaining < 0.0f || DoT.RemainingDuration < ShortestRemaining)
			{
				ShortestRemaining = DoT.RemainingDuration;
			}
		}
	}

	return ShortestRemaining;
}

int32 USuspenseCoreDoTService::GetActiveDoTCount(AActor* Target) const
{
	if (!Target)
	{
		return 0;
	}

	FScopeLock Lock(&DoTLock);

	const TArray<FSuspenseCoreActiveDoT>* DoTs = ActiveDoTs.Find(Target);
	if (DoTs)
	{
		return DoTs->Num();
	}

	return 0;
}

//========================================================================
// Registration API
//========================================================================

void USuspenseCoreDoTService::RegisterDoTApplied(
	AActor* Target,
	FGameplayTag DoTType,
	float DamagePerTick,
	float TickInterval,
	float Duration,
	AActor* Source)
{
	if (!Target || !DoTType.IsValid())
	{
		DOT_LOG(Warning, TEXT("RegisterDoTApplied: Invalid parameters"));
		return;
	}

	DOT_LOG(Log, TEXT("DoT Applied: %s on %s (%.1f dmg/%.1fs, duration: %.1fs)"),
		*DoTType.ToString(), *Target->GetName(), DamagePerTick, TickInterval, Duration);

	FScopeLock Lock(&DoTLock);

	// Find or create entry
	TArray<FSuspenseCoreActiveDoT>& TargetDoTs = ActiveDoTs.FindOrAdd(Target);

	// Check if this type already exists (refresh instead of add)
	FSuspenseCoreActiveDoT* ExistingDoT = nullptr;
	for (FSuspenseCoreActiveDoT& DoT : TargetDoTs)
	{
		if (DoT.DoTType == DoTType)
		{
			ExistingDoT = &DoT;
			break;
		}
	}

	FSuspenseCoreActiveDoT NewDoT;
	NewDoT.DoTType = DoTType;
	NewDoT.DamagePerTick = DamagePerTick;
	NewDoT.TickInterval = TickInterval;
	NewDoT.RemainingDuration = Duration;
	NewDoT.StackCount = 1;
	NewDoT.ApplicationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	NewDoT.SourceActor = Source;

	if (ExistingDoT)
	{
		// Refresh existing (update values, increment stack if applicable)
		ExistingDoT->RemainingDuration = Duration;
		ExistingDoT->DamagePerTick = FMath::Max(ExistingDoT->DamagePerTick, DamagePerTick);
		// Don't increment stack count for refresh
	}
	else
	{
		// Add new DoT
		TargetDoTs.Add(NewDoT);
	}

	// Publish event
	FSuspenseCoreDoTEventPayload Payload;
	Payload.AffectedActor = Target;
	Payload.DoTType = DoTType;
	Payload.DoTData = ExistingDoT ? *ExistingDoT : NewDoT;

	PublishDoTEvent(SuspenseCoreTags::Event::DoT::Applied, Payload);
}

void USuspenseCoreDoTService::RegisterDoTTick(
	AActor* Target,
	FGameplayTag DoTType,
	float DamageDealt)
{
	if (!Target || !DoTType.IsValid())
	{
		return;
	}

	DOT_LOG(Verbose, TEXT("DoT Tick: %s on %s (%.1f dmg)"),
		*DoTType.ToString(), *Target->GetName(), DamageDealt);

	FScopeLock Lock(&DoTLock);

	FSuspenseCoreActiveDoT* DoTEntry = FindDoTEntry(Target, DoTType);
	if (!DoTEntry)
	{
		DOT_LOG(Warning, TEXT("DoT Tick for untracked effect: %s"), *DoTType.ToString());
		return;
	}

	// Publish tick event
	FSuspenseCoreDoTEventPayload Payload;
	Payload.AffectedActor = Target;
	Payload.DoTType = DoTType;
	Payload.DoTData = *DoTEntry;
	Payload.DamageDealt = DamageDealt;

	PublishDoTEvent(SuspenseCoreTags::Event::DoT::Tick, Payload);
}

void USuspenseCoreDoTService::RegisterDoTRemoved(
	AActor* Target,
	FGameplayTag DoTType,
	bool bExpired)
{
	if (!Target || !DoTType.IsValid())
	{
		return;
	}

	DOT_LOG(Log, TEXT("DoT %s: %s on %s"),
		bExpired ? TEXT("Expired") : TEXT("Removed"),
		*DoTType.ToString(), *Target->GetName());

	FScopeLock Lock(&DoTLock);

	TArray<FSuspenseCoreActiveDoT>* TargetDoTs = ActiveDoTs.Find(Target);
	if (!TargetDoTs)
	{
		return;
	}

	// Find and remove
	FSuspenseCoreActiveDoT RemovedDoT;
	bool bFound = false;
	for (int32 i = TargetDoTs->Num() - 1; i >= 0; --i)
	{
		if ((*TargetDoTs)[i].DoTType == DoTType)
		{
			RemovedDoT = (*TargetDoTs)[i];
			TargetDoTs->RemoveAt(i);
			bFound = true;
			break;
		}
	}

	// Clean up empty arrays
	if (TargetDoTs->Num() == 0)
	{
		ActiveDoTs.Remove(Target);
	}

	if (bFound)
	{
		// Publish event
		FSuspenseCoreDoTEventPayload Payload;
		Payload.AffectedActor = Target;
		Payload.DoTType = DoTType;
		Payload.DoTData = RemovedDoT;

		FGameplayTag EventTag = bExpired ?
			SuspenseCoreTags::Event::DoT::Expired :
			SuspenseCoreTags::Event::DoT::Removed;

		PublishDoTEvent(EventTag, Payload);
	}
}

//========================================================================
// Internal Methods
//========================================================================

void USuspenseCoreDoTService::InitializeEventBus()
{
	// Get EventBus from GameInstance
	if (UGameInstance* GI = GetGameInstance())
	{
		// Try to find EventBus subsystem or from ServiceLocator
		// This depends on your EventBus initialization pattern
		// For now, we'll use deferred lookup
		EventBus = nullptr;  // Will be set on first publish
	}
}

void USuspenseCoreDoTService::CleanupStaleEntries()
{
	FScopeLock Lock(&DoTLock);

	// Remove entries for destroyed actors
	TArray<TWeakObjectPtr<AActor>> ToRemove;
	for (auto& Pair : ActiveDoTs)
	{
		if (!Pair.Key.IsValid())
		{
			ToRemove.Add(Pair.Key);
		}
	}

	for (const auto& Key : ToRemove)
	{
		ActiveDoTs.Remove(Key);
	}
}

void USuspenseCoreDoTService::PublishDoTEvent(FGameplayTag EventTag, const FSuspenseCoreDoTEventPayload& Payload)
{
	// Get EventBus if not cached
	if (!EventBus.IsValid())
	{
		// Try to get from world
		if (UWorld* World = GetWorld())
		{
			// This assumes EventBus is accessible via a subsystem or singleton
			// Adjust based on your EventBus initialization pattern
			if (UGameInstance* GI = World->GetGameInstance())
			{
				// Look for EventBus in the game instance's subsystems or components
				// This is a placeholder - adjust to your actual EventBus location
				for (TFieldIterator<FObjectProperty> PropIt(GI->GetClass()); PropIt; ++PropIt)
				{
					FObjectProperty* Prop = *PropIt;
					if (Prop->PropertyClass->IsChildOf(USuspenseCoreEventBus::StaticClass()))
					{
						EventBus = Cast<USuspenseCoreEventBus>(Prop->GetObjectPropertyValue_InContainer(GI));
						break;
					}
				}
			}
		}
	}

	if (EventBus.IsValid())
	{
		FSuspenseCoreEventData EventData = Payload.ToEventData();
		EventBus->Publish(EventTag, EventData);
	}
	else
	{
		DOT_LOG(Warning, TEXT("EventBus not available for DoT event: %s"), *EventTag.ToString());
	}
}

FSuspenseCoreActiveDoT* USuspenseCoreDoTService::FindDoTEntry(AActor* Target, FGameplayTag DoTType)
{
	// Note: Caller must hold DoTLock

	TArray<FSuspenseCoreActiveDoT>* TargetDoTs = ActiveDoTs.Find(Target);
	if (!TargetDoTs)
	{
		return nullptr;
	}

	for (FSuspenseCoreActiveDoT& DoT : *TargetDoTs)
	{
		if (DoT.DoTType == DoTType)
		{
			return &DoT;
		}
	}

	return nullptr;
}

void USuspenseCoreDoTService::UpdateDurations(float DeltaTime)
{
	FScopeLock Lock(&DoTLock);

	TArray<TPair<TWeakObjectPtr<AActor>, FGameplayTag>> ToExpire;

	for (auto& Pair : ActiveDoTs)
	{
		if (!Pair.Key.IsValid())
		{
			continue;
		}

		for (FSuspenseCoreActiveDoT& DoT : Pair.Value)
		{
			// Skip infinite duration effects (bleeding)
			if (DoT.IsInfinite())
			{
				continue;
			}

			DoT.RemainingDuration -= DeltaTime;

			if (DoT.RemainingDuration <= 0.0f)
			{
				ToExpire.Add(TPair<TWeakObjectPtr<AActor>, FGameplayTag>(Pair.Key, DoT.DoTType));
			}
		}
	}

	// Unlock before calling RegisterDoTRemoved (which will reacquire lock)
	Lock.Unlock();

	// Process expirations
	for (const auto& Expired : ToExpire)
	{
		if (Expired.Key.IsValid())
		{
			RegisterDoTRemoved(Expired.Key.Get(), Expired.Value, true);  // bExpired = true
		}
	}
}

void USuspenseCoreDoTService::OnDurationUpdateTimer()
{
	// Cleanup stale entries periodically
	CleanupStaleEntries();

	// Update durations (0.5s delta since timer fires every 0.5s)
	UpdateDurations(0.5f);
}
