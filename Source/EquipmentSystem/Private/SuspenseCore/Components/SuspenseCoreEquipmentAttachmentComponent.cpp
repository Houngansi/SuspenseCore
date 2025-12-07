// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreEquipmentAttachmentComponent.h"
#include "SuspenseCore/Components/SuspenseCoreWeaponStanceComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

// Socket priority lists
const TArray<FName> USuspenseCoreEquipmentAttachmentComponent::WeaponSocketPriority = {
	FName("WeaponSocket"), FName("RightHandSocket"), FName("weapon_r")
};

const TArray<FName> USuspenseCoreEquipmentAttachmentComponent::ArmorSocketPriority = {
	FName("ArmorSocket"), FName("ChestSocket"), FName("armor_chest")
};

const TArray<FName> USuspenseCoreEquipmentAttachmentComponent::AccessorySocketPriority = {
	FName("AccessorySocket"), FName("BeltSocket"), FName("accessory_01")
};

USuspenseCoreEquipmentAttachmentComponent::USuspenseCoreEquipmentAttachmentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedComponent(true);

	SpawnedEquipmentActor = nullptr;
	AttachTarget = nullptr;
	bIsAttached = false;
	bIsInActiveState = false;
	CurrentSocketName = NAME_None;
	CurrentAttachmentOffset = FTransform::Identity;
	bDidSpawnActor = false;
	AttachmentVersion = 1;
	NextAttachmentPredictionKey = 1;
	LastAnimationInterfaceCacheTime = -1000.0f;
	LastSocketCacheTime = -1000.0f;
}

void USuspenseCoreEquipmentAttachmentComponent::BeginPlay()
{
	Super::BeginPlay();
	SUSPENSECORE_LOG(Log, TEXT("BeginPlay"));

	// Auto-link stance component if enabled
	if (bAutoLinkStanceComponent && GetOwner())
	{
		USuspenseCoreWeaponStanceComponent* StanceComp = GetOwner()->FindComponentByClass<USuspenseCoreWeaponStanceComponent>();
		if (StanceComp)
		{
			LinkStanceComponent(StanceComp);
		}
	}
}

void USuspenseCoreEquipmentAttachmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Cleanup();
	Super::EndPlay(EndPlayReason);
}

void USuspenseCoreEquipmentAttachmentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateAnimationState(DeltaTime);
	CleanupExpiredPredictions();
}

void USuspenseCoreEquipmentAttachmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USuspenseCoreEquipmentAttachmentComponent, SpawnedEquipmentActor);
	DOREPLIFETIME(USuspenseCoreEquipmentAttachmentComponent, bIsAttached);
	DOREPLIFETIME(USuspenseCoreEquipmentAttachmentComponent, bIsInActiveState);
	DOREPLIFETIME(USuspenseCoreEquipmentAttachmentComponent, CurrentSocketName);
	DOREPLIFETIME(USuspenseCoreEquipmentAttachmentComponent, CurrentAttachmentOffset);
	DOREPLIFETIME(USuspenseCoreEquipmentAttachmentComponent, CurrentWeaponType);
	DOREPLIFETIME(USuspenseCoreEquipmentAttachmentComponent, bDidSpawnActor);
	DOREPLIFETIME(USuspenseCoreEquipmentAttachmentComponent, AttachmentVersion);
	DOREPLIFETIME(USuspenseCoreEquipmentAttachmentComponent, AnimationState);
}

void USuspenseCoreEquipmentAttachmentComponent::InitializeWithItemInstance(AActor* InOwner, UAbilitySystemComponent* InASC, const FSuspenseInventoryItemInstance& ItemInstance)
{
	Super::InitializeWithItemInstance(InOwner, InASC, ItemInstance);
	SUSPENSECORE_LOG(Log, TEXT("InitializeWithItemInstance: %s"), *ItemInstance.ItemID.ToString());

	// Implementation stub - spawn equipment actor if needed
}

void USuspenseCoreEquipmentAttachmentComponent::Cleanup()
{
	SUSPENSECORE_LOG(Log, TEXT("Cleanup"));

	Detach(false);
	DestroyEquipmentActor();

	Super::Cleanup();
}

