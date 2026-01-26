#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameplayTagContainer.h"
#include "MedComEnemyState.generated.h"

// Forward declarations
class AMedComEnemyCharacter;
class UMedComEnemyFSMComponent;
enum class EEnemyEvent : uint8;

/** Структура для хранения разнотипных значений параметров */
USTRUCT(BlueprintType)
struct FStateParamValue
{
    GENERATED_BODY()

    enum class EParamType : uint8
    {
        Float,
        String,
        Bool
    };

    EParamType Type;
    float FloatValue;
    FString StringValue;
    bool BoolValue;

    FStateParamValue() : Type(EParamType::Float), FloatValue(0.0f), BoolValue(false) {}

    explicit FStateParamValue(float Value) 
        : Type(EParamType::Float), FloatValue(Value), BoolValue(false) {}
    
    explicit FStateParamValue(const FString& Value) 
        : Type(EParamType::String), FloatValue(0.0f), StringValue(Value), BoolValue(false) {}
    
    explicit FStateParamValue(bool Value) 
        : Type(EParamType::Bool), FloatValue(0.0f), BoolValue(Value) {}
};

/**
 * Базовый класс состояния для FSM врага.
 * Определяет интерфейс для всех состояний ИИ.
 */
UCLASS(Abstract, Blueprintable)
class MEDCOMCORE_API UMedComEnemyState : public UObject
{
    GENERATED_BODY()

public:
    UMedComEnemyState();

    /** Инициализация состояния с параметрами типа float */
    virtual void InitializeState(const TMap<FName, float>& InStateParams);
    
    /** Инициализация с расширенными параметрами */
    virtual void InitializeWithParams(const TMap<FName, FStateParamValue>& InExtendedParams);

    /** Вызывается при входе в состояние */
    virtual void OnEnter(AMedComEnemyCharacter* Owner);
    
    /** Вызывается при выходе из состояния */
    virtual void OnExit(AMedComEnemyCharacter* Owner);
    
    /** Обработка событий в рамках текущего состояния */
    virtual void OnEvent(AMedComEnemyCharacter* Owner, EEnemyEvent Event, AActor* EventInstigator);

    /** Обработка срабатывания таймера */
    virtual void OnTimerFired(AMedComEnemyCharacter* Owner, FName TimerName);

    /** Возвращает тег состояния */
    UFUNCTION(BlueprintPure, Category = "AI|State")
    FGameplayTag GetStateTag() const { return StateTag; }

    /** Устанавливает владельца FSM для вызова событий */
    void SetFSMComponent(UMedComEnemyFSMComponent* InFSMComponent) { FSMComponent = InFSMComponent; }

    /** Вызывается каждый кадр для обработки логики состояния */
    virtual void ProcessTick(AMedComEnemyCharacter* Owner, float DeltaTime);

protected:
    /** Тег Gameplay, представляющий это состояние */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State")
    FGameplayTag StateTag;

    /** Ссылка на FSM компонент для вызова событий */
    UPROPERTY()
    UMedComEnemyFSMComponent* FSMComponent;
    
    /** Параметры состояния из Data Asset (обратная совместимость) */
    UPROPERTY()
    TMap<FName, float> StateParams;
    
    /** Расширенные параметры состояния */
    TMap<FName, FStateParamValue> ExtendedParams;

    /** Запуск таймера состояния */
    void StartStateTimer(AMedComEnemyCharacter* Owner, FName TimerName, float Duration, bool bLoop = false);
    
    /** Остановка таймера состояния */
    void StopStateTimer(AMedComEnemyCharacter* Owner, FName TimerName);
    
    /** Проверка видимости цели с использованием канала ECC_Camera */
    bool CanSeeTarget(AMedComEnemyCharacter* Owner, AActor* Target) const;
    
    /** Получение дистанции до цели */
    float GetDistanceToTarget(AMedComEnemyCharacter* Owner, AActor* Target) const;
    
    /** Получение параметра float из состояния (с проверкой) */
    float GetStateParamFloat(const FName& ParamName, float DefaultValue = 0.0f) const;
    
    /** Получение параметра string из состояния (с проверкой) */
    FString GetStateParamString(const FName& ParamName, const FString& DefaultValue = TEXT("")) const;
    
    /** Получение параметра bool из состояния (с проверкой) */
    bool GetStateParamBool(const FName& ParamName, bool DefaultValue = false) const;
    
    /** Активирует GameplayAbility если настроена интеграция с GAS */
    bool TryActivateAbility(AMedComEnemyCharacter* Owner, const FString& AbilityClassName);
    
    /** Деактивирует GameplayAbility если она активна */
    bool TryDeactivateAbility(AMedComEnemyCharacter* Owner, const FString& AbilityClassName);
    
    /** Логирование с префиксом состояния */
    void LogStateMessage(AMedComEnemyCharacter* Owner, const FString& Message, ELogVerbosity::Type Verbosity = ELogVerbosity::Log) const;
};