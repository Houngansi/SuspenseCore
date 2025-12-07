// ISuspenseCoreWeaponAnimation.h
// SuspenseCore compatibility wrapper
// Copyright Suspense Team. All Rights Reserved.
//
// This file provides SuspenseCore* interface aliases for compatibility with
// EquipmentSystem components that use the SuspenseCore naming convention.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/Weapon/ISuspenseWeaponAnimation.h"

// Interface aliases for SuspenseCore naming convention compatibility

/** Alias for USuspenseWeaponAnimation UInterface class */
using USuspenseCoreWeaponAnimation = USuspenseWeaponAnimation;

/** Alias for ISuspenseWeaponAnimation interface */
using ISuspenseCoreWeaponAnimation = ISuspenseWeaponAnimation;
