/* Maintained Unreal AngelScript fork — source-owned, unresolved type syntax. */
#pragma once

#include "frontend/Basic/as_source_location.h"
#include "Containers/Array.h"

BEGIN_AS_NAMESPACE

enum class asETypeReferenceDirection : uint8 { None, In, Out, InOut };

	// Parsed type components are authoritative input to resolution. Spelling is
	// only an observation of those components, never source fed back to a parser.
	struct asSTypeSyntax
	{
		TArray<FString> NameParts;
		TArray<asCSourceRange> ComponentRanges;
		TArray<asSTypeSyntax> GenericArguments;
		TArray<asCSourceRange> ArrayDimensions;
		asCSourceRange Range;
		bool bGlobal = false;
		bool bConst = false;
		bool bWildcard = false;
		bool bHandle = false;
		asETypeReferenceDirection Reference = asETypeReferenceDirection::None;

		bool IsValid() const { return (bWildcard || !NameParts.IsEmpty()) && Range.IsOrdered(); }
		FString GetQualifiedName() const { return FString::Join(NameParts, TEXT("::")); }
		FString GetSpelling() const
		{
			FString Result = bConst ? TEXT("const ") : TEXT("");
			if (bGlobal) Result += TEXT("::");
			Result += bWildcard ? TEXT("?") : GetQualifiedName();
			if (!GenericArguments.IsEmpty())
			{
				Result += TEXT("<");
				for (int32 Index = 0; Index < GenericArguments.Num(); ++Index)
				{
					if (Index) Result += TEXT(",");
					Result += GenericArguments[Index].GetSpelling();
				}
				Result += TEXT(">");
			}
			for (int32 Index = 0; Index < ArrayDimensions.Num(); ++Index) Result += TEXT("[]");
			if (bHandle) Result += TEXT("@");
			if (Reference == asETypeReferenceDirection::In) Result += TEXT("&in");
			else if (Reference == asETypeReferenceDirection::Out) Result += TEXT("&out");
			else if (Reference == asETypeReferenceDirection::InOut) Result += TEXT("&inout");
			return Result;
		}
	};

END_AS_NAMESPACE
