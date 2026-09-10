#include "CQTest.h"
#include "TypeOwnershipTestSupport.h"
#include "Core/AngelscriptTypeBindInfoApply.h"
#include "Core/AngelscriptTypeBindInfoStore.h"
#include "as_context.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(Admission, "Angelscript.UnitTest.NativeEngine.TypeOwnership",
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
		Result.Source.Provider = TEXT("TypeOwnership.Admission");
		Result.Source.File = TEXT("TypeOwnershipAdmissionTests.cpp");
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

public:
	TEST_METHOD(ForeignTypeInfoRejected)
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
		asCScriptFunction* SumB = static_cast<asCScriptFunction*>(EngineB.Ptr->GetFunctionById(Setup.FunctionId));
		ASSERT_THAT(IsNotNull(SumB));
		ASSERT_THAT(AreEqual(Setup.FunctionId, SumB->GetId()));
		asIScriptContext* ContextA = EngineA.Ptr->CreateContext();
		ASSERT_THAT(IsNotNull(ContextA));
		ASSERT_THAT(AreNotEqual(0, ContextA->Prepare(SumB)));
		ContextA->Release();
		ASSERT_THAT(IsTrue(EngineA.Ptr->GetTypeInfoById(Setup.TypeId) != EngineB.Ptr->GetTypeInfoById(Setup.TypeId)));
	}

	TEST_METHOD(NoImageBoundEngine)
	{
		FPairSetup Setup;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(RecordAndPublish(Setup, Diagnostic), *Diagnostic));
		TUniquePtr<FAngelscriptTypeBindInfoInstallation> Prepared;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoApply::Prepare(Setup.Store, Prepared, Diagnostic), *Diagnostic));
		TUniquePtr<asCMetadataImage> Templates = Prepared->TakeTemplateImage();
		TUniquePtr<asCMetadataImage> Image = Prepared->TakeImage();
		FNativeEngineOwner Engine;
		if (Templates)
		{
			const auto TemplateStatus = Engine.Ptr->RegisterMetadataImage(MoveTemp(Templates));
			ASSERT_THAT(IsTrue(TemplateStatus == asEMetadataRegistrationResult::Succeeded
				|| TemplateStatus == asEMetadataRegistrationResult::AlreadyRegistered));
		}
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded,
			FAngelscriptTypeBindInfoApply::MaterializePublishedTypes(Setup.Store, *Engine.Ptr, MoveTemp(Image))));
		ASSERT_THAT(IsNull(Image.Get()));
		asCTypeInfo* Type = static_cast<asCTypeInfo*>(Engine.Ptr->GetTypeInfoById(Setup.TypeId));
		ASSERT_THAT(IsNotNull(Type));
		ASSERT_THAT(IsTrue(Type->GetEngine() == Engine.Ptr));
		asCMetadataImage* OwnerImage = Type->GetMetadataImage();
		ASSERT_THAT(IsNotNull(OwnerImage));
		ASSERT_THAT(IsNull(OwnerImage->GetBoundEngine()));
		asCScriptFunction* Sum = static_cast<asCScriptFunction*>(Engine.Ptr->GetFunctionById(Setup.FunctionId));
		ASSERT_THAT(IsNotNull(Sum));
		ASSERT_THAT(IsTrue(Sum->GetEngine() == Engine.Ptr));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Sum)));
		Context->Release();
	}
};

#endif
