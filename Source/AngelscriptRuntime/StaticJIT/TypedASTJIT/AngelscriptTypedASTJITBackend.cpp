#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITBackend.h"

#include "AngelscriptEngine.h"
#include "StaticJIT/AngelscriptStaticJITGenerator.h"
#if WITH_ANGELSCRIPT_UNITTESTS
#include "StaticJIT/AngelscriptStaticJITBytecodeAccessAudit.h"
#endif
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallClosure.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITDependencies.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEmitter.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEligibility.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITProviderEmitter.h"

#include "Misc/ScopeLock.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if AS_CAN_GENERATE_JIT

namespace AngelscriptTypedASTJITBackend_Private
{
#if WITH_ANGELSCRIPT_UNITTESTS
	FCriticalSection GTaskObservationMutex;
	FAngelscriptTypedASTJITTaskObservation GTaskObservation;

	void RecordCreatedInstance()
	{
		FScopeLock Lock(&GTaskObservationMutex);
		++GTaskObservation.CreatedInstanceCount;
	}

	void RecordDestroyedInstance()
	{
		FScopeLock Lock(&GTaskObservationMutex);
		++GTaskObservation.DestroyedInstanceCount;
	}

	void BeginTaskObservation(
		const FAngelscriptStaticJITBackendRequest& Request)
	{
		FScopeLock Lock(&GTaskObservationMutex);
		++GTaskObservation.GenerateInvocationCount;
		GTaskObservation.LastModuleCount =
			Request.CompiledSourceGraph.Modules.Num();
		GTaskObservation.LastFunctionCount =
			Request.CompiledSourceGraph.Functions.Num();
		GTaskObservation.LastVerifiedTypedHIRCount = 0;
		GTaskObservation.LastUFunctionRootCount = 0;
		GTaskObservation.LastCallClosurePlanCount = 0;
		GTaskObservation.LastEligibleCallClosurePlanCount = 0;
		GTaskObservation.LastPlannedCallCount = 0;
		GTaskObservation.LastDirectScriptCallCount = 0;
		GTaskObservation.LastInternalSemanticHelperCallCount = 0;
		GTaskObservation.LastBridgeCallCount = 0;
		GTaskObservation.LastUnsupportedCallCount = 0;
		GTaskObservation.bLastTaskUsedOnlySameCompilationHIR = false;
		GTaskObservation.LastFailure.Reset();
	}

	void RecordRejectedTask(const FString& Failure)
	{
		FScopeLock Lock(&GTaskObservationMutex);
		++GTaskObservation.RejectedTaskCount;
		GTaskObservation.LastFailure = Failure;
	}

	void RecordAcceptedTask(
		const int32 VerifiedTypedHIRCount,
		const int32 UFunctionRootCount,
		const TConstArrayView<FAngelscriptTypedASTJITCallClosurePlan>
			CallClosurePlans)
	{
		FScopeLock Lock(&GTaskObservationMutex);
		++GTaskObservation.AcceptedTaskCount;
		GTaskObservation.LastVerifiedTypedHIRCount = VerifiedTypedHIRCount;
		GTaskObservation.LastUFunctionRootCount = UFunctionRootCount;
		GTaskObservation.LastCallClosurePlanCount =
			CallClosurePlans.Num();
		for (const FAngelscriptTypedASTJITCallClosurePlan& Plan
			: CallClosurePlans)
		{
			GTaskObservation.LastEligibleCallClosurePlanCount +=
				Plan.bEligible ? 1 : 0;
			GTaskObservation.LastPlannedCallCount += Plan.Calls.Num();
			for (const FAngelscriptTypedASTJITCallClosureCall& Call
				: Plan.Calls)
			{
				switch (Call.Disposition)
				{
				case EAngelscriptTypedASTJITCallDisposition::DirectScript:
					++GTaskObservation.LastDirectScriptCallCount;
					break;
				case EAngelscriptTypedASTJITCallDisposition::InternalSemanticHelper:
					++GTaskObservation.LastInternalSemanticHelperCallCount;
					break;
				case EAngelscriptTypedASTJITCallDisposition::Bridge:
					++GTaskObservation.LastBridgeCallCount;
					break;
				case EAngelscriptTypedASTJITCallDisposition::Unsupported:
					++GTaskObservation.LastUnsupportedCallCount;
					break;
				default:
					break;
				}
			}
		}
		GTaskObservation.bLastTaskUsedOnlySameCompilationHIR = true;
		GTaskObservation.LastFailure.Reset();
	}

	void RecordStructuredDiagnostics(
		const TConstArrayView<FAngelscriptStaticJITFunctionResult> Functions)
	{
		FScopeLock Lock(&GTaskObservationMutex);
		for (const FAngelscriptStaticJITFunctionResult& Function : Functions)
		{
			if (!Function.TypedDiagnostic.IsSet())
			{
				continue;
			}
			const FAngelscriptStaticJITTypedFunctionDiagnostic& Diagnostic =
				Function.TypedDiagnostic.GetValue();
			GTaskObservation.StructuredFunctionDiagnosticCount +=
				Diagnostic.SemanticFunction.IsSet() ? 1 : 0;
			if (!Diagnostic.CallClosure.IsSet())
			{
				continue;
			}
			++GTaskObservation.StructuredRootClosureDiagnosticCount;
			const FAngelscriptStaticJITTypedCallClosureDiagnostic& Closure =
				Diagnostic.CallClosure.GetValue();
			GTaskObservation.StructuredExceptionPayloadDiagnosticCount +=
				Closure.ExceptionPayloadState
					== EAngelscriptStaticJITExceptionPayloadState::
						DirectFailureRecord
				? 1 : 0;
			for (const FAngelscriptStaticJITTypedSemanticFunctionDiagnostic&
				SemanticFunction : Closure.Functions)
			{
				GTaskObservation.StructuredHelperDiagnosticCount +=
					SemanticFunction.Role
						== EAngelscriptStaticJITTypedClosureFunctionRole::
							InternalSemanticHelper
					? 1 : 0;
				GTaskObservation.StructuredVerifiedHIRDiagnosticCount +=
					SemanticFunction.HIRState
						== EAngelscriptStaticJITTypedHIRState::Verified
						? 1 : 0;
			}
			GTaskObservation.StructuredCallDiagnosticCount +=
				Closure.Calls.Num();
			for (const FAngelscriptStaticJITTypedCallDiagnostic& Call
				: Closure.Calls)
			{
				GTaskObservation.StructuredEmittedCallDiagnosticCount +=
					!Call.EmittedCppCallee.IsEmpty() ? 1 : 0;
				GTaskObservation.StructuredBridgeCallDiagnosticCount +=
					Call.EmittedRoute
						== EAngelscriptJITNativeCallSiteRoute::Bridge
					? 1 : 0;
			}
		}
	}
#endif

	class FFactory final : public IAngelscriptStaticJITBackendFactory
	{
	public:
		FAngelscriptStaticJITBackendId GetBackendId() const override
		{
			return FAngelscriptStaticJITBackendId::TypedAST();
		}

		TUniquePtr<IAngelscriptStaticJITBackend> CreateBackend() const override
		{
			return MakeUnique<FAngelscriptTypedASTJIT>();
		}
	};

	static FFactory GFactory;

	bool IsModuleSelected(
		const FAngelscriptStaticJITBackendRequest& Request,
		const FAngelscriptStableModuleKey& ModuleKey)
	{
		return Request.EmitModuleSet.IsEmpty()
			|| Request.EmitModuleSet.ContainsByPredicate(
				[&ModuleKey](const FAngelscriptStableModuleKey& Candidate)
				{
					return Candidate == ModuleKey;
				});
	}

	FAngelscriptStaticJITTypedFunctionDiagnostic BuildTypedDiagnostic(
		const EAngelscriptTypedASTJITFallbackReason Reason,
		const asSTypedSemanticSourceSpan& SourceSpan,
		const FString& Detail,
		const FAngelscriptStaticJITCompiledSourceGraph& Graph);

	FString ResolveStableDiagnosticSourceSection(
		const asSTypedSemanticSourceSpan& SourceSpan,
		const FAngelscriptStaticJITCompiledSourceGraph& Graph)
	{
		if (SourceSpan.section.GetLength() == 0)
		{
			return FString();
		}

		const FString Section = UTF8_TO_TCHAR(SourceSpan.section.AddressOf());
		for (const FAngelscriptModuleDesc* const ModuleDesc
			: Graph.ResolvedModuleDescriptors)
		{
			if (ModuleDesc == nullptr)
			{
				continue;
			}

			for (const FAngelscriptModuleDesc::FCodeSection& CodeSection
				: ModuleDesc->Code)
			{
				const bool bMatches =
					Section.Equals(CodeSection.AbsoluteFilename,
						ESearchCase::CaseSensitive)
					|| Section.Equals(CodeSection.VirtualPath,
						ESearchCase::CaseSensitive)
					|| Section.Equals(CodeSection.RelativeFilename,
						ESearchCase::CaseSensitive)
					|| (!CodeSection.AbsoluteFilename.IsEmpty()
						&& Section.Equals(CodeSection.AbsoluteFilename,
							ESearchCase::IgnoreCase));
				if (!bMatches)
				{
					continue;
				}

				if (!CodeSection.VirtualPath.IsEmpty())
				{
					return CodeSection.VirtualPath;
				}
				if (!CodeSection.RelativeFilename.IsEmpty())
				{
					return CodeSection.RelativeFilename;
				}
				return Section;
			}
		}
		return Section;
	}

	FString BuildStableEligibilityDiagnosticString(
		const FAngelscriptTypedASTJITEligibility& Eligibility,
		const FAngelscriptStaticJITCompiledSourceGraph& Graph)
	{
		FString Detail = Eligibility.ToDiagnosticString();
		if (Eligibility.SourceSpan.section.GetLength() == 0)
		{
			return Detail;
		}

		const FString OriginalSection = UTF8_TO_TCHAR(
			Eligibility.SourceSpan.section.AddressOf());
		const FString StableSection = ResolveStableDiagnosticSourceSection(
			Eligibility.SourceSpan, Graph);
		if (!StableSection.IsEmpty() && StableSection != OriginalSection)
		{
			Detail.ReplaceInline(
				*OriginalSection, *StableSection, ESearchCase::CaseSensitive);
		}
		return Detail;
	}

	FAngelscriptStaticJITProcessedSource BuildProcessedSource(
		const asSTypedSemanticSourceSpan& SourceSpan,
		const FAngelscriptStaticJITCompiledSourceGraph& Graph)
	{
		FAngelscriptStaticJITProcessedSource Source;
		Source.Section = ResolveStableDiagnosticSourceSection(
			SourceSpan, Graph);
		Source.Row = SourceSpan.row;
		Source.Column = SourceSpan.column;
		return Source;
	}

	FAngelscriptStaticJITProcessedSource BuildSourceAnchor(
		const asSTypedSemanticSourceAnchor& SourceAnchor,
		const FAngelscriptStaticJITCompiledSourceGraph& Graph)
	{
		asSTypedSemanticSourceSpan Span;
		Span.section = SourceAnchor.section;
		Span.sourceOffset = SourceAnchor.sourceOffset;
		Span.sourceLength = SourceAnchor.sourceLength;
		Span.row = SourceAnchor.row;
		Span.column = SourceAnchor.column;
		return BuildProcessedSource(Span, Graph);
	}

