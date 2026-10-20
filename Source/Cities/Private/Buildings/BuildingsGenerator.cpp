#include "Buildings/BuildingsGenerator.h"

FGeneratedMesh FBuildingsGenerator::GenerateBuildingsWallMesh(const TArray<FOSMFeature>& InBuildingsFootprints, float Height)
{
	FGeneratedMesh Mesh;

	// Pre Alokowaæ vertices i indeksy
	int32 AllFootprintsPointsNum = 0;
	for (const FOSMFeature& Building : InBuildingsFootprints)
	{
		AllFootprintsPointsNum += Building.Points.Num();
	}

	Mesh.DynamicVertices.Reserve(AllFootprintsPointsNum * 2); // Dwa poziomy: ground i roof
	Mesh.Indices.Reserve(AllFootprintsPointsNum * 6);  // Ka¿dy segment krawêdzi budynku tworzy dwa trójk¹ty (6 indeksów)

	// Generate vertices and indices for each building footprint

	FVector2f UV_0_0 = FVector2f(0.0f, 0.0f);
	FVector2f UV_0_1 = FVector2f(0.0f, 1.0f);
	FVector2f UV_1_0 = FVector2f(1.0f, 0.0f);
	FVector2f UV_1_1 = FVector2f(1.0f, 1.0f);
	
	for (const FOSMFeature& Building : InBuildingsFootprints)
	{
		for (int index = 0; index < Building.Points.Num(); index++)
		{
			const FVector& FirstPoint = Building.Points[index];
			const FVector& SecondPoint = Building.Points[(index + 1) % Building.Points.Num()]; // Wrap around to the first point ( czyli zamiast robiæ if, wystarczy zrobiæ modulo )

			// Create vertices for the wall segment
			FDynamicMeshVertex Vertex_one = CreateDynamicMeshVertex
			(
				FVector3f(FirstPoint.X, FirstPoint.Y, FirstPoint.Z),
				UV_0_0,
				FColor::Red
			);

			FDynamicMeshVertex Vertex_two = CreateDynamicMeshVertex
			(
				FVector3f(FirstPoint.X, FirstPoint.Y, FirstPoint.Z + Height),
				UV_0_1,
				FColor::Red
			);

			FDynamicMeshVertex Vertex_three = CreateDynamicMeshVertex
			(
				FVector3f(SecondPoint.X, SecondPoint.Y, SecondPoint.Z),
				UV_1_0,
				FColor::Red
			);

			FDynamicMeshVertex Vertex_four = CreateDynamicMeshVertex
			(
				FVector3f(SecondPoint.X, SecondPoint.Y, SecondPoint.Z + Height),
				UV_1_1,
				FColor::Red
			);

			int32 BaseIndex = Mesh.DynamicVertices.Num();
			Mesh.DynamicVertices.Add(MoveTemp(Vertex_one));
			Mesh.DynamicVertices.Add(MoveTemp(Vertex_two));
			Mesh.DynamicVertices.Add(MoveTemp(Vertex_three));
			Mesh.DynamicVertices.Add(MoveTemp(Vertex_four));

			// First triangle
			Mesh.Indices.Add(BaseIndex);
			Mesh.Indices.Add(BaseIndex + 1);
			Mesh.Indices.Add(BaseIndex + 2);

			// Second triangle  -> Tu mo¿e byæ te¿ kwestia kolejnoœci vertexów
			Mesh.Indices.Add(BaseIndex + 2);
			Mesh.Indices.Add(BaseIndex + 1);
			Mesh.Indices.Add(BaseIndex + 3);
			
			
		}	
	}
	return Mesh;
}

