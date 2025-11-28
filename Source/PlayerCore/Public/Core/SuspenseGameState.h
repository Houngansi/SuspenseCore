// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "SuspenseGameState.generated.h"

// Match state enumeration for Suspense
UENUM(BlueprintType)
enum class ESuspenseMatchState : uint8
{
	WaitingToStart,
	InProgress,
	Paused,
	WaitingPostMatch,
	LeavingMap,
	GameOver
};

/**
 * GameState class for Suspense
 * Manages match state and public game data
 */
UCLASS()
class PLAYERCORE_API ASuspenseGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ASuspenseGameState();

	// Get current match state
	UFUNCTION(BlueprintCallable, Category = "Suspense|Game")
	ESuspenseMatchState GetMatchState() const { return MatchState; }

	// Set current match state - server only
	void SetMatchState(ESuspenseMatchState NewState);

	// Event when match state changes
	UFUNCTION(BlueprintImplementableEvent, Category = "Suspense|Game")
	void OnMatchStateChanged(ESuspenseMatchState PreviousState, ESuspenseMatchState NewState);

	// Delegate for binding C++ code to match state changes
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMatchStateChangedDelegate, ESuspenseMatchState /*PreviousState*/, ESuspenseMatchState /*NewState*/);
	FOnMatchStateChangedDelegate OnMatchStateChangedDelegate;

protected:
	// Current match state
	UPROPERTY(ReplicatedUsing = OnRep_MatchState, BlueprintReadOnly, Category = "Suspense|Game")
	ESuspenseMatchState MatchState;

	// Callback when match state replicates
	UFUNCTION()
	void OnRep_MatchState(ESuspenseMatchState OldState);

public:
	// Setup replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
