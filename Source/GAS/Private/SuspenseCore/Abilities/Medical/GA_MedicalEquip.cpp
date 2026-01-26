// GA_MedicalEquip.cpp
// Ability for equipping medical items (Tarkov-style flow)
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Medical/GA_MedicalEquip.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Tags/SuspenseCoreMedicalNativeTags.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreQuickSlotProvider.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogMedicalEquip, Log, All);

#define MEDICAL_LOG(Verbosity, Format, ...) \
	UE_LOG(LogMedicalEquip, Verbosity, TEXT("[MedicalEquip] " Format), ##__VA_ARGS__)

//==================================================================
// Constructor
//==================================================================

UGA_MedicalEquip::UGA_MedicalEquip()
{
	// Instanced per execution - we need state
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;

	// Ability tags - use SetAssetTags for new API
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(SuspenseCoreMedicalTags::Ability::TAG_Ability_Medical_Equip);
	SetAssetTags(AssetTags);

	// Configure ability trigger so handler can pass medical item data
	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = SuspenseCoreMedicalTags::Ability::TAG_Ability_Medical_Equip;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);

	// Blocking tags - can't equip while these states are active
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Dead);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Stunned);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Disabled);
	ActivationBlockedTags.AddTag(SuspenseCoreMedicalTags::State::TAG_State_Medical_Equipped);

	// Cancel these abilities when we activate
	CancelAbilitiesWithTag.AddTag(SuspenseCoreTags::Ability::Weapon::Fire);
	CancelAbilitiesWithTag.AddTag(SuspenseCoreTags::Ability::Weapon::Reload);
	CancelAbilitiesWithTag.AddTag(SuspenseCoreTags::Ability::Weapon::AimDownSight);

	// Cancel grenade equip - prevents Fire input conflict
	// When medical item is equipped, grenade must be unequipped
	CancelAbilitiesWithTag.AddTag(SuspenseCoreTags::Ability::Throwable::Equip);
	CancelAbilitiesWithTag.AddTag(SuspenseCoreTags::Ability::Throwable::Grenade);

	// Default timing
	MinEquipTime = 0.2f;
	DrawMontagePlayRate = 1.0f;

	// EventBus integration
	bPublishAbilityEvents = true;
}

//==================================================================
// Public Methods
//==================================================================

