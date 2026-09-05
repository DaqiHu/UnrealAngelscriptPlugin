/* Maintained Unreal AngelScript fork — authored type spelling and ranges. */
#pragma once

#include "frontend/as_type.h"

BEGIN_AS_NAMESPACE

class ANGELSCRIPTRUNTIME_API asCTypeLoc
	{
	public:
		asCTypeLoc(asCQualType Type, FString Spelling, asCSourceRange Range)
			: Type(Type), Spelling(MoveTemp(Spelling)), Range(MoveTemp(Range)) {}
		const asCQualType& GetQualType() const { return Type; }
		const FString& GetSpelling() const { return Spelling; }
		const asCSourceRange& GetSourceRange() const { return Range; }
		void AddComponentRange(asCSourceRange Component) { ComponentRanges.Add(MoveTemp(Component)); }
		TConstArrayView<asCSourceRange> GetComponentRanges() const { return MakeArrayView(ComponentRanges); }
	private:
		asCQualType Type;
		FString Spelling;
		asCSourceRange Range;
		TArray<asCSourceRange> ComponentRanges;
	};

	class ANGELSCRIPTRUNTIME_API asCTypeSourceInfo
	{
	public:
		explicit asCTypeSourceInfo(asCTypeLoc InLoc) : Loc(MoveTemp(InLoc)) {}
		const asCTypeLoc& GetTypeLoc() const { return Loc; }
	private:
		asCTypeLoc Loc;
	};

END_AS_NAMESPACE
