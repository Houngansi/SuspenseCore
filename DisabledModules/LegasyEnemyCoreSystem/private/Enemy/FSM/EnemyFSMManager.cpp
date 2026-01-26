#include "Core/Enemy/FSM/EnemyFSMManager.h"
#include "Core/Enemy/FSM/MedComEnemyFSMComponent.h"
#include "TimerManager.h"
#include "Engine/Engine.h"

// Определение лога
DEFINE_LOG_CATEGORY_STATIC(LogEnemyFSM, Log, All);

void UEnemyFSMManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    // Сброс начального индекса и аккумулированного времени
    LastProcessedIndex = 0;
    AccumulatedTime = 0.0f;
    
    UE_LOG(LogEnemyFSM, Log, TEXT("Enemy FSM Manager initialized"));
}

void UEnemyFSMManager::Deinitialize()
{
    // Очистка списка FSM
    RegisteredFSMs.Empty();
    
    Super::Deinitialize();
    
    UE_LOG(LogEnemyFSM, Log, TEXT("Enemy FSM Manager deinitialized"));
}

void UEnemyFSMManager::RegisterFSM(UMedComEnemyFSMComponent* FSM)
{
    if (FSM)
    {
        RegisteredFSMs.AddUnique(FSM);
    }
}

void UEnemyFSMManager::UnregisterFSM(UMedComEnemyFSMComponent* FSM)
{
    if (FSM)
    {
        RegisteredFSMs.Remove(FSM);
        
        // Проверяем, не был ли удаленный FSM после индекса последней обработки
        if (LastProcessedIndex >= RegisteredFSMs.Num())
        {
            LastProcessedIndex = 0;
        }
    }
}

void UEnemyFSMManager::Tick(float DeltaTime)
{
    // Вызываем GlobalTick напрямую из Tick
    AccumulatedTime += DeltaTime;
    GlobalTick();
}

TStatId UEnemyFSMManager::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UEnemyFSMManager, STATGROUP_Tickables);
}

void UEnemyFSMManager::GlobalTick()
{
    if (!GetWorld())
        return;
        
    // Получаем реальное DeltaTime между кадрами
    float DeltaTime = GetWorld()->GetDeltaSeconds();
    
    // Удаляем невалидные FSM
    RegisteredFSMs.RemoveAll([](const TWeakObjectPtr<UMedComEnemyFSMComponent>& Ptr) {
        return !Ptr.IsValid();
    });
    
    // Если нет FSM, выходим
    if (RegisteredFSMs.Num() == 0)
        return;
    
    // Выполняем time-sliced обновление
    PerformTimeSlicedUpdate(DeltaTime);
}

void UEnemyFSMManager::PerformTimeSlicedUpdate(float DeltaTime)
{
    double StartTime = FPlatformTime::Seconds();
    int32 TotalCount = RegisteredFSMs.Num();
    int32 Processed = 0;
    
    // Обрабатываем до ChunkSize FSM или до конца массива
    while (Processed < ChunkSize && Processed < TotalCount)
    {
        int32 Index = (LastProcessedIndex + Processed) % TotalCount;
        
        if (UMedComEnemyFSMComponent* FSM = RegisteredFSMs[Index].Get())
        {
            FSM->MasterTick(DeltaTime);
        }
        
        Processed++;
        
        // Проверяем время выполнения после каждой обработки
        double ElapsedMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
        if (ElapsedMs > MaxTimeSliceBudgetMs)
        {
            // Превышен бюджет времени - выходим из цикла
            AdjustChunkSize(ElapsedMs);
            LastProcessedIndex = (LastProcessedIndex + Processed) % TotalCount;
            return;
        }
    }
    
    // Если дошли сюда, значит обработали весь чанк или все FSM
    LastProcessedIndex = (LastProcessedIndex + Processed) % TotalCount;
    
    // Измеряем общее время для корректировки размера чанка
    double TotalMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
    AdjustChunkSize(TotalMs);
}

void UEnemyFSMManager::AdjustChunkSize(double ElapsedMs)
{
    // Адаптивная настройка размера чанка
    if (ElapsedMs < (MaxTimeSliceBudgetMs * 0.5))
    {
        // Если используем меньше половины бюджета, можем увеличить чанк
        ChunkSize = FMath::Clamp(ChunkSize + 5, MinChunkSize, MaxChunkSize);
    }
    else if (ElapsedMs > MaxTimeSliceBudgetMs)
    {
        // Если превысили бюджет, уменьшаем размер чанка
        ChunkSize = FMath::Clamp(ChunkSize - 5, MinChunkSize, MaxChunkSize);
    }
}