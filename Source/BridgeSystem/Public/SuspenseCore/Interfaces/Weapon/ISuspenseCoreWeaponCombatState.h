// ISuspenseCoreWeaponCombatState.h
// SuspenseCore - Weapon Combat State Interface
// Copyright Suspense Team. All Rights Reserved.
//
// Interface for weapon combat state management (aiming, firing, reloading).
// Implemented by WeaponStanceComponent, used by GAS abilities.
// This interface lives in BridgeSystem to avoid circular dependencies.
//
// Usage:
//   if (ISuspenseCoreWeaponCombatState* CombatState = GetCombatStateInterface())
//   {
//       CombatState->SetAiming(true);
//   }

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ISuspenseCoreWeaponCombatState.generated.h"

UINTERFACE(MinimalAPI, Blueprintable, meta = (CannotImplementInterfaceInBlueprint))
class USuspenseCoreWeaponCombatState : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreWeaponCombatState
 *
 * Interface for weapon combat state queries and commands.
 * Provides abstraction layer between GAS abilities and WeaponStanceComponent.
 *
 * This interface breaks the circular dependency:
 * - GAS module depends on BridgeSystem (uses interface)
 * - EquipmentSystem depends on BridgeSystem (implements interface)
 * - No direct GAS <-> EquipmentSystem dependency
 *
 * Implemented by:
 * - USuspenseCoreWeaponStanceComponent
 *
 * Used by:
 * - USuspenseCoreAimDownSightAbility
 * - USuspenseCoreFireAbility
 * - USuspenseCoreReloadAbility
 *
 * @see USuspenseCoreWeaponStanceComponent
 */
class BRIDGESYSTEM_API ISuspenseCoreWeaponCombatState
{
	GENERATED_BODY()

public:
	//========================================================================
	// State Queries (const - safe for CanActivateAbility checks)
	//========================================================================

	/** Check if weapon is currently drawn (not holstered) */
	virtual bool IsWeaponDrawn() const = 0;

	/** Check if currently aiming down sights */
	virtual bool IsAiming() const = 0;

	/** Check if currently firing */
	virtual bool IsFiring() const = 0;

	/** Check if currently reloading */
	virtual bool IsReloading() const = 0;

	/** Check if currently holding breath (sniper stability) */
	virtual bool IsHoldingBreath() const = 0;

	/** Check if montage is currently playing */
	virtual bool IsMontageActive() const = 0;

	/** Get current aim pose alpha (0 = hip, 1 = full ADS) */
	virtual float GetAimPoseAlpha() const = 0;

	//========================================================================
	// State Commands (modify state - call from abilities)
	//========================================================================

	/**
	 * Set aiming state.
	 * This will:
	 * - Update bIsAiming (replicated)
	 * - Set TargetAimPoseAlpha for interpolation
	 * - Publish EventBus: AimStarted/AimEnded
	 * - Force network update
	 *
	 * @param bNewAiming True to start aiming, false to stop
	 */
	virtual void SetAiming(bool bNewAiming) = 0;

	/**
	 * Set firing state.
	 * @param bNewFiring True if firing, false otherwise
	 */
	virtual void SetFiring(bool bNewFiring) = 0;

	/**
	 * Set reloading state.
	 * Note: Setting reloading to true will automatically cancel aiming.
	 * @param bNewReloading True if reloading, false otherwise
	 */
	virtual void SetReloading(bool bNewReloading) = 0;

	/**
	 * Set breath holding state (for sniper stability).
	 * @param bNewHoldingBreath True if holding breath, false otherwise
	 */
	virtual void SetHoldingBreath(bool bNewHoldingBreath) = 0;

	/**
	 * Set montage active state.
	 * Called by AnimInstance when weapon montage plays/ends.
	 * @param bNewMontageActive True if montage playing, false otherwise
	 */
	virtual void SetMontageActive(bool bNewMontageActive) = 0;
};
