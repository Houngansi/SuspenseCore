#!/bin/bash

# MedComShared → BridgeSystem Migration Script
# Date: 2025-11-25
# Migrates MedComShared files directly into BridgeSystem module
# Following ModuleStructureGuidelines.md and SuspenseNamingConvention.md

set -e

echo "Starting MedComShared → BridgeSystem migration..."

# Source and destination
SRC_DIR="Source/BridgeSystem/MedComShared"
DST_DIR="Source/BridgeSystem"

# Create directory structure in BridgeSystem
mkdir -p "$DST_DIR/Private/Abilities/Inventory"
mkdir -p "$DST_DIR/Private/Core/Services"
mkdir -p "$DST_DIR/Private/Core/Utils"
mkdir -p "$DST_DIR/Private/Delegates"
mkdir -p "$DST_DIR/Private/Interfaces/Abilities"
mkdir -p "$DST_DIR/Private/Interfaces/Core"
mkdir -p "$DST_DIR/Private/Interfaces/Equipment"
mkdir -p "$DST_DIR/Private/Interfaces/Interaction"
mkdir -p "$DST_DIR/Private/Interfaces/Inventory"
mkdir -p "$DST_DIR/Private/Interfaces/UI"
mkdir -p "$DST_DIR/Private/Interfaces/Weapon"
mkdir -p "$DST_DIR/Private/ItemSystem"
mkdir -p "$DST_DIR/Private/Types/Animation"
mkdir -p "$DST_DIR/Private/Types/Inventory"
mkdir -p "$DST_DIR/Private/Types/Loadout"

mkdir -p "$DST_DIR/Public/Abilities/Inventory"
mkdir -p "$DST_DIR/Public/Core/Services"
mkdir -p "$DST_DIR/Public/Core/Utils"
mkdir -p "$DST_DIR/Public/Delegates"
mkdir -p "$DST_DIR/Public/Input"
mkdir -p "$DST_DIR/Public/Interfaces/Abilities"
mkdir -p "$DST_DIR/Public/Interfaces/Core"
mkdir -p "$DST_DIR/Public/Interfaces/Equipment"
mkdir -p "$DST_DIR/Public/Interfaces/Interaction"
mkdir -p "$DST_DIR/Public/Interfaces/Inventory"
mkdir -p "$DST_DIR/Public/Interfaces/Screens"
mkdir -p "$DST_DIR/Public/Interfaces/Tabs"
mkdir -p "$DST_DIR/Public/Interfaces/UI"
mkdir -p "$DST_DIR/Public/Interfaces/Weapon"
mkdir -p "$DST_DIR/Public/ItemSystem"
mkdir -p "$DST_DIR/Public/Operations"
mkdir -p "$DST_DIR/Public/Types/Animation"
mkdir -p "$DST_DIR/Public/Types/Equipment"
mkdir -p "$DST_DIR/Public/Types/Events"
mkdir -p "$DST_DIR/Public/Types/Inventory"
mkdir -p "$DST_DIR/Public/Types/Loadout"
mkdir -p "$DST_DIR/Public/Types/Network"
mkdir -p "$DST_DIR/Public/Types/Rules"
mkdir -p "$DST_DIR/Public/Types/Transaction"
mkdir -p "$DST_DIR/Public/Types/UI"
mkdir -p "$DST_DIR/Public/Types/Weapon"

