#include "CQTest.h"
#include "Framework/Source/AngelscriptTestCodeBuilder.h"
#include "Framework/Source/AngelscriptTestSourceResult.h"

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

	TEST_METHOD(ValidDescriptorBecomesImmutableSource)
	{
		const uint8 CleanBytes[] = {0x61, 0xce, 0xb2, 0x62};
		AngelscriptTest::FAngelscriptTestSourceDescriptor Descriptor;
		Descriptor.AuthoredPath = TEXT("Language/Counter.as");
		Descriptor.AuthoredLine = 12;
		Descriptor.AuthoredEnd = 50;
		Descriptor.Annotations.Points.Add({TEXT("beta"), 1});
		Descriptor.Annotations.Breakpoints.Add({TEXT("tail"), 3});
		Descriptor.Annotations.Ranges.Add({TEXT("beta"), 1, 3});
		Descriptor.OriginSpans.Add({0, 10, 1});
		Descriptor.OriginSpans.Add({1, 20, 2});
		Descriptor.OriginSpans.Add({3, 40, 1});

		AngelscriptTest::FAngelscriptTestCodeBuilder CodeBuilder(MakeFileMeta());
		CodeBuilder.AddRoot(
			MakeVersionMeta(TEXT("root"), {}),
			AngelscriptTest::FAngelscriptTestSource::FromBytes(MakeArrayView(CleanBytes)),
			Descriptor);

		auto Result = CodeBuilder.Build();
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		if (!Result.IsSuccess())
		{
			return;
		}

		const auto* Root = FindCase(Result, TEXT("root"));
		ASSERT_THAT(IsNotNull(Root));
		if (Root == nullptr)
		{
			return;
		}

		const auto Source = Root->GetSource();
		ASSERT_THAT(AreEqual(4, Source.GetBytes().Num()));
		ASSERT_THAT(IsNotNull(Source.GetAnnotations().GetPoint(TEXT("beta"))));
		ASSERT_THAT(AreEqual(1, *Source.GetAnnotations().GetPoint(TEXT("beta"))));
		ASSERT_THAT(IsNotNull(Source.GetAnnotations().GetBreakpoint(TEXT("tail"))));
		ASSERT_THAT(AreEqual(3, *Source.GetAnnotations().GetBreakpoint(TEXT("tail"))));
		ASSERT_THAT(IsNotNull(Source.GetAnnotations().GetRange(TEXT("beta"))));
		ASSERT_THAT(AreEqual(1, Source.GetAnnotations().GetRange(TEXT("beta"))->Begin));
		ASSERT_THAT(AreEqual(3, Source.GetAnnotations().GetRange(TEXT("beta"))->End));
		ASSERT_THAT(AreEqual(FString(TEXT("Language/Counter.as")), FString(Source.GetOriginMap().GetHostFile())));
		ASSERT_THAT(AreEqual(12, Source.GetOriginMap().GetHostLine()));
		ASSERT_THAT(AreEqual(10, Source.GetOriginMap().MapToOriginalByteOffset(0)));
		ASSERT_THAT(AreEqual(20, Source.GetOriginMap().MapToOriginalByteOffset(1)));
		ASSERT_THAT(AreEqual(21, Source.GetOriginMap().MapToOriginalByteOffset(2)));
		ASSERT_THAT(AreEqual(40, Source.GetOriginMap().MapToOriginalByteOffset(3)));
		ASSERT_THAT(AreEqual(50, Source.GetOriginMap().MapToOriginalByteOffset(4)));
	}

	TEST_METHOD(TemporaryDescriptorLifetimeEndsSafely)
	{
		TOptional<AngelscriptTest::FAngelscriptTestSourceCase> RetainedCase;
		{
			AngelscriptTest::FAngelscriptTestSourceDescriptor Descriptor;
			Descriptor.AuthoredPath = TEXT("Language/Temp.as");
			Descriptor.AuthoredLine = 3;
			Descriptor.AuthoredEnd = 2;
			Descriptor.Annotations.Points.Add({TEXT("end"), 1});
			Descriptor.OriginSpans.Add({0, 0, 1});

			const uint8 CleanByte = static_cast<uint8>('x');
			AngelscriptTest::FAngelscriptTestCodeBuilder CodeBuilder(MakeFileMeta());
			CodeBuilder.AddRoot(
				MakeVersionMeta(TEXT("root"), {}),
				AngelscriptTest::FAngelscriptTestSource::FromBytes(MakeArrayView(&CleanByte, 1)),
				Descriptor);
			auto Result = CodeBuilder.Build();
			ASSERT_THAT(IsTrue(Result.IsSuccess()));
			if (!Result.IsSuccess())
			{
				return;
			}
			RetainedCase.Emplace(*FindCase(Result, TEXT("root")));
		}

		const auto Source = RetainedCase->GetSource();
		ASSERT_THAT(AreEqual(FString(TEXT("Language/Temp.as")), FString(Source.GetOriginMap().GetHostFile())));
		ASSERT_THAT(AreEqual(3, Source.GetOriginMap().GetHostLine()));
		ASSERT_THAT(AreEqual(1, *Source.GetAnnotations().GetPoint(TEXT("end"))));
		ASSERT_THAT(AreEqual(0, Source.GetOriginMap().MapToOriginalByteOffset(0)));
		ASSERT_THAT(AreEqual(2, Source.GetOriginMap().MapToOriginalByteOffset(1)));
	}

	TEST_METHOD(IndependentDescriptorErrorsAccumulate)
	{
		const uint8 CleanBytes[] = {0x61, 0x62};
		AngelscriptTest::FAngelscriptTestSourceDescriptor Descriptor;
		Descriptor.AuthoredEnd = 2;
		Descriptor.Annotations.Points.Add({TEXT("beyond"), 3});
		Descriptor.OriginSpans.Add({0, 0, 1});

		AngelscriptTest::FAngelscriptTestCodeBuilder CodeBuilder(MakeFileMeta());
		CodeBuilder.AddRoot(
			MakeVersionMeta(TEXT("root"), {}),
			AngelscriptTest::FAngelscriptTestSource::FromBytes(MakeArrayView(CleanBytes)),
			Descriptor);

		auto Result = CodeBuilder.Build();
		ASSERT_THAT(IsFalse(Result.IsSuccess()));
		ASSERT_THAT(IsTrue(HasErrorCode(Result.GetErrors(), TEXT("InvalidAnnotationOffset"))));
		ASSERT_THAT(IsTrue(HasErrorCode(Result.GetErrors(), TEXT("IncompleteOriginCoverage"))));
	}

	TEST_METHOD(RangeAndSpanBoundariesRejectAmbiguity)
	{
		const uint8 CleanBytes[] = {0x61, 0x62};
		const auto Clean = AngelscriptTest::FAngelscriptTestSource::FromBytes(MakeArrayView(CleanBytes));

		{
			AngelscriptTest::FAngelscriptTestSourceDescriptor Descriptor;
			Descriptor.AuthoredEnd = 2;
			Descriptor.Annotations.Ranges.Add({TEXT("bad"), 2, 1});
			Descriptor.OriginSpans.Add({0, 0, 2});
			AngelscriptTest::FAngelscriptTestCodeBuilder CodeBuilder(MakeFileMeta());
			CodeBuilder.AddRoot(MakeVersionMeta(TEXT("root"), {}), Clean, Descriptor);
			auto Result = CodeBuilder.Build();
			ASSERT_THAT(IsFalse(Result.IsSuccess()));
			ASSERT_THAT(IsTrue(HasErrorCode(Result.GetErrors(), TEXT("InvalidSourceRange"))));
		}
		{
			AngelscriptTest::FAngelscriptTestSourceDescriptor Descriptor;
			Descriptor.AuthoredEnd = 2;
			Descriptor.Annotations.Ranges.Add({TEXT("bad"), 0, 3});
			Descriptor.OriginSpans.Add({0, 0, 2});
			AngelscriptTest::FAngelscriptTestCodeBuilder CodeBuilder(MakeFileMeta());
			CodeBuilder.AddRoot(MakeVersionMeta(TEXT("root"), {}), Clean, Descriptor);
			auto Result = CodeBuilder.Build();
			ASSERT_THAT(IsFalse(Result.IsSuccess()));
			ASSERT_THAT(IsTrue(HasErrorCode(Result.GetErrors(), TEXT("InvalidSourceRange"))));
		}
		{
			AngelscriptTest::FAngelscriptTestSourceDescriptor Descriptor;
			Descriptor.AuthoredEnd = 2;
			Descriptor.OriginSpans.Add({0, 0, 0});
			AngelscriptTest::FAngelscriptTestCodeBuilder CodeBuilder(MakeFileMeta());
			CodeBuilder.AddRoot(MakeVersionMeta(TEXT("root"), {}), Clean, Descriptor);
			auto Result = CodeBuilder.Build();
			ASSERT_THAT(IsFalse(Result.IsSuccess()));
			ASSERT_THAT(IsTrue(HasErrorCode(Result.GetErrors(), TEXT("InvalidOriginSpan"))));
		}
		{
			AngelscriptTest::FAngelscriptTestSourceDescriptor Descriptor;
			Descriptor.AuthoredEnd = 1;
			Descriptor.OriginSpans.Add({0, 0, 2});
			AngelscriptTest::FAngelscriptTestCodeBuilder CodeBuilder(MakeFileMeta());
			CodeBuilder.AddRoot(MakeVersionMeta(TEXT("root"), {}), Clean, Descriptor);
			auto Result = CodeBuilder.Build();
			ASSERT_THAT(IsFalse(Result.IsSuccess()));
			ASSERT_THAT(IsTrue(HasErrorCode(Result.GetErrors(), TEXT("InvalidAuthoredEnd"))));
		}
	}

	TEST_METHOD(ExistingTwoArgumentAddRootRemainsValid)
	{
		AngelscriptTest::FAngelscriptTestCodeBuilder CodeBuilder(MakeFileMeta());
		CodeBuilder.AddRoot(MakeVersionMeta(TEXT("root"), {}), AS_TEST_SOURCE_EXACT("ok"));
		auto Result = CodeBuilder.Build();
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		const auto* Root = FindCase(Result, TEXT("root"));
		ASSERT_THAT(IsNotNull(Root));
		if (Root != nullptr)
		{
			const auto Source = Root->GetSource();
			ASSERT_THAT(AreEqual(0, Source.GetAnnotations().Num()));
			ASSERT_THAT(AreEqual(2, Source.GetBytes().Num()));
		}
	}

	TEST_METHOD(InvalidAngelScriptRemainsValidSource)
	{
		AngelscriptTest::FAngelscriptTestSourceDescriptor Descriptor;
		Descriptor.AuthoredEnd = 14;
		Descriptor.OriginSpans.Add({0, 0, 14});
		AngelscriptTest::FAngelscriptTestCodeBuilder CodeBuilder(MakeFileMeta());
		CodeBuilder.AddRoot(
			MakeVersionMeta(TEXT("root"), {}),
			AS_TEST_SOURCE_EXACT("int X=Missing;"),
			Descriptor);
		auto Result = CodeBuilder.Build();
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
	}

	TEST_METHOD(AddVersionWithoutParentSucceeds)
	{
		AngelscriptTest::FAngelscriptTestCodeBuilder CodeBuilder(MakeFileMeta());
		CodeBuilder.AddVersion(MakeVersionMeta(TEXT("alpha"), {}), MakeBody('A'));
		CodeBuilder.AddVersion(MakeVersionMeta(TEXT("beta"), {}), MakeBody('B'));

		auto Result = CodeBuilder.Build();
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(IsFalse(HasErrorCode(Result.GetErrors(), TEXT("VersionParentRequired"))));
		ASSERT_THAT(IsFalse(HasErrorCode(Result.GetErrors(), TEXT("MissingRoot"))));
		if (!Result.IsSuccess())
		{
			return;
		}

		const auto* Alpha = FindCase(Result, TEXT("alpha"));
		const auto* Beta = FindCase(Result, TEXT("beta"));
		ASSERT_THAT(IsNotNull(Alpha));
		ASSERT_THAT(IsNotNull(Beta));
		if (Alpha != nullptr && Beta != nullptr)
		{
			ASSERT_THAT(IsFalse(Alpha->GetVersionMeta().Parent.IsSet()));
			ASSERT_THAT(IsFalse(Beta->GetVersionMeta().Parent.IsSet()));
		}
	}

	TEST_METHOD(RootTagHasNoPrivilege)
	{
		AngelscriptTest::FAngelscriptTestCodeBuilder CodeBuilder(MakeFileMeta());
		CodeBuilder.AddVersion(MakeVersionMeta(TEXT("root"), {}), MakeBody('R'));
		CodeBuilder.AddVersion(MakeVersionMeta(TEXT("other"), {}), MakeBody('O'));

		auto Result = CodeBuilder.Build();
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(IsFalse(HasErrorCode(Result.GetErrors(), TEXT("RootMustUseAddRoot"))));
		ASSERT_THAT(IsFalse(HasErrorCode(Result.GetErrors(), TEXT("VersionParentRequired"))));
		if (!Result.IsSuccess())
		{
			return;
		}

		const auto* Root = FindCase(Result, TEXT("root"));
		const auto* Other = FindCase(Result, TEXT("other"));
		ASSERT_THAT(IsNotNull(Root));
		ASSERT_THAT(IsNotNull(Other));
		if (Root != nullptr && Other != nullptr)
		{
			ASSERT_THAT(IsFalse(Root->GetVersionMeta().Parent.IsSet()));
			ASSERT_THAT(IsFalse(Other->GetVersionMeta().Parent.IsSet()));
		}
	}
};

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
