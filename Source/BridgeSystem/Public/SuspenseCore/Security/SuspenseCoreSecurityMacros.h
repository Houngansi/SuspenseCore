// SuspenseCoreSecurityMacros.h
// SuspenseCore - Security Convenience Macros
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "SuspenseCoreSecurityValidator.h"

/**
 * Security Macros for SuspenseCore
 *
 * Provides convenient macros for common security patterns.
 * Use these instead of manual CheckAuthority calls for consistency.
 *
 * USAGE:
 * ```cpp
 * bool USuspenseCoreInventoryComponent::AddItem(FName ItemID, int32 Quantity)
 * {
 *     SUSPENSE_CHECK_AUTHORITY(GetOwner(), AddItem);
 *     // ... rest of implementation (only runs on server)
 * }
 * ```
 */

// ═══════════════════════════════════════════════════════════════════════════════
// AUTHORITY CHECK MACROS
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Check authority and return false if not server.
 * Use in functions returning bool.
 */
#define SUSPENSE_CHECK_AUTHORITY(Actor, FunctionName) \
	do { \
		USuspenseCoreSecurityValidator* _Security = USuspenseCoreSecurityValidator::Get(Actor); \
		if (_Security && !_Security->CheckAuthority(Actor, TEXT(#FunctionName))) \
		{ \
			return false; \
		} \
	} while(0)

/**
 * Check authority and return void if not server.
 * Use in void functions.
 */
#define SUSPENSE_CHECK_AUTHORITY_VOID(Actor, FunctionName) \
	do { \
		USuspenseCoreSecurityValidator* _Security = USuspenseCoreSecurityValidator::Get(Actor); \
		if (_Security && !_Security->CheckAuthority(Actor, TEXT(#FunctionName))) \
		{ \
			return; \
		} \
	} while(0)

/**
 * Check authority and return custom value if not server.
 */
#define SUSPENSE_CHECK_AUTHORITY_RETURN(Actor, FunctionName, ReturnValue) \
	do { \
		USuspenseCoreSecurityValidator* _Security = USuspenseCoreSecurityValidator::Get(Actor); \
		if (_Security && !_Security->CheckAuthority(Actor, TEXT(#FunctionName))) \
		{ \
			return ReturnValue; \
		} \
	} while(0)

/**
 * Check component owner authority and return false if not server.
 */
#define SUSPENSE_CHECK_COMPONENT_AUTHORITY(Component, FunctionName) \
	do { \
		USuspenseCoreSecurityValidator* _Security = USuspenseCoreSecurityValidator::Get(Component); \
		if (_Security && !_Security->CheckComponentAuthority(Component, TEXT(#FunctionName))) \
		{ \
			return false; \
		} \
	} while(0)

/**
 * Check component authority and return void if not server.
 */
#define SUSPENSE_CHECK_COMPONENT_AUTHORITY_VOID(Component, FunctionName) \
	do { \
		USuspenseCoreSecurityValidator* _Security = USuspenseCoreSecurityValidator::Get(Component); \
		if (_Security && !_Security->CheckComponentAuthority(Component, TEXT(#FunctionName))) \
		{ \
			return; \
		} \
	} while(0)

// ═══════════════════════════════════════════════════════════════════════════════
// AUTHORITY WITH SERVER RPC FALLBACK
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Check authority and call Server RPC if client, then return false.
 * Pattern: Client calls RPC, returns false; Server continues execution.
 *
 * @param Actor Actor to check
 * @param ServerRPC Server RPC function to call
 * @param ... Arguments to pass to RPC
 */
#define SUSPENSE_AUTHORITY_OR_RPC(Actor, ServerRPC, ...) \
	do { \
		if (!(Actor) || !(Actor)->HasAuthority()) \
		{ \
			ServerRPC(__VA_ARGS__); \
			return false; \
		} \
	} while(0)

/**
 * Check authority and call Server RPC if client, then return void.
 */
#define SUSPENSE_AUTHORITY_OR_RPC_VOID(Actor, ServerRPC, ...) \
	do { \
		if (!(Actor) || !(Actor)->HasAuthority()) \
		{ \
			ServerRPC(__VA_ARGS__); \
			return; \
		} \
	} while(0)

// ═══════════════════════════════════════════════════════════════════════════════
// RPC VALIDATION MACROS
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Validate item RPC parameters (for _Validate functions).
 * Checks: ItemID not None, Quantity > 0, Quantity <= MaxQuantity
 */
#define SUSPENSE_VALIDATE_ITEM_RPC(ItemID, Quantity) \
	(!(ItemID).IsNone() && (Quantity) > 0 && (Quantity) <= 9999)

/**
 * Validate item RPC with custom max quantity.
 */
#define SUSPENSE_VALIDATE_ITEM_RPC_MAX(ItemID, Quantity, MaxQuantity) \
	(!(ItemID).IsNone() && (Quantity) > 0 && (Quantity) <= (MaxQuantity))

/**
 * Validate GUID is valid.
 */
#define SUSPENSE_VALIDATE_GUID(InstanceID) \
	((InstanceID).IsValid())

/**
 * Validate slot index is in range.
 */
#define SUSPENSE_VALIDATE_SLOT(SlotIndex, MaxSlots) \
	((SlotIndex) >= 0 && (SlotIndex) < (MaxSlots))

/**
 * Validate two different slots (for swap/move operations).
 */
#define SUSPENSE_VALIDATE_SLOTS(Slot1, Slot2, MaxSlots) \
	((Slot1) >= 0 && (Slot1) < (MaxSlots) && \
	 (Slot2) >= 0 && (Slot2) < (MaxSlots) && \
	 (Slot1) != (Slot2))

/**
 * Validate quantity range.
 */
#define SUSPENSE_VALIDATE_QUANTITY(Quantity, MinQty, MaxQty) \
	((Quantity) >= (MinQty) && (Quantity) <= (MaxQty))

// ═══════════════════════════════════════════════════════════════════════════════
// RATE LIMITING MACROS
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Check rate limit and return false if exceeded.
 */
#define SUSPENSE_CHECK_RATE_LIMIT(Actor, FunctionName, MaxPerSecond) \
	do { \
		USuspenseCoreSecurityValidator* _Security = USuspenseCoreSecurityValidator::Get(Actor); \
		if (_Security && !_Security->CheckRateLimit(Actor, TEXT(#FunctionName), MaxPerSecond)) \
		{ \
			return false; \
		} \
	} while(0)

/**
 * Check rate limit with default rate (10/sec).
 */
#define SUSPENSE_CHECK_RATE_LIMIT_DEFAULT(Actor, FunctionName) \
	SUSPENSE_CHECK_RATE_LIMIT(Actor, FunctionName, 10.0f)

// ═══════════════════════════════════════════════════════════════════════════════
// SUSPICIOUS ACTIVITY MACROS
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Report suspicious activity.
 */
#define SUSPENSE_REPORT_SUSPICIOUS(Actor, Reason) \
	do { \
		USuspenseCoreSecurityValidator* _Security = USuspenseCoreSecurityValidator::Get(Actor); \
		if (_Security) \
		{ \
			_Security->ReportSuspiciousActivity(Actor, Reason); \
		} \
	} while(0)

// ═══════════════════════════════════════════════════════════════════════════════
// COMBINED SECURITY CHECK MACROS
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Full security check: authority + rate limit.
 * Returns false if any check fails.
 */
#define SUSPENSE_FULL_SECURITY_CHECK(Actor, FunctionName, MaxPerSecond) \
	do { \
		USuspenseCoreSecurityValidator* _Security = USuspenseCoreSecurityValidator::Get(Actor); \
		if (_Security) \
		{ \
			if (!_Security->CheckAuthority(Actor, TEXT(#FunctionName))) \
			{ \
				return false; \
			} \
			if (!_Security->CheckRateLimit(Actor, TEXT(#FunctionName), MaxPerSecond)) \
			{ \
				return false; \
			} \
		} \
	} while(0)

/**
 * Full security check for component.
 */
#define SUSPENSE_FULL_COMPONENT_SECURITY_CHECK(Component, FunctionName, MaxPerSecond) \
	do { \
		USuspenseCoreSecurityValidator* _Security = USuspenseCoreSecurityValidator::Get(Component); \
		if (_Security) \
		{ \
			if (!_Security->CheckComponentAuthority(Component, TEXT(#FunctionName))) \
			{ \
				return false; \
			} \
			AActor* _Owner = (Component)->GetOwner(); \
			if (!_Security->CheckRateLimit(_Owner, TEXT(#FunctionName), MaxPerSecond)) \
			{ \
				return false; \
			} \
		} \
	} while(0)
