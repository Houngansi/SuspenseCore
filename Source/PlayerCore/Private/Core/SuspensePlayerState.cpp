// SuspensePlayerState.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "Core/SuspensePlayerState.h"
#include "Core/SuspenseGameInstance.h"
#include "Components/SuspenseAbilitySystemComponent.h"
#include "Components/SuspenseInventoryComponent.h"
#include "Attributes/SuspenseBaseAttributeSet.h"
#include "Attributes/SuspenseDefaultAttributeSet.h"
#include "Effects/SuspenseInitialAttributesEffect.h"
#include "Types/Loadout/SuspenseLoadoutManager.h"
#include "Types/Loadout/LoadoutSettings.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "Characters/SuspenseCharacter.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystemGlobals.h"
#include "GameplayAbilitySpec.h"
#include "Engine/ActorChannel.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Delegates/SuspenseEventManager.h"
#include "Interfaces/Core/ISuspenseCharacter.h"
#include "Interfaces/Core/ISuspenseAttributeProvider.h"
#include "Interfaces/Core/ISuspenseLoadout.h"

// Equipment module
#include "Components/Core/SuspenseEquipmentDataStore.h"
#include "Components/Transaction/SuspenseEquipmentTransactionProcessor.h"
#include "Components/Core/SuspenseEquipmentOperationExecutor.h"
#include "Components/Network/SuspenseEquipmentReplicationManager.h"
#include "Components/Network/SuspenseEquipmentPredictionSystem.h"
#include "Components/Network/SuspenseEquipmentNetworkDispatcher.h"
#include "Components/Core/SuspenseEquipmentInventoryBridge.h"
#include "Components/Coordination/SuspenseEquipmentEventDispatcher.h"
#include "Components/Core/SuspenseWeaponStateManager.h"
#include "Components/Core/SuspenseSystemCoordinator.h"

// Services
#include "Services/EquipmentNetworkServiceImpl.h"
#include "Services/EquipmentDataServiceImpl.h"
#include "Services/EquipmentOperationServiceImpl.h"
#include "Services/EquipmentValidationServiceImpl.h"
#include "Services/EquipmentAbilityServiceImpl.h"
#include "Components/Rules/SuspenseRulesCoordinator.h"
#include "Core/Services/EquipmentServiceLocator.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "Subsystems/SuspenseSystemCoordinatorSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspensePlayerState, Log, All);

ASuspensePlayerState::ASuspensePlayerState()
{
    // ASC
    ASC = CreateDefaultSubobject<USuspenseAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    ASC->SetIsReplicated(true);
    ASC->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    // Inventory
    InventoryComponent = CreateDefaultSubobject<USuspenseInventoryComponent>(TEXT("InventoryComponent"));
    InventoryComponent->SetIsReplicated(true);
    
    // Attribute config
    InitialAttributeSetClass = TSubclassOf<USuspenseBaseAttributeSet>(UMedComDefaultAttributeSet::StaticClass());
    InitialAttributesEffect = TSubclassOf<UGameplayEffect>(UMedComInitialAttributesEffect::StaticClass());

    // Weapon params
    bHasWeapon = false;
    CurrentWeaponActor = nullptr;
    
    // Loadout defaults
    CurrentLoadoutID = NAME_None;
    DefaultLoadoutID = TEXT("Default_Soldier");
    bAutoApplyDefaultLoadout = true;
    bLogLoadoutOperations = true;
    bComponentListenersSetup = false;
    
    SprintingTag = FGameplayTag::RequestGameplayTag(TEXT("State.Sprinting"));

    //========================================
    // Equipment Module Components (Per-Player)
    //========================================
    
    EquipmentDataStore = CreateDefaultSubobject<USuspenseEquipmentDataStore>(TEXT("EquipmentDataStore"));
    EquipmentDataStore->SetIsReplicated(true);

    EquipmentTxnProcessor = CreateDefaultSubobject<USuspenseEquipmentTransactionProcessor>(TEXT("EquipmentTxnProcessor"));
    EquipmentTxnProcessor->SetIsReplicated(true);

    EquipmentOps = CreateDefaultSubobject<USuspenseEquipmentOperationExecutor>(TEXT("EquipmentOperationExecutor"));
    EquipmentOps->SetIsReplicated(true);

    EquipmentReplication = CreateDefaultSubobject<USuspenseEquipmentReplicationManager>(TEXT("EquipmentReplicationManager"));
    EquipmentReplication->SetIsReplicated(true);

    EquipmentPrediction = CreateDefaultSubobject<USuspenseEquipmentPredictionSystem>(TEXT("EquipmentPredictionSystem"));
    EquipmentPrediction->SetIsReplicated(true);

    EquipmentNetworkDispatcher = CreateDefaultSubobject<USuspenseEquipmentNetworkDispatcher>(TEXT("EquipmentNetworkDispatcher"));
    EquipmentNetworkDispatcher->SetIsReplicated(true);

    EquipmentEventDispatcher = CreateDefaultSubobject<USuspenseEquipmentEventDispatcher>(TEXT("EquipmentEventDispatcher"));
    EquipmentEventDispatcher->SetIsReplicated(true);

    WeaponStateManager = CreateDefaultSubobject<USuspenseWeaponStateManager>(TEXT("WeaponStateManager"));
    WeaponStateManager->SetIsReplicated(true);

    EquipmentInventoryBridge = CreateDefaultSubobject<USuspenseEquipmentInventoryBridge>(TEXT("EquipmentInventoryBridge"));
    EquipmentInventoryBridge->SetIsReplicated(true);

    // DEPRECATED: Kept for backward compatibility
    EquipmentSystemCoordinator = CreateDefaultSubobject<USuspenseSystemCoordinator>(TEXT("SystemCoordinator"));
    EquipmentSystemCoordinator->SetIsReplicated(true);

    // Validator created during WireEquipmentModule()
    EquipmentSlotValidator = nullptr;
}


