// SuspenseCoreWeaponAsyncTask_PerformTrace.h
// SuspenseCore - Async Weapon Trace Task
// Copyright Suspense Team. All Rights Reserved.
//
// Performs async weapon traces for visual effects (tracers, impacts).
// Does NOT handle damage - that's server-authoritative via BaseFireAbility.
//
// UNIT CONVERSION:
// =================
// This task automatically converts weapon range from meters (DataTable)
// to UE units (centimeters) for traces. Uses SuspenseCoreUnits constants.
//
// Usage:
//   auto Task = USuspenseCoreWeaponAsyncTask_PerformTrace::PerformWeaponTrace(this, Config);
//   Task->OnCompleted.AddDynamic(this, &UMyAbility::OnTraceCompleted);
//   Task->ReadyForActivation();
//
// @see SuspenseCoreUnits.h for conversion constants
// @see Documentation/GAS/UnitConversionSystem.md

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "SuspenseCore/Types/Equipment/SuspenseCoreWeaponTypes.h"
#include "SuspenseCore/Core/SuspenseCoreUnits.h"
#include "SuspenseCoreWeaponAsyncTask_PerformTrace.generated.h"

class USuspenseCoreWeaponAttributeSet;

/**
 * Configuration for weapon trace operation.
 */
USTRUCT(BlueprintType)
struct GAS_API FSuspenseCoreWeaponTraceConfig
{
	GENERATED_BODY()

	/** Use muzzle location to screen center trace (standard FPS) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
	bool bUseMuzzleToScreenCenter;

	/** Enable debug visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
	bool bDebug;

	/** Debug visualization duration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace", meta = (EditCondition = "bDebug"))
	float DebugDrawTime;

	/** Collision profile for traces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
	FName TraceProfile;

	/** Override number of traces (for shotguns). 0 = use weapon default */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
	int32 OverrideNumTraces;

	/** Override spread angle. < 0 = use weapon attributes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
	float OverrideSpreadAngle;

	/** Override max range. < 0 = use weapon attributes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
	float OverrideMaxRange;

	FSuspenseCoreWeaponTraceConfig()
		: bUseMuzzleToScreenCenter(true)
		, bDebug(false)
		, DebugDrawTime(2.0f)
		, TraceProfile(FName("Weapon"))
		, OverrideNumTraces(0)
		, OverrideSpreadAngle(-1.0f)
		, OverrideMaxRange(-1.0f)
	{
	}
};

/**
 * Result of weapon trace operation.
 */
USTRUCT(BlueprintType)
struct GAS_API FSuspenseCoreWeaponTraceResult
{
	GENERATED_BODY()

	/** All hit results from trace(s) */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TArray<FHitResult> HitResults;

	/** Number of traces performed */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 NumTraces;

	/** Whether any blocking hit occurred */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bHadBlockingHit;

	/** Applied spread angle (for reference) */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	float AppliedSpreadAngle;

	/** Muzzle location used */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FVector MuzzleLocation;

	/** Primary aim direction */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FVector AimDirection;

	FSuspenseCoreWeaponTraceResult()
		: NumTraces(0)
		, bHadBlockingHit(false)
		, AppliedSpreadAngle(0.0f)
		, MuzzleLocation(FVector::ZeroVector)
		, AimDirection(FVector::ForwardVector)
	{
	}
};

/**
 * Delegate for trace completion.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FSuspenseCoreTraceCompletedDelegate,
	const FSuspenseCoreWeaponTraceResult&, TraceResult
);

/**
 * USuspenseCoreWeaponAsyncTask_PerformTrace
 *
 * Async ability task that performs weapon traces for visual effects.
 * Used for client-side tracer rendering and impact effects.
 *
 * Features:
 * - Support for single and multi-trace (shotgun) weapons
 * - Spread modifiers based on player state
 * - Configurable via weapon attributes or overrides
 * - Debug visualization support
 *
 * Note: This task is for VISUALS ONLY. Damage is handled server-side
 * in the fire ability using authoritative traces.
 *
 * @see USuspenseCoreBaseFireAbility
 * @see USuspenseCoreTraceUtils
 */
UCLASS()
class GAS_API USuspenseCoreWeaponAsyncTask_PerformTrace : public UAbilityTask
{
	GENERATED_BODY()

public:
	USuspenseCoreWeaponAsyncTask_PerformTrace();

