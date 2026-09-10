#include "CQTest.h"
#include "TypeOwnershipTestSupport.h"
#include "Core/AngelscriptTypeBindInfoApply.h"
#include "Core/AngelscriptTypeBindInfoStore.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(Shared, "Angelscript.UnitTest.NativeEngine.TypeOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FPairSetup
	{
		TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
		FAngelscriptTypeBindHandle Type;
		FAngelscriptTypeBindMemberHandle Sum;
		int TypeId = -1;
		int FunctionId = -1;
	};

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
		Result.Source.Provider = TEXT("TypeOwnership.Shared");
		Result.Source.File = TEXT("TypeOwnershipSharedPublicationTests.cpp");
		Result.Source.Line = 1;
		return Result;
	}

	static bool RecordAndPublish(FPairSetup& Out, FString& Diagnostic)
	{
		if (!Out.Store->RecordType(PairDeclaration(), Out.Type, Diagnostic)) return false;
		FAngelscriptTypeBindInfoMember Property;
		Property.Kind = EAngelscriptTypeBindMemberKind::Property;
		Property.Declaration = TEXT("int X");
		Property.PropertyOffset = STRUCT_OFFSET(FTypeOwnershipPair, X);
		Property.Source = PairDeclaration().Source;
		FAngelscriptTypeBindMemberHandle Handle;
		if (!Out.Store->RecordMember(Out.Type, Property, Handle, Diagnostic)) return false;
		Property.Declaration = TEXT("int Y");
		Property.PropertyOffset = STRUCT_OFFSET(FTypeOwnershipPair, Y);
		if (!Out.Store->RecordMember(Out.Type, Property, Handle, Diagnostic)) return false;
		FAngelscriptTypeBindInfoMember Sum;
		Sum.Kind = EAngelscriptTypeBindMemberKind::Method;
		Sum.Declaration = TEXT("int Sum() const");
		Sum.Source = PairDeclaration().Source;
		if (!Out.Store->RecordMember(Out.Type, Sum, Out.Sum, Diagnostic) || !Out.Store->Seal(Diagnostic)) return false;
		if (Out.Store->PublishProcessIds() != asEMetadataRegistrationResult::Succeeded) return false;
		Out.TypeId = Out.Store->GetPublishedTypeId(Out.Type);
		Out.FunctionId = Out.Store->GetPublishedFunctionId(Out.Sum);
		return Out.TypeId > asTYPEID_LAST_PRIMITIVE && Out.FunctionId > 0;
	}

	static int32 ExecuteSum(asCScriptEngine& Engine, asCScriptFunction* Sum, FTypeOwnershipPair& Host)
	{
		if (!Sum) return MIN_int32;
		if (Engine.BindNativeFunction(Sum->GetStableKey(), asFUNCTION(TypeOwnershipTest::GenericPairSum), asCALL_GENERIC)
			!= asEByteCodeLinkStatus::Succeeded)
		{
			return MIN_int32;
		}
		asIScriptContext* Context = Engine.CreateContext();
		if (!Context) return MIN_int32;
		const int32 Result = (Context->Prepare(Sum) == 0 && Context->SetObject(&Host) == 0
			&& Context->Execute() == asEXECUTION_FINISHED)
			? static_cast<int32>(Context->GetReturnDWord())
			: MIN_int32;
		Context->Release();
		return Result;
	}

