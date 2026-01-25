// SuspenseCoreMedicalItemActor.cpp
// Visual actor for medical items held in hand
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Actors/SuspenseCoreMedicalItemActor.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
#include "Components/StaticMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogMedicalItemActor, Log, All);

#define MEDICAL_ACTOR_LOG(Verbosity, Format, ...) \
	UE_LOG(LogMedicalItemActor, Verbosity, TEXT("[MedicalItem][%s] " Format), *GetNameSafe(this), ##__VA_ARGS__)

//==================================================================
// Constructor
//==================================================================

ASuspenseCoreMedicalItemActor::ASuspenseCoreMedicalItemActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create mesh component
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);

	// Configure for visual-only use
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetSimulatePhysics(false);
	MeshComponent->SetGenerateOverlapEvents(false);
	MeshComponent->CastShadow = true;

	// Default transform offsets (can be overridden in Blueprint)
	BandageOffset = FTransform::Identity;
	MedkitOffset = FTransform::Identity;
	InjectorOffset = FTransform::Identity;
	SplintOffset = FTransform::Identity;
	SurgicalOffset = FTransform::Identity;
	PillsOffset = FTransform::Identity;
}

//==================================================================
// AActor Overrides
//==================================================================

void ASuspenseCoreMedicalItemActor::BeginPlay()
{
	Super::BeginPlay();

	MEDICAL_ACTOR_LOG(Log, TEXT("BeginPlay: Type=%d"), static_cast<int32>(VisualType));
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
		MEDICAL_ACTOR_LOG(Warning, TEXT("InitializeFromItemID: No DataManager, using default mesh"));
		SetMesh(GetMeshForType(ESuspenseCoreMedicalVisualType::Generic));
		return;
	}

	// Get item data from SSOT
	FSuspenseCoreUnifiedItemData ItemData;
	if (InDataManager->GetUnifiedItemData(ItemID, ItemData))
	{
		// Determine visual type from item tags
		VisualType = DetermineVisualType(ItemData.ItemTags);

		// Try to load item-specific mesh from SSOT
		// TODO: Add MeshPath to FSuspenseCoreUnifiedItemData for custom meshes
		// For now, use default mesh for type
		UStaticMesh* TypeMesh = GetMeshForType(VisualType);
		SetMesh(TypeMesh);

		MEDICAL_ACTOR_LOG(Log, TEXT("InitializeFromItemID: %s -> Type=%d, Mesh=%s"),
			*ItemID.ToString(),
			static_cast<int32>(VisualType),
			TypeMesh ? *TypeMesh->GetName() : TEXT("None"));
	}
	else
	{
		MEDICAL_ACTOR_LOG(Warning, TEXT("InitializeFromItemID: Item %s not found in SSOT"), *ItemID.ToString());
		VisualType = ESuspenseCoreMedicalVisualType::Generic;
		SetMesh(GetMeshForType(VisualType));
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

	SetMesh(GetMeshForType(VisualType));

	MEDICAL_ACTOR_LOG(Log, TEXT("InitializeFromTypeTag: %s -> Type=%d"),
		*MedicalTypeTag.ToString(), static_cast<int32>(VisualType));
}

void ASuspenseCoreMedicalItemActor::SetMesh(UStaticMesh* NewMesh)
{
	if (MeshComponent && NewMesh)
	{
		MeshComponent->SetStaticMesh(NewMesh);
		MEDICAL_ACTOR_LOG(Verbose, TEXT("SetMesh: %s"), *NewMesh->GetName());
	}
}

//==================================================================
// Runtime Accessors
//==================================================================

FTransform ASuspenseCoreMedicalItemActor::GetAttachOffset() const
{
	switch (VisualType)
	{
		case ESuspenseCoreMedicalVisualType::Bandage:
			return BandageOffset;
		case ESuspenseCoreMedicalVisualType::Medkit:
			return MedkitOffset;
		case ESuspenseCoreMedicalVisualType::Injector:
			return InjectorOffset;
		case ESuspenseCoreMedicalVisualType::Splint:
			return SplintOffset;
		case ESuspenseCoreMedicalVisualType::Surgical:
			return SurgicalOffset;
		case ESuspenseCoreMedicalVisualType::Pills:
			return PillsOffset;
		default:
			return FTransform::Identity;
	}
}

//==================================================================
// Visual State
//==================================================================

void ASuspenseCoreMedicalItemActor::SetVisibility(bool bVisible)
{
	SetActorHiddenInGame(!bVisible);

	MEDICAL_ACTOR_LOG(Verbose, TEXT("SetVisibility: %s"), bVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void ASuspenseCoreMedicalItemActor::ResetForPool()
{
	// Reset state for reuse
	MedicalItemID = NAME_None;
	VisualType = ESuspenseCoreMedicalVisualType::Generic;

	// Detach from any parent
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// Hide and disable
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	// Reset transform
	SetActorTransform(FTransform::Identity);

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

UStaticMesh* ASuspenseCoreMedicalItemActor::GetMeshForType(ESuspenseCoreMedicalVisualType Type) const
{
	switch (Type)
	{
		case ESuspenseCoreMedicalVisualType::Bandage:
			return BandageMesh ? BandageMesh.Get() : GenericMesh.Get();
		case ESuspenseCoreMedicalVisualType::Medkit:
			return MedkitMesh ? MedkitMesh.Get() : GenericMesh.Get();
		case ESuspenseCoreMedicalVisualType::Injector:
			return InjectorMesh ? InjectorMesh.Get() : GenericMesh.Get();
		case ESuspenseCoreMedicalVisualType::Splint:
			return SplintMesh ? SplintMesh.Get() : GenericMesh.Get();
		case ESuspenseCoreMedicalVisualType::Surgical:
			return SurgicalMesh ? SurgicalMesh.Get() : GenericMesh.Get();
		case ESuspenseCoreMedicalVisualType::Pills:
			return PillsMesh ? PillsMesh.Get() : GenericMesh.Get();
		default:
			return GenericMesh.Get();
	}
}

#undef MEDICAL_ACTOR_LOG
