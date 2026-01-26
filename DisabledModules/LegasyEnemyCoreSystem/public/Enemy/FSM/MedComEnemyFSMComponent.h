#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Containers/Queue.h"
#include "IO/IoHash.h"
#include "MedComEnemyFSMComponent.generated.h"

// Forward declarations
class UMedComEnemyState;
class AMedComEnemyCharacter;
class UEnemyBehaviorDataAsset;

/** Типы событий ИИ */
UENUM(BlueprintType)
enum class EEnemyEvent : uint8
{
    None,
    PlayerSeen,
    PlayerLost,
    TookDamage,
    ReachedTarget,
    TargetOutOfRange,
    AmmoEmpty, 
    ReloadComplete,
    PatrolComplete,
    IdleTimeout,
    SearchTimeout,
    ReturnComplete,
    Dead
};

// Структура для очереди событий
USTRUCT()
struct FPendingFSMEvent
{
    GENERATED_BODY()
    
    // Тип события
    UPROPERTY()
    EEnemyEvent Event;
    
    // Вместо TWeakObjectPtr используем обычный указатель
    // TWeakObjectPtr будет использоваться только внутри функций компонента
    UPROPERTY()
    AActor* InstigatorPtr;
    
    // Конструктор по умолчанию
    FPendingFSMEvent() 
        : Event(EEnemyEvent::None), InstigatorPtr(nullptr) 
    {
    }
    
    // Упрощенный конструктор с обычным указателем
    FPendingFSMEvent(EEnemyEvent InEvent, AActor* InInstigator)
        : Event(InEvent), InstigatorPtr(InInstigator)
    {
    }
};
/**
 * Компонент FSM для управления состояниями врага.
 * Использует событийно-ориентированную архитектуру с защитой от рекурсии
 * и принципом "одно активное состояние".
 */
UCLASS(ClassGroup=(AI), meta=(BlueprintSpawnableComponent))
class MEDCOMCORE_API UMedComEnemyFSMComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMedComEnemyFSMComponent();

    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    
    /** Инициализация FSM с Data Asset конфигурацией */
    UFUNCTION(BlueprintCallable, Category = "AI|FSM")
    void Initialize(UEnemyBehaviorDataAsset* BehaviorConfig, AMedComEnemyCharacter* Owner);

    /**
    * Обрабатывает события ИИ, такие как обнаружение игрока или получение урона.
    * Имеет защиту от рекурсивных вызовов.
    */
    UFUNCTION(BlueprintCallable, Category = "AI|FSM")
    void ProcessFSMEvent(EEnemyEvent Event, AActor* EventInstigator = nullptr);
    
    /** 
    * Добавляет событие в очередь для асинхронной обработки.
    * Предотвращает проблемы с рекурсивными вызовами.
    */
    UFUNCTION(BlueprintCallable, Category = "AI|FSM")
    void EnqueueFSMEvent(EEnemyEvent Event, AActor* EventInstigator = nullptr);

    /** Смена текущего состояния по классу с защитой от рекурсии */
    UFUNCTION(BlueprintCallable, Category = "AI|FSM") 
    void ChangeState(TSubclassOf<UMedComEnemyState> NewStateClass);

    /** Смена текущего состояния по имени с защитой от рекурсии */
    UFUNCTION(BlueprintCallable, Category = "AI|FSM")
    void ChangeStateByName(FName StateName); 

    /** Проверяет, доступен ли переход в указанное состояние */
    UFUNCTION(BlueprintPure, Category = "AI|FSM")
    bool IsTransitionValid(FName TargetState) const;

    /** Возвращает текущее состояние */
    UFUNCTION(BlueprintPure, Category = "AI|FSM")
    UMedComEnemyState* GetCurrentState() const { return CurrentState; }

    /** Возвращает имя текущего состояния */
    UFUNCTION(BlueprintPure, Category = "AI|FSM")
    FName GetCurrentStateName() const { return CurrentStateName; }

    /** Возвращает текущий тег состояния */
    UFUNCTION(BlueprintPure, Category = "AI|FSM")
    FGameplayTag GetCurrentStateTag() const;

    /** Возвращает владельца FSM */
    UFUNCTION(BlueprintPure, Category = "AI|FSM")
    AMedComEnemyCharacter* GetOwnerEnemy() const { return OwnerEnemy; }

    /** Запускает таймер состояния с безопасной чисткой при смене состояния */
    UFUNCTION(BlueprintCallable, Category = "AI|FSM")
    void StartStateTimer(const FName& TimerName, float Duration, bool bLoop = false);
    
    /** Останавливает таймер состояния */
    UFUNCTION(BlueprintCallable, Category = "AI|FSM")
    void StopStateTimer(const FName& TimerName);
    
    /** Останавливает все таймеры */
    UFUNCTION(BlueprintCallable, Category = "AI|FSM")
    void StopAllStateTimers();

    /** Обработчики событий восприятия */
    UFUNCTION()
    void OnSeePawn(APawn* SeenPawn);

    UFUNCTION()
    void OnHearNoise(APawn* NoiseInstigator, const FVector& Location, float Volume);

    UFUNCTION()
    void OnDamaged(AActor* DamagedActor, float Damage, class AController* InstigatedBy, 
                 FVector HitLocation, class UPrimitiveComponent* FHitComponent, 
                 FName BoneName, FVector ShotFromDirection, 
                 const class UDamageType* DamageType, AActor* DamageCauser);

    /** 
     * Метод покадрового обновления FSM.
     * Вызывается из FSMManager каждый кадр.
     */
    void MasterTick(float DeltaTime);


    /** Обработчик таймера для состояний */

    UFUNCTION()
    void OnStateTimerFired(FName TimerName);
    /** 
        * Безопасно добавляет таймер в карту таймеров состояния.
        * Используется для внешнего управления таймерами из состояний.
        * @param TimerName Уникальное имя таймера
        * @param Handle Дескриптор таймера
        */
    UFUNCTION(BlueprintCallable, Category = "AI|FSM")
    void AddStateTimer(const FName& TimerName, const FTimerHandle& Handle);
    /**
    * Сохраняет пользовательские данные в FSM компоненте.
    * Используется для передачи информации между состояниями.
    * @param Key Ключ данных
    * @param Value Значение данных
    */
    UFUNCTION(BlueprintCallable, Category = "AI|FSM")
    void SetCustomData(const FString& Key, const FString& Value);
    
    /**
     * Получает пользовательские данные из FSM компонента.
     * @param Key Ключ данных
     * @param DefaultValue Значение по умолчанию, если данные не найдены
     * @return Значение данных или DefaultValue, если данные не найдены
     */
    UFUNCTION(BlueprintPure, Category = "AI|FSM")
    FString GetCustomData(const FString& Key, const FString& DefaultValue = TEXT("")) const;
    
    /**
     * Удаляет пользовательские данные из FSM компонента.
     * @param Key Ключ данных для удаления
     */
    UFUNCTION(BlueprintCallable, Category = "AI|FSM")
    void ClearCustomData(const FString& Key);
