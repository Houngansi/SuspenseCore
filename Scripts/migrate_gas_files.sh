#!/bin/bash

# MedComGAS → GAS Migration Script
# Date: 2025-11-25
# Migrates all MedComGAS files to GAS module

set -e

echo "Starting MedComGAS → GAS migration..."

# Source and destination directories
SRC_DIR="Source/GAS/MedComGAS"
DST_DIR="Source/GAS"

# Create directory structure
mkdir -p "$DST_DIR/Private/Abilities"
mkdir -p "$DST_DIR/Private/Attributes"
mkdir -p "$DST_DIR/Private/Components"
mkdir -p "$DST_DIR/Private/Effects"
mkdir -p "$DST_DIR/Private/Subsystems"

mkdir -p "$DST_DIR/Public/Abilities"
mkdir -p "$DST_DIR/Public/Attributes"
mkdir -p "$DST_DIR/Public/Components"
mkdir -p "$DST_DIR/Public/Effects"
mkdir -p "$DST_DIR/Public/Subsystems"

# Function to migrate and transform file
migrate() {
    local src_file="$1"
    local dst_file="$2"

    echo "Migrating: $(basename $src_file)"

    cp "$src_file" "$dst_file"

    # Module-level replacements
    sed -i 's/MedComGAS\.h/GAS.h/g' "$dst_file"
    sed -i 's/MedComGAS\.cpp/GAS.cpp/g' "$dst_file"
    sed -i 's/FMedComGASModule/FGASModule/g' "$dst_file"
    sed -i 's/MEDCOMGAS_API/GAS_API/g' "$dst_file"

    # Base class replacements (must come before specific classes to avoid conflicts)
    sed -i 's/UMedComGameplayAbility/UGASAbility/g' "$dst_file"
    sed -i 's/UMedComBaseAttributeSet/UGASAttributeSet/g' "$dst_file"
    sed -i 's/UMedComGameplayEffect\([^_]\)/UGASEffect\1/g' "$dst_file"

    # Ability classes (drop MedCom prefix)
    sed -i 's/UMedComCharacterCrouchAbility/UCharacterCrouchAbility/g' "$dst_file"
    sed -i 's/UMedComCharacterJumpAbility/UCharacterJumpAbility/g' "$dst_file"
    sed -i 's/UMedComCharacterSprintAbility/UCharacterSprintAbility/g' "$dst_file"
    sed -i 's/UMedComInteractAbility/UInteractAbility/g' "$dst_file"
    sed -i 's/UMedComWeaponSwitchAbility/UWeaponSwitchAbility/g' "$dst_file"
    sed -i 's/UMedComWeaponToggleAbility/UWeaponToggleAbility/g' "$dst_file"

    # Attribute Set classes (drop MedCom prefix)
    sed -i 's/UMedComAmmoAttributeSet/UAmmoAttributeSet/g' "$dst_file"
    sed -i 's/UMedComArmorAttributeSet/UArmorAttributeSet/g' "$dst_file"
    sed -i 's/UMedComDefaultAttributeSet/UDefaultAttributeSet/g' "$dst_file"
    sed -i 's/UMedComWeaponAttributeSet/UWeaponAttributeSet/g' "$dst_file"

    # Component classes
    sed -i 's/UMedComAbilitySystemComponent/UGASAbilitySystemComponent/g' "$dst_file"

    # GameplayEffect classes (drop MedCom prefix from specific effects)
    sed -i 's/UMedComGameplayEffect_/UGameplayEffect_/g' "$dst_file"
    sed -i 's/UMedComInitialAttributesEffect/UInitialAttributesEffect/g' "$dst_file"

    # Subsystem classes (drop MedCom prefix)
    sed -i 's/UMedComWeaponAnimationSubsystem/UWeaponAnimationSubsystem/g' "$dst_file"

    # File includes (update to new file names)
    sed -i 's/MedComGameplayAbility\.h/GASAbility.h/g' "$dst_file"
    sed -i 's/MedComBaseAttributeSet\.h/GASAttributeSet.h/g' "$dst_file"
    sed -i 's/MedComGameplayEffect\.h/GASEffect.h/g' "$dst_file"
    sed -i 's/MedComCharacterCrouchAbility\.h/CharacterCrouchAbility.h/g' "$dst_file"
    sed -i 's/MedComCharacterJumpAbility\.h/CharacterJumpAbility.h/g' "$dst_file"
    sed -i 's/MedComCharacterSprintAbility\.h/CharacterSprintAbility.h/g' "$dst_file"
    sed -i 's/MedComInteractAbility\.h/InteractAbility.h/g' "$dst_file"
    sed -i 's/MedComWeaponSwitchAbility\.h/WeaponSwitchAbility.h/g' "$dst_file"
    sed -i 's/MedComWeaponToggleAbility\.h/WeaponToggleAbility.h/g' "$dst_file"
    sed -i 's/MedComAmmoAttributeSet\.h/AmmoAttributeSet.h/g' "$dst_file"
    sed -i 's/MedComArmorAttributeSet\.h/ArmorAttributeSet.h/g' "$dst_file"
    sed -i 's/MedComDefaultAttributeSet\.h/DefaultAttributeSet.h/g' "$dst_file"
    sed -i 's/MedComWeaponAttributeSet\.h/WeaponAttributeSet.h/g' "$dst_file"
    sed -i 's/MedComAbilitySystemComponent\.h/GASAbilitySystemComponent.h/g' "$dst_file"
    sed -i 's/MedComGameplayEffect_\([^"]*\)\.h/GameplayEffect_\1.h/g' "$dst_file"
    sed -i 's/MedComInitialAttributesEffect\.h/InitialAttributesEffect.h/g' "$dst_file"
    sed -i 's/MedComWeaponAnimationSubsystem\.h/WeaponAnimationSubsystem.h/g' "$dst_file"

    # Blueprint categories
    sed -i 's/Category="MedCom|/Category="GAS|/g' "$dst_file"
    sed -i 's/Category = "MedCom|/Category = "GAS|/g' "$dst_file"

    # Copyright
    sed -i 's/Copyright MedCom Team/Copyright Suspense Team/g' "$dst_file"

    # Russian comments
    sed -i 's/для MedCom/для GAS/g' "$dst_file"
    sed -i 's/проекта MedCom/проекта GAS/g' "$dst_file"
    sed -i 's/Собственный компонент AbilitySystem для проекта GAS/Собственный компонент AbilitySystem для GAS/g' "$dst_file"
}

