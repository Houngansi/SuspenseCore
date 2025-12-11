// SuspenseSecureKeyStorage.h
// Copyright SuspenseCore Team. All Rights Reserved.
//
// Secure key storage with memory obfuscation for HMAC and cryptographic keys.
// Prevents plain-text key exposure in memory dumps.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Misc/SecureHash.h"

/**
 * Secure key storage with XOR obfuscation and key rotation.
 *
 * Security Features:
 * - XOR obfuscation with random mask (prevents plain-text in memory)
 * - Key segmentation (key split into multiple parts)
 * - Automatic mask rotation on access
 * - Secure zeroing on destruction
 * - Thread-safe operations
 */
class EQUIPMENTSYSTEM_API FSuspenseSecureKeyStorage
{
public:
    FSuspenseSecureKeyStorage();
    ~FSuspenseSecureKeyStorage();

    // Non-copyable, non-movable to prevent key leakage
    FSuspenseSecureKeyStorage(const FSuspenseSecureKeyStorage&) = delete;
    FSuspenseSecureKeyStorage& operator=(const FSuspenseSecureKeyStorage&) = delete;
    FSuspenseSecureKeyStorage(FSuspenseSecureKeyStorage&&) = delete;
    FSuspenseSecureKeyStorage& operator=(FSuspenseSecureKeyStorage&&) = delete;

    /**
     * Store a key securely with obfuscation
     * @param InKey The key to store (will be obfuscated immediately)
     */
    void SetKey(const FString& InKey);

    /**
     * Retrieve the deobfuscated key (temporary exposure)
     * @return The original key string
     * @note Key is re-obfuscated with new mask after retrieval
     */
    FString GetKey() const;

    /**
     * Check if a valid key is stored
     * @return true if key is set and non-empty
     */
    bool HasKey() const;

    /**
     * Securely clear the stored key
     * Memory is overwritten before deallocation
     */
    void ClearKey();

    /**
     * Generate HMAC signature using the stored key
     * @param Data The data to sign
     * @return HMAC-SHA256 signature as hex string
     */
    FString GenerateHMAC(const FString& Data) const;

    /**
     * Verify HMAC signature using the stored key
     * @param Data The original data
     * @param Signature The signature to verify
     * @return true if signature matches
     */
    bool VerifyHMAC(const FString& Data, const FString& Signature) const;

    /**
     * Load key from secure sources (environment, config, file)
     * @return true if key was loaded successfully
     */
    bool LoadFromSecureSources();

    /**
     * Generate a new cryptographically secure key
     * @param KeyLength Desired key length in characters (default 64)
     * @return true if key was generated successfully
     */
    bool GenerateNewKey(int32 KeyLength = 64);

    /**
     * Alias for GenerateNewKey for compatibility
     * @param KeyLength Desired key length in bytes (default 32)
     * @return true if key was generated successfully
     */
    bool GenerateKey(int32 KeyLength = 32) { return GenerateNewKey(KeyLength); }

    /**
     * Get the raw key bytes (for HMAC computation)
     * @param OutBytes Array to receive the key bytes
     */
    void GetKeyBytes(TArray<uint8>& OutBytes) const;

    /**
     * Load key from file
     * @param FilePath Path to the key file
     * @return true if key was loaded successfully
     */
    bool LoadFromFile(const FString& FilePath);

    /**
     * Save key to file
     * @param FilePath Path to save the key
     * @return true if key was saved successfully
     */
    bool SaveToFile(const FString& FilePath) const;

private:
    /** XOR mask for obfuscation */
    TArray<uint8> ObfuscationMask;

    /** Obfuscated key data (XOR'd with mask) */
    TArray<uint8> ObfuscatedKeyData;

    /** Secondary mask for additional layer */
    TArray<uint8> SecondaryMask;

    /** Key integrity checksum */
    uint32 KeyChecksum;

    /** Thread safety */
    mutable FCriticalSection KeyLock;

    /** Access counter for rotation timing */
    mutable uint32 AccessCounter;

    /** Rotation threshold */
    static constexpr uint32 RotationThreshold = 100;

    /**
     * Generate random bytes for masks
     * @param OutBytes Buffer to fill with random data
     * @param NumBytes Number of bytes to generate
     */
    void GenerateRandomBytes(TArray<uint8>& OutBytes, int32 NumBytes) const;

    /**
     * Apply XOR obfuscation to data
     * @param Data Data to obfuscate/deobfuscate
     * @param Mask XOR mask to apply
     */
    void ApplyXOR(TArray<uint8>& Data, const TArray<uint8>& Mask) const;

    /**
     * Rotate the obfuscation mask
     * Re-obfuscates key with new random mask
     */
    void RotateMask() const;

    /**
     * Securely zero memory
     * @param Data Array to zero
     */
    void SecureZero(TArray<uint8>& Data);

    /**
     * Calculate checksum for integrity verification
     * @param Data Data to checksum
     * @return CRC32 checksum
     */
    uint32 CalculateChecksum(const TArray<uint8>& Data) const;
};

/**
 * RAII wrapper for temporary key access
 * Automatically triggers mask rotation after use
 */
class EQUIPMENTSYSTEM_API FScopedKeyAccess
{
public:
    explicit FScopedKeyAccess(const FSuspenseSecureKeyStorage& InStorage);
    ~FScopedKeyAccess();

    /** Get the key (valid only during scope) */
    const FString& GetKey() const { return Key; }

    /** Check if key is valid */
    bool IsValid() const { return !Key.IsEmpty(); }

private:
    FString Key;
    const FSuspenseSecureKeyStorage& Storage;
};
