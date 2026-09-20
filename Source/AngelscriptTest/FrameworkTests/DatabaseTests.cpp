#include "CQTest.h"
#include "Framework/Catalog/AngelscriptTestCode.h"
#include "Framework/Source/AngelscriptTestCodeBuilder.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest
{
struct FAngelscriptTestCodeDatabaseTestAccess
{
	struct FBatch
	{
		FString OwnerModule;
		TArray<FAngelscriptTestCodeBuildResult> Files;
		TArray<FAngelscriptTestError> Errors;
	};

	static TUniquePtr<FAngelscriptTestCode> CreateDetached()
	{
		return TUniquePtr<FAngelscriptTestCode>(new FAngelscriptTestCode());
	}

	static FAngelscriptTestStatus Admit(
		FAngelscriptTestCode& Code,
		TArray<FBatch> Inputs)
	{
		TArray<FAngelscriptTestCode::FAdmissionBatch> Batches;
		Batches.Reserve(Inputs.Num());
		for (FBatch& Input : Inputs)
		{
			FAngelscriptTestCode::FAdmissionBatch& Batch = Batches.AddDefaulted_GetRef();
			Batch.OwnerModule = MoveTemp(Input.OwnerModule);
			Batch.Files = MoveTemp(Input.Files);
			Batch.Errors = MoveTemp(Input.Errors);
		}
		return Code.AdmitDetachedBatches(MoveTemp(Batches));
	}
};
}

