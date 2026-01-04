// SuspenseCoreEquipmentNativeTags.h
// Copyright SuspenseCore Team. All Rights Reserved.
//
// Native GameplayTags for Equipment System - Compile-time tag registration
// following UE5 best practices for AAA MMO shooters.
//
// Usage:
//   #include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
//   EventBus->Publish(SuspenseCoreEquipmentTags::Event::Equipped, Payload);

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

//========================================
// Native Tag Module Declaration
// All tags are declared within SuspenseCoreEquipmentTags namespace
// to avoid global namespace pollution and duplicate symbols
//========================================

namespace SuspenseCoreEquipmentTags
{
    //========================================
    // SERVICE IDENTIFICATION TAGS
    // Used by ServiceLocator for DI resolution
    //========================================
    namespace Service
    {
        // Root service tag
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Service_Equipment);

        // Individual service tags
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Service_Equipment_Ability);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Service_Equipment_Data);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Service_Equipment_Network);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Service_Equipment_Operations);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Service_Equipment_Validation);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Service_Equipment_Visualization);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Service_Equipment_Prediction);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Service_Equipment_Security);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Service_Equipment_Rules);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Service_Equipment_Transaction);

        // Supporting services
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Service_ActorFactory);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Service_AttachmentSystem);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Service_VisualController);
    }

    //========================================
    // EVENT TAGS - EventBus Communication
    //========================================
    namespace Event
    {
        // Root event tag
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event);

        //--- Item Events ---
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Equipped);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Unequipped);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_ItemEquipped);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_ItemUnequipped);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_SlotSwitched);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Commit);

        //--- Ability Events ---
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Ability);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Ability_Refresh);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Ability_Granted);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Ability_Removed);

        //--- Operation Events ---
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Operation);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Operation_Queued);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Operation_Started);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Operation_Completed);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Operation_Cancelled);

        //--- Validation Events ---
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Validation);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Validation_Changed);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Validation_Started);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Validation_Passed);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Validation_Failed);

        //--- Data Events ---
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Data);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Data_Changed);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Data_Synced);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Data_Snapshot);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Data_Delta);

        //--- Network Events ---
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Network);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Network_Result);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Network_Timeout);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Network_SecurityViolation);

        //--- Visual Events ---
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Visual);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Visual_RefreshAll);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Visual_Spawned);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Visual_Attached);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Visual_Detached);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Visual_StateChanged);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Visual_RequestSync);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Visual_Effect);

        //--- Weapon Events ---
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_Fired);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_FireModeChanged);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_ReloadStart);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_ReloadEnd);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_SpreadUpdated);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_AmmoChanged);

        //--- Weapon Stance Events (Combat States) ---
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_Stance);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_Stance_Changed);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_Stance_AimStarted);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_Stance_AimEnded);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_Stance_FireStarted);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_Stance_FireEnded);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_Stance_ReloadStarted);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_Stance_ReloadEnded);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_Stance_HoldBreathStarted);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_Stance_HoldBreathEnded);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_Stance_Drawn);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_Stance_Holstered);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_Stance_BlockStarted);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Weapon_Stance_BlockEnded);

        //--- Property Events ---
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_PropertyChanged);

        //--- Slot Events (used by DataStore for EventBus) ---
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_SlotUpdated);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Updated);
    }

    //========================================
    // WEAPON STATE TAGS
    // Used for weapon state machine
    //========================================
    namespace WeaponState
    {
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_State);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_State_Ready);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_State_Holstered);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_State_Drawing);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_State_Holstering);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_State_Firing);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_State_Reloading);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_State_Empty);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_State_Jammed);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_State_Inspecting);
    }

    //========================================
    // WEAPON ARCHETYPE TAGS
    // Used for Animation System stance selection
    // DataTable row names should match these tags
    //========================================
    namespace Archetype
    {
        // Root weapon tag
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon);

        // Rifles
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Rifle);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Rifle_Assault);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Rifle_Sniper);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Rifle_DMR);

        // Pistols
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Pistol);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Pistol_Semi);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Pistol_Revolver);

        // SMGs
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_SMG);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_SMG_Compact);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_SMG_Full);

        // Shotguns
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Shotgun);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Shotgun_Pump);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Shotgun_Auto);

        // Heavy
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Heavy);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Heavy_LMG);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Heavy_Launcher);

        // Melee
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Melee);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Melee_Knife);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Melee_Blunt);

        // Throwables
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Throwable);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Throwable_Grenade);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Throwable_Smoke);
    }

    //========================================
    // OPERATION TYPE TAGS
    // Used for operation classification
    //========================================
    namespace Operation
    {
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Operation);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Operation_Equip);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Operation_Unequip);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Operation_Swap);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Operation_Move);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Operation_Use);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Operation_Drop);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Operation_Batch);

        // Extended operation types (used by TransactionProcessor)
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Operation_Set);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Operation_Clear);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Operation_MoveSource);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Operation_MoveTarget);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Operation_Upgrade);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Operation_Modify);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Operation_Global);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Operation_Repair);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Operation_Combine);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Operation_Split);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Operation_Unknown);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Operation_Completed);
    }

    //========================================
    // STATE TAGS
    // Used for equipment state identification
    //========================================
    namespace State
    {
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_State);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_State_Idle);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_State_Inactive);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_State_Equipped);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_State_Ready);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_State_Equipping);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_State_Unequipping);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_State_InUse);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_State_Cooldown);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_State_Locked);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_State_Disabled);
    }

    //========================================
    // SLOT TYPE TAGS
    // Used for slot classification (AAA MMO FPS style)
    //========================================
    namespace Slot
    {
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_None);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Unknown);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Updated);

        // Weapons
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Primary);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_PrimaryWeapon);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Secondary);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_SecondaryWeapon);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Sidearm);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Holster);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Melee);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Scabbard);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Grenade);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Tactical);

        // Head gear
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Armor_Head);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Headwear);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Earpiece);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Eyewear);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_FaceCover);

        // Body gear
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Armor_Body);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_BodyArmor);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_TacticalRig);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Armor_Legs);

        // Storage
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Backpack);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_SecureContainer);

        // Quick slots
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_QuickSlot1);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_QuickSlot2);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_QuickSlot3);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_QuickSlot4);

        // Special
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Armband);
    }

    //========================================
    // UI TAGS
    // Used for UI-Equipment communication
    //========================================
    namespace UI
    {
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_UI_Equipment);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_UI_Equipment_DataReady);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_UI_Equipment_SlotClicked);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_UI_Equipment_SlotHovered);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_UI_Equipment_DragStarted);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_UI_Equipment_DragEnded);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_UI_Equipment_ContextMenu);
    }

    //========================================
    // VALIDATION RULE TAGS
    // Used by slot validator for rule identification
    //========================================
    namespace Validation
    {
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Validation_Rule);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Validation_Rule_ItemType);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Validation_Rule_Level);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Validation_Rule_Weight);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Validation_Rule_Unique);

        // Validation error tags
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Validation_Error);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Validation_Error_InvalidSlotConfig);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Validation_Error_RequirementNotMet);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Validation_Error_InvalidItem);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Validation_Error_InvalidSlotIndex);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Validation_Error_Conflict);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Validation_Error_NoItemData);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Validation_Error_TypeDisallowed);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Validation_Error_TypeMatrix);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Validation_Error_TooHeavy);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Validation_Error_TooLarge);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Validation_Error_UniqueGroup);
    }

    //========================================
    // ITEM TYPE TAGS
    // Used for item classification in equipment slots
    //========================================
    namespace Item
    {
        // Root item tag
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item);

        // Weapon tags
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Weapon);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Weapon_Rifle);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Weapon_AR);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Weapon_DMR);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Weapon_SR);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Weapon_Sniper);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Weapon_LMG);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Weapon_SMG);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Weapon_Shotgun);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Weapon_PDW);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Weapon_Primary);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Weapon_Pistol);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Weapon_Revolver);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Weapon_Melee);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Weapon_Melee_Knife);

        // Gear tags (head, body, storage, special)
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Gear);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Gear_Headwear);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Gear_Earpiece);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Gear_Eyewear);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Gear_FaceCover);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Gear_TacticalRig);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Gear_Backpack);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Gear_SecureContainer);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Gear_Armband);

        // Armor tags
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Armor);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Armor_Helmet);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Armor_BodyArmor);

        // Consumable/utility tags
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Consumable);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Medical);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Throwable);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Ammo);

        // Magazine tags (Tarkov-style)
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Magazine);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Magazine);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Consumable);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Grenade);
    }

    //========================================
    // MAGAZINE SYSTEM TAGS (Tarkov-Style)
    //========================================
    namespace Magazine
    {
        // Magazine events
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Magazine);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Magazine_Inserted);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Magazine_Ejected);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Magazine_Swapped);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Magazine_RoundsChanged);

        // Ammo loading events (Tarkov-style drag&drop / quick load)
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Ammo_LoadStarted);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Ammo_LoadCompleted);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Ammo_LoadCancelled);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Ammo_UnloadStarted);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Ammo_UnloadCompleted);

        // Chamber events
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Chamber);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Chamber_Chambered);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Chamber_Ejected);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Chamber_Fired);

        // Reload events
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Reload);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Reload_Tactical);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Reload_Empty);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_Reload_Emergency);
    }

    //========================================
    // ATTRIBUTESET TYPE TAGS
    // Used for categorizing AttributeSets in equipment component
    //========================================
    namespace AttributeSet
    {
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttributeSet);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttributeSet_Weapon);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttributeSet_Ammo);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttributeSet_Armor);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttributeSet_Equipment);
    }

    //========================================
    // QUICKSLOT SYSTEM TAGS
    //========================================
    namespace QuickSlot
    {
        // QuickSlot root tag
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_QuickSlot);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_QuickSlot_1);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_QuickSlot_2);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_QuickSlot_3);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_QuickSlot_4);

        // QuickSlot events
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_QuickSlot);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_QuickSlot_Assigned);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_QuickSlot_Cleared);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_QuickSlot_Used);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_QuickSlot_CooldownStarted);
        EQUIPMENTSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Event_QuickSlot_CooldownEnded);
    }

} // namespace SuspenseCoreEquipmentTags

//========================================
// CONVENIENCE ALIASES
// For shorter access in implementation files
//========================================
namespace EquipTags = SuspenseCoreEquipmentTags;
