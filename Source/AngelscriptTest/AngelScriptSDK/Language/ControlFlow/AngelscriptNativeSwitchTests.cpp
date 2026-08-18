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

TEST_CLASS_WITH_FLAGS(FSwitchTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.ControlFlow.Switch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FNamedCase
	{
		const ANSICHAR* CatalogName;
		bool bInvalid = false;
	};

	struct FSwitchExpectation
	{
		int32 ReturnValue = 0;
		bool bThrows = false;
	};

	inline static constexpr FNamedCase SelectorCases[] =
	{
		{ "int8" }, { "int16" }, { "int" }, { "int64" }, { "uint8" }, { "uint16" }, { "uint" }, { "uint64" }, { "enum" }, { "typedef" }, { "boundary" }, { "unsupported", true },
	};
	inline static constexpr FNamedCase CaseCases[] =
	{
		{ "first" }, { "middle" }, { "last" }, { "default" }, { "no_match" }, { "fallthrough" }, { "grouped" }, { "duplicate", true }, { "non_constant", true },
	};
	inline static constexpr FNamedCase ExitCases[] =
	{
		{ "break" }, { "fallthrough" }, { "return" }, { "exception" },
	};


	static bool IsCase(const FNamedCase& Case, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Case.CatalogName, Name) == 0;
	}

	static int32 SelectorValueFor(const FNamedCase& CaseCase)
	{
		if (IsCase(CaseCase, "first") || IsCase(CaseCase, "fallthrough"))
		{
			return 1;
		}
		if (IsCase(CaseCase, "middle") || IsCase(CaseCase, "grouped"))
		{
			return 2;
		}
		if (IsCase(CaseCase, "last"))
		{
			return 3;
		}
		if (IsCase(CaseCase, "default"))
		{
			return 4;
		}
		return 5;
	}

	static int32 TraceSeedFor(const FNamedCase& CaseCase)
	{
		if (IsCase(CaseCase, "first"))
		{
			return 11;
		}
		if (IsCase(CaseCase, "middle"))
		{
			return 12;
		}
		if (IsCase(CaseCase, "last"))
		{
			return 13;
		}
		if (IsCase(CaseCase, "default"))
		{
			return 14;
		}
		if (IsCase(CaseCase, "no_match"))
		{
			return 15;
		}
		if (IsCase(CaseCase, "fallthrough"))
		{
			return 16;
		}
		return 17;
	}

	static FString TypedLiteral(const FNamedCase& SelectorCase, const int32 Value)
	{
		if (IsCase(SelectorCase, "enum"))
		{
			switch (Value)
			{
			case 0: return TEXT("ESelector::Zero");
			case 1: return TEXT("ESelector::One");
			case 2: return TEXT("ESelector::Two");
			case 3: return TEXT("ESelector::Three");
			case 4: return TEXT("ESelector::Four");
			case 5: return TEXT("ESelector::Five");
			default: return TEXT("ESelector::Six");
			}
		}
		if (IsCase(SelectorCase, "boundary"))
		{
			return FString::FromInt(2147483641 + Value);
		}
		if (IsCase(SelectorCase, "typedef"))
		{
			return FString::Printf(TEXT("SelectorAlias(%d)"), Value);
		}
		if (!IsCase(SelectorCase, "unsupported"))
		{
			return FString::Printf(TEXT("%hs(%d)"), SelectorCase.CatalogName, Value);
		}
		return FString::FromInt(Value);
	}

	static void AppendSelectorDeclaration(FString& Source, const FNamedCase& SelectorCase, const FNamedCase& CaseCase)
	{
		if (IsCase(SelectorCase, "enum"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tESelector Value = %s;"), *TypedLiteral(SelectorCase, SelectorValueFor(CaseCase))));
		}
		else if (IsCase(SelectorCase, "typedef"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tSelectorAlias Value = SelectorAlias(%d);"), SelectorValueFor(CaseCase)));
		}
		else if (IsCase(SelectorCase, "boundary"))
		{
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\tint Value = %s;"),
					*TypedLiteral(SelectorCase, SelectorValueFor(CaseCase))));
		}
		else if (IsCase(SelectorCase, "unsupported"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFUnsupported Value;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%hs Value = %hs(%d);"),
				SelectorCase.CatalogName,
				SelectorCase.CatalogName,
				SelectorValueFor(CaseCase)));
		}
	}

	static FString CaseLabel(const FNamedCase& SelectorCase, const int32 Value)
	{
		return FString::Printf(TEXT("\tcase %s:"), *TypedLiteral(SelectorCase, Value));
	}

	static void AppendSelectedExit(
		FString& Source,
		const FNamedCase& SelectorCase,
		const FNamedCase& ExitCase,
		const FString& Indent)
	{
		if (IsCase(ExitCase, "break"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("break;"));
		}
		else if (IsCase(ExitCase, "fallthrough"))
		{
			AppendGeneratedAsLine(Source, Indent.LeftChop(1) + FString::Printf(TEXT("case %s:"), *TypedLiteral(SelectorCase, 6)));
			AppendGeneratedAsLine(Source, Indent + TEXT("Trace += 100;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("break;"));
		}
		else if (IsCase(ExitCase, "return"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("return Trace + 200;"));
		}
		else
		{
			// The current fork does not recognize string literals in throw(); use a
			// runtime divide-by-zero fault so the switch exit remains executable.
			AppendGeneratedAsLine(Source, Indent + TEXT("{"));
			AppendGeneratedAsLine(Source, Indent + TEXT("\tint ExceptionDivisor = 0;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("\tTrace += 1 / ExceptionDivisor;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("}"));
			AppendGeneratedAsLine(Source, Indent + TEXT("break;"));
		}
	}

	static void AppendSelectedCase(FString& Source, const FNamedCase& SelectorCase, const FNamedCase& CaseCase, const FNamedCase& ExitCase)
	{
		const int32 Seed = TraceSeedFor(CaseCase);
		if (IsCase(CaseCase, "duplicate"))
		{
			AppendGeneratedAsLine(Source, CaseLabel(SelectorCase, 1));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\tTrace += %d;"), Seed));
			AppendSelectedExit(Source, SelectorCase, ExitCase, TEXT("\t\t"));
			AppendGeneratedAsLine(Source, CaseLabel(SelectorCase, 1));
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 1000;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
			return;
		}
		if (IsCase(CaseCase, "non_constant"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tcase DynamicCase:"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\tTrace += %d;"), Seed));
			AppendSelectedExit(Source, SelectorCase, ExitCase, TEXT("\t\t"));
			return;
		}
		if (IsCase(CaseCase, "default") || IsCase(CaseCase, "no_match"))
		{
			AppendGeneratedAsLine(Source, CaseLabel(SelectorCase, 0));
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 1000;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
			AppendGeneratedAsLine(Source, TEXT("\tdefault:"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\tTrace += %d;"), Seed));
			AppendSelectedExit(Source, SelectorCase, ExitCase, TEXT("\t\t"));
			return;
		}
		if (IsCase(CaseCase, "grouped"))
		{
			AppendGeneratedAsLine(Source, CaseLabel(SelectorCase, 1));
			AppendGeneratedAsLine(Source, CaseLabel(SelectorCase, 2));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\tTrace += %d;"), Seed));
			AppendSelectedExit(Source, SelectorCase, ExitCase, TEXT("\t\t"));
			return;
		}
		if (IsCase(CaseCase, "fallthrough"))
		{
			AppendGeneratedAsLine(Source, CaseLabel(SelectorCase, 1));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\tTrace += %d;"), Seed));
			AppendGeneratedAsLine(Source, CaseLabel(SelectorCase, 2));
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 1;"));
			AppendSelectedExit(Source, SelectorCase, ExitCase, TEXT("\t\t"));
			return;
		}

		AppendGeneratedAsLine(Source, CaseLabel(SelectorCase, SelectorValueFor(CaseCase)));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\tTrace += %d;"), Seed));
		AppendSelectedExit(Source, SelectorCase, ExitCase, TEXT("\t\t"));
	}

	static void AppendBoundaryControl(FString& Source)
	{
		AppendGeneratedAsLine(Source, TEXT("int BoundaryControl()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Trace = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Lower = 2147483642;"));
		AppendGeneratedAsLine(Source, TEXT("\tswitch (Lower)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\tcase 2147483642:"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 1;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		AppendGeneratedAsLine(Source, TEXT("\tdefault:"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 100;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\tint Trigger = 2147483643;"));
		AppendGeneratedAsLine(Source, TEXT("\tswitch (Trigger)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\tcase 2147483643:"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 2;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		AppendGeneratedAsLine(Source, TEXT("\tdefault:"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 200;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\tint Dense = 2147483647;"));
		AppendGeneratedAsLine(Source, TEXT("\tswitch (Dense)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\tcase 2147483645:"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 400;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		AppendGeneratedAsLine(Source, TEXT("\tcase 2147483646:"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 500;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		AppendGeneratedAsLine(Source, TEXT("\tcase 2147483647:"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 3;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		AppendGeneratedAsLine(Source, TEXT("\tdefault:"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 800;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Trace;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString BuildSwitchSource(
		const FNamedCase& SelectorCase,
		const FNamedCase& CaseCase,
		const FNamedCase& ExitCase)
	{
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("enum ESelector"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tZero = 0,"));
		AppendGeneratedAsLine(Source, TEXT("\tOne = 1,"));
		AppendGeneratedAsLine(Source, TEXT("\tTwo = 2,"));
		AppendGeneratedAsLine(Source, TEXT("\tThree = 3,"));
		AppendGeneratedAsLine(Source, TEXT("\tFour = 4,"));
		AppendGeneratedAsLine(Source, TEXT("\tFive = 5,"));
		AppendGeneratedAsLine(Source, TEXT("\tSix = 6"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("// SelectorAlias is registered through the raw SDK before each module build."));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class FUnsupported"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		if (IsCase(SelectorCase, "boundary"))
		{
			AppendBoundaryControl(Source);
		}
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, IsCase(SelectorCase, "boundary")
			? TEXT("\tint Trace = BoundaryControl();")
			: TEXT("\tint Trace = 0;"));
		AppendSelectorDeclaration(Source, SelectorCase, CaseCase);
		if (IsCase(CaseCase, "non_constant"))
		{
			if (IsCase(SelectorCase, "enum"))
			{
				AppendGeneratedAsLine(Source, TEXT("\tESelector DynamicCase = ESelector::One;"));
			}
			else
			{
				AppendGeneratedAsLine(Source, TEXT("\tint DynamicCase = 1;"));
			}
		}
		AppendGeneratedAsLine(Source, TEXT("\tswitch (Value)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendSelectedCase(Source, SelectorCase, CaseCase, ExitCase);
		if (!IsCase(CaseCase, "default") && !IsCase(CaseCase, "no_match"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tdefault:"));
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 5000;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		}
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Trace;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FSwitchExpectation ExpectedResult(const FNamedCase& SelectorCase, const FNamedCase& CaseCase, const FNamedCase& ExitCase)
	{
		FSwitchExpectation Result;
		Result.ReturnValue = TraceSeedFor(CaseCase) + (IsCase(CaseCase, "fallthrough") ? 1 : 0);
		if (IsCase(SelectorCase, "boundary"))
		{
			Result.ReturnValue += 6;
		}
		if (IsCase(ExitCase, "exception"))
		{
			Result.bThrows = true;
		}
		else if (IsCase(ExitCase, "fallthrough"))
		{
			Result.ReturnValue += 100;
		}
		else if (IsCase(ExitCase, "return"))
		{
			Result.ReturnValue += 200;
		}
		return Result;
	}

	static FString BuildExhaustiveEnumSource()
	{
		return TEXT(R"AS(
			enum EExhaustiveSelector
			{
				Zero = 0,
				One = 1,
				Two = 2
			}

			int ExhaustiveEnumEntry(EExhaustiveSelector Value)
			{
				switch (Value)
				{
				case EExhaustiveSelector::Zero:
					return 10;
				case EExhaustiveSelector::One:
					return 20;
				case EExhaustiveSelector::Two:
					return 30;
				}
			}
		)AS");
	}

public:
	TEST_METHOD(SelectorsByCaseAndExit)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CF-SWITCH",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Bytecode);

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
			TEXT("Switch product should create its capture-off raw SDK engine")));
		ASSERT_THAT(IsNotNull(CaptureOnScriptEngine,
			TEXT("Switch product should create its capture-on raw SDK engine")));
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
		const int CaptureOffAliasResult =
			CaptureOffScriptEngine->RegisterTypedef("SelectorAlias", "int");
		const int CaptureOnAliasResult =
			CaptureOnScriptEngine->RegisterTypedef("SelectorAlias", "int");
		ASSERT_THAT(AreEqual(CaptureOffAliasResult, CaptureOnAliasResult));
		ASSERT_THAT(IsTrue(CaptureOffAliasResult >= 0,
			*FString::Printf(TEXT("Switch product should register its raw selector alias. Result=%d OffMessages={%s} OnMessages={%s}"),
				CaptureOffAliasResult,
				*CaptureOffEngine.GetMessagesText(),
				*CaptureOnEngine.GetMessagesText())));

		{
			const FString ModuleName = TEXT("SwitchExhaustiveEnumOracle");
			const FString Source = BuildExhaustiveEnumSource();
			const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
			const FTCHARToUTF8 SourceUtf8(*Source);
			CaptureOffEngine.Reset(*TestRunner);
			CaptureOnEngine.Reset(*TestRunner);
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
			ASSERT_THAT(AreEqual(CaptureOffBuildResult, CaptureOnBuildResult));
			ASSERT_THAT(AreEqual(CaptureOffDiagnostics, CaptureOnDiagnostics));
			ASSERT_THAT(AreEqual(asSUCCESS, CaptureOffBuildResult));
			ASSERT_THAT(IsNotNull(CaptureOffModule));
			ASSERT_THAT(IsNotNull(CaptureOnModule));
			if (CaptureOffModule != nullptr && CaptureOnModule != nullptr)
			{
				asIScriptFunction* const CaptureOffEntry =
					GetNativeFunctionByExactDecl(
						CaptureOffModule,
						"int ExhaustiveEnumEntry(EExhaustiveSelector Value)");
				asIScriptFunction* const CaptureOnEntry =
					GetNativeFunctionByExactDecl(
						CaptureOnModule,
						"int ExhaustiveEnumEntry(EExhaustiveSelector Value)");
				ASSERT_THAT(IsNotNull(CaptureOffEntry));
				ASSERT_THAT(IsNotNull(CaptureOnEntry));
				if (CaptureOffEntry != nullptr && CaptureOnEntry != nullptr)
				{
					ASSERT_THAT(AreEqual(
						CopyNativeBytecode(CaptureOffEntry),
						CopyNativeBytecode(CaptureOnEntry),
						TEXT("Exhaustive-enum HIR capture must not change bytecode")));
					asCScriptFunction* const CaptureOffInternalFunction =
						static_cast<asCScriptFunction*>(CaptureOffEntry);
					asCScriptFunction* const CaptureOnInternalFunction =
						static_cast<asCScriptFunction*>(CaptureOnEntry);
					ASSERT_THAT(IsNull(
						CaptureOffInternalFunction->GetTypedSemanticFunction()));
					const asCTypedSemanticFunction* const HIR =
						CaptureOnInternalFunction->GetTypedSemanticFunction();
					ASSERT_THAT(IsNotNull(
						HIR,
						*FString::Printf(
							TEXT("Exhaustive enum must publish HIR: %s"),
							UTF8_TO_TCHAR(CaptureOnInternalFunction
								->GetTypedSemanticCaptureDiagnostic().AddressOf()))));
					bool bFoundInvalidEnumEdge = false;
					if (HIR != nullptr)
					{
						ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
						for (asUINT StatementIndex = 0;
							StatementIndex < HIR->statements.GetLength();
							++StatementIndex)
						{
							const asSTypedSemanticStatement& Statement =
								HIR->statements[StatementIndex];
							if (Statement.kind == asETypedSemanticStatementKind::Switch
								&& Statement.switchIsExhaustive
								&& Statement.switchHasInvalidEnumValueEdge
								&& Statement.safePointRole
									== asETypedSemanticSafePointRole::SwitchInvalidValue)
							{
								bFoundInvalidEnumEdge = true;
							}
						}
					}
					ASSERT_THAT(IsTrue(bFoundInvalidEnumEdge));

					asIScriptEngine* const Engines[] = {
						CaptureOffScriptEngine,
						CaptureOnScriptEngine,
					};
					asIScriptFunction* const Entries[] = {
						CaptureOffEntry,
						CaptureOnEntry,
					};
					FString Exceptions[2];
					for (int32 RunIndex = 0; RunIndex < 2; ++RunIndex)
					{
						asIScriptContext* const Context = Engines[RunIndex]->CreateContext();
						ASSERT_THAT(IsNotNull(Context));
						if (Context != nullptr)
						{
							ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Entries[RunIndex])));
							ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(0, 99)));
							ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
							Exceptions[RunIndex] = UTF8_TO_TCHAR(Context->GetExceptionString());
							Context->Release();
						}
					}
					ASSERT_THAT(AreEqual(
						FString(TEXT("Invalid enum value passed to switch")),
						Exceptions[0]));
					ASSERT_THAT(AreEqual(Exceptions[0], Exceptions[1]));
				}
			}
			CaptureOffScriptEngine->DiscardModule(ModuleNameUtf8.Get());
			CaptureOnScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		}

		int32 ObservedCaseCount = 0;
		for (const FNamedCase& SelectorCase : SelectorCases)
		{
			for (const FNamedCase& CaseCase : CaseCases)
			{
				for (const FNamedCase& ExitCase : ExitCases)
				{
					++ObservedCaseCount;
					const FNativeCaseContext Case(MakeNativeCaseId("LANG-CF-SWITCH",
						{ ANSI_TO_TCHAR(SelectorCase.CatalogName), ANSI_TO_TCHAR(CaseCase.CatalogName), ANSI_TO_TCHAR(ExitCase.CatalogName) }));
					const FString ModuleName = TEXT("Switch_") + Case.GetId().RightChop(15).Replace(TEXT("-"), TEXT("_"));
					const FString Source = BuildSwitchSource(SelectorCase, CaseCase, ExitCase);
					PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);
					CaptureOffEngine.ResetMessages();
					CaptureOnEngine.ResetMessages();
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
						*Case.Describe(TEXT("HIR capture must preserve the switch build outcome"))));
					ASSERT_THAT(AreEqual(
						CaptureOffDiagnostics,
						CaptureOnDiagnostics,
						*Case.Describe(TEXT("HIR capture must preserve every switch diagnostic"))));
					// The current fork requires default to be the final case label. A
					// default/no-match fallthrough combination therefore remains an
					// intentional negative product instead of being silently omitted.
					const bool bInvalid = SelectorCase.bInvalid
						|| CaseCase.bInvalid
						|| ((IsCase(CaseCase, "default") || IsCase(CaseCase, "no_match"))
							&& IsCase(ExitCase, "fallthrough"));
					if (bInvalid)
					{
						ASSERT_THAT(IsTrue(CaptureOffBuildResult < 0, *Case.Describe(TEXT("invalid switch cell should be rejected"))));
						ASSERT_THAT(IsTrue(CaptureOffEngine.GetMessages().Entries.Num() > 0, *Case.Describe(TEXT("invalid switch cell should report a diagnostic"))));
					}
					else
					{
						const FSwitchExpectation Expected = ExpectedResult(SelectorCase, CaseCase, ExitCase);
						ASSERT_THAT(AreEqual(asSUCCESS, CaptureOffBuildResult,
							*FString::Printf(TEXT("%s switch cell should compile. BuildResult=%d Messages={%s}"),
								*Case.GetId(), CaptureOffBuildResult, *CaptureOffDiagnostics)));
						ASSERT_THAT(IsNotNull(CaptureOffModule));
						ASSERT_THAT(IsNotNull(CaptureOnModule));
						asIScriptFunction* const CaptureOffEntry =
							GetNativeFunctionByExactDecl(CaptureOffModule, "int Entry()");
						asIScriptFunction* const CaptureOnEntry =
							GetNativeFunctionByExactDecl(CaptureOnModule, "int Entry()");
						ASSERT_THAT(IsNotNull(CaptureOffEntry, *Case.Describe(TEXT("capture-off switch cell should publish exact Entry declaration"))));
						ASSERT_THAT(IsNotNull(CaptureOnEntry, *Case.Describe(TEXT("capture-on switch cell should publish exact Entry declaration"))));
						if (CaptureOffEntry != nullptr && CaptureOnEntry != nullptr)
						{
							const TArray<asDWORD> CaptureOffBytecode =
								CopyNativeBytecode(CaptureOffEntry);
							const TArray<asDWORD> CaptureOnBytecode =
								CopyNativeBytecode(CaptureOnEntry);
							ASSERT_THAT(IsTrue(CaptureOffBytecode.Num() > 0,
								*Case.Describe(TEXT("switch entry should expose bytecode"))));
							ASSERT_THAT(AreEqual(
								CaptureOffBytecode,
								CaptureOnBytecode,
								*Case.Describe(TEXT("HIR capture must not change any switch bytecode word"))));

							asCScriptFunction* const CaptureOffInternalFunction =
								static_cast<asCScriptFunction*>(CaptureOffEntry);
							asCScriptFunction* const CaptureOnInternalFunction =
								static_cast<asCScriptFunction*>(CaptureOnEntry);
							ASSERT_THAT(IsNull(
								CaptureOffInternalFunction->GetTypedSemanticFunction(),
								*Case.Describe(TEXT("capture-off switch entry must not publish HIR"))));
							const asCTypedSemanticFunction* const HIR =
								CaptureOnInternalFunction->GetTypedSemanticFunction();
							ASSERT_THAT(IsNotNull(
								HIR,
								*FString::Printf(
									TEXT("%s capture-on switch entry must publish HIR: %s"),
									*Case.GetId(),
									UTF8_TO_TCHAR(CaptureOnInternalFunction
										->GetTypedSemanticCaptureDiagnostic().AddressOf()))));
							if (HIR != nullptr)
							{
								ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
								int32 SwitchCount = 0;
								int32 FallthroughCount = 0;
								for (asUINT StatementIndex = 0;
									StatementIndex < HIR->statements.GetLength();
									++StatementIndex)
								{
									const asSTypedSemanticStatement& Statement =
										HIR->statements[StatementIndex];
									if (Statement.kind == asETypedSemanticStatementKind::Switch)
									{
										++SwitchCount;
										ASSERT_THAT(IsTrue(Statement.children.GetLength() > 0));
										ASSERT_THAT(IsTrue(
											Statement.switchNormalization
												!= asETypedSemanticSwitchNormalization::None));
									}
									else if (Statement.kind == asETypedSemanticStatementKind::Case
										&& Statement.fallsThrough)
									{
										++FallthroughCount;
									}
								}
								ASSERT_THAT(AreEqual(1, SwitchCount));
								const bool bExpectedFallthrough =
									IsCase(CaseCase, "fallthrough")
									|| IsCase(CaseCase, "grouped")
									|| IsCase(ExitCase, "fallthrough");
								ASSERT_THAT(AreEqual(
									bExpectedFallthrough,
									FallthroughCount > 0));
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
							FString Exceptions[2];
							for (int32 RunIndex = 0; RunIndex < 2; ++RunIndex)
							{
								asIScriptContext* const Context = Engines[RunIndex]->CreateContext();
								ASSERT_THAT(IsNotNull(Context, *Case.Describe(TEXT("switch cell should create both contexts"))));
								if (Context != nullptr)
								{
									ASSERT_THAT(AreEqual(
										Expected.bThrows ? asEXECUTION_EXCEPTION : asEXECUTION_FINISHED,
										PrepareAndExecute(Context, Entries[RunIndex]),
										*Case.Describe(TEXT("switch execution should match selected exit"))));
									if (Expected.bThrows)
									{
										Exceptions[RunIndex] = UTF8_TO_TCHAR(Context->GetExceptionString());
									}
									else
									{
										Results[RunIndex] = static_cast<int32>(Context->GetReturnDWord());
									}
									Context->Release();
								}
							}
							if (Expected.bThrows)
							{
								ASSERT_THAT(AreEqual(
									FString(TEXT("Divide by zero")),
									Exceptions[0],
									*Case.Describe(TEXT("switch exception path should preserve the fork's exact divide-by-zero text"))));
								ASSERT_THAT(AreEqual(Exceptions[0], Exceptions[1],
									*Case.Describe(TEXT("HIR capture must preserve the exact switch exception"))));
							}
							else
							{
								ASSERT_THAT(AreEqual(Expected.ReturnValue, Results[0],
									*FString::Printf(TEXT("%s switch should return the exact selected trace. Expected=%d Actual=%d"),
										*Case.GetId(), Expected.ReturnValue, Results[0])));
								ASSERT_THAT(AreEqual(Results[0], Results[1],
									*Case.Describe(TEXT("HIR capture must preserve the exact switch VM result"))));
							}
						}
					}
					CaptureOffScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					CaptureOnScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(CaptureOffScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS), *Case.Describe(TEXT("capture-off switch cell should discard its module"))));
					ASSERT_THAT(IsNull(CaptureOnScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS), *Case.Describe(TEXT("capture-on switch cell should discard its module"))));
				}
			}
		}
		ASSERT_THAT(AreEqual(
			UE_ARRAY_COUNT(SelectorCases) * UE_ARRAY_COUNT(CaseCases)
				* UE_ARRAY_COUNT(ExitCases),
			ObservedCaseCount,
			TEXT("LANG-CF-SWITCH must execute every selector, case-shape, and exit cell")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
