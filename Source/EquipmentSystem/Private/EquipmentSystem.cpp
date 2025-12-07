#include "EquipmentSystem.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceMacros.h"

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

// Legacy short aliases for backward compatibility
DEFINE_LOG_CATEGORY(LogEquipmentPrediction);
DEFINE_LOG_CATEGORY(LogEquipmentNetwork);
DEFINE_LOG_CATEGORY(LogEquipmentValidation);
DEFINE_LOG_CATEGORY(LogEquipmentOperation);
DEFINE_LOG_CATEGORY(LogEquipmentReplication);


#define LOCTEXT_NAMESPACE "FEquipmentSystemModule"

void FEquipmentSystemModule::StartupModule()
{

}

void FEquipmentSystemModule::ShutdownModule()
{

}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FEquipmentSystemModule, EquipmentSystem)
