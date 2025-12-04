// ISuspenseCoreInteractable.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "ISuspenseCoreInteractable.generated.h"

// Forward declarations
class APlayerController;

/**
 * USuspenseCoreInteractable
 *
 * UInterface definition for SuspenseCore interactable objects.
 *
 * IMPORTANT: This is a SuspenseCore interface.
 * DO NOT use legacy ISuspenseInteract in new SuspenseCore code!
 */
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreInteractable
 *
 * Interface for all interactable objects in SuspenseCore architecture.
 * Uses EventBus for event broadcasting instead of legacy delegate system.
 *
 * ARCHITECTURE PRINCIPLES:
 * - Minimal, focused interface - only essential interaction methods
 * - No legacy delegate manager dependency
 * - Events broadcast through ISuspenseCoreEventEmitter
 * - GameplayTag-based interaction types
 *
 * MIGRATION FROM LEGACY:
 * - Replace ISuspenseInteract with ISuspenseCoreInteractable
 * - Use EventBus for events instead of GetDelegateManager()
 * - Static broadcast helpers removed - use EmitEvent() directly
 *
 * @see ISuspenseInteract (Legacy - DO NOT USE in new code)
 * @see ISuspenseCoreEventEmitter
 */
class BRIDGESYSTEM_API ISuspenseCoreInteractable
{
	GENERATED_BODY()

public:
	//==================================================================
	// Core Interaction Methods
	//==================================================================

	/**
	 * Check if interaction is currently possible.
	 * @param InstigatingController Controller attempting interaction
	 * @return true if interaction is allowed
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Interaction")
	bool CanInteract(APlayerController* InstigatingController) const;

	/**
	 * Perform interaction with this object.
	 * Implementation should emit interaction events through EventBus.
	 * @param InstigatingController Controller performing interaction
	 * @return true if interaction was successful
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Interaction")
	bool Interact(APlayerController* InstigatingController);

	/**
	 * Get interaction type for UI and system classification.
	 * @return GameplayTag identifying interaction type
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Interaction")
	FGameplayTag GetInteractionType() const;

	/**
	 * Get localized text for interaction prompt.
	 * @return Display text for UI
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Interaction")
	FText GetInteractionPrompt() const;

	//==================================================================
	// Interaction Configuration
	//==================================================================

	/**
	 * Get interaction priority for sorting overlapping interactables.
	 * Higher values indicate higher priority.
	 * @return Priority value (default: 0)
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Interaction")
	int32 GetInteractionPriority() const;

	/**
	 * Get maximum interaction distance.
	 * @return Distance in Unreal units (default: 200.0)
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Interaction")
	float GetInteractionDistance() const;

	//==================================================================
	// Focus Notifications
	//==================================================================

	/**
	 * Called when object becomes the focused interaction target.
	 * Use for visual feedback (highlight, outline, etc.).
	 * @param InstigatingController Controller that focused on this object
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Interaction")
	void OnFocusGained(APlayerController* InstigatingController);

	/**
	 * Called when object loses interaction focus.
	 * Use to disable visual feedback.
	 * @param InstigatingController Controller that lost focus
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Interaction")
	void OnFocusLost(APlayerController* InstigatingController);
};
