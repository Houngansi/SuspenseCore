// SuspenseCoreEquipmentDebugger.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SuspenseCore/Types/Equipment/SuspenseCoreEquipmentTypes.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreLoadoutSettings.h"
#include "SuspenseCoreEquipmentDebugger.generated.h"

// Forward declarations
class USuspenseCoreEquipmentDataStore;
class USuspenseCoreEquipmentOperationExecutor;
class USuspenseCoreEquipmentTransactionProcessor;
class AActor;

/**
 * FSuspenseCoreEquipmentDebugInfo
 *
 * Debug information snapshot for equipment system.
 */
USTRUCT(BlueprintType)
struct EQUIPMENTSYSTEM_API FSuspenseCoreEquipmentDebugInfo
{
	GENERATED_BODY()

	/** Owner actor name */
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	FString OwnerName;

	/** Total equipment slots */
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	int32 TotalSlots = 0;

	/** Occupied slots count */
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	int32 OccupiedSlots = 0;

	/** Is DataStore initialized */
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	bool bDataStoreReady = false;

	/** Is OperationExecutor initialized */
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	bool bOperationsReady = false;

	/** Is TransactionProcessor ready */
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	bool bTransactionReady = false;

	/** Slot details */
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	TArray<FString> SlotDetails;

	/** Currently equipped items */
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	TArray<FString> EquippedItems;
};

/**
 * USuspenseCoreEquipmentDebugger
 *
 * Debug utilities for SuspenseCore equipment system.
 * Provides testing, logging, and diagnostic tools.
 *
 * Works with any Actor that has equipment components (typically PlayerState).
 * Uses component lookup by class to avoid circular module dependencies.
 *
 * USAGE:
 * - Console commands for testing equipment
 * - Programmatic test item equipping
 * - System diagnostics
 */
UCLASS(BlueprintType)
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentDebugger : public UObject
{
	GENERATED_BODY()

public:
	USuspenseCoreEquipmentDebugger();

	//==================================================================
	// Debug Info Collection
	//==================================================================

	/**
	 * Get debug info for equipment system.
	 * @param EquipmentOwner Actor that owns equipment components (typically PlayerState)
	 * @return Debug info snapshot
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Debug")
	static FSuspenseCoreEquipmentDebugInfo GetDebugInfo(AActor* EquipmentOwner);

	/**
	 * Get debug string for equipment system.
	 * @param EquipmentOwner Actor that owns equipment components
	 * @return Formatted debug string
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Debug")
	static FString GetDebugString(AActor* EquipmentOwner);

	//==================================================================
	// Test Utilities
	//==================================================================

	/**
	 * Test equip an item to a slot.
	 * Creates a test item instance and attempts to equip it.
	 *
	 * @param EquipmentOwner Actor that owns equipment components
	 * @param ItemID Item ID to equip (from DataTable)
	 * @param SlotIndex Target slot index (-1 = auto-find)
	 * @return Operation result
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Debug")
	static FEquipmentOperationResult TestEquipItem(
		AActor* EquipmentOwner,
		FName ItemID,
		int32 SlotIndex = -1
	);

	/**
	 * Test unequip an item from a slot.
	 *
	 * @param EquipmentOwner Actor that owns equipment components
	 * @param SlotIndex Slot to unequip
	 * @return Operation result
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Debug")
	static FEquipmentOperationResult TestUnequipItem(
		AActor* EquipmentOwner,
		int32 SlotIndex
	);

	/**
	 * Test equip items to all weapon slots.
	 * Equips test weapons to PrimaryWeapon, SecondaryWeapon, Holster, Scabbard.
	 *
	 * @param EquipmentOwner Actor that owns equipment components
	 * @return Number of items successfully equipped
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Debug")
	static int32 TestEquipAllWeaponSlots(AActor* EquipmentOwner);

	/**
	 * Clear all equipment slots.
	 *
	 * @param EquipmentOwner Actor that owns equipment components
	 * @return Number of items unequipped
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Debug")
	static int32 ClearAllEquipment(AActor* EquipmentOwner);

	//==================================================================
	// Diagnostics
	//==================================================================

	/**
	 * Run full diagnostic on equipment system.
	 * @param EquipmentOwner Actor that owns equipment components
	 * @param OutReport Diagnostic report
	 * @return true if no issues found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Debug")
	static bool RunDiagnostic(AActor* EquipmentOwner, FString& OutReport);

	/**
	 * Validate equipment system wiring.
	 * Checks all components are properly connected.
	 *
	 * @param EquipmentOwner Actor that owns equipment components
	 * @return Validation result string
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Debug")
	static FString ValidateWiring(AActor* EquipmentOwner);

	//==================================================================
	// Logging
	//==================================================================

	/**
	 * Log equipment state to console.
	 * @param EquipmentOwner Actor that owns equipment components
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Debug")
	static void LogEquipmentState(AActor* EquipmentOwner);

	/**
	 * Log all slot configurations.
	 * @param EquipmentOwner Actor that owns equipment components
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Debug")
	static void LogSlotConfigurations(AActor* EquipmentOwner);

	//==================================================================
	// Console Commands
	//==================================================================

	/**
	 * Register debug console commands.
	 */
	static void RegisterConsoleCommands();

	/**
	 * Unregister debug console commands.
	 */
	static void UnregisterConsoleCommands();

private:
	/** Console command handles */
	static TArray<IConsoleObject*> ConsoleCommands;

	/** Get DataStore from actor (finds component by class) */
	static USuspenseCoreEquipmentDataStore* GetDataStore(AActor* Owner);

	/** Get OperationExecutor from actor (finds component by class) */
	static USuspenseCoreEquipmentOperationExecutor* GetOperationExecutor(AActor* Owner);

	/** Create test item instance */
	static FSuspenseCoreInventoryItemInstance CreateTestItemInstance(FName ItemID);
};
