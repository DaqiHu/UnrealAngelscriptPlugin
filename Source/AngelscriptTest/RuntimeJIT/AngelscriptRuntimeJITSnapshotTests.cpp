#include "RuntimeJIT/AngelscriptRuntimeJITSnapshot.h"
#include "RuntimeJIT/AngelscriptRuntimeJITBackendRegistry.h"

#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "Async/Async.h"
#include "CQTest.h"
#include "Features/IModularFeatures.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptRuntimeJITSnapshotTests,
	"Angelscript.TestModule.RuntimeJIT.Coordinator.Snapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FAngelscriptHash256 MakeHash(const TCHAR* Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("runtime-jit-snapshot-test"));
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	static FAngelscriptRuntimeJITBackendId BackendId()
	{
		FAngelscriptRuntimeJITBackendId Result;
		check(FAngelscriptRuntimeJITBackendId::TryParse(
			TEXT("fake-snapshot"), Result));
		return Result;
	}

	static FAngelscriptFunctionRoute MakeRoute(
		asIScriptFunction& Function,
		const TCHAR* ExecutionRevision = TEXT("execution-v1"))
	{
		FAngelscriptFunctionRoute Route;
		Route.ModuleKey.Hash = MakeHash(TEXT("snapshot-module"));
		Route.Identity.FunctionKey.Hash = MakeHash(TEXT("snapshot-function"));
		Route.Identity.Content.Execution = MakeHash(ExecutionRevision);
		Route.Identity.Content.Debug = MakeHash(TEXT("debug-v1"));
		Route.Identity.Profile.Hash = MakeHash(TEXT("profile-v1"));
		Route.CanonicalDeclaration = UTF8_TO_TCHAR(
			Function.GetDeclaration(false, false, false));
		Route.Function = &Function;
		Route.NumericFunctionId = Function.GetId();
		Route.bHasVerifiedArtifactIdentity = true;
		return Route;
	}

	static FAngelscriptRuntimeJITSnapshotCaptureInput MakeCaptureInput(
		asIScriptFunction& Function,
		const FAngelscriptFunctionRoute& Route)
	{
		FAngelscriptRuntimeJITSnapshotCaptureInput Input;
		Input.Function = &Function;
		Input.Route = &Route;
		Input.BackendId = BackendId();
		Input.Platform = EAngelscriptRuntimeJITPlatform::Win64;
		Input.Configuration =
			EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment;
		Input.EngineNamespace = 901;
		Input.PublicationOrdinal = 7;
		Input.CancellationGeneration = 11;
		return Input;
	}

	static FAngelscriptRuntimeJITSnapshotCaptureResult CaptureScalar(
		asIScriptFunction& Function,
		const FAngelscriptFunctionRoute& Route,
		FAngelscriptRuntimeJITCompileSnapshot& OutSnapshot)
	{
		return FAngelscriptRuntimeJITCompileSnapshot::Capture(
			MakeCaptureInput(Function, Route), OutSnapshot);
	}

	class FCountingSession final :
		public IAngelscriptRuntimeJITBackendSession
	{
	public:
		FAngelscriptRuntimeJITCompileResult Compile(
			const FAngelscriptRuntimeJITCompileSnapshotView& Snapshot) override
		{
			++CompileCount;
			ObservedBytecodeWords = Snapshot.BytecodeWordCount;
			FAngelscriptRuntimeJITCompileResult Result;
			Result.Outcome = EAngelscriptRuntimeJITCompileOutcome::Unsupported;
			Result.Reason =
				EAngelscriptRuntimeJITDiagnosticReason::UnsupportedOpcode;
			Result.FunctionRevision = Snapshot.Header.FunctionRevision;
			Result.EntryAbiHash = Snapshot.Header.EntryAbiHash;
			return Result;
		}

		void Cancel(uint64) override
		{
		}

		int32 CompileCount = 0;
		uint32 ObservedBytecodeWords = 0;
	};

	class FCountingFactory final :
		public IAngelscriptRuntimeJITBackendFactory
	{
	public:
		FCountingFactory()
		{
			Metadata.BackendId = BackendId();
			Metadata.DisplayName = TEXT("Snapshot Fake");
			Metadata.SupportedPlatforms =
				EAngelscriptRuntimeJITPlatform::Win64;
			Metadata.SupportedConfigurations =
				EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment;
		}

		const FAngelscriptRuntimeJITBackendFactoryMetadata*
			GetMetadata() const override
		{
			return &Metadata;
		}

		TUniquePtr<IAngelscriptRuntimeJITBackendSession> CreateSession(
			const FAngelscriptRuntimeJITBackendSessionConfig&) override
		{
			TUniquePtr<FCountingSession> Created =
				MakeUnique<FCountingSession>();
			LastSession = Created.Get();
			return Created;
		}

		FAngelscriptRuntimeJITBackendFactoryMetadata Metadata;
		FCountingSession* LastSession = nullptr;
	};

	class FScopedFactoryRegistration final
	{
	public:
		explicit FScopedFactoryRegistration(FCountingFactory& InFactory)
			: Factory(InFactory)
		{
			IModularFeatures::Get().RegisterModularFeature(
				IAngelscriptRuntimeJITBackendFactory::FeatureName(), &Factory);
		}

		~FScopedFactoryRegistration()
		{
			IModularFeatures::Get().UnregisterModularFeature(
				IAngelscriptRuntimeJITBackendFactory::FeatureName(), &Factory);
		}

	private:
		FCountingFactory& Factory;
	};

	static FAngelscriptRuntimeJITSessionCreateResult CreateSession()
	{
		FAngelscriptRuntimeJITBackendSessionConfig Config;
		Config.BackendId = BackendId();
		Config.Platform = EAngelscriptRuntimeJITPlatform::Win64;
		Config.Configuration =
			EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment;
		Config.EngineNamespace = 901;
		return FAngelscriptRuntimeJITBackendSessionOwner::Create(Config);
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(CaptureCopiesStableScalarFunctionState)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			int SelectValue(int Value)
			{
				int Local = Value + 1;
				if (Local > 10)
				{
					return Local;
				}
				return 10;
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner, Engine, TEXT("ASRuntimeJITSnapshotScalar"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(),
			TEXT("Scalar snapshot module should compile")));
		asIScriptFunction* Function = ModuleScope.GetModule().GetFunctionByDecl(
			"int SelectValue(int)");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("Scalar snapshot function should resolve")));

		const FAngelscriptFunctionRoute Route = MakeRoute(*Function);
		FAngelscriptRuntimeJITCompileSnapshot Snapshot;
		const FAngelscriptRuntimeJITSnapshotCaptureResult Capture =
			FAngelscriptRuntimeJITCompileSnapshot::Capture(
				MakeCaptureInput(*Function, Route), Snapshot);
		ASSERT_THAT(IsTrue(Capture.IsCaptured(),
			FString::Printf(
				TEXT("Scalar function should produce an owned snapshot "
					"(code=%u reason=%u offset=%u validation=%u element=%d)"),
				static_cast<uint32>(Capture.Code),
				static_cast<uint32>(Capture.Reason),
				Capture.BytecodeOffset,
				static_cast<uint32>(Capture.Validation.Code),
				Capture.Validation.ElementIndex)));
		ASSERT_THAT(IsTrue(Snapshot.Validate().IsValid(),
			TEXT("Captured scalar snapshot should pass structural validation")));
		const FAngelscriptRuntimeJITCompileSnapshotView View = Snapshot.GetView();
		ASSERT_THAT(IsTrue(View.Header.ModuleKey == Route.ModuleKey,
			TEXT("Snapshot should retain the authoritative stable module key")));
		ASSERT_THAT(IsTrue(View.Header.FunctionKey == Route.Identity.FunctionKey,
			TEXT("Snapshot should retain the authoritative stable function key")));
		ASSERT_THAT(IsFalse(View.Header.FunctionRevision.IsZero(),
			TEXT("Snapshot should bind all copied content to a revision")));
		ASSERT_THAT(IsFalse(View.Header.EntryAbiHash.IsZero(),
			TEXT("Snapshot should identify its Runtime VMEntry ABI")));
		ASSERT_THAT(IsTrue(View.Header.Invocation ==
			EAngelscriptRuntimeJITInvocationKind::Global));
		ASSERT_THAT(IsTrue(View.Header.Receiver ==
			EAngelscriptRuntimeJITReceiverKind::None));
		FString FrameDetails;
		for (uint32 Index = 0; Index < View.FrameValueCount; ++Index)
		{
			const FAngelscriptRuntimeJITFrameValue& Value =
				View.FrameValues[Index];
			FrameDetails += FString::Printf(
				TEXT(" [%u:k=%u f=%u o=%u s=%u]"), Index,
				static_cast<uint32>(Value.Kind),
				static_cast<uint32>(Value.Flags),
				Value.ByteOffset, Value.ByteSize);
		}
		ASSERT_THAT(IsTrue(View.Header.FunctionProfile ==
			EAngelscriptRuntimeJITFunctionProfile::ScalarWholeFunction,
			FString::Printf(TEXT("Expected scalar profile, got %u; frames:%s"),
				static_cast<uint32>(View.Header.FunctionProfile),
				*FrameDetails)));
		ASSERT_THAT(IsTrue(View.FrameValueCount >= 3,
			TEXT("Result, parameter, and local scalar layout should be copied")));
		ASSERT_THAT(IsTrue(View.InstructionCount > 0,
			TEXT("Instruction boundaries should be decoded")));
		ASSERT_THAT(IsTrue(View.ControlFlowEdgeCount > 0,
			TEXT("Conditional control flow should be copied as indexed edges")));

		asUINT SourceWordCount = 0;
		const asDWORD* SourceBytecode = Function->GetByteCode(&SourceWordCount);
		ASSERT_THAT(AreEqual(static_cast<uint32>(SourceWordCount),
			View.BytecodeWordCount));
		ASSERT_THAT(IsTrue(View.Bytecode != SourceBytecode,
			TEXT("Worker bytecode must not alias live function storage")));
		ASSERT_THAT(IsTrue(FMemory::Memcmp(View.Bytecode, SourceBytecode,
			SourceWordCount * sizeof(asDWORD)) == 0,
			TEXT("Worker bytecode should be a byte-for-byte owned copy")));
		for (const uint32 InstructionOffset :
			Snapshot.GetInstructionWordOffsets())
		{
			const asEBCInstr Opcode = static_cast<asEBCInstr>(
				*reinterpret_cast<const uint8*>(
					&Snapshot.GetBytecode()[InstructionOffset]));
			if (Opcode == asBC_JitEntry)
			{
				ASSERT_THAT(IsTrue(
					asBC_PTRARG(&Snapshot.GetBytecode()[InstructionOffset]) == 0,
					TEXT("Snapshot JitEntry payloads must never carry live pointers")));
			}
		}

		FAngelscriptRuntimeJITCompileSnapshot CopiedSnapshot = Snapshot;
		const FAngelscriptRuntimeJITCompileSnapshotView CopiedView =
			CopiedSnapshot.GetView();
		ASSERT_THAT(IsTrue(CopiedView.Bytecode != View.Bytecode,
			TEXT("Copying a snapshot must rebuild views over copy-owned storage")));
		ASSERT_THAT(IsTrue(CopiedView.FrameValues != View.FrameValues,
			TEXT("Copied frame views must not point into the source snapshot")));

		FAngelscriptRuntimeJITCompileSnapshot SameSnapshot;
		const FAngelscriptRuntimeJITSnapshotCaptureResult SameCapture =
			CaptureScalar(*Function, Route, SameSnapshot);
		ASSERT_THAT(IsTrue(SameCapture.IsCaptured()));
		ASSERT_THAT(IsTrue(SameSnapshot.GetHeader().FunctionRevision
			== Snapshot.GetHeader().FunctionRevision,
			TEXT("Unchanged function content should have a stable revision")));
		FAngelscriptFunctionRoute ChangedRoute = MakeRoute(
			*Function, TEXT("execution-v2"));
		FAngelscriptRuntimeJITCompileSnapshot ChangedSnapshot;
		const FAngelscriptRuntimeJITSnapshotCaptureResult ChangedCapture =
			CaptureScalar(*Function, ChangedRoute, ChangedSnapshot);
		ASSERT_THAT(IsTrue(ChangedCapture.IsCaptured()));
		ASSERT_THAT(IsTrue(ChangedSnapshot.GetHeader().FunctionKey
			== Snapshot.GetHeader().FunctionKey));
		ASSERT_THAT(IsFalse(ChangedSnapshot.GetHeader().FunctionRevision
			== Snapshot.GetHeader().FunctionRevision,
			TEXT("Changed executable content should change the full revision")));
	}

	TEST_METHOD(CaptureClassifiesLoopSuspendBoundaryBeforeBackendWork)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			int SumUntil(int Limit)
			{
				int Sum = 0;
				for (int Index = 0; Index < Limit; ++Index)
				{
					Sum += Index;
				}
				return Sum;
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner, Engine, TEXT("ASRuntimeJITSnapshotLoop"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		asIScriptFunction* Function = ModuleScope.GetModule().GetFunctionByDecl(
			"int SumUntil(int)");
		ASSERT_THAT(IsNotNull(Function));
		const FAngelscriptFunctionRoute Route = MakeRoute(*Function);
		FAngelscriptRuntimeJITCompileSnapshot Snapshot;
		const FAngelscriptRuntimeJITSnapshotCaptureResult Capture =
			CaptureScalar(*Function, Route, Snapshot);
		ASSERT_THAT(IsTrue(Capture.IsCaptured()));
		ASSERT_THAT(IsTrue(Snapshot.GetHeader().FunctionProfile ==
			EAngelscriptRuntimeJITFunctionProfile::SuspendOrLatent,
			TEXT("A SUSPEND inside cyclic control flow needs whole-function VM")));
		ASSERT_THAT(IsTrue(Snapshot.GetControlFlowEdges().ContainsByPredicate(
			[](const FAngelscriptRuntimeJITControlFlowEdge& Edge)
			{
				return Edge.ToInstruction <= Edge.FromInstruction;
			}), TEXT("Loop fixture should contain a verified CFG back edge")));
	}

	TEST_METHOD(CaptureClassifiesManagedValuesBeforeBackendWork)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			int ObserveManagedValue(const FString& Value)
			{
				return 1;
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASRuntimeJITSnapshotManaged"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		asIScriptFunction* Function =
			ModuleScope.GetModule().GetFunctionByName("ObserveManagedValue");
		ASSERT_THAT(IsNotNull(Function));
		const FAngelscriptFunctionRoute Route = MakeRoute(*Function);
		FAngelscriptRuntimeJITCompileSnapshot Snapshot;
		const FAngelscriptRuntimeJITSnapshotCaptureResult Capture =
			CaptureScalar(*Function, Route, Snapshot);
		ASSERT_THAT(IsTrue(Capture.IsCaptured()));
		ASSERT_THAT(IsTrue(
			Snapshot.GetHeader().FunctionProfile
				== EAngelscriptRuntimeJITFunctionProfile::ManagedValues,
			TEXT("Managed values require whole-function VM fallback")));

		FCountingFactory Factory;
		FScopedFactoryRegistration Registration(Factory);
		FAngelscriptRuntimeJITSessionCreateResult Created = CreateSession();
		ASSERT_THAT(IsTrue(Created.IsCreated()));
		const FAngelscriptRuntimeJITCompileResult Result =
			Created.Owner->Compile(Snapshot.GetView());
		ASSERT_THAT(IsTrue(
			Result.Outcome == EAngelscriptRuntimeJITCompileOutcome::Unsupported));
		ASSERT_THAT(IsTrue(
			Result.Reason
				== EAngelscriptRuntimeJITDiagnosticReason::UnsupportedManagedValue));
		ASSERT_THAT(AreEqual(
			0,
			Factory.LastSession->CompileCount,
			TEXT("Managed-value snapshots must fail before concrete backend work")));
	}

	TEST_METHOD(CaptureCanonicalizesRepeatedDebugRecordsForOneLocalSlot)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			int ReusedLocalSlot(int Value)
			{
				int Local = Value + 1;
				return Local;
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner, Engine, TEXT("ASRuntimeJITReusedLocals"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(),
			TEXT("Reused-local fixture should compile")));
		asIScriptFunction* Function = ModuleScope.GetModule().GetFunctionByDecl(
			"int ReusedLocalSlot(int)");
		ASSERT_THAT(IsNotNull(Function));
		asCScriptFunction* Internal = static_cast<asCScriptFunction*>(Function);
		ASSERT_THAT(IsTrue(Internal->scriptData->variables.GetLength() > 0));
		asSScriptVariable* ReusedDebugRecord = nullptr;
		const int32 DebugVariableCount = static_cast<int32>(
			Internal->scriptData->variables.GetLength());
		for (int32 Index = 0; Index < DebugVariableCount; ++Index)
		{
			asSScriptVariable* const Candidate =
				Internal->scriptData->variables[Index];
			if (Candidate != nullptr && Candidate->stackOffset > 0)
			{
				ReusedDebugRecord = Candidate;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(ReusedDebugRecord));
		Internal->scriptData->variables.PushLast(ReusedDebugRecord);
		ON_SCOPE_EXIT
		{
			Internal->scriptData->variables.PopLast();
		};

		const FAngelscriptFunctionRoute Route = MakeRoute(*Function);
		FAngelscriptRuntimeJITCompileSnapshot Snapshot;
		const FAngelscriptRuntimeJITSnapshotCaptureResult Capture =
			CaptureScalar(*Function, Route, Snapshot);
		ASSERT_THAT(IsTrue(Capture.IsCaptured(),
			FString::Printf(
				TEXT("Repeated debug-local declarations sharing one physical slot "
					"must canonicalize to one valid frame range (validation=%u element=%d)"),
				static_cast<uint32>(Capture.Validation.Code),
				Capture.Validation.ElementIndex)));
		ASSERT_THAT(IsTrue(Snapshot.Validate().IsValid()));
	}

	TEST_METHOD(CaptureConvertsScriptCallsToOrderedStableTokens)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			int AddOne(int Value)
			{
				return Value + 1;
			}

			int CallTwice(int Value)
			{
				return AddOne(Value) + AddOne(Value);
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner, Engine, TEXT("ASRuntimeJITCallTokens"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(),
			TEXT("Call-token fixture should compile")));
		asIScriptFunction* Function = ModuleScope.GetModule().GetFunctionByDecl(
			"int CallTwice(int)");
		ASSERT_THAT(IsNotNull(Function));

		const FAngelscriptFunctionRoute Route = MakeRoute(*Function);
		FAngelscriptRuntimeJITCompileSnapshot Snapshot;
		const FAngelscriptRuntimeJITSnapshotCaptureResult Capture =
			CaptureScalar(*Function, Route, Snapshot);
		ASSERT_THAT(IsTrue(Capture.IsCaptured(),
			FString::Printf(
				TEXT("Script-call operands should be tokenized instead of rejected "
					"(code=%u reason=%u offset=%u)"),
				static_cast<uint32>(Capture.Code),
				static_cast<uint32>(Capture.Reason), Capture.BytecodeOffset)));
		ASSERT_THAT(AreEqual(static_cast<uint32>(2),
			static_cast<uint32>(Snapshot.GetHelperTokens().Num()),
			TEXT("Each call occurrence should receive one ordered token slot")));
		const FAngelscriptRuntimeJITHelperToken& First =
			Snapshot.GetHelperTokens()[0];
		const FAngelscriptRuntimeJITHelperToken& Second =
			Snapshot.GetHelperTokens()[1];
		ASSERT_THAT(IsTrue(First.Kind ==
			EAngelscriptRuntimeJITHelperTokenKind::ScriptFunction));
		ASSERT_THAT(AreEqual(static_cast<uint32>(0), First.SlotIndex));
		ASSERT_THAT(AreEqual(static_cast<uint32>(1), Second.SlotIndex));
		ASSERT_THAT(IsTrue(First.StableKey == Second.StableKey,
			TEXT("Repeated calls to one target should keep the same stable key")));
		ASSERT_THAT(IsTrue(First.ExpectedAbi == Second.ExpectedAbi,
			TEXT("Repeated calls to one target should keep the same expected ABI")));
		ASSERT_THAT(IsFalse(First.StableKey.IsZero()));
		ASSERT_THAT(IsFalse(First.ExpectedAbi.IsZero()));

		uint32 EncounteredCalls = 0;
		for (const uint32 InstructionOffset :
			Snapshot.GetInstructionWordOffsets())
		{
			const asEBCInstr Opcode = static_cast<asEBCInstr>(
				*reinterpret_cast<const uint8*>(
					&Snapshot.GetBytecode()[InstructionOffset]));
			if (Opcode == asBC_CALL)
			{
				ASSERT_THAT(AreEqual(EncounteredCalls,
					static_cast<uint32>(
						asBC_INTARG(&Snapshot.GetBytecode()[InstructionOffset])),
					TEXT("Canonical call operand should name its ordered token slot")));
				++EncounteredCalls;
			}
		}
		ASSERT_THAT(AreEqual(static_cast<uint32>(2), EncounteredCalls));
		ASSERT_THAT(IsTrue(Snapshot.Validate().IsValid()));
	}

	TEST_METHOD(StructuralValidationRejectsMalformedOwnedViews)
	{
		FAngelscriptRuntimeJITCompileSnapshotHeader Header;
		Header.BackendId = BackendId();
		Header.Platform = EAngelscriptRuntimeJITPlatform::Win64;
		Header.Configuration =
			EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment;
		Header.PointerWidth = sizeof(void*);
		Header.Endianness = EAngelscriptRuntimeJITEndianness::Little;
		Header.CallingConvention =
			EAngelscriptRuntimeJITCallingConvention::AngelscriptVMEntry;
		Header.Invocation = EAngelscriptRuntimeJITInvocationKind::Global;
		Header.Receiver = EAngelscriptRuntimeJITReceiverKind::None;
		Header.FunctionProfile =
			EAngelscriptRuntimeJITFunctionProfile::ScalarWholeFunction;
		Header.EntryAbiRevision =
			FAngelscriptRuntimeJITBackendAbi::EntryAbiRevision;
		Header.EngineNamespace = 901;
		Header.ModuleKey.Hash = MakeHash(TEXT("module"));
		Header.FunctionKey.Hash = MakeHash(TEXT("function"));
		Header.FunctionRevision = MakeHash(TEXT("revision"));
		Header.EntryAbiHash = MakeHash(TEXT("abi"));
		TArray<asDWORD> Bytecode{static_cast<asDWORD>(asBC_PshC4), 42,
			static_cast<asDWORD>(asBC_RET)};
		TArray<uint32> Offsets{0, 2};
		FAngelscriptRuntimeJITControlFlowEdge Edge;
		Edge.FromInstruction = 0;
		Edge.ToInstruction = 1;
		Edge.Kind = EAngelscriptRuntimeJITControlFlowEdgeKind::Fallthrough;
		TArray<FAngelscriptRuntimeJITControlFlowEdge> Edges{Edge};

		FAngelscriptRuntimeJITCompileSnapshot Valid =
			FAngelscriptRuntimeJITCompileSnapshot::FromOwnedData(
				Header, Bytecode, {}, Offsets, Edges, {});
		ASSERT_THAT(IsTrue(Valid.Validate().IsValid(),
			TEXT("Representative scalar owned snapshot should validate")));

		const int32 JitEntryWordCount =
			asBCTypeSize[asBCInfo[asBC_JitEntry].type];
		TArray<asDWORD> JitEntryBytecode;
		JitEntryBytecode.SetNumZeroed(JitEntryWordCount + 1);
		*reinterpret_cast<uint8*>(&JitEntryBytecode[0]) =
			static_cast<uint8>(asBC_JitEntry);
		JitEntryBytecode[JitEntryWordCount] = static_cast<asDWORD>(asBC_RET);
		TArray<uint32> JitEntryOffsets{
			0, static_cast<uint32>(JitEntryWordCount)};
		FAngelscriptRuntimeJITControlFlowEdge JitEntryEdge;
		JitEntryEdge.FromInstruction = 0;
		JitEntryEdge.ToInstruction = 1;
		JitEntryEdge.Kind =
			EAngelscriptRuntimeJITControlFlowEdgeKind::Fallthrough;
		FAngelscriptRuntimeJITCompileSnapshot ZeroJitEntry =
			FAngelscriptRuntimeJITCompileSnapshot::FromOwnedData(
				Header, JitEntryBytecode, {}, JitEntryOffsets, {JitEntryEdge}, {});
		ASSERT_THAT(IsTrue(ZeroJitEntry.Validate().IsValid()));
		asBC_PTRARG(JitEntryBytecode.GetData()) = 1;
		FAngelscriptRuntimeJITCompileSnapshot PointerJitEntry =
			FAngelscriptRuntimeJITCompileSnapshot::FromOwnedData(
				Header, JitEntryBytecode, {}, JitEntryOffsets, {JitEntryEdge}, {});
		ASSERT_THAT(IsTrue(PointerJitEntry.Validate().Code ==
			EAngelscriptRuntimeJITContractValidationCode::InvalidHelperToken,
			TEXT("Nonzero JitEntry payload must fail before worker access")));

		TArray<uint32> BadOffsets{0, 1};
		FAngelscriptRuntimeJITCompileSnapshot BadInstruction =
			FAngelscriptRuntimeJITCompileSnapshot::FromOwnedData(
				Header, Bytecode, {}, BadOffsets, Edges, {});
		ASSERT_THAT(IsTrue(BadInstruction.Validate().Code ==
			EAngelscriptRuntimeJITContractValidationCode::InvalidInstructionBoundary));

		TArray<FAngelscriptRuntimeJITControlFlowEdge> BadEdges = Edges;
		BadEdges[0].ToInstruction = 0;
		FAngelscriptRuntimeJITCompileSnapshot BadControlFlow =
			FAngelscriptRuntimeJITCompileSnapshot::FromOwnedData(
				Header, Bytecode, {}, Offsets, BadEdges, {});
		ASSERT_THAT(IsTrue(BadControlFlow.Validate().Code ==
			EAngelscriptRuntimeJITContractValidationCode::InvalidControlFlowEdge));

		FAngelscriptRuntimeJITFrameValue First;
		First.Kind = EAngelscriptRuntimeJITScalarKind::Int32;
		First.Flags = EAngelscriptRuntimeJITFrameValueFlags::Parameter;
		First.ByteSize = 4;
		First.Alignment = 4;
		FAngelscriptRuntimeJITFrameValue Overlap = First;
		Overlap.SourceIndex = 1;
		Header.ParameterFrameByteSize = 4;
		FAngelscriptRuntimeJITCompileSnapshot BadFrame =
			FAngelscriptRuntimeJITCompileSnapshot::FromOwnedData(
				Header, Bytecode, {First, Overlap}, Offsets, Edges, {});
		ASSERT_THAT(IsTrue(BadFrame.Validate().Code ==
			EAngelscriptRuntimeJITContractValidationCode::InvalidFrameLayout));

		Header.ParameterFrameByteSize = 0;
		Header.FunctionProfile =
			static_cast<EAngelscriptRuntimeJITFunctionProfile>(MAX_uint8);
		FAngelscriptRuntimeJITCompileSnapshot UnknownProfile =
			FAngelscriptRuntimeJITCompileSnapshot::FromOwnedData(
				Header, Bytecode, {}, Offsets, Edges, {});
		ASSERT_THAT(IsTrue(UnknownProfile.Validate().Code ==
			EAngelscriptRuntimeJITContractValidationCode::InvalidFunctionProfile));

		Header.FunctionProfile =
			EAngelscriptRuntimeJITFunctionProfile::ScalarWholeFunction;
		Header.EntryAbiRevision += 1;
		FAngelscriptRuntimeJITCompileSnapshot WrongAbi =
			FAngelscriptRuntimeJITCompileSnapshot::FromOwnedData(
				Header, Bytecode, {}, Offsets, Edges, {});
		ASSERT_THAT(IsTrue(WrongAbi.Validate().Code ==
			EAngelscriptRuntimeJITContractValidationCode::InvalidEntryAbi));

		Header.EntryAbiRevision =
			FAngelscriptRuntimeJITBackendAbi::EntryAbiRevision;
		FAngelscriptRuntimeJITHelperToken FirstToken;
		FirstToken.Kind = EAngelscriptRuntimeJITHelperTokenKind::ScriptFunction;
		FirstToken.StableKey = MakeHash(TEXT("helper-a"));
		FirstToken.ExpectedAbi = MakeHash(TEXT("helper-abi-a"));
		FAngelscriptRuntimeJITHelperToken SecondToken;
		SecondToken.Kind = EAngelscriptRuntimeJITHelperTokenKind::ScriptFunction;
		SecondToken.SlotIndex = 1;
		SecondToken.StableKey = MakeHash(TEXT("helper-b"));
		SecondToken.ExpectedAbi = MakeHash(TEXT("helper-abi-b"));
		TArray<asDWORD> TokenizedCallBytecode{
			static_cast<asDWORD>(asBC_CALL), 0,
			static_cast<asDWORD>(asBC_CALL), 1,
			static_cast<asDWORD>(asBC_RET)};
		TArray<uint32> TokenizedCallOffsets{0, 2, 4};
		FAngelscriptRuntimeJITControlFlowEdge FirstCallEdge;
		FirstCallEdge.FromInstruction = 0;
		FirstCallEdge.ToInstruction = 1;
		FirstCallEdge.Kind =
			EAngelscriptRuntimeJITControlFlowEdgeKind::Fallthrough;
		FAngelscriptRuntimeJITControlFlowEdge SecondCallEdge = FirstCallEdge;
		SecondCallEdge.FromInstruction = 1;
		SecondCallEdge.ToInstruction = 2;
		FAngelscriptRuntimeJITCompileSnapshot OrderedTokens =
			FAngelscriptRuntimeJITCompileSnapshot::FromOwnedData(
				Header, TokenizedCallBytecode, {}, TokenizedCallOffsets,
				{FirstCallEdge, SecondCallEdge},
				{FirstToken, SecondToken});
		ASSERT_THAT(IsTrue(OrderedTokens.Validate().IsValid(),
			TEXT("Ordered stable helper/reference tokens should validate")));
		SecondToken.SlotIndex = 0;
		FAngelscriptRuntimeJITCompileSnapshot DuplicateTokenSlot =
			FAngelscriptRuntimeJITCompileSnapshot::FromOwnedData(
				Header, TokenizedCallBytecode, {}, TokenizedCallOffsets,
				{FirstCallEdge, SecondCallEdge},
				{FirstToken, SecondToken});
		ASSERT_THAT(IsTrue(DuplicateTokenSlot.Validate().Code ==
			EAngelscriptRuntimeJITContractValidationCode::InvalidHelperToken));
	}

	TEST_METHOD(StructuralValidationRejectsHighBitsInPointerTokenSlots)
	{
		FAngelscriptRuntimeJITCompileSnapshotHeader Header;
		Header.BackendId = BackendId();
		Header.Platform = EAngelscriptRuntimeJITPlatform::Win64;
		Header.Configuration =
			EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment;
		Header.PointerWidth = sizeof(void*);
		Header.Endianness = EAngelscriptRuntimeJITEndianness::Little;
		Header.CallingConvention =
			EAngelscriptRuntimeJITCallingConvention::AngelscriptVMEntry;
		Header.Invocation = EAngelscriptRuntimeJITInvocationKind::Global;
		Header.Receiver = EAngelscriptRuntimeJITReceiverKind::None;
		Header.FunctionProfile =
			EAngelscriptRuntimeJITFunctionProfile::ScalarWholeFunction;
		Header.EntryAbiRevision =
			FAngelscriptRuntimeJITBackendAbi::EntryAbiRevision;
		Header.EngineNamespace = 901;
		Header.ModuleKey.Hash = MakeHash(TEXT("module"));
		Header.FunctionKey.Hash = MakeHash(TEXT("function"));
		Header.FunctionRevision = MakeHash(TEXT("revision"));
		Header.EntryAbiHash = MakeHash(TEXT("abi"));

		const int32 CallWordCount =
			asBCTypeSize[asBCInfo[asBC_CALLSYS].type];
		TArray<asDWORD> Bytecode;
		Bytecode.SetNumZeroed(CallWordCount + 1);
		*reinterpret_cast<uint8*>(&Bytecode[0]) =
			static_cast<uint8>(asBC_CALLSYS);
		asBC_PTRARG(Bytecode.GetData()) = static_cast<asPWORD>(
			static_cast<uint64>(MAX_uint32) + 1ull);
		Bytecode[CallWordCount] = static_cast<asDWORD>(asBC_RET);

		TArray<uint32> Offsets{0, static_cast<uint32>(CallWordCount)};
		FAngelscriptRuntimeJITControlFlowEdge Edge;
		Edge.FromInstruction = 0;
		Edge.ToInstruction = 1;
		Edge.Kind = EAngelscriptRuntimeJITControlFlowEdgeKind::Fallthrough;
		FAngelscriptRuntimeJITHelperToken Token;
		Token.Kind = EAngelscriptRuntimeJITHelperTokenKind::SystemFunction;
		Token.StableKey = MakeHash(TEXT("system-function"));
		Token.ExpectedAbi = MakeHash(TEXT("system-function-abi"));

		const FAngelscriptRuntimeJITCompileSnapshot Snapshot =
			FAngelscriptRuntimeJITCompileSnapshot::FromOwnedData(
				Header, MoveTemp(Bytecode), {}, MoveTemp(Offsets), {Edge}, {Token});
		ASSERT_THAT(IsTrue(Snapshot.Validate().Code ==
			EAngelscriptRuntimeJITContractValidationCode::InvalidHelperToken,
			TEXT("A pointer-bearing opcode must not hide raw high bits behind "
				"a matching 32-bit token slot")));
	}

	TEST_METHOD(ReceiverAndInvalidSnapshotsNeverReachConcreteBackend)
	{
		FCountingFactory Factory;
		FScopedFactoryRegistration Registration(Factory);
		FAngelscriptRuntimeJITSessionCreateResult Created = CreateSession();
		ASSERT_THAT(IsTrue(Created.IsCreated(),
			TEXT("Snapshot fake backend session should be created")));

		FAngelscriptRuntimeJITCompileSnapshotHeader Header;
		Header.BackendId = BackendId();
		Header.Platform = EAngelscriptRuntimeJITPlatform::Win64;
		Header.Configuration =
			EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment;
		Header.PointerWidth = sizeof(void*);
		Header.Endianness = EAngelscriptRuntimeJITEndianness::Little;
		Header.CallingConvention =
			EAngelscriptRuntimeJITCallingConvention::AngelscriptVMEntry;
		Header.Invocation = EAngelscriptRuntimeJITInvocationKind::Global;
		Header.Receiver = EAngelscriptRuntimeJITReceiverKind::None;
		Header.FunctionProfile =
			EAngelscriptRuntimeJITFunctionProfile::ScalarWholeFunction;
		Header.EntryAbiRevision =
			FAngelscriptRuntimeJITBackendAbi::EntryAbiRevision;
		Header.EngineNamespace = 901;
		Header.ModuleKey.Hash = MakeHash(TEXT("module"));
		Header.FunctionKey.Hash = MakeHash(TEXT("function"));
		Header.FunctionRevision = MakeHash(TEXT("revision"));
		Header.EntryAbiHash = MakeHash(TEXT("abi"));
		TArray<asDWORD> Bytecode{static_cast<asDWORD>(asBC_RET)};
		TArray<uint32> Offsets{0};

		auto Owned = [&](const FAngelscriptRuntimeJITCompileSnapshotHeader& InHeader,
			TArray<FAngelscriptRuntimeJITFrameValue> Values = {})
		{
			return FAngelscriptRuntimeJITCompileSnapshot::FromOwnedData(
				InHeader, Bytecode, MoveTemp(Values), Offsets, {}, {});
		};
		FAngelscriptRuntimeJITCompileSnapshot Valid = Owned(Header);
		Created.Owner->Compile(Valid.GetView());
		ASSERT_THAT(AreEqual(1, Factory.LastSession->CompileCount,
			TEXT("Valid scalar snapshot should reach the concrete backend")));

		TArray<asDWORD> TruncatedBytecode{
			static_cast<asDWORD>(asBC_PshC4)};
		TArray<uint32> TruncatedOffsets{0};
		const FAngelscriptRuntimeJITCompileSnapshot MalformedBytecode =
			FAngelscriptRuntimeJITCompileSnapshot::FromOwnedData(
				Header, MoveTemp(TruncatedBytecode), {},
				MoveTemp(TruncatedOffsets), {}, {});
		Created.Owner->Compile(MalformedBytecode.GetView());
		ASSERT_THAT(AreEqual(1, Factory.LastSession->CompileCount,
			TEXT("Truncated bytecode must fail before the backend")));

		TArray<asDWORD> BranchBytecode{
			static_cast<asDWORD>(asBC_PshC4), 42,
			static_cast<asDWORD>(asBC_RET)};
		TArray<uint32> BranchOffsets{0, 2};
		FAngelscriptRuntimeJITControlFlowEdge InvalidEdge;
		InvalidEdge.FromInstruction = 0;
		InvalidEdge.ToInstruction = 0;
		InvalidEdge.Kind =
			EAngelscriptRuntimeJITControlFlowEdgeKind::Fallthrough;
		const FAngelscriptRuntimeJITCompileSnapshot InvalidControlFlow =
			FAngelscriptRuntimeJITCompileSnapshot::FromOwnedData(
				Header, MoveTemp(BranchBytecode), {},
				MoveTemp(BranchOffsets), {InvalidEdge}, {});
		Created.Owner->Compile(InvalidControlFlow.GetView());
		ASSERT_THAT(AreEqual(1, Factory.LastSession->CompileCount,
			TEXT("Invalid control flow must fail before the backend")));

		Header.ParameterFrameByteSize = 4;
		FAngelscriptRuntimeJITFrameValue FirstFrameValue;
		FirstFrameValue.Kind = EAngelscriptRuntimeJITScalarKind::Int32;
		FirstFrameValue.Flags =
			EAngelscriptRuntimeJITFrameValueFlags::Parameter;
		FirstFrameValue.ByteSize = 4;
		FirstFrameValue.Alignment = 4;
		FAngelscriptRuntimeJITFrameValue OverlappingFrameValue =
			FirstFrameValue;
		OverlappingFrameValue.SourceIndex = 1;
		Created.Owner->Compile(Owned(
			Header, {FirstFrameValue, OverlappingFrameValue}).GetView());
		ASSERT_THAT(AreEqual(1, Factory.LastSession->CompileCount,
			TEXT("Overlapping frame values must fail before the backend")));
		Header.ParameterFrameByteSize = 0;

		Header.Invocation =
			static_cast<EAngelscriptRuntimeJITInvocationKind>(MAX_uint8);
		Created.Owner->Compile(Owned(Header).GetView());
		ASSERT_THAT(AreEqual(1, Factory.LastSession->CompileCount,
			TEXT("Unknown invocation kinds must fail before the backend")));
		Header.Invocation = EAngelscriptRuntimeJITInvocationKind::Global;

		Header.Receiver =
			static_cast<EAngelscriptRuntimeJITReceiverKind>(MAX_uint8);
		Created.Owner->Compile(Owned(Header).GetView());
		ASSERT_THAT(AreEqual(1, Factory.LastSession->CompileCount,
			TEXT("Unknown receiver kinds must fail before the backend")));
		Header.Receiver = EAngelscriptRuntimeJITReceiverKind::None;

		Header.FunctionProfile =
			static_cast<EAngelscriptRuntimeJITFunctionProfile>(MAX_uint8);
		Created.Owner->Compile(Owned(Header).GetView());
		ASSERT_THAT(AreEqual(1, Factory.LastSession->CompileCount,
			TEXT("Unknown profile must fail before the backend")));

		struct FProfileFallbackCase
		{
			EAngelscriptRuntimeJITFunctionProfile Profile;
			EAngelscriptRuntimeJITDiagnosticReason Reason;
		};
		const FProfileFallbackCase ProfileFallbackCases[] = {
			{EAngelscriptRuntimeJITFunctionProfile::ManagedValues,
				EAngelscriptRuntimeJITDiagnosticReason::UnsupportedManagedValue},
			{EAngelscriptRuntimeJITFunctionProfile::SuspendOrLatent,
				EAngelscriptRuntimeJITDiagnosticReason::SuspendOrLatent},
			{EAngelscriptRuntimeJITFunctionProfile::ExceptionCleanup,
				EAngelscriptRuntimeJITDiagnosticReason::ExceptionCleanup},
		};
		for (const FProfileFallbackCase& Fallback : ProfileFallbackCases)
		{
			Header.FunctionProfile = Fallback.Profile;
			const FAngelscriptRuntimeJITCompileResult ProfileResult =
				Created.Owner->Compile(Owned(Header).GetView());
			ASSERT_THAT(IsTrue(
				ProfileResult.Outcome
					== EAngelscriptRuntimeJITCompileOutcome::Unsupported));
			ASSERT_THAT(IsTrue(ProfileResult.Reason == Fallback.Reason));
			ASSERT_THAT(AreEqual(
				1,
				Factory.LastSession->CompileCount,
				TEXT("Non-scalar whole-function profiles must fail before the backend")));
		}

		Header.FunctionProfile =
			EAngelscriptRuntimeJITFunctionProfile::ScalarWholeFunction;
		Header.EntryAbiRevision += 1;
		const FAngelscriptRuntimeJITCompileResult AbiResult =
			Created.Owner->Compile(Owned(Header).GetView());
		ASSERT_THAT(IsTrue(AbiResult.Reason ==
			EAngelscriptRuntimeJITDiagnosticReason::EntryAbiMismatch));
		ASSERT_THAT(AreEqual(1, Factory.LastSession->CompileCount,
			TEXT("Entry ABI mismatch must fail before the backend")));

		Header.EntryAbiRevision =
			FAngelscriptRuntimeJITBackendAbi::EntryAbiRevision;
		Header.Receiver =
			EAngelscriptRuntimeJITReceiverKind::DeclaredParameterAlias;
		Header.ReceiverParameterIndex = 0;
		Header.ParameterFrameByteSize = 4;
		FAngelscriptRuntimeJITFrameValue Receiver;
		Receiver.Kind = EAngelscriptRuntimeJITScalarKind::Int32;
		Receiver.Flags = EAngelscriptRuntimeJITFrameValueFlags::Parameter
			| EAngelscriptRuntimeJITFrameValueFlags::ReceiverAlias;
		Receiver.ByteSize = 4;
		Receiver.Alignment = 4;
		const FAngelscriptRuntimeJITCompileResult ReceiverResult =
			Created.Owner->Compile(Owned(Header, {Receiver}).GetView());
		ASSERT_THAT(IsTrue(ReceiverResult.Outcome ==
			EAngelscriptRuntimeJITCompileOutcome::Unsupported));
		ASSERT_THAT(IsTrue(ReceiverResult.Reason ==
			EAngelscriptRuntimeJITDiagnosticReason::UnsupportedReceiver));
		ASSERT_THAT(AreEqual(1, Factory.LastSession->CompileCount,
			TEXT("Declared-parameter receiver aliases use whole-function VM fallback")));

		Header.ReceiverParameterIndex = 1;
		Created.Owner->Compile(Owned(Header, {Receiver}).GetView());
		ASSERT_THAT(AreEqual(1, Factory.LastSession->CompileCount,
			TEXT("Malformed receiver aliases must fail before the backend")));
	}

	TEST_METHOD(CaptureNormalizesExternalImplicitThisWithoutAddingAThisSlot)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			int Evaluate(int Target, int Delta)
			{
				return Target + Delta;
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner, Engine, TEXT("ASRuntimeJITExternalReceiver"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(),
			TEXT("External receiver fixture should compile")));
		asIScriptFunction* Function = ModuleScope.GetModule().GetFunctionByDecl(
			"int Evaluate(int, int)");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("External receiver fixture function should resolve")));
		asCScriptFunction* Internal = static_cast<asCScriptFunction*>(Function);
		Internal->traits.SetTrait(asTRAIT_EXTERNAL_IMPLICIT_THIS, true);
		ON_SCOPE_EXIT
		{
			Internal->traits.SetTrait(asTRAIT_EXTERNAL_IMPLICIT_THIS, false);
		};
		const FAngelscriptFunctionRoute Route = MakeRoute(*Function);
		FAngelscriptRuntimeJITCompileSnapshot Snapshot;
		const FAngelscriptRuntimeJITSnapshotCaptureResult Capture =
			CaptureScalar(*Function, Route, Snapshot);
		ASSERT_THAT(IsTrue(Capture.IsCaptured()));
		const auto& Header = Snapshot.GetHeader();
		ASSERT_THAT(IsTrue(Header.Invocation ==
			EAngelscriptRuntimeJITInvocationKind::Global));
		ASSERT_THAT(IsTrue(Header.Receiver ==
			EAngelscriptRuntimeJITReceiverKind::DeclaredParameterAlias));
		ASSERT_THAT(AreEqual(0, Header.ReceiverParameterIndex));
		ASSERT_THAT(AreEqual(static_cast<uint32>(2),
			static_cast<uint32>(Snapshot.GetFrameValues().FilterByPredicate(
				[](const FAngelscriptRuntimeJITFrameValue& Value)
				{
					return EnumHasAnyFlags(Value.Flags,
						EAngelscriptRuntimeJITFrameValueFlags::Parameter);
				}).Num()),
			TEXT("Receiver alias must not erase parameter zero or add a native slot")));
		ASSERT_THAT(IsTrue(EnumHasAnyFlags(Snapshot.GetFrameValues()[1].Flags,
			EAngelscriptRuntimeJITFrameValueFlags::ReceiverAlias),
			TEXT("Declared parameter zero should carry the receiver-alias marker")));

		FCountingFactory Factory;
		FScopedFactoryRegistration Registration(Factory);
		FAngelscriptRuntimeJITSessionCreateResult Created = CreateSession();
		ASSERT_THAT(IsTrue(Created.IsCreated(),
			TEXT("External receiver rejection session should be created")));
		const FAngelscriptRuntimeJITCompileResult CompileResult =
			Created.Owner->Compile(Snapshot.GetView());
		ASSERT_THAT(IsTrue(CompileResult.Outcome ==
			EAngelscriptRuntimeJITCompileOutcome::Unsupported));
		ASSERT_THAT(IsTrue(CompileResult.Reason ==
			EAngelscriptRuntimeJITDiagnosticReason::UnsupportedReceiver));
		ASSERT_THAT(AreEqual(0, Factory.LastSession->CompileCount,
			TEXT("A captured external implicit receiver must fall back before "
				"the concrete backend")));
	}

	TEST_METHOD(QueuedSnapshotSurvivesImmediateModuleDiscard)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FCountingFactory Factory;
		FScopedFactoryRegistration Registration(Factory);
		FAngelscriptRuntimeJITSessionCreateResult Created = CreateSession();
		ASSERT_THAT(IsTrue(Created.IsCreated()));
		FEvent* StartCompile = FPlatformProcess::GetSynchEventFromPool(true);
		ON_SCOPE_EXIT
		{
			FPlatformProcess::ReturnSynchEventToPool(StartCompile);
		};
		TFuture<FAngelscriptRuntimeJITCompileResult> Future;
		{
			const FString ScriptSource = ASTEST_AS(R"AS(
				int QueuedValue(int Value)
				{
					return Value + 5;
				}
				)AS");
			FScopedAngelscriptModule ModuleScope(
				*TestRunner, Engine, TEXT("ASRuntimeJITDiscarded"), ScriptSource);
			ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
			asIScriptFunction* Function = ModuleScope.GetModule().GetFunctionByDecl(
				"int QueuedValue(int)");
			ASSERT_THAT(IsNotNull(Function));
			const FAngelscriptFunctionRoute Route = MakeRoute(*Function);
			FAngelscriptRuntimeJITCompileSnapshot Snapshot;
			const FAngelscriptRuntimeJITSnapshotCaptureResult Capture =
				CaptureScalar(*Function, Route, Snapshot);
			ASSERT_THAT(IsTrue(Capture.IsCaptured()));
			Future = Async(EAsyncExecution::ThreadPool,
				[StartCompile, Snapshot = MoveTemp(Snapshot), &Created]() mutable
				{
					StartCompile->Wait(5000);
					return Created.Owner->Compile(Snapshot.GetView());
				});
		}
		// ModuleScope has discarded the source module before worker access.
		StartCompile->Trigger();
		const FAngelscriptRuntimeJITCompileResult Result = Future.Get();
		ASSERT_THAT(IsTrue(Result.Outcome ==
			EAngelscriptRuntimeJITCompileOutcome::Unsupported));
		ASSERT_THAT(AreEqual(1, Factory.LastSession->CompileCount));
		ASSERT_THAT(IsTrue(Factory.LastSession->ObservedBytecodeWords > 0,
			TEXT("Backend should read only queue-owned copied bytecode after discard")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
