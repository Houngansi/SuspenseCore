// Copyright MedCom Team. All Rights Reserved.

#include "Base/MedComItemBase.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "Base/InventoryLogs.h"
#include "ItemSystem/MedComItemManager.h"
#include "Types/Loadout/MedComItemDataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

//==============================================================================
// Конструктор и инициализация
//==============================================================================

UMedComItemBase::UMedComItemBase()
{
    // Генерируем уникальный ID экземпляра
    InstanceID = FGuid::NewGuid();
    
    // Инициализируем runtime состояние
    CurrentDurability = 0.0f;
    LastUsedTime = 0.0f;
    LastCacheUpdateTime = 0.0f;
    CachedItemData = nullptr;
    
    // Очищаем контейнеры
    RuntimePropertiesArray.Empty();
    RuntimePropertiesCache.Empty();
    
    // Настраиваем сетевую репликацию
    SetFlags(RF_DefaultSubObject);
}

void UMedComItemBase::PostInitProperties()
{
    Super::PostInitProperties();
    
    // Инициализируем runtime свойства если ItemID уже установлен
    if (!ItemID.IsNone())
    {
        Initialize(ItemID);
    }
}

void UMedComItemBase::BeginDestroy()
{
    // Очищаем кэш перед уничтожением
    CachedItemData = nullptr;
    
    Super::BeginDestroy();
}

//==============================================================================
// Методы доступа к статическим данным из DataTable
//==============================================================================

const FMedComUnifiedItemData* UMedComItemBase::GetItemData() const
{
    UpdateCacheIfNeeded();
    return CachedItemData;
}

void UMedComItemBase::RefreshItemData()
{
    CachedItemData = nullptr;
    LastCacheUpdateTime = 0.0f;
    UpdateCacheIfNeeded();
}

bool UMedComItemBase::HasValidItemData() const
{
    return GetItemData() != nullptr;
}

UMedComItemManager* UMedComItemBase::GetItemManager() const
{
    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GameInstance = World->GetGameInstance())
        {
            return GameInstance->GetSubsystem<UMedComItemManager>();
        }
    }
    return nullptr;
}

void UMedComItemBase::UpdateCacheIfNeeded() const
{
    // Проверяем, нужно ли обновить кэш
    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    
    if (CachedItemData != nullptr && 
        (CurrentTime - LastCacheUpdateTime) < CACHE_UPDATE_INTERVAL)
    {
        return; // Кэш еще актуален
    }
    
    // Обновляем кэш из DataTable
    if (UMedComItemManager* ItemManager = GetItemManager())
    {
        // Создаем статическую переменную для хранения данных
        // Это безопасно потому что данные из DataTable неизменяемы
        static TMap<FName, FMedComUnifiedItemData> StaticDataCache;
        
        FMedComUnifiedItemData* CachedData = StaticDataCache.Find(ItemID);
        if (!CachedData)
        {
            // Загружаем данные из DataTable в статический кэш
            FMedComUnifiedItemData NewData;
            if (ItemManager->GetUnifiedItemData(ItemID, NewData))
            {
                StaticDataCache.Add(ItemID, NewData);
                CachedData = StaticDataCache.Find(ItemID);
            }
        }
        
        CachedItemData = CachedData;
        LastCacheUpdateTime = CurrentTime;
    }
    
    if (!CachedItemData)
    {
        UE_LOG(LogInventory, Warning, TEXT("Failed to load item data for ID: %s"), *ItemID.ToString());
    }
}

//==============================================================================
// Convenience методы для доступа к статическим свойствам
//==============================================================================

FText UMedComItemBase::GetItemName() const
{
    const FMedComUnifiedItemData* Data = GetItemData();
    return Data ? Data->DisplayName : FText::FromString(TEXT("Unknown Item"));
}

FText UMedComItemBase::GetItemDescription() const
{
    const FMedComUnifiedItemData* Data = GetItemData();
    return Data ? Data->Description : FText::FromString(TEXT("No description available"));
}

