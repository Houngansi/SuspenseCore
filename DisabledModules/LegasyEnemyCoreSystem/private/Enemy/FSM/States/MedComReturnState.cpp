#include "Core/Enemy/FSM/States/MedComReturnState.h"
#include "Core/Enemy/MedComEnemyCharacter.h"
#include "Core/Enemy/FSM/MedComEnemyFSMComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "DrawDebugHelpers.h"

// Инициализация статических переменных
FVector UMedComReturnState::LastReturnPoint = FVector::ZeroVector;
const FName UMedComReturnState::PathUpdateTimerName = TEXT("PathUpdateTimer");
const FName UMedComReturnState::ReturnCompleteTimerName = TEXT("ReturnCompleteTimer");
const FName UMedComReturnState::StuckCheckTimerName = TEXT("StuckCheckTimer");

// Определение лога
DEFINE_LOG_CATEGORY_STATIC(LogMedComReturnState, Log, All);

UMedComReturnState::UMedComReturnState()
{
    // Устанавливаем тег состояния
    StateTag = FGameplayTag::RequestGameplayTag("State.Return");
    
    // Инициализация параметров (будут перезаписаны из StateParams)
    ReturnSpeed = 300.0f;
    AcceptanceRadius = 100.0f;
    PathUpdateInterval = 1.0f;
    StuckCheckInterval = 0.5f;
    MinMovementThreshold = 10.0f;
    MaxStuckCount = 3;
    ForceCompleteDistance = 150.0f;
    
    // Сброс рабочих переменных
    StuckCounter = 0;
    bReachedReturnPoint = false;
    bProcessingCompletion = false;
    PreviousLocation = FVector::ZeroVector;
    ReturnLocation = FVector::ZeroVector;
}

void UMedComReturnState::OnEnter(AMedComEnemyCharacter* Owner)
{
    // Вызываем базовый метод
    Super::OnEnter(Owner);
    
    if (!Owner)
    {
        return;
    }
    
    // Получаем параметры из конфигурации состояния
    ReturnSpeed = GetStateParamFloat(TEXT("ReturnSpeed"), ReturnSpeed);
    AcceptanceRadius = GetStateParamFloat(TEXT("AcceptanceRadius"), AcceptanceRadius);
    PathUpdateInterval = GetStateParamFloat(TEXT("PathUpdateInterval"), PathUpdateInterval);
    StuckCheckInterval = GetStateParamFloat(TEXT("StuckCheckInterval"), StuckCheckInterval);
    MinMovementThreshold = GetStateParamFloat(TEXT("MinMovementThreshold"), MinMovementThreshold);
    MaxStuckCount = FMath::RoundToInt(GetStateParamFloat(TEXT("MaxStuckCount"), MaxStuckCount));
    ForceCompleteDistance = GetStateParamFloat(TEXT("ForceCompleteDistance"), ForceCompleteDistance);
    
    // Сброс состояния
    bReachedReturnPoint = false;
    bProcessingCompletion = false;
    StuckCounter = 0;
    
    // Кэшируем контроллер для оптимизации
    CachedController = Cast<AAIController>(Owner->GetController());
    
    // Получаем и сохраняем точку возврата
    ReturnLocation = GetReturnPoint(Owner);
    
    // Настраиваем скорость движения
    if (UCharacterMovementComponent* MovementComp = Owner->GetCharacterMovement())
    {
        MovementComp->MaxWalkSpeed = ReturnSpeed;
        MovementComp->bOrientRotationToMovement = true;
    }
    
    // Запускаем таймеры
    if (FSMComponent)
    {
        FSMComponent->StartStateTimer(PathUpdateTimerName, PathUpdateInterval, true);
        FSMComponent->StartStateTimer(StuckCheckTimerName, StuckCheckInterval, true);
    }
    
    // Первое обновление пути
    UpdateReturnPath(Owner);
    
    // Сохраняем текущую позицию для отслеживания движения
    PreviousLocation = Owner->GetActorLocation();
    
    LogReturnState(Owner, FString::Printf(TEXT("Начинаем возврат к позиции: %s, дистанция: %.1f см"), 
                                          *ReturnLocation.ToString(), 
                                          FVector::Dist(Owner->GetActorLocation(), ReturnLocation)));
}