# Function to migrate and transform file
migrate() {
    local src_file="$1"
    local dst_file="$2"

    echo "Migrating: $(basename $src_file)"

    cp "$src_file" "$dst_file"

    # Module name changes (MedComShared → BridgeSystem)
    sed -i 's/MedComShared\.h/BridgeSystem_Module.h/g' "$dst_file"
    sed -i 's/MedComShared\.cpp/BridgeSystem_Module.cpp/g' "$dst_file"
    sed -i 's/FMedComSharedModule/FBridgeSystemModule/g' "$dst_file"
    sed -i 's/MEDCOMSHARED_API/BRIDGESYSTEM_API/g' "$dst_file"

    # ============================================
    # INTERFACES - CRITICAL TWO-STEP TRANSFORMATION!
    # ============================================
    # Step 1: Transform UINTERFACE wrappers (keep "Interface" suffix)
    # Pattern: class UMedComXXXInterface : public UInterface
    sed -i 's/\bUMedComInventoryInterface\b/USuspenseInventoryInterface/g' "$dst_file"
    sed -i 's/\bUMedComInventoryItemInterface\b/USuspenseInventoryItemInterface/g' "$dst_file"
    sed -i 's/\bUMedComEquipmentInterface\b/USuspenseEquipmentInterface/g' "$dst_file"
    sed -i 's/\bUMedComInteractInterface\b/USuspenseInteractInterface/g' "$dst_file"
    sed -i 's/\bUMedComPickupInterface\b/USuspensePickupInterface/g' "$dst_file"
    sed -i 's/\bUMedComItemFactoryInterface\b/USuspenseItemFactoryInterface/g' "$dst_file"
    sed -i 's/\bUMedComWeaponInterface\b/USuspenseWeaponInterface/g' "$dst_file"
    sed -i 's/\bUMedComCharacterInterface\b/USuspenseCharacterInterface/g' "$dst_file"
    sed -i 's/\bUMedComControllerInterface\b/USuspenseControllerInterface/g' "$dst_file"
    sed -i 's/\bUMedComEnemyInterface\b/USuspenseEnemyInterface/g' "$dst_file"
    sed -i 's/\bUMedComMovementInterface\b/USuspenseMovementInterface/g' "$dst_file"
    sed -i 's/\bUMedComAttributeProviderInterface\b/USuspenseAttributeProviderInterface/g' "$dst_file"
    sed -i 's/\bUMedComPropertyAccessInterface\b/USuspensePropertyAccessInterface/g' "$dst_file"
    sed -i 's/\bUMedComLoadoutInterface\b/USuspenseLoadoutInterface/g' "$dst_file"
    sed -i 's/\bUMedComAbilityProvider\b/USuspenseAbilityProvider/g' "$dst_file"
    sed -i 's/\bUMedComItemDefinitionInterface\b/USuspenseItemDefinitionInterface/g' "$dst_file"
    sed -i 's/\bUMedComWeaponAnimationInterface\b/USuspenseWeaponAnimationInterface/g' "$dst_file"
    sed -i 's/\bUMedComFireModeProviderInterface\b/USuspenseFireModeProviderInterface/g' "$dst_file"

    # UI Interfaces
    sed -i 's/\bUMedComUIWidgetInterface\b/USuspenseUIWidgetInterface/g' "$dst_file"
    sed -i 's/\bUMedComCrosshairWidgetInterface\b/USuspenseCrosshairWidgetInterface/g' "$dst_file"
    sed -i 's/\bUMedComInventoryUIBridgeWidget\b/USuspenseInventoryUIBridgeWidget/g' "$dst_file"
    sed -i 's/\bUMedComContainerUIInterface\b/USuspenseContainerUIInterface/g' "$dst_file"
    sed -i 's/\bUMedComHUDWidgetInterface\b/USuspenseHUDWidgetInterface/g' "$dst_file"
    sed -i 's/\bUMedComWeaponUIWidgetInterface\b/USuspenseWeaponUIWidgetInterface/g' "$dst_file"
    sed -i 's/\bUMedComHealthStaminaWidgetInterface\b/USuspenseHealthStaminaWidgetInterface/g' "$dst_file"
    sed -i 's/\bUMedComEquipmentUIBridgeWidget\b/USuspenseEquipmentUIBridgeWidget/g' "$dst_file"
    sed -i 's/\bUMedComNotificationWidgetInterface\b/USuspenseNotificationWidgetInterface/g' "$dst_file"
    sed -i 's/\bUMedComLayoutInterface\b/USuspenseLayoutInterface/g' "$dst_file"
    sed -i 's/\bUMedComTooltipSourceInterface\b/USuspenseTooltipSourceInterface/g' "$dst_file"
    sed -i 's/\bUMedComDropTargetInterface\b/USuspenseDropTargetInterface/g' "$dst_file"
    sed -i 's/\bUMedComTooltipInterface\b/USuspenseTooltipInterface/g' "$dst_file"
    sed -i 's/\bUMedComDraggableInterface\b/USuspenseDraggableInterface/g' "$dst_file"
    sed -i 's/\bUMedComSlotUIInterface\b/USuspenseSlotUIInterface/g' "$dst_file"
    sed -i 's/\bUMedComScreenInterface\b/USuspenseScreenInterface/g' "$dst_file"
    sed -i 's/\bUMedComTabBarInterface\b/USuspenseTabBarInterface/g' "$dst_file"

    # Equipment service interfaces
    sed -i 's/\bUMedComAbilityConnector\b/USuspenseAbilityConnector/g' "$dst_file"
    sed -i 's/\bUMedComActorFactory\b/USuspenseActorFactory/g' "$dst_file"
    sed -i 's/\bUMedComAttachmentProvider\b/USuspenseAttachmentProvider/g' "$dst_file"
    sed -i 's/\bUMedComEquipmentDataProvider\b/USuspenseEquipmentDataProvider/g' "$dst_file"
    sed -i 's/\bUMedComEquipmentFacade\b/USuspenseEquipmentFacade/g' "$dst_file"
    sed -i 's/\bUMedComEquipmentOperations\b/USuspenseEquipmentOperations/g' "$dst_file"
    sed -i 's/\bUMedComEquipmentOrchestrator\b/USuspenseEquipmentOrchestrator/g' "$dst_file"
    sed -i 's/\bUMedComEquipmentRules\b/USuspenseEquipmentRules/g' "$dst_file"
    sed -i 's/\bUMedComEventDispatcher\b/USuspenseEventDispatcher/g' "$dst_file"
    sed -i 's/\bUMedComInventoryBridge\b/USuspenseInventoryBridge/g' "$dst_file"
    sed -i 's/\bUMedComLoadoutAdapter\b/USuspenseLoadoutAdapter/g' "$dst_file"
    sed -i 's/\bUMedComNetworkDispatcher\b/USuspenseNetworkDispatcher/g' "$dst_file"
    sed -i 's/\bUMedComPredictionManager\b/USuspensePredictionManager/g' "$dst_file"
    sed -i 's/\bUMedComReplicationProvider\b/USuspenseReplicationProvider/g' "$dst_file"
    sed -i 's/\bUMedComSlotValidator\b/USuspenseSlotValidator/g' "$dst_file"
    sed -i 's/\bUMedComTransactionManager\b/USuspenseTransactionManager/g' "$dst_file"
    sed -i 's/\bUMedComVisualProvider\b/USuspenseVisualProvider/g' "$dst_file"
    sed -i 's/\bUMedComWeaponStateProvider\b/USuspenseWeaponStateProvider/g' "$dst_file"

    # Step 2: Transform interface implementations (DROP "Interface" suffix)
    # Pattern: class IMedComXXXInterface (the actual interface, not UINTERFACE)
    sed -i 's/\bIMedComInventoryInterface\b/ISuspenseInventory/g' "$dst_file"
    sed -i 's/\bIMedComInventoryItemInterface\b/ISuspenseInventoryItem/g' "$dst_file"
    sed -i 's/\bIMedComEquipmentInterface\b/ISuspenseEquipment/g' "$dst_file"
    sed -i 's/\bIMedComInteractInterface\b/ISuspenseInteract/g' "$dst_file"
    sed -i 's/\bIMedComPickupInterface\b/ISuspensePickup/g' "$dst_file"
    sed -i 's/\bIMedComItemFactoryInterface\b/ISuspenseItemFactory/g' "$dst_file"
    sed -i 's/\bIMedComWeaponInterface\b/ISuspenseWeapon/g' "$dst_file"
    sed -i 's/\bIMedComCharacterInterface\b/ISuspenseCharacter/g' "$dst_file"
    sed -i 's/\bIMedComControllerInterface\b/ISuspenseController/g' "$dst_file"
    sed -i 's/\bIMedComEnemyInterface\b/ISuspenseEnemy/g' "$dst_file"
    sed -i 's/\bIMedComMovementInterface\b/ISuspenseMovement/g' "$dst_file"
    sed -i 's/\bIMedComAttributeProviderInterface\b/ISuspenseAttributeProvider/g' "$dst_file"
    sed -i 's/\bIMedComPropertyAccessInterface\b/ISuspensePropertyAccess/g' "$dst_file"
    sed -i 's/\bIMedComLoadoutInterface\b/ISuspenseLoadout/g' "$dst_file"
    sed -i 's/\bIMedComAbilityProvider\b/ISuspenseAbilityProvider/g' "$dst_file"
    sed -i 's/\bIMedComItemDefinitionInterface\b/ISuspenseItemDefinition/g' "$dst_file"
    sed -i 's/\bIMedComWeaponAnimationInterface\b/ISuspenseWeaponAnimation/g' "$dst_file"
    sed -i 's/\bIMedComFireModeProviderInterface\b/ISuspenseFireModeProvider/g' "$dst_file"

    # UI Interfaces (drop Interface)
    sed -i 's/\bIMedComUIWidgetInterface\b/ISuspenseUIWidget/g' "$dst_file"
    sed -i 's/\bIMedComCrosshairWidgetInterface\b/ISuspenseCrosshairWidget/g' "$dst_file"
    sed -i 's/\bIMedComInventoryUIBridgeWidget\b/ISuspenseInventoryUIBridge/g' "$dst_file"
    sed -i 's/\bIMedComContainerUIInterface\b/ISuspenseContainerUI/g' "$dst_file"
    sed -i 's/\bIMedComHUDWidgetInterface\b/ISuspenseHUDWidget/g' "$dst_file"
    sed -i 's/\bIMedComWeaponUIWidgetInterface\b/ISuspenseWeaponUIWidget/g' "$dst_file"
    sed -i 's/\bIMedComHealthStaminaWidgetInterface\b/ISuspenseHealthStaminaWidget/g' "$dst_file"
    sed -i 's/\bIMedComEquipmentUIBridgeWidget\b/ISuspenseEquipmentUIBridge/g' "$dst_file"
    sed -i 's/\bIMedComNotificationWidgetInterface\b/ISuspenseNotificationWidget/g' "$dst_file"
    sed -i 's/\bIMedComLayoutInterface\b/ISuspenseLayout/g' "$dst_file"
    sed -i 's/\bIMedComTooltipSourceInterface\b/ISuspenseTooltipSource/g' "$dst_file"
    sed -i 's/\bIMedComDropTargetInterface\b/ISuspenseDropTarget/g' "$dst_file"
    sed -i 's/\bIMedComTooltipInterface\b/ISuspenseTooltip/g' "$dst_file"
    sed -i 's/\bIMedComDraggableInterface\b/ISuspenseDraggable/g' "$dst_file"
    sed -i 's/\bIMedComSlotUIInterface\b/ISuspenseSlotUI/g' "$dst_file"
    sed -i 's/\bIMedComScreenInterface\b/ISuspenseScreen/g' "$dst_file"
    sed -i 's/\bIMedComTabBarInterface\b/ISuspenseTabBar/g' "$dst_file"

    # Equipment service interfaces (drop Interface if present, otherwise just change prefix)
    sed -i 's/\bIEquipmentService\b/ISuspenseEquipmentService/g' "$dst_file"
    sed -i 's/\bIMedComAbilityConnector\b/ISuspenseAbilityConnector/g' "$dst_file"
    sed -i 's/\bIMedComActorFactory\b/ISuspenseActorFactory/g' "$dst_file"
    sed -i 's/\bIMedComAttachmentProvider\b/ISuspenseAttachmentProvider/g' "$dst_file"
    sed -i 's/\bIMedComEquipmentDataProvider\b/ISuspenseEquipmentDataProvider/g' "$dst_file"
    sed -i 's/\bIMedComEquipmentFacade\b/ISuspenseEquipmentFacade/g' "$dst_file"
    sed -i 's/\bIMedComEquipmentOperations\b/ISuspenseEquipmentOperations/g' "$dst_file"
    sed -i 's/\bIMedComEquipmentOrchestrator\b/ISuspenseEquipmentOrchestrator/g' "$dst_file"
    sed -i 's/\bIMedComEquipmentRules\b/ISuspenseEquipmentRules/g' "$dst_file"
    sed -i 's/\bIMedComEventDispatcher\b/ISuspenseEventDispatcher/g' "$dst_file"
    sed -i 's/\bIMedComInventoryBridge\b/ISuspenseInventoryBridge/g' "$dst_file"
    sed -i 's/\bIMedComLoadoutAdapter\b/ISuspenseLoadoutAdapter/g' "$dst_file"
    sed -i 's/\bIMedComNetworkDispatcher\b/ISuspenseNetworkDispatcher/g' "$dst_file"
    sed -i 's/\bIMedComPredictionManager\b/ISuspensePredictionManager/g' "$dst_file"
    sed -i 's/\bIMedComReplicationProvider\b/ISuspenseReplicationProvider/g' "$dst_file"
    sed -i 's/\bIMedComSlotValidator\b/ISuspenseSlotValidator/g' "$dst_file"
    sed -i 's/\bIMedComTransactionManager\b/ISuspenseTransactionManager/g' "$dst_file"
    sed -i 's/\bIMedComVisualProvider\b/ISuspenseVisualProvider/g' "$dst_file"
    sed -i 's/\bIMedComWeaponStateProvider\b/ISuspenseWeaponStateProvider/g' "$dst_file"

    # ============================================
    # CLASSES (UObjects)
    # ============================================
    sed -i 's/\bUMedComItemManager\b/USuspenseCoreItemManager/g' "$dst_file"
    sed -i 's/\bUEventDelegateManager\b/USuspenseEventManager/g' "$dst_file"
    sed -i 's/\bUMedComInventoryGASIntegration\b/USuspenseInventoryGASIntegration/g' "$dst_file"
    sed -i 's/\bUMedComLoadoutManager\b/USuspenseLoadoutManager/g' "$dst_file"
    sed -i 's/\bUItemSystemAccess\b/USuspenseItemSystemAccess/g' "$dst_file"
    sed -i 's/\bUEquipmentServiceLocator\b/USuspenseEquipmentServiceLocator/g' "$dst_file"
    sed -i 's/\bUMedComWorldBindable\b/USuspenseWorldBindable/g' "$dst_file"

    # ============================================
    # STRUCTS (F prefix)
    # ============================================
    # MedCom-prefixed structs
    sed -i 's/\bFMedComUnifiedItemData\b/FSuspenseCoreUnifiedItemData/g' "$dst_file"
    sed -i 's/\bFMedComItemDataTable\b/FSuspenseItemDataTable/g' "$dst_file"
    sed -i 's/\bFMCEquipmentSlot\b/FSuspenseEquipmentSlot/g' "$dst_file"
    sed -i 's/\bFMedComNetworkTypes\b/FSuspenseNetworkTypes/g' "$dst_file"
    sed -i 's/\bFMedComRulesTypes\b/FSuspenseRulesTypes/g' "$dst_file"

    # Generic structs (add Suspense prefix)
    sed -i 's/\bFInventoryItemInstance\b/FSuspenseInventoryItemInstance/g' "$dst_file"
    sed -i 's/\bFPickupSpawnData\b/FSuspensePickupSpawnData/g' "$dst_file"
    sed -i 's/\bFInventoryOperationResult\b/FSuspenseInventoryOperationResult/g' "$dst_file"
    sed -i 's/\bFInventoryConfig\b/FSuspenseInventoryConfig/g' "$dst_file"
    sed -i 's/\bFInventoryTypeRegistry\b/FSuspenseInventoryTypeRegistry/g' "$dst_file"
    sed -i 's/\bFInventoryAmmoState\b/FSuspenseInventoryAmmoState/g' "$dst_file"
    sed -i 's/\bFEquipmentEventBus\b/FSuspenseEquipmentEventBus/g' "$dst_file"
    sed -i 's/\bFEquipmentThreadGuard\b/FSuspenseEquipmentThreadGuard/g' "$dst_file"
    sed -i 's/\bFEquipmentCacheManager\b/FSuspenseEquipmentCacheManager/g' "$dst_file"
    sed -i 's/\bFGlobalCacheRegistry\b/FSuspenseGlobalCacheRegistry/g' "$dst_file"
    sed -i 's/\bFAnimationStateStruct\b/FSuspenseAnimationState/g' "$dst_file"
    sed -i 's/\bFEquipmentEventData\b/FSuspenseEquipmentEventData/g' "$dst_file"
    sed -i 's/\bFLoadoutSettings\b/FSuspenseLoadoutSettings/g' "$dst_file"
    sed -i 's/\bFTransactionTypes\b/FSuspenseTransactionTypes/g' "$dst_file"
    sed -i 's/\bFContainerUITypes\b/FSuspenseContainerUITypes/g' "$dst_file"
    sed -i 's/\bFEquipmentUITypes\b/FSuspenseEquipmentUITypes/g' "$dst_file"
    sed -i 's/\bFBaseItemTypes\b/FSuspenseBaseItemTypes/g' "$dst_file"
    sed -i 's/\bFEquipmentTypes\b/FSuspenseEquipmentTypes/g' "$dst_file"
    sed -i 's/\bFEquipmentVisualizationTypes\b/FSuspenseEquipmentVisualizationTypes/g' "$dst_file"
    sed -i 's/\bFWeaponTypes\b/FSuspenseWeaponTypes/g' "$dst_file"

    # ============================================
    # ENUMS
    # ============================================
    sed -i 's/\bEMCAbilityInputID\b/ESuspenseAbilityInputID/g' "$dst_file"
    sed -i 's/\bEInventoryErrorCode\b/ESuspenseInventoryErrorCode/g' "$dst_file"

    # ============================================
    # DELEGATES
    # ============================================
    sed -i 's/\bFOnInventoryUpdated\b/FSuspenseOnInventoryUpdated/g' "$dst_file"

    # ============================================
    # FILE INCLUDES
    # ============================================
    # Module
    sed -i 's/MedComShared\.h/BridgeSystem_Module.h/g' "$dst_file"

    # Interfaces (with new naming - drop Interface from filename)
    sed -i 's/IMedComInventoryInterface\.h/ISuspenseInventory.h/g' "$dst_file"
    sed -i 's/IMedComInventoryItemInterface\.h/ISuspenseInventoryItem.h/g' "$dst_file"
    sed -i 's/IMedComEquipmentInterface\.h/ISuspenseEquipment.h/g' "$dst_file"
    sed -i 's/IMedComInteractInterface\.h/ISuspenseInteract.h/g' "$dst_file"
    sed -i 's/IMedComPickupInterface\.h/ISuspensePickup.h/g' "$dst_file"
    sed -i 's/IMedComItemFactoryInterface\.h/ISuspenseItemFactory.h/g' "$dst_file"
    sed -i 's/IMedComWeaponInterface\.h/ISuspenseWeapon.h/g' "$dst_file"
    sed -i 's/IMedComCharacterInterface\.h/ISuspenseCharacter.h/g' "$dst_file"
    sed -i 's/IMedComControllerInterface\.h/ISuspenseController.h/g' "$dst_file"
    sed -i 's/IMedComEnemyInterface\.h/ISuspenseEnemy.h/g' "$dst_file"
    sed -i 's/IMedComMovementInterface\.h/ISuspenseMovement.h/g' "$dst_file"
    sed -i 's/IMedComAttributeProviderInterface\.h/ISuspenseAttributeProvider.h/g' "$dst_file"
    sed -i 's/IMedComPropertyAccessInterface\.h/ISuspensePropertyAccess.h/g' "$dst_file"
    sed -i 's/IMedComLoadoutInterface\.h/ISuspenseLoadout.h/g' "$dst_file"
    sed -i 's/IMedComAbilityProvider\.h/ISuspenseAbilityProvider.h/g' "$dst_file"
    sed -i 's/IMedComItemDefinitionInterface\.h/ISuspenseItemDefinition.h/g' "$dst_file"
    sed -i 's/IMedComWeaponAnimationInterface\.h/ISuspenseWeaponAnimation.h/g' "$dst_file"
    sed -i 's/IMedComFireModeProviderInterface\.h/ISuspenseFireModeProvider.h/g' "$dst_file"

    # UI Interfaces
    sed -i 's/IMedComUIWidgetInterface\.h/ISuspenseUIWidget.h/g' "$dst_file"
    sed -i 's/IMedComCrosshairWidgetInterface\.h/ISuspenseCrosshairWidget.h/g' "$dst_file"
    sed -i 's/IMedComInventoryUIBridgeWidget\.h/ISuspenseInventoryUIBridge.h/g' "$dst_file"
    sed -i 's/IMedComContainerUIInterface\.h/ISuspenseContainerUI.h/g' "$dst_file"
    sed -i 's/IMedComHUDWidgetInterface\.h/ISuspenseHUDWidget.h/g' "$dst_file"
    sed -i 's/IMedComWeaponUIWidgetInterface\.h/ISuspenseWeaponUIWidget.h/g' "$dst_file"
    sed -i 's/IMedComHealthStaminaWidgetInterface\.h/ISuspenseHealthStaminaWidget.h/g' "$dst_file"
    sed -i 's/IMedComEquipmentUIBridgeWidget\.h/ISuspenseEquipmentUIBridge.h/g' "$dst_file"
    sed -i 's/IMedComNotificationWidgetInterface\.h/ISuspenseNotificationWidget.h/g' "$dst_file"
    sed -i 's/IMedComLayoutInterface\.h/ISuspenseLayout.h/g' "$dst_file"
    sed -i 's/IMedComTooltipSourceInterface\.h/ISuspenseTooltipSource.h/g' "$dst_file"
    sed -i 's/IMedComDropTargetInterface\.h/ISuspenseDropTarget.h/g' "$dst_file"
    sed -i 's/IMedComTooltipInterface\.h/ISuspenseTooltip.h/g' "$dst_file"
    sed -i 's/IMedComDraggableInterface\.h/ISuspenseDraggable.h/g' "$dst_file"
    sed -i 's/IMedComSlotUIInterface\.h/ISuspenseSlotUI.h/g' "$dst_file"
    sed -i 's/IMedComScreenInterface\.h/ISuspenseScreen.h/g' "$dst_file"
    sed -i 's/IMedComTabBarInterface\.h/ISuspenseTabBar.h/g' "$dst_file"

    # Equipment service interfaces
    sed -i 's/IEquipmentService\.h/ISuspenseEquipmentService.h/g' "$dst_file"
    sed -i 's/IMedComAbilityConnector\.h/ISuspenseAbilityConnector.h/g' "$dst_file"
    sed -i 's/IMedComActorFactory\.h/ISuspenseActorFactory.h/g' "$dst_file"
    sed -i 's/IMedComAttachmentProvider\.h/ISuspenseAttachmentProvider.h/g' "$dst_file"
    sed -i 's/IMedComEquipmentDataProvider\.h/ISuspenseEquipmentDataProvider.h/g' "$dst_file"
    sed -i 's/IMedComEquipmentFacade\.h/ISuspenseEquipmentFacade.h/g' "$dst_file"
    sed -i 's/IMedComEquipmentOperations\.h/ISuspenseEquipmentOperations.h/g' "$dst_file"
    sed -i 's/IMedComEquipmentOrchestrator\.h/ISuspenseEquipmentOrchestrator.h/g' "$dst_file"
    sed -i 's/IMedComEquipmentRules\.h/ISuspenseEquipmentRules.h/g' "$dst_file"
    sed -i 's/IMedComEventDispatcher\.h/ISuspenseEventDispatcher.h/g' "$dst_file"
    sed -i 's/IMedComInventoryBridge\.h/ISuspenseInventoryBridge.h/g' "$dst_file"
    sed -i 's/IMedComLoadoutAdapter\.h/ISuspenseLoadoutAdapter.h/g' "$dst_file"
    sed -i 's/IMedComNetworkDispatcher\.h/ISuspenseNetworkDispatcher.h/g' "$dst_file"
    sed -i 's/IMedComPredictionManager\.h/ISuspensePredictionManager.h/g' "$dst_file"
    sed -i 's/IMedComReplicationProvider\.h/ISuspenseReplicationProvider.h/g' "$dst_file"
    sed -i 's/IMedComSlotValidator\.h/ISuspenseSlotValidator.h/g' "$dst_file"
    sed -i 's/IMedComTransactionManager\.h/ISuspenseTransactionManager.h/g' "$dst_file"
    sed -i 's/IMedComVisualProvider\.h/ISuspenseVisualProvider.h/g' "$dst_file"
    sed -i 's/IMedComWeaponStateProvider\.h/ISuspenseWeaponStateProvider.h/g' "$dst_file"

    # Classes
    sed -i 's/MedComItemManager\.h/SuspenseItemManager.h/g' "$dst_file"
    sed -i 's/EventDelegateManager\.h/SuspenseEventManager.h/g' "$dst_file"
    sed -i 's/MedComInventoryGASIntegration\.h/SuspenseInventoryGASIntegration.h/g' "$dst_file"
    sed -i 's/MedComLoadoutManager\.h/SuspenseLoadoutManager.h/g' "$dst_file"
    sed -i 's/ItemSystemAccess\.h/SuspenseItemSystemAccess.h/g' "$dst_file"
    sed -i 's/EquipmentServiceLocator\.h/SuspenseEquipmentServiceLocator.h/g' "$dst_file"
    sed -i 's/MedComWorldBindable\.h/SuspenseWorldBindable.h/g' "$dst_file"

    # Types
    sed -i 's/MedComItemDataTable\.h/SuspenseItemDataTable.h/g' "$dst_file"
    sed -i 's/FMCEquipmentSlot\.h/SuspenseEquipmentSlot.h/g' "$dst_file"
    sed -i 's/MedComNetworkTypes\.h/SuspenseNetworkTypes.h/g' "$dst_file"
    sed -i 's/MedComRulesTypes\.h/SuspenseRulesTypes.h/g' "$dst_file"
    sed -i 's/AnimationStateStruct\.h/SuspenseAnimationState.h/g' "$dst_file"
    sed -i 's/FEquipmentEventBus\.h/SuspenseEquipmentEventBus.h/g' "$dst_file"
    sed -i 's/FEquipmentThreadGuard\.h/SuspenseEquipmentThreadGuard.h/g' "$dst_file"
    sed -i 's/FEquipmentCacheManager\.h/SuspenseEquipmentCacheManager.h/g' "$dst_file"
    sed -i 's/FGlobalCacheRegistry\.h/SuspenseGlobalCacheRegistry.h/g' "$dst_file"
    sed -i 's/MCAbilityInputID\.h/SuspenseAbilityInputID.h/g' "$dst_file"

    # Generic types (keep existing names for now - these are shared types)
    sed -i 's/InventoryTypes\.h/SuspenseInventoryTypes.h/g' "$dst_file"
    sed -i 's/InventoryUtils\.h/SuspenseInventoryUtils.h/g' "$dst_file"
    sed -i 's/InventoryTypeRegistry\.h/SuspenseInventoryTypeRegistry.h/g' "$dst_file"
    sed -i 's/InventoryResult\.h/SuspenseInventoryResult.h/g' "$dst_file"
    sed -i 's/EquipmentTypes\.h/SuspenseEquipmentTypes.h/g' "$dst_file"
    sed -i 's/EquipmentVisualizationTypes\.h/SuspenseEquipmentVisualizationTypes.h/g' "$dst_file"
    sed -i 's/WeaponTypes\.h/SuspenseWeaponTypes.h/g' "$dst_file"
    sed -i 's/BaseItemTypes\.h/SuspenseBaseItemTypes.h/g' "$dst_file"
    sed -i 's/EquipmentEventData\.h/SuspenseEquipmentEventData.h/g' "$dst_file"
    sed -i 's/LoadoutSettings\.h/SuspenseLoadoutSettings.h/g' "$dst_file"
    sed -i 's/TransactionTypes\.h/SuspenseTransactionTypes.h/g' "$dst_file"
    sed -i 's/ContainerUITypes\.h/SuspenseContainerUITypes.h/g' "$dst_file"
    sed -i 's/EquipmentUITypes\.h/SuspenseEquipmentUITypes.h/g' "$dst_file"
    sed -i 's/FInventoryAmmoState\.h/SuspenseInventoryAmmoState.h/g' "$dst_file"

    # ============================================
    # .generated.h INCLUDES
    # ============================================
    # Interfaces
    sed -i 's/IMedComInventoryInterface\.generated\.h/ISuspenseInventory.generated.h/g' "$dst_file"
    sed -i 's/IMedComInventoryItemInterface\.generated\.h/ISuspenseInventoryItem.generated.h/g' "$dst_file"
    sed -i 's/IMedComEquipmentInterface\.generated\.h/ISuspenseEquipment.generated.h/g' "$dst_file"
    sed -i 's/IMedComInteractInterface\.generated\.h/ISuspenseInteract.generated.h/g' "$dst_file"
    sed -i 's/IMedComPickupInterface\.generated\.h/ISuspensePickup.generated.h/g' "$dst_file"
    sed -i 's/IMedComItemFactoryInterface\.generated\.h/ISuspenseItemFactory.generated.h/g' "$dst_file"
    sed -i 's/IMedComWeaponInterface\.generated\.h/ISuspenseWeapon.generated.h/g' "$dst_file"
    sed -i 's/IMedComCharacterInterface\.generated\.h/ISuspenseCharacter.generated.h/g' "$dst_file"
    sed -i 's/IMedComControllerInterface\.generated\.h/ISuspenseController.generated.h/g' "$dst_file"
    sed -i 's/IMedComEnemyInterface\.generated\.h/ISuspenseEnemy.generated.h/g' "$dst_file"
    sed -i 's/IMedComMovementInterface\.generated\.h/ISuspenseMovement.generated.h/g' "$dst_file"
    sed -i 's/IMedComAttributeProviderInterface\.generated\.h/ISuspenseAttributeProvider.generated.h/g' "$dst_file"
    sed -i 's/IMedComPropertyAccessInterface\.generated\.h/ISuspensePropertyAccess.generated.h/g' "$dst_file"
    sed -i 's/IMedComLoadoutInterface\.generated\.h/ISuspenseLoadout.generated.h/g' "$dst_file"
    sed -i 's/IMedComAbilityProvider\.generated\.h/ISuspenseAbilityProvider.generated.h/g' "$dst_file"
    sed -i 's/IMedComItemDefinitionInterface\.generated\.h/ISuspenseItemDefinition.generated.h/g' "$dst_file"
    sed -i 's/IMedComWeaponAnimationInterface\.generated\.h/ISuspenseWeaponAnimation.generated.h/g' "$dst_file"
    sed -i 's/IMedComFireModeProviderInterface\.generated\.h/ISuspenseFireModeProvider.generated.h/g' "$dst_file"

    # UI Interfaces .generated.h
    sed -i 's/IMedComUIWidgetInterface\.generated\.h/ISuspenseUIWidget.generated.h/g' "$dst_file"
    sed -i 's/IMedComCrosshairWidgetInterface\.generated\.h/ISuspenseCrosshairWidget.generated.h/g' "$dst_file"
    sed -i 's/IMedComInventoryUIBridgeWidget\.generated\.h/ISuspenseInventoryUIBridge.generated.h/g' "$dst_file"
    sed -i 's/IMedComContainerUIInterface\.generated\.h/ISuspenseContainerUI.generated.h/g' "$dst_file"
    sed -i 's/IMedComHUDWidgetInterface\.generated\.h/ISuspenseHUDWidget.generated.h/g' "$dst_file"
    sed -i 's/IMedComWeaponUIWidgetInterface\.generated\.h/ISuspenseWeaponUIWidget.generated.h/g' "$dst_file"
    sed -i 's/IMedComHealthStaminaWidgetInterface\.generated\.h/ISuspenseHealthStaminaWidget.generated.h/g' "$dst_file"
    sed -i 's/IMedComEquipmentUIBridgeWidget\.generated\.h/ISuspenseEquipmentUIBridge.generated.h/g' "$dst_file"
    sed -i 's/IMedComNotificationWidgetInterface\.generated\.h/ISuspenseNotificationWidget.generated.h/g' "$dst_file"
    sed -i 's/IMedComLayoutInterface\.generated\.h/ISuspenseLayout.generated.h/g' "$dst_file"
    sed -i 's/IMedComTooltipSourceInterface\.generated\.h/ISuspenseTooltipSource.generated.h/g' "$dst_file"
    sed -i 's/IMedComDropTargetInterface\.generated\.h/ISuspenseDropTarget.generated.h/g' "$dst_file"
    sed -i 's/IMedComTooltipInterface\.generated\.h/ISuspenseTooltip.generated.h/g' "$dst_file"
    sed -i 's/IMedComDraggableInterface\.generated\.h/ISuspenseDraggable.generated.h/g' "$dst_file"
    sed -i 's/IMedComSlotUIInterface\.generated\.h/ISuspenseSlotUI.generated.h/g' "$dst_file"
    sed -i 's/IMedComScreenInterface\.generated\.h/ISuspenseScreen.generated.h/g' "$dst_file"
    sed -i 's/IMedComTabBarInterface\.generated\.h/ISuspenseTabBar.generated.h/g' "$dst_file"

    # Equipment service interfaces .generated.h
    sed -i 's/IEquipmentService\.generated\.h/ISuspenseEquipmentService.generated.h/g' "$dst_file"
    sed -i 's/IMedComAbilityConnector\.generated\.h/ISuspenseAbilityConnector.generated.h/g' "$dst_file"
    sed -i 's/IMedComActorFactory\.generated\.h/ISuspenseActorFactory.generated.h/g' "$dst_file"
    sed -i 's/IMedComAttachmentProvider\.generated\.h/ISuspenseAttachmentProvider.generated.h/g' "$dst_file"
    sed -i 's/IMedComEquipmentDataProvider\.generated\.h/ISuspenseEquipmentDataProvider.generated.h/g' "$dst_file"
    sed -i 's/IMedComEquipmentFacade\.generated\.h/ISuspenseEquipmentFacade.generated.h/g' "$dst_file"
    sed -i 's/IMedComEquipmentOperations\.generated\.h/ISuspenseEquipmentOperations.generated.h/g' "$dst_file"
    sed -i 's/IMedComEquipmentOrchestrator\.generated\.h/ISuspenseEquipmentOrchestrator.generated.h/g' "$dst_file"
    sed -i 's/IMedComEquipmentRules\.generated\.h/ISuspenseEquipmentRules.generated.h/g' "$dst_file"
    sed -i 's/IMedComEventDispatcher\.generated\.h/ISuspenseEventDispatcher.generated.h/g' "$dst_file"
    sed -i 's/IMedComInventoryBridge\.generated\.h/ISuspenseInventoryBridge.generated.h/g' "$dst_file"
    sed -i 's/IMedComLoadoutAdapter\.generated\.h/ISuspenseLoadoutAdapter.generated.h/g' "$dst_file"
    sed -i 's/IMedComNetworkDispatcher\.generated\.h/ISuspenseNetworkDispatcher.generated.h/g' "$dst_file"
    sed -i 's/IMedComPredictionManager\.generated\.h/ISuspensePredictionManager.generated.h/g' "$dst_file"
    sed -i 's/IMedComReplicationProvider\.generated\.h/ISuspenseReplicationProvider.generated.h/g' "$dst_file"
    sed -i 's/IMedComSlotValidator\.generated\.h/ISuspenseSlotValidator.generated.h/g' "$dst_file"
    sed -i 's/IMedComTransactionManager\.generated\.h/ISuspenseTransactionManager.generated.h/g' "$dst_file"
    sed -i 's/IMedComVisualProvider\.generated\.h/ISuspenseVisualProvider.generated.h/g' "$dst_file"
    sed -i 's/IMedComWeaponStateProvider\.generated\.h/ISuspenseWeaponStateProvider.generated.h/g' "$dst_file"

    # Classes .generated.h
    sed -i 's/MedComItemManager\.generated\.h/SuspenseItemManager.generated.h/g' "$dst_file"
    sed -i 's/EventDelegateManager\.generated\.h/SuspenseEventManager.generated.h/g' "$dst_file"
    sed -i 's/MedComInventoryGASIntegration\.generated\.h/SuspenseInventoryGASIntegration.generated.h/g' "$dst_file"
    sed -i 's/MedComLoadoutManager\.generated\.h/SuspenseLoadoutManager.generated.h/g' "$dst_file"
    sed -i 's/ItemSystemAccess\.generated\.h/SuspenseItemSystemAccess.generated.h/g' "$dst_file"
    sed -i 's/EquipmentServiceLocator\.generated\.h/SuspenseEquipmentServiceLocator.generated.h/g' "$dst_file"
    sed -i 's/MedComWorldBindable\.generated\.h/SuspenseWorldBindable.generated.h/g' "$dst_file"
    sed -i 's/MedComItemDataTable\.generated\.h/SuspenseItemDataTable.generated.h/g' "$dst_file"
    sed -i 's/MCAbilityInputID\.generated\.h/SuspenseAbilityInputID.generated.h/g' "$dst_file"

    # ============================================
    # BLUEPRINT CATEGORIES
    # ============================================
    sed -i 's/Category="MedCom|/Category="SuspenseCore|/g' "$dst_file"
    sed -i 's/Category = "MedCom|/Category = "SuspenseCore|/g' "$dst_file"
    sed -i 's/Category="Inventory/Category="SuspenseCore|Inventory/g' "$dst_file"
    sed -i 's/Category="Equipment/Category="SuspenseCore|Equipment/g' "$dst_file"

    # ============================================
    # COPYRIGHT & COMMENTS
    # ============================================
    sed -i 's/Copyright MedCom Team/Copyright Suspense Team/g' "$dst_file"
    sed -i 's/для MedCom/для Suspense/g' "$dst_file"
    sed -i 's/проекта MedCom/проекта Suspense/g' "$dst_file"
    sed -i 's/MedCom Team/Suspense Team/g' "$dst_file"
}

