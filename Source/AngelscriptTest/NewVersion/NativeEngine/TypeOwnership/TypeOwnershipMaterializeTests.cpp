#include "CQTest.h"
#include "TypeOwnershipTestSupport.h"
#include "Core/AngelscriptTypeBindInfoApply.h"
#include "Core/AngelscriptTypeBindInfoStore.h"
#include "as_context.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(Materialize, "Angelscript.UnitTest.NativeEngine.TypeOwnership",
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
		Result.Source.Provider = TEXT("TypeOwnership.Materialize");
		Result.Source.File = TEXT("TypeOwnershipMaterializeTests.cpp");
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
	TEST_METHOD(Pair)
	{
		using namespace TypeOwnershipTest;
		FPairSetup Setup;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(RecordAndPublish(Setup, Diagnostic), *Diagnostic));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded,
			FAngelscriptTypeBindInfoApply::MaterializePublishedTypes(Setup.Store, *Engine.Ptr)));
		asITypeInfo* Type = Engine.Ptr->GetTypeInfoById(Setup.TypeId);
		ASSERT_THAT(IsNotNull(Type));
		ASSERT_THAT(IsTrue(Type->GetEngine() == Engine.Ptr));
		ASSERT_THAT(AreEqual(Setup.TypeId, Type->GetTypeId()));
		ASSERT_THAT(AreEqual(FString(TEXT("Pair")), FString(UTF8_TO_TCHAR(Type->GetName()))));
		asCScriptFunction* Sum = static_cast<asCScriptFunction*>(Engine.Ptr->GetFunctionById(Setup.FunctionId));
		ASSERT_THAT(IsNotNull(Sum));
		ASSERT_THAT(AreEqual(Setup.FunctionId, Sum->GetId()));
		FTypeOwnershipPair Host;
		Host.X = PairX;
		Host.Y = PairY;
		ASSERT_THAT(AreEqual(PairSum, ExecuteSum(*Engine.Ptr, Sum, Host)));
	}

	TEST_METHOD(DiscardImage)
	{
		using namespace TypeOwnershipTest;
		FPairSetup Setup;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(RecordAndPublish(Setup, Diagnostic), *Diagnostic));
		TUniquePtr<FAngelscriptTypeBindInfoInstallation> Prepared;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoApply::Prepare(Setup.Store, Prepared, Diagnostic), *Diagnostic));
		TUniquePtr<asCMetadataImage> Templates = Prepared->TakeTemplateImage();
		TUniquePtr<asCMetadataImage> Image = Prepared->TakeImage();
		ASSERT_THAT(IsNotNull(Image.Get()));
		FString SeenTypes;
		for (asCTypeInfo* Type : Image->GetTypes())
		{
			if (!Type) continue;
			SeenTypes += UTF8_TO_TCHAR(Type->GetName());
			SeenTypes += TEXT("@");
			SeenTypes += UTF8_TO_TCHAR(Type->GetNamespace());
			SeenTypes += TEXT(";");
		}
		ASSERT_THAT(AreEqual(FString(TEXT("Pair@Fixture;")), SeenTypes));
		FString SeenFunctions;
		for (asCScriptFunction* Function : Image->GetFunctions())
		{
			if (!Function) continue;
			SeenFunctions += UTF8_TO_TCHAR(Function->GetName());
			SeenFunctions += TEXT(";");
		}
		ASSERT_THAT(AreEqual(FString(TEXT("Sum;")), SeenFunctions));
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
		asCScriptFunction* Sum = static_cast<asCScriptFunction*>(Engine.Ptr->GetFunctionById(Setup.FunctionId));
		ASSERT_THAT(IsNotNull(Sum));
		FTypeOwnershipPair Host;
		Host.X = PairX;
		Host.Y = PairY;
		ASSERT_THAT(AreEqual(PairSum, ExecuteSum(*Engine.Ptr, Sum, Host)));
	}
};

#endif
