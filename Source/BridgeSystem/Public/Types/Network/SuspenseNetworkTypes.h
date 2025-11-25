// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Equipment/SuspenseEquipmentTypes.h"
#include "Engine/NetSerialization.h"
#include "Misc/Crc.h"             // FCrc::MemCrc32
#include "Misc/SecureHash.h"      // FSHA1::HashBuffer
#include "Misc/CString.h"         // BytesToHex overloads
#include "MedComNetworkTypes.generated.h"

/**
 * Network operation priority
 */
UENUM(BlueprintType)
enum class ENetworkOperationPriority : uint8
{
    Low = 0,
    Normal,
    High,
    Critical
};

/**
 * Network operation types for equipment system
 */
UENUM(BlueprintType)
enum class ENetworkOperationType : uint8
{
    None = 0,
    Equip,
    Unequip,
    Swap,
    Move,
    Drop,
    QuickSwitch,
    Reload,
    Inspect,
    Repair,
    Upgrade
};

/**
 * Network reliability mode
 */
UENUM(BlueprintType)
enum class ENetworkReliability : uint8
{
    Unreliable = 0,
    UnreliableOrdered,
    Reliable,
    ReliableOrdered,
    ReliableUnordered
};

/**
 * Network prediction mode
 */
UENUM(BlueprintType)
enum class EPredictionMode : uint8
{
    None = 0,
    ClientSide,
    ServerAuthority,
    Hybrid
};

/**
 * Compressed item data for network transmission
 */
USTRUCT()
struct FCompressedItemData
{
    GENERATED_BODY()

    UPROPERTY()
    uint16 ItemID = 0;

    UPROPERTY()
    uint8 Quantity = 1;

    UPROPERTY()
    uint8 SlotIndex = 255;

    UPROPERTY()
    uint32 InstanceID = 0;

    UPROPERTY()
    TArray<uint8> CompressedProperties;

    FCompressedItemData() = default;

    explicit FCompressedItemData(const FSuspenseInventoryItemInstance& Instance, int32 Slot)
    {
        // NOTE: Key hashing must be deterministic across builds; FName hashing is stable in-process.
        ItemID = static_cast<uint16>(GetTypeHash(Instance.ItemID));
        Quantity = static_cast<uint8>(FMath::Clamp(Instance.Quantity, 0, 255));
        SlotIndex = static_cast<uint8>(FMath::Clamp(Slot, 0, 255));
        InstanceID = GetTypeHash(Instance.InstanceID);
        CompressProperties(Instance.RuntimeProperties);
    }

    FSuspenseInventoryItemInstance Decompress() const
    {
        FSuspenseInventoryItemInstance Result;
        // Minimal restore; real reconstruction depends on your SSOT/catalog.
        Result.Quantity = Quantity;
        return Result;
    }

private:
    void CompressProperties(const TMap<FName, float>& Properties)
    {
        // Simple/naive compression: (KeyHash16, Value*100 -> int16)
        CompressedProperties.Reset(Properties.Num() * 4);
        for (const TPair<FName, float>& Pair : Properties)
        {
            const uint16 KeyHash = static_cast<uint16>(GetTypeHash(Pair.Key));
            CompressedProperties.Add(static_cast<uint8>(KeyHash >> 8));
            CompressedProperties.Add(static_cast<uint8>(KeyHash & 0xFF));

            const int16 CompressedValue = static_cast<int16>(FMath::Clamp(FMath::RoundToInt(Pair.Value * 100.0f), -32768, 32767));
            CompressedProperties.Add(static_cast<uint8>(CompressedValue >> 8));
            CompressedProperties.Add(static_cast<uint8>(CompressedValue & 0xFF));
        }
    }
};

