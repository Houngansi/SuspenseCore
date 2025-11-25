#!/bin/bash

# MedComEquipment → EquipmentSystem Migration - STAGE 4 (Integration + Network + Presentation)
# Date: 2025-11-25
# Stage 4: Integration, Network, Presentation (16 files)

set -e

echo "Starting MedComEquipment → EquipmentSystem Stage 4 migration..."
echo "Migrating: Integration + Network + Presentation"

# Source and destination
SRC_DIR="Source/EquipmentSystem/MedComEquipment"
DST_DIR="Source/EquipmentSystem"

# Create directory structure for Stage 4
mkdir -p "$DST_DIR/Private/Components/Integration"
mkdir -p "$DST_DIR/Private/Components/Network"
mkdir -p "$DST_DIR/Private/Components/Presentation"
mkdir -p "$DST_DIR/Public/Components/Integration"
mkdir -p "$DST_DIR/Public/Components/Network"
mkdir -p "$DST_DIR/Public/Components/Presentation"

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

    # Integration classes
    sed -i 's/\bUMedComEquipmentAbilityConnector\b/USuspenseEquipmentAbilityConnector/g' "$dst_file"
    sed -i 's/\bUMedComEquipmentLoadoutAdapter\b/USuspenseEquipmentLoadoutAdapter/g' "$dst_file"

    # Network classes (already referenced in Stage 1)
    sed -i 's/\bUMedComEquipmentNetworkDispatcher\b/USuspenseEquipmentNetworkDispatcher/g' "$dst_file"
    sed -i 's/\bUMedComEquipmentPredictionSystem\b/USuspenseEquipmentPredictionSystem/g' "$dst_file"
    sed -i 's/\bUMedComEquipmentReplicationManager\b/USuspenseEquipmentReplicationManager/g' "$dst_file"

    # Presentation classes
    sed -i 's/\bUMedComEquipmentActorFactory\b/USuspenseEquipmentActorFactory/g' "$dst_file"
    sed -i 's/\bUMedComEquipmentAttachmentSystem\b/USuspenseEquipmentAttachmentSystem/g' "$dst_file"
    sed -i 's/\bUMedComEquipmentVisualController\b/USuspenseEquipmentVisualController/g' "$dst_file"

    # Structs
    sed -i 's/\bFAbilityConnectorData\b/FSuspenseAbilityConnectorData/g' "$dst_file"
    sed -i 's/\bFLoadoutAdapterData\b/FSuspenseLoadoutAdapterData/g' "$dst_file"
    sed -i 's/\bFNetworkDispatchData\b/FSuspenseNetworkDispatchData/g' "$dst_file"
    sed -i 's/\bFPredictionData\b/FSuspensePredictionData/g' "$dst_file"
    sed -i 's/\bFReplicationData\b/FSuspenseReplicationData/g' "$dst_file"
    sed -i 's/\bFActorFactoryData\b/FSuspenseActorFactoryData/g' "$dst_file"
    sed -i 's/\bFAttachmentSystemData\b/FSuspenseAttachmentSystemData/g' "$dst_file"
    sed -i 's/\bFVisualControllerData\b/FSuspenseVisualControllerData/g' "$dst_file"

    # File includes - Integration
    sed -i 's/MedComEquipmentAbilityConnector\.h/SuspenseEquipmentAbilityConnector.h/g' "$dst_file"
    sed -i 's/MedComEquipmentLoadoutAdapter\.h/SuspenseEquipmentLoadoutAdapter.h/g' "$dst_file"

    # File includes - Network
    sed -i 's/MedComEquipmentNetworkDispatcher\.h/SuspenseEquipmentNetworkDispatcher.h/g' "$dst_file"
    sed -i 's/MedComEquipmentPredictionSystem\.h/SuspenseEquipmentPredictionSystem.h/g' "$dst_file"
    sed -i 's/MedComEquipmentReplicationManager\.h/SuspenseEquipmentReplicationManager.h/g' "$dst_file"

    # File includes - Presentation
    sed -i 's/MedComEquipmentActorFactory\.h/SuspenseEquipmentActorFactory.h/g' "$dst_file"
    sed -i 's/MedComEquipmentAttachmentSystem\.h/SuspenseEquipmentAttachmentSystem.h/g' "$dst_file"
    sed -i 's/MedComEquipmentVisualController\.h/SuspenseEquipmentVisualController.h/g' "$dst_file"

    # .generated.h includes - Integration
    sed -i 's/MedComEquipmentAbilityConnector\.generated\.h/SuspenseEquipmentAbilityConnector.generated.h/g' "$dst_file"
    sed -i 's/MedComEquipmentLoadoutAdapter\.generated\.h/SuspenseEquipmentLoadoutAdapter.generated.h/g' "$dst_file"

    # .generated.h includes - Network
    sed -i 's/MedComEquipmentNetworkDispatcher\.generated\.h/SuspenseEquipmentNetworkDispatcher.generated.h/g' "$dst_file"
    sed -i 's/MedComEquipmentPredictionSystem\.generated\.h/SuspenseEquipmentPredictionSystem.generated.h/g' "$dst_file"
    sed -i 's/MedComEquipmentReplicationManager\.generated\.h/SuspenseEquipmentReplicationManager.generated.h/g' "$dst_file"

    # .generated.h includes - Presentation
    sed -i 's/MedComEquipmentActorFactory\.generated\.h/SuspenseEquipmentActorFactory.generated.h/g' "$dst_file"
    sed -i 's/MedComEquipmentAttachmentSystem\.generated\.h/SuspenseEquipmentAttachmentSystem.generated.h/g' "$dst_file"
    sed -i 's/MedComEquipmentVisualController\.generated\.h/SuspenseEquipmentVisualController.generated.h/g' "$dst_file"

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
# STAGE 4: Integration + Network + Presentation
# ============================================================

