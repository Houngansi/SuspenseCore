// SuspenseCoreGrenadeEquipAbility.cpp
// Ability for equipping grenades (Tarkov-style flow)
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Throwable/SuspenseCoreGrenadeEquipAbility.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogGrenadeEquip, Log, All);

#define EQUIP_LOG(Verbosity, Format, ...) \
	UE_LOG(LogGrenadeEquip, Verbosity, TEXT("[GrenadeEquip] " Format), ##__VA_ARGS__)

//==================================================================
// Constructor
//==================================================================

USuspenseCoreGrenadeEquipAbility::USuspenseCoreGrenadeEquipAbility()
{
	// Instanced per execution - we need state
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;

	// Ability tags - use SetAssetTags for new API
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(SuspenseCoreTags::Ability::Throwable::Equip);
	SetAssetTags(AssetTags);

	// Blocking tags - can't equip while these states are active
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Dead);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Stunned);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Disabled);
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.GrenadeEquipped")));

	// This ability grants the equipped state
	// Will be removed when ability ends

	// Cancel these abilities when we activate
	CancelAbilitiesWithTag.AddTag(SuspenseCoreTags::Ability::Weapon::Fire);
	CancelAbilitiesWithTag.AddTag(SuspenseCoreTags::Ability::Weapon::Reload);
	CancelAbilitiesWithTag.AddTag(SuspenseCoreTags::Ability::Weapon::AimDownSight);

	// Default timing
	MinEquipTime = 0.3f;
	DrawMontagePlayRate = 1.0f;

	// EventBus integration
	bPublishAbilityEvents = true;
}

//==================================================================
// Public Methods
//==================================================================

float USuspenseCoreGrenadeEquipAbility::GetEquipTime() const
{
	if (!bGrenadeReady)
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

void USuspenseCoreGrenadeEquipAbility::SetGrenadeInfo(FName InGrenadeID, FGameplayTag InGrenadeTypeTag, int32 InSlotIndex)
{
	GrenadeID = InGrenadeID;
	GrenadeTypeTag = InGrenadeTypeTag;
	SourceQuickSlotIndex = InSlotIndex;

	EQUIP_LOG(Log, TEXT("SetGrenadeInfo: ID=%s, Type=%s, Slot=%d"),
		*GrenadeID.ToString(),
		*GrenadeTypeTag.ToString(),
		SourceQuickSlotIndex);
}

void USuspenseCoreGrenadeEquipAbility::RequestUnequip()
{
	if (bUnequipRequested)
	{
		return;
	}

	bUnequipRequested = true;
	EQUIP_LOG(Log, TEXT("Unequip requested"));

	OnGrenadeUnequipping();
	PlayHolsterMontage();
}

//==================================================================
// GameplayAbility Interface
//==================================================================

bool USuspenseCoreGrenadeEquipAbility::CanActivateAbility(
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

	// Must have valid grenade info set
	if (GrenadeID.IsNone())
	{
		EQUIP_LOG(Verbose, TEXT("CanActivate: No GrenadeID set"));
		return false;
	}

	return true;
}

void USuspenseCoreGrenadeEquipAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EQUIP_LOG(Warning, TEXT("Failed to commit ability"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	EQUIP_LOG(Log, TEXT("ActivateAbility: Equipping grenade %s"), *GrenadeID.ToString());

	// Reset state
	bGrenadeReady = false;
	bUnequipRequested = false;
	EquipStartTime = 0.0f;

	// Store previous weapon type (via gameplay tag query)
	StorePreviousWeaponState();

	// Request stance change via EventBus (WeaponStanceComponent listens for this)
	// This avoids circular dependency: GAS cannot directly depend on EquipmentSystem
	RequestStanceChange(true);

	// Grant State.GrenadeEquipped tag
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		FGameplayTag EquippedTag = FGameplayTag::RequestGameplayTag(FName("State.GrenadeEquipped"));
		ASC->AddLooseGameplayTag(EquippedTag);

		EQUIP_LOG(Verbose, TEXT("Granted State.GrenadeEquipped tag"));
	}

	// Broadcast equip started event
	BroadcastEquipEvent(SuspenseCoreTags::Event::Throwable::Equipped);

	// Play draw montage
	PlayDrawMontage();
}

void USuspenseCoreGrenadeEquipAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	EQUIP_LOG(Log, TEXT("EndAbility: Cancelled=%s"), bWasCancelled ? TEXT("true") : TEXT("false"));

	// Remove State.GrenadeEquipped tag
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		FGameplayTag EquippedTag = FGameplayTag::RequestGameplayTag(FName("State.GrenadeEquipped"));
		ASC->RemoveLooseGameplayTag(EquippedTag);

		EQUIP_LOG(Verbose, TEXT("Removed State.GrenadeEquipped tag"));
	}

	// Restore previous weapon state if cancelled (not if throw was executed)
	if (bWasCancelled && !bUnequipRequested)
	{
		RestorePreviousWeaponState();
	}

	// Broadcast unequip event
	BroadcastEquipEvent(SuspenseCoreTags::Event::Throwable::Unequipped);

	// Clean up montage task
	if (ActiveMontageTask)
	{
		ActiveMontageTask->EndTask();
		ActiveMontageTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void USuspenseCoreGrenadeEquipAbility::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	// Equip ability doesn't end on input release
	// It stays active until:
	// 1. Throw is executed (GA_GrenadeThrow ends this)
	// 2. Cancel is requested (weapon switch, etc.)
	// 3. Player presses QuickSlot again to unequip

	EQUIP_LOG(Verbose, TEXT("InputReleased - ability remains active"));
}

