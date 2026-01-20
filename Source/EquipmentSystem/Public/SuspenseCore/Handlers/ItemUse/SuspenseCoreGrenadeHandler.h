// SuspenseCoreGrenadeHandler.h
// Handler for grenade prepare and throw
// Copyright Suspense Team. All Rights Reserved.
//
// PURPOSE:
// Handles grenade preparation and throwing operations.
// Two-phase operation: Prepare (hold) → Throw (release)
//
// FLOW (Tarkov-style two-phase):
// PHASE 1 - EQUIP (QuickSlot press):
// 1. User presses QuickSlot (4-7) with grenade
// 2. ItemUseService routes to this handler (Context = QuickSlot)
// 3. Handler activates GA_GrenadeEquip ability
// 4. GA_GrenadeEquip plays draw montage, grants State.GrenadeEquipped tag
// 5. Grenade is now in hand, ready for throw
//
// PHASE 2 - THROW (Fire/LMB press while equipped):
// 1. User presses Fire (LMB) with grenade equipped
// 2. Input system detects State.GrenadeEquipped and routes to ItemUseService
//    (Context = Hotkey, the Fire input is redirected when grenade is equipped)
// 3. Handler checks State.GrenadeEquipped tag
// 4. Handler activates GA_GrenadeThrow ability
// 5. GA_GrenadeThrow plays throw animation, spawns grenade
//
// ARCHITECTURE:
// - Duration-based prepare phase
// - Cancellable (damage interrupts, puts pin back)
// - Spawns grenade actor on completion

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SuspenseCore/Interfaces/ItemUse/ISuspenseCoreItemUseHandler.h"
#include "SuspenseCore/Tags/SuspenseCoreItemUseNativeTags.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreGrenadeHandler.generated.h"

// Forward declarations
class USuspenseCoreDataManager;
class USuspenseCoreEventBus;

/**
 * Grenade type for different behaviors
 */
UENUM(BlueprintType)
enum class ESuspenseCoreGrenadeType : uint8
{
	Fragmentation   UMETA(DisplayName = "Fragmentation"),
	Smoke           UMETA(DisplayName = "Smoke"),
	Flashbang       UMETA(DisplayName = "Flashbang"),
	Incendiary      UMETA(DisplayName = "Incendiary"),
	Impact          UMETA(DisplayName = "Impact")
};

/**
 * USuspenseCoreGrenadeHandler
 *
 * Handler for Tarkov-style grenade equip and throw operations.
 * Implements two-phase grenade flow: Equip → Throw
 *
 * TARKOV-STYLE FLOW:
 * ┌─────────────────────────────────────────────────────────────────┐
 * │  [QuickSlot Press]  →  GA_GrenadeEquip  →  State.GrenadeEquipped │
 * │         ↓                                                        │
 * │  [Fire Press]       →  GA_GrenadeThrow  →  Spawn Projectile     │
 * └─────────────────────────────────────────────────────────────────┘
 *
 * SUPPORTED CONTEXTS:
 * - QuickSlot: Equip grenade (activate GA_GrenadeEquip)
 * - Hotkey: Throw grenade (checks State.GrenadeEquipped first, Fire input routed here)
 * - Programmatic: AI/script-triggered throw
 *
 * ABILITIES USED:
 * - GA_GrenadeEquip: Draw montage, grants State.GrenadeEquipped
 * - GA_GrenadeThrow: Pin pull, cooking, throw montage, spawn
 *
 * REQUIREMENTS:
 * - Source: Item with Item.Category.Grenade tag
 * - Player not stunned/dead
 *
 * @see ISuspenseCoreItemUseHandler
 * @see USuspenseCoreGrenadeThrowAbility
 */
UCLASS(BlueprintType)
class EQUIPMENTSYSTEM_API USuspenseCoreGrenadeHandler : public UObject, public ISuspenseCoreItemUseHandler
{
	GENERATED_BODY()

public:
	USuspenseCoreGrenadeHandler();

	//==================================================================
	// Initialization
	//==================================================================

	/**
	 * Initialize handler with dependencies
	 *
	 * @param InDataManager Data manager for grenade data
	 * @param InEventBus EventBus for publishing events
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemUse|Handler")
	void Initialize(USuspenseCoreDataManager* InDataManager, USuspenseCoreEventBus* InEventBus);

	/**
	 * Shutdown handler and unsubscribe from EventBus
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemUse|Handler")
	void Shutdown();

	//==================================================================
	// ISuspenseCoreItemUseHandler - Identity
	//==================================================================

	virtual FGameplayTag GetHandlerTag() const override;
	virtual ESuspenseCoreHandlerPriority GetPriority() const override;
	virtual FText GetDisplayName() const override;

	//==================================================================
	// ISuspenseCoreItemUseHandler - Supported Types
	//==================================================================

	virtual FGameplayTagContainer GetSupportedSourceTags() const override;
	virtual TArray<ESuspenseCoreItemUseContext> GetSupportedContexts() const override;

	//==================================================================
	// ISuspenseCoreItemUseHandler - Validation
	//==================================================================

	virtual bool CanHandle(const FSuspenseCoreItemUseRequest& Request) const override;
	virtual bool ValidateRequest(
		const FSuspenseCoreItemUseRequest& Request,
		FSuspenseCoreItemUseResponse& OutResponse) const override;

	//==================================================================
	// ISuspenseCoreItemUseHandler - Execution
	//==================================================================

	virtual FSuspenseCoreItemUseResponse Execute(
		const FSuspenseCoreItemUseRequest& Request,
		AActor* OwnerActor) override;

	virtual float GetDuration(const FSuspenseCoreItemUseRequest& Request) const override;
	virtual float GetCooldown(const FSuspenseCoreItemUseRequest& Request) const override;

	virtual bool CancelOperation(const FGuid& RequestID) override;
	virtual bool IsCancellable() const override { return true; }

	virtual FSuspenseCoreItemUseResponse OnOperationComplete(
		const FSuspenseCoreItemUseRequest& Request,
		AActor* OwnerActor) override;

protected:
	//==================================================================
	// Internal Methods - Ability Activation
	//==================================================================

	/**
	 * Activate GA_GrenadeEquip ability to equip grenade
	 * Called when Context == QuickSlot
	 *
	 * @param Request Original use request
	 * @param OwnerActor Actor equipping grenade
	 * @return Response indicating success/failure
	 */
	FSuspenseCoreItemUseResponse ExecuteEquip(
		const FSuspenseCoreItemUseRequest& Request,
		AActor* OwnerActor);

