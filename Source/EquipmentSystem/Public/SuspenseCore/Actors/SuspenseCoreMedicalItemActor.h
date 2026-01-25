// SuspenseCoreMedicalItemActor.h
// Visual actor for medical items held in hand during use animation
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE:
// This actor represents a medical item held in the character's hand.
// - Visual only (no gameplay logic)
// - Attached to hand socket during equip/use animations
// - Mesh loaded from SSOT (DataManager)
//
// FLOW:
// 1. GA_MedicalEquip activates → MedicalVisualHandler spawns this actor
// 2. Actor attached to character's weapon_r socket
// 3. Visible during equip and use animations
// 4. Destroyed when GA_MedicalEquip ends
//
// REFERENCES:
// - ASuspenseCoreGrenadeProjectile: Similar pattern for grenade visuals
// - SuspenseCoreGrenadeHandler: Spawn/attach logic reference
//
// @see USuspenseCoreMedicalVisualHandler
// @see GA_MedicalEquip, GA_MedicalUse

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreMedicalItemActor.generated.h"

// Forward declarations
class UStaticMeshComponent;
class USkeletalMeshComponent;
class USuspenseCoreDataManager;

/**
 * Medical item type for visual selection
 */
UENUM(BlueprintType)
enum class ESuspenseCoreMedicalVisualType : uint8
{
	Bandage         UMETA(DisplayName = "Bandage"),
	Medkit          UMETA(DisplayName = "Medkit/IFAK"),
	Injector        UMETA(DisplayName = "Injector/Syringe"),
	Splint          UMETA(DisplayName = "Splint"),
	Surgical        UMETA(DisplayName = "Surgical Kit"),
	Pills           UMETA(DisplayName = "Pills/Painkillers"),
	Generic         UMETA(DisplayName = "Generic Medical")
};

/**
 * ASuspenseCoreMedicalItemActor
 *
 * Visual representation of medical items during use.
 * Spawned by MedicalVisualHandler and attached to character's hand.
 *
 * USAGE:
 * 1. Spawn via MedicalVisualHandler::SpawnVisualMedical()
 * 2. Call InitializeFromItemID() to load mesh from SSOT
 * 3. Handler attaches to character hand socket
 * 4. Destroy when medical use completes/cancels
 *
 * CONFIGURATION:
 * - Set default meshes in Blueprint for each medical type
 * - SSOT can override with item-specific meshes
 *
 * @see USuspenseCoreMedicalVisualHandler
 */
UCLASS(BlueprintType, Blueprintable)
class EQUIPMENTSYSTEM_API ASuspenseCoreMedicalItemActor : public AActor
{
	GENERATED_BODY()

public:
	ASuspenseCoreMedicalItemActor();

	//==================================================================
	// Components
	//==================================================================

	/** Visual mesh component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	//==================================================================
	// Configuration - Default Meshes per Type
	//==================================================================

	/** Default mesh for bandages */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Meshes")
	TObjectPtr<UStaticMesh> BandageMesh;

	/** Default mesh for medkits (IFAK, AFAK, Salewa) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Meshes")
	TObjectPtr<UStaticMesh> MedkitMesh;

	/** Default mesh for injectors (morphine, adrenaline) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Meshes")
	TObjectPtr<UStaticMesh> InjectorMesh;

	/** Default mesh for splints */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Meshes")
	TObjectPtr<UStaticMesh> SplintMesh;

	/** Default mesh for surgical kits */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Meshes")
	TObjectPtr<UStaticMesh> SurgicalMesh;

	/** Default mesh for pills/painkillers */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Meshes")
	TObjectPtr<UStaticMesh> PillsMesh;

	/** Fallback mesh if no specific mesh found */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Meshes")
	TObjectPtr<UStaticMesh> GenericMesh;

	//==================================================================
	// Configuration - Transform Offsets per Type
	// Adjusts position/rotation when attached to hand
	//==================================================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Transform")
	FTransform BandageOffset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Transform")
	FTransform MedkitOffset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Transform")
	FTransform InjectorOffset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Transform")
	FTransform SplintOffset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Transform")
	FTransform SurgicalOffset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Transform")
	FTransform PillsOffset;

	//==================================================================
	// Initialization
	//==================================================================

	/**
	 * Initialize from item ID
	 * Loads mesh and settings from SSOT via DataManager
	 *
	 * @param ItemID Medical item ID (e.g., "Medical_IFAK")
	 * @param InDataManager DataManager for SSOT lookup
	 */
	UFUNCTION(BlueprintCallable, Category = "Medical")
	void InitializeFromItemID(FName ItemID, USuspenseCoreDataManager* InDataManager);

	/**
	 * Initialize from medical type tag
	 * Uses default mesh for the type
	 *
	 * @param MedicalTypeTag Tag like "Item.Medical.Medkit"
	 */
	UFUNCTION(BlueprintCallable, Category = "Medical")
	void InitializeFromTypeTag(FGameplayTag MedicalTypeTag);

	/**
	 * Set mesh directly
	 *
	 * @param NewMesh Static mesh to use
	 */
	UFUNCTION(BlueprintCallable, Category = "Medical")
	void SetMesh(UStaticMesh* NewMesh);

	//==================================================================
	// Runtime Accessors
	//==================================================================

	/** Get current medical item ID */
	UFUNCTION(BlueprintPure, Category = "Medical")
	FName GetMedicalItemID() const { return MedicalItemID; }

	/** Get current visual type */
	UFUNCTION(BlueprintPure, Category = "Medical")
	ESuspenseCoreMedicalVisualType GetVisualType() const { return VisualType; }

	/** Get transform offset for current type */
	UFUNCTION(BlueprintPure, Category = "Medical")
	FTransform GetAttachOffset() const;

	//==================================================================
	// Visual State
	//==================================================================

	/**
	 * Set visibility with optional fade
	 */
	UFUNCTION(BlueprintCallable, Category = "Medical")
	void SetVisibility(bool bVisible);

	/**
	 * Reset actor for pooling reuse
	 */
	UFUNCTION(BlueprintCallable, Category = "Medical")
	void ResetForPool();

protected:
	//==================================================================
	// AActor Overrides
	//==================================================================

	virtual void BeginPlay() override;

	//==================================================================
	// Internal Methods
	//==================================================================

	/**
	 * Determine visual type from item tags
	 */
	ESuspenseCoreMedicalVisualType DetermineVisualType(const FGameplayTagContainer& ItemTags) const;

	/**
	 * Get default mesh for visual type
	 */
	UStaticMesh* GetMeshForType(ESuspenseCoreMedicalVisualType Type) const;

private:
	//==================================================================
	// Runtime State
	//==================================================================

	/** Current medical item ID */
	UPROPERTY()
	FName MedicalItemID;

	/** Current visual type */
	UPROPERTY()
	ESuspenseCoreMedicalVisualType VisualType = ESuspenseCoreMedicalVisualType::Generic;

	/** Cached DataManager reference */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreDataManager> DataManager;
};
