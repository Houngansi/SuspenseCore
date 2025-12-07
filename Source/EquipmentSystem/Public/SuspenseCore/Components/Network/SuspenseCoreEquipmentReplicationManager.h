// Copyright Suspense Team. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/Equipment/ISuspenseCoreReplicationProvider.h"
#include "Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Engine/NetSerialization.h"
#include "SuspenseCoreEquipmentReplicationManager.generated.h"

class USuspenseCoreEquipmentNetworkService;

USTRUCT()
struct FReplicatedSlotItem : public FFastArraySerializerItem
{
    GENERATED_BODY()
    UPROPERTY() int32 SlotIndex=INDEX_NONE;
    UPROPERTY() FSuspenseCoreInventoryItemInstance ItemInstance;
    UPROPERTY() uint32 ItemVersion=0;
    UPROPERTY() FString ItemHMAC;
    void PreReplicatedRemove(const struct FReplicatedSlotArray& InArraySerializer);
    void PostReplicatedAdd(const struct FReplicatedSlotArray& InArraySerializer);
    void PostReplicatedChange(const struct FReplicatedSlotArray& InArraySerializer);
};

USTRUCT()
struct FReplicatedSlotArray : public FFastArraySerializer
{
    GENERATED_BODY()
    UPROPERTY() TArray<FReplicatedSlotItem> Items;
    UPROPERTY(NotReplicated) class USuspenseCoreEquipmentReplicationManager* OwnerManager=nullptr;
    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms){return FFastArraySerializer::FastArrayDeltaSerialize<FReplicatedSlotItem,FReplicatedSlotArray>(Items,DeltaParms,*this);}
    void PostReplicatedAdd(const TArrayView<int32>& AddedIndices,int32 FinalSize);
    void PostReplicatedChange(const TArrayView<int32>& ChangedIndices,int32 FinalSize);
    void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices,int32 FinalSize);
};
template<> struct TStructOpsTypeTraits<FReplicatedSlotArray> : public TStructOpsTypeTraitsBase2<FReplicatedSlotArray>{enum{WithNetDeltaSerializer=true};};

USTRUCT()
struct FSlotReplicationState
{
    GENERATED_BODY()
    UPROPERTY() int32 SlotIndex=INDEX_NONE;
    UPROPERTY() FSuspenseCoreInventoryItemInstance LastReplicatedItem;
    UPROPERTY() uint32 LastReplicatedVersion=0;
    UPROPERTY() bool bIsDirty=false;
    UPROPERTY() float ReplicationPriority=1.0f;
    UPROPERTY() float LastChangeTime=0.0f;
    UPROPERTY() int32 ChangeCount=0;
};

USTRUCT()
struct FClientReplicationState
{
    GENERATED_BODY()
    UPROPERTY() APlayerController* Client=nullptr;
    UPROPERTY() uint32 LastAcknowledgedVersion=0;
    UPROPERTY() TArray<int32> PendingSlots;
    UPROPERTY() float LastUpdateTime=0.0f;
    UPROPERTY() float ConnectionQuality=1.0f;
    UPROPERTY() int32 ConsecutiveDeltasSent=0;
    UPROPERTY() float AverageLatency=0.0f;
    UPROPERTY() int32 PacketLossCount=0;
};

USTRUCT()
struct FReplicationDeltaMask
{
    GENERATED_BODY()
    UPROPERTY() TArray<uint32> DirtySlotIndices;
    UPROPERTY() uint32 StartVersion=0;
    UPROPERTY() uint32 EndVersion=0;
    UPROPERTY() float Timestamp=0.0f;
    bool HasChanges()const{return DirtySlotIndices.Num()>0;}
    void AddSlot(int32 SlotIndex){DirtySlotIndices.AddUnique(SlotIndex);}
    void Clear(){DirtySlotIndices.Empty();StartVersion=0;EndVersion=0;}
};

USTRUCT()
struct FCompressedReplicationData
{
    GENERATED_BODY()
    UPROPERTY() uint8 CompressionType=0;
    UPROPERTY() TArray<uint8> CompressedBytes;
    UPROPERTY() int32 OriginalSize=0;
    UPROPERTY() uint32 Checksum=0;
    UPROPERTY() FString HMACSignature;
};

