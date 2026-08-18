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

TEST_CLASS_WITH_FLAGS(FNestedTargetTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.ControlFlow.NestedTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FNamedCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FNamedCase NestingCases[] =
	{
		{ "nested_loop" },
		{ "branch_loop" },
		{ "three_level" },
	};

	inline static constexpr FNamedCase TransferCases[] =
	{
		{ "break" },
		{ "continue" },
		{ "return" },
	};

	inline static constexpr FNamedCase TargetCases[] =
	{
		{ "inner" },
		{ "outer" },
	};


	static bool IsCase(const FNamedCase& Case, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Case.CatalogName, Name) == 0;
	}

	static void AppendTransfer(
		FString& Source,
		const FNamedCase& TransferCase,
		const FNamedCase& TargetCase,
		const FString& Indent)
	{
		if (!IsCase(TargetCase, "inner"))
		{
			return;
		}

		if (IsCase(TransferCase, "break"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("break;"));
		}
		else if (IsCase(TransferCase, "continue"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("continue;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("return Trace;"));
		}
	}

	static void AppendOuterTransfer(
		FString& Source,
		const FNamedCase& TransferCase,
		const FNamedCase& TargetCase)
	{
		if (!IsCase(TargetCase, "outer"))
		{
			return;
		}

		if (IsCase(TransferCase, "break"))
		{
			AppendGeneratedAsLine(Source, TEXT("		break;"));
		}
		else if (IsCase(TransferCase, "continue"))
		{
			AppendGeneratedAsLine(Source, TEXT("		continue;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("		return Trace;"));
		}
	}

	static FString BuildNestedSource(
		const FNamedCase& NestingCase,
		const FNamedCase& TransferCase,
		const FNamedCase& TargetCase)
	{
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("	int Trace = 0;"));
		AppendGeneratedAsLine(Source, TEXT("	for (int Outer = 0; Outer < 2; ++Outer)"));
		AppendGeneratedAsLine(Source, TEXT("	{"));
		AppendGeneratedAsLine(Source, TEXT("		Trace += 1;"));

		if (IsCase(NestingCase, "nested_loop"))
		{
			AppendGeneratedAsLine(Source, TEXT("		for (int Inner = 0; Inner < 2; ++Inner)"));
			AppendGeneratedAsLine(Source, TEXT("		{"));
			AppendGeneratedAsLine(Source, TEXT("			Trace += 10;"));
			AppendTransfer(Source, TransferCase, TargetCase, TEXT("			"));
			AppendGeneratedAsLine(Source, TEXT("		}"));
		}
		else if (IsCase(NestingCase, "branch_loop"))
		{
			AppendGeneratedAsLine(Source, TEXT("		if (Outer >= 0)"));
			AppendGeneratedAsLine(Source, TEXT("		{"));
			AppendGeneratedAsLine(Source, TEXT("			Trace += 2;"));
			AppendGeneratedAsLine(Source, TEXT("			for (int Inner = 0; Inner < 2; ++Inner)"));
			AppendGeneratedAsLine(Source, TEXT("			{"));
			AppendGeneratedAsLine(Source, TEXT("				Trace += 10;"));
			AppendTransfer(Source, TransferCase, TargetCase, TEXT("				"));
			AppendGeneratedAsLine(Source, TEXT("			}"));
			AppendGeneratedAsLine(Source, TEXT("		}"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("		if (Outer >= 0)"));
			AppendGeneratedAsLine(Source, TEXT("		{"));
			AppendGeneratedAsLine(Source, TEXT("			for (int Middle = 0; Middle < 2; ++Middle)"));
			AppendGeneratedAsLine(Source, TEXT("			{"));
			AppendGeneratedAsLine(Source, TEXT("				Trace += 10;"));
			AppendGeneratedAsLine(Source, TEXT("				for (int Inner = 0; Inner < 2; ++Inner)"));
			AppendGeneratedAsLine(Source, TEXT("				{"));
			AppendGeneratedAsLine(Source, TEXT("					Trace += 100;"));
			AppendTransfer(Source, TransferCase, TargetCase, TEXT("					"));
			AppendGeneratedAsLine(Source, TEXT("				}"));
			AppendGeneratedAsLine(Source, TEXT("			}"));
			AppendGeneratedAsLine(Source, TEXT("		}"));
		}

		AppendGeneratedAsLine(Source, TEXT("		Trace += 1000;"));
		AppendOuterTransfer(Source, TransferCase, TargetCase);
		AppendGeneratedAsLine(Source, TEXT("		Trace += 10000;"));
		AppendGeneratedAsLine(Source, TEXT("	}"));
		AppendGeneratedAsLine(Source, TEXT("	return Trace;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static int32 SimulateExpectedTrace(
		const FNamedCase& NestingCase,
		const FNamedCase& TransferCase,
		const FNamedCase& TargetCase)
	{
		int32 Trace = 0;
		for (int32 Outer = 0; Outer < 2; ++Outer)
		{
			Trace += 1;
			if (IsCase(NestingCase, "nested_loop"))
			{
				for (int32 Inner = 0; Inner < 2; ++Inner)
				{
					Trace += 10;
					if (IsCase(TargetCase, "inner"))
					{
						if (IsCase(TransferCase, "break"))
						{
							break;
						}
						if (IsCase(TransferCase, "continue"))
						{
							continue;
						}
						return Trace;
					}
				}
			}
			else if (IsCase(NestingCase, "branch_loop"))
			{
				Trace += 2;
				for (int32 Inner = 0; Inner < 2; ++Inner)
				{
					Trace += 10;
					if (IsCase(TargetCase, "inner"))
					{
						if (IsCase(TransferCase, "break"))
						{
							break;
						}
						if (IsCase(TransferCase, "continue"))
						{
							continue;
						}
						return Trace;
					}
				}
			}
			else
			{
				for (int32 Middle = 0; Middle < 2; ++Middle)
				{
					Trace += 10;
					for (int32 Inner = 0; Inner < 2; ++Inner)
					{
						Trace += 100;
						if (IsCase(TargetCase, "inner"))
						{
							if (IsCase(TransferCase, "break"))
							{
								break;
							}
							if (IsCase(TransferCase, "continue"))
							{
								continue;
							}
							return Trace;
						}
					}
				}
			}

			Trace += 1000;
			if (IsCase(TargetCase, "outer"))
			{
				if (IsCase(TransferCase, "break"))
				{
					break;
				}
				if (IsCase(TransferCase, "continue"))
				{
					continue;
				}
				return Trace;
			}
			Trace += 10000;
		}
		return Trace;
	}

public:
	TEST_METHOD(TransfersByNestingAndTarget)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CF-NESTED-TARGETS",
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
			TEXT("Nested-target product should create its capture-off raw SDK engine")));
		ASSERT_THAT(IsNotNull(CaptureOnScriptEngine,
			TEXT("Nested-target product should create its capture-on raw SDK engine")));
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
		for (const FNamedCase& NestingCase : NestingCases)
		{
			for (const FNamedCase& TransferCase : TransferCases)
			{
				for (const FNamedCase& TargetCase : TargetCases)
				{
					++ObservedCaseCount;
					const FNativeCaseContext Case(MakeNativeCaseId(
						"LANG-CF-NESTED-TARGETS",
						{
							ANSI_TO_TCHAR(NestingCase.CatalogName),
							ANSI_TO_TCHAR(TransferCase.CatalogName),
							ANSI_TO_TCHAR(TargetCase.CatalogName),
						}));
					const FString ModuleName = FString::Printf(
						TEXT("NestedTargets_%s_%s_%s"),
						ANSI_TO_TCHAR(NestingCase.CatalogName),
						ANSI_TO_TCHAR(TransferCase.CatalogName),
						ANSI_TO_TCHAR(TargetCase.CatalogName));
					const FString Source = BuildNestedSource(NestingCase, TransferCase, TargetCase);
					const int32 ExpectedTrace = SimulateExpectedTrace(NestingCase, TransferCase, TargetCase);
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
						*Case.Describe(TEXT("HIR capture must preserve the nested-target build outcome"))));
					ASSERT_THAT(AreEqual(
						CaptureOffDiagnostics,
						CaptureOnDiagnostics,
						*Case.Describe(TEXT("HIR capture must preserve every nested-target diagnostic"))));
					ASSERT_THAT(AreEqual(asSUCCESS, CaptureOffBuildResult,
						*Case.Describe(TEXT("nested transfer source should compile for the selected shape, transfer, and target"))));
					ASSERT_THAT(IsNotNull(CaptureOffModule,
						*Case.Describe(TEXT("capture-off nested transfer source should publish a module"))));
					ASSERT_THAT(IsNotNull(CaptureOnModule,
						*Case.Describe(TEXT("capture-on nested transfer source should publish a module"))));

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
							*Case.Describe(TEXT("capture-off nested transfer source should publish the exact Entry declaration"))));
						ASSERT_THAT(IsNotNull(CaptureOnEntry,
							*Case.Describe(TEXT("capture-on nested transfer source should publish the exact Entry declaration"))));
						if (CaptureOffEntry != nullptr && CaptureOnEntry != nullptr)
						{
							const TArray<asDWORD> CaptureOffBytecode =
								CopyNativeBytecode(CaptureOffEntry);
							const TArray<asDWORD> CaptureOnBytecode =
								CopyNativeBytecode(CaptureOnEntry);
							ASSERT_THAT(IsTrue(CaptureOffBytecode.Num() > 0,
								*Case.Describe(TEXT("nested transfer entry should retain executable bytecode"))));
							ASSERT_THAT(AreEqual(
								CaptureOffBytecode,
								CaptureOnBytecode,
								*Case.Describe(TEXT("HIR capture must not change any nested-target bytecode word"))));

							asCScriptFunction* const CaptureOffInternalFunction =
								static_cast<asCScriptFunction*>(CaptureOffEntry);
							asCScriptFunction* const CaptureOnInternalFunction =
								static_cast<asCScriptFunction*>(CaptureOnEntry);
							ASSERT_THAT(IsNull(
								CaptureOffInternalFunction->GetTypedSemanticFunction(),
								*Case.Describe(TEXT("capture-off nested-target entry must not publish HIR"))));
							const asCTypedSemanticFunction* const HIR =
								CaptureOnInternalFunction->GetTypedSemanticFunction();
							ASSERT_THAT(IsNotNull(
								HIR,
								*FString::Printf(
									TEXT("%s capture-on nested-target entry must publish HIR: %s"),
									*Case.GetId(),
									UTF8_TO_TCHAR(CaptureOnInternalFunction
										->GetTypedSemanticCaptureDiagnostic().AddressOf()))));
							if (HIR != nullptr)
							{
								ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
								asTypedSemanticStatementId OutermostFor;
								asTypedSemanticStatementId SelectedTransfer;
								asUINT MaximumReturnExitedScopes = 0;
								for (asUINT StatementIndex = 0;
									StatementIndex < HIR->statements.GetLength();
									++StatementIndex)
								{
									const asSTypedSemanticStatement& Statement =
										HIR->statements[StatementIndex];
									if (Statement.kind == asETypedSemanticStatementKind::For)
									{
										OutermostFor = Statement.id;
									}
									if ((IsCase(TransferCase, "break")
											&& Statement.kind == asETypedSemanticStatementKind::Break)
										|| (IsCase(TransferCase, "continue")
											&& Statement.kind == asETypedSemanticStatementKind::Continue))
									{
										SelectedTransfer = Statement.id;
									}
									if (Statement.kind == asETypedSemanticStatementKind::Return)
									{
										MaximumReturnExitedScopes = FMath::Max(
											MaximumReturnExitedScopes,
											Statement.exitedScopeCount);
									}
								}
								if (IsCase(TransferCase, "return"))
								{
									ASSERT_THAT(IsTrue(MaximumReturnExitedScopes > 1));
								}
								else
								{
									ASSERT_THAT(IsTrue(SelectedTransfer.IsValid()));
									if (SelectedTransfer.IsValid())
									{
										const asSTypedSemanticStatement& Transfer =
											HIR->statements[SelectedTransfer.value];
										ASSERT_THAT(IsTrue(Transfer.targetStatement.IsValid()));
										ASSERT_THAT(AreEqual(
											IsCase(TargetCase, "outer"),
											Transfer.targetStatement == OutermostFor));
									}
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
									*Case.Describe(TEXT("nested transfer source should create both execution contexts"))));
								if (Context != nullptr)
							{
									const int ExecuteResult = PrepareAndExecute(Context, Entries[RunIndex]);
								ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
									*Case.Describe(TEXT("nested transfer execution should finish without an exception"))));
									Results[RunIndex] = static_cast<int32>(Context->GetReturnDWord());
								Context->Release();
							}
							}
							ASSERT_THAT(AreEqual(ExpectedTrace, Results[0],
								*Case.Describe(TEXT("nested transfer should preserve the exact inner or outer target trace"))));
							ASSERT_THAT(AreEqual(Results[0], Results[1],
								*Case.Describe(TEXT("HIR capture must preserve the exact nested-target VM result"))));
						}
					}

					CaptureOffScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					CaptureOnScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(CaptureOffScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("capture-off nested transfer module should be discarded after the selected cell"))));
					ASSERT_THAT(IsNull(CaptureOnScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("capture-on nested transfer module should be discarded after the selected cell"))));
				}
			}
		}

		ASSERT_THAT(AreEqual(UE_ARRAY_COUNT(NestingCases) * UE_ARRAY_COUNT(TransferCases) * UE_ARRAY_COUNT(TargetCases),
			ObservedCaseCount,
			TEXT("LANG-CF-NESTED-TARGETS must execute every nesting, transfer, and target cell")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
