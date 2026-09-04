#include "CQTest.h"

#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "StaticJIT/AngelscriptJITGeneration.h"
#include "StaticJIT/AngelscriptStaticJITEntryPlan.h"
#include "StaticJIT/AngelscriptStaticJITGenerator.h"
#include "StaticJIT/Backends/AngelscriptStaticJITBackend.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITEntryPlanBytecodeConsumerTests,
	"Angelscript.TestModule.StaticJIT.EntryPlan.BytecodeConsumer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FAngelscriptHash256 MakeStableHash(const FStringView Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("static-jit-entry-plan-bytecode-consumer-test"));
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(FrozenScalarCarrierAndVmOffsetDriveBytecodeEntryEmission)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FName ModuleName(
			TEXT("ASStaticJITSharedEntryPlanBytecodeConsumer"));
		const FString ScriptSource = ASTEST_AS(R"AS(
			int ConsumeFrozenEntryPlan(int Value, int Other)
			{
				return Value + Other;
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASStaticJITSharedEntryPlanBytecodeConsumer"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(),
			TEXT("The shared Entry Plan Bytecode consumer fixture should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asCScriptFunction* const Function = static_cast<asCScriptFunction*>(
			ModuleScope.GetModule().GetFunctionByName("ConsumeFrozenEntryPlan"));
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}
		const TSharedPtr<FAngelscriptModuleDesc> ModuleDesc =
			Engine.GetModuleByModuleName(ModuleName.ToString());
		ASSERT_THAT(IsTrue(ModuleDesc.IsValid()));
		if (!ModuleDesc.IsValid())
		{
			return;
		}

		FStaticJITEntryPlan FrozenPlan;
		FString Error;
		ASSERT_THAT(IsTrue(BuildAngelscriptStaticJITEntryPlan(
			*Function, true, true, true, FrozenPlan, &Error), Error));
		ASSERT_THAT(AreEqual(2, FrozenPlan.Parameters.Num()));
		if (FrozenPlan.Parameters.Num() != 2)
		{
			return;
		}
		FStaticJITEntryPlan LegacyCompatibilityPlan = FrozenPlan;
		LegacyCompatibilityPlan.bTypedAdapterRepresentable = false;
		LegacyCompatibilityPlan.TypedAdapterFailure =
			TEXT("TestLegacyCompatibilityAdapter");
		FinalizeAngelscriptStaticJITEntryPlan(LegacyCompatibilityPlan);

		FAngelscriptStableModuleKey StableModuleKey;
		StableModuleKey.Hash = MakeStableHash(TEXTVIEW("module"));
		FAngelscriptStableFunctionKey StableFunctionKey;
		StableFunctionKey.Hash = MakeStableHash(TEXTVIEW("function"));

		FAngelscriptStaticJITCompiledFunctionView FunctionView;
		FunctionView.EngineLocalFunction = Function;
		FunctionView.ModuleKey = StableModuleKey;
		FunctionView.FunctionKey = StableFunctionKey;
		FunctionView.ExecutionHash = MakeStableHash(TEXTVIEW("execution"));
		FunctionView.DebugHash = MakeStableHash(TEXTVIEW("debug"));
		FunctionView.EntryAbiHash =
			FAngelscriptJITGeneration::BuildEntryAbiHash(
				FrozenPlan.CanonicalDeclaration,
				FrozenPlan.bHasVMEntry,
				FrozenPlan.bHasRawEntry,
				FrozenPlan.bHasParmsEntry);
		FunctionView.CanonicalDeclaration = FrozenPlan.CanonicalDeclaration;
		FunctionView.bHasVerifiedArtifactIdentity = true;
		FunctionView.bHasVerifiedArtifactReferences = true;
		FunctionView.bHasVMEntry = FrozenPlan.bHasVMEntry;
		FunctionView.bHasRawEntry = FrozenPlan.bHasRawEntry;
		FunctionView.bHasParmsEntry = FrozenPlan.bHasParmsEntry;
		FunctionView.EntryPlan = &FrozenPlan;

		asIScriptModule* const Modules[] = {&ModuleScope.GetModule()};
		const FAngelscriptModuleDesc* const ModuleDescriptors[] = {
			ModuleDesc.Get()};
		FAngelscriptStaticJITCompiledFunctionView Functions[] = {
			FunctionView};
		FAngelscriptStaticJITCompiledSourceGraph Graph;
		Graph.Settings.TargetProfile =
			EAngelscriptJITTargetProfile::EditorDevelopment;
		Graph.Settings.ProviderId.Hash = MakeStableHash(TEXTVIEW("provider"));
		Graph.Settings.ProviderName = TEXT("EntryPlanBytecodeConsumer");
		Graph.Settings.OwnerModuleName = TEXT("AngelscriptTest");
		Graph.Settings.ArtifactProfile.Hash = MakeStableHash(TEXTVIEW("profile"));
		Graph.Settings.NativeEnvironmentFingerprint =
			MakeStableHash(TEXTVIEW("environment"));
		Graph.Modules = Modules;
		Graph.ResolvedModuleDescriptors = ModuleDescriptors;
		Graph.Functions = Functions;
		Graph.CaptureProfile = EAngelscriptStaticJITCaptureProfile::Bytecode;

		ASSERT_THAT(IsTrue(RegisterAngelscriptBytecodeJITBackend(&Error), Error));
		const IAngelscriptStaticJITBackendFactory* const Factory =
			GetAngelscriptStaticJITBackendRegistry().FindFactory(
				FAngelscriptStaticJITBackendId::Bytecode());
		ASSERT_THAT(IsNotNull(Factory));
		if (Factory == nullptr)
		{
			return;
		}
		TUniquePtr<IAngelscriptStaticJITBackend> Backend =
			Factory->CreateBackend();
		ASSERT_THAT(IsNotNull(Backend.Get()));
		if (!Backend.IsValid())
		{
			return;
		}

		const FAngelscriptStaticJITBackendRequest Request{
			FAngelscriptStaticJITBackendId::Bytecode(), Graph, {}};
		auto TryGenerateBackendSource = [
			&Backend,
			&Error,
			&Functions,
			&Request](
			const FStaticJITEntryPlan* const EntryPlan,
			FString& OutSource)
		{
			Functions[0].EntryPlan = EntryPlan;
			FAngelscriptStaticJITBackendOutput Output;
			if (!Backend->Generate(Request, Output, &Error))
			{
				return false;
			}
			if (Output.Functions.Num() != 1
				|| !Output.Functions[0].EmittedFunction.IsSet())
			{
				Error = TEXT("Bytecode backend did not emit exactly one function");
				return false;
			}
			OutSource =
				Output.Functions[0].EmittedFunction->ImplementationTemplate;
			return true;
		};

		FString LegacyEntryText;
		const bool bGeneratedLegacyEntry =
			TryGenerateBackendSource(
				&LegacyCompatibilityPlan, LegacyEntryText);
		ASSERT_THAT(IsTrue(bGeneratedLegacyEntry, Error));
		if (!bGeneratedLegacyEntry)
		{
			return;
		}
		FString SharedPlanEntryText;
		const bool bGeneratedSharedPlanEntry =
			TryGenerateBackendSource(&FrozenPlan, SharedPlanEntryText);
		ASSERT_THAT(IsTrue(bGeneratedSharedPlanEntry, Error));
		if (!bGeneratedSharedPlanEntry)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			LegacyEntryText,
			SharedPlanEntryText,
			TEXT("An unmodified shared Entry Plan must preserve Bytecode entry text byte-for-byte")));

		// A generation snapshot is authoritative after the generation Engine has
		// frozen it. The carrier deliberately differs from the live function, and
		// the two real same-type VM slots are swapped. Keeping both valid slots in
		// the local map lets the body generator finish while still proving that the
		// entry adapter does not silently re-derive either value from the Engine.
		const int32 OriginalValueOffset =
			FrozenPlan.Parameters[0].VMFrameOffsetDWords;
		const int32 OriginalOtherOffset =
			FrozenPlan.Parameters[1].VMFrameOffsetDWords;
		ASSERT_THAT(IsTrue(OriginalValueOffset != OriginalOtherOffset));
		FrozenPlan.Parameters[0].EntryCppType =
			TEXT("FSharedEntryPlanCarrier");
		FrozenPlan.Parameters[0].VMFrameOffsetDWords = OriginalOtherOffset;
		FrozenPlan.Parameters[1].VMFrameOffsetDWords = OriginalValueOffset;
		FinalizeAngelscriptStaticJITEntryPlan(FrozenPlan);

		FString Source;
		const bool bGeneratedFrozenPlanEntry =
			TryGenerateBackendSource(&FrozenPlan, Source);
		ASSERT_THAT(IsTrue(bGeneratedFrozenPlanEntry, Error));
		if (!bGeneratedFrozenPlanEntry)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Source.Contains(
			TEXT("FSharedEntryPlanCarrier p_Value")),
			TEXT("The raw Bytecode entry must use the frozen carrier spelling")));
		const FString FrozenOffsetExpression = FString::Printf(
			TEXT("*(FSharedEntryPlanCarrier*)(l_fp + %d)"),
			OriginalOtherOffset);
		ASSERT_THAT(IsTrue(Source.Contains(FrozenOffsetExpression),
			TEXT("The VM Bytecode entry must use the frozen frame offset")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT
