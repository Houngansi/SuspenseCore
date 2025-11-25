#!/bin/bash

# MedComEquipment → EquipmentSystem Migration - STAGE 1 (Foundation)
# Date: 2025-11-25
# Stage 1: Module, Base, Services, Subsystems (22 files)

set -e

echo "Starting MedComEquipment → EquipmentSystem Stage 1 migration..."
echo "Migrating: Module entry, Base, Services, Subsystems"

# Source and destination
SRC_DIR="Source/EquipmentSystem/MedComEquipment"
DST_DIR="Source/EquipmentSystem"

# Create directory structure for Stage 1
mkdir -p "$DST_DIR/Private/Base"
mkdir -p "$DST_DIR/Private/Services"
mkdir -p "$DST_DIR/Private/Subsystems"

mkdir -p "$DST_DIR/Public/Base"
mkdir -p "$DST_DIR/Public/Services"
mkdir -p "$DST_DIR/Public/Subsystems"

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

    # Actor classes
    sed -i 's/\bAEquipmentActorBase\b/ASuspenseEquipmentActor/g' "$dst_file"
    sed -i 's/\bAWeaponActor\b/ASuspenseWeaponActor/g' "$dst_file"

    # Component classes (Equipment-specific)
    sed -i 's/\bUEquipmentMeshComponent\b/USuspenseEquipmentMeshComponent/g' "$dst_file"
    sed -i 's/\bUEquipmentAttributeComponent\b/USuspenseEquipmentAttributeComponent/g' "$dst_file"
    sed -i 's/\bUEquipmentAttachmentComponent\b/USuspenseEquipmentAttachmentComponent/g' "$dst_file"
    sed -i 's/\bUWeaponAmmoComponent\b/USuspenseWeaponAmmoComponent/g' "$dst_file"
    sed -i 's/\bUWeaponFireModeComponent\b/USuspenseWeaponFireModeComponent/g' "$dst_file"

    # Service classes
    sed -i 's/\bUEquipmentDataServiceImpl\b/USuspenseEquipmentDataService/g' "$dst_file"
    sed -i 's/\bUEquipmentNetworkServiceImpl\b/USuspenseEquipmentNetworkService/g' "$dst_file"
    sed -i 's/\bUEquipmentOperationServiceImpl\b/USuspenseEquipmentOperationService/g' "$dst_file"
    sed -i 's/\bUEquipmentValidationServiceImpl\b/USuspenseEquipmentValidationService/g' "$dst_file"
    sed -i 's/\bUEquipmentVisualizationServiceImpl\b/USuspenseEquipmentVisualizationService/g' "$dst_file"
    sed -i 's/\bUEquipmentAbilityServiceImpl\b/USuspenseEquipmentAbilityService/g' "$dst_file"

    # Subsystem classes
    sed -i 's/\bUMedComSystemCoordinatorSubsystem\b/USuspenseSystemCoordinator/g' "$dst_file"

    # Structs (Equipment-specific)
    sed -i 's/\bFEquipmentSlotData\b/FSuspenseEquipmentSlotData/g' "$dst_file"
    sed -i 's/\bFEquipmentOperationContext\b/FSuspenseEquipmentOperationContext/g' "$dst_file"
    sed -i 's/\bFEquipmentNetworkData\b/FSuspenseEquipmentNetworkData/g' "$dst_file"
    sed -i 's/\bFEquipmentValidationResult\b/FSuspenseEquipmentValidationResult/g' "$dst_file"
    sed -i 's/\bFEquipmentVisualData\b/FSuspenseEquipmentVisualData/g' "$dst_file"

    # Thread guard and utilities
    sed -i 's/\bFEquipmentThreadGuard\b/FSuspenseEquipmentThreadGuard/g' "$dst_file"
    sed -i 's/\bFEquipmentReadWriteLock\b/FSuspenseEquipmentReadWriteLock/g' "$dst_file"

    # Log categories
    sed -i 's/LogEquipment/LogSuspenseEquipment/g' "$dst_file"

    # File includes - Actor files
    sed -i 's/EquipmentActorBase\.h/SuspenseEquipmentActor.h/g' "$dst_file"
    sed -i 's/WeaponActor\.h/SuspenseWeaponActor.h/g' "$dst_file"

    # File includes - Component files
    sed -i 's/EquipmentMeshComponent\.h/SuspenseEquipmentMeshComponent.h/g' "$dst_file"
    sed -i 's/EquipmentAttributeComponent\.h/SuspenseEquipmentAttributeComponent.h/g' "$dst_file"
    sed -i 's/EquipmentAttachmentComponent\.h/SuspenseEquipmentAttachmentComponent.h/g' "$dst_file"
    sed -i 's/WeaponAmmoComponent\.h/SuspenseWeaponAmmoComponent.h/g' "$dst_file"
    sed -i 's/WeaponFireModeComponent\.h/SuspenseWeaponFireModeComponent.h/g' "$dst_file"

    # File includes - Service files
    sed -i 's/EquipmentDataServiceImpl\.h/SuspenseEquipmentDataService.h/g' "$dst_file"
    sed -i 's/EquipmentNetworkServiceImpl\.h/SuspenseEquipmentNetworkService.h/g' "$dst_file"
    sed -i 's/EquipmentOperationServiceImpl\.h/SuspenseEquipmentOperationService.h/g' "$dst_file"
    sed -i 's/EquipmentValidationServiceImpl\.h/SuspenseEquipmentValidationService.h/g' "$dst_file"
    sed -i 's/EquipmentVisualizationServiceImpl\.h/SuspenseEquipmentVisualizationService.h/g' "$dst_file"
    sed -i 's/EquipmentAbilityServiceImpl\.h/SuspenseEquipmentAbilityService.h/g' "$dst_file"
    sed -i 's/EquipmentServiceMacros\.h/SuspenseEquipmentServiceMacros.h/g' "$dst_file"

    # File includes - Subsystem files
    sed -i 's/MedComSystemCoordinatorSubsystem\.h/SuspenseSystemCoordinator.h/g' "$dst_file"

    # File includes - Thread guard
    sed -i 's/FEquipmentThreadGuard\.h/FSuspenseEquipmentThreadGuard.h/g' "$dst_file"

    # .generated.h includes - Actors
    sed -i 's/EquipmentActorBase\.generated\.h/SuspenseEquipmentActor.generated.h/g' "$dst_file"
    sed -i 's/WeaponActor\.generated\.h/SuspenseWeaponActor.generated.h/g' "$dst_file"

    # .generated.h includes - Services
    sed -i 's/EquipmentDataServiceImpl\.generated\.h/SuspenseEquipmentDataService.generated.h/g' "$dst_file"
    sed -i 's/EquipmentNetworkServiceImpl\.generated\.h/SuspenseEquipmentNetworkService.generated.h/g' "$dst_file"
    sed -i 's/EquipmentOperationServiceImpl\.generated\.h/SuspenseEquipmentOperationService.generated.h/g' "$dst_file"
    sed -i 's/EquipmentValidationServiceImpl\.generated\.h/SuspenseEquipmentValidationService.generated.h/g' "$dst_file"
    sed -i 's/EquipmentVisualizationServiceImpl\.generated\.h/SuspenseEquipmentVisualizationService.generated.h/g' "$dst_file"
    sed -i 's/EquipmentAbilityServiceImpl\.generated\.h/SuspenseEquipmentAbilityService.generated.h/g' "$dst_file"

    # .generated.h includes - Subsystems
    sed -i 's/MedComSystemCoordinatorSubsystem\.generated\.h/SuspenseSystemCoordinator.generated.h/g' "$dst_file"

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
# STAGE 1: Module, Base, Services, Subsystems
# ============================================================

