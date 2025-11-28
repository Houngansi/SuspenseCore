// MedComInventoryTemplate.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "Templates/SuspenseInventoryTemplate.h"
#include "Components/SuspenseInventoryComponent.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "Base/SuspenseInventoryLogs.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

// Import InventoryUtils for new architecture
namespace InventoryUtils
{
    extern bool GetUnifiedItemData(const FName& ItemID, FSuspenseUnifiedItemData& OutData);
    extern float GetItemWeight(const FName& ItemID);
    extern FSuspenseInventoryItemInstance CreateItemInstance(const FName& ItemID, int32 Quantity);
}

//==================================================================
// Constructor and Basic Setup
//==================================================================

USuspenseInventoryTemplate::USuspenseInventoryTemplate()
{
    // Set up basic template values
    DisplayName = FText::FromString(TEXT("New Inventory Template"));
    Description = FText::FromString(TEXT("A customizable inventory template for character loadouts"));
    Icon = nullptr;
    TemplateCategory = FGameplayTag::RequestGameplayTag(TEXT("Template.Category.General"));
    ComplexityLevel = 1;

    // Set up main inventory with reasonable defaults
    MainInventoryConfig = FInventoryTemplateConfig();
    MainInventoryConfig.InventoryName = TEXT("MainInventory");
    MainInventoryConfig.GridWidth = 10;
    MainInventoryConfig.GridHeight = 5;
    MainInventoryConfig.MaxWeight = 100.0f;

    UE_LOG(LogInventory, VeryVerbose, TEXT("USuspenseInventoryTemplate: Constructor completed with default configuration"));
}

void USuspenseInventoryTemplate::PostLoad()
{
    Super::PostLoad();

    // Perform post-load validation
    TArray<FName> MissingItems;
    TArray<FString> ValidationErrors;

    if (!ValidateTemplate(MissingItems, ValidationErrors))
    {
        UE_LOG(LogInventory, Warning, TEXT("Template '%s' has validation issues after loading:"),
            *DisplayName.ToString());

        for (const FString& Error : ValidationErrors)
        {
            UE_LOG(LogInventory, Warning, TEXT("  - %s"), *Error);
        }

        if (MissingItems.Num() > 0)
        {
            UE_LOG(LogInventory, Warning, TEXT("  Missing items: %d"), MissingItems.Num());
            for (const FName& MissingItem : MissingItems)
            {
                UE_LOG(LogInventory, Warning, TEXT("    - %s"), *MissingItem.ToString());
            }
        }
    }
}

//==================================================================
// Template Export and Application
//==================================================================

TMap<FName, FSuspenseInventoryConfig> USuspenseInventoryTemplate::ExportInventoryConfigurations() const
{
    TMap<FName, FSuspenseInventoryConfig> Configurations;

    UE_LOG(LogInventory, Log, TEXT("ExportInventoryConfigurations: Exporting %d inventory configurations"),
        1 + AdditionalInventories.Num());

    // Export main inventory
    Configurations.Add(MainInventoryConfig.InventoryName, MainInventoryConfig.ToInventoryConfig());

    // Export additional inventories
    for (const auto& Pair : AdditionalInventories)
    {
        Configurations.Add(Pair.Key, Pair.Value.ToInventoryConfig());
    }

    return Configurations;
}

bool USuspenseInventoryTemplate::ApplyToInventoryComponent(USuspenseInventoryComponent* InventoryComponent,
                                                       const FName& InventoryName) const
{
    if (!InventoryComponent)
    {
        UE_LOG(LogInventory, Error, TEXT("ApplyToInventoryComponent: Invalid inventory component"));
        return false;
    }

    UE_LOG(LogInventory, Log, TEXT("ApplyToInventoryComponent: Applying template '%s' to component"),
        *DisplayName.ToString());

    // Get the configuration to apply
    const FInventoryTemplateConfig* ConfigToUse = GetInventoryConfig(InventoryName);
    if (!ConfigToUse)
    {
        UE_LOG(LogInventory, Error, TEXT("ApplyToInventoryComponent: Configuration not found for inventory '%s'"),
            *InventoryName.ToString());
        return false;
    }

    // Apply configuration to component
    FSuspenseInventoryConfig Config = ConfigToUse->ToInventoryConfig();
    InventoryComponent->InitializeInventory(Config);

    // Create starting items if configured
    if (ConfigToUse->bAutoPlaceStartingItems && ConfigToUse->StartingItems.Num() > 0)
    {
        int32 CreatedItems = CreateStartingItemsInInventory(InventoryComponent, InventoryName);
        UE_LOG(LogInventory, Log, TEXT("ApplyToInventoryComponent: Created %d starting items"), CreatedItems);
    }

    UE_LOG(LogInventory, Log, TEXT("ApplyToInventoryComponent: Successfully applied template '%s'"),
        *DisplayName.ToString());

    return true;
}

