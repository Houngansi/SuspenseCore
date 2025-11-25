// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Log category declaration for inventory system
INVENTORYSYSTEM_API DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseInventory, Log, All);

// Convenience macros for inventory logging
#define SUSPENSE_INVENTORY_LOG(Verbosity, Format, ...) \
    UE_LOG(LogSuspenseInventory, Verbosity, TEXT("%s - %s"), *FString(__FUNCTION__), *FString::Printf(Format, ##__VA_ARGS__))

#define SUSPENSE_INVENTORY_CLOG(Condition, Verbosity, Format, ...) \
    UE_CLOG(Condition, LogSuspenseInventory, Verbosity, TEXT("%s - %s"), *FString(__FUNCTION__), *FString::Printf(Format, ##__VA_ARGS__))
