#pragma once

#include "CoreMinimal.h"
#include "Core/AbilitySystem/Abilities/WeaponAbilities/MedComBaseFireAbility.h"
#include "MedComBurstFireAbility.generated.h"

/**
 * UMedComBurstFireAbility
 * 
 * Способность стрельбы очередью (Burst Fire) с сетевой репликацией.
 * При активации абилки происходит серия выстрелов (до BurstCount, если хватает патронов).
 * За каждое нажатие запускается BURST, который не прерывается до полного выполнения.
 */
UCLASS()
class MEDCOMCORE_API UMedComBurstFireAbility : public UMedComBaseFireAbility
{
    GENERATED_BODY()

public:
    UMedComBurstFireAbility();

    // Переопределяем проверку возможности активации для очереди
    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags,
        const FGameplayTagContainer* TargetTags,
        FGameplayTagContainer* OptionalRelevantTags) const override;

    // Переопределяем завершение способности для очистки таймеров очереди
    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled) override;

protected:
    /** Реализация метода выполнения выстрела из базового класса */
    virtual void FireNextShot() override;

    /** Сбрасывает состояние очереди */
    void ResetBurstState();

    /** Выполнение одного выстрела очереди */
    void ExecuteBurstShot(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo);

    // ===== Параметры Burst =====
    /** Количество выстрелов в одном BURST-е */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Burst")
    int32 BurstCount;

    /** Задержка между выстрелами BURST-а */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Burst")
    float BurstDelay;

    /** Тег блокировки активации во время выполнения очереди */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Gameplay Tags")
    FGameplayTag BurstActiveTag;

private:
    /** Текущее количество выстрелов в очереди */
    int32 CurrentBurstShotCount;
    
    /** Таймер для последовательных выстрелов очереди */
    FTimerHandle BurstTimerHandle;
};