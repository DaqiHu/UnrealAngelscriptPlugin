#include "CQTest.h"

#include "AngelscriptEngine.h"
#include "Cache/AngelscriptCacheEnvironment.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotGeneration.h"
#include "StaticJIT/AngelscriptStaticJITGenerationSnapshot.h"

#include "StartAngelscriptHeaders.h"
#include "as_module.h"
#include "as_restore.h"
#include "as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotGenerationFactsTests,
	"Angelscript.TestModule.StaticJIT.AOT.GenerationFacts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	class FArtifactProbeStream final : public asIBinaryStream
	{
	public:
		virtual int Read(void* Data, const asUINT Size) override
		{
			if ((Data == nullptr && Size != 0)
				|| ReadOffset > Bytes.Num()
				|| Size > static_cast<asUINT>(Bytes.Num() - ReadOffset))
			{
				return asERROR;
			}
			if (Size != 0)
			{
				FMemory::Memcpy(Data, Bytes.GetData() + ReadOffset, Size);
				ReadOffset += static_cast<int32>(Size);
			}
			return asSUCCESS;
		}

		virtual int Write(const void* Data, const asUINT Size) override
		{
			if ((Data == nullptr && Size != 0)
				|| Size > static_cast<asUINT>(MAX_int32 - Bytes.Num()))
			{
				return asOUT_OF_MEMORY;
			}
			if (Size != 0)
			{
				const int32 Offset = Bytes.AddUninitialized(
					static_cast<int32>(Size));
				FMemory::Memcpy(Bytes.GetData() + Offset, Data, Size);
			}
			return asSUCCESS;
		}

		TArray<uint8> Bytes;
		int32 ReadOffset = 0;
	};

