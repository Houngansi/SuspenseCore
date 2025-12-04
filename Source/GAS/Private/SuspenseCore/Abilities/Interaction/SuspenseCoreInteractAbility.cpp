// SuspenseCoreInteractAbility.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Interaction/SuspenseCoreInteractAbility.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Interfaces/Interaction/ISuspenseCoreInteractable.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"

USuspenseCoreInteractAbility::USuspenseCoreInteractAbility()
{
	InteractDistance = FScalableFloat(300.0f);
	TraceSphereRadius = 0.0f;
	TraceChannel = ECC_Visibility;
	CooldownDuration = FScalableFloat(0.5f);
	bShowDebugTrace = false;
	DebugTraceDuration = 2.0f;

	// Configure ability
	AbilityInputID = ESuspenseAbilityInputID::Interact;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bRetriggerInstancedAbility = false;

	// Initialize tags
	InteractInputTag = FGameplayTag::RequestGameplayTag(FName("Ability.Input.Interact"));
	InteractSuccessTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Ability.Interact.Success"));
	InteractFailedTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Ability.Interact.Failed"));
	InteractCooldownTag = FGameplayTag::RequestGameplayTag(FName("Ability.Cooldown.Interact"));
	InteractingTag = FGameplayTag::RequestGameplayTag(FName("State.Interacting"));

	// AbilityTags - CRITICAL: TryActivateAbilitiesByTag uses AbilityTags, NOT AssetTags!
	// This tag must match what PlayerController passes to ActivateAbilityByTag()
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Ability.Interact")));

	// Also add general interaction tag for categorization
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Interaction")));

	// Applied while interacting
	ActivationOwnedTags.AddTag(InteractingTag);

	// Block tags
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Stunned")));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Disabled")));

	// Cooldown tags
	CooldownTags.AddTag(InteractCooldownTag);
}

//==================================================================
// GameplayAbility Interface
//==================================================================

bool USuspenseCoreInteractAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// Check if on cooldown
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (ASC && ASC->HasAnyMatchingGameplayTags(CooldownTags))
	{
		return false;
	}

	return true;
}

void USuspenseCoreInteractAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UE_LOG(LogTemp, Warning, TEXT("=== USuspenseCoreInteractAbility::ActivateAbility ==="));
	UE_LOG(LogTemp, Warning, TEXT("  Avatar: %s"), *GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr));
	UE_LOG(LogTemp, Warning, TEXT("  IsNetAuthority: %s"), ActorInfo && ActorInfo->IsNetAuthority() ? TEXT("Yes") : TEXT("No"));

	// Store prediction key for networking
	CurrentPredictionKey = ActivationInfo.GetActivationPredictionKey();

	// Perform trace to find target
	AActor* TargetActor = PerformInteractionTrace(ActorInfo);
	UE_LOG(LogTemp, Warning, TEXT("  Trace result: %s"), TargetActor ? *TargetActor->GetName() : TEXT("NO TARGET"));

	if (!TargetActor)
	{
		LogAbilityDebug(TEXT("No interactable target found"), true);
		BroadcastInteractionFailed(nullptr, TEXT("No target found"));
		K2_EndAbility();
		return;
	}

	// Call super to broadcast activation event
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Execute interaction (local prediction + server validation)
	if (ActorInfo->IsNetAuthority())
	{
		// Server: execute directly
		if (ExecuteInteraction(TargetActor))
		{
			BroadcastInteractionSuccess(TargetActor);
		}
		else
		{
			BroadcastInteractionFailed(TargetActor, TEXT("Interaction denied"));
		}
	}
	else
	{
		// Client: send to server for validation
		ServerPerformInteraction(TargetActor);

		// Local prediction: attempt interaction
		ExecuteInteraction(TargetActor);
	}

	// Apply cooldown
	ApplyCooldown(Handle, ActorInfo, ActivationInfo);

	// End ability immediately (interaction is instant)
	K2_EndAbility();
}

void USuspenseCoreInteractAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	LogAbilityDebug(TEXT("Interact ability ended"));
}

//==================================================================
// Network RPCs
//==================================================================

void USuspenseCoreInteractAbility::ServerPerformInteraction_Implementation(AActor* TargetActor)
{
	if (!TargetActor)
	{
		ClientInteractionResult(false, nullptr);
		return;
	}

	// Validate target still exists and is interactable
	if (!TargetActor->GetClass()->ImplementsInterface(USuspenseCoreInteractable::StaticClass()))
	{
		ClientInteractionResult(false, TargetActor);
		return;
	}

	// Get player controller for interaction validation
	APlayerController* PC = nullptr;
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo && ActorInfo->PlayerController.IsValid())
	{
		PC = ActorInfo->PlayerController.Get();
	}

	// Check if interaction is allowed
	if (!ISuspenseCoreInteractable::Execute_CanInteract(TargetActor, PC))
	{
		ClientInteractionResult(false, TargetActor);
		return;
	}

	// Execute server-side interaction
	bool bSuccess = ISuspenseCoreInteractable::Execute_Interact(TargetActor, PC);

	// Notify client of result
	ClientInteractionResult(bSuccess, TargetActor);

	// Broadcast server-side event
	if (bSuccess)
	{
		BroadcastInteractionSuccess(TargetActor);
	}
	else
	{
		BroadcastInteractionFailed(TargetActor, TEXT("Server rejected interaction"));
	}
}

void USuspenseCoreInteractAbility::ClientInteractionResult_Implementation(
	bool bSuccess,
	AActor* TargetActor)
{
	if (bSuccess)
	{
		LogAbilityDebug(FString::Printf(TEXT("Interaction confirmed with %s"),
			*GetNameSafe(TargetActor)));
	}
	else
	{
		LogAbilityDebug(FString::Printf(TEXT("Interaction denied for %s"),
			*GetNameSafe(TargetActor)), true);

		// Could rollback local prediction here if needed
	}
}

//==================================================================
// Interaction Logic
//==================================================================

AActor* USuspenseCoreInteractAbility::PerformInteractionTrace(
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		return nullptr;
	}

	AActor* OwnerActor = ActorInfo->AvatarActor.Get();
	ACharacter* Character = Cast<ACharacter>(OwnerActor);

	// Get view point
	FVector CameraLocation;
	FRotator CameraRotation;

	if (APlayerController* PC = Cast<APlayerController>(ActorInfo->PlayerController.Get()))
	{
		PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
	}
	else if (UCameraComponent* Camera = OwnerActor->FindComponentByClass<UCameraComponent>())
	{
		CameraLocation = Camera->GetComponentLocation();
		CameraRotation = Camera->GetComponentRotation();
	}
	else if (Character)
	{
		CameraLocation = Character->GetActorLocation() + FVector(0, 0, Character->BaseEyeHeight);
		CameraRotation = Character->GetControlRotation();
	}
	else
	{
		CameraLocation = OwnerActor->GetActorLocation() + FVector(0, 0, 50);
		CameraRotation = OwnerActor->GetActorRotation();
	}

	// Calculate trace endpoints
	float Distance = InteractDistance.GetValueAtLevel(1);
	FVector TraceStart = CameraLocation;
	FVector TraceEnd = TraceStart + CameraRotation.Vector() * Distance;

	// Setup trace params
	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(OwnerActor);
	TraceParams.bReturnPhysicalMaterial = false;

	TArray<FHitResult> HitResults;
	bool bHit = false;

	// Perform trace
	if (TraceSphereRadius > 0.0f)
	{
		bHit = GetWorld()->SweepMultiByChannel(
			HitResults,
			TraceStart,
			TraceEnd,
			FQuat::Identity,
			TraceChannel,
			FCollisionShape::MakeSphere(TraceSphereRadius),
			TraceParams
		);
	}
	else
	{
		FHitResult SingleHit;
		bHit = GetWorld()->LineTraceSingleByChannel(
			SingleHit,
			TraceStart,
			TraceEnd,
			TraceChannel,
			TraceParams
		);
		if (bHit)
		{
			HitResults.Add(SingleHit);
		}
	}

	// Try additional channels if primary failed
	if (!bHit)
	{
		for (ECollisionChannel AdditionalChannel : AdditionalTraceChannels)
		{
			FHitResult SingleHit;
			bHit = GetWorld()->LineTraceSingleByChannel(
				SingleHit,
				TraceStart,
				TraceEnd,
				AdditionalChannel,
				TraceParams
			);
			if (bHit)
			{
				HitResults.Add(SingleHit);
				break;
			}
		}
	}

	// Debug visualization
	if (bShowDebugTrace)
	{
		DrawDebugInteraction(TraceStart, TraceEnd, bHit, HitResults);
	}

	// Find first interactable
	for (const FHitResult& Hit : HitResults)
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor && HitActor->GetClass()->ImplementsInterface(USuspenseCoreInteractable::StaticClass()))
		{
			// Verify can interact
			APlayerController* PC = Cast<APlayerController>(ActorInfo->PlayerController.Get());
			if (ISuspenseCoreInteractable::Execute_CanInteract(HitActor, PC))
			{
				return HitActor;
			}
		}
	}

	return nullptr;
}

