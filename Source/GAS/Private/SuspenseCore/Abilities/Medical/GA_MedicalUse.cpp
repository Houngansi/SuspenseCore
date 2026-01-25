// GA_MedicalUse.cpp
// Medical item use ability with animation montage support
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Medical/GA_MedicalUse.h"
#include "SuspenseCore/Abilities/Medical/GA_MedicalEquip.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Tags/SuspenseCoreMedicalNativeTags.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreQuickSlotProvider.h"
#include "SuspenseCore/Handlers/ItemUse/SuspenseCoreMedicalUseHandler.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogMedicalUse, Log, All);

#define MEDICAL_LOG(Verbosity, Format, ...) \
	UE_LOG(LogMedicalUse, Verbosity, TEXT("[MedicalUse][%s] " Format), *GetNameSafe(GetOwningActorFromActorInfo()), ##__VA_ARGS__)

//==================================================================
// Constructor
//==================================================================

UGA_MedicalUse::UGA_MedicalUse()
{
	// Input binding - Fire (LMB) triggers use when medical item is equipped
	AbilityInputID = ESuspenseCoreAbilityInputID::Fire;

	// AbilityTags for activation via TryActivateAbilitiesByTag()
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(SuspenseCoreMedicalTags::Ability::TAG_Ability_Medical_Use);
	SetAssetTags(AssetTags);

	// Ability configuration
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	bRetriggerInstancedAbility = false;

	// Network configuration
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;

	// Blocking tags - can't use while doing these
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Firing);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Reloading);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Dead);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Stunned);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Disabled);

	// Tarkov-style flow: require medical item to be equipped first
	ActivationRequiredTags.AddTag(SuspenseCoreMedicalTags::State::TAG_State_Medical_Equipped);

	// Tags applied while using
	ActivationOwnedTags.AddTag(SuspenseCoreMedicalTags::State::TAG_State_Medical_UsingAnimation);

	// Cancel these abilities when using medical item
	CancelAbilitiesWithTag.AddTag(SuspenseCoreTags::Ability::Sprint);
	CancelAbilitiesWithTag.AddTag(SuspenseCoreTags::Ability::Weapon::AimDownSight);

	// Default cancellation behavior (Tarkov-style)
	bCancelOnDamage = true;
	bCancelOnSprint = true;

	// EventBus configuration
	bPublishAbilityEvents = true;
}

//==================================================================
// Runtime Accessors
//==================================================================

float UGA_MedicalUse::GetUseTime() const
{
	if (!bIsUsing)
	{
		return 0.0f;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	return World->GetTimeSeconds() - UseStartTime;
}

void UGA_MedicalUse::SetMedicalInfo(FName InMedicalItemID, int32 InSlotIndex)
{
	if (InMedicalItemID.IsNone())
	{
		MEDICAL_LOG(Warning, TEXT("SetMedicalInfo: MedicalItemID is None - use may fail"));
	}

	if (InSlotIndex < -1 || InSlotIndex > 7)
	{
		MEDICAL_LOG(Warning, TEXT("SetMedicalInfo: SlotIndex %d is out of valid range [-1 to 7]"), InSlotIndex);
	}

	CurrentMedicalItemID = InMedicalItemID;
	CurrentSlotIndex = InSlotIndex;
	bMedicalInfoSet = true;

	MEDICAL_LOG(Log, TEXT("SetMedicalInfo: MedicalItemID=%s, SlotIndex=%d"),
		*InMedicalItemID.ToString(), InSlotIndex);
}

//==================================================================
// GameplayAbility Interface
//==================================================================

bool UGA_MedicalUse::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	MEDICAL_LOG(Log, TEXT("CanActivateAbility: Starting validation"));

	// Super check includes ActivationRequiredTags (State.Medical.Equipped)
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		MEDICAL_LOG(Warning, TEXT("CanActivateAbility: Super check FAILED"));
		return false;
	}

	// Check if already using
	if (bIsUsing)
	{
		MEDICAL_LOG(Warning, TEXT("CanActivateAbility: Already in use sequence"));
		return false;
	}

	MEDICAL_LOG(Log, TEXT("CanActivateAbility: PASSED (State.Medical.Equipped verified by Super)"));
	return true;
}

