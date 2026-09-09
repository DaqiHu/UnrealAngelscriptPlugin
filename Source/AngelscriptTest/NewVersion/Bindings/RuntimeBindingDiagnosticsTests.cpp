#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Binds/Bind_Console.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"
#include "Core/AngelscriptTypeBindInfoInspection.h"
#include "Core/FCpuProfilerTraceScoped.h"

#include "HAL/IConsoleManager.h"
#include "Misc/OutputDevice.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingDiagnosticsFixture
{
static const FName DiagnosticProviderNames[] = {
	TEXT("Console.Types"), TEXT("Console.Variables"), TEXT("Console.Commands"),
	TEXT("Debugging.Manual"), TEXT("FCpuProfilerTraceScoped"), TEXT("FMessageDialog"),
	TEXT("Logging.Functions"), TEXT("Stats.Types"), TEXT("Stats.Functions")
};

struct FDiagnosticsFixture
{
	TSharedRef<FAngelscriptTypeBindInfoStore> Store;
	TUniquePtr<FAngelscriptEngine> Owner;
	FString Diagnostic;

	explicit FDiagnosticsFixture(const FAngelscriptBindRecordingPolicy& Policy = {})
		: Store(MakeShared<FAngelscriptTypeBindInfoStore>(Policy)) {}

	bool Create(bool bInstall = true)
	{
		auto Fail = [this](const TCHAR* Stage) { Diagnostic = FString(Stage) + TEXT(": ") + Diagnostic; UE_LOG(LogTemp, Error, TEXT("Diagnostics fixture %s"), *Diagnostic); return false; };
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return Fail(TEXT("Providers"));
		const auto& Collection = FAngelscriptBind::GetRegisteredCollection();
		if (!FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(Collection, *Store, Diagnostic)) return Fail(TEXT("Declarations"));
		TArray<FName> Surfaces;
		for (const FName Name : DiagnosticProviderNames)
			if (!Store->GetProviders().ContainsByPredicate([Name](const auto& Provider) { return Provider.Source.Provider == Name; })) Surfaces.Add(Name);
		if (!FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(Collection, Surfaces, *Store, Diagnostic)) return Fail(TEXT("Surfaces"));
		if (!Store->Seal(Diagnostic)) return Fail(TEXT("Seal"));
		if (!bInstall) return true;
		FAngelscriptBindingDiagnostic BindingDiagnostic; Owner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic);
		if (!Owner) { Diagnostic = BindingDiagnostic.Message; return Fail(TEXT("Create")); }
		return true;
	}

	asCScriptFunction* Function(const TCHAR* OwnerName, const TCHAR* Namespace, const TCHAR* Declaration) const
	{
		for (const auto& Installed : Owner->GetBindingInstallation()->GetMembers())
		{
			const auto* Record = Store->GetMember(Installed.Record); const auto* Type = Store->GetType(Installed.Record.Type);
			if (Installed.Function && Record && Type && Type->Declaration.Name == OwnerName
				&& Type->Declaration.Namespace == Namespace && Record->Declaration == Declaration) return Installed.Function;
		}
		return nullptr;
	}

	const FAngelscriptTypeBindInfoMember* Record(const TCHAR* OwnerName, const TCHAR* Namespace, const TCHAR* Declaration) const
	{
		for (const auto& Type : Store->GetTypes())
			if (Type.Declaration.Name == OwnerName && Type.Declaration.Namespace == Namespace)
				if (const auto* Member = Type.Members.FindByPredicate([Declaration](const auto& M) { return M.Declaration == Declaration; })) return Member;
		return nullptr;
	}
};

struct FCapturedOutput final : FOutputDevice
{
	FString Text;
	virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type, const FName&) override { Text += Message; }
};
}

