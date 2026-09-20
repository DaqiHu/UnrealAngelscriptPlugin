#include "CQTest.h"
#include "NativeSourceExecutionTestSupport.h"
#include "as_bytecode_image_codec.h"
#include "as_datatype.h"
#include "as_generic.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "frontend/Sema/as_type_identity.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	TArray<int32> GSourceCacheMarkTrace;

	void GenericSourceCacheMark(asIScriptGeneric* Generic)
	{
		const int32 Value = static_cast<int32>(Generic->GetArgDWord(0));
		GSourceCacheMarkTrace.Add(Value);
		Generic->SetReturnDWord(static_cast<asDWORD>(Value));
	}

	asSDefinitionParameter MakeSourceCacheIntParam(const char* Name, asCDefinitions& Set)
	{
		asSDefinitionParameter Parameter;
		Parameter.Name = Name;
		Parameter.Type = Set.Primitive(ttInt);
		return Parameter;
	}

	asCScriptFunction* DefineSourceCacheMark(FDetachedDefinitionFixture& Fixture)
	{
		Fixture.Scope = TEXT("vm-source-host");
		asSDefinitionParameter Param = MakeSourceCacheIntParam("n", *Fixture.Set);
		return Fixture.DefineFunction(TEXT("Mark"), TEXT("Host"), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), MakeArrayView(&Param, 1));
	}

	asCScriptFunction* DefineSourceCacheConstRead(FDetachedDefinitionFixture& Fixture, asCObjectType* Owner)
	{
		if (!Fixture.BindContext() || !Owner) return nullptr;
		AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor IntUse;
		IntUse.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Primitive;
		IntUse.Primitive = AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32;
		AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Descriptor;
		Descriptor.StableScope = Fixture.Scope;
		Descriptor.Name = TEXT("Read");
		Descriptor.Kind = AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Method;
		Descriptor.Owner = Owner->GetStableKey();
		Descriptor.Modifiers = AS_NAMESPACE_QUALIFIER asEFunctionIdentityModifiers::ConstMethod;
		if (Fixture.Context->GetIdentityRegistry().InternTypeUse(IntUse, Descriptor.ReturnType)
			!= AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
		{
			return nullptr;
		}
		AS_NAMESPACE_QUALIFIER asSStableKey Key;
		if (Fixture.Context->GetIdentityRegistry().InternFunction(Descriptor, Key)
			!= AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
		{
			return nullptr;
		}
		asCScriptFunction* Function = nullptr;
		if (Fixture.Set->CreateFunction(Key, "Read", "", asFUNC_SCRIPT, Function) != asEDefinitionResult::Succeeded
			|| Fixture.Set->DefineFunction(*Function, Fixture.Set->Primitive(ttInt), {}, Owner) != asEDefinitionResult::Succeeded
			|| Fixture.Set->SetFunctionDeclarationFacts(*Function, AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Method, Owner) != asEDefinitionResult::Succeeded)
		{
			return nullptr;
		}
		Function->SetReadOnly(true);
		return Function;
	}

	asCObjectType* DefineSourceCacheValueClass(FDetachedDefinitionFixture& Fixture, asCScriptFunction*& OutCtor, asCScriptFunction*& OutRead)
	{
		OutCtor = nullptr;
		OutRead = nullptr;
		Fixture.Scope = TEXT("vm-source");
		asCObjectType* Value = Fixture.DefineType(TEXT("Value"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		if (!Value) return nullptr;
		if (!Fixture.DefineProperty(*Value, "Number", Fixture.Set->Primitive(ttInt))) return nullptr;
		asSDefinitionParameter Initial = MakeSourceCacheIntParam("Initial", *Fixture.Set);
		OutCtor = Fixture.DefineFunction(
			TEXT("Value"),
			TEXT(""),
			asFUNC_SCRIPT,
			Fixture.Set->Primitive(ttVoid),
			MakeArrayView(&Initial, 1),
			Value,
			AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Constructor);
		if (!OutCtor) return nullptr;
		if (Fixture.Set->AddBehaviour(*Value, asBEHAVE_CONSTRUCT, *OutCtor) != asEDefinitionResult::Succeeded)
		{
			return nullptr;
		}
		OutRead = DefineSourceCacheConstRead(Fixture, Value);
		if (!OutRead) return nullptr;
		return Value;
	}

	asCScriptFunction* DefineSourceCacheRun(FDetachedDefinitionFixture& Fixture, bool bWithFlag)
	{
		Fixture.Scope = TEXT("vm-source");
		if (!bWithFlag)
		{
			return Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		}
		asSDefinitionParameter Flag = MakeSourceCacheIntParam("flag", *Fixture.Set);
		return Fixture.DefineFunction(
			TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), MakeArrayView(&Flag, 1));
	}
}

TEST_CLASS_WITH_FLAGS(VMSourceIntegration, "Angelscript.UnitTest.NativeEngine.SourceExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(SourceProducedCacheRunsAfterProducerRelease)
	{
		GSourceCacheMarkTrace.Reset();
		TArray<uint8> Bytes;
		asSStableKey RunKey;
		{
			FDetachedDefinitionFixture Host;
			asCScriptFunction* Mark = DefineSourceCacheMark(Host);
			ASSERT_THAT(IsNotNull(Mark));
			ASSERT_THAT(IsTrue(Host.FinalizeAndFreeze()));
			FSourceExecutionInput Input(
				TEXT("class Value { int Number; Value(int Initial) { this.Number = Initial; } int Read() const { return Number; } } int Run() { Value Local(Host::Mark(7)); return Local.Read(); }"),
				Host.Context, Host.Set.Get());
			asCBuilder Builder(Input.Sources, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
			asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
			ASSERT_THAT(IsNotNull(Function));
			RunKey = Function->GetStableKey();
			FSessionEmission Emitted = EmitFromSession(Builder);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
			FNativeEngineOwner EngineA;
			ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, EngineA.Ptr->RegisterExternalDefinitions(Host.TakeSet())));
			ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*EngineA.Ptr, Builder)));
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
				EngineA.Ptr->BindNativeFunction(Mark->GetStableKey(), asFUNCTION(GenericSourceCacheMark), asCALL_GENERIC)));
			asCExecutableSnapshot* SnapshotA = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*EngineA.Ptr, *Emitted.Image, SnapshotA)));
			asIScriptContext* ContextA = EngineA.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, ContextA->Prepare(Function)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ContextA->Execute()));
			ASSERT_THAT(AreEqual(7, static_cast<int32>(ContextA->GetReturnDWord())));
			ASSERT_THAT(AreEqual(1, GSourceCacheMarkTrace.Num()));
			ASSERT_THAT(AreEqual(7, GSourceCacheMarkTrace[0]));
			ContextA->Release();
			SnapshotA->Release();
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Emitted.Image, Bytes)));
		}
		GSourceCacheMarkTrace.Reset();
		FDetachedDefinitionFixture HostB;
		asCScriptFunction* MarkB = DefineSourceCacheMark(HostB);
		ASSERT_THAT(IsNotNull(MarkB));
		ASSERT_THAT(IsTrue(HostB.FinalizeAndFreeze()));
		FDetachedDefinitionFixture Dest;
		asCScriptFunction* DestCtor = nullptr;
		asCScriptFunction* DestRead = nullptr;
		ASSERT_THAT(IsNotNull(DefineSourceCacheValueClass(Dest, DestCtor, DestRead)));
		asCScriptFunction* FunctionB = DefineSourceCacheRun(Dest, false);
		ASSERT_THAT(IsNotNull(FunctionB));
		ASSERT_THAT(IsTrue(Dest.FinalizeAndFreeze()));
		ASSERT_THAT(IsTrue(FunctionB->GetStableKey() == RunKey, TEXT("Run key")));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, EngineB.Ptr->RegisterExternalDefinitions(HostB.TakeSet())));
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, EngineB.Ptr->RegisterExternalDefinitions(Dest.TakeSet())));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			EngineB.Ptr->BindNativeFunction(MarkB->GetStableKey(), asFUNCTION(GenericSourceCacheMark), asCALL_GENERIC)));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* SnapshotB = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*EngineB.Ptr, *Decoded, SnapshotB)));
		asIScriptContext* ContextB = EngineB.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, ContextB->Prepare(FunctionB)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ContextB->Execute()));
		ASSERT_THAT(AreEqual(7, static_cast<int32>(ContextB->GetReturnDWord())));
		ASSERT_THAT(AreEqual(1, GSourceCacheMarkTrace.Num()));
		ASSERT_THAT(AreEqual(7, GSourceCacheMarkTrace[0]));
		ContextB->Release();
		SnapshotB->Release();
	}

	TEST_METHOD(SourceMixedLoopObjectExceptionCache)
	{
		TArray<uint8> Bytes;
		asSStableKey RunKey;
		{
			FSourceExecutionInput Input(TEXT(
				"class Value { int Number; Value(int Initial) { this.Number = Initial; } int Read() const { return Number; } } "
				"int Run(int flag) { int s = 0; for (int i = 1; i <= 3; i = i + 1) { s = s + i; } Value Local(s); if (flag == 0) return Local.Read(); int z = 0; return 1 / z; }"));
			asCBuilder Builder(Input.Sources, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
			asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
			ASSERT_THAT(IsNotNull(Function));
			RunKey = Function->GetStableKey();
			FSessionEmission Emitted = EmitFromSession(Builder);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
			FNativeEngineOwner EngineA;
			ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*EngineA.Ptr, Builder)));
			asCExecutableSnapshot* SnapshotA = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*EngineA.Ptr, *Emitted.Image, SnapshotA)));
			asIScriptContext* ContextA = EngineA.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, ContextA->Prepare(Function)));
			ASSERT_THAT(AreEqual(0, ContextA->SetArgDWord(0, 0)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ContextA->Execute()));
			ASSERT_THAT(AreEqual(6, static_cast<int32>(ContextA->GetReturnDWord())));
			ASSERT_THAT(AreEqual(0, ContextA->Prepare(Function)));
			ASSERT_THAT(AreEqual(0, ContextA->SetArgDWord(0, 1)));
			ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, ContextA->Execute()));
			ContextA->Release();
			SnapshotA->Release();
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Emitted.Image, Bytes)));
		}
		FDetachedDefinitionFixture Dest;
		asCScriptFunction* DestCtor = nullptr;
		asCScriptFunction* DestRead = nullptr;
		ASSERT_THAT(IsNotNull(DefineSourceCacheValueClass(Dest, DestCtor, DestRead)));
		asCScriptFunction* FunctionB = DefineSourceCacheRun(Dest, true);
		ASSERT_THAT(IsNotNull(FunctionB));
		ASSERT_THAT(IsTrue(Dest.FinalizeAndFreeze()));
		ASSERT_THAT(IsTrue(FunctionB->GetStableKey() == RunKey, TEXT("mixed Run key")));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, EngineB.Ptr->RegisterExternalDefinitions(Dest.TakeSet())));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* SnapshotB = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*EngineB.Ptr, *Decoded, SnapshotB)));
		asIScriptContext* ContextB = EngineB.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, ContextB->Prepare(FunctionB)));
		ASSERT_THAT(AreEqual(0, ContextB->SetArgDWord(0, 0)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ContextB->Execute()));
		ASSERT_THAT(AreEqual(6, static_cast<int32>(ContextB->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, ContextB->Prepare(FunctionB)));
		ASSERT_THAT(AreEqual(0, ContextB->SetArgDWord(0, 1)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, ContextB->Execute()));
		ASSERT_THAT(AreEqual(0, ContextB->Prepare(FunctionB)));
		ASSERT_THAT(AreEqual(0, ContextB->SetArgDWord(0, 0)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ContextB->Execute()));
		ASSERT_THAT(AreEqual(6, static_cast<int32>(ContextB->GetReturnDWord())));
		ContextB->Release();
		SnapshotB->Release();
	}

	TEST_METHOD(MissingDestinationFunctionRejectsSourceCache)
	{
		TArray<uint8> Bytes;
		{
			FSourceExecutionInput Input(TEXT("int Run() { return 42; }"));
			asCBuilder Builder(Input.Sources, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough()));
			asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
			FSessionEmission Emitted = EmitFromSession(Builder);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Emitted.Image, Bytes)));
		}
		FNativeEngineOwner EngineB;
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::MissingDeclaration, asLinkByteCodeImage(*EngineB.Ptr, *Decoded, Snapshot)));
		ASSERT_THAT(IsNull(Snapshot));
	}

	TEST_METHOD(PreinstalledBodyRejectsSourceCache)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return 42; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Emitted.Image, Bytes)));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* First = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, First)));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* Second = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::BodyConflict, asLinkByteCodeImage(*Engine.Ptr, *Decoded, Second)));
		ASSERT_THAT(IsNull(Second));
		First->Release();
	}
};

#endif
