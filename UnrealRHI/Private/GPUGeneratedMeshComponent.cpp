#pragma once

#include "GPUGeneratedMeshComponent.h"
#include "DeclareShader.h"
#include "RHIStaticStates.h"
#include "ShaderParameterStruct.h"

#include "TriangleUtilities.h"
#include "DynamicMeshBuilder.h"
#include "RHIResourceUtils.h"

FGPUGeneratedMeshSceneProxy::FGPUGeneratedMeshSceneProxy(const UGPUGeneratedMeshComponent* InComponent, UMaterialInterface* InMaterial, TArray<FDynamicMeshVertex>& InVertices, TArray<int32>& InIndices)
	: FPrimitiveSceneProxy(InComponent)  // Tu tworzymy obiekt klasy bazowej, czyli to samo co Super() -> z tym ¿e nie jest to UCLASS wiêc nie mamy super
	, MaterialRenderProxy(InMaterial->GetRenderProxy())
	, MaterialRelevance(InMaterial->GetRelevance_Concurrent(GetScene().GetFeatureLevel()))
	, VertexFactory(GetScene().GetFeatureLevel(), "FGPUGeneratedMeshSceneProxy")
{
	
	
}

void FGPUGeneratedMeshSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
{
    // Ok - czyli tu przygotowujemy zasoby GPU do render threadu - 
    // 1) - inicjalizujemy buffory - a w trakcie inicja
	// 2) - tworzymy vertex factory - czyli opis jak GPU ma interpretowaæ dane w buforze ( to co robiliœmy wczeœniej w ENQUEUE_RENDER_COMMAND ) 
	check(IsInRenderingThread());

    InitializeCPUDataBuffers();

    InitializeGPUBuffers(RHICmdList);
	InitializeVertexFactory(RHICmdList);
	
	GenerateMeshOnGPU(RHICmdList);
}

void FGPUGeneratedMeshSceneProxy::DestroyRenderThreadResources()
{
    check(IsInRenderingThread());

    VertexFactory.ReleaseResource();  // Zwalniamy w kolejnoœci odwrotnej do tworzenia - czyli najpierw zwalniamy vertex factory, a póŸniej buffory

    // Jak ma siê zwalnianie buforów danych "In" do zwalaniania w kolejnoœci tworzenia ? 

    VertexPosBuffer.ReleaseResource();
    TangentBuffer.ReleaseResource();
    TexCoordBuffer.ReleaseResource();
    IndexBuffer.ReleaseResource();

    // 3. Zwalniamy widoki SRV
    PositionsSRV.SafeRelease();
    TangentsSRV.SafeRelease();
    TexCoordsSRV.SafeRelease();

    // 4. Zwalniamy widoki UAV
    PositionsUAV.SafeRelease();
    TangentsUAV.SafeRelease();
    TexCoordsUAV.SafeRelease();
    IndicesUAV.SafeRelease();

    // 5. Zwalniamy w³asne referencje do buforów RHI
    PositionsRHI.SafeRelease();
    TangentsRHI.SafeRelease();
    TexCoordsRHI.SafeRelease();
    IndicesRHI.SafeRelease();
}

void FGPUGeneratedMeshSceneProxy::InitializeCPUDataBuffers(FRHICommandListBase& RHICmdList)
{
	check(InFootprints.Num() > 0);
	check(InBuildingsDescription.Num() > 0);

    // Footprints buffers
    FRHIBufferCreateDesc FootprintsBufferDesc = 
		FRHIBufferCreateDesc::CreateStructured(
            TEXT("InFootprintsBuffer"),
            sizeof(FVector3f) * InFootprints.Num(),
			sizeof(FVector3f)
        );

	FootprintsBufferDesc.AddUsage(EBufferUsageFlags::ShaderResource);
    FootprintsBufferDesc.SetInitialState(ERHIAccess::SRVCompute);

    // To poni¿ej tworzy bufor RHI i kopiuje od razu do niego dane z Array
    InFootprintsBufferRHI = UE::RHIResourceUtils::CreateBufferWithArray(
        RHICmdList,
        FootprintsBufferDesc,
        InFootprints
	);

    // Tworzymy SRV bufora ze skopiowanymi ju¿ danymi. SRV pozwoli na odczyt przez shader
    InFootprintsSRV = RHICmdList.CreateShaderResourceView(
        InFootprintsBufferRHI.GetReference(),
        FRHIViewDesc::CreateBufferSRV()
        .SetType(FRHIViewDesc::EBufferType::Structured)  // ::Structured oznacza ¿e bufor zawiera elementy o okreœlonym stride, i jego zawartoœæ zostanie opisana w HLSL
    );
}

