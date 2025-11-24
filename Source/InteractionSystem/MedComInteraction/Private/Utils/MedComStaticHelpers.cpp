// Copyright MedCom Team. All Rights Reserved.

#include "Utils/MedComStaticHelpers.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Components/ActorComponent.h"
#include "ItemSystem/MedComItemManager.h"
#include "Interfaces/Inventory/IMedComInventoryInterface.h"
#include "Delegates/EventDelegateManager.h"
#include "Types/Loadout/MedComItemDataTable.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

// Log categories definition
DEFINE_LOG_CATEGORY(LogMedComInteraction);
DEFINE_LOG_CATEGORY(LogInventoryStatistics);

//==================================================================
// Component Discovery Implementation
//==================================================================

UObject* UMedComStaticHelpers::FindInventoryComponent(AActor* Actor)
{
    if (!Actor)
    {
        UE_LOG(LogMedComInteraction, Warning, TEXT("FindInventoryComponent: Actor is null"));
        return nullptr;
    }
    
    // Find PlayerState first
    APlayerState* PS = FindPlayerState(Actor);
    if (!PS)
    {
        UE_LOG(LogMedComInteraction, Warning, TEXT("FindInventoryComponent: PlayerState not found for actor %s"), 
            *Actor->GetName());
        return nullptr;
    }
    
    // Check all components in PlayerState
    for (UActorComponent* Component : PS->GetComponents())
    {
        if (Component && ImplementsInventoryInterface(Component))
        {
            UE_LOG(LogMedComInteraction, Log, TEXT("FindInventoryComponent: Found inventory component %s in PlayerState"), 
                *Component->GetName());
            return Component;
        }
    }
    
    // If not found in PlayerState, check the actor itself
    if (Actor != PS)
    {
        for (UActorComponent* Component : Actor->GetComponents())
        {
            if (Component && ImplementsInventoryInterface(Component))
            {
                UE_LOG(LogMedComInteraction, Log, TEXT("FindInventoryComponent: Found inventory component %s in Actor"), 
                    *Component->GetName());
                return Component;
            }
        }
        
        // Check controller if actor is a character
        if (ACharacter* Character = Cast<ACharacter>(Actor))
        {
            if (AController* Controller = Character->GetController())
            {
                for (UActorComponent* Component : Controller->GetComponents())
                {
                    if (Component && ImplementsInventoryInterface(Component))
                    {
                        UE_LOG(LogMedComInteraction, Log, TEXT("FindInventoryComponent: Found inventory component %s in Controller"), 
                            *Component->GetName());
                        return Component;
                    }
                }
            }
        }
    }
    
    UE_LOG(LogMedComInteraction, Warning, TEXT("FindInventoryComponent: No inventory component found for actor %s"), 
        *Actor->GetName());
    return nullptr;
}

APlayerState* UMedComStaticHelpers::FindPlayerState(AActor* Actor)
{
    if (!Actor)
    {
        return nullptr;
    }
    
    // Direct cast if actor is PlayerState
    if (APlayerState* PS = Cast<APlayerState>(Actor))
    {
        return PS;
    }
    
    // Check if actor is controller
    if (AController* Controller = Cast<AController>(Actor))
    {
        if (APlayerController* PC = Cast<APlayerController>(Controller))
        {
            return PC->PlayerState;
        }
    }
    
    // Check if actor is pawn
    if (APawn* Pawn = Cast<APawn>(Actor))
    {
        if (AController* Controller = Pawn->GetController())
        {
            if (APlayerController* PC = Cast<APlayerController>(Controller))
            {
                return PC->PlayerState;
            }
        }
    }
    
    // Check InstigatorController
    if (AController* Controller = Actor->GetInstigatorController())
    {
        if (APlayerController* PC = Cast<APlayerController>(Controller))
        {
            return PC->PlayerState;
        }
    }
    
    return nullptr;
}

bool UMedComStaticHelpers::ImplementsInventoryInterface(UObject* Object)
{
    if (!Object)
    {
        return false;
    }
    
    return Object->GetClass()->ImplementsInterface(UMedComInventoryInterface::StaticClass());
}

//==================================================================
// Item Operations Implementation
//==================================================================

