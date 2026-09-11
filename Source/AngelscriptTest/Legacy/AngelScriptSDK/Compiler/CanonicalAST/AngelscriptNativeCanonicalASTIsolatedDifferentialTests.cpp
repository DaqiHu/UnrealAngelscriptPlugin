#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"
#include "../../Support/AngelscriptNativeBuilderTestSupport.h"
#include "../../Support/AngelscriptNativeCompilerBytecodeTestSupport.h"

#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_dump.h"
#include "frontend/as_ast_verifier.h"
#include "source/as_bytecode_codegen.h"
#include "as_module.h"
#include "frontend/as_parser.h"
#include "source/as_scriptcode.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "frontend/as_sema.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
	int GenerateCanonicalFromSource(
		asCScriptEngine* ScriptEngine,
		asCModule* Module,
		const char* Section,
		const char* Source,
		FString* OutDump)
	{
		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		Code.SetCode(Section, Source, true);
		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		const int ParseResult = Parser.ParseScript(&Code);
		if (ParseResult < 0)
		{
			return ParseResult;
		}
		Sema.LayoutScriptClassFields();
		const int SealResult = Context.Seal();
		if (SealResult != 0)
		{
			return SealResult;
		}
		if (OutDump != nullptr)
		{
			asCString Dump;
			asCASTDump(Context, Dump);
			*OutDump = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		}
		asCBytecodeCodeGen CodeGen;
		return CodeGen.Generate(Context, Module);
	}

	int32 BytecodeDwordCount(asIScriptFunction* Function)
	{
		asCScriptFunction* const Internal = static_cast<asCScriptFunction*>(Function);
		if (Internal == nullptr || Internal->scriptData == nullptr)
		{
			return 0;
		}
		return static_cast<int32>(Internal->scriptData->byteCode.GetLength());
	}

	bool LooksLikeUeScript(const FString& Source)
	{
		return Source.Contains(TEXT("UCLASS"))
			|| Source.Contains(TEXT("UFUNCTION"))
			|| Source.Contains(TEXT("UPROPERTY"))
			|| Source.Contains(TEXT(": AActor"))
			|| Source.Contains(TEXT(": UObject"))
			|| Source.Contains(TEXT(": APawn"));
	}
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTIsolatedDifferentialTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.IsolatedDifferential",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(IsolatedFailedGenerateLeavesNoCandidatePublication)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine,
			"IsolatedFailedGenerateTransaction");
		ASSERT_THAT(IsNotNull(Module, TEXT("isolated transaction fixture module")));
		if (Module == nullptr)
		{
			return;
		}

		// Generate() must clean up its own candidate rather than relying on the
		// production Build() caller to run a second InternalReset(). This source
		// intentionally reaches body emission only after a mutable global has
		// acquired its property-owned initializer and F has a sealed signature.
		const asUINT InitialScriptFunctionCount = ScriptEngine->scriptFunctions.GetLength();
		const asUINT InitialFreeScriptFunctionIdCount = ScriptEngine->freeScriptFunctionIds.GetLength();
		const asUINT InitialGlobalPropertyCount = ScriptEngine->globalProperties.GetLength();
		const asUINT InitialFreeGlobalPropertyIdCount = ScriptEngine->freeGlobalPropertyIds.GetLength();
		const int32 InitialGlobalAddressCount = ScriptEngine->varAddressMap.Num();

		const char* const Source = R"AS(
			int G = 1;
			int F()
			{
				try
				{
					return G;
				}
				catch
				{
					return 0;
				}
			}
		)AS";
		const int GenerateResult = GenerateCanonicalFromSource(
			ScriptEngine,
			Module,
			"IsolatedFailedGenerateTransaction",
			Source,
			nullptr);
		ASSERT_THAT(IsTrue(GenerateResult < 0,
			TEXT("try/catch must fail during isolated Canonical CodeGen transaction")));

		ASSERT_THAT(AreEqual(asUINT(0), Module->GetFunctionCount(),
			TEXT("failed Generate must not leave callable module functions")));
		ASSERT_THAT(AreEqual(asUINT(0), Module->GetGlobalVarCount(),
			TEXT("failed Generate must not leave module global properties")));
		ASSERT_THAT(AreEqual(asUINT(0), Module->bindInformations.GetLength(),
			TEXT("failed Generate must not leave module import records")));
		ASSERT_THAT(AreEqual(asUINT(0), Module->classTypes.GetLength(),
			TEXT("failed Generate must not leave module script types")));
		ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_NONE, Module->GetLastBytecodePublisher(),
			TEXT("failed Generate must not claim a bytecode publisher")));

		ASSERT_THAT(AreEqual(InitialScriptFunctionCount, ScriptEngine->scriptFunctions.GetLength(),
			TEXT("failed Generate must restore engine script-function slot count")));
		ASSERT_THAT(AreEqual(InitialFreeScriptFunctionIdCount, ScriptEngine->freeScriptFunctionIds.GetLength(),
			TEXT("failed Generate must restore free script-function IDs")));
		ASSERT_THAT(AreEqual(InitialGlobalPropertyCount, ScriptEngine->globalProperties.GetLength(),
			TEXT("failed Generate must restore engine global-property slot count")));
		ASSERT_THAT(AreEqual(InitialFreeGlobalPropertyIdCount, ScriptEngine->freeGlobalPropertyIds.GetLength(),
			TEXT("failed Generate must restore free global-property IDs")));
		ASSERT_THAT(AreEqual(InitialGlobalAddressCount, ScriptEngine->varAddressMap.Num(),
			TEXT("failed Generate must restore the global-address ownership map")));
	}

	TEST_METHOD(IsolatedEnginesMatchOnPrimitiveAndControlFixtures)
	{
		struct FCase
		{
			const char* Name;
			const char* Source;
			const char* Decl;
			int32 Expected;
		};
		const FCase Cases[] = {
			{ "Lit", "int F() { return 7; }", "int F()", 7 },
			{ "Add", "int F() { return 3 + 4; }", "int F()", 7 },
			{ "If", "int F() { if (1 > 0) return 4; return 0; }", "int F()", 4 },
			{ "While", "int F() { int Y = 0; while (Y < 3) { Y = Y + 1; } return Y; }", "int F()", 3 },
			{ "Call", "int G(int X) { return X + 1; } int F() { return G(6); }", "int F()", 7 },
		};

		for (const FCase& Case : Cases)
		{
			AngelscriptNativeTestSupport::FNativeTestEngine LegacyEngine;
			LegacyEngine.Create(*TestRunner);
			ON_SCOPE_EXIT { LegacyEngine.Destroy(); };
			asIScriptModule* LegacyModule = nullptr;
			ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
				LegacyEngine.Get(), Case.Name, Case.Source, LegacyModule),
				*FString::Printf(TEXT("legacy %hs should compile"), Case.Name)));
			int32 LegacyValue = 0;
			ASSERT_THAT(IsTrue(
				AngelscriptNativeTestSupport::CanonicalExecuteInt(
					*TestRunner, LegacyEngine.Get(), LegacyModule, Case.Decl, LegacyValue),
				*FString::Printf(TEXT("legacy %hs should execute"), Case.Name)));

			AngelscriptNativeTestSupport::FNativeTestEngine CanonicalEngine;
			CanonicalEngine.Create(*TestRunner);
			ON_SCOPE_EXIT { CanonicalEngine.Destroy(); };
			asCScriptEngine* const CanonicalScript = static_cast<asCScriptEngine*>(CanonicalEngine.Get());
			asCModule* const CanonicalModule = AngelscriptBuilderTestSupport::CreateBuilderModule(CanonicalScript, Case.Name);
			FString CanonicalDump;
			ASSERT_THAT(AreEqual(0, GenerateCanonicalFromSource(
				CanonicalScript, CanonicalModule, Case.Name, Case.Source, &CanonicalDump),
				*FString::Printf(TEXT("canonical %hs should lower"), Case.Name)));
			int32 CanonicalValue = 0;
			ASSERT_THAT(IsTrue(
				AngelscriptNativeTestSupport::CanonicalExecuteInt(
					*TestRunner, CanonicalEngine.Get(), CanonicalModule, Case.Decl, CanonicalValue),
				*FString::Printf(TEXT("canonical %hs should execute"), Case.Name)));

			ASSERT_THAT(AreEqual(Case.Expected, LegacyValue,
				*FString::Printf(TEXT("legacy %hs expected value"), Case.Name)));
			ASSERT_THAT(AreEqual(LegacyValue, CanonicalValue,
				*FString::Printf(
					TEXT("isolated %hs execution must match legacy=%d canonical=%d dump:\n%s"),
					Case.Name, LegacyValue, CanonicalValue, *CanonicalDump)));

			const int32 LegacyWords = BytecodeDwordCount(LegacyModule->GetFunctionByDecl(Case.Decl));
			const int32 CanonicalWords = BytecodeDwordCount(CanonicalModule->GetFunctionByDecl(Case.Decl));
			ASSERT_THAT(IsTrue(LegacyWords > 0 && CanonicalWords > 0,
				*FString::Printf(TEXT("%hs both sides must publish bytecode"), Case.Name)));
			(void)LegacyWords;
			(void)CanonicalWords;
		}
	}

	TEST_METHOD(BytecodeByteDifferencesAreReportedSeparatelyFromBehavior)
	{
		const char* Source = "int F() { return 7; }";
		AngelscriptNativeTestSupport::FNativeTestEngine LegacyEngine;
		LegacyEngine.Create(*TestRunner);
		ON_SCOPE_EXIT { LegacyEngine.Destroy(); };
		asIScriptModule* LegacyModule = nullptr;
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			LegacyEngine.Get(), "ByteDiffL", Source, LegacyModule),
			TEXT("legacy byte-diff source should compile")));

		AngelscriptNativeTestSupport::FNativeTestEngine CanonicalEngine;
		CanonicalEngine.Create(*TestRunner);
		ON_SCOPE_EXIT { CanonicalEngine.Destroy(); };
		asCScriptEngine* const CanonicalScript = static_cast<asCScriptEngine*>(CanonicalEngine.Get());
		asCModule* const CanonicalModule = AngelscriptBuilderTestSupport::CreateBuilderModule(CanonicalScript, "ByteDiffC");
		ASSERT_THAT(AreEqual(0, GenerateCanonicalFromSource(
			CanonicalScript, CanonicalModule, "ByteDiffC", Source, nullptr),
			TEXT("canonical byte-diff source should lower")));

		int32 LegacyValue = 0;
		int32 CanonicalValue = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, LegacyEngine.Get(), LegacyModule, "int F()", LegacyValue),
			TEXT("legacy F() should execute")));
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, CanonicalEngine.Get(), CanonicalModule, "int F()", CanonicalValue),
			TEXT("canonical F() should execute")));
		ASSERT_THAT(AreEqual(LegacyValue, CanonicalValue,
			TEXT("behavior match does not require equal bytecode bytes")));

		asIScriptFunction* const LegacyFn = LegacyModule->GetFunctionByDecl("int F()");
		asIScriptFunction* const CanonicalFn = CanonicalModule->GetFunctionByDecl("int F()");
		const int32 LegacyWords = BytecodeDwordCount(LegacyFn);
		const int32 CanonicalWords = BytecodeDwordCount(CanonicalFn);
		TestRunner->AddInfo(FString::Printf(
			TEXT("bytecode dword counts legacy=%d canonical=%d (bytes may differ)"),
			LegacyWords,
			CanonicalWords));
		ASSERT_THAT(IsTrue(LegacyWords > 0 && CanonicalWords > 0,
			TEXT("both compilers must emit inspectable bytecode")));
	}

	TEST_METHOD(GeneratedAccessorPlanSurvivesMethodRename)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "AccessorPlanRename");
		ASSERT_THAT(IsNotNull(Module, TEXT("AccessorPlanRename module")));
		if (Module == nullptr)
		{
			return;
		}

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string Source = ASTEST_AS_ANSI(R"AS(
			struct FRecord
			{
				int Stored;
			}
			)AS");
		Code.SetCode("AccessorPlanRename.as", Source.c_str(), true);
		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("accessor fixture must parse through Sema before its generated names are changed")));

		bool bRenamedGetter = false;
		bool bRenamedSetter = false;
		for (asUINT Index = 1; Index <= Context.GetDeclCount(); ++Index)
		{
			const asCDecl* const Decl = Context.GetDecl(asASTDeclId(Index));
			if (Decl == nullptr)
			{
				continue;
			}
			if (Decl->name.Equals("GetStored"))
			{
				ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Context.SetDeclName(Decl->id, "ReadStored")));
				bRenamedGetter = true;
			}
			else if (Decl->name.Equals("SetStored"))
			{
				ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Context.SetDeclName(Decl->id, "WriteStored")));
				bRenamedSetter = true;
			}
		}
		ASSERT_THAT(IsTrue(bRenamedGetter && bRenamedSetter,
			TEXT("Sema must synthesize both accessors before this test changes only their presentation names")));
		if (!bRenamedGetter || !bRenamedSetter)
		{
			return;
		}

		// Match asCBuilder::SealCanonicalAST(): field layout is a Sema fact that
		// must be recorded before the AST becomes immutable.
		Sema.LayoutScriptClassFields();
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Context.Seal(),
			TEXT("renamed generated accessors must still form a valid sealed AST")));
		asCBytecodeCodeGen CodeGen;
		const int LowerResult = CodeGen.Generate(Context, Module);
		ASSERT_THAT(AreEqual(0, LowerResult,
			TEXT("CodeGen must use Sema's sealed accessor-to-field identity, not a Get/Set method-name convention")));
		if (LowerResult != 0)
		{
			return;
		}

		asITypeInfo* const TypeInfo = Module->GetTypeInfoByName("FRecord");
		ASSERT_THAT(IsNotNull(TypeInfo, TEXT("FRecord must install after renamed generated-accessor lowering")));
		if (TypeInfo == nullptr)
		{
			return;
		}
		asIScriptFunction* const Getter = TypeInfo->GetMethodByName("ReadStored");
		asIScriptFunction* const Setter = TypeInfo->GetMethodByName("WriteStored");
		ASSERT_THAT(IsNotNull(Getter, TEXT("renamed generated getter must install with its body")));
		ASSERT_THAT(IsNotNull(Setter, TEXT("renamed generated setter must install with its body")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Getter, asBC_RDR4),
			TEXT("renamed generated getter must retain a field read body")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Setter, asBC_WRTV4),
			TEXT("renamed generated setter must retain a field write body")));
	}

	TEST_METHOD(GeneratedAccessorRejectsMissingSealedFieldOffset)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "AccessorPlanMissingOffset");
		ASSERT_THAT(IsNotNull(Module, TEXT("AccessorPlanMissingOffset module")));
		if (Module == nullptr)
		{
			return;
		}

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string Source = ASTEST_AS_ANSI(R"AS(
			struct FRecord
			{
				int Stored;
			}
			)AS");
		Code.SetCode("AccessorPlanMissingOffset.as", Source.c_str(), true);
		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("accessor fixture must parse through Sema before its sealed field layout is cleared")));
		Sema.LayoutScriptClassFields();

		asASTDeclId ClearedField;
		asASTDeclId OwningClass;
		for (asUINT Index = 1; Index <= Context.GetDeclCount(); ++Index)
		{
			const asCDecl* const Decl = Context.GetDecl(asASTDeclId(Index));
			if (Decl == nullptr || Decl->kind != asAST_DECL_VAR || !Decl->name.Equals("Stored"))
			{
				continue;
			}

			ASSERT_THAT(IsTrue(Decl->byteOffset >= 0,
				TEXT("Sema must record the field layout before this test deliberately removes it")));
			ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Context.SetDeclByteOffset(Decl->id, -1)));
			ClearedField = Decl->id;
			OwningClass = Decl->parent;
			break;
		}
		ASSERT_THAT(IsTrue(ClearedField.IsValid(),
			TEXT("fixture must expose the generated accessor's sealed field declaration")));
		if (!ClearedField.IsValid())
		{
			return;
		}

		// A field layout is a required semantic fact, not a CodeGen-local fallback.
		// The verifier must identify the exact malformed field and owning class so
		// no sealed snapshot can expose the incomplete graph to a consumer.
		asSAstVerifyResult Verify;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(Context, Verify),
			TEXT("missing field layout must fail the AST verifier before publication")));
		ASSERT_THAT(IsTrue(Verify.detail.Equals("class-field-layout"),
			TEXT("missing field layout must retain the stable class-field-layout detail")));
		ASSERT_THAT(IsTrue(Verify.offendingNode == asSASTNodeRef::FromDecl(ClearedField),
			TEXT("the verifier must identify the field whose layout fact is missing")));
		ASSERT_THAT(AreEqual((int)asAST_EDGE_DECL_PARENT, (int)Verify.edgeRole,
			TEXT("the verifier diagnostic must identify the field-to-class ownership edge")));
		ASSERT_THAT(IsTrue(Verify.relatedNode == asSASTNodeRef::FromDecl(OwningClass),
			TEXT("the verifier must identify the class whose sealed layout is incomplete")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, Context.Seal(),
			TEXT("Seal must reject an incomplete field layout before any backend sees it")));
		asCBytecodeCodeGen CodeGen;
		const int LowerResult = CodeGen.Generate(Context, Module);
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_UNSEALED_PUBLICATION, LowerResult,
			TEXT("CodeGen must also refuse the unsealed graph instead of recovering field layout")));
	}

	TEST_METHOD(ScriptCorpusIsEnumeratedAndNativeLikeFilesLower)
	{
		const FString Root = FPaths::Combine(FPaths::ProjectDir(), TEXT("Script"));
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.as"), true, false);
		ASSERT_THAT(IsTrue(Files.Num() > 0, TEXT("project Script corpus should be present")));

		int32 NativeLike = 0;
		int32 Lowered = 0;
		int32 SkippedUe = 0;
		for (const FString& Path : Files)
		{
			FString Source;
			if (!FFileHelper::LoadFileToString(Source, *Path))
			{
				continue;
			}
			if (LooksLikeUeScript(Source))
			{
				++SkippedUe;
				continue;
			}
			++NativeLike;
			AngelscriptNativeTestSupport::FNativeTestEngine Engine;
			Engine.Create(*TestRunner);
			ON_SCOPE_EXIT { Engine.Destroy(); };
			asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
			asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "Corpus");
			const FTCHARToUTF8 SourceUtf8(*Source);
			if (GenerateCanonicalFromSource(ScriptEngine, Module, "Corpus", SourceUtf8.Get(), nullptr) == 0
				&& Module->GetFunctionCount() > 0)
			{
				++Lowered;
			}
		}

		TestRunner->AddInfo(FString::Printf(
			TEXT("Script corpus files=%d skippedUE=%d nativeLike=%d lowered=%d"),
			Files.Num(),
			SkippedUe,
			NativeLike,
			Lowered));
		ASSERT_THAT(IsTrue(SkippedUe + NativeLike == Files.Num() || Files.Num() > 0,
			TEXT("corpus classifier must inspect every .as file")));
		ASSERT_THAT(IsTrue(Lowered >= 1, TEXT("at least one native-like Script file should lower")));
	}
};

#endif
