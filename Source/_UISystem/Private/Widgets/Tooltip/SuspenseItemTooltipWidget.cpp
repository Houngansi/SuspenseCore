// Copyright Suspense Team Team. All Rights Reserved.

#include "Widgets/Tooltip/SuspenseItemTooltipWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/Engine.h"
#include "Components/Border.h"
#include "TimerManager.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "AttributeSet.h"
#include "Engine/DataTable.h"

USuspenseItemTooltipWidget::USuspenseItemTooltipWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Default configuration - increased offset for better visibility
    MouseOffset = FVector2D(20.0f, 0.0f);
    EdgePadding = 20.0f;
    bAutoAdjustPosition = true;
    HideDelay = 0.0f;
    FadeInDuration = 0.0f;  // Disable fade for now
    FadeOutDuration = 0.0f; // Disable fade for now
    TooltipAnchor = FVector2D(0.0f, 0.0f);
    TooltipPivot = FVector2D(0.0f, 0.0f);
    
    // Set initial state
    bIsFading = false;
    CurrentFadeAlpha = 1.0f;
    TargetFadeAlpha = 1.0f;
    
    // Initially hidden
    SetVisibility(ESlateVisibility::Collapsed);
    
    // Ensure tooltip doesn't block input
    SetIsFocusable(false);
    
    // Initialize pointers
    FullItemData = nullptr;
}

void USuspenseItemTooltipWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Set alignment for proper positioning
    SetAlignmentInViewport(FVector2D(0.0f, 0.0f));
    
    // Ensure full opacity
    SetRenderOpacity(1.0f);
    
    // Disable fade by default
    bIsFading = false;
    CurrentFadeAlpha = 1.0f;
    TargetFadeAlpha = 1.0f;
    
    // Validate widget bindings
    if (!ValidateWidgetBindings())
    {
        UE_LOG(LogTemp, Error, TEXT("[Tooltip] Widget bindings validation failed"));
    }
    
    // Log DataTable status
    if (ItemDataTable)
    {
        UE_LOG(LogTemp, Log, TEXT("[Tooltip] ItemDataTable loaded: %s"), *ItemDataTable->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[Tooltip] No ItemDataTable assigned!"));
    }
}

void USuspenseItemTooltipWidget::NativeDestruct()
{
    // Clear any active timers
    if (GetWorld() && HideTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(HideTimerHandle);
    }
    
    // Clear dynamic content
    ClearAttributeLines();
    
    // Reset pointers
    FullItemData = nullptr;
    
    Super::NativeDestruct();
}

void USuspenseItemTooltipWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    // Handle fade animation if enabled
    if (bIsFading && bEnableFadeAnimation)
    {
        UpdateFade(InDeltaTime);
    }
    
    // Auto-update position if visible
    if (bAutoAdjustPosition && IsTooltipVisible_Implementation())
    {
        RepositionTooltip();
    }
}

void USuspenseItemTooltipWidget::ShowTooltip_Implementation(const FItemUIData& ItemData, const FVector2D& ScreenPosition)
{
    // Cancel any pending hide
    if (GetWorld() && HideTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(HideTimerHandle);
    }
    
    // Validate item data
    if (!ItemData.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Tooltip] ShowTooltip called with invalid item data"));
        return;
    }
    
    // Store data
    CurrentItemData = ItemData;
    CurrentMousePosition = ScreenPosition;
    
    // Load full item data from DataTable
    LoadFullItemData();
    
    // Update display
    UpdateDisplayData();
    
    // Set visibility properties
    SetVisibility(ESlateVisibility::HitTestInvisible);
    SetIsEnabled(true);
    SetIsFocusable(false);
    
    // Set full opacity immediately
    CurrentFadeAlpha = 1.0f;
    TargetFadeAlpha = 1.0f;
    bIsFading = false;
    
    SetRenderOpacity(1.0f);
    SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
    
    // Force layout update
    ForceLayoutPrepass();
    
    // Position tooltip
    if (APlayerController* PC = GetOwningPlayer())
    {
        float MouseX, MouseY;
        if (PC->GetMousePosition(MouseX, MouseY))
        {
            RepositionTooltip();
        }
        else
        {
            CurrentMousePosition = ScreenPosition;
            RepositionTooltip();
        }
    }
    
    // Final opacity check after all operations
    FTimerHandle OpacityCheckTimer;
    GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
    {
        if (GetRenderOpacity() < 1.0f)
        {
            UE_LOG(LogTemp, Error, TEXT("[Tooltip] Opacity was reset! Forcing back to 1.0"));
            SetRenderOpacity(1.0f);
        }
    });
    
    UE_LOG(LogTemp, Log, TEXT("[Tooltip] Showing tooltip with opacity %.2f for item: %s"), 
        GetRenderOpacity(), *CurrentItemData.DisplayName.ToString());
}