UTexture2D* UMedComItemBase::GetItemIcon() const
{
    const FMedComUnifiedItemData* Data = GetItemData();
    return Data && !Data->Icon.IsNull() ? Data->Icon.LoadSynchronous() : nullptr;
}

FGameplayTag UMedComItemBase::GetItemType() const
{
    const FMedComUnifiedItemData* Data = GetItemData();
    return Data ? Data->ItemType : FGameplayTag();
}

FIntPoint UMedComItemBase::GetGridSize() const
{
    const FMedComUnifiedItemData* Data = GetItemData();
    return Data ? Data->GridSize : FIntPoint(1, 1);
}

int32 UMedComItemBase::GetMaxStackSize() const
{
    const FMedComUnifiedItemData* Data = GetItemData();
    return Data ? Data->MaxStackSize : 1;
}

float UMedComItemBase::GetWeight() const
{
    const FMedComUnifiedItemData* Data = GetItemData();
    return Data ? Data->Weight : 1.0f;
}

int32 UMedComItemBase::GetBaseValue() const
{
    const FMedComUnifiedItemData* Data = GetItemData();
    return Data ? Data->BaseValue : 0;
}

bool UMedComItemBase::IsEquippable() const
{
    const FMedComUnifiedItemData* Data = GetItemData();
    return Data ? Data->bIsEquippable : false;
}

FGameplayTag UMedComItemBase::GetEquipmentSlot() const
{
    const FMedComUnifiedItemData* Data = GetItemData();
    return Data ? Data->EquipmentSlot : FGameplayTag();
}

bool UMedComItemBase::IsConsumable() const
{
    const FMedComUnifiedItemData* Data = GetItemData();
    return Data ? Data->bIsConsumable : false;
}

bool UMedComItemBase::IsDroppable() const
{
    const FMedComUnifiedItemData* Data = GetItemData();
    return Data ? Data->bCanDrop : true;
}

bool UMedComItemBase::IsTradeable() const
{
    const FMedComUnifiedItemData* Data = GetItemData();
    return Data ? Data->bCanTrade : true;
}

//==============================================================================
// Методы работы с runtime свойствами
//==============================================================================

float UMedComItemBase::GetRuntimeProperty(const FName& PropertyName, float DefaultValue) const
{
    // Сначала проверяем локальный кэш для быстрого доступа
    const float* CachedValue = RuntimePropertiesCache.Find(PropertyName);
    if (CachedValue)
    {
        return *CachedValue;
    }
    
    // Если не в кэше, синхронизируем и проверяем снова
    SyncPropertiesCacheWithArray();
    CachedValue = RuntimePropertiesCache.Find(PropertyName);
    
    return CachedValue ? *CachedValue : DefaultValue;
}

void UMedComItemBase::SetRuntimeProperty(const FName& PropertyName, float Value)
{
    // Обновляем локальный кэш
    RuntimePropertiesCache.Add(PropertyName, Value);
    
    // Синхронизируем с реплицируемым массивом
    SyncPropertiesArrayWithCache();
}

TArray<FItemRuntimeProperty> UMedComItemBase::GetAllRuntimeProperties() const
{
    // Убеждаемся, что массив актуален
    const_cast<UMedComItemBase*>(this)->SyncPropertiesArrayWithCache();
    return RuntimePropertiesArray;
}
void UMedComItemBase::RemoveRuntimeProperty(const FName& PropertyName)
{
    // Удаляем из кэша
    RuntimePropertiesCache.Remove(PropertyName);
    
    // Синхронизируем с массивом
    SyncPropertiesArrayWithCache();
}

bool UMedComItemBase::HasRuntimeProperty(const FName& PropertyName) const
{
    // Проверяем кэш
    if (RuntimePropertiesCache.Contains(PropertyName))
    {
        return true;
    }
    
    // Синхронизируем и проверяем снова
    SyncPropertiesCacheWithArray();
    return RuntimePropertiesCache.Contains(PropertyName);
}

//==============================================================================
// Методы работы с прочностью
//==============================================================================