void UMedComReturnState::OnExit(AMedComEnemyCharacter* Owner)
{
    // Останавливаем все таймеры связанные с состоянием
    if (FSMComponent)
    {
        FSMComponent->StopStateTimer(PathUpdateTimerName);
        FSMComponent->StopStateTimer(ReturnCompleteTimerName);
        FSMComponent->StopStateTimer(StuckCheckTimerName);
    }
    
    // Останавливаем движение
    if (CachedController.IsValid())
    {
        CachedController->StopMovement();
    }
    
    LogReturnState(Owner, TEXT("Покидаем состояние Return"));
    
    // Важно: вызываем базовый метод в конце
    Super::OnExit(Owner);
}

void UMedComReturnState::ProcessTick(AMedComEnemyCharacter* Owner, float DeltaTime)
{
    Super::ProcessTick(Owner, DeltaTime);
    
    if (!Owner || !CachedController.IsValid())
    {
        return;
    }
    
    // Если уже в процессе завершения, пропускаем обработку
    if (bProcessingCompletion)
    {
        return;
    }
    
    // Проверяем, достигли ли точки возврата
    if (!bReachedReturnPoint)
    {
        bool bIsClose = IsCloseToReturnPoint(Owner);
        
        // Если достигли нужного расстояния или слишком медленные - считаем, что достигли точки
        float CurrentDistance = FVector::Dist(Owner->GetActorLocation(), ReturnLocation);
        float CurrentSpeed = Owner->GetVelocity().Size();
        
        if (bIsClose || (CurrentDistance <= ForceCompleteDistance && CurrentSpeed < MinMovementThreshold))
        {
            bReachedReturnPoint = true;
            bProcessingCompletion = true;
            
            // Останавливаем движение
            if (CachedController.IsValid())
            {
                CachedController->StopMovement();
            }
            
            LogReturnState(Owner, FString::Printf(
                TEXT("Достигли точки возврата! Дистанция: %.1f см, скорость: %.1f"),
                CurrentDistance, CurrentSpeed));
            
            // Запускаем таймер перехода с небольшой задержкой
            if (FSMComponent)
            {
                FSMComponent->StartStateTimer(ReturnCompleteTimerName, 0.1f, false);
            }
        }
    }
    
    // Отображение отладочной информации
    #if !UE_BUILD_SHIPPING
    if (GEngine && GEngine->bEnableOnScreenDebugMessages)
    {
        float CurrentDistance = FVector::Dist(Owner->GetActorLocation(), ReturnLocation);
        float CurrentSpeed = Owner->GetVelocity().Size();
        
        FString DebugMessage = FString::Printf(
            TEXT("[Return] Dist: %.1f cm, Speed: %.1f, Stuck: %d/%d"),
            CurrentDistance, CurrentSpeed, StuckCounter, MaxStuckCount);
        
        GEngine->AddOnScreenDebugMessage(
            /* Key */ -1,
            /* Time */ 0.0f,
            /* Color */ FColor::Yellow,
            /* Message */ DebugMessage);
        
        DrawDebugDirectionalArrow(
            Owner->GetWorld(),
            Owner->GetActorLocation(),
            ReturnLocation,
            20.0f,
            FColor::Yellow,
            false,
            -1.0f,
            0,
            3.0f);
    }
    #endif
}

void UMedComReturnState::OnEvent(AMedComEnemyCharacter* Owner, EEnemyEvent Event, AActor* Instigator)
{
    // Вызываем родительский метод
    Super::OnEvent(Owner, Event, Instigator);
    
    // Принципиально важно: НЕ вызываем ProcessFSMEvent здесь, чтобы избежать рекурсии!
    // FSM компонент автоматически обработает переходы по таблице состояний
    
    // Просто логируем информацию
    if (Event == EEnemyEvent::PlayerSeen && Instigator)
    {
        LogReturnState(Owner, FString::Printf(TEXT("Замечен игрок: %s (во время возврата)"), 
                                             *Instigator->GetName()));
    }
    else if (Event == EEnemyEvent::TookDamage && Instigator)
    {
        LogReturnState(Owner, FString::Printf(TEXT("Получен урон от: %s (во время возврата)"), 
                                             *Instigator->GetName()));
    }
}

