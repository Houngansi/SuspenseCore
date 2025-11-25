#!/bin/bash

# MedComInventory → InventorySystem Migration Script
# Date: 2025-11-25
# Migrates MedComInventory files directly into InventorySystem module

set -e

echo "Starting MedComInventory → InventorySystem migration..."

# Source and destination
SRC_DIR="Source/InventorySystem/MedComInventory"
DST_DIR="Source/InventorySystem"

# Create directory structure in InventorySystem
mkdir -p "$DST_DIR/Private/Base"
mkdir -p "$DST_DIR/Private/Components"
mkdir -p "$DST_DIR/Private/Debug"
mkdir -p "$DST_DIR/Private/Events"
mkdir -p "$DST_DIR/Private/Network"
mkdir -p "$DST_DIR/Private/Operations"
mkdir -p "$DST_DIR/Private/Serialization"
mkdir -p "$DST_DIR/Private/Storage"
mkdir -p "$DST_DIR/Private/Templates"
mkdir -p "$DST_DIR/Private/UI"
mkdir -p "$DST_DIR/Private/Utility"
mkdir -p "$DST_DIR/Private/Validation"

mkdir -p "$DST_DIR/Public/Base"
mkdir -p "$DST_DIR/Public/Components"
mkdir -p "$DST_DIR/Public/Debug"
mkdir -p "$DST_DIR/Public/Events"
mkdir -p "$DST_DIR/Public/Network"
mkdir -p "$DST_DIR/Public/Operations"
mkdir -p "$DST_DIR/Public/Serialization"
mkdir -p "$DST_DIR/Public/Storage"
mkdir -p "$DST_DIR/Public/Templates"
mkdir -p "$DST_DIR/Public/UI"
mkdir -p "$DST_DIR/Public/Utility"
mkdir -p "$DST_DIR/Public/Validation"

