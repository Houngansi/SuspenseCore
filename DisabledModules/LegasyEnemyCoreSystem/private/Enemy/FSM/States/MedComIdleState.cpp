#include "Core/Enemy/FSM/States/MedComIdleState.h"
#include "Core/Enemy/MedComEnemyCharacter.h"
#include "Core/Enemy/FSM/MedComEnemyFSMComponent.h"
#include "Engine/World.h"
#include "AIController.h"
#include "Core/Enemy/FSM/EnemyBehaviorDataAsset.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/FloatingPawnMovement.h"

UMedComIdleState::UMedComIdleState()  : CurrentLookDirection()
{
    StateTag = FGameplayTag::RequestGameplayTag("State.Idle");
    
    // Инициализация параметров осмотра
    CurrentLookAngle = 0.0f;
    TargetLookAngle = 0.0f;
    LookDirection = 1.0f;
    CurrentIdleTime = 0.0f;
    MaxIdleTime = 5.0f;
    LookInterval = 2.0f;
    LookRotationSpeed = 1.0f;
    MaxLookAngle = 60.0f;
    bIdleTimeoutSent = false;
}

void UMedComIdleState::OnEnter(AMedComEnemyCharacter* Owner)
{
    Super::OnEnter(Owner);
    
    if (!Owner)
        return;
    
    // Остановить движение через контроллер
    if (AAIController* AICtrl = Cast<AAIController>(Owner->GetController()))
    {
        AICtrl->StopMovement();
    }
    
    // Принудительная остановка обоих компонентов движения
    if (UCharacterMovementComponent* MoveComp = Owner->GetCharacterMovement())
    {
        if (MoveComp->IsComponentTickEnabled())
        {
            MoveComp->StopMovementImmediately();
            MoveComp->MaxWalkSpeed = 0.0f;
        }
    }
    
    if (UFloatingPawnMovement* FloatComp = Owner->GetFloatingMovementComponent())
    {
        if (FloatComp->IsComponentTickEnabled())
        {
            FloatComp->StopMovementImmediately();
            FloatComp->MaxSpeed = 0.0f;
        }
    }
    
    // Получаем параметры из конфигурации
    MaxIdleTime = GetStateParamFloat("IdleTime", 5.0f);
    
    // Если в DataAsset есть глобальный параметр IdleTime, используем его
    if (FSMComponent && FSMComponent->GetOwnerEnemy() && FSMComponent->GetOwnerEnemy()->GetBehaviorAsset())
    {
        auto BehaviorAsset = FSMComponent->GetOwnerEnemy()->GetBehaviorAsset();
        if (MaxIdleTime <= 0.0f) // если не задано в состоянии
        {
            MaxIdleTime = BehaviorAsset->IdleTime;
        }
    }
    
    // Проверяем, есть ли специальный флаг перехода из Return
    if (FSMComponent)
    {
        FString ForceIdleTimeStr = FSMComponent->GetCustomData(TEXT("ForceIdleTime"));
        if (!ForceIdleTimeStr.IsEmpty())
        {
            // Парсим указанное время
            float ForceTime = FCString::Atof(*ForceIdleTimeStr);
            if (ForceTime > 0.0f)
            {
                MaxIdleTime = ForceTime;
                LogStateMessage(Owner, FString::Printf(TEXT("Использую принудительное время Idle: %.1f секунд из-за перехода из Return"), MaxIdleTime));
            }
            
            // Очищаем флаг после использования
            FSMComponent->ClearCustomData(TEXT("ForceIdleTime"));
        }
    }
    
    // Важно: сбрасываем флаг отправки события таймаута
    bIdleTimeoutSent = false;
    CurrentIdleTime = 0.0f;
    
    // Явно запускаем таймер ожидания через FSMComponent
    if (FSMComponent)
    {
        // ВАЖНОЕ ИСПРАВЛЕНИЕ: используем специальный метод для таймера
        FSMComponent->StartStateTimer(FName("IdleTimer"), MaxIdleTime, false);
        
        LogStateMessage(Owner, FString::Printf(TEXT("Запущен таймер Idle на %.1f секунд"), MaxIdleTime));
    }
    else
    {
        LogStateMessage(Owner, TEXT("ОШИБКА: FSMComponent не найден!"), ELogVerbosity::Error);
    }
    
    // Запускаем таймер осмотра
    StartStateTimer(Owner, "LookTimer", LookInterval, true);
    
    // Начальное направление осмотра
    float RandomAngle = FMath::RandRange(-MaxLookAngle, MaxLookAngle);
    FRotator OwnerRotation = Owner->GetActorRotation();
    FVector LookDir = OwnerRotation.RotateVector(FVector::ForwardVector);
    LookDir = OwnerRotation.RotateVector(FRotator(0, RandomAngle, 0).RotateVector(FVector::ForwardVector));
    StartLookAt(Owner, LookDir);
    
    LogStateMessage(Owner, FString::Printf(TEXT("Idle for %.1f seconds"), MaxIdleTime));
    
    // Добавляем геймплей-тег для анимаций
    Owner->AddGameplayTag(FGameplayTag::RequestGameplayTag("State.Idle"));
}

