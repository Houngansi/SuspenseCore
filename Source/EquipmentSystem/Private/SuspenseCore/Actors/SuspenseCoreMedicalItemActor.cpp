// SuspenseCoreMedicalItemActor.cpp
// Visual actor for medical items held in hand
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Actors/SuspenseCoreMedicalItemActor.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimationAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogMedicalItemActor, Log, All);

#define MEDICAL_ACTOR_LOG(Verbosity, Format, ...) \
	UE_LOG(LogMedicalItemActor, Verbosity, TEXT("[MedicalItem][%s] " Format), *GetNameSafe(this), ##__VA_ARGS__)

//==================================================================
// Constructor
//==================================================================

ASuspenseCoreMedicalItemActor::ASuspenseCoreMedicalItemActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create static mesh component (for simple items like bandages)
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	SetRootComponent(StaticMeshComponent);

	// Configure static mesh for visual-only use
	StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaticMeshComponent->SetSimulatePhysics(false);
	StaticMeshComponent->SetGenerateOverlapEvents(false);
	StaticMeshComponent->CastShadow = true;

	// Create skeletal mesh component (for animated items like syringes)
	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMeshComponent"));
	SkeletalMeshComponent->SetupAttachment(StaticMeshComponent);

	// Configure skeletal mesh for visual-only use
	SkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkeletalMeshComponent->SetSimulatePhysics(false);
	SkeletalMeshComponent->SetGenerateOverlapEvents(false);
	SkeletalMeshComponent->CastShadow = true;

	// Default: static mesh visible, skeletal hidden
	ActiveMeshType = ESuspenseCoreMedicalMeshType::StaticMesh;
}

//==================================================================
// AActor Overrides
//==================================================================

void ASuspenseCoreMedicalItemActor::BeginPlay()
{
	Super::BeginPlay();

	// Initial visibility update
	UpdateComponentVisibility();

	MEDICAL_ACTOR_LOG(Log, TEXT("BeginPlay: Type=%d, MeshType=%d"),
		static_cast<int32>(VisualType),
		static_cast<int32>(ActiveMeshType));
}

//==================================================================
// Initialization
//==================================================================

void ASuspenseCoreMedicalItemActor::InitializeFromItemID(FName ItemID, USuspenseCoreDataManager* InDataManager)
{
	MedicalItemID = ItemID;
	DataManager = InDataManager;

	if (!InDataManager)
	{
		MEDICAL_ACTOR_LOG(Warning, TEXT("InitializeFromItemID: No DataManager, using generic config"));
		VisualType = ESuspenseCoreMedicalVisualType::Generic;
		ApplyConfig(GenericConfig);
		return;
	}

	// Get item data from SSOT
	FSuspenseCoreUnifiedItemData ItemData;
	if (InDataManager->GetUnifiedItemData(ItemID, ItemData))
	{
		// Determine visual type from item tags
		VisualType = DetermineVisualType(ItemData.ItemTags);

		// Get and apply config for this type
		const FSuspenseCoreMedicalVisualConfig& Config = GetConfigForType(VisualType);
		ApplyConfig(Config);

		MEDICAL_ACTOR_LOG(Log, TEXT("InitializeFromItemID: %s -> Type=%d, MeshType=%d"),
			*ItemID.ToString(),
			static_cast<int32>(VisualType),
			static_cast<int32>(ActiveMeshType));
	}
	else
	{
		MEDICAL_ACTOR_LOG(Warning, TEXT("InitializeFromItemID: Item %s not found in SSOT"), *ItemID.ToString());
		VisualType = ESuspenseCoreMedicalVisualType::Generic;
		ApplyConfig(GenericConfig);
	}
}

