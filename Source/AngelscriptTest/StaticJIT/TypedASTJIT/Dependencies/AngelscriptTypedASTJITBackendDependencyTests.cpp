#include "CQTest.h"

#include "AngelscriptTestJITProbes.h"
#include "Misc/ScopeExit.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotGeneration.h"
#include "StaticJIT/AngelscriptJITGeneration.h"
#include "StaticJIT/AngelscriptStaticJITBytecodeAccessAudit.h"
#include "StaticJIT/AngelscriptStaticJITGenerationProfile.h"
#include "StaticJIT/AngelscriptStaticJITGenerationSnapshot.h"
#include "StaticJIT/AngelscriptStaticJITGenerator.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITBackendDependencyTests,
	"Angelscript.TestModule.StaticJIT.TypedASTJIT.Dependencies.Backend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FString DescribeBackendAttempts(
		const FAngelscriptStaticJITFunctionResult& Result)
	{
		TArray<FString> Parts;
		for (const FAngelscriptStaticJITBackendAttemptDiagnostic& Attempt
			: Result.BackendAttempts)
		{
			FString Typed;
			if (Attempt.TypedDiagnostic.IsSet())
			{
				Typed = FString::Printf(TEXT(" reason=%s typed=%s"),
					LexToString(Attempt.TypedDiagnostic->FallbackReason),
					*Attempt.TypedDiagnostic->Detail);
			}
			Parts.Add(FString::Printf(TEXT("%s disposition=%d detail=%s%s"),
				*Attempt.BackendId.ToString(),
				static_cast<int32>(Attempt.Disposition),
				*Attempt.Detail, *Typed));
		}
		return FString::Join(Parts, TEXT(" | "));
	}

	static FAngelscriptJITProviderGenerationSettings MakeSettings(
		const FAngelscriptStaticJITGenerationSnapshot& Snapshot)
	{
		FAngelscriptJITProviderGenerationSettings Settings;
		Settings.TargetProfile =
			EAngelscriptJITTargetProfile::EditorDevelopment;
		Settings.ProviderId = AngelscriptTestJITProbes::GetStableProviderId();
		Settings.ProviderName =
			TEXT("AngelscriptTestJIT semantic dependency backend");
		Settings.OwnerModuleName = TEXT("AngelscriptTestJIT");
		Settings.KnownNativeCallModules = {
			FName(TEXT("Core")),
			FName(TEXT("CoreUObject")),
			FName(TEXT("AngelscriptRuntime")),
			FName(TEXT("AngelscriptTest")),
		};
		Settings.PublicNativeCallDependencyModules = {
			FName(TEXT("Core")),
			FName(TEXT("CoreUObject")),
			FName(TEXT("AngelscriptRuntime")),
		};
		Settings.ArtifactProfile = Snapshot.ArtifactProfile;
		Settings.NativeEnvironmentFingerprint =
			FAngelscriptJITTargetProfiles::
				BuildCurrentNativeEnvironmentFingerprint(Settings.TargetProfile);
		return Settings;
	}

	static void AssertNoTypedBytecodeTraversal(
		FAutomationTestBase& Test,
		const FAngelscriptStaticJITBytecodeAccessObservation& Observation)
	{
		Test.TestTrue(
			TEXT("Typed eligibility and planning must not invoke BytecodeJIT analysis"),
			Observation.BytecodeAnalysisDuringTypedASTCount == 0);
		Test.TestTrue(
			TEXT("Typed reference collection must not scan bytecode"),
			Observation.BytecodeReferenceScanDuringTypedASTCount == 0);
		Test.TestTrue(
			TEXT("Typed generation must not read the VM instruction stream"),
			Observation.GetByteCodeDuringTypedASTCount == 0);
		Test.TestTrue(
			TEXT("Typed emission must not dispatch FAngelscriptBytecode"),
			Observation.BytecodeDispatchDuringTypedASTCount == 0);
	}

