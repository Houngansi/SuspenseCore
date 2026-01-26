#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CrowdManagerSubsystem.generated.h"

class AMedComEnemyCharacter;

/**
 * Управляет движением групп ботов с оптимизацией запросов к навигации
 * и разрешением коллизий между ними.
 * Использует покадровое обновление для плавного движения.
 */
UCLASS()
class MEDCOMCORE_API UCrowdManagerSubsystem : public UWorldSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    
    // Регистрация и отмена регистрации агентов
    void RegisterAgent(AMedComEnemyCharacter* Agent);
    void UnregisterAgent(AMedComEnemyCharacter* Agent);
    
    // Основной метод управления перемещением (устаревший, для обратной совместимости)
    void UpdateCrowdMovement();
    
    // Обновление стратегии движения для конкретного агента
    void RequestAgentMove(AMedComEnemyCharacter* Agent, const FVector& Destination);
    
    // Получить актуальный вектор скорости для агента
    FVector GetAgentVelocity(AMedComEnemyCharacter* Agent);
    
    // FTickableGameObject interface
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickableWhenPaused() const override { return false; }
    virtual bool IsTickableInEditor() const override { return false; }
    virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
    virtual bool IsTickable() const override { return true; }

private:
    // Размер ячейки для пространственного хеширования
    static constexpr float GridCellSize = 500.0f;
    
    // Обработка столкновений между агентами через пространственное хеширование
    void ResolveAgentCollisions(float DeltaTime);
    
    // Оптимизация запросов к навигационной системе
    void BatchProcessPathRequests();
    
    // Структура для хранения данных агента
    struct FCrowdAgentData
    {
        TWeakObjectPtr<AMedComEnemyCharacter> Agent;
        FVector TargetDestination;
        FVector CurrentVelocity;
        float Speed;
        bool bHasPathRequest;
        bool bIsMoving;
        float DistanceToTarget;
        
        FCrowdAgentData() : 
            TargetDestination(FVector::ZeroVector),
            CurrentVelocity(FVector::ZeroVector),
            Speed(300.0f),
            bHasPathRequest(false),
            bIsMoving(false),
            DistanceToTarget(0.0f)
        {}
    };
    
    // Список зарегистрированных агентов
    TMap<TWeakObjectPtr<AMedComEnemyCharacter>, FCrowdAgentData> AgentDataMap;
    
    // Список агентов с неразрешенными запросами пути
    TArray<TWeakObjectPtr<AMedComEnemyCharacter>> PendingPathRequests;
    
    // Максимальное количество одновременных запросов пути
    int32 MaxPathRequestsPerFrame = 10;
    
    // Параметры коллизий
    float CollisionRadius = 100.0f;
    float AvoidanceStrength = 0.5f;
    
    // Аккумулированное дельта-время для управления частотой обработки коллизий
    float AccumulatedDeltaTime = 0.0f;
    float CollisionCheckInterval = 0.05f;  // 20 раз в секунду
    
    // Обновление движения ботов и обработка коллизий
    void CrowdUpdateTick(float DeltaTime);
    
    // Структура для пространственного хеширования
    struct FSpatialHashGrid
    {
        // Хеш-функция для позиции
        uint32 GetPositionHash(const FVector& Position) const
        {
            int32 X = FMath::FloorToInt(Position.X / GridCellSize);
            int32 Y = FMath::FloorToInt(Position.Y / GridCellSize);
            return ::HashCombine(::GetTypeHash(X), ::GetTypeHash(Y));
        }
        
        // Получить ячейку для позиции
        TArray<TWeakObjectPtr<AMedComEnemyCharacter>> GetCellAgents(const FVector& Position) const
        {
            TArray<TWeakObjectPtr<AMedComEnemyCharacter>> Result;
            
            uint32 CellHash = GetPositionHash(Position);
            if (const auto* CellAgents = Grid.Find(CellHash))
            {
                Result.Append(*CellAgents);
            }
            
            // Проверяем 8 соседних ячеек
            for (int32 dx = -1; dx <= 1; ++dx)
            {
                for (int32 dy = -1; dy <= 1; ++dy)
                {
                    if (dx == 0 && dy == 0) continue;
                    FVector NeighborPos = Position + FVector(dx * GridCellSize, dy * GridCellSize, 0);
                    uint32 NeighborHash = GetPositionHash(NeighborPos);
                    if (const auto* NeighborAgents = Grid.Find(NeighborHash))
                    {
                        Result.Append(*NeighborAgents);
                    }
                }
            }
            
            return Result;
        }
        
        // Добавить агента в сетку
        void AddAgent(const FVector& Position, TWeakObjectPtr<AMedComEnemyCharacter> Agent)
        {
            uint32 CellHash = GetPositionHash(Position);
            Grid.FindOrAdd(CellHash).AddUnique(Agent);
        }
        
        // Очистить сетку
        void Clear()
        {
            Grid.Empty();
        }
        
        // Хранилище агентов по хешу ячейки
        TMap<uint32, TArray<TWeakObjectPtr<AMedComEnemyCharacter>>> Grid;
    };
    
    // Сетка для пространственного хеширования
    FSpatialHashGrid SpatialGrid;
    
    // Построение сетки для пространственного хеширования
    void BuildSpatialGrid();
};