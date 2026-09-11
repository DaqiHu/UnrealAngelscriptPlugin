#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallClosure.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCanonical.h"

#include "StartAngelscriptHeaders.h"
#include "frontend/as_ast_context.h"
#include "as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if AS_CAN_GENERATE_JIT

namespace AngelscriptTypedASTJITCallClosure_Private
{
	static bool KeyLess(
		const FAngelscriptStableFunctionKey& Left,
		const FAngelscriptStableFunctionKey& Right)
	{
		return Left.Hash < Right.Hash;
	}

	static FString KeyString(
		const FAngelscriptStableFunctionKey& Key)
	{
		return Key.Hash.ToHexString();
	}

	static const TCHAR* LexToString(
		const EAngelscriptTypedASTJITCallDisposition Disposition)
	{
		switch (Disposition)
		{
		case EAngelscriptTypedASTJITCallDisposition::DirectScript:
			return TEXT("DirectScript");
		case EAngelscriptTypedASTJITCallDisposition::InternalSemanticHelper:
			return TEXT("InternalSemanticHelper");
		case EAngelscriptTypedASTJITCallDisposition::DirectExported:
			return TEXT("DirectExported");
		case EAngelscriptTypedASTJITCallDisposition::DirectInline:
			return TEXT("DirectInline");
		case EAngelscriptTypedASTJITCallDisposition::RuntimeThunk:
			return TEXT("RuntimeThunk");
		case EAngelscriptTypedASTJITCallDisposition::CurrentNativeBinding:
			return TEXT("CurrentNativeBinding");
		case EAngelscriptTypedASTJITCallDisposition::Bridge:
			return TEXT("Bridge");
		case EAngelscriptTypedASTJITCallDisposition::Unsupported:
			return TEXT("Unsupported");
		default:
			return TEXT("Unknown");
		}
	}

	static const TCHAR* LexToString(
		const EAngelscriptTypedASTJITCallTargetKind TargetKind)
	{
		switch (TargetKind)
		{
		case EAngelscriptTypedASTJITCallTargetKind::ScriptFunction:
			return TEXT("ScriptFunction");
		case EAngelscriptTypedASTJITCallTargetKind::SystemFunction:
			return TEXT("SystemFunction");
		case EAngelscriptTypedASTJITCallTargetKind::ImportedFunction:
			return TEXT("ImportedFunction");
		default:
			return TEXT("Unspecified");
		}
	}

	static const TCHAR* LexToString(
		const EAngelscriptTypedASTJITClosureFunctionDisposition Disposition)
	{
		switch (Disposition)
		{
		case EAngelscriptTypedASTJITClosureFunctionDisposition::Root:
			return TEXT("Root");
		case EAngelscriptTypedASTJITClosureFunctionDisposition::DirectScript:
			return TEXT("DirectScript");
		case EAngelscriptTypedASTJITClosureFunctionDisposition::InternalSemanticHelper:
			return TEXT("InternalSemanticHelper");
		case EAngelscriptTypedASTJITClosureFunctionDisposition::Bridge:
			return TEXT("Bridge");
		case EAngelscriptTypedASTJITClosureFunctionDisposition::Unsupported:
			return TEXT("Unsupported");
		default:
			return TEXT("Unknown");
		}
	}

	static asSSourceSpan CanonicalSpan(
		const asCASTContext& Context,
		const asCSourceRange& Range)
	{
		asSSourceSpan Span;
		if (!Range.begin.IsValid())
		{
			return Span;
		}
		asSSourceLineColumn LineColumn;
		if (Context.GetSourceManager().GetLineColumn(
				Range.begin, LineColumn))
		{
			Span.row = static_cast<asUINT>(LineColumn.row);
			Span.column = static_cast<asUINT>(LineColumn.column);
		}
		if (const char* const LogicalSection =
			Context.GetSourceManager().GetLogicalKey(Range.begin.fileID))
		{
			Span.section = LogicalSection;
		}
		return Span;
	}

	static const asCDecl* GetCanonicalFunctionDecl(
		const FAngelscriptTypedASTJITCallClosureFunctionInput& Function)
	{
		if (Function.Eligibility.SealedAST == nullptr
			|| !Function.Eligibility.SealedAST->IsPublishable()
			|| !Function.Eligibility.CanonicalFunctionDecl.IsValid())
		{
			return nullptr;
		}
		return Function.Eligibility.SealedAST->GetDecl(
			Function.Eligibility.CanonicalFunctionDecl);
	}

	static asSSourceSpan FunctionInputSpan(
		const FAngelscriptTypedASTJITCallClosureFunctionInput& Function)
	{
		if (const asCDecl* const Decl = GetCanonicalFunctionDecl(Function))
		{
			return CanonicalSpan(*Function.Eligibility.SealedAST, Decl->range);
		}
		return {};
	}

	static int32 FindExactCanonicalFunctionIndex(
		const TConstArrayView<
			FAngelscriptTypedASTJITCallClosureFunctionInput> Functions,
		const asCASTContext& Context,
		const asASTDeclId TargetDecl,
		bool& bOutAmbiguous)
	{
		bOutAmbiguous = false;
		int32 Match = INDEX_NONE;
		for (int32 Index = 0; Index < Functions.Num(); ++Index)
		{
			const FAngelscriptTypedASTJITCallClosureFunctionInput& Candidate =
				Functions[Index];
			if (Candidate.Eligibility.SealedAST != &Context
				|| Candidate.Eligibility.CanonicalFunctionDecl != TargetDecl
				|| GetCanonicalFunctionDecl(Candidate) == nullptr)
			{
				continue;
			}
			if (Match != INDEX_NONE)
			{
				bOutAmbiguous = true;
				return INDEX_NONE;
			}
			Match = Index;
		}
		return Match;
	}

