#include "CQTest.h"

#include "AngelscriptEngine.h"
#include "StaticJIT/AngelscriptProjectSourceGraph.h"
#include "StaticJIT/AngelscriptStaticJITGenerationSnapshot.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallDescriptor.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "as_module.h"
#include "as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT

namespace AngelscriptProjectSourceGraphTests
{
	using namespace AngelscriptEditor::StaticJIT;

	static FString MakeProject(FString& OutRoot)
	{
		OutRoot = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/AngelscriptProjectSourceGraph"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
		check(IFileManager::Get().MakeDirectory(*OutRoot, true));
		const FString ProjectFile = FPaths::Combine(
			OutRoot, TEXT("CollectBindsFixture.uproject"));
		check(FFileHelper::SaveStringToFile(
			TEXT("{\n  \"FileVersion\": 3,\n  \"Modules\": []\n}\n"),
			*ProjectFile,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
		const FString ScriptRoot = FPaths::Combine(OutRoot, TEXT("Script"));
		check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		check(FFileHelper::SaveStringToFile(
			TEXT("UFUNCTION()\n")
			TEXT("int CollectBindsEntry()\n")
			TEXT("{\n")
			TEXT("\treturn 1;\n")
			TEXT("}\n"),
			*FPaths::Combine(ScriptRoot, TEXT("CollectBindsFixture.as")),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
		return ProjectFile;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptProjectSourceGraphTests,
	"Angelscript.TestModule.StaticJIT.ProjectSourceGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(StaticJITArtifactCompileRetainsReviewedDescriptorAndNativeForm)
	{
		using namespace AngelscriptEditor::StaticJIT;
		using namespace AngelscriptProjectSourceGraphTests;

		FString Root;
		const FString ProjectFile = MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};

		FAngelscriptProjectSourceGraphCompileRequest Request;
		Request.ProjectFile = ProjectFile;
		Request.TargetProfile =
			EAngelscriptJITTargetProfile::EditorDevelopment;
		Request.CaptureProfile =
			EAngelscriptStaticJITCaptureProfile::VerifiedCanonicalAST;
		Request.ASTRetentionPolicy =
			static_cast<int32>(asAST_RETAIN_SNAPSHOT);

		bool bSawCollectFlag = false;
		bool bSawCanonicalASTRequirement = false;
		bool bSawIsRunningCommandlet = false;
		bool bSawNativeForm = false;
		bool bSawCanonicalCompilerPipeline = false;
		bool bAllModulesPublishedCanonicalBytecode = true;
		bool bAllModulesSkippedLegacyCompiler = true;
		bool bAllModulesPublishedCanonicalDigest = true;
		int32 AuditedModuleCount = 0;
		FString Error;
		ASSERT_THAT(IsTrue(FAngelscriptProjectSourceGraph::Compile(
			Request,
			[&](const FAngelscriptProjectSourceGraphCompileView& View,
				FString*)
			{
				bSawCollectFlag =
					View.Engine.bCollectStaticJITCompatibilityBinds;
				bSawCanonicalASTRequirement =
					View.Engine.GetRuntimeConfig()
						.bRequireCanonicalASTForStaticJITGeneration;
				const asCScriptEngine* const ScriptEngine =
					static_cast<const asCScriptEngine*>(
						View.Engine.GetScriptEngine());
				bSawCanonicalCompilerPipeline = ScriptEngine != nullptr
					&& ScriptEngine->GetCompilerPipeline()
						== asCOMPILER_PIPELINE_CANONICAL;
				for (asIScriptModule* PublicModule : View.Modules)
				{
					const asCModule* const Module =
						static_cast<const asCModule*>(PublicModule);
					if (Module == nullptr)
					{
						bAllModulesPublishedCanonicalBytecode = false;
						bAllModulesSkippedLegacyCompiler = false;
						bAllModulesPublishedCanonicalDigest = false;
						continue;
					}
					++AuditedModuleCount;
					bAllModulesPublishedCanonicalBytecode &=
						Module->GetLastBytecodePublisher()
							== asBYTECODE_PUBLISHER_CANONICAL_CODEGEN;
					bAllModulesSkippedLegacyCompiler &=
						Module->GetLastLegacyCompilerInvocationCount() == 0;
					asQWORD Digest = 0;
					bAllModulesPublishedCanonicalDigest &=
						Module->GetLastCanonicalCodeGenAstDigest(Digest);
				}
				for (const FAngelscriptStaticJITNativeCallInventoryEntry&
					Entry : View.Snapshot->NativeCallInventory)
				{
					bSawNativeForm |= Entry.bHasNativeForm;
					if (Entry.bHasExternalDescriptor
						&& Entry.ExternalDescriptor.Symbol
							== TEXT("IsRunningCommandlet")
						&& Entry.ExternalDescriptor.Linkage
							== EAngelscriptStaticJITNativeCallLinkage::
								HeaderInline)
					{
						bSawIsRunningCommandlet = true;
					}
				}
				return true;
			},
			&Error), Error));
		ASSERT_THAT(IsTrue(bSawCollectFlag,
			TEXT("Official StaticJITArtifact compile must collect compatibility binds")));
		ASSERT_THAT(IsTrue(bSawCanonicalASTRequirement,
			TEXT("Official TypedAST generation must require the retained canonical AST snapshot")));
		ASSERT_THAT(IsTrue(bSawCanonicalCompilerPipeline,
			TEXT("Official contained TypedAST generation must select CANONICAL before source compilation")));
		ASSERT_THAT(IsTrue(AuditedModuleCount > 0,
			TEXT("Official contained generation must expose at least one compiled source module")));
		ASSERT_THAT(IsTrue(bAllModulesPublishedCanonicalBytecode,
			TEXT("Every contained generation module must publish Bytecode from Canonical CodeGen")));
		ASSERT_THAT(IsTrue(bAllModulesSkippedLegacyCompiler,
			TEXT("Contained generation must not construct asCCompiler as a hidden fallback")));
		ASSERT_THAT(IsTrue(bAllModulesPublishedCanonicalDigest,
			TEXT("Every contained generation module must publish its exact sealed-AST digest")));
		ASSERT_THAT(IsTrue(bSawIsRunningCommandlet,
			TEXT("Official compile snapshot must keep the reviewed IsRunningCommandlet HeaderInline descriptor")));
		ASSERT_THAT(IsTrue(bSawNativeForm,
			TEXT("Official compile snapshot must keep at least one native form")));
	}

	TEST_METHOD(VerifiedCanonicalASTRequiresPublicASTRetention)
	{
		using namespace AngelscriptEditor::StaticJIT;
		using namespace AngelscriptProjectSourceGraphTests;

		FString Root;
		const FString ProjectFile = MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};

		FAngelscriptProjectSourceGraphCompileRequest Request;
		Request.ProjectFile = ProjectFile;
		Request.TargetProfile =
			EAngelscriptJITTargetProfile::EditorDevelopment;
		Request.CaptureProfile =
			EAngelscriptStaticJITCaptureProfile::VerifiedCanonicalAST;

		bool bConsumerCalled = false;
		FString Error;
		ASSERT_THAT(IsFalse(FAngelscriptProjectSourceGraph::Compile(
			Request,
			[&bConsumerCalled](
				const FAngelscriptProjectSourceGraphCompileView&,
				FString*)
			{
				bConsumerCalled = true;
				return true;
			},
			&Error)));
		ASSERT_THAT(IsFalse(bConsumerCalled));
		ASSERT_THAT(IsTrue(Error.Contains(
			TEXT("requires AST snapshot retention"))));
	}
};

#endif
