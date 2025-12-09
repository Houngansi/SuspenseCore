#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FUISystemModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
