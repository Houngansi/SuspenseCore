#!/bin/bash

# MedComEquipment → EquipmentSystem Migration - STAGE 3 (Rules + Validation)
# Date: 2025-11-25
# Stage 3: Rules Engine + Validation (14 files)

set -e

echo "Starting MedComEquipment → EquipmentSystem Stage 3 migration..."
echo "Migrating: Rules + Validation"

# Source and destination
SRC_DIR="Source/EquipmentSystem/MedComEquipment"
DST_DIR="Source/EquipmentSystem"

# Create directory structure for Stage 3
mkdir -p "$DST_DIR/Private/Components/Rules"
mkdir -p "$DST_DIR/Private/Components/Validation"
mkdir -p "$DST_DIR/Public/Components/Rules"
mkdir -p "$DST_DIR/Public/Components/Validation"

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

    # Rules Engine classes (MedCom prefix removal)
    sed -i 's/\bUMedComEquipmentRulesEngine\b/USuspenseEquipmentRulesEngine/g' "$dst_file"
    sed -i 's/\bUMedComCompatibilityRulesEngine\b/USuspenseCompatibilityRulesEngine/g' "$dst_file"
    sed -i 's/\bUMedComConflictRulesEngine\b/USuspenseConflictRulesEngine/g' "$dst_file"
    sed -i 's/\bUMedComRequirementRulesEngine\b/USuspenseRequirementRulesEngine/g' "$dst_file"
    sed -i 's/\bUMedComWeightRulesEngine\b/USuspenseWeightRulesEngine/g' "$dst_file"
    sed -i 's/\bUMedComRulesCoordinator\b/USuspenseRulesCoordinator/g' "$dst_file"

    # Validation classes (already referenced in Stage 1)
    sed -i 's/\bUMedComEquipmentSlotValidator\b/USuspenseEquipmentSlotValidator/g' "$dst_file"

    # Structs
    sed -i 's/\bFEquipmentRule\b/FSuspenseEquipmentRule/g' "$dst_file"
    sed -i 's/\bFCompatibilityRule\b/FSuspenseCompatibilityRule/g' "$dst_file"
    sed -i 's/\bFConflictRule\b/FSuspenseConflictRule/g' "$dst_file"
    sed -i 's/\bFRequirementRule\b/FSuspenseRequirementRule/g' "$dst_file"
    sed -i 's/\bFWeightRule\b/FSuspenseWeightRule/g' "$dst_file"
    sed -i 's/\bFRuleEvaluationContext\b/FSuspenseRuleEvaluationContext/g' "$dst_file"
    sed -i 's/\bFRuleEvaluationResult\b/FSuspenseRuleEvaluationResult/g' "$dst_file"
    sed -i 's/\bFSlotValidationResult\b/FSuspenseSlotValidationResult/g' "$dst_file"

    # Enums
    sed -i 's/\bERuleType\b/ESuspenseRuleType/g' "$dst_file"
    sed -i 's/\bERulePriority\b/ESuspenseRulePriority/g' "$dst_file"
    sed -i 's/\bEValidationResult\b/ESuspenseValidationResult/g' "$dst_file"

    # File includes - Rules
    sed -i 's/MedComEquipmentRulesEngine\.h/SuspenseEquipmentRulesEngine.h/g' "$dst_file"
    sed -i 's/MedComCompatibilityRulesEngine\.h/SuspenseCompatibilityRulesEngine.h/g' "$dst_file"
    sed -i 's/MedComConflictRulesEngine\.h/SuspenseConflictRulesEngine.h/g' "$dst_file"
    sed -i 's/MedComRequirementRulesEngine\.h/SuspenseRequirementRulesEngine.h/g' "$dst_file"
    sed -i 's/MedComWeightRulesEngine\.h/SuspenseWeightRulesEngine.h/g' "$dst_file"
    sed -i 's/MedComRulesCoordinator\.h/SuspenseRulesCoordinator.h/g' "$dst_file"

    # File includes - Validation
    sed -i 's/MedComEquipmentSlotValidator\.h/SuspenseEquipmentSlotValidator.h/g' "$dst_file"

    # .generated.h includes - Rules
    sed -i 's/MedComEquipmentRulesEngine\.generated\.h/SuspenseEquipmentRulesEngine.generated.h/g' "$dst_file"
    sed -i 's/MedComCompatibilityRulesEngine\.generated\.h/SuspenseCompatibilityRulesEngine.generated.h/g' "$dst_file"
    sed -i 's/MedComConflictRulesEngine\.generated\.h/SuspenseConflictRulesEngine.generated.h/g' "$dst_file"
    sed -i 's/MedComRequirementRulesEngine\.generated\.h/SuspenseRequirementRulesEngine.generated.h/g' "$dst_file"
    sed -i 's/MedComWeightRulesEngine\.generated\.h/SuspenseWeightRulesEngine.generated.h/g' "$dst_file"
    sed -i 's/MedComRulesCoordinator\.generated\.h/SuspenseRulesCoordinator.generated.h/g' "$dst_file"

    # .generated.h includes - Validation
    sed -i 's/MedComEquipmentSlotValidator\.generated\.h/SuspenseEquipmentSlotValidator.generated.h/g' "$dst_file"

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
# STAGE 3: Rules + Validation
# ============================================================

