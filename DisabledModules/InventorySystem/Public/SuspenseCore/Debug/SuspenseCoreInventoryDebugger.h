// SuspenseCoreInventoryDebugger.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCoreInventoryDebugger.generated.h"

// Forward declarations
class USuspenseCoreInventoryComponent;
class USuspenseCoreInventoryManager;
class UCanvas;

/**
 * FSuspenseCoreInventoryDebugInfo
 *
 * Debug information snapshot.
 */
USTRUCT(BlueprintType)
struct INVENTORYSYSTEM_API FSuspenseCoreInventoryDebugInfo
{
	GENERATED_BODY()

	/** Owner actor name */
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	FString OwnerName;

	/** Grid dimensions */
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	FIntPoint GridSize;

	/** Total slots */
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	int32 TotalSlots;

	/** Used slots */
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	int32 UsedSlots;

	/** Item count */
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	int32 ItemCount;

	/** Current weight */
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	float CurrentWeight;

	/** Max weight */
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	float MaxWeight;

	/** Is initialized */
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	bool bIsInitialized;

	/** Has active transaction */
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	bool bHasActiveTransaction;

	/** Item details */
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	TArray<FString> ItemDetails;

	/** Slot occupation map */
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	TArray<bool> SlotOccupation;

	FSuspenseCoreInventoryDebugInfo()
		: GridSize(FIntPoint::ZeroValue)
		, TotalSlots(0)
		, UsedSlots(0)
		, ItemCount(0)
		, CurrentWeight(0.0f)
		, MaxWeight(0.0f)
		, bIsInitialized(false)
		, bHasActiveTransaction(false)
	{
	}
};

/**
 * USuspenseCoreInventoryDebugger
 *
 * Debug utilities for SuspenseCore inventory system.
 * Provides visualization, logging, and testing tools.
 *
 * USAGE:
 * - In-game debug overlay
 * - Console commands
 * - Unit test support
 */
UCLASS(BlueprintType)
class INVENTORYSYSTEM_API USuspenseCoreInventoryDebugger : public UObject
{
	GENERATED_BODY()

public:
	USuspenseCoreInventoryDebugger();

	//==================================================================
	// Debug Info Collection
	//==================================================================

	/**
	 * Get debug info for inventory.
	 * @param Component Inventory to inspect
	 * @return Debug info snapshot
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Debug")
	static FSuspenseCoreInventoryDebugInfo GetDebugInfo(USuspenseCoreInventoryComponent* Component);

	/**
	 * Get debug string for inventory.
	 * @param Component Inventory to describe
	 * @return Formatted debug string
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Debug")
	static FString GetDebugString(USuspenseCoreInventoryComponent* Component);

	/**
	 * Get grid visualization string.
	 * @param Component Inventory to visualize
	 * @return ASCII art grid representation
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Debug")
	static FString GetGridVisualization(USuspenseCoreInventoryComponent* Component);

	//==================================================================
	// Logging
	//==================================================================

	/**
	 * Log inventory contents.
	 * @param Component Inventory to log
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Debug")
	static void LogInventoryContents(USuspenseCoreInventoryComponent* Component);

	/**
	 * Log all registered inventories.
	 * @param Manager Inventory manager
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Debug")
	static void LogAllInventories(USuspenseCoreInventoryManager* Manager);

	/**
	 * Log item details.
	 * @param Instance Item to log
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Debug")
	static void LogItemDetails(const FSuspenseCoreItemInstance& Instance);

	//==================================================================
	// Validation
	//==================================================================

	/**
	 * Run full diagnostic on inventory.
	 * @param Component Inventory to diagnose
	 * @param OutReport Diagnostic report
	 * @return true if no issues found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Debug")
	static bool RunDiagnostic(USuspenseCoreInventoryComponent* Component, FString& OutReport);

	/**
	 * Check for orphaned slots.
	 * @param Component Inventory to check
	 * @param OutOrphanedSlots Slots with issues
	 * @return Number of issues found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Debug")
	static int32 CheckOrphanedSlots(USuspenseCoreInventoryComponent* Component, TArray<int32>& OutOrphanedSlots);

	/**
	 * Check for duplicate instances.
	 * @param Component Inventory to check
	 * @param OutDuplicates Duplicate instance IDs
	 * @return Number of duplicates found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Debug")
	static int32 CheckDuplicateInstances(USuspenseCoreInventoryComponent* Component, TArray<FGuid>& OutDuplicates);

	/**
	 * Verify weight calculation.
	 * @param Component Inventory to verify
	 * @param OutExpectedWeight Calculated expected weight
	 * @return true if weight matches
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Debug")
	static bool VerifyWeight(USuspenseCoreInventoryComponent* Component, float& OutExpectedWeight);

	//==================================================================
	// Test Utilities
	//==================================================================

	/**
	 * Fill inventory with random items.
	 * @param Component Target inventory
	 * @param ItemCount Number of items to add
	 * @param ItemIDs Available item IDs (random if empty)
	 * @return Number of items added
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Debug")
	static int32 FillWithRandomItems(
		USuspenseCoreInventoryComponent* Component,
		int32 ItemCount,
		const TArray<FName>& ItemIDs
	);

	/**
	 * Stress test inventory operations.
	 * @param Component Target inventory
	 * @param OperationCount Number of operations
	 * @param OutReport Test report
	 * @return true if all operations succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Debug")
	static bool StressTest(
		USuspenseCoreInventoryComponent* Component,
		int32 OperationCount,
		FString& OutReport
	);

	/**
	 * Test add/remove cycle.
	 * @param Component Target inventory
	 * @param ItemID Item to test
	 * @param Iterations Number of cycles
	 * @return true if all cycles passed
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Debug")
	static bool TestAddRemoveCycle(
		USuspenseCoreInventoryComponent* Component,
		FName ItemID,
		int32 Iterations
	);

	//==================================================================
	// Visual Debug
	//==================================================================

#if !UE_BUILD_SHIPPING
	/**
	 * Draw debug overlay.
	 * @param Canvas Canvas to draw on
	 * @param Component Inventory to visualize
	 * @param ScreenPosition Screen position
	 */
	static void DrawDebugOverlay(
		UCanvas* Canvas,
		USuspenseCoreInventoryComponent* Component,
		FVector2D ScreenPosition
	);
#endif

	/**
	 * Enable/disable debug drawing for component.
	 * @param Component Target component
	 * @param bEnable Enable flag
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Debug")
	static void SetDebugDrawEnabled(USuspenseCoreInventoryComponent* Component, bool bEnable);

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
	/** Handle console command: inventory.debug */
	static void HandleDebugCommand(const TArray<FString>& Args);

	/** Handle console command: inventory.list */
	static void HandleListCommand(const TArray<FString>& Args);

	/** Handle console command: inventory.add */
	static void HandleAddCommand(const TArray<FString>& Args);

	/** Handle console command: inventory.clear */
	static void HandleClearCommand(const TArray<FString>& Args);

	/** Console command handles */
	static TArray<IConsoleObject*> ConsoleCommands;
};
