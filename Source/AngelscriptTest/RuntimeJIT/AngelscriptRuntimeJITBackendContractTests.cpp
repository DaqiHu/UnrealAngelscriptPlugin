#include "JIT/AngelscriptRuntimeJITBackend.h"
#include "RuntimeJIT/AngelscriptRuntimeJITBackendRegistry.h"

#include "CQTest.h"
#include "Async/Async.h"
#include "Features/IModularFeatures.h"

#include <type_traits>

#if WITH_ANGELSCRIPT_UNITTESTS

struct FAngelscriptStaticJITBackendId;

namespace AngelscriptRuntimeJITBackendContractTests_Private
{
	static FAngelscriptHash256 MakeHash(const TCHAR* Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("runtime-jit-backend-contract-test"));
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	static void RuntimeVmEntry(FScriptExecution&, asDWORD*, asQWORD*)
	{
	}

	static void ReleaseLease(void* UserData)
	{
		++*static_cast<int32*>(UserData);
	}

	static FAngelscriptRuntimeJITBackendId BackendId(const TCHAR* Value)
	{
		FAngelscriptRuntimeJITBackendId Result;
		check(FAngelscriptRuntimeJITBackendId::TryParse(Value, Result));
		return Result;
	}

	static FAngelscriptRuntimeJITCompileSnapshotView MakeSnapshot()
	{
		static const asDWORD Bytecode[] = {
			static_cast<asDWORD>(asBC_PshC4),
			1u,
			static_cast<asDWORD>(asBC_RET),
		};
		static const uint32 InstructionWordOffsets[] = {0u, 2u};
		static const FAngelscriptRuntimeJITControlFlowEdge FallthroughEdge = []
		{
			FAngelscriptRuntimeJITControlFlowEdge Edge;
			Edge.FromInstruction = 0;
			Edge.ToInstruction = 1;
			Edge.Kind =
				EAngelscriptRuntimeJITControlFlowEdgeKind::Fallthrough;
			return Edge;
		}();
		FAngelscriptRuntimeJITCompileSnapshotView Snapshot;
		Snapshot.Header.BackendId = BackendId(TEXT("fake-runtime"));
		Snapshot.Header.Platform =
			EAngelscriptRuntimeJITPlatform::Win64;
		Snapshot.Header.Configuration =
			EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment;
		Snapshot.Header.PointerWidth = 8;
		Snapshot.Header.Endianness =
			EAngelscriptRuntimeJITEndianness::Little;
		Snapshot.Header.CallingConvention =
			EAngelscriptRuntimeJITCallingConvention::AngelscriptVMEntry;
		Snapshot.Header.EntryAbiRevision =
			FAngelscriptRuntimeJITBackendAbi::EntryAbiRevision;
		Snapshot.Header.EngineNamespace = 17;
		Snapshot.Header.PublicationOrdinal = 3;
		Snapshot.Header.CancellationGeneration = 5;
		Snapshot.Header.ModuleKey.Hash = MakeHash(TEXT("module"));
		Snapshot.Header.FunctionKey.Hash = MakeHash(TEXT("function"));
		Snapshot.Header.FunctionRevision = MakeHash(TEXT("revision"));
		Snapshot.Header.EntryAbiHash = MakeHash(TEXT("entry-abi"));
		Snapshot.Bytecode = Bytecode;
		Snapshot.BytecodeWordCount = UE_ARRAY_COUNT(Bytecode);
		Snapshot.InstructionWordOffsets = InstructionWordOffsets;
		Snapshot.InstructionCount = UE_ARRAY_COUNT(InstructionWordOffsets);
		Snapshot.ControlFlowEdges = &FallthroughEdge;
		Snapshot.ControlFlowEdgeCount = 1;
		return Snapshot;
	}

	class FFakeSession final : public IAngelscriptRuntimeJITBackendSession
	{
	public:
		explicit FFakeSession(int32* InDestroyedCount)
			: DestroyedCount(InDestroyedCount)
		{
			CancelEvent = FPlatformProcess::GetSynchEventFromPool(true);
		}

		~FFakeSession() override
		{
			CancelEvent->Trigger();
			FPlatformProcess::ReturnSynchEventToPool(CancelEvent);
			++*DestroyedCount;
		}

