// Copyright Suspense Team. All Rights Reserved.

#include "Interfaces/UI/ISuspenseEquipmentUIBridge.h"

// Global static variable for bridge instance
static TWeakInterfacePtr<ISuspenseEquipmentUIBridgeInterface> GEquipmentUIBridge;

ISuspenseEquipmentUIBridgeInterface* ISuspenseEquipmentUIBridgeInterface::GetEquipmentUIBridge(const UObject* WorldContext)
{
	if (GEquipmentUIBridge.IsValid())
	{
		return GEquipmentUIBridge.Get();
	}

	return nullptr;
}

void ISuspenseEquipmentUIBridgeInterface::SetGlobalEquipmentBridge(ISuspenseEquipmentUIBridgeInterface* Bridge)
{
	if (Bridge)
	{
		GEquipmentUIBridge = TWeakInterfacePtr<ISuspenseEquipmentUIBridgeInterface>(Bridge);
	}
}

void ISuspenseEquipmentUIBridgeInterface::ClearGlobalEquipmentBridge()
{
	GEquipmentUIBridge.Reset();
}