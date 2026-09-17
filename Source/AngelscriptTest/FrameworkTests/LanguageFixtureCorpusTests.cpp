#include "CQTest.h"
#include "Framework/Catalog/AngelscriptTestCode.h"
#include "Framework/Source/AngelscriptTestSource.h"

#include "GenericPlatform/GenericPlatformOutputDevices.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
void AppendAscii(TArray<uint8>& Out, const ANSICHAR* Text)
{
	while (*Text != '\0')
	{
		Out.Add(static_cast<uint8>(*Text++));
	}
}

void AppendUtf8(TArray<uint8>& Out, FStringView Text)
{
	const FTCHARToUTF8 Converted(Text.GetData(), Text.Len());
	const uint8* Bytes = reinterpret_cast<const uint8*>(Converted.Get());
	Out.Append(Bytes, Converted.Length());
}

void AppendJsonString(TArray<uint8>& Out, FStringView Text)
{
	Out.Add(static_cast<uint8>('"'));
	const FTCHARToUTF8 Converted(Text.GetData(), Text.Len());
	const uint8* Bytes = reinterpret_cast<const uint8*>(Converted.Get());
	for (int32 Index = 0; Index < Converted.Length(); ++Index)
	{
		const uint8 Byte = Bytes[Index];
		if (Byte == '"' || Byte == '\\')
		{
			Out.Add(static_cast<uint8>('\\'));
			Out.Add(Byte);
		}
		else if (Byte == '\n')
		{
			AppendAscii(Out, "\\n");
		}
		else if (Byte == '\r')
		{
			AppendAscii(Out, "\\r");
		}
		else if (Byte == '\t')
		{
			AppendAscii(Out, "\\t");
		}
		else if (Byte < 0x20)
		{
			AppendAscii(Out, "\\u00");
			constexpr ANSICHAR Hex[] = "0123456789abcdef";
			Out.Add(static_cast<uint8>(Hex[(Byte >> 4) & 0xF]));
			Out.Add(static_cast<uint8>(Hex[Byte & 0xF]));
		}
		else
		{
			Out.Add(Byte);
		}
	}
	Out.Add(static_cast<uint8>('"'));
}

void AppendJsonInt(TArray<uint8>& Out, int32 Value)
{
	AppendUtf8(Out, FString::FromInt(Value));
}

void AppendTopicArray(TArray<uint8>& Out, const TArray<FString>& Topics)
{
	Out.Add(static_cast<uint8>('['));
	for (int32 Index = 0; Index < Topics.Num(); ++Index)
	{
		if (Index > 0)
		{
			Out.Add(static_cast<uint8>(','));
		}
		AppendJsonString(Out, Topics[Index]);
	}
	Out.Add(static_cast<uint8>(']'));
}

struct FNamedOffset
{
	FString Name;
	int32 Offset = 0;
};

struct FNamedRange
{
	FString Name;
	int32 Begin = 0;
	int32 End = 0;
};

void CollectAnnotations(
	const AngelscriptTest::FAngelscriptTestAnnotations& Annotations,
	TArray<FNamedOffset>& OutPoints,
	TArray<FNamedOffset>& OutBreakpoints,
	TArray<FNamedRange>& OutRanges)
{
	static const TCHAR* PointNames[] = {TEXT("initial-value"), TEXT("member"), TEXT("downcast")};
	static const TCHAR* BreakpointNames[] = {TEXT("before-add"), TEXT("before-call")};
	static const TCHAR* RangeNames[] = {TEXT("delta"), TEXT("null-guard")};
	for (const TCHAR* Name : PointNames)
	{
		if (const int32* Offset = Annotations.GetPoint(Name))
		{
			OutPoints.Add({FString(Name), *Offset});
		}
	}
	for (const TCHAR* Name : BreakpointNames)
	{
		if (const int32* Offset = Annotations.GetBreakpoint(Name))
		{
			OutBreakpoints.Add({FString(Name), *Offset});
		}
	}
	for (const TCHAR* Name : RangeNames)
	{
		if (const auto* Range = Annotations.GetRange(Name))
		{
			OutRanges.Add({FString(Name), Range->Begin, Range->End});
		}
	}
	OutPoints.Sort([](const FNamedOffset& Left, const FNamedOffset& Right)
	{
		return Left.Name.Compare(Right.Name, ESearchCase::CaseSensitive) < 0;
	});
	OutBreakpoints.Sort([](const FNamedOffset& Left, const FNamedOffset& Right)
	{
		return Left.Name.Compare(Right.Name, ESearchCase::CaseSensitive) < 0;
	});
	OutRanges.Sort([](const FNamedRange& Left, const FNamedRange& Right)
	{
		return Left.Name.Compare(Right.Name, ESearchCase::CaseSensitive) < 0;
	});
}

