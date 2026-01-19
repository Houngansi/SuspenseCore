// GE_FlashbangEffect.cpp
// GameplayEffect for flashbang blind/deafen effects
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/Grenade/GE_FlashbangEffect.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"

//========================================================================
// UGE_FlashbangEffect
//========================================================================

UGE_FlashbangEffect::UGE_FlashbangEffect()
{
	// Duration set by ability via SetByCaller (based on distance)
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// Use SetByCaller for flash duration
	// Tag: Data.Grenade.FlashDuration
	FSetByCallerFloat SetByCallerDuration;
	SetByCallerDuration.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Grenade.FlashDuration"));
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDuration);

	// No periodic execution - effect is continuous
	Period = 0.0f;

	// Stacking: refresh duration on reapply, only one active
	// NOTE: Stacking API will be made private in future UE versions
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::ClearEntireStack;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Add State.Blinded and State.Deafened tags
	// These tags can be used for:
	// - Blocking abilities (e.g., can't aim precisely while blinded)
	// - UI feedback (screen flash effect)
	// - AI behavior (enemies lose track of player)
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Blinded")));
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Deafened")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Asset tags for GameplayCue support
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Grenade.Flashbang")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("GameplayCue.Grenade.Flashbang")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_LOG(LogTemp, Log, TEXT("GE_FlashbangEffect: Configured with SetByCaller duration, grants State.Blinded and State.Deafened"));
}

//========================================================================
// UGE_FlashbangEffect_Partial
//========================================================================

UGE_FlashbangEffect_Partial::UGE_FlashbangEffect_Partial()
{
	// Duration set by ability via SetByCaller
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// Use SetByCaller for flash duration (will be shorter than full effect)
	FSetByCallerFloat SetByCallerDuration;
	SetByCallerDuration.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Grenade.FlashDuration"));
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDuration);

	// No periodic execution
	Period = 0.0f;

	// Stacking: same as full effect
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::ClearEntireStack;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Partial effect - only applies Disoriented tag (reduced impairment)
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Disoriented")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Asset tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Grenade.Flashbang.Partial")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("GameplayCue.Grenade.Flashbang.Partial")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_LOG(LogTemp, Log, TEXT("GE_FlashbangEffect_Partial: Configured with reduced effect"));
}
