// SuspenseCoreMapTransitionSubsystem.h
// SuspenseCore - Clean Architecture BridgeSystem
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SuspenseCoreMapTransitionSubsystem.generated.h"

/**
 * Data stored during map transition
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreTransitionData
{
	GENERATED_BODY()

	/** Current player ID */
	UPROPERTY(BlueprintReadWrite)
	FString PlayerId;

	/** Source map name */
	UPROPERTY(BlueprintReadWrite)
	FName SourceMapName;

	/** Target map name */
	UPROPERTY(BlueprintReadWrite)
	FName TargetMapName;

	/** Transition reason/type */
	UPROPERTY(BlueprintReadWrite)
	FString TransitionReason;

	/** Custom data (JSON string) */
	UPROPERTY(BlueprintReadWrite)
	FString CustomData;

	/** Timestamp when transition started */
	UPROPERTY(BlueprintReadWrite)
	FDateTime TransitionTime;

	FSuspenseCoreTransitionData()
		: TransitionTime(FDateTime::Now())
	{
	}

	bool IsValid() const
	{
		return !PlayerId.IsEmpty();
	}
};

/**
 * USuspenseCoreMapTransitionSubsystem
 *
 * GameInstance subsystem that persists data during map transitions.
 * Since GameInstance survives level changes, this is the proper place
 * to store transition state.
 *
 * Usage:
 * - Before calling OpenLevel, set transition data
 * - In new level's GameMode, retrieve transition data
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreMapTransitionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════════════════════════════════
	// SUBSYSTEM INTERFACE
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ═══════════════════════════════════════════════════════════════════════════
	// STATIC ACCESS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Get subsystem from world context */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Transition", meta = (WorldContext = "WorldContextObject"))
	static USuspenseCoreMapTransitionSubsystem* Get(const UObject* WorldContextObject);

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API - TRANSITION DATA
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Set transition data before map change.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Transition")
	void SetTransitionData(const FSuspenseCoreTransitionData& Data);

	/**
	 * Get current transition data.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Transition")
	FSuspenseCoreTransitionData GetTransitionData() const { return TransitionData; }

	/**
	 * Clear transition data.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Transition")
	void ClearTransitionData();

	/**
	 * Check if there is valid transition data.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Transition")
	bool HasTransitionData() const { return TransitionData.IsValid(); }

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API - CONVENIENCE METHODS
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Set current player ID (most common use case).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Transition")
	void SetCurrentPlayerId(const FString& PlayerId);

	/**
	 * Get current player ID.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Transition")
	FString GetCurrentPlayerId() const { return TransitionData.PlayerId; }

	/**
	 * Prepare transition to game map.
	 * Sets up all necessary data and opens the level.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Transition")
	void TransitionToGameMap(const FString& PlayerId, FName GameMapName);

	/**
	 * Prepare transition to main menu.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Transition")
	void TransitionToMainMenu(FName MainMenuMapName);

	// ═══════════════════════════════════════════════════════════════════════════
	// DELEGATES
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when transition data is set */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTransitionDataSet, const FSuspenseCoreTransitionData&, Data);

	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Transition")
	FOnTransitionDataSet OnTransitionDataSet;

	/** Called before map transition begins */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMapTransitionBegin, FName, SourceMap, FName, TargetMap);

	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Transition")
	FOnMapTransitionBegin OnMapTransitionBegin;

protected:
	/** Current transition data */
	UPROPERTY()
	FSuspenseCoreTransitionData TransitionData;
};
