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

#define LOCTEXT_NAMESPACE "FSuspenseCoreEquipmentSystemModule"

void FSuspenseCoreEquipmentSystemModule::StartupModule()
{

}

void FSuspenseCoreEquipmentSystemModule::ShutdownModule()
{

}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSuspenseCoreEquipmentSystemModule, EquipmentSystem)
