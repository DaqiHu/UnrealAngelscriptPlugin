#include "CQTest.h"
#include "NativePreprocessorTestSupport.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace ConditionExpressionTests
{
	using AngelscriptNativePreprocessorTest::FPreprocessorRun;
	using AngelscriptNativePreprocessorTest::MakeFlags;

	TEST_CLASS_WITH_FLAGS(PreprocessorConditions, "Angelscript.UnitTest.NativeEngine",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(BooleanConstantsSelectOnlyEligibleBranch)
		{
			const FPreprocessorRun Run(TEXT("#if 0\nint Never;\n#elif 1\nint Selected;\n"
				"#elif UNKNOWN\nint AlsoNever;\n#else\nint Fallback;\n#endif\n"), {});
			ASSERT_THAT(IsTrue(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(FString(TEXT("int|Selected|;")), Run.GetActiveTokenProjection()));
			ASSERT_THAT(AreEqual(0, Run.DiagnosticRecords.Num()));
		}

		TEST_METHOD(DefinedTestsPresenceNotFlagValue)
		{
			const FPreprocessorRun Run(TEXT("#if defined(PRESENT) && defined PRESENT && !defined(ABSENT)\n"
				"int Exists;\n#endif\n#if PRESENT\nint Disabled;\n#endif\n"),
				MakeFlags({{TEXT("PRESENT"), false}}));
			ASSERT_THAT(IsTrue(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(FString(TEXT("int|Exists|;")), Run.GetActiveTokenProjection()));
			ASSERT_THAT(AreEqual(0, Run.DiagnosticRecords.Num()));
		}

		TEST_METHOD(ConjunctionBindsTighterThanDisjunction)
		{
			const FPreprocessorRun Run(TEXT("#if 1 || 0 && 0\nint First;\n#endif\n"
				"#if 0 && 1 || 1\nint Second;\n#endif\n"
				"#if 0 || 1 && 0\nint Wrong;\n#endif\n"), {});
			ASSERT_THAT(IsTrue(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(FString(TEXT("int|First|;|int|Second|;")), Run.GetActiveTokenProjection()));
		}

		TEST_METHOD(ParenthesesAndUnaryNegationOverridePrecedence)
		{
			const FPreprocessorRun Run(TEXT("#if (1 || 0) && 0\nint Wrong;\n#endif\n"
				"#if !!!0 && !!(FEATURE || 0)\nint Selected;\n#endif\n"),
				MakeFlags({{TEXT("FEATURE"), true}}));
			ASSERT_THAT(IsTrue(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(FString(TEXT("int|Selected|;")), Run.GetActiveTokenProjection()));
		}

		TEST_METHOD(ShortCircuitSuppressesOnlyUnreachableLookups)
		{
			const FPreprocessorRun Run(TEXT("#if 0 && UNKNOWN\nint Wrong;\n#endif\n"
				"#if 1 || (UNKNOWN && ALSO_UNKNOWN)\nint Selected;\n#endif\n"
				"#if 0\n#if UNKNOWN || (OTHER && 1)\nint Nested;\n#endif\n#endif\n"), {});
			ASSERT_THAT(IsTrue(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(FString(TEXT("int|Selected|;")), Run.GetActiveTokenProjection()));
			ASSERT_THAT(AreEqual(0, Run.DiagnosticRecords.Num()));
		}

		TEST_METHOD(ReachableUnknownOperandReportsItsOriginalRange)
		{
			const FPreprocessorRun Run(TEXT("#if 0 || UNKNOWN\nint Wrong;\n#endif\n"), {});
			ASSERT_THAT(IsFalse(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(0, Run.Result.ActiveTokens.Num()));
			ASSERT_THAT(AreEqual(1, Run.DiagnosticRecords.Num()));
			const auto& Diagnostic = Run.DiagnosticRecords[0];
			ASSERT_THAT(AreEqual(static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEPreprocessorDiagnosticID::UnknownFlag), Diagnostic.DiagnosticID));
			ASSERT_THAT(AreEqual(asUINT(9), Diagnostic.PrimaryRange.Begin.ByteOffset));
			ASSERT_THAT(AreEqual(asUINT(16), Diagnostic.PrimaryRange.End.ByteOffset));
		}

		TEST_METHOD(MalformedInactiveOperandsStillFailStructurally)
		{
			const TCHAR* Sources[] = {
				TEXT("#if 0 && (\nint Wrong;\n#endif\n"),
				TEXT("#if 1 || defined()\nint Wrong;\n#endif\n"),
				TEXT("#if 0\n#if !\nint Wrong;\n#endif\n#endif\n"),
				TEXT("#if 1\nint First;\n#elif 0 &&\nint Wrong;\n#endif\n")};
			for (const TCHAR* Source : Sources)
			{
				const FPreprocessorRun Run(Source, {});
				ASSERT_THAT(IsFalse(Run.Result.IsValid(), Source));
				ASSERT_THAT(AreEqual(0, Run.Result.ActiveTokens.Num()));
				ASSERT_THAT(AreEqual(1, Run.DiagnosticRecords.Num()));
				ASSERT_THAT(AreEqual(static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEPreprocessorDiagnosticID::MalformedCondition), Run.DiagnosticRecords[0].DiagnosticID));
			}
		}

		TEST_METHOD(UnsupportedArithmeticAndMalformedConditionsNeverPublishTokens)
		{
			const TCHAR* Conditions[] = {TEXT("2"), TEXT("01"), TEXT("1.0"), TEXT("1 + 0"),
				TEXT("1 & 0"), TEXT("1 == 1"), TEXT("1 ? 1 : 0"), TEXT("(1"), TEXT("1)"),
				TEXT("()"), TEXT("defined(1)"), TEXT("defined A B"), TEXT("1 0"), TEXT("")};
			for (const TCHAR* Condition : Conditions)
			{
				const FPreprocessorRun Run(FString::Printf(TEXT("#if %s\nint Wrong;\n#endif\n"), Condition), {});
				ASSERT_THAT(IsFalse(Run.Result.IsValid(), Condition));
				ASSERT_THAT(AreEqual(0, Run.Result.ActiveTokens.Num()));
				ASSERT_THAT(AreEqual(1, Run.DiagnosticRecords.Num()));
				ASSERT_THAT(AreEqual(static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEPreprocessorDiagnosticID::MalformedCondition), Run.DiagnosticRecords[0].DiagnosticID));
			}
		}

		TEST_METHOD(UnexpectedOperatorDiagnosticIdentifiesTheOperator)
		{
			const FPreprocessorRun Run(TEXT("#if 1 + 0\nint Wrong;\n#endif\n"), {});
			ASSERT_THAT(IsFalse(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(1, Run.DiagnosticRecords.Num()));
			ASSERT_THAT(AreEqual(asUINT(6), Run.DiagnosticRecords[0].PrimaryRange.Begin.ByteOffset));
			ASSERT_THAT(AreEqual(asUINT(7), Run.DiagnosticRecords[0].PrimaryRange.End.ByteOffset));
		}

		TEST_METHOD(SelectedSourceIdentifiersAreNotExpanded)
		{
			const FPreprocessorRun Run(TEXT("#if defined(FEATURE) && FEATURE\n"
				"int FEATURE; string defined = \"FEATURE\";\n#endif\n"),
				MakeFlags({{TEXT("FEATURE"), true}}));
			ASSERT_THAT(IsTrue(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(FString(TEXT("int|FEATURE|;|string|defined|=|\"FEATURE\"|;")), Run.GetActiveTokenProjection()));
		}

		TEST_METHOD(ExcessiveParenthesisDepthFailsWithoutStackExhaustion)
		{
			FString Source(TEXT("#if "));
			for (int32 Index = 0; Index < 512; ++Index) { Source += TEXT("("); }
			Source += TEXT("1");
			for (int32 Index = 0; Index < 512; ++Index) { Source += TEXT(")"); }
			Source += TEXT("\nint Wrong;\n#endif\n");
			const FPreprocessorRun Run(Source, {});
			ASSERT_THAT(IsFalse(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(0, Run.Result.ActiveTokens.Num()));
			ASSERT_THAT(AreEqual(1, Run.DiagnosticRecords.Num()));
		}

		TEST_METHOD(LongFlatUnaryAndBooleanChainsDoNotUseRecursiveOperatorParsing)
		{
			FString Source(TEXT("#if "));
			for (int32 Index = 0; Index < 2048; ++Index) { Source += TEXT("!"); }
			Source += TEXT("1");
			for (int32 Index = 0; Index < 1024; ++Index) { Source += TEXT(" && 1"); }
			Source += TEXT("\nint Selected;\n#endif\n");
			const FPreprocessorRun Run(Source, {});
			ASSERT_THAT(IsTrue(Run.Result.IsValid()));
			ASSERT_THAT(AreEqual(FString(TEXT("int|Selected|;")), Run.GetActiveTokenProjection()));
		}
	};
}

#endif