void UMedComReturnState::OnTimerFired(AMedComEnemyCharacter* Owner, FName TimerName)
{
    Super::OnTimerFired(Owner, TimerName);
    
    if (!Owner)
    {
        return;
    }
    
    // Если уже в процессе завершения, пропускаем таймеры обновления
    if (bProcessingCompletion && 
        (TimerName == PathUpdateTimerName || TimerName == StuckCheckTimerName))
    {
        return;
    }
    
    if (TimerName == PathUpdateTimerName)
    {
        // Обновляем путь если еще не достигли точки
        if (!bReachedReturnPoint)
        {
            UpdateReturnPath(Owner);
        }
    }
    else if (TimerName == StuckCheckTimerName)
    {
        // Проверяем на застревание
        if (!bReachedReturnPoint && CheckForStuck(Owner))
        {
            // Если застряли слишком много раз - принудительно завершаем
            if (StuckCounter >= MaxStuckCount)
            {
                LogReturnState(Owner, TEXT("Слишком много застреваний - принудительно завершаем возврат"), 
                              ELogVerbosity::Warning);
                
                bReachedReturnPoint = true;
                bProcessingCompletion = true;
                
                // Останавливаем движение
                if (CachedController.IsValid())
                {
                    CachedController->StopMovement();
                }
                
                // Запускаем событие завершения
                SendReturnCompleteEvent(Owner);
            }
            else
            {
                // Пробуем обновить путь
                UpdateReturnPath(Owner);
            }
        }
    }
    else if (TimerName == ReturnCompleteTimerName)
    {
        // Генерируем событие завершения возврата
        SendReturnCompleteEvent(Owner);
    }
}

void UMedComReturnState::SendReturnCompleteEvent(AMedComEnemyCharacter* Owner)
{
    if (!FSMComponent)
    {
        LogReturnState(Owner, TEXT("Не удается отправить ReturnComplete: FSMComponent не найден!"), 
                     ELogVerbosity::Error);
        return;
    }
    
    LogReturnState(Owner, TEXT("Бот вернулся в точку патрулирования - отправка ReturnComplete"), ELogVerbosity::Warning);
    
    // Устанавливаем специальный флаг в компоненте FSM
    FSMComponent->SetCustomData(TEXT("LastStateTransition"), TEXT("Return->Idle"));
    
    // Принудительно устанавливаем короткое время для Idle после Return
    FSMComponent->SetCustomData(TEXT("ForceIdleTime"), TEXT("1.0"));
    
    // Также устанавливаем флаг для корректного перехода из Idle в Patrol
    FSMComponent->SetCustomData(TEXT("ReturnToIdleTransition"), TEXT("true"));
    
    // Также устанавливаем время перехода для отслеживания
    const float CurrentTime = Owner->GetWorld()->GetTimeSeconds();
    FSMComponent->SetCustomData(TEXT("ReturnCompleteTime"), FString::Printf(TEXT("%.2f"), CurrentTime));
    
    // Вместо отправки события в очередь, используем существующую функцию ChangeStateByName
    // для немедленного перехода в Idle, что гарантирует переход
    FSMComponent->ChangeStateByName(FName("Idle"));
    
    LogReturnState(Owner, TEXT("Переход в Idle выполнен, скоро бот начнет патрулирование"), 
                  ELogVerbosity::Warning);
}

FVector UMedComReturnState::GetReturnPoint(AMedComEnemyCharacter* Owner) const
{
    if (!Owner)
    {
        return FVector::ZeroVector;
    }
    
    // Приоритеты:
    // 1. Последняя известная точка патрулирования
    // 2. Точка спавна (начальная позиция)
    // 3. Текущая позиция, если ничего не найдено
    
    // Получаем последнюю сохраненную точку патруля
    if (!LastReturnPoint.IsZero())
    {
        return LastReturnPoint;
    }
    
    // Используем начальную позицию как запасной вариант
    FVector SpawnLocation = Owner->GetInitialPosition();
    if (!SpawnLocation.IsZero())
    {
        return SpawnLocation;
    }
    
    // Если ничего не найдено, возвращаемся на текущую позицию
    return Owner->GetActorLocation();
}

