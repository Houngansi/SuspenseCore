// SuspenseCorePickupItem.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Pickup/SuspenseCorePickupItem.h"
#include "SuspenseCore/Utils/SuspenseCoreHelpers.h"
#include "SuspenseCore/Utils/SuspenseCoreInteractionSettings.h"
#include "SuspenseCore/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "Components/AudioComponent.h"
#include "Interfaces/Inventory/ISuspenseInventory.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/DataTable.h"
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCorePickup, Log, All);

ASuspenseCorePickupItem::ASuspenseCorePickupItem()
{
	bReplicates = true;
	SetReplicateMovement(true);

	// Get default settings
	const USuspenseCoreInteractionSettings* Settings = USuspenseCoreInteractionSettings::Get();
	TEnumAsByte<ECollisionChannel> TraceChannel =
		Settings ? Settings->DefaultTraceChannel : TEnumAsByte<ECollisionChannel>(ECC_Visibility);

	// Create root collision component
	SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
	RootComponent = SphereCollision;
	SphereCollision->InitSphereRadius(100.0f);
	SphereCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SphereCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	SphereCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SphereCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SphereCollision->SetCollisionResponseToChannel(TraceChannel, ECR_Block);

	// Create mesh component
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	MeshComponent->SetCollisionResponseToChannel(TraceChannel, ECR_Block);

	// Create VFX component (inactive by default)
	SpawnVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SpawnVFX"));
	SpawnVFXComponent->SetupAttachment(RootComponent);
	SpawnVFXComponent->bAutoActivate = false;

	// Create audio component (inactive by default)
	AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
	AudioComponent->SetupAttachment(RootComponent);
	AudioComponent->bAutoActivate = false;

	// Default values
	Amount = 1;
	bHasSavedAmmoState = false;
	SavedCurrentAmmo = 0.0f;
	SavedRemainingAmmo = 0.0f;
	DestroyDelay = 0.1f;
	InteractionPriority = 0;
	InteractionDistanceOverride = 0.0f;
	bDataCached = false;
	bUseRuntimeInstance = false;
}

//==================================================================
// AActor Interface
//==================================================================

void ASuspenseCorePickupItem::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogSuspenseCorePickup, Log, TEXT("Pickup BeginPlay: %s with ItemID: %s"),
		*GetName(), *ItemID.ToString());

	// Load item data from DataTable
	if (!ItemID.IsNone())
	{
		if (LoadItemData())
		{
			ApplyItemVisuals();
			ApplyItemAudio();
			ApplyItemVFX();
		}
		else
		{
			UE_LOG(LogSuspenseCorePickup, Error, TEXT("Pickup %s failed to load item data for: %s"),
				*GetName(), *ItemID.ToString());
		}
	}
	else
	{
		UE_LOG(LogSuspenseCorePickup, Warning, TEXT("Pickup %s has no ItemID set!"), *GetName());
	}

	// Broadcast spawn event through EventBus
	BroadcastPickupSpawned();
}

void ASuspenseCorePickupItem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CachedEventBus.Reset();
	Super::EndPlay(EndPlayReason);
}

void ASuspenseCorePickupItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASuspenseCorePickupItem, ItemID);
	DOREPLIFETIME(ASuspenseCorePickupItem, Amount);
	DOREPLIFETIME(ASuspenseCorePickupItem, bHasSavedAmmoState);
	DOREPLIFETIME(ASuspenseCorePickupItem, SavedCurrentAmmo);
	DOREPLIFETIME(ASuspenseCorePickupItem, SavedRemainingAmmo);
	DOREPLIFETIME(ASuspenseCorePickupItem, bUseRuntimeInstance);
	DOREPLIFETIME(ASuspenseCorePickupItem, PresetRuntimeProperties);
}

void ASuspenseCorePickupItem::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// In editor, try to load and apply visuals
	if (GetWorld() && GetWorld()->IsEditorWorld() && !ItemID.IsNone())
	{
		if (LoadItemData())
		{
			ApplyItemVisuals();
		}
	}
}