# Function to migrate and transform file
migrate() {
    local src_file="$1"
    local dst_file="$2"

    echo "Migrating: $(basename $src_file)"

    cp "$src_file" "$dst_file"

    # Module name changes (CRITICAL: MedComInventory → InventorySystem, not SuspenseInventory!)
    sed -i 's/MedComInventory\.h/InventorySystem.h/g' "$dst_file"
    sed -i 's/MedComInventory\.cpp/InventorySystem.cpp/g' "$dst_file"
    sed -i 's/FMedComInventoryModule/FInventorySystemModule/g' "$dst_file"
    sed -i 's/MEDCOMINVENTORY_API/INVENTORYSYSTEM_API/g' "$dst_file"

    # Component classes
    sed -i 's/UMedComInventoryComponent/USuspenseInventoryComponent/g' "$dst_file"

    # Storage and Core classes
    sed -i 's/UMedComInventoryStorage/USuspenseInventoryStorage/g' "$dst_file"
    sed -i 's/UMedComInventoryItem/USuspenseInventoryItem/g' "$dst_file"
    sed -i 's/UMedComItemBase/USuspenseItemBase/g' "$dst_file"

    # Event and Transaction classes
    sed -i 's/UMedComInventoryEvents/USuspenseInventoryEvents/g' "$dst_file"
    sed -i 's/UMedComInventoryTransaction/USuspenseInventoryTransaction/g' "$dst_file"

    # Utility and Service classes
    sed -i 's/UMedComInventorySerializer/USuspenseInventorySerializer/g' "$dst_file"
    sed -i 's/UMedComInventoryTemplate/USuspenseInventoryTemplate/g' "$dst_file"
    sed -i 's/UMedComInventoryUIConnector/USuspenseInventoryUIConnector/g' "$dst_file"
    sed -i 's/UMedComInventoryConstraints/USuspenseInventoryValidator/g' "$dst_file"

    # Blueprint Libraries
    sed -i 's/UMedComInventoryFunctionLibrary/USuspenseInventoryLibrary/g' "$dst_file"
    sed -i 's/UMedComItemBlueprintLibrary/USuspenseItemLibrary/g' "$dst_file"

    # Classes without MedCom prefix (add Suspense brand)
    sed -i 's/\bUInventoryManager\b/USuspenseInventoryManager/g' "$dst_file"
    sed -i 's/\bUInventoryReplicator\b/USuspenseInventoryReplicator/g' "$dst_file"
    sed -i 's/\bUInventoryDebugger\b/USuspenseInventoryDebugger/g' "$dst_file"

    # Operation classes (add Suspense prefix)
    sed -i 's/\bFInventoryOperation\b/FSuspenseInventoryOperation/g' "$dst_file"
    sed -i 's/\bFMoveOperation\b/FSuspenseMoveOperation/g' "$dst_file"
    sed -i 's/\bFStackOperation\b/FSuspenseStackOperation/g' "$dst_file"
    sed -i 's/\bFRotationOperation\b/FSuspenseRotationOperation/g' "$dst_file"
    sed -i 's/\bFInventoryHistory\b/FSuspenseInventoryHistory/g' "$dst_file"

    # Logs struct/namespace
    sed -i 's/\bFInventoryLogs\b/FSuspenseInventoryLogs/g' "$dst_file"
    sed -i 's/\bInventoryLogs\b/SuspenseInventoryLogs/g' "$dst_file"

    # File includes (update to new file names)
    sed -i 's/MedComInventoryComponent\.h/SuspenseInventoryComponent.h/g' "$dst_file"
    sed -i 's/MedComInventoryStorage\.h/SuspenseInventoryStorage.h/g' "$dst_file"
    sed -i 's/MedComInventoryItem\.h/SuspenseInventoryItem.h/g' "$dst_file"
    sed -i 's/MedComItemBase\.h/SuspenseItemBase.h/g' "$dst_file"
    sed -i 's/MedComInventoryEvents\.h/SuspenseInventoryEvents.h/g' "$dst_file"
    sed -i 's/MedComInventoryTransaction\.h/SuspenseInventoryTransaction.h/g' "$dst_file"
    sed -i 's/MedComInventorySerializer\.h/SuspenseInventorySerializer.h/g' "$dst_file"
    sed -i 's/MedComInventoryTemplate\.h/SuspenseInventoryTemplate.h/g' "$dst_file"
    sed -i 's/MedComInventoryUIConnector\.h/SuspenseInventoryUIConnector.h/g' "$dst_file"
    sed -i 's/MedComInventoryConstraints\.h/SuspenseInventoryValidator.h/g' "$dst_file"
    sed -i 's/MedComInventoryFunctionLibrary\.h/SuspenseInventoryLibrary.h/g' "$dst_file"
    sed -i 's/MedComItemBlueprintLibrary\.h/SuspenseItemLibrary.h/g' "$dst_file"
    sed -i 's/InventoryManager\.h/SuspenseInventoryManager.h/g' "$dst_file"
    sed -i 's/InventoryReplicator\.h/SuspenseInventoryReplicator.h/g' "$dst_file"
    sed -i 's/InventoryDebugger\.h/SuspenseInventoryDebugger.h/g' "$dst_file"
    sed -i 's/InventoryOperation\.h/SuspenseInventoryOperation.h/g' "$dst_file"
    sed -i 's/InventoryHistory\.h/SuspenseInventoryHistory.h/g' "$dst_file"
    sed -i 's/MoveOperation\.h/SuspenseMoveOperation.h/g' "$dst_file"
    sed -i 's/StackOperation\.h/SuspenseStackOperation.h/g' "$dst_file"
    sed -i 's/RotationOperation\.h/SuspenseRotationOperation.h/g' "$dst_file"
    sed -i 's/InventoryLogs\.h/SuspenseInventoryLogs.h/g' "$dst_file"

    # .generated.h includes
    sed -i 's/MedComInventoryComponent\.generated\.h/SuspenseInventoryComponent.generated.h/g' "$dst_file"
    sed -i 's/MedComInventoryStorage\.generated\.h/SuspenseInventoryStorage.generated.h/g' "$dst_file"
    sed -i 's/MedComInventoryItem\.generated\.h/SuspenseInventoryItem.generated.h/g' "$dst_file"
    sed -i 's/MedComItemBase\.generated\.h/SuspenseItemBase.generated.h/g' "$dst_file"
    sed -i 's/MedComInventoryEvents\.generated\.h/SuspenseInventoryEvents.generated.h/g' "$dst_file"
    sed -i 's/MedComInventoryTransaction\.generated\.h/SuspenseInventoryTransaction.generated.h/g' "$dst_file"
    sed -i 's/MedComInventorySerializer\.generated\.h/SuspenseInventorySerializer.generated.h/g' "$dst_file"
    sed -i 's/MedComInventoryTemplate\.generated\.h/SuspenseInventoryTemplate.generated.h/g' "$dst_file"
    sed -i 's/MedComInventoryUIConnector\.generated\.h/SuspenseInventoryUIConnector.generated.h/g' "$dst_file"
    sed -i 's/MedComInventoryConstraints\.generated\.h/SuspenseInventoryValidator.generated.h/g' "$dst_file"
    sed -i 's/MedComInventoryFunctionLibrary\.generated\.h/SuspenseInventoryLibrary.generated.h/g' "$dst_file"
    sed -i 's/MedComItemBlueprintLibrary\.generated\.h/SuspenseItemLibrary.generated.h/g' "$dst_file"
    sed -i 's/InventoryManager\.generated\.h/SuspenseInventoryManager.generated.h/g' "$dst_file"
    sed -i 's/InventoryReplicator\.generated\.h/SuspenseInventoryReplicator.generated.h/g' "$dst_file"
    sed -i 's/InventoryDebugger\.generated\.h/SuspenseInventoryDebugger.generated.h/g' "$dst_file"

    # Blueprint categories
    sed -i 's/Category="MedCom|Inventory/Category="SuspenseCore|Inventory/g' "$dst_file"
    sed -i 's/Category = "MedCom|Inventory/Category = "SuspenseCore|Inventory/g' "$dst_file"
    sed -i 's/Category="Inventory/Category="SuspenseCore|Inventory/g' "$dst_file"

    # Copyright
    sed -i 's/Copyright MedCom Team/Copyright Suspense Team/g' "$dst_file"

    # Comments
    sed -i 's/для MedCom/для Suspense/g' "$dst_file"
    sed -i 's/проекта MedCom/проекта Suspense/g' "$dst_file"
}

