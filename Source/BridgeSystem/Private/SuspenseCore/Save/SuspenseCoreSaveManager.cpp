// SuspenseCoreSaveManager.cpp
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Save/SuspenseCoreSaveManager.h"
#include "SuspenseCore/Save/SuspenseCoreFileSaveRepository.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AttributeSet.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreSaveManager, Log, All);

// ═══════════════════════════════════════════════════════════════════════════════
// LIFECYCLE
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreSaveManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogSuspenseCoreSaveManager, Log, TEXT("SaveManager initializing..."));

	// Create file repository
	SaveRepository = NewObject<USuspenseCoreFileSaveRepository>(this, TEXT("SaveRepository"));
	SaveRepository->Initialize(TEXT(""));

	// Record session start
	SessionStartTime = FPlatformTime::Seconds();

	UE_LOG(LogSuspenseCoreSaveManager, Log, TEXT("SaveManager initialized"));
}

void USuspenseCoreSaveManager::Deinitialize()
{
	UE_LOG(LogSuspenseCoreSaveManager, Log, TEXT("SaveManager deinitializing..."));

	// Stop auto-save
	StopAutoSaveTimer();

	Super::Deinitialize();
}

USuspenseCoreSaveManager* USuspenseCoreSaveManager::Get(const UObject* WorldContextObject)
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

	return GameInstance->GetSubsystem<USuspenseCoreSaveManager>();
}

