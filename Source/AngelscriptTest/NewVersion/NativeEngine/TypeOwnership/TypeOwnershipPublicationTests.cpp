#include "CQTest.h"
#include "TypeOwnershipTestSupport.h"
#include "Core/AngelscriptTypeBindInfoStore.h"
#include "as_type_id_registry.h"
#include "as_scriptengine.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(Publish, "Angelscript.UnitTest.NativeEngine.TypeOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FAngelscriptTypeBindDeclaration PairDeclaration(uint32 Alignment = alignof(FTypeOwnershipPair))
	{
		FAngelscriptTypeBindDeclaration Result;
		Result.Name = TEXT("Pair");
		Result.Namespace = TEXT("Fixture");
		Result.Kind = EAngelscriptTypeBindKind::Value;
		Result.NativeSize = sizeof(FTypeOwnershipPair);
		Result.NativeAlignment = Alignment;
		Result.ObjectFlags = asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS;
		Result.Source.OwnerModule = TEXT("AngelscriptTest");
		Result.Source.Provider = TEXT("TypeOwnership.Publish");
		Result.Source.File = TEXT("TypeOwnershipPublicationTests.cpp");
		Result.Source.Line = 1;
		return Result;
	}

	static bool RecordPair(
		FAngelscriptTypeBindInfoStore& Store,
		FAngelscriptTypeBindHandle& OutType,
		FAngelscriptTypeBindMemberHandle& OutSum,
		FString& Diagnostic,
		uint32 Alignment = alignof(FTypeOwnershipPair))
	{
		if (!Store.RecordType(PairDeclaration(Alignment), OutType, Diagnostic)) return false;
		FAngelscriptTypeBindInfoMember X;
		X.Kind = EAngelscriptTypeBindMemberKind::Property;
		X.Declaration = TEXT("int X");
		X.PropertyOffset = STRUCT_OFFSET(FTypeOwnershipPair, X);
		X.Source = PairDeclaration().Source;
		FAngelscriptTypeBindMemberHandle Property;
		if (!Store.RecordMember(OutType, X, Property, Diagnostic)) return false;
		FAngelscriptTypeBindInfoMember Y = X;
		Y.Declaration = TEXT("int Y");
		Y.PropertyOffset = STRUCT_OFFSET(FTypeOwnershipPair, Y);
		if (!Store.RecordMember(OutType, Y, Property, Diagnostic)) return false;
		FAngelscriptTypeBindInfoMember Sum;
		Sum.Kind = EAngelscriptTypeBindMemberKind::Method;
		Sum.Declaration = TEXT("int Sum() const");
		Sum.Source = PairDeclaration().Source;
		return Store.RecordMember(OutType, Sum, OutSum, Diagnostic) && Store.Seal(Diagnostic);
	}

public:
	TEST_METHOD(PairId)
	{
		FAngelscriptTypeBindInfoStore Store;
		FAngelscriptTypeBindHandle Type;
		FAngelscriptTypeBindMemberHandle Sum;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(RecordPair(Store, Type, Sum, Diagnostic), *Diagnostic));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Store.PublishProcessIds()));
		const int TypeId = Store.GetPublishedTypeId(Type);
		const int FunctionId = Store.GetPublishedFunctionId(Sum);
		ASSERT_THAT(IsTrue(TypeId > asTYPEID_LAST_PRIMITIVE));
		ASSERT_THAT(IsTrue(FunctionId > 0));
		FNativeEngineOwner Engine;
		ASSERT_THAT(IsNull(Engine.Ptr->GetTypeInfoById(TypeId)));
		ASSERT_THAT(IsNull(Engine.Ptr->GetFunctionById(FunctionId)));
	}

	TEST_METHOD(Invalid)
	{
		asCTypeIdAllocator& Allocator = asCTypeIdRegistry::Get().Allocator();
		const int BeforeType = Allocator.PeekNextTypeSequence();
		const int BeforeFunction = Allocator.PeekNextFunctionId();
		FAngelscriptTypeBindInfoStore Store;
		FAngelscriptTypeBindHandle Type;
		FAngelscriptTypeBindMemberHandle Sum;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(RecordPair(Store, Type, Sum, Diagnostic, 3), *Diagnostic));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::InvalidLayout, Store.PublishProcessIds()));
		ASSERT_THAT(AreEqual(-1, Store.GetPublishedTypeId(Type)));
		ASSERT_THAT(AreEqual(-1, Store.GetPublishedFunctionId(Sum)));
		ASSERT_THAT(AreEqual(BeforeType, Allocator.PeekNextTypeSequence()));
		ASSERT_THAT(AreEqual(BeforeFunction, Allocator.PeekNextFunctionId()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(IsNull(Engine.Ptr->GetTypeInfoById(BeforeType)));
	}

	TEST_METHOD(NoReuse)
	{
		FAngelscriptTypeBindInfoStore First;
		FAngelscriptTypeBindInfoStore Second;
		FAngelscriptTypeBindHandle FirstType;
		FAngelscriptTypeBindHandle SecondType;
		FAngelscriptTypeBindMemberHandle FirstSum;
		FAngelscriptTypeBindMemberHandle SecondSum;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(RecordPair(First, FirstType, FirstSum, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(RecordPair(Second, SecondType, SecondSum, Diagnostic), *Diagnostic));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, First.PublishProcessIds()));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Second.PublishProcessIds()));
		const int FirstId = First.GetPublishedTypeId(FirstType);
		const int SecondId = Second.GetPublishedTypeId(SecondType);
		ASSERT_THAT(IsTrue(FirstId > asTYPEID_LAST_PRIMITIVE));
		ASSERT_THAT(IsTrue(SecondId > asTYPEID_LAST_PRIMITIVE));
		ASSERT_THAT(IsTrue(FirstId != SecondId));
		ASSERT_THAT(IsTrue(First.GetPublishedFunctionId(FirstSum) != Second.GetPublishedFunctionId(SecondSum)));
		FNativeEngineOwner Engine;
		ASSERT_THAT(IsNull(Engine.Ptr->GetTypeInfoById(FirstId)));
		ASSERT_THAT(IsNull(Engine.Ptr->GetTypeInfoById(SecondId)));
	}
};

#endif
