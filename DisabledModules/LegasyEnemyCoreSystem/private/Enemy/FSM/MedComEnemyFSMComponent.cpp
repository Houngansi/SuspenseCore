#include "Core/Enemy/FSM/MedComEnemyFSMComponent.h"
#include "Core/Enemy/FSM/MedComEnemyState.h"
#include "Core/Enemy/FSM/EnemyBehaviorDataAsset.h"
#include "Core/Enemy/MedComEnemyCharacter.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "ProfilingDebugging/CsvProfiler.h"

// Определение лога
DEFINE_LOG_CATEGORY_STATIC(LogMedComEnemyFSM, Log, All);
CSV_DEFINE_CATEGORY(EnemyFSM, true);

UMedComEnemyFSMComponent::UMedComEnemyFSMComponent() : OwnerEnemy(nullptr), BehaviorConfig()
{
    PrimaryComponentTick.bCanEverTick = false; // Не использовать ComponentTick
    bIsInitialized = false;
    bIsProcessingEvent = false;
    bIsChangingState = false;
    CurrentState = nullptr;
    CurrentStateName = NAME_None;
    LastUpdateTime = 0.0f;
}

void UMedComEnemyFSMComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // Получаем владельца
    OwnerEnemy = Cast<AMedComEnemyCharacter>(GetOwner());
    
    // Инициализация будет выполнена через вызов Initialize
    LastUpdateTime = GetWorld()->GetTimeSeconds();
}

void UMedComEnemyFSMComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Очищаем все таймеры при уничтожении компонента
    StopAllStateTimers();
    
    // Выходим из текущего состояния с защитой от рекурсии
    if (CurrentState && IsValid(CurrentState) && OwnerEnemy && IsValid(OwnerEnemy) && !bIsChangingState)
    {
        bIsChangingState = true;
        CurrentState->OnExit(OwnerEnemy);
        bIsChangingState = false;
    }
    
    // Очищаем ссылки
    for (auto& Pair : StateMap)
    {
        if (Pair.Value && IsValid(Pair.Value))
        {
            Pair.Value->MarkAsGarbage();
        }
    }
    StateMap.Empty();
    CurrentState = nullptr;
    
    Super::EndPlay(EndPlayReason);
}

void UMedComEnemyFSMComponent::Initialize(UEnemyBehaviorDataAsset* InBehaviorConfig, AMedComEnemyCharacter* Owner)
{
    CSV_SCOPED_TIMING_STAT(EnemyFSM, Initialize);
    //
    if (!CurrentState)
    {
        UE_LOG(LogMedComEnemyFSM, Error, TEXT("%s: Не удалось создать начальное состояние!"), 
               *GetNameSafe(OwnerEnemy));
    
        // Принудительно пытаемся создать состояние Idle
        UMedComEnemyState* FallbackState = CreateStateInstance(FName("Idle"));
        if (FallbackState)
        {
            CurrentState = FallbackState;
            CurrentStateName = FName("Idle");
            CurrentState->OnEnter(OwnerEnemy);
        }
    }
    //
    // Проверяем входные параметры
    if (!InBehaviorConfig || !Owner)
    {
        UE_LOG(LogMedComEnemyFSM, Error, TEXT("Failed to initialize FSM: Invalid config or owner"));
        return;
    }
    
    // Сохраняем ссылки
    BehaviorConfig = InBehaviorConfig;
    OwnerEnemy = Owner;
    
#if WITH_EDITOR
    // Сохраняем хеш для отслеживания изменений
    BehaviorAssetGuid = BehaviorConfig->GetPackage()->GetSavedHash();
#endif
    
    // Если FSM уже была инициализирована, очищаем старое состояние
    if (bIsInitialized)
    {
        // Выходим из текущего состояния с защитой от рекурсии
        if (CurrentState && !bIsChangingState)
        {
            bIsChangingState = true;
            CurrentState->OnExit(OwnerEnemy);
            bIsChangingState = false;
        }
        
        // Очищаем таймеры
        StopAllStateTimers();
        
        // Очищаем карту состояний
        for (auto& Pair : StateMap)
        {
            if (Pair.Value)
            {
                Pair.Value->OnExit(OwnerEnemy);
                Pair.Value->MarkAsGarbage();
            }
        }
        StateMap.Empty();
        TransitionMap.Empty();
    }
    
    // Создаем экземпляры состояний из конфигурации
    for (const FStateDescription& StateDesc : BehaviorConfig->States)
    {
        UMedComEnemyState* NewState = CreateStateInstance(StateDesc.StateName);
        if (NewState)
        {
            StateMap.Add(StateDesc.StateName, NewState);
        }
    }
    
    // Строим карту переходов для быстрого поиска O(1)
    BuildTransitionMap();
    
    // Устанавливаем начальное состояние
    FName InitialStateName = BehaviorConfig->InitialState;
    if (InitialStateName.IsNone())
    {
        // Если не задано, берем первое из списка
        if (BehaviorConfig->States.Num() > 0)
        {
            InitialStateName = BehaviorConfig->States[0].StateName;
        }
        else
        {
            UE_LOG(LogMedComEnemyFSM, Error, TEXT("FSM initialization failed: No states defined"));
            return;
        }
    }
    
    // Переходим в начальное состояние
    ChangeStateByName(InitialStateName);
    
    bIsInitialized = true;
    
    UE_LOG(LogMedComEnemyFSM, Log, TEXT("%s: FSM initialized with %d states, initial state: %s"), 
           *OwnerEnemy->GetName(), StateMap.Num(), *InitialStateName.ToString());
}

