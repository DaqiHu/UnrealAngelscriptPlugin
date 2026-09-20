#include "CQTest.h"
#include "NativeSourceExecutionTestSupport.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(VMSourceControlFlow, "Angelscript.UnitTest.NativeEngine.SourceExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	asCScriptFunction* FindRun(asCBuilder& Builder)
	{
		for (asCScriptFunction* Candidate : Builder.GetDefinitions()->GetFunctions())
		{
			if (Candidate && Candidate->name == "Run") return Candidate;
		}
		return nullptr;
	}

public:
	TEST_METHOD(SourceIfElseSelectsBranch)
	{
		FSourceExecutionInput Input(TEXT("int Run(int x) { if (x == 1) return 1; else if (x == 2) return 2; else return 3; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = FindRun(Builder);
		ASSERT_THAT(IsNotNull(Function));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 1)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 2)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 9)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(SourceWhileSumOneToTenIsFiftyFive)
	{
		FSourceExecutionInput Input(TEXT("int Run() { int i = 1; int sum = 0; while (i <= 10) { sum = sum + i; i = i + 1; } return sum; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = FindRun(Builder);
		ASSERT_THAT(IsNotNull(Function));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(55, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(SourceForSkipTwoBreakFiveGivesEight)
	{
		FSourceExecutionInput Input(TEXT(
			"int Run() { int s = 0; for (int i = 0; i <= 5; i = i + 1) { if (i == 2) continue; if (i == 5) break; s = s + i; } return s; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = FindRun(Builder);
		ASSERT_THAT(IsNotNull(Function));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(8, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(SourceWhileZeroVersusDoOnce)
	{
		FSourceExecutionInput Input(TEXT(
			"int WhileZero() { int n = 0; while (false) { n = n + 1; } return n; } "
			"int DoOnce() { int n = 0; do { n = n + 1; } while (false); return n; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* WhileZero = FindSourceFunction(Builder, "WhileZero");
		asCScriptFunction* DoOnce = FindSourceFunction(Builder, "DoOnce");
		ASSERT_THAT(IsNotNull(WhileZero));
		ASSERT_THAT(IsNotNull(DoOnce));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(WhileZero)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(DoOnce)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(SourceSwitchFallthroughAndDefault)
	{
		FSourceExecutionInput Input(TEXT(
			"int Run(int x) { int s = 0; switch (x) { case 1: s = s + 1; fallthrough; case 2: s = s + 2; break; default: s = s + 4; } return s; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = FindRun(Builder);
		ASSERT_THAT(IsNotNull(Function));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 1)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 2)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 9)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(4, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(SourceLazyAndOrTernarySkipSideEffects)
	{
		FSourceExecutionInput Input(TEXT(
			"int AndCount(int flag) { int c = 0; bool t = (flag != 0) && ((c = c + 1) == 1); return c; } "
			"int OrCount(int flag) { int c = 0; bool t = (flag != 0) || ((c = c + 1) == 1); return c; } "
			"int TernaryCount(int flag) { int c = 0; int r = flag != 0 ? (c = 1) : (c = 2); return c; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* AndCount = FindSourceFunction(Builder, "AndCount");
		asCScriptFunction* OrCount = FindSourceFunction(Builder, "OrCount");
		asCScriptFunction* TernaryCount = FindSourceFunction(Builder, "TernaryCount");
		ASSERT_THAT(IsNotNull(AndCount));
		ASSERT_THAT(IsNotNull(OrCount));
		ASSERT_THAT(IsNotNull(TernaryCount));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(AndCount)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 0)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(AndCount)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 1)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(OrCount)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 1)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(OrCount)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 0)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(TernaryCount)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 1)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(TernaryCount)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 0)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(SourceLazyAndSkipsZeroDivisor)
	{
		FSourceExecutionInput Input(TEXT(
			"int Run(int flag) { int z = 0; bool t = (flag != 0) && ((1 / z) == 1); return t ? 1 : 0; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = FindRun(Builder);
		ASSERT_THAT(IsNotNull(Function));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 0)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 1)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(RuntimeN4LoopsSumZeroToThree)
	{
		FSourceExecutionInput Input(TEXT(
			"int WhileSum(int N){ int I = 0; int S = 0; while (I < N) { S = S + I; I = I + 1; } return S; } "
			"int DoSum(int N){ int I = 0; int S = 0; do { S = S + I; I = I + 1; } while (I < N); return S; } "
			"int ForSum(int N){ int S = 0; for (int I = 0; I < N; I = I + 1) { S = S + I; } return S; } "
			"int OmittedFor(int N){ int I = 0; int S = 0; for (; I < N;) { S = S + I; I = I + 1; } return S; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asCScriptFunction* WhileSum = FindSourceFunction(Builder, "WhileSum");
		asCScriptFunction* DoSum = FindSourceFunction(Builder, "DoSum");
		asCScriptFunction* ForSum = FindSourceFunction(Builder, "ForSum");
		asCScriptFunction* OmittedFor = FindSourceFunction(Builder, "OmittedFor");
		ASSERT_THAT(IsNotNull(WhileSum));
		ASSERT_THAT(IsNotNull(DoSum));
		ASSERT_THAT(IsNotNull(ForSum));
		ASSERT_THAT(IsNotNull(OmittedFor));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		auto Exec = [&](asCScriptFunction* Function)
		{
			ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
			ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 4)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(AreEqual(6, static_cast<int32>(Context->GetReturnDWord())));
		};
		Exec(WhileSum);
		Exec(DoSum);
		Exec(ForSum);
		Exec(OmittedFor);
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(RuntimeCommaKeepsLeftToRightLastValue)
	{
		// ParseExpression has no comma operator. The product's comma form is
		// for-increment lists (as_parser_statements.cpp) plus multi-declarators.
		FSourceExecutionInput Input(TEXT(
			"int IncrementList(){ int A = 0; int B = 0; for (int I = 0; I < 1; A = 1, A = A + 2, B = A + 4, I = I + 1) {} return B * 10 + A; } "
			"int MultiDecl(){ int First = 1, Second = First + 2; return Second * 10 + First; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asCScriptFunction* IncrementList = FindSourceFunction(Builder, "IncrementList");
		asCScriptFunction* MultiDecl = FindSourceFunction(Builder, "MultiDecl");
		ASSERT_THAT(IsNotNull(IncrementList));
		ASSERT_THAT(IsNotNull(MultiDecl));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(IncrementList)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(73, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(MultiDecl)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(31, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(RuntimeEarlyReturnSkipsLaterMark)
	{
		FSourceExecutionInput Input(TEXT(
			"int Run(){ int C = 0; if (true) return C; C = 9; return C; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}
};

#endif
