#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"

#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingPlatformFixture
{
static const FName PlatformProviderNames[] = {
	TEXT("CoreGlobals"), TEXT("FApp"), TEXT("FCommandLine"), TEXT("FFileHelper.Types"),
	TEXT("FFileHelper.Functions"), TEXT("FGenericPlatformMisc"), TEXT("FParse"), TEXT("FPaths"),
	TEXT("FPlatformApplicationMisc"), TEXT("FPlatformMisc"), TEXT("FPlatformProcess")
};

struct FPlatformFixture
{
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TUniquePtr<FAngelscriptEngine> Owner;
	FString Diagnostic;

	bool Create(bool bInstall = true)
	{
		auto Fail = [this](const TCHAR* Stage) { Diagnostic = FString(Stage) + TEXT(": ") + Diagnostic; UE_LOG(LogTemp, Error, TEXT("Platform fixture %s"), *Diagnostic); return false; };
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return Fail(TEXT("Providers"));
		const auto& Collection = FAngelscriptBind::GetRegisteredCollection();
		if (!FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(Collection, *Store, Diagnostic)) return Fail(TEXT("Declarations"));
		TArray<FName> SurfaceProviders;
		for (const FName Name : PlatformProviderNames)
		{
			const bool bRecorded = Store->GetProviders().ContainsByPredicate([Name](const FAngelscriptRecordedProvider& Provider) { return Provider.Source.Provider == Name; });
			if (!bRecorded) SurfaceProviders.Add(Name);
		}
		if (!FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(Collection, SurfaceProviders, *Store, Diagnostic)) return Fail(TEXT("Surfaces"));
		if (!Store->Seal(Diagnostic)) return Fail(TEXT("Seal"));
		if (!bInstall) return true;
		FAngelscriptBindingDiagnostic BindingDiagnostic; Owner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic);
		if (!Owner) { Diagnostic = BindingDiagnostic.Message; return Fail(TEXT("Create")); }
		return true;
	}

	asCScriptFunction* Global(const TCHAR* Namespace, const TCHAR* Declaration) const
	{
		for (const auto& Installed : Owner->GetBindingInstallation()->GetMembers())
		{
			const auto* Record = Store->GetMember(Installed.Record); const auto* Type = Store->GetType(Installed.Record.Type);
			if (Installed.Function && Record && Type && Type->Declaration.Name.IsEmpty()
				&& Type->Declaration.Namespace == Namespace && Record->Declaration == Declaration) return Installed.Function;
		}
		return nullptr;
	}
};

struct FScopedPlatformTempFile
{
	FString Path = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("RuntimeBindingPlatform_"), TEXT(".txt"));
	~FScopedPlatformTempFile() { IFileManager::Get().Delete(*Path, false, true, true); }
};
}