USTRUCT(BlueprintType)
struct FReplicationStatistics
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 TotalUpdates=0;
    UPROPERTY(BlueprintReadOnly) int32 DeltaUpdates=0;
    UPROPERTY(BlueprintReadOnly) int32 FullUpdates=0;
    UPROPERTY(BlueprintReadOnly) float BytesSent=0.0f;
    UPROPERTY(BlueprintReadOnly) float BytesSaved=0.0f;
    UPROPERTY(BlueprintReadOnly) float CompressionRatio=1.0f;
    UPROPERTY(BlueprintReadOnly) int32 ActiveClients=0;
    UPROPERTY(BlueprintReadOnly) int32 HMACValidations=0;
    UPROPERTY(BlueprintReadOnly) int32 HMACFailures=0;
};

UCLASS(ClassGroup=(Equipment),meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentReplicationManager : public UActorComponent, public ISuspenseCoreReplicationProvider
{
    GENERATED_BODY()
public:
    USuspenseCoreEquipmentReplicationManager();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const override;
    virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;

    virtual void MarkForReplication(int32 SlotIndex,bool bForceUpdate=false) override;
    virtual FReplicatedEquipmentData GetReplicatedData()const override;
    virtual void ApplyReplicatedData(const FReplicatedEquipmentData& Data,bool bIsInitialReplication=false) override;
    virtual void SetReplicationPolicy(EEquipmentReplicationPolicy Policy) override;
    virtual void ForceFullReplication() override;
    virtual bool ShouldReplicateTo(APlayerController* ViewTarget)const override;
    virtual bool GetReplicationPriority(APlayerController* ViewTarget,float& OutPriority)const override;
    virtual FReplicatedEquipmentData OptimizeReplicationData(const FReplicatedEquipmentData& Data)const override;
    virtual FReplicatedEquipmentData GetReplicationDelta(uint32 LastVersion)const override;
    virtual void OnReplicationCallback(const FName& PropertyName) override;

    UFUNCTION(BlueprintCallable,Category="SuspenseCoreCore|Equipment|Replication")
    bool Initialize(TScriptInterface<ISuspenseCoreEquipmentDataProvider> InDataProvider);
    UFUNCTION(BlueprintCallable,Category="SuspenseCoreCore|Equipment|Replication")
    void SetSecurityService(USuspenseCoreEquipmentNetworkService* InSecurityService);
    UFUNCTION(BlueprintCallable,Category="SuspenseCoreCore|Equipment|Replication")
    void SetRelevancyDistance(float Distance){RelevancyDistance=FMath::Max(100.0f,Distance);}
    UFUNCTION(BlueprintCallable,Category="SuspenseCoreCore|Equipment|Replication")
    void SetCompressionEnabled(bool bEnable){bUseCompression=bEnable;}
    UFUNCTION(BlueprintCallable,Category="SuspenseCoreCore|Equipment|Replication")
    void SetUpdateRate(float UpdatesPerSecond);
    UFUNCTION(BlueprintCallable,Category="SuspenseCoreCore|Equipment|Replication")
    void OnNetworkQualityUpdated(float Quality);
    UFUNCTION(BlueprintCallable,Category="SuspenseCoreCore|Equipment|Replication")
    FReplicationStatistics GetStatistics()const{return Statistics;}

    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDataReplicated,APlayerController*,const FReplicatedEquipmentData&);
    FOnDataReplicated OnDataReplicated;
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnReplicationFailed,APlayerController*,const FText&);
    FOnReplicationFailed OnReplicationFailed;
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnReplicatedStateApplied,const FReplicatedEquipmentData&);
    FOnReplicatedStateApplied OnReplicatedStateApplied;

    friend struct FReplicatedSlotItem;
    friend struct FReplicatedSlotArray;
