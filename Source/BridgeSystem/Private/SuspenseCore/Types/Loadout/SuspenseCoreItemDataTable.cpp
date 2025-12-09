// SuspenseCoreItemDataTable.cpp
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
#include "GameplayTagsManager.h"
#include "AbilitySystemGlobals.h"

//==================================================================
// FSuspenseCoreUnifiedItemData Implementation
//==================================================================

FMCPickupData FSuspenseCoreUnifiedItemData::ToPickupData(int32 Quantity) const
{
    FMCPickupData Result;
    
    // Основная связь с источником истины
    Result.ItemID = ItemID;
    Result.ItemName = DisplayName;
    Result.ItemType = ItemType;
    Result.Quantity = FMath::Clamp(Quantity, 1, MaxStackSize);
    
    // Ссылки на ассеты для world representation
    Result.WorldMesh = WorldMesh;
    Result.SpawnEffect = PickupSpawnVFX;
    Result.PickupSound = PickupSound;
    
    return Result;
}

FMCEquipmentData FSuspenseCoreUnifiedItemData::ToEquipmentData() const
{
    FMCEquipmentData Result;
    
    if (!bIsEquippable)
    {
        UE_LOG(LogTemp, Warning, TEXT("Attempting to create equipment data for non-equippable item: %s"), *ItemID.ToString());
        return Result;
    }
    
    // Основные данные экипировки
    Result.ItemID = ItemID;
    Result.SlotType = EquipmentSlot;
    Result.ActorClass = EquipmentActorClass;
    
    // ВАЖНО: Копируем оба сокета - активный и неактивный
    Result.AttachmentSocket = AttachmentSocket;
    Result.UnequippedSocket = UnequippedSocket;
    Result.AttachmentOffset = AttachmentOffset;
    Result.UnequippedOffset = UnequippedOffset;
    
    // Определяем какой AttributeSet и эффект инициализации использовать
    if (bIsWeapon && WeaponInitialization.WeaponAttributeSetClass)
    {
        Result.AttributeSetClass = WeaponInitialization.WeaponAttributeSetClass;
        Result.InitializationEffect = WeaponInitialization.WeaponInitEffect;
    }
    else if (bIsArmor && ArmorInitialization.ArmorAttributeSetClass)
    {
        Result.AttributeSetClass = ArmorInitialization.ArmorAttributeSetClass;
        Result.InitializationEffect = ArmorInitialization.ArmorInitEffect;
    }
    else if (EquipmentAttributeSet)
    {
        Result.AttributeSetClass = EquipmentAttributeSet;
        Result.InitializationEffect = EquipmentInitEffect;
    }
    
    // Конвертируем granted abilities
    Result.GrantedAbilities.Empty();
    for (const FGrantedAbilityData& AbilityData : GrantedAbilities)
    {
        if (AbilityData.AbilityClass)
        {
            Result.GrantedAbilities.Add(AbilityData.AbilityClass);
        }
    }
    
    return Result;
}

bool FSuspenseCoreUnifiedItemData::IsValid() const
{
    // Базовая валидация
    if (ItemID.IsNone())
        return false;
    
    if (DisplayName.IsEmpty())
        return false;
    
    if (!ItemType.IsValid())
        return false;
    
    // Проверяем что тип предмета начинается с базового тега Item
    static const FGameplayTag BaseItemTag = FGameplayTag::RequestGameplayTag(TEXT("Item"));
    if (!ItemType.MatchesTag(BaseItemTag))
        return false;
    
    // Валидация размеров сетки
    if (GridSize.X < 1 || GridSize.Y < 1 || GridSize.X > 10 || GridSize.Y > 10)
        return false;
    
    // Валидация стакинга
    if (MaxStackSize < 1)
        return false;
    
    // Валидация экипировки
    if (bIsEquippable)
    {
        if (!EquipmentSlot.IsValid())
            return false;
        
        if (EquipmentActorClass.IsNull())
            return false;
        
        // Специфичная валидация для оружия
        if (bIsWeapon)
        {
            if (!WeaponArchetype.IsValid())
                return false;
            
            // Проверяем что у оружия есть правильная инициализация
            if (!WeaponInitialization.WeaponAttributeSetClass || !WeaponInitialization.WeaponInitEffect)
                return false;
            
            // Проверяем наличие хотя бы одного режима огня
            if (FireModes.Num() == 0)
                return false;
        }
        
        // Специфичная валидация для брони
        if (bIsArmor)
        {
            if (!ArmorType.IsValid())
                return false;
            
            if (!ArmorInitialization.ArmorAttributeSetClass || !ArmorInitialization.ArmorInitEffect)
                return false;
        }
        
        // Общая экипировка должна иметь AttributeSet если не оружие/броня
        if (!bIsWeapon && !bIsArmor && !EquipmentAttributeSet)
            return false;
    }
    
    // Валидация боеприпасов
    if (bIsAmmo)
    {
        if (!AmmoCaliber.IsValid())
            return false;
        
        // Проверяем инициализацию атрибутов патронов
        if (!AmmoInitialization.AmmoAttributeSetClass || !AmmoInitialization.AmmoInitEffect)
            return false;
    }
    
    return true;
}

