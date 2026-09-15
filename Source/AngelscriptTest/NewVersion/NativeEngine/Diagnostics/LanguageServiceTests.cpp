#include "CQTest.h"
#include "../NativeEngineTestSupport.h"

#include "as_builder.h"
#include "frontend/Compile/as_language_service.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	LanguageService,
	"Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	static TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> SourcesFromManager(
		const AS_NAMESPACE_QUALIFIER asCSourceManager& Manager)
	{
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources;
		for (asUINT Index = 1; Index <= Manager.GetFileCount(); ++Index)
		{
			const AS_NAMESPACE_QUALIFIER asCSourceFileID File(Index);
			FStringView Key;
			TConstArrayView<uint8> Bytes;
			if (!Manager.TryGetLogicalSourceKey(File, Key) || !Manager.TryGetBytes(File, Bytes))
			{
				continue;
			}
			Sources.Add(AngelscriptNativeEngineTest::MakeSharedSourceBytes(Key, Bytes));
		}
		return Sources;
	}

	static const AS_NAMESPACE_QUALIFIER asSDiagnosticGroup* FindUnresolved(
		TConstArrayView<AS_NAMESPACE_QUALIFIER asSDiagnosticGroup> Groups)
	{
		for (const AS_NAMESPACE_QUALIFIER asSDiagnosticGroup& Group : Groups)
		{
			if (Group.Primary.DiagnosticID
				== static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnresolvedReference))
			{
				return &Group;
			}
		}
		return nullptr;
	}

	TEST_METHOD(RetainedSdkAttachment)
	{
		AS_NAMESPACE_QUALIFIER asSBuilderOptions Options;
		Options.StableScope = TEXT("language-service");
		TSharedPtr<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> Result;
		{
			AS_NAMESPACE_QUALIFIER asCBuilder Builder(
				AngelscriptNativeEngineTest::MakeSourceList(
					TEXT("Typo.as"),
					TEXT("int F(){ int Count=1; return Cout; }")),
				Options);
			ASSERT_THAT(IsFalse(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::BodiesAnalyzed)));
			TUniquePtr<AS_NAMESPACE_QUALIFIER asCCompileOutput> Output = Builder.TakeCompileOutput();
			ASSERT_THAT(IsNotNull(Output.Get()));
			Result = Output->GetDiagnosticResult();
		}
		ASSERT_THAT(IsTrue(Result.IsValid()));
		std::shared_ptr<AS_NAMESPACE_QUALIFIER asCLanguageService> Service =
			AS_NAMESPACE_QUALIFIER asCLanguageService::Create();
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asELanguageServiceStatus::Succeeded),
			static_cast<int32>(Service->AttachCompilation(Result.ToSharedRef()))));
		Result.Reset();
		const TConstArrayView<AS_NAMESPACE_QUALIFIER asSDiagnosticGroup> Groups = Service->GetGroups();
		const AS_NAMESPACE_QUALIFIER asSDiagnosticGroup* Group = FindUnresolved(Groups);
		ASSERT_THAT(IsNotNull(Group));
		FString Text;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asELanguageServiceStatus::Succeeded),
			static_cast<int32>(Service->Format(*Group, Text))));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("Cout"))));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("Count"))));
		ASSERT_THAT(AreEqual(1, Group->Fixes.Num()));
		TSharedPtr<const AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asELanguageServiceStatus::Succeeded),
			static_cast<int32>(Service->ApplyFix(Group->Fixes[0], Snapshot))));
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));
		TConstArrayView<uint8> Bytes;
		ASSERT_THAT(IsTrue(Snapshot->GetManager().TryGetBytes(
			AS_NAMESPACE_QUALIFIER asCSourceFileID(1), Bytes)));
		ASSERT_THAT(IsTrue(Bytes.Num() >= 5));
		bool bHasCount = false;
		for (int32 Index = 0; Index + 4 < Bytes.Num(); ++Index)
		{
			if (Bytes[Index] == 'C' && Bytes[Index + 1] == 'o' && Bytes[Index + 2] == 'u'
				&& Bytes[Index + 3] == 'n' && Bytes[Index + 4] == 't'
				&& (Index + 5 >= Bytes.Num() || Bytes[Index + 5] != 't'))
			{
				bHasCount = true;
			}
		}
		ASSERT_THAT(IsTrue(bHasCount));
	}

	TEST_METHOD(FeatureAndOwnerBoundaries)
	{
		std::shared_ptr<AS_NAMESPACE_QUALIFIER asCLanguageService> DiagnosticsOnly =
			AS_NAMESPACE_QUALIFIER asCLanguageService::Create(
				AS_NAMESPACE_QUALIFIER asLANGUAGE_SERVICE_DIAGNOSTICS);
		AS_NAMESPACE_QUALIFIER asSBuilderOptions Options;
		Options.StableScope = TEXT("language-service");
		AS_NAMESPACE_QUALIFIER asCBuilder FirstBuilder(
			AngelscriptNativeEngineTest::MakeSourceList(
				TEXT("Typo.as"),
				TEXT("int F(){ int Count=1; return Cout; }")),
			Options);
		ASSERT_THAT(IsFalse(FirstBuilder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::BodiesAnalyzed)));
		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> First =
			FirstBuilder.GetCompileOutput()->GetDiagnosticResult();
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asELanguageServiceStatus::Succeeded),
			static_cast<int32>(DiagnosticsOnly->AttachCompilation(First))));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asELanguageServiceStatus::Unavailable),
			static_cast<int32>(DiagnosticsOnly->QueryCompletion())));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asELanguageServiceStatus::Unavailable),
			static_cast<int32>(DiagnosticsOnly->QueryHover())));
		const AS_NAMESPACE_QUALIFIER asSDiagnosticGroup* Group = FindUnresolved(DiagnosticsOnly->GetGroups());
		ASSERT_THAT(IsNotNull(Group));
		FString Text;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asELanguageServiceStatus::Succeeded),
			static_cast<int32>(DiagnosticsOnly->Format(*Group, Text))));
		TSharedPtr<const AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asELanguageServiceStatus::Unavailable),
			static_cast<int32>(DiagnosticsOnly->ApplyFix(Group->Fixes[0], Snapshot))));
		ASSERT_THAT(IsFalse(Snapshot.IsValid()));

		AS_NAMESPACE_QUALIFIER asSDiagnosticGroup Copied = *Group;
		AS_NAMESPACE_QUALIFIER asCBuilder SecondBuilder(
			AngelscriptNativeEngineTest::MakeSourceList(TEXT("Ok.as"), TEXT("int G(){ return 2; }")),
			Options);
		ASSERT_THAT(IsTrue(SecondBuilder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::BodiesAnalyzed)));
		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> Second =
			SecondBuilder.GetCompileOutput()->GetDiagnosticResult();
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asELanguageServiceStatus::Succeeded),
			static_cast<int32>(DiagnosticsOnly->AttachCompilation(Second))));
		FString Unchanged = TEXT("keep");
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asELanguageServiceStatus::OwnerMismatch),
			static_cast<int32>(DiagnosticsOnly->Format(Copied, Unchanged))));
		ASSERT_THAT(AreEqual(FString(TEXT("keep")), Unchanged));

		std::shared_ptr<AS_NAMESPACE_QUALIFIER asCLanguageService> EmptyService =
			AS_NAMESPACE_QUALIFIER asCLanguageService::Create();
		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> Empty =
			MakeShared<AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe>();
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asELanguageServiceStatus::Succeeded),
			static_cast<int32>(EmptyService->AttachCompilation(Empty))));
		ASSERT_THAT(AreEqual(0, EmptyService->GetGroups().Num()));
		ASSERT_THAT(AreNotEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asELanguageServiceStatus::Unavailable),
			static_cast<int32>(EmptyService->AttachCompilation(Empty))));
	}

	TEST_METHOD(AcceptedLanguageServiceBoundaries)
	{
		AS_NAMESPACE_QUALIFIER asSBuilderOptions Options;
		Options.StableScope = TEXT("language-service");
		AS_NAMESPACE_QUALIFIER asCBuilder Builder(
			AngelscriptNativeEngineTest::MakeSourceList(
				TEXT("Typo.as"),
				TEXT("int F(){ int Count=1; return Cout; }")),
			Options);
		ASSERT_THAT(IsFalse(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::BodiesAnalyzed)));
		std::shared_ptr<AS_NAMESPACE_QUALIFIER asCLanguageService> Service =
			AS_NAMESPACE_QUALIFIER asCLanguageService::Create();
		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> Original =
			Builder.GetCompileOutput()->GetDiagnosticResult();
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asELanguageServiceStatus::Succeeded),
			static_cast<int32>(Service->AttachCompilation(Original))));
		const AS_NAMESPACE_QUALIFIER asSDiagnosticGroup* Group = FindUnresolved(Service->GetGroups());
		ASSERT_THAT(IsNotNull(Group));
		FString Text;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asELanguageServiceStatus::Succeeded),
			static_cast<int32>(Service->Format(*Group, Text))));
		const FString Expected = FString(TEXT(
			"Typo.as:1:30: error: unresolved reference 'Cout'\n"
			"int F(){ int Count=1; return Cout; }\n"
			"                             ^~~~\n"
			"Typo.as:1:10: note: did you mean 'Count'?\n"
			"int F(){ int Count=1; return Cout; }\n"
			"         ^~~~~~~~~~~"));
		ASSERT_THAT(AreEqual(Expected, Text));
		ASSERT_THAT(AreEqual(FString(TEXT("Count")), Group->Notes[0].Arguments[0].TextValue));
		ASSERT_THAT(AreEqual(5, Group->Fixes[0].SourceEdits[0].ReplacementUtf8.Num()));
		ASSERT_THAT(AreEqual(static_cast<uint8>('C'), Group->Fixes[0].SourceEdits[0].ReplacementUtf8[0]));
		TSharedPtr<const AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asELanguageServiceStatus::Succeeded),
			static_cast<int32>(Service->ApplyFix(Group->Fixes[0], Snapshot))));
		ASSERT_THAT(IsTrue(FindUnresolved(Original->GetGroups()) != nullptr));
		AS_NAMESPACE_QUALIFIER asSBuilderOptions Again = Options;
		AS_NAMESPACE_QUALIFIER asCBuilder Fixed(SourcesFromManager(Snapshot->GetManager()), Again);
		ASSERT_THAT(IsTrue(Fixed.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::BodiesAnalyzed)));
		ASSERT_THAT(IsNull(FindUnresolved(Fixed.GetCompileOutput()->GetDiagnosticResult()->GetGroups())));
	}
};

#endif