void ASuspensePlayerState::BeginPlay()
{
    Super::BeginPlay();

    // Attributes
    InitAttributes();

    // Abilities
    GrantStartupAbilities();

    // Passive effects
    ApplyPassiveStartupEffects();
    
    // Attribute callbacks
    SetupAttributeChangeCallbacks();
    
    // Apply loadout on server
    if (HasAuthority())
    {
        USuspenseGameInstance* GameInstance = Cast<USuspenseGameInstance>(GetGameInstance());
        if (!GameInstance)
        {
            UE_LOG(LogSuspensePlayerState, Error, TEXT("Failed to get SuspenseGameInstance"));
            return;
        }
        
        USuspenseLoadoutManager* LoadoutManager = GameInstance->GetLoadoutManager();
        if (!LoadoutManager)
        {
            UE_LOG(LogSuspensePlayerState, Error, TEXT("LoadoutManager not available from GameInstance"));
            return;
        }
        
        TArray<FName> AvailableLoadouts = LoadoutManager->GetAllLoadoutIDs();
        if (AvailableLoadouts.Num() == 0)
        {
            UE_LOG(LogSuspensePlayerState, Error, TEXT("No loadouts available in LoadoutManager!"));
            return;
        }
        
        FName LoadoutToApply = GameInstance->GetDefaultLoadoutID();
        if (LoadoutToApply.IsNone())
        {
            LoadoutToApply = DefaultLoadoutID;
        }
        
        if (!LoadoutManager->IsLoadoutValid(LoadoutToApply))
        {
            UE_LOG(LogSuspensePlayerState, Warning, TEXT("Loadout '%s' not valid, using first available"), 
                *LoadoutToApply.ToString());
            LoadoutToApply = AvailableLoadouts[0];
        }

        UE_LOG(LogSuspensePlayerState, Log, TEXT("Applying loadout '%s' to player %s"), 
            *LoadoutToApply.ToString(), *GetPlayerName());
            
        FLoadoutApplicationResult Result = ApplyLoadoutConfiguration_Implementation(
            LoadoutToApply, LoadoutManager, false);
        
        if (Result.bSuccess)
        {
            // ✅ ДИАГНОСТИКА: Проверяем состояние всех компонентов перед wiring
            UE_LOG(LogSuspensePlayerState, Warning, TEXT("=== LOADOUT APPLIED SUCCESSFULLY ==="));
            UE_LOG(LogSuspensePlayerState, Warning, TEXT("Loadout: %s"), *LoadoutToApply.ToString());
            UE_LOG(LogSuspensePlayerState, Warning, TEXT("Now verifying equipment components before wiring..."));
            
            UE_LOG(LogSuspensePlayerState, Warning, TEXT("Component Verification:"));
            UE_LOG(LogSuspensePlayerState, Warning, TEXT("  - EquipmentDataStore: %s"), 
                EquipmentDataStore ? TEXT("✓ OK") : TEXT("✗ NULL"));
            UE_LOG(LogSuspensePlayerState, Warning, TEXT("  - EquipmentTxnProcessor: %s"), 
                EquipmentTxnProcessor ? TEXT("✓ OK") : TEXT("✗ NULL"));
            UE_LOG(LogSuspensePlayerState, Warning, TEXT("  - EquipmentOps: %s"), 
                EquipmentOps ? TEXT("✓ OK") : TEXT("✗ NULL"));
            UE_LOG(LogSuspensePlayerState, Warning, TEXT("  - EquipmentInventoryBridge: %s"), 
                EquipmentInventoryBridge ? TEXT("✓ OK") : TEXT("✗ NULL"));
            UE_LOG(LogSuspensePlayerState, Warning, TEXT("  - InventoryComponent: %s"), 
                InventoryComponent ? TEXT("✓ OK") : TEXT("✗ NULL"));
            
            // Проверка что DataStore был инициализирован с слотами
            if (EquipmentDataStore)
            {
                const int32 SlotCount = EquipmentDataStore->GetSlotCount();
                UE_LOG(LogSuspensePlayerState, Warning, TEXT("  - DataStore Slots: %d"), SlotCount);
                
                if (SlotCount == 0)
                {
                    UE_LOG(LogSuspensePlayerState, Warning, 
                        TEXT("  ⚠ WARNING: DataStore has 0 slots! Loadout may not have applied correctly."));
                }
            }
            
            UE_LOG(LogSuspensePlayerState, Warning, TEXT("Starting Equipment Module wiring with retry mechanism..."));
            
            // ✅ Запускаем retry-механизм для подключения equipment module
            EquipmentWireRetryCount = 0;

            // Lambda для retry логики
            auto RetryLambda = [this, LoadoutToApply]()
            {
                // Пытаемся подключить equipment module
                if (TryWireEquipmentModuleOnce())
                {
                    // Успех - останавливаем таймер
                    GetWorld()->GetTimerManager().ClearTimer(EquipmentWireRetryHandle);

                    // Broadcast события об успешной инициализации
                    if (USuspenseEventManager* EventManager = GetDelegateManager())
                    {
                        const FGameplayTag InvInit = FGameplayTag::RequestGameplayTag(
                            TEXT("Player.Inventory.Initialized"));
                        const FGameplayTag EqInit = FGameplayTag::RequestGameplayTag(
                            TEXT("Player.Equipment.Initialized"));
                        const FGameplayTag LoadoutReady = FGameplayTag::RequestGameplayTag(
                            TEXT("Player.Loadout.Ready"));

                        const FString Payload = FString::Printf(TEXT("PlayerState:%s,LoadoutID:%s"),
                            *GetPlayerName(), *LoadoutToApply.ToString());
                        
                        EventManager->BroadcastGenericEvent(this, InvInit, Payload);
                        EventManager->BroadcastGenericEvent(this, EqInit, Payload);
                        EventManager->BroadcastGenericEvent(this, LoadoutReady, LoadoutToApply.ToString());
                    }
                    
                    UE_LOG(LogSuspensePlayerState, Warning, 
                        TEXT("=== Equipment initialization COMPLETE for player %s ==="), 
                        *GetPlayerName());
                    return;
                }

                // Не готово - проверяем лимит попыток
                ++EquipmentWireRetryCount;
                
                if (EquipmentWireRetryCount >= MaxEquipmentWireRetries)
                {
                    // Превышен лимит - останавливаем таймер и логируем ошибку
                    UE_LOG(LogSuspensePlayerState, Error,
                        TEXT("✗✗✗ Equipment wiring FAILED after %d retries for player %s ✗✗✗"),
                        MaxEquipmentWireRetries, *GetPlayerName());
                    UE_LOG(LogSuspensePlayerState, Error, 
                        TEXT("     Equipment-Inventory integration will NOT be available!"));
                    UE_LOG(LogSuspensePlayerState, Error,
                        TEXT("     Check that MedComSystemCoordinatorSubsystem initialized properly."));
                    
                    GetWorld()->GetTimerManager().ClearTimer(EquipmentWireRetryHandle);
                    
                    // Broadcast события об ошибке
                    if (USuspenseEventManager* EventManager = GetDelegateManager())
                    {
                        EventManager->BroadcastGenericEvent(this, 
                            FGameplayTag::RequestGameplayTag(TEXT("Player.Equipment.Failed")),
                            TEXT("Services initialization timeout"));
                    }
                }
                else
                {
                    UE_LOG(LogSuspensePlayerState, Verbose, 
                        TEXT("Retry attempt %d/%d - waiting for global services..."),
                        EquipmentWireRetryCount, MaxEquipmentWireRetries);
                }
            };

            // Первый запуск немедленно
            RetryLambda();
            
            // Если не удалось сразу - запускаем таймер для retry
            if (GetWorld()->GetTimerManager().IsTimerActive(EquipmentWireRetryHandle) == false)
            {
                GetWorld()->GetTimerManager().SetTimer(
                    EquipmentWireRetryHandle, 
                    FTimerDelegate::CreateLambda(RetryLambda),
                    EquipmentWireRetryInterval, 
                    true); // Повторяющийся таймер
                    
                UE_LOG(LogSuspensePlayerState, Log, 
                    TEXT("Retry timer started: checking every %.2fs (max %d attempts)"),
                    EquipmentWireRetryInterval, MaxEquipmentWireRetries);
            }
        }
        else
        {
            UE_LOG(LogSuspensePlayerState, Error, 
                TEXT("Failed to apply loadout '%s' to player %s"), 
                *LoadoutToApply.ToString(), *GetPlayerName());
            
            for (const FString& Error : Result.ErrorMessages)
            {
                UE_LOG(LogSuspensePlayerState, Error, TEXT("  - %s"), *Error);
            }
            
            if (USuspenseEventManager* EventManager = GetDelegateManager())
            {
                EventManager->BroadcastGenericEvent(this, 
                    FGameplayTag::RequestGameplayTag(TEXT("Player.Loadout.Failed")),
                    FString::Printf(TEXT("Failed to apply loadout: %s"), *LoadoutToApply.ToString()));
            }
        }
    }
}

void ASuspensePlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CleanupAttributeChangeCallbacks();
    CleanupComponentListeners();
    Super::EndPlay(EndPlayReason);
}

void ASuspensePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    DOREPLIFETIME(ASuspensePlayerState, ASC);
    DOREPLIFETIME(ASuspensePlayerState, InventoryComponent);
    DOREPLIFETIME(ASuspensePlayerState, CurrentLoadoutID);
    DOREPLIFETIME(ASuspensePlayerState, EquipmentDataStore);
    DOREPLIFETIME(ASuspensePlayerState, EquipmentTxnProcessor);
    DOREPLIFETIME(ASuspensePlayerState, EquipmentOps);
    DOREPLIFETIME(ASuspensePlayerState, EquipmentReplication);
    DOREPLIFETIME(ASuspensePlayerState, EquipmentPrediction);
    DOREPLIFETIME(ASuspensePlayerState, EquipmentNetworkDispatcher);
    DOREPLIFETIME(ASuspensePlayerState, EquipmentEventDispatcher);
    DOREPLIFETIME(ASuspensePlayerState, WeaponStateManager);
    DOREPLIFETIME(ASuspensePlayerState, EquipmentInventoryBridge);
    DOREPLIFETIME(ASuspensePlayerState, EquipmentSystemCoordinator);
}

bool ASuspensePlayerState::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
    bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
    
    if (ASC && IsValid(ASC))
    {
        bWroteSomething |= Channel->ReplicateSubobject(ASC, *Bunch, *RepFlags);
    }
    if (InventoryComponent && IsValid(InventoryComponent))
    {
        bWroteSomething |= Channel->ReplicateSubobject(InventoryComponent, *Bunch, *RepFlags);
    }
    
    auto RepComp = [&](UActorComponent* C){ if (C && IsValid(C)) { bWroteSomething |= Channel->ReplicateSubobject(C, *Bunch, *RepFlags); } };
    RepComp(EquipmentDataStore);
    RepComp(EquipmentTxnProcessor);
    RepComp(EquipmentOps);
    RepComp(EquipmentReplication);
    RepComp(EquipmentPrediction);
    RepComp(EquipmentNetworkDispatcher);
    RepComp(EquipmentEventDispatcher);
    RepComp(WeaponStateManager);
    RepComp(EquipmentInventoryBridge);
    RepComp(EquipmentSystemCoordinator);
    return bWroteSomething;
}

UAbilitySystemComponent* ASuspensePlayerState::GetAbilitySystemComponent() const
{
    return ASC;
}

UAbilitySystemComponent* ASuspensePlayerState::GetASC_Implementation() const
{
    return ASC;
}

void ASuspensePlayerState::SetHasWeapon_Implementation(bool bNewHasWeapon)
{
    bHasWeapon = bNewHasWeapon;
    if (USuspenseEventManager* Manager = GetDelegateManager())
    {
        AActor* WeaponActor = bHasWeapon ? CurrentWeaponActor : nullptr;
        ISuspenseCharacterInterface::BroadcastWeaponChanged(this, WeaponActor, bHasWeapon);
    }
}

void ASuspensePlayerState::SetCurrentWeaponActor_Implementation(AActor* WeaponActor)
{
    AActor* OldWeapon = CurrentWeaponActor;
    CurrentWeaponActor = WeaponActor;
    if (OldWeapon != CurrentWeaponActor)
    {
        if (USuspenseEventManager* Manager = GetDelegateManager())
        {
            ISuspenseCharacterInterface::BroadcastWeaponChanged(this, CurrentWeaponActor, bHasWeapon);
        }
    }
}

AActor* ASuspensePlayerState::GetCurrentWeaponActor_Implementation() const
{
    return CurrentWeaponActor;
}

bool ASuspensePlayerState::HasWeapon_Implementation() const
{
    return bHasWeapon && CurrentWeaponActor != nullptr;
}

float ASuspensePlayerState::GetCharacterLevel_Implementation() const
{
    if (Attributes)
    {
        return 1.0f; // TODO: replace with real level
    }
    return 1.0f;
}

bool ASuspensePlayerState::IsAlive_Implementation() const
{
    if (Attributes)
    {
        USuspenseBaseAttributeSet* BaseAttributes = Cast<USuspenseBaseAttributeSet>(Attributes);
        if (BaseAttributes)
        {
            return BaseAttributes->GetHealth() > 0.0f;
        }
    }
    return true;
}

int32 ASuspensePlayerState::GetTeamId_Implementation() const
{
    return 0; // TODO: implement proper team id
}

USuspenseEventManager* ASuspensePlayerState::GetDelegateManager() const
{
    return ISuspenseCharacter::GetDelegateManagerStatic(this);
}

//========================================
// ISuspenseLoadout Implementation
//========================================

