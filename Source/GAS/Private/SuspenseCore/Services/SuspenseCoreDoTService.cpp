// SuspenseCoreDoTService.cpp
// ASC-driven DoT tracking implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreDoTService.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "ActiveGameplayEffectHandle.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDoTService, Log, All);

//==================================================================
// FSuspenseCoreActiveDoT Helpers
//==================================================================

bool FSuspenseCoreActiveDoT::IsBleeding() const
{
	return DoTType.MatchesTag(FGameplayTag::RequestGameplayTag(FName("State.Health.Bleeding")));
}

bool FSuspenseCoreActiveDoT::IsBurning() const
{
	return DoTType.MatchesTag(FGameplayTag::RequestGameplayTag(FName("State.Burning"))) ||
		   DoTType.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Effect.DoT.Burn")));
}

FText FSuspenseCoreActiveDoT::GetDisplayName() const
{
	if (IsBleeding())
	{
		if (DoTType.MatchesTagExact(SuspenseCoreTags::State::Health::BleedingHeavy))
		{
			return NSLOCTEXT("DoT", "HeavyBleed", "Heavy Bleeding");
		}
		return NSLOCTEXT("DoT", "LightBleed", "Bleeding");
	}
	if (IsBurning())
	{
		return NSLOCTEXT("DoT", "Burning", "Burning");
	}
	return NSLOCTEXT("DoT", "Unknown", "Unknown Effect");
}

FString FSuspenseCoreActiveDoT::GetIconPath() const
{
	if (IsBleeding())
	{
		if (DoTType.MatchesTagExact(SuspenseCoreTags::State::Health::BleedingHeavy))
		{
			return TEXT("/Game/UI/Icons/Debuffs/T_Debuff_BleedHeavy");
		}
		return TEXT("/Game/UI/Icons/Debuffs/T_Debuff_BleedLight");
	}
	if (IsBurning())
	{
		return TEXT("/Game/UI/Icons/Debuffs/T_Debuff_Burning");
	}
	return TEXT("/Game/UI/Icons/Debuffs/T_Debuff_Unknown");
}

//==================================================================
// FSuspenseCoreDoTEventPayload
//==================================================================

FSuspenseCoreEventData FSuspenseCoreDoTEventPayload::ToEventData() const
{
	FSuspenseCoreEventData Data;
	Data.Source = AffectedActor.Get();
	Data.Timestamp = FPlatformTime::Seconds();

	// Add DoTType to both Tags container (for HasTag checks) and StringPayload (for string lookups)
	if (DoTType.IsValid())
	{
		Data.Tags.AddTag(DoTType);
	}
	Data.StringPayload.Add(TEXT("DoTType"), DoTType.ToString());
	Data.FloatPayload.Add(TEXT("DamagePerTick"), DoTData.DamagePerTick);
	Data.FloatPayload.Add(TEXT("TickInterval"), DoTData.TickInterval);
	Data.FloatPayload.Add(TEXT("RemainingDuration"), DoTData.RemainingDuration);
	Data.FloatPayload.Add(TEXT("Duration"), DoTData.RemainingDuration);  // Alias for UI widgets
	Data.FloatPayload.Add(TEXT("DamageDealt"), DamageDealt);
	Data.IntPayload.Add(TEXT("StackCount"), DoTData.StackCount);

	return Data;
}

FSuspenseCoreDoTEventPayload FSuspenseCoreDoTEventPayload::FromEventData(const FSuspenseCoreEventData& EventData)
{
	FSuspenseCoreDoTEventPayload Payload;
	Payload.AffectedActor = Cast<AActor>(EventData.Source.Get());

	if (const FString* TypeStr = EventData.StringPayload.Find(TEXT("DoTType")))
	{
		Payload.DoTType = FGameplayTag::RequestGameplayTag(FName(**TypeStr));
	}

	if (const float* Val = EventData.FloatPayload.Find(TEXT("DamagePerTick")))
	{
		Payload.DoTData.DamagePerTick = *Val;
	}
	if (const float* Val = EventData.FloatPayload.Find(TEXT("TickInterval")))
	{
		Payload.DoTData.TickInterval = *Val;
	}
	if (const float* Val = EventData.FloatPayload.Find(TEXT("RemainingDuration")))
	{
		Payload.DoTData.RemainingDuration = *Val;
	}
	if (const float* Val = EventData.FloatPayload.Find(TEXT("DamageDealt")))
	{
		Payload.DamageDealt = *Val;
	}
	if (const int32* Val = EventData.IntPayload.Find(TEXT("StackCount")))
	{
		Payload.DoTData.StackCount = *Val;
	}

	return Payload;
}

