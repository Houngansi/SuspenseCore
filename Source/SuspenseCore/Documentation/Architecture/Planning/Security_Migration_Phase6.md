# Phase 6: Security Migration Plan

**Версия:** 1.0
**Дата:** 2025-12-05
**Статус:** 🔴 ТРЕБУЕТ НЕМЕДЛЕННОЙ РЕАЛИЗАЦИИ
**Приоритет:** КРИТИЧЕСКИЙ

---

## Обзор проблемы

### Критический GAP безопасности

| Компонент | Legacy (SuspenseInventory) | New (SuspenseCore) | GAP |
|-----------|---------------------------|-------------------|-----|
| CheckAuthority вызовы | 20+ | 0 | 🔴 КРИТИЧНО |
| Server RPCs WithValidation | 3 | 0 | 🔴 КРИТИЧНО |
| ROLE_Authority проверки | 3+ | 0 | 🔴 КРИТИЧНО |
| Transaction rollback security | ✅ | Частично | 🟠 |
| Rate limiting | ❌ | ❌ | 🟡 |
| Anti-cheat hooks | ❌ | ❌ | 🟡 |

### Legacy паттерны для миграции

```cpp
// Legacy CheckAuthority - НЕОБХОДИМО мигрировать
bool USuspenseInventoryComponent::CheckAuthority(const FString& FunctionName) const
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        INVENTORY_LOG(Verbose, TEXT("%s requires server authority"), *FunctionName);
        return false;
    }
    return true;
}

// Legacy Server RPC - НЕОБХОДИМО мигрировать
UFUNCTION(Server, Reliable, WithValidation)
void Server_AddItemByID(const FName& ItemID, int32 Amount);

bool Server_AddItemByID_Validate(const FName& ItemID, int32 Amount)
{
    return !ItemID.IsNone() && Amount > 0 && Amount < 10000;
}
```

---

## Архитектурное решение

### 1. Centralized Security Validator (BridgeSystem)

```
Source/BridgeSystem/
├── Public/SuspenseCore/Security/
│   ├── SuspenseCoreSecurityValidator.h      ← NEW
│   ├── SuspenseCoreSecurityContext.h        ← NEW
│   ├── SuspenseCoreSecurityTypes.h          ← NEW
│   └── SuspenseCoreSecurityMacros.h         ← NEW
└── Private/SuspenseCore/Security/
    ├── SuspenseCoreSecurityValidator.cpp    ← NEW
    └── SuspenseCoreSecurityContext.cpp      ← NEW
```

### 2. Component-level Security Integration

```
Source/InventorySystem/
├── Public/SuspenseCore/Components/
│   └── SuspenseCoreInventoryComponent.h     ← UPDATE (add Server RPCs)
└── Private/SuspenseCore/Components/
    └── SuspenseCoreInventoryComponent.cpp   ← UPDATE (add CheckAuthority)

Source/EquipmentSystem/
├── Public/SuspenseCore/Components/
│   └── SuspenseCoreEquipmentComponent.h     ← UPDATE
└── Private/SuspenseCore/Components/
    └── SuspenseCoreEquipmentComponent.cpp   ← UPDATE
```

---

## Phase 6.1: Security Validator Infrastructure

### Checklist

- [ ] `SuspenseCoreSecurityTypes.h` создан
- [ ] `SuspenseCoreSecurityContext.h` создан
- [ ] `SuspenseCoreSecurityValidator.h` создан
- [ ] `SuspenseCoreSecurityValidator.cpp` создан
- [ ] `SuspenseCoreSecurityMacros.h` создан
- [ ] Компилируется без ошибок

### Классы для создания

#### SuspenseCoreSecurityTypes.h

