#include "CQTest.h"
#include "NativeSourceExecutionTestSupport.h"
#include "as_datatype.h"
#include "as_generic.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_ast_codec.h"
#include "frontend/AST/as_ast_projection.h"
#include "frontend/AST/as_ast_visitor.h"
#include "frontend/Compile/as_compilation_session.h"
#include "frontend/AST/as_expr.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	TArray<int32> GObjectMarkTrace;

	void ResetObjectMarkTrace()
	{
		GObjectMarkTrace.Reset();
	}

	void GenericObjectMark(asIScriptGeneric* Generic)
	{
		const int32 Value = static_cast<int32>(Generic->GetArgDWord(0));
		GObjectMarkTrace.Add(Value);
		Generic->SetReturnDWord(static_cast<asDWORD>(Value));
	}

	asSMetadataParameter MakeObjectIntParam(const char* Name)
	{
		asSMetadataParameter Parameter;
		Parameter.Name = Name;
		Parameter.Type = asCDataType::CreatePrimitive(ttInt, false);
		return Parameter;
	}

	struct FObjectHost
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Mark = nullptr;
		bool Initialize()
		{
			Fixture.Scope = TEXT("vm-source-object-host");
			asSMetadataParameter Param = MakeObjectIntParam("n");
			Param.Type = Fixture.Set->Primitive(ttInt);
			Mark = Fixture.DefineFunction(TEXT("Mark"), TEXT("Host"), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), MakeArrayView(&Param, 1));
			return Mark && Fixture.FinalizeAndFreeze();
		}
	};

	asCScriptFunction* FindRun(asCBuilder& Builder)
	{
		return FindSourceFunction(Builder, "Run");
	}

	bool ExecuteRun(asCBuilder& Builder, int32& OutValue, asCScriptEngine* Engine = nullptr)
	{
		asCScriptFunction* Function = FindRun(Builder);
		if (!Function) return false;
		FSessionEmission Emitted = EmitFromSession(Builder);
		if (Emitted.Status != asEByteCodeImageStatus::Succeeded || !Emitted.Image.Get()) return false;
		FNativeEngineOwner Owned;
		asCScriptEngine* Use = Engine ? Engine : Owned.Ptr;
		if (RegisterCompiledDefinitions(*Use, Builder) != asEMetadataRegistrationResult::Succeeded
			&& RegisterCompiledDefinitions(*Use, Builder) != asEMetadataRegistrationResult::AlreadyRegistered)
		{
			return false;
		}
		asCExecutableSnapshot* Snapshot = nullptr;
		if (asLinkByteCodeImage(*Use, *Emitted.Image, Snapshot) != asEByteCodeLinkStatus::Succeeded) return false;
		asIScriptContext* Context = Use->CreateContext();
		if (Context->Prepare(Function) != 0 || Context->Execute() != asEXECUTION_FINISHED)
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

	const asCConstructExpr* FindConstruct(const asCASTNode* Root)
	{
		const asCConstructExpr* Found = nullptr;
		asCASTVisitor::Walk(Root, [&Found](const asCASTNode& Node)
		{
			if (!Found) Found = dyn_cast<asCConstructExpr>(&Node);
		});
		return Found;
	}
}