TEST_CLASS_WITH_FLAGS(
	Database,
	"Angelscript.UnitTest.Framework",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FAccess = AngelscriptTest::FAngelscriptTestCodeDatabaseTestAccess;

	static AngelscriptTest::FAngelscriptTestCodeBuildResult BuildFile(
		FString FileTag,
		TArray<FString> FileTopics,
		FString Body = TEXT("0"),
		TArray<FString> VersionTopics = {})
	{
		AngelscriptTest::FAngelscriptTestCodeBuilder Builder({
			.Version = TEXT("v1"),
			.Tag = MoveTemp(FileTag),
			.Summary = TEXT("A test file."),
			.Topics = MoveTemp(FileTopics),
		});
		FTCHARToUTF8 Utf8(*Body);
		Builder.AddRoot({
			.Tag = TEXT("root"),
			.Summary = TEXT("The root version."),
			.Topics = MoveTemp(VersionTopics),
		}, AngelscriptTest::FAngelscriptTestSource::FromBytes(MakeArrayView(
			reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length())));
		return Builder.Build();
	}

	static AngelscriptTest::FAngelscriptTestCodeBuildResult BuildInvalidFile(FString FileTag)
	{
		AngelscriptTest::FAngelscriptTestCodeBuilder Builder({
			.Version = TEXT("v1"),
			.Tag = MoveTemp(FileTag),
			.Summary = TEXT("A test file."),
		});
		Builder.AddRoot({
			.Tag = TEXT("root"),
			.Summary = TEXT(""),
		}, AS_TEST_SOURCE_EXACT("bad"));
		return Builder.Build();
	}

	static FAccess::FBatch MakeBatch(FString Owner, AngelscriptTest::FAngelscriptTestCodeBuildResult File)
	{
		FAccess::FBatch Batch;
		Batch.OwnerModule = MoveTemp(Owner);
		Batch.Files.Add(MoveTemp(File));
		return Batch;
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

	static bool HasByte(
		const AngelscriptTest::FAngelscriptTestSourceResult& Result,
		uint8 Expected)
	{
		if (!Result.IsSuccess())
		{
			return false;
		}
		const auto Source = Result.GetCase()->GetSource();
		return Source.GetBytes().Num() == 1 && Source.GetBytes()[0] == Expected;
	}

	static TArray<FAccess::FBatch> MakeConflictBatches(bool bReverse)
	{
		FAccess::FBatch A;
		A.OwnerModule = TEXT("A");
		A.Files.Add(BuildFile(TEXT("Language/Counter"), {TEXT("Shared")}, TEXT("A")));
		A.Files.Add(BuildFile(TEXT("Language/AOnly"), {}, TEXT("a")));

		FAccess::FBatch B;
		B.OwnerModule = TEXT("B");
		B.Files.Add(BuildFile(TEXT("Language/Counter"), {TEXT("Shared")}, TEXT("B")));
		B.Files.Add(BuildFile(TEXT("Language/BOnly"), {}, TEXT("b")));

		FAccess::FBatch C = MakeBatch(
			TEXT("C"), BuildFile(TEXT("Language/Other"), {}, TEXT("C")));
		TArray<FAccess::FBatch> Result;
		if (bReverse)
		{
			Result.Add(MoveTemp(B));
			Result.Add(MoveTemp(A));
		}
		else
		{
			Result.Add(MoveTemp(A));
			Result.Add(MoveTemp(B));
		}
		Result.Add(MoveTemp(C));
		return Result;
	}

public:
	TEST_METHOD(AtomicIsolation)
	{
		auto Code = FAccess::CreateDetached();
		FAccess::FBatch A;
		A.OwnerModule = TEXT("A");
		A.Files.Add(BuildFile(TEXT("Language/AValid"), {}, TEXT("a")));
		A.Files.Add(BuildInvalidFile(TEXT("Language/AInvalid")));
		FAccess::FBatch B = MakeBatch(
			TEXT("B"), BuildFile(TEXT("Language/B"), {}, TEXT("b")));
		TArray<FAccess::FBatch> Batches;
		Batches.Add(MoveTemp(A));
		Batches.Add(MoveTemp(B));
		auto Status = FAccess::Admit(*Code, MoveTemp(Batches));

		ASSERT_THAT(IsFalse(Status.IsSuccess()));
		ASSERT_THAT(IsFalse(Code->Get(TEXT("Language/AValid"), TEXT("root")).IsSuccess()));
		ASSERT_THAT(IsFalse(Code->Get(TEXT("Language/AInvalid"), TEXT("root")).IsSuccess()));
		ASSERT_THAT(IsTrue(HasByte(Code->Get(TEXT("Language/B"), TEXT("root")), 'b')));
	}

	TEST_METHOD(SymmetricConflict)
	{
		for (const bool bReverse : {false, true})
		{
			auto Code = FAccess::CreateDetached();
			auto Status = FAccess::Admit(*Code, MakeConflictBatches(bReverse));
			ASSERT_THAT(IsFalse(Status.IsSuccess()));
			ASSERT_THAT(IsTrue(HasErrorCode(Code->GetRegistrationErrors(), TEXT("FileTagConflict"))));
			ASSERT_THAT(IsFalse(Code->Get(TEXT("Language/Counter"), TEXT("root")).IsSuccess()));
			ASSERT_THAT(IsFalse(Code->Get(TEXT("Language/AOnly"), TEXT("root")).IsSuccess()));
			ASSERT_THAT(IsFalse(Code->Get(TEXT("Language/BOnly"), TEXT("root")).IsSuccess()));
			ASSERT_THAT(IsTrue(HasByte(Code->Get(TEXT("Language/Other"), TEXT("root")), 'C')));
		}
	}

	TEST_METHOD(QueryCompleteness)
	{
		auto Code = FAccess::CreateDetached();
		FAccess::FBatch Unreadable;
		Unreadable.OwnerModule = TEXT("Unreadable");
		Unreadable.Errors.Add({
			.Code = TEXT("UnreadableResource"),
			.Message = TEXT("The provider resource could not be read."),
			.OwnerModule = TEXT("Unreadable"),
		});
		FAccess::FBatch Healthy = MakeBatch(TEXT("Healthy"), BuildFile(
			TEXT("Language/Other"), {TEXT("Language")}, TEXT("O"), {TEXT("Negative"), TEXT("Diagnostics")}));
		TArray<FAccess::FBatch> Batches;
		Batches.Add(MoveTemp(Unreadable));
		Batches.Add(MoveTemp(Healthy));
		FAccess::Admit(*Code, MoveTemp(Batches));

		TArray<AngelscriptTest::FAngelscriptTestFileMeta> Files = {{
			.Version = TEXT("sentinel"),
			.Tag = TEXT("Keep/Me"),
			.Summary = TEXT("Existing output."),
		}};
		auto FileStatus = Code->FindFiles({}, Files);
		ASSERT_THAT(IsFalse(FileStatus.IsSuccess()));
		ASSERT_THAT(AreEqual(1, Files.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("Keep/Me")), Files[0].Tag));
		ASSERT_THAT(IsTrue(HasByte(Code->Get(TEXT("Language/Other"), TEXT("root")), 'O')));

		TArray<AngelscriptTest::FAngelscriptTestSourceCase> Cases;
		auto CaseStatus = Code->FindCases(
			TEXT("Language/Other"), {FString(TEXT("Negative")), FString(TEXT("Diagnostics"))}, Cases);
		ASSERT_THAT(IsTrue(CaseStatus.IsSuccess()));
		ASSERT_THAT(AreEqual(1, Cases.Num()));
	}

	TEST_METHOD(TopicAndUnknown)
	{
		auto Code = FAccess::CreateDetached();
		TArray<FAccess::FBatch> Batches;
		Batches.Add(MakeBatch(TEXT("X"), BuildFile(
			TEXT("X"), {TEXT("A"), TEXT("B")}, TEXT("X"))));
		Batches.Add(MakeBatch(TEXT("Y"), BuildFile(
			TEXT("Y"), {TEXT("A")}, TEXT("Y"))));
		ASSERT_THAT(IsTrue(FAccess::Admit(*Code, MoveTemp(Batches)).IsSuccess()));

		TArray<AngelscriptTest::FAngelscriptTestFileMeta> Files;
		ASSERT_THAT(IsTrue(Code->FindFiles({FString(TEXT("A")), FString(TEXT("B"))}, Files).IsSuccess()));
		ASSERT_THAT(AreEqual(1, Files.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("X")), Files[0].Tag));

		ASSERT_THAT(IsTrue(Code->FindFiles({}, Files).IsSuccess()));
		ASSERT_THAT(AreEqual(2, Files.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("X")), Files[0].Tag));
		ASSERT_THAT(AreEqual(FString(TEXT("Y")), Files[1].Tag));

		ASSERT_THAT(IsTrue(Code->FindFiles({FString(TEXT("Z"))}, Files).IsSuccess()));
		ASSERT_THAT(AreEqual(0, Files.Num()));
		ASSERT_THAT(IsFalse(Code->Get(TEXT("X"), TEXT("missing")).IsSuccess()));
	}

	TEST_METHOD(DetachedOwners)
	{
		TOptional<AngelscriptTest::FAngelscriptTestSourceCase> RetainedCase;
		TOptional<AngelscriptTest::FAngelscriptTestSource> RetainedSource;
		{
			auto Code = FAccess::CreateDetached();
			TArray<FAccess::FBatch> Batches;
			Batches.Add(MakeBatch(TEXT("Owner"), BuildFile(
				TEXT("Language/Detached"), {}, TEXT("D"))));
			FAccess::Admit(*Code, MoveTemp(Batches));
			auto Result = Code->Get(TEXT("Language/Detached"), TEXT("root"));
			ASSERT_THAT(IsTrue(Result.IsSuccess()));
			if (!Result.IsSuccess())
			{
				return;
			}
			RetainedCase.Emplace(*Result.GetCase());
			RetainedSource.Emplace(Result.GetCase()->GetSource());
		}

		ASSERT_THAT(AreEqual(FString(TEXT("Language/Detached")), RetainedCase->GetFileMeta().Tag));
		ASSERT_THAT(AreEqual(FString(TEXT("root")), RetainedCase->GetVersionMeta().Tag));
		RetainedCase.Reset();
		ASSERT_THAT(AreEqual(static_cast<uint8>('D'), RetainedSource->GetBytes()[0]));
	}
};

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
