// SuspenseCorePreviewRotationWidget.h
// SuspenseCore - Clean Architecture UISystem
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCorePreviewRotationWidget.generated.h"

class USuspenseCoreEventBus;
class ASuspenseCoreCharacterPreviewActor;

/**
 * USuspenseCorePreviewRotationWidget
 *
 * Invisible/transparent overlay widget that captures mouse drag input
 * to rotate the 3D character preview. Place this widget over the
 * area where the preview character is displayed.
 *
 * INPUT HANDLING:
 * - Left mouse button drag rotates the preview character
 * - Delta X movement is converted to yaw rotation
 * - Publishes SuspenseCore.Event.Preview.Rotate event via EventBus
 *
 * USAGE:
 * 1. Add this widget as overlay in your UI (WBP_CharacterSelect, WBP_Registration)
 * 2. Position/size it to cover the preview area
 * 3. Set RotationSensitivity as needed
 * 4. Optional: Set PreviewActorReference for direct control (bypasses EventBus)
 *
 * ARCHITECTURE:
 * - Publishes: SuspenseCore.Event.Preview.Rotate (DeltaYaw: float)
 * - CharacterPreviewActor subscribes to this event
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API USuspenseCorePreviewRotationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCorePreviewRotationWidget(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Rotation sensitivity (degrees per pixel of mouse movement) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Preview Rotation")
	float RotationSensitivity = 0.5f;

	/** Invert rotation direction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Preview Rotation")
	bool bInvertRotation = false;

	/** Optional direct reference to preview actor (bypasses EventBus) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Preview Rotation")
	ASuspenseCoreCharacterPreviewActor* PreviewActorReference;

	/** Enable rotation input */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Preview Rotation")
	bool bRotationEnabled = true;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/** Check if currently dragging */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Preview Rotation")
	bool IsDragging() const { return bIsDragging; }

	/** Set preview actor reference at runtime */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Preview Rotation")
	void SetPreviewActor(ASuspenseCoreCharacterPreviewActor* InPreviewActor);

	/** Reset rotation to default (0 degrees) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Preview Rotation")
	void ResetRotation();

	/** Set absolute rotation */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Preview Rotation")
	void SetRotation(float Yaw);

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when drag starts */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Preview Rotation")
	void OnDragStarted();

	/** Called when drag ends */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Preview Rotation")
	void OnDragEnded();

	/** Called when rotation changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Preview Rotation")
	void OnRotationChanged(float DeltaYaw, float TotalYaw);

	// ═══════════════════════════════════════════════════════════════════════════
	// DELEGATES
	// ═══════════════════════════════════════════════════════════════════════════

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDragStartedDelegate);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDragEndedDelegate);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRotationChangedDelegate, float, DeltaYaw, float, TotalYaw);

	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Preview Rotation")
	FOnDragStartedDelegate OnDragStartedDelegate;

	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Preview Rotation")
	FOnDragEndedDelegate OnDragEndedDelegate;

	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Preview Rotation")
	FOnRotationChangedDelegate OnRotationChangedDelegate;

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// UUserWidget Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL STATE
	// ═══════════════════════════════════════════════════════════════════════════

	/** Is currently dragging */
	bool bIsDragging = false;

	/** Last mouse position during drag */
	FVector2D LastMousePosition;

	/** Accumulated rotation since widget creation */
	float AccumulatedYaw = 0.0f;

	/** Cached EventBus */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Get EventBus from EventManager */
	USuspenseCoreEventBus* GetEventBus();

	/** Apply rotation delta (via EventBus or direct reference) */
	void ApplyRotationDelta(float DeltaYaw);

	/** Publish rotation event via EventBus */
	void PublishRotationEvent(float DeltaYaw);
};
