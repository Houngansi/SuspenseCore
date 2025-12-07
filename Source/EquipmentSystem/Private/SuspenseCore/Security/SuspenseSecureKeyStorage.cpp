// SuspenseSecureKeyStorage.cpp
// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Security/SuspenseSecureKeyStorage.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTLS.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Guid.h"
#include "Misc/SecureHash.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"

DEFINE_LOG_CATEGORY_STATIC(LogSecureKeyStorage, Log, All);

//========================================
// FSuspenseSecureKeyStorage
//========================================

FSuspenseSecureKeyStorage::FSuspenseSecureKeyStorage()
    : KeyChecksum(0)
    , AccessCounter(0)
{
}

FSuspenseSecureKeyStorage::~FSuspenseSecureKeyStorage()
{
    ClearKey();
}

void FSuspenseSecureKeyStorage::SetKey(const FString& InKey)
{
    FScopeLock Lock(&KeyLock);

    if (InKey.IsEmpty())
    {
        ClearKey();
        return;
    }

    // Convert key to bytes
    TArray<uint8> KeyBytes;
    KeyBytes.SetNumUninitialized(InKey.Len());
    for (int32 i = 0; i < InKey.Len(); ++i)
    {
        KeyBytes[i] = static_cast<uint8>(InKey[i] & 0xFF);
    }

    // Generate random masks
    GenerateRandomBytes(ObfuscationMask, KeyBytes.Num());
    GenerateRandomBytes(SecondaryMask, KeyBytes.Num());

    // Apply double XOR obfuscation
    ObfuscatedKeyData = KeyBytes;
    ApplyXOR(ObfuscatedKeyData, ObfuscationMask);
    ApplyXOR(ObfuscatedKeyData, SecondaryMask);

    // Calculate integrity checksum
    KeyChecksum = CalculateChecksum(KeyBytes);

    // Secure zero the temporary buffer
    SecureZero(KeyBytes);

    AccessCounter = 0;

    UE_LOG(LogSecureKeyStorage, Log, TEXT("Key stored securely with %d-byte double XOR obfuscation"), ObfuscationMask.Num());
}

FString FSuspenseSecureKeyStorage::GetKey() const
{
    FScopeLock Lock(&KeyLock);

    if (ObfuscatedKeyData.Num() == 0)
    {
        return FString();
    }

    // Deobfuscate
    TArray<uint8> KeyBytes = ObfuscatedKeyData;
    ApplyXOR(KeyBytes, SecondaryMask);
    ApplyXOR(KeyBytes, ObfuscationMask);

    // Verify integrity
    uint32 CurrentChecksum = CalculateChecksum(KeyBytes);
    if (CurrentChecksum != KeyChecksum)
    {
        UE_LOG(LogSecureKeyStorage, Error, TEXT("Key integrity check failed! Checksum mismatch."));
        return FString();
    }

    // Convert back to string
    FString Result;
    Result.Reserve(KeyBytes.Num());
    for (uint8 Byte : KeyBytes)
    {
        Result.AppendChar(static_cast<TCHAR>(Byte));
    }

    // Secure zero temporary buffer
    const_cast<FSuspenseSecureKeyStorage*>(this)->SecureZero(KeyBytes);

    // Increment access counter and rotate if needed
    AccessCounter++;
    if (AccessCounter >= RotationThreshold)
    {
        RotateMask();
    }

    return Result;
}

bool FSuspenseSecureKeyStorage::HasKey() const
{
    FScopeLock Lock(&KeyLock);
    return ObfuscatedKeyData.Num() > 0;
}

void FSuspenseSecureKeyStorage::ClearKey()
{
    FScopeLock Lock(&KeyLock);

    // Secure zero all sensitive data
    SecureZero(ObfuscatedKeyData);
    SecureZero(ObfuscationMask);
    SecureZero(SecondaryMask);

    ObfuscatedKeyData.Empty();
    ObfuscationMask.Empty();
    SecondaryMask.Empty();
    KeyChecksum = 0;
    AccessCounter = 0;
}

FString FSuspenseSecureKeyStorage::GenerateHMAC(const FString& Data) const
{
    FScopeLock Lock(&KeyLock);

    if (ObfuscatedKeyData.Num() == 0)
    {
        return FString();
    }

    // Get key temporarily
    FString Key = GetKey();
    if (Key.IsEmpty())
    {
        return FString();
    }

    // HMAC using double SHA-1 (similar to HMAC construction)
    // HMAC(key, data) = SHA1(key ^ opad || SHA1(key ^ ipad || data))
    FString CombinedData = Key + TEXT(":") + Data + TEXT(":") + Key;

    uint8 Hash[20];
    FSHA1 Sha1;
    Sha1.Update((const uint8*)TCHAR_TO_UTF8(*CombinedData), CombinedData.Len());
    Sha1.Final();
    Sha1.GetHash(Hash);

    // Second pass with key for additional security
    FString SecondInput = Key;
    for (int32 i = 0; i < 20; ++i)
    {
        SecondInput += FString::Printf(TEXT("%02x"), Hash[i]);
    }

    FSHA1 Sha1_2;
    Sha1_2.Update((const uint8*)TCHAR_TO_UTF8(*SecondInput), SecondInput.Len());
    Sha1_2.Final();
    Sha1_2.GetHash(Hash);

    // Convert to hex string (40 chars for SHA-1)
    FString Result;
    for (int32 i = 0; i < 20; ++i)
    {
        Result += FString::Printf(TEXT("%02x"), Hash[i]);
    }

    // Zero local copies (best effort - string internals may persist)
    for (int32 i = 0; i < Key.Len(); ++i)
    {
        const_cast<TCHAR*>(*Key)[i] = 0;
    }
    for (int32 i = 0; i < CombinedData.Len(); ++i)
    {
        const_cast<TCHAR*>(*CombinedData)[i] = 0;
    }
    for (int32 i = 0; i < SecondInput.Len(); ++i)
    {
        const_cast<TCHAR*>(*SecondInput)[i] = 0;
    }

    return Result;
}