void FGPUGeneratedMeshSceneProxy::InitializeGPUBuffers(FRHICommandListBase& RHICmdList)
{
    check(IsInRenderingThread());
    check(RHISupportsIndexBufferUAVs(GMaxRHIShaderPlatform));

	// CreateVertexBuffer s³u¿y do tworzenia buforów z vertexami, tangensami, texcoords. IndexBuffer tworzymy osobno
    auto CreateVertexBuffer = [&](
        const TCHAR* Name,
        uint32 Size,
        EPixelFormat UAVFormat,
        EPixelFormat SRVFormat,
        FBufferRHIRef& OutBuffer,
        FUnorderedAccessViewRHIRef& OutUAV,
        FShaderResourceViewRHIRef& OutSRV
        )
    {
			// W tej lambdzie tworzymy bufor i zwracamy do niego referencje w OutBuffer, 
            // a tak¿e tworzymy jego UAV i SRV i zrwacamy przez ref

			FRHIBufferCreateDesc Desc = FRHIBufferCreateDesc::CreateVertex(Name, Size);
            // Do czego bufor bêdzie u¿ywany
			Desc.AddUsage(EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource);
            // Stan po zainicjalizowaniu
			Desc.SetInitialState(ERHIAccess::UAVCompute);
			// Alokacja bufora w pamiêci GPU
            OutBuffer = RHICmdList.CreateBuffer(Desc);

			// Tworzymy typed UAV - czyli view do bufora, który pozwala na zapis z compute shaderów
			OutUAV = RHICmdList.CreateUnorderedAccessView(
                OutBuffer.GetReference(), 
                FRHIViewDesc::CreateBufferUAV()
				.SetType(FRHIViewDesc::EBufferType::Typed)
				.SetFormat(UAVFormat)
            );

			// Tworzymy typed SRV - czyli view do bufora, który pozwala na odczyt z shaderów
            OutSRV = RHICmdList.CreateShaderResourceView(
                OutBuffer.GetReference(),
                FRHIViewDesc::CreateBufferSRV()
                .SetType(FRHIViewDesc::EBufferType::Typed)
                .SetFormat(SRVFormat)
            );
    };

    CreateVertexBuffer(
        TEXT("PositionsBuffer"),
        sizeof(FVector3f) * 4,  // 4 wierzcho³ki
        PF_R32_FLOAT,  // problem tu -> powinienem w shaderze mieæ buffor<float> i zapisywaæ kolejne 3 floaty dla ka¿dego wierzcho³ka
		PF_R32_FLOAT,  // Dx12 nie obs³uguje PF_R32G32B32_FLOAT jako typu UAV/SRV, wiêc musimy u¿yæ PF_R32_FLOAT i w shaderze zapisywaæ kolejne floaty jako czêœci sk³adowe wektora
        PositionsRHI,
        PositionsUAV,
        PositionsSRV
    );

    CreateVertexBuffer(
        TEXT("WallTangents"),
		sizeof(FPackedNormal) * 2 * 4,  // 4 wierzcho³ki, 2x FPackedNormal na wierzcho³ek (TangentX i TangentZ)
        PF_R32_UINT,
        PF_R8G8B8A8_SNORM,
        TangentsRHI,
        TangentsUAV,
        TangentsSRV
    );

    CreateVertexBuffer(
        TEXT("TexCoords"),
        sizeof(FVector2f) * 4,  // 4 wierzcho³ki, 2x FPackedNormal na wierzcho³ek (TangentX i TangentZ)
        PF_G32R32F,
		PF_G32R32F,   
        TexCoordsRHI,
        TexCoordsUAV,
        TexCoordsSRV
    );

	// Index buffer - tworzymy osobno bo nie jest to vertex buffer
    FRHIBufferCreateDesc IndexBufferDesc = FRHIBufferCreateDesc::CreateIndex(
		TEXT("IndexBuffer"),
        sizeof(uint32) * 6,
        sizeof(uint32)
    );

	IndexBufferDesc.AddUsage(EBufferUsageFlags::UnorderedAccess);
	IndexBufferDesc.SetInitialState(ERHIAccess::UAVCompute);

	IndicesRHI = RHICmdList.CreateBuffer(IndexBufferDesc);

    IndicesUAV = RHICmdList.CreateUnorderedAccessView(
        IndicesRHI.GetReference(),
        FRHIViewDesc::CreateBufferUAV()
        .SetType(FRHIViewDesc::EBufferType::Typed)
        .SetFormat(PF_R32_UINT)
	);

	VertexPosBuffer.SetRHI(PositionsRHI);
	TangentBuffer.SetRHI(TangentsRHI);
	TexCoordBuffer.SetRHI(TexCoordsRHI);
	IndexBuffer.SetRHI(IndicesRHI);

    VertexPosBuffer.InitResource(RHICmdList);
    TangentBuffer.InitResource(RHICmdList);
    TexCoordBuffer.InitResource(RHICmdList);
    IndexBuffer.InitResource(RHICmdList);
}