# Migrate module header/cpp
migrate "$SRC_DIR/Public/MedComInventory.h" "$DST_DIR/Public/InventorySystem_Module.h"
migrate "$SRC_DIR/Private/MedComInventory.cpp" "$DST_DIR/Private/InventorySystem_Module.cpp"

# Migrate Base files
migrate "$SRC_DIR/Public/Base/InventoryLogs.h" "$DST_DIR/Public/Base/SuspenseInventoryLogs.h"
migrate "$SRC_DIR/Public/Base/InventoryManager.h" "$DST_DIR/Public/Base/SuspenseInventoryManager.h"
migrate "$SRC_DIR/Private/Base/InventoryManager.cpp" "$DST_DIR/Private/Base/SuspenseInventoryManager.cpp"
migrate "$SRC_DIR/Public/Base/MedComInventoryFunctionLibrary.h" "$DST_DIR/Public/Base/SuspenseInventoryLibrary.h"
migrate "$SRC_DIR/Private/Base/MedComInventoryFunctionLibrary.cpp" "$DST_DIR/Private/Base/SuspenseInventoryLibrary.cpp"
migrate "$SRC_DIR/Public/Base/MedComInventoryItem.h" "$DST_DIR/Public/Base/SuspenseInventoryItem.h"
migrate "$SRC_DIR/Private/Base/MedComInventoryItem.cpp" "$DST_DIR/Private/Base/SuspenseInventoryItem.cpp"
migrate "$SRC_DIR/Public/Base/MedComItemBase.h" "$DST_DIR/Public/Base/SuspenseItemBase.h"
migrate "$SRC_DIR/Private/Base/MedComItemBase.cpp" "$DST_DIR/Private/Base/SuspenseItemBase.cpp"
migrate "$SRC_DIR/Private/Base/InventoryLogs.cpp" "$DST_DIR/Private/Base/SuspenseInventoryLogs.cpp"

# Migrate Components
migrate "$SRC_DIR/Public/Components/MedComInventoryComponent.h" "$DST_DIR/Public/Components/SuspenseInventoryComponent.h"
migrate "$SRC_DIR/Private/Components/MedComInventoryComponent.cpp" "$DST_DIR/Private/Components/SuspenseInventoryComponent.cpp"

# Migrate Debug
migrate "$SRC_DIR/Public/Debug/InventoryDebugger.h" "$DST_DIR/Public/Debug/SuspenseInventoryDebugger.h"
migrate "$SRC_DIR/Private/Debug/InventoryDebugger.cpp" "$DST_DIR/Private/Debug/SuspenseInventoryDebugger.cpp"

# Migrate Events
migrate "$SRC_DIR/Public/Events/MedComInventoryEvents.h" "$DST_DIR/Public/Events/SuspenseInventoryEvents.h"
migrate "$SRC_DIR/Private/Events/MedComInventoryEvents.cpp" "$DST_DIR/Private/Events/SuspenseInventoryEvents.cpp"

