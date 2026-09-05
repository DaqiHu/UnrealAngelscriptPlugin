#include "CQTest.h"
#include "NativePreprocessorTestSupport.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

using AngelscriptNativePreprocessorTest::FPreprocessorRun;
using AngelscriptNativePreprocessorTest::MakeFlags;

namespace DirectiveRoutingTests
{
	TEST_CLASS_WITH_FLAGS(
		PreprocessorRouting,
		"Angelscript.UnitTest.NativeEngine",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(IfAndSingleNegationRouteActiveTokens)
		{
			const FPreprocessorRun Run(
				TEXT("#if FEATURE\nint Enabled;\n#else\nint Disabled;\n#endif\n"
					"#if !EXPERIMENTAL\nbool Stable;\n#endif\n"),
				MakeFlags({{TEXT("FEATURE"), true}, {TEXT("EXPERIMENTAL"), false}}));

			ASSERT_THAT(IsTrue(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(
				FString(TEXT("int|Enabled|;|bool|Stable|;")),
				Run.GetActiveTokenProjection()));
			ASSERT_THAT(AreEqual(0, Run.DiagnosticRecords.Num()));
		}

		TEST_METHOD(IfdefAndIfndefTestFlagPresence)
		{
			const FPreprocessorRun Run(
				TEXT("#ifdef PRESENT\nint Present;\n#endif\n"
					"#ifndef ABSENT\nint Absent;\n#endif\n"),
				MakeFlags({{TEXT("PRESENT"), false}}));

			ASSERT_THAT(IsTrue(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(
				FString(TEXT("int|Present|;|int|Absent|;")),
				Run.GetActiveTokenProjection()));
		}

		TEST_METHOD(ElifSelectsOnlyTheFirstEligibleBranch)
		{
			const FPreprocessorRun Run(
				TEXT("#if FIRST\nint First;\n#elif SECOND\nint Second;\n#elif THIRD\nint Third;\n#else\nint Fallback;\n#endif\n"),
				MakeFlags({
					{TEXT("FIRST"), false},
					{TEXT("SECOND"), true},
					{TEXT("THIRD"), true}}));

			ASSERT_THAT(IsTrue(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(FString(TEXT("int|Second|;")), Run.GetActiveTokenProjection()));
			ASSERT_THAT(AreEqual(4, Run.Result.DirectiveTree.GetBranches().Num()));
			ASSERT_THAT(IsTrue(Run.Result.DirectiveTree.GetBranches()[1].bTaken));
			ASSERT_THAT(AreEqual(
				AS_NAMESPACE_QUALIFIER asEConditionEvaluation::NotEvaluated,
				Run.Result.DirectiveTree.GetBranches()[2].Evaluation));
		}

		TEST_METHOD(NestedInactiveConditionDoesNotEvaluateUnknownFlag)
		{
			const FPreprocessorRun Run(
				TEXT("#if OUTER\n#if UNKNOWN\nint Never;\n#endif\n#else\nint Active;\n#endif\n"),
				MakeFlags({{TEXT("OUTER"), false}}));

			ASSERT_THAT(IsTrue(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(FString(TEXT("int|Active|;")), Run.GetActiveTokenProjection()));
			ASSERT_THAT(AreEqual(0, Run.DiagnosticRecords.Num()));
			ASSERT_THAT(IsTrue(Run.Result.DirectiveTree.GetBranches().ContainsByPredicate([](const AS_NAMESPACE_QUALIFIER asSConditionalBranch& Branch)
			{
				return Branch.Evaluation == AS_NAMESPACE_QUALIFIER asEConditionEvaluation::NotEvaluated;
			})));
		}

		TEST_METHOD(UnknownEvaluatedFlagFailsClosedWithStableDiagnostic)
		{
			const FPreprocessorRun Run(TEXT("#if UNKNOWN\nint Value;\n#endif\n"), {});

			ASSERT_THAT(IsFalse(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(0, Run.Result.ActiveTokens.Num()));
			ASSERT_THAT(AreEqual(1, Run.DiagnosticRecords.Num()));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEPreprocessorDiagnosticID::UnknownFlag),
				Run.DiagnosticRecords[0].DiagnosticID));
		}

		TEST_METHOD(ElifAfterElseFailsClosed)
		{
			const FPreprocessorRun Run(
				TEXT("#if FEATURE\nint A;\n#else\nint B;\n#elif FEATURE\nint C;\n#endif\n"),
				MakeFlags({{TEXT("FEATURE"), true}}));

			ASSERT_THAT(IsFalse(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(0, Run.Result.ActiveTokens.Num()));
			ASSERT_THAT(IsTrue(Run.DiagnosticRecords.ContainsByPredicate([](const AS_NAMESPACE_QUALIFIER asSDiagnosticRecord& Record)
			{
				return Record.DiagnosticID
					== static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEPreprocessorDiagnosticID::ElifAfterElse);
			})));
		}
	};
}

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