echo ""
echo "=== Stage 3: Rules & Validation Files ==="
echo ""

# Rules Components
echo "Migrating Rules Engine..."
migrate "$SRC_DIR/Public/Components/Rules/MedComEquipmentRulesEngine.h" "$DST_DIR/Public/Components/Rules/SuspenseEquipmentRulesEngine.h"
migrate "$SRC_DIR/Private/Components/Rules/MedComEquipmentRulesEngine.cpp" "$DST_DIR/Private/Components/Rules/SuspenseEquipmentRulesEngine.cpp"
migrate "$SRC_DIR/Public/Components/Rules/MedComCompatibilityRulesEngine.h" "$DST_DIR/Public/Components/Rules/SuspenseCompatibilityRulesEngine.h"
migrate "$SRC_DIR/Private/Components/Rules/MedComCompatibilityRulesEngine.cpp" "$DST_DIR/Private/Components/Rules/SuspenseCompatibilityRulesEngine.cpp"
migrate "$SRC_DIR/Public/Components/Rules/MedComConflictRulesEngine.h" "$DST_DIR/Public/Components/Rules/SuspenseConflictRulesEngine.h"
migrate "$SRC_DIR/Private/Components/Rules/MedComConflictRulesEngine.cpp" "$DST_DIR/Private/Components/Rules/SuspenseConflictRulesEngine.cpp"
migrate "$SRC_DIR/Public/Components/Rules/MedComRequirementRulesEngine.h" "$DST_DIR/Public/Components/Rules/SuspenseRequirementRulesEngine.h"
migrate "$SRC_DIR/Private/Components/Rules/MedComRequirementRulesEngine.cpp" "$DST_DIR/Private/Components/Rules/SuspenseRequirementRulesEngine.cpp"
migrate "$SRC_DIR/Public/Components/Rules/MedComWeightRulesEngine.h" "$DST_DIR/Public/Components/Rules/SuspenseWeightRulesEngine.h"
migrate "$SRC_DIR/Private/Components/Rules/MedComWeightRulesEngine.cpp" "$DST_DIR/Private/Components/Rules/SuspenseWeightRulesEngine.cpp"
migrate "$SRC_DIR/Public/Components/Rules/MedComRulesCoordinator.h" "$DST_DIR/Public/Components/Rules/SuspenseRulesCoordinator.h"
migrate "$SRC_DIR/Private/Components/Rules/MedComRulesCoordinator.cpp" "$DST_DIR/Private/Components/Rules/SuspenseRulesCoordinator.cpp"

echo ""
echo "Migrating Validation..."
# Validation Components
migrate "$SRC_DIR/Public/Components/Validation/MedComEquipmentSlotValidator.h" "$DST_DIR/Public/Components/Validation/SuspenseEquipmentSlotValidator.h"
migrate "$SRC_DIR/Private/Components/Validation/MedComEquipmentSlotValidator.cpp" "$DST_DIR/Private/Components/Validation/SuspenseEquipmentSlotValidator.cpp"

echo ""
echo "✅ Stage 3 Migration complete!"
echo "Files migrated: 14 files (7 headers + 7 cpp)"
echo ""
echo "Next steps:"
echo "1. Run fixup script for BridgeSystem references"
echo "2. Verify compilation"
echo "3. Proceed to Stage 4 (Integration + Network + Presentation)"