//==================================================================
// ISuspenseCoreEventEmitter Interface
//==================================================================

void ASuspenseCorePickupItem::EmitEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& Data)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (EventBus && EventTag.IsValid())
	{
		EventBus->Publish(EventTag, Data);
	}
}

USuspenseCoreEventBus* ASuspenseCorePickupItem::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	USuspenseCoreEventBus* EventBus = USuspenseCoreHelpers::GetEventBus(this);
	if (EventBus)
	{
		CachedEventBus = EventBus;
	}

	return EventBus;
}

//==================================================================
// ISuspenseCoreInteractable Interface
//==================================================================

bool ASuspenseCorePickupItem::CanInteract_Implementation(APlayerController* InstigatingController) const
{
	UE_LOG(LogSuspenseCorePickup, Log, TEXT("CanInteract: Checking for %s"), *GetName());

	if (!InstigatingController || !InstigatingController->GetPawn())
	{
		UE_LOG(LogSuspenseCorePickup, Warning, TEXT("CanInteract: No controller or pawn"));
		return false;
	}

	// Must have valid item ID and cached data
	if (ItemID.IsNone() || !bDataCached)
	{
		UE_LOG(LogSuspenseCorePickup, Warning,
			TEXT("CanInteract: Failed - ItemID=%s, DataCached=%d"),
			*ItemID.ToString(), bDataCached);
		return false;
	}

	// On client, always allow (server will validate)
	if (!HasAuthority())
	{
		return true;
	}

	// Check if can be picked up
	return CanPickup_Implementation(InstigatingController->GetPawn());
}

bool ASuspenseCorePickupItem::Interact_Implementation(APlayerController* InstigatingController)
{
	if (!HasAuthority())
	{
		UE_LOG(LogSuspenseCorePickup, Warning, TEXT("Interact called on client for %s"), *GetName());
		return false;
	}

	if (!InstigatingController || !InstigatingController->GetPawn())
	{
		return false;
	}

	AActor* Pawn = InstigatingController->GetPawn();

	// Broadcast interaction started through EventBus
	BroadcastInteractionStarted(InstigatingController);

	bool bSuccess = ExecutePickup_Implementation(Pawn);

	// Broadcast interaction completed through EventBus
	BroadcastInteractionCompleted(InstigatingController, bSuccess);

	return bSuccess;
}

FGameplayTag ASuspenseCorePickupItem::GetInteractionType_Implementation() const
{
	if (!bDataCached)
	{
		LoadItemData();
	}

	if (bDataCached && CachedItemData.bIsWeapon)
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Interaction.Type.Weapon"));
	}

	if (bDataCached && CachedItemData.bIsAmmo)
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Interaction.Type.Ammo"));
	}

	return FGameplayTag::RequestGameplayTag(TEXT("Interaction.Type.Pickup"));
}

FText ASuspenseCorePickupItem::GetInteractionPrompt_Implementation() const
{
	if (!bDataCached)
	{
		LoadItemData();
	}

	if (bDataCached)
	{
		return FText::Format(FText::FromString(TEXT("Pick up {0}")), CachedItemData.Identity.DisplayName);
	}

	return FText::FromString(TEXT("Pick up"));
}

int32 ASuspenseCorePickupItem::GetInteractionPriority_Implementation() const
{
	return InteractionPriority;
}

float ASuspenseCorePickupItem::GetInteractionDistance_Implementation() const
{
	if (InteractionDistanceOverride > 0.0f)
	{
		return InteractionDistanceOverride;
	}

	const USuspenseCoreInteractionSettings* Settings = USuspenseCoreInteractionSettings::Get();
	return Settings ? Settings->DefaultTraceDistance : 300.0f;
}

