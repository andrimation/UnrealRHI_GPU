#pragma once

#include "TrianglePrimitiveComponent.h"

#include "TriangleUtilities.h"
#include "DynamicMeshBuilder.h"

FTrianglePrimitiveSceneProxy::FTrianglePrimitiveSceneProxy(const UTrianglePrimitiveComponent* InComponent, UMaterialInterface* InMaterial)
	: FPrimitiveSceneProxy(InComponent)  // Tu tworzymy obiekt klasy bazowej, czyli to samo co Super() -> z tym ¿e nie jest to UCLASS wiêc nie mamy super
	, MaterialRenderProxy(InMaterial->GetRenderProxy())
	, MaterialRelevance(InMaterial->GetRelevance_Concurrent(GetScene().GetFeatureLevel()))
	, VertexFactory(GetScene().GetFeatureLevel(), "FTrianglePrimitiveSceneProxy")
{
	// Do vertex buffera.Verticec i do index buffera .Indices przekazujemy dane. PóŸniej w trakcie InitRHI() dane te 
	// zostan¹ skopiowane do bufora GPU. - czyli to co siedzi .Vertices i .Indices s¹ to dane po stronie CPU
    
    if (InComponent->StaticMeshToRender)
    {
		GetVerticesAndIndicesFromStaticMesh(InComponent->StaticMeshToRender);
    }
	// Jeœli nie podano StaticMesh, to generujemy po prostu kwadrat.
    else
    {
        Vertices =
        {
            CreateDynamicMeshVertex(
                FVector3f(0.0f, 0.0f, 0.0f),
                FVector2f(0.0f, 0.0f),
                FColor::Red
            ),

            CreateDynamicMeshVertex(
                FVector3f(200.0f, 0.0f, 0.0f),
                FVector2f(1.0f, 0.0f),
                FColor::Green
            ),

            CreateDynamicMeshVertex(
                FVector3f(200.0f, 0.0f, 200.0f),
                FVector2f(1.0f, 1.0f),
                FColor::Blue
            ),

            CreateDynamicMeshVertex(
                FVector3f(0.0f, 0.0f, 200.0f),
                FVector2f(0.0f, 1.0f),
                FColor::Blue
            )

        };

        // Kolejnoœæ indeksów w buforze wp³ywa na winding - czyli okreœlenie w któr¹ stronê s¹ zdefioniowane wierzcho³ki trójk¹ta, a tym samym okreœlenie która strona to front face a która to back face.
        // Natomiast za normala w sensie renderowania, odpowiada TangentZ - ustawiany w wierzcho³kach ! 
        Indices =
        {
            0,
            1,
            2,
            0,
            2,
            3
        };
    }
}

void FTrianglePrimitiveSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
{
    // Ok - czyli tu przygotowujemy zasoby GPU do render threadu - 
    // 1) - inicjalizujemy buffory - a w trakcie inicja
	// 2) - tworzymy vertex factory - czyli opis jak GPU ma interpretowaæ dane w buforze ( to co robiliœmy wczeœniej w ENQUEUE_RENDER_COMMAND ) 

	check(IsInRenderingThread());
	///// BUFFERS /////////// 

	// Najpierw kopiujemy dane do buforów - ich zasobów CPU np: VertexPosBuffer.CPUData
    CommitDataToBuffers(RHICmdList);


	// Inicjalizujemy buffery - Bufory s¹ inicjalizowane a to co jest w np VertexPosBuffer.CPUData zostaj kopiowane do bufora GPU 
	VertexPosBuffer.InitResource(RHICmdList);
	TangentBuffer.InitResource(RHICmdList);
	TexCoordBuffer.InitResource(RHICmdList);
	IndexBuffer.InitResource(RHICmdList);
   
 
	///// VERTEX FACTORY /////
	// Vertex Factory - czyli opisujemy jak GPU ma interpretowaæ dane w buforze
    FLocalVertexFactory::FDataType Data;

    Data.PositionComponentSRV = VertexPosBuffer.ShaderResourceView;
    Data.TangentsSRV = TangentBuffer.ShaderResourceView;
    Data.TextureCoordinatesSRV = TexCoordBuffer.ShaderResourceView;

    Data.PositionComponent = FVertexStreamComponent(
        &VertexPosBuffer,
		0,  //  <- STRUCT_OFFSET - wskazuje ile bajtów od pocz¹tku struktury nale¿y siê przesun¹æ, aby dostaæ siê do pola Position (w naszym przypadku bêdzie to 0 - moglibyœmy tu wpisaæ 0 z palca )
        sizeof(FVector3f),                   //  <- stride - czyli co ile bajtów jest koleny nowy vertex
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

   /* Data.ColorComponent = FVertexStreamComponent(
            &VertexBuffer,
            STRUCT_OFFSET(FDynamicMeshVertex,Color),
            sizeof(FDynamicMeshVertex),
            VET_Color
    );*/

	Data.NumTexCoords = 1;
	Data.LightMapCoordinateIndex = 0;
    
    VertexFactory.SetData(RHICmdList,Data);
	VertexFactory.InitResource(RHICmdList);
}

void FTrianglePrimitiveSceneProxy::DestroyRenderThreadResources()
{
    check(IsInRenderingThread());

	VertexFactory.ReleaseResource();  // Zwalniamy w kolejnoœci odwrotnej do tworzenia - czyli najpierw zwalniamy vertex factory, a póŸniej buffory

    PositionBufferSRV.SafeRelease();
    TangentBufferSRV.SafeRelease();
    TexCoordsBufferSRV.SafeRelease();

	VertexPosBuffer.ReleaseResource();
	TangentBuffer.ReleaseResource();
	TexCoordBuffer.ReleaseResource();
	IndexBuffer.ReleaseResource();
	
}

void FTrianglePrimitiveSceneProxy::GetVerticesAndIndicesFromStaticMesh(UStaticMesh* InStaticMesh)
{
    for (const FTriangleID& TriangleId : InStaticMesh->GetMeshDescription(0)->Triangles().GetElementIDs())
    {
        TArrayView<FVertexID> OutVertexIDs;
        InStaticMesh->GetMeshDescription(0)->GetTriangleVertices(TriangleId,OutVertexIDs);

        for (const FVertexID& VertexId : OutVertexIDs)
        {
            const FVertexInstanceID VertexInstanceId = InStaticMesh->GetMeshDescription(0)->GetTriangleVertexInstance(TriangleId, VertexId);
            const FVector3f Position = (FVector3f)InStaticMesh->GetMeshDescription(0)->GetVertexPosition(VertexId);
        
            Vertices.Add(CreateDynamicMeshVertex(Position, FVector2f(0.0), FColor::White));

			Indices.Add(VertexId.GetValue());
		}
        
    }
    
	//InStaticMesh->GetMeshDescription(0)->GetPolygonGroupTriangles();
   // InStaticMesh->GetRenderData()->LODResources[0].IndexBuffer;

   // int x = 10;
   // x++;
}

void FTrianglePrimitiveSceneProxy::CommitDataToBuffers(FRHICommandListBase& RHICmdList)
{
	TArray<FVector3f> Positions;
	TArray<FPackedNormal> Tangents;
    TArray<FVector2f> TexCoords;

	// Unpack the vertex data into the arrays and then move to buffers
    for (const FDynamicMeshVertex& Vertex : Vertices)
    {
        Positions.Add(Vertex.Position);
        Tangents.Add(Vertex.TangentX);
        Tangents.Add(Vertex.TangentZ);
        TexCoords.Add(Vertex.TextureCoordinate[0]);
	}

	VertexPosBuffer.CPUData = MoveTemp(Positions);
	TangentBuffer.CPUData = MoveTemp(Tangents);
	TexCoordBuffer.CPUData = MoveTemp(TexCoords);
    IndexBuffer.Indices = Indices;
}

void FTrianglePrimitiveSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
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
		BatchElement.MaxVertexIndex = Vertices.Num() - 1; // Maksymalny indeks wierzcho³ka ( w naszym przypadku 2 )
		BatchElement.NumInstances = 1;                  // Liczba instancji do renderowania ( w naszym przypadku 1 )

        Collector.AddMesh(ViewIndex, Mesh);
    }
}

