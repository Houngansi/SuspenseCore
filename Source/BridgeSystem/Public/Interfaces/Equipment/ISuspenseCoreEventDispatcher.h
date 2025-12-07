// ISuspenseCoreEventDispatcher.h
// SuspenseCore compatibility wrapper
// Copyright Suspense Team. All Rights Reserved.
//
// This file provides SuspenseCore* interface aliases for compatibility with
// EquipmentSystem components that use the SuspenseCore naming convention.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/Equipment/ISuspenseEventDispatcher.h"

// Interface aliases for SuspenseCore naming convention compatibility

/** Alias for USuspenseEventDispatcher UInterface class */
using USuspenseCoreEventDispatcher = USuspenseEventDispatcher;

/** Alias for ISuspenseEventDispatcher interface */
using ISuspenseCoreEventDispatcher = ISuspenseEventDispatcher;

/** Alias for FDispatcherEquipmentEventData struct */
using FSuspenseCoreDispatcherEquipmentEventData = FDispatcherEquipmentEventData;