int32 USuspenseInventoryTemplate::CreateStartingItemsInInventory(USuspenseInventoryComponent* InventoryComponent,
                                                             const FName& InventoryName) const
{
    if (!InventoryComponent)
    {
        UE_LOG(LogInventory, Error, TEXT("CreateStartingItemsInInventory: Invalid inventory component"));
        return 0;
    }

    // Get configuration
    const FInventoryTemplateConfig* ConfigToUse = GetInventoryConfig(InventoryName);
    if (!ConfigToUse)
    {
        UE_LOG(LogInventory, Error, TEXT("CreateStartingItemsInInventory: Configuration not found for inventory '%s'"),
            *InventoryName.ToString());
        return 0;
    }

    UE_LOG(LogInventory, Log, TEXT("CreateStartingItemsInInventory: Creating %d starting items"),
        ConfigToUse->StartingItems.Num());

    int32 SuccessCount = 0;

    // Create each starting item
    for (const FSuspensePickupSpawnData& SpawnData : ConfigToUse->StartingItems)
    {
        if (!SpawnData.IsValid())
        {
            UE_LOG(LogInventory, Warning, TEXT("CreateStartingItemsInInventory: Invalid spawn data for item: %s"),
                *SpawnData.ItemID.ToString());
            continue;
        }

        // Create runtime instance
        FSuspenseInventoryItemInstance NewInstance = SpawnData.CreateInventoryInstance();

        // Add to inventory
        FSuspenseInventoryOperationResult AddResult = InventoryComponent->AddItemInstance(NewInstance);
        if (AddResult.bSuccess)
        {
            SuccessCount++;
            UE_LOG(LogInventory, VeryVerbose, TEXT("CreateStartingItemsInInventory: Created item: %s"),
                *NewInstance.GetShortDebugString());
        }
        else
        {
            UE_LOG(LogInventory, Warning, TEXT("CreateStartingItemsInInventory: Failed to add item: %s. Reason: %s"),
                *SpawnData.ItemID.ToString(), *AddResult.ErrorMessage.ToString());
        }
    }

    UE_LOG(LogInventory, Log, TEXT("CreateStartingItemsInInventory: Successfully created %d/%d items"),
        SuccessCount, ConfigToUse->StartingItems.Num());

    return SuccessCount;
}

//==================================================================
// Validation and Analysis
//==================================================================

bool USuspenseInventoryTemplate::ValidateTemplate(TArray<FName>& OutMissingItems, TArray<FString>& OutValidationErrors) const
{
    OutMissingItems.Empty();
    OutValidationErrors.Empty();

    UE_LOG(LogInventory, VeryVerbose, TEXT("ValidateTemplate: Validating template '%s'"), *DisplayName.ToString());

    bool bIsValid = true;

    // Basic validation
    if (DisplayName.IsEmpty())
    {
        OutValidationErrors.Add(TEXT("Template must have a display name"));
        bIsValid = false;
    }

    // Validate main inventory
    if (!ValidateInventoryConfig(MainInventoryConfig, TEXT("MainInventory"), OutValidationErrors))
    {
        bIsValid = false;
    }

    // Validate additional inventories
    for (const auto& Pair : AdditionalInventories)
    {
        if (!ValidateInventoryConfig(Pair.Value, Pair.Key.ToString(), OutValidationErrors))
        {
            bIsValid = false;
        }
    }

    // Validate starting items
    if (!ValidateStartingItems(MainInventoryConfig.StartingItems, OutMissingItems, OutValidationErrors))
    {
        bIsValid = false;
    }

    for (const auto& Pair : AdditionalInventories)
    {
        if (!ValidateStartingItems(Pair.Value.StartingItems, OutMissingItems, OutValidationErrors))
        {
            bIsValid = false;
        }
    }

    UE_LOG(LogInventory, Log, TEXT("ValidateTemplate: Validation completed - Valid: %s, Errors: %d, Missing: %d"),
        bIsValid ? TEXT("Yes") : TEXT("No"), OutValidationErrors.Num(), OutMissingItems.Num());

    return bIsValid;
}

