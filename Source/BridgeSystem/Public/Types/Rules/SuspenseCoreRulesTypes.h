// SuspenseCoreRulesTypes.h
// SuspenseCore compatibility wrapper
// Copyright Suspense Team. All Rights Reserved.
//
// This file provides SuspenseCore* type aliases for compatibility with
// EquipmentSystem components that use the SuspenseCore naming convention.

#pragma once

#include "CoreMinimal.h"
#include "Types/Rules/SuspenseRulesTypes.h"

// Type aliases for SuspenseCore naming convention compatibility
// These allow EquipmentSystem/SuspenseCore components to use SuspenseCore* names
// while the actual implementations remain in BridgeSystem with Suspense* names

/** Alias for FSuspenseRuleCheckResult */
using FSuspenseCoreRuleCheckResult = FSuspenseRuleCheckResult;

/** Alias for FSuspenseAggregatedRuleResult */
using FSuspenseCoreAggregatedRuleResult = FSuspenseAggregatedRuleResult;

/** Alias for FSuspenseRuleContext */
using FSuspenseCoreRuleContext = FSuspenseRuleContext;

/** Alias for ESuspenseConflictResolution enum */
using ESuspenseCoreConflictResolution = ESuspenseConflictResolution;

/** Alias for ESuspenseRuleType enum */
using ESuspenseCoreRuleType = ESuspenseRuleType;

/** Alias for ESuspenseRuleSeverity enum */
using ESuspenseCoreRuleSeverity = ESuspenseRuleSeverity;