void UMedComEnemyFSMComponent::BuildTransitionMap()
{
    // Очищаем старые данные
    TransitionMap.Empty();
    
    if (!BehaviorConfig)
    {
        return;
    }
    
    // Для каждого состояния строим карту переходов
    for (const FStateDescription& StateDesc : BehaviorConfig->States)
    {
        TMap<EEnemyEvent, FName> StateTransitions;
        
        // Добавляем все переходы
        for (const FStateTransition& Transition : StateDesc.Transitions)
        {
            StateTransitions.Add(Transition.TriggerEvent, Transition.TargetState);
        }
        
        // Добавляем в общую карту
        TransitionMap.Add(StateDesc.StateName, StateTransitions);
    }
    
    // КРИТИЧЕСКАЯ ПРОВЕРКА: Проверяем переход из Idle в Patrol
    TMap<EEnemyEvent, FName>* IdleTransitions = TransitionMap.Find(FName("Idle"));
    if (IdleTransitions)
    {
        FName* PatrolTarget = IdleTransitions->Find(EEnemyEvent::IdleTimeout);
        if (!PatrolTarget || PatrolTarget->IsNone())
        {
            UE_LOG(LogMedComEnemyFSM, Warning, 
                TEXT("КРИТИЧНО: Не найден переход IdleTimeout -> Patrol! Добавляем стандартный..."));
            
            // Добавляем переход по умолчанию
            IdleTransitions->Add(EEnemyEvent::IdleTimeout, FName("Patrol"));
        }
        else
        {
            UE_LOG(LogMedComEnemyFSM, Log, 
                TEXT("Найден переход IdleTimeout -> %s"), *PatrolTarget->ToString());
        }
    }
    else
    {
        UE_LOG(LogMedComEnemyFSM, Error, 
            TEXT("КРИТИЧНАЯ ОШИБКА: Состояние Idle не найдено в карте переходов!"));
    }
    
    // Проверка перехода из Return в Idle
    TMap<EEnemyEvent, FName>* ReturnTransitions = TransitionMap.Find(FName("Return"));
    if (ReturnTransitions)
    {
        FName* IdleTarget = ReturnTransitions->Find(EEnemyEvent::ReturnComplete);
        if (!IdleTarget || IdleTarget->IsNone())
        {
            UE_LOG(LogMedComEnemyFSM, Warning, 
                TEXT("ReturnComplete -> Idle transition not defined! Adding default..."));
            
            // Добавляем переход по умолчанию
            ReturnTransitions->Add(EEnemyEvent::ReturnComplete, FName("Idle"));
        }
    }
    
    UE_LOG(LogMedComEnemyFSM, Verbose, TEXT("Built transition map with %d states"), TransitionMap.Num());
    
    // Дебаг: вывод всех переходов
    for (const auto& StatePair : TransitionMap)
    {
        UE_LOG(LogMedComEnemyFSM, Log, TEXT("Состояние: %s"), *StatePair.Key.ToString());
        for (const auto& TransitionPair : StatePair.Value)
        {
            UE_LOG(LogMedComEnemyFSM, Log, TEXT("  Событие %s -> Состояние %s"), 
                   *UEnum::GetValueAsString(TransitionPair.Key), *TransitionPair.Value.ToString());
        }
    }
}

UMedComEnemyState* UMedComEnemyFSMComponent::CreateStateInstance(const FName& StateName)
{
    if (!BehaviorConfig)
    {
        UE_LOG(LogMedComEnemyFSM, Error, TEXT("%s: Не могу создать состояние - нет конфигурации поведения"), 
               *GetNameSafe(GetOwner()));
        return nullptr;
    }
    
    // Ищем описание состояния в конфигурации
    for (const FStateDescription& StateDesc : BehaviorConfig->States)
    {
        if (StateDesc.StateName == StateName && StateDesc.StateClass)
        {
            // Создаем экземпляр состояния с явным указанием владельца (this)
            UMedComEnemyState* NewState = NewObject<UMedComEnemyState>(this, StateDesc.StateClass);
            if (NewState)
            {
                // Проверка валидности
                if (!IsValid(NewState))
                {
                    UE_LOG(LogMedComEnemyFSM, Error, 
                           TEXT("%s: Созданное состояние %s оказалось невалидным!"), 
                           *GetNameSafe(GetOwner()), *StateName.ToString());
                    return nullptr;
                }
                
                // Устанавливаем ссылку на FSM
                NewState->SetFSMComponent(this);
                
                // Передаем параметры из конфигурации
                NewState->InitializeState(StateDesc.StateParams);
                
                return NewState;
            }
        }
    }
    
    UE_LOG(LogMedComEnemyFSM, Error, TEXT("%s: Не удалось создать состояние: %s - не найдено в конфигурации или неверный класс"), 
           *GetNameSafe(GetOwner()), *StateName.ToString());
    return nullptr;
}

