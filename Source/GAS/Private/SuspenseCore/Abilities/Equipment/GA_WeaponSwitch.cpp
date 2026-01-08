// GA_WeaponSwitch.cpp
// Weapon slot switching ability implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Equipment/GA_WeaponSwitch.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeaponSwitch, Log, All);

//==================================================================
// UGA_WeaponSwitch - Base Class
//==================================================================

UGA_WeaponSwitch::UGA_WeaponSwitch()
{
	TargetSlotIndex = 0;

	// Instanced per actor - required for UE5.5+
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Blocking tags - cannot switch while in these states
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Stunned")));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Disabled")));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Reloading")));

	// Enable EventBus integration
	bPublishAbilityEvents = true;
}

bool UGA_WeaponSwitch::CanActivateAbility(
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

	ISuspenseCoreEquipmentDataProvider* Provider =
		const_cast<UGA_WeaponSwitch*>(this)->GetEquipmentDataProvider();

	if (!Provider)
	{
		UE_LOG(LogWeaponSwitch, Warning, TEXT("CanActivateAbility: No EquipmentDataProvider found"));
		return false;
	}

	// Check slot has weapon
	if (!Provider->IsSlotOccupied(TargetSlotIndex))
	{
		UE_LOG(LogWeaponSwitch, Verbose, TEXT("CanActivateAbility: Slot %d is empty"), TargetSlotIndex);
		return false;
	}

	// Check not already active
	if (Provider->GetActiveWeaponSlot() == TargetSlotIndex)
	{
		UE_LOG(LogWeaponSwitch, Verbose, TEXT("CanActivateAbility: Slot %d already active"), TargetSlotIndex);
		return false;
	}

	return true;
}

void UGA_WeaponSwitch::ActivateAbility(
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

	ISuspenseCoreEquipmentDataProvider* Provider = GetEquipmentDataProvider();
	if (Provider)
	{
		const int32 PreviousSlot = Provider->GetActiveWeaponSlot();
		const bool bSuccess = Provider->SetActiveWeaponSlot(TargetSlotIndex);

		if (bSuccess)
		{
			UE_LOG(LogWeaponSwitch, Log, TEXT("Weapon switched: Slot %d → Slot %d"),
				PreviousSlot, TargetSlotIndex);

			// Publish EventBus event for UI/animation systems
			if (USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(GetAvatarActorFromActorInfo()))
			{
				if (USuspenseCoreEventBus* EventBus = EventManager->GetEventBus())
				{
					FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetAvatarActorFromActorInfo());
					EventData.SetInt(FName("PreviousSlot"), PreviousSlot);
					EventData.SetInt(FName("NewSlot"), TargetSlotIndex);

					EventBus->Publish(
						SuspenseCoreTags::Event::Equipment::WeaponSlotSwitched,
						EventData);
				}
			}
		}
		else
		{
			UE_LOG(LogWeaponSwitch, Warning, TEXT("Weapon switch to slot %d failed"), TargetSlotIndex);
		}
	}

	// Instant ability - end immediately
	// Future: Add animation montage wait here
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

ISuspenseCoreEquipmentDataProvider* UGA_WeaponSwitch::GetEquipmentDataProvider() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		return nullptr;
	}

	// DataStore is on PlayerState for persistence across respawns
	if (APawn* Pawn = Cast<APawn>(AvatarActor))
	{
		if (APlayerState* PS = Pawn->GetPlayerState())
		{
			TArray<UActorComponent*> Components;
			PS->GetComponents(Components);

			for (UActorComponent* Comp : Components)
			{
				if (Comp && Comp->GetClass()->ImplementsInterface(USuspenseCoreEquipmentDataProvider::StaticClass()))
				{
					return Cast<ISuspenseCoreEquipmentDataProvider>(Comp);
				}
			}
		}
	}

	// Fallback: check AvatarActor components directly
	TArray<UActorComponent*> Components;
	AvatarActor->GetComponents(Components);

	for (UActorComponent* Comp : Components)
	{
		if (Comp && Comp->GetClass()->ImplementsInterface(USuspenseCoreEquipmentDataProvider::StaticClass()))
		{
			return Cast<ISuspenseCoreEquipmentDataProvider>(Comp);
		}
	}

	return nullptr;
}

//==================================================================
// Concrete Weapon Slot Abilities
//==================================================================

UGA_WeaponSwitch_Primary::UGA_WeaponSwitch_Primary()
{
	TargetSlotIndex = 0;
	AbilityInputID = ESuspenseCoreAbilityInputID::WeaponSlot1;

	// Set ability tag for input binding via TryActivateAbilitiesByTag
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(SuspenseCoreTags::Ability::WeaponSlot::Primary);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

UGA_WeaponSwitch_Secondary::UGA_WeaponSwitch_Secondary()
{
	TargetSlotIndex = 1;
	AbilityInputID = ESuspenseCoreAbilityInputID::WeaponSlot2;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(SuspenseCoreTags::Ability::WeaponSlot::Secondary);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

UGA_WeaponSwitch_Sidearm::UGA_WeaponSwitch_Sidearm()
{
	TargetSlotIndex = 2;
	AbilityInputID = ESuspenseCoreAbilityInputID::WeaponSlot3;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(SuspenseCoreTags::Ability::WeaponSlot::Sidearm);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

UGA_WeaponSwitch_Melee::UGA_WeaponSwitch_Melee()
{
	TargetSlotIndex = 3;
	AbilityInputID = ESuspenseCoreAbilityInputID::MeleeWeapon;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(SuspenseCoreTags::Ability::WeaponSlot::Melee);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