	static FString DescribeCanonicalExpression(
		const asCASTContext& Context,
		const asCExpr& Expression)
	{
		TArray<FString> Children;
		Children.Reserve(static_cast<int32>(
			Expression.children.GetLength()));
		for (asUINT Index = 0;
			Index < Expression.children.GetLength(); ++Index)
		{
			const asASTExprId ChildId = Expression.children[Index];
			const asCExpr* const Child = Context.GetExpr(ChildId);
			Children.Add(Child != nullptr
				? FString::Printf(TEXT("%u:kind=%u:decl=%u:literal=%s"),
					ChildId.value, static_cast<uint32>(Child->kind),
					Child->resolvedDecl.value,
					UTF8_TO_TCHAR(Child->literal.AddressOf()))
				: FString::Printf(TEXT("%u:dangling"), ChildId.value));
		}
		return FString::Printf(TEXT(
			"Kind=%u DeclId=%u Receiver=%u Literal=%s Children=[%s]"),
			static_cast<uint32>(Expression.kind),
			Expression.resolvedDecl.value,
			Expression.receiver.value,
			UTF8_TO_TCHAR(Expression.literal.AddressOf()),
			*FString::Join(Children, TEXT(",")));
	}

	static void SetFailure(
		FAngelscriptTypedASTJITCallClosurePlan& Plan,
		const EAngelscriptTypedASTJITFallbackReason Reason,
		const asSSourceSpan& Span,
		const FString& Detail)
	{
		Plan.bEligible = false;
		Plan.RootEligibility = {};
		Plan.RootEligibility.Reason = Reason;
		Plan.RootEligibility.SourceSpan = Span;
		Plan.RootEligibility.Detail = Detail;
	}

	static FAngelscriptTypedASTJITEligibility EvaluateFunction(
		const FAngelscriptTypedASTJITCallClosureFunctionInput& Function,
		const bool bSelectedRoot)
	{
		FAngelscriptTypedASTJITEligibilityInput Eligibility =
			Function.Eligibility;
		Eligibility.bIsUFunctionRoot = Function.bIsUFunctionRoot;
		Eligibility.Purpose = bSelectedRoot
			? EAngelscriptTypedASTJITEligibilityPurpose::RootCallClosure
			: (Function.bIsUFunctionRoot
				? EAngelscriptTypedASTJITEligibilityPurpose::RootCallClosure
				: EAngelscriptTypedASTJITEligibilityPurpose::InternalHelperCallClosure);
		return EvaluateAngelscriptTypedASTJITEligibility(Eligibility);
	}

	static EAngelscriptTypedASTJITClosureFunctionDisposition
		ClassifyFunctionDisposition(
			const FAngelscriptTypedASTJITCallClosureFunctionInput& Function,
			const FAngelscriptTypedASTJITEligibility& Eligibility,
			const bool bSelectedRoot)
	{
		if (bSelectedRoot)
		{
			return Eligibility.bEligible
				? EAngelscriptTypedASTJITClosureFunctionDisposition::Root
				: EAngelscriptTypedASTJITClosureFunctionDisposition::Unsupported;
		}
		if (Eligibility.bEligible)
		{
			return Function.bIsUFunctionRoot
				? EAngelscriptTypedASTJITClosureFunctionDisposition::DirectScript
				: EAngelscriptTypedASTJITClosureFunctionDisposition::InternalSemanticHelper;
		}
		return Function.bScalarBridgeAvailable
			? EAngelscriptTypedASTJITClosureFunctionDisposition::Bridge
			: EAngelscriptTypedASTJITClosureFunctionDisposition::Unsupported;
	}

	static EAngelscriptTypedASTJITCallDisposition CallDispositionFor(
		const EAngelscriptTypedASTJITClosureFunctionDisposition FunctionDisposition)
	{
		switch (FunctionDisposition)
		{
		case EAngelscriptTypedASTJITClosureFunctionDisposition::Root:
		case EAngelscriptTypedASTJITClosureFunctionDisposition::DirectScript:
			return EAngelscriptTypedASTJITCallDisposition::DirectScript;
		case EAngelscriptTypedASTJITClosureFunctionDisposition::InternalSemanticHelper:
			return EAngelscriptTypedASTJITCallDisposition::InternalSemanticHelper;
		case EAngelscriptTypedASTJITClosureFunctionDisposition::Bridge:
			return EAngelscriptTypedASTJITCallDisposition::Bridge;
		default:
			return EAngelscriptTypedASTJITCallDisposition::Unsupported;
		}
	}

	static bool IsDirectDisposition(
		const EAngelscriptTypedASTJITClosureFunctionDisposition Disposition)
	{
		return Disposition
			== EAngelscriptTypedASTJITClosureFunctionDisposition::Root
			|| Disposition
				== EAngelscriptTypedASTJITClosureFunctionDisposition::DirectScript
			|| Disposition
				== EAngelscriptTypedASTJITClosureFunctionDisposition::InternalSemanticHelper;
	}

	static bool ComponentContains(
		const FAngelscriptTypedASTJITCallClosureComponent& Component,
		const FAngelscriptStableFunctionKey& Key)
	{
		return Component.Members.Contains(Key);
	}
}

const FAngelscriptTypedASTJITCallClosureFunction*
FAngelscriptTypedASTJITCallClosurePlan::FindFunction(
	const FAngelscriptStableFunctionKey& FunctionKey) const
{
	return Functions.FindByPredicate(
		[&FunctionKey](
			const FAngelscriptTypedASTJITCallClosureFunction& Function)
		{
			return Function.FunctionKey == FunctionKey;
		});
}

