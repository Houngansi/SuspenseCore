// SuspenseCoreFileSaveRepository.h
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCoreSaveInterfaces.h"
#include "SuspenseCoreFileSaveRepository.generated.h"

/**
 * USuspenseCoreFileSaveRepository
 *
 * File-based save repository.
 * Saves to: [Project]/Saved/SaveGames/[PlayerId]/Slot_X.sav
 *
 * Features:
 * - JSON serialization for readability
 * - Async operations via AsyncTask
 * - Auto-save and QuickSave slots
 * - Header caching for fast UI display
 */
UCLASS(BlueprintType)
class BRIDGESYSTEM_API USuspenseCoreFileSaveRepository : public UObject, public ISuspenseCoreSaveRepository
{
	GENERATED_BODY()

public:
	USuspenseCoreFileSaveRepository();

	// ═══════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════

	/**
	 * Initialize repository.
	 * @param InBasePath - Custom base path (empty = default)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	void Initialize(const FString& InBasePath = TEXT(""));

	/**
	 * Get base path.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	FString GetBasePath() const { return BasePath; }

	// ═══════════════════════════════════════════════════════════════
	// ISuspenseCoreSaveRepository Implementation
	// ═══════════════════════════════════════════════════════════════

	virtual ESuspenseCoreSaveResult SaveToSlot(
		const FString& PlayerId,
		int32 SlotIndex,
		const FSuspenseCoreSaveData& SaveData) override;

	virtual ESuspenseCoreSaveResult LoadFromSlot(
		const FString& PlayerId,
		int32 SlotIndex,
		FSuspenseCoreSaveData& OutSaveData) override;

	virtual ESuspenseCoreSaveResult DeleteSlot(
		const FString& PlayerId,
		int32 SlotIndex) override;

	virtual bool SlotExists(
		const FString& PlayerId,
		int32 SlotIndex) override;

	virtual void GetSaveHeaders(
		const FString& PlayerId,
		TArray<FSuspenseCoreSaveHeader>& OutHeaders) override;

	virtual bool GetSlotHeader(
		const FString& PlayerId,
		int32 SlotIndex,
		FSuspenseCoreSaveHeader& OutHeader) override;

	virtual int32 GetMaxSlots() const override { return MaxSaveSlots; }

	virtual void SaveToSlotAsync(
		const FString& PlayerId,
		int32 SlotIndex,
		const FSuspenseCoreSaveData& SaveData,
		FOnSuspenseCoreSaveComplete OnComplete) override;

	virtual void LoadFromSlotAsync(
		const FString& PlayerId,
		int32 SlotIndex,
		FOnSuspenseCoreLoadComplete OnComplete) override;

	virtual FString GetRepositoryType() const override { return TEXT("FileRepository"); }
	virtual bool IsAvailable() const override { return true; }

	// ═══════════════════════════════════════════════════════════════
	// SPECIAL SLOTS
	// ═══════════════════════════════════════════════════════════════

	/** Auto-save slot index */
	static constexpr int32 AUTOSAVE_SLOT = 100;

	/** Quick-save slot index */
	static constexpr int32 QUICKSAVE_SLOT = 101;

protected:
	/** Base path for saves */
	FString BasePath;

	/** Maximum regular save slots */
	int32 MaxSaveSlots = 10;

	/** Header cache */
	TMap<FString, TMap<int32, FSuspenseCoreSaveHeader>> HeaderCache;

	/** Lock for thread safety */
	mutable FCriticalSection RepositoryLock;

	// ═══════════════════════════════════════════════════════════════
	// INTERNAL
	// ═══════════════════════════════════════════════════════════════

	/**
	 * Get full file path for a save slot.
	 */
	FString GetSlotFilePath(const FString& PlayerId, int32 SlotIndex) const;

	/**
	 * Get player directory path.
	 */
	FString GetPlayerDirectory(const FString& PlayerId) const;

	/**
	 * Serialize save data to JSON string.
	 */
	bool SerializeToJson(const FSuspenseCoreSaveData& Data, FString& OutJson) const;

	/**
	 * Deserialize save data from JSON string.
	 */
	bool DeserializeFromJson(const FString& Json, FSuspenseCoreSaveData& OutData) const;

	/**
	 * Ensure player directory exists.
	 */
	bool EnsurePlayerDirectory(const FString& PlayerId) const;

	/**
	 * Update header cache.
	 */
	void UpdateHeaderCache(const FString& PlayerId, int32 SlotIndex, const FSuspenseCoreSaveHeader& Header);

	/**
	 * Remove from header cache.
	 */
	void RemoveFromHeaderCache(const FString& PlayerId, int32 SlotIndex);
};

