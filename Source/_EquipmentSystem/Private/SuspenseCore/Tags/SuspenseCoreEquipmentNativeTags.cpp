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

        // Weapon Events
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon, "SuspenseCore.Event.Equipment.Weapon");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_Fired, "SuspenseCore.Event.Equipment.Weapon.Fired");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_FireModeChanged, "SuspenseCore.Event.Equipment.Weapon.FireModeChanged");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_ReloadStart, "SuspenseCore.Event.Equipment.Weapon.ReloadStart");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_ReloadEnd, "SuspenseCore.Event.Equipment.Weapon.ReloadEnd");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_SpreadUpdated, "SuspenseCore.Event.Equipment.Weapon.SpreadUpdated");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_AmmoChanged, "SuspenseCore.Event.Equipment.Weapon.AmmoChanged");
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
    }

    //========================================
    // STATE TAGS
    //========================================
    namespace State
    {
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_State, "Equipment.State");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_State_Idle, "Equipment.State.Idle");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_State_Equipping, "Equipment.State.Equipping");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_State_Unequipping, "Equipment.State.Unequipping");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_State_InUse, "Equipment.State.InUse");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_State_Cooldown, "Equipment.State.Cooldown");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_State_Locked, "Equipment.State.Locked");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_State_Disabled, "Equipment.State.Disabled");
    }

    //========================================
    // SLOT TYPE TAGS
    //========================================
    namespace Slot
    {
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot, "Equipment.Slot");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Primary, "Equipment.Slot.Primary");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Secondary, "Equipment.Slot.Secondary");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Sidearm, "Equipment.Slot.Sidearm");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Melee, "Equipment.Slot.Melee");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Grenade, "Equipment.Slot.Grenade");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Tactical, "Equipment.Slot.Tactical");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Armor_Head, "Equipment.Slot.Armor.Head");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Armor_Body, "Equipment.Slot.Armor.Body");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Armor_Legs, "Equipment.Slot.Armor.Legs");
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

} // namespace SuspenseCoreEquipmentTags