FLoadoutApplicationResult ASuspensePlayerState::ApplyLoadoutConfiguration_Implementation(
    const FName& LoadoutID, 
    USuspenseLoadoutManager* LoadoutManager,
    bool bForceApply)
{
    if (!bForceApply && CurrentLoadoutID == LoadoutID)
    {
        return FLoadoutApplicationResult::CreateSuccess(LoadoutID, FGameplayTagContainer());
    }
    if (!LoadoutManager)
    {
        return FLoadoutApplicationResult::CreateFailure(LoadoutID, TEXT("LoadoutManager is null"));
    }
    return ApplyLoadoutToComponents(LoadoutID, LoadoutManager);
}

FName ASuspensePlayerState::GetCurrentLoadoutID_Implementation() const
{
    return CurrentLoadoutID;
}

bool ASuspensePlayerState::CanAcceptLoadout_Implementation(
    const FName& LoadoutID,
    const USuspenseLoadoutManager* LoadoutManager,
    FString& OutReason) const
{
    if (!LoadoutManager)
    {
        OutReason = TEXT("LoadoutManager is null");
        return false;
    }
    if (!LoadoutManager->IsLoadoutValid(LoadoutID))
    {
        OutReason = FString::Printf(TEXT("Loadout '%s' is not valid"), *LoadoutID.ToString());
        return false;
    }
    OutReason.Empty();
    return true;
}

FGameplayTag ASuspensePlayerState::GetLoadoutComponentType_Implementation() const
{
    return FGameplayTag::RequestGameplayTag(TEXT("Loadout.Component.PlayerState"));
}

void ASuspensePlayerState::ResetForLoadout_Implementation(bool bPreserveRuntimeData)
{
    ResetComponentsForLoadout(bPreserveRuntimeData);
}

FString ASuspensePlayerState::SerializeLoadoutState_Implementation() const
{
    return FString::Printf(TEXT("LoadoutID=%s"), *CurrentLoadoutID.ToString());
}

bool ASuspensePlayerState::RestoreLoadoutState_Implementation(const FString& SerializedState)
{
    FString LoadoutIDStr;
    if (SerializedState.Split(TEXT("LoadoutID="), nullptr, &LoadoutIDStr))
    {
        CurrentLoadoutID = FName(*LoadoutIDStr);
        return true;
    }
    return false;
}

void ASuspensePlayerState::OnLoadoutPreChange_Implementation(const FName& OldLoadoutID, const FName& NewLoadoutID)
{
    if (bLogLoadoutOperations)
    {
        UE_LOG(LogSuspensePlayerState, Log, TEXT("Preparing to change loadout from '%s' to '%s' for player %s"),
            *OldLoadoutID.ToString(), *NewLoadoutID.ToString(), *GetPlayerName());
    }
}

void ASuspensePlayerState::OnLoadoutPostChange_Implementation(const FName& PreviousLoadoutID, const FName& NewLoadoutID)
{
    if (bLogLoadoutOperations)
    {
        UE_LOG(LogSuspensePlayerState, Log, TEXT("Successfully changed loadout from '%s' to '%s' for player %s"),
            *PreviousLoadoutID.ToString(), *NewLoadoutID.ToString(), *GetPlayerName());
    }
    CurrentLoadoutID = NewLoadoutID;
    if (USuspenseLoadoutManager* LoadoutManager = GetLoadoutManager())
    {
        LoadoutManager->BroadcastLoadoutChange(NewLoadoutID, this, true);
    }
}

FGameplayTagContainer ASuspensePlayerState::GetRequiredLoadoutFeatures_Implementation() const
{
    FGameplayTagContainer RequiredFeatures;
    RequiredFeatures.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Loadout.Feature.Inventory")));
    RequiredFeatures.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Loadout.Feature.Equipment")));
    return RequiredFeatures;
}

bool ASuspensePlayerState::ValidateAgainstLoadout_Implementation(TArray<FString>& OutViolations) const
{
    OutViolations.Empty();
    // Inventory validation stub; equipment validation handled by rules/validator services
    return true;
}

//========================================
// Loadout System Public Methods
//========================================

bool ASuspensePlayerState::ApplyLoadout(const FName& LoadoutID, bool bForceReapply)
{
    if (!CheckAuthority(TEXT("ApplyLoadout")))
    {
        return false;
    }
    if (LoadoutID.IsNone())
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("Cannot apply loadout: LoadoutID is None"));
        return false;
    }
    USuspenseLoadoutManager* LoadoutManager = GetLoadoutManager();
    if (!LoadoutManager)
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("Cannot apply loadout: LoadoutManager not found"));
        return false;
    }
    FLoadoutApplicationResult Result = ApplyLoadoutConfiguration_Implementation(LoadoutID, LoadoutManager, bForceReapply);
    if (bLogLoadoutOperations)
    {
        if (Result.bSuccess)
        {
            UE_LOG(LogSuspensePlayerState, Log, TEXT("Successfully applied loadout '%s' to player %s"),
                *LoadoutID.ToString(), *GetPlayerName());
            for (const FGameplayTag& ComponentTag : Result.AppliedComponents)
            {
                UE_LOG(LogSuspensePlayerState, Log, TEXT("  - Applied to: %s"), *ComponentTag.ToString());
            }
        }
        else
        {
            UE_LOG(LogSuspensePlayerState, Error, TEXT("Failed to apply loadout '%s' to player %s: %s"),
                *LoadoutID.ToString(), *GetPlayerName(), *Result.GetSummary());
            for (const FString& Error : Result.ErrorMessages)
            {
                UE_LOG(LogSuspensePlayerState, Error, TEXT("  - Error: %s"), *Error);
            }
            for (const FString& Warning : Result.Warnings)
            {
                UE_LOG(LogSuspensePlayerState, Warning, TEXT("  - Warning: %s"), *Warning);
            }
        }
    }
    return Result.bSuccess;
}

bool ASuspensePlayerState::SwitchLoadout(const FName& NewLoadoutID, bool bPreserveRuntimeData)
{
    if (!CheckAuthority(TEXT("SwitchLoadout")))
    {
        return false;
    }
    if (NewLoadoutID == CurrentLoadoutID)
    {
        UE_LOG(LogSuspensePlayerState, Warning, TEXT("Already using loadout '%s'"), *NewLoadoutID.ToString());
        return true;
    }
    OnLoadoutPreChange_Implementation(CurrentLoadoutID, NewLoadoutID);
    if (!bPreserveRuntimeData)
    {
        ResetForLoadout_Implementation(false);
    }
    bool bSuccess = ApplyLoadout(NewLoadoutID, true);
    if (bSuccess)
    {
        OnLoadoutPostChange_Implementation(CurrentLoadoutID, NewLoadoutID);
    }
    return bSuccess;
}

void ASuspensePlayerState::LogLoadoutStatus()
{
    UE_LOG(LogSuspensePlayerState, Log, TEXT("=== Loadout Status for %s ==="), *GetPlayerName());
    UE_LOG(LogSuspensePlayerState, Log, TEXT("Current Loadout: %s"), 
        CurrentLoadoutID.IsNone() ? TEXT("None") : *CurrentLoadoutID.ToString());
    
    if (USuspenseLoadoutManager* LoadoutManager = GetLoadoutManager())
    {
        if (!CurrentLoadoutID.IsNone())
        {
            TArray<FName> InventoryNames = LoadoutManager->GetInventoryNames(CurrentLoadoutID);
            UE_LOG(LogSuspensePlayerState, Log, TEXT("Configured Inventories: %d"), InventoryNames.Num());
            for (const FName& InvName : InventoryNames)
            {
                FInventoryConfig InvConfig;
                if (LoadoutManager->GetInventoryConfigBP(CurrentLoadoutID, InvName, InvConfig))
                {
                    UE_LOG(LogSuspensePlayerState, Log, TEXT("  - %s: %dx%d grid, %.1f max weight"),
                        InvName.IsNone() ? TEXT("Main") : *InvName.ToString(),
                        InvConfig.Width, InvConfig.Height, InvConfig.MaxWeight);
                }
            }
            TArray<FEquipmentSlotConfig> EquipmentSlots = LoadoutManager->GetEquipmentSlots(CurrentLoadoutID);
            UE_LOG(LogSuspensePlayerState, Log, TEXT("Equipment Slots: %d"), EquipmentSlots.Num());
            for (const FEquipmentSlotConfig& Slot : EquipmentSlots)
            {
                UE_LOG(LogSuspensePlayerState, Log, TEXT("  - %s (%s)"),
                    *Slot.DisplayName.ToString(),
                    Slot.bIsRequired ? TEXT("Required") : TEXT("Optional"));
            }
        }
    }
    UE_LOG(LogSuspensePlayerState, Log, TEXT("================================="));
}

//========================================
// Protected Methods
//========================================

USuspenseLoadoutManager* ASuspensePlayerState::GetLoadoutManager() const
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        return GameInstance->GetSubsystem<USuspenseLoadoutManager>();
    }
    return nullptr;
}

