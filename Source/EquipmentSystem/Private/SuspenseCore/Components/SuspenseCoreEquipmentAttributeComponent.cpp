// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreEquipmentAttributeComponent.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"

USuspenseCoreEquipmentAttributeComponent::USuspenseCoreEquipmentAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedComponent(true);

	CurrentAttributeSet = nullptr;
	WeaponAttributeSet = nullptr;
	ArmorAttributeSet = nullptr;
	AmmoAttributeSet = nullptr;
	AttributeReplicationVersion = 0;
	NextAttributePredictionKey = 1;
}

void USuspenseCoreEquipmentAttributeComponent::BeginPlay()
{
	Super::BeginPlay();
	SUSPENSECORE_LOG(Log, TEXT("BeginPlay"));
}

void USuspenseCoreEquipmentAttributeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USuspenseCoreEquipmentAttributeComponent, ReplicatedAttributes);
	DOREPLIFETIME(USuspenseCoreEquipmentAttributeComponent, ReplicatedAttributeSetClasses);
	DOREPLIFETIME(USuspenseCoreEquipmentAttributeComponent, AttributeReplicationVersion);
}

void USuspenseCoreEquipmentAttributeComponent::InitializeWithItemInstance(AActor* InOwner, UAbilitySystemComponent* InASC, const FSuspenseInventoryItemInstance& ItemInstance)
{
	Super::InitializeWithItemInstance(InOwner, InASC, ItemInstance);
	SUSPENSECORE_LOG(Log, TEXT("InitializeWithItemInstance: %s"), *ItemInstance.ItemID.ToString());

	FSuspenseUnifiedItemData ItemData;
	if (GetEquippedItemData(ItemData))
	{
		CreateAttributeSetsForItem(ItemData);
		ApplyItemEffects(ItemData);
	}
}

void USuspenseCoreEquipmentAttributeComponent::Cleanup()
{
	SUSPENSECORE_LOG(Log, TEXT("Cleanup"));

	RemoveItemEffects();
	CleanupAttributeSets();

	Super::Cleanup();
}

void USuspenseCoreEquipmentAttributeComponent::UpdateEquippedItem(const FSuspenseInventoryItemInstance& ItemInstance)
{
	Super::UpdateEquippedItem(ItemInstance);
	SUSPENSECORE_LOG(Log, TEXT("UpdateEquippedItem: %s"), *ItemInstance.ItemID.ToString());

	// Reapply effects with new item data
	FSuspenseUnifiedItemData ItemData;
	if (GetEquippedItemData(ItemData))
	{
		RemoveItemEffects();
		ApplyItemEffects(ItemData);
		ForceAttributeReplication();
	}
}

void USuspenseCoreEquipmentAttributeComponent::ApplyItemEffects(const FSuspenseUnifiedItemData& ItemData)
{
	SUSPENSECORE_LOG(Log, TEXT("ApplyItemEffects"));

	if (!CachedASC)
	{
		SUSPENSECORE_LOG(Error, TEXT("Cannot apply effects without ASC"));
		return;
	}

	ApplyPassiveEffects(ItemData);
	ApplyGrantedAbilities(ItemData);
	CollectReplicatedAttributes();
}

void USuspenseCoreEquipmentAttributeComponent::RemoveItemEffects()
{
	SUSPENSECORE_LOG(Log, TEXT("RemoveItemEffects"));

	if (!CachedASC)
	{
		return;
	}

	// Remove all applied effects
	for (const FActiveGameplayEffectHandle& Handle : AppliedEffectHandles)
	{
		if (Handle.IsValid())
		{
			CachedASC->RemoveActiveGameplayEffect(Handle);
		}
	}
	AppliedEffectHandles.Empty();

	// Remove all granted abilities
	for (const FGameplayAbilitySpecHandle& Handle : GrantedAbilityHandles)
	{
		if (Handle.IsValid())
		{
			CachedASC->ClearAbility(Handle);
		}
	}
	GrantedAbilityHandles.Empty();
}

int32 USuspenseCoreEquipmentAttributeComponent::PredictAttributeChange(const FString& AttributeName, float NewValue)
{
	FAttributePredictionData Prediction;
	Prediction.PredictionKey = NextAttributePredictionKey++;
	Prediction.AttributeName = AttributeName;
	Prediction.PredictedValue = NewValue;

	float CurrentValue = 0.0f;
	GetAttributeValue(AttributeName, CurrentValue);
	Prediction.OriginalValue = CurrentValue;
	Prediction.PredictionTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	ActiveAttributePredictions.Add(Prediction);

	SUSPENSECORE_LOG(Verbose, TEXT("PredictAttributeChange: %s = %f (Key: %d)"),
		*AttributeName, NewValue, Prediction.PredictionKey);

	return Prediction.PredictionKey;
}

