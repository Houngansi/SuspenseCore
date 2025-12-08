// ISuspenseCoreEquipment.h
// SuspenseCore compatibility wrapper
// Copyright Suspense Team. All Rights Reserved.
//
// This file provides SuspenseCore* interface aliases for compatibility with
// EquipmentSystem components that use the SuspenseCore naming convention.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/Equipment/ISuspenseEquipment.h"

// Interface aliases for SuspenseCore naming convention compatibility

/** Alias for USuspenseEquipment UInterface class */
using USuspenseCoreEquipment = USuspenseEquipment;

/** Alias for ISuspenseEquipment interface */
using ISuspenseCoreEquipment = ISuspenseEquipment;
