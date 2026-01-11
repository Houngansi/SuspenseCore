// SuspenseCoreSpreadProcessor.h
// SuspenseCore - Weapon Spread Calculator
// Copyright Suspense Team. All Rights Reserved.
//
// Calculates weapon spread based on player state (aiming, moving, etc.)
// Uses modifiers from WeaponAttributeSet and player movement state.
//
// Usage:
//   float CurrentSpread = USuspenseCoreSpreadProcessor::CalculateCurrentSpread(
//       BaseSpread, bIsAiming, MovementSpeed, RecoilModifier);

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SuspenseCoreSpreadProcessor.generated.h"

// NOTE: Attribute-based spread calculation has moved to GAS module
// See USuspenseCoreSpreadCalculator in SuspenseCore/Utils/SuspenseCoreSpreadCalculator.h

/**
 * Spread modifier configuration for different player states.
 * All modifiers are multiplicative (1.0 = no change).
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreSpreadModifiers
{
	GENERATED_BODY()

	/** Spread multiplier when aiming down sights (typically < 1.0 for reduction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float AimingModifier;

	/** Spread multiplier when crouching (typically < 1.0 for reduction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float CrouchingModifier;

	/** Spread multiplier when sprinting (typically > 1.0 for increase) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ClampMin = "1.0", ClampMax = "5.0"))
	float SprintingModifier;

	/** Spread multiplier when in air/jumping (typically > 1.0 for increase) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ClampMin = "1.0", ClampMax = "6.0"))
	float JumpingModifier;

	/** Spread multiplier when moving (applied per unit of speed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ClampMin = "0.0", ClampMax = "0.1"))
	float MovementSpeedFactor;

	/** Spread multiplier when firing in auto mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float AutoFireModifier;

	/** Spread multiplier when firing in burst mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ClampMin = "1.0", ClampMax = "3.0"))
	float BurstFireModifier;

	/** Default constructor with Tarkov-like values */
	FSuspenseCoreSpreadModifiers()
		: AimingModifier(0.3f)       // -70% spread when ADS
		, CrouchingModifier(0.7f)    // -30% spread when crouching
		, SprintingModifier(3.0f)    // +200% spread when sprinting
		, JumpingModifier(4.0f)      // +300% spread when jumping
		, MovementSpeedFactor(0.01f) // +1% per unit of speed
		, AutoFireModifier(2.0f)     // +100% spread for auto fire
		, BurstFireModifier(1.5f)    // +50% spread for burst fire
	{
	}
};

/**
 * Input parameters for spread calculation.
 * Encapsulates all state needed to calculate current spread.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreSpreadInput
{
	GENERATED_BODY()

	/** Base spread from weapon attributes (in degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	float BaseSpread;

	/** Is player aiming down sights */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bIsAiming;

	/** Is player crouching */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bIsCrouching;

	/** Is player sprinting */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bIsSprinting;

	/** Is player in air (jumping/falling) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bIsInAir;

	/** Is weapon in auto fire mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bIsAutoFire;

	/** Is weapon in burst fire mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bIsBurstFire;

	/** Current movement speed (units per second) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	float MovementSpeed;

	/** Current recoil multiplier from continuous fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	float RecoilModifier;

	/** Movement threshold - below this is considered stationary */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	float StationaryThreshold;

	FSuspenseCoreSpreadInput()
		: BaseSpread(1.0f)
		, bIsAiming(false)
		, bIsCrouching(false)
		, bIsSprinting(false)
		, bIsInAir(false)
		, bIsAutoFire(false)
		, bIsBurstFire(false)
		, MovementSpeed(0.0f)
		, RecoilModifier(1.0f)
		, StationaryThreshold(10.0f)
	{
	}
};

/**
 * USuspenseCoreSpreadProcessor
 *
 * Calculates weapon spread based on player state and weapon attributes.
 * All calculations are stateless and can be called from any thread.
 *
 * Spread Formula:
 *   CurrentSpread = BaseSpread * AimMod * CrouchMod * MovementMod * FireModeMod * RecoilMod
 *
 * Where:
 *   - AimMod: 0.3 when ADS, 1.0 otherwise
 *   - CrouchMod: 0.7 when crouching, 1.0 otherwise
 *   - MovementMod: 1.0 + (Speed * 0.01) when moving, up to max
 *   - FireModeMod: 2.0 for auto, 1.5 for burst, 1.0 for semi
 *   - RecoilMod: Increases with continuous fire
 */
UCLASS(Blueprintable)
class BRIDGESYSTEM_API USuspenseCoreSpreadProcessor : public UObject
{
	GENERATED_BODY()

public:
	//========================================================================
	// Main Calculation Functions
	//========================================================================

	/**
	 * Calculate current weapon spread with full input parameters.
	 *
	 * @param Input All input parameters for spread calculation
	 * @param Modifiers Spread modifier configuration (optional, uses defaults if null)
	 * @return Final spread value in degrees
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Spread")
	static float CalculateSpread(
		const FSuspenseCoreSpreadInput& Input,
		const FSuspenseCoreSpreadModifiers& Modifiers
	);

	/**
	 * Simple spread calculation with common parameters.
	 * Uses default modifiers.
	 *
	 * @param BaseSpread Base spread from weapon (degrees)
	 * @param bIsAiming Is player aiming down sights
	 * @param MovementSpeed Current movement speed
	 * @param RecoilModifier Current recoil multiplier (default 1.0)
	 * @return Final spread value in degrees
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Spread")
	static float CalculateCurrentSpread(
		float BaseSpread,
		bool bIsAiming,
		float MovementSpeed,
		float RecoilModifier = 1.0f
	);

	// NOTE: CalculateSpreadFromAttributes has been moved to GAS module
	// See USuspenseCoreSpreadCalculator::CalculateSpreadWithAttributes

	//========================================================================
	// Individual Modifier Calculations
	//========================================================================

	/**
	 * Calculate movement-based spread modifier.
	 *
	 * @param MovementSpeed Current movement speed
	 * @param StationaryThreshold Speed below which is considered stationary
	 * @param SpeedFactor Spread increase per unit of speed
	 * @return Movement spread modifier (>= 1.0)
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Spread")
	static float GetMovementModifier(
		float MovementSpeed,
		float StationaryThreshold = 10.0f,
		float SpeedFactor = 0.01f
	);

	/**
	 * Calculate fire mode spread modifier.
	 *
	 * @param bIsAutoFire Is auto fire mode active
	 * @param bIsBurstFire Is burst fire mode active
	 * @param AutoMod Auto fire modifier
	 * @param BurstMod Burst fire modifier
	 * @return Fire mode spread modifier
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Spread")
	static float GetFireModeModifier(
		bool bIsAutoFire,
		bool bIsBurstFire,
		float AutoMod = 2.0f,
		float BurstMod = 1.5f
	);

	//========================================================================
	// Default Modifiers
	//========================================================================

	/** Get default spread modifiers (Tarkov-style values) */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Spread")
	static FSuspenseCoreSpreadModifiers GetDefaultModifiers();

	//========================================================================
	// Constants
	//========================================================================

	/** Minimum spread value (prevents zero spread) */
	static constexpr float MinSpread = 0.1f;

	/** Maximum spread value (prevents absurd spread) */
	static constexpr float MaxSpread = 45.0f;

	/** Maximum movement modifier (caps movement penalty) */
	static constexpr float MaxMovementModifier = 5.0f;
};
