// MedComShared/Types/Inventory/InventoryTypeRegistry.cpp
// Copyright MedCom Team. All Rights Reserved.

#include "Types/Inventory/InventoryTypeRegistry.h"
#include "GameplayTagsManager.h" 

// Initialize static singleton
UInventoryTypeRegistry* UInventoryTypeRegistry::Instance = nullptr;

UInventoryTypeRegistry::UInventoryTypeRegistry()
{
    // Конструктор не должен делать ничего рискованного
}

void UInventoryTypeRegistry::PostInitProperties()
{
    Super::PostInitProperties();
    
    // Безопасно зарегистрировать объект как синглтон
    if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && !Instance)
    {
        Instance = this;
        AddToRoot(); // Сразу защищаем от сборщика мусора
        InitializeDefaultTypes();
    }
}

void UInventoryTypeRegistry::BeginDestroy()
{
    if (Instance == this)
    {
        RemoveFromRoot();
        Instance = nullptr;
    }
    
    Super::BeginDestroy();
}

bool UInventoryTypeRegistry::RegisterItemType(const FInventoryItemTypeInfo& TypeInfo)
{
    // Validate type tag
    if (!TypeInfo.TypeTag.IsValid())
    {
        return false;
    }
    
    // Check if already registered
    if (IsTypeRegistered(TypeInfo.TypeTag))
    {
        // Update existing entry
        for (int32 i = 0; i < RegisteredTypes.Num(); i++)
        {
            if (RegisteredTypes[i].TypeTag == TypeInfo.TypeTag)
            {
                RegisteredTypes[i] = TypeInfo;
                return true;
            }
        }
    }
    
    // Add new entry
    RegisteredTypes.Add(TypeInfo);
    return true;
}

bool UInventoryTypeRegistry::IsTypeRegistered(const FGameplayTag& TypeTag) const
{
    if (!TypeTag.IsValid())
    {
        return false;
    }
    
    // Check for exact match first
    for (const FInventoryItemTypeInfo& TypeInfo : RegisteredTypes)
    {
        if (TypeInfo.TypeTag == TypeTag)
        {
            return true;
        }
    }
    
    // Then check for parent types
    for (const FInventoryItemTypeInfo& TypeInfo : RegisteredTypes)
    {
        if (TypeTag.MatchesTag(TypeInfo.TypeTag))
        {
            return true;
        }
    }
    
    return false;
}

bool UInventoryTypeRegistry::GetTypeInfo(const FGameplayTag& TypeTag, FInventoryItemTypeInfo& OutTypeInfo) const
{
    if (!TypeTag.IsValid())
    {
        return false;
    }
    
    // Look for exact match
    for (const FInventoryItemTypeInfo& TypeInfo : RegisteredTypes)
    {
        if (TypeInfo.TypeTag == TypeTag)
        {
            OutTypeInfo = TypeInfo;
            return true;
        }
    }
    
    // Look for parent match
    for (const FInventoryItemTypeInfo& TypeInfo : RegisteredTypes)
    {
        if (TypeTag.MatchesTag(TypeInfo.TypeTag))
        {
            OutTypeInfo = TypeInfo;
            return true;
        }
    }
    
    return false;
}

bool UInventoryTypeRegistry::AreTypesCompatible(const FGameplayTag& ItemType, const FGameplayTag& SlotType) const
{
    if (!ItemType.IsValid() || !SlotType.IsValid())
    {
        return false;
    }
    
    // Get type info
    FInventoryItemTypeInfo TypeInfo;
    if (!GetTypeInfo(ItemType, TypeInfo))
    {
        return false;
    }
    
    // Check compatibility
    if (TypeInfo.CompatibleSlots.IsEmpty())
    {
        // If no specific slots, check if slot type is parent of item type
        return ItemType.MatchesTag(SlotType);
    }
    else
    {
        // Check if slot type is in compatible slots
        for (const FGameplayTag& CompatibleSlot : TypeInfo.CompatibleSlots)
        {
            if (SlotType == CompatibleSlot || SlotType.MatchesTag(CompatibleSlot))
            {
                return true;
            }
        }
    }
    
    return false;
}

FVector2D UInventoryTypeRegistry::GetDefaultGridSize(const FGameplayTag& TypeTag) const
{
    // Get type info
    FInventoryItemTypeInfo TypeInfo;
    if (GetTypeInfo(TypeTag, TypeInfo))
    {
        return TypeInfo.DefaultGridSize;
    }
    
    // Default value
    return FVector2D(1.0f, 1.0f);
}

float UInventoryTypeRegistry::GetDefaultWeight(const FGameplayTag& TypeTag) const
{
    // Get type info
    FInventoryItemTypeInfo TypeInfo;
    if (GetTypeInfo(TypeTag, TypeInfo))
    {
        return TypeInfo.DefaultWeight;
    }
    
    // Default value
    return 1.0f;
}

TArray<FInventoryItemTypeInfo> UInventoryTypeRegistry::GetAllRegisteredTypes() const
{
    return RegisteredTypes;
}

