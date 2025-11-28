#include "EquipmentSystem.h"
#include "Services/SuspenseEquipmentServiceMacros.h"

// Define log categories declared in SuspenseEquipmentServiceMacros.h
DEFINE_LOG_CATEGORY(LogSuspenseEquipmentData);
DEFINE_LOG_CATEGORY(LogSuspenseEquipmentNetwork);
DEFINE_LOG_CATEGORY(LogSuspenseEquipmentOperation);
DEFINE_LOG_CATEGORY(LogSuspenseEquipmentVisualization);
DEFINE_LOG_CATEGORY(LogSuspenseEquipmentValidation);
DEFINE_LOG_CATEGORY(LogSuspenseEquipmentAbility);
DEFINE_LOG_CATEGORY(LogSuspenseEquipmentPrediction);
DEFINE_LOG_CATEGORY(LogSuspenseEquipmentReplication);
DEFINE_LOG_CATEGORY(LogSuspenseEquipmentDelta);

// Short aliases for legacy code
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