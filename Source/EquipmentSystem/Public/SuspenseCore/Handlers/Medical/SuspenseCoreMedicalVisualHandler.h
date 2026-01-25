// SuspenseCoreMedicalVisualHandler.h
// Handles visual medical item spawning and attachment
// Copyright Suspense Team. All Rights Reserved.
//
// PURPOSE:
// Manages visual representation of medical items during equip/use animations.
// Spawns ASuspenseCoreMedicalItemActor and attaches to character's hand.
//
// ARCHITECTURE:
// - Subscribes to EventBus medical events (Equipped, Unequipped)
// - Spawns/pools visual actors for performance
// - Attaches to character's weapon_r socket (same as grenades)
//
// FLOW:
// 1. GA_MedicalEquip broadcasts Event.Medical.Equipped
// 2. This handler receives event via EventBus
// 3. SpawnVisualMedical() creates/reuses actor from pool
// 4. AttachToCharacterHand() attaches to socket
// 5. GA_MedicalEquip ends → Event.Medical.Unequipped
// 6. DestroyVisualMedical() recycles to pool
//
// @see ASuspenseCoreMedicalItemActor
// @see USuspenseCoreGrenadeHandler (reference implementation)
// @see GA_MedicalEquip, GA_MedicalUse

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreMedicalVisualHandler.generated.h"

// Forward declarations
class ASuspenseCoreMedicalItemActor;
class USuspenseCoreDataManager;
class USuspenseCoreEventBus;

/**
 * USuspenseCoreMedicalVisualHandler
 *
 * Handles visual medical item actors during equip/use animations.
 * Similar pattern to GrenadeHandler for visual grenade management.
 *
 * FEATURES:
 * - Object pooling for spawn performance
 * - Automatic EventBus subscription
 * - Socket attachment with offset support
 * - Hide/show for weapon visibility coordination
 *
 * @see ASuspenseCoreMedicalItemActor
 */
UCLASS(BlueprintType, Blueprintable)
class EQUIPMENTSYSTEM_API USuspenseCoreMedicalVisualHandler : public UObject
{
	GENERATED_BODY()

public:
	USuspenseCoreMedicalVisualHandler();
	virtual ~USuspenseCoreMedicalVisualHandler();

	//==================================================================
	// Initialization
	//==================================================================

	/**
	 * Initialize handler with dependencies
	 * Sets up EventBus subscriptions
	 *
	 * @param InDataManager DataManager for SSOT lookup
	 * @param InEventBus EventBus for event subscription
	 */
	UFUNCTION(BlueprintCallable, Category = "Medical|Visual")
	void Initialize(USuspenseCoreDataManager* InDataManager, USuspenseCoreEventBus* InEventBus);

	/**
	 * Cleanup handler
	 * Unsubscribes from events and destroys pooled actors
	 */
	UFUNCTION(BlueprintCallable, Category = "Medical|Visual")
	void Shutdown();

	//==================================================================
	// Configuration
	//==================================================================

	/** Blueprint class for medical item actors */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Config")
	TSubclassOf<ASuspenseCoreMedicalItemActor> MedicalItemActorClass;

	/** Socket name to attach to (default: weapon_r) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Config")
	FName AttachSocketName = FName("weapon_r");

	/** Alternative socket names if primary not found */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Config")
	TArray<FName> AlternativeSocketNames;

	/** Maximum actors to keep in pool */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Medical|Config")
	int32 MaxPoolSize = 5;

	//==================================================================
	// Visual Management
	//==================================================================

	/**
	 * Spawn visual medical item for character
	 *
	 * @param Character Character to attach to
	 * @param MedicalItemID Item ID for mesh lookup
	 * @return true if spawn successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Medical|Visual")
	bool SpawnVisualMedical(AActor* Character, FName MedicalItemID);

	/**
	 * Destroy/hide visual medical item for character
	 *
	 * @param Character Character to remove visual from
	 */
	UFUNCTION(BlueprintCallable, Category = "Medical|Visual")
	void DestroyVisualMedical(AActor* Character);

	/**
	 * Hide visual without destroying (for use phase transitions)
	 *
	 * @param Character Character to hide visual for
	 */
	UFUNCTION(BlueprintCallable, Category = "Medical|Visual")
	void HideVisualMedical(AActor* Character);

	/**
	 * Show previously hidden visual
	 *
	 * @param Character Character to show visual for
	 */
	UFUNCTION(BlueprintCallable, Category = "Medical|Visual")
	void ShowVisualMedical(AActor* Character);

	/**
	 * Get current visual actor for character
	 *
	 * @param Character Character to query
	 * @return Visual actor or nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "Medical|Visual")
	ASuspenseCoreMedicalItemActor* GetVisualMedical(AActor* Character) const;

protected:
	//==================================================================
	// EventBus Handlers
	//==================================================================

	/**
	 * Called when medical item is equipped
	 * Spawns visual and attaches to character
	 */
	void OnMedicalEquipped(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/**
	 * Called when medical item is unequipped
	 * Destroys/recycles visual actor
	 */
	void OnMedicalUnequipped(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	//==================================================================
	// Internal Methods
	//==================================================================

	/**
	 * Get or spawn actor from pool
	 */
	ASuspenseCoreMedicalItemActor* AcquireFromPool();

	/**
	 * Return actor to pool
	 */
	void ReleaseToPool(ASuspenseCoreMedicalItemActor* Actor);

	/**
	 * Attach actor to character's hand socket
	 *
	 * @param Actor Actor to attach
	 * @param Character Character to attach to
	 * @return true if attachment successful
	 */
	bool AttachToCharacterHand(ASuspenseCoreMedicalItemActor* Actor, AActor* Character);

	/**
	 * Find suitable skeletal mesh and socket
	 */
	USkeletalMeshComponent* FindAttachmentTarget(
		AActor* Character,
		FName& OutSocketName) const;

private:
	//==================================================================
	// Dependencies
	//==================================================================

	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreDataManager> DataManager;

	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> EventBus;

	//==================================================================
	// Active Visuals (Character -> Actor mapping)
	//==================================================================

	UPROPERTY()
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<ASuspenseCoreMedicalItemActor>> ActiveVisuals;

	//==================================================================
	// Object Pool
	//==================================================================

	UPROPERTY()
	TArray<TWeakObjectPtr<ASuspenseCoreMedicalItemActor>> ActorPool;

	//==================================================================
	// EventBus Subscription Handles
	//==================================================================

	FSuspenseCoreSubscriptionHandle EquippedSubscriptionHandle;
	FSuspenseCoreSubscriptionHandle UnequippedSubscriptionHandle;

	//==================================================================
	// State
	//==================================================================

	bool bInitialized = false;
};
