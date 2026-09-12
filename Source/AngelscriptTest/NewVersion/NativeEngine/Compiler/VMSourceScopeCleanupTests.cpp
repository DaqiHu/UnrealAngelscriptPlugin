#include "CQTest.h"
#include "NativeSourceExecutionTestSupport.h"
#include "as_datatype.h"
#include "as_generic.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace VMSourceScopeCleanupPrivate
{
	TArray<int32> GScopeCleanupTrace;
	int32 GCtorCount = 0;
	int32 GDtorCount = 0;

	void ResetTraces()
	{
		GScopeCleanupTrace.Reset();
		GCtorCount = 0;
		GDtorCount = 0;
	}

	void GenericMark(asIScriptGeneric* Generic)
	{
		const int32 Value = static_cast<int32>(Generic->GetArgDWord(0));
		GScopeCleanupTrace.Add(Value);
		Generic->SetReturnDWord(static_cast<asDWORD>(Value));
	}

	asSMetadataParameter MakeIntParam(const char* Name)
	{
		asSMetadataParameter Parameter;
		Parameter.Name = Name;
		Parameter.Type = asCDataType::CreatePrimitive(ttInt, false);
		return Parameter;
	}

	struct FCleanupHost
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Mark = nullptr;

		bool Initialize()
		{
			Fixture.Scope = TEXT("vm-source-scope-host");
			asSMetadataParameter Param = MakeIntParam("n");
			Param.Type = Fixture.Set->Primitive(ttInt);
			Mark = Fixture.DefineFunction(TEXT("Mark"), TEXT("Host"), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), MakeArrayView(&Param, 1));
			return Mark && Fixture.FinalizeAndFreeze();
		}
	};

	const TCHAR* BoxSource()
	{
		return TEXT("class Box { int Id; Box(int Initial) { Id = Initial; } ~Box() { Host::Mark(Id); } } ");
	}

	FString GLastExecuteError;

	bool ExecuteRun(asCBuilder& Builder, asCScriptEngine& Engine, int32& OutValue)
	{
		GLastExecuteError.Reset();
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		if (!Function)
		{
			GLastExecuteError = TEXT("missing-run");
			return false;
		}
		FSessionEmission Emitted = EmitFromSession(Builder);
		if (Emitted.EmissionStatus != asEByteCodeEmissionStatus::Succeeded || !Emitted.Image.Get())
		{
			GLastExecuteError = FString::Printf(TEXT("emit-%d"), int32(Emitted.EmissionStatus));
			return false;
		}
		if (RegisterCompiledDefinitions(Engine, Builder) != asEMetadataRegistrationResult::Succeeded
			&& RegisterCompiledDefinitions(Engine, Builder) != asEMetadataRegistrationResult::AlreadyRegistered)
		{
			GLastExecuteError = TEXT("register-defs");
			return false;
		}
		asCExecutableSnapshot* Snapshot = nullptr;
		const asEByteCodeLinkStatus Link = asLinkByteCodeImage(Engine, *Emitted.Image, Snapshot);
		if (Link != asEByteCodeLinkStatus::Succeeded)
		{
			GLastExecuteError = FString::Printf(TEXT("link-%d"), int32(Link));
			return false;
		}
		asIScriptContext* Context = Engine.CreateContext();
		const int Prepare = Context->Prepare(Function);
		const int Exec = Prepare == 0 ? Context->Execute() : Prepare;
		if (Prepare != 0 || Exec != asEXECUTION_FINISHED)
		{
			GLastExecuteError = FString::Printf(TEXT("prep-%d-exec-%d-%s"), Prepare, Exec,
				Prepare == 0 && Context->GetExceptionString() ? UTF8_TO_TCHAR(Context->GetExceptionString()) : TEXT(""));
			Context->Release();
			if (Snapshot)
			{
				Snapshot->Release();
			}
			return false;
		}
		OutValue = static_cast<int32>(Context->GetReturnDWord());
		Context->Release();
		Snapshot->Release();
		return true;
	}

	FString GLastCleanupError;

	bool RunCleanup(const FString& Body, int32& OutValue)
	{
		GLastCleanupError.Reset();
		FCleanupHost Host;
		if (!Host.Initialize())
		{
			GLastCleanupError = TEXT("host-init");
			return false;
		}
		FSourceExecutionInput Input(FString(BoxSource()) + Body, Host.Fixture.Context, Host.Fixture.Set.Get());
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		if (!Builder.RunThrough())
		{
			GLastCleanupError = TEXT("compile:") + DescribeBuilderFailure(Builder);
			return false;
		}
		FNativeEngineOwner Engine;
		if (RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Host.Fixture.Set)) != asEMetadataRegistrationResult::Succeeded)
		{
			GLastCleanupError = TEXT("register");
			return false;
		}
		if (Engine.Ptr->BindNativeFunction(Host.Mark->GetStableKey(), asFUNCTION(GenericMark), asCALL_GENERIC) != asEByteCodeLinkStatus::Succeeded)
		{
			GLastCleanupError = TEXT("bind");
			return false;
		}
		if (!ExecuteRun(Builder, *Engine.Ptr, OutValue))
		{
			GLastCleanupError = TEXT("execute:") + GLastExecuteError;
			return false;
		}
		return true;
	}
}

