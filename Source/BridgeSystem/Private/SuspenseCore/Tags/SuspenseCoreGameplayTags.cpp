// SuspenseCoreGameplayTags.cpp
// SuspenseCore - Centralized GameplayTags Registry Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "GameplayTagsManager.h"

namespace SuspenseCoreTags
{
	//==================================================================
	// EVENT TAGS
	//==================================================================
	namespace Event
	{
		//--------------------------------------------------------------
		// Player Events
		//--------------------------------------------------------------
		namespace Player
		{
			UE_DEFINE_GAMEPLAY_TAG(Spawned, "SuspenseCore.Event.Player.Spawned");
			UE_DEFINE_GAMEPLAY_TAG(Died, "SuspenseCore.Event.Player.Died");
			UE_DEFINE_GAMEPLAY_TAG(Respawned, "SuspenseCore.Event.Player.Respawned");
			UE_DEFINE_GAMEPLAY_TAG(Initialized, "SuspenseCore.Event.Player.Initialized");
			UE_DEFINE_GAMEPLAY_TAG(Left, "SuspenseCore.Event.Player.Left");
			UE_DEFINE_GAMEPLAY_TAG(Possessed, "SuspenseCore.Event.Player.Possessed");
			UE_DEFINE_GAMEPLAY_TAG(UnPossessed, "SuspenseCore.Event.Player.UnPossessed");
			UE_DEFINE_GAMEPLAY_TAG(LevelChanged, "SuspenseCore.Event.Player.LevelChanged");
			UE_DEFINE_GAMEPLAY_TAG(TeamChanged, "SuspenseCore.Event.Player.TeamChanged");
			UE_DEFINE_GAMEPLAY_TAG(ClassChanged, "SuspenseCore.Event.Player.ClassChanged");
			UE_DEFINE_GAMEPLAY_TAG(ControllerReady, "SuspenseCore.Event.Player.ControllerReady");
			UE_DEFINE_GAMEPLAY_TAG(ControllerPossessed, "SuspenseCore.Event.Player.ControllerPossessed");
			UE_DEFINE_GAMEPLAY_TAG(ControllerUnPossessed, "SuspenseCore.Event.Player.ControllerUnPossessed");
			UE_DEFINE_GAMEPLAY_TAG(ComponentReady, "SuspenseCore.Event.Player.ComponentReady");
			UE_DEFINE_GAMEPLAY_TAG(IdentifierChanged, "SuspenseCore.Event.Player.IdentifierChanged");
			UE_DEFINE_GAMEPLAY_TAG(SprintStarted, "SuspenseCore.Event.Player.SprintStarted");
			UE_DEFINE_GAMEPLAY_TAG(SprintStopped, "SuspenseCore.Event.Player.SprintStopped");
			UE_DEFINE_GAMEPLAY_TAG(WeaponStateChanged, "SuspenseCore.Event.Player.WeaponStateChanged");
			UE_DEFINE_GAMEPLAY_TAG(WeaponChanged, "SuspenseCore.Event.Player.WeaponChanged");
			UE_DEFINE_GAMEPLAY_TAG(MovementStateChanged, "SuspenseCore.Event.Player.MovementStateChanged");
		}

		//--------------------------------------------------------------
		// GAS Events
		//--------------------------------------------------------------
		namespace GAS
		{
			UE_DEFINE_GAMEPLAY_TAG(Initialized, "SuspenseCore.Event.GAS.Initialized");

			namespace Attribute
			{
				UE_DEFINE_GAMEPLAY_TAG(Changed, "SuspenseCore.Event.GAS.Attribute.Changed");
				UE_DEFINE_GAMEPLAY_TAG(Health, "SuspenseCore.Event.GAS.Attribute.Health");
				UE_DEFINE_GAMEPLAY_TAG(Stamina, "SuspenseCore.Event.GAS.Attribute.Stamina");
			}
		}

