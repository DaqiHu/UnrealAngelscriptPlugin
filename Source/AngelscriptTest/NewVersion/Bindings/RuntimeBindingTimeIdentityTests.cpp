#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingTimeIdentityFixture
{
struct FTimeIdentityFixture
{
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TUniquePtr<FAngelscriptEngine> FirstOwner;
	TUniquePtr<FAngelscriptEngine> SecondOwner;
	FString Diagnostic;

	bool Record()
	{
		Diagnostic = TEXT("finalizing providers");
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return false;
		const auto& Collection = FAngelscriptBind::GetRegisteredCollection();
		Diagnostic = TEXT("recording installable declarations");
		if (!FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(Collection, *Store, Diagnostic)) return false;
		const FName Providers[] = {
			TEXT("FDateTime.ToStringContribution"), TEXT("FDateTime.Functions"), TEXT("FTimespan.Functions"),
			TEXT("FGuid"), TEXT("FFrameTime"), TEXT("FRange"),
			TEXT("FRandomStream.Functions"), TEXT("FRandomStream.ToStringContribution"),
			TEXT("FMath.Manual"), TEXT("Hash")};
		for (const FName Provider : Providers)
		{
			Diagnostic = FString::Printf(TEXT("recording provider %s"), *Provider.ToString());
			if (!FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(Collection, MakeArrayView(&Provider, 1), *Store, Diagnostic))
			{
				Diagnostic = FString::Printf(TEXT("Provider %s failed: %s"), *Provider.ToString(), *Diagnostic);
				UE_LOG(LogTemp, Display, TEXT("TimeIdentity fixture failed: %s"), *Diagnostic);
				return false;
			}
		}
		Diagnostic = TEXT("sealing time identity store");
		if (!Store->Seal(Diagnostic)) { UE_LOG(LogTemp, Display, TEXT("TimeIdentity fixture failed: %s"), *Diagnostic); return false; }
		FAngelscriptBindingDiagnostic BindingDiagnostic;
		Diagnostic = TEXT("creating first time identity owner");
		FirstOwner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic);
		if (!FirstOwner) { if (!BindingDiagnostic.Message.IsEmpty()) Diagnostic = BindingDiagnostic.Message; UE_LOG(LogTemp, Display, TEXT("TimeIdentity fixture failed: %s"), *Diagnostic); return false; }
		Diagnostic = TEXT("creating second time identity owner");
		SecondOwner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic);
		if (!SecondOwner) { if (!BindingDiagnostic.Message.IsEmpty()) Diagnostic = BindingDiagnostic.Message; UE_LOG(LogTemp, Display, TEXT("TimeIdentity fixture failed: %s"), *Diagnostic); return false; }
		return true;
	}

	int32 CountProvider(const FName Provider) const
	{
		int32 Count = 0;
		for (const auto& Type : Store->GetTypes())
			for (const auto& Member : Type.Members) if (Member.Source.Provider == Provider) ++Count;
		return Count;
	}
};
}

