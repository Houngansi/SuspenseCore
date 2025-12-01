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
 * Simple 3D character preview actor for character selection screen.
 * Place this actor on your CharacterSelectMap to display character preview.
 *
 * KEY FEATURES:
 * - SkeletalMeshComponent for 3D character display
 * - EventBus subscription for class changes
 * - Automatic mesh/animation update when class changes
 * - Optional rotation support (mouse drag)
 *
 * USAGE:
 * 1. Place BP_CharacterPreviewActor on CharacterSelectMap
 * 2. Position camera to view the actor
 * 3. When RegistrationWidget class buttons are clicked, this actor updates automatically
 *
 * ARCHITECTURE:
 * - Receives SuspenseCore.Event.CharacterClass.Changed events
 * - Updates SkeletalMesh and AnimBP from CharacterClassData
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
	 * Get the preview mesh component.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Preview")
	USkeletalMeshComponent* GetPreviewMesh() const { return PreviewMesh; }

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// COMPONENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Skeletal mesh for character preview */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Components")
	USkeletalMeshComponent* PreviewMesh;

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

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL STATE
	// ═══════════════════════════════════════════════════════════════════════════

	/** Currently displayed class data */
	UPROPERTY()
	USuspenseCoreCharacterClassData* CurrentClassData;

	/** Current yaw rotation */
	float CurrentYaw = 0.0f;

	/** Cached EventBus */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** EventBus subscription handle */
	FSuspenseCoreSubscriptionHandle ClassChangedEventHandle;

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

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when character class changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Preview")
	void OnClassChanged(USuspenseCoreCharacterClassData* NewClassData);
};
