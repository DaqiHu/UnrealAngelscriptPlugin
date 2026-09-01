#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"
#include "../../Support/AngelscriptNativeBuilderTestSupport.h"

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

// Focused SemaAuthority slice for generation `__CreateLiteralAsset(Type, "Name")`:
// a type identifier rewrites to a host VALUE (or TEMPLATE VALUE) global that
// converts through zero-arg `opImplConv` to the first formal, while a second
// exact argument stays viable. Prefix stays under Compiler.CanonicalAST.SemaAuthority.

namespace CanonicalASTSemaTypeIdImplConvCallTest
{
	struct FHost
	{
		int32 Value = 0;
	};

	struct FTemplateSlot
	{
		void* Ptr = nullptr;
	};

	static FHost StaticHost;
	static FTemplateSlot StaticTemplateHost;

	static void ConstructHost(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr && Generic->GetObject() != nullptr)
		{
			new (Generic->GetObject()) FHost();
		}
	}

	static void ConstructTemplateHost(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr && Generic->GetObject() != nullptr)
		{
			new (Generic->GetObject()) FTemplateSlot();
		}
	}

	static void ConvertHost(asIScriptGeneric*)
	{
	}

	static void CreateTarget(asIScriptGeneric*)
	{
	}

	static bool RegisterCommonRef(asCScriptEngine* ScriptEngine)
	{
		return ScriptEngine != nullptr
			&& ScriptEngine->RegisterObjectType(
				"CClass",
				0,
				asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE) >= 0
			&& ScriptEngine->RegisterObjectType(
				"CObj",
				0,
				asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE) >= 0;
	}

	static const asCExpr* FindNamedCall(asCASTContext& Context, const char* Name)
	{
		for (asUINT ExprIndex = 1; ExprIndex <= Context.GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Expr = Context.GetExpr(asASTExprId(ExprIndex));
			if (Expr == nullptr || Expr->kind != asAST_EXPR_CALL)
			{
				continue;
			}
			const asCDecl* const Target = Context.GetDecl(Expr->resolvedDecl);
			if (Target != nullptr && Target->name.Equals(Name))
			{
				return Expr;
			}
		}
		return nullptr;
	}

	static const asCExpr* FindCallConversionArg(asCASTContext& Context, const asCExpr* Call)
	{
		if (Call == nullptr)
		{
			return nullptr;
		}
		for (asUINT ChildIndex = 0; ChildIndex < Call->children.GetLength(); ++ChildIndex)
		{
			const asCExpr* const Child = Context.GetExpr(Call->children[ChildIndex]);
			if (Child != nullptr && Child->kind == asAST_EXPR_CONVERSION)
			{
				return Child;
			}
		}
		return nullptr;
	}

	static void DestructTemplateHost(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr && Generic->GetObject() != nullptr)
		{
			static_cast<FTemplateSlot*>(Generic->GetObject())->~FTemplateSlot();
		}
	}

	static void TemplateCallback(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		if (int* const OutFlag = static_cast<int*>(Generic->GetAddressOfArg(1)))
		{
			*OutFlag = 0;
		}
		Generic->SetReturnByte(1);
	}
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTSemaTypeIdImplConvCallTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.SemaAuthority.TypeIdImplConvCall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(TypeIdentifierValueOpImplConvMakesTwoArgCallViable)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			CanonicalASTSemaTypeIdImplConvCallTest::RegisterCommonRef(ScriptEngine),
			TEXT("CClass/CObj must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FHost",
				sizeof(CanonicalASTSemaTypeIdImplConvCallTest::FHost),
				asOBJ_VALUE | asOBJ_POD
					| asGetTypeTraits<CanonicalASTSemaTypeIdImplConvCallTest::FHost>()) >= 0,
			TEXT("FHost VALUE type must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"FHost",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(CanonicalASTSemaTypeIdImplConvCallTest::ConstructHost),
				asCALL_GENERIC) >= 0,
			TEXT("FHost constructor must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectMethod(
				"FHost",
				"CClass opImplConv() const",
				asFUNCTION(CanonicalASTSemaTypeIdImplConvCallTest::ConvertHost),
				asCALL_GENERIC) >= 0,
			TEXT("FHost opImplConv to CClass must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterGlobalProperty(
				"const FHost __StaticType_CObj",
				&CanonicalASTSemaTypeIdImplConvCallTest::StaticHost) >= 0,
			TEXT("__StaticType_CObj VALUE global must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterGlobalFunction(
				"void Create(CClass Cls, int Tag)",
				asFUNCTION(CanonicalASTSemaTypeIdImplConvCallTest::CreateTarget),
				asCALL_GENERIC) >= 0,
			TEXT("Create(CClass, int) must register")));
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "SemaTypeIdImplConvCall");
		ASSERT_THAT(IsNotNull(Module, TEXT("SemaTypeIdImplConvCall module")));
		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			void F()
			{
				Create(CObj, 1);
			}
			)AS");
		Code.SetCode("SemaTypeIdImplConvCall.as", ScriptSource.c_str(), true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("type-id VALUE opImplConv two-arg call must parse")));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("type-id VALUE opImplConv two-arg call must seal")));

		asCString Dump;
		asCASTDump(Context, Dump);
		const FString Text = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		const FString DumpMsg = FString::Printf(
			TEXT("Create(CObj, 1) must resolve via __StaticType_CObj opImplConv. dump:\n%s"),
			*Text);

		const asCExpr* const Call =
			CanonicalASTSemaTypeIdImplConvCallTest::FindNamedCall(Context, "Create");
		ASSERT_THAT(IsNotNull(Call, *DumpMsg));
		if (Call == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Call->children.GetLength() >= 2, *DumpMsg));
		const asSASTCallArgument* ClassArgument = nullptr;
		for (asUINT ArgumentIndex = 0;
			ArgumentIndex < Call->callArguments.GetLength(); ++ArgumentIndex)
		{
			if (Call->callArguments[ArgumentIndex].formalIndex == 0)
			{
				ClassArgument = &Call->callArguments[ArgumentIndex];
				break;
			}
		}
		const asCExpr* const Argument = ClassArgument != nullptr
			? Context.GetExpr(ClassArgument->expression) : nullptr;
		const asCDecl* const Formal = ClassArgument != nullptr
			? Context.GetDecl(ClassArgument->formal) : nullptr;
		ASSERT_THAT(IsTrue(
			Argument != nullptr
				&& Argument->kind == asAST_EXPR_MATERIALIZE_TEMPORARY
				&& Argument->children.GetLength() == 1
				&& Formal != nullptr
				&& Formal->name.Equals("Cls")
				&& ClassArgument->origin == asAST_CALL_ARGUMENT_POSITIONAL
				&& ClassArgument->formalType == Formal->type,
			*DumpMsg));
		const asCExpr* const ConversionCall = Argument != nullptr
			&& Argument->children.GetLength() == 1
			? Context.GetExpr(Argument->children[0]) : nullptr;
		const asCDecl* const ConversionMethod = ConversionCall != nullptr
			? Context.GetDecl(ConversionCall->resolvedDecl) : nullptr;
		const asCExpr* const Receiver = ConversionCall != nullptr
			? Context.GetExpr(ConversionCall->receiver) : nullptr;
		const asCType* const ArgumentType = ConversionCall != nullptr
			? Context.GetType(ConversionCall->type.type) : nullptr;
		ASSERT_THAT(IsTrue(
			ConversionCall != nullptr
				&& ConversionCall->kind == asAST_EXPR_CALL
				&& ConversionMethod != nullptr
				&& ConversionMethod->kind == asAST_DECL_METHOD
				&& ConversionMethod->name.Equals("opImplConv")
				&& ConversionCall->receiver.IsValid()
				&& ArgumentType != nullptr
				&& ArgumentType->stableKey.Equals("CClass"),
			*DumpMsg));
		const asCDecl* const ReceiverDecl = Receiver != nullptr
			? Context.GetDecl(Receiver->resolvedDecl) : nullptr;
		ASSERT_THAT(IsTrue(
			ReceiverDecl != nullptr
				&& ReceiverDecl->kind == asAST_DECL_VAR
				&& ReceiverDecl->name.Equals("__StaticType_CObj"),
			*DumpMsg));
	}

	TEST_METHOD(TypeIdentifierTemplateValueOpImplConvMakesTwoArgCallViable)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			CanonicalASTSemaTypeIdImplConvCallTest::RegisterCommonRef(ScriptEngine),
			TEXT("CClass/CObj must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"THost<class T>",
				sizeof(CanonicalASTSemaTypeIdImplConvCallTest::FTemplateSlot),
				asOBJ_VALUE | asOBJ_APP_CLASS_CDA | asOBJ_TEMPLATE) >= 0,
			TEXT("THost<T> VALUE template must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"THost<T>",
				asBEHAVE_TEMPLATE_CALLBACK,
				"bool f(int&in, int&out)",
				asFUNCTION(CanonicalASTSemaTypeIdImplConvCallTest::TemplateCallback),
				asCALL_GENERIC) >= 0,
			TEXT("THost<T> template callback must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"THost<T>",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(CanonicalASTSemaTypeIdImplConvCallTest::ConstructTemplateHost),
				asCALL_GENERIC) >= 0,
			TEXT("THost<T> constructor must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"THost<T>",
				asBEHAVE_DESTRUCT,
				"void f()",
				asFUNCTION(CanonicalASTSemaTypeIdImplConvCallTest::DestructTemplateHost),
				asCALL_GENERIC) >= 0,
			TEXT("THost<T> destructor must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectMethod(
				"THost<T>",
				"CClass opImplConv() const",
				asFUNCTION(CanonicalASTSemaTypeIdImplConvCallTest::ConvertHost),
				asCALL_GENERIC) >= 0,
			TEXT("THost<T> opImplConv to CClass must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterGlobalProperty(
				"const THost<CObj> __StaticType_CObj",
				&CanonicalASTSemaTypeIdImplConvCallTest::StaticTemplateHost) >= 0,
			TEXT("__StaticType_CObj TEMPLATE VALUE global must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterGlobalFunction(
				"void Create(CClass Cls, int Tag)",
				asFUNCTION(CanonicalASTSemaTypeIdImplConvCallTest::CreateTarget),
				asCALL_GENERIC) >= 0,
			TEXT("Create(CClass, int) must register")));
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "SemaTypeIdTemplateImplConvCall");
		ASSERT_THAT(IsNotNull(Module, TEXT("SemaTypeIdTemplateImplConvCall module")));
		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			void F()
			{
				Create(CObj, 1);
			}
			)AS");
		Code.SetCode("SemaTypeIdTemplateImplConvCall.as", ScriptSource.c_str(), true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("type-id TEMPLATE VALUE opImplConv two-arg call must parse")));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("type-id TEMPLATE VALUE opImplConv two-arg call must seal")));

		asCString Dump;
		asCASTDump(Context, Dump);
		const FString Text = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		const FString DumpMsg = FString::Printf(
			TEXT("Create(CObj, 1) must resolve via THost<CObj> opImplConv. dump:\n%s"),
			*Text);

		const asCExpr* const Call =
			CanonicalASTSemaTypeIdImplConvCallTest::FindNamedCall(Context, "Create");
		ASSERT_THAT(IsNotNull(Call, *DumpMsg));
		if (Call == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Call->children.GetLength() >= 2, *DumpMsg));
		const asCExpr* const Argument =
			CanonicalASTSemaTypeIdImplConvCallTest::FindCallConversionArg(Context, Call);
		ASSERT_THAT(IsTrue(
			Argument != nullptr && Argument->kind == asAST_EXPR_CONVERSION, *DumpMsg));
		const asCType* const ArgumentType = Argument != nullptr
			? Context.GetType(Argument->type.type) : nullptr;
		ASSERT_THAT(IsTrue(
			ArgumentType != nullptr && ArgumentType->stableKey.Equals("CClass"),
			*DumpMsg));
		const asCExpr* const Inner = Argument != nullptr && Argument->children.GetLength() >= 1
			? Context.GetExpr(Argument->children[0]) : nullptr;
		const asCDecl* const InnerDecl = Inner != nullptr
			? Context.GetDecl(Inner->resolvedDecl) : nullptr;
		ASSERT_THAT(IsTrue(
			InnerDecl != nullptr
				&& InnerDecl->kind == asAST_DECL_VAR
				&& InnerDecl->name.Equals("__StaticType_CObj"),
			*DumpMsg));
		const asCType* const InnerType = Inner != nullptr
			? Context.GetType(Inner->type.type) : nullptr;
		ASSERT_THAT(IsTrue(
			InnerType != nullptr
				&& InnerType->kind == asAST_TYPE_TEMPLATE
				&& InnerType->stableKey.Equals("THost<CObj>"),
			*DumpMsg));
	}
};

#endif
