#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingRotationFixture
{
using namespace RuntimeBindingTestSupport;

static const FName RotationProviderNames[] = {
	TEXT("FRotator.Functions"), TEXT("FRotator3f.Functions"), TEXT("FQuat.Functions"),
	TEXT("FQuat4f.Functions"), TEXT("FTransform.Functions"), TEXT("FTransform3f.Functions"), TEXT("FMatrix")
};

struct FRotationFixture
{
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TUniquePtr<FAngelscriptEngine> Owner;
	FString Diagnostic;

	bool Record(bool bCreateOwner = true)
	{
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return false;
		const auto& Collection = FAngelscriptBind::GetRegisteredCollection();
		if (!FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(Collection, *Store, Diagnostic)
			|| !FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(Collection, RotationProviderNames, *Store, Diagnostic)
			|| !Store->Seal(Diagnostic))
		{
			UE_LOG(LogTemp, Error, TEXT("Rotation fixture recording failed: %s"), *Diagnostic);
			return false;
		}
		if (!bCreateOwner) return true;
		FAngelscriptBindingDiagnostic BindingDiagnostic;
		Owner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic);
		if (!Owner) { Diagnostic = BindingDiagnostic.Message; UE_LOG(LogTemp, Error, TEXT("Rotation fixture creation failed: %s"), *Diagnostic); }
		return Owner.IsValid();
	}

	asCScriptFunction* Function(const TCHAR* OwnerName, const TCHAR* Declaration) const
	{
		for (const auto& Installed : Owner->GetBindingInstallation()->GetMembers())
		{
			const auto* Record = Store->GetMember(Installed.Record);
			const auto* Type = Store->GetType(Installed.Record.Type);
			if (Installed.Function && Type && Type->Declaration.Name == OwnerName && Record && Record->Declaration == Declaration)
				return Installed.Function;
		}
		return nullptr;
	}
};

struct FRotationContext
{
	asIScriptContext* Ptr = nullptr;
	explicit FRotationContext(FRotationFixture& Fixture) : Ptr(Fixture.Owner->Engine->CreateContext()) {}
	~FRotationContext() { if (Ptr) Ptr->Release(); }
	asIScriptContext* operator->() const { return Ptr; }
};

template<typename ResultType, typename ObjectType, typename ArgumentType>
static ResultType InvokeConstAddressMethod(FRotationFixture& Fixture, const TCHAR* OwnerName, const TCHAR* Declaration,
	ObjectType& Object, ArgumentType& Argument)
{
	FRotationContext Context(Fixture);
	auto* Function = Fixture.Function(OwnerName, Declaration);
	if (!Function || Context->Prepare(Function) < 0 || Context->SetObject(&Object) < 0
		|| Context->SetArgAddress(0, &Argument) < 0 || Context->Execute() != asEXECUTION_FINISHED)
		return ResultType{};
	return *static_cast<const ResultType*>(Context->GetReturnObject());
}
}

using namespace RuntimeBindingRotationFixture;
TEST_CLASS_WITH_FLAGS(Rotations, "Angelscript.UnitTest.RuntimeBindings.Values.Rotations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(AllFamilyDeclarationsAndProviderSurfaceAreRecorded)
	{
		FRotationFixture F; ASSERT_THAT(IsTrue(F.Record(false)));
		for (const TCHAR* Name : {TEXT("FRotator"), TEXT("FRotator3f"), TEXT("FQuat"), TEXT("FQuat4f"),
			TEXT("FTransform"), TEXT("FTransform3f"), TEXT("FMatrix")})
		{
			const auto* Type = F.Store->GetType(F.Store->FindType(Name));
			ASSERT_THAT(IsNotNull(Type)); ASSERT_THAT(IsTrue(Type->Members.Num() >= 30));
		}
		int32 Count = 0;
		for (const auto& Type : F.Store->GetTypes()) for (const auto& Member : Type.Members)
			for (const FName ProviderName : RotationProviderNames) if (Member.Source.Provider == ProviderName) { ++Count; break; }
		ASSERT_THAT(AreEqual(386, Count));
	}

	TEST_METHOD(IdentityTransformPreservesLiteralPosition)
	{
		FRotationFixture F; ASSERT_THAT(IsTrue(F.Record())); FTransform Identity = FTransform::Identity; FVector Input(1.0, 2.0, 3.0);
		const FVector Result = InvokeConstAddressMethod<FVector>(F, TEXT("FTransform"), TEXT("FVector TransformPosition(const FVector& V) const"), Identity, Input);
		ASSERT_THAT(IsTrue(Result.Equals(Input, 0.0)));
	}

	TEST_METHOD(TranslationAddsTenTwentyThirty)
	{
		FRotationFixture F; ASSERT_THAT(IsTrue(F.Record())); FTransform Translation(FVector(10.0, 20.0, 30.0)); FVector Input(1.0, 2.0, 3.0);
		const FVector Result = InvokeConstAddressMethod<FVector>(F, TEXT("FTransform"), TEXT("FVector TransformPosition(const FVector& V) const"), Translation, Input);
		ASSERT_THAT(IsTrue(Result.Equals(FVector(11.0, 22.0, 33.0), 0.0)));
	}

	TEST_METHOD(InverseTransformReturnsOriginalWithinEpsilon)
	{
		FRotationFixture F; ASSERT_THAT(IsTrue(F.Record()));
		FTransform Transform(FRotator(17.0, 31.0, -9.0), FVector(10.0, 20.0, 30.0), FVector(2.0, 3.0, 4.0)); FVector Input(1.0, 2.0, 3.0);
		FVector World = InvokeConstAddressMethod<FVector>(F, TEXT("FTransform"), TEXT("FVector TransformPosition(const FVector& V) const"), Transform, Input);
		FVector Local = InvokeConstAddressMethod<FVector>(F, TEXT("FTransform"), TEXT("FVector InverseTransformPosition(const FVector& V) const"), Transform, World);
		ASSERT_THAT(IsTrue(Local.Equals(Input, 1.e-9)));
	}

	TEST_METHOD(IdentityQuaternionCompositionIsUnchanged)
	{
		FRotationFixture F; ASSERT_THAT(IsTrue(F.Record())); FQuat Value(FVector::UpVector, 0.75); FQuat Identity = FQuat::Identity;
		const FQuat Result = InvokeConstAddressMethod<FQuat>(F, TEXT("FQuat"), TEXT("FQuat opMul(const FQuat& Other) const"), Value, Identity);
		ASSERT_THAT(IsTrue(Result.Equals(Value, 1.e-12)));
	}

	TEST_METHOD(MatrixIdentityPreservesHomogeneousPosition)
	{
		FRotationFixture F; ASSERT_THAT(IsTrue(F.Record())); FMatrix Identity = FMatrix::Identity; FVector Input(1.0, 2.0, 3.0);
		const FVector4 Result = InvokeConstAddressMethod<FVector4>(F, TEXT("FMatrix"), TEXT("FVector4 TransformPosition(const FVector& Position) const"), Identity, Input);
		ASSERT_THAT(IsTrue(Result.X == 1.0 && Result.Y == 2.0 && Result.Z == 3.0 && Result.W == 1.0));
	}
};
#endif
