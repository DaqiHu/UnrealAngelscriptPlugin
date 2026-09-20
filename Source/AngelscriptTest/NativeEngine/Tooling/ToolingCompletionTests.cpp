#include "CQTest.h"
#include "NativeToolingTestSupport.h"
#include "as_builder.h"
#include "as_scriptfunction.h"
#include "frontend/AST/as_type_context.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ToolingCompletion,
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

	static bool HasItem(
		const AS_NAMESPACE_QUALIFIER asSCompletionResult& Result,
		const FString& Name)
	{
		for (const AS_NAMESPACE_QUALIFIER asSCompletionItem& Item : Result.Items)
		{
			if (Item.Name == Name)
			{
				return true;
			}
		}
		return false;
	}

	TEST_METHOD(ScopeAndMemberCompletion)
	{
		FString MemberSource;
		asUINT MemberOffset = 0;
		ASSERT_THAT(IsTrue(StripMarker(
			TEXT("class Item { int Value; private int Hidden; } void F(){ Item obj; obj.| }"),
			MemberSource,
			MemberOffset)));
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> MemberInputs =
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Member.as"), MemberSource}});
		AS_NAMESPACE_QUALIFIER asCToolingSession MemberSession(MemberInputs);
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse Formal = MemberSession.Analyze({});
		ASSERT_THAT(IsTrue(Formal.Result.IsValid()));
		const FString Before = Formal.Result->GetSession()->SerializeStableDiagnostics();
		AS_NAMESPACE_QUALIFIER asSQueryPosition MemberPos;
		MemberPos.LogicalSourceKey = TEXT("Member.as");
		MemberPos.ByteOffset = MemberOffset;
		const AS_NAMESPACE_QUALIFIER asSCompletionResult Members = MemberSession.Complete(MemberPos, {});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(Members.Status)));
		ASSERT_THAT(IsTrue(HasItem(Members, TEXT("Value"))));
		ASSERT_THAT(IsFalse(HasItem(Members, TEXT("Hidden"))));

		FString ScopeSource;
		asUINT ScopeOffset = 0;
		ASSERT_THAT(IsTrue(StripMarker(
			TEXT("void F(){ int Outer=1; { int Inner=2; | int Later=3; } }"),
			ScopeSource,
			ScopeOffset)));
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> ScopeInputs =
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Scope.as"), ScopeSource}});
		AS_NAMESPACE_QUALIFIER asCToolingSession ScopeSession(ScopeInputs);
		AS_NAMESPACE_QUALIFIER asSQueryPosition ScopePos;
		ScopePos.LogicalSourceKey = TEXT("Scope.as");
		ScopePos.ByteOffset = ScopeOffset;
		const AS_NAMESPACE_QUALIFIER asSCompletionResult Scope = ScopeSession.Complete(ScopePos, {});
		ASSERT_THAT(IsTrue(HasItem(Scope, TEXT("Inner"))));
		ASSERT_THAT(IsTrue(HasItem(Scope, TEXT("Outer"))));
		ASSERT_THAT(IsFalse(HasItem(Scope, TEXT("Later"))));

		FString PartialSource;
		asUINT PartialOffset = 0;
		ASSERT_THAT(IsTrue(StripMarker(
			TEXT("class Item { int Value; } void F(){ Item obj; obj.Va| }"),
			PartialSource,
			PartialOffset)));
		AS_NAMESPACE_QUALIFIER asCToolingSession PartialSession(
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Partial.as"), PartialSource}}));
		AS_NAMESPACE_QUALIFIER asSQueryPosition PartialPos;
		PartialPos.LogicalSourceKey = TEXT("Partial.as");
		PartialPos.ByteOffset = PartialOffset;
		const AS_NAMESPACE_QUALIFIER asSCompletionResult Partial = PartialSession.Complete(PartialPos, {});
		ASSERT_THAT(IsTrue(HasItem(Partial, TEXT("Value"))));
		bool bFoundReplacement = false;
		for (const AS_NAMESPACE_QUALIFIER asSCompletionItem& Item : Partial.Items)
		{
			if (Item.Name == TEXT("Value") && Item.Replacement.IsOrdered())
			{
				bFoundReplacement = Item.Replacement.End.ByteOffset > Item.Replacement.Begin.ByteOffset;
			}
		}
		ASSERT_THAT(IsTrue(bFoundReplacement));
		ASSERT_THAT(AreEqual(Before, Formal.Result->GetSession()->SerializeStableDiagnostics()));
	}

	TEST_METHOD(RealSignatureContext)
	{
		auto HelpAt = [](const FString& Marked)
		{
			FString Source;
			asUINT Offset = 0;
			StripMarker(Marked, Source, Offset);
			const FString Header = TEXT("int Pick(int First, int Second=2);\nint Outer(int A, int B);\nint Inner(int X, int Y);\n");
			TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Inputs =
				AngelscriptNativeToolingTest::MakeInputs({{TEXT("Pick.as"), Header + Source}});
			AS_NAMESPACE_QUALIFIER asCToolingSession Session(Inputs);
			AS_NAMESPACE_QUALIFIER asSQueryPosition Position;
			Position.LogicalSourceKey = TEXT("Pick.as");
			Position.ByteOffset = Offset + static_cast<asUINT>(Header.Len());
			return Session.GetSignatureHelp(Position, {});
		};

		const AS_NAMESPACE_QUALIFIER asSSignatureHelpResult Open = HelpAt(TEXT("int F(){ Pick(| }"));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(Open.Status)));
		ASSERT_THAT(AreEqual(TEXT("Pick"), Open.CallName));
		ASSERT_THAT(AreEqual(0, Open.ActiveParameter));

		const AS_NAMESPACE_QUALIFIER asSSignatureHelpResult Second = HelpAt(TEXT("int F(){ Pick(1, | }"));
		ASSERT_THAT(AreEqual(TEXT("Pick"), Second.CallName));
		ASSERT_THAT(AreEqual(1, Second.ActiveParameter));

		const AS_NAMESPACE_QUALIFIER asSSignatureHelpResult Named = HelpAt(TEXT("int F(){ Pick(Second: | }"));
		ASSERT_THAT(AreEqual(TEXT("Pick"), Named.CallName));
		ASSERT_THAT(AreEqual(1, Named.ActiveParameter));

		const AS_NAMESPACE_QUALIFIER asSSignatureHelpResult Nested = HelpAt(TEXT("int F(){ Outer(Inner(1, | }"));
		ASSERT_THAT(AreEqual(TEXT("Inner"), Nested.CallName));
		ASSERT_THAT(AreEqual(1, Nested.ActiveParameter));

		FString StringSource;
		asUINT StringOffset = 0;
		ASSERT_THAT(IsTrue(StripMarker(
			TEXT("int F(){ /* | */ return 1; }"),
			StringSource,
			StringOffset)));
		AS_NAMESPACE_QUALIFIER asCToolingSession CommentSession(
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Comment.as"), StringSource}}));
		AS_NAMESPACE_QUALIFIER asSQueryPosition CommentPos;
		CommentPos.LogicalSourceKey = TEXT("Comment.as");
		CommentPos.ByteOffset = StringOffset;
		const AS_NAMESPACE_QUALIFIER asSCompletionResult Comment = CommentSession.Complete(CommentPos, {});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(Comment.Status)));
		ASSERT_THAT(AreEqual(0, Comment.Items.Num()));
	}

	TEST_METHOD(ExitedScopesAndNearestShadow)
	{
		FString Source;
		asUINT Offset = 0;
		ASSERT_THAT(IsTrue(StripMarker(
			TEXT("void F(int Param){ int Outer=1; { int Expired=2; } | }"),
			Source,
			Offset)));
		AS_NAMESPACE_QUALIFIER asCToolingSession Session(
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Scope.as"), Source}}));
		AS_NAMESPACE_QUALIFIER asSQueryPosition Position;
		Position.LogicalSourceKey = TEXT("Scope.as");
		Position.ByteOffset = Offset;
		const AS_NAMESPACE_QUALIFIER asSCompletionResult Result = Session.Complete(Position, {});
		ASSERT_THAT(IsTrue(HasItem(Result, TEXT("Param"))));
		ASSERT_THAT(IsTrue(HasItem(Result, TEXT("Outer"))));
		ASSERT_THAT(IsFalse(HasItem(Result, TEXT("Expired"))));
	}

	TEST_METHOD(GrammarExpectedTypeAndSourceOwnership)
	{
		const FString Header = TEXT("namespace N { class Item {} }\n");
		FString FileSource;
		asUINT FileOffset = 0;
		ASSERT_THAT(IsTrue(StripMarker(Header + TEXT("|"), FileSource, FileOffset)));
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> FileInputs =
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("File.as"), FileSource}});
		AS_NAMESPACE_QUALIFIER asCToolingSession FileSession(FileInputs);
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse Formal = FileSession.Analyze({});
		ASSERT_THAT(IsTrue(Formal.Result.IsValid()));
		const FString BeforeDiag = Formal.Result->GetSession()->SerializeStableDiagnostics();
		const FString BeforeBody = Formal.Result->GetSession()->SerializeStableBodyProjection();
		AS_NAMESPACE_QUALIFIER asSQueryPosition FilePos;
		FilePos.LogicalSourceKey = TEXT("File.as");
		FilePos.ByteOffset = FileOffset;
		const AS_NAMESPACE_QUALIFIER asSCompletionResult FileComplete = FileSession.Complete(FilePos, {});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(FileComplete.Status)));
		ASSERT_THAT(IsTrue(HasItem(FileComplete, TEXT("N"))));
		ASSERT_THAT(IsTrue(HasItem(FileComplete, TEXT("Item"))));
		ASSERT_THAT(IsTrue(HasItem(FileComplete, TEXT("class"))));
		ASSERT_THAT(IsTrue(HasItem(FileComplete, TEXT("namespace"))));
		ASSERT_THAT(IsTrue(HasItem(FileComplete, TEXT("int"))));
		ASSERT_THAT(IsFalse(HasItem(FileComplete, TEXT("function"))));
		ASSERT_THAT(IsFalse(HasItem(FileComplete, TEXT("lambda"))));
		const AS_NAMESPACE_QUALIFIER asSCompletionResult Repeat = FileSession.Complete(FilePos, {});
		ASSERT_THAT(AreEqual(FileComplete.Items.Num(), Repeat.Items.Num()));
		ASSERT_THAT(AreEqual(BeforeDiag, Formal.Result->GetSession()->SerializeStableDiagnostics()));
		ASSERT_THAT(AreEqual(BeforeBody, Formal.Result->GetSession()->SerializeStableBodyProjection()));
		ASSERT_THAT(AreEqual(FileSource, FString(Formal.Result->GetInputs().Sources[0]->GetSourceText())));

		FString RankSource;
		asUINT RankOffset = 0;
		ASSERT_THAT(IsTrue(StripMarker(
			TEXT("int F(){ float Wrong=1; int Ready=2; return | }"),
			RankSource,
			RankOffset)));
		AS_NAMESPACE_QUALIFIER asCToolingSession RankSession(
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Rank.as"), RankSource}}));
		AS_NAMESPACE_QUALIFIER asSQueryPosition RankPos;
		RankPos.LogicalSourceKey = TEXT("Rank.as");
		RankPos.ByteOffset = RankOffset;
		const AS_NAMESPACE_QUALIFIER asSCompletionResult Ranked = RankSession.Complete(RankPos, {});
		int32 ReadyRank = MAX_int32;
		int32 WrongRank = MAX_int32;
		for (const AS_NAMESPACE_QUALIFIER asSCompletionItem& Item : Ranked.Items)
		{
			if (Item.Name == TEXT("Ready"))
			{
				ReadyRank = Item.Rank;
			}
			if (Item.Name == TEXT("Wrong"))
			{
				WrongRank = Item.Rank;
			}
		}
		ASSERT_THAT(IsTrue(HasItem(Ranked, TEXT("Ready"))));
		ASSERT_THAT(IsTrue(HasItem(Ranked, TEXT("Wrong"))));
		ASSERT_THAT(IsTrue(ReadyRank < WrongRank));

		FString PartialSource;
		asUINT PartialOffset = 0;
		ASSERT_THAT(IsTrue(StripMarker(
			TEXT("class Item { int Value; } void F(){ Item obj; obj.Va| }"),
			PartialSource,
			PartialOffset)));
		AS_NAMESPACE_QUALIFIER asCToolingSession PartialSession(
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Partial.as"), PartialSource}}));
		AS_NAMESPACE_QUALIFIER asSQueryPosition PartialPos;
		PartialPos.LogicalSourceKey = TEXT("Partial.as");
		PartialPos.ByteOffset = PartialOffset;
		const AS_NAMESPACE_QUALIFIER asSCompletionResult Partial = PartialSession.Complete(PartialPos, {});
		bool bVaOnly = false;
		for (const AS_NAMESPACE_QUALIFIER asSCompletionItem& Item : Partial.Items)
		{
			if (Item.Name == TEXT("Value") && Item.Replacement.IsOrdered())
			{
				bVaOnly = Item.Replacement.End.ByteOffset - Item.Replacement.Begin.ByteOffset == 2;
			}
		}
		ASSERT_THAT(IsTrue(bVaOnly));
	}

	TEST_METHOD(ProtectedMemberFromFreeFunction)
	{
		FString Source;
		asUINT Offset = 0;
		ASSERT_THAT(IsTrue(StripMarker(
			TEXT("class Item { protected int Hidden; int Value; } void F(){ Item obj; obj.| }"),
			Source,
			Offset)));
		AS_NAMESPACE_QUALIFIER asCToolingSession Session(
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Protected.as"), Source}}));
		AS_NAMESPACE_QUALIFIER asSQueryPosition Position;
		Position.LogicalSourceKey = TEXT("Protected.as");
		Position.ByteOffset = Offset;
		const AS_NAMESPACE_QUALIFIER asSCompletionResult Result = Session.Complete(Position, {});
		ASSERT_THAT(IsTrue(HasItem(Result, TEXT("Value"))));
		ASSERT_THAT(IsFalse(HasItem(Result, TEXT("Hidden"))));

		const FString Denied =
			TEXT("class Item { protected int Hidden; int Value; } void F(){ Item obj; obj.Hidden; }");
		AS_NAMESPACE_QUALIFIER asCToolingSession DeniedSession(
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Denied.as"), Denied}}));
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse DeniedAnalysis = DeniedSession.Analyze({});
		ASSERT_THAT(IsTrue(DeniedAnalysis.Result.IsValid()));
		ASSERT_THAT(IsTrue(DeniedAnalysis.Result->GetSession()->SerializeStableBodyDiagnostics().Contains(
			TEXT("inaccessible-member"))));

		const FString Allowed =
			TEXT("class Item { protected int Hidden; int Value; } void F(){ Item obj; obj.Value; }");
		AS_NAMESPACE_QUALIFIER asCToolingSession AllowedSession(
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Allowed.as"), Allowed}}));
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse AllowedAnalysis = AllowedSession.Analyze({});
		ASSERT_THAT(IsTrue(AllowedAnalysis.Result.IsValid()));
		ASSERT_THAT(IsFalse(AllowedAnalysis.Result->GetSession()->SerializeStableBodyDiagnostics().Contains(
			TEXT("inaccessible-member"))));
	}

	TEST_METHOD(LegalClassAccessAndCompatibleReceiver)
	{
		FString InsideSource;
		asUINT InsideOffset = 0;
		ASSERT_THAT(IsTrue(StripMarker(
			TEXT("class Item { private int Secret; protected int Family; int Value; void Mutate(); void Peek() const; void Inside(){ this.| } }"),
			InsideSource,
			InsideOffset)));
		AS_NAMESPACE_QUALIFIER asCToolingSession InsideSession(
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Inside.as"), InsideSource}}));
		AS_NAMESPACE_QUALIFIER asSQueryPosition InsidePos;
		InsidePos.LogicalSourceKey = TEXT("Inside.as");
		InsidePos.ByteOffset = InsideOffset;
		const AS_NAMESPACE_QUALIFIER asSCompletionResult Inside = InsideSession.Complete(InsidePos, {});
		ASSERT_THAT(IsTrue(HasItem(Inside, TEXT("Secret"))));
		ASSERT_THAT(IsTrue(HasItem(Inside, TEXT("Family"))));
		ASSERT_THAT(IsTrue(HasItem(Inside, TEXT("Value"))));
		ASSERT_THAT(IsTrue(HasItem(Inside, TEXT("Mutate"))));
		ASSERT_THAT(IsTrue(HasItem(Inside, TEXT("Peek"))));

		FString DerivedSource;
		asUINT DerivedOffset = 0;
		ASSERT_THAT(IsTrue(StripMarker(
			TEXT("class Item { private int Secret; protected int Family; int Value; } class Child : Item { void Derived(){ this.| } }"),
			DerivedSource,
			DerivedOffset)));
		AS_NAMESPACE_QUALIFIER asCToolingSession DerivedSession(
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Derived.as"), DerivedSource}}));
		AS_NAMESPACE_QUALIFIER asSQueryPosition DerivedPos;
		DerivedPos.LogicalSourceKey = TEXT("Derived.as");
		DerivedPos.ByteOffset = DerivedOffset;
		const AS_NAMESPACE_QUALIFIER asSCompletionResult Derived = DerivedSession.Complete(DerivedPos, {});
		ASSERT_THAT(IsTrue(HasItem(Derived, TEXT("Family"))));
		ASSERT_THAT(IsTrue(HasItem(Derived, TEXT("Value"))));
		ASSERT_THAT(IsFalse(HasItem(Derived, TEXT("Secret"))));

		FString ConstSource;
		asUINT ConstOffset = 0;
		ASSERT_THAT(IsTrue(StripMarker(
			TEXT("class Item { void Mutate(); void Peek() const; } void F(){ const Item obj; obj.| }"),
			ConstSource,
			ConstOffset)));
		AS_NAMESPACE_QUALIFIER asCToolingSession ConstSession(
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Const.as"), ConstSource}}));
		AS_NAMESPACE_QUALIFIER asSQueryPosition ConstPos;
		ConstPos.LogicalSourceKey = TEXT("Const.as");
		ConstPos.ByteOffset = ConstOffset;
		const AS_NAMESPACE_QUALIFIER asSCompletionResult ConstReceiver = ConstSession.Complete(ConstPos, {});
		ASSERT_THAT(IsTrue(HasItem(ConstReceiver, TEXT("Peek"))));
		ASSERT_THAT(IsFalse(HasItem(ConstReceiver, TEXT("Mutate"))));

		FString MutableSource;
		asUINT MutableOffset = 0;
		ASSERT_THAT(IsTrue(StripMarker(
			TEXT("class Item { void Mutate(); void Peek() const; } void F(){ Item obj; obj.| }"),
			MutableSource,
			MutableOffset)));
		AS_NAMESPACE_QUALIFIER asCToolingSession MutableSession(
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Mutable.as"), MutableSource}}));
		AS_NAMESPACE_QUALIFIER asSQueryPosition MutablePos;
		MutablePos.LogicalSourceKey = TEXT("Mutable.as");
		MutablePos.ByteOffset = MutableOffset;
		const AS_NAMESPACE_QUALIFIER asSCompletionResult MutableReceiver = MutableSession.Complete(MutablePos, {});
		ASSERT_THAT(IsTrue(HasItem(MutableReceiver, TEXT("Peek"))));
		ASSERT_THAT(IsTrue(HasItem(MutableReceiver, TEXT("Mutate"))));

		FString UnrelatedSource;
		asUINT UnrelatedOffset = 0;
		ASSERT_THAT(IsTrue(StripMarker(
			TEXT("class Other { int OtherValue; } class Item { int Value; } void F(){ Item obj; obj.| }"),
			UnrelatedSource,
			UnrelatedOffset)));
		AS_NAMESPACE_QUALIFIER asCToolingSession UnrelatedSession(
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Unrelated.as"), UnrelatedSource}}));
		AS_NAMESPACE_QUALIFIER asSQueryPosition UnrelatedPos;
		UnrelatedPos.LogicalSourceKey = TEXT("Unrelated.as");
		UnrelatedPos.ByteOffset = UnrelatedOffset;
		const AS_NAMESPACE_QUALIFIER asSCompletionResult Unrelated = UnrelatedSession.Complete(UnrelatedPos, {});
		ASSERT_THAT(IsTrue(HasItem(Unrelated, TEXT("Value"))));
		ASSERT_THAT(IsFalse(HasItem(Unrelated, TEXT("OtherValue"))));
	}

	static const AS_NAMESPACE_QUALIFIER asSSignatureCandidate* FindSignature(
		const AS_NAMESPACE_QUALIFIER asSSignatureHelpResult& Result,
		int32 ParameterCount)
	{
		for (const AS_NAMESPACE_QUALIFIER asSSignatureCandidate& Signature : Result.Signatures)
		{
			if (Signature.Parameters.Num() == ParameterCount)
			{
				return &Signature;
			}
		}
		return nullptr;
	}

	TEST_METHOD(ObservableOverloadAndNamedArgumentPayload)
	{
		const FString Header = TEXT("int Pick(int First, int Second=2);\nfloat Pick(float First);\n");
		auto HelpAt = [&Header](const FString& Marked)
		{
			FString Source;
			asUINT Offset = 0;
			StripMarker(Marked, Source, Offset);
			TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Inputs =
				AngelscriptNativeToolingTest::MakeInputs({{TEXT("Pick.as"), Header + Source}});
			AS_NAMESPACE_QUALIFIER asCToolingSession Session(Inputs);
			AS_NAMESPACE_QUALIFIER asSQueryPosition Position;
			Position.LogicalSourceKey = TEXT("Pick.as");
			Position.ByteOffset = Offset + static_cast<asUINT>(Header.Len());
			return Session.GetSignatureHelp(Position, {});
		};

		const AS_NAMESPACE_QUALIFIER asSSignatureHelpResult Open = HelpAt(TEXT("int F(){ Pick(| }"));
		ASSERT_THAT(AreEqual(TEXT("Pick"), Open.CallName));
		ASSERT_THAT(AreEqual(2, Open.Signatures.Num()));
		const AS_NAMESPACE_QUALIFIER asSSignatureCandidate* Wide = FindSignature(Open, 2);
		const AS_NAMESPACE_QUALIFIER asSSignatureCandidate* Narrow = FindSignature(Open, 1);
		ASSERT_THAT(IsTrue(Wide != nullptr));
		ASSERT_THAT(IsTrue(Narrow != nullptr));
		ASSERT_THAT(AreEqual(TEXT("First"), Wide->Parameters[0].Name));
		ASSERT_THAT(AreEqual(TEXT("Second"), Wide->Parameters[1].Name));
		ASSERT_THAT(IsTrue(Wide->Parameters[1].bHasDefault));
		ASSERT_THAT(AreEqual(TEXT("First"), Narrow->Parameters[0].Name));
		ASSERT_THAT(IsTrue(Wide->bViable));
		ASSERT_THAT(IsTrue(Narrow->bViable));
		bool bSawUnknownFuture = false;
		for (const AS_NAMESPACE_QUALIFIER asEArgumentSlotKind Slot : Wide->FormalSlots)
		{
			bSawUnknownFuture |= Slot == AS_NAMESPACE_QUALIFIER asEArgumentSlotKind::UnknownFuture;
		}
		ASSERT_THAT(IsTrue(bSawUnknownFuture));

		const AS_NAMESPACE_QUALIFIER asSSignatureHelpResult Named = HelpAt(TEXT("int F(){ Pick(Second: 4, First: 3| }"));
		const AS_NAMESPACE_QUALIFIER asSSignatureCandidate* NamedWide = FindSignature(Named, 2);
		ASSERT_THAT(IsTrue(NamedWide != nullptr));
		ASSERT_THAT(IsTrue(NamedWide->bViable));
		ASSERT_THAT(AreEqual(2, NamedWide->AuthoredToFormal.Num()));
		ASSERT_THAT(AreEqual(1, NamedWide->AuthoredToFormal[0]));
		ASSERT_THAT(AreEqual(0, NamedWide->AuthoredToFormal[1]));

		const AS_NAMESPACE_QUALIFIER asSSignatureHelpResult Duplicate = HelpAt(TEXT("int F(){ Pick(First: 1, First: | }"));
		const AS_NAMESPACE_QUALIFIER asSSignatureCandidate* DuplicateWide = FindSignature(Duplicate, 2);
		ASSERT_THAT(IsTrue(DuplicateWide != nullptr));
		ASSERT_THAT(IsFalse(DuplicateWide->bViable));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asECandidateRejection::DuplicateNamedArgument),
			static_cast<int32>(DuplicateWide->Rejection)));

		const AS_NAMESPACE_QUALIFIER asSSignatureHelpResult Incompatible = HelpAt(TEXT("int F(){ Pick(true, | }"));
		bool bSawIncompatible = false;
		for (const AS_NAMESPACE_QUALIFIER asSSignatureCandidate& Signature : Incompatible.Signatures)
		{
			bSawIncompatible |= Signature.Rejection == AS_NAMESPACE_QUALIFIER asECandidateRejection::IncompatibleArgument;
		}
		ASSERT_THAT(IsTrue(bSawIncompatible));
	}

	TEST_METHOD(NestedHostAndNonmutationBoundaries)
	{
		FString InnerSource;
		asUINT InnerOffset = 0;
		ASSERT_THAT(IsTrue(StripMarker(
			TEXT("int Outer(int A, int B);\nint Inner(int X, int Y);\nint F(){ Outer(Inner(1, | }"),
			InnerSource,
			InnerOffset)));
		AS_NAMESPACE_QUALIFIER asCToolingSession InnerSession(
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Nested.as"), InnerSource}}));
		AS_NAMESPACE_QUALIFIER asSQueryPosition InnerPos;
		InnerPos.LogicalSourceKey = TEXT("Nested.as");
		InnerPos.ByteOffset = InnerOffset;
		const AS_NAMESPACE_QUALIFIER asSSignatureHelpResult Inner = InnerSession.GetSignatureHelp(InnerPos, {});
		ASSERT_THAT(AreEqual(TEXT("Inner"), Inner.CallName));
		ASSERT_THAT(AreEqual(1, Inner.ActiveParameter));

		FString OuterSource;
		asUINT OuterOffset = 0;
		ASSERT_THAT(IsTrue(StripMarker(
			TEXT("int Outer(int A, int B);\nint Inner(int X, int Y);\nint F(){ Outer(Inner(1), | }"),
			OuterSource,
			OuterOffset)));
		AS_NAMESPACE_QUALIFIER asCToolingSession OuterSession(
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Nested.as"), OuterSource}}));
		AS_NAMESPACE_QUALIFIER asSQueryPosition OuterPos;
		OuterPos.LogicalSourceKey = TEXT("Nested.as");
		OuterPos.ByteOffset = OuterOffset;
		const AS_NAMESPACE_QUALIFIER asSSignatureHelpResult Outer = OuterSession.GetSignatureHelp(OuterPos, {});
		ASSERT_THAT(AreEqual(TEXT("Outer"), Outer.CallName));
		ASSERT_THAT(AreEqual(1, Outer.ActiveParameter));

		FString ListSource;
		asUINT ListOffset = 0;
		ASSERT_THAT(IsTrue(StripMarker(
			TEXT("int Outer(int A, int B);\nint F(){ Outer({1, 2, |}) }"),
			ListSource,
			ListOffset)));
		AS_NAMESPACE_QUALIFIER asCToolingSession ListSession(
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("List.as"), ListSource}}));
		AS_NAMESPACE_QUALIFIER asSQueryPosition ListPos;
		ListPos.LogicalSourceKey = TEXT("List.as");
		ListPos.ByteOffset = ListOffset;
		const AS_NAMESPACE_QUALIFIER asSSignatureHelpResult List = ListSession.GetSignatureHelp(ListPos, {});
		ASSERT_THAT(AreEqual(TEXT("Outer"), List.CallName));
		ASSERT_THAT(AreEqual(0, List.ActiveParameter));

		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> TypeContext =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TUniquePtr<asCDefinitions> Set = MakeUnique<asCDefinitions>();
		ASSERT_THAT(IsTrue(Set->SetIdentityContext(TypeContext) == asEDefinitionResult::Succeeded));
		AS_NAMESPACE_QUALIFIER asSStableKey IntType;
		ASSERT_THAT(IsTrue(
			TypeContext->InternPrimitiveTypeUse(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32, IntType)
			== AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded));
		asSFunctionIdentityDescriptor WideIdentity;
		WideIdentity.StableScope = TEXT("tooling-host");
		WideIdentity.NamespacePath = {TEXT("Host")};
		WideIdentity.Name = TEXT("Pick");
		WideIdentity.Kind = AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Global;
		WideIdentity.ReturnType = IntType;
		WideIdentity.Parameters.Add({IntType});
		WideIdentity.Parameters.Add({IntType});
		AS_NAMESPACE_QUALIFIER asSStableKey WideKey;
		asCScriptFunction* Wide = nullptr;
		asSDefinitionParameter First;
		First.Name = "First";
		First.Type = Set->Primitive(ttInt);
		asSDefinitionParameter Second;
		Second.Name = "Second";
		Second.Type = Set->Primitive(ttInt);
		Second.DefaultArgument = "2";
		ASSERT_THAT(IsTrue(
			TypeContext->GetIdentityRegistry().InternFunction(WideIdentity, WideKey)
			== AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded
			&& Set->CreateFunction(WideKey, "Pick", "Host", asFUNC_SCRIPT, Wide) == asEDefinitionResult::Succeeded
			&& Set->DefineFunction(*Wide, Set->Primitive(ttInt), {First, Second}) == asEDefinitionResult::Succeeded
			&& Set->SetFunctionDeclarationFacts(*Wide, AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Global)
				== asEDefinitionResult::Succeeded));
		asSFunctionIdentityDescriptor NarrowIdentity = WideIdentity;
		NarrowIdentity.Parameters.Reset();
		NarrowIdentity.Parameters.Add({IntType});
		AS_NAMESPACE_QUALIFIER asSStableKey NarrowKey;
		asCScriptFunction* Narrow = nullptr;
		asSDefinitionParameter Value;
		Value.Name = "Value";
		Value.Type = Set->Primitive(ttInt);
		ASSERT_THAT(IsTrue(
			TypeContext->GetIdentityRegistry().InternFunction(NarrowIdentity, NarrowKey)
			== AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded
			&& Set->CreateFunction(NarrowKey, "Pick", "Host", asFUNC_SCRIPT, Narrow) == asEDefinitionResult::Succeeded
			&& Set->DefineFunction(*Narrow, Set->Primitive(ttInt), {Value}) == asEDefinitionResult::Succeeded
			&& Set->SetFunctionDeclarationFacts(*Narrow, AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Global)
				== asEDefinitionResult::Succeeded
			&& Set->FinalizeLayouts() == asEDefinitionResult::Succeeded
			&& Set->Freeze() == asEDefinitionResult::Succeeded
			&& Set->ValidateFrozenDefinitions()));
		TSharedPtr<asCDefinitions, ESPMode::ThreadSafe> Host(Set.Release());
		FString HostSource;
		asUINT HostOffset = 0;
		ASSERT_THAT(IsTrue(StripMarker(TEXT("int F(){ Host::Pick(| }"), HostSource, HostOffset)));
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> HostInputs =
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Host.as"), HostSource}}, TypeContext, Host);
		AS_NAMESPACE_QUALIFIER asCToolingSession HostSession(HostInputs);
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse Formal = HostSession.Analyze({});
		ASSERT_THAT(IsTrue(Formal.Result.IsValid()));
		const FString BeforeDiag = Formal.Result->GetSession()->SerializeStableDiagnostics();
		const FString BeforeBody = Formal.Result->GetSession()->SerializeStableBodyProjection();
		const int32 BeforeNodes = Formal.Result->GetASTContext()->GetNodeCount();
		AS_NAMESPACE_QUALIFIER asSQueryPosition HostPos;
		HostPos.LogicalSourceKey = TEXT("Host.as");
		HostPos.ByteOffset = HostOffset;
		const AS_NAMESPACE_QUALIFIER asSSignatureHelpResult FirstHelp = HostSession.GetSignatureHelp(HostPos, {});
		const AS_NAMESPACE_QUALIFIER asSSignatureHelpResult RepeatHelp = HostSession.GetSignatureHelp(HostPos, {});
		int32 Unsupported = 0;
		int32 Viable = 0;
		for (const AS_NAMESPACE_QUALIFIER asSSignatureCandidate& Signature : FirstHelp.Signatures)
		{
			if (Signature.Rejection == AS_NAMESPACE_QUALIFIER asECandidateRejection::UnsupportedDefault)
			{
				++Unsupported;
				ASSERT_THAT(IsFalse(Signature.bViable));
			}
			if (Signature.bViable)
			{
				++Viable;
			}
		}
		ASSERT_THAT(AreEqual(1, Unsupported));
		ASSERT_THAT(AreEqual(1, Viable));
		ASSERT_THAT(AreEqual(FirstHelp.Signatures.Num(), RepeatHelp.Signatures.Num()));
		ASSERT_THAT(AreEqual(BeforeDiag, Formal.Result->GetSession()->SerializeStableDiagnostics()));
		ASSERT_THAT(AreEqual(BeforeBody, Formal.Result->GetSession()->SerializeStableBodyProjection()));
		ASSERT_THAT(AreEqual(BeforeNodes, Formal.Result->GetASTContext()->GetNodeCount()));

		AS_NAMESPACE_QUALIFIER asSSignatureHelpResult Retained = FirstHelp;
		{
			AS_NAMESPACE_QUALIFIER asCToolingSession Dropped(HostInputs);
			Retained = Dropped.GetSignatureHelp(HostPos, {});
		}
		ASSERT_THAT(AreEqual(FirstHelp.Signatures.Num(), Retained.Signatures.Num()));
		ASSERT_THAT(AreEqual(FirstHelp.CallName, Retained.CallName));
	}

	TEST_METHOD(InvalidUnavailableAndEmptyAreDistinct)
	{
		const FString ValidText = TEXT("void F(){}");
		AS_NAMESPACE_QUALIFIER asCToolingSession Session(
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Valid.as"), ValidText}}));
		AS_NAMESPACE_QUALIFIER asSQueryPosition Missing;
		Missing.LogicalSourceKey = TEXT("Missing.as");
		Missing.ByteOffset = 0;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::InvalidPosition),
			static_cast<int32>(Session.Complete(Missing, {}).Status)));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::InvalidPosition),
			static_cast<int32>(Session.GetSignatureHelp(Missing, {}).Status)));

		AS_NAMESPACE_QUALIFIER asSQueryPosition PastEnd;
		PastEnd.LogicalSourceKey = TEXT("Valid.as");
		PastEnd.ByteOffset = static_cast<asUINT>(ValidText.Len()) + 1;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::InvalidPosition),
			static_cast<int32>(Session.Complete(PastEnd, {}).Status)));

		AS_NAMESPACE_QUALIFIER asSQueryPosition EmptyKey;
		EmptyKey.ByteOffset = 0;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::InvalidInput),
			static_cast<int32>(Session.Complete(EmptyKey, {}).Status)));

		AS_NAMESPACE_QUALIFIER asCToolingSession UtfSession(
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Utf.as"), TEXT("π")}}));
		AS_NAMESPACE_QUALIFIER asSQueryPosition MidCodepoint;
		MidCodepoint.LogicalSourceKey = TEXT("Utf.as");
		MidCodepoint.ByteOffset = 1;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::InvalidPosition),
			static_cast<int32>(UtfSession.Complete(MidCodepoint, {}).Status)));

		AS_NAMESPACE_QUALIFIER asSQueryPosition FileEnd;
		FileEnd.LogicalSourceKey = TEXT("Valid.as");
		FileEnd.ByteOffset = static_cast<asUINT>(ValidText.Len());
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(Session.Complete(FileEnd, {}).Status)));

		Session.DebugFailNextDeclarationStage();
		AS_NAMESPACE_QUALIFIER asSQueryPosition Inside;
		Inside.LogicalSourceKey = TEXT("Valid.as");
		Inside.ByteOffset = 0;
		const AS_NAMESPACE_QUALIFIER asSCompletionResult Unavailable = Session.Complete(Inside, {});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::AnalysisUnavailable),
			static_cast<int32>(Unavailable.Status)));
		ASSERT_THAT(AreEqual(0, Unavailable.Items.Num()));
	}

	TEST_METHOD(CancellationAfterWorkStarts)
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Inputs =
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Work.as"), TEXT("void F(){ int Ready=1; | }")}});
		FString Source;
		asUINT Offset = 0;
		ASSERT_THAT(IsTrue(StripMarker(TEXT("void F(){ int Ready=1; | }"), Source, Offset)));
		Inputs = AngelscriptNativeToolingTest::MakeInputs({{TEXT("Work.as"), Source}});
		AS_NAMESPACE_QUALIFIER asCToolingSession Session(Inputs);
		bool bCancelled = false;
		AS_NAMESPACE_QUALIFIER asSCancellationToken Token;
		Token.Cancelled = &bCancelled;
		AS_NAMESPACE_QUALIFIER asSQueryPosition Position;
		Position.LogicalSourceKey = TEXT("Work.as");
		Position.ByteOffset = Offset;
		Session.DebugCancelAt(
			AS_NAMESPACE_QUALIFIER asCToolingSession::asEToolingCancelCheckpoint::AfterDeclarations,
			&bCancelled);
		const AS_NAMESPACE_QUALIFIER asSCompletionResult Cancelled = Session.Complete(Position, Token);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Cancelled),
			static_cast<int32>(Cancelled.Status)));
		ASSERT_THAT(AreEqual(0, Cancelled.Items.Num()));

		bCancelled = false;
		Session.DebugCancelAt(
			AS_NAMESPACE_QUALIFIER asCToolingSession::asEToolingCancelCheckpoint::DuringCursorParse,
			&bCancelled);
		const AS_NAMESPACE_QUALIFIER asSSignatureHelpResult CancelledHelp = Session.GetSignatureHelp(Position, Token);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Cancelled),
			static_cast<int32>(CancelledHelp.Status)));
		ASSERT_THAT(AreEqual(0, CancelledHelp.Signatures.Num()));

		const AS_NAMESPACE_QUALIFIER asSCompletionResult Recovered = Session.Complete(Position, {});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(Recovered.Status)));
		ASSERT_THAT(IsTrue(HasItem(Recovered, TEXT("Ready"))));
	}

	TEST_METHOD(OneActualDeclarationPreparation)
	{
		FString CompleteSource;
		asUINT CompleteOffset = 0;
		ASSERT_THAT(IsTrue(StripMarker(
			TEXT("void F(){ int Ready=1; | }"),
			CompleteSource,
			CompleteOffset)));
		AS_NAMESPACE_QUALIFIER asCToolingSession CompleteSession(
			AngelscriptNativeToolingTest::MakeInputs({
				{TEXT("A.as"), CompleteSource},
				{TEXT("B.as"), TEXT("void G(){ int Other=2; }")},
			}));
		AS_NAMESPACE_QUALIFIER asSQueryPosition CompletePos;
		CompletePos.LogicalSourceKey = TEXT("A.as");
		CompletePos.ByteOffset = CompleteOffset;
		AS_NAMESPACE_QUALIFIER asCBuilder::DebugResetDeclarationsResolvedEntries();
		const AS_NAMESPACE_QUALIFIER asSCompletionResult Completed = CompleteSession.Complete(CompletePos, {});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(Completed.Status)));
		ASSERT_THAT(IsTrue(HasItem(Completed, TEXT("Ready"))));
		ASSERT_THAT(AreEqual(1, AS_NAMESPACE_QUALIFIER asCBuilder::DebugGetDeclarationsResolvedEntries()));

		AS_NAMESPACE_QUALIFIER asCBuilder::DebugResetDeclarationsResolvedEntries();
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(CompleteSession.Complete(CompletePos, {}).Status)));
		ASSERT_THAT(AreEqual(1, AS_NAMESPACE_QUALIFIER asCBuilder::DebugGetDeclarationsResolvedEntries()));

		FString HelpSource;
		asUINT HelpOffset = 0;
		ASSERT_THAT(IsTrue(StripMarker(
			TEXT("void Pick(int A); void F(){ Pick(|); }"),
			HelpSource,
			HelpOffset)));
		AS_NAMESPACE_QUALIFIER asCToolingSession HelpSession(
			AngelscriptNativeToolingTest::MakeInputs({
				{TEXT("A.as"), HelpSource},
				{TEXT("B.as"), TEXT("void G(){}")},
			}));
		AS_NAMESPACE_QUALIFIER asSQueryPosition HelpPos;
		HelpPos.LogicalSourceKey = TEXT("A.as");
		HelpPos.ByteOffset = HelpOffset;
		AS_NAMESPACE_QUALIFIER asCBuilder::DebugResetDeclarationsResolvedEntries();
		const AS_NAMESPACE_QUALIFIER asSSignatureHelpResult Help = HelpSession.GetSignatureHelp(HelpPos, {});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(Help.Status)));
		ASSERT_THAT(AreEqual(TEXT("Pick"), Help.CallName));
		ASSERT_THAT(AreEqual(1, AS_NAMESPACE_QUALIFIER asCBuilder::DebugGetDeclarationsResolvedEntries()));

		AS_NAMESPACE_QUALIFIER asCBuilder::DebugResetDeclarationsResolvedEntries();
		const AS_NAMESPACE_QUALIFIER asSCursorContext Cursor = CompleteSession.PrepareCursor(CompletePos, {});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(Cursor.Status)));
		ASSERT_THAT(AreEqual(1, AS_NAMESPACE_QUALIFIER asCBuilder::DebugGetDeclarationsResolvedEntries()));
	}
};

#endif
