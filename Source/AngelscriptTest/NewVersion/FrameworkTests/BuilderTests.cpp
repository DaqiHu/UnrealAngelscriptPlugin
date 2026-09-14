#include "CQTest.h"
#include "NewVersion/Framework/Source/AngelscriptTestCodeBuilder.h"
#include "NewVersion/Framework/Source/AngelscriptTestSourceResult.h"

#include <type_traits>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest
{
struct FAngelscriptTestCodeBuildResultTestAccess
{
	static const TArray<FAngelscriptTestSourceCase>& GetCases(
		const FAngelscriptTestCodeBuildResult& Result)
	{
		return Result.GetCasesForAdmission();
	}

	static FAngelscriptTestSourceResult MakeSourceResult(FAngelscriptTestSourceCase SourceCase)
	{
		return FAngelscriptTestSourceResult(MoveTemp(SourceCase));
	}
};
}

template <typename T>
concept CCanBorrowFileMeta = requires(T&& Value)
{
	Forward<T>(Value).GetFileMeta();
};

template <typename T>
concept CCanBorrowVersionMeta = requires(T&& Value)
{
	Forward<T>(Value).GetVersionMeta();
};

template <typename T>
concept CCanBorrowResultCase = requires(T&& Value)
{
	Forward<T>(Value).GetCase();
};

template <typename T>
concept CCanBorrowResultError = requires(T&& Value)
{
	Forward<T>(Value).GetError();
};

static_assert(CCanBorrowFileMeta<const AngelscriptTest::FAngelscriptTestSourceCase&>);
static_assert(!CCanBorrowFileMeta<AngelscriptTest::FAngelscriptTestSourceCase>);
static_assert(CCanBorrowVersionMeta<const AngelscriptTest::FAngelscriptTestSourceCase&>);
static_assert(!CCanBorrowVersionMeta<AngelscriptTest::FAngelscriptTestSourceCase>);
static_assert(CCanBorrowResultCase<const AngelscriptTest::FAngelscriptTestSourceResult&>);
static_assert(!CCanBorrowResultCase<AngelscriptTest::FAngelscriptTestSourceResult>);
static_assert(CCanBorrowResultError<const AngelscriptTest::FAngelscriptTestSourceResult&>);
static_assert(!CCanBorrowResultError<AngelscriptTest::FAngelscriptTestSourceResult>);