FInventoryTemplateAnalysis USuspenseInventoryTemplate::AnalyzeTemplate() const
{
    FInventoryTemplateAnalysis Analysis;

    UE_LOG(LogInventory, VeryVerbose, TEXT("AnalyzeTemplate: Analyzing template '%s'"),
        *DisplayName.ToString());

    // Analyze main inventory
    Analysis.TotalGridCells += MainInventoryConfig.GetTotalCells();
    Analysis.TotalMaxWeight += MainInventoryConfig.MaxWeight;
    Analysis.InventoryCount = 1;
    Analysis.StartingItemCount += MainInventoryConfig.StartingItems.Num();

    // Analyze additional inventories
    for (const auto& Pair : AdditionalInventories)
    {
        const FInventoryTemplateConfig& Config = Pair.Value;
        Analysis.TotalGridCells += Config.GetTotalCells();
        Analysis.TotalMaxWeight += Config.MaxWeight;
        Analysis.InventoryCount++;
        Analysis.StartingItemCount += Config.StartingItems.Num();
    }

    // Analyze starting items
    TSet<FGameplayTag> UniqueTypes;
    float EstimatedWeight = 0.0f;

    USuspenseItemManager* ItemManager = GetItemManager();
    if (ItemManager)
    {
        // Main inventory items
        for (const FSuspensePickupSpawnData& SpawnData : MainInventoryConfig.StartingItems)
        {
            FSuspenseUnifiedItemData ItemData;
            if (ItemManager->GetUnifiedItemData(SpawnData.ItemID, ItemData))
            {
                UniqueTypes.Add(ItemData.ItemType);
                EstimatedWeight += ItemData.Weight * SpawnData.Quantity;
            }
        }

        // Additional inventory items
        for (const auto& Pair : AdditionalInventories)
        {
            for (const FSuspensePickupSpawnData& SpawnData : Pair.Value.StartingItems)
            {
                FSuspenseUnifiedItemData ItemData;
                if (ItemManager->GetUnifiedItemData(SpawnData.ItemID, ItemData))
                {
                    UniqueTypes.Add(ItemData.ItemType);
                    EstimatedWeight += ItemData.Weight * SpawnData.Quantity;
                }
            }
        }
    }

    Analysis.UniqueItemTypes = UniqueTypes.Array();
    Analysis.EstimatedStartingWeight = EstimatedWeight;

    // Determine complexity
    Analysis.bIsComplex = (Analysis.InventoryCount > 2) ||
                         (Analysis.TotalGridCells > 100) ||
                         (Analysis.StartingItemCount > 20);

    // Generate warnings
    if (Analysis.EstimatedStartingWeight > Analysis.TotalMaxWeight * 0.8f)
    {
        Analysis.Warnings.Add(TEXT("Starting items may exceed 80% of total weight capacity"));
    }

    if (Analysis.StartingItemCount > Analysis.TotalGridCells * 0.5f)
    {
        Analysis.Warnings.Add(TEXT("Starting items may require more than 50% of available grid space"));
    }

    if (Analysis.InventoryCount > 3)
    {
        Analysis.Warnings.Add(TEXT("Template has many inventories - may be complex for new players"));
    }

    UE_LOG(LogInventory, Log, TEXT("AnalyzeTemplate: Analysis complete - Complex: %s, Warnings: %d"),
        Analysis.bIsComplex ? TEXT("Yes") : TEXT("No"), Analysis.Warnings.Num());

    return Analysis;
}

bool USuspenseInventoryTemplate::IsCompatibleWithCharacterClass(const FGameplayTag& CharacterClass) const
{
    // If no restrictions, compatible with all
    if (CompatibleCharacterClasses.IsEmpty())
    {
        return true;
    }

    // Check exact or hierarchical match
    return CompatibleCharacterClasses.HasTag(CharacterClass);
}

//==================================================================
// Static Template Factory Implementation
//==================================================================