FGeneratedMesh FBuildingsGenerator::GenerateBuildingsRoofMesh(const TArray<FOSMFeature>& InBuildingsFootprints, float Height)
{
    FGeneratedMesh Mesh;

    // Pre Alokowaæ vertices i indeksy
    int32 AllFootprintsPointsNum = 0;
    int32 AllFootprintsRoofsIndices = 0;
    for (const FOSMFeature& Building : InBuildingsFootprints)
    {
        AllFootprintsPointsNum += Building.Points.Num();
		AllFootprintsRoofsIndices += (Building.Points.Num() - 2) * 3; // Ka¿dy budynek tworzy (N-2) trójk¹tów, ka¿dy trójk¹t ma 3 indeksy
    }

	TArray<int32> NextRoof;
    Mesh.DynamicVertices.Reserve(AllFootprintsPointsNum);
    Mesh.Vertices.Reserve(AllFootprintsPointsNum);

    Mesh.Indices.Reserve(AllFootprintsRoofsIndices);

    FVector2f UV_0_0 = FVector2f(0.0f, 0.0f);
   // FVector2f UV_0_1 = FVector2f(0.0f, 1.0f);
   // FVector2f UV_1_0 = FVector2f(1.0f, 0.0f);
   // FVector2f UV_1_1 = FVector2f(1.0f, 1.0f);

    int32 NextRoofIndex = 0;
    for (const FOSMFeature& Building : InBuildingsFootprints)
    {     
        for (const FVector& Point : Building.Points)
        {
            FDynamicMeshVertex Vertex_roof = CreateDynamicMeshVertex
            (
                FVector3f(Point.X, Point.Y, Point.Z + Height),
                UV_0_0,
                FColor::Red
            );

            Vertex_roof.SetTangents(
                FVector3f(1.0f, 0.0f, 0.0f),
                FVector3f(0.0f, 1.0f, 0.0f),
                FVector3f(0.0f, 0.0f, 1.0f)
            );

			NextRoofIndex++;
			Mesh.DynamicVertices.Add(MoveTemp(Vertex_roof));

            Mesh.Vertices.Add(FVector(Point.X, Point.Y, Point.Z + Height));
        }
		NextRoof.Add(NextRoofIndex);
    }

	TriangulateEarClipping(Mesh.Vertices,NextRoof, Mesh.Indices);

	return Mesh;
}

