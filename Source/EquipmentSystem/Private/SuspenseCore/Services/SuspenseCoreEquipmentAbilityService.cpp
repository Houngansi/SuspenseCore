// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreEquipmentAbilityService.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbility.h"
#include "GameplayEffect.h"

USuspenseCoreEquipmentAbilityService::USuspenseCoreEquipmentAbilityService()
	: ServiceState(EServiceLifecycleState::Uninitialized)
	, bAutoActivateAbilities(false)
	, bRemoveEffectsOnUnequip(true)
	, bEnableDetailedLogging(false)
	, TotalAbilitiesGranted(0)
	, TotalAbilitiesRemoved(0)
	, TotalEffectsApplied(0)
	, TotalEffectsRemoved(0)
	, ActiveAbilityCount(0)
	, ActiveEffectCount(0)
{
}

USuspenseCoreEquipmentAbilityService::~USuspenseCoreEquipmentAbilityService()
{
}

//========================================
// ISuspenseEquipmentService Interface
//========================================

bool USuspenseCoreEquipmentAbilityService::InitializeService(const FServiceInitParams& Params)
{
	TRACK_SERVICE_INIT();

	ServiceState = EServiceLifecycleState::Initializing;
	ServiceLocator = Params.ServiceLocator;

	if (!InitializeGASIntegration())
	{
		LOG_SERVICE_ERROR(TEXT("Failed to initialize GAS integration"));
		ServiceState = EServiceLifecycleState::Failed;
		return false;
	}

	SetupEventSubscriptions();

	InitializationTime = FDateTime::UtcNow();
	ServiceState = EServiceLifecycleState::Ready;

	LOG_SERVICE_INFO(TEXT("Service initialized successfully"));
	return true;
}

bool USuspenseCoreEquipmentAbilityService::ShutdownService(bool bForce)
{
	TRACK_SERVICE_SHUTDOWN();

	ServiceState = EServiceLifecycleState::Shutting;
	CleanupGrantedAbilitiesAndEffects();
	ServiceState = EServiceLifecycleState::Shutdown;

	LOG_SERVICE_INFO(TEXT("Service shut down"));
	return true;
}

EServiceLifecycleState USuspenseCoreEquipmentAbilityService::GetServiceState() const
{
	return ServiceState;
}

bool USuspenseCoreEquipmentAbilityService::IsServiceReady() const
{
	return ServiceState == EServiceLifecycleState::Ready;
}

FGameplayTag USuspenseCoreEquipmentAbilityService::GetServiceTag() const
{
	static FGameplayTag ServiceTag = FGameplayTag::RequestGameplayTag(FName("Equipment.Service.Ability"));
	return ServiceTag;
}

FGameplayTagContainer USuspenseCoreEquipmentAbilityService::GetRequiredDependencies() const
{
	FGameplayTagContainer Dependencies;
	Dependencies.AddTag(FGameplayTag::RequestGameplayTag(FName("Equipment.Service.Data")));
	return Dependencies;
}

bool USuspenseCoreEquipmentAbilityService::ValidateService(TArray<FText>& OutErrors) const
{
	if (!AbilitySystemComponent.IsValid())
	{
		OutErrors.Add(FText::FromString(TEXT("AbilitySystemComponent is null")));
		return false;
	}

	return true;
}

void USuspenseCoreEquipmentAbilityService::ResetService()
{
	CleanupGrantedAbilitiesAndEffects();
	TotalAbilitiesGranted = 0;
	TotalAbilitiesRemoved = 0;
	TotalEffectsApplied = 0;
	TotalEffectsRemoved = 0;
	ActiveAbilityCount = 0;
	ActiveEffectCount = 0;
	LOG_SERVICE_INFO(TEXT("Service reset"));
}

FString USuspenseCoreEquipmentAbilityService::GetServiceStats() const
{
	return FString::Printf(TEXT("GAS - Abilities Granted: %d, Removed: %d, Active: %d | Effects Applied: %d, Removed: %d, Active: %d"),
		TotalAbilitiesGranted, TotalAbilitiesRemoved, ActiveAbilityCount,
		TotalEffectsApplied, TotalEffectsRemoved, ActiveEffectCount);
}

//========================================
// Ability Management
//========================================

