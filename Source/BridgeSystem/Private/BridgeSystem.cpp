#include "BridgeSystem.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCore, Log, All);

#define LOCTEXT_NAMESPACE "FBridgeSystemModule"

void FBridgeSystemModule::StartupModule()
{
    UE_LOG(LogSuspenseCore, Log, TEXT("SuspenseCore BridgeSystem: Module starting up..."));

    // Validate critical GameplayTags at startup (Editor and PIE only)
#if WITH_EDITOR
    // Defer validation until GameplayTags are fully loaded
    FCoreDelegates::OnPostEngineInit.AddLambda([]()
    {
        if (!FSuspenseCoreTagValidator::ValidateCriticalTags())
        {
            TArray<FString> MissingTags = FSuspenseCoreTagValidator::GetMissingTags();
            UE_LOG(LogSuspenseCore, Error, TEXT("SuspenseCore: %d critical GameplayTags are missing!"), MissingTags.Num());
            UE_LOG(LogSuspenseCore, Error, TEXT("Add missing tags to Config/DefaultGameplayTags.ini"));

            for (const FString& Tag : MissingTags)
            {
                UE_LOG(LogSuspenseCore, Error, TEXT("  - %s"), *Tag);
            }
        }
        else
        {
            UE_LOG(LogSuspenseCore, Log, TEXT("SuspenseCore: All critical GameplayTags validated successfully"));
        }
    });
#endif

    UE_LOG(LogSuspenseCore, Log, TEXT("SuspenseCore BridgeSystem: Module started"));
}

void FBridgeSystemModule::ShutdownModule()
{
    
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FBridgeSystemModule, BridgeSystem)