void UMedComEnemyFSMComponent::ProcessFSMEvent(EEnemyEvent Event, AActor* EventInstigator)
{
    CSV_SCOPED_TIMING_STAT(EnemyFSM, ProcessEvent);

    if (!bIsInitialized || !CurrentState || !BehaviorConfig)
    {
        UE_LOG(LogMedComEnemyFSM, Warning,
               TEXT("%s: Cannot process event – FSM not initialized"), 
               OwnerEnemy ? *OwnerEnemy->GetName() : TEXT("Unknown"));
        return;
    }

    // Защита от рекурсивной обработки событий
    if (bIsProcessingEvent)
    {
        UE_LOG(LogMedComEnemyFSM, Warning,
               TEXT("%s: Preventing recursive event processing for %s - adding to queue"),
               *OwnerEnemy->GetName(), *UEnum::GetValueAsString(Event));
        
        EnqueueFSMEvent(Event, EventInstigator);
        return;
    }

    // Устанавливаем флаг обработки
    bIsProcessingEvent = true;

    // ИСПРАВЛЕНИЕ: Специальная обработка для IdleTimeout
    if (Event == EEnemyEvent::IdleTimeout)
    {
        UE_LOG(LogMedComEnemyFSM, Warning, 
               TEXT("%s: ОБРАБОТКА IdleTimeout события (текущее состояние: %s)"),
               *OwnerEnemy->GetName(), *CurrentStateName.ToString());
    }

    // Даём активному состоянию шанс обработать событие
    CurrentState->OnEvent(OwnerEnemy, Event, EventInstigator);

    // Поиск перехода в карте переходов
    const TMap<EEnemyEvent, FName>* StateTransitions = TransitionMap.Find(CurrentStateName);
    if (!StateTransitions) 
    {
        UE_LOG(LogMedComEnemyFSM, Warning,
               TEXT("%s: Не найдены переходы для состояния %s"),
               *OwnerEnemy->GetName(), *CurrentStateName.ToString());
        
        bIsProcessingEvent = false;
        return; 
    }

    const FName* TargetState = StateTransitions->Find(Event);
    if (!TargetState || TargetState->IsNone()) 
    {
        // ИСПРАВЛЕНИЕ: Более подробный лог для IdleTimeout
        if (Event == EEnemyEvent::IdleTimeout)
        {
            UE_LOG(LogMedComEnemyFSM, Error,
                   TEXT("%s: НЕ НАЙДЕН ПЕРЕХОД для IdleTimeout из состояния %s!"),
                   *OwnerEnemy->GetName(), *CurrentStateName.ToString());
            
            // Проверим все возможные переходы из текущего состояния
            UE_LOG(LogMedComEnemyFSM, Warning, TEXT("Доступные переходы из %s:"), *CurrentStateName.ToString());
            for (const auto& Pair : *StateTransitions)
            {
                UE_LOG(LogMedComEnemyFSM, Warning, TEXT("  %s -> %s"), 
                       *UEnum::GetValueAsString(Pair.Key), *Pair.Value.ToString());
            }
        }
        else
        {
            UE_LOG(LogMedComEnemyFSM, Verbose,
                   TEXT("%s: No transition found for event %s in state %s"),
                   *OwnerEnemy->GetName(), *UEnum::GetValueAsString(Event), *CurrentStateName.ToString());
        }
        
        bIsProcessingEvent = false;
        return; 
    }

    // Определяем задержку перехода
    float TransitionDelay = 0.f;
    const FStateDescription* ThisStateDesc = nullptr;
    
    if (BehaviorConfig && BehaviorConfig->States.Num() > 0)
    {
        ThisStateDesc = BehaviorConfig->States.FindByPredicate(
            [this](const FStateDescription& Desc)
            { return Desc.StateName == CurrentStateName; });
    }

    if (ThisStateDesc)
    {
        const FStateTransition* Exact =
            ThisStateDesc->Transitions.FindByPredicate(
                [&](const FStateTransition& Tr)
                { return Tr.TriggerEvent == Event && Tr.TargetState == *TargetState; });

        if (Exact) { TransitionDelay = Exact->Delay; }
    }

    // ИСПРАВЛЕНИЕ: Для IdleTimeout делаем переход немедленно
    if (Event == EEnemyEvent::IdleTimeout)
    {
        TransitionDelay = 0.0f;
        UE_LOG(LogMedComEnemyFSM, Warning,
               TEXT("%s: НЕМЕДЛЕННЫЙ переход по IdleTimeout из %s в %s"),
               *OwnerEnemy->GetName(), *CurrentStateName.ToString(), *TargetState->ToString());
    }

    // Выполняем переход (с задержкой или сразу)
    if (TransitionDelay > 0.f)
    {
        UE_LOG(LogMedComEnemyFSM, Log,
               TEXT("%s: Delayed transition from %s to %s (%.2f s)"),
               *OwnerEnemy->GetName(), *CurrentStateName.ToString(), 
               *TargetState->ToString(), TransitionDelay);
               
        FTimerDelegate DelayDelegate;
        DelayDelegate.BindUObject(
            this,
            &UMedComEnemyFSMComponent::ChangeStateByName,
            *TargetState);

        const FName DelayKey = *FString::Printf(TEXT("DelayedTransition_%s"),
                                                *TargetState->ToString());
        
        // Безопасно добавляем таймер
        FTimerHandle NewHandle;
        GetWorld()->GetTimerManager().SetTimer(
            NewHandle, DelayDelegate, TransitionDelay, false);
        
        // Добавляем в карту после установки таймера
        StateTimers.Add(DelayKey, NewHandle);
    }
    else
    {
        UE_LOG(LogMedComEnemyFSM, Log,
               TEXT("%s: Immediate transition from %s to %s"),
               *OwnerEnemy->GetName(), *CurrentStateName.ToString(), 
               *TargetState->ToString());
               
        ChangeStateByName(*TargetState);
    }
    
    // Снимаем флаг обработки
    bIsProcessingEvent = false;
    
    // Обрабатываем следующее событие в очереди, если есть
    ProcessEventQueue();
}

