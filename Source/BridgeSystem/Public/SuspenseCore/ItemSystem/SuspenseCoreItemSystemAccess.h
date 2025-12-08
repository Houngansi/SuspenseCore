// MedComShared/Public/ItemSystem/SuspenseItemSystemAccess.h
// Copyright Suspense Team. All Rights Reserved.

#ifndef SUSPENSECORE_ITEMSYSTEM_SUSPENSECOREITEMSYSTEMACCESS_H
#define SUSPENSECORE_ITEMSYSTEM_SUSPENSECOREITEMSYSTEMACCESS_H

#include "CoreMinimal.h"

// Forward declarations
class USuspenseItemManager;
class UWorld;
class UGameInstance;

/**
 * Centralized access point for ItemManager subsystem
 * 
 * ARCHITECTURAL PURPOSE:
 * This is the ONLY place in the codebase where World → GameInstance → ItemManager 
 * navigation happens. All other code must use this accessor to get ItemManager.
 * 
 * WHY THIS MATTERS FOR MMO FPS:
 * - Single point of failure = easier debugging in production
 * - Consistent error handling and logging across entire codebase  
 * - Makes dependency on World context explicit and traceable
 * - Prevents duplicate navigation logic scattered across files
 * 
 * USAGE PATTERN:
 * Always pass 'this' from your calling context (Component, Actor, Ability):
 * 
 *   USuspenseItemManager* ItemManager = FItemSystemAccess::GetItemManager(this);
 *   if (!ItemManager)
 *   {
 *       // Handle error - ItemManager is critical for gameplay
 *       return;
 *   }
 *   // Use ItemManager safely
 * 
 * THREAD SAFETY:
 * This accessor is NOT thread-safe. Always call from GameThread.
 * UE Subsystems are designed for GameThread access only.
 */
class BRIDGESYSTEM_API FItemSystemAccess
{
public:
    /**
     * Get ItemManager subsystem from WorldContextObject
     * 
     * This is the primary method for accessing ItemManager throughout the codebase.
     * Performs full validation of each step in the chain and logs detailed errors.
     * 
     * NAVIGATION CHAIN:
     * WorldContextObject → UWorld → UGameInstance → USuspenseItemManager (Subsystem)
     * 
     * ERROR HANDLING:
     * Returns nullptr and logs detailed warning if any step fails.
     * Calling code MUST check for nullptr before using returned pointer.
     * 
     * PERFORMANCE:
     * This involves pointer dereferences but is fast enough for normal gameplay.
     * For hot paths (called every frame), consider component-level caching.
     * 
     * @param WorldContextObject - Any UObject with valid World context (Actor, Component, etc)
     * @return Valid ItemManager pointer or nullptr if chain is broken
     */
    static USuspenseItemManager* GetItemManager(const UObject* WorldContextObject);
    
    /**
     * Get ItemManager with comprehensive diagnostics
     * 
     * Same as GetItemManager but provides detailed step-by-step diagnostics
     * about what failed in the navigation chain. Use this for debugging when
     * ItemManager access mysteriously fails.
     * 
     * @param WorldContextObject - Any UObject with valid World context
     * @param OutDiagnostics - Detailed string describing each step of the attempt
     * @return Valid ItemManager pointer or nullptr if chain is broken
     */
    static USuspenseItemManager* GetItemManagerWithDiagnostics(
        const UObject* WorldContextObject, 
        FString& OutDiagnostics);
    
private:
    // Static utility class - no instances allowed
    FItemSystemAccess() = delete;
    ~FItemSystemAccess() = delete;
    FItemSystemAccess(const FItemSystemAccess&) = delete;
    FItemSystemAccess& operator=(const FItemSystemAccess&) = delete;
};

#endif // SUSPENSECORE_ITEMSYSTEM_SUSPENSECOREITEMSYSTEMACCESS_H