	void PopulateDiagnosticSourceProvenance(
		const asSTypedSemanticSourceSpan& SourceSpan,
		const FAngelscriptStaticJITCompiledSourceGraph& Graph,
		FAngelscriptStaticJITTypedCallDiagnostic& OutDiagnostic)
	{
		OutDiagnostic.ProcessedSource = BuildProcessedSource(SourceSpan, Graph);
		if (SourceSpan.hasAuthoredOrigin)
		{
			OutDiagnostic.AuthoredSource = BuildSourceAnchor(
				SourceSpan.authoredOrigin, Graph);
		}
		if (SourceSpan.hasGeneratedOrigin
			&& SourceSpan.generatedOrigin.hasAuthoredAnchor)
		{
			OutDiagnostic.GeneratedSource = BuildSourceAnchor(
				SourceSpan.generatedOrigin.authoredAnchor, Graph);
		}
	}

	FAngelscriptStaticJITTypedFunctionDiagnostic BuildTypedDiagnostic(
		const EAngelscriptTypedASTJITFallbackReason Reason,
		const asSTypedSemanticSourceSpan& SourceSpan,
		const FString& Detail,
		const FAngelscriptStaticJITCompiledSourceGraph& Graph)
	{
		FAngelscriptStaticJITTypedFunctionDiagnostic Diagnostic;
		Diagnostic.FallbackReason = Reason;
		Diagnostic.Detail = Detail;
		Diagnostic.ProcessedSource = BuildProcessedSource(SourceSpan, Graph);
		return Diagnostic;
	}

	FAngelscriptStaticJITTypedFunctionDiagnostic BuildTypedDiagnostic(
		const FAngelscriptTypedASTJITEligibility& Eligibility,
		const FAngelscriptStaticJITCompiledSourceGraph& Graph)
	{
		return BuildTypedDiagnostic(
			Eligibility.Reason,
			Eligibility.SourceSpan,
			Eligibility.Detail,
			Graph);
	}

	EAngelscriptStaticJITTypedClosureFunctionRole ConvertClosureRole(
		const EAngelscriptTypedASTJITClosureFunctionDisposition Disposition)
	{
		switch (Disposition)
		{
		case EAngelscriptTypedASTJITClosureFunctionDisposition::Root:
			return EAngelscriptStaticJITTypedClosureFunctionRole::
				UFunctionRoot;
		case EAngelscriptTypedASTJITClosureFunctionDisposition::DirectScript:
			return EAngelscriptStaticJITTypedClosureFunctionRole::DirectScript;
		case EAngelscriptTypedASTJITClosureFunctionDisposition::
			InternalSemanticHelper:
			return EAngelscriptStaticJITTypedClosureFunctionRole::
				InternalSemanticHelper;
		case EAngelscriptTypedASTJITClosureFunctionDisposition::Bridge:
			return EAngelscriptStaticJITTypedClosureFunctionRole::Bridge;
		case EAngelscriptTypedASTJITClosureFunctionDisposition::Unsupported:
		default:
			return EAngelscriptStaticJITTypedClosureFunctionRole::Unsupported;
		}
	}

	EAngelscriptStaticJITTypedExecutionProfileKind ClassifyProfile(
		const EAngelscriptTypedASTJITDirectCapability Capabilities)
	{
		using ECapability = EAngelscriptTypedASTJITDirectCapability;
		constexpr ECapability InstrumentedCapabilities =
			ECapability::LineCallback
			| ECapability::DebuggerStep
			| ECapability::DebuggerLocals
			| ECapability::Coverage
			| ECapability::LoopTimeout
			| ECapability::AbortSuspend;
		if (EnumHasAnyFlags(Capabilities, InstrumentedCapabilities))
		{
			return EAngelscriptStaticJITTypedExecutionProfileKind::Instrumented;
		}
		if (EnumHasAnyFlags(Capabilities, ECapability::FramePosition))
		{
			return EAngelscriptStaticJITTypedExecutionProfileKind::PositionOnly;
		}
		return EAngelscriptStaticJITTypedExecutionProfileKind::Unavailable;
	}

	EAngelscriptStaticJITTypedCallDisposition ConvertCallDisposition(
		const EAngelscriptTypedASTJITCallDisposition Disposition)
	{
		switch (Disposition)
		{
		case EAngelscriptTypedASTJITCallDisposition::DirectScript:
			return EAngelscriptStaticJITTypedCallDisposition::DirectScript;
		case EAngelscriptTypedASTJITCallDisposition::InternalSemanticHelper:
			return EAngelscriptStaticJITTypedCallDisposition::
				InternalSemanticHelper;
		case EAngelscriptTypedASTJITCallDisposition::DirectExported:
			return EAngelscriptStaticJITTypedCallDisposition::DirectExported;
		case EAngelscriptTypedASTJITCallDisposition::DirectInline:
			return EAngelscriptStaticJITTypedCallDisposition::DirectInline;
		case EAngelscriptTypedASTJITCallDisposition::RuntimeThunk:
			return EAngelscriptStaticJITTypedCallDisposition::RuntimeThunk;
		case EAngelscriptTypedASTJITCallDisposition::CurrentNativeBinding:
			return EAngelscriptStaticJITTypedCallDisposition::
				CurrentNativeBinding;
		case EAngelscriptTypedASTJITCallDisposition::Bridge:
			return EAngelscriptStaticJITTypedCallDisposition::Bridge;
		case EAngelscriptTypedASTJITCallDisposition::Unsupported:
		default:
			return EAngelscriptStaticJITTypedCallDisposition::Unsupported;
		}
	}

	FAngelscriptStaticJITTypedCallDiagnostic BuildCallDiagnostic(
		const FAngelscriptTypedASTJITCallClosureCall& Call,
		const FAngelscriptStaticJITCompiledSourceGraph& Graph)
	{
		FAngelscriptStaticJITTypedCallDiagnostic Diagnostic;
		Diagnostic.CallerFunctionKey = Call.CallerFunctionKey;
		Diagnostic.CalleeFunctionKey = Call.CalleeFunctionKey;
		Diagnostic.ExpressionIndex = Call.Expression.value;
		PopulateDiagnosticSourceProvenance(
			Call.SourceSpan, Graph, Diagnostic);
		Diagnostic.TargetKind = Call.TargetKind;
		Diagnostic.Disposition = ConvertCallDisposition(Call.Disposition);
		Diagnostic.CalleeEligibilityReason = Call.CalleeEligibility.Reason;
		Diagnostic.CalleeEligibilityDetail = Call.CalleeEligibility.Detail;
		Diagnostic.CalleeEligibilitySource = BuildProcessedSource(
			Call.CalleeEligibility.SourceSpan, Graph);
		Diagnostic.bHasNativeLinkageValidation = Call.TargetKind
			!= asETypedSemanticCallTargetKind::ScriptFunction;
		Diagnostic.NativeLinkageValidation = Call.ExternalCallValidation.Code;
		Diagnostic.NativeLinkageValidationDetail =
			Call.ExternalCallValidation.Detail;

		if (const FAngelscriptStaticJITCompiledFunctionView* const Callee =
			Graph.Functions.FindByPredicate(
				[&Call](const auto& Candidate)
				{
					return Candidate.FunctionKey == Call.CalleeFunctionKey;
				}))
		{
			Diagnostic.CanonicalDeclaration =
				FString(Callee->CanonicalDeclaration);
		}

		if (const FAngelscriptStaticJITNativeCallTarget* const Target =
			Graph.NativeCallTargets.FindByPredicate(
				[&Call](const auto& Candidate)
				{
					return Candidate.EngineLocalFunctionId
						== Call.EngineLocalTargetFunctionId;
				}))
		{
			if (!Target->CanonicalDeclaration.IsEmpty())
			{
				Diagnostic.CanonicalDeclaration =
					Target->CanonicalDeclaration;
			}
			Diagnostic.RegisteredTarget =
				Target->RegisteredTargetDisplay.IsEmpty()
					? Call.ExternalCallDescriptor.Symbol
					: Target->RegisteredTargetDisplay;
			Diagnostic.StableTargetKey = Target->Reference.StableKey;
			Diagnostic.ExpectedAbi = Target->Reference.ExpectedAbi;
		}
		return Diagnostic;
	}

	FAngelscriptStaticJITTypedSemanticFunctionDiagnostic
	BuildSemanticFunctionDiagnostic(
		const FAngelscriptTypedASTJITCallClosureFunctionInput& Input,
		const EAngelscriptStaticJITTypedClosureFunctionRole Role,
		const FAngelscriptTypedASTJITEligibility& Eligibility,
		const FAngelscriptStaticJITCompiledSourceGraph& Graph,
		const int32 ComponentIndex = INDEX_NONE,
		const bool bRecursiveComponent = false)
	{
		FAngelscriptStaticJITTypedSemanticFunctionDiagnostic Diagnostic;
		Diagnostic.FunctionKey = Input.FunctionKey;
		Diagnostic.Role = Role;
		Diagnostic.RawTraitBits = static_cast<uint32>(
			Input.Eligibility.RawTraitBits);
		Diagnostic.UnknownTraitBits = static_cast<uint32>(
			ClassifyTypedSemanticFunctionTraits(
				Input.Eligibility.RawTraitBits).
				unknownTraitBits);
		Diagnostic.InvocationKind = Input.Eligibility.Shape.InvocationKind;
		Diagnostic.ReceiverKind = Input.Eligibility.Shape.ReceiverKind;
		Diagnostic.ReceiverParameterIndex =
			Input.Eligibility.Shape.ReceiverParameterIndex;
		Diagnostic.ComponentIndex = ComponentIndex;
		Diagnostic.bRecursiveComponent = bRecursiveComponent;
		Diagnostic.EligibilityReason = Eligibility.Reason;
		Diagnostic.EligibilityDetail = Eligibility.Detail;
		Diagnostic.EligibilitySource = BuildProcessedSource(
			Eligibility.SourceSpan, Graph);
		if (Input.Eligibility.TypedHIR != nullptr)
		{
			Diagnostic.HIRState =
				EAngelscriptStaticJITTypedHIRState::Verified;
			Diagnostic.CleanupPlanState =
				Input.Eligibility.TypedHIR->header.cleanupPlanState;
			Diagnostic.bHasSuspendState =
				Input.Eligibility.TypedHIR->header.hasSuspendState;
			Diagnostic.bHasExceptionCleanup =
				Input.Eligibility.TypedHIR->header.hasExceptionCleanup;
			Diagnostic.bCleanupPlanCoversAllTransfers =
				Input.Eligibility.TypedHIR->header.
					cleanupPlanCoversAllTransfers;
		}
		return Diagnostic;
	}

	const FAngelscriptTypedASTJITCallClosureFunctionInput*
	FindClosureInput(
		const TConstArrayView<FAngelscriptTypedASTJITCallClosureFunctionInput>
			Inputs,
		const FAngelscriptStableFunctionKey& FunctionKey)
	{
		return Inputs.FindByPredicate(
			[&FunctionKey](
				const FAngelscriptTypedASTJITCallClosureFunctionInput& Candidate)
			{
				return Candidate.FunctionKey == FunctionKey;
			});
	}

