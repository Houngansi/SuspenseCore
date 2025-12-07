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
    // SERVICE IDENTIFICATION TAGS
    //========================================
    namespace Service
    {
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_Equipment, "Service.Equipment");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_Equipment_Ability, "Service.Equipment.Ability");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_Equipment_Data, "Service.Equipment.Data");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_Equipment_Network, "Service.Equipment.Network");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_Equipment_Operations, "Service.Equipment.Operations");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_Equipment_Validation, "Service.Equipment.Validation");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_Equipment_Visualization, "Service.Equipment.Visualization");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_Equipment_Prediction, "Service.Equipment.Prediction");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_Equipment_Security, "Service.Equipment.Security");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_Equipment_Rules, "Service.Equipment.Rules");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_Equipment_Transaction, "Service.Equipment.Transaction");

        // Supporting services
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_ActorFactory, "Service.ActorFactory");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_AttachmentSystem, "Service.AttachmentSystem");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Service_VisualController, "Service.VisualController");
    }

    //========================================
    // EVENT TAGS
    //========================================
    namespace Event
    {
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event, "Equipment.Event");

        // Item Events
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Equipped, "Equipment.Event.Equipped");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Unequipped, "Equipment.Event.Unequipped");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_ItemEquipped, "Equipment.Event.ItemEquipped");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_ItemUnequipped, "Equipment.Event.ItemUnequipped");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_SlotSwitched, "Equipment.Event.SlotSwitched");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Commit, "Equipment.Event.Commit");

        // Ability Events
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Ability, "Equipment.Event.Ability");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Ability_Refresh, "Equipment.Event.Ability.Refresh");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Ability_Granted, "Equipment.Event.Ability.Granted");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Ability_Removed, "Equipment.Event.Ability.Removed");

        // Operation Events
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Operation, "Equipment.Event.Operation");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Operation_Queued, "Equipment.Event.Operation.Queued");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Operation_Started, "Equipment.Event.Operation.Started");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Operation_Completed, "Equipment.Event.Operation.Completed");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Operation_Cancelled, "Equipment.Event.Operation.Cancelled");

        // Validation Events
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Validation, "Equipment.Event.Validation");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Validation_Changed, "Equipment.Event.Validation.Changed");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Validation_Started, "Equipment.Event.Validation.Started");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Validation_Passed, "Equipment.Event.Validation.Passed");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Validation_Failed, "Equipment.Event.Validation.Failed");

        // Data Events
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Data, "Equipment.Event.Data");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Data_Changed, "Equipment.Event.Data.Changed");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Data_Synced, "Equipment.Event.Data.Synced");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Data_Snapshot, "Equipment.Event.Data.Snapshot");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Data_Delta, "Equipment.Event.Data.Delta");

        // Network Events
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Network, "Equipment.Event.Network");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Network_Result, "Equipment.Event.Network.Result");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Network_Timeout, "Equipment.Event.Network.Timeout");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Network_SecurityViolation, "Equipment.Event.Network.SecurityViolation");

        // Visual Events
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Visual, "Equipment.Event.Visual");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Visual_RefreshAll, "Equipment.Event.VisRefreshAll");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Visual_Spawned, "Equipment.Event.Visual.Spawned");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Visual_Attached, "Equipment.Event.Visual.Attached");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Visual_Detached, "Equipment.Event.Visual.Detached");

        // Weapon Events
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon, "Equipment.Event.Weapon");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_Fired, "Equipment.Event.WeaponFired");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_FireModeChanged, "Equipment.Event.FireModeChanged");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_ReloadStart, "Equipment.Event.ReloadStart");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_ReloadEnd, "Equipment.Event.ReloadEnd");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_SpreadUpdated, "Equipment.Event.SpreadUpdated");
        UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Event_Weapon_AmmoChanged, "Equipment.Event.AmmoChanged");
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
    // UI TAGS
    //========================================
    namespace UI
    {
        UE_DEFINE_GAMEPLAY_TAG(TAG_UI_Equipment, "UI.Equipment");
        UE_DEFINE_GAMEPLAY_TAG(TAG_UI_Equipment_DataReady, "UI.Equipment.DataReady");
        UE_DEFINE_GAMEPLAY_TAG(TAG_UI_Equipment_SlotClicked, "UI.Equipment.SlotClicked");
        UE_DEFINE_GAMEPLAY_TAG(TAG_UI_Equipment_SlotHovered, "UI.Equipment.SlotHovered");
        UE_DEFINE_GAMEPLAY_TAG(TAG_UI_Equipment_DragStarted, "UI.Equipment.DragStarted");
        UE_DEFINE_GAMEPLAY_TAG(TAG_UI_Equipment_DragEnded, "UI.Equipment.DragEnded");
        UE_DEFINE_GAMEPLAY_TAG(TAG_UI_Equipment_ContextMenu, "UI.Equipment.ContextMenu");
    }

} // namespace SuspenseCoreEquipmentTags