void ASuspenseCorePickupItem::OnFocusGained_Implementation(APlayerController* InstigatingController)
{
	BroadcastFocusChanged(InstigatingController, true);
	HandleInteractionFeedback(true);
}

void ASuspenseCorePickupItem::OnFocusLost_Implementation(APlayerController* InstigatingController)
{
	BroadcastFocusChanged(InstigatingController, false);
	HandleInteractionFeedback(false);
}

//==================================================================
// ISuspenseCorePickup Interface
//==================================================================

FName ASuspenseCorePickupItem::GetItemID_Implementation() const
{
	return ItemID;
}

void ASuspenseCorePickupItem::SetItemID_Implementation(FName NewItemID)
{
	if (ItemID != NewItemID)
	{
		ItemID = NewItemID;
		bDataCached = false;

		if (HasAuthority() || (GetWorld() && GetWorld()->IsEditorWorld()))
		{
			LoadItemData();
		}
	}
}

int32 ASuspenseCorePickupItem::GetQuantity_Implementation() const
{
	return Amount;
}

void ASuspenseCorePickupItem::SetQuantity_Implementation(int32 NewQuantity)
{
	Amount = FMath::Max(1, NewQuantity);
}

bool ASuspenseCorePickupItem::CanPickup_Implementation(AActor* InstigatorActor) const
{
	UE_LOG(LogSuspenseCorePickup, Log,
		TEXT("CanPickup: Checking pickup %s for actor %s"),
		*GetName(), *GetNameSafe(InstigatorActor));

	if (!InstigatorActor)
	{
		return false;
	}

	if (!bDataCached)
	{
		LoadItemData();
	}

	if (!bDataCached)
	{
		UE_LOG(LogSuspenseCorePickup, Warning,
			TEXT("CanPickup: Failed to load item data for %s"),
			*GetName());
		return false;
	}

	// Validate item type hierarchy
	static const FGameplayTag BaseItemTag = FGameplayTag::RequestGameplayTag(TEXT("Item"));
	if (!CachedItemData.Classification.ItemType.MatchesTag(BaseItemTag))
	{
		UE_LOG(LogSuspenseCorePickup, Error,
			TEXT("CanPickup: Item type %s is not in Item.* hierarchy!"),
			*CachedItemData.Classification.ItemType.ToString());
		return false;
	}

	// Check through helper with EventBus integration
	return USuspenseCoreHelpers::CanActorPickupItem(InstigatorActor, ItemID, Amount);
}

bool ASuspenseCorePickupItem::ExecutePickup_Implementation(AActor* InstigatorActor)
{
	if (!HasAuthority() || !InstigatorActor)
	{
		return false;
	}

	if (!CanPickup_Implementation(InstigatorActor))
	{
		return false;
	}

	// Try to add to inventory
	if (TryAddToInventory(InstigatorActor))
	{
		OnPickedUp(InstigatorActor);
		return true;
	}

	return false;
}

bool ASuspenseCorePickupItem::HasAmmoState_Implementation() const
{
	return bHasSavedAmmoState;
}

bool ASuspenseCorePickupItem::GetAmmoState_Implementation(float& OutCurrentAmmo, float& OutReserveAmmo) const
{
	if (bHasSavedAmmoState)
	{
		OutCurrentAmmo = SavedCurrentAmmo;
		OutReserveAmmo = SavedRemainingAmmo;
		return true;
	}

	return false;
}

void ASuspenseCorePickupItem::SetAmmoState_Implementation(float CurrentAmmo, float ReserveAmmo)
{
	bHasSavedAmmoState = true;
	SavedCurrentAmmo = CurrentAmmo;
	SavedRemainingAmmo = ReserveAmmo;
}

FGameplayTag ASuspenseCorePickupItem::GetItemType_Implementation() const
{
	if (!bDataCached)
	{
		LoadItemData();
	}

	if (bDataCached)
	{
		return CachedItemData.GetEffectiveItemType();
	}

	return FGameplayTag::RequestGameplayTag(TEXT("Item.Generic"));
}

