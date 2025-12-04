// SuspenseCoreDataManager.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Settings/SuspenseCoreSettings.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "Engine/DataTable.h"
#include "Engine/DataAsset.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreData, Log, All);

//========================================================================
// Static Access
//========================================================================

USuspenseCoreDataManager* USuspenseCoreDataManager::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	return GameInstance->GetSubsystem<USuspenseCoreDataManager>();
}

//========================================================================
// USubsystem Interface
//========================================================================

bool USuspenseCoreDataManager::ShouldCreateSubsystem(UObject* Outer) const
{
	// Always create - this is the core data system
	return true;
}

void USuspenseCoreDataManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogSuspenseCoreData, Log, TEXT("═══════════════════════════════════════════════════════════════"));
	UE_LOG(LogSuspenseCoreData, Log, TEXT("  SUSPENSECORE DATA MANAGER - INITIALIZATION START"));
	UE_LOG(LogSuspenseCoreData, Log, TEXT("═══════════════════════════════════════════════════════════════"));

	// Get settings
	const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
	if (!Settings)
	{
		UE_LOG(LogSuspenseCoreData, Error, TEXT("SuspenseCoreSettings not found! Data Manager cannot initialize."));
		return;
	}

	// Validate settings
	TArray<FString> ConfigErrors;
	if (!Settings->ValidateConfiguration(ConfigErrors))
	{
		UE_LOG(LogSuspenseCoreData, Warning, TEXT("Settings validation found issues:"));
		for (const FString& Error : ConfigErrors)
		{
			UE_LOG(LogSuspenseCoreData, Warning, TEXT("  - %s"), *Error);
		}
	}

	//========================================================================
	// Initialize Systems
	//========================================================================

	// Item System (Primary - most critical)
	bItemSystemReady = InitializeItemSystem();
	if (!bItemSystemReady)
	{
		UE_LOG(LogSuspenseCoreData, Error, TEXT("Item System initialization FAILED!"));
	}
	else
	{
		UE_LOG(LogSuspenseCoreData, Log, TEXT("Item System: READY (%d items cached)"), ItemCache.Num());
	}

	// Character System
	bCharacterSystemReady = InitializeCharacterSystem();
	if (!bCharacterSystemReady)
	{
		UE_LOG(LogSuspenseCoreData, Warning, TEXT("Character System initialization failed (may be optional)"));
	}
	else
	{
		UE_LOG(LogSuspenseCoreData, Log, TEXT("Character System: READY"));
	}

	// Loadout System
	bLoadoutSystemReady = InitializeLoadoutSystem();
	if (!bLoadoutSystemReady)
	{
		UE_LOG(LogSuspenseCoreData, Warning, TEXT("Loadout System initialization failed (may be optional)"));
	}
	else
	{
		UE_LOG(LogSuspenseCoreData, Log, TEXT("Loadout System: READY"));
	}

	//========================================================================
	// Validation (if enabled)
	//========================================================================

	if (Settings->bValidateItemsOnStartup && bItemSystemReady)
	{
		UE_LOG(LogSuspenseCoreData, Log, TEXT("Running item validation..."));

		TArray<FString> ValidationErrors;
		int32 ErrorCount = ValidateAllItems(ValidationErrors);

		if (ErrorCount > 0)
		{
			UE_LOG(LogSuspenseCoreData, Warning, TEXT("Validation found %d items with errors:"), ErrorCount);
			for (const FString& Error : ValidationErrors)
			{
				UE_LOG(LogSuspenseCoreData, Warning, TEXT("  - %s"), *Error);
			}

			if (Settings->bStrictItemValidation)
			{
				UE_LOG(LogSuspenseCoreData, Error, TEXT("STRICT VALIDATION ENABLED - Critical items have errors!"));
				BroadcastValidationResult(false, ValidationErrors);
			}
			else
			{
				BroadcastValidationResult(true, ValidationErrors);
			}
		}
		else
		{
			UE_LOG(LogSuspenseCoreData, Log, TEXT("All items validated successfully"));
			BroadcastValidationResult(true, TArray<FString>());
		}
	}

	//========================================================================
	// Complete Initialization
	//========================================================================

	bIsInitialized = bItemSystemReady; // At minimum, items must work

	if (bIsInitialized)
	{
		BroadcastInitialized();
	}

	UE_LOG(LogSuspenseCoreData, Log, TEXT("═══════════════════════════════════════════════════════════════"));
	UE_LOG(LogSuspenseCoreData, Log, TEXT("  SUSPENSECORE DATA MANAGER - INITIALIZATION %s"),
		bIsInitialized ? TEXT("COMPLETE") : TEXT("FAILED"));
	UE_LOG(LogSuspenseCoreData, Log, TEXT("═══════════════════════════════════════════════════════════════"));
}

