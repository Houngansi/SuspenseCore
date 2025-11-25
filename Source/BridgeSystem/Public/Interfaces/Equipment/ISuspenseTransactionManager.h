// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Types/Equipment/SuspenseEquipmentTypes.h"
#include "Types/Transaction/SuspenseTransactionTypes.h"
#include "ISuspenseTransactionManager.generated.h"

// Объявляем интерфейс для Unreal Engine системы рефлексии
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseTransactionManager : public UInterface
{
    GENERATED_BODY()
};

/**
 * Transaction Manager Interface
 * 
 * Design Philosophy:
 * This interface provides the contract for implementing ACID-compliant transactional
 * operations in the equipment system. It ensures atomicity of complex operations
 * through an all-or-nothing execution model with full rollback capability.
 * 
 * Key Architectural Principles:
 * 1. ATOMICITY - All operations in a transaction succeed or fail together
 * 2. CONSISTENCY - System remains in valid state before and after transaction
 * 3. ISOLATION - Concurrent transactions don't interfere with each other
 * 4. DURABILITY - Committed changes persist through system failures
 * 
 * Implementation Requirements:
 * - Must support nested transactions with savepoints
 * - Must handle concurrent transaction conflicts
 * - Must provide rollback capability at any point
 * - Must maintain transaction history for auditing
 * - Must validate transaction integrity before commit
 * 
 * Extended Operations Support:
 * - Новое API поддерживает детализированное отслеживание операций
 * - Каждая операция регистрируется и применяется с полным контекстом
 * - После коммита доступны дельты изменений для репликации
 * - Поддержка как legacy API (операции по ID), так и нового расширенного
 * 
 * Thread Safety:
 * All implementations must be thread-safe as transactions may be
 * initiated from different threads (game, network, async loading).
 * 
 * Performance Considerations:
 * - Use optimistic locking where possible
 * - Batch operations within transactions
 * - Minimize lock contention through proper lock hierarchy
 * - Consider write-ahead logging for recovery
 */
class BRIDGESYSTEM_API ISuspenseTransactionManager
{
    GENERATED_BODY()

public:
    /**
     * Begin a new transaction
     * 
     * This method starts a new transactional context. All subsequent operations
     * will be part of this transaction until it's committed or rolled back.
     * 
     * @param Description Human-readable description of the transaction purpose
     * @return Transaction ID for tracking, or invalid GUID if failed
     */
    virtual FGuid BeginTransaction(const FString& Description = TEXT("")) = 0;
    
    /**
     * Commit the specified transaction
     * 
     * Applies all operations in the transaction atomically. If any operation
     * fails, the entire transaction is rolled back automatically.
     * 
     * @param TransactionId ID of the transaction to commit
     * @return True if successfully committed, false if rolled back
     */
    virtual bool CommitTransaction(const FGuid& TransactionId) = 0;
    
    /**
     * Rollback the specified transaction
     * 
     * Reverts all operations in the transaction, restoring the system
     * to its state before the transaction began.
     * 
     * @param TransactionId ID of the transaction to rollback
     * @return True if successfully rolled back, false if error occurred
     */
    virtual bool RollbackTransaction(const FGuid& TransactionId) = 0;
    
    /**
     * Create a savepoint within the current transaction
     * 
     * Savepoints allow partial rollback within a transaction. You can rollback
     * to a savepoint without abandoning the entire transaction.
     * 
     * @param SavepointName Descriptive name for the savepoint
     * @return Savepoint ID for later reference, or invalid GUID if failed
     */
    virtual FGuid CreateSavepoint(const FString& SavepointName) = 0;
    
    /**
     * Rollback to a specific savepoint
     * 
     * Reverts operations performed after the savepoint was created,
     * while keeping the transaction active for further operations.
     * 
     * @param SavepointId ID of the savepoint to rollback to
     * @return True if successfully rolled back to savepoint
     */
    virtual bool RollbackToSavepoint(const FGuid& SavepointId) = 0;
    
    /**
     * Get the currently active transaction
     * 
     * Returns the top-most transaction in the transaction stack.
     * This is the transaction that new operations will be added to.
     * 
     * @return Current transaction data, or empty structure if no active transaction
     */
    virtual FEquipmentTransaction GetCurrentTransaction() const = 0;
    
    /**
     * Check if any transaction is currently active
     * 
     * Quick check to determine if operations should be transactional.
     * 
     * @return True if at least one transaction is active
     */
    virtual bool IsTransactionActive() const = 0;
    