void USuspenseItemTooltipWidget::HideTooltip_Implementation()
{
    // Cancel any timers
    if (GetWorld() && HideTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(HideTimerHandle);
    }
    
    // Hide immediately without fade
    SetVisibility(ESlateVisibility::Collapsed);
    CurrentItemData = FItemUIData();
    CurrentFadeAlpha = 0.0f;
    bIsFading = false;
    
    // Clear dynamic content
    ClearAttributeLines();
    
    // Reset full item data pointer
    FullItemData = nullptr;
    
    UE_LOG(LogTemp, Verbose, TEXT("[Tooltip] Hiding tooltip"));
}

void USuspenseItemTooltipWidget::UpdateTooltipPosition_Implementation(const FVector2D& ScreenPosition)
{
    CurrentMousePosition = ScreenPosition;
    
    if (IsTooltipVisible_Implementation())
    {
        RepositionTooltip();
    }
}

bool USuspenseItemTooltipWidget::IsTooltipVisible_Implementation() const
{
    return GetVisibility() != ESlateVisibility::Collapsed && 
           GetVisibility() != ESlateVisibility::Hidden;
}

void USuspenseItemTooltipWidget::SetTooltipAnchor_Implementation(const FVector2D& Anchor, const FVector2D& Pivot)
{
    TooltipAnchor = Anchor;
    TooltipPivot = Pivot;
    
    SetAlignmentInViewport(TooltipPivot);
}

void USuspenseItemTooltipWidget::LoadFullItemData()
{
    FullItemData = nullptr;
    
    if (!ItemDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Tooltip] No ItemDataTable set!"));
        return;
    }
    
    // Find row in DataTable using the item ID
    FSuspenseUnifiedItemData* FoundData = ItemDataTable->FindRow<FSuspenseUnifiedItemData>(
        CurrentItemData.ItemID, 
        TEXT("LoadFullItemData")
    );
    
    if (FoundData)
    {
        FullItemData = FoundData;
        UE_LOG(LogTemp, Log, TEXT("[Tooltip] Loaded full data for item: %s"), 
            *CurrentItemData.ItemID.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[Tooltip] Failed to find item %s in DataTable"), 
            *CurrentItemData.ItemID.ToString());
    }
}

