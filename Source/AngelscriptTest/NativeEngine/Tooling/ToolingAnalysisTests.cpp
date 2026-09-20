#include "CQTest.h"
#include "NativeToolingTestSupport.h"

#include "as_builder.h"
#include "as_objecttype.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_ast_context.h"
#include "frontend/AST/as_decl.h"
#include "frontend/AST/as_expr.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ToolingAnalysis,
	"Angelscript.UnitTest.NativeEngine.Tooling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(RetainedOwnershipClosure)
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Types =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TSharedPtr<asCDefinitions, ESPMode::ThreadSafe> Host =
			AngelscriptNativeToolingTest::MakeHostType(Types, TEXT("HostValue"));
		ASSERT_THAT(IsTrue(Host.IsValid()));
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Inputs =
			AngelscriptNativeToolingTest::MakeInputs(
				{{TEXT("Use.as"), TEXT("int Use(HostValue@ Value){ return 1; }")}},
				Types,
				Host);
		Host.Reset();
		AS_NAMESPACE_QUALIFIER asCToolingSession Session(Inputs);
		Inputs = MakeShared<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe>();
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse Response = Session.Analyze({});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(Response.Status)));
		ASSERT_THAT(IsTrue(Response.Result.IsValid()));
		ASSERT_THAT(IsNotNull(Response.Result->GetSession()));
		ASSERT_THAT(AreEqual(1, Response.Result->GetSession()->Lookup(TEXT("Use")).Num()));
		ASSERT_THAT(AreEqual(1, Response.Result->GetInputs().OwnedDefinitions.Num()));
		ASSERT_THAT(IsTrue(Response.Result->GetInputs().OwnedDefinitions[0]->IsFrozen()));
	}

	TEST_METHOD(PartialReadFreeze)
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Inputs =
			AngelscriptNativeToolingTest::MakeInputs(
				{{TEXT("Partial.as"), TEXT("int F(){ return Missing; } int G(){ return 2; }")}});
		AS_NAMESPACE_QUALIFIER asCToolingSession Session(Inputs);
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse Response = Session.Analyze({});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Partial),
			static_cast<int32>(Response.Status)));
		ASSERT_THAT(IsTrue(Response.Result.IsValid()));
		const AS_NAMESPACE_QUALIFIER asCCompilationSession* Compilation = Response.Result->GetSession();
		ASSERT_THAT(IsNotNull(Compilation));
		ASSERT_THAT(AreEqual(1, Compilation->Lookup(TEXT("G")).Num()));
		auto* Function = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(
			Compilation->Lookup(TEXT("G"))[0]);
		ASSERT_THAT(IsNotNull(Function));
		ASSERT_THAT(IsNotNull(Function->GetBody()));
		AS_NAMESPACE_QUALIFIER asCASTContext* Context = Response.Result->GetASTContext();
		ASSERT_THAT(IsNotNull(Context));
		ASSERT_THAT(IsTrue(Context->IsFrozenForTooling()));
		ASSERT_THAT(IsFalse(Context->IsSealed()));
		AS_NAMESPACE_QUALIFIER asCSourceRange Range = Function->GetSourceRange();
		ASSERT_THAT(IsNull(Context->Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(int64(1), Range)));
		ASSERT_THAT(IsFalse(const_cast<AS_NAMESPACE_QUALIFIER asCCompilationSession*>(Compilation)->VerifyAST()));
	}

	TEST_METHOD(IdentityAndCancellation)
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Types =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TSharedPtr<asCDefinitions, ESPMode::ThreadSafe> Host =
			AngelscriptNativeToolingTest::MakeHostType(Types, TEXT("HostValue"));
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Hosted =
			AngelscriptNativeToolingTest::MakeInputs(
				{{TEXT("Same.as"), TEXT("int F(){ return 1; }")}},
				Types,
				Host);
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Bare =
			AngelscriptNativeToolingTest::MakeInputs(
				{{TEXT("Same.as"), TEXT("int F(){ return 1; }")}});
		AS_NAMESPACE_QUALIFIER asCToolingSession HostedSession(Hosted);
		AS_NAMESPACE_QUALIFIER asCToolingSession BareSession(Bare);
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse HostedResponse = HostedSession.Analyze({});
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse BareResponse = BareSession.Analyze({});
		ASSERT_THAT(IsTrue(HostedResponse.Result.IsValid() && BareResponse.Result.IsValid()));
		auto* Function = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(
			HostedResponse.Result->GetSession()->Lookup(TEXT("F"))[0]);
		const AS_NAMESPACE_QUALIFIER asSAnalysisEntityHandle Handle = HostedResponse.Result->MakeHandle(Function);
		ASSERT_THAT(IsTrue(Handle.IsValid()));
		ASSERT_THAT(IsTrue(HostedResponse.Result->OwnsHandle(Handle)));
		ASSERT_THAT(IsFalse(BareResponse.Result->OwnsHandle(Handle)));
		ASSERT_THAT(IsNull(BareResponse.Result->ResolveHandle(Handle)));

		bool bCancelled = true;
		AS_NAMESPACE_QUALIFIER asSCancellationToken Token;
		Token.Cancelled = &bCancelled;
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse Cancelled = HostedSession.Analyze(Token);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Cancelled),
			static_cast<int32>(Cancelled.Status)));
		ASSERT_THAT(IsFalse(Cancelled.Result.IsValid()));

		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Raw =
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Raw.as"), TEXT("int F(){ return 1; }")}});
		Raw->Options.Dependencies.Add(Host.Get());
		AS_NAMESPACE_QUALIFIER asCToolingSession Rejected(Raw);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::InvalidInput),
			static_cast<int32>(Rejected.Analyze({}).Status)));
	}

	TEST_METHOD(ForeignRangeOrReadableEdge)
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Inputs =
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Safe.as"), TEXT("int F(){ return 1; }")}});
		AS_NAMESPACE_QUALIFIER asCToolingSession Session(Inputs);
		Session.DebugPlantForeignRangeOnNextAnalyze();
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse Unsafe = Session.Analyze({});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::AnalysisUnavailable),
			static_cast<int32>(Unsafe.Status)));
		ASSERT_THAT(IsFalse(Unsafe.Result.IsValid()));

		AS_NAMESPACE_QUALIFIER asCBuilder Builder(
			AngelscriptNativeEngineTest::MakeSourceList(TEXT("Direct.as"), TEXT("int G(){ return 2; }")),
			Inputs->Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::BodiesAnalyzed)));
		AS_NAMESPACE_QUALIFIER asCASTContext& Context = Builder.GetCompilationSession()->GetASTContext();
		AS_NAMESPACE_QUALIFIER asCASTNode* Target = nullptr;
		for (AS_NAMESPACE_QUALIFIER asCASTNode* Node : Context.GetNodes())
		{
			if (Node && Node->GetFamily() != AS_NAMESPACE_QUALIFIER asEASTNodeFamily::Type
				&& Node->GetSourceRange().IsOrdered())
			{
				Target = Node;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(Target));
		AS_NAMESPACE_QUALIFIER asCSourceRange Foreign = Target->GetSourceRange();
		Foreign.Begin.SnapshotID ^= 0x9F17ULL;
		Foreign.End.SnapshotID = Foreign.Begin.SnapshotID;
		Target->DebugForceSourceRange(Foreign);
		ASSERT_THAT(IsFalse(Context.FreezeForTooling()));
		ASSERT_THAT(IsFalse(Context.IsFrozenForTooling()));
		ASSERT_THAT(IsFalse(Context.IsSealed()));

		AS_NAMESPACE_QUALIFIER asCToolingSession Recovery(
			AngelscriptNativeToolingTest::MakeInputs(
				{{TEXT("Partial.as"), TEXT("int F(){ return Missing; } int G(){ return 2; }")}}));
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse Partial = Recovery.Analyze({});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Partial),
			static_cast<int32>(Partial.Status)));
		ASSERT_THAT(IsTrue(Partial.Result.IsValid()));
		ASSERT_THAT(IsTrue(Partial.Result->GetASTContext()->IsFrozenForTooling()));
		ASSERT_THAT(IsFalse(Partial.Result->GetASTContext()->IsSealed()));
	}

	TEST_METHOD(TransitiveLeasesAndPartialFreezeControl)
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Types =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TSharedPtr<asCDefinitions, ESPMode::ThreadSafe> Inner =
			AngelscriptNativeToolingTest::MakeHostType(Types, TEXT("InnerValue"));
		ASSERT_THAT(IsTrue(Inner.IsValid()));
		TUniquePtr<asCDefinitions> OuterUnique = MakeUnique<asCDefinitions>();
		AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor;
		Descriptor.StableScope = TEXT("tooling-host");
		Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class;
		Descriptor.Name = TEXT("HostValue");
		AS_NAMESPACE_QUALIFIER asSStableKey Key;
		asCObjectType* Type = nullptr;
		ASSERT_THAT(IsTrue(
			OuterUnique->SetIdentityContext(Types) == asEDefinitionResult::Succeeded
			&& Types->GetIdentityRegistry().InternNominalType(Descriptor, Key)
				== AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded
			&& OuterUnique->AddDependency(Inner.Get()) == asEDefinitionResult::Succeeded
			&& OuterUnique->CreateObjectType(Key, "HostValue", "", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Type)
				== asEDefinitionResult::Succeeded
			&& OuterUnique->SetTypeDeclarationFacts(*Type, AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class, 0)
				== asEDefinitionResult::Succeeded
			&& OuterUnique->FinalizeLayouts() == asEDefinitionResult::Succeeded
			&& OuterUnique->Freeze() == asEDefinitionResult::Succeeded
			&& OuterUnique->ValidateFrozenDefinitions()));
		TSharedPtr<asCDefinitions, ESPMode::ThreadSafe> Outer(OuterUnique.Release());

		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> MissingInner =
			AngelscriptNativeToolingTest::MakeInputs(
				{{TEXT("Use.as"), TEXT("int Use(HostValue@ Value){ return 1; }")}},
				Types,
				Outer);
		AS_NAMESPACE_QUALIFIER asCToolingSession Rejected(MissingInner);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::InvalidInput),
			static_cast<int32>(Rejected.Analyze({}).Status)));

		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Retained =
			AngelscriptNativeToolingTest::MakeInputs(
				{{TEXT("Use.as"), TEXT("int Use(HostValue@ Value){ return 1; }")}},
				Types,
				Outer);
		Retained->OwnedDefinitions.Add(Inner);
		Retained->Options.Dependencies.Add(Inner.Get());
		TWeakPtr<asCDefinitions, ESPMode::ThreadSafe> WeakInner = Inner;
		TWeakPtr<asCDefinitions, ESPMode::ThreadSafe> WeakOuter = Outer;
		AS_NAMESPACE_QUALIFIER asCToolingSession Session(Retained);
		Inner.Reset();
		Outer.Reset();
		Retained = MakeShared<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe>();
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse Response = Session.Analyze({});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(Response.Status)));
		ASSERT_THAT(IsTrue(Response.Result.IsValid()));
		ASSERT_THAT(IsTrue(WeakInner.IsValid()));
		ASSERT_THAT(IsTrue(WeakOuter.IsValid()));
		ASSERT_THAT(AreEqual(2, Response.Result->GetInputs().OwnedDefinitions.Num()));
		ASSERT_THAT(IsTrue(Response.Result->GetInputs().OwnedDefinitions[0]->IsFrozen()));
		ASSERT_THAT(IsTrue(Response.Result->GetInputs().OwnedDefinitions[1]->IsFrozen()));
		ASSERT_THAT(AreEqual(1, Response.Result->GetSession()->Lookup(TEXT("Use")).Num()));

		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> PartialInputs =
			AngelscriptNativeToolingTest::MakeInputs(
				{{TEXT("Partial.as"), TEXT("int F(){ return Missing; } int G(){ return 2; }")}});
		AS_NAMESPACE_QUALIFIER asCToolingSession PartialSession(PartialInputs);
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse Partial = PartialSession.Analyze({});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Partial),
			static_cast<int32>(Partial.Status)));
		AS_NAMESPACE_QUALIFIER asCASTContext* Context = Partial.Result->GetASTContext();
		ASSERT_THAT(IsNotNull(Context));
		ASSERT_THAT(IsTrue(Context->IsFrozenForTooling()));
		ASSERT_THAT(IsNull(Context->Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(int64(1),
			AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(
				Partial.Result->GetSession()->Lookup(TEXT("G"))[0])->GetSourceRange())));
		ASSERT_THAT(IsFalse(const_cast<AS_NAMESPACE_QUALIFIER asCCompilationSession*>(
			Partial.Result->GetSession())->VerifyAST()));
	}
};

#endif
