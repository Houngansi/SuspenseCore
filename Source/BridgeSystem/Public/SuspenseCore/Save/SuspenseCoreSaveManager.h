// SuspenseCoreSaveManager.h
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SuspenseCoreSaveTypes.h"
#include "SuspenseCoreSaveInterfaces.h"
#include "SuspenseCoreSaveManager.generated.h"

class USuspenseCoreFileSaveRepository;

// ═══════════════════════════════════════════════════════════════════════════════
// DELEGATES
// ═══════════════════════════════════════════════════════════════════════════════

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSuspenseCoreSaveStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSuspenseCoreSaveCompleted, bool, bSuccess, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSuspenseCoreLoadStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSuspenseCoreLoadCompleted, bool, bSuccess, const FString&, ErrorMessage);

/**
 * USuspenseCoreSaveManager
 *
 * GameInstance Subsystem for managing game saves.
 * Main API for save/load operations.
 *
 * Features:
 * - Quick Save/Load (F5/F9)
 * - Auto-save with configurable interval
 * - Multiple save slots
 * - Async operations
 * - Character state collection
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreSaveManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════════════════════
	// LIFECYCLE
	// ═══════════════════════════════════════════════════════════════

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Static accessor.
	 */
	static USuspenseCoreSaveManager* Get(const UObject* WorldContextObject);

	// ═══════════════════════════════════════════════════════════════
	// PLAYER MANAGEMENT
	// ═══════════════════════════════════════════════════════════════

	/**
	 * Set current player for save operations.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	void SetCurrentPlayer(const FString& PlayerId);

	/**
	 * Get current player ID.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	FString GetCurrentPlayerId() const { return CurrentPlayerId; }

	/**
	 * Check if player is set.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	bool HasCurrentPlayer() const { return !CurrentPlayerId.IsEmpty(); }

	// ═══════════════════════════════════════════════════════════════
	// QUICK SAVE/LOAD
	// ═══════════════════════════════════════════════════════════════

	/**
	 * Quick save to dedicated slot (F5).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	void QuickSave();

	/**
	 * Quick load from dedicated slot (F9).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	void QuickLoad();

	/**
	 * Check if quick save exists.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	bool HasQuickSave() const;

	// ═══════════════════════════════════════════════════════════════
	// SLOT MANAGEMENT
	// ═══════════════════════════════════════════════════════════════

	/**
	 * Save to a specific slot.
	 * @param SlotIndex - Slot index (0-based)
	 * @param SlotName - Display name for the slot
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	void SaveToSlot(int32 SlotIndex, const FString& SlotName = TEXT(""));

	/**
	 * Load from a specific slot.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	void LoadFromSlot(int32 SlotIndex);

	/**
	 * Delete a save slot.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	void DeleteSlot(int32 SlotIndex);

	/**
	 * Get all save slot headers for UI.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	TArray<FSuspenseCoreSaveHeader> GetAllSlotHeaders();

	/**
	 * Check if slot exists.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	bool SlotExists(int32 SlotIndex) const;

	/**
	 * Get maximum slot count.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	int32 GetMaxSlots() const;

	// ═══════════════════════════════════════════════════════════════
	// AUTO-SAVE
	// ═══════════════════════════════════════════════════════════════

	/**
	 * Enable/disable auto-save.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	void SetAutoSaveEnabled(bool bEnabled);

	/**
	 * Check if auto-save is enabled.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	bool IsAutoSaveEnabled() const { return bAutoSaveEnabled; }

	/**
	 * Set auto-save interval.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	void SetAutoSaveInterval(float IntervalSeconds);

	/**
	 * Get auto-save interval.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	float GetAutoSaveInterval() const { return AutoSaveInterval; }

	/**
	 * Trigger auto-save manually (e.g., on checkpoint).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	void TriggerAutoSave();

	/**
	 * Check if auto-save exists.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	bool HasAutoSave() const;

	// ═══════════════════════════════════════════════════════════════
	// STATE COLLECTION
	// ═══════════════════════════════════════════════════════════════

	/**
	 * Collect current game state for saving.
	 * Override CollectCharacterState() in Character to customize.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	FSuspenseCoreSaveData CollectCurrentGameState();

	/**
	 * Apply loaded save data to game.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	void ApplyLoadedState(const FSuspenseCoreSaveData& SaveData);

	/**
	 * Set profile data (from login/registration).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	void SetProfileData(const FSuspenseCorePlayerData& ProfileData);

	/**
	 * Get current profile data.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	FSuspenseCorePlayerData GetProfileData() const { return CachedProfileData; }

	// ═══════════════════════════════════════════════════════════════
	// STATUS
	// ═══════════════════════════════════════════════════════════════

	/**
	 * Check if save operation is in progress.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	bool IsSaving() const { return bIsSaving; }

	/**
	 * Check if load operation is in progress.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	bool IsLoading() const { return bIsLoading; }

	// ═══════════════════════════════════════════════════════════════
	// EVENTS
	// ═══════════════════════════════════════════════════════════════

	/** Called when save starts */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Save")
	FOnSuspenseCoreSaveStarted OnSaveStarted;

	/** Called when save completes */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Save")
	FOnSuspenseCoreSaveCompleted OnSaveCompleted;

	/** Called when load starts */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Save")
	FOnSuspenseCoreLoadStarted OnLoadStarted;

	/** Called when load completes */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Save")
	FOnSuspenseCoreLoadCompleted OnLoadCompleted;

protected:
	// ═══════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════

	/** Current player ID */
	UPROPERTY()
	FString CurrentPlayerId;

	/** Cached profile data */
	UPROPERTY()
	FSuspenseCorePlayerData CachedProfileData;

	/** Save repository */
	UPROPERTY()
	USuspenseCoreFileSaveRepository* SaveRepository;

	/** Auto-save enabled */
	bool bAutoSaveEnabled = true;

	/** Auto-save interval in seconds */
	float AutoSaveInterval = 300.0f; // 5 minutes

	/** Auto-save timer */
	FTimerHandle AutoSaveTimerHandle;

	/** Is save in progress */
	bool bIsSaving = false;

	/** Is load in progress */
	bool bIsLoading = false;

	/** Last save timestamp */
	FDateTime LastSaveTime;

	/** Play time counter */
	double SessionStartTime = 0.0;

	// ═══════════════════════════════════════════════════════════════
	// INTERNAL
	// ═══════════════════════════════════════════════════════════════

	/** Setup auto-save timer */
	void SetupAutoSaveTimer();

	/** Stop auto-save timer */
	void StopAutoSaveTimer();

	/** Auto-save timer callback */
	void OnAutoSaveTimer();

	/** Internal save implementation */
	void SaveToSlotInternal(int32 SlotIndex, const FString& SlotName, bool bIsAutoSave);

	/** Internal load implementation */
	void LoadFromSlotInternal(int32 SlotIndex);

	/** Handle save complete */
	void OnSaveCompleteInternal(ESuspenseCoreSaveResult Result, const FString& ErrorMessage);

	/** Handle load complete */
	void OnLoadCompleteInternal(ESuspenseCoreSaveResult Result, const FSuspenseCoreSaveData& Data, const FString& ErrorMessage);

	/** Collect character state from current pawn */
	FSuspenseCoreCharacterState CollectCharacterState();

	/** Get current map name */
	FName GetCurrentMapName() const;

	/** Calculate play time */
	int64 GetTotalPlayTime() const;
};