void UMedComEnemyFSMComponent::EnqueueFSMEvent(EEnemyEvent Event, AActor* EventInstigator)
{
    // Добавляем событие в очередь
    EventQueue.Enqueue(FPendingFSMEvent(Event, EventInstigator));
    
    UE_LOG(LogMedComEnemyFSM, Verbose,
           TEXT("%s: Event %s added to queue"),
           *OwnerEnemy->GetName(), *UEnum::GetValueAsString(Event));
}

void UMedComEnemyFSMComponent::ProcessEventQueue()
{
    // Если очередь пуста или уже обрабатываем событие, выходим
    if (EventQueue.IsEmpty() || bIsProcessingEvent)
    {
        return;
    }
    
    // Извлекаем следующее событие из очереди
    FPendingFSMEvent PendingEvent;
    if (EventQueue.Dequeue(PendingEvent))
    {
        // Обрабатываем событие
        // Здесь мы используем обычный указатель, а не TWeakObjectPtr
        ProcessFSMEvent(PendingEvent.Event, PendingEvent.InstigatorPtr);
    }
}

void UMedComEnemyFSMComponent::ChangeState(TSubclassOf<UMedComEnemyState> NewStateClass)
{
    if (!NewStateClass || !OwnerEnemy)
    {
        UE_LOG(LogMedComEnemyFSM, Error, TEXT("Cannot change state: Invalid state class or owner"));
        return;
    }
    
    // Защита от рекурсивной смены состояний
    if (bIsChangingState)
    {
        UE_LOG(LogMedComEnemyFSM, Warning, 
               TEXT("%s: Preventing recursive state change to %s"), 
               *OwnerEnemy->GetName(), *NewStateClass->GetName());
        return;
    }
    
    // Устанавливаем флаг смены состояния
    bIsChangingState = true;
    
    // Выходим из текущего состояния
    if (CurrentState)
    {
        CurrentState->OnExit(OwnerEnemy);
    }
    
    // Создаем новое состояние
    UMedComEnemyState* NewState = NewObject<UMedComEnemyState>(this, NewStateClass);
    if (NewState)
    {
        // Устанавливаем ссылку на FSM
        NewState->SetFSMComponent(this);
        
        // Выполняем фактическую смену состояния
        FName NewStateName = FName(*NewState->GetClass()->GetName());
        if (PerformStateChange(NewState, NewStateName))
        {
            UE_LOG(LogMedComEnemyFSM, Log, TEXT("%s: Changed state to %s"), 
                   *OwnerEnemy->GetName(), *NewStateName.ToString());
        }
    }
    
    // Снимаем флаг смены состояния
    bIsChangingState = false;
}

void UMedComEnemyFSMComponent::ChangeStateByName(FName StateName)
{
    // Проверяем валидность имени
    if (StateName.IsNone())
    {
        UE_LOG(LogMedComEnemyFSM, Error, TEXT("Cannot change state: Invalid state name"));
        return;
    }
    
    // Защита от рекурсивной смены состояний
    if (bIsChangingState)
    {
        UE_LOG(LogMedComEnemyFSM, Warning, 
               TEXT("%s: Preventing recursive state change to %s"), 
               *OwnerEnemy->GetName(), *StateName.ToString());
        return;
    }
    
    // Сохраняем предыдущее состояние для специальных переходов
    FName PreviousStateName = CurrentStateName;
    
    // Проверяем, не переходим ли в то же состояние
    if (CurrentStateName == StateName)
    {
        UE_LOG(LogMedComEnemyFSM, Verbose, 
               TEXT("%s: Already in state %s, ignoring transition"), 
               *OwnerEnemy->GetName(), *StateName.ToString());
        return;
    }
    
    // Устанавливаем флаг смены состояния
    bIsChangingState = true;
    
    // Ищем состояние в карте или создаем новое
    UMedComEnemyState* NewState = StateMap.FindRef(StateName);
    if (!NewState)
    {
        // Пробуем создать
        NewState = CreateStateInstance(StateName);
        if (NewState)
        {
            StateMap.Add(StateName, NewState);
        }
        else
        {
            UE_LOG(LogMedComEnemyFSM, Error, TEXT("Failed to change state: State %s not found"), 
                   *StateName.ToString());
            bIsChangingState = false;
            return;
        }
    }
    
    // Выходим из текущего состояния
    if (CurrentState)
    {
        CurrentState->OnExit(OwnerEnemy);
    }
    
    // Выполняем фактическую смену состояния
    bool bSuccess = PerformStateChange(NewState, StateName);
    
    if (bSuccess)
    {
        UE_LOG(LogMedComEnemyFSM, Log, TEXT("%s: Changed state to %s from %s"), 
               *OwnerEnemy->GetName(), *StateName.ToString(), *PreviousStateName.ToString());
               
        // *** КЛЮЧЕВОЕ ИСПРАВЛЕНИЕ ***
        // Особая обработка для перехода Return -> Idle
        if (PreviousStateName == FName("Return") && StateName == FName("Idle"))
        {
            // Гарантируем запуск таймера Idle с небольшой задержкой
            FTimerHandle PostReturnTimer;
            GetWorld()->GetTimerManager().SetTimer(
                PostReturnTimer,
                [this]() {
                    if (CurrentStateName == FName("Idle") && !bIsChangingState && !bIsProcessingEvent)
                    {
                        UE_LOG(LogMedComEnemyFSM, Warning, 
                               TEXT("%s: POST-RETURN: Перезапуск Idle таймера"), 
                               *OwnerEnemy->GetName());
                               
                        // Останавливаем все существующие Idle таймеры
                        StopStateTimer(FName("IdleTimer"));
                        
                        // Принудительно запускаем новый таймер Idle
                        StartStateTimer(FName("IdleTimer"), 
                                       BehaviorConfig ? BehaviorConfig->IdleTime : 5.0f, 
                                       false);
                    }
                },
                0.5f,  // Даем небольшую задержку для стабилизации состояния
                false
            );
        }
    }
    else
    {
        UE_LOG(LogMedComEnemyFSM, Error, TEXT("%s: Failed to change state to %s"), 
               *OwnerEnemy->GetName(), *StateName.ToString());
    }
    
    // Снимаем флаг смены состояния
    bIsChangingState = false;
}