void USuspenseCoreEquipmentAttachmentComponent::UpdateEquippedItem(const FSuspenseInventoryItemInstance& NewItemInstance)
{
	Super::UpdateEquippedItem(NewItemInstance);
	SUSPENSECORE_LOG(Log, TEXT("UpdateEquippedItem: %s"), *NewItemInstance.ItemID.ToString());

	// Implementation stub - update attachment based on new item
}

bool USuspenseCoreEquipmentAttachmentComponent::AttachToCharacter(AActor* Character, bool bUseActiveSocket, USceneComponent* ComponentToAttach)
{
	if (!Character)
	{
		SUSPENSECORE_LOG(Error, TEXT("AttachToCharacter: Invalid character"));
		return false;
	}

	SUSPENSECORE_LOG(Log, TEXT("AttachToCharacter: %s, UseActiveSocket: %d"),
		*Character->GetName(), bUseActiveSocket);

	USkeletalMeshComponent* CharacterMesh = GetCharacterMesh(Character);
	if (!CharacterMesh)
	{
		SUSPENSECORE_LOG(Error, TEXT("AttachToCharacter: No character mesh found"));
		return false;
	}

	FSuspenseUnifiedItemData ItemData;
	if (!GetEquippedItemData(ItemData))
	{
		SUSPENSECORE_LOG(Error, TEXT("AttachToCharacter: Failed to get item data"));
		return false;
	}

	// Find best socket
	FName SocketName = FindBestAttachmentSocket(CharacterMesh, ItemData, bUseActiveSocket);
	FTransform Offset = GetAttachmentOffset(bUseActiveSocket);
	FGameplayTag WeaponType = GetWeaponArchetypeFromItem();

	// Apply attachment
	USceneComponent* CompToAttach = ComponentToAttach;
	if (!CompToAttach && SpawnedEquipmentActor)
	{
		CompToAttach = SpawnedEquipmentActor->GetRootComponent();
	}

	if (CompToAttach)
	{
		ApplyAttachment(CompToAttach, CharacterMesh, SocketName, Offset);

		bIsAttached = true;
		bIsInActiveState = bUseActiveSocket;
		CurrentSocketName = SocketName;
		CurrentAttachmentOffset = Offset;
		CurrentWeaponType = WeaponType;
		AttachedCharacter = Character;
		AttachTarget = CharacterMesh;

		BroadcastAttachmentEvent(true, Character, SocketName);
		NotifyStanceOfAttachment(true);

		return true;
	}

	return false;
}

void USuspenseCoreEquipmentAttachmentComponent::Detach(bool bMaintainWorldTransform)
{
	SUSPENSECORE_LOG(Log, TEXT("Detach"));

	if (SpawnedEquipmentActor && bIsAttached)
	{
		SpawnedEquipmentActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}

	AActor* Character = AttachedCharacter.Get();
	FName PrevSocket = CurrentSocketName;

	bIsAttached = false;
	bIsInActiveState = false;
	CurrentSocketName = NAME_None;
	AttachTarget = nullptr;
	AttachedCharacter.Reset();

	BroadcastAttachmentEvent(false, Character, PrevSocket);
	NotifyStanceOfAttachment(false);
}

void USuspenseCoreEquipmentAttachmentComponent::UpdateAttachmentState(bool bMakeActive, bool bAnimated)
{
	SUSPENSECORE_LOG(Log, TEXT("UpdateAttachmentState: MakeActive: %d, Animated: %d"),
		bMakeActive, bAnimated);

	if (!bIsAttached)
	{
		SUSPENSECORE_LOG(Warning, TEXT("UpdateAttachmentState: Not attached"));
		return;
	}

	if (bIsInActiveState == bMakeActive)
	{
		return; // Already in target state
	}

	AActor* Character = AttachedCharacter.Get();
	if (!Character)
	{
		return;
	}

	// Play animation if requested
	if (bAnimated)
	{
		PlayAttachmentAnimation(bMakeActive, 0.0f);
	}

	// Re-attach to new socket
	AttachToCharacter(Character, bMakeActive, nullptr);
}