		//--------------------------------------------------------------
		// Weapon Events
		//--------------------------------------------------------------
		namespace Weapon
		{
			UE_DEFINE_GAMEPLAY_TAG(Fired, "SuspenseCore.Event.Weapon.Fired");
			UE_DEFINE_GAMEPLAY_TAG(Equipped, "SuspenseCore.Event.Weapon.Equipped");
			UE_DEFINE_GAMEPLAY_TAG(Unequipped, "SuspenseCore.Event.Weapon.Unequipped");
			UE_DEFINE_GAMEPLAY_TAG(Reloaded, "SuspenseCore.Event.Weapon.Reloaded");
			UE_DEFINE_GAMEPLAY_TAG(AmmoChanged, "SuspenseCore.Event.Weapon.AmmoChanged");
			UE_DEFINE_GAMEPLAY_TAG(FireModeChanged, "SuspenseCore.Event.Weapon.FireModeChanged");
			UE_DEFINE_GAMEPLAY_TAG(ReloadStarted, "SuspenseCore.Event.Weapon.ReloadStarted");
			UE_DEFINE_GAMEPLAY_TAG(ReloadCompleted, "SuspenseCore.Event.Weapon.ReloadCompleted");
			UE_DEFINE_GAMEPLAY_TAG(SpreadChanged, "SuspenseCore.Event.Weapon.SpreadChanged");
			UE_DEFINE_GAMEPLAY_TAG(RecoilImpulse, "SuspenseCore.Event.Weapon.RecoilImpulse");
		}

		//--------------------------------------------------------------
		// Interaction Events
		//--------------------------------------------------------------
		namespace Interaction
		{
			UE_DEFINE_GAMEPLAY_TAG(Started, "SuspenseCore.Event.Interaction.Started");
			UE_DEFINE_GAMEPLAY_TAG(Completed, "SuspenseCore.Event.Interaction.Completed");
			UE_DEFINE_GAMEPLAY_TAG(Cancelled, "SuspenseCore.Event.Interaction.Cancelled");
			UE_DEFINE_GAMEPLAY_TAG(FocusChanged, "SuspenseCore.Event.Interaction.FocusChanged");
			UE_DEFINE_GAMEPLAY_TAG(ValidationFailed, "SuspenseCore.Event.Interaction.ValidationFailed");
		}

		//--------------------------------------------------------------
		// Inventory Events
		//--------------------------------------------------------------
		namespace Inventory
		{
			UE_DEFINE_GAMEPLAY_TAG(ItemAdded, "SuspenseCore.Event.Inventory.ItemAdded");
			UE_DEFINE_GAMEPLAY_TAG(ItemRemoved, "SuspenseCore.Event.Inventory.ItemRemoved");
			UE_DEFINE_GAMEPLAY_TAG(ItemMoved, "SuspenseCore.Event.Inventory.ItemMoved");
			UE_DEFINE_GAMEPLAY_TAG(Updated, "SuspenseCore.Event.Inventory.Updated");
			UE_DEFINE_GAMEPLAY_TAG(OperationFailed, "SuspenseCore.Event.Inventory.OperationFailed");
		}

		//--------------------------------------------------------------
		// Equipment Events
		//--------------------------------------------------------------
		namespace Equipment
		{
			UE_DEFINE_GAMEPLAY_TAG(Initialized, "SuspenseCore.Event.Equipment.Initialized");
			UE_DEFINE_GAMEPLAY_TAG(SlotChanged, "SuspenseCore.Event.Equipment.SlotChanged");
			UE_DEFINE_GAMEPLAY_TAG(LoadoutChanged, "SuspenseCore.Event.Equipment.LoadoutChanged");
			UE_DEFINE_GAMEPLAY_TAG(WeaponSlotSwitched, "SuspenseCore.Event.Equipment.WeaponSlotSwitched");
		}

		//--------------------------------------------------------------
		// Pickup/Factory Events
		//--------------------------------------------------------------
		namespace Pickup
		{
			UE_DEFINE_GAMEPLAY_TAG(Spawned, "SuspenseCore.Event.Pickup.Spawned");
			UE_DEFINE_GAMEPLAY_TAG(Collected, "SuspenseCore.Event.Pickup.Collected");
		}

		namespace Factory
		{
			UE_DEFINE_GAMEPLAY_TAG(ItemCreated, "SuspenseCore.Event.Factory.ItemCreated");
			UE_DEFINE_GAMEPLAY_TAG(SpawnFailed, "SuspenseCore.Event.Factory.SpawnFailed");
		}

