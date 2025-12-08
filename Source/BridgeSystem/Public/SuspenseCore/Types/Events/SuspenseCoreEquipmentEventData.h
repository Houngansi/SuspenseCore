// Types/Events/SuspenseEquipmentEventData.h
// Copyright MedCom.

#pragma once


#include "CoreMinimal.h"
#include "SuspenseCore/Core/Utils/SuspenseCoreEquipmentEventBus.h"

/*
 * DEPRECATION NOTICE:
 * This header is now a thin shim to the canonical event definition.
 * Use FSuspenseEquipmentEventData from Core/Utils/FSuspenseEquipmentEventBus.h.
 * We intentionally removed any cross-module includes (e.g., inventory),
 * USTRUCT and *.generated.h to avoid ODR and module coupling.
 */


