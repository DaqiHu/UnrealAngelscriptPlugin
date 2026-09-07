#include "CQTest.h"
#include "NativeSourceExecutionTestSupport.h"
#include "as_datatype.h"
#include "as_generic.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	TArray<int32> GMarkTrace;
	int32 GNativeAddCalls = 0;
	int32 GNativeLastA = 0;
	int32 GNativeLastB = 0;

	void ResetNativeCallTrace()
	{
		GMarkTrace.Reset();
		GNativeAddCalls = 0;
		GNativeLastA = 0;
		GNativeLastB = 0;
	}

	void GenericMark(asIScriptGeneric* Generic)
	{
		const int32 Value = static_cast<int32>(Generic->GetArgDWord(0));
		GMarkTrace.Add(Value);
		Generic->SetReturnDWord(static_cast<asDWORD>(Value));
	}

	void GenericPack(asIScriptGeneric* Generic)
	{
		const int32 A = static_cast<int32>(Generic->GetArgDWord(0));
		const int32 B = static_cast<int32>(Generic->GetArgDWord(1));
		Generic->SetReturnDWord(static_cast<asDWORD>(10 * A + B));
	}

	void GenericAdd(asIScriptGeneric* Generic)
	{
		const int32 A = static_cast<int32>(Generic->GetArgDWord(0));
		const int32 B = static_cast<int32>(Generic->GetArgDWord(1));
		++GNativeAddCalls;
		GNativeLastA = A;
		GNativeLastB = B;
		Generic->SetReturnDWord(static_cast<asDWORD>(A + B));
	}

	void GenericThrow(asIScriptGeneric* Generic)
	{
		(void)Generic;
		asGetActiveContext()->SetException("native failed");
	}

	asSMetadataParameter MakeIntParam(const char* Name)
	{
		asSMetadataParameter Parameter;
		Parameter.Name = Name;
		Parameter.Type = asCDataType::CreatePrimitive(ttInt, false);
		return Parameter;
	}

	struct FCallHost
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Add = nullptr;
		asCScriptFunction* Mark = nullptr;
		asCScriptFunction* Pack = nullptr;
		asCScriptFunction* Fail = nullptr;

		bool Initialize()
		{
			Fixture.Scope = TEXT("vm-source-host");
			asSMetadataParameter AddParams[2] = {MakeIntParam("A"), MakeIntParam("B")};
			AddParams[0].Type = Fixture.Image->Primitive(ttInt);
			AddParams[1].Type = Fixture.Image->Primitive(ttInt);
			Add = Fixture.DefineFunction(TEXT("Add"), TEXT("Host"), asFUNC_SYSTEM, Fixture.Image->Primitive(ttInt), MakeArrayView(AddParams, 2));
			asSMetadataParameter MarkParam = MakeIntParam("n");
			MarkParam.Type = Fixture.Image->Primitive(ttInt);
			Mark = Fixture.DefineFunction(TEXT("Mark"), TEXT("Host"), asFUNC_SYSTEM, Fixture.Image->Primitive(ttInt), MakeArrayView(&MarkParam, 1));
			asSMetadataParameter PackParams[2] = {MakeIntParam("a"), MakeIntParam("b")};
			PackParams[0].Type = Fixture.Image->Primitive(ttInt);
			PackParams[1].Type = Fixture.Image->Primitive(ttInt);
			Pack = Fixture.DefineFunction(TEXT("Pack"), TEXT("Host"), asFUNC_SYSTEM, Fixture.Image->Primitive(ttInt), MakeArrayView(PackParams, 2));
			Fail = Fixture.DefineFunction(TEXT("Fail"), TEXT("Host"), asFUNC_SYSTEM, Fixture.Image->Primitive(ttInt), {});
			return Add && Mark && Pack && Fail && Fixture.FinalizeAndFreeze();
		}
	};

	bool BindCallHost(asCScriptEngine& Engine, const FCallHost& Host)
	{
		return Engine.BindNativeFunction(Host.Add->GetStableKey(), asFUNCTION(GenericAdd), asCALL_GENERIC) == asEByteCodeLinkStatus::Succeeded
			&& Engine.BindNativeFunction(Host.Mark->GetStableKey(), asFUNCTION(GenericMark), asCALL_GENERIC) == asEByteCodeLinkStatus::Succeeded
			&& Engine.BindNativeFunction(Host.Pack->GetStableKey(), asFUNCTION(GenericPack), asCALL_GENERIC) == asEByteCodeLinkStatus::Succeeded
			&& Engine.BindNativeFunction(Host.Fail->GetStableKey(), asFUNCTION(GenericThrow), asCALL_GENERIC) == asEByteCodeLinkStatus::Succeeded;
	}

	bool ExecuteSourceFunction(asCBuilder& Builder, asCScriptFunction* Function, int32& OutValue, asCScriptEngine* Engine = nullptr, int32 Arg0 = MAX_int32)
	{
		asSByteCodeEmissionResult Emitted = asCByteCodeEmitter::Emit(Builder, *Function);
		if (Emitted.Status != asEByteCodeImageStatus::Succeeded || !Emitted.Image.Get())
		{
			return false;
		}
		FNativeEngineOwner Owned;
		asCScriptEngine* UseEngine = Engine ? Engine : Owned.Ptr;
		if (UseEngine->RegisterMetadataImage(Builder.GetDefinitions()) != asEMetadataRegistrationResult::Succeeded
			&& UseEngine->RegisterMetadataImage(Builder.GetDefinitions()) != asEMetadataRegistrationResult::AlreadyRegistered)
		{
			return false;
		}
		asCExecutableSnapshot* Snapshot = nullptr;
		if (UseEngine->LinkByteCodeImage(*Emitted.Image, Snapshot) != asEByteCodeLinkStatus::Succeeded)
		{
			return false;
		}
		asIScriptContext* Context = UseEngine->CreateContext();
		if (Context->Prepare(Function) != 0)
		{
			Context->Release();
			if (Snapshot) Snapshot->Release();
			return false;
		}
		if (Arg0 != MAX_int32)
		{
			Context->SetArgDWord(0, static_cast<asDWORD>(Arg0));
		}
		const int Status = Context->Execute();
		if (Status != asEXECUTION_FINISHED)
		{
			Context->Release();
			if (Snapshot) Snapshot->Release();
			return false;
		}
		OutValue = static_cast<int32>(Context->GetReturnDWord());
		Context->Release();
		Snapshot->Release();
		return true;
	}
}