//==================================================================
// Subsystem Lifecycle
//==================================================================

void USuspenseCoreDoTService::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Cache root tags for filtering
	DoTRootTag = FGameplayTag::RequestGameplayTag(FName("Effect.DoT"), false);
	BleedingRootTag = FGameplayTag::RequestGameplayTag(FName("State.Health.Bleeding"), false);
	BurningRootTag = FGameplayTag::RequestGameplayTag(FName("State.Burning"), false);

	InitializeEventBus();

	// Start duration update timer for legacy cache
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DurationUpdateTimerHandle,
			this,
			&USuspenseCoreDoTService::OnDurationUpdateTimer,
			0.1f,  // 10 Hz update
			true
		);
	}

	UE_LOG(LogDoTService, Log, TEXT("DoT Service initialized (ASC-driven mode)"));
}

void USuspenseCoreDoTService::Deinitialize()
{
	// Clear timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DurationUpdateTimerHandle);
	}

	// Unbind from all ASCs
	for (const FSuspenseCoreASCBinding& Binding : BoundASCs)
	{
		if (Binding.ASC.IsValid())
		{
			// Manually remove delegates (avoid recursive call)
			Binding.ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(Binding.OnEffectAddedHandle);
			Binding.ASC->OnAnyGameplayEffectRemovedDelegate().Remove(Binding.OnEffectRemovedHandle);
		}
	}
	BoundASCs.Empty();

	// Clear legacy cache
	ManualDoTCache.Empty();

	Super::Deinitialize();
}

//==================================================================
// Static Access
//==================================================================

USuspenseCoreDoTService* USuspenseCoreDoTService::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return nullptr;
	}

	return GI->GetSubsystem<USuspenseCoreDoTService>();
}

//==================================================================
// IISuspenseCoreDoTService Implementation - Query API
//==================================================================

TArray<FSuspenseCoreActiveDoT> USuspenseCoreDoTService::GetActiveDoTs(AActor* Target) const
{
	TArray<FSuspenseCoreActiveDoT> Result;

	if (!Target)
	{
		return Result;
	}

	// PRIMARY: Query from ASC directly (source of truth)
	UAbilitySystemComponent* ASC = GetASCFromActor(Target);
	if (ASC)
	{
		// Get all active effects with DoT tags
		FGameplayEffectQuery Query;
		TArray<FActiveGameplayEffectHandle> ActiveHandles = ASC->GetActiveEffects(Query);

		for (const FActiveGameplayEffectHandle& Handle : ActiveHandles)
		{
			const FActiveGameplayEffect* ActiveEffect = ASC->GetActiveGameplayEffect(Handle);
			if (ActiveEffect && IsDoTEffect(*ActiveEffect))
			{
				Result.Add(BuildDoTDataFromEffect(*ActiveEffect, Target));
			}
		}
	}

	// FALLBACK: Check legacy manual cache
	FScopeLock Lock(&ServiceLock);
	if (const TArray<FSuspenseCoreActiveDoT>* ManualDoTs = ManualDoTCache.Find(Target))
	{
		for (const FSuspenseCoreActiveDoT& DoT : *ManualDoTs)
		{
			// Avoid duplicates (if ASC already has this)
			bool bAlreadyInResult = Result.ContainsByPredicate([&](const FSuspenseCoreActiveDoT& Existing) {
				return Existing.DoTType == DoT.DoTType;
			});

			if (!bAlreadyInResult)
			{
				Result.Add(DoT);
			}
		}
	}

	return Result;
}

