// Copyright Suspense Team. All Rights Reserved.

#include "Interfaces/UI/ISuspenseInventoryUIBridge.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

// Global static variable for storing bridge instance
namespace
{
    // Using TWeakInterfacePtr for safe weak reference
    TWeakInterfacePtr<ISuspenseInventoryUIBridgeInterface> GInventoryUIBridge;
}

// =====================================================
// Static Method Implementations
// =====================================================

ISuspenseInventoryUIBridgeInterface* ISuspenseInventoryUIBridgeInterface::GetInventoryUIBridge(const UObject* WorldContext)
{
    // First try global bridge
    if (GInventoryUIBridge.IsValid())
    {
        return GInventoryUIBridge.Get();
    }
    
    // Fallback: try to find from world context
    if (WorldContext)
    {
        UWorld* World = WorldContext->GetWorld();
        if (World)
        {
            // You could implement additional search logic here
            // For example, searching through GameInstance subsystems
            // or specific actors in the world
        }
    }
    
    return nullptr;
}

TScriptInterface<ISuspenseInventoryUIBridgeInterface> ISuspenseInventoryUIBridgeInterface::GetGlobalBridge(const UObject* WorldContext)
{
    TScriptInterface<ISuspenseInventoryUIBridgeInterface> Result;

    // First check global static
    if (GInventoryUIBridge.IsValid())
    {
        UObject* BridgeObject = GInventoryUIBridge.GetObject();
        ISuspenseInventoryUIBridgeInterface* BridgeInterface = GInventoryUIBridge.Get();
        
        if (BridgeObject && BridgeInterface)
        {
            Result.SetObject(BridgeObject);
            Result.SetInterface(BridgeInterface);
            return Result;
        }
    }
    
    // Fallback to raw pointer search and convert
    ISuspenseInventoryUIBridgeInterface* RawBridge = GetInventoryUIBridge(WorldContext);
    if (RawBridge)
    {
        return MakeScriptInterface(RawBridge);
    }
    
    return Result; // Returns empty/invalid TScriptInterface
}

void ISuspenseInventoryUIBridgeInterface::SetGlobalBridge(ISuspenseInventoryUIBridgeInterface* Bridge)
{
    if (Bridge)
    {
        // TWeakInterfacePtr can be constructed from raw interface pointer
        GInventoryUIBridge = TWeakInterfacePtr<ISuspenseInventoryUIBridgeInterface>(Bridge);

        UE_LOG(LogTemp, Log, TEXT("[ISuspenseInventoryUIBridgeInterface] Global bridge set successfully"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[ISuspenseInventoryUIBridgeInterface] Attempted to set null bridge"));
    }
}

void ISuspenseInventoryUIBridgeInterface::ClearGlobalBridge()
{
    GInventoryUIBridge.Reset();
    UE_LOG(LogTemp, Log, TEXT("[ISuspenseInventoryUIBridgeInterface] Global bridge cleared"));
}

TScriptInterface<ISuspenseInventoryUIBridgeInterface> ISuspenseInventoryUIBridgeInterface::MakeScriptInterface(ISuspenseInventoryUIBridgeInterface* RawInterface)
{
    TScriptInterface<ISuspenseInventoryUIBridgeInterface> Result;

    if (RawInterface)
    {
        // Get the UObject that implements this interface
        UObject* InterfaceObject = Cast<UObject>(RawInterface);
        if (InterfaceObject)
        {
            Result.SetObject(InterfaceObject);
            Result.SetInterface(RawInterface);
        }
        else
        {
            // This shouldn't happen in normal circumstances
            // but we handle it for safety
            UE_LOG(LogTemp, Error, TEXT("[ISuspenseInventoryUIBridgeInterface] Failed to cast interface to UObject"));
        }
    }

    return Result;
}