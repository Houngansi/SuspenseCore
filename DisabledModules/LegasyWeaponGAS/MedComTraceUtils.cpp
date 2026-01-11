// Copyright MedCom Team. All Rights Reserved.

#include "Core/AbilitySystem/Abilities/Tasks/MedComTraceUtils.h"
#include "DrawDebugHelpers.h"
#include "Engine/EngineTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void UMedComTraceUtils::DrawDebugTrace(const UWorld* World, const FVector& Start, const FVector& End, 
                                      const TArray<FHitResult>& Hits, float DrawTime)
{
    if (!World || !GEngine || !GEngine->bEnableOnScreenDebugMessages)
    {
        return;
    }
    
    // Если нет попаданий, просто рисуем зеленую линию (нет блокирующих хитов)
    if (Hits.Num() == 0)
    {
        DrawDebugLine(World, Start, End, FColor::Green, false, DrawTime, 0, 1.0f);
        return;
    }
    
    FVector PrevPoint = Start;
    bool bHasBlockingHit = false;
    
    // Сначала проверяем, есть ли блокирующие хиты
    for (const FHitResult& Hit : Hits)
    {
        if (Hit.bBlockingHit)
        {
            bHasBlockingHit = true;
            break;
        }
    }
    
    // Рисуем отдельные сегменты для каждого попадания
    for (const FHitResult& Hit : Hits)
    {
        // Выбираем цвет: красный для блокирующих, оранжевый для неблокирующих
        FColor LineColor = Hit.bBlockingHit ? FColor::Red : FColor::Orange;
        
        // Если нет блокирующих хитов, используем зеленый цвет
        if (!bHasBlockingHit)
        {
            LineColor = FColor::Green;
        }
        
        // Рисуем линию до точки попадания
        DrawDebugLine(World, PrevPoint, Hit.ImpactPoint, LineColor, false, DrawTime, 0, 2.0f);
        
        // Рисуем сферу в точке попадания
        DrawDebugSphere(World, Hit.ImpactPoint, 10.0f, 8, LineColor, false, DrawTime);
        
        // Рисуем нормаль поверхности
        DrawDebugLine(World, Hit.ImpactPoint, Hit.ImpactPoint + Hit.ImpactNormal * 50.0f, 
                     FColor::Blue, false, DrawTime, 0, 1.0f);
        
        // Рисуем имя объекта, в который попали
        if (Hit.GetActor())
        {
            // Исправленный вызов DrawDebugString
            DrawDebugString(World, Hit.ImpactPoint, Hit.GetActor()->GetName(), 
                            nullptr, FColor::White, DrawTime);
        }
        
        PrevPoint = Hit.ImpactPoint;
    }
    
    // Рисуем оставшийся сегмент линии после последнего попадания
    if (Hits.Num() > 0)
    {
        const FHitResult& LastHit = Hits.Last();
        if (LastHit.bBlockingHit)
        {
            DrawDebugLine(World, LastHit.ImpactPoint, End, FColor::Green, false, DrawTime, 0, 1.0f);
        }
    }
}

bool UMedComTraceUtils::PerformLineTrace(const UWorld* World, const FVector& Start, const FVector& End, 
                                        FName TraceProfile, const TArray<AActor*>& ActorsToIgnore, 
                                        bool bDebug, float DebugDrawTime, TArray<FHitResult>& OutHits)
{
    if (!World)
    {
        return false;
    }
    
    // Настраиваем параметры трассировки
    FCollisionQueryParams Params(SCENE_QUERY_STAT(MedComLineTrace), true);
    Params.bTraceComplex = true;
    Params.bReturnPhysicalMaterial = true;
    
    // Добавляем игнорируемые акторы
    for (AActor* Actor : ActorsToIgnore)
    {
        if (Actor)
        {
            Params.AddIgnoredActor(Actor);
        }
    }
    
    // Выполняем трассировку
    bool bHit = World->LineTraceMultiByProfile(OutHits, Start, End, TraceProfile, Params);
    
    // Визуализация для отладки
    if (bDebug)
    {
        DrawDebugTrace(World, Start, End, OutHits, DebugDrawTime);
    }
    
    // Если нет попаданий - добавляем "фиктивный" хит в конечную точку
    if (OutHits.Num() == 0)
    {
        FHitResult DefaultHit;
        DefaultHit.TraceStart = Start;
        DefaultHit.TraceEnd = End;
        DefaultHit.Location = End;
        DefaultHit.ImpactPoint = End;
        DefaultHit.bBlockingHit = false;
        OutHits.Add(DefaultHit);
    }
    
    return bHit;
}

bool UMedComTraceUtils::GetAimPoint(const APlayerController* PlayerController, float MaxRange, 
                                   FName TraceProfile, const TArray<AActor*>& ActorsToIgnore, 
                                   bool bDebug, float DebugDrawTime, 
                                   FVector& OutCameraLocation, FVector& OutAimPoint)
{
    if (!PlayerController)
    {
        return false;
    }
    
    UWorld* World = PlayerController->GetWorld();
    if (!World)
    {
        return false;
    }
    
    // Получаем положение и направление камеры
    FVector CameraLocation;
    FRotator CameraRotation;
    PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);
    
    OutCameraLocation = CameraLocation;
    
    // Трассировка из камеры вперед для определения точки прицеливания
    FVector CameraForward = CameraRotation.Vector();
    FVector TraceEnd = CameraLocation + (CameraForward * MaxRange);
    
    TArray<FHitResult> HitResults;
    bool bSuccess = PerformLineTrace(World, CameraLocation, TraceEnd, TraceProfile, 
                                   ActorsToIgnore, bDebug, DebugDrawTime, HitResults);
    
    if (HitResults.Num() > 0 && HitResults[0].bBlockingHit)
    {
        OutAimPoint = HitResults[0].ImpactPoint;
        return true;
    }
    else
    {
        OutAimPoint = TraceEnd;
        return false;
    }
}