void USuspenseItemTooltipWidget::UpdateDisplayData()
{
    if (!ValidateWidgetBindings())
    {
        return;
    }
    
    // Clear previous attributes
    ClearAttributeLines();
    
    // Item name - prioritize display name
    if (!CurrentItemData.DisplayName.IsEmpty())
    {
        ItemNameText->SetText(CurrentItemData.DisplayName);
    }
    else
    {
        ItemNameText->SetText(FText::FromName(CurrentItemData.ItemID));
    }
    
    // Description
    ItemDescription->SetText(CurrentItemData.Description);
    
    // Icon
    if (UTexture2D* IconTexture = CurrentItemData.GetIcon())
    {
        ItemIcon->SetBrushFromTexture(IconTexture);
        ItemIcon->SetVisibility(ESlateVisibility::Visible);
    }
    else
    {
        ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
    }
    
    // Item type
    ItemTypeText->SetText(GameplayTagToText(CurrentItemData.ItemType));
    
    // Grid size
    GridSizeText->SetText(FormatGridSize(CurrentItemData.GridSize));
    
    // Total weight calculation
    float TotalWeight = CurrentItemData.Weight * CurrentItemData.Quantity;
    WeightText->SetText(FormatWeight(TotalWeight));
    
    // Stack info
    if (CurrentItemData.MaxStackSize > 1)
    {
        StackInfoText->SetText(FText::Format(
            NSLOCTEXT("Tooltip", "StackInfo", "{0} / {1}"),
            FText::AsNumber(CurrentItemData.Quantity),
            FText::AsNumber(CurrentItemData.MaxStackSize)
        ));
        StackInfoText->SetVisibility(ESlateVisibility::Visible);
    }
    else
    {
        StackInfoText->SetVisibility(ESlateVisibility::Collapsed);
    }
    
    // Equipment info
    if (CurrentItemData.bIsEquippable && CurrentItemData.EquipmentSlotType.IsValid())
    {
        EquipmentSlotText->SetText(GameplayTagToText(CurrentItemData.EquipmentSlotType));
        EquipmentSlotText->SetVisibility(ESlateVisibility::Visible);
    }
    else
    {
        EquipmentSlotText->SetVisibility(ESlateVisibility::Collapsed);
    }
    
    // Ammo info
    if (CurrentItemData.bHasAmmo && !CurrentItemData.AmmoText.IsEmpty())
    {
        AmmoInfoText->SetText(CurrentItemData.AmmoText);
        AmmoInfoText->SetVisibility(ESlateVisibility::Visible);
    }
    else
    {
        AmmoInfoText->SetVisibility(ESlateVisibility::Collapsed);
    }
    
    // Display attributes if we have full data from DataTable
    if (FullItemData && AttributesContainer && AttributesSeparator)
    {
        AttributesSeparator->SetVisibility(ESlateVisibility::Visible);
        
        // Display attributes based on item type
        if (FullItemData->bIsWeapon)
        {
            DisplayWeaponAttributes();
        }
        else if (FullItemData->bIsArmor)
        {
            DisplayArmorAttributes();
        }
        else if (FullItemData->bIsAmmo)
        {
            DisplayAmmoAttributes();
        }
    }
    else
    {
        if (AttributesSeparator)
        {
            AttributesSeparator->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

void USuspenseItemTooltipWidget::DisplayWeaponAttributes()
{
    // Validate that we have weapon data
    if (!FullItemData || !FullItemData->bIsWeapon)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[Tooltip] DisplayWeaponAttributes: Not a weapon or no full data"));
        return;
    }
    
    // Add section header
    AddAttributeLine(TEXT("--- WEAPON STATS ---"), 0.0f, TEXT(""));
    
    // Validate weapon initialization data
    if (!FullItemData->WeaponInitialization.WeaponAttributeSetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Tooltip] No WeaponAttributeSetClass configured for %s"), 
            *CurrentItemData.ItemID.ToString());
        AddAttributeLine(TEXT("No attribute data available"), 0.0f, TEXT(""));
        return;
    }
    
    if (!FullItemData->WeaponInitialization.WeaponInitEffect)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Tooltip] No WeaponInitEffect configured for %s"), 
            *CurrentItemData.ItemID.ToString());
        AddAttributeLine(TEXT("Weapon initialization not configured"), 0.0f, TEXT(""));
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("[Tooltip] Extracting weapon attributes for %s"), 
        *CurrentItemData.ItemID.ToString());
    
    // Store references to avoid repeated property access
    TSubclassOf<UAttributeSet> AttrSetClass = FullItemData->WeaponInitialization.WeaponAttributeSetClass;
    TSubclassOf<UGameplayEffect> InitEffect = FullItemData->WeaponInitialization.WeaponInitEffect;
    
    // Extract and display common weapon attributes
    // These names should match your WeaponAttributeSet property names
    
    float BaseDamage = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("BaseDamage"));
    if (BaseDamage > 0.0f)
    {
        AddAttributeLine(TEXT("Damage"), BaseDamage, TEXT("%.0f"));
    }
    
    float RateOfFire = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("RateOfFire"));
    if (RateOfFire > 0.0f)
    {
        AddAttributeLine(TEXT("Rate of Fire"), RateOfFire, TEXT("%.0f RPM"));
    }
    
    float MagazineSize = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("MagazineSize"));
    if (MagazineSize > 0.0f)
    {
        AddAttributeLine(TEXT("Magazine"), MagazineSize, TEXT("%.0f"));
    }
    
    float ReloadTime = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("ReloadTime"));
    if (ReloadTime > 0.0f)
    {
        AddAttributeLine(TEXT("Reload Time"), ReloadTime, TEXT("%.1fs"));
    }
    
    float BaseSpread = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("BaseSpread"));
    if (BaseSpread > 0.0f)
    {
        AddAttributeLine(TEXT("Hip Fire Spread"), BaseSpread, TEXT("%.2f°"));
    }
    
    float AimSpread = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("AimSpread"));
    if (AimSpread > 0.0f)
    {
        AddAttributeLine(TEXT("ADS Spread"), AimSpread, TEXT("%.2f°"));
    }
    
    float Recoil = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("Recoil"));
    if (Recoil > 0.0f)
    {
        AddAttributeLine(TEXT("Recoil"), Recoil, TEXT("%.1f"));
    }
    
    float Range = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("EffectiveRange"));
    if (Range > 0.0f)
    {
        AddAttributeLine(TEXT("Effective Range"), Range, TEXT("%.0fm"));
    }
    
    float HeadshotMultiplier = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("HeadshotMultiplier"));
    if (HeadshotMultiplier > 1.0f)
    {
        AddAttributeLine(TEXT("Headshot Multiplier"), HeadshotMultiplier, TEXT("%.1fx"));
    }
    
    float ArmorPenetration = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("ArmorPenetration"));
    if (ArmorPenetration > 0.0f)
    {
        AddAttributeLine(TEXT("Armor Penetration"), ArmorPenetration * 100.0f, TEXT("%.0f%%"));
    }
    
    // Display fire modes if available
    if (FullItemData->FireModes.Num() > 0)
    {
        FString FireModesStr = TEXT("Fire Modes: ");
        for (int32 i = 0; i < FullItemData->FireModes.Num(); ++i)
        {
            if (i > 0)
            {
                FireModesStr += TEXT(", ");
            }
            FireModesStr += FullItemData->FireModes[i].DisplayName.ToString();
        }
        AddAttributeLine(FireModesStr, 0.0f, TEXT(""));
    }
    
    // Display compatible ammo type
    if (FullItemData->AmmoType.IsValid())
    {
        FString AmmoStr = TEXT("Ammo Type: ") + GameplayTagToText(FullItemData->AmmoType).ToString();
        AddAttributeLine(AmmoStr, 0.0f, TEXT(""));
    }
}

