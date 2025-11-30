// SuspenseCoreCharacterClassSubsystem.cpp
// SuspenseCore - Character Class Management
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Subsystems/SuspenseCoreCharacterClassSubsystem.h"
#include "SuspenseCore/Data/SuspenseCoreCharacterClassData.h"
#include "SuspenseCore/Components/SuspenseCoreAbilitySystemComponent.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "SuspenseCore/Attributes/SuspenseCoreShieldAttributeSet.h"
#include "SuspenseCore/Attributes/SuspenseCoreMovementAttributeSet.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "PlayerCore/Public/SuspenseCore/Core/SuspenseCorePlayerState.h"
#include "Engine/AssetManager.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbility.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreClass, Log, All);

void USuspenseCoreCharacterClassSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogSuspenseCoreClass, Log, TEXT("CharacterClassSubsystem initializing..."));

	// Load all class data assets asynchronously
	LoadAllClasses();
}

void USuspenseCoreCharacterClassSubsystem::Deinitialize()
{
	// Cancel any pending loads
	if (ClassLoadHandle.IsValid())
	{
		ClassLoadHandle->CancelHandle();
		ClassLoadHandle.Reset();
	}

	LoadedClasses.Empty();
	PlayerClassMap.Empty();

	Super::Deinitialize();
}

USuspenseCoreCharacterClassSubsystem* USuspenseCoreCharacterClassSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	return GameInstance->GetSubsystem<USuspenseCoreCharacterClassSubsystem>();
}

void USuspenseCoreCharacterClassSubsystem::LoadAllClasses()
{
	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		UE_LOG(LogSuspenseCoreClass, Warning, TEXT("AssetManager not available, using synchronous load"));

		// Fallback: Try to find classes in content
		// Classes should be at /Game/Data/CharacterClasses/
		return;
	}

	// Get all primary asset IDs of type CharacterClass
	TArray<FPrimaryAssetId> ClassAssetIds;
	AssetManager->GetPrimaryAssetIdList(CharacterClassAssetType, ClassAssetIds);

	if (ClassAssetIds.Num() == 0)
	{
		UE_LOG(LogSuspenseCoreClass, Warning, TEXT("No CharacterClass assets found. Make sure to configure AssetManager."));
		bClassesLoaded = true;
		return;
	}

	UE_LOG(LogSuspenseCoreClass, Log, TEXT("Found %d CharacterClass assets to load"), ClassAssetIds.Num());

	// Async load all classes
	TArray<FSoftObjectPath> AssetPaths;
	for (const FPrimaryAssetId& AssetId : ClassAssetIds)
	{
		FSoftObjectPath Path = AssetManager->GetPrimaryAssetPath(AssetId);
		if (Path.IsValid())
		{
			AssetPaths.Add(Path);
		}
	}

	if (AssetPaths.Num() > 0)
	{
		ClassLoadHandle = StreamableManager.RequestAsyncLoad(
			AssetPaths,
			FStreamableDelegate::CreateUObject(this, &USuspenseCoreCharacterClassSubsystem::OnClassesLoadComplete)
		);
	}
	else
	{
		bClassesLoaded = true;
	}
}

void USuspenseCoreCharacterClassSubsystem::OnClassesLoadComplete()
{
	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		bClassesLoaded = true;
		return;
	}

	TArray<FPrimaryAssetId> ClassAssetIds;
	AssetManager->GetPrimaryAssetIdList(CharacterClassAssetType, ClassAssetIds);

	for (const FPrimaryAssetId& AssetId : ClassAssetIds)
	{
		UObject* LoadedAsset = AssetManager->GetPrimaryAssetObject(AssetId);
		USuspenseCoreCharacterClassData* ClassData = Cast<USuspenseCoreCharacterClassData>(LoadedAsset);

		if (ClassData)
		{
			LoadedClasses.Add(ClassData->ClassID, ClassData);
			UE_LOG(LogSuspenseCoreClass, Log, TEXT("Loaded class: %s (%s)"),
				*ClassData->DisplayName.ToString(), *ClassData->ClassID.ToString());
		}
	}

	bClassesLoaded = true;

	UE_LOG(LogSuspenseCoreClass, Log, TEXT("CharacterClassSubsystem loaded %d classes"), LoadedClasses.Num());

	// Broadcast event
	OnClassesLoaded.Broadcast(LoadedClasses.Num());
}

