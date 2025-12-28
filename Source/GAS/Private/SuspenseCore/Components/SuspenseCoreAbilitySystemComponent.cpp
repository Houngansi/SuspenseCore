// SuspenseCoreAbilitySystemComponent.cpp
// SuspenseCore - Clean Architecture GAS Integration
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreAbilitySystemComponent.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeDefaults.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreASC, Log, All);

USuspenseCoreAbilitySystemComponent::USuspenseCoreAbilitySystemComponent()
{
	// Default settings for networked gameplay
	SetIsReplicatedByDefault(true);
	ReplicationMode = EGameplayEffectReplicationMode::Mixed;

	// Initialize stamina regen block tags
	StaminaRegenBlockTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Sprinting")));
	StaminaRegenBlockTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Dead")));
}

// ═══════════════════════════════════════════════════════════════════════════════
// UAbilitySystemComponent Interface
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	if (InOwnerActor)
	{
		UE_LOG(LogSuspenseCoreASC, Log, TEXT("InitAbilityActorInfo: Owner=%s, Avatar=%s"),
			*GetNameSafe(InOwnerActor), *GetNameSafe(InAvatarActor));
	}
}

void USuspenseCoreAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();

	SetupEventBusSubscriptions();

	// Start stamina regeneration timer
	if (bStaminaRegenEnabled)
	{
		StartStaminaRegenTimer();
	}

	// Publish initialization event
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData Data = FSuspenseCoreEventData::Create(GetOwner());
		Data.SetObject(TEXT("AbilitySystemComponent"), this);

		EventBus->Publish(
			FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.GAS.Initialized"))),
			Data
		);
	}

	UE_LOG(LogSuspenseCoreASC, Log, TEXT("SuspenseCoreASC BeginPlay on %s"), *GetNameSafe(GetOwner()));
}

void USuspenseCoreAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Stop stamina regen timer
	StopStaminaRegenTimer();

	TeardownEventBusSubscriptions();

	Super::EndPlay(EndPlayReason);

	UE_LOG(LogSuspenseCoreASC, Log, TEXT("SuspenseCoreASC EndPlay on %s"), *GetNameSafe(GetOwner()));
}

// ═══════════════════════════════════════════════════════════════════════════════
// EVENTBUS INTEGRATION
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreAbilitySystemComponent::SetAttributeEventsEnabled(bool bEnabled)
{
	bPublishAttributeEvents = bEnabled;
}

void USuspenseCoreAbilitySystemComponent::PublishAttributeChangeEvent(
	const FGameplayAttribute& Attribute,
	float OldValue,
	float NewValue)
{
	UE_LOG(LogSuspenseCoreASC, Warning, TEXT("[ASC] PublishAttributeChangeEvent - %s: %.2f -> %.2f"),
		*Attribute.GetName(), OldValue, NewValue);

	if (!bPublishAttributeEvents)
	{
		UE_LOG(LogSuspenseCoreASC, Warning, TEXT("[ASC] bPublishAttributeEvents is FALSE - events disabled!"));
		return;
	}

	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		UE_LOG(LogSuspenseCoreASC, Warning, TEXT("[ASC] EventBus is NULL!"));
		return;
	}
	UE_LOG(LogSuspenseCoreASC, Warning, TEXT("[ASC] EventBus found, publishing..."));

	// Get MaxValue for attributes that have corresponding Max attributes
	float MaxValue = NewValue; // Default: no max, use current value
	const FString AttributeName = Attribute.GetName();

	// Query AttributeSet for Max values
	if (const USuspenseCoreAttributeSet* AttributeSet = GetSet<USuspenseCoreAttributeSet>())
	{
		if (AttributeName == TEXT("Health"))
		{
			MaxValue = AttributeSet->GetMaxHealth();
		}
		else if (AttributeName == TEXT("Stamina"))
		{
			MaxValue = AttributeSet->GetMaxStamina();
		}
		// For MaxHealth, MaxStamina themselves - they ARE the max
		else if (AttributeName == TEXT("MaxHealth") || AttributeName == TEXT("MaxStamina"))
		{
			MaxValue = NewValue;
		}
	}

	// Create event data with UI-compatible keys
	FSuspenseCoreEventData Data = FSuspenseCoreEventData::Create(GetOwner());

	// Primary keys for UI widgets (what widgets expect)
	Data.SetFloat(TEXT("Value"), NewValue);
	Data.SetFloat(TEXT("MaxValue"), MaxValue);

	// Legacy keys for backwards compatibility
	Data.SetString(TEXT("AttributeName"), AttributeName);
	Data.SetFloat(TEXT("OldValue"), OldValue);
	Data.SetFloat(TEXT("NewValue"), NewValue);
	Data.SetFloat(TEXT("Delta"), NewValue - OldValue);
	Data.SetObject(TEXT("AbilitySystemComponent"), this);

	// Form event tag: SuspenseCore.Event.GAS.Attribute.<AttributeName>
	FString TagString = FString::Printf(TEXT("SuspenseCore.Event.GAS.Attribute.%s"), *AttributeName);
	FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(FName(*TagString), false);

	UE_LOG(LogSuspenseCoreASC, Warning, TEXT("[ASC] Requesting tag: %s, Valid: %s"),
		*TagString, EventTag.IsValid() ? TEXT("YES") : TEXT("NO"));

	if (!EventTag.IsValid())
	{
		// Use generic tag if specific one is not registered
		UE_LOG(LogSuspenseCoreASC, Warning, TEXT("[ASC] Tag not found, trying generic..."));
		EventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.GAS.Attribute.Changed")), false);
	}

	if (EventTag.IsValid())
	{
		UE_LOG(LogSuspenseCoreASC, Warning, TEXT("[ASC] PUBLISHING EVENT: %s (Value: %.2f)"),
			*EventTag.ToString(), NewValue);
		EventBus->Publish(EventTag, Data);
	}
	else
	{
		UE_LOG(LogSuspenseCoreASC, Warning, TEXT("[ASC] ERROR: No valid event tag! Cannot publish."));
	}
}

