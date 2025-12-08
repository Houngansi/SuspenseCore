// ISuspenseCoreLoadout.h
// SuspenseCore compatibility wrapper
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/Core/ISuspenseLoadout.h"

// Interface aliases for SuspenseCore naming convention compatibility

/** Alias for USuspenseLoadout UInterface class */
using USuspenseCoreLoadout = USuspenseLoadout;

/** Alias for ISuspenseLoadout interface */
using ISuspenseCoreLoadout = ISuspenseLoadout;

/** Alias for FLoadoutApplicationResult */
using FSuspenseCoreLoadoutResult = FLoadoutApplicationResult;
