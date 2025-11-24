// Copyright MedCom Team. All Rights Reserved.

#include "Characters/MedComCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/MedComBaseAttributeSet.h"
#include "Interfaces/Core/IMedComMovementInterface.h"

UMedComCharacterMovementComponent::UMedComCharacterMovementComponent()
{
    // НЕ устанавливаем MaxWalkSpeed здесь! Она будет взята из AttributeSet
    // Устанавливаем только физические параметры движения
    GroundFriction = 8.0f;
    BrakingFriction = 2.0f;
    BrakingDecelerationWalking = 2048.0f;
    
    // Инициализируем теги
    SprintingTag = FGameplayTag::RequestGameplayTag(TEXT("State.Sprinting"));
    CrouchingTag = FGameplayTag::RequestGameplayTag(TEXT("State.Crouching"));
    
    // Инициализируем состояния
    bIsSprintingGAS = false;
    bIsCrouchingGAS = false;
    bIsJumping = false;
    bIsSliding = false;
    
    SyncLogCounter = 0;
}

void UMedComCharacterMovementComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // Выполняем начальную синхронизацию скорости
    // Даем небольшую задержку для гарантии инициализации AttributeSet
    if (CharacterOwner && CharacterOwner->GetWorld())
    {
        FTimerHandle InitTimer;
        CharacterOwner->GetWorld()->GetTimerManager().SetTimer(InitTimer, [this]()
        {
            SyncMovementSpeedFromAttributes();
            UE_LOG(LogTemp, Log, TEXT("[MovementComponent] Initial speed sync completed"));
        }, 0.1f, false);
    }
}

void UMedComCharacterMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    // Синхронизируем скорость с AttributeSet каждый тик
    // Это гарантирует, что любые изменения через GameplayEffects применятся немедленно
    SyncMovementSpeedFromAttributes();
    
    // Обновляем состояния движения на основе тегов GAS
    UpdateMovementStateFromTags();
    
    // Обновляем слайдинг если активен
    if (bIsSliding)
    {
        UpdateSliding(DeltaTime);
    }
    
    // Обновляем состояние прыжка
    if (IsFalling())
    {
        bIsJumping = Velocity.Z > 0.0f;
    }
    else
    {
        bIsJumping = false;
    }
}

void UMedComCharacterMovementComponent::SyncMovementSpeedFromAttributes()
{
    // Получаем ASC
    UAbilitySystemComponent* ASC = GetOwnerASC();
    if (!ASC)
    {
        // Логируем только раз в секунду чтобы не спамить
        if (++SyncLogCounter % 60 == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("[MovementSync] No ASC found for speed sync"));
        }
        return;
    }
    
    // Получаем AttributeSet
    const UMedComBaseAttributeSet* AttributeSet = GetOwnerAttributeSet();
    if (!AttributeSet)
    {
        if (++SyncLogCounter % 60 == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("[MovementSync] No AttributeSet found for speed sync"));
        }
        return;
    }
    
    // КРИТИЧНО: Получаем ТЕКУЩЕЕ значение атрибута с учетом ВСЕХ модификаторов
    // Это включает базовое значение + все активные GameplayEffects
    float CurrentAttributeSpeed = ASC->GetNumericAttribute(AttributeSet->GetMovementSpeedAttribute());
    
    // Синхронизируем только если есть расхождение
    if (!FMath::IsNearlyEqual(MaxWalkSpeed, CurrentAttributeSpeed, 0.1f))
    {
        float OldSpeed = MaxWalkSpeed;
        MaxWalkSpeed = CurrentAttributeSpeed;
        
        // Логируем изменения скорости
        UE_LOG(LogTemp, Log, TEXT("[MovementSync] Speed updated: %.1f -> %.1f"), 
            OldSpeed, CurrentAttributeSpeed);
        
        // При уведомлении об изменении скорости используем GAS-синхронизированное значение
        if (CharacterOwner && CharacterOwner->GetClass()->ImplementsInterface(UMedComMovementInterface::StaticClass()))
        {
            IMedComMovementInterface::NotifyMovementSpeedChanged(
                CharacterOwner, OldSpeed, CurrentAttributeSpeed, bIsSprintingGAS);
        }
    }
}

void UMedComCharacterMovementComponent::UpdateMovementStateFromTags()
{
    UAbilitySystemComponent* ASC = GetOwnerASC();
    if (!ASC)
    {
        return;
    }
    
    // Синхронизируем состояние спринта с тегом
    bool bHasSprintTag = ASC->HasMatchingGameplayTag(SprintingTag);
    if (bIsSprintingGAS != bHasSprintTag)
    {
        bIsSprintingGAS = bHasSprintTag;
        UE_LOG(LogTemp, Log, TEXT("[MovementSync] Sprint state updated from tags: %s"), 
            bIsSprintingGAS ? TEXT("ON") : TEXT("OFF"));
    }
    
    // Синхронизируем состояние приседания с тегом
    bool bHasCrouchTag = ASC->HasMatchingGameplayTag(CrouchingTag);
    if (bIsCrouchingGAS != bHasCrouchTag)
    {
        bIsCrouchingGAS = bHasCrouchTag;
        UE_LOG(LogTemp, Log, TEXT("[MovementSync] Crouch state updated from tags: %s"), 
            bIsCrouchingGAS ? TEXT("ON") : TEXT("OFF"));
    }
}

UAbilitySystemComponent* UMedComCharacterMovementComponent::GetOwnerASC() const
{
    if (!CharacterOwner)
    {
        return nullptr;
    }
    
    // Сначала пробуем получить ASC напрямую с персонажа
    UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CharacterOwner);
    
    // Если не нашли, проверяем PlayerState
    if (!ASC)
    {
        if (APlayerState* PS = CharacterOwner->GetPlayerState())
        {
            ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS);
        }
    }
    
    return ASC;
}

