// MedComShared/Private/ItemSystem/ItemSystemAccess.cpp
// Copyright MedCom Team. All Rights Reserved.

#include "ItemSystem/ItemSystemAccess.h"
#include "ItemSystem/MedComItemManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

// Dedicated log category for ItemSystem access issues
DEFINE_LOG_CATEGORY_STATIC(LogItemSystemAccess, Log, All);

UMedComItemManager* FItemSystemAccess::GetItemManager(const UObject* WorldContextObject)
{
    // Step 1: Validate input parameter
    if (!WorldContextObject)
    {
        UE_LOG(LogItemSystemAccess, Warning, 
            TEXT("GetItemManager: WorldContextObject is null - cannot access ItemManager"));
        UE_LOG(LogItemSystemAccess, Warning, 
            TEXT("  Common causes: Called from constructor, called with invalid 'this' pointer"));
        UE_LOG(LogItemSystemAccess, Warning, 
            TEXT("  Solution: Ensure you're calling from BeginPlay or later, pass valid UObject"));
        return nullptr;
    }
    
    // Step 2: Get World from context object
    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        UE_LOG(LogItemSystemAccess, Warning, 
            TEXT("GetItemManager: Cannot get World from context object '%s' (class: %s)"),
            *WorldContextObject->GetName(),
            *WorldContextObject->GetClass()->GetName());
        UE_LOG(LogItemSystemAccess, Warning, 
            TEXT("  Common causes: Object not yet added to world, called too early in lifecycle"));
        UE_LOG(LogItemSystemAccess, Warning, 
            TEXT("  Solution: Call from BeginPlay or later when World is guaranteed valid"));
        return nullptr;
    }
    
    // Step 3: Get GameInstance from World
    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance)
    {
        UE_LOG(LogItemSystemAccess, Warning, 
            TEXT("GetItemManager: World '%s' has no GameInstance"),
            *World->GetName());
        UE_LOG(LogItemSystemAccess, Warning, 
            TEXT("  Common causes: Called during world transition, called on incomplete world"));
        UE_LOG(LogItemSystemAccess, Warning, 
            TEXT("  Solution: Ensure World is fully initialized before accessing subsystems"));
        return nullptr;
    }
    
    // Step 4: Get ItemManager subsystem from GameInstance
    UMedComItemManager* ItemManager = GameInstance->GetSubsystem<UMedComItemManager>();
    if (!ItemManager)
    {
        UE_LOG(LogItemSystemAccess, Error, 
            TEXT("GetItemManager: ItemManager subsystem not found in GameInstance '%s'"),
            *GameInstance->GetName());
        UE_LOG(LogItemSystemAccess, Error, 
            TEXT("  CRITICAL: This indicates serious configuration problem!"));
        UE_LOG(LogItemSystemAccess, Error, 
            TEXT("  Possible causes:"));
        UE_LOG(LogItemSystemAccess, Error, 
            TEXT("    1. ItemManager not registered as GameInstance Subsystem"));
        UE_LOG(LogItemSystemAccess, Error, 
            TEXT("    2. Module dependencies not set up correctly"));
        UE_LOG(LogItemSystemAccess, Error, 
            TEXT("    3. GameInstance is not MedComGameInstance"));
        UE_LOG(LogItemSystemAccess, Error, 
            TEXT("  Solution: Check that UMedComItemManager is properly defined as GameInstanceSubsystem"));
        return nullptr;
    }
    
    // Success - we have valid ItemManager
    UE_LOG(LogItemSystemAccess, VeryVerbose, 
        TEXT("GetItemManager: Successfully retrieved ItemManager from context '%s'"),
        *WorldContextObject->GetName());
    
    return ItemManager;
}