TArray<USuspenseCoreCharacterClassData*> USuspenseCoreCharacterClassSubsystem::GetAllClasses() const
{
	TArray<USuspenseCoreCharacterClassData*> Result;
	LoadedClasses.GenerateValueArray(Result);
	return Result;
}

TArray<USuspenseCoreCharacterClassData*> USuspenseCoreCharacterClassSubsystem::GetUnlockedClasses(int32 PlayerLevel) const
{
	TArray<USuspenseCoreCharacterClassData*> Result;

	for (const auto& Pair : LoadedClasses)
	{
		if (Pair.Value && Pair.Value->IsUnlockedForLevel(PlayerLevel))
		{
			Result.Add(Pair.Value);
		}
	}

	return Result;
}

TArray<USuspenseCoreCharacterClassData*> USuspenseCoreCharacterClassSubsystem::GetStarterClasses() const
{
	TArray<USuspenseCoreCharacterClassData*> Result;

	for (const auto& Pair : LoadedClasses)
	{
		if (Pair.Value && Pair.Value->bIsStarterClass)
		{
			Result.Add(Pair.Value);
		}
	}

	return Result;
}

USuspenseCoreCharacterClassData* USuspenseCoreCharacterClassSubsystem::GetClassById(FName ClassId) const
{
	const USuspenseCoreCharacterClassData* const* Found = LoadedClasses.Find(ClassId);
	return Found ? const_cast<USuspenseCoreCharacterClassData*>(*Found) : nullptr;
}

bool USuspenseCoreCharacterClassSubsystem::ClassExists(FName ClassId) const
{
	return LoadedClasses.Contains(ClassId);
}

bool USuspenseCoreCharacterClassSubsystem::ApplyClassToPlayer(ASuspenseCorePlayerState* PlayerState, FName ClassId)
{
	if (!PlayerState)
	{
		UE_LOG(LogSuspenseCoreClass, Warning, TEXT("ApplyClassToPlayer: Invalid PlayerState"));
		return false;
	}

	USuspenseCoreCharacterClassData* ClassData = GetClassById(ClassId);
	if (!ClassData)
	{
		UE_LOG(LogSuspenseCoreClass, Warning, TEXT("ApplyClassToPlayer: Class '%s' not found"), *ClassId.ToString());
		return false;
	}

	return ApplyClassDataToPlayer(PlayerState, ClassData);
}

bool USuspenseCoreCharacterClassSubsystem::ApplyClassDataToPlayer(ASuspenseCorePlayerState* PlayerState, USuspenseCoreCharacterClassData* ClassData)
{
	if (!PlayerState || !ClassData)
	{
		return false;
	}

	USuspenseCoreAbilitySystemComponent* ASC = PlayerState->GetSuspenseCoreASC();
	if (!ASC)
	{
		UE_LOG(LogSuspenseCoreClass, Warning, TEXT("ApplyClassDataToPlayer: Player has no ASC"));
		return false;
	}

	UE_LOG(LogSuspenseCoreClass, Log, TEXT("Applying class '%s' to player %s"),
		*ClassData->DisplayName.ToString(), *PlayerState->GetPlayerName());

	// 1. Remove previous class if any
	RemoveClassFromPlayer(PlayerState);

	// 2. Apply attribute modifiers
	ApplyAttributeModifiers(ASC, ClassData);

	// 3. Grant class abilities
	int32 PlayerLevel = PlayerState->GetPlayerLevel();
	GrantClassAbilities(ASC, ClassData, PlayerLevel);

	// 4. Apply passive effects
	ApplyPassiveEffects(ASC, ClassData);

	// 5. Store class reference
	PlayerClassMap.Add(PlayerState, ClassData);

	// 6. Add class tag
	ASC->AddLooseGameplayTag(ClassData->ClassTag);

	// 7. Broadcast events
	OnClassApplied.Broadcast(PlayerState, ClassData);
	PublishClassChangeEvent(PlayerState, ClassData);

	UE_LOG(LogSuspenseCoreClass, Log, TEXT("Class '%s' applied successfully"), *ClassData->ClassID.ToString());

	return true;
}

