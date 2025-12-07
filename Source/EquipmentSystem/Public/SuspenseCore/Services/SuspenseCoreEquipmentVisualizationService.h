// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Interfaces/Equipment/ISuspenseEquipmentService.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEquipmentServiceMacros.h"
#include "SuspenseCoreEquipmentVisualizationService.generated.h"

// Forward declarations
class USuspenseEquipmentServiceLocator;
class USuspenseCoreEventBus;
class AActor;
class UMeshComponent;
class USkeletalMeshComponent;
class ISuspenseVisualProvider;
class ISuspenseActorFactory;
class ISuspenseAttachmentProvider;
struct FEquipmentVisualData;

/**
 * SuspenseCoreEquipmentVisualizationService
 *
 * Philosophy:
 * Manages visual presentation of equipment in the game world.
 * Separates visual concerns from data and logic layers.
 *
 * Key Responsibilities:
 * - Equipment actor spawning and management
 * - Mesh attachment to character
 * - Visual state synchronization
 * - Animation integration
 * - Material/texture management
 * - VFX coordination
 * - LOD management
 *
 * Architecture Patterns:
 * - Event Bus: Subscribes to equipment events for visual updates
 * - Dependency Injection: Uses ServiceLocator
 * - GameplayTags: Visual state categorization
 * - Observer Pattern: Reacts to equipment state changes
 * - Factory Pattern: Actor creation
 *
 * Visual Update Flow:
 * 1. Subscribe to equipment events (equipped, unequipped, etc.)
 * 2. On event received, spawn/despawn actors
 * 3. Attach actors to appropriate sockets
 * 4. Update visual state (materials, animations)
 * 5. Publish visual ready events
 */