void USuspenseCoreDataManager::Deinitialize()
{
	UE_LOG(LogSuspenseCoreData, Log, TEXT("SuspenseCoreDataManager shutting down..."));

	// Clear caches
	ItemCache.Empty();
	LoadedItemDataTable = nullptr;
	LoadedCharacterClassesDataAsset = nullptr;
	LoadedLoadoutDataTable = nullptr;
	CachedEventBus.Reset();

	bIsInitialized = false;
	bItemSystemReady = false;
	bCharacterSystemReady = false;
	bLoadoutSystemReady = false;

	Super::Deinitialize();
}

//========================================================================
// Initialization Helpers
//========================================================================

bool USuspenseCoreDataManager::InitializeItemSystem()
{
	const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
	if (!Settings)
	{
		return false;
	}

	// Check if ItemDataTable is configured
	if (Settings->ItemDataTable.IsNull())
	{
		UE_LOG(LogSuspenseCoreData, Error,
			TEXT("ItemDataTable not configured in Project Settings → Game → SuspenseCore!"));
		UE_LOG(LogSuspenseCoreData, Error,
			TEXT("Please configure ItemDataTable to enable the item system."));
		return false;
	}

	// Load the DataTable
	UE_LOG(LogSuspenseCoreData, Log, TEXT("Loading ItemDataTable: %s"),
		*Settings->ItemDataTable.ToString());

	LoadedItemDataTable = Settings->ItemDataTable.LoadSynchronous();

	if (!LoadedItemDataTable)
	{
		UE_LOG(LogSuspenseCoreData, Error, TEXT("Failed to load ItemDataTable!"));
		return false;
	}

	UE_LOG(LogSuspenseCoreData, Log, TEXT("ItemDataTable loaded: %s"),
		*LoadedItemDataTable->GetName());

	// Verify row structure
	const UScriptStruct* RowStruct = LoadedItemDataTable->GetRowStruct();
	if (!RowStruct)
	{
		UE_LOG(LogSuspenseCoreData, Error, TEXT("ItemDataTable has no row structure!"));
		return false;
	}

	UE_LOG(LogSuspenseCoreData, Log, TEXT("Row Structure: %s"), *RowStruct->GetName());

	// Build cache
	if (!BuildItemCache(LoadedItemDataTable))
	{
		UE_LOG(LogSuspenseCoreData, Error, TEXT("Failed to build item cache!"));
		return false;
	}

	return true;
}

