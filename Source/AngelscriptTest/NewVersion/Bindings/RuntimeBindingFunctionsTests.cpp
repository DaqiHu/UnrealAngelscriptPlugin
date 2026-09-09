#include "CQTest.h"
#include "RuntimeBindingFunctionTestTypes.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"
#include "Core/AngelscriptTypeBindInfoReflection.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingFunctionsFixture
{
struct FFunctionsFixture
{
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TSharedPtr<const FAngelscriptReflectedTypeSnapshot> Snapshot;
	TUniquePtr<FAngelscriptEngine> Owner;
	FString Diagnostic;

	bool Create()
	{
		auto Fail = [this](const TCHAR* Stage)
		{
			Diagnostic = FString(Stage) + TEXT(": ") + Diagnostic;
			UE_LOG(LogTemp, Error, TEXT("RuntimeBindingFunctions fixture %s"), *Diagnostic);
			return false;
		};
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return Fail(TEXT("Providers"));
		TArray<UObject*> Roots = {
			URuntimeBindingFunctionFixtureChild::StaticClass(),
			URuntimeBindingFunctionFixtureParent::StaticClass(),
			URuntimeBindingFunctionArgument::StaticClass(),
			FRuntimeBindingFunctionPayload::StaticStruct()
		};
		if (!FAngelscriptTypeBindInfoReflection::Capture(Roots, Store->GetPolicy(), Snapshot, Diagnostic)) return Fail(TEXT("Capture"));
		if (!FAngelscriptTypeBindInfoReflection::RecordDeclarations(*Store, Snapshot.ToSharedRef(), Diagnostic)) return Fail(TEXT("Declarations"));
		const FName DependencyProviders[] = {TEXT("FString.TypeDeclarations")};
		if (!FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(
			FAngelscriptBind::GetRegisteredCollection(), DependencyProviders, *Store, Diagnostic)) return Fail(TEXT("Catalog"));
		if (!FAngelscriptTypeBindInfoReflection::RecordFunctions(*Store, Snapshot.ToSharedRef(), Diagnostic)) return Fail(TEXT("Functions"));
		if (!Store->Seal(Diagnostic)) return Fail(TEXT("Seal"));
		FAngelscriptBindingDiagnostic BindingDiagnostic;
		Owner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic);
		if (!Owner) { Diagnostic = BindingDiagnostic.Message; return Fail(TEXT("Create")); }
		return Owner.IsValid();
	}

	const FAngelscriptReflectedTypeRecord* Record(UObject* Native) const { return Snapshot->Find(Native); }
	asCTypeInfo* Type(UObject* Native) const
	{
		const auto* R = Record(Native);
		return R ? Owner->GetBindingInstallation()->FindType(R->Declaration.Name, R->Declaration.Namespace) : nullptr;
	}
	const FAngelscriptTypeBindInfoInstallation& Install() const { return *Owner->GetBindingInstallation(); }
	asCScriptFunction* Function(const TCHAR* Name) const
	{
		for (const auto& Member : Install().GetMembers())
		{
			const auto* RecordMember = Store->GetMember(Member.Record);
			const auto* OwnerRecord = Store->GetType(Member.Record.Type);
			if (Member.Function && OwnerRecord && RecordMember
				&& OwnerRecord->Declaration.Name == Record(URuntimeBindingFunctionFixtureParent::StaticClass())->Declaration.Name
				&& RecordMember->Declaration.Contains(FString::Printf(TEXT(" %s("), Name)))
				return Member.Function;
		}
		return nullptr;
	}
	bool Invoke(UObject* Receiver, FName Name, TConstArrayView<void*> Arguments,
		TConstArrayView<EAngelscriptReflectedParameterDirection> Directions, void* ReturnAddress = nullptr,
		EAngelscriptReflectedFunctionDispatch* Dispatch = nullptr)
	{
		return Install().InvokeReflectedFunction(Type(URuntimeBindingFunctionFixtureChild::StaticClass()),
			URuntimeBindingFunctionFixtureChild::StaticClass(), Receiver, Name, Arguments, Directions,
			ReturnAddress, Dispatch, Diagnostic);
	}
};
}

