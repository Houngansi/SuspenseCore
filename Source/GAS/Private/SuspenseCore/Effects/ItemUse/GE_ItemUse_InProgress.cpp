// GE_ItemUse_InProgress.cpp
// GameplayEffect for item use in-progress state
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/ItemUse/GE_ItemUse_InProgress.h"
#include "SuspenseCore/Tags/SuspenseCoreItemUseNativeTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"

UGE_ItemUse_InProgress::UGE_ItemUse_InProgress(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Duration set by ability via SetByCaller
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// Use SetByCaller for duration
	// Note: FSetByCallerFloat must be default-constructed and DataTag set
	FSetByCallerFloat SetByCallerDuration;
	SetByCallerDuration.DataTag = SuspenseCoreItemUseTags::Data::TAG_Data_ItemUse_Duration.GetTag();
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDuration);

	// No periodic execution needed
	Period = 0.0f;

	// Stacking: unique per source (one operation per ability instance)
	StackingType = EGameplayEffectStackingType::AggregateBySource;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;

	// Add State.ItemUse.InProgress tag
	UTargetTagsGameplayEffectComponent* TagComponent =
		ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this, TEXT("InProgressTargetTags"));

	if (TagComponent)
	{
		FInheritedTagContainer TagContainer;
		TagContainer.Added.AddTag(SuspenseCoreItemUseTags::State::TAG_State_ItemUse_InProgress);

		TagComponent->SetAndApplyTargetTagChanges(TagContainer);
		GEComponents.Add(TagComponent);
	}

	// Add asset tag for identification
	UAssetTagsGameplayEffectComponent* AssetTagsComponent =
		ObjectInitializer.CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(
			this, TEXT("InProgressAssetTags"));

	if (AssetTagsComponent)
	{
		FInheritedTagContainer AssetTagContainer;
		// Use a general effect identification tag
		AssetTagContainer.Added.AddTag(
			FGameplayTag::RequestGameplayTag(FName("Effect.ItemUse.InProgress"), false));

		AssetTagsComponent->SetAndApplyAssetTagChanges(AssetTagContainer);
		GEComponents.Add(AssetTagsComponent);
	}

	UE_LOG(LogTemp, Log, TEXT("GE_ItemUse_InProgress: Configured with SetByCaller duration"));
}
