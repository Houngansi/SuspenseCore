#pragma once

#include "CoreMinimal.h"
#include "Core/Enemy/FSM/MedComEnemyState.h"
#include "Navigation/PathFollowingComponent.h"
#include "MedComPatrolState.generated.h"

class AAIController;
class AMedComEnemyCharacter;

/**
 * Состояние «Patrol» – бот перемещается между точками патрулирования.
 * • автоматически создаёт маршрут патрулирования вокруг начальной точки
 * • поддерживает цикличное и двунаправленное патрулирование
 * • реагирует на обнаружение игрока переходом в Chase
 */
UCLASS()
class MEDCOMCORE_API UMedComPatrolState : public UMedComEnemyState
{
    GENERATED_BODY()

public:
    UMedComPatrolState();

    // ===== MedComEnemyState overrides =====
    virtual void OnEnter(AMedComEnemyCharacter* Owner) override;
    virtual void OnExit(AMedComEnemyCharacter* Owner) override;
    virtual void ProcessTick(AMedComEnemyCharacter* Owner, float DeltaTime) override;
    virtual void OnEvent(AMedComEnemyCharacter* Owner, EEnemyEvent Event, AActor* Instigator) override;

    // Статическая переменная для хранения последней точки патрулирования
    static FVector LastPatrolPoint;

    // Метод для получения массива точек патрулирования
    const TArray<FVector>& GetPatrolPoints() const { return PatrolPoints; }
    
    // Сеттер для сохранения последней точки
    static void SetLastPatrolPoint(const FVector& Point) { LastPatrolPoint = Point; }
    
private:
    /** Создаёт и возвращает точки патрулирования */
    TArray<FVector> GeneratePatrolPoints(AMedComEnemyCharacter* Owner);
    
    /** Начинает движение к следующей точке патрулирования */
    void MoveToNextPoint(AMedComEnemyCharacter* Owner);
    
    /** Обработка завершения движения */
    UFUNCTION()
    void OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result);
    
    /** Настройка компонента движения */
    void SetupMovementComponent(AMedComEnemyCharacter* Owner) const;
    
    /** Проверяет необходимость обновления пути */
    bool NeedsPathRefresh(const AMedComEnemyCharacter* Owner) const;
    
    /** Реализует функцию "смотреть по сторонам" при патрулировании */
    void LookAround(AMedComEnemyCharacter* Owner, float DeltaTime);

    /** Кеш контроллера */
    TWeakObjectPtr<AAIController> CachedController;
    
    /** Точки патрулирования */
    TArray<FVector> PatrolPoints;
    
    /** Текущий индекс точки */
    int32 CurrentPointIndex;
    
    /** Направление патрулирования (1 или -1) */
    int32 PatrolDirection;

    /** — Параметры из DataAsset / StateParams — */
    float PatrolSpeed;                // «PatrolSpeed» (деф. 300)
    float AcceptanceRadius;           // «AcceptanceRadius» (деф. 100)
    bool bLoopPatrol;                 // «LoopPatrol» (деф. true)
    bool bUseRandomPatrol;            // «UseRandomPatrol» (деф. false)
    float MaxPatrolDistance;          // «MaxPatrolDistance» (деф. 1000)
    float RepathDistance;             // «RepathDistance» (деф. 100)
    float PatrolRotationRate;         // «PatrolRotationRate» (деф. 300)
    bool bLookAroundWhilePatrolling;  // «LookAroundWhilePatrolling» (деф. false)
    float PatrolLookAroundInterval;   // «PatrolLookAroundInterval» (деф. 3.0)
    float PatrolLookAroundDuration;   // «PatrolLookAroundDuration» (деф. 1.5)
    int32 NumPatrolPoints;            // «NumPatrolPoints» (деф. 4)

    /** — Runtime — */
    float LastMoveRequestTime;
    float LastLookAroundTime;
    FRotator TargetLookRotation;
    bool bIsLookingAround;
    FTimerHandle RetryMoveTimerHandle;
    
    UPROPERTY()
    FVector LastPathGoal;
};