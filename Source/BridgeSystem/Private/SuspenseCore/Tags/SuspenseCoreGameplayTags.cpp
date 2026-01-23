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
			UE_DEFINE_GAMEPLAY_TAG(StanceChangeRequested, "Event.Weapon.StanceChangeRequested");
			UE_DEFINE_GAMEPLAY_TAG(StanceRestoreRequested, "Event.Weapon.StanceRestoreRequested");
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

		//--------------------------------------------------------------
		// Throwable Events (Grenades, etc.)
		//--------------------------------------------------------------
		namespace Throwable
		{
			UE_DEFINE_GAMEPLAY_TAG(PrepareStarted, "SuspenseCore.Event.Throwable.PrepareStarted");
			UE_DEFINE_GAMEPLAY_TAG(PinPulled, "SuspenseCore.Event.Throwable.PinPulled");
			UE_DEFINE_GAMEPLAY_TAG(CookingStarted, "SuspenseCore.Event.Throwable.CookingStarted");
			UE_DEFINE_GAMEPLAY_TAG(Releasing, "SuspenseCore.Event.Throwable.Releasing");
			UE_DEFINE_GAMEPLAY_TAG(Thrown, "SuspenseCore.Event.Throwable.Thrown");
			UE_DEFINE_GAMEPLAY_TAG(Cancelled, "SuspenseCore.Event.Throwable.Cancelled");
			UE_DEFINE_GAMEPLAY_TAG(SpawnRequested, "SuspenseCore.Event.Throwable.SpawnRequested");
			UE_DEFINE_GAMEPLAY_TAG(Equipped, "SuspenseCore.Event.Throwable.Equipped");
			UE_DEFINE_GAMEPLAY_TAG(Unequipped, "SuspenseCore.Event.Throwable.Unequipped");
			UE_DEFINE_GAMEPLAY_TAG(Ready, "SuspenseCore.Event.Throwable.Ready");
		}

		//--------------------------------------------------------------
		// DoT (Damage-over-Time) Events
		// Published by DoTService for UI/gameplay integration
		//--------------------------------------------------------------
		namespace DoT
		{
			UE_DEFINE_GAMEPLAY_TAG(Applied, "SuspenseCore.Event.DoT.Applied");
			UE_DEFINE_GAMEPLAY_TAG(Tick, "SuspenseCore.Event.DoT.Tick");
			UE_DEFINE_GAMEPLAY_TAG(Expired, "SuspenseCore.Event.DoT.Expired");
			UE_DEFINE_GAMEPLAY_TAG(Removed, "SuspenseCore.Event.DoT.Removed");
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
		UE_DEFINE_GAMEPLAY_TAG(ThrowingGrenade, "State.ThrowingGrenade");
		UE_DEFINE_GAMEPLAY_TAG(WeaponDrawn, "State.WeaponDrawn");
		UE_DEFINE_GAMEPLAY_TAG(GrenadeEquipped, "State.GrenadeEquipped");

		// Grenade effect states
		UE_DEFINE_GAMEPLAY_TAG(Blinded, "State.Blinded");
		UE_DEFINE_GAMEPLAY_TAG(Deafened, "State.Deafened");
		UE_DEFINE_GAMEPLAY_TAG(Disoriented, "State.Disoriented");
		UE_DEFINE_GAMEPLAY_TAG(Burning, "State.Burning");
		UE_DEFINE_GAMEPLAY_TAG(InFireZone, "State.InFireZone");
		UE_DEFINE_GAMEPLAY_TAG(Damaged, "State.Damaged");

		//--------------------------------------------------------------
		// DoT (Damage-over-Time) States
		//--------------------------------------------------------------
		namespace Health
		{
			UE_DEFINE_GAMEPLAY_TAG(BleedingLight, "State.Health.Bleeding.Light");
			UE_DEFINE_GAMEPLAY_TAG(BleedingHeavy, "State.Health.Bleeding.Heavy");
			UE_DEFINE_GAMEPLAY_TAG(Regenerating, "State.Health.Regenerating");
			UE_DEFINE_GAMEPLAY_TAG(Poisoned, "State.Health.Poisoned");
			UE_DEFINE_GAMEPLAY_TAG(Fracture, "State.Health.Fracture");
			UE_DEFINE_GAMEPLAY_TAG(FractureLeg, "State.Health.Fracture.Leg");
			UE_DEFINE_GAMEPLAY_TAG(FractureArm, "State.Health.Fracture.Arm");
			UE_DEFINE_GAMEPLAY_TAG(Painkiller, "State.Health.Painkiller");
			UE_DEFINE_GAMEPLAY_TAG(Dehydrated, "State.Health.Dehydrated");
			UE_DEFINE_GAMEPLAY_TAG(Exhausted, "State.Health.Exhausted");
		}

		//--------------------------------------------------------------
		// Combat Status States
		//--------------------------------------------------------------
		namespace Combat
		{
			UE_DEFINE_GAMEPLAY_TAG(Blinded, "State.Combat.Blinded");
			UE_DEFINE_GAMEPLAY_TAG(Suppressed, "State.Combat.Suppressed");
			UE_DEFINE_GAMEPLAY_TAG(Adrenaline, "State.Combat.Adrenaline");
			UE_DEFINE_GAMEPLAY_TAG(Fortified, "State.Combat.Fortified");
			UE_DEFINE_GAMEPLAY_TAG(Stunned, "State.Combat.Stunned");
			UE_DEFINE_GAMEPLAY_TAG(NoADS, "State.Combat.NoADS");
			UE_DEFINE_GAMEPLAY_TAG(Painkiller, "State.Combat.Painkiller");
			UE_DEFINE_GAMEPLAY_TAG(PainImmune, "State.Combat.PainImmune");
			UE_DEFINE_GAMEPLAY_TAG(DamageResist, "State.Combat.DamageResist");
		}

		//--------------------------------------------------------------
		// Movement Status States
		//--------------------------------------------------------------
		namespace Movement
		{
			UE_DEFINE_GAMEPLAY_TAG(Slowed, "State.Movement.Slowed");
			UE_DEFINE_GAMEPLAY_TAG(Disabled, "State.Movement.Disabled");
			UE_DEFINE_GAMEPLAY_TAG(Limp, "State.Movement.Limp");
			UE_DEFINE_GAMEPLAY_TAG(NoSprint, "State.Movement.NoSprint");
			UE_DEFINE_GAMEPLAY_TAG(Haste, "State.Movement.Haste");
		}

		//--------------------------------------------------------------
		// Action Status States
		//--------------------------------------------------------------
		namespace Action
		{
			UE_DEFINE_GAMEPLAY_TAG(Disabled, "State.Action.Disabled");
		}

		//--------------------------------------------------------------
		// Survival Status States
		//--------------------------------------------------------------
		namespace Survival
		{
			UE_DEFINE_GAMEPLAY_TAG(Dehydrated, "State.Survival.Dehydrated");
			UE_DEFINE_GAMEPLAY_TAG(Exhausted, "State.Survival.Exhausted");
			UE_DEFINE_GAMEPLAY_TAG(Hungry, "State.Survival.Hungry");
		}

		//--------------------------------------------------------------
		// Immunity Tags
		//--------------------------------------------------------------
		namespace Immunity
		{
			UE_DEFINE_GAMEPLAY_TAG(Pain, "State.Immunity.Pain");
			UE_DEFINE_GAMEPLAY_TAG(Fire, "State.Immunity.Fire");
			UE_DEFINE_GAMEPLAY_TAG(Poison, "State.Immunity.Poison");
			UE_DEFINE_GAMEPLAY_TAG(Stun, "State.Immunity.Stun");
			UE_DEFINE_GAMEPLAY_TAG(Flash, "State.Immunity.Flash");
			UE_DEFINE_GAMEPLAY_TAG(Slow, "State.Immunity.Slow");
		}
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

		// Throwable abilities (grenade, consumable throw)
		namespace Throwable
		{
			UE_DEFINE_GAMEPLAY_TAG(Equip, "SuspenseCore.Ability.Throwable.Equip");
			UE_DEFINE_GAMEPLAY_TAG(Grenade, "SuspenseCore.Ability.Throwable.Grenade");
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

		// Grenade type tags for stance/animation selection
		namespace Grenade
		{
			UE_DEFINE_GAMEPLAY_TAG(Frag, "Weapon.Grenade.Frag");
			UE_DEFINE_GAMEPLAY_TAG(Smoke, "Weapon.Grenade.Smoke");
			UE_DEFINE_GAMEPLAY_TAG(Flash, "Weapon.Grenade.Flash");
			UE_DEFINE_GAMEPLAY_TAG(Incendiary, "Weapon.Grenade.Incendiary");
		}
	}

	//==================================================================
	// DATA TAGS
	//==================================================================
	namespace Data
	{
		UE_DEFINE_GAMEPLAY_TAG(ItemID, "Data.ItemID");
		UE_DEFINE_GAMEPLAY_TAG(Damage, "Data.Damage");
		UE_DEFINE_GAMEPLAY_TAG(ErgonomicsPenalty, "Data.ErgonomicsPenalty");

		// Cost tags for SetByCaller in GameplayEffects
		namespace Cost
		{
			UE_DEFINE_GAMEPLAY_TAG(Stamina, "Data.Cost.Stamina");
			UE_DEFINE_GAMEPLAY_TAG(StaminaPerSecond, "Data.Cost.StaminaPerSecond");
			UE_DEFINE_GAMEPLAY_TAG(SpeedMultiplier, "Data.Cost.SpeedMultiplier");
		}

		// Effect tags for SetByCaller (GameplayEffects duration/magnitude)
		namespace Effect
		{
			UE_DEFINE_GAMEPLAY_TAG(Duration, "Data.Effect.Duration");
		}

		// Damage data tags for SetByCaller (prefixed to avoid conflict with Data::Damage tag)
		UE_DEFINE_GAMEPLAY_TAG(DamagePoison, "Data.Damage.Poison");

		// Heal data tags for SetByCaller
		namespace Heal
		{
			UE_DEFINE_GAMEPLAY_TAG(PerTick, "Data.Heal.PerTick");
		}

		// Grenade effect data tags
		namespace Grenade
		{
			UE_DEFINE_GAMEPLAY_TAG(FlashDuration, "Data.Grenade.FlashDuration");
			UE_DEFINE_GAMEPLAY_TAG(BurnDuration, "Data.Grenade.BurnDuration");
		}

		// Burn damage data tag
		UE_DEFINE_GAMEPLAY_TAG(DamageBurn, "Data.Damage.Burn");

		//--------------------------------------------------------------
		// DoT Data Tags (SetByCaller magnitudes)
		//--------------------------------------------------------------
		namespace DoT
		{
			UE_DEFINE_GAMEPLAY_TAG(Bleed, "Data.Damage.Bleed");
			UE_DEFINE_GAMEPLAY_TAG(BurnArmor, "Data.Damage.Burn.Armor");
			UE_DEFINE_GAMEPLAY_TAG(BurnHealth, "Data.Damage.Burn.Health");
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

		// Damage effect tags
		UE_DEFINE_GAMEPLAY_TAG(Damage, "Effect.Damage");
		UE_DEFINE_GAMEPLAY_TAG(DamageGrenade, "Effect.Damage.Grenade");
		UE_DEFINE_GAMEPLAY_TAG(DamageExplosion, "Effect.Damage.Explosion");
		UE_DEFINE_GAMEPLAY_TAG(DamageShrapnel, "Effect.Damage.Shrapnel");
		UE_DEFINE_GAMEPLAY_TAG(DamageBurn, "Effect.Damage.Burn");
		UE_DEFINE_GAMEPLAY_TAG(DamagePoison, "Effect.Damage.Poison");
		UE_DEFINE_GAMEPLAY_TAG(DamageWithHitInfo, "Effect.Damage.WithHitInfo");
		UE_DEFINE_GAMEPLAY_TAG(DamageBleed, "Effect.Damage.Bleed");
		UE_DEFINE_GAMEPLAY_TAG(DamageBleedLight, "Effect.Damage.Bleed.Light");
		UE_DEFINE_GAMEPLAY_TAG(DamageBleedHeavy, "Effect.Damage.Bleed.Heavy");

		// Base effect type tags
		UE_DEFINE_GAMEPLAY_TAG(Debuff, "Effect.Debuff");
		UE_DEFINE_GAMEPLAY_TAG(Buff, "Effect.Buff");
		UE_DEFINE_GAMEPLAY_TAG(HoT, "Effect.HoT");

		// Debuff Effect Tags (prefixed to avoid conflict with Effect::Debuff tag)
		UE_DEFINE_GAMEPLAY_TAG(DebuffStun, "Effect.Debuff.Stun");
		UE_DEFINE_GAMEPLAY_TAG(DebuffSuppression, "Effect.Debuff.Suppression");
		UE_DEFINE_GAMEPLAY_TAG(DebuffFracture, "Effect.Debuff.Fracture");
		UE_DEFINE_GAMEPLAY_TAG(DebuffSurvival, "Effect.Debuff.Survival");

		// Grenade effect tags
		UE_DEFINE_GAMEPLAY_TAG(GrenadeFlashbang, "Effect.Grenade.Flashbang");
		UE_DEFINE_GAMEPLAY_TAG(GrenadeFlashbangPartial, "Effect.Grenade.Flashbang.Partial");
		UE_DEFINE_GAMEPLAY_TAG(GrenadeIncendiary, "Effect.Grenade.Incendiary");
		UE_DEFINE_GAMEPLAY_TAG(GrenadeIncendiaryZone, "Effect.Grenade.Incendiary.Zone");
		UE_DEFINE_GAMEPLAY_TAG(GrenadeIncendiaryArmorBypass, "Effect.Grenade.Incendiary.ArmorBypass");
		UE_DEFINE_GAMEPLAY_TAG(GrenadeShrapnel, "Effect.Grenade.Shrapnel");

		//--------------------------------------------------------------
		// DoT Effect Tags (Effect.DoT.*)
		//--------------------------------------------------------------
		namespace DoT
		{
			UE_DEFINE_GAMEPLAY_TAG(Root, "Effect.DoT");
			UE_DEFINE_GAMEPLAY_TAG(BleedLight, "Effect.DoT.Bleed.Light");
			UE_DEFINE_GAMEPLAY_TAG(BleedHeavy, "Effect.DoT.Bleed.Heavy");
			UE_DEFINE_GAMEPLAY_TAG(Burn, "Effect.DoT.Burn");
			UE_DEFINE_GAMEPLAY_TAG(BurnArmorBypass, "Effect.DoT.Burn.ArmorBypass");
			UE_DEFINE_GAMEPLAY_TAG(Poison, "Effect.DoT.Poison");
		}

		//--------------------------------------------------------------
		// Cure Effect Tags (Effect.Cure.*)
		//--------------------------------------------------------------
		namespace Cure
		{
			UE_DEFINE_GAMEPLAY_TAG(Bleeding, "Effect.Cure.Bleeding");
			UE_DEFINE_GAMEPLAY_TAG(BleedingHeavy, "Effect.Cure.Bleeding.Heavy");
			UE_DEFINE_GAMEPLAY_TAG(Fire, "Effect.Cure.Fire");
			UE_DEFINE_GAMEPLAY_TAG(Poison, "Effect.Cure.Poison");
			UE_DEFINE_GAMEPLAY_TAG(Fracture, "Effect.Cure.Fracture");
			UE_DEFINE_GAMEPLAY_TAG(Dehydration, "Effect.Cure.Dehydration");
			UE_DEFINE_GAMEPLAY_TAG(Exhaustion, "Effect.Cure.Exhaustion");
		}

		// Buff Effect Tags (prefixed to avoid conflict with Effect::Buff tag)
		UE_DEFINE_GAMEPLAY_TAG(BuffRegenerating, "Effect.Buff.Regenerating");
		UE_DEFINE_GAMEPLAY_TAG(BuffPainkiller, "Effect.Buff.Painkiller");
		UE_DEFINE_GAMEPLAY_TAG(BuffAdrenaline, "Effect.Buff.Adrenaline");
		UE_DEFINE_GAMEPLAY_TAG(BuffFortified, "Effect.Buff.Fortified");
		UE_DEFINE_GAMEPLAY_TAG(BuffHeal, "Effect.Buff.Heal");
		UE_DEFINE_GAMEPLAY_TAG(BuffCombat, "Effect.Buff.Combat");
		UE_DEFINE_GAMEPLAY_TAG(BuffDefense, "Effect.Buff.Defense");
		UE_DEFINE_GAMEPLAY_TAG(BuffMovement, "Effect.Buff.Movement");
	}

	//==================================================================
	// GAMEPLAYCUE TAGS
	//==================================================================
	namespace GameplayCue
	{
		// Grenade cues
		UE_DEFINE_GAMEPLAY_TAG(GrenadeFlashbang, "GameplayCue.Grenade.Flashbang");
		UE_DEFINE_GAMEPLAY_TAG(GrenadeFlashbangPartial, "GameplayCue.Grenade.Flashbang.Partial");
		UE_DEFINE_GAMEPLAY_TAG(GrenadeBurn, "GameplayCue.Grenade.Burn");

		//--------------------------------------------------------------
		// DoT GameplayCues (GameplayCue.DoT.*)
		//--------------------------------------------------------------
		namespace DoT
		{
			UE_DEFINE_GAMEPLAY_TAG(BleedLight, "GameplayCue.DoT.Bleed.Light");
			UE_DEFINE_GAMEPLAY_TAG(BleedHeavy, "GameplayCue.DoT.Bleed.Heavy");
			UE_DEFINE_GAMEPLAY_TAG(Burn, "GameplayCue.DoT.Burn");
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

		// DoT tags (critical for damage system)
		TEXT("State.Health.Bleeding.Light"),
		TEXT("State.Health.Bleeding.Heavy"),
		TEXT("SuspenseCore.Event.DoT.Applied"),
		TEXT("SuspenseCore.Event.DoT.Removed"),
		TEXT("Data.Damage.Bleed"),
		TEXT("Data.Damage.Burn.Armor"),
		TEXT("Data.Damage.Burn.Health"),
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
