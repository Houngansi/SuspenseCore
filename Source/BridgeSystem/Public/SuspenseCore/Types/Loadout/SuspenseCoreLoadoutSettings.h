// SuspenseLoadoutSettings.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryLegacyTypes.h"
#include "SuspenseLoadoutSettings.generated.h"

/**
 * Equipment slot types for Tarkov-style MMO FPS
 */
UENUM(BlueprintType)
enum class EEquipmentSlotType : uint8
{
    None                    UMETA(DisplayName = "None"),

    // ===== WEAPONS =====
    PrimaryWeapon           UMETA(DisplayName = "Primary Weapon"),
    SecondaryWeapon         UMETA(DisplayName = "Secondary Weapon"),
    Holster                 UMETA(DisplayName = "Holster"),
    Scabbard                UMETA(DisplayName = "Scabbard"),

    // ===== HEAD GEAR =====
    Headwear                UMETA(DisplayName = "Headwear"),
    Earpiece                UMETA(DisplayName = "Earpiece"),
    Eyewear                 UMETA(DisplayName = "Eyewear"),
    FaceCover               UMETA(DisplayName = "Face Cover"),

    // ===== BODY GEAR =====
    BodyArmor               UMETA(DisplayName = "Body Armor"),
    TacticalRig             UMETA(DisplayName = "Tactical Rig"),

    // ===== STORAGE =====
    Backpack                UMETA(DisplayName = "Backpack"),
    SecureContainer         UMETA(DisplayName = "Secure Container"),

    // ===== QUICK ACCESS =====
    QuickSlot1              UMETA(DisplayName = "Quick Slot 1"),
    QuickSlot2              UMETA(DisplayName = "Quick Slot 2"),
    QuickSlot3              UMETA(DisplayName = "Quick Slot 3"),
    QuickSlot4              UMETA(DisplayName = "Quick Slot 4"),

    // ===== SPECIAL =====
    Armband                 UMETA(DisplayName = "Armband"),

    MAX                     UMETA(Hidden)
};

