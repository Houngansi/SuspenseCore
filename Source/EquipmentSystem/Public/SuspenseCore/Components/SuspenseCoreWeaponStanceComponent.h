// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreWeaponStanceComponent.generated.h"

// Forward declarations
class USuspenseWeaponAnimation;
class ISuspenseWeaponAnimation;

/**
 * SuspenseCore Weapon Stance Component - manages weapon stance and animations
 *
 * ARCHITECTURE:
 * - Integrates with attachment component for weapon pose management
 * - Uses AnimationInterface for accessing animation data
 * - Coordinates stance state with equipment state machine
 * - Manages weapon drawn/holstered states
 */
UCLASS(ClassGroup=(SuspenseCore), meta=(BlueprintSpawnableComponent), BlueprintType, Blueprintable)
class EQUIPMENTSYSTEM_API USuspenseCoreWeaponStanceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USuspenseCoreWeaponStanceComponent();

	//~UObject
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -------- API for USuspenseCoreEquipmentAttachmentComponent --------

	/**
	 * Called when equipment changes
	 * @param NewEquipmentActor New equipment actor
	 */
	UFUNCTION(BlueprintCallable, Category="SuspenseCore|Weapon|Stance")
	void OnEquipmentChanged(AActor* NewEquipmentActor);

	/**
	 * Set weapon stance based on weapon type
	 * @param WeaponTypeTag Weapon type tag
	 * @param bImmediate Whether to change immediately or animate
	 */
	UFUNCTION(BlueprintCallable, Category="SuspenseCore|Weapon|Stance")
	void SetWeaponStance(const FGameplayTag& WeaponTypeTag, bool bImmediate = false);

	/**
	 * Clear current weapon stance
	 * @param bImmediate Whether to clear immediately or animate
	 */
	UFUNCTION(BlueprintCallable, Category="SuspenseCore|Weapon|Stance")
	void ClearWeaponStance(bool bImmediate = false);

	/**
	 * Set weapon drawn state
	 * @param bDrawn Whether weapon is drawn
	 */
	UFUNCTION(BlueprintCallable, Category="SuspenseCore|Weapon|Stance")
	void SetWeaponDrawnState(bool bDrawn);

	/**
	 * Get animation interface for current weapon
	 * @return Animation interface or nullptr
	 */
	UFUNCTION(BlueprintCallable, Category="SuspenseCore|Weapon|Stance")
	TScriptInterface<ISuspenseWeaponAnimation> GetAnimationInterface() const;

	// -------- State Queries --------

	/**
	 * Get current weapon type
	 * @return Current weapon type tag
	 */
	UFUNCTION(BlueprintPure, Category="SuspenseCore|Weapon|Stance")
	FGameplayTag GetCurrentWeaponType() const { return CurrentWeaponType; }

	/**
	 * Check if weapon is drawn
	 * @return True if weapon is drawn
	 */
	UFUNCTION(BlueprintPure, Category="SuspenseCore|Weapon|Stance")
	bool IsWeaponDrawn() const { return bWeaponDrawn; }

	/**
	 * Get tracked equipment actor
	 * @return Equipment actor or nullptr
	 */
	UFUNCTION(BlueprintPure, Category="SuspenseCore|Weapon|Stance")
	AActor* GetTrackedEquipmentActor() const { return TrackedEquipmentActor.Get(); }

protected:
	// Replication callbacks
	UFUNCTION()
	void OnRep_WeaponType();

	UFUNCTION()
	void OnRep_DrawnState();

	/**
	 * Push current stance to animation layer
	 * @param bSkipIfNoInterface Skip if no interface available
	 */
	void PushToAnimationLayer(bool bSkipIfNoInterface = true) const;

private:
	//================================================
	// Replicated Stance Data
	//================================================

	/** Current weapon type for stance */
	UPROPERTY(ReplicatedUsing=OnRep_WeaponType)
	FGameplayTag CurrentWeaponType;

	/** Whether weapon is drawn */
	UPROPERTY(ReplicatedUsing=OnRep_DrawnState)
	bool bWeaponDrawn = false;

	/** Tracked equipment actor (owner or spawned), weak reference */
	UPROPERTY()
	TWeakObjectPtr<AActor> TrackedEquipmentActor;

	/** Cached animation interface */
	UPROPERTY(Transient)
	mutable TScriptInterface<ISuspenseWeaponAnimation> CachedAnimationInterface;

	//================================================
	// Cache Settings
	//================================================

	/** Animation interface cache lifetime */
	UPROPERTY(EditDefaultsOnly, Category="SuspenseCore|Weapon|Stance")
	float AnimationInterfaceCacheLifetime = 0.25f;

	/** Last animation interface cache time */
	mutable float LastAnimationInterfaceCacheTime = -1000.0f;
};
