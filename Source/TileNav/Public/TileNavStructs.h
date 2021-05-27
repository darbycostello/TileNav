#pragma once
#include "TileNavStructs.generated.h"

USTRUCT(BlueprintType)
struct FEdgeNeighbors {
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	TArray<int32> NavSections;
	
	FEdgeNeighbors() {}
	
	FEdgeNeighbors(const TArray<int32> Sections) {
		NavSections = Sections;
	}
};

FORCEINLINE FArchive& operator<<(FArchive& Ar, FEdgeNeighbors& EdgeNeighbors) {
	Ar << EdgeNeighbors.NavSections;
	return Ar;
}

USTRUCT(BlueprintType)
struct TILENAV_API FTileNavSection {
	GENERATED_BODY()
	friend class UTileNavComponent;

	UPROPERTY(BlueprintReadWrite)
	int32 SectionIndex;
	
	UPROPERTY(BlueprintReadWrite)
	TArray<int32> Neighbors;

	FTileNavSection(): SectionIndex(-1) {}
	FTileNavSection(const int32 InSectionIndex) {
		SectionIndex = InSectionIndex;
	}
};

FORCEINLINE FArchive& operator<<(FArchive& Ar, FTileNavSection& TileNavSection) {
	Ar << TileNavSection.SectionIndex;
	Ar << TileNavSection.Neighbors;
	return Ar;
}

USTRUCT(BlueprintType)
struct TILENAV_API FTileTraceVertex {
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FVector WorldLocation;
	
	UPROPERTY(BlueprintReadWrite)
	bool bCollision;

	FTileTraceVertex(): bCollision(false) { WorldLocation = FVector::ZeroVector; }

	FTileTraceVertex(const FVector WorldLocation, const bool bCollision) {
		this->WorldLocation = WorldLocation;
		this->bCollision = bCollision;
	}
};

USTRUCT(BlueprintType)
struct TILENAV_API FTileNavTile {
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	bool bWalkable;
	
	UPROPERTY(BlueprintReadWrite)
	TArray<int32> Neighbors;

	FTileNavTile(): bWalkable(false) {}

	FTileNavTile(const bool bWalkable, const TArray<int32> Neighbors) {
		this->bWalkable = bWalkable;
		this->Neighbors = Neighbors;
	}
};

FORCEINLINE FArchive& operator<<(FArchive& Ar, FTileNavTile& TileNavTile) {
	Ar << TileNavTile.bWalkable;
	Ar << TileNavTile.Neighbors;
	return Ar;
}

USTRUCT(BlueprintType)
struct TILENAV_API FPathTile {
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	int32 Index;

	UPROPERTY(BlueprintReadWrite)
	int32 Parent;
	
	UPROPERTY(BlueprintReadWrite)
	float G;

	UPROPERTY(BlueprintReadWrite)
	float H;

	UPROPERTY(BlueprintReadWrite)
	float F;

	explicit FPathTile(): Index(-1), Parent(-1), G(0), H(0), F(0) {
	}

	explicit FPathTile(const int32 Index): Parent(-1), G(0), H(0), F(0) {
		this->Index = Index;
	}

	bool operator==(const FPathTile & Other) const {
		return Other.Index == Index;
	}
};