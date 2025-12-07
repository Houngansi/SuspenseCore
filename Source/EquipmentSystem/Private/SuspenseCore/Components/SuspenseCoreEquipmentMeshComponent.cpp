// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreEquipmentMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "NiagaraComponent.h"
#include "Components/AudioComponent.h"

// Socket name constants
const FName USuspenseCoreEquipmentMeshComponent::DefaultMuzzleSocket = FName("Muzzle");
const FName USuspenseCoreEquipmentMeshComponent::DefaultScopeSocket = FName("Scope");
const FName USuspenseCoreEquipmentMeshComponent::DefaultMagazineSocket = FName("Magazine");

USuspenseCoreEquipmentMeshComponent::USuspenseCoreEquipmentMeshComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	MeshComponent = nullptr;
	ScopeCamera = nullptr;
	MuzzleFlashComponent = nullptr;
	AudioComponent = nullptr;
	bVisualsInitialized = false;
	NextPredictionKey = 1;
	AdditionalOffset = FTransform::Identity;
}

void USuspenseCoreEquipmentMeshComponent::BeginPlay()
{
	Super::BeginPlay();
	SUSPENSECORE_LOG(Log, TEXT("BeginPlay"));
}

void USuspenseCoreEquipmentMeshComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanupVisuals();
	Super::EndPlay(EndPlayReason);
}

void USuspenseCoreEquipmentMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Clean up expired predictions periodically
	CleanupExpiredPredictions();
}

bool USuspenseCoreEquipmentMeshComponent::InitializeFromItemInstance(const FSuspenseInventoryItemInstance& ItemInstance)
{
	SUSPENSECORE_LOG(Log, TEXT("InitializeFromItemInstance: %s"), *ItemInstance.ItemID.ToString());

	CurrentItemInstance = ItemInstance;

	FSuspenseUnifiedItemData ItemData;
	if (!GetEquippedItemData(ItemData))
	{
		SUSPENSECORE_LOG(Error, TEXT("Failed to get item data for: %s"), *ItemInstance.ItemID.ToString());
		return false;
	}

	CachedItemData = ItemData;
	InitializeVisualComponents(ItemData);
	LoadMeshFromItemData(ItemData);
	CreateDynamicMaterials();

	bVisualsInitialized = true;
	return true;
}

void USuspenseCoreEquipmentMeshComponent::UpdateVisualState(const FSuspenseInventoryItemInstance& ItemInstance)
{
	SUSPENSECORE_LOG(Verbose, TEXT("UpdateVisualState"));
	CurrentItemInstance = ItemInstance;

	// Update visual state based on item properties
	UpdateDynamicMaterials();
	NotifyVisualStateChanged();
}

void USuspenseCoreEquipmentMeshComponent::CleanupVisuals()
{
	SUSPENSECORE_LOG(Log, TEXT("CleanupVisuals"));

	// Cleanup effect components
	for (UNiagaraComponent* EffectComp : ActiveEffectComponents)
	{
		if (EffectComp && EffectComp->IsValidLowLevel())
		{
			EffectComp->DestroyComponent();
		}
	}
	ActiveEffectComponents.Empty();

	// Cleanup pooled effects
	for (UNiagaraComponent* PooledComp : PooledEffectComponents)
	{
		if (PooledComp && PooledComp->IsValidLowLevel())
		{
			PooledComp->DestroyComponent();
		}
	}
	PooledEffectComponents.Empty();

	// Cleanup dynamic materials
	DynamicMaterials.Empty();

	// Cleanup child components
	if (ScopeCamera)
	{
		ScopeCamera->DestroyComponent();
		ScopeCamera = nullptr;
	}

	if (MuzzleFlashComponent)
	{
		MuzzleFlashComponent->DestroyComponent();
		MuzzleFlashComponent = nullptr;
	}

	if (AudioComponent)
	{
		AudioComponent->DestroyComponent();
		AudioComponent = nullptr;
	}

	bVisualsInitialized = false;
}

void USuspenseCoreEquipmentMeshComponent::ApplyVisualState(const FEquipmentVisualState& NewState, bool bForceUpdate)
{
	FScopeLock Lock(&VisualStateCriticalSection);

	if (!bForceUpdate && CurrentVisualState == NewState)
	{
		return;
	}

	PreviousVisualState = CurrentVisualState;
	CurrentVisualState = NewState;

	UpdateDynamicMaterials();
}

bool USuspenseCoreEquipmentMeshComponent::HasVisualStateChanged(const FEquipmentVisualState& OtherState) const
{
	FScopeLock Lock(&VisualStateCriticalSection);
	return CurrentVisualState != OtherState;
}