//==================================================================
// Internal Methods
//==================================================================

void USuspenseCoreGrenadeEquipAbility::RequestStanceChange(bool bEquipping)
{
	// Broadcast stance change request via EventBus
	// WeaponStanceComponent (in EquipmentSystem) listens for this event
	// This decouples GAS from EquipmentSystem (avoids circular dependency)

	if (USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this))
	{
		if (USuspenseCoreEventBus* EventBus = EventManager->GetEventBus())
		{
			FSuspenseCoreEventData EventData;
			EventData.Source = GetAvatarActorFromActorInfo();
			EventData.Timestamp = FPlatformTime::Seconds();

			// Grenade type tag for animation selection
			FGameplayTag StanceTag = GrenadeTypeTag.IsValid() ?
				GrenadeTypeTag :
				FGameplayTag::RequestGameplayTag(FName("Weapon.Grenade.Frag"));

			EventData.StringPayload.Add(TEXT("WeaponType"), StanceTag.ToString());
			EventData.BoolPayload.Add(TEXT("IsDrawn"), bEquipping);
			EventData.BoolPayload.Add(TEXT("IsGrenade"), true);

			// Use a generic weapon stance change event
			FGameplayTag EventTag = bEquipping ?
				FGameplayTag::RequestGameplayTag(FName("Event.Weapon.StanceChangeRequested")) :
				FGameplayTag::RequestGameplayTag(FName("Event.Weapon.StanceRestoreRequested"));

			EventBus->Publish(EventTag, EventData);

			EQUIP_LOG(Log, TEXT("Requested stance change: %s, Equipping=%s"),
				*StanceTag.ToString(), bEquipping ? TEXT("true") : TEXT("false"));
		}
	}
}

void USuspenseCoreGrenadeEquipAbility::PlayDrawMontage()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		EQUIP_LOG(Warning, TEXT("PlayDrawMontage: No character"));
		OnDrawMontageCompleted();
		return;
	}

	// Use default draw montage
	// Animation data lookup can be added via EventBus request if needed
	UAnimMontage* DrawMontage = DefaultDrawMontage;

	if (!DrawMontage)
	{
		EQUIP_LOG(Log, TEXT("No draw montage available, completing immediately"));
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
		ActiveMontageTask->OnCompleted.AddDynamic(this, &USuspenseCoreGrenadeEquipAbility::OnDrawMontageCompleted);
		ActiveMontageTask->OnBlendOut.AddDynamic(this, &USuspenseCoreGrenadeEquipAbility::OnDrawMontageCompleted);
		ActiveMontageTask->OnInterrupted.AddDynamic(this, &USuspenseCoreGrenadeEquipAbility::OnDrawMontageInterrupted);
		ActiveMontageTask->OnCancelled.AddDynamic(this, &USuspenseCoreGrenadeEquipAbility::OnDrawMontageInterrupted);

		ActiveMontageTask->ReadyForActivation();

		EQUIP_LOG(Log, TEXT("Playing draw montage: %s"), *DrawMontage->GetName());
	}
	else
	{
		EQUIP_LOG(Warning, TEXT("Failed to create montage task"));
		OnDrawMontageCompleted();
	}
}