TEST_CLASS_WITH_FLAGS(
	Builder,
	"Angelscript.UnitTest.Framework",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static AngelscriptTest::FAngelscriptTestFileMeta MakeFileMeta()
	{
		return {
			.Version = TEXT("v1"),
			.Tag = TEXT("Language/Counter"),
			.Summary = TEXT("Counter source variants."),
			.Topics = {TEXT("Language")},
		};
	}

	static AngelscriptTest::FAngelscriptTestVersionMeta MakeVersionMeta(
		FString Tag,
		TOptional<FString> Parent,
		FString Summary = TEXT("A complete source version."))
	{
		return {
			.Tag = MoveTemp(Tag),
			.Parent = MoveTemp(Parent),
			.Summary = MoveTemp(Summary),
		};
	}

	static AngelscriptTest::FAngelscriptTestSource MakeBody(uint8 Byte)
	{
		return AngelscriptTest::FAngelscriptTestSource::FromBytes(MakeArrayView(&Byte, 1));
	}

	static bool HasErrorCode(
		const TArray<AngelscriptTest::FAngelscriptTestError>& Errors,
		FStringView Code)
	{
		return Errors.ContainsByPredicate([Code](const AngelscriptTest::FAngelscriptTestError& Error)
		{
			return Error.Code == Code;
		});
	}

	static const AngelscriptTest::FAngelscriptTestSourceCase* FindCase(
		const AngelscriptTest::FAngelscriptTestCodeBuildResult& Result,
		FStringView VersionTag)
	{
		return AngelscriptTest::FAngelscriptTestCodeBuildResultTestAccess::GetCases(Result)
			.FindByPredicate([VersionTag](const AngelscriptTest::FAngelscriptTestSourceCase& SourceCase)
			{
				return SourceCase.GetVersionMeta().Tag == VersionTag;
			});
	}

public:
	TEST_METHOD(IndependentErrors)
	{
		AngelscriptTest::FAngelscriptTestCodeBuilder CodeBuilder(MakeFileMeta());
		CodeBuilder.AddRoot(MakeVersionMeta(TEXT("root"), {}), MakeBody('0'));
		CodeBuilder.AddVersion(MakeVersionMeta(TEXT("x"), FString(TEXT("root"))), MakeBody('1'));
		CodeBuilder.AddVersion(MakeVersionMeta(TEXT("x"), FString(TEXT("root")), TEXT("")), MakeBody('2'));

		auto Result = CodeBuilder.Build();
		ASSERT_THAT(IsFalse(Result.IsSuccess()));
		ASSERT_THAT(IsTrue(HasErrorCode(Result.GetErrors(), TEXT("DuplicateVersionTag"))));
		ASSERT_THAT(IsTrue(HasErrorCode(Result.GetErrors(), TEXT("EmptyVersionSummary"))));
		ASSERT_THAT(AreEqual(2, Result.GetErrors().Num()));
	}

	TEST_METHOD(GraphAndForwardParent)
	{
		AngelscriptTest::FAngelscriptTestCodeBuilder CodeBuilder(MakeFileMeta());
		CodeBuilder.AddVersion(MakeVersionMeta(TEXT("right"), FString(TEXT("left"))), MakeBody('2'));
		CodeBuilder.AddRoot(MakeVersionMeta(TEXT("root"), {}), MakeBody('0'));
		CodeBuilder.AddVersion(MakeVersionMeta(TEXT("left"), FString(TEXT("root"))), MakeBody('1'));

		auto Result = CodeBuilder.Build();
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		if (!Result.IsSuccess())
		{
			return;
		}

		for (const TPair<FStringView, uint8>& Expected : {
			TPair<FStringView, uint8>(TEXT("root"), '0'),
			TPair<FStringView, uint8>(TEXT("left"), '1'),
			TPair<FStringView, uint8>(TEXT("right"), '2')})
		{
			const auto* SourceCase = FindCase(Result, Expected.Key);
			ASSERT_THAT(IsNotNull(SourceCase));
			if (SourceCase != nullptr)
			{
				const auto Source = SourceCase->GetSource();
				ASSERT_THAT(AreEqual(1, Source.GetBytes().Num()));
				ASSERT_THAT(AreEqual(Expected.Value, Source.GetBytes()[0]));
			}
		}

		AngelscriptTest::FAngelscriptTestCodeBuilder CycleBuilder(MakeFileMeta());
		CycleBuilder.AddRoot(MakeVersionMeta(TEXT("root"), {}), MakeBody('0'));
		CycleBuilder.AddVersion(MakeVersionMeta(TEXT("x"), FString(TEXT("y"))), MakeBody('1'));
		CycleBuilder.AddVersion(MakeVersionMeta(TEXT("y"), FString(TEXT("x"))), MakeBody('2'));
		auto CycleResult = CycleBuilder.Build();
		ASSERT_THAT(IsFalse(CycleResult.IsSuccess()));
		ASSERT_THAT(IsTrue(HasErrorCode(CycleResult.GetErrors(), TEXT("VersionCycle"))));
	}

	TEST_METHOD(CaseLifetime)
	{
		TOptional<AngelscriptTest::FAngelscriptTestSourceCase> RetainedCase;
		TOptional<AngelscriptTest::FAngelscriptTestSource> RetainedSource;
		{
			AngelscriptTest::FAngelscriptTestCodeBuilder CodeBuilder(MakeFileMeta());
			CodeBuilder.AddRoot(MakeVersionMeta(TEXT("root"), {}), MakeBody('0'));
			CodeBuilder.AddVersion(MakeVersionMeta(TEXT("sibling"), FString(TEXT("root"))), MakeBody('1'));
			auto BuildResult = CodeBuilder.Build();
			ASSERT_THAT(IsTrue(BuildResult.IsSuccess()));
			if (!BuildResult.IsSuccess())
			{
				return;
			}

			const auto* RootCase = FindCase(BuildResult, TEXT("root"));
			ASSERT_THAT(IsNotNull(RootCase));
			if (RootCase == nullptr)
			{
				return;
			}

			auto LookupResult = AngelscriptTest::FAngelscriptTestCodeBuildResultTestAccess::
				MakeSourceResult(*RootCase);
			ASSERT_THAT(IsTrue(LookupResult.IsSuccess()));
			RetainedCase.Emplace(*LookupResult.GetCase());
			RetainedSource.Emplace(RetainedCase->GetSource());
		}

		ASSERT_THAT(AreEqual(FString(TEXT("Language/Counter")), RetainedCase->GetFileMeta().Tag));
		ASSERT_THAT(AreEqual(FString(TEXT("root")), RetainedCase->GetVersionMeta().Tag));
		RetainedCase.Reset();
		ASSERT_THAT(AreEqual(1, RetainedSource->GetBytes().Num()));
		ASSERT_THAT(AreEqual(static_cast<uint8>('0'), RetainedSource->GetBytes()[0]));
	}

	TEST_METHOD(BrokenASAndConsumedBuilder)
	{
		AngelscriptTest::FAngelscriptTestCodeBuilder CodeBuilder(MakeFileMeta());
		CodeBuilder.AddRoot(
			MakeVersionMeta(TEXT("root"), {}),
			AS_TEST_SOURCE_EXACT("int X=Missing;"));

		auto First = CodeBuilder.Build();
		ASSERT_THAT(IsTrue(First.IsSuccess()));
		auto Second = CodeBuilder.Build();
		ASSERT_THAT(IsFalse(Second.IsSuccess()));
		ASSERT_THAT(IsTrue(HasErrorCode(Second.GetErrors(), TEXT("BuilderConsumed"))));
	}

	TEST_METHOD(BorrowedAccessorBoundary)
	{
		ASSERT_THAT(IsTrue(std::is_copy_constructible_v<AngelscriptTest::FAngelscriptTestSourceCase>));
		ASSERT_THAT(IsTrue(CCanBorrowFileMeta<const AngelscriptTest::FAngelscriptTestSourceCase&>));
		ASSERT_THAT(IsFalse(CCanBorrowFileMeta<AngelscriptTest::FAngelscriptTestSourceCase>));
		ASSERT_THAT(IsFalse(CCanBorrowResultCase<AngelscriptTest::FAngelscriptTestSourceResult>));
	}
};

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
