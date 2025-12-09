// SuspenseCoreCharacterSelectionSubsystem.cpp
// SuspenseCore - Clean Architecture BridgeSystem
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Subsystems/SuspenseCoreCharacterSelectionSubsystem.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreCharacterSelection, Log, All);

// ═══════════════════════════════════════════════════════════════════════════════
// UGAMEINSTANCESUBSYSTEM INTERFACE
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreCharacterSelectionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	SelectedClassData = nullptr;
	SelectedClassId = NAME_None;

	UE_LOG(LogSuspenseCoreCharacterSelection, Log, TEXT("CharacterSelectionSubsystem initialized"));
}

void USuspenseCoreCharacterSelectionSubsystem::Deinitialize()
{
	SelectedClassData = nullptr;
	SelectedClassId = NAME_None;
	ClassRegistry.Empty();
	CachedEventBus.Reset();

	UE_LOG(LogSuspenseCoreCharacterSelection, Log, TEXT("CharacterSelectionSubsystem deinitialized"));

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

void USuspenseCoreCharacterSelectionSubsystem::SelectCharacterClass(UObject* ClassData, FName ClassId)
{
	if (!ClassData || ClassId == NAME_None)
	{
		UE_LOG(LogSuspenseCoreCharacterSelection, Warning, TEXT("SelectCharacterClass: Invalid parameters"));
		return;
	}

	// Skip if already selected
	if (SelectedClassData == ClassData && SelectedClassId == ClassId)
	{
		return;
	}

	SelectedClassData = ClassData;
	SelectedClassId = ClassId;

	// Register if not already registered
	if (!ClassRegistry.Contains(SelectedClassId))
	{
		ClassRegistry.Add(SelectedClassId, ClassData);
	}

	UE_LOG(LogSuspenseCoreCharacterSelection, Log, TEXT("Selected class: %s"), *SelectedClassId.ToString());

	// Publish event for PreviewActor and other listeners
	PublishClassChangedEvent();
}

void USuspenseCoreCharacterSelectionSubsystem::SelectCharacterClassById(FName ClassId)
{
	if (ClassId == NAME_None)
	{
		UE_LOG(LogSuspenseCoreCharacterSelection, Warning, TEXT("SelectCharacterClassById: ClassId is NAME_None"));
		return;
	}

	// Try to find in registry first
	UObject* ClassData = LoadClassById(ClassId);
	if (ClassData)
	{
		SelectCharacterClass(ClassData, ClassId);
	}
	else
	{
		// Store the ID even if data not found (will be loaded later)
		SelectedClassId = ClassId;
		SelectedClassData = nullptr;
		UE_LOG(LogSuspenseCoreCharacterSelection, Warning, TEXT("Class '%s' not found in registry, storing ID only"), *ClassId.ToString());

		// Still publish event so UI can react
		PublishClassChangedEvent();
	}
}

void USuspenseCoreCharacterSelectionSubsystem::ClearSelection()
{
	SelectedClassData = nullptr;
	SelectedClassId = NAME_None;

	UE_LOG(LogSuspenseCoreCharacterSelection, Log, TEXT("Selection cleared"));
}

UObject* USuspenseCoreCharacterSelectionSubsystem::LoadClassById(FName ClassId)
{
	if (ClassId == NAME_None)
	{
		return nullptr;
	}

	// Check registry
	if (UObject** FoundClass = ClassRegistry.Find(ClassId))
	{
		return *FoundClass;
	}

	UE_LOG(LogSuspenseCoreCharacterSelection, Warning, TEXT("Class '%s' not found in registry"), *ClassId.ToString());
	return nullptr;
}

void USuspenseCoreCharacterSelectionSubsystem::RegisterClassData(UObject* ClassData, FName ClassId)
{
	if (!ClassData || ClassId == NAME_None)
	{
		UE_LOG(LogSuspenseCoreCharacterSelection, Warning, TEXT("Cannot register class with invalid data or NAME_None ID"));
		return;
	}

	ClassRegistry.Add(ClassId, ClassData);

	UE_LOG(LogSuspenseCoreCharacterSelection, Log, TEXT("Registered class: %s"), *ClassId.ToString());
}

TArray<FName> USuspenseCoreCharacterSelectionSubsystem::GetAllRegisteredClassIds() const
{
	TArray<FName> Result;
	ClassRegistry.GetKeys(Result);
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
		UE_LOG(LogSuspenseCoreCharacterSelection, Warning, TEXT("Cannot publish event - EventBus not available"));
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

	UE_LOG(LogSuspenseCoreCharacterSelection, Log, TEXT("Published CharacterClass.Changed event for: %s"), *SelectedClassId.ToString());
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

	UE_LOG(LogSuspenseCoreCharacterSelection, Log, TEXT("Published CharacterClass.Selected event for: %s"), *SelectedClassId.ToString());
}
