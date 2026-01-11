#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/EngineTypes.h" 
#include "GameFramework/Actor.h"
#include "MedComTraceUtils.generated.h"

/**
 * Вспомогательный класс для трассировки в системе оружия.
 * Содержит функции для визуализации отладочной информации и 
 * общие утилиты трассировки.
 */
UCLASS()
class MEDCOMCORE_API UMedComTraceUtils : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** 
     * Визуализирует трассировку и попадания для отладки.
     * Зеленая линия - нет блокирующих хитов, красная - первый блокирующий хит.
     */
    UFUNCTION(BlueprintCallable, Category = "MedCom|Trace|Debug")
    static void DrawDebugTrace(const UWorld* World, const FVector& Start, const FVector& End, 
                               const TArray<FHitResult>& Hits, float DrawTime = 2.0f);
    
    /**
     * Выполняет трассировку по заданным параметрам, возвращает результаты.
     * @param World - Игровой мир
     * @param Start - Начальная точка трассировки
     * @param End - Конечная точка трассировки
     * @param TraceProfile - Профиль трассировки
     * @param ActorsToIgnore - Акторы, которые нужно игнорировать при трассировке
     * @param bDebug - Визуализировать трассировку (для отладки)
     * @param DebugDrawTime - Длительность отображения отладочных линий (сек)
     * @param OutHits - Результаты трассировки
     * @return true, если трассировка выполнена успешно
     */
    UFUNCTION(BlueprintCallable, Category = "MedCom|Trace")
    static bool PerformLineTrace(const UWorld* World, const FVector& Start, const FVector& End, 
                                FName TraceProfile, const TArray<AActor*>& ActorsToIgnore, 
                                bool bDebug, float DebugDrawTime, TArray<FHitResult>& OutHits);
    
    /**
     * Определяет точку прицеливания в мире (куда направлен центр экрана).
     * @param PlayerController - Контроллер игрока
     * @param MaxRange - Максимальная дальность трассировки
     * @param TraceProfile - Профиль трассировки
     * @param ActorsToIgnore - Акторы, которые нужно игнорировать при трассировке
     * @param bDebug - Визуализировать трассировку (для отладки)
     * @param DebugDrawTime - Длительность отображения отладочных линий (сек)
     * @param OutCameraLocation - [Out] Позиция камеры
     * @param OutAimPoint - [Out] Точка прицеливания
     * @return true, если точка прицеливания успешно определена
     */
    UFUNCTION(BlueprintCallable, Category = "MedCom|Trace")
    static bool GetAimPoint(const APlayerController* PlayerController, float MaxRange, 
                           FName TraceProfile, const TArray<AActor*>& ActorsToIgnore, 
                           bool bDebug, float DebugDrawTime, 
                           FVector& OutCameraLocation, FVector& OutAimPoint);
};