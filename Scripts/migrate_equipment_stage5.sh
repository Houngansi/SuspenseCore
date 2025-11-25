#!/bin/bash

# MedComEquipment → EquipmentSystem Migration - STAGE 5 (Final - Coordination + Transaction)
# Date: 2025-11-25
# Stage 5: Coordination, Transaction (4 files)

set -e

echo "Starting MedComEquipment → EquipmentSystem Stage 5 migration..."
echo "Migrating: Coordination + Transaction (FINAL STAGE)"

# Source and destination
SRC_DIR="Source/EquipmentSystem/MedComEquipment"
DST_DIR="Source/EquipmentSystem"

# Create directory structure for Stage 5
mkdir -p "$DST_DIR/Private/Components/Coordination"
mkdir -p "$DST_DIR/Private/Components/Transaction"
mkdir -p "$DST_DIR/Public/Components/Coordination"
mkdir -p "$DST_DIR/Public/Components/Transaction"

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

    # Coordination classes
    sed -i 's/\bUMedComEquipmentEventDispatcher\b/USuspenseEquipmentEventDispatcher/g' "$dst_file"

    # Transaction classes (already referenced in Stage 1)
    sed -i 's/\bUMedComEquipmentTransactionProcessor\b/USuspenseEquipmentTransactionProcessor/g' "$dst_file"

    # Structs
    sed -i 's/\bFEquipmentEvent\b/FSuspenseEquipmentEvent/g' "$dst_file"
    sed -i 's/\bFEventDispatchData\b/FSuspenseEventDispatchData/g' "$dst_file"
    sed -i 's/\bFTransactionData\b/FSuspenseTransactionData/g' "$dst_file"
    sed -i 's/\bFTransactionResult\b/FSuspenseTransactionResult/g' "$dst_file"

    # File includes - Coordination
    sed -i 's/MedComEquipmentEventDispatcher\.h/SuspenseEquipmentEventDispatcher.h/g' "$dst_file"

    # File includes - Transaction
    sed -i 's/MedComEquipmentTransactionProcessor\.h/SuspenseEquipmentTransactionProcessor.h/g' "$dst_file"

    # .generated.h includes - Coordination
    sed -i 's/MedComEquipmentEventDispatcher\.generated\.h/SuspenseEquipmentEventDispatcher.generated.h/g' "$dst_file"

    # .generated.h includes - Transaction
    sed -i 's/MedComEquipmentTransactionProcessor\.generated\.h/SuspenseEquipmentTransactionProcessor.generated.h/g' "$dst_file"

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
# STAGE 5: Coordination + Transaction (FINAL)
# ============================================================

echo ""
echo "=== Stage 5: Coordination + Transaction (FINAL) ==="
echo ""

# Coordination Components
echo "Migrating Coordination..."
migrate "$SRC_DIR/Public/Components/Coordination/MedComEquipmentEventDispatcher.h" "$DST_DIR/Public/Components/Coordination/SuspenseEquipmentEventDispatcher.h"
migrate "$SRC_DIR/Private/Components/Coordination/MedComEquipmentEventDispatcher.cpp" "$DST_DIR/Private/Components/Coordination/SuspenseEquipmentEventDispatcher.cpp"

echo ""
echo "Migrating Transaction..."
# Transaction Components
migrate "$SRC_DIR/Public/Components/Transaction/MedComEquipmentTransactionProcessor.h" "$DST_DIR/Public/Components/Transaction/SuspenseEquipmentTransactionProcessor.h"
migrate "$SRC_DIR/Private/Components/Transaction/MedComEquipmentTransactionProcessor.cpp" "$DST_DIR/Private/Components/Transaction/SuspenseEquipmentTransactionProcessor.cpp"

echo ""
echo "✅✅✅ Stage 5 Migration complete! ✅✅✅"
echo "Files migrated: 4 files (2 headers + 2 cpp)"
echo ""
echo "🎉🎉🎉 ALL EQUIPMENT MIGRATION COMPLETE! 🎉🎉🎉"
echo "Total files migrated: 80/80 (100%)"
