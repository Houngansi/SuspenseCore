// Copyright Suspense Team. All Rights Reserved.

#include "Core/SuspenseGameState.h"
#include "Net/UnrealNetwork.h"

ASuspenseGameState::ASuspenseGameState()
	: MedComMatchState(EMedComMatchState::WaitingToStart)
{
}

void ASuspenseGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
	DOREPLIFETIME(ASuspenseGameState, MedComMatchState);
}

void ASuspenseGameState::SetMedComMatchState(EMedComMatchState NewState)
{
	if (HasAuthority() && MedComMatchState != NewState)
	{
		EMedComMatchState OldState = MedComMatchState;
		MedComMatchState = NewState;
        
		// Вызов события для подклассов Blueprint
		OnMedComMatchStateChanged(OldState, MedComMatchState);
        
		// Отправка делегата для слушателей C++
		OnMatchStateChanged.Broadcast(OldState, MedComMatchState);
	}
}

void ASuspenseGameState::OnRep_MedComMatchState(EMedComMatchState OldState)
{
	// При репликации клиентам, вызываем событие
	OnMedComMatchStateChanged(OldState, MedComMatchState);
    
	// Отправка делегата для слушателей C++
	OnMatchStateChanged.Broadcast(OldState, MedComMatchState);
}