void UMedComIdleState::OnExit(AMedComEnemyCharacter* Owner)
{
    // Сбрасываем таймеры
    if (FSMComponent)
    {
        FSMComponent->StopStateTimer(FName("IdleTimer"));
        FSMComponent->StopStateTimer(FName("LookTimer"));
    }
    
    if (Owner)
    {
        // Удаляем геймплей-тег
        Owner->RemoveGameplayTag(FGameplayTag::RequestGameplayTag("State.Idle"));
        
        // Восстанавливаем контроллер
        if (AAIController* AICtrl = Cast<AAIController>(Owner->GetController()))
        {
            AICtrl->ClearFocus(EAIFocusPriority::Gameplay);
        }
    }
    
    Super::OnExit(Owner);
}

void UMedComIdleState::OnEvent(AMedComEnemyCharacter* Owner, EEnemyEvent Event, AActor* EventInstigator)
{
    Super::OnEvent(Owner, Event, EventInstigator);
    
    if (!Owner)
        return;
    
    // В Idle особенно важно реагировать на обнаружение игрока
    // FSM автоматически выполнит переход в Chase по таблице переходов
    if (Event == EEnemyEvent::PlayerSeen && EventInstigator)
    {
        // Можем дополнительно повернуться к игроку
        if (AAIController* AICtrl = Cast<AAIController>(Owner->GetController()))
        {
            AICtrl->SetFocus(EventInstigator);
            LogStateMessage(Owner, FString::Printf(TEXT("Spotted player: %s"), *EventInstigator->GetName()));
        }
    }
}

void UMedComIdleState::OnTimerFired(AMedComEnemyCharacter* Owner, FName TimerName)
{
    Super::OnTimerFired(Owner, TimerName);
    
    if (!Owner)
        return;
    
    if (TimerName == FName("LookTimer"))
    {
        // Генерируем новое направление осмотра
        float RandomAngle = FMath::RandRange(-MaxLookAngle, MaxLookAngle);
        FRotator OwnerRotation = Owner->GetActorRotation();
        FVector LookDir = OwnerRotation.RotateVector(FRotator(0, RandomAngle, 0).RotateVector(FVector::ForwardVector));
        StartLookAt(Owner, LookDir);
        
        // Запускаем анимацию осмотра
        TriggerLookAnimation(Owner);
    }
    else if (TimerName == FName("IdleTimer"))
    {
        // Время ожидания истекло - FSM сам сделает переход по таблице
        LogStateMessage(Owner, TEXT("Idle time expired! Sending IdleTimeout event"), ELogVerbosity::Warning);
        
        // Проверяем, не отправляли ли мы уже событие (защита от дублирования)
        if (!bIdleTimeoutSent && FSMComponent)
        {
            bIdleTimeoutSent = true;
            
            // ВАЖНОЕ ИСПРАВЛЕНИЕ: Используем EnqueueFSMEvent для предотвращения рекурсии
            FSMComponent->EnqueueFSMEvent(EEnemyEvent::IdleTimeout);
            
            LogStateMessage(Owner, TEXT("IdleTimeout event sent to FSM"), ELogVerbosity::Warning);
        }
    }
}

