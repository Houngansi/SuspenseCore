// ISuspenseCoreAbilityProvider.h
// SuspenseCore compatibility wrapper
// Copyright Suspense Team. All Rights Reserved.
//
// This file provides SuspenseCore* interface aliases for compatibility with
// EquipmentSystem components that use the SuspenseCore naming convention.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/Abilities/ISuspenseAbilityProvider.h"

// Interface aliases for SuspenseCore naming convention compatibility

/** Alias for USuspenseAbilityProvider UInterface class */
using USuspenseCoreAbilityProvider = USuspenseAbilityProvider;

/** Alias for ISuspenseAbilityProvider interface */
using ISuspenseCoreAbilityProvider = ISuspenseAbilityProvider;
