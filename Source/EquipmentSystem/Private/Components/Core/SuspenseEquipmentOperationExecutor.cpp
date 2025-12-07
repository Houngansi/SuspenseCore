// MedComEquipmentOperationExecutor.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/Core/SuspenseEquipmentOperationExecutor.h"
#include "SuspenseCore/Components/Core/SuspenseEquipmentDataStore.h"
#include "SuspenseCore/Components/Validation/SuspenseEquipmentSlotValidator.h"
#include "SuspenseCore/Services/SuspenseEquipmentServiceMacros.h"
#include "Types/Loadout/SuspenseLoadoutSettings.h"  // Для ESuspenseEquipmentSlotType

// Define proper log category
DEFINE_LOG_CATEGORY_STATIC(LogEquipmentExecutor, Log, All);

// Constructor
USuspenseEquipmentOperationExecutor::USuspenseEquipmentOperationExecutor()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(false); // Pure planning logic, no replication
}

//========================================
// UActorComponent Interface
//========================================

void USuspenseEquipmentOperationExecutor::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogEquipmentExecutor, Log, TEXT("EquipmentOperationExecutor: Initialized as pure planner"));
}

void USuspenseEquipmentOperationExecutor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Reset statistics
    ResetStatistics();

    Super::EndPlay(EndPlayReason);
}

//========================================
// Planning API Implementation
//========================================

bool USuspenseEquipmentOperationExecutor::BuildPlan(
    const FEquipmentOperationRequest& Request,
    FTransactionPlan& OutPlan,
    FText& OutError) const
{
    FScopeLock Lock(&PlanningCriticalSection);

    // Validate request
    if (!Request.IsValid())
    {
        OutError = NSLOCTEXT("Equipment", "InvalidRequest", "Invalid equipment request");
        return false;
    }

    // Check initialization
    if (!IsInitialized())
    {
        OutError = NSLOCTEXT("Equipment", "ExecutorNotInitialized", "Executor not initialized");
        return false;
    }

    // Initialize plan
    OutPlan = FTransactionPlan();
    OutPlan.DebugLabel = FString::Printf(TEXT("Plan-%s-%s"),
        *Request.OperationId.ToString(),
        *UEnum::GetValueAsString(Request.OperationType));

    // Build plan based on operation type
    switch (Request.OperationType)
    {
        case EEquipmentOperationType::Equip:
            Expand_Equip(Request, OutPlan);
            break;

        case EEquipmentOperationType::Unequip:
            Expand_Unequip(Request, OutPlan);
            break;

        case EEquipmentOperationType::Move:
            Expand_Move(Request, OutPlan);
            break;

        case EEquipmentOperationType::Drop:
            Expand_Drop(Request, OutPlan);
            break;

        case EEquipmentOperationType::Swap:
            Expand_Swap(Request, OutPlan);
            break;

        case EEquipmentOperationType::QuickSwitch:
            Expand_QuickSwitch(Request, OutPlan);
            break;

        case EEquipmentOperationType::Transfer:
            Expand_Transfer(Request, OutPlan);
            break;

        case EEquipmentOperationType::Reload:
            Expand_Reload(Request, OutPlan);
            break;

        case EEquipmentOperationType::Repair:
            Expand_Repair(Request, OutPlan);
            break;

        case EEquipmentOperationType::Upgrade:
            Expand_Upgrade(Request, OutPlan);
            break;

        case EEquipmentOperationType::Modify:
            Expand_Modify(Request, OutPlan);
            break;

        case EEquipmentOperationType::Combine:
            Expand_Combine(Request, OutPlan);
            break;

        case EEquipmentOperationType::Split:
            Expand_Split(Request, OutPlan);
            break;

        default:
            // For unsupported operations, create single-step plan
            OutPlan.Add(FTransactionPlanStep(Request, TEXT("Direct execution")));
            break;
    }

    // Validate plan has steps
    if (!OutPlan.IsValid())
    {
        OutError = NSLOCTEXT("Equipment", "EmptyPlan", "Failed to build operation plan");
        return false;
    }

    // Check plan complexity
    if (OutPlan.Num() > MaxPlanComplexity)
    {
        OutError = FText::Format(
            NSLOCTEXT("Equipment", "PlanTooComplex", "Plan exceeds maximum complexity: {0} > {1}"),
            OutPlan.Num(),
            MaxPlanComplexity
        );
        return false;
    }

    // Update statistics
    TotalPlansBuilt.fetch_add(1);
    float CurrentAvg = AveragePlanSize.load();
    AveragePlanSize.store(CurrentAvg * 0.9f + OutPlan.Num() * 0.1f);

    // Estimate execution time
    OutPlan.EstimatedExecutionTimeMs = EstimatePlanExecutionTime(OutPlan);

    // Set idempotency flag
    OutPlan.bIdempotent = IsPlanIdempotent(OutPlan);

    // Add metadata
    OutPlan.Metadata.Add(TEXT("RequestType"), UEnum::GetValueAsString(Request.OperationType));
    OutPlan.Metadata.Add(TEXT("BuildTime"), FString::SanitizeFloat(FPlatformTime::Seconds()));

    if (bEnableDetailedLogging)
    {
        UE_LOG(LogEquipmentExecutor, Verbose,
            TEXT("Built plan [%s]: %d steps, ~%.1fms, Idempotent=%s"),
            *OutPlan.PlanId.ToString(),
            OutPlan.Num(),
            OutPlan.EstimatedExecutionTimeMs,
            OutPlan.bIdempotent ? TEXT("Yes") : TEXT("No")
        );
    }

    return true;
}

bool USuspenseEquipmentOperationExecutor::ValidatePlan(
    const FTransactionPlan& Plan,
    FText& OutError) const
{
    FScopeLock Lock(&PlanningCriticalSection);

    if (!Plan.IsValid())
    {
        OutError = NSLOCTEXT("Equipment", "InvalidPlan", "Invalid transaction plan");
        FailedValidations.fetch_add(1);
        return false;
    }

    // If no validator, consider all plans valid
    if (!SlotValidator.GetInterface())
    {
        if (bRequireValidation)
        {
            UE_LOG(LogEquipmentExecutor, Warning,
                TEXT("Validation required but no validator available"));
        }
        SuccessfulValidations.fetch_add(1);
        return true;
    }

    // Validate each step
    for (int32 i = 0; i < Plan.Steps.Num(); i++)
    {
        const FTransactionPlanStep& Step = Plan.Steps[i];

        if (!ValidateStep(Step, OutError))
        {
            OutError = FText::Format(
                NSLOCTEXT("Equipment", "StepValidationFailed", "Step {0} validation failed: {1}"),
                i + 1,
                OutError
            );
            FailedValidations.fetch_add(1);
            return false;
        }
    }

    SuccessfulValidations.fetch_add(1);
    return true;
}