		FAngelscriptRuntimeJITCompileResult Compile(
			const FAngelscriptRuntimeJITCompileSnapshotView& Snapshot) override
		{
			++CompileCount;
			const int32 ActiveCount = ActiveCompileCount.Increment();
			{
				FScopeLock Lock(&ConcurrencyMutex);
				MaxActiveCompileCount = FMath::Max(
					MaxActiveCompileCount, ActiveCount);
			}
			if (CompileDelaySeconds > 0.0f)
			{
				FPlatformProcess::Sleep(CompileDelaySeconds);
			}
			if (bBlockUntilCancelled)
			{
				CompileEntered.Increment();
				CancelEvent->Wait(5000);
			}
			FAngelscriptRuntimeJITCompileResult Result;
			Result.Outcome = NextOutcome;
			Result.Reason = NextReason;
			Result.BytecodeOffset = NextBytecodeOffset;
			Result.FunctionRevision = Snapshot.Header.FunctionRevision;
			if (NextOutcome == EAngelscriptRuntimeJITCompileOutcome::Compiled)
			{
				Result.VMEntry = &RuntimeVmEntry;
				Result.CodeSize = 64;
				Result.CompileLatencyMicroseconds = 11;
				Result.EntryAbiHash = Snapshot.Header.EntryAbiHash;
				FAngelscriptRuntimeJITCodeLeaseView LeaseView;
				LeaseView.UserData = LeaseReleaseCount;
				LeaseView.Release = &ReleaseLease;
				Result.CodeLease =
					FAngelscriptRuntimeJITCodeLease::Adopt(LeaseView);
			}
			ActiveCompileCount.Decrement();
			return Result;
		}

		void Cancel(const uint64 CancellationGeneration) override
		{
			LastCancellationGeneration = CancellationGeneration;
			CancelEvent->Trigger();
		}

		EAngelscriptRuntimeJITCompileOutcome NextOutcome =
			EAngelscriptRuntimeJITCompileOutcome::Unsupported;
		EAngelscriptRuntimeJITDiagnosticReason NextReason =
			EAngelscriptRuntimeJITDiagnosticReason::UnsupportedOpcode;
		uint32 NextBytecodeOffset = MAX_uint32;
		int32* LeaseReleaseCount = nullptr;
		float CompileDelaySeconds = 0.0f;
		bool bBlockUntilCancelled = false;
		int32 CompileCount = 0;
		FThreadSafeCounter ActiveCompileCount;
		FThreadSafeCounter CompileEntered;
		FCriticalSection ConcurrencyMutex;
		int32 MaxActiveCompileCount = 0;
		uint64 LastCancellationGeneration = 0;

	private:
		int32* DestroyedCount = nullptr;
		FEvent* CancelEvent = nullptr;
	};

	class FFakeFactory final : public IAngelscriptRuntimeJITBackendFactory
	{
	public:
		FFakeFactory(const TCHAR* InBackendId, const TCHAR* InDisplayName)
		{
			Metadata.BackendId = BackendId(InBackendId);
			DisplayName = InDisplayName;
			Metadata.DisplayName = *DisplayName;
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
			const FAngelscriptRuntimeJITBackendSessionConfig& Config) override
		{
			++CreateCount;
			LastConfig = Config;
			TUniquePtr<FFakeSession> Session =
				MakeUnique<FFakeSession>(&DestroyedCount);
			Session->NextOutcome = NextOutcome;
			Session->NextReason = NextReason;
			Session->NextBytecodeOffset = NextBytecodeOffset;
			Session->LeaseReleaseCount = &LeaseReleaseCount;
			Session->CompileDelaySeconds = CompileDelaySeconds;
			Session->bBlockUntilCancelled = bBlockUntilCancelled;
			LastSession = Session.Get();
			return Session;
		}

		FString DisplayName;
		FAngelscriptRuntimeJITBackendFactoryMetadata Metadata;
		FAngelscriptRuntimeJITBackendSessionConfig LastConfig;
		EAngelscriptRuntimeJITCompileOutcome NextOutcome =
			EAngelscriptRuntimeJITCompileOutcome::Unsupported;
		EAngelscriptRuntimeJITDiagnosticReason NextReason =
			EAngelscriptRuntimeJITDiagnosticReason::UnsupportedOpcode;
		uint32 NextBytecodeOffset = MAX_uint32;
		FFakeSession* LastSession = nullptr;
		int32 CreateCount = 0;
		int32 DestroyedCount = 0;
		int32 LeaseReleaseCount = 0;
		float CompileDelaySeconds = 0.0f;
		bool bBlockUntilCancelled = false;
	};

