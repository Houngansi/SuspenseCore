// SuspenseCoreEquipmentDebugger.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Debug/SuspenseCoreEquipmentDebugger.h"
#include "SuspenseCore/Components/Core/SuspenseCoreEquipmentDataStore.h"
#include "SuspenseCore/Components/Core/SuspenseCoreEquipmentOperationExecutor.h"
#include "SuspenseCore/Components/Transaction/SuspenseCoreEquipmentTransactionProcessor.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreEquipmentDebug, Log, All);

TArray<IConsoleObject*> USuspenseCoreEquipmentDebugger::ConsoleCommands;

USuspenseCoreEquipmentDebugger::USuspenseCoreEquipmentDebugger()
{
}

//==================================================================
// Debug Info Collection
//==================================================================

FSuspenseCoreEquipmentDebugInfo USuspenseCoreEquipmentDebugger::GetDebugInfo(AActor* EquipmentOwner)
{
	FSuspenseCoreEquipmentDebugInfo Info;

	if (!EquipmentOwner)
	{
		Info.OwnerName = TEXT("Invalid Actor");
		return Info;
	}

	Info.OwnerName = EquipmentOwner->GetName();

	// Get DataStore by component class
	USuspenseCoreEquipmentDataStore* DataStore = GetDataStore(EquipmentOwner);
	if (DataStore)
	{
		Info.bDataStoreReady = true;
		Info.TotalSlots = DataStore->GetSlotCount();

		// Count occupied slots and gather details
		for (int32 i = 0; i < Info.TotalSlots; ++i)
		{
			FEquipmentSlotConfig SlotConfig = DataStore->GetSlotConfiguration(i);
			bool bOccupied = DataStore->IsSlotOccupied(i);

			if (bOccupied)
			{
				Info.OccupiedSlots++;
				FSuspenseCoreInventoryItemInstance Item = DataStore->GetSlotItem(i);
				Info.EquippedItems.Add(FString::Printf(TEXT("[%d] %s: %s"),
					i,
					*UEnum::GetValueAsString(SlotConfig.SlotType),
					*Item.ItemID.ToString()));
			}

			Info.SlotDetails.Add(FString::Printf(TEXT("[%d] %s - %s"),
				i,
				*UEnum::GetValueAsString(SlotConfig.SlotType),
				bOccupied ? TEXT("OCCUPIED") : TEXT("EMPTY")));
		}
	}

	// Get OperationExecutor by component class
	USuspenseCoreEquipmentOperationExecutor* OpsExecutor = GetOperationExecutor(EquipmentOwner);
	if (OpsExecutor)
	{
		Info.bOperationsReady = OpsExecutor->IsInitialized();
	}

	// Check for TransactionProcessor by class
	USuspenseCoreEquipmentTransactionProcessor* TxnProcessor =
		EquipmentOwner->FindComponentByClass<USuspenseCoreEquipmentTransactionProcessor>();
	if (TxnProcessor)
	{
		Info.bTransactionReady = true;
	}

	return Info;
}

FString USuspenseCoreEquipmentDebugger::GetDebugString(AActor* EquipmentOwner)
{
	FSuspenseCoreEquipmentDebugInfo Info = GetDebugInfo(EquipmentOwner);

	FString Result;
	Result += TEXT("========== EQUIPMENT DEBUG ==========\n");
	Result += FString::Printf(TEXT("Owner: %s\n"), *Info.OwnerName);
	Result += FString::Printf(TEXT("DataStore Ready: %s\n"), Info.bDataStoreReady ? TEXT("YES") : TEXT("NO"));
	Result += FString::Printf(TEXT("Operations Ready: %s\n"), Info.bOperationsReady ? TEXT("YES") : TEXT("NO"));
	Result += FString::Printf(TEXT("Transaction Ready: %s\n"), Info.bTransactionReady ? TEXT("YES") : TEXT("NO"));
	Result += FString::Printf(TEXT("Total Slots: %d\n"), Info.TotalSlots);
	Result += FString::Printf(TEXT("Occupied Slots: %d\n"), Info.OccupiedSlots);

	Result += TEXT("\n--- SLOT DETAILS ---\n");
	for (const FString& Detail : Info.SlotDetails)
	{
		Result += Detail + TEXT("\n");
	}

	if (Info.EquippedItems.Num() > 0)
	{
		Result += TEXT("\n--- EQUIPPED ITEMS ---\n");
		for (const FString& Item : Info.EquippedItems)
		{
			Result += Item + TEXT("\n");
		}
	}

	Result += TEXT("=====================================\n");
	return Result;
}