/**
 * Network operation request with enhanced security features
 *
 * Security notes:
 * - Integrity: CRC32 with per-session salt.
 * - Signature: SHA1(Key || '|' || Data). Named HMACSignature for backward compatibility.
 * - Replay protection: Nonce + ClientTimestamp validation.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FNetworkOperationRequest
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid RequestId;

    UPROPERTY()
    FEquipmentOperationRequest Operation;

    UPROPERTY()
    ENetworkOperationPriority Priority = ENetworkOperationPriority::Normal;

    UPROPERTY()
    float Timestamp = 0.0f;

    UPROPERTY()
    int32 RetryCount = 0;

    UPROPERTY()
    bool bRequiresConfirmation = true;

    // Security fields
    UPROPERTY()
    uint64 Nonce = 0;

    UPROPERTY()
    uint32 Checksum = 0;

    UPROPERTY()
    float ClientTimestamp = 0.0f;

    // Kept name for API compatibility; actually SHA1-based signature.
    UPROPERTY()
    FString HMACSignature;

    // Session salt for checksum mixing
    static constexpr uint32 ChecksumSalt = 0xDEADBEEF;

    /** Calculates salted CRC32 over critical fields. */
    uint32 CalculateChecksum() const
    {
        TArray<uint8> DataBuffer;
        DataBuffer.Reserve(64);

        // Serialize critical fields into a byte buffer (packed and stable)
        {
            // RequestId
            DataBuffer.Append(reinterpret_cast<const uint8*>(&RequestId), sizeof(FGuid));

            // OperationId + OpType + TargetSlot
            const FGuid OpId = Operation.OperationId;
            DataBuffer.Append(reinterpret_cast<const uint8*>(&OpId), sizeof(FGuid));

            const uint8 OpType = static_cast<uint8>(Operation.OperationType);
            DataBuffer.Add(OpType);

            const int32 TargetSlot = Operation.TargetSlotIndex;
            DataBuffer.Append(reinterpret_cast<const uint8*>(&TargetSlot), sizeof(int32));

            // Nonce + client ts
            DataBuffer.Append(reinterpret_cast<const uint8*>(&Nonce), sizeof(uint64));
            DataBuffer.Append(reinterpret_cast<const uint8*>(&ClientTimestamp), sizeof(float));
        }

        uint32 Calculated = FCrc::MemCrc32(DataBuffer.GetData(), DataBuffer.Num());
        Calculated ^= ChecksumSalt;
        return Calculated;
    }

    /** Updates the stored checksum. Call before send. */
    void UpdateChecksum()
    {
        Checksum = CalculateChecksum();
    }

    /** Validates integrity against stored checksum. */
    bool ValidateIntegrity() const
    {
        const uint32 Expected = CalculateChecksum();
        const bool bOk = (Expected == Checksum);
        if (!bOk)
        {
            UE_LOG(LogTemp, Warning, TEXT("NetworkOperationRequest checksum mismatch. Expected=%u, Got=%u"), Expected, Checksum);
        }
        return bOk;
    }

    /**
     * Generates a SHA1-based signature of the request.
     * NOTE: Not a true HMAC; replace with platform HMAC(SHA-256) when available.
     */
    FString GenerateHMAC(const FString& SecretKey) const
    {
        // Compose stable string
        const FString DataToSign = FString::Printf(
            TEXT("%s|%llu|%.6f|%d|%d"),
            *RequestId.ToString(),
            static_cast<unsigned long long>(Nonce),
            static_cast<double>(ClientTimestamp),
            static_cast<int32>(Operation.OperationType),
            Operation.TargetSlotIndex
        );

        // Bytes: Key + '|' + Data
        FTCHARToUTF8 KeyUtf8(*SecretKey);
        FTCHARToUTF8 DataUtf8(*DataToSign);

        const int32 TotalLen = KeyUtf8.Length() + 1 + DataUtf8.Length();
        TArray<uint8> Buffer;
        Buffer.AddUninitialized(TotalLen);

        FMemory::Memcpy(Buffer.GetData(), KeyUtf8.Get(), KeyUtf8.Length());
        Buffer[KeyUtf8.Length()] = static_cast<uint8>('|');
        FMemory::Memcpy(Buffer.GetData() + KeyUtf8.Length() + 1, DataUtf8.Get(), DataUtf8.Length());

        // SHA1 digest
        uint8 Digest[20] = {0};
        FSHA1::HashBuffer(Buffer.GetData(), Buffer.Num(), Digest);

        // To hex (UE overload requiring FString&)
        FString Hex;
        BytesToHex(Digest, 20, Hex);
        return Hex;
    }

    /** Verifies stored signature against freshly generated one. */
    bool VerifyHMAC(const FString& SecretKey) const
    {
        if (HMACSignature.IsEmpty())
        {
            // No signature; accept non-critical, require for Critical
            return Priority != ENetworkOperationPriority::Critical;
        }
        const FString Expected = GenerateHMAC(SecretKey);
        return HMACSignature.Equals(Expected, ESearchCase::CaseSensitive);
    }
};

/**
 * Network operation response
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FNetworkOperationResponse
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid RequestId;

    UPROPERTY()
    bool bSuccess = false;

    UPROPERTY()
    FEquipmentOperationResult Result;

    UPROPERTY()
    float ServerTimestamp = 0.0f;

    UPROPERTY()
    float Latency = 0.0f;
};

/**
 * Network RPC data packet
 */