	class FScopedFactoryRegistration final
	{
	public:
		explicit FScopedFactoryRegistration(
			IAngelscriptRuntimeJITBackendFactory& InFactory)
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
		IAngelscriptRuntimeJITBackendFactory& Factory;
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptRuntimeJITBackendContractTests,
	"Angelscript.TestModule.RuntimeJIT.Coordinator.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static_assert(std::is_standard_layout_v<
		FAngelscriptRuntimeJITBackendId>);
	static_assert(std::is_trivially_copyable_v<
		FAngelscriptRuntimeJITBackendId>);
	static_assert(std::is_standard_layout_v<
		FAngelscriptRuntimeJITBackendFactoryMetadata>);
	static_assert(std::is_standard_layout_v<
		FAngelscriptRuntimeJITCompileSnapshotHeader>);
	static_assert(std::is_standard_layout_v<
		FAngelscriptRuntimeJITCompileSnapshotView>);
	static_assert(std::is_standard_layout_v<
		FAngelscriptRuntimeJITCodeLeaseView>);

public:
	TEST_METHOD(BackendIdsAreStableValidatedAndTypeSeparated)
	{
		using namespace AngelscriptRuntimeJITBackendContractTests_Private;
		ASSERT_THAT(AreEqual(static_cast<uint32>(2),
			FAngelscriptRuntimeJITBackendAbi::Revision,
			TEXT("Snapshot-header ABI changes require Runtime contract revision 2")));
		FAngelscriptRuntimeJITBackendId Parsed;
		ASSERT_THAT(IsTrue(FAngelscriptRuntimeJITBackendId::TryParse(
			TEXT("angelsea-mir"), Parsed)));
		ASSERT_THAT(AreEqual(FString(TEXT("angelsea-mir")),
			Parsed.ToString()));
		ASSERT_THAT(IsFalse(FAngelscriptRuntimeJITBackendId::TryParse(
			TEXT(""), Parsed)));
		ASSERT_THAT(IsFalse(FAngelscriptRuntimeJITBackendId::TryParse(
			TEXT("Angelsea-MIR"), Parsed)));
		ASSERT_THAT(IsFalse(FAngelscriptRuntimeJITBackendId::TryParse(
			TEXT("-leading"), Parsed)));
		ASSERT_THAT(IsFalse(FAngelscriptRuntimeJITBackendId::TryParse(
			TEXT("trailing-"), Parsed)));
		ASSERT_THAT(IsFalse(FAngelscriptRuntimeJITBackendId::TryParse(
			TEXT("double--dash"), Parsed)));
		static_assert(!std::is_same_v<
			FAngelscriptRuntimeJITBackendId,
			FAngelscriptStaticJITBackendId>);
	}

	TEST_METHOD(FactoryAndSnapshotAbiRejectMalformedViews)
	{
		using namespace AngelscriptRuntimeJITBackendContractTests_Private;
		FFakeFactory Factory(TEXT("fake-runtime"), TEXT("Fake Runtime"));
		ASSERT_THAT(IsTrue(FAngelscriptRuntimeJITContract::ValidateFactoryMetadata(
			Factory.Metadata).IsValid()));

		FAngelscriptRuntimeJITBackendFactoryMetadata Invalid =
			Factory.Metadata;
		Invalid.StructSize -= 1;
		ASSERT_THAT(IsTrue(FAngelscriptRuntimeJITContract::ValidateFactoryMetadata(
			Invalid).Code ==
			EAngelscriptRuntimeJITContractValidationCode::InvalidStructSize));
		Invalid = Factory.Metadata;
		Invalid.AbiRevision += 1;
		ASSERT_THAT(IsTrue(FAngelscriptRuntimeJITContract::ValidateFactoryMetadata(
			Invalid).Code ==
			EAngelscriptRuntimeJITContractValidationCode::UnsupportedRevision));
		Invalid = Factory.Metadata;
		Invalid.DisplayName = nullptr;
		ASSERT_THAT(IsFalse(FAngelscriptRuntimeJITContract::ValidateFactoryMetadata(
			Invalid).IsValid()));
		Invalid = Factory.Metadata;
		Invalid.SupportedPlatforms =
			static_cast<EAngelscriptRuntimeJITPlatform>(1u << 30);
		ASSERT_THAT(IsFalse(FAngelscriptRuntimeJITContract::ValidateFactoryMetadata(
			Invalid).IsValid()));

		FAngelscriptRuntimeJITCompileSnapshotView Snapshot = MakeSnapshot();
		ASSERT_THAT(IsTrue(FAngelscriptRuntimeJITContract::ValidateSnapshot(
			Snapshot).IsValid()));
		Snapshot.StructSize -= 1;
		ASSERT_THAT(IsFalse(FAngelscriptRuntimeJITContract::ValidateSnapshot(
			Snapshot).IsValid()));
		Snapshot = MakeSnapshot();
		Snapshot.Header.AbiRevision += 1;
		ASSERT_THAT(IsFalse(FAngelscriptRuntimeJITContract::ValidateSnapshot(
			Snapshot).IsValid()));
		Snapshot = MakeSnapshot();
		Snapshot.Bytecode = nullptr;
		ASSERT_THAT(IsTrue(FAngelscriptRuntimeJITContract::ValidateSnapshot(
			Snapshot).Code ==
			EAngelscriptRuntimeJITContractValidationCode::MissingBytecode));
		Snapshot = MakeSnapshot();
		Snapshot.Header.Platform = EAngelscriptRuntimeJITPlatform::None;
		ASSERT_THAT(IsTrue(FAngelscriptRuntimeJITContract::ValidateSnapshot(
			Snapshot).Code ==
			EAngelscriptRuntimeJITContractValidationCode::InvalidPlatform));
		Snapshot = MakeSnapshot();
		Snapshot.HelperTokenCount =
			FAngelscriptRuntimeJITBackendAbi::MaximumHelperTokenCount + 1;
		ASSERT_THAT(IsTrue(FAngelscriptRuntimeJITContract::ValidateSnapshot(
			Snapshot).Code ==
			EAngelscriptRuntimeJITContractValidationCode::InvalidHelperTokenCount));

		FAngelscriptRuntimeJITCodeLeaseView InvalidLease;
		InvalidLease.Release = &ReleaseLease;
		ASSERT_THAT(IsTrue(FAngelscriptRuntimeJITContract::ValidateCodeLease(
			InvalidLease).Code ==
			EAngelscriptRuntimeJITContractValidationCode::InvalidCodeLease));
		InvalidLease.UserData = &Factory.CreateCount;
		InvalidLease.AbiRevision += 1;
		ASSERT_THAT(IsTrue(FAngelscriptRuntimeJITContract::ValidateCodeLease(
			InvalidLease).Code ==
			EAngelscriptRuntimeJITContractValidationCode::UnsupportedRevision));

		FAngelscriptRuntimeJITFrameValue FrameValue;
		FrameValue.StructSize = 0;
		Snapshot = MakeSnapshot();
		Snapshot.FrameValues = &FrameValue;
		Snapshot.FrameValueCount = 1;
		FAngelscriptRuntimeJITContractValidationResult ElementResult =
			FAngelscriptRuntimeJITContract::ValidateSnapshot(Snapshot);
		ASSERT_THAT(IsTrue(ElementResult.Code ==
			EAngelscriptRuntimeJITContractValidationCode::InvalidStructSize));
		ASSERT_THAT(AreEqual(0, ElementResult.ElementIndex));

		FAngelscriptRuntimeJITControlFlowEdge Edge;
		Edge.StructSize = 0;
		Snapshot = MakeSnapshot();
		Snapshot.ControlFlowEdges = &Edge;
		Snapshot.ControlFlowEdgeCount = 1;
		ElementResult = FAngelscriptRuntimeJITContract::ValidateSnapshot(Snapshot);
		ASSERT_THAT(IsTrue(ElementResult.Code ==
			EAngelscriptRuntimeJITContractValidationCode::InvalidStructSize));
		ASSERT_THAT(AreEqual(0, ElementResult.ElementIndex));

		FAngelscriptRuntimeJITHelperToken Token;
		Token.StructSize = 0;
		Snapshot = MakeSnapshot();
		Snapshot.HelperTokens = &Token;
		Snapshot.HelperTokenCount = 1;
		ElementResult = FAngelscriptRuntimeJITContract::ValidateSnapshot(Snapshot);
		ASSERT_THAT(IsTrue(ElementResult.Code ==
			EAngelscriptRuntimeJITContractValidationCode::InvalidStructSize));
		ASSERT_THAT(AreEqual(0, ElementResult.ElementIndex));
	}