bool USuspenseCoreDoTService::HasActiveBleeding(AActor* Target) const
{
	TArray<FSuspenseCoreActiveDoT> DoTs = GetActiveDoTs(Target);
	for (const FSuspenseCoreActiveDoT& DoT : DoTs)
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
	TArray<FSuspenseCoreActiveDoT> DoTs = GetActiveDoTs(Target);
	for (const FSuspenseCoreActiveDoT& DoT : DoTs)
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
	float TotalDPS = 0.0f;
	TArray<FSuspenseCoreActiveDoT> DoTs = GetActiveDoTs(Target);

	for (const FSuspenseCoreActiveDoT& DoT : DoTs)
	{
		if (DoT.IsBleeding() && DoT.TickInterval > 0.0f)
		{
			TotalDPS += (DoT.DamagePerTick / DoT.TickInterval) * DoT.StackCount;
		}
	}

	return TotalDPS;
}

float USuspenseCoreDoTService::GetBurnTimeRemaining(AActor* Target) const
{
	float MinRemaining = -1.0f;
	TArray<FSuspenseCoreActiveDoT> DoTs = GetActiveDoTs(Target);

	for (const FSuspenseCoreActiveDoT& DoT : DoTs)
	{
		if (DoT.IsBurning() && !DoT.IsInfinite())
		{
			if (MinRemaining < 0.0f || DoT.RemainingDuration < MinRemaining)
			{
				MinRemaining = DoT.RemainingDuration;
			}
		}
	}

	return MinRemaining;
}

int32 USuspenseCoreDoTService::GetActiveDoTCount(AActor* Target) const
{
	return GetActiveDoTs(Target).Num();
}

bool USuspenseCoreDoTService::HasActiveDoTOfType(AActor* Target, FGameplayTag DoTType) const
{
	TArray<FSuspenseCoreActiveDoT> DoTs = GetActiveDoTs(Target);
	for (const FSuspenseCoreActiveDoT& DoT : DoTs)
	{
		if (DoT.DoTType.MatchesTag(DoTType))
		{
			return true;
		}
	}
	return false;
}

//==================================================================
// IISuspenseCoreDoTService Implementation - Registration API
//==================================================================

void USuspenseCoreDoTService::NotifyDoTApplied(
	AActor* Target,
	FGameplayTag DoTType,
	float DamagePerTick,
	float TickInterval,
	float Duration,
	AActor* Source)
{
	if (!Target)
	{
		return;
	}

	FScopeLock Lock(&ServiceLock);

	// Add to legacy cache (for non-ASC systems)
	TArray<FSuspenseCoreActiveDoT>& DoTs = ManualDoTCache.FindOrAdd(Target);

	// Check if already exists
	FSuspenseCoreActiveDoT* Existing = DoTs.FindByPredicate([&](const FSuspenseCoreActiveDoT& D) {
		return D.DoTType == DoTType;
	});

	FSuspenseCoreActiveDoT DoTData;
	DoTData.DoTType = DoTType;
	DoTData.DamagePerTick = DamagePerTick;
	DoTData.TickInterval = TickInterval;
	DoTData.RemainingDuration = Duration;
	DoTData.StackCount = Existing ? Existing->StackCount + 1 : 1;
	DoTData.ApplicationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	DoTData.SourceActor = Source;

	if (Existing)
	{
		*Existing = DoTData;
	}
	else
	{
		DoTs.Add(DoTData);
	}

	// Publish event (async for performance)
	FSuspenseCoreDoTEventPayload Payload;
	Payload.AffectedActor = Target;
	Payload.DoTType = DoTType;
	Payload.DoTData = DoTData;

	PublishDoTEvent(SuspenseCoreTags::Event::DoT::Applied, Payload);

	UE_LOG(LogDoTService, Verbose, TEXT("DoT applied (manual): %s on %s (DPS=%.1f)"),
		*DoTType.ToString(), *Target->GetName(),
		TickInterval > 0.0f ? DamagePerTick / TickInterval : 0.0f);
}