void USuspenseCoreEquipmentAttachmentComponent::PlayAttachmentAnimation(bool bToActive, float Duration)
{
	SUSPENSECORE_LOG(Log, TEXT("PlayAttachmentAnimation: ToActive: %d, Duration: %f"),
		bToActive, Duration);

	// Implementation stub - play attachment transition animation
	AnimationState.bIsPlaying = true;
	AnimationState.StartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

TScriptInterface<ISuspenseWeaponAnimation> USuspenseCoreEquipmentAttachmentComponent::GetAnimationInterface() const
{
	// Implementation stub - return animation interface
	return CachedAnimationInterface;
}

void USuspenseCoreEquipmentAttachmentComponent::OnAttachmentAnimationComplete()
{
	SUSPENSECORE_LOG(Log, TEXT("OnAttachmentAnimationComplete"));

	AnimationState.bIsPlaying = false;
	AnimationState.CurrentMontage = nullptr;
}

void USuspenseCoreEquipmentAttachmentComponent::LinkStanceComponent(USuspenseCoreWeaponStanceComponent* StanceComponent)
{
	if (StanceComponent)
	{
		LinkedStanceComponent = StanceComponent;
		SUSPENSECORE_LOG(Log, TEXT("LinkStanceComponent: Linked"));
	}
}

void USuspenseCoreEquipmentAttachmentComponent::NotifyStanceOfAttachment(bool bAttached)
{
	if (USuspenseCoreWeaponStanceComponent* StanceComp = LinkedStanceComponent.Get())
	{
		if (bAttached && SpawnedEquipmentActor)
		{
			StanceComp->OnEquipmentChanged(SpawnedEquipmentActor);
			StanceComp->SetWeaponStance(CurrentWeaponType, false);
		}
		else
		{
			StanceComp->ClearWeaponStance(false);
		}
	}
}

FGameplayTag USuspenseCoreEquipmentAttachmentComponent::GetWeaponTypeTag() const
{
	return CurrentWeaponType;
}

FName USuspenseCoreEquipmentAttachmentComponent::GetAttachmentSocketName(bool bActive) const
{
	// Implementation stub - get socket name from item data
	return NAME_None;
}

FTransform USuspenseCoreEquipmentAttachmentComponent::GetAttachmentOffset(bool bActive) const
{
	// Implementation stub - get offset from item data
	return FTransform::Identity;
}

int32 USuspenseCoreEquipmentAttachmentComponent::PredictAttachment(AActor* Character, bool bUseActiveSocket, const FName& SocketName,
                                                                   const FTransform& Offset, const FGameplayTag& WeaponType)
{
	FAttachmentPredictionData Prediction;
	Prediction.PredictionKey = NextAttachmentPredictionKey++;
	Prediction.bPredictedAttached = true;
	Prediction.PredictedSocketName = SocketName;
	Prediction.PredictedOffset = Offset;
	Prediction.PredictedCharacter = Character;
	Prediction.bPredictedActive = bUseActiveSocket;
	Prediction.WeaponTypeTag = WeaponType;
	Prediction.PredictionTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	AttachmentPredictions.Add(Prediction);
	ApplyPredictedAttachment(Prediction);

	return Prediction.PredictionKey;
}

int32 USuspenseCoreEquipmentAttachmentComponent::PredictDetachment()
{
	FAttachmentPredictionData Prediction;
	Prediction.PredictionKey = NextAttachmentPredictionKey++;
	Prediction.bPredictedAttached = false;
	Prediction.PredictionTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	AttachmentPredictions.Add(Prediction);
	ApplyPredictedAttachment(Prediction);

	return Prediction.PredictionKey;
}

void USuspenseCoreEquipmentAttachmentComponent::ConfirmAttachmentPrediction(int32 PredictionKey, bool bSuccess)
{
	SUSPENSECORE_LOG(Verbose, TEXT("ConfirmAttachmentPrediction Key: %d, Success: %d"),
		PredictionKey, bSuccess);

	AttachmentPredictions.RemoveAll([PredictionKey](const FAttachmentPredictionData& Pred)
	{
		return Pred.PredictionKey == PredictionKey;
	});
}

TArray<FSocketSearchResult> USuspenseCoreEquipmentAttachmentComponent::GetValidSocketsForItem(const FSuspenseUnifiedItemData& ItemData, USkeletalMeshComponent* TargetMesh) const
{
	TArray<FSocketSearchResult> Results;
	// Implementation stub - search for valid sockets
	return Results;
}

bool USuspenseCoreEquipmentAttachmentComponent::ValidateSocket(const FName& SocketName, USkeletalMeshComponent* TargetMesh) const
{
	return TargetMesh && TargetMesh->DoesSocketExist(SocketName);
}

void USuspenseCoreEquipmentAttachmentComponent::OnEquipmentInitialized()
{
	Super::OnEquipmentInitialized();
	SUSPENSECORE_LOG(Log, TEXT("OnEquipmentInitialized"));
}

void USuspenseCoreEquipmentAttachmentComponent::OnEquippedItemChanged(const FSuspenseInventoryItemInstance& OldItem, const FSuspenseInventoryItemInstance& NewItem)
{
	Super::OnEquippedItemChanged(OldItem, NewItem);
	SUSPENSECORE_LOG(Log, TEXT("OnEquippedItemChanged: %s -> %s"),
		*OldItem.ItemID.ToString(), *NewItem.ItemID.ToString());
}

AActor* USuspenseCoreEquipmentAttachmentComponent::SpawnEquipmentActor(const FSuspenseUnifiedItemData& ItemData)
{
	SUSPENSECORE_LOG(Log, TEXT("SpawnEquipmentActor"));
	// Implementation stub - spawn equipment actor from item data
	return nullptr;
}

void USuspenseCoreEquipmentAttachmentComponent::DestroyEquipmentActor()
{
	if (SpawnedEquipmentActor && bDidSpawnActor)
	{
		SpawnedEquipmentActor->Destroy();
	}
	SpawnedEquipmentActor = nullptr;
	bDidSpawnActor = false;
}

FName USuspenseCoreEquipmentAttachmentComponent::FindBestAttachmentSocket(USkeletalMeshComponent* TargetMesh, const FSuspenseUnifiedItemData& ItemData, bool bForActive) const
{
	// Implementation stub - find best socket based on item data and availability
	return FName("WeaponSocket");
}

FName USuspenseCoreEquipmentAttachmentComponent::GetDefaultSocketForSlot(const FGameplayTag& SlotType, bool bForActive) const
{
	// Implementation stub - return default socket for slot type
	return FName("WeaponSocket");
}

void USuspenseCoreEquipmentAttachmentComponent::ApplyAttachment(USceneComponent* ComponentToAttach, USceneComponent* TargetComponent,
                                                               const FName& SocketName, const FTransform& AttachmentOffset)
{
	if (!ComponentToAttach || !TargetComponent)
	{
		return;
	}

	FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, false);
	ComponentToAttach->AttachToComponent(TargetComponent, AttachRules, SocketName);
	ComponentToAttach->SetRelativeTransform(AttachmentOffset);
}

