// SuspenseCoreInventoryDebugger.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Debug/SuspenseCoreInventoryDebugger.h"
#include "SuspenseCore/Components/SuspenseCoreInventoryComponent.h"
#include "SuspenseCore/Base/SuspenseCoreInventoryManager.h"
#include "SuspenseCore/Base/SuspenseCoreInventoryLogs.h"
#include "Engine/Canvas.h"
#include "HAL/IConsoleManager.h"

TArray<IConsoleObject*> USuspenseCoreInventoryDebugger::ConsoleCommands;

USuspenseCoreInventoryDebugger::USuspenseCoreInventoryDebugger()
{
}

FSuspenseCoreInventoryDebugInfo USuspenseCoreInventoryDebugger::GetDebugInfo(USuspenseCoreInventoryComponent* Component)
{
	FSuspenseCoreInventoryDebugInfo Info;

	if (!Component)
	{
		return Info;
	}

	Info.OwnerName = Component->GetOwner() ? Component->GetOwner()->GetName() : TEXT("Unknown");
	Info.GridSize = Component->Execute_GetGridSize(Component);
	Info.TotalSlots = Info.GridSize.X * Info.GridSize.Y;
	Info.ItemCount = Component->Execute_GetTotalItemCount(Component);
	Info.CurrentWeight = Component->Execute_GetCurrentWeight(Component);
	Info.MaxWeight = Component->Execute_GetMaxWeight(Component);
	Info.bIsInitialized = Component->IsInitialized();
	Info.bHasActiveTransaction = Component->IsTransactionActive();

	// Count used slots
	Info.SlotOccupation.SetNum(Info.TotalSlots);
	Info.UsedSlots = 0;
	for (int32 i = 0; i < Info.TotalSlots; ++i)
	{
		bool bOccupied = Component->IsSlotOccupied(i);
		Info.SlotOccupation[i] = bOccupied;
		if (bOccupied)
		{
			Info.UsedSlots++;
		}
	}

	// Get item details
	TArray<FSuspenseCoreItemInstance> Items = Component->GetAllItemInstances();
	for (const FSuspenseCoreItemInstance& Item : Items)
	{
		FString Detail = FString::Printf(TEXT("%s x%d @ Slot %d"),
			*Item.ItemID.ToString(),
			Item.Quantity,
			Item.SlotIndex);
		Info.ItemDetails.Add(Detail);
	}

	return Info;
}

FString USuspenseCoreInventoryDebugger::GetDebugString(USuspenseCoreInventoryComponent* Component)
{
	if (!Component)
	{
		return TEXT("Invalid Component");
	}

	FSuspenseCoreInventoryDebugInfo Info = GetDebugInfo(Component);

	FString Result;
	Result += FString::Printf(TEXT("=== Inventory Debug: %s ===\n"), *Info.OwnerName);
	Result += FString::Printf(TEXT("Grid: %dx%d (%d slots, %d used)\n"),
		Info.GridSize.X, Info.GridSize.Y, Info.TotalSlots, Info.UsedSlots);
	Result += FString::Printf(TEXT("Weight: %.2f / %.2f\n"), Info.CurrentWeight, Info.MaxWeight);
	Result += FString::Printf(TEXT("Items: %d\n"), Info.ItemCount);
	Result += FString::Printf(TEXT("Initialized: %s, Transaction: %s\n"),
		Info.bIsInitialized ? TEXT("Yes") : TEXT("No"),
		Info.bHasActiveTransaction ? TEXT("Active") : TEXT("None"));

	if (Info.ItemDetails.Num() > 0)
	{
		Result += TEXT("Contents:\n");
		for (const FString& Detail : Info.ItemDetails)
		{
			Result += FString::Printf(TEXT("  - %s\n"), *Detail);
		}
	}

	return Result;
}

FString USuspenseCoreInventoryDebugger::GetGridVisualization(USuspenseCoreInventoryComponent* Component)
{
	if (!Component)
	{
		return TEXT("Invalid Component");
	}

	FIntPoint GridSize = Component->Execute_GetGridSize(Component);
	FString Result;

	// Header
	Result += TEXT("+");
	for (int32 X = 0; X < GridSize.X; ++X)
	{
		Result += TEXT("---+");
	}
	Result += TEXT("\n");

	// Grid rows
	for (int32 Y = 0; Y < GridSize.Y; ++Y)
	{
		Result += TEXT("|");
		for (int32 X = 0; X < GridSize.X; ++X)
		{
			int32 Slot = Y * GridSize.X + X;
			if (Component->IsSlotOccupied(Slot))
			{
				FSuspenseCoreItemInstance Instance;
				if (Component->GetItemInstanceAtSlot(Slot, Instance))
				{
					// Show first letter of item ID
					FString ItemStr = Instance.ItemID.ToString().Left(1);
					Result += FString::Printf(TEXT(" %s |"), *ItemStr);
				}
				else
				{
					Result += TEXT(" X |");
				}
			}
			else
			{
				Result += TEXT("   |");
			}
		}
		Result += TEXT("\n+");
		for (int32 X = 0; X < GridSize.X; ++X)
		{
			Result += TEXT("---+");
		}
		Result += TEXT("\n");
	}

	return Result;
}

