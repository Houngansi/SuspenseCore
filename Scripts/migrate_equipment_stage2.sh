#!/bin/bash

# MedComEquipment → EquipmentSystem Migration - STAGE 2 (Core Components)
# Date: 2025-11-25
# Stage 2: Core Components + Root Components (24 files)

set -e

echo "Starting MedComEquipment → EquipmentSystem Stage 2 migration..."
echo "Migrating: Core Components + Root Components"

# Source and destination
SRC_DIR="Source/EquipmentSystem/MedComEquipment"
DST_DIR="Source/EquipmentSystem"

# Create directory structure for Stage 2
mkdir -p "$DST_DIR/Private/Components/Core"
mkdir -p "$DST_DIR/Public/Components/Core"

# Function to migrate and transform file
migrate() {
    local src_file="$1"
    local dst_file="$2"

    echo "Migrating: $(basename $src_file)"

    cp "$src_file" "$dst_file"

    # Module name changes
    sed -i 's/MedComEquipment\.h/EquipmentSystem.h/g' "$dst_file"
    sed -i 's/MedComEquipment\.cpp/EquipmentSystem.cpp/g' "$dst_file"
    sed -i 's/FMedComEquipmentModule/FEquipmentSystemModule/g' "$dst_file"
    sed -i 's/MEDCOMEQUIPMENT_API/EQUIPMENTSYSTEM_API/g' "$dst_file"

    # Update module dependencies
    sed -i 's/"MedComShared"/"BridgeSystem"/g' "$dst_file"
    sed -i 's/"MedComGAS"/"GAS"/g' "$dst_file"
    sed -i 's/MedComShared\//BridgeSystem\//g' "$dst_file"
    sed -i 's/MedComGAS\//GAS\//g' "$dst_file"

    # Core Component classes (MedCom prefix removal)
    sed -i 's/\bUMedComEquipmentDataStore\b/USuspenseEquipmentDataStore/g' "$dst_file"
    sed -i 's/\bUMedComEquipmentInventoryBridge\b/USuspenseEquipmentInventoryBridge/g' "$dst_file"
    sed -i 's/\bUMedComEquipmentOperationExecutor\b/USuspenseEquipmentOperationExecutor/g' "$dst_file"
    sed -i 's/\bUMedComSystemCoordinator\b/USuspenseSystemCoordinatorComponent/g' "$dst_file"
    sed -i 's/\bUMedComWeaponStateManager\b/USuspenseWeaponStateManager/g' "$dst_file"

    # Root Component classes (add Suspense prefix)
    sed -i 's/\bUEquipmentComponentBase\b/USuspenseEquipmentComponentBase/g' "$dst_file"
    sed -i 's/\bUEquipmentAttachmentComponent\b/USuspenseEquipmentAttachmentComponent/g' "$dst_file"
    sed -i 's/\bUEquipmentAttributeComponent\b/USuspenseEquipmentAttributeComponent/g' "$dst_file"
    sed -i 's/\bUEquipmentMeshComponent\b/USuspenseEquipmentMeshComponent/g' "$dst_file"
    sed -i 's/\bUMedComWeaponStanceComponent\b/USuspenseWeaponStanceComponent/g' "$dst_file"
    sed -i 's/\bUWeaponAmmoComponent\b/USuspenseWeaponAmmoComponent/g' "$dst_file"
    sed -i 's/\bUWeaponFireModeComponent\b/USuspenseWeaponFireModeComponent/g' "$dst_file"

    # Service locator
    sed -i 's/\bUEquipmentServiceLocator\b/USuspenseEquipmentServiceLocator/g' "$dst_file"

    # Structs
    sed -i 's/\bFEquipmentSlotState\b/FSuspenseEquipmentSlotState/g' "$dst_file"
    sed -i 's/\bFEquipmentOperationQueue\b/FSuspenseEquipmentOperationQueue/g' "$dst_file"
    sed -i 's/\bFWeaponAmmoData\b/FSuspenseWeaponAmmoData/g' "$dst_file"
    sed -i 's/\bFWeaponFireModeData\b/FSuspenseWeaponFireModeData/g' "$dst_file"
    sed -i 's/\bFEquipmentAttachmentData\b/FSuspenseEquipmentAttachmentData/g' "$dst_file"
    sed -i 's/\bFPendingEventData\b/FSuspensePendingEventData/g' "$dst_file"

    # Enums
    sed -i 's/\bEEquipmentSlotType\b/ESuspenseEquipmentSlotType/g' "$dst_file"
    sed -i 's/\bEWeaponStance\b/ESuspenseWeaponStance/g' "$dst_file"

    # File includes - Core Components
    sed -i 's/MedComEquipmentDataStore\.h/SuspenseEquipmentDataStore.h/g' "$dst_file"
    sed -i 's/MedComEquipmentInventoryBridge\.h/SuspenseEquipmentInventoryBridge.h/g' "$dst_file"
    sed -i 's/MedComEquipmentOperationExecutor\.h/SuspenseEquipmentOperationExecutor.h/g' "$dst_file"
    sed -i 's/MedComSystemCoordinator\.h/SuspenseSystemCoordinatorComponent.h/g' "$dst_file"
    sed -i 's/MedComWeaponStateManager\.h/SuspenseWeaponStateManager.h/g' "$dst_file"

    # File includes - Root Components
    sed -i 's/EquipmentComponentBase\.h/SuspenseEquipmentComponentBase.h/g' "$dst_file"
    sed -i 's/EquipmentAttachmentComponent\.h/SuspenseEquipmentAttachmentComponent.h/g' "$dst_file"
    sed -i 's/EquipmentAttributeComponent\.h/SuspenseEquipmentAttributeComponent.h/g' "$dst_file"
    sed -i 's/EquipmentMeshComponent\.h/SuspenseEquipmentMeshComponent.h/g' "$dst_file"
    sed -i 's/MedComWeaponStanceComponent\.h/SuspenseWeaponStanceComponent.h/g' "$dst_file"
    sed -i 's/WeaponAmmoComponent\.h/SuspenseWeaponAmmoComponent.h/g' "$dst_file"
    sed -i 's/WeaponFireModeComponent\.h/SuspenseWeaponFireModeComponent.h/g' "$dst_file"

    # .generated.h includes - Core Components
    sed -i 's/MedComEquipmentDataStore\.generated\.h/SuspenseEquipmentDataStore.generated.h/g' "$dst_file"
    sed -i 's/MedComEquipmentInventoryBridge\.generated\.h/SuspenseEquipmentInventoryBridge.generated.h/g' "$dst_file"
    sed -i 's/MedComEquipmentOperationExecutor\.generated\.h/SuspenseEquipmentOperationExecutor.generated.h/g' "$dst_file"
    sed -i 's/MedComSystemCoordinator\.generated\.h/SuspenseSystemCoordinatorComponent.generated.h/g' "$dst_file"
    sed -i 's/MedComWeaponStateManager\.generated\.h/SuspenseWeaponStateManager.generated.h/g' "$dst_file"

    # .generated.h includes - Root Components
    sed -i 's/EquipmentComponentBase\.generated\.h/SuspenseEquipmentComponentBase.generated.h/g' "$dst_file"
    sed -i 's/EquipmentAttachmentComponent\.generated\.h/SuspenseEquipmentAttachmentComponent.generated.h/g' "$dst_file"
    sed -i 's/EquipmentAttributeComponent\.generated\.h/SuspenseEquipmentAttributeComponent.generated.h/g' "$dst_file"
    sed -i 's/EquipmentMeshComponent\.generated\.h/SuspenseEquipmentMeshComponent.generated.h/g' "$dst_file"
    sed -i 's/MedComWeaponStanceComponent\.generated\.h/SuspenseWeaponStanceComponent.generated.h/g' "$dst_file"
    sed -i 's/WeaponAmmoComponent\.generated\.h/SuspenseWeaponAmmoComponent.generated.h/g' "$dst_file"
    sed -i 's/WeaponFireModeComponent\.generated\.h/SuspenseWeaponFireModeComponent.generated.h/g' "$dst_file"

    # Blueprint categories
    sed -i 's/Category="MedCom|Equipment/Category="SuspenseCore|Equipment/g' "$dst_file"
    sed -i 's/Category = "MedCom|Equipment/Category = "SuspenseCore|Equipment/g' "$dst_file"
    sed -i 's/Category="Equipment/Category="SuspenseCore|Equipment/g' "$dst_file"

    # Copyright
    sed -i 's/Copyright MedCom Team/Copyright Suspense Team/g' "$dst_file"

    # Comments (Russian)
    sed -i 's/для MedCom/для Suspense/g' "$dst_file"
    sed -i 's/проекта MedCom/проекта Suspense/g' "$dst_file"
}

