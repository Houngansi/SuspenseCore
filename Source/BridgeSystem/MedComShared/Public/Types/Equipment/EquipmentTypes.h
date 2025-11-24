// EquipmentTypes.h
// Copyright MedCom Team. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/Inventory/InventoryTypes.h"
#include "Types/Loadout/LoadoutSettings.h"
#include "Interfaces/Core/IMedComLoadoutInterface.h" // canonical FLoadoutApplicationResult
#include "EquipmentTypes.generated.h"

UENUM(BlueprintType)
enum class EEquipmentOperationType:uint8
{
    None=0 UMETA(DisplayName="None"),
    Equip UMETA(DisplayName="Equip Item"),
    Unequip UMETA(DisplayName="Unequip Item"),
    Swap UMETA(DisplayName="Swap Items"),
    Move UMETA(DisplayName="Move Item"),
    Drop UMETA(DisplayName="Drop Item"),
    Transfer UMETA(DisplayName="Transfer Between Containers"),
    QuickSwitch UMETA(DisplayName="Quick Switch Weapon"),
    Reload UMETA(DisplayName="Reload Weapon"),
    Inspect UMETA(DisplayName="Inspect Item"),
    Repair UMETA(DisplayName="Repair Item"),
    Upgrade UMETA(DisplayName="Upgrade Item"),
    Modify UMETA(DisplayName="Modify Item"),
    Combine UMETA(DisplayName="Combine Items"),
    Split UMETA(DisplayName="Split Stack")
};

UENUM(BlueprintType)
enum class EEquipmentOperationPriority:uint8
{
    Low=0 UMETA(DisplayName="Low Priority"),
    Normal=1 UMETA(DisplayName="Normal Priority"),
    High=2 UMETA(DisplayName="High Priority"),
    Critical=3 UMETA(DisplayName="Critical Priority"),
    System=255 UMETA(DisplayName="System Priority")
};

UENUM(BlueprintType)
enum class EEquipmentState:uint8
{
    Idle=0 UMETA(DisplayName="Idle"),
    Equipping UMETA(DisplayName="Equipping"),
    Unequipping UMETA(DisplayName="Unequipping"),
    Switching UMETA(DisplayName="Switching"),
    Reloading UMETA(DisplayName="Reloading"),
    Inspecting UMETA(DisplayName="Inspecting"),
    Repairing UMETA(DisplayName="Repairing"),
    Upgrading UMETA(DisplayName="Upgrading"),
    Locked UMETA(DisplayName="Locked"),
    Error UMETA(DisplayName="Error State")
};

UENUM(BlueprintType)
enum class ETransactionState:uint8
{
    None=0 UMETA(DisplayName="None"),
    Active UMETA(DisplayName="Active"),
    Committing UMETA(DisplayName="Committing"),
    Committed UMETA(DisplayName="Committed"),
    RollingBack UMETA(DisplayName="Rolling Back"),
    RolledBack UMETA(DisplayName="Rolled Back"),
    Failed UMETA(DisplayName="Failed")
};

UENUM(BlueprintType)
enum class EEquipmentValidationFailure:uint8
{
    None=0 UMETA(DisplayName="No Failure"),
    InvalidSlot UMETA(DisplayName="Invalid Slot"),
    SlotOccupied UMETA(DisplayName="Slot Occupied"),
    IncompatibleType UMETA(DisplayName="Incompatible Type"),
    RequirementsNotMet UMETA(DisplayName="Requirements Not Met"),
    WeightLimit UMETA(DisplayName="Weight Limit Exceeded"),
    ConflictingItem UMETA(DisplayName="Conflicting Item"),
    LevelRequirement UMETA(DisplayName="Level Requirement Not Met"),
    ClassRestriction UMETA(DisplayName="Class Restriction"),
    UniqueConstraint UMETA(DisplayName="Unique Item Constraint"),
    CooldownActive UMETA(DisplayName="Cooldown Active"),
    TransactionActive UMETA(DisplayName="Transaction Active"),
    NetworkError UMETA(DisplayName="Network Error"),
    SystemError UMETA(DisplayName="System Error")
};