# Migrate module header/cpp
migrate "$SRC_DIR/Public/MedComShared.h" "$DST_DIR/Public/BridgeSystem_Module.h"
migrate "$SRC_DIR/Private/MedComShared.cpp" "$DST_DIR/Private/BridgeSystem_Module.cpp"

# Migrate Input
migrate "$SRC_DIR/Public/Input/MCAbilityInputID.h" "$DST_DIR/Public/Input/SuspenseAbilityInputID.h"

# Migrate ItemSystem
migrate "$SRC_DIR/Public/ItemSystem/MedComItemManager.h" "$DST_DIR/Public/ItemSystem/SuspenseItemManager.h"
migrate "$SRC_DIR/Private/ItemSystem/MedComItemManager.cpp" "$DST_DIR/Private/ItemSystem/SuspenseItemManager.cpp"
migrate "$SRC_DIR/Public/ItemSystem/ItemSystemAccess.h" "$DST_DIR/Public/ItemSystem/SuspenseItemSystemAccess.h"
migrate "$SRC_DIR/Private/ItemSystem/ItemSystemAccess.cpp" "$DST_DIR/Private/ItemSystem/SuspenseItemSystemAccess.cpp"

# Migrate Abilities/Inventory
migrate "$SRC_DIR/Public/Abilities/Inventory/MedComInventoryGASIntegration.h" "$DST_DIR/Public/Abilities/Inventory/SuspenseInventoryGASIntegration.h"
migrate "$SRC_DIR/Private/Abilities/Inventory/MedComInventoryGASIntegration.cpp" "$DST_DIR/Private/ItemSystem/SuspenseInventoryGASIntegration.cpp"

