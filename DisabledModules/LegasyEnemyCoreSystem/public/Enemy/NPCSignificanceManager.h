#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NPCSignificanceManager.generated.h"

class AMedComEnemyCharacter;

/**
 * Глобальный менеджер для управления LOD ботов в зависимости от дистанции до игрока.
 * Использует покадровое обновление для плавного перехода между уровнями детализации.
 */
UCLASS()
class MEDCOMCORE_API UNPCSignificanceManager : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// Начало/конец работы подсистемы
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
    
	// Регистрация бота в системе LOD
	void RegisterNPC(AMedComEnemyCharacter* NPC);
    
	// Отмена регистрации бота
	void UnregisterNPC(AMedComEnemyCharacter* NPC);
    
	// FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return false; }
	virtual bool IsTickableInEditor() const override { return false; }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual bool IsTickable() const override { return true; }

private:
	// Зарегистрированные боты
	TArray<TWeakObjectPtr<AMedComEnemyCharacter>> RegisteredNPCs;
    
	// Максимальное количество ботов для обновления за один тик
	int32 MaxNPCUpdatesPerFrame = 30;
    
	// Индекс последнего обработанного бота
	int32 LastProcessedIndex = 0;
    
	// Аккумулированное время для управления частотой обновления
	float AccumulatedTime = 0.0f;
	float UpdateInterval = 0.25f;  // 4 раза в секунду
    
	// Равномерно распределяет обновления по группам ботов
	void UpdateNPCBatch(float DeltaTime);
};