float UGA_MedicalEquip::GetEquipTime() const
{
	if (!bMedicalReady)
	{
		return 0.0f;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	return World->GetTimeSeconds() - EquipStartTime;
}

void UGA_MedicalEquip::SetMedicalInfo(FName InMedicalItemID, FGameplayTag InMedicalTypeTag, int32 InSlotIndex)
{
	// Validate MedicalItemID
	if (InMedicalItemID.IsNone())
	{
		MEDICAL_LOG(Warning, TEXT("SetMedicalInfo: MedicalItemID is None - this may cause issues"));
	}

	// Validate SlotIndex (QuickSlots are typically 0-7)
	if (InSlotIndex < 0 || InSlotIndex > 7)
	{
		MEDICAL_LOG(Warning, TEXT("SetMedicalInfo: SlotIndex %d is out of valid range [0-7]"), InSlotIndex);
	}

	// Validate MedicalTypeTag
	if (!InMedicalTypeTag.IsValid())
	{
		MEDICAL_LOG(Log, TEXT("SetMedicalInfo: MedicalTypeTag is invalid - will use default"));
	}

	MedicalItemID = InMedicalItemID;
	MedicalTypeTag = InMedicalTypeTag;
	SourceQuickSlotIndex = InSlotIndex;

	MEDICAL_LOG(Log, TEXT("SetMedicalInfo: ID=%s, Type=%s, Slot=%d"),
		*MedicalItemID.ToString(),
		*MedicalTypeTag.ToString(),
		SourceQuickSlotIndex);
}

void UGA_MedicalEquip::RequestUnequip()
{
	if (bUnequipRequested)
	{
		return;
	}

	bUnequipRequested = true;
	MEDICAL_LOG(Log, TEXT("Unequip requested"));

	OnMedicalUnequipping();
	PlayHolsterMontage();
}

//==================================================================
// GameplayAbility Interface
//==================================================================

bool UGA_MedicalEquip::CanActivateAbility(
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

	// Medical item data can be passed via FGameplayEventData
	// so MedicalItemID may not be set yet at this point
	return true;
}

void UGA_MedicalEquip::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Double-activation guard
	if (bMedicalReady || bUnequipRequested)
	{
		MEDICAL_LOG(Warning, TEXT("ActivateAbility: Already in active state (Ready=%s, Unequip=%s) - aborting"),
			bMedicalReady ? TEXT("true") : TEXT("false"),
			bUnequipRequested ? TEXT("true") : TEXT("false"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		MEDICAL_LOG(Warning, TEXT("Failed to commit ability"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Extract medical item info from TriggerEventData if available
	if (TriggerEventData)
	{
		// MedicalTypeTag from EventTag
		if (TriggerEventData->EventTag.IsValid())
		{
			MedicalTypeTag = TriggerEventData->EventTag;
			MEDICAL_LOG(Log, TEXT("Extracted MedicalTypeTag from EventTag: %s"), *MedicalTypeTag.ToString());
		}

		// Extract MedicalType from InstigatorTags
		for (const FGameplayTag& Tag : TriggerEventData->InstigatorTags)
		{
			FString TagStr = Tag.ToString();
			if (TagStr.StartsWith(TEXT("Item.Medical.")))
			{
				MedicalTypeTag = Tag;
				MEDICAL_LOG(Log, TEXT("Extracted MedicalTypeTag from InstigatorTags: %s"), *MedicalTypeTag.ToString());
				break;
			}
		}

		// SlotIndex from EventMagnitude
		if (TriggerEventData->EventMagnitude >= 0.0f)
		{
			SourceQuickSlotIndex = FMath::RoundToInt(TriggerEventData->EventMagnitude);
			MEDICAL_LOG(Log, TEXT("Extracted SlotIndex from EventMagnitude: %d"), SourceQuickSlotIndex);
		}
	}

	// Look up MedicalItemID from QuickSlot component using slot index
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (AvatarActor && SourceQuickSlotIndex >= 0)
	{
		TArray<UActorComponent*> Components;
		AvatarActor->GetComponents(Components);

		for (UActorComponent* Comp : Components)
		{
			if (Comp && Comp->Implements<USuspenseCoreQuickSlotProvider>())
			{
				FSuspenseCoreQuickSlot SlotData =
					ISuspenseCoreQuickSlotProvider::Execute_GetQuickSlot(Comp, SourceQuickSlotIndex);

				if (!SlotData.AssignedItemID.IsNone())
				{
					MedicalItemID = SlotData.AssignedItemID;
					MEDICAL_LOG(Log, TEXT("Looked up MedicalItemID from QuickSlot[%d]: %s"),
						SourceQuickSlotIndex, *MedicalItemID.ToString());
				}
				break;
			}
		}
	}

	MEDICAL_LOG(Log, TEXT("ActivateAbility: Equipping medical item %s"), *MedicalItemID.ToString());

	// Reset state
	bMedicalReady = false;
	bUnequipRequested = false;
	EquipStartTime = 0.0f;

	// Store previous weapon type
	StorePreviousWeaponState();

	// Request stance change via EventBus
	RequestStanceChange(true);

	// Grant State.Medical.Equipped tag
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		ASC->AddLooseGameplayTag(SuspenseCoreMedicalTags::State::TAG_State_Medical_Equipped);
		MEDICAL_LOG(Verbose, TEXT("Granted State.Medical.Equipped tag"));
	}

	// Broadcast equip started event
	BroadcastEquipEvent(SuspenseCoreMedicalTags::Event::TAG_Event_Medical_Equipped);

	// Play draw montage
	PlayDrawMontage();
}

void UGA_MedicalEquip::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	MEDICAL_LOG(Log, TEXT("EndAbility: Cancelled=%s"), bWasCancelled ? TEXT("true") : TEXT("false"));

	// Remove State.Medical.Equipped tag
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		ASC->RemoveLooseGameplayTag(SuspenseCoreMedicalTags::State::TAG_State_Medical_Equipped);
		MEDICAL_LOG(Verbose, TEXT("Removed State.Medical.Equipped tag"));
	}

	// Always restore previous weapon state when medical equip ends
	RestorePreviousWeaponState();
	MEDICAL_LOG(Log, TEXT("EndAbility: Restored previous weapon state"));

	// Broadcast unequip event
	BroadcastEquipEvent(SuspenseCoreMedicalTags::Event::TAG_Event_Medical_Unequipped);

	// Clean up montage task
	if (ActiveMontageTask)
	{
		ActiveMontageTask->EndTask();
		ActiveMontageTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_MedicalEquip::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	// Equip ability doesn't end on input release
	// It stays active until:
	// 1. Use is complete (GA_MedicalUse ends this)
	// 2. Cancel is requested (weapon switch, etc.)
	// 3. Player presses QuickSlot again to unequip

	MEDICAL_LOG(Verbose, TEXT("InputReleased - ability remains active"));
}

//==================================================================
// Internal Methods
//==================================================================

void UGA_MedicalEquip::RequestStanceChange(bool bEquipping)
{
	// Broadcast stance change request via EventBus
	if (USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this))
	{
		if (USuspenseCoreEventBus* EventBus = EventManager->GetEventBus())
		{
			FSuspenseCoreEventData EventData;
			EventData.Source = GetAvatarActorFromActorInfo();
			EventData.Timestamp = FPlatformTime::Seconds();

			// Medical type tag for animation selection
			FGameplayTag StanceTag = MedicalTypeTag.IsValid() ?
				MedicalTypeTag :
				SuspenseCoreTags::Item::Medical;

			EventData.StringPayload.Add(TEXT("WeaponType"), StanceTag.ToString());
			EventData.BoolPayload.Add(TEXT("IsDrawn"), bEquipping);
			EventData.BoolPayload.Add(TEXT("IsMedical"), true);

			FGameplayTag EventTag = bEquipping ?
				SuspenseCoreTags::Event::Weapon::StanceChangeRequested :
				SuspenseCoreTags::Event::Weapon::StanceRestoreRequested;

			EventBus->Publish(EventTag, EventData);

			MEDICAL_LOG(Log, TEXT("Requested stance change: %s, Equipping=%s"),
				*StanceTag.ToString(), bEquipping ? TEXT("true") : TEXT("false"));
		}
	}
}

void UGA_MedicalEquip::PlayDrawMontage()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		MEDICAL_LOG(Warning, TEXT("PlayDrawMontage: No character"));
		OnDrawMontageCompleted();
		return;
	}

	UAnimMontage* DrawMontage = DefaultDrawMontage;

	if (!DrawMontage)
	{
		MEDICAL_LOG(Log, TEXT("No draw montage available, completing immediately"));
		OnDrawMontageCompleted();
		return;
	}

	// Create and activate montage task
	ActiveMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		DrawMontage,
		DrawMontagePlayRate,
		NAME_None,
		true,  // bStopWhenAbilityEnds
		1.0f   // AnimRootMotionTranslationScale
	);

	if (ActiveMontageTask)
	{
		ActiveMontageTask->OnCompleted.AddDynamic(this, &UGA_MedicalEquip::OnDrawMontageCompleted);
		ActiveMontageTask->OnBlendOut.AddDynamic(this, &UGA_MedicalEquip::OnDrawMontageCompleted);
		ActiveMontageTask->OnInterrupted.AddDynamic(this, &UGA_MedicalEquip::OnDrawMontageInterrupted);
		ActiveMontageTask->OnCancelled.AddDynamic(this, &UGA_MedicalEquip::OnDrawMontageInterrupted);

		ActiveMontageTask->ReadyForActivation();

		MEDICAL_LOG(Log, TEXT("Playing draw montage: %s"), *DrawMontage->GetName());
	}
	else
	{
		MEDICAL_LOG(Warning, TEXT("Failed to create montage task"));
		OnDrawMontageCompleted();
	}
}