void USuspenseCoreAbilitySystemComponent::PublishCriticalEvent(
	FGameplayTag EventTag,
	float CurrentValue,
	float MaxValue)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus || !EventTag.IsValid())
	{
		return;
	}

	FSuspenseCoreEventData Data = FSuspenseCoreEventData::Create(GetOwner());
	Data.SetFloat(TEXT("CurrentValue"), CurrentValue);
	Data.SetFloat(TEXT("MaxValue"), MaxValue);
	Data.SetFloat(TEXT("Percent"), MaxValue > 0.0f ? CurrentValue / MaxValue : 0.0f);
	Data.SetObject(TEXT("AbilitySystemComponent"), this);
	Data.Priority = ESuspenseCoreEventPriority::High;

	EventBus->Publish(EventTag, Data);

	UE_LOG(LogSuspenseCoreASC, Log, TEXT("Critical event %s: %.2f / %.2f"),
		*EventTag.ToString(), CurrentValue, MaxValue);
}

USuspenseCoreEventBus* USuspenseCoreAbilitySystemComponent::GetEventBus() const
{
	// Check cache
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	// Get through EventManager
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetOwner());
	if (Manager)
	{
		CachedEventBus = Manager->GetEventBus();
		return CachedEventBus.Get();
	}

	return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ABILITY HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

FGameplayAbilitySpecHandle USuspenseCoreAbilitySystemComponent::GiveAbilityOfClass(
	TSubclassOf<UGameplayAbility> AbilityClass,
	int32 Level,
	int32 InputID)
{
	if (!AbilityClass)
	{
		UE_LOG(LogSuspenseCoreASC, Warning, TEXT("GiveAbilityOfClass: AbilityClass is null"));
		return FGameplayAbilitySpecHandle();
	}

	FGameplayAbilitySpec AbilitySpec(AbilityClass, Level, InputID, GetOwner());
	return GiveAbility(AbilitySpec);
}

void USuspenseCoreAbilitySystemComponent::RemoveAbilitiesOfClass(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!AbilityClass)
	{
		return;
	}

	TArray<FGameplayAbilitySpecHandle> AbilitiesToRemove;

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->GetClass() == AbilityClass)
		{
			AbilitiesToRemove.Add(Spec.Handle);
		}
	}

	for (const FGameplayAbilitySpecHandle& Handle : AbilitiesToRemove)
	{
		ClearAbility(Handle);
	}
}

bool USuspenseCoreAbilitySystemComponent::HasAbilityWithTag(FGameplayTag AbilityTag) const
{
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->GetAssetTags().HasTag(AbilityTag))
		{
			return true;
		}
	}
	return false;
}

bool USuspenseCoreAbilitySystemComponent::TryActivateAbilityByTag(FGameplayTag AbilityTag, bool bAllowRemoteActivation)
{
	FGameplayTagContainer TagContainer;
	TagContainer.AddTag(AbilityTag);
	return TryActivateAbilitiesByTag(TagContainer, bAllowRemoteActivation);
}

