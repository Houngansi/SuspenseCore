// SuspenseCoreCharacterClassSubsystem.cpp
// SuspenseCore - Character Class Management
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Subsystems/SuspenseCoreCharacterClassSubsystem.h"
#include "SuspenseCore/Data/SuspenseCoreCharacterClassData.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "SuspenseCore/Attributes/SuspenseCoreShieldAttributeSet.h"
#include "SuspenseCore/Attributes/SuspenseCoreMovementAttributeSet.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Subsystems/SuspenseCoreCharacterSelectionSubsystem.h"
#include "SuspenseCore/Settings/SuspenseCoreProjectSettings.h"
#include "Engine/AssetManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbility.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreClass, Log, All);

void USuspenseCoreCharacterClassSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const USuspenseCoreProjectSettings* Settings = USuspenseCoreProjectSettings::Get();
	if (Settings && Settings->bLogClassLoading)
	{
		UE_LOG(LogSuspenseCoreClass, Log, TEXT("CharacterClassSubsystem initializing..."));
		UE_LOG(LogSuspenseCoreClass, Log, TEXT("  Asset Path: %s"), *Settings->CharacterClassAssetPath.Path);
		UE_LOG(LogSuspenseCoreClass, Log, TEXT("  Asset Type: %s"), *Settings->CharacterClassAssetType);
	}

	// Load all class data assets
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
	ActorClassMap.Empty();

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
	const USuspenseCoreProjectSettings* Settings = USuspenseCoreProjectSettings::Get();
	const bool bVerbose = Settings ? Settings->bLogClassLoading : true;
	const FString AssetPath = Settings ? Settings->CharacterClassAssetPath.Path : TEXT("/Game/Blueprints/Core/Data");

	// First try Asset Manager (preferred for production)
	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (AssetManager)
	{
		TArray<FPrimaryAssetId> ClassAssetIds;
		AssetManager->GetPrimaryAssetIdList(CharacterClassAssetType, ClassAssetIds);

		if (ClassAssetIds.Num() > 0)
		{
			if (bVerbose)
			{
				UE_LOG(LogSuspenseCoreClass, Log, TEXT("Found %d CharacterClass assets via AssetManager"), ClassAssetIds.Num());
			}

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
				return;
			}
		}
	}

	// Fallback: Direct Asset Registry scan (works without AssetManager configuration)
	if (bVerbose)
	{
		UE_LOG(LogSuspenseCoreClass, Log, TEXT("Scanning Asset Registry for CharacterClass assets at: %s"), *AssetPath);
	}

	LoadClassesFromAssetRegistry(AssetPath);
}

void USuspenseCoreCharacterClassSubsystem::LoadClassesFromAssetRegistry(const FString& Path)
{
	const USuspenseCoreProjectSettings* Settings = USuspenseCoreProjectSettings::Get();
	const bool bVerbose = Settings ? Settings->bLogClassLoading : true;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Find all assets of type SuspenseCoreCharacterClassData in the specified path
	TArray<FAssetData> AssetDataList;

	FARFilter Filter;
	Filter.ClassPaths.Add(USuspenseCoreCharacterClassData::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(*Path));
	Filter.bRecursivePaths = true;

	AssetRegistry.GetAssets(Filter, AssetDataList);

	if (AssetDataList.Num() == 0)
	{
		UE_LOG(LogSuspenseCoreClass, Error, TEXT("═══════════════════════════════════════════════════════════════"));
		UE_LOG(LogSuspenseCoreClass, Error, TEXT("  NO CHARACTER CLASS DATA ASSETS FOUND!"));
		UE_LOG(LogSuspenseCoreClass, Error, TEXT("═══════════════════════════════════════════════════════════════"));
		UE_LOG(LogSuspenseCoreClass, Error, TEXT("  Searched path: %s"), *Path);
		UE_LOG(LogSuspenseCoreClass, Error, TEXT(""));
		UE_LOG(LogSuspenseCoreClass, Error, TEXT("  TO FIX:"));
		UE_LOG(LogSuspenseCoreClass, Error, TEXT("  1. Create Data Assets of type 'SuspenseCoreCharacterClassData'"));
		UE_LOG(LogSuspenseCoreClass, Error, TEXT("  2. Place them in: %s"), *Path);
		UE_LOG(LogSuspenseCoreClass, Error, TEXT("  3. OR configure path in Project Settings → Game → SuspenseCore"));
		UE_LOG(LogSuspenseCoreClass, Error, TEXT("═══════════════════════════════════════════════════════════════"));

		bClassesLoaded = true;
		PublishClassesLoadedEvent(0);
		return;
	}

	if (bVerbose)
	{
		UE_LOG(LogSuspenseCoreClass, Log, TEXT("Found %d CharacterClass assets in Asset Registry"), AssetDataList.Num());
	}

	// Load assets synchronously (they should be small Data Assets)
	for (const FAssetData& AssetData : AssetDataList)
	{
		USuspenseCoreCharacterClassData* ClassData = Cast<USuspenseCoreCharacterClassData>(AssetData.GetAsset());
		if (ClassData)
		{
			LoadedClasses.Add(ClassData->ClassID, ClassData);
			if (bVerbose)
			{
				UE_LOG(LogSuspenseCoreClass, Log, TEXT("  Loaded: %s (ID: %s)"), *ClassData->DisplayName.ToString(), *ClassData->ClassID.ToString());
			}
		}
	}

	bClassesLoaded = true;
	RegisterClassesWithSelectionSubsystem();

	UE_LOG(LogSuspenseCoreClass, Log, TEXT("CharacterClassSubsystem loaded %d classes"), LoadedClasses.Num());
	PublishClassesLoadedEvent(LoadedClasses.Num());
}