USTRUCT()
struct FEquipmentRPCPacket
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid PacketId;

    UPROPERTY()
    ENetworkOperationType OperationType = ENetworkOperationType::None;

    UPROPERTY()
    FCompressedItemData ItemData;

    UPROPERTY()
    uint8 SourceSlot = 255;

    UPROPERTY()
    uint8 TargetSlot = 255;

    UPROPERTY()
    float Timestamp = 0.0f;

    UPROPERTY()
    uint32 SequenceNumber = 0;

    UPROPERTY()
    uint16 Checksum = 0;

    /** Custom net serializer to minimize bandwidth. */
    bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
    {
        bOutSuccess = true;

        Ar << PacketId;

        // Enum as uint8 for stability
        if (Ar.IsSaving())
        {
            uint8 Op = static_cast<uint8>(OperationType);
            Ar << Op;
        }
        else
        {
            uint8 Op = 0;
            Ar << Op;
            OperationType = static_cast<ENetworkOperationType>(Op);
        }

        Ar << SourceSlot;
        Ar << TargetSlot;
        Ar << Timestamp;
        Ar << SequenceNumber;

        // Serialize compressed data payload
        if (Ar.IsSaving())
        {
            const uint16 DataSize = static_cast<uint16>(FMath::Min(ItemData.CompressedProperties.Num(), 0xFFFF));
            Ar << const_cast<uint16&>(DataSize);
            for (int32 i = 0; i < DataSize; ++i)
            {
                uint8 Byte = ItemData.CompressedProperties[i];
                Ar << Byte;
            }
        }
        else
        {
            uint16 DataSize = 0;
            Ar << DataSize;
            ItemData.CompressedProperties.SetNum(DataSize);
            for (int32 i = 0; i < DataSize; ++i)
            {
                Ar << ItemData.CompressedProperties[i];
            }
        }

        Ar << Checksum;

        return bOutSuccess;
    }
};

template<>
struct TStructOpsTypeTraits<FEquipmentRPCPacket> : public TStructOpsTypeTraitsBase2<FEquipmentRPCPacket>
{
    enum
    {
        WithNetSerializer = true,
        WithNetSharedSerialization = true,
    };
};

/**
 * Prediction data for client-side prediction
 */
USTRUCT()
struct FEquipmentPredictionData
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid PredictionId;

    UPROPERTY()
    FEquipmentRPCPacket OriginalPacket;

    UPROPERTY()
    FEquipmentStateSnapshot StateBefore;

    UPROPERTY()
    FEquipmentStateSnapshot PredictedState;

    UPROPERTY()
    float PredictionTime = 0.0f;

    UPROPERTY()
    bool bConfirmed = false;

    UPROPERTY()
    bool bRolledBack = false;
};

/**
 * Network synchronization data
 */
USTRUCT()
struct FEquipmentSyncData
{
    GENERATED_BODY()

    UPROPERTY()
    uint32 SyncVersion = 0;

    UPROPERTY()
    TArray<FCompressedItemData> Items;

    UPROPERTY()
    uint8 ActiveWeaponSlot = 255;

    UPROPERTY()
    FGameplayTag CurrentState;

    UPROPERTY()
    float LastSyncTime = 0.0f;

    UPROPERTY()
    uint16 Checksum = 0;

    uint16 CalculateChecksum() const
    {
        uint32 Hash = SyncVersion;
        Hash = HashCombine(Hash, static_cast<uint32>(GetTypeHash(CurrentState)));
        Hash = HashCombine(Hash, static_cast<uint32>(ActiveWeaponSlot));

        for (const FCompressedItemData& Item : Items)
        {
            Hash = HashCombine(Hash, static_cast<uint32>(Item.ItemID));
            Hash = HashCombine(Hash, static_cast<uint32>(Item.Quantity));
            Hash = HashCombine(Hash, static_cast<uint32>(Item.SlotIndex));
            Hash = HashCombine(Hash, static_cast<uint32>(Item.InstanceID));
        }

        return static_cast<uint16>(Hash & 0xFFFF);
    }

    bool Validate() const
    {
        return CalculateChecksum() == Checksum;
    }
};

/**
 * Network latency compensation data
 */
USTRUCT()
struct BRIDGESYSTEM_API FLatencyCompensationData
{
    GENERATED_BODY()

    UPROPERTY()
    float EstimatedLatency = 0.0f;

    UPROPERTY()
    float ServerTime = 0.0f;

    UPROPERTY()
    float ClientTime = 0.0f;

    UPROPERTY()
    float TimeDilation = 1.0f;

    UPROPERTY()
    int32 PacketLoss = 0;

    float GetCompensatedTime() const
    {
        // Simple midpoint estimator: ServerTime + RTT/2
        return ServerTime + EstimatedLatency * 0.5f;
    }
};
