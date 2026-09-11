/*
   Maintained Unreal AngelScript fork — session-local identifier interning.
*/

#pragma once

#include "frontend/as_token.h"

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Templates/UniquePtr.h"

BEGIN_AS_NAMESPACE

enum class asEReflectionSpelling : uint8
	{
		None,
		UClass,
		UStruct,
		UEnum,
		UDelegate,
		UFunction,
		UProperty,
		UMeta,
	};

	class ANGELSCRIPTRUNTIME_API asCIdentifierInfo
	{
	public:
		TConstArrayView<uint8> GetSpelling() const { return MakeArrayView(Spelling); }
		asETokenKind GetTokenKind() const { return TokenKind; }
		asEReflectionSpelling GetReflectionKind() const { return ReflectionKind; }

	private:
		friend class asCIdentifierTable;
		TArray<uint8> Spelling;
		asETokenKind TokenKind = asETokenKind::Identifier;
		asEReflectionSpelling ReflectionKind = asEReflectionSpelling::None;
	};

	class ANGELSCRIPTRUNTIME_API asCIdentifierTable
	{
	public:
		asCIdentifierTable() = default;
		~asCIdentifierTable() = default;

		asCIdentifierTable(const asCIdentifierTable&) = delete;
		asCIdentifierTable& operator=(const asCIdentifierTable&) = delete;

		const asCIdentifierInfo* Intern(TConstArrayView<uint8> Utf8Spelling);
		asUINT GetEntryCount() const { return static_cast<asUINT>(Entries.Num()); }
		asUINT GetUniqueEntryAllocationCount() const { return UniqueEntryAllocationCount; }
		asUINT GetOwnedSpellingByteCount() const { return OwnedSpellingByteCount; }

	private:
		static uint64 HashSpelling(TConstArrayView<uint8> Utf8Spelling);
		static asETokenKind ClassifyTokenKind(TConstArrayView<uint8> Utf8Spelling);
		static asEReflectionSpelling ClassifyReflectionSpelling(TConstArrayView<uint8> Utf8Spelling);

		TArray<TUniquePtr<asCIdentifierInfo>> Entries;
		TMap<uint64, TArray<asCIdentifierInfo*>> EntriesByHash;
		asUINT UniqueEntryAllocationCount = 0;
		asUINT OwnedSpellingByteCount = 0;
	};

END_AS_NAMESPACE