FPrimitiveViewRelevance FTrianglePrimitiveSceneProxy::GetViewRelevance(const FSceneView* View) const
{
    FPrimitiveViewRelevance Result;

	Result.bDrawRelevance = IsShown(View); // IsShown - sprawdza czy dany FPrimitiveSceneProxy jest widoczny w danym FSceneView

	Result.bDynamicRelevance = true; // dynamiczne elementy s¹ renderowane dla widoku  <- bardzo wa¿ne w naszym przypadku, bo
	// u¿ywamy GetDynamicMeshElements() - czyli dynamicznego renderowania, a nie statycznego ( GetMeshElements() )

	// Generalnie u¿ycie statycznego renderowania ( GetMeshElements() ) jest bardziej wydajne, 
    // bo nie trzeba za ka¿dym razem wywo³ywaæ GetDynamicMeshElements() - jeœli mesh nie zmienia siê czêsto ( np nie animujemy go albo
    // coœ takiego, to warto u¿yæ statycznego renderowania i nie robiæ FMeshBatch za ka¿dym razem

	Result.bShadowRelevance = IsShadowCast(View); // IsShadowCast - sprawdza czy dany FPrimitiveSceneProxy rzuca cieñ w danym FSceneView
	Result.bRenderInMainPass = ShouldRenderInMainPass(); // ShouldRenderInMainPass - sprawdza czy dany FPrimitiveSceneProxy powinien byæ renderowany w g³ównym passie renderowania ( w naszym przypadku tak )

	MaterialRelevance.SetPrimitiveViewRelevance(Result); // Ustawia MaterialRelevance dla danego FPrimitiveViewRelevance

    return Result;
}

bool FTrianglePrimitiveSceneProxy::CanBeOccluded() const
{
    return !MaterialRelevance.bDisableDepthTest;
}

uint32 FTrianglePrimitiveSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + GetAllocatedSize();
}

SIZE_T FTrianglePrimitiveSceneProxy::GetTypeHash() const
{
    static size_t UniquePointer; 

    return reinterpret_cast<size_t>(&UniquePointer);
}

UTrianglePrimitiveComponent::UTrianglePrimitiveComponent()
{
}

FPrimitiveSceneProxy* UTrianglePrimitiveComponent::CreateSceneProxy()
{
    UMaterialInterface* MaterialToUse = Material ? Material : UMaterial::GetDefaultMaterial(MD_Surface);
    return new FTrianglePrimitiveSceneProxy(this, MaterialToUse);
}

FBoxSphereBounds UTrianglePrimitiveComponent::CalcBounds(const FTransform& LocalToWorld) const
{
    const FBox LocalBox(
        FVector(-10.0f, -10.0f, -10.0f), 
        FVector(210.0f, 10.0f, 210.0f)
    );

	return FBoxSphereBounds(LocalBox).TransformBy(LocalToWorld);
}

void UTrianglePrimitiveComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	UMaterialInterface* MaterialToUse = Material ? Material : UMaterial::GetDefaultMaterial(MD_Surface);
	OutMaterials.Add(MaterialToUse);
}