bool USuspenseCoreInteractAbility::ExecuteInteraction(AActor* TargetActor)
{
	if (!TargetActor)
	{
		return false;
	}

	if (!TargetActor->GetClass()->ImplementsInterface(USuspenseCoreInteractable::StaticClass()))
	{
		return false;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	APlayerController* PC = ActorInfo ? Cast<APlayerController>(ActorInfo->PlayerController.Get()) : nullptr;

	return ISuspenseCoreInteractable::Execute_Interact(TargetActor, PC);
}

void USuspenseCoreInteractAbility::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC)
	{
		return;
	}

	// Apply cooldown via commit ability
	// Note: In production, use a dedicated cooldown effect
	float Duration = CooldownDuration.GetValueAtLevel(1);
	if (Duration > 0.0f)
	{
		// Could apply cooldown effect here
		// For now, rely on ability system's built-in cooldown
	}
}

//==================================================================
// EventBus Broadcasting
//==================================================================

void USuspenseCoreInteractAbility::BroadcastInteractionSuccess(AActor* TargetActor)
{
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

	EventData.SetObject(TEXT("TargetActor"), TargetActor);

	if (TargetActor && TargetActor->GetClass()->ImplementsInterface(USuspenseCoreInteractable::StaticClass()))
	{
		FGameplayTag InteractionType = ISuspenseCoreInteractable::Execute_GetInteractionType(TargetActor);
		EventData.SetString(TEXT("InteractionType"), InteractionType.ToString());
	}

	EventBus->Publish(InteractSuccessTag, EventData);
}

void USuspenseCoreInteractAbility::BroadcastInteractionFailed(AActor* TargetActor, const FString& Reason)
{
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

	EventData.SetObject(TEXT("TargetActor"), TargetActor);
	EventData.SetString(TEXT("Reason"), Reason);

	EventBus->Publish(InteractFailedTag, EventData);
}

//==================================================================
// Debug Helpers
//==================================================================

void USuspenseCoreInteractAbility::DrawDebugInteraction(
	const FVector& Start,
	const FVector& End,
	bool bHit,
	const TArray<FHitResult>& Hits) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FColor LineColor = bHit ? FColor::Green : FColor::Red;

	DrawDebugLine(
		World,
		Start,
		End,
		LineColor,
		false,
		DebugTraceDuration,
		0,
		1.0f
	);

	for (const FHitResult& Hit : Hits)
	{
		DrawDebugSphere(
			World,
			Hit.ImpactPoint,
			10.0f,
			8,
			FColor::Yellow,
			false,
			DebugTraceDuration
		);
	}
}