void USuspenseItemTooltipWidget::DisplayArmorAttributes()
{
    if (!FullItemData || !FullItemData->bIsArmor)
        return;
    
    // Add section header
    AddAttributeLine(TEXT("--- ARMOR STATS ---"), 0.0f, TEXT(""));
    
    // Extract attributes from ArmorAttributeSet
    if (FullItemData->ArmorInitialization.ArmorAttributeSetClass)
    {
        TSubclassOf<UAttributeSet> AttrSetClass = FullItemData->ArmorInitialization.ArmorAttributeSetClass;
        TSubclassOf<UGameplayEffect> InitEffect = FullItemData->ArmorInitialization.ArmorInitEffect;
        
        // Common armor attributes
        float ArmorValue = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("ArmorValue"));
        if (ArmorValue > 0)
            AddAttributeLine(TEXT("Armor"), ArmorValue, TEXT("%.0f"));
        
        float DamageReduction = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("DamageReduction"));
        if (DamageReduction > 0)
            AddAttributeLine(TEXT("Damage Reduction"), DamageReduction * 100.0f, TEXT("%.0f%%"));
        
        float Durability = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("Durability"));
        float MaxDurability = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("MaxDurability"));
        if (MaxDurability > 0)
        {
            FString DurabilityStr = FString::Printf(TEXT("%.0f / %.0f"), Durability, MaxDurability);
            AddAttributeLine(TEXT("Durability"), 0.0f, DurabilityStr);
        }
        
        float MovementSpeedPenalty = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("MovementSpeedPenalty"));
        if (MovementSpeedPenalty != 0)
            AddAttributeLine(TEXT("Movement Speed"), -MovementSpeedPenalty * 100.0f, TEXT("%+.0f%%"));
        
        float StaminaPenalty = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("StaminaPenalty"));
        if (StaminaPenalty != 0)
            AddAttributeLine(TEXT("Stamina Penalty"), -StaminaPenalty * 100.0f, TEXT("%+.0f%%"));
        
        float NoiseReduction = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("NoiseReduction"));
        if (NoiseReduction > 0)
            AddAttributeLine(TEXT("Noise Reduction"), NoiseReduction * 100.0f, TEXT("%.0f%%"));
    }
    
    // Armor type
    if (FullItemData->ArmorType.IsValid())
    {
        FString ArmorTypeStr = TEXT("Type: ") + GameplayTagToText(FullItemData->ArmorType).ToString();
        AddAttributeLine(ArmorTypeStr, 0.0f, TEXT(""));
    }
}

