#include "CQTest.h"
#include "../NativeEngineTestSupport.h"
#include "../VM/NativeVMTestSupport.h"
#include "frontend/as_definition_consumer.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace CallableSDKTests
{
	int32 ReceiverDestructions = 0;
	void GenericFortyTwo(asIScriptGeneric* Call) { Call->SetReturnDWord(42); }
	void DestructReceiver(asIScriptGeneric*) { ++ReceiverDestructions; }

	bool EmitReturn(asCByteCodeImageBuilder& Builder, asCScriptFunction& Function, int32 Value)
	{
		asUINT Slot = 0;
		const auto Local = ByteLocal(1), Ret = ByteImm16(ByteArgumentDwords(Function));
		const asSByteCodeOperand Set[2] = {Local, ByteImm32(Value)};
		return Builder.AddFunctionRequirement(Function, Slot) == asEByteCodeImageStatus::Succeeded
			&& Builder.BeginFunction(Function.GetStableKey(), Function.GetStableKey(), 4, ByteArgumentDwords(Function)) == asEByteCodeImageStatus::Succeeded
			&& Builder.Emit(asBC_SetV4, MakeArrayView(Set, 2)) == asEByteCodeImageStatus::Succeeded
			&& Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Local, 1)) == asEByteCodeImageStatus::Succeeded
			&& Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) == asEByteCodeImageStatus::Succeeded
			&& Builder.EndFunction() == asEByteCodeImageStatus::Succeeded;
	}

	bool BuildIndirect(asCScriptFunction& Target, asCScriptFunction& Run, TUniquePtr<asCByteCodeImage>& Out)
	{
		asCByteCodeImageBuilder Builder;
		if (Target.funcType == asFUNC_SCRIPT && !EmitReturn(Builder, Target, 42)) return false;
		asUINT TargetSlot = 0, RunSlot = 0;
		if (Builder.AddFunctionRequirement(Target, TargetSlot) != asEByteCodeImageStatus::Succeeded
			|| Builder.AddFunctionRequirement(Run, RunSlot) != asEByteCodeImageStatus::Succeeded
			|| Builder.BeginFunction(Run.GetStableKey(), Run.GetStableKey(), 8, 0) != asEByteCodeImageStatus::Succeeded) return false;
		const auto TargetOperand = ByteSlot(asEByteCodeOperandKind::FunctionSlot, TargetSlot);
		const auto Local = ByteLocal(2), Ret = ByteImm16(0);
		const asSByteCodeOperand Indirect[2] = {Local, TargetOperand};
		return Builder.Emit(asBC_FuncPtr, MakeArrayView(&TargetOperand, 1)) == asEByteCodeImageStatus::Succeeded
			&& Builder.Emit(asBC_PopRPtr, {}) == asEByteCodeImageStatus::Succeeded
			&& Builder.Emit(asBC_CpyRtoV8, MakeArrayView(&Local, 1)) == asEByteCodeImageStatus::Succeeded
			&& Builder.Emit(asBC_CallPtr, MakeArrayView(Indirect, 2)) == asEByteCodeImageStatus::Succeeded
			&& Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) == asEByteCodeImageStatus::Succeeded
			&& Builder.EndFunction() == asEByteCodeImageStatus::Succeeded
			&& Builder.Finish(Out) == asEByteCodeImageStatus::Succeeded;
	}

	// These dependent checks run as RED assertions before API removal.
	template<class T> constexpr bool HasRegisterFuncdef = requires(T& Value) { Value.RegisterFuncdef("void Callback()"); };
	template<class T> constexpr bool HasFuncdefEnumeration = requires(T& Value) { Value.GetFuncdefCount(); Value.GetFuncdefByIndex(0); };
	template<class T> constexpr bool HasOldSignatureQuery = requires(T& Value) { Value.GetFuncdefSignature(); };
	template<class T> constexpr bool HasOldChildQueries = requires(T& Value) { Value.GetChildFuncdefCount(); Value.GetChildFuncdef(0); };
	template<class T> constexpr bool HasOldDataTypeQuery = requires(T& Value) { Value.IsFuncdef(); };
	template<class T, class Callable> constexpr bool HasOldFactory = requires(T& Image, asSStableKey Key, asCScriptFunction& Signature, Callable*& Out)
	{ Image.CreateFuncdefType(Key, "Callback", "", Signature, Out); };
	template<class T, class Callable> constexpr bool HasOldChildFactory = requires(T& Image, asCObjectType& Parent, Callable& Child)
	{ Image.AddChildFuncdef(Parent, Child); };

	static_assert(!HasRegisterFuncdef<asIScriptEngine> && !HasRegisterFuncdef<asCScriptEngine>);
	static_assert(!HasFuncdefEnumeration<asIScriptEngine> && !HasFuncdefEnumeration<asCScriptEngine>);
	static_assert(!HasOldSignatureQuery<asITypeInfo> && !HasOldSignatureQuery<asCTypeInfo>);
	static_assert(!HasOldChildQueries<asITypeInfo> && !HasOldChildQueries<asCTypeInfo>);
	static_assert(!HasOldDataTypeQuery<asCDataType>);
	static_assert(!HasOldFactory<asCMetadataImage, asCCallableType> && !HasOldChildFactory<asCMetadataImage, asCCallableType>);

}