void USuspenseCoreEquipmentAttachmentComponent::BroadcastAttachmentEvent(bool bAttached, AActor* Character, const FName& SocketName)
{
	SUSPENSECORE_LOG(Verbose, TEXT("BroadcastAttachmentEvent: Attached: %d, Character: %s, Socket: %s"),
		bAttached, *GetNameSafe(Character), *SocketName.ToString());

	// Implementation stub - broadcast to event bus
}

USkeletalMeshComponent* USuspenseCoreEquipmentAttachmentComponent::GetCharacterMesh(AActor* Character) const
{
	if (!Character)
	{
		return nullptr;
	}

	// Try to find skeletal mesh component
	return Character->FindComponentByClass<USkeletalMeshComponent>();
}

void USuspenseCoreEquipmentAttachmentComponent::UpdateReplicatedAttachmentState()
{
	// State is already replicated via UPROPERTY replication
	SUSPENSECORE_LOG(Verbose, TEXT("UpdateReplicatedAttachmentState"));
}

void USuspenseCoreEquipmentAttachmentComponent::ApplyPredictedAttachment(const FAttachmentPredictionData& Prediction)
{
	SUSPENSECORE_LOG(Verbose, TEXT("ApplyPredictedAttachment Key: %d"), Prediction.PredictionKey);
	// Implementation stub - apply predicted attachment locally
}

