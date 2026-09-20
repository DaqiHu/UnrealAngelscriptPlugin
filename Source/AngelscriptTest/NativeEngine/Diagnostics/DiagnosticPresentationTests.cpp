#include "CQTest.h"
#include "NativeEngine/NativeEngineTestSupport.h"

#include "frontend/Basic/as_diagnostics.h"
#include "frontend/Basic/as_source_position_codec.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	DiagnosticsPresentation,
	"Angelscript.UnitTest.NativeEngine.Diagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(IndependentPresentationOracle)
	{
		AS_NAMESPACE_QUALIFIER asCSourceFileID FileID;
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager =
			AngelscriptNativeEngineTest::MakeSourceManager(
				TEXT("A.as"),
				TEXT("int Count=1; Cout;"),
				&FileID);
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Engine(Manager);
		AS_NAMESPACE_QUALIFIER asCDiagnosticFragment Fragment = Engine.CreateFragment(TEXT("sema"));
		AS_NAMESPACE_QUALIFIER asSDiagnosticGroup Group;
		Group.Primary.DiagnosticID = static_cast<asUINT>(
			AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnresolvedReference);
		Group.Primary.PrimaryRange = Manager.MakeRange(FileID, 13, 17);
		Group.Primary.Arguments.Add(AS_NAMESPACE_QUALIFIER asSDiagnosticArgument::FromText(TEXT("Cout")));
		AS_NAMESPACE_QUALIFIER asSDiagnosticRecord Note;
		Note.DiagnosticID = static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::DidYouMean);
		Note.PrimaryRange = Manager.MakeRange(FileID, 4, 9);
		Note.Arguments.Add(AS_NAMESPACE_QUALIFIER asSDiagnosticArgument::FromText(TEXT("Count")));
		Group.Notes.Add(Note);
		uint64 Revision = 0;
		ASSERT_THAT(IsTrue(Manager.TryGetContentRevision(FileID, Revision)));
		AS_NAMESPACE_QUALIFIER asSDiagnosticFixAlternative First;
		First.Title = TEXT("Count");
		AS_NAMESPACE_QUALIFIER asSSourceEdit Replace;
		Replace.LogicalSourceKey = TEXT("A.as");
		Replace.ExpectedRevision = 9007199254740993ull;
		Replace.Range = Manager.MakeRange(FileID, 13, 17);
		Replace.ReplacementUtf8 = { 'C', 'o', 'u', 'n', 't' };
		First.SourceEdits.Add(Replace);
		AS_NAMESPACE_QUALIFIER asSDiagnosticFixAlternative Second;
		Second.Title = TEXT("semicolon");
		AS_NAMESPACE_QUALIFIER asSSourceEdit Semi;
		Semi.LogicalSourceKey = TEXT("A.as");
		Semi.ExpectedRevision = 9007199254740993ull;
		Semi.Range = Manager.MakeRange(FileID, 18, 18);
		Second.SourceEdits.Add(Semi);
		Group.Fixes.Add(First);
		Group.Fixes.Add(Second);
		ASSERT_THAT(IsTrue(Fragment.ReportGroup(MoveTemp(Group))));
		ASSERT_THAT(IsTrue(Engine.Submit(MoveTemp(Fragment))));

		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> Result =
			Engine.CaptureResult();
		const AS_NAMESPACE_QUALIFIER asCDiagnosticRenderer Renderer;
		const FString Text = Renderer.Format(*Result, 0);
		const FString Expected = FString(TEXT(
			"A.as:1:14: error: unresolved reference 'Cout'\n"
			"int Count=1; Cout;\n"
			"             ^~~~\n"
			"A.as:1:5: note: did you mean 'Count'?\n"
			"int Count=1; Cout;\n"
			"    ^~~~~"));
		ASSERT_THAT(AreEqual(Expected, Text));

		const FString Json = Renderer.SerializeJson(*Result);
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		ASSERT_THAT(IsTrue(FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()));
		ASSERT_THAT(AreEqual(FString(TEXT("diagnostic-v1")), Root->GetStringField(TEXT("version"))));
		const TArray<TSharedPtr<FJsonValue>>* Groups = nullptr;
		ASSERT_THAT(IsTrue(Root->TryGetArrayField(TEXT("groups"), Groups) && Groups && Groups->Num() == 1));
		const TSharedPtr<FJsonObject> GroupObject = (*Groups)[0]->AsObject();
		ASSERT_THAT(IsTrue(GroupObject.IsValid()));
		ASSERT_THAT(IsTrue(GroupObject->GetNumberField(TEXT("id")) == 4003.0));
		ASSERT_THAT(AreEqual(FString(TEXT("Cout")), GroupObject->GetArrayField(TEXT("arguments"))[0]->AsObject()->GetStringField(TEXT("text"))));
		ASSERT_THAT(AreEqual(1, GroupObject->GetArrayField(TEXT("notes")).Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("Count")), GroupObject->GetArrayField(TEXT("notes"))[0]->AsObject()->GetArrayField(TEXT("arguments"))[0]->AsObject()->GetStringField(TEXT("text"))));
		const TArray<TSharedPtr<FJsonValue>> Fixes = GroupObject->GetArrayField(TEXT("fixes"));
		ASSERT_THAT(AreEqual(2, Fixes.Num()));
		ASSERT_THAT(AreEqual(
			FString(TEXT("9007199254740993")),
			Fixes[0]->AsObject()->GetArrayField(TEXT("edits"))[0]->AsObject()->GetStringField(TEXT("revision"))));
		ASSERT_THAT(AreEqual(
			FString(TEXT("436F756E74")),
			Fixes[0]->AsObject()->GetArrayField(TEXT("edits"))[0]->AsObject()->GetStringField(TEXT("replacement"))));
		(void)Revision;
	}

	TEST_METHOD(AcceptedPresentationBoundaries)
	{
		const uint8 Bytes[] = {
			0x41, 0xE4, 0xB8, 0xAD, 0xF0, 0x9F, 0x98, 0x80, 0x0D, 0x0A, 0x5A};
		AS_NAMESPACE_QUALIFIER asCSourceFileID FileID;
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager =
			AngelscriptNativeEngineTest::MakeSourceManagerFromBytes(
				TEXT("U.as"),
				MakeArrayView(Bytes),
				&FileID);
		const AS_NAMESPACE_QUALIFIER asCSourcePositionCodec Codec(Manager);
		int32 Line = 0;
		int32 Column = 0;
		ASSERT_THAT(IsTrue(Codec.ByteToPosition(Manager.MakeLocation(FileID, 8), false, Line, Column)));
		ASSERT_THAT(AreEqual(0, Line));
		ASSERT_THAT(AreEqual(8, Column));
		ASSERT_THAT(IsTrue(Codec.ByteToPosition(Manager.MakeLocation(FileID, 8), true, Line, Column)));
		ASSERT_THAT(AreEqual(0, Line));
		ASSERT_THAT(AreEqual(4, Column));
		ASSERT_THAT(IsTrue(Codec.ByteToPosition(Manager.MakeLocation(FileID, 10), true, Line, Column)));
		ASSERT_THAT(AreEqual(1, Line));
		ASSERT_THAT(AreEqual(0, Column));
		ASSERT_THAT(IsTrue(Codec.ByteToPosition(Manager.MakeLocation(FileID, 11), true, Line, Column)));
		ASSERT_THAT(AreEqual(1, Line));
		ASSERT_THAT(AreEqual(1, Column));
		ASSERT_THAT(IsFalse(Codec.ByteToPosition(Manager.MakeLocation(FileID, 9), true, Line, Column)));
		AS_NAMESPACE_QUALIFIER asCSourceLocation Rejected;
		ASSERT_THAT(IsFalse(Codec.PositionToByte(FileID, 0, 3, true, Rejected)));

		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Engine(Manager);
		AS_NAMESPACE_QUALIFIER asCDiagnosticFragment Fragment = Engine.CreateFragment(TEXT("driver"));
		Fragment.DiagUnlocated(static_cast<asUINT>(
			AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::InputAdmission))
			<< FStringView(TEXT("source-less"));
		Engine.Submit(MoveTemp(Fragment));
		const AS_NAMESPACE_QUALIFIER asCDiagnosticRenderer Renderer;
		const FString Text = Renderer.Format(*Engine.CaptureResult(), 0);
		ASSERT_THAT(IsTrue(Text.StartsWith(TEXT("error:"))));
		ASSERT_THAT(IsFalse(Text.Contains(TEXT(":1:"))));
	}
};

#endif