		//--------------------------------------------------------------
		// Camera Events
		//--------------------------------------------------------------
		namespace Camera
		{
			UE_DEFINE_GAMEPLAY_TAG(FOVChanged, "SuspenseCore.Event.Camera.FOVChanged");
			UE_DEFINE_GAMEPLAY_TAG(PresetApplied, "SuspenseCore.Event.Camera.PresetApplied");
			UE_DEFINE_GAMEPLAY_TAG(Reset, "SuspenseCore.Event.Camera.Reset");
			UE_DEFINE_GAMEPLAY_TAG(ADSEnter, "SuspenseCore.Event.Camera.ADSEnter");
			UE_DEFINE_GAMEPLAY_TAG(ADSExit, "SuspenseCore.Event.Camera.ADSExit");

			// Camera Shake Events
			UE_DEFINE_GAMEPLAY_TAG(ShakeWeapon, "SuspenseCore.Event.Camera.ShakeWeapon");
			UE_DEFINE_GAMEPLAY_TAG(ShakeMovement, "SuspenseCore.Event.Camera.ShakeMovement");
			UE_DEFINE_GAMEPLAY_TAG(ShakeDamage, "SuspenseCore.Event.Camera.ShakeDamage");
			UE_DEFINE_GAMEPLAY_TAG(ShakeExplosion, "SuspenseCore.Event.Camera.ShakeExplosion");
		}

		//--------------------------------------------------------------
		// Security Events
		//--------------------------------------------------------------
		namespace Security
		{
			UE_DEFINE_GAMEPLAY_TAG(ViolationDetected, "SuspenseCore.Event.Security.ViolationDetected");
			UE_DEFINE_GAMEPLAY_TAG(PlayerKicked, "SuspenseCore.Event.Security.PlayerKicked");
		}

		//--------------------------------------------------------------
		// Service Lifecycle Events
		//--------------------------------------------------------------
		namespace Service
		{
			UE_DEFINE_GAMEPLAY_TAG(Ready, "SuspenseCore.Service.Ready");
			UE_DEFINE_GAMEPLAY_TAG(Initialized, "SuspenseCore.Service.Initialized");
			UE_DEFINE_GAMEPLAY_TAG(Registered, "SuspenseCore.Service.Registered");
			UE_DEFINE_GAMEPLAY_TAG(Failed, "SuspenseCore.Service.Failed");
			UE_DEFINE_GAMEPLAY_TAG(Shutdown, "SuspenseCore.Service.Shutdown");
		}

		//--------------------------------------------------------------
		// Settings Events
		//--------------------------------------------------------------
		namespace Settings
		{
			UE_DEFINE_GAMEPLAY_TAG(InteractionChanged, "SuspenseCore.Event.Settings.InteractionChanged");
		}

		//--------------------------------------------------------------
		// Save System Events
		//--------------------------------------------------------------
		namespace Save
		{
			UE_DEFINE_GAMEPLAY_TAG(QuickSave, "SuspenseCore.Event.Save.QuickSave");
			UE_DEFINE_GAMEPLAY_TAG(QuickLoad, "SuspenseCore.Event.Save.QuickLoad");
		}

		//--------------------------------------------------------------
		// Input Events
		//--------------------------------------------------------------
		namespace Input
		{
			UE_DEFINE_GAMEPLAY_TAG(AbilityActivated, "SuspenseCore.Event.Input.AbilityActivated");
		}

		//--------------------------------------------------------------
		// Class System Events
		//--------------------------------------------------------------
		namespace Class
		{
			UE_DEFINE_GAMEPLAY_TAG(Loaded, "SuspenseCore.Event.Class.Loaded");
			UE_DEFINE_GAMEPLAY_TAG(Applied, "SuspenseCore.Event.CharacterClass.Applied");
		}

		//--------------------------------------------------------------
		// UI Panel Events
		//--------------------------------------------------------------
		namespace UIPanel
		{
			UE_DEFINE_GAMEPLAY_TAG(Inventory, "SuspenseCore.Event.UIPanel.Inventory");
		}

		//--------------------------------------------------------------
		// Ability Events
		//--------------------------------------------------------------
		namespace Ability
		{
			namespace CharacterJump
			{
				UE_DEFINE_GAMEPLAY_TAG(Landed, "SuspenseCore.Event.Ability.CharacterJump.Landed");
			}

			namespace Interact
			{
				UE_DEFINE_GAMEPLAY_TAG(Success, "SuspenseCore.Event.Ability.Interact.Success");
				UE_DEFINE_GAMEPLAY_TAG(Failed, "SuspenseCore.Event.Ability.Interact.Failed");
			}
		}
	}

