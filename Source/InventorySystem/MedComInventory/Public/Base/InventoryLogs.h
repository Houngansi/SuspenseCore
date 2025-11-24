// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Объявление категории логирования для инвентаря
MEDCOMINVENTORY_API DECLARE_LOG_CATEGORY_EXTERN(LogInventory, Log, All);

// Макросы для удобного логирования
#define INVENTORY_LOG(Verbosity, Format, ...) \
UE_LOG(LogInventory, Verbosity, TEXT("%s - %s"), *FString(__FUNCTION__), *FString::Printf(Format, ##__VA_ARGS__))

#define INVENTORY_CLOG(Condition, Verbosity, Format, ...) \
UE_CLOG(Condition, LogInventory, Verbosity, TEXT("%s - %s"), *FString(__FUNCTION__), *FString::Printf(Format, ##__VA_ARGS__))