#pragma once

#include "CoreMinimal.h"
#include "Core/Enemy/FSM/MedComEnemyState.h"
#include "AIController.h"
#include "MedComReturnState.generated.h"

UCLASS()
class MEDCOMCORE_API UMedComReturnState : public UMedComEnemyState
{
    GENERATED_BODY()

public:
    UMedComReturnState();
    
    virtual void OnEnter(AMedComEnemyCharacter* Owner) override;
    virtual void OnExit(AMedComEnemyCharacter* Owner) override;
    virtual void ProcessTick(AMedComEnemyCharacter* Owner, float DeltaTime) override;
    virtual void OnEvent(AMedComEnemyCharacter* Owner, EEnemyEvent Event, AActor* Instigator) override;
    virtual void OnTimerFired(AMedComEnemyCharacter* Owner, FName TimerName) override;
    
    // Статический метод для получения последней точки возврата
    UFUNCTION(BlueprintCallable, Category = "AI|States")
    static FVector GetLastReturnPoint();
    
    // Статический метод для установки последней точки возврата
    UFUNCTION(BlueprintCallable, Category = "AI|States")
    static void SetLastReturnPoint(const FVector& Point);

protected:
    // Статическое хранилище для последней точки возврата
    static FVector LastReturnPoint;
    
    // Константы для имён таймеров
    static const FName PathUpdateTimerName;
    static const FName ReturnCompleteTimerName;
    static const FName StuckCheckTimerName;
    
    // Метод для получения оптимальной точки возврата
    FVector GetReturnPoint(AMedComEnemyCharacter* Owner) const;
    
    // Обновление пути к точке возврата
    bool UpdateReturnPath(AMedComEnemyCharacter* Owner);
    
    // Проверка, достигли ли точки возврата
    bool IsCloseToReturnPoint(AMedComEnemyCharacter* Owner) const;
    
    // Метод для аварийного перехода при застревании
    bool CheckForStuck(AMedComEnemyCharacter* Owner);
    
    // Вспомогательный метод логирования с отладочной визуализацией
    void LogReturnState(AMedComEnemyCharacter* Owner, const FString& Message, 
                       ELogVerbosity::Type Verbosity = ELogVerbosity::Log);

    /**
    * Надежно отправляет событие завершения возврата.
    * Проверяет наличие FSM компонента и устанавливает данные перехода.
    * @param Owner Персонаж-владелец состояния
    */
    void SendReturnCompleteEvent(AMedComEnemyCharacter* Owner);
private:
    // Настройки поведения из конфигурации
    UPROPERTY()
    float ReturnSpeed;
    
    UPROPERTY()
    float AcceptanceRadius;
    
    UPROPERTY()
    float PathUpdateInterval;
    
    UPROPERTY()
    float StuckCheckInterval;
    
    UPROPERTY()
    float MinMovementThreshold;
    
    UPROPERTY()
    int32 MaxStuckCount;
    
    UPROPERTY()
    float ForceCompleteDistance;
    
    // Счетчики и состояния
    int32 StuckCounter;
    FVector PreviousLocation;
    FVector ReturnLocation;
    bool bReachedReturnPoint;
    bool bProcessingCompletion;
    
    // Временное хранилище контроллера для оптимизации
    TWeakObjectPtr<AAIController> CachedController;
    
};