FLoadoutApplicationResult ASuspensePlayerState::ApplyLoadoutToComponents(const FName& LoadoutID, USuspenseLoadoutManager* LoadoutManager)
{
    UE_LOG(LogSuspensePlayerState , Warning, TEXT("=== ApplyLoadoutToComponents START ==="));
    UE_LOG(LogSuspensePlayerState , Warning, TEXT("LoadoutID: %s"), *LoadoutID.ToString());
    
    if (!LoadoutManager)
    {
        UE_LOG(LogSuspensePlayerState , Error, TEXT("LoadoutManager is null"));
        return FLoadoutApplicationResult::CreateFailure(LoadoutID, TEXT("LoadoutManager is null"));
    }
    
    if (!LoadoutManager->IsLoadoutValid(LoadoutID))
    {
        UE_LOG(LogSuspensePlayerState , Error, TEXT("Loadout '%s' not found or invalid"), *LoadoutID.ToString());
        return FLoadoutApplicationResult::CreateFailure(LoadoutID, 
            FString::Printf(TEXT("Loadout '%s' not found or invalid"), *LoadoutID.ToString()));
    }
    
    FLoadoutApplicationResult Result;
    Result.AppliedLoadoutID = LoadoutID;
    Result.ApplicationTime = FDateTime::Now();
    Result.bSuccess = true;
    
    // Get full loadout configuration
    FLoadoutConfiguration LoadoutConfig;
    if (!LoadoutManager->GetLoadoutConfigBP(LoadoutID, LoadoutConfig))
    {
        UE_LOG(LogSuspensePlayerState , Error, TEXT("Failed to get loadout configuration for '%s'"), *LoadoutID.ToString());
        return FLoadoutApplicationResult::CreateFailure(LoadoutID, TEXT("Failed to retrieve loadout configuration"));
    }
    
    // Step 1: Apply to Inventory Component
    if (InventoryComponent)
    {
        UE_LOG(LogSuspensePlayerState , Warning, TEXT("Step 1: Applying loadout to inventory component..."));
        
        bool bInventorySuccess = LoadoutManager->ApplyLoadoutToInventory(InventoryComponent, LoadoutID);
        FGameplayTag InventoryTag = FGameplayTag::RequestGameplayTag(TEXT("Loadout.Component.Inventory"));
        
        Result.MergeComponentResult(InventoryTag, bInventorySuccess, 
            bInventorySuccess ? TEXT("") : TEXT("Failed to apply inventory configuration"));
        
        if (bInventorySuccess)
        {
            UE_LOG(LogSuspensePlayerState , Log, TEXT("Inventory configuration applied successfully"));
        }
        else
        {
            UE_LOG(LogSuspensePlayerState , Error, TEXT("Failed to apply inventory configuration"));
        }
    }
    else
    {
        UE_LOG(LogSuspensePlayerState , Warning, TEXT("No inventory component found"));
        Result.Warnings.Add(TEXT("No inventory component found"));
    }

    // Step 2: Apply to Equipment DataStore
    if (!EquipmentDataStore)
    {
        UE_LOG(LogSuspensePlayerState , Error, TEXT("EquipmentDataStore is NULL"));
        Result.Warnings.Add(TEXT("No equipment DataStore found"));
    }
    else
    {
        UE_LOG(LogSuspensePlayerState , Warning, TEXT("Step 2: Applying loadout to equipment DataStore..."));
        
        // Step 2a: Initialize equipment slots
        TArray<FEquipmentSlotConfig> SlotConfigs = LoadoutManager->GetEquipmentSlots(LoadoutID);
        
        if (SlotConfigs.Num() == 0)
        {
            UE_LOG(LogSuspensePlayerState , Warning, TEXT("No equipment slot configurations in loadout"));
            Result.Warnings.Add(TEXT("No equipment slot config in loadout"));
        }
        else
        {
            UE_LOG(LogSuspensePlayerState , Log, TEXT("Initializing %d equipment slots..."), SlotConfigs.Num());
            
            if (!EquipmentDataStore->InitializeSlots(SlotConfigs))
            {
                UE_LOG(LogSuspensePlayerState , Error, TEXT("Failed to initialize equipment slots in DataStore"));
                Result.MergeComponentResult(
                    FGameplayTag::RequestGameplayTag(TEXT("Loadout.Component.Equipment")),
                    false, TEXT("Failed to initialize equipment slots in DataStore"));
            }
            else
            {
                UE_LOG(LogSuspensePlayerState , Log, TEXT("Equipment slots initialized successfully"));
                Result.AppliedComponents.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Loadout.Component.Equipment")));
                
                // Step 2b: Apply starting equipment items
                if (LoadoutConfig.StartingEquipment.Num() > 0)
                {
                    UE_LOG(LogSuspensePlayerState , Warning, TEXT("Step 2b: Applying %d starting equipment items..."), 
                        LoadoutConfig.StartingEquipment.Num());
                    
                    int32 SuccessfulEquips = 0;
                    
                    for (const auto& EquipPair : LoadoutConfig.StartingEquipment)
                    {
                        const EEquipmentSlotType SlotType = EquipPair.Key;
                        const FName ItemID = EquipPair.Value;
                        
                        if (ItemID.IsNone())
                        {
                            continue;
                        }
                        
                        UE_LOG(LogSuspensePlayerState , Log, TEXT("  Equipping %s to slot %s..."), 
                            *ItemID.ToString(), 
                            *UEnum::GetValueAsString(SlotType));
                        
                        // Find slot index for this slot type
                        int32 TargetSlotIndex = INDEX_NONE;
                        for (int32 i = 0; i < SlotConfigs.Num(); i++)
                        {
                            if (SlotConfigs[i].SlotType == SlotType)
                            {
                                TargetSlotIndex = i;
                                break;
                            }
                        }
                        
                        if (TargetSlotIndex == INDEX_NONE)
                        {
                            UE_LOG(LogSuspensePlayerState , Warning, TEXT("    Slot type %s not found in slot configs"), 
                                *UEnum::GetValueAsString(SlotType));
                            continue;
                        }
                        
                        // Create item instance
                        FInventoryItemInstance ItemInstance;
                        ItemInstance.ItemID = ItemID;
                        ItemInstance.InstanceID = FGuid::NewGuid();
                        ItemInstance.Quantity = 1;
                        
                        // Apply to slot
                        bool bEquipSuccess = EquipmentDataStore->SetSlotItem(TargetSlotIndex, ItemInstance, true);
                        
                        if (bEquipSuccess)
                        {
                            SuccessfulEquips++;
                            UE_LOG(LogSuspensePlayerState , Log, TEXT("    Successfully equipped %s to slot %d"), 
                                *ItemID.ToString(), 
                                TargetSlotIndex);
                        }
                        else
                        {
                            UE_LOG(LogSuspensePlayerState , Warning, TEXT("    Failed to equip %s to slot %d"), 
                                *ItemID.ToString(), 
                                TargetSlotIndex);
                        }
                    }
                    
                    UE_LOG(LogSuspensePlayerState , Log, TEXT("Equipment application: %d items equipped successfully"), 
                        SuccessfulEquips);
                    
                    // Broadcast equipment initialized event
                    if (SuccessfulEquips > 0)
                    {
                        // Get EventDelegateManager through GameInstance subsystem
                        if (UGameInstance* GI = GetGameInstance())
                        {
                            if (USuspenseEventManager* EventManager = GI->GetSubsystem<USuspenseEventManager>())
                            {
                                FGameplayTag EquipInitTag = FGameplayTag::RequestGameplayTag(
                                    TEXT("Player.Equipment.Initialized"));
                                
                                FString EventData = FString::Printf(TEXT("LoadoutID:%s,ItemsEquipped:%d"), 
                                    *LoadoutID.ToString(), SuccessfulEquips);
                                
                                EventManager->BroadcastGenericEvent(this, EquipInitTag, EventData);
                                
                                UE_LOG(LogSuspensePlayerState , Log, TEXT("Broadcasted Equipment.Initialized event"));
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Step 3: Finalize
    if (Result.bSuccess)
    {
        CurrentLoadoutID = LoadoutID;
        SetupComponentListeners();
        
        UE_LOG(LogSuspensePlayerState , Log, TEXT("Loadout '%s' applied successfully"), *LoadoutID.ToString());
    }
    
    UE_LOG(LogSuspensePlayerState , Warning, TEXT("=== ApplyLoadoutToComponents END ==="));
    
    return Result;
}

void ASuspensePlayerState::ResetComponentsForLoadout(bool bPreserveRuntimeData)
{
    if (bLogLoadoutOperations)
    {
        UE_LOG(LogSuspensePlayerState, Log, TEXT("Resetting components for loadout change (preserve runtime: %s)"),
            bPreserveRuntimeData ? TEXT("Yes") : TEXT("No"));
    }
    // Inventory: component handles reset on re-apply
    // Equipment module: clear DataStore if not preserving runtime
    if (EquipmentDataStore && !bPreserveRuntimeData)
    {
        EquipmentDataStore->ResetToDefault();
    }
}

void ASuspensePlayerState::HandleComponentInitialized()
{
    UE_LOG(LogSuspensePlayerState, Log, TEXT("Component initialized for player %s"), *GetPlayerName());
}

void ASuspensePlayerState::HandleComponentUpdated()
{
    UE_LOG(LogSuspensePlayerState, Verbose, TEXT("Component updated for player %s"), *GetPlayerName());
}

bool ASuspensePlayerState::SetupComponentListeners()
{
    if (bComponentListenersSetup)
    {
        return true;
    }
    bComponentListenersSetup = true;
    UE_LOG(LogSuspensePlayerState, Log, TEXT("Set up component listeners for player %s"), *GetPlayerName());
    return true;
}

void ASuspensePlayerState::CleanupComponentListeners()
{
    if (!bComponentListenersSetup)
    {
        return;
    }
    bComponentListenersSetup = false;
    UE_LOG(LogSuspensePlayerState, Verbose, TEXT("Cleaned up component listeners for player %s"), *GetPlayerName());
}

// ========================================
// ISuspenseAttributeProvider Implementation
// ========================================

UAttributeSet* ASuspensePlayerState::GetAttributeSet_Implementation() const
{
    return Attributes;
}

TSubclassOf<UAttributeSet> ASuspensePlayerState::GetAttributeSetClass_Implementation() const
{
    return InitialAttributeSetClass;
}

TSubclassOf<UGameplayEffect> ASuspensePlayerState::GetBaseStatsEffect_Implementation() const
{
    return InitialAttributesEffect;
}

void ASuspensePlayerState::InitializeAttributes_Implementation(UAttributeSet* AttributeSet) const
{
    // Empty - we initialize via GE
}

void ASuspensePlayerState::ApplyEffects_Implementation(UAbilitySystemComponent* InASC) const
{
    if (!InASC || !InitialAttributesEffect)
    {
        return;
    }
    FGameplayEffectContextHandle EffectContext = InASC->MakeEffectContext();
    EffectContext.AddSourceObject(const_cast<ASuspensePlayerState*>(this));
    FGameplayEffectSpecHandle SpecHandle = InASC->MakeOutgoingSpec(InitialAttributesEffect, 1.0f, EffectContext);
    if (SpecHandle.IsValid())
    {
        InASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
    }
}

bool ASuspensePlayerState::HasAttributes_Implementation() const
{
    return Attributes != nullptr;
}

void ASuspensePlayerState::SetAttributeSetClass_Implementation(TSubclassOf<UAttributeSet> NewClass)
{
    InitialAttributeSetClass = NewClass;
}

FSuspenseAttributeData ASuspensePlayerState::GetAttributeData_Implementation(const FGameplayTag& AttributeTag) const
{
    if (AttributeTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Attribute.Health"))))
    {
        return GetHealthData_Implementation();
    }
    else if (AttributeTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Attribute.Stamina"))))
    {
        return GetStaminaData_Implementation();
    }
    else if (AttributeTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Attribute.Armor"))))
    {
        return GetArmorData_Implementation();
    }
    return FSuspenseAttributeData();
}

FSuspenseAttributeData ASuspensePlayerState::GetHealthData_Implementation() const
{
    if (!Attributes)
    {
        return FSuspenseAttributeData();
    }
    USuspenseBaseAttributeSet* BaseAttributes = Cast<USuspenseBaseAttributeSet>(Attributes);
    if (BaseAttributes)
    {
        float CurrentHealth = BaseAttributes->GetHealth();
        float MaxHealth = BaseAttributes->GetMaxHealth();
        return FSuspenseAttributeData::CreateAttributeData(CurrentHealth, MaxHealth,
            FGameplayTag::RequestGameplayTag(TEXT("Attribute.Health")), FText::FromString(TEXT("Health")));
    }
    return FSuspenseAttributeData();
}

FSuspenseAttributeData ASuspensePlayerState::GetStaminaData_Implementation() const
{
    if (!Attributes)
    {
        return FSuspenseAttributeData();
    }
    USuspenseBaseAttributeSet* BaseAttributes = Cast<USuspenseBaseAttributeSet>(Attributes);
    if (BaseAttributes)
    {
        float CurrentStamina = BaseAttributes->GetStamina();
        float MaxStamina = BaseAttributes->GetMaxStamina();
        return FSuspenseAttributeData::CreateAttributeData(CurrentStamina, MaxStamina,
            FGameplayTag::RequestGameplayTag(TEXT("Attribute.Stamina")), FText::FromString(TEXT("Stamina")));
    }
    return FSuspenseAttributeData();
}

FSuspenseAttributeData ASuspensePlayerState::GetArmorData_Implementation() const
{
    if (!Attributes)
    {
        return FSuspenseAttributeData();
    }
    USuspenseBaseAttributeSet* BaseAttributes = Cast<USuspenseBaseAttributeSet>(Attributes);
    if (BaseAttributes)
    {
        float Armor = BaseAttributes->GetArmor();
        return FSuspenseAttributeData::CreateAttributeData(Armor, Armor,
            FGameplayTag::RequestGameplayTag(TEXT("Attribute.Armor")), FText::FromString(TEXT("Armor")));
    }
    return FSuspenseAttributeData();
}

TArray<FSuspenseAttributeData> ASuspensePlayerState::GetAllAttributeData_Implementation() const
{
    TArray<FSuspenseAttributeData> AllData;
    AllData.Add(GetHealthData_Implementation());
    AllData.Add(GetStaminaData_Implementation());
    AllData.Add(GetArmorData_Implementation());
    AllData.RemoveAll([](const FSuspenseAttributeData& Data) { return !Data.bIsValid; });
    return AllData;
}

bool ASuspensePlayerState::GetAttributeValue_Implementation(const FGameplayTag& AttributeTag, float& OutCurrentValue, float& OutMaxValue) const
{
    FSuspenseAttributeData Data = GetAttributeData_Implementation(AttributeTag);
    if (Data.bIsValid)
    {
        OutCurrentValue = Data.CurrentValue;
        OutMaxValue = Data.MaxValue;
        return true;
    }
    OutCurrentValue = 0.0f;
    OutMaxValue = 0.0f;
    return false;
}

void ASuspensePlayerState::NotifyAttributeChanged_Implementation(const FGameplayTag& AttributeTag, float NewValue, float OldValue)
{
    if (AttributeTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Attribute.Health"))))
    {
        if (Attributes)
        {
            USuspenseBaseAttributeSet* BaseAttributes = Cast<USuspenseBaseAttributeSet>(Attributes);
            if (BaseAttributes)
            {
                float MaxHealth = BaseAttributes->GetMaxHealth();
                ISuspenseAttributeProvider::BroadcastHealthUpdate(this, NewValue, MaxHealth);
            }
        }
    }
    else if (AttributeTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Attribute.Stamina"))))
    {
        if (Attributes)
        {
            USuspenseBaseAttributeSet* BaseAttributes = Cast<USuspenseBaseAttributeSet>(Attributes);
            if (BaseAttributes)
            {
                float MaxStamina = BaseAttributes->GetMaxStamina();
                ISuspenseAttributeProvider::BroadcastStaminaUpdate(this, NewValue, MaxStamina);
            }
        }
    }
}

// ========================================
// Attribute Change Callbacks
// ========================================

void ASuspensePlayerState::SetupAttributeChangeCallbacks()
{
    if (!ASC || !Attributes)
    {
        return;
    }
    USuspenseBaseAttributeSet* BaseAttributes = Cast<USuspenseBaseAttributeSet>(Attributes);
    if (!BaseAttributes)
    {
        return;
    }
    HealthChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(
        BaseAttributes->GetHealthAttribute()).AddUObject(this, &ASuspensePlayerState::OnHealthChanged);
    MaxHealthChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(
        BaseAttributes->GetMaxHealthAttribute()).AddUObject(this, &ASuspensePlayerState::OnMaxHealthChanged);
    StaminaChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(
        BaseAttributes->GetStaminaAttribute()).AddUObject(this, &ASuspensePlayerState::OnStaminaChanged);
    MaxStaminaChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(
        BaseAttributes->GetMaxStaminaAttribute()).AddUObject(this, &ASuspensePlayerState::OnMaxStaminaChanged);
    MovementSpeedChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(
        BaseAttributes->GetMovementSpeedAttribute()).AddUObject(this, &ASuspensePlayerState::OnMovementSpeedChanged);
    SprintTagChangedDelegateHandle = ASC->RegisterGameplayTagEvent(
        SprintingTag, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ASuspensePlayerState::OnSprintTagChanged);
    UE_LOG(LogSuspensePlayerState, Log, TEXT("[PlayerState] Attribute change callbacks setup completed"));
}

void ASuspensePlayerState::CleanupAttributeChangeCallbacks()
{
    if (!ASC || !Attributes)
    {
        return;
    }
    USuspenseBaseAttributeSet* BaseAttributes = Cast<USuspenseBaseAttributeSet>(Attributes);
    if (!BaseAttributes)
    {
        return;
    }
    if (HealthChangedDelegateHandle.IsValid())
    {
        ASC->GetGameplayAttributeValueChangeDelegate(BaseAttributes->GetHealthAttribute()).Remove(HealthChangedDelegateHandle);
    }
    if (MaxHealthChangedDelegateHandle.IsValid())
    {
        ASC->GetGameplayAttributeValueChangeDelegate(BaseAttributes->GetMaxHealthAttribute()).Remove(MaxHealthChangedDelegateHandle);
    }
    if (StaminaChangedDelegateHandle.IsValid())
    {
        ASC->GetGameplayAttributeValueChangeDelegate(BaseAttributes->GetStaminaAttribute()).Remove(StaminaChangedDelegateHandle);
    }
    if (MaxStaminaChangedDelegateHandle.IsValid())
    {
        ASC->GetGameplayAttributeValueChangeDelegate(BaseAttributes->GetMaxStaminaAttribute()).Remove(MaxStaminaChangedDelegateHandle);
    }
    if (MovementSpeedChangedDelegateHandle.IsValid())
    {
        ASC->GetGameplayAttributeValueChangeDelegate(BaseAttributes->GetMovementSpeedAttribute()).Remove(MovementSpeedChangedDelegateHandle);
    }
    if (SprintTagChangedDelegateHandle.IsValid())
    {
        ASC->RegisterGameplayTagEvent(SprintingTag, EGameplayTagEventType::NewOrRemoved).Remove(SprintTagChangedDelegateHandle);
    }
}

void ASuspensePlayerState::OnHealthChanged(const FOnAttributeChangeData& Data)
{
    if (Attributes)
    {
        USuspenseBaseAttributeSet* BaseAttributes = Cast<USuspenseBaseAttributeSet>(Attributes);
        if (BaseAttributes)
        {
            float MaxHealth = BaseAttributes->GetMaxHealth();
            ISuspenseAttributeProvider::BroadcastHealthUpdate(this, Data.NewValue, MaxHealth);
            UE_LOG(LogSuspensePlayerState, Verbose, TEXT("[PlayerState] Health changed: %.1f/%.1f (was %.1f)"), 
                Data.NewValue, MaxHealth, Data.OldValue);
        }
    }
}

void ASuspensePlayerState::OnMaxHealthChanged(const FOnAttributeChangeData& Data)
{
    if (Attributes)
    {
        USuspenseBaseAttributeSet* BaseAttributes = Cast<USuspenseBaseAttributeSet>(Attributes);
        if (BaseAttributes)
        {
            float CurrentHealth = BaseAttributes->GetHealth();
            ISuspenseAttributeProvider::BroadcastHealthUpdate(this, CurrentHealth, Data.NewValue);
            UE_LOG(LogSuspensePlayerState, Verbose, TEXT("[PlayerState] Max health changed: %.1f (was %.1f)"), 
                Data.NewValue, Data.OldValue);
        }
    }
}

void ASuspensePlayerState::OnStaminaChanged(const FOnAttributeChangeData& Data)
{
    if (Attributes)
    {
        USuspenseBaseAttributeSet* BaseAttributes = Cast<USuspenseBaseAttributeSet>(Attributes);
        if (BaseAttributes)
        {
            float MaxStamina = BaseAttributes->GetMaxStamina();
            ISuspenseAttributeProvider::BroadcastStaminaUpdate(this, Data.NewValue, MaxStamina);
            UE_LOG(LogSuspensePlayerState, Verbose, TEXT("[PlayerState] Stamina changed: %.1f/%.1f (was %.1f)"), 
                Data.NewValue, MaxStamina, Data.OldValue);
        }
    }
}

void ASuspensePlayerState::OnMaxStaminaChanged(const FOnAttributeChangeData& Data)
{
    if (Attributes)
    {
        USuspenseBaseAttributeSet* BaseAttributes = Cast<USuspenseBaseAttributeSet>(Attributes);
        if (BaseAttributes)
        {
            float CurrentStamina = BaseAttributes->GetStamina();
            ISuspenseAttributeProvider::BroadcastStaminaUpdate(this, CurrentStamina, Data.NewValue);
            UE_LOG(LogSuspensePlayerState, Verbose, TEXT("[PlayerState] Max stamina changed: %.1f (was %.1f)"), 
                Data.NewValue, Data.OldValue);
        }
    }
}

void ASuspensePlayerState::OnMovementSpeedChanged(const FOnAttributeChangeData& Data)
{
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("[PlayerState] OnMovementSpeedChanged: OldValue=%.1f, NewValue=%.1f"), 
        Data.OldValue, Data.NewValue);
    APawn* Pawn = GetPawn();
    if (Pawn && Pawn->GetClass()->ImplementsInterface(UMedComMovementInterface::StaticClass()))
    {
        bool bIsSprinting = ASC && ASC->HasMatchingGameplayTag(SprintingTag);
        IMedComMovementInterface::NotifyMovementSpeedChanged(Pawn, Data.OldValue, Data.NewValue, bIsSprinting);
    }
    DebugActiveEffects();
}

void ASuspensePlayerState::OnSprintTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
    if (CallbackTag != SprintingTag)
    {
        return;
    }
    APawn* Pawn = GetPawn();
    if (!Pawn || !Pawn->GetClass()->ImplementsInterface(UMedComMovementInterface::StaticClass()))
    {
        return;
    }
    bool bIsSprinting = (NewCount > 0);
    if (bIsSprinting)
    {
        UE_LOG(LogSuspensePlayerState, Log, TEXT("[PlayerState] Sprint tag added, speed will be updated through attribute change"));
    }
    else
    {
        UE_LOG(LogSuspensePlayerState, Log, TEXT("[PlayerState] Sprint tag removed, speed will be updated through attribute change"));
    }
    FGameplayTag MovementState = bIsSprinting 
        ? FGameplayTag::RequestGameplayTag(TEXT("Movement.Sprinting"))
        : FGameplayTag::RequestGameplayTag(TEXT("Movement.Walking"));
    IMedComMovementInterface::NotifyMovementStateChanged(Pawn, MovementState, bIsSprinting);
    DebugActiveEffects();
}

bool ASuspensePlayerState::TryWireEquipmentModuleOnce()
{
    check(IsInGameThread());

    UE_LOG(LogSuspensePlayerState, Warning, TEXT("=== TryWireEquipmentModuleOnce: ATTEMPT START ==="));

    // =====================================================================
    // Step 1: Validate GameInstance
    // =====================================================================
    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        UE_LOG(LogSuspensePlayerState, Verbose, TEXT("TryWireEquipmentModuleOnce: GI not ready"));
        return false;
    }
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("✓ GameInstance available: %s"), *GI->GetClass()->GetName());

    // =====================================================================
    // Step 2: Get CoordinatorSubsystem
    // =====================================================================
    USuspenseSystemCoordinatorSubsystem* SysSub = GI->GetSubsystem<USuspenseSystemCoordinatorSubsystem>();
    if (!SysSub)
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("✗ CoordinatorSubsystem not found in GameInstance"));
        return false;
    }
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("✓ CoordinatorSubsystem found"));

    // =====================================================================
    // Step 3: Check if global services are ready
    // =====================================================================
    if (!SysSub->AreGlobalServicesReady())
    {
        UE_LOG(LogSuspensePlayerState, Verbose, TEXT("✗ Global services NOT ready yet (retry needed)"));
        return false;
    }
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("✓ Global services are READY"));

    // =====================================================================
    // Step 4: Get ServiceLocator
    // =====================================================================
    UEquipmentServiceLocator* Locator = SysSub->GetServiceLocator();
    if (!Locator)
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("✗ ServiceLocator is null"));
        return false;
    }
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("✓ ServiceLocator available"));

    // =====================================================================
    // Step 5: Validate local equipment components
    // =====================================================================
    if (!EquipmentDataStore)
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("✗ CRITICAL: EquipmentDataStore is NULL!"));
        return false;
    }
    if (!EquipmentTxnProcessor)
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("✗ CRITICAL: EquipmentTxnProcessor is NULL!"));
        return false;
    }
    if (!EquipmentOps)
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("✗ CRITICAL: EquipmentOps is NULL!"));
        return false;
    }
    if (!EquipmentInventoryBridge)
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("✗ CRITICAL: EquipmentInventoryBridge is NULL!"));
        return false;
    }
    if (!InventoryComponent)
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("✗ CRITICAL: InventoryComponent is NULL!"));
        return false;
    }
    
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("✓ All local components validated"));

    // =====================================================================
    // Step 6: Wire DataStore <-> TransactionProcessor delta callback
    // =====================================================================
    UE_LOG(LogSuspensePlayerState, Log, TEXT("Wiring DataStore ↔ TransactionProcessor..."));
    EquipmentTxnProcessor->SetDeltaCallback(
        FOnTransactionDelta::CreateUObject(EquipmentDataStore, &USuspenseEquipmentDataStore::OnTransactionDelta));
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("✓ DataStore delta callback configured"));

    // =====================================================================
    // Step 7: Inject per-player executor into global OperationsService
    // =====================================================================
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("Injecting OperationsExecutor into global service..."));
    {
        const FGameplayTag OpsTag = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Operations"));
        UObject* OpsObj = Locator->TryGetService(OpsTag);
        
        if (!OpsObj)
        {
            UE_LOG(LogSuspensePlayerState, Error, TEXT("✗ OperationsService not found in locator"));
            return false;
        }
        
        UEquipmentOperationServiceImpl* OpsService = Cast<UEquipmentOperationServiceImpl>(OpsObj);
        if (!OpsService)
        {
            UE_LOG(LogSuspensePlayerState, Error, TEXT("✗ Service object is not UEquipmentOperationServiceImpl"));
            return false;
        }

        TScriptInterface<IMedComEquipmentOperations> ExecIf;
        ExecIf.SetObject(EquipmentOps);
        ExecIf.SetInterface(Cast<IMedComEquipmentOperations>(EquipmentOps));
        
        OpsService->SetOperationsExecutor(ExecIf);
        UE_LOG(LogSuspensePlayerState, Warning, TEXT("✓ Executor injected into OperationsService"));
    }

    // =====================================================================
    // Step 8: Prepare shared DataProvider interface
    // This interface will be used by multiple components
    // =====================================================================
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("=== Preparing shared DataProvider interface ==="));
    
    TScriptInterface<IMedComEquipmentDataProvider> DataProviderInterface;
    DataProviderInterface.SetObject(EquipmentDataStore);
    DataProviderInterface.SetInterface(Cast<IMedComEquipmentDataProvider>(EquipmentDataStore));
    
    if (!DataProviderInterface.GetInterface())
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("✗ DataStore does not implement IMedComEquipmentDataProvider!"));
        return false;
    }
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("✓ DataProvider interface prepared for reuse"));

    // =====================================================================
    // Step 9: CRITICAL FIX - Initialize TransactionProcessor with DataProvider
    // This MUST happen BEFORE EquipmentOps and Bridge initialization
    // Without this, BeginTransaction() will fail with "Processor not initialized"
    // =====================================================================
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("=== Initializing TransactionProcessor START ==="));
    
    const bool bTxnProcessorInit = EquipmentTxnProcessor->Initialize(DataProviderInterface);
    
    if (!bTxnProcessorInit)
    {
        UE_LOG(LogSuspensePlayerState, Error, 
            TEXT("✗ CRITICAL: EquipmentTxnProcessor->Initialize() FAILED!"));
        UE_LOG(LogSuspensePlayerState, Error, 
            TEXT("   Transactional operations will NOT work!"));
        UE_LOG(LogSuspensePlayerState, Error,
            TEXT("   BeginTransaction() will return invalid GUID"));
        return false;
    }
    
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("  ✓ TransactionProcessor initialized with DataProvider"));
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("  ✓ Transaction processor ready to accept transactions"));
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("=== Initializing TransactionProcessor END ==="));

    // =====================================================================
    // Step 10: Initialize EquipmentOps with DataProvider and SlotValidator
    // This enables item validation and slot compatibility checks
    // =====================================================================
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("=== Initializing EquipmentOps START ==="));
    
    // Prepare SlotValidator interface (optional but recommended)
    TScriptInterface<IMedComSlotValidator> ValidatorInterface;
    if (EquipmentSlotValidator)
    {
        ValidatorInterface.SetObject(EquipmentSlotValidator);
        ValidatorInterface.SetInterface(Cast<IMedComSlotValidator>(EquipmentSlotValidator));
        UE_LOG(LogSuspensePlayerState, Log, TEXT("  ✓ SlotValidator interface prepared"));
    }
    else
    {
        UE_LOG(LogSuspensePlayerState, Warning, TEXT("  ⚠ SlotValidator not available - validation will be limited"));
    }

    // Initialize EquipmentOps with both DataProvider and Validator
    const bool bOpsInitialized = EquipmentOps->Initialize(DataProviderInterface, ValidatorInterface);
    
    if (!bOpsInitialized)
    {
        UE_LOG(LogSuspensePlayerState, Error, 
            TEXT("✗ CRITICAL: EquipmentOps->Initialize() FAILED!"));
        UE_LOG(LogSuspensePlayerState, Error, 
            TEXT("   Item validation will NOT work!"));
        UE_LOG(LogSuspensePlayerState, Error,
            TEXT("   Equipment operations will fail validation checks"));
        return false;
    }
    
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("  ✓ EquipmentOps initialized with DataProvider"));
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("  ✓ Item validation system ready"));
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("=== Initializing EquipmentOps END ==="));

    // =====================================================================
    // Step 11: Initialize EquipmentInventoryBridge
    // NOW Bridge can use both EquipmentOps (initialized with DataProvider)
    // AND TransactionProcessor (initialized and ready for transactions)
    // =====================================================================
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("=== Initializing EquipmentInventoryBridge START ==="));
    
    // Prepare Operations interface
    TScriptInterface<IMedComEquipmentOperations> OperationsInterface;
    OperationsInterface.SetObject(EquipmentOps);
    OperationsInterface.SetInterface(Cast<IMedComEquipmentOperations>(EquipmentOps));
    
    if (!OperationsInterface.GetInterface())
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("✗ EquipmentOps does not implement IMedComEquipmentOperations!"));
        return false;
    }
    UE_LOG(LogSuspensePlayerState, Log, TEXT("  ✓ Operations interface prepared"));

    // Prepare TransactionManager interface
    TScriptInterface<IMedComTransactionManager> TransactionInterface;
    TransactionInterface.SetObject(EquipmentTxnProcessor);
    TransactionInterface.SetInterface(Cast<IMedComTransactionManager>(EquipmentTxnProcessor));
    
    if (!TransactionInterface.GetInterface())
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("✗ TxnProcessor does not implement IMedComTransactionManager!"));
        return false;
    }
    UE_LOG(LogSuspensePlayerState, Log, TEXT("  ✓ TransactionManager interface prepared"));

    // Initialize Bridge with all three dependencies:
    // 1. DataProvider (for reading equipment state)
    // 2. Operations (for executing equipment operations)
    // 3. TransactionManager (for atomic operations)
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("  Calling EquipmentInventoryBridge->Initialize()..."));
    const bool bBridgeInit = EquipmentInventoryBridge->Initialize(
        DataProviderInterface,
        OperationsInterface,
        TransactionInterface
    );

    if (!bBridgeInit)
    {
        UE_LOG(LogSuspensePlayerState, Error, 
            TEXT("✗ CRITICAL: EquipmentInventoryBridge->Initialize() FAILED!"));
        UE_LOG(LogSuspensePlayerState, Error, 
            TEXT("   Equipment-Inventory integration will NOT work!"));
        UE_LOG(LogSuspensePlayerState, Error,
            TEXT("   Players will not be able to equip items from inventory"));
        return false;
    }
    
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("  ✓ Bridge initialized successfully"));
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("  ✓ Bridge has access to all required systems"));

    // Connect Inventory interface to Bridge
    UE_LOG(LogSuspensePlayerState, Log, TEXT("  Connecting Inventory to Bridge..."));
    
    TScriptInterface<IMedComInventoryInterface> InventoryInterface;
    InventoryInterface.SetObject(InventoryComponent);
    InventoryInterface.SetInterface(Cast<IMedComInventoryInterface>(InventoryComponent));
    
    if (!InventoryInterface.GetInterface())
    {
        UE_LOG(LogSuspensePlayerState, Error, 
            TEXT("✗ InventoryComponent does not implement IMedComInventoryInterface!"));
        return false;
    }
    
    EquipmentInventoryBridge->SetInventoryInterface(InventoryInterface);
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("  ✓ Inventory connected to Bridge"));
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("  ✓ Bridge can now transfer items between systems"));

    UE_LOG(LogSuspensePlayerState, Warning, TEXT("=== Initializing EquipmentInventoryBridge END ==="));

    // =====================================================================
    // Step 12: Success - All systems operational
    // =====================================================================
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("=== Equipment Module Wiring COMPLETE ✓✓✓ ==="));
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("All systems operational for player: %s"), *GetPlayerName());
    UE_LOG(LogSuspensePlayerState, Warning, TEXT(""));
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("Component Status Summary:"));
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("  • DataStore:            Initialized with %d slots"), 
        EquipmentDataStore->GetSlotCount());
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("  • TransactionProcessor: Initialized and ready"));
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("  • EquipmentOps:         Initialized with validation"));
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("  • Bridge:               Connected to Inventory"));
    UE_LOG(LogSuspensePlayerState, Warning, TEXT(""));
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("Players can now:"));
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("  • Equip items from inventory to equipment slots"));
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("  • Unequip items back to inventory"));
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("  • Swap items between equipment slots"));
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("  • All operations are atomic and transactional"));
    
    return true;
}

