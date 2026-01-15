// SuspenseCoreCameraShakeComponent.h
// SuspenseCore - Camera Shake Component
// Copyright Suspense Team. All Rights Reserved.
//
// Component responsible for triggering camera shakes based on EventBus events.
// Lives on Character, subscribes to shake events via EventBus.
//
// ARCHITECTURE (AAA Pattern):
// - Game systems PUBLISH shake events via EventBus
// - Component SUBSCRIBES to events and triggers appropriate shakes
// - Decoupled: No direct calls from game systems to camera
//
// SUPPORTED EVENTS:
// - SuspenseCoreTags::Event::Camera::ShakeWeapon    - Weapon fire shake
// - SuspenseCoreTags::Event::Camera::ShakeMovement  - Jump, landing, sprint
// - SuspenseCoreTags::Event::Camera::ShakeDamage    - Damage received
// - SuspenseCoreTags::Event::Camera::ShakeExplosion - Explosions
//
// EVENT DATA:
// - "Type" (FString): Preset type (e.g., "Jump", "Landing", "Heavy")
// - "Scale" (float): Intensity multiplier (default 1.0)
// - "Distance" (float): For explosions - distance to explosion center

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/CameraShake/SuspenseCoreCameraShakeTypes.h"
#include "GameFramework/Character.h"
#include "SuspenseCoreCameraShakeComponent.generated.h"

// Forward declarations
class USuspenseCoreEventBus;
class USuspenseCoreWeaponCameraShake;
class USuspenseCoreMovementCameraShake;
class USuspenseCoreDamageCameraShake;
class USuspenseCoreExplosionCameraShake;
class USuspenseCoreCameraShakeManager;
class USuspenseCoreCameraShakeDataAsset;

/**
 * USuspenseCoreCameraShakeComponent
 *
 * Handles camera shake triggers based on EventBus events.
 * Decoupled from game logic - responds to events.
 *
 * EVENT-DRIVEN ARCHITECTURE:
 * - Subscribes to SuspenseCoreTags::Event::Camera::* events
 * - Triggers appropriate camera shake based on event type and data
 * - Uses presets from camera shake classes
 *
 * FLOW:
 * 1. Game system publishes shake event via EventBus
 * 2. Component receives event
 * 3. Component determines shake class and parameters
 * 4. Camera shake is triggered via PlayerController
 */