	TEST_METHOD(DiscoveryCopiesMetadataAndRejectsDuplicatesDeterministically)
	{
		using namespace AngelscriptRuntimeJITBackendContractTests_Private;
		FFakeFactory First(TEXT("fake-runtime"), TEXT("First Fake"));
		FScopedFactoryRegistration FirstRegistration(First);

		FAngelscriptRuntimeJITFactorySelection Selection =
			FAngelscriptRuntimeJITBackendRegistry::Select(
				BackendId(TEXT("fake-runtime")),
				EAngelscriptRuntimeJITPlatform::Win64,
				EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment);
		ASSERT_THAT(IsTrue(Selection.IsSelected()));
		ASSERT_THAT(AreEqual(FString(TEXT("First Fake")),
			Selection.Metadata.DisplayName));
		First.DisplayName[0] = TEXT('X');
		ASSERT_THAT(AreEqual(FString(TEXT("First Fake")),
			Selection.Metadata.DisplayName));

		FFakeFactory Second(TEXT("fake-runtime"), TEXT("Second Fake"));
		FScopedFactoryRegistration SecondRegistration(Second);
		Selection = FAngelscriptRuntimeJITBackendRegistry::Select(
			BackendId(TEXT("fake-runtime")),
			EAngelscriptRuntimeJITPlatform::Win64,
			EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment);
		ASSERT_THAT(IsTrue(Selection.Code ==
			EAngelscriptRuntimeJITFactorySelectionCode::DuplicateBackendId));
		ASSERT_THAT(IsNull(Selection.Factory));

		Second.Metadata.StructSize = 0;
		Selection = FAngelscriptRuntimeJITBackendRegistry::Select(
			BackendId(TEXT("fake-runtime")),
			EAngelscriptRuntimeJITPlatform::Win64,
			EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment);
		ASSERT_THAT(IsTrue(Selection.Code ==
			EAngelscriptRuntimeJITFactorySelectionCode::DuplicateBackendId));
		ASSERT_THAT(IsTrue(Selection.ValidationCode ==
			EAngelscriptRuntimeJITContractValidationCode::Valid));
	}