void ASuspensePlayerState::InitAttributes()
{
    if (!CheckAuthority(TEXT("InitAttributes")))
    {
        return;
    }
    if (!ASC)
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("Failed to initialize attributes: ASC is null"));
        return;
    }
    if (Attributes)
    {
        UE_LOG(LogSuspensePlayerState, Warning, TEXT("Attributes already initialized, skipping"));
        return;
    }
    UClass* AttributeSetClassToUse = InitialAttributeSetClass.Get();
    if (!AttributeSetClassToUse)
    {
        AttributeSetClassToUse = UMedComDefaultAttributeSet::StaticClass();
        UE_LOG(LogSuspensePlayerState, Warning, TEXT("Using default UMedComDefaultAttributeSet"));
    }
    Attributes = NewObject<USuspenseBaseAttributeSet>(this, AttributeSetClassToUse);
    if (!Attributes)
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("Failed to create AttributeSet"));
        return;
    }
    ASC->AddSpawnedAttribute(Attributes);
    UE_LOG(LogSuspensePlayerState, Log, TEXT("AttributeSet created and added to ASC"));

    if (!InitialAttributesEffect)
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("InitialAttributesEffect not configured! Attributes will remain at 0!"));
        return;
    }
    UE_LOG(LogSuspensePlayerState, Log, TEXT("Applying InitialAttributesEffect: %s"), *InitialAttributesEffect->GetName());
    FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
    EffectContext.AddSourceObject(this);
    FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(InitialAttributesEffect, 1.0f, EffectContext);
    if (!SpecHandle.IsValid())
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("Failed to create effect spec for InitialAttributesEffect"));
        return;
    }
    FActiveGameplayEffectHandle EffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
    if (!EffectHandle.IsValid())
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("Failed to apply InitialAttributesEffect"));
        return;
    }
    if (USuspenseBaseAttributeSet* BaseAttributes = Cast<USuspenseBaseAttributeSet>(Attributes))
    {
        UE_LOG(LogSuspensePlayerState, Warning, TEXT("=== Final Attribute Values After Initialization ==="));
        UE_LOG(LogSuspensePlayerState, Warning, TEXT("MovementSpeed: %.1f"), 
            ASC->GetNumericAttribute(BaseAttributes->GetMovementSpeedAttribute()));
        UE_LOG(LogSuspensePlayerState, Warning, TEXT("Health: %.1f / %.1f"), 
            ASC->GetNumericAttribute(BaseAttributes->GetHealthAttribute()),
            ASC->GetNumericAttribute(BaseAttributes->GetMaxHealthAttribute()));
        UE_LOG(LogSuspensePlayerState, Warning, TEXT("Stamina: %.1f / %.1f"), 
            ASC->GetNumericAttribute(BaseAttributes->GetStaminaAttribute()),
            ASC->GetNumericAttribute(BaseAttributes->GetMaxStaminaAttribute()));
        UE_LOG(LogSuspensePlayerState, Warning, TEXT("Armor: %.1f"), 
            ASC->GetNumericAttribute(BaseAttributes->GetArmorAttribute()));
        UE_LOG(LogSuspensePlayerState, Warning, TEXT("AttackPower: %.1f"), 
            ASC->GetNumericAttribute(BaseAttributes->GetAttackPowerAttribute()));
        UE_LOG(LogSuspensePlayerState, Warning, TEXT("================================================"));

        float InitialSpeed = ASC->GetNumericAttribute(BaseAttributes->GetMovementSpeedAttribute());
        if (APawn* Pawn = GetPawn())
        {
            if (ACharacter* Character = Cast<ACharacter>(Pawn))
            {
                if (UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement())
                {
                    MovementComp->MaxWalkSpeed = InitialSpeed;
                    UE_LOG(LogSuspensePlayerState, Log, TEXT("Applied initial movement speed %.1f to character"), InitialSpeed);
                }
            }
        }
    }
}