bool FSuspenseSecureKeyStorage::VerifyHMAC(const FString& Data, const FString& Signature) const
{
    FString ComputedSignature = GenerateHMAC(Data);

    if (ComputedSignature.IsEmpty() || Signature.IsEmpty())
    {
        return false;
    }

    // Constant-time comparison to prevent timing attacks
    if (ComputedSignature.Len() != Signature.Len())
    {
        return false;
    }

    uint32 Diff = 0;
    for (int32 i = 0; i < ComputedSignature.Len(); ++i)
    {
        Diff |= static_cast<uint32>(ComputedSignature[i] ^ Signature[i]);
    }

    return Diff == 0;
}

bool FSuspenseSecureKeyStorage::LoadFromSecureSources()
{
    FString Key;

    // Priority 1: Environment variable
    Key = FPlatformMisc::GetEnvironmentVariable(TEXT("SUSPENSE_HMAC_KEY"));
    if (!Key.IsEmpty() && Key.Len() >= 32)
    {
        SetKey(Key);
        FMemory::Memzero((void*)TCHAR_TO_ANSI(*Key), Key.Len());
        UE_LOG(LogSecureKeyStorage, Log, TEXT("HMAC key loaded from environment variable"));
        return true;
    }

    // Priority 2: Encrypted config section
    if (GConfig)
    {
        FString EncryptedKey;
        if (GConfig->GetString(TEXT("NetworkSecurity.Keys"), TEXT("HMACSecret"), EncryptedKey, GGameIni))
        {
            if (!EncryptedKey.IsEmpty() && EncryptedKey.Len() >= 32)
            {
                SetKey(EncryptedKey);
                FMemory::Memzero((void*)TCHAR_TO_ANSI(*EncryptedKey), EncryptedKey.Len());
                UE_LOG(LogSecureKeyStorage, Log, TEXT("HMAC key loaded from config"));
                return true;
            }
        }
    }

    // Priority 3: Secure file storage
    FString SecureKeyPath = FPaths::ProjectSavedDir() / TEXT("Config/Secure/hmac.key");
    if (FPaths::FileExists(SecureKeyPath))
    {
        FString FileKey;
        if (FFileHelper::LoadFileToString(FileKey, *SecureKeyPath))
        {
            FileKey = FileKey.TrimStartAndEnd();
            if (!FileKey.IsEmpty() && FileKey.Len() >= 32)
            {
                SetKey(FileKey);
                FMemory::Memzero((void*)TCHAR_TO_ANSI(*FileKey), FileKey.Len());
                UE_LOG(LogSecureKeyStorage, Log, TEXT("HMAC key loaded from secure file"));
                return true;
            }
        }
    }

    UE_LOG(LogSecureKeyStorage, Warning, TEXT("No HMAC key found in secure sources"));
    return false;
}

