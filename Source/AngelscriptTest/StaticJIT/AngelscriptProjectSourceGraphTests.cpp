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
		Request.RequestKind =
			EAngelscriptProjectSourceGraphRequestKind::StaticJITArtifact;
		Request.ProjectFile = ProjectFile;
		Request.TargetProfile =
			EAngelscriptJITTargetProfile::EditorDevelopment;
		Request.CaptureProfile =
			EAngelscriptStaticJITCaptureProfile::VerifiedTypedHIR;

		bool bSawCollectFlag = false;
		bool bSawIsRunningCommandlet = false;
		bool bSawNativeForm = false;
		FString Error;
		ASSERT_THAT(IsTrue(FAngelscriptProjectSourceGraph::Compile(
			Request,
			[&](const FAngelscriptProjectSourceGraphCompileView& View,
				FString*)
			{
				bSawCollectFlag =
					View.Engine.bCollectStaticJITCompatibilityBinds;
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
		ASSERT_THAT(IsTrue(bSawIsRunningCommandlet,
			TEXT("Official compile snapshot must keep the reviewed IsRunningCommandlet HeaderInline descriptor")));
		ASSERT_THAT(IsTrue(bSawNativeForm,
			TEXT("Official compile snapshot must keep at least one native form")));
	}
};

#endif