void USuspenseCoreCharacterClassSubsystem::OnClassesLoadComplete()
{
	const USuspenseCoreProjectSettings* Settings = USuspenseCoreProjectSettings::Get();
	const bool bVerbose = Settings ? Settings->bLogClassLoading : true;

	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		UE_LOG(LogSuspenseCoreClass, Error, TEXT("AssetManager not available in OnClassesLoadComplete!"));
		bClassesLoaded = true;
		PublishClassesLoadedEvent(0);
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
			if (bVerbose)
			{
				UE_LOG(LogSuspenseCoreClass, Log, TEXT("  Loaded: %s (ID: %s)"),
					*ClassData->DisplayName.ToString(), *ClassData->ClassID.ToString());
			}
		}
	}

	if (LoadedClasses.Num() == 0)
	{
		UE_LOG(LogSuspenseCoreClass, Error, TEXT("AssetManager async load completed but no classes were loaded!"));
	}

	bClassesLoaded = true;

	// Register all loaded classes with CharacterSelectionSubsystem for menu access
	RegisterClassesWithSelectionSubsystem();

	UE_LOG(LogSuspenseCoreClass, Log, TEXT("CharacterClassSubsystem loaded %d classes via AssetManager"), LoadedClasses.Num());

	// Publish event via EventBus
	PublishClassesLoadedEvent(LoadedClasses.Num());
}

TArray<USuspenseCoreCharacterClassData*> USuspenseCoreCharacterClassSubsystem::GetAllClasses() const
{
	TArray<USuspenseCoreCharacterClassData*> Result;
	for (const auto& Pair : LoadedClasses)
	{
		if (Pair.Value)
		{
			Result.Add(Pair.Value.Get());
		}
	}
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
	const TObjectPtr<USuspenseCoreCharacterClassData>* Found = LoadedClasses.Find(ClassId);
	return Found ? Found->Get() : nullptr;
}

bool USuspenseCoreCharacterClassSubsystem::ClassExists(FName ClassId) const
{
	return LoadedClasses.Contains(ClassId);
}

bool USuspenseCoreCharacterClassSubsystem::ApplyClassToActor(AActor* Actor, FName ClassId, int32 PlayerLevel)
{
	if (!Actor)
	{
		UE_LOG(LogSuspenseCoreClass, Warning, TEXT("ApplyClassToActor: Invalid Actor"));
		return false;
	}

	USuspenseCoreCharacterClassData* ClassData = GetClassById(ClassId);
	if (!ClassData)
	{
		UE_LOG(LogSuspenseCoreClass, Warning, TEXT("ApplyClassToActor: Class '%s' not found"), *ClassId.ToString());
		return false;
	}

	return ApplyClassDataToActor(Actor, ClassData, PlayerLevel);
}

bool USuspenseCoreCharacterClassSubsystem::ApplyClassDataToActor(AActor* Actor, USuspenseCoreCharacterClassData* ClassData, int32 PlayerLevel)
{
	if (!Actor || !ClassData)
	{
		return false;
	}

	// Get ASC via interface
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor);
	if (!ASI)
	{
		UE_LOG(LogSuspenseCoreClass, Warning, TEXT("ApplyClassDataToActor: Actor does not implement IAbilitySystemInterface"));
		return false;
	}

	UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
	if (!ASC)
	{
		UE_LOG(LogSuspenseCoreClass, Warning, TEXT("ApplyClassDataToActor: Actor has no ASC"));
		return false;
	}

	UE_LOG(LogSuspenseCoreClass, Log, TEXT("Applying class '%s' to actor %s"),
		*ClassData->DisplayName.ToString(), *Actor->GetName());

	// 1. Remove previous class if any
	RemoveClassFromActor(Actor);

	// 2. Apply attribute modifiers
	ApplyAttributeModifiers(ASC, ClassData);

	// 3. Grant class abilities
	GrantClassAbilities(ASC, ClassData, PlayerLevel);

	// 4. Apply passive effects
	ApplyPassiveEffects(ASC, ClassData);

	// 5. Store class reference
	ActorClassMap.Add(Actor, ClassData);

	// 6. Add class tag
	ASC->AddLooseGameplayTag(ClassData->ClassTag);

	// 7. Publish event via EventBus
	PublishClassChangeEvent(Actor, ClassData);

	UE_LOG(LogSuspenseCoreClass, Log, TEXT("Class '%s' applied successfully"), *ClassData->ClassID.ToString());

	return true;
}