# ============================================================
# STAGE 2: Core Components + Root Components
# ============================================================

echo ""
echo "=== Stage 2: Core & Root Component Files ==="
echo ""

# Core Components
echo "Migrating Core Components..."
migrate "$SRC_DIR/Public/Components/Core/MedComEquipmentDataStore.h" "$DST_DIR/Public/Components/Core/SuspenseEquipmentDataStore.h"
migrate "$SRC_DIR/Private/Components/Core/MedComEquipmentDataStore.cpp" "$DST_DIR/Private/Components/Core/SuspenseEquipmentDataStore.cpp"
migrate "$SRC_DIR/Public/Components/Core/MedComEquipmentInventoryBridge.h" "$DST_DIR/Public/Components/Core/SuspenseEquipmentInventoryBridge.h"
migrate "$SRC_DIR/Private/Components/Core/MedComEquipmentInventoryBridge.cpp" "$DST_DIR/Private/Components/Core/SuspenseEquipmentInventoryBridge.cpp"
migrate "$SRC_DIR/Public/Components/Core/MedComEquipmentOperationExecutor.h" "$DST_DIR/Public/Components/Core/SuspenseEquipmentOperationExecutor.h"
migrate "$SRC_DIR/Private/Components/Core/MedComEquipmentOperationExecutor.cpp" "$DST_DIR/Private/Components/Core/SuspenseEquipmentOperationExecutor.cpp"
migrate "$SRC_DIR/Public/Components/Core/MedComSystemCoordinator.h" "$DST_DIR/Public/Components/Core/SuspenseSystemCoordinatorComponent.h"
migrate "$SRC_DIR/Private/Components/Core/MedComSystemCoordinator.cpp" "$DST_DIR/Private/Components/Core/SuspenseSystemCoordinatorComponent.cpp"
migrate "$SRC_DIR/Public/Components/Core/MedComWeaponStateManager.h" "$DST_DIR/Public/Components/Core/SuspenseWeaponStateManager.h"
migrate "$SRC_DIR/Private/Components/Core/MedComWeaponStateManager.cpp" "$DST_DIR/Private/Components/Core/SuspenseWeaponStateManager.cpp"

