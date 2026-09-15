/*
   Maintained Unreal AngelScript fork — diagnostic presentation adapters.
*/

#include "frontend/Basic/as_diagnostic_renderer.h"
#include "frontend/Basic/as_diagnostics.h"

#include "Math/UnrealMathUtility.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

BEGIN_AS_NAMESPACE

namespace
	{
		const TCHAR* SeverityName(asEDiagnosticSeverity Severity)
		{
			switch (Severity)
			{
			case asEDiagnosticSeverity::Error: return TEXT("error");
			case asEDiagnosticSeverity::Warning: return TEXT("warning");
			case asEDiagnosticSeverity::Note: return TEXT("note");
			case asEDiagnosticSeverity::Remark: return TEXT("remark");
			default: return TEXT("error");
			}
		}

		FString ExpandTabs(FStringView Text)
		{
			FString Out;
			for (const TCHAR Char : Text)
			{
				if (Char == TEXT('\t'))
				{
					Out += TEXT("    ");
				}
				else
				{
					Out.AppendChar(Char);
				}
			}
			return Out;
		}

		FString DisplayBytes(TConstArrayView<uint8> Bytes)
		{
			FString Out;
			for (int32 Index = 0; Index < Bytes.Num(); )
			{
				const uint8 Byte = Bytes[Index];
				if (Byte == '\t')
				{
					Out += TEXT("    ");
					++Index;
					continue;
				}
				if (Byte == '\r' || Byte == '\n')
				{
					break;
				}
				if (Byte < 0x80)
				{
					Out.AppendChar(static_cast<TCHAR>(Byte));
					++Index;
					continue;
				}
				Out += FString::Printf(TEXT("\\x%02X"), Byte);
				++Index;
			}
			return Out;
		}

		bool LineBounds(TConstArrayView<uint8> FileBytes, asUINT Offset, asUINT& OutBegin, asUINT& OutEnd)
		{
			if (Offset > static_cast<asUINT>(FileBytes.Num()))
			{
				return false;
			}
			asUINT Begin = Offset;
			while (Begin > 0 && FileBytes[Begin - 1] != '\n')
			{
				--Begin;
			}
			asUINT End = Offset;
			while (End < static_cast<asUINT>(FileBytes.Num())
				&& FileBytes[End] != '\n'
				&& FileBytes[End] != '\r')
			{
				++End;
			}
			OutBegin = Begin;
			OutEnd = End;
			return true;
		}

		FString CaretLine(asUINT RangeBegin, asUINT RangeEnd, asUINT LineBegin, asUINT LineEnd)
		{
			const asUINT CaretStart = FMath::Clamp(RangeBegin, LineBegin, LineEnd);
			const asUINT CaretEnd = FMath::Max(CaretStart + 1, FMath::Min(RangeEnd, LineEnd));
			FString Caret;
			for (asUINT Index = LineBegin; Index < CaretStart; ++Index)
			{
				Caret.AppendChar(TEXT(' '));
			}
			Caret.AppendChar(TEXT('^'));
			for (asUINT Index = CaretStart + 1; Index < CaretEnd; ++Index)
			{
				Caret.AppendChar(TEXT('~'));
			}
			return Caret;
		}

		FString FormatRecord(
			const asCDiagnosticResult& Result,
			const asSDiagnosticRecord& Record,
			const TCHAR* KindOverride)
		{
			const asSDiagnosticCatalogDescriptor* Descriptor = asFindDiagnosticCatalog(Record.DiagnosticID);
			const FString Message = asCDiagnosticRenderer::Render(
				Descriptor ? FStringView(Descriptor->Format) : FStringView(TEXT("{0}")),
				Record);
			FString Header;
			if (Record.bHasLocation)
			{
				asSSourceLineColumn LineColumn;
				if (Result.GetSources().TryGetLineColumn(Record.PrimaryRange.Begin, LineColumn))
				{
					Header = FString::Printf(
						TEXT("%s:%u:%u: %s: %s"),
						*Record.StableSourceKey,
						LineColumn.Line,
						LineColumn.Column,
						KindOverride,
						*Message);
				}
				else
				{
					Header = FString::Printf(
						TEXT("%s: %s: %s"),
						*Record.StableSourceKey,
						KindOverride,
						*Message);
				}
			}
			else
			{
				Header = FString::Printf(TEXT("%s: %s"), KindOverride, *Message);
			}

			FString Text = Header;
			if (Record.bHasLocation)
			{
				TConstArrayView<uint8> FileBytes;
				if (Result.GetSources().TryGetBytes(Record.PrimaryRange.Begin.FileID, FileBytes))
				{
					asUINT LineBegin = 0;
					asUINT LineEnd = 0;
					if (LineBounds(FileBytes, Record.PrimaryRange.Begin.ByteOffset, LineBegin, LineEnd))
					{
						Text += TEXT("\n");
						Text += DisplayBytes(FileBytes.Slice(LineBegin, LineEnd - LineBegin));
						Text += TEXT("\n");
						Text += CaretLine(
							Record.PrimaryRange.Begin.ByteOffset,
							Record.PrimaryRange.End.ByteOffset,
							LineBegin,
							LineEnd);
					}
				}
			}
			return Text;
		}

		TSharedRef<FJsonObject> RecordJson(const asSDiagnosticRecord& Record)
		{
			TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetNumberField(TEXT("id"), Record.DiagnosticID);
			if (const asSDiagnosticCatalogDescriptor* Descriptor = asFindDiagnosticCatalog(Record.DiagnosticID))
			{
				Object->SetStringField(TEXT("name"), Descriptor->Name);
			}
			Object->SetStringField(TEXT("severity"), SeverityName(Record.Severity));
			if (Record.bHasLocation)
			{
				TSharedRef<FJsonObject> Location = MakeShared<FJsonObject>();
				Location->SetStringField(TEXT("file"), Record.StableSourceKey);
				Location->SetNumberField(TEXT("begin"), Record.PrimaryRange.Begin.ByteOffset);
				Location->SetNumberField(TEXT("end"), Record.PrimaryRange.End.ByteOffset);
				Object->SetObjectField(TEXT("location"), Location);
			}
			else
			{
				Object->SetField(TEXT("location"), MakeShared<FJsonValueNull>());
			}
			TArray<TSharedPtr<FJsonValue>> Arguments;
			for (const asSDiagnosticArgument& Argument : Record.Arguments)
			{
				TSharedRef<FJsonObject> ArgumentObject = MakeShared<FJsonObject>();
				ArgumentObject->SetNumberField(TEXT("kind"), static_cast<double>(Argument.Kind));
				ArgumentObject->SetStringField(TEXT("text"), Argument.ToDisplayString());
				if (Argument.Kind == asEDiagnosticArgumentKind::Signed)
				{
					ArgumentObject->SetStringField(
						TEXT("signed"),
						FString::Printf(TEXT("%lld"), static_cast<long long>(Argument.SignedValue)));
				}
				Arguments.Add(MakeShared<FJsonValueObject>(ArgumentObject));
			}
			Object->SetArrayField(TEXT("arguments"), Arguments);
			TArray<TSharedPtr<FJsonValue>> Highlights;
			for (const asCSourceRange& Range : Record.RelatedRanges)
			{
				TSharedRef<FJsonObject> Highlight = MakeShared<FJsonObject>();
				Highlight->SetNumberField(TEXT("begin"), Range.Begin.ByteOffset);
				Highlight->SetNumberField(TEXT("end"), Range.End.ByteOffset);
				Highlights.Add(MakeShared<FJsonValueObject>(Highlight));
			}
			Object->SetArrayField(TEXT("highlights"), Highlights);
			return Object;
		}
	}

