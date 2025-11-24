// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "Interfaces/Interaction/ISuspenseItemFactoryInterface.h"
#include "SuspenseItemFactory.generated.h"

// Forward declarations
class UEventDelegateManager;
class UMedComItemManager;
struct FMedComUnifiedItemData;

/**
 * Item factory for creating pickup actors
 * Works with unified DataTable system
 */
UCLASS()
class SUSPENSEINTERACTION_API USuspenseItemFactory : public UGameInstanceSubsystem, public ISuspenseItemFactoryInterface
{
    GENERATED_BODY()
    
public:
    // Subsystem lifecycle
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    
    /**
     * Create pickup from item ID
     * @param ItemID Item identifier from DataTable
     * @param World World for spawning
     * @param Transform Spawn transform
     * @param Quantity Item amount
     * @return Spawned pickup actor
     */
    virtual AActor* CreatePickupFromItemID_Implementation(
        FName ItemID,
        UWorld* World,
        const FTransform& Transform,
        int32 Quantity
    ) override;
    
    /**
     * Create pickup with custom properties
     * @param ItemID Item identifier
     * @param World World for spawning
     * @param Transform Spawn transform
     * @param Quantity Item amount
     * @param bWithAmmoState Whether to include ammo state
     * @param CurrentAmmo Current ammo count
     * @param RemainingAmmo Remaining ammo count
     * @return Spawned pickup actor
     */
    UFUNCTION(BlueprintCallable, Category = "Item Factory")
    AActor* CreatePickupWithAmmo(
        FName ItemID,
        UWorld* World,
        const FTransform& Transform,
        int32 Quantity,
        bool bWithAmmoState,
        float CurrentAmmo,
        float RemainingAmmo
    );
    
    /**
     * Get default pickup class
     * @return Default pickup actor class
     */
    UFUNCTION(BlueprintCallable, Category = "Item Factory")
	TSubclassOf<AActor> GetDefaultPickupClass_Implementation() const override;
    
    /**
     * Set default pickup class
     * @param NewDefaultClass New default class
     */
    UFUNCTION(BlueprintCallable, Category = "Item Factory")
    void SetDefaultPickupClass(TSubclassOf<AActor> NewDefaultClass);
    
protected:
    // Default pickup actor class
    UPROPERTY(EditDefaultsOnly, Category = "Factory Settings")
    TSubclassOf<AActor> DefaultPickupClass;
    
    // Cached references
    UPROPERTY(Transient)
    TWeakObjectPtr<UEventDelegateManager> CachedDelegateManager;
    
    UPROPERTY(Transient)
    TWeakObjectPtr<UMedComItemManager> CachedItemManager;
    
    // Get delegate manager
    UEventDelegateManager* GetDelegateManager() const;
    
    // Get item manager
    UMedComItemManager* GetItemManager() const;
    
    // Configure spawned pickup
    void ConfigurePickup(AActor* PickupActor, const FMedComUnifiedItemData& ItemData, int32 Quantity);
    
    // Configure weapon pickup
    void ConfigureWeaponPickup(AActor* PickupActor, const FMedComUnifiedItemData& ItemData, 
        bool bWithAmmoState, float CurrentAmmo, float RemainingAmmo);
    
    // Broadcast item creation event
    void BroadcastItemCreated(AActor* CreatedActor, FName ItemID, int32 Quantity);
};