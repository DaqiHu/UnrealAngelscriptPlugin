#include "CQTest.h"

#include "AngelscriptTestJITProbes.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptJITTestModuleOwnershipTests_Private
{
	static FString GetPluginRoot()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectPluginsDir(), TEXT("Angelscript")));
	}

	static bool LoadText(const FString& RelativePath, FString& OutText)
	{
		return FFileHelper::LoadFileToString(
			OutText, *FPaths::Combine(GetPluginRoot(), RelativePath));
	}

	static TSharedPtr<FJsonObject> LoadPluginDescriptor()
	{
		FString Contents;
		if (!LoadText(TEXT("Angelscript.uplugin"), Contents))
		{
			return nullptr;
		}
		TSharedPtr<FJsonObject> Descriptor;
		const TSharedRef<TJsonReader<>> Reader =
			TJsonReaderFactory<>::Create(Contents);
		if (!FJsonSerializer::Deserialize(Reader, Descriptor))
		{
			return nullptr;
		}
		return Descriptor;
	}

	static FString GetGeneratedProfileRoot()
	{
		return FPaths::Combine(
			GetPluginRoot(),
			TEXT("Source/AngelscriptTestJIT/Generated/EditorDevelopment"));
	}

	static bool ContainsAddressShapedLiteral(const FString& Contents)
	{
		// Generated Provider code may contain small protocol/flag constants, but
		// it must never persist a process pointer or an old archive reference.
		FRegexMatcher Matcher(
			FRegexPattern(TEXT("0x[0-9a-fA-F]{10,}")), Contents);
		return Matcher.FindNext();
	}

	static int32 FindModule(
		const TSharedPtr<FJsonObject>& Descriptor,
		const FStringView ModuleName,
		FString& OutType,
		FString& OutLoadingPhase)
	{
		if (!Descriptor.IsValid())
		{
			return INDEX_NONE;
		}
		const TArray<TSharedPtr<FJsonValue>>* Modules = nullptr;
		if (!Descriptor->TryGetArrayField(TEXT("Modules"), Modules)
			|| Modules == nullptr)
		{
			return INDEX_NONE;
		}
		for (int32 Index = 0; Index < Modules->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject> Module = (*Modules)[Index]->AsObject();
			FString Name;
			if (Module.IsValid()
				&& Module->TryGetStringField(TEXT("Name"), Name)
				&& Name == ModuleName)
			{
				Module->TryGetStringField(TEXT("Type"), OutType);
				Module->TryGetStringField(
					TEXT("LoadingPhase"), OutLoadingPhase);
				return Index;
			}
		}
		return INDEX_NONE;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptJITTestModuleOwnershipTests,
	"Angelscript.TestModule.StaticJIT.TestModuleOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(DescriptorAndBuildRulesKeepTheCarrierEditorOnlyAndOneWay)
	{
		using namespace AngelscriptJITTestModuleOwnershipTests_Private;
		const TSharedPtr<FJsonObject> Descriptor = LoadPluginDescriptor();
		ASSERT_THAT(IsTrue(Descriptor.IsValid()));

		FString TestJITType;
		FString TestJITPhase;
		FString TestType;
		FString TestPhase;
		const int32 TestJITIndex = FindModule(
			Descriptor, TEXT("AngelscriptTestJIT"), TestJITType, TestJITPhase);
		const int32 TestIndex = FindModule(
			Descriptor, TEXT("AngelscriptTest"), TestType, TestPhase);
		ASSERT_THAT(IsTrue(TestJITIndex != INDEX_NONE));
		ASSERT_THAT(AreEqual(FString(TEXT("Editor")), TestJITType));
		ASSERT_THAT(AreEqual(FString(TEXT("PostDefault")), TestJITPhase));
		ASSERT_THAT(IsTrue(TestJITIndex < TestIndex));

		FString TestJITBuildRules;
		ASSERT_THAT(IsTrue(LoadText(
			TEXT("Source/AngelscriptTestJIT/AngelscriptTestJIT.Build.cs"),
			TestJITBuildRules)));
		ASSERT_THAT(IsTrue(TestJITBuildRules.Contains(
			TEXT("\"AngelscriptRuntime\""))));
		ASSERT_THAT(IsFalse(TestJITBuildRules.Contains(
			TEXT("\"AngelscriptTest\""))));
		ASSERT_THAT(IsFalse(TestJITBuildRules.Contains(
			TEXT("\"AngelscriptEditor\""))));

		FString TestBuildRules;
		ASSERT_THAT(IsTrue(LoadText(
			TEXT("Source/AngelscriptTest/AngelscriptTest.Build.cs"),
			TestBuildRules)));
		ASSERT_THAT(IsTrue(TestBuildRules.Contains(
			TEXT("PrivateDependencyModuleNames"))));
		ASSERT_THAT(IsTrue(TestBuildRules.Contains(
			TEXT("\"AngelscriptTestJIT\""))));
		ASSERT_THAT(IsTrue(TestBuildRules.Contains(
			TEXT("PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(")),
			TEXT("AngelscriptTest must consume the root probe header through a private include path")));
		ASSERT_THAT(IsTrue(TestBuildRules.Contains(
			TEXT("ModuleDirectory, \"..\", \"AngelscriptTestJIT\"")),
			TEXT("The private include path must resolve to the AngelscriptTestJIT module root")));

		FString RuntimeBuildRules;
		ASSERT_THAT(IsTrue(LoadText(
			TEXT("Source/AngelscriptRuntime/AngelscriptRuntime.Build.cs"),
			RuntimeBuildRules)));
		ASSERT_THAT(IsFalse(RuntimeBuildRules.Contains(
			TEXT("AngelscriptTestJIT"))));
	}

	TEST_METHOD(CarrierContainsOnlyProviderCodeAndPluginOwnedProbes)
	{
		using namespace AngelscriptJITTestModuleOwnershipTests_Private;
		const FString SourceRoot = FPaths::Combine(
			GetPluginRoot(), TEXT("Source/AngelscriptTestJIT"));
		ASSERT_THAT(IsTrue(IFileManager::Get().DirectoryExists(*SourceRoot)));
		ASSERT_THAT(IsFalse(IFileManager::Get().DirectoryExists(
			*FPaths::Combine(SourceRoot, TEXT("Private")))));
		ASSERT_THAT(IsFalse(IFileManager::Get().DirectoryExists(
			*FPaths::Combine(SourceRoot, TEXT("Public")))));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
			*FPaths::Combine(SourceRoot, TEXT("AngelscriptTestJITModule.cpp")))));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
			*FPaths::Combine(SourceRoot, TEXT("AngelscriptTestJITProbes.cpp")))));
		FString ProbeHeader;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			ProbeHeader,
			*FPaths::Combine(SourceRoot, TEXT("AngelscriptTestJITProbes.h")))));
		ASSERT_THAT(IsTrue(ProbeHeader.Contains(
			TEXT("ANGELSCRIPTTESTJIT_API")),
			TEXT("The root-level probe declaration must retain DLL import/export")));

		TArray<FString> SourceFiles;
		IFileManager::Get().FindFilesRecursive(
			SourceFiles, *SourceRoot, TEXT("*.cpp"), true, false);
		ASSERT_THAT(IsTrue(SourceFiles.Num() > 0));
		for (const FString& SourceFile : SourceFiles)
		{
			FString Contents;
			ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
				Contents, *SourceFile)));
			ASSERT_THAT(IsFalse(Contents.Contains(TEXT("TEST_METHOD("))));
			ASSERT_THAT(IsFalse(Contents.Contains(TEXT("TEST_CLASS"))));
			ASSERT_THAT(IsFalse(Contents.Contains(TEXT("IMPLEMENT_"))
				&& Contents.Contains(TEXT("AUTOMATION_TEST"))));
			ASSERT_THAT(IsFalse(Contents.Contains(TEXT("CQTest"))));
			ASSERT_THAT(IsFalse(Contents.Contains(TEXT("ProjectScriptRoot"))));
			ASSERT_THAT(IsFalse(Contents.Contains(TEXT("Source/AngelscriptJIT"))));
			ASSERT_THAT(IsFalse(Contents.Contains(TEXT(".uproject"))));
			ASSERT_THAT(IsFalse(Contents.Contains(TEXT("Scaffold"))));
		}
	}

	TEST_METHOD(CarrierRebuildsTheGeneratedProviderViewForEveryObservation)
	{
		using namespace AngelscriptJITTestModuleOwnershipTests_Private;
		FString ModuleSource;
		ASSERT_THAT(IsTrue(LoadText(
			TEXT("Source/AngelscriptTestJIT/AngelscriptTestJITModule.cpp"),
			ModuleSource)));
		ASSERT_THAT(IsTrue(ModuleSource.Contains(
			TEXT("static thread_local FAngelscriptJITProviderView View")),
			TEXT("The fixed TestJIT carrier must own pointer-stable per-thread view storage")));
		ASSERT_THAT(IsTrue(ModuleSource.Contains(
			TEXT("View = GetGeneratedAngelscriptJITProviderView();")),
			TEXT("Every observation must repopulate the view from the latest Live Coding patch")));
		ASSERT_THAT(IsTrue(ModuleSource.Contains(TEXT("return &View;"))));
		ASSERT_THAT(IsFalse(ModuleSource.Contains(
			TEXT("return &GetGeneratedAngelscriptJITProviderView();")),
			TEXT("The generated accessor returns by value and must never be addressed directly")));
	}

	TEST_METHOD(FixedProviderIdentityIsStableAndNonZero)
	{
		const FAngelscriptJITProviderId& First =
			AngelscriptTestJITProbes::GetStableProviderId();
		const FAngelscriptJITProviderId& Second =
			AngelscriptTestJITProbes::GetStableProviderId();
		ASSERT_THAT(IsTrue(First.IsValid()));
		ASSERT_THAT(IsTrue(First == Second));
		ASSERT_THAT(AreEqual(
			FString(TEXT("7dc70fd55a76cc55d726e32d5c2434713bd93755a32e4dd524a4b7c39c8b5d44")),
			First.Hash.ToHexString()));
	}

	TEST_METHOD(IndependentCommandOwnsOnlyCommittedTestJITFixturePaths)
	{
		using namespace AngelscriptJITTestModuleOwnershipTests_Private;
		FString CommandletSource;
		FString GenerationSource;
		ASSERT_THAT(IsTrue(LoadText(
			TEXT("Source/AngelscriptTest/StaticJIT/AOT/AngelscriptStaticJITAotTestCommandlet.cpp"),
			CommandletSource)));
		ASSERT_THAT(IsTrue(LoadText(
			TEXT("Source/AngelscriptTest/StaticJIT/AOT/AngelscriptStaticJITAotGeneration.cpp"),
			GenerationSource)));
		const FString Combined = CommandletSource + GenerationSource;
		ASSERT_THAT(IsTrue(Combined.Contains(TEXT("AngelscriptTestJIT"))));
		ASSERT_THAT(IsTrue(GenerationSource.Contains(
			TEXT("GetTestJITModuleRoot()"))));
		ASSERT_THAT(IsTrue(GenerationSource.Contains(
			TEXT("Generated/EditorDevelopment"))));
		ASSERT_THAT(IsTrue(GenerationSource.Contains(
			TEXT("GetLegacyGeneratedProfileRoots")),
			TEXT("The fixed command must retain an explicit safe migration path for legacy generated roots")));
		ASSERT_THAT(IsFalse(Combined.Contains(
			TEXT("FAngelscriptJITProjectGeneration"))));
		ASSERT_THAT(IsFalse(Combined.Contains(
			TEXT("FAngelscriptJITProjectScaffold"))));
		ASSERT_THAT(IsFalse(Combined.Contains(TEXT("Source/AngelscriptJIT"))));
		ASSERT_THAT(IsFalse(Combined.Contains(TEXT(".uproject"))));
	}

	TEST_METHOD(GeneratedFixtureUsesOneCppPerNonEmptyAngelScriptModule)
	{
		using namespace AngelscriptJITTestModuleOwnershipTests_Private;
		const FString Root = GetGeneratedProfileRoot();
		TArray<FString> ModuleSources;
		IFileManager::Get().FindFilesRecursive(
			ModuleSources,
			*Root,
			TEXT("*.jit.cpp"),
			true, false);
		ASSERT_THAT(AreEqual(7, ModuleSources.Num()));
		ASSERT_THAT(IsFalse(IFileManager::Get().DirectoryExists(
			*FPaths::Combine(Root, TEXT("Modules")))));

		TArray<FString> LegacyHeaders;
		TArray<FString> LegacyBuckets;
		IFileManager::Get().FindFilesRecursive(
			LegacyHeaders, *Root, TEXT("*.jit.hpp"), true, false);
		IFileManager::Get().FindFilesRecursive(
			LegacyBuckets, *Root, TEXT("AngelscriptJitCode_*.jit.cpp"),
			true, false);
		ASSERT_THAT(AreEqual(0, LegacyHeaders.Num()));
		ASSERT_THAT(AreEqual(0, LegacyBuckets.Num()));
		ASSERT_THAT(IsFalse(IFileManager::Get().FileExists(
			*FPaths::Combine(Root, TEXT("StaticJITAotFixture.Cache")))));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
			*FPaths::Combine(Root, TEXT("Provider.generated.cpp")))));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
			*FPaths::Combine(
				Root, TEXT("TypedASTJITScalarProbe.generated.cpp")))));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
			*FPaths::Combine(
				Root, TEXT("TypedASTJITProviderProbe.generated.cpp")))));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
			*FPaths::Combine(Root, TEXT("OwnedFiles.generated.json")))));
	}

	TEST_METHOD(GeneratedFixtureUsesOnlyEngineLocalStableReferenceSlots)
	{
		using namespace AngelscriptJITTestModuleOwnershipTests_Private;
		const FString Root = GetGeneratedProfileRoot();
		TArray<FString> GeneratedSources;
		IFileManager::Get().FindFilesRecursive(
			GeneratedSources, *Root, TEXT("*.cpp"), true, false);
		ASSERT_THAT(AreEqual(31, GeneratedSources.Num()));

		bool bUsesSystemFunctionSlot = false;
		bool bUsesTypeSlot = false;
		bool bUsesGlobalStorageSlot = false;
		for (const FString& Source : GeneratedSources)
		{
			FString Contents;
			ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
				Contents, *Source)));
			ASSERT_THAT(IsFalse(
				Contents.Contains(TEXT("FJitRef_")), Source));
			ASSERT_THAT(IsFalse(
				Contents.Contains(TEXT("FJitVerify")), Source));
			ASSERT_THAT(IsFalse(
				ContainsAddressShapedLiteral(Contents), Source));
			bUsesSystemFunctionSlot |= Contents.Contains(
				TEXT("FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer"));
			bUsesTypeSlot |= Contents.Contains(
				TEXT("FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo"));
			bUsesGlobalStorageSlot |= Contents.Contains(
				TEXT("FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage"));
		}

		ASSERT_THAT(IsTrue(bUsesSystemFunctionSlot));
		ASSERT_THAT(IsTrue(bUsesTypeSlot));
		ASSERT_THAT(IsTrue(bUsesGlobalStorageSlot));
	}

	TEST_METHOD(LegacyPairedCacheWorkflowIsAbsent)
	{
		using namespace AngelscriptJITTestModuleOwnershipTests_Private;
		const FString LegacyRoot = FPaths::Combine(
			GetPluginRoot(),
			TEXT("Source/AngelscriptTest/StaticJIT/AOT/Generated"));
		TArray<FString> LegacyArtifacts;
		IFileManager::Get().FindFilesRecursive(
			LegacyArtifacts, *LegacyRoot, TEXT("*"), true, false);
		ASSERT_THAT(AreEqual(0, LegacyArtifacts.Num()));

		FString FixtureHeader;
		FString FixtureSource;
		ASSERT_THAT(IsTrue(LoadText(
			TEXT("Source/AngelscriptTest/StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"),
			FixtureHeader)));
		ASSERT_THAT(IsTrue(LoadText(
			TEXT("Source/AngelscriptTest/StaticJIT/AOT/AngelscriptStaticJITAotFixture.cpp"),
			FixtureSource)));
		const FString FixtureText = FixtureHeader + FixtureSource;
		ASSERT_THAT(IsFalse(FixtureText.Contains(
			TEXT("GetPrecompiledCacheFilename"))));
		ASSERT_THAT(IsFalse(FixtureText.Contains(
			TEXT("StaticJITAotFixture.Cache"))));
		ASSERT_THAT(IsFalse(FixtureText.Contains(
			TEXT("IsGeneratedOutputAvailable"))));

		FString Runner;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			Runner,
			*FPaths::Combine(
				FPaths::ProjectDir(), TEXT("Tools/RunStaticJITTests.ps1")))));
		ASSERT_THAT(IsTrue(Runner.Contains(TEXT("'-Commandlet'"))));
		ASSERT_THAT(IsTrue(Runner.Contains(TEXT("'AngelscriptTestJIT'"))));
		ASSERT_THAT(IsTrue(Runner.Contains(TEXT("-Mode=Generate"))));
		ASSERT_THAT(IsTrue(Runner.Contains(TEXT("-Mode=Verify"))));
		ASSERT_THAT(IsFalse(Runner.Contains(
			TEXT("AngelscriptStaticJITAotTest"))));
		ASSERT_THAT(IsFalse(Runner.Contains(
			TEXT("StaticJITAotFixture.Cache"))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