float USuspenseEquipmentOperationExecutor::EstimatePlanExecutionTime(const FTransactionPlan& Plan) const
{
    // Base estimates per operation type (in milliseconds)
    static const TMap<EEquipmentOperationType, float> OperationCosts = {
        {EEquipmentOperationType::Equip, 5.0f},
        {EEquipmentOperationType::Unequip, 3.0f},
        {EEquipmentOperationType::Move, 2.0f},
        {EEquipmentOperationType::Swap, 6.0f},
        {EEquipmentOperationType::Drop, 2.0f},
        {EEquipmentOperationType::QuickSwitch, 3.0f},
        {EEquipmentOperationType::Transfer, 4.0f},
        {EEquipmentOperationType::Reload, 4.0f},
        {EEquipmentOperationType::Repair, 8.0f},
        {EEquipmentOperationType::Upgrade, 10.0f},
        {EEquipmentOperationType::Modify, 6.0f},
        {EEquipmentOperationType::Combine, 5.0f},
        {EEquipmentOperationType::Split, 3.0f}
    };

    float TotalMs = 0.0f;

    for (const FTransactionPlanStep& Step : Plan.Steps)
    {
        if (const float* Cost = OperationCosts.Find(Step.Request.OperationType))
        {
            TotalMs += *Cost;
        }
        else
        {
            TotalMs += 5.0f; // Default cost for unknown operations
        }

        // Add validation overhead if required
        if (bRequireValidation && SlotValidator.GetInterface())
        {
            TotalMs += 1.0f;
        }
    }

    // Add transaction overhead
    if (Plan.bAtomic)
    {
        TotalMs += 2.0f; // Begin/commit overhead
    }

    if (Plan.bReversible)
    {
        TotalMs += 1.0f; // Savepoint overhead
    }

    return TotalMs;
}

bool USuspenseEquipmentOperationExecutor::IsPlanIdempotent(const FTransactionPlan& Plan) const
{
    // Plans are idempotent if they only contain absolute-set operations
    // Move/Swap/QuickSwitch operations are generally not idempotent

    for (const FTransactionPlanStep& Step : Plan.Steps)
    {
        switch (Step.Request.OperationType)
        {
            case EEquipmentOperationType::Move:
            case EEquipmentOperationType::Swap:
            case EEquipmentOperationType::QuickSwitch:
            case EEquipmentOperationType::Transfer:
                return false; // These modify relative state

            case EEquipmentOperationType::Equip:
            case EEquipmentOperationType::Unequip:
            case EEquipmentOperationType::Drop:
                // These are idempotent if they target specific items/slots
                if (Step.Request.TargetSlotIndex == INDEX_NONE &&
                    Step.Request.SourceSlotIndex == INDEX_NONE)
                {
                    return false;
                }
                break;

            case EEquipmentOperationType::Reload:
            case EEquipmentOperationType::Repair:
            case EEquipmentOperationType::Upgrade:
            case EEquipmentOperationType::Modify:
                // These might be idempotent depending on implementation
                break;

            case EEquipmentOperationType::Combine:
            case EEquipmentOperationType::Split:
                return false; // These create/destroy items

            default:
                break;
        }
    }

    return true;
}

//========================================
// Plan Expansion Methods (Pure Functions)
//========================================

void USuspenseEquipmentOperationExecutor::Expand_Equip(
    const FEquipmentOperationRequest& In,
    FTransactionPlan& Out) const
{
    // Simple equip is a single step
    FTransactionPlanStep Step(In, TEXT("Equip item to slot"));
    Step.StepPriority = static_cast<int32>(In.Priority);
    Out.Add(Step);
    Out.bReversible = true;
}

void USuspenseEquipmentOperationExecutor::Expand_Unequip(
    const FEquipmentOperationRequest& In,
    FTransactionPlan& Out) const
{
    // Simple unequip is a single step
    FTransactionPlanStep Step(In, TEXT("Unequip item from slot"));
    Step.StepPriority = static_cast<int32>(In.Priority);
    Out.Add(Step);
    Out.bReversible = true;
}

void USuspenseEquipmentOperationExecutor::Expand_Move(
    const FEquipmentOperationRequest& In,
    FTransactionPlan& Out) const
{
    // Move is a single atomic step
    FTransactionPlanStep Step(In, TEXT("Move item between slots"));
    Step.StepPriority = static_cast<int32>(In.Priority);
    Out.Add(Step);
    Out.bReversible = true;
}

void USuspenseEquipmentOperationExecutor::Expand_Drop(
    const FEquipmentOperationRequest& In,
    FTransactionPlan& Out) const
{
    // Drop is a single step but not easily reversible
    FTransactionPlanStep Step(In, TEXT("Drop item from slot"));
    Step.StepPriority = static_cast<int32>(In.Priority);
    Step.bReversible = false; // Can't easily undo a drop
    Out.Add(Step);
    Out.bReversible = false;
}

