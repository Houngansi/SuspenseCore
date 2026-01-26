#include "Core/Enemy/NPCSignificanceManager.h"
#include "Core/Enemy/MedComEnemyCharacter.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

// Определение лога
DEFINE_LOG_CATEGORY_STATIC(LogNPCSignificance, Log, All);

void UNPCSignificanceManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    // Инициализация переменных
    LastProcessedIndex = 0;
    AccumulatedTime = 0.0f;
    
    UE_LOG(LogNPCSignificance, Log, TEXT("NPC Significance Manager initialized with per-frame updates"));
}

void UNPCSignificanceManager::Deinitialize()
{
    // Очистка списка ботов
    RegisteredNPCs.Empty();
    
    Super::Deinitialize();
    
    UE_LOG(LogNPCSignificance, Log, TEXT("NPC Significance Manager deinitialized"));
}

void UNPCSignificanceManager::Tick(float DeltaTime)
{
    // Накапливаем время для контроля частоты обновления
    AccumulatedTime += DeltaTime;
    
    // Обновляем только с определенной частотой, чтобы не тратить ресурсы
    if (AccumulatedTime >= UpdateInterval)
    {
        UpdateNPCBatch(AccumulatedTime);
        AccumulatedTime = 0.0f;
    }
}

TStatId UNPCSignificanceManager::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UNPCSignificanceManager, STATGROUP_Tickables);
}

void UNPCSignificanceManager::RegisterNPC(AMedComEnemyCharacter* NPC)
{
    if (NPC)
    {
        RegisteredNPCs.AddUnique(NPC);
    }
}

void UNPCSignificanceManager::UnregisterNPC(AMedComEnemyCharacter* NPC)
{
    if (NPC)
    {
        RegisteredNPCs.Remove(NPC);
        
        // Проверяем, не был ли удаленный NPC после индекса последней обработки
        if (LastProcessedIndex >= RegisteredNPCs.Num())
        {
            LastProcessedIndex = 0;
        }
    }
}

void UNPCSignificanceManager::UpdateNPCBatch(float DeltaTime)
{
    // Получаем всех локальных игроков
    TArray<AActor*> PlayerActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), PlayerActors);
    
    // Фильтруем только игроков
    TArray<FVector> PlayerPositions;
    for (AActor* Actor : PlayerActors)
    {
        APawn* Pawn = Cast<APawn>(Actor);
        if (Pawn && Pawn->IsPlayerControlled())
        {
            PlayerPositions.Add(Pawn->GetActorLocation());
        }
    }
    
    // Если нет игроков, выходим
    if (PlayerPositions.Num() == 0)
        return;
    
    // Удаляем невалидные ссылки
    RegisteredNPCs.RemoveAll([](const TWeakObjectPtr<AMedComEnemyCharacter>& Ptr) {
        return !Ptr.IsValid();
    });
    
    // Если нет ботов, выходим
    if (RegisteredNPCs.Num() == 0)
        return;
    
    // Обрабатываем только часть ботов за один вызов
    int32 NumNPCs = RegisteredNPCs.Num();
    int32 BatchSize = FMath::Min(MaxNPCUpdatesPerFrame, NumNPCs);
    
    for (int32 i = 0; i < BatchSize; ++i)
    {
        // Вычисляем индекс текущего бота с учетом циклического обхода
        int32 CurrentIndex = (LastProcessedIndex + i) % NumNPCs;
        
        if (AMedComEnemyCharacter* NPC = RegisteredNPCs[CurrentIndex].Get())
        {
            // Находим минимальную дистанцию до игрока
            float MinDistance = FLT_MAX;
            for (const FVector& PlayerPos : PlayerPositions)
            {
                float Distance = FVector::Distance(NPC->GetActorLocation(), PlayerPos);
                MinDistance = FMath::Min(MinDistance, Distance);
            }
            
            // Обновляем LOD бота с плавным переходом
            NPC->UpdateDetailLevel(MinDistance);
        }
    }
    
    // Обновляем индекс последнего обработанного бота
    LastProcessedIndex = (LastProcessedIndex + BatchSize) % NumNPCs;
}