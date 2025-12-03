// SuspenseCoreCharacterPreviewActor.h
// SuspenseCore - Clean Architecture UISystem
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreCharacterPreviewActor.generated.h"

class USkeletalMeshComponent;
class USuspenseCoreCharacterClassData;
class USuspenseCoreEventBus;

/**
 * ASuspenseCoreCharacterPreviewActor
 *
 * 3D character preview controller for character selection screen.
 * Spawns and manages PreviewActorClass from CharacterClassData.
 *
 * KEY FEATURES:
 * - Spawns Blueprint Actor (MetaHuman, Character, etc.) from ClassData
 * - EventBus subscription for class changes
 * - Automatic actor swap when class changes
 * - Optional rotation support (mouse drag)
 * - Applies custom AnimBP if specified in ClassData
 *
 * USAGE:
 * 1. Place BP_CharacterPreviewActor on CharacterSelectMap
 * 2. Position camera to view the spawn location
 * 3. When RegistrationWidget class buttons are clicked, this actor swaps preview automatically
 *
 * ARCHITECTURE:
 * - Receives SuspenseCore.Event.CharacterClass.Changed events
 * - Spawns PreviewActorClass from CharacterClassData
 * - Manages spawned actor lifecycle (destroy old, spawn new)
 * - No RenderTarget, no SceneCaptureComponent - direct 3D display
 */
UCLASS()
class UISYSTEM_API ASuspenseCoreCharacterPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	ASuspenseCoreCharacterPreviewActor();

	// ═══════════════════════════════════════════════════════════════════════════
	// AActor Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Set character class data (updates mesh and animation).
	 * @param ClassData - Character class data asset
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Preview")
	void SetCharacterClass(USuspenseCoreCharacterClassData* ClassData);

	/**
	 * Get currently displayed character class.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Preview")
	USuspenseCoreCharacterClassData* GetCurrentClass() const { return CurrentClassData; }

	/**
	 * Rotate preview mesh (for mouse drag rotation).
	 * @param DeltaYaw - Rotation delta in degrees
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Preview")
	void RotatePreview(float DeltaYaw);

	/**
	 * Set preview rotation (absolute).
	 * @param Yaw - Absolute yaw rotation in degrees
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Preview")
	void SetPreviewRotation(float Yaw);

	/**
	 * Get current preview rotation.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Preview")
	float GetPreviewRotation() const { return CurrentYaw; }

	/**
	 * Get the preview mesh component from spawned actor.
	 * Returns first SkeletalMeshComponent found in the spawned preview actor.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Preview")
	USkeletalMeshComponent* GetPreviewMesh() const;

	/**
	 * Get the currently spawned preview actor.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Preview")
	AActor* GetSpawnedPreviewActor() const { return SpawnedPreviewActor; }

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// COMPONENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Scene root for positioning */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Components")
	USceneComponent* PreviewRoot;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Default class to display on BeginPlay (optional) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Config")
	USuspenseCoreCharacterClassData* DefaultClassData;

	/** Rotation speed multiplier for mouse drag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Config")
	float RotationSpeed = 1.0f;

	/** Enable automatic subscription to CharacterClass.Changed events */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Config")
	bool bAutoSubscribeToEvents = true;

	/**
	 * If true, this actor will ONLY spawn preview actors (not use itself).
	 * If false, this actor IS the preview and won't spawn anything.
	 * Use this when you place a MetaHuman directly on the level as preview.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Config")
	bool bSpawnsPreviewActor = true;

	/**
	 * Unique ID for this preview actor.
	 * Only ONE actor with each ID will be active at a time.
	 * Leave empty to allow multiple instances.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Config")
	FName PreviewActorId = NAME_None;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL STATE
	// ═══════════════════════════════════════════════════════════════════════════

	/** Currently displayed class data */
	UPROPERTY()
	USuspenseCoreCharacterClassData* CurrentClassData;

	/** Currently spawned preview actor (MetaHuman, Character, etc.) */
	UPROPERTY()
	AActor* SpawnedPreviewActor;

	/** Current yaw rotation */
	float CurrentYaw = 0.0f;

	/** Cached EventBus */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** EventBus subscription handles */
	FSuspenseCoreSubscriptionHandle ClassChangedEventHandle;
	FSuspenseCoreSubscriptionHandle RotateEventHandle;
	FSuspenseCoreSubscriptionHandle SetRotationEventHandle;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Subscribe to EventBus events */
	void SetupEventSubscriptions();

	/** Unsubscribe from events */
	void TeardownEventSubscriptions();

	/** Get EventBus from EventManager */
	USuspenseCoreEventBus* GetEventBus();

	/** Handle class changed event from EventBus */
	void OnCharacterClassChanged(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle rotate preview event from EventBus */
	void OnRotatePreviewEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle set rotation event from EventBus */
	void OnSetRotationEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Spawn preview actor from class data */
	void SpawnPreviewActor(USuspenseCoreCharacterClassData* ClassData);

	/** Destroy current preview actor */
	void DestroyPreviewActor();

	/** Apply animation blueprint to spawned actor */
	void ApplyAnimationBlueprint(USuspenseCoreCharacterClassData* ClassData);

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when character class changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Preview")
	void OnClassChanged(USuspenseCoreCharacterClassData* NewClassData);
};
