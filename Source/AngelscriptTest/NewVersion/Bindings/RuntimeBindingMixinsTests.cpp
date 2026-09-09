#include "CQTest.h"
#include "RuntimeBindingMixinTestTypes.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptEngine.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"
#include "Core/AngelscriptTypeBindInfoReflection.h"
#include "Engine/World.h"

void URuntimeBindingStaticLibrary::SetSelected(URuntimeBindingMixinTarget* Selected, int32 Value)
{
	if (Selected) Selected->Value = Value;
}

bool URuntimeBindingStaticLibrary::ObserveWorld(UObject* WorldContextObject)
{
	return WorldContextObject && FAngelscriptEngine::GetAmbientWorldContext() == WorldContextObject;
}

URuntimeBindingMixinTarget* URuntimeBindingStaticLibrary::SelectType(UClass*)
{
	return nullptr;
}

void URuntimeBindingMixinLibrary::AddToSelected(URuntimeBindingMixinTarget* Target, int32 Delta)
{
	if (Target) Target->Value += Delta;
}

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingMixinsFixture
{
struct FMixinsFixture
{
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TSharedPtr<const FAngelscriptReflectedTypeSnapshot> Snapshot;
	TUniquePtr<FAngelscriptEngine> Owner;
	FString Diagnostic;

	bool Create()
	{
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return false;
		TArray<UObject*> Roots = { URuntimeBindingMixinTarget::StaticClass(), URuntimeBindingStaticLibrary::StaticClass(),
			URuntimeBindingMixinLibrary::StaticClass(), UObject::StaticClass(), UClass::StaticClass() };
		if (!FAngelscriptTypeBindInfoReflection::Capture(Roots, Store->GetPolicy(), Snapshot, Diagnostic)) return false;
		if (!FAngelscriptTypeBindInfoReflection::RecordDeclarations(*Store, Snapshot.ToSharedRef(), Diagnostic)) return false;
		if (!FAngelscriptTypeBindInfoReflection::RecordFunctions(*Store, Snapshot.ToSharedRef(), Diagnostic)) return false;
		if (!Store->Seal(Diagnostic)) return false;
		FAngelscriptBindingDiagnostic BindingDiagnostic;
		Owner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic);
		if (!Owner) Diagnostic = BindingDiagnostic.Message;
		return Owner.IsValid();
	}

	bool Invoke(UClass* Library, UObject* Receiver, UObject* WorldContext, const TCHAR* Name,
		TConstArrayView<void*> Arguments, TConstArrayView<EAngelscriptReflectedParameterDirection> Directions,
		void* ReturnAddress = nullptr, UClass** Determined = nullptr)
	{
		return Owner->GetBindingInstallation()->InvokeReflectedLibraryFunction(Library, Receiver, WorldContext,
			Name, Arguments, Directions, ReturnAddress, Determined, Diagnostic);
	}
	asCScriptFunction* Function(const TCHAR* OwnerName, const TCHAR* Name, bool bNamespace) const
	{
		for (const auto& Installed : Owner->GetBindingInstallation()->GetMembers())
		{
			const auto* Member = Store->GetMember(Installed.Record);
			const auto* Type = Store->GetType(Installed.Record.Type);
			if (!Installed.Function || !Member || !Type) continue;
			const bool bOwnerMatches = bNamespace
				? Type->Declaration.Kind == EAngelscriptTypeBindKind::Namespace && Type->Declaration.Namespace == OwnerName
				: Type->Declaration.Name == OwnerName;
			if (bOwnerMatches && Member->Declaration.Contains(FString::Printf(TEXT(" %s("), Name))) return Installed.Function;
		}
		return nullptr;
	}
};
}