void USuspenseCoreCharacterClassSubsystem::RemoveClassFromPlayer(ASuspenseCorePlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}

	USuspenseCoreCharacterClassData** CurrentClass = PlayerClassMap.Find(PlayerState);
	if (!CurrentClass || !*CurrentClass)
	{
		return;
	}

	USuspenseCoreAbilitySystemComponent* ASC = PlayerState->GetSuspenseCoreASC();
	if (!ASC)
	{
		return;
	}

	USuspenseCoreCharacterClassData* OldClass = *CurrentClass;

	UE_LOG(LogSuspenseCoreClass, Log, TEXT("Removing class '%s' from player"), *OldClass->ClassID.ToString());

	// Remove class tag
	ASC->RemoveLooseGameplayTag(OldClass->ClassTag);

	// Remove class abilities
	for (const FSuspenseCoreClassAbilitySlot& Slot : OldClass->ClassAbilities)
	{
		if (Slot.AbilityClass)
		{
			FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(Slot.AbilityClass);
			if (Spec)
			{
				ASC->ClearAbility(Spec->Handle);
			}
		}
	}

	// Note: Passive effects should be handled by removing effects with class tag

	// Clear mapping
	PlayerClassMap.Remove(PlayerState);
}

USuspenseCoreCharacterClassData* USuspenseCoreCharacterClassSubsystem::GetPlayerCurrentClass(ASuspenseCorePlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		return nullptr;
	}

	USuspenseCoreCharacterClassData* const* Found = PlayerClassMap.Find(PlayerState);
	return Found ? *Found : nullptr;
}

void USuspenseCoreCharacterClassSubsystem::ApplyAttributeModifiers(USuspenseCoreAbilitySystemComponent* ASC, const USuspenseCoreCharacterClassData* ClassData)
{
	if (!ASC || !ClassData)
	{
		return;
	}

	const FSuspenseCoreAttributeModifier& Mods = ClassData->AttributeModifiers;

	// Get attribute sets
	const USuspenseCoreAttributeSet* CoreAttribs = ASC->GetSet<USuspenseCoreAttributeSet>();
	const USuspenseCoreShieldAttributeSet* ShieldAttribs = ASC->GetSet<USuspenseCoreShieldAttributeSet>();
	const USuspenseCoreMovementAttributeSet* MovementAttribs = ASC->GetSet<USuspenseCoreMovementAttributeSet>();

	// Apply core attribute modifiers (using base values * multipliers)
	if (CoreAttribs)
	{
		// Base values
		const float BaseMaxHealth = 100.0f;
		const float BaseHealthRegen = 1.0f;
		const float BaseMaxStamina = 100.0f;
		const float BaseStaminaRegen = 10.0f;
		const float BaseAttackPower = 1.0f;

		// Set modified values
		ASC->SetNumericAttributeBase(USuspenseCoreAttributeSet::GetMaxHealthAttribute(), BaseMaxHealth * Mods.MaxHealthMultiplier);
		ASC->SetNumericAttributeBase(USuspenseCoreAttributeSet::GetHealthAttribute(), BaseMaxHealth * Mods.MaxHealthMultiplier);
		ASC->SetNumericAttributeBase(USuspenseCoreAttributeSet::GetHealthRegenAttribute(), BaseHealthRegen * Mods.HealthRegenMultiplier);
		ASC->SetNumericAttributeBase(USuspenseCoreAttributeSet::GetMaxStaminaAttribute(), BaseMaxStamina * Mods.MaxStaminaMultiplier);
		ASC->SetNumericAttributeBase(USuspenseCoreAttributeSet::GetStaminaAttribute(), BaseMaxStamina * Mods.MaxStaminaMultiplier);
		ASC->SetNumericAttributeBase(USuspenseCoreAttributeSet::GetStaminaRegenAttribute(), BaseStaminaRegen * Mods.StaminaRegenMultiplier);
		ASC->SetNumericAttributeBase(USuspenseCoreAttributeSet::GetAttackPowerAttribute(), BaseAttackPower * Mods.AttackPowerMultiplier);
		ASC->SetNumericAttributeBase(USuspenseCoreAttributeSet::GetArmorAttribute(), Mods.ArmorBonus);
		ASC->SetNumericAttributeBase(USuspenseCoreAttributeSet::GetMovementSpeedAttribute(), Mods.MovementSpeedMultiplier);
	}

	// Apply shield attribute modifiers
	if (ShieldAttribs)
	{
		const float BaseMaxShield = 100.0f;
		const float BaseShieldRegen = 10.0f;
		const float BaseShieldRegenDelay = 3.0f;

		ASC->SetNumericAttributeBase(USuspenseCoreShieldAttributeSet::GetMaxShieldAttribute(), BaseMaxShield * Mods.MaxShieldMultiplier);
		ASC->SetNumericAttributeBase(USuspenseCoreShieldAttributeSet::GetShieldRegenAttribute(), BaseShieldRegen * Mods.ShieldRegenMultiplier);
		ASC->SetNumericAttributeBase(USuspenseCoreShieldAttributeSet::GetShieldRegenDelayAttribute(), BaseShieldRegenDelay * Mods.ShieldRegenDelayMultiplier);
	}

	// Apply movement attribute modifiers
	if (MovementAttribs)
	{
		const float BaseWalkSpeed = 400.0f;
		const float BaseSprintSpeed = 600.0f;
		const float BaseJumpHeight = 420.0f;

		ASC->SetNumericAttributeBase(USuspenseCoreMovementAttributeSet::GetWalkSpeedAttribute(), BaseWalkSpeed * Mods.MovementSpeedMultiplier);
		ASC->SetNumericAttributeBase(USuspenseCoreMovementAttributeSet::GetSprintSpeedAttribute(), BaseSprintSpeed * Mods.SprintSpeedMultiplier);
		ASC->SetNumericAttributeBase(USuspenseCoreMovementAttributeSet::GetJumpHeightAttribute(), BaseJumpHeight * Mods.JumpHeightMultiplier);
	}

	UE_LOG(LogSuspenseCoreClass, Log, TEXT("Applied attribute modifiers: Health=%.0f, Stamina=%.0f, Attack=%.2f"),
		CoreAttribs ? CoreAttribs->GetMaxHealth() : 0.0f,
		CoreAttribs ? CoreAttribs->GetMaxStamina() : 0.0f,
		CoreAttribs ? CoreAttribs->GetAttackPower() : 0.0f);
}

