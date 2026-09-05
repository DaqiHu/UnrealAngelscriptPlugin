#include "CQTest.h"
#include "NativePreprocessorTestSupport.h"

#include "frontend/as_directive_tree.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

using AngelscriptNativePreprocessorTest::FPreprocessorRun;
using AngelscriptNativePreprocessorTest::MakeFlags;

namespace DirectiveTreeTests
{
	TEST_CLASS_WITH_FLAGS(
		PreprocessorDirectiveTree,
		"Angelscript.UnitTest.NativeEngine",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(TreePreservesBranchesTerminatorAndExactRanges)
		{
			const FString Source = TEXT("#if FLAG\nint Active;\n#else\nint Inactive;\n#endif\n");
			const FPreprocessorRun Run(Source, MakeFlags({{TEXT("FLAG"), true}}));
			const AS_NAMESPACE_QUALIFIER asCDirectiveTree& Tree = Run.Result.DirectiveTree;

			ASSERT_THAT(IsTrue(Run.Result.IsValid()));
			ASSERT_THAT(IsTrue(Tree.IsStructurallyValid()));
			ASSERT_THAT(AreEqual(1, Tree.GetConditionals().Num()));
			ASSERT_THAT(AreEqual(2, Tree.GetBranches().Num()));
			ASSERT_THAT(AreEqual(3, Tree.GetDirectiveRecords().Num()));
			const AS_NAMESPACE_QUALIFIER asSConditionalDirective& Conditional = Tree.GetConditionals()[0];
			ASSERT_THAT(IsTrue(Conditional.bTerminated));
			ASSERT_THAT(AreEqual(0u, Conditional.TakenBranchIndex));
			ASSERT_THAT(AreEqual(2, Conditional.BranchIndices.Num()));
			ASSERT_THAT(AreEqual(0u, Conditional.FullRange.Begin.ByteOffset));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(Source.Find(TEXT("#endif")) + 6),
				Conditional.FullRange.End.ByteOffset));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(Source.Find(TEXT("\n"))),
				Tree.GetBranches()[0].BodyRange.Begin.ByteOffset));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(Source.Find(TEXT("#else"))),
				Tree.GetBranches()[0].BodyRange.End.ByteOffset));
		}

		TEST_METHOD(NoTakenBranchPreservesNormalizedSkippedRanges)
		{
			const FPreprocessorRun Run(
				TEXT("#if FIRST\nint First;\n#elif SECOND\nint Second;\n#endif\n"),
				MakeFlags({{TEXT("FIRST"), false}, {TEXT("SECOND"), false}}));
			const AS_NAMESPACE_QUALIFIER asCDirectiveTree& Tree = Run.Result.DirectiveTree;

			ASSERT_THAT(IsTrue(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asSConditionalBranch::InvalidBranchIndex, Tree.GetConditionals()[0].TakenBranchIndex));
			ASSERT_THAT(AreEqual(2, Tree.GetSkippedRanges().Num()));
			ASSERT_THAT(IsTrue(
				Tree.GetSkippedRanges()[0].Range.End.ByteOffset
				<= Tree.GetSkippedRanges()[1].Range.Begin.ByteOffset));
		}

		TEST_METHOD(NestedInactiveTreeRetainsNotEvaluatedChild)
		{
			const FPreprocessorRun Run(
				TEXT("#if OUTER\n#if UNKNOWN\nint Never;\n#endif\n#else\nint Active;\n#endif\n"),
				MakeFlags({{TEXT("OUTER"), false}}));
			const AS_NAMESPACE_QUALIFIER asCDirectiveTree& Tree = Run.Result.DirectiveTree;

			ASSERT_THAT(IsTrue(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(2, Tree.GetConditionals().Num()));
			ASSERT_THAT(AreEqual(0u, Tree.GetConditionals()[1].ParentConditionalIndex));
			ASSERT_THAT(AreEqual(
				AS_NAMESPACE_QUALIFIER asEConditionEvaluation::NotEvaluated,
				Tree.GetBranches()[1].Evaluation));
			ASSERT_THAT(AreEqual(1, Tree.GetSkippedRanges().Num()));
			ASSERT_THAT(AreEqual(0u, Tree.GetSkippedRanges()[0].BranchIndex));
		}

		TEST_METHOD(HashInsideStringOrCommentDoesNotCreateDirective)
		{
			const FPreprocessorRun Run(
				TEXT("string Text = \"#if FAKE\";\n// #endif\n#if REAL\nint Value;\n#endif\n"),
				MakeFlags({{TEXT("REAL"), true}}));

			ASSERT_THAT(IsTrue(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(1, Run.Result.DirectiveTree.GetConditionals().Num()));
			ASSERT_THAT(AreEqual(2, Run.Result.DirectiveTree.GetDirectiveRecords().Num()));
		}

		TEST_METHOD(MissingTerminatorRetainsInspectableRecoveryTree)
		{
			const FString Source = TEXT("#if FLAG\nint Value;\n");
			const FPreprocessorRun Run(Source, MakeFlags({{TEXT("FLAG"), true}}));
			const AS_NAMESPACE_QUALIFIER asCDirectiveTree& Tree = Run.Result.DirectiveTree;

			ASSERT_THAT(IsFalse(Run.Result.IsValid()));
			ASSERT_THAT(IsFalse(Tree.IsStructurallyValid()));
			ASSERT_THAT(AreEqual(1, Tree.GetConditionals().Num()));
			ASSERT_THAT(IsFalse(Tree.GetConditionals()[0].bTerminated));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(Source.Len()),
				Tree.GetBranches()[0].BodyRange.End.ByteOffset));
			ASSERT_THAT(IsTrue(Run.DiagnosticRecords.ContainsByPredicate([](const AS_NAMESPACE_QUALIFIER asSDiagnosticRecord& Record)
			{
				return Record.DiagnosticID == static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEPreprocessorDiagnosticID::MissingEndif);
			})));
		}

		TEST_METHOD(DuplicateElseRetainsInvalidRecoveryTree)
		{
			const FPreprocessorRun Run(
				TEXT("#if FLAG\nint A;\n#else\nint B;\n#else\nint C;\n#endif\n"),
				MakeFlags({{TEXT("FLAG"), false}}));

			ASSERT_THAT(IsFalse(Run.Result.IsValid()));
			ASSERT_THAT(IsFalse(Run.Result.DirectiveTree.IsStructurallyValid()));
			ASSERT_THAT(AreEqual(3, Run.Result.DirectiveTree.GetBranches().Num()));
			ASSERT_THAT(IsTrue(Run.DiagnosticRecords.ContainsByPredicate([](const AS_NAMESPACE_QUALIFIER asSDiagnosticRecord& Record)
			{
				return Record.DiagnosticID == static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEPreprocessorDiagnosticID::DuplicateElse);
			})));
		}
	};
}

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