	TEST_METHOD(EngineOwnersCreateDistinctSessionsAndHonorSerialization)
	{
		using namespace AngelscriptRuntimeJITBackendContractTests_Private;
		FFakeFactory Factory(TEXT("fake-runtime"), TEXT("Fake Runtime"));
		Factory.Metadata.CompileConcurrency =
			EAngelscriptRuntimeJITCompileConcurrency::SerializedPerSession;
		FScopedFactoryRegistration Registration(Factory);

		FAngelscriptRuntimeJITBackendSessionConfig Config;
		Config.BackendId = BackendId(TEXT("fake-runtime"));
		Config.Platform = EAngelscriptRuntimeJITPlatform::Win64;
		Config.Configuration =
			EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment;
		Config.EngineNamespace = 101;
		FAngelscriptRuntimeJITSessionCreateResult First =
			FAngelscriptRuntimeJITBackendSessionOwner::Create(Config);
		Config.EngineNamespace = 202;
		FAngelscriptRuntimeJITSessionCreateResult Second =
			FAngelscriptRuntimeJITBackendSessionOwner::Create(Config);
		ASSERT_THAT(IsTrue(First.IsCreated()));
		ASSERT_THAT(IsTrue(Second.IsCreated()));
		ASSERT_THAT(IsTrue(First.Owner.Get() != Second.Owner.Get()));
		ASSERT_THAT(AreEqual(2, Factory.CreateCount));
		ASSERT_THAT(IsTrue(First.Owner->RequiresSerializedCompilation()));

		First.Owner.Reset();
		ASSERT_THAT(AreEqual(1, Factory.DestroyedCount));
		ASSERT_THAT(IsNotNull(Second.Owner.Get()));
		Second.Owner.Reset();
		ASSERT_THAT(AreEqual(2, Factory.DestroyedCount));
	}

	TEST_METHOD(FakeSessionReportsEveryTypedOutcomeAndInvalidInputSkipsBackend)
	{
		using namespace AngelscriptRuntimeJITBackendContractTests_Private;
		const EAngelscriptRuntimeJITCompileOutcome Outcomes[] = {
			EAngelscriptRuntimeJITCompileOutcome::Unsupported,
			EAngelscriptRuntimeJITCompileOutcome::Cancelled,
			EAngelscriptRuntimeJITCompileOutcome::Stale,
			EAngelscriptRuntimeJITCompileOutcome::BackendFailure,
			EAngelscriptRuntimeJITCompileOutcome::InvalidInput,
		};
		for (const EAngelscriptRuntimeJITCompileOutcome Outcome : Outcomes)
		{
			FFakeFactory Factory(TEXT("fake-runtime"), TEXT("Fake Runtime"));
			Factory.NextOutcome = Outcome;
			switch (Outcome)
			{
			case EAngelscriptRuntimeJITCompileOutcome::Unsupported:
				Factory.NextReason =
					EAngelscriptRuntimeJITDiagnosticReason::UnsupportedOpcode;
				Factory.NextBytecodeOffset = 1;
				break;
			case EAngelscriptRuntimeJITCompileOutcome::Cancelled:
				Factory.NextReason =
					EAngelscriptRuntimeJITDiagnosticReason::Cancelled;
				break;
			case EAngelscriptRuntimeJITCompileOutcome::Stale:
				Factory.NextReason =
					EAngelscriptRuntimeJITDiagnosticReason::StaleRevision;
				break;
			case EAngelscriptRuntimeJITCompileOutcome::BackendFailure:
				Factory.NextReason =
					EAngelscriptRuntimeJITDiagnosticReason::BackendFailure;
				break;
			case EAngelscriptRuntimeJITCompileOutcome::InvalidInput:
				Factory.NextReason =
					EAngelscriptRuntimeJITDiagnosticReason::MalformedSnapshot;
				break;
			default:
				checkNoEntry();
			}
			FScopedFactoryRegistration Registration(Factory);
			FAngelscriptRuntimeJITBackendSessionConfig Config;
			Config.BackendId = BackendId(TEXT("fake-runtime"));
			Config.Platform = EAngelscriptRuntimeJITPlatform::Win64;
			Config.Configuration =
				EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment;
			Config.EngineNamespace = 71;
			FAngelscriptRuntimeJITSessionCreateResult Created =
				FAngelscriptRuntimeJITBackendSessionOwner::Create(Config);
			ASSERT_THAT(IsTrue(Created.IsCreated()));
			FAngelscriptRuntimeJITCompileSnapshotView Snapshot = MakeSnapshot();
			Snapshot.Header.EngineNamespace = Config.EngineNamespace;
			const FAngelscriptRuntimeJITCompileResult Result =
				Created.Owner->Compile(Snapshot);
			ASSERT_THAT(IsTrue(Result.Outcome == Outcome));
			ASSERT_THAT(IsNull(Result.VMEntry));
		}

		FFakeFactory Factory(TEXT("fake-runtime"), TEXT("Fake Runtime"));
		FScopedFactoryRegistration Registration(Factory);
		FAngelscriptRuntimeJITBackendSessionConfig Config;
		Config.BackendId = BackendId(TEXT("fake-runtime"));
		Config.Platform = EAngelscriptRuntimeJITPlatform::Win64;
		Config.Configuration =
			EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment;
		Config.EngineNamespace = 81;
		FAngelscriptRuntimeJITSessionCreateResult Created =
			FAngelscriptRuntimeJITBackendSessionOwner::Create(Config);
		FAngelscriptRuntimeJITCompileSnapshotView Invalid = MakeSnapshot();
		Invalid.Header.EngineNamespace = Config.EngineNamespace;
		Invalid.Bytecode = nullptr;
		const FAngelscriptRuntimeJITCompileResult Result =
			Created.Owner->Compile(Invalid);
		ASSERT_THAT(IsTrue(Result.Outcome ==
			EAngelscriptRuntimeJITCompileOutcome::InvalidInput));
		ASSERT_THAT(AreEqual(0, Factory.LastSession->CompileCount));

		FAngelscriptRuntimeJITCompileSnapshotView WrongTarget = MakeSnapshot();
		WrongTarget.Header.EngineNamespace = Config.EngineNamespace;
		WrongTarget.Header.Platform = EAngelscriptRuntimeJITPlatform::Linux;
		const FAngelscriptRuntimeJITCompileResult WrongTargetResult =
			Created.Owner->Compile(WrongTarget);
		ASSERT_THAT(IsTrue(WrongTargetResult.Outcome ==
			EAngelscriptRuntimeJITCompileOutcome::InvalidInput));
		ASSERT_THAT(AreEqual(0, Factory.LastSession->CompileCount));
		WrongTarget = MakeSnapshot();
		WrongTarget.Header.EngineNamespace = Config.EngineNamespace;
		WrongTarget.Header.Configuration =
			EAngelscriptRuntimeJITBuildConfiguration::Shipping;
		Created.Owner->Compile(WrongTarget);
		ASSERT_THAT(AreEqual(0, Factory.LastSession->CompileCount));
	}