void ASuspenseCoreMedicalItemActor::InitializeFromTypeTag(FGameplayTag MedicalTypeTag)
{
	FString TagStr = MedicalTypeTag.ToString();

	if (TagStr.Contains(TEXT("Bandage")))
	{
		VisualType = ESuspenseCoreMedicalVisualType::Bandage;
	}
	else if (TagStr.Contains(TEXT("Medkit")) || TagStr.Contains(TEXT("IFAK")) ||
	         TagStr.Contains(TEXT("AFAK")) || TagStr.Contains(TEXT("Salewa")))
	{
		VisualType = ESuspenseCoreMedicalVisualType::Medkit;
	}
	else if (TagStr.Contains(TEXT("Injector")) || TagStr.Contains(TEXT("Morphine")) ||
	         TagStr.Contains(TEXT("Stimulant")) || TagStr.Contains(TEXT("Adrenaline")))
	{
		VisualType = ESuspenseCoreMedicalVisualType::Injector;
	}
	else if (TagStr.Contains(TEXT("Splint")))
	{
		VisualType = ESuspenseCoreMedicalVisualType::Splint;
	}
	else if (TagStr.Contains(TEXT("Surgical")) || TagStr.Contains(TEXT("Grizzly")))
	{
		VisualType = ESuspenseCoreMedicalVisualType::Surgical;
	}
	else if (TagStr.Contains(TEXT("Painkiller")) || TagStr.Contains(TEXT("Pills")))
	{
		VisualType = ESuspenseCoreMedicalVisualType::Pills;
	}
	else
	{
		VisualType = ESuspenseCoreMedicalVisualType::Generic;
	}

	const FSuspenseCoreMedicalVisualConfig& Config = GetConfigForType(VisualType);
	ApplyConfig(Config);

	MEDICAL_ACTOR_LOG(Log, TEXT("InitializeFromTypeTag: %s -> Type=%d, MeshType=%d"),
		*MedicalTypeTag.ToString(),
		static_cast<int32>(VisualType),
		static_cast<int32>(ActiveMeshType));
}

void ASuspenseCoreMedicalItemActor::SetStaticMesh(UStaticMesh* NewMesh)
{
	if (!StaticMeshComponent)
	{
		return;
	}

	if (NewMesh)
	{
		StaticMeshComponent->SetStaticMesh(NewMesh);
		ActiveMeshType = ESuspenseCoreMedicalMeshType::StaticMesh;
		UpdateComponentVisibility();
		MEDICAL_ACTOR_LOG(Verbose, TEXT("SetStaticMesh: %s"), *NewMesh->GetName());
	}
}

void ASuspenseCoreMedicalItemActor::SetSkeletalMesh(USkeletalMesh* NewMesh, UAnimationAsset* OptionalAnim)
{
	if (!SkeletalMeshComponent)
	{
		return;
	}

	if (NewMesh)
	{
		SkeletalMeshComponent->SetSkeletalMesh(NewMesh);
		ActiveMeshType = ESuspenseCoreMedicalMeshType::SkeletalMesh;
		UpdateComponentVisibility();

		// Play optional animation
		if (OptionalAnim)
		{
			PlayAnimation(OptionalAnim, true);
		}

		MEDICAL_ACTOR_LOG(Verbose, TEXT("SetSkeletalMesh: %s, Anim=%s"),
			*NewMesh->GetName(),
			OptionalAnim ? *OptionalAnim->GetName() : TEXT("None"));
	}
}

//==================================================================
// Runtime Accessors
//==================================================================

FTransform ASuspenseCoreMedicalItemActor::GetAttachOffset() const
{
	return CurrentConfig.AttachOffset;
}

//==================================================================
// Visual State
//==================================================================

