#include "Core/Enemy/FSM/MedComEnemyState.h"
#include "Core/Enemy/FSM/MedComEnemyFSMComponent.h"
#include "Core/Enemy/MedComEnemyCharacter.h"
#include "AIController.h"
#include "Perception/PawnSensingComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"

// Определение категории лога
DEFINE_LOG_CATEGORY_STATIC(LogMedComEnemyState, Log, All);

UMedComEnemyState::UMedComEnemyState() : FSMComponent(nullptr)
{
    // Дефолтный тег
    StateTag = FGameplayTag::RequestGameplayTag("State.Base");
}

void UMedComEnemyState::InitializeState(const TMap<FName, float>& InStateParams)
{
    // Сохраняем параметры состояния (обратная совместимость)
    StateParams = InStateParams;
    
    // Преобразуем float-параметры в расширенный формат
    ExtendedParams.Empty();
    for (const auto& Param : InStateParams)
    {
        ExtendedParams.Add(Param.Key, FStateParamValue(Param.Value));
    }
}

void UMedComEnemyState::InitializeWithParams(const TMap<FName, FStateParamValue>& InExtendedParams)
{
    // Напрямую устанавливаем расширенные параметры
    ExtendedParams = InExtendedParams;
    
    // Обновляем традиционные float-параметры для обратной совместимости
    StateParams.Empty();
    for (const auto& Param : InExtendedParams)
    {
        if (Param.Value.Type == FStateParamValue::EParamType::Float)
        {
            StateParams.Add(Param.Key, Param.Value.FloatValue);
        }
    }
}

void UMedComEnemyState::OnEnter(AMedComEnemyCharacter* Owner)
{
    // Базовая реализация - просто логируем вход
    LogStateMessage(Owner, TEXT("Entered state"));
    
    // Устанавливаем соответствующий тег Gameplay для состояния
    if (Owner && StateTag.IsValid())
    {
        Owner->AddGameplayTag(StateTag);
    }
    
    // Проверяем, нужна ли интеграция с GAS
    bool bUseGASForMovement = GetStateParamBool(TEXT("UseGASForMovement"), false);
    if (bUseGASForMovement && Owner)
    {
        // Ищем имя класса способности в параметрах
        FString AbilityClassName = GetStateParamString(TEXT("AbilityClass"));
        if (!AbilityClassName.IsEmpty())
        {
            TryActivateAbility(Owner, AbilityClassName);
        }
    }
}

void UMedComEnemyState::OnExit(AMedComEnemyCharacter* Owner)
{
    // Базовая реализация - просто логируем выход
    LogStateMessage(Owner, TEXT("Exited state"));
    
    // Удаляем тег состояния
    if (Owner && StateTag.IsValid())
    {
        Owner->RemoveGameplayTag(StateTag);
    }
    
    // Останавливаем любые таймеры этого состояния
    if (FSMComponent)
    {
        for (const auto& Param : StateParams)
        {
            if (Param.Key.ToString().Contains(TEXT("Timer")))
            {
                FSMComponent->StopStateTimer(Param.Key);
            }
        }
    }
    
    // Деактивируем GAS способность, если она была активирована
    if (Owner)
    {
        FString AbilityClassName = GetStateParamString(TEXT("AbilityClass"));
        if (!AbilityClassName.IsEmpty())
        {
            TryDeactivateAbility(Owner, AbilityClassName);
        }
    }
}

void UMedComEnemyState::OnEvent(AMedComEnemyCharacter* Owner, EEnemyEvent Event, AActor* EventInstigator)
{
    // Базовая реализация - по умолчанию ничего не делаем
    // Производные классы могут переопределить для обработки событий
}

void UMedComEnemyState::OnTimerFired(AMedComEnemyCharacter* Owner, FName TimerName)
{
    // Базовая реализация - по умолчанию ничего не делаем
    // Производные классы могут переопределить для обработки таймеров
}

void UMedComEnemyState::ProcessTick(AMedComEnemyCharacter* Owner, float DeltaTime)
{
    // Пусто по умолчанию
}

