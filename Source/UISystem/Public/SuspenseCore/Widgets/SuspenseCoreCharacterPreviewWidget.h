// SuspenseCoreCharacterPreviewWidget.h
// SuspenseCore - Character Preview with Mouse Rotation
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreCharacterPreviewWidget.generated.h"

class UImage;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USuspenseCoreEventBus;

/**
 * USuspenseCoreCharacterPreviewWidget
 *
 * Widget for displaying character render target with mouse rotation support.
 * Communicates with character via EventBus (no direct module dependency).
 *
 * USAGE:
 * 1. Add this widget to your UI layout
 * 2. It will automatically subscribe to RenderTargetReady event
 * 3. Click and drag to rotate the character preview
 *
 * EVENTBUS COMMUNICATION:
 * - Subscribes to: SuspenseCore.Event.Player.RenderTargetReady
 * - Publishes: SuspenseCore.Event.UI.CharacterPreview.RequestCapture
 * - Publishes: SuspenseCore.Event.UI.CharacterPreview.RequestRotation
 */
UCLASS()
class UISYSTEM_API USuspenseCoreCharacterPreviewWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreCharacterPreviewWidget(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════
	// WIDGET LIFECYCLE
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ═══════════════════════════════════════════════════════════════════════════
	// MOUSE INPUT (Rotation Control)
	// ═══════════════════════════════════════════════════════════════════════════

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/** Enable character capture (call when widget becomes visible) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterPreview")
	void EnableCapture();

	/** Disable character capture (call when widget is hidden) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterPreview")
	void DisableCapture();

	/** Manually set the render target */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterPreview")
	void SetRenderTarget(UTextureRenderTarget2D* RenderTarget);

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Image widget for displaying character preview */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UImage* PreviewImage;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Base material for character preview display.
	 * Must have a TextureParameter named 'RenderTargetTexture'.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Config")
	UMaterialInterface* PreviewBaseMaterial;

	/** Mouse drag sensitivity for rotation */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Config")
	float RotationSensitivity = 0.5f;

	/** Image size for the preview brush */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Config")
	FVector2D PreviewImageSize = FVector2D(512.0f, 512.0f);

	/** Automatically enable capture when widget is constructed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Config")
	bool bAutoEnableCapture = true;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL STATE
	// ═══════════════════════════════════════════════════════════════════════════

	/** EventBus subscription handle for render target ready */
	FSuspenseCoreSubscriptionHandle RenderTargetReadyEventHandle;

	/** Cached EventBus */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** Dynamic material instance for preview */
	UPROPERTY()
	UMaterialInstanceDynamic* PreviewMaterial;

	/** Cached render target */
	UPROPERTY()
	UTextureRenderTarget2D* CachedRenderTarget;

	/** Is currently dragging for rotation */
	bool bIsDragging = false;

	/** Last mouse position during drag */
	FVector2D LastMousePosition = FVector2D::ZeroVector;

	/** Is capture currently enabled */
	bool bCaptureEnabled = false;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════════════════════════════════

	void SetupEventSubscriptions();
	void TeardownEventSubscriptions();
	void UpdatePreviewImage(UTextureRenderTarget2D* RenderTarget);
	void PublishRotationDelta(float DeltaYaw);
	void PublishCaptureRequest(bool bEnable);

	// ═══════════════════════════════════════════════════════════════════════════
	// EVENTBUS HANDLERS
	// ═══════════════════════════════════════════════════════════════════════════

	void OnRenderTargetReady(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
};
