// SuspenseCoreInteractionComponent.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreInteractionComponent.h"
#include "SuspenseCore/Utils/SuspenseCoreInteractionSettings.h"
#include "SuspenseCore/Utils/SuspenseCoreHelpers.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Interfaces/Interaction/ISuspenseCoreInteractable.h"
#include "Interfaces/Interaction/ISuspenseInteract.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreInteractionComp, Log, All);

USuspenseCoreInteractionComponent::USuspenseCoreInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f; // Default 10 Hz, can be modified by settings
	SetIsReplicatedByDefault(true);

	// Load settings from configuration
	const USuspenseCoreInteractionSettings* Settings = USuspenseCoreInteractionSettings::Get();
	if (Settings)
	{
		TraceDistance = Settings->DefaultTraceDistance;
		TraceSphereRadius = Settings->TraceSphereRadius;
		TraceChannel = Settings->DefaultTraceChannel;
		bEnableDebugTrace = Settings->bEnableDebugDraw;
		InteractionCooldown = Settings->DefaultInteractionCooldown;
	}
	else
	{
		TraceDistance = 300.0f;
		TraceSphereRadius = 0.0f;
		TraceChannel = ECC_Visibility;
		bEnableDebugTrace = false;
		InteractionCooldown = 0.5f;
	}

	// Initialize tags
	InteractAbilityTag = FGameplayTag::RequestGameplayTag(FName("Ability.Input.Interact"));
	InteractSuccessTag = FGameplayTag::RequestGameplayTag(FName("Ability.Interact.Success"));
	InteractFailedTag = FGameplayTag::RequestGameplayTag(FName("Ability.Interact.Failed"));

	// Initialize blocking tags
	BlockingTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
	BlockingTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Stunned")));
	BlockingTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Disabled")));

	bInteractionOnCooldown = false;
	FocusUpdateAccumulator = 0.0f;
}

//==================================================================
// UActorComponent Interface
//==================================================================

void USuspenseCoreInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// Cache AbilitySystemComponent
	CachedASC = GetOwnerASC();

	// Setup EventBus subscriptions
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		SetupEventSubscriptions(EventBus);
	}

	// Subscribe to GAS events if ASC exists
	if (CachedASC.IsValid())
	{
		CachedASC->GenericGameplayEventCallbacks.FindOrAdd(InteractSuccessTag).AddUObject(
			this, &USuspenseCoreInteractionComponent::HandleInteractionSuccessDelegate);

		CachedASC->GenericGameplayEventCallbacks.FindOrAdd(InteractFailedTag).AddUObject(
			this, &USuspenseCoreInteractionComponent::HandleInteractionFailureDelegate);

		LogInteraction(TEXT("Subscribed to AbilitySystemComponent events"));
	}
	else
	{
		LogInteraction(TEXT("AbilitySystemComponent not found, will retry on interaction"), true);
	}

	// Apply settings
	ApplySettings(USuspenseCoreInteractionSettings::Get());
}

void USuspenseCoreInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Teardown EventBus subscriptions
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		TeardownEventSubscriptions(EventBus);
	}

	// Unsubscribe from GAS events
	if (CachedASC.IsValid())
	{
		CachedASC->GenericGameplayEventCallbacks.FindOrAdd(InteractSuccessTag).RemoveAll(this);
		CachedASC->GenericGameplayEventCallbacks.FindOrAdd(InteractFailedTag).RemoveAll(this);
	}

	// Clear cooldown timer
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(CooldownTimerHandle);
	}

	// Clear focus
	if (LastInteractableActor.IsValid())
	{
		UpdateInteractionFocus(nullptr);
	}

	Super::EndPlay(EndPlayReason);
}

void USuspenseCoreInteractionComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Rate limit focus updates
	const USuspenseCoreInteractionSettings* Settings = USuspenseCoreInteractionSettings::Get();
	float UpdateInterval = Settings ? (1.0f / Settings->FocusUpdateRate) : 0.1f;

	FocusUpdateAccumulator += DeltaTime;
	if (FocusUpdateAccumulator < UpdateInterval)
	{
		return;
	}
	FocusUpdateAccumulator = 0.0f;

	// Update UI focus (only on clients or listen server local player)
	if (IsValid(GetOwner()) && !GetOwner()->HasAuthority())
	{
		AActor* InteractableActor = PerformUIInteractionTrace();

		// Update focus if changed
		if (InteractableActor != LastInteractableActor.Get())
		{
			UpdateInteractionFocus(InteractableActor);
		}
	}
}