void USuspenseCoreEquipmentAttributeComponent::ConfirmAttributePrediction(int32 PredictionKey, bool bSuccess, float ActualValue)
{
	SUSPENSECORE_LOG(Verbose, TEXT("ConfirmAttributePrediction Key: %d, Success: %d, Value: %f"),
		PredictionKey, bSuccess, ActualValue);

	// Remove prediction from active list
	ActiveAttributePredictions.RemoveAll([PredictionKey](const FAttributePredictionData& Pred)
	{
		return Pred.PredictionKey == PredictionKey;
	});
}

bool USuspenseCoreEquipmentAttributeComponent::GetAttributeValue(const FString& AttributeName, float& OutValue) const
{
	FScopeLock Lock(&AttributeCacheCriticalSection);

	// Check cache first
	if (const auto* CachedPair = AttributePropertyCache.Find(AttributeName))
	{
		if (CachedPair->Key && CachedPair->Value)
		{
			OutValue = GetAttributeValueFromProperty(CachedPair->Key, CachedPair->Value);
			return true;
		}
	}

	// Search all attribute sets
	TArray<UAttributeSet*> AttributeSets = { CurrentAttributeSet, WeaponAttributeSet, ArmorAttributeSet, AmmoAttributeSet };

	for (UAttributeSet* AttrSet : AttributeSets)
	{
		if (!AttrSet)
		{
			continue;
		}

		FProperty* Prop = FindAttributeProperty(AttrSet, AttributeName);
		if (Prop)
		{
			// Cache for future lookups
			const_cast<USuspenseCoreEquipmentAttributeComponent*>(this)->AttributePropertyCache.Add(AttributeName, TPair<UAttributeSet*, FProperty*>(AttrSet, Prop));
			OutValue = GetAttributeValueFromProperty(AttrSet, Prop);
			return true;
		}
	}

	return false;
}

void USuspenseCoreEquipmentAttributeComponent::SetAttributeValue(const FString& AttributeName, float NewValue, bool bForceReplication)
{
	SUSPENSECORE_LOG(Verbose, TEXT("SetAttributeValue: %s = %f"), *AttributeName, NewValue);

	FScopeLock Lock(&AttributeCacheCriticalSection);

	// Search all attribute sets
	TArray<UAttributeSet*> AttributeSets = { CurrentAttributeSet, WeaponAttributeSet, ArmorAttributeSet, AmmoAttributeSet };

	for (UAttributeSet* AttrSet : AttributeSets)
	{
		if (!AttrSet)
		{
			continue;
		}

		FProperty* Prop = FindAttributeProperty(AttrSet, AttributeName);
		if (Prop)
		{
			SetAttributeValueToProperty(AttrSet, Prop, NewValue);

			if (bForceReplication)
			{
				ForceAttributeReplication();
			}
			return;
		}
	}
}

TMap<FString, float> USuspenseCoreEquipmentAttributeComponent::GetAllAttributeValues() const
{
	TMap<FString, float> Result;

	// Implementation stub - collect all attributes from all sets
	SUSPENSECORE_LOG(Verbose, TEXT("GetAllAttributeValues"));

	return Result;
}

bool USuspenseCoreEquipmentAttributeComponent::HasAttribute(const FString& AttributeName) const
{
	float DummyValue;
	return GetAttributeValue(AttributeName, DummyValue);
}

bool USuspenseCoreEquipmentAttributeComponent::GetAttributeByTag(const FGameplayTag& AttributeTag, float& OutValue) const
{
	// Implementation stub - map tag to attribute name and get value
	SUSPENSECORE_LOG(Verbose, TEXT("GetAttributeByTag: %s"), *AttributeTag.ToString());
	return false;
}

void USuspenseCoreEquipmentAttributeComponent::ForceAttributeReplication()
{
	AttributeReplicationVersion++;
	CollectReplicatedAttributes();
	SUSPENSECORE_LOG(Verbose, TEXT("ForceAttributeReplication (Version: %d)"), AttributeReplicationVersion);
}

void USuspenseCoreEquipmentAttributeComponent::OnEquipmentInitialized()
{
	Super::OnEquipmentInitialized();
	SUSPENSECORE_LOG(Log, TEXT("OnEquipmentInitialized"));
}

void USuspenseCoreEquipmentAttributeComponent::OnEquippedItemChanged(const FSuspenseInventoryItemInstance& OldItem, const FSuspenseInventoryItemInstance& NewItem)
{
	Super::OnEquippedItemChanged(OldItem, NewItem);
	SUSPENSECORE_LOG(Log, TEXT("OnEquippedItemChanged: %s -> %s"),
		*OldItem.ItemID.ToString(), *NewItem.ItemID.ToString());
}

void USuspenseCoreEquipmentAttributeComponent::CreateAttributeSetsForItem(const FSuspenseUnifiedItemData& ItemData)
{
	SUSPENSECORE_LOG(Log, TEXT("CreateAttributeSetsForItem"));

	// Implementation stub - create appropriate attribute sets based on item type
	CleanupAttributeSets();
}