void USuspenseEquipmentOperationExecutor::Expand_Swap(
    const FEquipmentOperationRequest& In,
    FTransactionPlan& Out) const
{
    // Swap expands to three-step atomic operation for safety

    // Step 1: Move item from slot A to temporary storage
    FEquipmentOperationRequest TempMove = In;
    TempMove.OperationType = EEquipmentOperationType::Move;
    TempMove.OperationId = FGuid::NewGuid();
    TempMove.SourceSlotIndex = In.SourceSlotIndex;
    TempMove.TargetSlotIndex = -1; // Special marker for temp storage

    FTransactionPlanStep Step1(TempMove, TEXT("Swap step 1: Move A to temp"));
    Step1.StepPriority = static_cast<int32>(EEquipmentOperationPriority::Critical);
    Out.Add(Step1);

    // Step 2: Move item from slot B to slot A
    FEquipmentOperationRequest MoveBA = In;
    MoveBA.OperationType = EEquipmentOperationType::Move;
    MoveBA.OperationId = FGuid::NewGuid();
    MoveBA.SourceSlotIndex = In.TargetSlotIndex;
    MoveBA.TargetSlotIndex = In.SourceSlotIndex;

    FTransactionPlanStep Step2(MoveBA, TEXT("Swap step 2: Move B to A"));
    Step2.StepPriority = static_cast<int32>(EEquipmentOperationPriority::Critical);
    Out.Add(Step2);

    // Step 3: Move item from temp to slot B
    FEquipmentOperationRequest MoveTemp = In;
    MoveTemp.OperationType = EEquipmentOperationType::Move;
    MoveTemp.OperationId = FGuid::NewGuid();
    MoveTemp.SourceSlotIndex = -1; // From temporary storage
    MoveTemp.TargetSlotIndex = In.TargetSlotIndex;

    FTransactionPlanStep Step3(MoveTemp, TEXT("Swap step 3: Move temp to B"));
    Step3.StepPriority = static_cast<int32>(EEquipmentOperationPriority::Critical);
    Out.Add(Step3);

    // Mark as atomic and reversible
    Out.bAtomic = true;
    Out.bReversible = true;
}

void USuspenseEquipmentOperationExecutor::Expand_QuickSwitch(
    const FEquipmentOperationRequest& In,
    FTransactionPlan& Out) const
{
    if (!DataProvider.GetInterface())
    {
        return;
    }

    // Не затираем существующий префикс
    Out.DebugLabel += TEXT("QuickSwitch-");

    const int32 CurrentActive = GetCurrentActiveWeaponSlot();
    int32 TargetSlot = INDEX_NONE;

    // 1) Если в запросе явно указан TargetSlotIndex — уважаем его (пригоден и не равен текущему)
    if (In.TargetSlotIndex != INDEX_NONE
        && DataProvider->IsValidSlotIndex(In.TargetSlotIndex)
        && IsWeaponSlot(In.TargetSlotIndex)
        && DataProvider->IsSlotOccupied(In.TargetSlotIndex)
        && In.TargetSlotIndex != CurrentActive)
    {
        TargetSlot = In.TargetSlotIndex;
        Out.DebugLabel += TEXT("ExplicitTarget");
    }
    else
    {
        // 2) Иначе — стандартное циклическое переключение по приоритетам (Primary->Secondary->Holster->Scabbard)
        TargetSlot = FindNextWeaponSlot(CurrentActive);
        Out.DebugLabel += TEXT("NextInCycle");
    }

    // Нет валидной цели — ничего не планируем
    if (TargetSlot == INDEX_NONE || TargetSlot == CurrentActive)
    {
        Out.DebugLabel += TEXT("-NoValidTarget");
        return;
    }

    // Для совместимости с текущим TransactionProcessor представляем переключение как MOVE активности
    FEquipmentOperationRequest SwitchRequest = In;
    SwitchRequest.OperationType   = EEquipmentOperationType::Move;
    SwitchRequest.SourceSlotIndex = CurrentActive;
    SwitchRequest.TargetSlotIndex = TargetSlot;

    // Описание шага (если есть типы — показываем их; на случай отсутствия GetWeaponSlotType можно заменить на краткое)
    const EEquipmentSlotType FromType = GetWeaponSlotType(CurrentActive);
    const EEquipmentSlotType ToType   = GetWeaponSlotType(TargetSlot);

    const FString Description = FString::Printf(
        TEXT("Quick switch weapon: %s (slot %d) -> %s (slot %d)"),
        *UEnum::GetValueAsString(FromType), CurrentActive,
        *UEnum::GetValueAsString(ToType),   TargetSlot
    );

    FTransactionPlanStep Step(SwitchRequest, Description);
    Step.StepPriority = static_cast<int32>(EEquipmentOperationPriority::Critical);

    Out.Add(Step);
    Out.bReversible = true;
    Out.bAtomic     = true; // переключение должно быть атомарным
}

void USuspenseEquipmentOperationExecutor::Expand_Transfer(
    const FEquipmentOperationRequest& In,
    FTransactionPlan& Out) const
{
    // Transfer between containers - single step for now
    FTransactionPlanStep Step(In, TEXT("Transfer item between containers"));
    Step.StepPriority = static_cast<int32>(In.Priority);
    Out.Add(Step);
    Out.bReversible = true;
}

void USuspenseEquipmentOperationExecutor::Expand_Reload(
    const FEquipmentOperationRequest& In,
    FTransactionPlan& Out) const
{
    // Reload weapon - single step
    FTransactionPlanStep Step(In, TEXT("Reload weapon"));
    Step.StepPriority = static_cast<int32>(In.Priority);
    Out.Add(Step);
    Out.bReversible = false; // Can't un-reload
}

void USuspenseEquipmentOperationExecutor::Expand_Repair(
    const FEquipmentOperationRequest& In,
    FTransactionPlan& Out) const
{
    // Repair item - single step
    FTransactionPlanStep Step(In, TEXT("Repair item"));
    Step.StepPriority = static_cast<int32>(In.Priority);
    Out.Add(Step);
    Out.bReversible = false; // Can't un-repair
}

void USuspenseEquipmentOperationExecutor::Expand_Upgrade(
    const FEquipmentOperationRequest& In,
    FTransactionPlan& Out) const
{
    // Upgrade item - single step
    FTransactionPlanStep Step(In, TEXT("Upgrade item"));
    Step.StepPriority = static_cast<int32>(In.Priority);
    Step.bReversible = false; // Upgrades are permanent
    Out.Add(Step);
    Out.bReversible = false;
}

void USuspenseEquipmentOperationExecutor::Expand_Modify(
    const FEquipmentOperationRequest& In,
    FTransactionPlan& Out) const
{
    // Modify item - single step
    FTransactionPlanStep Step(In, TEXT("Modify item"));
    Step.StepPriority = static_cast<int32>(In.Priority);
    Out.Add(Step);
    Out.bReversible = true; // Modifications might be reversible
}

void USuspenseEquipmentOperationExecutor::Expand_Combine(
    const FEquipmentOperationRequest& In,
    FTransactionPlan& Out) const
{
    // Combine items - single step
    FTransactionPlanStep Step(In, TEXT("Combine items"));
    Step.StepPriority = static_cast<int32>(In.Priority);
    Step.bReversible = false; // Can't uncombine
    Out.Add(Step);
    Out.bReversible = false;
}