void UMedComIdleState::ProcessTick(AMedComEnemyCharacter* Owner, float DeltaTime)
{
    Super::ProcessTick(Owner, DeltaTime);
    
    if (!Owner)
        return;
    
    // Обновляем таймер ожидания
    CurrentIdleTime += DeltaTime;
    
    // Дополнительная проверка таймера Idle (на случай если таймер по какой-то причине не сработал)
    if (!bIdleTimeoutSent && CurrentIdleTime >= MaxIdleTime && FSMComponent)
    {
        LogStateMessage(Owner, TEXT("РЕЗЕРВНАЯ ПРОВЕРКА: Idle time expired in ProcessTick!"), ELogVerbosity::Warning);
        bIdleTimeoutSent = true;
        FSMComponent->EnqueueFSMEvent(EEnemyEvent::IdleTimeout);
    }
    
    // Обновляем поворот головы плавно
    RotateHead(Owner, DeltaTime);
    
    // Простая локальная анимация покачивания
    float TimeSinceBegan = Owner->GetWorld()->GetTimeSeconds();
    float IdleWiggle = FMath::Sin(TimeSinceBegan * 0.8f) * 0.5f;
    
    // Передаем небольшую вариацию в поворот, чтобы бот не выглядел статичным
    FRotator CurrentRot = Owner->GetActorRotation();
    FRotator TargetRot = CurrentRot;
    TargetRot.Yaw += IdleWiggle;
    
    // Плавная интерполяция вращения
    Owner->SetActorRotation(FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, 1.0f));
}

void UMedComIdleState::RotateHead(AMedComEnemyCharacter* Owner, float DeltaTime)
{
    // Плавно интерполируем текущий угол поворота к целевому
    CurrentLookAngle = FMath::FInterpTo(CurrentLookAngle, TargetLookAngle, DeltaTime, LookRotationSpeed);
    
    // Если мы почти достигли целевого угла, генерируем новый
    if (FMath::IsNearlyEqual(CurrentLookAngle, TargetLookAngle, 1.0f))
    {
        // Определяем новый диапазон для выбора (чтобы бот смотрел в разные стороны)
        float MinAngle = LookDirection > 0.0f ? 0.0f : -MaxLookAngle;
        float MaxAngle = LookDirection > 0.0f ? MaxLookAngle : 0.0f;
        
        // Выбираем новый целевой угол
        TargetLookAngle = FMath::RandRange(MinAngle, MaxAngle);
        
        // Меняем направление после достижения
        LookDirection *= -1.0f;
    }
    
    // Создаем поворот головы на основе текущего угла
    // Здесь используем поворот через AIController, если бы это был AnimInstance,
    // мы бы передавали CurrentLookAngle как переменную анимации
    if (AAIController* AICtrl = Owner ? Cast<AAIController>(Owner->GetController()) : nullptr)
    {
        FRotator BaseRot = Owner->GetActorRotation();
        FVector LookDir = BaseRot.RotateVector(FRotator(0, CurrentLookAngle, 0).RotateVector(FVector::ForwardVector));
        FVector LookPos = Owner->GetActorLocation() + LookDir * 1000.0f;
        
        // Устанавливаем фокус для контроллера (имитирует поворот головы)
        AICtrl->SetFocalPoint(LookPos);
    }
}

void UMedComIdleState::StartLookAt(AMedComEnemyCharacter* Owner, const FVector& Direction)
{
    if (!Owner)
        return;
        
    // Устанавливаем фокус на направление осмотра
    if (AAIController* AICtrl = Cast<AAIController>(Owner->GetController()))
    {
        CurrentLookDirection = Direction;
        
        // Вычисляем точку на расстоянии для фокуса
        FVector FocalPoint = Owner->GetActorLocation() + Direction * 1000.0f;
        AICtrl->SetFocalPoint(FocalPoint);
    }
}

void UMedComIdleState::TriggerLookAnimation(AMedComEnemyCharacter* Owner)
{
    if (!Owner)
        return;
        
    // Запуск анимации в скелетном меше
    // Эта функция может быть реализована с использованием AnimInstance, например:
    /*
    if (UAnimInstance* AnimInstance = Owner->GetMesh()->GetAnimInstance())
    {
        // Если есть переменная LookAround в анимации, устанавливаем её
        AnimInstance->SetBoolParameterOnAllAnimInstances(FName("LookAround"), true);
        
        // Можно также включить таймер, чтобы сбросить параметр через некоторое время
        FTimerHandle LookAnimTimer;
        Owner->GetWorld()->GetTimerManager().SetTimer(
            LookAnimTimer,
            [AnimInstance]() {
                AnimInstance->SetBoolParameterOnAllAnimInstances(FName("LookAround"), false);
            },
            0.5f,
            false
        );
    }
    */
    
    // Альтернативный подход - изменение направления взгляда
    float LookAngle = FMath::RandRange(-MaxLookAngle, MaxLookAngle);
    TargetLookAngle = LookAngle;
}