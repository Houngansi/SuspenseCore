// SuspenseCoreGameplayTags.h
// SuspenseCore - Centralized GameplayTags Registry
// Copyright Suspense Team. All Rights Reserved.
//
// USAGE:
//   #include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
//   EventBus->Publish(SuspenseCoreTags::Event::Player::Spawned, EventData);
//
// DO NOT use FGameplayTag::RequestGameplayTag() directly for common tags.
// All tags are pre-registered at module startup for maximum performance.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

/**
 * SuspenseCore Native GameplayTags Registry
 *
 * All frequently-used tags should be declared here to:
 * 1. Prevent typos in tag strings
 * 2. Enable compile-time validation
 * 3. Improve runtime performance (no string lookups)
 * 4. Provide IntelliSense autocomplete
 *
 * Pattern: UE_DECLARE_GAMEPLAY_TAG_EXTERN in header
 *          UE_DEFINE_GAMEPLAY_TAG in cpp
 */
namespace SuspenseCoreTags
{
	//==================================================================
	// EVENT TAGS - EventBus Publishing
	//==================================================================
	namespace Event
	{
		//--------------------------------------------------------------
		// Player Events
		//--------------------------------------------------------------
		namespace Player
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Spawned);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Died);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Respawned);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Initialized);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Left);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Possessed);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UnPossessed);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(LevelChanged);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TeamChanged);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ClassChanged);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ControllerReady);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ControllerPossessed);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ControllerUnPossessed);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ComponentReady);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(IdentifierChanged);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SprintStarted);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SprintStopped);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(WeaponStateChanged);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(WeaponChanged);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(MovementStateChanged);
		}

		//--------------------------------------------------------------
		// GAS Events
		//--------------------------------------------------------------
		namespace GAS
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Initialized);

			namespace Attribute
			{
				BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Changed);
				BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Health);
				BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stamina);
			}
		}

		//--------------------------------------------------------------
		// Weapon Events
		//--------------------------------------------------------------
		namespace Weapon
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fired);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Equipped);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Unequipped);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Reloaded);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AmmoChanged);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(FireModeChanged);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ReloadStarted);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ReloadCompleted);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SpreadChanged);
		}

		//--------------------------------------------------------------
		// Interaction Events
		//--------------------------------------------------------------
		namespace Interaction
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Started);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Completed);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cancelled);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(FocusChanged);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ValidationFailed);
		}

		//--------------------------------------------------------------
		// Inventory Events
		//--------------------------------------------------------------
		namespace Inventory
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ItemAdded);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ItemRemoved);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ItemMoved);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Updated);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(OperationFailed);
		}

		//--------------------------------------------------------------
		// Equipment Events
		//--------------------------------------------------------------
		namespace Equipment
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Initialized);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SlotChanged);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(LoadoutChanged);
		}

		//--------------------------------------------------------------
		// Pickup/Factory Events
		//--------------------------------------------------------------
		namespace Pickup
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Spawned);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Collected);
		}

		namespace Factory
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ItemCreated);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SpawnFailed);
		}

		//--------------------------------------------------------------
		// Camera Events
		//--------------------------------------------------------------
		namespace Camera
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(FOVChanged);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(PresetApplied);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Reset);
		}

		//--------------------------------------------------------------
		// Security Events
		//--------------------------------------------------------------
		namespace Security
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ViolationDetected);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(PlayerKicked);
		}

		//--------------------------------------------------------------
		// Service Lifecycle Events
		//--------------------------------------------------------------
		namespace Service
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ready);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Initialized);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Registered);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Failed);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shutdown);
		}

		//--------------------------------------------------------------
		// Settings Events
		//--------------------------------------------------------------
		namespace Settings
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InteractionChanged);
		}

		//--------------------------------------------------------------
		// Save System Events
		//--------------------------------------------------------------
		namespace Save
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(QuickSave);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(QuickLoad);
		}

		//--------------------------------------------------------------
		// Input Events
		//--------------------------------------------------------------
		namespace Input
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AbilityActivated);
		}

		//--------------------------------------------------------------
		// Class System Events
		//--------------------------------------------------------------
		namespace Class
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Loaded);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Applied);
		}

		//--------------------------------------------------------------
		// UI Panel Events
		//--------------------------------------------------------------
		namespace UIPanel
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Inventory);
		}

		//--------------------------------------------------------------
		// Ability Events
		//--------------------------------------------------------------
		namespace Ability
		{
			namespace CharacterJump
			{
				BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Landed);
			}

			namespace Interact
			{
				BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Success);
				BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Failed);
			}
		}
	}

	//==================================================================
	// STATE TAGS - Character States (GAS blocking tags)
	//==================================================================
	namespace State
	{
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Dead);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stunned);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Disabled);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Crouching);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sprinting);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interacting);
	}

	//==================================================================
	// ABILITY TAGS - Ability Activation Tags
	//==================================================================
	namespace Ability
	{
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Jump);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sprint);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Crouch);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact);

		namespace Movement
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Jump);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sprint);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Crouch);
		}

		namespace Input
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact);
		}

		namespace Cooldown
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact);
		}
	}

	//==================================================================
	// INTERACTION TYPE TAGS
	//==================================================================
	namespace InteractionType
	{
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pickup);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ammo);
	}

	//==================================================================
	// ITEM TAGS
	//==================================================================
	namespace Item
	{
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Generic);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root);
	}

	//==================================================================
	// DATA TAGS (SetByCaller)
	//==================================================================
	namespace Data
	{
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ItemID);
	}

	//==================================================================
	// EQUIPMENT SLOT TAGS
	//==================================================================
	namespace EquipmentSlot
	{
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(PrimaryWeapon);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SecondaryWeapon);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sidearm);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Melee);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BodyArmor);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Headwear);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TacticalRig);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Backpack);
	}
}

/**
 * Helper class for validating tags at startup
 */
class BRIDGESYSTEM_API FSuspenseCoreTagValidator
{
public:
	/** Validate all critical tags exist - call in StartupModule() */
	static bool ValidateCriticalTags();

	/** Get list of all missing tags */
	static TArray<FString> GetMissingTags();

private:
	/** Critical tags that MUST exist for proper operation */
	static const TArray<FString>& GetCriticalTagNames();
};
