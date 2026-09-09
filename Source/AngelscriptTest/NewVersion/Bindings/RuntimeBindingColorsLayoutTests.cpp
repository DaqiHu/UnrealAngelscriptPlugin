#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"
#include "Layout/Geometry.h"
#include "Layout/Margin.h"
#include "Widgets/Layout/Anchors.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingColorsLayoutFixture
{
using namespace RuntimeBindingTestSupport;

static const FName ColorsLayoutProviderNames[] = {
	TEXT("FColor"), TEXT("FLinearColor.Functions"), TEXT("FAnchors"), TEXT("FMargin"), TEXT("FGeometry")
};

struct FColorsLayoutFixture
{
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TUniquePtr<FAngelscriptEngine> Owner;
	FString Diagnostic;

	bool Record(bool bCreateOwner = true)
	{
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return false;
		const auto& Collection = FAngelscriptBind::GetRegisteredCollection();
		if (!FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(Collection, *Store, Diagnostic)
			|| !FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(Collection, ColorsLayoutProviderNames, *Store, Diagnostic)
			|| !Store->Seal(Diagnostic))
		{
			UE_LOG(LogTemp, Error, TEXT("Colors/layout fixture recording failed: %s"), *Diagnostic);
			return false;
		}
		if (!bCreateOwner) return true;
		FAngelscriptBindingDiagnostic BindingDiagnostic;
		Owner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic);
		if (!Owner) { Diagnostic = BindingDiagnostic.Message; UE_LOG(LogTemp, Error, TEXT("Colors/layout fixture creation failed: %s"), *Diagnostic); }
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

struct FColorsLayoutContext
{
	asIScriptContext* Ptr = nullptr;
	explicit FColorsLayoutContext(FColorsLayoutFixture& Fixture) : Ptr(Fixture.Owner->Engine->CreateContext()) {}
	~FColorsLayoutContext() { if (Ptr) Ptr->Release(); }
	asIScriptContext* operator->() const { return Ptr; }
};
}

using namespace RuntimeBindingColorsLayoutFixture;
TEST_CLASS_WITH_FLAGS(ColorsLayout, "Angelscript.UnitTest.RuntimeBindings.Values.ColorsLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CompleteProviderSurfaceAndPropertiesAreRecorded)
	{
		FColorsLayoutFixture F; ASSERT_THAT(IsTrue(F.Record(false)));
		for (const TCHAR* Name : {TEXT("FColor"), TEXT("FLinearColor"), TEXT("FAnchors"), TEXT("FMargin"), TEXT("FGeometry")})
			ASSERT_THAT(IsNotNull(F.Store->GetType(F.Store->FindType(Name))));
		for (const auto& Expected : {TPair<const TCHAR*, const TCHAR*>(TEXT("FColor"), TEXT("uint DWColor")),
			TPair<const TCHAR*, const TCHAR*>(TEXT("FLinearColor"), TEXT("float32 R"))})
		{
			const auto* Type = F.Store->GetType(F.Store->FindType(Expected.Key)); ASSERT_THAT(IsNotNull(Type));
			ASSERT_THAT(IsTrue(Type->Members.ContainsByPredicate([&](const auto& M) { return M.Declaration == Expected.Value; })));
		}
		int32 Count = 0;
		for (const auto& Type : F.Store->GetTypes()) for (const auto& Member : Type.Members)
			for (const FName Provider : ColorsLayoutProviderNames) if (Member.Source.Provider == Provider) { ++Count; break; }
		ASSERT_THAT(AreEqual(101, Count));
	}

	TEST_METHOD(ColorConstructorPreservesOneTwoThreeFourChannels)
	{
		FColorsLayoutFixture F; ASSERT_THAT(IsTrue(F.Record())); FColorsLayoutContext C(F); alignas(FColor) uint8 Storage[sizeof(FColor)] = {};
		auto* Fn = F.Function(TEXT("FColor"), TEXT("void f(uint8 R, uint8 G, uint8 B, uint8 A = 255)")); ASSERT_THAT(IsNotNull(Fn));
		ASSERT_THAT(AreEqual(0, C->Prepare(Fn))); ASSERT_THAT(AreEqual(0, C->SetObject(Storage)));
		ASSERT_THAT(AreEqual(0, C->SetArgByte(0, 1))); ASSERT_THAT(AreEqual(0, C->SetArgByte(1, 2)));
		ASSERT_THAT(AreEqual(0, C->SetArgByte(2, 3))); ASSERT_THAT(AreEqual(0, C->SetArgByte(3, 4)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); const auto& Color = *reinterpret_cast<const FColor*>(Storage);
		ASSERT_THAT(IsTrue(Color.R == 1 && Color.G == 2 && Color.B == 3 && Color.A == 4));
	}

	TEST_METHOD(LinearBlackAndWhiteConvertToPackedEndpoints)
	{
		FColorsLayoutFixture F; ASSERT_THAT(IsTrue(F.Record())); FColorsLayoutContext C(F);
		auto* Fn = F.Function(TEXT("FLinearColor"), TEXT("FColor ToFColor(bool bSRGB) const")); ASSERT_THAT(IsNotNull(Fn));
		for (const auto& Pair : {TPair<FLinearColor, FColor>(FLinearColor::Black, FColor::Black), TPair<FLinearColor, FColor>(FLinearColor::White, FColor::White)})
		{
			FLinearColor Input = Pair.Key; ASSERT_THAT(AreEqual(0, C->Prepare(Fn))); ASSERT_THAT(AreEqual(0, C->SetObject(&Input)));
			ASSERT_THAT(AreEqual(0, C->SetArgByte(0, 1))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
			const auto* Output = static_cast<const FColor*>(C->GetReturnObject()); ASSERT_THAT(IsNotNull(Output)); ASSERT_THAT(IsTrue(*Output == Pair.Value));
		}
	}

	TEST_METHOD(MarginOneTwoThreeFourSumsToTen)
	{
		FColorsLayoutFixture F; ASSERT_THAT(IsTrue(F.Record())); FColorsLayoutContext C(F); alignas(FMargin) uint8 Storage[sizeof(FMargin)] = {};
		auto* Ctor = F.Function(TEXT("FMargin"), TEXT("void f(float32 InLeft, float32 InTop, float32 InRight, float32 InBottom)")); ASSERT_THAT(IsNotNull(Ctor));
		ASSERT_THAT(AreEqual(0, C->Prepare(Ctor))); ASSERT_THAT(AreEqual(0, C->SetObject(Storage)));
		for (int32 Index = 0; Index != 4; ++Index) ASSERT_THAT(AreEqual(0, C->SetArgFloat(Index, float(Index + 1))));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); auto* Horizontal = F.Function(TEXT("FMargin"), TEXT("float32 GetTotalSpaceAlongHorizontal() const")); auto* Vertical = F.Function(TEXT("FMargin"), TEXT("float32 GetTotalSpaceAlongVertical() const"));
		ASSERT_THAT(IsNotNull(Horizontal)); ASSERT_THAT(IsNotNull(Vertical)); float Total = 0.f;
		for (auto* Fn : {Horizontal, Vertical}) { ASSERT_THAT(AreEqual(0, C->Prepare(Fn))); ASSERT_THAT(AreEqual(0, C->SetObject(Storage))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); Total += C->GetReturnFloat(); }
		ASSERT_THAT(IsTrue(Total == 10.f));
	}

	TEST_METHOD(AnchorsRangePreservesMinimumAndMaximum)
	{
		FColorsLayoutFixture F; ASSERT_THAT(IsTrue(F.Record())); FColorsLayoutContext C(F); alignas(FAnchors) uint8 Storage[sizeof(FAnchors)] = {};
		auto* Fn = F.Function(TEXT("FAnchors"), TEXT("void f(float32 MinX, float32 MinY, float32 MaxX, float32 MaxY)")); ASSERT_THAT(IsNotNull(Fn));
		ASSERT_THAT(AreEqual(0, C->Prepare(Fn))); ASSERT_THAT(AreEqual(0, C->SetObject(Storage)));
		ASSERT_THAT(AreEqual(0, C->SetArgFloat(0, .1f))); ASSERT_THAT(AreEqual(0, C->SetArgFloat(1, .2f))); ASSERT_THAT(AreEqual(0, C->SetArgFloat(2, .8f))); ASSERT_THAT(AreEqual(0, C->SetArgFloat(3, .9f)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); const auto& Anchors = *reinterpret_cast<const FAnchors*>(Storage);
		ASSERT_THAT(IsTrue(FVector2D(Anchors.Minimum).Equals(FVector2D(.1, .2), 1.e-6)
			&& FVector2D(Anchors.Maximum).Equals(FVector2D(.8, .9), 1.e-6)));
	}

	TEST_METHOD(IdentityGeometryLocalToAbsolutePreservesPoint)
	{
		FColorsLayoutFixture F; ASSERT_THAT(IsTrue(F.Record())); FColorsLayoutContext C(F);
		FGeometry Geometry = FGeometry::MakeRoot(FVector2f(100.f, 100.f), FSlateLayoutTransform()); FVector2D Point(7.0, 9.0);
		auto* Fn = F.Function(TEXT("FGeometry"), TEXT("FVector2D LocalToAbsolute(const FVector2D& Position) const")); ASSERT_THAT(IsNotNull(Fn));
		ASSERT_THAT(AreEqual(0, C->Prepare(Fn))); ASSERT_THAT(AreEqual(0, C->SetObject(&Geometry))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Point)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); const auto* Output = static_cast<const FVector2D*>(C->GetReturnObject()); ASSERT_THAT(IsNotNull(Output)); ASSERT_THAT(IsTrue(Output->Equals(Point, 0.0)));
	}
};
#endif