bool ASuspenseCorePickupItem::CreateInventoryInstance_Implementation(FSuspenseCoreItemInstance& OutInstance) const
{
	// Use full runtime instance if available
	if (bUseRuntimeInstance && RuntimeInstance.IsValid())
	{
		OutInstance = RuntimeInstance;
		UE_LOG(LogSuspenseCorePickup, Log,
			TEXT("CreateInventoryInstance: Using full runtime instance for %s"),
			*ItemID.ToString());
		return true;
	}

	// Create new instance
	if (!bDataCached)
	{
		LoadItemData();
	}

	if (!bDataCached)
	{
		return false;
	}

	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (!DataManager)
	{
		return false;
	}

	if (!DataManager->CreateItemInstance(ItemID, Amount, OutInstance))
	{
		return false;
	}

	// Apply preset properties
	for (const FSuspenseCorePresetProperty& PropertyPair : PresetRuntimeProperties)
	{
		OutInstance.SetProperty(PropertyPair.PropertyName, PropertyPair.PropertyValue);
	}

	// Apply ammo state for weapons
	if (CachedItemData.bIsWeapon && bHasSavedAmmoState)
	{
		OutInstance.WeaponState.bHasState = true;
		OutInstance.WeaponState.CurrentAmmo = SavedCurrentAmmo;
		OutInstance.WeaponState.ReserveAmmo = SavedRemainingAmmo;
	}

	return true;
}

FGameplayTag ASuspenseCorePickupItem::GetItemRarity_Implementation() const
{
	if (!bDataCached)
	{
		LoadItemData();
	}

	return bDataCached ? CachedItemData.Classification.Rarity : FGameplayTag();
}

FText ASuspenseCorePickupItem::GetDisplayName_Implementation() const
{
	if (!bDataCached)
	{
		LoadItemData();
	}

	return bDataCached ? CachedItemData.Identity.DisplayName : FText::FromString(ItemID.ToString());
}

bool ASuspenseCorePickupItem::IsStackable_Implementation() const
{
	if (!bDataCached)
	{
		LoadItemData();
	}

	return bDataCached && CachedItemData.InventoryProps.MaxStackSize > 1;
}

float ASuspenseCorePickupItem::GetWeight_Implementation() const
{
	if (!bDataCached)
	{
		LoadItemData();
	}

	return bDataCached ? CachedItemData.InventoryProps.Weight : 1.0f;
}

//==================================================================
// Data Access
//==================================================================

bool ASuspenseCorePickupItem::GetItemData(FSuspenseCoreItemData& OutItemData) const
{
	if (!bDataCached)
	{
		LoadItemData();
	}

	if (bDataCached)
	{
		OutItemData = CachedItemData;
		return true;
	}

	return false;
}

//==================================================================
// Initialization
//==================================================================

void ASuspenseCorePickupItem::InitializeFromInstance(const FSuspenseCoreItemInstance& Instance)
{
	if (!Instance.IsValid())
	{
		UE_LOG(LogSuspenseCorePickup, Warning, TEXT("InitializeFromInstance: Invalid instance provided"));
		return;
	}

	RuntimeInstance = Instance;
	bUseRuntimeInstance = true;

	ItemID = Instance.ItemID;
	Amount = Instance.Quantity;

	// Handle weapon ammo state
	if (Instance.WeaponState.bHasState)
	{
		bHasSavedAmmoState = true;
		SavedCurrentAmmo = Instance.WeaponState.CurrentAmmo;
		SavedRemainingAmmo = Instance.WeaponState.ReserveAmmo;
	}

	if (LoadItemData())
	{
		ApplyItemVisuals();
		ApplyItemAudio();
		ApplyItemVFX();
	}

	UE_LOG(LogSuspenseCorePickup, Log,
		TEXT("InitializeFromInstance: Initialized pickup for %s with full runtime state"),
		*ItemID.ToString());
}

