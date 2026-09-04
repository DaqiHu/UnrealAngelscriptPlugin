#include "CQTest.h"

#include "AngelscriptTestJITProbes.h"
#include "Core/AngelscriptBinds.h"
#include "Core/angelscript.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotGeneration.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallBridge.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "source/as_thread.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotBenchmarkTests,
	"Angelscript.TestModule.StaticJIT.AOT.Benchmarks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static int32 BridgeAdd(const int32 Left, const int32 Right)
	{
		return Left + Right;
	}

	static double NanosecondsPerCall(
		const uint64 StartCycles,
		const uint64 EndCycles,
		const int32 Iterations)
	{
		if (Iterations <= 0)
		{
			return 0.0;
		}
		const double Seconds = FPlatformTime::ToSeconds64(EndCycles - StartCycles);
		return (Seconds * 1.0e9) / static_cast<double>(Iterations);
	}

	static bool WriteBenchmarkCsv(
		const AngelscriptStaticJITAotGeneration::FStaticJITAotFirstVersionBenchmark&
			Generation,
		const double ScalarNs,
		const double DirectNs,
		const double BridgedNs,
		const int32 ScalarResult,
		const int32 DirectResult,
		const int32 BridgedResult,
		FString& OutPath)
	{
		const FString Directory = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("CanonicalTypedASTAOT/Benchmarks")));
		IFileManager::Get().MakeDirectory(*Directory, true);
		OutPath = FPaths::Combine(Directory, TEXT("first-version.csv"));
		const FString Csv = FString::Printf(
			TEXT("metric,value,unit,source\n")
			TEXT("canonical_ast_build_ms,%.3f,ms,CreateFixtureEngine canonical typed-ast Core generation Engine\n")
			TEXT("canonical_ast_working_set_delta_bytes,%lld,bytes,FPlatformMemory UsedPhysical after canonical generation Engine\n")
			TEXT("isolated_bytecode_generation_ms,%.3f,ms,AppendIsolatedDifferentialArtifact BytecodeJIT DifferentialScalarValue\n")
			TEXT("isolated_typed_generation_ms,%.3f,ms,AppendIsolatedDifferentialArtifact TypedASTJIT DifferentialScalarValue\n")
			TEXT("isolated_bytecode_generated_cpp_bytes,%d,bytes,BenchmarkBytecode.generated.cpp\n")
			TEXT("isolated_typed_generated_cpp_bytes,%d,bytes,BenchmarkTypedAST.generated.cpp\n")
			TEXT("scalar_raw_ns_per_call,%.3f,ns,AngelscriptTestJITProbes::ExecuteTypedASTScalarProviderRaw 256 iters\n")
			TEXT("direct_capability_raw_ns_per_call,%.3f,ns,ExecuteTypedASTCapabilityProviderRaw 256 iters\n")
			TEXT("bridged_native_ns_per_call,%.3f,ns,ExecuteTypedASTNativeBridge 256 iters\n")
			TEXT("scalar_raw_result,%d,int,ExecuteTypedASTScalarProviderRaw(3,4)\n")
			TEXT("direct_capability_raw_result,%d,int,ExecuteTypedASTCapabilityProviderRaw(3,4,true,true)\n")
			TEXT("bridged_native_result,%d,int,ExecuteTypedASTNativeBridge(19,23)\n"),
			Generation.CanonicalASTBuildMilliseconds,
			static_cast<long long>(Generation.CanonicalASTWorkingSetDeltaBytes),
			Generation.IsolatedBytecodeGenerationMilliseconds,
			Generation.IsolatedTypedGenerationMilliseconds,
			Generation.IsolatedBytecodeGeneratedCppBytes,
			Generation.IsolatedTypedGeneratedCppBytes,
			ScalarNs,
			DirectNs,
			BridgedNs,
			ScalarResult,
			DirectResult,
			BridgedResult);
		return FFileHelper::SaveStringToFile(Csv, *OutPath);
	}

