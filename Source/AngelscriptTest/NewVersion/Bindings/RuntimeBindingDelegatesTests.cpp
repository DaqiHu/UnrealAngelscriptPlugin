#include "CQTest.h"
#include "RuntimeBindingDelegateTestTypes.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"
#include "Core/AngelscriptTypeBindInfoReflection.h"
#include "UObject/UnrealType.h"

int32 FRuntimeBindingDelegatePayload::LiveCopies = 0;
int32 FRuntimeBindingDelegatePayload::Destructions = 0;

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingDelegatesFixture
{
struct FDelegateParameters { int32 Value = 0; };

struct FDelegatesFixture
{
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TSharedPtr<const FAngelscriptReflectedTypeSnapshot> Snapshot;
	TUniquePtr<FAngelscriptEngine> Owner;
	FString Diagnostic;

	bool Create()
	{
		auto Fail = [this](const TCHAR* Stage) { Diagnostic = FString(Stage) + TEXT(": ") + Diagnostic; UE_LOG(LogTemp, Error, TEXT("Delegate fixture %s"), *Diagnostic); return false; };
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return Fail(TEXT("Providers"));
		const auto& Collection = FAngelscriptBind::GetRegisteredCollection();
		if (!FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(Collection, *Store, Diagnostic)) return Fail(TEXT("Catalog"));
		Snapshot = Store->GetReflectedSnapshot();
		if (!Snapshot.IsValid() || !Snapshot->Find(URuntimeBindingDelegateReceiver::StaticClass())
			|| !Snapshot->Find(FRuntimeBindingDelegatePayload::StaticStruct())) return Fail(TEXT("Capture"));
		const FName Providers[] = {TEXT("Delegates.Functions"), TEXT("FAngelscriptDelegateWithPayload.Manual"),
			TEXT("BlueprintEvents.HelperGlobals"), TEXT("FLatentActionInfo")};
		if (!FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(Collection, Providers, *Store, Diagnostic)) return Fail(TEXT("Surfaces"));
		UObject* FunctionOwners[] = {URuntimeBindingDelegateReceiver::StaticClass()};
		if (!FAngelscriptTypeBindInfoReflection::RecordFunctions(*Store, Snapshot.ToSharedRef(), Diagnostic, FunctionOwners)) return Fail(TEXT("Functions"));
		if (!Store->Seal(Diagnostic)) return Fail(TEXT("Seal"));
		FAngelscriptBindingDiagnostic BindingDiagnostic; Owner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic);
		if (!Owner) { Diagnostic = BindingDiagnostic.Message; return Fail(TEXT("Create")); }
		return true;
	}

	const FAngelscriptTypeBindInfoInstallation& Install() const { return *Owner->GetBindingInstallation(); }
	UFunction* Signature(const FName PropertyName) const
	{
		const FProperty* Property = URuntimeBindingDelegateReceiver::StaticClass()->FindPropertyByName(PropertyName);
		if (const auto* Single = CastField<FDelegateProperty>(Property)) return Single->SignatureFunction;
		if (const auto* Multi = CastField<FMulticastDelegateProperty>(Property)) return Multi->SignatureFunction;
		return nullptr;
	}
};
}

