// SuspenseCoreCharacterClassSubsystem.h
// SuspenseCore - Character Class Management
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/StreamableManager.h"
#include "AbilitySystemInterface.h"
#include "SuspenseCoreCharacterClassSubsystem.generated.h"

class USuspenseCoreCharacterClassData;
class USuspenseCoreAbilitySystemComponent;
class UAbilitySystemComponent;
class UGameplayEffect;

/**
 * USuspenseCoreCharacterClassSubsystem
 *
 * Manages character class definitions and application.
 * Responsible for:
 * - Loading and caching class data assets
 * - Applying class attributes to players
 * - Managing class unlock progression
 *
 * NOTE: This subsystem works with IAbilitySystemInterface to avoid
 * circular dependencies between GAS and PlayerCore modules.
 *
 * Usage:
 *   auto* ClassSystem = GameInstance->GetSubsystem<USuspenseCoreCharacterClassSubsystem>();
 *   ClassSystem->ApplyClassToActor(PlayerState, "Assault", PlayerLevel);
 */
UCLASS()
class GAS_API USuspenseCoreCharacterClassSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════════════════════════════════
	// SUBSYSTEM LIFECYCLE
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	// ═══════════════════════════════════════════════════════════════════════════
	// STATIC ACCESS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Get subsystem from any world context */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Class", meta = (WorldContext = "WorldContextObject"))
	static USuspenseCoreCharacterClassSubsystem* Get(const UObject* WorldContextObject);

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API - CLASS MANAGEMENT
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Get all available character classes
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Class")
	TArray<USuspenseCoreCharacterClassData*> GetAllClasses() const;

	/**
	 * Get classes unlocked for a specific player level
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Class")
	TArray<USuspenseCoreCharacterClassData*> GetUnlockedClasses(int32 PlayerLevel) const;

	/**
	 * Get starter classes (no level requirement)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Class")
	TArray<USuspenseCoreCharacterClassData*> GetStarterClasses() const;

	/**
	 * Get class data by ID
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Class")
	USuspenseCoreCharacterClassData* GetClassById(FName ClassId) const;

	/**
	 * Check if a class exists
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Class")
	bool ClassExists(FName ClassId) const;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API - CLASS APPLICATION
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Apply a character class to an actor that implements IAbilitySystemInterface
	 * This will:
	 * - Set attribute modifiers
	 * - Grant class abilities
	 * - Apply passive effects
	 *
	 * @param Actor The actor to apply the class to (must implement IAbilitySystemInterface)
	 * @param ClassId The class identifier
	 * @param PlayerLevel The player's current level (for ability unlocks)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Class")
	bool ApplyClassToActor(AActor* Actor, FName ClassId, int32 PlayerLevel = 1);

	/**
	 * Apply class using data asset directly
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Class")
	bool ApplyClassDataToActor(AActor* Actor, USuspenseCoreCharacterClassData* ClassData, int32 PlayerLevel = 1);

	/**
	 * Apply class directly to an ASC
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Class")
	bool ApplyClassToASC(UAbilitySystemComponent* ASC, FName ClassId, int32 PlayerLevel = 1);

	/**
	 * Remove current class from actor (reset to defaults)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Class")
	void RemoveClassFromActor(AActor* Actor);

	/**
	 * Get the currently applied class for an actor
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Class")
	USuspenseCoreCharacterClassData* GetActorCurrentClass(AActor* Actor) const;

	// ═══════════════════════════════════════════════════════════════════════════
	// EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when classes are loaded */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnClassesLoaded, int32, NumClasses);
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Events")
	FOnClassesLoaded OnClassesLoaded;

	/** Called when a class is applied to an actor */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnClassApplied, AActor*, Actor, USuspenseCoreCharacterClassData*, ClassData);
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Events")
	FOnClassApplied OnClassApplied;

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Primary Asset Type for character classes */
	UPROPERTY(EditDefaultsOnly, Category = "Config")
	FPrimaryAssetType CharacterClassAssetType = FPrimaryAssetType(TEXT("CharacterClass"));

	// ═══════════════════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════════════════

	/** Cached class data assets */
	UPROPERTY()
	TMap<FName, USuspenseCoreCharacterClassData*> LoadedClasses;

	/** Actor -> Current Class mapping */
	UPROPERTY()
	TMap<TWeakObjectPtr<AActor>, USuspenseCoreCharacterClassData*> ActorClassMap;

	/** Streamable manager for async loading */
	FStreamableManager StreamableManager;

	/** Handles for async loading */
	TSharedPtr<FStreamableHandle> ClassLoadHandle;

	/** Flag for load completion */
	bool bClassesLoaded = false;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Load all class data assets */
	void LoadAllClasses();

	/** Called when async load completes */
	void OnClassesLoadComplete();

	/** Apply attribute modifiers from class data to ASC */
	void ApplyAttributeModifiers(UAbilitySystemComponent* ASC, const USuspenseCoreCharacterClassData* ClassData);

	/** Grant class abilities to ASC */
	void GrantClassAbilities(UAbilitySystemComponent* ASC, const USuspenseCoreCharacterClassData* ClassData, int32 PlayerLevel);

	/** Apply passive effects */
	void ApplyPassiveEffects(UAbilitySystemComponent* ASC, const USuspenseCoreCharacterClassData* ClassData);

	/** Publish class change to EventBus */
	void PublishClassChangeEvent(AActor* Actor, USuspenseCoreCharacterClassData* ClassData);
};