public:
	TEST_METHOD(RecordsFirstVersionCanonicalASTGenerationAndExecutionTimings)
	{
		using namespace AngelscriptStaticJITAotGeneration;
		const FStaticJITAotFirstVersionBenchmark Generation =
			RunFirstVersionBenchmark();
		ASSERT_THAT(IsTrue(Generation.bSuccess, Generation.Error));
		ASSERT_THAT(IsTrue(Generation.CanonicalASTBuildMilliseconds > 0.0,
			TEXT("Canonical AST build must record a positive duration")));
		ASSERT_THAT(IsTrue(
			Generation.IsolatedBytecodeGenerationMilliseconds > 0.0));
		ASSERT_THAT(IsTrue(
			Generation.IsolatedTypedGenerationMilliseconds > 0.0));
		ASSERT_THAT(IsTrue(
			Generation.IsolatedBytecodeGeneratedCppBytes > 0));
		ASSERT_THAT(IsTrue(
			Generation.IsolatedTypedGeneratedCppBytes > 0));

		ASSERT_THAT(IsTrue(
			AngelscriptTestJITProbes::HasTypedASTScalarProviderEntries(),
			TEXT("Scalar Raw probe must be registered by the shipped TestJIT Provider")));
		ASSERT_THAT(IsTrue(
			AngelscriptTestJITProbes::HasTypedASTCapabilityProviderEntries(),
			TEXT("Capability Raw probe must be registered by the shipped TestJIT Provider")));

		constexpr int32 Warmup = 16;
		constexpr int32 Iterations = 256;
		int32 ScalarResult = 0;
		for (int32 Index = 0; Index < Warmup; ++Index)
		{
			ScalarResult =
				AngelscriptTestJITProbes::ExecuteTypedASTScalarProviderRaw(3, 4);
		}
		uint64 Start = FPlatformTime::Cycles64();
		for (int32 Index = 0; Index < Iterations; ++Index)
		{
			ScalarResult =
				AngelscriptTestJITProbes::ExecuteTypedASTScalarProviderRaw(3, 4);
		}
		const double ScalarNs = NanosecondsPerCall(
			Start, FPlatformTime::Cycles64(), Iterations);
		ASSERT_THAT(AreEqual(6, ScalarResult,
			TEXT("Scalar Raw (3,4) must stay (3+4)-1=6")));

		int32 DirectResult = 0;
		for (int32 Index = 0; Index < Warmup; ++Index)
		{
			DirectResult =
				AngelscriptTestJITProbes::ExecuteTypedASTCapabilityProviderRaw(
					3, 4, true, true);
		}
		Start = FPlatformTime::Cycles64();
		for (int32 Index = 0; Index < Iterations; ++Index)
		{
			DirectResult =
				AngelscriptTestJITProbes::ExecuteTypedASTCapabilityProviderRaw(
					3, 4, true, true);
		}
		const double DirectNs = NanosecondsPerCall(
			Start, FPlatformTime::Cycles64(), Iterations);
		ASSERT_THAT(IsTrue(DirectResult != 0,
			TEXT("Capability Raw path must execute the shipped header-inline body")));

		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine BridgeEngine;
		BridgeEngine.Create(*TestRunner);
		ON_SCOPE_EXIT { BridgeEngine.Destroy(); };
		asIScriptEngine* const ScriptEngine = BridgeEngine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		const ASAutoCaller::FunctionCaller Caller =
			ASAutoCaller::MakeFunctionCaller(BridgeAdd);
		const int32 FunctionId = ScriptEngine->RegisterGlobalFunction(
			"int NativeBridgeAdd(int Left, int Right)",
			asFUNCTION(BridgeAdd),
			asCALL_CDECL,
			*(asFunctionCaller*)&Caller);
		asCScriptFunction* const Function = FunctionId >= 0
			? static_cast<asCScriptFunction*>(
				ScriptEngine->GetFunctionById(FunctionId))
			: nullptr;
		ASSERT_THAT(IsNotNull(Function,
			TEXT("Bridged-call benchmark must register a real system function")));
		if (Function == nullptr)
		{
			return;
		}

		FAngelscriptTypedASTJITBoundCallSite CallSite;
		CallSite.CanonicalDeclaration =
			TEXT("int NativeBridgeAdd(int Left, int Right)");
		CallSite.RegisteredTarget = TEXT("benchmark-bridge");
		CallSite.ReferenceSlotIndex = 0;
		FAngelscriptJITResolvedReferenceTable Table;
		FAngelscriptJITResolvedReferenceSlot& Slot =
			Table.Slots.AddDefaulted_GetRef();
		Slot.SlotIndex = 0;
		Slot.Kind = EAngelscriptArtifactReferenceKind::EnvironmentSymbol;
		Slot.Value = Function;
		FScriptExecution Execution(asCThreadManager::GetLocalData());
		Execution.jitEngine = ScriptEngine;
		Execution.jitReferenceTable = &Table;
		int32 BridgedResult = 0;
		for (int32 Index = 0; Index < Warmup; ++Index)
		{
			BridgedResult = AngelscriptTestJITProbes::ExecuteTypedASTNativeBridge(
				Execution, CallSite, 19, 23);
		}
		Start = FPlatformTime::Cycles64();
		for (int32 Index = 0; Index < Iterations; ++Index)
		{
			BridgedResult = AngelscriptTestJITProbes::ExecuteTypedASTNativeBridge(
				Execution, CallSite, 19, 23);
		}
		const double BridgedNs = NanosecondsPerCall(
			Start, FPlatformTime::Cycles64(), Iterations);
		ASSERT_THAT(AreEqual(42, BridgedResult,
			TEXT("Bridged native call must reach InvokeBoundViaVM and return 19+23")));

		FString CsvPath;
		ASSERT_THAT(IsTrue(WriteBenchmarkCsv(
			Generation,
			ScalarNs,
			DirectNs,
			BridgedNs,
			ScalarResult,
			DirectResult,
			BridgedResult,
			CsvPath), CsvPath));
		ASSERT_THAT(IsTrue(FPaths::FileExists(CsvPath), CsvPath));
	}
};

#endif
