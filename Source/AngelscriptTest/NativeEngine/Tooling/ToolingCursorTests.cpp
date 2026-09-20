#include "CQTest.h"
#include "NativeToolingTestSupport.h"

#include "frontend/AST/as_ast_cast.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ToolingCursor,
	"Angelscript.UnitTest.NativeEngine.Tooling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	static bool StripMarker(const FString& Marked, FString& OutSource, asUINT& OutOffset)
	{
		const int32 Index = Marked.Find(TEXT("|"));
		if (Index == INDEX_NONE)
		{
			return false;
		}
		OutSource = Marked.Left(Index) + Marked.Mid(Index + 1);
		OutOffset = static_cast<asUINT>(Index);
		return true;
	}

	TEST_METHOD(CursorPreservesScopeWithoutSourceEdit)
	{
		FString Source;
		asUINT Offset = 0;
		ASSERT_THAT(IsTrue(StripMarker(
			TEXT("void F(){ int Outer=1; { int Inner=2; | } }"),
			Source,
			Offset)));
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Inputs =
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Scope.as"), Source}});
		AS_NAMESPACE_QUALIFIER asCToolingSession Session(Inputs);
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse Formal = Session.Analyze({});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(Formal.Status)));
		ASSERT_THAT(IsTrue(Formal.Result.IsValid()));
		const FString BeforeDiagnostics = Formal.Result->GetSession()->SerializeStableDiagnostics();
		const FString BeforeBodies = Formal.Result->GetSession()->SerializeStableBodyProjection();
		const AS_NAMESPACE_QUALIFIER asCFunctionDecl* Function =
			AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(
				Formal.Result->GetSession()->Lookup(TEXT("F"))[0]);
		ASSERT_THAT(IsNotNull(Function));
		const AS_NAMESPACE_QUALIFIER asCCompoundStmt* Body =
			AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Function->GetBody());
		ASSERT_THAT(IsNotNull(Body));
		const int32 BeforeStatements = Body->GetStatements().Num();

		AS_NAMESPACE_QUALIFIER asSQueryPosition Position;
		Position.LogicalSourceKey = TEXT("Scope.as");
		Position.ByteOffset = Offset;
		const AS_NAMESPACE_QUALIFIER asSCursorContext Cursor = Session.PrepareCursor(Position, {});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(Cursor.Status)));
		ASSERT_THAT(IsTrue(Cursor.IsNameVisible(TEXT("Inner"))));
		ASSERT_THAT(IsTrue(Cursor.IsNameVisible(TEXT("Outer"))));
		ASSERT_THAT(AreEqual(BeforeDiagnostics, Formal.Result->GetSession()->SerializeStableDiagnostics()));
		ASSERT_THAT(AreEqual(BeforeBodies, Formal.Result->GetSession()->SerializeStableBodyProjection()));
		ASSERT_THAT(IsTrue(Formal.Result->GetSession()->Lookup(TEXT("F"))[0] == Function));
		ASSERT_THAT(AreEqual(
			BeforeStatements,
			AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Function->GetBody())
				->GetStatements().Num()));
		ASSERT_THAT(AreEqual(
			Source,
			FString(Formal.Result->GetInputs().Sources[0]->GetSourceText())));
	}

	TEST_METHOD(BarrierBeforeIncompleteBody)
	{
		FString UseSource;
		asUINT Offset = 0;
		ASSERT_THAT(IsTrue(StripMarker(
			TEXT("void F(){ Item obj; obj.| }"),
			UseSource,
			Offset)));
		const FString TypeSource = TEXT("class Item { int Value; }");
		auto RunOrder = [&](const TArray<TPair<FString, FString>>& Files)
		{
			TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Inputs =
				AngelscriptNativeToolingTest::MakeInputs(Files);
			AS_NAMESPACE_QUALIFIER asCToolingSession Session(Inputs);
			const AS_NAMESPACE_QUALIFIER asSAnalysisResponse Formal = Session.Analyze({});
			ASSERT_THAT(IsTrue(Formal.Result.IsValid()));
			const FString BeforeDiagnostics = Formal.Result->GetSession()->SerializeStableDiagnostics();
			AS_NAMESPACE_QUALIFIER asSQueryPosition Position;
			Position.LogicalSourceKey = TEXT("A.as");
			Position.ByteOffset = Offset;
			const AS_NAMESPACE_QUALIFIER asSCursorContext Cursor = Session.PrepareCursor(Position, {});
			ASSERT_THAT(AreEqual(
				static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
				static_cast<int32>(Cursor.Status)));
			ASSERT_THAT(AreEqual(TEXT("Item"), Cursor.ReceiverTypeName));
			ASSERT_THAT(AreEqual(BeforeDiagnostics, Formal.Result->GetSession()->SerializeStableDiagnostics()));
		};
		RunOrder({{TEXT("A.as"), UseSource}, {TEXT("B.as"), TypeSource}});
		RunOrder({{TEXT("B.as"), TypeSource}, {TEXT("A.as"), UseSource}});

		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Inputs =
			AngelscriptNativeToolingTest::MakeInputs(
				{{TEXT("A.as"), UseSource}, {TEXT("B.as"), TypeSource}});
		AS_NAMESPACE_QUALIFIER asCToolingSession Session(Inputs);
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse Formal = Session.Analyze({});
		bool bCancelled = true;
		AS_NAMESPACE_QUALIFIER asSCancellationToken Token;
		Token.Cancelled = &bCancelled;
		AS_NAMESPACE_QUALIFIER asSQueryPosition Position;
		Position.LogicalSourceKey = TEXT("A.as");
		Position.ByteOffset = Offset;
		const AS_NAMESPACE_QUALIFIER asSCursorContext Cancelled = Session.PrepareCursor(Position, Token);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Cancelled),
			static_cast<int32>(Cancelled.Status)));
		ASSERT_THAT(IsFalse(Cancelled.IsNameVisible(TEXT("obj"))));
		ASSERT_THAT(IsTrue(Cancelled.ReceiverTypeName.IsEmpty()));
		ASSERT_THAT(IsTrue(Formal.Result.IsValid()));
		ASSERT_THAT(AreEqual(1, Formal.Result->GetSession()->Lookup(TEXT("F")).Num()));
	}

	TEST_METHOD(ExitedScopesAndNearestShadow)
	{
		FString Source;
		asUINT Offset = 0;
		ASSERT_THAT(IsTrue(StripMarker(
			TEXT("void F(int Param){ int Outer=1; { int Expired=2; } | for(int Loop=0; Loop<1; ++Loop){} { int Sibling=3; } int Later=4; }"),
			Source,
			Offset)));
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Inputs =
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Scope.as"), Source}});
		AS_NAMESPACE_QUALIFIER asCToolingSession Session(Inputs);
		AS_NAMESPACE_QUALIFIER asSQueryPosition Position;
		Position.LogicalSourceKey = TEXT("Scope.as");
		Position.ByteOffset = Offset;
		const AS_NAMESPACE_QUALIFIER asSCursorContext Cursor = Session.PrepareCursor(Position, {});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(Cursor.Status)));
		ASSERT_THAT(IsTrue(Cursor.IsNameVisible(TEXT("Param"))));
		ASSERT_THAT(IsTrue(Cursor.IsNameVisible(TEXT("Outer"))));
		ASSERT_THAT(IsFalse(Cursor.IsNameVisible(TEXT("Expired"))));
		ASSERT_THAT(IsFalse(Cursor.IsNameVisible(TEXT("Loop"))));
		ASSERT_THAT(IsFalse(Cursor.IsNameVisible(TEXT("Sibling"))));
		ASSERT_THAT(IsFalse(Cursor.IsNameVisible(TEXT("Later"))));

		FString ShadowSource;
		asUINT ShadowOffset = 0;
		ASSERT_THAT(IsTrue(StripMarker(
			TEXT("void F(){ int Value=1; { int Value=2; | } }"),
			ShadowSource,
			ShadowOffset)));
		AS_NAMESPACE_QUALIFIER asCToolingSession ShadowSession(
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Shadow.as"), ShadowSource}}));
		AS_NAMESPACE_QUALIFIER asSQueryPosition ShadowPos;
		ShadowPos.LogicalSourceKey = TEXT("Shadow.as");
		ShadowPos.ByteOffset = ShadowOffset;
		const AS_NAMESPACE_QUALIFIER asSCursorContext Shadow = ShadowSession.PrepareCursor(ShadowPos, {});
		int32 ValueCount = 0;
		for (const FString& Name : Shadow.VisibleNames)
		{
			if (Name == TEXT("Value"))
			{
				++ValueCount;
			}
		}
		ASSERT_THAT(AreEqual(1, ValueCount));
		ASSERT_THAT(IsTrue(Shadow.IsNameVisible(TEXT("Value"))));
	}
};

#endif
