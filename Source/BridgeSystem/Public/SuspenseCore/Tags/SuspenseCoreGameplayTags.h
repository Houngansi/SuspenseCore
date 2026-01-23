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
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(RecoilImpulse);  // Recoil impulse for convergence
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(StanceChangeRequested);  // Request stance change (grenade equip)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(StanceRestoreRequested); // Request stance restore (grenade unequip)
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
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(WeaponSlotSwitched);
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
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ADSEnter);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ADSExit);

			// Camera Shake Events
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ShakeWeapon);     // Weapon fire camera shake
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ShakeMovement);   // Movement camera shake (jump, land, sprint)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ShakeDamage);     // Damage received camera shake
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ShakeExplosion);  // Explosion camera shake
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

		//--------------------------------------------------------------
		// Throwable Events (Grenades, etc.)
		//--------------------------------------------------------------
		namespace Throwable
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(PrepareStarted);  // Started prepare phase
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(PinPulled);       // Pin pulled, grenade armed
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(CookingStarted);  // Started cooking (holding)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Releasing);       // Release notify - hide visual before spawn
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Thrown);          // Grenade thrown
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cancelled);       // Throw cancelled (pin not pulled)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SpawnRequested);  // Request to spawn grenade actor
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Equipped);        // Grenade equipped (draw complete)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Unequipped);      // Grenade unequipped (holstered)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ready);           // Grenade ready to throw (equip done)
		}

		//--------------------------------------------------------------
		// DoT (Damage-over-Time) Events
		// Used by DoTService for EventBus communication
		//--------------------------------------------------------------
		namespace DoT
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Applied);    // DoT effect applied to target
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tick);       // DoT damage tick occurred
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Expired);    // DoT effect expired naturally
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Removed);    // DoT effect removed (healed)
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
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Jumping);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sprinting);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interacting);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Aiming);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Firing);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Reloading);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HoldingBreath);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(WeaponBlocked);  // Weapon blocked by obstacle/wall
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BurstActive);    // Burst fire in progress
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AutoFireActive); // Auto fire active
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ThrowingGrenade); // Grenade throw in progress
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(WeaponDrawn);     // Weapon currently drawn
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GrenadeEquipped); // Grenade equipped and ready to throw

		// Grenade effect states
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Blinded);        // Flashbang blindness
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Deafened);       // Flashbang deafness
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Disoriented);    // Flashbang disorientation
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Burning);        // On fire from incendiary
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InFireZone);     // Standing in fire zone
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damaged);        // Recently took damage

		//--------------------------------------------------------------
		// DoT (Damage-over-Time) States - Bleeding, Burning, etc.
		// @see FSuspenseCoreStatusEffectAttributeRow (SSOT)
		//--------------------------------------------------------------
		namespace Health
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BleedingLight);   // State.Health.Bleeding.Light
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BleedingHeavy);   // State.Health.Bleeding.Heavy
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Regenerating);    // State.Health.Regenerating (HoT active)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Poisoned);        // State.Health.Poisoned
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fracture);        // State.Health.Fracture
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(FractureLeg);     // State.Health.Fracture.Leg
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(FractureArm);     // State.Health.Fracture.Arm
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Painkiller);      // State.Health.Painkiller (pain suppression)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Dehydrated);      // State.Health.Dehydrated
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Exhausted);       // State.Health.Exhausted
		}

		//--------------------------------------------------------------
		// Combat Status States
		//--------------------------------------------------------------
		namespace Combat
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Blinded);         // State.Combat.Blinded (flashbang)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Suppressed);      // State.Combat.Suppressed (under fire)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Adrenaline);      // State.Combat.Adrenaline (combat high)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fortified);       // State.Combat.Fortified (damage resist)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stunned);         // State.Combat.Stunned
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(NoADS);           // State.Combat.NoADS (fracture arm)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Painkiller);      // State.Combat.Painkiller
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(PainImmune);      // State.Combat.PainImmune
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageResist);    // State.Combat.DamageResist
		}

		//--------------------------------------------------------------
		// Movement Status States
		//--------------------------------------------------------------
		namespace Movement
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slowed);          // State.Movement.Slowed
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Disabled);        // State.Movement.Disabled
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Limp);            // State.Movement.Limp (fracture leg)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(NoSprint);        // State.Movement.NoSprint
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Haste);           // State.Movement.Haste
		}

		//--------------------------------------------------------------
		// Action Status States
		//--------------------------------------------------------------
		namespace Action
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Disabled);        // State.Action.Disabled
		}

		//--------------------------------------------------------------
		// Survival Status States
		//--------------------------------------------------------------
		namespace Survival
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Dehydrated);      // State.Survival.Dehydrated
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Exhausted);       // State.Survival.Exhausted
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Hungry);          // State.Survival.Hungry
		}

		//--------------------------------------------------------------
		// Immunity Tags (blocks effect application)
		//--------------------------------------------------------------
		namespace Immunity
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pain);            // State.Immunity.Pain
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fire);            // State.Immunity.Fire
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Poison);          // State.Immunity.Poison
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stun);            // State.Immunity.Stun
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Flash);           // State.Immunity.Flash
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slow);            // State.Immunity.Slow
		}
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

		namespace Weapon
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AimDownSight);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fire);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Reload);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(FireModeSwitch);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Inspect);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HoldBreath);
		}

		// QuickSlot abilities (Tarkov-style magazine/item access via keys 4-7)
		namespace QuickSlot
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slot1);  // Key 4
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slot2);  // Key 5
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slot3);  // Key 6
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slot4);  // Key 7
		}

		// WeaponSlot abilities (direct weapon slot switching via keys 1-3, V)
		namespace WeaponSlot
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Primary);    // Key 1 → Slot 0
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Secondary);  // Key 2 → Slot 1
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sidearm);    // Key 3 → Slot 2
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Melee);      // Key V → Slot 3
		}

		namespace Input
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact);
		}

		namespace Cooldown
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact);
		}

		// Throwable abilities (grenade, consumable throw)
		namespace Throwable
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Equip);    // Equip grenade from QuickSlot
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Grenade);  // Throw equipped grenade
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

		// Weapon types
		namespace Weapon
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AR);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DMR);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SR);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(LMG);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SMG);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shotgun);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(PDW);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pistol);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Revolver);

			namespace Melee
			{
				BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Knife);
			}
		}

		// Armor types
		namespace Armor
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Helmet);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BodyArmor);
		}

		// Gear types
		namespace Gear
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Headwear);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Earpiece);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Eyewear);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(FaceCover);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TacticalRig);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Backpack);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SecureContainer);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Armband);
		}

		// Consumable types
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Consumable);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Medical);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Throwable);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ammo);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Magazine);
	}

	//==================================================================
	// WEAPON FIRE MODE TAGS
	// Used for fire mode selection (Auto, Semi, Burst, etc.)
	//==================================================================
	namespace Weapon
	{
		namespace FireMode
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Auto);      // Fully automatic
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Semi);      // Semi-automatic (single shot)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Single);    // Alias for Semi (Tarkov-style)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Burst);     // Burst fire (3-round, etc.)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Burst2);    // 2-round burst
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Burst3);    // 3-round burst
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Safe);      // Safety on
		}

		// Grenade type tags for stance/animation selection
		namespace Grenade
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Frag);       // Fragmentation grenade
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Smoke);      // Smoke grenade
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Flash);      // Flashbang grenade
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Incendiary); // Incendiary grenade
		}
	}

	//==================================================================
	// DATA TAGS (SetByCaller)
	//==================================================================
	namespace Data
	{
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ItemID);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage);  // SetByCaller damage value
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ErgonomicsPenalty);  // SetByCaller ergonomics penalty (magazine/attachment)

		// Cost tags for SetByCaller in GameplayEffects
		namespace Cost
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stamina);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(StaminaPerSecond);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SpeedMultiplier);
		}

		// Effect tags for SetByCaller (GameplayEffects duration/magnitude)
		namespace Effect
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Duration);  // Data.Effect.Duration
		}

		// Damage data tags for SetByCaller
		namespace Damage
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Poison);    // Data.Damage.Poison
		}

		// Heal data tags for SetByCaller
		namespace Heal
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(PerTick);   // Data.Heal.PerTick
		}

		// Grenade effect data tags
		namespace Grenade
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(FlashDuration);  // Flashbang effect duration
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BurnDuration);   // Incendiary burn duration
		}

		// Burn damage data tag (Data.Damage.Burn)
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageBurn);

		//--------------------------------------------------------------
		// DoT Data Tags (SetByCaller magnitudes)
		//--------------------------------------------------------------
		namespace DoT
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Bleed);           // Data.Damage.Bleed (HP/tick for bleeding)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BurnArmor);       // Data.Damage.Burn.Armor (armor bypass)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BurnHealth);      // Data.Damage.Burn.Health (health bypass)
		}
	}

	//==================================================================
	// EFFECT TAGS - GameplayEffect identification
	//==================================================================
	namespace Effect
	{
		namespace Movement
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SprintCost);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SprintBuff);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(JumpCost);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(CrouchDebuff);
		}

		// Damage effect tags (Effect.Damage.*)
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage);              // Effect.Damage (base)
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageGrenade);       // Effect.Damage.Grenade
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageExplosion);     // Effect.Damage.Explosion
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageShrapnel);      // Effect.Damage.Shrapnel
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageBurn);          // Effect.Damage.Burn
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamagePoison);        // Effect.Damage.Poison
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageWithHitInfo);   // Effect.Damage.WithHitInfo
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageBleed);         // Effect.Damage.Bleed (base bleeding)
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageBleedLight);    // Effect.Damage.Bleed.Light
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageBleedHeavy);    // Effect.Damage.Bleed.Heavy

		// Base effect type tags
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Debuff);              // Effect.Debuff (base debuff tag)
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Buff);                // Effect.Buff (base buff tag)
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HoT);                 // Effect.HoT (heal over time)

		//--------------------------------------------------------------
		// Debuff Effect Tags (Effect.Debuff.*)
		//--------------------------------------------------------------
		namespace Debuff
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stun);            // Effect.Debuff.Stun
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Suppression);     // Effect.Debuff.Suppression
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fracture);        // Effect.Debuff.Fracture
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Survival);        // Effect.Debuff.Survival
		}

		// Grenade effect tags (Effect.Grenade.*)
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GrenadeFlashbang);         // Effect.Grenade.Flashbang
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GrenadeFlashbangPartial);  // Effect.Grenade.Flashbang.Partial
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GrenadeIncendiary);        // Effect.Grenade.Incendiary
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GrenadeIncendiaryZone);    // Effect.Grenade.Incendiary.Zone
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GrenadeIncendiaryArmorBypass); // Effect.Grenade.Incendiary.ArmorBypass
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GrenadeShrapnel);          // Effect.Grenade.Shrapnel

		//--------------------------------------------------------------
		// DoT Effect Tags (Effect.DoT.*)
		//--------------------------------------------------------------
		namespace DoT
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root);                  // Effect.DoT (parent tag)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BleedLight);            // Effect.DoT.Bleed.Light
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BleedHeavy);            // Effect.DoT.Bleed.Heavy
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Burn);                  // Effect.DoT.Burn
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BurnArmorBypass);       // Effect.DoT.Burn.ArmorBypass
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Poison);                // Effect.DoT.Poison
		}

		//--------------------------------------------------------------
		// Cure Effect Tags (Effect.Cure.*)
		// Used by medical items to cure specific status effects
		// @see FSuspenseCoreStatusEffectAttributeRow.CureEffectTags
		//--------------------------------------------------------------
		namespace Cure
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Bleeding);              // Effect.Cure.Bleeding (all bleeding)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BleedingHeavy);         // Effect.Cure.Bleeding.Heavy
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fire);                  // Effect.Cure.Fire
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Poison);                // Effect.Cure.Poison
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fracture);              // Effect.Cure.Fracture
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Dehydration);           // Effect.Cure.Dehydration
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Exhaustion);            // Effect.Cure.Exhaustion
		}

		//--------------------------------------------------------------
		// Buff Effect Tags (Effect.Buff.*)
		// Positive effects applied by consumables
		//--------------------------------------------------------------
		namespace Buff
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Regenerating);          // Effect.Buff.Regenerating
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Painkiller);            // Effect.Buff.Painkiller
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Adrenaline);            // Effect.Buff.Adrenaline
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fortified);             // Effect.Buff.Fortified
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Heal);                  // Effect.Buff.Heal
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combat);                // Effect.Buff.Combat
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Defense);               // Effect.Buff.Defense
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement);              // Effect.Buff.Movement
		}
	}

	//==================================================================
	// GAMEPLAYCUE TAGS - Visual/Audio effects
	//==================================================================
	namespace GameplayCue
	{
		// Grenade cues (GameplayCue.Grenade.*)
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GrenadeFlashbang);         // GameplayCue.Grenade.Flashbang
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GrenadeFlashbangPartial);  // GameplayCue.Grenade.Flashbang.Partial
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GrenadeBurn);              // GameplayCue.Grenade.Burn

		//--------------------------------------------------------------
		// DoT GameplayCues (GameplayCue.DoT.*)
		// Used for visual/audio feedback during DoT effects
		//--------------------------------------------------------------
		namespace DoT
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BleedLight);   // GameplayCue.DoT.Bleed.Light (blood splatter)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BleedHeavy);   // GameplayCue.DoT.Bleed.Heavy (blood trail)
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Burn);         // GameplayCue.DoT.Burn (character on fire)
		}
	}

	//==================================================================
	// UI TAGS
	//==================================================================
	namespace UI
	{
		namespace Screen
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character);
		}

		namespace Panel
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Equipment);
		}

		namespace Tab
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Inventory);
		}

		namespace TabBar
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character);
		}

		namespace HUD
		{
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Crosshair);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HealthStamina);
			BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(WeaponInfo);
		}
	}

	//==================================================================
	// EQUIPMENT SLOT TAGS
	//==================================================================
	namespace EquipmentSlot
	{
		// Weapons
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(PrimaryWeapon);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SecondaryWeapon);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sidearm);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Holster);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Melee);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Scabbard);

		// Head gear
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Headwear);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Earpiece);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Eyewear);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(FaceCover);

		// Body gear
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BodyArmor);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TacticalRig);

		// Storage
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Backpack);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SecureContainer);

		// Quick slots
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(QuickSlot1);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(QuickSlot2);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(QuickSlot3);
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(QuickSlot4);

		// Special
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Armband);
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
