// MedComIdleState.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Enemy/FSM/MedComEnemyState.h"
#include "MedComIdleState.generated.h"

UCLASS()
class MEDCOMCORE_API UMedComIdleState : public UMedComEnemyState
{
	GENERATED_BODY()

public:
	UMedComIdleState();

	virtual void OnEnter(AMedComEnemyCharacter* Owner) override;
	virtual void OnExit(AMedComEnemyCharacter* Owner) override;
	virtual void OnEvent(AMedComEnemyCharacter* Owner, EEnemyEvent Event, AActor* Instigator) override;
	virtual void OnTimerFired(AMedComEnemyCharacter* Owner, FName TimerName) override;
	virtual void ProcessTick(AMedComEnemyCharacter* Owner, float DeltaTime) override;

protected:
	// Метод поворота головы
	void RotateHead(AMedComEnemyCharacter* Owner, float DeltaTime);
    
	// Начало осмотра в определенном направлении
	void StartLookAt(AMedComEnemyCharacter* Owner, const FVector& Direction);
    
	// Запуск анимации осмотра
	void TriggerLookAnimation(AMedComEnemyCharacter* Owner);

private:
	// Текущий угол поворота
	float CurrentLookAngle;
    
	// Целевой угол поворота
	float TargetLookAngle;
    
	// Направление поворота (1 или -1)
	float LookDirection;
    
	// Текущее время в состоянии Idle
	float CurrentIdleTime;
    
	// Максимальное время в Idle перед переходом в Patrol
	float MaxIdleTime;
    
	// Интервал между осмотрами
	float LookInterval;
    
	// Скорость поворота головы
	float LookRotationSpeed;
    
	// Максимальный угол поворота головы
	float MaxLookAngle;
    
	// Текущее направление взгляда
	FVector CurrentLookDirection;
    
	// Флаг для отслеживания отправки события таймаута
	bool bIdleTimeoutSent;
};