    /**
     * Get transaction data by ID
     * 
     * Retrieves transaction information for active or historical transactions.
     * Useful for debugging and auditing.
     * 
     * @param TransactionId ID of the transaction to retrieve
     * @return Transaction data, or empty structure if not found
     */
    virtual FEquipmentTransaction GetTransaction(const FGuid& TransactionId) const = 0;
    
    /**
     * Begin a nested transaction
     * 
     * Creates a new transaction within the current transaction context.
     * Nested transactions can be committed or rolled back independently,
     * but parent rollback will also rollback all nested transactions.
     * 
     * @param Description Human-readable description of the nested transaction
     * @return Nested transaction ID, or invalid GUID if no parent transaction
     */
    virtual FGuid BeginNestedTransaction(const FString& Description = TEXT("")) = 0;
    
    /**
     * Register an operation with the current transaction (Legacy API)
     * 
     * Associates an operation ID with the active transaction for tracking.
     * This is typically called automatically by operation executors.
     * Сохранено для обратной совместимости - новый код должен использовать
     * расширенный RegisterOperation с детализированными операциями.
     * 
     * @param OperationId ID of the operation to register
     * @return True if successfully registered
     */
    virtual bool RegisterOperation(const FGuid& OperationId) = 0;
    
    /**
     * Validate transaction integrity
     * 
     * Performs comprehensive validation of the transaction state,
     * checking for conflicts, constraint violations, and data consistency.
     * 
     * @param TransactionId ID of the transaction to validate
     * @return True if transaction is valid and can be committed
     */
    virtual bool ValidateTransaction(const FGuid& TransactionId) const = 0;
    
    /**
     * Get transaction history
     * 
     * Retrieves recent committed and rolled back transactions for
     * auditing and debugging purposes.
     * 
     * @param MaxCount Maximum number of transactions to return
     * @return Array of recent transactions, newest first
     */
    virtual TArray<FEquipmentTransaction> GetTransactionHistory(int32 MaxCount = 10) const = 0;

    // ===== РАСШИРЕННОЕ API ДЛЯ ДЕТАЛИЗИРОВАННЫХ ОПЕРАЦИЙ =====
    
    /**
     * Check if extended operations API is supported
     * 
     * Новые реализации менеджера должны поддерживать расширенные операции.
     * Старые реализации могут возвращать false для обратной совместимости.
     * Расширенное API позволяет отслеживать детализированные операции,
     * получать дельты изменений и улучшает поддержку репликации.
     * 
     * @return True if extended operations are supported
     */
    virtual bool SupportsExtendedOps() const = 0;
    
    /**
     * Register detailed operation with specific transaction
     * 
     * Регистрирует детализированную операцию в конкретной транзакции.
     * В отличие от legacy RegisterOperation, здесь передается полная
     * информация об операции: тип, целевые объекты, параметры изменений.
     * Это позволяет строить точные дельты для репликации.
     * 
     * @param TransactionId ID of the target transaction
     * @param Operation Detailed operation data with type and parameters
     * @return True if successfully registered
     */
    virtual bool RegisterOperation(const FGuid& TransactionId, const FTransactionOperation& Operation) = 0;
    
    /**
     * Apply registered operation to transaction working snapshot
     * 
     * Применяет зарегистрированную операцию к рабочему снапшоту транзакции.
     * Операция выполняется локально в контексте транзакции без нотификации
     * внешних систем. Фактические изменения будут применены только при коммите.
     * Позволяет валидировать операции и строить preview изменений.
     * 
     * @param TransactionId ID of the transaction containing the operation
     * @param Operation Operation to apply to the working snapshot
     * @return True if operation applied successfully
     */
    virtual bool ApplyOperation(const FGuid& TransactionId, const FTransactionOperation& Operation) = 0;
    
    /**
     * Get transaction deltas after successful commit
     * 
     * Возвращает список дельт изменений, которые были применены при коммите
     * транзакции. Дельты содержат точную информацию о том, что изменилось:
     * добавленные/удаленные/измененные элементы экипировки, их свойства.
     * Используется системой репликации для отправки изменений клиентам.
     * 
     * @param TransactionId ID of the committed transaction
     * @return Array of deltas describing exact changes made by transaction
     */
    virtual TArray<FEquipmentDelta> GetTransactionDeltas(const FGuid& TransactionId) const = 0;
};