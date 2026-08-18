#include "CQTest.h"

#include "StaticJIT/AngelscriptJITCommand.h"
#include "StaticJIT/AngelscriptJITCommandlet.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITBackend.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptJITCommandletTests
{
	static FAngelscriptHash256 MakeHash(const FStringView Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("jit-commandlet-test"));
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	static FString MakeProject(FString& OutRoot)
	{
		OutRoot = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/StaticJITCommandlet"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
		IFileManager::Get().MakeDirectory(*OutRoot, true);
		const FString ProjectFile = FPaths::Combine(
			OutRoot, TEXT("CommandFixture.uproject"));
		check(FFileHelper::SaveStringToFile(
			TEXT("{\n  \"FileVersion\": 3,\n  \"Modules\": []\n}\n"),
			*ProjectFile,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
		return ProjectFile;
	}

	static FAngelscriptJITGenerationOutput MakeOutput(
		const FStringView ProjectFile,
		const EAngelscriptJITTargetProfile Profile,
		const FStringView ExecutionIdentity = FStringView())
	{
		using namespace AngelscriptEditor::StaticJIT;
		FAngelscriptJITGenerationRequest Request;
		Request.TargetProfile = Profile;
		Request.ProviderId =
			FAngelscriptJITProjectScaffold::BuildProviderId(ProjectFile);
		Request.ProviderName = TEXT("CommandFixtureProvider");
		Request.OwnerModuleName = TEXT("AngelscriptJIT");
		Request.ArtifactProfile.Hash = MakeHash(
			FAngelscriptJITTargetProfiles::ToString(Profile));
		const TArray<FString> Inputs = {TEXT("CommandletTest")};
		Request.NativeEnvironmentFingerprint =
			FAngelscriptJITTargetProfiles::BuildNativeEnvironmentFingerprint(
				Profile, Inputs);
		FAngelscriptJITGenerationModule& Module =
			Request.Modules.AddDefaulted_GetRef();
		Module.ModuleKey.Hash = MakeHash(TEXT("Module"));
		Module.CanonicalModuleName = TEXT("CommandFixtureModule");
		Module.VirtualSourcePath =
			TEXT("/Angelscript/Game/CommandFixtureModule.as");
		FAngelscriptJITGenerationFunction& Function =
			Request.Functions.AddDefaulted_GetRef();
		Function.ModuleKey = Module.ModuleKey;
		Function.FunctionKey.Hash = MakeHash(TEXT("Function"));
		Function.ExecutionHash = MakeHash(
			ExecutionIdentity.IsEmpty()
				? FStringView(FAngelscriptJITTargetProfiles::ToString(Profile))
				: ExecutionIdentity);
		Function.DebugHash = MakeHash(TEXT("Debug"));
		Function.CanonicalDeclaration = TEXT("int Function()");
		Function.SourceSection = Module.VirtualSourcePath;
		Function.SourceLine = 1;
		Function.SourceColumn = 1;
		Function.EntryAbiHash = FAngelscriptJITGeneration::BuildEntryAbiHash(
			TEXT("int Function()"), true, false, false);
		Function.bHasVMEntry = true;
		Function.ImplementationTemplate =
			TEXT("{VM_ENTRY_COMMENT}void {VM_SYMBOL}(FScriptExecution&, asDWORD*, asQWORD*) {}\n");
		FAngelscriptJITGenerationOutput Output;
		FString Error;
		check(FAngelscriptJITGeneration::Generate(Request, Output, &Error));
		return Output;
	}

	static const FAngelscriptJITGeneratedFile* FindGeneratedFile(
		const FAngelscriptJITGenerationOutput& Output,
		const EAngelscriptJITGeneratedFileKind Kind)
	{
		return Output.Files.FindByPredicate(
			[Kind](const FAngelscriptJITGeneratedFile& File)
			{
				return File.Kind == Kind;
			});
	}

	static FString DescribeFirstDifference(
		const FString& First,
		const FString& Second)
	{
		const int32 SharedLength = FMath::Min(First.Len(), Second.Len());
		int32 DifferenceIndex = 0;
		while (DifferenceIndex < SharedLength
			&& First[DifferenceIndex] == Second[DifferenceIndex])
		{
			++DifferenceIndex;
		}
		const int32 ContextStart = FMath::Max(0, DifferenceIndex - 80);
		return FString::Printf(
			TEXT("first difference at %d; first='%s'; second='%s'"),
			DifferenceIndex,
			*First.Mid(ContextStart, 200).Replace(TEXT("\n"), TEXT("\\n")),
			*Second.Mid(ContextStart, 200).Replace(TEXT("\n"), TEXT("\\n")));
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptJITCommandletTests,
	"Angelscript.TestModule.StaticJIT.Commandlet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ParseExpandsAllProfilesAndRejectsInvalidModeOrProfile)
	{
		using namespace AngelscriptEditor::StaticJIT;
		using namespace AngelscriptJITCommandletTests;
		FString Root;
		const FString ProjectFile = MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};
		FAngelscriptJITCommandRequest Request;
		FString Error;
		ASSERT_THAT(IsTrue(FAngelscriptJITCommand::Parse(
			TEXT("Mode=Generate Profile=All"), ProjectFile, Request, Error), Error));
		ASSERT_THAT(AreEqual(3, Request.TargetProfiles.Num()));
		ASSERT_THAT(AreEqual(
			EAngelscriptJITTargetProfile::EditorDevelopment,
			Request.TargetProfiles[0]));
		ASSERT_THAT(AreEqual(
			EAngelscriptJITTargetProfile::GameShipping,
			Request.TargetProfiles[2]));
		ASSERT_THAT(IsFalse(FAngelscriptJITCommand::Parse(
			TEXT("Mode=Refresh"), ProjectFile, Request, Error)));
		ASSERT_THAT(IsFalse(FAngelscriptJITCommand::Parse(
			TEXT("Mode=Verify Profile=ServerMagic"),
			ProjectFile, Request, Error)));
	}

	TEST_METHOD(ScaffoldCommandCreatesFixedShellWithoutCallingArtifactBuilder)
	{
		using namespace AngelscriptEditor::StaticJIT;
		using namespace AngelscriptJITCommandletTests;
		FString Root;
		const FString ProjectFile = MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};
		FAngelscriptJITCommandRequest Request;
		FString Error;
		ASSERT_THAT(IsTrue(FAngelscriptJITCommand::Parse(
			TEXT("Mode=Scaffold"), ProjectFile, Request, Error), Error));
		int32 BuildCount = 0;
		const FAngelscriptJITCommandResult Result =
			FAngelscriptJITCommand::Execute(Request,
				[&BuildCount](FStringView,
					EAngelscriptJITTargetProfile,
					FAngelscriptJITGenerationOutput&,
					FString*)
				{
					++BuildCount;
					return false;
				});
		ASSERT_THAT(IsTrue(Result.IsSuccess(), Result.Error));
		ASSERT_THAT(AreEqual(0, BuildCount));
		ASSERT_THAT(IsTrue(Result.Scaffold.bRequiresFullBuild));
		ASSERT_THAT(IsTrue(Result.bRequiresFullBuild));
	}

	TEST_METHOD(GenerateAndVerifyUseThreeIsolatedProfiles)
	{
		using namespace AngelscriptEditor::StaticJIT;
		using namespace AngelscriptJITCommandletTests;
		FString Root;
		const FString ProjectFile = MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProjectScaffold::Scaffold(ProjectFile).bSuccess));
		const FString ProviderSelectorPath = FPaths::Combine(
			Root,
			TEXT("Source/AngelscriptJIT/Generated/Provider.generated.cpp"));
		const FDateTime DeliberatelyOldSelectorTimestamp(2001, 1, 1);
		IFileManager::Get().SetTimeStamp(
			*ProviderSelectorPath, DeliberatelyOldSelectorTimestamp);

		FAngelscriptJITCommandRequest GenerateRequest;
		FString Error;
		ASSERT_THAT(IsTrue(FAngelscriptJITCommand::Parse(
			TEXT("Mode=Generate Profile=All"),
			ProjectFile, GenerateRequest, Error), Error));
		TArray<EAngelscriptJITTargetProfile> BuiltProfiles;
		const FAngelscriptJITProjectArtifactBuilder Builder =
			[&BuiltProfiles](const FStringView InProjectFile,
				const EAngelscriptJITTargetProfile Profile,
				FAngelscriptJITGenerationOutput& OutOutput,
				FString*)
			{
				BuiltProfiles.Add(Profile);
				OutOutput = MakeOutput(InProjectFile, Profile);
				return true;
			};
		const FAngelscriptJITCommandResult Generated =
			FAngelscriptJITCommand::Execute(GenerateRequest, Builder);
		ASSERT_THAT(IsTrue(Generated.IsSuccess(), Generated.Error));
		ASSERT_THAT(AreEqual(3, Generated.Profiles.Num()));
		ASSERT_THAT(AreEqual(3, BuiltProfiles.Num()));
		ASSERT_THAT(IsTrue(Generated.bRequiresFullBuild));
		const FDateTime SelectorTimestampAfterGenerate =
			IFileManager::Get().GetTimeStamp(*ProviderSelectorPath);
		ASSERT_THAT(IsTrue(
			SelectorTimestampAfterGenerate > DeliberatelyOldSelectorTimestamp,
			TEXT("Adding generated module sources must invalidate the stable selector translation unit")));
		for (const FAngelscriptJITCommandProfileResult& Profile :
			Generated.Profiles)
		{
			ASSERT_THAT(IsTrue(Profile.bRequiresFullBuild));
			ASSERT_THAT(IsTrue(Profile.OutputDirectory.Contains(
				FAngelscriptJITTargetProfiles::ToString(
					Profile.TargetProfile))));
			ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
				*FPaths::Combine(Profile.OutputDirectory,
					TEXT("CommandFixtureModule.")
					+ MakeHash(TEXT("Module")).ToHexString().Left(8)
					+ TEXT(".")
					+ FAngelscriptJITTargetProfiles::ToString(
						Profile.TargetProfile)
					+ TEXT(".jit.cpp")))));
		}

		FAngelscriptJITCommandRequest VerifyRequest;
		ASSERT_THAT(IsTrue(FAngelscriptJITCommand::Parse(
			TEXT("Mode=Verify Profile=All"),
			ProjectFile, VerifyRequest, Error), Error));
		BuiltProfiles.Reset();
		const FAngelscriptJITCommandResult Verified =
			FAngelscriptJITCommand::Execute(VerifyRequest, Builder);
		ASSERT_THAT(IsTrue(Verified.IsSuccess(), Verified.Error));
		ASSERT_THAT(AreEqual(3, BuiltProfiles.Num()));
		ASSERT_THAT(IsFalse(Verified.bRequiresFullBuild));
		ASSERT_THAT(IsTrue(
			IFileManager::Get().GetTimeStamp(*ProviderSelectorPath)
				== SelectorTimestampAfterGenerate,
			TEXT("Read-only Verify must not touch the selector translation unit")));
		for (const FAngelscriptJITCommandProfileResult& Profile :
			Verified.Profiles)
		{
			ASSERT_THAT(IsFalse(Profile.bRequiresFullBuild));
			ASSERT_THAT(IsTrue(Profile.ProjectGeneration.Comparison.IsCurrent()));
			ASSERT_THAT(IsFalse(IFileManager::Get().DirectoryExists(
				*Profile.ProjectGeneration.VerificationDirectory)));
		}
	}

	TEST_METHOD(ProductionBuilderCompilesProjectScriptsAndVerifyIsReadOnly)
	{
		using namespace AngelscriptEditor::StaticJIT;
		using namespace AngelscriptJITCommandletTests;
		FString Root;
		const FString ProjectFile = MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};
		const FString ScriptRoot = FPaths::Combine(Root, TEXT("Script"));
		ASSERT_THAT(IsTrue(IFileManager::Get().MakeDirectory(
			*ScriptRoot, true)));
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			TEXT("int ProjectHelper() { return 17; }\n")
			TEXT("UFUNCTION()\n")
			TEXT("int ProjectEntry() { return ProjectHelper(); }\n"),
			*FPaths::Combine(ScriptRoot, TEXT("ProjectProvider.as")),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)));
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProjectScaffold::Scaffold(ProjectFile).bSuccess));

		FAngelscriptJITCommandRequest GenerateRequest;
		FString Error;
		ASSERT_THAT(IsTrue(FAngelscriptJITCommand::Parse(
			TEXT("Mode=Generate Profile=EditorDevelopment"),
			ProjectFile, GenerateRequest, Error), Error));
		TArray<FAngelscriptJITGenerationOutput> BuiltOutputs;
		const FAngelscriptJITProjectArtifactBuilder ProductionBuilder =
			[&BuiltOutputs](const FStringView InProjectFile,
				const EAngelscriptJITTargetProfile Profile,
				FAngelscriptJITGenerationOutput& OutOutput,
				FString* OutError)
			{
				const bool bBuilt =
					FAngelscriptJITProjectGeneration::BuildProjectArtifacts(
						InProjectFile, Profile, OutOutput, OutError);
				if (bBuilt)
				{
					BuiltOutputs.Add(OutOutput);
				}
				return bBuilt;
			};
		FAngelscriptTypedASTJIT::ResetTaskObservationForTesting();
		const FAngelscriptJITCommandResult Generated =
			FAngelscriptJITCommand::Execute(
				GenerateRequest, ProductionBuilder);
		ASSERT_THAT(IsTrue(Generated.IsSuccess(), Generated.Error));
		const FAngelscriptTypedASTJITTaskObservation
			GenerateObservation =
				FAngelscriptTypedASTJIT::GetTaskObservationForTesting();
		ASSERT_THAT(AreEqual(1,
			GenerateObservation.CreatedInstanceCount));
		ASSERT_THAT(AreEqual(1,
			GenerateObservation.GenerateInvocationCount));
		ASSERT_THAT(AreEqual(1,
			GenerateObservation.DestroyedInstanceCount));
		ASSERT_THAT(AreEqual(1,
			GenerateObservation.AcceptedTaskCount));
		ASSERT_THAT(AreEqual(0,
			GenerateObservation.RejectedTaskCount));
		ASSERT_THAT(IsTrue(
			GenerateObservation.LastVerifiedTypedHIRCount > 0));
		ASSERT_THAT(IsTrue(
			GenerateObservation.bLastTaskUsedOnlySameCompilationHIR));
		ASSERT_THAT(AreEqual(
			GenerateObservation.LastUFunctionRootCount,
			GenerateObservation.LastCallClosurePlanCount,
			TEXT("Every selected production root should own one closure plan")));
		ASSERT_THAT(IsTrue(
			GenerateObservation.LastPlannedCallCount > 0,
			TEXT("The real ProjectEntry -> ProjectHelper edge should enter the production closure")));
		ASSERT_THAT(IsTrue(
			GenerateObservation.LastInternalSemanticHelperCallCount > 0,
			TEXT("The real non-root same-task helper should be planned as an internal semantic helper")));
		ASSERT_THAT(AreEqual(1, Generated.Profiles.Num()));
		ASSERT_THAT(IsTrue(Generated.bRequiresFullBuild));
		ASSERT_THAT(IsTrue(Generated.Profiles[0].bRequiresFullBuild));
		const FString OutputDirectory =
			Generated.Profiles[0].OutputDirectory;
		const FString ManifestPath = FPaths::Combine(
			OutputDirectory, TEXT("ProviderManifest.generated.json"));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(*ManifestPath)));
		TArray<FString> ModuleSourceFiles;
		IFileManager::Get().FindFilesRecursive(
			ModuleSourceFiles, *OutputDirectory, TEXT("*.jit.cpp"), true, false);
		ASSERT_THAT(AreEqual(1, ModuleSourceFiles.Num(),
			TEXT("The real builder should group both functions into their AS module source")));
		TArray<uint8> ManifestBeforeVerify;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToArray(
			ManifestBeforeVerify, *ManifestPath)));

		FAngelscriptJITCommandRequest VerifyRequest;
		ASSERT_THAT(IsTrue(FAngelscriptJITCommand::Parse(
			TEXT("Mode=Verify Profile=EditorDevelopment"),
			ProjectFile, VerifyRequest, Error), Error));
		FAngelscriptTypedASTJIT::ResetTaskObservationForTesting();
		const FAngelscriptJITCommandResult Verified =
			FAngelscriptJITCommand::Execute(
				VerifyRequest, ProductionBuilder);
		const FAngelscriptTypedASTJITTaskObservation VerifyObservation =
			FAngelscriptTypedASTJIT::GetTaskObservationForTesting();
		ASSERT_THAT(AreEqual(1, VerifyObservation.CreatedInstanceCount));
		ASSERT_THAT(AreEqual(1, VerifyObservation.GenerateInvocationCount));
		ASSERT_THAT(AreEqual(1, VerifyObservation.DestroyedInstanceCount));
		ASSERT_THAT(AreEqual(1, VerifyObservation.AcceptedTaskCount));
		ASSERT_THAT(AreEqual(0, VerifyObservation.RejectedTaskCount));
		ASSERT_THAT(IsTrue(VerifyObservation.LastVerifiedTypedHIRCount > 0));
		ASSERT_THAT(IsTrue(
			VerifyObservation.bLastTaskUsedOnlySameCompilationHIR));
		ASSERT_THAT(AreEqual(
			VerifyObservation.LastUFunctionRootCount,
			VerifyObservation.LastCallClosurePlanCount));
		ASSERT_THAT(IsTrue(
			VerifyObservation.LastPlannedCallCount > 0));
		ASSERT_THAT(IsTrue(
			VerifyObservation.LastInternalSemanticHelperCallCount > 0));
		ASSERT_THAT(AreEqual(2, BuiltOutputs.Num()));
		ASSERT_THAT(AreEqual(
			BuiltOutputs[0].ProviderGeneration.Hash.ToHexString(),
			BuiltOutputs[1].ProviderGeneration.Hash.ToHexString(),
			TEXT("Repeated production builds must have one Provider generation")));
		const FAngelscriptJITGeneratedFile* FirstProvider = FindGeneratedFile(
			BuiltOutputs[0], EAngelscriptJITGeneratedFileKind::ProviderSource);
		const FAngelscriptJITGeneratedFile* SecondProvider = FindGeneratedFile(
			BuiltOutputs[1], EAngelscriptJITGeneratedFileKind::ProviderSource);
		ASSERT_THAT(IsNotNull(FirstProvider));
		ASSERT_THAT(IsNotNull(SecondProvider));
		ASSERT_THAT(IsFalse(FirstProvider->Content.Contains(
			TEXT("FJitRef_")),
			TEXT("Provider source must not retain legacy process-local references")));
		ASSERT_THAT(IsTrue(FirstProvider->Content == SecondProvider->Content,
			DescribeFirstDifference(
				FirstProvider->Content, SecondProvider->Content)));
		const FAngelscriptJITGeneratedFunction* const ProjectEntry =
			BuiltOutputs[0].Functions.FindByPredicate(
				[](const FAngelscriptJITGeneratedFunction& Function)
				{
					return Function.CanonicalDeclaration.Contains(
						TEXT("ProjectEntry"));
				});
		ASSERT_THAT(IsNotNull(ProjectEntry));
		ASSERT_THAT(AreEqual(1, ProjectEntry->ReferenceSlots.Num(),
			TEXT("The project entry should retain exactly its direct helper identity")));
		ASSERT_THAT(AreEqual(0u, ProjectEntry->ReferenceSlots[0].SlotIndex));
		ASSERT_THAT(IsTrue(
			ProjectEntry->ReferenceSlots[0].Reference.Kind
				== EAngelscriptArtifactReferenceKind::ScriptFunction));
		ASSERT_THAT(IsFalse(
			ProjectEntry->ReferenceSlots[0].Reference.ExpectedAbi.IsZero()));
		const bool bUsesStableFunctionSlot = BuiltOutputs[0].Files.ContainsByPredicate(
			[](const FAngelscriptJITGeneratedFile& File)
			{
				return File.Kind
						== EAngelscriptJITGeneratedFileKind::ModuleSource
					&& File.Content.Contains(
						TEXT("FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(\n"
							"\t\t\tExecution, 0u)"));
			});
		ASSERT_THAT(IsTrue(bUsesStableFunctionSlot,
			TEXT("Project caller should resolve its callee through stable slot zero")));
		ASSERT_THAT(IsTrue(Verified.IsSuccess(), Verified.Error));
		ASSERT_THAT(AreEqual(1, Verified.Profiles.Num()));
		ASSERT_THAT(IsFalse(Verified.bRequiresFullBuild));
		ASSERT_THAT(IsFalse(Verified.Profiles[0].bRequiresFullBuild));
		ASSERT_THAT(IsTrue(
			Verified.Profiles[0].ProjectGeneration.Comparison.IsCurrent()));
		ASSERT_THAT(IsFalse(IFileManager::Get().DirectoryExists(
			*Verified.Profiles[0].ProjectGeneration.VerificationDirectory)));
		TArray<uint8> ManifestAfterVerify;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToArray(
			ManifestAfterVerify, *ManifestPath)));
		ASSERT_THAT(IsTrue(ManifestBeforeVerify == ManifestAfterVerify,
			TEXT("Verify must not mutate checked-in generated output")));

		FString ManifestText;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			ManifestText, *ManifestPath)));
		ManifestText += TEXT("\nintentional-stale-marker\n");
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			ManifestText,
			*ManifestPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)));
		TArray<uint8> StaleManifestBeforeCommandlet;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToArray(
			StaleManifestBeforeCommandlet, *ManifestPath)));
		TestRunner->AddExpectedError(
			TEXT("Status=Stale"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		UAngelscriptJITCommandlet* Commandlet =
			NewObject<UAngelscriptJITCommandlet>();
		ASSERT_THAT(IsNotNull(Commandlet));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(EAngelscriptJITCommandExitCode::ExecutionFailure),
			Commandlet->Main(FString::Printf(
				TEXT("Mode=Verify Profile=EditorDevelopment Project=\"%s\""),
				*ProjectFile))));
		TArray<uint8> StaleManifestAfterCommandlet;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToArray(
			StaleManifestAfterCommandlet, *ManifestPath)));
		ASSERT_THAT(IsTrue(
			StaleManifestBeforeCommandlet == StaleManifestAfterCommandlet,
			TEXT("Failing UObject commandlet Verify must remain read-only")));
	}

	TEST_METHOD(VerifyReportsFullIdentityAndAllOwnedFileDifferences)
	{
		using namespace AngelscriptEditor::StaticJIT;
		using namespace AngelscriptJITCommandletTests;
		FString Root;
		const FString ProjectFile = MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProjectScaffold::Scaffold(ProjectFile).bSuccess));

		FAngelscriptJITGenerationOutput Baseline = MakeOutput(
			ProjectFile,
			EAngelscriptJITTargetProfile::EditorDevelopment,
			TEXT("VerifyBaseline"));
		FAngelscriptJITGenerationOutput Changed = MakeOutput(
			ProjectFile,
			EAngelscriptJITTargetProfile::EditorDevelopment,
			TEXT("VerifyChanged"));
		FAngelscriptJITGenerationOutput BaselineProjectOutput;
		FAngelscriptJITGenerationOutput ChangedProjectOutput;
		FString Error;
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProjectGeneration::PrepareProjectProfileOutput(
				Baseline, BaselineProjectOutput, &Error), Error));
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProjectGeneration::PrepareProjectProfileOutput(
				Changed, ChangedProjectOutput, &Error), Error));
		const FString OutputDirectory =
			FAngelscriptJITProjectGeneration::GetProfileOutputDirectory(
				ProjectFile,
				EAngelscriptJITTargetProfile::EditorDevelopment);
		ASSERT_THAT(IsTrue(OutputDirectory.EndsWith(
			TEXT("Generated/EditorDevelopment"))));
		ASSERT_THAT(IsFalse(OutputDirectory.Contains(TEXT("/Profiles/"))));
		ASSERT_THAT(IsTrue(FAngelscriptJITProjectGeneration::Generate(
			OutputDirectory, BaselineProjectOutput).bSuccess));

		FAngelscriptJITCommandRequest VerifyRequest;
		ASSERT_THAT(IsTrue(FAngelscriptJITCommand::Parse(
			TEXT("Mode=Verify Profile=EditorDevelopment"),
			ProjectFile, VerifyRequest, Error), Error));
		const FAngelscriptJITProjectArtifactBuilder ChangedBuilder =
			[Changed](FStringView,
				EAngelscriptJITTargetProfile,
				FAngelscriptJITGenerationOutput& OutOutput,
				FString*)
			{
				OutOutput = Changed;
				return true;
			};
		const FAngelscriptJITCommandResult Verified =
			FAngelscriptJITCommand::Execute(VerifyRequest, ChangedBuilder);
		ASSERT_THAT(IsFalse(Verified.IsSuccess()));
		ASSERT_THAT(IsFalse(Verified.bRequiresFullBuild));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(EAngelscriptJITCommandExitCode::ExecutionFailure),
			static_cast<int32>(Verified.ExitCode)));

		const FAngelscriptJITGeneratedFunction& Function =
			ChangedProjectOutput.Functions[0];
		const FAngelscriptJITGeneratedModule& Module =
			ChangedProjectOutput.Modules[0];
		const FString OldModuleSource =
			BaselineProjectOutput.Modules[0].SourceRelativePath;
		const FString& Report = Verified.Error;
		ASSERT_THAT(IsTrue(Report.Contains(TEXT("Status=Stale"))));
		ASSERT_THAT(IsTrue(Report.Contains(FString::Printf(
			TEXT("ProviderId=%s"),
			*ChangedProjectOutput.ProviderId.Hash.ToHexString()))));
		ASSERT_THAT(IsTrue(Report.Contains(
			TEXT("Profile=EditorDevelopment"))));
		ASSERT_THAT(IsTrue(Report.Contains(FString::Printf(
			TEXT("ProviderAbiRevision=%u"),
			FAngelscriptJITProviderAbi::Revision))));
		ASSERT_THAT(IsTrue(Report.Contains(FString::Printf(
			TEXT("StableFunctionKey=%s"),
			*Function.FunctionKey.Hash.ToHexString()))));
		ASSERT_THAT(IsTrue(Report.Contains(
			TEXT("ModuleSource=") + Function.ModuleSourceRelativePath)));
		ASSERT_THAT(IsTrue(Report.Contains(
			TEXT("CanonicalModuleName=") + Module.CanonicalModuleName)));
		ASSERT_THAT(IsTrue(Report.Contains(
			TEXT("VirtualSourcePath=") + Module.VirtualSourcePath)));
		ASSERT_THAT(IsTrue(Report.Contains(
			TEXT("Declaration=") + Function.CanonicalDeclaration)));
		ASSERT_THAT(IsTrue(Report.Contains(
			TEXT("Source=") + Function.SourceSection
				+ TEXT(":") + LexToString(Function.SourceLine)
				+ TEXT(":") + LexToString(Function.SourceColumn))));
		ASSERT_THAT(IsTrue(Report.Contains(
			TEXT("Symbol=") + Function.SymbolPrefix)));
		ASSERT_THAT(IsTrue(Report.Contains(
			TEXT("Difference Kind=ContentMismatch Path=")
				+ Function.ModuleSourceRelativePath)));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
			*FPaths::Combine(OutputDirectory, OldModuleSource)),
			TEXT("Read-only Verify must retain the old owned module source")));
	}

	TEST_METHOD(UObjectCommandletExposesScaffoldMode)
	{
		using namespace AngelscriptJITCommandletTests;
		FString Root;
		const FString ProjectFile = MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};
		UAngelscriptJITCommandlet* Commandlet =
			NewObject<UAngelscriptJITCommandlet>();
		ASSERT_THAT(IsNotNull(Commandlet));
		ASSERT_THAT(AreEqual(0, Commandlet->Main(FString::Printf(
			TEXT("Mode=Scaffold Project=\"%s\""), *ProjectFile))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
