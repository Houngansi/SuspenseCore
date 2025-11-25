// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interfaces/Core/ISuspenseLoadout.h"
#include "Types/Equipment/SuspenseEquipmentTypes.h"
#include "Types/Loadout/SuspenseLoadoutSettings.h"
#include "ISuspenseLoadoutAdapter.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseLoadoutAdapter : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for loadout system integration
 * 
 * Philosophy: Adapter for loadout configuration application.
 * Translates loadout data to equipment operations.
 */
class BRIDGESYSTEM_API ISuspenseLoadoutAdapter
{
    GENERATED_BODY()

public:
    /**
     * Apply loadout configuration
     * @param LoadoutId Loadout to apply
     * @param bForce Force application
     * @return Application result
     */
    virtual FLoadoutApplicationResult ApplyLoadout(const FName& LoadoutId, bool bForce = false) = 0;
    
    /**
     * Save current configuration as loadout
     * @param LoadoutId Loadout ID to save as
     * @return True if saved
     */
    virtual bool SaveAsLoadout(const FName& LoadoutId) = 0;
    
    /**
     * Validate loadout compatibility
     * @param LoadoutId Loadout to validate
     * @param OutErrors Validation errors
     * @return True if compatible
     */
    virtual bool ValidateLoadout(const FName& LoadoutId, TArray<FText>& OutErrors) const = 0;
    
    /**
     * Get current loadout ID
     * @return Current loadout or NAME_None
     */
    virtual FName GetCurrentLoadout() const = 0;
    
    /**
     * Convert equipment state to loadout format
     * @param State Equipment state
     * @return Loadout configuration
     */
    virtual FLoadoutConfiguration ConvertToLoadoutFormat(const FEquipmentStateSnapshot& State) const = 0;
    
    /**
     * Convert loadout to equipment operations
     * @param Loadout Loadout configuration
     * @return Array of operations
     */
    virtual TArray<FEquipmentOperationRequest> ConvertFromLoadoutFormat(const FLoadoutConfiguration& Loadout) const = 0;
    
    /**
     * Get loadout preview
     * @param LoadoutId Loadout to preview
     * @return Preview data
     */
    virtual FString GetLoadoutPreview(const FName& LoadoutId) const = 0;
};