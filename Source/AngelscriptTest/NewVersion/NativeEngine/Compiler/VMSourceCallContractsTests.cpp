#include "CQTest.h"
#include "NativeSourceExecutionTestSupport.h"
#include "as_datatype.h"
#include "as_generic.h"
#include "frontend/as_ast_cast.h"
#include "frontend/as_ast_codec.h"
#include "frontend/as_ast_projection.h"
#include "frontend/as_ast_visitor.h"
#include "frontend/as_compilation_session.h"
#include "frontend/as_decl.h"
#include "frontend/as_diagnostics.h"
#include "frontend/as_expr.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace VMSourceCallContractsPrivate
{
	TArray<int32> GMarkTrace;
	int32 GNativeRefCalls = 0;
	int32 GNativeCombineCalls = 0;

	void ResetTraces()
	{
		GMarkTrace.Reset();
		GNativeRefCalls = 0;
		GNativeCombineCalls = 0;
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

	void GenericFill(asIScriptGeneric* Generic)
	{
		++GNativeRefCalls;
		*static_cast<int32*>(Generic->GetArgAddress(0)) = 41;
	}

	void GenericBump(asIScriptGeneric* Generic)
	{
		++GNativeRefCalls;
		int32* Value = static_cast<int32*>(Generic->GetArgAddress(0));
		*Value = *Value + 1;
	}

	void GenericPeek(asIScriptGeneric* Generic)
	{
		++GNativeRefCalls;
		Generic->SetReturnDWord(*static_cast<int32*>(Generic->GetArgAddress(0)));
	}

	void GenericScaled(asIScriptGeneric* Generic)
	{
		++GNativeCombineCalls;
		const int32 A = static_cast<int32>(Generic->GetArgDWord(0));
		const int32 B = static_cast<int32>(Generic->GetArgDWord(1));
		Generic->SetReturnDWord(static_cast<asDWORD>(10 * A + B));
	}

	asSMetadataParameter MakeIntParam(const char* Name, asETypeModifiers Flags = asTM_NONE, bool bConst = false, const char* Default = nullptr)
	{
		asSMetadataParameter Parameter;
		Parameter.Name = Name;
		Parameter.Type = asCDataType::CreatePrimitive(ttInt, bConst);
		Parameter.Flags = Flags;
		if (Default)
		{
			Parameter.DefaultArgument = Default;
		}
		return Parameter;
	}

	struct FContractHost
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Fill = nullptr;
		asCScriptFunction* Bump = nullptr;
		asCScriptFunction* Peek = nullptr;
		asCScriptFunction* Mark = nullptr;
		asCScriptFunction* Pack = nullptr;
		asCScriptFunction* Scaled = nullptr;

		bool Initialize()
		{
			Fixture.Scope = TEXT("vm-source-call-host");
			asSMetadataParameter FillParam = MakeIntParam("X", asTM_OUTREF);
			FillParam.Type = Fixture.Image->Primitive(ttInt);
			FillParam.Type.MakeReference(true);
			Fill = Fixture.DefineFunction(TEXT("Fill"), TEXT("Host"), asFUNC_SYSTEM, Fixture.Image->Primitive(ttVoid), MakeArrayView(&FillParam, 1));
			asSMetadataParameter BumpParam = MakeIntParam("X", asTM_INOUTREF);
			BumpParam.Type = Fixture.Image->Primitive(ttInt);
			BumpParam.Type.MakeReference(true);
			Bump = Fixture.DefineFunction(TEXT("Bump"), TEXT("Host"), asFUNC_SYSTEM, Fixture.Image->Primitive(ttVoid), MakeArrayView(&BumpParam, 1));
			asSMetadataParameter PeekParam = MakeIntParam("X", asTM_INREF, true);
			PeekParam.Type = Fixture.Image->Primitive(ttInt, true);
			PeekParam.Type.MakeReference(true);
			Peek = Fixture.DefineFunction(TEXT("Peek"), TEXT("Host"), asFUNC_SYSTEM, Fixture.Image->Primitive(ttInt), MakeArrayView(&PeekParam, 1));
			asSMetadataParameter MarkParam = MakeIntParam("n");
			MarkParam.Type = Fixture.Image->Primitive(ttInt);
			Mark = Fixture.DefineFunction(TEXT("Mark"), TEXT("Host"), asFUNC_SYSTEM, Fixture.Image->Primitive(ttInt), MakeArrayView(&MarkParam, 1));
			asSMetadataParameter PackParams[2] = {MakeIntParam("a"), MakeIntParam("b")};
			PackParams[0].Type = Fixture.Image->Primitive(ttInt);
			PackParams[1].Type = Fixture.Image->Primitive(ttInt);
			Pack = Fixture.DefineFunction(TEXT("Pack"), TEXT("Host"), asFUNC_SYSTEM, Fixture.Image->Primitive(ttInt), MakeArrayView(PackParams, 2));
			asSMetadataParameter ScaledParams[2] = {MakeIntParam("A"), MakeIntParam("B", asTM_NONE, false, "4")};
			ScaledParams[0].Type = Fixture.Image->Primitive(ttInt);
			ScaledParams[1].Type = Fixture.Image->Primitive(ttInt);
			ScaledParams[1].DefaultArgument = "4";
			Scaled = Fixture.DefineFunction(TEXT("Scaled"), TEXT("Host"), asFUNC_SYSTEM, Fixture.Image->Primitive(ttInt), MakeArrayView(ScaledParams, 2));
			return Fill && Bump && Peek && Mark && Pack && Scaled && Fixture.FinalizeAndFreeze();
		}
	};

	bool BindHost(asCScriptEngine& Engine, const FContractHost& Host)
	{
		return Engine.BindNativeFunction(Host.Fill->GetStableKey(), asFUNCTION(GenericFill), asCALL_GENERIC) == asEByteCodeLinkStatus::Succeeded
			&& Engine.BindNativeFunction(Host.Bump->GetStableKey(), asFUNCTION(GenericBump), asCALL_GENERIC) == asEByteCodeLinkStatus::Succeeded
			&& Engine.BindNativeFunction(Host.Peek->GetStableKey(), asFUNCTION(GenericPeek), asCALL_GENERIC) == asEByteCodeLinkStatus::Succeeded
			&& Engine.BindNativeFunction(Host.Mark->GetStableKey(), asFUNCTION(GenericMark), asCALL_GENERIC) == asEByteCodeLinkStatus::Succeeded
			&& Engine.BindNativeFunction(Host.Pack->GetStableKey(), asFUNCTION(GenericPack), asCALL_GENERIC) == asEByteCodeLinkStatus::Succeeded
			&& Engine.BindNativeFunction(Host.Scaled->GetStableKey(), asFUNCTION(GenericScaled), asCALL_GENERIC) == asEByteCodeLinkStatus::Succeeded;
	}

	bool ExecuteLinked(
		asCBuilder& Builder,
		asCScriptEngine& Engine,
		asCExecutableSnapshot*& Snapshot,
		TUniquePtr<asCByteCodeImage>& Image)
	{
		asSByteCodeEmissionResult Emitted = EmitFromSession(Builder);
		if (Emitted.EmissionStatus != asEByteCodeEmissionStatus::Succeeded || !Emitted.Image.Get())
		{
			return false;
		}
		if (Engine.RegisterMetadataImage(Builder.TakeDefinitions()) != asEMetadataRegistrationResult::Succeeded
			&& Engine.RegisterMetadataImage(Builder.TakeDefinitions()) != asEMetadataRegistrationResult::AlreadyRegistered)
		{
			return false;
		}
		if (Engine.LinkByteCodeImage(*Emitted.Image, Snapshot) != asEByteCodeLinkStatus::Succeeded)
		{
			return false;
		}
		Image = MoveTemp(Emitted.Image);
		return Snapshot != nullptr;
	}

	bool ExecuteFunction(asCScriptEngine& Engine, asCScriptFunction* Function, int32& OutValue)
	{
		if (!Function)
		{
			return false;
		}
		asIScriptContext* Context = Engine.CreateContext();
		if (Context->Prepare(Function) != 0 || Context->Execute() != asEXECUTION_FINISHED)
		{
			Context->Release();
			return false;
		}
		OutValue = static_cast<int32>(Context->GetReturnDWord());
		Context->Release();
		return true;
	}

	bool ExecuteNamed(asCBuilder& Builder, const char* Name, int32& OutValue, asCScriptEngine* Engine = nullptr)
	{
		asCScriptFunction* Function = FindSourceFunction(Builder, Name);
		if (!Function)
		{
			return false;
		}
		FNativeEngineOwner Owned;
		asCScriptEngine* Use = Engine ? Engine : Owned.Ptr;
		asCExecutableSnapshot* Snapshot = nullptr;
		TUniquePtr<asCByteCodeImage> Image;
		if (!ExecuteLinked(Builder, *Use, Snapshot, Image))
		{
			return false;
		}
		const bool bOk = ExecuteFunction(*Use, Function, OutValue);
		if (Snapshot)
		{
			Snapshot->Release();
		}
		return bOk;
	}

	FString CollectDiagnosticText(asCDiagnosticsEngine& Diagnostics)
	{
		asCCollectingDiagnosticConsumer Consumer;
		Diagnostics.EmitTo(Consumer);
		FString Out;
		for (const asSDiagnosticRecord& Record : Consumer.GetRecords())
		{
			for (const asSDiagnosticArgument& Argument : Record.Arguments)
			{
				Out += Argument.ToDisplayString();
				Out += TEXT(" ");
			}
		}
		return Out;
	}

	const asCFunctionDecl* FindDecl(const asCCompilationSession& Session, const FString& Name)
	{
		const TConstArrayView<asCDecl*> Found = Session.Lookup(Name);
		return Found.Num() > 0 ? dyn_cast<asCFunctionDecl>(Found[0]) : nullptr;
	}

	const asCConstructExpr* FindConstruct(const asCASTNode* Root)
	{
		const asCConstructExpr* Found = nullptr;
		asCASTVisitor::Walk(Root, [&Found](const asCASTNode& Node)
		{
			if (!Found)
			{
				Found = dyn_cast<asCConstructExpr>(&Node);
			}
		});
		return Found;
	}

	const asCThisExpr* FindImplicitThis(const asCASTNode* Root)
	{
		const asCThisExpr* Found = nullptr;
		asCASTVisitor::Walk(Root, [&Found](const asCASTNode& Node)
		{
			if (!Found)
			{
				if (const asCThisExpr* This = dyn_cast<asCThisExpr>(&Node); This && This->IsImplicit())
				{
					Found = This;
				}
			}
		});
		return Found;
	}

	bool WriteU32(TArray<uint8>& Bytes, int32 Offset, uint32 Value)
	{
		if (Offset < 0 || Offset + 4 > Bytes.Num())
		{
			return false;
		}
		Bytes[Offset] = static_cast<uint8>(Value);
		Bytes[Offset + 1] = static_cast<uint8>(Value >> 8);
		Bytes[Offset + 2] = static_cast<uint8>(Value >> 16);
		Bytes[Offset + 3] = static_cast<uint8>(Value >> 24);
		return true;
	}

	int32 FindU32Sequence(const TArray<uint8>& Bytes, TConstArrayView<uint32> Values)
	{
		if (Values.Num() == 0)
		{
			return INDEX_NONE;
		}
		const int32 Span = Values.Num() * 4;
		for (int32 Offset = 0; Offset + Span <= Bytes.Num(); ++Offset)
		{
			bool bMatch = true;
			for (int32 Index = 0; bMatch && Index < Values.Num(); ++Index)
			{
				uint32 Read = uint32(Bytes[Offset + Index * 4])
					| (uint32(Bytes[Offset + Index * 4 + 1]) << 8)
					| (uint32(Bytes[Offset + Index * 4 + 2]) << 16)
					| (uint32(Bytes[Offset + Index * 4 + 3]) << 24);
				if (Read != Values[Index])
				{
					bMatch = false;
				}
			}
			if (bMatch)
			{
				return Offset;
			}
		}
		return INDEX_NONE;
	}

	FString LogicalKey(asCSourceManager& Sources, const asCSourceRange& Range)
	{
		asSStableSourceAnchor Anchor;
		if (!Sources.TryCreateStableAnchor(Range, Anchor))
		{
			return FString();
		}
		return Anchor.LogicalSourceKey;
	}
}

