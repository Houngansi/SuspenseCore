// SuspenseSystemCoordinator.h
// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreSystemCoordinator.generated.h"

class USuspenseCoreSystemCoordinatorComponent;
class USuspenseEquipmentServiceLocator;

/**
 * GameInstance-level subsystem that owns and manages global equipment services.
 *
 * ARCHITECTURE:
 * - Owns USuspenseCoreSystemCoordinatorComponent (persistent, not temporary)
 * - Registers global services once via ServiceLocator
 * - Survives seamless/non-seamless travel
 * - Rebinds world-dependent services on world transitions
 * - Ensures single-instance global services (no duplication)
 *
 * LIFECYCLE:
 * 1. Initialize() - Create coordinator, register services
 * 2. OnPostWorldInitialization() - Initial world bind
 * 3. OnPostLoadMapWithWorld() - Rebind on travel
 * 4. OnWorldBeginPlay() - Final rebind before gameplay
 * 5. Deinitialize() - Clean shutdown
 *
 * THREAD SAFETY:
 * All methods are GameThread-only (checked via check(IsInGameThread()))
 *
 * PIE/MULTIPLAYER:
 * - Works correctly with PIE multi-client
 * - Works with Listen/Dedicated servers
 * - Services are per-GameInstance, components are per-PlayerState
 */
UCLASS(DisplayName="MedCom System Coordinator Subsystem", meta=(Comment="Owns global equipment services and manages their lifecycle"))
class EQUIPMENTSYSTEM_API USuspenseCoreSystemCoordinator : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    USuspenseCoreSystemCoordinator();

    //========================================
    // USubsystem Interface
    //========================================

    /** Called to determine if subsystem should be created for this outer */
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

    /** Initialize subsystem - register services, bind world delegates */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Cleanup subsystem - shutdown services, unbind delegates */
    virtual void Deinitialize() override;

    //========================================
    // Public Status API
    //========================================

    /**
     * Check if global services are ready
     * @return true if services passed validation and are operational
     */
    UFUNCTION(BlueprintPure, Category="MedCom|Services")
    bool AreGlobalServicesReady() const { return bServicesReady; }

    /**
     * Get ServiceLocator instance (read-only)
     * @return ServiceLocator or nullptr if not initialized
     */
    UFUNCTION(BlueprintPure, Category="MedCom|Services")
    USuspenseEquipmentServiceLocator* GetServiceLocator() const { return ServiceLocator; }

    //========================================
    // Manual Control (for edge cases/tests)
    //========================================

    /**
     * Force rebind all world-bindable services to a specific world
     * Use case: manual recovery after abnormal travel, testing
     * @param World - World to rebind to, nullptr = use current world
     */
    UFUNCTION(BlueprintCallable, Category="MedCom|Services")
    void ForceRebindWorld(UWorld* World = nullptr);

    //========================================
    // Debug Commands (Exec functions)
    //========================================

    /**
     * Dump current services state to log
     * Usage: ~ DebugDumpServicesState
     */
    UFUNCTION(Exec, Category="MedCom|Debug")
    void DebugDumpServicesState();

    /**
     * Force rebind to current world
     * Usage: ~ DebugForceRebind
     */
    UFUNCTION(Exec, Category="MedCom|Debug")
    void DebugForceRebind();

    //========================================
    // Coordinator Lifecycle Methods
    //========================================

    /**
     * Shuts down the coordinator and cleans up resources
     * Called from Deinitialize()
     */
    void Shutdown();

    /**
     * Registers core equipment services with the ServiceLocator
     * Called during initial service registration
     */
    void RegisterCoreServices();

    /**
     * Warms up services by initializing caches and subscriptions
     * Called after services are registered
     */
    void WarmUpServices();

    /**
     * Validates all registered services are properly configured
     * @param OutErrors Array to receive validation error messages
     * @return true if all services pass validation
     */
    bool ValidateServices(TArray<FText>& OutErrors) const;

private:
    //========================================
    // World Lifecycle Handlers
    //========================================

    /**
     * Called when world is initialized (first time or after travel)
     * Triggers: EnsureServicesRegistered + RebindAllWorldBindableServices
     */
    void OnPostWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS);

    /**
     * Called after map is loaded (seamless/non-seamless travel)
     * Triggers: RebindAllWorldBindableServices
     */
    void OnPostLoadMapWithWorld(UWorld* LoadedWorld);

    //========================================
    // Internal Operations
    //========================================

    /**
     * Register global services if not already registered (idempotent)
     * @param ForWorld - World context for potential world-dependent registration params
     */
    void EnsureServicesRegistered(UWorld* ForWorld);

    /**
     * Iterate all registered services and call RebindWorld on implementers
     * @param ForWorld - World to rebind to
     */
    void RebindAllWorldBindableServices(UWorld* ForWorld);

    /**
     * Validate all services via Coordinator and log results
     */
    void ValidateAndLog();

    /**
     * Safely get current world from GameInstance
     * @return Current world or nullptr if GI not available/world not ready
     */
    UWorld* TryGetCurrentWorldSafe() const;

private:
    //========================================
    // Owned Objects
    //========================================

    /**
     * ServiceLocator instance - registry of all services
     * Either retrieved from world or created with GI outer
     */
    UPROPERTY(Transient)
    TObjectPtr<USuspenseEquipmentServiceLocator> ServiceLocator = nullptr;

    //========================================
    // State Flags
    //========================================

    /** Services have been registered in ServiceLocator */
    bool bServicesRegistered = false;

    /** Services passed validation and are operational */
    bool bServicesReady = false;

    /** Guard flag to prevent reentrant rebind calls during map load */
    bool bRebindInProgress = false;

    //========================================
    // Delegate Handles (for cleanup)
    //========================================

    FDelegateHandle PostWorldInitHandle;
    FDelegateHandle PostLoadMapHandle;

    //========================================
    // Metrics (for monitoring/debugging)
    //========================================

    /** Count of rebind operations performed */
    UPROPERTY(Transient)
    int32 RebindCount = 0;

    /** Last world that was bound */
    UPROPERTY(Transient)
    TWeakObjectPtr<UWorld> LastBoundWorld = nullptr;
};