FString FAngelscriptTypedASTJITCallClosurePlan::DumpNormalized() const
{
	using namespace AngelscriptTypedASTJITCallClosure_Private;
	FString Output = FString::Printf(
		TEXT("root=%s eligible=%s required=%s available=%s requirementsIdentity=%s capabilityIdentity=%s reason=%s\n"),
		*KeyString(RootFunctionKey), bEligible ? TEXT("true") : TEXT("false"),
		*LexToString(RequiredDirectCapabilities),
		*LexToString(AvailableDirectCapabilities),
		*ExecutionRequirementsIdentityHash.ToHexString(),
		*AvailableCapabilityProfileIdentityHash.ToHexString(),
		LexToString(RootEligibility.Reason));
	for (const FAngelscriptTypedASTJITCallClosureFunction& Function : Functions)
	{
		Output += FString::Printf(
			TEXT("function=%s disposition=%s component=%d recursive=%s eligibility=%s\n"),
			*KeyString(Function.FunctionKey), LexToString(Function.Disposition),
			Function.ComponentIndex,
			Function.bRecursiveComponent ? TEXT("true") : TEXT("false"),
			LexToString(Function.Eligibility.Reason));
	}
	for (const FAngelscriptTypedASTJITCallClosureCall& Call : Calls)
	{
		Output += FString::Printf(
			TEXT("call=%s->%s expression=%u canonicalExpression=%u target=%s targetId=%d disposition=%s span=%s:%u:%u eligibility=%s external=%s validation=%d\n"),
			*KeyString(Call.CallerFunctionKey),
			*KeyString(Call.CalleeFunctionKey), Call.Expression.value,
			Call.Expression.value,
			LexToString(Call.TargetKind),
			Call.EngineLocalTargetFunctionId,
			LexToString(Call.Disposition),
			UTF8_TO_TCHAR(Call.SourceSpan.section.AddressOf()),
			Call.SourceSpan.row, Call.SourceSpan.column,
			LexToString(Call.CalleeEligibility.Reason),
			*Call.ExternalCallDescriptor.Symbol,
			static_cast<int32>(Call.ExternalCallValidation.Code));
	}
	for (int32 Index = 0; Index < Components.Num(); ++Index)
	{
		TArray<FString> Members;
		for (const FAngelscriptStableFunctionKey& Key
			: Components[Index].Members)
		{
			Members.Add(KeyString(Key));
		}
		Output += FString::Printf(TEXT(
			"component=%d recursive=%s members=%s\n"), Index,
			Components[Index].bRecursive ? TEXT("true") : TEXT("false"),
			*FString::Join(Members, TEXT(",")));
	}
	if (!bEligible)
	{
		Output += TEXT("failure=") + RootEligibility.ToDiagnosticString()
			+ TEXT("\n");
	}
	return Output;
}

bool BuildAngelscriptTypedASTJITCallClosureInputs(
	const FAngelscriptStaticJITCompiledSourceGraph& Graph,
	const FAngelscriptTypedASTJITRootIndex& RootIndex,
	TArray<FAngelscriptTypedASTJITCallClosureFunctionInput>& OutFunctions,
	FString* OutError)
{
	OutFunctions.Reset();
	if (OutError != nullptr)
	{
		OutError->Reset();
	}
	if (!Graph.TypedASTJITCapabilityProfile.IsValid())
	{
		if (OutError != nullptr)
		{
			*OutError = TEXT(
				"TypedASTJITCallGraphInvalidCapabilityProfile: the compiled source graph requires one frozen generation capability profile.");
		}
		return false;
	}
	for (const FAngelscriptStaticJITCompiledFunctionView& Function
		: Graph.Functions)
	{
		if (Function.EngineLocalFunction == nullptr
			|| Function.FunctionKey.Hash.IsZero())
		{
			OutFunctions.Reset();
			if (OutError != nullptr)
			{
				*OutError = TEXT(
					"TypedASTJITCallGraphInvalidFunction: every normalized function requires an exact task-local function and stable key.");
			}
			return false;
		}

		FAngelscriptTypedASTJITCallClosureFunctionInput& Added =
			OutFunctions.Emplace_GetRef();
		Added.EngineLocalFunctionId = Function.EngineLocalFunction->GetId();
		Added.FunctionKey = Function.FunctionKey;
		const FAngelscriptTypedASTJITRoot* const Root =
			RootIndex.FindByFunction(Function.EngineLocalFunction);
		Added.bIsUFunctionRoot = Root != nullptr;
		Added.Eligibility = BuildAngelscriptTypedASTJITEligibilityInput(
			Root, Function);
		for (const FAngelscriptStaticJITSemanticDependencyView& Dependency
			: Graph.SemanticDependencies)
		{
			if (Dependency.OwnerFunctionKey != Function.FunctionKey
				|| Dependency.Reference.Kind
					!= EAngelscriptArtifactReferenceKind::ScriptGlobal)
			{
				continue;
			}
			FAngelscriptTypedASTJITGlobalDependency& Global =
				Added.Eligibility.GlobalDependencies.Emplace_GetRef();
			Global.EngineLocalGlobalPropertyId =
				Dependency.EngineLocalGlobalPropertyId;
			Global.Kind = Dependency.Kind;
			Global.Reference = Dependency.Reference;
			Global.ExpectedContentOrValue =
				Dependency.ExpectedContentOrValue;
			Global.CanonicalASTDeclKey = Dependency.CanonicalASTDeclKey;
		}
		// The scalar bridge is opened by its later implementation task. Every
		// function receives the one immutable generation capability profile;
		// tests may construct heterogeneous profiles to prove closure rejection.
		Added.bScalarBridgeAvailable = false;
		Added.CapabilityProfile = Graph.TypedASTJITCapabilityProfile;
	}
	return true;
}