using namespace RuntimeBindingDiagnosticsFixture;
TEST_CLASS_WITH_FLAGS(Diagnostics, "Angelscript.UnitTest.RuntimeBindings.Runtime.Diagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ScopedConsoleIntegerReadsWritesSevenThenIsRemoved)
	{
		FDiagnosticsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic));
		const FString Name = FString::Printf(TEXT("RuntimeBinding.Diagnostics.%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		IConsoleVariable* FixtureVariable = IConsoleManager::Get().RegisterConsoleVariable(*Name, 1, TEXT("runtime binding diagnostic fixture")); ASSERT_THAT(IsNotNull(FixtureVariable));
		FString Help(TEXT("runtime binding diagnostic fixture")); alignas(FScriptConsoleVariable<int32>) uint8 Storage[sizeof(FScriptConsoleVariable<int32>)] = {};
		auto* Construct = F.Function(TEXT("FConsoleVariable"), TEXT(""), TEXT("void f(const FString& Name, int DefaultValue, const FString& Help = \"\")"));
		auto* Set = F.Function(TEXT("FConsoleVariable"), TEXT(""), TEXT("void SetInt(int InValue) const"));
		auto* Get = F.Function(TEXT("FConsoleVariable"), TEXT(""), TEXT("int GetInt() const"));
		auto* Destruct = F.Function(TEXT("FConsoleVariable"), TEXT(""), TEXT("void f()"));
		ASSERT_THAT(IsNotNull(Construct)); ASSERT_THAT(IsNotNull(Set)); ASSERT_THAT(IsNotNull(Get)); ASSERT_THAT(IsNotNull(Destruct));
		asIScriptContext* C = F.Owner->Engine->CreateContext(); ASSERT_THAT(AreEqual(0, C->Prepare(Construct))); ASSERT_THAT(AreEqual(0, C->SetObject(Storage)));
		ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, const_cast<FString*>(&Name)))); ASSERT_THAT(AreEqual(0, C->SetArgDWord(1, 1))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(2, &Help))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		ASSERT_THAT(AreEqual(0, C->Prepare(Set))); ASSERT_THAT(AreEqual(0, C->SetObject(Storage))); ASSERT_THAT(AreEqual(0, C->SetArgDWord(0, 7))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		ASSERT_THAT(AreEqual(0, C->Prepare(Get))); ASSERT_THAT(AreEqual(0, C->SetObject(Storage))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(AreEqual(7, int32(C->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, C->Prepare(Destruct))); ASSERT_THAT(AreEqual(0, C->SetObject(Storage))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); C->Release();
		IConsoleObject* Registered = IConsoleManager::Get().FindConsoleObject(*Name); ASSERT_THAT(IsNotNull(Registered)); IConsoleManager::Get().UnregisterConsoleObject(Registered, false);
		ASSERT_THAT(IsNull(IConsoleManager::Get().FindConsoleObject(*Name)));
	}

	TEST_METHOD(FormattedLogReachesTemporaryOutputSink)
	{
		FDiagnosticsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); auto* Log = F.Function(TEXT(""), TEXT(""), TEXT("void Log(const FString& Text)")); ASSERT_THAT(IsNotNull(Log));
		FCapturedOutput Sink; GLog->AddOutputDevice(&Sink); ON_SCOPE_EXIT { GLog->RemoveOutputDevice(&Sink); };
		FString Message(TEXT("runtime-binding-diagnostic-7")); asIScriptContext* C = F.Owner->Engine->CreateContext(); ASSERT_THAT(AreEqual(0, C->Prepare(Log))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Message)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); C->Release(); GLog->FlushThreadedLogs(); ASSERT_THAT(IsTrue(Sink.Text.Contains(Message)));
	}

	TEST_METHOD(CompileOutPoliciesDistinguishDevelopmentAndShippingInputs)
	{
		FAngelscriptBindRecordingPolicy Development; Development.bLogging = true;
		FDiagnosticsFixture Dev(Development); ASSERT_THAT(IsTrue(Dev.Create(), *Dev.Diagnostic));
		auto* DevLog = Dev.Function(TEXT(""), TEXT(""), TEXT("void Log(const FString& Text)")); auto* DevEnsure = Dev.Function(TEXT(""), TEXT(""), TEXT("bool ensure(bool Condition)"));
		ASSERT_THAT(IsNotNull(DevLog)); ASSERT_THAT(IsNotNull(DevEnsure)); ASSERT_THAT(IsTrue(DevLog->compileOutType == asECompileOutType::CompileCalls)); ASSERT_THAT(IsTrue(DevEnsure->compileOutType == asECompileOutType::CompileCalls));
		FAngelscriptBindRecordingPolicy Shipping; Shipping.bShipping = true; Shipping.bLogging = false;
		FDiagnosticsFixture Ship(Shipping); ASSERT_THAT(IsTrue(Ship.Create(), *Ship.Diagnostic));
		auto* ShipLog = Ship.Function(TEXT(""), TEXT(""), TEXT("void Log(const FString& Text)")); auto* ShipEnsure = Ship.Function(TEXT(""), TEXT(""), TEXT("bool ensure(bool Condition)"));
		ASSERT_THAT(IsNotNull(ShipLog)); ASSERT_THAT(IsNotNull(ShipEnsure)); ASSERT_THAT(IsTrue(ShipLog->compileOutType == asECompileOutType::CompileOutEntirely)); ASSERT_THAT(IsTrue(ShipEnsure->compileOutType == asECompileOutType::ReplaceWithFirstParam));
	}

	TEST_METHOD(ProfilerScopeBalancesConstructionAndDestruction)
	{
		FDiagnosticsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); auto* Construct = F.Function(TEXT("FCpuProfilerTraceScoped"), TEXT(""), TEXT("void f(const FName& EventID)")); ASSERT_THAT(IsNotNull(Construct));
		alignas(FCpuProfilerTraceScoped) uint8 Storage[sizeof(FCpuProfilerTraceScoped)] = {}; FName Event(TEXT("RuntimeBindingDiagnostics")); asIScriptContext* C = F.Owner->Engine->CreateContext();
		ASSERT_THAT(AreEqual(0, C->Prepare(Construct))); ASSERT_THAT(AreEqual(0, C->SetObject(Storage))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Event))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); C->Release();
		reinterpret_cast<FCpuProfilerTraceScoped*>(Storage)->~FCpuProfilerTraceScoped();
	}

	TEST_METHOD(UnavailableDevelopmentOnlyConsoleSurfaceIsExcludedWithReason)
	{
		FAngelscriptBindRecordingPolicy Shipping; Shipping.bShipping = true; FDiagnosticsFixture F(Shipping); ASSERT_THAT(IsTrue(F.Create(false), *F.Diagnostic));
		const auto* Provider = F.Store->GetProviders().FindByPredicate([](const auto& Candidate) { return Candidate.Source.Provider == TEXT("Console.Commands"); });
		ASSERT_THAT(IsNotNull(Provider)); ASSERT_THAT(IsFalse(Provider->bEligible));
		FAngelscriptBindingInspection Inspection; ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoInspection::InspectStore(F.Store, EAngelscriptBindingInspectionScope::SelectedFamily, Inspection, F.Diagnostic), *F.Diagnostic));
		const auto* Row = Inspection.Providers.FindByPredicate([](const auto& Candidate) { return Candidate.Source.Provider == TEXT("Console.Commands"); });
		ASSERT_THAT(IsNotNull(Row)); ASSERT_THAT(AreEqual(FString(TEXT("excluded-by-condition")), Row->Disposition));
	}

	TEST_METHOD(DialogMetadataExistsWithoutInvocationAndProvidersAreAccounted)
	{
		FDiagnosticsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic));
		ASSERT_THAT(IsNotNull(F.Function(TEXT(""), TEXT("FMessageDialog"), TEXT("EAppReturnType Open(EAppMsgType MessageType, const FText& Message, FText OptionalTitle = FText())"))));
		for (const FName Name : DiagnosticProviderNames) ASSERT_THAT(IsTrue(F.Store->GetProviders().ContainsByPredicate([Name](const auto& Provider) { return Provider.Source.Provider == Name; })));
		int32 Contributions = 0, Recipes = 0; for (const auto& Type : F.Store->GetTypes()) for (const auto& Member : Type.Members) for (const FName Name : DiagnosticProviderNames) if (Member.Source.Provider == Name) { ++Contributions; if (!Member.NativeRecipes.IsEmpty()) ++Recipes; break; }
		ASSERT_THAT(AreEqual(62, Contributions)); ASSERT_THAT(AreEqual(1, Recipes));
	}
};
#endif