TEST_CLASS_WITH_FLAGS(VMSourceObjects, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(SourceFreeFunctionStillExecutes)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return 7; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteRun(Builder, Value)));
		ASSERT_THAT(AreEqual(7, Value));
	}

	TEST_METHOD(SourceValueLocalSevenReadsSeven)
	{
		FSourceExecutionInput Input(TEXT("class Value { int Number; Value(int Initial) { Number = Initial; } int Read() const { return Number; } } int Run() { Value Local(7); return Local.Read(); }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		const bool Built = Builder.RunThrough();
		ASSERT_THAT(IsTrue(Built, DescribeBuilderFailure(Builder)));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteRun(Builder, Value)));
		ASSERT_THAT(AreEqual(7, Value));
	}

	TEST_METHOD(SourceValueFieldWriteThenRead)
	{
		FSourceExecutionInput Input(TEXT("class Value { int Number; Value(int Initial) { Number = Initial; } int Read() const { return Number; } } int Run() { Value Local(7); Local.Number = 9; return Local.Read(); }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteRun(Builder, Value)));
		ASSERT_THAT(AreEqual(9, Value));
	}

	TEST_METHOD(SourceEmptyDefaultConstructCompletes)
	{
		FSourceExecutionInput Input(TEXT("class Empty {} int Run() { Empty Local; return 1; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteRun(Builder, Value)));
		ASSERT_THAT(AreEqual(1, Value));
	}

	TEST_METHOD(SourcePlainCopyReadsNine)
	{
		FSourceExecutionInput Input(TEXT("class Plain { int X; } int Run() { Plain A; A.X = 9; Plain B(A); return B.X; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteRun(Builder, Value)));
		ASSERT_THAT(AreEqual(9, Value));
	}

	TEST_METHOD(SourcePairNamedConstructorFormalOrder)
	{
		FSourceExecutionInput Input(TEXT("class Pair { int a; int b; Pair(int A, int B) { this.a = A; this.b = B; } int Read() const { return 10 * a + b; } } int Run() { Pair Local(b:2, a:1); return Local.Read(); }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteRun(Builder, Value)));
		ASSERT_THAT(AreEqual(12, Value));
	}

	TEST_METHOD(SourceConstructOrdinalsRoundtripAndRejectBadVersion)
	{
		FSourceExecutionInput Input(TEXT("class Pair { int a; int b; Pair(int A, int B) { a = A; b = B; } } int Run() { Pair Local(b:2, a:1); return 0; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		const asCCompilationSession* Session = Builder.GetCompilationSession();
		ASSERT_THAT(IsNotNull(Session));
		const TConstArrayView<asCDecl*> Found = Session->Lookup(TEXT("Run"));
		ASSERT_THAT(IsTrue(Found.Num() > 0));
		const asCFunctionDecl* Run = dyn_cast<asCFunctionDecl>(Found[0]);
		ASSERT_THAT(IsNotNull(Run));
		const asCConstructExpr* Construct = FindConstruct(Run->GetBody());
		ASSERT_THAT(IsNotNull(Construct));
		ASSERT_THAT(AreEqual(2, Construct->GetArguments().Num()));
		ASSERT_THAT(AreEqual(2, Construct->GetArgumentSourceOrdinals().Num()));
		ASSERT_THAT(AreEqual(1, Construct->GetArgumentSourceOrdinals()[0]));
		ASSERT_THAT(AreEqual(0, Construct->GetArgumentSourceOrdinals()[1]));
		asCASTProjection Projection;
		FString Error;
		ASSERT_THAT(IsTrue(asCASTProjection::Build(Session->GetASTContext(), Session->GetTranslationUnit(), Projection, Error), Error));
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEASTCodecStatus::Succeeded, asCASTCodec::Encode(Projection, Bytes)));
		asCASTProjection Decoded;
		ASSERT_THAT(AreEqual(asEASTCodecStatus::Succeeded, asCASTCodec::Decode(Bytes, Decoded)));
		ASSERT_THAT(IsTrue(Projection.Equals(Decoded)));
		ASSERT_THAT(IsTrue(Bytes.Num() > 8));
		Bytes[4] = 7;
		Bytes[5] = 0;
		asCASTProjection Old;
		ASSERT_THAT(AreEqual(asEASTCodecStatus::IncompatibleVersion, asCASTCodec::Decode(Bytes, Old)));
	}

	TEST_METHOD(SourceReverseDestroyOnReturn)
	{
		ResetObjectMarkTrace();
		FObjectHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput Input(TEXT("class Box { int Id; Box(int Initial) { Id = Initial; } ~Box() { Host::Mark(Id); } } int Run() { Box A(1); Box B(2); return 0; }"), Host.Fixture.Context, Host.Fixture.Set.Get());
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Host.Fixture.Set))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Host.Mark->GetStableKey(), asFUNCTION(GenericObjectMark), asCALL_GENERIC)));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteRun(Builder, Value, Engine.Ptr)));
		ASSERT_THAT(AreEqual(0, Value));
		ASSERT_THAT(AreEqual(2, GObjectMarkTrace.Num()));
		ASSERT_THAT(AreEqual(2, GObjectMarkTrace[0]));
		ASSERT_THAT(AreEqual(1, GObjectMarkTrace[1]));
	}
};

#endif