void UGA_MedicalEquip::OnDrawMontageCompleted()
{
	MEDICAL_LOG(Log, TEXT("Draw montage completed - medical item ready"));

	bMedicalReady = true;

	UWorld* World = GetWorld();
	if (World)
	{
		EquipStartTime = World->GetTimeSeconds();
	}

	// Notify blueprint
	OnMedicalEquipped();

	// Broadcast ready event
	BroadcastEquipEvent(SuspenseCoreMedicalTags::Event::TAG_Event_Medical_Ready);

	// Clear montage task reference
	ActiveMontageTask = nullptr;
}

void UGA_MedicalEquip::OnDrawMontageInterrupted()
{
	MEDICAL_LOG(Log, TEXT("Draw montage interrupted"));

	// If medical item is not yet ready, proceed to equipped state anyway
	if (!bMedicalReady)
	{
		MEDICAL_LOG(Warning, TEXT("Montage failed/interrupted before ready - proceeding to equipped state anyway"));
		OnDrawMontageCompleted();
		return;
	}

	CancelAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true);
}

void UGA_MedicalEquip::PlayHolsterMontage()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		OnHolsterMontageCompleted();
		return;
	}

	UAnimMontage* HolsterMontage = DefaultHolsterMontage;

	if (!HolsterMontage)
	{
		MEDICAL_LOG(Log, TEXT("No holster montage, completing immediately"));
		OnHolsterMontageCompleted();
		return;
	}

	// Create and activate montage task
	ActiveMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		HolsterMontage,
		1.0f,
		NAME_None,
		true,
		1.0f
	);

	if (ActiveMontageTask)
	{
		ActiveMontageTask->OnCompleted.AddDynamic(this, &UGA_MedicalEquip::OnHolsterMontageCompleted);
		ActiveMontageTask->OnBlendOut.AddDynamic(this, &UGA_MedicalEquip::OnHolsterMontageCompleted);
		ActiveMontageTask->OnInterrupted.AddDynamic(this, &UGA_MedicalEquip::OnHolsterMontageCompleted);
		ActiveMontageTask->OnCancelled.AddDynamic(this, &UGA_MedicalEquip::OnHolsterMontageCompleted);

		ActiveMontageTask->ReadyForActivation();

		MEDICAL_LOG(Log, TEXT("Playing holster montage"));
	}
	else
	{
		OnHolsterMontageCompleted();
	}
}

