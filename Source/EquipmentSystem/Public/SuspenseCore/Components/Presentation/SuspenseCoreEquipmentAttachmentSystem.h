// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/Equipment/ISuspenseCoreAttachmentProvider.h"

// фундаментальные утилиты
#include "Core/Utils/SuspenseCoreEquipmentCacheManager.h"
#include "Core/Utils/SuspenseCoreEquipmentEventBus.h"
#include "Services/SuspenseCoreEquipmentServiceMacros.h"

#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"

#include "SuspenseCoreEquipmentAttachmentSystem.generated.h"

/**
 * Attachment state tracking data
 */
USTRUCT()
struct FAttachmentStateData
{
	GENERATED_BODY()

	UPROPERTY()
	FEquipmentAttachmentState CurrentState;

	UPROPERTY()
	bool bIsTransitioning = false;

	UPROPERTY()
	float TransitionStartTime = 0.0f;

	UPROPERTY()
	float TransitionDuration = 0.0f;

	UPROPERTY()
	FTransform StartTransform;

	UPROPERTY()
	FTransform TargetTransform;

	UPROPERTY()
	FName PreviousSocket;
};

/**
 * Attachment system configuration
 */
USTRUCT(BlueprintType)
struct FAttachmentSystemConfig
{
	GENERATED_BODY()

	/** Enable smooth attachment transitions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transitions")
	bool bEnableSmoothTransitions = true;

	/** Default transition duration in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transitions", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float DefaultTransitionDuration = 0.3f;

	/** Validate socket existence before attachment */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	bool bValidateSockets = true;

	/** Allow multiple items on same socket */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	bool bAllowSocketSharing = false;

	/** Update rate for smooth transitions (Hz) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", meta = (ClampMin = "30.0", ClampMax = "120.0"))
	float TransitionUpdateRate = 60.0f;
};

/**
 * Socket mapping configuration for item types
 */
USTRUCT(BlueprintType)
struct FSocketMappingConfig
{
	GENERATED_BODY()

