// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreSystemCoordinatorComponent.generated.h"

// Forward declarations
class USuspenseCoreEventBus;
class USuspenseCoreServiceLocator;
class USuspenseCoreEquipmentOperationExecutor;
class USuspenseCoreWeaponStateManager;
class USuspenseCoreEquipmentDataStore;
class UAbilitySystemComponent;
struct FSuspenseCoreEventData;

/**
 * FSuspenseCoreSubsystemStatus
 *
 * Status information for a registered subsystem
 */
USTRUCT(BlueprintType)
struct FSuspenseCoreSubsystemStatus
{
	GENERATED_BODY()

	/** Subsystem identifier tag */
	UPROPERTY(BlueprintReadOnly, Category = "Status")
	FGameplayTag SubsystemTag;

	/** Is subsystem initialized */
	UPROPERTY(BlueprintReadOnly, Category = "Status")
	bool bIsInitialized = false;

	/** Is subsystem active */
	UPROPERTY(BlueprintReadOnly, Category = "Status")
	bool bIsActive = false;

	/** Last update timestamp */
	UPROPERTY(BlueprintReadOnly, Category = "Status")
	float LastUpdateTime = 0.0f;

	/** Error message if any */
	UPROPERTY(BlueprintReadOnly, Category = "Status")
	FText ErrorMessage;
};

/**
 * USuspenseCoreSystemCoordinatorComponent
 *
 * Coordinates all equipment subsystems and manages their lifecycle.
 *
 * Architecture:
 * - EventBus: Central communication hub for subsystem coordination
 * - ServiceLocator: Dependency injection for service access
 * - GameplayTags: Subsystem identification and event routing
 *
 * Responsibilities:
 * - Initialize and shutdown all equipment subsystems
 * - Coordinate communication between subsystems
 * - Monitor subsystem health and status
 * - Handle subsystem dependencies and initialization order
 * - Publish coordination events through EventBus
 */
