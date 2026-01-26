#include "Core/Enemy/FSM/States/MedComDeathState.h"
#include "Core/Enemy/MedComEnemyCharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "AIController.h"
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"

// Определение лога
DEFINE_LOG_CATEGORY_STATIC(LogMedComDeathState, Log, All);

// Статические константы для имен таймеров
const FName UMedComDeathState::RagdollTimerName = TEXT("RagdollTimer");
const FName UMedComDeathState::DespawnTimerName = TEXT("DespawnTimer");

UMedComDeathState::UMedComDeathState()
{
    // Устанавливаем тег состояния
    StateTag = FGameplayTag::RequestGameplayTag("State.Death");
}

void UMedComDeathState::OnEnter(AMedComEnemyCharacter* Owner)
{
    Super::OnEnter(Owner);
    
    if (!Owner)
    {
        return;
    }
    
    // Остановка движения если еще не остановлено
    AAIController* AIController = Cast<AAIController>(Owner->GetController());
    if (AIController)
    {
        AIController->StopMovement();
    }
    
    // Отключаем компонент восприятия
    if (UAIPerceptionComponent* PerceptionComp = Owner->GetPerceptionComponent())
    {
        PerceptionComp->SetComponentTickEnabled(false);
    }
    
    // Освобождаем все ресурсы
    //StopAllStateTimers();
    
    // Логируем событие
    LogStateMessage(Owner, TEXT("Enemy died"), ELogVerbosity::Log);
}

void UMedComDeathState::OnExit(AMedComEnemyCharacter* Owner)
{
    // Вряд ли будет вызвано, но для корректности реализуем
    Super::OnExit(Owner);
    
    LogStateMessage(Owner, TEXT("Exiting Death state (unusual)"));
}

void UMedComDeathState::ProcessTick(AMedComEnemyCharacter* Owner, float DeltaTime)
{
    // В этом состоянии обычно не нужна постоянная обработка
    Super::ProcessTick(Owner, DeltaTime);
}

void UMedComDeathState::OnTimerFired(AMedComEnemyCharacter* Owner, FName TimerName)
{
    if (!Owner)
    {
        return;
    }
    
    if (TimerName == RagdollTimerName)
    {
        // Включаем рагдолл
        EnableRagdoll(Owner);
    }
    else if (TimerName == DespawnTimerName)
    {
        // Обработка исчезновения трупа
        HandleDespawn(Owner);
    }
}

void UMedComDeathState::EnableRagdoll(AMedComEnemyCharacter* Owner)
{
    if (!Owner)
    {
        return;
    }
    
    // Включаем симуляцию физики для меша
    if (Owner->GetMesh())
    {
        Owner->GetMesh()->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
        Owner->GetMesh()->SetAllBodiesBelowSimulatePhysics(TEXT("pelvis"), true, true);
        Owner->GetMesh()->SetCollisionResponseToAllChannels(ECR_Block);
    }
    
    LogStateMessage(Owner, TEXT("Ragdoll enabled"));
}

void UMedComDeathState::HandleDespawn(AMedComEnemyCharacter* Owner)
{
    if (!Owner)
    {
        return;
    }
    
    // Проверяем, нужно ли полное уничтожение или деактивация
    bool bDestroyOnDeath = GetStateParamBool(TEXT("DestroyOnDeath"), true);
    
    if (bDestroyOnDeath)
    {
        // Уничтожаем актора
        LogStateMessage(Owner, TEXT("Despawning - destroying actor"));
        Owner->Destroy();
    }
    else
    {
        // Деактивируем актора (для пулинга)
        LogStateMessage(Owner, TEXT("Despawning - deactivating actor"));
        Owner->SetActorHiddenInGame(true);
        Owner->SetActorEnableCollision(false);
        Owner->SetActorTickEnabled(false);
    }
}