bool USuspenseCoreCharacterClassSubsystem::ApplyClassToASC(UAbilitySystemComponent* ASC, FName ClassId, int32 PlayerLevel)
{
	if (!ASC)
	{
		UE_LOG(LogSuspenseCoreClass, Warning, TEXT("ApplyClassToASC: Invalid ASC"));
		return false;
	}

	USuspenseCoreCharacterClassData* ClassData = GetClassById(ClassId);
	if (!ClassData)
	{
		UE_LOG(LogSuspenseCoreClass, Warning, TEXT("ApplyClassToASC: Class '%s' not found"), *ClassId.ToString());
		return false;
	}

	UE_LOG(LogSuspenseCoreClass, Log, TEXT("Applying class '%s' directly to ASC"),
		*ClassData->DisplayName.ToString());

	// Apply attribute modifiers
	ApplyAttributeModifiers(ASC, ClassData);

	// Grant class abilities
	GrantClassAbilities(ASC, ClassData, PlayerLevel);

	// Apply passive effects
	ApplyPassiveEffects(ASC, ClassData);

	// Add class tag
	ASC->AddLooseGameplayTag(ClassData->ClassTag);

	UE_LOG(LogSuspenseCoreClass, Log, TEXT("Class '%s' applied to ASC successfully"), *ClassData->ClassID.ToString());

	return true;
}

void USuspenseCoreCharacterClassSubsystem::RemoveClassFromActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	TObjectPtr<USuspenseCoreCharacterClassData>* CurrentClassPtr = ActorClassMap.Find(Actor);
	if (!CurrentClassPtr || !*CurrentClassPtr)
	{
		return;
	}

	// Get ASC via interface
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor);
	if (!ASI)
	{
		ActorClassMap.Remove(Actor);
		return;
	}

	UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
	if (!ASC)
	{
		ActorClassMap.Remove(Actor);
		return;
	}

	USuspenseCoreCharacterClassData* OldClass = CurrentClassPtr->Get();

	UE_LOG(LogSuspenseCoreClass, Log, TEXT("Removing class '%s' from actor"), *OldClass->ClassID.ToString());

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
	ActorClassMap.Remove(Actor);
}

USuspenseCoreCharacterClassData* USuspenseCoreCharacterClassSubsystem::GetActorCurrentClass(AActor* Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}

	const TObjectPtr<USuspenseCoreCharacterClassData>* Found = ActorClassMap.Find(Actor);
	return Found ? Found->Get() : nullptr;
}

void USuspenseCoreCharacterClassSubsystem::ApplyAttributeModifiers(UAbilitySystemComponent* ASC, const USuspenseCoreCharacterClassData* ClassData)
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

void USuspenseCoreCharacterClassSubsystem::GrantClassAbilities(UAbilitySystemComponent* ASC, const USuspenseCoreCharacterClassData* ClassData, int32 PlayerLevel)
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

void USuspenseCoreCharacterClassSubsystem::ApplyPassiveEffects(UAbilitySystemComponent* ASC, const USuspenseCoreCharacterClassData* ClassData)
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

void USuspenseCoreCharacterClassSubsystem::PublishClassesLoadedEvent(int32 NumClasses)
{
	// Get EventBus from GameInstance context
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GI);
	if (!Manager)
	{
		return;
	}

	USuspenseCoreEventBus* EventBus = Manager->GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetInt(FName("NumClasses"), NumClasses);

	FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Class.Loaded"), false);
	if (EventTag.IsValid())
	{
		EventBus->Publish(EventTag, EventData);
	}
}

void USuspenseCoreCharacterClassSubsystem::PublishClassChangeEvent(AActor* Actor, USuspenseCoreCharacterClassData* ClassData)
{
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(Actor);
	if (!Manager)
	{
		return;
	}

	USuspenseCoreEventBus* EventBus = Manager->GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(Actor);
	EventData.SetString(FName("ClassID"), ClassData->ClassID.ToString());
	EventData.SetString(FName("ClassName"), ClassData->DisplayName.ToString());

	FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.ClassChanged"), false);
	if (EventTag.IsValid())
	{
		EventBus->Publish(EventTag, EventData);
	}
}

void USuspenseCoreCharacterClassSubsystem::RegisterClassesWithSelectionSubsystem()
{
	// Get CharacterSelectionSubsystem from GameInstance
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		UE_LOG(LogSuspenseCoreClass, Warning, TEXT("Cannot register classes - GameInstance not available"));
		return;
	}

	USuspenseCoreCharacterSelectionSubsystem* SelectionSubsystem = GI->GetSubsystem<USuspenseCoreCharacterSelectionSubsystem>();
	if (!SelectionSubsystem)
	{
		UE_LOG(LogSuspenseCoreClass, Warning, TEXT("Cannot register classes - CharacterSelectionSubsystem not available"));
		return;
	}

	// Register all loaded classes
	for (const auto& Pair : LoadedClasses)
	{
		if (Pair.Value)
		{
			SelectionSubsystem->RegisterClassData(Pair.Value, Pair.Key);
		}
	}

	UE_LOG(LogSuspenseCoreClass, Log, TEXT("Registered %d classes with CharacterSelectionSubsystem"), LoadedClasses.Num());
}