bool UMedComEnemyFSMComponent::PerformStateChange(UMedComEnemyState* NewState, FName NewStateName)
{
    if (!NewState || !OwnerEnemy)
    {
        UE_LOG(LogMedComEnemyFSM, Error, TEXT("%s: Невозможно выполнить смену состояния - невалидные параметры"), 
               *GetNameSafe(GetOwner()));
        return false;
    }
    
    // Запоминаем предыдущее состояние для отладки
    UMedComEnemyState* PrevState = CurrentState;
    FName PrevStateName = CurrentStateName;
    
    // Обновляем текущее состояние с проверкой
    CurrentState = NewState;
    CurrentStateName = NewStateName;
    
    // Добавляем в карту, если ещё нет
    if (!StateMap.Contains(NewStateName) || !IsValid(StateMap[NewStateName]))
    {
        StateMap.Add(NewStateName, NewState);
    }
    
    // Проверка после установки
    if (!IsValid(CurrentState))
    {
        UE_LOG(LogMedComEnemyFSM, Error, 
               TEXT("%s: КРИТИЧЕСКАЯ ОШИБКА - состояние стало невалидным сразу после установки!"), 
               *GetNameSafe(GetOwner()));
        
        // Пытаемся восстановить предыдущее состояние
        if (IsValid(PrevState))
        {
            CurrentState = PrevState;
            CurrentStateName = PrevStateName;
            return false;
        }
    }
    
    // Входим в новое состояние
    CurrentState->OnEnter(OwnerEnemy);
    
    return true;
}

bool UMedComEnemyFSMComponent::IsTransitionValid(FName TargetState) const
{
    if (!BehaviorConfig || TargetState.IsNone() || CurrentStateName.IsNone())
    {
        return false;
    }
    
    // Проверяем, существует ли такое состояние
    bool bStateExists = false;
    for (const FStateDescription& StateDesc : BehaviorConfig->States)
    {
        if (StateDesc.StateName == TargetState)
        {
            bStateExists = true;
            break;
        }
    }
    
    if (!bStateExists)
    {
        return false;
    }
    
    // Проверяем, есть ли прямой переход из текущего состояния
    const TMap<EEnemyEvent, FName>* StateTransitions = TransitionMap.Find(CurrentStateName);
    if (!StateTransitions)
    {
        return false;
    }
    
    // Проверяем, есть ли хотя бы один переход в целевое состояние
    for (const auto& Pair : *StateTransitions)
    {
        if (Pair.Value == TargetState)
        {
            return true;
        }
    }
    
    return false;
}

FGameplayTag UMedComEnemyFSMComponent::GetCurrentStateTag() const
{
    return CurrentState ? CurrentState->GetStateTag() : FGameplayTag();
}

void UMedComEnemyFSMComponent::StartStateTimer(const FName& TimerName, float Duration, bool bLoop)
{
    if (!GetWorld())
    {
        UE_LOG(LogMedComEnemyFSM, Error, TEXT("Cannot start timer %s: World is invalid"), 
               *TimerName.ToString());
        return;
    }
    
    // Останавливаем существующий таймер, если есть
    StopStateTimer(TimerName);
    
    // Создаем делегат
    FTimerDelegate TimerDelegate;
    TimerDelegate.BindUObject(this, &UMedComEnemyFSMComponent::OnStateTimerFired, TimerName);
    
    // Создаем новый таймер
    FTimerHandle NewTimerHandle;
    GetWorld()->GetTimerManager().SetTimer(NewTimerHandle, TimerDelegate, Duration, bLoop);
    
    // Сохраняем хендл
    StateTimers.Add(TimerName, NewTimerHandle);
    
    UE_LOG(LogMedComEnemyFSM, Log, TEXT("%s: Started state timer: %s, duration: %.2f, loop: %d"), 
           OwnerEnemy ? *OwnerEnemy->GetName() : TEXT("Unknown"),
           *TimerName.ToString(), Duration, bLoop ? 1 : 0);
           
    // ИСПРАВЛЕНИЕ: Специальная обработка для IdleTimer - принудительно логируем его
    if (TimerName == FName("IdleTimer"))
    {
        UE_LOG(LogMedComEnemyFSM, Warning, TEXT("%s: ЗАПУЩЕН ТАЙМЕР IDLE на %.2f секунд"),
               OwnerEnemy ? *OwnerEnemy->GetName() : TEXT("Unknown"), Duration);
    }
}