bool UMedComStaticHelpers::AddItemToInventoryByID(UObject* InventoryComponent, FName ItemID, int32 Quantity)
{
    if (!InventoryComponent || !ImplementsInventoryInterface(InventoryComponent))
    {
        UE_LOG(LogMedComInteraction, Warning, TEXT("AddItemToInventoryByID: Invalid inventory component"));
        return false;
    }
    
    if (ItemID.IsNone() || Quantity <= 0)
    {
        UE_LOG(LogMedComInteraction, Warning, TEXT("AddItemToInventoryByID: Invalid ItemID or Quantity"));
        return false;
    }
    
    // Use the interface method directly
    bool bResult = IMedComInventoryInterface::Execute_AddItemByID(InventoryComponent, ItemID, Quantity);
    
    if (bResult)
    {
        UE_LOG(LogInventoryStatistics, Log, TEXT("AddItemToInventoryByID: Successfully added %s x%d"),
            *ItemID.ToString(), Quantity);
    }
    else
    {
        UE_LOG(LogInventoryStatistics, Warning, TEXT("AddItemToInventoryByID: Failed to add %s x%d"),
            *ItemID.ToString(), Quantity);
    }
    
    return bResult;
}

bool UMedComStaticHelpers::AddItemInstanceToInventory(UObject* InventoryComponent, const FInventoryItemInstance& ItemInstance)
{
    if (!InventoryComponent || !ImplementsInventoryInterface(InventoryComponent))
    {
        UE_LOG(LogMedComInteraction, Warning, TEXT("AddItemInstanceToInventory: Invalid inventory component"));
        return false;
    }
    
    if (!ItemInstance.IsValid())
    {
        UE_LOG(LogMedComInteraction, Warning, TEXT("AddItemInstanceToInventory: Invalid item instance"));
        return false;
    }
    
    // Get the interface implementation
    IMedComInventoryInterface* InventoryInterface = Cast<IMedComInventoryInterface>(InventoryComponent);
    if (!InventoryInterface)
    {
        UE_LOG(LogMedComInteraction, Error, TEXT("AddItemInstanceToInventory: Failed to cast to interface"));
        return false;
    }
    
    // Add the instance
    FInventoryOperationResult Result = InventoryInterface->AddItemInstance(ItemInstance);
    
    if (Result.bSuccess)
    {
        UE_LOG(LogInventoryStatistics, Log, TEXT("AddItemInstanceToInventory: Successfully added instance %s"),
            *ItemInstance.GetShortDebugString());
    }
    else
        UE_LOG(LogInventoryStatistics, Warning, TEXT("AddItemInstanceToInventory: Failed with error %s"),
            *Result.ErrorMessage.ToString());
    
    return Result.bSuccess;
}