TArray<FGameplayAbilitySpecHandle> USuspenseCoreEquipmentAbilityService::GrantEquipmentAbilities(int32 SlotIndex, const FSuspenseInventoryItemInstance& Item)
{
	CHECK_SERVICE_READY();

	TArray<FGameplayAbilitySpecHandle> GrantedHandles;

	if (!AbilitySystemComponent.IsValid())
	{
		LOG_SERVICE_ERROR(TEXT("AbilitySystemComponent is null"));
		return GrantedHandles;
	}

	TArray<TSubclassOf<UGameplayAbility>> AbilityClasses = GetAbilityClassesForItem(Item);

	for (TSubclassOf<UGameplayAbility> AbilityClass : AbilityClasses)
	{
		FGameplayAbilitySpecHandle Handle = GrantAbilityInternal(AbilityClass, 1);
		if (Handle.IsValid())
		{
			GrantedHandles.Add(Handle);
			TotalAbilitiesGranted++;
			ActiveAbilityCount++;
		}
	}

	GrantedAbilities.Add(SlotIndex, GrantedHandles);
	LOG_SERVICE_INFO(TEXT("Granted %d abilities for slot %d"), GrantedHandles.Num(), SlotIndex);

	return GrantedHandles;
}

bool USuspenseCoreEquipmentAbilityService::RemoveEquipmentAbilities(int32 SlotIndex)
{
	CHECK_SERVICE_READY_BOOL();

	TArray<FGameplayAbilitySpecHandle>* Handles = GrantedAbilities.Find(SlotIndex);
	if (!Handles)
	{
		return false;
	}

	for (const FGameplayAbilitySpecHandle& Handle : *Handles)
	{
		if (RemoveAbilityInternal(Handle))
		{
			TotalAbilitiesRemoved++;
			ActiveAbilityCount--;
		}
	}

	GrantedAbilities.Remove(SlotIndex);
	LOG_SERVICE_INFO(TEXT("Removed abilities for slot %d"), SlotIndex);

	return true;
}

TArray<FGameplayAbilitySpecHandle> USuspenseCoreEquipmentAbilityService::GetAbilitiesForSlot(int32 SlotIndex) const
{
	if (const TArray<FGameplayAbilitySpecHandle>* Handles = GrantedAbilities.Find(SlotIndex))
	{
		return *Handles;
	}

	return TArray<FGameplayAbilitySpecHandle>();
}

bool USuspenseCoreEquipmentAbilityService::IsAbilityFromEquipment(const FGameplayAbilitySpecHandle& AbilityHandle) const
{
	return AbilityToSlotMap.Contains(AbilityHandle);
}

bool USuspenseCoreEquipmentAbilityService::TryActivateEquipmentAbility(int32 SlotIndex, FGameplayTag AbilityTag)
{
	CHECK_SERVICE_READY_BOOL();

	if (!AbilitySystemComponent.IsValid())
	{
		return false;
	}

	// Try to activate ability by tag
	return AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(AbilityTag));
}

void USuspenseCoreEquipmentAbilityService::CancelEquipmentAbility(int32 SlotIndex, FGameplayTag AbilityTag)
{
	CHECK_SERVICE_READY();

	if (!AbilitySystemComponent.IsValid())
	{
		return;
	}

	AbilitySystemComponent->CancelAbilities(&FGameplayTagContainer(AbilityTag));
}

//========================================
// Gameplay Effect Management
//========================================

TArray<FActiveGameplayEffectHandle> USuspenseCoreEquipmentAbilityService::ApplyEquipmentEffects(int32 SlotIndex, const FSuspenseInventoryItemInstance& Item)
{
	CHECK_SERVICE_READY();

	TArray<FActiveGameplayEffectHandle> AppliedHandles;

	if (!AbilitySystemComponent.IsValid())
	{
		LOG_SERVICE_ERROR(TEXT("AbilitySystemComponent is null"));
		return AppliedHandles;
	}

	TArray<TSubclassOf<UGameplayEffect>> EffectClasses = GetEffectClassesForItem(Item);

	for (TSubclassOf<UGameplayEffect> EffectClass : EffectClasses)
	{
		FActiveGameplayEffectHandle Handle = ApplyEffectInternal(EffectClass, 1.0f);
		if (Handle.IsValid())
		{
			AppliedHandles.Add(Handle);
			TotalEffectsApplied++;
			ActiveEffectCount++;
		}
	}

	ActiveEffects.Add(SlotIndex, AppliedHandles);
	LOG_SERVICE_INFO(TEXT("Applied %d effects for slot %d"), AppliedHandles.Num(), SlotIndex);

	return AppliedHandles;
}

