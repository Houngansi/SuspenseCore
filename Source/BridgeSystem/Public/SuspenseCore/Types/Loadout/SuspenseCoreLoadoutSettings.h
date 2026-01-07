// SuspenseLoadoutSettings.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryBaseTypes.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCoreLoadoutSettings.generated.h"

// Forward declaration
class USuspenseCoreEquipmentSlotPresets;

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

/** Aliases for consistent SuspenseCore naming conventions */
using ESuspenseCoreEquipmentSlotType = EEquipmentSlotType;

/**
 * Equipment slot configuration for loadout system
 * Use FSuspenseCoreEquipmentSlotConfig alias for consistent naming
 * Note: For UI-specific slot config, see FSuspenseCoreEquipmentSlotUIConfig in SuspenseCoreUIContainerTypes.h
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

    //========================================
    // UI Layout (for auto-positioning in canvas)
    //========================================

    /** Position on canvas panel (pixels) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Layout")
    FVector2D UIPosition = FVector2D::ZeroVector;

    /** Size of slot widget (pixels). If zero, uses DefaultSlotSize from widget. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Layout")
    FVector2D UISize = FVector2D(64.0f, 64.0f);

    /** Icon to show when slot is empty */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Layout")
    TSoftObjectPtr<UTexture2D> EmptySlotIcon;

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

/** SuspenseCore prefixed alias for slot configuration */
using FSuspenseCoreEquipmentSlotConfig = FEquipmentSlotConfig;

