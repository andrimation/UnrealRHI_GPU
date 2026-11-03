#include "OSMImporter/OSMImporter.h"

#include "XmlFile.h"
#include "XmlNode.h"

namespace
{
    struct FLatLon
    {
        double Lat = 0.0;
        double Lon = 0.0;
    };

    // WGS84 latitude/longitude -> UE local coordinates (cm).
    FVector ToUnrealPosition(
        const FLatLon& Position,
        const FLatLon& Origin)
    {
        constexpr double EarthRadius = 6378137.0;

        const double Lat0 = FMath::DegreesToRadians(Origin.Lat);
        const double DeltaLat = FMath::DegreesToRadians(
            Position.Lat - Origin.Lat);

        const double DeltaLon = FMath::DegreesToRadians(
            Position.Lon - Origin.Lon);

        const double X = EarthRadius * DeltaLon * FMath::Cos(Lat0);
        const double Y = EarthRadius * DeltaLat;

        // Metres -> centimetres
        return FVector(X * 100.0, Y * 100.0, 0.0);
    }
}

bool FOSMImporter::LoadFromFile(
    const FString& FilePath,
    FOSMData& OutData)
{
    OutData = FOSMData{};

    FXmlFile Xml(FilePath, EConstructMethod::ConstructFromFile);

    if (!Xml.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("OSM XML error: %s"),
            *Xml.GetLastError());
        return false;
    }

    const FXmlNode* Root = Xml.GetRootNode();

    if (!Root || Root->GetTag() != TEXT("osm"))
    {
        return false;
    }

    // Step 1: Read nodes
    TMap<int64, FLatLon> Nodes;

    FLatLon Min;
    FLatLon Max;
    bool bHasNodes = false;

    for (const FXmlNode* Element : Root->GetChildrenNodes())
    {
        if (Element->GetTag() != TEXT("node"))
        {
            continue;
        }

        const int64 Id = FCString::Atoi64(
            *Element->GetAttribute(TEXT("id")));

        const double Lat = FCString::Atod(
            *Element->GetAttribute(TEXT("lat")));

        const double Lon = FCString::Atod(
            *Element->GetAttribute(TEXT("lon")));

        Nodes.Add(Id, FLatLon{ Lat, Lon });

        if (!bHasNodes)
        {
            Min = Max = FLatLon{ Lat, Lon };
            bHasNodes = true;
        }
        else
        {
            Min.Lat = FMath::Min(Min.Lat, Lat);
            Min.Lon = FMath::Min(Min.Lon, Lon);
            Max.Lat = FMath::Max(Max.Lat, Lat);
            Max.Lon = FMath::Max(Max.Lon, Lon);
        }
    }

    if (!bHasNodes)
    {
        return false;
    }

    const FLatLon Origin{
        (Min.Lat + Max.Lat) * 0.5,
        (Min.Lon + Max.Lon) * 0.5
    };

    // Step 2: Read ways
    for (const FXmlNode* Element : Root->GetChildrenNodes())
    {
        if (Element->GetTag() != TEXT("way"))
        {
            continue;
        }

        TArray<int64> NodeIds;
        FString BuildingType;
        FString HighwayType;

        for (const FXmlNode* Child : Element->GetChildrenNodes())
        {
            if (Child->GetTag() == TEXT("nd"))
            {
                NodeIds.Add(FCString::Atoi64(
                    *Child->GetAttribute(TEXT("ref"))));
            }
            else if (Child->GetTag() == TEXT("tag"))
            {
                const FString Key = Child->GetAttribute(TEXT("k"));
                const FString Value = Child->GetAttribute(TEXT("v"));

                if (Key == TEXT("building"))
                {
                    BuildingType = Value;
                }
                else if (Key == TEXT("highway"))
                {
                    HighwayType = Value;
                }
            }
        }

        const bool bBuilding =
            !BuildingType.IsEmpty() && BuildingType != TEXT("no");

        const bool bRoad =
            !HighwayType.IsEmpty() && HighwayType != TEXT("no");

        if ((!bBuilding && !bRoad) || NodeIds.Num() < 2)
        {
            continue;
        }

        const bool bClosed = NodeIds[0] == NodeIds.Last();

        FOSMFeature Feature;
        Feature.Id = FCString::Atoi64(
            *Element->GetAttribute(TEXT("id")));
        Feature.bClosed = bClosed;

        // Do not duplicate the final vertex of closed ways.
        const int32 Count = NodeIds.Num() - (bClosed ? 1 : 0);
        Feature.Points.Reserve(Count);

        bool bComplete = true;

        for (int32 i = 0; i < Count; ++i)
        {
            const FLatLon* Position = Nodes.Find(NodeIds[i]);

            if (!Position)
            {
                bComplete = false;
                break;
            }

            Feature.Points.Add(
                ToUnrealPosition(*Position, Origin));
        }

        // Skip incomplete ways.
        if (!bComplete)
        {
            continue;
        }

        if (bBuilding && bClosed && Feature.Points.Num() >= 3)
        {
            FOSMFeature Building = Feature;
            Building.Type = BuildingType;

            OutData.Buildings.Add(MoveTemp(Building));
        }

        if (bRoad && Feature.Points.Num() >= 2)
        {
            Feature.Type = HighwayType;
            OutData.Roads.Add(MoveTemp(Feature));
        }
    }

    UE_LOG(LogTemp, Log,
        TEXT("OSM: %d buildings, %d roads"),
        OutData.Buildings.Num(),
        OutData.Roads.Num());

    return true;
}