float UMedComItemBase::GetMaxDurability() const
{
    // Сначала проверяем runtime свойство (может быть изменено AttributeSet)
    if (HasRuntimeProperty(TEXT("MaxDurability")))
    {
        return GetRuntimeProperty(TEXT("MaxDurability"));
    }
    
    // TODO: В будущем получать из AttributeSet если предмет экипирован
    
    // Возвращаем значение по умолчанию для типа предмета
    const FMedComUnifiedItemData* Data = GetItemData();
    if (Data && Data->bIsEquippable)
    {
        if (Data->bIsWeapon)
        {
            return 150.0f; // Значение по умолчанию для оружия
        }
        else if (Data->bIsArmor)
        {
            return 200.0f; // Значение по умолчанию для брони
        }
        return 100.0f; // Значение по умолчанию для экипировки
    }
    
    return 0.0f; // Нет прочности
}

bool UMedComItemBase::HasDurability() const
{
    return GetMaxDurability() > 0.0f;
}

float UMedComItemBase::GetDurabilityPercent() const
{
    float MaxDur = GetMaxDurability();
    if (MaxDur <= 0.0f)
    {
        return 1.0f; // Предметы без прочности всегда "целые"
    }
    
    return FMath::Clamp(CurrentDurability / MaxDur, 0.0f, 1.0f);
}

void UMedComItemBase::SetCurrentDurability(float NewDurability)
{
    float MaxDur = GetMaxDurability();
    CurrentDurability = FMath::Clamp(NewDurability, 0.0f, MaxDur);
}

float UMedComItemBase::DamageDurability(float Damage)
{
    SetCurrentDurability(CurrentDurability - FMath::Abs(Damage));
    return CurrentDurability;
}

float UMedComItemBase::RepairDurability(float RepairAmount)
{
    SetCurrentDurability(CurrentDurability + FMath::Abs(RepairAmount));
    return CurrentDurability;
}

//==============================================================================
// Специализированные методы для оружия
//==============================================================================

int32 UMedComItemBase::GetCurrentAmmo() const
{
    return FMath::RoundToInt(GetRuntimeProperty(TEXT("Ammo"), 0.0f));
}

void UMedComItemBase::SetCurrentAmmo(int32 AmmoCount)
{
    int32 MaxAmmo = GetMaxAmmo();
    int32 ClampedAmmo = FMath::Clamp(AmmoCount, 0, MaxAmmo);
    SetRuntimeProperty(TEXT("Ammo"), static_cast<float>(ClampedAmmo));
}

int32 UMedComItemBase::GetMaxAmmo() const
{
    // Сначала проверяем runtime свойство
    if (HasRuntimeProperty(TEXT("MaxAmmo")))
    {
        return FMath::RoundToInt(GetRuntimeProperty(TEXT("MaxAmmo")));
    }
    
    // TODO: В будущем получать из AmmoAttributeSet
    
    // Временная логика на основе типа оружия
    const FMedComUnifiedItemData* Data = GetItemData();
    if (Data && Data->bIsWeapon)
    {
        FString ArchetypeString = Data->WeaponArchetype.ToString();
        
        if (ArchetypeString.Contains(TEXT("Rifle")))
        {
            return 30;
        }
        else if (ArchetypeString.Contains(TEXT("Pistol")))
        {
            return 15;
        }
        else if (ArchetypeString.Contains(TEXT("Shotgun")))
        {
            return 8;
        }
        else if (ArchetypeString.Contains(TEXT("Sniper")))
        {
            return 5;
        }
    }
    
    return 30; // Значение по умолчанию
}

bool UMedComItemBase::HasAmmo() const
{
    return GetCurrentAmmo() > 0;
}

//==============================================================================
// Методы кулдаунов
//==============================================================================

bool UMedComItemBase::IsOnCooldown(float CurrentTime) const
{
    float CooldownEndTime = GetRuntimeProperty(TEXT("CooldownEnd"), 0.0f);
    return CurrentTime < CooldownEndTime;
}

void UMedComItemBase::StartCooldown(float CurrentTime, float CooldownDuration)
{
    SetRuntimeProperty(TEXT("CooldownEnd"), CurrentTime + CooldownDuration);
}