void UGA_MedicalEquip::OnHolsterMontageCompleted()
{
	MEDICAL_LOG(Log, TEXT("Holster montage completed"));

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UGA_MedicalEquip::StorePreviousWeaponState()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		FGameplayTagContainer OwnedTags;
		ASC->GetOwnedGameplayTags(OwnedTags);

		// Find current weapon type tag
		for (const FGameplayTag& Tag : OwnedTags)
		{
			if (Tag.MatchesTag(SuspenseCoreTags::Weapon::Grenade::Frag) ||
			    Tag.MatchesTag(SuspenseCoreTags::Weapon::Grenade::Smoke) ||
			    Tag.MatchesTag(SuspenseCoreTags::Weapon::Grenade::Flash) ||
			    Tag.MatchesTag(SuspenseCoreTags::Weapon::Grenade::Incendiary))
			{
				PreviousWeaponType = Tag;
				break;
			}
		}

		bPreviousWeaponDrawn = OwnedTags.HasTag(SuspenseCoreTags::State::WeaponDrawn);

		MEDICAL_LOG(Verbose, TEXT("Stored previous weapon: %s (drawn=%s)"),
			*PreviousWeaponType.ToString(),
			bPreviousWeaponDrawn ? TEXT("true") : TEXT("false"));
	}
}

void UGA_MedicalEquip::RestorePreviousWeaponState()
{
	RequestStanceChange(false);
	MEDICAL_LOG(Log, TEXT("Requested restore to previous weapon: %s (valid=%s)"),
		*PreviousWeaponType.ToString(),
		PreviousWeaponType.IsValid() ? TEXT("true") : TEXT("false"));
}

void UGA_MedicalEquip::BroadcastEquipEvent(FGameplayTag EventTag)
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		MEDICAL_LOG(Warning, TEXT("BroadcastEquipEvent: No AvatarActor - event %s will have null Source"),
			*EventTag.ToString());
	}

	if (USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this))
	{
		if (USuspenseCoreEventBus* EventBus = EventManager->GetEventBus())
		{
			FSuspenseCoreEventData EventData;
			EventData.Source = AvatarActor;
			EventData.Timestamp = FPlatformTime::Seconds();
			EventData.StringPayload.Add(TEXT("MedicalItemID"), MedicalItemID.ToString());
			EventData.StringPayload.Add(TEXT("MedicalType"), MedicalTypeTag.ToString());
			EventData.IntPayload.Add(TEXT("QuickSlotIndex"), SourceQuickSlotIndex);
			EventData.BoolPayload.Add(TEXT("IsReady"), bMedicalReady);

			MEDICAL_LOG(Verbose, TEXT("Publishing event: %s (MedicalItemID=%s, Type=%s, Slot=%d)"),
				*EventTag.ToString(),
				*MedicalItemID.ToString(),
				*MedicalTypeTag.ToString(),
				SourceQuickSlotIndex);

			EventBus->Publish(EventTag, EventData);

			MEDICAL_LOG(Log, TEXT("Broadcast event: %s"), *EventTag.ToString());
		}
		else
		{
			MEDICAL_LOG(Error, TEXT("BroadcastEquipEvent: No EventBus available"));
		}
	}
	else
	{
		MEDICAL_LOG(Error, TEXT("BroadcastEquipEvent: No EventManager available"));
	}
}
