// Copyright MedCom Team. All Rights Reserved.

#include "Core/MedComGameState.h"
#include "Net/UnrealNetwork.h"

AMedComGameState::AMedComGameState()
	: MedComMatchState(EMedComMatchState::WaitingToStart)
{
}

void AMedComGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
	DOREPLIFETIME(AMedComGameState, MedComMatchState);
}

void AMedComGameState::SetMedComMatchState(EMedComMatchState NewState)
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

void AMedComGameState::OnRep_MedComMatchState(EMedComMatchState OldState)
{
	// При репликации клиентам, вызываем событие
	OnMedComMatchStateChanged(OldState, MedComMatchState);
    
	// Отправка делегата для слушателей C++
	OnMatchStateChanged.Broadcast(OldState, MedComMatchState);
}