public:
	TEST_METHOD(MissingScriptCallSignatureFallsBackBeforeTypedEmission)
	{
		using namespace AngelscriptStaticJITAotFixture;
		using namespace AngelscriptStaticJITAotGeneration;

		TUniquePtr<FStaticJITAotFixtureEngineSession> Session =
			CreateCompleteInterpreterFixtureEngineSession();
		ASSERT_THAT(IsTrue(Session.IsValid(),
			TEXT("The production dependency test must allocate its fixture session")));
		ASSERT_THAT(IsTrue(Session->IsValid(), Session->Error));
		FAngelscriptEngineScope EngineScope(*Session->Engine);

		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> SourceSnapshot =
				Session->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(SourceSnapshot.IsValid(),
			TEXT("The generation Engine must publish a same-compilation snapshot")));
		ASSERT_THAT(IsTrue(SourceSnapshot->IsComplete(),
			TEXT("The source snapshot must be complete before the test mutates its owned copy")));

		const TSharedPtr<FAngelscriptModuleDesc> RecursionModule =
			Session->Engine->GetModuleByModuleName(
				GetTypedRecursionModuleName().ToString());
		ASSERT_THAT(IsTrue(RecursionModule.IsValid(),
			TEXT("The complete fixture must contain its recursion module descriptor")));
		ASSERT_THAT(IsNotNull(RecursionModule->ScriptModule,
			TEXT("The recursion module descriptor must own a compiled module")));

		FTCHARToUTF8 EntryDeclarationUtf8(
			*GetTypedASTFrameEntryDeclaration());
		asIScriptFunction* const EntryFunction =
			RecursionModule->ScriptModule->GetFunctionByDecl(
				EntryDeclarationUtf8.Get());
		ASSERT_THAT(IsNotNull(EntryFunction,
			TEXT("The fixture must resolve TypedASTFrameEntry by exact declaration")));
		const FAngelscriptStaticJITGenerationFunction* const SnapshotFunction =
			SourceSnapshot->Functions.FindByPredicate(
				[EntryFunction](const auto& Candidate)
				{
					return Candidate.EngineLocalFunction == EntryFunction;
				});
		ASSERT_THAT(IsNotNull(SnapshotFunction,
			TEXT("The exact Engine-local root must have a stable snapshot function")));

		TSharedPtr<FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> MutatedSnapshot =
				MakeShared<FAngelscriptStaticJITGenerationSnapshot,
					ESPMode::ThreadSafe>(*SourceSnapshot);
		const int32 SignatureDependencyIndex =
			MutatedSnapshot->SemanticDependencies.IndexOfByPredicate(
				[SnapshotFunction](const auto& Dependency)
				{
					return Dependency.OwnerFunctionKey
							== SnapshotFunction->FunctionKey
						&& Dependency.Kind
							== EAngelscriptArtifactSemanticDependencyKind::Signature
						&& Dependency.Reference.Kind
							== EAngelscriptArtifactReferenceKind::ScriptFunction;
				});
		ASSERT_THAT(IsTrue(SignatureDependencyIndex != INDEX_NONE,
			TEXT("The real compiler snapshot must record the direct helper signature dependency")));
		const FAngelscriptArtifactReference RemovedReference =
			MutatedSnapshot->SemanticDependencies[SignatureDependencyIndex].Reference;
		MutatedSnapshot->SemanticDependencies.RemoveAt(SignatureDependencyIndex);
		ASSERT_THAT(IsTrue(MutatedSnapshot->Dependencies.ContainsByPredicate(
			[SnapshotFunction, &RemovedReference](const auto& Dependency)
			{
				return Dependency.OwnerFunctionKey == SnapshotFunction->FunctionKey
					&& Dependency.Reference == RemovedReference;
			}), TEXT("The disagreement fixture must preserve the exact flat reference")));

		FAngelscriptStaticJITGenerationProfile GenerationProfile;
		FString ProfileError;
		ASSERT_THAT(IsTrue(FAngelscriptStaticJITGenerationProfile::TryCreate(
			FAngelscriptStaticJITBackendId::TypedAST(),
			EAngelscriptJITTargetProfile::EditorDevelopment,
			GenerationProfile, &ProfileError), ProfileError));

		TArray<asIScriptModule*> CompiledModules;
		for (const FAngelscriptStaticJITGenerationModule& Module
			: MutatedSnapshot->Modules)
		{
			CompiledModules.Add(Module.EngineLocalModule);
		}
		asIScriptModule* EmitModules[] = {RecursionModule->ScriptModule};

		FAngelscriptStaticJITBytecodeAccessAudit::ResetForTesting();
		ON_SCOPE_EXIT
		{
			FAngelscriptStaticJITBytecodeAccessAudit::ResetForTesting();
		};

		FAngelscriptJITProviderGenerationSettings Settings =
			MakeSettings(*MutatedSnapshot);

		FAngelscriptStaticJITProviderGenerationRequest Request;
		Request.BackendId = FAngelscriptStaticJITBackendId::TypedAST();
		Request.Settings = MoveTemp(Settings);
		Request.TypedASTJITCapabilityProfile =
			GenerationProfile.TypedASTJITCapabilityProfile;
		Request.TypedASTJITExecutionRequirements =
			GenerationProfile.TypedASTJITExecutionRequirements;
		Request.CompiledSourceGraph = CompiledModules;
		Request.EmitModuleSet = MakeArrayView(EmitModules);
		Request.GenerationSnapshot = MutatedSnapshot;

		FAngelscriptStaticJITGeneratorOutput Output;
		FString GenerationError;
		ASSERT_THAT(IsTrue(GenerateStaticJITProviderArtifacts(
			Request, Output, &GenerationError), GenerationError));
		const FAngelscriptStaticJITFunctionResult* const Result =
			Output.Functions.FindByPredicate(
				[SnapshotFunction](const auto& Candidate)
				{
					return Candidate.FunctionKey
						== SnapshotFunction->FunctionKey;
				});
		ASSERT_THAT(IsNotNull(Result,
			TEXT("The generator must retain the exact root result after fallback")));
		ASSERT_THAT(AreEqual(FAngelscriptStaticJITBackendId::Bytecode(),
			Result->ActualBackendId,
			TEXT("A semantic dependency mismatch must fall back to BytecodeJIT")));

		const FAngelscriptStaticJITBackendAttemptDiagnostic* const TypedAttempt =
			Result->BackendAttempts.FindByPredicate(
				[](const auto& Attempt)
				{
					return Attempt.BackendId
						== FAngelscriptStaticJITBackendId::TypedAST();
				});
		ASSERT_THAT(IsNotNull(TypedAttempt,
			TEXT("The attempt trace must retain the rejected TypedASTJIT attempt")));
		ASSERT_THAT(IsTrue(TypedAttempt->TypedDiagnostic.IsSet(),
			TEXT("The rejected attempt must expose typed fallback diagnostics")));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::SemanticDependencyMismatch,
			TypedAttempt->TypedDiagnostic->FallbackReason,
			*FString::Printf(TEXT(
				"The missing compiler dependency must not be reported as an emitter failure: %s"),
				*DescribeBackendAttempts(*Result))));
		ASSERT_THAT(IsTrue(TypedAttempt->TypedDiagnostic->Detail.Contains(
			TEXT("ScriptCallSignature"), ESearchCase::CaseSensitive),
			TypedAttempt->TypedDiagnostic->Detail));

		ASSERT_THAT(IsTrue(Output.bHasProviderOutput,
			TEXT("BytecodeJIT fallback must still produce one Provider artifact")));
		const FAngelscriptJITGeneratedFunction* const GeneratedFunction =
			Output.ProviderOutput.Functions.FindByPredicate(
				[SnapshotFunction](const auto& Candidate)
				{
					return Candidate.FunctionKey
						== SnapshotFunction->FunctionKey;
				});
		ASSERT_THAT(IsNotNull(GeneratedFunction,
			TEXT("The Provider must contain the BytecodeJIT fallback function")));
		const FAngelscriptJITGeneratedFile* const ModuleSource =
			Output.ProviderOutput.Files.FindByPredicate(
				[GeneratedFunction](const auto& Candidate)
				{
					return Candidate.RelativePath
						== GeneratedFunction->ModuleSourceRelativePath;
				});
		ASSERT_THAT(IsNotNull(ModuleSource,
			TEXT("The fallback function must own one generated module source")));
		ASSERT_THAT(IsFalse(ModuleSource->Content.Contains(
			GeneratedFunction->SymbolPrefix + TEXT("_TypedBody"),
			ESearchCase::CaseSensitive),
			TEXT("No partial Typed body may survive dependency reconciliation")));
		AssertNoTypedBytecodeTraversal(
			*TestRunner,
			FAngelscriptStaticJITBytecodeAccessAudit::GetObservationForTesting());
	}

	TEST_METHOD(ValidDirectClosuresUseTypedBackendWithCompilerSignatureOnly)
	{
		using namespace AngelscriptStaticJITAotFixture;
		using namespace AngelscriptStaticJITAotGeneration;

		TUniquePtr<FStaticJITAotFixtureEngineSession> Session =
			CreateCompleteInterpreterFixtureEngineSession();
		ASSERT_THAT(IsTrue(Session.IsValid(),
			TEXT("The valid-closure test must allocate its fixture session")));
		ASSERT_THAT(IsTrue(Session->IsValid(), Session->Error));
		FAngelscriptEngineScope EngineScope(*Session->Engine);

		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> SourceSnapshot =
				Session->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(SourceSnapshot.IsValid(),
			TEXT("The generation Engine must publish a same-compilation snapshot")));
		ASSERT_THAT(IsTrue(SourceSnapshot->IsComplete(),
			TEXT("The source snapshot must be complete before generation")));
		ASSERT_THAT(IsTrue(SourceSnapshot->bHasVerifiedCanonicalAST,
			TEXT("typed-ast source snapshots retain the sealed Canonical AST")));
		ASSERT_THAT(IsTrue(!SourceSnapshot->NativeCallTargets.IsEmpty(),
			TEXT("sealed canonical calls must still capture exact native targets without HIR")));

		const TSharedPtr<FAngelscriptModuleDesc> RecursionModule =
			Session->Engine->GetModuleByModuleName(
				GetTypedRecursionModuleName().ToString());
		ASSERT_THAT(IsTrue(RecursionModule.IsValid(),
			TEXT("The complete fixture must contain its recursion module descriptor")));
		ASSERT_THAT(IsNotNull(RecursionModule->ScriptModule,
			TEXT("The recursion module descriptor must own a compiled module")));

		const FString RequiredDeclarations[] = {
			GetTypedASTSelfRecursionDeclaration(),
			GetTypedASTMutualRecursionDeclaration(),
			GetTypedASTFrameEntryDeclaration(),
		};

		FAngelscriptStaticJITGenerationProfile GenerationProfile;
		FString ProfileError;
		ASSERT_THAT(IsTrue(FAngelscriptStaticJITGenerationProfile::TryCreate(
			FAngelscriptStaticJITBackendId::TypedAST(),
			EAngelscriptJITTargetProfile::EditorDevelopment,
			GenerationProfile, &ProfileError), ProfileError));

		TArray<asIScriptModule*> CompiledModules;
		for (const FAngelscriptStaticJITGenerationModule& Module
			: SourceSnapshot->Modules)
		{
			CompiledModules.Add(Module.EngineLocalModule);
		}
		asIScriptModule* EmitModules[] = {RecursionModule->ScriptModule};

		FAngelscriptStaticJITBytecodeAccessAudit::ResetForTesting();
		ON_SCOPE_EXIT
		{
			FAngelscriptStaticJITBytecodeAccessAudit::ResetForTesting();
		};

		FAngelscriptStaticJITProviderGenerationRequest Request;
		Request.BackendId = FAngelscriptStaticJITBackendId::TypedAST();
		Request.Settings = MakeSettings(*SourceSnapshot);
		Request.TypedASTJITCapabilityProfile =
			GenerationProfile.TypedASTJITCapabilityProfile;
		Request.TypedASTJITExecutionRequirements =
			GenerationProfile.TypedASTJITExecutionRequirements;
		Request.CompiledSourceGraph = CompiledModules;
		Request.EmitModuleSet = MakeArrayView(EmitModules);
		Request.GenerationSnapshot = SourceSnapshot;

		FAngelscriptStaticJITGeneratorOutput Output;
		FString GenerationError;
		ASSERT_THAT(IsTrue(GenerateStaticJITProviderArtifacts(
			Request, Output, &GenerationError), GenerationError));

		for (const FString& Declaration : RequiredDeclarations)
		{
			FTCHARToUTF8 DeclarationUtf8(*Declaration);
			asIScriptFunction* const RequiredFunction =
				RecursionModule->ScriptModule->GetFunctionByDecl(
					DeclarationUtf8.Get());
			ASSERT_THAT(IsNotNull(RequiredFunction,
				*FString::Printf(TEXT("The fixture must resolve %s"),
					*Declaration)));
			const FAngelscriptStaticJITGenerationFunction* const
				SnapshotFunction = SourceSnapshot->Functions.FindByPredicate(
					[RequiredFunction](const auto& Candidate)
					{
						return Candidate.EngineLocalFunction
							== RequiredFunction;
					});
			ASSERT_THAT(IsNotNull(SnapshotFunction,
				*FString::Printf(TEXT(
					"The snapshot must contain %s"), *Declaration)));
			const FAngelscriptStaticJITFunctionResult* const Result =
				Output.Functions.FindByPredicate(
					[SnapshotFunction](const auto& Candidate)
					{
						return Candidate.FunctionKey
							== SnapshotFunction->FunctionKey;
					});
			ASSERT_THAT(IsNotNull(Result,
				*FString::Printf(TEXT(
					"The generator must retain %s"), *Declaration)));
			ASSERT_THAT(AreEqual(FAngelscriptStaticJITBackendId::TypedAST(),
				Result->ActualBackendId,
				*FString::Printf(TEXT(
					"TypedASTJIT self-recursion AOT fixture did not use production TypedAST backend: Function=%s Backend=%s Detail=%s Attempts=%s"),
					*Declaration,
					*Result->ActualBackendId.ToString(),
					*Result->Detail,
					*DescribeBackendAttempts(*Result))));
			ASSERT_THAT(IsTrue(Result->TypedDiagnostic.IsSet()
				&& Result->TypedDiagnostic->SemanticFunction.IsSet(),
				*FString::Printf(TEXT(
					"TypedASTJIT must retain Canonical semantic facts for %s"),
					*Declaration)));
			if (!Result->TypedDiagnostic.IsSet()
				|| !Result->TypedDiagnostic->SemanticFunction.IsSet())
			{
				continue;
			}
			const FAngelscriptStaticJITCanonicalFunctionDiagnostic&
				Semantic = Result->TypedDiagnostic->SemanticFunction.GetValue();
			ASSERT_THAT(AreEqual(
				EAngelscriptTypedASTJITCleanupPlanState::VerifiedEmpty,
				Semantic.CleanupPlanState,
				*FString::Printf(TEXT(
					"the production AOT diagnostic for %s must derive an empty cleanup plan from the same sealed Canonical AST"),
					*Declaration)));
			ASSERT_THAT(IsTrue(Semantic.bCleanupPlanCoversAllTransfers,
				*FString::Printf(TEXT(
					"a sealed function with no cleanup actions must prove complete transfer coverage: %s"),
					*Declaration)));
		}

		AssertNoTypedBytecodeTraversal(
			*TestRunner,
			FAngelscriptStaticJITBytecodeAccessAudit::GetObservationForTesting());
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