	//========================================================================
	// Task Creation
	//========================================================================

	/**
	 * Create trace task with configuration.
	 *
	 * @param OwningAbility Ability that owns this task
	 * @param Config Trace configuration
	 * @return Created task instance
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Tasks",
		meta = (DisplayName = "Perform Weapon Trace",
			HidePin = "OwningAbility",
			DefaultToSelf = "OwningAbility",
			BlueprintInternalUseOnly = "TRUE"))
	static USuspenseCoreWeaponAsyncTask_PerformTrace* PerformWeaponTrace(
		UGameplayAbility* OwningAbility,
		const FSuspenseCoreWeaponTraceConfig& Config
	);

	/**
	 * Create trace task from shot request (deterministic).
	 * Uses the request's random seed for consistent spread across client/server.
	 *
	 * @param OwningAbility Ability that owns this task
	 * @param ShotRequest Shot request with origin, direction, spread
	 * @param bDebug Enable debug visualization
	 * @return Created task instance
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Tasks",
		meta = (DisplayName = "Perform Weapon Trace From Request",
			HidePin = "OwningAbility",
			DefaultToSelf = "OwningAbility",
			BlueprintInternalUseOnly = "TRUE"))
	static USuspenseCoreWeaponAsyncTask_PerformTrace* PerformWeaponTraceFromRequest(
		UGameplayAbility* OwningAbility,
		const FWeaponShotParams& ShotRequest,
		bool bDebug = false
	);

	//========================================================================
	// Delegates
	//========================================================================

	/** Called when trace completes */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Tasks")
	FSuspenseCoreTraceCompletedDelegate OnCompleted;

	//========================================================================
	// Task Interface
	//========================================================================

	virtual void Activate() override;
	virtual FString GetDebugString() const override;

protected:
	/** Execute trace based on configuration */
	void ExecuteTrace();

	/** Execute trace from shot request */
	void ExecuteTraceFromRequest();

	/** Get weapon attribute set from current weapon */
	const USuspenseCoreWeaponAttributeSet* GetWeaponAttributes() const;

	/** Get muzzle location from weapon actor */
	FVector GetMuzzleLocation() const;

	/** Calculate spread modifiers based on player state */
	float CalculateSpreadModifier() const;

private:
	/** Trace configuration */
	UPROPERTY()
	FSuspenseCoreWeaponTraceConfig Config;

	/** Shot request for deterministic tracing */
	UPROPERTY()
	FWeaponShotParams ShotRequest;

	/** Whether using request-based tracing */
	bool bUseRequestMode;

	//========================================================================
	// Spread Modifier Constants
	//========================================================================

	/** Spread modifier when aiming */
	static constexpr float AimingModifier = 0.3f;

	/** Spread modifier when crouching */
	static constexpr float CrouchingModifier = 0.7f;

	/** Spread modifier when sprinting */
	static constexpr float SprintingModifier = 3.0f;

	/** Spread modifier when jumping */
	static constexpr float JumpingModifier = 4.0f;

	/** Spread modifier for auto fire */
	static constexpr float AutoFireModifier = 2.0f;

	/** Spread modifier for burst fire */
	static constexpr float BurstFireModifier = 1.5f;

	/** Spread modifier for movement (if speed > threshold) */
	static constexpr float MovementModifier = 1.5f;

	/** Movement speed threshold for spread penalty */
	static constexpr float MovementThreshold = 10.0f;

	/** Default max range if not specified (in UE units)
	 *  Uses SuspenseCoreUnits constant for consistency across codebase.
	 *  @see SuspenseCoreUnits::DefaultTraceRangeUnits */
	static constexpr float DefaultMaxRange = SuspenseCoreUnits::DefaultTraceRangeUnits;
};
