// SuspenseCoreSystemCoordinatorComponent.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Components/Validation/SuspenseCoreEquipmentSlotValidator.h"
#include "Core/Services/SuspenseEquipmentServiceLocator.h"
#include "SuspenseCoreSystemCoordinatorComponent.generated.h"

/**
 * System Coordinator - Service registration orchestrator
 * Services pull their own dependencies via ServiceLocator
 * Owned by UMedComSystemCoordinatorSubsystem for persistent lifecycle
 */
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreSystemCoordinatorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USuspenseCoreSystemCoordinatorComponent();

    //~ UActorComponent
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Explicit bootstrap - can be called from game code */
    UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Coordinator")
    bool BootstrapServices();

    /** Validate all services ready */
    UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Coordinator")
    bool ValidateServices(TArray<FText>& OutErrors) const;

    /** PUBLIC: for calling from subsystem or player state if needed */
    UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Coordinator")
    void RegisterCoreServices();
    /**
     * Register presentation layer services (Factory, Attachment, Visual)
     */
    void RegisterPresentationServices();
    /** PUBLIC: warm up services (initialize caches, subscriptions) */
    UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Coordinator")
    void WarmUpServices();

    /**
     * Explicit shutdown - clean up subscriptions, timers, references
     * Called by SystemCoordinatorSubsystem on Deinitialize
     * CRITICAL: Must be called to prevent leaks
     */
    void Shutdown();

protected:
    /** Get ServiceLocator */
    USuspenseEquipmentServiceLocator* GetLocator() const;

    /** Extract service tag from CDO */
    FGameplayTag GetServiceTagFromClass(UClass* ServiceClass) const;

private:
    /** Cached tags from CDO */
    UPROPERTY(Transient) FGameplayTag DataServiceTag;
    UPROPERTY(Transient) FGameplayTag ValidationServiceTag;
    UPROPERTY(Transient) FGameplayTag OperationServiceTag;
    UPROPERTY(Transient) FGameplayTag VisualizationServiceTag;
    UPROPERTY(Transient) FGameplayTag AbilityServiceTag;

    UPROPERTY()
    USuspenseCoreEquipmentSlotValidator* SlotValidator = nullptr;

    UPROPERTY(Transient)
    bool bBootstrapped = false;
};
