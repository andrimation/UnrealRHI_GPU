#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "BuildingGPUData.h"

class FTriangleShaderVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTriangleShaderVS);
};

class FTriangleShaderPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTriangleShaderPS);
};

class FGenerateWallCS : public FGlobalShader
{
public: 
	DECLARE_GLOBAL_SHADER(FGenerateWallCS);
	SHADER_USE_PARAMETER_STRUCT(FGenerateWallCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		
		// Do  GPU przekazujemy maksymalnie sp³aszczone dane 

		// Shader Input
		SHADER_PARAMETER_SRV(
			StructuredBuffer<float3>, 
			InFootprints
		)
		
		SHADER_PARAMETER_SRV(
			StructuredBuffer<FBuildingGPUData>, 
			InBuildings
		)

		SHADER_PARAMETER(uint32, BuildingsCount)  // Jeœli odpalamy shader dla wielu budynków, to musimy wiedzieæ ile ich jest.
	
		// Shader Output
		SHADER_PARAMETER_UAV(
			RWBuffer<float>,  // float3 nie jest obs³ugiwany jako UAV w DX12, wiêc musimy u¿yæ float i zapisywaæ kolejne 3 floaty jako czêœci sk³adowe wektora
			OutPositions
		)

		SHADER_PARAMETER_UAV(
			RWBuffer<uint>,
			OutTangents
		)

		SHADER_PARAMETER_UAV(
			RWBuffer<float2>,
			OutTextureCoords
		)

		SHADER_PARAMETER_UAV(
			RWBuffer<uint>,
			OutIndices
		)

	END_SHADER_PARAMETER_STRUCT()
};