//==================================================================
// ISuspenseCoreEventSubscriber Interface
//==================================================================

void USuspenseCoreInteractionComponent::SetupEventSubscriptions(USuspenseCoreEventBus* EventBus)
{
	if (!EventBus)
	{
		return;
	}

	// Subscribe to settings change events using native callback
	static const FGameplayTag SettingsChangedTag =
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Settings.InteractionChanged"));

	FSuspenseCoreSubscriptionHandle Handle = EventBus->SubscribeNative(
		SettingsChangedTag,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(
			this, &USuspenseCoreInteractionComponent::HandleSettingsChanged),
		ESuspenseCoreEventPriority::Normal
	);

	if (Handle.IsValid())
	{
		SubscriptionHandles.Add(Handle);
		LogInteraction(TEXT("Subscribed to settings change events"));
	}
}

void USuspenseCoreInteractionComponent::TeardownEventSubscriptions(USuspenseCoreEventBus* EventBus)
{
	if (!EventBus)
	{
		return;
	}

	// Unsubscribe all handles
	for (const FSuspenseCoreSubscriptionHandle& Handle : SubscriptionHandles)
	{
		EventBus->Unsubscribe(Handle);
	}

	SubscriptionHandles.Empty();
	LogInteraction(TEXT("Unsubscribed from all EventBus events"));
}

TArray<FSuspenseCoreSubscriptionHandle> USuspenseCoreInteractionComponent::GetSubscriptionHandles() const
{
	return SubscriptionHandles;
}

//==================================================================
// ISuspenseCoreEventEmitter Interface
//==================================================================

void USuspenseCoreInteractionComponent::EmitEvent(
	FGameplayTag EventTag,
	const FSuspenseCoreEventData& Data)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (EventBus && EventTag.IsValid())
	{
		EventBus->Publish(EventTag, Data);
	}
}

USuspenseCoreEventBus* USuspenseCoreInteractionComponent::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	USuspenseCoreEventBus* EventBus = USuspenseCoreHelpers::GetEventBus(this);
	if (EventBus)
	{
		CachedEventBus = EventBus;
	}

	return EventBus;
}

//==================================================================
// Main Interaction API
//==================================================================

void USuspenseCoreInteractionComponent::StartInteraction()
{
	// Check cooldown
	if (bInteractionOnCooldown)
	{
		LogInteraction(TEXT("Interaction on cooldown"), true);
		return;
	}

	// Check if we can interact
	if (!CanInteractNow())
	{
		LogInteraction(TEXT("Interaction blocked"), true);
		OnInteractionFailed.Broadcast(nullptr);
		BroadcastInteractionResult(nullptr, false);
		return;
	}

	// Set cooldown
	SetInteractionCooldown();

	// Get current target
	AActor* TargetActor = PerformUIInteractionTrace();

	// Broadcast interaction attempt through EventBus
	BroadcastInteractionAttempt(TargetActor);

	// Activate interaction ability through GAS
	if (CachedASC.IsValid())
	{
		CachedASC->TryActivateAbilitiesByTag(FGameplayTagContainer(InteractAbilityTag));
		LogInteraction(TEXT("Started interaction ability"));
		return;
	}

	// Try to get ASC again if cached became invalid
	CachedASC = GetOwnerASC();
	if (CachedASC.IsValid())
	{
		CachedASC->TryActivateAbilitiesByTag(FGameplayTagContainer(InteractAbilityTag));
		LogInteraction(TEXT("Started interaction ability (after cache update)"));
		return;
	}

	// No ASC available
	LogInteraction(TEXT("Failed to activate interaction ability - no AbilitySystemComponent"), true);
	OnInteractionFailed.Broadcast(nullptr);
	BroadcastInteractionResult(nullptr, false);
}

