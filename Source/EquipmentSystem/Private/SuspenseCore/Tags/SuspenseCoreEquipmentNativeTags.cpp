// SuspenseCoreEquipmentNativeTags.cpp
// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"

//========================================
// Native Tag Definitions
// Tags are registered at module load time (before CDO construction)
// All tags defined within SuspenseCoreEquipmentTags namespace
//========================================

namespace SuspenseCoreEquipmentTags
{
    //========================================
    // SERVICE IDENTIFICATION TAGS - Following SuspenseCore.Service.* convention
    //========================================
    namespace Service
    {
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_Equipment, "SuspenseCore.Service.Equipment");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_Equipment_Ability, "SuspenseCore.Service.Equipment.Ability");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_Equipment_Data, "SuspenseCore.Service.Equipment.Data");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_Equipment_Network, "SuspenseCore.Service.Equipment.Network");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_Equipment_Operations, "SuspenseCore.Service.Equipment.Operations");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_Equipment_Validation, "SuspenseCore.Service.Equipment.Validation");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_Equipment_Visualization, "SuspenseCore.Service.Equipment.Visualization");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_Equipment_Prediction, "SuspenseCore.Service.Equipment.Prediction");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_Equipment_Security, "SuspenseCore.Service.Equipment.Security");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_Equipment_Rules, "SuspenseCore.Service.Equipment.Rules");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_Equipment_Transaction, "SuspenseCore.Service.Equipment.Transaction");

        // Supporting services
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_ActorFactory, "SuspenseCore.Service.Equipment.ActorFactory");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_AttachmentSystem, "SuspenseCore.Service.Equipment.AttachmentSystem");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_VisualController, "SuspenseCore.Service.Equipment.VisualController");
    }

    //========================================
    // EVENT TAGS - Following SuspenseCore.Event.* convention (BestPractices.md)
    //========================================
    namespace Event
    {
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event, "SuspenseCore.Event.Equipment");

        // Item Events
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Equipped, "SuspenseCore.Event.Equipment.Equipped");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Unequipped, "SuspenseCore.Event.Equipment.Unequipped");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_ItemEquipped, "SuspenseCore.Event.Equipment.ItemEquipped");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_ItemUnequipped, "SuspenseCore.Event.Equipment.ItemUnequipped");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_SlotSwitched, "SuspenseCore.Event.Equipment.SlotSwitched");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Commit, "SuspenseCore.Event.Equipment.Commit");

        // Ability Events
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Ability, "SuspenseCore.Event.Equipment.Ability");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Ability_Refresh, "SuspenseCore.Event.Equipment.Ability.Refresh");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Ability_Granted, "SuspenseCore.Event.Equipment.Ability.Granted");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Ability_Removed, "SuspenseCore.Event.Equipment.Ability.Removed");

        // Operation Events
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Operation, "SuspenseCore.Event.Equipment.Operation");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Operation_Queued, "SuspenseCore.Event.Equipment.Operation.Queued");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Operation_Started, "SuspenseCore.Event.Equipment.Operation.Started");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Operation_Completed, "SuspenseCore.Event.Equipment.Operation.Completed");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Operation_Cancelled, "SuspenseCore.Event.Equipment.Operation.Cancelled");

        // Validation Events
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Validation, "SuspenseCore.Event.Equipment.Validation");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Validation_Changed, "SuspenseCore.Event.Equipment.Validation.Changed");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Validation_Started, "SuspenseCore.Event.Equipment.Validation.Started");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Validation_Passed, "SuspenseCore.Event.Equipment.Validation.Passed");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Validation_Failed, "SuspenseCore.Event.Equipment.Validation.Failed");

        // Data Events
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Data, "SuspenseCore.Event.Equipment.Data");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Data_Changed, "SuspenseCore.Event.Equipment.Data.Changed");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Data_Synced, "SuspenseCore.Event.Equipment.Data.Synced");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Data_Snapshot, "SuspenseCore.Event.Equipment.Data.Snapshot");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Data_Delta, "SuspenseCore.Event.Equipment.Data.Delta");

        // Network Events
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Network, "SuspenseCore.Event.Equipment.Network");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Network_Result, "SuspenseCore.Event.Equipment.Network.Result");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Network_Timeout, "SuspenseCore.Event.Equipment.Network.Timeout");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Network_SecurityViolation, "SuspenseCore.Event.Equipment.Network.SecurityViolation");

        // Visual Events
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Visual, "SuspenseCore.Event.Equipment.Visual");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Visual_RefreshAll, "SuspenseCore.Event.Equipment.Visual.RefreshAll");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Visual_Spawned, "SuspenseCore.Event.Equipment.Visual.Spawned");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Visual_Attached, "SuspenseCore.Event.Equipment.Visual.Attached");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Visual_Detached, "SuspenseCore.Event.Equipment.Visual.Detached");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Visual_StateChanged, "SuspenseCore.Event.Equipment.Visual.StateChanged");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Visual_RequestSync, "SuspenseCore.Event.Equipment.Visual.RequestSync");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Visual_Effect, "SuspenseCore.Event.Equipment.Visual.Effect");

        // Weapon Events
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon, "SuspenseCore.Event.Equipment.Weapon");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_Fired, "SuspenseCore.Event.Equipment.Weapon.Fired");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_FireModeChanged, "SuspenseCore.Event.Equipment.Weapon.FireModeChanged");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_ReloadStart, "SuspenseCore.Event.Equipment.Weapon.ReloadStart");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_ReloadEnd, "SuspenseCore.Event.Equipment.Weapon.ReloadEnd");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_SpreadUpdated, "SuspenseCore.Event.Equipment.Weapon.SpreadUpdated");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_AmmoChanged, "SuspenseCore.Event.Equipment.Weapon.AmmoChanged");

        // Weapon Stance Events (Combat States)
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_Stance, "SuspenseCore.Event.Equipment.Weapon.Stance");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_Stance_Changed, "SuspenseCore.Event.Equipment.Weapon.Stance.Changed");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_Stance_AimStarted, "SuspenseCore.Event.Equipment.Weapon.Stance.AimStarted");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_Stance_AimEnded, "SuspenseCore.Event.Equipment.Weapon.Stance.AimEnded");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_Stance_FireStarted, "SuspenseCore.Event.Equipment.Weapon.Stance.FireStarted");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_Stance_FireEnded, "SuspenseCore.Event.Equipment.Weapon.Stance.FireEnded");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_Stance_ReloadStarted, "SuspenseCore.Event.Equipment.Weapon.Stance.ReloadStarted");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_Stance_ReloadEnded, "SuspenseCore.Event.Equipment.Weapon.Stance.ReloadEnded");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_Stance_HoldBreathStarted, "SuspenseCore.Event.Equipment.Weapon.Stance.HoldBreathStarted");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_Stance_HoldBreathEnded, "SuspenseCore.Event.Equipment.Weapon.Stance.HoldBreathEnded");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_Stance_Drawn, "SuspenseCore.Event.Equipment.Weapon.Stance.Drawn");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_Stance_Holstered, "SuspenseCore.Event.Equipment.Weapon.Stance.Holstered");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_Stance_BlockStarted, "SuspenseCore.Event.Equipment.Weapon.Stance.BlockStarted");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_Stance_BlockEnded, "SuspenseCore.Event.Equipment.Weapon.Stance.BlockEnded");

        // Property Events
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_PropertyChanged, "Equipment.Event.PropertyChanged");

        // Slot Events (used by DataStore for EventBus)
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_SlotUpdated, "Equipment.Event.SlotUpdated");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Updated, "Equipment.Event.Updated");
    }

    //========================================
    // WEAPON STATE TAGS
    //========================================
    namespace WeaponState
    {
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_State, "Weapon.State");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_State_Ready, "Weapon.State.Ready");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_State_Holstered, "Weapon.State.Holstered");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_State_Drawing, "Weapon.State.Drawing");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_State_Holstering, "Weapon.State.Holstering");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_State_Firing, "Weapon.State.Firing");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_State_Reloading, "Weapon.State.Reloading");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_State_Empty, "Weapon.State.Empty");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_State_Jammed, "Weapon.State.Jammed");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_State_Inspecting, "Weapon.State.Inspecting");
    }

    //========================================
    // WEAPON ARCHETYPE TAGS
    // Used for Animation System stance selection
    //========================================
    namespace Archetype
    {
        // Root weapon tag
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon, "Weapon");

        // Rifles
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Rifle, "Weapon.Rifle");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Rifle_Assault, "Weapon.Rifle.Assault");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Rifle_Sniper, "Weapon.Rifle.Sniper");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Rifle_DMR, "Weapon.Rifle.DMR");

        // Pistols
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Pistol, "Weapon.Pistol");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Pistol_Semi, "Weapon.Pistol.Semi");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Pistol_Revolver, "Weapon.Pistol.Revolver");

        // SMGs
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_SMG, "Weapon.SMG");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_SMG_Compact, "Weapon.SMG.Compact");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_SMG_Full, "Weapon.SMG.Full");

        // Shotguns
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Shotgun, "Weapon.Shotgun");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Shotgun_Pump, "Weapon.Shotgun.Pump");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Shotgun_Auto, "Weapon.Shotgun.Auto");

        // Heavy
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Heavy, "Weapon.Heavy");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Heavy_LMG, "Weapon.Heavy.LMG");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Heavy_Launcher, "Weapon.Heavy.Launcher");

        // Melee
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Melee, "Weapon.Melee");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Melee_Knife, "Weapon.Melee.Knife");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Melee_Blunt, "Weapon.Melee.Blunt");

        // Throwables
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Throwable, "Weapon.Throwable");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Throwable_Grenade, "Weapon.Throwable.Grenade");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Throwable_Smoke, "Weapon.Throwable.Smoke");
    }

    //========================================
    // OPERATION TYPE TAGS
    //========================================
    namespace Operation
    {
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Operation, "Equipment.Operation");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Operation_Equip, "Equipment.Operation.Equip");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Operation_Unequip, "Equipment.Operation.Unequip");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Operation_Swap, "Equipment.Operation.Swap");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Operation_Move, "Equipment.Operation.Move");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Operation_Use, "Equipment.Operation.Use");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Operation_Drop, "Equipment.Operation.Drop");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Operation_Batch, "Equipment.Operation.Batch");

        // Extended operation types (used by TransactionProcessor)
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Operation_Set, "Equipment.Operation.Set");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Operation_Clear, "Equipment.Operation.Clear");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Operation_MoveSource, "Equipment.Operation.MoveSource");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Operation_MoveTarget, "Equipment.Operation.MoveTarget");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Operation_Upgrade, "Equipment.Operation.Upgrade");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Operation_Modify, "Equipment.Operation.Modify");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Operation_Global, "Equipment.Operation.Global");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Operation_Repair, "Equipment.Operation.Repair");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Operation_Combine, "Equipment.Operation.Combine");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Operation_Split, "Equipment.Operation.Split");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Operation_Unknown, "Equipment.Operation.Unknown");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Operation_Completed, "Equipment.Operation.Completed");
    }

    //========================================
    // STATE TAGS
    //========================================
    namespace State
    {
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_State, "Equipment.State");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_State_Idle, "Equipment.State.Idle");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_State_Inactive, "Equipment.State.Inactive");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_State_Equipped, "Equipment.State.Equipped");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_State_Ready, "Equipment.State.Ready");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_State_Equipping, "Equipment.State.Equipping");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_State_Unequipping, "Equipment.State.Unequipping");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_State_InUse, "Equipment.State.InUse");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_State_Cooldown, "Equipment.State.Cooldown");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_State_Locked, "Equipment.State.Locked");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_State_Disabled, "Equipment.State.Disabled");
    }

    //========================================
    // DELTA TAGS - For change tracking
    //========================================
    namespace Delta
    {
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Delta, "Equipment.Delta");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Delta_Initialize, "Equipment.Delta.Initialize");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Delta_ItemSet, "Equipment.Delta.ItemSet");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Delta_ItemClear, "Equipment.Delta.ItemClear");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Delta_ActiveWeapon, "Equipment.Delta.ActiveWeapon");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Delta_StateChange, "Equipment.Delta.StateChange");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Delta_SnapshotRestore, "Equipment.Delta.SnapshotRestore");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Delta_Reset, "Equipment.Delta.Reset");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Delta_Revert, "Equipment.Delta.Revert");
    }

    //========================================
    // REASON TAGS - For operation reasons
    //========================================
    namespace Reason
    {
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Reason, "Equipment.Reason");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Reason_Initialize, "Equipment.Reason.Initialize");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Reason_DirectSet, "Equipment.Reason.DirectSet");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Reason_DirectClear, "Equipment.Reason.DirectClear");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Reason_ActiveChange, "Equipment.Reason.ActiveChange");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Reason_StateTransition, "Equipment.Reason.StateTransition");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Reason_SnapshotRestore, "Equipment.Reason.SnapshotRestore");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Reason_ResetToDefault, "Equipment.Reason.ResetToDefault");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Reason_Rollback, "Equipment.Reason.Rollback");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Reason_Transaction, "Equipment.Reason.Transaction");
    }

    //========================================
    // SLOT TYPE TAGS (AAA MMO FPS style)
    //========================================
    namespace Slot
    {
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot, "Equipment.Slot");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_None, "Equipment.Slot.None");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Unknown, "Equipment.Slot.Unknown");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Updated, "Equipment.Slot.Updated");

        // Weapons
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Primary, "Equipment.Slot.Primary");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_PrimaryWeapon, "Equipment.Slot.PrimaryWeapon");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Secondary, "Equipment.Slot.Secondary");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_SecondaryWeapon, "Equipment.Slot.SecondaryWeapon");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Sidearm, "Equipment.Slot.Sidearm");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Holster, "Equipment.Slot.Holster");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Melee, "Equipment.Slot.Melee");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Scabbard, "Equipment.Slot.Scabbard");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Grenade, "Equipment.Slot.Grenade");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Tactical, "Equipment.Slot.Tactical");

        // Head gear
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Armor_Head, "Equipment.Slot.Armor.Head");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Headwear, "Equipment.Slot.Headwear");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Earpiece, "Equipment.Slot.Earpiece");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Eyewear, "Equipment.Slot.Eyewear");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_FaceCover, "Equipment.Slot.FaceCover");

        // Body gear
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Armor_Body, "Equipment.Slot.Armor.Body");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_BodyArmor, "Equipment.Slot.BodyArmor");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_TacticalRig, "Equipment.Slot.TacticalRig");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Armor_Legs, "Equipment.Slot.Armor.Legs");

        // Storage
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Backpack, "Equipment.Slot.Backpack");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_SecureContainer, "Equipment.Slot.SecureContainer");

        // Quick slots
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_QuickSlot1, "Equipment.Slot.QuickSlot1");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_QuickSlot2, "Equipment.Slot.QuickSlot2");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_QuickSlot3, "Equipment.Slot.QuickSlot3");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_QuickSlot4, "Equipment.Slot.QuickSlot4");

        // Special
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Armband, "Equipment.Slot.Armband");
    }

    //========================================
    // UI TAGS - Following SuspenseCore.Event.UI.* convention (BestPractices.md)
    //========================================
    namespace UI
    {
        UE_DEFINE_GAMEPLAY_TAG(TAG_UI_Equipment, "SuspenseCore.Event.UI.Equipment");
        UE_DEFINE_GAMEPLAY_TAG(TAG_UI_Equipment_DataReady, "SuspenseCore.Event.UI.Equipment.DataReady");
        UE_DEFINE_GAMEPLAY_TAG(TAG_UI_Equipment_SlotClicked, "SuspenseCore.Event.UI.Equipment.SlotClicked");
        UE_DEFINE_GAMEPLAY_TAG(TAG_UI_Equipment_SlotHovered, "SuspenseCore.Event.UI.Equipment.SlotHovered");
        UE_DEFINE_GAMEPLAY_TAG(TAG_UI_Equipment_DragStarted, "SuspenseCore.Event.UI.Equipment.DragStarted");
        UE_DEFINE_GAMEPLAY_TAG(TAG_UI_Equipment_DragEnded, "SuspenseCore.Event.UI.Equipment.DragEnded");
        UE_DEFINE_GAMEPLAY_TAG(TAG_UI_Equipment_ContextMenu, "SuspenseCore.Event.UI.Equipment.ContextMenu");
    }

    //========================================
    // VALIDATION RULE TAGS
    //========================================
    namespace Validation
    {
        UE_DEFINE_GAMEPLAY_TAG(TAG_Validation_Rule, "Validation.Rule");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Validation_Rule_ItemType, "Validation.Rule.ItemType");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Validation_Rule_Level, "Validation.Rule.Level");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Validation_Rule_Weight, "Validation.Rule.Weight");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Validation_Rule_Unique, "Validation.Rule.Unique");

        // Validation error tags
        UE_DEFINE_GAMEPLAY_TAG(TAG_Validation_Error, "Validation.Error");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Validation_Error_InvalidSlotConfig, "Validation.Error.InvalidSlotConfig");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Validation_Error_RequirementNotMet, "Validation.Error.RequirementNotMet");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Validation_Error_InvalidItem, "Validation.Error.InvalidItem");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Validation_Error_InvalidSlotIndex, "Validation.Error.InvalidSlotIndex");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Validation_Error_Conflict, "Validation.Error.Conflict");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Validation_Error_NoItemData, "Validation.Error.NoItemData");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Validation_Error_TypeDisallowed, "Validation.Error.TypeDisallowed");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Validation_Error_TypeMatrix, "Validation.Error.TypeMatrix");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Validation_Error_TooHeavy, "Validation.Error.TooHeavy");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Validation_Error_TooLarge, "Validation.Error.TooLarge");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Validation_Error_UniqueGroup, "Validation.Error.UniqueGroup");
    }

    //========================================
    // ITEM TYPE TAGS
    //========================================
    namespace Item
    {
        // Root item tag
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item, "Item");

        // Weapon tags
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Weapon, "Item.Weapon");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Weapon_Rifle, "Item.Weapon.Rifle");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Weapon_AR, "Item.Weapon.AR");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Weapon_DMR, "Item.Weapon.DMR");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Weapon_SR, "Item.Weapon.SR");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Weapon_Sniper, "Item.Weapon.Sniper");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Weapon_LMG, "Item.Weapon.LMG");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Weapon_SMG, "Item.Weapon.SMG");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Weapon_Shotgun, "Item.Weapon.Shotgun");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Weapon_PDW, "Item.Weapon.PDW");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Weapon_Primary, "Item.Weapon.Primary");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Weapon_Pistol, "Item.Weapon.Pistol");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Weapon_Revolver, "Item.Weapon.Revolver");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Weapon_Melee, "Item.Weapon.Melee");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Weapon_Melee_Knife, "Item.Weapon.Melee.Knife");

        // Gear tags (head, body, storage, special)
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Gear, "Item.Gear");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Gear_Headwear, "Item.Gear.Headwear");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Gear_Earpiece, "Item.Gear.Earpiece");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Gear_Eyewear, "Item.Gear.Eyewear");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Gear_FaceCover, "Item.Gear.FaceCover");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Gear_TacticalRig, "Item.Gear.TacticalRig");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Gear_Backpack, "Item.Gear.Backpack");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Gear_SecureContainer, "Item.Gear.SecureContainer");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Gear_Armband, "Item.Gear.Armband");

        // Armor tags
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Armor, "Item.Armor");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Armor_Helmet, "Item.Armor.Helmet");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Armor_BodyArmor, "Item.Armor.BodyArmor");

        // Consumable/utility tags
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Consumable, "Item.Consumable");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Medical, "Item.Medical");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Throwable, "Item.Throwable");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Ammo, "Item.Ammo");

        // Magazine tags (Tarkov-style)
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Magazine, "Item.Magazine");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Magazine, "Item.Category.Magazine");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Consumable, "Item.Category.Consumable");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Grenade, "Item.Category.Grenade");
    }

    //========================================
    // MAGAZINE SYSTEM TAGS (Tarkov-Style)
    //========================================
    namespace Magazine
    {
        // Magazine events
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Magazine, "SuspenseCore.Event.Equipment.Magazine");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Magazine_Inserted, "SuspenseCore.Event.Equipment.Magazine.Inserted");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Magazine_Ejected, "SuspenseCore.Event.Equipment.Magazine.Ejected");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Magazine_Swapped, "SuspenseCore.Event.Equipment.Magazine.Swapped");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Magazine_RoundsChanged, "SuspenseCore.Event.Equipment.Magazine.RoundsChanged");

        // Ammo loading events (Tarkov-style drag&drop / quick load)
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Ammo_LoadStarted, "SuspenseCore.Event.Equipment.Ammo.LoadStarted");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Ammo_RoundLoaded, "SuspenseCore.Event.Equipment.Ammo.RoundLoaded");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Ammo_LoadCompleted, "SuspenseCore.Event.Equipment.Ammo.LoadCompleted");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Ammo_LoadCancelled, "SuspenseCore.Event.Equipment.Ammo.LoadCancelled");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Ammo_UnloadStarted, "SuspenseCore.Event.Equipment.Ammo.UnloadStarted");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Ammo_RoundUnloaded, "SuspenseCore.Event.Equipment.Ammo.RoundUnloaded");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Ammo_UnloadCompleted, "SuspenseCore.Event.Equipment.Ammo.UnloadCompleted");

        // Chamber events
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Chamber, "SuspenseCore.Event.Equipment.Chamber");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Chamber_Chambered, "SuspenseCore.Event.Equipment.Chamber.Chambered");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Chamber_Ejected, "SuspenseCore.Event.Equipment.Chamber.Ejected");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Chamber_Fired, "SuspenseCore.Event.Equipment.Chamber.Fired");

        // Reload events
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Reload, "SuspenseCore.Event.Equipment.Reload");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Reload_Tactical, "SuspenseCore.Event.Equipment.Reload.Tactical");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Reload_Empty, "SuspenseCore.Event.Equipment.Reload.Empty");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Reload_Emergency, "SuspenseCore.Event.Equipment.Reload.Emergency");
    }

    //========================================
    // ATTRIBUTESET TYPE TAGS
    // Used for categorizing AttributeSets in equipment component
    //========================================
    namespace AttributeSet
    {
        UE_DEFINE_GAMEPLAY_TAG(TAG_AttributeSet, "AttributeSet");
        UE_DEFINE_GAMEPLAY_TAG(TAG_AttributeSet_Weapon, "AttributeSet.Weapon");
        UE_DEFINE_GAMEPLAY_TAG(TAG_AttributeSet_Ammo, "AttributeSet.Ammo");
        UE_DEFINE_GAMEPLAY_TAG(TAG_AttributeSet_Armor, "AttributeSet.Armor");
        UE_DEFINE_GAMEPLAY_TAG(TAG_AttributeSet_Equipment, "AttributeSet.Equipment");
    }

    //========================================
    // QUICKSLOT SYSTEM TAGS
    //========================================
    namespace QuickSlot
    {
        // QuickSlot root tag
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_QuickSlot, "Equipment.QuickSlot");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_QuickSlot_1, "Equipment.QuickSlot.1");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_QuickSlot_2, "Equipment.QuickSlot.2");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_QuickSlot_3, "Equipment.QuickSlot.3");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_QuickSlot_4, "Equipment.QuickSlot.4");

        // QuickSlot events
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_QuickSlot, "SuspenseCore.Event.Equipment.QuickSlot");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_QuickSlot_Assigned, "SuspenseCore.Event.Equipment.QuickSlot.Assigned");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_QuickSlot_Cleared, "SuspenseCore.Event.Equipment.QuickSlot.Cleared");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_QuickSlot_Used, "SuspenseCore.Event.Equipment.QuickSlot.Used");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_QuickSlot_CooldownStarted, "SuspenseCore.Event.Equipment.QuickSlot.CooldownStarted");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_QuickSlot_CooldownEnded, "SuspenseCore.Event.Equipment.QuickSlot.CooldownEnded");
    }

} // namespace SuspenseCoreEquipmentTags
