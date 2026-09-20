#include "CQTest.h"
#include "NativeToolingTestSupport.h"

#include "as_definitions.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_decl.h"
#include "frontend/AST/as_type_context.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ToolingNavigation,
	"Angelscript.UnitTest.NativeEngine.Tooling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	static TSharedPtr<asCDefinitions, ESPMode::ThreadSafe> MakeHostAdd(
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Types)
	{
		TUniquePtr<asCDefinitions> Set = MakeUnique<asCDefinitions>();
		if (Set->SetIdentityContext(Types) != asEDefinitionResult::Succeeded)
		{
			return nullptr;
		}
		AS_NAMESPACE_QUALIFIER asSStableKey IntType;
		Types->InternPrimitiveTypeUse(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32, IntType);
		AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor AddIdentity;
		AddIdentity.StableScope = TEXT("tooling-host");
		AddIdentity.NamespacePath = {TEXT("Host")};
		AddIdentity.Name = TEXT("Add");
		AddIdentity.Kind = AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Global;
		AddIdentity.ReturnType = IntType;
		AddIdentity.Parameters.Add({IntType});
		AS_NAMESPACE_QUALIFIER asSStableKey AddKey;
		asCScriptFunction* Add = nullptr;
		AS_NAMESPACE_QUALIFIER asSDefinitionParameter Parameter;
		Parameter.Name = "Value";
		Parameter.Type = Set->Primitive(ttInt);
		if (Types->GetIdentityRegistry().InternFunction(AddIdentity, AddKey)
				!= AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded
			|| Set->CreateFunction(AddKey, "Add", "Host", asFUNC_SCRIPT, Add) != asEDefinitionResult::Succeeded
			|| Set->DefineFunction(*Add, Set->Primitive(ttInt), {Parameter}) != asEDefinitionResult::Succeeded
			|| Set->SetFunctionDeclarationFacts(*Add, AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Global)
				!= asEDefinitionResult::Succeeded
			|| Set->FinalizeLayouts() != asEDefinitionResult::Succeeded
			|| Set->Freeze() != asEDefinitionResult::Succeeded
			|| !Set->ValidateFrozenDefinitions())
		{
			return nullptr;
		}
		return TSharedPtr<asCDefinitions, ESPMode::ThreadSafe>(Set.Release());
	}

	TEST_METHOD(ActualIdentifierBinding)
	{
		const FString Source = TEXT("int F(){ int Value=1; return Value; }");
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Inputs =
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Value.as"), Source}});
		AS_NAMESPACE_QUALIFIER asCToolingSession Session(Inputs);
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse Response = Session.Analyze({});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(Response.Status)));
		ASSERT_THAT(IsTrue(Response.Result.IsValid()));
		const FString Before = Response.Result->GetSession()->SerializeStableBodyProjection();

		AS_NAMESPACE_QUALIFIER asSQueryPosition Use;
		Use.LogicalSourceKey = TEXT("Value.as");
		Use.ByteOffset = static_cast<asUINT>(Source.Find(TEXT("return Value")) + 7);
		const AS_NAMESPACE_QUALIFIER asSHoverResult Hover = Session.GetHover(*Response.Result, Use, {});
		const AS_NAMESPACE_QUALIFIER asSDefinitionResult Definition = Session.FindDefinition(*Response.Result, Use, {});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(Hover.Status)));
		ASSERT_THAT(IsTrue(Hover.Display.Contains(TEXT("int"))));
		ASSERT_THAT(IsTrue(Hover.Display.Contains(TEXT("Value"))));
		ASSERT_THAT(IsFalse(Hover.Display.Contains(TEXT(" F("))));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(Definition.Status)));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEDefinitionTargetKind::SourceTarget),
			static_cast<int32>(Definition.Target)));
		const int32 DeclStart = Source.Find(TEXT("int Value=1"));
		ASSERT_THAT(IsTrue(Definition.Range.Begin.ByteOffset <= static_cast<asUINT>(DeclStart)));
		ASSERT_THAT(IsTrue(Definition.Range.End.ByteOffset > static_cast<asUINT>(DeclStart)));

		AS_NAMESPACE_QUALIFIER asSQueryPosition Semicolon;
		Semicolon.LogicalSourceKey = TEXT("Value.as");
		Semicolon.ByteOffset = static_cast<asUINT>(Source.Find(TEXT("return Value;")) + 12);
		const AS_NAMESPACE_QUALIFIER asSHoverResult Empty = Session.GetHover(*Response.Result, Semicolon, {});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(Empty.Status)));
		ASSERT_THAT(IsTrue(Empty.Display.IsEmpty()));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEDefinitionTargetKind::None),
			static_cast<int32>(Session.FindDefinition(*Response.Result, Semicolon, {}).Target)));

		const FString EofSource = TEXT("int F(){ int Value=1; return Value ; }");
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> EofInputs =
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Eof.as"), EofSource}});
		AS_NAMESPACE_QUALIFIER asCToolingSession EofSession(EofInputs);
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse EofResponse = EofSession.Analyze({});
		ASSERT_THAT(IsTrue(EofResponse.Result.IsValid()));
		AS_NAMESPACE_QUALIFIER asSQueryPosition Eof;
		Eof.LogicalSourceKey = TEXT("Eof.as");
		Eof.ByteOffset = static_cast<asUINT>(EofSource.Find(TEXT("Value ;")) + 5);
		const AS_NAMESPACE_QUALIFIER asSHoverResult EofHover = EofSession.GetHover(*EofResponse.Result, Eof, {});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(EofHover.Status)));
		ASSERT_THAT(IsTrue(EofHover.Display.Contains(TEXT("Value"))));

		bool bCancelled = true;
		AS_NAMESPACE_QUALIFIER asSCancellationToken Token;
		Token.Cancelled = &bCancelled;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Cancelled),
			static_cast<int32>(Session.GetHover(*Response.Result, Use, Token).Status)));
		ASSERT_THAT(AreEqual(Before, Response.Result->GetSession()->SerializeStableBodyProjection()));
	}

	TEST_METHOD(HostLocationAbsence)
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Types =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TSharedPtr<asCDefinitions, ESPMode::ThreadSafe> Host = MakeHostAdd(Types);
		ASSERT_THAT(IsTrue(Host.IsValid()));
		const FString HostSource = TEXT("int F(){ return Host::Add(42); }");
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Hosted =
			AngelscriptNativeToolingTest::MakeInputs(
				{{TEXT("Hosted.as"), HostSource}},
				Types,
				Host);
		AS_NAMESPACE_QUALIFIER asCToolingSession HostedSession(Hosted);
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse HostedResponse = HostedSession.Analyze({});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(HostedResponse.Status)));
		AS_NAMESPACE_QUALIFIER asSQueryPosition Add;
		Add.LogicalSourceKey = TEXT("Hosted.as");
		Add.ByteOffset = static_cast<asUINT>(HostSource.Find(TEXT("Add")));
		const AS_NAMESPACE_QUALIFIER asSHoverResult Hover = HostedSession.GetHover(*HostedResponse.Result, Add, {});
		const AS_NAMESPACE_QUALIFIER asSDefinitionResult Definition =
			HostedSession.FindDefinition(*HostedResponse.Result, Add, {});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(Hover.Status)));
		ASSERT_THAT(IsTrue(Hover.Display.Contains(TEXT("Add"))));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(Definition.Status)));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEDefinitionTargetKind::NoSourceTarget),
			static_cast<int32>(Definition.Target)));
		ASSERT_THAT(IsFalse(Definition.Range.IsOrdered()));

		const FString UseSource = TEXT("int F(){ return Shared(); }");
		const FString DefSource = TEXT("int Shared(){ return 1; }");
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Cross =
			AngelscriptNativeToolingTest::MakeInputs(
				{{TEXT("B.as"), UseSource}, {TEXT("A.as"), DefSource}});
		AS_NAMESPACE_QUALIFIER asCToolingSession CrossSession(Cross);
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse CrossResponse = CrossSession.Analyze({});
		ASSERT_THAT(IsTrue(CrossResponse.Result.IsValid()));
		AS_NAMESPACE_QUALIFIER asSQueryPosition SharedUse;
		SharedUse.LogicalSourceKey = TEXT("B.as");
		SharedUse.ByteOffset = static_cast<asUINT>(UseSource.Find(TEXT("Shared")));
		const AS_NAMESPACE_QUALIFIER asSDefinitionResult SharedDef =
			CrossSession.FindDefinition(*CrossResponse.Result, SharedUse, {});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEDefinitionTargetKind::SourceTarget),
			static_cast<int32>(SharedDef.Target)));
		FStringView Key;
		ASSERT_THAT(IsTrue(CrossResponse.Result->GetSession()->GetASTContext().GetSources().TryGetLogicalSourceKey(
			SharedDef.Range.Begin.FileID,
			Key)));
		ASSERT_THAT(AreEqual(TEXT("A.as"), FString(Key)));

		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Bare =
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Hosted.as"), HostSource}});
		AS_NAMESPACE_QUALIFIER asCToolingSession BareSession(Bare);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::SnapshotMismatch),
			static_cast<int32>(BareSession.GetHover(*HostedResponse.Result, Add, {}).Status)));
	}

	TEST_METHOD(RejectSamePathForeignResults)
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> ValueInputs =
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Value.as"), TEXT("int Value;")}});
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> OtherInputs =
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Value.as"), TEXT("int Other;")}});
		ASSERT_THAT(AreEqual(static_cast<uint64>(0), ValueInputs->Identity));
		ASSERT_THAT(AreEqual(static_cast<uint64>(0), OtherInputs->Identity));
		AS_NAMESPACE_QUALIFIER asCToolingSession ValueSession(ValueInputs);
		AS_NAMESPACE_QUALIFIER asCToolingSession OtherSession(OtherInputs);
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse ValueResponse = ValueSession.Analyze({});
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse OtherResponse = OtherSession.Analyze({});
		ASSERT_THAT(IsTrue(ValueResponse.Result.IsValid() && OtherResponse.Result.IsValid()));
		AS_NAMESPACE_QUALIFIER asSQueryPosition AtName;
		AtName.LogicalSourceKey = TEXT("Value.as");
		AtName.ByteOffset = 4;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::SnapshotMismatch),
			static_cast<int32>(OtherSession.GetHover(*ValueResponse.Result, AtName, {}).Status)));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::SnapshotMismatch),
			static_cast<int32>(OtherSession.FindDefinition(*ValueResponse.Result, AtName, {}).Status)));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(ValueSession.GetHover(*ValueResponse.Result, AtName, {}).Status)));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(OtherSession.GetHover(*OtherResponse.Result, AtName, {}).Status)));

		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Types =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TSharedPtr<asCDefinitions, ESPMode::ThreadSafe> Host = MakeHostAdd(Types);
		ASSERT_THAT(IsTrue(Host.IsValid()));
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Scoped =
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Value.as"), TEXT("int Value;")}});
		Scoped->Identity = 42;
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> OtherScope =
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Value.as"), TEXT("int Value;")}});
		OtherScope->Identity = 42;
		OtherScope->Options.StableScope = TEXT("other-scope");
		AS_NAMESPACE_QUALIFIER asCToolingSession ScopedSession(Scoped);
		AS_NAMESPACE_QUALIFIER asCToolingSession OtherScopeSession(OtherScope);
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse ScopedResponse = ScopedSession.Analyze({});
		ASSERT_THAT(IsTrue(ScopedResponse.Result.IsValid()));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::SnapshotMismatch),
			static_cast<int32>(OtherScopeSession.GetHover(*ScopedResponse.Result, AtName, {}).Status)));

		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Hosted =
			AngelscriptNativeToolingTest::MakeInputs(
				{{TEXT("Value.as"), TEXT("int Value;")}},
				Types,
				Host);
		Hosted->Identity = 42;
		AS_NAMESPACE_QUALIFIER asCToolingSession HostedSession(Hosted);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::SnapshotMismatch),
			static_cast<int32>(HostedSession.GetHover(*ScopedResponse.Result, AtName, {}).Status)));

		auto* ValueDecl = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCNamedDecl>(
			ValueResponse.Result->GetSession()->Lookup(TEXT("Value"))[0]);
		const AS_NAMESPACE_QUALIFIER asSAnalysisEntityHandle Handle = ValueResponse.Result->MakeHandle(ValueDecl);
		ASSERT_THAT(IsTrue(ValueResponse.Result->OwnsHandle(Handle)));
		ASSERT_THAT(IsFalse(OtherResponse.Result->OwnsHandle(Handle)));
	}

	TEST_METHOD(RetainedProvenanceAndExactTargets)
	{
		const FString Overload = TEXT("int Pick(int A); float Pick(float A); int F(){ return Pick(1); }");
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> OverloadInputs =
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Pick.as"), Overload}});
		AS_NAMESPACE_QUALIFIER asCToolingSession OverloadSession(OverloadInputs);
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse OverloadResponse = OverloadSession.Analyze({});
		ASSERT_THAT(IsTrue(OverloadResponse.Result.IsValid()));
		AS_NAMESPACE_QUALIFIER asSQueryPosition Call;
		Call.LogicalSourceKey = TEXT("Pick.as");
		Call.ByteOffset = static_cast<asUINT>(Overload.Find(TEXT("return Pick")) + 7);
		const AS_NAMESPACE_QUALIFIER asSDefinitionResult CallDef =
			OverloadSession.FindDefinition(*OverloadResponse.Result, Call, {});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEDefinitionTargetKind::SourceTarget),
			static_cast<int32>(CallDef.Target)));
		const int32 IntPick = Overload.Find(TEXT("int Pick(int A)"));
		const int32 FloatPick = Overload.Find(TEXT("float Pick(float A)"));
		ASSERT_THAT(IsTrue(CallDef.Range.Begin.ByteOffset <= static_cast<asUINT>(IntPick)));
		ASSERT_THAT(IsTrue(CallDef.Range.End.ByteOffset > static_cast<asUINT>(IntPick)));
		ASSERT_THAT(IsTrue(
			CallDef.Range.End.ByteOffset <= static_cast<asUINT>(FloatPick)
			|| CallDef.Range.Begin.ByteOffset < static_cast<asUINT>(FloatPick)));

		const FString Member = TEXT("class Item { int Value; } int F(){ Item obj; return obj.Value; }");
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> MemberInputs =
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Member.as"), Member}});
		AS_NAMESPACE_QUALIFIER asCToolingSession MemberSession(MemberInputs);
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse MemberResponse = MemberSession.Analyze({});
		ASSERT_THAT(IsTrue(MemberResponse.Result.IsValid()));
		AS_NAMESPACE_QUALIFIER asSQueryPosition MemberUse;
		MemberUse.LogicalSourceKey = TEXT("Member.as");
		MemberUse.ByteOffset = static_cast<asUINT>(Member.Find(TEXT("obj.Value")) + 4);
		const AS_NAMESPACE_QUALIFIER asSHoverResult MemberHover =
			MemberSession.GetHover(*MemberResponse.Result, MemberUse, {});
		const AS_NAMESPACE_QUALIFIER asSDefinitionResult MemberDef =
			MemberSession.FindDefinition(*MemberResponse.Result, MemberUse, {});
		ASSERT_THAT(IsTrue(MemberHover.Display.Contains(TEXT("Value"))));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEDefinitionTargetKind::SourceTarget),
			static_cast<int32>(MemberDef.Target)));
		ASSERT_THAT(IsTrue(MemberDef.Range.Begin.ByteOffset <= static_cast<asUINT>(Member.Find(TEXT("int Value")))));

		const FString Qualified = TEXT("namespace N { int Shared(){ return 1; } } int F(){ return N::Shared(); }");
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> QualifiedInputs =
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Qual.as"), Qualified}});
		AS_NAMESPACE_QUALIFIER asCToolingSession QualifiedSession(QualifiedInputs);
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse QualifiedResponse = QualifiedSession.Analyze({});
		ASSERT_THAT(IsTrue(QualifiedResponse.Result.IsValid()));
		AS_NAMESPACE_QUALIFIER asSQueryPosition SharedUse;
		SharedUse.LogicalSourceKey = TEXT("Qual.as");
		SharedUse.ByteOffset = static_cast<asUINT>(Qualified.Find(TEXT("N::Shared")) + 3);
		const AS_NAMESPACE_QUALIFIER asSDefinitionResult SharedDef =
			QualifiedSession.FindDefinition(*QualifiedResponse.Result, SharedUse, {});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEDefinitionTargetKind::SourceTarget),
			static_cast<int32>(SharedDef.Target)));
		ASSERT_THAT(IsTrue(SharedDef.Range.Begin.ByteOffset <= static_cast<asUINT>(Qualified.Find(TEXT("int Shared")))));

		const FString Comment = TEXT("int F(){ int Pick=1; /* Pick */ return Pick; }");
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> CommentInputs =
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Comment.as"), Comment}});
		AS_NAMESPACE_QUALIFIER asCToolingSession CommentSession(CommentInputs);
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse CommentResponse = CommentSession.Analyze({});
		ASSERT_THAT(IsTrue(CommentResponse.Result.IsValid()));
		AS_NAMESPACE_QUALIFIER asSQueryPosition InComment;
		InComment.LogicalSourceKey = TEXT("Comment.as");
		InComment.ByteOffset = static_cast<asUINT>(Comment.Find(TEXT("/* Pick")) + 3);
		const AS_NAMESPACE_QUALIFIER asSHoverResult CommentHover =
			CommentSession.GetHover(*CommentResponse.Result, InComment, {});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(CommentHover.Status)));
		ASSERT_THAT(IsTrue(CommentHover.Display.IsEmpty()));

		const FString Space = TEXT("int F(){ int Value=1; return Value ; }");
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> SpaceInputs =
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Space.as"), Space}});
		AS_NAMESPACE_QUALIFIER asCToolingSession SpaceSession(SpaceInputs);
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse SpaceResponse = SpaceSession.Analyze({});
		ASSERT_THAT(IsTrue(SpaceResponse.Result.IsValid()));
		AS_NAMESPACE_QUALIFIER asSQueryPosition Beyond;
		Beyond.LogicalSourceKey = TEXT("Space.as");
		Beyond.ByteOffset = static_cast<asUINT>(Space.Find(TEXT("Value ;")) + 6);
		ASSERT_THAT(IsTrue(SpaceSession.GetHover(*SpaceResponse.Result, Beyond, {}).Display.IsEmpty()));

		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Types =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TSharedPtr<asCDefinitions, ESPMode::ThreadSafe> Host = MakeHostAdd(Types);
		ASSERT_THAT(IsTrue(Host.IsValid()));
		const FString HostSource = TEXT("int F(){ return Host::Add(42); }");
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Hosted =
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Hosted.as"), HostSource}}, Types, Host);
		AS_NAMESPACE_QUALIFIER asCToolingSession HostedSession(Hosted);
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse HostedResponse = HostedSession.Analyze({});
		ASSERT_THAT(IsTrue(HostedResponse.Result.IsValid()));
		Host.Reset();
		Hosted = MakeShared<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe>();
		AS_NAMESPACE_QUALIFIER asSQueryPosition Add;
		Add.LogicalSourceKey = TEXT("Hosted.as");
		Add.ByteOffset = static_cast<asUINT>(HostSource.Find(TEXT("Add")));
		const AS_NAMESPACE_QUALIFIER asSHoverResult HostHover =
			HostedSession.GetHover(*HostedResponse.Result, Add, {});
		const AS_NAMESPACE_QUALIFIER asSDefinitionResult HostDef =
			HostedSession.FindDefinition(*HostedResponse.Result, Add, {});
		ASSERT_THAT(IsTrue(HostHover.Display.Contains(TEXT("Add"))));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEDefinitionTargetKind::NoSourceTarget),
			static_cast<int32>(HostDef.Target)));
		ASSERT_THAT(AreEqual(
			FUtf8String(TCHAR_TO_UTF8(*HostSource)),
			HostedResponse.Result->GetInputs().Sources[0]->GetSourceText()));
	}
};

#endif
