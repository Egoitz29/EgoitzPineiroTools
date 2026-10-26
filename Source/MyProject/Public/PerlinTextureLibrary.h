
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/Texture2D.h"
#include "PerlinTextureLibrary.generated.h"

UCLASS()
class UPerlinTextureLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Perlin Noise")
    static UTexture2D* GenerarTexturaPerlin(
        int32 Width = 256,
        int32 Height = 256,
        float Scale = 0.02f
    );
};