void USuspenseItemTooltipWidget::DisplayAmmoAttributes()
{
    if (!FullItemData || !FullItemData->bIsAmmo)
        return;
    
    // Add section header
    AddAttributeLine(TEXT("--- AMMO STATS ---"), 0.0f, TEXT(""));
    
    // Extract attributes from AmmoAttributeSet
    if (FullItemData->AmmoInitialization.AmmoAttributeSetClass)
    {
        TSubclassOf<UAttributeSet> AttrSetClass = FullItemData->AmmoInitialization.AmmoAttributeSetClass;
        TSubclassOf<UGameplayEffect> InitEffect = FullItemData->AmmoInitialization.AmmoInitEffect;
        
        // Common ammo attributes
        float Damage = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("Damage"));
        if (Damage > 0)
            AddAttributeLine(TEXT("Damage"), Damage, TEXT("%.0f"));
        
        float ArmorPenetration = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("ArmorPenetration"));
        if (ArmorPenetration > 0)
            AddAttributeLine(TEXT("Armor Penetration"), ArmorPenetration, TEXT("%.0f"));
        
        float Velocity = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("Velocity"));
        if (Velocity > 0)
            AddAttributeLine(TEXT("Velocity"), Velocity, TEXT("%.0f m/s"));
        
        float RicochetChance = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("RicochetChance"));
        if (RicochetChance > 0)
            AddAttributeLine(TEXT("Ricochet Chance"), RicochetChance * 100.0f, TEXT("%.0f%%"));
        
        float FragmentationChance = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("FragmentationChance"));
        if (FragmentationChance > 0)
            AddAttributeLine(TEXT("Fragmentation"), FragmentationChance * 100.0f, TEXT("%.0f%%"));
        
        float TracerBrightness = ExtractAttributeValue(AttrSetClass, InitEffect, TEXT("TracerBrightness"));
        if (TracerBrightness > 0)
            AddAttributeLine(TEXT("Tracer"), 0.0f, TEXT("Yes"));
    }
    
    // Caliber
    if (FullItemData->AmmoCaliber.IsValid())
    {
        FString CaliberStr = TEXT("Caliber: ") + GameplayTagToText(FullItemData->AmmoCaliber).ToString();
        AddAttributeLine(CaliberStr, 0.0f, TEXT(""));
    }
    
    // Quality
    if (FullItemData->AmmoQuality.IsValid())
    {
        FString QualityStr = TEXT("Quality: ") + GameplayTagToText(FullItemData->AmmoQuality).ToString();
        AddAttributeLine(QualityStr, 0.0f, TEXT(""));
    }
    
    // Compatible weapons
    if (FullItemData->CompatibleWeapons.Num() > 0)
    {
        AddAttributeLine(TEXT("Compatible with:"), 0.0f, TEXT(""));
        for (const FGameplayTag& WeaponTag : FullItemData->CompatibleWeapons)
        {
            FString WeaponStr = TEXT("  • ") + GameplayTagToText(WeaponTag).ToString();
            AddAttributeLine(WeaponStr, 0.0f, TEXT(""));
        }
    }
}

float USuspenseItemTooltipWidget::ExtractAttributeValue(
    TSubclassOf<UAttributeSet> AttributeSetClass,
    TSubclassOf<UGameplayEffect> InitEffect,
    const FString& AttributeName)
{
    // Validate inputs first to avoid unnecessary work
    if (!AttributeSetClass || !InitEffect)
    {
        return 0.0f;
    }
    
    // Get the Class Default Object of the GameplayEffect
    // CDO contains all the design-time configuration without needing instantiation
    const UGameplayEffect* EffectCDO = InitEffect->GetDefaultObject<UGameplayEffect>();
    if (!EffectCDO)
    {
        return 0.0f;
    }
    
    // Search through all modifiers in the effect definition
    // Each modifier represents a change to an attribute
    for (const FGameplayModifierInfo& Modifier : EffectCDO->Modifiers)
    {
        // Get the attribute this modifier affects
        const FGameplayAttribute& ModifiedAttribute = Modifier.Attribute;
        if (!ModifiedAttribute.IsValid())
        {
            continue;
        }
        
        // Extract the attribute name for comparison
        // GetName() returns something like "WeaponAttributeSet.BaseDamage"
        FString AttributePropertyName = ModifiedAttribute.GetName();
        
        // Check if this attribute matches what we're looking for
        // We use Contains because the full name includes the AttributeSet class prefix
        if (AttributePropertyName.Contains(AttributeName, ESearchCase::IgnoreCase))
        {
            // Found the attribute! Now extract its magnitude safely
            const FGameplayEffectModifierMagnitude& Magnitude = Modifier.ModifierMagnitude;
            
            // КЛЮЧЕВОЕ ИСПРАВЛЕНИЕ: Используем публичный API GetStaticMagnitudeIfPossible
            // Этот метод безопасно извлекает величину для типов не требующих контекста
            // Параметр InLevel мы передаем как 1.0 поскольку в tooltip нет информации об уровне
            float ExtractedValue = 0.0f;
            const bool bSuccess = Magnitude.GetStaticMagnitudeIfPossible(1.0f, ExtractedValue);
            
            if (bSuccess)
            {
                // Успешно извлекли статическое значение
                UE_LOG(LogTemp, Verbose, TEXT("[Tooltip] Found static magnitude for %s: %.2f"), 
                    *AttributeName, ExtractedValue);
                return ExtractedValue;
            }
            else
            {
                // Величина требует рантайм контекст (attribute-based, custom, или set-by-caller)
                // Для tooltip это невозможно вычислить, поэтому пробуем получить базовое значение
                UE_LOG(LogTemp, Verbose, 
                    TEXT("[Tooltip] Attribute %s requires runtime context, trying base value..."), 
                    *AttributeName);
                
                // Попробуем извлечь базовое значение из самого AttributeSet
                // Это значение по умолчанию которое установлено в конструкторе AttributeSet
                break; // Выходим из цикла чтобы попробовать альтернативный путь ниже
            }
        }
    }
    
    // Если не смогли получить значение из GameplayEffect модификаторов,
    // попробуем получить базовое значение напрямую из AttributeSet Class Default Object
    const UAttributeSet* AttributeSetCDO = AttributeSetClass->GetDefaultObject<UAttributeSet>();
    if (AttributeSetCDO)
    {
        // Используем рефлексию Unreal для поиска свойства по имени
        for (TFieldIterator<FProperty> PropIt(AttributeSetClass); PropIt; ++PropIt)
        {
            FProperty* Property = *PropIt;
            
            // Проверяем что это структурное свойство типа FGameplayAttributeData
            if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
            {
                // Проверяем что структура называется GameplayAttributeData
                if (StructProp->Struct && StructProp->Struct->GetFName() == FName("GameplayAttributeData"))
                {
                    // Получаем C++ имя свойства
                    FString PropName = Property->GetNameCPP();
                    
                    // Проверяем соответствие имени
                    if (PropName.Contains(AttributeName, ESearchCase::IgnoreCase))
                    {
                        // Получаем указатель на данные атрибута в CDO
                        const FGameplayAttributeData* AttrData = 
                            StructProp->ContainerPtrToValuePtr<FGameplayAttributeData>(AttributeSetCDO);
                        
                        if (AttrData)
                        {
                            // Извлекаем базовое значение атрибута
                            // Это значение установленное в конструкторе AttributeSet
                            float BaseValue = AttrData->GetBaseValue();
                            
                            UE_LOG(LogTemp, Verbose, TEXT("[Tooltip] Found base value for %s: %.2f"), 
                                *AttributeName, BaseValue);
                            
                            return BaseValue;
                        }
                    }
                }
            }
        }
    }
    
    // Атрибут не найден ни в эффекте ни в AttributeSet
    UE_LOG(LogTemp, Verbose, TEXT("[Tooltip] Attribute %s not found anywhere"), *AttributeName);
    return 0.0f;
}

