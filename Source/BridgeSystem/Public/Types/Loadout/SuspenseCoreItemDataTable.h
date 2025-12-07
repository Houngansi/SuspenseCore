// SuspenseCoreItemDataTable.h
// SuspenseCore compatibility wrapper
// Copyright Suspense Team. All Rights Reserved.
//
// This file provides SuspenseCore* type aliases for compatibility with
// EquipmentSystem components that use the SuspenseCore naming convention.

#pragma once

#include "CoreMinimal.h"
#include "Types/Loadout/SuspenseItemDataTable.h"

// Type aliases for SuspenseCore naming convention compatibility

/** Alias for FSuspenseUnifiedItemData - the main DataTable row type */
using FSuspenseCoreUnifiedItemData = FSuspenseUnifiedItemData;

/** Alias for FMCPickupData */
using FSuspenseCorePickupData = FMCPickupData;

/** Alias for FMCEquipmentData */
using FSuspenseCoreEquipmentData = FMCEquipmentData;

/** Alias for FWeaponInitializationData */
using FSuspenseCoreWeaponInitializationData = FWeaponInitializationData;

/** Alias for FAmmoInitializationData */
using FSuspenseCoreAmmoInitializationData = FAmmoInitializationData;