TArray<FText> FSuspenseCoreUnifiedItemData::GetValidationErrors() const
{
    TArray<FText> Errors;
    
    // Детальная валидация с описательными сообщениями
    if (ItemID.IsNone())
    {
        Errors.Add(FText::FromString(TEXT("ItemID is required and cannot be None")));
    }
    
    if (DisplayName.IsEmpty())
    {
        Errors.Add(FText::FromString(TEXT("DisplayName is required for UI display")));
    }
    
    if (!ItemType.IsValid())
    {
        Errors.Add(FText::FromString(TEXT("ItemType must be a valid GameplayTag from Item hierarchy")));
    }
    else
    {
        // Проверяем что тип предмета правильный
        static const FGameplayTag BaseItemTag = FGameplayTag::RequestGameplayTag(TEXT("Item"));
        if (!ItemType.MatchesTag(BaseItemTag))
        {
            Errors.Add(FText::Format(
                FText::FromString(TEXT("ItemType must be from Item.* hierarchy (current: {0})")), 
                FText::FromString(ItemType.ToString())));
        }
    }
    
    if (GridSize.X < 1 || GridSize.Y < 1)
    {
        Errors.Add(FText::Format(
            FText::FromString(TEXT("GridSize must be at least 1x1 (current: {0}x{1})")), 
            GridSize.X, GridSize.Y));
    }
    
    if (GridSize.X > 10 || GridSize.Y > 10)
    {
        Errors.Add(FText::Format(
            FText::FromString(TEXT("GridSize cannot exceed 10x10 (current: {0}x{1})")), 
            GridSize.X, GridSize.Y));
    }
    
    if (MaxStackSize < 1)
    {
        Errors.Add(FText::Format(
            FText::FromString(TEXT("MaxStackSize must be at least 1 (current: {0})")), 
            MaxStackSize));
    }
    
    if (Weight < 0.0f)
    {
        Errors.Add(FText::Format(
            FText::FromString(TEXT("Weight cannot be negative (current: {0})")), 
            Weight));
    }
    
    // Валидация экипировки
    if (bIsEquippable)
    {
        if (!EquipmentSlot.IsValid())
        {
            Errors.Add(FText::FromString(TEXT("Equippable items must have valid EquipmentSlot tag")));
        }
        
        if (EquipmentActorClass.IsNull())
        {
            Errors.Add(FText::FromString(TEXT("Equippable items must have EquipmentActorClass set")));
        }
        
        // Валидация оружия
        if (bIsWeapon)
        {
            if (!WeaponArchetype.IsValid())
            {
                Errors.Add(FText::FromString(TEXT("Weapons must have valid WeaponArchetype tag")));
            }
            
            if (!WeaponInitialization.WeaponAttributeSetClass)
            {
                Errors.Add(FText::FromString(TEXT("Weapons must have WeaponAttributeSetClass")));
            }
            
            if (!WeaponInitialization.WeaponInitEffect)
            {
                Errors.Add(FText::FromString(TEXT("Weapons must have WeaponInitEffect for attribute initialization")));
            }
            
            if (FireModes.Num() == 0)
            {
                Errors.Add(FText::FromString(TEXT("Weapons must have at least one fire mode defined")));
            }
            else
            {
                // Валидация каждого режима огня
                for (int32 i = 0; i < FireModes.Num(); ++i)
                {
                    const FWeaponFireModeData& FireMode = FireModes[i];
                    
                    if (!FireMode.FireModeTag.IsValid())
                    {
                        Errors.Add(FText::Format(
                            FText::FromString(TEXT("Fire mode {0} has invalid tag")), 
                            i));
                    }
                    
                    if (!FireMode.FireModeAbility)
                    {
                        Errors.Add(FText::Format(
                            FText::FromString(TEXT("Fire mode {0} missing ability class")), 
                            i));
                    }
                }
            }
        }
        
        // Валидация брони
        if (bIsArmor)
        {
            if (!ArmorType.IsValid())
            {
                Errors.Add(FText::FromString(TEXT("Armor must have valid ArmorType tag")));
            }
            
            if (!ArmorInitialization.ArmorAttributeSetClass)
            {
                Errors.Add(FText::FromString(TEXT("Armor must have ArmorAttributeSetClass for stats")));
            }
            
            if (!ArmorInitialization.ArmorInitEffect)
            {
                Errors.Add(FText::FromString(TEXT("Armor must have ArmorInitEffect for initialization")));
            }
        }
        
        // Валидация общей экипировки
        if (!bIsWeapon && !bIsArmor && !EquipmentAttributeSet)
        {
            Errors.Add(FText::FromString(TEXT("Non-weapon/armor equipment must have EquipmentAttributeSet")));
        }
    }
    
    // Валидация боеприпасов
    if (bIsAmmo)
    {
        if (!AmmoCaliber.IsValid())
        {
            Errors.Add(FText::FromString(TEXT("Ammo must have valid AmmoCaliber tag")));
        }
        
        if (!AmmoInitialization.AmmoAttributeSetClass)
        {
            Errors.Add(FText::FromString(TEXT("Ammo must have AmmoAttributeSetClass")));
        }
        
        if (!AmmoInitialization.AmmoInitEffect)
        {
            Errors.Add(FText::FromString(TEXT("Ammo must have AmmoInitEffect for attribute initialization")));
        }
        
        if (CompatibleWeapons.IsEmpty())
        {
            Errors.Add(FText::FromString(TEXT("Ammo should specify compatible weapon types")));
        }
    }
    
    // Логическая валидация
    if (bIsWeapon && bIsArmor)
    {
        Errors.Add(FText::FromString(TEXT("Item cannot be both weapon and armor")));
    }
    
    if (bIsAmmo && bIsEquippable)
    {
        Errors.Add(FText::FromString(TEXT("Ammunition cannot be equippable")));
    }
    
    return Errors;
}

