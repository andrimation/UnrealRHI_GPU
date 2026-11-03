// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "../OSMImporter/OSMImporter.h"
#include "UnrealRHI/Public/TriangleUtilities.h"


// Ok - zaczynam od totalnie prostego mesha.
struct FGeneratedMesh
{
	TArray<FDynamicMeshVertex> DynamicVertices;
    TArray<int32> Indices;


    TArray<FVector> Vertices;
    TArray<FVector2D> UVs;
};

class FBuildingsGenerator 
{

public: 

	static FGeneratedMesh GenerateBuildingsWallMesh(const TArray<FOSMFeature>& InBuildingsFootprints, float Height);
    static FGeneratedMesh GenerateBuildingsRoofMesh(const TArray<FOSMFeature>& InBuildingsFootprints, float Height);

    static bool TriangulateEarClipping(const TArray<FVector>& Points, const TArray<int32>& InNextRoofIndex, TArray<int32>& OutIndices);

};