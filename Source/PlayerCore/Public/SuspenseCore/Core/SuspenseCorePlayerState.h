// SuspenseCorePlayerState.h
// Copyright Suspense Team. All Rights Reserved.
// Clean Architecture PlayerState with EventBus integration

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCorePlayerState.generated.h"

class USuspenseCoreAbilitySystemComponent;
class USuspenseCoreAttributeSet;
class USuspenseCoreEventBus;
class USuspenseCoreInventoryComponent;
class UGameplayAbility;
class UGameplayEffect;

// Equipment module forward declarations
class USuspenseEquipmentDataStore;
class USuspenseEquipmentTransactionProcessor;
class USuspenseEquipmentReplicationManager;
class USuspenseEquipmentPredictionSystem;
class USuspenseWeaponStateManager;
class USuspenseEquipmentNetworkDispatcher;
class USuspenseEquipmentInventoryBridge;
class USuspenseEquipmentEventDispatcher;
class USuspenseEquipmentOperationExecutor;
class USuspenseEquipmentSlotValidator;
class USuspenseLoadoutManager;

// ═══════════════════════════════════════════════════════════════════════════════
// DELEGATES (Internal use only - EventBus preferred for external communication)
// ═══════════════════════════════════════════════════════════════════════════════

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSuspenseCoreOnAttributeChanged, FGameplayTag, AttributeTag, float, NewValue, float, OldValue);

// ═══════════════════════════════════════════════════════════════════════════════
// ABILITY CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════════

USTRUCT(BlueprintType)
struct FSuspenseCoreAbilityEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UGameplayAbility> AbilityClass = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag InputTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 AbilityLevel = 1;
};

// ═══════════════════════════════════════════════════════════════════════════════
// PLAYER STATE
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Clean Architecture PlayerState with integrated GAS and EventBus
 *
 * DESIGN PRINCIPLES:
 * - EventBus for all external communication (no direct delegates)
 * - Service Locator for dependency injection
 * - Repository pattern for player data persistence
 * - Minimal coupling with other systems
 *
 * RESPONSIBILITIES:
 * - Owns AbilitySystemComponent (ASC lives here, not on Character)
 * - Manages attributes through SuspenseCoreAttributeSet
 * - Broadcasts state changes via EventBus
 * - Persists across pawn respawns
 */
UCLASS()
class PLAYERCORE_API ASuspenseCorePlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ASuspenseCorePlayerState();

	// ═══════════════════════════════════════════════════════════════════════════════
	// ACTOR LIFECYCLE
	// ═══════════════════════════════════════════════════════════════════════════════

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ═══════════════════════════════════════════════════════════════════════════════
	// IABILITYSYSTEMINTERFACE
	// ═══════════════════════════════════════════════════════════════════════════════

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - GAS
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Get the SuspenseCore ASC (typed) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|GAS")
	USuspenseCoreAbilitySystemComponent* GetSuspenseCoreASC() const { return AbilitySystemComponent; }

	/** Get the attribute set */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|GAS")
	USuspenseCoreAttributeSet* GetSuspenseCoreAttributes() const { return AttributeSet; }

	/** Grant an ability to this player */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|GAS")
	bool GrantAbility(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level = 1);

	/** Remove an ability from this player */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|GAS")
	bool RemoveAbility(TSubclassOf<UGameplayAbility> AbilityClass);

	/** Apply a gameplay effect to this player */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|GAS")
	bool ApplyEffect(TSubclassOf<UGameplayEffect> EffectClass, float Level = 1.0f);

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - INVENTORY
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Get the inventory component */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory")
	USuspenseCoreInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - EQUIPMENT
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Get the equipment data store (core equipment state) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment")
	USuspenseEquipmentDataStore* GetEquipmentDataStore() const { return EquipmentDataStore; }

	/** Get the equipment transaction processor (atomic operations) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment")
	USuspenseEquipmentTransactionProcessor* GetEquipmentTxnProcessor() const { return EquipmentTxnProcessor; }

	/** Get the equipment operation executor (validated operations) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment")
	USuspenseEquipmentOperationExecutor* GetEquipmentOps() const { return EquipmentOps; }

	/** Get the equipment prediction system (client-side prediction) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment")
	USuspenseEquipmentPredictionSystem* GetEquipmentPrediction() const { return EquipmentPrediction; }

	/** Get the equipment inventory bridge (connects equipment to inventory) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment")
	USuspenseEquipmentInventoryBridge* GetEquipmentInventoryBridge() const { return EquipmentInventoryBridge; }

	/** Get the weapon state manager (weapon FSM) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment")
	USuspenseWeaponStateManager* GetWeaponStateManager() const { return WeaponStateManager; }

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - STATE
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Check if player is alive */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|State")
	bool IsAlive() const;

	/** Get player level */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|State")
	int32 GetPlayerLevel() const { return PlayerLevel; }

	/** Set player level */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|State")
	void SetPlayerLevel(int32 NewLevel);

	/** Get team ID */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|State")
	int32 GetTeamId() const { return TeamId; }

	/** Set team ID */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|State")
	void SetTeamId(int32 NewTeamId);

	/** Get character class ID */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|State")
	FName GetCharacterClassId() const { return CharacterClassId; }

	/**
	 * Apply a character class to this player.
	 * Loads class data from CharacterClassSubsystem and applies modifiers.
	 * @param ClassId The class identifier (e.g., "Assault", "Medic", "Sniper")
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|State")
	bool ApplyCharacterClass(FName ClassId);

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - ATTRIBUTES (Convenience Wrappers)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Get current health */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Attributes")
	float GetHealth() const;

	/** Get max health */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Attributes")
	float GetMaxHealth() const;

	/** Get health percentage (0-1) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Attributes")
	float GetHealthPercent() const;

	/** Get current stamina */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Attributes")
	float GetStamina() const;

	/** Get max stamina */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Attributes")
	float GetMaxStamina() const;

	/** Get stamina percentage (0-1) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Attributes")
	float GetStaminaPercent() const;

	// ═══════════════════════════════════════════════════════════════════════════════
	// EVENTS
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Called when any attribute changes (for internal use, prefer EventBus) */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Events")
	FSuspenseCoreOnAttributeChanged OnAttributeChanged;