void UGA_MedicalUse::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Cache for later use
	CachedActorInfo = ActorInfo;
	CachedSpecHandle = Handle;
	CachedActivationInfo = ActivationInfo;

	// Get medical item info from GA_MedicalEquip if not already set
	if (!bMedicalInfoSet)
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		if (ASC)
		{
			// Find active GA_MedicalEquip instance to get item info
			FGameplayTagContainer EquipTags;
			EquipTags.AddTag(SuspenseCoreMedicalTags::Ability::TAG_Ability_Medical_Equip);

			TArray<FGameplayAbilitySpec*> MatchingSpecs;
			ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(EquipTags, MatchingSpecs, false);

			for (FGameplayAbilitySpec* Spec : MatchingSpecs)
			{
				if (Spec && Spec->IsActive())
				{
					UGA_MedicalEquip* EquipAbility = Cast<UGA_MedicalEquip>(Spec->GetPrimaryInstance());
					if (EquipAbility)
					{
						CurrentMedicalItemID = EquipAbility->GetMedicalItemID();
						CurrentSlotIndex = EquipAbility->SourceQuickSlotIndex;
						CurrentMedicalTypeTag = EquipAbility->MedicalTypeTag;
						bMedicalInfoSet = true;

						MEDICAL_LOG(Log, TEXT("ActivateAbility: Got info from GA_MedicalEquip: %s"),
							*CurrentMedicalItemID.ToString());
						break;
					}
				}
			}
		}
	}

	if (!bMedicalInfoSet || CurrentMedicalItemID.IsNone())
	{
		MEDICAL_LOG(Warning, TEXT("ActivateAbility: No medical item info available"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Start use phase
	bIsUsing = true;
	bEffectsApplied = false;
	UseStartTime = GetWorld()->GetTimeSeconds();

	// Apply effects (speed debuff)
	ApplyUseEffects();

	// Play montage
	if (!PlayUseMontage())
	{
		MEDICAL_LOG(Warning, TEXT("ActivateAbility: Failed to play use montage"));
		RemoveUseEffects();
		bIsUsing = false;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Broadcast use started
	PlaySound(UseStartSound);
	OnUseStarted();
	BroadcastMedicalEvent(SuspenseCoreMedicalTags::Event::TAG_Event_Medical_UseStarted);

	MEDICAL_LOG(Log, TEXT("Medical use started: Item=%s"), *CurrentMedicalItemID.ToString());

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UGA_MedicalUse::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Unsubscribe from AnimNotify before cleanup
	if (CachedAnimInstance.IsValid())
	{
		CachedAnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(
			this, &UGA_MedicalUse::OnAnimNotifyBegin);
		CachedAnimInstance.Reset();
	}

	// Clean up
	RemoveUseEffects();
	StopUseMontage();

	if (bWasCancelled)
	{
		// Only broadcast cancelled if effects weren't applied
		if (!bEffectsApplied)
		{
			OnUseCancelled();
			BroadcastMedicalEvent(SuspenseCoreMedicalTags::Event::TAG_Event_Medical_UseCancelled);
			PlaySound(CancelSound);
			MEDICAL_LOG(Log, TEXT("Medical use cancelled (effects not applied)"));
		}
		else
		{
			// Effects were applied but animation cancelled - still consume item
			ConsumeMedicalItem();
			MEDICAL_LOG(Log, TEXT("Medical use cancelled after effects applied - item consumed"));
		}
	}

	// Reset state
	bIsUsing = false;
	bEffectsApplied = false;
	bMedicalInfoSet = false;
	UseStartTime = 0.0f;
	CurrentSlotIndex = -1;
	CurrentMedicalItemID = NAME_None;
	CurrentMedicalTypeTag = FGameplayTag();

	// Cancel GA_MedicalEquip after successful use
	if (!bWasCancelled)
	{
		CancelEquipAbility();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_MedicalUse::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	// Medical use continues even after input release (unlike grenades)
	// Player must wait for animation to complete or cancel
	MEDICAL_LOG(Verbose, TEXT("InputReleased - use continues"));

	Super::InputReleased(Handle, ActorInfo, ActivationInfo);
}

//==================================================================
// Animation Notify Handlers
//==================================================================

void UGA_MedicalUse::OnStartNotify()
{
	MEDICAL_LOG(Log, TEXT("OnStartNotify: Use animation started"));

	// This is when the item is visually "in hand" and use begins
	// No special action needed, just confirmation
}

void UGA_MedicalUse::OnApplyNotify()
{
	MEDICAL_LOG(Log, TEXT("OnApplyNotify: Applying medical effects"));

	// Apply effects - this is the point of no return
	if (ApplyMedicalEffects())
	{
		bEffectsApplied = true;
		PlaySound(ApplySound);
		OnEffectsApplied();
		BroadcastMedicalEvent(SuspenseCoreMedicalTags::Event::TAG_Event_Medical_ApplyEffect);

		MEDICAL_LOG(Log, TEXT("Medical effects applied successfully"));
	}
	else
	{
		MEDICAL_LOG(Warning, TEXT("Failed to apply medical effects"));
	}
}

void UGA_MedicalUse::OnCompleteNotify()
{
	MEDICAL_LOG(Log, TEXT("OnCompleteNotify: Use complete"));

	// Consume the item
	ConsumeMedicalItem();

	PlaySound(CompleteSound);
	OnUseCompleted();
	BroadcastMedicalEvent(SuspenseCoreMedicalTags::Event::TAG_Event_Medical_UseComplete);

	MEDICAL_LOG(Log, TEXT("Medical use completed, item consumed"));
}

void UGA_MedicalUse::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bInterrupted)
	{
		EndAbility(CachedSpecHandle, CachedActorInfo, CachedActivationInfo, true, true);
	}
	else
	{
		// Montage completed - if effects weren't applied via notify, apply now
		if (!bEffectsApplied)
		{
			MEDICAL_LOG(Log, TEXT("OnMontageEnded: Montage complete, applying effects"));
			OnApplyNotify();
		}

		// Complete if not already done via notify
		if (bEffectsApplied && CurrentSlotIndex >= 0)
		{
			OnCompleteNotify();
		}

		MEDICAL_LOG(Log, TEXT("Medical use completed"));
		EndAbility(CachedSpecHandle, CachedActorInfo, CachedActivationInfo, true, false);
	}
}

void UGA_MedicalUse::OnMontageBlendOut(UAnimMontage* Montage, bool bInterrupted)
{
	// Optional: Handle blend out if needed
}

void UGA_MedicalUse::OnAnimNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload)
{
	MEDICAL_LOG(Log, TEXT("AnimNotify received: '%s'"), *NotifyName.ToString());

	// Phase 1: Start - "Start" or "Begin" marks use started
	if (NotifyName == FName("Start") ||
		NotifyName == FName("Begin") ||
		NotifyName == FName("UseStart"))
	{
		MEDICAL_LOG(Log, TEXT("  -> Phase 1: Start"));
		OnStartNotify();
	}
	// Phase 2: Apply - "Apply" or "Effect" or "Heal" marks effect application
	else if (NotifyName == FName("Apply") ||
	         NotifyName == FName("Effect") ||
	         NotifyName == FName("Heal") ||
	         NotifyName == FName("ClipIn") ||  // Reuse grenade notifies if needed
	         NotifyName == FName("Continue"))
	{
		MEDICAL_LOG(Log, TEXT("  -> Phase 2: Apply"));
		OnApplyNotify();
	}
	// Phase 3: Complete - "Complete" or "Finalize" or "Done" marks completion
	else if (NotifyName == FName("Complete") ||
	         NotifyName == FName("Finalize") ||
	         NotifyName == FName("Done") ||
	         NotifyName == FName("Consume"))
	{
		MEDICAL_LOG(Log, TEXT("  -> Phase 3: Complete"));
		OnCompleteNotify();
	}
}

//==================================================================
// Internal Methods
//==================================================================

UAnimMontage* UGA_MedicalUse::GetMontageForMedicalType() const
{
	// Check medical type tag to select appropriate montage
	FString TypeStr = CurrentMedicalTypeTag.ToString();

	if (TypeStr.Contains(TEXT("Bandage")))
	{
		if (BandageUseMontage)
		{
			return BandageUseMontage.Get();
		}
	}
	else if (TypeStr.Contains(TEXT("Medkit")) || TypeStr.Contains(TEXT("IFAK")))
	{
		if (MedkitUseMontage)
		{
			return MedkitUseMontage.Get();
		}
	}
	else if (TypeStr.Contains(TEXT("Injector")) || TypeStr.Contains(TEXT("Stimulant")) || TypeStr.Contains(TEXT("Morphine")))
	{
		if (InjectorUseMontage)
		{
			return InjectorUseMontage.Get();
		}
	}

	// Fallback to default
	return DefaultUseMontage.Get();
}

bool UGA_MedicalUse::PlayUseMontage()
{
	UAnimMontage* Montage = GetMontageForMedicalType();
	if (!Montage)
	{
		MEDICAL_LOG(Warning, TEXT("PlayUseMontage: No montage available"));
		return false;
	}

	MEDICAL_LOG(Log, TEXT("PlayUseMontage: Using montage '%s'"), *Montage->GetName());

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	ACharacter* Character = Cast<ACharacter>(AvatarActor);
	if (!Character)
	{
		MEDICAL_LOG(Warning, TEXT("PlayUseMontage: No character available"));
		return false;
	}

	USkeletalMeshComponent* MeshComp = Character->GetMesh();
	if (!MeshComp)
	{
		MEDICAL_LOG(Warning, TEXT("PlayUseMontage: No mesh component"));
		return false;
	}

	UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
	if (!AnimInstance)
	{
		// Try other skeletal mesh components (MetaHuman)
		TArray<USkeletalMeshComponent*> SkeletalMeshes;
		Character->GetComponents<USkeletalMeshComponent>(SkeletalMeshes);

		for (USkeletalMeshComponent* SMC : SkeletalMeshes)
		{
			if (SMC && SMC != MeshComp && SMC->GetAnimInstance())
			{
				AnimInstance = SMC->GetAnimInstance();
				break;
			}
		}

		if (!AnimInstance)
		{
			MEDICAL_LOG(Warning, TEXT("PlayUseMontage: No AnimInstance found"));
			return false;
		}
	}

	// Play montage
	float Duration = AnimInstance->Montage_Play(Montage, 1.0f);
	if (Duration <= 0.0f)
	{
		MEDICAL_LOG(Warning, TEXT("PlayUseMontage: Montage_Play failed"));
		return false;
	}

	MEDICAL_LOG(Log, TEXT("PlayUseMontage: SUCCESS! Duration=%.2f"), Duration);

	// Bind to montage end
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &UGA_MedicalUse::OnMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);

	// Bind to blend out
	FOnMontageBlendingOutStarted BlendOutDelegate;
	BlendOutDelegate.BindUObject(this, &UGA_MedicalUse::OnMontageBlendOut);
	AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, Montage);

	// Bind to AnimNotify events
	CachedAnimInstance = AnimInstance;
	AnimInstance->OnPlayMontageNotifyBegin.AddDynamic(this, &UGA_MedicalUse::OnAnimNotifyBegin);

	return true;
}