float UMedComItemBase::GetRemainingCooldown(float CurrentTime) const
{
    float CooldownEndTime = GetRuntimeProperty(TEXT("CooldownEnd"), 0.0f);
    return FMath::Max(0.0f, CooldownEndTime - CurrentTime);
}

//==============================================================================
// Основные методы предмета
//==============================================================================

void UMedComItemBase::UseItem_Implementation(ACharacter* Character)
{
    if (!Character)
    {
        UE_LOG(LogInventory, Warning, TEXT("UseItem called with null Character"));
        return;
    }
    
    const FMedComUnifiedItemData* Data = GetItemData();
    if (!Data)
    {
        UE_LOG(LogInventory, Error, TEXT("UseItem failed: No item data for %s"), *ItemID.ToString());
        return;
    }
    
    UE_LOG(LogInventory, Log, TEXT("Item %s used by character %s"), 
        *ItemID.ToString(), *Character->GetName());
    
    // Обновляем время последнего использования
    LastUsedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    
    // Если предмет имеет кулдаун, запускаем его
    if (Data->bIsConsumable && Data->UseTime > 0.0f)
    {
        StartCooldown(LastUsedTime, Data->UseTime);
    }
    
    // Применяем эффекты потребления из DataTable
    if (Data->bIsConsumable && Data->ConsumeEffects.Num() > 0)
    {
        // TODO: Применить GameplayEffects к персонажу
        UE_LOG(LogInventory, Log, TEXT("Applied %d consume effects"), Data->ConsumeEffects.Num());
    }
}

bool UMedComItemBase::IsValid() const
{
    return !ItemID.IsNone() && HasValidItemData();
}

FString UMedComItemBase::GetDebugString() const
{
    const FMedComUnifiedItemData* Data = GetItemData();
    if (!Data)
    {
        return FString::Printf(TEXT("INVALID ITEM: %s"), *ItemID.ToString());
    }
    
    // Используем размер кэша для подсчета свойств
    return FString::Printf(
        TEXT("Item: %s (ID: %s, Type: %s, Durability: %.1f/%.1f, Props: %d, Instance: %s)"),
        *Data->DisplayName.ToString(),
        *ItemID.ToString(),
        *Data->ItemType.ToString(),
        CurrentDurability,
        GetMaxDurability(),
        RuntimePropertiesCache.Num(),  // Изменено с RuntimeProperties.Num()
        *InstanceID.ToString()
    );
}

//==============================================================================
// Конверсия и совместимость
//==============================================================================

FInventoryItemInstance UMedComItemBase::ToInventoryInstance(int32 Quantity) const
{
    // Create instance using factory method
    FInventoryItemInstance Instance = FInventoryItemInstance::Create(ItemID, Quantity);
    
    // Копируем runtime свойства из кэша
    Instance.RuntimeProperties = RuntimePropertiesCache;
    Instance.LastUsedTime = LastUsedTime;
    
    // Устанавливаем прочность
    if (HasDurability())
    {
        Instance.SetRuntimeProperty(TEXT("Durability"), CurrentDurability);
        Instance.SetRuntimeProperty(TEXT("MaxDurability"), GetMaxDurability());
    }
    
    return Instance;
}
void UMedComItemBase::InitFromInventoryInstance(const FInventoryItemInstance& Instance)
{
    ItemID = Instance.ItemID;
    RuntimePropertiesCache = Instance.RuntimeProperties;
    LastUsedTime = Instance.LastUsedTime;
    CurrentDurability = Instance.GetCurrentDurability();
    
    // Синхронизируем массив с кэшем для репликации
    SyncPropertiesArrayWithCache();
    
    // Принудительно обновляем кэш данных
    RefreshItemData();
}

//==============================================================================
// Методы инициализации
//==============================================================================

void UMedComItemBase::Initialize(const FName& InItemID)
{
    ItemID = InItemID;
    
    // Очищаем старые данные
    ResetToDefaults();
    
    // Обновляем кэш из DataTable
    RefreshItemData();
    
    // Инициализируем runtime свойства
    InitializeRuntimePropertiesFromData();
    
    UE_LOG(LogInventory, Log, TEXT("Initialized item: %s"), *GetDebugString());
}

