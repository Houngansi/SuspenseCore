// Copyright Suspense Team. All Rights Reserved.
#include "SuspenseCore/Components/Integration/SuspenseCoreEquipmentLoadoutAdapter.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"
#include "GameplayTagContainer.h"

#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentOperations.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreTransactionManager.h"
#include "SuspenseCore/Interfaces/Inventory/ISuspenseCoreInventoryBridge.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEventDispatcher.h"
#include "SuspenseCore/Interfaces/Core/ISuspenseCoreLoadout.h"

#include "Types/Loadout/SuspenseCoreLoadoutManager.h"
#include "ItemSystem/SuspenseCoreItemManager.h"
#include "Types/Events/SuspenseCoreEquipmentEventData.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "Types/Equipment/SuspenseCoreEquipmentTypes.h"

#include "SuspenseCore/Services/SuspenseCoreEquipmentOperationService.h"
#include "Core/Services/SuspenseCoreEquipmentServiceLocator.h"

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
	InventoryBridge     = nullptr;
	EventDispatcher     = nullptr;

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

FLoadoutApplicationResult USuspenseCoreEquipmentLoadoutAdapter::ApplyLoadout(const FName& LoadoutId, bool bForce)
{
	FScopeLock Lock(&AdapterCriticalSection);

	if (!bIsInitialized) { return FLoadoutApplicationResult::CreateFailure(LoadoutId, TEXT("Adapter not initialized")); }
	if (bIsApplying)      { return FLoadoutApplicationResult::CreateFailure(LoadoutId, TEXT("Another loadout is being applied")); }

	const USuspenseCoreLoadoutManager* Manager = GetLoadoutManager();
	if (!Manager) { return FLoadoutApplicationResult::CreateFailure(LoadoutId, TEXT("LoadoutManager not found")); }

	const FLoadoutConfiguration* Config = Manager->GetLoadoutConfig(LoadoutId);
	if (!Config) { return FLoadoutApplicationResult::CreateFailure(LoadoutId, TEXT("Loadout not found")); }

	// ——— optional centralized path via OperationService (S8 pipeline) ———
	if (bPreferOperationService)
	{
		if (USuspenseCoreEquipmentOperationService* OpSvc = GetOperationService())
		{
			// Preflight validation (optional, unless bForce)
			if (!bForce)
			{
				TArray<FText> ValidationErrors;
				if (!ValidateLoadoutConfiguration(*Config, ValidationOptions, ValidationErrors))
				{
					FString Combined;
					for (const FText& E : ValidationErrors) { Combined += E.ToString() + TEXT("\n"); }
					return FLoadoutApplicationResult::CreateFailure(LoadoutId, Combined);
				}
			}

			// Build & execute a batch of operations atomically
			const TArray<FEquipmentOperationRequest> Requests = CreateOperationsFromLoadout(*Config);

			TArray<FEquipmentOperationResult> Results;
			const FGuid BatchId = OpSvc->BatchOperationsEx(Requests, /*bAtomic=*/true, Results);

			int32  SuccessCount = 0;
			FString FirstError;
			for (const FEquipmentOperationResult& R : Results)
			{
				if (R.bSuccess) { ++SuccessCount; }
				else if (FirstError.IsEmpty()) { FirstError = R.ErrorMessage.ToString(); } // FText -> FString
			}

			FLoadoutApplicationResult Result;
			Result.AppliedLoadoutID = Config->LoadoutID;
			Result.ApplicationTime  = FDateTime::Now();
			Result.bSuccess         = (SuccessCount == Requests.Num());
			if (!Result.bSuccess && !FirstError.IsEmpty())
			{
				Result.ErrorMessages.Add(FirstError); // без несуществующего AddError
			}

			// Event: Loadout applied (EventBus)
			if (EventDispatcher.GetInterface())
			{
				FSuspenseCoreEquipmentEventData Ev;
				Ev.EventType  = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Loadout.Applied"));
				Ev.Source     = this;
				Ev.Payload    = LoadoutId.ToString();
				Ev.Timestamp  = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
				Ev.Metadata.Add(TEXT("BatchId"),       BatchId.ToString());
				Ev.Metadata.Add(TEXT("SuccessCount"),  LexToString(SuccessCount));
				Ev.Metadata.Add(TEXT("Total"),         LexToString(Requests.Num()));
				EventDispatcher->BroadcastEvent(Ev);
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
	if (EventDispatcher.GetInterface())
	{
		FSuspenseCoreEquipmentEventData Event;
		Event.EventType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Loadout.Start"));
		Event.Source    = this;
		Event.Payload   = LoadoutId.ToString();
		Event.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		EventDispatcher->BroadcastEvent(Event);
	}

	bIsApplying = true;
	ON_SCOPE_EXIT { bIsApplying = false; };

	FLoadoutApplicationResult LocalResult = ApplyLoadoutConfiguration(*Config, bForce);

	if (EventDispatcher.GetInterface())
	{
		FSuspenseCoreEquipmentEventData Event;
		Event.EventType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Loadout.End"));
		Event.Source    = this;
		Event.Payload   = LoadoutId.ToString();
		Event.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		Event.Metadata.Add(TEXT("Success"), LocalResult.bSuccess ? TEXT("true") : TEXT("false"));
		EventDispatcher->BroadcastEvent(Event);
	}

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

	const FLoadoutConfiguration NewLoadout = BuildLoadoutFromCurrentState(LoadoutId);
	UE_LOG(LogLoadoutAdapter, Log, TEXT("SaveAsLoadout: Built loadout '%s'"), *LoadoutId.ToString());
	return true;
}

bool USuspenseCoreEquipmentLoadoutAdapter::ValidateLoadout(const FName& LoadoutId, TArray<FText>& OutErrors) const
{
	FScopeLock Lock(&AdapterCriticalSection);
	OutErrors.Empty();

	USuspenseCoreLoadoutManager* LoadoutManager = GetLoadoutManager();
	if (!LoadoutManager)
	{
		OutErrors.Add(FText::FromString(TEXT("LoadoutManager not available")));
		return false;
	}

	const FLoadoutConfiguration* Config = LoadoutManager->GetLoadoutConfig(LoadoutId);
	if (!Config)
	{
		OutErrors.Add(FText::Format(FText::FromString(TEXT("Loadout '{0}' not found")), FText::FromName(LoadoutId)));
		return false;
	}

	return ValidateLoadoutConfiguration(*Config, ValidationOptions, OutErrors);
}

FName USuspenseCoreEquipmentLoadoutAdapter::GetCurrentLoadout() const
{
	FScopeLock Lock(&AdapterCriticalSection);
	return CurrentLoadoutId;
}

FLoadoutConfiguration USuspenseCoreEquipmentLoadoutAdapter::ConvertToLoadoutFormat(const FEquipmentStateSnapshot& State) const
{
	FLoadoutConfiguration Loadout;
	Loadout.LoadoutID   = FName(FString::Printf(TEXT("Snapshot_%s"), *FGuid::NewGuid().ToString()));
	Loadout.LoadoutName = FText::FromString(TEXT("Equipment Snapshot"));
	Loadout.Description = FText::Format(FText::FromString(TEXT("Snapshot taken at {0}")),
	                                    FText::FromString(State.Timestamp.ToString()));

	for (const FEquipmentSlotSnapshot& SlotSnapshot : State.SlotSnapshots)
	{
		Loadout.EquipmentSlots.Add(SlotSnapshot.Configuration);
		if (SlotSnapshot.ItemInstance.IsValid())
		{
			Loadout.StartingEquipment.Add(SlotSnapshot.Configuration.SlotType, SlotSnapshot.ItemInstance.ItemID);
		}
	}
	return Loadout;
}

TArray<FEquipmentOperationRequest> USuspenseCoreEquipmentLoadoutAdapter::ConvertFromLoadoutFormat(const FLoadoutConfiguration& Loadout) const
{
	return CreateOperationsFromLoadout(Loadout);
}

FString USuspenseCoreEquipmentLoadoutAdapter::GetLoadoutPreview(const FName& LoadoutId) const
{
	USuspenseCoreLoadoutManager* LoadoutManager = GetLoadoutManager();
	if (!LoadoutManager) { return TEXT("LoadoutManager not available"); }

	const FLoadoutConfiguration* Config = LoadoutManager->GetLoadoutConfig(LoadoutId);
	if (!Config) { return FString::Printf(TEXT("Loadout '%s' not found"), *LoadoutId.ToString()); }

	return GenerateLoadoutPreview(*Config);
}

void USuspenseCoreEquipmentLoadoutAdapter::SetApplicationStrategy(ELoadoutApplicationStrategy Strategy)
{
	ApplicationStrategy = Strategy;
	UE_LOG(LogLoadoutAdapter, Log, TEXT("SetApplicationStrategy: %d"), (int32)Strategy);
}

void USuspenseCoreEquipmentLoadoutAdapter::SetValidationOptions(const FLoadoutValidationOptions& Options)
{
	ValidationOptions = Options;
	UE_LOG(LogLoadoutAdapter, Log, TEXT("SetValidationOptions: updated"));
}

void USuspenseCoreEquipmentLoadoutAdapter::SetInventoryBridge(TScriptInterface<ISuspenseCoreInventoryBridge> Bridge)
{
	InventoryBridge = Bridge;
	UE_LOG(LogLoadoutAdapter, Log, TEXT("SetInventoryBridge: %s"), Bridge.GetInterface() ? TEXT("set") : TEXT("cleared"));
}

void USuspenseCoreEquipmentLoadoutAdapter::SetEventDispatcher(TScriptInterface<ISuspenseCoreEventDispatcher> Dispatcher)
{
	EventDispatcher = Dispatcher;
	UE_LOG(LogLoadoutAdapter, Log, TEXT("SetEventDispatcher: %s"), Dispatcher.GetInterface() ? TEXT("set") : TEXT("cleared"));
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

	const FLoadoutConfiguration* Config = LoadoutManager->GetLoadoutConfig(LoadoutId);
	if (!Config) { return 0.0f; }

	int32 OperationCount = Config->StartingEquipment.Num();
	if (ApplicationStrategy == ELoadoutApplicationStrategy::Replace)
	{
		if (DataProvider.GetInterface())
		{
			OperationCount += DataProvider->GetAllEquippedItems().Num();
		}
	}
	return OperationCount * 0.1f;
}

// ==================== Internal helpers ====================

FLoadoutApplicationResult USuspenseCoreEquipmentLoadoutAdapter::ApplyLoadoutConfiguration(const FLoadoutConfiguration& Config, bool bForce)
{
	FLoadoutApplicationResult Result;
	Result.AppliedLoadoutID = Config.LoadoutID;
	Result.ApplicationTime  = FDateTime::Now();

	if (!bForce)
	{
		TArray<FText> ValidationErrors;
		if (!ValidateLoadoutConfiguration(Config, ValidationOptions, ValidationErrors))
		{
			FString Combined;
			for (const FText& E : ValidationErrors) { Combined += E.ToString() + TEXT("\n"); }
			return FLoadoutApplicationResult::CreateFailure(Config.LoadoutID, Combined);
		}
	}

	ActiveTransactionId = TransactionManager->BeginTransaction(FString::Printf(TEXT("ApplyLoadout_%s"), *Config.LoadoutID.ToString()));
	if (!ActiveTransactionId.IsValid())
	{
		return FLoadoutApplicationResult::CreateFailure(Config.LoadoutID, TEXT("Failed to begin transaction"));
	}

	if (ApplicationStrategy == ELoadoutApplicationStrategy::Replace)
	{
		if (!ClearCurrentEquipment())
		{
			TransactionManager->RollbackTransaction(ActiveTransactionId);
			ActiveTransactionId.Invalidate();
			return FLoadoutApplicationResult::CreateFailure(Config.LoadoutID, TEXT("Failed to clear current equipment"));
		}
	}

	if (DataProvider.GetInterface())
	{
		if (!DataProvider->InitializeSlots(Config.EquipmentSlots))
		{
			TransactionManager->RollbackTransaction(ActiveTransactionId);
			ActiveTransactionId.Invalidate();
			return FLoadoutApplicationResult::CreateFailure(Config.LoadoutID, TEXT("Failed to initialize equipment slots"));
		}
		Result.MergeComponentResult(FGameplayTag::RequestGameplayTag(TEXT("Loadout.Component.Equipment.Slots")),
		                            true,
		                            FString::Printf(TEXT("Initialized %d slots"), Config.EquipmentSlots.Num()));
	}

	const int32 EquippedCount = ApplyStartingEquipment(Config.StartingEquipment);
	Result.MergeComponentResult(FGameplayTag::RequestGameplayTag(TEXT("Loadout.Component.Equipment.Items")),
	                            EquippedCount > 0,
	                            FString::Printf(TEXT("Equipped %d/%d items"), EquippedCount, Config.StartingEquipment.Num()));

	if (!TransactionManager->CommitTransaction(ActiveTransactionId))
	{
		TransactionManager->RollbackTransaction(ActiveTransactionId);
		ActiveTransactionId.Invalidate();
		return FLoadoutApplicationResult::CreateFailure(Config.LoadoutID, TEXT("Failed to commit transaction"));
	}

	ActiveTransactionId.Invalidate();
	Result.bSuccess = true;

	UE_LOG(LogLoadoutAdapter, Log, TEXT("ApplyLoadoutConfiguration: Applied '%s'"), *Config.LoadoutID.ToString());
	return Result;
}

TArray<FEquipmentOperationRequest> USuspenseCoreEquipmentLoadoutAdapter::CreateOperationsFromLoadout(const FLoadoutConfiguration& Config) const
{
	TArray<FEquipmentOperationRequest> Ops;

	int32 SlotIndex = 0;
	for (const FEquipmentSlotConfig& SlotConfig : Config.EquipmentSlots)
	{
		if (const FName* ItemId = Config.StartingEquipment.Find(SlotConfig.SlotType))
		{
			if (!ItemId->IsNone())
			{
				Ops.Add(CreateEquipOperation(SlotConfig, *ItemId, SlotIndex));
			}
		}
		++SlotIndex;
	}
	return Ops;
}

FEquipmentOperationRequest USuspenseCoreEquipmentLoadoutAdapter::CreateEquipOperation(const FEquipmentSlotConfig& SlotConfig, const FName& ItemId, int32 SlotIndex) const
{
	FEquipmentOperationRequest Req;
	Req.OperationType   = EEquipmentOperationType::Equip;
	Req.TargetSlotIndex = SlotIndex;
	Req.OperationId     = FGuid::NewGuid();
	Req.Timestamp       = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	// создаём валидный инстанс
	FSuspenseCoreInventoryItemInstance ItemInstance;
	ItemInstance.ItemID   = ItemId;
	ItemInstance.Quantity = 1;
	Req.ItemInstance      = ItemInstance;

	const UEnum* EnumPtr     = StaticEnum<EEquipmentSlotType>();
	const FString SlotTypeNm = EnumPtr ? EnumPtr->GetNameStringByValue((int64)SlotConfig.SlotType) : TEXT("Unknown");

	Req.Parameters.Add(TEXT("SlotType"), SlotTypeNm);
	Req.Parameters.Add(TEXT("SlotTag"),  SlotConfig.SlotTag.ToString());
	// Поля Reason в FEquipmentOperationRequest нет — не трогаем.

	return Req;
}

bool USuspenseCoreEquipmentLoadoutAdapter::ValidateLoadoutConfiguration(const FLoadoutConfiguration& Config, const FLoadoutValidationOptions& Options, TArray<FText>& OutErrors) const
{
	bool bValid = true;

	if (Options.bCheckSlotCompatibility)
	{
		for (const auto& Pair : Config.StartingEquipment)
		{
			const EEquipmentSlotType SlotType = Pair.Key;
			const FName              ItemId   = Pair.Value;

			const FEquipmentSlotConfig* SlotConfig = nullptr;
			for (const FEquipmentSlotConfig& Slot : Config.EquipmentSlots)
			{
				if (Slot.SlotType == SlotType) { SlotConfig = &Slot; break; }
			}

			if (SlotConfig && !CheckSlotCompatibility(*SlotConfig, ItemId))
			{
				OutErrors.Add(FText::Format(FText::FromString(TEXT("Item '{0}' not compatible with slot '{1}'")),
				                            FText::FromName(ItemId), SlotConfig->DisplayName));
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
		for (const auto& Pair : Config.StartingEquipment)
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
			for (const auto& Pair : Config.StartingEquipment)
			{
				FSuspenseCoreUnifiedItemData Data;
				if (ItemManager->GetUnifiedItemData(Pair.Value, Data))
				{
					TotalWeight += Data.Weight;
				}
			}
		}
		if (TotalWeight > Config.MaxTotalWeight)
		{
			OutErrors.Add(FText::Format(FText::FromString(TEXT("Total weight ({0} kg) exceeds limit ({1} kg)")),
			                            FText::AsNumber(TotalWeight), FText::AsNumber(Config.MaxTotalWeight)));
			bValid = false;
		}
	}

	return bValid;
}

bool USuspenseCoreEquipmentLoadoutAdapter::CheckSlotCompatibility(const FEquipmentSlotConfig& SlotConfig, const FName& ItemId) const
{
	USuspenseCoreItemManager* ItemManager = GetItemManager();
	if (!ItemManager) { return false; }

	FSuspenseCoreUnifiedItemData ItemData;
	if (!ItemManager->GetUnifiedItemData(ItemId, ItemData)) { return false; }

	return SlotConfig.CanEquipItemType(ItemData.ItemType);
}

bool USuspenseCoreEquipmentLoadoutAdapter::CheckInventorySpace(const FLoadoutConfiguration& Config) const
{
	if (!InventoryBridge.GetInterface()) { return true; }

	for (const auto& Pair : Config.StartingEquipment)
	{
		FSuspenseCoreInventoryItemInstance Tmp;
		Tmp.ItemID   = Pair.Value;
		Tmp.Quantity = 1;

		if (!InventoryBridge->InventoryHasSpace(Tmp)) { return false; }
	}
	return true;
}

bool USuspenseCoreEquipmentLoadoutAdapter::CheckItemAvailability(const FName& ItemId) const
{
	USuspenseCoreItemManager* ItemManager = GetItemManager();
	if (!ItemManager) { return false; }
	return ItemManager->HasItem(ItemId);
}

FLoadoutConfiguration USuspenseCoreEquipmentLoadoutAdapter::BuildLoadoutFromCurrentState(const FName& LoadoutId) const
{
	FLoadoutConfiguration Loadout;
	Loadout.LoadoutID   = LoadoutId;
	Loadout.LoadoutName = FText::FromName(LoadoutId);

	if (!DataProvider.GetInterface()) { return Loadout; }

	Loadout.EquipmentSlots = DataProvider->GetAllSlotConfigurations();

	const TMap<int32, FSuspenseCoreInventoryItemInstance> Equipped = DataProvider->GetAllEquippedItems();
	for (const auto& Pair : Equipped)
	{
		const int32 SlotIndex = Pair.Key;
		const FSuspenseCoreInventoryItemInstance& Instance = Pair.Value;

		if (Loadout.EquipmentSlots.IsValidIndex(SlotIndex))
		{
			const FEquipmentSlotConfig& SlotCfg = Loadout.EquipmentSlots[SlotIndex];
			Loadout.StartingEquipment.Add(SlotCfg.SlotType, Instance.ItemID);
		}
	}
	return Loadout;
}

FString USuspenseCoreEquipmentLoadoutAdapter::GenerateLoadoutPreview(const FLoadoutConfiguration& Config) const
{
	FString S;
	S += FString::Printf(TEXT("Loadout: %s\n"), *Config.LoadoutName.ToString());
	S += FString::Printf(TEXT("Description: %s\n"), *Config.Description.ToString());
	S += FString::Printf(TEXT("Equipment Slots: %d\n"),  Config.EquipmentSlots.Num());
	S += FString::Printf(TEXT("Starting Items: %d\n"),   Config.StartingEquipment.Num());
	S += TEXT("\nEquipment:\n");

	for (const auto& Pair : Config.StartingEquipment)
	{
		const FString SlotName = StaticEnum<EEquipmentSlotType>()->GetNameStringByValue((int64)Pair.Key);
		S += FString::Printf(TEXT("  %s: %s\n"), *SlotName, *Pair.Value.ToString());
	}
	S += FString::Printf(TEXT("\nMax Weight: %.1f kg\n"), Config.MaxTotalWeight);
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

int32 USuspenseCoreEquipmentLoadoutAdapter::ApplyStartingEquipment(const TMap<EEquipmentSlotType, FName>& StartingEquipment)
{
	// Предпочитаем централизованный батч через OperationService
	if (bPreferOperationService)
	{
		if (USuspenseCoreEquipmentOperationService* OpSvc = GetOperationService())
		{
			TArray<FEquipmentOperationRequest> Requests;
			Requests.Reserve(StartingEquipment.Num());

			// Подготовим сопоставление SlotType -> SlotIndex из текущих конфигураций
			TMap<EEquipmentSlotType, int32> SlotTypeToIndex;
			if (DataProvider.GetInterface())
			{
				const TArray<FEquipmentSlotConfig> Slots = DataProvider->GetAllSlotConfigurations();
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

				FSuspenseCoreInventoryItemInstance ItemInstance;
				ItemInstance.ItemID   = Pair.Value;
				ItemInstance.Quantity = 1;
				Req.ItemInstance      = ItemInstance;

				Req.Parameters.Add(TEXT("SlotType"), StaticEnum<EEquipmentSlotType>()->GetNameStringByValue((int64)Pair.Key));
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
	TMap<EEquipmentSlotType, int32> SlotTypeToIndex;
	if (DataProvider.GetInterface())
	{
		const TArray<FEquipmentSlotConfig> Slots = DataProvider->GetAllSlotConfigurations();
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
	if (!EventDispatcher.GetInterface()) { return; }

	FSuspenseCoreEquipmentEventData Event;
	Event.EventType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Loadout.Changed"));
	Event.Source    = this;
	Event.Payload   = LoadoutId.ToString();
	Event.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	Event.Metadata.Add(TEXT("Success"), bSuccess ? TEXT("true") : TEXT("false"));

	EventDispatcher->BroadcastEvent(Event);
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