void USuspenseItemTooltipWidget::AddAttributeLine(const FString& AttributeName, float Value, const FString& Format)
{
    if (!AttributesContainer)
        return;
    
    // Create new text block for the attribute line
    UTextBlock* NewLine = NewObject<UTextBlock>(this);
    NewLine->SetText(FText::GetEmpty());
    
    // Format the text based on input parameters
    FString FormattedText;
    if (Format.IsEmpty() || Format == TEXT(""))
    {
        // Just display the attribute name (for headers/text-only lines)
        FormattedText = AttributeName;
    }
    else if (Format.Contains(TEXT("%")))
    {
        // ИСПРАВЛЕНИЕ: Используем конкретные форматы вместо динамической строки
        if (Format == TEXT("%.0f"))
        {
            FormattedText = FString::Printf(TEXT("%s: %.0f"), *AttributeName, Value);
        }
        else if (Format == TEXT("%.1f"))
        {
            FormattedText = FString::Printf(TEXT("%s: %.1f"), *AttributeName, Value);
        }
        else if (Format == TEXT("%.2f"))
        {
            FormattedText = FString::Printf(TEXT("%s: %.2f"), *AttributeName, Value);
        }
        else if (Format == TEXT("%.0f RPM"))
        {
            FormattedText = FString::Printf(TEXT("%s: %.0f RPM"), *AttributeName, Value);
        }
        else if (Format == TEXT("%.1fs"))
        {
            FormattedText = FString::Printf(TEXT("%s: %.1fs"), *AttributeName, Value);
        }
        else if (Format == TEXT("%.2f°"))
        {
            FormattedText = FString::Printf(TEXT("%s: %.2f°"), *AttributeName, Value);
        }
        else if (Format == TEXT("%.0fm"))
        {
            FormattedText = FString::Printf(TEXT("%s: %.0fm"), *AttributeName, Value);
        }
        else if (Format == TEXT("%.1fx"))
        {
            FormattedText = FString::Printf(TEXT("%s: %.1fx"), *AttributeName, Value);
        }
        else if (Format == TEXT("%.0f%%"))
        {
            FormattedText = FString::Printf(TEXT("%s: %.0f%%"), *AttributeName, Value);
        }
        else if (Format == TEXT("%+.0f%%"))
        {
            FormattedText = FString::Printf(TEXT("%s: %+.0f%%"), *AttributeName, Value);
        }
        else if (Format == TEXT("%.0f m/s"))
        {
            FormattedText = FString::Printf(TEXT("%s: %.0f m/s"), *AttributeName, Value);
        }
        else
        {
            // Fallback для неизвестных форматов
            FormattedText = FString::Printf(TEXT("%s: %.1f"), *AttributeName, Value);
        }
    }
    else
    {
        // Format is actually the full value string
        FormattedText = FString::Printf(TEXT("%s: %s"), *AttributeName, *Format);
    }
    
    NewLine->SetText(FText::FromString(FormattedText));
    
    // Style the text - smaller font and slightly dimmed color
    FSlateFontInfo FontInfo = NewLine->GetFont();
    FontInfo.Size = 10;
    NewLine->SetFont(FontInfo);
    
    // Different colors for headers vs attributes
    if (AttributeName.Contains(TEXT("---")))
    {
        // Header style
        NewLine->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.8f, 0.4f, 1.0f)));
    }
    else
    {
        // Regular attribute style
        NewLine->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f)));
    }
    
    // Add to container with proper spacing
    UVerticalBoxSlot* NewSlot = AttributesContainer->AddChildToVerticalBox(NewLine);
    if (NewSlot)
    {
        NewSlot->SetPadding(FMargin(4.0f, 1.0f));
    }
}