void ASuspensePlayerState::GrantStartupAbilities()
{
    if (!CheckAuthority(TEXT("GrantStartupAbilities")))
    {
        return;
    }
    if (!ASC)
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("Failed to grant abilities: ASC is null"));
        return;
    }
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("=== GrantStartupAbilities: Starting ability registration ==="));

    for (const FAbilityInfo& AbilityInfo : AbilityPool)
    {
        if (AbilityInfo.Ability)
        {
            FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(
                FGameplayAbilitySpec(AbilityInfo.Ability, 1, AbilityInfo.InputID));
            UE_LOG(LogSuspensePlayerState, Warning, TEXT("  - Added ability from pool: %s, InputID=%d, Handle Valid=%s"),
                   *AbilityInfo.Ability->GetName(), 
                   AbilityInfo.InputID,
                   Handle.IsValid() ? TEXT("YES") : TEXT("NO"));
        }
    }

    if (InteractAbility) 
    {
        int32 InteractInputID = static_cast<int32>(EMCAbilityInputID::Interact);
        FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(InteractAbility, 1, InteractInputID));
        UE_LOG(LogSuspensePlayerState, Warning, TEXT("  - Added Interact: %s, InputID=%d, Handle Valid=%s"),
               *InteractAbility->GetName(), InteractInputID, Handle.IsValid() ? TEXT("YES") : TEXT("NO"));
    }
    else
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("  - InteractAbility NOT SET in Blueprint!"));
    }
    
    if (SprintAbility) 
    {
        int32 SprintInputID = static_cast<int32>(EMCAbilityInputID::Sprint);
        FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(SprintAbility, 1, SprintInputID));
        UE_LOG(LogSuspensePlayerState, Warning, TEXT("  - Added Sprint: %s, InputID=%d, Handle Valid=%s"),
               *SprintAbility->GetName(), SprintInputID, Handle.IsValid() ? TEXT("YES") : TEXT("NO"));
    }
    else
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("  - SprintAbility NOT SET in Blueprint!"));
    }
    
    if (CrouchAbility) 
    {
        int32 CrouchInputID = static_cast<int32>(EMCAbilityInputID::Crouch);
        FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(CrouchAbility, 1, CrouchInputID));
        UE_LOG(LogSuspensePlayerState, Warning, TEXT("  - Added Crouch: %s, InputID=%d, Handle Valid=%s"),
               *CrouchAbility->GetName(), CrouchInputID, Handle.IsValid() ? TEXT("YES") : TEXT("NO"));
        if (Handle.IsValid())
        {
            if (FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle))
            {
                UE_LOG(LogSuspensePlayerState, Warning, TEXT("    Crouch Spec confirmed: InputID=%d, Level=%d"),
                    Spec->InputID, Spec->Level);
            }
        }
    }
    else
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("  - CrouchAbility NOT SET in Blueprint!"));
        UE_LOG(LogSuspensePlayerState, Error, TEXT("    CRITICAL: Check BP_SuspensePlayerState and set CrouchAbility!"));
    }
    
    if (JumpAbility) 
    {
        int32 JumpInputID = static_cast<int32>(EMCAbilityInputID::Jump);
        FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(JumpAbility, 1, JumpInputID));
        UE_LOG(LogSuspensePlayerState, Warning, TEXT("  - Added Jump: %s, InputID=%d, Handle Valid=%s"),
               *JumpAbility->GetName(), JumpInputID, Handle.IsValid() ? TEXT("YES") : TEXT("NO"));
    }
    else
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("  - JumpAbility NOT SET in Blueprint!"));
    }

    if (WeaponSwitchAbility) 
    {
        ASC->GiveAbility(FGameplayAbilitySpec(WeaponSwitchAbility, 1, static_cast<int32>(EMCAbilityInputID::NextWeapon)));
        ASC->GiveAbility(FGameplayAbilitySpec(WeaponSwitchAbility, 1, static_cast<int32>(EMCAbilityInputID::PrevWeapon)));
        ASC->GiveAbility(FGameplayAbilitySpec(WeaponSwitchAbility, 1, static_cast<int32>(EMCAbilityInputID::QuickSwitch)));
        ASC->GiveAbility(FGameplayAbilitySpec(WeaponSwitchAbility, 1, static_cast<int32>(EMCAbilityInputID::WeaponSlot1)));
        ASC->GiveAbility(FGameplayAbilitySpec(WeaponSwitchAbility, 1, static_cast<int32>(EMCAbilityInputID::WeaponSlot2)));
        ASC->GiveAbility(FGameplayAbilitySpec(WeaponSwitchAbility, 1, static_cast<int32>(EMCAbilityInputID::WeaponSlot3)));
        ASC->GiveAbility(FGameplayAbilitySpec(WeaponSwitchAbility, 1, static_cast<int32>(EMCAbilityInputID::WeaponSlot4)));
        ASC->GiveAbility(FGameplayAbilitySpec(WeaponSwitchAbility, 1, static_cast<int32>(EMCAbilityInputID::WeaponSlot5)));
        UE_LOG(LogSuspensePlayerState, Warning, TEXT("Added WeaponSwitchAbility with 8 different InputIDs for all weapon switch methods"));
    }
    else
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("  - WeaponSwitchAbility NOT SET in Blueprint!"));
    }

    UE_LOG(LogSuspensePlayerState, Warning, TEXT("=== Total abilities granted: %d ==="), ASC->GetActivatableAbilities().Num());
    for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
    {
        if (Spec.Ability)
        {
            UE_LOG(LogSuspensePlayerState, Warning, TEXT("  Ability: %s, InputID: %d, Level: %d"),
                *Spec.Ability->GetName(), Spec.InputID, Spec.Level);
        }
    }
}