# Migrate Delegates
migrate "$SRC_DIR/Public/Delegates/EventDelegateManager.h" "$DST_DIR/Public/Delegates/SuspenseEventManager.h"
migrate "$SRC_DIR/Private/Delegates/EventDelegateManager.cpp" "$DST_DIR/Private/Delegates/SuspenseEventManager.cpp"

# Migrate Core/Services
migrate "$SRC_DIR/Public/Core/Services/EquipmentServiceLocator.h" "$DST_DIR/Public/Core/Services/SuspenseEquipmentServiceLocator.h"
migrate "$SRC_DIR/Private/Core/Services/EquipmentServiceLocator.cpp" "$DST_DIR/Private/Core/Services/SuspenseEquipmentServiceLocator.cpp"

# Migrate Core/Utils
migrate "$SRC_DIR/Public/Core/Utils/FEquipmentEventBus.h" "$DST_DIR/Public/Core/Utils/SuspenseEquipmentEventBus.h"
migrate "$SRC_DIR/Private/Core/Utils/FEquipmentEventBus.cpp" "$DST_DIR/Private/Core/Utils/SuspenseEquipmentEventBus.cpp"
migrate "$SRC_DIR/Public/Core/Utils/FEquipmentThreadGuard.h" "$DST_DIR/Public/Core/Utils/SuspenseEquipmentThreadGuard.h"
migrate "$SRC_DIR/Private/Core/Utils/FEquipmentThreadGuard.cpp" "$DST_DIR/Private/Core/Utils/SuspenseEquipmentThreadGuard.cpp"
migrate "$SRC_DIR/Public/Core/Utils/FEquipmentCacheManager.h" "$DST_DIR/Public/Core/Utils/SuspenseEquipmentCacheManager.h"
migrate "$SRC_DIR/Public/Core/Utils/FGlobalCacheRegistry.h" "$DST_DIR/Public/Core/Utils/SuspenseGlobalCacheRegistry.h"
migrate "$SRC_DIR/Private/Core/Utils/FGlobalCacheRegistry.cpp" "$DST_DIR/Private/Core/Utils/SuspenseGlobalCacheRegistry.cpp"

