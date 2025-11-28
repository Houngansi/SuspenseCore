// Copyright Suspense Team. All Rights Reserved.

#include "Abilities/CharacterCrouchAbility.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "GameplayEffectTypes.h"
#include "Attributes/GASAttributeSet.h"
#include "Interfaces/Core/ISuspenseMovement.h"
#include "Interfaces/Core/ISuspenseCharacter.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "AbilitySystemLog.h"

UCharacterCrouchAbility::UCharacterCrouchAbility()
{
    // Базовые параметры способности - ТАКИЕ ЖЕ КАК У SPRINT!
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // Установка тегов способности
    FGameplayTag CrouchTag = FGameplayTag::RequestGameplayTag("Ability.Input.Crouch");
    SetAssetTags(FGameplayTagContainer(CrouchTag));

    // ВАЖНО: Устанавливаем AbilityTags для проверки активности
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag("Ability.Active.Crouch"));

    // Блокирующие теги
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("State.Dead"));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("State.Stunned"));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("State.Disabled.Movement"));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("State.Sprinting"));

    // Параметры крауча
    CrouchSpeedMultiplier = 0.5f;
}

bool UCharacterCrouchAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        return false;
    }

    // Проверяем через интерфейс движения
    if (ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
        AActor* Avatar = ActorInfo->AvatarActor.Get();

        // Проверяем поддержку интерфейса
        if (!Avatar->GetClass()->ImplementsInterface(USuspenseMovement::StaticClass()))
        {
            ABILITY_LOG(Warning, TEXT("[Crouch] Actor doesn't support IMedComMovementInterface"));
            return false;
        }

        // Проверяем возможность крауча
        if (!ISuspenseMovement::Execute_CanCrouch(Avatar))
        {
            ABILITY_LOG(Warning, TEXT("[Crouch] Character cannot crouch"));
            return false;
        }

        // Проверяем, не в крауче ли уже
        if (ISuspenseMovement::Execute_IsCrouching(Avatar))
        {
            ABILITY_LOG(Warning, TEXT("[Crouch] Character is already crouching"));
            return false;
        }
    }

    return true;
}

void UCharacterCrouchAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    if (!HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
    {
        ABILITY_LOG(Error, TEXT("[Crouch] No authority or prediction key"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        ABILITY_LOG(Error, TEXT("[Crouch] Failed to commit ability"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Сохраняем параметры активации
    CurrentSpecHandle = Handle;
    CurrentActorInfo = ActorInfo;
    CurrentActivationInfo = ActivationInfo;

    ABILITY_LOG(Display, TEXT("[Crouch] Activating crouch ability"));

    // Получаем аватар и ASC
    AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    if (!Avatar)
    {
        ABILITY_LOG(Error, TEXT("[Crouch] No valid avatar"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (!ASC)
    {
        ABILITY_LOG(Error, TEXT("[Crouch] No ASC"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // Выполняем крауч через интерфейс
    ISuspenseMovement::Execute_Crouch(Avatar);

    // Применяем дебафф крауча (снижение скорости + тег)
    if (CrouchDebuffEffectClass)
    {
        FGameplayEffectContextHandle DebuffContext = ASC->MakeEffectContext();
        DebuffContext.AddSourceObject(Avatar);

        FGameplayEffectSpecHandle DebuffSpecHandle = ASC->MakeOutgoingSpec(
            CrouchDebuffEffectClass, GetAbilityLevel(), DebuffContext);

        if (DebuffSpecHandle.IsValid())
        {
            CrouchDebuffEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*DebuffSpecHandle.Data.Get());

            if (CrouchDebuffEffectHandle.IsValid())
            {
                ABILITY_LOG(Display, TEXT("[Crouch] Crouch debuff effect applied successfully"));
            }
            else
            {
                ABILITY_LOG(Error, TEXT("[Crouch] Failed to apply crouch debuff effect"));
                EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
                return;
            }
        }
    }
    else
    {
        ABILITY_LOG(Warning, TEXT("[Crouch] CrouchDebuffEffectClass not configured!"));
    }

    // Настраиваем отслеживание отпускания кнопки
    UAbilityTask_WaitInputRelease* WaitReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
    if (WaitReleaseTask)
    {
        WaitReleaseTask->OnRelease.AddDynamic(this, &UCharacterCrouchAbility::OnCrouchInputReleased);
        WaitReleaseTask->ReadyForActivation();
        ABILITY_LOG(Display, TEXT("[Crouch] Input release task activated"));
    }

    // Уведомляем об изменении состояния движения
    FGameplayTag CrouchMovementState = FGameplayTag::RequestGameplayTag("Movement.Crouching");
    ISuspenseMovement::NotifyMovementStateChanged(Avatar, CrouchMovementState, true);
    ISuspenseMovement::NotifyCrouchStateChanged(Avatar, true);

    // Воспроизводим звук начала крауча
    if (CrouchStartSound && ActorInfo->IsLocallyControlled())
    {
        UGameplayStatics::PlaySound2D(GetWorld(), CrouchStartSound);
    }

    ABILITY_LOG(Display, TEXT("[Crouch] Ability activated successfully"));
    ABILITY_LOG(Display, TEXT("  - Crouch debuff: %s"), CrouchDebuffEffectHandle.IsValid() ? TEXT("Active") : TEXT("Failed"));
}

void UCharacterCrouchAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    ABILITY_LOG(Display, TEXT("[Crouch] Ending ability (Cancelled: %s)"),
        bWasCancelled ? TEXT("Yes") : TEXT("No"));

    // Получаем ASC для удаления эффектов
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;

    // Удаляем дебафф крауча
    if (ASC && CrouchDebuffEffectHandle.IsValid())
    {
        bool bRemoved = ASC->RemoveActiveGameplayEffect(CrouchDebuffEffectHandle);
        ABILITY_LOG(Display, TEXT("[Crouch] Crouch debuff effect removed: %s"),
            bRemoved ? TEXT("Yes") : TEXT("No"));

        CrouchDebuffEffectHandle.Invalidate();
    }

    // Встаём через интерфейс
    if (Avatar && Avatar->GetClass()->ImplementsInterface(USuspenseMovement::StaticClass()))
    {
        ISuspenseMovement::Execute_UnCrouch(Avatar);
    }

    // Уведомляем об изменении состояния движения
    if (Avatar)
    {
        FGameplayTag WalkingState = FGameplayTag::RequestGameplayTag("Movement.Walking");
        ISuspenseMovement::NotifyMovementStateChanged(Avatar, WalkingState, false);
        ISuspenseMovement::NotifyCrouchStateChanged(Avatar, false);
    }

    // Воспроизводим звук окончания крауча
    if (CrouchEndSound && ActorInfo->IsLocallyControlled())
    {
        UGameplayStatics::PlaySound2D(GetWorld(), CrouchEndSound);
    }

    // Очищаем сохранённые параметры
    CurrentSpecHandle = FGameplayAbilitySpecHandle();
    CurrentActorInfo = nullptr;
    CurrentActivationInfo = FGameplayAbilityActivationInfo();

    // Вызываем родительскую реализацию
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCharacterCrouchAbility::InputReleased(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
    Super::InputReleased(Handle, ActorInfo, ActivationInfo);

    ABILITY_LOG(Display, TEXT("[Crouch] InputReleased called"));

    // Проверяем, активна ли способность
    if (IsActive())
    {
        ABILITY_LOG(Display, TEXT("[Crouch] Ability is active, ending it"));

        // Завершаем способность при отпускании кнопки
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UCharacterCrouchAbility::OnCrouchInputReleased(float TimeHeld)
{
    ABILITY_LOG(Display, TEXT("[Crouch] Button released (held for %.2f sec)"), TimeHeld);

    // Используем сохранённые параметры
    if (CurrentSpecHandle.IsValid() && CurrentActorInfo)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UCharacterCrouchAbility::InputPressed(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
    Super::InputPressed(Handle, ActorInfo, ActivationInfo);

    // Логируем нажатие для отладки
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        const FGameplayAbilitySpec* Spec = ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
        if (Spec)
        {
            ABILITY_LOG(Display, TEXT("[Crouch] InputPressed with InputID: %d"), Spec->InputID);
        }
    }
}
