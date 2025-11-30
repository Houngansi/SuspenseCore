// SuspenseCoreCharacterPreviewComponent.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreCharacterPreviewComponent.generated.h"

class USkeletalMeshComponent;
class USuspenseCoreEventBus;
class USuspenseCoreCharacterClassData;
class USkeletalMesh;
class UAnimSequence;

/**
 * USuspenseCoreCharacterPreviewComponent
 *
 * Component for displaying character class preview in UI.
 * Subscribes to EventBus and updates skeletal mesh when class selection changes.
 *
 * EventBus Events (subscribed):
 * - SuspenseCore.Event.UI.ClassPreview.Selected
 *
 * Usage:
 * 1. Add this component to an Actor (e.g., CharacterPreviewActor)
 * 2. Assign a SkeletalMeshComponent in Details panel
 * 3. Place the actor in front of a camera for widget viewport capture
 * 4. When class buttons are clicked, publish ClassPreview.Selected event
 * 5. This component automatically updates the mesh
 *
 * Alternative: Use SetClassById() directly from Blueprint
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UISYSTEM_API USuspenseCoreCharacterPreviewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USuspenseCoreCharacterPreviewComponent();

	// ═══════════════════════════════════════════════════════════════════════════
	// COMPONENT LIFECYCLE
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Set the preview to a specific class by ID.
	 * @param ClassId - The ClassID from CharacterClassData (e.g., "Assault", "Medic")
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterPreview")
	void SetClassById(FName ClassId);

	/**
	 * Set the preview using ClassData directly.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterPreview")
	void SetClassData(USuspenseCoreCharacterClassData* ClassData);

	/**
	 * Clear the preview (hide mesh).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterPreview")
	void ClearPreview();

	/**
	 * Play the idle preview animation.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterPreview")
	void PlayPreviewAnimation();

	/**
	 * Get current class ID.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterPreview")
	FName GetCurrentClassId() const { return CurrentClassId; }

	/**
	 * Get current class data.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterPreview")
	USuspenseCoreCharacterClassData* GetCurrentClassData() const { return CurrentClassData.Get(); }

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when class preview changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|CharacterPreview|Events")
	void OnClassPreviewChanged(FName ClassId, USuspenseCoreCharacterClassData* ClassData);

	/** Called when mesh loading starts (for loading indicators) */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|CharacterPreview|Events")
	void OnMeshLoadingStarted();

	/** Called when mesh loading completes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|CharacterPreview|Events")
	void OnMeshLoadingCompleted();

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** The skeletal mesh component to update. If null, will try to find one on owner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|CharacterPreview|Config")
	TObjectPtr<USkeletalMeshComponent> PreviewMeshComponent;

	/** Default class to show on BeginPlay */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|CharacterPreview|Config")
	FName DefaultClassId = NAME_None;

	/** Auto-subscribe to EventBus for class preview events */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|CharacterPreview|Config")
	bool bAutoSubscribeToEvents = true;

	/** Play animation automatically when class changes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|CharacterPreview|Config")
	bool bAutoPlayAnimation = true;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL STATE
	// ═══════════════════════════════════════════════════════════════════════════

	/** Current class ID */
	FName CurrentClassId;

	/** Current class data */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreCharacterClassData> CurrentClassData;

	/** Cached EventBus */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** EventBus subscription handle */
	FSuspenseCoreSubscriptionHandle ClassPreviewEventHandle;

	/** Is mesh currently loading? */
	bool bIsLoadingMesh = false;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Setup EventBus subscriptions */
	void SetupEventSubscriptions();

	/** Teardown EventBus subscriptions */
	void TeardownEventSubscriptions();

	/** Find skeletal mesh component on owner if not assigned */
	void FindMeshComponentIfNeeded();

	/** Handle ClassPreview event from EventBus */
	void OnClassPreviewEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Apply class data to mesh */
	void ApplyClassDataToMesh(USuspenseCoreCharacterClassData* ClassData);

	/** Async load mesh and apply */
	void LoadAndApplyMesh(TSoftObjectPtr<USkeletalMesh> MeshPtr);

	/** Called when mesh loading completes */
	void OnMeshLoaded();
};
