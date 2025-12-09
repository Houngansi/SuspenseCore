// SuspenseCore.h
// Main entry point for SuspenseCore plugin
// Copyright Suspense Team. All Rights Reserved.
//
// USAGE:
//   Add "SuspenseCore" to your module dependencies
//   #include "SuspenseCore.h" - includes all core headers
//
// For selective includes, use individual headers:
//   #include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
//   #include "SuspenseCore/Events/SuspenseCoreEventBus.h"
//   #include "SuspenseCore/Utils/SuspenseCoreHelpers.h"

#pragma once

#include "Modules/ModuleManager.h"

// ═══════════════════════════════════════════════════════════════════════════════
// CORE INCLUDES - Re-exported from BridgeSystem
// ═══════════════════════════════════════════════════════════════════════════════

// GameplayTags - Centralized tag definitions
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"

// EventBus - Event-driven communication
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"

// Interfaces - Core contracts
#include "SuspenseCore/SuspenseCoreInterfaces.h"

// Services - Dependency injection
#include "SuspenseCore/Services/SuspenseCoreServiceProvider.h"

// Helpers - Utility functions
#include "SuspenseCore/Utils/SuspenseCoreHelpers.h"

// ═══════════════════════════════════════════════════════════════════════════════
// INVENTORY SYSTEM - Re-exported from InventorySystem module
// ═══════════════════════════════════════════════════════════════════════════════

// Inventory Component
#include "SuspenseCore/Components/SuspenseCoreInventoryComponent.h"

// Inventory Manager
#include "SuspenseCore/Base/SuspenseCoreInventoryManager.h"

// Inventory Events
#include "SuspenseCore/Events/Inventory/SuspenseCoreInventoryEvents.h"

/**
 * FSuspenseCoreModule
 *
 * Main module for SuspenseCore plugin.
 * Provides unified access to all SuspenseCore subsystems.
 *
 * Module loading order:
 * 1. BridgeSystem (PreDefault) - Core infrastructure
 * 2. GAS (PreDefault) - Gameplay Ability System
 * 3. PlayerCore (Default) - Player systems
 * 4. InteractionSystem (Default) - Interaction systems
 * 5. InventorySystem (Default) - Inventory systems
 * 6. SuspenseCore (Default) - This module, re-exports all
 */
class SUSPENSECORE_API FSuspenseCoreModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Get module instance
	 */
	static FSuspenseCoreModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FSuspenseCoreModule>("SuspenseCore");
	}

	/**
	 * Check if module is loaded
	 */
	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("SuspenseCore");
	}
};
