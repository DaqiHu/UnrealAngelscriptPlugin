#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"
#include "../../Support/AngelscriptNativeBuilderTestSupport.h"

#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "StartAngelscriptHeaders.h"
#include "frontend/AST/as_ast_fwd.h"
#include "source/as_bytecode_codegen.h"
#include "as_module.h"
#include "as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FCanonicalASTCutoverTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.Cutover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(TransitionalDefaultIsLegacyRejectsDualAndAllowsCanonicalOptIn)
	{
#if defined(asCOMPILER_PIPELINE_DUAL)
		ASSERT_THAT(IsTrue(false, TEXT("production dual compiler selection must not be registered")));
#endif
		static_assert(asCOMPILER_PIPELINE_LEGACY == 0, "legacy keeps its stable public enum value");
		static_assert(asCOMPILER_PIPELINE_CANONICAL == 1, "canonical keeps its stable public enum value");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(AreEqual(asCOMPILER_PIPELINE_LEGACY, ScriptEngine->GetCompilerPipeline(),
			TEXT("new engines must retain the LEGACY default until the final Canonical cutover gate")));
		ASSERT_THAT(IsFalse(ScriptEngine->IsCanonicalBytecodeCodeGenReady(),
			TEXT("a LEGACY-default Engine must not advertise Canonical CodeGen readiness")));
		ASSERT_THAT(IsFalse(ScriptEngine->SetCompilerPipeline(static_cast<asECompilerPipeline>(2)),
			TEXT("unknown/dual pipeline values must be rejected")));
		ASSERT_THAT(AreEqual(asCOMPILER_PIPELINE_LEGACY, ScriptEngine->GetCompilerPipeline(),
			TEXT("rejected dual selection must leave the transitional LEGACY default in place")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("CANONICAL must remain available as an explicit migration/compiler selection")));
		ASSERT_THAT(AreEqual(asCOMPILER_PIPELINE_CANONICAL, ScriptEngine->GetCompilerPipeline(),
			TEXT("explicit CANONICAL selection must replace the transitional default for that engine")));
		ASSERT_THAT(IsTrue(ScriptEngine->IsCanonicalBytecodeCodeGenReady(),
			TEXT("a CANONICAL-selected Engine must advertise sealed-AST CodeGen readiness")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_LEGACY),
			TEXT("LEGACY must remain available as an explicit comparison and rollback path")));
		ASSERT_THAT(AreEqual(asCOMPILER_PIPELINE_LEGACY, ScriptEngine->GetCompilerPipeline(),
			TEXT("explicit LEGACY opt-out must replace the default pipeline for that engine")));
		ASSERT_THAT(IsFalse(ScriptEngine->IsCanonicalBytecodeCodeGenReady(),
			TEXT("a LEGACY-selected Engine must not advertise Canonical CodeGen readiness")));
	}

	TEST_METHOD(CanonicalBuildProvenanceExcludesLegacyCompiler)
	{
		// The publisher enum alone is not evidence: a future fallback could call
		// asCCompiler and then overwrite the enum during canonical publication.
		// The fork-internal counter observes the legacy backend itself.
		AngelscriptNativeTestSupport::FNativeTestEngine CanonicalEngine;
		CanonicalEngine.Create(*TestRunner);
		ON_SCOPE_EXIT { CanonicalEngine.Destroy(); };
		asCScriptEngine* const CanonicalScriptEngine = static_cast<asCScriptEngine*>(CanonicalEngine.Get());
		ASSERT_THAT(IsTrue(CanonicalScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* CanonicalModule = nullptr;
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			CanonicalEngine.Get(), "CanonicalProvenance", "int F() { return 7; }", CanonicalModule)));
		asCModule* const CanonicalConcrete = static_cast<asCModule*>(CanonicalModule);
		ASSERT_THAT(IsNotNull(CanonicalConcrete, TEXT("canonical provenance build must publish a module")));
		if( CanonicalConcrete )
		{
			ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
				CanonicalConcrete->GetLastBytecodePublisher()));
			ASSERT_THAT(AreEqual((asUINT)0, CanonicalConcrete->GetLastLegacyCompilerInvocationCount(),
				TEXT("CANONICAL Build must not invoke asCCompiler before CodeGen publication")));
			asCString CanonicalDump;
			asCBytecodeCodeGenDumpModule(CanonicalConcrete, CanonicalDump);
			const FString CanonicalDumpText = UTF8_TO_TCHAR(
				CanonicalDump.AddressOf() ? CanonicalDump.AddressOf() : "");
			ASSERT_THAT(IsTrue(CanonicalDumpText.Contains(TEXT("publisher=2"))
				&& CanonicalDumpText.Contains(TEXT("legacyCompilerInvocations=0")),
				*FString::Printf(TEXT("canonical debug dump must expose provenance:\n%s"), *CanonicalDumpText)));
		}

		AngelscriptNativeTestSupport::FNativeTestEngine LegacyEngine;
		LegacyEngine.Create(*TestRunner);
		ON_SCOPE_EXIT { LegacyEngine.Destroy(); };
		asCScriptEngine* const LegacyScriptEngine = static_cast<asCScriptEngine*>(LegacyEngine.Get());
		ASSERT_THAT(IsTrue(LegacyScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_LEGACY),
			TEXT("legacy provenance fixture must explicitly select the rollback compiler")));
		asIScriptModule* LegacyModule = nullptr;
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			LegacyEngine.Get(), "LegacyProvenance", "int F() { return 7; }", LegacyModule)));
		asCModule* const LegacyConcrete = static_cast<asCModule*>(LegacyModule);
		ASSERT_THAT(IsNotNull(LegacyConcrete, TEXT("legacy provenance build must publish a module")));
		if( LegacyConcrete )
		{
			ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_COMPILER,
				LegacyConcrete->GetLastBytecodePublisher()));
			ASSERT_THAT(IsTrue(LegacyConcrete->GetLastLegacyCompilerInvocationCount() > 0,
				TEXT("the counter must observe a real legacy asCCompiler build")));
			asCString LegacyDump;
			asCBytecodeCodeGenDumpModule(LegacyConcrete, LegacyDump);
			const FString LegacyDumpText = UTF8_TO_TCHAR(
				LegacyDump.AddressOf() ? LegacyDump.AddressOf() : "");
			ASSERT_THAT(IsTrue(LegacyDumpText.Contains(TEXT("publisher=1"))
				&& LegacyDumpText.Contains(TEXT("legacyCompilerInvocations="))
				&& !LegacyDumpText.Contains(TEXT("legacyCompilerInvocations=0")),
				*FString::Printf(TEXT("legacy debug dump must expose provenance:\n%s"), *LegacyDumpText)));
		}
	}

	TEST_METHOD(CanonicalCandidatePromotionPublishesLegacyCompilerInvocationProvenance)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		asCModule ActiveModule("CanonicalPromotionActive", ScriptEngine);
		asCModule CandidateModule("CanonicalPromotionCandidate", ScriptEngine);
		ActiveModule.RecordLegacyCompilerInvocation();
		CandidateModule.RecordLegacyCompilerInvocation();
		CandidateModule.RecordLegacyCompilerInvocation();

		ActiveModule.PromoteCanonicalBuildCandidate(CandidateModule);

		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(2),
			ActiveModule.GetLastLegacyCompilerInvocationCount(),
			TEXT("canonical candidate promotion must replace stale active provenance with the candidate's complete legacy-compiler count")));
	}

	TEST_METHOD(CanonicalRetainedBuildDumpBindsPublisherToSealedSnapshot)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("retained provenance fixture selects CANONICAL")));

		asIScriptModule* const Module = Engine.Get()->GetModule("CanonicalDigest", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("retained provenance fixture needs a module")));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT),
			TEXT("retained provenance fixture must keep the completed Canonical snapshot")));
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"CanonicalDigest.as",
			"struct FValue { int Value; } int F() { FValue Value; Value.Value = 41; return Value.Value + 1; }"),
			TEXT("retained provenance fixture source must add")));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("retained provenance fixture must build through Canonical CodeGen")));

		asCModule* const ConcreteModule = static_cast<asCModule*>(Module);
		ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			ConcreteModule->GetLastBytecodePublisher(),
			TEXT("retained provenance fixture must have a Canonical publisher")));
		ASSERT_THAT(AreEqual((asUINT)0, ConcreteModule->GetLastLegacyCompilerInvocationCount(),
			TEXT("retained provenance fixture must not invoke asCCompiler")));

		asIASTSnapshot* const Snapshot = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(Snapshot, TEXT("retained provenance fixture must publish a snapshot")));
		if (Snapshot == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Snapshot->Release(); };
		ASSERT_THAT(IsTrue(Snapshot->IsCurrentGeneration(),
			TEXT("the retained Canonical snapshot must be current")));

		const asCASTContext* const SealedContext = ConcreteModule->GetCanonicalASTContext();
		ASSERT_THAT(IsNotNull(SealedContext,
			TEXT("the retained Canonical snapshot must expose its sealed internal context for diagnostics")));
		if (SealedContext == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(SealedContext->IsPublishable(),
			TEXT("the CodeGen provenance digest must describe a sealed snapshot")));

		asCString AstDump;
		asCASTDump(*SealedContext, AstDump);
		asQWORD ExpectedDigest = 1469598103934665603ULL;
		for (const unsigned char* Cursor = reinterpret_cast<const unsigned char*>(AstDump.AddressOf());
			Cursor != nullptr && *Cursor != 0;
			++Cursor)
		{
			ExpectedDigest ^= *Cursor;
			ExpectedDigest *= 1099511628211ULL;
		}

		asCString ModuleDump;
		asCBytecodeCodeGenDumpModule(ConcreteModule, ModuleDump);
		const FString ExpectedDigestToken = FString::Printf(
			TEXT("canonicalAstDigest=%016llx"),
			static_cast<unsigned long long>(ExpectedDigest));
		const FString ModuleDumpText = UTF8_TO_TCHAR(ModuleDump.AddressOf() ? ModuleDump.AddressOf() : "");
		ASSERT_THAT(IsTrue(ModuleDumpText.Contains(ExpectedDigestToken),
			*FString::Printf(
				TEXT("Canonical module dump must bind the publisher to the exact sealed AST digest %s; dump:\n%s"),
				*ExpectedDigestToken,
				*ModuleDumpText)));
	}

	TEST_METHOD(SourceBuildPurposesSelectCanonicalAndExecute)
	{
		struct FPurpose
		{
			const char* Name;
			const char* ModuleName;
			bool (*Run)(FAutomationTestBase& Test, asCScriptEngine* ScriptEngine, int32& OutValue);
		};

		auto PrimaryBuild = [](FAutomationTestBase& Test, asCScriptEngine* ScriptEngine, int32& OutValue) -> bool
		{
			asIScriptModule* Module = nullptr;
			if (AngelscriptNativeTestSupport::CompileNativeModule(
					ScriptEngine, "PrimaryCutover", "int F() { return 7; }", Module) < 0
				|| Module == nullptr)
			{
				return false;
			}
			return AngelscriptNativeTestSupport::CanonicalExecuteInt(Test, ScriptEngine, Module, "int F()", OutValue);
		};
		auto SingleFunction = [](FAutomationTestBase& Test, asCScriptEngine* ScriptEngine, int32& OutValue) -> bool
		{
			asIScriptModule* const Module = ScriptEngine->GetModule("SingleFnCutover", asGM_ALWAYS_CREATE);
			if (Module == nullptr)
			{
				return false;
			}
			asIScriptFunction* Function = nullptr;
			if (Module->CompileFunction("SingleFn", "int Extra() { return 11; }", 0, asCOMP_ADD_TO_MODULE, &Function) < 0
				|| Function == nullptr)
			{
				return false;
			}
			return AngelscriptNativeTestSupport::CanonicalExecuteInt(
				Test, ScriptEngine, Module, "int Extra()", OutValue);
		};
		auto HotReloadReplacement = [](FAutomationTestBase& Test, asCScriptEngine* ScriptEngine, int32& OutValue) -> bool
		{
			asIScriptModule* First = nullptr;
			if (AngelscriptNativeTestSupport::CompileNativeModule(
					ScriptEngine, "ReloadCutover", "int F() { return 1; }", First) < 0)
			{
				return false;
			}
			asIScriptModule* Second = nullptr;
			if (AngelscriptNativeTestSupport::CompileNativeModule(
					ScriptEngine, "ReloadCutover", "int F() { return 9; }", Second) < 0
				|| Second == nullptr)
			{
				return false;
			}
			return AngelscriptNativeTestSupport::CanonicalExecuteInt(Test, ScriptEngine, Second, "int F()", OutValue);
		};
		auto GenerationLike = [](FAutomationTestBase& Test, asCScriptEngine* ScriptEngine, int32& OutValue) -> bool
		{
			asIScriptModule* Module = nullptr;
			if (AngelscriptNativeTestSupport::CompileNativeModule(
					ScriptEngine, "GenerateCutover", "int F() { return 5; }", Module) < 0
				|| Module == nullptr)
			{
				return false;
			}
			if (ScriptEngine->GetCompilerPipeline() != asCOMPILER_PIPELINE_CANONICAL)
			{
				return false;
			}
			return AngelscriptNativeTestSupport::CanonicalExecuteInt(Test, ScriptEngine, Module, "int F()", OutValue);
		};
		auto CommandletLike = [](FAutomationTestBase& Test, asCScriptEngine* ScriptEngine, int32& OutValue) -> bool
		{
			asIScriptModule* Module = nullptr;
			if (AngelscriptNativeTestSupport::CompileNativeModule(
					ScriptEngine, "CommandletCutover", "int F() { return 3; }", Module) < 0
				|| Module == nullptr)
			{
				return false;
			}
			return AngelscriptNativeTestSupport::CanonicalExecuteInt(Test, ScriptEngine, Module, "int F()", OutValue);
		};
		auto StandaloneLike = [](FAutomationTestBase& Test, asCScriptEngine* ScriptEngine, int32& OutValue) -> bool
		{
			asIScriptModule* Module = nullptr;
			if (AngelscriptNativeTestSupport::CompileNativeModule(
					ScriptEngine, "StandaloneCutover", "int F() { return 4; }", Module) < 0
				|| Module == nullptr)
			{
				return false;
			}
			return AngelscriptNativeTestSupport::CanonicalExecuteInt(Test, ScriptEngine, Module, "int F()", OutValue);
		};

		const FPurpose Purposes[] = {
			{ "primary", "PrimaryCutover", PrimaryBuild },
			{ "Hot Reload", "ReloadCutover", HotReloadReplacement },
			{ "single-function compile", "SingleFnCutover", SingleFunction },
			{ "generation", "GenerateCutover", GenerationLike },
			{ "commandlet", "CommandletCutover", CommandletLike },
			{ "Standalone", "StandaloneCutover", StandaloneLike },
		};

		for (const FPurpose& Purpose : Purposes)
		{
			AngelscriptNativeTestSupport::FNativeTestEngine Engine;
			Engine.Create(*TestRunner);
			ON_SCOPE_EXIT { Engine.Destroy(); };
			asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
			ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
				*FString::Printf(TEXT("%hs purpose must be able to select CANONICAL capture"), Purpose.Name)));
			ASSERT_THAT(IsTrue(ScriptEngine->IsCanonicalBytecodeCodeGenReady(),
				*FString::Printf(TEXT("%hs purpose reports CodeGen-ready because CANONICAL Build() calls Generate"), Purpose.Name)));
			int32 Value = 0;
			ASSERT_THAT(IsTrue(Purpose.Run(*TestRunner, ScriptEngine, Value),
				*FString::Printf(TEXT("%hs purpose must compile and execute after CANONICAL selection"), Purpose.Name)));
			ASSERT_THAT(IsTrue(Value != 0,
				*FString::Printf(TEXT("%hs purpose must publish executable bytecode"), Purpose.Name)));
			asCModule* const Concrete = static_cast<asCModule*>(
				ScriptEngine->GetModule(Purpose.ModuleName, asGM_ONLY_IF_EXISTS));
			ASSERT_THAT(IsNotNull(Concrete,
				*FString::Printf(TEXT("%hs purpose must leave a concrete module"), Purpose.Name)));
			ASSERT_THAT(AreEqual(
				asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
				Concrete->GetLastBytecodePublisher(),
				*FString::Printf(
					TEXT("%hs must publish Bytecode from the sealed Canonical AST CodeGen path"),
					Purpose.Name)));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(0),
				Concrete->GetLastLegacyCompilerInvocationCount(),
				*FString::Printf(
					TEXT("%hs must not instantiate the legacy asCCompiler"),
					Purpose.Name)));
			asQWORD CanonicalDigest = 0;
			ASSERT_THAT(IsTrue(
				Concrete->GetLastCanonicalCodeGenAstDigest(CanonicalDigest)
					&& CanonicalDigest != 0,
				*FString::Printf(
					TEXT("%hs must bind CodeGen provenance to a sealed AST digest"),
					Purpose.Name)));
		}
	}

	TEST_METHOD(ProductionDefaultHasNoDualOrLlvm)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsFalse(ScriptEngine->SetCompilerPipeline(static_cast<asECompilerPipeline>(99)),
			TEXT("no dual/unknown compiler selection")));

		const FString ForkRoot = FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source"));
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *ForkRoot, TEXT("*.cpp"), true, false);
		IFileManager::Get().FindFilesRecursive(Files, *ForkRoot, TEXT("*.h"), true, false);
		int32 Scanned = 0;
		for (const FString& Path : Files)
		{
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *Path))
			{
				continue;
			}
			++Scanned;
			ASSERT_THAT(IsFalse(Text.Contains(TEXT("llvm::")),
				*FString::Printf(TEXT("%s must not include LLVM types"), *Path)));
			ASSERT_THAT(IsFalse(Text.Contains(TEXT("clangAST")),
				*FString::Printf(TEXT("%s must not include Clang AST types"), *Path)));
			ASSERT_THAT(IsFalse(Text.Contains(TEXT("asCOMPILER_PIPELINE_DUAL")),
				*FString::Printf(TEXT("%s must not register dual pipeline"), *Path)));
			const FString FileName = FPaths::GetCleanFilename(Path);
			const bool bSemaOrAstBackend =
				FileName.StartsWith(TEXT("as_sema"))
				|| FileName.StartsWith(TEXT("as_ast"))
				|| FileName.StartsWith(TEXT("as_bytecode_codegen"))
				|| FileName.StartsWith(TEXT("as_decl"))
				|| FileName.StartsWith(TEXT("as_stmt"))
				|| FileName.StartsWith(TEXT("as_expr"));
			if (bSemaOrAstBackend)
			{
				ASSERT_THAT(IsFalse(Text.Contains(TEXT("#include \"as_compiler.h\"")),
					*FString::Printf(
						TEXT("%s must not include the legacy bytecode compiler"), *Path)));
			}
		}
		ASSERT_THAT(IsTrue(Scanned > 0, TEXT("forbidden-symbol scan must inspect the maintained fork")));
	}

	TEST_METHOD(CompileFunctionAddToModuleRetiresIncompleteSnapshot)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asIScriptModule* const Module = Engine.Get()->GetModule("CompileFnRetain", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("CompileFunction retain module")));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection("CompileFnRetain", "int F() { return 1; }")));
		ASSERT_THAT(IsTrue(
			static_cast<asCScriptEngine*>(Engine.Get())->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("CompileFunction retain test selects CANONICAL capture explicitly")));
		ASSERT_THAT(AreEqual(0, Module->Build(), TEXT("module Build must succeed under CANONICAL selection")));
		asCModule* const ConcreteModule = static_cast<asCModule*>(Module);
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			ConcreteModule->GetLastBytecodePublisher(),
			TEXT("CANONICAL retain Build must record CodeGen")));
		asIASTSnapshot* const Before = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(Before, TEXT("retain Build must publish AST V1")));
		const FString KeyBefore = UTF8_TO_TCHAR(Before->GetGenerationKey());

		asIScriptFunction* Extra = nullptr;
		ASSERT_THAT(AreEqual(0, Module->CompileFunction(
			"CompileFnRetainExtra",
			"int Extra() { return 11; }",
			0,
			asCOMP_ADD_TO_MODULE,
			&Extra)));
		ASSERT_THAT(IsNotNull(Extra, TEXT("CompileFunction must publish the extra function")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			ConcreteModule->GetLastBytecodePublisher(),
			TEXT("CANONICAL CompileFunction must publish from asCBytecodeCodeGen")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			ConcreteModule->GetLastLegacyCompilerInvocationCount(),
			TEXT("CANONICAL CompileFunction must not instantiate asCCompiler")));
		asQWORD CompileFunctionDigest = 0;
		ASSERT_THAT(IsTrue(
			ConcreteModule->GetLastCanonicalCodeGenAstDigest(CompileFunctionDigest)
				&& CompileFunctionDigest != 0,
			TEXT("CompileFunction CodeGen must record the exact sealed ephemeral AST digest")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Extra()", Value)
				&& Value == 11,
			TEXT("CompileFunction bytecode must execute")));
		// The ephemeral single-function AST is a complete CodeGen input, but it is
		// not a complete replacement for the retained module graph. The previous
		// snapshot therefore becomes an old but readable lease; a fresh acquire
		// must not expose it as a complete current generation.
		ASSERT_THAT(IsTrue(!Before->IsCurrentGeneration(),
			TEXT("ADD_TO_MODULE must retire the now-incomplete retained snapshot")));
		ASSERT_THAT(AreEqual(KeyBefore, FString(UTF8_TO_TCHAR(Before->GetGenerationKey())),
			TEXT("retired snapshot lease must remain immutable and readable")));
		asIASTSnapshot* const Fresh = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNull(Fresh,
			TEXT("fresh acquire must not publish the incomplete pre-CompileFunction snapshot")));
		Before->Release();
		Extra->Release();
	}

	TEST_METHOD(CanonicalCompileFunctionDetachedKeepsCompleteSnapshotCurrent)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("detached CompileFunction fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"CompileFnDetached", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("detached CompileFunction owner module")));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string BaselineSource = ASTEST_AS_ANSI(R"AS(
			int Baseline()
			{
				return 1;
			}
		)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"CompileFnDetachedBaseline.as", BaselineSource.c_str())));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("detached CompileFunction baseline must publish a complete snapshot")));

		asCModule* const ConcreteModule = static_cast<asCModule*>(Module);
		asIASTSnapshot* const Before = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(Before, TEXT("detached baseline snapshot")));
		if (Before == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Before->Release(); };
		const FString GenerationBefore = UTF8_TO_TCHAR(Before->GetGenerationKey());
		const asUINT FunctionCountBefore = Module->GetFunctionCount();

		const std::string DetachedSource = ASTEST_AS_ANSI(R"AS(
			int DetachedExtra()
			{
				return 13;
			}
		)AS");
		asIScriptFunction* Detached = nullptr;
		ASSERT_THAT(AreEqual(0, Module->CompileFunction(
			"CompileFnDetachedExtra.as",
			DetachedSource.c_str(),
			3,
			0,
			&Detached)));
		ASSERT_THAT(IsNotNull(Detached, TEXT("detached CompileFunction result")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(FunctionCountBefore),
			static_cast<int32>(Module->GetFunctionCount()),
			TEXT("detached CodeGen must not mutate module function inventory")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			ConcreteModule->GetLastBytecodePublisher(),
			TEXT("detached CANONICAL CompileFunction must publish from CodeGen")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			ConcreteModule->GetLastLegacyCompilerInvocationCount(),
			TEXT("detached CANONICAL CompileFunction must not instantiate asCCompiler")));
		asQWORD DetachedDigest = 0;
		ASSERT_THAT(IsTrue(
			ConcreteModule->GetLastCanonicalCodeGenAstDigest(DetachedDigest)
				&& DetachedDigest != 0,
			TEXT("detached CodeGen must retain sealed ephemeral AST provenance")));

		if (Detached != nullptr)
		{
			asIScriptContext* const Context = Engine.Get()->CreateContext();
			ASSERT_THAT(IsNotNull(Context, TEXT("detached function execution context")));
			if (Context != nullptr)
			{
				ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Detached)));
				ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
				ASSERT_THAT(AreEqual(13, static_cast<int32>(Context->GetReturnDWord())));
				Context->Release();
			}
			Detached->Release();
		}

		ASSERT_THAT(IsTrue(Before->IsCurrentGeneration(),
			TEXT("detached compilation must leave the complete module snapshot current")));
		asIASTSnapshot* const Fresh = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(Fresh, TEXT("detached compilation must preserve fresh acquire")));
		if (Fresh != nullptr)
		{
			ASSERT_THAT(AreEqual(
				GenerationBefore,
				FString(UTF8_TO_TCHAR(Fresh->GetGenerationKey())),
				TEXT("detached compilation must not replace the complete generation")));
			Fresh->Release();
		}
	}

	TEST_METHOD(CanonicalCompileFunctionFailureLeavesPublishedGenerationUntouched)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("failure-atomic CompileFunction fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"CompileFnAtomicFailure", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("failure-atomic CompileFunction module")));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"CompileFnAtomicFailure_Baseline.as", "int Stable() { return 7; }")));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("failure-atomic baseline must publish")));

		asCModule* const ConcreteModule = static_cast<asCModule*>(Module);
		asIScriptFunction* const StableBefore = Module->GetFunctionByDecl("int Stable()");
		ASSERT_THAT(IsNotNull(StableBefore, TEXT("failure-atomic baseline function")));
		asIASTSnapshot* const Before = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(Before, TEXT("failure-atomic baseline snapshot")));
		if (Before == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Before->Release(); };
		const FString GenerationBefore = UTF8_TO_TCHAR(Before->GetGenerationKey());
		const asUINT ModuleFunctionCountBefore = Module->GetFunctionCount();

		TArray<asCScriptFunction*> EngineFunctionSlotsBefore;
		for (asUINT Index = 0; Index < ScriptEngine->scriptFunctions.GetLength(); ++Index)
		{
			EngineFunctionSlotsBefore.Add(ScriptEngine->scriptFunctions[Index]);
		}
		TArray<int32> EngineFreeIdsBefore;
		for (asUINT Index = 0; Index < ScriptEngine->freeScriptFunctionIds.GetLength(); ++Index)
		{
			EngineFreeIdsBefore.Add(ScriptEngine->freeScriptFunctionIds[Index]);
		}

		const std::string RejectedSource = ASTEST_AS_ANSI(R"AS(
			int Rejected()
			{
				try
				{
					return 1;
				}
				catch
				{
					return 0;
				}
			}
		)AS");
		asIScriptFunction* Rejected = StableBefore;
		ASSERT_THAT(IsTrue(Module->CompileFunction(
			"CompileFnAtomicFailure_Rejected.as",
			RejectedSource.c_str(),
			0,
			asCOMP_ADD_TO_MODULE,
			&Rejected) < 0,
			TEXT("unsupported CompileFunction must fail before publication")));
		ASSERT_THAT(IsNull(Rejected,
			TEXT("failed CompileFunction must clear the caller output")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(ModuleFunctionCountBefore),
			static_cast<int32>(Module->GetFunctionCount()),
			TEXT("failed CompileFunction must not change module function inventory")));
		ASSERT_THAT(IsTrue(Module->GetFunctionByDecl("int Stable()") == StableBefore,
			TEXT("failed CompileFunction must retain the exact published function")));
		ASSERT_THAT(AreEqual(
			EngineFunctionSlotsBefore.Num(),
			static_cast<int32>(ScriptEngine->scriptFunctions.GetLength()),
			TEXT("failed CompileFunction must not grow Engine function slots")));
		for (int32 Index = 0; Index < EngineFunctionSlotsBefore.Num(); ++Index)
		{
			ASSERT_THAT(IsTrue(
				ScriptEngine->scriptFunctions[static_cast<asUINT>(Index)]
					== EngineFunctionSlotsBefore[Index],
				TEXT("failed CompileFunction must preserve every Engine function slot")));
		}
		ASSERT_THAT(AreEqual(
			EngineFreeIdsBefore.Num(),
			static_cast<int32>(ScriptEngine->freeScriptFunctionIds.GetLength()),
			TEXT("failed CompileFunction must not mutate the Engine free-id stack")));
		for (int32 Index = 0; Index < EngineFreeIdsBefore.Num(); ++Index)
		{
			ASSERT_THAT(AreEqual(
				EngineFreeIdsBefore[Index],
				ScriptEngine->freeScriptFunctionIds[static_cast<asUINT>(Index)],
				TEXT("failed CompileFunction must preserve Engine free-id ordering")));
		}
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			ConcreteModule->GetLastLegacyCompilerInvocationCount(),
			TEXT("failed CANONICAL CompileFunction must not instantiate asCCompiler")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_NONE,
			ConcreteModule->GetLastBytecodePublisher(),
			TEXT("failed CompileFunction must not claim a Bytecode publisher")));
		asQWORD RejectedDigest = 0;
		ASSERT_THAT(IsFalse(ConcreteModule->GetLastCanonicalCodeGenAstDigest(RejectedDigest),
			TEXT("failed CompileFunction must not retain a false CodeGen digest")));

		int32 StableValue = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Stable()", StableValue)
				&& StableValue == 7,
			TEXT("failed CompileFunction must leave the prior generation executable")));
		ASSERT_THAT(IsTrue(Before->IsCurrentGeneration(),
			TEXT("failed attached CompileFunction must keep the prior snapshot current")));
		asIASTSnapshot* const Fresh = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(Fresh, TEXT("failed CompileFunction must preserve fresh snapshot acquire")));
		if (Fresh != nullptr)
		{
			ASSERT_THAT(AreEqual(
				GenerationBefore,
				FString(UTF8_TO_TCHAR(Fresh->GetGenerationKey())),
				TEXT("failed CompileFunction must preserve the exact snapshot generation")));
			Fresh->Release();
		}
	}

	TEST_METHOD(CanonicalCompileFunctionOwnsNestedLambdaClosureWithoutExposingIt)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("nested-lambda CompileFunction fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"CompileFnLambda", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("CompileFunction lambda module")));
		const asUINT PublicCountBefore = Module->GetFunctionCount();
		const asUINT InternalCountBefore = static_cast<asCModule*>(Module)->scriptFunctions.GetLength();
		const std::string AttachedSource = ASTEST_AS_ANSI(R"AS(
			int AttachedLambda()
			{
				int X = 41;
				return function()
				{
					return X + 1;
				}();
			}
		)AS");
		asIScriptFunction* Attached = nullptr;
		ASSERT_THAT(AreEqual(0, Module->CompileFunction(
			"CompileFnLambda_Attached.as",
			AttachedSource.c_str(),
			0,
			asCOMP_ADD_TO_MODULE,
			&Attached)));
		ASSERT_THAT(IsNotNull(Attached, TEXT("attached lambda CompileFunction result")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(PublicCountBefore + 1),
			static_cast<int32>(Module->GetFunctionCount()),
			TEXT("only the direct function may enter the public module inventory")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(InternalCountBefore + 2),
			static_cast<int32>(static_cast<asCModule*>(Module)->scriptFunctions.GetLength()),
			TEXT("the module must own the direct function and its nested lambda")));
		ASSERT_THAT(IsNull(Module->GetFunctionByName("<lambda>"),
			TEXT("a nested lambda must not be exposed as a global module function")));
		int32 AttachedValue = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int AttachedLambda()", AttachedValue)
				&& AttachedValue == 42,
			TEXT("attached CompileFunction lambda closure must execute")));
		if (Attached != nullptr)
		{
			Attached->Release();
		}

		const asUINT PublicCountAttached = Module->GetFunctionCount();
		const asUINT InternalCountAttached = static_cast<asCModule*>(Module)->scriptFunctions.GetLength();
		const std::string DetachedSource = ASTEST_AS_ANSI(R"AS(
			int DetachedLambda()
			{
				int X = 0;
				return AttachedLambda() + function(int Y)
				{
					return X + Y;
				}(0);
			}
		)AS");
		asIScriptFunction* Detached = nullptr;
		ASSERT_THAT(AreEqual(0, Module->CompileFunction(
			"CompileFnLambda_Detached.as",
			DetachedSource.c_str(),
			0,
			0,
			&Detached)));
		ASSERT_THAT(IsNotNull(Detached, TEXT("detached lambda CompileFunction result")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(PublicCountAttached),
			static_cast<int32>(Module->GetFunctionCount()),
			TEXT("detached lambda closure must not change public module inventory")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(InternalCountAttached),
			static_cast<int32>(static_cast<asCModule*>(Module)->scriptFunctions.GetLength()),
			TEXT("detached lambda closure must not change internal module ownership")));
		if (Detached != nullptr)
		{
			ASSERT_THAT(IsNull(static_cast<asCScriptFunction*>(Detached)->module,
				TEXT("detached primary function must not retain module ownership")));
			asIScriptContext* const Context = Engine.Get()->CreateContext();
			ASSERT_THAT(IsNotNull(Context, TEXT("detached lambda execution context")));
			if (Context != nullptr)
			{
				ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Detached)));
				ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
				ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()),
					TEXT("detached function must bind both its lambda and an existing module function")));
				Context->Release();
			}
			Detached->Release();
		}
	}

	TEST_METHOD(CanonicalCompileFunctionBindsCurrentModuleFunctionsAndGlobals)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("module-scope CompileFunction fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"CompileFnModuleScope", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("CompileFunction module-scope owner")));
		const std::string BaselineSource = ASTEST_AS_ANSI(R"AS(
			int CurrentValue = 40;
			int CurrentFunction()
			{
				return 2;
			}
		)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"CompileFnModuleScope_Baseline.as", BaselineSource.c_str())));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("current-module function/global baseline must build")));

		const asUINT FunctionCountBefore = Module->GetFunctionCount();
		const asUINT GlobalCountBefore = Module->GetGlobalVarCount();
		const std::string DetachedSource = ASTEST_AS_ANSI(R"AS(
			int ReadCurrentModuleScope()
			{
				return CurrentValue + CurrentFunction();
			}
		)AS");
		asIScriptFunction* Detached = nullptr;
		ASSERT_THAT(AreEqual(0, Module->CompileFunction(
			"CompileFnModuleScope_Detached.as",
			DetachedSource.c_str(),
			0,
			0,
			&Detached)));
		ASSERT_THAT(IsNotNull(Detached, TEXT("module-scope detached CompileFunction result")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(FunctionCountBefore),
			static_cast<int32>(Module->GetFunctionCount()),
			TEXT("module-scope views must not republish functions")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(GlobalCountBefore),
			static_cast<int32>(Module->GetGlobalVarCount()),
			TEXT("module-scope views must not republish globals")));
		if (Detached != nullptr)
		{
			asIScriptContext* const Context = Engine.Get()->CreateContext();
			ASSERT_THAT(IsNotNull(Context, TEXT("module-scope detached execution context")));
			if (Context != nullptr)
			{
				ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Detached)));
				ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
				ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()),
					TEXT("detached CompileFunction must bind current-module function and global views")));
				Context->Release();
			}
			Detached->Release();
		}
	}

	TEST_METHOD(CanonicalCompileFunctionProjectsCurrentModuleDefaultsForNamedCall)
	{
		// Production mutation caught: dropping Runtime defaults while projecting
		// current-module functions makes Pack(B: 7) reject omitted formal A.
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("CompileFunction default-projection fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"CompileFnDefaultProjection", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const std::string BaselineSource = ASTEST_AS_ANSI(R"AS(
			int Pack(int A = 1, int B = 2)
			{
				return A * 10 + B;
			}
		)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"CompileFnDefaultProjection_Baseline.as",
			BaselineSource.c_str())));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("CompileFunction default-projection baseline must build")));

		const std::string DetachedSource = ASTEST_AS_ANSI(R"AS(
			int ReadProjectedDefault()
			{
				return Pack(B: 7);
			}
		)AS");
		asIScriptFunction* Detached = nullptr;
		const int CompileResult = Module->CompileFunction(
			"CompileFnDefaultProjection_Detached.as",
			DetachedSource.c_str(),
			0,
			0,
			&Detached);
		ASSERT_THAT(AreEqual(0, CompileResult,
			TEXT("CompileFunction must project current-module typed defaults")));
		ASSERT_THAT(IsNotNull(Detached));
		if (CompileResult < 0 || Detached == nullptr)
		{
			return;
		}

		asIScriptContext* const Context = Engine.Get()->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		if (Context != nullptr)
		{
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Detached)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(AreEqual(17,
				static_cast<int32>(Context->GetReturnDWord()),
				TEXT("projected Pack(B: 7) must execute A=1 B=7")));
			Context->Release();
		}
		Detached->Release();
	}

	TEST_METHOD(CanonicalCompileFunctionRuntimeShellOwnsAuthoredDefault)
	{
		// Production mutation caught: CompileFunction uses detached CodeGen, so
		// an unconditional null defaultArgs entry loses its public metadata.
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("CompileFunction default-metadata fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"CompileFnDefaultMetadata", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const std::string FunctionSource = ASTEST_AS_ANSI(R"AS(
			int OwnDefault(int Value = 7)
			{
				return Value;
			}
		)AS");
		asIScriptFunction* Function = nullptr;
		ASSERT_THAT(AreEqual(0, Module->CompileFunction(
			"CompileFnDefaultMetadata_Attached.as",
			FunctionSource.c_str(),
			0,
			asCOMP_ADD_TO_MODULE,
			&Function),
			TEXT("attached CompileFunction default fixture must compile")));
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}

		const char* DefaultValue = nullptr;
		ASSERT_THAT(AreEqual(asSUCCESS,
			Function->GetParam(0, nullptr, nullptr, nullptr, &DefaultValue),
			TEXT("CompileFunction default metadata must be queryable")));
		ASSERT_THAT(AreEqual(FString(TEXT("7")),
			FString(UTF8_TO_TCHAR(
				DefaultValue != nullptr ? DefaultValue : "")),
			TEXT("CompileFunction Runtime shell must own authored default =7")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CompileFunction default metadata must come from Canonical CodeGen")));
	}

	TEST_METHOD(CanonicalCompileFunctionBindsCurrentModuleValueParameterABI)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("value-parameter CompileFunction fixture selects CANONICAL")));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"CompileFnValueParameterABI", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		const std::string BaselineSource = ASTEST_AS_ANSI(R"AS(
			int ReadCompileValue(int Value)
			{
				return Value;
			}
		)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"CompileFnValueParameterABI_Baseline.as",
			BaselineSource.c_str())));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("value-parameter CompileFunction baseline must build")));

		const std::string DetachedSource = ASTEST_AS_ANSI(R"AS(
			int ReadCurrentModuleValueParameterABI()
			{
				return ReadCompileValue(42);
			}
		)AS");
		asIScriptFunction* Detached = nullptr;
		ASSERT_THAT(AreEqual(0, Module->CompileFunction(
			"CompileFnValueParameterABI_Detached.as",
			DetachedSource.c_str(), 0, 0, &Detached),
			TEXT("CompileFunction must bind a source by-value formal to its normalized Runtime shell")));
		ASSERT_THAT(IsNotNull(Detached));
		if (Detached != nullptr)
		{
			asIScriptContext* const Context = Engine.Get()->CreateContext();
			ASSERT_THAT(IsNotNull(Context));
			if (Context != nullptr)
			{
				ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Detached)));
				ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
				ASSERT_THAT(AreEqual(
					42, static_cast<int32>(Context->GetReturnDWord()),
					TEXT("CompileFunction must execute through the exact current-module Runtime ABI")));
				Context->Release();
			}
			Detached->Release();
		}
	}

	TEST_METHOD(CanonicalSelectionPublishesValueObjectsFromCodeGen)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("value-object compile may select CANONICAL")));
		ASSERT_THAT(IsTrue(ScriptEngine->IsCanonicalBytecodeCodeGenReady(),
			TEXT("Ready is a binary capability because CANONICAL Build() calls Generate")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FValue
			{
				int Value;
				FValue()
				{
					Value = 41;
				}
			}

			int F()
			{
				FValue Object;
				return Object.Value + 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"ValueObjectCutover",
			ScriptSource.c_str(),
			Module),
			TEXT("CANONICAL value-object Build must succeed from CodeGen")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 42,
			TEXT("CANONICAL value-object F() must execute from CodeGen")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL value-object Bytecode must be published by CodeGen, not asCCompiler")));
	}
};

#endif