UMedComItemManager* FItemSystemAccess::GetItemManagerWithDiagnostics(
    const UObject* WorldContextObject,
    FString& OutDiagnostics)
{
    OutDiagnostics.Empty();
    
    // Build detailed diagnostic string as we walk the chain
    OutDiagnostics += TEXT("=== ItemManager Access Diagnostics ===\n");
    
    // Step 1: WorldContextObject validation
    OutDiagnostics += TEXT("Step 1: WorldContextObject validation\n");
    if (!WorldContextObject)
    {
        OutDiagnostics += TEXT("  FAILED: WorldContextObject is nullptr\n");
        OutDiagnostics += TEXT("  This means the calling code passed invalid pointer\n");
        return nullptr;
    }
    OutDiagnostics += FString::Printf(TEXT("  SUCCESS: Valid object '%s' (class: %s)\n"),
        *WorldContextObject->GetName(),
        *WorldContextObject->GetClass()->GetName());
    
    // Step 2: World retrieval
    OutDiagnostics += TEXT("Step 2: World retrieval from context\n");
    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        OutDiagnostics += TEXT("  FAILED: GetWorld() returned nullptr\n");
        OutDiagnostics += TEXT("  Object exists but has no World context\n");
        OutDiagnostics += TEXT("  Likely called too early in object lifecycle\n");
        return nullptr;
    }
    OutDiagnostics += FString::Printf(TEXT("  SUCCESS: Valid World '%s'\n"), *World->GetName());
    
    // ИСПРАВЛЕНО: Правильная конверсия EWorldType::Type enum в строку
    FString WorldTypeString;
    switch (World->WorldType)
    {
        case EWorldType::None: WorldTypeString = TEXT("None"); break;
        case EWorldType::Game: WorldTypeString = TEXT("Game"); break;
        case EWorldType::Editor: WorldTypeString = TEXT("Editor"); break;
        case EWorldType::PIE: WorldTypeString = TEXT("PIE"); break;
        case EWorldType::EditorPreview: WorldTypeString = TEXT("EditorPreview"); break;
        case EWorldType::GamePreview: WorldTypeString = TEXT("GamePreview"); break;
        case EWorldType::GameRPC: WorldTypeString = TEXT("GameRPC"); break;
        case EWorldType::Inactive: WorldTypeString = TEXT("Inactive"); break;
        default: WorldTypeString = TEXT("Unknown"); break;
    }
    OutDiagnostics += FString::Printf(TEXT("  World Type: %s\n"), *WorldTypeString);
    
    // ИСПРАВЛЕНО: Правильная конверсия ENetMode enum в строку
    FString NetModeString;
    switch (World->GetNetMode())
    {
        case NM_Standalone: NetModeString = TEXT("Standalone"); break;
        case NM_DedicatedServer: NetModeString = TEXT("DedicatedServer"); break;
        case NM_ListenServer: NetModeString = TEXT("ListenServer"); break;
        case NM_Client: NetModeString = TEXT("Client"); break;
        default: NetModeString = TEXT("Unknown"); break;
    }
    OutDiagnostics += FString::Printf(TEXT("  Net Mode: %s\n"), *NetModeString);
    
    // Step 3: GameInstance retrieval
    OutDiagnostics += TEXT("Step 3: GameInstance retrieval from World\n");
    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance)
    {
        OutDiagnostics += TEXT("  FAILED: GetGameInstance() returned nullptr\n");
        OutDiagnostics += TEXT("  World exists but GameInstance not initialized\n");
        OutDiagnostics += TEXT("  This can happen during world transition\n");
        return nullptr;
    }
    OutDiagnostics += FString::Printf(TEXT("  SUCCESS: Valid GameInstance '%s' (class: %s)\n"),
        *GameInstance->GetName(),
        *GameInstance->GetClass()->GetName());
    
    // Step 4: ItemManager subsystem retrieval  
    OutDiagnostics += TEXT("Step 4: ItemManager subsystem retrieval\n");
    UMedComItemManager* ItemManager = GameInstance->GetSubsystem<UMedComItemManager>();
    if (!ItemManager)
    {
        OutDiagnostics += TEXT("  FAILED: GetSubsystem<UMedComItemManager>() returned nullptr\n");
        OutDiagnostics += TEXT("  CRITICAL FAILURE - Subsystem not registered!\n");
        OutDiagnostics += TEXT("  Check:\n");
        OutDiagnostics += TEXT("    - UMedComItemManager defined with UCLASS() macro\n");
        OutDiagnostics += TEXT("    - Inherits from UGameInstanceSubsystem\n");
        OutDiagnostics += TEXT("    - Module properly linked in Build.cs\n");
        return nullptr;
    }
    OutDiagnostics += FString::Printf(TEXT("  SUCCESS: Valid ItemManager at address %p\n"), ItemManager);
    
    // Additional ItemManager state diagnostics
    OutDiagnostics += TEXT("Step 5: ItemManager state validation\n");
    if (ItemManager->GetCachedItemCount() == 0)
    {
        OutDiagnostics += TEXT("  WARNING: ItemManager has zero cached items\n");
        OutDiagnostics += TEXT("  This might indicate ItemManager not yet initialized\n");
        OutDiagnostics += TEXT("  Or DataTable not loaded correctly\n");
    }
    else
    {
        OutDiagnostics += FString::Printf(TEXT("  ItemManager has %d cached items\n"), 
            ItemManager->GetCachedItemCount());
    }
    
    OutDiagnostics += TEXT("=== Diagnostics Complete ===\n");
    OutDiagnostics += TEXT("RESULT: ItemManager access SUCCESSFUL\n");
    
    return ItemManager;
}