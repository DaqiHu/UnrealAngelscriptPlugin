#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_dump.h"
#include "source/as_ast_sidecar.h"
#include "source/as_ast_verifier.h"
#include "source/as_decl.h"
#include "source/as_expr.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#include <cstring>

#if WITH_ANGELSCRIPT_UNITTESTS

// CTA-S175 ports the historical imported-call target oracle without retaining
// an Engine-local import slot or mutable bound function in Canonical AST. The
// Runtime binding can change; the sealed ImportDecl and Call relation cannot.
namespace CanonicalASTImportBindingImmutabilityTest
{
	static constexpr const char* ProviderAName =
		"CanonicalASTImportImmutabilityProviderA";
	static constexpr const char* ProviderBName =
		"CanonicalASTImportImmutabilityProviderB";
	static constexpr const char* ConsumerName =
		"CanonicalASTImportImmutabilityConsumer";

	struct FFixture
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		asCScriptEngine* ScriptEngine = nullptr;
		asIScriptModule* ProviderA = nullptr;
		asIScriptModule* ProviderB = nullptr;
		asCModule* Consumer = nullptr;

		bool Build(FAutomationTestBase& Test)
		{
			using namespace AngelscriptNativeTestSupport;

			Engine.Create(Test);
			ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
			if (ScriptEngine == nullptr
				|| !ScriptEngine->SetCompilerPipeline(
					asCOMPILER_PIPELINE_CANONICAL))
			{
				return false;
			}
			const int ProviderAResult = CompileNativeModule(
				ScriptEngine,
				ProviderAName,
				"int ImportedValue() { return 11; }",
				ProviderA);
			const int ProviderBResult = CompileNativeModule(
				ScriptEngine,
				ProviderBName,
				"int ImportedValue() { return 29; }",
				ProviderB);
			asIScriptModule* const ConsumerInterface = ScriptEngine->GetModule(
				ConsumerName, asGM_ALWAYS_CREATE);
			const int RetentionResult = ConsumerInterface != nullptr
				? ConsumerInterface->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)
				: asERROR;
			const int SectionResult = ConsumerInterface != nullptr
				? ConsumerInterface->AddScriptSection(
					"CanonicalASTImportImmutabilityConsumer.as",
					"import int ImportedValue() from "
					"\"CanonicalASTImportImmutabilityProviderA\";\n"
					"int RunImported() { return ImportedValue(); }")
				: asERROR;
			const int ConsumerResult = ConsumerInterface != nullptr
				&& RetentionResult >= 0 && SectionResult >= 0
					? ConsumerInterface->Build() : asERROR;
			Consumer = static_cast<asCModule*>(ConsumerInterface);
			return ProviderAResult >= 0 && ProviderBResult >= 0
				&& RetentionResult >= 0 && SectionResult >= 0
				&& ConsumerResult >= 0 && ProviderA != nullptr
				&& ProviderB != nullptr && Consumer != nullptr;
		}
	};

	struct FSnapshotFingerprint
	{
		FString Dump;
		asCArray<asBYTE> Bytes;
	};

	static bool CaptureFingerprint(
		const asCASTContext& Context,
		FSnapshotFingerprint& Out)
	{
		asCString RawDump;
		asCASTDump(Context, RawDump);
		Out.Dump =
			AngelscriptNativeTestSupport::CanonicalAstDumpToFString(RawDump);
		asSASTSidecarHeader Header;
		Header.functionKey = "CanonicalASTImportImmutabilityConsumer";
		Header.profile = "Win64-ImportBindingImmutability";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		return asCASTEncodeSidecar(Context, Header, Out.Bytes)
			== asAST_SIDECAR_OK;
	}

	static bool FingerprintsEqual(
		const FSnapshotFingerprint& Left,
		const FSnapshotFingerprint& Right)
	{
		return Left.Dump == Right.Dump
			&& Left.Bytes.GetLength() == Right.Bytes.GetLength()
			&& (Left.Bytes.GetLength() == 0 || std::memcmp(
				Left.Bytes.AddressOf(), Right.Bytes.AddressOf(),
				Left.Bytes.GetLength()) == 0);
	}

	static const asCDecl* FindImportDecl(const asCASTContext& Context)
	{
		const asCDecl* Result = nullptr;
		for (asUINT Index = 1; Index <= Context.GetDeclCount(); ++Index)
		{
			const asCDecl* const Candidate =
				Context.GetDecl(asASTDeclId(Index));
			if (Candidate != nullptr && Candidate->kind == asAST_DECL_IMPORT
				&& Candidate->name.Equals("ImportedValue"))
			{
				if (Result != nullptr)
				{
					return nullptr;
				}
				Result = Candidate;
			}
		}
		return Result;
	}

	static const asCExpr* FindImportCall(
		const asCASTContext& Context,
		asASTDeclId ImportDecl)
	{
		const asCExpr* Result = nullptr;
		for (asUINT Index = 1; Index <= Context.GetExprCount(); ++Index)
		{
			const asCExpr* const Candidate =
				Context.GetExpr(asASTExprId(Index));
			if (Candidate != nullptr && Candidate->kind == asAST_EXPR_CALL
				&& Candidate->resolvedDecl == ImportDecl)
			{
				if (Result != nullptr)
				{
					return nullptr;
				}
				Result = Candidate;
			}
		}
		return Result;
	}
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTImportBindingImmutabilitySemaTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.SemaAuthority.ImportBindingImmutability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(BindRebindAndUnbindLeaveDumpAndSidecarByteIdentical)
	{
		using namespace CanonicalASTImportBindingImmutabilityTest;

		FFixture Fixture;
		const bool bBuilt = Fixture.Build(*TestRunner);
		ON_SCOPE_EXIT { Fixture.Engine.Destroy(); };
		ASSERT_THAT(IsTrue(bBuilt,
			*FString::Printf(TEXT("canonical import fixture must compile: %s"),
				*Fixture.Engine.GetMessagesText())));
		if (!bBuilt)
		{
			return;
		}

		const asCASTContext* const Context =
			Fixture.Consumer->GetCanonicalASTContext();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Context->IsPublishable(),
			TEXT("import consumer must retain a Frozen/Publishable snapshot")));
		asSAstVerifyResult Verify;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK,
			asCASTVerify(*Context, Verify),
			TEXT("retained import snapshot must verify before Runtime binding")));

		const asCDecl* const ImportDecl = FindImportDecl(*Context);
		const asCExpr* const ImportCall = ImportDecl != nullptr
			? FindImportCall(*Context, ImportDecl->id) : nullptr;
		const asCType* const ResultType = ImportCall != nullptr
			? Context->GetType(ImportCall->type.type) : nullptr;
		ASSERT_THAT(IsTrue(
			ImportDecl != nullptr
				&& ImportDecl->origin.Equals(ProviderAName)
				&& ImportDecl->stableKey.GetLength() != 0
				&& ImportCall != nullptr
				&& !ImportCall->receiver.IsValid()
				&& ImportCall->children.GetLength() == 0
				&& ImportCall->callArguments.GetLength() == 0
				&& ResultType != nullptr
				&& ResultType->kind == asAST_TYPE_PRIMITIVE
				&& ResultType->primitiveToken == ttInt,
			TEXT("Sema must retain the exact provider-A ImportDecl, zero-argument Call, and concrete int result")));

		ASSERT_THAT(AreEqual(1,
			static_cast<int32>(Fixture.Consumer->bindInformations.GetLength())));
		sBindInfo* const Binding = Fixture.Consumer->bindInformations[0];
		ASSERT_THAT(IsNotNull(Binding));
		if (Binding == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(-1, Binding->boundFunctionId));

		FSnapshotFingerprint Baseline;
		ASSERT_THAT(IsTrue(CaptureFingerprint(*Context, Baseline),
			TEXT("publishable import snapshot must encode as Sidecar V13")));
		auto AssertUnchanged = [this, Context, &Baseline](const TCHAR* Phase)
		{
			FSnapshotFingerprint Current;
			ASSERT_THAT(IsTrue(CaptureFingerprint(*Context, Current),
				*FString::Printf(TEXT("%s snapshot must remain encodable"), Phase)));
			ASSERT_THAT(IsTrue(FingerprintsEqual(Baseline, Current),
				*FString::Printf(TEXT("%s must not mutate Canonical dump or Sidecar bytes"), Phase)));
		};

		asIScriptFunction* const TargetA =
			Fixture.ProviderA->GetFunctionByDecl("int ImportedValue()");
		asIScriptFunction* const TargetB =
			Fixture.ProviderB->GetFunctionByDecl("int ImportedValue()");
		ASSERT_THAT(IsNotNull(TargetA));
		ASSERT_THAT(IsNotNull(TargetB));
		if (TargetA == nullptr || TargetB == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(asSUCCESS,
			Fixture.Consumer->BindImportedFunction(0, TargetA)));
		ASSERT_THAT(AreEqual(TargetA->GetId(), Binding->boundFunctionId));
		AssertUnchanged(TEXT("bind-provider-A"));

		ASSERT_THAT(AreEqual(asSUCCESS,
			Fixture.Consumer->BindImportedFunction(0, TargetB)));
		ASSERT_THAT(AreEqual(TargetB->GetId(), Binding->boundFunctionId));
		AssertUnchanged(TEXT("rebind-provider-B"));

		ASSERT_THAT(AreEqual(asSUCCESS,
			Fixture.Consumer->UnbindImportedFunction(0)));
		ASSERT_THAT(AreEqual(-1, Binding->boundFunctionId));
		AssertUnchanged(TEXT("unbind"));
	}
};