void UGA_MedicalUse::StopUseMontage()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	UAnimMontage* Montage = GetMontageForMedicalType();
	if (Montage && AnimInstance->Montage_IsPlaying(Montage))
	{
		AnimInstance->Montage_Stop(0.2f, Montage);
	}
}

bool UGA_MedicalUse::ApplyMedicalEffects()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		MEDICAL_LOG(Error, TEXT("ApplyMedicalEffects: No AvatarActor"));
		return false;
	}

	// Build item use request
	FSuspenseCoreItemUseRequest Request;
	Request.ItemID = CurrentMedicalItemID;
	Request.SourceSlotIndex = CurrentSlotIndex;
	Request.OwnerActor = AvatarActor;
	Request.Timestamp = FPlatformTime::Seconds();

	// Publish apply effect event - MedicalUseHandler will handle the actual effects
	if (USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this))
	{
		if (USuspenseCoreEventBus* EventBus = EventManager->GetEventBus())
		{
			FSuspenseCoreEventData EventData;
			EventData.Source = AvatarActor;
			EventData.Timestamp = FPlatformTime::Seconds();
			EventData.StringPayload.Add(TEXT("MedicalItemID"), CurrentMedicalItemID.ToString());
			EventData.IntPayload.Add(TEXT("QuickSlotIndex"), CurrentSlotIndex);

			// This event can be subscribed to by MedicalUseHandler to apply effects
			EventBus->Publish(SuspenseCoreMedicalTags::Event::TAG_Event_Medical_ApplyEffect, EventData);

			MEDICAL_LOG(Log, TEXT("Published ApplyEffect event for %s"), *CurrentMedicalItemID.ToString());
			return true;
		}
	}

	MEDICAL_LOG(Warning, TEXT("ApplyMedicalEffects: No EventBus available"));
	return false;
}

