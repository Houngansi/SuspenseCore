#include "EquipmentSystem.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceMacros.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "Misc/CoreDelegates.h"
#include "GameplayTagsManager.h"
#include "Modules/ModuleManager.h"

// Define log categories declared in SuspenseCoreEquipmentServiceMacros.h
DEFINE_LOG_CATEGORY(LogSuspenseCoreEquipmentData);
DEFINE_LOG_CATEGORY(LogSuspenseCoreEquipmentNetwork);
DEFINE_LOG_CATEGORY(LogSuspenseCoreEquipmentOperation);
DEFINE_LOG_CATEGORY(LogSuspenseCoreEquipmentVisualization);
DEFINE_LOG_CATEGORY(LogSuspenseCoreEquipmentValidation);
DEFINE_LOG_CATEGORY(LogSuspenseCoreEquipmentAbility);
DEFINE_LOG_CATEGORY(LogSuspenseCoreEquipmentPrediction);
DEFINE_LOG_CATEGORY(LogSuspenseCoreEquipmentReplication);
DEFINE_LOG_CATEGORY(LogSuspenseCoreEquipmentDelta);
DEFINE_LOG_CATEGORY(LogSuspenseCoreEquipmentTransaction);
DEFINE_LOG_CATEGORY(LogSuspenseCoreEquipmentRules);

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreEquipment, Log, All);

#define LOCTEXT_NAMESPACE "FSuspenseCoreEquipmentSystemModule"

namespace SuspenseCoreEquipmentModule
{
    // Critical tags that must exist for equipment system to work correctly
    static TArray<FString> GetCriticalTags()
    {
        return {
            // Service tags
            TEXT("SuspenseCore.Service.Equipment"),
            TEXT("SuspenseCore.Service.Equipment.Data"),
            TEXT("SuspenseCore.Service.Equipment.Validation"),
            TEXT("SuspenseCore.Service.Equipment.Operations"),

            // State tags
            TEXT("Equipment.State.Inactive"),
            TEXT("Equipment.State.Equipped"),
            TEXT("Equipment.State.Ready"),

            // Event tags
            TEXT("SuspenseCore.Event.Equipment.Equipped"),
            TEXT("SuspenseCore.Event.Equipment.Unequipped"),
            TEXT("Equipment.Event.PropertyChanged"),

            // Slot tags
            TEXT("Equipment.Slot.PrimaryWeapon"),
            TEXT("Equipment.Slot.SecondaryWeapon"),
            TEXT("Equipment.Slot.Holster"),

            // Weapon state tags
            TEXT("Weapon.State.Ready"),
            TEXT("Weapon.State.Holstered"),
        };
    }

    static bool ValidateCriticalTags(TArray<FString>& OutMissingTags)
    {
        OutMissingTags.Empty();

        if (!FModuleManager::Get().IsModuleLoaded(TEXT("GameplayTags")))
        {
            return false;
        }

        const TArray<FString> CriticalTags = GetCriticalTags();
        for (const FString& TagName : CriticalTags)
        {
            const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
            if (!Tag.IsValid())
            {
                OutMissingTags.Add(TagName);
            }
        }

        return OutMissingTags.Num() == 0;
    }
}

void FSuspenseCoreEquipmentSystemModule::StartupModule()
{
    UE_LOG(LogSuspenseCoreEquipment, Log, TEXT("SuspenseCore EquipmentSystem: Module starting up..."));

    // Validate critical GameplayTags at startup (Editor and PIE only)
#if WITH_EDITOR
    FCoreDelegates::OnPostEngineInit.AddLambda([]()
    {
        TArray<FString> MissingTags;
        if (!SuspenseCoreEquipmentModule::ValidateCriticalTags(MissingTags))
        {
            UE_LOG(LogSuspenseCoreEquipment, Warning,
                TEXT("SuspenseCore Equipment: %d critical GameplayTags not found (they will be registered via UE_DEFINE_GAMEPLAY_TAG)"),
                MissingTags.Num());

            for (const FString& Tag : MissingTags)
            {
                UE_LOG(LogSuspenseCoreEquipment, Verbose, TEXT("  - %s (will be created by native registration)"), *Tag);
            }
        }
        else
        {
            UE_LOG(LogSuspenseCoreEquipment, Log,
                TEXT("SuspenseCore Equipment: All critical GameplayTags validated successfully"));
        }
    });
#endif

    UE_LOG(LogSuspenseCoreEquipment, Log, TEXT("SuspenseCore EquipmentSystem: Module started"));
}

void FSuspenseCoreEquipmentSystemModule::ShutdownModule()
{
    UE_LOG(LogSuspenseCoreEquipment, Log, TEXT("SuspenseCore EquipmentSystem: Module shutting down"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSuspenseCoreEquipmentSystemModule, EquipmentSystem)