void USuspenseCoreGrenadeEquipAbility::OnDrawMontageCompleted()
{
	EQUIP_LOG(Log, TEXT("Draw montage completed - grenade ready"));

	bGrenadeReady = true;

	UWorld* World = GetWorld();
	if (World)
	{
		EquipStartTime = World->GetTimeSeconds();
	}

	// Notify blueprint
	OnGrenadeEquipped();

	// Broadcast ready event
	BroadcastEquipEvent(FGameplayTag::RequestGameplayTag(FName("Event.Throwable.Ready")));

	// Clear montage task reference
	ActiveMontageTask = nullptr;
}

void USuspenseCoreGrenadeEquipAbility::OnDrawMontageInterrupted()
{
	EQUIP_LOG(Log, TEXT("Draw montage interrupted"));

	// Cancel the ability
	CancelAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true);
}

void USuspenseCoreGrenadeEquipAbility::PlayHolsterMontage()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		OnHolsterMontageCompleted();
		return;
	}

	UAnimMontage* HolsterMontage = DefaultHolsterMontage;

	// TODO: Get from animation data

	if (!HolsterMontage)
	{
		EQUIP_LOG(Log, TEXT("No holster montage, completing immediately"));
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
		ActiveMontageTask->OnCompleted.AddDynamic(this, &USuspenseCoreGrenadeEquipAbility::OnHolsterMontageCompleted);
		ActiveMontageTask->OnBlendOut.AddDynamic(this, &USuspenseCoreGrenadeEquipAbility::OnHolsterMontageCompleted);
		ActiveMontageTask->OnInterrupted.AddDynamic(this, &USuspenseCoreGrenadeEquipAbility::OnHolsterMontageCompleted);
		ActiveMontageTask->OnCancelled.AddDynamic(this, &USuspenseCoreGrenadeEquipAbility::OnHolsterMontageCompleted);

		ActiveMontageTask->ReadyForActivation();

		EQUIP_LOG(Log, TEXT("Playing holster montage"));
	}
	else
	{
		OnHolsterMontageCompleted();
	}
}

void USuspenseCoreGrenadeEquipAbility::OnHolsterMontageCompleted()
{
	EQUIP_LOG(Log, TEXT("Holster montage completed"));

	// Restore previous weapon
	RestorePreviousWeaponState();

	// End the ability normally
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void USuspenseCoreGrenadeEquipAbility::StorePreviousWeaponState()
{
	// Query current weapon type from ASC tags
	// WeaponStanceComponent adds Weapon.Type.* tags when weapon is equipped
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		FGameplayTagContainer OwnedTags;
		ASC->GetOwnedGameplayTags(OwnedTags);

		// Find current weapon type tag
		FGameplayTag WeaponTypeParent = FGameplayTag::RequestGameplayTag(FName("Weapon.Type"));
		for (const FGameplayTag& Tag : OwnedTags)
		{
			if (Tag.MatchesTag(WeaponTypeParent))
			{
				PreviousWeaponType = Tag;
				break;
			}
		}

		// Check if weapon is drawn
		bPreviousWeaponDrawn = OwnedTags.HasTag(FGameplayTag::RequestGameplayTag(FName("State.WeaponDrawn")));

		EQUIP_LOG(Verbose, TEXT("Stored previous weapon: %s (drawn=%s)"),
			*PreviousWeaponType.ToString(),
			bPreviousWeaponDrawn ? TEXT("true") : TEXT("false"));
	}
}

void USuspenseCoreGrenadeEquipAbility::RestorePreviousWeaponState()
{
	// Request stance restoration via EventBus
	if (PreviousWeaponType.IsValid())
	{
		RequestStanceChange(false);
		EQUIP_LOG(Log, TEXT("Requested restore to previous weapon: %s"), *PreviousWeaponType.ToString());
	}
}

void USuspenseCoreGrenadeEquipAbility::BroadcastEquipEvent(FGameplayTag EventTag)
{
	if (USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this))
	{
		if (USuspenseCoreEventBus* EventBus = EventManager->GetEventBus())
		{
			FSuspenseCoreEventData EventData;
			EventData.Source = GetAvatarActorFromActorInfo();
			EventData.Timestamp = FPlatformTime::Seconds();
			EventData.StringPayload.Add(TEXT("GrenadeID"), GrenadeID.ToString());
			EventData.StringPayload.Add(TEXT("GrenadeType"), GrenadeTypeTag.ToString());
			EventData.IntPayload.Add(TEXT("QuickSlotIndex"), SourceQuickSlotIndex);
			EventData.BoolPayload.Add(TEXT("IsReady"), bGrenadeReady);

			EventBus->Publish(EventTag, EventData);

			EQUIP_LOG(Verbose, TEXT("Broadcast event: %s"), *EventTag.ToString());
		}
	}
}