void USuspenseCoreCharacterClassSubsystem::GrantClassAbilities(USuspenseCoreAbilitySystemComponent* ASC, const USuspenseCoreCharacterClassData* ClassData, int32 PlayerLevel)
{
	if (!ASC || !ClassData)
	{
		return;
	}

	TArray<TSubclassOf<UGameplayAbility>> AbilitiesToGrant = ClassData->GetAbilitiesForLevel(PlayerLevel);

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : AbilitiesToGrant)
	{
		if (AbilityClass)
		{
			// Check if already has this ability
			if (ASC->FindAbilitySpecFromClass(AbilityClass))
			{
				continue;
			}

			FGameplayAbilitySpec AbilitySpec(AbilityClass, 1, INDEX_NONE, ASC->GetOwner());
			ASC->GiveAbility(AbilitySpec);

			UE_LOG(LogSuspenseCoreClass, Log, TEXT("Granted ability: %s"), *AbilityClass->GetName());
		}
	}
}

void USuspenseCoreCharacterClassSubsystem::ApplyPassiveEffects(USuspenseCoreAbilitySystemComponent* ASC, const USuspenseCoreCharacterClassData* ClassData)
{
	if (!ASC || !ClassData)
	{
		return;
	}

	for (const TSubclassOf<UGameplayEffect>& EffectClass : ClassData->PassiveEffects)
	{
		if (EffectClass)
		{
			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddSourceObject(ASC->GetOwner());

			FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, 1.0f, Context);
			if (SpecHandle.IsValid())
			{
				ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
				UE_LOG(LogSuspenseCoreClass, Log, TEXT("Applied passive effect: %s"), *EffectClass->GetName());
			}
		}
	}
}

void USuspenseCoreCharacterClassSubsystem::PublishClassChangeEvent(ASuspenseCorePlayerState* PlayerState, USuspenseCoreCharacterClassData* ClassData)
{
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(PlayerState);
	if (!Manager)
	{
		return;
	}

	USuspenseCoreEventBus* EventBus = Manager->GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(PlayerState);
	EventData.SetString(FName("ClassID"), ClassData->ClassID.ToString());
	EventData.SetString(FName("ClassName"), ClassData->DisplayName.ToString());

	FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(FName("Event.Player.ClassChanged"));
	EventBus->Publish(EventTag, EventData);
}
