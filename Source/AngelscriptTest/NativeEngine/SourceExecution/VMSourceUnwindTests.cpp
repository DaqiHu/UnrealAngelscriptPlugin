#include "CQTest.h"
#include "NativeSourceExecutionTestSupport.h"
#include "as_bytecode_image_codec.h"
#include "as_datatype.h"
#include "as_generic.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace VMSourceUnwindPrivate
{
	TArray<int32> GMarkTrace;
	FString GLastError;

	void ResetTrace()
	{
		GMarkTrace.Reset();
		GLastError.Reset();
	}

	void GenericMark(asIScriptGeneric* Generic)
	{
		GMarkTrace.Add(static_cast<int32>(Generic->GetArgDWord(0)));
		Generic->SetReturnDWord(Generic->GetArgDWord(0));
	}

	void GenericFail(asIScriptGeneric* Generic)
	{
		(void)Generic;
		if (asIScriptContext* Context = asGetActiveContext())
		{
			Context->SetException("fail");
		}
	}

	asSDefinitionParameter MakeIntParam(const char* Name)
	{
		asSDefinitionParameter Parameter;
		Parameter.Name = Name;
		Parameter.Type = asCDataType::CreatePrimitive(ttInt, false);
		return Parameter;
	}

	struct FUnwindHost
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Mark = nullptr;
		asCScriptFunction* Fail = nullptr;

		bool Initialize()
		{
			Fixture.Scope = TEXT("vm-source-unwind-host");
			asSDefinitionParameter Param = MakeIntParam("n");
			Param.Type = Fixture.Set->Primitive(ttInt);
			Mark = Fixture.DefineFunction(TEXT("Mark"), TEXT("Host"), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), MakeArrayView(&Param, 1));
			Fail = Fixture.DefineFunction(TEXT("Fail"), TEXT("Host"), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), {});
			return Mark && Fail && Fixture.FinalizeAndFreeze();
		}
	};

	const TCHAR* BoxSource()
	{
		return TEXT("class Box { int Id; Box(int Initial) { Id = Initial; } ~Box() { Host::Mark(Id); } } ");
	}

	bool SetupEngine(asCScriptEngine& Engine, FUnwindHost& Host, asCBuilder& Builder)
	{
		if (RegisterCompiledDefinitions(Engine, MoveTemp(Host.Fixture.Set)) != asERegistrationResult::Succeeded)
		{
			GLastError = TEXT("register-host");
			return false;
		}
		if (Engine.BindNativeFunction(Host.Mark->GetStableKey(), asFUNCTION(GenericMark), asCALL_GENERIC) != asEByteCodeLinkStatus::Succeeded
			|| Engine.BindNativeFunction(Host.Fail->GetStableKey(), asFUNCTION(GenericFail), asCALL_GENERIC) != asEByteCodeLinkStatus::Succeeded)
		{
			GLastError = TEXT("bind");
			return false;
		}
		const asERegistrationResult Status = RegisterCompiledDefinitions(Engine, Builder);
		if (Status != asERegistrationResult::Succeeded && Status != asERegistrationResult::AlreadyRegistered)
		{
			GLastError = TEXT("register-source");
			return false;
		}
		return true;
	}

	bool EmitImage(asCBuilder& Builder, TUniquePtr<asCByteCodeImage>& OutImage)
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
		OutImage = MoveTemp(Emitted.Image);
		if (asCDefinitions* Set = Builder.GetDefinitions())
		{
			(void)Set->TakeEmittedByteCode();
		}
		return true;
	}

	bool RecoverNinetySeven(asIScriptContext* Context, asCScriptFunction* Recovery)
	{
		if (!Context || !Recovery) return false;
		return Context->Prepare(Recovery) == 0
			&& Context->Execute() == asEXECUTION_FINISHED
			&& static_cast<int32>(Context->GetReturnDWord()) == 97;
	}
}

