#include "AngelscriptTestMacros.h"
#include "AngelScriptSDK/Support/AngelscriptNativeCoreTestSupport.h"
#include "Dump/AngelscriptASTDiagnostics.h"

#include "CQTest.h"
#include "Dom/JsonObject.h"
#include "HAL/IConsoleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_diagnostics.h"
#include "source/as_ast_dump.h"
#include "source/as_ast_public_view.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
	asIScriptModule* BuildRetainedDiagnosticModule(
		asIScriptEngine* Engine,
		const char* ModuleName,
		const char* Source)
	{
		asIScriptModule* Module = Engine != nullptr
			? Engine->GetModule(ModuleName, asGM_ALWAYS_CREATE)
			: nullptr;
		if (Module == nullptr
			|| Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT) < 0
			|| Module->AddScriptSection(ModuleName, Source) < 0
			|| Module->Build() < 0)
		{
			return nullptr;
		}
		return Module;
	}

	FString ToFString(const asCString& Value)
	{
		return UTF8_TO_TCHAR(Value.AddressOf() != nullptr ? Value.AddressOf() : "");
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptASTDiagnosticsTests,
	"Angelscript.TestModule.Dump.ASTDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ReadOnlyOperationsBalanceSnapshotLeasesAndExposeStableMetadata)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asIScriptModule* Left = BuildRetainedDiagnosticModule(
			Engine.Get(), "ASTDiagLeft", "int F() { return 42; } int G() { return F(); }");
		asIScriptModule* Right = BuildRetainedDiagnosticModule(
			Engine.Get(), "ASTDiagRight", "int F() { return 43; } int G() { return F(); }");
		ASSERT_THAT(IsNotNull(Left));
		ASSERT_THAT(IsNotNull(Right));

		asIASTSnapshot* Anchor = Left->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(Anchor));
		ON_SCOPE_EXIT { Anchor->Release(); };
		asCASTSnapshot* Internal = static_cast<asCASTSnapshot*>(Anchor);
		const asDWORD ReferenceCountBefore = Internal->GetReferenceCountForDiagnostics();

		asCASTContext const* Context = Internal->GetContext();
		ASSERT_THAT(IsNotNull(Context));
		asCString ASTBefore;
		asCASTDump(*Context, ASTBefore);

		const asEASTDiagnosticOperation Operations[] = {
			asAST_DIAGNOSTIC_LIST,
			asAST_DIAGNOSTIC_DUMP,
			asAST_DIAGNOSTIC_VERIFY,
			asAST_DIAGNOSTIC_QUERY,
			asAST_DIAGNOSTIC_DIFF,
		};
		for (asEASTDiagnosticOperation Operation : Operations)
		{
			asSASTDiagnosticRequest Request;
			Request.operation = Operation;
			Request.moduleName = "ASTDiagLeft";
			Request.compareModuleName = "ASTDiagRight";
			Request.filter.stableDeclKey = "F()";
			asCString Output;
			ASSERT_THAT(AreEqual(asSUCCESS,
				asCASTRunDiagnostics(Engine.Get(), Request, Output),
				*FString::Printf(TEXT("operation %d failed: %s"),
					static_cast<int32>(Operation), *ToFString(Output))));
			const FString Text = ToFString(Output);
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("diagnosticOnly=true"))));
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("cacheInput=false"))));
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("readOnly=true"))));
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("mutation=refused"))));
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("lease=acquired-released"))));
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("module=\"ASTDiagLeft\""))));
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("generation=\"ASTDiagLeft:1\""))));
			ASSERT_THAT(IsTrue(Text.Contains(TEXT("provenance=source-build"))));
			ASSERT_THAT(IsFalse(Text.Contains(TEXT("0x"))));
			ASSERT_THAT(AreEqual(ReferenceCountBefore,
				Internal->GetReferenceCountForDiagnostics(),
				TEXT("each operation must release every lease it acquires")));
		}

		asCString ASTAfter;
		asCASTDump(*Context, ASTAfter);
		ASSERT_THAT(IsTrue(ASTBefore.Equals(ASTAfter),
			TEXT("diagnostics must not mutate the retained AST")));

		for (asEASTDiagnosticOperation Operation : Operations)
		{
			asSASTDiagnosticRequest Request;
			Request.operation = Operation;
			Request.format = asAST_DIAGNOSTIC_JSON;
			Request.moduleName = "ASTDiagLeft";
			Request.compareModuleName = "ASTDiagRight";
			Request.filter.stableDeclKey = "F()";
			asCString Output;
			ASSERT_THAT(AreEqual(asSUCCESS, asCASTRunDiagnostics(Engine.Get(), Request, Output)));
			const FString JsonText = ToFString(Output);
			TSharedPtr<FJsonObject> JsonObject;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
			ASSERT_THAT(IsTrue(FJsonSerializer::Deserialize(Reader, JsonObject)
				&& JsonObject.IsValid(),
				*FString::Printf(TEXT("operation %d must emit valid JSON: %s"),
					static_cast<int32>(Operation), *JsonText)));
			ASSERT_THAT(IsTrue(JsonObject->GetBoolField(TEXT("diagnosticOnly"))));
			ASSERT_THAT(IsFalse(JsonObject->GetBoolField(TEXT("cacheInput"))));
			ASSERT_THAT(IsTrue(JsonObject->GetBoolField(TEXT("readOnly"))));
			ASSERT_THAT(AreEqual(FString(TEXT("refused")),
				JsonObject->GetStringField(TEXT("mutation"))));
			ASSERT_THAT(AreEqual(ReferenceCountBefore,
				Internal->GetReferenceCountForDiagnostics()));
		}
	}

	TEST_METHOD(FilteredDumpQueryVerifyAndDiffRemainMutationFree)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asIScriptModule* Left = BuildRetainedDiagnosticModule(
			Engine.Get(), "ASTDiagFilteredLeft", "int F() { return 42; } int G() { return F(); }");
		asIScriptModule* Right = BuildRetainedDiagnosticModule(
			Engine.Get(), "ASTDiagFilteredRight", "int F() { return 43; } int G() { return F(); }");
		asIScriptModule* Discard = Engine.Get()->GetModule("ASTDiagDiscard", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Left));
		ASSERT_THAT(IsNotNull(Right));
		ASSERT_THAT(IsNotNull(Discard));
		ASSERT_THAT(AreEqual(asSUCCESS, Discard->AddScriptSection(
			"ASTDiagDiscard", "int F() { return 1; }")));
		ASSERT_THAT(AreEqual(asSUCCESS, Discard->Build()));

		asSASTDiagnosticRequest Query;
		Query.operation = asAST_DIAGNOSTIC_QUERY;
		Query.moduleName = "ASTDiagFilteredLeft";
		Query.filter.stableDeclKey = "F()";
		asCString QueryOutput;
		ASSERT_THAT(AreEqual(asSUCCESS, asCASTRunDiagnostics(Engine.Get(), Query, QueryOutput)));
		asCString RepeatedQueryOutput;
		ASSERT_THAT(AreEqual(asSUCCESS,
			asCASTRunDiagnostics(Engine.Get(), Query, RepeatedQueryOutput)));
		ASSERT_THAT(IsTrue(QueryOutput.Equals(RepeatedQueryOutput),
			TEXT("repeating a query over the same immutable generation must be byte deterministic")));
		const FString QueryText = ToFString(QueryOutput);
		ASSERT_THAT(IsTrue(QueryText.Contains(TEXT("matchCount=1"))));
		ASSERT_THAT(IsTrue(QueryText.Contains(TEXT("key=\"F()\""))));
		ASSERT_THAT(IsFalse(QueryText.Contains(TEXT("key=\"G()\"")),
			TEXT("query returns exact matches, not dump closure")));

		asSASTDiagnosticRequest Dump = Query;
		Dump.operation = asAST_DIAGNOSTIC_DUMP;
		Dump.format = asAST_DIAGNOSTIC_JSON;
		asCString DumpOutput;
		ASSERT_THAT(AreEqual(asSUCCESS, asCASTRunDiagnostics(Engine.Get(), Dump, DumpOutput)));
		const FString DumpText = ToFString(DumpOutput);
		ASSERT_THAT(IsTrue(DumpText.StartsWith(TEXT("{"))));
		ASSERT_THAT(IsTrue(DumpText.Contains(TEXT("\"diagnosticOnly\":true"))));
		ASSERT_THAT(IsTrue(DumpText.Contains(TEXT("\"cacheInput\":false"))));
		ASSERT_THAT(IsTrue(DumpText.Contains(TEXT("\"readOnly\":true"))));
		ASSERT_THAT(IsTrue(DumpText.Contains(TEXT("\"mutation\":\"refused\""))));

		asSASTDiagnosticRequest Verify;
		Verify.operation = asAST_DIAGNOSTIC_VERIFY;
		Verify.moduleName = "ASTDiagFilteredLeft";
		asCString VerifyOutput;
		ASSERT_THAT(AreEqual(asSUCCESS, asCASTRunDiagnostics(Engine.Get(), Verify, VerifyOutput)));
		ASSERT_THAT(IsTrue(ToFString(VerifyOutput).Contains(TEXT("verify=pass"))));

		asSASTDiagnosticRequest Diff;
		Diff.operation = asAST_DIAGNOSTIC_DIFF;
		Diff.moduleName = "ASTDiagFilteredLeft";
		Diff.compareModuleName = "ASTDiagFilteredRight";
		asCString DiffOutput;
		ASSERT_THAT(AreEqual(asSUCCESS, asCASTRunDiagnostics(Engine.Get(), Diff, DiffOutput)));
		ASSERT_THAT(IsTrue(ToFString(DiffOutput).Contains(TEXT("mismatch path="))));

		asSASTDiagnosticRequest Missing;
		Missing.operation = asAST_DIAGNOSTIC_VERIFY;
		Missing.moduleName = "NoSuchASTModule";
		asCString MissingOutput;
		ASSERT_THAT(AreEqual(asNO_MODULE, asCASTRunDiagnostics(Engine.Get(), Missing, MissingOutput)));

		Missing.moduleName = "ASTDiagDiscard";
		MissingOutput = "";
		ASSERT_THAT(AreEqual(asNOT_SUPPORTED,
			asCASTRunDiagnostics(Engine.Get(), Missing, MissingOutput),
			TEXT("a module without a retained snapshot must fail without borrowing a raw AST")));
	}

	TEST_METHOD(RuntimeParserRejectsMutationAndRegistersDeveloperCommand)
	{
		ASSERT_THAT(AreEqual(FString(TEXT("as.AST")),
			FString(FAngelscriptASTDiagnostics::GetCommandName())));
		ASSERT_THAT(IsNotNull(IConsoleManager::Get().FindConsoleObject(
			FAngelscriptASTDiagnostics::GetCommandName())));

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		ASSERT_THAT(IsNotNull(BuildRetainedDiagnosticModule(
			Engine.Get(), "ASTDiagRuntime", "int F() { return 42; }")));

		FString Output;
		FString Error;
		ASSERT_THAT(AreEqual(asINVALID_ARG,
			FAngelscriptASTDiagnostics::Run(
				Engine.Get(), {TEXT("mutate"), TEXT("ASTDiagRuntime")}, Output, Error)));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("read-only"))));
		ASSERT_THAT(AreEqual(asSUCCESS,
			FAngelscriptASTDiagnostics::Run(
				Engine.Get(), {TEXT("query"), TEXT("ASTDiagRuntime"), TEXT("--key=F()")}, Output, Error)));
		ASSERT_THAT(IsTrue(Output.Contains(TEXT("matchCount=1"))));
	}
};

#endif