void UGA_MedicalUse::ConsumeMedicalItem()
{
	if (CurrentSlotIndex < 0)
	{
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		return;
	}

	// Find QuickSlotProvider and clear the slot
	TArray<UActorComponent*> Components;
	AvatarActor->GetComponents(Components);

	for (UActorComponent* Comp : Components)
	{
		if (Comp && Comp->Implements<USuspenseCoreQuickSlotProvider>())
		{
			ISuspenseCoreQuickSlotProvider::Execute_ClearSlot(Comp, CurrentSlotIndex);
			MEDICAL_LOG(Log, TEXT("ConsumeMedicalItem: Cleared slot %d"), CurrentSlotIndex);
			break;
		}
	}
}

void UGA_MedicalUse::ApplyUseEffects()
{
	if (UseSpeedDebuffClass && CachedActorInfo && CachedActorInfo->AbilitySystemComponent.IsValid())
	{
		FGameplayEffectContextHandle EffectContext = CachedActorInfo->AbilitySystemComponent->MakeEffectContext();
		EffectContext.AddSourceObject(this);

		FGameplayEffectSpecHandle SpecHandle = CachedActorInfo->AbilitySystemComponent->MakeOutgoingSpec(
			UseSpeedDebuffClass, 1.0f, EffectContext);

		if (SpecHandle.IsValid())
		{
			UseSpeedEffectHandle = CachedActorInfo->AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}
}

void UGA_MedicalUse::RemoveUseEffects()
{
	if (UseSpeedEffectHandle.IsValid() && CachedActorInfo && CachedActorInfo->AbilitySystemComponent.IsValid())
	{
		CachedActorInfo->AbilitySystemComponent->RemoveActiveGameplayEffect(UseSpeedEffectHandle);
		UseSpeedEffectHandle.Invalidate();
	}
}

void UGA_MedicalUse::PlaySound(USoundBase* Sound)
{
	if (!Sound)
	{
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(
		AvatarActor,
		Sound,
		AvatarActor->GetActorLocation(),
		AvatarActor->GetActorRotation(),
		1.0f,
		1.0f,
		0.0f,
		nullptr,
		nullptr,
		AvatarActor
	);

	MEDICAL_LOG(Verbose, TEXT("PlaySound: %s"), *Sound->GetName());
}

void UGA_MedicalUse::BroadcastMedicalEvent(FGameplayTag EventTag)
{
	if (USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this))
	{
		if (USuspenseCoreEventBus* EventBus = EventManager->GetEventBus())
		{
			FSuspenseCoreEventData EventData;
			EventData.Source = GetAvatarActorFromActorInfo();
			EventData.Timestamp = FPlatformTime::Seconds();
			EventData.StringPayload.Add(TEXT("MedicalItemID"), CurrentMedicalItemID.ToString());
			EventData.StringPayload.Add(TEXT("MedicalType"), CurrentMedicalTypeTag.ToString());
			EventData.IntPayload.Add(TEXT("QuickSlotIndex"), CurrentSlotIndex);
			EventData.FloatPayload.Add(TEXT("UseTime"), GetUseTime());
			EventData.BoolPayload.Add(TEXT("EffectsApplied"), bEffectsApplied);

			EventBus->Publish(EventTag, EventData);

			MEDICAL_LOG(Verbose, TEXT("BroadcastMedicalEvent: %s"), *EventTag.ToString());
		}
	}
}

USuspenseCoreMedicalUseHandler* UGA_MedicalUse::GetMedicalUseHandler() const
{
	// MedicalUseHandler is typically accessed via ServiceProvider
	// For now, rely on EventBus for communication
	return nullptr;
}

void UGA_MedicalUse::CancelEquipAbility()
{
	if (!CachedActorInfo || !CachedActorInfo->AbilitySystemComponent.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* ASC = CachedActorInfo->AbilitySystemComponent.Get();

	// Cancel GA_MedicalEquip by tag
	FGameplayTagContainer EquipTags;
	EquipTags.AddTag(SuspenseCoreMedicalTags::Ability::TAG_Ability_Medical_Equip);
	ASC->CancelAbilities(&EquipTags);

	MEDICAL_LOG(Log, TEXT("Cancelled GA_MedicalEquip after successful use"));
}

#undef MEDICAL_LOG
