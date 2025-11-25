#!/bin/bash

# Fixup script: Update BridgeSystem interface/class references in Equipment Stage 1 files
# These interfaces were migrated from MedComShared → BridgeSystem

set -e

echo "Fixing BridgeSystem interface references in EquipmentSystem Stage 1 files..."

DST_DIR="Source/EquipmentSystem"

# Function to update file
fixup() {
    local file="$1"
    echo "Fixing: $(basename $file)"

    # Interface implementations (drop "Interface" suffix from BridgeSystem interfaces)
    sed -i 's/\bIMedComEquipmentInterface\b/ISuspenseEquipment/g' "$file"
    sed -i 's/\bIMedComWeaponInterface\b/ISuspenseWeapon/g' "$file"
    sed -i 's/\bIMedComFireModeProviderInterface\b/ISuspenseFireModeProvider/g' "$file"
    sed -i 's/\bIMedComInventoryInterface\b/ISuspenseInventory/g' "$file"
    sed -i 's/\bIMedComLoadoutInterface\b/ISuspenseLoadout/g' "$file"
    sed -i 's/\bIMedComStorageInterface\b/ISuspenseStorage/g' "$file"
    sed -i 's/\bIMedComNetworkDispatcher\b/ISuspenseNetworkDispatcher/g' "$file"
    sed -i 's/\bIMedComPredictionManager\b/ISuspensePredictionManager/g' "$file"
    sed -i 's/\bIMedComReplicationProvider\b/ISuspenseReplicationProvider/g' "$file"
    sed -i 's/\bIMedComEquipmentDataProvider\b/ISuspenseEquipmentDataProvider/g' "$file"
    sed -i 's/\bIMedComEquipmentOperations\b/ISuspenseEquipmentOperations/g' "$file"
    sed -i 's/\bIMedComEquipmentValidation\b/ISuspenseEquipmentValidation/g' "$file"
    sed -i 's/\bIMedComEquipmentVisualization\b/ISuspenseEquipmentVisualization/g' "$file"
    sed -i 's/\bIMedComAbilityIntegration\b/ISuspenseAbilityIntegration/g' "$file"
    sed -i 's/\bIMedComTransactionManager\b/ISuspenseTransactionManager/g' "$file"
    sed -i 's/\bIMedComEquipmentRules\b/ISuspenseEquipmentRules/g' "$file"
    sed -i 's/\bIMedComActorFactory\b/ISuspenseActorFactory/g' "$file"
    sed -i 's/\bIMedComAbilityProvider\b/ISuspenseAbilityProvider/g' "$file"
    sed -i 's/\bIMedComWeaponAnimationInterface\b/ISuspenseWeaponAnimation/g' "$file"
    sed -i 's/\bIMedComInventoryBridge\b/ISuspenseInventoryBridge/g' "$file"
    sed -i 's/\bIMedComWeaponStateProvider\b/ISuspenseWeaponStateProvider/g' "$file"
    sed -i 's/\bIMedComEventDispatcher\b/ISuspenseEventDispatcher/g' "$file"
    sed -i 's/\bIMedComSlotValidator\b/ISuspenseSlotValidator/g' "$file"
    sed -i 's/\bIMedComItemDataProvider\b/ISuspenseItemDataProvider/g' "$file"

    # UINTERFACE wrappers (keep "Interface" suffix)
    sed -i 's/\bUMedComEquipmentInterface\b/USuspenseEquipmentInterface/g' "$file"
    sed -i 's/\bUMedComWeaponInterface\b/USuspenseWeaponInterface/g' "$file"
    sed -i 's/\bUMedComFireModeProviderInterface\b/USuspenseFireModeProviderInterface/g' "$file"
    sed -i 's/\bUMedComInventoryInterface\b/USuspenseInventoryInterface/g' "$file"
    sed -i 's/\bUMedComLoadoutInterface\b/USuspenseLoadoutInterface/g' "$file"
    sed -i 's/\bUMedComStorageInterface\b/USuspenseStorageInterface/g' "$file"

    # BridgeSystem classes
    sed -i 's/\bUMedComItemManager\b/USuspenseItemManager/g' "$file"
    sed -i 's/\bUEventDelegateManager\b/USuspenseEventManager/g' "$file"
    sed -i 's/\bUMedComItemSystemAccess\b/USuspenseItemSystemAccess/g' "$file"
    sed -i 's/\bUMedComEquipmentNetworkDispatcher\b/USuspenseEquipmentNetworkDispatcher/g' "$file"
    sed -i 's/\bUMedComEquipmentPredictionSystem\b/USuspenseEquipmentPredictionSystem/g' "$file"
    sed -i 's/\bUMedComEquipmentReplicationManager\b/USuspenseEquipmentReplicationManager/g' "$file"
    sed -i 's/\bUMedComReplicationProvider\b/USuspenseReplicationProviderInterface/g' "$file"

    # Equipment module component classes (forward declarations for classes migrated in later stages)
    sed -i 's/\bUMedComEquipmentAbilityConnector\b/USuspenseEquipmentAbilityConnector/g' "$file"
    sed -i 's/\bUMedComEquipmentDataStore\b/USuspenseEquipmentDataStore/g' "$file"
    sed -i 's/\bUMedComEquipmentTransactionProcessor\b/USuspenseEquipmentTransactionProcessor/g' "$file"
    sed -i 's/\bUMedComEquipmentSlotValidator\b/USuspenseEquipmentSlotValidator/g' "$file"
    sed -i 's/\bUMedComSystemCoordinator\b/USuspenseSystemCoordinatorComponent/g' "$file"

    # BridgeSystem structs
    sed -i 's/\bFMedComUnifiedItemData\b/FSuspenseUnifiedItemData/g' "$file"
    sed -i 's/\bFInventoryItemInstance\b/FSuspenseInventoryItemInstance/g' "$file"
    sed -i 's/\bFInventoryOperationResult\b/FSuspenseInventoryOperationResult/g' "$file"

    # File includes - Interface files (drop "Interface" from filename, add BridgeSystem/ prefix for clarity in comments)
    sed -i 's|Interfaces/Equipment/IMedComEquipmentInterface\.h|Interfaces/Equipment/ISuspenseEquipment.h|g' "$file"
    sed -i 's|Interfaces/Equipment/IMedComNetworkDispatcher\.h|Interfaces/Equipment/ISuspenseNetworkDispatcher.h|g' "$file"
    sed -i 's|Interfaces/Equipment/IMedComPredictionManager\.h|Interfaces/Equipment/ISuspensePredictionManager.h|g' "$file"
    sed -i 's|Interfaces/Equipment/IMedComReplicationProvider\.h|Interfaces/Equipment/ISuspenseReplicationProvider.h|g' "$file"
    sed -i 's|Interfaces/Equipment/IMedComEquipmentDataProvider\.h|Interfaces/Equipment/ISuspenseEquipmentDataProvider.h|g' "$file"
    sed -i 's|Interfaces/Equipment/IMedComEquipmentOperations\.h|Interfaces/Equipment/ISuspenseEquipmentOperations.h|g' "$file"
    sed -i 's|Interfaces/Equipment/IMedComEquipmentValidation\.h|Interfaces/Equipment/ISuspenseEquipmentValidation.h|g' "$file"
    sed -i 's|Interfaces/Equipment/IMedComEquipmentVisualization\.h|Interfaces/Equipment/ISuspenseEquipmentVisualization.h|g' "$file"
    sed -i 's|Interfaces/Equipment/IMedComAbilityIntegration\.h|Interfaces/Equipment/ISuspenseAbilityIntegration.h|g' "$file"
    sed -i 's|Interfaces/Equipment/IMedComTransactionManager\.h|Interfaces/Equipment/ISuspenseTransactionManager.h|g' "$file"
    sed -i 's|Interfaces/Equipment/IMedComEquipmentRules\.h|Interfaces/Equipment/ISuspenseEquipmentRules.h|g' "$file"
    sed -i 's|Interfaces/Equipment/IMedComActorFactory\.h|Interfaces/Equipment/ISuspenseActorFactory.h|g' "$file"
    sed -i 's|Interfaces/Equipment/IMedComInventoryBridge\.h|Interfaces/Equipment/ISuspenseInventoryBridge.h|g' "$file"
    sed -i 's|Interfaces/Equipment/IMedComWeaponStateProvider\.h|Interfaces/Equipment/ISuspenseWeaponStateProvider.h|g' "$file"
    sed -i 's|Interfaces/Equipment/IMedComSlotValidator\.h|Interfaces/Equipment/ISuspenseSlotValidator.h|g' "$file"
    sed -i 's|Interfaces/Weapon/IMedComWeaponInterface\.h|Interfaces/Weapon/ISuspenseWeapon.h|g' "$file"
    sed -i 's|Interfaces/Weapon/IMedComFireModeProviderInterface\.h|Interfaces/Weapon/ISuspenseFireModeProvider.h|g' "$file"
    sed -i 's|Interfaces/Weapon/IMedComWeaponAnimationInterface\.h|Interfaces/Weapon/ISuspenseWeaponAnimation.h|g' "$file"
    sed -i 's|Interfaces/Abilities/IMedComAbilityProvider\.h|Interfaces/Abilities/ISuspenseAbilityProvider.h|g' "$file"
    sed -i 's|Interfaces/Core/IMedComEventDispatcher\.h|Interfaces/Core/ISuspenseEventDispatcher.h|g' "$file"
    sed -i 's|Interfaces/Inventory/IMedComInventoryInterface\.h|Interfaces/Inventory/ISuspenseInventory.h|g' "$file"
    sed -i 's|Interfaces/Loadout/IMedComLoadoutInterface\.h|Interfaces/Loadout/ISuspenseLoadout.h|g' "$file"
    sed -i 's|Interfaces/Storage/IMedComStorageInterface\.h|Interfaces/Storage/ISuspenseStorage.h|g' "$file"

    # ItemSystem includes
    sed -i 's|ItemSystem/MedComItemManager\.h|ItemSystem/SuspenseItemManager.h|g' "$file"
    sed -i 's|ItemSystem/MedComItemSystemAccess\.h|ItemSystem/SuspenseItemSystemAccess.h|g' "$file"

    # Type includes
    sed -i 's|Types/Inventory/FInventoryItemInstance\.h|Types/Inventory/FSuspenseInventoryItemInstance.h|g' "$file"
    sed -i 's|Types/Inventory/FInventoryOperationResult\.h|Types/Inventory/FSuspenseInventoryOperationResult.h|g' "$file"
    sed -i 's|Types/Loadout/MedComItemDataTable\.h|Types/Loadout/SuspenseItemDataTable.h|g' "$file"
}

# Fix all Equipment files (Stage 1 + Stage 2)
for file in $(find "$DST_DIR/Public/Base" "$DST_DIR/Private/Base" "$DST_DIR/Public/Services" "$DST_DIR/Private/Services" "$DST_DIR/Public/Subsystems" "$DST_DIR/Private/Subsystems" "$DST_DIR/Public/Components" "$DST_DIR/Private/Components" -name "*.h" -o -name "*.cpp" 2>/dev/null); do
    fixup "$file"
done

echo ""
echo "✅ BridgeSystem references fixed in Equipment files!"
