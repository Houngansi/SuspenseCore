// Copyright Suspense Team. All Rights Reserved.

#include "Interfaces/UI/ISuspenseEquipmentUIBridge.h"

// Global static variable for bridge instance
static TWeakInterfacePtr<ISuspenseEquipmentUIBridge> GEquipmentUIBridge;

ISuspenseEquipmentUIBridge* ISuspenseEquipmentUIBridge::GetEquipmentUIBridge(const UObject* WorldContext)
{
	if (GEquipmentUIBridge.IsValid())
	{
		return GEquipmentUIBridge.Get();
	}
    
	return nullptr;
}

void ISuspenseEquipmentUIBridge::SetGlobalEquipmentBridge(ISuspenseEquipmentUIBridge* Bridge)
{
	if (Bridge)
	{
		GEquipmentUIBridge = TWeakInterfacePtr<ISuspenseEquipmentUIBridge>(Bridge);
	}
}

void ISuspenseEquipmentUIBridge::ClearGlobalEquipmentBridge()
{
	GEquipmentUIBridge.Reset();
}