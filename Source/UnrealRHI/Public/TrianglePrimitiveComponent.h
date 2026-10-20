// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "MeshBuffers.h"

#include "TrianglePrimitiveComponent.generated.h"

// Tu definiujemy klasê dziedzicz¹c¹ po FPrimitiveSceneProxy
class FTrianglePrimitiveSceneProxy : public FPrimitiveSceneProxy
{
public:

	FTrianglePrimitiveSceneProxy(const UTrianglePrimitiveComponent* InComponent, UMaterialInterface* InMaterial,TArray<FDynamicMeshVertex>& InVertices, TArray<int32>& InIndices);
	
	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override;
	virtual void DestroyRenderThreadResources() override;

	void GetVerticesAndIndicesFromStaticMesh(UStaticMesh* InStaticMesh);

	void CommitDataToBuffers(FRHICommandListBase& RHICmdList);

	// Funkcja przekazuje meshe do renderera
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	// FSceneViewFamily - obiekt reprexentuj¹cy rodzinê widoków (FSceneView) renderowanych razem


	// Sprawdza czy dane FPrimitiveSceneProxy jest relevantne - istotne dla danego widoku ( FSceneView ); Generalnie 
	// FSceneView jest to obiekt reprezentuj¹cy widok z kamery na scenê. Z tym ¿e jedna klatka nie zawsze musi oznaczaæ jedn¹
	// kamerê - np w przypadku VR mamy dwie kamery - ka¿da dla jednego oka. Równie¿ w przypadku split-screen mamy 
	// wiêcej ni¿ jedn¹ kamerê.
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	
	// CanBeOccluded - U¿ywa Occlusion Culling - sprawdza czy dany FPrimitiveSceneProxy mo¿e byæ ukryty w przypadku gdy nie jest widoczny z kamery.
	virtual bool CanBeOccluded() const override;
	virtual uint32 GetMemoryFootprint() const override;
	virtual SIZE_T GetTypeHash() const override;

private:
	// CPU Data
	TArray<FDynamicMeshVertex> Vertices;
	TArray<uint32> Indices;

	// GPU Data
	TMeshGenericBuffer<FVector3f> VertexPosBuffer { TEXT("VertexPosBuffer"), PF_R32_FLOAT };
	TMeshGenericBuffer<FPackedNormal> TangentBuffer { TEXT("TangentBuffer"), PF_R8G8B8A8_SNORM };
	TMeshGenericBuffer<FVector2f> TexCoordBuffer { TEXT("TexCoordBuffer"), PF_G32R32F };
	FMeshIndexBuffer  IndexBuffer;

	FShaderResourceViewRHIRef PositionBufferSRV;
	FShaderResourceViewRHIRef TangentBufferSRV;
	FShaderResourceViewRHIRef TexCoordsBufferSRV;
	
	//FShaderResourceViewRHIRef PositionBufferSRV;


	//FVertexPositionBuffer VertexBuffer;
	//FMeshIndexBuffer IndexBuffer;

	int32 NumAllocatedVertices = 0;
	int32 NumTexCoords = 2;

	const FMaterialRenderProxy* MaterialRenderProxy;
	FMaterialRelevance MaterialRelevance;
	FLocalVertexFactory VertexFactory;
};


UCLASS()
class UNREALRHI_API UTrianglePrimitiveComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UTrianglePrimitiveComponent();

	UPROPERTY(EditAnywhere)
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UStaticMesh> StaticMeshToRender;

	// PrimitiveComponent interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;  // CreateSceneProxy - chocia¿ zwraca FPrimitiveSceneProxy* , 
	//to tak na prawdê bêdzie zwracaæ wskaŸnik na FTrianglePrimitiveSceneProxy, który jest klas¹ pochodn¹ FPrimitiveSceneProxy 

	// Po co s¹ boundsy ? - u¿ywane s¹ m.in do przyspeszenia renderowania - szybkiego odrzucenia obiektów - bo s¹ bardzo tanim przybli¿eniem obiektu
 	/*/ Component->Bounds
		|
		+ -- > frustum culling  - "czy pude³ko obejmuj¹ce ten obiekt ma szansê byæ widoczne w kamerze" 
		+ -- > occlusion culling
		+ -- > shadow rendering
		+ -- > visibility
		+ -- > inne systemy renderera*/
	// - dziêki boundsom, b³yskawicznie odrzucamy obiekty któych na pewno nie widaæ w kamerze, nie rzucaj¹ cienia itp
	// - np te¿ jak mamy budynek X a za nim budynek Y, to jeœli X zas³ania ca³e boundsy Y, to nie musimy ju¿ sprawdzaæ geometrii Y - po prostu wiemy ¿e nie musimy go renderowaæ.
	// - Rozmiar bondsów jest bardzo wa¿ny - jeœli s¹ za du¿e, mog¹ byæ renderowane obiekty których na prawdê nie widaæ, jeœli za ma³e, obiekty mog¹ znikaæ mimo ¿e s¹ widoczne
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	// UWAGA !! po zmianie pozycji vertexów na pewno mam z³e Boundsy w SceneProxy - trzeba je przeliczyæ i zaktualizowaæ w FPrimitiveSceneProxy::UpdateBounds() - bo inaczej nie bêdzie poprawnego renderowania
	// - dodaæ funkcje która mo¿e to robi automatycznie ?


	// GetUsedMaterials - zwraca wszystkie materia³y u¿ywane przez dany komponent ( w naszym przypadku tylko jeden )
	// - generalnie nie s³u¿y bezpoœrednio do renderowania, ale zg³aszania u¿ywanych materia³ów do systemu silnika
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const override;

	void SetVerticesAndIndices(TArray<FDynamicMeshVertex>&& InVertices, TArray<int32>&& InIndices);

private:

	TArray<FDynamicMeshVertex> Vertices;
	TArray<int32> Indices;

};
