// SuspenseCoreAbility.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Base/SuspenseCoreAbility.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreAbility, Log, All);

USuspenseCoreAbility::USuspenseCoreAbility()
{
	bPublishAbilityEvents = true;

	// Default ability tags
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bRetriggerInstancedAbility = false;
}

//==================================================================
// GameplayAbility Overrides
//==================================================================

void USuspenseCoreAbility::OnGiveAbility(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	LogAbilityDebug(FString::Printf(TEXT("Ability granted to %s"),
		*GetNameSafe(ActorInfo->AvatarActor.Get())));
}

void USuspenseCoreAbility::OnRemoveAbility(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilitySpec& Spec)
{
	LogAbilityDebug(FString::Printf(TEXT("Ability removed from %s"),
		*GetNameSafe(ActorInfo->AvatarActor.Get())));

	Super::OnRemoveAbility(ActorInfo, Spec);
}

void USuspenseCoreAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Broadcast activation event
	BroadcastAbilityActivated();

	LogAbilityDebug(TEXT("Ability activated"));
}

void USuspenseCoreAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Broadcast end event before calling super
	BroadcastAbilityEnded(bWasCancelled);

	LogAbilityDebug(FString::Printf(TEXT("Ability ended (cancelled: %s)"),
		bWasCancelled ? TEXT("true") : TEXT("false")));

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

//==================================================================
// EventBus Helpers
//==================================================================

USuspenseCoreEventBus* USuspenseCoreAbility::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo || !ActorInfo->OwnerActor.IsValid())
	{
		return nullptr;
	}

	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(ActorInfo->OwnerActor.Get());
	if (!Manager)
	{
		return nullptr;
	}

	USuspenseCoreEventBus* EventBus = Manager->GetEventBus();
	if (EventBus)
	{
		CachedEventBus = EventBus;
	}

	return EventBus;
}

void USuspenseCoreAbility::PublishEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	if (!bPublishAbilityEvents)
	{
		return;
	}

	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (EventBus && EventTag.IsValid())
	{
		EventBus->Publish(EventTag, EventData);
	}
}

void USuspenseCoreAbility::PublishSimpleEvent(FGameplayTag EventTag)
{
	if (!bPublishAbilityEvents)
	{
		return;
	}

	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (EventBus && EventTag.IsValid())
	{
		const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
		UObject* Source = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
		EventBus->PublishSimple(EventTag, Source);
	}
}

void USuspenseCoreAbility::BroadcastAbilityActivated()
{
	if (!bPublishAbilityEvents)
	{
		return;
	}

	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr,
		ESuspenseCoreEventPriority::Normal
	);

	EventData.SetString(TEXT("AbilityClass"), GetClass()->GetName());

	FGameplayTag EventTag = GetAbilitySpecificTag(TEXT("Activated"));

	// Only publish if the tag is valid (exists in config)
	if (EventTag.IsValid())
	{
		EventBus->Publish(EventTag, EventData);
	}
}

void USuspenseCoreAbility::BroadcastAbilityEnded(bool bWasCancelled)
{
	if (!bPublishAbilityEvents)
	{
		return;
	}

	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr,
		ESuspenseCoreEventPriority::Normal
	);

	EventData.SetString(TEXT("AbilityClass"), GetClass()->GetName());
	EventData.SetBool(TEXT("WasCancelled"), bWasCancelled);

	FGameplayTag EventTag = bWasCancelled
		? GetAbilitySpecificTag(TEXT("Cancelled"))
		: GetAbilitySpecificTag(TEXT("Ended"));

	// Only publish if the tag is valid (exists in config)
	if (EventTag.IsValid())
	{
		EventBus->Publish(EventTag, EventData);
	}
}

//==================================================================
// Utility Helpers
//==================================================================

ACharacter* USuspenseCoreAbility::GetOwningCharacter() const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		return nullptr;
	}

	return Cast<ACharacter>(ActorInfo->AvatarActor.Get());
}

FGameplayTag USuspenseCoreAbility::GetAbilitySpecificTag(const FString& Suffix) const
{
	// Use custom tag if specified
	if (AbilityEventTag.IsValid())
	{
		FString TagString = AbilityEventTag.ToString() + TEXT(".") + Suffix;
		return FGameplayTag::RequestGameplayTag(FName(*TagString), false);
	}

	// Generate tag from class name
	FString ClassName = GetClass()->GetName();
	// Remove "USuspenseCore" prefix if present
	ClassName.RemoveFromStart(TEXT("USuspenseCore"));
	// Remove "Ability" suffix if present
	ClassName.RemoveFromEnd(TEXT("Ability"));

	FString TagString = FString::Printf(TEXT("SuspenseCore.Event.Ability.%s.%s"), *ClassName, *Suffix);
	return FGameplayTag::RequestGameplayTag(FName(*TagString), false);
}

void USuspenseCoreAbility::LogAbilityDebug(const FString& Message, bool bError) const
{
	const FString ClassName = GetClass()->GetName();

	if (bError)
	{
		UE_LOG(LogSuspenseCoreAbility, Warning, TEXT("[%s] %s"), *ClassName, *Message);
	}
	else
	{
		UE_LOG(LogSuspenseCoreAbility, Log, TEXT("[%s] %s"), *ClassName, *Message);
	}
}
