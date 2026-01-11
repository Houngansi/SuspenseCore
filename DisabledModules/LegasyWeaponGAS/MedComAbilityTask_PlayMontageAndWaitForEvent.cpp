#include "Core/AbilitySystem/Abilities/Tasks/MedComAbilityTask_PlayMontageAndWaitForEvent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "Animation/AnimInstance.h"
#include "AbilitySystemGlobals.h" // Добавим этот инклюд для доступа к UAbilitySystemGlobals

UMedComAbilityTask_PlayMontageAndWaitForEvent* UMedComAbilityTask_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
    UGameplayAbility* OwningAbility,
    FName TaskInstanceName,
    UAnimMontage* InMontageToPlay,
    FGameplayTagContainer InEventTags,
    float InRate,
    FName InStartSection,
    bool bInStopWhenAbilityEnds,
    float InAnimRootMotionTranslationScale)
{
    UMedComAbilityTask_PlayMontageAndWaitForEvent* MyObj = NewAbilityTask<UMedComAbilityTask_PlayMontageAndWaitForEvent>(OwningAbility, TaskInstanceName);
    MyObj->MontageToPlay = InMontageToPlay;
    MyObj->EventTags = InEventTags;
    MyObj->Rate = InRate;
    MyObj->StartSection = InStartSection;
    MyObj->bStopWhenAbilityEnds = bInStopWhenAbilityEnds;
    MyObj->AnimRootMotionTranslationScale = InAnimRootMotionTranslationScale;
    return MyObj;
}

// Добавим метод-помощник для получения аватара в задаче
AActor* UMedComAbilityTask_PlayMontageAndWaitForEvent::GetAvatarActorFromActorInfo() const
{
    if (!AbilitySystemComponent.IsValid())
    {
        return nullptr;
    }
    
    return AbilitySystemComponent->AbilityActorInfo->AvatarActor.Get();
}

void UMedComAbilityTask_PlayMontageAndWaitForEvent::Activate()
{
    if (!Ability || !MontageToPlay)
    {
        OnCancelled.Broadcast(FGameplayTag(), FGameplayEventData());
        EndTask();
        return;
    }

    // Получаем аним инстанс
    AActor* AvatarActor = GetAvatarActorFromActorInfo();
    if (!AvatarActor)
    {
        OnCancelled.Broadcast(FGameplayTag(), FGameplayEventData());
        EndTask();
        return;
    }

    USkeletalMeshComponent* MeshComp = AvatarActor->FindComponentByClass<USkeletalMeshComponent>();
    if (!MeshComp)
    {
        OnCancelled.Broadcast(FGameplayTag(), FGameplayEventData());
        EndTask();
        return;
    }

    UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
    if (!AnimInstance)
    {
        OnCancelled.Broadcast(FGameplayTag(), FGameplayEventData());
        EndTask();
        return;
    }

    // Запускаем анимацию
    float MontageDuration = AnimInstance->Montage_Play(MontageToPlay, Rate);
    if (MontageDuration <= 0.f)
    {
        OnCancelled.Broadcast(FGameplayTag(), FGameplayEventData());
        EndTask();
        return;
    }

    // Переходим в нужную секцию (если указана)
    if (StartSection != NAME_None)
    {
        AnimInstance->Montage_JumpToSection(StartSection, MontageToPlay);
    }

    // Подписываемся на делегаты
    FOnMontageBlendingOutStarted BlendingOutDelegate;
    BlendingOutDelegate.BindUObject(this, &UMedComAbilityTask_PlayMontageAndWaitForEvent::OnMontageBlendingOut);
    AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, MontageToPlay);

    FOnMontageEnded EndedDelegate;
    EndedDelegate.BindUObject(this, &UMedComAbilityTask_PlayMontageAndWaitForEvent::OnMontageEnded);
    AnimInstance->Montage_SetEndDelegate(EndedDelegate, MontageToPlay);

    bIsPlayingMontage = true;
}

void UMedComAbilityTask_PlayMontageAndWaitForEvent::OnDestroy(bool AbilityEnded)
{
    if (bStopWhenAbilityEnds && bIsPlayingMontage)
    {
        AActor* AvatarActor = GetAvatarActorFromActorInfo();
        if (AvatarActor)
        {
            if (USkeletalMeshComponent* MeshComp = AvatarActor->FindComponentByClass<USkeletalMeshComponent>())
            {
                if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
                {
                    AnimInstance->Montage_Stop(0.2f, MontageToPlay);
                }
            }
        }
    }

    Super::OnDestroy(AbilityEnded);
}

void UMedComAbilityTask_PlayMontageAndWaitForEvent::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
    if (bInterrupted)
    {
        OnInterrupted.Broadcast(FGameplayTag(), FGameplayEventData());
    }
    else
    {
        OnBlendOut.Broadcast(FGameplayTag(), FGameplayEventData());
    }
}

void UMedComAbilityTask_PlayMontageAndWaitForEvent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (bInterrupted)
    {
        OnInterrupted.Broadcast(FGameplayTag(), FGameplayEventData());
    }
    else
    {
        OnCompleted.Broadcast(FGameplayTag(), FGameplayEventData());
    }
    EndTask();
}