bool USuspenseCoreEquipmentAbilityService::RemoveEquipmentEffects(int32 SlotIndex)
{
	CHECK_SERVICE_READY_BOOL();

	TArray<FActiveGameplayEffectHandle>* Handles = ActiveEffects.Find(SlotIndex);
	if (!Handles)
	{
		return false;
	}

	for (const FActiveGameplayEffectHandle& Handle : *Handles)
	{
		if (RemoveEffectInternal(Handle))
		{
			TotalEffectsRemoved++;
			ActiveEffectCount--;
		}
	}

	ActiveEffects.Remove(SlotIndex);
	LOG_SERVICE_INFO(TEXT("Removed effects for slot %d"), SlotIndex);

	return true;
}

TArray<FActiveGameplayEffectHandle> USuspenseCoreEquipmentAbilityService::GetEffectsForSlot(int32 SlotIndex) const
{
	if (const TArray<FActiveGameplayEffectHandle>* Handles = ActiveEffects.Find(SlotIndex))
	{
		return *Handles;
	}

	return TArray<FActiveGameplayEffectHandle>();
}

bool USuspenseCoreEquipmentAbilityService::UpdateEffectMagnitude(FActiveGameplayEffectHandle EffectHandle, float NewMagnitude)
{
	CHECK_SERVICE_READY_BOOL();

	if (!AbilitySystemComponent.IsValid())
	{
		return false;
	}

	// Update effect magnitude logic
	return true;
}

float USuspenseCoreEquipmentAbilityService::GetEffectRemainingDuration(FActiveGameplayEffectHandle EffectHandle) const
{
	CHECK_SERVICE_READY();

	if (!AbilitySystemComponent.IsValid())
	{
		return 0.0f;
	}

	return AbilitySystemComponent->GetGameplayEffectDuration(EffectHandle);
}

//========================================
// Attribute Management
//========================================

bool USuspenseCoreEquipmentAbilityService::ApplyAttributeModifiers(int32 SlotIndex, const FSuspenseInventoryItemInstance& Item)
{
	CHECK_SERVICE_READY_BOOL();

	// Apply attribute modifiers through effects
	return ApplyEquipmentEffects(SlotIndex, Item).Num() > 0;
}

bool USuspenseCoreEquipmentAbilityService::RemoveAttributeModifiers(int32 SlotIndex)
{
	CHECK_SERVICE_READY_BOOL();

	return RemoveEquipmentEffects(SlotIndex);
}

float USuspenseCoreEquipmentAbilityService::GetAttributeValue(FGameplayAttribute Attribute) const
{
	CHECK_SERVICE_READY();

	if (!AbilitySystemComponent.IsValid())
	{
		return 0.0f;
	}

	return AbilitySystemComponent->GetNumericAttribute(Attribute);
}

float USuspenseCoreEquipmentAbilityService::CalculateTotalModifier(FGameplayAttribute Attribute) const
{
	CHECK_SERVICE_READY();

	// Calculate total modifier from all equipment
	return 0.0f;
}

//========================================
// GAS Component Access
//========================================

void USuspenseCoreEquipmentAbilityService::SetAbilitySystemComponent(UAbilitySystemComponent* ASC)
{
	AbilitySystemComponent = ASC;
	LOG_SERVICE_INFO(TEXT("AbilitySystemComponent set"));
}

UAbilitySystemComponent* USuspenseCoreEquipmentAbilityService::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

//========================================
// Event Publishing
//========================================

void USuspenseCoreEquipmentAbilityService::PublishAbilityGranted(int32 SlotIndex, FGameplayTag AbilityTag)
{
	PUBLISH_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Ability.Granted")),
		FSuspenseEquipmentEventData()
	);
}

void USuspenseCoreEquipmentAbilityService::PublishAbilityRemoved(int32 SlotIndex, FGameplayTag AbilityTag)
{
	PUBLISH_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Ability.Removed")),
		FSuspenseEquipmentEventData()
	);
}

void USuspenseCoreEquipmentAbilityService::PublishEffectApplied(int32 SlotIndex, FGameplayTag EffectTag)
{
	PUBLISH_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Effect.Applied")),
		FSuspenseEquipmentEventData()
	);
}