void USuspenseCoreInventoryDebugger::LogInventoryContents(USuspenseCoreInventoryComponent* Component)
{
	FString DebugStr = GetDebugString(Component);
	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("%s"), *DebugStr);
}

void USuspenseCoreInventoryDebugger::LogAllInventories(USuspenseCoreInventoryManager* Manager)
{
	if (!Manager)
	{
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("Invalid Inventory Manager"));
		return;
	}

	TArray<USuspenseCoreInventoryComponent*> Inventories = Manager->GetAllInventories();

	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("=== All Registered Inventories (%d) ==="), Inventories.Num());

	for (USuspenseCoreInventoryComponent* Comp : Inventories)
	{
		LogInventoryContents(Comp);
	}
}

void USuspenseCoreInventoryDebugger::LogItemDetails(const FSuspenseCoreItemInstance& Instance)
{
	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("Item Details:"));
	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("  InstanceID: %s"), *Instance.UniqueInstanceID.ToString());
	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("  ItemID: %s"), *Instance.ItemID.ToString());
	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("  Quantity: %d"), Instance.Quantity);
	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("  SlotIndex: %d"), Instance.SlotIndex);
	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("  GridPosition: (%d, %d)"), Instance.GridPosition.X, Instance.GridPosition.Y);
	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("  Rotation: %d"), Instance.Rotation);

	if (Instance.WeaponState.bHasState)
	{
		UE_LOG(LogSuspenseCoreInventory, Log, TEXT("  WeaponState: Ammo=%d/%d"),
			FMath::RoundToInt(Instance.WeaponState.CurrentAmmo),
			FMath::RoundToInt(Instance.WeaponState.ReserveAmmo));
	}

	for (const FSuspenseCoreRuntimeProperty& Prop : Instance.RuntimeProperties)
	{
		UE_LOG(LogSuspenseCoreInventory, Log, TEXT("  %s: %.2f"), *Prop.PropertyName.ToString(), Prop.Value);
	}
}

bool USuspenseCoreInventoryDebugger::RunDiagnostic(USuspenseCoreInventoryComponent* Component, FString& OutReport)
{
	if (!Component)
	{
		OutReport = TEXT("Invalid component");
		return false;
	}

	TArray<FString> Issues;
	OutReport = TEXT("=== Inventory Diagnostic Report ===\n");

	// Check initialization
	if (!Component->IsInitialized())
	{
		Issues.Add(TEXT("Component not initialized"));
	}

	// Check orphaned slots
	TArray<int32> OrphanedSlots;
	if (CheckOrphanedSlots(Component, OrphanedSlots) > 0)
	{
		Issues.Add(FString::Printf(TEXT("Found %d orphaned slots"), OrphanedSlots.Num()));
	}

	// Check duplicates
	TArray<FGuid> Duplicates;
	if (CheckDuplicateInstances(Component, Duplicates) > 0)
	{
		Issues.Add(FString::Printf(TEXT("Found %d duplicate instances"), Duplicates.Num()));
	}

	// Verify weight
	float ExpectedWeight;
	if (!VerifyWeight(Component, ExpectedWeight))
	{
		Issues.Add(FString::Printf(TEXT("Weight mismatch: expected %.2f"), ExpectedWeight));
	}

	// Validate integrity
	TArray<FString> IntegrityErrors;
	Component->ValidateIntegrity(IntegrityErrors);
	Issues.Append(IntegrityErrors);

	// Build report
	if (Issues.Num() == 0)
	{
		OutReport += TEXT("All checks passed. No issues found.\n");
		return true;
	}
	else
	{
		OutReport += FString::Printf(TEXT("Found %d issues:\n"), Issues.Num());
		for (const FString& Issue : Issues)
		{
			OutReport += FString::Printf(TEXT("  - %s\n"), *Issue);
		}
		return false;
	}
}