	//==================================================================
	// STATE TAGS
	//==================================================================
	namespace State
	{
		UE_DEFINE_GAMEPLAY_TAG(Dead, "State.Dead");
		UE_DEFINE_GAMEPLAY_TAG(Stunned, "State.Stunned");
		UE_DEFINE_GAMEPLAY_TAG(Disabled, "State.Disabled");
		UE_DEFINE_GAMEPLAY_TAG(Crouching, "State.Crouching");
		UE_DEFINE_GAMEPLAY_TAG(Jumping, "State.Jumping");
		UE_DEFINE_GAMEPLAY_TAG(Sprinting, "State.Sprinting");
		UE_DEFINE_GAMEPLAY_TAG(Interacting, "State.Interacting");
		UE_DEFINE_GAMEPLAY_TAG(Aiming, "State.Aiming");
		UE_DEFINE_GAMEPLAY_TAG(Firing, "State.Firing");
		UE_DEFINE_GAMEPLAY_TAG(Reloading, "State.Reloading");
		UE_DEFINE_GAMEPLAY_TAG(HoldingBreath, "State.HoldingBreath");
		UE_DEFINE_GAMEPLAY_TAG(WeaponBlocked, "State.WeaponBlocked");
		UE_DEFINE_GAMEPLAY_TAG(BurstActive, "State.BurstActive");
		UE_DEFINE_GAMEPLAY_TAG(AutoFireActive, "State.AutoFireActive");
	}

	//==================================================================
	// ABILITY TAGS
	//==================================================================
	namespace Ability
	{
		UE_DEFINE_GAMEPLAY_TAG(Jump, "SuspenseCore.Ability.Jump");
		UE_DEFINE_GAMEPLAY_TAG(Sprint, "SuspenseCore.Ability.Sprint");
		UE_DEFINE_GAMEPLAY_TAG(Crouch, "SuspenseCore.Ability.Crouch");
		UE_DEFINE_GAMEPLAY_TAG(Interact, "SuspenseCore.Ability.Interact");

		namespace Movement
		{
			UE_DEFINE_GAMEPLAY_TAG(Jump, "Ability.Movement.Jump");
			UE_DEFINE_GAMEPLAY_TAG(Sprint, "Ability.Movement.Sprint");
			UE_DEFINE_GAMEPLAY_TAG(Crouch, "Ability.Movement.Crouch");
		}

		namespace Weapon
		{
			UE_DEFINE_GAMEPLAY_TAG(AimDownSight, "SuspenseCore.Ability.Weapon.AimDownSight");
			UE_DEFINE_GAMEPLAY_TAG(Fire, "SuspenseCore.Ability.Weapon.Fire");
			UE_DEFINE_GAMEPLAY_TAG(Reload, "SuspenseCore.Ability.Weapon.Reload");
			UE_DEFINE_GAMEPLAY_TAG(FireModeSwitch, "SuspenseCore.Ability.Weapon.FireModeSwitch");
			UE_DEFINE_GAMEPLAY_TAG(Inspect, "SuspenseCore.Ability.Weapon.Inspect");
			UE_DEFINE_GAMEPLAY_TAG(HoldBreath, "SuspenseCore.Ability.Weapon.HoldBreath");
		}

		// QuickSlot abilities (Tarkov-style magazine/item access via keys 4-7)
		namespace QuickSlot
		{
			UE_DEFINE_GAMEPLAY_TAG(Slot1, "SuspenseCore.Ability.QuickSlot.1");
			UE_DEFINE_GAMEPLAY_TAG(Slot2, "SuspenseCore.Ability.QuickSlot.2");
			UE_DEFINE_GAMEPLAY_TAG(Slot3, "SuspenseCore.Ability.QuickSlot.3");
			UE_DEFINE_GAMEPLAY_TAG(Slot4, "SuspenseCore.Ability.QuickSlot.4");
		}