void UMedComEnemyState::StartStateTimer(AMedComEnemyCharacter* Owner, FName TimerName, float Duration, bool bLoop)
{
    // Проверки безопасности
    if (!IsValid(this) || !FSMComponent || !IsValid(FSMComponent) || !FSMComponent->GetWorld() || !IsValid(Owner))
    {
        return;
    }
    
    // Создаем безопасный делегат с использованием слабых указателей
    TWeakObjectPtr<UMedComEnemyFSMComponent> WeakFSM(FSMComponent);
    FName TimerNameCopy = TimerName;
    
    FTimerDelegate TimerDelegate;
    // Опция 1: Если доступно использование лямбда с захватом WeakFSM
    TimerDelegate.BindLambda([WeakFSM, TimerNameCopy]() {
        if (WeakFSM.IsValid())
        {
            WeakFSM->OnStateTimerFired(TimerNameCopy);
        }
    });
    
    // Опция 2: Стандартный подход с UObject
    // TimerDelegate.BindUObject(FSMComponent, &UMedComEnemyFSMComponent::OnStateTimerFired, TimerName);
    
    FTimerHandle NewTimerHandle;
    FSMComponent->GetWorld()->GetTimerManager().SetTimer(
        NewTimerHandle, TimerDelegate, Duration, bLoop);
        
    // Безопасно добавляем таймер
    FSMComponent->AddStateTimer(TimerName, NewTimerHandle);
}

void UMedComEnemyState::StopStateTimer(AMedComEnemyCharacter* Owner, FName TimerName)
{
    if (FSMComponent)
    {
        FSMComponent->StopStateTimer(TimerName);
    }
}

bool UMedComEnemyState::CanSeeTarget(AMedComEnemyCharacter* Owner, AActor* Target) const
{
    // Проверка видимости цели с помощью трассировки
    if (!Owner || !Target || !Owner->GetWorld())
    {
        return false;
    }
    
    FVector StartLocation = Owner->GetActorLocation() + FVector(0, 0, 50.0f); // Eye level
    FVector EndLocation = Target->GetActorLocation() + FVector(0, 0, 50.0f);
    
    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Owner);
    
    // Используем ECC_Camera вместо ECC_Visibility для более точной проверки
    bool bHit = Owner->GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, 
                                                          ECC_Camera, QueryParams);
    
    return (!bHit || HitResult.GetActor() == Target);
}

float UMedComEnemyState::GetDistanceToTarget(AMedComEnemyCharacter* Owner, AActor* Target) const
{
    if (!Owner || !Target)
    {
        return -1.0f;
    }
    
    return FVector::Distance(Owner->GetActorLocation(), Target->GetActorLocation());
}

float UMedComEnemyState::GetStateParamFloat(const FName& ParamName, float DefaultValue) const
{
    // Сначала ищем в расширенных параметрах
    const FStateParamValue* FoundExtValue = ExtendedParams.Find(ParamName);
    if (FoundExtValue && FoundExtValue->Type == FStateParamValue::EParamType::Float)
    {
        return FoundExtValue->FloatValue;
    }
    
    // Для обратной совместимости проверяем также в старых параметрах
    const float* FoundValue = StateParams.Find(ParamName);
    return FoundValue ? *FoundValue : DefaultValue;
}

FString UMedComEnemyState::GetStateParamString(const FName& ParamName, const FString& DefaultValue) const
{
    const FStateParamValue* FoundValue = ExtendedParams.Find(ParamName);
    if (FoundValue && FoundValue->Type == FStateParamValue::EParamType::String)
    {
        return FoundValue->StringValue;
    }
    return DefaultValue;
}

bool UMedComEnemyState::GetStateParamBool(const FName& ParamName, bool DefaultValue) const
{
    // Сначала проверяем расширенные параметры
    const FStateParamValue* FoundValue = ExtendedParams.Find(ParamName);
    if (FoundValue && FoundValue->Type == FStateParamValue::EParamType::Bool)
    {
        return FoundValue->BoolValue;
    }
    
    // Для обратной совместимости можем интерпретировать float как bool
    const float* FoundFloatValue = StateParams.Find(ParamName);
    if (FoundFloatValue)
    {
        return FMath::Abs(*FoundFloatValue) > SMALL_NUMBER;
    }
    
    return DefaultValue;
}