echo ""
echo "=== Stage 4: Integration, Network, Presentation ==="
echo ""

# Integration Components
echo "Migrating Integration..."
migrate "$SRC_DIR/Public/Components/Integration/MedComEquipmentAbilityConnector.h" "$DST_DIR/Public/Components/Integration/SuspenseEquipmentAbilityConnector.h"
migrate "$SRC_DIR/Private/Components/Integration/MedComEquipmentAbilityConnector.cpp" "$DST_DIR/Private/Components/Integration/SuspenseEquipmentAbilityConnector.cpp"
migrate "$SRC_DIR/Public/Components/Integration/MedComEquipmentLoadoutAdapter.h" "$DST_DIR/Public/Components/Integration/SuspenseEquipmentLoadoutAdapter.h"
migrate "$SRC_DIR/Private/Components/Integration/MedComEquipmentLoadoutAdapter.cpp" "$DST_DIR/Private/Components/Integration/SuspenseEquipmentLoadoutAdapter.cpp"

echo ""
echo "Migrating Network..."
# Network Components
migrate "$SRC_DIR/Public/Components/Network/MedComEquipmentNetworkDispatcher.h" "$DST_DIR/Public/Components/Network/SuspenseEquipmentNetworkDispatcher.h"
migrate "$SRC_DIR/Private/Components/Network/MedComEquipmentNetworkDispatcher.cpp" "$DST_DIR/Private/Components/Network/SuspenseEquipmentNetworkDispatcher.cpp"
migrate "$SRC_DIR/Public/Components/Network/MedComEquipmentPredictionSystem.h" "$DST_DIR/Public/Components/Network/SuspenseEquipmentPredictionSystem.h"
migrate "$SRC_DIR/Private/Components/Network/MedComEquipmentPredictionSystem.cpp" "$DST_DIR/Private/Components/Network/SuspenseEquipmentPredictionSystem.cpp"
migrate "$SRC_DIR/Public/Components/Network/MedComEquipmentReplicationManager.h" "$DST_DIR/Public/Components/Network/SuspenseEquipmentReplicationManager.h"
migrate "$SRC_DIR/Private/Components/Network/MedComEquipmentReplicationManager.cpp" "$DST_DIR/Private/Components/Network/SuspenseEquipmentReplicationManager.cpp"

echo ""
echo "Migrating Presentation..."
# Presentation Components
migrate "$SRC_DIR/Public/Components/Presentation/MedComEquipmentActorFactory.h" "$DST_DIR/Public/Components/Presentation/SuspenseEquipmentActorFactory.h"
migrate "$SRC_DIR/Private/Components/Presentation/MedComEquipmentActorFactory.cpp" "$DST_DIR/Private/Components/Presentation/SuspenseEquipmentActorFactory.cpp"
migrate "$SRC_DIR/Public/Components/Presentation/MedComEquipmentAttachmentSystem.h" "$DST_DIR/Public/Components/Presentation/SuspenseEquipmentAttachmentSystem.h"
migrate "$SRC_DIR/Private/Components/Presentation/MedComEquipmentAttachmentSystem.cpp" "$DST_DIR/Private/Components/Presentation/SuspenseEquipmentAttachmentSystem.cpp"
migrate "$SRC_DIR/Public/Components/Presentation/MedComEquipmentVisualController.h" "$DST_DIR/Public/Components/Presentation/SuspenseEquipmentVisualController.h"
migrate "$SRC_DIR/Private/Components/Presentation/MedComEquipmentVisualController.cpp" "$DST_DIR/Private/Components/Presentation/SuspenseEquipmentVisualController.cpp"

echo ""
echo "✅ Stage 4 Migration complete!"
echo "Files migrated: 16 files (8 headers + 8 cpp)"
echo ""
echo "Next: Stage 5 (Coordination + Transaction)"
