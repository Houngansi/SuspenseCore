#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UObject/ObjectSaveContext.h"
#include "Core/Enemy/FSM/MedComEnemyFSMComponent.h"
#include "EnemyBehaviorDataAsset.generated.h"

// Forward declarations
class UMedComEnemyState;
enum class EEnemyEvent : uint8;

/** Структура для описания перехода между состояниями */
USTRUCT(BlueprintType)
struct FStateTransition
{
    GENERATED_BODY()

    /** Событие, вызывающее переход */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FSM")
    EEnemyEvent TriggerEvent;

    /** Целевое состояние */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FSM")
    FName TargetState;
    
    /** Задержка перед переходом (если нужна) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FSM", meta = (ClampMin = "0.0"))
    float Delay = 0.0f;

    FStateTransition() : TriggerEvent(EEnemyEvent::None), TargetState(NAME_None), Delay(0.0f) {}
};

/** Структура для описания состояния в FSM */
USTRUCT(BlueprintType)
struct FStateDescription
{
    GENERATED_BODY()

    /** Имя состояния */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FSM")
    FName StateName;

    /** Класс, реализующий состояние */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FSM")
    TSubclassOf<UMedComEnemyState> StateClass;

    /** Список переходов из этого состояния */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FSM")
    TArray<FStateTransition> Transitions;

    /** Параметры состояния */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FSM")
    TMap<FName, float> StateParams;

    FStateDescription() : StateName(NAME_None) {}
};

/**
 * Data Asset для настройки поведения врага.
 * Определяет состояния FSM и переходы между ними.
 */
UCLASS(BlueprintType)
class MEDCOMCORE_API UEnemyBehaviorDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    /** Начальное состояние FSM */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FSM")
    FName InitialState;

    /** Список состояний FSM */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FSM")
    TArray<FStateDescription> States;

    /** === Общие параметры поведения === */
    
    /** Сокет для крепления оружия */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Weapon")
    FName WeaponSocket = TEXT("GripPoint");
    
    /** === Параметры Idle состояния === */
    
    /** Время в Idle состоянии (сек) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Idle", meta = (ClampMin = "0.0"))
    float IdleTime = 5.0f;

    /** Интервал между поворотами головы в состоянии Idle (сек) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Idle", meta = (ClampMin = "0.0")) 
    float LookIntervalTime = 2.0f;

    /** Максимальный угол поворота головы в Idle (градусы) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Idle", meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float MaxLookAngle = 60.0f;
    
    /** Скорость поворота головы в Idle */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Idle", meta = (ClampMin = "0.0"))
    float LookRotationSpeed = 2.0f;
    
    /** === Параметры состояния Patrol === */
    
    /** Скорость патрулирования */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Patrol", meta = (ClampMin = "0.0"))
    float PatrolSpeed = 300.0f;
    
    /** Циклическое патрулирование (от последней точки к первой) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Patrol")
    bool bLoopPatrol = true;

    /** Использовать случайные точки патрулирования */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Patrol")
    bool bUseRandomPatrol = false;

    /** Радиус достижения точки патрулирования (см) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Patrol", meta = (ClampMin = "0.0"))
    float PatrolAcceptanceRadius = 100.0f;
    
    /** Количество точек патрулирования */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Patrol", meta = (ClampMin = "1", ClampMax = "12"))
    int32 NumPatrolPoints = 4;
    
    /** Максимальное расстояние патрулирования от начальной точки (см) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Patrol", meta = (ClampMin = "0.0"))
    float MaxPatrolDistance = 3000.0f;
    
    /** Расстояние для перерасчета пути (см) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Patrol", meta = (ClampMin = "0.0"))
    float RepathDistance = 100.0f;
    
    /** Скорость поворота при патрулировании (градусов/сек) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Patrol", meta = (ClampMin = "0.0"))
    float PatrolRotationRate = 300.0f;
    
    /** Включить функцию "смотреть по сторонам" при патрулировании */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Patrol")
    bool bLookAroundWhilePatrolling = false;
    
