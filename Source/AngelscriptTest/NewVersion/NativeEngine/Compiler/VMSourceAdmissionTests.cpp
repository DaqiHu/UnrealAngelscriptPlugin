#include "CQTest.h"
#include "NativeSourceExecutionTestSupport.h"
#include "../Definitions/NativeDetachedDefinitionTestSupport.h"
#include "as_scriptfunction.h"
#include "frontend/as_ast_cast.h"
#include "frontend/as_compilation_session.h"
#include "frontend/as_decl.h"
#include "frontend/as_stmt.h"
#include "frontend/as_type_context.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace VMSourceAdmissionPrivate
{
	asCScriptFunction* RequireFunction(asCBuilder& Builder, const char* Name)
	{
		return FindSourceFunction(Builder, Name);
	}

	TArray<int> CopyParameterOffsets(const asCScriptFunction& Function)
	{
		TArray<int> Offsets;
		for (asUINT Index = 0; Index < Function.parameterOffsets.GetLength(); ++Index)
		{
			Offsets.Add(Function.parameterOffsets[Index]);
		}
		return Offsets;
	}
}

TEST_CLASS_WITH_FLAGS(VMSourceAdmission, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(NullSessionOrDefinitionsReturnInvalidInput)
	{
		asSByteCodeEmissionOptions Options;
		asSByteCodeEmissionResult NullSession = asCByteCodeEmitter::Emit(
			nullptr, nullptr, Options);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::InvalidInput, NullSession.EmissionStatus));
		ASSERT_THAT(IsNull(NullSession.Image.Get()));

		FSourceExecutionInput Input(TEXT("int Run() { return 42; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asSByteCodeEmissionResult NullDefs = asCByteCodeEmitter::Emit(
			Builder.GetCompilationSession(), nullptr, Options);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::InvalidInput, NullDefs.EmissionStatus));
		ASSERT_THAT(IsNull(NullDefs.Image.Get()));
	}

	TEST_METHOD(UnverifiedSessionReturnsNotReady)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return 42; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DeclarationsResolved)));
		const asCCompilationSession* Session = Builder.GetCompilationSession();
		ASSERT_THAT(IsNotNull(Session));
		ASSERT_THAT(IsFalse(Session->IsASTVerified()));
		asSByteCodeEmissionResult Emitted = asCByteCodeEmitter::Emit(Session, Builder.GetDefinitions().get(), asSByteCodeEmissionOptions());
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::NotReady, Emitted.EmissionStatus));
		ASSERT_THAT(IsNull(Emitted.Image.Get()));
	}

	TEST_METHOD(BodiesOnlySessionReturnsNotReady)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return 42; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::BodiesAnalyzed)));
		const asCCompilationSession* Session = Builder.GetCompilationSession();
		ASSERT_THAT(IsNotNull(Session));
		ASSERT_THAT(IsTrue(Session->AreBodiesAnalyzed()));
		ASSERT_THAT(IsFalse(Session->IsASTVerified()));
		asSByteCodeEmissionResult Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::NotReady, Emitted.EmissionStatus));
		ASSERT_THAT(IsNull(Emitted.Image.Get()));
	}

	TEST_METHOD(RecoverySessionReturnsInvalidInput)
	{
		FSourceExecutionInput Input(TEXT("asset Old of Payload { Values = { 1, { 2 } }; } int Run() { return 1; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsFalse(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DeclarationsCollected)));
		const asCCompilationSession* Session = Builder.GetCompilationSession();
		ASSERT_THAT(IsNotNull(Session));
		ASSERT_THAT(IsNotNull(Session->GetTranslationUnit()));
		bool bRecovery = Session->HasRecoveryNodes();
		for (AS_NAMESPACE_QUALIFIER asCDecl* Decl : Session->GetTranslationUnit()->GetDecls())
		{
			if (AS_NAMESPACE_QUALIFIER isa<AS_NAMESPACE_QUALIFIER asCRecoveryDecl>(Decl))
			{
				bRecovery = true;
				break;
			}
		}
		ASSERT_THAT(IsTrue(bRecovery));
		asSByteCodeEmissionResult Emitted = asCByteCodeEmitter::Emit(Session, Builder.GetDefinitions().get(), asSByteCodeEmissionOptions());
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::InvalidInput, Emitted.EmissionStatus));
		ASSERT_THAT(IsNull(Emitted.Image.Get()));
	}

	TEST_METHOD(UnfrozenDefinitionsReturnNotReady)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return 42; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsBuilt)));
		ASSERT_THAT(IsNotNull(Builder.GetDefinitions().get()));
		ASSERT_THAT(IsFalse(Builder.GetDefinitions()->IsFrozen()));
		const asCCompilationSession* Session = Builder.GetCompilationSession();
		ASSERT_THAT(IsTrue(Session->IsASTVerified()));
		asSByteCodeEmissionResult Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::NotReady, Emitted.EmissionStatus));
		ASSERT_THAT(IsNull(Emitted.Image.Get()));
	}

	TEST_METHOD(ForeignEqualKeyDefinitionsReturnInvalidInput)
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> TypeContext =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		FSourceExecutionInput Input(TEXT("int Run() { return 42; }"), TypeContext, nullptr);
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* SourceRun = VMSourceAdmissionPrivate::RequireFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(SourceRun));
		const asSStableKey ExpectedKey = SourceRun->GetStableKey();

		FDetachedDefinitionFixture Foreign;
		Foreign.Context = TypeContext;
		Foreign.Scope = TEXT("vm-source");
		asCScriptFunction* ForeignRun = Foreign.DefineFunction(
			TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Foreign.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsNotNull(ForeignRun));
		ASSERT_THAT(IsTrue(ExpectedKey == ForeignRun->GetStableKey()));
		ASSERT_THAT(IsTrue(Foreign.FinalizeAndFreeze()));

		asSByteCodeEmissionResult Emitted = asCByteCodeEmitter::Emit(
			Builder.GetCompilationSession(), Foreign.Image.get(), asSByteCodeEmissionOptions());
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::InvalidInput, Emitted.EmissionStatus));
		ASSERT_THAT(IsNull(Emitted.Image.Get()));
	}

	TEST_METHOD(UnsupportedTargetReturnsUnsupportedLowering)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return 42; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asSByteCodeEmissionOptions Options;
		Options.PointerByteSize = 4;
		asSByteCodeEmissionResult Emitted = EmitFromSession(Builder, Options);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::UnsupportedLowering, Emitted.EmissionStatus));
		ASSERT_THAT(IsNull(Emitted.Image.Get()));
	}

	TEST_METHOD(ResourceBudgetReturnsResourceLimit)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return 42; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asSByteCodeEmissionOptions Options;
		Options.MaxFrameDWords = 1;
		asSByteCodeEmissionResult Emitted = EmitFromSession(Builder, Options);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::ResourceLimit, Emitted.EmissionStatus));
		ASSERT_THAT(IsNull(Emitted.Image.Get()));
	}

	TEST_METHOD(VerifiedSupportedBodySucceedsEngineFree)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return 42; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = VMSourceAdmissionPrivate::RequireFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		ASSERT_THAT(IsNull(Function->GetEngine()));
		asSByteCodeEmissionResult Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		ASSERT_THAT(IsNotNull(Emitted.Image.Get()));
		ASSERT_THAT(IsNull(Function->GetEngine()));
	}

	TEST_METHOD(UnsupportedBodyPublishesNoPartialImage)
	{
		FSourceExecutionInput Input(TEXT(
			"int Good() { return 1; } "
			"class Range { int opForBegin() const; bool opForEnd(int Iterator) const; "
			"void opForNext(int Iterator) const; int opForValue(int Iterator) const; } "
			"int Bad(Range Values) { int Sum = 0; foreach (auto Value : Values) { Sum += Value; } return Sum; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asSByteCodeEmissionResult Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::UnsupportedLowering, Emitted.EmissionStatus));
		ASSERT_THAT(IsNull(Emitted.Image.Get()));
		ASSERT_THAT(AreEqual(static_cast<uint16>(AS_NAMESPACE_QUALIFIER asEStmtKind::Foreach), Emitted.Failure.NodeKind));
	}

	TEST_METHOD(DiagnosticsSurviveProducerDestruction)
	{
		asSByteCodeEmissionResult Emitted;
		asSStableKey FunctionKey;
		{
			FSourceExecutionInput Input(TEXT("int Run() { return 42; }"));
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough()));
			asCScriptFunction* Function = VMSourceAdmissionPrivate::RequireFunction(Builder, "Run");
			ASSERT_THAT(IsNotNull(Function));
			FunctionKey = Function->GetStableKey();
			asSByteCodeEmissionOptions Options;
			Options.MaxFrameDWords = 1;
			Emitted = EmitFromSession(Builder, Options);
		}
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::ResourceLimit, Emitted.EmissionStatus));
		ASSERT_THAT(IsNull(Emitted.Image.Get()));
		ASSERT_THAT(IsTrue(Emitted.Failure.FunctionKey == FunctionKey));
		ASSERT_THAT(IsTrue(Emitted.Failure.Diagnostics.Num() > 0));
		ASSERT_THAT(IsTrue(Emitted.Failure.StableFragmentKey.Len() > 0));
	}

	TEST_METHOD(AdapterDoesNotMutateFrozenParameterOffsets)
	{
		FSourceExecutionInput Input(TEXT("int Add(int a, int b) { return a+b; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = VMSourceAdmissionPrivate::RequireFunction(Builder, "Add");
		ASSERT_THAT(IsNotNull(Function));
		const TArray<int> Before = VMSourceAdmissionPrivate::CopyParameterOffsets(*Function);
		asSByteCodeEmissionResult Emitted = asCByteCodeEmitter::Emit(Builder, *Function);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		ASSERT_THAT(IsNotNull(Emitted.Image.Get()));
		const TArray<int> After = VMSourceAdmissionPrivate::CopyParameterOffsets(*Function);
		ASSERT_THAT(AreEqual(Before.Num(), After.Num()));
		for (int32 Index = 0; Index < Before.Num(); ++Index)
		{
			ASSERT_THAT(AreEqual(Before[Index], After[Index]));
		}
	}

	TEST_METHOD(ValidEmissionLinksThroughAtomicPublish)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return 42; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = VMSourceAdmissionPrivate::RequireFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		asSByteCodeEmissionResult Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		ASSERT_THAT(IsNotNull(Emitted.Image.Get()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.GetDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}
};

#endif
