// MedComAsyncTaskRepositionCalculation.h
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "GameplayTagContainer.h"
#include "NavigationSystem.h"
#include "MedComAsyncTaskRepositionCalculation.generated.h"

class AMedComEnemyCharacter;
class UMedComAIMovementComponent;
class ACharacter;

/**
 * Результат операции расчета позиции для перепозиционирования
 */
USTRUCT(BlueprintType)
struct FRepositionCalculationResult
{
    GENERATED_BODY()

    /** Рассчитанная позиция для перемещения */
    UPROPERTY(BlueprintReadWrite, Category = "AI|Repositioning")
    FVector TargetPosition;

    /** Успешно ли выполнен расчет */
    UPROPERTY(BlueprintReadWrite, Category = "AI|Repositioning")
    bool bSuccess;

    /** Дополнительная информация о расчете */
    UPROPERTY(BlueprintReadWrite, Category = "AI|Repositioning")
    FString DebugInfo;

    FRepositionCalculationResult()
        : TargetPosition(FVector::ZeroVector)
        , bSuccess(false)
    {
    }
};

/**
 * Делегат, вызываемый при завершении расчета позиции
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRepositionTargetCalculated, const FRepositionCalculationResult&, Result);

/**
 * Асинхронная задача для расчета оптимальной позиции перепозиционирования без блокировки основного потока
 */
UCLASS()
class MEDCOMCORE_API UMedComAsyncTaskRepositionCalculation : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()

public:
    /** Делегат, вызываемый при успешном расчете позиции */
    UPROPERTY(BlueprintAssignable)
    FOnRepositionTargetCalculated OnCalculationComplete;

    /** Делегат, вызываемый при ошибке или невозможности расчета */
    UPROPERTY(BlueprintAssignable)
    FOnRepositionTargetCalculated OnCalculationFailed;

    /**
     * Создает и запускает асинхронную задачу расчета позиции для перепозиционирования
     * 
     * @param Enemy - Персонаж бота, для которого выполняется расчет
     * @param Target - Целевой персонаж (обычно игрок)
     * @param CurrentDistance - Текущая дистанция до цели
     * @param FireMode - Текущий режим стрельбы
     * @param DebugMode - Включить расширенную отладку
     * @return Экземпляр асинхронной задачи
     */
    UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"), Category = "AI|Tasks")
    static UMedComAsyncTaskRepositionCalculation* CalculateRepositionTargetAsync(
        AMedComEnemyCharacter* Enemy, 
        ACharacter* Target, 
        float CurrentDistance, 
        FGameplayTag FireMode,
        bool DebugMode = false);

    // Initializes the task with the required data
    void Initialize(
        AMedComEnemyCharacter* InEnemy,
        ACharacter* InTarget,
        float InCurrentDistance,
        FGameplayTag InFireMode,
        bool InDebugMode);

    // Begin UBlueprintAsyncActionBase interface
    virtual void Activate() override;
    // End UBlueprintAsyncActionBase interface

    // Cancels the task
    UFUNCTION(BlueprintCallable, Category = "AI|Tasks")
    void ExplicitCancel();

protected:
    /** Character reference */
    UPROPERTY()
    AMedComEnemyCharacter* EnemyCharacter;

    /** Target reference */
    UPROPERTY()
    ACharacter* TargetCharacter;

    /** Current distance value */
    float CurrentDistance;

    /** Current fire mode */
    FGameplayTag CurrentFireMode;

    /** Debug flag */
    bool bDebugMode;

private:
    /** Cached AI Movement component */
    UPROPERTY()
    UMedComAIMovementComponent* CachedAIMovementComponent;

    /** Parameters used for scoring positions */
    struct FRepositionParams
    {
        float OptimalDistance;
        float DistanceVariation;
        float MinTargetDistance;
        float MaxTargetDistance;
        float MinEnemyDistance;
        int32 RepositionAttemptsCount;
        float CoverDetectionRadius;
        bool bPreferCoverPositions;
    };
    FRepositionParams CalculationParams;

    // НОВОЕ: Кэшированные позиции врагов, собранные в Game Thread
    TArray<FVector> EnemyPositions;
    
    // НОВОЕ: Предварительный сбор данных в Game Thread перед запуском асинхронной задачи
    void CollectGameThreadData();

    // Worker function executed in another thread
    void ExecuteCalculation();

    // Call delegate with the result
    void FinishCalculation(const FRepositionCalculationResult& Result);

    // Find AI movement component
    UMedComAIMovementComponent* GetAIMovementComponent() const;

    // Check if position has cover nearby (simplified for async operation)
    bool IsPositionNearCover(const FVector& Position, float CoverRadius);

    // Check if target is visible from position
    bool CanSeeTargetFromPosition(const ACharacter* Target, const FVector& Position);

    // Project point to navigation with error handling
    FVector ProjectPointToNavigation(UNavigationSystemV1* NavSys, const FVector& Point, const FVector& OwnerLocation);

    // Find position away from other enemies using pre-collected positions
    FVector FindPositionAwayFromOtherEnemies(const FVector& BasePosition, float MinDistance);
};