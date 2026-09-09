#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Binds/Bind_FText.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/StringTableRegistry.h"
#include "as_scriptengine.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingTextFixture
{
struct FTextFixture
{
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TUniquePtr<FAngelscriptEngine> Owner;
	FString Diagnostic;

	bool Record()
	{
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return false;
		const auto& Collection = FAngelscriptBind::GetRegisteredCollection();
		if (!FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(Collection, *Store, Diagnostic)) return false;
		Store->SetSource({TEXT("FixtureModule"), TEXT("TextDependencies"), TEXT("Bindings/RuntimeBindingTextTests.cpp"), 25});
		FAngelscriptBinds DependencyBinds(*Store); FBindFlags DependencyFlags;
		if (Store->FindType(TEXT("FDateTime")).Index == INDEX_NONE)
			DependencyBinds.ValueClassForTarget<FDateTime>("FDateTime", DependencyFlags);
		if (Store->FindType(TEXT("FTimespan")).Index == INDEX_NONE)
			DependencyBinds.ValueClassForTarget<FTimespan>("FTimespan", DependencyFlags);
		const FName Providers[] = {
			TEXT("FText.Infrastructure"), TEXT("FText.Functions"), TEXT("FFormatArgumentValue.Functions"),
			TEXT("FNumberFormattingOptions.Manual"), TEXT("FStringTableRegistry.Manual")};
		if (!FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(Collection, Providers, *Store, Diagnostic)) return false;
		if (!Store->Seal(Diagnostic)) return false;
		FAngelscriptBindingDiagnostic BindingDiagnostic; Owner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic);
		if (!Owner) Diagnostic = BindingDiagnostic.Message; return Owner.IsValid();
	}

	int32 CountProvider(const FName Provider) const
	{
		int32 Count = 0;
		for (const auto& Type : Store->GetTypes())
			for (const auto& Member : Type.Members) if (Member.Source.Provider == Provider) ++Count;
		return Count;
	}
};

struct FScopedCulture
{
	FString Previous;
	FScopedCulture() : Previous(FInternationalization::Get().GetCurrentCulture()->GetName())
	{
		FInternationalization::Get().SetCurrentCulture(TEXT("en"));
	}
	~FScopedCulture() { FInternationalization::Get().SetCurrentCulture(Previous); }
};
}

using namespace RuntimeBindingTextFixture;
TEST_CLASS_WITH_FLAGS(Text, "Angelscript.UnitTest.RuntimeBindings.Values.Text",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CompleteTextFormattingFamilyIsRecorded)
	{
		FTextFixture F; const bool bRecorded = F.Record(); ASSERT_THAT(IsTrue(bRecorded, *F.Diagnostic));
		ASSERT_THAT(AreEqual(0, F.CountProvider(TEXT("FText.Infrastructure"))));
		ASSERT_THAT(AreEqual(41, F.CountProvider(TEXT("FText.Functions"))));
		ASSERT_THAT(AreEqual(9, F.CountProvider(TEXT("FFormatArgumentValue.Functions"))));
		ASSERT_THAT(AreEqual(12, F.CountProvider(TEXT("FNumberFormattingOptions.Manual"))));
		ASSERT_THAT(AreEqual(6, F.CountProvider(TEXT("FStringTableRegistry.Manual"))));
	}

	TEST_METHOD(OrderedFormatSubstitutesSeven)
	{
		FScopedCulture Culture; TArray<FFormatArgumentValue> Arguments; Arguments.Emplace(int32(7));
		const FText Result = FAngelscriptFTextBinds::OrderedTextFormat(FText::AsCultureInvariant(TEXT("{0}")), Arguments);
		ASSERT_THAT(IsTrue(Result.ToString() == TEXT("7")));
	}

	TEST_METHOD(NamedFormatSubstitutesSeven)
	{
		FScopedCulture Culture; TMap<FString, FFormatArgumentValue> Arguments; Arguments.Add(TEXT("Count"), int32(7));
		const FText Result = FAngelscriptFTextBinds::NamedTextFormat(FText::AsCultureInvariant(TEXT("{Count}")), Arguments);
		ASSERT_THAT(IsTrue(Result.ToString() == TEXT("7")));
	}

	TEST_METHOD(TextAndStringConversionPreservesHello)
	{
		const FString Source(TEXT("hello")); const FText Text = FText::FromString(Source); FString Result;
		FAngelscriptFTextBinds::AppendToString(&const_cast<FText&>(Text), Result);
		ASSERT_THAT(IsTrue(Text.ToString() == Source)); ASSERT_THAT(IsTrue(Result == Source));
	}

	TEST_METHOD(NumberFormattingOptionsSurviveCopy)
	{
		FNumberFormattingOptions Original; Original.SetAlwaysSign(true).SetUseGrouping(false)
			.SetMinimumIntegralDigits(3).SetMaximumFractionalDigits(2);
		const FNumberFormattingOptions Copy(Original);
		ASSERT_THAT(IsTrue(Original.IsIdentical(Copy)));
		ASSERT_THAT(AreEqual(GetTypeHash(Original), GetTypeHash(Copy)));
	}

	TEST_METHOD(InvalidFormatArgumentReportsExplicitTypeDiagnostic)
	{
		FFormatOrderedArguments Arguments; int32 Value = 7; FString Diagnostic;
		ASSERT_THAT(IsFalse(FAngelscriptFTextBinds::TryAddFormatArgument(Arguments, &Value, asTYPEID_VOID, Diagnostic)));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("Invalid argument type passed to FText::Format"))));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("type id")))); ASSERT_THAT(AreEqual(0, Arguments.Num()));
	}

	TEST_METHOD(MissingStringTableEntryIsEmptyWithoutCreatingGlobalFixture)
	{
		const FName TableId(TEXT("RuntimeBindingText_Missing_Table_8F157D"));
		TestRunner->AddExpectedError(TEXT("Failed to find string table entry"), EAutomationExpectedErrorFlags::Contains, 1);
		ASSERT_THAT(IsFalse(FStringTableRegistry::Get().FindStringTable(TableId).IsValid()));
		const FText Result = FText::FromStringTable(TableId, TEXT("Missing"), EStringTableLoadingPolicy::Find);
		ASSERT_THAT(IsFalse(Result.IsEmpty()));
		ASSERT_THAT(IsFalse(FStringTableRegistry::Get().FindStringTable(TableId).IsValid()));
	}
};
#endif
