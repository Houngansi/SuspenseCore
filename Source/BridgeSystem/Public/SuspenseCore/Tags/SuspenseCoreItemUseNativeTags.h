// SuspenseCoreItemUseNativeTags.h
// Native GameplayTags for Item Use System
// Copyright Suspense Team. All Rights Reserved.
//
// USAGE:
//   #include "SuspenseCore/Tags/SuspenseCoreItemUseNativeTags.h"
//   EventBus->Publish(SuspenseCoreItemUseTags::Event::TAG_ItemUse_Event_Started, EventData);
//
// DO NOT use FGameplayTag::RequestGameplayTag() for these tags.
// Using native tags ensures:
// 1. Compile-time validation (no typos)
// 2. Better performance (no runtime string lookups)
// 3. IDE autocomplete support
//
// RELATED: SuspenseCoreItemUseTypes.h, ISuspenseCoreItemUseHandler.h

#pragma once

#include "NativeGameplayTags.h"

namespace SuspenseCoreItemUseTags
{
	//==================================================================
	// Handler Tags
	// Used to identify registered handlers
	//==================================================================

	namespace Handler
	{
		/** Base handler tag - parent for all handlers */
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Handler);

		/** Ammo to Magazine loader handler */
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Handler_AmmoToMagazine);

		/** Magazine swap from QuickSlot handler */
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Handler_MagazineSwap);

		/** Medical item use handler */
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Handler_Medical);

		/** Grenade prepare/throw handler */
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Handler_Grenade);

		/** Container open handler */
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Handler_Container);
	}

	//==================================================================
	// Event Tags
	// Published to EventBus during item use operations
	//==================================================================

	namespace Event
	{
		/** Item use started (time-based operation began) */
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Event_Started);

		/** Item use progress update */
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Event_Progress);

		/** Item use completed successfully */
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Event_Completed);

		/** Item use cancelled (by user, damage, etc.) */
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Event_Cancelled);

		/** Item use failed (validation, handler error) */
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Event_Failed);

		/** Item depleted (all uses consumed, item removed from slot) */
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_ItemUse_Event_ItemDepleted);
	}

	//==================================================================
	// State Tags
	// Applied via GameplayEffects during item use
	//==================================================================

	namespace State
	{
		/** Currently using item (blocks other actions) */
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_ItemUse_InProgress);

		/** Item use on cooldown */
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_ItemUse_Cooldown);
	}

	//==================================================================
	// Ability Tags
	// Used by GAS abilities
	//==================================================================

	namespace Ability
	{
		/** Base item use ability tag */
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_ItemUse);

		/** QuickSlot use ability tag */
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_QuickSlotUse);
	}

	//==================================================================
	// Data Tags (SetByCaller)
	// Used for GameplayEffect magnitude parameters
	//==================================================================

	namespace Data
	{
		/** Duration for timed item use (SetByCaller) */
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_ItemUse_Duration);

		/** Cooldown duration after item use (SetByCaller) */
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_ItemUse_Cooldown);

		/** Current progress 0-1 (SetByCaller) */
		BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_ItemUse_Progress);
	}
}
