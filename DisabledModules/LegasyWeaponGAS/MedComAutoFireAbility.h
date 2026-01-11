#pragma once

#include "CoreMinimal.h"
#include "Core/AbilitySystem/Abilities/WeaponAbilities/MedComBaseFireAbility.h"
#include "MedComAutoFireAbility.generated.h"

/**
 * UMedComAutoFireAbility
 * 
 * Способность автоматической стрельбы с сетевой репликацией.
 * При нажатии кнопки начинается серия выстрелов, которая продолжается, пока кнопка зажата.
 * Каждый последующий выстрел увеличивает рекойл и разброс для реалистичной имитации автоматического оружия.
 */
UCLASS()
class MEDCOMCORE_API UMedComAutoFireAbility : public UMedComBaseFireAbility
{
    GENERATED_BODY()

public:
    UMedComAutoFireAbility();

    // Обработка отпускания кнопки для прекращения автоматической стрельбы
    virtual void InputReleased(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo) override;

    // Переопределение для проверки блокировки активации при активной стрельбе
    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags,
        const FGameplayTagContainer* TargetTags,
        FGameplayTagContainer* OptionalRelevantTags) const override;

    // Переопределение EndAbility для очистки таймеров и тегов авто-стрельбы
    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled) override;

protected:
    /** Реализация метода выполнения выстрела из базового класса */
    virtual void FireNextShot() override;

    /** Время начала автострельбы для расчета разброса */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Auto Fire")
    float AutoFireStartTime;
    
    /** Интервал между выстрелами (сек) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Auto Fire")
    float FireRate;

    /** Тег автоматической стрельбы */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Gameplay Tags")
    FGameplayTag AutoFireActiveTag;

private:
    /** Флаг, указывающий, активна ли автоматическая стрельба */
    bool bIsAutoFireActive;
    
    /** Флаг, указывающий, идет ли стрельба в данный момент */
    bool bIsFiring;
    
    /** Таймер для автоматической стрельбы */
    FTimerHandle AutoFireTimerHandle;
    
    /** Начало автоматической стрельбы */
    void StartAutoFire();
    
    /** Остановка автоматической стрельбы */
    void StopAutoFire();
    
    /** Выполнение одного выстрела в автоматическом режиме */
    void ExecuteAutoShot();
};