	TEST_METHOD(MalformedTypedResultsAreRejectedBeforePublication)
	{
		using namespace AngelscriptRuntimeJITBackendContractTests_Private;
		auto RunMalformed = [](
			const EAngelscriptRuntimeJITCompileOutcome Outcome,
			const EAngelscriptRuntimeJITDiagnosticReason Reason,
			const uint32 BytecodeOffset)
		{
			FFakeFactory Factory(TEXT("fake-runtime"), TEXT("Fake Runtime"));
			Factory.NextOutcome = Outcome;
			Factory.NextReason = Reason;
			Factory.NextBytecodeOffset = BytecodeOffset;
			FScopedFactoryRegistration Registration(Factory);
			FAngelscriptRuntimeJITBackendSessionConfig Config;
			Config.BackendId = BackendId(TEXT("fake-runtime"));
			Config.Platform = EAngelscriptRuntimeJITPlatform::Win64;
			Config.Configuration =
				EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment;
			Config.EngineNamespace = 91;
			FAngelscriptRuntimeJITSessionCreateResult Created =
				FAngelscriptRuntimeJITBackendSessionOwner::Create(Config);
			FAngelscriptRuntimeJITCompileSnapshotView Snapshot = MakeSnapshot();
			Snapshot.Header.EngineNamespace = Config.EngineNamespace;
			return Created.Owner->Compile(Snapshot);
		};

		FAngelscriptRuntimeJITCompileResult Result = RunMalformed(
			EAngelscriptRuntimeJITCompileOutcome::Cancelled,
			EAngelscriptRuntimeJITDiagnosticReason::BackendFailure,
			MAX_uint32);
		ASSERT_THAT(IsTrue(Result.Outcome ==
			EAngelscriptRuntimeJITCompileOutcome::InvalidInput));
		ASSERT_THAT(IsTrue(Result.Reason ==
			EAngelscriptRuntimeJITDiagnosticReason::InvalidBackendResult));

		Result = RunMalformed(
			EAngelscriptRuntimeJITCompileOutcome::Unsupported,
			static_cast<EAngelscriptRuntimeJITDiagnosticReason>(MAX_uint16),
			MAX_uint32);
		ASSERT_THAT(IsTrue(Result.Reason ==
			EAngelscriptRuntimeJITDiagnosticReason::InvalidBackendResult));

		Result = RunMalformed(
			EAngelscriptRuntimeJITCompileOutcome::Unsupported,
			EAngelscriptRuntimeJITDiagnosticReason::UnsupportedOpcode,
			3);
		ASSERT_THAT(IsTrue(Result.Reason ==
			EAngelscriptRuntimeJITDiagnosticReason::InvalidBackendResult));

		Result = RunMalformed(
			EAngelscriptRuntimeJITCompileOutcome::BackendFailure,
			EAngelscriptRuntimeJITDiagnosticReason::BackendFailure,
			0);
		ASSERT_THAT(IsTrue(Result.Reason ==
			EAngelscriptRuntimeJITDiagnosticReason::InvalidBackendResult));
	}

