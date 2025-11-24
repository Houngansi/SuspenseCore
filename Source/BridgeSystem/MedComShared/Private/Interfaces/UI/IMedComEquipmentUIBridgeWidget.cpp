// Copyright MedCom Team. All Rights Reserved.

#include "Interfaces/UI/IMedComEquipmentUIBridgeWidget.h"

// Global static variable for bridge instance
static TWeakInterfacePtr<IMedComEquipmentUIBridgeWidget> GEquipmentUIBridge;

IMedComEquipmentUIBridgeWidget* IMedComEquipmentUIBridgeWidget::GetEquipmentUIBridge(const UObject* WorldContext)
{
	if (GEquipmentUIBridge.IsValid())
	{
		return GEquipmentUIBridge.Get();
	}
    
	return nullptr;
}

void IMedComEquipmentUIBridgeWidget::SetGlobalEquipmentBridge(IMedComEquipmentUIBridgeWidget* Bridge)
{
	if (Bridge)
	{
		GEquipmentUIBridge = TWeakInterfacePtr<IMedComEquipmentUIBridgeWidget>(Bridge);
	}
}

void IMedComEquipmentUIBridgeWidget::ClearGlobalEquipmentBridge()
{
	GEquipmentUIBridge.Reset();
}