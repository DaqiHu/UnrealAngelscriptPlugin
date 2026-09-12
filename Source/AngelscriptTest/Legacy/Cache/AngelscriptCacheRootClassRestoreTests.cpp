#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheRestore.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Shared/AngelscriptReflectiveAccess.h"
#include "Shared/AngelscriptTestFixture.h"
#include "UObject/UObjectGlobals.h"

#include "StartAngelscriptHeaders.h"
#include "frontend/AST/as_ast_context.h"
#include "as_module.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheRootClassRestoreTests,
	"Angelscript.TestModule.Cache.RootClassRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	class FPackSource final : public IAngelscriptCachePackSource
	{
	public:
		explicit FPackSource(
			const TConstArrayView<FAngelscriptEncodedPack> InPacks)
			: Packs(InPacks)
		{
		}

		virtual bool TryGetCompletePack(
			const FAngelscriptHash256& PackId,
			TConstArrayView<uint8>& OutBytes) override
		{
			for (const FAngelscriptEncodedPack& Pack : Packs)
			{
				if (Pack.PackId == PackId)
				{
					OutBytes = Pack.Bytes;
					return true;
				}
			}
			OutBytes = {};
			return false;
		}

	private:
		TConstArrayView<FAngelscriptEncodedPack> Packs;
	};

	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions()
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2RootClassRestore"),
			TEXT("VmExecutionCodec=5"),
		};
		Options.Compatibility =
			FAngelscriptArtifactIdentityBuilder::BuildCompatibilityKey(
				Compatibility);

		FAngelscriptContextDescriptor Context;
		Context.CanonicalInputs = {
			TEXT("SourceMount=Game"),
			TEXT("DebugSidecar=Enabled"),
		};
		Options.Context =
			FAngelscriptArtifactIdentityBuilder::BuildContextKey(Context);
		Options.Profile =
			FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
				Options.Compatibility, Options.Context);
		Options.CanonicalCompileOptions = {
			TEXT("AutomaticImports=false"),
		};
		return Options;
	}