bool UMedComStaticHelpers::CanActorPickupItem(AActor* Actor, FName ItemID, int32 Quantity)
{
    if (!Actor || ItemID.IsNone() || Quantity <= 0)
    {
        UE_LOG(LogMedComInteraction, Warning, 
            TEXT("CanActorPickupItem: Invalid parameters - Actor:%s, ItemID:%s, Quantity:%d"), 
            *GetNameSafe(Actor), *ItemID.ToString(), Quantity);
        return false;
    }
    
    // Получаем компонент инвентаря
    UObject* InventoryComponent = FindInventoryComponent(Actor);
    if (!InventoryComponent || !ImplementsInventoryInterface(InventoryComponent))
    {
        UE_LOG(LogMedComInteraction, Warning, 
            TEXT("CanActorPickupItem: No valid inventory component found for actor %s"), 
            *Actor->GetName());
        return false;
    }
    
    // Получаем менеджер предметов
    UMedComItemManager* ItemManager = GetItemManager(Actor);
    if (!ItemManager)
    {
        UE_LOG(LogMedComInteraction, Warning, 
            TEXT("CanActorPickupItem: ItemManager not found"));
        return false;
    }
    
    // Получаем унифицированные данные предмета
    FMedComUnifiedItemData UnifiedData;
    if (!ItemManager->GetUnifiedItemData(ItemID, UnifiedData))
    {
        UE_LOG(LogMedComInteraction, Warning, 
            TEXT("CanActorPickupItem: Item %s not found in DataTable"), 
            *ItemID.ToString());
        return false;
    }
    
    // Детальная диагностика данных предмета
    UE_LOG(LogMedComInteraction, Log, 
        TEXT("CanActorPickupItem: Checking item - ID:%s, Type:%s, Weight:%.2f, Quantity:%d"), 
        *ItemID.ToString(), 
        *UnifiedData.ItemType.ToString(),
        UnifiedData.Weight,
        Quantity);
    
    // Проверяем базовую валидность типа предмета
    static const FGameplayTag BaseItemTag = FGameplayTag::RequestGameplayTag(TEXT("Item"));
    if (!UnifiedData.ItemType.MatchesTag(BaseItemTag))
    {
        UE_LOG(LogMedComInteraction, Error, 
            TEXT("CanActorPickupItem: Item type %s is not in Item.* hierarchy!"), 
            *UnifiedData.ItemType.ToString());
        return false;
    }
    
    // Проверяем через интерфейс с детальной диагностикой
    bool bCanReceive = IMedComInventoryInterface::Execute_CanReceiveItem(
        InventoryComponent, 
        UnifiedData, 
        Quantity
    );
    
    if (!bCanReceive)
    {
        // Дополнительная диагностика почему не можем принять предмет
        UE_LOG(LogMedComInteraction, Warning, 
            TEXT("CanActorPickupItem: Inventory cannot receive item %s"), 
            *ItemID.ToString());
        
        // Проверяем разрешенные типы
        FGameplayTagContainer AllowedTypes = IMedComInventoryInterface::Execute_GetAllowedItemTypes(InventoryComponent);
        if (!AllowedTypes.IsEmpty())
        {
            UE_LOG(LogMedComInteraction, Warning, 
                TEXT("  - Inventory has type restrictions (%d allowed types)"), 
                AllowedTypes.Num());
            
            // Проверяем с учетом иерархии тегов
            bool bTypeAllowed = false;
            for (const FGameplayTag& AllowedTag : AllowedTypes)
            {
                if (UnifiedData.ItemType.MatchesTag(AllowedTag))
                {
                    bTypeAllowed = true;
                    break;
                }
            }
            
            UE_LOG(LogMedComInteraction, Warning, 
                TEXT("  - Item type %s allowed: %s"), 
                *UnifiedData.ItemType.ToString(),
                bTypeAllowed ? TEXT("YES") : TEXT("NO"));
            
            // Логируем все разрешенные типы для отладки
            for (const FGameplayTag& AllowedTag : AllowedTypes)
            {
                UE_LOG(LogMedComInteraction, Verbose, 
                    TEXT("    - Allowed type: %s"), 
                    *AllowedTag.ToString());
            }
        }
        else
        {
            UE_LOG(LogMedComInteraction, Log, 
                TEXT("  - Inventory has no type restrictions (all Item.* types allowed)"));
        }
        
        // Проверяем вес
        float CurrentWeight = IMedComInventoryInterface::Execute_GetCurrentWeight(InventoryComponent);
        float MaxWeight = IMedComInventoryInterface::Execute_GetMaxWeight(InventoryComponent);
        float RequiredWeight = UnifiedData.Weight * Quantity;
        
        UE_LOG(LogMedComInteraction, Warning, 
            TEXT("  - Weight check: Current=%.2f, Max=%.2f, Required=%.2f, Would fit: %s"), 
            CurrentWeight, MaxWeight, RequiredWeight,
            (CurrentWeight + RequiredWeight <= MaxWeight) ? TEXT("YES") : TEXT("NO"));
        
        // Проверяем размер предмета и доступное место
        FIntPoint ItemSize = UnifiedData.GridSize;
        UE_LOG(LogMedComInteraction, Warning, 
            TEXT("  - Item grid size: %dx%d"), 
            ItemSize.X, ItemSize.Y);
        
        // Проверяем стакаемость
        if (UnifiedData.MaxStackSize > 1)
        {
            UE_LOG(LogMedComInteraction, Log, 
                TEXT("  - Item is stackable (max stack: %d)"), 
                UnifiedData.MaxStackSize);
        }
    }
    else
    {
        UE_LOG(LogMedComInteraction, Log, 
            TEXT("CanActorPickupItem: Inventory CAN receive item %s"), 
            *ItemID.ToString());
    }
    
    return bCanReceive;
}

