#include "CQTest.h"
#include "NativeEngine/NativeEngineTestSupport.h"

#ifndef WITH_ANGELSCRIPT_UNITTESTS
#error WITH_ANGELSCRIPT_UNITTESTS must be defined by AngelscriptRuntime.Build.cs.
#endif

#ifndef WITH_ANGELSCRIPT_TESTS
#error WITH_ANGELSCRIPT_TESTS must be defined by AngelscriptRuntime.Build.cs.
#endif

static_assert(WITH_ANGELSCRIPT_TESTS == 1, "Replacement NativeEngine tests require the replacement test gate.");
static_assert(WITH_ANGELSCRIPT_UNITTESTS == 0, "Replacement NativeEngine tests must not enable the legacy test gate.");

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	Foundation,
	"Angelscript.UnitTest.NativeEngine.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ReplacementGateProvidesCQTest)
	{
		ASSERT_THAT(IsTrue(
			WITH_ANGELSCRIPT_TESTS == 1,
			TEXT("The replacement test gate should own NativeEngine CQTest registration.")));
	}

	TEST_METHOD(LegacyGateRemainsDisabled)
	{
		ASSERT_THAT(IsTrue(
			WITH_ANGELSCRIPT_UNITTESTS == 0,
			TEXT("Using CQTest must not reactivate the legacy AngelScript test framework.")));
	}

	TEST_METHOD(SourceInputOwnsUtf8Bytes)
	{
		FString MutableSource = TEXT("class C {}");
		const AngelscriptNativeEngineTest::FSourceInput Input =
			AngelscriptNativeEngineTest::FSourceInput::FromText(TEXT("/Game/Foundation.as"), MutableSource);
		MutableSource[0] = TEXT('X');

		const ANSICHAR Expected[] = "class C {}";
		const TConstArrayView<uint8> Bytes = Input.GetUtf8Bytes();
		ASSERT_THAT(AreEqual(FString(TEXT("/Game/Foundation.as")), Input.GetLogicalPath()));
		ASSERT_THAT(AreEqual(static_cast<int32>(sizeof(Expected) - 1), Bytes.Num()));
		ASSERT_THAT(IsTrue(
			FMemory::Memcmp(Bytes.GetData(), Expected, Bytes.Num()) == 0,
			TEXT("The fixture should own the original UTF-8 bytes instead of borrowing mutable text.")));
	}

	TEST_METHOD(DiagnosticCapturePreservesOrderAndResets)
	{
		AngelscriptNativeEngineTest::FDiagnosticCapture Capture;
		Capture.Add(AngelscriptNativeEngineTest::EDiagnosticSeverity::Warning, 7, TEXT("first"));
		Capture.Add(AngelscriptNativeEngineTest::EDiagnosticSeverity::Error, 19, TEXT("second"));

		const TConstArrayView<AngelscriptNativeEngineTest::FDiagnosticObservation> Observations =
			Capture.GetObservations();
		ASSERT_THAT(AreEqual(2, Observations.Num()));
		ASSERT_THAT(AreEqual(AngelscriptNativeEngineTest::EDiagnosticSeverity::Warning, Observations[0].Severity));
		ASSERT_THAT(AreEqual(7, Observations[0].ByteOffset));
		ASSERT_THAT(AreEqual(FString(TEXT("first")), Observations[0].Message));
		ASSERT_THAT(AreEqual(AngelscriptNativeEngineTest::EDiagnosticSeverity::Error, Observations[1].Severity));
		ASSERT_THAT(AreEqual(19, Observations[1].ByteOffset));
		ASSERT_THAT(AreEqual(FString(TEXT("second")), Observations[1].Message));

		Capture.Reset();
		ASSERT_THAT(AreEqual(0, Capture.GetObservations().Num()));
	}

};

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
