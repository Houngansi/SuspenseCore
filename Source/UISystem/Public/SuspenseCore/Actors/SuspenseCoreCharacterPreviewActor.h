// SuspenseCoreCharacterPreviewActor.h
// Separate actor for character preview - solves OwnerNoSee issues in UE5.7+
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreCharacterPreviewActor.generated.h"

class USkeletalMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UPointLightComponent;
class USpringArmComponent;
class USuspenseCoreEventBus;

/**
 * ASuspenseCoreCharacterPreviewActor
 *
 * Dedicated actor for character preview rendering.
 * Spawns in a hidden location and captures character mesh to RenderTarget.
 *
 * WHY SEPARATE ACTOR:
 * - SceneCaptureComponent respects OwnerNoSee
 * - If attached to Character, it won't see the mesh (same owner)
 * - Separate actor = separate owner = no visibility issues
 *
 * USAGE:
 * 1. Spawn this actor when menu opens
 * 2. Call SetPreviewMesh() with the skeletal mesh to display
 * 3. Get RenderTarget via GetRenderTarget()
 * 4. Destroy when menu closes
 *
 * EVENTBUS:
 * - Subscribes to: SuspenseCore.Event.UI.CharacterPreview.RequestRotation
 * - Publishes: SuspenseCore.Event.Player.RenderTargetReady
 */
UCLASS()
class UISYSTEM_API ASuspenseCoreCharacterPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	ASuspenseCoreCharacterPreviewActor();

	// ═══════════════════════════════════════════════════════════════════════════
	// LIFECYCLE
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Set the skeletal mesh to preview.
	 * @param Mesh - Skeletal mesh asset to display
	 * @param AnimInstance - Optional animation blueprint class
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Preview")
	void SetPreviewMesh(USkeletalMesh* Mesh, TSubclassOf<UAnimInstance> AnimInstance = nullptr);

	/**
	 * Copy mesh and animation from a character.
	 * @param SourceCharacter - Character to copy from
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Preview")
	void CopyFromCharacter(ACharacter* SourceCharacter);

	/** Get the render target for UI display */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Preview")
	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

	/** Rotate preview mesh */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Preview")
	void RotatePreview(float DeltaYaw);

	/** Set absolute rotation */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Preview")
	void SetPreviewRotation(float Yaw);

	/** Play animation on preview mesh */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Preview")
	void PlayAnimation(UAnimationAsset* Animation, bool bLooping = true);

	/** Force capture update */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Preview")
	void RefreshCapture();

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// COMPONENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Preview skeletal mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Components")
	USkeletalMeshComponent* PreviewMesh;

	/** Camera boom for positioning */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Components")
	USpringArmComponent* CameraBoom;

	/** Scene capture component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Components")
	USceneCaptureComponent2D* CaptureComponent;

	/** Lighting for the preview */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Components")
	UPointLightComponent* PreviewLight;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Render target resolution */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	int32 RenderTargetWidth = 512;

	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	int32 RenderTargetHeight = 512;

	/** Camera distance from mesh */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	float CameraDistance = 200.0f;

	/** Camera height offset */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	float CameraHeightOffset = 80.0f;

	/** Camera field of view */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config", meta = (ClampMin = "5.0", ClampMax = "170.0"))
	float CameraFOV = 30.0f;

	/** Light intensity */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	float LightIntensity = 5000.0f;

	/** Background color for render target */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	FLinearColor BackgroundColor = FLinearColor::Transparent;

	// ═══════════════════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════════════════

	/** Created render target */
	UPROPERTY(Transient)
	UTextureRenderTarget2D* RenderTarget;

	/** Current preview rotation */
	float CurrentYaw = 0.0f;

	/** EventBus subscription */
	FSuspenseCoreSubscriptionHandle RotationEventHandle;

	/** Cached EventBus */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL
	// ═══════════════════════════════════════════════════════════════════════════

	void CreateRenderTarget();
	void SetupCaptureComponent();
	void SetupEventSubscriptions();
	void TeardownEventSubscriptions();
	void PublishRenderTargetReady();

	void OnRotationRequested(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
};