void UMedComItemBase::ResetToDefaults()
{
    CurrentDurability = 0.0f;
    LastUsedTime = 0.0f;
    
    // Очищаем оба контейнера runtime свойств
    RuntimePropertiesCache.Empty();
    RuntimePropertiesArray.Empty();
    
    InstanceID = FGuid::NewGuid();
}

void UMedComItemBase::InitializeRuntimePropertiesFromData()
{
    const FMedComUnifiedItemData* Data = GetItemData();
    if (!Data)
    {
        return;
    }
    
    // Инициализируем прочность для экипируемых предметов
    if (Data->bIsEquippable)
    {
        float MaxDur = GetMaxDurability();
        SetRuntimeProperty(TEXT("MaxDurability"), MaxDur);
        CurrentDurability = MaxDur; // Начинаем с полной прочности
    }
    
    // Инициализируем патроны для оружия
    if (Data->bIsWeapon)
    {
        int32 MaxAmmo = GetMaxAmmo();
        SetRuntimeProperty(TEXT("MaxAmmo"), static_cast<float>(MaxAmmo));
        SetRuntimeProperty(TEXT("Ammo"), static_cast<float>(MaxAmmo)); // Полный магазин
    }
    
    // Инициализируем заряды для расходуемых предметов
    if (Data->bIsConsumable)
    {
        SetRuntimeProperty(TEXT("Charges"), 1.0f); // Один заряд по умолчанию
    }
}
void UMedComItemBase::SyncPropertiesCacheWithArray() const
{
    // Обновляем кэш из массива (для клиентов после репликации)
    RuntimePropertiesCache.Empty();
    
    for (const FItemRuntimeProperty& Prop : RuntimePropertiesArray)
    {
        RuntimePropertiesCache.Add(Prop.PropertyName, Prop.PropertyValue);
    }
}

void UMedComItemBase::SyncPropertiesArrayWithCache()
{
    // Обновляем массив из кэша (для сервера перед репликацией)
    RuntimePropertiesArray.Empty();
    
    for (const auto& Pair : RuntimePropertiesCache)
    {
        RuntimePropertiesArray.Add(FItemRuntimeProperty(Pair.Key, Pair.Value));
    }
}

bool UMedComItemBase::GetItemDataCopy(FMedComUnifiedItemData& OutItemData) const
{
    const FMedComUnifiedItemData* Data = GetItemData();
    if (Data)
    {
        OutItemData = *Data;
        return true;
    }
    
    OutItemData = FMedComUnifiedItemData(); // Пустая структура
    return false;
}

void UMedComItemBase::OnRep_RuntimeProperties()
{
    // Вызывается на клиенте после репликации массива свойств
    // Синхронизируем локальный кэш с полученными данными
    SyncPropertiesCacheWithArray();
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("Runtime properties replicated for item %s, count: %d"), 
        *ItemID.ToString(), RuntimePropertiesArray.Num());
}
//==============================================================================
// Сетевая репликация
//==============================================================================

void UMedComItemBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Реплицируем только runtime данные
    DOREPLIFETIME(UMedComItemBase, ItemID);
    DOREPLIFETIME(UMedComItemBase, CurrentDurability);
    DOREPLIFETIME(UMedComItemBase, RuntimePropertiesArray); // Теперь массив вместо TMap
    DOREPLIFETIME(UMedComItemBase, LastUsedTime);
}

#if WITH_EDITOR
void UMedComItemBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    
    if (PropertyChangedEvent.Property)
    {
        const FName PropertyName = PropertyChangedEvent.Property->GetFName();
        
        // При изменении ItemID в редакторе, обновляем данные
        if (PropertyName == GET_MEMBER_NAME_CHECKED(UMedComItemBase, ItemID))
        {
            if (!ItemID.IsNone())
            {
                Initialize(ItemID);
            }
        }
    }
}
#endif