protected:
	// ═══════════════════════════════════════════════════════════════════════════════
	// COMPONENTS
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Ability System Component - created in constructor */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "SuspenseCore|Components")
	USuspenseCoreAbilitySystemComponent* AbilitySystemComponent = nullptr;

	/** Inventory Component - created in constructor, persists across respawns */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "SuspenseCore|Components")
	USuspenseCoreInventoryComponent* InventoryComponent = nullptr;

	/** Attribute Set - spawned by ASC */
	UPROPERTY()
	USuspenseCoreAttributeSet* AttributeSet = nullptr;

	// ═══════════════════════════════════════════════════════════════════════════════
	// EQUIPMENT MODULE COMPONENTS (Per-Player)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Core data store for equipment state (Server authoritative, replicated) */
	UPROPERTY(VisibleAnywhere, Replicated, Category = "SuspenseCore|Equipment|Core")
	USuspenseEquipmentDataStore* EquipmentDataStore = nullptr;

	/** Transaction processor for atomic equipment changes */
	UPROPERTY(VisibleAnywhere, Replicated, Category = "SuspenseCore|Equipment|Core")
	USuspenseEquipmentTransactionProcessor* EquipmentTxnProcessor = nullptr;

	/** Operation executor (deterministic, validated) */
	UPROPERTY(VisibleAnywhere, Replicated, Category = "SuspenseCore|Equipment|Core")
	USuspenseEquipmentOperationExecutor* EquipmentOps = nullptr;

	/** Prediction system (client owning) */
	UPROPERTY(VisibleAnywhere, Replicated, Category = "SuspenseCore|Equipment|Networking")
	USuspenseEquipmentPredictionSystem* EquipmentPrediction = nullptr;

	/** Replication manager (delta-based replication) */
	UPROPERTY(VisibleAnywhere, Replicated, Category = "SuspenseCore|Equipment|Networking")
	USuspenseEquipmentReplicationManager* EquipmentReplication = nullptr;

	/** Network dispatcher (RPC/request queue) */
	UPROPERTY(VisibleAnywhere, Replicated, Category = "SuspenseCore|Equipment|Networking")
	USuspenseEquipmentNetworkDispatcher* EquipmentNetworkDispatcher = nullptr;

	/** Event dispatcher / equipment event bus (local) */
	UPROPERTY(VisibleAnywhere, Replicated, Category = "SuspenseCore|Equipment|Events")
	USuspenseEquipmentEventDispatcher* EquipmentEventDispatcher = nullptr;

	/** Weapon state manager (FSM) */
	UPROPERTY(VisibleAnywhere, Replicated, Category = "SuspenseCore|Equipment|Weapon")
	USuspenseWeaponStateManager* WeaponStateManager = nullptr;

	/** Inventory bridge (connects equipment to existing inventory) */
	UPROPERTY(VisibleAnywhere, Replicated, Category = "SuspenseCore|Equipment|Inventory")
	USuspenseEquipmentInventoryBridge* EquipmentInventoryBridge = nullptr;

	/** Slot validator (UObject, not component) - created during WireEquipmentModule() */
	UPROPERTY()
	USuspenseEquipmentSlotValidator* EquipmentSlotValidator = nullptr;

	// ═══════════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Attribute set class to spawn */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config|GAS")
	TSubclassOf<USuspenseCoreAttributeSet> AttributeSetClass;

	/** Effect to apply for initial attribute values */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config|GAS")
	TSubclassOf<UGameplayEffect> InitialAttributesEffect;

	/** Abilities to grant on startup */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config|GAS")
	TArray<FSuspenseCoreAbilityEntry> StartupAbilities;

	/** Passive effects to apply on startup (regen, etc.) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config|GAS")
	TArray<TSubclassOf<UGameplayEffect>> PassiveEffects;

	// ═══════════════════════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Player level for scaling */
	UPROPERTY(ReplicatedUsing = OnRep_PlayerLevel, BlueprintReadOnly, Category = "SuspenseCore|State")
	int32 PlayerLevel = 1;

	/** Team identifier */
	UPROPERTY(ReplicatedUsing = OnRep_TeamId, BlueprintReadOnly, Category = "SuspenseCore|State")
	int32 TeamId = 0;

	/** Character class ID (Assault, Medic, Sniper, etc.) */
	UPROPERTY(ReplicatedUsing = OnRep_CharacterClassId, BlueprintReadOnly, Category = "SuspenseCore|State")
	FName CharacterClassId;

	// ═══════════════════════════════════════════════════════════════════════════════
	// REPLICATION CALLBACKS
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Called when PlayerLevel replicates to clients */
	UFUNCTION()
	void OnRep_PlayerLevel(int32 OldPlayerLevel);

	/** Called when TeamId replicates to clients */
	UFUNCTION()
	void OnRep_TeamId(int32 OldTeamId);

	/** Called when CharacterClassId replicates to clients */
	UFUNCTION()
	void OnRep_CharacterClassId(FName OldClassId);

	// ═══════════════════════════════════════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Initialize GAS components */
	void InitializeAbilitySystem();

	/** Grant startup abilities */
	void GrantStartupAbilities();

	/** Apply initial attributes and passive effects */
	void ApplyInitialEffects();

	/** Setup attribute change callbacks */
	void SetupAttributeCallbacks();

	/** Cleanup attribute callbacks */
	void CleanupAttributeCallbacks();

	/** Publish state to EventBus */
	void PublishPlayerStateEvent(const FGameplayTag& EventTag, const FString& Payload = TEXT(""));

	/** Handle attribute change and forward to EventBus */
	void HandleAttributeChange(const FGameplayTag& AttributeTag, float NewValue, float OldValue);

	// ═══════════════════════════════════════════════════════════════════════════════
	// EQUIPMENT MODULE WIRING
	// ═══════════════════════════════════════════════════════════════════════════════

	/**
	 * Wire up per-player equipment components with global services.
	 * CRITICAL: Called AFTER loadout application to ensure slots are configured.
	 * @param LoadoutManager - Manager containing loadout configuration (can be null for defaults)
	 * @param AppliedLoadoutID - ID of loadout that was just applied (can be NAME_None)
	 * @return true if wiring succeeded, false otherwise
	 */
	bool WireEquipmentModule(USuspenseLoadoutManager* LoadoutManager = nullptr, const FName& AppliedLoadoutID = NAME_None);

	/**
	 * Attempt to wire equipment module once.
	 * Checks if global services are ready and wires if so.
	 * @return true if successfully wired, false if needs retry
	 */
	bool TryWireEquipmentModuleOnce();

	/** Initialize all equipment module components */
	void InitializeEquipmentComponents();

	/** Initialize inventory component from loadout configuration */
	void InitializeInventoryFromLoadout();

	// ═══════════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════════

	/**
	 * Default loadout ID for this player.
	 * References row in SuspenseCoreDataManager's LoadoutDataTable.
	 * LoadoutDataTable row (FSuspenseCoreTemplateLoadout) contains:
	 * - InventoryWidth, InventoryHeight, MaxWeight for inventory config
	 * - Equipment slots configuration
	 * - Initial items
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Loadout")
	FName DefaultLoadoutID = TEXT("Default_Soldier");

private:
	/** Cached EventBus reference (mutable for const getter caching) */
	UPROPERTY()
	mutable TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** Get or find EventBus */
	USuspenseCoreEventBus* GetEventBus() const;

	/** Attribute callback handles */
	TArray<FDelegateHandle> AttributeCallbackHandles;

	/** Flag for initialization state */
	bool bAbilitySystemInitialized = false;

	/** Flag for equipment module initialization state */
	bool bEquipmentModuleInitialized = false;

	// ═══════════════════════════════════════════════════════════════════════════════
	// EQUIPMENT WIRING RETRY (Server-side)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Current retry count for equipment module wiring */
	int32 EquipmentWireRetryCount = 0;

	/** Timer handle for retry */
	FTimerHandle EquipmentWireRetryHandle;

	/** Maximum retry attempts (20 × 50ms = 1 second) */
	static constexpr int32 MaxEquipmentWireRetries = 20;

	/** Interval between retry attempts (50 milliseconds) */
	static constexpr float EquipmentWireRetryInterval = 0.05f;
};