TEST_CLASS_WITH_FLAGS(VMSourceCalls, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(SourceLiteralReturnExecutes)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return 42; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteSourceFunction(Builder, Function, Value)));
		ASSERT_THAT(AreEqual(42, Value));
	}

	TEST_METHOD(SourceCallerReturnsSeven)
	{
		FSourceExecutionInput Input(TEXT("int Callee() { return 7; } int Run() { return Callee(); }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteSourceFunction(Builder, Function, Value)));
		ASSERT_THAT(AreEqual(7, Value));
	}

	TEST_METHOD(SourceForwardCalleeAfterCallerReturnsSeven)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return Callee(); } int Callee() { return 7; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteSourceFunction(Builder, Function, Value)));
		ASSERT_THAT(AreEqual(7, Value));
	}

	TEST_METHOD(SourceFactorialZeroOneFive)
	{
		FSourceExecutionInput Input(TEXT("int Fact(int n) { if (n <= 1) return 1; return n * Fact(n - 1); }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Fact");
		ASSERT_THAT(IsNotNull(Function));
		asSByteCodeEmissionResult Emitted = asCByteCodeEmitter::Emit(Builder, *Function);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.GetDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 0)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 1)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 5)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(120, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(SourceOverloadsReturnDistinctMarkers)
	{
		FSourceExecutionInput Input(TEXT("int Marker(int x) { return 1; } int Marker(int x, int y) { return 2; } int Run() { return Marker(0) * 10 + Marker(0, 1); }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteSourceFunction(Builder, Function, Value)));
		ASSERT_THAT(AreEqual(12, Value));
	}

	TEST_METHOD(SourceCombineNamedAndDefaultIsThirtyFour)
	{
		FSourceExecutionInput Input(TEXT("int Combine(int A, int B = 4) { return 10 * A + B; } int Named() { return Combine(B:4, A:3); } int Defaulted() { return Combine(3); }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asCScriptFunction* Named = FindSourceFunction(Builder, "Named");
		asCScriptFunction* Defaulted = FindSourceFunction(Builder, "Defaulted");
		ASSERT_THAT(IsNotNull(Named));
		ASSERT_THAT(IsNotNull(Defaulted));
		asSByteCodeEmissionResult Emitted = asCByteCodeEmitter::Emit(Builder, *Named);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.GetDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Named)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(34, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Defaulted)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(34, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(SourcePackMarkPositionalAndNamedReturnTwelve)
	{
		FSourceExecutionInput Input(TEXT("int Mark(int n) { return n; } int Pack(int a, int b) { return 10 * a + b; } int Positional() { return Pack(Mark(1), Mark(2)); } int Named() { return Pack(b:Mark(2), a:Mark(1)); }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asCScriptFunction* Positional = FindSourceFunction(Builder, "Positional");
		asCScriptFunction* Named = FindSourceFunction(Builder, "Named");
		ASSERT_THAT(IsNotNull(Positional));
		ASSERT_THAT(IsNotNull(Named));
		asSByteCodeEmissionResult Emitted = asCByteCodeEmitter::Emit(Builder, *Positional);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.GetDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Positional)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(12, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Named)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(12, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(SourceFillAndBumpYieldFortyTwo)
	{
		FSourceExecutionInput Input(TEXT("void Fill(int&out X) { int T = 41; X = T; } void Bump(int&inout X) { int T = X + 1; X = T; } int Run() { int A = 0; Fill(A); Bump(A); return A; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteSourceFunction(Builder, Function, Value)));
		ASSERT_THAT(AreEqual(42, Value));
	}

	TEST_METHOD(SourceInRefPreservesCallerValue)
	{
		FSourceExecutionInput Input(TEXT("int Peek(const int&in X) { return X; } int Run() { int A = 7; int B = Peek(A); return A * 10 + B; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteSourceFunction(Builder, Function, Value)));
		ASSERT_THAT(AreEqual(77, Value));
	}

	TEST_METHOD(NativePackMarkTracesReverseFormal)
	{
		ResetNativeCallTrace();
		FCallHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput Input(TEXT("int Positional() { return Host::Pack(Host::Mark(1), Host::Mark(2)); } int Named() { return Host::Pack(b:Host::Mark(2), a:Host::Mark(1)); }"), Host.Fixture.Context, Host.Fixture.Image);
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Host.Fixture.Image)));
		ASSERT_THAT(IsTrue(BindCallHost(*Engine.Ptr, Host)));
		asCScriptFunction* PositionalFn = FindSourceFunction(Builder, "Positional");
		asCScriptFunction* NamedFn = FindSourceFunction(Builder, "Named");
		ASSERT_THAT(IsNotNull(PositionalFn));
		ASSERT_THAT(IsNotNull(NamedFn));
		asSByteCodeEmissionResult Emitted = asCByteCodeEmitter::Emit(Builder, *PositionalFn);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.GetDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(PositionalFn)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(12, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(2, GMarkTrace.Num()));
		ASSERT_THAT(AreEqual(2, GMarkTrace[0]));
		ASSERT_THAT(AreEqual(1, GMarkTrace[1]));
		ResetNativeCallTrace();
		ASSERT_THAT(AreEqual(0, Context->Prepare(NamedFn)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(12, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(2, GMarkTrace.Num()));
		ASSERT_THAT(AreEqual(2, GMarkTrace[0]));
		ASSERT_THAT(AreEqual(1, GMarkTrace[1]));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(NativeHostAddTwentyAndTwentyTwo)
	{
		ResetNativeCallTrace();
		FCallHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput Input(TEXT("int Run() { return Host::Add(20, 22); }"), Host.Fixture.Context, Host.Fixture.Image);
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Host.Fixture.Image)));
		ASSERT_THAT(IsTrue(BindCallHost(*Engine.Ptr, Host)));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteSourceFunction(Builder, Function, Value, Engine.Ptr)));
		ASSERT_THAT(AreEqual(42, Value));
		ASSERT_THAT(AreEqual(1, GNativeAddCalls));
		ASSERT_THAT(AreEqual(20, GNativeLastA));
		ASSERT_THAT(AreEqual(22, GNativeLastB));
	}

	TEST_METHOD(TypedDefaultMarkRunsPerCallSite)
	{
		ResetNativeCallTrace();
		FCallHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput Input(TEXT("int Marker() { return Host::Mark(4); } int Combine(int A, int B = Marker()) { return 10 * A + B; } int Run() { return Combine(3) + Combine(3); }"), Host.Fixture.Context, Host.Fixture.Image);
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		const bool Built = Builder.RunThrough();
		ASSERT_THAT(IsTrue(Built, DescribeBuilderFailure(Builder)));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Host.Fixture.Image)));
		ASSERT_THAT(IsTrue(BindCallHost(*Engine.Ptr, Host)));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteSourceFunction(Builder, Function, Value, Engine.Ptr)));
		ASSERT_THAT(AreEqual(68, Value));
		ASSERT_THAT(AreEqual(2, GMarkTrace.Num()));
		ASSERT_THAT(AreEqual(4, GMarkTrace[0]));
		ASSERT_THAT(AreEqual(4, GMarkTrace[1]));
	}

	TEST_METHOD(NativeSetExceptionReportsVmException)
	{
		FCallHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput Input(TEXT("int Run() { return Host::Fail(); }"), Host.Fixture.Context, Host.Fixture.Image);
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Host.Fixture.Image)));
		ASSERT_THAT(IsTrue(BindCallHost(*Engine.Ptr, Host)));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		asSByteCodeEmissionResult Emitted = asCByteCodeEmitter::Emit(Builder, *Function);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.GetDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
		ASSERT_THAT(IsTrue(FCStringAnsi::Strstr(Context->GetExceptionString(), "native failed") != nullptr));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(WrongNamedArgumentPublishesNoImage)
	{
		FSourceExecutionInput Input(TEXT("int Combine(int A, int B) { return 10 * A + B; } int Run() { return Combine(C:1, A:2); }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsFalse(Builder.RunThrough()));
	}

	TEST_METHOD(MissingOverloadPublishesNoImage)
	{
		FSourceExecutionInput Input(TEXT("int Marker(int x) { return 1; } int Run() { return Marker(true); }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsFalse(Builder.RunThrough()));
	}

	TEST_METHOD(OutTemporaryPublishesNoImage)
	{
		FSourceExecutionInput Input(TEXT("void Fill(int&out X) { X = 41; } int Run() { Fill(1); return 0; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsFalse(Builder.RunThrough()));
	}

	TEST_METHOD(MissingNativeBindingRejectsLink)
	{
		ResetNativeCallTrace();
		FCallHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput Input(TEXT("int Run() { return Host::Add(20, 22); }"), Host.Fixture.Context, Host.Fixture.Image);
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		asSByteCodeEmissionResult Emitted = asCByteCodeEmitter::Emit(Builder, *Function);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Host.Fixture.Image)));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.GetDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::MissingBinding, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
		ASSERT_THAT(IsNull(Snapshot));
		ASSERT_THAT(AreEqual(0, GNativeAddCalls));
	}

	TEST_METHOD(UnsupportedThiscallBindingRejected)
	{
		ResetNativeCallTrace();
		FCallHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Host.Fixture.Image)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::InvalidArgument,
			Engine.Ptr->BindNativeFunction(Host.Add->GetStableKey(), asFUNCTION(GenericAdd), asCALL_THISCALL)));
		ASSERT_THAT(AreEqual(0, GNativeAddCalls));
	}
};

#endif
