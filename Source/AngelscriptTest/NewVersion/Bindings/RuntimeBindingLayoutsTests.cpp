#include "CQTest.h"
#include "Core/AngelscriptTypeBindInfoDraft.h"
#include "as_objecttype.h"
#include "as_typeinfo.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
BEGIN_AS_NAMESPACE
namespace BindingLayoutTests
{
TEST_CLASS_WITH_FLAGS(Layouts, "Angelscript.UnitTest.RuntimeBindings.Types",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	struct FPair { int32 X; int32 Y; };
	struct FHolder { int32 Tag; FPair Pair; };
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TUniquePtr<FAngelscriptTypeBindInfoDraft> Draft;
	FString Diagnostic;
	FAngelscriptTypeBindDeclaration Declaration(const TCHAR* Name, uint32 Size, uint32 Alignment,
		EAngelscriptTypeBindKind Kind = EAngelscriptTypeBindKind::Value)
	{
		FAngelscriptTypeBindDeclaration D;
		D.Name = Name; D.Namespace = TEXT("Fixture"); D.Kind = Kind; D.NativeSize = Size; D.NativeAlignment = Alignment;
		D.ObjectFlags = Kind == EAngelscriptTypeBindKind::ObjectHandle || Kind == EAngelscriptTypeBindKind::Interface
			? asOBJ_REF | asOBJ_NOCOUNT : asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS;
		D.Source = {TEXT("FixtureModule"), TEXT("LayoutProvider"), TEXT("Fixture/Layouts.cpp"), 29};
		return D;
	}
	FAngelscriptTypeBindHandle Add(const FAngelscriptTypeBindDeclaration& D)
	{
		FAngelscriptTypeBindHandle Handle;
		check(Store->RecordType(D, Handle, Diagnostic)); return Handle;
	}
	void Property(FAngelscriptTypeBindHandle Owner, const TCHAR* Declaration, int32 Offset)
	{
		FAngelscriptTypeBindInfoMember Member;
		Member.Kind = EAngelscriptTypeBindMemberKind::Property;
		Member.Declaration = Declaration; Member.PropertyOffset = Offset;
		Member.Source = {TEXT("FixtureModule"), TEXT("MemberProvider"), TEXT("Fixture/Members.cpp"), 47};
		FAngelscriptTypeBindMemberHandle Handle;
		ASSERT_THAT(IsTrue(Store->RecordMember(Owner, Member, Handle, Diagnostic)));
	}
	bool Build() { return Store->Seal(Diagnostic) && FAngelscriptTypeBindInfoDraft::Create(Store, Draft, Diagnostic); }
	TEST_METHOD(DerivedBeforeBaseAndForwardValuePropertyKeepNativeLayout)
	{
		auto Derived = Declaration(TEXT("Derived"), 8, 4); Derived.BaseType = TEXT("Base"); Add(Derived);
		const auto Holder = Add(Declaration(TEXT("Holder"), sizeof(FHolder), alignof(FHolder)));
		Property(Holder, TEXT("Pair Value"), STRUCT_OFFSET(FHolder, Pair));
		const auto Pair = Add(Declaration(TEXT("Pair"), sizeof(FPair), alignof(FPair)));
		Property(Pair, TEXT("int X"), STRUCT_OFFSET(FPair, X));
		Add(Declaration(TEXT("Base"), 4, 4));
		ASSERT_THAT(IsTrue(Build()));
		auto* DerivedType = static_cast<asCObjectType*>(Draft->FindType(TEXT("Derived"), TEXT("Fixture")));
		ASSERT_THAT(IsNotNull(DerivedType));
		ASSERT_THAT(IsTrue(DerivedType->derivedFrom == Draft->FindType(TEXT("Base"), TEXT("Fixture"))));
		auto* HolderType = static_cast<asCObjectType*>(Draft->FindType(TEXT("Holder"), TEXT("Fixture")));
		ASSERT_THAT(IsNotNull(HolderType));
		ASSERT_THAT(AreEqual(int32(sizeof(FHolder)), HolderType->size));
		ASSERT_THAT(AreEqual(int32(alignof(FHolder)), HolderType->alignment));
		auto* Field = HolderType->GetFirstProperty("Value"); ASSERT_THAT(IsNotNull(Field));
		ASSERT_THAT(AreEqual(int32(STRUCT_OFFSET(FHolder, Pair)), Field->byteOffset));
		ASSERT_THAT(IsTrue(Field->type.GetTypeInfo() == Draft->FindType(TEXT("Pair"), TEXT("Fixture"))));
		ASSERT_THAT(IsFalse(Draft->GetSet()->IsFrozen()));
		ASSERT_THAT(IsNull(Draft->GetSet()->GetBoundEngine()));
	}
	TEST_METHOD(MutualObjectHandleReferencesDoNotFormALayoutCycle)
	{
		const auto A = Add(Declaration(TEXT("A"), sizeof(void*), alignof(void*), EAngelscriptTypeBindKind::ObjectHandle));
		const auto B = Add(Declaration(TEXT("B"), sizeof(void*), alignof(void*), EAngelscriptTypeBindKind::ObjectHandle));
		Property(A, TEXT("B@ Other"), 0); Property(B, TEXT("A@ Other"), 0);
		ASSERT_THAT(IsTrue(Build()));
		auto* AType = static_cast<asCObjectType*>(Draft->FindType(TEXT("A"), TEXT("Fixture")));
		ASSERT_THAT(IsNotNull(AType));
		ASSERT_THAT(IsTrue(AType->GetFirstProperty("Other")->type.IsObjectHandle()));
	}
	TEST_METHOD(MissingNominalReportsTheMemberProviderAndDeclaration)
	{
		Property(Add(Declaration(TEXT("Holder"), 8, 4)), TEXT("MissingType Value"), 0);
		ASSERT_THAT(IsFalse(Build())); ASSERT_THAT(IsFalse(Draft.IsValid()));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("MissingType Value"))));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("MemberProvider"))));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("Fixture/Members.cpp:47"))));
	}
	TEST_METHOD(BaseCycleReportsItsDeclarationsAndPublishesNothing)
	{
		auto A = Declaration(TEXT("BaseA"), 8, 4); A.BaseType = TEXT("BaseB"); Add(A);
		auto B = Declaration(TEXT("BaseB"), 8, 4); B.BaseType = TEXT("BaseA"); Add(B);
		ASSERT_THAT(IsFalse(Build())); ASSERT_THAT(IsFalse(Draft.IsValid()));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("BaseA")))); ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("BaseB"))));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("LayoutProvider"))));
	}
	TEST_METHOD(ByValueCycleIsRejectedEvenWithExplicitNativeSizes)
	{
		const auto A = Add(Declaration(TEXT("A"), 8, 4)); const auto B = Add(Declaration(TEXT("B"), 8, 4));
		Property(A, TEXT("B Other"), 0); Property(B, TEXT("A Other"), 0);
		ASSERT_THAT(IsFalse(Build())); ASSERT_THAT(IsFalse(Draft.IsValid()));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("cycle")))); ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("MemberProvider"))));
	}
	TEST_METHOD(InvalidAlignmentAndOutOfBoundsOffsetFailWithSource)
	{
		Add(Declaration(TEXT("BadAlignment"), 8, 3));
		ASSERT_THAT(IsFalse(Build())); ASSERT_THAT(IsFalse(Draft.IsValid()));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("BadAlignment"))));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("Fixture/Layouts.cpp:29"))));
		Store = MakeShared<FAngelscriptTypeBindInfoStore>();
		Property(Add(Declaration(TEXT("Pair"), 8, 4)), TEXT("int BeyondEnd"), 8);
		ASSERT_THAT(IsFalse(Build())); ASSERT_THAT(IsFalse(Draft.IsValid()));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("BeyondEnd"))));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("Fixture/Members.cpp:47"))));
	}
	TEST_METHOD(EnumAliasAndInterfaceFactsRoundTrip)
	{
		auto Alias = Declaration(TEXT("Count"), 0, 0, EAngelscriptTypeBindKind::Alias); Alias.AliasTarget = TEXT("int"); Add(Alias);
		auto Object = Declaration(TEXT("Object"), 8, 8, EAngelscriptTypeBindKind::ObjectHandle); Object.Interfaces.Add(TEXT("Readable")); Add(Object);
		Add(Declaration(TEXT("Readable"), 0, 0, EAngelscriptTypeBindKind::Interface));
		const auto Mode = Add(Declaration(TEXT("Mode"), 4, 4, EAngelscriptTypeBindKind::Enum));
		FAngelscriptTypeBindInfoMember Value; Value.Kind = EAngelscriptTypeBindMemberKind::EnumValue; Value.Declaration = TEXT("On"); Value.EnumValue = 7;
		FAngelscriptTypeBindMemberHandle Handle; ASSERT_THAT(IsTrue(Store->RecordMember(Mode, Value, Handle, Diagnostic)));
		ASSERT_THAT(IsTrue(Build()));
		auto* Count = static_cast<asCTypedefType*>(Draft->FindType(TEXT("Count"), TEXT("Fixture"))); ASSERT_THAT(IsNotNull(Count));
		ASSERT_THAT(IsTrue(Count->aliasForType.GetTokenType() == ttInt));
		ASSERT_THAT(IsTrue(Count->GetStableKey() == Draft->FindNominal(TEXT("::Fixture::Count"), {})));
		auto* Enum = static_cast<asCEnumType*>(Draft->FindType(TEXT("Mode"), TEXT("Fixture"))); ASSERT_THAT(IsNotNull(Enum));
		ASSERT_THAT(AreEqual(asUINT(1), Enum->enumValues.GetLength())); ASSERT_THAT(AreEqual(7, Enum->enumValues[0]->value));
		auto* ObjectType = static_cast<asCObjectType*>(Draft->FindType(TEXT("Object"), TEXT("Fixture")));
		ASSERT_THAT(AreEqual(asUINT(1), ObjectType->interfaces.GetLength()));
		ASSERT_THAT(IsTrue(ObjectType->interfaces[0]->IsInterface()));
	}
	TEST_METHOD(UnsealedStoreCannotStartADraft)
	{
		ASSERT_THAT(IsFalse(FAngelscriptTypeBindInfoDraft::Create(Store, Draft, Diagnostic)));
		ASSERT_THAT(IsFalse(Draft.IsValid())); ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("sealed"))));
	}
	TEST_METHOD(CaseSensitiveNominalsKeepIndependentNativeLayouts)
	{
		Add(Declaration(TEXT("Pair"), 4, 4)); Add(Declaration(TEXT("pair"), 8, 4));
		auto Other = Declaration(TEXT("Pair"), 16, 8); Other.Namespace = TEXT("Other"); Add(Other);
		ASSERT_THAT(IsTrue(Build()));
		auto* Upper = Draft->FindType(TEXT("Pair"), TEXT("Fixture"));
		auto* Lower = Draft->FindType(TEXT("pair"), TEXT("Fixture"));
		auto* Qualified = Draft->FindType(TEXT("::Other::Pair"));
		ASSERT_THAT(IsNotNull(Upper)); ASSERT_THAT(IsNotNull(Lower)); ASSERT_THAT(IsNotNull(Qualified));
		ASSERT_THAT(IsTrue(Upper != Lower && Upper != Qualified && Lower != Qualified));
		ASSERT_THAT(AreEqual(4, Upper->size)); ASSERT_THAT(AreEqual(8, Lower->size)); ASSERT_THAT(AreEqual(16, Qualified->size));
	}
};
}
END_AS_NAMESPACE
#endif
