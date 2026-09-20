#include "CQTest.h"

#include "NativeEngine/NativeEngineTestSupport.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_decl.h"
#include "frontend/Compile/as_compilation_session.h"
#include "frontend/Parser/as_parser.h"
#include "Containers/StringConv.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace DelegateDiagnosticsTests
{
	using FSessionRun = AngelscriptNativeEngineTest::FFrontendSessionRun;

	TArray<AS_NAMESPACE_QUALIFIER asSDiagnosticRecord> GetDiagnostics(FSessionRun& Run)
	{
		AS_NAMESPACE_QUALIFIER asCCollectingDiagnosticConsumer Consumer;
		Run.Diagnostics.EmitTo(Consumer);
		TArray<AS_NAMESPACE_QUALIFIER asSDiagnosticRecord> Records;
		Records.Append(Consumer.GetRecords().GetData(), Consumer.GetRecords().Num());
		return Records;
	}

	FString SourceText(const FSessionRun& Run, const AS_NAMESPACE_QUALIFIER asCSourceRange& Range)
	{
		TConstArrayView<uint8> Bytes;
		if (!Run.SourceManager.TryGetBytes(Range, Bytes)) return TEXT("<invalid-range>");
		FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
		return FString(Text.Length(), Text.Get());
	}

	bool HasDiagnostic(const TArray<AS_NAMESPACE_QUALIFIER asSDiagnosticRecord>& Records, asUINT ID)
	{
		for (const AS_NAMESPACE_QUALIFIER asSDiagnosticRecord& Record : Records)
		{
			if (Record.DiagnosticID == ID) return true;
		}
		return false;
	}
}

TEST_CLASS_WITH_FLAGS(DelegateDiagnostics, "Angelscript.UnitTest.NativeEngine.Sema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(RejectDelegateKeyword)
	{
		DelegateDiagnosticsTests::FSessionRun Run({{TEXT("Reject.as"), TEXT("delegate int FOnDone(); class Good {}")}});
		ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
		ASSERT_THAT(AreEqual(0, Run.Session->Lookup(TEXT("FOnDone")).Num()));
		ASSERT_THAT(AreEqual(1, Run.Session->Lookup(TEXT("Good")).Num()));
		const auto Diagnostics = DelegateDiagnosticsTests::GetDiagnostics(Run);
		ASSERT_THAT(IsTrue(DelegateDiagnosticsTests::HasDiagnostic(Diagnostics,
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDeclarationDiagnostic::RemovedDelegateEventKeyword))));
		for (const auto& Record : Diagnostics)
		{
			if (Record.DiagnosticID != static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDeclarationDiagnostic::RemovedDelegateEventKeyword)) continue;
			ASSERT_THAT(AreEqual(FString(TEXT("delegate")), DelegateDiagnosticsTests::SourceText(Run, Record.PrimaryRange)));
		}
	}

	TEST_METHOD(RejectEventKeyword)
	{
		DelegateDiagnosticsTests::FSessionRun Run({{TEXT("RejectEvent.as"), TEXT("event void FOnChanged();")}});
		ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
		ASSERT_THAT(AreEqual(0, Run.Session->Lookup(TEXT("FOnChanged")).Num()));
		const auto Diagnostics = DelegateDiagnosticsTests::GetDiagnostics(Run);
		ASSERT_THAT(IsTrue(DelegateDiagnosticsTests::HasDiagnostic(Diagnostics,
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDeclarationDiagnostic::RemovedDelegateEventKeyword))));
	}

	TEST_METHOD(UnsupportedEventFamily)
	{
		DelegateDiagnosticsTests::FSessionRun Run({{TEXT("EventFamily.as"), TEXT("DECLARE_EVENT(AOwner, FOwnerEvent); class Next {}")}});
		ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
		ASSERT_THAT(AreEqual(0, Run.Session->Lookup(TEXT("FOwnerEvent")).Num()));
		ASSERT_THAT(AreEqual(1, Run.Session->Lookup(TEXT("Next")).Num()));
		const auto Diagnostics = DelegateDiagnosticsTests::GetDiagnostics(Run);
		ASSERT_THAT(IsTrue(DelegateDiagnosticsTests::HasDiagnostic(Diagnostics,
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDeclarationDiagnostic::UnsupportedDelegateDeclarationForm))));
	}

	TEST_METHOD(MulticastRetValRejected)
	{
		DelegateDiagnosticsTests::FSessionRun Run({{TEXT("MulticastRetVal.as"), TEXT("DECLARE_MULTICAST_DELEGATE_RetVal(int, FBad);")}});
		ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
		ASSERT_THAT(AreEqual(0, Run.Session->Lookup(TEXT("FBad")).Num()));
		const auto Diagnostics = DelegateDiagnosticsTests::GetDiagnostics(Run);
		ASSERT_THAT(IsTrue(DelegateDiagnosticsTests::HasDiagnostic(Diagnostics,
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDeclarationDiagnostic::UnsupportedDelegateDeclarationForm))));
	}
};

#endif
