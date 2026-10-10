// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshBuilder.h"

inline FDynamicMeshVertex CreateDynamicMeshVertex(const FVector3f& Position, const FVector2f& UV, const FColor& Color)
{
	FDynamicMeshVertex Vertex;

	Vertex.Position = Position;
	Vertex.TextureCoordinate[0] = UV;
	Vertex.Color = Color;

	Vertex.SetTangents(
		FVector3f(1.0f, 0.0f, 0.0f),    // TangentX
		FVector3f(0.0f, 0.0f, 1.0f),    // TangentY
		FVector3f(0.0f, 1.0f, 0.0f)     // Normal   -> Normal jest wyliczany jako CrossProduct z obu Tangentów. Jeœli oba s¹ dodatnie, albo oba ujemne, to normal jest +Z 
										// jeœli jeden jest dodatni a drugi ujemny ( albo na odwrót ) to normal jest zawsze -Z
	);

	return Vertex;
}
