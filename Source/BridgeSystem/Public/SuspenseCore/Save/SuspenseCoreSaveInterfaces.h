// SuspenseCoreSaveInterfaces.h
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SuspenseCoreSaveTypes.h"
#include "SuspenseCoreSaveInterfaces.generated.h"

// ═══════════════════════════════════════════════════════════════════════════════
// DELEGATES
// ═══════════════════════════════════════════════════════════════════════════════

DECLARE_DELEGATE_TwoParams(FOnSuspenseCoreSaveComplete, ESuspenseCoreSaveResult /*Result*/, const FString& /*ErrorMessage*/);
DECLARE_DELEGATE_ThreeParams(FOnSuspenseCoreLoadComplete, ESuspenseCoreSaveResult /*Result*/, const FSuspenseCoreSaveData& /*Data*/, const FString& /*ErrorMessage*/);

// ═══════════════════════════════════════════════════════════════════════════════
// SAVE REPOSITORY INTERFACE
// ═══════════════════════════════════════════════════════════════════════════════

UINTERFACE(MinimalAPI, BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class USuspenseCoreSaveRepository : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreSaveRepository
 *
 * Interface for save data storage backends.
 * Abstracts file, cloud, or database storage.
 */
class BRIDGESYSTEM_API ISuspenseCoreSaveRepository
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════════════════════
	// SYNC OPERATIONS
	// ═══════════════════════════════════════════════════════════════

	/**
	 * Save data to a slot.
	 * @param PlayerId - Player identifier
	 * @param SlotIndex - Slot index (0-based)
	 * @param SaveData - Data to save
	 * @return Success or failure
	 */
	virtual ESuspenseCoreSaveResult SaveToSlot(
		const FString& PlayerId,
		int32 SlotIndex,
		const FSuspenseCoreSaveData& SaveData) = 0;

	/**
	 * Load data from a slot.
	 * @param PlayerId - Player identifier
	 * @param SlotIndex - Slot index
	 * @param OutSaveData - Loaded data
	 * @return Success or failure
	 */
	virtual ESuspenseCoreSaveResult LoadFromSlot(
		const FString& PlayerId,
		int32 SlotIndex,
		FSuspenseCoreSaveData& OutSaveData) = 0;

	/**
	 * Delete a save slot.
	 */
	virtual ESuspenseCoreSaveResult DeleteSlot(
		const FString& PlayerId,
		int32 SlotIndex) = 0;

	/**
	 * Check if slot exists.
	 */
	virtual bool SlotExists(
		const FString& PlayerId,
		int32 SlotIndex) = 0;

	// ═══════════════════════════════════════════════════════════════
	// METADATA
	// ═══════════════════════════════════════════════════════════════

	/**
	 * Get headers for all save slots (for UI display).
	 */
	virtual void GetSaveHeaders(
		const FString& PlayerId,
		TArray<FSuspenseCoreSaveHeader>& OutHeaders) = 0;

	/**
	 * Get header for a specific slot.
	 */
	virtual bool GetSlotHeader(
		const FString& PlayerId,
		int32 SlotIndex,
		FSuspenseCoreSaveHeader& OutHeader) = 0;

	/**
	 * Get maximum number of save slots.
	 */
	virtual int32 GetMaxSlots() const = 0;

	// ═══════════════════════════════════════════════════════════════
	// ASYNC OPERATIONS
	// ═══════════════════════════════════════════════════════════════

	/**
	 * Async save operation.
	 */
	virtual void SaveToSlotAsync(
		const FString& PlayerId,
		int32 SlotIndex,
		const FSuspenseCoreSaveData& SaveData,
		FOnSuspenseCoreSaveComplete OnComplete) = 0;

	/**
	 * Async load operation.
	 */
	virtual void LoadFromSlotAsync(
		const FString& PlayerId,
		int32 SlotIndex,
		FOnSuspenseCoreLoadComplete OnComplete) = 0;

	// ═══════════════════════════════════════════════════════════════
	// INFO
	// ═══════════════════════════════════════════════════════════════

	/**
	 * Get repository type name.
	 */
	virtual FString GetRepositoryType() const = 0;

	/**
	 * Check if repository is available.
	 */
	virtual bool IsAvailable() const = 0;
};