// ═══════════════════════════════════════════════════════════════════════════════
// EFFECT HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

FActiveGameplayEffectHandle USuspenseCoreAbilitySystemComponent::ApplyEffectToSelf(
	TSubclassOf<UGameplayEffect> EffectClass,
	float Level)
{
	if (!EffectClass)
	{
		UE_LOG(LogSuspenseCoreASC, Warning, TEXT("ApplyEffectToSelf: EffectClass is null"));
		return FActiveGameplayEffectHandle();
	}

	FGameplayEffectContextHandle Context = MakeEffectContext();
	Context.AddSourceObject(GetOwner());

	return ApplyEffectToSelfWithContext(EffectClass, Context, Level);
}

FActiveGameplayEffectHandle USuspenseCoreAbilitySystemComponent::ApplyEffectToSelfWithContext(
	TSubclassOf<UGameplayEffect> EffectClass,
	const FGameplayEffectContextHandle& Context,
	float Level)
{
	if (!EffectClass)
	{
		return FActiveGameplayEffectHandle();
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(EffectClass, Level, Context);
	if (SpecHandle.IsValid())
	{
		return ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}

	return FActiveGameplayEffectHandle();
}

void USuspenseCoreAbilitySystemComponent::RemoveActiveEffectsOfClass(TSubclassOf<UGameplayEffect> EffectClass)
{
	if (!EffectClass)
	{
		return;
	}

	FGameplayEffectQuery Query;
	Query.EffectDefinition = EffectClass;
	RemoveActiveEffects(Query);
}

// ═══════════════════════════════════════════════════════════════════════════════
// PROTECTED
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreAbilitySystemComponent::SetupEventBusSubscriptions()
{
	// Base class does nothing
	// Subclasses can override to subscribe to events
}

void USuspenseCoreAbilitySystemComponent::TeardownEventBusSubscriptions()
{
	// Clear cache
	CachedEventBus.Reset();
}

// ═══════════════════════════════════════════════════════════════════════════════
// STAMINA REGENERATION
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreAbilitySystemComponent::SetStaminaRegenEnabled(bool bEnabled)
{
	if (bStaminaRegenEnabled == bEnabled)
	{
		return;
	}

	bStaminaRegenEnabled = bEnabled;

	if (bEnabled)
	{
		StartStaminaRegenTimer();
	}
	else
	{
		StopStaminaRegenTimer();
	}
}

void USuspenseCoreAbilitySystemComponent::StartStaminaRegenTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			StaminaRegenTimerHandle,
			this,
			&USuspenseCoreAbilitySystemComponent::OnStaminaRegenTick,
			StaminaRegenTickRate,
			true  // Looping
		);

		UE_LOG(LogSuspenseCoreASC, Log, TEXT("Stamina regen timer started (%.2fs period)"), StaminaRegenTickRate);
	}
}

void USuspenseCoreAbilitySystemComponent::StopStaminaRegenTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StaminaRegenTimerHandle);
		UE_LOG(LogSuspenseCoreASC, Log, TEXT("Stamina regen timer stopped"));
	}
}

void USuspenseCoreAbilitySystemComponent::OnStaminaRegenTick()
{
	// Check if we have any blocking tags active
	if (HasAnyMatchingGameplayTags(StaminaRegenBlockTags))
	{
		return;
	}

	// Get attribute set
	const USuspenseCoreAttributeSet* AttributeSet = GetSet<USuspenseCoreAttributeSet>();
	if (!AttributeSet)
	{
		return;
	}

	// Check if stamina is already at max
	const float CurrentStamina = AttributeSet->GetStamina();
	const float MaxStamina = AttributeSet->GetMaxStamina();

	if (CurrentStamina >= MaxStamina)
	{
		return;  // No regen needed
	}

	// Calculate regen amount based on StaminaRegen attribute
	// StaminaRegen is "per second", we tick 10x/sec, so divide by 10
	const float StaminaRegenPerSecond = AttributeSet->GetStaminaRegen();
	const float RegenAmount = StaminaRegenPerSecond * StaminaRegenTickRate;

	// Apply regeneration directly to base value (with clamping)
	const float NewStamina = FMath::Min(CurrentStamina + RegenAmount, MaxStamina);
	SetNumericAttributeBase(USuspenseCoreAttributeSet::GetStaminaAttribute(), NewStamina);
}
