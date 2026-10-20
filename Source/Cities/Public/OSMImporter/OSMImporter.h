#pragma once

#include "CoreMinimal.h"

struct FOSMFeature
{
    int64 Id = 0;
    FString Type;
    TArray<FVector> Points;
    bool bClosed = false;
};

struct FOSMData
{
    TArray<FOSMFeature> Buildings;
    TArray<FOSMFeature> Roads;
};

class FOSMImporter
{
public:
    static bool LoadFromFile(const FString& FilePath, FOSMData& OutData );
};