bool USuspenseCoreInteractionComponent::CanInteractNow() const
{
	// Check blocking tags
	if (HasBlockingTags())
	{
		LogInteraction(TEXT("Interaction blocked by state tags"), true);
		return false;
	}

	// Check ASC availability
	if (!CachedASC.IsValid())
	{
		UAbilitySystemComponent* CurrentASC = GetOwnerASC();
		if (!CurrentASC)
		{
			LogInteraction(TEXT("No AbilitySystemComponent"), true);
			return false;
		}
		const_cast<USuspenseCoreInteractionComponent*>(this)->CachedASC = CurrentASC;
	}

	// Check for interactable object
	AActor* InteractableActor = PerformUIInteractionTrace();
	if (!InteractableActor)
	{
		LogInteraction(TEXT("No interactable object within reach"), true);
		return false;
	}

	// Check interface implementation
	if (!InteractableActor->GetClass()->ImplementsInterface(USuspenseInteract::StaticClass()))
	{
		LogInteraction(TEXT("Object doesn't support interaction interface"), true);
		return false;
	}

	// Check interaction possibility through interface
	APlayerController* PC = GetOwner()->GetInstigatorController<APlayerController>();
	if (!PC)
	{
		LogInteraction(TEXT("No PlayerController for interaction"), true);
		return false;
	}

	if (!ISuspenseInteract::Execute_CanInteract(InteractableActor, PC))
	{
		LogInteraction(TEXT("Object doesn't allow interaction at this moment"), true);
		return false;
	}

	return true;
}

AActor* USuspenseCoreInteractionComponent::PerformUIInteractionTrace() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	// Get view point
	FVector CameraLocation;
	FRotator CameraRotation;

	if (APlayerController* PC = Cast<APlayerController>(OwnerActor->GetInstigatorController()))
	{
		PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
	}
	else if (UCameraComponent* FoundCamera = OwnerActor->FindComponentByClass<UCameraComponent>())
	{
		CameraLocation = FoundCamera->GetComponentLocation();
		CameraRotation = FoundCamera->GetComponentRotation();
	}
	else if (ACharacter* Character = Cast<ACharacter>(OwnerActor))
	{
		CameraLocation = Character->GetActorLocation() + FVector(0, 0, Character->BaseEyeHeight);
		CameraRotation = Character->GetControlRotation();
	}
	else
	{
		CameraLocation = OwnerActor->GetActorLocation() + FVector(0, 0, 50);
		CameraRotation = OwnerActor->GetActorRotation();
	}

	// Calculate trace
	FVector TraceStart = CameraLocation;
	FVector TraceEnd = TraceStart + CameraRotation.Vector() * TraceDistance;

	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(OwnerActor);

	bool bHit = false;

	// Use sphere trace if radius > 0
	if (TraceSphereRadius > 0.0f)
	{
		bHit = GetWorld()->SweepSingleByChannel(
			HitResult,
			TraceStart,
			TraceEnd,
			FQuat::Identity,
			TraceChannel,
			FCollisionShape::MakeSphere(TraceSphereRadius),
			Params
		);
	}
	else
	{
		bHit = GetWorld()->LineTraceSingleByChannel(
			HitResult,
			TraceStart,
			TraceEnd,
			TraceChannel,
			Params
		);
	}

	// Debug visualization
	if (bEnableDebugTrace)
	{
		DrawDebugLine(GetWorld(), TraceStart, TraceEnd, bHit ? FColor::Green : FColor::Red, false, 0.1f);
		if (bHit)
		{
			DrawDebugSphere(GetWorld(), HitResult.ImpactPoint, 10.f, 8, FColor::Yellow, false, 0.1f);
		}
	}

	if (bHit && HitResult.GetActor())
	{
		AActor* HitActor = HitResult.GetActor();

		// Check if actor implements interaction interface
		if (HitActor->GetClass()->ImplementsInterface(USuspenseInteract::StaticClass()))
		{
			return HitActor;
		}
	}

	return nullptr;
}

//==================================================================
// Event Handlers
//==================================================================

void USuspenseCoreInteractionComponent::HandleInteractionSuccessDelegate(const FGameplayEventData* Payload)
{
	if (Payload)
	{
		HandleInteractionSuccess(*Payload);
	}
}

void USuspenseCoreInteractionComponent::HandleInteractionFailureDelegate(const FGameplayEventData* Payload)
{
	if (Payload)
	{
		HandleInteractionFailure(*Payload);
	}
}

void USuspenseCoreInteractionComponent::HandleInteractionSuccess(const FGameplayEventData& Payload)
{
	AActor* TargetActor = nullptr;

	if (Payload.Target)
	{
		TargetActor = const_cast<AActor*>(Cast<const AActor>(Payload.Target.Get()));
	}

	if (!TargetActor)
	{
		LogInteraction(TEXT("HandleInteractionSuccess: No target actor in Payload"), true);
		return;
	}

	// Fire Blueprint delegate
	OnInteractionSucceeded.Broadcast(TargetActor);

	// Broadcast through EventBus
	BroadcastInteractionResult(TargetActor, true);

	// Update UI with interaction type
	if (TargetActor->GetClass()->ImplementsInterface(USuspenseInteract::StaticClass()))
	{
		FGameplayTag InteractionType = ISuspenseInteract::Execute_GetInteractionType(TargetActor);
		OnInteractionTypeChanged.Broadcast(TargetActor, InteractionType);
	}

	LogInteraction(FString::Printf(TEXT("Successful interaction with %s"), *TargetActor->GetName()));
}

