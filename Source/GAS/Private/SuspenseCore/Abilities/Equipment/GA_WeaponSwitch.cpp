// GA_WeaponSwitch.cpp
// Weapon slot switching ability implementation
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE:
// - Uses ISuspenseCoreEquipmentDataProvider interface (BridgeSystem)
// - No direct dependency on EquipmentSystem module
// - EventBus integration for UI/Animation notifications
// - Native tags for all blocking/ability tags (no RequestGameplayTag)
//
// PIPELINE:
// 1. Input Key → GAS Activation via TryActivateAbilitiesByTag
// 2. CanActivate: Check slot occupied && not already active && no blocking tags
// 3. Activate: SetActiveWeaponSlot() → EventBus publish
// 4. End (instant, future: montage wait)

#include "SuspenseCore/Abilities/Equipment/GA_WeaponSwitch.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeaponSwitch, Log, All);

//==================================================================
// NOTE: AbilityTags are set directly in constructors via GetAbilityTagsMutable()
// This is the correct approach for UE5.5+ as SetAssetTags() is protected
//==================================================================

//==================================================================
// UGA_WeaponSwitch - Base Class
//==================================================================

UGA_WeaponSwitch::UGA_WeaponSwitch()
{
	TargetSlotIndex = 0;

	// Instanced per actor - required for UE5.5+
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Blocking tags - cannot switch while in these states
	// CRITICAL: Use native tags from SuspenseCoreTags::State namespace
	// Never use RequestGameplayTag() for frequently-used tags
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Dead);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Stunned);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Disabled);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Reloading);

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
	UE_LOG(LogWeaponSwitch, Warning, TEXT("=== CanActivateAbility START: TargetSlot=%d ==="), TargetSlotIndex);

	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		UE_LOG(LogWeaponSwitch, Warning, TEXT("  FAIL: Super::CanActivateAbility returned false (blocking tags?)"));
		return false;
	}

	ISuspenseCoreEquipmentDataProvider* Provider =
		const_cast<UGA_WeaponSwitch*>(this)->GetEquipmentDataProvider();

	if (!Provider)
	{
		UE_LOG(LogWeaponSwitch, Warning, TEXT("  FAIL: No EquipmentDataProvider found on PlayerState!"));
		return false;
	}

	UE_LOG(LogWeaponSwitch, Warning, TEXT("  Provider found. Checking slot %d..."), TargetSlotIndex);

	// Check slot has weapon
	const bool bSlotOccupied = Provider->IsSlotOccupied(TargetSlotIndex);
	UE_LOG(LogWeaponSwitch, Warning, TEXT("  IsSlotOccupied(%d) = %s"), TargetSlotIndex, bSlotOccupied ? TEXT("YES") : TEXT("NO"));

	if (!bSlotOccupied)
	{
		UE_LOG(LogWeaponSwitch, Warning, TEXT("  FAIL: Slot %d is empty"), TargetSlotIndex);
		return false;
	}

	// Check not already active
	const int32 ActiveSlot = Provider->GetActiveWeaponSlot();
	UE_LOG(LogWeaponSwitch, Warning, TEXT("  GetActiveWeaponSlot() = %d"), ActiveSlot);

	if (ActiveSlot == TargetSlotIndex)
	{
		UE_LOG(LogWeaponSwitch, Warning, TEXT("  FAIL: Slot %d already active"), TargetSlotIndex);
		return false;
	}

	UE_LOG(LogWeaponSwitch, Warning, TEXT("  SUCCESS: Can activate for slot %d"), TargetSlotIndex);
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
			// CRITICAL: Use WeaponSlotSwitched from BridgeSystem (SuspenseCoreTags)
			// GAS cannot depend on EquipmentSystem - DI principle
			if (USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(GetAvatarActorFromActorInfo()))
			{
				if (USuspenseCoreEventBus* EventBus = EventManager->GetEventBus())
				{
					AActor* AvatarActor = GetAvatarActorFromActorInfo();
					FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(AvatarActor);
					EventData.SetObject(FName("Target"), AvatarActor);
					EventData.SetInt(FName("PreviousSlot"), PreviousSlot);
					EventData.SetInt(FName("NewSlot"), TargetSlotIndex);

					// NOTE: WeaponActor is not included here because DataProvider interface
					// doesn't expose weapon actors (it manages item data, not visual actors).
					// AmmoCounterWidget has fallback to query DataProvider directly.

					// Use BridgeSystem tag - GAS cannot depend on EquipmentSystem (DI)
					EventBus->Publish(
						SuspenseCoreTags::Event::Equipment::WeaponSlotSwitched,
						EventData);

					UE_LOG(LogWeaponSwitch, Log, TEXT("  Published WeaponSlotSwitched event to EventBus"));
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
		UE_LOG(LogWeaponSwitch, Warning, TEXT("GetEquipmentDataProvider: No AvatarActor!"));
		return nullptr;
	}

	// DataStore is on PlayerState for persistence across respawns
	if (APawn* Pawn = Cast<APawn>(AvatarActor))
	{
		if (APlayerState* PS = Pawn->GetPlayerState())
		{
			TArray<UActorComponent*> Components;
			PS->GetComponents(Components);

			UE_LOG(LogWeaponSwitch, Verbose, TEXT("GetEquipmentDataProvider: Checking %d components on PlayerState %s"),
				Components.Num(), *PS->GetName());

			for (UActorComponent* Comp : Components)
			{
				if (Comp && Comp->GetClass()->ImplementsInterface(USuspenseCoreEquipmentDataProvider::StaticClass()))
				{
					UE_LOG(LogWeaponSwitch, Verbose, TEXT("GetEquipmentDataProvider: Found provider component: %s"),
						*Comp->GetName());
					return Cast<ISuspenseCoreEquipmentDataProvider>(Comp);
				}
			}

			UE_LOG(LogWeaponSwitch, Warning, TEXT("GetEquipmentDataProvider: No ISuspenseCoreEquipmentDataProvider on PlayerState %s (%d components checked)"),
				*PS->GetName(), Components.Num());
		}
		else
		{
			UE_LOG(LogWeaponSwitch, Warning, TEXT("GetEquipmentDataProvider: Pawn has no PlayerState!"));
		}
	}
	else
	{
		UE_LOG(LogWeaponSwitch, Warning, TEXT("GetEquipmentDataProvider: AvatarActor %s is not a Pawn!"), *AvatarActor->GetName());
	}

	// Fallback: check AvatarActor components directly
	TArray<UActorComponent*> Components;
	AvatarActor->GetComponents(Components);

	for (UActorComponent* Comp : Components)
	{
		if (Comp && Comp->GetClass()->ImplementsInterface(USuspenseCoreEquipmentDataProvider::StaticClass()))
		{
			UE_LOG(LogWeaponSwitch, Verbose, TEXT("GetEquipmentDataProvider: Found provider on AvatarActor: %s"), *Comp->GetName());
			return Cast<ISuspenseCoreEquipmentDataProvider>(Comp);
		}
	}

	UE_LOG(LogWeaponSwitch, Warning, TEXT("GetEquipmentDataProvider: No provider found anywhere!"));
	return nullptr;
}