	/** Item type tag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	FGameplayTag ItemType;

	/** Active position socket name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	FName ActiveSocket;

	/** Holstered/inactive position socket name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	FName InactiveSocket;

	/** Priority for this mapping (higher = preferred) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping", meta = (ClampMin = "0", ClampMax = "100"))
	int32 Priority = 0;
};

/**
 * Equipment Attachment System Component
 *
 * SRP: только «физическое» крепление актёров к сокетам (никаких эффектов и спавна).
 * Потокобезопасность — через FEquipmentRWLock, обмен — через EventBus, кеш — FEquipmentCacheManager.
 */
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentAttachmentSystem : public UActorComponent, public ISuspenseCoreAttachmentProvider
{
	GENERATED_BODY()

public:
	USuspenseCoreEquipmentAttachmentSystem();

	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent Interface

	//~ Begin ISuspenseCoreAttachmentProvider Interface
	virtual bool AttachEquipment(AActor* Equipment, USceneComponent* Target, const FEquipmentAttachmentConfig& Config) override;
	virtual bool DetachEquipment(AActor* Equipment, bool bMaintainWorldTransform = false) override;
	virtual bool UpdateAttachment(AActor* Equipment, const FEquipmentAttachmentConfig& NewConfig, bool bSmooth = false) override;
	virtual FEquipmentAttachmentState GetAttachmentState(AActor* Equipment) const override;
	virtual FName FindBestSocket(USkeletalMeshComponent* Target, const FGameplayTag& ItemType, bool bIsActive) const override;
	virtual bool SwitchAttachmentState(AActor* Equipment, bool bMakeActive, float Duration = 0.0f) override;
	virtual FEquipmentAttachmentConfig GetSlotAttachmentConfig(int32 SlotIndex, bool bIsActive) const override;
	virtual bool ValidateSocket(USceneComponent* Target, const FName& SocketName) const override;
	//~ End ISuspenseCoreAttachmentProvider Interface

	// ===== High-level API для VisualizationService =====

	/** Вернуть компонент крепления (скелетный меш персонажа или root) */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Attachment")
	USceneComponent* GetAttachmentTarget(AActor* Character) const;

	/** Привязать визуальный актёр к персонажу (по слоту/сокету/офсету) */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Attachment")
	bool AttachToCharacter(AActor* Equipment, AActor* TargetCharacter,
	                       FName Socket, FTransform Offset,
	                       bool bSmooth = true, float BlendTime = 0.2f);

	/** Отвязать визуальный актёр от персонажа (мягко) */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Attachment")
	bool DetachFromCharacter(AActor* Equipment, AActor* TargetCharacter, bool bSmooth = true);

	/**
	 * Set system configuration
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment")
	void SetSystemConfiguration(const FAttachmentSystemConfig& NewConfig) { SystemConfig = NewConfig; }

	/**
	 * Get current configuration
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment")
	FAttachmentSystemConfig GetSystemConfiguration() const { return SystemConfig; }

	/**
	 * Add socket mapping for item type
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment")
	void AddSocketMapping(const FSocketMappingConfig& Mapping) { SocketMappings.Add(Mapping); }

	/**
	 * Remove socket mapping for item type
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment")
	void RemoveSocketMapping(const FGameplayTag& ItemType);

	/**
	 * Get all attached equipment
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment")
	TArray<AActor*> GetAllAttachedEquipment() const;

	/**
	 * Check if equipment is attached
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment")
	bool IsEquipmentAttached(AActor* Equipment) const;

	/**
	 * Force update all attachments
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment")
	void ForceUpdateAllAttachments();

	/**
	 * Get attachment statistics
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment")
	FString GetAttachmentStatistics() const;

protected:
	/**
	 * System configuration
	 */
	UPROPERTY(EditAnywhere, Category = "Attachment Config")
	FAttachmentSystemConfig SystemConfig;

	/**
	 * Socket mappings for different item types
	 */
	UPROPERTY(EditAnywhere, Category = "Socket Mappings")
	TArray<FSocketMappingConfig> SocketMappings;

	/**
	 * Active attachment states
	 */
	UPROPERTY()
	TMap<AActor*, FAttachmentStateData> AttachmentStates;

	/**
	 * Socket occupation tracking
	 */
	TMap<FName, AActor*> OccupiedSockets;

	/**
	 * Cache for socket configurations (mutable — чтобы вызывать Get() в const методе)
	 */
	mutable FSuspenseCoreEquipmentCacheManager<FGameplayTag, FSocketMappingConfig> SocketConfigCache;

	/**
	 * Thread synchronization (RW)
	 */
	mutable FEquipmentRWLock AttachmentStateLock;
	mutable FEquipmentRWLock SocketOccupationLock;

	/**
	 * Active transition counter
	 */
	int32 ActiveTransitionCount = 0;

private:
	bool InternalAttach(AActor* Equipment, USceneComponent* Target, const FName& SocketName, const FTransform& RelativeTransform,
						EAttachmentRule LocationRule, EAttachmentRule RotationRule, EAttachmentRule ScaleRule, bool bWeldBodies);
	void UpdateTransitions(float DeltaTime);
	const FSocketMappingConfig* FindSocketMapping(const FGameplayTag& ItemType) const;
	void MarkSocketOccupied(const FName& SocketName, AActor* Equipment);
	void ClearSocketOccupation(const FName& SocketName);
	USceneComponent* GetEquipmentRootComponent(AActor* Equipment) const;
	void InitializeDefaultMappings();
	void BroadcastAttachmentEvent(const FGameplayTag& EventTag, AActor* Equipment, bool bSuccess);
	void LogAttachmentOperation(const FString& Operation, const FString& Details) const;
	USkeletalMeshComponent* FindPrimarySkelMesh(AActor* Character) const;
};
