// SuspenseCoreMedicalItemActor.h
// Visual actor for medical items held in hand during use animation
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE:
// This actor represents a medical item held in the character's hand.
// - Visual only (no gameplay logic)
// - Attached to hand socket during equip/use animations
// - Supports both StaticMesh and SkeletalMesh
// - Mesh loaded from SSOT (DataManager)
//
// MESH TYPES:
// - StaticMesh: Simple items (bandages, medkits, pills)
// - SkeletalMesh: Items with animation (syringes, surgical tools)
//
// FLOW:
// 1. GA_MedicalEquip activates → MedicalVisualHandler spawns this actor
// 2. Actor attached to character's weapon_r socket
// 3. Visible during equip and use animations
// 4. Destroyed when GA_MedicalEquip ends
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
class UAnimationAsset;
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
 * Mesh type selection for medical items
 */
UENUM(BlueprintType)
enum class ESuspenseCoreMedicalMeshType : uint8
{
	StaticMesh      UMETA(DisplayName = "Static Mesh"),
	SkeletalMesh    UMETA(DisplayName = "Skeletal Mesh")
};

/**
 * Configuration for a single medical visual type
 * Allows choosing between Static and Skeletal mesh per type
 */
USTRUCT(BlueprintType)
struct FSuspenseCoreMedicalVisualConfig
{
	GENERATED_BODY()

	/** Which mesh type to use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	ESuspenseCoreMedicalMeshType MeshType = ESuspenseCoreMedicalMeshType::StaticMesh;

	/** Static mesh (used if MeshType == StaticMesh) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config",
		meta = (EditCondition = "MeshType == ESuspenseCoreMedicalMeshType::StaticMesh"))
	TObjectPtr<UStaticMesh> StaticMesh;

	/** Skeletal mesh (used if MeshType == SkeletalMesh) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config",
		meta = (EditCondition = "MeshType == ESuspenseCoreMedicalMeshType::SkeletalMesh"))
	TObjectPtr<USkeletalMesh> SkeletalMesh;

	/** Animation to play on skeletal mesh (optional) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config",
		meta = (EditCondition = "MeshType == ESuspenseCoreMedicalMeshType::SkeletalMesh"))
	TObjectPtr<UAnimationAsset> IdleAnimation;

	/** Transform offset when attached to hand */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	FTransform AttachOffset;

	FSuspenseCoreMedicalVisualConfig()
		: MeshType(ESuspenseCoreMedicalMeshType::StaticMesh)
		, AttachOffset(FTransform::Identity)
	{
	}
};