using namespace RuntimeBindingTimeIdentityFixture;
TEST_CLASS_WITH_FLAGS(TimeIdentity, "Angelscript.UnitTest.RuntimeBindings.Values.TimeIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(AllFamilyRecordsAreAccounted)
	{
		FTimeIdentityFixture F; ASSERT_THAT(IsTrue(F.Record(), *F.Diagnostic));
		ASSERT_THAT(IsTrue(F.Store->FindType(TEXT("EGuidFormats")).Index != INDEX_NONE));
		ASSERT_THAT(IsTrue(F.Store->FindType(TEXT("FRandomStream")).Index != INDEX_NONE));
		const TPair<FName, int32> Expected[] = {{TEXT("FDateTime.Functions"), 38}, {TEXT("FTimespan.Functions"), 46},
			{TEXT("FGuid"), 14}, {TEXT("FFrameTime"), 9}, {TEXT("FRange"), 122},
			{TEXT("FRandomStream.Functions"), 18}, {TEXT("FMath.Manual"), 251}, {TEXT("Hash"), 8}};
		for (const auto& Pair : Expected)
			ASSERT_THAT(AreEqual(Pair.Value, F.CountProvider(Pair.Key), *Pair.Key.ToString()));
	}

	TEST_METHOD(OneSecondTimespanUsesExactTicks)
	{
		const FTimespan Value = FTimespan::FromSeconds(1.0);
		ASSERT_THAT(AreEqual(ETimespan::TicksPerSecond, Value.GetTicks()));
	}

	TEST_METHOD(DateComponentsRoundTrip)
	{
		const FDateTime Value(2024, 2, 29, 12, 34, 56, 789);
		ASSERT_THAT(AreEqual(2024, Value.GetYear())); ASSERT_THAT(AreEqual(2, Value.GetMonth()));
		ASSERT_THAT(AreEqual(29, Value.GetDay())); ASSERT_THAT(AreEqual(12, Value.GetHour()));
		ASSERT_THAT(AreEqual(34, Value.GetMinute())); ASSERT_THAT(AreEqual(56, Value.GetSecond()));
		ASSERT_THAT(AreEqual(789, Value.GetMillisecond()));
	}

	TEST_METHOD(GuidValidityAndFixedParseRoundTrip)
	{
		ASSERT_THAT(IsFalse(FGuid().IsValid()));
		const FString Source(TEXT("00112233-4455-6677-8899-AABBCCDDEEFF")); FGuid Parsed;
		ASSERT_THAT(IsTrue(FGuid::Parse(Source, Parsed)));
		ASSERT_THAT(IsTrue(Parsed.IsValid()));
		ASSERT_THAT(IsTrue(Parsed.ToString(EGuidFormats::DigitsWithHyphens).Equals(Source, ESearchCase::IgnoreCase)));
	}

	TEST_METHOD(RangeIncludesLowerAndExcludesUpper)
	{
		const FInt32Range Value(FInt32RangeBound::Inclusive(3), FInt32RangeBound::Exclusive(7));
		ASSERT_THAT(IsTrue(Value.Contains(3))); ASSERT_THAT(IsTrue(Value.Contains(6)));
		ASSERT_THAT(IsFalse(Value.Contains(7)));
	}

	TEST_METHOD(FrameTimePreservesFrameAndSubframe)
	{
		const FFrameTime Value(FFrameNumber(12), 0.25f);
		ASSERT_THAT(AreEqual(12, Value.GetFrame().Value));
		ASSERT_THAT(IsTrue(Value.GetSubFrame() == 0.25f));
		ASSERT_THAT(IsTrue(Value.AsDecimal() == 12.25));
	}

	TEST_METHOD(RandomStreamsRepeatAcrossOwnersAndReset)
	{
		FTimeIdentityFixture F; ASSERT_THAT(IsTrue(F.Record(), *F.Diagnostic));
		FRandomStream First(1337); FRandomStream Second(1337);
		const uint32 A0 = First.GetUnsignedInt(); const uint32 A1 = First.GetUnsignedInt();
		ASSERT_THAT(AreEqual(A0, Second.GetUnsignedInt())); ASSERT_THAT(AreEqual(A1, Second.GetUnsignedInt()));
		First.Reset(); ASSERT_THAT(AreEqual(A0, First.GetUnsignedInt())); ASSERT_THAT(AreEqual(A1, First.GetUnsignedInt()));
	}

	TEST_METHOD(MathClampAndEqualValueHashAreStable)
	{
		ASSERT_THAT(AreEqual(3, FMath::Clamp(5, 0, 3)));
		const FGuid Left(1, 2, 3, 4); const FGuid Right(1, 2, 3, 4);
		ASSERT_THAT(AreEqual(GetTypeHash(Left), GetTypeHash(Right)));
	}
};
#endif
