#include "CQTest.h"

#include "StaticJIT/AngelscriptJITGeneration.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_WITH_STATIC_JIT_DIAGNOSTICS

namespace AngelscriptStaticJITAotGeneratedProviderDiagnosticsTests_Private
{
	FAngelscriptHash256 MakeHash(const uint8 Value)
	{
		FBlake3Hash::ByteArray Bytes{};
		for (uint8& Byte : Bytes)
		{
			Byte = Value;
		}
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	FAngelscriptJITGenerationRequest MakeRequest(
		const EAngelscriptJITTargetProfile TargetProfile)
	{
		FAngelscriptJITGenerationRequest Request;
		Request.TargetProfile = TargetProfile;
		Request.ProviderId.Hash = MakeHash(0x11);
		Request.ProviderName = TEXT("GeneratedDiagnosticProvider");
		Request.OwnerModuleName = TEXT("AngelscriptTestJIT");
		Request.ArtifactProfile.Hash = MakeHash(0x22);
		Request.NativeEnvironmentFingerprint = MakeHash(0x33);

		FAngelscriptJITGenerationModule& Module =
			Request.Modules.AddDefaulted_GetRef();
		Module.ModuleKey.Hash = MakeHash(0x44);
		Module.CanonicalModuleName = TEXT("Diagnostics.Generated");
		Module.VirtualSourcePath =
			TEXT("/Angelscript/Game/Diagnostics/Generated.as");

		FAngelscriptJITGenerationFunction& Function =
			Request.Functions.AddDefaulted_GetRef();
		Function.ModuleKey = Module.ModuleKey;
		Function.FunctionKey.Hash = MakeHash(0x55);
		Function.ExecutionHash = MakeHash(0x66);
		Function.DebugHash = MakeHash(0x77);
		Function.EntryAbiHash = MakeHash(0x88);
		Function.CanonicalDeclaration = TEXT("int GeneratedDiagnostics(int Value)");
		Function.SourceSection = Module.VirtualSourcePath;
		Function.SourceLine = 7;
		Function.SourceColumn = 2;
		Function.bHasVMEntry = true;
		Function.ImplementationTemplate =
			TEXT("{VM_ENTRY_COMMENT}void {VM_SYMBOL}(FScriptExecution&, asDWORD*, asQWORD*) {}\n");

		FAngelscriptJITGenerationDiagnostics& Diagnostics =
			Request.Diagnostics.Emplace();
		Diagnostics.RequestedBackendId = TEXT("typed-ast");
		Diagnostics.CaptureState = TEXT("VerifiedCanonicalAST");
		Diagnostics.FallbackChain = {TEXT("typed-ast"), TEXT("bytecode")};
		Diagnostics.SourceCompilePassCount = 1;
		Diagnostics.CompiledModuleCount = 1;
		Diagnostics.CompiledFunctionCount = 1;
		Diagnostics.EmitModuleCount = 1;
		Diagnostics.bHasVerifiedCanonicalAST = true;

		FAngelscriptJITGenerationFunctionDiagnostic& FunctionDiagnostic =
			Diagnostics.Functions.AddDefaulted_GetRef();
		FunctionDiagnostic.FunctionKey = Function.FunctionKey;
		FunctionDiagnostic.ActualBackendId = TEXT("typed-ast");
		FunctionDiagnostic.Disposition = TEXT("Emitted");
		FunctionDiagnostic.Detail = TEXT("typed function emitted");

		FAngelscriptJITGenerationBackendAttemptDiagnostic& Attempt =
			FunctionDiagnostic.BackendAttempts.AddDefaulted_GetRef();
		Attempt.BackendId = TEXT("typed-ast");
		Attempt.Disposition = TEXT("Emitted");
		Attempt.Detail = TEXT("typed attempt emitted");
		Attempt.bHasTypedDiagnostic = true;
		Attempt.FallbackReason = TEXT("None");
		Attempt.TypedDetail = TEXT("eligible");
		Attempt.ProcessedSource.Section = Module.VirtualSourcePath;
		Attempt.ProcessedSource.Row = Function.SourceLine;
		Attempt.ProcessedSource.Column = Function.SourceColumn;
		Attempt.ProfileKind = TEXT("PositionOnly");
		Attempt.ExceptionControlState = TEXT("DirectExecutionState");
		Attempt.ExceptionPayloadState = TEXT("DirectFailureRecord");

		FAngelscriptJITGenerationCanonicalFunctionDiagnostic& Semantic =
			Attempt.SemanticFunctions.AddDefaulted_GetRef();
		Semantic.FunctionKey = Function.FunctionKey;
		Semantic.Role = TEXT("UFunctionRoot");
		Semantic.InvocationKind = TEXT("Global");
		Semantic.ReceiverKind = TEXT("None");
		Semantic.CanonicalASTState = TEXT("Verified");
		Semantic.CleanupPlanState = TEXT("VerifiedEmpty");
		Semantic.EligibilityReason = TEXT("None");
		Semantic.EligibilityDetail = TEXT("eligible");
		Semantic.EligibilitySource = Attempt.ProcessedSource;
		Attempt.PrimarySemanticFunctionIndex = 0;
		return Request;
	}

	const FAngelscriptJITGeneratedFile* FindProviderSource(
		const FAngelscriptJITGenerationOutput& Output)
	{
		return Output.Files.FindByPredicate([](const auto& File)
		{
			return File.RelativePath == TEXT("Provider.generated.cpp");
		});
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotGeneratedProviderDiagnosticTests,
	"Angelscript.TestModule.StaticJIT.AOT.Diagnostics.GeneratedProvider",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(EditorProviderEmitsDiagnosticsWithoutChangingExecutionIdentity)
	{
		using namespace
			AngelscriptStaticJITAotGeneratedProviderDiagnosticsTests_Private;
		FAngelscriptJITGenerationRequest WithDiagnostics = MakeRequest(
			EAngelscriptJITTargetProfile::EditorDevelopment);
		FAngelscriptJITGenerationRequest WithoutDiagnostics = WithDiagnostics;
		WithoutDiagnostics.Diagnostics.Reset();

		FAngelscriptJITGenerationOutput DiagnosticOutput;
		FAngelscriptJITGenerationOutput ExecutionOnlyOutput;
		FString Error;
		ASSERT_THAT(IsTrue(FAngelscriptJITGeneration::Generate(
			WithDiagnostics, DiagnosticOutput, &Error), Error));
		ASSERT_THAT(IsTrue(FAngelscriptJITGeneration::Generate(
			WithoutDiagnostics, ExecutionOnlyOutput, &Error), Error));
		ASSERT_THAT(IsTrue(DiagnosticOutput.Diagnostics.IsSet()));
		ASSERT_THAT(IsFalse(DiagnosticOutput.DiagnosticDigest.IsZero()));
		ASSERT_THAT(IsTrue(DiagnosticOutput.ArtifactSetDigest
			== ExecutionOnlyOutput.ArtifactSetDigest));
		ASSERT_THAT(IsTrue(DiagnosticOutput.ProviderGeneration
			== ExecutionOnlyOutput.ProviderGeneration));

		const FAngelscriptJITGeneratedFile* ProviderSource =
			FindProviderSource(DiagnosticOutput);
		ASSERT_THAT(IsNotNull(ProviderSource));
		ASSERT_THAT(IsTrue(ProviderSource->Content.Contains(
			TEXT("AngelscriptJITProviderDiagnostics.h"))));
		ASSERT_THAT(IsTrue(ProviderSource->Content.Contains(
			TEXT("GProviderDiagnostics"))));
		ASSERT_THAT(IsTrue(ProviderSource->Content.Contains(
			DiagnosticOutput.DiagnosticDigest.ToHexString())));
	}

	TEST_METHOD(ShippingProviderOmitsDiagnosticCatalog)
	{
		using namespace
			AngelscriptStaticJITAotGeneratedProviderDiagnosticsTests_Private;
		FAngelscriptJITGenerationRequest Request = MakeRequest(
			EAngelscriptJITTargetProfile::GameShipping);
		FAngelscriptJITGenerationOutput Output;
		FString Error;
		ASSERT_THAT(IsTrue(FAngelscriptJITGeneration::Generate(
			Request, Output, &Error), Error));
		ASSERT_THAT(IsFalse(Output.Diagnostics.IsSet()));
		ASSERT_THAT(IsTrue(Output.DiagnosticDigest.IsZero()));
		const FAngelscriptJITGeneratedFile* ProviderSource =
			FindProviderSource(Output);
		ASSERT_THAT(IsNotNull(ProviderSource));
		ASSERT_THAT(IsFalse(ProviderSource->Content.Contains(
			TEXT("AngelscriptJITProviderDiagnostics.h"))));
		ASSERT_THAT(IsFalse(ProviderSource->Content.Contains(
			TEXT("GProviderDiagnostics"))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS && AS_WITH_STATIC_JIT_DIAGNOSTICS