void FGPUGeneratedMeshSceneProxy::InitializeVertexFactory(FRHICommandListBase& RHICmdList)
{
    FLocalVertexFactory::FDataType Data;

    Data.PositionComponentSRV = PositionsSRV;
    Data.TangentsSRV = TangentsSRV;
    Data.TextureCoordinatesSRV = TexCoordsSRV;

    Data.PositionComponent = FVertexStreamComponent(
        &VertexPosBuffer,
        0,  //  <- STRUCT_OFFSET - wskazuje ile bajtów od pocz¹tku struktury nale¿y siê przesun¹æ, aby dostaæ siê do pola Position (w naszym przypadku bêdzie to 0 - moglibyœmy tu wpisaæ 0 z palca )
        sizeof(FVector3f), //  <- stride - czyli co ile bajtów jest koleny nowy vertex
        VET_Float3
    );

    Data.TangentBasisComponents[0] = FVertexStreamComponent(
        &TangentBuffer,
        0,                          // 0 TangentX jest na pocz¹tku struktury FPackedNormal wiêc offset = 0
        2 * sizeof(FPackedNormal),  // 2 * sizeof(FPackedNormal) bo na jeden wierzcho³ek przechowujemy TangentX i TangentZ
        VET_PackedNormal
    );

    Data.TangentBasisComponents[1] = FVertexStreamComponent(
        &TangentBuffer,
        sizeof(FPackedNormal),     // TangentZ jest po TangentX wiêc offset = sizeof(FPackedNormal)
        2 * sizeof(FPackedNormal), // <- jak przejdziemy po 2 FPackedNormal, to znaczy ¿e zaczyna siê nowy wierzcho³ek
        VET_PackedNormal
    );

    Data.TextureCoordinates.Add(
        FVertexStreamComponent(
            &TexCoordBuffer,
            0,
            sizeof(FVector2f),
            VET_Float2
        )
    );

    Data.NumTexCoords = 1;
    Data.LightMapCoordinateIndex = 0;

    VertexFactory.SetData(RHICmdList, Data);
    VertexFactory.InitResource(RHICmdList);
}

