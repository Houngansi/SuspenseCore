// ISuspenseCoreInventoryBridge.h
// SuspenseCore compatibility wrapper
// Copyright Suspense Team. All Rights Reserved.
//
// This file provides SuspenseCore* interface aliases for compatibility with
// EquipmentSystem components that use the SuspenseCore naming convention.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/Equipment/ISuspenseInventoryBridge.h"

// Interface aliases for SuspenseCore naming convention compatibility

/** Alias for USuspenseInventoryBridge UInterface class */
using USuspenseCoreInventoryBridge = USuspenseInventoryBridge;

/** Alias for ISuspenseInventoryBridge interface */
using ISuspenseCoreInventoryBridge = ISuspenseInventoryBridge;

/** Alias for FInventoryTransferRequest struct */
using FSuspenseCoreInventoryTransferRequest = FInventoryTransferRequest;
