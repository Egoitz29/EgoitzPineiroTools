
#include "PerlinTextureLibrary.h"

UTexture2D* UPerlinTextureLibrary::GenerarTexturaPerlin(
    int32 Width, int32 Height, float Scale)
{
    if (Width <= 0 || Height <= 0 || Scale <= 0.0f)
        return nullptr;

    UTexture2D* Texture = UTexture2D::CreateTransient(
        Width, Height, PF_B8G8R8A8
    );

    if (!Texture)
        return nullptr;

    FTexture2DMipMap& Mip =
        Texture->GetPlatformData()->Mips[0];

    FColor* Pixels = static_cast<FColor*>(
        Mip.BulkData.Lock(LOCK_READ_WRITE)
        );

    float OffsetX = FMath::FRandRange(0.f, 1000.f);
    float OffsetY = FMath::FRandRange(0.f, 1000.f);

    for (int32 Y = 0; Y < Height; Y++)
    {
        for (int32 X = 0; X < Width; X++)
        {
            float Noise = FMath::PerlinNoise2D(
                FVector2D(
                    X * Scale + OffsetX,
                    Y * Scale + OffsetY
                )
            );

            float Value = (Noise + 1.0f) * 0.5f;

            uint8 Gray = static_cast<uint8>(
                FMath::Clamp(
                    FMath::RoundToInt(Value * 255.f),
                    0, 255
                )
                );

            Pixels[Y * Width + X] =
                FColor(Gray, Gray, Gray, 255);
        }
    }

    Mip.BulkData.Unlock();

    Texture->UpdateResource();

    return Texture;
}