TEST_CLASS_WITH_FLAGS(VMSourceUnwind, "Angelscript.UnitTest.NativeEngine.SourceExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(IntegerControlStillSeven)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return 1+2*3; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(IsTrue(VMSourceUnwindPrivate::EmitImage(Builder, Image), VMSourceUnwindPrivate::GLastError));
		asCScriptFunction* Run = FindSourceFunction(Builder, "Run");
		FNativeEngineOwner Engine;
		ASSERT_THAT(IsTrue(RegisterCompiledDefinitions(*Engine.Ptr, Builder) == asERegistrationResult::Succeeded
			|| RegisterCompiledDefinitions(*Engine.Ptr, Builder) == asERegistrationResult::AlreadyRegistered));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(7, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		if (Snapshot) Snapshot->Release();
	}

	TEST_METHOD(TwoLocalsDivByZeroUnwindsReverseThenRecoveryNinetySeven)
	{
		VMSourceUnwindPrivate::ResetTrace();
		VMSourceUnwindPrivate::FUnwindHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput Input(
			FString(VMSourceUnwindPrivate::BoxSource())
			+ TEXT("int Run() { Box A(1); Box B(2); int D = 0; return 1/D; } int Recovery() { return 97; }"),
			Host.Fixture.Context, Host.Fixture.Set.Get());
		asCBuilder Builder(Input.Sources, Input.Options);
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(IsTrue(VMSourceUnwindPrivate::EmitImage(Builder, Image), VMSourceUnwindPrivate::GLastError));
		asCScriptFunction* Run = FindSourceFunction(Builder, "Run");
		asCScriptFunction* Recovery = FindSourceFunction(Builder, "Recovery");
		FNativeEngineOwner Engine;
		ASSERT_THAT(IsTrue(VMSourceUnwindPrivate::SetupEngine(*Engine.Ptr, Host, Builder), VMSourceUnwindPrivate::GLastError));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
		ASSERT_THAT(AreEqual(2, VMSourceUnwindPrivate::GMarkTrace.Num()));
		ASSERT_THAT(AreEqual(2, VMSourceUnwindPrivate::GMarkTrace[0]));
		ASSERT_THAT(AreEqual(1, VMSourceUnwindPrivate::GMarkTrace[1]));
		ASSERT_THAT(IsTrue(VMSourceUnwindPrivate::RecoverNinetySeven(Context, Recovery)));
		Context->Release();
		if (Snapshot) Snapshot->Release();
	}

	TEST_METHOD(NativeFailUnwindsReverseThenRecoveryNinetySeven)
	{
		VMSourceUnwindPrivate::ResetTrace();
		VMSourceUnwindPrivate::FUnwindHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput Input(
			FString(VMSourceUnwindPrivate::BoxSource())
			+ TEXT("int Run() { Box A(1); Box B(2); Host::Fail(); return 0; } int Recovery() { return 97; }"),
			Host.Fixture.Context, Host.Fixture.Set.Get());
		asCBuilder Builder(Input.Sources, Input.Options);
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(IsTrue(VMSourceUnwindPrivate::EmitImage(Builder, Image), VMSourceUnwindPrivate::GLastError));
		asCScriptFunction* Run = FindSourceFunction(Builder, "Run");
		asCScriptFunction* Recovery = FindSourceFunction(Builder, "Recovery");
		FNativeEngineOwner Engine;
		ASSERT_THAT(IsTrue(VMSourceUnwindPrivate::SetupEngine(*Engine.Ptr, Host, Builder), VMSourceUnwindPrivate::GLastError));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
		ASSERT_THAT(AreEqual(2, VMSourceUnwindPrivate::GMarkTrace.Num()));
		ASSERT_THAT(AreEqual(2, VMSourceUnwindPrivate::GMarkTrace[0]));
		ASSERT_THAT(AreEqual(1, VMSourceUnwindPrivate::GMarkTrace[1]));
		ASSERT_THAT(IsTrue(VMSourceUnwindPrivate::RecoverNinetySeven(Context, Recovery)));
		Context->Release();
		if (Snapshot) Snapshot->Release();
	}

	TEST_METHOD(BrokenConstructorDestroysCompletedLocalOnly)
	{
		VMSourceUnwindPrivate::ResetTrace();
		VMSourceUnwindPrivate::FUnwindHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput Input(
			FString(VMSourceUnwindPrivate::BoxSource())
			+ TEXT("class Broken { Broken() { Box Local(1); Host::Fail(); } } ")
			+ TEXT("int Run() { Broken X(); return 0; } int Recovery() { return 97; }"),
			Host.Fixture.Context, Host.Fixture.Set.Get());
		asCBuilder Builder(Input.Sources, Input.Options);
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(IsTrue(VMSourceUnwindPrivate::EmitImage(Builder, Image), VMSourceUnwindPrivate::GLastError));
		asCScriptFunction* Run = FindSourceFunction(Builder, "Run");
		asCScriptFunction* Recovery = FindSourceFunction(Builder, "Recovery");
		FNativeEngineOwner Engine;
		ASSERT_THAT(IsTrue(VMSourceUnwindPrivate::SetupEngine(*Engine.Ptr, Host, Builder), VMSourceUnwindPrivate::GLastError));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
		ASSERT_THAT(AreEqual(1, VMSourceUnwindPrivate::GMarkTrace.Num()));
		ASSERT_THAT(AreEqual(1, VMSourceUnwindPrivate::GMarkTrace[0]));
		ASSERT_THAT(IsTrue(VMSourceUnwindPrivate::RecoverNinetySeven(Context, Recovery)));
		Context->Release();
		if (Snapshot) Snapshot->Release();
	}

	TEST_METHOD(FailedConstructorNeverRunsCompleteDestructor)
	{
		VMSourceUnwindPrivate::ResetTrace();
		VMSourceUnwindPrivate::FUnwindHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput Input(
			FString(VMSourceUnwindPrivate::BoxSource())
			+ TEXT("class Broken { Broken() { Box Local(1); Host::Fail(); } ~Broken() { Host::Mark(99); } } ")
			+ TEXT("int Run() { Broken X(); return 0; } int Recovery() { return 97; }"),
			Host.Fixture.Context, Host.Fixture.Set.Get());
		asCBuilder Builder(Input.Sources, Input.Options);
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(IsTrue(VMSourceUnwindPrivate::EmitImage(Builder, Image), VMSourceUnwindPrivate::GLastError));
		asCScriptFunction* Run = FindSourceFunction(Builder, "Run");
		asCScriptFunction* Recovery = FindSourceFunction(Builder, "Recovery");
		FNativeEngineOwner Engine;
		ASSERT_THAT(IsTrue(VMSourceUnwindPrivate::SetupEngine(*Engine.Ptr, Host, Builder), VMSourceUnwindPrivate::GLastError));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
		ASSERT_THAT(AreEqual(1, VMSourceUnwindPrivate::GMarkTrace.Num()));
		ASSERT_THAT(AreEqual(1, VMSourceUnwindPrivate::GMarkTrace[0]));
		ASSERT_THAT(IsTrue(VMSourceUnwindPrivate::RecoverNinetySeven(Context, Recovery)));
		Context->Release();
		if (Snapshot) Snapshot->Release();
	}

	TEST_METHOD(NestedMemberPartialConstructionOrder)
	{
		VMSourceUnwindPrivate::ResetTrace();
		VMSourceUnwindPrivate::FUnwindHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput Input(
			FString(VMSourceUnwindPrivate::BoxSource())
			+ TEXT("class Inner { Box Completed; Inner() { Completed = Box(2); Host::Fail(); } } ")
			+ TEXT("class Outer { Box First; Inner Nested; Outer() { First = Box(1); Nested = Inner(); } } ")
			+ TEXT("int Run() { Outer Value(); return 0; } int Recovery() { return 97; }"),
			Host.Fixture.Context, Host.Fixture.Set.Get());
		asCBuilder Builder(Input.Sources, Input.Options);
		TUniquePtr<asCByteCodeImage> Image;
		if (!VMSourceUnwindPrivate::EmitImage(Builder, Image))
		{
			ASSERT_THAT(IsTrue(false, VMSourceUnwindPrivate::GLastError));
			return;
		}
		asCScriptFunction* Run = FindSourceFunction(Builder, "Run");
		asCScriptFunction* Recovery = FindSourceFunction(Builder, "Recovery");
		FNativeEngineOwner Engine;
		ASSERT_THAT(IsTrue(VMSourceUnwindPrivate::SetupEngine(*Engine.Ptr, Host, Builder), VMSourceUnwindPrivate::GLastError));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
		ASSERT_THAT(IsTrue(VMSourceUnwindPrivate::GMarkTrace.Num() >= 2));
		ASSERT_THAT(AreEqual(2, VMSourceUnwindPrivate::GMarkTrace[0]));
		ASSERT_THAT(AreEqual(1, VMSourceUnwindPrivate::GMarkTrace[1]));
		ASSERT_THAT(IsTrue(VMSourceUnwindPrivate::RecoverNinetySeven(Context, Recovery)));
		Context->Release();
		if (Snapshot) Snapshot->Release();
	}

	TEST_METHOD(StackLimitReportsSourceLocationThenRecovery)
	{
		VMSourceUnwindPrivate::ResetTrace();
		VMSourceUnwindPrivate::FUnwindHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput Input(
			FString(VMSourceUnwindPrivate::BoxSource())
			+ TEXT("int Rec(int N) { Box Hold(N); return Rec(N + 1); } int Recovery() { return 97; }"),
			Host.Fixture.Context, Host.Fixture.Set.Get());
		asCBuilder Builder(Input.Sources, Input.Options);
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(IsTrue(VMSourceUnwindPrivate::EmitImage(Builder, Image), VMSourceUnwindPrivate::GLastError));
		asCScriptFunction* Rec = FindSourceFunction(Builder, "Rec");
		asCScriptFunction* Recovery = FindSourceFunction(Builder, "Recovery");
		FNativeEngineOwner Engine;
		ASSERT_THAT(IsTrue(VMSourceUnwindPrivate::SetupEngine(*Engine.Ptr, Host, Builder), VMSourceUnwindPrivate::GLastError));
		Engine.Ptr->SetEngineProperty(asEP_MAX_STACK_SIZE, 256);
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Rec)));
		Context->SetArgDWord(0, 1);
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
		int Column = 0;
		const char* Section = nullptr;
		const int Line = Context->GetExceptionLineNumber(&Column, &Section);
		ASSERT_THAT(IsTrue(Line > 0));
		ASSERT_THAT(IsNotNull(Section));
		ASSERT_THAT(IsTrue(VMSourceUnwindPrivate::GMarkTrace.Num() > 0));
		ASSERT_THAT(IsTrue(VMSourceUnwindPrivate::RecoverNinetySeven(Context, Recovery)));
		Context->Release();
		if (Snapshot) Snapshot->Release();
	}

	TEST_METHOD(SourceRecordsSurviveEncodeDecodeAndProducerRelease)
	{
		VMSourceUnwindPrivate::ResetTrace();
		VMSourceUnwindPrivate::FUnwindHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		TArray<uint8> Bytes;
		TUniquePtr<asCDefinitions> SourceDefs;
		asSStableKey RunKey;
		asSStableKey RecoveryKey;
		{
			FSourceExecutionInput Input(
				FString(VMSourceUnwindPrivate::BoxSource())
				+ TEXT("int Run() { Box A(1); Host::Fail(); return 0; } int Recovery() { return 97; }"),
				Host.Fixture.Context, Host.Fixture.Set.Get());
			asCBuilder Builder(Input.Sources, Input.Options);
			TUniquePtr<asCByteCodeImage> Image;
			ASSERT_THAT(IsTrue(VMSourceUnwindPrivate::EmitImage(Builder, Image), VMSourceUnwindPrivate::GLastError));
			asCScriptFunction* Run = FindSourceFunction(Builder, "Run");
			asCScriptFunction* Recovery = FindSourceFunction(Builder, "Recovery");
			ASSERT_THAT(IsNotNull(Run));
			ASSERT_THAT(IsNotNull(Recovery));
			RunKey = Run->GetStableKey();
			RecoveryKey = Recovery->GetStableKey();
			SourceDefs = Builder.TakeDefinitions();
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
			ASSERT_THAT(IsTrue(Image->Functions.Num() > 0 && Image->Functions[0].Source.Num() > 0));
		}
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*EngineB.Ptr, MoveTemp(Host.Fixture.Set))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			EngineB.Ptr->BindNativeFunction(Host.Mark->GetStableKey(), asFUNCTION(VMSourceUnwindPrivate::GenericMark), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			EngineB.Ptr->BindNativeFunction(Host.Fail->GetStableKey(), asFUNCTION(VMSourceUnwindPrivate::GenericFail), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*EngineB.Ptr, MoveTemp(SourceDefs))));
		asCScriptFunction* RunB = EngineB.Ptr->FindFunction(RunKey);
		asCScriptFunction* RecoveryB = EngineB.Ptr->FindFunction(RecoveryKey);
		ASSERT_THAT(IsNotNull(RunB));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*EngineB.Ptr, *Decoded, Snapshot)));
		asIScriptContext* Context = EngineB.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(RunB)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
		int Column = 0;
		const char* Section = nullptr;
		ASSERT_THAT(IsTrue(Context->GetExceptionLineNumber(&Column, &Section) > 0));
		ASSERT_THAT(IsTrue(VMSourceUnwindPrivate::RecoverNinetySeven(Context, RecoveryB)));
		Context->Release();
		if (Snapshot) Snapshot->Release();
	}
};

#endif