void FGPUGeneratedMeshSceneProxy::GenerateMeshOnGPU(FRHICommandListBase& RHICmdList)
{

    // Get compiled shader 
    TShaderMapRef<FGenerateWallCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

    // Set up parameters for the compute shader
    FGenerateWallCS::FParameters Parameters;

    Parameters.InFootprints = InFootprintsSRV;
    Parameters.InBuildings = 
    Parameters.BuildingsCount

    Parameters.OutPositions = PositionsUAV;  // ustawiamy UAV bufora jako OutPositions w shaderze
	Parameters.OutTangents  = TangentsUAV;
	Parameters.OutTextureCoords = TexCoordsUAV;
	Parameters.OutIndices = IndicesUAV;


    // .UAV umi¿liwia zapis z compute shadera
   // .SRV umo¿liwia odczyt z shadera
   // .Buffer - to po prostu w³aœciwy bufor

    FRHIComputeCommandList& ComputeCmdList = FRHIComputeCommandList::Get(RHICmdList);

    // Bindujemy compute shader do pipeline
    SetComputePipelineState(
        ComputeCmdList,
        ComputeShader.GetComputeShader()
    );

    // Bindujemy parametry do compute shader
    SetShaderParameters(
        ComputeCmdList,
        ComputeShader,
        ComputeShader.GetComputeShader(),
        Parameters
    );

    // Execute on thread group count of 1,1,1 ( czyli jeden thread group ) 
    ComputeCmdList.DispatchComputeShader(
        1, // ThreadGroupCountX
        1, // ThreadGroupCountY
        1  // ThreadGroupCountZ
    );

    // Buffer transition -> przekszta³camy bufor do stanu w którym mo¿e byæ odczytywany
    ComputeCmdList.Transition(
        FRHITransitionInfo(  // Tu tworzymy obiekt transition info, który dopiero wype³niamy opisem transition )
            PositionsRHI,
            ERHIAccess::UAVCompute,  // Z jakiego stanu przechodzimy,
            ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVGraphicsNonPixel
            // Do jakiego stanu przechodzimy - VertexOrIndexBuffer oznacza ¿e bufor bêdzie u¿ywany jako vertex buffer
        )
    );

    ComputeCmdList.Transition(
        FRHITransitionInfo(  // Tu tworzymy obiekt transition info, który dopiero wype³niamy opisem transition )
            TangentsRHI,
            ERHIAccess::UAVCompute,  // Z jakiego stanu przechodzimy,
            ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVGraphicsNonPixel
            // Do jakiego stanu przechodzimy - VertexOrIndexBuffer oznacza ¿e bufor bêdzie u¿ywany jako vertex buffer
        )
    );

    ComputeCmdList.Transition(
        FRHITransitionInfo(  // Tu tworzymy obiekt transition info, który dopiero wype³niamy opisem transition )
            TexCoordsRHI,
            ERHIAccess::UAVCompute,  // Z jakiego stanu przechodzimy,
            ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVGraphicsNonPixel
            // Do jakiego stanu przechodzimy - VertexOrIndexBuffer oznacza ¿e bufor bêdzie u¿ywany jako vertex buffer
        )
    );

    ComputeCmdList.Transition(
        FRHITransitionInfo(  // Tu tworzymy obiekt transition info, który dopiero wype³niamy opisem transition )
            IndicesRHI,
            ERHIAccess::UAVCompute,  // Z jakiego stanu przechodzimy,
			ERHIAccess::VertexOrIndexBuffer // Index buffer nie potrzebuje SRVGraphicsNonPixel bo nie jest u¿ywany w shaderze, tylko w DrawIndexedPrimitive
            // Do jakiego stanu przechodzimy - VertexOrIndexBuffer oznacza ¿e bufor bêdzie u¿ywany jako vertex buffer
        )
    );

    // Ok teraz pytanie - jak powinny by ustawione pozosta³e bufory - chyba tak na prawdê tak samo ? 
}


void FGPUGeneratedMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
    check(IsInParallelRenderingThread());

    for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
    {
        // Ok za³ó¿my ¿e VisibilityMap jest 5 - czyli 0101 
        // 1 << x przesuwa 1 o x bitów w lewo
        //                         // sprawdzamy z: 0101
        // 1) iteracja - ViewIndex = 0 ->  1 << 0 = 0001 -> czyli nie robimy continue, bo != 0
        // 2) iteracja - ViewIndex = 1 ->  1 << 1 = 0010 -> czyli robimy continue, bo == 0
        // 3) iteracja - ViewIndex = 2 ->  1 << 2 = 0100 -> czyli nie robimy continue, bo != 0
        // 4) iteracja - ViewIndex = 3 ->  1 << 3 = 1000 -> czyli robimy continue, bo == 0

        if ((VisibilityMap & (1 << ViewIndex)) == 0)  // VisibilityMap - czyli dla których FSceneView dany FPrimitiveSceneProxy jest widoczny.
        {
            continue;
        }

        // Opis geometrii
        FMeshBatch& Mesh = Collector.AllocateMesh(); // FMeshBatch - struktura opisuj¹ca jak renderowaæ danego mesha ( jest po stronie CPU )
        // FMeshBatch - jest to obiekt korzystaj¹cy z tego samego materia³u i tego samego vertex buffera

        Mesh.VertexFactory = &VertexFactory;            // Vertex factory - czyli opisujemy jak GPU ma interpretowaæ dane w buforze
        Mesh.MaterialRenderProxy = MaterialRenderProxy; // materia³ do renderowania
        Mesh.Type = PT_TriangleList;                    // typ prymitywu - w naszym przypadku trójk¹ty
        Mesh.DepthPriorityGroup = SDPG_World;           // Depth priority group - czyli w jakiej kolejnoœci renderowaæ ( w naszym przypadku World - czyli normalnie )
        Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative(); // Czy odwróciæ culling - czyli czy odwróciæ kolejnoœæ wierzcho³ków ( w naszym przypadku nie )
        Mesh.bDisableBackfaceCulling = false;            // Czy wy³¹czyæ culling - czyli czy renderowaæ obie strony ( w naszym przypadku nie )
        Mesh.bCanApplyViewModeOverrides = true;         // Czy mo¿na zastosowaæ nadpisanie trybu widoku ( w naszym przypadku tak )
        Mesh.bUseForMaterial = true;                    // Czy u¿ywaæ do materia³u ( w naszym przypadku tak )
        Mesh.bUseForDepthPass = true;                   // Czy u¿ywaæ do passu g³êbokoœci ( w naszym przypadku tak )
        Mesh.CastShadow = true;                         // Czy rzucaæ cieñ ( w naszym przypadku tak )

        // FMeshBatch - to jest jakby "stan renderowania" -czyli "u¿ywaj¹c okreœlonego vertex factory, okreœlonego materia³u i innych okreœlonych ustawieñ, 
        // wyrenderuj geometriê okreœlon¹ w Mesh.Elements. Elements jest TArray<FMeshBatchElement> - czyli u¿ywaj¹c tych samych ustawieñ, mo¿emy wyrenderowaæ wiele
        // porcji geometrii. Ka¿dy osobny FBatchElement to osobny "Draw Call" ( co nie oznacza ¿e "tylko" jeden - bo np unreal mo¿e wyrenderowaæ
        // Depth pass, Base pass, Shadow pass itd.

        // Kilka FMeshBatchElement bêdzie korzystaæ z tego samego vertex buffera. 


        // Jeden Draw -
        FMeshBatchElement& BatchElement = Mesh.Elements[0];
        BatchElement.IndexBuffer = &IndexBuffer;        // Index buffer - czyli bufor indeksów ( w naszym przypadku 0,1,2 )
        BatchElement.FirstIndex = 0;                    // Pierwszy indeks W INDEX BUFFERZE do renderowania ( w naszym przypadku 0 ) 
        BatchElement.NumPrimitives = 2;                 // Liczba prymitywów do renderowania ( w naszym przypadku 1 trójk¹t )
        BatchElement.MinVertexIndex = 0;                // Minimalny indeks wierzcho³ka ( w naszym przypadku 0 )
        BatchElement.MaxVertexIndex = 3; // Maksymalny indeks wierzcho³ka ( w naszym przypadku 2 )
        BatchElement.NumInstances = 1;                  // Liczba instancji do renderowania ( w naszym przypadku 1 )

        Collector.AddMesh(ViewIndex, Mesh);
    }
}