```cpp
#pragma once

#include "CoreMinimal.h"
#include "SuspenseCoreSecurityTypes.generated.h"

/**
 * ESuspenseCoreSecurityResult
 * Result codes for security validation
 */
UENUM(BlueprintType)
enum class ESuspenseCoreSecurityResult : uint8
{
    /** Operation allowed */
    Allowed = 0          UMETA(DisplayName = "Allowed"),

    /** Client has no authority */
    NoAuthority          UMETA(DisplayName = "No Authority"),

    /** RPC validation failed */
    ValidationFailed     UMETA(DisplayName = "Validation Failed"),

    /** Rate limit exceeded */
    RateLimited          UMETA(DisplayName = "Rate Limited"),

    /** Suspicious activity detected */
    SuspiciousActivity   UMETA(DisplayName = "Suspicious Activity"),

    /** Actor not found or invalid */
    InvalidActor         UMETA(DisplayName = "Invalid Actor"),

    /** Insufficient permissions */
    InsufficientPerms    UMETA(DisplayName = "Insufficient Permissions")
};

/**
 * ESuspenseCoreSecurityLevel
 * Security sensitivity level for operations
 */
UENUM(BlueprintType)
enum class ESuspenseCoreSecurityLevel : uint8
{
    /** Low security - read operations */
    Low = 0              UMETA(DisplayName = "Low"),

    /** Normal security - standard gameplay */
    Normal               UMETA(DisplayName = "Normal"),

    /** High security - currency, trading */
    High                 UMETA(DisplayName = "High"),

    /** Critical security - admin operations */
    Critical             UMETA(DisplayName = "Critical")
};

/**
 * FSuspenseCoreSecurityViolation
 * Record of security violation for logging/analytics
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreSecurityViolation
{
    GENERATED_BODY()

    /** Player/Actor that caused violation */
    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<AActor> Violator;

    /** Operation that was attempted */
    UPROPERTY(BlueprintReadOnly)
    FString OperationName;

    /** Violation type */
    UPROPERTY(BlueprintReadOnly)
    ESuspenseCoreSecurityResult Result;

    /** Timestamp */
    UPROPERTY(BlueprintReadOnly)
    double Timestamp;

    /** Additional context */
    UPROPERTY(BlueprintReadOnly)
    FString Context;

    /** Is this a repeat offender */
    UPROPERTY(BlueprintReadOnly)
    int32 ViolationCount;
};
```

#### SuspenseCoreSecurityValidator.h

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SuspenseCoreSecurityTypes.h"
#include "SuspenseCoreSecurityValidator.generated.h"

