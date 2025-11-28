// Copyright Suspense Team. All Rights Reserved.

#include "Abilities/CharacterJumpAbility.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectTypes.h"
#include "Attributes/GASAttributeSet.h"
#include "Interfaces/Core/ISuspenseMovement.h"
#include "Interfaces/Core/ISuspenseCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "AbilitySystemLog.h"

UCharacterJumpAbility::UCharacterJumpAbility()
{
    // Базовые параметры способности
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // Устанавливаем теги способности
    FGameplayTag JumpTag = FGameplayTag::RequestGameplayTag("Ability.Input.Jump");
    SetAssetTags(FGameplayTagContainer(JumpTag));
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag("Ability.Active.Jump"));

    // Блокирующие теги
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("State.Dead"));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("State.Stunned"));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("State.Disabled.Movement"));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("Block.Jump"));

    // Параметры прыжка по умолчанию
    JumpPowerMultiplier = 1.0f;
    StaminaCostPerJump = 15.0f;
    MinimumStaminaToJump = 5.0f;

    // Параметры безопасности
    MaxJumpDuration = 3.0f;      // Максимум 3 секунды в воздухе
    GroundCheckInterval = 0.1f;   // Проверяем каждые 100мс

    // Инициализация состояния
    bIsEnding = false;
}

bool UCharacterJumpAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    // Базовые проверки родительского класса
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        return false;
    }

    // Получаем аватара для проверок
    AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    if (!Avatar)
    {
        ABILITY_LOG(Warning, TEXT("[Jump] Нет валидного аватара"));
        return false;
    }

    // Проверяем поддержку интерфейса движения
    if (!Avatar->GetClass()->ImplementsInterface(USuspenseMovement::StaticClass()))
    {
        ABILITY_LOG(Warning, TEXT("[Jump] Актор не поддерживает IMedComMovementInterface"));
        return false;
    }

    // Проверяем базовую возможность прыгать
    if (!ISuspenseMovement::Execute_CanJump(Avatar))
    {
        ABILITY_LOG(Verbose, TEXT("[Jump] Персонаж не может прыгать"));
        return false;
    }

    // Проверяем, что персонаж на земле
    if (!IsCharacterGrounded(ActorInfo))
    {
        ABILITY_LOG(Verbose, TEXT("[Jump] Персонаж не на земле"));
        return false;
    }

    // Проверяем стамину
    if (const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
    {
        if (const UGASAttributeSet* Attributes = ASC->GetSet<UGASAttributeSet>())
        {
            float CurrentStamina = Attributes->GetStamina();
            if (CurrentStamina < MinimumStaminaToJump)
            {
                ABILITY_LOG(Warning, TEXT("[Jump] Недостаточно стамины: %.1f/%.1f"),
                    CurrentStamina, MinimumStaminaToJump);
                return false;
            }
        }
    }

    return true;
}

void UCharacterJumpAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    // Проверка полномочий
    if (!HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Коммитим способность (проверка тегов и кулдаунов)
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Сбрасываем флаг завершения
    bIsEnding = false;

    ABILITY_LOG(Display, TEXT("[Jump] Активация способности прыжка"));

    // Получаем необходимые компоненты
    AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

    if (!Avatar || !ASC)
    {
        ABILITY_LOG(Error, TEXT("[Jump] Отсутствуют необходимые компоненты"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // Применяем расход стамины
    if (!ApplyStaminaCost(ActorInfo))
    {
        ABILITY_LOG(Error, TEXT("[Jump] Не удалось применить расход стамины"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // Добавляем тег состояния прыжка
    FGameplayTag JumpingTag = FGameplayTag::RequestGameplayTag("State.Jumping");
    if (!ASC->HasMatchingGameplayTag(JumpingTag))
    {
        ASC->AddLooseGameplayTag(JumpingTag);
        ASC->AddReplicatedLooseGameplayTag(JumpingTag);
    }

    // Уведомляем систему о начале прыжка
    FGameplayTag JumpMovementState = FGameplayTag::RequestGameplayTag("Movement.Jumping");
    ISuspenseMovement::NotifyMovementStateChanged(Avatar, JumpMovementState, true);
    ISuspenseMovement::NotifyJumpStateChanged(Avatar, true);

    // Выполняем сам прыжок
    PerformJump(ActorInfo);

    // Запускаем таймеры безопасности
    if (UWorld* World = GetWorld())
    {
        // Таймер проверки приземления
        World->GetTimerManager().SetTimer(
            LandingCheckTimer,
            this,
            &UCharacterJumpAbility::CheckForLanding,
            GroundCheckInterval,
            true,  // Повторяющийся
            GroundCheckInterval  // Первая проверка через интервал
        );

        // Таймер принудительного завершения
        World->GetTimerManager().SetTimer(
            SafetyTimer,
            this,
            &UCharacterJumpAbility::ForceEndAbility,
            MaxJumpDuration,
            false  // Одноразовый
        );

        ABILITY_LOG(Display, TEXT("[Jump] Таймеры безопасности запущены"));
    }
}

void UCharacterJumpAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // Предотвращаем двойное завершение
    if (bIsEnding)
    {
        return;
    }
    bIsEnding = true;

    ABILITY_LOG(Display, TEXT("[Jump] Завершение способности"));

    // Очищаем таймеры
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(LandingCheckTimer);
        World->GetTimerManager().ClearTimer(SafetyTimer);
    }

    // Получаем компоненты для очистки состояния
    AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

    // Останавливаем прыжок
    if (Avatar && Avatar->GetClass()->ImplementsInterface(USuspenseMovement::StaticClass()))
    {
        ISuspenseMovement::Execute_StopJumping(Avatar);
        ISuspenseMovement::NotifyJumpStateChanged(Avatar, false);
    }

    // Убираем тег прыжка
    if (ASC)
    {
        FGameplayTag JumpingTag = FGameplayTag::RequestGameplayTag("State.Jumping");
        if (ASC->HasMatchingGameplayTag(JumpingTag))
        {
            ASC->RemoveLooseGameplayTag(JumpingTag);
            ASC->RemoveReplicatedLooseGameplayTag(JumpingTag);
        }
    }

    // Восстанавливаем состояние движения
    if (Avatar && IsCharacterGrounded(ActorInfo))
    {
        FGameplayTag WalkingState = FGameplayTag::RequestGameplayTag("Movement.Walking");
        ISuspenseMovement::NotifyMovementStateChanged(Avatar, WalkingState, false);
    }

    // Вызываем базовую реализацию
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCharacterJumpAbility::InputReleased(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
    // Контроль высоты прыжка при отпускании кнопки
    if (AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr)
    {
        // Проверяем, что персонаж еще в воздухе
        if (!IsCharacterGrounded(ActorInfo))
        {
            if (ACharacter* Character = Cast<ACharacter>(Avatar))
            {
                if (UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement())
                {
                    // Уменьшаем вертикальную скорость, если движемся вверх
                    if (MovementComp->Velocity.Z > 0)
                    {
                        FVector NewVelocity = MovementComp->Velocity;
                        NewVelocity.Z *= 0.5f;
                        MovementComp->Velocity = NewVelocity;

                        ABILITY_LOG(Display, TEXT("[Jump] Высота прыжка уменьшена"));
                    }
                }
            }
        }
    }
}

bool UCharacterJumpAbility::IsCharacterGrounded(const FGameplayAbilityActorInfo* ActorInfo) const
{
    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
    {
        return false;
    }

    AActor* Avatar = ActorInfo->AvatarActor.Get();

    // Используем интерфейс движения для проверки
    if (Avatar->GetClass()->ImplementsInterface(USuspenseMovement::StaticClass()))
    {
        return ISuspenseMovement::Execute_IsGrounded(Avatar);
    }

    return false;
}

bool UCharacterJumpAbility::ApplyStaminaCost(const FGameplayAbilityActorInfo* ActorInfo)
{
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (!ASC || !JumpStaminaCostEffectClass)
    {
        return false;
    }

    // Создаем и применяем эффект расхода стамины
    FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
    EffectContext.AddSourceObject(ActorInfo->AvatarActor.Get());

    FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(
        JumpStaminaCostEffectClass, GetAbilityLevel(), EffectContext);

    if (SpecHandle.IsValid())
    {
        // Устанавливаем расход через SetByCaller
        FGameplayTag CostTag = FGameplayTag::RequestGameplayTag("Cost.Stamina");
        SpecHandle.Data.Get()->SetSetByCallerMagnitude(CostTag, -StaminaCostPerJump);

        // Применяем эффект (для мгновенных эффектов не проверяем handle)
        ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

        ABILITY_LOG(Display, TEXT("[Jump] Расход стамины применен: %.1f"), StaminaCostPerJump);
        return true;
    }

    return false;
}

void UCharacterJumpAbility::PerformJump(const FGameplayAbilityActorInfo* ActorInfo)
{
    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
    {
        return;
    }

    AActor* Avatar = ActorInfo->AvatarActor.Get();

    // Выполняем прыжок через интерфейс
    if (Avatar->GetClass()->ImplementsInterface(USuspenseMovement::StaticClass()))
    {
        // Получаем и модифицируем силу прыжка
        float OriginalJumpPower = ISuspenseMovement::Execute_GetJumpZVelocity(Avatar);
        float ModifiedJumpPower = OriginalJumpPower * JumpPowerMultiplier;

        // Применяем модифицированную силу
        ISuspenseMovement::Execute_SetJumpZVelocity(Avatar, ModifiedJumpPower);

        // Выполняем прыжок
        ISuspenseMovement::Execute_Jump(Avatar);

        // Восстанавливаем оригинальную силу
        ISuspenseMovement::Execute_SetJumpZVelocity(Avatar, OriginalJumpPower);

        ABILITY_LOG(Display, TEXT("[Jump] Прыжок выполнен с силой: %.1f"), ModifiedJumpPower);
    }
}

void UCharacterJumpAbility::CheckForLanding()
{
    // Проверяем, приземлился ли персонаж
    if (IsCharacterGrounded(GetCurrentActorInfo()))
    {
        ABILITY_LOG(Display, TEXT("[Jump] Обнаружено приземление"));

        // Завершаем способность
        EndAbility(GetCurrentAbilitySpecHandle(),
                   GetCurrentActorInfo(),
                   GetCurrentActivationInfo(),
                   true, false);
    }
}

void UCharacterJumpAbility::ForceEndAbility()
{
    ABILITY_LOG(Warning, TEXT("[Jump] Принудительное завершение по таймауту"));

    // Принудительно завершаем способность
    EndAbility(GetCurrentAbilitySpecHandle(),
               GetCurrentActorInfo(),
               GetCurrentActivationInfo(),
               true, false);
}
