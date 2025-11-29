// SuspenseCoreAbilitySystemComponent.cpp
// SuspenseCore - Clean Architecture GAS Integration
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreAbilitySystemComponent.h"
#include "SuspenseCore/SuspenseCoreEventBus.h"
#include "SuspenseCore/SuspenseCoreEventManager.h"
#include "SuspenseCore/SuspenseCoreTypes.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreASC, Log, All);

USuspenseCoreAbilitySystemComponent::USuspenseCoreAbilitySystemComponent()
{
	// Настройки по умолчанию для сетевой игры
	SetIsReplicatedByDefault(true);
	ReplicationMode = EGameplayEffectReplicationMode::Mixed;
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

	// Публикуем событие инициализации
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
	if (!bPublishAttributeEvents)
	{
		return;
	}

	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	// Создаём данные события
	FSuspenseCoreEventData Data = FSuspenseCoreEventData::Create(GetOwner());
	Data.SetString(TEXT("AttributeName"), Attribute.GetName());
	Data.SetFloat(TEXT("OldValue"), OldValue);
	Data.SetFloat(TEXT("NewValue"), NewValue);
	Data.SetFloat(TEXT("Delta"), NewValue - OldValue);
	Data.SetObject(TEXT("AbilitySystemComponent"), this);

	// Формируем тег события: SuspenseCore.Event.GAS.Attribute.<AttributeName>
	FString TagString = FString::Printf(TEXT("SuspenseCore.Event.GAS.Attribute.%s"), *Attribute.GetName());
	FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(FName(*TagString), false);

	if (!EventTag.IsValid())
	{
		// Используем generic тег если специфичный не зарегистрирован
		EventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.GAS.Attribute.Changed")), false);
	}

	if (EventTag.IsValid())
	{
		EventBus->Publish(EventTag, Data);
	}

	UE_LOG(LogSuspenseCoreASC, Verbose, TEXT("Attribute %s changed: %.2f -> %.2f"),
		*Attribute.GetName(), OldValue, NewValue);
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
	// Проверяем кэш
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	// Получаем через EventManager
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
	// В базовом классе ничего не делаем
	// Наследники могут переопределить для подписки на события
}

void USuspenseCoreAbilitySystemComponent::TeardownEventBusSubscriptions()
{
	// Очищаем кэш
	CachedEventBus.Reset();
}