void USuspenseEquipmentOperationExecutor::Expand_Split(
    const FEquipmentOperationRequest& In,
    FTransactionPlan& Out) const
{
    // Split stack - single step
    FTransactionPlanStep Step(In, TEXT("Split item stack"));
    Step.StepPriority = static_cast<int32>(In.Priority);
    Out.Add(Step);
    Out.bReversible = true; // Can recombine
}

//========================================
// Validation Methods
//========================================

bool USuspenseEquipmentOperationExecutor::ValidateStep(
    const FTransactionPlanStep& Step,
    FText& OutError) const
{
    if (!bRequireValidation)
    {
        return true;
    }

    // Use existing validation methods
    FSlotValidationResult ValidationResult = ValidateOperation(Step.Request);

    if (!ValidationResult.bIsValid)
    {
        OutError = ValidationResult.ErrorMessage;
        return false;
    }

    return true;
}

FSlotValidationResult USuspenseEquipmentOperationExecutor::ValidateOperation(
    const FEquipmentOperationRequest& Request) const
{
    switch (Request.OperationType)
    {
        case EEquipmentOperationType::Equip:
            return ValidateEquip(Request);

        case EEquipmentOperationType::Unequip:
            return ValidateUnequip(Request);

        case EEquipmentOperationType::Swap:
            return ValidateSwap(Request);

        case EEquipmentOperationType::Move:
            return ValidateMove(Request);

        case EEquipmentOperationType::Drop:
            return ValidateDrop(Request);

        case EEquipmentOperationType::QuickSwitch:
            return ValidateQuickSwitch(Request);

        default:
            // For unhandled types, assume valid if no validator
            if (!SlotValidator.GetInterface())
            {
                return FSlotValidationResult::Success();
            }
            return FSlotValidationResult::Failure(
                NSLOCTEXT("Equipment", "UnknownOperation", "Unknown operation type"),
                EEquipmentValidationFailure::SystemError
            );
    }
}

FSlotValidationResult USuspenseEquipmentOperationExecutor::ValidateEquip(
    const FEquipmentOperationRequest& Request) const
{
    UE_LOG(LogTemp, Verbose, TEXT("=== ValidateEquip START ==="));
    UE_LOG(LogTemp, Verbose, TEXT("ItemID: %s"), *Request.ItemInstance.ItemID.ToString());
    UE_LOG(LogTemp, Verbose, TEXT("TargetSlot: %d"), Request.TargetSlotIndex);

    // Step 1: Validate item instance
    if (!Request.ItemInstance.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("ValidateEquip: Invalid item instance"));

        return FSlotValidationResult::Failure(
            NSLOCTEXT("Equipment", "InvalidItem", "Invalid item instance"),
            EEquipmentValidationFailure::SystemError,
            FGameplayTag::RequestGameplayTag(TEXT("Validation.Error.InvalidItem"))
        );
    }

    // Step 2: Validate DataProvider
    if (!DataProvider.GetInterface())
    {
        UE_LOG(LogTemp, Error, TEXT("ValidateEquip: No data provider"));

        return FSlotValidationResult::Failure(
            NSLOCTEXT("Equipment", "NoDataProvider", "Data provider not available"),
            EEquipmentValidationFailure::SystemError,
            FGameplayTag::RequestGameplayTag(TEXT("Validation.Error.NoProvider"))
        );
    }

    // Step 3: Validate slot index
    if (!DataProvider->IsValidSlotIndex(Request.TargetSlotIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("ValidateEquip: Invalid slot index %d"), Request.TargetSlotIndex);

        return FSlotValidationResult::Failure(
            NSLOCTEXT("Equipment", "InvalidSlot", "Invalid slot index"),
            EEquipmentValidationFailure::InvalidSlot,
            FGameplayTag::RequestGameplayTag(TEXT("Validation.Error.InvalidSlotIndex"))
        );
    }

    // Step 4: CRITICAL CHECK - Verify target slot is not already occupied
    // This is the PRIMARY cause of failed equip operations
    if (DataProvider->IsSlotOccupied(Request.TargetSlotIndex))
    {
        const FSuspenseInventoryItemInstance ExistingItem = DataProvider->GetSlotItem(Request.TargetSlotIndex);

        UE_LOG(LogTemp, Warning,
            TEXT("ValidateEquip FAILED: Slot %d is occupied by %s (instance %s)"),
            Request.TargetSlotIndex,
            *ExistingItem.ItemID.ToString(),
            *ExistingItem.InstanceID.ToString());

        FSlotValidationResult Result = FSlotValidationResult::Failure(
            FText::Format(
                NSLOCTEXT("Equipment", "SlotOccupied", "Slot {0} is already occupied by {1}. Unequip or use swap operation."),
                FText::AsNumber(Request.TargetSlotIndex),
                FText::FromName(ExistingItem.ItemID)
            ),
            EEquipmentValidationFailure::SlotOccupied,
            FGameplayTag::RequestGameplayTag(TEXT("Validation.Error.SlotOccupied"))
        );

        // Add diagnostic context
        Result.Context.Add(TEXT("OccupiedByItemID"), ExistingItem.ItemID.ToString());
        Result.Context.Add(TEXT("OccupiedByInstanceID"), ExistingItem.InstanceID.ToString());
        Result.Context.Add(TEXT("TargetSlotIndex"), FString::FromInt(Request.TargetSlotIndex));
        Result.Context.Add(TEXT("AttemptedItemID"), Request.ItemInstance.ItemID.ToString());

        return Result;
    }

    UE_LOG(LogTemp, Verbose, TEXT("✓ Slot %d is empty"), Request.TargetSlotIndex);

    // Step 5: Detailed compatibility validation through SlotValidator
    if (!SlotValidator.GetInterface())
    {
        // No validator available - permissive mode
        if (bEnableDetailedLogging)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("ValidateEquip: No SlotValidator, skipping detailed validation"));
        }

        UE_LOG(LogTemp, Verbose, TEXT("=== ValidateEquip END (Success - no validator) ==="));
        return FSlotValidationResult::Success();
    }

    // Get slot configuration
    FEquipmentSlotConfig SlotConfig = DataProvider->GetSlotConfiguration(Request.TargetSlotIndex);

    // Perform detailed validation (type, level, weight, etc.)
    FSlotValidationResult ValidationResult = SlotValidator->CanPlaceItemInSlot(
        SlotConfig,
        Request.ItemInstance
    );

    if (!ValidationResult.bIsValid)
    {
        // Add operation context to error
        ValidationResult.Context.Add(TEXT("OperationType"), TEXT("Equip"));
        ValidationResult.Context.Add(TEXT("OperationID"), Request.OperationId.ToString());
        ValidationResult.Context.Add(TEXT("RequestedSlot"), FString::FromInt(Request.TargetSlotIndex));
        ValidationResult.Context.Add(TEXT("ItemID"), Request.ItemInstance.ItemID.ToString());

        if (bEnableDetailedLogging)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("ValidateEquip FAILED: %s (Type: %s)"),
                *ValidationResult.ErrorMessage.ToString(),
                *UEnum::GetValueAsString(ValidationResult.FailureType));
        }

        UE_LOG(LogTemp, Verbose, TEXT("=== ValidateEquip END (Failed) ==="));
        return ValidationResult;
    }

    UE_LOG(LogTemp, Verbose, TEXT("✓ Passed detailed validation"));
    UE_LOG(LogTemp, Verbose, TEXT("=== ValidateEquip END (Success) ==="));

    return FSlotValidationResult::Success();
}