void ASuspenseCorePickupItem::InitializeFromItemID(FName InItemID, int32 InQuantity)
{
	if (InItemID.IsNone())
	{
		UE_LOG(LogSuspenseCorePickup, Warning,
			TEXT("InitializeFromItemID: Invalid ItemID provided"));
		return;
	}

	ItemID = InItemID;
	Amount = FMath::Max(1, InQuantity);
	bUseRuntimeInstance = false;

	if (LoadItemData())
	{
		ApplyItemVisuals();
		ApplyItemAudio();
		ApplyItemVFX();
	}

	UE_LOG(LogSuspenseCorePickup, Log,
		TEXT("InitializeFromItemID: Initialized pickup for %s x%d"),
		*ItemID.ToString(), Amount);
}

void ASuspenseCorePickupItem::SetPickupAmmoState(bool bHasState, float CurrentAmmo, float RemainingAmmo)
{
	bHasSavedAmmoState = bHasState;
	SavedCurrentAmmo = CurrentAmmo;
	SavedRemainingAmmo = RemainingAmmo;
}

//==================================================================
// Preset Property Management
//==================================================================

float ASuspenseCorePickupItem::GetPresetProperty(FName PropertyName, float DefaultValue) const
{
	const FSuspenseCorePresetProperty* Found = FindPresetProperty(PropertyName);
	return Found ? Found->PropertyValue : DefaultValue;
}

void ASuspenseCorePickupItem::SetPresetProperty(FName PropertyName, float Value)
{
	if (!HasAuthority())
	{
		UE_LOG(LogSuspenseCorePickup, Warning,
			TEXT("SetPresetProperty called on client for %s"), *GetName());
		return;
	}

	FSuspenseCorePresetProperty* Found = FindPresetProperty(PropertyName);
	if (Found)
	{
		Found->PropertyValue = Value;
	}
	else
	{
		PresetRuntimeProperties.Add(FSuspenseCorePresetProperty(PropertyName, Value));
	}
}

bool ASuspenseCorePickupItem::HasPresetProperty(FName PropertyName) const
{
	return FindPresetProperty(PropertyName) != nullptr;
}

bool ASuspenseCorePickupItem::RemovePresetProperty(FName PropertyName)
{
	if (!HasAuthority())
	{
		return false;
	}

	const int32 RemovedCount = PresetRuntimeProperties.RemoveAll(
		[PropertyName](const FSuspenseCorePresetProperty& Pair)
		{
			return Pair.PropertyName == PropertyName;
		});

	return RemovedCount > 0;
}

TMap<FName, float> ASuspenseCorePickupItem::GetPresetPropertiesAsMap() const
{
	TMap<FName, float> ResultMap;

	for (const FSuspenseCorePresetProperty& Pair : PresetRuntimeProperties)
	{
		ResultMap.Add(Pair.PropertyName, Pair.PropertyValue);
	}

	return ResultMap;
}

void ASuspenseCorePickupItem::SetPresetPropertiesFromMap(const TMap<FName, float>& NewProperties)
{
	if (!HasAuthority())
	{
		return;
	}

	PresetRuntimeProperties.Empty(NewProperties.Num());

	for (const auto& PropertyPair : NewProperties)
	{
		PresetRuntimeProperties.Add(
			FSuspenseCorePresetProperty(PropertyPair.Key, PropertyPair.Value)
		);
	}
}

//==================================================================
// Event Handlers
//==================================================================