TArray<USuspenseInventoryTemplate*> USuspenseInventoryTemplate::GetPredefinedTemplates(const FGameplayTag& CategoryFilter)
{
    TArray<USuspenseInventoryTemplate*> Templates;

    UE_LOG(LogInventory, Log, TEXT("GetPredefinedTemplates: Creating predefined templates"));

    // Create all basic templates
    Templates.Add(CreateMinimalTemplate());
    Templates.Add(CreateStandardTemplate());
    Templates.Add(CreateExpandedTemplate());
    Templates.Add(CreateSurvivalTemplate());

    // Create class-specific templates
    Templates.Add(CreateClassSpecificTemplate(FGameplayTag::RequestGameplayTag(TEXT("Character.Class.Soldier"))));
    Templates.Add(CreateClassSpecificTemplate(FGameplayTag::RequestGameplayTag(TEXT("Character.Class.Medic"))));
    Templates.Add(CreateClassSpecificTemplate(FGameplayTag::RequestGameplayTag(TEXT("Character.Class.Engineer"))));

    // Filter by category if specified
    if (CategoryFilter.IsValid())
    {
        Templates.RemoveAll([&CategoryFilter](const USuspenseInventoryTemplate* Template)
        {
            return Template && !Template->TemplateCategory.MatchesTag(CategoryFilter);
        });
    }

    UE_LOG(LogInventory, Log, TEXT("GetPredefinedTemplates: Created %d templates"), Templates.Num());

    return Templates;
}

USuspenseInventoryTemplate* USuspenseInventoryTemplate::CreateMinimalTemplate()
{
    USuspenseInventoryTemplate* Template = NewObject<USuspenseInventoryTemplate>();

    // Basic properties
    Template->DisplayName = FText::FromString(TEXT("Minimal Inventory"));
    Template->Description = FText::FromString(TEXT("Lightweight inventory for quick missions"));
    Template->TemplateCategory = FGameplayTag::RequestGameplayTag(TEXT("Template.Category.Minimal"));
    Template->ComplexityLevel = 1;

    // Main inventory - small and light
    Template->MainInventoryConfig.InventoryName = TEXT("MainInventory");
    Template->MainInventoryConfig.GridWidth = 6;
    Template->MainInventoryConfig.GridHeight = 4;
    Template->MainInventoryConfig.MaxWeight = 50.0f;

    // Basic starting items
    FSuspensePickupSpawnData HealthKit;
    HealthKit.ItemID = TEXT("Consumable_HealthKit_Small");
    HealthKit.Quantity = 2;
    Template->MainInventoryConfig.StartingItems.Add(HealthKit);

    FSuspensePickupSpawnData BasicAmmo;
    BasicAmmo.ItemID = TEXT("Ammo_Universal_Small");
    BasicAmmo.Quantity = 3;
    Template->MainInventoryConfig.StartingItems.Add(BasicAmmo);

    UE_LOG(LogInventory, VeryVerbose, TEXT("CreateMinimalTemplate: Created minimal template"));

    return Template;
}

USuspenseInventoryTemplate* USuspenseInventoryTemplate::CreateStandardTemplate()
{
    USuspenseInventoryTemplate* Template = NewObject<USuspenseInventoryTemplate>();

    // Basic properties
    Template->DisplayName = FText::FromString(TEXT("Standard Inventory"));
    Template->Description = FText::FromString(TEXT("Balanced inventory for general purpose"));
    Template->TemplateCategory = FGameplayTag::RequestGameplayTag(TEXT("Template.Category.Standard"));
    Template->ComplexityLevel = 2;

    // Main inventory - standard size
    Template->MainInventoryConfig.InventoryName = TEXT("MainInventory");
    Template->MainInventoryConfig.GridWidth = 10;
    Template->MainInventoryConfig.GridHeight = 6;
    Template->MainInventoryConfig.MaxWeight = 100.0f;

    // Add backpack
    FInventoryTemplateConfig BackpackConfig;
    BackpackConfig.InventoryName = TEXT("Backpack");
    BackpackConfig.GridWidth = 8;
    BackpackConfig.GridHeight = 5;
    BackpackConfig.MaxWeight = 80.0f;
    BackpackConfig.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Type.Consumable")));
    BackpackConfig.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Type.Ammo")));

    // Starting items for backpack
    FSuspensePickupSpawnData HealthKits;
    HealthKits.ItemID = TEXT("Consumable_HealthKit_Medium");
    HealthKits.Quantity = 4;
    BackpackConfig.StartingItems.Add(HealthKits);

    FSuspensePickupSpawnData AmmoBox;
    AmmoBox.ItemID = TEXT("Ammo_Universal_Standard");
    AmmoBox.Quantity = 6;
    BackpackConfig.StartingItems.Add(AmmoBox);

    Template->AdditionalInventories.Add(TEXT("Backpack"), BackpackConfig);

    UE_LOG(LogInventory, VeryVerbose, TEXT("CreateStandardTemplate: Created standard template"));

    return Template;
}

