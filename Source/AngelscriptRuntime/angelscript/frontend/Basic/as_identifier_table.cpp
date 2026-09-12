/*
   Maintained Unreal AngelScript fork — session-local identifier interning.
*/

#include "frontend/Basic/as_identifier_table.h"

BEGIN_AS_NAMESPACE

namespace
	{
		bool EqualsAnsi(TConstArrayView<uint8> Bytes, const ANSICHAR* Text)
		{
			const int32 Length = FCStringAnsi::Strlen(Text);
			return Bytes.Num() == Length
				&& (Length == 0 || FMemory::Memcmp(Bytes.GetData(), Text, Length) == 0);
		}
	}

	const asCIdentifierInfo* asCIdentifierTable::Intern(TConstArrayView<uint8> Utf8Spelling)
	{
		if (Utf8Spelling.IsEmpty())
		{
			return nullptr;
		}

		const uint64 Hash = HashSpelling(Utf8Spelling);
		if (const TArray<asCIdentifierInfo*>* Bucket = EntriesByHash.Find(Hash))
		{
			for (const asCIdentifierInfo* Existing : *Bucket)
			{
				const TConstArrayView<uint8> ExistingSpelling = Existing->GetSpelling();
				if (ExistingSpelling.Num() == Utf8Spelling.Num()
					&& FMemory::Memcmp(
						ExistingSpelling.GetData(),
						Utf8Spelling.GetData(),
						Utf8Spelling.Num()) == 0)
				{
					return Existing;
				}
			}
		}

		TUniquePtr<asCIdentifierInfo> NewEntry = MakeUnique<asCIdentifierInfo>();
		NewEntry->Spelling.Append(Utf8Spelling.GetData(), Utf8Spelling.Num());
		NewEntry->TokenKind = ClassifyTokenKind(Utf8Spelling);
		NewEntry->ReflectionKind = ClassifyReflectionSpelling(Utf8Spelling);
		asCIdentifierInfo* StableEntry = NewEntry.Get();
		Entries.Add(MoveTemp(NewEntry));
		EntriesByHash.FindOrAdd(Hash).Add(StableEntry);
		++UniqueEntryAllocationCount;
		OwnedSpellingByteCount += static_cast<asUINT>(Utf8Spelling.Num());
		return StableEntry;
	}

	uint64 asCIdentifierTable::HashSpelling(TConstArrayView<uint8> Utf8Spelling)
	{
		uint64 Hash = 14695981039346656037ull;
		for (const uint8 Byte : Utf8Spelling)
		{
			Hash ^= Byte;
			Hash *= 1099511628211ull;
		}
		return Hash;
	}

	asETokenKind asCIdentifierTable::ClassifyTokenKind(TConstArrayView<uint8> Utf8Spelling)
	{
		for (asUINT Value = 0; Value < static_cast<asUINT>(asETokenKind::Count); ++Value)
		{
			const asETokenKind Kind = static_cast<asETokenKind>(Value);
			if (asIsKeyword(Kind) && EqualsAnsi(Utf8Spelling, asGetTokenSpellingAnsi(Kind)))
			{
				return Kind;
			}
		}
		return asETokenKind::Identifier;
	}

	asEReflectionSpelling asCIdentifierTable::ClassifyReflectionSpelling(
		TConstArrayView<uint8> Utf8Spelling)
	{
		if (EqualsAnsi(Utf8Spelling, "UCLASS")) { return asEReflectionSpelling::UClass; }
		if (EqualsAnsi(Utf8Spelling, "USTRUCT")) { return asEReflectionSpelling::UStruct; }
		if (EqualsAnsi(Utf8Spelling, "UENUM")) { return asEReflectionSpelling::UEnum; }
		if (EqualsAnsi(Utf8Spelling, "UFUNCTION")) { return asEReflectionSpelling::UFunction; }
		if (EqualsAnsi(Utf8Spelling, "UPROPERTY")) { return asEReflectionSpelling::UProperty; }
		if (EqualsAnsi(Utf8Spelling, "UMETA")) { return asEReflectionSpelling::UMeta; }
		return asEReflectionSpelling::None;
	}

END_AS_NAMESPACE
