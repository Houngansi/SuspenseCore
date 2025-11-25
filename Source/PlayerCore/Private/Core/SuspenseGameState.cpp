// Copyright Suspense Team. All Rights Reserved.

#include "Core/SuspenseGameState.h"
#include "Net/UnrealNetwork.h"

ASuspenseGameState::ASuspenseGameState()
	: MatchState(ESuspenseMatchState::WaitingToStart)
{
}

void ASuspenseGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASuspenseGameState, MatchState);
}

void ASuspenseGameState::SetMatchState(ESuspenseMatchState NewState)
{
	if (HasAuthority() && MatchState != NewState)
	{
		ESuspenseMatchState OldState = MatchState;
		MatchState = NewState;

		// Call event for Blueprint subclasses
		OnMatchStateChanged(OldState, MatchState);

		// Broadcast delegate for C++ listeners
		OnMatchStateChangedDelegate.Broadcast(OldState, MatchState);
	}
}

void ASuspenseGameState::OnRep_MatchState(ESuspenseMatchState OldState)
{
	// When replicated to clients, call event
	OnMatchStateChanged(OldState, MatchState);

	// Broadcast delegate for C++ listeners
	OnMatchStateChangedDelegate.Broadcast(OldState, MatchState);
}
