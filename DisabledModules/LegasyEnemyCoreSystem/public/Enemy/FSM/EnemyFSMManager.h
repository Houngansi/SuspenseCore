#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EnemyFSMManager.generated.h"

class UMedComEnemyFSMComponent;

/**
 * Глобальный менеджер для централизованного обновления всех FSM компонентов
 * Оптимизирует обновление путем замены множества таймеров на единый цикл обновления
 * с time-slice поддержкой для равномерной нагрузки
 */
UCLASS()
class MEDCOMCORE_API UEnemyFSMManager : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// Начало/конец работы подсистемы
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
    
	// Регистрация/отмена FSM компонентов
	void RegisterFSM(UMedComEnemyFSMComponent* FSM);
	void UnregisterFSM(UMedComEnemyFSMComponent* FSM);
    
	// Глобальный тик для всех FSM с time-slice поддержкой
	void GlobalTick();
    
	// FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return false; }
	virtual bool IsTickableInEditor() const override { return false; }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual bool IsTickable() const override { return true; }

private:
	// Зарегистрированные FSM компоненты
	TArray<TWeakObjectPtr<UMedComEnemyFSMComponent>> RegisteredFSMs;
    
	// Таймер обновлений (для обратной совместимости)
	FTimerHandle MasterTickHandle;
    
	// Базовый размер пакета для обработки за один раз
	int32 ChunkSize = 20;
    
	// Минимальный и максимальный размер чанка
	int32 MinChunkSize = 10;
	int32 MaxChunkSize = 100;
    
	// Максимальный бюджет времени на обработку (ms)
	float MaxTimeSliceBudgetMs = 3.0f;
    
	// Индекс последнего обработанного FSM компонента
	int32 LastProcessedIndex = 0;
    
	// Вспомогательные методы
	void PerformTimeSlicedUpdate(float DeltaTime);
	void AdjustChunkSize(double ElapsedMs);
    
	// Аккумулированное время с прошлого обновления (для ограничения частоты)
	float AccumulatedTime = 0.0f;
};