void USuspenseCoreInteractionComponent::HandleInteractionFailure(const FGameplayEventData& Payload)
{
	AActor* TargetActor = nullptr;

	if (Payload.Target)
	{
		TargetActor = const_cast<AActor*>(Cast<const AActor>(Payload.Target.Get()));
	}

	// Fire Blueprint delegate
	OnInteractionFailed.Broadcast(TargetActor);

	// Broadcast through EventBus
	BroadcastInteractionResult(TargetActor, false);

	if (TargetActor)
	{
		LogInteraction(FString::Printf(TEXT("Failed interaction with %s"), *TargetActor->GetName()), true);
	}
	else
	{
		LogInteraction(TEXT("Failed interaction, target not found"), true);
	}
}

void USuspenseCoreInteractionComponent::HandleSettingsChanged(
	FGameplayTag EventTag,
	const FSuspenseCoreEventData& EventData)
{
	// Re-apply settings when they change
	ApplySettings(USuspenseCoreInteractionSettings::Get());

	LogInteraction(FString::Printf(TEXT("Settings changed: %s"),
		*EventData.GetString(TEXT("PropertyName"))));
}

//==================================================================
// Helper Methods
//==================================================================

bool USuspenseCoreInteractionComponent::HasBlockingTags() const
{
	if (CachedASC.IsValid())
	{
		return CachedASC->HasAnyMatchingGameplayTags(BlockingTags);
	}

	UAbilitySystemComponent* CurrentASC = GetOwnerASC();
	if (CurrentASC)
	{
		return CurrentASC->HasAnyMatchingGameplayTags(BlockingTags);
	}

	return false;
}

void USuspenseCoreInteractionComponent::LogInteraction(const FString& Message, bool bError) const
{
	const FString Prefix = FString::Printf(TEXT("[%s] "), *GetNameSafe(GetOwner()));

	if (bError)
	{
		UE_LOG(LogSuspenseCoreInteractionComp, Warning, TEXT("%s%s"), *Prefix, *Message);
	}
	else
	{
		UE_LOG(LogSuspenseCoreInteractionComp, Log, TEXT("%s%s"), *Prefix, *Message);
	}
}

UAbilitySystemComponent* USuspenseCoreInteractionComponent::GetOwnerASC() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	// Check owner first (Character may delegate to PlayerState)
	if (OwnerActor->Implements<UAbilitySystemInterface>())
	{
		if (IAbilitySystemInterface* ASInterface = Cast<IAbilitySystemInterface>(OwnerActor))
		{
			if (UAbilitySystemComponent* ASC = ASInterface->GetAbilitySystemComponent())
			{
				return ASC;
			}
		}
	}

	// Check controller or PlayerState
	if (APlayerController* PC = Cast<APlayerController>(OwnerActor->GetInstigatorController()))
	{
		// Check controller
		if (PC->Implements<UAbilitySystemInterface>())
		{
			if (IAbilitySystemInterface* ASInterface = Cast<IAbilitySystemInterface>(PC))
			{
				if (UAbilitySystemComponent* ASC = ASInterface->GetAbilitySystemComponent())
				{
					return ASC;
				}
			}
		}

		// Check pawn
		if (APawn* Pawn = PC->GetPawn())
		{
			if (Pawn->Implements<UAbilitySystemInterface>())
			{
				if (IAbilitySystemInterface* ASInterface = Cast<IAbilitySystemInterface>(Pawn))
				{
					if (UAbilitySystemComponent* ASC = ASInterface->GetAbilitySystemComponent())
					{
						return ASC;
					}
				}
			}
		}
	}

	return nullptr;
}

void USuspenseCoreInteractionComponent::SetInteractionCooldown()
{
	if (InteractionCooldown > 0.0f && GetWorld())
	{
		bInteractionOnCooldown = true;
		GetWorld()->GetTimerManager().SetTimer(
			CooldownTimerHandle,
			this,
			&USuspenseCoreInteractionComponent::ResetInteractionCooldown,
			InteractionCooldown,
			false
		);
	}
}

