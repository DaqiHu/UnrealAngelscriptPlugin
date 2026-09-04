#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"
#include "../../Support/AngelscriptNativeBuilderTestSupport.h"
#include "../../Support/AngelscriptNativeExecutionTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_dump.h"
#include "source/as_ast_type.h"
#include "source/as_builder.h"
#include "source/as_decl.h"
#include "source/as_expr.h"
#include "source/as_generic.h"
#include "source/as_module.h"
#include "source/as_parser.h"
#include "source/as_scriptcode.h"
#include "source/as_scriptengine.h"
#include "source/as_sema.h"
#include "EndAngelscriptHeaders.h"

#include <string>

#if WITH_ANGELSCRIPT_UNITTESTS

// Focused SemaAuthority slice: asOBJ_ASHANDLE is a semantic type trait. Plain
// `=` on such a value must bind opHndlAssign even when an ordinary opAssign is
// also registered. A backend must not rediscover that trait after publication.

namespace CanonicalASTSemaHandleAssignTest
{
	struct FHandleValue
	{
		int32 Stored = 0;

		FHandleValue() = default;
		FHandleValue(const FHandleValue&) = default;
		~FHandleValue() = default;
		FHandleValue& operator=(const FHandleValue&) = default;
	};

	static void Construct(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr && Generic->GetObject() != nullptr)
		{
			new (Generic->GetObject()) FHandleValue();
		}
	}

	static void CopyConstruct(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr && Generic->GetObject() != nullptr)
		{
			const FHandleValue* const Other =
				static_cast<const FHandleValue*>(Generic->GetArgAddress(0));
			new (Generic->GetObject()) FHandleValue(
				Other != nullptr ? *Other : FHandleValue());
		}
	}

	static void Destruct(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr && Generic->GetObject() != nullptr)
		{
			static_cast<FHandleValue*>(Generic->GetObject())->~FHandleValue();
		}
	}

	static void ValueAssign(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FHandleValue* const Self =
			static_cast<FHandleValue*>(Generic->GetObject());
		if (Self != nullptr)
		{
			Self->Stored = -100;
		}
		Generic->SetReturnAddress(Self);
	}

	static void HandleAssign(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FHandleValue* const Self =
			static_cast<FHandleValue*>(Generic->GetObject());
		const FHandleValue* const Other =
			static_cast<const FHandleValue*>(Generic->GetArgAddress(0));
		if (Self != nullptr && Other != nullptr)
		{
			Self->Stored = Other->Stored + 21;
		}
		Generic->SetReturnAddress(Self);
	}

	static void SetStored(asIScriptGeneric* Generic)
	{
		FHandleValue* const Self = Generic != nullptr
			? static_cast<FHandleValue*>(Generic->GetObject()) : nullptr;
		if (Self != nullptr)
		{
			Self->Stored = static_cast<int32>(Generic->GetArgDWord(0));
		}
	}

	static void ReadStored(asIScriptGeneric* Generic)
	{
		const FHandleValue* const Self = Generic != nullptr
			? static_cast<const FHandleValue*>(Generic->GetObject()) : nullptr;
		if (Generic != nullptr)
		{
			Generic->SetReturnDWord(
				Self != nullptr ? static_cast<asDWORD>(Self->Stored) : 0u);
		}
	}

	static bool RegisterSurface(
		asCScriptEngine* ScriptEngine,
		bool bRegisterHandleAssign = true)
	{
		if (ScriptEngine == nullptr
			|| ScriptEngine->RegisterObjectType(
				"FHandleValue",
				sizeof(FHandleValue),
				asOBJ_VALUE | asOBJ_ASHANDLE
					| asGetTypeTraits<FHandleValue>()) < 0
			|| ScriptEngine->RegisterObjectBehaviour(
				"FHandleValue",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(Construct),
				asCALL_GENERIC) < 0
			|| ScriptEngine->RegisterObjectBehaviour(
				"FHandleValue",
				asBEHAVE_CONSTRUCT,
				"void f(const FHandleValue&in Other)",
				asFUNCTION(CopyConstruct),
				asCALL_GENERIC) < 0
			|| ScriptEngine->RegisterObjectBehaviour(
				"FHandleValue",
				asBEHAVE_DESTRUCT,
				"void f()",
				asFUNCTION(Destruct),
				asCALL_GENERIC) < 0
			|| ScriptEngine->RegisterObjectMethod(
				"FHandleValue",
				"FHandleValue& opAssign(const FHandleValue&in Other)",
				asFUNCTION(ValueAssign),
				asCALL_GENERIC) < 0)
		{
			return false;
		}

		if (bRegisterHandleAssign
			&& ScriptEngine->RegisterObjectMethod(
				"FHandleValue",
				"FHandleValue& opHndlAssign(const FHandleValue&in Other)",
				asFUNCTION(HandleAssign),
				asCALL_GENERIC) < 0)
		{
			return false;
		}

		return ScriptEngine->RegisterObjectMethod(
				"FHandleValue",
				"void SetStored(int Value)",
				asFUNCTION(SetStored),
				asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterObjectMethod(
				"FHandleValue",
				"int ReadStored() const",
				asFUNCTION(ReadStored),
				asCALL_GENERIC) >= 0;
	}
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTSemaHandleAssignTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.SemaAuthority.HandleAssign",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(AsHandlePlainAssignmentSealsOpHndlAssignCall)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			CanonicalASTSemaHandleAssignTest::RegisterSurface(ScriptEngine),
			TEXT("asOBJ_ASHANDLE fixture must register both assignment operators")));

		asCModule* const Module =
			AngelscriptBuilderTestSupport::CreateBuilderModule(
				ScriptEngine, "SemaHandleAssign");
		ASSERT_THAT(IsNotNull(Module, TEXT("SemaHandleAssign module")));
		if (Module == nullptr)
		{
			return;
		}

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			void F()
			{
				FHandleValue Left;
				FHandleValue Right;
				Left = Right;
			}
			)AS");
		Code.SetCode("SemaHandleAssign.as", ScriptSource.c_str(), true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("asOBJ_ASHANDLE assignment must parse")));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("asOBJ_ASHANDLE assignment must seal without backend inference")));

		asCString RawDump;
		asCASTDump(Context, RawDump);
		const FString Dump =
			AngelscriptNativeTestSupport::CanonicalAstDumpToFString(RawDump);
		const FString DumpMsg = FString::Printf(
			TEXT("FHandleValue Left = Right must bind opHndlAssign from the asOBJ_ASHANDLE trait, never opAssign. dump:\n%s"),
			*Dump);

		const asCExpr* HandleAssign = nullptr;
		bool bSawValueAssign = false;
		for (asUINT ExprIndex = 1; ExprIndex <= Context.GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Expr = Context.GetExpr(asASTExprId(ExprIndex));
			const asCDecl* const Callee = Expr != nullptr
				? Context.GetDecl(Expr->resolvedDecl) : nullptr;
			if (Callee == nullptr || Callee->kind != asAST_DECL_METHOD)
			{
				continue;
			}
			if (Callee->name.Equals("opHndlAssign"))
			{
				HandleAssign = Expr;
			}
			else if (Callee->name.Equals("opAssign"))
			{
				bSawValueAssign = true;
			}
		}

		const asCDecl* const Callee = HandleAssign != nullptr
			? Context.GetDecl(HandleAssign->resolvedDecl) : nullptr;
		const asCDecl* const Formal = HandleAssign != nullptr
			&& HandleAssign->callArguments.GetLength() == 1
			? Context.GetDecl(HandleAssign->callArguments[0].formal) : nullptr;
		ASSERT_THAT(IsTrue(
			HandleAssign != nullptr
				&& HandleAssign->kind == asAST_EXPR_CALL
				&& Callee != nullptr
				&& Callee->name.Equals("opHndlAssign")
				&& HandleAssign->receiver.IsValid()
				&& HandleAssign->callDispatch == asAST_CALL_DISPATCH_DIRECT
				&& HandleAssign->callArguments.GetLength() == 1
				&& HandleAssign->callArguments[0].origin
					== asAST_CALL_ARGUMENT_POSITIONAL
				&& HandleAssign->callArguments[0].formalIndex == 0
				&& Formal != nullptr
				&& Formal->name.Equals("Other")
				&& !bSawValueAssign,
			*DumpMsg));
	}

	TEST_METHOD(AsHandleWithoutOpHndlAssignFailsClosedBeforeCodeGen)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			CanonicalASTSemaHandleAssignTest::RegisterSurface(
				ScriptEngine, false),
			TEXT("negative asOBJ_ASHANDLE fixture must omit only opHndlAssign")));

		asCModule* const Module =
			AngelscriptBuilderTestSupport::CreateBuilderModule(
				ScriptEngine, "SemaHandleAssignMissingOperator");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			void F()
			{
				FHandleValue Left;
				FHandleValue Right;
				Left = Right;
			}
			)AS");
		Code.SetCode(
			"SemaHandleAssignMissingOperator.as",
			ScriptSource.c_str(),
			true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("missing-opHndlAssign fixture must parse for Sema recovery")));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("the recovered AST remains structurally sealable")));

		asCString RawDump;
		asCASTDump(Context, RawDump);
		const FString Dump =
			AngelscriptNativeTestSupport::CanonicalAstDumpToFString(RawDump);
		bool bSawValueAssign = false;
		for (asUINT ExprIndex = 1; ExprIndex <= Context.GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Expr = Context.GetExpr(asASTExprId(ExprIndex));
			const asCDecl* const Callee = Expr != nullptr
				? Context.GetDecl(Expr->resolvedDecl) : nullptr;
			bSawValueAssign = bSawValueAssign
				|| (Callee != nullptr && Callee->name.Equals("opAssign"));
		}
		const asCArray<asCString>& Diagnostics = Sema.GetDiagnostics();
		const FString Evidence = FString::Printf(
			TEXT("missing opHndlAssign must diagnose in Sema and must never fall back to opAssign; diagnostics=%u dump:\n%s"),
			Diagnostics.GetLength(),
			*Dump);
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
			Diagnostics.GetLength(), *Evidence));
		ASSERT_THAT(IsTrue(
			Diagnostics.GetLength() == 1
				&& Diagnostics[0].Equals("no-appropriate-opHndlAssign")
				&& !bSawValueAssign,
			*Evidence));
	}
};