void AppendOffsetObject(TArray<uint8>& Out, const TArray<FNamedOffset>& Items)
{
	Out.Add(static_cast<uint8>('{'));
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		if (Index > 0)
		{
			Out.Add(static_cast<uint8>(','));
		}
		AppendJsonString(Out, Items[Index].Name);
		Out.Add(static_cast<uint8>(':'));
		AppendJsonInt(Out, Items[Index].Offset);
	}
	Out.Add(static_cast<uint8>('}'));
}

void AppendRangeObject(TArray<uint8>& Out, const TArray<FNamedRange>& Items)
{
	Out.Add(static_cast<uint8>('{'));
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		if (Index > 0)
		{
			Out.Add(static_cast<uint8>(','));
		}
		AppendJsonString(Out, Items[Index].Name);
		AppendAscii(Out, ":[");
		AppendJsonInt(Out, Items[Index].Begin);
		Out.Add(static_cast<uint8>(','));
		AppendJsonInt(Out, Items[Index].End);
		Out.Add(static_cast<uint8>(']'));
	}
	Out.Add(static_cast<uint8>('}'));
}

FString ResolvedActualPath()
{
	const FString LogFile(FGenericPlatformOutputDevices::GetAbsoluteLogFilename());
	if (LogFile.IsEmpty())
	{
		return FString();
	}
	const FString Directory = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::GetPath(LogFile), TEXT("HandwrittenCorpus")));
	return FPaths::Combine(Directory, TEXT("Actual.as"));
}

bool WriteActualDump(const TArray<uint8>& Bytes, FString& OutError)
{
	const FString Target = ResolvedActualPath();
	if (Target.IsEmpty() || Target.Contains(TEXT("..")))
	{
		OutError = TEXT("handwritten dump path is unavailable");
		return false;
	}
	const FString Parent = FPaths::GetPath(Target);
	if (!IFileManager::Get().DirectoryExists(*Parent)
		&& !IFileManager::Get().MakeDirectory(*Parent, true))
	{
		OutError = FString::Printf(TEXT("could not create HandwrittenCorpus: %s"), *Parent);
		return false;
	}
	if (!FFileHelper::SaveArrayToFile(Bytes, *Target))
	{
		OutError = FString::Printf(TEXT("failed to write Actual.as: %s"), *Target);
		return false;
	}
	return true;
}
}

