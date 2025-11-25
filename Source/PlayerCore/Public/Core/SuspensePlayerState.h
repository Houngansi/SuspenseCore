// SuspensePlayerState.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "Interfaces/Core/IMedComCharacterInterface.h"
#include "Interfaces/Core/IMedComAttributeProviderInterface.h"
#include "Interfaces/Core/IMedComLoadoutInterface.h"
#include "Input/MCAbilityInputID.h"
#include "GameplayEffectTypes.h"
#include "Components/Validation/MedComEquipmentSlotValidator.h"
#include "SuspensePlayerState.generated.h"

class UMedComAbilitySystemComponent;
class UMedComBaseAttributeSet;
class UGameplayAbility;
class UGameplayEffect;
class UMedComInventoryComponent;
class UEventDelegateManager;
class UMedComLoadoutManager;

// Equipment module forward declarations
class UMedComEquipmentDataStore;
class UMedComEquipmentTransactionProcessor;
class UMedComEquipmentReplicationManager;
class UMedComEquipmentPredictionSystem;
class UMedComWeaponStateManager;
class UMedComEquipmentNetworkDispatcher;
class UMedComEquipmentInventoryBridge;
class UMedComEquipmentEventDispatcher;
class UMedComEquipmentOperationExecutor;
class UMedComSystemCoordinator;

USTRUCT(BlueprintType)
struct FAbilityInfo
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly)
    TSubclassOf<UGameplayAbility> Ability = nullptr;

    UPROPERTY(EditDefaultsOnly)
    int32 InputID = static_cast<int32>(EMCAbilityInputID::None);
};

/**
 * Player state with integrated GAS, inventory and equipment systems.
 * 
 * ARCHITECTURE:
 * - Global services managed by UMedComSystemCoordinatorSubsystem (GameInstance-level)
 * - Per-player components owned by this PlayerState (DataStore, TransactionProcessor, etc.)
 * - Components communicate with global services through ServiceLocator
 * 
 * LIFECYCLE:
 * 1. BeginPlay(): Apply loadout, initialize attributes, grant abilities
 * 2. WireEquipmentModule(): Connect per-player components with global services
 * 3. EndPlay(): Cleanup listeners and callbacks
 * 
 * MULTIPLAYER:
 * - Server authority: all components created on server
 * - Client: components replicated, read-only access
 * - Global services: single instance per GameInstance, accessed via ServiceLocator
 */