bool UMedComStaticHelpers::CreateItemInstance(FName ItemID, int32 Quantity, FInventoryItemInstance& OutInstance)
{
    if (ItemID.IsNone() || Quantity <= 0)
    {
        UE_LOG(LogMedComInteraction, Warning, TEXT("CreateItemInstance: Invalid parameters"));
        return false;
    }
    
    // Need world context to get ItemManager
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(GEngine, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World)
    {
        UE_LOG(LogMedComInteraction, Error, TEXT("CreateItemInstance: No world context available"));
        return false;
    }
    
    UMedComItemManager* ItemManager = GetItemManager(World);
    if (!ItemManager)
    {
        UE_LOG(LogMedComInteraction, Error, TEXT("CreateItemInstance: ItemManager not found"));
        return false;
    }
    
    return ItemManager->CreateItemInstance(ItemID, Quantity, OutInstance);
}

//==================================================================
// Item Information Implementation
//==================================================================

bool UMedComStaticHelpers::GetUnifiedItemData(FName ItemID, FMedComUnifiedItemData& OutItemData)
{
    if (ItemID.IsNone())
    {
        return false;
    }
    
    // Need world context to get ItemManager
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(GEngine, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World)
    {
        return false;
    }
    
    UMedComItemManager* ItemManager = GetItemManager(World);
    if (!ItemManager)
    {
        return false;
    }
    
    return ItemManager->GetUnifiedItemData(ItemID, OutItemData);
}

FText UMedComStaticHelpers::GetItemDisplayName(FName ItemID)
{
    FMedComUnifiedItemData ItemData;
    if (GetUnifiedItemData(ItemID, ItemData))
    {
        return ItemData.DisplayName;
    }
    
    return FText::FromString(ItemID.ToString());
}

float UMedComStaticHelpers::GetItemWeight(FName ItemID)
{
    FMedComUnifiedItemData ItemData;
    if (GetUnifiedItemData(ItemID, ItemData))
    {
        return ItemData.Weight;
    }
    
    return 0.0f;
}

bool UMedComStaticHelpers::IsItemStackable(FName ItemID)
{
    FMedComUnifiedItemData ItemData;
    if (GetUnifiedItemData(ItemID, ItemData))
    {
        return ItemData.MaxStackSize > 1;
    }
    
    return false;
}

//==================================================================
// Subsystem Access Implementation
//==================================================================

UMedComItemManager* UMedComStaticHelpers::GetItemManager(const UObject* WorldContextObject)
{
    if (!WorldContextObject)
    {
        return nullptr;
    }
    
    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        return nullptr;
    }
    
    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance)
    {
        return nullptr;
    }
    
    return GameInstance->GetSubsystem<UMedComItemManager>();
}

UEventDelegateManager* UMedComStaticHelpers::GetEventDelegateManager(const UObject* WorldContextObject)
{
    if (!WorldContextObject)
    {
        return nullptr;
    }
    
    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        return nullptr;
    }
    
    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance)
    {
        return nullptr;
    }
    
    return GameInstance->GetSubsystem<UEventDelegateManager>();
}

//==================================================================
// Inventory Validation Implementation
//==================================================================

bool UMedComStaticHelpers::ValidateInventorySpace(UObject* InventoryComponent, FName ItemID, int32 Quantity, FString& OutErrorMessage)
{
    OutErrorMessage.Empty();
    
    if (!InventoryComponent || !ImplementsInventoryInterface(InventoryComponent))
    {
        OutErrorMessage = TEXT("Invalid inventory component");
        return false;
    }
    
    // Get item data
    FMedComUnifiedItemData ItemData;
    if (!GetUnifiedItemData(ItemID, ItemData))
    {
        OutErrorMessage = FString::Printf(TEXT("Item %s not found"), *ItemID.ToString());
        return false;
    }
    
    // Check if inventory can receive item
    bool bCanReceive = IMedComInventoryInterface::Execute_CanReceiveItem(InventoryComponent, ItemData, Quantity);
    
    if (!bCanReceive)
    {
        OutErrorMessage = TEXT("Insufficient space or item type not allowed");
    }
    
    return bCanReceive;
}