void UMedComEnemyFSMComponent::StopStateTimer(const FName& TimerName)
{
    if (!GetWorld())
    {
        return;
    }
    
    // Ищем таймер в карте
    FTimerHandle* TimerHandle = StateTimers.Find(TimerName);
    if (TimerHandle && GetWorld()->GetTimerManager().IsTimerActive(*TimerHandle))
    {
        // Останавливаем таймер
        GetWorld()->GetTimerManager().ClearTimer(*TimerHandle);
        UE_LOG(LogMedComEnemyFSM, Verbose, TEXT("Stopped state timer: %s"), *TimerName.ToString());
    }
}

void UMedComEnemyFSMComponent::StopAllStateTimers()
{
    if (!GetWorld()) { return; }

    for (auto& Pair : StateTimers)
    {
        FTimerHandle& Handle = Pair.Value;
        if (GetWorld()->GetTimerManager().IsTimerActive(Handle))
        {
            GetWorld()->GetTimerManager().ClearTimer(Handle);
        }
    }

    GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
    StateTimers.Empty();

    UE_LOG(LogMedComEnemyFSM, Verbose, TEXT("Stopped all state timers"));
}

// Переопределение метода OnStateTimerFired для более надежной работы таймеров
void UMedComEnemyFSMComponent::OnStateTimerFired(FName TimerName)
{
    UE_LOG(LogMedComEnemyFSM, Verbose, TEXT("State timer fired: %s"), *TimerName.ToString());
    
    // Проверяем, что состояние валидное
    if (!CurrentState || !OwnerEnemy)
    {
        UE_LOG(LogMedComEnemyFSM, Warning, 
               TEXT("Timer %s fired but FSM is in invalid state - ignoring"), 
               *TimerName.ToString());
        return;
    }
    
    // Обработка в зависимости от типа таймера
    if (TimerName == FName("IdleTimer"))
    {
        // Проверяем, что текущее состояние действительно Idle
        if (CurrentStateName == FName("Idle"))
        {
            UE_LOG(LogMedComEnemyFSM, Warning, 
                   TEXT("%s: IdleTimer сработал - активация перехода в Patrol!"), 
                   *OwnerEnemy->GetName());
                   
            // Проверяем, есть ли специальный флаг перехода из Return
            bool bFromReturn = false;
            FString ReturnToIdleStr = GetCustomData(TEXT("ReturnToIdleTransition"));
            if (ReturnToIdleStr == TEXT("true"))
            {
                bFromReturn = true;
                UE_LOG(LogMedComEnemyFSM, Warning, 
                       TEXT("%s: Переход в Patrol после Return->Idle цепочки"), 
                       *OwnerEnemy->GetName());
                       
                // Очищаем флаг после использования
                ClearCustomData(TEXT("ReturnToIdleTransition"));
            }
            
            // Проверяем наличие предыдущего перехода из Return
            FString LastTransition = GetCustomData(TEXT("LastStateTransition"));
            if (!LastTransition.IsEmpty())
            {
                UE_LOG(LogMedComEnemyFSM, Warning, 
                       TEXT("%s: Последний переход: %s"), 
                       *OwnerEnemy->GetName(), *LastTransition);
                       
                // Очищаем флаг после использования
                ClearCustomData(TEXT("LastStateTransition"));
            }
            
            // Прямой переход в Patrol для случаев после Return
            if (bFromReturn)
            {
                // Принудительно меняем состояние
                UE_LOG(LogMedComEnemyFSM, Warning, 
                       TEXT("%s: Принудительный переход в Patrol после Return"), 
                       *OwnerEnemy->GetName());
                ChangeStateByName(FName("Patrol"));
            }
            else
            {
                // Обычная обработка через события
                EnqueueFSMEvent(EEnemyEvent::IdleTimeout);
            }
            
            // Дополнительная защита: через 0.5 секунды проверим, произошел ли переход
            FTimerHandle VerificationTimer;
            GetWorld()->GetTimerManager().SetTimer(
                VerificationTimer,
                [this]() {
                    if (CurrentStateName == FName("Idle"))
                    {
                        UE_LOG(LogMedComEnemyFSM, Error, 
                               TEXT("%s: АВАРИЙНАЯ СИТУАЦИЯ - до сих пор в Idle! Принудительно активируем переход."), 
                               *OwnerEnemy->GetName());
                               
                        // Принудительный переход в Patrol в случае застревания
                        if (!bIsChangingState && !bIsProcessingEvent)
                        {
                            ChangeStateByName(FName("Patrol"));
                        }
                    }
                },
                0.5f,  // Полсекунды должно хватить для перехода в нормальной ситуации
                false
            );
        }
        else
        {
            UE_LOG(LogMedComEnemyFSM, Warning, 
                   TEXT("%s: IdleTimer сработал, но текущее состояние не Idle (%s)!"), 
                   *OwnerEnemy->GetName(), *CurrentStateName.ToString());
        }
    }
    else if (TimerName == FName("ReturnCompleteTimer"))
    {
        if (CurrentStateName == FName("Return"))
        {
            UE_LOG(LogMedComEnemyFSM, Warning, 
                   TEXT("%s: Сработал ReturnCompleteTimer - активация перехода в Idle"), 
                   *OwnerEnemy->GetName());
                   
            // Устанавливаем флаг для отслеживания
            SetCustomData(TEXT("LastStateTransition"), TEXT("Timer-Return->Idle"));
            
            // Отправляем событие
            EnqueueFSMEvent(EEnemyEvent::ReturnComplete);
        }
    }
    else
    {
        // Любой другой таймер - передаем в текущее состояние
        // Используем защиту от рекурсии только если не обрабатываем сейчас другое событие
        if (!bIsProcessingEvent && !bIsChangingState)
        {
            CurrentState->OnTimerFired(OwnerEnemy, TimerName);
        }
        else
        {
            // Отложенная обработка таймера
            UE_LOG(LogMedComEnemyFSM, Verbose, 
                   TEXT("%s: Откладываем обработку таймера %s из-за текущей обработки события"), 
                   *OwnerEnemy->GetName(), *TimerName.ToString());
            
            FTimerHandle DeferredTimerHandle;
            GetWorld()->GetTimerManager().SetTimer(
                DeferredTimerHandle,
                [this, TimerName]() {
                    if (CurrentState && OwnerEnemy)
                    {
                        CurrentState->OnTimerFired(OwnerEnemy, TimerName);
                    }
                },
                0.1f,
                false
            );
        }
    }
}

