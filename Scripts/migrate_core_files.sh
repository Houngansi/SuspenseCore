#!/bin/bash
# MedComCore → SuspenseCore Migration Script
set -e

echo "Starting MedComCore → SuspenseCore migration..."

SRC="Source/PlayerCore/MedComCore"
DST="Source/PlayerCore"

# Function to migrate file
migrate() {
    local src_file=$1
    local dst_file=$2
    echo "Migrating: $(basename $src_file)"
    cp "$src_file" "$dst_file"

    # Perform replacements
    sed -i 's/MedComCore/SuspenseCore/g' "$dst_file"
    sed -i 's/MedComBaseCharacter/SuspenseCharacter/g' "$dst_file"
    sed -i 's/MedComCharacterMovementComponent/SuspenseCharacterMovementComponent/g' "$dst_file"
    sed -i 's/MedComGameInstance/SuspenseGameInstance/g' "$dst_file"
    sed -i 's/MedComGameMode/SuspenseGameMode/g' "$dst_file"
    sed -i 's/MedComGameState/SuspenseGameState/g' "$dst_file"
    sed -i 's/MedComPlayerController/SuspensePlayerController/g' "$dst_file"
    sed -i 's/MedComPlayerState/SuspensePlayerState/g' "$dst_file"
    sed -i 's/MEDCOMCORE_API/SUSPENSECORE_API/g' "$dst_file"
    sed -i 's/Copyright Epic Games, Inc\./Copyright Suspense Team/g' "$dst_file"
}

# Migrate module files
migrate "$SRC/Public/MedComCore.h" "$DST/Public/SuspenseCore.h"
migrate "$SRC/Private/MedComCore.cpp" "$DST/Private/SuspenseCore.cpp"

# Migrate Character files
migrate "$SRC/Public/Characters/MedComBaseCharacter.h" "$DST/Public/Characters/SuspenseCharacter.h"
migrate "$SRC/Private/Characters/MedComBaseCharacter.cpp" "$DST/Private/Characters/SuspenseCharacter.cpp"
migrate "$SRC/Public/Characters/MedComCharacterMovementComponent.h" "$DST/Public/Characters/SuspenseCharacterMovementComponent.h"
migrate "$SRC/Private/Characters/MedComCharacterMovementComponent.cpp" "$DST/Private/Characters/SuspenseCharacterMovementComponent.cpp"

# Migrate Core files
migrate "$SRC/Public/Core/MedComGameInstance.h" "$DST/Public/Core/SuspenseGameInstance.h"
migrate "$SRC/Private/Core/MedComGameInstance.cpp" "$DST/Private/Core/SuspenseGameInstance.cpp"
migrate "$SRC/Public/Core/MedComGameMode.h" "$DST/Public/Core/SuspenseGameMode.h"
migrate "$SRC/Private/Core/MedComGameMode.cpp" "$DST/Private/Core/SuspenseGameMode.cpp"
migrate "$SRC/Public/Core/MedComGameState.h" "$DST/Public/Core/SuspenseGameState.h"
migrate "$SRC/Private/Core/MedComGameState.cpp" "$DST/Private/Core/SuspenseGameState.cpp"
migrate "$SRC/Public/Core/MedComPlayerController.h" "$DST/Public/Core/SuspensePlayerController.h"
migrate "$SRC/Private/Core/MedComPlayerController.cpp" "$DST/Private/Core/SuspensePlayerController.cpp"
migrate "$SRC/Public/Core/MedComPlayerState.h" "$DST/Public/Core/SuspensePlayerState.h"
migrate "$SRC/Private/Core/MedComPlayerState.cpp" "$DST/Private/Core/SuspensePlayerState.cpp"

echo "✅ Migration complete!"
echo "Files migrated: 16 files (8 headers + 8 cpp)"
