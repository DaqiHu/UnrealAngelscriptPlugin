#include "CQTest.h"
#include "NativePreprocessorTestSupport.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

using AngelscriptNativePreprocessorTest::FPreprocessorRun;
using AngelscriptNativePreprocessorTest::MakeFlags;

namespace PreprocessorPolicyTests
{
	TEST_CLASS_WITH_FLAGS(
		PreprocessorPolicy,
		"Angelscript.UnitTest.NativeEngine.Lexer",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(ValidUsageRestrictionsBecomeTypedRecords)
		{
			const FString Source =
				TEXT("#restrict usage allow Gameplay\n"
					"#restrict usage disallow EditorOnly\n"
					"int Value;\n");
			const FPreprocessorRun Run(Source, {});
			const TConstArrayView<AS_NAMESPACE_QUALIFIER asSRestrictionDirective> Restrictions =
				Run.Result.DirectiveTree.GetRestrictions();

			ASSERT_THAT(IsTrue(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(2, Restrictions.Num()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asERestrictionPolicy::Allow, Restrictions[0].Policy));
			ASSERT_THAT(AreEqual(FString(TEXT("Gameplay")), Restrictions[0].Pattern));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asERestrictionPolicy::Disallow, Restrictions[1].Policy));
			ASSERT_THAT(AreEqual(FString(TEXT("EditorOnly")), Restrictions[1].Pattern));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(Source.Find(TEXT("Gameplay"))),
				Restrictions[0].PatternRange.Begin.ByteOffset));
			ASSERT_THAT(AreEqual(FString(TEXT("int|Value|;")), Run.GetActiveTokenProjection()));
		}

		TEST_METHOD(ActiveNestedRestrictionRetainsBranchOwnership)
		{
			const FPreprocessorRun Run(
				TEXT("#if FEATURE\n#restrict usage allow Runtime\n#endif\n"),
				MakeFlags({{TEXT("FEATURE"), true}}));
			const TConstArrayView<AS_NAMESPACE_QUALIFIER asSRestrictionDirective> Restrictions =
				Run.Result.DirectiveTree.GetRestrictions();

			ASSERT_THAT(IsTrue(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(1, Restrictions.Num()));
			ASSERT_THAT(AreEqual(0u, Restrictions[0].BranchIndex));
			ASSERT_THAT(IsTrue(Restrictions[0].bActive));
		}

		TEST_METHOD(InactiveRestrictionIsPreservedButNotPublishedAsPolicy)
		{
			const FPreprocessorRun Run(
				TEXT("#if FEATURE\n#restrict nonsense maybe too many tokens\n#endif\n"),
				MakeFlags({{TEXT("FEATURE"), false}}));

			ASSERT_THAT(IsTrue(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(0, Run.Result.DirectiveTree.GetRestrictions().Num()));
			ASSERT_THAT(AreEqual(0, Run.DiagnosticRecords.Num()));
			ASSERT_THAT(AreEqual(3, Run.Result.DirectiveTree.GetDirectiveRecords().Num()));
		}

		TEST_METHOD(UnknownRestrictionOperationFailsAtOperation)
		{
			const FString Source = TEXT("#restrict access allow Gameplay\n");
			const FPreprocessorRun Run(Source, {});

			ASSERT_THAT(IsFalse(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(1, Run.DiagnosticRecords.Num()));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEPreprocessorDiagnosticID::InvalidRestrictionOperation),
				Run.DiagnosticRecords[0].DiagnosticID));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(Source.Find(TEXT("access"))),
				Run.DiagnosticRecords[0].PrimaryRange.Begin.ByteOffset));
		}

		TEST_METHOD(MissingPolicyAndExtraArgumentFailClosed)
		{
			const FPreprocessorRun MissingPolicy(TEXT("#restrict usage Gameplay\n"), {});
			ASSERT_THAT(IsFalse(MissingPolicy.Result.IsValid()));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEPreprocessorDiagnosticID::InvalidRestrictionPolicy),
				MissingPolicy.DiagnosticRecords[0].DiagnosticID));

			const FPreprocessorRun ExtraArgument(TEXT("#restrict usage allow Gameplay Extra\n"), {});
			ASSERT_THAT(IsFalse(ExtraArgument.Result.IsValid()));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEPreprocessorDiagnosticID::ExtraRestrictionArgument),
				ExtraArgument.DiagnosticRecords[0].DiagnosticID));
		}

		TEST_METHOD(ActiveIncludeIsRejectedWithoutImportGuidance)
		{
			const FPreprocessorRun Run(TEXT("#include \"Other.as\"\nint Value;\n"), {});

			ASSERT_THAT(IsFalse(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(0, Run.Result.ActiveTokens.Num()));
			ASSERT_THAT(AreEqual(1, Run.DiagnosticRecords.Num()));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEPreprocessorDiagnosticID::UnsupportedInclude),
				Run.DiagnosticRecords[0].DiagnosticID));
			for (const AS_NAMESPACE_QUALIFIER asSDiagnosticArgument& Argument : Run.DiagnosticRecords[0].Arguments)
			{
				ASSERT_THAT(IsFalse(Argument.ToDisplayString().Contains(TEXT("import"), ESearchCase::IgnoreCase)));
			}
			ASSERT_THAT(AreEqual(0, Run.Result.DirectiveTree.GetRestrictions().Num()));
		}
	};
}

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
