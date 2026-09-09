#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingVectorFixture
{
using namespace RuntimeBindingTestSupport;

static const FName ProviderNames[] = {
	TEXT("FVector"), TEXT("FVector2D.Functions"), TEXT("FVector2f.Functions"), TEXT("FVector3f.Manual"),
	TEXT("FVector4.Functions"), TEXT("FVector4f.Functions"), TEXT("FIntPoint.Functions"),
	TEXT("FIntVector.Functions"), TEXT("FIntVector2.Functions"), TEXT("FIntVector4.Functions")
};

struct FFixture
{
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TUniquePtr<FAngelscriptEngine> Owner;
	FString Diagnostic;

	bool Record(bool bCreateOwner = true)
	{
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return false;
		const auto& Collection = FAngelscriptBind::GetRegisteredCollection();
		if (!FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(Collection, *Store, Diagnostic)
			|| !FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(Collection, ProviderNames, *Store, Diagnostic)
			|| !Store->Seal(Diagnostic))
		{
			UE_LOG(LogTemp, Error, TEXT("Vector fixture recording failed: %s"), *Diagnostic);
			return false;
		}
		if (!bCreateOwner) return true;
		FAngelscriptBindingDiagnostic BindingDiagnostic;
		Owner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic);
		if (!Owner) { Diagnostic = BindingDiagnostic.Message; UE_LOG(LogTemp, Error, TEXT("Vector fixture creation failed: %s"), *Diagnostic); }
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

struct FContext
{
	asIScriptContext* Ptr = nullptr;
	explicit FContext(FFixture& Fixture) : Ptr(Fixture.Owner->Engine->CreateContext()) {}
	~FContext() { if (Ptr) FixtureRelease(); }
	void FixtureRelease() { Ptr->Release(); Ptr = nullptr; }
	asIScriptContext* operator->() const { return Ptr; }
};
}

using namespace RuntimeBindingVectorFixture;
TEST_CLASS_WITH_FLAGS(Vectors, "Angelscript.UnitTest.RuntimeBindings.Values.Vectors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(AllFamilyDeclarationsAndRepresentativeSurfaceAreRecorded)
	{
		FFixture F; ASSERT_THAT(IsTrue(F.Record(false)));
		int32 SelectedProviderMemberCount = 0;
		for (const TCHAR* Name : {TEXT("FVector"), TEXT("FVector2D"), TEXT("FVector2f"), TEXT("FVector3f"),
			TEXT("FVector4"), TEXT("FVector4f"), TEXT("FIntPoint"), TEXT("FIntVector"), TEXT("FIntVector2"), TEXT("FIntVector4")})
		{
			const auto Handle = F.Store->FindType(Name); const auto* Type = F.Store->GetType(Handle);
			ASSERT_THAT(IsNotNull(Type)); ASSERT_THAT(IsTrue(Type->Members.Num() >= 4));
			ASSERT_THAT(IsTrue(Type->Members.ContainsByPredicate([](const auto& M) { return M.Kind == EAngelscriptTypeBindMemberKind::Constructor; })));
			ASSERT_THAT(IsTrue(Type->Members.ContainsByPredicate([](const auto& M) { return M.Kind == EAngelscriptTypeBindMemberKind::Property; })));
		}
		for (const auto& Type : F.Store->GetTypes())
			for (const auto& Member : Type.Members)
				for (const FName ProviderName : ProviderNames)
					if (Member.Source.Provider == ProviderName)
					{
						++SelectedProviderMemberCount;
						break;
					}
		// 372 type-bound constructors/properties/methods plus 17 namespace constants.
		ASSERT_THAT(AreEqual(389, SelectedProviderMemberCount));
	}

	TEST_METHOD(RepresentativeFloatAndIntegerConstructorsUseNativeWidths)
	{
		FFixture F; ASSERT_THAT(IsTrue(F.Record())); FContext C(F);
		alignas(FVector3f) uint8 VectorStorage[sizeof(FVector3f)] = {};
		auto* VectorCtor = F.Function(TEXT("FVector3f"), TEXT("void f(float32 X, float32 Y, float32 Z)"));
		ASSERT_THAT(IsNotNull(VectorCtor)); ASSERT_THAT(AreEqual(0, C->Prepare(VectorCtor))); ASSERT_THAT(AreEqual(0, C->SetObject(VectorStorage)));
		ASSERT_THAT(AreEqual(0, C->SetArgFloat(0, 1.25f))); ASSERT_THAT(AreEqual(0, C->SetArgFloat(1, 2.5f))); ASSERT_THAT(AreEqual(0, C->SetArgFloat(2, 3.75f)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		const auto& Vector = *reinterpret_cast<const FVector3f*>(VectorStorage);
		ASSERT_THAT(IsTrue(Vector.X == 1.25f && Vector.Y == 2.5f && Vector.Z == 3.75f));

		alignas(FIntVector4) uint8 IntegerStorage[sizeof(FIntVector4)] = {};
		auto* IntegerCtor = F.Function(TEXT("FIntVector4"), TEXT("void f(int32 X, int32 Y, int32 Z, int32 W)"));
		ASSERT_THAT(IsNotNull(IntegerCtor)); ASSERT_THAT(AreEqual(0, C->Prepare(IntegerCtor))); ASSERT_THAT(AreEqual(0, C->SetObject(IntegerStorage)));
		ASSERT_THAT(AreEqual(0, C->SetArgDWord(0, 10))); ASSERT_THAT(AreEqual(0, C->SetArgDWord(1, 20)));
		ASSERT_THAT(AreEqual(0, C->SetArgDWord(2, 30))); ASSERT_THAT(AreEqual(0, C->SetArgDWord(3, 40)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		const auto& Integer = *reinterpret_cast<const FIntVector4*>(IntegerStorage);
		ASSERT_THAT(IsTrue(Integer.X == 10 && Integer.Y == 20 && Integer.Z == 30 && Integer.W == 40));
	}

	TEST_METHOD(DoubleVectorSizeSquaredUsesLiteralComponents)
	{
		FFixture F; ASSERT_THAT(IsTrue(F.Record())); FVector Value(1.0, 2.0, 3.0); FContext C(F);
		auto* Fn = F.Function(TEXT("FVector"), TEXT("float64 SizeSquared() const")); ASSERT_THAT(IsNotNull(Fn));
		ASSERT_THAT(AreEqual(0, C->Prepare(Fn))); ASSERT_THAT(AreEqual(0, C->SetObject(&Value)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(IsTrue(C->GetReturnDouble() == 14.0));
	}

	TEST_METHOD(DoubleVectorAdditionReturnsFiveSevenNine)
	{
		FFixture F; ASSERT_THAT(IsTrue(F.Record())); FVector A(1.0, 2.0, 3.0), B(4.0, 5.0, 6.0); FContext C(F);
		auto* Fn = F.Function(TEXT("FVector"), TEXT("FVector opAdd(const FVector& Other) const")); ASSERT_THAT(IsNotNull(Fn));
		ASSERT_THAT(AreEqual(0, C->Prepare(Fn))); ASSERT_THAT(AreEqual(0, C->SetObject(&A))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &B)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); const auto* Sum = static_cast<const FVector*>(C->GetReturnObject());
		ASSERT_THAT(IsNotNull(Sum)); ASSERT_THAT(IsTrue(Sum->X == 5.0 && Sum->Y == 7.0 && Sum->Z == 9.0));
	}

	TEST_METHOD(IntegerPointIndexAliasesOnlySelectedComponent)
	{
		FFixture F; ASSERT_THAT(IsTrue(F.Record())); FIntPoint Point(10, 20); FContext C(F);
		auto* Fn = F.Function(TEXT("FIntPoint"), TEXT("int32& opIndex(int32 Index)")); ASSERT_THAT(IsNotNull(Fn));
		ASSERT_THAT(AreEqual(0, C->Prepare(Fn))); ASSERT_THAT(AreEqual(0, C->SetObject(&Point))); ASSERT_THAT(AreEqual(0, C->SetArgDWord(0, 1)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); auto* Selected = static_cast<int32*>(C->GetReturnAddress()); ASSERT_THAT(IsNotNull(Selected));
		*Selected = 99; ASSERT_THAT(AreEqual(10, Point.X)); ASSERT_THAT(AreEqual(99, Point.Y));
	}

	TEST_METHOD(FloatAndDoubleVariantsRetainWidthLayoutAndProperties)
	{
		FFixture F; ASSERT_THAT(IsTrue(F.Record(false)));
		struct FExpected { const TCHAR* Name; uint32 Size; uint32 Alignment; const TCHAR* Property; };
		for (const FExpected& E : {FExpected{TEXT("FVector"), sizeof(FVector), alignof(FVector), TEXT("float64 X")},
			FExpected{TEXT("FVector3f"), sizeof(FVector3f), alignof(FVector3f), TEXT("float32 X")},
			FExpected{TEXT("FVector4"), sizeof(FVector4), alignof(FVector4), TEXT("float64 W")},
			FExpected{TEXT("FVector4f"), sizeof(FVector4f), alignof(FVector4f), TEXT("float32 W")}})
		{
			const auto* Type = F.Store->GetType(F.Store->FindType(E.Name)); ASSERT_THAT(IsNotNull(Type));
			ASSERT_THAT(AreEqual(E.Size, Type->Declaration.NativeSize)); ASSERT_THAT(AreEqual(E.Alignment, Type->Declaration.NativeAlignment));
			ASSERT_THAT(IsTrue(Type->Members.ContainsByPredicate([&](const auto& M) { return M.Kind == EAngelscriptTypeBindMemberKind::Property && M.Declaration == E.Property; })));
		}
	}

	TEST_METHOD(NormalizingZeroPreservesZeroContract)
	{
		FFixture F; ASSERT_THAT(IsTrue(F.Record())); FVector Zero = FVector::ZeroVector, Fallback(7.0, 8.0, 9.0); FContext C(F);
		auto* Fn = F.Function(TEXT("FVector"), TEXT("FVector GetSafeNormal(float64 Tolerance = SMALL_NUMBER, const FVector& ResultIfZero = FVector::ZeroVector) const"));
		ASSERT_THAT(IsNotNull(Fn)); ASSERT_THAT(AreEqual(0, C->Prepare(Fn))); ASSERT_THAT(AreEqual(0, C->SetObject(&Zero)));
		ASSERT_THAT(AreEqual(0, C->SetArgDouble(0, SMALL_NUMBER))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(1, &Fallback)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); const auto* Result = static_cast<const FVector*>(C->GetReturnObject());
		ASSERT_THAT(IsNotNull(Result)); ASSERT_THAT(IsTrue(*Result == Fallback)); ASSERT_THAT(IsTrue(Zero.IsZero()));
	}
};
#endif