using namespace RuntimeBindingDelegatesFixture;
TEST_CLASS_WITH_FLAGS(Delegates, "Angelscript.UnitTest.RuntimeBindings.Reflection.Delegates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(NativeSubscriberExecutesOnce)
	{
		FScriptDelegate Delegate; FDelegatesFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); auto* Receiver = NewObject<URuntimeBindingDelegateReceiver>();
		ASSERT_THAT(IsTrue(F.Install().BindDelegate(Delegate, Receiver, TEXT("OnValue"), F.Signature(TEXT("Single")), F.Diagnostic), *F.Diagnostic));
		FDelegateParameters Params{7}; ASSERT_THAT(IsTrue(F.Install().ExecuteDelegate(Delegate, F.Signature(TEXT("Single")), &Params, F.Diagnostic), *F.Diagnostic));
		ASSERT_THAT(AreEqual(1, Receiver->Calls)); ASSERT_THAT(AreEqual(7, Receiver->Total));
	}

	TEST_METHOD(MulticastInvokesTwoAndRemovalPreventsRemovedCall)
	{
		FMulticastScriptDelegate Delegate; FDelegatesFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); auto* A = NewObject<URuntimeBindingDelegateReceiver>(); auto* B = NewObject<URuntimeBindingDelegateReceiver>();
		ASSERT_THAT(IsTrue(F.Install().AddMulticastDelegate(Delegate, A, TEXT("OnValue"), F.Signature(TEXT("Multicast")), F.Diagnostic), *F.Diagnostic));
		ASSERT_THAT(IsTrue(F.Install().AddMulticastDelegate(Delegate, B, TEXT("OnValue"), F.Signature(TEXT("Multicast")), F.Diagnostic), *F.Diagnostic));
		FDelegateParameters Params{3}; ASSERT_THAT(IsTrue(F.Install().ExecuteMulticastDelegate(Delegate, F.Signature(TEXT("Multicast")), &Params, F.Diagnostic), *F.Diagnostic));
		ASSERT_THAT(AreEqual(3, A->Total)); ASSERT_THAT(AreEqual(3, B->Total));
		ASSERT_THAT(IsTrue(F.Install().RemoveMulticastDelegate(Delegate, A, TEXT("OnValue"), F.Diagnostic), *F.Diagnostic)); Params.Value = 4;
		ASSERT_THAT(IsTrue(F.Install().ExecuteMulticastDelegate(Delegate, F.Signature(TEXT("Multicast")), &Params, F.Diagnostic), *F.Diagnostic));
		ASSERT_THAT(AreEqual(3, A->Total)); ASSERT_THAT(AreEqual(7, B->Total));
	}

	TEST_METHOD(PayloadSurvivesCallerFrameThenReleasesAtUnbind)
	{
		FRuntimeBindingDelegatePayload::LiveCopies = FRuntimeBindingDelegatePayload::Destructions = 0;
		FDelegatesFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); auto* Receiver = NewObject<URuntimeBindingDelegateReceiver>(); int32 Handle = INDEX_NONE;
		{ FRuntimeBindingDelegatePayload Payload(11); ASSERT_THAT(IsTrue(F.Install().BindDelegatePayload(Receiver, TEXT("OnPayload"), FRuntimeBindingDelegatePayload::StaticStruct(), &Payload, Handle, F.Diagnostic), *F.Diagnostic)); }
		ASSERT_THAT(IsTrue(F.Install().ExecuteDelegatePayload(Handle, F.Diagnostic), *F.Diagnostic)); ASSERT_THAT(AreEqual(11, Receiver->Total));
		const int32 Before = FRuntimeBindingDelegatePayload::Destructions; ASSERT_THAT(IsTrue(F.Install().UnbindDelegatePayload(Handle, F.Diagnostic), *F.Diagnostic));
		ASSERT_THAT(IsTrue(FRuntimeBindingDelegatePayload::Destructions > Before));
	}

	TEST_METHOD(ReflectedEventArgumentAndOutSignatureMatchesFixture)
	{
		FDelegatesFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); const auto* Record = F.Install().RegisterReflectedEvent(URuntimeBindingDelegateReceiver::StaticClass(), TEXT("ReflectedEvent"), F.Diagnostic);
		ASSERT_THAT(IsNotNull(Record)); ASSERT_THAT(AreEqual(2, Record->Parameters.Num()));
		ASSERT_THAT(IsTrue(Record->Parameters[0].Direction == EAngelscriptReflectedParameterDirection::In));
		ASSERT_THAT(IsTrue(Record->Parameters[1].Direction == EAngelscriptReflectedParameterDirection::Out));
	}

	TEST_METHOD(WrongSignatureAndForeignOwnerAreRejected)
	{
		FScriptDelegate Delegate; FDelegatesFixture A, B; ASSERT_THAT(IsTrue(A.Create(), *A.Diagnostic)); ASSERT_THAT(IsTrue(B.Create(), *B.Diagnostic)); auto* Receiver = NewObject<URuntimeBindingDelegateReceiver>();
		ASSERT_THAT(IsFalse(A.Install().BindDelegate(Delegate, Receiver, TEXT("WrongSignature"), A.Signature(TEXT("Single")), A.Diagnostic)));
		ASSERT_THAT(IsTrue(A.Install().BindDelegate(Delegate, Receiver, TEXT("OnValue"), A.Signature(TEXT("Single")), A.Diagnostic), *A.Diagnostic));
		FDelegateParameters Params{2}; ASSERT_THAT(IsFalse(B.Install().ExecuteDelegate(Delegate, B.Signature(TEXT("Single")), &Params, B.Diagnostic)));
	}

	TEST_METHOD(DestroyingOwnerReleasesItsRegistryWithoutAffectingAnother)
	{
		FScriptDelegate DA, DB; FDelegatesFixture A, B; ASSERT_THAT(IsTrue(A.Create(), *A.Diagnostic)); ASSERT_THAT(IsTrue(B.Create(), *B.Diagnostic));
		auto* ReceiverA = NewObject<URuntimeBindingDelegateReceiver>(); auto* ReceiverB = NewObject<URuntimeBindingDelegateReceiver>();
		ASSERT_THAT(IsTrue(A.Install().BindDelegate(DA, ReceiverA, TEXT("OnValue"), A.Signature(TEXT("Single")), A.Diagnostic), *A.Diagnostic));
		ASSERT_THAT(IsTrue(B.Install().BindDelegate(DB, ReceiverB, TEXT("OnValue"), B.Signature(TEXT("Single")), B.Diagnostic), *B.Diagnostic));
		ASSERT_THAT(AreEqual(1, A.Install().GetDelegateRegistryEntryCount())); ASSERT_THAT(AreEqual(1, B.Install().GetDelegateRegistryEntryCount()));
		A.Owner.Reset(); ASSERT_THAT(IsFalse(DA.IsBound())); ASSERT_THAT(IsTrue(DB.IsBound())); ASSERT_THAT(AreEqual(1, B.Install().GetDelegateRegistryEntryCount()));
	}
};
#endif