const UMedComBaseAttributeSet* UMedComCharacterMovementComponent::GetOwnerAttributeSet() const
{
    if (UAbilitySystemComponent* ASC = GetOwnerASC())
    {
        return ASC->GetSet<UMedComBaseAttributeSet>();
    }
    return nullptr;
}

EMedComMovementMode UMedComCharacterMovementComponent::GetCurrentMovementMode() const
{
    if (IsFlying())
        return EMedComMovementMode::Flying;
    
    if (IsSwimming())
        return EMedComMovementMode::Swimming;
    
    if (IsFalling())
    {
        if (bIsJumping && Velocity.Z > 0.0f)
            return EMedComMovementMode::Jumping;
        else
            return EMedComMovementMode::Falling;
    }
    
    if (bIsSliding)
        return EMedComMovementMode::Sliding;
    
    // Check if crouching (GAS-synchronized)
    if (bIsCrouchingGAS)
        return EMedComMovementMode::Crouching;
    
    // Check if sprinting (GAS-synchronized)
    if (bIsSprintingGAS)
    {
        // IMPORTANT: Only return Sprinting if actually moving!
        float Speed2D = Velocity.Size2D();
        if (Speed2D > 10.0f)
        {
            return EMedComMovementMode::Sprinting;
        }
        // If sprinting but not moving, fall through to idle check
    }
    
    // CRITICAL: Check actual velocity to determine idle vs walking
    float Speed2D = Velocity.Size2D();
    
    // Use a small threshold to account for floating point precision
    const float IdleThreshold = 10.0f;
    
    if (Speed2D < IdleThreshold)
    {
        // Character is stationary
        return EMedComMovementMode::None; // This represents idle state
    }
    
    // Character is moving on ground
    if (IsMovingOnGround())
    {
        return EMedComMovementMode::Walking;
    }
    
    return EMedComMovementMode::None;
}

void UMedComCharacterMovementComponent::StartSliding()
{
    if (CanSlide())
    {
        bIsSliding = true;
        SlideTimer = SlideDuration;
        SlideStartVelocity = Velocity;
        
        // Уменьшаем трение для слайдинга
        GroundFriction = SlideFriction;
        BrakingFriction = 0.0f;
        
        // Форсируем присед во время слайда
        if (CharacterOwner)
        {
            CharacterOwner->Crouch();
        }
        
        UE_LOG(LogTemp, Log, TEXT("[Movement] Slide started"));
    }
}

void UMedComCharacterMovementComponent::StopSliding()
{
    if (bIsSliding)
    {
        bIsSliding = false;
        SlideTimer = 0.0f;
        
        // Восстанавливаем нормальное трение
        GroundFriction = 8.0f;
        BrakingFriction = 2.0f;
        
        // Пытаемся встать
        if (CharacterOwner)
        {
            CharacterOwner->UnCrouch();
        }
        
        UE_LOG(LogTemp, Log, TEXT("[Movement] Slide stopped"));
    }
}

bool UMedComCharacterMovementComponent::CanSlide() const
{
    return IsMovingOnGround() && 
           !bIsSliding && 
           !IsFalling() &&
           Velocity.Size() >= MinSlideSpeed;
}

void UMedComCharacterMovementComponent::UpdateSliding(float DeltaTime)
{
    if (!bIsSliding)
        return;
    
    SlideTimer -= DeltaTime;
    
    // Завершаем слайд когда таймер истек или скорость слишком низкая
    if (SlideTimer <= 0.0f || Velocity.Size() < MinSlideSpeed * 0.5f)
    {
        StopSliding();
        return;
    }
    
    // Применяем физику слайдинга
    FVector ForwardDir = Velocity.GetSafeNormal();
    if (ForwardDir.IsZero())
    {
        ForwardDir = CharacterOwner->GetActorForwardVector();
    }
    
    // Добавляем небольшой импульс вперед для поддержания слайда
    AddForce(ForwardDir * SlideSpeed * 2.0f);
}

bool UMedComCharacterMovementComponent::DoJump(bool bReplayingMoves)
{
    // Останавливаем слайдинг при прыжке
    if (bIsSliding)
    {
        StopSliding();
    }
    
    bool bJumpSuccess = Super::DoJump(bReplayingMoves);
    
    if (bJumpSuccess)
    {
        bIsJumping = true;
        UE_LOG(LogTemp, Log, TEXT("[Movement] Jump started"));
    }
    
    return bJumpSuccess;
}

void UMedComCharacterMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
    Super::ProcessLanded(Hit, remainingTime, Iterations);
    
    // Сбрасываем флаг прыжка при приземлении
    bIsJumping = false;
    
    // Уведомляем о приземлении
    if (CharacterOwner && CharacterOwner->GetClass()->ImplementsInterface(UMedComMovementInterface::StaticClass()))
    {
        float ImpactVelocity = Velocity.Z;
        IMedComMovementInterface::NotifyLanded(CharacterOwner, ImpactVelocity);
    }
    
    UE_LOG(LogTemp, Log, TEXT("[Movement] Landed"));
}

void UMedComCharacterMovementComponent::Crouch(bool bClientSimulation)
{
    // Не разрешаем обычный присед во время слайда
    if (!bIsSliding)
    {
        Super::Crouch(bClientSimulation);
        UE_LOG(LogTemp, Log, TEXT("[Movement] Crouch called"));
    }
}

void UMedComCharacterMovementComponent::UnCrouch(bool bClientSimulation)
{
    // Не разрешаем встать во время слайда
    if (!bIsSliding)
    {
        Super::UnCrouch(bClientSimulation);
        UE_LOG(LogTemp, Log, TEXT("[Movement] UnCrouch called"));
    }
}