/**
 * USuspenseCoreSecurityValidator
 *
 * Centralized security validation for all SuspenseCore operations.
 * Implements AAA-standard anti-cheat patterns.
 *
 * FEATURES:
 * - Authority checking (server-side validation)
 * - Rate limiting (prevent spam/DoS)
 * - Suspicious activity detection
 * - Violation logging for analytics
 * - EventBus integration for security events
 *
 * USAGE:
 * ```cpp
 * USuspenseCoreSecurityValidator* Security = USuspenseCoreSecurityValidator::Get(this);
 * if (!Security->CheckAuthority(GetOwner(), TEXT("AddItem")))
 * {
 *     // Handle unauthorized access
 *     return;
 * }
 * ```
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreSecurityValidator : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    //==================================================================
    // Static Access
    //==================================================================

    /** Get validator instance */
    static USuspenseCoreSecurityValidator* Get(const UObject* WorldContextObject);

    //==================================================================
    // USubsystem Interface
    //==================================================================

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    //==================================================================
    // Authority Checking
    //==================================================================

    /**
     * Check if actor has server authority.
     * @param Actor Actor to check
     * @param OperationName Name for logging
     * @return true if has authority
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Security")
    bool CheckAuthority(AActor* Actor, const FString& OperationName) const;

    /**
     * Check if actor has authority with detailed result.
     * @param Actor Actor to check
     * @param OperationName Name for logging
     * @param OutResult Detailed result
     * @return true if has authority
     */
    bool CheckAuthorityWithResult(AActor* Actor, const FString& OperationName,
        ESuspenseCoreSecurityResult& OutResult) const;

    /**
     * Check if component owner has authority.
     * @param Component Component to check
     * @param OperationName Name for logging
     * @return true if has authority
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Security")
    bool CheckComponentAuthority(UActorComponent* Component, const FString& OperationName) const;

    //==================================================================
    // Rate Limiting
    //==================================================================

    /**
     * Check if operation is rate limited.
     * @param Actor Actor performing operation
     * @param OperationName Operation identifier
     * @param MaxPerSecond Max operations per second
     * @return true if allowed (not rate limited)
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Security")
    bool CheckRateLimit(AActor* Actor, const FString& OperationName, float MaxPerSecond = 10.0f);

    /**
     * Reset rate limit for actor.
     * @param Actor Actor to reset
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Security")
    void ResetRateLimit(AActor* Actor);

    //==================================================================
    // RPC Validation
    //==================================================================

    /**
     * Validate Server RPC parameters.
     * Generic validation for common parameter types.
     * @param ItemID Item ID parameter
     * @param Quantity Quantity parameter
     * @param MaxQuantity Maximum allowed quantity
     * @return true if valid
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Security")
    bool ValidateItemRPC(FName ItemID, int32 Quantity, int32 MaxQuantity = 9999) const;

    /**
     * Validate GUID parameter.
     * @param InstanceID GUID to validate
     * @return true if valid
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Security")
    bool ValidateGUID(const FGuid& InstanceID) const;

    /**
     * Validate slot index.
     * @param SlotIndex Slot to validate
     * @param MaxSlots Maximum slots in inventory
     * @return true if valid
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Security")
    bool ValidateSlotIndex(int32 SlotIndex, int32 MaxSlots) const;

    //==================================================================
    // Suspicious Activity Detection
    //==================================================================

    /**
     * Check for suspicious activity patterns.
     * @param Actor Actor to check
     * @param OperationName Operation being performed
     * @param Level Security level for operation
     * @return true if activity is normal
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Security")
    bool CheckSuspiciousActivity(AActor* Actor, const FString& OperationName,
        ESuspenseCoreSecurityLevel Level = ESuspenseCoreSecurityLevel::Normal);

    /**
     * Report suspicious activity.
     * @param Actor Suspicious actor
     * @param Reason Reason for suspicion
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Security")
    void ReportSuspiciousActivity(AActor* Actor, const FString& Reason);

    //==================================================================
    // Violation Tracking
    //==================================================================

    /**
     * Log security violation.
     * @param Violation Violation data
     */
    void LogViolation(const FSuspenseCoreSecurityViolation& Violation);

    /**
     * Get violation count for actor.
     * @param Actor Actor to check
     * @return Number of violations
     */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Security")
    int32 GetViolationCount(AActor* Actor) const;

    /**
     * Get all violations (for admin tools).
     */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Security")
    const TArray<FSuspenseCoreSecurityViolation>& GetAllViolations() const { return ViolationLog; }

    //==================================================================
    // EventBus Integration
    //==================================================================

    /**
     * Broadcast security event.
     * @param EventTag Event tag
     * @param Actor Related actor
     * @param Context Additional context
     */
    void BroadcastSecurityEvent(FGameplayTag EventTag, AActor* Actor, const FString& Context);

    //==================================================================
    // Configuration
    //==================================================================

    /** Enable/disable rate limiting */
    UPROPERTY(EditAnywhere, Category = "Configuration")
    bool bEnableRateLimiting = true;

    /** Enable/disable suspicious activity detection */
    UPROPERTY(EditAnywhere, Category = "Configuration")
    bool bEnableSuspiciousActivityDetection = true;

    /** Max violations before kick (0 = never kick) */
    UPROPERTY(EditAnywhere, Category = "Configuration")
    int32 MaxViolationsBeforeKick = 10;

    /** Violation decay time (seconds) */
    UPROPERTY(EditAnywhere, Category = "Configuration")
    float ViolationDecayTime = 300.0f;

protected:
    /** Rate limit tracking */
    TMap<uint32, TMap<FString, double>> RateLimitMap;

    /** Violation count per actor */
    TMap<uint32, int32> ViolationCounts;

    /** Full violation log */
    UPROPERTY()
    TArray<FSuspenseCoreSecurityViolation> ViolationLog;

    /** Cached EventBus */
    UPROPERTY()
    TWeakObjectPtr<class USuspenseCoreEventBus> CachedEventBus;

    /** Get actor hash for tracking */
    uint32 GetActorHash(AActor* Actor) const;

    /** Internal violation handler */
    void HandleViolation(AActor* Actor, const FString& OperationName,
        ESuspenseCoreSecurityResult Result, const FString& Context);
};
```

#### SuspenseCoreSecurityMacros.h

```cpp
#pragma once

#include "SuspenseCoreSecurityValidator.h"

/**
 * Security Macros for SuspenseCore
 *
 * Provides convenient macros for common security patterns.
 * Use these instead of manual CheckAuthority calls.
 */