using namespace RuntimeBindingPlatformFixture;
TEST_CLASS_WITH_FLAGS(Platform, "Angelscript.UnitTest.RuntimeBindings.Runtime.Platform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ParseFindsKnownInteger)
	{
		FPlatformFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); auto* Parse = F.Global(TEXT("FParse"), TEXT("bool Value(const FString& Stream, const FString& Match, int& Value)")); ASSERT_THAT(IsNotNull(Parse));
		FString Stream(TEXT("Name=fixture Count=7")); FString Match(TEXT("Count=")); int32 Value = 0; asIScriptContext* C = F.Owner->Engine->CreateContext();
		ASSERT_THAT(AreEqual(0, C->Prepare(Parse))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Stream))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(1, &Match))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(2, &Value))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(AreEqual(asBYTE(1), C->GetReturnByte())); ASSERT_THAT(AreEqual(7, Value)); C->Release();
	}

	TEST_METHOD(PathsCombineAndNormalizeKnownComponents)
	{
		FPlatformFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); auto* Combine = F.Global(TEXT("FPaths"), TEXT("FString CombinePaths(const FString& FirstPath, const FString& SecondPath)")); ASSERT_THAT(IsNotNull(Combine));
		FString First(TEXT("alpha/beta")); FString Second(TEXT("../gamma/file.txt")); asIScriptContext* C = F.Owner->Engine->CreateContext(); ASSERT_THAT(AreEqual(0, C->Prepare(Combine))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &First))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(1, &Second))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		FString Result = *static_cast<const FString*>(C->GetReturnObject()); auto* Collapse = F.Global(TEXT("FPaths"), TEXT("bool CollapseRelativeDirectories(FString& InPath)")); ASSERT_THAT(IsNotNull(Collapse)); ASSERT_THAT(AreEqual(0, C->Prepare(Collapse))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Result))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(AreEqual(asBYTE(1), C->GetReturnByte())); ASSERT_THAT(IsTrue(Result.Replace(TEXT("\\"), TEXT("/")).EndsWith(TEXT("alpha/gamma/file.txt")))); C->Release();
	}

	TEST_METHOD(TemporaryUtfTextRoundTripsAndCleansUp)
	{
		FPlatformFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); FScopedPlatformTempFile Temp; auto* Save = F.Global(TEXT("FFileHelper"), TEXT("bool SaveStringToFile(const FString& String, const FString& Filename, FFileHelper::EEncodingOptions EncodingOptions = FFileHelper::EEncodingOptions::AutoDetect, uint32 WriteFlags = uint32(EFileWrite::None))")); auto* Load = F.Global(TEXT("FFileHelper"), TEXT("bool LoadFileToString(FString& Result, const FString& Filename, FFileHelper::EHashOptions HashOptions = FFileHelper::EHashOptions::None, uint32 ReadFlags = uint32(EFileRead::None))")); ASSERT_THAT(IsNotNull(Save)); ASSERT_THAT(IsNotNull(Load));
		FString Input(TEXT("fixture-\u96ea-7")); FString Output; asIScriptContext* C = F.Owner->Engine->CreateContext(); ASSERT_THAT(AreEqual(0, C->Prepare(Save))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Input))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(1, &Temp.Path))); ASSERT_THAT(AreEqual(0, C->SetArgDWord(2, uint32(FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)))); ASSERT_THAT(AreEqual(0, C->SetArgDWord(3, FILEWRITE_None))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(AreEqual(asBYTE(1), C->GetReturnByte()));
		ASSERT_THAT(AreEqual(0, C->Prepare(Load))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Output))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(1, &Temp.Path))); ASSERT_THAT(AreEqual(0, C->SetArgDWord(2, uint32(FFileHelper::EHashOptions::None)))); ASSERT_THAT(AreEqual(0, C->SetArgDWord(3, FILEREAD_None))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(AreEqual(asBYTE(1), C->GetReturnByte())); ASSERT_THAT(AreEqual(Input, Output)); C->Release();
	}

	TEST_METHOD(MissingFileReturnsFailureAndRetainsOutput)
	{
		FPlatformFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); auto* Load = F.Global(TEXT("FFileHelper"), TEXT("bool LoadFileToString(FString& Result, const FString& Filename, FFileHelper::EHashOptions HashOptions = FFileHelper::EHashOptions::None, uint32 ReadFlags = uint32(EFileRead::None))")); ASSERT_THAT(IsNotNull(Load)); FString Output(TEXT("sentinel")); FString Missing = FPaths::Combine(FPaths::ProjectSavedDir(), FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".missing")); asIScriptContext* C = F.Owner->Engine->CreateContext();
		ASSERT_THAT(AreEqual(0, C->Prepare(Load))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Output))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(1, &Missing))); ASSERT_THAT(AreEqual(0, C->SetArgDWord(2, uint32(FFileHelper::EHashOptions::None)))); ASSERT_THAT(AreEqual(0, C->SetArgDWord(3, FILEREAD_None))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(AreEqual(asBYTE(0), C->GetReturnByte())); ASSERT_THAT(IsTrue(Output.IsEmpty())); C->Release();
	}

	TEST_METHOD(CommandLineMatchesCapturedHostString)
	{
		FPlatformFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); auto* Get = F.Global(TEXT("FCommandLine"), TEXT("FString Get()")); ASSERT_THAT(IsNotNull(Get)); asIScriptContext* C = F.Owner->Engine->CreateContext(); ASSERT_THAT(AreEqual(0, C->Prepare(Get))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(AreEqual(FString(FCommandLine::Get()), *static_cast<const FString*>(C->GetReturnObject()))); C->Release();
	}

	TEST_METHOD(ProcessLaunchMetadataAndEveryProviderAreAccounted)
	{
		FPlatformFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); ASSERT_THAT(IsNotNull(F.Global(TEXT("FPlatformProcess"), TEXT("void LaunchURL(const FString& URL, const FString& Params = FString())"))));
		for (const FName Name : PlatformProviderNames) ASSERT_THAT(IsTrue(F.Store->GetProviders().ContainsByPredicate([Name](const FAngelscriptRecordedProvider& Provider) { return Provider.Source.Provider == Name; })));
		int32 Contributions = 0, Recipes = 0; for (const auto& Type : F.Store->GetTypes()) for (const auto& Member : Type.Members) for (const FName Name : PlatformProviderNames) if (Member.Source.Provider == Name) { ++Contributions; if (!Member.NativeRecipes.IsEmpty()) ++Recipes; break; }
		ASSERT_THAT(AreEqual(74, Contributions)); ASSERT_THAT(AreEqual(7, Recipes));
	}
};
#endif