FSlotValidationResult USuspenseEquipmentOperationExecutor::ValidateUnequip(
    const FEquipmentOperationRequest& Request) const
{
    if (!DataProvider.GetInterface() || !DataProvider->IsValidSlotIndex(Request.SourceSlotIndex))
    {
        return FSlotValidationResult::Failure(
            NSLOCTEXT("Equipment", "InvalidSlot", "Invalid slot index"),
            EEquipmentValidationFailure::InvalidSlot
        );
    }

    if (!DataProvider->IsSlotOccupied(Request.SourceSlotIndex))
    {
        return FSlotValidationResult::Failure(
            NSLOCTEXT("Equipment", "SlotEmpty", "Slot is empty"),
            EEquipmentValidationFailure::InvalidSlot
        );
    }

    return FSlotValidationResult::Success();
}

FSlotValidationResult USuspenseEquipmentOperationExecutor::ValidateSwap(
    const FEquipmentOperationRequest& Request) const
{
    if (!DataProvider.GetInterface())
    {
        return FSlotValidationResult::Failure(
            NSLOCTEXT("Equipment", "NoDataProvider", "Data provider not available"),
            EEquipmentValidationFailure::SystemError
        );
    }

    if (!DataProvider->IsValidSlotIndex(Request.SourceSlotIndex) ||
        !DataProvider->IsValidSlotIndex(Request.TargetSlotIndex))
    {
        return FSlotValidationResult::Failure(
            NSLOCTEXT("Equipment", "InvalidSlots", "Invalid slot indices"),
            EEquipmentValidationFailure::InvalidSlot
        );
    }

    if (!SlotValidator.GetInterface())
    {
        return FSlotValidationResult::Success();
    }

    FSuspenseInventoryItemInstance ItemA = DataProvider->GetSlotItem(Request.SourceSlotIndex);
    FSuspenseInventoryItemInstance ItemB = DataProvider->GetSlotItem(Request.TargetSlotIndex);
    FEquipmentSlotConfig ConfigA = DataProvider->GetSlotConfiguration(Request.SourceSlotIndex);
    FEquipmentSlotConfig ConfigB = DataProvider->GetSlotConfiguration(Request.TargetSlotIndex);

    return SlotValidator->CanSwapItems(ConfigA, ItemA, ConfigB, ItemB);
}

FSlotValidationResult USuspenseEquipmentOperationExecutor::ValidateMove(
    const FEquipmentOperationRequest& Request) const
{
    if (!DataProvider.GetInterface())
    {
        return FSlotValidationResult::Failure(
            NSLOCTEXT("Equipment", "NoDataProvider", "Data provider not available"),
            EEquipmentValidationFailure::SystemError
        );
    }

    if (!DataProvider->IsValidSlotIndex(Request.SourceSlotIndex) ||
        !DataProvider->IsValidSlotIndex(Request.TargetSlotIndex))
    {
        return FSlotValidationResult::Failure(
            NSLOCTEXT("Equipment", "InvalidSlots", "Invalid slot indices"),
            EEquipmentValidationFailure::InvalidSlot
        );
    }

    if (!DataProvider->IsSlotOccupied(Request.SourceSlotIndex))
    {
        return FSlotValidationResult::Failure(
            NSLOCTEXT("Equipment", "SourceEmpty", "Source slot is empty"),
            EEquipmentValidationFailure::InvalidSlot
        );
    }

    if (DataProvider->IsSlotOccupied(Request.TargetSlotIndex))
    {
        return FSlotValidationResult::Failure(
            NSLOCTEXT("Equipment", "TargetOccupied", "Target slot is not empty"),
            EEquipmentValidationFailure::SlotOccupied
        );
    }

    if (!SlotValidator.GetInterface())
    {
        return FSlotValidationResult::Success();
    }

    FSuspenseInventoryItemInstance Item = DataProvider->GetSlotItem(Request.SourceSlotIndex);
    FEquipmentSlotConfig TargetConfig = DataProvider->GetSlotConfiguration(Request.TargetSlotIndex);

    return SlotValidator->CanPlaceItemInSlot(TargetConfig, Item);
}

FSlotValidationResult USuspenseEquipmentOperationExecutor::ValidateDrop(
    const FEquipmentOperationRequest& Request) const
{
    if (!DataProvider.GetInterface() || !DataProvider->IsValidSlotIndex(Request.SourceSlotIndex))
    {
        return FSlotValidationResult::Failure(
            NSLOCTEXT("Equipment", "InvalidSlot", "Invalid slot index"),
            EEquipmentValidationFailure::InvalidSlot
        );
    }

    if (!DataProvider->IsSlotOccupied(Request.SourceSlotIndex))
    {
        return FSlotValidationResult::Failure(
            NSLOCTEXT("Equipment", "SlotEmpty", "Slot is empty"),
            EEquipmentValidationFailure::InvalidSlot
        );
    }

    // Check if item can be dropped
    FEquipmentSlotConfig SlotConfig = DataProvider->GetSlotConfiguration(Request.SourceSlotIndex);
    if (SlotConfig.bIsRequired)
    {
        return FSlotValidationResult::Failure(
            NSLOCTEXT("Equipment", "RequiredSlot", "Cannot drop item from required slot"),
            EEquipmentValidationFailure::RequirementsNotMet
        );
    }

    return FSlotValidationResult::Success();
}