int32 USuspenseCoreInventoryDebugger::CheckOrphanedSlots(USuspenseCoreInventoryComponent* Component, TArray<int32>& OutOrphanedSlots)
{
	OutOrphanedSlots.Empty();

	if (!Component)
	{
		return 0;
	}

	FIntPoint GridSize = Component->Execute_GetGridSize(Component);
	int32 TotalSlots = GridSize.X * GridSize.Y;

	TArray<FSuspenseCoreItemInstance> Items = Component->GetAllItemInstances();
	TSet<int32> ValidSlots;

	// Build set of valid slots from items
	for (const FSuspenseCoreItemInstance& Item : Items)
	{
		if (Item.SlotIndex >= 0 && Item.SlotIndex < TotalSlots)
		{
			ValidSlots.Add(Item.SlotIndex);
		}
	}

	// Check each slot
	for (int32 i = 0; i < TotalSlots; ++i)
	{
		if (Component->IsSlotOccupied(i) && !ValidSlots.Contains(i))
		{
			OutOrphanedSlots.Add(i);
		}
	}

	return OutOrphanedSlots.Num();
}

int32 USuspenseCoreInventoryDebugger::CheckDuplicateInstances(USuspenseCoreInventoryComponent* Component, TArray<FGuid>& OutDuplicates)
{
	OutDuplicates.Empty();

	if (!Component)
	{
		return 0;
	}

	TArray<FSuspenseCoreItemInstance> Items = Component->GetAllItemInstances();
	TSet<FGuid> SeenIDs;

	for (const FSuspenseCoreItemInstance& Item : Items)
	{
		if (SeenIDs.Contains(Item.UniqueInstanceID))
		{
			OutDuplicates.AddUnique(Item.UniqueInstanceID);
		}
		else
		{
			SeenIDs.Add(Item.UniqueInstanceID);
		}
	}

	return OutDuplicates.Num();
}

bool USuspenseCoreInventoryDebugger::VerifyWeight(USuspenseCoreInventoryComponent* Component, float& OutExpectedWeight)
{
	OutExpectedWeight = 0.0f;

	if (!Component)
	{
		return false;
	}

	// This would need DataManager to get item weights
	// For now, just return stored weight
	OutExpectedWeight = Component->Execute_GetCurrentWeight(Component);
	return true;
}

int32 USuspenseCoreInventoryDebugger::FillWithRandomItems(
	USuspenseCoreInventoryComponent* Component,
	int32 ItemCount,
	const TArray<FName>& ItemIDs)
{
	if (!Component || ItemCount <= 0)
	{
		return 0;
	}

	TArray<FName> IDs = ItemIDs;
	if (IDs.Num() == 0)
	{
		// Default test items
		IDs.Add(FName("TestItem_01"));
		IDs.Add(FName("TestItem_02"));
		IDs.Add(FName("TestItem_03"));
	}

	int32 Added = 0;
	for (int32 i = 0; i < ItemCount; ++i)
	{
		FName ItemID = IDs[FMath::RandRange(0, IDs.Num() - 1)];
		int32 Quantity = FMath::RandRange(1, 10);

		if (Component->Execute_AddItemByID(Component, ItemID, Quantity))
		{
			++Added;
		}
	}

	return Added;
}

