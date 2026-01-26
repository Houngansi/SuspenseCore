#pragma once

#include "CoreMinimal.h"
#include "Core/Enemy/FSM/MedComEnemyState.h"
#include "MedComDeathState.generated.h"

/**
 * Состояние смерти для врага.
 * Управляет процессом умирания врага и последующей обработкой (рагдолл, исчезновение и т.д.)
 */
UCLASS()
class MEDCOMCORE_API UMedComDeathState : public UMedComEnemyState
{
    GENERATED_BODY()
    
public:
    UMedComDeathState();
    
    // Переопределенные методы базового класса
    virtual void OnEnter(AMedComEnemyCharacter* Owner) override;
    virtual void OnExit(AMedComEnemyCharacter* Owner) override;
    virtual void ProcessTick(AMedComEnemyCharacter* Owner, float DeltaTime) override;
    virtual void OnTimerFired(AMedComEnemyCharacter* Owner, FName TimerName) override;
    
private:
    // Имена таймеров
    static const FName RagdollTimerName;
    static const FName DespawnTimerName;
    
    // Вспомогательные методы
    void EnableRagdoll(AMedComEnemyCharacter* Owner);
    void HandleDespawn(AMedComEnemyCharacter* Owner);
};