TEST_CLASS_WITH_FLAGS(
	LanguageFixtureCorpus,
	"Angelscript.UnitTest.Framework",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(DumpsAllAuthoredSources)
	{
		auto& Code = AngelscriptTest::FAngelscriptTestCode::GetInstance();
		ASSERT_THAT(IsTrue(Code.IsActivated()));
		ASSERT_THAT(AreEqual(0, Code.GetRegistrationErrors().Num()));
		ASSERT_THAT(IsFalse(Code.Get(TEXT("Language/Counter"), TEXT("root")).IsSuccess()));

		TArray<AngelscriptTest::FAngelscriptTestFileMeta> Files;
		const TArray<FString> LanguageTopic = {FString(TEXT("Language"))};
		const auto FileStatus = Code.FindFiles(LanguageTopic, Files);
		ASSERT_THAT(IsTrue(FileStatus.IsSuccess()));
		if (!FileStatus.IsSuccess())
		{
			return;
		}

		struct FDumpEntry
		{
			FString FileTag;
			FString VersionTag;
			AngelscriptTest::FAngelscriptTestSourceCase Case;
		};
		TArray<FDumpEntry> Entries;
		for (const AngelscriptTest::FAngelscriptTestFileMeta& File : Files)
		{
			TArray<AngelscriptTest::FAngelscriptTestSourceCase> Cases;
			const auto CaseStatus = Code.FindCases(File.Tag, TArray<FString>(), Cases);
			ASSERT_THAT(IsTrue(CaseStatus.IsSuccess()));
			if (!CaseStatus.IsSuccess())
			{
				return;
			}
			for (const AngelscriptTest::FAngelscriptTestSourceCase& Case : Cases)
			{
				Entries.Add({File.Tag, Case.GetVersionMeta().Tag, Case});
			}
		}
		Entries.Sort([](const FDumpEntry& Left, const FDumpEntry& Right)
		{
			const int32 FileOrder = Left.FileTag.Compare(Right.FileTag, ESearchCase::CaseSensitive);
			if (FileOrder != 0)
			{
				return FileOrder < 0;
			}
			return Left.VersionTag.Compare(Right.VersionTag, ESearchCase::CaseSensitive) < 0;
		});

		TArray<uint8> Dump;
		int32 FoundAnnotations = 0;
		for (const FDumpEntry& Entry : Entries)
		{
			const AngelscriptTest::FAngelscriptTestSource Source = Entry.Case.GetSource();
			const TConstArrayView<uint8> Bytes = Source.GetBytes();
			const auto& FileMeta = Entry.Case.GetFileMeta();
			const auto& VersionMeta = Entry.Case.GetVersionMeta();
			TArray<FNamedOffset> Points;
			TArray<FNamedOffset> Breakpoints;
			TArray<FNamedRange> Ranges;
			CollectAnnotations(Source.GetAnnotations(), Points, Breakpoints, Ranges);
			FoundAnnotations += Points.Num() + Breakpoints.Num() + Ranges.Num();
			ASSERT_THAT(AreEqual(
				Source.GetAnnotations().Num(),
				Points.Num() + Breakpoints.Num() + Ranges.Num()));

			Dump.Append(reinterpret_cast<const uint8*>("// "), 3);
			Dump.Add(static_cast<uint8>('{'));
			AppendAscii(Dump, "\"file_tag\":");
			AppendJsonString(Dump, FileMeta.Tag);
			AppendAscii(Dump, ",\"version_tag\":");
			AppendJsonString(Dump, VersionMeta.Tag);
			AppendAscii(Dump, ",\"file_version\":");
			AppendJsonString(Dump, FileMeta.Version);
			AppendAscii(Dump, ",\"file_summary\":");
			AppendJsonString(Dump, FileMeta.Summary);
			AppendAscii(Dump, ",\"file_topics\":");
			AppendTopicArray(Dump, FileMeta.Topics);
			AppendAscii(Dump, ",\"parent\":");
			if (VersionMeta.Parent.IsSet())
			{
				AppendJsonString(Dump, VersionMeta.Parent.GetValue());
			}
			else
			{
				AppendAscii(Dump, "null");
			}
			AppendAscii(Dump, ",\"version_summary\":");
			AppendJsonString(Dump, VersionMeta.Summary);
			AppendAscii(Dump, ",\"version_topics\":");
			AppendTopicArray(Dump, VersionMeta.Topics);
			AppendAscii(Dump, ",\"points\":");
			AppendOffsetObject(Dump, Points);
			AppendAscii(Dump, ",\"breakpoints\":");
			AppendOffsetObject(Dump, Breakpoints);
			AppendAscii(Dump, ",\"ranges\":");
			AppendRangeObject(Dump, Ranges);
			AppendAscii(Dump, ",\"byte_length\":");
			AppendJsonInt(Dump, Bytes.Num());
			Dump.Add(static_cast<uint8>('}'));
			Dump.Add(static_cast<uint8>('\n'));
			Dump.Append(Bytes.GetData(), Bytes.Num());
			Dump.Add(static_cast<uint8>('\n'));
		}

		FString WriteError;
		ASSERT_THAT(IsTrue(WriteActualDump(Dump, WriteError)));
		if (!WriteError.IsEmpty())
		{
			ASSERT_THAT(IsTrue(false, *WriteError));
		}
		ASSERT_THAT(IsTrue(FoundAnnotations >= 3));
	}

	TEST_METHOD(CorpusHasClassHandleCast)
	{
		auto& Code = AngelscriptTest::FAngelscriptTestCode::GetInstance();
		TArray<AngelscriptTest::FAngelscriptTestFileMeta> Files;
		const TArray<FString> LanguageTopic = {FString(TEXT("Language"))};
		const auto FileStatus = Code.FindFiles(LanguageTopic, Files);
		ASSERT_THAT(IsTrue(FileStatus.IsSuccess()));
		if (!FileStatus.IsSuccess())
		{
			return;
		}

		const bool bHasClassHandleCast = Files.ContainsByPredicate(
			[](const AngelscriptTest::FAngelscriptTestFileMeta& File)
			{
				return File.Tag == TEXT("Language/Casting/ClassHandleCast");
			});
		const bool bHasClassCast = Files.ContainsByPredicate(
			[](const AngelscriptTest::FAngelscriptTestFileMeta& File)
			{
				return File.Tag == TEXT("Language/Casting/ClassCast");
			});
		ASSERT_THAT(IsTrue(bHasClassHandleCast));
		ASSERT_THAT(IsFalse(bHasClassCast));
	}
};

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