void UMedComEnemyFSMComponent::AddStateTimer(const FName& TimerName, const FTimerHandle& Handle)
{
    // Проверка на существующий таймер
    FTimerHandle* ExistingTimer = StateTimers.Find(TimerName);
    if (ExistingTimer)
    {
        // Если таймер с таким именем уже существует, сначала очищаем его
        if (GetWorld() && GetWorld()->GetTimerManager().TimerExists(*ExistingTimer))
        {
            GetWorld()->GetTimerManager().ClearTimer(*ExistingTimer);
        }
        
        // Обновляем существующую запись
        *ExistingTimer = Handle;
        
        UE_LOG(LogMedComEnemyFSM, Verbose, TEXT("Updated existing timer: %s"), 
               *TimerName.ToString());
    }
    else
    {
        // Добавляем новый таймер
        StateTimers.Add(TimerName, Handle);
        
        UE_LOG(LogMedComEnemyFSM, Verbose, TEXT("Added new timer: %s"), 
               *TimerName.ToString());
    }
}

void UMedComEnemyFSMComponent::SetCustomData(const FString& Key, const FString& Value)
{
    // Если ключ уже существует, обновляем значение
    if (CustomTransitionData.Contains(Key))
    {
        CustomTransitionData[Key] = Value;
        UE_LOG(LogMedComEnemyFSM, Verbose, TEXT("%s: Обновлены данные FSM: %s = %s"), 
               *GetNameSafe(OwnerEnemy), *Key, *Value);
    }
    else
    {
        // Иначе добавляем новую пару ключ-значение
        CustomTransitionData.Add(Key, Value);
        UE_LOG(LogMedComEnemyFSM, Verbose, TEXT("%s: Добавлены данные FSM: %s = %s"), 
               *GetNameSafe(OwnerEnemy), *Key, *Value);
    }
}

FString UMedComEnemyFSMComponent::GetCustomData(const FString& Key, const FString& DefaultValue) const
{
    const FString* FoundValue = CustomTransitionData.Find(Key);
    if (FoundValue)
    {
        UE_LOG(LogMedComEnemyFSM, Verbose, TEXT("%s: Получены данные FSM: %s = %s"), 
               *GetNameSafe(OwnerEnemy), *Key, *(*FoundValue));
        return *FoundValue;
    }
    
    UE_LOG(LogMedComEnemyFSM, Verbose, TEXT("%s: Данные FSM не найдены для ключа: %s, возвращено значение по умолчанию: %s"), 
           *GetNameSafe(OwnerEnemy), *Key, *DefaultValue);
    return DefaultValue;
}

void UMedComEnemyFSMComponent::ClearCustomData(const FString& Key)
{
    if (CustomTransitionData.Remove(Key) > 0)
    {
        UE_LOG(LogMedComEnemyFSM, Verbose, TEXT("%s: Удалены данные FSM: %s"), 
               *GetNameSafe(OwnerEnemy), *Key);
    }
    else
    {
        UE_LOG(LogMedComEnemyFSM, Verbose, TEXT("%s: Попытка удалить несуществующие данные FSM: %s"), 
               *GetNameSafe(OwnerEnemy), *Key);
    }
}

void UMedComEnemyFSMComponent::OnSeePawn(APawn* SeenPawn)
{
    // Проверка, что объект это игрок или вражеская фракция
    if (SeenPawn && SeenPawn->IsPlayerControlled())
    {
        // Используем асинхронную очередь вместо прямого вызова для безопасности
        EnqueueFSMEvent(EEnemyEvent::PlayerSeen, SeenPawn);
    }
}

