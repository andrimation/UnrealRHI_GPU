// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DynamicMeshBuilder.h"
#include "RHIActor.h"

// Aby wygenerowaæ pure mesha - trójk¹t w scenie unreala, tworzymy w³asn¹ klasê bufora, która dziedziczy po FVertexBuffer. Klasa ta musi implementowaæ metodê InitRHI(FRHICommandListBase& RHICmdList), w której tworzymy bufor
// i mo¿emy skopiowaæ wierzcho³ki do bufora. InitRHI(FRHICommandListBase& RHICmdList) zapewnia ¿e w odpowiednim momencie bufor zostanie utworzony w Render Thread
// ==============
// Generic Buffer 
// ==============

template<typename T>
class TMeshGenericBuffer : public FVertexBuffer
{
public:

	TMeshGenericBuffer() = default;

	TMeshGenericBuffer(const TCHAR* InDebugName, EPixelFormat InShaderResourceFormat)
		: DebugName(InDebugName)
		, ShaderResourceFormat(InShaderResourceFormat)
	{
	}

	TArray<T> CPUData;
	FShaderResourceViewRHIRef ShaderResourceView;

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		uint32 BufferSize = CPUData.Num() * sizeof(T);
		FRHIBufferCreateDesc BufferDesc = FRHIBufferCreateDesc::CreateVertex(TEXT("VertexPositionBuffer"), BufferSize);
		BufferDesc.AddUsage(EBufferUsageFlags::ShaderResource | EBufferUsageFlags::Static);
		BufferDesc.SetInitialState(ERHIAccess::VertexOrIndexBuffer);

		VertexBufferRHI = RHICmdList.CreateBuffer(BufferDesc);

		void* BufferData = RHICmdList.LockBuffer(VertexBufferRHI, 0, BufferSize, RLM_WriteOnly);
		memcpy(BufferData, CPUData.GetData(), BufferSize);
		RHICmdList.UnlockBuffer(VertexBufferRHI);

		ShaderResourceView = RHICmdList.CreateShaderResourceView(VertexBufferRHI, FRHIViewDesc::CreateBufferSRV()
			.SetType(FRHIViewDesc::EBufferType::Typed)
			.SetFormat(ShaderResourceFormat));

	};

	virtual void ReleaseRHI() override
	{
		VertexBufferRHI.SafeRelease();
		ShaderResourceView.SafeRelease();
	}

private:
	FString DebugName;
	EPixelFormat ShaderResourceFormat;
};

// ==============
// Index Buffer 
// ==============

class FMeshIndexBuffer : public FIndexBuffer
{
public:
	TArray<uint32> Indices;

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		int32 BufferSize = Indices.Num() * sizeof(int32);
		FRHIBufferCreateDesc BufferDesc = FRHIBufferCreateDesc::CreateIndex(TEXT("MyIndexBuffer"), BufferSize, sizeof(int32));
		BufferDesc.SetInitialState(ERHIAccess::VertexOrIndexBuffer);
		IndexBufferRHI = RHICmdList.CreateBuffer(BufferDesc);

		void* BufferToWrite = RHICmdList.LockBuffer(IndexBufferRHI, 0, BufferSize, EResourceLockMode::RLM_WriteOnly);
		memcpy(BufferToWrite, Indices.GetData(), BufferSize);
		RHICmdList.UnlockBuffer(IndexBufferRHI);
	}
};