protected:
    UPROPERTY(ReplicatedUsing=OnRep_SlotArray) FReplicatedSlotArray ReplicatedSlotArray;
    UPROPERTY(ReplicatedUsing=OnRep_Version) uint32 CurrentVersion=0;
    UPROPERTY(ReplicatedUsing=OnRep_ActiveWeaponSlot) int32 ReplicatedActiveWeaponSlot=INDEX_NONE;
    UPROPERTY(ReplicatedUsing=OnRep_EquipmentState) FGameplayTag ReplicatedEquipmentState;
    UPROPERTY(Replicated) FCompressedReplicationData CompressedData;
    UPROPERTY() FReplicationDeltaMask CurrentDeltaMask;

    UFUNCTION() void OnRep_SlotArray();
    UFUNCTION() void OnRep_Version();
    UFUNCTION() void OnRep_ActiveWeaponSlot();
    UFUNCTION() void OnRep_EquipmentState();

    void ProcessReplication();
    void UpdateClientReplication(FClientReplicationState& ClientState);
    FReplicatedEquipmentData BuildReplicationData(APlayerController* Client,bool bForceFull)const;
    FReplicationDeltaMask BuildDeltaMask(uint32 FromVersion,uint32 ToVersion)const;
    FString GenerateSlotHMAC(const FSuspenseCoreInventoryItemInstance& SlotData)const;
    bool VerifySlotHMAC(const FSuspenseCoreInventoryItemInstance& SlotData,const FString& HMACSignature)const;
    FCompressedReplicationData CompressData(const FReplicatedEquipmentData& Data)const;
    bool DecompressData(const FCompressedReplicationData& Compressed,FReplicatedEquipmentData& OutData)const;
    float CalculateEnhancedRelevancy(APlayerController* ViewTarget)const;
    void UpdateSlotPriority(int32 SlotIndex);
    bool SlotNeedsReplication(int32 SlotIndex,uint32 ClientVersion)const;
    uint32 CalculateChecksum(const FReplicatedEquipmentData& Data)const;
    void OnDataChanged(int32 SlotIndex,const FSuspenseCoreInventoryItemInstance& NewData);
    void CleanupClientStates();
    void UpdateStatistics(int32 BytesSent,bool bWasDelta);
    void AdaptReplicationStrategy(float NetworkQuality);

private:
    UPROPERTY(EditDefaultsOnly,Category="Replication|Config") float RelevancyDistance=5000.0f;
    UPROPERTY(EditDefaultsOnly,Category="Replication|Config") bool bUseCompression=true;
    UPROPERTY(EditDefaultsOnly,Category="Replication|Config") float MinUpdateInterval=0.1f;
    UPROPERTY(EditDefaultsOnly,Category="Replication|Config") int32 MaxDeltasBeforeFull=10;
    UPROPERTY(EditDefaultsOnly,Category="Replication|Config") int32 CompressionThreshold=128;
    UPROPERTY(EditDefaultsOnly,Category="Replication|Config") bool bUseHMACSecurity=true;
    UPROPERTY(EditDefaultsOnly,Category="Replication|Config") bool bUseEnhancedRelevancy=true;

    UPROPERTY() TScriptInterface<ISuspenseCoreEquipmentDataProvider> DataProvider;
    UPROPERTY() USuspenseCoreEquipmentNetworkService* SecurityService=nullptr;

    UPROPERTY() EEquipmentReplicationPolicy CurrentPolicy=EEquipmentReplicationPolicy::OnlyToRelevant;
    UPROPERTY() TArray<FSlotReplicationState> SlotStates;
    TArray<FClientReplicationState> ClientStates;
    TSet<int32> DirtySlots;
    TMap<uint32,FReplicationDeltaMask> VersionHistory;
    bool bForceFullReplication=false;
    float LastReplicationTime=0.0f;
    float UpdateInterval=0.1f;
    float CurrentNetworkQuality=1.0f;
    int32 DynamicMaxDeltasBeforeFull=10;

    mutable FReplicationStatistics Statistics;

    mutable FCriticalSection SlotStateLock;
    mutable FCriticalSection ClientStateLock;
    mutable FCriticalSection DirtySlotLock;
};