bool USuspenseCoreDataManager::BuildItemCache(UDataTable* DataTable)
{
	if (!DataTable)
	{
		return false;
	}

	ItemCache.Empty();

	const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
	const bool bVerbose = Settings && Settings->bLogItemOperations;

	// Get all row names
	TArray<FName> RowNames = DataTable->GetRowNames();
	UE_LOG(LogSuspenseCoreData, Log, TEXT("Building cache from %d rows..."), RowNames.Num());

	int32 LoadedCount = 0;
	int32 FailedCount = 0;

	for (const FName& RowName : RowNames)
	{
		// Try to get as FSuspenseUnifiedItemData
		FSuspenseUnifiedItemData* RowData = DataTable->FindRow<FSuspenseUnifiedItemData>(RowName, TEXT(""));

		if (RowData)
		{
			// Use RowName as ItemID if not set
			if (RowData->ItemID.IsNone())
			{
				RowData->ItemID = RowName;
			}

			ItemCache.Add(RowName, *RowData);
			LoadedCount++;

			if (bVerbose)
			{
				UE_LOG(LogSuspenseCoreData, Verbose, TEXT("  Cached: %s (%s)"),
					*RowName.ToString(), *RowData->DisplayName.ToString());
			}
		}
		else
		{
			FailedCount++;
			UE_LOG(LogSuspenseCoreData, Warning, TEXT("  Failed to read row: %s"), *RowName.ToString());
		}
	}

	UE_LOG(LogSuspenseCoreData, Log, TEXT("Item cache built: %d items loaded, %d failed"),
		LoadedCount, FailedCount);

	return LoadedCount > 0;
}

bool USuspenseCoreDataManager::InitializeCharacterSystem()
{
	const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
	if (!Settings)
	{
		return false;
	}

	if (Settings->CharacterClassesDataAsset.IsNull())
	{
		UE_LOG(LogSuspenseCoreData, Verbose, TEXT("CharacterClassesDataAsset not configured (optional)"));
		return false;
	}

	LoadedCharacterClassesDataAsset = Settings->CharacterClassesDataAsset.LoadSynchronous();

	if (!LoadedCharacterClassesDataAsset)
	{
		UE_LOG(LogSuspenseCoreData, Warning, TEXT("Failed to load CharacterClassesDataAsset"));
		return false;
	}

	UE_LOG(LogSuspenseCoreData, Log, TEXT("CharacterClassesDataAsset loaded: %s"),
		*LoadedCharacterClassesDataAsset->GetName());

	return true;
}

bool USuspenseCoreDataManager::InitializeLoadoutSystem()
{
	const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
	if (!Settings)
	{
		return false;
	}

	if (Settings->LoadoutDataTable.IsNull())
	{
		UE_LOG(LogSuspenseCoreData, Verbose, TEXT("LoadoutDataTable not configured (optional)"));
		return false;
	}

	LoadedLoadoutDataTable = Settings->LoadoutDataTable.LoadSynchronous();

	if (!LoadedLoadoutDataTable)
	{
		UE_LOG(LogSuspenseCoreData, Warning, TEXT("Failed to load LoadoutDataTable"));
		return false;
	}

	UE_LOG(LogSuspenseCoreData, Log, TEXT("LoadoutDataTable loaded: %s"),
		*LoadedLoadoutDataTable->GetName());

	return true;
}

//========================================================================
// Item Data Access
//========================================================================

bool USuspenseCoreDataManager::GetItemData(FName ItemID, FSuspenseUnifiedItemData& OutItemData) const
{
	if (ItemID.IsNone())
	{
		return false;
	}

	const FSuspenseUnifiedItemData* Found = ItemCache.Find(ItemID);

	if (Found)
	{
		OutItemData = *Found;
		BroadcastItemLoaded(ItemID, OutItemData);
		return true;
	}

	BroadcastItemNotFound(ItemID);
	return false;
}

bool USuspenseCoreDataManager::HasItem(FName ItemID) const
{
	return ItemCache.Contains(ItemID);
}

TArray<FName> USuspenseCoreDataManager::GetAllItemIDs() const
{
	TArray<FName> ItemIDs;
	ItemCache.GetKeys(ItemIDs);
	return ItemIDs;
}