UCLASS()
class SUSPENSECORE_API ASuspensePlayerState
    : public APlayerState
    , public IAbilitySystemInterface
    , public IMedComCharacterInterface
    , public IMedComAttributeProviderInterface
    , public IMedComLoadoutInterface
{
    GENERATED_BODY()

public:
    ASuspensePlayerState();

    //========================================
    // Actor Lifecycle
    //========================================
    
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

    //========================================
    // IAbilitySystemInterface
    //========================================
    
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    //========================================
    // IMedComCharacterInterface
    //========================================
    
    virtual UAbilitySystemComponent* GetASC_Implementation() const override;
    virtual void SetHasWeapon_Implementation(bool bHasWeapon) override;
    virtual void SetCurrentWeaponActor_Implementation(AActor* WeaponActor) override;
    virtual AActor* GetCurrentWeaponActor_Implementation() const override;
    virtual bool HasWeapon_Implementation() const override;
    virtual float GetCharacterLevel_Implementation() const override;
    virtual bool IsAlive_Implementation() const override;
    virtual int32 GetTeamId_Implementation() const override;
    virtual UEventDelegateManager* GetDelegateManager() const override;
    
    //========================================
    // IMedComAttributeProviderInterface
    //========================================
    
    virtual UAttributeSet* GetAttributeSet_Implementation() const override;
    virtual TSubclassOf<UAttributeSet> GetAttributeSetClass_Implementation() const override;
    virtual TSubclassOf<UGameplayEffect> GetBaseStatsEffect_Implementation() const override;
    virtual void InitializeAttributes_Implementation(UAttributeSet* AttributeSet) const override;
    virtual void ApplyEffects_Implementation(UAbilitySystemComponent* ASC) const override;
    virtual bool HasAttributes_Implementation() const override;
    virtual void SetAttributeSetClass_Implementation(TSubclassOf<UAttributeSet> NewClass) override;
    virtual FMedComAttributeData GetAttributeData_Implementation(const FGameplayTag& AttributeTag) const override;
    virtual FMedComAttributeData GetHealthData_Implementation() const override;
    virtual FMedComAttributeData GetStaminaData_Implementation() const override;
    virtual FMedComAttributeData GetArmorData_Implementation() const override;
    virtual TArray<FMedComAttributeData> GetAllAttributeData_Implementation() const override;
    virtual bool GetAttributeValue_Implementation(const FGameplayTag& AttributeTag, float& OutCurrentValue, float& OutMaxValue) const override;
    virtual void NotifyAttributeChanged_Implementation(const FGameplayTag& AttributeTag, float NewValue, float OldValue) override;
    
    //========================================
    // IMedComLoadoutInterface
    //========================================
    
    virtual FLoadoutApplicationResult ApplyLoadoutConfiguration_Implementation(
        const FName& LoadoutID, 
        UMedComLoadoutManager* LoadoutManager,
        bool bForceApply = false) override;
    virtual FName GetCurrentLoadoutID_Implementation() const override;
    virtual bool CanAcceptLoadout_Implementation(
        const FName& LoadoutID,
        const UMedComLoadoutManager* LoadoutManager,
        FString& OutReason) const override;
    virtual FGameplayTag GetLoadoutComponentType_Implementation() const override;
    virtual void ResetForLoadout_Implementation(bool bPreserveRuntimeData = false) override;
    virtual FString SerializeLoadoutState_Implementation() const override;
    virtual bool RestoreLoadoutState_Implementation(const FString& SerializedState) override;
    virtual void OnLoadoutPreChange_Implementation(const FName& CurrentLoadoutID, const FName& NewLoadoutID) override;
    virtual void OnLoadoutPostChange_Implementation(const FName& PreviousLoadoutID, const FName& NewLoadoutID) override;
    virtual FGameplayTagContainer GetRequiredLoadoutFeatures_Implementation() const override;
    virtual bool ValidateAgainstLoadout_Implementation(TArray<FString>& OutViolations) const override;
    
    //========================================
    // Public API
    //========================================
    
    UFUNCTION(BlueprintCallable, Category = "MedCom|Attributes")
    const UMedComBaseAttributeSet* GetBaseAttributeSet() const { return Attributes; }
    
    UFUNCTION(BlueprintCallable, Category = "MedCom|Loadout", meta = (CallInEditor = "true"))
    bool ApplyLoadout(const FName& LoadoutID, bool bForceReapply = false);
    
    UFUNCTION(BlueprintCallable, Category = "MedCom|Loadout")
    bool SwitchLoadout(const FName& NewLoadoutID, bool bPreserveRuntimeData = false);
    
    UFUNCTION(BlueprintCallable, Category = "MedCom|Loadout")
    FName GetCurrentLoadout() const { return CurrentLoadoutID; }
    
    UFUNCTION(BlueprintCallable, Category = "MedCom|Loadout")
    bool HasLoadout() const { return !CurrentLoadoutID.IsNone(); }
    
    UFUNCTION(BlueprintCallable, Category = "MedCom|Loadout")
    void LogLoadoutStatus();
    
    //========================================
    // Component Access
    //========================================
    
    UFUNCTION(BlueprintCallable, Category = "MedCom|Inventory")
    UMedComInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

    UFUNCTION(BlueprintCallable, Category="MedCom|Equipment")
    UMedComEquipmentDataStore* GetEquipmentDataStore() const { return EquipmentDataStore; }
    
    UFUNCTION(BlueprintCallable, Category="MedCom|Equipment")
    UMedComEquipmentTransactionProcessor* GetEquipmentTxnProcessor() const { return EquipmentTxnProcessor; }
    
    UFUNCTION(BlueprintCallable, Category="MedCom|Equipment")
    UMedComEquipmentOperationExecutor* GetEquipmentOps() const { return EquipmentOps; }

    //========================================
    // Debug
    //========================================
    
    void DebugActiveEffects();

protected:
    //========================================
    // Core Components (Replicated)
    //========================================
    
    /** Ability System Component */
    UPROPERTY(VisibleAnywhere, Replicated, Category = "MedCom|Core")
    UMedComAbilitySystemComponent* ASC = nullptr;
    
    /** Inventory component for item management */
    UPROPERTY(VisibleAnywhere, Replicated, Category = "MedCom|Inventory")
    UMedComInventoryComponent* InventoryComponent = nullptr;

    /** Attribute set (spawned by ASC) */
    UPROPERTY()
    UMedComBaseAttributeSet* Attributes = nullptr;

    //========================================
    // GAS Configuration
    //========================================
    
    UPROPERTY(EditDefaultsOnly, Category = "MedCom|Attributes")
    TSubclassOf<UMedComBaseAttributeSet> InitialAttributeSetClass;

    UPROPERTY(EditDefaultsOnly, Category = "MedCom|Attributes")
    TSubclassOf<UGameplayEffect> InitialAttributesEffect;

    UPROPERTY(EditDefaultsOnly, Category = "MedCom|Abilities") 
    TArray<FAbilityInfo> AbilityPool;
    
    UPROPERTY(EditDefaultsOnly, Category = "MedCom|Abilities") 
    TSubclassOf<UGameplayAbility> InteractAbility;
    
    UPROPERTY(EditDefaultsOnly, Category = "MedCom|Abilities") 
    TSubclassOf<UGameplayAbility> SprintAbility;
    
    UPROPERTY(EditDefaultsOnly, Category = "MedCom|Abilities") 
    TSubclassOf<UGameplayAbility> CrouchAbility;
    
    UPROPERTY(EditDefaultsOnly, Category = "MedCom|Abilities") 
    TSubclassOf<UGameplayAbility> JumpAbility;

    UPROPERTY(EditDefaultsOnly, Category = "MedCom|Abilities") 
    TSubclassOf<UGameplayAbility> WeaponSwitchAbility;
 
    UPROPERTY(EditDefaultsOnly, Category = "MedCom|Passive Effects")
    TSubclassOf<UGameplayEffect> PassiveHealthRegenEffect;
    
    UPROPERTY(EditDefaultsOnly, Category = "MedCom|Passive Effects")
    TSubclassOf<UGameplayEffect> PassiveStaminaRegenEffect;
    
    //========================================
    // Weapon State
    //========================================
    
    UPROPERTY(Transient)
    bool bHasWeapon = false;
    
    UPROPERTY(Transient)
    AActor* CurrentWeaponActor = nullptr;
    
    //========================================
    // Loadout State
    //========================================
    
    /** Currently applied loadout ID (replicated for UI) */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "MedCom|Loadout")
    FName CurrentLoadoutID = NAME_None;
    
    /** Default loadout to apply on spawn */
    UPROPERTY(EditDefaultsOnly, Category = "MedCom|Loadout")
    FName DefaultLoadoutID = TEXT("Default_Soldier");
    
    /** Whether to automatically apply default loadout on begin play */
    UPROPERTY(EditDefaultsOnly, Category = "MedCom|Loadout")
    bool bAutoApplyDefaultLoadout = true;
    
    /** Whether to log loadout operations */
    UPROPERTY(EditDefaultsOnly, Category = "MedCom|Loadout")
    bool bLogLoadoutOperations = true;

    /** Flag to track if component listeners have been set up */
    UPROPERTY(Transient)
    bool bComponentListenersSetup = false;

    //========================================
    // Equipment Module Components (Per-Player)
    //========================================
    
    /** Core data store for equipment state (Server authoritative, replicated) */
    UPROPERTY(VisibleAnywhere, Replicated, Category = "MedCom|Equipment|Core")
    UMedComEquipmentDataStore* EquipmentDataStore = nullptr;

    /** Transaction processor for atomic equipment changes */
    UPROPERTY(VisibleAnywhere, Replicated, Category = "MedCom|Equipment|Core")
    UMedComEquipmentTransactionProcessor* EquipmentTxnProcessor = nullptr;

    /** Operation executor (deterministic, validated) */
    UPROPERTY(VisibleAnywhere, Replicated, Category = "MedCom|Equipment|Core")
    UMedComEquipmentOperationExecutor* EquipmentOps = nullptr;

    /** Prediction system (client owning) */
    UPROPERTY(VisibleAnywhere, Replicated, Category = "MedCom|Equipment|Networking")
    UMedComEquipmentPredictionSystem* EquipmentPrediction = nullptr;

    /** Replication manager (delta-based replication) */
    UPROPERTY(VisibleAnywhere, Replicated, Category = "MedCom|Equipment|Networking")
    UMedComEquipmentReplicationManager* EquipmentReplication = nullptr;

    /** Network dispatcher (RPC/request queue) */
    UPROPERTY(VisibleAnywhere, Replicated, Category = "MedCom|Equipment|Networking")
    UMedComEquipmentNetworkDispatcher* EquipmentNetworkDispatcher = nullptr;

    /** Event dispatcher / equipment event bus (local) */
    UPROPERTY(VisibleAnywhere, Replicated, Category = "MedCom|Equipment|Events")
    UMedComEquipmentEventDispatcher* EquipmentEventDispatcher = nullptr;

    /** Weapon state manager (FSM) */
    UPROPERTY(VisibleAnywhere, Replicated, Category = "MedCom|Equipment|Weapon")
    UMedComWeaponStateManager* WeaponStateManager = nullptr;

    /** Inventory bridge (connects equipment to existing inventory) */
    UPROPERTY(VisibleAnywhere, Replicated, Category = "MedCom|Equipment|Inventory")
    UMedComEquipmentInventoryBridge* EquipmentInventoryBridge = nullptr;

    /**
     * DEPRECATED: System coordinator - kept for backward compatibility only
     * Global services are now managed by UMedComSystemCoordinatorSubsystem
     */
    UPROPERTY(VisibleAnywhere, Replicated, Category = "MedCom|Equipment|DEPRECATED")
    UMedComSystemCoordinator* EquipmentSystemCoordinator = nullptr;

    /**
     * Slot validator (UObject, not component)
     * Created during WireEquipmentModule()
     * Validates equipment slot operations
     */
    UPROPERTY()
    UMedComEquipmentSlotValidator* EquipmentSlotValidator = nullptr;
    //========================================
    // Equipment Wiring Retry (Server-side)
    //========================================
    
    /** Текущее количество попыток подключения equipment module */
    int32 EquipmentWireRetryCount = 0;
    
    /** Handle таймера для retry */
    FTimerHandle EquipmentWireRetryHandle;
    
    /** Максимальное количество попыток (20 × 50ms = 1 секунда) */
    static constexpr int32 MaxEquipmentWireRetries = 20;
    
    /** Интервал между попытками (50 миллисекунд) */
    static constexpr float EquipmentWireRetryInterval = 0.05f;

    /**
     * Попытка подключить equipment module один раз
     * Проверяет готовность глобальных сервисов через CoordinatorSubsystem
     * @return true если успешно подключено, false если нужна ещё одна попытка
     */
    bool TryWireEquipmentModuleOnce();
    //========================================
    // Internal Methods
    //========================================
    
    void InitAttributes();
    void GrantStartupAbilities();
    void ApplyPassiveStartupEffects();
    bool CheckAuthority(const TCHAR* Fn) const;
    
    UMedComLoadoutManager* GetLoadoutManager() const;
    FLoadoutApplicationResult ApplyLoadoutToComponents(const FName& LoadoutID, UMedComLoadoutManager* LoadoutManager);
    void ResetComponentsForLoadout(bool bPreserveRuntimeData);
    
    UFUNCTION() 
    void HandleComponentInitialized();
    
    UFUNCTION() 
    void HandleComponentUpdated();

    bool SetupComponentListeners();
    void CleanupComponentListeners();

    /**
     * Wire up per-player equipment components with global services
     * CRITICAL: Called AFTER loadout application to ensure slots are configured
     * 
     * @param LoadoutManager - Manager containing loadout configuration
     * @param AppliedLoadoutID - ID of loadout that was just applied
     * @return true if wiring succeeded, false otherwise
     */
    bool WireEquipmentModule(UMedComLoadoutManager* LoadoutManager, const FName& AppliedLoadoutID);

    //========================================
    // Attribute Change Callbacks
    //========================================
    
    void SetupAttributeChangeCallbacks();
    void CleanupAttributeChangeCallbacks();
    void OnHealthChanged(const FOnAttributeChangeData& Data);
    void OnMaxHealthChanged(const FOnAttributeChangeData& Data);
    void OnStaminaChanged(const FOnAttributeChangeData& Data);
    void OnMaxStaminaChanged(const FOnAttributeChangeData& Data);
    void OnMovementSpeedChanged(const FOnAttributeChangeData& Data);
    void OnSprintTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
    
    FDelegateHandle HealthChangedDelegateHandle;
    FDelegateHandle MaxHealthChangedDelegateHandle;
    FDelegateHandle StaminaChangedDelegateHandle;
    FDelegateHandle MaxStaminaChangedDelegateHandle;
    FDelegateHandle MovementSpeedChangedDelegateHandle;
    FDelegateHandle SprintTagChangedDelegateHandle;

    FGameplayTag SprintingTag;
};