void USuspenseCoreDoTService::NotifyDoTRemoved(
	AActor* Target,
	FGameplayTag DoTType,
	bool bExpired)
{
	if (!Target)
	{
		return;
	}

	FScopeLock Lock(&ServiceLock);

	// Remove from legacy cache
	if (TArray<FSuspenseCoreActiveDoT>* DoTs = ManualDoTCache.Find(Target))
	{
		int32 RemovedIndex = DoTs->IndexOfByPredicate([&](const FSuspenseCoreActiveDoT& D) {
			return D.DoTType == DoTType;
		});

		FSuspenseCoreDoTEventPayload Payload;
		Payload.AffectedActor = Target;
		Payload.DoTType = DoTType;

		if (RemovedIndex != INDEX_NONE)
		{
			Payload.DoTData = (*DoTs)[RemovedIndex];
			DoTs->RemoveAt(RemovedIndex);
		}

		// Publish event
		FGameplayTag EventTag = bExpired
			? SuspenseCoreTags::Event::DoT::Expired
			: SuspenseCoreTags::Event::DoT::Removed;

		PublishDoTEvent(EventTag, Payload);

		UE_LOG(LogDoTService, Verbose, TEXT("DoT removed (manual): %s from %s (expired=%d)"),
			*DoTType.ToString(), *Target->GetName(), bExpired);
	}
}

//==================================================================
// ASC Binding
//==================================================================

void USuspenseCoreDoTService::BindToASC(UAbilitySystemComponent* ASC)
{
	if (!ASC)
	{
		return;
	}

	FScopeLock Lock(&ServiceLock);

	// Check if already bound
	for (const FSuspenseCoreASCBinding& Binding : BoundASCs)
	{
		if (Binding.ASC == ASC)
		{
			return;  // Already bound
		}
	}

	// Create new binding
	FSuspenseCoreASCBinding NewBinding;
	NewBinding.ASC = ASC;

	// Subscribe to effect added delegate
	NewBinding.OnEffectAddedHandle = ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(
		this, &USuspenseCoreDoTService::OnActiveGameplayEffectAdded);

	// Subscribe to effect removed delegate
	NewBinding.OnEffectRemovedHandle = ASC->OnAnyGameplayEffectRemovedDelegate().AddUObject(
		this, &USuspenseCoreDoTService::OnActiveGameplayEffectRemoved);

	BoundASCs.Add(NewBinding);

	UE_LOG(LogDoTService, Log, TEXT("Bound to ASC: %s"),
		ASC->GetOwner() ? *ASC->GetOwner()->GetName() : TEXT("Unknown"));
}

void USuspenseCoreDoTService::UnbindFromASC(UAbilitySystemComponent* ASC)
{
	if (!ASC)
	{
		return;
	}

	FScopeLock Lock(&ServiceLock);

	for (int32 i = BoundASCs.Num() - 1; i >= 0; --i)
	{
		FSuspenseCoreASCBinding& Binding = BoundASCs[i];
		if (Binding.ASC == ASC)
		{
			// Remove delegates
			ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(Binding.OnEffectAddedHandle);
			ASC->OnAnyGameplayEffectRemovedDelegate().Remove(Binding.OnEffectRemovedHandle);

			BoundASCs.RemoveAt(i);

			UE_LOG(LogDoTService, Log, TEXT("Unbound from ASC: %s"),
				ASC->GetOwner() ? *ASC->GetOwner()->GetName() : TEXT("Unknown"));
			return;
		}
	}
}

//==================================================================
// ASC Delegate Handlers
//==================================================================

void USuspenseCoreDoTService::OnActiveGameplayEffectAdded(
	UAbilitySystemComponent* ASC,
	const FGameplayEffectSpec& Spec,
	FActiveGameplayEffectHandle Handle)
{
	if (!ASC || !IsDoTEffect(Spec))
	{
		return;
	}

	AActor* TargetActor = ASC->GetOwner();
	FGameplayTag DoTType = GetDoTTypeFromEffect(Spec);

	// Get effect data
	const FActiveGameplayEffect* ActiveEffect = ASC->GetActiveGameplayEffect(Handle);
	if (!ActiveEffect)
	{
		return;
	}

	FSuspenseCoreActiveDoT DoTData = BuildDoTDataFromEffect(*ActiveEffect, TargetActor);

	// Publish event (async for performance)
	FSuspenseCoreDoTEventPayload Payload;
	Payload.AffectedActor = TargetActor;
	Payload.DoTType = DoTType;
	Payload.DoTData = DoTData;

	PublishDoTEvent(SuspenseCoreTags::Event::DoT::Applied, Payload);

	UE_LOG(LogDoTService, Log, TEXT("DoT added (ASC): %s on %s"),
		*DoTType.ToString(), TargetActor ? *TargetActor->GetName() : TEXT("Unknown"));
}

