// Types/Events/SuspenseEquipmentEventData.h
// Copyright MedCom.

#ifndef SUSPENSECORE_TYPES_EVENTS_SUSPENSECOREEQUIPMENTEVENTDATA_H
#define SUSPENSECORE_TYPES_EVENTS_SUSPENSECOREEQUIPMENTEVENTDATA_H

#include "CoreMinimal.h"
#include "SuspenseCore/Core/Utils/SuspenseCoreEquipmentEventBus.h"

/*
 * DEPRECATION NOTICE:
 * This header is now a thin shim to the canonical event definition.
 * Use FSuspenseEquipmentEventData from Core/Utils/FSuspenseEquipmentEventBus.h.
 * We intentionally removed any cross-module includes (e.g., inventory),
 * USTRUCT and *.generated.h to avoid ODR and module coupling.
 */


#endif // SUSPENSECORE_TYPES_EVENTS_SUSPENSECOREEQUIPMENTEVENTDATA_H