public:
	TEST_METHOD(ReflectedRootClassRoundTripsIntoFreshEngine)
	{
		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		{
			FAngelscriptTestFixture Producer(
				*TestRunner, ETestEngineMode::IsolatedFull);
			ASSERT_THAT(IsTrue(Producer.IsValid(),
				TEXT("Producer Engine should initialize")));
			Producer.GetEngine().NextCompileASTRetentionPolicy = static_cast<int32>(
				asAST_RETAIN_SNAPSHOT);

			const FString ScriptSource = ASTEST_AS(R"AS(
				UCLASS()
				class UCacheV2RestoredRootObject : UObject
				{
					UPROPERTY()
					int Value = 37;

					int ScriptOnlyAdd(int Delta)
					{
						return Value + Delta;
					}

					UFUNCTION()
					int ReflectedValue()
					{
						return ScriptOnlyAdd(5);
					}
				}
			)AS");
			asIScriptModule* ScriptModule = Producer.BuildModule(
				"ASCacheV2RootClassRestore", ScriptSource);
			ASSERT_THAT(IsNotNull(ScriptModule,
				TEXT("Root-class producer module should compile")));
			TSharedPtr<FAngelscriptModuleDesc> Module =
				Producer.GetEngine().GetModule(ScriptModule);
			ASSERT_THAT(IsTrue(Module.IsValid(),
				TEXT("Producer should retain the root-class descriptor")));
			ASSERT_THAT(AreEqual(1, Module->Classes.Num()));
			ASSERT_THAT(AreEqual(1, Module->Classes[0]->Properties.Num()));
			ASSERT_THAT(AreEqual(1, Module->Classes[0]->Methods.Num()));
			asCModule* const InternalModule =
				static_cast<asCModule*>(ScriptModule);
			const asCASTContext* const CanonicalAST =
				InternalModule->GetCanonicalASTContext();
			ASSERT_THAT(IsNotNull(CanonicalAST,
				TEXT("Root-class producer must retain its sealed Canonical AST")));
			const asCDecl* StaticClassDeclaration = nullptr;
			asUINT StaticClassDeclarationCount = 0;
			if (CanonicalAST != nullptr)
			{
				for (asUINT DeclarationIndex = 1;
					DeclarationIndex <= CanonicalAST->GetDeclCount(); ++DeclarationIndex)
				{
					const asCDecl* const Declaration = CanonicalAST->GetDecl(
						asASTDeclId(DeclarationIndex));
					if (Declaration != nullptr
						&& Declaration->kind == asAST_DECL_FUNCTION
						&& Declaration->stableKey.Equals(
							"UCacheV2RestoredRootObject::StaticClass()"))
					{
						StaticClassDeclaration = Declaration;
						++StaticClassDeclarationCount;
					}
				}
			}
			ASSERT_THAT(AreEqual(1, static_cast<int32>(
				StaticClassDeclarationCount),
				TEXT("The sealed Canonical AST must own exactly one stable StaticClass helper")));
			ASSERT_THAT(IsTrue(StaticClassDeclaration != nullptr
				&& StaticClassDeclaration->body.IsValid(),
				TEXT("The sealed StaticClass helper must own its source-derived body before Cache capture")));
			ASSERT_THAT(IsTrue(StaticClassDeclaration != nullptr
				&& (StaticClassDeclaration->traits & asAST_TRAIT_GENERATED) != 0,
				TEXT("Canonical Sema must seal the parsed __generated decorator before Cache capture")));

			const FAngelscriptCacheCleanCaptureResult Capture =
				CaptureAngelscriptCleanCompiledModule(
					Module.ToSharedRef(), Options, Artifacts);
			TestRunner->AddInfo(FString::Printf(
				TEXT("IC-434 root-class capture: Error=%u Records=%d GraphRecords=%u Detail=%s"),
				static_cast<uint32>(Capture.Error),
				Artifacts.Records.Num(),
				Capture.ValidatedGraphRecordCount,
				*Capture.Detail));
			ASSERT_THAT(IsTrue(Capture.IsSuccess(),
				TEXT("A complete reflected root class should be cacheable")));
			int32 ASTBodySidecarCount = 0;
			for (const FAngelscriptPreparedRecord& Record : Artifacts.Records)
			{
				ASTBodySidecarCount +=
					Record.RecordId.Kind == EAngelscriptCacheRecordKind::ASTBodySidecar
						? 1 : 0;
			}
			ASSERT_THAT(AreEqual(1, ASTBodySidecarCount,
				TEXT("a retained root-class module must emit one shared Canonical AST sidecar")));
		}

		FAngelscriptCachePackPolicy PackPolicy;
		PackPolicy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		FAngelscriptCachePreparedColdGeneration Prepared;
		const FAngelscriptCacheCleanCaptureResult Preparation =
			PrepareAngelscriptCacheColdGeneration(
				Artifacts, Options, PackPolicy, Codec, Prepared);
		ASSERT_THAT(IsTrue(Preparation.IsSuccess(),
			TEXT("Root-class artifacts should form a generation")));

		FPackSource Packs(Prepared.Packs);
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptValidatedGeneration> Validated;
		const FAngelscriptCacheValidationResult Validation =
			ValidateAngelscriptCacheGeneration(
				Prepared.EncodedManifest.CompleteBytes,
				Prepared.EncodedManifest.ComputedGenerationId,
				Packs,
				Limits,
				Budget,
				Codec,
				Validated);
		ASSERT_THAT(IsTrue(Validation.IsSuccess(),
			TEXT("Root-class generation should validate")));
		ASSERT_THAT(IsTrue(Validated.IsSet(),
			TEXT("Validation should promote the root-class generation")));

		FAngelscriptTestFixture Consumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid(),
			TEXT("Consumer Engine should initialize")));
		Consumer.GetEngine().NextCompileASTRetentionPolicy = static_cast<int32>(
			asAST_RETAIN_SNAPSHOT);
		const FAngelscriptCacheRestoreResult Restore =
			RestoreAngelscriptCacheModule(
				Consumer.GetEngine(),
				Validated.GetValue(),
				Artifacts.ModuleKey,
				Limits);
		TestRunner->AddInfo(FString::Printf(
			TEXT("IC-434 root-class restore: Error=%u Stage=%u Types=%u Functions=%u Detail=%s"),
			static_cast<uint32>(Restore.Error),
			static_cast<uint32>(Restore.Stage),
			Restore.RestoredTypeCount,
			Restore.RestoredFunctionCount,
			*Restore.Detail));
		ASSERT_THAT(IsTrue(Restore.IsSuccess(),
			TEXT("A fresh Engine should restore the complete root class")));
		ASSERT_THAT(AreEqual(uint32(1), Restore.RestoredTypeCount));
		ASSERT_THAT(AreEqual(
			static_cast<uint32>(
				Artifacts.ValidatedFunctionArtifactIdentities.Num()),
			Restore.RestoredFunctionCount));

		TSharedPtr<FAngelscriptModuleDesc> RestoredModule =
			Consumer.GetEngine().GetModuleByModuleName(
				TEXT("ASCacheV2RootClassRestore"));
		ASSERT_THAT(IsTrue(RestoredModule.IsValid()));
		ASSERT_THAT(IsNotNull(RestoredModule->ScriptModule));
		asIASTSnapshot* const Snapshot =
			RestoredModule->ScriptModule->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(Snapshot,
			TEXT("retained root-class restore must publish its verified Canonical AST")));
		if (Snapshot != nullptr)
		{
			Snapshot->Release();
		}
		ASSERT_THAT(AreEqual(1, RestoredModule->Classes.Num()));
		const TSharedRef<FAngelscriptClassDesc>& RestoredClass =
			RestoredModule->Classes[0];
		ASSERT_THAT(AreEqual(
			FString(TEXT("UCacheV2RestoredRootObject")),
			RestoredClass->ClassName));
		ASSERT_THAT(IsNotNull(RestoredClass->ScriptType));
		ASSERT_THAT(IsNotNull(RestoredClass->Class));
		ASSERT_THAT(AreEqual(1, RestoredClass->Properties.Num()));
		ASSERT_THAT(AreEqual(1, RestoredClass->Methods.Num()));

		UFunction* ReflectedValue = RestoredClass->Class->FindFunctionByName(
			TEXT("ReflectedValue"));
		ASSERT_THAT(IsNotNull(ReflectedValue,
			TEXT("ClassGenerator should recreate the cached UFUNCTION")));
		UObject* Instance = NewObject<UObject>(
			GetTransientPackage(), RestoredClass->Class);
		ASSERT_THAT(IsNotNull(Instance,
			TEXT("The restored UClass should create an instance")));
		FFunctionInvoker Invoker(
			*TestRunner, Instance, FName(TEXT("ReflectedValue")));
		ASSERT_THAT(IsTrue(Invoker.IsValid(),
			TEXT("The restored UFUNCTION should be invokable")));
		ASSERT_THAT(AreEqual(42,
			Invoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("Restored reflected method dependencies should execute")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