echo ""
echo "=== Stage 1: Foundation Files ==="
echo ""

# Module entry
migrate "$SRC_DIR/Public/MedComEquipment.h" "$DST_DIR/Public/EquipmentSystem_Module.h"
migrate "$SRC_DIR/Private/MedComEquipment.cpp" "$DST_DIR/Private/EquipmentSystem_Module.cpp"

# Base files
migrate "$SRC_DIR/Public/Base/EquipmentActorBase.h" "$DST_DIR/Public/Base/SuspenseEquipmentActor.h"
migrate "$SRC_DIR/Private/Base/EquipmentActorBase.cpp" "$DST_DIR/Private/Base/SuspenseEquipmentActor.cpp"
migrate "$SRC_DIR/Public/Base/WeaponActor.h" "$DST_DIR/Public/Base/SuspenseWeaponActor.h"
migrate "$SRC_DIR/Private/Base/WeaponActor.cpp" "$DST_DIR/Private/Base/SuspenseWeaponActor.cpp"

# Services files
migrate "$SRC_DIR/Public/Services/EquipmentServiceMacros.h" "$DST_DIR/Public/Services/SuspenseEquipmentServiceMacros.h"
migrate "$SRC_DIR/Public/Services/EquipmentDataServiceImpl.h" "$DST_DIR/Public/Services/SuspenseEquipmentDataService.h"
migrate "$SRC_DIR/Private/Services/EquipmentDataServiceImpl.cpp" "$DST_DIR/Private/Services/SuspenseEquipmentDataService.cpp"
migrate "$SRC_DIR/Public/Services/EquipmentNetworkServiceImpl.h" "$DST_DIR/Public/Services/SuspenseEquipmentNetworkService.h"
migrate "$SRC_DIR/Private/Services/EquipmentNetworkServiceImpl.cpp" "$DST_DIR/Private/Services/SuspenseEquipmentNetworkService.cpp"
migrate "$SRC_DIR/Public/Services/EquipmentOperationServiceImpl.h" "$DST_DIR/Public/Services/SuspenseEquipmentOperationService.h"
migrate "$SRC_DIR/Private/Services/EquipmentOperationServiceImpl.cpp" "$DST_DIR/Private/Services/SuspenseEquipmentOperationService.cpp"
migrate "$SRC_DIR/Public/Services/EquipmentValidationServiceImpl.h" "$DST_DIR/Public/Services/SuspenseEquipmentValidationService.h"
migrate "$SRC_DIR/Private/Services/EquipmentValidationServiceImpl.cpp" "$DST_DIR/Private/Services/SuspenseEquipmentValidationService.cpp"
migrate "$SRC_DIR/Public/Services/EquipmentVisualizationServiceImpl.h" "$DST_DIR/Public/Services/SuspenseEquipmentVisualizationService.h"
migrate "$SRC_DIR/Private/Services/EquipmentVisualizationServiceImpl.cpp" "$DST_DIR/Private/Services/SuspenseEquipmentVisualizationService.cpp"
migrate "$SRC_DIR/Public/Services/EquipmentAbilityServiceImpl.h" "$DST_DIR/Public/Services/SuspenseEquipmentAbilityService.h"
migrate "$SRC_DIR/Private/Services/EquipmentAbilityServiceImpl.cpp" "$DST_DIR/Private/Services/SuspenseEquipmentAbilityService.cpp"

# Subsystems
migrate "$SRC_DIR/Public/Subsystems/MedComSystemCoordinatorSubsystem.h" "$DST_DIR/Public/Subsystems/SuspenseSystemCoordinator.h"
migrate "$SRC_DIR/Private/Subsystems/MedComSystemCoordinatorSubsystem.cpp" "$DST_DIR/Private/Subsystems/SuspenseSystemCoordinator.cpp"

echo ""
echo "✅ Stage 1 Migration complete!"
echo "Files migrated: 22 files (11 headers + 11 cpp)"
echo ""
echo "Next steps:"
echo "1. Update EquipmentSystem.Build.cs dependencies"
echo "2. Verify compilation"
echo "3. Proceed to Stage 2 (Core Components)"