echo ""
echo "Migrating Root Components..."
# Root Components
migrate "$SRC_DIR/Public/Components/EquipmentComponentBase.h" "$DST_DIR/Public/Components/SuspenseEquipmentComponentBase.h"
migrate "$SRC_DIR/Private/Components/EquipmentComponentBase.cpp" "$DST_DIR/Private/Components/SuspenseEquipmentComponentBase.cpp"
migrate "$SRC_DIR/Public/Components/EquipmentAttachmentComponent.h" "$DST_DIR/Public/Components/SuspenseEquipmentAttachmentComponent.h"
migrate "$SRC_DIR/Private/Components/EquipmentAttachmentComponent.cpp" "$DST_DIR/Private/Components/SuspenseEquipmentAttachmentComponent.cpp"
migrate "$SRC_DIR/Public/Components/EquipmentAttributeComponent.h" "$DST_DIR/Public/Components/SuspenseEquipmentAttributeComponent.h"
migrate "$SRC_DIR/Private/Components/EquipmentAttributeComponent.cpp" "$DST_DIR/Private/Components/SuspenseEquipmentAttributeComponent.cpp"
migrate "$SRC_DIR/Public/Components/EquipmentMeshComponent.h" "$DST_DIR/Public/Components/SuspenseEquipmentMeshComponent.h"
migrate "$SRC_DIR/Private/Components/EquipmentMeshComponent.cpp" "$DST_DIR/Private/Components/SuspenseEquipmentMeshComponent.cpp"
migrate "$SRC_DIR/Public/Components/MedComWeaponStanceComponent.h" "$DST_DIR/Public/Components/SuspenseWeaponStanceComponent.h"
migrate "$SRC_DIR/Private/Components/MedComWeaponStanceComponent.cpp" "$DST_DIR/Private/Components/SuspenseWeaponStanceComponent.cpp"
migrate "$SRC_DIR/Public/Components/WeaponAmmoComponent.h" "$DST_DIR/Public/Components/SuspenseWeaponAmmoComponent.h"
migrate "$SRC_DIR/Private/Components/WeaponAmmoComponent.cpp" "$DST_DIR/Private/Components/SuspenseWeaponAmmoComponent.cpp"
migrate "$SRC_DIR/Public/Components/WeaponFireModeComponent.h" "$DST_DIR/Public/Components/SuspenseWeaponFireModeComponent.h"
migrate "$SRC_DIR/Private/Components/WeaponFireModeComponent.cpp" "$DST_DIR/Private/Components/SuspenseWeaponFireModeComponent.cpp"

echo ""
echo "✅ Stage 2 Migration complete!"
echo "Files migrated: 24 files (12 headers + 12 cpp)"
echo ""
echo "Next steps:"
echo "1. Run fixup script for BridgeSystem references"
echo "2. Verify compilation"
echo "3. Proceed to Stage 3 (Rules + Validation)"
