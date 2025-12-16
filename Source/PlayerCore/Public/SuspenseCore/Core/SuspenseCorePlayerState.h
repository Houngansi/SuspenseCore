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
class UGameplayAbility;
class UGameplayEffect;

// ═══════════════════════════════════════════════════════════════════════════════
// OPTIONAL MODULE FORWARD DECLARATIONS
// Note: These classes may not exist if modules are disabled.
// We use UActorComponent* base types for UPROPERTY to satisfy UHT.
// ═══════════════════════════════════════════════════════════════════════════════

class USuspenseCoreLoadoutManager;

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
 *
 * OPTIONAL MODULES:
 * - InventorySystem: InventoryComponent (nullptr if disabled)
 * - EquipmentSystem: Equipment components (nullptr if disabled)
 * Components are created in constructor only if module is enabled (WITH_*_SYSTEM=1)
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
	// PUBLIC API - INVENTORY (Returns nullptr if WITH_INVENTORY_SYSTEM=0)
	// Cast to USuspenseCoreInventoryComponent* in code when module is enabled
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Get the inventory component (nullptr if InventorySystem disabled) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory")
	UActorComponent* GetInventoryComponent() const { return InventoryComponent; }

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - EQUIPMENT (Returns nullptr if WITH_EQUIPMENT_SYSTEM=0)
	// Cast to specific equipment component types in code when module is enabled
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Get the equipment data store (core equipment state) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment")
	UActorComponent* GetEquipmentDataStore() const { return EquipmentDataStore; }

	/** Get the equipment transaction processor (atomic operations) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment")
	UActorComponent* GetEquipmentTxnProcessor() const { return EquipmentTxnProcessor; }

	/** Get the equipment operation executor (validated operations) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment")
	UActorComponent* GetEquipmentOps() const { return EquipmentOps; }

	/** Get the equipment prediction system (client-side prediction) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment")
	UActorComponent* GetEquipmentPrediction() const { return EquipmentPrediction; }

	/** Get the equipment inventory bridge (connects equipment to inventory) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment")
	UActorComponent* GetEquipmentInventoryBridge() const { return EquipmentInventoryBridge; }

	/** Get the weapon state manager (weapon FSM) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment")
	UActorComponent* GetWeaponStateManager() const { return WeaponStateManager; }

	/** Get the equipment UI provider (ISuspenseCoreUIDataProvider for widget binding) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment")
	UActorComponent* GetEquipmentUIProvider() const { return EquipmentUIProvider; }

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
	// CORE COMPONENTS
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Ability System Component - created in constructor */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "SuspenseCore|Components")
	USuspenseCoreAbilitySystemComponent* AbilitySystemComponent = nullptr;

	/** Attribute Set - spawned by ASC */
	UPROPERTY()
	USuspenseCoreAttributeSet* AttributeSet = nullptr;

	// ═══════════════════════════════════════════════════════════════════════════════
	// INVENTORY COMPONENT (nullptr if WITH_INVENTORY_SYSTEM=0)
	// Created in constructor only if InventorySystem module is enabled.
	// Use base type UActorComponent* to satisfy UHT when module is disabled.
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Inventory Component - created in constructor, persists across respawns */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "SuspenseCore|Components")
	UActorComponent* InventoryComponent = nullptr;

	// ═══════════════════════════════════════════════════════════════════════════════
	// EQUIPMENT MODULE COMPONENTS (nullptr if WITH_EQUIPMENT_SYSTEM=0)
	// Created in constructor only if EquipmentSystem module is enabled.
	// Use base type UActorComponent* to satisfy UHT when module is disabled.
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Core data store for equipment state (Server authoritative, replicated) */
	UPROPERTY(VisibleAnywhere, Replicated, Category = "SuspenseCore|Equipment|Core")
	UActorComponent* EquipmentDataStore = nullptr;

	/** Transaction processor for atomic equipment changes */
	UPROPERTY(VisibleAnywhere, Replicated, Category = "SuspenseCore|Equipment|Core")
	UActorComponent* EquipmentTxnProcessor = nullptr;

	/** Operation executor (deterministic, validated) */
	UPROPERTY(VisibleAnywhere, Replicated, Category = "SuspenseCore|Equipment|Core")
	UActorComponent* EquipmentOps = nullptr;

	/** Prediction system (client owning) */
	UPROPERTY(VisibleAnywhere, Replicated, Category = "SuspenseCore|Equipment|Networking")
	UActorComponent* EquipmentPrediction = nullptr;

	/** Replication manager (delta-based replication) */
	UPROPERTY(VisibleAnywhere, Replicated, Category = "SuspenseCore|Equipment|Networking")
	UActorComponent* EquipmentReplication = nullptr;

	/** Network dispatcher (RPC/request queue) */
	UPROPERTY(VisibleAnywhere, Replicated, Category = "SuspenseCore|Equipment|Networking")
	UActorComponent* EquipmentNetworkDispatcher = nullptr;

	/** Event dispatcher / equipment event bus (local) */
	UPROPERTY(VisibleAnywhere, Replicated, Category = "SuspenseCore|Equipment|Events")
	UActorComponent* EquipmentEventDispatcher = nullptr;

	/** Weapon state manager (FSM) */
	UPROPERTY(VisibleAnywhere, Replicated, Category = "SuspenseCore|Equipment|Weapon")
	UActorComponent* WeaponStateManager = nullptr;

	/** Inventory bridge (connects equipment to existing inventory) */
	UPROPERTY(VisibleAnywhere, Replicated, Category = "SuspenseCore|Equipment|Inventory")
	UActorComponent* EquipmentInventoryBridge = nullptr;

	/** UI Provider for Equipment Widget binding (ISuspenseCoreUIDataProvider) */
	UPROPERTY(VisibleAnywhere, Replicated, Category = "SuspenseCore|Equipment|UI")
	UActorComponent* EquipmentUIProvider = nullptr;

	/** Slot validator (UObject, not component) - created during WireEquipmentModule() */
	UPROPERTY()
	UObject* EquipmentSlotValidator = nullptr;

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

	/**
	 * Default loadout ID for this player.
	 * References row in SuspenseCoreDataManager's LoadoutDataTable.
	 * Used only if InventorySystem or EquipmentSystem is enabled.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Loadout")
	FName DefaultLoadoutID = TEXT("Default_Soldier");

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
	// These methods exist but do nothing if WITH_EQUIPMENT_SYSTEM=0
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Wire up per-player equipment components with global services */
	bool WireEquipmentModule(USuspenseCoreLoadoutManager* LoadoutManager = nullptr, const FName& AppliedLoadoutID = NAME_None);

	/** Attempt to wire equipment module once */
	bool TryWireEquipmentModuleOnce();

	/** Initialize all equipment module components */
	void InitializeEquipmentComponents();

	/** Initialize inventory component from loadout configuration */
	void InitializeInventoryFromLoadout();

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

	// Equipment wiring retry (non-reflected, can use #if in .cpp)
	int32 EquipmentWireRetryCount = 0;
	FTimerHandle EquipmentWireRetryHandle;
	static constexpr int32 MaxEquipmentWireRetries = 20;
	static constexpr float EquipmentWireRetryInterval = 0.05f;
};