	FAngelscriptStaticJITTypedFunctionDiagnostic BuildTypedDiagnostic(
		const FAngelscriptTypedASTJITEligibility& Eligibility,
		const FAngelscriptTypedASTJITCallClosureFunctionInput& FunctionInput,
		const FAngelscriptTypedASTJITCallClosurePlan* Plan,
		const TConstArrayView<FAngelscriptTypedASTJITCallClosureFunctionInput>
			AllInputs,
		const FAngelscriptStaticJITCompiledSourceGraph& Graph)
	{
		FAngelscriptStaticJITTypedFunctionDiagnostic Diagnostic =
			BuildTypedDiagnostic(Eligibility, Graph);
		Diagnostic.SemanticFunction = BuildSemanticFunctionDiagnostic(
			FunctionInput,
			FunctionInput.bIsUFunctionRoot
				? EAngelscriptStaticJITTypedClosureFunctionRole::UFunctionRoot
				: EAngelscriptStaticJITTypedClosureFunctionRole::NonRoot,
			Eligibility,
			Graph);
		if (Plan == nullptr)
		{
			return Diagnostic;
		}

		Diagnostic.CallClosure.Emplace();
		FAngelscriptStaticJITTypedCallClosureDiagnostic& Closure =
			Diagnostic.CallClosure.GetValue();
		Closure.bEligible = Plan->bEligible;
		Closure.RequiredCapabilities = Plan->RequiredDirectCapabilities;
		Closure.AvailableCapabilities = Plan->AvailableDirectCapabilities;
		Closure.ExecutionRequirementsIdentityHash =
			Plan->ExecutionRequirementsIdentityHash;
		Closure.AvailableCapabilityProfileIdentityHash =
			Plan->AvailableCapabilityProfileIdentityHash;
		Closure.ProfileKind = ClassifyProfile(
			Plan->AvailableDirectCapabilities);
		Closure.bRecursionGuardAvailable = EnumHasAnyFlags(
			Plan->AvailableDirectCapabilities,
			EAngelscriptTypedASTJITDirectCapability::RecursionBudget);
		Closure.Functions.Reserve(Plan->Functions.Num());
		for (const FAngelscriptTypedASTJITCallClosureFunction& PlannedFunction
			: Plan->Functions)
		{
			const FAngelscriptTypedASTJITCallClosureFunctionInput* const Input =
				FindClosureInput(AllInputs, PlannedFunction.FunctionKey);
			if (!ensureMsgf(Input != nullptr,
				TEXT("Every planned TypedAST closure function must retain its generation input.")))
			{
				continue;
			}
			Closure.Functions.Add(BuildSemanticFunctionDiagnostic(
				*Input,
				ConvertClosureRole(PlannedFunction.Disposition),
				PlannedFunction.Eligibility,
				Graph,
				PlannedFunction.ComponentIndex,
				PlannedFunction.bRecursiveComponent));
		}
		Closure.Calls.Reserve(Plan->Calls.Num());
		for (const FAngelscriptTypedASTJITCallClosureCall& Call : Plan->Calls)
		{
			Closure.Calls.Add(BuildCallDiagnostic(Call, Graph));
		}
		return Diagnostic;
	}

	void UpdateTypedFallback(
		FAngelscriptStaticJITTypedFunctionDiagnostic& Diagnostic,
		const EAngelscriptTypedASTJITFallbackReason Reason,
		const asSTypedSemanticSourceSpan& SourceSpan,
		const FString& Detail,
		const FAngelscriptStaticJITCompiledSourceGraph& Graph)
	{
		Diagnostic.FallbackReason = Reason;
		Diagnostic.Detail = Detail;
		Diagnostic.ProcessedSource = BuildProcessedSource(SourceSpan, Graph);
	}

	bool ResolveSourceMetadata(
		const FAngelscriptStaticJITCompiledSourceGraph& Graph,
		const FAngelscriptStaticJITCompiledFunctionView& Function,
		FAngelscriptJITGenerationModule& OutModule,
		FString& OutSourceSection,
		uint32& OutSourceLine,
		uint32& OutSourceColumn,
		FString& OutError)
	{
		asIScriptModule* const ScriptModule =
			Function.EngineLocalFunction->GetModule();
		const int32 ModuleIndex = Graph.Modules.IndexOfByKey(ScriptModule);
		if (ModuleIndex == INDEX_NONE
			|| !Graph.ResolvedModuleDescriptors.IsValidIndex(ModuleIndex))
		{
			OutError = TEXT(
				"TypedASTJITSourceMetadataMissing: function module has no exact generation descriptor.");
			return false;
		}
		const FAngelscriptModuleDesc* const ModuleDesc =
			Graph.ResolvedModuleDescriptors[ModuleIndex];
		if (ModuleDesc == nullptr || ModuleDesc->ModuleName.IsEmpty()
			|| ModuleDesc->Code.Num() != 1
			|| ModuleDesc->Code[0].VirtualPath.IsEmpty())
		{
			OutError = TEXT(
				"TypedASTJITSourceMetadataUnsupported: an emitted module must resolve to exactly one virtual AngelScript source.");
			return false;
		}

		const char* const ScriptSectionAnsi =
			Function.EngineLocalFunction->GetScriptSectionName();
		if (ScriptSectionAnsi == nullptr)
		{
			OutError = TEXT(
				"TypedASTJITSourceMetadataMissing: emitted function has no script section.");
			return false;
		}
		const FString ScriptSection = ANSI_TO_TCHAR(ScriptSectionAnsi);
		const FAngelscriptModuleDesc::FCodeSection& CodeSection =
			ModuleDesc->Code[0];
		if (!ScriptSection.Equals(CodeSection.AbsoluteFilename,
					ESearchCase::CaseSensitive)
			&& !ScriptSection.Equals(CodeSection.VirtualPath,
					ESearchCase::CaseSensitive)
			&& !ScriptSection.Equals(CodeSection.RelativeFilename,
					ESearchCase::CaseSensitive)
			&& (CodeSection.AbsoluteFilename.IsEmpty()
				|| !ScriptSection.Equals(CodeSection.AbsoluteFilename,
					ESearchCase::IgnoreCase)))
		{
			OutError = FString::Printf(TEXT(
				"TypedASTJITSourceMetadataMismatch: section '%s' does not match the module's authoritative source."),
				*ScriptSection);
			return false;
		}

		const asCScriptFunction* const ScriptFunction =
			static_cast<const asCScriptFunction*>(
				Function.EngineLocalFunction);
		int32 PackedSourceLocation = ScriptFunction->scriptData != nullptr
			? ScriptFunction->scriptData->declaredAt : 0;
		if ((PackedSourceLocation & 0xFFFFF) == 0)
		{
			PackedSourceLocation = const_cast<asCScriptFunction*>(
				ScriptFunction)->GetLineNumber(0, nullptr);
		}

		OutModule.ModuleKey = Function.ModuleKey;
		OutModule.CanonicalModuleName = ModuleDesc->ModuleName;
		OutModule.VirtualSourcePath = CodeSection.VirtualPath;
		OutSourceSection = CodeSection.VirtualPath;
		OutSourceLine = static_cast<uint32>(
			FMath::Max(0, PackedSourceLocation & 0xFFFFF));
		OutSourceColumn = static_cast<uint32>(
			FMath::Max(0, PackedSourceLocation >> 20));
		OutError.Reset();
		return true;
	}

	bool IsDirectClosureFunction(
		const EAngelscriptTypedASTJITClosureFunctionDisposition Disposition)
	{
		return Disposition
				== EAngelscriptTypedASTJITClosureFunctionDisposition::Root
			|| Disposition
				== EAngelscriptTypedASTJITClosureFunctionDisposition::DirectScript
			|| Disposition
				== EAngelscriptTypedASTJITClosureFunctionDisposition::
					InternalSemanticHelper;
	}

	bool ContainsLoopBackedge(
		const asCTypedSemanticFunction& Function)
	{
		for (asUINT StatementIndex = 0;
			StatementIndex < Function.statements.GetLength();
			++StatementIndex)
		{
			if (Function.statements[StatementIndex].safePointRole
				== asETypedSemanticSafePointRole::LoopBackedge)
			{
				return true;
			}
		}
		return false;
	}

	const FAngelscriptStaticJITCompiledFunctionView* FindCompiledFunction(
		const FAngelscriptStaticJITCompiledSourceGraph& Graph,
		const FAngelscriptStableFunctionKey& FunctionKey)
	{
		return Graph.Functions.FindByPredicate(
			[&FunctionKey](const auto& Candidate)
			{
				return Candidate.FunctionKey == FunctionKey;
			});
	}

	FString SanitizeHelperStem(const asIScriptFunction& Function)
	{
		const FString Name = UTF8_TO_TCHAR(Function.GetName());
		FString Result;
		Result.Reserve(Name.Len() + 1);
		for (const TCHAR Character : Name)
		{
			const bool bAsciiIdentifier = Character == TEXT('_')
				|| (Character >= TEXT('a') && Character <= TEXT('z'))
				|| (Character >= TEXT('A') && Character <= TEXT('Z'))
				|| (Character >= TEXT('0') && Character <= TEXT('9'));
			Result.AppendChar(bAsciiIdentifier ? Character : TEXT('_'));
		}
		if (Result.IsEmpty())
		{
			Result = TEXT("Function");
		}
		if (Result[0] >= TEXT('0') && Result[0] <= TEXT('9'))
		{
			Result.InsertAt(0, TEXT('_'));
		}
		return Result;
	}

	FString BuildInternalHelperSymbol(
		const TCHAR* Prefix,
		const FAngelscriptStableFunctionKey& RootFunctionKey,
		const FAngelscriptStaticJITCompiledFunctionView& Function)
	{
		const FString FunctionHash = Function.FunctionKey.Hash.ToHexString();
		const FString RootHash = RootFunctionKey.Hash.ToHexString();
		return FString::Printf(TEXT("%s_%s_%s_%s"),
			Prefix,
			*SanitizeHelperStem(*Function.EngineLocalFunction),
			*FunctionHash.Left(16),
			*RootHash.Left(16));
	}

	bool FindDirectScriptReference(
		const FAngelscriptStaticJITCompiledSourceGraph& Graph,
		const FAngelscriptTypedASTJITCallClosureCall& Call,
		FAngelscriptArtifactReference& OutReference,
		FString& OutError)
	{
		OutReference = {};
		for (const FAngelscriptStaticJITDependencyView& Dependency
			: Graph.Dependencies)
		{
			if (Dependency.OwnerFunctionKey != Call.CallerFunctionKey
				|| Dependency.Reference.Kind
					!= EAngelscriptArtifactReferenceKind::ScriptFunction
				|| Dependency.Reference.StableKey
					!= Call.CalleeFunctionKey.Hash)
			{
				continue;
			}
			if (!Dependency.Reference.IsValid()
				|| Dependency.Reference.ExpectedAbi.IsZero())
			{
				OutError = TEXT(
					"TypedASTJITDirectHelperReferenceInvalid: the frozen ScriptFunction dependency lacks an exact key or declaration ABI.");
				return false;
			}
			if (OutReference.IsValid()
				&& OutReference != Dependency.Reference)
			{
				OutError = TEXT(
					"TypedASTJITDirectHelperReferenceAmbiguous: one helper key resolved to conflicting declaration ABIs.");
				return false;
			}
			OutReference = Dependency.Reference;
		}
		if (!OutReference.IsValid())
		{
			OutError = FString::Printf(TEXT(
				"TypedASTJITDirectHelperReferenceMissing: Caller=%s Callee=%s."),
				*Call.CallerFunctionKey.Hash.ToHexString(),
				*Call.CalleeFunctionKey.Hash.ToHexString());
			return false;
		}
		OutError.Reset();
		return true;
	}

