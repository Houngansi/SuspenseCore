// SuspenseCoreAbilitySystemComponent.cpp
// SuspenseCore - Clean Architecture GAS Integration
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreAbilitySystemComponent.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"

USuspenseCoreAbilitySystemComponent::USuspenseCoreAbilitySystemComponent()
{
	SetIsReplicatedByDefault(true);
	ReplicationMode = EGameplayEffectReplicationMode::Mixed;
}

// ═══════════════════════════════════════════════════════════════════════════════
// UAbilitySystemComponent Interface
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);
}

void USuspenseCoreAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();

	SetupEventBusSubscriptions();

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
}

void USuspenseCoreAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TeardownEventBusSubscriptions();
	Super::EndPlay(EndPlayReason);
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
	if (!bPublishAttributeEvents)
	{
		return;
	}

	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	// Get MaxValue for attributes that have corresponding Max attributes
	float MaxValue = NewValue;
	const FString AttributeName = Attribute.GetName();

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
		else if (AttributeName == TEXT("MaxHealth") || AttributeName == TEXT("MaxStamina"))
		{
			MaxValue = NewValue;
		}
	}

	// Create event data with UI-compatible keys
	FSuspenseCoreEventData Data = FSuspenseCoreEventData::Create(GetOwner());

	Data.SetFloat(TEXT("Value"), NewValue);
	Data.SetFloat(TEXT("MaxValue"), MaxValue);
	Data.SetString(TEXT("AttributeName"), AttributeName);
	Data.SetFloat(TEXT("OldValue"), OldValue);
	Data.SetFloat(TEXT("NewValue"), NewValue);
	Data.SetFloat(TEXT("Delta"), NewValue - OldValue);
	Data.SetObject(TEXT("AbilitySystemComponent"), this);

	// Form event tag: SuspenseCore.Event.GAS.Attribute.<AttributeName>
	FString TagString = FString::Printf(TEXT("SuspenseCore.Event.GAS.Attribute.%s"), *AttributeName);
	FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(FName(*TagString), false);

	if (!EventTag.IsValid())
	{
		EventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.GAS.Attribute.Changed")), false);
	}

	if (EventTag.IsValid())
	{
		EventBus->Publish(EventTag, Data);
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
}

USuspenseCoreEventBus* USuspenseCoreAbilitySystemComponent::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

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
	// Base class does nothing - subclasses can override
}

void USuspenseCoreAbilitySystemComponent::TeardownEventBusSubscriptions()
{
	CachedEventBus.Reset();
}