bool ASuspenseCorePickupItem::OnPickedUp_Implementation(AActor* InstigatorActor)
{
	UE_LOG(LogSuspenseCorePickup, Log, TEXT("Item %s picked up by %s"),
		*ItemID.ToString(), *InstigatorActor->GetName());

	// Broadcast through EventBus
	BroadcastPickupCollected(InstigatorActor);

	// Schedule destruction
	SetLifeSpan(DestroyDelay);

	// Disable collision
	SphereCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Hide visuals
	if (MeshComponent)
	{
		MeshComponent->SetVisibility(false);
	}

	// Play collect VFX
	if (bDataCached && !CachedItemData.Visuals.PickupCollectVFX.IsNull())
	{
		UNiagaraSystem* CollectVFX = CachedItemData.Visuals.PickupCollectVFX.LoadSynchronous();
		if (CollectVFX)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(),
				CollectVFX,
				GetActorLocation(),
				GetActorRotation()
			);
		}
	}

	// Play pickup sound
	if (bDataCached && !CachedItemData.Audio.PickupSound.IsNull())
	{
		USoundBase* Sound = CachedItemData.Audio.PickupSound.LoadSynchronous();
		if (Sound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
		}
	}

	return true;
}

//==================================================================
// Data Management
//==================================================================

bool ASuspenseCorePickupItem::LoadItemData() const
{
	if (ItemID.IsNone())
	{
		UE_LOG(LogSuspenseCorePickup, Warning, TEXT("LoadItemData: ItemID is None"));
		return false;
	}

	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (!DataManager)
	{
		UE_LOG(LogSuspenseCorePickup, Warning, TEXT("LoadItemData: DataManager not found"));
		return false;
	}

	if (DataManager->GetItemData(ItemID, CachedItemData))
	{
		bDataCached = true;

		UE_LOG(LogSuspenseCorePickup, Log, TEXT("LoadItemData: Loaded data for %s"), *ItemID.ToString());

		// Call blueprint events for type-specific setup
		if (CachedItemData.bIsWeapon)
		{
			const_cast<ASuspenseCorePickupItem*>(this)->OnWeaponPickupSetup();
		}
		else if (CachedItemData.bIsArmor)
		{
			const_cast<ASuspenseCorePickupItem*>(this)->OnArmorPickupSetup();
		}

		return true;
	}

	UE_LOG(LogSuspenseCorePickup, Warning, TEXT("LoadItemData: Failed to load data for %s"), *ItemID.ToString());
	return false;
}

void ASuspenseCorePickupItem::ApplyItemVisuals()
{
	if (!bDataCached || !MeshComponent)
	{
		return;
	}

	if (!CachedItemData.Visuals.WorldMesh.IsNull())
	{
		UStaticMesh* Mesh = CachedItemData.Visuals.WorldMesh.LoadSynchronous();
		if (Mesh)
		{
			MeshComponent->SetStaticMesh(Mesh);
			UE_LOG(LogSuspenseCorePickup, Log, TEXT("Applied mesh for %s"), *ItemID.ToString());
		}
	}

	OnVisualsApplied();
}

void ASuspenseCorePickupItem::ApplyItemAudio()
{
	// Audio is played on pickup, not ambient
}

void ASuspenseCorePickupItem::ApplyItemVFX()
{
	if (!bDataCached || !SpawnVFXComponent)
	{
		return;
	}

	if (!CachedItemData.Visuals.PickupSpawnVFX.IsNull())
	{
		UNiagaraSystem* SpawnVFX = CachedItemData.Visuals.PickupSpawnVFX.LoadSynchronous();
		if (SpawnVFX)
		{
			SpawnVFXComponent->SetAsset(SpawnVFX);
			SpawnVFXComponent->Activate();
		}
	}
}

//==================================================================
// Utility Methods
//==================================================================