void USuspenseCoreEquipmentAbilityService::PublishEffectRemoved(int32 SlotIndex, FGameplayTag EffectTag)
{
	PUBLISH_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Effect.Removed")),
		FSuspenseEquipmentEventData()
	);
}

//========================================
// Service Lifecycle
//========================================

bool USuspenseCoreEquipmentAbilityService::InitializeGASIntegration()
{
	// GAS integration initialization
	return true;
}

void USuspenseCoreEquipmentAbilityService::SetupEventSubscriptions()
{
	// Subscribe to equipment events
	SUBSCRIBE_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Equipped")),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentAbilityService::OnEquipmentEquipped)
	);

	SUBSCRIBE_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Unequipped")),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentAbilityService::OnEquipmentUnequipped)
	);
}

void USuspenseCoreEquipmentAbilityService::CleanupGrantedAbilitiesAndEffects()
{
	for (auto& Pair : GrantedAbilities)
	{
		RemoveEquipmentAbilities(Pair.Key);
	}

	for (auto& Pair : ActiveEffects)
	{
		RemoveEquipmentEffects(Pair.Key);
	}

	GrantedAbilities.Empty();
	ActiveEffects.Empty();
	AbilityToSlotMap.Empty();

	LOG_SERVICE_INFO(TEXT("All granted abilities and effects cleaned up"));
}

//========================================
// Ability Operations
//========================================

FGameplayAbilitySpecHandle USuspenseCoreEquipmentAbilityService::GrantAbilityInternal(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level)
{
	if (!AbilitySystemComponent.IsValid() || !AbilityClass)
	{
		return FGameplayAbilitySpecHandle();
	}

	return AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, Level));
}

bool USuspenseCoreEquipmentAbilityService::RemoveAbilityInternal(const FGameplayAbilitySpecHandle& AbilityHandle)
{
	if (!AbilitySystemComponent.IsValid())
	{
		return false;
	}

	AbilitySystemComponent->ClearAbility(AbilityHandle);
	return true;
}

TArray<TSubclassOf<UGameplayAbility>> USuspenseCoreEquipmentAbilityService::GetAbilityClassesForItem(const FSuspenseInventoryItemInstance& Item) const
{
	// Get ability classes from item definition
	return TArray<TSubclassOf<UGameplayAbility>>();
}

//========================================
// Effect Operations
//========================================

FActiveGameplayEffectHandle USuspenseCoreEquipmentAbilityService::ApplyEffectInternal(TSubclassOf<UGameplayEffect> EffectClass, float Level)
{
	if (!AbilitySystemComponent.IsValid() || !EffectClass)
	{
		return FActiveGameplayEffectHandle();
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(EffectClass, Level, EffectContext);

	if (SpecHandle.IsValid())
	{
		return AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}

	return FActiveGameplayEffectHandle();
}

bool USuspenseCoreEquipmentAbilityService::RemoveEffectInternal(FActiveGameplayEffectHandle EffectHandle)
{
	if (!AbilitySystemComponent.IsValid())
	{
		return false;
	}

	return AbilitySystemComponent->RemoveActiveGameplayEffect(EffectHandle);
}

TArray<TSubclassOf<UGameplayEffect>> USuspenseCoreEquipmentAbilityService::GetEffectClassesForItem(const FSuspenseInventoryItemInstance& Item) const
{
	// Get effect classes from item definition
	return TArray<TSubclassOf<UGameplayEffect>>();
}

//========================================
// Event Handlers
//========================================

void USuspenseCoreEquipmentAbilityService::OnEquipmentEquipped(const FSuspenseEquipmentEventData& EventData)
{
	// Handle equipment equipped - grant abilities/effects
}

void USuspenseCoreEquipmentAbilityService::OnEquipmentUnequipped(const FSuspenseEquipmentEventData& EventData)
{
	// Handle equipment unequipped - remove abilities/effects
}

void USuspenseCoreEquipmentAbilityService::OnEquipmentModified(const FSuspenseEquipmentEventData& EventData)
{
	// Handle equipment modified - update abilities/effects
}

void USuspenseCoreEquipmentAbilityService::OnAbilityActivated(UGameplayAbility* Ability)
{
	// Handle ability activated
}

void USuspenseCoreEquipmentAbilityService::OnAbilityEnded(UGameplayAbility* Ability)
{
	// Handle ability ended
}
