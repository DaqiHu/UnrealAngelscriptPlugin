#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestExprResolutionGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ExprResolutionGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString Needle;
		int32 Expected = 0;
		bool bReject = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-EXPR-RESOLUTION-INITIALIZER-IDENTIFIER-EXACT");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-CONDITION-IF-BOOL_LITERAL-TRUE");

	static const TCHAR* ContextToken(const int32 Context)
	{
		static const TCHAR* const kTokens[] = {
			TEXT("INITIALIZER"), TEXT("ASSIGNMENT"), TEXT("ARGUMENT"), TEXT("RETURN"),
			TEXT("CONDITION"), TEXT("INDEX"), TEXT("MEMBER_RECEIVER")};
		return kTokens[Context];
	}

	static const TCHAR* ShapeToken(const int32 Shape)
	{
		static const TCHAR* const kTokens[] = {
			TEXT("IDENTIFIER"), TEXT("CALL"), TEXT("MEMBER"), TEXT("SCOPED_NAME")};
		return kTokens[Shape];
	}

	static const TCHAR* StateToken(const int32 State)
	{
		static const TCHAR* const kTokens[] = {
			TEXT("EXACT"), TEXT("NAMESPACE_QUALIFIED"), TEXT("OVERLOAD"), TEXT("CONVERSION"),
			TEXT("MISSING"), TEXT("AMBIGUOUS"), TEXT("INACCESSIBLE"), TEXT("WRONG_TYPE")};
		return kTokens[State];
	}

	static const TCHAR* IndependentNeedle(const int32 Shape, const int32 State)
	{
		if (State == 0)
		{
			static const TCHAR* const kExact[] = {
				TEXT("ExactIdentifier"), TEXT("ResolveExactCall()"), TEXT("ResolutionOwner.ExactMember"),
				TEXT("ResolutionExact::ExactScoped")};
			return kExact[Shape];
		}
		if (State == 1)
		{
			static const TCHAR* const kNs[] = {
				TEXT("ResolutionQualified::QualifiedIdentifier"), TEXT("ResolutionQualified::ResolveCall()"),
				TEXT("QualifiedOwner.QualifiedMember"), TEXT("ResolutionOuter::ResolutionInner::QualifiedScoped")};
			return kNs[Shape];
		}
		if (State == 2)
		{
			static const TCHAR* const kOver[] = {
				TEXT("OverloadIdentifier"), TEXT("ResolveCallOverload(7)"),
				TEXT("ResolutionOwner.ResolveMemberOverload(7)"), TEXT("ResolutionOverload::Resolve(7)")};
			return kOver[Shape];
		}
		if (State == 3)
		{
			static const TCHAR* const kConv[] = {
				TEXT("ConversionIdentifier"), TEXT("ResolveCallConversion(int8(7))"),
				TEXT("ResolutionOwner.ResolveMemberConversion(int8(7))"), TEXT("ResolutionConversion::Resolve(int8(7))")};
			return kConv[Shape];
		}
		if (State == 4)
		{
			static const TCHAR* const kMiss[] = {
				TEXT("MissingIdentifier"), TEXT("MissingCall()"), TEXT("MissingMember"), TEXT("MissingScoped")};
			return kMiss[Shape];
		}
		if (State == 5)
		{
			static const TCHAR* const kAmb[] = {
				TEXT("AmbiguousIdentifier()"), TEXT("ResolveAmbiguousCall(nullptr)"),
				TEXT("ResolveAmbiguousMember(nullptr)"), TEXT("ResolutionAmbiguous::Resolve(nullptr)")};
			return kAmb[Shape];
		}
		if (State == 6)
		{
			static const TCHAR* const kHid[] = {
				TEXT("HiddenIdentifier"), TEXT("HiddenCall()"), TEXT("HiddenMember"), TEXT("HiddenScoped")};
			return kHid[Shape];
		}
		static const TCHAR* const kWrong[] = {
			TEXT("WrongIdentifier"), TEXT("ResolveWrongCall()"), TEXT("WrongMember"), TEXT("ResolutionWrong::Resolve()")};
		return kWrong[Shape];
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		TArray<FExpectedCell> Cells;
		Cells.SetNum(224);
		for (int32 Index = 0; Index < 224; ++Index)
		{
			const int32 Context = Index / 32;
			const int32 Shape = (Index / 8) % 4;
			const int32 State = Index % 8;
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-EXPR-RESOLUTION-%s-%s-%s"),
				ContextToken(Context),
				ShapeToken(Shape),
				StateToken(State));
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Needle = IndependentNeedle(Shape, State);
			Cell.bReject = State >= 4;
			Cell.Expected = Cell.bReject ? 0 : ((State + 1) * 100 + (Shape + 1));
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EExprResolutionContext;
		using AngelscriptTest::Generate::EExprResolutionShape;
		using AngelscriptTest::Generate::EExprResolutionState;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FExprResolutionGenerator;
		using AngelscriptTest::Generate::FExprResolutionParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FExprResolutionGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-EXPR-RESOLUTION")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangExprResolutionInitializerIdentifierExact")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ExprResolutionGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ExprResolutionGenerator dump path=%s cases=224 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(112, CaseCount));
		ASSERT_THAT(AreEqual(224, Cases.Num()));
		ASSERT_THAT(AreEqual(112, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/ExprResolutionGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 RejectCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			if (Expected.bReject)
			{
				++RejectCount;
				ASSERT_THAT(AreEqual(FString(), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::CompileReject));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(RejectIds.Contains(Expected.CaseId)));
				const FString RejectSource = Generator.BuildRejectSource(Expected.CaseId);
				ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.Needle)));
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
			}
			else
			{
				ASSERT_THAT(AreEqual(
					FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				const FString CaseSource = Generator.BuildCaseSource(Expected.CaseId);
				ASSERT_THAT(IsTrue(CaseSource.Contains(Expected.Needle)));
			}
			if (Representative.Contains(Expected.EntryName))
			{
				const FString CaseSource = Expected.bReject
					? Generator.BuildRejectSource(Expected.CaseId)
					: Generator.BuildCaseSource(Expected.CaseId);
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}
		ASSERT_THAT(AreEqual(112, RejectCount));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("struct FNativeCaseValue"))));

		FExprResolutionParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildExpressionResolutionSource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildExpressionResolutionSource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildExpressionResolutionSource(DefaultParams).IsEmpty()));
		DefaultParams.FunctionName.Reset();
		DefaultParams.State = static_cast<EExprResolutionState>(8);
		ASSERT_THAT(IsTrue(Generator.BuildExpressionResolutionSource(DefaultParams).IsEmpty()));
		FExprResolutionParams RejectParams;
		RejectParams.State = EExprResolutionState::Missing;
		ASSERT_THAT(IsTrue(Generator.BuildExpressionResolutionSource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(224, CountOccurrences(Dump, TEXT("// Case: LANG-EXPR-RESOLUTION-"))));
		ASSERT_THAT(AreEqual(112, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/ExprResolutionGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ExprResolutionGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
