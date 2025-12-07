// ISuspenseCoreAbilityConnector.h
// SuspenseCore compatibility wrapper
// Copyright Suspense Team. All Rights Reserved.
//
// This file provides SuspenseCore* interface aliases for compatibility with
// EquipmentSystem components that use the SuspenseCore naming convention.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/Equipment/ISuspenseAbilityConnector.h"

// Interface aliases for SuspenseCore naming convention compatibility

/** Alias for USuspenseAbilityConnector UInterface class */
using USuspenseCoreAbilityConnector = USuspenseAbilityConnector;

/** Alias for ISuspenseAbilityConnector interface */
using ISuspenseCoreAbilityConnector = ISuspenseAbilityConnector;