TEST_CLASS_WITH_FLAGS(VMSourceScopeCleanup, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ConditionalReturnsEachDestroyOuterLocalOnce)
	{
		using namespace VMSourceScopeCleanupPrivate;
		for (int32 Flag : {0, 1})
		{
			ResetTraces();
			int32 Value = 0;
			const FString Source = FString::Printf(TEXT("int Run() { Box A(1); if (%d == 0) return 7; return 9; }"), Flag);
			ASSERT_THAT(IsTrue(RunCleanup(Source, Value), GLastCleanupError));
			ASSERT_THAT(AreEqual(Flag == 0 ? 7 : 9, Value));
			ASSERT_THAT(AreEqual(1, GScopeCleanupTrace.Num()));
			ASSERT_THAT(AreEqual(1, GScopeCleanupTrace[0]));
		}
	}

	TEST_METHOD(IntegerControlStillSeven)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return 1+2*3; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		FNativeEngineOwner Engine;
		int32 Value = 0;
		ASSERT_THAT(IsTrue(VMSourceScopeCleanupPrivate::ExecuteRun(Builder, *Engine.Ptr, Value)));
		ASSERT_THAT(AreEqual(7, Value));
	}

	TEST_METHOD(NestedInnerDestroysBeforeMarkerThenOuter)
	{
		using namespace VMSourceScopeCleanupPrivate;
		ResetTraces();
		int32 Value = 0;
		ASSERT_THAT(IsTrue(RunCleanup(TEXT("int Run() { Box A(1); { Box B(2); } return Host::Mark(3); }"), Value)));
		ASSERT_THAT(AreEqual(3, Value));
		ASSERT_THAT(AreEqual(3, GScopeCleanupTrace.Num()));
		ASSERT_THAT(AreEqual(2, GScopeCleanupTrace[0]));
		ASSERT_THAT(AreEqual(3, GScopeCleanupTrace[1]));
		ASSERT_THAT(AreEqual(1, GScopeCleanupTrace[2]));
	}

	TEST_METHOD(UntakenConstructionBranchHasNoDestructor)
	{
		using namespace VMSourceScopeCleanupPrivate;
		ResetTraces();
		int32 Value = 0;
		ASSERT_THAT(IsTrue(RunCleanup(TEXT("int Run() { Box A(1); if (1 == 0) { Box B(2); } return Host::Mark(3); }"), Value), GLastCleanupError));
		ASSERT_THAT(AreEqual(3, Value));
		ASSERT_THAT(AreEqual(2, GScopeCleanupTrace.Num()));
		ASSERT_THAT(AreEqual(3, GScopeCleanupTrace[0]));
		ASSERT_THAT(AreEqual(1, GScopeCleanupTrace[1]));
	}

	TEST_METHOD(ForInitSurvivesContinueBodyDestroyedEachIteration)
	{
		using namespace VMSourceScopeCleanupPrivate;
		ResetTraces();
		int32 Value = 0;
		ASSERT_THAT(IsTrue(RunCleanup(
			TEXT("int Run() { int I = 0; for (Box Init(10); I < 2; I = I + 1) { Box Body(20 + I); continue; } return 0; }"),
			Value), GLastCleanupError));
		ASSERT_THAT(AreEqual(0, Value));
		ASSERT_THAT(AreEqual(3, GScopeCleanupTrace.Num()));
		ASSERT_THAT(AreEqual(20, GScopeCleanupTrace[0]));
		ASSERT_THAT(AreEqual(21, GScopeCleanupTrace[1]));
		ASSERT_THAT(AreEqual(10, GScopeCleanupTrace[2]));
	}

	TEST_METHOD(BreakDestroysBodyBeforeInitializer)
	{
		using namespace VMSourceScopeCleanupPrivate;
		ResetTraces();
		int32 Value = 0;
		ASSERT_THAT(IsTrue(RunCleanup(
			TEXT("int Run() { int I = 0; for (Box Init(10); I < 5; I = I + 1) { Box Body(20); break; } return 0; }"),
			Value), GLastCleanupError));
		ASSERT_THAT(AreEqual(0, Value));
		ASSERT_THAT(AreEqual(2, GScopeCleanupTrace.Num()));
		ASSERT_THAT(AreEqual(20, GScopeCleanupTrace[0]));
		ASSERT_THAT(AreEqual(10, GScopeCleanupTrace[1]));
	}

	TEST_METHOD(NestedWhileContinueUsesBackedge)
	{
		using namespace VMSourceScopeCleanupPrivate;
		ResetTraces();
		int32 Value = 0;
		ASSERT_THAT(IsTrue(RunCleanup(
			TEXT("int Run() { int N = 0; while (N < 2) { Box Body(30 + N); N = N + 1; continue; } return N; }"),
			Value), GLastCleanupError));
		ASSERT_THAT(AreEqual(2, Value));
		ASSERT_THAT(AreEqual(2, GScopeCleanupTrace.Num()));
		ASSERT_THAT(AreEqual(30, GScopeCleanupTrace[0]));
		ASSERT_THAT(AreEqual(31, GScopeCleanupTrace[1]));
	}

	TEST_METHOD(NestedForBreakSelectsNearest)
	{
		using namespace VMSourceScopeCleanupPrivate;
		ResetTraces();
		int32 Value = 0;
		ASSERT_THAT(IsTrue(RunCleanup(
			TEXT("int Run() { int Outer = 0; for (Box A(1); Outer < 2; Outer = Outer + 1) { int Inner = 0; for (Box B(2); Inner < 2; Inner = Inner + 1) { Box C(3); break; } } return 0; }"),
			Value), GLastCleanupError));
		ASSERT_THAT(AreEqual(0, Value));
		ASSERT_THAT(AreEqual(5, GScopeCleanupTrace.Num()));
		ASSERT_THAT(AreEqual(3, GScopeCleanupTrace[0]));
		ASSERT_THAT(AreEqual(2, GScopeCleanupTrace[1]));
		ASSERT_THAT(AreEqual(3, GScopeCleanupTrace[2]));
		ASSERT_THAT(AreEqual(2, GScopeCleanupTrace[3]));
		ASSERT_THAT(AreEqual(1, GScopeCleanupTrace[4]));
	}

	TEST_METHOD(SwitchBreakSelectsNearest)
	{
		using namespace VMSourceScopeCleanupPrivate;
		ResetTraces();
		int32 Value = 0;
		ASSERT_THAT(IsTrue(RunCleanup(
			TEXT("int Run() { Box A(1); switch (1) { case 1: { Box B(2); break; } } return Host::Mark(3); }"),
			Value), GLastCleanupError));
		ASSERT_THAT(AreEqual(3, Value));
		ASSERT_THAT(AreEqual(3, GScopeCleanupTrace.Num()));
		ASSERT_THAT(AreEqual(2, GScopeCleanupTrace[0]));
		ASSERT_THAT(AreEqual(3, GScopeCleanupTrace[1]));
		ASSERT_THAT(AreEqual(1, GScopeCleanupTrace[2]));
	}

	TEST_METHOD(EarlyReturnPreservesValueThroughDestructors)
	{
		using namespace VMSourceScopeCleanupPrivate;
		ResetTraces();
		int32 Value = 0;
		ASSERT_THAT(IsTrue(RunCleanup(
			TEXT("int Run() { Box A(1); { Box B(2); return 42; } }"),
			Value)));
		ASSERT_THAT(AreEqual(42, Value));
		ASSERT_THAT(AreEqual(2, GScopeCleanupTrace.Num()));
		ASSERT_THAT(AreEqual(2, GScopeCleanupTrace[0]));
		ASSERT_THAT(AreEqual(1, GScopeCleanupTrace[1]));
	}

	TEST_METHOD(NoDuplicateDestructionOnNestedScopes)
	{
		using namespace VMSourceScopeCleanupPrivate;
		ResetTraces();
		int32 Value = 0;
		ASSERT_THAT(IsTrue(RunCleanup(
			TEXT("int Run() { Box A(1); { Box B(2); } { Box C(4); } return Host::Mark(3); }"),
			Value)));
		ASSERT_THAT(AreEqual(3, Value));
		ASSERT_THAT(AreEqual(4, GScopeCleanupTrace.Num()));
		ASSERT_THAT(AreEqual(2, GScopeCleanupTrace[0]));
		ASSERT_THAT(AreEqual(4, GScopeCleanupTrace[1]));
		ASSERT_THAT(AreEqual(3, GScopeCleanupTrace[2]));
		ASSERT_THAT(AreEqual(1, GScopeCleanupTrace[3]));
	}
};

#endif