bool BuildAngelscriptTypedASTJITCallClosure(
	const TConstArrayView<FAngelscriptTypedASTJITCallClosureFunctionInput>
		Functions,
	const FAngelscriptStableFunctionKey& RootFunctionKey,
	const FAngelscriptTypedASTJITCallClosureOptions& Options,
	FAngelscriptTypedASTJITCallClosurePlan& OutPlan,
	FString* OutError)
{
	using namespace AngelscriptTypedASTJITCallClosure_Private;
	OutPlan = {};
	if (OutError != nullptr)
	{
		OutError->Reset();
	}
	auto FailInput = [&](const FString& Error)
	{
		OutPlan = {};
		if (OutError != nullptr)
		{
			*OutError = Error;
		}
		return false;
	};

	if (RootFunctionKey.Hash.IsZero())
	{
		return FailInput(TEXT(
			"TypedASTJITCallGraphRootMissing: root stable key is empty."));
	}
	if (!Options.ExecutionRequirements.IsValid())
	{
		return FailInput(TEXT(
			"TypedASTJITCallGraphInvalidRequirementsSnapshot: closure planning requires one frozen current execution-requirements snapshot."));
	}

	TMap<int32, int32> IndexByFunctionId;
	TMap<int32, const FAngelscriptStaticJITNativeCallTarget*>
		NativeCallTargetByFunctionId;
	TMap<FString, const FAngelscriptStaticJITNativeCallTarget*>
		NativeCallTargetByCanonicalDeclKey;
	for (const FAngelscriptStaticJITNativeCallTarget& Target
		: Options.NativeCallTargets)
	{
		const bool bDescriptorAbsent = Target.Descriptor.IsAbsent();
		const bool bDescriptorBridge = !bDescriptorAbsent
			&& EnumHasAnyFlags(
				Target.Descriptor.RouteFlags,
				EAngelscriptStaticJITNativeCallRouteFlags::ScalarBridge);
		const bool bDescriptorCurrentNative = !bDescriptorAbsent
			&& EnumHasAnyFlags(
				Target.Descriptor.RouteFlags,
				EAngelscriptStaticJITNativeCallRouteFlags::CurrentNativeCall);
		if (Target.EngineLocalFunctionId < 0
			|| Target.CanonicalTargetDeclKey.IsEmpty()
			|| !Target.ExpectedNativeABI.IsValid()
			|| (bDescriptorAbsent
				&& (!Target.bScalarBridgeCallable
					|| !Target.ExpectedVMBridgeABI.IsValid()))
			|| (!bDescriptorAbsent
				&& Target.bScalarBridgeCallable != bDescriptorBridge)
			|| Target.bCurrentNativeCallable != bDescriptorCurrentNative
			|| (Target.bCurrentNativeCallable
				&& (Target.Reference.Kind
						!= EAngelscriptArtifactReferenceKind::EnvironmentSymbol
					|| !Target.Reference.IsValid()
					|| Target.Reference.ExpectedAbi.IsZero()))
			|| (Target.bScalarBridgeCallable
				&& !Target.ExpectedVMBridgeABI.IsValid())
			|| (!bDescriptorAbsent
				&& Target.Descriptor.ABIDomain
					== EAngelscriptStaticJITNativeCallABIDomain::Scalar
				&& !Target.ExpectedScalarABI.IsValid())
			|| (!bDescriptorAbsent
				&& Target.Descriptor.ABIDomain
					== EAngelscriptStaticJITNativeCallABIDomain::BorrowedManagedPOD
				&& Target.ExpectedScalarABI.IsValid()))
		{
			return FailInput(TEXT(
				"TypedASTJITCallGraphInvalidNativeCallTarget: target ID, exact Canonical target key, registered native ABI, and lowering-domain ABI must be complete."));
		}
		if (NativeCallTargetByFunctionId.Contains(
			Target.EngineLocalFunctionId))
		{
			return FailInput(FString::Printf(TEXT(
				"TypedASTJITCallGraphDuplicateNativeCallTarget: FunctionId=%d."),
				Target.EngineLocalFunctionId));
		}
		if (NativeCallTargetByCanonicalDeclKey.Contains(
			Target.CanonicalTargetDeclKey))
		{
			return FailInput(FString::Printf(TEXT(
				"TypedASTJITCallGraphDuplicateNativeCanonicalTarget: CanonicalTargetDeclKey=%s."),
				*Target.CanonicalTargetDeclKey));
		}
		NativeCallTargetByFunctionId.Add(
			Target.EngineLocalFunctionId, &Target);
		NativeCallTargetByCanonicalDeclKey.Add(
			Target.CanonicalTargetDeclKey, &Target);
	}
	TSet<FString> SeenStableKeys;
	int32 RootIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Functions.Num(); ++Index)
	{
		const FAngelscriptTypedASTJITCallClosureFunctionInput& Function =
			Functions[Index];
		if (Function.EngineLocalFunctionId < 0)
		{
			return FailInput(TEXT(
				"TypedASTJITCallGraphInvalidFunctionId: function coordinates must be non-negative."));
		}
		if (IndexByFunctionId.Contains(Function.EngineLocalFunctionId))
		{
			return FailInput(TEXT(
				"TypedASTJITCallGraphDuplicateFunctionId: task-local function coordinates must be unique."));
		}
		if (Function.FunctionKey.Hash.IsZero())
		{
			return FailInput(TEXT(
				"TypedASTJITCallGraphInvalidStableKey: function stable keys must be non-zero."));
		}
		if (!Function.CapabilityProfile.IsValid())
		{
			return FailInput(TEXT(
				"TypedASTJITCallGraphInvalidCapabilityProfile: every normalized function requires one verified generation capability profile."));
		}
		const FString StableKey = KeyString(Function.FunctionKey);
		if (SeenStableKeys.Contains(StableKey))
		{
			return FailInput(TEXT(
				"TypedASTJITCallGraphDuplicateStableKey: distinct task-local functions cannot share one stable key."));
		}
		IndexByFunctionId.Add(Function.EngineLocalFunctionId, Index);
		SeenStableKeys.Add(StableKey);
		if (Function.FunctionKey == RootFunctionKey)
		{
			RootIndex = Index;
		}
	}
	if (RootIndex == INDEX_NONE || !Functions[RootIndex].bIsUFunctionRoot)
	{
		return FailInput(TEXT(
			"TypedASTJITCallGraphRootMissing: selected stable key is not an exact UFUNCTION root."));
	}

	OutPlan.RootFunctionKey = RootFunctionKey;
	OutPlan.RequiredDirectCapabilities =
		Options.ExecutionRequirements.RequiredCapabilities;
	OutPlan.ExecutionRequirementsIdentityHash =
		Options.ExecutionRequirements.IdentityHash;
	TArray<TArray<int32>> DirectEdges;
	DirectEdges.SetNum(Functions.Num());
	TArray<uint8> VisitState;
	VisitState.SetNumZeroed(Functions.Num());
	TSet<int32> DirectFunctionIndices;

	auto FindPlannedFunction = [&](
		const FAngelscriptStableFunctionKey& Key)
		-> FAngelscriptTypedASTJITCallClosureFunction*
	{
		return OutPlan.Functions.FindByPredicate(
			[&Key](FAngelscriptTypedASTJITCallClosureFunction& Function)
			{
				return Function.FunctionKey == Key;
			});
	};

	auto EnsureFunctionPlan = [&](const int32 FunctionIndex,
		const bool bSelectedRoot)
		-> FAngelscriptTypedASTJITCallClosureFunction&
	{
		const FAngelscriptTypedASTJITCallClosureFunctionInput& Input =
			Functions[FunctionIndex];
		if (FAngelscriptTypedASTJITCallClosureFunction* Existing =
			FindPlannedFunction(Input.FunctionKey))
		{
			return *Existing;
		}
		FAngelscriptTypedASTJITCallClosureFunction& Added =
			OutPlan.Functions.Emplace_GetRef();
		Added.FunctionKey = Input.FunctionKey;
		Added.Eligibility = EvaluateFunction(Input, bSelectedRoot);
		Added.Disposition = ClassifyFunctionDisposition(
			Input, Added.Eligibility, bSelectedRoot);
		return Added;
	};

	FAngelscriptTypedASTJITCallClosureFunction& PlannedRoot =
		EnsureFunctionPlan(RootIndex, true);
	OutPlan.RootEligibility = PlannedRoot.Eligibility;
	if (!PlannedRoot.Eligibility.bEligible)
	{
		return true;
	}

	bool bSemanticFailure = false;
	auto PlanNativeCall = [&OutPlan, &Options, &bSemanticFailure](
		FAngelscriptTypedASTJITCallClosureCall& Call,
		const FAngelscriptStaticJITNativeCallTarget& Target,
		const asSSourceSpan& SourceSpan)
	{
		Call.TargetKind = EAngelscriptTypedASTJITCallTargetKind::SystemFunction;
		Call.EngineLocalTargetFunctionId = Target.EngineLocalFunctionId;
		Call.ExternalCallDescriptor = Target.Descriptor;
		if (!Target.Descriptor.IsAbsent()
			&& EnumHasAnyFlags(
				Target.Descriptor.RouteFlags,
				EAngelscriptStaticJITNativeCallRouteFlags::MaySuspend))
		{
			SetFailure(OutPlan,
				EAngelscriptTypedASTJITFallbackReason::SuspendOrExceptionState,
				SourceSpan,
				FString::Printf(TEXT(
					"NativeCallMaySuspend: SystemFunctionId=%d Declaration=%s"),
					Target.EngineLocalFunctionId,
					*Target.CanonicalDeclaration));
			bSemanticFailure = true;
			return false;
		}
		if (Target.Descriptor.IsAbsent())
		{
			Call.ExternalCallValidation.bMetadataValid =
				Target.bScalarBridgeCallable
				&& Target.ExpectedVMBridgeABI.IsValid();
			Call.ExternalCallValidation.bScalarBridgeCallable =
				Call.ExternalCallValidation.bMetadataValid;
			Call.ExternalCallValidation.Code =
				Call.ExternalCallValidation.bMetadataValid
					? EAngelscriptStaticJITNativeCallValidationCode::Valid
					: EAngelscriptStaticJITNativeCallValidationCode::Absent;
			Call.ExternalCallValidation.Detail =
				Call.ExternalCallValidation.bMetadataValid
					? FString::Printf(TEXT("AutomaticVMScalarBridge: %s"),
						*Target.ExpectedVMBridgeABI.Canonical)
					: TEXT("AutomaticVMScalarBridgeUnavailable");
			if (Call.ExternalCallValidation.bScalarBridgeCallable
				&& Options.bNativeScalarBridgeAvailable)
			{
				Call.Disposition =
					EAngelscriptTypedASTJITCallDisposition::Bridge;
				return true;
			}
			SetFailure(OutPlan,
				EAngelscriptTypedASTJITFallbackReason::UnsupportedCall,
				SourceSpan,
				FString::Printf(TEXT(
					"ExternalCallRouteUnavailable: SystemFunctionId=%d Declaration=%s Detail=%s"),
					Target.EngineLocalFunctionId,
					*Target.CanonicalDeclaration,
					*Call.ExternalCallValidation.Detail));
			bSemanticFailure = true;
			return false;
		}

		FAngelscriptStaticJITNativeCallValidationContext ValidationContext;
		ValidationContext.ExpectedNativeABI = Target.ExpectedNativeABI;
		ValidationContext.ExpectedScalarABI = Target.ExpectedScalarABI;
		ValidationContext.KnownModules = Options.KnownNativeCallModules;
		ValidationContext.PublicDependencyModules =
			Options.PublicNativeCallDependencyModules;
		Call.ExternalCallValidation =
			ValidateAngelscriptStaticJITNativeCallDescriptor(
				Target.Descriptor, ValidationContext);
		if (!Call.ExternalCallValidation.bMetadataValid)
		{
			SetFailure(OutPlan,
				EAngelscriptTypedASTJITFallbackReason::UnsupportedCall,
				SourceSpan,
				FString::Printf(TEXT(
					"ExternalCallDescriptorRejected: SystemFunctionId=%d Declaration=%s Detail=%s"),
					Target.EngineLocalFunctionId,
					*Target.CanonicalDeclaration,
					*Call.ExternalCallValidation.Detail));
			bSemanticFailure = true;
			return false;
		}
		if (Call.ExternalCallValidation.bDirectCallable
			&& Target.Descriptor.ExceptionPolicy
				== EAngelscriptStaticJITNativeCallExceptionPolicy::MaySetScriptException)
		{
			SetFailure(OutPlan,
				EAngelscriptTypedASTJITFallbackReason::ExceptionPayloadUnavailable,
				SourceSpan,
				FString::Printf(TEXT(
					"DirectNativeExceptionPayloadUnavailable: SystemFunctionId=%d Declaration=%s"),
					Target.EngineLocalFunctionId,
					*Target.CanonicalDeclaration));
			bSemanticFailure = true;
			return false;
		}
		if (Call.ExternalCallValidation.bDirectCallable)
		{
			switch (Target.Descriptor.Linkage)
			{
			case EAngelscriptStaticJITNativeCallLinkage::ExportedSymbol:
				Call.Disposition =
					EAngelscriptTypedASTJITCallDisposition::DirectExported;
				break;
			case EAngelscriptStaticJITNativeCallLinkage::HeaderInline:
				Call.Disposition =
					EAngelscriptTypedASTJITCallDisposition::DirectInline;
				break;
			case EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeThunk:
				Call.Disposition =
					EAngelscriptTypedASTJITCallDisposition::RuntimeThunk;
				break;
			case EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeCallable:
				Call.Disposition =
					EAngelscriptTypedASTJITCallDisposition::DirectExported;
				break;
			default:
				break;
			}
			if (Call.Disposition
				!= EAngelscriptTypedASTJITCallDisposition::Unsupported)
			{
				return true;
			}
		}
		else if (Call.ExternalCallValidation.bCurrentNativeCallable
			&& Target.bCurrentNativeCallable)
		{
			Call.Disposition =
				EAngelscriptTypedASTJITCallDisposition::CurrentNativeBinding;
			return true;
		}
		else if (Call.ExternalCallValidation.bScalarBridgeCallable
			&& Options.bNativeScalarBridgeAvailable)
		{
			Call.Disposition =
				EAngelscriptTypedASTJITCallDisposition::Bridge;
			return true;
		}

		SetFailure(OutPlan,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedCall,
			SourceSpan,
			FString::Printf(TEXT(
				"ExternalCallRouteUnavailable: SystemFunctionId=%d Declaration=%s Detail=%s"),
				Target.EngineLocalFunctionId,
				*Target.CanonicalDeclaration,
				*Call.ExternalCallValidation.Detail));
		bSemanticFailure = true;
		return false;
	};
	TFunction<void(int32)> VisitFunction;
	VisitFunction = [&](const int32 CallerIndex)
	{
		if (bSemanticFailure || VisitState[CallerIndex] == 2)
		{
            return;
		}
		if (VisitState[CallerIndex] == 1)
		{
            return;
		}
		VisitState[CallerIndex] = 1;
		DirectFunctionIndices.Add(CallerIndex);
		const FAngelscriptTypedASTJITCallClosureFunctionInput& Caller = Functions[CallerIndex];
		if (Caller.Eligibility.SealedAST != nullptr && Caller.Eligibility.SealedAST->IsPublishable())
		{
            const asCASTContext &Context = *Caller.Eligibility.SealedAST;
            TArray<asASTExprId> CanonicalCalls;
            CollectAngelscriptTypedASTJITCanonicalCallExprs(
                Context, Caller.Eligibility.CanonicalFunctionDecl,
                CanonicalCalls);
            for (const asASTExprId CallId : CanonicalCalls)
			{
				const asCExpr* const Expression = Context.GetExpr(CallId);
				const asCDecl* const Target =
					Expression != nullptr ? Context.GetDecl(Expression->resolvedDecl) : nullptr;
				const asSSourceSpan SourceSpan =
					Expression != nullptr ? CanonicalSpan(Context, Expression->range)
										  : asSSourceSpan{};
				if (Expression == nullptr || Target == nullptr)
				{
					SetFailure(
						OutPlan,
						EAngelscriptTypedASTJITFallbackReason::UnsupportedCall,
						SourceSpan,
						FString::Printf(TEXT("CanonicalResolvedTargetMissing: Caller=%s "
											 "CallExpr=%u %s."),
										*KeyString(Caller.FunctionKey),
										CallId.value,
										Expression != nullptr
											? *DescribeCanonicalExpression(Context, *Expression)
											: TEXT("Expression=dangling")));
					bSemanticFailure = true;
					break;
				}

				FAngelscriptTypedASTJITCallClosureCall& Call = OutPlan.Calls.Emplace_GetRef();
				Call.CallerFunctionKey = Caller.FunctionKey;
				Call.Expression = CallId;
				Call.SourceSpan = SourceSpan;
				Call.Disposition = EAngelscriptTypedASTJITCallDisposition::Unsupported;
				if (Target->kind == asAST_DECL_IMPORT)
				{
					Call.TargetKind = EAngelscriptTypedASTJITCallTargetKind::ImportedFunction;
					SetFailure(OutPlan,
							   EAngelscriptTypedASTJITFallbackReason::UnsupportedImportedRoute,
							   SourceSpan,
							   FString::Printf(TEXT("ImportedBindingRouteUnavailable: Caller=%s "
													"CallExpr=%u TargetDecl=%s stays mutable."),
											   *KeyString(Caller.FunctionKey),
											   CallId.value,
											   UTF8_TO_TCHAR(Target->stableKey.AddressOf())));
					bSemanticFailure = true;
					break;
				}

				bool bAmbiguous = false;
				const int32 CalleeIndex =
					FindExactCanonicalFunctionIndex(Functions, Context, Target->id, bAmbiguous);
				if (CalleeIndex == INDEX_NONE)
				{
					const FString TargetStableKey = UTF8_TO_TCHAR(
						Target->stableKey.AddressOf());
					const FString TargetOrigin = UTF8_TO_TCHAR(Target->origin.AddressOf());
					const FAngelscriptStaticJITNativeCallTarget* NativeTarget = nullptr;
					if (!bAmbiguous && !TargetStableKey.IsEmpty())
					{
						NativeTarget = NativeCallTargetByCanonicalDeclKey.FindRef(
							TargetStableKey);
					}
					if (NativeTarget != nullptr)
					{
						PlanNativeCall(Call, *NativeTarget, SourceSpan);
						if (bSemanticFailure)
						{
							break;
						}
						continue;
					}

					Call.TargetKind = TargetStableKey.IsEmpty()
										  ? EAngelscriptTypedASTJITCallTargetKind::Unspecified
										  : EAngelscriptTypedASTJITCallTargetKind::SystemFunction;
					SetFailure(
						OutPlan,
						EAngelscriptTypedASTJITFallbackReason::UnsupportedCall,
						SourceSpan,
						FString::Printf(TEXT("CanonicalTargetRouteUnavailable: Caller=%s "
											 "CallExpr=%u TargetDeclId=%u StableDeclKey=%s "
											 "Origin=%s ExactScriptMatches=%s "
											 "ExactNativeMatches=%s. The closure planner never "
											 "guesses by function name."),
										*KeyString(Caller.FunctionKey),
										CallId.value,
										Target->id.value,
										*TargetStableKey,
										*TargetOrigin,
										bAmbiguous ? TEXT("ambiguous") : TEXT("none"),
										TEXT("none")));
					bSemanticFailure = true;
					break;
				}

				const FAngelscriptTypedASTJITCallClosureFunctionInput& Callee =
					Functions[CalleeIndex];
				FAngelscriptTypedASTJITCallClosureFunction& PlannedCallee =
					EnsureFunctionPlan(CalleeIndex, false);
				Call.CalleeFunctionKey = Callee.FunctionKey;
				Call.TargetKind = EAngelscriptTypedASTJITCallTargetKind::ScriptFunction;
				Call.EngineLocalTargetFunctionId = Callee.EngineLocalFunctionId;
				Call.CalleeEligibility = PlannedCallee.Eligibility;
				Call.Disposition = CallDispositionFor(PlannedCallee.Disposition);

				if (PlannedCallee.Disposition ==
					EAngelscriptTypedASTJITClosureFunctionDisposition::Unsupported)
				{
					SetFailure(
						OutPlan,
						EAngelscriptTypedASTJITFallbackReason::UnsupportedCall,
						SourceSpan,
						FString::Printf(TEXT("CanonicalCalleeDisposition=Unsupported Caller=%s "
											 "CallExpr=%u TargetDecl=%s Callee=%s Reason=%s "
											 "Detail=%s"),
										*KeyString(Caller.FunctionKey),
										CallId.value,
										UTF8_TO_TCHAR(Target->stableKey.AddressOf()),
										*KeyString(Callee.FunctionKey),
										LexToString(PlannedCallee.Eligibility.Reason),
										*PlannedCallee.Eligibility.Detail));
					bSemanticFailure = true;
					break;
				}
				if (PlannedCallee.Disposition ==
					EAngelscriptTypedASTJITClosureFunctionDisposition::Bridge)
				{
					continue;
				}

				DirectEdges[CallerIndex].AddUnique(CalleeIndex);
				VisitFunction(CalleeIndex);
				if (bSemanticFailure)
				{
					break;
				}
			}
			VisitState[CallerIndex] = 2;
			return;
		}

		SetFailure(OutPlan,
				   EAngelscriptTypedASTJITFallbackReason::MissingCanonicalAST,
				   {},
				   TEXT("Direct TypedASTJIT closure member has no sealed "
						"canonical AST."));
		bSemanticFailure = true;
	};

	VisitFunction(RootIndex);
	if (bSemanticFailure)
	{
		OutPlan.Functions.Sort([](const auto& Left, const auto& Right)
							   { return KeyLess(Left.FunctionKey, Right.FunctionKey); });
		return true;
	}

	TArray<int32> TarjanIndex;
	TArray<int32> LowLink;
	TArray<bool> OnStack;
	TarjanIndex.Init(INDEX_NONE, Functions.Num());
	LowLink.Init(INDEX_NONE, Functions.Num());
	OnStack.Init(false, Functions.Num());
	TArray<int32> Stack;
	int32 NextIndex = 0;
	TFunction<void(int32)> StrongConnect;
	StrongConnect = [&](const int32 Vertex)
	{
		TarjanIndex[Vertex] = NextIndex;
		LowLink[Vertex] = NextIndex;
		++NextIndex;
		Stack.Add(Vertex);
		OnStack[Vertex] = true;

		TArray<int32> Neighbours = DirectEdges[Vertex];
		Neighbours.Sort([&](const int32 Left, const int32 Right)
		{
			return KeyLess(Functions[Left].FunctionKey,
				Functions[Right].FunctionKey);
		});
		for (const int32 Neighbour : Neighbours)
		{
			if (!DirectFunctionIndices.Contains(Neighbour))
			{
				continue;
			}
			if (TarjanIndex[Neighbour] == INDEX_NONE)
			{
				StrongConnect(Neighbour);
				LowLink[Vertex] = FMath::Min(
					LowLink[Vertex], LowLink[Neighbour]);
			}
			else if (OnStack[Neighbour])
			{
				LowLink[Vertex] = FMath::Min(
					LowLink[Vertex], TarjanIndex[Neighbour]);
			}
		}

		if (LowLink[Vertex] == TarjanIndex[Vertex])
		{
			FAngelscriptTypedASTJITCallClosureComponent Component;
			for (;;)
			{
				const int32 Member = Stack.Pop(EAllowShrinking::No);
				OnStack[Member] = false;
				Component.Members.Add(Functions[Member].FunctionKey);
				if (Member == Vertex)
				{
					break;
				}
			}
			Component.Members.Sort(KeyLess);
			Component.bRecursive = Component.Members.Num() > 1;
			if (!Component.bRecursive)
			{
				Component.bRecursive = DirectEdges[Vertex].Contains(Vertex);
			}
			OutPlan.Components.Add(MoveTemp(Component));
		}
	};

	TArray<int32> OrderedDirectIndices = DirectFunctionIndices.Array();
	OrderedDirectIndices.Sort([&](const int32 Left, const int32 Right)
	{
		return KeyLess(Functions[Left].FunctionKey,
			Functions[Right].FunctionKey);
	});
	for (const int32 FunctionIndex : OrderedDirectIndices)
	{
		if (TarjanIndex[FunctionIndex] == INDEX_NONE)
		{
			StrongConnect(FunctionIndex);
		}
	}
	OutPlan.Components.Sort([](const auto& Left, const auto& Right)
	{
		return KeyLess(Left.Members[0], Right.Members[0]);
	});

	bool bHasRecursiveComponent = false;
	for (int32 ComponentIndex = 0;
		ComponentIndex < OutPlan.Components.Num(); ++ComponentIndex)
	{
		const FAngelscriptTypedASTJITCallClosureComponent& Component =
			OutPlan.Components[ComponentIndex];
		bHasRecursiveComponent |= Component.bRecursive;
		for (const FAngelscriptStableFunctionKey& Member : Component.Members)
		{
			if (FAngelscriptTypedASTJITCallClosureFunction* Planned =
				FindPlannedFunction(Member))
			{
				Planned->ComponentIndex = ComponentIndex;
				Planned->bRecursiveComponent = Component.bRecursive;
			}
		}
	}
	if (bHasRecursiveComponent)
	{
		OutPlan.RequiredDirectCapabilities |=
			EAngelscriptTypedASTJITDirectCapability::RecursionBudget;
		FAngelscriptTypedASTJITExecutionRequirementsSnapshot
			ClosureRequirements;
		FString RequirementsError;
		if (!BuildAngelscriptTypedASTJITExecutionRequirementsSnapshot(
			OutPlan.RequiredDirectCapabilities, ClosureRequirements,
			&RequirementsError))
		{
			return FailInput(RequirementsError);
		}
		OutPlan.ExecutionRequirementsIdentityHash =
			ClosureRequirements.IdentityHash;
	}

	bool bHasAvailableIntersection = false;
	for (const int32 FunctionIndex : OrderedDirectIndices)
	{
		const EAngelscriptTypedASTJITDirectCapability Available =
			Functions[FunctionIndex].CapabilityProfile.AvailableCapabilities;
		if (!bHasAvailableIntersection)
		{
			OutPlan.AvailableDirectCapabilities = Available;
			bHasAvailableIntersection = true;
		}
		else
		{
			OutPlan.AvailableDirectCapabilities &= Available;
		}
	}
	FAngelscriptTypedASTJITGenerationCapabilityProfile
		ClosureCapabilityProfile;
	FString CapabilityProfileError;
	if (!BuildAngelscriptTypedASTJITGenerationCapabilityProfile(
		OutPlan.AvailableDirectCapabilities, ClosureCapabilityProfile,
		&CapabilityProfileError))
	{
		return FailInput(CapabilityProfileError);
	}
	OutPlan.AvailableCapabilityProfileIdentityHash =
		ClosureCapabilityProfile.IdentityHash;

	const uint16 RequiredBits = static_cast<uint16>(
		OutPlan.RequiredDirectCapabilities);
	const uint16 AvailableBits = static_cast<uint16>(
		OutPlan.AvailableDirectCapabilities);
	const EAngelscriptTypedASTJITDirectCapability MissingCapabilities =
		static_cast<EAngelscriptTypedASTJITDirectCapability>(
			RequiredBits & ~AvailableBits);
	if (MissingCapabilities
		!= EAngelscriptTypedASTJITDirectCapability::None)
	{
		int32 MissingFunctionIndex = RootIndex;
		for (const int32 FunctionIndex : OrderedDirectIndices)
		{
			const uint16 FunctionAvailable = static_cast<uint16>(
				Functions[FunctionIndex].CapabilityProfile.
					AvailableCapabilities);
			if ((RequiredBits & ~FunctionAvailable) != 0)
			{
				MissingFunctionIndex = FunctionIndex;
				break;
			}
		}

		asSSourceSpan FailureSpan = FunctionInputSpan(
			Functions[MissingFunctionIndex]);
		for (FAngelscriptTypedASTJITCallClosureCall& Call : OutPlan.Calls)
		{
			if (Call.CalleeFunctionKey
				== Functions[MissingFunctionIndex].FunctionKey
				&& Call.Disposition
					!= EAngelscriptTypedASTJITCallDisposition::Bridge)
			{
				FailureSpan = Call.SourceSpan;
				Call.Disposition =
					EAngelscriptTypedASTJITCallDisposition::Unsupported;
				break;
			}
		}
		if (FAngelscriptTypedASTJITCallClosureFunction* MissingFunction =
			FindPlannedFunction(
				Functions[MissingFunctionIndex].FunctionKey))
		{
			MissingFunction->Disposition =
				EAngelscriptTypedASTJITClosureFunctionDisposition::Unsupported;
		}

		const bool bRecursion = EnumHasAnyFlags(MissingCapabilities,
			EAngelscriptTypedASTJITDirectCapability::RecursionBudget);
		FAngelscriptTypedASTJITExecutionRequirementsSnapshot
			ClosureRequirements;
		FString RequirementsError;
		if (!BuildAngelscriptTypedASTJITExecutionRequirementsSnapshot(
			OutPlan.RequiredDirectCapabilities, ClosureRequirements,
			&RequirementsError))
		{
			return FailInput(RequirementsError);
		}
		const FAngelscriptTypedASTJITExecutionRouteEligibility Route =
			EvaluateAngelscriptTypedASTJITExecutionRoute(
				Functions[MissingFunctionIndex].CapabilityProfile,
				ClosureRequirements,
				MissingFunctionIndex != RootIndex && !bRecursion);
		SetFailure(OutPlan,
			Route.Reason,
			FailureSpan,
			FString::Printf(TEXT("%s: Required=%s Available=%s Callee=%s"),
				*Route.StableDetail,
				*LexToString(OutPlan.RequiredDirectCapabilities),
				*LexToString(Functions[MissingFunctionIndex].
					CapabilityProfile.AvailableCapabilities),
				*KeyString(Functions[MissingFunctionIndex].FunctionKey)));
	}
	else
	{
		OutPlan.bEligible = true;
		OutPlan.RootEligibility.bEligible = true;
		OutPlan.RootEligibility.Reason =
			EAngelscriptTypedASTJITFallbackReason::None;
		OutPlan.RootEligibility.Detail = FString::Printf(TEXT(
			"Verified direct closure: Functions=%d Calls=%d Components=%d Required=%s Available=%s"),
			OutPlan.Functions.Num(), OutPlan.Calls.Num(),
			OutPlan.Components.Num(),
			*LexToString(OutPlan.RequiredDirectCapabilities),
			*LexToString(OutPlan.AvailableDirectCapabilities));
	}

	OutPlan.Functions.Sort([](const auto& Left, const auto& Right)
	{
		return KeyLess(Left.FunctionKey, Right.FunctionKey);
	});
	OutPlan.Calls.Sort([](const auto& Left, const auto& Right)
	{
		if (Left.CallerFunctionKey != Right.CallerFunctionKey)
		{
			return KeyLess(Left.CallerFunctionKey, Right.CallerFunctionKey);
		}
		if (Left.Expression.value != Right.Expression.value)
		{
			return Left.Expression.value < Right.Expression.value;
		}
		return KeyLess(Left.CalleeFunctionKey, Right.CalleeFunctionKey);
	});
	return true;
}

#endif // AS_CAN_GENERATE_JIT
