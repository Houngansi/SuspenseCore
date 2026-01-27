#include "SuspenseCore/Characters/SuspenseCoreEnemyCharacter.h"
#include "SuspenseCore/Core/SuspenseCoreEnemyState.h"
#include "SuspenseCore/FSM/SuspenseCoreEnemyFSMComponent.h"
#include "SuspenseCore/Data/SuspenseCoreEnemyBehaviorData.h"
#include "SuspenseCore/Attributes/SuspenseCoreEnemyAttributeSet.h"
#include "SuspenseCore/Tags/SuspenseCoreEnemyTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "EnemySystem.h"

ASuspenseCoreEnemyCharacter::ASuspenseCoreEnemyCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    FSMComponent = CreateDefaultSubobject<USuspenseCoreEnemyFSMComponent>(TEXT("FSMComponent"));

    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    EnemyTypeTag = SuspenseCoreEnemyTags::Type::Scav;
}

void ASuspenseCoreEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (DefaultBehaviorData)
    {
        InitializeEnemy(DefaultBehaviorData);
    }
}

void ASuspenseCoreEnemyCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    SetupAbilitySystem();
}

void ASuspenseCoreEnemyCharacter::UnPossessed()
{
    Super::UnPossessed();
}

UAbilitySystemComponent* ASuspenseCoreEnemyCharacter::GetAbilitySystemComponent() const
{
    if (EnemyState)
    {
        return EnemyState->GetAbilitySystemComponent();
    }
    return nullptr;
}

void ASuspenseCoreEnemyCharacter::InitializeEnemy(USuspenseCoreEnemyBehaviorData* BehaviorData)
{
    if (!BehaviorData)
    {
        UE_LOG(LogEnemySystem, Error, TEXT("[%s] Cannot initialize: BehaviorData is null"), *GetName());
        return;
    }

    if (FSMComponent)
    {
        FSMComponent->Initialize(BehaviorData);
    }

    if (EnemyState && BehaviorData->StartupAbilities.Num() > 0)
    {
        EnemyState->InitializeAbilities(BehaviorData->StartupAbilities);
    }

    if (EnemyState && BehaviorData->StartupEffects.Num() > 0)
    {
        EnemyState->ApplyStartupEffects(BehaviorData->StartupEffects);
    }

    UE_LOG(LogEnemySystem, Log, TEXT("[%s] Enemy initialized with behavior: %s"),
        *GetName(), *BehaviorData->GetName());
}

void ASuspenseCoreEnemyCharacter::SetupAbilitySystem()
{
    AController* MyController = GetController();
    if (!MyController)
    {
        return;
    }

    EnemyState = MyController->GetPlayerState<ASuspenseCoreEnemyState>();
    if (!EnemyState)
    {
        UE_LOG(LogEnemySystem, Warning, TEXT("[%s] EnemyState not found on controller"), *GetName());
        return;
    }

    UAbilitySystemComponent* AbilitySystemComponent = EnemyState->GetAbilitySystemComponent();
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(EnemyState, this);
    }
}

bool ASuspenseCoreEnemyCharacter::IsAlive() const
{
    if (!EnemyState)
    {
        return true;
    }

    USuspenseCoreEnemyAttributeSet* AttributeSet = EnemyState->GetAttributeSet();
    if (AttributeSet)
    {
        return AttributeSet->IsAlive();
    }

    return true;
}

FGameplayTag ASuspenseCoreEnemyCharacter::GetCurrentStateTag() const
{
    if (FSMComponent)
    {
        return FSMComponent->GetCurrentStateTag();
    }
    return FGameplayTag();
}

void ASuspenseCoreEnemyCharacter::StopMovement()
{
    if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
    {
        MovementComp->StopMovementImmediately();
    }

    AAIController* AIController = Cast<AAIController>(GetController());
    if (AIController)
    {
        AIController->StopMovement();
    }
}

void ASuspenseCoreEnemyCharacter::ExecuteAttack()
{
    UE_LOG(LogEnemySystem, Verbose, TEXT("[%s] Executing attack on target: %s"),
        *GetName(),
        CurrentTarget.IsValid() ? *CurrentTarget->GetName() : TEXT("None"));
}

AActor* ASuspenseCoreEnemyCharacter::GetCurrentTarget() const
{
    return CurrentTarget.Get();
}

void ASuspenseCoreEnemyCharacter::SetCurrentTarget(AActor* NewTarget)
{
    CurrentTarget = NewTarget;
}

void ASuspenseCoreEnemyCharacter::OnPerceptionUpdated(AActor* Actor, bool bIsSensed)
{
    if (!FSMComponent)
    {
        return;
    }

    if (bIsSensed)
    {
        // CRITICAL: Set the detected actor as target BEFORE sending event
        // Otherwise Chase state will get null target and fail immediately
        SetCurrentTarget(Actor);
        FSMComponent->SendFSMEvent(SuspenseCoreEnemyTags::Event::PlayerDetected, Actor);

        UE_LOG(LogEnemySystem, Log, TEXT("[%s] Target acquired: %s"),
            *GetName(), *Actor->GetName());
    }
    else
    {
        if (CurrentTarget.Get() == Actor)
        {
            FSMComponent->SendFSMEvent(SuspenseCoreEnemyTags::Event::PlayerLost, Actor);
            // Don't clear target immediately - let Chase state handle memory/timeout
        }
    }
}
