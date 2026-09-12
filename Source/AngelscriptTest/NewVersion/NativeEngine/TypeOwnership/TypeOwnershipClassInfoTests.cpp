#include "CQTest.h"
#include "TypeOwnershipTestSupport.h"
#include "Core/AngelscriptTypeBindInfoStore.h"
#include "as_module_definition_set.h"
#include "as_scriptengine.h"
#include "as_type_id_registry.h"
#include "as_typeinfo.h"

#include <type_traits>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(ClassInfo, "Angelscript.UnitTest.NativeEngine.TypeOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FAngelscriptTypeBindDeclaration PairDeclaration()
	{
		FAngelscriptTypeBindDeclaration Result;
		Result.Name = TEXT("Pair");
		Result.Namespace = TEXT("Fixture");
		Result.Kind = EAngelscriptTypeBindKind::Value;
		Result.NativeSize = sizeof(FTypeOwnershipPair);
		Result.NativeAlignment = alignof(FTypeOwnershipPair);
		Result.ObjectFlags = asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS;
		Result.Source.OwnerModule = TEXT("AngelscriptTest");
		Result.Source.Provider = TEXT("TypeOwnership.ClassInfo");
		Result.Source.File = TEXT("TypeOwnershipClassInfoTests.cpp");
		Result.Source.Line = 1;
		return Result;
	}

	static bool RecordAndPublishPair(FAngelscriptTypeBindInfoStore& Store, int& OutTypeId, FString& Diagnostic)
	{
		FAngelscriptTypeBindHandle Type;
		if (!Store.RecordType(PairDeclaration(), Type, Diagnostic)) return false;
		FAngelscriptTypeBindInfoMember Property;
		Property.Kind = EAngelscriptTypeBindMemberKind::Property;
		Property.Declaration = TEXT("int X");
		Property.PropertyOffset = STRUCT_OFFSET(FTypeOwnershipPair, X);
		Property.Source = PairDeclaration().Source;
		FAngelscriptTypeBindMemberHandle Handle;
		if (!Store.RecordMember(Type, Property, Handle, Diagnostic)) return false;
		Property.Declaration = TEXT("int Y");
		Property.PropertyOffset = STRUCT_OFFSET(FTypeOwnershipPair, Y);
		if (!Store.RecordMember(Type, Property, Handle, Diagnostic)) return false;
		FAngelscriptTypeBindInfoMember Sum;
		Sum.Kind = EAngelscriptTypeBindMemberKind::Method;
		Sum.Declaration = TEXT("int Sum() const");
		Sum.Source = PairDeclaration().Source;
		if (!Store.RecordMember(Type, Sum, Handle, Diagnostic) || !Store.Seal(Diagnostic)) return false;
		if (Store.PublishProcessIds() != asEMetadataRegistrationResult::Succeeded) return false;
		OutTypeId = Store.GetPublishedTypeId(Type);
		return OutTypeId > asTYPEID_LAST_PRIMITIVE;
	}

	static const FAngelscriptPublishedMemberInfo* FindMember(
		const FAngelscriptPublishedClassInfo& Info, const TCHAR* Name)
	{
		return Info.Members.FindByPredicate([Name](const FAngelscriptPublishedMemberInfo& Member)
		{
			return Member.Name == Name;
		});
	}

public:
	TEST_METHOD(PairMembers)
	{
		static_assert(!std::is_convertible_v<FAngelscriptPublishedClassInfo, asCTypeInfo*>);
		FAngelscriptTypeBindInfoStore Store;
		int TypeId = -1;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(RecordAndPublishPair(Store, TypeId, Diagnostic), *Diagnostic));
		TUniquePtr<asCModuleDefinitionSet> Set = asCModuleDefinitionSet::Create();
		ASSERT_THAT(IsNotNull(Set.Get()));
		Set.Reset();
		FAngelscriptPublishedClassInfo Info;
		ASSERT_THAT(AreEqual(asTYPE_QUERY_SUCCEEDED, Store.GetPublishedClassInfo(TypeId, Info)));
		ASSERT_THAT(AreEqual(FString(TEXT("Pair")), Info.Name));
		ASSERT_THAT(AreEqual(FString(TEXT("Fixture")), Info.Namespace));
		ASSERT_THAT(AreEqual(TypeId, Info.TypeId));
		const FAngelscriptPublishedMemberInfo* X = FindMember(Info, TEXT("X"));
		const FAngelscriptPublishedMemberInfo* Y = FindMember(Info, TEXT("Y"));
		const FAngelscriptPublishedMemberInfo* Sum = FindMember(Info, TEXT("Sum"));
		ASSERT_THAT(IsNotNull(X));
		ASSERT_THAT(IsNotNull(Y));
		ASSERT_THAT(IsNotNull(Sum));
		ASSERT_THAT(AreEqual(SIZE_T(STRUCT_OFFSET(FTypeOwnershipPair, X)), X->PropertyOffset));
		ASSERT_THAT(AreEqual(SIZE_T(STRUCT_OFFSET(FTypeOwnershipPair, Y)), Y->PropertyOffset));
		ASSERT_THAT(AreEqual(FString(TEXT("int")), X->TypeName));
		ASSERT_THAT(AreEqual(FString(TEXT("int")), Y->TypeName));
		ASSERT_THAT(AreEqual(FString(TEXT("int Sum() const")), Sum->Declaration));
		ASSERT_THAT(IsTrue(Sum->PublishedFunctionId > 0));
	}

	TEST_METHOD(NoTypeInfoPointer)
	{
		FAngelscriptTypeBindInfoStore Store;
		int TypeId = -1;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(RecordAndPublishPair(Store, TypeId, Diagnostic), *Diagnostic));
		TUniquePtr<asCModuleDefinitionSet> Set = asCModuleDefinitionSet::Create();
		Set.Reset();
		FAngelscriptPublishedClassInfo Info;
		ASSERT_THAT(AreEqual(asTYPE_QUERY_SUCCEEDED, Store.GetPublishedClassInfo(TypeId, Info)));
		ASSERT_THAT(AreEqual(TypeId, Info.TypeId));
		FNativeEngineOwner Engine;
		ASSERT_THAT(IsNull(Engine.Ptr->GetTypeInfoById(Info.TypeId)));
		ASSERT_THAT(IsNull(Engine.Ptr->GetTypeInfoByDecl("Pair")));
		ASSERT_THAT(IsNull(Engine.Ptr->GetTypeInfoByDecl("Fixture::Pair")));
	}
};

#endif