# Migrate ALL Interfaces - Inventory
migrate "$SRC_DIR/Public/Interfaces/Inventory/IMedComInventoryInterface.h" "$DST_DIR/Public/Interfaces/Inventory/ISuspenseInventory.h"
migrate "$SRC_DIR/Private/Interfaces/Inventory/IMedComInventoryInterface.cpp" "$DST_DIR/Private/Interfaces/Inventory/ISuspenseInventory.cpp"
migrate "$SRC_DIR/Public/Interfaces/Inventory/IMedComInventoryItemInterface.h" "$DST_DIR/Public/Interfaces/Inventory/ISuspenseInventoryItem.h"

# Migrate Interfaces - Equipment (all 20 files)
migrate "$SRC_DIR/Public/Interfaces/Equipment/IEquipmentService.h" "$DST_DIR/Public/Interfaces/Equipment/ISuspenseEquipmentService.h"
migrate "$SRC_DIR/Public/Interfaces/Equipment/IMedComAbilityConnector.h" "$DST_DIR/Public/Interfaces/Equipment/ISuspenseAbilityConnector.h"
migrate "$SRC_DIR/Public/Interfaces/Equipment/IMedComActorFactory.h" "$DST_DIR/Public/Interfaces/Equipment/ISuspenseActorFactory.h"
migrate "$SRC_DIR/Public/Interfaces/Equipment/IMedComAttachmentProvider.h" "$DST_DIR/Public/Interfaces/Equipment/ISuspenseAttachmentProvider.h"
migrate "$SRC_DIR/Public/Interfaces/Equipment/IMedComEquipmentDataProvider.h" "$DST_DIR/Public/Interfaces/Equipment/ISuspenseEquipmentDataProvider.h"
migrate "$SRC_DIR/Public/Interfaces/Equipment/IMedComEquipmentFacade.h" "$DST_DIR/Public/Interfaces/Equipment/ISuspenseEquipmentFacade.h"
migrate "$SRC_DIR/Public/Interfaces/Equipment/IMedComEquipmentInterface.h" "$DST_DIR/Public/Interfaces/Equipment/ISuspenseEquipment.h"
migrate "$SRC_DIR/Private/Interfaces/Equipment/IMedComEquipmentInterface.cpp" "$DST_DIR/Private/Interfaces/Equipment/ISuspenseEquipment.cpp"
migrate "$SRC_DIR/Public/Interfaces/Equipment/IMedComEquipmentOperations.h" "$DST_DIR/Public/Interfaces/Equipment/ISuspenseEquipmentOperations.h"
migrate "$SRC_DIR/Public/Interfaces/Equipment/IMedComEquipmentOrchestrator.h" "$DST_DIR/Public/Interfaces/Equipment/ISuspenseEquipmentOrchestrator.h"
migrate "$SRC_DIR/Public/Interfaces/Equipment/IMedComEquipmentRules.h" "$DST_DIR/Public/Interfaces/Equipment/ISuspenseEquipmentRules.h"
migrate "$SRC_DIR/Public/Interfaces/Equipment/IMedComEventDispatcher.h" "$DST_DIR/Public/Interfaces/Equipment/ISuspenseEventDispatcher.h"
migrate "$SRC_DIR/Public/Interfaces/Equipment/IMedComInventoryBridge.h" "$DST_DIR/Public/Interfaces/Equipment/ISuspenseInventoryBridge.h"
migrate "$SRC_DIR/Public/Interfaces/Equipment/IMedComItemDefinitionInterface.h" "$DST_DIR/Public/Interfaces/Equipment/ISuspenseItemDefinition.h"
migrate "$SRC_DIR/Private/Interfaces/Equipment/IMedComItemDefinitionInterface.cpp" "$DST_DIR/Private/Interfaces/Equipment/ISuspenseItemDefinition.cpp"
migrate "$SRC_DIR/Public/Interfaces/Equipment/IMedComLoadoutAdapter.h" "$DST_DIR/Public/Interfaces/Equipment/ISuspenseLoadoutAdapter.h"
migrate "$SRC_DIR/Public/Interfaces/Equipment/IMedComNetworkDispatcher.h" "$DST_DIR/Public/Interfaces/Equipment/ISuspenseNetworkDispatcher.h"
migrate "$SRC_DIR/Public/Interfaces/Equipment/IMedComPredictionManager.h" "$DST_DIR/Public/Interfaces/Equipment/ISuspensePredictionManager.h"
migrate "$SRC_DIR/Public/Interfaces/Equipment/IMedComReplicationProvider.h" "$DST_DIR/Public/Interfaces/Equipment/ISuspenseReplicationProvider.h"
migrate "$SRC_DIR/Public/Interfaces/Equipment/IMedComSlotValidator.h" "$DST_DIR/Public/Interfaces/Equipment/ISuspenseSlotValidator.h"
migrate "$SRC_DIR/Public/Interfaces/Equipment/IMedComTransactionManager.h" "$DST_DIR/Public/Interfaces/Equipment/ISuspenseTransactionManager.h"
migrate "$SRC_DIR/Public/Interfaces/Equipment/IMedComVisualProvider.h" "$DST_DIR/Public/Interfaces/Equipment/ISuspenseVisualProvider.h"
migrate "$SRC_DIR/Public/Interfaces/Equipment/IMedComWeaponStateProvider.h" "$DST_DIR/Public/Interfaces/Equipment/ISuspenseWeaponStateProvider.h"