void USuspenseCoreEquipmentAttachmentComponent::RevertPredictedAttachment(const FAttachmentPredictionData& Prediction)
{
	SUSPENSECORE_LOG(Verbose, TEXT("RevertPredictedAttachment Key: %d"), Prediction.PredictionKey);
	// Implementation stub - revert predicted attachment
}

void USuspenseCoreEquipmentAttachmentComponent::CleanupExpiredPredictions()
{
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float ExpirationTime = 2.0f;

	AttachmentPredictions.RemoveAll([CurrentTime, ExpirationTime](const FAttachmentPredictionData& Pred)
	{
		return (CurrentTime - Pred.PredictionTime) > ExpirationTime;
	});
}

void USuspenseCoreEquipmentAttachmentComponent::UpdateAnimationState(float DeltaTime)
{
	if (AnimationState.bIsPlaying)
	{
		// Implementation stub - update animation state
	}
}

FGameplayTag USuspenseCoreEquipmentAttachmentComponent::GetWeaponArchetypeFromItem() const
{
	// Implementation stub - get weapon archetype from item data
	return FGameplayTag();
}

void USuspenseCoreEquipmentAttachmentComponent::OnRep_AttachmentState()
{
	SUSPENSECORE_LOG(Verbose, TEXT("OnRep_AttachmentState"));
	// Implementation stub - handle attachment state replication
}

void USuspenseCoreEquipmentAttachmentComponent::OnRep_SpawnedEquipmentActor()
{
	SUSPENSECORE_LOG(Verbose, TEXT("OnRep_SpawnedEquipmentActor"));
	// Implementation stub - handle spawned actor replication
}

void USuspenseCoreEquipmentAttachmentComponent::OnRep_AnimationState()
{
	SUSPENSECORE_LOG(Verbose, TEXT("OnRep_AnimationState"));
	// Implementation stub - handle animation state replication
}

bool USuspenseCoreEquipmentAttachmentComponent::ServerRequestAttachment_Validate(AActor* Character, bool bUseActiveSocket, FName RequestedSocket,
                                                                                 FTransform RequestedOffset, FGameplayTag WeaponType, int32 PredictionKey)
{
	return true;
}

void USuspenseCoreEquipmentAttachmentComponent::ServerRequestAttachment_Implementation(AActor* Character, bool bUseActiveSocket, FName RequestedSocket,
                                                                                       FTransform RequestedOffset, FGameplayTag WeaponType, int32 PredictionKey)
{
	SUSPENSECORE_LOG(Log, TEXT("ServerRequestAttachment: Character: %s, PredictionKey: %d"),
		*GetNameSafe(Character), PredictionKey);

	bool bSuccess = AttachToCharacter(Character, bUseActiveSocket, nullptr);
	ClientConfirmAttachment(PredictionKey, bSuccess, CurrentSocketName, CurrentAttachmentOffset);
}

bool USuspenseCoreEquipmentAttachmentComponent::ServerRequestDetachment_Validate(bool bMaintainTransform, int32 PredictionKey)
{
	return true;
}

void USuspenseCoreEquipmentAttachmentComponent::ServerRequestDetachment_Implementation(bool bMaintainTransform, int32 PredictionKey)
{
	SUSPENSECORE_LOG(Log, TEXT("ServerRequestDetachment: PredictionKey: %d"), PredictionKey);
	Detach(bMaintainTransform);
}

void USuspenseCoreEquipmentAttachmentComponent::ClientConfirmAttachment_Implementation(int32 PredictionKey, bool bSuccess, FName ActualSocket, FTransform ActualOffset)
{
	ConfirmAttachmentPrediction(PredictionKey, bSuccess);
}

void USuspenseCoreEquipmentAttachmentComponent::MulticastAttachment_Implementation(AActor* Actor, USceneComponent* Parent, FName Socket,
                                                                                   FTransform Offset, FGameplayTag WeaponType, bool bAnimated)
{
	// Implementation stub - multicast attachment to all clients
}

void USuspenseCoreEquipmentAttachmentComponent::MulticastDetachment_Implementation(AActor* Actor, bool bMaintainTransform)
{
	// Implementation stub - multicast detachment to all clients
}