FGameplayTagContainer UInventoryTypeRegistry::GetCompatibleSlots(const FGameplayTag& ItemType) const
{
    // Get type info
    FInventoryItemTypeInfo TypeInfo;
    if (GetTypeInfo(ItemType, TypeInfo))
    {
        return TypeInfo.CompatibleSlots;
    }
    
    // Return empty container
    return FGameplayTagContainer();
}

UInventoryTypeRegistry* UInventoryTypeRegistry::GetInstance()
{
    if (!Instance)
    {
        // Создаем синглтон с защитой от гонки потоков
        FScopeLock Lock(new FCriticalSection());
        
        if (!Instance) // Двойная проверка для предотвращения race condition
        {
            Instance = NewObject<UInventoryTypeRegistry>(GetTransientPackage());
            Instance->AddToRoot(); // Prevent garbage collection
            Instance->InitializeDefaultTypes();
        }
    }
    
    return Instance;
}

void UInventoryTypeRegistry::InitializeDefaultTypes()
{
    // Используем TagManager вместо статических методов для безопасного получения тегов
    UGameplayTagsManager& TagManager = UGameplayTagsManager::Get();
    
    // Register base item type
    FInventoryItemTypeInfo BaseItemType;
    BaseItemType.TypeTag = TagManager.RequestGameplayTag(FName("Item"), false);
    if (!BaseItemType.TypeTag.IsValid())
    {
        // Критическая ошибка, логируем и выходим
        UE_LOG(LogTemp, Error, TEXT("Failed to find 'Item' tag in gameplay tag manager"));
        return;
    }
    BaseItemType.DisplayName = FText::FromString(TEXT("Generic Item"));
    BaseItemType.Description = FText::FromString(TEXT("Base class for all inventory items"));
    BaseItemType.DefaultWeight = 1.0f;
    BaseItemType.DefaultGridSize = FVector2D(1.0f, 1.0f);
    RegisterItemType(BaseItemType);
    
    // Register weapon type
    FInventoryItemTypeInfo WeaponType;
    WeaponType.TypeTag = TagManager.RequestGameplayTag(FName("Item.Weapon"), false);
    if (!WeaponType.TypeTag.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to find 'Item.Weapon' tag"));
        return;
    }
    WeaponType.DisplayName = FText::FromString(TEXT("Weapon"));
    WeaponType.Description = FText::FromString(TEXT("Weapons and firearms"));
    WeaponType.DefaultWeight = 3.0f;
    WeaponType.DefaultGridSize = FVector2D(2.0f, 3.0f);
    
    FGameplayTag WeaponSlotTag = TagManager.RequestGameplayTag(FName("Equipment.Slot.Weapon"), false);
    if (WeaponSlotTag.IsValid())
    {
        WeaponType.CompatibleSlots.AddTag(WeaponSlotTag);
    }
    RegisterItemType(WeaponType);
    
    // Register armor type
    FInventoryItemTypeInfo ArmorType;
    ArmorType.TypeTag = TagManager.RequestGameplayTag(FName("Item.Armor"), false);
    if (!ArmorType.TypeTag.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to find 'Item.Armor' tag"));
        return;
    }
    ArmorType.DisplayName = FText::FromString(TEXT("Armor"));
    ArmorType.Description = FText::FromString(TEXT("Protective gear and armor"));
    ArmorType.DefaultWeight = 5.0f;
    ArmorType.DefaultGridSize = FVector2D(2.0f, 2.0f);
    
    FGameplayTag ArmorSlotTag = TagManager.RequestGameplayTag(FName("Equipment.Slot.Armor"), false);
    if (ArmorSlotTag.IsValid())
    {
        ArmorType.CompatibleSlots.AddTag(ArmorSlotTag);
    }
    RegisterItemType(ArmorType);
    
    // Register consumable type
    FInventoryItemTypeInfo ConsumableType;
    ConsumableType.TypeTag = TagManager.RequestGameplayTag(FName("Item.Consumable"), false);
    if (!ConsumableType.TypeTag.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to find 'Item.Consumable' tag"));
        return;
    }
    ConsumableType.DisplayName = FText::FromString(TEXT("Consumable"));
    ConsumableType.Description = FText::FromString(TEXT("Consumable items like food, medicine"));
    ConsumableType.DefaultWeight = 0.5f;
    ConsumableType.DefaultGridSize = FVector2D(1.0f, 1.0f);
    RegisterItemType(ConsumableType);
    
    // Register ammo type
    FInventoryItemTypeInfo AmmoType;
    AmmoType.TypeTag = TagManager.RequestGameplayTag(FName("Item.Ammo"), false);
    if (!AmmoType.TypeTag.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to find 'Item.Ammo' tag"));
        return;
    }
    AmmoType.DisplayName = FText::FromString(TEXT("Ammunition"));
    AmmoType.Description = FText::FromString(TEXT("Ammunition for weapons"));
    AmmoType.DefaultWeight = 0.1f;
    AmmoType.DefaultGridSize = FVector2D(1.0f, 1.0f);
    RegisterItemType(AmmoType);
}