# Migrate Interfaces - Interaction
migrate "$SRC_DIR/Public/Interfaces/Interaction/IMedComInteractInterface.h" "$DST_DIR/Public/Interfaces/Interaction/ISuspenseInteract.h"
migrate "$SRC_DIR/Private/Interfaces/Interaction/IMedComInteractInterface.cpp" "$DST_DIR/Private/Interfaces/Interaction/ISuspenseInteract.cpp"
migrate "$SRC_DIR/Public/Interfaces/Interaction/IMedComItemFactoryInterface.h" "$DST_DIR/Public/Interfaces/Interaction/ISuspenseItemFactory.h"
migrate "$SRC_DIR/Public/Interfaces/Interaction/IMedComPickupInterface.h" "$DST_DIR/Public/Interfaces/Interaction/ISuspensePickup.h"

# Migrate Interfaces - Core
migrate "$SRC_DIR/Public/Interfaces/Core/IMedComAttributeProviderInterface.h" "$DST_DIR/Public/Interfaces/Core/ISuspenseAttributeProvider.h"
migrate "$SRC_DIR/Private/Interfaces/Core/IMedComAttributeProviderInterface.cpp" "$DST_DIR/Private/Interfaces/Core/ISuspenseAttributeProvider.cpp"
migrate "$SRC_DIR/Public/Interfaces/Core/IMedComCharacterInterface.h" "$DST_DIR/Public/Interfaces/Core/ISuspenseCharacter.h"
migrate "$SRC_DIR/Private/Interfaces/Core/IMedComCharacterInterface.cpp" "$DST_DIR/Private/Interfaces/Core/ISuspenseCharacter.cpp"
migrate "$SRC_DIR/Public/Interfaces/Core/IMedComControllerInterface.h" "$DST_DIR/Public/Interfaces/Core/ISuspenseController.h"
migrate "$SRC_DIR/Private/Interfaces/Core/IMedComControllerInterface.cpp" "$DST_DIR/Private/Interfaces/Core/ISuspenseController.cpp"
migrate "$SRC_DIR/Public/Interfaces/Core/IMedComEnemyInterface.h" "$DST_DIR/Public/Interfaces/Core/ISuspenseEnemy.h"
migrate "$SRC_DIR/Private/Interfaces/Core/IMedComEnemyInterface.cpp" "$DST_DIR/Private/Interfaces/Core/ISuspenseEnemy.cpp"
migrate "$SRC_DIR/Public/Interfaces/Core/IMedComLoadoutInterface.h" "$DST_DIR/Public/Interfaces/Core/ISuspenseLoadout.h"
migrate "$SRC_DIR/Public/Interfaces/Core/IMedComMovementInterface.h" "$DST_DIR/Public/Interfaces/Core/ISuspenseMovement.h"
migrate "$SRC_DIR/Private/Interfaces/Core/IMedComMovementInterface.cpp" "$DST_DIR/Private/Interfaces/Core/ISuspenseMovement.cpp"
migrate "$SRC_DIR/Public/Interfaces/Core/IMedComPropertyAccessInterface.h" "$DST_DIR/Public/Interfaces/Core/ISuspensePropertyAccess.h"
migrate "$SRC_DIR/Private/Interfaces/Core/IMedComPropertyAccessInterface.cpp" "$DST_DIR/Private/Interfaces/Core/ISuspensePropertyAccess.cpp"
migrate "$SRC_DIR/Public/Interfaces/Core/MedComWorldBindable.h" "$DST_DIR/Public/Interfaces/Core/SuspenseWorldBindable.h"