USTRUCT(BlueprintType)
struct MEDCOMSHARED_API FEquipmentOperationRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite,Category="Operation") 
    FGuid OperationId;
    
    UPROPERTY(BlueprintReadWrite,Category="Operation") 
    EEquipmentOperationType OperationType=EEquipmentOperationType::None;
    
    UPROPERTY(BlueprintReadWrite,Category="Operation") 
    EEquipmentOperationPriority Priority=EEquipmentOperationPriority::Normal;
    
    UPROPERTY(BlueprintReadWrite,Category="Operation") 
    FInventoryItemInstance ItemInstance;
    
    UPROPERTY(BlueprintReadWrite,Category="Operation") 
    int32 SourceSlotIndex=INDEX_NONE;
    
    UPROPERTY(BlueprintReadWrite,Category="Operation") 
    int32 TargetSlotIndex=INDEX_NONE;
    
    UPROPERTY(BlueprintReadWrite,Category="Operation") 
    float Timestamp=0.0f;
    
    UPROPERTY(BlueprintReadWrite,Category="Operation") 
    TWeakObjectPtr<AActor> Instigator;
    
    UPROPERTY(BlueprintReadWrite,Category="Operation") 
    TMap<FString,FString> Parameters;
    
    UPROPERTY(BlueprintReadWrite,Category="Operation") 
    bool bForceOperation=false;
    
    UPROPERTY(BlueprintReadWrite,Category="Operation") 
    bool bIsSimulated=false;
    
    uint32 SequenceNumber=0;

    /** Static factory method to create new request */
    static FEquipmentOperationRequest Create()
    {
        FEquipmentOperationRequest Request;
        Request.OperationId = FGuid::NewGuid();
        Request.Timestamp = FPlatformTime::Seconds();
        return Request;
    }

    static FEquipmentOperationRequest CreateRequest(EEquipmentOperationType Type,const FInventoryItemInstance& Item,int32 TargetSlot)
    {
        FEquipmentOperationRequest Request;
        Request.OperationId = FGuid::NewGuid();
        Request.OperationType=Type;
        Request.ItemInstance=Item;
        Request.TargetSlotIndex=TargetSlot;
        Request.Timestamp = FPlatformTime::Seconds();
        return Request;
    }

    static FEquipmentOperationRequest CreateSwapRequest(int32 SlotA,int32 SlotB)
    {
        FEquipmentOperationRequest Request;
        Request.OperationId = FGuid::NewGuid();
        Request.OperationType=EEquipmentOperationType::Swap;
        Request.SourceSlotIndex=SlotA;
        Request.TargetSlotIndex=SlotB;
        Request.Timestamp = FPlatformTime::Seconds();
        return Request;
    }

    bool IsValid() const
    {
        return OperationType!=EEquipmentOperationType::None && OperationId.IsValid();
    }

    FString GetDescription() const
    {
        return FString::Printf(TEXT("Op[%s]: Type=%s, Target=%d, Priority=%d"),
            *OperationId.ToString(),
            *UEnum::GetValueAsString(OperationType),
            TargetSlotIndex,
            (int32)Priority);
    }
};

USTRUCT(BlueprintType)
struct MEDCOMSHARED_API FEquipmentOperationResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite,Category="Result") bool bSuccess=false;
    UPROPERTY(BlueprintReadWrite,Category="Result") FText ErrorMessage;
    UPROPERTY(BlueprintReadWrite,Category="Result") EEquipmentValidationFailure FailureType=EEquipmentValidationFailure::None;
    UPROPERTY(BlueprintReadWrite,Category="Result") FGuid OperationId;
    UPROPERTY(BlueprintReadWrite,Category="Result") FGuid TransactionId;
    UPROPERTY(BlueprintReadWrite,Category="Result") TArray<int32> AffectedSlots;
    UPROPERTY(BlueprintReadWrite,Category="Result") TArray<FInventoryItemInstance> AffectedItems;
    UPROPERTY(BlueprintReadWrite,Category="Result") TMap<FString,FString> ResultMetadata;
    UPROPERTY(BlueprintReadWrite,Category="Result") float ExecutionTime=0.0f;
    UPROPERTY(BlueprintReadWrite,Category="Result") TArray<FText> Warnings;

    static FEquipmentOperationResult CreateSuccess(const FGuid& OpId)
    {
        FEquipmentOperationResult R;R.bSuccess=true;R.OperationId=OpId;return R;
    }

    static FEquipmentOperationResult CreateFailure(const FGuid& OpId,const FText& Error,EEquipmentValidationFailure Failure=EEquipmentValidationFailure::SystemError)
    {
        FEquipmentOperationResult R;R.bSuccess=false;R.OperationId=OpId;R.ErrorMessage=Error;R.FailureType=Failure;return R;
    }
};