		// WeaponSlot abilities (direct weapon slot switching via keys 1-3, V)
		namespace WeaponSlot
		{
			UE_DEFINE_GAMEPLAY_TAG(Primary, "SuspenseCore.Ability.WeaponSlot.Primary");
			UE_DEFINE_GAMEPLAY_TAG(Secondary, "SuspenseCore.Ability.WeaponSlot.Secondary");
			UE_DEFINE_GAMEPLAY_TAG(Sidearm, "SuspenseCore.Ability.WeaponSlot.Sidearm");
			UE_DEFINE_GAMEPLAY_TAG(Melee, "SuspenseCore.Ability.WeaponSlot.Melee");
		}

		namespace Input
		{
			UE_DEFINE_GAMEPLAY_TAG(Interact, "Ability.Input.Interact");
		}

		namespace Cooldown
		{
			UE_DEFINE_GAMEPLAY_TAG(Interact, "Ability.Cooldown.Interact");
		}
	}

	//==================================================================
	// INTERACTION TYPE TAGS
	//==================================================================
	namespace InteractionType
	{
		UE_DEFINE_GAMEPLAY_TAG(Pickup, "Interaction.Type.Pickup");
		UE_DEFINE_GAMEPLAY_TAG(Weapon, "Interaction.Type.Weapon");
		UE_DEFINE_GAMEPLAY_TAG(Ammo, "Interaction.Type.Ammo");
	}

	//==================================================================
	// ITEM TAGS
	//==================================================================
	namespace Item
	{
		UE_DEFINE_GAMEPLAY_TAG(Generic, "Item.Generic");
		UE_DEFINE_GAMEPLAY_TAG(Root, "Item");

		// Weapon types
		namespace Weapon
		{
			UE_DEFINE_GAMEPLAY_TAG(AR, "Item.Weapon.AR");
			UE_DEFINE_GAMEPLAY_TAG(DMR, "Item.Weapon.DMR");
			UE_DEFINE_GAMEPLAY_TAG(SR, "Item.Weapon.SR");
			UE_DEFINE_GAMEPLAY_TAG(LMG, "Item.Weapon.LMG");
			UE_DEFINE_GAMEPLAY_TAG(SMG, "Item.Weapon.SMG");
			UE_DEFINE_GAMEPLAY_TAG(Shotgun, "Item.Weapon.Shotgun");
			UE_DEFINE_GAMEPLAY_TAG(PDW, "Item.Weapon.PDW");
			UE_DEFINE_GAMEPLAY_TAG(Pistol, "Item.Weapon.Pistol");
			UE_DEFINE_GAMEPLAY_TAG(Revolver, "Item.Weapon.Revolver");

			namespace Melee
			{
				UE_DEFINE_GAMEPLAY_TAG(Knife, "Item.Weapon.Melee.Knife");
			}
		}

		// Armor types
		namespace Armor
		{
			UE_DEFINE_GAMEPLAY_TAG(Helmet, "Item.Armor.Helmet");
			UE_DEFINE_GAMEPLAY_TAG(BodyArmor, "Item.Armor.BodyArmor");
		}

		// Gear types
		namespace Gear
		{
			UE_DEFINE_GAMEPLAY_TAG(Headwear, "Item.Gear.Headwear");
			UE_DEFINE_GAMEPLAY_TAG(Earpiece, "Item.Gear.Earpiece");
			UE_DEFINE_GAMEPLAY_TAG(Eyewear, "Item.Gear.Eyewear");
			UE_DEFINE_GAMEPLAY_TAG(FaceCover, "Item.Gear.FaceCover");
			UE_DEFINE_GAMEPLAY_TAG(TacticalRig, "Item.Gear.TacticalRig");
			UE_DEFINE_GAMEPLAY_TAG(Backpack, "Item.Gear.Backpack");
			UE_DEFINE_GAMEPLAY_TAG(SecureContainer, "Item.Gear.SecureContainer");
			UE_DEFINE_GAMEPLAY_TAG(Armband, "Item.Gear.Armband");
		}

		// Consumable types
		UE_DEFINE_GAMEPLAY_TAG(Consumable, "Item.Consumable");
		UE_DEFINE_GAMEPLAY_TAG(Medical, "Item.Medical");
		UE_DEFINE_GAMEPLAY_TAG(Throwable, "Item.Throwable");
		UE_DEFINE_GAMEPLAY_TAG(Ammo, "Item.Ammo");
		UE_DEFINE_GAMEPLAY_TAG(Magazine, "Item.Magazine");
	}

