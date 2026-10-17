#include "SeaNavigationMap.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformTime.h"
#include <queue>
#include <vector>

TSharedPtr<const FSeaNavigationMap> FSeaNavigationMap::Load(const FString& Filename, FString& Error)
{
    // All callers are game-thread Blueprint updates. Weak cache does not retain unused worlds.
    static TMap<FString, TWeakPtr<const FSeaNavigationMap>> Cache;
    if (const auto* Entry = Cache.Find(Filename)) if (auto P = Entry->Pin()) return P;
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *Filename) || Text.Len() > 8*1024*1024)
    { Error = TEXT("Navigation dataset missing or exceeds 8 MiB"); return nullptr; }
    TSharedPtr<FJsonObject> J;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid())
    { Error = TEXT("Navigation dataset is not valid JSON"); return nullptr; }
    FString Format; double Version=0, W=0,H=0,C=0;
    if (!J->TryGetStringField(TEXT("format"),Format) || Format!=TEXT("VDT_SEA_NAV") ||
        !J->TryGetNumberField(TEXT("version"),Version) || Version!=1 ||
        !J->TryGetNumberField(TEXT("width"),W) || !J->TryGetNumberField(TEXT("height"),H) ||
        W<3 || H<3 || W>1024 || H>1024 || W!=FMath::FloorToDouble(W) || H!=FMath::FloorToDouble(H) ||
        !J->TryGetNumberField(TEXT("cell_size"),C) || !FMath::IsFinite(C) || C<1 || C>1000)
    { Error=TEXT("Unsupported or invalid navigation grid header"); return nullptr; }
    auto M=MakeShared<FSeaNavigationMap>(); M->Width=int32(W);M->Height=int32(H);M->CellSize=C;
    auto Number=[&](const TCHAR* Key,double& Out){return J->TryGetNumberField(Key,Out) && FMath::IsFinite(Out);};
    auto Vector=[&](const TCHAR* Key,FVector& Out)
    {
        const TArray<TSharedPtr<FJsonValue>>* V=nullptr;
        if (!J->TryGetArrayField(Key,V) || V->Num()!=3) return false;
        Out=FVector((*V)[0]->AsNumber(),(*V)[1]->AsNumber(),(*V)[2]->AsNumber()); return !Out.ContainsNaN();
    };
    FString CellBytes,ClearBytes,CRS;
    if (!Number(TEXT("min_east"),M->MinEast) || !Number(TEXT("min_north"),M->MinNorth) ||
        !Number(TEXT("safety_margin"),M->SafetyMargin) || !Number(TEXT("max_vessel_radius_m"),M->MaxVesselRadius) ||
        !Vector(TEXT("origin_llh"),M->OriginLLH) || !Vector(TEXT("origin_ecef"),M->OriginECEF) ||
        !Vector(TEXT("east_axis"),M->East) || !Vector(TEXT("north_axis"),M->North) ||
        !J->TryGetStringField(TEXT("coordinate_system"),CRS) || CRS!=TEXT("WGS84_ECEF_FIXED_ENU_METERS") ||
        !J->TryGetStringField(TEXT("cells_base64"),CellBytes) || !FBase64::Decode(CellBytes,M->Cells) ||
        !J->TryGetStringField(TEXT("clearance_base64"),ClearBytes) || !FBase64::Decode(ClearBytes,M->Clearance) ||
        M->Cells.Num()!=M->Width*M->Height || M->Clearance.Num()!=M->Cells.Num() ||
        !FMath::IsNearlyEqual(M->East.SizeSquared(),1.,1.e-8) ||
        !FMath::IsNearlyEqual(M->North.SizeSquared(),1.,1.e-8) ||
        FMath::Abs(FVector::DotProduct(M->East,M->North))>1.e-8 || M->SafetyMargin<M->MaxVesselRadius)
    { Error=TEXT("Invalid coordinate convention, grid payload, or safety margin"); return nullptr; }
    for (int32 Id=0;Id<M->Cells.Num();++Id)
        if (M->Cells[Id]>1 || ((Id%M->Width==0 || Id%M->Width==M->Width-1 || Id<M->Width || Id>=M->Cells.Num()-M->Width) && M->Cells[Id]!=0))
        { Error=TEXT("Invalid cells or unblocked coverage boundary"); return nullptr; }
    // Shared once-per-load connectivity prevents behaviors choosing an unreachable water pocket.
    M->ConnectedRegions.Init(0,M->Cells.Num());int32 Region=0;TArray<int32> Queue;
    for(int32 Seed=0;Seed<M->Cells.Num();++Seed)
    {
        if(!M->Cells[Seed] || M->ConnectedRegions[Seed])continue;
        ++Region;Queue.Reset();Queue.Add(Seed);M->ConnectedRegions[Seed]=Region;
        for(int32 Head=0;Head<Queue.Num();++Head)
        {
            const int32 Id=Queue[Head],X=Id%M->Width,Y=Id/M->Width;
            for(int32 DY=-1;DY<=1;++DY)for(int32 DX=-1;DX<=1;++DX)
            {
                const int32 NX=X+DX,NY=Y+DY;
                if((DX==0 && DY==0) || !M->IsWater(NX,NY) || (DX && DY && (!M->IsWater(X+DX,Y) || !M->IsWater(X,Y+DY))))continue;
                const int32 Next=NY*M->Width+NX;if(!M->ConnectedRegions[Next]){M->ConnectedRegions[Next]=Region;Queue.Add(Next);}
            }
        }
    }
    TSharedPtr<const FSeaNavigationMap> Immutable=M;
    Cache.Add(Filename,Immutable); return Immutable;
}