FPrimitiveViewRelevance FGPUGeneratedMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
{
    FPrimitiveViewRelevance Result;
    Result.bDrawRelevance = IsShown(View);
    Result.bShadowRelevance = IsShadowCast(View);
    Result.bDynamicRelevance = true;   // wymusza wywo³anie GetDynamicMeshElements
    Result.bRenderInMainPass = ShouldRenderInMainPass();
    Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
    Result.bRenderCustomDepth = ShouldRenderCustomDepth();
    MaterialRelevance.SetPrimitiveViewRelevance(Result);
    Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;
    return Result;
}

bool FGPUGeneratedMeshSceneProxy::CanBeOccluded() const
{
    return !MaterialRelevance.bDisableDepthTest;
}

uint32 FGPUGeneratedMeshSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + GetAllocatedSize();
}

SIZE_T FGPUGeneratedMeshSceneProxy::GetTypeHash() const
{
    static size_t UniquePointer; 

    return reinterpret_cast<size_t>(&UniquePointer);
}

UGPUGeneratedMeshComponent::UGPUGeneratedMeshComponent()
{
}

FPrimitiveSceneProxy* UGPUGeneratedMeshComponent::CreateSceneProxy()
{
    UMaterialInterface* MaterialToUse = Material ? Material : UMaterial::GetDefaultMaterial(MD_Surface);
    return new FGPUGeneratedMeshSceneProxy(this, MaterialToUse,Vertices,Indices);
}

FBoxSphereBounds UGPUGeneratedMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
    // Geometria powstaje na GPU - bounds musz¹ odpowiadaæ parametrom compute shadera
    // (StartPosition = (-100,0,0), EndPosition = (100,0,0), WallHeight = 400)
    const FBox LocalBox(FVector(-100.0, -1.0, 0.0), FVector(100.0, 1.0, 400.0));

    return FBoxSphereBounds(LocalBox.ExpandBy(1.0)).TransformBy(LocalToWorld);
}

void UGPUGeneratedMeshComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	UMaterialInterface* MaterialToUse = Material ? Material : UMaterial::GetDefaultMaterial(MD_Surface);
	OutMaterials.Add(MaterialToUse);
}

void UGPUGeneratedMeshComponent::SetVerticesAndIndices(TArray<FDynamicMeshVertex>&& InVertices, TArray<int32>&& InIndices)
{
	Vertices = MoveTemp(InVertices);
	Indices =  MoveTemp(InIndices);
}

