// SuspenseCoreInteractionSettings.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreInteractionSettings.generated.h"

// Forward declarations
class USuspenseCoreEventBus;

/**
 * USuspenseCoreInteractionSettings
 *
 * Developer Settings for Interaction System with EventBus integration.
 * Supports hot-reload of settings in Editor via PostEditChangeProperty.
 *
 * EVENTBUS INTEGRATION:
 * - Broadcasts SuspenseCore.Event.Settings.InteractionChanged on property changes
 * - Uses FSuspenseCoreEventData for typed payload delivery
 * - Allows UI and gameplay systems to react to settings changes
 *
 * @see USuspenseInteractionSettings (Legacy reference)
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Suspense Core Interaction Settings"))
class INTERACTIONSYSTEM_API USuspenseCoreInteractionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USuspenseCoreInteractionSettings();

	//==================================================================
	// UDeveloperSettings Interface
	//==================================================================

	/** Get the category name for settings */
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Get the section name in the category */
	virtual FName GetSectionName() const override { return TEXT("Suspense Core Interaction"); }

#if WITH_EDITOR
	/**
	 * Called when a property is changed in the Editor.
	 * Broadcasts settings change event through EventBus.
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//==================================================================
	// Trace Settings
	//==================================================================

	/**
	 * Default trace distance for interaction detection.
	 * Used by SuspenseCoreInteractionComponent when no override is set.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Trace",
		meta = (ClampMin = "10.0", ClampMax = "5000.0", DisplayName = "Default Trace Distance"))
	float DefaultTraceDistance;

	/**
	 * Collision channel used for interaction traces.
	 * Should match the channel used by interactable objects.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Trace",
		meta = (DisplayName = "Trace Channel"))
	TEnumAsByte<ECollisionChannel> DefaultTraceChannel;

	/**
	 * Trace sphere radius for spherecast (0 = line trace).
	 * Larger values make interaction easier on small objects.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Trace",
		meta = (ClampMin = "0.0", ClampMax = "100.0", DisplayName = "Trace Sphere Radius"))
	float TraceSphereRadius;

	//==================================================================
	// Cooldown Settings
	//==================================================================

	/**
	 * Default cooldown between interaction attempts.
	 * Prevents spam clicking on interactables.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cooldown",
		meta = (ClampMin = "0.0", ClampMax = "5.0", DisplayName = "Interaction Cooldown"))
	float DefaultInteractionCooldown;

	//==================================================================
	// Focus Settings
	//==================================================================

	/**
	 * Rate limit for focus update traces (per second).
	 * Lower values reduce CPU usage but may feel less responsive.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Focus",
		meta = (ClampMin = "1.0", ClampMax = "60.0", DisplayName = "Focus Update Rate"))
	float FocusUpdateRate;

	/**
	 * Whether to broadcast focus change events through EventBus.
	 * Disable if you handle focus purely through local delegates.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Focus",
		meta = (DisplayName = "Broadcast Focus Events"))
	bool bBroadcastFocusEvents;

	//==================================================================
	// Debug Settings
	//==================================================================

	/**
	 * Enable debug visualization for traces.
	 * Only works in non-shipping builds.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug",
		meta = (DisplayName = "Enable Debug Draw"))
	bool bEnableDebugDraw;

	/**
	 * Duration to display debug lines (seconds).
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug",
		meta = (ClampMin = "0.0", ClampMax = "5.0", EditCondition = "bEnableDebugDraw",
			DisplayName = "Debug Line Duration"))
	float DebugLineDuration;

	/**
	 * Enable verbose logging for interaction system.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug",
		meta = (DisplayName = "Enable Verbose Logging"))
	bool bEnableVerboseLogging;

	//==================================================================
	// Static Accessors
	//==================================================================

	/**
	 * Get the settings instance.
	 * @return Settings instance (never null)
	 */
	static const USuspenseCoreInteractionSettings* Get();

	/**
	 * Get mutable settings instance (for runtime modification).
	 * @return Mutable settings instance
	 */
	static USuspenseCoreInteractionSettings* GetMutable();

protected:
	//==================================================================
	// EventBus Integration
	//==================================================================

	/**
	 * Broadcast settings changed event through EventBus.
	 * @param ChangedPropertyName Name of the property that changed
	 */
	void BroadcastSettingsChanged(FName ChangedPropertyName);

	/**
	 * Get EventBus for broadcasting.
	 * @return EventBus instance or nullptr
	 */
	USuspenseCoreEventBus* GetEventBus() const;

private:
	/** Cached EventBus reference */
	mutable TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;
};