TEST_CLASS_WITH_FLAGS(CallableSDK, "Angelscript.UnitTest.NativeEngine.LanguageSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(StringDeclarationRegistrationAndEngineEnumerationAreAbsent)
	{
		ASSERT_THAT(IsFalse(CallableSDKTests::HasRegisterFuncdef<asIScriptEngine>));
		ASSERT_THAT(IsFalse(CallableSDKTests::HasRegisterFuncdef<asCScriptEngine>));
		ASSERT_THAT(IsFalse(CallableSDKTests::HasFuncdefEnumeration<asIScriptEngine>));
		ASSERT_THAT(IsFalse(CallableSDKTests::HasFuncdefEnumeration<asCScriptEngine>));
	}

	TEST_METHOD(OldTypeSignatureAndChildQueriesAreAbsent)
	{
		ASSERT_THAT(IsFalse(CallableSDKTests::HasOldSignatureQuery<asITypeInfo>));
		ASSERT_THAT(IsFalse(CallableSDKTests::HasOldSignatureQuery<asCTypeInfo>));
		ASSERT_THAT(IsFalse(CallableSDKTests::HasOldChildQueries<asITypeInfo>));
		ASSERT_THAT(IsFalse(CallableSDKTests::HasOldChildQueries<asCTypeInfo>));
		ASSERT_THAT(IsFalse(CallableSDKTests::HasOldDataTypeQuery<asCDataType>));
	}

	TEST_METHOD(OldNominalFactoryAndChildFactoryAreAbsent)
	{
		ASSERT_THAT(IsFalse(CallableSDKTests::HasOldFactory<asCMetadataImage, asCCallableType>));
		ASSERT_THAT(IsFalse(CallableSDKTests::HasOldChildFactory<asCMetadataImage, asCCallableType>));
	}

	TEST_METHOD(NominalDelegateAndEventShareOnlyTheirStructuralSignature)
	{
		AngelscriptNativeEngineTest::FFrontendSessionRun Run({{TEXT("CallableTypes.as"),
			TEXT("delegate void Callback(int Value); event void Changed(int Value);")}});
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
		ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		const auto Result = asCDefinitionConsumer().Consume(*Run.Session);
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(2, Result.Image->GetTypes().Num()));
		auto* Left = static_cast<asCCallableType*>(Result.Image->GetTypes()[0]);
		auto* Right = static_cast<asCCallableType*>(Result.Image->GetTypes()[1]);
		ASSERT_THAT(IsNotNull(Left)); ASSERT_THAT(IsNotNull(Right));
		ASSERT_THAT(IsTrue(Left->GetStableKey() != Right->GetStableKey()));
		ASSERT_THAT(IsTrue(Left->GetCallableSignature() == Right->GetCallableSignature()));
		auto* Signature = static_cast<asCScriptFunction*>(Left->GetCallableSignature());
		ASSERT_THAT(IsTrue(Signature->IsMetadataCallableSignature()));
		ASSERT_THAT(IsTrue(Signature->GetStableKey() != Left->GetStableKey()));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Result.Image->FinalizeLayouts()));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Result.Image->Freeze()));
		ASSERT_THAT(IsTrue(Result.Image->ValidateFrozenDefinitions()));
	}

	TEST_METHOD(NamedScriptAndGenericNativeCallbacksExecuteFortyTwoIndirectly)
	{
		for (const asEFuncType Kind : {asFUNC_SCRIPT, asFUNC_SYSTEM})
		{
			FDetachedDefinitionFixture Fixture;
			auto* Callback = Fixture.DefineFunction(TEXT("FortyTwo"), TEXT(""), Kind, Fixture.Image->Primitive(ttInt), {});
			auto* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
			ASSERT_THAT(IsNotNull(Callback)); ASSERT_THAT(IsNotNull(Run));
			ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
			FNativeEngineOwner Engine;
			ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
			if (Kind == asFUNC_SYSTEM)
				ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->BindNativeFunction(Callback->GetStableKey(), asFUNCTION(CallableSDKTests::GenericFortyTwo), asCALL_GENERIC)));
			TUniquePtr<asCByteCodeImage> Image;
			ASSERT_THAT(IsTrue(CallableSDKTests::BuildIndirect(*Callback, *Run, Image)));
			asCExecutableSnapshot* Snapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
			ON_SCOPE_EXIT { Snapshot->Release(); };
			auto* Context = Engine.Ptr->CreateContext();
			ON_SCOPE_EXIT { Context->Release(); };
			ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(AreEqual(42u, Context->GetReturnDWord()));
		}
	}

	TEST_METHOD(ReturnTypeMismatchCannotReplaceTheInstalledCallable)
	{
		FDetachedDefinitionFixture Fixture;
		auto* Callback = Fixture.DefineFunction(TEXT("FortyTwo"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		auto* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(IsTrue(CallableSDKTests::BuildIndirect(*Callback, *Run, Image)));
		asCExecutableSnapshot* Installed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Installed)));
		ON_SCOPE_EXIT { Installed->Release(); };
		asSStableKey WrongReturn;
		ASSERT_THAT(AreEqual(asETypeIdentityStatus::Succeeded, Fixture.Context->InternPrimitiveTypeUse(asEPrimitiveType::Float64, WrongReturn)));
		bool Changed = false;
		for (auto& Requirement : Image->Requirements)
			if (Requirement.Key == Callback->GetStableKey())
			{
				Requirement.ReturnTypeUse = WrongReturn;
				asEncodeByteCodeRequirementContract(Requirement, Requirement.ContractWitness);
				Changed = true;
			}
		ASSERT_THAT(IsTrue(Changed));
		asCExecutableSnapshot* Rejected = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::SchemaMismatch, Engine.Ptr->LinkByteCodeImage(*Image, Rejected)));
		ASSERT_THAT(IsNull(Rejected));
		auto* Context = Engine.Ptr->CreateContext();
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42u, Context->GetReturnDWord()));
	}

	TEST_METHOD(CallableSignatureLeaseSurvivesProducerRelease)
	{
		asCCallableType* Retained = nullptr;
		asSStableKey TypeKey, SignatureKey;
		{
			AngelscriptNativeEngineTest::FFrontendSessionRun Run({{TEXT("Retained.as"), TEXT("delegate int Callback(int Value);")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
			const auto Result = asCDefinitionConsumer().Consume(*Run.Session);
			ASSERT_THAT(IsTrue(Result.Succeeded()));
			ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Result.Image->FinalizeLayouts()));
			ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Result.Image->Freeze()));
			Retained = static_cast<asCCallableType*>(Result.Image->GetTypes()[0]);
			ASSERT_THAT(IsNotNull(Retained));
			TypeKey = Retained->GetStableKey();
			SignatureKey = static_cast<asCScriptFunction*>(Retained->GetCallableSignature())->GetStableKey();
			Retained->AddRef();
		}
		ON_SCOPE_EXIT { Retained->Release(); };
		ASSERT_THAT(IsTrue(Retained->GetStableKey() == TypeKey));
		ASSERT_THAT(IsTrue(static_cast<asCScriptFunction*>(Retained->GetCallableSignature())->GetStableKey() == SignatureKey));
		ASSERT_THAT(AreEqual(1u, Retained->GetCallableSignature()->GetParamCount()));
		ASSERT_THAT(IsTrue(Retained->GetMetadataImage()->ValidateFrozenDefinitions()));
	}
	TEST_METHOD(BoundNamedCallbackReleasesItsReceiverExactlyOnce)
	{
		CallableSDKTests::ReceiverDestructions = 0;
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("Target"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		asCScriptFunction* Dtor = Fixture.DefineFunction(
			TEXT("Destruct"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttVoid), {}, Type);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Fixture.Image->AddBehaviour(*Type, asBEHAVE_DESTRUCT, *Dtor)));
		asCScriptFunction* Value = Fixture.DefineFunction(TEXT("Value"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {}, Type);
		auto Reference = asCDataType::CreateType(Type, false); Reference.MakeReference(true);
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Reference, {});
		asSMetadataParameter Fn;
		Fn.Name = "fn";
		Fn.Type = Fixture.Image->Primitive(ttInt64);
		asCScriptFunction* Invoke = Fixture.DefineFunction(
			TEXT("Invoke"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), MakeArrayView(&Fn, 1));
		asCScriptFunction* Shape = Fixture.DefineFunction(TEXT("DelegateShape"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(CallableSDKTests::DestructReceiver), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT RunSlot = 0;
		asUINT InvokeSlot = 0;
		asUINT ShapeSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Shape, ShapeSlot)));
		ASSERT_THAT(IsTrue(CallableSDKTests::EmitReturn(Builder, *Value, 42)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Invoke, InvokeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, 0)));
		asSByteCodeOperand Obj = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), ByteImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Obj, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Invoke->GetStableKey(), Invoke->GetStableKey(), 8, ByteArgumentDwords(*Invoke))));
		asSByteCodeOperand FnLocal = ByteLocal(0);
		asSByteCodeOperand Indirect[2] = {FnLocal, ByteSlot(asEByteCodeOperandKind::FunctionSlot, ShapeSlot)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CallPtr, MakeArrayView(Indirect, 2))));
		Ret = ByteImm16(ByteArgumentDwords(*Invoke));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		void* First = Context->GetReturnAddress();
		ASSERT_THAT(IsNotNull(First));
		asIScriptFunction* FirstDelegate = Engine.Ptr->CreateDelegate(Value, First);
		ASSERT_THAT(IsNotNull(FirstDelegate));
		ASSERT_THAT(IsTrue(FirstDelegate->GetDelegateObject() == First));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Invoke)));
		ASSERT_THAT(AreEqual(0, Context->SetArgAddress(0, FirstDelegate)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, CallableSDKTests::ReceiverDestructions));
		Engine.Ptr->ReleaseScriptObject(First, Type);
		ASSERT_THAT(AreEqual(0, CallableSDKTests::ReceiverDestructions));
		FirstDelegate->Release();
		Engine.Ptr->GarbageCollect(asGC_FULL_CYCLE);
		ASSERT_THAT(AreEqual(1, CallableSDKTests::ReceiverDestructions));
		Context->Release();
		Snapshot->Release();
	}
};
#endif
