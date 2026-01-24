// SuspenseCoreMedicalNativeTags.cpp
// Native GameplayTags for Medical/Healing System
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Tags/SuspenseCoreMedicalNativeTags.h"

namespace SuspenseCoreMedicalTags
{
	//==================================================================
	// Data Tags (SetByCaller)
	//==================================================================

	namespace Data
	{
		UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Medical_InstantHeal, "Data.Medical.InstantHeal");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Medical_HealPerTick, "Data.Medical.HealPerTick");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Medical_HoTDuration, "Data.Medical.HoTDuration");
	}

	//==================================================================
	// Effect Tags
	//==================================================================

	namespace Effect
	{
		UE_DEFINE_GAMEPLAY_TAG(TAG_Effect_Medical_InstantHeal, "Effect.Medical.InstantHeal");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Effect_Medical_HealOverTime, "Effect.Medical.HealOverTime");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Effect_Medical_InProgress, "Effect.Medical.InProgress");
	}

	//==================================================================
	// Event Tags
	//==================================================================

	namespace Event
	{
		UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Medical_HealApplied, "SuspenseCore.Event.Medical.HealApplied");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Medical_BleedingCured, "SuspenseCore.Event.Medical.BleedingCured");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Medical_StatusCured, "SuspenseCore.Event.Medical.StatusCured");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Medical_HoTStarted, "SuspenseCore.Event.Medical.HoTStarted");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Medical_HoTInterrupted, "SuspenseCore.Event.Medical.HoTInterrupted");
	}

	//==================================================================
	// State Tags
	//==================================================================

	namespace State
	{
		UE_DEFINE_GAMEPLAY_TAG(TAG_State_Medical_Regenerating, "State.Medical.Regenerating");
		UE_DEFINE_GAMEPLAY_TAG(TAG_State_Medical_UsingItem, "State.Medical.UsingItem");
	}

	//==================================================================
	// Cure Tags
	//==================================================================

	namespace Cure
	{
		UE_DEFINE_GAMEPLAY_TAG(TAG_Cure_Bleeding_Light, "Item.Medical.Cure.Bleeding.Light");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Cure_Bleeding_Heavy, "Item.Medical.Cure.Bleeding.Heavy");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Cure_Bleeding_All, "Item.Medical.Cure.Bleeding");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Cure_Fracture, "Item.Medical.Cure.Fracture");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Cure_Pain, "Item.Medical.Cure.Pain");
	}
}