# Migrate module files
migrate "$SRC_DIR/Public/MedComGAS.h" "$DST_DIR/Public/GAS.h"
migrate "$SRC_DIR/Private/MedComGAS.cpp" "$DST_DIR/Private/GAS.cpp"

# Migrate Abilities
migrate "$SRC_DIR/Public/Abilities/MedComGameplayAbility.h" "$DST_DIR/Public/Abilities/GASAbility.h"
migrate "$SRC_DIR/Private/Abilities/MedComGameplayAbility.cpp" "$DST_DIR/Private/Abilities/GASAbility.cpp"
migrate "$SRC_DIR/Public/Abilities/MedComCharacterCrouchAbility.h" "$DST_DIR/Public/Abilities/CharacterCrouchAbility.h"
migrate "$SRC_DIR/Private/Abilities/MedComCharacterCrouchAbility.cpp" "$DST_DIR/Private/Abilities/CharacterCrouchAbility.cpp"
migrate "$SRC_DIR/Public/Abilities/MedComCharacterJumpAbility.h" "$DST_DIR/Public/Abilities/CharacterJumpAbility.h"
migrate "$SRC_DIR/Private/Abilities/MedComCharacterJumpAbility.cpp" "$DST_DIR/Private/Abilities/CharacterJumpAbility.cpp"
migrate "$SRC_DIR/Public/Abilities/MedComCharacterSprintAbility.h" "$DST_DIR/Public/Abilities/CharacterSprintAbility.h"
migrate "$SRC_DIR/Private/Abilities/MedComCharacterSprintAbility.cpp" "$DST_DIR/Private/Abilities/CharacterSprintAbility.cpp"
migrate "$SRC_DIR/Public/Abilities/MedComInteractAbility.h" "$DST_DIR/Public/Abilities/InteractAbility.h"
migrate "$SRC_DIR/Private/Abilities/MedComInteractAbility.cpp" "$DST_DIR/Private/Abilities/InteractAbility.cpp"
migrate "$SRC_DIR/Public/Abilities/MedComWeaponSwitchAbility.h" "$DST_DIR/Public/Abilities/WeaponSwitchAbility.h"
migrate "$SRC_DIR/Private/Abilities/MedComWeaponSwitchAbility.cpp" "$DST_DIR/Private/Abilities/WeaponSwitchAbility.cpp"
migrate "$SRC_DIR/Public/Abilities/MedComWeaponToggleAbility.h" "$DST_DIR/Public/Abilities/WeaponToggleAbility.h"
migrate "$SRC_DIR/Private/Abilities/MedComWeaponToggleAbility.cpp" "$DST_DIR/Private/Abilities/WeaponToggleAbility.cpp"