TEST_CLASS_WITH_FLAGS(FCanonicalASTProductionHandleAssignTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.ProductionCodeGen.HandleAssign",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(AsHandlePlainAssignmentExecutesOpHndlAssignWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			CanonicalASTSemaHandleAssignTest::RegisterSurface(ScriptEngine),
			TEXT("production asOBJ_ASHANDLE fixture must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("production asOBJ_ASHANDLE fixture selects CANONICAL")));

		asIScriptModule* const Module = ScriptEngine->GetModule(
			"ProductionHandleAssign", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(
			asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				FHandleValue Left;
				FHandleValue Right;
				Right.SetStored(21);
				Left = Right;
				return Left.ReadStored();
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProductionHandleAssign.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));

		const int BuildResult = Module->Build();
		asCModule* const Internal = static_cast<asCModule*>(Module);
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		asCString RawDump;
		if (const asCASTContext* const Context = Internal != nullptr
			? Internal->GetCanonicalASTContext() : nullptr)
		{
			asCASTDump(*Context, RawDump);
		}
		const FString Dump =
			AngelscriptNativeTestSupport::CanonicalAstDumpToFString(RawDump);
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("asOBJ_ASHANDLE assignment must publish from its sealed opHndlAssign Call; build=%d messages=%s dump:\n%s"),
				BuildResult,
				*Messages,
				*Dump)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("asOBJ_ASHANDLE assignment must publish through Canonical CodeGen")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("asOBJ_ASHANDLE assignment must not construct asCCompiler")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 42,
			*FString::Printf(
				TEXT("opHndlAssign must execute instead of opAssign; Entry()=%d dump:\n%s"),
				Value,
				*Dump)));
	}
};

#endif