void USuspenseItemTooltipWidget::ClearAttributeLines()
{
    if (!AttributesContainer)
        return;
    
    // Clear all dynamically added children
    AttributesContainer->ClearChildren();
}

void USuspenseItemTooltipWidget::RepositionTooltip()
{
    APlayerController* PC = GetOwningPlayer();
    if (!PC)
    {
        return;
    }

    // Get actual mouse position in viewport space
    float MouseX, MouseY;
    if (!PC->GetMousePosition(MouseX, MouseY))
    {
        return;
    }

    // Get viewport size and scale
    FVector2D ViewportSize = FVector2D::ZeroVector;
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->GetViewportSize(ViewportSize);
    }
    
    if (ViewportSize.IsZero())
    {
        return;
    }

    // Get DPI scale
    float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
    
    // Convert mouse position to slate units (considering DPI)
    FVector2D MousePosition = FVector2D(MouseX, MouseY) / ViewportScale;
    
    // Get tooltip size
    FVector2D TooltipSize = GetDesiredSize();
    
    // If size is not ready, try cached geometry
    if (TooltipSize.IsZero())
    {
        const FGeometry& CachedGeometry = GetCachedGeometry();
        if (CachedGeometry.GetLocalSize().X > 0 && CachedGeometry.GetLocalSize().Y > 0)
        {
            TooltipSize = CachedGeometry.GetLocalSize();
        }
        else
        {
            // Use reasonable default size
            TooltipSize = FVector2D(300.0f, 200.0f);
        }
    }

    // Convert viewport size to slate units
    FVector2D ViewportSizeInSlateUnits = ViewportSize / ViewportScale;

    // Calculate tooltip position relative to mouse
    FVector2D TooltipPosition = MousePosition;

    // Position tooltip below cursor with small offset
    const float VerticalOffset = 20.0f;
    
    // Determine if tooltip should be on the right or left of cursor
    bool bShowOnRight = true;
    
    // Check if there's enough space on the right
    if (MousePosition.X + MouseOffset.X + TooltipSize.X > ViewportSizeInSlateUnits.X - EdgePadding)
    {
        bShowOnRight = false;
    }
    
    // Position horizontally
    if (bShowOnRight)
    {
        // Show on the right of cursor
        TooltipPosition.X = MousePosition.X + MouseOffset.X;
    }
    else
    {
        // Show on the left of cursor
        TooltipPosition.X = MousePosition.X - MouseOffset.X - TooltipSize.X;
    }
    
    // Position vertically - tooltip below cursor
    TooltipPosition.Y = MousePosition.Y + VerticalOffset;
    
    // Check if tooltip goes beyond bottom edge
    if (TooltipPosition.Y + TooltipSize.Y > ViewportSizeInSlateUnits.Y - EdgePadding)
    {
        // Show above cursor instead
        TooltipPosition.Y = MousePosition.Y - VerticalOffset - TooltipSize.Y;
    }
    
    // Final bounds check
    TooltipPosition.X = FMath::Clamp(
        TooltipPosition.X, 
        EdgePadding, 
        ViewportSizeInSlateUnits.X - TooltipSize.X - EdgePadding
    );
    
    TooltipPosition.Y = FMath::Clamp(
        TooltipPosition.Y, 
        EdgePadding, 
        ViewportSizeInSlateUnits.Y - TooltipSize.Y - EdgePadding
    );

    // Apply position
    SetPositionInViewport(TooltipPosition, false);

    UE_LOG(LogTemp, VeryVerbose, TEXT("[Tooltip] Repositioned - Mouse: (%.1f, %.1f), Tooltip: (%.1f, %.1f), Size: (%.1f, %.1f)"),
        MousePosition.X, MousePosition.Y, 
        TooltipPosition.X, TooltipPosition.Y, 
        TooltipSize.X, TooltipSize.Y);
}

