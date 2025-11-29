// SuspenseCorePlayerComponent.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Core/SuspenseCorePlayerComponent.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"

// ═══════════════════════════════════════════════════════════════════════════════
// CONSTRUCTOR
// ═══════════════════════════════════════════════════════════════════════════════

USuspenseCorePlayerComponent::USuspenseCorePlayerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// COMPONENT LIFECYCLE
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCorePlayerComponent::BeginPlay()
{
	Super::BeginPlay();

	Initialize();
}

void USuspenseCorePlayerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanupSubscriptions();
	CachedEventBus.Reset();

	Super::EndPlay(EndPlayReason);
}

void USuspenseCorePlayerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API - EVENTS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCorePlayerComponent::PublishEvent(const FGameplayTag& EventTag, const FString& Payload)
{
	if (!EventTag.IsValid())
	{
		return;
	}

	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		EventBus->Publish(GetOwner(), EventTag, Payload);
	}
}

bool USuspenseCorePlayerComponent::SubscribeToEvent(const FGameplayTag& EventTag)
{
	if (!EventTag.IsValid())
	{
		return false;
	}

	// Check if already subscribed
	for (const FSuspenseCoreEventSubscription& Sub : ActiveSubscriptions)
	{
		if (Sub.EventTag == EventTag)
		{
			return true; // Already subscribed
		}
	}

	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return false;
	}

	// Subscribe and store handle
	FDelegateHandle Handle = EventBus->Subscribe(
		EventTag,
		FOnSuspenseCoreEvent::FDelegate::CreateUObject(this, &USuspenseCorePlayerComponent::HandleEventReceived)
	);

	if (Handle.IsValid())
	{
		FSuspenseCoreEventSubscription NewSub;
		NewSub.EventTag = EventTag;
		NewSub.Handle = Handle;
		ActiveSubscriptions.Add(NewSub);
		return true;
	}

	return false;
}

void USuspenseCorePlayerComponent::UnsubscribeFromEvent(const FGameplayTag& EventTag)
{
	if (!EventTag.IsValid())
	{
		return;
	}

	USuspenseCoreEventBus* EventBus = GetEventBus();

	for (int32 i = ActiveSubscriptions.Num() - 1; i >= 0; --i)
	{
		if (ActiveSubscriptions[i].EventTag == EventTag)
		{
			if (EventBus)
			{
				EventBus->Unsubscribe(EventTag, ActiveSubscriptions[i].Handle);
			}
			ActiveSubscriptions.RemoveAt(i);
			break;
		}
	}
}

void USuspenseCorePlayerComponent::UnsubscribeFromAllEvents()
{
	CleanupSubscriptions();
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API - STATE
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCorePlayerComponent::SetPlayerIdentifier(const FString& NewIdentifier)
{
	if (PlayerIdentifier != NewIdentifier)
	{
		const FString OldIdentifier = PlayerIdentifier;
		PlayerIdentifier = NewIdentifier;

		PublishEvent(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Player.IdentifierChanged")),
			FString::Printf(TEXT("{\"old\":\"%s\",\"new\":\"%s\"}"), *OldIdentifier, *PlayerIdentifier)
		);
	}
}

UAbilitySystemComponent* USuspenseCorePlayerComponent::GetOwnerASC() const
{
	if (AActor* Owner = GetOwner())
	{
		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Owner))
		{
			return ASI->GetAbilitySystemComponent();
		}
	}
	return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════════
// OVERRIDABLE HANDLERS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCorePlayerComponent::OnEventReceived(const FGameplayTag& EventTag, const FString& Payload, UObject* Source)
{
	// Base implementation calls Blueprint event
	K2_OnEventReceived(EventTag, Payload, Source);
}

void USuspenseCorePlayerComponent::OnReady()
{
	// Base implementation calls Blueprint event
	K2_OnReady();
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCorePlayerComponent::Initialize()
{
	// Generate identifier if needed
	if (bAutoGenerateIdentifier && PlayerIdentifier.IsEmpty())
	{
		if (AActor* Owner = GetOwner())
		{
			PlayerIdentifier = FString::Printf(TEXT("%s_%s"),
				*Owner->GetName(),
				*FGuid::NewGuid().ToString(EGuidFormats::Short));
		}
	}

	// Setup auto subscriptions
	SetupAutoSubscriptions();

	// Mark as ready
	bIsReady = true;

	// Notify
	OnReady();

	// Publish ready event
	PublishEvent(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.PlayerComponent.Ready")),
		FString::Printf(TEXT("{\"identifier\":\"%s\"}"), *PlayerIdentifier)
	);
}

void USuspenseCorePlayerComponent::SetupAutoSubscriptions()
{
	for (const FGameplayTag& EventTag : AutoSubscribeEvents)
	{
		SubscribeToEvent(EventTag);
	}
}

void USuspenseCorePlayerComponent::CleanupSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();

	for (const FSuspenseCoreEventSubscription& Sub : ActiveSubscriptions)
	{
		if (EventBus && Sub.Handle.IsValid())
		{
			EventBus->Unsubscribe(Sub.EventTag, Sub.Handle);
		}
	}

	ActiveSubscriptions.Empty();
}

USuspenseCoreEventBus* USuspenseCorePlayerComponent::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	if (AActor* Owner = GetOwner())
	{
		USuspenseCoreEventBus* EventBus = USuspenseCoreServiceLocator::GetEventBus(Owner->GetWorld());
		if (EventBus)
		{
			const_cast<USuspenseCorePlayerComponent*>(this)->CachedEventBus = EventBus;
		}
		return EventBus;
	}

	return nullptr;
}

void USuspenseCorePlayerComponent::HandleEventReceived(const UObject* Source, const FGameplayTag& EventTag, const FString& Payload)
{
	// Forward to overridable handler
	OnEventReceived(EventTag, Payload, const_cast<UObject*>(Source));
}