USuspenseInventoryTemplate* USuspenseInventoryTemplate::CreateExpandedTemplate()
{
    USuspenseInventoryTemplate* Template = NewObject<USuspenseInventoryTemplate>();

    // Basic properties
    Template->DisplayName = FText::FromString(TEXT("Expanded Inventory"));
    Template->Description = FText::FromString(TEXT("Large capacity inventory for extended operations"));
    Template->TemplateCategory = FGameplayTag::RequestGameplayTag(TEXT("Template.Category.Expanded"));
    Template->ComplexityLevel = 4;

    // Main inventory - enlarged
    Template->MainInventoryConfig.InventoryName = TEXT("MainInventory");
    Template->MainInventoryConfig.GridWidth = 12;
    Template->MainInventoryConfig.GridHeight = 8;
    Template->MainInventoryConfig.MaxWeight = 150.0f;

    // Large backpack
    FInventoryTemplateConfig BackpackConfig;
    BackpackConfig.InventoryName = TEXT("Backpack");
    BackpackConfig.GridWidth = 10;
    BackpackConfig.GridHeight = 8;
    BackpackConfig.MaxWeight = 120.0f;
    Template->AdditionalInventories.Add(TEXT("Backpack"), BackpackConfig);

    // Utility belt
    FInventoryTemplateConfig UtilityConfig;
    UtilityConfig.InventoryName = TEXT("UtilityBelt");
    UtilityConfig.GridWidth = 8;
    UtilityConfig.GridHeight = 2;
    UtilityConfig.MaxWeight = 30.0f;
    UtilityConfig.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Type.Tool")));
    UtilityConfig.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Type.Consumable")));
    Template->AdditionalInventories.Add(TEXT("UtilityBelt"), UtilityConfig);

    // Diverse starting items
    FSuspensePickupSpawnData HealthKits;
    HealthKits.ItemID = TEXT("Consumable_HealthKit_Large");
    HealthKits.Quantity = 6;
    BackpackConfig.StartingItems.Add(HealthKits);

    FSuspensePickupSpawnData MultiTool;
    MultiTool.ItemID = TEXT("Tool_MultiTool");
    MultiTool.Quantity = 1;
    UtilityConfig.StartingItems.Add(MultiTool);

    Template->AdditionalInventories[TEXT("Backpack")] = BackpackConfig;
    Template->AdditionalInventories[TEXT("UtilityBelt")] = UtilityConfig;

    UE_LOG(LogInventory, VeryVerbose, TEXT("CreateExpandedTemplate: Created expanded template"));

    return Template;
}

USuspenseInventoryTemplate* USuspenseInventoryTemplate::CreateSurvivalTemplate()
{
    USuspenseInventoryTemplate* Template = NewObject<USuspenseInventoryTemplate>();

    // Basic properties
    Template->DisplayName = FText::FromString(TEXT("Survival Inventory"));
    Template->Description = FText::FromString(TEXT("Specialized inventory for survival scenarios"));
    Template->TemplateCategory = FGameplayTag::RequestGameplayTag(TEXT("Template.Category.Survival"));
    Template->ComplexityLevel = 3;

    // Main inventory
    Template->MainInventoryConfig.InventoryName = TEXT("MainInventory");
    Template->MainInventoryConfig.GridWidth = 10;
    Template->MainInventoryConfig.GridHeight = 7;
    Template->MainInventoryConfig.MaxWeight = 120.0f;

    // Survival backpack
    FInventoryTemplateConfig BackpackConfig;
    BackpackConfig.InventoryName = TEXT("SurvivalBackpack");
    BackpackConfig.GridWidth = 10;
    BackpackConfig.GridHeight = 6;
    BackpackConfig.MaxWeight = 100.0f;

    // Tool pouch
    FInventoryTemplateConfig ToolConfig;
    ToolConfig.InventoryName = TEXT("ToolPouch");
    ToolConfig.GridWidth = 6;
    ToolConfig.GridHeight = 3;
    ToolConfig.MaxWeight = 40.0f;
    ToolConfig.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Type.Tool")));

    // Survival items
    FSuspensePickupSpawnData SurvivalKit;
    SurvivalKit.ItemID = TEXT("Consumable_SurvivalKit");
    SurvivalKit.Quantity = 3;
    BackpackConfig.StartingItems.Add(SurvivalKit);

    FSuspensePickupSpawnData WaterPurifier;
    WaterPurifier.ItemID = TEXT("Tool_WaterPurifier");
    WaterPurifier.Quantity = 1;
    ToolConfig.StartingItems.Add(WaterPurifier);

    FSuspensePickupSpawnData FireStarter;
    FireStarter.ItemID = TEXT("Tool_FireStarter");
    FireStarter.Quantity = 2;
    ToolConfig.StartingItems.Add(FireStarter);

    Template->AdditionalInventories.Add(TEXT("SurvivalBackpack"), BackpackConfig);
    Template->AdditionalInventories.Add(TEXT("ToolPouch"), ToolConfig);

    UE_LOG(LogInventory, VeryVerbose, TEXT("CreateSurvivalTemplate: Created survival template"));

    return Template;
}

