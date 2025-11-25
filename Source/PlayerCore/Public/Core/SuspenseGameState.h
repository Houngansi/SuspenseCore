// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "SuspenseGameState.generated.h"

// Перечисление состояний матча для Suspense
UENUM(BlueprintType)
enum class EMedComMatchState : uint8
{
	WaitingToStart,
	InProgress,
	Paused,
	WaitingPostMatch,
	LeavingMap,
	GameOver
};

/**
 * Класс GameState для Suspense
 * Управляет состоянием матча и публичными игровыми данными
 */
UCLASS()
class SUSPENSECORE_API ASuspenseGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ASuspenseGameState();

	// Получить текущее состояние матча
	UFUNCTION(BlueprintCallable, Category = "MedCom|Game")
	EMedComMatchState GetMedComMatchState() const { return MedComMatchState; }

	// Установить текущее состояние матча - только для сервера
	void SetMedComMatchState(EMedComMatchState NewState);

	// Событие при изменении состояния матча
	UFUNCTION(BlueprintImplementableEvent, Category = "MedCom|Game")
	void OnMedComMatchStateChanged(EMedComMatchState PreviousState, EMedComMatchState NewState);

	// Делегат для привязки кода C++ к изменениям состояния матча
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMatchStateChangedDelegate, EMedComMatchState /*PreviousState*/, EMedComMatchState /*NewState*/);
	FOnMatchStateChangedDelegate OnMatchStateChanged;

protected:
	// Текущее состояние матча
	UPROPERTY(ReplicatedUsing = OnRep_MedComMatchState, BlueprintReadOnly, Category = "MedCom|Game")
	EMedComMatchState MedComMatchState;

	// Обратный вызов при репликации состояния матча
	UFUNCTION()
	void OnRep_MedComMatchState(EMedComMatchState OldState);

public:
	// Настройка репликации
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};