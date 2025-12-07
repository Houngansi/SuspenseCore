// ISuspenseCoreWeapon.h
// SuspenseCore compatibility wrapper
// Copyright Suspense Team. All Rights Reserved.
//
// This file provides SuspenseCore* interface aliases for compatibility with
// EquipmentSystem components that use the SuspenseCore naming convention.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/Weapon/ISuspenseWeapon.h"

// Interface aliases for SuspenseCore naming convention compatibility

/** Alias for USuspenseWeapon UInterface class */
using USuspenseCoreWeapon = USuspenseWeapon;

/** Alias for ISuspenseWeapon interface */
using ISuspenseCoreWeapon = ISuspenseWeapon;

/** Alias for FWeaponInitializationResult struct */
using FSuspenseCoreWeaponInitializationResult = FWeaponInitializationResult;

/** Alias for FWeaponFireParams struct */
using FSuspenseCoreWeaponFireParams = FWeaponFireParams;

/** Alias for FWeaponStateFlags struct */
using FSuspenseCoreWeaponStateFlags = FWeaponStateFlags;