bool FBuildingsGenerator::TriangulateEarClipping(const TArray<FVector>& Points,const TArray<int32>& InNextRoofIndex,TArray<int32>& OutIndices)
{
    constexpr double PositionEpsilon = 0.01;
    constexpr double AreaEpsilon = 1e-8;

    if (Points.IsEmpty() || InNextRoofIndex.IsEmpty())
    {
        return false;
    }

    // Validate roof ranges before modifying OutIndices.
    int32 PreviousEnd = 0;
    int32 MaxRoofPoints = 0;

    for (const int32 End : InNextRoofIndex)
    {
        if (End <= PreviousEnd || End > Points.Num())
        {
            return false;
        }

        MaxRoofPoints = FMath::Max(
            MaxRoofPoints,
            End - PreviousEnd
        );

        PreviousEnd = End;
    }

    if (PreviousEnd != Points.Num())
    {
        return false;
    }

    auto To2D = [&](int32 Index) -> FVector2d
        {
            return FVector2d(
                Points[Index].X,
                Points[Index].Y
            );
        };

    auto Cross = [](
        const FVector2d& A,
        const FVector2d& B,
        const FVector2d& C) -> double
        {
            return (B.X - A.X) * (C.Y - A.Y)
                - (B.Y - A.Y) * (C.X - A.X);
        };

    const double PositionEpsilonSq =
        PositionEpsilon * PositionEpsilon;

    // Scratch buffers shared between ALL roofs.
    TArray<int32> Order;
    TArray<int32> Prev;
    TArray<int32> Next;
    TArray<uint8> Alive;
    TArray<uint8> Ear;

    Order.Reserve(MaxRoofPoints);
    Prev.Reserve(MaxRoofPoints);
    Next.Reserve(MaxRoofPoints);
    Alive.Reserve(MaxRoofPoints);
    Ear.Reserve(MaxRoofPoints);

    bool bAllRoofsSucceeded = true;

    int32 Start = 0;

    // ==================================
    // Process each roof separately
    // ==================================

    for (int32 RoofIndex = 0;
        RoofIndex < InNextRoofIndex.Num();
        ++RoofIndex)
    {
        const int32 End = InNextRoofIndex[RoofIndex];
        const int32 RoofIndexStart = OutIndices.Num();

        // [Start, End) contains one footprint.
        Order.Reset();

        // ==================================
        // 1. Build order using GLOBAL indices
        // ==================================

        for (int32 i = Start; i < End; ++i)
        {
            if (Order.IsEmpty() ||
                (To2D(i) - To2D(Order.Last())).SizeSquared()
        > PositionEpsilonSq)
            {
                Order.Add(i);
            }
        }

        // Remove duplicated closing point.
        if (Order.Num() > 1 &&
            (To2D(Order[0]) - To2D(Order.Last())).SizeSquared()
            <= PositionEpsilonSq)
        {
            Order.RemoveAt(Order.Num() - 1);
        }

        // Set the next roof's starting offset.
        Start = End;

        if (Order.Num() < 3)
        {
            bAllRoofsSucceeded = false;
            continue;
        }

        // ==================================
        // 2. Remove collinear points
        // ==================================

        bool bRemovedPoint = false;

        do
        {
            bRemovedPoint = false;

            for (int32 i = 0;
                i < Order.Num() && Order.Num() > 3;
                ++i)
            {
                const int32 Count = Order.Num();

                const FVector2d A =
                    To2D(Order[(i + Count - 1) % Count]);

                const FVector2d B =
                    To2D(Order[i]);

                const FVector2d C =
                    To2D(Order[(i + 1) % Count]);

                const FVector2d AC = C - A;

                if (AC.SizeSquared() > PositionEpsilonSq &&
                    FMath::Abs(Cross(A, B, C)) <=
                    PositionEpsilon * AC.Size() &&
                    FVector2d::DotProduct(
                        B - A, C - B) >= 0.0)
                {
                    Order.RemoveAt(i);
                    bRemovedPoint = true;
                    break;
                }
            }
        } while (bRemovedPoint);

        const int32 N = Order.Num();

        // ==================================
        // 3. Calculate winding
        // ==================================

        double SignedArea2 = 0.0;

        const FVector2d Origin = To2D(Order[0]);

        for (int32 i = 0; i < N; ++i)
        {
            SignedArea2 += Cross(
                Origin,
                To2D(Order[i]),
                To2D(Order[(i + 1) % N])
            );
        }

        if (!FMath::IsFinite(SignedArea2) ||
            FMath::Abs(SignedArea2) <= AreaEpsilon)
        {
            bAllRoofsSucceeded = false;
            continue;
        }

        // Normalize to CCW.
        if (SignedArea2 < 0.0)
        {
            for (int32 i = 0, j = N - 1;
                i < j;
                ++i, --j)
            {
                Swap(Order[i], Order[j]);
            }
        }

        // ==================================
        // 4. Initialize linked polygon
        // ==================================

        Prev.SetNumUninitialized(N);
        Next.SetNumUninitialized(N);
        Alive.SetNumUninitialized(N);
        Ear.SetNumUninitialized(N);

        for (int32 i = 0; i < N; ++i)
        {
            Prev[i] = (i + N - 1) % N;
            Next[i] = (i + 1) % N;

            Alive[i] = 1;
            Ear[i] = 0;
        }

        // ==================================
        // 5. Detect ears
        // ==================================

        auto IsEar = [&](int32 i) -> bool
            {
                if (!Alive[i])
                {
                    return false;
                }

                const int32 AIndex = Prev[i];
                const int32 CIndex = Next[i];

                const FVector2d A = To2D(Order[AIndex]);
                const FVector2d B = To2D(Order[i]);
                const FVector2d C = To2D(Order[CIndex]);

                // Vertex must be convex.
                if (Cross(A, B, C) <= AreaEpsilon)
                {
                    return false;
                }

                // Check remaining active vertices.
                for (int32 j = 0; j < N; ++j)
                {
                    if (!Alive[j] ||
                        j == AIndex ||
                        j == i ||
                        j == CIndex)
                    {
                        continue;
                    }

                    const FVector2d P = To2D(Order[j]);

                    const double C0 = Cross(A, B, P);
                    const double C1 = Cross(B, C, P);
                    const double C2 = Cross(C, A, P);

                    if (C0 >= -AreaEpsilon &&
                        C1 >= -AreaEpsilon &&
                        C2 >= -AreaEpsilon)
                    {
                        return false;
                    }
                }

                return true;
            };

        for (int32 i = 0; i < N; ++i)
        {
            Ear[i] = IsEar(i) ? 1 : 0;
        }

        // ==================================
        // 6. Clip ears
        // ==================================

        int32 Remaining = N;
        int32 Current = 0;

        bool bRoofSucceeded = true;

        while (Remaining > 3)
        {
            int32 EarIndex = INDEX_NONE;

            for (int32 Step = 0; Step < N; ++Step)
            {
                const int32 i = (Current + Step) % N;

                if (Alive[i] && Ear[i])
                {
                    EarIndex = i;
                    break;
                }
            }

            if (EarIndex == INDEX_NONE)
            {
                bRoofSucceeded = false;
                break;
            }

            const int32 A = Prev[EarIndex];
            const int32 B = EarIndex;
            const int32 C = Next[EarIndex];

            // Order contains GLOBAL vertex indices.
            // No additional offset is needed!
            /*
            OutIndices.Add(Order[A]);
            OutIndices.Add(Order[B]);
            OutIndices.Add(Order[C]);
            */
			// Zmieniamy winding wierzcho³ków, aby by³y zgodne z CCW (Counter-Clockwise)
            OutIndices.Add(Order[A]);
            OutIndices.Add(Order[C]);
            OutIndices.Add(Order[B]);

            // Remove ear tip.
            Alive[B] = 0;
            Ear[B] = 0;

            Next[A] = C;
            Prev[C] = A;

            --Remaining;

            // Only adjacent vertices need updating.
            Ear[A] = IsEar(A) ? 1 : 0;
            Ear[C] = IsEar(C) ? 1 : 0;

            Current = C;
        }

        // ==================================
        // 7. Append final triangle
        // ==================================

        if (bRoofSucceeded)
        {
            int32 LastVertex = INDEX_NONE;

            for (int32 i = 0; i < N; ++i)
            {
                if (Alive[i])
                {
                    LastVertex = i;
                    break;
                }
            }

            if (LastVertex == INDEX_NONE)
            {
                bRoofSucceeded = false;
            }
            else
            {
                const int32 A = LastVertex;
                const int32 B = Next[A];
                const int32 C = Next[B];

                if (Cross(
                    To2D(Order[A]),
                    To2D(Order[B]),
                    To2D(Order[C])) <= AreaEpsilon)
                {
                    bRoofSucceeded = false;
                }
                else
                {
                    /*
                    OutIndices.Add(Order[A]);
                    OutIndices.Add(Order[B]);
                    OutIndices.Add(Order[C]);
                    */

					// Zmieniamy winding wierzcho³ków, aby by³y zgodne z
                    OutIndices.Add(Order[A]);
                    OutIndices.Add(Order[C]);
                    OutIndices.Add(Order[B]);
                }
            }
        }

        // ==================================
        // 8. Verify this roof
        // ==================================

        const int32 AddedIndices =
            OutIndices.Num() - RoofIndexStart;

        if (!bRoofSucceeded ||
            AddedIndices != (N - 2) * 3)
        {
            // Roll back ONLY this roof's indices.
            OutIndices.SetNum(RoofIndexStart, EAllowShrinking::No);

            bAllRoofsSucceeded = false;

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Ear clipping failed for roof %d"),
                RoofIndex
            );
        }
    }

    return bAllRoofsSucceeded;
}