//==================================================================
// Concrete Weapon Slot Abilities
//
// Each ability targets a specific weapon slot (0-3):
// - Primary (Key 1)   → Slot 0 (PrimaryWeapon)
// - Secondary (Key 2) → Slot 1 (SecondaryWeapon)
// - Sidearm (Key 3)   → Slot 2 (Holster/Pistol)
// - Melee (Key V)     → Slot 3 (Scabbard/Knife)
//
// Ability tags enable activation via TryActivateAbilitiesByTag
//==================================================================

UGA_WeaponSwitch_Primary::UGA_WeaponSwitch_Primary()
{
	TargetSlotIndex = 0;
	AbilityInputID = ESuspenseCoreAbilityInputID::WeaponSlot1;

	// Set ability tag for input binding via TryActivateAbilitiesByTag
	// UE5.5+: Use SetAssetTags() in constructor instead of deprecated AbilityTags
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(SuspenseCoreTags::Ability::WeaponSlot::Primary);
	SetAssetTags(AssetTags);
}

UGA_WeaponSwitch_Secondary::UGA_WeaponSwitch_Secondary()
{
	TargetSlotIndex = 1;
	AbilityInputID = ESuspenseCoreAbilityInputID::WeaponSlot2;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(SuspenseCoreTags::Ability::WeaponSlot::Secondary);
	SetAssetTags(AssetTags);
}

UGA_WeaponSwitch_Sidearm::UGA_WeaponSwitch_Sidearm()
{
	TargetSlotIndex = 2;
	AbilityInputID = ESuspenseCoreAbilityInputID::WeaponSlot3;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(SuspenseCoreTags::Ability::WeaponSlot::Sidearm);
	SetAssetTags(AssetTags);
}

UGA_WeaponSwitch_Melee::UGA_WeaponSwitch_Melee()
{
	TargetSlotIndex = 3;
	AbilityInputID = ESuspenseCoreAbilityInputID::MeleeWeapon;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(SuspenseCoreTags::Ability::WeaponSlot::Melee);
	SetAssetTags(AssetTags);
}