/**
 * ASuspenseCoreMedicalItemActor
 *
 * Visual representation of medical items during use.
 * Supports both StaticMesh (bandages) and SkeletalMesh (syringes).
 *
 * USAGE:
 * 1. Spawn via MedicalVisualHandler::SpawnVisualMedical()
 * 2. Call InitializeFromItemID() to load mesh from SSOT
 * 3. Handler attaches to character hand socket
 * 4. Destroy when medical use completes/cancels
 *
 * CONFIGURATION:
 * - Set VisualConfigs in Blueprint for each medical type
 * - Each type can use StaticMesh OR SkeletalMesh
 * - Skeletal meshes can have idle animations
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

	/** Static mesh component (for simple items like bandages) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;

	/** Skeletal mesh component (for animated items like syringes) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;

	//==================================================================
	// Configuration - Visual Configs per Type
	// Each type can use either StaticMesh or SkeletalMesh
	//==================================================================

	/** Configuration for bandages (typically StaticMesh) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Config")
	FSuspenseCoreMedicalVisualConfig BandageConfig;

	/** Configuration for medkits/IFAK (typically StaticMesh) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Config")
	FSuspenseCoreMedicalVisualConfig MedkitConfig;

	/** Configuration for injectors/syringes (typically SkeletalMesh with animation) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Config")
	FSuspenseCoreMedicalVisualConfig InjectorConfig;

	/** Configuration for splints (typically StaticMesh) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Config")
	FSuspenseCoreMedicalVisualConfig SplintConfig;

	/** Configuration for surgical kits (typically SkeletalMesh) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Config")
	FSuspenseCoreMedicalVisualConfig SurgicalConfig;

	/** Configuration for pills/painkillers (typically StaticMesh) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Config")
	FSuspenseCoreMedicalVisualConfig PillsConfig;

	/** Fallback configuration if no specific config found */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Config")
	FSuspenseCoreMedicalVisualConfig GenericConfig;

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
	 * Uses default config for the type
	 *
	 * @param MedicalTypeTag Tag like "Item.Medical.Medkit"
	 */
	UFUNCTION(BlueprintCallable, Category = "Medical")
	void InitializeFromTypeTag(FGameplayTag MedicalTypeTag);

	/**
	 * Set static mesh directly
	 *
	 * @param NewMesh Static mesh to use
	 */
	UFUNCTION(BlueprintCallable, Category = "Medical")
	void SetStaticMesh(UStaticMesh* NewMesh);

	/**
	 * Set skeletal mesh directly
	 *
	 * @param NewMesh Skeletal mesh to use
	 * @param OptionalAnim Optional animation to play
	 */
	UFUNCTION(BlueprintCallable, Category = "Medical")
	void SetSkeletalMesh(USkeletalMesh* NewMesh, UAnimationAsset* OptionalAnim = nullptr);

	//==================================================================
	// Runtime Accessors
	//==================================================================

	/** Get current medical item ID */
	UFUNCTION(BlueprintPure, Category = "Medical")
	FName GetMedicalItemID() const { return MedicalItemID; }

	/** Get current visual type */
	UFUNCTION(BlueprintPure, Category = "Medical")
	ESuspenseCoreMedicalVisualType GetVisualType() const { return VisualType; }

	/** Get current mesh type being used */
	UFUNCTION(BlueprintPure, Category = "Medical")
	ESuspenseCoreMedicalMeshType GetActiveMeshType() const { return ActiveMeshType; }

	/** Get transform offset for current type */
	UFUNCTION(BlueprintPure, Category = "Medical")
	FTransform GetAttachOffset() const;

	/** Check if using skeletal mesh */
	UFUNCTION(BlueprintPure, Category = "Medical")
	bool IsUsingSkeletal() const { return ActiveMeshType == ESuspenseCoreMedicalMeshType::SkeletalMesh; }

	//==================================================================
	// Visual State
	//==================================================================

	/**
	 * Set visibility
	 */
	UFUNCTION(BlueprintCallable, Category = "Medical")
	void SetVisibility(bool bVisible);

	/**
	 * Play animation on skeletal mesh (if using skeletal)
	 */
	UFUNCTION(BlueprintCallable, Category = "Medical")
	void PlayAnimation(UAnimationAsset* Animation, bool bLooping = true);

	/**
	 * Stop any playing animation
	 */
	UFUNCTION(BlueprintCallable, Category = "Medical")
	void StopAnimation();

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
	 * Get config for visual type
	 */
	const FSuspenseCoreMedicalVisualConfig& GetConfigForType(ESuspenseCoreMedicalVisualType Type) const;

	/**
	 * Apply config to components
	 * Shows correct component and sets mesh
	 */
	void ApplyConfig(const FSuspenseCoreMedicalVisualConfig& Config);

	/**
	 * Update component visibility based on active mesh type
	 */
	void UpdateComponentVisibility();

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

	/** Which mesh type is currently active */
	UPROPERTY()
	ESuspenseCoreMedicalMeshType ActiveMeshType = ESuspenseCoreMedicalMeshType::StaticMesh;

	/** Cached current config */
	FSuspenseCoreMedicalVisualConfig CurrentConfig;

	/** Cached DataManager reference */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreDataManager> DataManager;
};