void USuspenseCoreDoTService::OnActiveGameplayEffectRemoved(const FActiveGameplayEffect& Effect)
{
	if (!IsDoTEffect(Effect))
	{
		return;
	}

	// Find owning ASC and target actor
	AActor* TargetActor = nullptr;
	for (const FSuspenseCoreASCBinding& Binding : BoundASCs)
	{
		if (Binding.ASC.IsValid())
		{
			TargetActor = Binding.ASC->GetOwner();
			break;
		}
	}

	FGameplayTag DoTType = GetDoTTypeFromEffect(Effect);

	// Publish removal event
	FSuspenseCoreDoTEventPayload Payload;
	Payload.AffectedActor = TargetActor;
	Payload.DoTType = DoTType;

	// Check if expired naturally
	float WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	bool bExpired = Effect.GetTimeRemaining(WorldTime) <= 0.0f;

	FGameplayTag EventTag = bExpired
		? SuspenseCoreTags::Event::DoT::Expired
		: SuspenseCoreTags::Event::DoT::Removed;

	PublishDoTEvent(EventTag, Payload);

	UE_LOG(LogDoTService, Log, TEXT("DoT removed (ASC): %s (expired=%d)"),
		*DoTType.ToString(), bExpired);
}

void USuspenseCoreDoTService::OnGameplayEffectStackChanged(
	FActiveGameplayEffectHandle Handle,
	int32 NewStackCount,
	int32 OldStackCount)
{
	// Could publish stack change event if needed
	UE_LOG(LogDoTService, Verbose, TEXT("DoT stack changed: %d -> %d"), OldStackCount, NewStackCount);
}

//==================================================================
// Internal Helpers
//==================================================================

bool USuspenseCoreDoTService::IsDoTEffect(const FGameplayEffectSpec& Spec) const
{
	if (!Spec.Def)
	{
		return false;
	}

	// Check if effect grants DoT-related tags
	const FGameplayTagContainer& GrantedTags = Spec.Def->GetAssetTags();

	return GrantedTags.HasTag(DoTRootTag) ||
		   GrantedTags.HasTag(BleedingRootTag) ||
		   GrantedTags.HasTag(BurningRootTag) ||
		   GrantedTags.HasTag(SuspenseCoreTags::State::Health::BleedingLight) ||
		   GrantedTags.HasTag(SuspenseCoreTags::State::Health::BleedingHeavy) ||
		   GrantedTags.HasTag(SuspenseCoreTags::Effect::DoT::Burn);
}

bool USuspenseCoreDoTService::IsDoTEffect(const FActiveGameplayEffect& Effect) const
{
	return IsDoTEffect(Effect.Spec);
}

FGameplayTag USuspenseCoreDoTService::GetDoTTypeFromEffect(const FGameplayEffectSpec& Spec) const
{
	if (!Spec.Def)
	{
		return DoTRootTag;
	}

	const FGameplayTagContainer& GrantedTags = Spec.Def->GetAssetTags();

	// Check specific tags first
	if (GrantedTags.HasTag(SuspenseCoreTags::State::Health::BleedingHeavy))
	{
		return SuspenseCoreTags::State::Health::BleedingHeavy;
	}
	if (GrantedTags.HasTag(SuspenseCoreTags::State::Health::BleedingLight))
	{
		return SuspenseCoreTags::State::Health::BleedingLight;
	}
	if (GrantedTags.HasTag(SuspenseCoreTags::Effect::DoT::Burn))
	{
		return SuspenseCoreTags::Effect::DoT::Burn;
	}
	if (GrantedTags.HasTag(BleedingRootTag))
	{
		return BleedingRootTag;
	}
	if (GrantedTags.HasTag(BurningRootTag))
	{
		return BurningRootTag;
	}

	// Fallback to root DoT tag
	return DoTRootTag;
}

