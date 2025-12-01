// SuspenseCoreCharacterSelectionSubsystem.cpp
// SuspenseCore - Clean Architecture BridgeSystem
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Subsystems/SuspenseCoreCharacterSelectionSubsystem.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Data/SuspenseCoreCharacterClassData.h"
#include "Engine/GameInstance.h"

// ═══════════════════════════════════════════════════════════════════════════════
// UGAMEINSTANCESUBSYSTEM INTERFACE
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreCharacterSelectionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	SelectedClassData = nullptr;
	SelectedClassId = NAME_None;

	UE_LOG(LogTemp, Log, TEXT("[CharacterSelectionSubsystem] Initialized"));
}

void USuspenseCoreCharacterSelectionSubsystem::Deinitialize()
{
	SelectedClassData = nullptr;
	SelectedClassId = NAME_None;
	ClassRegistry.Empty();
	CachedEventBus.Reset();

	UE_LOG(LogTemp, Log, TEXT("[CharacterSelectionSubsystem] Deinitialized"));

	Super::Deinitialize();
}

// ═══════════════════════════════════════════════════════════════════════════════
// STATIC ACCESSOR
// ═══════════════════════════════════════════════════════════════════════════════

USuspenseCoreCharacterSelectionSubsystem* USuspenseCoreCharacterSelectionSubsystem::Get(const UObject* WorldContextObject)
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

	return GameInstance->GetSubsystem<USuspenseCoreCharacterSelectionSubsystem>();
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreCharacterSelectionSubsystem::SelectCharacterClass(USuspenseCoreCharacterClassData* ClassData)
{
	if (!ClassData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterSelectionSubsystem] SelectCharacterClass: ClassData is null"));
		return;
	}

	// Skip if already selected
	if (SelectedClassData == ClassData)
	{
		return;
	}

	SelectedClassData = ClassData;
	SelectedClassId = ClassData->ClassID;

	// Register if not already registered
	if (!ClassRegistry.Contains(SelectedClassId))
	{
		ClassRegistry.Add(SelectedClassId, ClassData);
	}

	UE_LOG(LogTemp, Log, TEXT("[CharacterSelectionSubsystem] Selected class: %s"), *SelectedClassId.ToString());

	// Publish event for PreviewActor and other listeners
	PublishClassChangedEvent();
}

void USuspenseCoreCharacterSelectionSubsystem::SelectCharacterClassById(FName ClassId)
{
	if (ClassId == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterSelectionSubsystem] SelectCharacterClassById: ClassId is NAME_None"));
		return;
	}

	// Try to find in registry first
	USuspenseCoreCharacterClassData* ClassData = LoadClassById(ClassId);
	if (ClassData)
	{
		SelectCharacterClass(ClassData);
	}
	else
	{
		// Store the ID even if data not found (will be loaded later)
		SelectedClassId = ClassId;
		UE_LOG(LogTemp, Warning, TEXT("[CharacterSelectionSubsystem] Class '%s' not found in registry, storing ID only"), *ClassId.ToString());

		// Still publish event so UI can react
		PublishClassChangedEvent();
	}
}

void USuspenseCoreCharacterSelectionSubsystem::ClearSelection()
{
	SelectedClassData = nullptr;
	SelectedClassId = NAME_None;

	UE_LOG(LogTemp, Log, TEXT("[CharacterSelectionSubsystem] Selection cleared"));
}

USuspenseCoreCharacterClassData* USuspenseCoreCharacterSelectionSubsystem::LoadClassById(FName ClassId)
{
	if (ClassId == NAME_None)
	{
		return nullptr;
	}

	// Check registry
	if (USuspenseCoreCharacterClassData** FoundClass = ClassRegistry.Find(ClassId))
	{
		return *FoundClass;
	}

	UE_LOG(LogTemp, Warning, TEXT("[CharacterSelectionSubsystem] Class '%s' not found in registry"), *ClassId.ToString());
	return nullptr;
}

void USuspenseCoreCharacterSelectionSubsystem::RegisterClassData(USuspenseCoreCharacterClassData* ClassData)
{
	if (!ClassData)
	{
		return;
	}

	FName ClassId = ClassData->ClassID;
	if (ClassId == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterSelectionSubsystem] Cannot register class with NAME_None ID"));
		return;
	}

	ClassRegistry.Add(ClassId, ClassData);

	UE_LOG(LogTemp, Log, TEXT("[CharacterSelectionSubsystem] Registered class: %s (%s)"),
		*ClassId.ToString(), *ClassData->DisplayName.ToString());
}

TArray<USuspenseCoreCharacterClassData*> USuspenseCoreCharacterSelectionSubsystem::GetAllRegisteredClasses() const
{
	TArray<USuspenseCoreCharacterClassData*> Result;
	ClassRegistry.GenerateValueArray(Result);
	return Result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL METHODS
// ═══════════════════════════════════════════════════════════════════════════════

USuspenseCoreEventBus* USuspenseCoreCharacterSelectionSubsystem::GetEventBus()
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return nullptr;
	}

	// Try to get EventManager
	if (USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GI))
	{
		USuspenseCoreEventBus* EventBus = Manager->GetEventBus();
		if (EventBus)
		{
			CachedEventBus = EventBus;
		}
		return EventBus;
	}

	return nullptr;
}

void USuspenseCoreCharacterSelectionSubsystem::PublishClassChangedEvent()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterSelectionSubsystem] Cannot publish event - EventBus not available"));
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetString(FName("ClassId"), SelectedClassId.ToString());

	if (SelectedClassData)
	{
		EventData.SetObject(FName("ClassData"), SelectedClassData);
	}

	EventBus->Publish(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.CharacterClass.Changed")),
		EventData
	);

	UE_LOG(LogTemp, Log, TEXT("[CharacterSelectionSubsystem] Published CharacterClass.Changed event for: %s"), *SelectedClassId.ToString());
}

void USuspenseCoreCharacterSelectionSubsystem::PublishClassSelectedEvent()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetString(FName("ClassId"), SelectedClassId.ToString());

	if (SelectedClassData)
	{
		EventData.SetObject(FName("ClassData"), SelectedClassData);
	}

	EventBus->Publish(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.CharacterClass.Selected")),
		EventData
	);

	UE_LOG(LogTemp, Log, TEXT("[CharacterSelectionSubsystem] Published CharacterClass.Selected event for: %s"), *SelectedClassId.ToString());
}