TEST_CLASS_WITH_FLAGS(FCanonicalASTImportBindingImmutabilityCodeGenTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.ProductionCodeGen.ImportBindingImmutability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ExecutionChangesOnlyThroughTheRuntimeImportSlot)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace CanonicalASTImportBindingImmutabilityTest;

		FFixture Fixture;
		const bool bBuilt = Fixture.Build(*TestRunner);
		ON_SCOPE_EXIT { Fixture.Engine.Destroy(); };
		ASSERT_THAT(IsTrue(bBuilt,
			*FString::Printf(TEXT("canonical import fixture must compile: %s"),
				*Fixture.Engine.GetMessagesText())));
		if (!bBuilt)
		{
			return;
		}

		asIScriptFunction* const Caller =
			GetNativeFunctionByDecl(Fixture.Consumer, "int RunImported()");
		asIScriptFunction* const TargetA =
			Fixture.ProviderA->GetFunctionByDecl("int ImportedValue()");
		asIScriptFunction* const TargetB =
			Fixture.ProviderB->GetFunctionByDecl("int ImportedValue()");
		ASSERT_THAT(IsTrue(
			Caller != nullptr && TargetA != nullptr && TargetB != nullptr,
			TEXT("consumer and both compatible providers must publish functions")));
		if (Caller == nullptr || TargetA == nullptr || TargetB == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(
				Caller, asBC_CALLBND),
			TEXT("Canonical CodeGen must retain CALLBND to the imported slot")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Fixture.Consumer->GetLastBytecodePublisher()));
		ASSERT_THAT(AreEqual(0u,
			Fixture.Consumer->GetLastLegacyCompilerInvocationCount()));

		int32 Value = 0;
		FString Exception;
		ASSERT_THAT(AreEqual(asSUCCESS,
			Fixture.Consumer->BindImportedFunction(0, TargetA)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED,
			CanonicalExecuteIntStatus(
				Fixture.ScriptEngine, Fixture.Consumer,
				"int RunImported()", Value, Exception)));
		ASSERT_THAT(AreEqual(11, Value));

		ASSERT_THAT(AreEqual(asSUCCESS,
			Fixture.Consumer->BindImportedFunction(0, TargetB)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED,
			CanonicalExecuteIntStatus(
				Fixture.ScriptEngine, Fixture.Consumer,
				"int RunImported()", Value, Exception)));
		ASSERT_THAT(AreEqual(29, Value));

		ASSERT_THAT(AreEqual(asSUCCESS,
			Fixture.Consumer->UnbindImportedFunction(0)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION,
			CanonicalExecuteIntStatus(
				Fixture.ScriptEngine, Fixture.Consumer,
				"int RunImported()", Value, Exception),
			TEXT("unbound CALLBND must fail through Runtime state, not AST retargeting")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