TEST_CLASS_WITH_FLAGS(VMSourceCallContracts, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(NativeFillBumpPeekLocalWritesFortyTwo)
	{
		using namespace VMSourceCallContractsPrivate;
		ResetTraces();
		FContractHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput Input(
			TEXT("int Run() { int A = 0; Host::Fill(A); Host::Bump(A); return Host::Peek(A); }"),
			Host.Fixture.Context, Host.Fixture.Image.Get());
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(MoveTemp(Host.Fixture.Image))));
		ASSERT_THAT(IsTrue(BindHost(*Engine.Ptr, Host)));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteNamed(Builder, "Run", Value, Engine.Ptr)));
		ASSERT_THAT(AreEqual(42, Value));
		ASSERT_THAT(AreEqual(3, GNativeRefCalls));
	}

	TEST_METHOD(NativeFillBumpPeekMemberWritesFortyTwo)
	{
		using namespace VMSourceCallContractsPrivate;
		ResetTraces();
		FContractHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput Input(
			TEXT("class Holder { int Value; } int Run() { Holder H; H.Value = 0; Host::Fill(H.Value); Host::Bump(H.Value); return Host::Peek(H.Value); }"),
			Host.Fixture.Context, Host.Fixture.Image.Get());
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(MoveTemp(Host.Fixture.Image))));
		ASSERT_THAT(IsTrue(BindHost(*Engine.Ptr, Host)));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteNamed(Builder, "Run", Value, Engine.Ptr)));
		ASSERT_THAT(AreEqual(42, Value));
		ASSERT_THAT(AreEqual(3, GNativeRefCalls));
	}

	TEST_METHOD(NativeFillBumpPeekForwardedRefWritesFortyTwo)
	{
		using namespace VMSourceCallContractsPrivate;
		ResetTraces();
		FContractHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput Input(
			TEXT("void ForwardFill(int&out X) { Host::Fill(X); } void ForwardBump(int&inout X) { Host::Bump(X); } int ForwardPeek(const int&in X) { return Host::Peek(X); } int Run() { int A = 0; ForwardFill(A); ForwardBump(A); return ForwardPeek(A); }"),
			Host.Fixture.Context, Host.Fixture.Image.Get());
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(MoveTemp(Host.Fixture.Image))));
		ASSERT_THAT(IsTrue(BindHost(*Engine.Ptr, Host)));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteNamed(Builder, "Run", Value, Engine.Ptr)));
		ASSERT_THAT(AreEqual(42, Value));
		ASSERT_THAT(AreEqual(3, GNativeRefCalls));
	}

	TEST_METHOD(ScriptFillRunsThenLiteralOutRejects)
	{
		using namespace VMSourceCallContractsPrivate;
		FSourceExecutionInput Valid(TEXT("void Fill(int&out X) { int T = 41; X = T; } int Ok() { int A = 0; Fill(A); return A; }"));
		asCBuilder ValidBuilder(Valid.Snapshot, Valid.Diagnostics, Valid.Options);
		ASSERT_THAT(IsTrue(ValidBuilder.RunThrough(), DescribeBuilderFailure(ValidBuilder)));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteNamed(ValidBuilder, "Ok", Value)));
		ASSERT_THAT(AreEqual(41, Value));

		FSourceExecutionInput Invalid(TEXT("void Fill(int&out X) { X = 41; } int Run() { Fill(1); return 0; }"));
		asCBuilder InvalidBuilder(Invalid.Snapshot, Invalid.Diagnostics, Invalid.Options);
		ASSERT_THAT(IsFalse(InvalidBuilder.RunThrough()));
		asSByteCodeEmissionResult Emitted = EmitFromSession(InvalidBuilder);
		ASSERT_THAT(IsTrue(Emitted.EmissionStatus != asEByteCodeEmissionStatus::Succeeded));
		ASSERT_THAT(IsNull(Emitted.Image.Get()));
	}

	TEST_METHOD(ConstOutRvalueMismatchesReject)
	{
		using namespace VMSourceCallContractsPrivate;
		ResetTraces();
		FContractHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput ConstInput(
			TEXT("int Run() { const int A = 0; Host::Fill(A); return A; }"),
			Host.Fixture.Context, Host.Fixture.Image.Get());
		asCBuilder ConstBuilder(ConstInput.Snapshot, ConstInput.Diagnostics, ConstInput.Options);
		ASSERT_THAT(IsFalse(ConstBuilder.RunThrough()));
		ASSERT_THAT(AreEqual(0, GNativeRefCalls));

		FSourceExecutionInput RvalueInput(
			TEXT("int Run() { Host::Fill(1); return 0; }"),
			Host.Fixture.Context, Host.Fixture.Image.Get());
		asCBuilder RvalueBuilder(RvalueInput.Snapshot, RvalueInput.Diagnostics, RvalueInput.Options);
		ASSERT_THAT(IsFalse(RvalueBuilder.RunThrough()));
		ASSERT_THAT(AreEqual(0, GNativeRefCalls));

		FSourceExecutionInput OutRvalue(
			TEXT("void Fill(int&out X) { X = 41; } int Run() { Fill(1); return 0; }"));
		asCBuilder OutBuilder(OutRvalue.Snapshot, OutRvalue.Diagnostics, OutRvalue.Options);
		ASSERT_THAT(IsFalse(OutBuilder.RunThrough()));
	}

	TEST_METHOD(TypedDefaultHostMarkTwiceIsSixtyEight)
	{
		using namespace VMSourceCallContractsPrivate;
		ResetTraces();
		FContractHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput Input(
			TEXT("int Combine(int A, int B=Host::Mark(4)) { return A*10+B; } int Run() { return Combine(3)+Combine(3); }"),
			Host.Fixture.Context, Host.Fixture.Image.Get());
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(MoveTemp(Host.Fixture.Image))));
		ASSERT_THAT(IsTrue(BindHost(*Engine.Ptr, Host)));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteNamed(Builder, "Run", Value, Engine.Ptr)));
		ASSERT_THAT(AreEqual(68, Value));
		ASSERT_THAT(AreEqual(2, GMarkTrace.Num()));
		ASSERT_THAT(AreEqual(4, GMarkTrace[0]));
		ASSERT_THAT(AreEqual(4, GMarkTrace[1]));
	}

	TEST_METHOD(ExplicitArgSuppressesTypedDefault)
	{
		using namespace VMSourceCallContractsPrivate;
		ResetTraces();
		FContractHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput Input(
			TEXT("int Combine(int A, int B=Host::Mark(4)) { return A*10+B; } int Run() { return Combine(3, 5); }"),
			Host.Fixture.Context, Host.Fixture.Image.Get());
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(MoveTemp(Host.Fixture.Image))));
		ASSERT_THAT(IsTrue(BindHost(*Engine.Ptr, Host)));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteNamed(Builder, "Run", Value, Engine.Ptr)));
		ASSERT_THAT(AreEqual(35, Value));
		ASSERT_THAT(AreEqual(0, GMarkTrace.Num()));
	}

	TEST_METHOD(MetadataStringDefaultOmitRejectsWithoutImage)
	{
		using namespace VMSourceCallContractsPrivate;
		ResetTraces();
		FContractHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput Input(
			TEXT("int Run() { return Host::Scaled(3); }"),
			Host.Fixture.Context, Host.Fixture.Image.Get());
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsFalse(Builder.RunThrough()));
		const FString Text = CollectDiagnosticText(Input.Diagnostics) + DescribeBuilderFailure(Builder);
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("unsupported-default"))));
		asSByteCodeEmissionResult Emitted = EmitFromSession(Builder);
		ASSERT_THAT(IsTrue(Emitted.EmissionStatus != asEByteCodeEmissionStatus::Succeeded));
		ASSERT_THAT(IsNull(Emitted.Image.Get()));
		ASSERT_THAT(AreEqual(0, GNativeCombineCalls));
	}

	TEST_METHOD(MetadataStringDefaultExplicitSucceeds)
	{
		using namespace VMSourceCallContractsPrivate;
		ResetTraces();
		FContractHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput Input(
			TEXT("int Run() { return Host::Scaled(3, 5); }"),
			Host.Fixture.Context, Host.Fixture.Image.Get());
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(MoveTemp(Host.Fixture.Image))));
		ASSERT_THAT(IsTrue(BindHost(*Engine.Ptr, Host)));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteNamed(Builder, "Run", Value, Engine.Ptr)));
		ASSERT_THAT(AreEqual(35, Value));
		ASSERT_THAT(AreEqual(1, GNativeCombineCalls));
	}

	TEST_METHOD(TwoFilesEitherOrderReturnSevenWithLocations)
	{
		using namespace VMSourceCallContractsPrivate;
		const TArray<TPair<FString, FString>> CallerFirst = {
			{TEXT("Caller.as"), TEXT("int Run() { return Callee(); }")},
			{TEXT("Callee.as"), TEXT("int Callee() { return 7; }")}
		};
		const TArray<TPair<FString, FString>> CalleeFirst = {
			{TEXT("Callee.as"), TEXT("int Callee() { return 7; }")},
			{TEXT("Caller.as"), TEXT("int Run() { return Callee(); }")}
		};
		for (const TArray<TPair<FString, FString>>& Files : {CallerFirst, CalleeFirst})
		{
			FSourceExecutionInput Input(Files);
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
			int32 Value = 0;
			ASSERT_THAT(IsTrue(ExecuteNamed(Builder, "Run", Value)));
			ASSERT_THAT(AreEqual(7, Value));
			const asCCompilationSession* Session = Builder.GetCompilationSession();
			ASSERT_THAT(IsNotNull(Session));
			const asCFunctionDecl* Run = FindDecl(*Session, TEXT("Run"));
			const asCFunctionDecl* Callee = FindDecl(*Session, TEXT("Callee"));
			ASSERT_THAT(IsNotNull(Run));
			ASSERT_THAT(IsNotNull(Callee));
			ASSERT_THAT(AreEqual(FString(TEXT("Caller.as")), LogicalKey(Input.Sources, Run->GetSourceRange())));
			ASSERT_THAT(AreEqual(FString(TEXT("Callee.as")), LogicalKey(Input.Sources, Callee->GetSourceRange())));
		}
	}

	TEST_METHOD(PositionalNamedReverseFormalTraceTwelve)
	{
		using namespace VMSourceCallContractsPrivate;
		ResetTraces();
		FContractHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput Input(
			TEXT("int Positional() { return Host::Pack(Host::Mark(1), Host::Mark(2)); } int Named() { return Host::Pack(b:Host::Mark(2), a:Host::Mark(1)); }"),
			Host.Fixture.Context, Host.Fixture.Image.Get());
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(MoveTemp(Host.Fixture.Image))));
		ASSERT_THAT(IsTrue(BindHost(*Engine.Ptr, Host)));
		asCExecutableSnapshot* Snapshot = nullptr;
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(IsTrue(ExecuteLinked(Builder, *Engine.Ptr, Snapshot, Image)));
		int32 Positional = 0;
		ASSERT_THAT(IsTrue(ExecuteFunction(*Engine.Ptr, FindSourceFunction(Builder, "Positional"), Positional)));
		ASSERT_THAT(AreEqual(12, Positional));
		ASSERT_THAT(AreEqual(2, GMarkTrace.Num()));
		ASSERT_THAT(AreEqual(2, GMarkTrace[0]));
		ASSERT_THAT(AreEqual(1, GMarkTrace[1]));
		ResetTraces();
		int32 Named = 0;
		ASSERT_THAT(IsTrue(ExecuteFunction(*Engine.Ptr, FindSourceFunction(Builder, "Named"), Named)));
		ASSERT_THAT(AreEqual(12, Named));
		ASSERT_THAT(AreEqual(2, GMarkTrace.Num()));
		ASSERT_THAT(AreEqual(2, GMarkTrace[0]));
		ASSERT_THAT(AreEqual(1, GMarkTrace[1]));
		if (Snapshot)
		{
			Snapshot->Release();
		}
	}

	TEST_METHOD(PairNamedCtorStoresWithReverseFormalTrace)
	{
		using namespace VMSourceCallContractsPrivate;
		ResetTraces();
		FContractHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput Input(
			TEXT("class Pair { int a; int b; Pair(int a, int b) { this.a = a; this.b = b; } int Read() const { return 10 * a + b; } } int Run() { Pair Local(b:Host::Mark(2), a:Host::Mark(1)); return Local.Read(); }"),
			Host.Fixture.Context, Host.Fixture.Image.Get());
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(MoveTemp(Host.Fixture.Image))));
		ASSERT_THAT(IsTrue(BindHost(*Engine.Ptr, Host)));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteNamed(Builder, "Run", Value, Engine.Ptr)));
		ASSERT_THAT(AreEqual(12, Value));
		ASSERT_THAT(AreEqual(2, GMarkTrace.Num()));
		ASSERT_THAT(AreEqual(2, GMarkTrace[0]));
		ASSERT_THAT(AreEqual(1, GMarkTrace[1]));
	}

	TEST_METHOD(ImplicitThisMemberAccessReturnsSeven)
	{
		using namespace VMSourceCallContractsPrivate;
		FSourceExecutionInput Input(TEXT("class Holder { int Value; void Write(int X) { Value = X; } int Read() const { return Value; } } int Run() { Holder H; H.Write(7); return H.Read(); }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteNamed(Builder, "Run", Value)));
		ASSERT_THAT(AreEqual(7, Value));
		const asCCompilationSession* Session = Builder.GetCompilationSession();
		ASSERT_THAT(IsNotNull(Session));
		const asCFunctionDecl* Write = FindDecl(*Session, TEXT("Write"));
		ASSERT_THAT(IsNotNull(Write));
		ASSERT_THAT(IsNotNull(FindImplicitThis(Write->GetBody())));
	}

	TEST_METHOD(ConstructorOrdinalsRoundtripAndRejectCorrupt)
	{
		using namespace VMSourceCallContractsPrivate;
		FSourceExecutionInput Input(TEXT("class Pair { int a; int b; Pair(int A, int B = 0) { a = A; b = B; } } int Named() { Pair Local(b:2, a:1); return 0; } int Defaulted() { Pair Local(1); return 0; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		const asCCompilationSession* Session = Builder.GetCompilationSession();
		ASSERT_THAT(IsNotNull(Session));
		const asCFunctionDecl* Named = FindDecl(*Session, TEXT("Named"));
		const asCFunctionDecl* Defaulted = FindDecl(*Session, TEXT("Defaulted"));
		ASSERT_THAT(IsNotNull(Named));
		ASSERT_THAT(IsNotNull(Defaulted));
		const asCConstructExpr* NamedConstruct = FindConstruct(Named->GetBody());
		const asCConstructExpr* DefaultConstruct = FindConstruct(Defaulted->GetBody());
		ASSERT_THAT(IsNotNull(NamedConstruct));
		ASSERT_THAT(IsNotNull(DefaultConstruct));
		ASSERT_THAT(AreEqual(2, NamedConstruct->GetArgumentSourceOrdinals().Num()));
		ASSERT_THAT(AreEqual(1, NamedConstruct->GetArgumentSourceOrdinals()[0]));
		ASSERT_THAT(AreEqual(0, NamedConstruct->GetArgumentSourceOrdinals()[1]));
		ASSERT_THAT(AreEqual(2, DefaultConstruct->GetArgumentSourceOrdinals().Num()));
		ASSERT_THAT(AreEqual(0, DefaultConstruct->GetArgumentSourceOrdinals()[0]));
		ASSERT_THAT(AreEqual(INDEX_NONE, DefaultConstruct->GetArgumentSourceOrdinals()[1]));

		asCASTProjection Projection;
		FString Error;
		ASSERT_THAT(IsTrue(asCASTProjection::Build(Session->GetASTContext(), NamedConstruct, Projection, Error), Error));
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEASTCodecStatus::Succeeded, asCASTCodec::Encode(Projection, Bytes)));
		ASSERT_THAT(IsTrue(Bytes.Num() > 6));
		ASSERT_THAT(AreEqual(static_cast<uint8>(asCASTCodec::CurrentVersion), Bytes[4]));
		asCASTProjection Decoded;
		ASSERT_THAT(AreEqual(asEASTCodecStatus::Succeeded, asCASTCodec::Decode(Bytes, Decoded)));
		ASSERT_THAT(IsTrue(Projection.Equals(Decoded)));

		uint32 NamedPattern[3] = {2, 1, 0};
		const int32 NamedOffset = FindU32Sequence(Bytes, MakeArrayView(NamedPattern, 3));
		ASSERT_THAT(IsTrue(NamedOffset != INDEX_NONE));

		TArray<uint8> Duplicate = Bytes;
		ASSERT_THAT(IsTrue(WriteU32(Duplicate, NamedOffset + 8, 1)));
		asCASTProjection DuplicateDecoded;
		ASSERT_THAT(AreEqual(asEASTCodecStatus::InvalidPayload, asCASTCodec::Decode(Duplicate, DuplicateDecoded)));

		TArray<uint8> Missing = Bytes;
		ASSERT_THAT(IsTrue(WriteU32(Missing, NamedOffset + 4, 1)));
		ASSERT_THAT(IsTrue(WriteU32(Missing, NamedOffset + 8, static_cast<uint32>(INDEX_NONE))));
		asCASTProjection MissingDecoded;
		ASSERT_THAT(AreEqual(asEASTCodecStatus::InvalidPayload, asCASTCodec::Decode(Missing, MissingDecoded)));

		TArray<uint8> Corrupt = Bytes;
		ASSERT_THAT(IsTrue(WriteU32(Corrupt, NamedOffset + 4, 99)));
		asCASTProjection CorruptDecoded;
		ASSERT_THAT(AreEqual(asEASTCodecStatus::InvalidPayload, asCASTCodec::Decode(Corrupt, CorruptDecoded)));
	}
};

#endif
