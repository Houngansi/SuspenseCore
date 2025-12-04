// SuspenseCoreInventorySerializer.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventorySerializationTypes.h"
#include "SuspenseCore/Save/SuspenseCoreSaveTypes.h"
#include "SuspenseCoreInventorySerializer.generated.h"

// Forward declarations
class USuspenseCoreInventoryComponent;
class ISuspenseCoreSaveRepository;

/**
 * USuspenseCoreInventorySerializer
 *
 * Handles serialization/deserialization of inventory data.
 * Integrates with SuspenseCore Save system.
 *
 * ARCHITECTURE:
 * - Converts between FSuspenseCoreItemInstance and save formats
 * - Supports JSON and binary serialization
 * - Integrates with FSuspenseCoreInventoryState for save system
 * - Handles version migration
 *
 * SAVE SYSTEM INTEGRATION:
 * Uses FSuspenseCoreInventoryState from SuspenseCoreSaveTypes.h
 * and converts to/from FSuspenseCoreSerializedInventory.
 */
UCLASS(BlueprintType)
class INVENTORYSYSTEM_API USuspenseCoreInventorySerializer : public UObject
{
	GENERATED_BODY()

public:
	USuspenseCoreInventorySerializer();

	//==================================================================
	// Component Serialization
	//==================================================================

	/**
	 * Serialize inventory component to save format.
	 * @param Component Inventory to serialize
	 * @param OutSaveState Save system compatible state
	 * @return true if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Serialization")
	static bool SerializeToSaveState(
		USuspenseCoreInventoryComponent* Component,
		FSuspenseCoreInventoryState& OutSaveState
	);

	/**
	 * Deserialize inventory from save format.
	 * @param SaveState Saved state
	 * @param Component Target inventory
	 * @return true if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Serialization")
	static bool DeserializeFromSaveState(
		const FSuspenseCoreInventoryState& SaveState,
		USuspenseCoreInventoryComponent* Component
	);

	/**
	 * Serialize to internal format.
	 * @param Component Inventory to serialize
	 * @param OutData Serialized data
	 * @return true if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Serialization")
	static bool SerializeInventory(
		USuspenseCoreInventoryComponent* Component,
		FSuspenseCoreSerializedInventory& OutData
	);

	/**
	 * Deserialize from internal format.
	 * @param Data Serialized data
	 * @param Component Target inventory
	 * @return true if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Serialization")
	static bool DeserializeInventory(
		const FSuspenseCoreSerializedInventory& Data,
		USuspenseCoreInventoryComponent* Component
	);

	//==================================================================
	// Format Conversion
	//==================================================================

	/**
	 * Convert FSuspenseCoreItemInstance to FSuspenseCoreRuntimeItem.
	 * @param Instance Source instance
	 * @return Save-compatible runtime item
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Serialization")
	static FSuspenseCoreRuntimeItem InstanceToRuntimeItem(const FSuspenseCoreItemInstance& Instance);

	/**
	 * Convert FSuspenseCoreRuntimeItem to FSuspenseCoreItemInstance.
	 * @param RuntimeItem Source runtime item
	 * @return Inventory item instance
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Serialization")
	static FSuspenseCoreItemInstance RuntimeItemToInstance(const FSuspenseCoreRuntimeItem& RuntimeItem);

	/**
	 * Convert FSuspenseCoreItemInstance to FSuspenseCoreSerializedItem.
	 * @param Instance Source instance
	 * @return Serialized item
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Serialization")
	static FSuspenseCoreSerializedItem InstanceToSerializedItem(const FSuspenseCoreItemInstance& Instance);

	/**
	 * Convert FSuspenseCoreSerializedItem to FSuspenseCoreItemInstance.
	 * @param SerializedItem Source serialized item
	 * @return Item instance
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Serialization")
	static FSuspenseCoreItemInstance SerializedItemToInstance(const FSuspenseCoreSerializedItem& SerializedItem);

	//==================================================================
	// JSON Serialization
	//==================================================================

	/**
	 * Serialize inventory to JSON string.
	 * @param Component Inventory to serialize
	 * @param OutJson JSON string output
	 * @return true if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Serialization")
	static bool SerializeToJson(
		USuspenseCoreInventoryComponent* Component,
		FString& OutJson
	);

	/**
	 * Deserialize inventory from JSON string.
	 * @param Json JSON string
	 * @param Component Target inventory
	 * @return true if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Serialization")
	static bool DeserializeFromJson(
		const FString& Json,
		USuspenseCoreInventoryComponent* Component
	);

	/**
	 * Serialize single item to JSON.
	 * @param Instance Item instance
	 * @param OutJson JSON string output
	 * @return true if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Serialization")
	static bool ItemToJson(const FSuspenseCoreItemInstance& Instance, FString& OutJson);

	/**
	 * Deserialize single item from JSON.
	 * @param Json JSON string
	 * @param OutInstance Item instance output
	 * @return true if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Serialization")
	static bool JsonToItem(const FString& Json, FSuspenseCoreItemInstance& OutInstance);

	//==================================================================
	// Binary Serialization
	//==================================================================

	/**
	 * Serialize inventory to binary.
	 * @param Component Inventory to serialize
	 * @param OutBytes Binary data output
	 * @return true if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Serialization")
	static bool SerializeToBinary(
		USuspenseCoreInventoryComponent* Component,
		TArray<uint8>& OutBytes
	);

	/**
	 * Deserialize inventory from binary.
	 * @param Bytes Binary data
	 * @param Component Target inventory
	 * @return true if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Serialization")
	static bool DeserializeFromBinary(
		const TArray<uint8>& Bytes,
		USuspenseCoreInventoryComponent* Component
	);

	//==================================================================
	// Version Migration
	//==================================================================

	/**
	 * Migrate serialized data to current version.
	 * @param Data Data to migrate (modified in place)
	 * @param OutMigration Migration info
	 * @return true if migration successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Serialization")
	static bool MigrateToCurrentVersion(
		UPARAM(ref) FSuspenseCoreSerializedInventory& Data,
		FSuspenseCoreInventoryMigration& OutMigration
	);

	/**
	 * Get current serialization version.
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Serialization")
	static int32 GetCurrentVersion() { return FSuspenseCoreSerializedInventory::CURRENT_VERSION; }

	//==================================================================
	// Validation
	//==================================================================

	/**
	 * Validate serialized data.
	 * @param Data Data to validate
	 * @param OutErrors Error messages
	 * @return true if valid
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Serialization")
	static bool ValidateSerializedData(
		const FSuspenseCoreSerializedInventory& Data,
		TArray<FString>& OutErrors
	);

	/**
	 * Calculate diff between two serialized states.
	 * @param OldState Previous state
	 * @param NewState Current state
	 * @param OutDiff Difference output
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Serialization")
	static void CalculateDiff(
		const FSuspenseCoreSerializedInventory& OldState,
		const FSuspenseCoreSerializedInventory& NewState,
		FSuspenseCoreInventoryDiff& OutDiff
	);
};
