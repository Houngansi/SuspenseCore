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
#include "Engine/AssetManager.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
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
		CreateDefaultClasses();
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

	// If no classes were loaded from assets, create defaults
	if (LoadedClasses.Num() == 0)
	{
		CreateDefaultClasses();
	}

	bClassesLoaded = true;

	UE_LOG(LogSuspenseCoreClass, Log, TEXT("CharacterClassSubsystem loaded %d classes"), LoadedClasses.Num());

	// Broadcast event
	OnClassesLoaded.Broadcast(LoadedClasses.Num());
}

USuspenseCoreCharacterClassData* USuspenseCoreCharacterClassSubsystem::CreateClassData(
	FName ClassId,
	const FText& DisplayName,
	const FText& Description,
	ESuspenseCoreClassRole Role,
	const FSuspenseCoreAttributeModifier& Modifiers,
	FLinearColor PrimaryColor)
{
	USuspenseCoreCharacterClassData* ClassData = NewObject<USuspenseCoreCharacterClassData>(this);
	ClassData->ClassID = ClassId;
	ClassData->DisplayName = DisplayName;
	ClassData->ShortDescription = Description;
	ClassData->Role = Role;
	ClassData->AttributeModifiers = Modifiers;
	ClassData->PrimaryColor = PrimaryColor;
	ClassData->bIsStarterClass = true;
	ClassData->ClassTag = FGameplayTag::RequestGameplayTag(FName(*FString::Printf(TEXT("SuspenseCore.Class.%s"), *ClassId.ToString())));

	return ClassData;
}