FVector USuspenseCoreEquipmentMeshComponent::GetSocketLocationSafe(const FName& SocketName) const
{
	if (MeshComponent && MeshComponent->DoesSocketExist(SocketName))
	{
		return MeshComponent->GetSocketLocation(SocketName);
	}
	return GetComponentLocation();
}

FRotator USuspenseCoreEquipmentMeshComponent::GetSocketRotationSafe(const FName& SocketName) const
{
	if (MeshComponent && MeshComponent->DoesSocketExist(SocketName))
	{
		return MeshComponent->GetSocketRotation(SocketName);
	}
	return GetComponentRotation();
}

FTransform USuspenseCoreEquipmentMeshComponent::GetSocketTransformSafe(const FName& SocketName) const
{
	if (MeshComponent && MeshComponent->DoesSocketExist(SocketName))
	{
		return MeshComponent->GetSocketTransform(SocketName);
	}
	return GetComponentTransform();
}

void USuspenseCoreEquipmentMeshComponent::ApplyOffsetTransform(const FTransform& Offset)
{
	AdditionalOffset = Offset;
	// Apply to mesh component if available
	if (MeshComponent)
	{
		MeshComponent->SetRelativeTransform(AdditionalOffset);
	}
}

void USuspenseCoreEquipmentMeshComponent::SetupWeaponVisuals(const FSuspenseUnifiedItemData& WeaponData)
{
	SUSPENSECORE_LOG(Log, TEXT("SetupWeaponVisuals"));
	// Implementation stub - setup weapon-specific visual components
}

FVector USuspenseCoreEquipmentMeshComponent::GetMuzzleLocation() const
{
	return GetSocketLocationSafe(DefaultMuzzleSocket);
}

FVector USuspenseCoreEquipmentMeshComponent::GetMuzzleDirection() const
{
	FRotator MuzzleRotation = GetSocketRotationSafe(DefaultMuzzleSocket);
	return MuzzleRotation.Vector();
}

int32 USuspenseCoreEquipmentMeshComponent::PlayMuzzleFlash()
{
	// Implementation stub - play muzzle flash with prediction
	return NextPredictionKey++;
}

void USuspenseCoreEquipmentMeshComponent::SetupScopeCamera(float FOV, bool bShouldAutoActivate)
{
	SUSPENSECORE_LOG(Log, TEXT("SetupScopeCamera FOV: %f"), FOV);
	// Implementation stub - setup scope camera
}

void USuspenseCoreEquipmentMeshComponent::SetScopeCameraActive(bool bActivate)
{
	if (ScopeCamera)
	{
		ScopeCamera->SetActive(bActivate);
	}
}

void USuspenseCoreEquipmentMeshComponent::SetConditionVisual(float ConditionPercent)
{
	FScopeLock Lock(&VisualStateCriticalSection);
	CurrentVisualState.ConditionPercent = FMath::Clamp(ConditionPercent, 0.0f, 1.0f);
	UpdateDynamicMaterials();
}

void USuspenseCoreEquipmentMeshComponent::SetRarityVisual(const FGameplayTag& RarityTag)
{
	SUSPENSECORE_LOG(Verbose, TEXT("SetRarityVisual: %s"), *RarityTag.ToString());
	// Implementation stub - set rarity visual effects
}

int32 USuspenseCoreEquipmentMeshComponent::PlayEquipmentEffect(const FGameplayTag& EffectType)
{
	// Implementation stub - play equipment effect with prediction
	return NextPredictionKey++;
}

void USuspenseCoreEquipmentMeshComponent::ConfirmEffectPrediction(int32 PredictionKey, bool bSuccess)
{
	SUSPENSECORE_LOG(Verbose, TEXT("ConfirmEffectPrediction Key: %d, Success: %d"), PredictionKey, bSuccess);
	// Implementation stub
}

void USuspenseCoreEquipmentMeshComponent::SetMaterialParameter(const FName& ParameterName, float Value)
{
	FScopeLock Lock(&VisualStateCriticalSection);
	CurrentVisualState.MaterialScalarParams.Add(ParameterName, Value);

	for (UMaterialInstanceDynamic* DynMat : DynamicMaterials)
	{
		if (DynMat)
		{
			DynMat->SetScalarParameterValue(ParameterName, Value);
		}
	}
}

void USuspenseCoreEquipmentMeshComponent::SetMaterialColorParameter(const FName& ParameterName, const FLinearColor& Color)
{
	FScopeLock Lock(&VisualStateCriticalSection);
	CurrentVisualState.MaterialVectorParams.Add(ParameterName, Color);

	for (UMaterialInstanceDynamic* DynMat : DynamicMaterials)
	{
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(ParameterName, Color);
		}
	}
}