bool ASuspenseCorePickupItem::TryAddToInventory(AActor* InstigatorActor)
{
	if (!HasAuthority() || !InstigatorActor || !bDataCached)
	{
		UE_LOG(LogSuspenseCorePickup, Warning,
			TEXT("TryAddToInventory: Basic validation failed - HasAuth:%d, Actor:%s, DataCached:%d"),
			HasAuthority(), *GetNameSafe(InstigatorActor), bDataCached);
		return false;
	}

	// Validate item type hierarchy
	static const FGameplayTag BaseItemTag = FGameplayTag::RequestGameplayTag(TEXT("Item"));
	if (!CachedItemData.Classification.ItemType.MatchesTag(BaseItemTag))
	{
		UE_LOG(LogSuspenseCorePickup, Error,
			TEXT("TryAddToInventory: Item type %s is not in Item.* hierarchy!"),
			*CachedItemData.Classification.ItemType.ToString());
		return false;
	}

	// Create item instance
	FSuspenseCoreItemInstance ItemInstance;
	if (!CreateInventoryInstance_Implementation(ItemInstance))
	{
		UE_LOG(LogSuspenseCorePickup, Warning, TEXT("TryAddToInventory: Failed to create item instance"));
		return false;
	}

	// Find inventory component
	UObject* InventoryComponent = USuspenseCoreHelpers::FindInventoryComponent(InstigatorActor);
	if (!InventoryComponent)
	{
		UE_LOG(LogSuspenseCorePickup, Warning, TEXT("TryAddToInventory: No inventory component found"));
		return false;
	}

	// Check interface implementation
	if (!InventoryComponent->GetClass()->ImplementsInterface(USuspenseInventory::StaticClass()))
	{
		UE_LOG(LogSuspenseCorePickup, Warning, TEXT("TryAddToInventory: Inventory doesn't implement interface"));
		return false;
	}

	// Add to inventory directly by ID (let inventory handle validation)
	UE_LOG(LogSuspenseCorePickup, Log, TEXT("TryAddToInventory: Adding item through interface..."));

	bool bAdded = ISuspenseInventory::Execute_AddItemByID(InventoryComponent, ItemID, Amount);

	if (bAdded)
	{
		UE_LOG(LogSuspenseCorePickup, Log, TEXT("Successfully added %s to inventory"), *ItemID.ToString());
	}
	else
	{
		UE_LOG(LogSuspenseCorePickup, Warning, TEXT("Failed to add %s to inventory"), *ItemID.ToString());

		ISuspenseInventory::BroadcastInventoryError(
			InventoryComponent,
			ESuspenseInventoryErrorCode::NoSpace,
			TEXT("Pickup failed")
		);
	}

	return bAdded;
}

USuspenseCoreDataManager* ASuspenseCorePickupItem::GetDataManager() const
{
	return USuspenseCoreDataManager::Get(this);
}

void ASuspenseCorePickupItem::HandleInteractionFeedback(bool bGainedFocus)
{
	// Visual feedback can be implemented here
	// For example: outline effect, glow, etc.
}

//==================================================================
// EventBus Broadcasting
//==================================================================

void ASuspenseCorePickupItem::BroadcastInteractionStarted(APlayerController* InstigatingController)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus || !InstigatingController)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		this,
		ESuspenseCoreEventPriority::Normal
	);

	EventData.SetObject(TEXT("Interactable"), const_cast<ASuspenseCorePickupItem*>(this));
	EventData.SetObject(TEXT("Instigator"), InstigatingController);
	EventData.SetString(TEXT("InteractionType"), GetInteractionType_Implementation().ToString());
	EventData.SetString(TEXT("ItemID"), ItemID.ToString());

	static const FGameplayTag InteractionStartedTag =
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Interaction.Started"));

	EmitEvent(InteractionStartedTag, EventData);

	UE_LOG(LogSuspenseCorePickup, Log,
		TEXT("Broadcast InteractionStarted: %s by %s"),
		*GetName(), *InstigatingController->GetName());
}