	//==================================================================
	//==================================================================
	// WEAPON FIRE MODE TAGS
	//==================================================================
	namespace Weapon
	{
		namespace FireMode
		{
			UE_DEFINE_GAMEPLAY_TAG(Auto, "Weapon.FireMode.Auto");
			UE_DEFINE_GAMEPLAY_TAG(Semi, "Weapon.FireMode.Semi");
			UE_DEFINE_GAMEPLAY_TAG(Single, "Weapon.FireMode.Single");
			UE_DEFINE_GAMEPLAY_TAG(Burst, "Weapon.FireMode.Burst");
			UE_DEFINE_GAMEPLAY_TAG(Burst2, "Weapon.FireMode.Burst2");
			UE_DEFINE_GAMEPLAY_TAG(Burst3, "Weapon.FireMode.Burst3");
			UE_DEFINE_GAMEPLAY_TAG(Safe, "Weapon.FireMode.Safe");
		}
	}

	//==================================================================
	// DATA TAGS
	//==================================================================
	namespace Data
	{
		UE_DEFINE_GAMEPLAY_TAG(ItemID, "Data.ItemID");
		UE_DEFINE_GAMEPLAY_TAG(Damage, "Data.Damage");

		// Cost tags for SetByCaller in GameplayEffects
		namespace Cost
		{
			UE_DEFINE_GAMEPLAY_TAG(Stamina, "Data.Cost.Stamina");
			UE_DEFINE_GAMEPLAY_TAG(StaminaPerSecond, "Data.Cost.StaminaPerSecond");
			UE_DEFINE_GAMEPLAY_TAG(SpeedMultiplier, "Data.Cost.SpeedMultiplier");
		}
	}

	//==================================================================
	// EFFECT TAGS
	//==================================================================
	namespace Effect
	{
		namespace Movement
		{
			UE_DEFINE_GAMEPLAY_TAG(SprintCost, "Effect.Movement.SprintCost");
			UE_DEFINE_GAMEPLAY_TAG(SprintBuff, "Effect.Movement.SprintBuff");
			UE_DEFINE_GAMEPLAY_TAG(JumpCost, "Effect.Movement.JumpCost");
			UE_DEFINE_GAMEPLAY_TAG(CrouchDebuff, "Effect.Movement.CrouchDebuff");
		}
	}

	//==================================================================
	// UI TAGS
	//==================================================================
	namespace UI
	{
		namespace Screen
		{
			UE_DEFINE_GAMEPLAY_TAG(Character, "UI.Screen.Character");
		}

		namespace Panel
		{
			UE_DEFINE_GAMEPLAY_TAG(Equipment, "SuspenseCore.UI.Panel.Equipment");
		}

		namespace Tab
		{
			UE_DEFINE_GAMEPLAY_TAG(Inventory, "UI.Tab.Inventory");
		}

		namespace TabBar
		{
			UE_DEFINE_GAMEPLAY_TAG(Character, "UI.TabBar.Character");
		}

		namespace HUD
		{
			UE_DEFINE_GAMEPLAY_TAG(Crosshair, "UI.HUD.Crosshair");
			UE_DEFINE_GAMEPLAY_TAG(HealthStamina, "UI.HUD.HealthStamina");
			UE_DEFINE_GAMEPLAY_TAG(WeaponInfo, "UI.HUD.WeaponInfo");
		}
	}

	//==================================================================
	// EQUIPMENT SLOT TAGS
	//==================================================================
	namespace EquipmentSlot
	{
		// Weapons
		UE_DEFINE_GAMEPLAY_TAG(PrimaryWeapon, "Equipment.Slot.PrimaryWeapon");
		UE_DEFINE_GAMEPLAY_TAG(SecondaryWeapon, "Equipment.Slot.SecondaryWeapon");
		UE_DEFINE_GAMEPLAY_TAG(Sidearm, "Equipment.Slot.Sidearm");
		UE_DEFINE_GAMEPLAY_TAG(Holster, "Equipment.Slot.Holster");
		UE_DEFINE_GAMEPLAY_TAG(Melee, "Equipment.Slot.Melee");
		UE_DEFINE_GAMEPLAY_TAG(Scabbard, "Equipment.Slot.Scabbard");