using namespace RuntimeBindingFunctionsFixture;
TEST_CLASS_WITH_FLAGS(Functions, "Angelscript.UnitTest.RuntimeBindings.Reflection.Functions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ReflectedAddExecutesThroughInstalledGenericCallable)
	{
		FFunctionsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic));
		auto* Object = NewObject<URuntimeBindingFunctionFixtureChild>();
		auto* Function = F.Function(TEXT("Add")); ASSERT_THAT(IsNotNull(Function));
		asIScriptContext* Context = F.Owner->Engine->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function))); ASSERT_THAT(AreEqual(0, Context->SetObject(Object)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 2))); ASSERT_THAT(AreEqual(0, Context->SetArgDWord(1, 3)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), Context->Execute())); ASSERT_THAT(AreEqual(asDWORD(5), Context->GetReturnDWord()));
		Context->Release();
	}

	TEST_METHOD(StringAndStructReturnsOwnIndependentLifetime)
	{
		FFunctionsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); auto* Object = NewObject<URuntimeBindingFunctionFixtureChild>();
		FString Input = TEXT("value"), StringResult; void* StringArgs[] = {&Input};
		const EAngelscriptReflectedParameterDirection In[] = {EAngelscriptReflectedParameterDirection::In};
		ASSERT_THAT(IsTrue(F.Invoke(Object, TEXT("MakeString"), StringArgs, In, &StringResult), *F.Diagnostic));
		ASSERT_THAT(AreEqual(FString(TEXT("value-owned")), StringResult)); Input = TEXT("changed");
		ASSERT_THAT(AreEqual(FString(TEXT("value-owned")), StringResult));
		FString PayloadInput = TEXT("payload"); FRuntimeBindingFunctionPayload PayloadResult; void* PayloadArgs[] = {&PayloadInput};
		ASSERT_THAT(IsTrue(F.Invoke(Object, TEXT("MakePayload"), PayloadArgs, In, &PayloadResult), *F.Diagnostic));
		PayloadInput = TEXT("changed"); ASSERT_THAT(AreEqual(FString(TEXT("payload")), PayloadResult.Text));
	}

	TEST_METHOD(OutAndInOutWriteBackSevenAndEight)
	{
		FFunctionsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); auto* Object = NewObject<URuntimeBindingFunctionFixtureChild>();
		int32 Out = 0, InOut = 3; void* Args[] = {&Out, &InOut};
		const EAngelscriptReflectedParameterDirection Directions[] = {
			EAngelscriptReflectedParameterDirection::Out, EAngelscriptReflectedParameterDirection::InOut};
		ASSERT_THAT(IsTrue(F.Invoke(Object, TEXT("Fill"), Args, Directions), *F.Diagnostic));
		ASSERT_THAT(AreEqual(7, Out)); ASSERT_THAT(AreEqual(8, InOut));
	}

	TEST_METHOD(InvalidReceiverObjectAndDirectionAreDiagnosed)
	{
		FFunctionsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic));
		int32 A = 2, B = 3, Result = 0; void* AddArgs[] = {&A, &B};
		const EAngelscriptReflectedParameterDirection Inputs[] = {
			EAngelscriptReflectedParameterDirection::In, EAngelscriptReflectedParameterDirection::In};
		ASSERT_THAT(IsFalse(F.Invoke(nullptr, TEXT("Add"), AddArgs, Inputs, &Result))); ASSERT_THAT(IsTrue(F.Diagnostic.Contains(TEXT("non-null receiver"))));
		auto* Object = NewObject<URuntimeBindingFunctionFixtureChild>(); UObject* Wrong = NewObject<URuntimeBindingFunctionFixtureParent>(); void* ObjectArgs[] = {&Wrong};
		const EAngelscriptReflectedParameterDirection OneInput[] = {EAngelscriptReflectedParameterDirection::In};
		ASSERT_THAT(IsFalse(F.Invoke(Object, TEXT("AcceptArgument"), ObjectArgs, OneInput, &Result))); ASSERT_THAT(IsTrue(F.Diagnostic.Contains(TEXT("incompatible"))));
		int32 Out = 0, InOut = 3; void* FillArgs[] = {&Out, &InOut};
		ASSERT_THAT(IsFalse(F.Invoke(Object, TEXT("Fill"), FillArgs, Inputs))); ASSERT_THAT(IsTrue(F.Diagnostic.Contains(TEXT("direction mismatch"))));
	}

	TEST_METHOD(GeneratedMethodIsSelectedOverReflectiveFallback)
	{
		FFunctionsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); auto* Object = NewObject<URuntimeBindingFunctionFixtureChild>();
		int32 A = 2, B = 3, Result = 0; void* Args[] = {&A, &B}; const EAngelscriptReflectedParameterDirection Inputs[] = {
			EAngelscriptReflectedParameterDirection::In, EAngelscriptReflectedParameterDirection::In};
		EAngelscriptReflectedFunctionDispatch Dispatch = EAngelscriptReflectedFunctionDispatch::ReflectiveFallback;
		ASSERT_THAT(IsTrue(F.Invoke(Object, TEXT("Add"), Args, Inputs, &Result, &Dispatch), *F.Diagnostic));
		ASSERT_THAT(AreEqual(5, Result)); ASSERT_THAT(IsTrue(Dispatch == EAngelscriptReflectedFunctionDispatch::Generated));
	}

	TEST_METHOD(InheritedFunctionUsesChildReceiverAndParentOwner)
	{
		FFunctionsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic)); auto* Object = NewObject<URuntimeBindingFunctionFixtureChild>();
		int32 A = 8, B = 5, Result = 0; void* Args[] = {&A, &B}; const EAngelscriptReflectedParameterDirection Inputs[] = {
			EAngelscriptReflectedParameterDirection::In, EAngelscriptReflectedParameterDirection::In};
		ASSERT_THAT(IsTrue(F.Invoke(Object, TEXT("Add"), Args, Inputs, &Result), *F.Diagnostic)); ASSERT_THAT(AreEqual(13, Result));
		const auto* Found = F.Install().FindReflectedFunction(F.Type(URuntimeBindingFunctionFixtureChild::StaticClass()),
			URuntimeBindingFunctionFixtureChild::StaticClass(), TEXT("Add"), F.Diagnostic);
		ASSERT_THAT(IsNotNull(Found)); ASSERT_THAT(IsTrue(Found->NativeFunction->GetOuterUClass() == URuntimeBindingFunctionFixtureParent::StaticClass()));
	}

	TEST_METHOD(AllEligibleMethodsAndDefaultsAreAccounted)
	{
		FFunctionsFixture F; ASSERT_THAT(IsTrue(F.Create(), *F.Diagnostic));
		const auto* Parent = F.Record(URuntimeBindingFunctionFixtureParent::StaticClass()); ASSERT_THAT(IsNotNull(Parent));
		int32 ParentEligible = 0; for (const auto& Function : Parent->Functions) ParentEligible += Function.bEligible ? 1 : 0;
		ASSERT_THAT(AreEqual(5, ParentEligible));
		int32 SnapshotEligible = 0;
		for (const auto& Type : F.Snapshot->GetTypes())
			if (Type.bEligible) for (const auto& Function : Type.Functions) SnapshotEligible += Function.bEligible ? 1 : 0;
		ASSERT_THAT(AreEqual(6, SnapshotEligible));
		ASSERT_THAT(AreEqual(SnapshotEligible, F.Install().GetInstalledReflectedFunctionCount()));
		const auto* Add = Parent->Functions.FindByPredicate([](const auto& Function) { return Function.Name == TEXT("Add"); });
		ASSERT_THAT(IsNotNull(Add)); ASSERT_THAT(AreEqual(FString(TEXT("3")), Add->Parameters[1].DefaultValue));
		ASSERT_THAT(IsTrue(F.Function(TEXT("Add")) != nullptr));
	}
};
#endif