bool USuspenseCoreInventoryDebugger::StressTest(
	USuspenseCoreInventoryComponent* Component,
	int32 OperationCount,
	FString& OutReport)
{
	if (!Component)
	{
		OutReport = TEXT("Invalid component");
		return false;
	}

	OutReport = TEXT("=== Stress Test Report ===\n");
	int32 Successes = 0;
	int32 Failures = 0;
	double StartTime = FPlatformTime::Seconds();

	for (int32 i = 0; i < OperationCount; ++i)
	{
		int32 Op = FMath::RandRange(0, 2);

		switch (Op)
		{
		case 0: // Add
		{
			FName ItemID = FName(*FString::Printf(TEXT("StressItem_%d"), FMath::RandRange(1, 10)));
			if (Component->Execute_AddItemByID(Component, ItemID, 1))
			{
				++Successes;
			}
			else
			{
				++Failures;
			}
			break;
		}
		case 1: // Remove
		{
			TArray<FSuspenseCoreItemInstance> Items = Component->GetAllItemInstances();
			if (Items.Num() > 0)
			{
				int32 Index = FMath::RandRange(0, Items.Num() - 1);
				if (Component->RemoveItemInstance(Items[Index].UniqueInstanceID))
				{
					++Successes;
				}
				else
				{
					++Failures;
				}
			}
			break;
		}
		case 2: // Move
		{
			TArray<FSuspenseCoreItemInstance> Items = Component->GetAllItemInstances();
			if (Items.Num() > 0)
			{
				int32 Index = FMath::RandRange(0, Items.Num() - 1);
				FIntPoint GridSize = Component->Execute_GetGridSize(Component);
				int32 TargetSlot = FMath::RandRange(0, GridSize.X * GridSize.Y - 1);
				if (Component->Execute_MoveItem(Component, Items[Index].SlotIndex, TargetSlot))
				{
					++Successes;
				}
				else
				{
					++Failures;
				}
			}
			break;
		}
		}
	}

	double Duration = FPlatformTime::Seconds() - StartTime;

	OutReport += FString::Printf(TEXT("Operations: %d\n"), OperationCount);
	OutReport += FString::Printf(TEXT("Successes: %d\n"), Successes);
	OutReport += FString::Printf(TEXT("Failures: %d\n"), Failures);
	OutReport += FString::Printf(TEXT("Duration: %.3f seconds\n"), Duration);
	OutReport += FString::Printf(TEXT("Ops/sec: %.1f\n"), OperationCount / Duration);

	return Failures == 0;
}

bool USuspenseCoreInventoryDebugger::TestAddRemoveCycle(
	USuspenseCoreInventoryComponent* Component,
	FName ItemID,
	int32 Iterations)
{
	if (!Component || ItemID.IsNone())
	{
		return false;
	}

	for (int32 i = 0; i < Iterations; ++i)
	{
		// Add
		if (!Component->Execute_AddItemByID(Component, ItemID, 1))
		{
			UE_LOG(LogSuspenseCoreInventory, Error, TEXT("AddRemoveCycle: Add failed at iteration %d"), i);
			return false;
		}

		// Get the instance we just added
		TArray<FSuspenseCoreItemInstance> Items = Component->GetAllItemInstances();
		FSuspenseCoreItemInstance* Found = Items.FindByPredicate([ItemID](const FSuspenseCoreItemInstance& Item)
		{
			return Item.ItemID == ItemID;
		});

		if (!Found)
		{
			UE_LOG(LogSuspenseCoreInventory, Error, TEXT("AddRemoveCycle: Item not found at iteration %d"), i);
			return false;
		}

		// Remove
		if (!Component->RemoveItemInstance(Found->UniqueInstanceID))
		{
			UE_LOG(LogSuspenseCoreInventory, Error, TEXT("AddRemoveCycle: Remove failed at iteration %d"), i);
			return false;
		}

		// Verify empty (for single item test)
		if (Component->Execute_GetItemCountByID(Component, ItemID) != 0)
		{
			UE_LOG(LogSuspenseCoreInventory, Error, TEXT("AddRemoveCycle: Item still present at iteration %d"), i);
			return false;
		}
	}

	return true;
}

void USuspenseCoreInventoryDebugger::SetDebugDrawEnabled(USuspenseCoreInventoryComponent* Component, bool bEnable)
{
	// Would set a flag on component to enable debug drawing
	// Implementation depends on component having debug draw support
}

void USuspenseCoreInventoryDebugger::RegisterConsoleCommands()
{
#if !UE_BUILD_SHIPPING
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("SuspenseCore.Inventory.Debug"),
		TEXT("Show inventory debug info"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&HandleDebugCommand)
	));

	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("SuspenseCore.Inventory.List"),
		TEXT("List all registered inventories"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&HandleListCommand)
	));
#endif
}

void USuspenseCoreInventoryDebugger::UnregisterConsoleCommands()
{
#if !UE_BUILD_SHIPPING
	for (IConsoleObject* Cmd : ConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Cmd);
	}
	ConsoleCommands.Empty();
#endif
}

void USuspenseCoreInventoryDebugger::HandleDebugCommand(const TArray<FString>& Args)
{
	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("Inventory Debug Command"));
	// Implementation would get first player's inventory and log it
}

void USuspenseCoreInventoryDebugger::HandleListCommand(const TArray<FString>& Args)
{
	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("Inventory List Command"));
	// Implementation would list all registered inventories
}

void USuspenseCoreInventoryDebugger::HandleAddCommand(const TArray<FString>& Args)
{
	// inventory.add <ItemID> [Quantity]
}

void USuspenseCoreInventoryDebugger::HandleClearCommand(const TArray<FString>& Args)
{
	// inventory.clear
}