void FSuspenseCoreUnifiedItemData::SanitizeData()
{
    // Автоматическая генерация ItemID если отсутствует
    if (ItemID.IsNone() && !DisplayName.IsEmpty())
    {
        FString SanitizedName = DisplayName.ToString();
        SanitizedName.ReplaceInline(TEXT(" "), TEXT("_"));
        SanitizedName.ReplaceInline(TEXT("-"), TEXT("_"));
        SanitizedName.ReplaceInline(TEXT("("), TEXT(""));
        SanitizedName.ReplaceInline(TEXT(")"), TEXT(""));
        SanitizedName.ReplaceInline(TEXT("["), TEXT(""));
        SanitizedName.ReplaceInline(TEXT("]"), TEXT(""));
        ItemID = FName(*SanitizedName);
        
        UE_LOG(LogTemp, Log, TEXT("Auto-generated ItemID: %s from DisplayName: %s"), 
            *ItemID.ToString(), *DisplayName.ToString());
    }
    
    // Автоматическая миграция старых тегов на новые
    FString ItemTypeString = ItemType.ToString();
    if (ItemTypeString.StartsWith(TEXT("Item.Type.")))
    {
        // Удаляем "Type." из пути тега
        FString NewTagString = ItemTypeString.Replace(TEXT("Item.Type."), TEXT("Item."));
        FGameplayTag NewTag = FGameplayTag::RequestGameplayTag(FName(*NewTagString));
        
        if (NewTag.IsValid())
        {
            UE_LOG(LogTemp, Log, TEXT("Auto-migrated item type from %s to %s for item %s"), 
                *ItemType.ToString(), *NewTag.ToString(), *ItemID.ToString());
            ItemType = NewTag;
        }
    }
    
    // Ограничиваем размер сетки
    GridSize.X = FMath::Clamp(GridSize.X, 1, 10);
    GridSize.Y = FMath::Clamp(GridSize.Y, 1, 10);
    
    // Обеспечиваем валидный размер стека
    MaxStackSize = FMath::Max(1, MaxStackSize);
    
    // Обеспечиваем неотрицательный вес
    Weight = FMath::Max(0.0f, Weight);
    
    // Обеспечиваем неотрицательную стоимость
    BaseValue = FMath::Max(0, BaseValue);
    
    // Исправляем логические противоречия
    if (bIsWeapon || bIsArmor)
    {
        bIsEquippable = true;
    }
    
    if (bIsWeapon && bIsArmor)
    {
        bIsArmor = false;
        UE_LOG(LogTemp, Warning, TEXT("Item %s was marked as both weapon and armor - set to weapon only"), 
            *ItemID.ToString());
    }
    
    if (bIsAmmo)
    {
        bIsEquippable = false;
        bIsConsumable = false;
    }
    
    // Автоматическая установка default fire mode если не задан
    if (bIsWeapon && !DefaultFireMode.IsValid() && FireModes.Num() > 0)
    {
        DefaultFireMode = FireModes[0].FireModeTag;
        UE_LOG(LogTemp, Log, TEXT("Auto-set default fire mode to %s for weapon %s"), 
            *DefaultFireMode.ToString(), *ItemID.ToString());
    }
    
    // Проверяем инициализацию атрибутов для производственной версии
    if (bIsWeapon && !WeaponInitialization.WeaponAttributeSetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("Weapon %s missing AttributeSet - production items must use AttributeSet"), 
            *ItemID.ToString());
    }
    
    if (bIsArmor && !ArmorInitialization.ArmorAttributeSetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("Armor %s missing AttributeSet - production items must use AttributeSet"), 
            *ItemID.ToString());
    }
    
    if (bIsAmmo && !AmmoInitialization.AmmoAttributeSetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("Ammo %s missing AttributeSet - production items must use AttributeSet"), 
            *ItemID.ToString());
    }
}

