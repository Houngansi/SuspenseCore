// SuspenseCoreCharacterSelectionSubsystem.h
// SuspenseCore - Clean Architecture BridgeSystem
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreCharacterSelectionSubsystem.generated.h"

class USuspenseCoreCharacterClassData;
class USuspenseCoreEventBus;

/**
 * USuspenseCoreCharacterSelectionSubsystem
 *
 * GameInstance subsystem that persists selected character class between maps.
 * Used for character selection screen → gameplay transition.
 *
 * KEY RESPONSIBILITIES:
 * - Store selected character class (persists across map transitions)
 * - Provide class data to Character on spawn
 * - Publish EventBus events when class changes
 * - Load class data from DataAsset registry
 *
 * USAGE:
 * 1. RegistrationWidget calls SelectCharacterClassById() when user clicks class button
 * 2. Subsystem publishes CharacterClass.Changed event
 * 3. PreviewActor on map receives event and updates mesh
 * 4. When game loads, Character calls GetSelectedClass() in BeginPlay
 *
 * ARCHITECTURE:
 * - Lives in BridgeSystem module (accessible from all modules)
 * - Uses EventBus for decoupled communication
 * - No dependencies on PlayerCore or UISystem
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreCharacterSelectionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════════════════════════════════
	// UGameInstanceSubsystem Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Select character class by data asset reference.
	 * Publishes CharacterClass.Changed event.
	 * @param ClassData - Character class data asset
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterSelection")
	void SelectCharacterClass(USuspenseCoreCharacterClassData* ClassData);

	/**
	 * Select character class by ID (e.g., "Assault", "Medic", "Sniper").
	 * Loads class data from registry and publishes event.
	 * @param ClassId - Class identifier (FName)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterSelection")
	void SelectCharacterClassById(FName ClassId);

	/**
	 * Get currently selected character class.
	 * @return Selected class data or nullptr if none selected
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterSelection")
	USuspenseCoreCharacterClassData* GetSelectedClass() const { return SelectedClassData; }

	/**
	 * Get currently selected class ID.
	 * @return Class ID or NAME_None if none selected
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterSelection")
	FName GetSelectedClassId() const { return SelectedClassId; }

	/**
	 * Check if a class is currently selected.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterSelection")
	bool HasSelectedClass() const { return SelectedClassData != nullptr; }

	/**
	 * Clear current selection.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterSelection")
	void ClearSelection();

	/**
	 * Load character class data by ID from the class registry.
	 * @param ClassId - Class identifier
	 * @return Loaded class data or nullptr if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterSelection")
	USuspenseCoreCharacterClassData* LoadClassById(FName ClassId);

	/**
	 * Register a class data asset with the subsystem.
	 * Called by CharacterClassSubsystem or manually.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterSelection")
	void RegisterClassData(USuspenseCoreCharacterClassData* ClassData);

	/**
	 * Get all registered class data assets.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterSelection")
	TArray<USuspenseCoreCharacterClassData*> GetAllRegisteredClasses() const;

	// ═══════════════════════════════════════════════════════════════════════════
	// STATIC ACCESSOR
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Get subsystem instance from world context.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterSelection", meta = (WorldContext = "WorldContextObject"))
	static USuspenseCoreCharacterSelectionSubsystem* Get(const UObject* WorldContextObject);

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL STATE
	// ═══════════════════════════════════════════════════════════════════════════

	/** Currently selected class data */
	UPROPERTY()
	USuspenseCoreCharacterClassData* SelectedClassData;

	/** Currently selected class ID */
	FName SelectedClassId;

	/** Registry of class data assets (ClassID → DataAsset) */
	UPROPERTY()
	TMap<FName, USuspenseCoreCharacterClassData*> ClassRegistry;

	/** Cached EventBus */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Get EventBus from EventManager */
	USuspenseCoreEventBus* GetEventBus();

	/** Publish class changed event */
	void PublishClassChangedEvent();

	/** Publish class selected (confirmed) event */
	void PublishClassSelectedEvent();
};
