// Copyright Suspense Team. All Rights Reserved.
#include "SuspenseCore/Components/Integration/SuspenseCoreEquipmentLoadoutAdapter.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreLoadoutAdapter.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentOperations.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreTransactionManager.h"
#include "SuspenseCore/Services/SuspenseCoreLoadoutManager.h"
#include "SuspenseCore/ItemSystem/SuspenseCoreItemManager.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCore/Types/Equipment/SuspenseCoreEquipmentTypes.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentOperationService.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceLocator.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"

DEFINE_LOG_CATEGORY_STATIC(LogLoadoutAdapter, Log, All);

USuspenseCoreEquipmentLoadoutAdapter::USuspenseCoreEquipmentLoadoutAdapter()
{
	PrimaryComponentTick.bCanEverTick = false;

	ValidationOptions.bCheckCharacterClass    = true;
	ValidationOptions.bCheckInventorySpace    = true;
	ValidationOptions.bCheckItemAvailability  = true;
	ValidationOptions.bCheckSlotCompatibility = true;
	ValidationOptions.bCheckWeightLimits      = true;
}

void USuspenseCoreEquipmentLoadoutAdapter::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogLoadoutAdapter, Log, TEXT("LoadoutAdapter: BeginPlay"));
}

void USuspenseCoreEquipmentLoadoutAdapter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ActiveTransactionId.IsValid() && TransactionManager.GetInterface())
	{
		TransactionManager->RollbackTransaction(ActiveTransactionId);
		ActiveTransactionId.Invalidate();
	}

	DataProvider        = nullptr;
	OperationsExecutor  = nullptr;
	TransactionManager  = nullptr;

	UE_LOG(LogLoadoutAdapter, Log, TEXT("LoadoutAdapter: EndPlay"));
	Super::EndPlay(EndPlayReason);
}

bool USuspenseCoreEquipmentLoadoutAdapter::Initialize(
	TScriptInterface<ISuspenseCoreEquipmentDataProvider> InDataProvider,
	TScriptInterface<ISuspenseCoreEquipmentOperations>   InOperations,
	TScriptInterface<ISuspenseCoreTransactionManager>    InTransactionManager)
{
	FScopeLock Lock(&AdapterCriticalSection);

	if (!InDataProvider.GetInterface())       { UE_LOG(LogLoadoutAdapter, Error, TEXT("Initialize: DataProvider is null"));        return false; }
	if (!InOperations.GetInterface())         { UE_LOG(LogLoadoutAdapter, Error, TEXT("Initialize: Operations executor is null")); return false; }
	if (!InTransactionManager.GetInterface()) { UE_LOG(LogLoadoutAdapter, Error, TEXT("Initialize: TransactionManager is null"));  return false; }

	DataProvider       = InDataProvider;
	OperationsExecutor = InOperations;
	TransactionManager = InTransactionManager;

	bIsInitialized = true;
	UE_LOG(LogLoadoutAdapter, Log, TEXT("Initialize: Adapter initialized"));
	return true;
}

