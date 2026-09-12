#include "CQTest.h"
#include "NativeSourceExecutionTestSupport.h"
#include "as_bytecode_image_codec.h"
#include "as_datatype.h"
#include "as_generic.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace VMSourceCacheContractsPrivate
{
	TArray<int32> GSourceCacheMarkTrace;
	int32 GFillWrites = 0;
	FString GLastError;

	void Reset()
	{
		GSourceCacheMarkTrace.Reset();
		GFillWrites = 0;
		GLastError.Reset();
	}

	void GenericMark(asIScriptGeneric* Generic)
	{
		const int32 Value = static_cast<int32>(Generic->GetArgDWord(0));
		GSourceCacheMarkTrace.Add(Value);
		Generic->SetReturnDWord(static_cast<asDWORD>(Value));
	}

	void GenericFail(asIScriptGeneric* Generic)
	{
		(void)Generic;
		if (asIScriptContext* Context = asGetActiveContext())
		{
			Context->SetException("fail");
		}
	}

	void GenericFill(asIScriptGeneric* Generic)
	{
		++GFillWrites;
		Generic->SetReturnDWord(42);
	}

	asSMetadataParameter MakeIntParam(const char* Name)
	{
		asSMetadataParameter Parameter;
		Parameter.Name = Name;
		Parameter.Type = asCDataType::CreatePrimitive(ttInt, false);
		return Parameter;
	}

	struct FHost
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Mark = nullptr;
		asCScriptFunction* Fail = nullptr;
		asCScriptFunction* Fill = nullptr;

		bool Initialize()
		{
			Fixture.Scope = TEXT("vm-source-host");
			asSMetadataParameter Param = MakeIntParam("n");
			Param.Type = Fixture.Set->Primitive(ttInt);
			Mark = Fixture.DefineFunction(TEXT("Mark"), TEXT("Host"), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), MakeArrayView(&Param, 1));
			Fail = Fixture.DefineFunction(TEXT("Fail"), TEXT("Host"), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), {});
			Fill = Fixture.DefineFunction(TEXT("Fill"), TEXT("Host"), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), {});
			return Mark && Fail && Fill && Fixture.FinalizeAndFreeze();
		}
	};

	const TCHAR* BoxSource()
	{
		return TEXT("class Box { int Id; Box(int Initial) { Id = Initial; } ~Box() { Host::Mark(Id); } } ");
	}

	asCScriptFunction* DefineRun(FDetachedDefinitionFixture& Fixture, bool bWithThrowAt)
	{
		Fixture.Scope = TEXT("vm-source");
		if (!bWithThrowAt)
		{
			return Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		}
		asSMetadataParameter Param = MakeIntParam("ThrowAt");
		Param.Type = Fixture.Set->Primitive(ttInt);
		return Fixture.DefineFunction(
			TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), MakeArrayView(&Param, 1));
	}

	asCScriptFunction* DefineRecovery(FDetachedDefinitionFixture& Fixture)
	{
		Fixture.Scope = TEXT("vm-source");
		return Fixture.DefineFunction(TEXT("Recovery"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
	}

	bool DefineBoxClass(FDetachedDefinitionFixture& Fixture)
	{
		Fixture.Scope = TEXT("vm-source");
		asCObjectType* Box = Fixture.DefineType(TEXT("Box"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		if (!Box) return false;
		if (!Fixture.DefineProperty(*Box, "Id", Fixture.Set->Primitive(ttInt))) return false;
		asSMetadataParameter Initial = MakeIntParam("Initial");
		Initial.Type = Fixture.Set->Primitive(ttInt);
		asCScriptFunction* Ctor = Fixture.DefineFunction(
			TEXT("Box"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttVoid), MakeArrayView(&Initial, 1),
			Box, AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Constructor);
		asCScriptFunction* Dtor = Fixture.DefineFunction(
			TEXT("~Box"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttVoid), {},
			Box, AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Destructor);
		if (!Ctor || !Dtor) return false;
		return Fixture.Set->AddBehaviour(*Box, asBEHAVE_CONSTRUCT, *Ctor) == asEMetadataResult::Succeeded
			&& Fixture.Set->AddBehaviour(*Box, asBEHAVE_DESTRUCT, *Dtor) == asEMetadataResult::Succeeded;
	}

	bool BindHost(asCScriptEngine& Engine, FHost& Host)
	{
		if (RegisterCompiledDefinitions(Engine, MoveTemp(Host.Fixture.Set)) != asEMetadataRegistrationResult::Succeeded)
		{
			GLastError = TEXT("register-host");
			return false;
		}
		if (Engine.BindNativeFunction(Host.Mark->GetStableKey(), asFUNCTION(GenericMark), asCALL_GENERIC) != asEByteCodeLinkStatus::Succeeded
			|| Engine.BindNativeFunction(Host.Fail->GetStableKey(), asFUNCTION(GenericFail), asCALL_GENERIC) != asEByteCodeLinkStatus::Succeeded
			|| Engine.BindNativeFunction(Host.Fill->GetStableKey(), asFUNCTION(GenericFill), asCALL_GENERIC) != asEByteCodeLinkStatus::Succeeded)
		{
			GLastError = TEXT("bind-host");
			return false;
		}
		return true;
	}

	bool EmitAndEncode(asCBuilder& Builder, TArray<uint8>& OutBytes)
	{
		if (!Builder.RunThrough())
		{
			GLastError = TEXT("compile:") + DescribeBuilderFailure(Builder);
			return false;
		}
		FSessionEmission Emitted = EmitFromSession(Builder);
		if (Emitted.EmissionStatus != asEByteCodeEmissionStatus::Succeeded || !Emitted.Image.Get())
		{
			GLastError = FString::Printf(TEXT("emit-%d"), int32(Emitted.EmissionStatus));
			return false;
		}
		const TUniquePtr<asCModuleDefinitionSet> KeepDefs = Builder.TakeModuleDefinitionSet();
		(void)KeepDefs;
		if (asCByteCodeImageCodec::Encode(*Emitted.Image, OutBytes) != asEByteCodeImageStatus::Succeeded)
		{
			GLastError = TEXT("encode");
			return false;
		}
		return true;
	}

	bool RecoverNinetySeven(asIScriptContext* Context, asCScriptFunction* Recovery)
	{
		return Context && Recovery
			&& Context->Prepare(Recovery) == 0
			&& Context->Execute() == asEXECUTION_FINISHED
			&& static_cast<int32>(Context->GetReturnDWord()) == 97;
	}

	int ExceptionLine(asIScriptContext* Context)
	{
		int Column = 0;
		const char* Section = nullptr;
		return Context ? Context->GetExceptionLineNumber(&Column, &Section) : 0;
	}
}

TEST_CLASS_WITH_FLAGS(VMSourceCacheContracts, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(IntegerControlStillSeven)
	{
		using namespace VMSourceCacheContractsPrivate;
		Reset();
		TArray<uint8> Bytes;
		asSStableKey RunKey;
		{
			FSourceExecutionInput Input(TEXT("int Run() { return 1+2*3; }"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(EmitAndEncode(Builder, Bytes), GLastError));
			asCScriptFunction* Run = FindSourceFunction(Builder, "Run");
			ASSERT_THAT(IsNotNull(Run));
			RunKey = Run->GetStableKey();
		}
		FDetachedDefinitionFixture Dest;
		asCScriptFunction* RunB = DefineRun(Dest, false);
		ASSERT_THAT(IsNotNull(RunB));
		ASSERT_THAT(IsTrue(Dest.FinalizeAndFreeze()));
		ASSERT_THAT(IsTrue(RunB->GetStableKey() == RunKey));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineB.Ptr->InstallDefinitionSet(Dest.TakeSet())));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*EngineB.Ptr, *Decoded, Snapshot)));
		asIScriptContext* Context = EngineB.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(RunB)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(7, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(IsTrue(Context->Prepare(RunB) == 0 && EngineB.Ptr->FindMetadataFunction(RunKey) == RunB));
		Context->Release();
		if (Snapshot) Snapshot->Release();
	}

	TEST_METHOD(SourceCacheExecutesOnBAfterProducerRelease)
	{
		using namespace VMSourceCacheContractsPrivate;
		Reset();
		TArray<uint8> Bytes;
		asSStableKey RunKey;
		{
			FHost HostA;
			ASSERT_THAT(IsTrue(HostA.Initialize()));
			FSourceExecutionInput Input(
				FString(BoxSource()) + TEXT("int Run() { Box Local(7); return Host::Fill() + Local.Id; }"),
				HostA.Fixture.Context, HostA.Fixture.Set.Get());
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
			FSessionEmission Emitted = EmitFromSession(Builder);
			ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
			asCScriptFunction* RunA = FindSourceFunction(Builder, "Run");
			ASSERT_THAT(IsNotNull(RunA));
			RunKey = RunA->GetStableKey();
			FNativeEngineOwner EngineA;
			ASSERT_THAT(IsTrue(BindHost(*EngineA.Ptr, HostA), GLastError));
			ASSERT_THAT(IsTrue(RegisterCompiledDefinitions(*EngineA.Ptr, Builder) == asEMetadataRegistrationResult::Succeeded
				|| RegisterCompiledDefinitions(*EngineA.Ptr, Builder) == asEMetadataRegistrationResult::AlreadyRegistered));
			asCExecutableSnapshot* SnapshotA = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*EngineA.Ptr, *Emitted.Image, SnapshotA)));
			asIScriptContext* ContextA = EngineA.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, ContextA->Prepare(RunA)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ContextA->Execute()));
			ASSERT_THAT(AreEqual(49, static_cast<int32>(ContextA->GetReturnDWord())));
			ASSERT_THAT(AreEqual(1, GSourceCacheMarkTrace.Num()));
			ASSERT_THAT(AreEqual(7, GSourceCacheMarkTrace[0]));
			ASSERT_THAT(AreEqual(1, GFillWrites));
			ContextA->Release();
			if (SnapshotA) SnapshotA->Release();
			const TUniquePtr<asCModuleDefinitionSet> KeepDefs = Builder.TakeModuleDefinitionSet();
			(void)KeepDefs;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Emitted.Image, Bytes)));
		}
		Reset();
		FHost HostB;
		ASSERT_THAT(IsTrue(HostB.Initialize()));
		FDetachedDefinitionFixture Dest;
		ASSERT_THAT(IsTrue(DefineBoxClass(Dest)));
		asCScriptFunction* RunB = DefineRun(Dest, false);
		ASSERT_THAT(IsNotNull(RunB));
		ASSERT_THAT(IsTrue(Dest.FinalizeAndFreeze()));
		ASSERT_THAT(IsTrue(RunB->GetStableKey() == RunKey));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(IsTrue(BindHost(*EngineB.Ptr, HostB), GLastError));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineB.Ptr->InstallDefinitionSet(Dest.TakeSet())));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* SnapshotB = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*EngineB.Ptr, *Decoded, SnapshotB)));
		asIScriptContext* ContextB = EngineB.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, ContextB->Prepare(RunB)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ContextB->Execute()));
		ASSERT_THAT(AreEqual(49, static_cast<int32>(ContextB->GetReturnDWord())));
		ASSERT_THAT(AreEqual(1, GSourceCacheMarkTrace.Num()));
		ASSERT_THAT(AreEqual(7, GSourceCacheMarkTrace[0]));
		ASSERT_THAT(AreEqual(1, GFillWrites));
		ASSERT_THAT(IsTrue(EngineB.Ptr->FindMetadataFunction(RunKey) == RunB));
		ContextB->Release();
		if (SnapshotB) SnapshotB->Release();
	}

	TEST_METHOD(MixedLoopBodyObjectsContinueThrowRecoverOnAAndB)
	{
		using namespace VMSourceCacheContractsPrivate;
		Reset();
		const FString Program = FString(BoxSource())
			+ TEXT("int Run(int ThrowAt) { int I = 0; for (Box Init(10); I < 3; I = I + 1) { Box Body(20 + I); if (I == 0) continue; if (I == ThrowAt) Host::Fail(); } return 41; } ")
			+ TEXT("int Recovery() { return 97; }");
		TArray<uint8> Bytes;
		asSStableKey RunKey;
		asSStableKey RecoveryKey;
		int LineA = 0;
		{
			FHost HostA;
			ASSERT_THAT(IsTrue(HostA.Initialize()));
			FSourceExecutionInput Input(Program, HostA.Fixture.Context, HostA.Fixture.Set.Get());
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
			FSessionEmission Emitted = EmitFromSession(Builder);
			ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
			asCScriptFunction* RunA = FindSourceFunction(Builder, "Run");
			asCScriptFunction* RecoveryA = FindSourceFunction(Builder, "Recovery");
			ASSERT_THAT(IsNotNull(RunA));
			ASSERT_THAT(IsNotNull(RecoveryA));
			RunKey = RunA->GetStableKey();
			RecoveryKey = RecoveryA->GetStableKey();
			FNativeEngineOwner EngineA;
			ASSERT_THAT(IsTrue(BindHost(*EngineA.Ptr, HostA), GLastError));
			ASSERT_THAT(IsTrue(RegisterCompiledDefinitions(*EngineA.Ptr, Builder) == asEMetadataRegistrationResult::Succeeded
				|| RegisterCompiledDefinitions(*EngineA.Ptr, Builder) == asEMetadataRegistrationResult::AlreadyRegistered));
			asCExecutableSnapshot* SnapshotA = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*EngineA.Ptr, *Emitted.Image, SnapshotA)));
			asIScriptContext* ContextA = EngineA.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, ContextA->Prepare(RunA)));
			ContextA->SetArgDWord(0, -1);
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ContextA->Execute()));
			ASSERT_THAT(AreEqual(41, static_cast<int32>(ContextA->GetReturnDWord())));
			ASSERT_THAT(AreEqual(4, GSourceCacheMarkTrace.Num()));
			ASSERT_THAT(AreEqual(20, GSourceCacheMarkTrace[0]));
			ASSERT_THAT(AreEqual(21, GSourceCacheMarkTrace[1]));
			ASSERT_THAT(AreEqual(22, GSourceCacheMarkTrace[2]));
			ASSERT_THAT(AreEqual(10, GSourceCacheMarkTrace[3]));
			Reset();
			ASSERT_THAT(AreEqual(0, ContextA->Prepare(RunA)));
			ContextA->SetArgDWord(0, 1);
			ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, ContextA->Execute()));
			ASSERT_THAT(AreEqual(3, GSourceCacheMarkTrace.Num()));
			ASSERT_THAT(AreEqual(20, GSourceCacheMarkTrace[0]));
			ASSERT_THAT(AreEqual(21, GSourceCacheMarkTrace[1]));
			ASSERT_THAT(AreEqual(10, GSourceCacheMarkTrace[2]));
			LineA = ExceptionLine(ContextA);
			ASSERT_THAT(IsTrue(LineA > 0));
			ASSERT_THAT(IsTrue(RecoverNinetySeven(ContextA, RecoveryA)));
			ASSERT_THAT(AreEqual(3, GSourceCacheMarkTrace.Num()));
			ContextA->Release();
			if (SnapshotA) SnapshotA->Release();
			const TUniquePtr<asCModuleDefinitionSet> KeepDefs = Builder.TakeModuleDefinitionSet();
			(void)KeepDefs;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Emitted.Image, Bytes)));
		}
		Reset();
		FHost HostB;
		ASSERT_THAT(IsTrue(HostB.Initialize()));
		FDetachedDefinitionFixture Dest;
		ASSERT_THAT(IsTrue(DefineBoxClass(Dest)));
		asCScriptFunction* RunB = DefineRun(Dest, true);
		asCScriptFunction* RecoveryB = DefineRecovery(Dest);
		ASSERT_THAT(IsNotNull(RunB));
		ASSERT_THAT(IsNotNull(RecoveryB));
		ASSERT_THAT(IsTrue(Dest.FinalizeAndFreeze()));
		ASSERT_THAT(IsTrue(RunB->GetStableKey() == RunKey));
		ASSERT_THAT(IsTrue(RecoveryB->GetStableKey() == RecoveryKey));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(IsTrue(BindHost(*EngineB.Ptr, HostB), GLastError));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineB.Ptr->InstallDefinitionSet(Dest.TakeSet())));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* SnapshotB = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*EngineB.Ptr, *Decoded, SnapshotB)));
		asIScriptContext* ContextB = EngineB.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, ContextB->Prepare(RunB)));
		ContextB->SetArgDWord(0, -1);
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ContextB->Execute()));
		ASSERT_THAT(AreEqual(41, static_cast<int32>(ContextB->GetReturnDWord())));
		ASSERT_THAT(AreEqual(4, GSourceCacheMarkTrace.Num()));
		ASSERT_THAT(AreEqual(20, GSourceCacheMarkTrace[0]));
		ASSERT_THAT(AreEqual(21, GSourceCacheMarkTrace[1]));
		ASSERT_THAT(AreEqual(22, GSourceCacheMarkTrace[2]));
		ASSERT_THAT(AreEqual(10, GSourceCacheMarkTrace[3]));
		Reset();
		ASSERT_THAT(AreEqual(0, ContextB->Prepare(RunB)));
		ContextB->SetArgDWord(0, 1);
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, ContextB->Execute()));
		ASSERT_THAT(AreEqual(3, GSourceCacheMarkTrace.Num()));
		ASSERT_THAT(AreEqual(20, GSourceCacheMarkTrace[0]));
		ASSERT_THAT(AreEqual(21, GSourceCacheMarkTrace[1]));
		ASSERT_THAT(AreEqual(10, GSourceCacheMarkTrace[2]));
		const int LineB = ExceptionLine(ContextB);
		ASSERT_THAT(IsTrue(LineB > 0));
		ASSERT_THAT(AreEqual(LineA, LineB));
		ASSERT_THAT(IsTrue(RecoverNinetySeven(ContextB, RecoveryB)));
		ASSERT_THAT(AreEqual(3, GSourceCacheMarkTrace.Num()));
		ASSERT_THAT(IsTrue(EngineB.Ptr->FindMetadataFunction(RunKey) == RunB));
		ContextB->Release();
		if (SnapshotB) SnapshotB->Release();
	}

	TEST_METHOD(MissingClassRejectsWithoutInstalling)
	{
		using namespace VMSourceCacheContractsPrivate;
		Reset();
		TArray<uint8> Bytes;
		{
			FHost HostA;
			ASSERT_THAT(IsTrue(HostA.Initialize()));
			FSourceExecutionInput Input(
				FString(BoxSource()) + TEXT("int Run() { Box Local(7); return Local.Id; }"),
				HostA.Fixture.Context, HostA.Fixture.Set.Get());
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(EmitAndEncode(Builder, Bytes), GLastError));
		}
		FHost HostB;
		ASSERT_THAT(IsTrue(HostB.Initialize()));
		FDetachedDefinitionFixture Dest;
		asCScriptFunction* RunB = DefineRun(Dest, false);
		ASSERT_THAT(IsNotNull(RunB));
		ASSERT_THAT(IsTrue(Dest.FinalizeAndFreeze()));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(IsTrue(BindHost(*EngineB.Ptr, HostB), GLastError));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineB.Ptr->InstallDefinitionSet(Dest.TakeSet())));
		const auto Counts = EngineB.Ptr->GetMetadataRegistrationCounts();
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::MissingDeclaration, asLinkByteCodeImage(*EngineB.Ptr, *Decoded, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsNull(EngineB.Ptr->FindPublishedExecutable(RunB->GetStableKey())));
		ASSERT_THAT(IsTrue(EngineB.Ptr->GetMetadataRegistrationCounts() == Counts));
		ASSERT_THAT(AreEqual(0, GFillWrites));
		ASSERT_THAT(AreEqual(0, GSourceCacheMarkTrace.Num()));
	}

	TEST_METHOD(MissingFreeFunctionRejectsWithoutInstalling)
	{
		using namespace VMSourceCacheContractsPrivate;
		Reset();
		TArray<uint8> Bytes;
		{
			FSourceExecutionInput Input(TEXT("int Run() { return 42; }"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(EmitAndEncode(Builder, Bytes), GLastError));
		}
		FDetachedDefinitionFixture Dest;
		asCScriptFunction* Other = Dest.DefineFunction(TEXT("Other"), TEXT(""), asFUNC_SCRIPT, Dest.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsNotNull(Other));
		ASSERT_THAT(IsTrue(Dest.FinalizeAndFreeze()));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineB.Ptr->InstallDefinitionSet(Dest.TakeSet())));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::MissingDeclaration, asLinkByteCodeImage(*EngineB.Ptr, *Decoded, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsNull(EngineB.Ptr->FindPublishedExecutable(Other->GetStableKey())));
	}

	TEST_METHOD(ChangedMethodReturnRejects)
	{
		using namespace VMSourceCacheContractsPrivate;
		Reset();
		TArray<uint8> Bytes;
		asSStableKey RunKey;
		{
			FSourceExecutionInput Input(TEXT("int Run() { return 42; }"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(EmitAndEncode(Builder, Bytes), GLastError));
			asCScriptFunction* Run = FindSourceFunction(Builder, "Run");
			ASSERT_THAT(IsNotNull(Run));
			RunKey = Run->GetStableKey();
		}
		FDetachedDefinitionFixture Dest;
		Dest.Scope = TEXT("vm-source");
		asCScriptFunction* RunB = Dest.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Dest.Set->Primitive(ttFloat64), {});
		ASSERT_THAT(IsNotNull(RunB));
		ASSERT_THAT(IsTrue(Dest.FinalizeAndFreeze()));
		ASSERT_THAT(IsTrue(RunB->GetStableKey() == RunKey));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineB.Ptr->InstallDefinitionSet(Dest.TakeSet())));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::SchemaMismatch, asLinkByteCodeImage(*EngineB.Ptr, *Decoded, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsNull(EngineB.Ptr->FindPublishedExecutable(RunKey)));
	}

	TEST_METHOD(MissingNativeBindingRejects)
	{
		using namespace VMSourceCacheContractsPrivate;
		Reset();
		TArray<uint8> Bytes;
		{
			FHost HostA;
			ASSERT_THAT(IsTrue(HostA.Initialize()));
			FSourceExecutionInput Input(
				TEXT("int Run() { return Host::Fill(); }"),
				HostA.Fixture.Context, HostA.Fixture.Set.Get());
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(EmitAndEncode(Builder, Bytes), GLastError));
		}
		FHost HostB;
		ASSERT_THAT(IsTrue(HostB.Initialize()));
		FDetachedDefinitionFixture Dest;
		asCScriptFunction* RunB = DefineRun(Dest, false);
		ASSERT_THAT(IsNotNull(RunB));
		ASSERT_THAT(IsTrue(Dest.FinalizeAndFreeze()));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*EngineB.Ptr, MoveTemp(HostB.Fixture.Set))));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineB.Ptr->InstallDefinitionSet(Dest.TakeSet())));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::MissingBinding, asLinkByteCodeImage(*EngineB.Ptr, *Decoded, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsNull(EngineB.Ptr->FindPublishedExecutable(RunB->GetStableKey())));
		ASSERT_THAT(AreEqual(0, GFillWrites));
	}

	TEST_METHOD(ForeignImageRejects)
	{
		using namespace VMSourceCacheContractsPrivate;
		Reset();
		FHost HostA;
		ASSERT_THAT(IsTrue(HostA.Initialize()));
		FSourceExecutionInput Input(TEXT("int Run() { return 42; }"), HostA.Fixture.Context, HostA.Fixture.Set.Get());
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		TArray<uint8> Bytes;
		ASSERT_THAT(IsTrue(EmitAndEncode(Builder, Bytes), GLastError));
		FNativeEngineOwner EngineA;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*EngineA.Ptr, Builder)));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::ForeignEngine, RegisterCompiledDefinitions(*EngineB.Ptr, Builder)));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::MissingDeclaration, asLinkByteCodeImage(*EngineB.Ptr, *Decoded, Failed)));
		ASSERT_THAT(IsNull(Failed));
	}

	TEST_METHOD(ConflictingBodyLeavesInstalledCallable)
	{
		using namespace VMSourceCacheContractsPrivate;
		Reset();
		TArray<uint8> SimpleBytes;
		TArray<uint8> LoopBytes;
		asSStableKey RunKey;
		{
			FSourceExecutionInput Simple(TEXT("int Run(int ThrowAt) { return 7; }"));
			asCBuilder SimpleBuilder(Simple.Snapshot, Simple.Diagnostics, Simple.Options);
			ASSERT_THAT(IsTrue(EmitAndEncode(SimpleBuilder, SimpleBytes), GLastError));
			asCScriptFunction* SimpleRun = FindSourceFunction(SimpleBuilder, "Run");
			ASSERT_THAT(IsNotNull(SimpleRun));
			RunKey = SimpleRun->GetStableKey();
		}
		{
			FHost HostA;
			ASSERT_THAT(IsTrue(HostA.Initialize()));
			FSourceExecutionInput Loop(
				FString(BoxSource())
				+ TEXT("int Run(int ThrowAt) { Box Local(1); if (ThrowAt == 1) Host::Fail(); return 41; }"),
				HostA.Fixture.Context, HostA.Fixture.Set.Get());
			asCBuilder LoopBuilder(Loop.Snapshot, Loop.Diagnostics, Loop.Options);
			ASSERT_THAT(IsTrue(EmitAndEncode(LoopBuilder, LoopBytes), GLastError));
			asCScriptFunction* LoopRun = FindSourceFunction(LoopBuilder, "Run");
			ASSERT_THAT(IsNotNull(LoopRun));
			ASSERT_THAT(IsTrue(LoopRun->GetStableKey() == RunKey));
		}
		FHost HostB;
		ASSERT_THAT(IsTrue(HostB.Initialize()));
		FDetachedDefinitionFixture Dest;
		ASSERT_THAT(IsTrue(DefineBoxClass(Dest)));
		asCScriptFunction* RunB = DefineRun(Dest, true);
		ASSERT_THAT(IsNotNull(RunB));
		ASSERT_THAT(IsTrue(Dest.FinalizeAndFreeze()));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(IsTrue(BindHost(*EngineB.Ptr, HostB), GLastError));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineB.Ptr->InstallDefinitionSet(Dest.TakeSet())));
		TUniquePtr<asCByteCodeImage> SimpleDecoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(SimpleBytes, SimpleDecoded)));
		asCExecutableSnapshot* First = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*EngineB.Ptr, *SimpleDecoded, First)));
		asIScriptContext* Context = EngineB.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(RunB)));
		Context->SetArgDWord(0, 0);
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(7, static_cast<int32>(Context->GetReturnDWord())));
		TUniquePtr<asCByteCodeImage> LoopDecoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(LoopBytes, LoopDecoded)));
		asCExecutableSnapshot* Second = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::BodyConflict, asLinkByteCodeImage(*EngineB.Ptr, *LoopDecoded, Second)));
		ASSERT_THAT(IsNull(Second));
		ASSERT_THAT(AreEqual(0, Context->Prepare(RunB)));
		Context->SetArgDWord(0, 0);
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(7, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, GSourceCacheMarkTrace.Num()));
		Context->Release();
		if (First) First->Release();
	}
};

#endif
