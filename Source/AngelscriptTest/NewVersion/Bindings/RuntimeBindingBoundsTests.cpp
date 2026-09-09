#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingBoundsFixture
{
using namespace RuntimeBindingTestSupport;

static const FName BoundsProviderNames[] = {
	TEXT("FBox.Functions"), TEXT("FBox2D"), TEXT("FBox3f.Functions"), TEXT("FBoxSphereBounds.Functions"),
	TEXT("FBoxSphereBounds3f.Functions"), TEXT("FPlane"), TEXT("FPlane4f"), TEXT("FSphere.Functions"), TEXT("FSphere3f.Functions")
};

struct FBoundsFixture
{
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TUniquePtr<FAngelscriptEngine> Owner;
	FString Diagnostic;

	bool Record(bool bCreateOwner = true)
	{
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return false;
		const auto& Collection = FAngelscriptBind::GetRegisteredCollection();
		if (!FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(Collection, *Store, Diagnostic)
			|| !FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(Collection, BoundsProviderNames, *Store, Diagnostic)
			|| !Store->Seal(Diagnostic))
		{
			UE_LOG(LogTemp, Error, TEXT("Bounds fixture recording failed: %s"), *Diagnostic);
			return false;
		}
		if (!bCreateOwner) return true;
		FAngelscriptBindingDiagnostic BindingDiagnostic;
		Owner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic);
		if (!Owner) { Diagnostic = BindingDiagnostic.Message; UE_LOG(LogTemp, Error, TEXT("Bounds fixture creation failed: %s"), *Diagnostic); }
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

struct FBoundsContext
{
	asIScriptContext* Ptr = nullptr;
	explicit FBoundsContext(FBoundsFixture& Fixture) : Ptr(Fixture.Owner->Engine->CreateContext()) {}
	~FBoundsContext() { if (Ptr) Ptr->Release(); }
	asIScriptContext* operator->() const { return Ptr; }
};
}

using namespace RuntimeBindingBoundsFixture;
TEST_CLASS_WITH_FLAGS(Bounds, "Angelscript.UnitTest.RuntimeBindings.Values.Bounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(AllDeclarationsAndCompleteProviderSurfaceAreRecorded)
	{
		FBoundsFixture F; ASSERT_THAT(IsTrue(F.Record(false)));
		for (const TCHAR* Name : {TEXT("FBox"), TEXT("FBox2D"), TEXT("FBox3f"), TEXT("FBoxSphereBounds"),
			TEXT("FBoxSphereBounds3f"), TEXT("FPlane"), TEXT("FPlane4f"), TEXT("FSphere"), TEXT("FSphere3f")})
			ASSERT_THAT(IsNotNull(F.Store->GetType(F.Store->FindType(Name))));
		int32 Count = 0;
		for (const auto& Type : F.Store->GetTypes()) for (const auto& Member : Type.Members)
			for (const FName ProviderName : BoundsProviderNames) if (Member.Source.Provider == ProviderName) { ++Count; break; }
		ASSERT_THAT(AreEqual(144, Count));
	}

	TEST_METHOD(BoxCenterUsesLiteralCorners)
	{
		FBoundsFixture F; ASSERT_THAT(IsTrue(F.Record())); FBox Box(FVector(0.0), FVector(2.0, 4.0, 6.0)); FBoundsContext C(F);
		auto* Fn = F.Function(TEXT("FBox"), TEXT("FVector GetCenter() const")); ASSERT_THAT(IsNotNull(Fn));
		ASSERT_THAT(AreEqual(0, C->Prepare(Fn))); ASSERT_THAT(AreEqual(0, C->SetObject(&Box))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		const auto* Center = static_cast<const FVector*>(C->GetReturnObject()); ASSERT_THAT(IsNotNull(Center));
		ASSERT_THAT(IsTrue(Center->Equals(FVector(1.0, 2.0, 3.0), 0.0)));
	}

	TEST_METHOD(BoxInsideAndOutsidePointsClassifyCorrectly)
	{
		FBoundsFixture F; ASSERT_THAT(IsTrue(F.Record())); FBox Box(FVector(0.0), FVector(2.0, 4.0, 6.0)); FVector Inside(1.0, 2.0, 3.0), Outside(3.0, 2.0, 3.0); FBoundsContext C(F);
		auto* Fn = F.Function(TEXT("FBox"), TEXT("bool IsInside( const FVector& In ) const")); ASSERT_THAT(IsNotNull(Fn));
		ASSERT_THAT(AreEqual(0, C->Prepare(Fn))); ASSERT_THAT(AreEqual(0, C->SetObject(&Box))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Inside)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(IsTrue(C->GetReturnByte() != 0));
		ASSERT_THAT(AreEqual(0, C->Prepare(Fn))); ASSERT_THAT(AreEqual(0, C->SetObject(&Box))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Outside)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(IsFalse(C->GetReturnByte() != 0));
	}

	TEST_METHOD(BoxUnionExpandsBothEnds)
	{
		FBoundsFixture F; ASSERT_THAT(IsTrue(F.Record())); FBox A(FVector(0.0), FVector(1.0)), B(FVector(-2.0), FVector(3.0)); FBoundsContext C(F);
		auto* Fn = F.Function(TEXT("FBox"), TEXT("FBox opAdd(const FBox& Other) const")); ASSERT_THAT(IsNotNull(Fn));
		ASSERT_THAT(AreEqual(0, C->Prepare(Fn))); ASSERT_THAT(AreEqual(0, C->SetObject(&A))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &B)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); const auto* Union = static_cast<const FBox*>(C->GetReturnObject()); ASSERT_THAT(IsNotNull(Union));
		ASSERT_THAT(IsTrue(Union->Min.Equals(FVector(-2.0), 0.0) && Union->Max.Equals(FVector(3.0), 0.0)));
	}

	TEST_METHOD(SphereRadiusTwoContainsOriginButNotThree)
	{
		FBoundsFixture F; ASSERT_THAT(IsTrue(F.Record())); FSphere Sphere(FVector::ZeroVector, 2.0); FVector Origin = FVector::ZeroVector, Far(3.0, 0.0, 0.0); FBoundsContext C(F);
		auto* Fn = F.Function(TEXT("FSphere"), TEXT("bool IsInside(const FVector& In, float64 Tolerance = KINDA_SMALL_NUMBER) const")); ASSERT_THAT(IsNotNull(Fn));
		ASSERT_THAT(AreEqual(0, C->Prepare(Fn))); ASSERT_THAT(AreEqual(0, C->SetObject(&Sphere))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Origin))); ASSERT_THAT(AreEqual(0, C->SetArgDouble(1, KINDA_SMALL_NUMBER)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(IsTrue(C->GetReturnByte() != 0));
		ASSERT_THAT(AreEqual(0, C->Prepare(Fn))); ASSERT_THAT(AreEqual(0, C->SetObject(&Sphere))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Far))); ASSERT_THAT(AreEqual(0, C->SetArgDouble(1, KINDA_SMALL_NUMBER)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(IsFalse(C->GetReturnByte() != 0));
	}

	TEST_METHOD(PlaneDistanceUsesKnownNormal)
	{
		FBoundsFixture F; ASSERT_THAT(IsTrue(F.Record())); FPlane Plane(FVector(0.0, 0.0, 2.0), FVector::UpVector); FVector Point(0.0, 0.0, 5.0); FBoundsContext C(F);
		auto* Fn = F.Function(TEXT("FPlane"), TEXT("float64 PlaneDot(const FVector& Location) const")); ASSERT_THAT(IsNotNull(Fn));
		ASSERT_THAT(AreEqual(0, C->Prepare(Fn))); ASSERT_THAT(AreEqual(0, C->SetObject(&Plane))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Point)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(IsTrue(C->GetReturnDouble() == 3.0));
	}

	TEST_METHOD(DefaultBoxConstructorPreservesInvalidEmptyState)
	{
		FBoundsFixture F; ASSERT_THAT(IsTrue(F.Record())); alignas(FBox) uint8 Storage[sizeof(FBox)] = {}; FBoundsContext C(F);
		auto* Fn = F.Function(TEXT("FBox"), TEXT("void f()")); ASSERT_THAT(IsNotNull(Fn));
		ASSERT_THAT(AreEqual(0, C->Prepare(Fn))); ASSERT_THAT(AreEqual(0, C->SetObject(Storage))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		const auto& Box = *reinterpret_cast<const FBox*>(Storage); ASSERT_THAT(IsFalse(Box.IsValid != 0));
	}
};
#endif