USuspenseInventoryTemplate* USuspenseInventoryTemplate::CreateClassSpecificTemplate(const FGameplayTag& CharacterClass)
{
    USuspenseInventoryTemplate* Template = NewObject<USuspenseInventoryTemplate>();

    // Base configuration depends on class
    if (CharacterClass.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Character.Class.Soldier"))))
    {
        Template->DisplayName = FText::FromString(TEXT("Soldier Inventory"));
        Template->Description = FText::FromString(TEXT("Military-optimized inventory layout"));
        Template->ComplexityLevel = 2;

        // Soldier-specific setup
        Template->MainInventoryConfig.GridWidth = 10;
        Template->MainInventoryConfig.GridHeight = 6;
        Template->MainInventoryConfig.MaxWeight = 120.0f;

        // Add ammo pouch
        FInventoryTemplateConfig AmmoPouch;
        AmmoPouch.InventoryName = TEXT("AmmoPouch");
        AmmoPouch.GridWidth = 6;
        AmmoPouch.GridHeight = 4;
        AmmoPouch.MaxWeight = 50.0f;
        AmmoPouch.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Type.Ammo")));

        FSuspensePickupSpawnData MilitaryAmmo;
        MilitaryAmmo.ItemID = TEXT("Ammo_Military_Standard");
        MilitaryAmmo.Quantity = 8;
        AmmoPouch.StartingItems.Add(MilitaryAmmo);

        Template->AdditionalInventories.Add(TEXT("AmmoPouch"), AmmoPouch);
    }
    else if (CharacterClass.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Character.Class.Medic"))))
    {
        Template->DisplayName = FText::FromString(TEXT("Medic Inventory"));
        Template->Description = FText::FromString(TEXT("Medical specialist inventory"));
        Template->ComplexityLevel = 3;

        // Medic-specific setup
        Template->MainInventoryConfig.GridWidth = 8;
        Template->MainInventoryConfig.GridHeight = 6;
        Template->MainInventoryConfig.MaxWeight = 80.0f;

        // Medical bag
        FInventoryTemplateConfig MedicalBag;
        MedicalBag.InventoryName = TEXT("MedicalBag");
        MedicalBag.GridWidth = 10;
        MedicalBag.GridHeight = 8;
        MedicalBag.MaxWeight = 100.0f;
        MedicalBag.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Type.Medical")));

        FSuspensePickupSpawnData AdvancedMedKit;
        AdvancedMedKit.ItemID = TEXT("Medical_AdvancedKit");
        AdvancedMedKit.Quantity = 3;
        MedicalBag.StartingItems.Add(AdvancedMedKit);

        FSuspensePickupSpawnData Morphine;
        Morphine.ItemID = TEXT("Medical_Morphine");
        Morphine.Quantity = 6;
        MedicalBag.StartingItems.Add(Morphine);

        Template->AdditionalInventories.Add(TEXT("MedicalBag"), MedicalBag);
    }
    else
    {
        // Generic class-specific template
        Template->DisplayName = FText::FromString(TEXT("Specialized Inventory"));
        Template->Description = FText::FromString(TEXT("Class-optimized inventory"));
        Template->ComplexityLevel = 2;

        // Basic generic setup
        Template->MainInventoryConfig.GridWidth = 8;
        Template->MainInventoryConfig.GridHeight = 5;
        Template->MainInventoryConfig.MaxWeight = 100.0f;
    }

    // Common settings
    Template->TemplateCategory = FGameplayTag::RequestGameplayTag(TEXT("Template.Category.ClassSpecific"));
    Template->CompatibleCharacterClasses.AddTag(CharacterClass);

    UE_LOG(LogInventory, VeryVerbose, TEXT("CreateClassSpecificTemplate: Created template for class '%s'"),
        *CharacterClass.ToString());

    return Template;
}

//==================================================================
// Template Customization
//==================================================================

USuspenseInventoryTemplate* USuspenseInventoryTemplate::CreateCustomizedCopy(const TMap<FString, FString>& Modifications) const
{
    USuspenseInventoryTemplate* CustomizedTemplate = DuplicateObject<USuspenseInventoryTemplate>(this, nullptr);

    UE_LOG(LogInventory, Log, TEXT("CreateCustomizedCopy: Creating customized copy with %d modifications"),
        Modifications.Num());

    // Apply modifications
    for (const auto& Pair : Modifications)
    {
        const FString& PropertyName = Pair.Key;
        const FString& PropertyValue = Pair.Value;

        if (PropertyName == TEXT("DisplayName"))
        {
            CustomizedTemplate->DisplayName = FText::FromString(PropertyValue);
        }
        else if (PropertyName == TEXT("MainGridWidth"))
        {
            CustomizedTemplate->MainInventoryConfig.GridWidth = FCString::Atoi(*PropertyValue);
        }
        else if (PropertyName == TEXT("MainGridHeight"))
        {
            CustomizedTemplate->MainInventoryConfig.GridHeight = FCString::Atoi(*PropertyValue);
        }
        else if (PropertyName == TEXT("MainMaxWeight"))
        {
            CustomizedTemplate->MainInventoryConfig.MaxWeight = FCString::Atof(*PropertyValue);
        }
        else
        {
            UE_LOG(LogInventory, Warning, TEXT("CreateCustomizedCopy: Unknown modification property '%s'"),
                *PropertyName);
        }
    }

    UE_LOG(LogInventory, Log, TEXT("CreateCustomizedCopy: Created customized template"));

    return CustomizedTemplate;
}

bool USuspenseInventoryTemplate::AddAdditionalInventory(const FName& InventoryName, const FInventoryTemplateConfig& Configuration)
{
    if (InventoryName.IsNone())
    {
        UE_LOG(LogInventory, Error, TEXT("AddAdditionalInventory: Invalid inventory name"));
        return false;
    }

    if (AdditionalInventories.Contains(InventoryName))
    {
        UE_LOG(LogInventory, Warning, TEXT("AddAdditionalInventory: Inventory '%s' already exists, replacing"),
            *InventoryName.ToString());
    }

    if (!Configuration.IsValid())
    {
        UE_LOG(LogInventory, Error, TEXT("AddAdditionalInventory: Invalid configuration for inventory '%s'"),
            *InventoryName.ToString());
        return false;
    }

    AdditionalInventories.Add(InventoryName, Configuration);

    UE_LOG(LogInventory, Log, TEXT("AddAdditionalInventory: Added inventory '%s' (%dx%d)"),
        *InventoryName.ToString(), Configuration.GridWidth, Configuration.GridHeight);

    return true;
}

bool USuspenseInventoryTemplate::RemoveAdditionalInventory(const FName& InventoryName)
{
    if (AdditionalInventories.Remove(InventoryName) > 0)
    {
        UE_LOG(LogInventory, Log, TEXT("RemoveAdditionalInventory: Removed inventory '%s'"),
            *InventoryName.ToString());
        return true;
    }

    UE_LOG(LogInventory, Warning, TEXT("RemoveAdditionalInventory: Inventory '%s' not found"),
        *InventoryName.ToString());
    return false;
}

//==================================================================
// Internal Helper Methods
//==================================================================

USuspenseItemManager* USuspenseInventoryTemplate::GetItemManager() const
{
    // Try to get ItemManager from any available world context
    if (GEngine && GEngine->GetWorldContexts().Num() > 0)
    {
        UWorld* World = GEngine->GetWorldContexts()[0].World();
        if (World && World->GetGameInstance())
        {
            return World->GetGameInstance()->GetSubsystem<USuspenseItemManager>();
        }
    }

    return nullptr;
}

bool USuspenseInventoryTemplate::ValidateInventoryConfig(const FInventoryTemplateConfig& Config,
                                                      const FString& ConfigName,
                                                      TArray<FString>& OutErrors) const
{
    bool bIsValid = true;

    if (!Config.IsValid())
    {
        OutErrors.Add(FString::Printf(TEXT("%s: Basic configuration validation failed"), *ConfigName));
        bIsValid = false;
    }

    if (Config.GridWidth * Config.GridHeight > 1000)
    {
        OutErrors.Add(FString::Printf(TEXT("%s: Grid too large (%dx%d = %d cells, max 1000)"),
            *ConfigName, Config.GridWidth, Config.GridHeight, Config.GridWidth * Config.GridHeight));
        bIsValid = false;
    }

    if (Config.MaxWeight > 1000.0f)
    {
        OutErrors.Add(FString::Printf(TEXT("%s: Max weight too high (%.1f, max 1000.0)"),
            *ConfigName, Config.MaxWeight));
        bIsValid = false;
    }

    return bIsValid;
}

bool USuspenseInventoryTemplate::ValidateStartingItems(const TArray<FSuspensePickupSpawnData>& StartingItems,
                                                    TArray<FName>& OutMissingItems,
                                                    TArray<FString>& OutErrors) const
{
    bool bIsValid = true;
    USuspenseItemManager* ItemManager = GetItemManager();

    if (!ItemManager)
    {
        OutErrors.Add(TEXT("ItemManager not available for starting items validation"));
        return false;
    }

    for (const FSuspensePickupSpawnData& SpawnData : StartingItems)
    {
        if (!SpawnData.IsValid())
        {
            OutErrors.Add(FString::Printf(TEXT("Invalid spawn data for item: %s"),
                *SpawnData.ItemID.ToString()));
            bIsValid = false;
            continue;
        }

        FSuspenseUnifiedItemData ItemData;
        if (!ItemManager->GetUnifiedItemData(SpawnData.ItemID, ItemData))
        {
            OutMissingItems.AddUnique(SpawnData.ItemID);
            OutErrors.Add(FString::Printf(TEXT("Starting item '%s' not found in DataTable"),
                *SpawnData.ItemID.ToString()));
            bIsValid = false;
        }
        else
        {
            // Validate quantity against max stack size
            if (SpawnData.Quantity > ItemData.MaxStackSize)
            {
                OutErrors.Add(FString::Printf(TEXT("Starting item '%s' quantity %d exceeds max stack size %d"),
                    *SpawnData.ItemID.ToString(), SpawnData.Quantity, ItemData.MaxStackSize));
                bIsValid = false;
            }
        }
    }

    return bIsValid;
}

const FInventoryTemplateConfig* USuspenseInventoryTemplate::GetInventoryConfig(const FName& InventoryName) const
{
    if (InventoryName.IsNone() || InventoryName == MainInventoryConfig.InventoryName)
    {
        return &MainInventoryConfig;
    }

    return AdditionalInventories.Find(InventoryName);
}

#if WITH_EDITOR
EDataValidationResult USuspenseInventoryTemplate::IsDataValid(TArray<FText>& ValidationErrors)
{
    EDataValidationResult Result = Super::IsDataValid(ValidationErrors);

    // Perform additional validation in editor
    TArray<FName> MissingItems;
    TArray<FString> CustomErrors;

    if (!ValidateTemplate(MissingItems, CustomErrors))
    {
        for (const FString& Error : CustomErrors)
        {
            ValidationErrors.Add(FText::FromString(Error));
        }
        Result = EDataValidationResult::Invalid;
    }

    if (MissingItems.Num() > 0)
    {
        FString MissingItemsStr = TEXT("Missing items in DataTable: ");
        for (int32 i = 0; i < MissingItems.Num(); ++i)
        {
            MissingItemsStr += MissingItems[i].ToString();
            if (i < MissingItems.Num() - 1)
            {
                MissingItemsStr += TEXT(", ");
            }
        }
        ValidationErrors.Add(FText::FromString(MissingItemsStr));
        Result = EDataValidationResult::Invalid;
    }

    return Result;
}

void USuspenseInventoryTemplate::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (PropertyChangedEvent.Property)
    {
        const FName PropertyName = PropertyChangedEvent.Property->GetFName();

        // Automatic fixes when properties change
        if (PropertyName == GET_MEMBER_NAME_CHECKED(USuspenseInventoryTemplate, DisplayName))
        {
            // Ensure display name is not empty
            if (DisplayName.IsEmpty())
            {
                DisplayName = FText::FromString(TEXT("Unnamed Template"));
            }
        }
    }
}
#endif