bool FSeaNavigationMap::IsWater(int32 X,int32 Y) const
{return X>=0 && Y>=0 && X<Width && Y<Height && Cells.IsValidIndex(Y*Width+X) && Cells[Y*Width+X]==1;}
int32 FSeaNavigationMap::CellAt(const FVector2D& P) const
{
    if (P.ContainsNaN()) return INDEX_NONE;
    const double GX=(P.X-MinEast)/CellSize,GY=(P.Y-MinNorth)/CellSize;
    if (GX<0 || GY<0 || GX>=Width || GY>=Height) return INDEX_NONE;
    return FMath::FloorToInt(GY)*Width+FMath::FloorToInt(GX);
}
bool FSeaNavigationMap::IsWater(const FVector2D& P) const
{const int32 I=CellAt(P);return I!=INDEX_NONE && Cells[I]==1;}
bool FSeaNavigationMap::SameWaterRegion(const FVector2D& A,const FVector2D& B) const
{const int32 I=CellAt(A),J=CellAt(B);return IsWater(A) && IsWater(B) && ConnectedRegions.IsValidIndex(I) && ConnectedRegions.IsValidIndex(J) && ConnectedRegions[I]==ConnectedRegions[J];}
FVector2D FSeaNavigationMap::Center(int32 I) const
{return FVector2D(MinEast+(I%Width+0.5)*CellSize,MinNorth+(I/Width+0.5)*CellSize);}
FVector2D FSeaNavigationMap::ToMetric(const FVector& P) const
{const FVector D=P-OriginECEF;return FVector2D(FVector::DotProduct(D,East),FVector::DotProduct(D,North));}
FVector FSeaNavigationMap::ToECEF(const FVector2D& P) const
{return OriginECEF+East*P.X+North*P.Y;}

bool FSeaNavigationMap::SegmentClear(const FVector2D& A,const FVector2D& B) const
{
    if (!IsWater(A) || !IsWater(B)) return false;
    const FVector2D GA=(A-FVector2D(MinEast,MinNorth))/CellSize;
    const FVector2D GB=(B-FVector2D(MinEast,MinNorth))/CellSize;
    const FVector2D D=GB-GA;
    int32 X=FMath::FloorToInt(GA.X),Y=FMath::FloorToInt(GA.Y);
    const int32 EX=FMath::FloorToInt(GB.X),EY=FMath::FloorToInt(GB.Y);
    const int32 SX=D.X>0?1:-1,SY=D.Y>0?1:-1;
    const double TX=FMath::Abs(D.X)>1.e-12?1./FMath::Abs(D.X):DBL_MAX;
    const double TY=FMath::Abs(D.Y)>1.e-12?1./FMath::Abs(D.Y):DBL_MAX;
    double NX=TX==DBL_MAX?DBL_MAX:(SX>0?X+1-GA.X:GA.X-X)*TX;
    double NY=TY==DBL_MAX?DBL_MAX:(SY>0?Y+1-GA.Y:GA.Y-Y)*TY;
    // A line along a cell boundary touches BOTH cells; do not permit corner cuts.
    auto Check=[&](int32 CX,int32 CY)
    {
        if (!IsWater(CX,CY)) return false;
        if (FMath::Abs(D.X)<1.e-12 && FMath::IsNearlyEqual(GA.X,FMath::RoundToDouble(GA.X),1.e-9) && !IsWater(CX-1,CY)) return false;
        if (FMath::Abs(D.Y)<1.e-12 && FMath::IsNearlyEqual(GA.Y,FMath::RoundToDouble(GA.Y),1.e-9) && !IsWater(CX,CY-1)) return false;
        return true;
    };
    for(int32 Guard=0;Guard<Width+Height+4;++Guard)
    {
        if (!Check(X,Y)) return false;
        if(X==EX && Y==EY) return true;
        if(FMath::Abs(NX-NY)<1.e-10)
        {if(!Check(X+SX,Y) || !Check(X,Y+SY))return false;X+=SX;Y+=SY;NX+=TX;NY+=TY;}
        else if(NX<NY){X+=SX;NX+=TX;}else{Y+=SY;NY+=TY;}
    }
    return false;
}

