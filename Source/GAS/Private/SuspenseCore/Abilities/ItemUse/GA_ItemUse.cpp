// GA_ItemUse.cpp
// Base Gameplay Ability for Item Use System
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/ItemUse/GA_ItemUse.h"
#include "SuspenseCore/Interfaces/ItemUse/ISuspenseCoreItemUseService.h"
#include "SuspenseCore/Services/SuspenseCoreItemUseService.h"
#include "SuspenseCore/Services/SuspenseCoreServiceProvider.h"
#include "SuspenseCore/Tags/SuspenseCoreItemUseNativeTags.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "TimerManager.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogGA_ItemUse, Log, All);

#define ITEMUSE_ABILITY_LOG(Verbosity, Format, ...) \
	UE_LOG(LogGA_ItemUse, Verbosity, TEXT("[GA_ItemUse] " Format), ##__VA_ARGS__)

//==================================================================
// Constructor
//==================================================================

UGA_ItemUse::UGA_ItemUse()
{
	// Instanced per actor for state tracking
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Can be activated while other abilities are active
	bRetriggerInstancedAbility = false;

	// Blocking tags
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Stunned")));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Disabled")));

	// Block during another item use in progress
	ActivationBlockedTags.AddTag(SuspenseCoreItemUseTags::State::TAG_State_ItemUse_InProgress);

	// Cancel on these tags
	CancelOnTagsAdded.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Damaged")));
	CancelOnTagsAdded.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Stunned")));

	// EventBus integration
	bPublishAbilityEvents = true;
}

//==================================================================
// GameplayAbility Interface
//==================================================================

bool UGA_ItemUse::CanActivateAbility(
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

	// Check service availability
	ISuspenseCoreItemUseService* Service = GetItemUseService();
	if (!Service)
	{
		ITEMUSE_ABILITY_LOG(Warning, TEXT("CanActivateAbility: ItemUseService not available"));
		return false;
	}

	// Build request to validate
	FSuspenseCoreItemUseRequest Request = BuildItemUseRequest(ActorInfo, nullptr);

	if (!Request.IsValid())
	{
		return false;
	}

	// Validate with service
	return Service->CanUseItem(Request);
}

