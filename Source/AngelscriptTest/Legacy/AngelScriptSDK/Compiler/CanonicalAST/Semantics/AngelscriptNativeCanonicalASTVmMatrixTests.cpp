#include "AngelscriptTestMacros.h"
#include "../../../Support/AngelscriptNativeCanonicalASTTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_bytecode_codegen.h"
#include "as_module.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
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

	void AssertLegacyCompilerProvenance(asIScriptModule* Module, const TCHAR* Fixture)
	{
		asCModule* const ConcreteModule = static_cast<asCModule*>(Module);
		ASSERT_THAT(IsNotNull(ConcreteModule,
			*FString::Printf(TEXT("%s must expose a concrete module"), Fixture)));
		if (ConcreteModule == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_COMPILER,
			ConcreteModule->GetLastBytecodePublisher(),
			*FString::Printf(TEXT("%s must publish through asCCompiler"), Fixture)));
		ASSERT_THAT(IsTrue(ConcreteModule->GetLastLegacyCompilerInvocationCount() > 0,
			*FString::Printf(TEXT("%s must record a legacy compiler invocation"), Fixture)));
		asQWORD CanonicalDigest = 0;
		ASSERT_THAT(IsFalse(ConcreteModule->GetLastCanonicalCodeGenAstDigest(CanonicalDigest),
			*FString::Printf(TEXT("%s must not publish a Canonical AST digest"), Fixture)));
	}

	void AssertCanonicalCodeGenProvenance(asIScriptModule* Module, const TCHAR* Fixture)
	{
		asCModule* const ConcreteModule = static_cast<asCModule*>(Module);
		ASSERT_THAT(IsNotNull(ConcreteModule,
			*FString::Printf(TEXT("%s must expose a concrete module"), Fixture)));
		if (ConcreteModule == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			ConcreteModule->GetLastBytecodePublisher(),
			*FString::Printf(TEXT("%s must publish through Canonical CodeGen"), Fixture)));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			ConcreteModule->GetLastLegacyCompilerInvocationCount(),
			*FString::Printf(TEXT("%s must not instantiate asCCompiler"), Fixture)));
		asQWORD CanonicalDigest = 0;
		ASSERT_THAT(IsTrue(
			ConcreteModule->GetLastCanonicalCodeGenAstDigest(CanonicalDigest)
				&& CanonicalDigest != 0,
			*FString::Printf(TEXT("%s must publish a sealed AST digest"), Fixture)));
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
					T v = T();
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
					T v = T();
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

				int MutateAndReadField()
				{
					T v = T();
					v[0] += 1;
					return v.Value;
				}

				int ReadIndex()
				{
					T v = T();
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
			*TestRunner, Engine.Get(), Module, "int MutateAndReadField()", Result)));
		ASSERT_THAT(AreEqual(42, Result,
			*FString::Printf(TEXT("v[0] += 1 must write through opIndex, not a CopyVar temp. got=%d"), Result)));
		ASSERT_THAT(IsTrue(CanonicalExecuteInt(
			*TestRunner, Engine.Get(), Module, "int ReadIndex()", Result)));
		ASSERT_THAT(AreEqual(41, Result,
			*FString::Printf(TEXT("reading v[0] must load the scalar through opIndex's returned reference. got=%d"), Result)));
	}

	TEST_METHOD(IsolatedIndexCompoundAssignMatchesLegacyCanonicalTrace)
	{
		using namespace AngelscriptNativeTestSupport;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class T
			{
				int Value = 41;

				int &opIndex(int Index)
				{
					Trace(2);
					return Value;
				}
			}

			T Make()
			{
				Trace(1);
				T v = T();
				return v;
			}

			int MakeRhs()
			{
				Trace(3);
				return 1;
			}

			int RunMake()
			{
				Make()[0] += MakeRhs();
				return 1;
			}

			class AliasingT
			{
				int SharedRhs = 10;
				int SharedSink = 1;

				int &GetSharedRhs()
				{
					Trace(8);
					return SharedRhs;
				}

				int &opIndex(int Index)
				{
					Trace(2);
					return SharedSink;
				}

				AliasingT MutateAndReturn()
				{
					Trace(9);
					SharedRhs = 20;
					return this;
				}

				int RunAliasedRhs()
				{
					SharedRhs = 10;
					SharedSink = 1;
					MutateAndReturn()[0] += GetSharedRhs();
					return SharedSink;
				}
			}

			int RunAliasedRhs()
			{
				AliasingT Value = AliasingT();
				return Value.RunAliasedRhs();
			}

			int RunLocal()
			{
				T v = T();
				v[0] += 1;
				return v.Value;
			}

			int RunMakePrefix()
			{
				return ++Make()[0];
			}

			int RunLocalPrefixAndRead()
			{
				T v = T();
				int Updated = ++v[0];
				return Updated * 100 + v.Value;
			}

			int RunMakePostfix()
			{
				return Make()[0]++;
			}

			int RunLocalPostfixAndRead()
			{
				T v = T();
				v[0]++;
				return v.Value;
			}

			int ReadLocal()
			{
				T v = T();
				return v[0];
			}

			struct FCounter
			{
				int Value = 41;

				int opPreInc()
				{
					Trace(5);
					Value += 1;
					return Value;
				}

				int opPostInc()
				{
					Trace(7);
					int Original = Value;
					Value += 1;
					return Original;
				}
			}

			struct FCounterBox
			{
				FCounter Item;

				FCounter &opIndex(int Index)
				{
					Trace(6);
					return Item;
				}
			}

			int RunObjectPrefixAndRead()
			{
				FCounterBox Box;
				int Updated = ++Box[0];
				return Updated * 100 + Box.Item.Value;
			}

			int RunObjectPostfixAndRead()
			{
				FCounterBox Box;
				int Original = Box[0]++;
				return Original * 100 + Box.Item.Value;
			}

			class BoolBox
			{
				bool Value = false;
				int Marker = 8;

				bool &opIndex(int Index)
				{
					Trace(Marker);
					return Value;
				}
			}

			bool ReadFalseBoolValue()
			{
				BoolBox Box = BoolBox();
				return Box[0];
			}

			bool ReadConditionalBoolValue(bool PickFirst)
			{
				BoolBox First = BoolBox();
				BoolBox Second = BoolBox();
				Second.Value = true;
				Second.Marker = 9;
				return PickFirst ? First[0] : Second[0];
			}

			int ReadFalseBool()
			{
				return ReadFalseBoolValue() ? 1 : 0;
			}

			int ReadConditionalFirstFalseBool()
			{
				return ReadConditionalBoolValue(true) ? 1 : 0;
			}

			int ReadConditionalSecondTrueBool()
			{
				return ReadConditionalBoolValue(false) ? 1 : 0;
			}

			int ReadReferenceConditional(
				bool PickFirst,
				const int &in First,
				const int &in Second)
			{
				return PickFirst ? First : Second;
			}

			int ReadFirstReferenceConditional()
			{
				int First = 11;
				int Second = 29;
				return ReadReferenceConditional(true, First, Second);
			}

			int ReadSecondReferenceConditional()
			{
				int First = 11;
				int Second = 29;
				return ReadReferenceConditional(false, First, Second);
			}
			)AS");

		auto RunAll = [&](FNativeTestEngine& Engine, asIScriptModule* Module, const TCHAR* Side)
		{
			FCanonicalExecutionTrace Trace;
			int32 Result = 0;
			{
				FCanonicalTraceScope Scope(Trace);
				ASSERT_THAT(IsTrue(CanonicalExecuteInt(
					*TestRunner, Engine.Get(), Module, "int RunMake()", Result)));
			}
			ASSERT_THAT(AreEqual(1, Result,
				*FString::Printf(TEXT("%s RunMake result"), Side)));
			ASSERT_THAT(AreEqual(FString(TEXT("3,1,2")), Trace.ToNormalizedString(),
				*FString::Printf(TEXT("%s RHS then Make then opIndex exactly once. trace=%s"),
					Side, *Trace.ToNormalizedString())));

			{
				FCanonicalTraceScope Scope(Trace);
				ASSERT_THAT(IsTrue(CanonicalExecuteInt(
					*TestRunner, Engine.Get(), Module, "int RunAliasedRhs()", Result)));
			}
			ASSERT_THAT(AreEqual(11, Result,
				*FString::Printf(
					TEXT("%s scalar-reference RHS must snapshot 10 before receiver mutates the shared source; got=%d"),
					Side, Result)));
			ASSERT_THAT(AreEqual(FString(TEXT("8,9,2")), Trace.ToNormalizedString(),
				*FString::Printf(
					TEXT("%s aliased RHS must evaluate and load before receiver then opIndex exactly once. trace=%s"),
					Side, *Trace.ToNormalizedString())));

			{
				FCanonicalTraceScope Scope(Trace);
				ASSERT_THAT(IsTrue(CanonicalExecuteInt(
					*TestRunner, Engine.Get(), Module, "int RunLocal()", Result)));
			}
			ASSERT_THAT(AreEqual(42, Result,
				*FString::Printf(TEXT("%s local index compound assignment must write through"), Side)));
			ASSERT_THAT(AreEqual(FString(TEXT("2")), Trace.ToNormalizedString(),
				*FString::Printf(TEXT("%s local mutation calls opIndex once. trace=%s"),
					Side, *Trace.ToNormalizedString())));

			{
				FCanonicalTraceScope Scope(Trace);
				ASSERT_THAT(IsTrue(CanonicalExecuteInt(
					*TestRunner, Engine.Get(), Module, "int ReadLocal()", Result)));
			}
			ASSERT_THAT(AreEqual(41, Result,
				*FString::Printf(TEXT("%s scalar reference index read must load the payload"), Side)));
			ASSERT_THAT(AreEqual(FString(TEXT("2")), Trace.ToNormalizedString(),
				*FString::Printf(TEXT("%s local read calls opIndex once. trace=%s"),
					Side, *Trace.ToNormalizedString())));

			{
				FCanonicalTraceScope Scope(Trace);
				ASSERT_THAT(IsTrue(CanonicalExecuteInt(
					*TestRunner, Engine.Get(), Module, "int RunMakePrefix()", Result)));
			}
			ASSERT_THAT(AreEqual(42, Result,
				*FString::Printf(TEXT("%s prefix index mutation must yield the updated value"), Side)));
			ASSERT_THAT(AreEqual(FString(TEXT("1,2")), Trace.ToNormalizedString(),
				*FString::Printf(TEXT("%s prefix index mutation evaluates Make then opIndex once. trace=%s"),
					Side, *Trace.ToNormalizedString())));

			{
				FCanonicalTraceScope Scope(Trace);
				ASSERT_THAT(IsTrue(CanonicalExecuteInt(
					*TestRunner, Engine.Get(), Module, "int RunLocalPrefixAndRead()", Result)));
			}
			ASSERT_THAT(AreEqual(4242, Result,
				*FString::Printf(
					TEXT("%s local prefix index mutation must yield and store the same updated value"),
					Side)));
			ASSERT_THAT(AreEqual(FString(TEXT("2")), Trace.ToNormalizedString(),
				*FString::Printf(TEXT("%s local prefix index mutation calls opIndex once. trace=%s"),
					Side, *Trace.ToNormalizedString())));

			{
				FCanonicalTraceScope Scope(Trace);
				ASSERT_THAT(IsTrue(CanonicalExecuteInt(
					*TestRunner, Engine.Get(), Module, "int RunMakePostfix()", Result)));
			}
			ASSERT_THAT(AreEqual(41, Result,
				*FString::Printf(TEXT("%s postfix index mutation must yield the original value"), Side)));
			ASSERT_THAT(AreEqual(FString(TEXT("1,2")), Trace.ToNormalizedString(),
				*FString::Printf(TEXT("%s postfix index mutation evaluates Make then opIndex once. trace=%s"),
					Side, *Trace.ToNormalizedString())));

			{
				FCanonicalTraceScope Scope(Trace);
				ASSERT_THAT(IsTrue(CanonicalExecuteInt(
					*TestRunner, Engine.Get(), Module, "int RunLocalPostfixAndRead()", Result)));
			}
			ASSERT_THAT(AreEqual(42, Result,
				*FString::Printf(TEXT("%s postfix index mutation must write through its reference"), Side)));
			ASSERT_THAT(AreEqual(FString(TEXT("2")), Trace.ToNormalizedString(),
				*FString::Printf(TEXT("%s local postfix index mutation calls opIndex once. trace=%s"),
					Side, *Trace.ToNormalizedString())));

			{
				FCanonicalTraceScope Scope(Trace);
				ASSERT_THAT(IsTrue(CanonicalExecuteInt(
					*TestRunner, Engine.Get(), Module, "int RunObjectPrefixAndRead()", Result)));
			}
			ASSERT_THAT(AreEqual(4242, Result,
				*FString::Printf(
					TEXT("%s object-reference prefix must call opPreInc and update the indexed object"),
					Side)));
			ASSERT_THAT(AreEqual(FString(TEXT("6,5")), Trace.ToNormalizedString(),
				*FString::Printf(
					TEXT("%s object-reference prefix must call opIndex then opPreInc once. trace=%s"),
					Side, *Trace.ToNormalizedString())));

			{
				FCanonicalTraceScope Scope(Trace);
				ASSERT_THAT(IsTrue(CanonicalExecuteInt(
					*TestRunner, Engine.Get(), Module, "int RunObjectPostfixAndRead()", Result)));
			}
			ASSERT_THAT(AreEqual(4142, Result,
				*FString::Printf(
					TEXT("%s object-reference postfix must yield old value and update the indexed object"),
					Side)));
			ASSERT_THAT(AreEqual(FString(TEXT("6,7")), Trace.ToNormalizedString(),
				*FString::Printf(
					TEXT("%s object-reference postfix must call opIndex then opPostInc once. trace=%s"),
					Side, *Trace.ToNormalizedString())));

			ASSERT_THAT(IsTrue(CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int ReadFalseBool()", Result)));
			ASSERT_THAT(AreEqual(0, Result,
				*FString::Printf(TEXT("%s bool reference return must load false payload"), Side)));
			{
				FCanonicalTraceScope Scope(Trace);
				ASSERT_THAT(IsTrue(CanonicalExecuteInt(
					*TestRunner, Engine.Get(), Module,
					"int ReadConditionalFirstFalseBool()", Result)));
			}
			ASSERT_THAT(AreEqual(0, Result,
				*FString::Printf(
					TEXT("%s conditional bool reference must select and load the first false payload"),
					Side)));
			ASSERT_THAT(AreEqual(FString(TEXT("8")), Trace.ToNormalizedString(),
				*FString::Printf(
					TEXT("%s reference Conditional true path must evaluate only the first address arm. trace=%s"),
					Side, *Trace.ToNormalizedString())));
			{
				FCanonicalTraceScope Scope(Trace);
				ASSERT_THAT(IsTrue(CanonicalExecuteInt(
					*TestRunner, Engine.Get(), Module,
					"int ReadConditionalSecondTrueBool()", Result)));
			}
			ASSERT_THAT(AreEqual(1, Result,
				*FString::Printf(
					TEXT("%s conditional bool reference must select and load the second true payload"),
					Side)));
			ASSERT_THAT(AreEqual(FString(TEXT("9")), Trace.ToNormalizedString(),
				*FString::Printf(
					TEXT("%s reference Conditional false path must evaluate only the second address arm. trace=%s"),
					Side, *Trace.ToNormalizedString())));
			ASSERT_THAT(IsTrue(CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module,
				"int ReadFirstReferenceConditional()", Result)));
			ASSERT_THAT(AreEqual(11, Result,
				*FString::Printf(
					TEXT("%s reference Conditional must preserve the first DeclRef address"), Side)));
			ASSERT_THAT(IsTrue(CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module,
				"int ReadSecondReferenceConditional()", Result)));
			ASSERT_THAT(AreEqual(29, Result,
				*FString::Printf(
					TEXT("%s reference Conditional must preserve the second DeclRef address"), Side)));
		};

		{
			FNativeTestEngine LegacyEngine;
			LegacyEngine.Create(*TestRunner);
			ON_SCOPE_EXIT { LegacyEngine.Destroy(); };
			asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(LegacyEngine.Get());
			ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_LEGACY)));
			ASSERT_THAT(AreEqual(asCOMPILER_PIPELINE_LEGACY, ScriptEngine->GetCompilerPipeline(),
				TEXT("LEGACY engine must retain the explicitly selected pipeline")));
			ASSERT_THAT(IsTrue(RegisterTrace(LegacyEngine), TEXT("LEGACY index mutation Trace")));
			FScopedNativeModule Module(
				*TestRunner, LegacyEngine, "CanonicalASTVmIndexLegacy", ScriptSource);
			ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("LEGACY index mutation fixture must compile")));
			if (!Module.IsValid())
			{
				TestRunner->AddInfo(LegacyEngine.GetMessagesText());
				return;
			}
			AssertLegacyCompilerProvenance(
				static_cast<asIScriptModule*>(Module), TEXT("LEGACY index fixture"));
			RunAll(LegacyEngine, Module, TEXT("LEGACY"));
		}

		{
			FNativeTestEngine CanonicalEngine;
			CanonicalEngine.Create(*TestRunner);
			ON_SCOPE_EXIT { CanonicalEngine.Destroy(); };
			asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(CanonicalEngine.Get());
			ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL)));
			ASSERT_THAT(AreEqual(asCOMPILER_PIPELINE_CANONICAL, ScriptEngine->GetCompilerPipeline(),
				TEXT("CANONICAL engine must retain the explicitly selected pipeline")));
			ASSERT_THAT(IsTrue(RegisterTrace(CanonicalEngine), TEXT("CANONICAL index mutation Trace")));
			FScopedNativeModule Module(
				*TestRunner, CanonicalEngine, "CanonicalASTVmIndexCanonical", ScriptSource);
			ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("CANONICAL index mutation fixture must Generate")));
			if (!Module.IsValid())
			{
				TestRunner->AddInfo(CanonicalEngine.GetMessagesText());
				return;
			}
			AssertCanonicalCodeGenProvenance(
				static_cast<asIScriptModule*>(Module), TEXT("CANONICAL index fixture"));
			RunAll(CanonicalEngine, Module, TEXT("CANONICAL"));
		}
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
			AssertLegacyCompilerProvenance(
				static_cast<asIScriptModule*>(Module), TEXT("LEGACY logical fixture"));
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
			AssertCanonicalCodeGenProvenance(
				static_cast<asIScriptModule*>(Module), TEXT("CANONICAL logical fixture"));
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
			AssertLegacyCompilerProvenance(
				static_cast<asIScriptModule*>(Module), TEXT("LEGACY conditional fixture"));
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
			AssertCanonicalCodeGenProvenance(
				static_cast<asIScriptModule*>(Module), TEXT("CANONICAL conditional fixture"));
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
			AssertLegacyCompilerProvenance(
				static_cast<asIScriptModule*>(Module), TEXT("LEGACY value-temporary fixture"));
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
			AssertCanonicalCodeGenProvenance(
				static_cast<asIScriptModule*>(Module), TEXT("CANONICAL value-temporary fixture"));
			RunOnce(CanonicalEngine, Module, TEXT("CANONICAL"));
		}
	}

	TEST_METHOD(IsolatedCompilerGeneratedDefaultConstructorMatchesLegacyCanonicalTrace)
	{
		using namespace AngelscriptNativeTestSupport;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int MakeGenerated(int Value)
			{
				Trace(Value);
				return Value;
			}

			struct FGenerated
			{
				int First = MakeGenerated(1);
				int Second = MakeGenerated(2);
			}

			int Entry()
			{
				FGenerated v;
				return v.First * 10 + v.Second;
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
			ASSERT_THAT(AreEqual(12, Result,
				*FString::Printf(TEXT("%s compiler-generated default constructor result"), Side)));
			ASSERT_THAT(AreEqual(FString(TEXT("1,2")), Trace.ToNormalizedString(),
				*FString::Printf(
					TEXT("%s generated construction must evaluate member initializers in declaration order once. trace=%s"),
					Side, *Trace.ToNormalizedString())));
		};

		{
			FNativeTestEngine LegacyEngine;
			LegacyEngine.Create(*TestRunner);
			ON_SCOPE_EXIT { LegacyEngine.Destroy(); };
			asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(LegacyEngine.Get());
			ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_LEGACY)));
			ASSERT_THAT(AreEqual(asCOMPILER_PIPELINE_LEGACY, ScriptEngine->GetCompilerPipeline()));
			ASSERT_THAT(IsTrue(RegisterTrace(LegacyEngine), TEXT("LEGACY generated-value Trace")));
			FScopedNativeModule Module(
				*TestRunner, LegacyEngine, "CanonicalASTVmGeneratedLegacy", ScriptSource);
			ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("LEGACY generated default constructor must compile")));
			if (!Module.IsValid())
			{
				TestRunner->AddInfo(LegacyEngine.GetMessagesText());
				return;
			}
			AssertLegacyCompilerProvenance(
				static_cast<asIScriptModule*>(Module), TEXT("LEGACY generated-value fixture"));
			RunOnce(LegacyEngine, Module, TEXT("LEGACY"));
		}

		{
			FNativeTestEngine CanonicalEngine;
			CanonicalEngine.Create(*TestRunner);
			ON_SCOPE_EXIT { CanonicalEngine.Destroy(); };
			asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(CanonicalEngine.Get());
			ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL)));
			ASSERT_THAT(AreEqual(asCOMPILER_PIPELINE_CANONICAL, ScriptEngine->GetCompilerPipeline()));
			ASSERT_THAT(IsTrue(RegisterTrace(CanonicalEngine), TEXT("CANONICAL generated-value Trace")));
			FScopedNativeModule Module(
				*TestRunner, CanonicalEngine, "CanonicalASTVmGeneratedCanonical", ScriptSource);
			ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("CANONICAL generated default constructor must Generate")));
			if (!Module.IsValid())
			{
				TestRunner->AddInfo(CanonicalEngine.GetMessagesText());
				return;
			}
			AssertCanonicalCodeGenProvenance(
				static_cast<asIScriptModule*>(Module), TEXT("CANONICAL generated-value fixture"));
			RunOnce(CanonicalEngine, Module, TEXT("CANONICAL"));
		}
	}

	TEST_METHOD(LegacyPropertyUnaryAccessorFormsRemainRejected)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->SetEngineProperty(asEP_PROPERTY_ACCESSOR_MODE, 2),
			TEXT("LEGACY negative gate uses the maintained accessor profile")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_LEGACY),
			TEXT("LEGACY negative gate must explicitly select asCCompiler")));
		ASSERT_THAT(IsTrue(RegisterTrace(Engine),
			TEXT("LEGACY negative property gate registers an observable runtime trace")));

		const std::string SharedSource = ASTEST_AS_ANSI(R"AS(
			class T
			{
				int Stored = 5;

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
				T Object = T();
				return Object;
			}
			)AS");

		auto AssertRejected = [&](const char* ModuleName, const std::string& EntrySource,
			const TCHAR* ExpectedDiagnostic, const TCHAR* Form)
		{
			Engine.ResetMessages();
			asIScriptModule* const Module = ScriptEngine->GetModule(
				ModuleName, asGM_ALWAYS_CREATE);
			ASSERT_THAT(IsNotNull(Module));
			const std::string ScriptSource = SharedSource + EntrySource;
			ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
				ModuleName, ScriptSource.c_str(),
				static_cast<unsigned int>(ScriptSource.size()))));

			FCanonicalExecutionTrace Trace;
			int BuildResult = asSUCCESS;
			{
				FCanonicalTraceScope Scope(Trace);
				BuildResult = Module->Build();
			}
			const FString Messages = CollectMessages(Engine.GetMessages());
			ASSERT_THAT(IsTrue(BuildResult < 0,
				*FString::Printf(
					TEXT("LEGACY %s must remain rejected independently; result=%d messages=%s"),
					Form, BuildResult, *Messages)));
			ASSERT_THAT(IsTrue(Messages.Contains(ExpectedDiagnostic),
				*FString::Printf(
					TEXT("LEGACY %s must retain diagnostic '%s'. messages=%s"),
					Form, ExpectedDiagnostic, *Messages)));
			ASSERT_THAT(IsNull(Module->GetFunctionByDecl("int Entry()"),
				*FString::Printf(TEXT("failed LEGACY %s build must not publish Entry"), Form)));
			ASSERT_THAT(AreEqual(FString(), Trace.ToNormalizedString(),
				*FString::Printf(
					TEXT("LEGACY %s compile rejection must not execute Make/Get/Set"), Form)));
		};

		AssertRejected(
			"CanonicalASTVmPropertyPrefixTemporaryLegacyNegative",
			ASTEST_AS_ANSI("int Entry() { return ++Make().Value; }"),
			TEXT("Reference is temporary"),
			TEXT("temporary property prefix"));
		AssertRejected(
			"CanonicalASTVmPropertyPostfixTemporaryLegacyNegative",
			ASTEST_AS_ANSI("int Entry() { return Make().Value++; }"),
			TEXT("Reference is temporary"),
			TEXT("temporary property postfix"));
		AssertRejected(
			"CanonicalASTVmPropertyPostfixLocalLegacyNegative",
			ASTEST_AS_ANSI(
				"int Entry() { T Object = T(); Object.Value++; return Object.Stored; }"),
			TEXT("Property accessors cannot be used in combined read/write operations"),
			TEXT("local property postfix"));
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

			int MakeRhs()
			{
				Trace(4);
				return 1;
			}

			int RunMake()
			{
				Make().Value += MakeRhs();
				return 1;
			}

			int RunLocal()
			{
				T v = T();
				v.Value += 1;
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
			ASSERT_THAT(AreEqual(FString(TEXT("1,2,4,3")), Trace.ToNormalizedString(),
				*FString::Printf(TEXT("%s Make then Get then RHS then Set. trace=%s"),
					Side, *Trace.ToNormalizedString())));

			{
				FCanonicalTraceScope Scope(Trace);
				ASSERT_THAT(IsTrue(CanonicalExecuteInt(
					*TestRunner, Engine.Get(), Module, "int RunLocal()", Result)));
			}
			ASSERT_THAT(AreEqual(42, Result, *FString::Printf(TEXT("%s RunLocal Stored after += 1"), Side)));
			ASSERT_THAT(AreEqual(FString(TEXT("2,3")), Trace.ToNormalizedString(),
				*FString::Printf(TEXT("%s v.Value += 1 must Get then Set. trace=%s result=%d"),
					Side, *Trace.ToNormalizedString(), Result)));
		};

		{
			FNativeTestEngine LegacyEngine;
			LegacyEngine.Create(*TestRunner);
			ON_SCOPE_EXIT { LegacyEngine.Destroy(); };
			asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(LegacyEngine.Get());
			ASSERT_THAT(AreEqual(asSUCCESS,
				ScriptEngine->SetEngineProperty(asEP_PROPERTY_ACCESSOR_MODE, 2),
				TEXT("LEGACY property fixture enables the shared GetX/SetX accessor profile")));
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
			AssertLegacyCompilerProvenance(
				static_cast<asIScriptModule*>(Module), TEXT("LEGACY property fixture"));
			RunBoth(LegacyEngine, Module, TEXT("LEGACY"));
		}

		{
			FNativeTestEngine CanonicalEngine;
			CanonicalEngine.Create(*TestRunner);
			ON_SCOPE_EXIT { CanonicalEngine.Destroy(); };
			asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(CanonicalEngine.Get());
			ASSERT_THAT(AreEqual(asSUCCESS,
				ScriptEngine->SetEngineProperty(asEP_PROPERTY_ACCESSOR_MODE, 2),
				TEXT("CANONICAL property fixture enables the shared GetX/SetX accessor profile")));
			ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL)));
			ASSERT_THAT(AreEqual(asCOMPILER_PIPELINE_CANONICAL, ScriptEngine->GetCompilerPipeline()));
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
			AssertCanonicalCodeGenProvenance(
				static_cast<asIScriptModule*>(Module), TEXT("CANONICAL property fixture"));
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
						Total += 100;
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
		if (Context == nullptr)
		{
			return;
		}
		const auto ExecuteSelector = [this, Context, Function](
			const asDWORD Selector,
			const int32 Expected,
			const TCHAR* const Meaning)
		{
			const FString Label = FString::Printf(
				TEXT("Selector=%u (%s)"), Selector, Meaning);
			ASSERT_THAT(AreEqual(0, Context->Prepare(Function),
				*FString::Printf(TEXT("Prepare %s"), *Label)));
			ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, Selector),
				*FString::Printf(TEXT("Set %s"), *Label)));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				Context->Execute(),
				*FString::Printf(TEXT("Execute %s"), *Label)));
			const int32 Actual =
				static_cast<int32>(Context->GetReturnDWord());
			ASSERT_THAT(AreEqual(
				Expected,
				Actual,
				*FString::Printf(
					TEXT("Result %s expected=%d actual=%d"),
					*Label, Expected, Actual)));
		};
		ExecuteSelector(1, 305,
			TEXT("break exits switch and executes the observable loop tail"));
		ExecuteSelector(2, 32,
			TEXT("continue targets for and skips the observable loop tail"));
		ExecuteSelector(0, 0,
			TEXT("default return exits before later loops"));

		const asCASTContext* const AST =
			static_cast<asCModule*>(static_cast<asIScriptModule*>(Module))->GetCanonicalASTContext();
		ASSERT_THAT(IsNotNull(AST, TEXT("control-flow module must retain its sealed canonical AST")));
		if (AST == nullptr)
		{
			return;
		}

		const asCStmt* ForStatement = nullptr;
		const asCStmt* SwitchStatement = nullptr;
		int32 ForCount = 0;
		int32 SwitchCount = 0;
		int32 WhileCount = 0;
		int32 DoWhileCount = 0;
		int32 CaseCount = 0;
		int32 FunctionEntryCount = 0;
		int32 LoopEntryCount = 0;
		int32 LoopBackedgeCount = 0;
		int32 ReturnCount = 0;
		bool bAllCasesTargetSwitch = true;
		bool bBreakTargetsSwitch = false;
		bool bContinueTargetsFor = false;
		bool bAllTransfersHaveExactRole = true;
		bool bAllReturnsHaveExactRole = true;
		for (asUINT Index = 1; Index <= AST->GetStmtCount(); ++Index)
		{
			const asCStmt* const Statement = AST->GetStmt(asASTStmtId(Index));
			if (Statement == nullptr)
			{
				continue;
			}
			if (Statement->kind == asAST_STMT_FOR)
			{
				++ForCount;
				ForStatement = Statement;
			}
			else if (Statement->kind == asAST_STMT_SWITCH)
			{
				++SwitchCount;
				SwitchStatement = Statement;
			}
			else if (Statement->kind == asAST_STMT_WHILE)
			{
				++WhileCount;
			}
			else if (Statement->kind == asAST_STMT_DO_WHILE)
			{
				++DoWhileCount;
			}
			else if (Statement->kind == asAST_STMT_CASE)
			{
				++CaseCount;
			}
			else if (Statement->kind == asAST_STMT_RETURN)
			{
				++ReturnCount;
				bAllReturnsHaveExactRole = bAllReturnsHaveExactRole
					&& Statement->safePointRole == asAST_SAFEPOINT_RETURN;
			}

			if (Statement->safePointRole == asAST_SAFEPOINT_FUNCTION_ENTRY)
			{
				++FunctionEntryCount;
			}
			else if (Statement->safePointRole == asAST_SAFEPOINT_LOOP_BACKEDGE)
			{
				++LoopBackedgeCount;
			}
			if (Statement->loopBodySafePointRole == asAST_SAFEPOINT_LOOP_ENTRY)
			{
				++LoopEntryCount;
			}
		}
		ASSERT_THAT(AreEqual(1, ForCount, TEXT("exactly one canonical for")));
		ASSERT_THAT(AreEqual(1, SwitchCount, TEXT("exactly one canonical switch")));
		ASSERT_THAT(AreEqual(1, WhileCount, TEXT("exactly one canonical while")));
		ASSERT_THAT(AreEqual(1, DoWhileCount, TEXT("exactly one canonical do-while")));
		ASSERT_THAT(AreEqual(3, CaseCount, TEXT("two cases plus default")));
		ASSERT_THAT(AreEqual(1, FunctionEntryCount,
			TEXT("function body owns the exact entry role")));
		ASSERT_THAT(AreEqual(3, LoopEntryCount,
			TEXT("for/while/do body edges own exact loop-entry roles")));
		ASSERT_THAT(AreEqual(3, LoopBackedgeCount,
			TEXT("for/while/do loops own exact backedge roles")));
		ASSERT_THAT(AreEqual(2, ReturnCount,
			TEXT("default and final return are both sealed")));
		ASSERT_THAT(IsTrue(bAllReturnsHaveExactRole,
			TEXT("all returns own Return roles")));
		ASSERT_THAT(IsTrue(SwitchStatement != nullptr
			&& SwitchStatement->safePointRole == asAST_SAFEPOINT_STATEMENT,
			TEXT("authored switch owns the Statement role")));

		for (asUINT Index = 1; Index <= AST->GetStmtCount(); ++Index)
		{
			const asCStmt* const Statement = AST->GetStmt(asASTStmtId(Index));
			if (Statement == nullptr)
			{
				continue;
			}
			if (Statement->kind == asAST_STMT_CASE)
			{
				bAllCasesTargetSwitch = bAllCasesTargetSwitch
					&& SwitchStatement != nullptr
					&& Statement->target == SwitchStatement->id
					&& Statement->safePointRole == asAST_SAFEPOINT_STATEMENT;
			}
			else if (Statement->kind == asAST_STMT_BREAK)
			{
				bAllTransfersHaveExactRole = bAllTransfersHaveExactRole
					&& Statement->safePointRole == asAST_SAFEPOINT_TRANSFER;
				bBreakTargetsSwitch = SwitchStatement != nullptr
					&& Statement->target == SwitchStatement->id;
			}
			else if (Statement->kind == asAST_STMT_CONTINUE)
			{
				bAllTransfersHaveExactRole = bAllTransfersHaveExactRole
					&& Statement->safePointRole == asAST_SAFEPOINT_TRANSFER;
				bContinueTargetsFor = ForStatement != nullptr
					&& Statement->target == ForStatement->id;
			}
		}
		ASSERT_THAT(IsTrue(bAllCasesTargetSwitch,
			TEXT("all cases own Statement roles and target the exact switch")));
		ASSERT_THAT(IsTrue(bAllTransfersHaveExactRole,
			TEXT("break/continue own exact Transfer roles")));
		ASSERT_THAT(IsTrue(bBreakTargetsSwitch,
			TEXT("break targets the nearest switch")));
		ASSERT_THAT(IsTrue(bContinueTargetsFor,
			TEXT("continue targets the nearest loop, not the switch")));
	}

	TEST_METHOD(LocalCompoundTailKeepsOneDeclAndFreshFrameAcrossContextReuse)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("the reduced Tail probe selects CANONICAL")));

		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"CanonicalASTVmLocalCompoundTail",
			ASTEST_AS_ANSI(R"AS(
				int Probe(bool Mutate)
				{
					int Tail = 0;
					if (Mutate)
						Tail += 100;
					return Tail;
				}
			)AS"),
			asAST_RETAIN_SNAPSHOT);
		ASSERT_THAT(IsTrue(Module.IsValid(),
			TEXT("the reduced Tail probe must compile through Canonical CodeGen")));
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}
		AssertCanonicalCodeGenProvenance(Module, TEXT("the reduced Tail probe"));

		asIScriptFunction* const Function =
			GetNativeFunctionByDecl(Module, "int Probe(bool)");
		ASSERT_THAT(IsNotNull(Function,
			*FString::Printf(
				TEXT("the reduced Tail function must exist; functions={%s}; diagnostics={%s}"),
				*CollectFunctionDeclarations(Module), *Engine.GetMessagesText())));
		if (Function == nullptr)
		{
			return;
		}

		const auto ExecuteFresh = [this, &Engine, Function](
			const bool bMutate,
			const int32 Expected,
			const TCHAR* const Label)
		{
			asIScriptContext* const Context = Engine.Get()->CreateContext();
			ASSERT_THAT(IsNotNull(Context,
				*FString::Printf(TEXT("fresh context for %s"), Label)));
			if (Context == nullptr)
			{
				return;
			}
			ON_SCOPE_EXIT { Context->Release(); };
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Function)));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->SetArgByte(0, bMutate ? 1 : 0)));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				Context->Execute(),
				*FString::Printf(TEXT("fresh execution for %s"), Label)));
			ASSERT_THAT(AreEqual(
				Expected,
				static_cast<int32>(Context->GetReturnDWord()),
				*FString::Printf(TEXT("fresh result for %s"), Label)));
		};
		ExecuteFresh(false, 0, TEXT("Mutate=false"));
		ExecuteFresh(true, 100, TEXT("Mutate=true"));

		asIScriptContext* const ReusedContext = Engine.Get()->CreateContext();
		ASSERT_THAT(IsNotNull(ReusedContext,
			TEXT("the reduced Tail probe needs one reusable VM context")));
		ON_SCOPE_EXIT { if (ReusedContext != nullptr) { ReusedContext->Release(); } };
		if (ReusedContext == nullptr)
		{
			return;
		}
		const auto ExecuteReused = [this, ReusedContext, Function](
			const bool bMutate,
			const int32 Expected,
			const TCHAR* const Label)
		{
			ASSERT_THAT(AreEqual(asSUCCESS, ReusedContext->Prepare(Function)));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				ReusedContext->SetArgByte(0, bMutate ? 1 : 0)));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				ReusedContext->Execute(),
				*FString::Printf(TEXT("reused execution for %s"), Label)));
			ASSERT_THAT(AreEqual(
				Expected,
				static_cast<int32>(ReusedContext->GetReturnDWord()),
				*FString::Printf(TEXT("reused result for %s"), Label)));
		};
		ExecuteReused(false, 0, TEXT("first false"));
		ExecuteReused(true, 100, TEXT("middle true"));
		ExecuteReused(false, 0, TEXT("final false"));

		const asCASTContext* const AST =
			static_cast<asCModule*>(static_cast<asIScriptModule*>(Module))
				->GetCanonicalASTContext();
		ASSERT_THAT(IsNotNull(AST,
			TEXT("the reduced Tail probe must retain its sealed Canonical AST")));
		if (AST == nullptr)
		{
			return;
		}

		const asCDecl* TailDecl = nullptr;
		int32 TailDeclCount = 0;
		for (asUINT DeclIndex = 1; DeclIndex <= AST->GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Decl = AST->GetDecl(asASTDeclId(DeclIndex));
			if (Decl != nullptr
				&& Decl->kind == asAST_DECL_VAR
				&& Decl->name.Equals("Tail"))
			{
				TailDecl = Decl;
				++TailDeclCount;
			}
		}

		const asCStmt* TailDeclStatement = nullptr;
		const asCStmt* ReturnStatement = nullptr;
		for (asUINT StmtIndex = 1; StmtIndex <= AST->GetStmtCount(); ++StmtIndex)
		{
			const asCStmt* const Statement = AST->GetStmt(asASTStmtId(StmtIndex));
			if (Statement == nullptr)
			{
				continue;
			}
			if (TailDecl != nullptr
				&& Statement->kind == asAST_STMT_DECL
				&& Statement->decl == TailDecl->id)
			{
				TailDeclStatement = Statement;
			}
			else if (Statement->kind == asAST_STMT_RETURN)
			{
				ReturnStatement = Statement;
			}
		}

		const asCExpr* InitializerAssign = nullptr;
		const asCExpr* CompoundAssign = nullptr;
		for (asUINT ExprIndex = 1; ExprIndex <= AST->GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Expr = AST->GetExpr(asASTExprId(ExprIndex));
			if (Expr != nullptr
				&& Expr->kind == asAST_EXPR_ASSIGN
				&& Expr->literal.Equals("=")
				&& Expr->children.GetLength() == 2
				&& TailDecl != nullptr
				&& TailDecl->inits.GetLength() == 1
				&& Expr->children[1] == TailDecl->inits[0])
			{
				ASSERT_THAT(IsNull(InitializerAssign,
					TEXT("the reduced fixture must contain one Tail initializer assignment")));
				InitializerAssign = Expr;
			}
			else if (Expr != nullptr
				&& Expr->kind == asAST_EXPR_ASSIGN
				&& Expr->literal.Equals("+="))
			{
				ASSERT_THAT(IsNull(CompoundAssign,
					TEXT("the reduced fixture must contain exactly one compound assignment")));
				CompoundAssign = Expr;
			}
		}

		const asCExpr* const Initializer = TailDecl != nullptr
			&& TailDecl->inits.GetLength() == 1
			? AST->GetExpr(TailDecl->inits[0]) : nullptr;
		const asCExpr* const InitializerLhs = InitializerAssign != nullptr
			&& InitializerAssign->children.GetLength() == 2
			? AST->GetExpr(InitializerAssign->children[0]) : nullptr;
		const asCExpr* const CompoundLhs = CompoundAssign != nullptr
			&& CompoundAssign->children.GetLength() == 2
			? AST->GetExpr(CompoundAssign->children[0]) : nullptr;
		const asCExpr* const ReturnExpr = ReturnStatement != nullptr
			&& ReturnStatement->expr.IsValid()
			? AST->GetExpr(ReturnStatement->expr) : nullptr;

		ASSERT_THAT(IsTrue(
			TailDeclCount == 1
			&& TailDecl != nullptr
			&& TailDeclStatement != nullptr
			&& Initializer != nullptr
			&& Initializer->kind == asAST_EXPR_INTEGER_LITERAL
			&& Initializer->literalBits == 0
			&& InitializerLhs != nullptr
			&& InitializerLhs->kind == asAST_EXPR_DECL_REF
			&& InitializerLhs->resolvedDecl == TailDecl->id,
			TEXT("the declaration, initializer write, and zero RHS must belong to one exact Tail VarDecl")));
		ASSERT_THAT(IsTrue(
			CompoundAssign != nullptr
			&& CompoundLhs != nullptr
			&& CompoundLhs->kind == asAST_EXPR_DECL_REF
			&& CompoundLhs->resolvedDecl == TailDecl->id,
			TEXT("Tail += 100 must write the exact initialized Tail DeclId")));
		ASSERT_THAT(IsTrue(
			ReturnExpr != nullptr
			&& ReturnExpr->kind == asAST_EXPR_DECL_REF
			&& ReturnExpr->resolvedDecl == TailDecl->id,
			TEXT("return Tail must read the same exact initialized Tail DeclId")));
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