void ASuspensePlayerState::ApplyPassiveStartupEffects()
{
    if (!CheckAuthority(TEXT("ApplyPassiveStartupEffects")))
    {
        return;
    }
    if (!ASC)
    {
        UE_LOG(LogSuspensePlayerState, Error, TEXT("Failed to apply passive effects: ASC is null"));
        return;
    }
    FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
    EffectContext.AddSourceObject(this);

    if (PassiveHealthRegenEffect)
    {
        FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(PassiveHealthRegenEffect, 1.0f, EffectContext);
        if (SpecHandle.IsValid())
        {
            ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }
    }
    if (PassiveStaminaRegenEffect)
    {
        FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(PassiveStaminaRegenEffect, 1.0f, EffectContext);
        if (SpecHandle.IsValid())
        {
            ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }
    }
}

bool ASuspensePlayerState::CheckAuthority(const TCHAR* Fn) const
{
    if (!HasAuthority())
    {
        UE_LOG(LogSuspensePlayerState, Warning, TEXT("Attempted to call %s on client"), Fn);
        return false;
    }
    return true;
}

void ASuspensePlayerState::DebugActiveEffects()
{
    if (!ASC)
    {
        return;
    }
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("=== Active GameplayEffects Debug ==="));
    TArray<FActiveGameplayEffectHandle> ActiveHandles = ASC->GetActiveEffects(FGameplayEffectQuery());
    for (const FActiveGameplayEffectHandle& Handle : ActiveHandles)
    {
        const FActiveGameplayEffect* ActiveEffect = ASC->GetActiveGameplayEffect(Handle);
        if (ActiveEffect && ActiveEffect->Spec.Def)
        {
            UE_LOG(LogSuspensePlayerState, Warning, TEXT("Active Effect: %s"), *ActiveEffect->Spec.Def->GetName());
            FGameplayTagContainer GrantedTags;
            ActiveEffect->Spec.GetAllGrantedTags(GrantedTags);
            UE_LOG(LogSuspensePlayerState, Warning, TEXT("  Granted Tags: %s"), *GrantedTags.ToString());
            for (int32 i = 0; i < ActiveEffect->Spec.Modifiers.Num(); i++)
            {
                float Magnitude = ActiveEffect->Spec.GetModifierMagnitude(i, true);
                UE_LOG(LogSuspensePlayerState, Warning, TEXT("  Modifier[%d] Magnitude: %.2f"), i, Magnitude);
            }
        }
    }
    FGameplayTagContainer OwnedTags;
    ASC->GetOwnedGameplayTags(OwnedTags);
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("Current Owned Tags: %s"), *OwnedTags.ToString());
    if (Attributes)
    {
        USuspenseBaseAttributeSet* BaseAttributes = Cast<USuspenseBaseAttributeSet>(Attributes);
        if (BaseAttributes)
        {
            UE_LOG(LogSuspensePlayerState, Warning, TEXT("MovementSpeed Base: %.1f, Current: %.1f"), 
                BaseAttributes->GetMovementSpeed(), 
                ASC->GetNumericAttribute(BaseAttributes->GetMovementSpeedAttribute()));
        }
    }
}

