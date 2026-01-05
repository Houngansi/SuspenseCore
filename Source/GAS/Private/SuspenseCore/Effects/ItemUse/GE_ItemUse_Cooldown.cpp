// GE_ItemUse_Cooldown.cpp
// GameplayEffect for item use cooldown
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/ItemUse/GE_ItemUse_Cooldown.h"
#include "SuspenseCore/Tags/SuspenseCoreItemUseNativeTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"

UGE_ItemUse_Cooldown::UGE_ItemUse_Cooldown(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Duration set by ability via SetByCaller
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// Use SetByCaller for cooldown duration
	DurationMagnitude = FGameplayEffectModifierMagnitude(
		FSetByCallerFloat(SuspenseCoreItemUseTags::Data::TAG_Data_ItemUse_Cooldown)
	);

	// No periodic execution
	Period = 0.0f;

	// Stacking: only one cooldown active at a time, refresh on reapply
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::ClearEntireStack;

	// Add State.ItemUse.Cooldown tag (blocks GA_ItemUse activation)
	UTargetTagsGameplayEffectComponent* TagComponent =
		ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this, TEXT("CooldownTargetTags"));

	if (TagComponent)
	{
		FInheritedTagContainer TagContainer;
		TagContainer.Added.AddTag(SuspenseCoreItemUseTags::State::TAG_State_ItemUse_Cooldown);

		TagComponent->SetAndApplyTargetTagChanges(TagContainer);
		GEComponents.Add(TagComponent);
	}

	// Add asset tag for identification and UI queries
	UAssetTagsGameplayEffectComponent* AssetTagsComponent =
		ObjectInitializer.CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(
			this, TEXT("CooldownAssetTags"));

	if (AssetTagsComponent)
	{
		FInheritedTagContainer AssetTagContainer;
		AssetTagContainer.Added.AddTag(
			FGameplayTag::RequestGameplayTag(FName("Effect.ItemUse.Cooldown"), false));

		AssetTagsComponent->SetAndApplyAssetTagChanges(AssetTagContainer);
		GEComponents.Add(AssetTagsComponent);
	}

	UE_LOG(LogTemp, Log, TEXT("GE_ItemUse_Cooldown: Configured with SetByCaller duration"));
}
