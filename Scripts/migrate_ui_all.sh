#!/bin/bash

# migrate_ui_all.sh - Migrate MedComUI → UISystem (All 48 files)
# Copyright Suspense Team. All Rights Reserved.

set -e

SRC_DIR="Source/UISystem/MedComUI"
DST_DIR="Source/UISystem"

echo "🚀 Migrating MedComUI → UISystem (All components)"
echo "=================================================="

# Function to migrate a file pair (.h and .cpp)
migrate_file() {
    local subdir="$1"        # e.g., "Components/"
    local old_name="$2"      # e.g., "MedComUIManager"
    local new_name="$3"      # e.g., "SuspenseUIManager"

    local src_h="${SRC_DIR}/Public/${subdir}${old_name}.h"
    local src_cpp="${SRC_DIR}/Private/${subdir}${old_name}.cpp"

    local dst_h="${DST_DIR}/Public/${subdir}${new_name}.h"
    local dst_cpp="${DST_DIR}/Private/${subdir}${new_name}.cpp"

    # Create destination directories
    mkdir -p "$(dirname "$dst_h")"
    mkdir -p "$(dirname "$dst_cpp")"

    # Copy and transform header
    if [ -f "$src_h" ]; then
        cp "$src_h" "$dst_h"

        # Update header guard
        local old_guard="${old_name^^}_H"
        local new_guard="${new_name^^}_H"
        sed -i "s/${old_guard}/${new_guard}/g" "$dst_h"

        # Update API macro
        sed -i 's/\bMEDCOMUI_API\b/UISYSTEM_API/g' "$dst_h"

        # Rename class/struct declarations (class name patterns)
        sed -i "s/\bU${old_name}\b/U${new_name}/g" "$dst_h"
        sed -i "s/\bF${old_name}\b/F${new_name}/g" "$dst_h"

        # Update copyright
        sed -i 's/Copyright MedCom/Copyright Suspense Team/g' "$dst_h"

        echo "  ✓ ${new_name}.h"
    fi

    # Copy and transform cpp
    if [ -f "$src_cpp" ]; then
        cp "$src_cpp" "$dst_cpp"

        # Update include path
        sed -i "s|${old_name}\.h|${new_name}.h|g" "$dst_cpp"

        # Rename class references
        sed -i "s/\bU${old_name}\b/U${new_name}/g" "$dst_cpp"
        sed -i "s/\bF${old_name}\b/F${new_name}/g" "$dst_cpp"

        # Update copyright
        sed -i 's/Copyright MedCom/Copyright Suspense Team/g' "$dst_cpp"

        echo "  ✓ ${new_name}.cpp"
    fi
}

echo ""
echo "📦 Stage 1: Components (3 components = 6 files)"
echo "------------------------------------------------"

migrate_file "Components/" "MedComUIManager" "SuspenseUIManager"
migrate_file "Components/" "MedComEquipmentUIBridge" "SuspenseEquipmentUIBridge"
migrate_file "Components/" "MedComInventoryUIBridge" "SuspenseInventoryUIBridge"

echo ""
echo "📦 Stage 2: DragDrop + Tooltip (2 components = 4 files)"
echo "--------------------------------------------------------"

migrate_file "DragDrop/" "MedComDragDropHandler" "SuspenseDragDropHandler"
migrate_file "Tooltip/" "MedComTooltipManager" "SuspenseTooltipManager"

echo ""
echo "📦 Stage 3: Widgets/Base (3 widgets = 6 files)"
echo "-----------------------------------------------"

migrate_file "Widgets/Base/" "MedComBaseWidget" "SuspenseBaseWidget"
migrate_file "Widgets/Base/" "MedComBaseSlotWidget" "SuspenseBaseSlotWidget"
migrate_file "Widgets/Base/" "MedComBaseContainerWidget" "SuspenseBaseContainerWidget"

echo ""
echo "📦 Stage 4: Widgets/DragDrop (2 widgets = 4 files)"
echo "---------------------------------------------------"

migrate_file "Widgets/DragDrop/" "MedComDragDropOperation" "SuspenseDragDropOperation"
migrate_file "Widgets/DragDrop/" "MedComDragVisualWidget" "SuspenseDragVisualWidget"

echo ""
echo "📦 Stage 5: Widgets/Equipment (2 widgets = 4 files)"
echo "----------------------------------------------------"

migrate_file "Widgets/Equipment/" "MedComEquipmentSlotWidget" "SuspenseEquipmentSlotWidget"
migrate_file "Widgets/Equipment/" "MedComEquipmentContainerWidget" "SuspenseEquipmentContainerWidget"

echo ""
echo "📦 Stage 6: Widgets/HUD (4 widgets = 8 files)"
echo "----------------------------------------------"

migrate_file "Widgets/HUD/" "MedComCrosshairWidget" "SuspenseCrosshairWidget"
migrate_file "Widgets/HUD/" "MedComHealthStaminaWidget" "SuspenseHealthStaminaWidget"
migrate_file "Widgets/HUD/" "MedComMainHUDWidget" "SuspenseMainHUDWidget"
migrate_file "Widgets/HUD/" "MedComWeaponUIWidget" "SuspenseWeaponUIWidget"

echo ""
echo "📦 Stage 7: Widgets/Inventory (2 widgets = 4 files)"
echo "----------------------------------------------------"

migrate_file "Widgets/Inventory/" "MedComInventorySlotWidget" "SuspenseInventorySlotWidget"
migrate_file "Widgets/Inventory/" "MedComInventoryWidget" "SuspenseInventoryWidget"

echo ""
echo "📦 Stage 8: Widgets/Layout (2 widgets = 4 files)"
echo "-------------------------------------------------"

migrate_file "Widgets/Layout/" "MedComBaseLayoutWidget" "SuspenseBaseLayoutWidget"
migrate_file "Widgets/Layout/" "MedComHorizontalLayoutWidget" "SuspenseHorizontalLayoutWidget"

echo ""
echo "📦 Stage 9: Widgets/Screens + Tabs + Tooltip (3 widgets = 6 files)"
echo "-------------------------------------------------------------------"

migrate_file "Widgets/Screens/" "MedComCharacterScreen" "SuspenseCharacterScreen"
migrate_file "Widgets/Tabs/" "MedComUpperTabBar" "SuspenseUpperTabBar"
migrate_file "Widgets/Tooltip/" "MedComItemTooltipWidget" "SuspenseItemTooltipWidget"

echo ""
echo "✅ Migration complete! 48 files migrated to UISystem"
echo "====================================================="