void USuspenseCoreEquipmentAttributeComponent::CleanupAttributeSets()
{
	SUSPENSECORE_LOG(Log, TEXT("CleanupAttributeSets"));

	if (CachedASC)
	{
		// Remove attribute sets from ASC
		// Note: Attribute sets are typically managed by the ASC
	}

	CurrentAttributeSet = nullptr;
	WeaponAttributeSet = nullptr;
	ArmorAttributeSet = nullptr;
	AmmoAttributeSet = nullptr;
	AttributeSetsByType.Empty();
	AttributePropertyCache.Empty();
}

void USuspenseCoreEquipmentAttributeComponent::ApplyInitializationEffect(UAttributeSet* AttributeSet, TSubclassOf<UGameplayEffect> InitEffect, const FSuspenseUnifiedItemData& ItemData)
{
	if (!CachedASC || !InitEffect || !AttributeSet)
	{
		return;
	}

	SUSPENSECORE_LOG(Log, TEXT("ApplyInitializationEffect"));

	// Implementation stub - apply initialization effect to attribute set
}

void USuspenseCoreEquipmentAttributeComponent::ApplyPassiveEffects(const FSuspenseUnifiedItemData& ItemData)
{
	SUSPENSECORE_LOG(Log, TEXT("ApplyPassiveEffects"));

	// Implementation stub - apply passive effects from item data
}

void USuspenseCoreEquipmentAttributeComponent::ApplyGrantedAbilities(const FSuspenseUnifiedItemData& ItemData)
{
	SUSPENSECORE_LOG(Log, TEXT("ApplyGrantedAbilities"));

	// Implementation stub - grant abilities from item data
}

void USuspenseCoreEquipmentAttributeComponent::CollectReplicatedAttributes()
{
	ReplicatedAttributes.Empty();

	// Implementation stub - collect all attribute values for replication
	SUSPENSECORE_LOG(Verbose, TEXT("CollectReplicatedAttributes"));
}

void USuspenseCoreEquipmentAttributeComponent::ApplyReplicatedAttributes()
{
	SUSPENSECORE_LOG(Verbose, TEXT("ApplyReplicatedAttributes"));

	// Implementation stub - apply replicated attributes to local sets
}

FProperty* USuspenseCoreEquipmentAttributeComponent::FindAttributeProperty(UAttributeSet* AttributeSet, const FString& AttributeName) const
{
	if (!AttributeSet)
	{
		return nullptr;
	}

	// Implementation stub - find property by name using reflection
	return nullptr;
}

float USuspenseCoreEquipmentAttributeComponent::GetAttributeValueFromProperty(UAttributeSet* AttributeSet, FProperty* Property) const
{
	if (!AttributeSet || !Property)
	{
		return 0.0f;
	}

	// Implementation stub - get value via reflection
	return 0.0f;
}

void USuspenseCoreEquipmentAttributeComponent::SetAttributeValueToProperty(UAttributeSet* AttributeSet, FProperty* Property, float Value)
{
	if (!AttributeSet || !Property)
	{
		return;
	}

	// Implementation stub - set value via reflection
}

FGameplayAttribute USuspenseCoreEquipmentAttributeComponent::GetGameplayAttributeFromProperty(UAttributeSet* AttributeSet, FProperty* Property) const
{
	// Implementation stub - convert property to GameplayAttribute
	return FGameplayAttribute();
}

void USuspenseCoreEquipmentAttributeComponent::OnRep_ReplicatedAttributes()
{
	SUSPENSECORE_LOG(Verbose, TEXT("OnRep_ReplicatedAttributes"));
	ApplyReplicatedAttributes();
}

void USuspenseCoreEquipmentAttributeComponent::OnRep_AttributeSetClasses()
{
	SUSPENSECORE_LOG(Verbose, TEXT("OnRep_AttributeSetClasses"));
	// Implementation stub - recreate attribute sets on clients
}

bool USuspenseCoreEquipmentAttributeComponent::ServerSetAttributeValue_Validate(const FString& AttributeName, float NewValue)
{
	return true;
}

void USuspenseCoreEquipmentAttributeComponent::ServerSetAttributeValue_Implementation(const FString& AttributeName, float NewValue)
{
	SetAttributeValue(AttributeName, NewValue, true);
}

bool USuspenseCoreEquipmentAttributeComponent::ServerApplyItemEffects_Validate(const FName& ItemID)
{
	return true;
}

void USuspenseCoreEquipmentAttributeComponent::ServerApplyItemEffects_Implementation(const FName& ItemID)
{
	SUSPENSECORE_LOG(Log, TEXT("ServerApplyItemEffects: %s"), *ItemID.ToString());

	FSuspenseUnifiedItemData ItemData;
	if (GetEquippedItemData(ItemData))
	{
		ApplyItemEffects(ItemData);
	}
}

bool USuspenseCoreEquipmentAttributeComponent::ServerRemoveItemEffects_Validate()
{
	return true;
}

void USuspenseCoreEquipmentAttributeComponent::ServerRemoveItemEffects_Implementation()
{
	RemoveItemEffects();
}