void USuspenseCoreCharacterClassSubsystem::CreateDefaultClasses()
{
	UE_LOG(LogSuspenseCoreClass, Log, TEXT("Creating default character classes..."));

	// ═══════════════════════════════════════════════════════════════════════════
	// ASSAULT - Balanced frontline damage dealer
	// ═══════════════════════════════════════════════════════════════════════════
	{
		FSuspenseCoreAttributeModifier AssaultMods;
		AssaultMods.MaxHealthMultiplier = 1.0f;
		AssaultMods.HealthRegenMultiplier = 1.0f;
		AssaultMods.MaxShieldMultiplier = 1.0f;
		AssaultMods.ShieldRegenMultiplier = 1.0f;
		AssaultMods.AttackPowerMultiplier = 1.15f;  // +15% damage
		AssaultMods.AccuracyMultiplier = 1.0f;
		AssaultMods.ReloadSpeedMultiplier = 1.1f;   // +10% reload
		AssaultMods.MaxStaminaMultiplier = 1.0f;
		AssaultMods.MovementSpeedMultiplier = 1.0f;
		AssaultMods.SprintSpeedMultiplier = 1.0f;

		USuspenseCoreCharacterClassData* Assault = CreateClassData(
			FName("Assault"),
			NSLOCTEXT("SuspenseCore", "Class_Assault", "Штурмовик"),
			NSLOCTEXT("SuspenseCore", "Class_Assault_Desc", "Сбалансированный боец передовой линии. Повышенный урон и скорость перезарядки."),
			ESuspenseCoreClassRole::Assault,
			AssaultMods,
			FLinearColor(0.9f, 0.4f, 0.1f, 1.0f)  // Orange
		);
		Assault->DifficultyRating = 1;
		Assault->DefaultPrimaryWeapon = FName("WPN_AssaultRifle");
		Assault->DefaultSecondaryWeapon = FName("WPN_Pistol");

		LoadedClasses.Add(Assault->ClassID, Assault);
		UE_LOG(LogSuspenseCoreClass, Log, TEXT("Created default class: Assault"));
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// MEDIC - Support healer with survivability
	// ═══════════════════════════════════════════════════════════════════════════
	{
		FSuspenseCoreAttributeModifier MedicMods;
		MedicMods.MaxHealthMultiplier = 0.9f;       // -10% health
		MedicMods.HealthRegenMultiplier = 1.5f;    // +50% health regen
		MedicMods.MaxShieldMultiplier = 1.1f;      // +10% shield
		MedicMods.ShieldRegenMultiplier = 1.3f;    // +30% shield regen
		MedicMods.ShieldRegenDelayMultiplier = 0.8f; // -20% delay (faster)
		MedicMods.AttackPowerMultiplier = 0.9f;    // -10% damage
		MedicMods.AccuracyMultiplier = 1.0f;
		MedicMods.ReloadSpeedMultiplier = 1.0f;
		MedicMods.MaxStaminaMultiplier = 1.1f;     // +10% stamina
		MedicMods.StaminaRegenMultiplier = 1.2f;   // +20% stamina regen
		MedicMods.MovementSpeedMultiplier = 1.05f; // +5% speed

		USuspenseCoreCharacterClassData* Medic = CreateClassData(
			FName("Medic"),
			NSLOCTEXT("SuspenseCore", "Class_Medic", "Медик"),
			NSLOCTEXT("SuspenseCore", "Class_Medic_Desc", "Поддержка команды. Быстрая регенерация здоровья и щита, повышенная мобильность."),
			ESuspenseCoreClassRole::Support,
			MedicMods,
			FLinearColor(0.2f, 0.8f, 0.3f, 1.0f)  // Green
		);
		Medic->DifficultyRating = 2;
		Medic->DefaultPrimaryWeapon = FName("WPN_SMG");
		Medic->DefaultSecondaryWeapon = FName("WPN_Pistol");
		Medic->DefaultEquipment.Add(FName("EQP_MedKit"));

		LoadedClasses.Add(Medic->ClassID, Medic);
		UE_LOG(LogSuspenseCoreClass, Log, TEXT("Created default class: Medic"));
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// SNIPER - Long-range precision with high damage
	// ═══════════════════════════════════════════════════════════════════════════
	{
		FSuspenseCoreAttributeModifier SniperMods;
		SniperMods.MaxHealthMultiplier = 0.85f;     // -15% health
		SniperMods.HealthRegenMultiplier = 0.9f;   // -10% health regen
		SniperMods.MaxShieldMultiplier = 0.9f;     // -10% shield
		SniperMods.ShieldRegenMultiplier = 1.0f;
		SniperMods.AttackPowerMultiplier = 1.3f;   // +30% damage
		SniperMods.AccuracyMultiplier = 1.25f;     // +25% accuracy
		SniperMods.ReloadSpeedMultiplier = 0.9f;   // -10% reload (slower)
		SniperMods.MaxStaminaMultiplier = 0.9f;    // -10% stamina
		SniperMods.StaminaRegenMultiplier = 1.0f;
		SniperMods.MovementSpeedMultiplier = 0.95f; // -5% speed

		USuspenseCoreCharacterClassData* Sniper = CreateClassData(
			FName("Sniper"),
			NSLOCTEXT("SuspenseCore", "Class_Sniper", "Снайпер"),
			NSLOCTEXT("SuspenseCore", "Class_Sniper_Desc", "Дальнобойный стрелок. Высокий урон и точность, но низкая выживаемость."),
			ESuspenseCoreClassRole::Recon,
			SniperMods,
			FLinearColor(0.3f, 0.5f, 0.9f, 1.0f)  // Blue
		);
		Sniper->DifficultyRating = 3;
		Sniper->DefaultPrimaryWeapon = FName("WPN_SniperRifle");
		Sniper->DefaultSecondaryWeapon = FName("WPN_Pistol");

		LoadedClasses.Add(Sniper->ClassID, Sniper);
		UE_LOG(LogSuspenseCoreClass, Log, TEXT("Created default class: Sniper"));
	}

	UE_LOG(LogSuspenseCoreClass, Log, TEXT("Created %d default character classes"), LoadedClasses.Num());
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

	// 7. Broadcast events
	OnClassApplied.Broadcast(Actor, ClassData);
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

	FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.ClassChanged"));
	EventBus->Publish(EventTag, EventData);
}