FSlotValidationResult USuspenseEquipmentOperationExecutor::ValidateQuickSwitch(
    const FEquipmentOperationRequest& Request) const
{
    if (!DataProvider.GetInterface())
    {
        return FSlotValidationResult::Failure(
            NSLOCTEXT("Equipment", "NoDataProvider", "Data provider not available"),
            EEquipmentValidationFailure::SystemError
        );
    }

    int32 CurrentActive = GetCurrentActiveWeaponSlot();
    int32 NextWeapon = FindNextWeaponSlot(CurrentActive);

    if (NextWeapon == INDEX_NONE || NextWeapon == CurrentActive)
    {
        return FSlotValidationResult::Failure(
            NSLOCTEXT("Equipment", "NoOtherWeapon", "No other weapon available"),
            EEquipmentValidationFailure::RequirementsNotMet
        );
    }

    return FSlotValidationResult::Success();
}

//========================================
// ISuspenseEquipmentOperations Implementation (Legacy)
//========================================

FEquipmentOperationResult USuspenseEquipmentOperationExecutor::ExecuteOperation(
    const FEquipmentOperationRequest& Request)
{
    UE_LOG(LogEquipmentExecutor, Error, TEXT("╔═══════════════════════════════════════════════════════════╗"));
    UE_LOG(LogEquipmentExecutor, Error, TEXT("║  ExecuteOperation START (PLANNER MODE)                   ║"));
    UE_LOG(LogEquipmentExecutor, Error, TEXT("╚═══════════════════════════════════════════════════════════╝"));
    UE_LOG(LogEquipmentExecutor, Error, TEXT("Operation Type: %s"),
        *UEnum::GetValueAsString(Request.OperationType));
    UE_LOG(LogEquipmentExecutor, Error, TEXT("Item ID:        %s"),
        *Request.ItemInstance.ItemID.ToString());
    UE_LOG(LogEquipmentExecutor, Error, TEXT("Instance ID:    %s"),
        *Request.ItemInstance.InstanceID.ToString());
    UE_LOG(LogEquipmentExecutor, Error, TEXT("Target Slot:    %d"),
        Request.TargetSlotIndex);

    FScopeLock Lock(&PlanningCriticalSection);

    // Build plan
    FTransactionPlan Plan;
    FText PlanError;

    UE_LOG(LogEquipmentExecutor, Warning, TEXT("Building execution plan..."));

    if (!BuildPlan(Request, Plan, PlanError))
    {
        UE_LOG(LogEquipmentExecutor, Error, TEXT("❌ Plan building FAILED: %s"),
            *PlanError.ToString());

        return FEquipmentOperationResult::CreateFailure(
            Request.OperationId,
            PlanError,
            EEquipmentValidationFailure::SystemError
        );
    }

    UE_LOG(LogEquipmentExecutor, Log, TEXT("✓ Plan built successfully: %d steps"),
        Plan.Num());

    // Validate plan if required
    if (bRequireValidation && SlotValidator.GetInterface())
    {
        UE_LOG(LogEquipmentExecutor, Warning, TEXT("Validating plan..."));

        FText ValidationError;
        if (!ValidatePlan(Plan, ValidationError))
        {
            UE_LOG(LogEquipmentExecutor, Error, TEXT("❌ Plan validation FAILED: %s"),
                *ValidationError.ToString());

            return FEquipmentOperationResult::CreateFailure(
                Request.OperationId,
                ValidationError,
                EEquipmentValidationFailure::RequirementsNotMet
            );
        }

        UE_LOG(LogEquipmentExecutor, Log, TEXT("✓ Plan validated successfully"));
    }

    // Return success with plan metadata
    // ⚠️ ВАЖНО: Actual execution happens in the service layer!
    FEquipmentOperationResult Result = FEquipmentOperationResult::CreateSuccess(Request.OperationId);
    Result.ResultMetadata.Add(TEXT("PlanId"), Plan.PlanId.ToString());
    Result.ResultMetadata.Add(TEXT("PlanSteps"), FString::FromInt(Plan.Num()));
    Result.ResultMetadata.Add(TEXT("EstimatedMs"), FString::SanitizeFloat(Plan.EstimatedExecutionTimeMs));
    Result.ResultMetadata.Add(TEXT("Idempotent"), Plan.bIdempotent ? TEXT("true") : TEXT("false"));

    UE_LOG(LogEquipmentExecutor, Warning, TEXT("⚠️ ExecuteOperation returns SUCCESS (plan created)"));
    UE_LOG(LogEquipmentExecutor, Warning, TEXT("   BUT actual execution must happen in SERVICE LAYER!"));
    UE_LOG(LogEquipmentExecutor, Warning, TEXT("   Plan ID: %s"), *Plan.PlanId.ToString());
    UE_LOG(LogEquipmentExecutor, Warning, TEXT("   Steps: %d"), Plan.Num());
    UE_LOG(LogEquipmentExecutor, Error, TEXT("╚═══════════════════════════════════════════════════════════╝"));

    return Result;
}

FEquipmentOperationResult USuspenseEquipmentOperationExecutor::EquipItem(
    const FSuspenseInventoryItemInstance& ItemInstance,
    int32 SlotIndex)
{
    FEquipmentOperationRequest Request;
    Request.OperationType = EEquipmentOperationType::Equip;
    Request.ItemInstance = ItemInstance;
    Request.TargetSlotIndex = SlotIndex;
    Request.OperationId = GenerateOperationId();
    Request.Timestamp = FPlatformTime::Seconds();

    return ExecuteOperation(Request);
}

FEquipmentOperationResult USuspenseEquipmentOperationExecutor::UnequipItem(int32 SlotIndex)
{
    FEquipmentOperationRequest Request;
    Request.OperationType = EEquipmentOperationType::Unequip;
    Request.SourceSlotIndex = SlotIndex;
    Request.OperationId = GenerateOperationId();
    Request.Timestamp = FPlatformTime::Seconds();

    return ExecuteOperation(Request);
}