void ASuspenseCoreMedicalItemActor::SetVisibility(bool bVisible)
{
	SetActorHiddenInGame(!bVisible);

	MEDICAL_ACTOR_LOG(Verbose, TEXT("SetVisibility: %s"), bVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void ASuspenseCoreMedicalItemActor::PlayAnimation(UAnimationAsset* Animation, bool bLooping)
{
	if (!SkeletalMeshComponent || !Animation)
	{
		return;
	}

	if (ActiveMeshType != ESuspenseCoreMedicalMeshType::SkeletalMesh)
	{
		MEDICAL_ACTOR_LOG(Warning, TEXT("PlayAnimation: Not using skeletal mesh"));
		return;
	}

	SkeletalMeshComponent->PlayAnimation(Animation, bLooping);

	MEDICAL_ACTOR_LOG(Verbose, TEXT("PlayAnimation: %s, Looping=%d"),
		*Animation->GetName(), bLooping ? 1 : 0);
}

void ASuspenseCoreMedicalItemActor::StopAnimation()
{
	if (!SkeletalMeshComponent)
	{
		return;
	}

	SkeletalMeshComponent->Stop();

	MEDICAL_ACTOR_LOG(Verbose, TEXT("StopAnimation"));
}

void ASuspenseCoreMedicalItemActor::ResetForPool()
{
	// Reset state for reuse
	MedicalItemID = NAME_None;
	VisualType = ESuspenseCoreMedicalVisualType::Generic;
	ActiveMeshType = ESuspenseCoreMedicalMeshType::StaticMesh;
	CurrentConfig = FSuspenseCoreMedicalVisualConfig();

	// Stop any animations
	StopAnimation();

	// Detach from any parent
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// Hide and disable
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	// Reset transform
	SetActorTransform(FTransform::Identity);

	// Reset visibility
	UpdateComponentVisibility();

	MEDICAL_ACTOR_LOG(Log, TEXT("ResetForPool: Ready for reuse"));
}

//==================================================================
// Internal Methods
//==================================================================

ESuspenseCoreMedicalVisualType ASuspenseCoreMedicalItemActor::DetermineVisualType(
	const FGameplayTagContainer& ItemTags) const
{
	// Check tags in priority order
	if (ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Medical.Bandage"), false)))
	{
		return ESuspenseCoreMedicalVisualType::Bandage;
	}
	if (ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Medical.Medkit"), false)) ||
	    ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Medical.IFAK"), false)))
	{
		return ESuspenseCoreMedicalVisualType::Medkit;
	}
	if (ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Medical.Injector"), false)) ||
	    ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Medical.Stimulant"), false)))
	{
		return ESuspenseCoreMedicalVisualType::Injector;
	}
	if (ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Medical.Splint"), false)))
	{
		return ESuspenseCoreMedicalVisualType::Splint;
	}
	if (ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Medical.Surgical"), false)))
	{
		return ESuspenseCoreMedicalVisualType::Surgical;
	}
	if (ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Medical.Painkiller"), false)))
	{
		return ESuspenseCoreMedicalVisualType::Pills;
	}

	return ESuspenseCoreMedicalVisualType::Generic;
}

const FSuspenseCoreMedicalVisualConfig& ASuspenseCoreMedicalItemActor::GetConfigForType(
	ESuspenseCoreMedicalVisualType Type) const
{
	switch (Type)
	{
		case ESuspenseCoreMedicalVisualType::Bandage:
			return BandageConfig;
		case ESuspenseCoreMedicalVisualType::Medkit:
			return MedkitConfig;
		case ESuspenseCoreMedicalVisualType::Injector:
			return InjectorConfig;
		case ESuspenseCoreMedicalVisualType::Splint:
			return SplintConfig;
		case ESuspenseCoreMedicalVisualType::Surgical:
			return SurgicalConfig;
		case ESuspenseCoreMedicalVisualType::Pills:
			return PillsConfig;
		default:
			return GenericConfig;
	}
}

void ASuspenseCoreMedicalItemActor::ApplyConfig(const FSuspenseCoreMedicalVisualConfig& Config)
{
	CurrentConfig = Config;
	ActiveMeshType = Config.MeshType;

	if (Config.MeshType == ESuspenseCoreMedicalMeshType::StaticMesh)
	{
		// Use static mesh
		if (StaticMeshComponent && Config.StaticMesh)
		{
			StaticMeshComponent->SetStaticMesh(Config.StaticMesh);
		}
	}
	else // SkeletalMesh
	{
		// Use skeletal mesh
		if (SkeletalMeshComponent && Config.SkeletalMesh)
		{
			SkeletalMeshComponent->SetSkeletalMesh(Config.SkeletalMesh);

			// Play idle animation if specified
			if (Config.IdleAnimation)
			{
				SkeletalMeshComponent->PlayAnimation(Config.IdleAnimation, true);
			}
		}
	}

	UpdateComponentVisibility();

	MEDICAL_ACTOR_LOG(Log, TEXT("ApplyConfig: MeshType=%d, HasStaticMesh=%d, HasSkeletalMesh=%d"),
		static_cast<int32>(Config.MeshType),
		Config.StaticMesh != nullptr ? 1 : 0,
		Config.SkeletalMesh != nullptr ? 1 : 0);
}

void ASuspenseCoreMedicalItemActor::UpdateComponentVisibility()
{
	if (!StaticMeshComponent || !SkeletalMeshComponent)
	{
		return;
	}

	if (ActiveMeshType == ESuspenseCoreMedicalMeshType::StaticMesh)
	{
		StaticMeshComponent->SetVisibility(true);
		SkeletalMeshComponent->SetVisibility(false);
	}
	else // SkeletalMesh
	{
		StaticMeshComponent->SetVisibility(false);
		SkeletalMeshComponent->SetVisibility(true);
	}
}

#undef MEDICAL_ACTOR_LOG