//========================================================================
// TODO: Item Instance Creation (Future Implementation)
//========================================================================
//
// CreateItemInstance() will be implemented when SuspenseCore has its own
// inventory types (FSuspenseCoreItemInstance). Currently removed to avoid
// legacy dependency on FSuspenseInventoryItemInstance from BridgeSystem.
//
// Implementation requirements:
// 1. Create FSuspenseCoreItemInstance in SuspenseCore/Types/SuspenseCoreInventoryTypes.h
// 2. Broadcast SuspenseCore.Event.Item.InstanceCreated via EventBus
// 3. Include proper runtime properties initialization
//
//========================================================================

//========================================================================
// Item Validation
//========================================================================

bool USuspenseCoreDataManager::ValidateItem(FName ItemID, TArray<FString>& OutErrors) const
{
	OutErrors.Empty();

	const FSuspenseUnifiedItemData* ItemData = ItemCache.Find(ItemID);
	if (!ItemData)
	{
		OutErrors.Add(FString::Printf(TEXT("Item '%s' not found in cache"), *ItemID.ToString()));
		return false;
	}

	bool bIsValid = true;

	// Validate display name
	if (ItemData->DisplayName.IsEmpty())
	{
		OutErrors.Add(FString::Printf(TEXT("[%s] DisplayName is empty"), *ItemID.ToString()));
		bIsValid = false;
	}

	// Validate item type
	if (!ItemData->ItemType.IsValid())
	{
		OutErrors.Add(FString::Printf(TEXT("[%s] ItemType tag is invalid"), *ItemID.ToString()));
		bIsValid = false;
	}
	else
	{
		// Must be in Item.* hierarchy
		static const FGameplayTag BaseItemTag = FGameplayTag::RequestGameplayTag(TEXT("Item"), false);
		if (BaseItemTag.IsValid() && !ItemData->ItemType.MatchesTag(BaseItemTag))
		{
			OutErrors.Add(FString::Printf(TEXT("[%s] ItemType '%s' is not in Item.* hierarchy"),
				*ItemID.ToString(), *ItemData->ItemType.ToString()));
			bIsValid = false;
		}
	}

	// Validate weapon-specific
	if (ItemData->bIsWeapon)
	{
		if (ItemData->FireModes.Num() == 0)
		{
			OutErrors.Add(FString::Printf(TEXT("[%s] Weapon has no fire modes"), *ItemID.ToString()));
			bIsValid = false;
		}

		if (!ItemData->WeaponInitialization.WeaponAttributeSetClass)
		{
			OutErrors.Add(FString::Printf(TEXT("[%s] Weapon missing WeaponAttributeSetClass"),
				*ItemID.ToString()));
			bIsValid = false;
		}
	}

	// Validate stack size
	if (ItemData->MaxStackSize <= 0)
	{
		OutErrors.Add(FString::Printf(TEXT("[%s] Invalid MaxStackSize: %d"),
			*ItemID.ToString(), ItemData->MaxStackSize));
		bIsValid = false;
	}

	return bIsValid;
}

int32 USuspenseCoreDataManager::ValidateAllItems(TArray<FString>& OutErrors) const
{
	OutErrors.Empty();
	int32 ErrorCount = 0;

	for (const auto& ItemPair : ItemCache)
	{
		TArray<FString> ItemErrors;
		if (!ValidateItem(ItemPair.Key, ItemErrors))
		{
			ErrorCount++;
			OutErrors.Append(ItemErrors);
		}
	}

	return ErrorCount;
}

//========================================================================
// Character Data Access
//========================================================================

UDataAsset* USuspenseCoreDataManager::GetCharacterClassesDataAsset() const
{
	return LoadedCharacterClassesDataAsset;
}

FGameplayTag USuspenseCoreDataManager::GetDefaultCharacterClass() const
{
	const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
	return Settings ? Settings->DefaultCharacterClass : FGameplayTag();
}

//========================================================================
// Loadout Data Access
//========================================================================

UDataTable* USuspenseCoreDataManager::GetLoadoutDataTable() const
{
	return LoadedLoadoutDataTable;
}