FString asCDiagnosticRenderer::Render(FStringView Format, const asSDiagnosticRecord& Record)
{
	FString Result(Format);
	for (int32 Index = 0; Index < Record.Arguments.Num(); ++Index)
	{
		const FString Placeholder = FString::Printf(TEXT("{%d}"), Index);
		const FString Value = Record.Arguments[Index].ToDisplayString();
		Result.ReplaceInline(*Placeholder, *Value, ESearchCase::CaseSensitive);
	}
	return Result;
}

FString asCDiagnosticRenderer::SerializeStable(TConstArrayView<asSDiagnosticRecord> Records)
{
	FString Result;
	for (const asSDiagnosticRecord& Record : Records)
	{
		Result += FString::Printf(
			TEXT("%s|%u|%u|%u|%u|%s|%u"),
			*Record.StableSourceKey,
			Record.PrimaryRange.Begin.ByteOffset,
			Record.PrimaryRange.End.ByteOffset,
			static_cast<asUINT>(Record.Severity),
			Record.DiagnosticID,
			*Record.StableFragmentKey,
			Record.LocalSequence);
		for (const asSDiagnosticArgument& Argument : Record.Arguments)
		{
			Result += TEXT("|");
			Result += FString::Printf(TEXT("%u:"), static_cast<asUINT>(Argument.Kind));
			Result += Argument.ToDisplayString();
		}
		Result += TEXT("\n");
	}
		return Result;
	}

	FString asCDiagnosticRenderer::Format(const asCDiagnosticResult& Result, int32 GroupIndex) const
	{
		if (!Result.GetGroups().IsValidIndex(GroupIndex))
		{
			return {};
		}
		const asSDiagnosticGroup& Group = Result.GetGroups()[GroupIndex];
		FString Text = FormatRecord(Result, Group.Primary, SeverityName(Group.Primary.Severity));
		for (const asSDiagnosticRecord& Note : Group.Notes)
		{
			Text += TEXT("\n");
			Text += FormatRecord(Result, Note, TEXT("note"));
		}
		return Text;
	}

	FString asCDiagnosticRenderer::SerializeJson(const asCDiagnosticResult& Result) const
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("version"), TEXT("diagnostic-v1"));
		Root->SetBoolField(TEXT("languageFailure"), Result.HasLanguageFailure());
		Root->SetBoolField(TEXT("truncated"), Result.IsTruncated());
		TArray<TSharedPtr<FJsonValue>> Groups;
		for (const asSDiagnosticGroup& Group : Result.GetGroups())
		{
			TSharedRef<FJsonObject> GroupObject = RecordJson(Group.Primary);
			TArray<TSharedPtr<FJsonValue>> Notes;
			for (const asSDiagnosticRecord& Note : Group.Notes)
			{
				Notes.Add(MakeShared<FJsonValueObject>(RecordJson(Note)));
			}
			GroupObject->SetArrayField(TEXT("notes"), Notes);
			TArray<TSharedPtr<FJsonValue>> Fixes;
			for (const asSDiagnosticFixAlternative& Fix : Group.Fixes)
			{
				TSharedRef<FJsonObject> FixObject = MakeShared<FJsonObject>();
				FixObject->SetStringField(TEXT("title"), Fix.Title);
				TArray<TSharedPtr<FJsonValue>> Edits;
				for (const asSSourceEdit& Edit : Fix.SourceEdits)
				{
					TSharedRef<FJsonObject> EditObject = MakeShared<FJsonObject>();
					EditObject->SetStringField(TEXT("file"), Edit.LogicalSourceKey);
					EditObject->SetStringField(
						TEXT("revision"),
						FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(Edit.ExpectedRevision)));
					EditObject->SetNumberField(TEXT("begin"), Edit.Range.Begin.ByteOffset);
					EditObject->SetNumberField(TEXT("end"), Edit.Range.End.ByteOffset);
					FString Replacement;
					for (const uint8 Byte : Edit.ReplacementUtf8)
					{
						Replacement += FString::Printf(TEXT("%02X"), Byte);
					}
					EditObject->SetStringField(TEXT("replacement"), Replacement);
					Edits.Add(MakeShared<FJsonValueObject>(EditObject));
				}
				FixObject->SetArrayField(TEXT("edits"), Edits);
				Fixes.Add(MakeShared<FJsonValueObject>(FixObject));
			}
			GroupObject->SetArrayField(TEXT("fixes"), Fixes);
			Groups.Add(MakeShared<FJsonValueObject>(GroupObject));
		}
		Root->SetArrayField(TEXT("groups"), Groups);
		FString Output;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
		FJsonSerializer::Serialize(Root, Writer);
		return Output;
	}

END_AS_NAMESPACE