bool UMedComReturnState::UpdateReturnPath(AMedComEnemyCharacter* Owner)
{
    if (!Owner || !CachedController.IsValid())
    {
        return false;
    }
    
    // Проверяем, что еще не достигли точки и не обрабатываем завершение
    if (bReachedReturnPoint || bProcessingCompletion)
    {
        return false;
    }
    
    // Останавливаем текущее движение
    CachedController->StopMovement();
    
    // Запускаем новое движение
    EPathFollowingRequestResult::Type Result = CachedController->MoveToLocation(
        ReturnLocation, 
        AcceptanceRadius, 
        true,  // bStopOnOverlap
        true,  // bUsePathfinding
        false, // bProjectDestinationToNavigation
        true   // bCanStrafe
    );
    
    // Проверяем результат
    bool bSuccess = (Result != EPathFollowingRequestResult::Failed);
    
    if (!bSuccess)
    {
        LogReturnState(Owner, TEXT("Не удалось построить путь к точке возврата!"), ELogVerbosity::Warning);
    }
    else
    {
        LogReturnState(Owner, FString::Printf(TEXT("Обновлен путь к точке возврата, дистанция: %.1f см"),
                                             FVector::Dist(Owner->GetActorLocation(), ReturnLocation)),
                      ELogVerbosity::Verbose);
    }
    
    return bSuccess;
}

bool UMedComReturnState::IsCloseToReturnPoint(AMedComEnemyCharacter* Owner) const
{
    if (!Owner)
    {
        return false;
    }
    
    // Проверяем расстояние до точки возврата
    float Distance = FVector::Dist(Owner->GetActorLocation(), ReturnLocation);
    
    // Используем увеличенный радиус принятия
    // Это позволит боту считать точку достигнутой с большего расстояния
    // без необходимости создания новых переменных
    return (Distance <= AcceptanceRadius * 3.0f);
}
bool UMedComReturnState::CheckForStuck(AMedComEnemyCharacter* Owner)
{
    if (!Owner)
    {
        return false;
    }
    
    // Вычисляем пройденное расстояние с прошлой проверки
    FVector CurrentLocation = Owner->GetActorLocation();
    float MovedDistance = FVector::Dist(CurrentLocation, PreviousLocation);
    
    // Обновляем последнюю позицию
    PreviousLocation = CurrentLocation;
    
    // Получаем текущую скорость
    float CurrentSpeed = Owner->GetVelocity().Size();
    
    // Если движемся очень медленно, считаем что застряли
    bool bIsStuck = (MovedDistance < MinMovementThreshold && CurrentSpeed < MinMovementThreshold);
    
    if (bIsStuck)
    {
        StuckCounter++;
        LogReturnState(Owner, FString::Printf(TEXT("Обнаружено застревание (%d/%d): скорость %.1f, перемещение %.1f"),
                                             StuckCounter, MaxStuckCount, CurrentSpeed, MovedDistance),
                      ELogVerbosity::Warning);
        return true;
    }
    else
    {
        // Сбрасываем счетчик если вышли из застревания
        if (StuckCounter > 0 && CurrentSpeed > MinMovementThreshold * 2.0f)
        {
            LogReturnState(Owner, TEXT("Вышли из застревания, сбрасываем счетчик"), ELogVerbosity::Verbose);
            StuckCounter = 0;
        }
        return false;
    }
}

void UMedComReturnState::LogReturnState(AMedComEnemyCharacter* Owner, const FString& Message, 
                                      ELogVerbosity::Type Verbosity)
{
    FString OwnerName = Owner ? Owner->GetName() : TEXT("Unknown");
    
    // Выводим в лог
    if (Verbosity == ELogVerbosity::Warning)
    {
        UE_LOG(LogMedComReturnState, Warning, TEXT("[%s] %s"), *OwnerName, *Message);
    }
    else if (Verbosity == ELogVerbosity::Error)
    {
        UE_LOG(LogMedComReturnState, Error, TEXT("[%s] %s"), *OwnerName, *Message);
    }
    else
    {
        UE_LOG(LogMedComReturnState, Log, TEXT("[%s] %s"), *OwnerName, *Message);
    }
    
    // Вызываем логирование состояния
    LogStateMessage(Owner, Message, Verbosity);
}

// Статические методы для доступа к последней точке
FVector UMedComReturnState::GetLastReturnPoint()
{
    return LastReturnPoint;
}

void UMedComReturnState::SetLastReturnPoint(const FVector& Point)
{
    LastReturnPoint = Point;
}