# Migrate Interfaces - Abilities
migrate "$SRC_DIR/Public/Interfaces/Abilities/IMedComAbilityProvider.h" "$DST_DIR/Public/Interfaces/Abilities/ISuspenseAbilityProvider.h"
migrate "$SRC_DIR/Private/Interfaces/Abilities/IMedComAbilityProvider.cpp" "$DST_DIR/Private/Interfaces/Abilities/ISuspenseAbilityProvider.cpp"

# Migrate Interfaces - Weapon
migrate "$SRC_DIR/Public/Interfaces/Weapon/IMedComFireModeProviderInterface.h" "$DST_DIR/Public/Interfaces/Weapon/ISuspenseFireModeProvider.h"
migrate "$SRC_DIR/Private/Interfaces/Weapon/IMedComFireModeProviderInterface.cpp" "$DST_DIR/Private/Interfaces/Weapon/ISuspenseFireModeProvider.cpp"
migrate "$SRC_DIR/Public/Interfaces/Weapon/IMedComWeaponAnimationInterface.h" "$DST_DIR/Public/Interfaces/Weapon/ISuspenseWeaponAnimation.h"
migrate "$SRC_DIR/Private/Interfaces/Weapon/IMedComWeaponAnimationInterface.cpp" "$DST_DIR/Private/Interfaces/Weapon/ISuspenseWeaponAnimation.cpp"
migrate "$SRC_DIR/Public/Interfaces/Weapon/IMedComWeaponInterface.h" "$DST_DIR/Public/Interfaces/Weapon/ISuspenseWeapon.h"
migrate "$SRC_DIR/Private/Interfaces/Weapon/IMedComWeaponInterface.cpp" "$DST_DIR/Private/Interfaces/Weapon/ISuspenseWeapon.cpp"