UCLASS(ClassGroup=(SuspenseCore), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreSystemCoordinatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USuspenseCoreSystemCoordinatorComponent();

	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent Interface

	//================================================
	// Initialization
	//================================================

	/**
	 * Initialize coordinator with dependencies
	 * @param InServiceLocator ServiceLocator for dependency injection
	 * @param InASC Ability System Component
	 * @return True if initialized successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Coordinator")
	bool Initialize(USuspenseCoreServiceLocator* InServiceLocator, UAbilitySystemComponent* InASC);

	/**
	 * Shutdown coordinator and all subsystems
	 * @param bForce Force shutdown even if subsystems are busy
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Coordinator")
	void Shutdown(bool bForce = false);

	/**
	 * Check if coordinator is initialized
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Coordinator")
	bool IsInitialized() const { return bIsInitialized; }

	//================================================
	// Subsystem Management
	//================================================

	/**
	 * Register a subsystem with the coordinator
	 * @param SubsystemTag Gameplay tag identifying the subsystem
	 * @param Subsystem The subsystem component/object to register
	 * @return True if registration succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Coordinator|Subsystems")
	bool RegisterSubsystem(FGameplayTag SubsystemTag, UObject* Subsystem);

	/**
	 * Unregister a subsystem
	 * @param SubsystemTag Tag of subsystem to unregister
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Coordinator|Subsystems")
	void UnregisterSubsystem(FGameplayTag SubsystemTag);

	/**
	 * Get registered subsystem by tag
	 * @param SubsystemTag Tag of subsystem to retrieve
	 * @return Subsystem object or nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Coordinator|Subsystems")
	UObject* GetSubsystem(FGameplayTag SubsystemTag) const;

	/**
	 * Get subsystem status
	 * @param SubsystemTag Tag of subsystem
	 * @return Status information
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Coordinator|Subsystems")
	FSuspenseCoreSubsystemStatus GetSubsystemStatus(FGameplayTag SubsystemTag) const;

	/**
	 * Check if subsystem is ready
	 * @param SubsystemTag Tag of subsystem
	 * @return True if subsystem is initialized and active
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Coordinator|Subsystems")
	bool IsSubsystemReady(FGameplayTag SubsystemTag) const;

	/**
	 * Get all registered subsystems
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Coordinator|Subsystems")
	TArray<FGameplayTag> GetRegisteredSubsystems() const;

	//================================================
	// Coordination Operations
	//================================================

	/**
	 * Initialize all registered subsystems in dependency order
	 * @return True if all subsystems initialized successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Coordinator")
	bool InitializeAllSubsystems();

	/**
	 * Activate all subsystems
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Coordinator")
	void ActivateAllSubsystems();

	/**
	 * Deactivate all subsystems
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Coordinator")
	void DeactivateAllSubsystems();

	/**
	 * Validate all subsystems are healthy
	 * @param OutErrors Array to receive error messages
	 * @return True if all subsystems are healthy
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Coordinator")
	bool ValidateAllSubsystems(TArray<FText>& OutErrors) const;

	//================================================
	// Event Coordination
	//================================================

	/**
	 * Broadcast event to all subsystems
	 * @param EventTag Event to broadcast
	 * @param EventData Event data
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Coordinator|Events")
	void BroadcastToSubsystems(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/**
	 * Request subsystem synchronization
	 * Forces all subsystems to update their state
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Coordinator")
	void RequestSubsystemSync();

	//================================================
	// Statistics and Monitoring
	//================================================

	/**
	 * Get coordinator statistics
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Coordinator|Stats")
	FString GetCoordinatorStats() const;

	/**
	 * Get number of registered subsystems
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Coordinator|Stats")
	int32 GetSubsystemCount() const { return RegisteredSubsystems.Num(); }

protected:
	//================================================
	// Event Handlers
	//================================================

	/** Handle subsystem state changed event */
	void OnSubsystemStateChanged(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle subsystem error event */
	void OnSubsystemError(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	//================================================
	// Internal Methods
	//================================================

	/** Initialize core subsystems */
	bool InitializeCoreSubsystems();

	/** Setup event subscriptions */
	void SetupEventSubscriptions();

	/** Update subsystem statuses */
	void UpdateSubsystemStatuses();

	/** Publish coordinator event */
	void PublishCoordinatorEvent(FGameplayTag EventTag, const FString& Message);

private:
	//================================================
	// Dependencies (Injected)
	//================================================

	/** ServiceLocator for dependency injection */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreServiceLocator> ServiceLocator;

	/** EventBus for event coordination */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> EventBus;

	/** Ability System Component */
	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	//================================================
	// Registered Subsystems
	//================================================

	/** Map of subsystem tags to subsystem objects */
	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<UObject>> RegisteredSubsystems;

	/** Map of subsystem tags to status information */
	UPROPERTY()
	TMap<FGameplayTag, FSuspenseCoreSubsystemStatus> SubsystemStatuses;

	//================================================
	// State
	//================================================

	/** Initialization flag */
	UPROPERTY()
	bool bIsInitialized;

	/** Active flag */
	UPROPERTY()
	bool bIsActive;

	/** Last sync time */
	UPROPERTY()
	float LastSyncTime;

	//================================================
	// Configuration
	//================================================

	/** Enable automatic subsystem health monitoring */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	bool bEnableHealthMonitoring;

	/** Health check interval in seconds */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	float HealthCheckInterval;

	//================================================
	// Statistics
	//================================================

	/** Total coordination events published */
	UPROPERTY()
	int32 TotalCoordinationEvents;

	/** Total subsystem errors handled */
	UPROPERTY()
	int32 TotalSubsystemErrors;

	//================================================
	// Thread Safety
	//================================================

	/** Critical section for thread-safe subsystem access */
	mutable FCriticalSection SubsystemCriticalSection;
};