/**
 * Equipment slot configuration
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FEquipmentSlotConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Config")
    EEquipmentSlotType SlotType = EEquipmentSlotType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Config", meta = (Categories = "Equipment.Slot"))
    FGameplayTag SlotTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Config")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
    FName AttachmentSocket = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
    FTransform AttachmentOffset = FTransform::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Config", meta = (Categories = "Item"))
    FGameplayTagContainer AllowedItemTypes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Config", meta = (Categories = "Item"))
    FGameplayTagContainer DisallowedItemTypes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Config")
    bool bIsRequired = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Config")
    bool bIsVisible = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Config")
    FName DefaultItemID = NAME_None;

    FEquipmentSlotConfig() = default;

    FEquipmentSlotConfig(EEquipmentSlotType InSlotType, const FGameplayTag& InSlotTag)
        : SlotType(InSlotType)
        , SlotTag(InSlotTag)
        , AttachmentOffset(FTransform::Identity)
        , bIsRequired(false)
        , bIsVisible(true)
    {
        SetDefaultDisplayName();
    }

    bool CanEquipItemType(const FGameplayTag& ItemType) const
    {
        if (!DisallowedItemTypes.IsEmpty() && DisallowedItemTypes.HasTagExact(ItemType))
        {
            return false;
        }

        if (AllowedItemTypes.IsEmpty())
        {
            return true;
        }

        return AllowedItemTypes.HasTag(ItemType);
    }

    bool IsValid() const
    {
        return SlotType != EEquipmentSlotType::None && SlotTag.IsValid();
    }

private:
    void SetDefaultDisplayName()
    {
        switch (SlotType)
        {
            case EEquipmentSlotType::PrimaryWeapon:
                DisplayName = FText::FromString(TEXT("Primary Weapon"));
                break;
            case EEquipmentSlotType::SecondaryWeapon:
                DisplayName = FText::FromString(TEXT("Secondary Weapon"));
                break;
            case EEquipmentSlotType::Holster:
                DisplayName = FText::FromString(TEXT("Holster"));
                break;
            case EEquipmentSlotType::Scabbard:
                DisplayName = FText::FromString(TEXT("Scabbard"));
                break;
            case EEquipmentSlotType::Headwear:
                DisplayName = FText::FromString(TEXT("Headwear"));
                break;
            case EEquipmentSlotType::Earpiece:
                DisplayName = FText::FromString(TEXT("Earpiece"));
                break;
            case EEquipmentSlotType::Eyewear:
                DisplayName = FText::FromString(TEXT("Eyewear"));
                break;
            case EEquipmentSlotType::FaceCover:
                DisplayName = FText::FromString(TEXT("Face Cover"));
                break;
            case EEquipmentSlotType::BodyArmor:
                DisplayName = FText::FromString(TEXT("Body Armor"));
                break;
            case EEquipmentSlotType::TacticalRig:
                DisplayName = FText::FromString(TEXT("Tactical Rig"));
                break;
            case EEquipmentSlotType::Backpack:
                DisplayName = FText::FromString(TEXT("Backpack"));
                break;
            case EEquipmentSlotType::SecureContainer:
                DisplayName = FText::FromString(TEXT("Secure Container"));
                break;
            case EEquipmentSlotType::QuickSlot1:
                DisplayName = FText::FromString(TEXT("Quick Slot 1"));
                break;
            case EEquipmentSlotType::QuickSlot2:
                DisplayName = FText::FromString(TEXT("Quick Slot 2"));
                break;
            case EEquipmentSlotType::QuickSlot3:
                DisplayName = FText::FromString(TEXT("Quick Slot 3"));
                break;
            case EEquipmentSlotType::QuickSlot4:
                DisplayName = FText::FromString(TEXT("Quick Slot 4"));
                break;
            case EEquipmentSlotType::Armband:
                DisplayName = FText::FromString(TEXT("Armband"));
                break;
            default:
                DisplayName = FText::FromString(TEXT("Equipment Slot"));
                break;
        }
    }
};

/**
 * Inventory configuration
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseInventoryConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Config")
    FText InventoryName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid", meta = (ClampMin = "1", ClampMax = "50"))
    int32 Width = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid", meta = (ClampMin = "1", ClampMax = "50"))
    int32 Height = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits", meta = (ClampMin = "0.0"))
    float MaxWeight = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filters", meta = (Categories = "Item"))
    FGameplayTagContainer AllowedItemTypes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filters", meta = (Categories = "Item"))
    FGameplayTagContainer DisallowedItemTypes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Starting Items")
    TArray<FSuspensePickupSpawnData> StartingItems;

    FSuspenseInventoryConfig()
    {
        InventoryName = FText::FromString(TEXT("Inventory"));
        Width = 10;
        Height = 5;
        MaxWeight = 100.0f;
        AllowedItemTypes.Reset();
        DisallowedItemTypes.Reset();
    }

    FSuspenseInventoryConfig(const FText& InName, int32 InWidth, int32 InHeight, float InMaxWeight)
        : InventoryName(InName)
        , Width(FMath::Clamp(InWidth, 1, 50))
        , Height(FMath::Clamp(InHeight, 1, 50))
        , MaxWeight(FMath::Max(0.0f, InMaxWeight))
    {
        AllowedItemTypes.Reset();
        DisallowedItemTypes.Reset();
    }

    bool IsValid() const
    {
        return Width > 0 && Height > 0 && MaxWeight >= 0.0f;
    }

    int32 GetTotalCells() const
    {
        return Width * Height;
    }

    bool IsItemTypeAllowed(const FGameplayTag& ItemType) const
    {
        static const FGameplayTag BaseItemTag = FGameplayTag::RequestGameplayTag(TEXT("Item"));
        if (!ItemType.MatchesTag(BaseItemTag))
        {
            return false;
        }

        if (!DisallowedItemTypes.IsEmpty())
        {
            if (DisallowedItemTypes.HasTag(ItemType))
            {
                return false;
            }
        }

        if (AllowedItemTypes.IsEmpty())
        {
            return true;
        }

        return AllowedItemTypes.HasTag(ItemType);
    }
};

/**
 * Complete loadout configuration
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FLoadoutConfiguration : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Identity")
    FName LoadoutID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Identity")
    FText LoadoutName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Identity", meta = (MultiLine = "true"))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Identity")
    TSoftObjectPtr<UTexture2D> LoadoutIcon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Inventory")
    FSuspenseInventoryConfig MainInventory;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Inventory")
    TMap<FName, FSuspenseInventoryConfig> AdditionalInventories;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Equipment")
    TArray<FEquipmentSlotConfig> EquipmentSlots;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Equipment")
    TMap<EEquipmentSlotType, FName> StartingEquipment;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Limits", meta = (ClampMin = "0.0"))
    float MaxTotalWeight = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Limits", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float OverweightSpeedMultiplier = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Limits", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float OverweightThreshold = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Classification", meta = (Categories = "Loadout"))
    FGameplayTagContainer LoadoutTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Classification", meta = (Categories = "Character.Class"))
    FGameplayTagContainer CompatibleClasses;

    FLoadoutConfiguration()
    {
        LoadoutName = FText::FromString(TEXT("Default PMC Loadout"));
        Description = FText::FromString(TEXT("Standard PMC loadout configuration"));

        MainInventory = FSuspenseInventoryConfig(
            FText::FromString(TEXT("Pockets")),
            4, 1, 10.0f
        );

        MaxTotalWeight = 200.0f;
        OverweightSpeedMultiplier = 0.5f;
        OverweightThreshold = 0.8f;

        SetupDefaultEquipmentSlots();
    }

    const FSuspenseInventoryConfig* GetInventoryConfig(const FName& InventoryName = NAME_None) const
    {
        if (InventoryName.IsNone())
        {
            return &MainInventory;
        }
        return AdditionalInventories.Find(InventoryName);
    }

    const FEquipmentSlotConfig* GetEquipmentSlotConfig(EEquipmentSlotType SlotType) const
    {
        for (const FEquipmentSlotConfig& SlotConfig : EquipmentSlots)
        {
            if (SlotConfig.SlotType == SlotType)
            {
                return &SlotConfig;
            }
        }
        return nullptr;
    }

    void AddAdditionalInventory(const FName& InventoryName, const FSuspenseInventoryConfig& Config)
    {
        AdditionalInventories.Add(InventoryName, Config);
    }

    void AddEquipmentSlot(const FEquipmentSlotConfig& SlotConfig)
    {
        for (const FEquipmentSlotConfig& ExistingSlot : EquipmentSlots)
        {
            if (ExistingSlot.SlotType == SlotConfig.SlotType)
            {
                UE_LOG(LogTemp, Warning, TEXT("Equipment slot type already exists: %d"), (int32)SlotConfig.SlotType);
                return;
            }
        }
        EquipmentSlots.Add(SlotConfig);
    }

    float GetTotalInventoryWeight() const
    {
        float TotalWeight = MainInventory.MaxWeight;
        for (const auto& InventoryPair : AdditionalInventories)
        {
            TotalWeight += InventoryPair.Value.MaxWeight;
        }
        return TotalWeight;
    }

    int32 GetTotalInventoryCells() const
    {
        int32 TotalCells = MainInventory.GetTotalCells();
        for (const auto& InventoryPair : AdditionalInventories)
        {
            TotalCells += InventoryPair.Value.GetTotalCells();
        }
        return TotalCells;
    }

    bool IsValid() const
    {
        if (LoadoutID.IsNone())
        {
            return false;
        }

        if (!MainInventory.IsValid())
        {
            return false;
        }

        for (const auto& InventoryPair : AdditionalInventories)
        {
            if (!InventoryPair.Value.IsValid())
            {
                return false;
            }
        }

        for (const FEquipmentSlotConfig& SlotConfig : EquipmentSlots)
        {
            if (!SlotConfig.IsValid())
            {
                return false;
            }
        }

        return true;
    }

    bool IsCompatibleWithClass(const FGameplayTag& CharacterClass) const
    {
        if (CompatibleClasses.IsEmpty())
        {
            return true;
        }
        return CompatibleClasses.HasTag(CharacterClass);
    }

private:
    void SetupDefaultEquipmentSlots()
    {
        EquipmentSlots.Empty();

        // WEAPONS
        FEquipmentSlotConfig PrimaryWeaponSlot(
            EEquipmentSlotType::PrimaryWeapon,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.PrimaryWeapon"))
        );
        PrimaryWeaponSlot.AttachmentSocket = TEXT("weapon_r");
        PrimaryWeaponSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.AR")));
        PrimaryWeaponSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.DMR")));
        PrimaryWeaponSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.SR")));
        PrimaryWeaponSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Shotgun")));
        PrimaryWeaponSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.LMG")));
        EquipmentSlots.Add(PrimaryWeaponSlot);

        FEquipmentSlotConfig SecondaryWeaponSlot(
            EEquipmentSlotType::SecondaryWeapon,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.SecondaryWeapon"))
        );
        SecondaryWeaponSlot.AttachmentSocket = TEXT("spine_03");
        SecondaryWeaponSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.SMG")));
        SecondaryWeaponSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Shotgun")));
        SecondaryWeaponSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.PDW")));
        EquipmentSlots.Add(SecondaryWeaponSlot);

        FEquipmentSlotConfig HolsterSlot(
            EEquipmentSlotType::Holster,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Holster"))
        );
        HolsterSlot.AttachmentSocket = TEXT("thigh_r");
        HolsterSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Pistol")));
        HolsterSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Revolver")));
        EquipmentSlots.Add(HolsterSlot);

        FEquipmentSlotConfig ScabbardSlot(
            EEquipmentSlotType::Scabbard,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Scabbard"))
        );
        ScabbardSlot.AttachmentSocket = TEXT("spine_02");
        ScabbardSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Melee.Knife")));
        EquipmentSlots.Add(ScabbardSlot);

        // HEAD GEAR
        FEquipmentSlotConfig HeadwearSlot(
            EEquipmentSlotType::Headwear,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Headwear"))
        );
        HeadwearSlot.AttachmentSocket = TEXT("head");
        HeadwearSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Armor.Helmet")));
        HeadwearSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.Headwear")));
        EquipmentSlots.Add(HeadwearSlot);

        FEquipmentSlotConfig EarpieceSlot(
            EEquipmentSlotType::Earpiece,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Earpiece"))
        );
        EarpieceSlot.AttachmentSocket = TEXT("head");
        EarpieceSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.Earpiece")));
        EquipmentSlots.Add(EarpieceSlot);

        FEquipmentSlotConfig EyewearSlot(
            EEquipmentSlotType::Eyewear,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Eyewear"))
        );
        EyewearSlot.AttachmentSocket = TEXT("head");
        EyewearSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.Eyewear")));
        EquipmentSlots.Add(EyewearSlot);

        FEquipmentSlotConfig FaceCoverSlot(
            EEquipmentSlotType::FaceCover,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.FaceCover"))
        );
        FaceCoverSlot.AttachmentSocket = TEXT("head");
        FaceCoverSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.FaceCover")));
        EquipmentSlots.Add(FaceCoverSlot);

        // BODY GEAR
        FEquipmentSlotConfig BodyArmorSlot(
            EEquipmentSlotType::BodyArmor,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.BodyArmor"))
        );
        BodyArmorSlot.AttachmentSocket = TEXT("spine_03");
        BodyArmorSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Armor.BodyArmor")));
        EquipmentSlots.Add(BodyArmorSlot);

        FEquipmentSlotConfig TacticalRigSlot(
            EEquipmentSlotType::TacticalRig,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.TacticalRig"))
        );
        TacticalRigSlot.AttachmentSocket = TEXT("spine_03");
        TacticalRigSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.TacticalRig")));
        EquipmentSlots.Add(TacticalRigSlot);

        // STORAGE
        FEquipmentSlotConfig BackpackSlot(
            EEquipmentSlotType::Backpack,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Backpack"))
        );
        BackpackSlot.AttachmentSocket = TEXT("spine_02");
        BackpackSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.Backpack")));
        EquipmentSlots.Add(BackpackSlot);

        FEquipmentSlotConfig SecureContainerSlot(
            EEquipmentSlotType::SecureContainer,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.SecureContainer"))
        );
        SecureContainerSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.SecureContainer")));
        EquipmentSlots.Add(SecureContainerSlot);

        // QUICK SLOTS
        for (int32 i = 1; i <= 4; ++i)
        {
            EEquipmentSlotType QuickSlotType = static_cast<EEquipmentSlotType>(
                static_cast<int32>(EEquipmentSlotType::QuickSlot1) + i - 1
            );

            FString SlotName = FString::Printf(TEXT("Equipment.Slot.QuickSlot%d"), i);
            FEquipmentSlotConfig QuickSlot(
                QuickSlotType,
                FGameplayTag::RequestGameplayTag(*SlotName)
            );

            QuickSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Consumable")));
            QuickSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Medical")));
            QuickSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Throwable")));
            QuickSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Ammo")));
            EquipmentSlots.Add(QuickSlot);
        }

        // SPECIAL
        FEquipmentSlotConfig ArmbandSlot(
            EEquipmentSlotType::Armband,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Armband"))
        );
        ArmbandSlot.AttachmentSocket = TEXT("upperarm_l");
        ArmbandSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.Armband")));
        EquipmentSlots.Add(ArmbandSlot);
    }

#if WITH_EDITOR
public:
    virtual void OnDataTableChanged(const UDataTable* InDataTable, const FName InRowName) override
    {
        if (!IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("LoadoutConfiguration '%s' has validation errors"), *LoadoutID.ToString());
        }

        for (const FSuspensePickupSpawnData& StartingItem : MainInventory.StartingItems)
        {
            if (!StartingItem.IsValid())
            {
                UE_LOG(LogTemp, Warning, TEXT("Invalid starting item in main inventory: %s"), *StartingItem.ItemID.ToString());
            }
        }

        for (const auto& EquipmentPair : StartingEquipment)
        {
            if (EquipmentPair.Value.IsNone())
            {
                UE_LOG(LogTemp, Warning, TEXT("Empty starting equipment for slot: %d"), (int32)EquipmentPair.Key);
            }
        }
    }
};