	void BuildFunctionShape(
		const FAngelscriptStaticJITCompiledFunctionView& Function,
		const FString& SymbolName,
		FAngelscriptTypedASTJITFunctionShape& OutShape)
	{
		OutShape = {};
		const asCScriptFunction* const ScriptFunction =
			static_cast<const asCScriptFunction*>(
				Function.EngineLocalFunction);
		OutShape.SymbolName = SymbolName;
		OutShape.ReturnType = ScriptFunction->returnType;
		OutShape.InvocationKind =
			Function.VerifiedTypedHIR->header.invocationKind;
		OutShape.ReceiverKind =
			Function.VerifiedTypedHIR->header.receiver.kind;
		OutShape.ReceiverParameterIndex =
			Function.VerifiedTypedHIR->header.receiver.parameterIndex;
		OutShape.bHasNativeObjectSlot = ScriptFunction->objectType != nullptr;
		OutShape.bReturnsOnStack =
			Function.VerifiedTypedHIR->header.returnsOnStack;
		for (asUINT ParameterIndex = 0;
			ParameterIndex < ScriptFunction->parameterTypes.GetLength();
			++ParameterIndex)
		{
			OutShape.ParameterTypes.Add(
				ScriptFunction->parameterTypes[ParameterIndex]);
		}
	}

	struct FDirectClosureMemberEmission
	{
		const FAngelscriptTypedASTJITCallClosureFunction* Plan = nullptr;
		const FAngelscriptStaticJITCompiledFunctionView* Function = nullptr;
		FString BodySymbol;
		FString WrapperSymbol;
		FString SourceSection;
		uint32 SourceLine = 0;
		uint32 SourceColumn = 0;
		bool bNeedsWrapper = false;
		TArray<FAngelscriptTypedASTJITNativeCallEmissionPlan> NativeCalls;
		TArray<FAngelscriptTypedASTJITScriptCallEmissionPlan> ScriptCalls;
		TArray<FAngelscriptTypedASTJITGlobalDependency> GlobalDependencies;
		FAngelscriptTypedASTJITDependencyAnalysis DependencyAnalysis;
		FAngelscriptTypedASTJITEmission Body;
		FAngelscriptTypedASTJITProviderEntryPlan EntryPlan;
		FAngelscriptTypedASTJITInternalHelperEmission Wrapper;
	};

	FAngelscriptStaticJITTypedCallDiagnostic* FindCallDiagnostic(
		FAngelscriptStaticJITTypedCallClosureDiagnostic& Closure,
		const FAngelscriptStableFunctionKey& CallerFunctionKey,
		const asTypedSemanticExpressionId Expression)
	{
		return Closure.Calls.FindByPredicate(
			[&](const FAngelscriptStaticJITTypedCallDiagnostic& Candidate)
			{
				return Candidate.CallerFunctionKey == CallerFunctionKey
					&& Candidate.ExpressionIndex == Expression.value;
			});
	}

	void RecordEmittedCallDiagnostics(
		FAngelscriptStaticJITTypedCallClosureDiagnostic& Closure,
		const TConstArrayView<FDirectClosureMemberEmission> Members)
	{
		for (const FDirectClosureMemberEmission& Member : Members)
		{
			for (const FAngelscriptTypedASTJITScriptCallEmissionPlan& Call
				: Member.ScriptCalls)
			{
				if (FAngelscriptStaticJITTypedCallDiagnostic* const Diagnostic =
					FindCallDiagnostic(Closure, Member.Function->FunctionKey,
						Call.Expression))
				{
					Diagnostic->EmittedCppCallee = Call.Symbol;
					Diagnostic->DirectCppSymbol = Call.Symbol;
				}
			}
			for (const FAngelscriptTypedASTJITNativeCallSiteMetadata& Call
				: Member.Body.NativeCallSites)
			{
				if (FAngelscriptStaticJITTypedCallDiagnostic* const Diagnostic =
					FindCallDiagnostic(Closure, Member.Function->FunctionKey,
						Call.Expression))
				{
					Diagnostic->EmittedRoute = Call.Route;
					Diagnostic->EmittedCppCallee = Call.EmittedCppCallee;
					Diagnostic->RuntimeDllCore = Call.RuntimeDllCore;
					Diagnostic->MetadataRowSymbol = Call.MetadataRowSymbol;
					Diagnostic->RegisteredTarget = Call.RegisteredTarget;
					Diagnostic->StableTargetKey = Call.StableTargetKey;
					Diagnostic->ExpectedAbi = Call.ExpectedAbi;
					Diagnostic->ReferenceSlotIndex = Call.ReferenceSlotIndex;
					if (Call.Route
						== EAngelscriptJITNativeCallSiteRoute::DirectExported
						|| Call.Route
							== EAngelscriptJITNativeCallSiteRoute::DirectInline
						|| Call.Route
							== EAngelscriptJITNativeCallSiteRoute::RuntimeThunk)
					{
						Diagnostic->DirectCppSymbol = Call.EmittedCppCallee;
					}
				}
			}
		}
	}
}

FAngelscriptTypedASTJIT::FAngelscriptTypedASTJIT()
{
#if WITH_ANGELSCRIPT_UNITTESTS
	AngelscriptTypedASTJITBackend_Private::RecordCreatedInstance();
#endif
}

FAngelscriptTypedASTJIT::~FAngelscriptTypedASTJIT()
{
#if WITH_ANGELSCRIPT_UNITTESTS
	AngelscriptTypedASTJITBackend_Private::RecordDestroyedInstance();
#endif
}

