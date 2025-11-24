// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IMedComPropertyAccessInterface.generated.h"

// Forward declarations
class UClass;
class UObject;

UINTERFACE(MinimalAPI, BlueprintType)
class UMedComPropertyAccessInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for generic property access
 * Allows getting and setting property values by name
 */
class MEDCOMSHARED_API IMedComPropertyAccessInterface
{
    GENERATED_BODY()

public:
    //================================================
    // Numeric Properties
    //================================================
    
    /**
     * Gets numeric property value
     * @param PropertyName Property name
     * @return Property value or 0 if not found
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Properties|Numeric")
    float GetFloatProperty(const FString& PropertyName) const;
    
    /**
     * Sets numeric property value
     * @param PropertyName Property name
     * @param Value New value
     * @return true if property was successfully set
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Properties|Numeric")
    bool SetFloatProperty(const FString& PropertyName, float Value);
    
    /**
     * Gets integer property value
     * @param PropertyName Property name
     * @return Property value or 0 if not found
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Properties|Numeric")
    int32 GetIntProperty(const FString& PropertyName) const;
    
    /**
     * Sets integer property value
     * @param PropertyName Property name
     * @param Value New value
     * @return true if property was successfully set
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Properties|Numeric")
    bool SetIntProperty(const FString& PropertyName, int32 Value);
    
    //================================================
    // String Properties
    //================================================
    
    /**
     * Gets string property value
     * @param PropertyName Property name
     * @return Property value or empty string if not found
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Properties|String")
    FString GetStringProperty(const FString& PropertyName) const;
    
    /**
     * Sets string property value
     * @param PropertyName Property name
     * @param Value New value
     * @return true if property was successfully set
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Properties|String")
    bool SetStringProperty(const FString& PropertyName, const FString& Value);
    
    /**
     * Gets name property value
     * @param PropertyName Property name
     * @return Property value or NAME_None if not found
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Properties|String")
    FName GetNameProperty(const FString& PropertyName) const;
    
    /**
     * Sets name property value
     * @param PropertyName Property name
     * @param Value New value
     * @return true if property was successfully set
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Properties|String")
    bool SetNameProperty(const FString& PropertyName, const FName& Value);
    
    //================================================
    // Object Properties
    //================================================
    
    /**
     * Gets class property value
     * @param PropertyName Property name
     * @return Property value or nullptr if not found
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Properties|Object")
    UClass* GetClassProperty(const FString& PropertyName) const;
    
    /**
     * Sets class property value
     * @param PropertyName Property name
     * @param Value New value
     * @return true if property was successfully set
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Properties|Object")
    bool SetClassProperty(const FString& PropertyName, UClass* Value);
    
    /**
     * Gets object property value
     * @param PropertyName Property name
     * @return Property value or nullptr if not found
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Properties|Object")
    UObject* GetObjectProperty(const FString& PropertyName) const;
    
    /**
     * Sets object property value
     * @param PropertyName Property name
     * @param Value New value
     * @return true if property was successfully set
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Properties|Object")
    bool SetObjectProperty(const FString& PropertyName, UObject* Value);
    
    //================================================
    // Property Validation
    //================================================
    
    /**
     * Checks if property exists
     * @param PropertyName Property name
     * @return true if property exists
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Properties|Validation")
    bool HasProperty(const FString& PropertyName) const;
    
    /**
     * Gets property type
     * @param PropertyName Property name
     * @return Property type name or empty string if not found
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Properties|Validation")
    FString GetPropertyType(const FString& PropertyName) const;
};