USTRUCT(BlueprintType)
struct MEDCOMSHARED_API FSlotValidationResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite,Category="Validation") bool bIsValid=false;
    UPROPERTY(BlueprintReadWrite,Category="Validation") FText ErrorMessage;
    UPROPERTY(BlueprintReadWrite,Category="Validation") EEquipmentValidationFailure FailureType=EEquipmentValidationFailure::None;
    UPROPERTY(BlueprintReadWrite,Category="Validation") FGameplayTag ErrorTag;
    UPROPERTY(BlueprintReadWrite,Category="Validation") TArray<FText> Warnings;
    UPROPERTY(BlueprintReadWrite,Category="Validation") float ConfidenceScore=1.0f;
    UPROPERTY(BlueprintReadWrite,Category="Validation") bool bCanOverride=false;
    UPROPERTY(BlueprintReadWrite,Category="Validation") TMap<FString,FString> Context;

    static FSlotValidationResult Success(){FSlotValidationResult R;R.bIsValid=true;R.ConfidenceScore=1.0f;return R;}
    static FSlotValidationResult Failure(const FText& Error,EEquipmentValidationFailure Failure=EEquipmentValidationFailure::SystemError,const FGameplayTag& Tag=FGameplayTag())
    {FSlotValidationResult R;R.bIsValid=false;R.ErrorMessage=Error;R.FailureType=Failure;R.ErrorTag=Tag;R.ConfidenceScore=0.0f;return R;}
    static FSlotValidationResult Warning(const FText& WarningText)
    {FSlotValidationResult R;R.bIsValid=true;R.Warnings.Add(WarningText);R.ConfidenceScore=0.8f;R.bCanOverride=true;return R;}
};

USTRUCT(BlueprintType)
struct MEDCOMSHARED_API FEquipmentSlotSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite,Category="Snapshot") 
    int32 SlotIndex=INDEX_NONE;
    
    UPROPERTY(BlueprintReadWrite,Category="Snapshot") 
    FInventoryItemInstance ItemInstance;
    
    UPROPERTY(BlueprintReadWrite,Category="Snapshot") 
    FEquipmentSlotConfig Configuration;
    
    UPROPERTY(BlueprintReadWrite,Category="Snapshot") 
    FDateTime Timestamp;
    
    UPROPERTY(BlueprintReadWrite,Category="Snapshot") 
    FGuid SnapshotId;
    
    UPROPERTY(BlueprintReadWrite,Category="Snapshot") 
    TMap<FString,FString> Metadata;

    /** Static factory method to create snapshot with generated ID */
    static FEquipmentSlotSnapshot Create()
    {
        FEquipmentSlotSnapshot Snapshot;
        Snapshot.SnapshotId = FGuid::NewGuid();
        Snapshot.Timestamp = FDateTime::Now();
        return Snapshot;
    }
    
    /** Static factory method to create snapshot with specific ID */
    static FEquipmentSlotSnapshot CreateWithID(const FGuid& InSnapshotId)
    {
        FEquipmentSlotSnapshot Snapshot;
        Snapshot.SnapshotId = InSnapshotId;
        Snapshot.Timestamp = FDateTime::Now();
        return Snapshot;
    }
};