	TEST_METHOD(CompiledResultsCancelAndConcurrencyFollowFactoryCapability)
	{
		using namespace AngelscriptRuntimeJITBackendContractTests_Private;
		FFakeFactory Factory(TEXT("fake-runtime"), TEXT("Fake Runtime"));
		Factory.NextOutcome = EAngelscriptRuntimeJITCompileOutcome::Compiled;
		Factory.NextReason = EAngelscriptRuntimeJITDiagnosticReason::None;
		Factory.CompileDelaySeconds = 0.05f;
		FScopedFactoryRegistration Registration(Factory);

		auto CreateOwner = [&Factory](
			const uint64 EngineNamespace,
			const EAngelscriptRuntimeJITCompileConcurrency Concurrency)
		{
			Factory.Metadata.CompileConcurrency = Concurrency;
			FAngelscriptRuntimeJITBackendSessionConfig Config;
			Config.BackendId = BackendId(TEXT("fake-runtime"));
			Config.Platform = EAngelscriptRuntimeJITPlatform::Win64;
			Config.Configuration =
				EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment;
			Config.EngineNamespace = EngineNamespace;
			return FAngelscriptRuntimeJITBackendSessionOwner::Create(Config);
		};

		FAngelscriptRuntimeJITSessionCreateResult Serialized = CreateOwner(
			303,
			EAngelscriptRuntimeJITCompileConcurrency::SerializedPerSession);
		ASSERT_THAT(IsTrue(Serialized.IsCreated()));
		FAngelscriptRuntimeJITCompileSnapshotView SerializedSnapshot =
			MakeSnapshot();
		SerializedSnapshot.Header.EngineNamespace = 303;
		FFakeSession* const SerializedSession = Factory.LastSession;
		FAngelscriptRuntimeJITCompileResult SerializedResultA;
		FAngelscriptRuntimeJITCompileResult SerializedResultB;
		{
			TFuture<FAngelscriptRuntimeJITCompileResult> SerializedFirst = Async(
				EAsyncExecution::ThreadPool,
				[&Serialized, SerializedSnapshot]()
				{
					return Serialized.Owner->Compile(SerializedSnapshot);
				});
			TFuture<FAngelscriptRuntimeJITCompileResult> SerializedSecond = Async(
				EAsyncExecution::ThreadPool,
				[&Serialized, SerializedSnapshot]()
				{
					return Serialized.Owner->Compile(SerializedSnapshot);
				});
			SerializedResultA = SerializedFirst.Get();
			SerializedResultB = SerializedSecond.Get();
		}
		ASSERT_THAT(IsTrue(SerializedResultA.Outcome ==
			EAngelscriptRuntimeJITCompileOutcome::Compiled));
		ASSERT_THAT(IsTrue(SerializedResultB.Outcome ==
			EAngelscriptRuntimeJITCompileOutcome::Compiled));
		ASSERT_THAT(AreEqual(1, SerializedSession->MaxActiveCompileCount));
		Serialized.Owner->Cancel(19);
		ASSERT_THAT(AreEqual(static_cast<uint64>(19),
			SerializedSession->LastCancellationGeneration));
		SerializedResultA.CodeLease.Reset();
		SerializedResultB.CodeLease.Reset();
		ASSERT_THAT(AreEqual(2, Factory.LeaseReleaseCount));
		Serialized.Owner.Reset();

		FAngelscriptRuntimeJITSessionCreateResult Concurrent = CreateOwner(
			404,
			EAngelscriptRuntimeJITCompileConcurrency::ConcurrentPerSession);
		ASSERT_THAT(IsTrue(Concurrent.IsCreated()));
		ASSERT_THAT(IsFalse(Concurrent.Owner->RequiresSerializedCompilation()));
		FAngelscriptRuntimeJITCompileSnapshotView ConcurrentSnapshot =
			MakeSnapshot();
		ConcurrentSnapshot.Header.EngineNamespace = 404;
		FFakeSession* const ConcurrentSession = Factory.LastSession;
		FAngelscriptRuntimeJITCompileResult ConcurrentResultA;
		FAngelscriptRuntimeJITCompileResult ConcurrentResultB;
		{
			TFuture<FAngelscriptRuntimeJITCompileResult> ConcurrentFirst = Async(
				EAsyncExecution::ThreadPool,
				[&Concurrent, ConcurrentSnapshot]()
				{
					return Concurrent.Owner->Compile(ConcurrentSnapshot);
				});
			TFuture<FAngelscriptRuntimeJITCompileResult> ConcurrentSecond = Async(
				EAsyncExecution::ThreadPool,
				[&Concurrent, ConcurrentSnapshot]()
				{
					return Concurrent.Owner->Compile(ConcurrentSnapshot);
				});
			ConcurrentResultA = ConcurrentFirst.Get();
			ConcurrentResultB = ConcurrentSecond.Get();
		}
		ASSERT_THAT(IsTrue(ConcurrentSession->MaxActiveCompileCount >= 2));
		ConcurrentResultA.CodeLease.Reset();
		ConcurrentResultB.CodeLease.Reset();
		ASSERT_THAT(AreEqual(4, Factory.LeaseReleaseCount));
	}