using namespace RuntimeBindingMixinsFixture;
TEST_CLASS_WITH_FLAGS(Mixins, "Angelscript.UnitTest.RuntimeBindings.Reflection.Mixins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(StaticHelperWritesOnlySelectedArgument)
	{
		FMixinsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic));
		auto* Selected = NewObject<URuntimeBindingMixinTarget>(); auto* Unrelated = NewObject<URuntimeBindingMixinTarget>();
		Selected->Value = 1; Unrelated->Value = 9;
		auto* Function = F.Function(TEXT("URuntimeBindingStaticLibrary"), TEXT("SetSelected"), true); ASSERT_THAT(IsNotNull(Function));
		asIScriptContext* Context = F.Owner->Engine->CreateContext(); ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(0, Context->SetArgObject(0, Selected))); ASSERT_THAT(AreEqual(0, Context->SetArgDWord(1, 4)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), Context->Execute())); Context->Release();
		ASSERT_THAT(AreEqual(4, Selected->Value)); ASSERT_THAT(AreEqual(9, Unrelated->Value));
	}

	TEST_METHOD(ObjectFirstMixinMutatesOnlySelectedReceiver)
	{
		FMixinsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic));
		auto* Selected = NewObject<URuntimeBindingMixinTarget>(); auto* Other = NewObject<URuntimeBindingMixinTarget>();
		Selected->Value = 2; Other->Value = 8;
		auto* Function = F.Function(TEXT("URuntimeBindingMixinTarget"), TEXT("AddToSelected"), false); ASSERT_THAT(IsNotNull(Function));
		asIScriptContext* Context = F.Owner->Engine->CreateContext(); ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(0, Context->SetObject(Selected))); ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 3)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), Context->Execute())); Context->Release();
		ASSERT_THAT(AreEqual(5, Selected->Value)); ASSERT_THAT(AreEqual(8, Other->Value));
	}

	TEST_METHOD(MissingRequiredWorldContextIsDiagnosed)
	{
		FMixinsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); bool Result = false;
		ASSERT_THAT(IsFalse(F.Invoke(URuntimeBindingStaticLibrary::StaticClass(), nullptr, nullptr,
			TEXT("ObserveWorld"), {}, {}, &Result)));
		ASSERT_THAT(IsTrue(F.Diagnostic.Contains(TEXT("world context"))));
	}

	TEST_METHOD(SuppliedTransientWorldIsObservedAndPreviousContextRestored)
	{
		FMixinsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic));
		auto* Previous = NewObject<URuntimeBindingMixinTarget>(); auto* World = NewObject<UWorld>(GetTransientPackage());
		FAngelscriptEngine::AssignWorldContext(Previous); bool Result = false;
		ASSERT_THAT(IsTrue(F.Invoke(URuntimeBindingStaticLibrary::StaticClass(), nullptr, World,
			TEXT("ObserveWorld"), {}, {}, &Result), *F.Diagnostic));
		ASSERT_THAT(IsTrue(Result)); ASSERT_THAT(IsTrue(FAngelscriptEngine::GetAmbientWorldContext() == Previous));
		FAngelscriptEngine::AssignWorldContext(nullptr);
	}

	TEST_METHOD(DeterminesOutputTypeMatchesIntendedObjectParameter)
	{
		FMixinsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic));
		UClass* Requested = URuntimeBindingMixinTarget::StaticClass(); UObject* Result = nullptr; UClass* Determined = nullptr;
		void* Args[] = {&Requested}; const EAngelscriptReflectedParameterDirection In[] = {EAngelscriptReflectedParameterDirection::In};
		ASSERT_THAT(IsTrue(F.Invoke(URuntimeBindingStaticLibrary::StaticClass(), nullptr, nullptr,
			TEXT("SelectType"), Args, In, &Result, &Determined), *F.Diagnostic));
		ASSERT_THAT(IsTrue(Determined == Requested));
	}

	TEST_METHOD(FailureRestoresPreviousExplicitCallScopeWithoutAmbientDefault)
	{
		FMixinsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic));
		auto* Previous = NewObject<URuntimeBindingMixinTarget>(); auto* World = NewObject<UWorld>(GetTransientPackage());
		FAngelscriptEngine::AssignWorldContext(Previous); bool Result = false; int32 Wrong = 1; void* Args[] = {&Wrong};
		const EAngelscriptReflectedParameterDirection In[] = {EAngelscriptReflectedParameterDirection::In};
		ASSERT_THAT(IsFalse(F.Invoke(URuntimeBindingStaticLibrary::StaticClass(), nullptr, World,
			TEXT("ObserveWorld"), Args, In, &Result)));
		ASSERT_THAT(IsTrue(F.Diagnostic.Contains(TEXT("expected 0 typed arguments"))));
		ASSERT_THAT(IsTrue(FAngelscriptEngine::GetAmbientWorldContext() == Previous));
		FAngelscriptEngine::AssignWorldContext(nullptr); ASSERT_THAT(IsNull(FAngelscriptEngine::GetAmbientWorldContext()));
	}
};
#endif
