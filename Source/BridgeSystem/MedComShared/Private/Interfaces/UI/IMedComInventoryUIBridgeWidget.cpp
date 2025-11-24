// Copyright MedCom Team. All Rights Reserved.

#include "Interfaces/UI/IMedComInventoryUIBridgeWidget.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

// Global static variable for storing bridge instance
namespace
{
    // Using TWeakInterfacePtr for safe weak reference
    TWeakInterfacePtr<IMedComInventoryUIBridgeWidget> GInventoryUIBridge;
}

// =====================================================
// Static Method Implementations
// =====================================================

IMedComInventoryUIBridgeWidget* IMedComInventoryUIBridgeWidget::GetInventoryUIBridge(const UObject* WorldContext)
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

TScriptInterface<IMedComInventoryUIBridgeWidget> IMedComInventoryUIBridgeWidget::GetGlobalBridge(const UObject* WorldContext)
{
    TScriptInterface<IMedComInventoryUIBridgeWidget> Result;
    
    // First check global static
    if (GInventoryUIBridge.IsValid())
    {
        UObject* BridgeObject = GInventoryUIBridge.GetObject();
        IMedComInventoryUIBridgeWidget* BridgeInterface = GInventoryUIBridge.Get();
        
        if (BridgeObject && BridgeInterface)
        {
            Result.SetObject(BridgeObject);
            Result.SetInterface(BridgeInterface);
            return Result;
        }
    }
    
    // Fallback to raw pointer search and convert
    IMedComInventoryUIBridgeWidget* RawBridge = GetInventoryUIBridge(WorldContext);
    if (RawBridge)
    {
        return MakeScriptInterface(RawBridge);
    }
    
    return Result; // Returns empty/invalid TScriptInterface
}

void IMedComInventoryUIBridgeWidget::SetGlobalBridge(IMedComInventoryUIBridgeWidget* Bridge)
{
    if (Bridge)
    {
        // TWeakInterfacePtr can be constructed from raw interface pointer
        GInventoryUIBridge = TWeakInterfacePtr<IMedComInventoryUIBridgeWidget>(Bridge);
        
        UE_LOG(LogTemp, Log, TEXT("[IMedComInventoryUIBridgeWidget] Global bridge set successfully"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[IMedComInventoryUIBridgeWidget] Attempted to set null bridge"));
    }
}

void IMedComInventoryUIBridgeWidget::ClearGlobalBridge()
{
    GInventoryUIBridge.Reset();
    UE_LOG(LogTemp, Log, TEXT("[IMedComInventoryUIBridgeWidget] Global bridge cleared"));
}

TScriptInterface<IMedComInventoryUIBridgeWidget> IMedComInventoryUIBridgeWidget::MakeScriptInterface(IMedComInventoryUIBridgeWidget* RawInterface)
{
    TScriptInterface<IMedComInventoryUIBridgeWidget> Result;
    
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
            UE_LOG(LogTemp, Error, TEXT("[IMedComInventoryUIBridgeWidget] Failed to cast interface to UObject"));
        }
    }
    
    return Result;
}