UCLASS(BlueprintType)
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentVisualizationService : public UObject, public IEquipmentVisualizationService
{
	GENERATED_BODY()

public:
	USuspenseCoreEquipmentVisualizationService();
	virtual ~USuspenseCoreEquipmentVisualizationService();

	//========================================
	// ISuspenseEquipmentService Interface
	//========================================

	virtual bool InitializeService(const FServiceInitParams& Params) override;
	virtual bool ShutdownService(bool bForce = false) override;
	virtual EServiceLifecycleState GetServiceState() const override;
	virtual bool IsServiceReady() const override;
	virtual FGameplayTag GetServiceTag() const override;
	virtual FGameplayTagContainer GetRequiredDependencies() const override;
	virtual bool ValidateService(TArray<FText>& OutErrors) const override;
	virtual void ResetService() override;
	virtual FString GetServiceStats() const override;

	//========================================
	// IEquipmentVisualizationService Interface
	//========================================

	virtual ISuspenseVisualProvider* GetVisualProvider() override;
	virtual ISuspenseActorFactory* GetActorFactory() override;
	virtual ISuspenseAttachmentProvider* GetAttachmentProvider() override;

	//========================================
	// Actor Management
	//========================================

	/**
	 * Spawn equipment actor
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Visualization")
	AActor* SpawnEquipmentActor(const struct FSuspenseInventoryItemInstance& Item, AActor* Owner);

	/**
	 * Despawn equipment actor
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Visualization")
	bool DespawnEquipmentActor(AActor* EquipmentActor);

	/**
	 * Get spawned actor for slot
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment|Visualization")
	AActor* GetActorForSlot(int32 SlotIndex) const;

	/**
	 * Get all spawned equipment actors
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Visualization")
	TArray<AActor*> GetAllEquipmentActors() const;

	//========================================
	// Attachment Management
	//========================================

	/**
	 * Attach equipment actor to socket
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Visualization")
	bool AttachToSocket(AActor* EquipmentActor, USkeletalMeshComponent* ParentMesh, FName SocketName);

	/**
	 * Detach equipment actor
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Visualization")
	bool DetachEquipmentActor(AActor* EquipmentActor);

	/**
	 * Get attachment socket for slot
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment|Visualization")
	FName GetSocketForSlot(int32 SlotIndex) const;

	/**
	 * Update attachment transform
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Visualization")
	bool UpdateAttachmentTransform(AActor* EquipmentActor, const FTransform& RelativeTransform);

	//========================================
	// Visual State Management
	//========================================

	/**
	 * Update visual state for slot
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Visualization")
	void UpdateSlotVisuals(int32 SlotIndex);

	/**
	 * Set equipment visibility
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Visualization")
	void SetEquipmentVisibility(int32 SlotIndex, bool bVisible);

	/**
	 * Apply visual customization
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Visualization")
	void ApplyVisualCustomization(int32 SlotIndex, const struct FEquipmentVisualData& VisualData);

	/**
	 * Refresh all equipment visuals
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Visualization")
	void RefreshAllVisuals();

	//========================================
	// Material Management
	//========================================

	/**
	 * Update equipment materials
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Visualization")
	void UpdateEquipmentMaterials(AActor* EquipmentActor, const TArray<class UMaterialInterface*>& Materials);

	/**
	 * Set material parameter
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Visualization")
	void SetMaterialParameter(AActor* EquipmentActor, FName ParameterName, float Value);

	/**
	 * Set material vector parameter
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Visualization")
	void SetMaterialVectorParameter(AActor* EquipmentActor, FName ParameterName, FLinearColor Value);

	//========================================
	// Animation Integration
	//========================================

	/**
	 * Notify animation system of equipment change
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Visualization")
	void NotifyAnimationSystemChanged(int32 SlotIndex);

	/**
	 * Get animation set for equipment
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Visualization")
	class UAnimationAsset* GetEquipmentAnimationSet(int32 SlotIndex) const;

	//========================================
	// Event Publishing
	//========================================

	/**
	 * Publish visual spawned event
	 */
	void PublishVisualSpawned(int32 SlotIndex, AActor* SpawnedActor);

	/**
	 * Publish visual despawned event
	 */
	void PublishVisualDespawned(int32 SlotIndex, AActor* DespawnedActor);

	/**
	 * Publish visual updated event
	 */
	void PublishVisualUpdated(int32 SlotIndex);

protected:
	//========================================
	// Service Lifecycle
	//========================================

	/** Initialize visual providers */
	bool InitializeVisualProviders();

	/** Setup event subscriptions */
	void SetupEventSubscriptions();

	/** Cleanup spawned actors */
	void CleanupSpawnedActors();

	//========================================
	// Visual Operations
	//========================================

	/** Spawn actor internal */
	AActor* SpawnActorInternal(const struct FSuspenseInventoryItemInstance& Item, AActor* Owner);

	/** Get actor class for item */
	TSubclassOf<AActor> GetActorClassForItem(const struct FSuspenseInventoryItemInstance& Item) const;

	/** Configure spawned actor */
	void ConfigureSpawnedActor(AActor* Actor, const struct FSuspenseInventoryItemInstance& Item);

	//========================================
	// Event Handlers
	//========================================

	/** Handle equipment equipped event */
	void OnEquipmentEquipped(const struct FSuspenseEquipmentEventData& EventData);

	/** Handle equipment unequipped event */
	void OnEquipmentUnequipped(const struct FSuspenseEquipmentEventData& EventData);

	/** Handle equipment swapped event */
	void OnEquipmentSwapped(const struct FSuspenseEquipmentEventData& EventData);

	/** Handle visual settings changed */
	void OnVisualSettingsChanged(const struct FSuspenseEquipmentEventData& EventData);

private:
	//========================================
	// Service State
	//========================================

	/** Current service lifecycle state */
	UPROPERTY()
	EServiceLifecycleState ServiceState;

	/** Service initialization timestamp */
	UPROPERTY()
	FDateTime InitializationTime;

	//========================================
	// Dependencies (via ServiceLocator)
	//========================================

	/** ServiceLocator for dependency injection */
	UPROPERTY()
	TWeakObjectPtr<USuspenseEquipmentServiceLocator> ServiceLocator;

	/** EventBus for event subscription/publishing */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> EventBus;

	/** Data service for equipment state queries */
	UPROPERTY()
	TWeakObjectPtr<UObject> DataService;

	//========================================
	// Spawned Actors
	//========================================

	/** Map of slot index to spawned actor */
	UPROPERTY()
	TMap<int32, TObjectPtr<AActor>> SpawnedActors;

	/** Actors pending despawn (delayed destruction) */
	UPROPERTY()
	TArray<TObjectPtr<AActor>> PendingDespawn;

	//========================================
	// Attachment Configuration
	//========================================

	/** Map of slot index to socket name */
	UPROPERTY()
	TMap<int32, FName> SlotSocketMap;

	/** Default attachment socket */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	FName DefaultAttachSocket;

	//========================================
	// Configuration
	//========================================

	/** Enable actor pooling */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	bool bEnableActorPooling;

	/** Pool size per actor class */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	int32 ActorPoolSize;

	/** Enable LOD management */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	bool bEnableLODManagement;

	/** Enable detailed logging */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	bool bEnableDetailedLogging;

	//========================================
	// Statistics
	//========================================

	/** Total actors spawned */
	UPROPERTY()
	int32 TotalActorsSpawned;

	/** Total actors despawned */
	UPROPERTY()
	int32 TotalActorsDespawned;

	/** Currently active actors */
	UPROPERTY()
	int32 ActiveActorCount;

	/** Pool hits */
	UPROPERTY()
	int32 PoolHits;

	/** Pool misses */
	UPROPERTY()
	int32 PoolMisses;
};