USTRUCT(BlueprintType)
struct MEDCOMSHARED_API FEquipmentStateSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite,Category="Snapshot") 
    TArray<FEquipmentSlotSnapshot> SlotSnapshots;
    
    UPROPERTY(BlueprintReadWrite,Category="Snapshot") 
    int32 ActiveWeaponSlotIndex=INDEX_NONE;
    
    UPROPERTY(BlueprintReadWrite,Category="Snapshot") 
    int32 PreviousWeaponSlotIndex=INDEX_NONE;
    
    UPROPERTY(BlueprintReadWrite,Category="Snapshot") 
    EEquipmentState CurrentState=EEquipmentState::Idle;
    
    UPROPERTY(BlueprintReadWrite,Category="Snapshot") 
    FGameplayTag CurrentStateTag;
    
    UPROPERTY(BlueprintReadWrite,Category="Snapshot") 
    FGuid SnapshotId;
    
    FDateTime Timestamp;
    uint32 Version=1;
    
    UPROPERTY(BlueprintReadWrite,Category="Snapshot") 
    TMap<FString,FString> StateData;

    /** Static factory method to create snapshot with generated ID */
    static FEquipmentStateSnapshot Create()
    {
        FEquipmentStateSnapshot Snapshot;
        Snapshot.SnapshotId = FGuid::NewGuid();
        Snapshot.Timestamp = FDateTime::Now();
        return Snapshot;
    }
    
    /** Static factory method to create snapshot with specific ID */
    static FEquipmentStateSnapshot CreateWithID(const FGuid& InSnapshotId)
    {
        FEquipmentStateSnapshot Snapshot;
        Snapshot.SnapshotId = InSnapshotId;
        Snapshot.Timestamp = FDateTime::Now();
        return Snapshot;
    }
    
    bool IsValid() const 
    {
        return SnapshotId.IsValid();
    }
};

USTRUCT(BlueprintType)
struct MEDCOMSHARED_API FEquipmentTransaction
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite,Category="Transaction") 
    FGuid TransactionId;
    
    UPROPERTY(BlueprintReadWrite,Category="Transaction") 
    ETransactionState State=ETransactionState::None;
    
    UPROPERTY(BlueprintReadWrite,Category="Transaction") 
    TArray<FEquipmentOperationRequest> Operations;
    
    UPROPERTY(BlueprintReadWrite,Category="Transaction") 
    TArray<FGuid> OperationIds;
    
    UPROPERTY(BlueprintReadWrite,Category="Transaction") 
    FEquipmentStateSnapshot StateBefore;
    
    UPROPERTY(BlueprintReadWrite,Category="Transaction") 
    FEquipmentStateSnapshot StateAfter;
    
    UPROPERTY(BlueprintReadWrite,Category="Transaction") 
    FDateTime StartTime;
    
    UPROPERTY(BlueprintReadWrite,Category="Transaction") 
    FDateTime EndTime;
    
    UPROPERTY(BlueprintReadWrite,Category="Transaction") 
    bool bIsCommitted=false;
    
    UPROPERTY(BlueprintReadWrite,Category="Transaction") 
    bool bIsRolledBack=false;
    
    UPROPERTY(BlueprintReadWrite,Category="Transaction") 
    bool bIsNested=false;
    
    UPROPERTY(BlueprintReadWrite,Category="Transaction") 
    FGuid ParentTransactionId;
    
    UPROPERTY(BlueprintReadWrite,Category="Transaction") 
    FString Description;

    /** Static factory method to create transaction with generated ID */
    static FEquipmentTransaction Create()
    {
        FEquipmentTransaction Transaction;
        Transaction.TransactionId = FGuid::NewGuid();
        Transaction.StartTime = FDateTime::Now();
        return Transaction;
    }
    
    /** Static factory method to create transaction with specific ID */
    static FEquipmentTransaction CreateWithID(const FGuid& InTransactionId)
    {
        FEquipmentTransaction Transaction;
        Transaction.TransactionId = InTransactionId;
        Transaction.StartTime = FDateTime::Now();
        return Transaction;
    }
    
    bool IsFinalized() const 
    {
        return State==ETransactionState::Committed||State==ETransactionState::RolledBack||State==ETransactionState::Failed;
    }
    
    bool CanModify() const 
    {
        return State==ETransactionState::Active;
    }
};

USTRUCT(BlueprintType)
struct MEDCOMSHARED_API FEquipmentComparisonResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite,Category="Comparison") TMap<FGameplayTag,float> AttributeChanges;
    UPROPERTY(BlueprintReadWrite,Category="Comparison") bool bIsBetter=false;
    UPROPERTY(BlueprintReadWrite,Category="Comparison") float ComparisonScore=0.0f;
    UPROPERTY(BlueprintReadWrite,Category="Comparison") TArray<FText> Improvements;
    UPROPERTY(BlueprintReadWrite,Category="Comparison") TArray<FText> Downgrades;
    UPROPERTY(BlueprintReadWrite,Category="Comparison") TArray<FText> Notes;
};