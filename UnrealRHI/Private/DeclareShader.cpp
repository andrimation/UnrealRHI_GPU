#include "DeclareShader.h"

IMPLEMENT_GLOBAL_SHADER(
	FTriangleShaderVS,
	"/UnrealRHI/TriangleShader.usf",
	"MainVS",
	SF_Vertex
);

IMPLEMENT_GLOBAL_SHADER(
	FTriangleShaderPS,
	"/UnrealRHI/TriangleShader.usf",
	"MainPS",
	SF_Pixel
);

//IMPLEMENT_GLOBAL_SHADER(
//	FGenerateWallCS,
//	"/UnrealRHI/GenerateWall.usf",
//	"MainCS",
//	SF_Compute
//);

IMPLEMENT_GLOBAL_SHADER(
	FGenerateWallCS,
	"/UnrealRHI/GenerateWall.usf",
	"GenerateBuildingsWalls",
	SF_Compute
);

// Coœ w startup module ! -> mapowaine œcie¿ek 