bool ASuspensePlayerState::WireEquipmentModule(USuspenseLoadoutManager* LoadoutManager, const FName& AppliedLoadoutID)
{
    UE_LOG(LogSuspensePlayerState, Warning, TEXT("=== WireEquipmentModule START ==="));

    if (!CheckAuthority(TEXT("WireEquipmentModule")))
    {
        UE_LOG(LogSuspensePlayerState, Verbose, TEXT("WireEquipmentModule: non-authority, skip wiring on this instance"));
        return true; // клиенты ожидают репликацию
    }

    // Пытаемся один раз
    if (TryWireEquipmentModuleOnce())
    {
        UE_LOG(LogSuspensePlayerState, Warning, TEXT("=== WireEquipmentModule COMPLETE (immediate) ==="));
        return true;
    }

    // Планируем повторные попытки: 20 × 50мс (1 сек) — без Fatal
    EquipmentWireRetryCount = 0;
    GetWorldTimerManager().SetTimer(
        EquipmentWireRetryHandle,
        [this]()
        {
            ++EquipmentWireRetryCount;

            if (TryWireEquipmentModuleOnce())
            {
                GetWorldTimerManager().ClearTimer(EquipmentWireRetryHandle);
                UE_LOG(LogSuspensePlayerState, Warning, TEXT("WireEquipmentModule: succeeded on retry %d"), EquipmentWireRetryCount);
                return;
            }

            if (EquipmentWireRetryCount >= MaxEquipmentWireRetries)
            {
                GetWorldTimerManager().ClearTimer(EquipmentWireRetryHandle);
                UE_LOG(LogSuspensePlayerState, Error,
                    TEXT("Equipment wiring failed after %d retries. Equipment will NOT be available yet."),
                    EquipmentWireRetryCount);
            }
        },
        EquipmentWireRetryInterval,
        true);

    UE_LOG(LogSuspensePlayerState, Warning, TEXT("=== WireEquipmentModule DEFERRED (waiting for global services) ==="));
    return false;
}