# Migrate Attributes
migrate "$SRC_DIR/Public/Attributes/MedComBaseAttributeSet.h" "$DST_DIR/Public/Attributes/GASAttributeSet.h"
migrate "$SRC_DIR/Private/Attributes/MedComBaseAttributeSet.cpp" "$DST_DIR/Private/Attributes/GASAttributeSet.cpp"
migrate "$SRC_DIR/Public/Attributes/MedComAmmoAttributeSet.h" "$DST_DIR/Public/Attributes/AmmoAttributeSet.h"
migrate "$SRC_DIR/Private/Attributes/MedComAmmoAttributeSet.cpp" "$DST_DIR/Private/Attributes/AmmoAttributeSet.cpp"
migrate "$SRC_DIR/Public/Attributes/MedComArmorAttributeSet.h" "$DST_DIR/Public/Attributes/ArmorAttributeSet.h"
migrate "$SRC_DIR/Private/Attributes/MedComArmorAttributeSet.cpp" "$DST_DIR/Private/Attributes/ArmorAttributeSet.cpp"
migrate "$SRC_DIR/Public/Attributes/MedComDefaultAttributeSet.h" "$DST_DIR/Public/Attributes/DefaultAttributeSet.h"
migrate "$SRC_DIR/Private/Attributes/MedComDefaultAttributeSet.cpp" "$DST_DIR/Private/Attributes/DefaultAttributeSet.cpp"
migrate "$SRC_DIR/Public/Attributes/MedComWeaponAttributeSet.h" "$DST_DIR/Public/Attributes/WeaponAttributeSet.h"
migrate "$SRC_DIR/Private/Attributes/MedComWeaponAttributeSet.cpp" "$DST_DIR/Private/Attributes/WeaponAttributeSet.cpp"

# Migrate Components
migrate "$SRC_DIR/Public/Components/MedComAbilitySystemComponent.h" "$DST_DIR/Public/Components/GASAbilitySystemComponent.h"
migrate "$SRC_DIR/Private/Components/MedComAbilitySystemComponent.cpp" "$DST_DIR/Private/Components/GASAbilitySystemComponent.cpp"

# Migrate Effects
migrate "$SRC_DIR/Public/Effects/MedComGameplayEffect.h" "$DST_DIR/Public/Effects/GASEffect.h"
migrate "$SRC_DIR/Private/Effects/MedComGameplayEffect.cpp" "$DST_DIR/Private/Effects/GASEffect.cpp"
migrate "$SRC_DIR/Public/Effects/MedComGameplayEffect_CrouchDebuff.h" "$DST_DIR/Public/Effects/GameplayEffect_CrouchDebuff.h"
migrate "$SRC_DIR/Private/Effects/MedComGameplayEffect_CrouchDebuff.cpp" "$DST_DIR/Private/Effects/GameplayEffect_CrouchDebuff.cpp"
migrate "$SRC_DIR/Public/Effects/MedComGameplayEffect_HealthRegen.h" "$DST_DIR/Public/Effects/GameplayEffect_HealthRegen.h"
migrate "$SRC_DIR/Private/Effects/MedComGameplayEffect_HealthRegen.cpp" "$DST_DIR/Private/Effects/GameplayEffect_HealthRegen.cpp"
migrate "$SRC_DIR/Public/Effects/MedComGameplayEffect_JumpCost.h" "$DST_DIR/Public/Effects/GameplayEffect_JumpCost.h"
migrate "$SRC_DIR/Private/Effects/MedComGameplayEffect_JumpCost.cpp" "$DST_DIR/Private/Effects/GameplayEffect_JumpCost.cpp"
migrate "$SRC_DIR/Public/Effects/MedComGameplayEffect_SprintBuff.h" "$DST_DIR/Public/Effects/GameplayEffect_SprintBuff.h"
migrate "$SRC_DIR/Private/Effects/MedComGameplayEffect_SprintBuff.cpp" "$DST_DIR/Private/Effects/GameplayEffect_SprintBuff.cpp"
migrate "$SRC_DIR/Public/Effects/MedComGameplayEffect_SprintCost.h" "$DST_DIR/Public/Effects/GameplayEffect_SprintCost.h"
migrate "$SRC_DIR/Private/Effects/MedComGameplayEffect_SprintCost.cpp" "$DST_DIR/Private/Effects/GameplayEffect_SprintCost.cpp"
migrate "$SRC_DIR/Public/Effects/MedComGameplayEffect_StaminaRegen.h" "$DST_DIR/Public/Effects/GameplayEffect_StaminaRegen.h"
migrate "$SRC_DIR/Private/Effects/MedComGameplayEffect_StaminaRegen.cpp" "$DST_DIR/Private/Effects/GameplayEffect_StaminaRegen.cpp"
migrate "$SRC_DIR/Public/Effects/MedComInitialAttributesEffect.h" "$DST_DIR/Public/Effects/InitialAttributesEffect.h"
migrate "$SRC_DIR/Private/Effects/MedComInitialAttributesEffect.cpp" "$DST_DIR/Private/Effects/InitialAttributesEffect.cpp"

# Migrate Subsystems
migrate "$SRC_DIR/Public/Subsystems/MedComWeaponAnimationSubsystem.h" "$DST_DIR/Public/Subsystems/WeaponAnimationSubsystem.h"
migrate "$SRC_DIR/Private/Subsystems/MedComWeaponAnimationSubsystem.cpp" "$DST_DIR/Private/Subsystems/WeaponAnimationSubsystem.cpp"

echo "✅ Migration complete!"
echo "Files migrated: 46 files (23 headers + 23 cpp)"
echo ""
echo "Note: GAS.Build.cs wrapper already exists and doesn't need updating"