    /** Интервал между осмотрами во время патрулирования (сек) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Patrol", meta = (ClampMin = "0.0", EditCondition = "bLookAroundWhilePatrolling"))
    float PatrolLookAroundInterval = 3.0f;
    
    /** Длительность осмотра во время патрулирования (сек) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Patrol", meta = (ClampMin = "0.0", EditCondition = "bLookAroundWhilePatrolling"))
    float PatrolLookAroundDuration = 1.5f;
    
    /** === Параметры состояния Chase === */
    
    /** Скорость преследования */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Chase", meta = (ClampMin = "0.0"))
    float ChaseSpeed = 600.0f;
    
    /** Интервал обновления пути при преследовании (сек) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Chase", meta = (ClampMin = "0.0"))
    float ChaseUpdateInterval = 0.5f;
    
    /** Время до потери цели после потери видимости (сек) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Chase", meta = (ClampMin = "0.0"))
    float LoseTargetTime = 5.0f;
    
    /** Минимальная дистанция до цели (см) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Chase", meta = (ClampMin = "0.0"))
    float MinTargetDistance = 500.0f;
    
    /** Скорость поворота при преследовании (градусов/сек) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Chase", meta = (ClampMin = "0.0"))
    float ChaseRotationRate = 600.0f;
    
    /** === Параметры Combat состояния === */
    
    /** Дистанция для атаки (см) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Combat", meta = (ClampMin = "0.0"))
    float AttackRange = 1000.0f;
    
    /** Интервал между атаками (сек) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Combat", meta = (ClampMin = "0.0"))
    float AttackInterval = 1.5f;
    
    /** Урон от атаки */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Combat", meta = (ClampMin = "0.0"))
    float AttackDamage = 10.0f;
    
    /** Радиус атаки (для атак по области) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Combat", meta = (ClampMin = "0.0"))
    float AttackRadius = 50.0f;
    
    /** Угол атаки в градусах (для конусных атак) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Combat", meta = (ClampMin = "0.0", ClampMax = "360.0"))
    float AttackAngle = 60.0f;
    
    /** Задержка перед нанесением урона в анимации атаки (сек) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Combat", meta = (ClampMin = "0.0"))
    float AttackDelay = 0.3f;
    
    /** === Параметры поиска === */
    
    /** Время поиска цели (сек) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Search", meta = (ClampMin = "0.0"))
    float SearchTime = 10.0f;
    
    /** === Параметры состояния Return === */
    
    /** Скорость возврата */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Return", meta = (ClampMin = "0.0"))
    float ReturnSpeed = 350.0f;
    
    /** Интервал обновления пути при возврате (сек) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Return", meta = (ClampMin = "0.0"))
    float ReturnUpdateInterval = 1.0f;
    
    /** Радиус достижения точки возврата (см) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Return", meta = (ClampMin = "0.0"))
    float ReturnAcceptanceRadius = 150.0f;
    
    /** === Параметры состояния Death === */
    
    /** Задержка перед включением рагдолла (сек) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Death", meta = (ClampMin = "0.0"))
    float RagdollDelay = 0.1f;
    
    /** Время до исчезновения трупа (сек) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Death", meta = (ClampMin = "0.0"))
    float DespawnTime = 10.0f;
    
    /** Полностью уничтожить актора при смерти (true) или деактивировать (false, для пулинга) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters|Death")
    bool bDestroyOnDeath = true;

    UEnemyBehaviorDataAsset();

#if WITH_EDITOR
    /** Проверка данных на ошибки */
    virtual void PostLoad() override;
    
    /** Валидация при сохранении в редакторе */
    void PreSave(FObjectPreSaveContext SaveContext) override;
    
    /** Уведомление об изменении данных */
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

// Редакторные утилиты для работы с FSM - НЕ используем UCLASS(), чтобы избежать генерации кода UHT
#if WITH_EDITOR
namespace EnemyBehaviorEditorUtility
{
    MEDCOMCORE_API void VisualizeStateMachine(class UEnemyBehaviorDataAsset* BehaviorAsset);
}
#endif