/**
 * Loadout inventory configuration
 * Used for configuring inventories within loadout definitions
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreLoadoutInventoryConfig
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
    TArray<FSuspenseCorePickupSpawnData> StartingItems;

    FSuspenseCoreLoadoutInventoryConfig()
    {
        InventoryName = FText::FromString(TEXT("Inventory"));
        Width = 10;
        Height = 5;
        MaxWeight = 100.0f;
        AllowedItemTypes.Reset();
        DisallowedItemTypes.Reset();
    }

    FSuspenseCoreLoadoutInventoryConfig(const FText& InName, int32 InWidth, int32 InHeight, float InMaxWeight)
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
        static const FGameplayTag BaseItemTag = FGameplayTag::RequestGameplayTag(TEXT("Item"), false);
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
    FSuspenseCoreLoadoutInventoryConfig MainInventory;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Inventory")
    TMap<FName, FSuspenseCoreLoadoutInventoryConfig> AdditionalInventories;

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

        MainInventory = FSuspenseCoreLoadoutInventoryConfig(
            FText::FromString(TEXT("Pockets")),
            4, 1, 10.0f
        );

        MaxTotalWeight = 200.0f;
        OverweightSpeedMultiplier = 0.5f;
        OverweightThreshold = 0.8f;

        SetupDefaultEquipmentSlots();
    }

    const FSuspenseCoreLoadoutInventoryConfig* GetInventoryConfig(const FName& InventoryName = NAME_None) const
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

    void AddAdditionalInventory(const FName& InventoryName, const FSuspenseCoreLoadoutInventoryConfig& Config)
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
    /**
     * Setup default equipment slots using Native Tags.
     * Called from constructor as fallback when no DataAsset is configured.
     *
     * NOTE: Prefer using USuspenseCoreEquipmentSlotPresets DataAsset
     * configured in Project Settings -> Game -> SuspenseCore -> EquipmentSlotPresetsAsset
     */
    void SetupDefaultEquipmentSlots()
    {
        using namespace SuspenseCoreTags;

        EquipmentSlots.Empty();
        EquipmentSlots.Reserve(17);

        // ===== WEAPONS =====

        // Primary Weapon (AR, DMR, SR, Shotgun, LMG)
        FEquipmentSlotConfig PrimaryWeaponSlot(EEquipmentSlotType::PrimaryWeapon, EquipmentSlot::PrimaryWeapon);
        PrimaryWeaponSlot.AttachmentSocket = TEXT("weapon_r");
        PrimaryWeaponSlot.AllowedItemTypes.AddTag(Item::Weapon::AR);
        PrimaryWeaponSlot.AllowedItemTypes.AddTag(Item::Weapon::DMR);
        PrimaryWeaponSlot.AllowedItemTypes.AddTag(Item::Weapon::SR);
        PrimaryWeaponSlot.AllowedItemTypes.AddTag(Item::Weapon::Shotgun);
        PrimaryWeaponSlot.AllowedItemTypes.AddTag(Item::Weapon::LMG);
        EquipmentSlots.Add(PrimaryWeaponSlot);

        // Secondary Weapon (SMG, Shotgun, PDW)
        FEquipmentSlotConfig SecondaryWeaponSlot(EEquipmentSlotType::SecondaryWeapon, EquipmentSlot::SecondaryWeapon);
        SecondaryWeaponSlot.AttachmentSocket = TEXT("spine_03");
        SecondaryWeaponSlot.AllowedItemTypes.AddTag(Item::Weapon::SMG);
        SecondaryWeaponSlot.AllowedItemTypes.AddTag(Item::Weapon::Shotgun);
        SecondaryWeaponSlot.AllowedItemTypes.AddTag(Item::Weapon::PDW);
        EquipmentSlots.Add(SecondaryWeaponSlot);

        // Holster (Pistol, Revolver)
        FEquipmentSlotConfig HolsterSlot(EEquipmentSlotType::Holster, EquipmentSlot::Holster);
        HolsterSlot.AttachmentSocket = TEXT("thigh_r");
        HolsterSlot.AllowedItemTypes.AddTag(Item::Weapon::Pistol);
        HolsterSlot.AllowedItemTypes.AddTag(Item::Weapon::Revolver);
        EquipmentSlots.Add(HolsterSlot);

        // Scabbard (Melee/Knife)
        FEquipmentSlotConfig ScabbardSlot(EEquipmentSlotType::Scabbard, EquipmentSlot::Scabbard);
        ScabbardSlot.AttachmentSocket = TEXT("spine_02");
        ScabbardSlot.AllowedItemTypes.AddTag(Item::Weapon::Melee::Knife);
        EquipmentSlots.Add(ScabbardSlot);

        // ===== HEAD GEAR =====

        // Headwear (Helmet, Headwear)
        FEquipmentSlotConfig HeadwearSlot(EEquipmentSlotType::Headwear, EquipmentSlot::Headwear);
        HeadwearSlot.AttachmentSocket = TEXT("head");
        HeadwearSlot.AllowedItemTypes.AddTag(Item::Armor::Helmet);
        HeadwearSlot.AllowedItemTypes.AddTag(Item::Gear::Headwear);
        EquipmentSlots.Add(HeadwearSlot);

        // Earpiece
        FEquipmentSlotConfig EarpieceSlot(EEquipmentSlotType::Earpiece, EquipmentSlot::Earpiece);
        EarpieceSlot.AttachmentSocket = TEXT("head");
        EarpieceSlot.AllowedItemTypes.AddTag(Item::Gear::Earpiece);
        EquipmentSlots.Add(EarpieceSlot);

        // Eyewear
        FEquipmentSlotConfig EyewearSlot(EEquipmentSlotType::Eyewear, EquipmentSlot::Eyewear);
        EyewearSlot.AttachmentSocket = TEXT("head");
        EyewearSlot.AllowedItemTypes.AddTag(Item::Gear::Eyewear);
        EquipmentSlots.Add(EyewearSlot);

        // Face Cover
        FEquipmentSlotConfig FaceCoverSlot(EEquipmentSlotType::FaceCover, EquipmentSlot::FaceCover);
        FaceCoverSlot.AttachmentSocket = TEXT("head");
        FaceCoverSlot.AllowedItemTypes.AddTag(Item::Gear::FaceCover);
        EquipmentSlots.Add(FaceCoverSlot);

        // ===== BODY GEAR =====

        // Body Armor
        FEquipmentSlotConfig BodyArmorSlot(EEquipmentSlotType::BodyArmor, EquipmentSlot::BodyArmor);
        BodyArmorSlot.AttachmentSocket = TEXT("spine_03");
        BodyArmorSlot.AllowedItemTypes.AddTag(Item::Armor::BodyArmor);
        EquipmentSlots.Add(BodyArmorSlot);

        // Tactical Rig
        FEquipmentSlotConfig TacticalRigSlot(EEquipmentSlotType::TacticalRig, EquipmentSlot::TacticalRig);
        TacticalRigSlot.AttachmentSocket = TEXT("spine_03");
        TacticalRigSlot.AllowedItemTypes.AddTag(Item::Gear::TacticalRig);
        EquipmentSlots.Add(TacticalRigSlot);

        // ===== STORAGE =====

        // Backpack
        FEquipmentSlotConfig BackpackSlot(EEquipmentSlotType::Backpack, EquipmentSlot::Backpack);
        BackpackSlot.AttachmentSocket = TEXT("spine_02");
        BackpackSlot.AllowedItemTypes.AddTag(Item::Gear::Backpack);
        EquipmentSlots.Add(BackpackSlot);

        // Secure Container
        FEquipmentSlotConfig SecureContainerSlot(EEquipmentSlotType::SecureContainer, EquipmentSlot::SecureContainer);
        SecureContainerSlot.AllowedItemTypes.AddTag(Item::Gear::SecureContainer);
        EquipmentSlots.Add(SecureContainerSlot);

        // ===== SPECIAL =====
        // NOTE: Armband MUST be at index 12, before QuickSlots (indices 13-16)
        // This matches SuspenseCoreEquipmentComponentBase slot mapping

        // Armband (index 12)
        FEquipmentSlotConfig ArmbandSlot(EEquipmentSlotType::Armband, EquipmentSlot::Armband);
        ArmbandSlot.AttachmentSocket = TEXT("upperarm_l");
        ArmbandSlot.AllowedItemTypes.AddTag(Item::Gear::Armband);
        EquipmentSlots.Add(ArmbandSlot);

        // ===== QUICK SLOTS =====
        // QuickSlots at indices 13-16 (matching EquipmentComponentBase)

        // Quick Slot 1 (index 13)
        FEquipmentSlotConfig QuickSlot1(EEquipmentSlotType::QuickSlot1, EquipmentSlot::QuickSlot1);
        QuickSlot1.AllowedItemTypes.AddTag(Item::Consumable);
        QuickSlot1.AllowedItemTypes.AddTag(Item::Medical);
        QuickSlot1.AllowedItemTypes.AddTag(Item::Throwable);
        QuickSlot1.AllowedItemTypes.AddTag(Item::Ammo);
        EquipmentSlots.Add(QuickSlot1);

        // Quick Slot 2 (index 14)
        FEquipmentSlotConfig QuickSlot2(EEquipmentSlotType::QuickSlot2, EquipmentSlot::QuickSlot2);
        QuickSlot2.AllowedItemTypes.AddTag(Item::Consumable);
        QuickSlot2.AllowedItemTypes.AddTag(Item::Medical);
        QuickSlot2.AllowedItemTypes.AddTag(Item::Throwable);
        QuickSlot2.AllowedItemTypes.AddTag(Item::Ammo);
        EquipmentSlots.Add(QuickSlot2);

        // Quick Slot 3 (index 15)
        FEquipmentSlotConfig QuickSlot3(EEquipmentSlotType::QuickSlot3, EquipmentSlot::QuickSlot3);
        QuickSlot3.AllowedItemTypes.AddTag(Item::Consumable);
        QuickSlot3.AllowedItemTypes.AddTag(Item::Medical);
        QuickSlot3.AllowedItemTypes.AddTag(Item::Throwable);
        QuickSlot3.AllowedItemTypes.AddTag(Item::Ammo);
        EquipmentSlots.Add(QuickSlot3);

        // Quick Slot 4 (index 16)
        FEquipmentSlotConfig QuickSlot4(EEquipmentSlotType::QuickSlot4, EquipmentSlot::QuickSlot4);
        QuickSlot4.AllowedItemTypes.AddTag(Item::Consumable);
        QuickSlot4.AllowedItemTypes.AddTag(Item::Medical);
        QuickSlot4.AllowedItemTypes.AddTag(Item::Throwable);
        QuickSlot4.AllowedItemTypes.AddTag(Item::Ammo);
        EquipmentSlots.Add(QuickSlot4);
    }

#if WITH_EDITOR
public:
    virtual void OnDataTableChanged(const UDataTable* InDataTable, const FName InRowName) override
    {
        if (!IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("LoadoutConfiguration '%s' has validation errors"), *LoadoutID.ToString());
        }

        for (const FSuspenseCorePickupSpawnData& StartingItem : MainInventory.StartingItems)
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
#endif
};

