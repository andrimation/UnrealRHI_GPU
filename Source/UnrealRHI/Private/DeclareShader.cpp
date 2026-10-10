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

// Coœ w startup module ! -> mapowaine œcie¿ek 