public:
	TEST_METHOD(LargeFixtureBuildsVerifiedFactsWithoutCacheRecords)
	{
		TUniquePtr<AngelscriptStaticJITAotGeneration::
			FStaticJITAotFixtureEngineSession> Session =
				AngelscriptStaticJITAotGeneration::
					CreateInterpreterFixtureEngineSession();
		ASSERT_THAT(IsNotNull(Session.Get()));
		ASSERT_THAT(IsTrue(Session->IsValid()));
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
				Session->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));
		ASSERT_THAT(IsTrue(Snapshot->IsComplete()));
		ASSERT_THAT(AreEqual(0u, Snapshot->InputCacheRecordCount));
		const FAngelscriptStaticJITGenerationModule* const FixtureModule =
			Snapshot->Modules.FindByPredicate([](const auto& Module)
			{
				return Module.CanonicalModuleName == TEXT("ASStaticJITAotFixture");
			});
		ASSERT_THAT(IsNotNull(FixtureModule));
		int32 FixtureFunctionCount = 0;
		for (const FAngelscriptStaticJITGenerationFunction& Function
			: Snapshot->Functions)
		{
			if (Function.ModuleKey == FixtureModule->ModuleKey)
			{
				++FixtureFunctionCount;
			}
		}
		const TSharedPtr<FAngelscriptModuleDesc> LiveFixtureModule =
			Session->Engine->GetModuleByModuleName(
				FixtureModule->CanonicalModuleName);
		ASSERT_THAT(IsTrue(LiveFixtureModule.IsValid()));
		ASSERT_THAT(IsNotNull(LiveFixtureModule->ScriptModule));
		// The isolated generation Engine intentionally does not publish the
		// process-facing FunctionRoute snapshot. Compare against its complete VM
		// module function table instead, so this remains independent of Cache V2
		// records and automatically follows authored/generated fixture growth.
		int32 ExpectedFixtureFunctionCount = 0;
		for (asUINT FunctionIndex = 0;
			FunctionIndex
				< LiveFixtureModule->ScriptModule->scriptFunctions.GetLength();
			++FunctionIndex)
		{
			const asCScriptFunction* const Function =
				LiveFixtureModule->ScriptModule->scriptFunctions[FunctionIndex];
			if (Function != nullptr
				&& Function->module == LiveFixtureModule->ScriptModule)
			{
				++ExpectedFixtureFunctionCount;
			}
		}
		ASSERT_THAT(AreEqual(
			ExpectedFixtureFunctionCount, FixtureFunctionCount));
		const FAngelscriptStaticJITGenerationFunction* const SemanticFunction =
			Snapshot->Functions.FindByPredicate(
				[FixtureModule](const auto& Function)
				{
					return Function.ModuleKey == FixtureModule->ModuleKey
						&& Function.CanonicalDeclaration.Contains(
							TEXT("SemanticScalarBranch"));
				});
		ASSERT_THAT(IsNotNull(SemanticFunction));
		ASSERT_THAT(IsTrue(
			SemanticFunction->bHasVerifiedArtifactIdentity));
		ASSERT_THAT(IsTrue(
			SemanticFunction->bHasVerifiedArtifactReferences));
	}

	TEST_METHOD(CompleteFixtureFunctionRelocationsHaveDeclaredDependencies)
	{
		TUniquePtr<AngelscriptStaticJITAotGeneration::
			FStaticJITAotFixtureEngineSession> Session =
				AngelscriptStaticJITAotGeneration::
					CreateCompleteInterpreterFixtureEngineSession();
		ASSERT_THAT(IsNotNull(Session.Get()));
		ASSERT_THAT(IsTrue(Session->IsValid(), Session->Error));

		const TSharedPtr<FAngelscriptModuleDesc> FixtureModule =
			Session->Engine->GetModuleByModuleName(TEXT("ASStaticJITAotFixture"));
		ASSERT_THAT(IsTrue(FixtureModule.IsValid()));
		ASSERT_THAT(IsNotNull(FixtureModule->ScriptModule));

		asCScriptFunction* ObjectLifetimeMethod = nullptr;
		for (asUINT FunctionIndex = 0;
			FunctionIndex < FixtureModule->ScriptModule->scriptFunctions.GetLength();
			++FunctionIndex)
		{
			asCScriptFunction* const Candidate =
				FixtureModule->ScriptModule->scriptFunctions[FunctionIndex];
			if (Candidate != nullptr
				&& Candidate->module == FixtureModule->ScriptModule
				&& Candidate->name == "ObjectLifetimeEntryForAOT"
				&& Candidate->objectType != nullptr
				&& Candidate->objectType->name
					== "UStaticJITAotFunctionCarrier")
			{
				ASSERT_THAT(IsNull(ObjectLifetimeMethod));
				ObjectLifetimeMethod = Candidate;
			}
		}
		ASSERT_THAT(IsNotNull(ObjectLifetimeMethod));
		ASSERT_THAT(IsNotNull(ObjectLifetimeMethod->scriptData));

		FArtifactProbeStream Artifact;
		asCWriter Writer(
			FixtureModule->ScriptModule,
			&Artifact,
			FixtureModule->ScriptModule->engine,
			true);
		asSFunctionArtifactWriteDiagnostics WriteDiagnostics{};
		ASSERT_THAT(AreEqual(asSUCCESS, Writer.WriteFunctionArtifact(
			ObjectLifetimeMethod, &WriteDiagnostics)));
		ASSERT_THAT(IsFalse(Artifact.Bytes.IsEmpty()));

		asCReader Reader(
			FixtureModule->ScriptModule,
			&Artifact,
			FixtureModule->ScriptModule->engine);
		asSFunctionArtifactValidationDiagnostics ReadDiagnostics{};
		ASSERT_THAT(AreEqual(asSUCCESS, Reader.ValidateFunctionArtifact(
			static_cast<asUINT>(Artifact.Bytes.Num()), &ReadDiagnostics)));

		int32 FunctionUseCount = 0;
		for (asUINT UseIndex = 0;
			UseIndex < Reader.GetFunctionArtifactSymbolUseCount(); ++UseIndex)
		{
			const asSFunctionArtifactSymbolUse* const Use =
				Reader.GetFunctionArtifactSymbolUse(UseIndex);
			if (Use == nullptr
				|| Use->kind
					!= asFUNCTION_ARTIFACT_SYMBOL_FUNCTION_SIGNATURE)
			{
				continue;
			}
			++FunctionUseCount;
			ASSERT_THAT(IsNotNull(Use->function));

			bool bHasDeclaredDependency = false;
			for (asUINT DependencyIndex = 0;
				DependencyIndex < ObjectLifetimeMethod->scriptData->
					artifactDependencies.GetLength(); ++DependencyIndex)
			{
				const asSBuildArtifactDependency& Dependency =
					ObjectLifetimeMethod->scriptData->
						artifactDependencies[DependencyIndex];
				bHasDeclaredDependency |= Dependency.kind
						== asBUILD_ARTIFACT_DEPENDENCY_SIGNATURE
					&& Dependency.referenceKind
						== asBUILD_ARTIFACT_REFERENCE_FUNCTION
					&& Dependency.function == Use->function;
			}

			FAngelscriptArtifactReference EnvironmentReference;
			const bool bEnvironmentReference = Use->function != nullptr
				&& Use->function->module == nullptr
				&& FAngelscriptCacheEnvironmentIdentity::TryBuildFunctionReference(
					*Use->function, EnvironmentReference);
			const FString Detail = FString::Printf(
				TEXT("Function artifact use %u declaration='%s' owner='%s' key=%s must have an exact compiler dependency"),
				UseIndex,
				Use->function != nullptr
					? UTF8_TO_TCHAR(Use->function->GetDeclaration(
						false, false, false)) : TEXT("<none>"),
				Use->function != nullptr && Use->function->objectType != nullptr
					? UTF8_TO_TCHAR(Use->function->objectType->GetName())
					: TEXT("<none>"),
				bEnvironmentReference
					? *EnvironmentReference.StableKey.ToHexString()
					: TEXT("<non-environment>"));
			ASSERT_THAT(IsTrue(bHasDeclaredDependency, Detail));
		}
		ASSERT_THAT(IsTrue(FunctionUseCount > 0));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