FEquipmentOperationResult USuspenseEquipmentOperationExecutor::SwapItems(
    int32 SlotIndexA,
    int32 SlotIndexB)
{
    FEquipmentOperationRequest Request;
    Request.OperationType = EEquipmentOperationType::Swap;
    Request.SourceSlotIndex = SlotIndexA;
    Request.TargetSlotIndex = SlotIndexB;
    Request.OperationId = GenerateOperationId();
    Request.Timestamp = FPlatformTime::Seconds();

    return ExecuteOperation(Request);
}

FEquipmentOperationResult USuspenseEquipmentOperationExecutor::MoveItem(
    int32 SourceSlot,
    int32 TargetSlot)
{
    FEquipmentOperationRequest Request;
    Request.OperationType = EEquipmentOperationType::Move;
    Request.SourceSlotIndex = SourceSlot;
    Request.TargetSlotIndex = TargetSlot;
    Request.OperationId = GenerateOperationId();
    Request.Timestamp = FPlatformTime::Seconds();

    return ExecuteOperation(Request);
}

FEquipmentOperationResult USuspenseEquipmentOperationExecutor::DropItem(int32 SlotIndex)
{
    FEquipmentOperationRequest Request;
    Request.OperationType = EEquipmentOperationType::Drop;
    Request.SourceSlotIndex = SlotIndex;
    Request.OperationId = GenerateOperationId();
    Request.Timestamp = FPlatformTime::Seconds();

    return ExecuteOperation(Request);
}

FEquipmentOperationResult USuspenseEquipmentOperationExecutor::QuickSwitchWeapon()
{
    FEquipmentOperationRequest Request;
    Request.OperationType = EEquipmentOperationType::QuickSwitch;
    Request.OperationId = GenerateOperationId();
    Request.Timestamp = FPlatformTime::Seconds();

    return ExecuteOperation(Request);
}

// History methods - deprecated
TArray<FEquipmentOperationResult> USuspenseEquipmentOperationExecutor::GetOperationHistory(
    int32 MaxCount) const
{
    // History is now managed by the service/transaction layer
    return TArray<FEquipmentOperationResult>();
}

bool USuspenseEquipmentOperationExecutor::CanUndoLastOperation() const
{
    // Undo is now managed by the service/transaction layer
    return false;
}

FEquipmentOperationResult USuspenseEquipmentOperationExecutor::UndoLastOperation()
{
    // Undo is now managed by the service/transaction layer
    return FEquipmentOperationResult::CreateFailure(
        FGuid::NewGuid(),
        NSLOCTEXT("Equipment", "UndoMovedToService", "Undo functionality has been moved to service layer"),
        EEquipmentValidationFailure::SystemError
    );
}

//========================================
// Configuration Methods
//========================================

bool USuspenseEquipmentOperationExecutor::Initialize(
    TScriptInterface<ISuspenseEquipmentDataProvider> InDataProvider,
    TScriptInterface<ISuspenseSlotValidator> InValidator)
{
    if (!InDataProvider.GetInterface())
    {
        UE_LOG(LogEquipmentExecutor, Error, TEXT("Invalid data provider provided"));
        return false;
    }

    DataProvider = InDataProvider;
    SlotValidator = InValidator; // Validator is optional

    UE_LOG(LogEquipmentExecutor, Log,
        TEXT("Executor initialized with data provider. Validator: %s"),
        SlotValidator.GetInterface() ? TEXT("Present") : TEXT("Absent"));

    return true;
}

bool USuspenseEquipmentOperationExecutor::IsInitialized() const
{
    return DataProvider.GetInterface() != nullptr;
}

void USuspenseEquipmentOperationExecutor::ResetStatistics()
{
    TotalPlansBuilt.store(0);
    SuccessfulValidations.store(0);
    FailedValidations.store(0);
    AveragePlanSize.store(0.0f);
}

FString USuspenseEquipmentOperationExecutor::GetStatistics() const
{
    return FString::Printf(
        TEXT("Plans Built: %d, Validations: %d/%d, Avg Plan Size: %.1f"),
        TotalPlansBuilt.load(),
        SuccessfulValidations.load(),
        SuccessfulValidations.load() + FailedValidations.load(),
        AveragePlanSize.load()
    );
}

//========================================
// Helper Methods
//========================================

FGuid USuspenseEquipmentOperationExecutor::GenerateOperationId() const
{
    return FGuid::NewGuid();
}

int32 USuspenseEquipmentOperationExecutor::FindBestSlotForItem(
    const FSuspenseInventoryItemInstance& ItemInstance) const
{
    if (!ItemInstance.IsValid() || !DataProvider.GetInterface())
    {
        return INDEX_NONE;
    }

    // Find first compatible empty slot
    for (int32 i = 0; i < DataProvider->GetSlotCount(); i++)
    {
        if (!DataProvider->IsSlotOccupied(i))
        {
            if (!SlotValidator.GetInterface())
            {
                return i; // No validator - use first empty
            }

            FEquipmentSlotConfig SlotConfig = DataProvider->GetSlotConfiguration(i);
            FSlotValidationResult ValidationResult = SlotValidator->CanPlaceItemInSlot(SlotConfig, ItemInstance);

            if (ValidationResult.bIsValid)
            {
                return i;
            }
        }
    }

    return INDEX_NONE;
}

// Обновленный метод IsWeaponSlot с поддержкой новых типов слотов из LoadoutSettings
bool USuspenseEquipmentOperationExecutor::IsWeaponSlot(int32 SlotIndex) const
{
    if (!DataProvider.GetInterface() || !DataProvider->IsValidSlotIndex(SlotIndex))
    {
        return false;
    }

    FEquipmentSlotConfig SlotConfig = DataProvider->GetSlotConfiguration(SlotIndex);

    // Поддерживаем новые типы слотов согласно LoadoutSettings.h
    // PrimaryWeapon - основное оружие (AR, DMR, SR, Shotgun, LMG)
    // SecondaryWeapon - вторичное оружие (SMG, PDW, Shotgun)
    // Holster - пистолеты и револьверы
    // Scabbard - холодное оружие (ножи)
    return SlotConfig.SlotType == EEquipmentSlotType::PrimaryWeapon ||
           SlotConfig.SlotType == EEquipmentSlotType::SecondaryWeapon ||
           SlotConfig.SlotType == EEquipmentSlotType::Holster ||
           SlotConfig.SlotType == EEquipmentSlotType::Scabbard;
}