#if WITH_EDITOR
    /** Обработчик изменения данных в редакторе */
    void OnDataAssetChanged();
    
    /** Проверка, не происходит ли это в PIE */
    bool ShouldUpdateInEditor() const;
#endif
    UEnemyBehaviorDataAsset* GetBehaviorConfig() const { return BehaviorConfig; }

protected:
    /** Создает экземпляр состояния по имени из конфигурации */
    UMedComEnemyState* CreateStateInstance(const FName& StateName);
    
    
    
    /** Обрабатывает события из очереди */
    void ProcessEventQueue();
    
    /** Выполняет фактическую смену состояния с проверками */
    bool PerformStateChange(UMedComEnemyState* NewState, FName NewStateName);

private:
    /** Владелец FSM */
    UPROPERTY()
    AMedComEnemyCharacter* OwnerEnemy;

    /** Текущее активное состояние */
    UPROPERTY()
    UMedComEnemyState* CurrentState;
    
    /** Имя текущего состояния */
    UPROPERTY()
    FName CurrentStateName;

    /** Карта всех доступных состояний (создается из конфигурации) */
    UPROPERTY()
    TMap<FName, UMedComEnemyState*> StateMap;
    
    /** Карта переходов для быстрого доступа O(1) */
    TMap<FName, TMap<EEnemyEvent, FName>> TransitionMap;

    /** Текущий Data Asset с конфигурацией поведения */
    UPROPERTY()
    UEnemyBehaviorDataAsset* BehaviorConfig;
    
    /** GUID Data Asset для отслеживания изменений */
    FIoHash BehaviorAssetGuid;
    
    /** Таймеры состояний */
    TMap<FName, FTimerHandle> StateTimers;
    
    /** Флаг инициализации */
    bool bIsInitialized;
    
    /** Флаг защиты от рекурсивной обработки событий */
    bool bIsProcessingEvent;
    
    /** Флаг защиты от рекурсивной смены состояний */
    bool bIsChangingState;
    
    /** Очередь ожидающих событий для асинхронной обработки */
    TQueue<FPendingFSMEvent> EventQueue;
    
    /** Строит карту переходов для быстрого поиска O(1) */
    void BuildTransitionMap();
    
    /** Время последнего обновления */
    float LastUpdateTime;

    /** Словарь для хранения временных данных между переходами состояний */
    TMap<FString, FString> CustomTransitionData;
};