FName USuspenseCoreDataManager::GetDefaultLoadoutID() const
{
	const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
	return Settings ? Settings->DefaultLoadoutID : NAME_None;
}

//========================================================================
// EventBus Integration
//========================================================================

USuspenseCoreEventBus* USuspenseCoreDataManager::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this);
	if (EventManager)
	{
		USuspenseCoreEventBus* EventBus = EventManager->GetEventBus();
		CachedEventBus = EventBus;
		return EventBus;
	}

	return nullptr;
}

//========================================================================
// EventBus Broadcasting
//========================================================================

void USuspenseCoreDataManager::BroadcastInitialized()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		const_cast<USuspenseCoreDataManager*>(this),
		ESuspenseCoreEventPriority::High
	);

	EventData.SetInt(TEXT("CachedItemCount"), ItemCache.Num());
	EventData.SetBool(TEXT("ItemSystemReady"), bItemSystemReady);
	EventData.SetBool(TEXT("CharacterSystemReady"), bCharacterSystemReady);
	EventData.SetBool(TEXT("LoadoutSystemReady"), bLoadoutSystemReady);

	static const FGameplayTag InitializedTag =
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Data.Initialized"));

	EventBus->Publish(InitializedTag, EventData);

	UE_LOG(LogSuspenseCoreData, Log, TEXT("Broadcast: Data.Initialized (Items: %d)"), ItemCache.Num());
}

void USuspenseCoreDataManager::BroadcastItemLoaded(FName ItemID, const FSuspenseUnifiedItemData& ItemData) const
{
	const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
	if (!Settings || !Settings->bLogItemOperations)
	{
		return; // Skip event if not logging
	}

	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		const_cast<USuspenseCoreDataManager*>(this),
		ESuspenseCoreEventPriority::Low
	);

	EventData.SetString(TEXT("ItemID"), ItemID.ToString());
	EventData.SetString(TEXT("DisplayName"), ItemData.DisplayName.ToString());
	EventData.SetString(TEXT("ItemType"), ItemData.ItemType.ToString());

	static const FGameplayTag ItemLoadedTag =
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Data.ItemLoaded"));

	EventBus->Publish(ItemLoadedTag, EventData);
}

void USuspenseCoreDataManager::BroadcastItemNotFound(FName ItemID) const
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		const_cast<USuspenseCoreDataManager*>(this),
		ESuspenseCoreEventPriority::Normal
	);

	EventData.SetString(TEXT("ItemID"), ItemID.ToString());

	static const FGameplayTag ItemNotFoundTag =
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Data.ItemNotFound"));

	EventBus->Publish(ItemNotFoundTag, EventData);

	UE_LOG(LogSuspenseCoreData, Warning, TEXT("Broadcast: Data.ItemNotFound - %s"), *ItemID.ToString());
}

void USuspenseCoreDataManager::BroadcastValidationResult(bool bPassed, const TArray<FString>& Errors) const
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		const_cast<USuspenseCoreDataManager*>(this),
		ESuspenseCoreEventPriority::High
	);

	EventData.SetBool(TEXT("Passed"), bPassed);
	EventData.SetInt(TEXT("ErrorCount"), Errors.Num());

	// Concatenate errors for event
	FString ErrorSummary;
	for (int32 i = 0; i < FMath::Min(Errors.Num(), 10); i++)
	{
		ErrorSummary += Errors[i] + TEXT("\n");
	}
	EventData.SetString(TEXT("Errors"), ErrorSummary);

	FGameplayTag ValidationTag = bPassed
		? FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Data.ValidationPassed"))
		: FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Data.ValidationFailed"));

	EventBus->Publish(ValidationTag, EventData);

	UE_LOG(LogSuspenseCoreData, Log, TEXT("Broadcast: Data.Validation%s"),
		bPassed ? TEXT("Passed") : TEXT("Failed"));
}
