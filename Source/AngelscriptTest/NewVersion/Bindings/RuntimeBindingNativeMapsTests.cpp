#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingNativeMapsFixture
{
using namespace RuntimeBindingTestSupport;

static void SumThunk(UObject*, FAngelscriptNativeModuleFunctionBindingCallFrame* Frame)
{
	const int32 Left = *static_cast<const int32*>(Frame->ArgSlots[0]);
	const int32 Right = *static_cast<const int32*>(Frame->ArgSlots[1]);
	*static_cast<int32*>(Frame->ReturnSlot) = Left + Right;
}

static void GeneratedThunk(UObject*, FAngelscriptNativeModuleFunctionBindingCallFrame* Frame)
{
	*static_cast<int32*>(Frame->ReturnSlot) = 99;
}

static FAngelscriptNativeFunctionMapRecord Record(
	void (*Thunk)(UObject*, FAngelscriptNativeModuleFunctionBindingCallFrame*) = &SumThunk,
	EAngelscriptFunctionBindingOrigin Origin = EAngelscriptFunctionBindingOrigin::Generated,
	bool bPlaceholder = false)
{
	FAngelscriptNativeFunctionMapRecord Result;
	Result.ModuleName = TEXT("FixtureModule"); Result.OwnerType = TEXT("FixtureNativeMap");
	Result.FunctionName = TEXT("Add"); Result.Signature = TEXT("int Add(int Left, int Right)");
	Result.Thunk = Thunk; Result.ArgCount = 2; Result.ReturnSize = sizeof(int32);
	Result.Flags = FAngelscriptNativeModuleFunctionBindingBridge::FlagStatic;
	Result.Origin = Origin; Result.bPlaceholder = bPlaceholder;
	Result.Source = {TEXT("FixtureModule"), Origin == EAngelscriptFunctionBindingOrigin::Manual ? TEXT("ManualMap") : TEXT("GeneratedMap"), TEXT("Fixture/NativeMaps.cpp"), 42, EAngelscriptBindPhase::GeneratedBindings};
	return Result;
}

static TUniquePtr<FAngelscriptEngine> CreateOwner(TSharedRef<FAngelscriptTypeBindInfoStore> Store, FString& Diagnostic)
{
	if (!Store->Seal(Diagnostic)) return nullptr;
	FAngelscriptBindingDiagnostic BindingDiagnostic;
	auto Owner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic);
	if (!Owner) Diagnostic = BindingDiagnostic.Message;
	return Owner;
}

static bool InvokeAdd(const FAngelscriptTypeBindInfoInstallation& Installation, int32 Left, int32 Right,
	int32& Result, FString& Diagnostic)
{
	void* Arguments[] = {&Left, &Right};
	return Installation.InvokeNativeFunctionMap(TEXT("FixtureNativeMap"), TEXT("Add"), nullptr,
		Arguments, &Result, Diagnostic);
}

}

