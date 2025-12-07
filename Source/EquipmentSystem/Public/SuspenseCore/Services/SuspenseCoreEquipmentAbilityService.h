// SuspenseEquipmentAbilityService.h
// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Interfaces/Equipment/ISuspenseEquipmentService.h"
#include "Core/Utils/SuspenseEquipmentThreadGuard.h"
#include "Core/Utils/SuspenseEquipmentEventBus.h"
#include "Core/Utils/SuspenseEquipmentCacheManager.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceMacros.h"
#include "GameplayTagContainer.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Engine/DataTable.h"
#include "Engine/StreamableManager.h"
#include <atomic>
#include "SuspenseCoreEquipmentAbilityService.generated.h"

// Forward declarations
class USuspenseCoreEquipmentAbilityConnector;
class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;

/**
 * Configuration for equipment abilities loaded from DataTable
 * Maps equipment items to their granted abilities and effects
 */
USTRUCT(BlueprintType)
struct FSuspenseCoreEquipmentAbilityMapping : public FTableRowBase
{
    GENERATED_BODY()

    /** Item ID this mapping applies to */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    FName ItemID;

    /** Abilities to grant when this equipment is active */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;

    /** Passive effects to apply when this equipment is active */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    TArray<TSubclassOf<UGameplayEffect>> PassiveEffects;

    /** Required tags on the equipment actor to grant abilities */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability", meta = (Categories = "Equipment"))
    FGameplayTagContainer RequiredTags;

    /** Tags that prevent ability granting if present on equipment */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability", meta = (Categories = "Equipment"))
    FGameplayTagContainer BlockedTags;

    /** Input tag for primary ability activation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability", meta = (Categories = "Input"))
    FGameplayTag PrimaryInputTag;

    /** Input tag for secondary ability activation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability", meta = (Categories = "Input"))
    FGameplayTag SecondaryInputTag;

    /** Validates this mapping entry */
    bool IsValid() const
    {
        return !ItemID.IsNone();
    }
};

/**
 * Equipment Ability Service - Equipment Actor Coordinator
 *
 * ARCHITECTURAL PHILOSOPHY:
 * This service manages abilities that EQUIPMENT ACTORS provide, NOT character abilities.
 * - Character abilities (sprint, jump, etc.) are managed in PlayerState
 * - Equipment abilities (weapon fire, armor shield, etc.) are managed here
 * - Each equipment actor gets its own AbilityConnector
 * - The connector bridges equipment abilities to the owner's ASC
 *
 * KEY RESPONSIBILITIES:
 * 1. Create/destroy AbilityConnectors for equipment actors
 * 2. Load ability mappings from DataTables (item->abilities configuration)
 * 3. React to equipment spawn/destroy events
 * 4. Coordinate ability granting to owner's ASC through connectors
 *
 * Thread Safety: All public methods MUST be called on GameThread (GAS requirement)
 */