UCLASS(ClassGroup = (SuspenseCore), meta = (BlueprintSpawnableComponent))
class PLAYERCORE_API USuspenseCoreCameraShakeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USuspenseCoreCameraShakeComponent();

	//========================================================================
	// Configuration
	//========================================================================

	/** Master shake scale - global intensity multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|CameraShake", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MasterShakeScale = 1.0f;

	/** Weapon shake scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|CameraShake", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float WeaponShakeScale = 1.0f;

	/** Movement shake scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|CameraShake", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MovementShakeScale = 1.0f;

	/** Damage shake scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|CameraShake", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float DamageShakeScale = 1.0f;

	/** Explosion shake scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|CameraShake", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float ExplosionShakeScale = 1.0f;

	/** Enable/disable all camera shakes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|CameraShake")
	bool bEnableCameraShakes = true;

	/** Bind directly to Character's LandedDelegate for landing shake (independent of abilities) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|CameraShake")
	bool bBindToLandedDelegate = true;

	/** Use layered shake manager for priority-based shake blending (AAA pattern) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|CameraShake")
	bool bUseLayeredShakeManager = false;

	/** Enable priority-based weight reduction in layered manager */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|CameraShake", meta = (EditCondition = "bUseLayeredShakeManager"))
	bool bEnablePriorityBlending = true;

	/**
	 * Override settings from SSOT.
	 * If true, component will use its own settings instead of USuspenseCoreSettings.
	 * Useful for testing or special cases.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|CameraShake|SSOT")
	bool bOverrideSSOTSettings = false;

	//========================================================================
	// Camera Shake Classes
	//========================================================================

	/** Weapon camera shake class */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|CameraShake|Classes")
	TSubclassOf<USuspenseCoreWeaponCameraShake> WeaponShakeClass;

	/** Movement camera shake class */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|CameraShake|Classes")
	TSubclassOf<USuspenseCoreMovementCameraShake> MovementShakeClass;

	/** Damage camera shake class */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|CameraShake|Classes")
	TSubclassOf<USuspenseCoreDamageCameraShake> DamageShakeClass;

	/** Explosion camera shake class */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|CameraShake|Classes")
	TSubclassOf<USuspenseCoreExplosionCameraShake> ExplosionShakeClass;

	//========================================================================
	// Public API
	//========================================================================

	/**
	 * Manually trigger weapon shake.
	 * @param WeaponType "Rifle", "Pistol", "SMG", "Shotgun", "Sniper"
	 * @param Scale Intensity multiplier
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CameraShake")
	void PlayWeaponShake(const FString& WeaponType, float Scale = 1.0f);

	/**
	 * Manually trigger movement shake.
	 * @param MovementType "Jump", "Landing", "HardLanding", "Sprint", "Crouch"
	 * @param Scale Intensity multiplier
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CameraShake")
	void PlayMovementShake(const FString& MovementType, float Scale = 1.0f);

	/**
	 * Manually trigger damage shake.
	 * @param DamageType "Light", "Heavy", "Critical", "Headshot"
	 * @param Scale Intensity multiplier
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CameraShake")
	void PlayDamageShake(const FString& DamageType, float Scale = 1.0f);

	/**
	 * Manually trigger explosion shake.
	 * @param ExplosionType "Nearby", "Medium", "Distant", "Grenade", "Artillery", "Vehicle"
	 * @param Scale Intensity multiplier
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CameraShake")
	void PlayExplosionShake(const FString& ExplosionType, float Scale = 1.0f);

	/**
	 * Play explosion shake based on distance.
	 * @param Distance Distance from explosion in cm
	 * @param Scale Intensity multiplier
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CameraShake")
	void PlayExplosionShakeByDistance(float Distance, float Scale = 1.0f);

	/**
	 * Stop all active camera shakes.
	 * @param bImmediately If true, stop immediately without blend out
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CameraShake")
	void StopAllShakes(bool bImmediately = false);

protected:
	//========================================================================
	// UActorComponent Interface
	//========================================================================

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	//========================================================================
	// EventBus Subscription
	//========================================================================

	/** Subscribe to shake events from EventBus */
	void SubscribeToEvents();

	/** Unsubscribe from EventBus */
	void UnsubscribeFromEvents();

	/** Handle weapon shake event */
	void OnWeaponShakeEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle movement shake event */
	void OnMovementShakeEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle damage shake event */
	void OnDamageShakeEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle explosion shake event */
	void OnExplosionShakeEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Cached EventBus reference */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** Layered shake manager (created when bUseLayeredShakeManager is true) */
	UPROPERTY()
	TObjectPtr<USuspenseCoreCameraShakeManager> ShakeManager;

	/** Cached camera shake DataAsset from SSOT settings */
	UPROPERTY()
	TObjectPtr<USuspenseCoreCameraShakeDataAsset> CachedShakeDataAsset;

	/** Subscription handles for cleanup */
	FSuspenseCoreSubscriptionHandle WeaponShakeHandle;
	FSuspenseCoreSubscriptionHandle MovementShakeHandle;
	FSuspenseCoreSubscriptionHandle DamageShakeHandle;
	FSuspenseCoreSubscriptionHandle ExplosionShakeHandle;

	//========================================================================
	// Internal Methods
	//========================================================================

	/** Get EventBus from EventManager */
	USuspenseCoreEventBus* GetEventBus() const;

	/** Get owning player controller */
	APlayerController* GetOwnerPlayerController() const;

	/** Start camera shake via player controller */
	void StartCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass, float Scale);

	/** Start camera shake via layered manager with priority */
	void StartLayeredCameraShake(
		TSubclassOf<UCameraShakeBase> ShakeClass,
		float Scale,
		ESuspenseCoreShakePriority Priority,
		FName Category);

	/** Initialize layered shake manager */
	void InitializeShakeManager();

	/** Apply settings from SSOT (USuspenseCoreSettings) */
	void ApplySSOTSettings();

	/** Load and cache the DataAsset from settings */
	void LoadDataAssetFromSettings();

	/** Called when character lands (via LandedDelegate) */
	UFUNCTION()
	void OnCharacterLanded(const FHitResult& Hit);
};