void USuspenseCoreInteractionComponent::ResetInteractionCooldown()
{
	bInteractionOnCooldown = false;
}

void USuspenseCoreInteractionComponent::UpdateInteractionFocus(AActor* NewFocusActor)
{
	APlayerController* PC = GetOwner() ? GetOwner()->GetInstigatorController<APlayerController>() : nullptr;

	// Handle previous focus loss
	if (LastInteractableActor.IsValid() && LastInteractableActor.Get() != NewFocusActor)
	{
		if (LastInteractableActor->GetClass()->ImplementsInterface(USuspenseInteract::StaticClass()))
		{
			if (PC)
			{
				ISuspenseInteract::Execute_OnInteractionFocusLost(LastInteractableActor.Get(), PC);
			}

			// Broadcast focus lost through EventBus
			BroadcastFocusChanged(LastInteractableActor.Get(), false);

			// Notify UI
			OnInteractionTypeChanged.Broadcast(nullptr, FGameplayTag::EmptyTag);
		}
	}

	// Update focus
	LastInteractableActor = NewFocusActor;

	// Handle new focus gain
	if (NewFocusActor && NewFocusActor->GetClass()->ImplementsInterface(USuspenseInteract::StaticClass()))
	{
		if (PC)
		{
			ISuspenseInteract::Execute_OnInteractionFocusGained(NewFocusActor, PC);
		}

		// Broadcast focus gained through EventBus
		BroadcastFocusChanged(NewFocusActor, true);

		// Notify UI
		FGameplayTag InteractionType = ISuspenseInteract::Execute_GetInteractionType(NewFocusActor);
		OnInteractionTypeChanged.Broadcast(NewFocusActor, InteractionType);
	}
}

void USuspenseCoreInteractionComponent::ApplySettings(const USuspenseCoreInteractionSettings* Settings)
{
	if (!Settings)
	{
		return;
	}

	TraceDistance = Settings->DefaultTraceDistance;
	TraceSphereRadius = Settings->TraceSphereRadius;
	TraceChannel = Settings->DefaultTraceChannel;
	bEnableDebugTrace = Settings->bEnableDebugDraw;
	InteractionCooldown = Settings->DefaultInteractionCooldown;

	// Update tick interval based on focus update rate
	if (Settings->FocusUpdateRate > 0.0f)
	{
		PrimaryComponentTick.TickInterval = 1.0f / Settings->FocusUpdateRate;
	}
}

//==================================================================
// EventBus Broadcasting
//==================================================================

void USuspenseCoreInteractionComponent::BroadcastInteractionAttempt(AActor* TargetActor)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		GetOwner(),
		ESuspenseCoreEventPriority::Normal
	);

	EventData.SetObject(TEXT("TargetActor"), TargetActor);
	EventData.SetObject(TEXT("Instigator"), GetOwner());

	static const FGameplayTag StartedTag =
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Interaction.Started"));

	EmitEvent(StartedTag, EventData);
}

void USuspenseCoreInteractionComponent::BroadcastInteractionResult(AActor* TargetActor, bool bSuccess)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		GetOwner(),
		ESuspenseCoreEventPriority::Normal
	);

	EventData.SetObject(TEXT("TargetActor"), TargetActor);
	EventData.SetObject(TEXT("Instigator"), GetOwner());
	EventData.SetBool(TEXT("Success"), bSuccess);

	static const FGameplayTag CompletedTag =
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Interaction.Completed"));
	static const FGameplayTag CancelledTag =
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Interaction.Cancelled"));

	EmitEvent(bSuccess ? CompletedTag : CancelledTag, EventData);
}

void USuspenseCoreInteractionComponent::BroadcastFocusChanged(AActor* FocusedActor, bool bGained)
{
	// Check if focus broadcasting is enabled
	const USuspenseCoreInteractionSettings* Settings = USuspenseCoreInteractionSettings::Get();
	if (Settings && !Settings->bBroadcastFocusEvents)
	{
		return;
	}

	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		GetOwner(),
		ESuspenseCoreEventPriority::Low
	);

	EventData.SetObject(TEXT("FocusedActor"), FocusedActor);
	EventData.SetObject(TEXT("Instigator"), GetOwner());

	static const FGameplayTag FocusGainedTag =
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Interaction.FocusGained"));
	static const FGameplayTag FocusLostTag =
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Interaction.FocusLost"));

	EmitEvent(bGained ? FocusGainedTag : FocusLostTag, EventData);
}