void UGA_ItemUse::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		ITEMUSE_ABILITY_LOG(Warning, TEXT("ActivateAbility: CommitAbility failed"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Get service
	ISuspenseCoreItemUseService* Service = GetItemUseService();
	if (!Service)
	{
		ITEMUSE_ABILITY_LOG(Error, TEXT("ActivateAbility: ItemUseService not available"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Build request
	CurrentRequest = BuildItemUseRequest(ActorInfo, TriggerEventData);
	if (!CurrentRequest.IsValid())
	{
		ITEMUSE_ABILITY_LOG(Warning, TEXT("ActivateAbility: Invalid request"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Execute via service
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	CurrentResponse = Service->UseItem(CurrentRequest, AvatarActor);

	ITEMUSE_ABILITY_LOG(Log, TEXT("ActivateAbility: UseItem result=%d, Duration=%.2f"),
		static_cast<int32>(CurrentResponse.Result),
		CurrentResponse.Duration);

	if (CurrentResponse.IsFailed())
	{
		OnItemUseFailed(CurrentResponse);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (CurrentResponse.IsInProgress())
	{
		// Time-based operation - apply in-progress effect and wait
		bIsInProgress = true;

		// Apply in-progress effect
		if (InProgressEffectClass)
		{
			InProgressEffectHandle = ApplyInProgressEffect(CurrentResponse.Duration);
		}

		// Start duration timer
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				DurationTimerHandle,
				this,
				&UGA_ItemUse::OnDurationTimerComplete,
				CurrentResponse.Duration,
				false
			);

			ITEMUSE_ABILITY_LOG(Log, TEXT("ActivateAbility: Started duration timer for %.2fs"),
				CurrentResponse.Duration);
		}

		// Register for cancel tag changes if cancellable
		if (bIsCancellable && CancelOnTagsAdded.Num() > 0)
		{
			// The ability will be cancelled via CancelAbility if blocking tags are added
			// This is handled by the AbilitySystemComponent automatically
		}
	}
	else
	{
		// Instant operation - complete immediately
		OnItemUseCompleted(CurrentResponse);

		// Apply cooldown if any
		if (CooldownEffectClass && CurrentResponse.Cooldown > 0.0f)
		{
			ApplyCooldownEffect(CurrentResponse.Cooldown);
		}

		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UGA_ItemUse::OnDurationTimerComplete()
{
	if (!bIsInProgress)
	{
		return;
	}

	ITEMUSE_ABILITY_LOG(Log, TEXT("OnDurationTimerComplete: Completing operation %s"),
		*CurrentRequest.RequestID.ToString().Left(8));

	// Get service implementation and complete operation
	// CompleteOperation is on the implementation class, not the interface
	if (USuspenseCoreServiceProvider* Provider = USuspenseCoreServiceProvider::Get(this))
	{
		if (USuspenseCoreItemUseServiceImpl* ServiceImpl = Provider->GetService<USuspenseCoreItemUseServiceImpl>())
		{
			CurrentResponse = ServiceImpl->CompleteOperation(CurrentRequest.RequestID);
		}
		else
		{
			CurrentResponse.Result = ESuspenseCoreItemUseResult::Success;
			CurrentResponse.Progress = 1.0f;
		}
	}
	else
	{
		CurrentResponse.Result = ESuspenseCoreItemUseResult::Success;
		CurrentResponse.Progress = 1.0f;
	}

	bIsInProgress = false;

	// Remove in-progress effect
	RemoveInProgressEffect();

	// Notify completion
	OnItemUseCompleted(CurrentResponse);

	// Apply cooldown if any
	if (CooldownEffectClass && CurrentResponse.Cooldown > 0.0f)
	{
		ApplyCooldownEffect(CurrentResponse.Cooldown);
	}

	// End ability
	if (GetCurrentAbilitySpecHandle().IsValid())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
			GetCurrentActivationInfo(), true, false);
	}
}

void UGA_ItemUse::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Clean up timer
	if (DurationTimerHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(DurationTimerHandle);
		}
	}

	// Remove in-progress effect if still active
	if (bIsInProgress)
	{
		RemoveInProgressEffect();
		bIsInProgress = false;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_ItemUse::CancelAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateCancelAbility)
{
	if (bIsInProgress)
	{
		ITEMUSE_ABILITY_LOG(Log, TEXT("CancelAbility: Cancelling operation %s"),
			*CurrentRequest.RequestID.ToString().Left(8));

		// Cancel via service
		ISuspenseCoreItemUseService* Service = GetItemUseService();
		if (Service)
		{
			Service->CancelUse(CurrentRequest.RequestID);
		}

		// Notify cancellation
		OnItemUseCancelled();

		// Apply cooldown on cancel if configured
		if (bApplyCooldownOnCancel && CooldownEffectClass && CurrentResponse.Cooldown > 0.0f)
		{
			ApplyCooldownEffect(CurrentResponse.Cooldown);
		}
	}

	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

//==================================================================
// Request Building
//==================================================================

FSuspenseCoreItemUseRequest UGA_ItemUse::BuildItemUseRequest(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData) const
{
	FSuspenseCoreItemUseRequest Request;
	Request.Context = ESuspenseCoreItemUseContext::Programmatic;
	Request.RequestingActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;

	if (const UWorld* World = GetWorld())
	{
		Request.RequestTime = World->GetTimeSeconds();
	}

	// Subclasses should override to populate SourceItem, TargetItem, etc.
	return Request;
}

void UGA_ItemUse::OnItemUseCompleted_Implementation(const FSuspenseCoreItemUseResponse& Response)
{
	ITEMUSE_ABILITY_LOG(Log, TEXT("OnItemUseCompleted: RequestID=%s, Handler=%s"),
		*Response.RequestID.ToString().Left(8),
		*Response.HandlerTag.ToString());
}

void UGA_ItemUse::OnItemUseFailed_Implementation(const FSuspenseCoreItemUseResponse& Response)
{
	ITEMUSE_ABILITY_LOG(Warning, TEXT("OnItemUseFailed: RequestID=%s, Result=%d, Message=%s"),
		*Response.RequestID.ToString().Left(8),
		static_cast<int32>(Response.Result),
		*Response.Message.ToString());
}

void UGA_ItemUse::OnItemUseCancelled_Implementation()
{
	ITEMUSE_ABILITY_LOG(Log, TEXT("OnItemUseCancelled: RequestID=%s"),
		*CurrentRequest.RequestID.ToString().Left(8));
}

//==================================================================
// Effects Application
//==================================================================

FActiveGameplayEffectHandle UGA_ItemUse::ApplyInProgressEffect(float Duration)
{
	if (!InProgressEffectClass)
	{
		return FActiveGameplayEffectHandle();
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return FActiveGameplayEffectHandle();
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(InProgressEffectClass, GetAbilityLevel());
	if (!SpecHandle.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	// Set duration via SetByCaller
	// Note: SetSetByCallerMagnitude requires FGameplayTag, not FNativeGameplayTag directly
	SpecHandle.Data->SetSetByCallerMagnitude(SuspenseCoreItemUseTags::Data::TAG_Data_ItemUse_Duration.GetTag(), Duration);

	ITEMUSE_ABILITY_LOG(Verbose, TEXT("ApplyInProgressEffect: Duration=%.2f"), Duration);

	return ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
}

FActiveGameplayEffectHandle UGA_ItemUse::ApplyCooldownEffect(float Cooldown)
{
	if (!CooldownEffectClass)
	{
		return FActiveGameplayEffectHandle();
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return FActiveGameplayEffectHandle();
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(CooldownEffectClass, GetAbilityLevel());
	if (!SpecHandle.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	// Set cooldown via SetByCaller
	// Note: SetSetByCallerMagnitude requires FGameplayTag, not FNativeGameplayTag directly
	SpecHandle.Data->SetSetByCallerMagnitude(SuspenseCoreItemUseTags::Data::TAG_Data_ItemUse_Cooldown.GetTag(), Cooldown);

	ITEMUSE_ABILITY_LOG(Verbose, TEXT("ApplyCooldownEffect: Cooldown=%.2f"), Cooldown);

	return ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
}

void UGA_ItemUse::RemoveInProgressEffect()
{
	if (!InProgressEffectHandle.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		ASC->RemoveActiveGameplayEffect(InProgressEffectHandle);
	}

	InProgressEffectHandle.Invalidate();
}

//==================================================================
// Service Access
//==================================================================

ISuspenseCoreItemUseService* UGA_ItemUse::GetItemUseService() const
{
	if (USuspenseCoreServiceProvider* Provider = USuspenseCoreServiceProvider::Get(this))
	{
		// Get service by name for interface access
		return Provider->GetServiceAs<ISuspenseCoreItemUseService>(FName("ItemUseService"));
	}

	return nullptr;
}
