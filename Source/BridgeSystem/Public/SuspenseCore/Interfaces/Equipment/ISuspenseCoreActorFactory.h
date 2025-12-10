// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryBaseTypes.h"
#include "ISuspenseCoreActorFactory.generated.h"

/**
 * Actor spawn parameters for equipment visualization
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FEquipmentActorSpawnParams
{
	GENERATED_BODY()

	/** Item ID to spawn actor for */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn")
	FName ItemId = NAME_None;

	/** Actor class to spawn (if known) */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn")
	TSubclassOf<AActor> ActorClass;

	/** Owner actor for the spawned equipment */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn")
	TWeakObjectPtr<AActor> Owner;

	/** Spawn transform */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn")
	FTransform SpawnTransform = FTransform::Identity;

	/** Target slot index */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn")
	int32 SlotIndex = INDEX_NONE;

	/** Item instance data */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn")
	FSuspenseCoreInventoryItemInstance ItemInstance;

	/** Whether to use object pooling */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn")
	bool bUsePooling = true;

	/** Whether to auto-attach after spawn */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn")
	bool bAutoAttach = false;

	/** Socket name for attachment */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn")
	FName AttachSocket = NAME_None;
};

/**
 * Result of actor spawn operation
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FEquipmentActorSpawnResult
{
	GENERATED_BODY()

	/** Whether spawn was successful */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bSuccess = false;

	/** Spawned actor (valid if success) */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TWeakObjectPtr<AActor> SpawnedActor;

	/** Error message if failed */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FText ErrorMessage;

	/** Whether actor came from pool */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bFromPool = false;

	/** Slot index assigned */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 AssignedSlotIndex = INDEX_NONE;

	static FEquipmentActorSpawnResult Success(AActor* Actor, int32 SlotIndex = INDEX_NONE, bool bPooled = false)
	{
		FEquipmentActorSpawnResult Result;
		Result.bSuccess = true;
		Result.SpawnedActor = Actor;
		Result.AssignedSlotIndex = SlotIndex;
		Result.bFromPool = bPooled;
		return Result;
	}

	static FEquipmentActorSpawnResult Failure(const FText& Error)
	{
		FEquipmentActorSpawnResult Result;
		Result.bSuccess = false;
		Result.ErrorMessage = Error;
		return Result;
	}
};

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreActorFactory : public UInterface
{
	GENERATED_BODY()
};

/**
 * SuspenseCore Actor Factory Interface
 *
 * Factory pattern for equipment actor creation and lifecycle management.
 * Supports object pooling for performance optimization.
 */
class BRIDGESYSTEM_API ISuspenseCoreActorFactory
{
	GENERATED_BODY()

public:
	/** Spawn equipment actor with parameters */
	virtual FEquipmentActorSpawnResult SpawnEquipmentActor(const FEquipmentActorSpawnParams& Params) = 0;

	/** Destroy equipment actor */
	virtual bool DestroyEquipmentActor(AActor* Actor, bool bImmediate = false) = 0;

	/** Configure equipment actor with item instance data */
	virtual bool ConfigureEquipmentActor(AActor* Actor, const FSuspenseCoreInventoryItemInstance& ItemInstance) = 0;

	/** Return actor to pool for reuse */
	virtual bool RecycleActor(AActor* Actor) = 0;

	/** Get actor from pool */
	virtual AActor* GetPooledActor(TSubclassOf<AActor> ActorClass) = 0;

	/** Preload actor class for item */
	virtual bool PreloadActorClass(const FName& ItemId) = 0;

	/** Get spawn transform for slot */
	virtual FTransform GetSpawnTransformForSlot(int32 SlotIndex, AActor* Owner) const = 0;

	/** Register spawned actor for tracking */
	virtual bool RegisterSpawnedActor(AActor* Actor, int32 SlotIndex) = 0;

	/** Unregister actor from tracking */
	virtual bool UnregisterActor(AActor* Actor) = 0;

	/** Get all spawned actors by slot */
	virtual TMap<int32, AActor*> GetAllSpawnedActors() const = 0;

	/** Clear all tracked actors */
	virtual void ClearAllActors(bool bDestroy = true) = 0;
};
