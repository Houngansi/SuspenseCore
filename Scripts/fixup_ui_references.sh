#!/bin/bash

# fixup_ui_references.sh - Fix all MedCom references in UISystem
# Copyright Suspense Team. All Rights Reserved.

set -e

DST_DIR="Source/UISystem"

echo "🔧 Fixing BridgeSystem/Equipment/Inventory references in UISystem..."
echo "===================================================================="

fixup() {
    local file="$1"

    # ===== BridgeSystem Interfaces =====
    sed -i 's/\bIMedComInventoryInterface\b/ISuspenseInventoryInterface/g' "$file"
    sed -i 's/\bIMedComItemDataProvider\b/ISuspenseItemDataProvider/g' "$file"
    sed -i 's/\bIMedComEventDispatcher\b/ISuspenseEventDispatcher/g' "$file"
    sed -i 's/\bIMedComControllerInterface\b/ISuspenseControllerInterface/g' "$file"
    sed -i 's/\bIMedComLoadoutInterface\b/ISuspenseLoadoutInterface/g' "$file"
    sed -i 's/\bIMedComWeaponInterface\b/ISuspenseWeaponInterface/g' "$file"

    # ===== BridgeSystem Classes =====
    sed -i 's/\bUMedComItemManager\b/USuspenseCoreItemManager/g' "$file"
    sed -i 's/\bUMedComLoadoutManager\b/USuspenseLoadoutManager/g' "$file"

    # ===== BridgeSystem Structs =====
    sed -i 's/\bFMedComUnifiedItemData\b/FSuspenseCoreUnifiedItemData/g' "$file"
    sed -i 's/\bFInventoryItemInstance\b/FSuspenseInventoryItemInstance/g' "$file"

    # ===== EquipmentSystem Interfaces =====
    sed -i 's/\bIMedComEquipmentDataProvider\b/ISuspenseEquipmentDataProvider/g' "$file"
    sed -i 's/\bIMedComEquipmentInterface\b/ISuspenseEquipmentInterface/g' "$file"
    sed -i 's/\bIMedComEquipmentOperations\b/ISuspenseEquipmentOperations/g' "$file"

    # ===== UISystem Internal Classes (widget references) =====
    sed -i 's/\bUMedComBaseWidget\b/USuspenseBaseWidget/g' "$file"
    sed -i 's/\bUMedComBaseSlotWidget\b/USuspenseBaseSlotWidget/g' "$file"
    sed -i 's/\bUMedComBaseContainerWidget\b/USuspenseBaseContainerWidget/g' "$file"
    sed -i 's/\bUMedComBaseLayoutWidget\b/USuspenseBaseLayoutWidget/g' "$file"
    sed -i 's/\bUMedComHorizontalLayoutWidget\b/USuspenseHorizontalLayoutWidget/g' "$file"
    sed -i 's/\bUMedComInventoryWidget\b/USuspenseInventoryWidget/g' "$file"
    sed -i 's/\bUMedComInventorySlotWidget\b/USuspenseInventorySlotWidget/g' "$file"
    sed -i 's/\bUMedComEquipmentSlotWidget\b/USuspenseEquipmentSlotWidget/g' "$file"
    sed -i 's/\bUMedComEquipmentContainerWidget\b/USuspenseEquipmentContainerWidget/g' "$file"
    sed -i 's/\bUMedComItemTooltipWidget\b/USuspenseItemTooltipWidget/g' "$file"
    sed -i 's/\bUMedComDragDropOperation\b/USuspenseDragDropOperation/g' "$file"
    sed -i 's/\bUMedComDragVisualWidget\b/USuspenseDragVisualWidget/g' "$file"
    sed -i 's/\bUMedComCrosshairWidget\b/USuspenseCrosshairWidget/g' "$file"
    sed -i 's/\bUMedComHealthStaminaWidget\b/USuspenseHealthStaminaWidget/g' "$file"
    sed -i 's/\bUMedComMainHUDWidget\b/USuspenseMainHUDWidget/g' "$file"
    sed -i 's/\bUMedComWeaponUIWidget\b/USuspenseWeaponUIWidget/g' "$file"
    sed -i 's/\bUMedComCharacterScreen\b/USuspenseCharacterScreen/g' "$file"
    sed -i 's/\bUMedComUpperTabBar\b/USuspenseUpperTabBar/g' "$file"

    # ===== UISystem Components =====
    sed -i 's/\bUMedComUIManager\b/USuspenseUIManager/g' "$file"
    sed -i 's/\bUMedComEquipmentUIBridge\b/USuspenseEquipmentUIBridge/g' "$file"
    sed -i 's/\bUMedComInventoryUIBridge\b/USuspenseInventoryUIBridge/g' "$file"
    sed -i 's/\bUMedComDragDropHandler\b/USuspenseDragDropHandler/g' "$file"
    sed -i 's/\bUMedComTooltipManager\b/USuspenseTooltipManager/g' "$file"

    # ===== UI-specific Interfaces =====
    sed -i 's/\bIMedComEquipmentUIBridgeWidget\b/ISuspenseEquipmentUIBridgeWidget/g' "$file"
    sed -i 's/\bIMedComEquipmentUIPort\b/ISuspenseEquipmentUIPort/g' "$file"
    sed -i 's/\bIMedComInventoryUIBridgeWidget\b/ISuspenseInventoryUIBridgeWidget/g' "$file"
    sed -i 's/\bIMedComInventoryUIPort\b/ISuspenseInventoryUIPort/g' "$file"
    sed -i 's/\bIMedComUIWidget\b/ISuspenseUIWidget/g' "$file"
    sed -i 's/\bIMedComUIWidgetInterface\b/ISuspenseUIWidgetInterface/g' "$file"
    sed -i 's/\bIMedComDraggableInterface\b/ISuspenseDraggableInterface/g' "$file"
    sed -i 's/\bIMedComDropTargetInterface\b/ISuspenseDropTargetInterface/g' "$file"
    sed -i 's/\bIMedComSlotUIInterface\b/ISuspenseSlotUIInterface/g' "$file"
    sed -i 's/\bIMedComContainerUIInterface\b/ISuspenseContainerUIInterface/g' "$file"
    sed -i 's/\bIMedComLayoutInterface\b/ISuspenseLayoutInterface/g' "$file"
    sed -i 's/\bIMedComTooltipInterface\b/ISuspenseTooltipInterface/g' "$file"
    sed -i 's/\bIMedComTooltipSourceInterface\b/ISuspenseTooltipSourceInterface/g' "$file"
    sed -i 's/\bIMedComCrosshairWidget\b/ISuspenseCrosshairWidget/g' "$file"
    sed -i 's/\bIMedComCrosshairWidgetInterface\b/ISuspenseCrosshairWidgetInterface/g' "$file"
    sed -i 's/\bIMedComHealthStaminaWidgetInterface\b/ISuspenseHealthStaminaWidgetInterface/g' "$file"
    sed -i 's/\bIMedComWeaponUIWidgetInterface\b/ISuspenseWeaponUIWidgetInterface/g' "$file"
    sed -i 's/\bIMedComHUDWidgetInterface\b/ISuspenseHUDWidgetInterface/g' "$file"
    sed -i 's/\bIMedComScreenInterface\b/ISuspenseScreenInterface/g' "$file"
    sed -i 's/\bIMedComTabBarInterface\b/ISuspenseTabBarInterface/g' "$file"
    sed -i 's/\bIMedComAttributeProviderInterface\b/ISuspenseAttributeProviderInterface/g' "$file"

    # ===== UI-specific Structs =====
    sed -i 's/\bFMedComWidgetInfo\b/FSuspenseWidgetInfo/g' "$file"
    sed -i 's/\bFMedComSlotWidgetStyle\b/FSuspenseSlotWidgetStyle/g' "$file"
    sed -i 's/\bFMedComDragDropPayload\b/FSuspenseDragDropPayload/g' "$file"
    sed -i 's/\bFMedComTabConfig\b/FSuspenseTabConfig/g' "$file"
    sed -i 's/\bFMedComAttributeData\b/FSuspenseAttributeData/g' "$file"
    sed -i 's/\bFMedComUIModule\b/FSuspenseUIModule/g' "$file"

    # ===== UINTERFACE Wrapper Classes (for StaticClass() and ImplementsInterface()) =====
    # BridgeSystem interfaces
    sed -i 's/\bUMedComControllerInterface\b/USuspenseControllerInterface/g' "$file"
    sed -i 's/\bUMedComLoadoutInterface\b/USuspenseLoadoutInterface/g' "$file"
    sed -i 's/\bUMedComWeaponInterface\b/USuspenseWeaponInterface/g' "$file"
    sed -i 's/\bUMedComInventoryInterface\b/USuspenseInventoryInterface/g' "$file"
    sed -i 's/\bUMedComEquipmentInterface\b/USuspenseEquipmentInterface/g' "$file"
    sed -i 's/\bUMedComEquipmentDataProvider\b/USuspenseEquipmentDataProviderInterface/g' "$file"

    # UI interfaces
    sed -i 's/\bUMedComUIWidgetInterface\b/USuspenseUIWidgetInterface/g' "$file"
    sed -i 's/\bUMedComDraggableInterface\b/USuspenseDraggableInterface/g' "$file"
    sed -i 's/\bUMedComSlotUIInterface\b/USuspenseSlotUIInterface/g' "$file"
    sed -i 's/\bUMedComContainerUIInterface\b/USuspenseContainerUIInterface/g' "$file"
    sed -i 's/\bUMedComTooltipInterface\b/USuspenseTooltipInterface/g' "$file"
    sed -i 's/\bUMedComTooltipSourceInterface\b/USuspenseTooltipSourceInterface/g' "$file"
    sed -i 's/\bUMedComHUDWidgetInterface\b/USuspenseHUDWidgetInterface/g' "$file"
    sed -i 's/\bUMedComScreenInterface\b/USuspenseScreenInterface/g' "$file"
    sed -i 's/\bUMedComAttributeProviderInterface\b/USuspenseAttributeProviderInterface/g' "$file"
    sed -i 's/\bUMedComInventoryUIBridgeWidget\b/USuspenseInventoryUIBridgeWidgetInterface/g' "$file"

    # ===== Include paths =====
    # BridgeSystem includes
    sed -i 's|Interfaces/Inventory/IMedComInventoryInterface\.h|Interfaces/Inventory/ISuspenseInventory.h|g' "$file"
    sed -i 's|ItemSystem/MedComItemManager\.h|ItemSystem/SuspenseItemManager.h|g' "$file"
    sed -i 's|Types/Inventory/FInventoryItemInstance\.h|Types/Inventory/FSuspenseInventoryItemInstance.h|g' "$file"

    # EquipmentSystem includes
    sed -i 's|Interfaces/Equipment/IMedComEquipmentInterface\.h|Interfaces/Equipment/ISuspenseEquipment.h|g' "$file"

    # UISystem internal includes - update MedCom prefix in include paths
    sed -i 's|Components/MedCom\([A-Za-z]*\)\.h|Components/Suspense\1.h|g' "$file"
    sed -i 's|Widgets/Base/MedCom\([A-Za-z]*\)\.h|Widgets/Base/Suspense\1.h|g' "$file"
    sed -i 's|Widgets/Equipment/MedCom\([A-Za-z]*\)\.h|Widgets/Equipment/Suspense\1.h|g' "$file"
    sed -i 's|Widgets/Inventory/MedCom\([A-Za-z]*\)\.h|Widgets/Inventory/Suspense\1.h|g' "$file"
    sed -i 's|Widgets/HUD/MedCom\([A-Za-z]*\)\.h|Widgets/HUD/Suspense\1.h|g' "$file"
    sed -i 's|Widgets/Layout/MedCom\([A-Za-z]*\)\.h|Widgets/Layout/Suspense\1.h|g' "$file"
    sed -i 's|Widgets/Screens/MedCom\([A-Za-z]*\)\.h|Widgets/Screens/Suspense\1.h|g' "$file"
    sed -i 's|Widgets/Tabs/MedCom\([A-Za-z]*\)\.h|Widgets/Tabs/Suspense\1.h|g' "$file"
    sed -i 's|Widgets/Tooltip/MedCom\([A-Za-z]*\)\.h|Widgets/Tooltip/Suspense\1.h|g' "$file"
    sed -i 's|Widgets/DragDrop/MedCom\([A-Za-z]*\)\.h|Widgets/DragDrop/Suspense\1.h|g' "$file"
    sed -i 's|DragDrop/MedCom\([A-Za-z]*\)\.h|DragDrop/Suspense\1.h|g' "$file"
    sed -i 's|Tooltip/MedCom\([A-Za-z]*\)\.h|Tooltip/Suspense\1.h|g' "$file"

    # GENERATED.H files - critical for UE reflection
    sed -i 's|MedComUIManager\.generated\.h|SuspenseUIManager.generated.h|g' "$file"
    sed -i 's|MedComEquipmentUIBridge\.generated\.h|SuspenseEquipmentUIBridge.generated.h|g' "$file"
    sed -i 's|MedComInventoryUIBridge\.generated\.h|SuspenseInventoryUIBridge.generated.h|g' "$file"
    sed -i 's|MedComDragDropHandler\.generated\.h|SuspenseDragDropHandler.generated.h|g' "$file"
    sed -i 's|MedComTooltipManager\.generated\.h|SuspenseTooltipManager.generated.h|g' "$file"
    sed -i 's|MedComBaseWidget\.generated\.h|SuspenseBaseWidget.generated.h|g' "$file"
    sed -i 's|MedComBaseSlotWidget\.generated\.h|SuspenseBaseSlotWidget.generated.h|g' "$file"
    sed -i 's|MedComBaseContainerWidget\.generated\.h|SuspenseBaseContainerWidget.generated.h|g' "$file"
    sed -i 's|MedComBaseLayoutWidget\.generated\.h|SuspenseBaseLayoutWidget.generated.h|g' "$file"
    sed -i 's|MedComHorizontalLayoutWidget\.generated\.h|SuspenseHorizontalLayoutWidget.generated.h|g' "$file"
    sed -i 's|MedComInventoryWidget\.generated\.h|SuspenseInventoryWidget.generated.h|g' "$file"
    sed -i 's|MedComInventorySlotWidget\.generated\.h|SuspenseInventorySlotWidget.generated.h|g' "$file"
    sed -i 's|MedComEquipmentSlotWidget\.generated\.h|SuspenseEquipmentSlotWidget.generated.h|g' "$file"
    sed -i 's|MedComEquipmentContainerWidget\.generated\.h|SuspenseEquipmentContainerWidget.generated.h|g' "$file"
    sed -i 's|MedComItemTooltipWidget\.generated\.h|SuspenseItemTooltipWidget.generated.h|g' "$file"
    sed -i 's|MedComDragDropOperation\.generated\.h|SuspenseDragDropOperation.generated.h|g' "$file"
    sed -i 's|MedComDragVisualWidget\.generated\.h|SuspenseDragVisualWidget.generated.h|g' "$file"
    sed -i 's|MedComCrosshairWidget\.generated\.h|SuspenseCrosshairWidget.generated.h|g' "$file"
    sed -i 's|MedComHealthStaminaWidget\.generated\.h|SuspenseHealthStaminaWidget.generated.h|g' "$file"
    sed -i 's|MedComMainHUDWidget\.generated\.h|SuspenseMainHUDWidget.generated.h|g' "$file"
    sed -i 's|MedComWeaponUIWidget\.generated\.h|SuspenseWeaponUIWidget.generated.h|g' "$file"
    sed -i 's|MedComCharacterScreen\.generated\.h|SuspenseCharacterScreen.generated.h|g' "$file"
    sed -i 's|MedComUpperTabBar\.generated\.h|SuspenseUpperTabBar.generated.h|g' "$file"
}

# Fix all UI files
for file in $(find "$DST_DIR/Public" "$DST_DIR/Private" -name "*.h" -o -name "*.cpp" | grep -v "MedComUI/"); do
    echo "Fixing: $(basename $file)"
    fixup "$file"
done

echo ""
echo "✅ All references fixed in UISystem!"
echo "===================================="