		// Head gear
		UE_DEFINE_GAMEPLAY_TAG(Headwear, "Equipment.Slot.Headwear");
		UE_DEFINE_GAMEPLAY_TAG(Earpiece, "Equipment.Slot.Earpiece");
		UE_DEFINE_GAMEPLAY_TAG(Eyewear, "Equipment.Slot.Eyewear");
		UE_DEFINE_GAMEPLAY_TAG(FaceCover, "Equipment.Slot.FaceCover");

		// Body gear
		UE_DEFINE_GAMEPLAY_TAG(BodyArmor, "Equipment.Slot.BodyArmor");
		UE_DEFINE_GAMEPLAY_TAG(TacticalRig, "Equipment.Slot.TacticalRig");

		// Storage
		UE_DEFINE_GAMEPLAY_TAG(Backpack, "Equipment.Slot.Backpack");
		UE_DEFINE_GAMEPLAY_TAG(SecureContainer, "Equipment.Slot.SecureContainer");

		// Quick slots
		UE_DEFINE_GAMEPLAY_TAG(QuickSlot1, "Equipment.Slot.QuickSlot1");
		UE_DEFINE_GAMEPLAY_TAG(QuickSlot2, "Equipment.Slot.QuickSlot2");
		UE_DEFINE_GAMEPLAY_TAG(QuickSlot3, "Equipment.Slot.QuickSlot3");
		UE_DEFINE_GAMEPLAY_TAG(QuickSlot4, "Equipment.Slot.QuickSlot4");

		// Special
		UE_DEFINE_GAMEPLAY_TAG(Armband, "Equipment.Slot.Armband");
	}
}

//==================================================================
// TAG VALIDATION
//==================================================================

const TArray<FString>& FSuspenseCoreTagValidator::GetCriticalTagNames()
{
	static TArray<FString> CriticalTags = {
		// Player events
		TEXT("SuspenseCore.Event.Player.Spawned"),
		TEXT("SuspenseCore.Event.Player.Initialized"),
		TEXT("SuspenseCore.Event.Player.Left"),

		// GAS events
		TEXT("SuspenseCore.Event.GAS.Initialized"),
		TEXT("SuspenseCore.Event.GAS.Attribute.Changed"),

		// Weapon events
		TEXT("SuspenseCore.Event.Weapon.Fired"),
		TEXT("SuspenseCore.Event.Weapon.AmmoChanged"),

		// Interaction events
		TEXT("SuspenseCore.Event.Interaction.Started"),
		TEXT("SuspenseCore.Event.Interaction.Completed"),

		// Inventory events
		TEXT("SuspenseCore.Event.Inventory.ItemAdded"),
		TEXT("SuspenseCore.Event.Inventory.ItemRemoved"),
		TEXT("SuspenseCore.Event.Inventory.OperationFailed"),

		// Equipment events
		TEXT("SuspenseCore.Event.Equipment.Initialized"),

		// Data tags (SetByCaller)
		TEXT("Data.ItemID"),

		// State tags
		TEXT("State.Dead"),
		TEXT("State.Stunned"),
		TEXT("State.Disabled"),

		// Ability tags
		TEXT("SuspenseCore.Ability.Jump"),
		TEXT("SuspenseCore.Ability.Sprint"),
		TEXT("SuspenseCore.Ability.Interact"),

		// Interaction types
		TEXT("Interaction.Type.Pickup"),
		TEXT("Interaction.Type.Weapon"),
		TEXT("Interaction.Type.Ammo"),

		// Equipment slots
		TEXT("Equipment.Slot.PrimaryWeapon"),
		TEXT("Equipment.Slot.BodyArmor"),
	};

	return CriticalTags;
}

bool FSuspenseCoreTagValidator::ValidateCriticalTags()
{
	TArray<FString> MissingTags = GetMissingTags();

	if (MissingTags.Num() > 0)
	{
		for (const FString& Tag : MissingTags)
		{
			UE_LOG(LogTemp, Error, TEXT("SuspenseCore: Critical GameplayTag missing: %s"), *Tag);
		}
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("SuspenseCore: All %d critical GameplayTags validated successfully"),
		GetCriticalTagNames().Num());
	return true;
}

TArray<FString> FSuspenseCoreTagValidator::GetMissingTags()
{
	TArray<FString> Missing;

	for (const FString& TagName : GetCriticalTagNames())
	{
		FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
		if (!Tag.IsValid())
		{
			Missing.Add(TagName);
		}
	}

	return Missing;
}
