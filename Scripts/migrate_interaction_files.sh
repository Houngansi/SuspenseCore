#!/bin/bash
# MedComInteraction → SuspenseInteraction Migration Script
set -e

echo "Starting migration..."

SRC="Source/InteractionSystem/MedComInteraction"
DST="Source/InteractionSystem/SuspenseInteraction"

# Function to migrate file
migrate() {
    local src_file=$1
    local dst_file=$2
    echo "Migrating: $(basename $src_file)"
    cp "$src_file" "$dst_file"
    sed -i 's/MedComBasePickupItem/SuspensePickupItem/g' "$dst_file"
    sed -i 's/MedComItemFactory/SuspenseItemFactory/g' "$dst_file"
    sed -i 's/MedComStaticHelpers/SuspenseHelpers/g' "$dst_file"
    sed -i 's/MedComInteractionSettings/SuspenseInteractionSettings/g' "$dst_file"
    sed -i 's/MedComInteraction/SuspenseInteraction/g' "$dst_file"
    sed -i 's/MEDCOMINTERACTION_API/SUSPENSEINTERACTION_API/g' "$dst_file"
    sed -i 's/LogMedComInteraction/LogSuspenseInteraction/g' "$dst_file"
    sed -i 's/LogInventoryStatistics/LogSuspenseInventoryStatistics/g' "$dst_file"
    sed -i 's/ClassGroup = (MedCom)/ClassGroup = (Suspense)/g' "$dst_file"
    sed -i 's/Category = "MedCom|/Category = "Suspense|/g' "$dst_file"
    sed -i 's/Copyright MedCom Team/Copyright Suspense Team/g' "$dst_file"
    sed -i 's/DisplayName = "MedCom/DisplayName = "Suspense/g' "$dst_file"
}

# Migrate files
migrate "$SRC/Public/Pickup/MedComBasePickupItem.h" "$DST/Public/Pickup/SuspensePickupItem.h"
migrate "$SRC/Private/Pickup/MedComBasePickupItem.cpp" "$DST/Private/Pickup/SuspensePickupItem.cpp"
migrate "$SRC/Public/Utils/MedComItemFactory.h" "$DST/Public/Utils/SuspenseItemFactory.h"
migrate "$SRC/Private/Utils/MedComItemFactory.cpp" "$DST/Private/Utils/SuspenseItemFactory.cpp"
migrate "$SRC/Public/Utils/MedComStaticHelpers.h" "$DST/Public/Utils/SuspenseHelpers.h"
migrate "$SRC/Private/Utils/MedComStaticHelpers.cpp" "$DST/Private/Utils/SuspenseHelpers.cpp"
migrate "$SRC/Public/Utils/MedComInteractionSettings.h" "$DST/Public/Utils/SuspenseInteractionSettings.h"

echo "✅ Migration complete!"