void UMedComEnemyFSMComponent::OnHearNoise(APawn* NoiseInstigator, const FVector& Location, float Volume)
{
    // Обработка звуков для AI
    if (NoiseInstigator && NoiseInstigator->IsPlayerControlled())
    {
        // Добавляем в очередь для безопасной обработки
        EnqueueFSMEvent(EEnemyEvent::PlayerSeen, NoiseInstigator);
    }
}

void UMedComEnemyFSMComponent::OnDamaged(AActor* DamagedActor, float Damage, AController* InstigatedBy, 
                                       FVector HitLocation, UPrimitiveComponent* FHitComponent, 
                                       FName BoneName, FVector ShotFromDirection, 
                                       const UDamageType* DamageType, AActor* DamageCauser)
{
    // Обработка получения урона через очередь
    EnqueueFSMEvent(EEnemyEvent::TookDamage, DamageCauser);
}

void UMedComEnemyFSMComponent::MasterTick(float DeltaTime)
{
    // Проверка базовых условий
    if (!IsValid(this) || !bIsInitialized || !IsValid(OwnerEnemy))
    {
        UE_LOG(LogMedComEnemyFSM, Error, TEXT("%s: MasterTick - базовая проверка не пройдена"), 
               *GetNameSafe(GetOwner()));
        return;
    }
    
    // Восстановление состояния, если оно невалидно
    if (!IsValid(CurrentState))
    {
        UE_LOG(LogMedComEnemyFSM, Warning, TEXT("%s: MasterTick - Восстановление невалидного CurrentState"), 
               *GetNameSafe(GetOwner()));
        
        // Попытка восстановить состояние из карты состояний
        if (!CurrentStateName.IsNone())
        {
            UMedComEnemyState* StateFromMap = nullptr;
            
            // Проверяем, есть ли состояние в карте
            if (StateMap.Contains(CurrentStateName))
            {
                StateFromMap = StateMap[CurrentStateName];
                
                // Если состояние в карте тоже невалидно, пересоздаем его
                if (!IsValid(StateFromMap))
                {
                    UE_LOG(LogMedComEnemyFSM, Warning, TEXT("%s: Состояние в карте также невалидно, пересоздаем"), 
                          *GetNameSafe(GetOwner()));
                          
                    StateFromMap = CreateStateInstance(CurrentStateName);
                    if (StateFromMap)
                    {
                        StateMap[CurrentStateName] = StateFromMap;
                    }
                }
            }
            else
            {
                // Если состояния нет в карте, пытаемся создать его
                StateFromMap = CreateStateInstance(CurrentStateName);
                if (StateFromMap)
                {
                    StateMap.Add(CurrentStateName, StateFromMap);
                }
            }
            
            // Если удалось восстановить или создать состояние
            if (IsValid(StateFromMap))
            {
                CurrentState = StateFromMap;
                UE_LOG(LogMedComEnemyFSM, Warning, TEXT("%s: Состояние %s восстановлено"), 
                      *GetNameSafe(GetOwner()), *CurrentStateName.ToString());
            }
            else if (CurrentStateName != FName("Idle"))
            {
                // Если восстановление текущего состояния не удалось, 
                // и оно не Idle, пробуем вернуться к Idle
                UE_LOG(LogMedComEnemyFSM, Warning, TEXT("%s: Не удалось восстановить %s, возвращаемся к Idle"), 
                      *GetNameSafe(GetOwner()), *CurrentStateName.ToString());
                      
                UMedComEnemyState* IdleState = StateMap.FindRef(FName("Idle"));
                if (!IsValid(IdleState))
                {
                    IdleState = CreateStateInstance(FName("Idle"));
                    if (IdleState)
                    {
                        StateMap.Add(FName("Idle"), IdleState);
                    }
                }
                
                if (IsValid(IdleState))
                {
                    CurrentState = IdleState;
                    CurrentStateName = FName("Idle");
                    CurrentState->OnEnter(OwnerEnemy);
                }
            }
        }
        
        // Если всё равно не получилось восстановить состояние
        if (!IsValid(CurrentState))
        {
            UE_LOG(LogMedComEnemyFSM, Error, TEXT("%s: Не удалось восстановить состояние!"), 
                  *GetNameSafe(GetOwner()));
            return;
        }
    }
    
    // Продолжаем обычную обработку
    ProcessEventQueue();
    
    if (!bIsChangingState && !bIsProcessingEvent)
    {
        CurrentState->ProcessTick(OwnerEnemy, DeltaTime);
    }
}

#if WITH_EDITOR
void UMedComEnemyFSMComponent::OnDataAssetChanged()
{
    // Проверяем, изменился ли Data Asset и не происходит ли это в PIE
    if (BehaviorConfig && BehaviorConfig->GetPackage()->GetSavedHash() != BehaviorAssetGuid && ShouldUpdateInEditor())
    {
        UE_LOG(LogMedComEnemyFSM, Log, TEXT("BehaviorAsset changed, reinitializing FSM"));
        
        // Переинициализируем FSM с новой конфигурацией
        Initialize(BehaviorConfig, OwnerEnemy);
    }
}

bool UMedComEnemyFSMComponent::ShouldUpdateInEditor() const
{
#if WITH_EDITORONLY_DATA
    // Проверка, что объект не шаблон и не в режиме PIE/Simulate
    return !IsTemplate() && !GetWorld()->IsGameWorld();
#else
    return false;
#endif
}
#endif