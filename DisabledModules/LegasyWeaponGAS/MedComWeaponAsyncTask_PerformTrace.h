#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Equipment/MedComWeaponTypes.h"
#include "MedComWeaponAsyncTask_PerformTrace.generated.h"

class AWeaponActor;
class UMedComWeaponAttributeSet;

/**
 * Конфигурация трассировки с возможностью отладки.
 * Большинство параметров берутся из атрибутов оружия.
 */
USTRUCT(BlueprintType)
struct MEDCOMCORE_API FMedComWeaponTraceConfig
{
    GENERATED_BODY()
    
    /** Использовать трассировку из дула оружия в центр экрана */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace")
    bool bUseMuzzleToScreenCenter;
    
    /** Визуализировать трассировку (для отладки) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace")
    bool bDebug;
    
    /** Длительность отображения отладочных линий (сек) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace", meta=(EditCondition="bDebug"))
    float DebugDrawTime;
    
    /** Канал или профиль коллизии */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace")
    FName TraceProfile;
    
    /** Принудительное количество трасс (для дробовика)
     * Если <= 0, будет использовано значение PelletCount из атрибутов */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace")
    int32 OverrideNumTraces;

    FMedComWeaponTraceConfig()
        : bUseMuzzleToScreenCenter(true)
        , bDebug(false)
        , DebugDrawTime(2.0f)
        , TraceProfile(TEXT("Weapon"))
        , OverrideNumTraces(0)
    {}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAsyncWeaponTraceDelegate, const TArray<FHitResult>&, HitResults);

/**
 * Асинхронная задача для выполнения трейса выстрела с учетом атрибутов оружия.
 * Интегрируется с системой атрибутов для получения разброса, дальности и других параметров.
 */
UCLASS()
class MEDCOMCORE_API UMedComWeaponAsyncTask_PerformTrace : public UAbilityTask
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FAsyncWeaponTraceDelegate OnCompleted;

    UPROPERTY(BlueprintAssignable)
    FAsyncWeaponTraceDelegate OnCancelled;

    /**
     * Инициализация задачи асинхронной трассировки с учетом атрибутов оружия.
     * @param OwningAbility - Способность, запускающая трейс
     * @param TaskInstanceName - Имя задачи
     * @param TraceConfig - Минимальная конфигурация трассировки (большинство параметров берутся из атрибутов)
     */
    UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta=(HidePin="OwningAbility", DefaultToSelf="OwningAbility"))
    static UMedComWeaponAsyncTask_PerformTrace* PerformWeaponTrace(
        UGameplayAbility* OwningAbility,
        FName TaskInstanceName,
        const FMedComWeaponTraceConfig& TraceConfig);
        
    /**
     * Инициализация задачи асинхронной трассировки с использованием структуры ShotRequest.
     * @param OwningAbility - Способность, запускающая трейс
     * @param TaskInstanceName - Имя задачи
     * @param ShotRequest - Полная конфигурация выстрела
     */
    UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta=(HidePin="OwningAbility", DefaultToSelf="OwningAbility"))
    static UMedComWeaponAsyncTask_PerformTrace* PerformWeaponTraceFromRequest(
        UGameplayAbility* OwningAbility,
        FName TaskInstanceName,
        const FMedComShotRequest& ShotRequest);

    virtual void Activate() override;
    virtual void ExternalCancel() override;
    virtual FString GetDebugString() const override;

protected:
    /** Получает аватара из информации о способности */
    AActor* GetAvatarActorFromActorInfo() const;
    
    /** Получает текущее оружие персонажа или противника */
    AWeaponActor* GetWeaponFromAvatar() const;
    
    /** Получает атрибуты оружия */
    const UMedComWeaponAttributeSet* GetWeaponAttributeSet() const;
    
    /** Определяет точку прицеливания в мире (куда направлен центр экрана) */
    bool GetAimPoint(FVector& OutCameraLocation, FVector& OutAimPoint) const;
    
    /** Выполняет трассировку на основе конфигурации */
    void ExecuteTrace();
    
    /** Выполняет трассировку на основе ShotRequest */
    void ExecuteTraceFromRequest();

private:
    /** Базовая конфигурация трассировки */
    FMedComWeaponTraceConfig TraceConfig;
    
    /** Запрос на выстрел (если используется) */
    FMedComShotRequest ShotRequest;
    
    /** Флаг использования ShotRequest вместо базовой конфигурации */
    bool bUseShotRequest;
    
    /** Кэшированный указатель на атрибутсет оружия */
    mutable const UMedComWeaponAttributeSet* CachedWeaponAttributeSet;

    /** Вызывается при завершении трассировки */
    void HandleTraceCompleted(const TArray<FHitResult>& HitResults);
};