/** Check authority and return false if not server */
#define SUSPENSE_CHECK_AUTHORITY(Actor, FunctionName) \
    do { \
        USuspenseCoreSecurityValidator* _Validator = USuspenseCoreSecurityValidator::Get(Actor); \
        if (_Validator && !_Validator->CheckAuthority(Actor, TEXT(#FunctionName))) \
        { \
            return false; \
        } \
    } while(0)

/** Check authority and return void if not server */
#define SUSPENSE_CHECK_AUTHORITY_VOID(Actor, FunctionName) \
    do { \
        USuspenseCoreSecurityValidator* _Validator = USuspenseCoreSecurityValidator::Get(Actor); \
        if (_Validator && !_Validator->CheckAuthority(Actor, TEXT(#FunctionName))) \
        { \
            return; \
        } \
    } while(0)

/** Check component authority and return false if not server */
#define SUSPENSE_CHECK_COMPONENT_AUTHORITY(Component, FunctionName) \
    do { \
        USuspenseCoreSecurityValidator* _Validator = USuspenseCoreSecurityValidator::Get(Component); \
        if (_Validator && !_Validator->CheckComponentAuthority(Component, TEXT(#FunctionName))) \
        { \
            return false; \
        } \
    } while(0)

/** Check authority and call Server RPC if client */
#define SUSPENSE_AUTHORITY_OR_RPC(Actor, ServerRPC, ...) \
    do { \
        if (!Actor || !Actor->HasAuthority()) \
        { \
            ServerRPC(__VA_ARGS__); \
            return false; \
        } \
    } while(0)

/** Validate item RPC parameters */
#define SUSPENSE_VALIDATE_ITEM_RPC(ItemID, Quantity) \
    (!ItemID.IsNone() && Quantity > 0 && Quantity <= 9999)

/** Validate GUID is valid */
#define SUSPENSE_VALIDATE_GUID(InstanceID) \
    (InstanceID.IsValid())

/** Validate slot index */
#define SUSPENSE_VALIDATE_SLOT(SlotIndex, MaxSlots) \
    (SlotIndex >= 0 && SlotIndex < MaxSlots)

/** Rate limit check */
#define SUSPENSE_CHECK_RATE_LIMIT(Actor, FunctionName, MaxPerSecond) \
    do { \
        USuspenseCoreSecurityValidator* _Validator = USuspenseCoreSecurityValidator::Get(Actor); \
        if (_Validator && !_Validator->CheckRateLimit(Actor, TEXT(#FunctionName), MaxPerSecond)) \
        { \
            return false; \
        } \
    } while(0)
```

---

## Phase 6.2: Inventory Component Security

### Checklist

- [ ] Добавить Server RPCs в SuspenseCoreInventoryComponent.h
- [ ] Добавить _Validate функции для всех RPCs
- [ ] Добавить CheckAuthority вызовы в критические операции
- [ ] Интегрировать SuspenseCoreSecurityValidator
- [ ] Обновить AddItemByID для использования Server RPC
- [ ] Обновить RemoveItemByID для использования Server RPC
- [ ] Обновить MoveItem для использования Server RPC
- [ ] Тесты безопасности проходят

### Server RPCs для добавления

```cpp
// В SuspenseCoreInventoryComponent.h

protected:
    //==================================================================
    // Server RPCs - Security Layer
    //==================================================================

    /** Server: Add item by ID */
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_AddItemByID(FName ItemID, int32 Quantity);
    bool Server_AddItemByID_Validate(FName ItemID, int32 Quantity);
    void Server_AddItemByID_Implementation(FName ItemID, int32 Quantity);

    /** Server: Remove item by ID */
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_RemoveItemByID(FName ItemID, int32 Quantity);
    bool Server_RemoveItemByID_Validate(FName ItemID, int32 Quantity);
    void Server_RemoveItemByID_Implementation(FName ItemID, int32 Quantity);

    /** Server: Move item between slots */
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_MoveItem(int32 FromSlot, int32 ToSlot);
    bool Server_MoveItem_Validate(int32 FromSlot, int32 ToSlot);
    void Server_MoveItem_Implementation(int32 FromSlot, int32 ToSlot);

    /** Server: Swap items */
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_SwapItems(int32 Slot1, int32 Slot2);
    bool Server_SwapItems_Validate(int32 Slot1, int32 Slot2);
    void Server_SwapItems_Implementation(int32 Slot1, int32 Slot2);

    /** Server: Split stack */
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_SplitStack(int32 SourceSlot, int32 SplitQuantity, int32 TargetSlot);
    bool Server_SplitStack_Validate(int32 SourceSlot, int32 SplitQuantity, int32 TargetSlot);
    void Server_SplitStack_Implementation(int32 SourceSlot, int32 SplitQuantity, int32 TargetSlot);

    /** Security validator helper */
    bool CheckInventoryAuthority(const FString& FunctionName) const;
```

### Validation Implementation Pattern

```cpp
// SuspenseCoreInventoryComponent.cpp

bool USuspenseCoreInventoryComponent::Server_AddItemByID_Validate(FName ItemID, int32 Quantity)
{
    // Basic parameter validation
    if (ItemID.IsNone())
    {
        return false;
    }

    // Quantity sanity check (prevent overflow attacks)
    if (Quantity <= 0 || Quantity > 9999)
    {
        return false;
    }

    // Rate limit check (using SecurityValidator)
    USuspenseCoreSecurityValidator* Security = USuspenseCoreSecurityValidator::Get(this);
    if (Security && !Security->CheckRateLimit(GetOwner(), TEXT("AddItem"), 10.0f))
    {
        return false;
    }

    return true;
}

bool USuspenseCoreInventoryComponent::CheckInventoryAuthority(const FString& FunctionName) const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return false;
    }

    if (!Owner->HasAuthority())
    {
        SUSPENSECORE_INV_LOG(Verbose, TEXT("%s: No authority on %s"),
            *FunctionName, *Owner->GetName());
        return false;
    }

    return true;
}
```

---

## Phase 6.3: Equipment & Other Components

### Checklist

- [ ] SuspenseCoreEquipmentComponent - добавить Server RPCs
- [ ] SuspenseCoreWeaponComponent - добавить Server RPCs
- [ ] SuspenseCoreInteractionComponent - добавить CheckAuthority
- [ ] Все компоненты используют SecurityValidator
- [ ] Компилируется без ошибок

### Паттерн для всех компонентов

```cpp
// Стандартный паттерн для любого компонента

class USuspenseCoreXXXComponent : public UActorComponent
{
    // 1. Публичный API - делегирует в Server RPC если нет authority
    bool PerformAction(FName ActionID)
    {
        if (!CheckXXXAuthority(TEXT("PerformAction")))
        {
            Server_PerformAction(ActionID);
            return false; // Client returns false, server will process
        }

        return PerformActionInternal(ActionID);
    }

    // 2. Server RPC с валидацией
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_PerformAction(FName ActionID);

    bool Server_PerformAction_Validate(FName ActionID)
    {
        return !ActionID.IsNone();
    }

    void Server_PerformAction_Implementation(FName ActionID)
    {
        PerformActionInternal(ActionID);
    }

    // 3. Внутренняя реализация (только server)
    bool PerformActionInternal(FName ActionID)
    {
        // Actual logic here, only runs on server
    }

    // 4. Authority check helper
    bool CheckXXXAuthority(const FString& FunctionName) const
    {
        USuspenseCoreSecurityValidator* Security = USuspenseCoreSecurityValidator::Get(this);
        return Security ? Security->CheckComponentAuthority(
            const_cast<USuspenseCoreXXXComponent*>(this), FunctionName) : false;
    }
};
```

---

## Phase 6.4: EventBus Security Events

### GameplayTags для добавления

```ini
; Config/DefaultGameplayTags.ini

; Security Events
+GameplayTagList=(Tag="SuspenseCore.Event.Security.ViolationDetected",DevComment="Security violation detected")
+GameplayTagList=(Tag="SuspenseCore.Event.Security.RateLimitExceeded",DevComment="Rate limit exceeded")
+GameplayTagList=(Tag="SuspenseCore.Event.Security.SuspiciousActivity",DevComment="Suspicious activity detected")
+GameplayTagList=(Tag="SuspenseCore.Event.Security.AuthorityDenied",DevComment="Authority check failed")
+GameplayTagList=(Tag="SuspenseCore.Event.Security.ValidationFailed",DevComment="RPC validation failed")
+GameplayTagList=(Tag="SuspenseCore.Event.Security.PlayerKicked",DevComment="Player kicked for violations")
```

### Checklist

- [ ] GameplayTags добавлены в DefaultGameplayTags.ini
- [ ] SecurityValidator публикует события
- [ ] События логируются для аналитики
- [ ] Admin tools могут подписаться на события

---

## Phase 6.5: Testing & Validation

### Checklist

- [ ] Unit тест: CheckAuthority блокирует client
- [ ] Unit тест: Server RPC проходит валидацию
- [ ] Unit тест: Rate limiting работает
- [ ] Integration тест: Client→Server→Client flow
- [ ] Integration тест: Cheat attempt blocked
- [ ] Load тест: 100 RPC/s не вызывает issues
- [ ] Security audit пройден

### Тестовые сценарии

```cpp
// Тест 1: Клиент не может напрямую добавлять items
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecurityClientAddItemTest,
    "SuspenseCore.Security.ClientCannotAddDirectly",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSecurityClientAddItemTest::RunTest(const FString& Parameters)
{
    // Setup: Create component with client authority
    USuspenseCoreInventoryComponent* Inventory = CreateTestInventory();
    SetClientAuthority(Inventory);

    // Execute: Try to add item
    bool bResult = Inventory->AddItemByID_Implementation(TEXT("TestItem"), 1);

    // Verify: Should fail (client blocked)
    TestFalse(TEXT("Client should not be able to add items directly"), bResult);

    return true;
}

// Тест 2: Rate limiting работает
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecurityRateLimitTest,
    "SuspenseCore.Security.RateLimitEnforced",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSecurityRateLimitTest::RunTest(const FString& Parameters)
{
    USuspenseCoreSecurityValidator* Security = CreateTestValidator();

    // Execute: Spam 100 operations
    int32 BlockedCount = 0;
    for (int32 i = 0; i < 100; ++i)
    {
        if (!Security->CheckRateLimit(TestActor, TEXT("TestOp"), 10.0f))
        {
            BlockedCount++;
        }
    }

    // Verify: Most should be blocked
    TestTrue(TEXT("Rate limiting should block rapid operations"), BlockedCount > 80);

    return true;
}
```

---

## Миграционный путь из Legacy

### Шаг 1: Добавить Security Infrastructure

1. Создать файлы в `BridgeSystem/Public/SuspenseCore/Security/`
2. Компилировать и тестировать базовую функциональность
3. Обновить `BridgeSystem.Build.cs` если нужно

### Шаг 2: Мигрировать Inventory

1. Добавить Server RPCs в `SuspenseCoreInventoryComponent`
2. Обновить публичные методы для делегирования
3. Тестировать в PIE с 2 клиентами

### Шаг 3: Мигрировать остальные компоненты

1. Equipment
2. Weapon
3. Interaction
4. Pickup

### Шаг 4: Интеграционное тестирование

1. Full client-server flow
2. Cheat attempt scenarios
3. Performance под нагрузкой

---

## Сравнение с AAA стандартами

| Критерий | Industry Standard | Legacy | SuspenseCore After |
|----------|-------------------|--------|-------------------|
| Server Authority | ✅ | ✅ | ✅ |
| RPC Validation | ✅ | ✅ | ✅ |
| Rate Limiting | ✅ | ❌ | ✅ |
| Violation Logging | ✅ | ❌ | ✅ |
| EventBus Integration | - | ❌ | ✅ |
| Centralized Validator | ✅ | ❌ | ✅ |
| Auto-kick | ✅ | ❌ | ✅ |

**После реализации Phase 6: SuspenseCore ПРЕВОСХОДИТ большинство AAA решений благодаря:**
- Централизованной системе безопасности
- EventBus интеграции для мониторинга
- Rate limiting из коробки
- Автоматическому логированию для аналитики

---

## Риски и митигация

| Риск | Вероятность | Митигация |
|------|-------------|-----------|
| Breaking change в API | Средняя | Методы делегируют, API не меняется |
| Performance overhead | Низкая | CheckAuthority ~0.001ms |
| False positives rate limit | Низкая | Настраиваемые пороги |
| Complex debugging | Средняя | Детальное логирование |

---

## Definition of Done

### Phase Complete когда:

1. ✅ Все чекбоксы отмечены
2. ✅ Код компилируется без warnings
3. ✅ Все тесты проходят
4. ✅ Security audit пройден
5. ✅ Документация обновлена

### Acceptance Criteria:

1. Client НЕ может добавлять items напрямую
2. Client НЕ может удалять items напрямую
3. Client НЕ может перемещать items напрямую
4. Все операции проходят через Server RPC
5. Rate limiting блокирует spam
6. Violations логируются
7. Нет regression в существующем функционале

---

## Связанная документация

- [MMO Scalability Checklist](./MMO_Scalability_Implementation_Checklist.md)
- [Best Practices](../../Guides/BestPractices.md)
- [SuspenseCore Architecture](../../../BridgeSystem/Documentation/SuspenseCoreArchitecture.md)
- [Legacy Inventory Analysis](../Analysis/MedComInventory_Analysis.md)

---

**Автор:** Tech Lead
**Review Required:** Senior Developer
**Следующий milestone:** Phase 6.1 Complete
