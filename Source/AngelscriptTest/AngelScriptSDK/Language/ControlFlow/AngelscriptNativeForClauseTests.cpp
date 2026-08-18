#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FForClauseTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.ControlFlow.ForClauses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FPresenceCase
	{
		const ANSICHAR* CatalogName;
		bool bPresent;
	};

	struct FCountCase
	{
		const ANSICHAR* CatalogName;
		int32 Limit;
	};

	struct FForExpectation
	{
		int32 InitCount = 0;
		int32 BodyCount = 0;
		int32 ConditionCount = 0;
		int32 IncrementCount = 0;
	};

	inline static constexpr FPresenceCase PresenceCases[] =
	{
		{ "present", true },
		{ "omitted", false },
	};

	inline static constexpr FCountCase CountCases[] =
	{
		{ "zero", 0 },
		{ "one", 1 },
		{ "many", 4 },
	};


	static FString PresenceClause(const FPresenceCase& Case, const TCHAR* PresentText)
	{
		return Case.bPresent ? FString(PresentText) : FString();
	}

	static FString BuildForSource(
		const FPresenceCase& InitCase,
		const FPresenceCase& ConditionCase,
		const FPresenceCase& IncrementCase,
		const FCountCase& CountCase)
	{
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int InitializeIndex(int&inout InitCount)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("	++InitCount;"));
		AppendGeneratedAsLine(Source, TEXT("	return 0;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("bool CountCondition(int Index, int Limit, int&inout ConditionCount)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("	++ConditionCount;"));
		AppendGeneratedAsLine(Source, TEXT("	return Index < Limit;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int CountIncrement(int&inout Index, int&inout IncrementCount)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("	++IncrementCount;"));
		AppendGeneratedAsLine(Source, TEXT("	++Index;"));
		AppendGeneratedAsLine(Source, TEXT("	return Index;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("	int Limit = %d;"), CountCase.Limit));
		AppendGeneratedAsLine(Source, TEXT("	int Index = 0;"));
		AppendGeneratedAsLine(Source, TEXT("	int InitCount = 0;"));
		AppendGeneratedAsLine(Source, TEXT("	int BodyCount = 0;"));
		AppendGeneratedAsLine(Source, TEXT("	int ConditionCount = 0;"));
		AppendGeneratedAsLine(Source, TEXT("	int IncrementCount = 0;"));

		const FString InitClause = PresenceClause(InitCase, TEXT("Index = InitializeIndex(InitCount)"));
		const FString ConditionClause = PresenceClause(ConditionCase, TEXT("CountCondition(Index, Limit, ConditionCount)"));
		const FString IncrementClause = PresenceClause(IncrementCase, TEXT("CountIncrement(Index, IncrementCount)"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("	for (%s; %s; %s)"), *InitClause, *ConditionClause, *IncrementClause));
		AppendGeneratedAsLine(Source, TEXT("	{"));
		AppendGeneratedAsLine(Source, TEXT("		++BodyCount;"));
		if (!ConditionCase.bPresent)
		{
			AppendGeneratedAsLine(Source, TEXT("		if (Index >= Limit - 1)"));
			AppendGeneratedAsLine(Source, TEXT("		{"));
			AppendGeneratedAsLine(Source, TEXT("			break;"));
			AppendGeneratedAsLine(Source, TEXT("		}"));
		}
		if (!IncrementCase.bPresent)
		{
			AppendGeneratedAsLine(Source, TEXT("		++Index;"));
		}
		AppendGeneratedAsLine(Source, TEXT("	}"));
		AppendGeneratedAsLine(Source, TEXT("	return InitCount * 1000 + BodyCount * 100 + ConditionCount * 10 + IncrementCount;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FForExpectation GetExpected(
		const FPresenceCase& InitCase,
		const FPresenceCase& ConditionCase,
		const FPresenceCase& IncrementCase,
		const FCountCase& CountCase)
	{
		FForExpectation Expected;
		Expected.InitCount = InitCase.bPresent ? 1 : 0;
		Expected.BodyCount = ConditionCase.bPresent
			? CountCase.Limit
			: (CountCase.Limit > 0 ? CountCase.Limit : 1);
		Expected.ConditionCount = ConditionCase.bPresent ? CountCase.Limit + 1 : 0;
		if (IncrementCase.bPresent)
		{
			Expected.IncrementCount = ConditionCase.bPresent
				? CountCase.Limit
				: (Expected.BodyCount > 0 ? Expected.BodyCount - 1 : 0);
		}
		return Expected;
	}

public:
	TEST_METHOD(ClausesByPresenceAndCount)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CF-FOR-CLAUSES",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Bytecode
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine CaptureOffEngine;
		AngelscriptNativeTestSupport::FNativeTestEngine CaptureOnEngine;
		CaptureOffEngine.Create(*TestRunner);
		CaptureOnEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			CaptureOnEngine.Destroy();
			CaptureOffEngine.Destroy();
		};

		asIScriptEngine* const CaptureOffScriptEngine = CaptureOffEngine.Get();
		asIScriptEngine* const CaptureOnScriptEngine = CaptureOnEngine.Get();
		ASSERT_THAT(IsNotNull(CaptureOffScriptEngine,
			TEXT("For-clause product should create its capture-off raw SDK engine")));
		ASSERT_THAT(IsNotNull(CaptureOnScriptEngine,
			TEXT("For-clause product should create its capture-on raw SDK engine")));
		if (CaptureOffScriptEngine == nullptr || CaptureOnScriptEngine == nullptr)
		{
			return;
		}
		asCScriptEngine* const CaptureOffInternal =
			static_cast<asCScriptEngine*>(CaptureOffScriptEngine);
		asCScriptEngine* const CaptureOnInternal =
			static_cast<asCScriptEngine*>(CaptureOnScriptEngine);
		ASSERT_THAT(IsFalse(CaptureOffInternal->IsTypedSemanticIRCaptureEnabled()));
		ASSERT_THAT(IsFalse(CaptureOnInternal->IsTypedSemanticIRCaptureEnabled()));
		CaptureOnInternal->SetTypedSemanticIRCapture(true);

		int32 ObservedCaseCount = 0;
		for (const FPresenceCase& InitCase : PresenceCases)
		{
			for (const FPresenceCase& ConditionCase : PresenceCases)
			{
				for (const FPresenceCase& IncrementCase : PresenceCases)
				{
					for (const FCountCase& CountCase : CountCases)
					{
						++ObservedCaseCount;
						const FNativeCaseContext Case(MakeNativeCaseId(
							"LANG-CF-FOR-CLAUSES",
							{
								ANSI_TO_TCHAR(InitCase.CatalogName),
								ANSI_TO_TCHAR(ConditionCase.CatalogName),
								ANSI_TO_TCHAR(IncrementCase.CatalogName),
								ANSI_TO_TCHAR(CountCase.CatalogName),
							}));
						const FString ModuleName = FString::Printf(
							TEXT("ForClauses_%s_%s_%s_%s"),
							ANSI_TO_TCHAR(InitCase.CatalogName),
							ANSI_TO_TCHAR(ConditionCase.CatalogName),
							ANSI_TO_TCHAR(IncrementCase.CatalogName),
							ANSI_TO_TCHAR(CountCase.CatalogName));
						const FString Source = BuildForSource(InitCase, ConditionCase, IncrementCase, CountCase);
						const FForExpectation Expected = GetExpected(InitCase, ConditionCase, IncrementCase, CountCase);
						const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
						const FTCHARToUTF8 SourceUtf8(*Source);

						CaptureOffEngine.Reset(*TestRunner);
						CaptureOnEngine.Reset(*TestRunner);
						PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
						asIScriptModule* CaptureOffModule = nullptr;
						asIScriptModule* CaptureOnModule = nullptr;
						const int CaptureOffBuildResult = CompileNativeModule(
							CaptureOffScriptEngine,
							ModuleNameUtf8.Get(),
							SourceUtf8.Get(),
							CaptureOffModule);
						const FString CaptureOffDiagnostics =
							CaptureOffEngine.GetMessagesText();
						const int CaptureOnBuildResult = CompileNativeModule(
							CaptureOnScriptEngine,
							ModuleNameUtf8.Get(),
							SourceUtf8.Get(),
							CaptureOnModule);
						const FString CaptureOnDiagnostics =
							CaptureOnEngine.GetMessagesText();
						ASSERT_THAT(AreEqual(
							CaptureOffBuildResult,
							CaptureOnBuildResult,
							*Case.Describe(TEXT("HIR capture must preserve the for-clause build outcome"))));
						ASSERT_THAT(AreEqual(
							CaptureOffDiagnostics,
							CaptureOnDiagnostics,
							*Case.Describe(TEXT("HIR capture must preserve every for-clause diagnostic"))));
						ASSERT_THAT(AreEqual(asSUCCESS, CaptureOffBuildResult,
							*Case.Describe(TEXT("for clause source should compile for the selected presence and count"))));
						ASSERT_THAT(IsNotNull(CaptureOffModule,
							*Case.Describe(TEXT("capture-off for clause source should publish a module"))));
						ASSERT_THAT(IsNotNull(CaptureOnModule,
							*Case.Describe(TEXT("capture-on for clause source should publish a module"))));

						if (CaptureOffBuildResult >= 0
							&& CaptureOnBuildResult >= 0
							&& CaptureOffModule != nullptr
							&& CaptureOnModule != nullptr)
						{
							asIScriptFunction* const CaptureOffEntry =
								GetNativeFunctionByExactDecl(CaptureOffModule, "int Entry()");
							asIScriptFunction* const CaptureOnEntry =
								GetNativeFunctionByExactDecl(CaptureOnModule, "int Entry()");
							ASSERT_THAT(IsNotNull(CaptureOffEntry,
								*Case.Describe(TEXT("capture-off for clause source should publish the exact Entry declaration"))));
							ASSERT_THAT(IsNotNull(CaptureOnEntry,
								*Case.Describe(TEXT("capture-on for clause source should publish the exact Entry declaration"))));
							if (CaptureOffEntry != nullptr && CaptureOnEntry != nullptr)
							{
								const TArray<asDWORD> CaptureOffBytecode =
									CopyNativeBytecode(CaptureOffEntry);
								const TArray<asDWORD> CaptureOnBytecode =
									CopyNativeBytecode(CaptureOnEntry);
								ASSERT_THAT(IsTrue(CaptureOffBytecode.Num() > 0,
									*Case.Describe(TEXT("for clause entry should retain executable bytecode"))));
								ASSERT_THAT(AreEqual(
									CaptureOffBytecode,
									CaptureOnBytecode,
									*Case.Describe(TEXT("HIR capture must not change any for-clause bytecode word"))));

								asCScriptFunction* const CaptureOffInternalFunction =
									static_cast<asCScriptFunction*>(CaptureOffEntry);
								asCScriptFunction* const CaptureOnInternalFunction =
									static_cast<asCScriptFunction*>(CaptureOnEntry);
								ASSERT_THAT(IsNull(
									CaptureOffInternalFunction->GetTypedSemanticFunction(),
									*Case.Describe(TEXT("capture-off for clause entry must not publish HIR"))));
								const asCTypedSemanticFunction* const HIR =
									CaptureOnInternalFunction->GetTypedSemanticFunction();
								ASSERT_THAT(IsNotNull(
									HIR,
									*FString::Printf(
										TEXT("%s capture-on for clause entry must publish HIR: %s"),
										*Case.GetId(),
										UTF8_TO_TCHAR(CaptureOnInternalFunction
											->GetTypedSemanticCaptureDiagnostic().AddressOf()))));
								if (HIR != nullptr)
								{
									ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
									const asSTypedSemanticStatement* For = nullptr;
									for (asUINT StatementIndex = 0;
										StatementIndex < HIR->statements.GetLength();
										++StatementIndex)
									{
										const asSTypedSemanticStatement& Statement =
											HIR->statements[StatementIndex];
										if (Statement.kind == asETypedSemanticStatementKind::For)
										{
											For = &Statement;
											break;
										}
									}
									ASSERT_THAT(IsNotNull(For));
									if (For != nullptr)
									{
										ASSERT_THAT(AreEqual(
											InitCase.bPresent,
											For->initializerStatement.IsValid()));
										ASSERT_THAT(AreEqual(
											ConditionCase.bPresent,
											For->conditionExpression.IsValid()));
										ASSERT_THAT(AreEqual(
											IncrementCase.bPresent,
											For->incrementStatement.IsValid()));
									}
								}

								asIScriptEngine* const Engines[] = {
									CaptureOffScriptEngine,
									CaptureOnScriptEngine,
								};
								asIScriptFunction* const Entries[] = {
									CaptureOffEntry,
									CaptureOnEntry,
								};
								int32 Results[2] = {0, 0};
								for (int32 RunIndex = 0; RunIndex < 2; ++RunIndex)
								{
									asIScriptContext* const Context = Engines[RunIndex]->CreateContext();
									ASSERT_THAT(IsNotNull(Context,
										*Case.Describe(TEXT("for clause source should create both execution contexts"))));
									if (Context != nullptr)
									{
										const int ExecuteResult = PrepareAndExecute(Context, Entries[RunIndex]);
										ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
											*Case.Describe(TEXT("for clause execution should finish without an exception"))));
										Results[RunIndex] = static_cast<int32>(Context->GetReturnDWord());
										Context->Release();
									}
								}
								const int32 ExpectedMarker = Expected.InitCount * 1000
									+ Expected.BodyCount * 100
									+ Expected.ConditionCount * 10
									+ Expected.IncrementCount;
								ASSERT_THAT(AreEqual(ExpectedMarker, Results[0],
									*Case.Describe(TEXT("for clause event counters should preserve exact initialization, body, condition, and increment behavior"))));
								ASSERT_THAT(AreEqual(Results[0], Results[1],
									*Case.Describe(TEXT("HIR capture must preserve the exact for-clause VM result"))));
							}
						}

						CaptureOffScriptEngine->DiscardModule(ModuleNameUtf8.Get());
						CaptureOnScriptEngine->DiscardModule(ModuleNameUtf8.Get());
						ASSERT_THAT(IsNull(CaptureOffScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
							*Case.Describe(TEXT("capture-off for clause module should be discarded after the selected cell"))));
						ASSERT_THAT(IsNull(CaptureOnScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
							*Case.Describe(TEXT("capture-on for clause module should be discarded after the selected cell"))));
					}
				}
			}
		}

		ASSERT_THAT(AreEqual(UE_ARRAY_COUNT(PresenceCases) * UE_ARRAY_COUNT(PresenceCases)
			* UE_ARRAY_COUNT(PresenceCases) * UE_ARRAY_COUNT(CountCases),
			ObservedCaseCount,
			TEXT("LANG-CF-FOR-CLAUSES must execute every clause-presence and count cell")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