public:
	TEST_METHOD(DistinctPointers)
	{
		FPairSetup Setup;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(RecordAndPublish(Setup, Diagnostic), *Diagnostic));
		FNativeEngineOwner EngineA;
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded,
			FAngelscriptTypeBindInfoApply::MaterializePublishedTypes(Setup.Store, *EngineA.Ptr)));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded,
			FAngelscriptTypeBindInfoApply::MaterializePublishedTypes(Setup.Store, *EngineB.Ptr)));
		asITypeInfo* TypeA = EngineA.Ptr->GetTypeInfoById(Setup.TypeId);
		asITypeInfo* TypeB = EngineB.Ptr->GetTypeInfoById(Setup.TypeId);
		ASSERT_THAT(IsNotNull(TypeA));
		ASSERT_THAT(IsNotNull(TypeB));
		ASSERT_THAT(IsTrue(TypeA != TypeB));
		ASSERT_THAT(AreEqual(Setup.TypeId, TypeA->GetTypeId()));
		ASSERT_THAT(AreEqual(Setup.TypeId, TypeB->GetTypeId()));
		ASSERT_THAT(IsTrue(TypeA->GetEngine() == EngineA.Ptr));
		ASSERT_THAT(IsTrue(TypeB->GetEngine() == EngineB.Ptr));
		ASSERT_THAT(IsTrue(EngineA.Ptr->GetTypeInfoById(Setup.TypeId) != TypeB));
		ASSERT_THAT(IsTrue(EngineB.Ptr->GetTypeInfoById(Setup.TypeId) != TypeA));
	}

	TEST_METHOD(IndependentAuxiliary)
	{
		using namespace TypeOwnershipTest;
		FPairSetup Setup;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(RecordAndPublish(Setup, Diagnostic), *Diagnostic));
		FNativeEngineOwner EngineA;
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded,
			FAngelscriptTypeBindInfoApply::MaterializePublishedTypes(Setup.Store, *EngineA.Ptr)));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded,
			FAngelscriptTypeBindInfoApply::MaterializePublishedTypes(Setup.Store, *EngineB.Ptr)));
		asCScriptFunction* SumA = static_cast<asCScriptFunction*>(EngineA.Ptr->GetFunctionById(Setup.FunctionId));
		asCScriptFunction* SumB = static_cast<asCScriptFunction*>(EngineB.Ptr->GetFunctionById(Setup.FunctionId));
		ASSERT_THAT(IsNotNull(SumA));
		ASSERT_THAT(IsNotNull(SumB));
		ASSERT_THAT(IsTrue(SumA != SumB));
		FTypeOwnershipPair HostA;
		HostA.X = PairX;
		HostA.Y = PairY;
		FTypeOwnershipPair HostB;
		HostB.X = 40;
		HostB.Y = 59;
		ASSERT_THAT(AreEqual(PairSum, ExecuteSum(*EngineA.Ptr, SumA, HostA)));
		ASSERT_THAT(AreEqual(99, ExecuteSum(*EngineB.Ptr, SumB, HostB)));
	}

	TEST_METHOD(DestroyALeavesB)
	{
		using namespace TypeOwnershipTest;
		FPairSetup Setup;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(RecordAndPublish(Setup, Diagnostic), *Diagnostic));
		FNativeEngineOwner EngineA;
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded,
			FAngelscriptTypeBindInfoApply::MaterializePublishedTypes(Setup.Store, *EngineA.Ptr)));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded,
			FAngelscriptTypeBindInfoApply::MaterializePublishedTypes(Setup.Store, *EngineB.Ptr)));
		asITypeInfo* TypeB = EngineB.Ptr->GetTypeInfoById(Setup.TypeId);
		asCScriptFunction* SumB = static_cast<asCScriptFunction*>(EngineB.Ptr->GetFunctionById(Setup.FunctionId));
		ASSERT_THAT(IsNotNull(TypeB));
		ASSERT_THAT(IsNotNull(SumB));
		EngineA.Reset();
		ASSERT_THAT(IsTrue(EngineB.Ptr->GetTypeInfoById(Setup.TypeId) == TypeB));
		ASSERT_THAT(AreEqual(Setup.TypeId, TypeB->GetTypeId()));
		FTypeOwnershipPair Host;
		Host.X = PairX;
		Host.Y = PairY;
		ASSERT_THAT(AreEqual(PairSum, ExecuteSum(*EngineB.Ptr, SumB, Host)));
	}
};

#endif