// ═══════════════════════════════════════════════════════════════════════════════
// PLAYER MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreSaveManager::SetCurrentPlayer(const FString& PlayerId)
{
	CurrentPlayerId = PlayerId;
	UE_LOG(LogSuspenseCoreSaveManager, Log, TEXT("Current player set: %s"), *PlayerId);

	// Setup auto-save when player is set
	if (bAutoSaveEnabled && !CurrentPlayerId.IsEmpty())
	{
		SetupAutoSaveTimer();
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// QUICK SAVE/LOAD
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreSaveManager::QuickSave()
{
	if (!HasCurrentPlayer())
	{
		UE_LOG(LogSuspenseCoreSaveManager, Warning, TEXT("QuickSave: No current player set"));
		return;
	}

	SaveToSlotInternal(USuspenseCoreFileSaveRepository::QUICKSAVE_SLOT, TEXT("Quick Save"), false);
}

void USuspenseCoreSaveManager::QuickLoad()
{
	if (!HasCurrentPlayer())
	{
		UE_LOG(LogSuspenseCoreSaveManager, Warning, TEXT("QuickLoad: No current player set"));
		return;
	}

	LoadFromSlotInternal(USuspenseCoreFileSaveRepository::QUICKSAVE_SLOT);
}

bool USuspenseCoreSaveManager::HasQuickSave() const
{
	if (!SaveRepository || CurrentPlayerId.IsEmpty())
	{
		return false;
	}

	return SaveRepository->SlotExists(CurrentPlayerId, USuspenseCoreFileSaveRepository::QUICKSAVE_SLOT);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SLOT MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreSaveManager::SaveToSlot(int32 SlotIndex, const FString& SlotName)
{
	if (!HasCurrentPlayer())
	{
		UE_LOG(LogSuspenseCoreSaveManager, Warning, TEXT("SaveToSlot: No current player set"));
		OnSaveCompleted.Broadcast(false, TEXT("No current player"));
		return;
	}

	SaveToSlotInternal(SlotIndex, SlotName, false);
}

void USuspenseCoreSaveManager::LoadFromSlot(int32 SlotIndex)
{
	if (!HasCurrentPlayer())
	{
		UE_LOG(LogSuspenseCoreSaveManager, Warning, TEXT("LoadFromSlot: No current player set"));
		OnLoadCompleted.Broadcast(false, TEXT("No current player"));
		return;
	}

	LoadFromSlotInternal(SlotIndex);
}

void USuspenseCoreSaveManager::DeleteSlot(int32 SlotIndex)
{
	if (!SaveRepository || CurrentPlayerId.IsEmpty())
	{
		return;
	}

	ESuspenseCoreSaveResult Result = SaveRepository->DeleteSlot(CurrentPlayerId, SlotIndex);

	if (Result == ESuspenseCoreSaveResult::Success)
	{
		UE_LOG(LogSuspenseCoreSaveManager, Log, TEXT("Deleted slot %d"), SlotIndex);
	}
	else
	{
		UE_LOG(LogSuspenseCoreSaveManager, Warning, TEXT("Failed to delete slot %d"), SlotIndex);
	}
}

TArray<FSuspenseCoreSaveHeader> USuspenseCoreSaveManager::GetAllSlotHeaders()
{
	TArray<FSuspenseCoreSaveHeader> Headers;

	if (!SaveRepository || CurrentPlayerId.IsEmpty())
	{
		return Headers;
	}

	SaveRepository->GetSaveHeaders(CurrentPlayerId, Headers);
	return Headers;
}

bool USuspenseCoreSaveManager::SlotExists(int32 SlotIndex) const
{
	if (!SaveRepository || CurrentPlayerId.IsEmpty())
	{
		return false;
	}

	return SaveRepository->SlotExists(CurrentPlayerId, SlotIndex);
}

int32 USuspenseCoreSaveManager::GetMaxSlots() const
{
	if (!SaveRepository)
	{
		return 0;
	}

	return SaveRepository->GetMaxSlots();
}

// ═══════════════════════════════════════════════════════════════════════════════
// AUTO-SAVE
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreSaveManager::SetAutoSaveEnabled(bool bEnabled)
{
	bAutoSaveEnabled = bEnabled;

	if (bEnabled && HasCurrentPlayer())
	{
		SetupAutoSaveTimer();
	}
	else
	{
		StopAutoSaveTimer();
	}

	UE_LOG(LogSuspenseCoreSaveManager, Log, TEXT("Auto-save %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
}

void USuspenseCoreSaveManager::SetAutoSaveInterval(float IntervalSeconds)
{
	AutoSaveInterval = FMath::Max(30.0f, IntervalSeconds); // Minimum 30 seconds

	// Restart timer with new interval
	if (bAutoSaveEnabled && HasCurrentPlayer())
	{
		SetupAutoSaveTimer();
	}

	UE_LOG(LogSuspenseCoreSaveManager, Log, TEXT("Auto-save interval set to %.0f seconds"), AutoSaveInterval);
}

void USuspenseCoreSaveManager::TriggerAutoSave()
{
	if (!HasCurrentPlayer())
	{
		return;
	}

	SaveToSlotInternal(USuspenseCoreFileSaveRepository::AUTOSAVE_SLOT, TEXT("Auto-Save"), true);
}

bool USuspenseCoreSaveManager::HasAutoSave() const
{
	if (!SaveRepository || CurrentPlayerId.IsEmpty())
	{
		return false;
	}

	return SaveRepository->SlotExists(CurrentPlayerId, USuspenseCoreFileSaveRepository::AUTOSAVE_SLOT);
}

// ═══════════════════════════════════════════════════════════════════════════════
// STATE COLLECTION
// ═══════════════════════════════════════════════════════════════════════════════

FSuspenseCoreSaveData USuspenseCoreSaveManager::CollectCurrentGameState()
{
	FSuspenseCoreSaveData SaveData = FSuspenseCoreSaveData::CreateEmpty();

	// Profile data
	SaveData.ProfileData = CachedProfileData;

	// Character state
	SaveData.CharacterState = CollectCharacterState();

	// Header
	SaveData.Header.CharacterName = CachedProfileData.DisplayName;
	SaveData.Header.CharacterLevel = CachedProfileData.Level;
	SaveData.Header.LocationName = GetCurrentMapName().ToString();
	SaveData.Header.TotalPlayTimeSeconds = GetTotalPlayTime();
	SaveData.Header.SaveTimestamp = FDateTime::UtcNow();

	return SaveData;
}

void USuspenseCoreSaveManager::ApplyLoadedState(const FSuspenseCoreSaveData& SaveData)
{
	// Update cached profile
	CachedProfileData = SaveData.ProfileData;

	// Apply character state
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		return;
	}

	// Set position
	Pawn->SetActorLocationAndRotation(
		SaveData.CharacterState.WorldPosition,
		SaveData.CharacterState.WorldRotation
	);

	// Apply GAS attributes (ASC is on PlayerState)
	APlayerState* PlayerState = PC->GetPlayerState<APlayerState>();
	if (PlayerState)
	{
		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(PlayerState))
		{
			if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
			{
				// Apply attributes via reflection - works without direct AttributeSet include
				TArray<UAttributeSet*> AttributeSets;
				ASC->GetAllAttributes(AttributeSets);

				for (UAttributeSet* AttrSet : AttributeSets)
				{
					if (!AttrSet) continue;

					for (TFieldIterator<FNumericProperty> PropIt(AttrSet->GetClass()); PropIt; ++PropIt)
					{
						FNumericProperty* Property = *PropIt;
						FString PropName = Property->GetName();

						float ValueToSet = 0.0f;
						bool bShouldSet = false;

						if (PropName == TEXT("Health")) { ValueToSet = SaveData.CharacterState.CurrentHealth; bShouldSet = true; }
						else if (PropName == TEXT("MaxHealth")) { ValueToSet = SaveData.CharacterState.MaxHealth; bShouldSet = true; }
						else if (PropName == TEXT("Stamina")) { ValueToSet = SaveData.CharacterState.CurrentStamina; bShouldSet = true; }
						else if (PropName == TEXT("MaxStamina")) { ValueToSet = SaveData.CharacterState.MaxStamina; bShouldSet = true; }
						else if (PropName == TEXT("Armor")) { ValueToSet = SaveData.CharacterState.CurrentArmor; bShouldSet = true; }
						else if (PropName == TEXT("Shield")) { ValueToSet = SaveData.CharacterState.CurrentShield; bShouldSet = true; }

						if (bShouldSet)
						{
							// Create FGameplayAttribute and set via ASC
							FGameplayAttribute Attribute(Property);
							ASC->SetNumericAttributeBase(Attribute, ValueToSet);
						}
					}
				}

				// Re-apply saved active effects
				for (const FSuspenseCoreActiveEffect& SavedEffect : SaveData.CharacterState.ActiveEffects)
				{
					if (!SavedEffect.EffectId.IsEmpty())
					{
						UClass* EffectClass = LoadClass<UGameplayEffect>(nullptr, *SavedEffect.EffectId);
						if (EffectClass)
						{
							FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(
								EffectClass, SavedEffect.Level, ASC->MakeEffectContext());
							if (SpecHandle.IsValid())
							{
								ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
							}
						}
					}
				}

				UE_LOG(LogSuspenseCoreSaveManager, Log, TEXT("Applied GAS state - Health: %.1f/%.1f, Effects restored: %d"),
					SaveData.CharacterState.CurrentHealth, SaveData.CharacterState.MaxHealth, SaveData.CharacterState.ActiveEffects.Num());
			}
		}
	}

	// Apply movement state
	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (SaveData.CharacterState.bIsCrouching)
		{
			Character->Crouch();
		}
	}

	UE_LOG(LogSuspenseCoreSaveManager, Log, TEXT("Applied loaded state for %s at %s"),
		*SaveData.ProfileData.DisplayName, *SaveData.CharacterState.CurrentMapName.ToString());
}

void USuspenseCoreSaveManager::SetProfileData(const FSuspenseCorePlayerData& ProfileData)
{
	CachedProfileData = ProfileData;
	CurrentPlayerId = ProfileData.PlayerId;

	UE_LOG(LogSuspenseCoreSaveManager, Log, TEXT("Profile data set: %s (ID: %s)"),
		*ProfileData.DisplayName, *ProfileData.PlayerId);
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreSaveManager::SetupAutoSaveTimer()
{
	StopAutoSaveTimer();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		AutoSaveTimerHandle,
		this,
		&USuspenseCoreSaveManager::OnAutoSaveTimer,
		AutoSaveInterval,
		true // Looping
	);

	UE_LOG(LogSuspenseCoreSaveManager, Log, TEXT("Auto-save timer started (%.0fs interval)"), AutoSaveInterval);
}

void USuspenseCoreSaveManager::StopAutoSaveTimer()
{
	if (AutoSaveTimerHandle.IsValid())
	{
		UWorld* World = GetWorld();
		if (World)
		{
			World->GetTimerManager().ClearTimer(AutoSaveTimerHandle);
		}
		AutoSaveTimerHandle.Invalidate();
	}
}

void USuspenseCoreSaveManager::OnAutoSaveTimer()
{
	if (!bAutoSaveEnabled || bIsSaving || CurrentPlayerId.IsEmpty())
	{
		return;
	}

	UE_LOG(LogSuspenseCoreSaveManager, Log, TEXT("Auto-save triggered"));
	TriggerAutoSave();
}

void USuspenseCoreSaveManager::SaveToSlotInternal(int32 SlotIndex, const FString& SlotName, bool bIsAutoSave)
{
	if (bIsSaving)
	{
		UE_LOG(LogSuspenseCoreSaveManager, Warning, TEXT("Save already in progress"));
		return;
	}

	bIsSaving = true;
	OnSaveStarted.Broadcast();

	// Collect game state
	FSuspenseCoreSaveData SaveData = CollectCurrentGameState();
	SaveData.Header.SlotName = SlotName.IsEmpty() ?
		FString::Printf(TEXT("Save %d"), SlotIndex) : SlotName;
	SaveData.Header.bIsAutoSave = bIsAutoSave;
	SaveData.Header.SlotIndex = SlotIndex;

	// Save async
	SaveRepository->SaveToSlotAsync(
		CurrentPlayerId,
		SlotIndex,
		SaveData,
		FOnSuspenseCoreSaveComplete::CreateUObject(this, &USuspenseCoreSaveManager::OnSaveCompleteInternal)
	);
}

void USuspenseCoreSaveManager::LoadFromSlotInternal(int32 SlotIndex)
{
	if (bIsLoading)
	{
		UE_LOG(LogSuspenseCoreSaveManager, Warning, TEXT("Load already in progress"));
		return;
	}

	bIsLoading = true;
	OnLoadStarted.Broadcast();

	// Load async
	SaveRepository->LoadFromSlotAsync(
		CurrentPlayerId,
		SlotIndex,
		FOnSuspenseCoreLoadComplete::CreateUObject(this, &USuspenseCoreSaveManager::OnLoadCompleteInternal)
	);
}

void USuspenseCoreSaveManager::OnSaveCompleteInternal(ESuspenseCoreSaveResult Result, const FString& ErrorMessage)
{
	bIsSaving = false;

	bool bSuccess = (Result == ESuspenseCoreSaveResult::Success);

	if (bSuccess)
	{
		LastSaveTime = FDateTime::UtcNow();
		UE_LOG(LogSuspenseCoreSaveManager, Log, TEXT("Save completed successfully"));
	}
	else
	{
		UE_LOG(LogSuspenseCoreSaveManager, Error, TEXT("Save failed: %s"), *ErrorMessage);
	}

	OnSaveCompleted.Broadcast(bSuccess, ErrorMessage);
}

void USuspenseCoreSaveManager::OnLoadCompleteInternal(ESuspenseCoreSaveResult Result, const FSuspenseCoreSaveData& Data, const FString& ErrorMessage)
{
	bIsLoading = false;

	bool bSuccess = (Result == ESuspenseCoreSaveResult::Success);

	if (bSuccess)
	{
		ApplyLoadedState(Data);
		UE_LOG(LogSuspenseCoreSaveManager, Log, TEXT("Load completed successfully"));
	}
	else
	{
		UE_LOG(LogSuspenseCoreSaveManager, Error, TEXT("Load failed: %s"), *ErrorMessage);
	}

	OnLoadCompleted.Broadcast(bSuccess, ErrorMessage);
}

FSuspenseCoreCharacterState USuspenseCoreSaveManager::CollectCharacterState()
{
	FSuspenseCoreCharacterState State;

	UWorld* World = GetWorld();
	if (!World)
	{
		return State;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return State;
	}

	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		return State;
	}

	// Position
	State.WorldPosition = Pawn->GetActorLocation();
	State.WorldRotation = Pawn->GetActorRotation();
	State.CurrentMapName = GetCurrentMapName();

	// Get attributes from GAS (ASC is on PlayerState)
	APlayerState* PlayerState = PC->GetPlayerState<APlayerState>();
	if (PlayerState)
	{
		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(PlayerState))
		{
			if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
			{
				// Get attributes via GetNumericAttributeBase - works without direct AttributeSet include
				// Find any spawned AttributeSet to get attributes
				TArray<UAttributeSet*> AttributeSets;
				ASC->GetAllAttributes(AttributeSets);

				for (UAttributeSet* AttrSet : AttributeSets)
				{
					if (!AttrSet) continue;

					// Try to get Health/MaxHealth/Stamina/MaxStamina/Armor from any AttributeSet
					for (TFieldIterator<FNumericProperty> PropIt(AttrSet->GetClass()); PropIt; ++PropIt)
					{
						FNumericProperty* Property = *PropIt;
						FString PropName = Property->GetName();

						float Value = 0.0f;
						Property->GetValue_InContainer(AttrSet, &Value);

						if (PropName == TEXT("Health")) State.CurrentHealth = Value;
						else if (PropName == TEXT("MaxHealth")) State.MaxHealth = Value;
						else if (PropName == TEXT("Stamina")) State.CurrentStamina = Value;
						else if (PropName == TEXT("MaxStamina")) State.MaxStamina = Value;
						else if (PropName == TEXT("Armor")) State.CurrentArmor = Value;
						else if (PropName == TEXT("Shield")) State.CurrentShield = Value;
					}
				}

				// Collect active gameplay effects
				const FActiveGameplayEffectsContainer& ActiveEffects = ASC->GetActiveGameplayEffects();
				for (const FActiveGameplayEffect& Effect : &ActiveEffects)
				{
					if (Effect.Spec.Def)
					{
						FSuspenseCoreActiveEffect ActiveEffect;
						ActiveEffect.EffectId = Effect.Spec.Def->GetPathName();
						ActiveEffect.RemainingDuration = Effect.GetDuration();
						ActiveEffect.StackCount = Effect.Spec.GetStackCount();
						ActiveEffect.Level = Effect.Spec.GetLevel();
						State.ActiveEffects.Add(ActiveEffect);
					}
				}

				UE_LOG(LogSuspenseCoreSaveManager, Log, TEXT("Collected GAS state - Health: %.1f/%.1f, Stamina: %.1f/%.1f, Effects: %d"),
					State.CurrentHealth, State.MaxHealth, State.CurrentStamina, State.MaxStamina, State.ActiveEffects.Num());
			}
		}
	}

	// Fallback to defaults if ASC not found
	if (State.MaxHealth <= 0.0f)
	{
		UE_LOG(LogSuspenseCoreSaveManager, Warning, TEXT("ASC not found, using default attribute values"));
		State.CurrentHealth = 100.0f;
		State.MaxHealth = 100.0f;
		State.CurrentStamina = 100.0f;
		State.MaxStamina = 100.0f;
	}

	// Movement state from Character
	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		State.bIsCrouching = Character->bIsCrouched;
	}

	return State;
}

FName USuspenseCoreSaveManager::GetCurrentMapName() const
{
	UWorld* World = GetWorld();
	if (World)
	{
		return FName(*World->GetMapName());
	}
	return NAME_None;
}

int64 USuspenseCoreSaveManager::GetTotalPlayTime() const
{
	double CurrentTime = FPlatformTime::Seconds();
	double SessionTime = CurrentTime - SessionStartTime;

	// Add previous play time from profile
	return CachedProfileData.Stats.PlayTimeSeconds + static_cast<int64>(SessionTime);
}