USuspenseCoreEquipmentOperationService* USuspenseCoreEquipmentLoadoutAdapter::GetOperationService()
{
	if (CachedOpService.IsValid())
	{
		return CachedOpService.Get();
	}

	if (USuspenseCoreEquipmentServiceLocator* Locator = USuspenseCoreEquipmentServiceLocator::Get(this))
	{
		// По соглашению сервис транзакций/операций помечен Service.Equipment.Transaction
		if (UObject* SvcObj = Locator->GetService(FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Transaction"))))
		{
			if (USuspenseCoreEquipmentOperationService* Impl = Cast<USuspenseCoreEquipmentOperationService>(SvcObj))
			{
				CachedOpService = Impl; // теперь метод не const — кэшируем нормально
				return Impl;
			}
		}
	}
	return nullptr;
}

FSuspenseCoreLoadoutApplicationResult USuspenseCoreEquipmentLoadoutAdapter::ApplyLoadout(const FName& LoadoutId, bool bForce)
{
	FScopeLock Lock(&AdapterCriticalSection);

	if (!bIsInitialized) { return FSuspenseCoreLoadoutApplicationResult::Failure(LoadoutId, FText::FromString(TEXT("Adapter not initialized"))); }
	if (bIsApplying)      { return FSuspenseCoreLoadoutApplicationResult::Failure(LoadoutId, FText::FromString(TEXT("Another loadout is being applied"))); }

	if (!GetLoadoutManager()) { return FSuspenseCoreLoadoutApplicationResult::Failure(LoadoutId, FText::FromString(TEXT("LoadoutManager not found"))); }

	FSuspenseCoreLoadoutConfiguration Config;
	if (!GetLoadoutConfiguration(LoadoutId, Config)) { return FSuspenseCoreLoadoutApplicationResult::Failure(LoadoutId, FText::FromString(TEXT("Loadout not found"))); }

	// ——— optional centralized path via OperationService (S8 pipeline) ———
	if (bPreferOperationService)
	{
		if (USuspenseCoreEquipmentOperationService* OpSvc = GetOperationService())
		{
			// Preflight validation (optional, unless bForce)
			if (!bForce)
			{
				TArray<FText> ValidationErrors;
				if (!ValidateLoadoutConfiguration(Config, ValidationOptions, ValidationErrors))
				{
					FString Combined;
					for (const FText& E : ValidationErrors) { Combined += E.ToString() + TEXT("\n"); }
					return FSuspenseCoreLoadoutApplicationResult::Failure(LoadoutId, FText::FromString(Combined));
				}
			}

			// Build & execute a batch of operations atomically
			const TArray<FEquipmentOperationRequest> Requests = CreateOperationsFromLoadout(Config);

			TArray<FEquipmentOperationResult> Results;
			const FGuid BatchId = OpSvc->BatchOperationsEx(Requests, /*bAtomic=*/true, Results);

			int32  SuccessCount = 0;
			FString FirstError;
			for (const FEquipmentOperationResult& R : Results)
			{
				if (R.bSuccess) { ++SuccessCount; }
				else if (FirstError.IsEmpty()) { FirstError = R.ErrorMessage.ToString(); }
			}

			FSuspenseCoreLoadoutApplicationResult Result;
			Result.LoadoutId = LoadoutId;
			Result.bSuccess  = (SuccessCount == Requests.Num());
			Result.ItemsEquipped = SuccessCount;
			Result.ItemsFailed = Requests.Num() - SuccessCount;
			if (!Result.bSuccess && !FirstError.IsEmpty())
			{
				Result.AddError(FText::FromString(FirstError));
			}

			if (Result.bSuccess)
			{
				CurrentLoadoutId = LoadoutId;
				NotifyLoadoutChange(LoadoutId, /*bSuccess=*/true);
			}

			LastApplicationResult = Result;
			return Result;
		}
	}

	// ——— fallback path via TransactionManager/OperationsExecutor ———
	bIsApplying = true;
	ON_SCOPE_EXIT { bIsApplying = false; };

	FSuspenseCoreLoadoutApplicationResult LocalResult = ApplyLoadoutConfiguration(Config, ESuspenseCoreLoadoutStrategy::Replace);

	if (LocalResult.bSuccess)
	{
		CurrentLoadoutId = LoadoutId;
		NotifyLoadoutChange(LoadoutId, /*bSuccess=*/true);
	}

	LastApplicationResult = LocalResult;
	return LocalResult;
}

bool USuspenseCoreEquipmentLoadoutAdapter::SaveAsLoadout(const FName& LoadoutId)
{
	FScopeLock Lock(&AdapterCriticalSection);
	if (!bIsInitialized || !DataProvider.GetInterface())
	{
		UE_LOG(LogLoadoutAdapter, Error, TEXT("SaveAsLoadout: Not initialized"));
		return false;
	}

	const FSuspenseCoreLoadoutConfiguration NewLoadout = BuildLoadoutFromCurrentState(LoadoutId);
	UE_LOG(LogLoadoutAdapter, Log, TEXT("SaveAsLoadout: Built loadout '%s'"), *LoadoutId.ToString());
	return true;
}

bool USuspenseCoreEquipmentLoadoutAdapter::SaveAsLoadoutWithName(const FName& LoadoutId, const FText& DisplayName)
{
	FScopeLock Lock(&AdapterCriticalSection);
	if (!bIsInitialized || !DataProvider.GetInterface())
	{
		UE_LOG(LogLoadoutAdapter, Error, TEXT("SaveAsLoadoutWithName: Not initialized"));
		return false;
	}

	FSuspenseCoreLoadoutConfiguration NewLoadout = BuildLoadoutFromCurrentState(LoadoutId);
	NewLoadout.DisplayName = DisplayName;
	UE_LOG(LogLoadoutAdapter, Log, TEXT("SaveAsLoadoutWithName: Built loadout '%s' (%s)"), *LoadoutId.ToString(), *DisplayName.ToString());
	return true;
}

bool USuspenseCoreEquipmentLoadoutAdapter::ValidateLoadout(const FName& LoadoutId, TArray<FText>& OutErrors) const
{
	FScopeLock Lock(&AdapterCriticalSection);
	OutErrors.Empty();

	if (!GetLoadoutManager())
	{
		OutErrors.Add(FText::FromString(TEXT("LoadoutManager not available")));
		return false;
	}

	FSuspenseCoreLoadoutConfiguration Config;
	if (!GetLoadoutConfiguration(LoadoutId, Config))
	{
		OutErrors.Add(FText::Format(FText::FromString(TEXT("Loadout '{0}' not found")), FText::FromName(LoadoutId)));
		return false;
	}

	return ValidateLoadoutConfiguration(Config, ValidationOptions, OutErrors);
}

bool USuspenseCoreEquipmentLoadoutAdapter::ValidateLoadoutWithOptions(const FName& LoadoutId, const FSuspenseCoreLoadoutAdapterOptions& Options, TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	FScopeLock Lock(&AdapterCriticalSection);
	OutErrors.Empty();
	OutWarnings.Empty();

	if (!GetLoadoutManager())
	{
		OutErrors.Add(FText::FromString(TEXT("LoadoutManager not available")));
		return false;
	}

	FSuspenseCoreLoadoutConfiguration Config;
	if (!GetLoadoutConfiguration(LoadoutId, Config))
	{
		OutErrors.Add(FText::Format(FText::FromString(TEXT("Loadout '{0}' not found")), FText::FromName(LoadoutId)));
		return false;
	}

	// Convert adapter options to validation options
	FSuspenseCoreLoadoutValidationOptions ValidationOpts;
	ValidationOpts.bCheckCharacterClass = Options.bCheckCharacterClass;
	ValidationOpts.bCheckInventorySpace = Options.bCheckInventorySpace;
	ValidationOpts.bCheckItemAvailability = Options.bCheckItemAvailability;
	ValidationOpts.bCheckSlotCompatibility = Options.bCheckSlotCompatibility;
	ValidationOpts.bCheckWeightLimits = Options.bCheckWeightLimits;

	return ValidateLoadoutConfiguration(Config, ValidationOpts, OutErrors);
}

FName USuspenseCoreEquipmentLoadoutAdapter::GetCurrentLoadout() const
{
	FScopeLock Lock(&AdapterCriticalSection);
	return CurrentLoadoutId;
}

bool USuspenseCoreEquipmentLoadoutAdapter::GetLoadoutConfiguration(const FName& LoadoutId, FSuspenseCoreLoadoutConfiguration& OutConfiguration) const
{
	USuspenseCoreLoadoutManager* LoadoutManager = GetLoadoutManager();
	if (!LoadoutManager) { return false; }

	// Get the raw loadout config from manager
	const FLoadoutConfiguration* RawConfig = LoadoutManager->GetLoadoutConfig(LoadoutId);
	if (!RawConfig) { return false; }

	// Convert FLoadoutConfiguration to FSuspenseCoreLoadoutConfiguration
	OutConfiguration.LoadoutId = RawConfig->LoadoutID;
	OutConfiguration.DisplayName = RawConfig->LoadoutName;
	OutConfiguration.RequiredTags = RawConfig->LoadoutTags;

	// Extract first compatible class as character class
	if (!RawConfig->CompatibleClasses.IsEmpty())
	{
		TArray<FGameplayTag> ClassTags;
		RawConfig->CompatibleClasses.GetGameplayTagArray(ClassTags);
		if (ClassTags.Num() > 0)
		{
			OutConfiguration.CharacterClass = ClassTags[0];
		}
	}

	// Populate SlotTypeToItem from starting equipment if available
	for (const auto& SlotPair : RawConfig->StartingEquipment)
	{
		OutConfiguration.SlotTypeToItem.Add(SlotPair.Key, SlotPair.Value);
	}

	OutConfiguration.ModifiedTime = FDateTime::Now();
	return true;
}

TArray<FName> USuspenseCoreEquipmentLoadoutAdapter::GetAvailableLoadouts() const
{
	USuspenseCoreLoadoutManager* LoadoutManager = GetLoadoutManager();
	if (!LoadoutManager) { return TArray<FName>(); }

	return LoadoutManager->GetAllLoadoutIDs();
}

FSuspenseCoreLoadoutConfiguration USuspenseCoreEquipmentLoadoutAdapter::ConvertToLoadoutFormat(const FEquipmentStateSnapshot& State) const
{
	FSuspenseCoreLoadoutConfiguration Loadout;
	Loadout.LoadoutId = FName(FString::Printf(TEXT("Snapshot_%s"), *FGuid::NewGuid().ToString()));
	Loadout.DisplayName = FText::FromString(TEXT("Equipment Snapshot"));
	Loadout.CreatedTime = FDateTime::Now();

	for (const FEquipmentSlotSnapshot& SlotSnapshot : State.SlotSnapshots)
	{
		if (SlotSnapshot.ItemInstance.IsValid())
		{
			Loadout.SlotToItem.Add(SlotSnapshot.SlotIndex, SlotSnapshot.ItemInstance.ItemID);
			Loadout.SlotTypeToItem.Add(SlotSnapshot.Configuration.SlotType, SlotSnapshot.ItemInstance.ItemID);
		}
	}
	return Loadout;
}

TArray<FEquipmentOperationRequest> USuspenseCoreEquipmentLoadoutAdapter::ConvertFromLoadoutFormat(const FSuspenseCoreLoadoutConfiguration& Configuration) const
{
	return CreateOperationsFromLoadout(Configuration);
}

FString USuspenseCoreEquipmentLoadoutAdapter::GetLoadoutPreview(const FName& LoadoutId) const
{
	USuspenseCoreLoadoutManager* LoadoutManager = GetLoadoutManager();
	if (!LoadoutManager) { return TEXT("LoadoutManager not available"); }

	FSuspenseCoreLoadoutConfiguration Config;
	if (!GetLoadoutConfiguration(LoadoutId, Config)) { return FString::Printf(TEXT("Loadout '%s' not found"), *LoadoutId.ToString()); }

	return GenerateLoadoutPreview(Config);
}

bool USuspenseCoreEquipmentLoadoutAdapter::GetLoadoutDiff(const FName& LoadoutId, TArray<FName>& OutItemsToAdd, TArray<FName>& OutItemsToRemove) const
{
	OutItemsToAdd.Empty();
	OutItemsToRemove.Empty();

	USuspenseCoreLoadoutManager* LoadoutManager = GetLoadoutManager();
	if (!LoadoutManager || !DataProvider.GetInterface()) { return false; }

	FSuspenseCoreLoadoutConfiguration Config;
	if (!GetLoadoutConfiguration(LoadoutId, Config)) { return false; }

	// Get current equipped items
	TSet<FName> CurrentItems;
	const TMap<int32, FSuspenseCoreInventoryItemInstance> Equipped = DataProvider->GetAllEquippedItems();
	for (const auto& Pair : Equipped)
	{
		CurrentItems.Add(Pair.Value.ItemID);
	}

	// Get loadout items
	TSet<FName> LoadoutItems;
	for (const auto& Pair : Config.SlotToItem)
	{
		LoadoutItems.Add(Pair.Value);
	}

	// Items to add (in loadout but not equipped)
	for (const FName& Item : LoadoutItems)
	{
		if (!CurrentItems.Contains(Item))
		{
			OutItemsToAdd.Add(Item);
		}
	}

	// Items to remove (equipped but not in loadout)
	for (const FName& Item : CurrentItems)
	{
		if (!LoadoutItems.Contains(Item))
		{
			OutItemsToRemove.Add(Item);
		}
	}

	return true;
}

USuspenseCoreEventBus* USuspenseCoreEquipmentLoadoutAdapter::GetEventBus() const
{
	return CachedEventBus.Get();
}

void USuspenseCoreEquipmentLoadoutAdapter::SetEventBus(USuspenseCoreEventBus* InEventBus)
{
	CachedEventBus = InEventBus;
}

bool USuspenseCoreEquipmentLoadoutAdapter::IsApplyingLoadout() const
{
	return bIsApplying;
}

FSuspenseCoreLoadoutApplicationResult USuspenseCoreEquipmentLoadoutAdapter::GetLastApplicationResult() const
{
	return LastApplicationResult;
}

bool USuspenseCoreEquipmentLoadoutAdapter::CancelApplication()
{
	FScopeLock Lock(&AdapterCriticalSection);

	if (!bIsApplying) { return false; }

	if (ActiveTransactionId.IsValid() && TransactionManager.GetInterface())
	{
		TransactionManager->RollbackTransaction(ActiveTransactionId);
		ActiveTransactionId.Invalidate();
	}

	bIsApplying = false;
	return true;
}

FSuspenseCoreLoadoutApplicationResult USuspenseCoreEquipmentLoadoutAdapter::ApplyLoadoutWithStrategy(const FName& LoadoutId, ESuspenseCoreLoadoutStrategy Strategy)
{
	FScopeLock Lock(&AdapterCriticalSection);

	if (!bIsInitialized) { return FSuspenseCoreLoadoutApplicationResult::Failure(LoadoutId, FText::FromString(TEXT("Adapter not initialized"))); }
	if (bIsApplying) { return FSuspenseCoreLoadoutApplicationResult::Failure(LoadoutId, FText::FromString(TEXT("Another loadout is being applied"))); }

	USuspenseCoreLoadoutManager* LoadoutManager = GetLoadoutManager();
	if (!LoadoutManager) { return FSuspenseCoreLoadoutApplicationResult::Failure(LoadoutId, FText::FromString(TEXT("LoadoutManager not found"))); }

	FSuspenseCoreLoadoutConfiguration Config;
	if (!GetLoadoutConfiguration(LoadoutId, Config)) { return FSuspenseCoreLoadoutApplicationResult::Failure(LoadoutId, FText::FromString(TEXT("Loadout not found"))); }

	return ApplyLoadoutConfiguration(Config, Strategy);
}

void USuspenseCoreEquipmentLoadoutAdapter::SetApplicationStrategy(ESuspenseCoreLoadoutApplicationStrategy Strategy)
{
	ApplicationStrategy = Strategy;
	UE_LOG(LogLoadoutAdapter, Log, TEXT("SetApplicationStrategy: %d"), (int32)Strategy);
}

void USuspenseCoreEquipmentLoadoutAdapter::SetValidationOptions(const FSuspenseCoreLoadoutValidationOptions& Options)
{
	ValidationOptions = Options;
	UE_LOG(LogLoadoutAdapter, Log, TEXT("SetValidationOptions: updated"));
}

TArray<FName> USuspenseCoreEquipmentLoadoutAdapter::GetCompatibleLoadouts() const
{
	TArray<FName> Compatible;

	USuspenseCoreLoadoutManager* LoadoutManager = GetLoadoutManager();
	if (!LoadoutManager) { return Compatible; }

	const TArray<FName> All = LoadoutManager->GetAllLoadoutIDs();
	for (const FName& Id : All)
	{
		TArray<FText> Errors;
		if (ValidateLoadout(Id, Errors)) { Compatible.Add(Id); }
	}
	return Compatible;
}

float USuspenseCoreEquipmentLoadoutAdapter::EstimateApplicationTime(const FName& LoadoutId) const
{
	USuspenseCoreLoadoutManager* LoadoutManager = GetLoadoutManager();
	if (!LoadoutManager) { return 0.0f; }

	FSuspenseCoreLoadoutConfiguration Config;
	if (!GetLoadoutConfiguration(LoadoutId, Config)) { return 0.0f; }

	int32 OperationCount = Config.SlotToItem.Num();
	if (ApplicationStrategy == ESuspenseCoreLoadoutApplicationStrategy::Replace)
	{
		if (DataProvider.GetInterface())
		{
			OperationCount += DataProvider->GetAllEquippedItems().Num();
		}
	}
	return OperationCount * 0.1f;
}

// ==================== Internal helpers ====================

FSuspenseCoreLoadoutApplicationResult USuspenseCoreEquipmentLoadoutAdapter::ApplyLoadoutConfiguration(const FSuspenseCoreLoadoutConfiguration& Config, ESuspenseCoreLoadoutStrategy Strategy)
{
	FSuspenseCoreLoadoutApplicationResult Result;
	Result.LoadoutId = Config.LoadoutId;
	Result.StrategyUsed = Strategy;

	bool bForce = (Strategy == ESuspenseCoreLoadoutStrategy::ValidateOnly);

	if (!bForce)
	{
		TArray<FText> ValidationErrors;
		if (!ValidateLoadoutConfiguration(Config, ValidationOptions, ValidationErrors))
		{
			FString Combined;
			for (const FText& E : ValidationErrors) { Combined += E.ToString() + TEXT("\n"); }
			return FSuspenseCoreLoadoutApplicationResult::Failure(Config.LoadoutId, FText::FromString(Combined));
		}
	}

	if (Strategy == ESuspenseCoreLoadoutStrategy::ValidateOnly)
	{
		// Just validate, don't apply
		Result.bSuccess = true;
		return Result;
	}

	ActiveTransactionId = TransactionManager->BeginTransaction(FString::Printf(TEXT("ApplyLoadout_%s"), *Config.LoadoutId.ToString()));
	if (!ActiveTransactionId.IsValid())
	{
		return FSuspenseCoreLoadoutApplicationResult::Failure(Config.LoadoutId, FText::FromString(TEXT("Failed to begin transaction")));
	}

	if (Strategy == ESuspenseCoreLoadoutStrategy::Replace)
	{
		if (!ClearCurrentEquipment())
		{
			TransactionManager->RollbackTransaction(ActiveTransactionId);
			ActiveTransactionId.Invalidate();
			return FSuspenseCoreLoadoutApplicationResult::Failure(Config.LoadoutId, FText::FromString(TEXT("Failed to clear current equipment")));
		}
	}

	// Convert SlotTypeToItem to equipment map
	TMap<ESuspenseCoreEquipmentSlotType, FName> StartingEquipment;
	for (const auto& Pair : Config.SlotTypeToItem)
	{
		StartingEquipment.Add(Pair.Key, Pair.Value);
	}

	const int32 EquippedCount = ApplyStartingEquipment(StartingEquipment);
	Result.ItemsEquipped = EquippedCount;
	Result.ItemsFailed = StartingEquipment.Num() - EquippedCount;

	if (!TransactionManager->CommitTransaction(ActiveTransactionId))
	{
		TransactionManager->RollbackTransaction(ActiveTransactionId);
		ActiveTransactionId.Invalidate();
		return FSuspenseCoreLoadoutApplicationResult::Failure(Config.LoadoutId, FText::FromString(TEXT("Failed to commit transaction")));
	}

	ActiveTransactionId.Invalidate();
	Result.bSuccess = true;

	UE_LOG(LogLoadoutAdapter, Log, TEXT("ApplyLoadoutConfiguration: Applied '%s'"), *Config.LoadoutId.ToString());
	return Result;
}

TArray<FEquipmentOperationRequest> USuspenseCoreEquipmentLoadoutAdapter::CreateOperationsFromLoadout(const FSuspenseCoreLoadoutConfiguration& Config) const
{
	TArray<FEquipmentOperationRequest> Ops;

	// Build operations from SlotToItem map
	for (const auto& Pair : Config.SlotToItem)
	{
		const int32 SlotIndex = Pair.Key;
		const FName& ItemId = Pair.Value;

		if (!ItemId.IsNone())
		{
			FSuspenseCoreEquipmentSlotConfig SlotConfig;
			// Get slot config from DataProvider if available
			if (DataProvider.GetInterface())
			{
				const TArray<FSuspenseCoreEquipmentSlotConfig> AllSlots = DataProvider->GetAllSlotConfigurations();
				if (AllSlots.IsValidIndex(SlotIndex))
				{
					SlotConfig = AllSlots[SlotIndex];
				}
			}
			Ops.Add(CreateEquipOperation(SlotConfig, ItemId, SlotIndex));
		}
	}
	return Ops;
}

FEquipmentOperationRequest USuspenseCoreEquipmentLoadoutAdapter::CreateEquipOperation(const FSuspenseCoreEquipmentSlotConfig& SlotConfig, const FName& ItemId, int32 SlotIndex) const
{
	FEquipmentOperationRequest Req;
	Req.OperationType   = EEquipmentOperationType::Equip;
	Req.TargetSlotIndex = SlotIndex;
	Req.OperationId     = FGuid::NewGuid();
	Req.Timestamp       = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	// создаём валидный инстанс (FSuspenseCoreItemInstance для FEquipmentOperationRequest)
	FSuspenseCoreItemInstance ItemInstance;
	ItemInstance.ItemID   = ItemId;
	ItemInstance.Quantity = 1;
	Req.ItemInstance      = ItemInstance;

	const UEnum* EnumPtr     = StaticEnum<ESuspenseCoreEquipmentSlotType>();
	const FString SlotTypeNm = EnumPtr ? EnumPtr->GetNameStringByValue((int64)SlotConfig.SlotType) : TEXT("Unknown");

	Req.Parameters.Add(TEXT("SlotType"), SlotTypeNm);
	Req.Parameters.Add(TEXT("SlotTag"),  SlotConfig.SlotTag.ToString());
	// Поля Reason в FEquipmentOperationRequest нет — не трогаем.

	return Req;
}

bool USuspenseCoreEquipmentLoadoutAdapter::ValidateLoadoutConfiguration(const FSuspenseCoreLoadoutConfiguration& Config, const FSuspenseCoreLoadoutValidationOptions& Options, TArray<FText>& OutErrors) const
{
	bool bValid = true;

	if (Options.bCheckSlotCompatibility)
	{
		for (const auto& Pair : Config.SlotTypeToItem)
		{
			const ESuspenseCoreEquipmentSlotType SlotType = Pair.Key;
			const FName ItemId = Pair.Value;

			// Get slot config from DataProvider
			FSuspenseCoreEquipmentSlotConfig SlotConfig;
			bool bFoundSlot = false;
			if (DataProvider.GetInterface())
			{
				for (const FSuspenseCoreEquipmentSlotConfig& Slot : DataProvider->GetAllSlotConfigurations())
				{
					if (Slot.SlotType == SlotType) { SlotConfig = Slot; bFoundSlot = true; break; }
				}
			}

			if (bFoundSlot && !CheckSlotCompatibility(SlotConfig, ItemId))
			{
				OutErrors.Add(FText::Format(FText::FromString(TEXT("Item '{0}' not compatible with slot '{1}'")),
				                            FText::FromName(ItemId), SlotConfig.DisplayName));
				bValid = false;
			}
		}
	}

	if (Options.bCheckInventorySpace && !CheckInventorySpace(Config))
	{
		OutErrors.Add(FText::FromString(TEXT("Insufficient inventory space for loadout items")));
		bValid = false;
	}

	if (Options.bCheckItemAvailability)
	{
		for (const auto& Pair : Config.SlotTypeToItem)
		{
			if (!CheckItemAvailability(Pair.Value))
			{
				OutErrors.Add(FText::Format(FText::FromString(TEXT("Item '{0}' not available")), FText::FromName(Pair.Value)));
				bValid = false;
			}
		}
	}

	if (Options.bCheckWeightLimits)
	{
		float TotalWeight = 0.0f;
		if (USuspenseCoreItemManager* ItemManager = GetItemManager())
		{
			for (const auto& Pair : Config.SlotTypeToItem)
			{
				FSuspenseCoreUnifiedItemData Data;
				if (ItemManager->GetUnifiedItemData(Pair.Value, Data))
				{
					TotalWeight += Data.Weight;
				}
			}
		}
		// Note: FSuspenseCoreLoadoutConfiguration doesn't have MaxTotalWeight - skip weight check
	}

	return bValid;
}

bool USuspenseCoreEquipmentLoadoutAdapter::CheckSlotCompatibility(const FSuspenseCoreEquipmentSlotConfig& SlotConfig, const FName& ItemId) const
{
	USuspenseCoreItemManager* ItemManager = GetItemManager();
	if (!ItemManager) { return false; }

	FSuspenseCoreUnifiedItemData ItemData;
	if (!ItemManager->GetUnifiedItemData(ItemId, ItemData)) { return false; }

	return SlotConfig.CanEquipItemType(ItemData.ItemType);
}

bool USuspenseCoreEquipmentLoadoutAdapter::CheckInventorySpace(const FSuspenseCoreLoadoutConfiguration& Config) const
{
	// Legacy InventoryBridge removed - assume space is available
	return true;
}

bool USuspenseCoreEquipmentLoadoutAdapter::CheckItemAvailability(const FName& ItemId) const
{
	USuspenseCoreItemManager* ItemManager = GetItemManager();
	if (!ItemManager) { return false; }
	return ItemManager->HasItem(ItemId);
}

FSuspenseCoreLoadoutConfiguration USuspenseCoreEquipmentLoadoutAdapter::BuildLoadoutFromCurrentState(const FName& LoadoutId) const
{
	FSuspenseCoreLoadoutConfiguration Loadout;
	Loadout.LoadoutId = LoadoutId;
	Loadout.DisplayName = FText::FromName(LoadoutId);
	Loadout.CreatedTime = FDateTime::Now();
	Loadout.ModifiedTime = FDateTime::Now();

	if (!DataProvider.GetInterface()) { return Loadout; }

	const TArray<FSuspenseCoreEquipmentSlotConfig> AllSlots = DataProvider->GetAllSlotConfigurations();
	const TMap<int32, FSuspenseCoreInventoryItemInstance> Equipped = DataProvider->GetAllEquippedItems();
	for (const auto& Pair : Equipped)
	{
		const int32 SlotIndex = Pair.Key;
		const FSuspenseCoreInventoryItemInstance& Instance = Pair.Value;

		Loadout.SlotToItem.Add(SlotIndex, Instance.ItemID);

		if (AllSlots.IsValidIndex(SlotIndex))
		{
			const FSuspenseCoreEquipmentSlotConfig& SlotCfg = AllSlots[SlotIndex];
			Loadout.SlotTypeToItem.Add(SlotCfg.SlotType, Instance.ItemID);
		}
	}
	return Loadout;
}

FString USuspenseCoreEquipmentLoadoutAdapter::GenerateLoadoutPreview(const FSuspenseCoreLoadoutConfiguration& Config) const
{
	FString S;
	S += FString::Printf(TEXT("Loadout: %s\n"), *Config.DisplayName.ToString());
	S += FString::Printf(TEXT("ID: %s\n"), *Config.LoadoutId.ToString());
	S += FString::Printf(TEXT("Slot Items: %d\n"), Config.SlotToItem.Num());
	S += TEXT("\nEquipment:\n");

	for (const auto& Pair : Config.SlotTypeToItem)
	{
		const FString SlotName = StaticEnum<ESuspenseCoreEquipmentSlotType>()->GetNameStringByValue((int64)Pair.Key);
		S += FString::Printf(TEXT("  %s: %s\n"), *SlotName, *Pair.Value.ToString());
	}
	return S;
}

bool USuspenseCoreEquipmentLoadoutAdapter::ClearCurrentEquipment()
{
	if (!OperationsExecutor.GetInterface() || !DataProvider.GetInterface()) { return false; }

	const TMap<int32, FSuspenseCoreInventoryItemInstance> Equipped = DataProvider->GetAllEquippedItems();
	for (const auto& Pair : Equipped)
	{
		const FEquipmentOperationResult Res = OperationsExecutor->UnequipItem(Pair.Key);
		if (!Res.bSuccess)
		{
			UE_LOG(LogLoadoutAdapter, Warning, TEXT("ClearCurrentEquipment: Failed to unequip slot %d"), Pair.Key);
			return false;
		}
	}
	return true;
}

int32 USuspenseCoreEquipmentLoadoutAdapter::ApplyStartingEquipment(const TMap<ESuspenseCoreEquipmentSlotType, FName>& StartingEquipment)
{
	// Предпочитаем централизованный батч через OperationService
	if (bPreferOperationService)
	{
		if (USuspenseCoreEquipmentOperationService* OpSvc = GetOperationService())
		{
			TArray<FEquipmentOperationRequest> Requests;
			Requests.Reserve(StartingEquipment.Num());

			// Подготовим сопоставление SlotType -> SlotIndex из текущих конфигураций
			TMap<ESuspenseCoreEquipmentSlotType, int32> SlotTypeToIndex;
			if (DataProvider.GetInterface())
			{
				const TArray<FSuspenseCoreEquipmentSlotConfig> Slots = DataProvider->GetAllSlotConfigurations();
				for (int32 i = 0; i < Slots.Num(); ++i)
				{
					SlotTypeToIndex.Add(Slots[i].SlotType, i);
				}
			}

			for (const auto& Pair : StartingEquipment)
			{
				if (Pair.Value.IsNone()) { continue; }

				const int32* SlotIndexPtr = SlotTypeToIndex.Find(Pair.Key);
				if (!SlotIndexPtr) { continue; }

				FEquipmentOperationRequest Req;
				Req.OperationType   = EEquipmentOperationType::Equip;
				Req.TargetSlotIndex = *SlotIndexPtr;

				FSuspenseCoreItemInstance ItemInstance;
				ItemInstance.ItemID   = Pair.Value;
				ItemInstance.Quantity = 1;
				Req.ItemInstance      = ItemInstance;

				Req.Parameters.Add(TEXT("SlotType"), StaticEnum<ESuspenseCoreEquipmentSlotType>()->GetNameStringByValue((int64)Pair.Key));
				Requests.Add(Req);
			}

			TArray<FEquipmentOperationResult> Results;
			OpSvc->BatchOperationsEx(Requests, /*bAtomic=*/true, Results);

			int32 Success = 0;
			for (const auto& R : Results) { if (R.bSuccess) ++Success; }
			return Success;
		}
	}

	// Фолбэк — прямые вызовы
	if (!OperationsExecutor.GetInterface()) { return 0; }

	int32 Success = 0;

	// Карта SlotType -> Index на базе текущей конфигурации
	TMap<ESuspenseCoreEquipmentSlotType, int32> SlotTypeToIndex;
	if (DataProvider.GetInterface())
	{
		const TArray<FSuspenseCoreEquipmentSlotConfig> Slots = DataProvider->GetAllSlotConfigurations();
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			SlotTypeToIndex.Add(Slots[i].SlotType, i);
		}
	}

	for (const auto& Pair : StartingEquipment)
	{
		if (Pair.Value.IsNone()) { continue; }

		FSuspenseCoreInventoryItemInstance ItemInstance;
		if (USuspenseCoreItemManager* ItemManager = GetItemManager())
		{
			if (!ItemManager->CreateItemInstance(Pair.Value, 1, ItemInstance)) { continue; }
		}
		else
		{
			// На случай отсутствия менеджера — минимально валидная структура
			ItemInstance.ItemID   = Pair.Value;
			ItemInstance.Quantity = 1;
		}

		const int32* SlotIndexPtr = SlotTypeToIndex.Find(Pair.Key);
		if (!SlotIndexPtr) { continue; }

		const FEquipmentOperationResult Res = OperationsExecutor->EquipItem(ItemInstance, *SlotIndexPtr);
		if (Res.bSuccess) { ++Success; }
		else
		{
			UE_LOG(LogLoadoutAdapter, Warning, TEXT("ApplyStartingEquipment: Failed to equip %s"), *Pair.Value.ToString());
		}
	}
	return Success;
}

void USuspenseCoreEquipmentLoadoutAdapter::NotifyLoadoutChange(const FName& LoadoutId, bool bSuccess)
{
	// Legacy EventDispatcher removed - logging only
	UE_LOG(LogLoadoutAdapter, Log, TEXT("LoadoutChange: %s, Success=%s"),
		*LoadoutId.ToString(), bSuccess ? TEXT("true") : TEXT("false"));
}

void USuspenseCoreEquipmentLoadoutAdapter::LogAdapterState() const
{
	UE_LOG(LogLoadoutAdapter, Verbose, TEXT("=== LoadoutAdapter State ==="));
	UE_LOG(LogLoadoutAdapter, Verbose, TEXT("Initialized: %s"), bIsInitialized ? TEXT("Yes") : TEXT("No"));
	UE_LOG(LogLoadoutAdapter, Verbose, TEXT("Current Loadout: %s"), *CurrentLoadoutId.ToString());
	UE_LOG(LogLoadoutAdapter, Verbose, TEXT("Application Strategy: %d"), (int32)ApplicationStrategy);
	UE_LOG(LogLoadoutAdapter, Verbose, TEXT("Is Applying: %s"), bIsApplying ? TEXT("Yes") : TEXT("No"));
	UE_LOG(LogLoadoutAdapter, Verbose, TEXT("Active Transaction: %s"), ActiveTransactionId.IsValid() ? *ActiveTransactionId.ToString() : TEXT("None"));
}

USuspenseCoreLoadoutManager* USuspenseCoreEquipmentLoadoutAdapter::GetLoadoutManager() const
{
	if (!CachedLoadoutManager.IsValid() || (GetWorld() && GetWorld()->GetTimeSeconds() - LastCacheTime > CacheLifetime))
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				CachedLoadoutManager = GI->GetSubsystem<USuspenseCoreLoadoutManager>();
				LastCacheTime        = World->GetTimeSeconds();
			}
		}
	}
	return CachedLoadoutManager.Get();
}

USuspenseCoreItemManager* USuspenseCoreEquipmentLoadoutAdapter::GetItemManager() const
{
	if (!CachedItemManager.IsValid() || (GetWorld() && GetWorld()->GetTimeSeconds() - LastCacheTime > CacheLifetime))
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				CachedItemManager = GI->GetSubsystem<USuspenseCoreItemManager>();
				LastCacheTime     = World->GetTimeSeconds();
			}
		}
	}
	return CachedItemManager.Get();
}