# Migrate Network
migrate "$SRC_DIR/Public/Network/InventoryReplicator.h" "$DST_DIR/Public/Network/SuspenseInventoryReplicator.h"
migrate "$SRC_DIR/Private/Network/InventoryReplicator.cpp" "$DST_DIR/Private/Network/SuspenseInventoryReplicator.cpp"

# Migrate Operations
migrate "$SRC_DIR/Public/Operations/InventoryHistory.h" "$DST_DIR/Public/Operations/SuspenseInventoryHistory.h"
migrate "$SRC_DIR/Private/Operations/InventoryHistory.cpp" "$DST_DIR/Private/Operations/SuspenseInventoryHistory.cpp"
migrate "$SRC_DIR/Public/Operations/InventoryOperation.h" "$DST_DIR/Public/Operations/SuspenseInventoryOperation.h"
migrate "$SRC_DIR/Public/Operations/MedComInventoryTransaction.h" "$DST_DIR/Public/Operations/SuspenseInventoryTransaction.h"
migrate "$SRC_DIR/Private/Operations/MedComInventoryTransaction.cpp" "$DST_DIR/Private/Operations/SuspenseInventoryTransaction.cpp"
migrate "$SRC_DIR/Public/Operations/MoveOperation.h" "$DST_DIR/Public/Operations/SuspenseMoveOperation.h"
migrate "$SRC_DIR/Private/Operations/MoveOperation.cpp" "$DST_DIR/Private/Operations/SuspenseMoveOperation.cpp"
migrate "$SRC_DIR/Public/Operations/RotationOperation.h" "$DST_DIR/Public/Operations/SuspenseRotationOperation.h"
migrate "$SRC_DIR/Private/Operations/RotationOperation.cpp" "$DST_DIR/Private/Operations/SuspenseRotationOperation.cpp"
migrate "$SRC_DIR/Public/Operations/StackOperation.h" "$DST_DIR/Public/Operations/SuspenseStackOperation.h"
migrate "$SRC_DIR/Private/Operations/StackOperation.cpp" "$DST_DIR/Private/Operations/SuspenseStackOperation.cpp"

# Migrate Serialization
migrate "$SRC_DIR/Public/Serialization/MedComInventorySerializer.h" "$DST_DIR/Public/Serialization/SuspenseInventorySerializer.h"
migrate "$SRC_DIR/Private/Serialization/MedComInventorySerializer.cpp" "$DST_DIR/Private/Serialization/SuspenseInventorySerializer.cpp"

# Migrate Storage
migrate "$SRC_DIR/Public/Storage/MedComInventoryStorage.h" "$DST_DIR/Public/Storage/SuspenseInventoryStorage.h"
migrate "$SRC_DIR/Private/Storage/MedComInventoryStorage.cpp" "$DST_DIR/Private/Storage/SuspenseInventoryStorage.cpp"

# Migrate Templates
migrate "$SRC_DIR/Public/Templates/MedComInventoryTemplate.h" "$DST_DIR/Public/Templates/SuspenseInventoryTemplate.h"
migrate "$SRC_DIR/Private/Templates/MedComInventoryTemplate.cpp" "$DST_DIR/Private/Templates/SuspenseInventoryTemplate.cpp"

# Migrate UI
migrate "$SRC_DIR/Public/UI/MedComInventoryUIConnector.h" "$DST_DIR/Public/UI/SuspenseInventoryUIConnector.h"
migrate "$SRC_DIR/Private/UI/MedComInventoryUIConnector.cpp" "$DST_DIR/Private/UI/SuspenseInventoryUIConnector.cpp"

# Migrate Utility
migrate "$SRC_DIR/Public/Utility/MedComItemBlueprintLibrary.h" "$DST_DIR/Public/Utility/SuspenseItemLibrary.h"
migrate "$SRC_DIR/Private/Utility/MedComItemBlueprintLibrary.cpp" "$DST_DIR/Private/Utility/SuspenseItemLibrary.cpp"

# Migrate Validation
migrate "$SRC_DIR/Public/Validation/MedComInventoryConstraints.h" "$DST_DIR/Public/Validation/SuspenseInventoryValidator.h"
migrate "$SRC_DIR/Private/Validation/MedComInventoryConstraints.cpp" "$DST_DIR/Private/Validation/SuspenseInventoryValidator.cpp"

echo "✅ Migration complete!"
echo "Files migrated: 43 files (21 headers + 21 cpp + 1 module)"
echo ""
echo "Note: InventorySystem.Build.cs needs to be updated with dependencies"