	/**
	 * Activate GA_GrenadeThrow ability to throw equipped grenade
	 * Called when Context == Fire AND State.GrenadeEquipped is present
	 *
	 * @param Request Original use request
	 * @param OwnerActor Actor throwing grenade
	 * @return Response indicating success/failure
	 */
	FSuspenseCoreItemUseResponse ExecuteThrow(
		const FSuspenseCoreItemUseRequest& Request,
		AActor* OwnerActor);

	/**
	 * Check if actor has State.GrenadeEquipped tag
	 *
	 * @param Actor Actor to check
	 * @return true if grenade is currently equipped
	 */
	bool IsGrenadeEquipped(AActor* Actor) const;

	//==================================================================
	// Internal Methods - Grenade Data
	//==================================================================

	/**
	 * Get grenade type from item tags
	 *
	 * @param ItemTags Item's gameplay tags
	 * @return Grenade type
	 */
	ESuspenseCoreGrenadeType GetGrenadeType(const FGameplayTagContainer& ItemTags) const;

	/**
	 * Get prepare duration for grenade type
	 *
	 * @param Type Grenade type
	 * @return Duration in seconds
	 */
	float GetPrepareDuration(ESuspenseCoreGrenadeType Type) const;

	/**
	 * Spawn and throw grenade
	 *
	 * @param Request The use request
	 * @param OwnerActor Actor throwing the grenade
	 * @return true if grenade was thrown
	 */
	bool ThrowGrenade(
		const FSuspenseCoreItemUseRequest& Request,
		AActor* OwnerActor);

	/**
	 * Publish grenade event
	 */
	void PublishGrenadeEvent(
		const FSuspenseCoreItemUseRequest& Request,
		const FSuspenseCoreItemUseResponse& Response,
		AActor* OwnerActor);

private:
	//==================================================================
	// EventBus Callbacks
	//==================================================================

	/**
	 * Called when GrenadeThrowAbility requests grenade spawn via EventBus
	 * Extracts spawn parameters from event data and calls ThrowGrenadeFromEvent()
	 */
	void OnSpawnRequested(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/**
	 * Called when grenade is equipped (GA_GrenadeEquip broadcasts Event.Throwable.Equipped)
	 * Spawns visual grenade and attaches to character's hand
	 */
	void OnGrenadeEquipped(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/**
	 * Called when grenade is unequipped (GA_GrenadeEquip broadcasts Event.Throwable.Unequipped)
	 * Destroys the visual grenade
	 */
	void OnGrenadeUnequipped(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/**
	 * Spawn and attach visual grenade to character hand
	 */
	bool SpawnVisualGrenade(AActor* Character, FName GrenadeID);

	/**
	 * Destroy visual grenade for character
	 */
	void DestroyVisualGrenade(AActor* Character);

	/**
	 * Spawn grenade from EventBus SpawnRequested event
	 * Uses parameters from GrenadeThrowAbility (location, direction, force, cook time)
	 */
	bool ThrowGrenadeFromEvent(
		AActor* OwnerActor,
		FName GrenadeID,
		const FVector& ThrowLocation,
		const FVector& ThrowDirection,
		float ThrowForce,
		float CookTime);

	//==================================================================
	// Dependencies
	//==================================================================

	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreDataManager> DataManager;

	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> EventBus;

	//==================================================================
	// EventBus Subscription
	//==================================================================

	/** Handle for SpawnRequested event subscription */
	FSuspenseCoreSubscriptionHandle SpawnRequestedHandle;

	/** Handle for grenade equipped event subscription */
	FSuspenseCoreSubscriptionHandle EquippedHandle;

	/** Handle for grenade unequipped event subscription */
	FSuspenseCoreSubscriptionHandle UnequippedHandle;

	//==================================================================
	// Visual Grenade Tracking
	//==================================================================

	/** Currently spawned visual grenade actors per character */
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<AActor>> VisualGrenades;

	//==================================================================
	// Configuration
	//==================================================================

	/** Time to prepare grenade (pull pin) */
	UPROPERTY(EditDefaultsOnly, Category = "Handler|Config")
	float PrepareDuration = 0.5f;

	/** Cooldown after throw (seconds) */
	UPROPERTY(EditDefaultsOnly, Category = "Handler|Config")
	float ThrowCooldown = 1.0f;

	/** Default throw force */
	UPROPERTY(EditDefaultsOnly, Category = "Handler|Config")
	float DefaultThrowForce = 1500.0f;
};
