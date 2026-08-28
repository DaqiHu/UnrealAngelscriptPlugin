#include "AngelscriptTestMacros.h"
#include "../../../Support/AngelscriptNativeCanonicalASTTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_bytecode_codegen.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FCanonicalASTVmMatrixTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.Semantics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static void RaiseCanonicalExceptionGeneric(asIScriptGeneric*)
	{
		if (asIScriptContext* const Context = asGetActiveContext())
		{
			Context->SetException("canonical-vm-matrix");
		}
	}

	static bool RegisterTrace(AngelscriptNativeTestSupport::FNativeTestEngine& Engine)
	{
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		return ScriptEngine != nullptr
			&& AngelscriptNativeTestSupport::RegisterCanonicalExecutionTrace(ScriptEngine) >= 0
			&& ScriptEngine->RegisterGlobalFunction(
				"void Boom()",
				asFUNCTION(RaiseCanonicalExceptionGeneric),
				asCALL_GENERIC) >= 0;
	}

public:
	TEST_METHOD(EvaluationOrderAndReverseFormalArguments)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("evaluation-order VM matrix selects CANONICAL")));
		ASSERT_THAT(IsTrue(RegisterTrace(Engine), TEXT("VM matrix needs the execution Trace binding")));

		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"CanonicalASTVmEvalOrder",
			ASTEST_AS_ANSI(R"AS(
				int Mark(int Value)
				{
					Trace(Value);
					return Value;
				}

				int Collect3(int A, int B, int C)
				{
					return A * 100 + B * 10 + C;
				}

				int RunPositional()
				{
					return Collect3(Mark(1), Mark(2), Mark(3));
				}

				int RunNamedAndDefault(int A, int B = 20, int C = 30)
				{
					return A * 100 + B * 10 + C;
				}

				int RunNamed()
				{
					return RunNamedAndDefault(C: Mark(3), A: Mark(1));
				}

				int RunShortCircuit()
				{
					return (Mark(1) == 0 && Mark(9) == 9) ? 1 : 0;
				}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("eval-order script should compile")));
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		FCanonicalExecutionTrace Trace;
		int32 Result = 0;
		{
			FCanonicalTraceScope Scope(Trace);
			ASSERT_THAT(IsTrue(CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int RunPositional()", Result)));
		}
		ASSERT_THAT(AreEqual(123, Result, TEXT("positional values bind A=1 B=2 C=3")));
		ASSERT_THAT(AreEqual(FString(TEXT("3,2,1")), Trace.ToNormalizedString(),
			TEXT("positional arguments evaluate reverse-formal")));

		{
			FCanonicalTraceScope Scope(Trace);
			ASSERT_THAT(IsTrue(CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int RunNamed()", Result)));
		}
		ASSERT_THAT(AreEqual(303, Result,
			*FString::Printf(TEXT("named A=1 default B=20 named C=3; got=%d trace=%s"),
				Result, *Trace.ToNormalizedString())));
		ASSERT_THAT(AreEqual(FString(TEXT("3,1")), Trace.ToNormalizedString(),
			TEXT("named/default arguments evaluate only provided source operands")));

		{
			FCanonicalTraceScope Scope(Trace);
			ASSERT_THAT(IsTrue(CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int RunShortCircuit()", Result)));
		}
		ASSERT_THAT(AreEqual(0, Result, TEXT("false && ... must yield 0")));
		ASSERT_THAT(AreEqual(FString(TEXT("1")), Trace.ToNormalizedString(),
			TEXT("short-circuit must not evaluate the RHS of false &&")));
	}

	TEST_METHOD(PropertyRewriteAndMutationSingleEvaluation)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("property-rewrite VM matrix selects CANONICAL")));
		ASSERT_THAT(IsTrue(RegisterTrace(Engine)));

		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"CanonicalASTVmProperty",
			ASTEST_AS_ANSI(R"AS(
				class FBox
				{
					int Stored = 7;

					int GetValue() const
					{
						Trace(1);
						return Stored;
					}

					void SetValue(int Next)
					{
						Trace(2);
						Stored = Next;
					}
				}

				FBox MakeBox()
				{
					Trace(8);
					return FBox();
				}

				int RunProperty()
				{
					FBox Box = FBox();
					Box.SetValue(Box.GetValue() + 1);
					return Box.Stored;
				}

				int RunSingleEval()
				{
					MakeBox().Stored += 3;
					return 0;
				}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("property/mutation script should compile")));
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		FCanonicalExecutionTrace Trace;
		int32 Result = 0;
		{
			FCanonicalTraceScope Scope(Trace);
			ASSERT_THAT(IsTrue(CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int RunProperty()", Result)));
		}
		ASSERT_THAT(AreEqual(8, Result, TEXT("virtual property write uses getter then setter")));
		ASSERT_THAT(AreEqual(FString(TEXT("1,2")), Trace.ToNormalizedString(),
			TEXT("accessor rewrite must call GetValue then SetValue")));

		{
			FCanonicalTraceScope Scope(Trace);
			ASSERT_THAT(IsTrue(CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int RunSingleEval()", Result)));
		}
		ASSERT_THAT(AreEqual(FString(TEXT("8")), Trace.ToNormalizedString(),
			TEXT("compound mutation must evaluate the receiver once")));
	}

	TEST_METHOD(CanonicalMemberPostfixCallEvaluatesReceiverOnce)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL)));
		ASSERT_THAT(IsTrue(RegisterTrace(Engine), TEXT("canonical member eval-once needs Trace")));

		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"CanonicalASTVmMemberOnce",
			ASTEST_AS_ANSI(R"AS(
				class T
				{
					int Get()
					{
						Trace(2);
						return 1;
					}
				}

				T Make()
				{
					Trace(1);
					T v;
					return v;
				}

				int Entry()
				{
					return Make().Get();
				}
				)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("CANONICAL Make().Get() must Generate")));
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		FCanonicalExecutionTrace Trace;
		int32 Result = 0;
		{
			FCanonicalTraceScope Scope(Trace);
			ASSERT_THAT(IsTrue(CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Result)));
		}
		ASSERT_THAT(AreEqual(1, Result, TEXT("Get() returns 1")));
		ASSERT_THAT(AreEqual(FString(TEXT("1,2")), Trace.ToNormalizedString(),
			TEXT("Make() then Get(); Make() must run once")));
	}

	TEST_METHOD(CanonicalIndexCompoundAssignEvaluatesMakeOnce)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL)));
		ASSERT_THAT(IsTrue(RegisterTrace(Engine), TEXT("canonical index mutation needs Trace")));

		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"CanonicalASTVmIndexOnce",
			ASTEST_AS_ANSI(R"AS(
				class T
				{
					int Value = 0;

					int &opIndex(int Index)
					{
						Trace(2);
						return Value;
					}
				}

				T Make()
				{
					Trace(1);
					T v;
					return v;
				}

				int Entry()
				{
					Make()[0] += 1;
					return 1;
				}
				)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("CANONICAL Make()[0] += 1 must Generate")));
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		FCanonicalExecutionTrace Trace;
		int32 Result = 0;
		{
			FCanonicalTraceScope Scope(Trace);
			ASSERT_THAT(IsTrue(CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Result)));
		}
		ASSERT_THAT(AreEqual(1, Result, TEXT("Entry returns 1")));
		ASSERT_THAT(AreEqual(FString(TEXT("1,2")), Trace.ToNormalizedString(),
			*FString::Printf(TEXT("Make() then opIndex; Make() must run once. trace=%s result=%d"),
				*Trace.ToNormalizedString(), Result)));
	}

	TEST_METHOD(CanonicalIndexCompoundAssignWritesThroughLocal)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL)));
		ASSERT_THAT(IsTrue(RegisterTrace(Engine), TEXT("canonical index write-through needs Trace register")));

		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"CanonicalASTVmIndexWrite",
			ASTEST_AS_ANSI(R"AS(
				class T
				{
					int Value = 41;

					int &opIndex(int Index)
					{
						return Value;
					}
				}

				int Entry()
				{
					T v;
					v[0] += 1;
					return v[0];
				}
				)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("CANONICAL v[0] += 1 must Generate")));
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		int32 Result = 0;
		ASSERT_THAT(IsTrue(CanonicalExecuteInt(
			*TestRunner, Engine.Get(), Module, "int Entry()", Result)));
		ASSERT_THAT(AreEqual(42, Result,
			*FString::Printf(TEXT("v[0] += 1 must write through opIndex, not a CopyVar temp. got=%d"), Result)));
	}

	TEST_METHOD(IsolatedLogicalShortCircuitMatchesLegacyCanonicalTrace)
	{
		using namespace AngelscriptNativeTestSupport;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			bool F(int a)
			{
				Trace(1);
				return a != 0;
			}

			bool F(float a)
			{
				Trace(9);
				return false;
			}

			bool G(int a)
			{
				Trace(2);
				return true;
			}

			int RunAndTrue()
			{
				return F(1) && G(2) ? 1 : 0;
			}

			int RunAndFalse()
			{
				return F(0) && G(2) ? 1 : 0;
			}

			int RunOrTrue()
			{
				return F(1) || G(2) ? 1 : 0;
			}

			int RunOrFalse()
			{
				return F(0) || G(2) ? 1 : 0;
			}
			)AS");

		auto RunFour = [&](FNativeTestEngine& Engine, asIScriptModule* Module, const TCHAR* Side)
		{
			FCanonicalExecutionTrace Trace;
			int32 Result = 0;
			{
				FCanonicalTraceScope Scope(Trace);
				ASSERT_THAT(IsTrue(CanonicalExecuteInt(
					*TestRunner, Engine.Get(), Module, "int RunAndTrue()", Result)));
			}
			ASSERT_THAT(AreEqual(1, Result, *FString::Printf(TEXT("%s RunAndTrue result"), Side)));
			ASSERT_THAT(AreEqual(FString(TEXT("1,2")), Trace.ToNormalizedString(),
				*FString::Printf(TEXT("%s RunAndTrue Trace F then G; F(float) must not run. trace=%s"),
					Side, *Trace.ToNormalizedString())));

			{
				FCanonicalTraceScope Scope(Trace);
				ASSERT_THAT(IsTrue(CanonicalExecuteInt(
					*TestRunner, Engine.Get(), Module, "int RunAndFalse()", Result)));
			}
			ASSERT_THAT(AreEqual(0, Result, *FString::Printf(TEXT("%s RunAndFalse result"), Side)));
			ASSERT_THAT(AreEqual(FString(TEXT("1")), Trace.ToNormalizedString(),
				*FString::Printf(TEXT("%s false && must not evaluate G. trace=%s"),
					Side, *Trace.ToNormalizedString())));

			{
				FCanonicalTraceScope Scope(Trace);
				ASSERT_THAT(IsTrue(CanonicalExecuteInt(
					*TestRunner, Engine.Get(), Module, "int RunOrTrue()", Result)));
			}
			ASSERT_THAT(AreEqual(1, Result, *FString::Printf(TEXT("%s RunOrTrue result"), Side)));
			ASSERT_THAT(AreEqual(FString(TEXT("1")), Trace.ToNormalizedString(),
				*FString::Printf(TEXT("%s true || must not evaluate G. trace=%s"),
					Side, *Trace.ToNormalizedString())));

			{
				FCanonicalTraceScope Scope(Trace);
				ASSERT_THAT(IsTrue(CanonicalExecuteInt(
					*TestRunner, Engine.Get(), Module, "int RunOrFalse()", Result)));
			}
			ASSERT_THAT(AreEqual(1, Result, *FString::Printf(TEXT("%s RunOrFalse result"), Side)));
			ASSERT_THAT(AreEqual(FString(TEXT("1,2")), Trace.ToNormalizedString(),
				*FString::Printf(TEXT("%s false || must evaluate G. trace=%s"),
					Side, *Trace.ToNormalizedString())));
		};

		{
			FNativeTestEngine LegacyEngine;
			LegacyEngine.Create(*TestRunner);
			ON_SCOPE_EXIT { LegacyEngine.Destroy(); };
			asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(LegacyEngine.Get());
			ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_LEGACY),
				TEXT("differential LEGACY fixture explicitly selects the rollback compiler")));
			ASSERT_THAT(AreEqual(asCOMPILER_PIPELINE_LEGACY, ScriptEngine->GetCompilerPipeline()));
			ASSERT_THAT(IsTrue(RegisterTrace(LegacyEngine), TEXT("LEGACY logical Trace")));
			FScopedNativeModule Module(
				*TestRunner, LegacyEngine, "CanonicalASTVmLogicalLegacy", ScriptSource);
			ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("LEGACY F && G / F || G must compile")));
			if (!Module.IsValid())
			{
				TestRunner->AddInfo(LegacyEngine.GetMessagesText());
				return;
			}
			ASSERT_THAT(AreEqual(
				asBYTECODE_PUBLISHER_COMPILER,
				static_cast<asCModule*>(static_cast<asIScriptModule*>(Module))->GetLastBytecodePublisher(),
				TEXT("LEGACY engine must stay asCCompiler")));
			RunFour(LegacyEngine, Module, TEXT("LEGACY"));
		}

		{
			FNativeTestEngine CanonicalEngine;
			CanonicalEngine.Create(*TestRunner);
			ON_SCOPE_EXIT { CanonicalEngine.Destroy(); };
			asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(CanonicalEngine.Get());
			ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL)));
			ASSERT_THAT(IsTrue(RegisterTrace(CanonicalEngine), TEXT("CANONICAL logical Trace")));
			FScopedNativeModule Module(
				*TestRunner, CanonicalEngine, "CanonicalASTVmLogicalCanonical", ScriptSource);
			ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("CANONICAL F && G / F || G must Generate")));
			if (!Module.IsValid())
			{
				TestRunner->AddInfo(CanonicalEngine.GetMessagesText());
				return;
			}
			ASSERT_THAT(AreEqual(
				asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
				static_cast<asCModule*>(static_cast<asIScriptModule*>(Module))->GetLastBytecodePublisher(),
				TEXT("CANONICAL engine must publish CodeGen, not asCCompiler")));
			RunFour(CanonicalEngine, Module, TEXT("CANONICAL"));
		}
	}

	TEST_METHOD(IsolatedConditionalMismatchedArmsMatchesLegacyCanonicalTrace)
	{
		using namespace AngelscriptNativeTestSupport;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Mark(int Value)
			{
				Trace(Value);
				return Value;
			}

			float MarkF(int Value)
			{
				Trace(Value);
				return 2.0f;
			}

			int EntryThen()
			{
				float v = true ? Mark(1) : MarkF(2);
				if (v > 0.5f)
				{
					if (v < 1.5f)
					{
						return 1;
					}
				}
				return 0;
			}

			int EntryElse()
			{
				float v = false ? Mark(1) : MarkF(2);
				if (v > 1.5f)
				{
					return 2;
				}
				return 0;
			}
			)AS");

		auto RunBoth = [&](FNativeTestEngine& Engine, asIScriptModule* Module, const TCHAR* Side)
		{
			FCanonicalExecutionTrace Trace;
			int32 Result = 0;
			{
				FCanonicalTraceScope Scope(Trace);
				ASSERT_THAT(IsTrue(CanonicalExecuteInt(
					*TestRunner, Engine.Get(), Module, "int EntryThen()", Result)));
			}
			ASSERT_THAT(AreEqual(1, Result, *FString::Printf(TEXT("%s EntryThen result"), Side)));
			ASSERT_THAT(AreEqual(FString(TEXT("1")), Trace.ToNormalizedString(),
				*FString::Printf(TEXT("%s then arm only. trace=%s"), Side, *Trace.ToNormalizedString())));

			{
				FCanonicalTraceScope Scope(Trace);
				ASSERT_THAT(IsTrue(CanonicalExecuteInt(
					*TestRunner, Engine.Get(), Module, "int EntryElse()", Result)));
			}
			ASSERT_THAT(AreEqual(2, Result, *FString::Printf(TEXT("%s EntryElse result"), Side)));
			ASSERT_THAT(AreEqual(FString(TEXT("2")), Trace.ToNormalizedString(),
				*FString::Printf(TEXT("%s else arm only. trace=%s"), Side, *Trace.ToNormalizedString())));
		};

		{
			FNativeTestEngine LegacyEngine;
			LegacyEngine.Create(*TestRunner);
			ON_SCOPE_EXIT { LegacyEngine.Destroy(); };
			asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(LegacyEngine.Get());
			ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_LEGACY),
				TEXT("differential LEGACY fixture explicitly selects the rollback compiler")));
			ASSERT_THAT(AreEqual(asCOMPILER_PIPELINE_LEGACY, ScriptEngine->GetCompilerPipeline()));
			ASSERT_THAT(IsTrue(RegisterTrace(LegacyEngine), TEXT("LEGACY conditional Trace")));
			FScopedNativeModule Module(
				*TestRunner, LegacyEngine, "CanonicalASTVmCondLegacy", ScriptSource);
			ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("LEGACY mismatched-arm conditional must compile")));
			if (!Module.IsValid())
			{
				TestRunner->AddInfo(LegacyEngine.GetMessagesText());
				return;
			}
			ASSERT_THAT(AreEqual(
				asBYTECODE_PUBLISHER_COMPILER,
				static_cast<asCModule*>(static_cast<asIScriptModule*>(Module))->GetLastBytecodePublisher(),
				TEXT("LEGACY engine must stay asCCompiler")));
			RunBoth(LegacyEngine, Module, TEXT("LEGACY"));
		}

		{
			FNativeTestEngine CanonicalEngine;
			CanonicalEngine.Create(*TestRunner);
			ON_SCOPE_EXIT { CanonicalEngine.Destroy(); };
			asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(CanonicalEngine.Get());
			ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL)));
			ASSERT_THAT(IsTrue(RegisterTrace(CanonicalEngine), TEXT("CANONICAL conditional Trace")));
			FScopedNativeModule Module(
				*TestRunner, CanonicalEngine, "CanonicalASTVmCondCanonical", ScriptSource);
			ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("CANONICAL mismatched-arm conditional must Generate")));
			if (!Module.IsValid())
			{
				TestRunner->AddInfo(CanonicalEngine.GetMessagesText());
				return;
			}
			ASSERT_THAT(AreEqual(
				asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
				static_cast<asCModule*>(static_cast<asIScriptModule*>(Module))->GetLastBytecodePublisher(),
				TEXT("CANONICAL engine must publish CodeGen, not asCCompiler")));
			RunBoth(CanonicalEngine, Module, TEXT("CANONICAL"));
		}
	}

	TEST_METHOD(IsolatedValueTemporaryCtorDtorMatchesLegacyCanonicalTrace)
	{
		using namespace AngelscriptNativeTestSupport;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FValue
			{
				int Value = 41;

				FValue()
				{
					Trace(1);
				}

				~FValue()
				{
					Trace(2);
				}
			}

			int Entry()
			{
				return FValue().Value + 1;
			}
			)AS");

		auto RunOnce = [&](FNativeTestEngine& Engine, asIScriptModule* Module, const TCHAR* Side)
		{
			FCanonicalExecutionTrace Trace;
			int32 Result = 0;
			{
				FCanonicalTraceScope Scope(Trace);
				ASSERT_THAT(IsTrue(CanonicalExecuteInt(
					*TestRunner, Engine.Get(), Module, "int Entry()", Result)));
			}
			ASSERT_THAT(AreEqual(42, Result,
				*FString::Printf(TEXT("%s FValue().Value+1 got=%d"), Side, Result)));
			ASSERT_THAT(AreEqual(FString(TEXT("1,2")), Trace.ToNormalizedString(),
				*FString::Printf(TEXT("%s ctor then dtor once. trace=%s result=%d"),
					Side, *Trace.ToNormalizedString(), Result)));
		};

		{
			FNativeTestEngine LegacyEngine;
			LegacyEngine.Create(*TestRunner);
			ON_SCOPE_EXIT { LegacyEngine.Destroy(); };
			asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(LegacyEngine.Get());
			ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_LEGACY),
				TEXT("differential LEGACY fixture explicitly selects the rollback compiler")));
			ASSERT_THAT(AreEqual(asCOMPILER_PIPELINE_LEGACY, ScriptEngine->GetCompilerPipeline()));
			ASSERT_THAT(IsTrue(RegisterTrace(LegacyEngine), TEXT("LEGACY value-temporary Trace")));
			FScopedNativeModule Module(
				*TestRunner, LegacyEngine, "CanonicalASTVmTempLegacy", ScriptSource);
			ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("LEGACY struct FValue() must compile")));
			if (!Module.IsValid())
			{
				TestRunner->AddInfo(LegacyEngine.GetMessagesText());
				return;
			}
			ASSERT_THAT(AreEqual(
				asBYTECODE_PUBLISHER_COMPILER,
				static_cast<asCModule*>(static_cast<asIScriptModule*>(Module))->GetLastBytecodePublisher(),
				TEXT("LEGACY engine must stay asCCompiler")));
			RunOnce(LegacyEngine, Module, TEXT("LEGACY"));
		}

		{
			FNativeTestEngine CanonicalEngine;
			CanonicalEngine.Create(*TestRunner);
			ON_SCOPE_EXIT { CanonicalEngine.Destroy(); };
			asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(CanonicalEngine.Get());
			ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL)));
			ASSERT_THAT(IsTrue(RegisterTrace(CanonicalEngine), TEXT("CANONICAL value-temporary Trace")));
			FScopedNativeModule Module(
				*TestRunner, CanonicalEngine, "CanonicalASTVmTempCanonical", ScriptSource);
			ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("CANONICAL struct FValue() must Generate")));
			if (!Module.IsValid())
			{
				TestRunner->AddInfo(CanonicalEngine.GetMessagesText());
				return;
			}
			ASSERT_THAT(AreEqual(
				asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
				static_cast<asCModule*>(static_cast<asIScriptModule*>(Module))->GetLastBytecodePublisher(),
				TEXT("CANONICAL engine must publish CodeGen, not asCCompiler")));
			RunOnce(CanonicalEngine, Module, TEXT("CANONICAL"));
		}
	}

	TEST_METHOD(IsolatedPropertyCompoundAssignMatchesLegacyCanonicalTrace)
	{
		using namespace AngelscriptNativeTestSupport;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class T
			{
				int Stored = 41;

				int GetValue()
				{
					Trace(2);
					return Stored;
				}

				void SetValue(int Next)
				{
					Trace(3);
					Stored = Next;
				}
			}

			T Make()
			{
				Trace(1);
				T v = T();
				return v;
			}

			int RunMake()
			{
				T t = Make();
				t.SetValue(t.GetValue() + 1);
				return 1;
			}

			int RunLocal()
			{
				T v = T();
				v.SetValue(v.GetValue() + 1);
				return v.Stored;
			}
			)AS");

		auto RunBoth = [&](FNativeTestEngine& Engine, asIScriptModule* Module, const TCHAR* Side)
		{
			FCanonicalExecutionTrace Trace;
			int32 Result = 0;
			{
				FCanonicalTraceScope Scope(Trace);
				ASSERT_THAT(IsTrue(CanonicalExecuteInt(
					*TestRunner, Engine.Get(), Module, "int RunMake()", Result)));
			}
			ASSERT_THAT(AreEqual(1, Result, *FString::Printf(TEXT("%s RunMake result"), Side)));
			ASSERT_THAT(AreEqual(FString(TEXT("1,2,3")), Trace.ToNormalizedString(),
				*FString::Printf(TEXT("%s Make then Get then Set. trace=%s"),
					Side, *Trace.ToNormalizedString())));

			{
				FCanonicalTraceScope Scope(Trace);
				ASSERT_THAT(IsTrue(CanonicalExecuteInt(
					*TestRunner, Engine.Get(), Module, "int RunLocal()", Result)));
			}
			ASSERT_THAT(AreEqual(42, Result, *FString::Printf(TEXT("%s RunLocal Stored after += 1"), Side)));
			ASSERT_THAT(AreEqual(FString(TEXT("2,3")), Trace.ToNormalizedString(),
				*FString::Printf(TEXT("%s v.SetValue(GetValue()+1) must Get then Set. trace=%s result=%d"),
					Side, *Trace.ToNormalizedString(), Result)));
		};

		{
			FNativeTestEngine LegacyEngine;
			LegacyEngine.Create(*TestRunner);
			ON_SCOPE_EXIT { LegacyEngine.Destroy(); };
			asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(LegacyEngine.Get());
			ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_LEGACY),
				TEXT("differential LEGACY fixture explicitly selects the rollback compiler")));
			ASSERT_THAT(AreEqual(asCOMPILER_PIPELINE_LEGACY, ScriptEngine->GetCompilerPipeline()));
			ASSERT_THAT(IsTrue(RegisterTrace(LegacyEngine), TEXT("LEGACY property mutation Trace")));
			FScopedNativeModule Module(
				*TestRunner, LegacyEngine, "CanonicalASTVmPropLegacy", ScriptSource);
			ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("LEGACY Make().Value += 1 must compile")));
			if (!Module.IsValid())
			{
				TestRunner->AddInfo(LegacyEngine.GetMessagesText());
				return;
			}
			ASSERT_THAT(AreEqual(
				asBYTECODE_PUBLISHER_COMPILER,
				static_cast<asCModule*>(static_cast<asIScriptModule*>(Module))->GetLastBytecodePublisher(),
				TEXT("LEGACY engine must stay asCCompiler")));
			RunBoth(LegacyEngine, Module, TEXT("LEGACY"));
		}

		{
			FNativeTestEngine CanonicalEngine;
			CanonicalEngine.Create(*TestRunner);
			ON_SCOPE_EXIT { CanonicalEngine.Destroy(); };
			asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(CanonicalEngine.Get());
			ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL)));
			ASSERT_THAT(IsTrue(RegisterTrace(CanonicalEngine), TEXT("CANONICAL property mutation Trace")));
			FScopedNativeModule Module(
				*TestRunner, CanonicalEngine, "CanonicalASTVmPropCanonical", ScriptSource);
			ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("CANONICAL Make().Value += 1 must Generate")));
			if (!Module.IsValid())
			{
				TestRunner->AddInfo(CanonicalEngine.GetMessagesText());
				return;
			}
			ASSERT_THAT(AreEqual(
				asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
				static_cast<asCModule*>(static_cast<asIScriptModule*>(Module))->GetLastBytecodePublisher(),
				TEXT("CANONICAL engine must publish CodeGen, not asCCompiler")));
			RunBoth(CanonicalEngine, Module, TEXT("CANONICAL"));
		}
	}

	TEST_METHOD(CanonicalPropertyCompoundAssignEvaluatesMakeOnce)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL)));
		ASSERT_THAT(IsTrue(RegisterTrace(Engine), TEXT("canonical property mutation needs Trace")));

		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"CanonicalASTVmPropOnce",
			ASTEST_AS_ANSI(R"AS(
				class T
				{
					int Stored = 41;

					int GetValue()
					{
						Trace(2);
						return Stored;
					}

					void SetValue(int Next)
					{
						Trace(3);
						Stored = Next;
					}
				}

				T Make()
				{
					Trace(1);
					T v;
					return v;
				}

				int Entry()
				{
					Make().Value += 1;
					return 1;
				}
				)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("CANONICAL Make().Value += 1 must Generate")));
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(static_cast<asIScriptModule*>(Module))->GetLastBytecodePublisher(),
			TEXT("CANONICAL Make().Value += 1 must publish CodeGen")));

		FCanonicalExecutionTrace Trace;
		int32 Result = 0;
		asIScriptModule* const ScriptModule = static_cast<asIScriptModule*>(Module);
		asIScriptFunction* const EntryFunction = ScriptModule->GetFunctionByDecl("int Entry()");
		asIScriptFunction* const MakeFunction = ScriptModule->GetFunctionByDecl("T Make()");
		asIScriptFunction* const GetFunction = ScriptModule->GetFunctionByDecl("int T::GetValue()");
		asIScriptFunction* const SetFunction = ScriptModule->GetFunctionByDecl("void T::SetValue(int)");
		asCString EntryDump;
		asCString MakeDump;
		asCString GetDump;
		asCString SetDump;
		asCBytecodeCodeGenDumpFunction(static_cast<asCScriptFunction*>(EntryFunction), EntryDump);
		asCBytecodeCodeGenDumpFunction(static_cast<asCScriptFunction*>(MakeFunction), MakeDump);
		asCBytecodeCodeGenDumpFunction(static_cast<asCScriptFunction*>(GetFunction), GetDump);
		asCBytecodeCodeGenDumpFunction(static_cast<asCScriptFunction*>(SetFunction), SetDump);
		{
			FCanonicalTraceScope Scope(Trace);
			const bool bExecuted = CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Result);
			ASSERT_THAT(IsTrue(bExecuted,
				*FString::Printf(
					TEXT("CANONICAL property compound execution failed.\nEntry:\n%s\nMake:\n%s\nGet:\n%s\nSet:\n%s"),
					UTF8_TO_TCHAR(EntryDump.AddressOf() ? EntryDump.AddressOf() : ""),
					UTF8_TO_TCHAR(MakeDump.AddressOf() ? MakeDump.AddressOf() : ""),
					UTF8_TO_TCHAR(GetDump.AddressOf() ? GetDump.AddressOf() : ""),
					UTF8_TO_TCHAR(SetDump.AddressOf() ? SetDump.AddressOf() : ""))));
		}
		ASSERT_THAT(AreEqual(1, Result, TEXT("Entry returns 1")));
		ASSERT_THAT(AreEqual(FString(TEXT("1,2,3")), Trace.ToNormalizedString(),
			*FString::Printf(TEXT("Make().Value += 1 must Make once then Get then Set. trace=%s"),
				*Trace.ToNormalizedString())));
	}

	TEST_METHOD(LoopsSwitchTransfersAndSafePoints)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("control-flow VM matrix selects CANONICAL")));
		ASSERT_THAT(IsTrue(RegisterTrace(Engine)));

		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"CanonicalASTVmControl",
			ASTEST_AS_ANSI(R"AS(
				int RunLoopSwitch(int Selector)
				{
					int Total = 0;
					for (int Index = 0; Index < 3; ++Index)
					{
						switch (Selector)
						{
						case 1:
							Total += Index;
							break;
						case 2:
							Total += 10;
							continue;
						default:
							return Total;
						}
					}
					while (Total < 4)
					{
						Total += 1;
					}
					int Guard = 0;
					do
					{
						++Guard;
					} while (Guard < 2);
					return Total + Guard;
				}
			)AS"),
			asAST_RETAIN_SNAPSHOT);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("control-flow script should compile")));
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asIScriptFunction* const Function =
			GetNativeFunctionByDecl(Module, "int RunLoopSwitch(int)");
		ASSERT_THAT(IsNotNull(Function,
			*FString::Printf(TEXT("loop/switch entry must exist; functions={%s}; diagnostics={%s}"),
				*CollectFunctionDeclarations(Module), *Engine.GetMessagesText())));
		if (Function == nullptr)
		{
			return;
		}
		asIScriptContext* const Context = Engine.Get()->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("loop/switch needs a VM context")));
		ON_SCOPE_EXIT { if (Context != nullptr) { Context->Release(); } };
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function), TEXT("Prepare loop/switch")));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 1), TEXT("Selector=1")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(),
			TEXT("loop/switch must finish")));
		const int32 Result = static_cast<int32>(Context->GetReturnDWord());
		ASSERT_THAT(AreEqual(6, Result,
			TEXT("for 0+1+2, while to 4, do-while Guard=2 => 6")));

		const asCASTContext* const AST =
			static_cast<asCModule*>(static_cast<asIScriptModule*>(Module))->GetCanonicalASTContext();
		ASSERT_THAT(IsNotNull(AST, TEXT("control-flow module must retain its sealed canonical AST")));
		if (AST == nullptr)
		{
			return;
		}

		bool bHasFor = false;
		bool bHasSwitch = false;
		bool bHasWhileOrDo = false;
		bool bHasSafePoint = false;
		for (asUINT Index = 1; Index <= AST->GetStmtCount(); ++Index)
		{
			const asCStmt* const Statement = AST->GetStmt(asASTStmtId(Index));
			if (Statement == nullptr)
			{
				continue;
			}
			bHasFor = bHasFor || Statement->kind == asAST_STMT_FOR;
			bHasSwitch = bHasSwitch || Statement->kind == asAST_STMT_SWITCH;
			bHasWhileOrDo = bHasWhileOrDo
				|| Statement->kind == asAST_STMT_WHILE
				|| Statement->kind == asAST_STMT_DO_WHILE;
			if (Statement->safePointRole != asAST_SAFEPOINT_NONE)
			{
				bHasSafePoint = true;
			}
		}
		ASSERT_THAT(IsTrue(bHasFor, TEXT("canonical AST must record for")));
		ASSERT_THAT(IsTrue(bHasSwitch, TEXT("canonical AST must record switch")));
		ASSERT_THAT(IsTrue(bHasWhileOrDo, TEXT("canonical AST must record while/do")));
		ASSERT_THAT(IsTrue(bHasSafePoint,
			TEXT("canonical AST control-flow compilation must publish safe-point roles")));
	}

	TEST_METHOD(CleanupDestructionExceptionsImportsAndGlobals)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("cleanup/import/global VM matrix selects CANONICAL")));
		ASSERT_THAT(IsTrue(RegisterTrace(Engine)));

		FScopedNativeModule Provider(
			*TestRunner,
			Engine,
			"CanonicalASTVmImportProvider",
			ASTEST_AS_ANSI(R"AS(
				int SharedValue()
				{
					return 77;
				}
			)AS"));
		ASSERT_THAT(IsTrue(Provider.IsValid(), TEXT("import provider should compile")));
		asIScriptModule* TryCatchModule = nullptr;
		const std::string TryCatchSource = ASTEST_AS_ANSI(R"AS(
				int RunTryCatch()
				{
					try {} catch {}
					return 0;
				}
			)AS");
		const int TryCatchResult = CompileNativeModule(
			Engine.Get(),
			"CanonicalASTVmTryCatch",
			TryCatchSource.c_str(),
			TryCatchModule);
		ASSERT_THAT(IsTrue(TryCatchResult < 0,
			TEXT("try/catch remains rejected so the consumer script must fail closed")));
		const FString TryCatchDiagnostics = CanonicalNormalizeDiagnostics(Engine.GetMessages());
		ASSERT_THAT(IsTrue(TryCatchDiagnostics.Contains(TEXT("Error")),
			TEXT("rejected try/catch must emit a shipped diagnostic")));

		Engine.ResetMessages();
		FScopedNativeModule RuntimeConsumer(
			*TestRunner,
			Engine,
			"CanonicalASTVmImportRuntime",
			ASTEST_AS_ANSI(R"AS(
				import int SharedValue() from "CanonicalASTVmImportProvider";
				const int Frozen = 4;

				class FTraceBox
				{
					int Id;

					FTraceBox(int Value)
					{
						Id = Value;
						Trace(100 + Value);
					}

					~FTraceBox()
					{
						Trace(200 + Id);
					}
				}

				int RunImport()
				{
					return SharedValue() + Frozen;
				}

				int RunCleanup()
				{
					FTraceBox First = FTraceBox(1);
					{
						FTraceBox Second = FTraceBox(2);
					}
					return First.Id;
				}

				int RunException()
				{
					Boom();
					return 1;
				}
			)AS"));
		ASSERT_THAT(IsTrue(RuntimeConsumer.IsValid(), TEXT("import/cleanup script should compile without try/catch")));
		if (!RuntimeConsumer.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asIScriptFunction* const Shared = Provider->GetFunctionByDecl("int SharedValue()");
		ASSERT_THAT(AreEqual(0, RuntimeConsumer->BindImportedFunction(0, Shared),
			TEXT("imported function must bind to the provider")));

		int32 Result = 0;
		ASSERT_THAT(IsTrue(CanonicalExecuteInt(
			*TestRunner, Engine.Get(), RuntimeConsumer, "int RunImport()", Result)));
		ASSERT_THAT(AreEqual(81, Result, TEXT("import 77 + const global 4")));

		FCanonicalExecutionTrace Trace;
		{
			FCanonicalTraceScope Scope(Trace);
			ASSERT_THAT(IsTrue(CanonicalExecuteInt(
				*TestRunner, Engine.Get(), RuntimeConsumer, "int RunCleanup()", Result)));
			Engine.Get()->GarbageCollect();
		}
		ASSERT_THAT(AreEqual(1, Result, TEXT("outer object remains live")));
		ASSERT_THAT(IsTrue(Trace.ToNormalizedString().Contains(TEXT("101")),
			TEXT("constructor of First must run")));
		ASSERT_THAT(IsTrue(Trace.ToNormalizedString().Contains(TEXT("102")),
			TEXT("constructor of Second must run")));

		FString Exception;
		const int Status = CanonicalExecuteStatus(
			Engine.Get(), RuntimeConsumer, "int RunException()", Exception);
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), Status,
			TEXT("Boom() must take the shipped VM exception path")));
		ASSERT_THAT(IsTrue(
			Exception.Contains(TEXT("canonical-vm-matrix")) || Exception.Len() > 0,
			*FString::Printf(TEXT("exception string must be populated, got '%s'"), *Exception)));

		Engine.ResetMessages();
		asIScriptModule* MutableGlobal = nullptr;
		const int MutableResult = CompileNativeModule(
			Engine.Get(),
			"CanonicalASTVmMutableGlobal",
			"int Mutable = 1; int Read() { return Mutable; }",
			MutableGlobal);
		ASSERT_THAT(AreEqual(0, MutableResult,
			TEXT("canonical source compilation must support module-level mutable globals")));
		if (MutableResult >= 0 && MutableGlobal != nullptr)
		{
			ASSERT_THAT(AreEqual(
				asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
				static_cast<asCModule*>(MutableGlobal)->GetLastBytecodePublisher(),
				TEXT("mutable-global module must publish canonical CodeGen bytecode")));
			int32 MutableValue = 0;
			ASSERT_THAT(IsTrue(CanonicalExecuteInt(
				*TestRunner, Engine.Get(), MutableGlobal, "int Read()", MutableValue)));
			ASSERT_THAT(AreEqual(1, MutableValue, TEXT("canonical mutable global initializes to 1")));
		}
	}
};

#endif
