#include "CQTest.h"
#include "AngelscriptSource.h"
#include "unreal/AngelscriptSource.h"

#include "Containers/Utf8String.h"
#include "Templates/SharedPointer.h"

#include <type_traits>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	SourceInput,
	"Angelscript.UnitTest.NativeEngine.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FUtf8String MakeUtf8(const ANSICHAR* Bytes)
	{
		return FUtf8String(reinterpret_cast<const UTF8CHAR*>(Bytes));
	}

	static bool Utf8Equals(const FUtf8String& Text, const ANSICHAR* Expected)
	{
		const int32 ExpectedLen = FCStringAnsi::Strlen(Expected);
		return Text.Len() == ExpectedLen
			&& FMemory::Memcmp(*Text, Expected, ExpectedLen) == 0;
	}

public:
	TEST_METHOD(MoveRetainsBodyAddress)
	{
		FUtf8String Body = MakeUtf8("class Unit {}");
		const UTF8CHAR* const Allocated = *Body;
		const int32 ByteCount = Body.Len();

		FAngelscriptSource Source(FString(TEXT("memory/Unit.as")), MoveTemp(Body));

		ASSERT_THAT(IsTrue(
			*Source.GetSourceText() == Allocated,
			TEXT("Move should keep the UTF-8 allocation instead of copying the body.")));
		ASSERT_THAT(AreEqual(ByteCount, Source.GetSourceText().Len()));
		ASSERT_THAT(AreEqual(FString(TEXT("memory/Unit.as")), FString(Source.GetPath())));
		ASSERT_THAT(IsTrue(Source.bHasSourceText));
	}

	TEST_METHOD(PreparationRetainsHostFields)
	{
		FAngelscriptSource Unprepared;
		ASSERT_THAT(IsFalse(Unprepared.bHasSourceText));

		FAngelscriptSource Source = FAngelscriptSource::FromGameFile(
			TEXT("Unit.as"),
			TEXT("C:/Scripts/Unit.as"));
		FUtf8String Body = MakeUtf8("class Unit {}");
		const UTF8CHAR* const Allocated = *Body;
		Source.SourceText = MoveTemp(Body);
		Source.bHasSourceText = true;

		ASSERT_THAT(AreEqual(FString(TEXT("C:/Scripts/Unit.as")), Source.AbsoluteFilename));
		ASSERT_THAT(AreEqual(FString(TEXT("Unit.as")), Source.RelativeFilename));
		ASSERT_THAT(AreEqual(FString(TEXT("Unit")), Source.ModuleName));
		ASSERT_THAT(AreEqual(EAngelscriptSourceKind::Game, Source.SourceKind));
		ASSERT_THAT(IsTrue(Source.VirtualPath.IsValid()));
		ASSERT_THAT(IsTrue(
			*Source.GetSourceText() == Allocated,
			TEXT("Host preparation should own one moved UTF-8 body.")));
		ASSERT_THAT(IsTrue(Source.bHasSourceText));
	}

	TEST_METHOD(SourceCannotCopy)
	{
		ASSERT_THAT(IsFalse(std::is_copy_constructible_v<FAngelscriptSource>));
		ASSERT_THAT(IsFalse(std::is_copy_assignable_v<FAngelscriptSource>));
		ASSERT_THAT(IsTrue(std::is_move_constructible_v<FAngelscriptSource>));
		ASSERT_THAT(IsTrue(std::is_same_v<
			decltype(FAngelscriptSource()),
			FAngelscriptSource>));
	}

	TEST_METHOD(GenericPathDoesNotRequireMount)
	{
		FAngelscriptSource Exact(FString(TEXT("A.as")), MakeUtf8(""));
		FAngelscriptSource CaseFold(FString(TEXT("a.as")), MakeUtf8(""));
		FAngelscriptSource Relative(FString(TEXT("scratch/../A.as")), MakeUtf8(""));

		ASSERT_THAT(AreEqual(FString(TEXT("A.as")), FString(Exact.GetPath())));
		ASSERT_THAT(AreEqual(FString(TEXT("a.as")), FString(CaseFold.GetPath())));
		ASSERT_THAT(AreEqual(FString(TEXT("scratch/../A.as")), FString(Relative.GetPath())));
		ASSERT_THAT(IsTrue(Exact.bHasSourceText));
		ASSERT_THAT(IsTrue(CaseFold.bHasSourceText));
		ASSERT_THAT(IsTrue(Relative.bHasSourceText));
	}

	TEST_METHOD(PreparedEmptyBody)
	{
		FUtf8String Empty;
		FAngelscriptSource Source(FString(TEXT("Empty.as")), MoveTemp(Empty));

		ASSERT_THAT(IsTrue(Source.bHasSourceText));
		ASSERT_THAT(AreEqual(0, Source.GetSourceText().Len()));
		ASSERT_THAT(AreEqual(FString(TEXT("Empty.as")), FString(Source.GetPath())));
	}

	TEST_METHOD(IndependentPublishedVersions)
	{
		TSharedPtr<FAngelscriptSource, ESPMode::ThreadSafe> OldPrep =
			MakeShared<FAngelscriptSource, ESPMode::ThreadSafe>(
				FString(TEXT("A.as")),
				MakeUtf8("class Old {}"));
		TSharedPtr<FAngelscriptSource, ESPMode::ThreadSafe> NewPrep =
			MakeShared<FAngelscriptSource, ESPMode::ThreadSafe>(
				FString(TEXT("A.as")),
				MakeUtf8("class New {}"));

		const TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe> OldPublished =
			OldPrep.ToSharedRef();
		const TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe> NewPublished =
			NewPrep.ToSharedRef();
		OldPrep.Reset();
		NewPrep.Reset();

		ASSERT_THAT(AreEqual(FString(TEXT("A.as")), FString(OldPublished->GetPath())));
		ASSERT_THAT(AreEqual(FString(TEXT("A.as")), FString(NewPublished->GetPath())));
		ASSERT_THAT(IsTrue(
			Utf8Equals(OldPublished->GetSourceText(), "class Old {}"),
			TEXT("The old published version should keep class Old {}.")));
		ASSERT_THAT(IsTrue(
			Utf8Equals(NewPublished->GetSourceText(), "class New {}"),
			TEXT("The new published version should keep class New {}.")));
	}
};

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