bool FAngelscriptTypedASTJIT::Generate(
	const FAngelscriptStaticJITBackendRequest& Request,
	FAngelscriptStaticJITBackendOutput& OutOutput,
	FString* OutError)
{
#if WITH_ANGELSCRIPT_UNITTESTS
	FScopedAngelscriptTypedASTJITBytecodeIsolationAudit
		BytecodeIsolationAudit;
#endif
	OutOutput = {};
#if WITH_ANGELSCRIPT_UNITTESTS
	AngelscriptTypedASTJITBackend_Private::BeginTaskObservation(Request);
#endif
	auto Reject = [OutError](const FString& Failure)
	{
#if WITH_ANGELSCRIPT_UNITTESTS
		AngelscriptTypedASTJITBackend_Private::RecordRejectedTask(Failure);
#endif
		if (OutError != nullptr)
		{
			*OutError = Failure;
		}
		return false;
	};

	if (bTaskConsumed)
	{
		return Reject(TEXT(
			"TypedASTJITTaskInstanceAlreadyConsumed: one backend instance may execute exactly one generation task."));
	}
	bTaskConsumed = true;

	if (Request.RequestedBackendId
			!= FAngelscriptStaticJITBackendId::TypedAST()
		|| Request.CompiledSourceGraph.CaptureProfile
			!= EAngelscriptStaticJITCaptureProfile::VerifiedTypedHIR)
	{
		return Reject(TEXT(
			"TypedASTJITCaptureProfileMismatch: the backend requires the verified-typed-hir capture profile."));
	}
	if (Request.CompiledSourceGraph.Modules.IsEmpty())
	{
		return Reject(TEXT(
			"TypedASTJITEngineMismatch: the compiled graph contains no Engine-owned module."));
	}
	if (Request.CompiledSourceGraph.SourceCompilePassCount != 1u)
	{
		return Reject(TEXT(
			"TypedASTJITSourceCompilePassMismatch: same-compilation HIR requires exactly one authoritative source compile pass."));
	}

	asIScriptEngine* ScriptEngine = nullptr;
	for (asIScriptModule* Module : Request.CompiledSourceGraph.Modules)
	{
		if (Module == nullptr)
		{
			return Reject(TEXT(
				"TypedASTJITEngineMismatch: the compiled graph contains a null module."));
		}
		asIScriptEngine* const ModuleEngine = Module->GetEngine();
		if (ScriptEngine == nullptr)
		{
			ScriptEngine = ModuleEngine;
		}
		if (ModuleEngine == nullptr || ModuleEngine != ScriptEngine)
		{
			return Reject(TEXT(
				"TypedASTJITEngineMismatch: all compiled modules must belong to one generation Engine."));
		}
	}

	int32 VerifiedTypedHIRCount = 0;
	for (const FAngelscriptStaticJITCompiledFunctionView& Function
		: Request.CompiledSourceGraph.Functions)
	{
		if (Function.EngineLocalFunction == nullptr
			|| Function.EngineLocalFunction->GetEngine() != ScriptEngine)
		{
			return Reject(TEXT(
				"TypedASTJITEngineMismatch: every compiled function must belong to the task generation Engine."));
		}
		asIScriptModule* const FunctionModule =
			Function.EngineLocalFunction->GetModule();
		if (FunctionModule == nullptr
			|| !Request.CompiledSourceGraph.Modules.Contains(FunctionModule))
		{
			return Reject(TEXT(
				"TypedASTJITTaskGraphMismatch: every compiled function must belong to the exact task module graph."));
		}
		if (Function.VerifiedTypedHIR == nullptr)
		{
			continue;
		}

		const asCScriptFunction* const ScriptFunction =
			static_cast<const asCScriptFunction*>(
				Function.EngineLocalFunction);
		if (ScriptFunction->GetTypedSemanticFunction()
			!= Function.VerifiedTypedHIR)
		{
			return Reject(TEXT(
				"TypedASTJITSameCompilationHIRRequired: serialized, reconstructed, stale, or cross-Engine HIR is rejected."));
		}
		++VerifiedTypedHIRCount;
	}

	FAngelscriptTypedASTJITRootIndex RootIndex;
	FString RootIndexError;
	if (!BuildAngelscriptTypedASTJITRootIndex(
		Request.CompiledSourceGraph, RootIndex, &RootIndexError))
	{
		return Reject(RootIndexError);
	}

	TArray<FAngelscriptTypedASTJITCallClosureFunctionInput>
		CallClosureInputs;
	FString CallClosureError;
	if (!BuildAngelscriptTypedASTJITCallClosureInputs(
		Request.CompiledSourceGraph, RootIndex, CallClosureInputs,
		&CallClosureError))
	{
		return Reject(CallClosureError);
	}

	TArray<FAngelscriptTypedASTJITCallClosurePlan> CallClosurePlans;
	CallClosurePlans.Reserve(RootIndex.Roots.Num());
	for (const FAngelscriptTypedASTJITRoot& Root : RootIndex.Roots)
	{
		FAngelscriptTypedASTJITCallClosurePlan& Plan =
			CallClosurePlans.Emplace_GetRef();
		FAngelscriptTypedASTJITCallClosureOptions ClosureOptions;
		ClosureOptions.ExecutionRequirements =
			Request.CompiledSourceGraph.TypedASTJITExecutionRequirements;
		ClosureOptions.NativeCallTargets =
			Request.CompiledSourceGraph.NativeCallTargets;
		ClosureOptions.KnownNativeCallModules =
			Request.CompiledSourceGraph.Settings.KnownNativeCallModules;
		ClosureOptions.PublicNativeCallDependencyModules =
			Request.CompiledSourceGraph.Settings.
				PublicNativeCallDependencyModules;
		ClosureOptions.bNativeScalarBridgeAvailable = true;
		if (!BuildAngelscriptTypedASTJITCallClosure(
			CallClosureInputs, Root.FunctionKey, ClosureOptions, Plan,
			&CallClosureError))
		{
			return Reject(CallClosureError);
		}
	}

#if WITH_ANGELSCRIPT_UNITTESTS
	AngelscriptTypedASTJITBackend_Private::RecordAcceptedTask(
		VerifiedTypedHIRCount, RootIndex.Roots.Num(), CallClosurePlans);
#endif
	OutOutput.Functions.Reserve(
		Request.CompiledSourceGraph.Functions.Num());
	TSet<FString> RequiredIncludes;
	for (const FAngelscriptStaticJITCompiledFunctionView& Function
		: Request.CompiledSourceGraph.Functions)
	{
		FAngelscriptStaticJITFunctionResult& Result =
			OutOutput.Functions.Emplace_GetRef();
		Result.FunctionKey = Function.FunctionKey;
		Result.ActualBackendId =
			FAngelscriptStaticJITBackendId::TypedAST();
		Result.Disposition =
			EAngelscriptStaticJITFunctionDisposition::Unsupported;
		const FAngelscriptTypedASTJITCallClosureFunctionInput* const
			DiagnosticInput =
				AngelscriptTypedASTJITBackend_Private::FindClosureInput(
					CallClosureInputs, Function.FunctionKey);
		if (DiagnosticInput == nullptr)
		{
			return Reject(TEXT(
				"TypedASTJITDiagnosticInputMissing: every compiled function must retain one exact closure input."));
		}
		const FAngelscriptTypedASTJITRoot* const Root =
			RootIndex.FindByFunction(Function.EngineLocalFunction);
		if (Root == nullptr)
		{
			const FAngelscriptTypedASTJITEligibility Eligibility =
				EvaluateAngelscriptTypedASTJITEligibility(
					BuildAngelscriptTypedASTJITEligibilityInput(
						nullptr, Function));
			Result.Detail =
				AngelscriptTypedASTJITBackend_Private::
					BuildStableEligibilityDiagnosticString(
						Eligibility, Request.CompiledSourceGraph);
			Result.TypedDiagnostic =
				AngelscriptTypedASTJITBackend_Private::BuildTypedDiagnostic(
					Eligibility, *DiagnosticInput, nullptr,
					CallClosureInputs, Request.CompiledSourceGraph);
			continue;
		}

		const FAngelscriptTypedASTJITCallClosurePlan* const Plan =
			CallClosurePlans.FindByPredicate(
				[Root](
					const FAngelscriptTypedASTJITCallClosurePlan& Candidate)
				{
					return Candidate.RootFunctionKey == Root->FunctionKey;
				});
		if (Plan == nullptr)
		{
			return Reject(TEXT(
				"TypedASTJITCallGraphRootPlanMissing: every selected root must own one exact closure plan."));
		}
		if (!Plan->bEligible)
		{
			Result.Detail =
				AngelscriptTypedASTJITBackend_Private::
					BuildStableEligibilityDiagnosticString(
						Plan->RootEligibility,
						Request.CompiledSourceGraph);
			Result.TypedDiagnostic =
				AngelscriptTypedASTJITBackend_Private::BuildTypedDiagnostic(
					Plan->RootEligibility, *DiagnosticInput, Plan,
					CallClosureInputs, Request.CompiledSourceGraph);
			continue;
		}
		Result.TypedDiagnostic =
			AngelscriptTypedASTJITBackend_Private::BuildTypedDiagnostic(
				Plan->RootEligibility, *DiagnosticInput, Plan,
				CallClosureInputs, Request.CompiledSourceGraph);
		Result.Detail = FString::Printf(TEXT(
			"TypedASTJITCallClosurePassed: %s"),
			*Plan->RootEligibility.Detail);
		if (!AngelscriptTypedASTJITBackend_Private::IsModuleSelected(
			Request, Function.ModuleKey))
		{
			Result.Detail += TEXT(
				" Emission skipped because the root module is outside EmitModuleSet.");
			continue;
		}

		using namespace AngelscriptTypedASTJITBackend_Private;
		TArray<FDirectClosureMemberEmission> DirectMembers;
		FString ClosureEmissionError;
		FAngelscriptJITGenerationModule GeneratedModule;
		for (const FAngelscriptTypedASTJITCallClosureFunction& PlannedFunction
			: Plan->Functions)
		{
			if (!IsDirectClosureFunction(PlannedFunction.Disposition))
			{
				ClosureEmissionError = TEXT(
					"TypedASTJITProviderClosureUnsupported: the current closure contains a non-direct script function disposition.");
				break;
			}
			const FAngelscriptStaticJITCompiledFunctionView* const
				CompiledFunction = FindCompiledFunction(
					Request.CompiledSourceGraph,
					PlannedFunction.FunctionKey);
			if (CompiledFunction == nullptr
				|| CompiledFunction->VerifiedTypedHIR == nullptr)
			{
				ClosureEmissionError = TEXT(
					"TypedASTJITProviderClosureFunctionMissing: every direct closure member requires the exact same-compilation HIR view.");
				break;
			}

			FDirectClosureMemberEmission& Member =
				DirectMembers.Emplace_GetRef();
			Member.Plan = &PlannedFunction;
			Member.Function = CompiledFunction;
			const bool bRootMember =
				CompiledFunction->FunctionKey == Function.FunctionKey;
			Member.BodySymbol = bRootMember
				? TEXT("{SYMBOL_PREFIX}_TypedBody")
				: BuildInternalHelperSymbol(
					TEXT("ASJIT_HelperBody"),
					Function.FunctionKey,
					*CompiledFunction);
			Member.WrapperSymbol = BuildInternalHelperSymbol(
				TEXT("ASJIT_Helper"), Function.FunctionKey,
				*CompiledFunction);
			Member.bNeedsWrapper = !bRootMember;
			for (const FAngelscriptStaticJITSemanticDependencyView& Dependency
				: Request.CompiledSourceGraph.SemanticDependencies)
			{
				if (Dependency.OwnerFunctionKey
						!= CompiledFunction->FunctionKey
					|| Dependency.Reference.Kind
						!= EAngelscriptArtifactReferenceKind::ScriptGlobal)
				{
					continue;
				}
				FAngelscriptTypedASTJITGlobalDependency& Global =
					Member.GlobalDependencies.Emplace_GetRef();
				Global.EngineLocalGlobalPropertyId =
					Dependency.EngineLocalGlobalPropertyId;
				Global.Kind = Dependency.Kind;
				Global.Reference = Dependency.Reference;
				Global.ExpectedContentOrValue =
					Dependency.ExpectedContentOrValue;
			}

			FAngelscriptJITGenerationModule SourceModule;
			FString SourceError;
			if (!ResolveSourceMetadata(
					Request.CompiledSourceGraph,
					*CompiledFunction,
					SourceModule,
					Member.SourceSection,
					Member.SourceLine,
					Member.SourceColumn,
					SourceError))
			{
				ClosureEmissionError = SourceError;
				break;
			}
			if (bRootMember)
			{
				GeneratedModule = MoveTemp(SourceModule);
			}

			const FStaticJITEntryPlan* const SharedEntryPlan =
				bRootMember && Root->EntryPlan != nullptr
					? Root->EntryPlan
					: CompiledFunction->EntryPlan;
			if (SharedEntryPlan == nullptr)
			{
				ClosureEmissionError = TEXT(
					"TypedASTJITEntryPlanMissing: the compiled function has no frozen shared Entry Plan.");
				break;
			}
			FString EntryPlanError;
			if (!BuildAngelscriptTypedASTJITProviderEntryPlan(
					*SharedEntryPlan,
					Member.EntryPlan,
					&EntryPlanError))
			{
				ClosureEmissionError = EntryPlanError;
				break;
			}
			Member.EntryPlan.CanonicalDeclaration =
				FString(CompiledFunction->CanonicalDeclaration);
			Member.EntryPlan.SourceSection = Member.SourceSection;
			Member.EntryPlan.SourceLine = Member.SourceLine;
			Member.EntryPlan.InstrumentationProfile =
				Request.CompiledSourceGraph.TypedASTJITCapabilityProfile;
		}
		DirectMembers.Sort([](const auto& Left, const auto& Right)
		{
			return Left.Function->FunctionKey.Hash
				< Right.Function->FunctionKey.Hash;
		});
		auto FindMember = [&DirectMembers](
			const FAngelscriptStableFunctionKey& FunctionKey)
			-> FDirectClosureMemberEmission*
		{
			return DirectMembers.FindByPredicate(
				[&FunctionKey](const auto& Candidate)
				{
					return Candidate.Function->FunctionKey == FunctionKey;
				});
		};
		FDirectClosureMemberEmission* const RootMember =
			FindMember(Function.FunctionKey);
		if (ClosureEmissionError.IsEmpty() && RootMember == nullptr)
		{
			ClosureEmissionError = TEXT(
				"TypedASTJITProviderClosureRootMissing: the direct member set omitted its selected root.");
		}

		TArray<FAngelscriptArtifactReference> ClosureReferences;
		for (const FAngelscriptTypedASTJITCallClosureCall& Call : Plan->Calls)
		{
			if (!ClosureEmissionError.IsEmpty())
			{
				break;
			}
			if (Call.Disposition
					== EAngelscriptTypedASTJITCallDisposition::DirectScript
				|| Call.Disposition
					== EAngelscriptTypedASTJITCallDisposition::
						InternalSemanticHelper)
			{
				FAngelscriptArtifactReference Reference;
				if (!FindDirectScriptReference(
						Request.CompiledSourceGraph,
						Call,
						Reference,
						ClosureEmissionError))
				{
					break;
				}
				ClosureReferences.AddUnique(Reference);
				FDirectClosureMemberEmission* const Callee =
					FindMember(Call.CalleeFunctionKey);
				if (Callee == nullptr)
				{
					ClosureEmissionError = TEXT(
						"TypedASTJITProviderDirectHelperMissing: a planned direct call has no emitted closure member.");
					break;
				}
				Callee->bNeedsWrapper = true;
				continue;
			}
			const bool bNeedsCurrentEngineNativeReference =
				Call.Disposition
					== EAngelscriptTypedASTJITCallDisposition::Bridge
				|| Call.Disposition
					== EAngelscriptTypedASTJITCallDisposition::
						CurrentNativeBinding
				|| Call.Disposition
					== EAngelscriptTypedASTJITCallDisposition::DirectExported
				|| Call.Disposition
					== EAngelscriptTypedASTJITCallDisposition::RuntimeThunk;
			if (!bNeedsCurrentEngineNativeReference)
			{
				continue;
			}
			const FAngelscriptStaticJITNativeCallTarget* const Target =
				Request.CompiledSourceGraph.NativeCallTargets.FindByPredicate(
					[&Call](const auto& Candidate)
					{
						return Candidate.EngineLocalFunctionId
							== Call.EngineLocalTargetFunctionId;
					});
			if (Target == nullptr)
			{
				ClosureEmissionError = TEXT(
					"TypedASTJITProviderBoundTargetMissing: a planned current-Engine call lacks its frozen target row.");
				break;
			}
			const bool bScalarCurrentNativeBinding =
				Call.Disposition
					!= EAngelscriptTypedASTJITCallDisposition::Bridge
				&& Target->ExpectedScalarABI.IsValid();
			if (Call.Disposition
					!= EAngelscriptTypedASTJITCallDisposition::Bridge
				&& !bScalarCurrentNativeBinding)
			{
				continue;
			}
			if (Target->Reference.Kind
					!= EAngelscriptArtifactReferenceKind::EnvironmentSymbol
				|| !Target->Reference.IsValid()
				|| Target->Reference.ExpectedAbi.IsZero())
			{
				ClosureEmissionError = TEXT(
					"TypedASTJITProviderBoundReferenceMissing: a planned current-Engine call lacks its frozen EnvironmentSymbol reference.");
				break;
			}
			ClosureReferences.AddUnique(Target->Reference);
		}
		for (const FDirectClosureMemberEmission& Member : DirectMembers)
		{
			for (const FAngelscriptTypedASTJITGlobalDependency& Dependency
				: Member.GlobalDependencies)
			{
				if (Dependency.Kind
						== EAngelscriptArtifactSemanticDependencyKind::HardValue
					&& Dependency.Reference.IsValid())
				{
					ClosureReferences.AddUnique(Dependency.Reference);
				}
			}
		}
		ClosureReferences.Sort();

		TArray<FAngelscriptTypedASTJITNativeCallEmissionPlan> AllNativeCalls;
		TArray<FAngelscriptStableFunctionKey> NativeCallOwners;
		TArray<FAngelscriptTypedASTJITScriptCallEmissionPlan> AllScriptCalls;
		TArray<FAngelscriptStableFunctionKey> ScriptCallOwners;
		for (const FAngelscriptTypedASTJITCallClosureCall& Call : Plan->Calls)
		{
			if (!ClosureEmissionError.IsEmpty())
			{
				break;
			}
			if (FindMember(Call.CallerFunctionKey) == nullptr)
			{
				ClosureEmissionError = TEXT(
					"TypedASTJITProviderCallOwnerMissing: a planned call is not owned by a direct closure member.");
				break;
			}
			if (Call.Disposition
					== EAngelscriptTypedASTJITCallDisposition::DirectScript
				|| Call.Disposition
					== EAngelscriptTypedASTJITCallDisposition::
						InternalSemanticHelper)
			{
				const FDirectClosureMemberEmission* const Callee =
					FindMember(Call.CalleeFunctionKey);
				if (Callee == nullptr || !Callee->bNeedsWrapper)
				{
					ClosureEmissionError = TEXT(
						"TypedASTJITProviderDirectHelperWrapperMissing: a direct edge has no internal wrapper.");
					break;
				}
				FAngelscriptTypedASTJITScriptCallEmissionPlan& ScriptCall =
					AllScriptCalls.Emplace_GetRef();
				ScriptCall.Expression = Call.Expression;
				ScriptCall.EngineLocalTargetFunctionId =
					Call.EngineLocalTargetFunctionId;
				ScriptCall.Symbol = Callee->WrapperSymbol;
				if (!FindDirectScriptReference(
						Request.CompiledSourceGraph,
						Call,
						ScriptCall.TargetReference,
						ClosureEmissionError))
				{
					break;
				}
				ScriptCall.bRequiresFunctionContent =
					Call.CalleeFunctionKey != Call.CallerFunctionKey;
				ScriptCallOwners.Add(Call.CallerFunctionKey);
				continue;
			}

			const bool bDirectInline = Call.Disposition
				== EAngelscriptTypedASTJITCallDisposition::DirectInline;
			const bool bCurrentNativeCandidate =
				Call.Disposition
					== EAngelscriptTypedASTJITCallDisposition::DirectExported
				|| Call.Disposition
					== EAngelscriptTypedASTJITCallDisposition::RuntimeThunk
				|| Call.Disposition
					== EAngelscriptTypedASTJITCallDisposition::
						CurrentNativeBinding;
			const bool bBridge = Call.Disposition
				== EAngelscriptTypedASTJITCallDisposition::Bridge;
			if (!bDirectInline && !bCurrentNativeCandidate && !bBridge)
			{
				ClosureEmissionError = TEXT(
					"TypedASTJITProviderClosureUnsupported: a planned call has no reviewed C++ emission route.");
				break;
			}
			const FAngelscriptStaticJITNativeCallTarget* const Target =
				Request.CompiledSourceGraph.NativeCallTargets.FindByPredicate(
					[&Call](const auto& Candidate)
					{
						return Candidate.EngineLocalFunctionId
							== Call.EngineLocalTargetFunctionId;
					});
			if (Target == nullptr)
			{
				ClosureEmissionError = TEXT(
					"TypedASTJITProviderNativeTargetMissing: a planned native call has no frozen target row.");
				break;
			}
			const bool bCurrentNativeBinding = bCurrentNativeCandidate
				&& Target->ExpectedScalarABI.IsValid();
			const bool bDirectSymbol = bDirectInline
				|| bCurrentNativeCandidate && !bCurrentNativeBinding;
			FAngelscriptTypedASTJITNativeCallEmissionPlan& NativeCall =
				AllNativeCalls.Emplace_GetRef();
			NativeCallOwners.Add(Call.CallerFunctionKey);
			NativeCall.Expression = Call.Expression;
			NativeCall.EngineLocalTargetFunctionId =
				Call.EngineLocalTargetFunctionId;
			NativeCall.CanonicalDeclaration = Target->CanonicalDeclaration;
			NativeCall.TargetReference = Target->Reference;
			NativeCall.RegisteredTarget =
				Target->RegisteredTargetDisplay.IsEmpty()
					? Call.ExternalCallDescriptor.Symbol
					: Target->RegisteredTargetDisplay;
			if (bDirectSymbol)
			{
				NativeCall.Symbol = Call.ExternalCallDescriptor.Symbol;
				NativeCall.Include = Call.ExternalCallDescriptor.Include;
				switch (Call.Disposition)
				{
				case EAngelscriptTypedASTJITCallDisposition::DirectInline:
					NativeCall.NativeCallSiteRoute =
						EAngelscriptJITNativeCallSiteRoute::DirectInline;
					break;
				case EAngelscriptTypedASTJITCallDisposition::RuntimeThunk:
					NativeCall.NativeCallSiteRoute =
						EAngelscriptJITNativeCallSiteRoute::RuntimeThunk;
					break;
				case EAngelscriptTypedASTJITCallDisposition::DirectExported:
					NativeCall.NativeCallSiteRoute =
						EAngelscriptJITNativeCallSiteRoute::DirectExported;
					break;
				default:
					break;
				}
				continue;
			}

			const int32 ReferenceSlotIndex =
				ClosureReferences.IndexOfByKey(Target->Reference);
			if (ReferenceSlotIndex == INDEX_NONE)
			{
				ClosureEmissionError = TEXT(
					"TypedASTJITProviderBoundSlotMissing: the sorted closure reference set omitted a current-Engine target.");
				break;
			}
			NativeCall.Route = bCurrentNativeBinding
				? EAngelscriptTypedASTJITCallEmissionRoute::CurrentNativeBinding
				: EAngelscriptTypedASTJITCallEmissionRoute::Bridge;
			NativeCall.NativeCallSiteRoute = bCurrentNativeBinding
				? EAngelscriptJITNativeCallSiteRoute::CurrentNativeBinding
				: EAngelscriptJITNativeCallSiteRoute::Bridge;
			NativeCall.StableTargetKey =
				FAngelscriptStableFunctionKey{Target->Reference.StableKey};
			NativeCall.ExpectedABI = Target->Reference.ExpectedAbi;
			NativeCall.ReferenceSlotIndex = ReferenceSlotIndex;
		}
		bool bSemanticDependencyFailure = false;
		asSTypedSemanticSourceSpan SemanticDependencyFailureSpan;
		for (FDirectClosureMemberEmission& Member : DirectMembers)
		{
			if (!ClosureEmissionError.IsEmpty())
			{
				break;
			}

			TArray<FAngelscriptTypedASTJITNativeCallEmissionPlan>
				MemberNativeCalls;
			TArray<FAngelscriptTypedASTJITScriptCallEmissionPlan>
				MemberScriptCalls;
			for (int32 Index = 0; Index < AllNativeCalls.Num(); ++Index)
			{
				if (NativeCallOwners[Index] == Member.Function->FunctionKey)
				{
					MemberNativeCalls.Add(AllNativeCalls[Index]);
				}
			}
			for (int32 Index = 0; Index < AllScriptCalls.Num(); ++Index)
			{
				if (ScriptCallOwners[Index] == Member.Function->FunctionKey)
				{
					MemberScriptCalls.Add(AllScriptCalls[Index]);
				}
			}

			FAngelscriptTypedASTJITFunctionShape Shape;
			BuildFunctionShape(*Member.Function, Member.BodySymbol, Shape);
			Member.DependencyAnalysis =
				AnalyzeAngelscriptTypedASTJITDependencies(
					*Member.Function->VerifiedTypedHIR,
					Shape,
					*Member.Function,
					Request.CompiledSourceGraph,
					MemberNativeCalls,
					MemberScriptCalls,
					Member.GlobalDependencies);
			if (!Member.DependencyAnalysis.bValid)
			{
				bSemanticDependencyFailure = true;
				SemanticDependencyFailureSpan =
					Member.DependencyAnalysis.FailureSpan;
				ClosureEmissionError = FString::Printf(TEXT(
					"TypedASTJITSemanticDependencyMismatch: Function=%s Detail=%s"),
					*FString(Member.Function->CanonicalDeclaration),
					*Member.DependencyAnalysis.FailureDetail);
				break;
			}
			for (const FAngelscriptArtifactReference& Reference
				: Member.DependencyAnalysis.Manifest.RequiredReferences)
			{
				ClosureReferences.AddUnique(Reference);
			}
		}
		ClosureReferences.Sort();

		for (FAngelscriptTypedASTJITNativeCallEmissionPlan& NativeCall
			: AllNativeCalls)
		{
			if (!ClosureEmissionError.IsEmpty()
				|| NativeCall.Route
					== EAngelscriptTypedASTJITCallEmissionRoute::Direct)
			{
				continue;
			}
			const int32 ReferenceSlotIndex =
				ClosureReferences.IndexOfByKey(NativeCall.TargetReference);
			if (ReferenceSlotIndex == INDEX_NONE)
			{
				ClosureEmissionError = TEXT(
					"TypedASTJITProviderBoundSlotMissing: the reconciled semantic manifest omitted a current-Engine target.");
				break;
			}
			NativeCall.StableTargetKey =
				FAngelscriptStableFunctionKey{
					NativeCall.TargetReference.StableKey};
			NativeCall.ExpectedABI = NativeCall.TargetReference.ExpectedAbi;
			NativeCall.ReferenceSlotIndex = ReferenceSlotIndex;
		}

		FString CallSiteSymbolError;
		if (ClosureEmissionError.IsEmpty()
			&& !AssignAngelscriptTypedASTJITBridgeCallSiteSymbols(
				AllNativeCalls, Root->FunctionKey, &CallSiteSymbolError))
		{
			ClosureEmissionError = CallSiteSymbolError;
		}
		FString ClosureBridgeCallSiteRows;
		if (ClosureEmissionError.IsEmpty()
			&& !EmitAngelscriptTypedASTJITBridgeCallSiteRows(
				AllNativeCalls,
				ClosureBridgeCallSiteRows,
				&CallSiteSymbolError))
		{
			ClosureEmissionError = CallSiteSymbolError;
		}
		for (int32 Index = 0;
			ClosureEmissionError.IsEmpty() && Index < AllNativeCalls.Num();
			++Index)
		{
			FDirectClosureMemberEmission* const Owner =
				FindMember(NativeCallOwners[Index]);
			if (Owner == nullptr)
			{
				ClosureEmissionError = TEXT(
					"TypedASTJITProviderNativeOwnerMissing: assigned native call lost its closure owner.");
				break;
			}
			Owner->NativeCalls.Add(AllNativeCalls[Index]);
		}
		for (int32 Index = 0;
			ClosureEmissionError.IsEmpty() && Index < AllScriptCalls.Num();
			++Index)
		{
			FDirectClosureMemberEmission* const Owner =
				FindMember(ScriptCallOwners[Index]);
			if (Owner == nullptr)
			{
				ClosureEmissionError = TEXT(
					"TypedASTJITProviderScriptOwnerMissing: assigned helper call lost its closure owner.");
				break;
			}
			Owner->ScriptCalls.Add(AllScriptCalls[Index]);
		}

		for (FDirectClosureMemberEmission& Member : DirectMembers)
		{
			if (!ClosureEmissionError.IsEmpty())
			{
				break;
			}
			FAngelscriptTypedASTJITFunctionShape Shape;
			BuildFunctionShape(*Member.Function, Member.BodySymbol, Shape);
			FAngelscriptTypedASTJITEmitOptions EmitOptions;
			EmitOptions.NativeCalls = Member.NativeCalls;
			EmitOptions.ScriptCalls = Member.ScriptCalls;
			EmitOptions.GlobalDependencies = Member.GlobalDependencies;
			EmitOptions.InstrumentationProfile =
				&Request.CompiledSourceGraph.TypedASTJITCapabilityProfile;
			EmitOptions.bEmitBridgeCallSiteRows = false;
			Member.Body = EmitTypedASTJITFunction(
				*Member.Function->VerifiedTypedHIR,
				Shape,
				EmitOptions);
			if (!Member.Body.bSuccess)
			{
				ClosureEmissionError = FString::Printf(TEXT(
					"TypedASTJITProviderBodyUnsupported: Function=%s Detail=%s"),
					*FString(Member.Function->CanonicalDeclaration),
					*Member.Body.FailureDetail);
				break;
			}
		}

		for (FDirectClosureMemberEmission& Member : DirectMembers)
		{
			if (!ClosureEmissionError.IsEmpty() || !Member.bNeedsWrapper)
			{
				continue;
			}
			const FAngelscriptArtifactReference* ScriptReference = nullptr;
			for (const FAngelscriptArtifactReference& Reference
				: ClosureReferences)
			{
				if (Reference.Kind
						!= EAngelscriptArtifactReferenceKind::ScriptFunction
					|| Reference.StableKey
						!= Member.Function->FunctionKey.Hash)
				{
					continue;
				}
				if (ScriptReference != nullptr
					&& *ScriptReference != Reference)
				{
					ClosureEmissionError = TEXT(
						"TypedASTJITDirectHelperReferenceAmbiguous: closure callers disagree about the helper declaration ABI.");
					break;
				}
				ScriptReference = &Reference;
			}
			if (!ClosureEmissionError.IsEmpty())
			{
				break;
			}
			const int32 ReferenceSlotIndex = ScriptReference != nullptr
				? ClosureReferences.IndexOfByKey(*ScriptReference)
				: INDEX_NONE;
			if (ReferenceSlotIndex == INDEX_NONE)
			{
				ClosureEmissionError = TEXT(
					"TypedASTJITDirectHelperReferenceMissing: wrapper has no sorted ScriptFunction slot.");
				break;
			}

			FAngelscriptTypedASTJITInternalHelperPlan HelperPlan;
			HelperPlan.Symbol = Member.WrapperSymbol;
			HelperPlan.BodySymbol = Member.BodySymbol;
			HelperPlan.ReturnBodyCppType =
				Member.EntryPlan.Return.BodyCppType;
			HelperPlan.CanonicalDeclaration =
				Member.EntryPlan.CanonicalDeclaration;
			HelperPlan.SourceSection = Member.SourceSection;
			HelperPlan.SourceLine = Member.SourceLine;
			HelperPlan.InstrumentationProfile =
				Request.CompiledSourceGraph.TypedASTJITCapabilityProfile;
			HelperPlan.FunctionReferenceSlotIndex = ReferenceSlotIndex;
			HelperPlan.Parameters = Member.EntryPlan.Parameters;
			Member.Wrapper = EmitAngelscriptTypedASTJITInternalHelper(
				HelperPlan, Member.Body);
			if (!Member.Wrapper.bSuccess)
			{
				ClosureEmissionError = Member.Wrapper.FailureDetail;
				break;
			}
		}
		if (!ClosureEmissionError.IsEmpty())
		{
			Result.Detail += TEXT(" ") + ClosureEmissionError;
			UpdateTypedFallback(Result.TypedDiagnostic.GetValue(),
				bSemanticDependencyFailure
					? EAngelscriptTypedASTJITFallbackReason::
						SemanticDependencyMismatch
					: EAngelscriptTypedASTJITFallbackReason::EmitterFailure,
				bSemanticDependencyFailure
					? SemanticDependencyFailureSpan
					: Plan->RootEligibility.SourceSpan,
				ClosureEmissionError,
				Request.CompiledSourceGraph);
			continue;
		}

		const asCScriptFunction* const ScriptFunction =
			static_cast<const asCScriptFunction*>(
				Function.EngineLocalFunction);
		const FString CanonicalDeclaration(Function.CanonicalDeclaration);
		const FString SourceSection = RootMember->SourceSection;
		const uint32 SourceLine = RootMember->SourceLine;
		const uint32 SourceColumn = RootMember->SourceColumn;
		FAngelscriptTypedASTJITProviderEmission ProviderEmission =
			EmitAngelscriptTypedASTJITProviderFunction(
				RootMember->EntryPlan, RootMember->Body);
		if (!ProviderEmission.bSuccess)
		{
			Result.Detail += TEXT(" ") + ProviderEmission.FailureDetail;
			UpdateTypedFallback(Result.TypedDiagnostic.GetValue(),
				EAngelscriptTypedASTJITFallbackReason::EmitterFailure,
				Plan->RootEligibility.SourceSpan,
				ProviderEmission.FailureDetail,
				Request.CompiledSourceGraph);
			continue;
		}

		FString ClosurePreamble;
		for (const FDirectClosureMemberEmission& Member : DirectMembers)
		{
			ClosurePreamble += Member.Body.Declaration;
		}
		for (const FDirectClosureMemberEmission& Member : DirectMembers)
		{
			if (Member.bNeedsWrapper)
			{
				ClosurePreamble += Member.Wrapper.ForwardDeclaration;
			}
		}
		ClosurePreamble += TEXT("\n");
		ClosurePreamble += ClosureBridgeCallSiteRows;
		for (const FDirectClosureMemberEmission& Member : DirectMembers)
		{
			if (Member.Function->FunctionKey != Function.FunctionKey)
			{
				ClosurePreamble += Member.Body.Definition;
				ClosurePreamble += TEXT("\n");
			}
		}
		for (const FDirectClosureMemberEmission& Member : DirectMembers)
		{
			if (Member.bNeedsWrapper)
			{
				ClosurePreamble += Member.Wrapper.Implementation;
				ClosurePreamble += TEXT("\n");
			}
		}
		ProviderEmission.ImplementationTemplate = ClosurePreamble
			+ ProviderEmission.ImplementationTemplate;

		const FAngelscriptHash256 RebuiltEntryAbiHash =
			FAngelscriptJITGeneration::BuildEntryAbiHash(
				CanonicalDeclaration,
				Function.bHasVMEntry,
				Function.bHasRawEntry,
				Function.bHasParmsEntry);
		if (!(RebuiltEntryAbiHash == Function.EntryAbiHash))
		{
			return Reject(TEXT(
				"TypedASTJITEntryAbiMismatch: the frozen function entry set does not match EntryAbiHash."));
		}

		FAngelscriptJITGenerationFunction GeneratedFunction;
		GeneratedFunction.Flags |=
			EAngelscriptJITArtifactEntryFlags::HasExecutionCapabilityProfile;
		GeneratedFunction.ModuleKey = Function.ModuleKey;
		GeneratedFunction.FunctionKey = Function.FunctionKey;
		GeneratedFunction.ExecutionHash = Function.ExecutionHash;
		GeneratedFunction.DebugHash = Function.DebugHash;
		GeneratedFunction.EntryAbiHash = Function.EntryAbiHash;
		static_assert(static_cast<uint32>(
			EAngelscriptTypedASTJITDirectCapability::RecursionBudget)
			== static_cast<uint32>(
				EAngelscriptJITExecutionCapability::RecursionBudget));
		GeneratedFunction.AvailableExecutionCapabilities =
			static_cast<EAngelscriptJITExecutionCapability>(
				Request.CompiledSourceGraph.TypedASTJITCapabilityProfile.
					AvailableCapabilities);
		GeneratedFunction.ExecutionCapabilityProfileHash =
			Request.CompiledSourceGraph.TypedASTJITCapabilityProfile.IdentityHash;
		for (const FDirectClosureMemberEmission& Member : DirectMembers)
		{
			if (ContainsLoopBackedge(*Member.Function->VerifiedTypedHIR))
			{
				GeneratedFunction.RuntimeExecutionRequirementTriggers |=
					EAngelscriptJITExecutionCapability::LoopTimeout;
			}
		}
		GeneratedFunction.CanonicalDeclaration = CanonicalDeclaration;
		GeneratedFunction.SourceSection = SourceSection;
		GeneratedFunction.SourceLine = SourceLine;
		GeneratedFunction.SourceColumn = SourceColumn;
		GeneratedFunction.bHasVMEntry = Function.bHasVMEntry;
		GeneratedFunction.bHasRawEntry = Function.bHasRawEntry;
		GeneratedFunction.bHasParmsEntry = Function.bHasParmsEntry;
		const FString SymbolPrefix =
			FAngelscriptJITGeneration::BuildSymbolPrefix(
				Function.FunctionKey, Function.ExecutionHash);
		GeneratedFunction.VMEntrySymbol = SymbolPrefix + TEXT("_VM");
		GeneratedFunction.RawEntrySymbol = SymbolPrefix + TEXT("_Raw");
		GeneratedFunction.ParmsEntrySymbol = Function.bHasParmsEntry
			? SymbolPrefix + TEXT("_Parms") : FString();
		ProviderEmission.RawEntryDeclaration.ReplaceInline(
			TEXT("{RAW_SYMBOL}"),
			*GeneratedFunction.RawEntrySymbol,
			ESearchCase::CaseSensitive);
		GeneratedFunction.RawEntryDeclaration =
			MoveTemp(ProviderEmission.RawEntryDeclaration);
		for (const FDirectClosureMemberEmission& Member : DirectMembers)
		{
			for (const FAngelscriptTypedASTJITNativeCallSiteMetadata&
				BodyCallSite : Member.Body.NativeCallSites)
			{
				FAngelscriptJITGenerationNativeCallSite& GeneratedCallSite =
					GeneratedFunction.NativeCallSites.Emplace_GetRef();
				GeneratedCallSite.Route = BodyCallSite.Route;
				GeneratedCallSite.CanonicalDeclaration =
					BodyCallSite.CanonicalDeclaration;
				GeneratedCallSite.EmittedCppCallee =
					BodyCallSite.EmittedCppCallee;
				GeneratedCallSite.RuntimeDllCore =
					BodyCallSite.RuntimeDllCore;
				GeneratedCallSite.MetadataRowSymbol =
					BodyCallSite.MetadataRowSymbol;
				GeneratedCallSite.RegisteredTarget =
					BodyCallSite.RegisteredTarget;
				GeneratedCallSite.StableTargetKey =
					BodyCallSite.StableTargetKey;
				GeneratedCallSite.ExpectedAbi = BodyCallSite.ExpectedAbi;
				GeneratedCallSite.ReferenceSlotIndex =
					BodyCallSite.ReferenceSlotIndex;
			}
		}
		for (const FAngelscriptArtifactReference& Reference : ClosureReferences)
		{
			GeneratedFunction.References.Add({
				EAngelscriptJITReferenceSlotFlags::None,
				Reference});
		}
		{
			TArray<FAngelscriptArtifactSemanticDependency> SemanticRows;
			if (RootMember != nullptr)
			{
				SemanticRows.Append(
					RootMember->DependencyAnalysis.Manifest.ArtifactDependencies);
			}
			for (const FDirectClosureMemberEmission& Member : DirectMembers)
			{
				if (RootMember != nullptr && &Member == RootMember)
				{
					continue;
				}
				for (const FAngelscriptArtifactSemanticDependency& Dependency
					: Member.DependencyAnalysis.Manifest.ArtifactDependencies)
				{
					if (Dependency.Kind
							!= EAngelscriptArtifactSemanticDependencyKind::
								FunctionContent
						|| Dependency.Target.StableKey
							== Function.FunctionKey.Hash)
					{
						continue;
					}
					SemanticRows.Add(Dependency);
				}
			}
			TArray<FAngelscriptArtifactSemanticDependency> SerializableRows;
			for (FAngelscriptArtifactSemanticDependency& Dependency
				: SemanticRows)
			{
				if (Dependency.Target.Kind
						== EAngelscriptArtifactReferenceKind::ScriptFunction
					&& (Dependency.Kind
							== EAngelscriptArtifactSemanticDependencyKind::
								Signature
						|| Dependency.Kind
							== EAngelscriptArtifactSemanticDependencyKind::
								FunctionContent))
				{
					// Compiler rows carry declaration SignatureHash. Runtime
					// matching (and the 4.10b contract) compares the helper's
					// current JIT entry ABI. Rewrite before packaging so the
					// baked Provider row matches MakeRouteEntryAbi().
					for (const FAngelscriptStaticJITCompiledFunctionView&
						Candidate : Request.CompiledSourceGraph.Functions)
					{
						if (Candidate.FunctionKey.Hash
								== Dependency.Target.StableKey
							&& !Candidate.EntryAbiHash.IsZero())
						{
							Dependency.Target.ExpectedAbi =
								Candidate.EntryAbiHash;
							break;
						}
					}
				}
				if (FAngelscriptJITProviderManifest::
						SemanticDependencyRequiresExpectedContent(
							Dependency.Kind)
					&& !Dependency.ExpectedContentOrValue.IsSet())
				{
					for (const FAngelscriptStaticJITCompiledFunctionView&
						Candidate : Request.CompiledSourceGraph.Functions)
					{
						if (Candidate.FunctionKey.Hash
								== Dependency.Target.StableKey
							&& !Candidate.ExecutionHash.IsZero())
						{
							Dependency.ExpectedContentOrValue =
								Candidate.ExecutionHash;
							break;
						}
					}
				}
				if (!Dependency.Target.IsValid()
					|| Dependency.Target.ExpectedAbi.IsZero())
				{
					if (FAngelscriptJITProviderManifest::
							SemanticDependencyRequiresExpectedContent(
								Dependency.Kind))
					{
						ClosureEmissionError = FString::Printf(TEXT(
							"TypedASTJITSemanticDependencyUnserializable: Function=%s Kind=%u"),
							*FString(Function.CanonicalDeclaration),
							static_cast<uint32>(Dependency.Kind));
						break;
					}
					continue;
				}
				if (FAngelscriptJITProviderManifest::
						SemanticDependencyRequiresExpectedContent(
							Dependency.Kind)
					&& !Dependency.ExpectedContentOrValue.IsSet())
				{
					ClosureEmissionError = FString::Printf(TEXT(
						"TypedASTJITSemanticDependencyMissingContent: Function=%s"),
						*FString(Function.CanonicalDeclaration));
					break;
				}
				SerializableRows.Add(Dependency);
			}
			if (ClosureEmissionError.IsEmpty())
			{
				const FAngelscriptJITSemanticDependencyCanonicalResult
					Canonical =
						FAngelscriptJITProviderManifest::
							CanonicalizeSemanticDependencies(SerializableRows);
				if (!Canonical.IsValid())
				{
					ClosureEmissionError = FString::Printf(TEXT(
						"TypedASTJITSemanticDependencyCanonicalize: Function=%s Code=%u"),
						*FString(Function.CanonicalDeclaration),
						static_cast<uint32>(Canonical.Code));
				}
				else
				{
					GeneratedFunction.SemanticDependencies = Canonical.Rows;
				}
			}
		}
		if (!ClosureEmissionError.IsEmpty())
		{
			Result.Detail += TEXT(" ") + ClosureEmissionError;
			UpdateTypedFallback(Result.TypedDiagnostic.GetValue(),
				EAngelscriptTypedASTJITFallbackReason::
					SemanticDependencyMismatch,
				Plan->RootEligibility.SourceSpan,
				ClosureEmissionError,
				Request.CompiledSourceGraph);
			continue;
		}
		GeneratedFunction.ImplementationTemplate =
			MoveTemp(ProviderEmission.ImplementationTemplate);
		for (const FString& Include : ProviderEmission.RequiredIncludes)
		{
			RequiredIncludes.Add(Include);
		}
		for (const FDirectClosureMemberEmission& Member : DirectMembers)
		{
			for (const FString& Include
				: Member.DependencyAnalysis.Manifest.RequiredIncludes)
			{
				RequiredIncludes.Add(Include);
			}
			for (const FString& Include : Member.Body.RequiredIncludes)
			{
				RequiredIncludes.Add(Include);
			}
			for (const FString& Include : Member.Wrapper.RequiredIncludes)
			{
				RequiredIncludes.Add(Include);
			}
		}
		if (!OutOutput.Modules.ContainsByPredicate(
			[&GeneratedModule](
				const FAngelscriptJITGenerationModule& Candidate)
			{
				return Candidate.ModuleKey == GeneratedModule.ModuleKey;
			}))
		{
			OutOutput.Modules.Add(MoveTemp(GeneratedModule));
		}
		Result.Disposition =
			EAngelscriptStaticJITFunctionDisposition::Emitted;
		Result.EmittedFunction = MoveTemp(GeneratedFunction);
		if (Result.TypedDiagnostic.IsSet()
			&& Result.TypedDiagnostic->CallClosure.IsSet())
		{
			FAngelscriptStaticJITTypedCallClosureDiagnostic& DiagnosticClosure =
				Result.TypedDiagnostic->CallClosure.GetValue();
			RecordEmittedCallDiagnostics(DiagnosticClosure, DirectMembers);
			// Successful Provider emission always passes the root through the
			// shared execution state/frame and its owned first-failure record.
			DiagnosticClosure.ExceptionControlState =
				EAngelscriptStaticJITExceptionControlState::
					DirectExecutionState;
			DiagnosticClosure.ExceptionPayloadState =
				EAngelscriptStaticJITExceptionPayloadState::
					DirectFailureRecord;
		}
		Result.Detail += FString::Printf(TEXT(
			" TypedASTJITProviderEmitted: exact scalar raw/VM/Parms adapters and %d direct closure member(s) generated."),
			DirectMembers.Num());
	}
	if (!OutOutput.Modules.IsEmpty())
	{
		RequiredIncludes.Add(TEXT(
			"StaticJIT/StaticJITHeader.h"));
		TArray<FString> SortedIncludes = RequiredIncludes.Array();
		SortedIncludes.Sort([](const FString& Left, const FString& Right)
		{
			return Left.Compare(Right, ESearchCase::CaseSensitive) < 0;
		});
		OutOutput.ModuleSourcePreamble =
			TEXT("#include \"StaticJIT/StaticJITConfig.h\"\n")
			TEXT("#ifndef AS_SKIP_JITTED_CODE\n\n");
		for (const FString& Include : SortedIncludes)
		{
			OutOutput.ModuleSourcePreamble += FString::Printf(
				TEXT("#include \"%s\"\n"), *Include);
		}
		OutOutput.ModuleSourcePreamble += TEXT("\n");
		OutOutput.ModuleSourceFooter = TEXT("#endif\n");
		OutOutput.ProviderSourcePreamble =
			TEXT("#include \"StaticJIT/StaticJITHeader.h\"\n\n");
	}
	if (OutError != nullptr)
	{
		OutError->Reset();
	}
#if WITH_ANGELSCRIPT_UNITTESTS
	AngelscriptTypedASTJITBackend_Private::RecordStructuredDiagnostics(
		OutOutput.Functions);
#endif
	return true;
}

