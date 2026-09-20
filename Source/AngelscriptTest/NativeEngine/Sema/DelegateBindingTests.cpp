#include "CQTest.h"

#include "NativeEngine/NativeEngineTestSupport.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_decl.h"
#include "frontend/AST/as_expr.h"
#include "frontend/AST/as_stmt.h"
#include "frontend/AST/as_type.h"
#include "frontend/Compile/as_compilation_session.h"
#include "frontend/Parser/as_parser.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace DelegateBindingTests
{
	using FSessionRun = AngelscriptNativeEngineTest::FFrontendSessionRun;

	const AS_NAMESPACE_QUALIFIER asCFunctionDecl* FindIntAdd(const FSessionRun& Run)
	{
		for (AS_NAMESPACE_QUALIFIER asCDecl* Decl : Run.Session->Lookup(TEXT("Add")))
		{
			const auto* Function = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Decl);
			if (!Function || Function->GetParameters().Num() != 1) continue;
			const auto* Builtin = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCBuiltinType>(Function->GetParameters()[0]->GetResolvedType());
			if (Builtin && Builtin->GetPrimitive() == AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32) return Function;
		}
		return nullptr;
	}

	const AS_NAMESPACE_QUALIFIER asCCallExpr* BindCall(const FSessionRun& Run)
	{
		const AS_NAMESPACE_QUALIFIER asCFunctionDecl* Test = Run.Function(TEXT("Test"));
		if (!Test) return nullptr;
		const auto* Body = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Test->GetBody());
		if (!Body || Body->GetStatements().IsEmpty()) return nullptr;
		return AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCallExpr>(Body->GetStatements()[0]);
	}

	const AS_NAMESPACE_QUALIFIER asCFunctionDecl* SelectedBindTarget(const FSessionRun& Run)
	{
		const AS_NAMESPACE_QUALIFIER asCCallExpr* Call = BindCall(Run);
		if (!Call) return nullptr;
		const auto* Member = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCMemberExpr>(Call->GetCallee());
		return Member ? AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Member->GetMember()) : nullptr;
	}

	TArray<AS_NAMESPACE_QUALIFIER asSDiagnosticRecord> GetDiagnostics(FSessionRun& Run)
	{
		AS_NAMESPACE_QUALIFIER asCCollectingDiagnosticConsumer Consumer;
		Run.Diagnostics.EmitTo(Consumer);
		TArray<AS_NAMESPACE_QUALIFIER asSDiagnosticRecord> Records;
		Records.Append(Consumer.GetRecords().GetData(), Consumer.GetRecords().Num());
		return Records;
	}
}

TEST_CLASS_WITH_FLAGS(DelegateBinding, "Angelscript.UnitTest.NativeEngine.Sema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(BindIntOverload)
	{
		DelegateBindingTests::FSessionRun Run({{TEXT("BindInt.as"), TEXT(
			"DECLARE_DELEGATE_RetVal_OneParam(int, FAdd, int);\n"
			"int Add(bool Value);\n"
			"int Add(int Value);\n"
			"void Test(FAdd Handler) { Handler.Bind(Add); }\n")}});
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
		const auto* Selected = DelegateBindingTests::SelectedBindTarget(Run);
		ASSERT_THAT(IsNotNull(Selected));
		ASSERT_THAT(IsTrue(Selected == DelegateBindingTests::FindIntAdd(Run)));
	}

	TEST_METHOD(RejectBoolOnly)
	{
		DelegateBindingTests::FSessionRun Run({{TEXT("RejectBool.as"), TEXT(
			"DECLARE_DELEGATE_RetVal_OneParam(int, FAdd, int);\n"
			"int Add(bool Value);\n"
			"void Test(FAdd Handler) { Handler.Bind(Add); }\n")}});
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
		ASSERT_THAT(IsTrue(Run.Session->SerializeStableBodyDiagnostics().Contains(TEXT("signature-mismatch"))));
		ASSERT_THAT(IsNull(DelegateBindingTests::SelectedBindTarget(Run)));
	}

	TEST_METHOD(RejectRemovedKeywordBind)
	{
		DelegateBindingTests::FSessionRun Run({{TEXT("Keyword.as"), TEXT("delegate int FAdd(int);")}});
		ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
		ASSERT_THAT(AreEqual(0, Run.Session->Lookup(TEXT("FAdd")).Num()));
		bool bRemoved = false;
		for (const auto& Record : DelegateBindingTests::GetDiagnostics(Run))
		{
			if (Record.DiagnosticID == static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDeclarationDiagnostic::RemovedDelegateEventKeyword))
				bRemoved = true;
		}
		ASSERT_THAT(IsTrue(bRemoved));
	}

	TEST_METHOD(DynamicNamedBind)
	{
		DelegateBindingTests::FSessionRun Run({{TEXT("DynamicBind.as"), TEXT(
			"DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(int, FAdd, int, Value);\n"
			"int Add(int Value);\n"
			"void Test(FAdd Handler) { Handler.Bind(Add); }\n")}});
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
		ASSERT_THAT(IsNotNull(DelegateBindingTests::SelectedBindTarget(Run)));
	}

	TEST_METHOD(BindDynamicUFunction)
	{
		DelegateBindingTests::FSessionRun Run({{TEXT("BindDynamic.as"), TEXT(
			"DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(int, FAdd, int, Value);\n"
			"int Add(int Value);\n"
			"void Test(FAdd Handler) { Handler.BindDynamic(Add); }\n")}});
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
		ASSERT_THAT(IsTrue(Run.Session->SerializeStableBodyDiagnostics().Contains(TEXT("bind-requires-ufunction"))));
	}
};

#endif
