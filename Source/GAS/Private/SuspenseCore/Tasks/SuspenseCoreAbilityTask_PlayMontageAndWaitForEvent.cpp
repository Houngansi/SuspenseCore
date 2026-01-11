// SuspenseCoreAbilityTask_PlayMontageAndWaitForEvent.cpp
// SuspenseCore - Montage Ability Task Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Tasks/SuspenseCoreAbilityTask_PlayMontageAndWaitForEvent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Character.h"

USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent::USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent()
	: MontageToPlay(nullptr)
	, Rate(1.0f)
	, StartSection(NAME_None)
	, AnimRootMotionTranslationScale(1.0f)
	, StartTimeSeconds(0.0f)
	, bStopWhenAbilityEnds(true)
	, bIsPlayingMontage(false)
{
}

//========================================================================
// Task Creation
//========================================================================

USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent* USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent::CreateTask(
	UGameplayAbility* OwningAbility,
	FName TaskInstanceName,
	UAnimMontage* InMontageToPlay,
	float InRate,
	FName InStartSection,
	bool bInStopWhenAbilityEnds,
	float InAnimRootMotionTranslationScale,
	float InStartTimeSeconds)
{
	USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent* Task = NewAbilityTask<USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent>(
		OwningAbility, TaskInstanceName);

	if (Task)
	{
		Task->MontageToPlay = InMontageToPlay;
		Task->Rate = InRate;
		Task->StartSection = InStartSection;
		Task->bStopWhenAbilityEnds = bInStopWhenAbilityEnds;
		Task->AnimRootMotionTranslationScale = InAnimRootMotionTranslationScale;
		Task->StartTimeSeconds = InStartTimeSeconds;
	}

	return Task;
}

USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent* USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent::PlayMontageAndWait(
	UGameplayAbility* OwningAbility,
	UAnimMontage* InMontageToPlay,
	float InRate)
{
	return CreateTask(
		OwningAbility,
		FName("PlayMontageAndWait"),
		InMontageToPlay,
		InRate,
		NAME_None,
		true,
		1.0f,
		0.0f
	);
}

//========================================================================
// Event Configuration
//========================================================================

void USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent::SetEventTagsToListenFor(const FGameplayTagContainer& EventTags)
{
	EventTagsToListenFor = EventTags;
}

//========================================================================
// Task Interface
//========================================================================

void USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent::Activate()
{
	if (!Ability)
	{
		return;
	}

	// Get animation instance
	UAnimInstance* AnimInstance = GetAnimInstanceFromAvatar();
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreAbilityTask_PlayMontageAndWaitForEvent: No AnimInstance found"));
		EndTask();
		return;
	}

	// Validate montage
	if (!MontageToPlay)
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreAbilityTask_PlayMontageAndWaitForEvent: No montage specified"));
		EndTask();
		return;
	}

	// Bind to ability cancelled event
	if (Ability->GetCurrentActorInfo() && Ability->GetCurrentActorInfo()->AbilitySystemComponent.IsValid())
	{
		UAbilitySystemComponent* ASC = Ability->GetCurrentActorInfo()->AbilitySystemComponent.Get();
		CancelledHandle = ASC->OnAbilityEnded.AddUObject(this, &USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent::OnAbilityCancelled);

		// Register for gameplay events if tags specified
		for (const FGameplayTag& Tag : EventTagsToListenFor)
		{
			FDelegateHandle Handle = ASC->GenericGameplayEventCallbacks.FindOrAdd(Tag).AddUObject(
				this, &USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent::OnGameplayEvent, Tag);
			EventHandles.Add(Handle);
		}
	}

	// Play the montage
	const float MontageLength = AnimInstance->Montage_Play(MontageToPlay, Rate, EMontagePlayReturnType::MontageLength, StartTimeSeconds);

	if (MontageLength > 0.0f)
	{
		bIsPlayingMontage = true;

		// Jump to section if specified
		if (StartSection != NAME_None)
		{
			AnimInstance->Montage_JumpToSection(StartSection, MontageToPlay);
		}

		// Bind blend out delegate
		BlendingOutDelegate.BindUObject(this, &USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent::OnMontageBlendingOut);
		AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, MontageToPlay);

		// Bind ended delegate
		MontageEndedDelegate.BindUObject(this, &USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent::OnMontageEnded);
		AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, MontageToPlay);

		// Set root motion scale
		if (ACharacter* Character = Cast<ACharacter>(GetAvatarActor()))
		{
			if (UAnimInstance* CharAnimInstance = Character->GetMesh()->GetAnimInstance())
			{
				CharAnimInstance->SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreAbilityTask_PlayMontageAndWaitForEvent: Failed to play montage %s"), *MontageToPlay->GetName());
		EndTask();
	}
}

void USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent::ExternalCancel()
{
	OnCancelled.Broadcast();
	Super::ExternalCancel();
}

FString USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent::GetDebugString() const
{
	return FString::Printf(TEXT("PlayMontageAndWaitForEvent: %s (Playing: %s)"),
		MontageToPlay ? *MontageToPlay->GetName() : TEXT("None"),
		bIsPlayingMontage ? TEXT("Yes") : TEXT("No"));
}

void USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent::OnDestroy(bool bInOwnerFinished)
{
	// Unbind delegates
	if (Ability && Ability->GetCurrentActorInfo() && Ability->GetCurrentActorInfo()->AbilitySystemComponent.IsValid())
	{
		UAbilitySystemComponent* ASC = Ability->GetCurrentActorInfo()->AbilitySystemComponent.Get();
		ASC->OnAbilityEnded.Remove(CancelledHandle);

		// Remove event handles
		for (int32 i = 0; i < EventHandles.Num() && i < EventTagsToListenFor.Num(); ++i)
		{
			TArray<FGameplayTag> Tags;
			EventTagsToListenFor.GetGameplayTagArray(Tags);
			if (i < Tags.Num())
			{
				ASC->GenericGameplayEventCallbacks.FindOrAdd(Tags[i]).Remove(EventHandles[i]);
			}
		}
		EventHandles.Empty();
	}

	// Stop montage if still playing and configured to do so
	if (bStopWhenAbilityEnds && bIsPlayingMontage)
	{
		StopPlayingMontage();
	}

	Super::OnDestroy(bInOwnerFinished);
}

//========================================================================
// Montage Callbacks
//========================================================================

void USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != MontageToPlay)
	{
		return;
	}

	if (bInterrupted)
	{
		OnInterrupted.Broadcast();
	}
	else
	{
		OnBlendOut.Broadcast();
	}
}

void USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != MontageToPlay)
	{
		return;
	}

	bIsPlayingMontage = false;

	if (!bInterrupted)
	{
		OnCompleted.Broadcast();
	}

	EndTask();
}

void USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent::OnAbilityCancelled(const FAbilityEndedData& AbilityEndedData)
{
	// Only cancel if this is our ability that ended
	if (Ability && AbilityEndedData.AbilitySpecHandle == Ability->GetCurrentAbilitySpecHandle())
	{
		if (StopPlayingMontage())
		{
			OnCancelled.Broadcast();
		}
	}
}

void USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent::OnGameplayEvent(const FGameplayEventData* Payload, FGameplayTag EventTag)
{
	if (!bIsPlayingMontage)
	{
		return;
	}

	FGameplayEventData EventData;
	if (Payload)
	{
		EventData = *Payload;
	}

	EventReceived.Broadcast(EventTag, EventData);
}

//========================================================================
// Internal Functions
//========================================================================

bool USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent::StopPlayingMontage()
{
	if (!bIsPlayingMontage)
	{
		return false;
	}

	UAnimInstance* AnimInstance = GetAnimInstanceFromAvatar();
	if (!AnimInstance)
	{
		return false;
	}

	// Check if this montage is still playing
	if (AnimInstance->Montage_IsPlaying(MontageToPlay))
	{
		// Unbind delegates before stopping by unbinding our callbacks
		BlendingOutDelegate.Unbind();
		MontageEndedDelegate.Unbind();

		// Stop with blend out
		AnimInstance->Montage_Stop(0.2f, MontageToPlay);
		bIsPlayingMontage = false;
		return true;
	}

	bIsPlayingMontage = false;
	return false;
}

UAnimInstance* USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent::GetAnimInstanceFromAvatar() const
{
	if (!Ability)
	{
		return nullptr;
	}

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo)
	{
		return nullptr;
	}

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	if (!AvatarActor)
	{
		return nullptr;
	}

	// Try to get from Character's mesh
	if (ACharacter* Character = Cast<ACharacter>(AvatarActor))
	{
		if (USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			return Mesh->GetAnimInstance();
		}
	}

	// Fallback: look for skeletal mesh component
	if (USkeletalMeshComponent* Mesh = AvatarActor->FindComponentByClass<USkeletalMeshComponent>())
	{
		return Mesh->GetAnimInstance();
	}

	return nullptr;
}