void ASuspenseCorePickupItem::BroadcastInteractionCompleted(APlayerController* InstigatingController, bool bSuccess)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus || !InstigatingController)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		this,
		ESuspenseCoreEventPriority::Normal
	);

	EventData.SetObject(TEXT("Interactable"), const_cast<ASuspenseCorePickupItem*>(this));
	EventData.SetObject(TEXT("Instigator"), InstigatingController);
	EventData.SetBool(TEXT("Success"), bSuccess);
	EventData.SetString(TEXT("InteractionType"), GetInteractionType_Implementation().ToString());
	EventData.SetString(TEXT("ItemID"), ItemID.ToString());

	static const FGameplayTag InteractionCompletedTag =
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Interaction.Completed"));

	EmitEvent(InteractionCompletedTag, EventData);

	UE_LOG(LogSuspenseCorePickup, Log,
		TEXT("Broadcast InteractionCompleted: %s by %s, Success=%d"),
		*GetName(), *InstigatingController->GetName(), bSuccess);
}

void ASuspenseCorePickupItem::BroadcastFocusChanged(APlayerController* InstigatingController, bool bGainedFocus)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus || !InstigatingController)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		this,
		ESuspenseCoreEventPriority::Low
	);

	EventData.SetObject(TEXT("Interactable"), const_cast<ASuspenseCorePickupItem*>(this));
	EventData.SetObject(TEXT("Instigator"), InstigatingController);
	EventData.SetBool(TEXT("GainedFocus"), bGainedFocus);
	EventData.SetString(TEXT("InteractionType"), GetInteractionType_Implementation().ToString());
	EventData.SetString(TEXT("ItemID"), ItemID.ToString());

	static const FGameplayTag FocusChangedTag =
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Interaction.FocusChanged"));

	EmitEvent(FocusChangedTag, EventData);
}

void ASuspenseCorePickupItem::BroadcastPickupSpawned()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		this,
		ESuspenseCoreEventPriority::Normal
	);

	EventData.SetString(TEXT("ItemID"), ItemID.ToString());
	EventData.SetInt(TEXT("Amount"), Amount);
	EventData.SetVector(TEXT("Location"), GetActorLocation());
	EventData.SetObject(TEXT("PickupActor"), const_cast<ASuspenseCorePickupItem*>(this));

	static const FGameplayTag SpawnedTag =
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Pickup.Spawned"));

	EmitEvent(SpawnedTag, EventData);

	UE_LOG(LogSuspenseCorePickup, Log,
		TEXT("Broadcast PickupSpawned: ItemID=%s, Amount=%d"),
		*ItemID.ToString(), Amount);
}

void ASuspenseCorePickupItem::BroadcastPickupCollected(AActor* Collector)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus || !Collector)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		this,
		ESuspenseCoreEventPriority::Normal
	);

	EventData.SetString(TEXT("ItemID"), ItemID.ToString());
	EventData.SetInt(TEXT("Amount"), Amount);
	EventData.SetObject(TEXT("Collector"), Collector);
	EventData.SetObject(TEXT("PickupActor"), const_cast<ASuspenseCorePickupItem*>(this));
	EventData.SetVector(TEXT("Location"), GetActorLocation());

	static const FGameplayTag CollectedTag =
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Pickup.Collected"));

	EmitEvent(CollectedTag, EventData);

	UE_LOG(LogSuspenseCorePickup, Log,
		TEXT("Broadcast PickupCollected: ItemID=%s, Amount=%d, Collector=%s"),
		*ItemID.ToString(), Amount, *Collector->GetName());
}

//==================================================================
// Internal Helpers
//==================================================================

FSuspenseCorePresetProperty* ASuspenseCorePickupItem::FindPresetProperty(FName PropertyName)
{
	return PresetRuntimeProperties.FindByPredicate(
		[PropertyName](const FSuspenseCorePresetProperty& Pair)
		{
			return Pair.PropertyName == PropertyName;
		});
}

const FSuspenseCorePresetProperty* ASuspenseCorePickupItem::FindPresetProperty(FName PropertyName) const
{
	return PresetRuntimeProperties.FindByPredicate(
		[PropertyName](const FSuspenseCorePresetProperty& Pair)
		{
			return Pair.PropertyName == PropertyName;
		});
}
