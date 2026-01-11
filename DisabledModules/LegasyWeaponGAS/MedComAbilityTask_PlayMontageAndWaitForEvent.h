#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GameplayTagContainer.h"
#include "Animation/AnimMontage.h"
#include "MedComAbilityTask_PlayMontageAndWaitForEvent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMedComPlayMontageAndWaitForEventDelegate, FGameplayTag, EventTag, FGameplayEventData, EventData);

/**
 * Расширенная задача для проигрывания монтажей с возможностью задать скорость и секцию,
 * а также дождаться GameplayEvent, AnimNotify или завершения анимации.
 */
UCLASS()
class MEDCOMCORE_API UMedComAbilityTask_PlayMontageAndWaitForEvent : public UAbilityTask
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FMedComPlayMontageAndWaitForEventDelegate OnCompleted;

    UPROPERTY(BlueprintAssignable)
    FMedComPlayMontageAndWaitForEventDelegate OnBlendOut;

    UPROPERTY(BlueprintAssignable)
    FMedComPlayMontageAndWaitForEventDelegate OnInterrupted;

    UPROPERTY(BlueprintAssignable)
    FMedComPlayMontageAndWaitForEventDelegate OnCancelled;

    UPROPERTY(BlueprintAssignable)
    FMedComPlayMontageAndWaitForEventDelegate EventReceived;

    /**
     * Запуск задачи проигрывания монтажа.
     * @param MontageToPlay - Анимационный монтаж
     * @param Rate - Скорость воспроизведения
     * @param StartSection - Секция анимации
     */
    UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta=(HidePin="OwningAbility", DefaultToSelf="OwningAbility"))
    static UMedComAbilityTask_PlayMontageAndWaitForEvent* PlayMontageAndWaitForEvent(
        UGameplayAbility* OwningAbility,
        FName TaskInstanceName,
        UAnimMontage* MontageToPlay,
        FGameplayTagContainer EventTags,
        float Rate = 1.0f,
        FName StartSection = NAME_None,
        bool bStopWhenAbilityEnds = true,
        float AnimRootMotionTranslationScale = 1.0f);

    virtual void Activate() override;
    virtual void OnDestroy(bool AbilityEnded) override;

protected:
    // Добавляем вспомогательный метод для получения аватара в задаче
    AActor* GetAvatarActorFromActorInfo() const;

    UPROPERTY()
    UAnimMontage* MontageToPlay;

    UPROPERTY()
    FGameplayTagContainer EventTags;

    UPROPERTY()
    float Rate;

    UPROPERTY()
    FName StartSection;

    UPROPERTY()
    bool bStopWhenAbilityEnds;

    UPROPERTY()
    float AnimRootMotionTranslationScale;

    bool bIsPlayingMontage = false;

    void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);
    void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);
};