# Migrate Interfaces - UI (all 16 files)
migrate "$SRC_DIR/Public/Interfaces/UI/IMedComContainerUIInterface.h" "$DST_DIR/Public/Interfaces/UI/ISuspenseContainerUI.h"
migrate "$SRC_DIR/Private/Interfaces/UI/IMedComContainerUIInterface.cpp" "$DST_DIR/Private/Interfaces/UI/ISuspenseContainerUI.cpp"
migrate "$SRC_DIR/Public/Interfaces/UI/IMedComCrosshairWidgetInterface.h" "$DST_DIR/Public/Interfaces/UI/ISuspenseCrosshairWidget.h"
migrate "$SRC_DIR/Private/Interfaces/UI/IMedComCrosshairWidgetInterface.cpp" "$DST_DIR/Private/Interfaces/UI/ISuspenseCrosshairWidget.cpp"
migrate "$SRC_DIR/Public/Interfaces/UI/IMedComDraggableInterface.h" "$DST_DIR/Public/Interfaces/UI/ISuspenseDraggable.h"
migrate "$SRC_DIR/Public/Interfaces/UI/IMedComDropTargetInterface.h" "$DST_DIR/Public/Interfaces/UI/ISuspenseDropTarget.h"
migrate "$SRC_DIR/Public/Interfaces/UI/IMedComEquipmentUIBridgeWidget.h" "$DST_DIR/Public/Interfaces/UI/ISuspenseEquipmentUIBridge.h"
migrate "$SRC_DIR/Private/Interfaces/UI/IMedComEquipmentUIBridgeWidget.cpp" "$DST_DIR/Private/Interfaces/UI/ISuspenseEquipmentUIBridge.cpp"
migrate "$SRC_DIR/Public/Interfaces/UI/IMedComHealthStaminaWidgetInterface.h" "$DST_DIR/Public/Interfaces/UI/ISuspenseHealthStaminaWidget.h"
migrate "$SRC_DIR/Private/Interfaces/UI/IMedComHealthStaminaWidgetInterface.cpp" "$DST_DIR/Private/Interfaces/UI/ISuspenseHealthStaminaWidget.cpp"
migrate "$SRC_DIR/Public/Interfaces/UI/IMedComHUDWidgetInterface.h" "$DST_DIR/Public/Interfaces/UI/ISuspenseHUDWidget.h"
migrate "$SRC_DIR/Private/Interfaces/UI/IMedComHUDWidgetInterface.cpp" "$DST_DIR/Private/Interfaces/UI/ISuspenseHUDWidget.cpp"
migrate "$SRC_DIR/Public/Interfaces/UI/IMedComInventoryUIBridgeWidget.h" "$DST_DIR/Public/Interfaces/UI/ISuspenseInventoryUIBridge.h"
migrate "$SRC_DIR/Private/Interfaces/UI/IMedComInventoryUIBridgeWidget.cpp" "$DST_DIR/Private/Interfaces/UI/ISuspenseInventoryUIBridge.cpp"
migrate "$SRC_DIR/Public/Interfaces/UI/IMedComLayoutInterface.h" "$DST_DIR/Public/Interfaces/UI/ISuspenseLayout.h"
migrate "$SRC_DIR/Public/Interfaces/UI/IMedComNotificationWidgetInterface.h" "$DST_DIR/Public/Interfaces/UI/ISuspenseNotificationWidget.h"
migrate "$SRC_DIR/Private/Interfaces/UI/IMedComNotificationWidgetInterface.cpp" "$DST_DIR/Private/Interfaces/UI/ISuspenseNotificationWidget.cpp"
migrate "$SRC_DIR/Public/Interfaces/UI/IMedComSlotUIInterface.h" "$DST_DIR/Public/Interfaces/UI/ISuspenseSlotUI.h"
migrate "$SRC_DIR/Public/Interfaces/UI/IMedComTooltipInterface.h" "$DST_DIR/Public/Interfaces/UI/ISuspenseTooltip.h"
migrate "$SRC_DIR/Public/Interfaces/UI/IMedComTooltipSourceInterface.h" "$DST_DIR/Public/Interfaces/UI/ISuspenseTooltipSource.h"
migrate "$SRC_DIR/Public/Interfaces/UI/IMedComUIWidgetInterface.h" "$DST_DIR/Public/Interfaces/UI/ISuspenseUIWidget.h"
migrate "$SRC_DIR/Private/Interfaces/UI/IMedComUIWidgetInterface.cpp" "$DST_DIR/Private/Interfaces/UI/ISuspenseUIWidget.cpp"
migrate "$SRC_DIR/Public/Interfaces/UI/IMedComWeaponUIWidgetInterface.h" "$DST_DIR/Public/Interfaces/UI/ISuspenseWeaponUIWidget.h"
migrate "$SRC_DIR/Private/Interfaces/UI/IMedComWeaponUIWidgetInterface.cpp" "$DST_DIR/Private/Interfaces/UI/ISuspenseWeaponUIWidget.cpp"

# Migrate Interfaces - Screens & Tabs
migrate "$SRC_DIR/Public/Interfaces/Screens/IMedComScreenInterface.h" "$DST_DIR/Public/Interfaces/Screens/ISuspenseScreen.h"
migrate "$SRC_DIR/Public/Interfaces/Tabs/IMedComTabBarInterface.h" "$DST_DIR/Public/Interfaces/Tabs/ISuspenseTabBar.h"

# Migrate Operations
migrate "$SRC_DIR/Public/Operations/InventoryResult.h" "$DST_DIR/Public/Operations/SuspenseInventoryResult.h"

# Migrate Types - Animation
migrate "$SRC_DIR/Public/Types/Animation/AnimationStateStruct.h" "$DST_DIR/Public/Types/Animation/SuspenseAnimationState.h"
migrate "$SRC_DIR/Private/Types/Animation/AnimationStateStruct.cpp" "$DST_DIR/Private/Types/Animation/SuspenseAnimationState.cpp"

# Migrate Types - Equipment
migrate "$SRC_DIR/Public/Types/Equipment/EquipmentTypes.h" "$DST_DIR/Public/Types/Equipment/SuspenseEquipmentTypes.h"
migrate "$SRC_DIR/Public/Types/Equipment/EquipmentVisualizationTypes.h" "$DST_DIR/Public/Types/Equipment/SuspenseEquipmentVisualizationTypes.h"
migrate "$SRC_DIR/Public/Types/Equipment/FMCEquipmentSlot.h" "$DST_DIR/Public/Types/Equipment/SuspenseEquipmentSlot.h"
migrate "$SRC_DIR/Public/Types/Equipment/WeaponTypes.h" "$DST_DIR/Public/Types/Equipment/SuspenseWeaponTypes.h"

# Migrate Types - Events
migrate "$SRC_DIR/Public/Types/Events/EquipmentEventData.h" "$DST_DIR/Public/Types/Events/SuspenseEquipmentEventData.h"

# Migrate Types - Inventory
migrate "$SRC_DIR/Public/Types/Inventory/InventoryTypeRegistry.h" "$DST_DIR/Public/Types/Inventory/SuspenseInventoryTypeRegistry.h"
migrate "$SRC_DIR/Private/Types/Inventory/InventoryTypeRegistry.cpp" "$DST_DIR/Private/Types/Inventory/SuspenseInventoryTypeRegistry.cpp"
migrate "$SRC_DIR/Public/Types/Inventory/InventoryTypes.h" "$DST_DIR/Public/Types/Inventory/SuspenseInventoryTypes.h"
migrate "$SRC_DIR/Private/Types/Inventory/InventoryTypes.cpp" "$DST_DIR/Private/Types/Inventory/SuspenseInventoryTypes.cpp"
migrate "$SRC_DIR/Public/Types/Inventory/InventoryUtils.h" "$DST_DIR/Public/Types/Inventory/SuspenseInventoryUtils.h"

# Migrate Types - Loadout
migrate "$SRC_DIR/Public/Types/Loadout/LoadoutSettings.h" "$DST_DIR/Public/Types/Loadout/SuspenseLoadoutSettings.h"
migrate "$SRC_DIR/Public/Types/Loadout/MedComItemDataTable.h" "$DST_DIR/Public/Types/Loadout/SuspenseItemDataTable.h"
migrate "$SRC_DIR/Private/Types/Loadout/MedComItemDataTable.cpp" "$DST_DIR/Private/Types/Loadout/SuspenseItemDataTable.cpp"
migrate "$SRC_DIR/Public/Types/Loadout/MedComLoadoutManager.h" "$DST_DIR/Public/Types/Loadout/SuspenseLoadoutManager.h"
migrate "$SRC_DIR/Private/Types/Loadout/MedComLoadoutManager.cpp" "$DST_DIR/Private/Types/Loadout/SuspenseLoadoutManager.cpp"

# Migrate Types - Network
migrate "$SRC_DIR/Public/Types/Network/MedComNetworkTypes.h" "$DST_DIR/Public/Types/Network/SuspenseNetworkTypes.h"

# Migrate Types - Rules
migrate "$SRC_DIR/Public/Types/Rules/MedComRulesTypes.h" "$DST_DIR/Public/Types/Rules/SuspenseRulesTypes.h"

# Migrate Types - Transaction
migrate "$SRC_DIR/Public/Types/Transaction/TransactionTypes.h" "$DST_DIR/Public/Types/Transaction/SuspenseTransactionTypes.h"

# Migrate Types - UI
migrate "$SRC_DIR/Public/Types/UI/ContainerUITypes.h" "$DST_DIR/Public/Types/UI/SuspenseContainerUITypes.h"
migrate "$SRC_DIR/Public/Types/UI/EquipmentUITypes.h" "$DST_DIR/Public/Types/UI/SuspenseEquipmentUITypes.h"

# Migrate Types - Weapon
migrate "$SRC_DIR/Public/Types/Weapon/BaseItemTypes.h" "$DST_DIR/Public/Types/Weapon/SuspenseBaseItemTypes.h"
migrate "$SRC_DIR/Public/Types/Weapon/FInventoryAmmoState.h" "$DST_DIR/Public/Types/Weapon/SuspenseInventoryAmmoState.h"

echo "✅ Migration complete!"
echo "Files migrated: 123 files (62 headers + 60 cpp + 1 module)"
echo ""
echo "Next step: Update BridgeSystem.Build.cs with full dependencies"