bool UMedComStaticHelpers::ValidateWeightCapacity(UObject* InventoryComponent, FName ItemID, int32 Quantity, float& OutRemainingCapacity)
{
    OutRemainingCapacity = 0.0f;
    
    if (!InventoryComponent || !ImplementsInventoryInterface(InventoryComponent))
    {
        return false;
    }
    
    // Get item weight
    float ItemWeight = GetItemWeight(ItemID);
    float TotalWeight = ItemWeight * Quantity;
    
    // Get inventory weight capacity
    float CurrentWeight = IMedComInventoryInterface::Execute_GetCurrentWeight(InventoryComponent);
    float MaxWeight = IMedComInventoryInterface::Execute_GetMaxWeight(InventoryComponent);
    float RemainingWeight = MaxWeight - CurrentWeight;
    
    OutRemainingCapacity = RemainingWeight;
    
    return RemainingWeight >= TotalWeight;
}

//==================================================================
// Utility Functions Implementation
//==================================================================

void UMedComStaticHelpers::GetInventoryStatistics(UObject* InventoryComponent, int32& OutTotalItems, float& OutTotalWeight, int32& OutUsedSlots)
{
    OutTotalItems = 0;
    OutTotalWeight = 0.0f;
    OutUsedSlots = 0;
    
    if (!InventoryComponent || !ImplementsInventoryInterface(InventoryComponent))
    {
        return;
    }
    
    // Get interface implementation
    IMedComInventoryInterface* InventoryInterface = Cast<IMedComInventoryInterface>(InventoryComponent);
    if (!InventoryInterface)
    {
        return;
    }
    
    // Get all items
    TArray<FInventoryItemInstance> AllInstances = InventoryInterface->GetAllItemInstances();
    
    OutUsedSlots = AllInstances.Num();
    
    for (const FInventoryItemInstance& Instance : AllInstances)
    {
        OutTotalItems += Instance.Quantity;
        
        // Get item weight from DataTable
        FMedComUnifiedItemData ItemData;
        if (GetUnifiedItemData(Instance.ItemID, ItemData))
        {
            OutTotalWeight += ItemData.Weight * Instance.Quantity;
        }
    }
    
    UE_LOG(LogInventoryStatistics, Log, TEXT("Inventory Statistics: %d items, %.2f weight, %d slots used"),
        OutTotalItems, OutTotalWeight, OutUsedSlots);
}

void UMedComStaticHelpers::LogInventoryContents(UObject* InventoryComponent, const FString& LogCategory)
{
    if (!InventoryComponent || !ImplementsInventoryInterface(InventoryComponent))
    {
        UE_LOG(LogInventoryStatistics, Warning, TEXT("LogInventoryContents: Invalid inventory component"));
        return;
    }
    
    // Get interface implementation
    IMedComInventoryInterface* InventoryInterface = Cast<IMedComInventoryInterface>(InventoryComponent);
    if (!InventoryInterface)
    {
        return;
    }
    
    // Get all items
    TArray<FInventoryItemInstance> AllInstances = InventoryInterface->GetAllItemInstances();
    
    UE_LOG(LogInventoryStatistics, Log, TEXT("=== Inventory Contents (%s) ==="), *LogCategory);
    UE_LOG(LogInventoryStatistics, Log, TEXT("Total slots used: %d"), AllInstances.Num());
    
    for (const FInventoryItemInstance& Instance : AllInstances)
    {
        FString ItemName = Instance.ItemID.ToString();
        FText DisplayName = GetItemDisplayName(Instance.ItemID);
        
        UE_LOG(LogInventoryStatistics, Log, TEXT("  - %s (%s) x%d [Slot: %d, Rotated: %s]"),
            *DisplayName.ToString(),
            *ItemName,
            Instance.Quantity,
            Instance.AnchorIndex,
            Instance.bIsRotated ? TEXT("Yes") : TEXT("No"));
        
        // Log runtime properties if any
        if (Instance.RuntimeProperties.Num() > 0)
        {
            UE_LOG(LogInventoryStatistics, Log, TEXT("    Runtime Properties:"));
            for (const auto& PropPair : Instance.RuntimeProperties)
            {
                UE_LOG(LogInventoryStatistics, Log, TEXT("      %s: %.2f"), 
                    *PropPair.Key.ToString(), PropPair.Value);
            }
        }
    }
    
    UE_LOG(LogInventoryStatistics, Log, TEXT("=== End Inventory Contents ==="));
}