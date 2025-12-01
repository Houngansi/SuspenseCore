// SuspenseCoreCharacterSelectionSubsystem.h
// SuspenseCore - Clean Architecture BridgeSystem
// Copyright (c) 2025. All Rights Reserved.
//
// NOTE: This subsystem uses UObject* instead of USuspenseCoreCharacterClassData*
// to avoid module dependency on GAS. Cast to concrete type at usage sites:
//   USuspenseCoreCharacterClassData* ClassData = Cast<USuspenseCoreCharacterClassData>(Subsystem->GetSelectedClass());

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreCharacterSelectionSubsystem.generated.h"

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
 * 1. RegistrationWidget calls SelectCharacterClass() when user clicks class button
 * 2. Subsystem publishes CharacterClass.Changed event
 * 3. PreviewActor on map receives event and updates mesh
 * 4. When game loads, Character calls GetSelectedClass() in BeginPlay
 *
 * ARCHITECTURE:
 * - Lives in BridgeSystem module (accessible from all modules)
 * - Uses EventBus for decoupled communication
 * - Uses UObject* to avoid GAS dependency (cast at usage sites)
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
	 * @param ClassData - Character class data asset (UObject* to avoid GAS dependency)
	 * @param ClassId - Class identifier (FName)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterSelection")
	void SelectCharacterClass(UObject* ClassData, FName ClassId);

	/**
	 * Select character class by ID only.
	 * Loads class data from registry if registered, otherwise stores ID only.
	 * @param ClassId - Class identifier (FName)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterSelection")
	void SelectCharacterClassById(FName ClassId);

	/**
	 * Get currently selected character class data.
	 * Cast to USuspenseCoreCharacterClassData* at usage site.
	 * @return Selected class data or nullptr if none selected
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterSelection")
	UObject* GetSelectedClass() const { return SelectedClassData; }

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
	bool HasSelectedClass() const { return SelectedClassData != nullptr || SelectedClassId != NAME_None; }

	/**
	 * Clear current selection.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterSelection")
	void ClearSelection();

	/**
	 * Load character class data by ID from the class registry.
	 * Cast to USuspenseCoreCharacterClassData* at usage site.
	 * @param ClassId - Class identifier
	 * @return Loaded class data or nullptr if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterSelection")
	UObject* LoadClassById(FName ClassId);

	/**
	 * Register a class data asset with the subsystem.
	 * Called by CharacterClassSubsystem or manually.
	 * @param ClassData - The class data asset (UObject*)
	 * @param ClassId - The class identifier
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterSelection")
	void RegisterClassData(UObject* ClassData, FName ClassId);

	/**
	 * Get all registered class IDs.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterSelection")
	TArray<FName> GetAllRegisteredClassIds() const;

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

	/** Currently selected class data (UObject* to avoid GAS dependency) */
	UPROPERTY()
	UObject* SelectedClassData;

	/** Currently selected class ID */
	FName SelectedClassId;

	/** Registry of class data assets (ClassID → DataAsset as UObject*) */
	UPROPERTY()
	TMap<FName, UObject*> ClassRegistry;

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