bool UMedComEnemyState::TryActivateAbility(AMedComEnemyCharacter* Owner, const FString& AbilityClassName)
{
    if (!Owner)
    {
        return false;
    }
    
    // Получаем AbilitySystemComponent
    UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent();
    if (!ASC)
    {
        LogStateMessage(Owner, FString::Printf(TEXT("Failed to activate ability: No AbilitySystemComponent found")), 
                        ELogVerbosity::Warning);
        return false;
    }
    
    // Находим класс способности по имени
    UClass* AbilityClass = FindObject<UClass>(nullptr, *AbilityClassName);
    if (!AbilityClass || !AbilityClass->IsChildOf(UGameplayAbility::StaticClass()))
    {
        LogStateMessage(Owner, FString::Printf(TEXT("Failed to activate ability: Invalid class '%s'"), 
                                               *AbilityClassName), ELogVerbosity::Warning);
        return false;
    }
    
    // Ищем способность с этим классом в активных способностях
    for (const FGameplayAbilitySpec& AbilitySpec : ASC->GetActivatableAbilities())
    {
        if (AbilitySpec.Ability && AbilitySpec.Ability->GetClass() == AbilityClass)
        {
            // Активируем способность
            ASC->TryActivateAbility(AbilitySpec.Handle);
            LogStateMessage(Owner, FString::Printf(TEXT("Activated ability: %s"), *AbilityClassName));
            return true;
        }
    }
    
    LogStateMessage(Owner, FString::Printf(TEXT("Failed to activate ability: '%s' not found in activatable abilities"), 
                                          *AbilityClassName), ELogVerbosity::Warning);
    return false;
}

bool UMedComEnemyState::TryDeactivateAbility(AMedComEnemyCharacter* Owner, const FString& AbilityClassName)
{
    if (!Owner)
    {
        return false;
    }
    
    // Получаем AbilitySystemComponent
    UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent();
    if (!ASC)
    {
        return false;
    }
    
    // Находим класс способности по имени
    UClass* AbilityClass = FindObject<UClass>(nullptr, *AbilityClassName);
    if (!AbilityClass || !AbilityClass->IsChildOf(UGameplayAbility::StaticClass()))
    {
        return false;
    }
    
    // Ищем активную способность с этим классом
    bool bDeactivated = false;
    for (const FGameplayAbilitySpec& AbilitySpec : ASC->GetActivatableAbilities())
    {
        if (AbilitySpec.Ability && AbilitySpec.Ability->GetClass() == AbilityClass && AbilitySpec.IsActive())
        {
            // Деактивируем способность
            ASC->CancelAbilityHandle(AbilitySpec.Handle);
            bDeactivated = true;
            LogStateMessage(Owner, FString::Printf(TEXT("Deactivated ability: %s"), *AbilityClassName));
        }
    }
    
    return bDeactivated;
}

void UMedComEnemyState::LogStateMessage(AMedComEnemyCharacter* Owner, const FString& Message, ELogVerbosity::Type Verbosity) const
{
    FString OwnerName = Owner ? Owner->GetName() : TEXT("Unknown");
    FString StateName = GetClass()->GetName();
    
    switch (Verbosity)
    {
        case ELogVerbosity::Error:
            UE_LOG(LogMedComEnemyState, Error, TEXT("[%s] State '%s': %s"), *OwnerName, *StateName, *Message);
            break;
        case ELogVerbosity::Warning:
            UE_LOG(LogMedComEnemyState, Warning, TEXT("[%s] State '%s': %s"), *OwnerName, *StateName, *Message);
            break;
        case ELogVerbosity::Display:
            UE_LOG(LogMedComEnemyState, Display, TEXT("[%s] State '%s': %s"), *OwnerName, *StateName, *Message);
            break;
        case ELogVerbosity::Verbose:
            UE_LOG(LogMedComEnemyState, Verbose, TEXT("[%s] State '%s': %s"), *OwnerName, *StateName, *Message);
            break;
        default:
            UE_LOG(LogMedComEnemyState, Log, TEXT("[%s] State '%s': %s"), *OwnerName, *StateName, *Message);
            break;
    }
}