FGameplayTag FSuspenseCoreUnifiedItemData::GetEffectiveItemType() const
{
    // Возвращаем наиболее специфичный тип предмета
    if (bIsWeapon && WeaponArchetype.IsValid())
    {
        return WeaponArchetype;
    }
    
    if (bIsArmor && ArmorType.IsValid())
    {
        return ArmorType;
    }
    
    if (bIsAmmo && AmmoCaliber.IsValid())
    {
        return AmmoCaliber;
    }
    
    return ItemType;
}

bool FSuspenseCoreUnifiedItemData::MatchesTags(const FGameplayTagContainer& Tags) const
{
    if (Tags.IsEmpty())
    {
        return true;
    }
    
    // Собираем полный набор тегов для этого предмета
    FGameplayTagContainer ItemTagSet;
    ItemTagSet.AddTag(ItemType);
    ItemTagSet.AddTag(Rarity);
    ItemTagSet.AppendTags(ItemTags);
    
    if (bIsEquippable)
    {
        ItemTagSet.AddTag(EquipmentSlot);
    }
    
    if (bIsWeapon)
    {
        ItemTagSet.AddTag(WeaponArchetype);
        
        if (AmmoType.IsValid())
        {
            ItemTagSet.AddTag(AmmoType);
        }
        
        // Добавляем теги режимов огня
        for (const FWeaponFireModeData& FireMode : FireModes)
        {
            if (FireMode.FireModeTag.IsValid())
            {
                ItemTagSet.AddTag(FireMode.FireModeTag);
            }
        }
    }
    
    if (bIsArmor)
    {
        ItemTagSet.AddTag(ArmorType);
    }
    
    if (bIsAmmo)
    {
        ItemTagSet.AddTag(AmmoCaliber);
        ItemTagSet.AppendTags(CompatibleWeapons);
        ItemTagSet.AddTag(AmmoQuality);
        ItemTagSet.AppendTags(AmmoSpecialProperties);
    }
    
    return ItemTagSet.HasAny(Tags);
}

FLinearColor FSuspenseCoreUnifiedItemData::GetRarityColor() const
{
    // Стандартная цветовая схема редкости
    // Используем MatchesTag вместо MatchesTagExact для поддержки иерархии
    if (Rarity.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Rarity.Common"))))
        return FLinearColor(0.5f, 0.5f, 0.5f, 1.0f); // Серый
    
    if (Rarity.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Rarity.Uncommon"))))
        return FLinearColor(0.0f, 1.0f, 0.0f, 1.0f); // Зеленый
    
    if (Rarity.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Rarity.Rare"))))
        return FLinearColor(0.0f, 0.5f, 1.0f, 1.0f); // Синий
    
    if (Rarity.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Rarity.Epic"))))
        return FLinearColor(0.7f, 0.0f, 1.0f, 1.0f); // Фиолетовый
    
    if (Rarity.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Rarity.Legendary"))))
        return FLinearColor(1.0f, 0.5f, 0.0f, 1.0f); // Оранжевый
    
    // Специальные уровни редкости для MMO
    if (Rarity.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Rarity.Mythic"))))
        return FLinearColor(1.0f, 0.0f, 0.0f, 1.0f); // Красный
    
    if (Rarity.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Rarity.Unique"))))
        return FLinearColor(1.0f, 1.0f, 0.0f, 1.0f); // Желтый
    
    return FLinearColor::White;
}

#if WITH_EDITOR
void FSuspenseCoreUnifiedItemData::OnDataTableChanged(const UDataTable* InDataTable, const FName InRowName)
{
    // Автоматическая санитизация при редактировании
    SanitizeData();
    
    // Валидация и логирование ошибок
    TArray<FText> ValidationErrors = GetValidationErrors();
    if (ValidationErrors.Num() > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("DataTable item '%s' has %d validation errors:"), 
            *ItemID.ToString(), ValidationErrors.Num());
            
        for (int32 i = 0; i < ValidationErrors.Num(); ++i)
        {
            UE_LOG(LogTemp, Warning, TEXT("  Error %d: %s"), i + 1, *ValidationErrors[i].ToString());
        }
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("DataTable item '%s' validation passed successfully"), *ItemID.ToString());
    }
}
#endif