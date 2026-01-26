#include "SuspenseCore/Controllers/SuspenseCoreEnemyAIController.h"
#include "SuspenseCore/Core/SuspenseCoreEnemyState.h"
#include "SuspenseCore/Characters/SuspenseCoreEnemy.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "EnemySystem.h"

ASuspenseCoreEnemyAIController::ASuspenseCoreEnemyAIController()
{
    AIPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComponent"));
    SetPerceptionComponent(*AIPerceptionComponent);

    UAISenseConfig_Sight* SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
    SightConfig->SightRadius = 2000.0f;
    SightConfig->LoseSightRadius = 2500.0f;
    SightConfig->PeripheralVisionAngleDegrees = 90.0f;
    SightConfig->SetMaxAge(5.0f);
    SightConfig->AutoSuccessRangeFromLastSeenLocation = 500.0f;
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

    UAISenseConfig_Hearing* HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
    HearingConfig->HearingRange = 1500.0f;
    HearingConfig->SetMaxAge(3.0f);
    HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
    HearingConfig->DetectionByAffiliation.bDetectFriendlies = false;
    HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;

    AIPerceptionComponent->ConfigureSense(*SightConfig);
    AIPerceptionComponent->ConfigureSense(*HearingConfig);
    AIPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());

    EnemyStateClass = ASuspenseCoreEnemyState::StaticClass();

    bWantsPlayerState = true;
}

void ASuspenseCoreEnemyAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    ControlledEnemy = Cast<ASuspenseCoreEnemy>(InPawn);

    if (AIPerceptionComponent)
    {
        AIPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(
            this,
            &ASuspenseCoreEnemyAIController::OnTargetPerceptionUpdated
        );
    }

    UE_LOG(LogEnemySystem, Log, TEXT("[%s] Possessed enemy: %s"),
        *GetName(),
        InPawn ? *InPawn->GetName() : TEXT("None"));
}

void ASuspenseCoreEnemyAIController::OnUnPossess()
{
    if (AIPerceptionComponent)
    {
        AIPerceptionComponent->OnTargetPerceptionUpdated.RemoveDynamic(
            this,
            &ASuspenseCoreEnemyAIController::OnTargetPerceptionUpdated
        );
    }

    ControlledEnemy.Reset();

    Super::OnUnPossess();
}

void ASuspenseCoreEnemyAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    if (!ControlledEnemy.IsValid())
    {
        return;
    }

    bool bIsSensed = Stimulus.WasSuccessfullySensed();

    ControlledEnemy->OnPerceptionUpdated(Actor, bIsSensed);

    UE_LOG(LogEnemySystem, Verbose, TEXT("[%s] Perception updated - Actor: %s, Sensed: %s"),
        *GetName(),
        Actor ? *Actor->GetName() : TEXT("None"),
        bIsSensed ? TEXT("Yes") : TEXT("No"));
}