UCLASS()
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentAbilityService : public UObject, public ISuspenseEquipmentService
{
    GENERATED_BODY()

public:
    USuspenseCoreEquipmentAbilityService();
    virtual ~USuspenseCoreEquipmentAbilityService();

    //========================================
    // IEquipmentService Implementation
    //========================================

    virtual bool InitializeService(const FServiceInitParams& Params) override;
    virtual bool ShutdownService(bool bForce = false) override;
    virtual EServiceLifecycleState GetServiceState() const override { return ServiceState; }
    virtual bool IsServiceReady() const override { return ServiceState == EServiceLifecycleState::Ready; }
    virtual FGameplayTag GetServiceTag() const override;
    virtual FGameplayTagContainer GetRequiredDependencies() const override;
    virtual bool ValidateService(TArray<FText>& OutErrors) const override;
    virtual void ResetService() override;
    virtual FString GetServiceStats() const override;
    virtual void BeginDestroy() override;
    //========================================
    // Public API - Configuration
    //========================================

    /** Load ability mappings from a DataTable */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Abilities")
    int32 LoadAbilityMappings(UDataTable* MappingTable);

    /**
     * Get or create ability connector for an equipment actor
     * @return Connector component or nullptr if ASC not found on owner
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Abilities")
    USuspenseCoreEquipmentAbilityConnector* GetOrCreateConnectorForEquipment(
        AActor* EquipmentActor,
        AActor* OwnerActor
    );

    /** Remove ability connector from equipment actor */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Abilities")
    bool RemoveConnectorForEquipment(AActor* EquipmentActor);

    /** Check if item has ability mapping */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Abilities", BlueprintPure)
    bool HasAbilityMapping(FName ItemID) const;

    /** Get ability mapping for item */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Abilities")
    bool GetAbilityMapping(FName ItemID, FSuspenseCoreEquipmentAbilityMapping& OutMapping) const;

    /** Export service metrics to CSV file */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Abilities|Debug")
    bool ExportMetricsToCSV(const FString& FilePath) const;

    //========================================
    // Public API - Operations
    //========================================

    /** Process equipment spawn - creates connector and grants abilities */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Abilities")
    void ProcessEquipmentSpawn(
        AActor* EquipmentActor,
        AActor* OwnerActor,
        const FSuspenseInventoryItemInstance& ItemInstance
    );

    /** Process equipment destroy - removes connector and abilities */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Abilities")
    void ProcessEquipmentDestroy(AActor* EquipmentActor);

    /** Update equipment abilities when item data changes */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Abilities")
    void UpdateEquipmentAbilities(
        AActor* EquipmentActor,
        const FSuspenseInventoryItemInstance& UpdatedItemInstance
    );

    /** Clean up invalid/destroyed equipment connectors */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Abilities|Debug")
    int32 CleanupInvalidConnectors();

protected:
    /** Initialize default ability mappings */
    void InitializeDefaultMappings();

    /** Setup event subscriptions */
    void SetupEventHandlers();

    /** Ensure configuration is valid */
    void EnsureValidConfig();

    /** Handle equipment spawned event */
    void OnEquipmentSpawned(const FSuspenseEquipmentEventData& EventData);

    /** Handle equipment destroyed event */
    void OnEquipmentDestroyed(const FSuspenseEquipmentEventData& EventData);

    /** New S7 handlers */
    void OnEquipped(const FSuspenseEquipmentEventData& EventData);
    void OnUnequipped(const FSuspenseEquipmentEventData& EventData);
    void OnAbilitiesRefresh(const FSuspenseEquipmentEventData& EventData);
    void OnCommit(const FSuspenseEquipmentEventData& EventData);

    /** Handle equipment actor destroyed directly */
    UFUNCTION()
    void OnEquipmentActorDestroyed(AActor* DestroyedActor);

    /** Timer callback for periodic cleanup */
    UFUNCTION()
    void OnCleanupTimer();

    /** Create connector for equipment actor */
    USuspenseCoreEquipmentAbilityConnector* CreateConnectorForEquipment(
        AActor* EquipmentActor,
        AActor* OwnerActor
    );

    /** Find AbilitySystemComponent on owner actor */
    UAbilitySystemComponent* FindOwnerAbilitySystemComponent(AActor* OwnerActor) const;

    /** Get gameplay tags from equipment actor */
    FGameplayTagContainer GetEquipmentTags(AActor* EquipmentActor) const;

    /** Parse equipment event data */
    bool ParseEquipmentEventData(
        const FSuspenseEquipmentEventData& EventData,
        FSuspenseInventoryItemInstance& OutItem,
        AActor*& OutEquipmentActor,
        AActor*& OutOwnerActor
    ) const;

private:
    //========================================
    // Service State
    //========================================
    UPROPERTY()
    EServiceLifecycleState ServiceState = EServiceLifecycleState::Uninitialized;

    //========================================
    // Configuration Data
    //========================================
    UPROPERTY()
    TMap<FName, FSuspenseCoreEquipmentAbilityMapping> AbilityMappings;

    UPROPERTY(EditDefaultsOnly, Category = "Configuration",
        meta = (AllowedClasses = "/Script/Engine.DataTable"))
    TSoftObjectPtr<UDataTable> DefaultMappingTable;

    UPROPERTY(EditDefaultsOnly, Category = "Configuration", meta = (ClampMin = "60.0", ClampMax = "3600.0"))
    float MappingCacheTTL = 300.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Configuration", meta = (ClampMin = "10.0", ClampMax = "300.0"))
    float CleanupInterval = 60.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    bool bEnableDetailedLogging = false;

    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    bool bEnablePeriodicCleanup = true;

    /** Was cache registered in the global registry */
    bool bCacheRegistered = false;

    //========================================
    // Runtime Data
    //========================================
    UPROPERTY()
    TMap<TWeakObjectPtr<AActor>, USuspenseCoreEquipmentAbilityConnector*> EquipmentConnectors;

    UPROPERTY()
    TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<AActor>> EquipmentToOwnerMap;

    TSharedPtr<FSuspenseEquipmentCacheManager<FName, FSuspenseCoreEquipmentAbilityMapping>> MappingCache;
    FStreamableManager StreamableManager;
    FTimerHandle CleanupTimerHandle;

    //========================================
    // Thread Safety
    //========================================
    mutable FEquipmentRWLock ConnectorLock;
    mutable FEquipmentRWLock MappingLock;

    //========================================
    // Event Management
    //========================================
    TArray<FEventSubscriptionHandle> EventSubscriptions;

    /** S7 event tags (initialized in InitializeService) */
    FGameplayTag Tag_OnEquipped;
    FGameplayTag Tag_OnUnequipped;
    FGameplayTag Tag_OnAbilitiesRefresh;
    FGameplayTag Tag_OnCommit;

    //========================================
    // Metrics and Statistics
    //========================================
    mutable FServiceMetrics ServiceMetrics;

    // THREAD-SAFETY FIX: Use atomic for concurrent access
    mutable std::atomic<int32> CacheHits{0};
    mutable std::atomic<int32> CacheMisses{0};
};