//==================================================================
// Test Utilities
//==================================================================

FEquipmentOperationResult USuspenseCoreEquipmentDebugger::TestEquipItem(
	AActor* EquipmentOwner,
	FName ItemID,
	int32 SlotIndex)
{
	FEquipmentOperationResult Result;
	Result.bSuccess = false;

	UE_LOG(LogSuspenseCoreEquipmentDebug, Warning,
		TEXT("========== TEST EQUIP ITEM =========="));
	UE_LOG(LogSuspenseCoreEquipmentDebug, Warning,
		TEXT("ItemID: %s, SlotIndex: %d"), *ItemID.ToString(), SlotIndex);

	if (!EquipmentOwner)
	{
		Result.ErrorMessage = FText::FromString(TEXT("Invalid Actor"));
		UE_LOG(LogSuspenseCoreEquipmentDebug, Error, TEXT("FAILED: %s"), *Result.ErrorMessage.ToString());
		return Result;
	}

	USuspenseCoreEquipmentOperationExecutor* OpsExecutor = GetOperationExecutor(EquipmentOwner);
	if (!OpsExecutor)
	{
		Result.ErrorMessage = FText::FromString(TEXT("OperationExecutor not found"));
		UE_LOG(LogSuspenseCoreEquipmentDebug, Error, TEXT("FAILED: %s"), *Result.ErrorMessage.ToString());
		return Result;
	}

	USuspenseCoreEquipmentDataStore* DataStore = GetDataStore(EquipmentOwner);
	if (!DataStore)
	{
		Result.ErrorMessage = FText::FromString(TEXT("DataStore not found"));
		UE_LOG(LogSuspenseCoreEquipmentDebug, Error, TEXT("FAILED: %s"), *Result.ErrorMessage.ToString());
		return Result;
	}

	// Check if OpsExecutor is initialized
	if (!OpsExecutor->IsInitialized())
	{
		UE_LOG(LogSuspenseCoreEquipmentDebug, Warning,
			TEXT("OperationExecutor not initialized, attempting to initialize..."));

		// Try to initialize with DataStore
		TScriptInterface<ISuspenseCoreEquipmentDataProvider> DataProvider;
		DataProvider.SetObject(DataStore);
		DataProvider.SetInterface(Cast<ISuspenseCoreEquipmentDataProvider>(DataStore));

		bool bInitialized = OpsExecutor->Initialize(DataProvider, nullptr);
		UE_LOG(LogSuspenseCoreEquipmentDebug, Warning,
			TEXT("Initialize result: %s"), bInitialized ? TEXT("SUCCESS") : TEXT("FAILED"));

		if (!bInitialized)
		{
			Result.ErrorMessage = FText::FromString(TEXT("Failed to initialize OperationExecutor"));
			return Result;
		}
	}

	// Create test item instance
	FSuspenseCoreInventoryItemInstance TestItem = CreateTestItemInstance(ItemID);

	UE_LOG(LogSuspenseCoreEquipmentDebug, Warning,
		TEXT("Created test item - InstanceID: %s, ItemID: %s"),
		*TestItem.InstanceID.ToString(), *TestItem.ItemID.ToString());

	// Execute equip operation
	Result = OpsExecutor->EquipItem(TestItem, SlotIndex);

	UE_LOG(LogSuspenseCoreEquipmentDebug, Warning,
		TEXT("EquipItem Result: %s"),
		Result.bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"));

	if (!Result.bSuccess)
	{
		UE_LOG(LogSuspenseCoreEquipmentDebug, Error,
			TEXT("Error: %s"), *Result.ErrorMessage.ToString());
	}

	UE_LOG(LogSuspenseCoreEquipmentDebug, Warning,
		TEXT("========================================="));

	return Result;
}

FEquipmentOperationResult USuspenseCoreEquipmentDebugger::TestUnequipItem(
	AActor* EquipmentOwner,
	int32 SlotIndex)
{
	FEquipmentOperationResult Result;
	Result.bSuccess = false;

	UE_LOG(LogSuspenseCoreEquipmentDebug, Warning,
		TEXT("========== TEST UNEQUIP ITEM =========="));
	UE_LOG(LogSuspenseCoreEquipmentDebug, Warning,
		TEXT("SlotIndex: %d"), SlotIndex);

	if (!EquipmentOwner)
	{
		Result.ErrorMessage = FText::FromString(TEXT("Invalid Actor"));
		return Result;
	}

	USuspenseCoreEquipmentOperationExecutor* OpsExecutor = GetOperationExecutor(EquipmentOwner);
	if (!OpsExecutor)
	{
		Result.ErrorMessage = FText::FromString(TEXT("OperationExecutor not found"));
		return Result;
	}

	Result = OpsExecutor->UnequipItem(SlotIndex);

	UE_LOG(LogSuspenseCoreEquipmentDebug, Warning,
		TEXT("UnequipItem Result: %s"),
		Result.bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"));

	return Result;
}

int32 USuspenseCoreEquipmentDebugger::TestEquipAllWeaponSlots(AActor* EquipmentOwner)
{
	UE_LOG(LogSuspenseCoreEquipmentDebug, Warning,
		TEXT("========== TEST EQUIP ALL WEAPON SLOTS =========="));

	int32 EquippedCount = 0;

	if (!EquipmentOwner)
	{
		UE_LOG(LogSuspenseCoreEquipmentDebug, Error, TEXT("Invalid Actor"));
		return 0;
	}

	// Test weapons for different slots
	struct FTestWeapon
	{
		FName ItemID;
		int32 SlotIndex;
		const TCHAR* SlotName;
	};

	// These are test item IDs - adjust based on your DataTable
	TArray<FTestWeapon> TestWeapons = {
		{ FName("TEST_AR_001"), 0, TEXT("PrimaryWeapon") },      // Slot 0 = Primary
		{ FName("TEST_SMG_001"), 1, TEXT("SecondaryWeapon") },   // Slot 1 = Secondary
		{ FName("TEST_PISTOL_001"), 2, TEXT("Holster") },        // Slot 2 = Holster
		{ FName("TEST_KNIFE_001"), 3, TEXT("Scabbard") }         // Slot 3 = Scabbard
	};

	for (const FTestWeapon& Weapon : TestWeapons)
	{
		UE_LOG(LogSuspenseCoreEquipmentDebug, Log,
			TEXT("Attempting to equip %s to %s (slot %d)"),
			*Weapon.ItemID.ToString(), Weapon.SlotName, Weapon.SlotIndex);

		FEquipmentOperationResult Result = TestEquipItem(EquipmentOwner, Weapon.ItemID, Weapon.SlotIndex);

		if (Result.bSuccess)
		{
			EquippedCount++;
			UE_LOG(LogSuspenseCoreEquipmentDebug, Log,
				TEXT("SUCCESS: Equipped %s to %s"),
				*Weapon.ItemID.ToString(), Weapon.SlotName);
		}
		else
		{
			UE_LOG(LogSuspenseCoreEquipmentDebug, Warning,
				TEXT("FAILED: Could not equip %s to %s - %s"),
				*Weapon.ItemID.ToString(), Weapon.SlotName, *Result.ErrorMessage.ToString());
		}
	}

	UE_LOG(LogSuspenseCoreEquipmentDebug, Warning,
		TEXT("Equipped %d/%d weapon slots"), EquippedCount, TestWeapons.Num());

	return EquippedCount;
}

int32 USuspenseCoreEquipmentDebugger::ClearAllEquipment(AActor* EquipmentOwner)
{
	UE_LOG(LogSuspenseCoreEquipmentDebug, Warning,
		TEXT("========== CLEAR ALL EQUIPMENT =========="));

	int32 ClearedCount = 0;

	if (!EquipmentOwner)
	{
		return 0;
	}

	USuspenseCoreEquipmentDataStore* DataStore = GetDataStore(EquipmentOwner);
	USuspenseCoreEquipmentOperationExecutor* OpsExecutor = GetOperationExecutor(EquipmentOwner);

	if (!DataStore || !OpsExecutor)
	{
		return 0;
	}

	int32 SlotCount = DataStore->GetSlotCount();
	for (int32 i = 0; i < SlotCount; ++i)
	{
		if (DataStore->IsSlotOccupied(i))
		{
			FEquipmentOperationResult Result = OpsExecutor->UnequipItem(i);
			if (Result.bSuccess)
			{
				ClearedCount++;
			}
		}
	}

	UE_LOG(LogSuspenseCoreEquipmentDebug, Warning,
		TEXT("Cleared %d items"), ClearedCount);

	return ClearedCount;
}

//==================================================================
// Diagnostics
//==================================================================

bool USuspenseCoreEquipmentDebugger::RunDiagnostic(AActor* EquipmentOwner, FString& OutReport)
{
	OutReport.Empty();
	bool bAllOk = true;

	OutReport += TEXT("========== EQUIPMENT DIAGNOSTIC ==========\n\n");

	if (!EquipmentOwner)
	{
		OutReport += TEXT("ERROR: Invalid Actor\n");
		return false;
	}

	OutReport += FString::Printf(TEXT("Owner: %s\n\n"), *EquipmentOwner->GetName());

	// Check DataStore
	OutReport += TEXT("--- Component Check ---\n");
	USuspenseCoreEquipmentDataStore* DataStore = GetDataStore(EquipmentOwner);
	if (DataStore)
	{
		OutReport += TEXT("[OK] EquipmentDataStore present\n");
	}
	else
	{
		OutReport += TEXT("[FAIL] EquipmentDataStore MISSING\n");
		bAllOk = false;
	}

	// Check OperationExecutor
	USuspenseCoreEquipmentOperationExecutor* OpsExecutor = GetOperationExecutor(EquipmentOwner);
	if (OpsExecutor)
	{
		OutReport += TEXT("[OK] EquipmentOperationExecutor present\n");
		if (OpsExecutor->IsInitialized())
		{
			OutReport += TEXT("[OK] OperationExecutor initialized\n");
		}
		else
		{
			OutReport += TEXT("[WARN] OperationExecutor NOT initialized\n");
		}
	}
	else
	{
		OutReport += TEXT("[FAIL] EquipmentOperationExecutor MISSING\n");
		bAllOk = false;
	}

	// Check TransactionProcessor by class
	USuspenseCoreEquipmentTransactionProcessor* TxnProcessor =
		EquipmentOwner->FindComponentByClass<USuspenseCoreEquipmentTransactionProcessor>();
	if (TxnProcessor)
	{
		OutReport += TEXT("[OK] EquipmentTxnProcessor present\n");
	}
	else
	{
		OutReport += TEXT("[WARN] EquipmentTxnProcessor MISSING\n");
	}

	// Check other equipment components by class name pattern
	TArray<UActorComponent*> AllComponents;
	EquipmentOwner->GetComponents(AllComponents);

	bool bHasPrediction = false;
	bool bHasReplication = false;
	bool bHasEventDispatcher = false;
	bool bHasInventoryBridge = false;
	bool bHasWeaponStateManager = false;

	for (UActorComponent* Comp : AllComponents)
	{
		if (!Comp) continue;
		FString ClassName = Comp->GetClass()->GetName();

		if (ClassName.Contains(TEXT("Prediction")))
		{
			bHasPrediction = true;
		}
		else if (ClassName.Contains(TEXT("ReplicationManager")))
		{
			bHasReplication = true;
		}
		else if (ClassName.Contains(TEXT("EventDispatcher")))
		{
			bHasEventDispatcher = true;
		}
		else if (ClassName.Contains(TEXT("InventoryBridge")))
		{
			bHasInventoryBridge = true;
		}
		else if (ClassName.Contains(TEXT("WeaponStateManager")))
		{
			bHasWeaponStateManager = true;
		}
	}

	OutReport += bHasPrediction ? TEXT("[OK] EquipmentPrediction present\n") : TEXT("[WARN] EquipmentPrediction MISSING\n");
	OutReport += bHasReplication ? TEXT("[OK] EquipmentReplication present\n") : TEXT("[WARN] EquipmentReplication MISSING\n");
	OutReport += bHasEventDispatcher ? TEXT("[OK] EquipmentEventDispatcher present\n") : TEXT("[WARN] EquipmentEventDispatcher MISSING\n");
	OutReport += bHasInventoryBridge ? TEXT("[OK] EquipmentInventoryBridge present\n") : TEXT("[WARN] EquipmentInventoryBridge MISSING\n");
	OutReport += bHasWeaponStateManager ? TEXT("[OK] WeaponStateManager present\n") : TEXT("[WARN] WeaponStateManager MISSING\n");

	// Slot configuration check
	OutReport += TEXT("\n--- Slot Configuration ---\n");
	if (DataStore)
	{
		int32 SlotCount = DataStore->GetSlotCount();
		OutReport += FString::Printf(TEXT("Total Slots: %d\n"), SlotCount);

		for (int32 i = 0; i < SlotCount; ++i)
		{
			FEquipmentSlotConfig Config = DataStore->GetSlotConfiguration(i);
			OutReport += FString::Printf(TEXT("  [%d] %s - Tag: %s, Valid: %s\n"),
				i,
				*UEnum::GetValueAsString(Config.SlotType),
				*Config.SlotTag.ToString(),
				Config.IsValid() ? TEXT("YES") : TEXT("NO"));
		}
	}

	OutReport += TEXT("\n==========================================\n");
	OutReport += FString::Printf(TEXT("RESULT: %s\n"), bAllOk ? TEXT("ALL OK") : TEXT("ISSUES FOUND"));

	return bAllOk;
}

FString USuspenseCoreEquipmentDebugger::ValidateWiring(AActor* EquipmentOwner)
{
	FString Report;
	RunDiagnostic(EquipmentOwner, Report);
	return Report;
}

//==================================================================
// Logging
//==================================================================

void USuspenseCoreEquipmentDebugger::LogEquipmentState(AActor* EquipmentOwner)
{
	FString DebugStr = GetDebugString(EquipmentOwner);
	UE_LOG(LogSuspenseCoreEquipmentDebug, Warning, TEXT("\n%s"), *DebugStr);
}

void USuspenseCoreEquipmentDebugger::LogSlotConfigurations(AActor* EquipmentOwner)
{
	if (!EquipmentOwner)
	{
		UE_LOG(LogSuspenseCoreEquipmentDebug, Error, TEXT("Invalid Actor"));
		return;
	}

	USuspenseCoreEquipmentDataStore* DataStore = GetDataStore(EquipmentOwner);
	if (!DataStore)
	{
		UE_LOG(LogSuspenseCoreEquipmentDebug, Error, TEXT("DataStore not found"));
		return;
	}

	UE_LOG(LogSuspenseCoreEquipmentDebug, Warning, TEXT("========== SLOT CONFIGURATIONS =========="));

	int32 SlotCount = DataStore->GetSlotCount();
	for (int32 i = 0; i < SlotCount; ++i)
	{
		FEquipmentSlotConfig Config = DataStore->GetSlotConfiguration(i);
		UE_LOG(LogSuspenseCoreEquipmentDebug, Log,
			TEXT("[%d] Type: %s | Tag: %s | Socket: %s | AllowedTypes: %d"),
			i,
			*UEnum::GetValueAsString(Config.SlotType),
			*Config.SlotTag.ToString(),
			*Config.AttachmentSocket.ToString(),
			Config.AllowedItemTypes.Num());
	}

	UE_LOG(LogSuspenseCoreEquipmentDebug, Warning, TEXT("=========================================="));
}

//==================================================================
// Console Commands
//==================================================================

void USuspenseCoreEquipmentDebugger::RegisterConsoleCommands()
{
	// Will be implemented with IConsoleManager
	UE_LOG(LogSuspenseCoreEquipmentDebug, Log, TEXT("Equipment debug console commands registered"));
}

void USuspenseCoreEquipmentDebugger::UnregisterConsoleCommands()
{
	for (IConsoleObject* Cmd : ConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Cmd);
	}
	ConsoleCommands.Empty();
}

//==================================================================
// Private Helpers
//==================================================================

USuspenseCoreEquipmentDataStore* USuspenseCoreEquipmentDebugger::GetDataStore(AActor* Owner)
{
	if (!Owner)
	{
		return nullptr;
	}
	return Owner->FindComponentByClass<USuspenseCoreEquipmentDataStore>();
}

USuspenseCoreEquipmentOperationExecutor* USuspenseCoreEquipmentDebugger::GetOperationExecutor(AActor* Owner)
{
	if (!Owner)
	{
		return nullptr;
	}
	return Owner->FindComponentByClass<USuspenseCoreEquipmentOperationExecutor>();
}

FSuspenseCoreInventoryItemInstance USuspenseCoreEquipmentDebugger::CreateTestItemInstance(FName ItemID)
{
	FSuspenseCoreInventoryItemInstance TestItem;
	TestItem.InstanceID = FGuid::NewGuid();
	TestItem.ItemID = ItemID;
	TestItem.StackCount = 1;
	TestItem.Durability = 100.0f;
	TestItem.MaxDurability = 100.0f;
	return TestItem;
}