// Статический метод для оптимизации - определение приоритета слотов
static inline int32 GetWeaponSlotPriorityStatic(EEquipmentSlotType SlotType)
{
    // Приоритетность переключения оружия для Tarkov-style геймплея
    // Меньшее значение = выше приоритет при quick switch
    switch (SlotType)
    {
        case EEquipmentSlotType::PrimaryWeapon:
            return 1; // Высший приоритет - основное оружие
        case EEquipmentSlotType::SecondaryWeapon:
            return 2; // Вторичное оружие
        case EEquipmentSlotType::Holster:
            return 3; // Пистолет в кобуре
        case EEquipmentSlotType::Scabbard:
            return 4; // Нож - низший приоритет
        default:
            return 999; // Не оружейный слот
    }
}

// Публичный метод-обертка
int32 USuspenseEquipmentOperationExecutor::GetWeaponSlotPriority(EEquipmentSlotType SlotType) const
{
    return GetWeaponSlotPriorityStatic(SlotType);
}

int32 USuspenseEquipmentOperationExecutor::GetCurrentActiveWeaponSlot() const
{
    if (!DataProvider.GetInterface())
    {
        return INDEX_NONE;
    }

    return DataProvider->GetActiveWeaponSlot();
}

// Умное переключение между слотами оружия с учетом приоритетов
int32 USuspenseEquipmentOperationExecutor::FindNextWeaponSlot(int32 CurrentSlot) const
{
    if (!DataProvider.GetInterface())
    {
        return INDEX_NONE;
    }

    int32 SlotCount = DataProvider->GetSlotCount();

    // Структура для хранения информации о доступных слотах
    struct FWeaponSlotInfo
    {
        int32 SlotIndex;
        EEquipmentSlotType SlotType;
        int32 Priority;
    };

    TArray<FWeaponSlotInfo> AvailableWeaponSlots;

    // Собираем все доступные слоты с оружием
    for (int32 i = 0; i < SlotCount; i++)
    {
        if (i != CurrentSlot && IsWeaponSlot(i) && DataProvider->IsSlotOccupied(i))
        {
            FEquipmentSlotConfig SlotConfig = DataProvider->GetSlotConfiguration(i);
            FWeaponSlotInfo Info;
            Info.SlotIndex = i;
            Info.SlotType = SlotConfig.SlotType;
            Info.Priority = GetWeaponSlotPriorityStatic(SlotConfig.SlotType);
            AvailableWeaponSlots.Add(Info);
        }
    }

    if (AvailableWeaponSlots.Num() == 0)
    {
        return INDEX_NONE;
    }

    // Сортируем по приоритету для предсказуемого переключения
    // При одинаковом приоритете сохраняется порядок индексов слотов (tie-break: по возрастанию индекса)
    AvailableWeaponSlots.Sort([](const FWeaponSlotInfo& A, const FWeaponSlotInfo& B)
    {
        if (A.Priority == B.Priority)
        {
            return A.SlotIndex < B.SlotIndex; // При одинаковом приоритете - по индексу
        }
        return A.Priority < B.Priority;
    });

    // Определяем текущий приоритет для циклического переключения
    int32 CurrentPriority = 999;
    if (CurrentSlot != INDEX_NONE && DataProvider->IsValidSlotIndex(CurrentSlot))
    {
        FEquipmentSlotConfig CurrentConfig = DataProvider->GetSlotConfiguration(CurrentSlot);
        CurrentPriority = GetWeaponSlotPriorityStatic(CurrentConfig.SlotType);
    }

    // Ищем следующий слот с более низким приоритетом
    for (const FWeaponSlotInfo& SlotInfo : AvailableWeaponSlots)
    {
        if (SlotInfo.Priority > CurrentPriority)
        {
            return SlotInfo.SlotIndex;
        }
    }

    // Если не нашли слот с более низким приоритетом, возвращаем первый доступный
    // Это обеспечивает циклическое переключение
    return AvailableWeaponSlots[0].SlotIndex;
}

// Проверка на огнестрельное оружие
// Используется для UI подсказок амуниции и анимаций прицеливания (aiming state machine)
bool USuspenseEquipmentOperationExecutor::IsFirearmSlot(int32 SlotIndex) const
{
    if (!DataProvider.GetInterface() || !DataProvider->IsValidSlotIndex(SlotIndex))
    {
        return false;
    }

    FEquipmentSlotConfig SlotConfig = DataProvider->GetSlotConfiguration(SlotIndex);

    // Только слоты для огнестрельного оружия (исключаем холодное)
    // Используется для определения возможности прицеливания и показа UI амуниции
    return SlotConfig.SlotType == EEquipmentSlotType::PrimaryWeapon ||
           SlotConfig.SlotType == EEquipmentSlotType::SecondaryWeapon ||
           SlotConfig.SlotType == EEquipmentSlotType::Holster;
}

// Проверка на холодное оружие
// Используется для специальных анимаций атаки (melee anim state machines)
bool USuspenseEquipmentOperationExecutor::IsMeleeWeaponSlot(int32 SlotIndex) const
{
    if (!DataProvider.GetInterface() || !DataProvider->IsValidSlotIndex(SlotIndex))
    {
        return false;
    }

    FEquipmentSlotConfig SlotConfig = DataProvider->GetSlotConfiguration(SlotIndex);

    // Только слот для холодного оружия
    // Используется для переключения на melee combat mode
    return SlotConfig.SlotType == EEquipmentSlotType::Scabbard;
}

// Получение типа оружейного слота
EEquipmentSlotType USuspenseEquipmentOperationExecutor::GetWeaponSlotType(int32 SlotIndex) const
{
    if (!DataProvider.GetInterface() || !DataProvider->IsValidSlotIndex(SlotIndex))
    {
        return EEquipmentSlotType::None;
    }

    FEquipmentSlotConfig SlotConfig = DataProvider->GetSlotConfiguration(SlotIndex);

    // Возвращаем тип только для валидных оружейных слотов
    if (IsWeaponSlot(SlotIndex))
    {
        return SlotConfig.SlotType;
    }

    return EEquipmentSlotType::None;
}