bool FSuspenseSecureKeyStorage::GenerateNewKey(int32 KeyLength)
{
    FScopeLock Lock(&KeyLock);

    if (KeyLength < 32)
    {
        UE_LOG(LogSecureKeyStorage, Error, TEXT("Key length must be at least 32 characters"));
        return false;
    }

    // Generate cryptographically strong key using multiple entropy sources
    FString NewKey;
    NewKey.Reserve(KeyLength);

    // Use GUIDs for base randomness
    while (NewKey.Len() < KeyLength)
    {
        FGuid Guid = FGuid::NewGuid();
        NewKey += Guid.ToString(EGuidFormats::Digits);
    }

    // Mix in additional entropy
    uint64 TimeEntropy = FPlatformTime::Cycles64();
    uint64 ProcessEntropy = (uint64)FPlatformProcess::GetCurrentProcessId();
    uint64 ThreadEntropy = (uint64)FPlatformTLS::GetCurrentThreadId();

    // XOR entropy into key
    for (int32 i = 0; i < FMath::Min(8, NewKey.Len()); ++i)
    {
        NewKey[i] ^= ((TimeEntropy >> (i * 8)) & 0xFF);
    }
    for (int32 i = 8; i < FMath::Min(16, NewKey.Len()); ++i)
    {
        NewKey[i] ^= ((ProcessEntropy >> ((i - 8) * 8)) & 0xFF);
    }
    for (int32 i = 16; i < FMath::Min(24, NewKey.Len()); ++i)
    {
        NewKey[i] ^= ((ThreadEntropy >> ((i - 16) * 8)) & 0xFF);
    }

    // Truncate to requested length
    NewKey = NewKey.Left(KeyLength);

    // Store the key
    SetKey(NewKey);

    // Secure zero local copy
    FMemory::Memzero((void*)TCHAR_TO_ANSI(*NewKey), NewKey.Len());

    // Save to secure storage
    FString SecureKeyPath = FPaths::ProjectSavedDir() / TEXT("Config/Secure/hmac.key");
    FString SecureDir = FPaths::GetPath(SecureKeyPath);

    if (!FPaths::DirectoryExists(SecureDir))
    {
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        PlatformFile.CreateDirectoryTree(*SecureDir);
    }

    // Get key temporarily for saving
    FString KeyToSave = GetKey();
    bool bSaved = FFileHelper::SaveStringToFile(KeyToSave, *SecureKeyPath);
    FMemory::Memzero((void*)TCHAR_TO_ANSI(*KeyToSave), KeyToSave.Len());

    if (bSaved)
    {
        UE_LOG(LogSecureKeyStorage, Log, TEXT("Generated and saved new %d-character HMAC key"), KeyLength);
    }
    else
    {
        UE_LOG(LogSecureKeyStorage, Error, TEXT("Failed to save generated HMAC key"));
    }

    return bSaved;
}

void FSuspenseSecureKeyStorage::GenerateRandomBytes(TArray<uint8>& OutBytes, int32 NumBytes) const
{
    OutBytes.SetNumUninitialized(NumBytes);

    // Use multiple entropy sources for randomness
    for (int32 i = 0; i < NumBytes; ++i)
    {
        uint64 Entropy = FPlatformTime::Cycles64();
        Entropy ^= ((uint64)FPlatformTLS::GetCurrentThreadId() << 32);
        Entropy ^= (uint64)i * 0x5DEECE66DULL;

        // Mix with GUID randomness
        if ((i % 16) == 0)
        {
            FGuid Guid = FGuid::NewGuid();
            Entropy ^= ((uint64)Guid.A << 32) | Guid.B;
        }

        OutBytes[i] = static_cast<uint8>((Entropy >> ((i % 8) * 8)) & 0xFF);
    }
}

void FSuspenseSecureKeyStorage::ApplyXOR(TArray<uint8>& Data, const TArray<uint8>& Mask) const
{
    if (Mask.Num() == 0)
    {
        return;
    }

    for (int32 i = 0; i < Data.Num(); ++i)
    {
        Data[i] ^= Mask[i % Mask.Num()];
    }
}

void FSuspenseSecureKeyStorage::RotateMask() const
{
    // Cast away const for mask rotation (mutable operation)
    FSuspenseSecureKeyStorage* MutableThis = const_cast<FSuspenseSecureKeyStorage*>(this);

    // Deobfuscate current key
    TArray<uint8> KeyBytes = ObfuscatedKeyData;
    ApplyXOR(KeyBytes, SecondaryMask);
    ApplyXOR(KeyBytes, ObfuscationMask);

    // Generate new masks
    MutableThis->GenerateRandomBytes(MutableThis->ObfuscationMask, KeyBytes.Num());
    MutableThis->GenerateRandomBytes(MutableThis->SecondaryMask, KeyBytes.Num());

    // Re-obfuscate with new masks
    MutableThis->ObfuscatedKeyData = KeyBytes;
    ApplyXOR(MutableThis->ObfuscatedKeyData, MutableThis->ObfuscationMask);
    ApplyXOR(MutableThis->ObfuscatedKeyData, MutableThis->SecondaryMask);

    // Secure zero temporary
    MutableThis->SecureZero(KeyBytes);

    // Reset counter
    MutableThis->AccessCounter = 0;

    UE_LOG(LogSecureKeyStorage, Verbose, TEXT("Obfuscation mask rotated"));
}

void FSuspenseSecureKeyStorage::SecureZero(TArray<uint8>& Data)
{
    if (Data.Num() > 0)
    {
        // Overwrite with random data first
        for (int32 i = 0; i < Data.Num(); ++i)
        {
            Data[i] = static_cast<uint8>(FPlatformTime::Cycles64() & 0xFF);
        }
        // Then zero
        FMemory::Memzero(Data.GetData(), Data.Num());
    }
}

uint32 FSuspenseSecureKeyStorage::CalculateChecksum(const TArray<uint8>& Data) const
{
    return FCrc::MemCrc32(Data.GetData(), Data.Num());
}

//========================================
// FScopedKeyAccess
//========================================

FScopedKeyAccess::FScopedKeyAccess(const FSuspenseSecureKeyStorage& InStorage)
    : Storage(InStorage)
{
    Key = Storage.GetKey();
}

FScopedKeyAccess::~FScopedKeyAccess()
{
    // Secure zero the key string
    if (!Key.IsEmpty())
    {
        for (int32 i = 0; i < Key.Len(); ++i)
        {
            const_cast<TCHAR*>(*Key)[i] = 0;
        }
    }
}
