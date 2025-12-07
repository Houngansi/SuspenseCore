// SuspenseCoreEquipmentServiceLocator.h
// SuspenseCore compatibility wrapper
// Copyright Suspense Team. All Rights Reserved.
//
// This file provides SuspenseCore* class aliases for compatibility with
// EquipmentSystem components that use the SuspenseCore naming convention.

#pragma once

#include "CoreMinimal.h"
#include "Core/Services/SuspenseEquipmentServiceLocator.h"

// Class aliases for SuspenseCore naming convention compatibility

/** Alias for USuspenseEquipmentServiceLocator class */
using USuspenseEquipmentServiceLocator = USuspenseEquipmentServiceLocator;

/** Alias for FServiceRegistration struct */
using FSuspenseCoreServiceRegistration = FServiceRegistration;
