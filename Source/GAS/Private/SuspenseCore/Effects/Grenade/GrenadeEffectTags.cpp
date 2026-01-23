// GrenadeEffectTags.cpp
// Local native tag registration for Grenade GameplayEffects
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE NOTE:
// These tags are registered locally in the GAS module because they are used
// in CDO (Class Default Object) constructors of GameplayEffect classes.
// CDO constructors run during module loading, BEFORE other modules' native
// tags are registered. This ensures tags are available when needed.
//
// The same tags are also declared in SuspenseCoreGameplayTags.h for
// external access via the SuspenseCoreTags:: namespace.

#include "NativeGameplayTags.h"

// ═══════════════════════════════════════════════════════════════════
// DoT Effect Tags (used in GE_BleedingEffect, GE_IncendiaryEffect)
// ═══════════════════════════════════════════════════════════════════

// Base damage tags
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Effect_Damage, "Effect.Damage");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Effect_Damage_Burn, "Effect.Damage.Burn");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Effect_Damage_Bleed, "Effect.Damage.Bleed");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Effect_Damage_Bleed_Light, "Effect.Damage.Bleed.Light");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Effect_Damage_Bleed_Heavy, "Effect.Damage.Bleed.Heavy");

// Grenade effect tags
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Effect_Grenade_Incendiary, "Effect.Grenade.Incendiary");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Effect_Grenade_Incendiary_Zone, "Effect.Grenade.Incendiary.Zone");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Effect_Grenade_Incendiary_ArmorBypass, "Effect.Grenade.Incendiary.ArmorBypass");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Effect_Grenade_Shrapnel, "Effect.Grenade.Shrapnel");

// DoT parent tag
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Effect_DoT, "Effect.DoT");

// ═══════════════════════════════════════════════════════════════════
// State Tags (granted by GE effects)
// ═══════════════════════════════════════════════════════════════════
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Burning, "State.Burning");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_InFireZone, "State.InFireZone");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Health_Bleeding_Light, "State.Health.Bleeding.Light");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Health_Bleeding_Heavy, "State.Health.Bleeding.Heavy");

// ═══════════════════════════════════════════════════════════════════
// GameplayCue Tags (for VFX/SFX)
// ═══════════════════════════════════════════════════════════════════
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GameplayCue_Grenade_Burn, "GameplayCue.Grenade.Burn");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GameplayCue_DoT_Bleed_Light, "GameplayCue.DoT.Bleed.Light");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GameplayCue_DoT_Bleed_Heavy, "GameplayCue.DoT.Bleed.Heavy");

// ═══════════════════════════════════════════════════════════════════
// Data Tags (SetByCaller magnitudes)
// ═══════════════════════════════════════════════════════════════════
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Data_Grenade_BurnDuration, "Data.Grenade.BurnDuration");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Data_Damage_Burn, "Data.Damage.Burn");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Data_Damage_Burn_Armor, "Data.Damage.Burn.Armor");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Data_Damage_Burn_Health, "Data.Damage.Burn.Health");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Data_Damage_Bleed, "Data.Damage.Bleed");
