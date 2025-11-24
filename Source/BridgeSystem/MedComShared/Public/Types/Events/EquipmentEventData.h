// Types/Events/EquipmentEventData.h
// Copyright MedCom.

#pragma once

#include "CoreMinimal.h"
#include "Core/Utils/FEquipmentEventBus.h"

/*
 * DEPRECATION NOTICE:
 * This header is now a thin shim to the canonical event definition.
 * Use FEquipmentEventData from Core/Utils/FEquipmentEventBus.h.
 * We intentionally removed any cross-module includes (e.g., inventory),
 * USTRUCT and *.generated.h to avoid ODR and module coupling.
 */