FName USuspenseCoreEquipmentMeshComponent::GetAttachmentSocket(const FGameplayTag& ModificationType) const
{
	// Implementation stub - return attachment socket for modification type
	return NAME_None;
}

bool USuspenseCoreEquipmentMeshComponent::HasAttachmentSocket(const FGameplayTag& ModificationType) const
{
	FName SocketName = GetAttachmentSocket(ModificationType);
	return MeshComponent && MeshComponent->DoesSocketExist(SocketName);
}

void USuspenseCoreEquipmentMeshComponent::NotifyVisualStateChanged()
{
	BroadcastEquipmentUpdated();
}

void USuspenseCoreEquipmentMeshComponent::RequestStateSync()
{
	SUSPENSECORE_LOG(Verbose, TEXT("RequestStateSync"));
	// Implementation stub - request sync from server
}

void USuspenseCoreEquipmentMeshComponent::InitializeVisualComponents(const FSuspenseUnifiedItemData& ItemData)
{
	SUSPENSECORE_LOG(Log, TEXT("InitializeVisualComponents"));
	// Implementation stub - initialize visual components based on item type
}

bool USuspenseCoreEquipmentMeshComponent::LoadMeshFromItemData(const FSuspenseUnifiedItemData& ItemData)
{
	SUSPENSECORE_LOG(Log, TEXT("LoadMeshFromItemData"));
	// Implementation stub - load mesh from item data
	return true;
}

void USuspenseCoreEquipmentMeshComponent::CreateDynamicMaterials()
{
	if (!MeshComponent)
	{
		return;
	}

	DynamicMaterials.Empty();

	const int32 NumMaterials = MeshComponent->GetNumMaterials();
	for (int32 i = 0; i < NumMaterials; ++i)
	{
		UMaterialInterface* BaseMaterial = MeshComponent->GetMaterial(i);
		if (BaseMaterial)
		{
			UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			if (DynMat)
			{
				MeshComponent->SetMaterial(i, DynMat);
				DynamicMaterials.Add(DynMat);
			}
		}
	}
}

void USuspenseCoreEquipmentMeshComponent::UpdateDynamicMaterials()
{
	FScopeLock Lock(&VisualStateCriticalSection);

	// Apply material parameters from current visual state
	for (UMaterialInstanceDynamic* DynMat : DynamicMaterials)
	{
		if (!DynMat)
		{
			continue;
		}

		// Apply scalar parameters
		for (const auto& Param : CurrentVisualState.MaterialScalarParams)
		{
			DynMat->SetScalarParameterValue(Param.Key, Param.Value);
		}

		// Apply vector parameters
		for (const auto& Param : CurrentVisualState.MaterialVectorParams)
		{
			DynMat->SetVectorParameterValue(Param.Key, Param.Value);
		}

		// Apply condition and rarity
		DynMat->SetScalarParameterValue(FName("Condition"), CurrentVisualState.ConditionPercent);
		DynMat->SetScalarParameterValue(FName("RarityGlow"), CurrentVisualState.RarityGlowIntensity);
		DynMat->SetVectorParameterValue(FName("RarityColor"), CurrentVisualState.RarityColor);
	}
}

UNiagaraComponent* USuspenseCoreEquipmentMeshComponent::PlayVisualEffectAtLocation(const FGameplayTag& EffectType, const FVector& Location, const FRotator& Rotation)
{
	SUSPENSECORE_LOG(Verbose, TEXT("PlayVisualEffectAtLocation: %s"), *EffectType.ToString());
	// Implementation stub - play visual effect
	return nullptr;
}

void USuspenseCoreEquipmentMeshComponent::CleanupExpiredPredictions()
{
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	ActivePredictions.RemoveAll([CurrentTime](const FVisualEffectPrediction& Prediction)
	{
		const float Age = CurrentTime - Prediction.StartTime;
		return Age > (Prediction.Duration + 1.0f); // 1 second grace period
	});
}

void USuspenseCoreEquipmentMeshComponent::ApplyPredictedEffect(const FVisualEffectPrediction& Prediction)
{
	SUSPENSECORE_LOG(Verbose, TEXT("ApplyPredictedEffect Key: %d"), Prediction.PredictionKey);
	// Implementation stub
}

void USuspenseCoreEquipmentMeshComponent::StopPredictedEffect(const FVisualEffectPrediction& Prediction)
{
	SUSPENSECORE_LOG(Verbose, TEXT("StopPredictedEffect Key: %d"), Prediction.PredictionKey);
	// Implementation stub
}
