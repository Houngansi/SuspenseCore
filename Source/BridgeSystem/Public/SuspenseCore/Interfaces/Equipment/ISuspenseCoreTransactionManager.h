// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SuspenseCore/Types/Equipment/SuspenseCoreEquipmentTypes.h"
#include "SuspenseCore/Types/Transaction/SuspenseCoreTransactionTypes.h"
#include "ISuspenseCoreTransactionManager.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreTransactionManager : public UInterface
{
    GENERATED_BODY()
};

/**
 * SuspenseCore Transaction Manager Interface
 *
 * Provides ACID-compliant transactional operations for the equipment system.
 * Ensures atomicity of complex operations with full rollback capability.
 *
 * Key Architectural Principles:
 * 1. ATOMICITY - All operations succeed or fail together
 * 2. CONSISTENCY - System remains in valid state
 * 3. ISOLATION - Concurrent transactions don't interfere
 * 4. DURABILITY - Committed changes persist
 *
 * Implementation Requirements:
 * - Support nested transactions with savepoints
 * - Handle concurrent transaction conflicts
 * - Provide rollback at any point
 * - Maintain transaction history for auditing
 * - Validate integrity before commit
 *
 * Thread Safety:
 * All implementations must be thread-safe.
 */
class BRIDGESYSTEM_API ISuspenseCoreTransactionManager
{
    GENERATED_BODY()

public:
    //========================================
    // Core Transaction Operations
    //========================================

    /**
     * Begin a new transaction
     * @param Description Human-readable description
     * @return Transaction ID, or invalid GUID if failed
     */
    virtual FGuid BeginTransaction(const FString& Description = TEXT("")) = 0;

    /**
     * Commit the specified transaction
     * @param TransactionId ID of the transaction to commit
     * @return True if successfully committed
     */
    virtual bool CommitTransaction(const FGuid& TransactionId) = 0;

    /**
     * Rollback the specified transaction
     * @param TransactionId ID of the transaction to rollback
     * @return True if successfully rolled back
     */
    virtual bool RollbackTransaction(const FGuid& TransactionId) = 0;

    //========================================
    // Savepoint Management
    //========================================

    /**
     * Create a savepoint within the current transaction
     * @param SavepointName Descriptive name
     * @return Savepoint ID, or invalid GUID if failed
     */
    virtual FGuid CreateSavepoint(const FString& SavepointName) = 0;

    /**
     * Rollback to a specific savepoint
     * @param SavepointId ID of the savepoint
     * @return True if rolled back successfully
     */
    virtual bool RollbackToSavepoint(const FGuid& SavepointId) = 0;

    //========================================
    // Transaction State
    //========================================

    /**
     * Get the currently active transaction
     * @return Current transaction data
     */
    virtual FEquipmentTransaction GetCurrentTransaction() const = 0;

    /**
     * Check if any transaction is active
     * @return True if transaction is active
     */
    virtual bool IsTransactionActive() const = 0;

    /**
     * Get transaction data by ID
     * @param TransactionId Transaction ID
     * @return Transaction data
     */
    virtual FEquipmentTransaction GetTransaction(const FGuid& TransactionId) const = 0;

    //========================================
    // Nested Transactions
    //========================================

    /**
     * Begin a nested transaction
     * @param Description Human-readable description
     * @return Nested transaction ID
     */
    virtual FGuid BeginNestedTransaction(const FString& Description = TEXT("")) = 0;

    //========================================
    // Operation Registration
    //========================================

    /**
     * Register an operation with current transaction (Legacy API)
     * @param OperationId Operation ID
     * @return True if registered
     */
    virtual bool RegisterOperation(const FGuid& OperationId) = 0;

    /**
     * Validate transaction integrity
     * @param TransactionId Transaction ID
     * @return True if valid
     */
    virtual bool ValidateTransaction(const FGuid& TransactionId) const = 0;

    /**
     * Get transaction history
     * @param MaxCount Maximum transactions to return
     * @return Recent transactions
     */
    virtual TArray<FEquipmentTransaction> GetTransactionHistory(int32 MaxCount = 10) const = 0;

    //========================================
    // Extended Operations API
    //========================================

    /**
     * Check if extended operations API is supported
     * @return True if extended ops supported
     */
    virtual bool SupportsExtendedOps() const = 0;

    /**
     * Register detailed operation with transaction
     * @param TransactionId Target transaction ID
     * @param Operation Detailed operation data
     * @return True if registered
     */
    virtual bool RegisterOperation(const FGuid& TransactionId, const FTransactionOperation& Operation) = 0;

    /**
     * Apply operation to transaction working snapshot
     * @param TransactionId Transaction ID
     * @param Operation Operation to apply
     * @return True if applied
     */
    virtual bool ApplyOperation(const FGuid& TransactionId, const FTransactionOperation& Operation) = 0;

    /**
     * Get transaction deltas after commit
     * @param TransactionId Committed transaction ID
     * @return Array of change deltas
     */
    virtual TArray<FEquipmentDelta> GetTransactionDeltas(const FGuid& TransactionId) const = 0;
};

