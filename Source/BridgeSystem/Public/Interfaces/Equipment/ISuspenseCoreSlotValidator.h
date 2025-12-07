// ISuspenseCoreSlotValidator.h
// SuspenseCore compatibility wrapper
// Copyright Suspense Team. All Rights Reserved.
//
// This file provides SuspenseCore* interface aliases for compatibility with
// EquipmentSystem components that use the SuspenseCore naming convention.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/Equipment/ISuspenseSlotValidator.h"

// Interface aliases for SuspenseCore naming convention compatibility

/** Alias for USuspenseSlotValidator UInterface class */
using USuspenseCoreSlotValidator = USuspenseSlotValidator;

/** Alias for ISuspenseSlotValidator interface */
using ISuspenseCoreSlotValidator = ISuspenseSlotValidator;
