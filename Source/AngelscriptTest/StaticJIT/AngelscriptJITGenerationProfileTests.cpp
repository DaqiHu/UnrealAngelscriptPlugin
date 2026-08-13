#include "CQTest.h"

#include "HAL/PlatformProperties.h"
#include "Misc/EngineVersion.h"
#include "StaticJIT/AngelscriptJITGeneration.h"
#include "StaticJIT/AngelscriptJITProviderMatcher.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptJITGenerationProfileTests
{
	static FAngelscriptHash256 MakeHash(const FStringView Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("jit-generation-profile-test"));
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	static FAngelscriptArtifactProfileKey BuildArtifactProfile(
		const EAngelscriptJITTargetProfile TargetProfile)
	{
		const TOptional<FAngelscriptJITTargetProfileTraits> Traits =
			FAngelscriptJITTargetProfiles::GetTraits(TargetProfile);
		check(Traits.IsSet());

		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs.Add(TEXT("ProfileTestCompatibility=1"));
		FAngelscriptContextDescriptor Context;
		Context.CanonicalInputs = {
			FString::Printf(TEXT("TargetProfile=%s"),
				FAngelscriptJITTargetProfiles::ToString(TargetProfile)),
			FString::Printf(TEXT("EDITOR=%d"), Traits->bEditor),
			FString::Printf(TEXT("EDITORONLY_DATA=%d"),
				Traits->bEditorOnlyData),
			FString::Printf(TEXT("RELEASE=%d"), Traits->bRelease),
			FString::Printf(TEXT("TEST=%d"), Traits->bTest),
			FString::Printf(TEXT("CookedBindings=%d"),
				Traits->bUseCookedBindings),
		};
		return FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
			FAngelscriptArtifactIdentityBuilder::BuildCompatibilityKey(
				Compatibility),
			FAngelscriptArtifactIdentityBuilder::BuildContextKey(Context));
	}

	static FAngelscriptJITGenerationRequest MakeRequest(
		const EAngelscriptJITTargetProfile TargetProfile)
	{
		FAngelscriptJITGenerationRequest Request;
		Request.TargetProfile = TargetProfile;
		Request.ProviderId.Hash = MakeHash(TEXT("ProfileProvider"));
		Request.ProviderName = TEXT("ProfileProvider");
		Request.OwnerModuleName = TEXT("ProfileProviderModule");
		Request.ArtifactProfile = BuildArtifactProfile(TargetProfile);
		const TArray<FString> NativeInputs = {
			TEXT("Platform=Win64"),
			TEXT("Architecture=x64"),
			TEXT("Compiler=ProfileTestCompiler"),
		};
		Request.NativeEnvironmentFingerprint =
			FAngelscriptJITTargetProfiles::BuildNativeEnvironmentFingerprint(
				TargetProfile, NativeInputs);

		FAngelscriptJITGenerationModule& Module =
			Request.Modules.AddDefaulted_GetRef();
		Module.ModuleKey.Hash = MakeHash(TEXT("ProfileModule"));
		Module.CanonicalModuleName = TEXT("ProfileModule");
		Module.VirtualSourcePath = TEXT("/Angelscript/Game/ProfileModule.as");

		FAngelscriptJITGenerationFunction& Function =
			Request.Functions.AddDefaulted_GetRef();
		Function.ModuleKey = Module.ModuleKey;
		Function.FunctionKey.Hash = MakeHash(TEXT("ProfileFunction"));
		Function.ExecutionHash = MakeHash(TEXT("ProfileExecution"));
		Function.DebugHash = MakeHash(TEXT("ProfileDebug"));
		Function.CanonicalDeclaration = TEXT("void ProfileFunction()");
		Function.SourceSection = Module.VirtualSourcePath;
		Function.SourceLine = 1;
		Function.SourceColumn = 1;
		Function.EntryAbiHash =
			FAngelscriptJITGeneration::BuildEntryAbiHash(
				TEXT("void ProfileFunction()"), true, false, false);
		Function.bHasVMEntry = true;
		Function.ImplementationTemplate =
			TEXT("{VM_ENTRY_COMMENT}void {VM_SYMBOL}(FScriptExecution&, asDWORD*, asQWORD*) {}\n");
		return Request;
	}

	static const FAngelscriptJITGeneratedFile* FindManifest(
		const FAngelscriptJITGenerationOutput& Output)
	{
		return Output.Files.FindByPredicate(
			[](const FAngelscriptJITGeneratedFile& File)
			{
				return File.Kind
					== EAngelscriptJITGeneratedFileKind::ProviderManifest;
			});
	}

	static const FAngelscriptJITGeneratedFile* FindModuleSource(
		const FAngelscriptJITGenerationOutput& Output)
	{
		return Output.Files.FindByPredicate(
			[](const FAngelscriptJITGeneratedFile& File)
			{
				return File.Kind
					== EAngelscriptJITGeneratedFileKind::ModuleSource;
			});
	}

	static void VMEntry(FScriptExecution&, asDWORD*, asQWORD*)
	{
	}

	class FNoReferences final : public IAngelscriptJITReferenceResolver
	{
	public:
		FAngelscriptJITReferenceResolution Resolve(
			const FAngelscriptArtifactReference&) const override
		{
			return {EAngelscriptJITReferenceResolutionCode::Missing, nullptr};
		}
	};

	struct FMatchFixture
	{
		TSharedPtr<FAngelscriptJITProviderCatalog, ESPMode::ThreadSafe> Catalog;
		TSharedPtr<FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> Snapshot;
		FAngelscriptJITFunctionMatchInput Input;
		FNoReferences Resolver;

		explicit FMatchFixture(
			const FAngelscriptJITGenerationOutput& Output)
		{
			check(Output.Functions.Num() == 1);
			const FAngelscriptJITGeneratedFunction& Function =
				Output.Functions[0];
			Catalog = MakeShared<FAngelscriptJITProviderCatalog,
				ESPMode::ThreadSafe>();
			Catalog->ProviderId = Output.ProviderId;
			Catalog->ProviderGeneration = Output.ProviderGeneration;
			Catalog->ArtifactSetDigest = Output.ArtifactSetDigest;
			Catalog->ArtifactProfile = Output.ArtifactProfile;
			Catalog->NativeEnvironmentFingerprint =
				Output.NativeEnvironmentFingerprint;
			Catalog->ProviderName = TEXT("ProfileProvider");
			Catalog->OwnerModuleName = TEXT("ProfileProviderModule");

			FAngelscriptJITProviderCatalogEntry& Entry =
				Catalog->Entries.AddDefaulted_GetRef();
			Entry.Flags = Function.Flags;
			Entry.ModuleKey = Function.ModuleKey;
			Entry.FunctionKey = Function.FunctionKey;
			Entry.ExecutionHash = Function.ExecutionHash;
			Entry.DebugHash = Function.DebugHash;
			Entry.ArtifactProfile = Output.ArtifactProfile;
			Entry.EntryAbiHash = Function.EntryAbiHash;
			Entry.NativeEnvironmentFingerprint =
				Output.NativeEnvironmentFingerprint;
			Entry.EntryPoints.VMEntry = &VMEntry;

			Snapshot = MakeShared<FAngelscriptJITProviderRegistrySnapshot,
				ESPMode::ThreadSafe>();
			Snapshot->PublicationOrdinal = 1;
			Snapshot->Providers.Add(Catalog);

			Input.ModuleKey = Entry.ModuleKey;
			Input.Identity.FunctionKey = Entry.FunctionKey;
			Input.Identity.Content.Execution = Entry.ExecutionHash;
			Input.Identity.Content.Debug = Entry.DebugHash;
			Input.Identity.Profile = Entry.ArtifactProfile;
			Input.EntryAbiHash = Entry.EntryAbiHash;
			Input.NativeEnvironmentFingerprint =
				Entry.NativeEnvironmentFingerprint;
			Input.CurrentArtifactSetDigest = Catalog->ArtifactSetDigest;
			Input.RequiredEntryPoints = EAngelscriptJITRequiredEntryPoints::VM;
		}

		EAngelscriptArtifactMatchResult Match() const
		{
			return FAngelscriptJITProviderMatcher::Match(
				*Snapshot, Input, Resolver).Result;
		}
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptJITGenerationProfileTests,
	"Angelscript.TestModule.StaticJIT.Generation.Profiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ConcreteProfilesFreezeExplicitScriptAndBindingSurfaces)
	{
		using namespace AngelscriptJITGenerationProfileTests;
		struct FExpected
		{
			EAngelscriptJITTargetProfile Profile;
			const TCHAR* Name;
			bool bEditor;
			bool bEditorOnlyData;
			bool bRelease;
			bool bTest;
			bool bCookedBindings;
			bool bDevelopmentScripts;
			bool bEditorScripts;
		};
		const FExpected Expected[] = {
			{EAngelscriptJITTargetProfile::EditorDevelopment,
				TEXT("EditorDevelopment"), true, true, false, true,
				false, true, true},
			{EAngelscriptJITTargetProfile::GameDevelopment,
				TEXT("GameDevelopment"), false, false, false, true,
				true, false, false},
			{EAngelscriptJITTargetProfile::GameShipping,
				TEXT("GameShipping"), false, false, true, false,
				true, false, false},
		};

		for (const FExpected& Value : Expected)
		{
			ASSERT_THAT(AreEqual(FString(Value.Name),
				FString(FAngelscriptJITTargetProfiles::ToString(Value.Profile))));
			EAngelscriptJITTargetProfile Parsed =
				EAngelscriptJITTargetProfile::Invalid;
			ASSERT_THAT(IsTrue(FAngelscriptJITTargetProfiles::TryParse(
				Value.Name, Parsed)));
			ASSERT_THAT(IsTrue(Parsed == Value.Profile));
			const TOptional<FAngelscriptJITTargetProfileTraits> Traits =
				FAngelscriptJITTargetProfiles::GetTraits(Value.Profile);
			ASSERT_THAT(IsTrue(Traits.IsSet()));
			ASSERT_THAT(AreEqual(Value.bEditor, Traits->bEditor));
			ASSERT_THAT(AreEqual(Value.bEditorOnlyData,
				Traits->bEditorOnlyData));
			ASSERT_THAT(AreEqual(Value.bRelease, Traits->bRelease));
			ASSERT_THAT(AreEqual(Value.bTest, Traits->bTest));
			ASSERT_THAT(AreEqual(Value.bCookedBindings,
				Traits->bUseCookedBindings));
			ASSERT_THAT(AreEqual(Value.bDevelopmentScripts,
				Traits->bIncludeDevelopmentScripts));
			ASSERT_THAT(AreEqual(Value.bEditorScripts,
				Traits->bIncludeEditorScripts));
		}

		EAngelscriptJITTargetProfile Parsed =
			EAngelscriptJITTargetProfile::EditorDevelopment;
		ASSERT_THAT(IsFalse(FAngelscriptJITTargetProfiles::TryParse(
			TEXT("Development"), Parsed)));
		ASSERT_THAT(IsTrue(Parsed == EAngelscriptJITTargetProfile::Invalid));
		ASSERT_THAT(IsFalse(FAngelscriptJITTargetProfiles::GetTraits(
			EAngelscriptJITTargetProfile::All).IsSet()));

		const TArray<FString> ForwardInputs = {
			TEXT("Compiler=ProfileTestCompiler"),
			TEXT("Architecture=x64"),
			TEXT("Platform=Win64"),
		};
		const TArray<FString> ReorderedInputs = {
			TEXT("Platform=Win64"),
			TEXT("Architecture=x64"),
			TEXT("Compiler=ProfileTestCompiler"),
			TEXT("Compiler=ProfileTestCompiler"),
		};
		ASSERT_THAT(IsTrue(
			FAngelscriptJITTargetProfiles::BuildNativeEnvironmentFingerprint(
				EAngelscriptJITTargetProfile::EditorDevelopment,
				ForwardInputs)
			== FAngelscriptJITTargetProfiles::BuildNativeEnvironmentFingerprint(
				EAngelscriptJITTargetProfile::EditorDevelopment,
				ReorderedInputs)));
		ASSERT_THAT(IsTrue(
			FAngelscriptJITTargetProfiles::BuildNativeEnvironmentFingerprint(
				EAngelscriptJITTargetProfile::All, ForwardInputs).IsZero()));
	}

	TEST_METHOD(ProfileBoundGenerationHasThreeIsolatedOutputIdentities)
	{
		using namespace AngelscriptJITGenerationProfileTests;
		const EAngelscriptJITTargetProfile Profiles[] = {
			EAngelscriptJITTargetProfile::EditorDevelopment,
			EAngelscriptJITTargetProfile::GameDevelopment,
			EAngelscriptJITTargetProfile::GameShipping,
		};
		TArray<FAngelscriptJITGenerationOutput> Outputs;
		TSet<FString> ModuleSourceBaseNames;
		for (const EAngelscriptJITTargetProfile Profile : Profiles)
		{
			FAngelscriptJITGenerationOutput& Output =
				Outputs.AddDefaulted_GetRef();
			FString Error;
			ASSERT_THAT(IsTrue(FAngelscriptJITGeneration::Generate(
				MakeRequest(Profile), Output, &Error), Error));
			ASSERT_THAT(IsTrue(Output.TargetProfile == Profile));
			const FAngelscriptJITGeneratedFile* Manifest = FindManifest(Output);
			ASSERT_THAT(IsNotNull(Manifest));
			ASSERT_THAT(IsTrue(Manifest->Content.Contains(FString::Printf(
				TEXT("\"targetProfile\": \"%s\""),
				FAngelscriptJITTargetProfiles::ToString(Profile)))));
			const FAngelscriptJITGeneratedFile* ModuleSource =
				FindModuleSource(Output);
			ASSERT_THAT(IsNotNull(ModuleSource));
			const FString ModuleSourceBaseName =
				FPaths::GetCleanFilename(ModuleSource->RelativePath);
			ASSERT_THAT(IsFalse(ModuleSourceBaseNames.Contains(
				ModuleSourceBaseName), FString::Printf(
				TEXT("UBT flattens intermediate object names, so every target "
					"profile requires a distinct module-source basename: %s"),
				*ModuleSourceBaseName)));
			ModuleSourceBaseNames.Add(ModuleSourceBaseName);
			const TCHAR* ExpectedGuard =
				Profile == EAngelscriptJITTargetProfile::EditorDevelopment
					? TEXT("#if WITH_EDITOR && UE_BUILD_DEVELOPMENT")
					: Profile == EAngelscriptJITTargetProfile::GameDevelopment
						? TEXT("#if !WITH_EDITOR && UE_BUILD_DEVELOPMENT")
						: TEXT("#if !WITH_EDITOR && UE_BUILD_SHIPPING");
			ASSERT_THAT(IsTrue(ModuleSource->Content.Contains(ExpectedGuard)));
		}

		for (int32 Left = 0; Left < Outputs.Num(); ++Left)
		{
			for (int32 Right = Left + 1; Right < Outputs.Num(); ++Right)
			{
				ASSERT_THAT(IsFalse(Outputs[Left].ArtifactProfile.Hash
					== Outputs[Right].ArtifactProfile.Hash));
				ASSERT_THAT(IsFalse(
					Outputs[Left].NativeEnvironmentFingerprint
					== Outputs[Right].NativeEnvironmentFingerprint));
				ASSERT_THAT(IsFalse(Outputs[Left].ProviderGeneration
					== Outputs[Right].ProviderGeneration));
			}
		}

		FAngelscriptJITGenerationRequest Invalid = MakeRequest(
			EAngelscriptJITTargetProfile::EditorDevelopment);
		Invalid.TargetProfile = EAngelscriptJITTargetProfile::All;
		FAngelscriptJITGenerationOutput InvalidOutput;
		FString Error;
		ASSERT_THAT(IsFalse(FAngelscriptJITGeneration::Generate(
			Invalid, InvalidOutput, &Error)));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("concrete target profile"))));
	}

	TEST_METHOD(GameProfilesUseTheTargetPlatformFamilyForNativeIdentity)
	{
		const TArray<FString> GameNativeInputs = {
			FString::Printf(TEXT("Platform=%hs"),
				FPlatformProperties::IniPlatformName()),
			FString::Printf(TEXT("PointerBits=%u"),
				static_cast<uint32>(sizeof(void*) * 8)),
			TEXT("Engine=") + FEngineVersion::Current().ToString(),
			TEXT("Bindings=Cooked"),
		};
		for (const EAngelscriptJITTargetProfile Profile : {
				EAngelscriptJITTargetProfile::GameDevelopment,
				EAngelscriptJITTargetProfile::GameShipping})
		{
			ASSERT_THAT(IsTrue(
				FAngelscriptJITTargetProfiles::
					BuildCurrentNativeEnvironmentFingerprint(Profile)
				== FAngelscriptJITTargetProfiles::
					BuildNativeEnvironmentFingerprint(
						Profile, GameNativeInputs)));
		}
	}

	TEST_METHOD(MatcherRejectsProfileEnvironmentAndEntryAbiBoundaries)
	{
		using namespace AngelscriptJITGenerationProfileTests;
		FAngelscriptJITGenerationOutput EditorOutput;
		FString Error;
		ASSERT_THAT(IsTrue(FAngelscriptJITGeneration::Generate(
			MakeRequest(EAngelscriptJITTargetProfile::EditorDevelopment),
			EditorOutput, &Error), Error));
		const FAngelscriptJITGenerationRequest GameRequest = MakeRequest(
			EAngelscriptJITTargetProfile::GameDevelopment);

		FMatchFixture Fixture(EditorOutput);
		ASSERT_THAT(IsTrue(Fixture.Match()
			== EAngelscriptArtifactMatchResult::Exact));

		Fixture.Input.Identity.Profile = GameRequest.ArtifactProfile;
		ASSERT_THAT(IsTrue(Fixture.Match()
			== EAngelscriptArtifactMatchResult::ProfileMismatch));
		Fixture.Input.Identity.Profile = EditorOutput.ArtifactProfile;

		Fixture.Input.NativeEnvironmentFingerprint =
			GameRequest.NativeEnvironmentFingerprint;
		ASSERT_THAT(IsTrue(Fixture.Match()
			== EAngelscriptArtifactMatchResult::EnvironmentMismatch));
		Fixture.Input.NativeEnvironmentFingerprint =
			EditorOutput.NativeEnvironmentFingerprint;

		Fixture.Input.EntryAbiHash = MakeHash(TEXT("OtherEntryAbi"));
		ASSERT_THAT(IsTrue(Fixture.Match()
			== EAngelscriptArtifactMatchResult::EntryAbiMismatch));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