FGameplayTag USuspenseCoreDoTService::GetDoTTypeFromEffect(const FActiveGameplayEffect& Effect) const
{
	return GetDoTTypeFromEffect(Effect.Spec);
}

FSuspenseCoreActiveDoT USuspenseCoreDoTService::BuildDoTDataFromEffect(
	const FActiveGameplayEffect& Effect,
	AActor* TargetActor) const
{
	FSuspenseCoreActiveDoT Data;

	Data.DoTType = GetDoTTypeFromEffect(Effect);
	Data.EffectHandle = Effect.Handle;
	Data.StackCount = Effect.Spec.GetStackCount();

	// Get duration
	float WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	float Duration = Effect.GetDuration();
	float Remaining = Effect.GetTimeRemaining(WorldTime);

	Data.RemainingDuration = (Duration == FGameplayEffectConstants::INFINITE_DURATION) ? -1.0f : Remaining;
	Data.ApplicationTime = Effect.StartWorldTime;

	// Get damage per tick from SetByCaller
	const FGameplayEffectSpec& Spec = Effect.Spec;
	if (Data.IsBleeding())
	{
		Data.DamagePerTick = Spec.GetSetByCallerMagnitude(SuspenseCoreTags::Data::DoT::Bleed, false, 0.0f);
		Data.TickInterval = 1.0f;  // Default bleeding tick
	}
	else if (Data.IsBurning())
	{
		float ArmorDmg = Spec.GetSetByCallerMagnitude(SuspenseCoreTags::Data::DoT::BurnArmor, false, 0.0f);
		float HealthDmg = Spec.GetSetByCallerMagnitude(SuspenseCoreTags::Data::DoT::BurnHealth, false, 0.0f);
		Data.DamagePerTick = FMath::Abs(ArmorDmg) + FMath::Abs(HealthDmg);
		Data.TickInterval = 0.5f;  // Default burn tick
	}

	// Get source actor
	const FGameplayEffectContextHandle& Context = Spec.GetContext();
	Data.SourceActor = Context.GetInstigator();

	return Data;
}

void USuspenseCoreDoTService::PublishDoTEvent(FGameplayTag EventTag, const FSuspenseCoreDoTEventPayload& Payload)
{
	if (!EventBus.IsValid())
	{
		InitializeEventBus();
	}

	if (EventBus.IsValid())
	{
		// Use async publish for performance (non-blocking)
		EventBus->PublishAsync(EventTag, Payload.ToEventData());
	}
}

UAbilitySystemComponent* USuspenseCoreDoTService::GetASCFromActor(AActor* Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}

	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
}

void USuspenseCoreDoTService::InitializeEventBus()
{
	if (EventBus.IsValid())
	{
		return;
	}

	if (USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this))
	{
		EventBus = Manager->GetEventBus();
	}
}

//==================================================================
// Legacy Support
//==================================================================

void USuspenseCoreDoTService::OnDurationUpdateTimer()
{
	FScopeLock Lock(&ServiceLock);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float DeltaTime = 0.1f;  // Timer interval

	// Update durations in legacy cache
	for (auto& Pair : ManualDoTCache)
	{
		TArray<FSuspenseCoreActiveDoT>& DoTs = Pair.Value;

		for (int32 i = DoTs.Num() - 1; i >= 0; --i)
		{
			FSuspenseCoreActiveDoT& DoT = DoTs[i];

			// Skip infinite duration effects
			if (DoT.IsInfinite())
			{
				continue;
			}

			// Reduce remaining duration
			DoT.RemainingDuration -= DeltaTime;

			// Check if expired
			if (DoT.RemainingDuration <= 0.0f)
			{
				// Publish expiry event
				FSuspenseCoreDoTEventPayload Payload;
				Payload.AffectedActor = Pair.Key.Get();
				Payload.DoTType = DoT.DoTType;
				Payload.DoTData = DoT;

				PublishDoTEvent(SuspenseCoreTags::Event::DoT::Expired, Payload);

				DoTs.RemoveAt(i);
			}
		}
	}

	// Cleanup stale entries
	CleanupStaleEntries();
}

void USuspenseCoreDoTService::CleanupStaleEntries()
{
	// Remove entries for destroyed actors
	for (auto It = ManualDoTCache.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