	TEST_METHOD(SerializedBlockingCompileCanBeCancelledWithoutDeadlock)
	{
		using namespace AngelscriptRuntimeJITBackendContractTests_Private;
		FFakeFactory Factory(TEXT("fake-runtime"), TEXT("Fake Runtime"));
		Factory.Metadata.CompileConcurrency =
			EAngelscriptRuntimeJITCompileConcurrency::SerializedPerSession;
		Factory.bBlockUntilCancelled = true;
		FScopedFactoryRegistration Registration(Factory);
		FAngelscriptRuntimeJITBackendSessionConfig Config;
		Config.BackendId = BackendId(TEXT("fake-runtime"));
		Config.Platform = EAngelscriptRuntimeJITPlatform::Win64;
		Config.Configuration =
			EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment;
		Config.EngineNamespace = 505;
		FAngelscriptRuntimeJITSessionCreateResult Created =
			FAngelscriptRuntimeJITBackendSessionOwner::Create(Config);
		ASSERT_THAT(IsTrue(Created.IsCreated()));
		FAngelscriptRuntimeJITCompileSnapshotView Snapshot = MakeSnapshot();
		Snapshot.Header.EngineNamespace = Config.EngineNamespace;
		FFakeSession* const Session = Factory.LastSession;
		TFuture<FAngelscriptRuntimeJITCompileResult> CompileFuture = Async(
			EAsyncExecution::ThreadPool,
			[&Created, Snapshot]()
			{
				return Created.Owner->Compile(Snapshot);
			});
		const double WaitDeadline = FPlatformTime::Seconds() + 1.0;
		while (Session->CompileEntered.GetValue() == 0
			&& FPlatformTime::Seconds() < WaitDeadline)
		{
			FPlatformProcess::Sleep(0.001f);
		}
		ASSERT_THAT(AreEqual(1, Session->CompileEntered.GetValue()));
		const double CancelStart = FPlatformTime::Seconds();
		Created.Owner->Cancel(23);
		const double CancelDuration = FPlatformTime::Seconds() - CancelStart;
		ASSERT_THAT(IsTrue(CancelDuration < 0.5));
		const FAngelscriptRuntimeJITCompileResult Result = CompileFuture.Get();
		ASSERT_THAT(IsTrue(Result.Outcome ==
			EAngelscriptRuntimeJITCompileOutcome::Unsupported));
		ASSERT_THAT(AreEqual(static_cast<uint64>(23),
			Session->LastCancellationGeneration));
	}

	TEST_METHOD(CodeLeasesReleaseExactlyOnceAcrossPublicationAndReaders)
	{
		using namespace AngelscriptRuntimeJITBackendContractTests_Private;
		int32 ReleaseCount = 0;
		FAngelscriptRuntimeJITCodeLeaseView View;
		View.UserData = &ReleaseCount;
		View.Release = &ReleaseLease;
		{
			FAngelscriptRuntimeJITCodeLease Unpublished =
				FAngelscriptRuntimeJITCodeLease::Adopt(View);
			ASSERT_THAT(IsTrue(Unpublished.IsValid()));
		}
		ASSERT_THAT(AreEqual(1, ReleaseCount));

		{
			FAngelscriptRuntimeJITCodeLease Published =
				FAngelscriptRuntimeJITCodeLease::Adopt(View);
			FAngelscriptRuntimeJITCodeLease ActiveReader = Published;
			FAngelscriptRuntimeJITCodeLease Replacement =
				FAngelscriptRuntimeJITCodeLease::Adopt(View);
			Published.Reset();
			ASSERT_THAT(AreEqual(1, ReleaseCount));
			Replacement.Reset();
			ASSERT_THAT(AreEqual(2, ReleaseCount));
			ActiveReader.Reset();
			ASSERT_THAT(AreEqual(3, ReleaseCount));
		}
		ASSERT_THAT(AreEqual(3, ReleaseCount));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