using namespace RuntimeBindingNativeMapsFixture;
TEST_CLASS_WITH_FLAGS(NativeMaps, "Angelscript.UnitTest.RuntimeBindings.Reflection.NativeMaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ManualBindingWinsEquivalentGeneratedContribution)
	{
		auto Store = RecordFixture(false, false); FString Diagnostic;
		ASSERT_THAT(IsTrue(Store->RecordNativeFunctionMap(Record(&GeneratedThunk), Diagnostic)));
		ASSERT_THAT(IsTrue(Store->RecordNativeFunctionMap(Record(&SumThunk, EAngelscriptFunctionBindingOrigin::Manual), Diagnostic)));
		ASSERT_THAT(AreEqual(1, Store->GetNativeFunctionMaps().Num()));
		ASSERT_THAT(IsTrue(Store->GetNativeFunctionMaps()[0].Origin == EAngelscriptFunctionBindingOrigin::Manual));
	}

	TEST_METHOD(PlaceholderIsCompletedByRealNativeAddress)
	{
		auto Store = RecordFixture(false, false); FString Diagnostic;
		ASSERT_THAT(IsTrue(Store->RecordNativeFunctionMap(Record(nullptr, EAngelscriptFunctionBindingOrigin::Generated, true), Diagnostic)));
		ASSERT_THAT(IsTrue(Store->RecordNativeFunctionMap(Record(), Diagnostic)));
		ASSERT_THAT(AreEqual(1, Store->GetNativeFunctionMaps().Num()));
		ASSERT_THAT(IsTrue(Store->GetNativeFunctionMaps()[0].Thunk == &SumThunk));
	}

	TEST_METHOD(ConfiguredTransportInvokesAddTwoAndThree)
	{
		const FAngelscriptNativeModuleFunctionBinding Binding = {TEXT("FixtureNativeMap"), TEXT("Add"), &SumThunk, 2, sizeof(int32), FAngelscriptNativeModuleFunctionBindingBridge::FlagStatic};
		const FAngelscriptNativeModuleFunctionBindingView View = {&Binding, 1, TEXT("FixtureModule"), FAngelscriptNativeModuleFunctionBindingBridge::LayoutVersionExpected};
		auto Store = RecordFixture(false, false); FString Diagnostic;
		const FAngelscriptTypeBindSource Source = {TEXT("FixtureModule"), TEXT("NativeModuleFunctionBinding.GeneratedTransport"), TEXT("Fixture/GeneratedTransport.cpp"), 42, EAngelscriptBindPhase::GeneratedBindings};
		ASSERT_THAT(IsTrue(Store->RecordNativeFunctionMapView(View, Source, Diagnostic), *Diagnostic));
		auto Owner = CreateOwner(Store, Diagnostic); ASSERT_THAT(IsTrue(Owner.IsValid(), *Diagnostic)); int32 Result = 0;
		ASSERT_THAT(IsTrue(InvokeAdd(*Owner->GetBindingInstallation(), 2, 3, Result, Diagnostic), *Diagnostic)); ASSERT_THAT(AreEqual(5, Result));
	}

	TEST_METHOD(EquivalentExplicitAlternateTableInvokesAdd)
	{
		auto Store = RecordFixture(false, false); FString Diagnostic; ASSERT_THAT(IsTrue(Store->RecordNativeFunctionMap(Record(), Diagnostic)));
		auto Owner = CreateOwner(Store, Diagnostic); ASSERT_THAT(IsTrue(Owner.IsValid(), *Diagnostic)); const auto Alternate = Record(&SumThunk, EAngelscriptFunctionBindingOrigin::NativeModule);
		ASSERT_THAT(IsTrue(Owner->GetBindingInstallation()->InstallAlternateNativeFunctionMap(MakeArrayView(&Alternate, 1), Diagnostic), *Diagnostic)); int32 Result = 0;
		ASSERT_THAT(IsTrue(InvokeAdd(*Owner->GetBindingInstallation(), 2, 3, Result, Diagnostic), *Diagnostic)); ASSERT_THAT(AreEqual(5, Result));
	}

	TEST_METHOD(WrongModuleVersionAndSignatureAreRejectedWithProvenance)
	{
		auto Store = RecordFixture(false, false); FString Diagnostic; ASSERT_THAT(IsTrue(Store->RecordNativeFunctionMap(Record(), Diagnostic)));
		auto Owner = CreateOwner(Store, Diagnostic); ASSERT_THAT(IsTrue(Owner.IsValid(), *Diagnostic));
		auto WrongModule = Record(); WrongModule.ModuleName = TEXT("OtherModule"); ASSERT_THAT(IsFalse(Owner->GetBindingInstallation()->InstallAlternateNativeFunctionMap(MakeArrayView(&WrongModule, 1), Diagnostic))); ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("module")) && Diagnostic.Contains(TEXT("GeneratedMap"))));
		auto WrongVersion = Record(); WrongVersion.LayoutVersion += 1; ASSERT_THAT(IsFalse(Owner->GetBindingInstallation()->InstallAlternateNativeFunctionMap(MakeArrayView(&WrongVersion, 1), Diagnostic))); ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("version"))));
		auto WrongSignature = Record(); WrongSignature.Signature = TEXT("int Add(int Left)"); ASSERT_THAT(IsFalse(Owner->GetBindingInstallation()->InstallAlternateNativeFunctionMap(MakeArrayView(&WrongSignature, 1), Diagnostic))); ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("signature"))));
	}

	TEST_METHOD(MissingSymbolFailsWithSourceProvenance)
	{
		auto Store = RecordFixture(false, false); FString Diagnostic; ASSERT_THAT(IsTrue(Store->RecordNativeFunctionMap(Record(), Diagnostic)));
		auto Owner = CreateOwner(Store, Diagnostic); ASSERT_THAT(IsTrue(Owner.IsValid(), *Diagnostic)); int32 Value = 0; void* Arguments[] = {&Value, &Value};
		ASSERT_THAT(IsFalse(Owner->GetBindingInstallation()->InvokeNativeFunctionMap(TEXT("FixtureNativeMap"), TEXT("Missing"), nullptr, Arguments, &Value, Diagnostic)));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("FixtureNativeMap")) && Diagnostic.Contains(TEXT("Missing"))));
	}

	TEST_METHOD(DestroyingOneOwnerLeavesOtherNativeMapUsable)
	{
		auto Store = RecordFixture(false, false); FString Diagnostic; ASSERT_THAT(IsTrue(Store->RecordNativeFunctionMap(Record(), Diagnostic))); ASSERT_THAT(IsTrue(Store->Seal(Diagnostic)));
		FAngelscriptBindingDiagnostic BindingDiagnostic; auto A = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic); auto B = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic); ASSERT_THAT(IsTrue(A.IsValid() && B.IsValid(), *BindingDiagnostic.Message)); A.Reset(); int32 Result = 0;
		ASSERT_THAT(IsTrue(InvokeAdd(*B->GetBindingInstallation(), 2, 3, Result, Diagnostic), *Diagnostic)); ASSERT_THAT(AreEqual(5, Result));
	}
};
#endif