FVector2D USuspenseItemTooltipWidget::GetViewportSize() const
{
    FVector2D Result = FVector2D::ZeroVector;
    
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->GetViewportSize(Result);
    }
    
    return Result;
}

FText USuspenseItemTooltipWidget::GameplayTagToText(const FGameplayTag& Tag) const
{
    if (!Tag.IsValid())
    {
        return FText::GetEmpty();
    }
    
    // Get last part of tag for display
    FString TagString = Tag.ToString();
    FString DisplayName;
    
    if (TagString.Split(TEXT("."), nullptr, &DisplayName, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
    {
        // Convert CamelCase to readable text
        FString Result;
        for (int32 i = 0; i < DisplayName.Len(); ++i)
        {
            if (i > 0 && FChar::IsUpper(DisplayName[i]))
            {
                Result += TEXT(" ");
            }
            Result += DisplayName[i];
        }
        
        return FText::FromString(Result);
    }
    
    return FText::FromString(TagString);
}

FText USuspenseItemTooltipWidget::FormatWeight(float Weight) const
{
    return FText::Format(
        NSLOCTEXT("Tooltip", "Weight", "{0} kg"),
        FText::AsNumber(Weight, &FNumberFormattingOptions().SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(1))
    );
}

FText USuspenseItemTooltipWidget::FormatGridSize(const FIntPoint& GridSize) const
{
    return FText::Format(
        NSLOCTEXT("Tooltip", "GridSize", "{0}x{1}"),
        FText::AsNumber(GridSize.X),
        FText::AsNumber(GridSize.Y)
    );
}

void USuspenseItemTooltipWidget::UpdateFade(float DeltaTime)
{
    // Currently disabled - keeping full opacity
    CurrentFadeAlpha = 1.0f;
    TargetFadeAlpha = 1.0f;
    bIsFading = false;
    SetRenderOpacity(1.0f);
}

void USuspenseItemTooltipWidget::StartFadeIn()
{
    bIsFading = true;
    TargetFadeAlpha = 1.0f;
    
    // Start from 0 if not already fading
    if (!bIsFading || CurrentFadeAlpha < 0.1f)
    {
        CurrentFadeAlpha = 0.0f;
    }
    
    // If fade is disabled, immediately set to full opacity
    if (FadeInDuration <= 0.0f)
    {
        CurrentFadeAlpha = 1.0f;
        TargetFadeAlpha = 1.0f;
        bIsFading = false;
        ApplyFade(1.0f);
    }
}

void USuspenseItemTooltipWidget::StartFadeOut()
{
    bIsFading = true;
    TargetFadeAlpha = 0.0f;
}

void USuspenseItemTooltipWidget::ApplyFade(float Alpha)
{
    // Always set full opacity for now
    SetRenderOpacity(1.0f);
    CurrentFadeAlpha = 1.0f;
    TargetFadeAlpha = 1.0f;
    bIsFading = false;
}

bool USuspenseItemTooltipWidget::ValidateWidgetBindings() const
{
    bool bAllValid = true;

    if (!ItemNameText)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Tooltip] ItemNameText not bound"));
        bAllValid = false;
    }

    if (!ItemDescription)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Tooltip] ItemDescription not bound"));
        bAllValid = false;
    }

    if (!ItemIcon)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Tooltip] ItemIcon not bound"));
        bAllValid = false;
    }

    if (!ItemTypeText)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Tooltip] ItemTypeText not bound"));
        bAllValid = false;
    }

    if (!GridSizeText)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Tooltip] GridSizeText not bound"));
        bAllValid = false;
    }

    if (!WeightText)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Tooltip] WeightText not bound"));
        bAllValid = false;
    }

    if (!StackInfoText)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Tooltip] StackInfoText not bound"));
        bAllValid = false;
    }

    if (!EquipmentSlotText)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Tooltip] EquipmentSlotText not bound"));
        bAllValid = false;
    }

    if (!AmmoInfoText)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Tooltip] AmmoInfoText not bound"));
        bAllValid = false;
    }

    // Check new widgets for attributes
    if (!AttributesContainer)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Tooltip] AttributesContainer not bound - attributes won't display"));
    }

    if (!AttributesSeparator)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Tooltip] AttributesSeparator not bound"));
    }

    return bAllValid;
}