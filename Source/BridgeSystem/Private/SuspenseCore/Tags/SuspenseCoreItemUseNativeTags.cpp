// SuspenseCoreItemUseNativeTags.cpp
// Native GameplayTags definitions for Item Use System
// Copyright Suspense Team. All Rights Reserved.
//
// These tags are registered at module load time.
// They MUST also be registered in Config/DefaultGameplayTags.ini

#include "SuspenseCore/Tags/SuspenseCoreItemUseNativeTags.h"

namespace SuspenseCoreItemUseTags
{
	//==================================================================
	// Handler Tags
	//==================================================================

	namespace Handler
	{
		UE_DEFINE_GAMEPLAY_TAG(TAG_ItemUse_Handler, "ItemUse.Handler");
		UE_DEFINE_GAMEPLAY_TAG(TAG_ItemUse_Handler_AmmoToMagazine, "ItemUse.Handler.AmmoToMagazine");
		UE_DEFINE_GAMEPLAY_TAG(TAG_ItemUse_Handler_MagazineSwap, "ItemUse.Handler.MagazineSwap");
		UE_DEFINE_GAMEPLAY_TAG(TAG_ItemUse_Handler_Medical, "ItemUse.Handler.Medical");
		UE_DEFINE_GAMEPLAY_TAG(TAG_ItemUse_Handler_Grenade, "ItemUse.Handler.Grenade");
		UE_DEFINE_GAMEPLAY_TAG(TAG_ItemUse_Handler_Container, "ItemUse.Handler.Container");
	}

	//==================================================================
	// Event Tags
	//==================================================================

	namespace Event
	{
		UE_DEFINE_GAMEPLAY_TAG(TAG_ItemUse_Event_Started, "SuspenseCore.Event.ItemUse.Started");
		UE_DEFINE_GAMEPLAY_TAG(TAG_ItemUse_Event_Progress, "SuspenseCore.Event.ItemUse.Progress");
		UE_DEFINE_GAMEPLAY_TAG(TAG_ItemUse_Event_Completed, "SuspenseCore.Event.ItemUse.Completed");
		UE_DEFINE_GAMEPLAY_TAG(TAG_ItemUse_Event_Cancelled, "SuspenseCore.Event.ItemUse.Cancelled");
		UE_DEFINE_GAMEPLAY_TAG(TAG_ItemUse_Event_Failed, "SuspenseCore.Event.ItemUse.Failed");
		UE_DEFINE_GAMEPLAY_TAG(TAG_ItemUse_Event_ItemDepleted, "SuspenseCore.Event.ItemUse.ItemDepleted");
	}

	//==================================================================
	// State Tags
	//==================================================================

	namespace State
	{
		UE_DEFINE_GAMEPLAY_TAG(TAG_State_ItemUse_InProgress, "State.ItemUse.InProgress");
		UE_DEFINE_GAMEPLAY_TAG(TAG_State_ItemUse_Cooldown, "State.ItemUse.Cooldown");
	}

	//==================================================================
	// Ability Tags
	//==================================================================

	namespace Ability
	{
		UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_ItemUse, "SuspenseCore.Ability.ItemUse");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_QuickSlotUse, "SuspenseCore.Ability.QuickSlotUse");
	}

	//==================================================================
	// Data Tags (SetByCaller)
	//==================================================================

	namespace Data
	{
		UE_DEFINE_GAMEPLAY_TAG(TAG_Data_ItemUse_Duration, "Data.ItemUse.Duration");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Data_ItemUse_Cooldown, "Data.ItemUse.Cooldown");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Data_ItemUse_Progress, "Data.ItemUse.Progress");
	}
}
