#include "EnemySystem.h"

DEFINE_LOG_CATEGORY(LogEnemySystem);

#define LOCTEXT_NAMESPACE "FEnemySystemModule"

void FEnemySystemModule::StartupModule()
{
    UE_LOG(LogEnemySystem, Log, TEXT("EnemySystem module initialized"));
}

void FEnemySystemModule::ShutdownModule()
{
    UE_LOG(LogEnemySystem, Log, TEXT("EnemySystem module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FEnemySystemModule, EnemySystem)