FSeaRouteResult FSeaNavigationMap::FindPath(const FVector2D& Start,const FVector2D& Goal,
    int32 MaxExpansions,double MaxMilliseconds,const TArray<FVector2D>& Exclusions) const
{
    FSeaRouteResult R;const double Beg=FPlatformTime::Seconds();
    auto Fail=[&](const TCHAR* S){R.Failure=S;R.Milliseconds=(FPlatformTime::Seconds()-Beg)*1000.;return R;};
    const int32 S=CellAt(Start),Gid=CellAt(Goal);
    if(!IsWater(Start) || !IsWater(Goal))return Fail(TEXT("Start or destination is blocked or outside coverage"));
    auto Excluded=[&](const FVector2D& P){for(const auto& E:Exclusions)if(FVector2D::DistSquared(P,E)<FMath::Square(CellSize*1.5))return true;return false;};
    if(Excluded(Goal))return Fail(TEXT("Destination lies in a temporarily failed corridor"));
    if(!SegmentClear(Start,Center(S)) || !SegmentClear(Center(Gid),Goal))return Fail(TEXT("Unsafe endpoint connector"));
    TArray<double> Costs;Costs.Init(DBL_MAX,Cells.Num());Costs[S]=0;
    TArray<int32> Parents;Parents.Init(INDEX_NONE,Cells.Num());
    TBitArray<> Closed(false,Cells.Num());
    struct FOpen{double F,G;int32 Id,Turns;bool operator<(const FOpen& O)const{return F==O.F?Turns>O.Turns:F>O.F;}};
    std::priority_queue<FOpen,std::vector<FOpen>> Open;
    Open.push({FVector2D::Distance(Start,Goal),0,S,0});
    const int32 DX[8]={1,1,0,-1,-1,-1,0,1},DY[8]={0,1,1,1,0,-1,-1,-1};
    while(!Open.empty())
    {
        FOpen Cur=Open.top();Open.pop();if(Closed[Cur.Id] || Cur.G>Costs[Cur.Id])continue;
        if(++R.Expansions>MaxExpansions)return Fail(TEXT("A* expansion limit reached"));
        if((R.Expansions&63)==0 && (FPlatformTime::Seconds()-Beg)*1000.>MaxMilliseconds)return Fail(TEXT("A* time limit reached"));
        if(Cur.Id==Gid)
        {
            TArray<FVector2D> Reverse;
            for(int32 I=Gid;I!=INDEX_NONE;I=Parents[I]){Reverse.Add(Center(I));if(Reverse.Num()>Cells.Num())return Fail(TEXT("Route reconstruction guard"));}
            TArray<FVector2D> Raw;Raw.Add(Start);for(int32 I=Reverse.Num()-1;I>=0;--I)Raw.Add(Reverse[I]);Raw.Add(Goal);
            // Bounded line-of-sight string pulling, every shortcut uses the SAME conservative grid.
            R.Points.Add(Start);int32 Anchor=0;
            auto Shortcut=[&](const FVector2D& A,const FVector2D& B)
            {if(!SegmentClear(A,B))return false;for(const auto& E:Exclusions){const FVector2D D=B-A;const double T=FMath::Clamp(FVector2D::DotProduct(E-A,D)/FMath::Max(D.SizeSquared(),1.e-9),0.,1.);if(FVector2D::DistSquared(E,A+D*T)<FMath::Square(CellSize*1.5))return false;}return true;};
            while(Anchor<Raw.Num()-1)
            {
                int32 Next=Anchor+1;
                for(int32 I=FMath::Min(Anchor+64,Raw.Num()-1);I>Anchor+1;--I)if(Shortcut(Raw[Anchor],Raw[I])){Next=I;break;}
                if(!Shortcut(Raw[Anchor],Raw[Next]))return Fail(TEXT("Smoothed route validation failed"));
                R.Points.Add(Raw[Next]);Anchor=Next;
            }
            R.bSuccess=true;R.Milliseconds=(FPlatformTime::Seconds()-Beg)*1000.;return R;
        }
        Closed[Cur.Id]=true;const int32 X=Cur.Id%Width,Y=Cur.Id/Width;
        for(int32 D=0;D<8;++D)
        {
            const int32 NX=X+DX[D],NY=Y+DY[D];if(!IsWater(NX,NY))continue;
            const int32 Id=NY*Width+NX;if(Closed[Id] || Excluded(Center(Id)))continue;
            if(DX[D] && DY[D] && (!IsWater(X+DX[D],Y) || !IsWater(X,Y+DY[D])))continue;
            const double Step=CellSize*(DX[D] && DY[D]?UE_SQRT_2:1.);
            const double NewCost=Cur.G+Step*(1.+0.15/(1.+Clearance[Id]));
            if(NewCost<Costs[Id])
            {
                int32 Turns=Cur.Turns;
                if(Parents[Cur.Id]!=INDEX_NONE && (X-Parents[Cur.Id]%Width!=DX[D] || Y-Parents[Cur.Id]/Width!=DY[D]))++Turns;
                Parents[Id]=Cur.Id;Costs[Id]=NewCost;Open.push({NewCost+FVector2D::Distance(Center(Id),Center(Gid)),NewCost,Id,Turns});
                if(Open.size()>200000)return Fail(TEXT("A* open-set memory limit reached"));
            }
        }
    }
    return Fail(TEXT("No connected water route"));
}