#if WITH_ANGELSCRIPT_UNITTESTS
void FAngelscriptTypedASTJIT::ResetTaskObservationForTesting()
{
	FScopeLock Lock(
		&AngelscriptTypedASTJITBackend_Private::GTaskObservationMutex);
	AngelscriptTypedASTJITBackend_Private::GTaskObservation = {};
}

FAngelscriptTypedASTJITTaskObservation
FAngelscriptTypedASTJIT::GetTaskObservationForTesting()
{
	FScopeLock Lock(
		&AngelscriptTypedASTJITBackend_Private::GTaskObservationMutex);
	return AngelscriptTypedASTJITBackend_Private::GTaskObservation;
}

FAngelscriptStaticJITTypedCallDiagnostic
FAngelscriptTypedASTJIT::BuildSourceProvenanceDiagnosticForTesting(
	const asSTypedSemanticSourceSpan& SourceSpan)
{
	FAngelscriptStaticJITTypedCallDiagnostic Diagnostic;
	FAngelscriptStaticJITCompiledSourceGraph EmptyGraph;
	AngelscriptTypedASTJITBackend_Private::PopulateDiagnosticSourceProvenance(
		SourceSpan, EmptyGraph, Diagnostic);
	return Diagnostic;
}
#endif

bool RegisterAngelscriptTypedASTJITBackend(FString* OutError)
{
	FAngelscriptStaticJITBackendRegistry& Registry =
		GetAngelscriptStaticJITBackendRegistry();
	const IAngelscriptStaticJITBackendFactory* const Existing =
		Registry.FindFactory(FAngelscriptStaticJITBackendId::TypedAST());
	if (Existing == &AngelscriptTypedASTJITBackend_Private::GFactory)
	{
		return true;
	}
	if (Existing != nullptr)
	{
		if (OutError != nullptr)
		{
			*OutError = TEXT(
				"StaticJIT backend registration rejected duplicate 'typed-ast' factory.");
		}
		return false;
	}

	const FAngelscriptStaticJITBackendRegistrationResult Result =
		Registry.RegisterFactory(
			AngelscriptTypedASTJITBackend_Private::GFactory);
	if (!Result.IsAccepted())
	{
		if (OutError != nullptr)
		{
			*OutError = TEXT(
				"StaticJIT failed to register the built-in 'typed-ast' backend.");
		}
		return false;
	}
	if (OutError != nullptr)
	{
		OutError->Reset();
	}
	return true;
}

#endif
