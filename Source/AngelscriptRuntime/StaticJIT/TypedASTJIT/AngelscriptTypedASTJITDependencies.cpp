#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITDependencies.h"

#include "Cache/AngelscriptCacheEnvironment.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_typeinfo.h"
#include "EndAngelscriptHeaders.h"

#if AS_CAN_GENERATE_JIT

namespace AngelscriptTypedASTJITDependencies_Private
{
	FAngelscriptTypedASTJITDependencyAnalysis Fail(
		const asSTypedSemanticSourceSpan& Span,
		const FString& Detail)
	{
		FAngelscriptTypedASTJITDependencyAnalysis Result;
		Result.FailureReason =
			EAngelscriptTypedASTJITFallbackReason::SemanticDependencyMismatch;
		Result.FailureSpan = Span;
		Result.FailureDetail = Detail;
		return Result;
	}

	FString JoinKinds(
		const TArray<EAngelscriptArtifactSemanticDependencyKind>& Kinds)
	{
		TArray<FString> Names;
		for (const EAngelscriptArtifactSemanticDependencyKind Kind : Kinds)
		{
			switch (Kind)
			{
			case EAngelscriptArtifactSemanticDependencyKind::Import:
				Names.Add(TEXT("Import"));
				break;
			case EAngelscriptArtifactSemanticDependencyKind::Declaration:
				Names.Add(TEXT("Declaration"));
				break;
			case EAngelscriptArtifactSemanticDependencyKind::Signature:
				Names.Add(TEXT("Signature"));
				break;
			case EAngelscriptArtifactSemanticDependencyKind::Inheritance:
				Names.Add(TEXT("Inheritance"));
				break;
			case EAngelscriptArtifactSemanticDependencyKind::ValueLayout:
				Names.Add(TEXT("ValueLayout"));
				break;
			case EAngelscriptArtifactSemanticDependencyKind::PropertyLayout:
				Names.Add(TEXT("PropertyLayout"));
				break;
			case EAngelscriptArtifactSemanticDependencyKind::GlobalStorage:
				Names.Add(TEXT("GlobalStorage"));
				break;
			case EAngelscriptArtifactSemanticDependencyKind::HardValue:
				Names.Add(TEXT("HardValue"));
				break;
			case EAngelscriptArtifactSemanticDependencyKind::Initializer:
				Names.Add(TEXT("Initializer"));
				break;
			case EAngelscriptArtifactSemanticDependencyKind::CompileOption:
				Names.Add(TEXT("CompileOption"));
				break;
			case EAngelscriptArtifactSemanticDependencyKind::EnvironmentAbi:
				Names.Add(TEXT("EnvironmentAbi"));
				break;
			case EAngelscriptArtifactSemanticDependencyKind::FunctionContent:
				Names.Add(TEXT("FunctionContent"));
				break;
			default:
				Names.Add(TEXT("Invalid"));
				break;
			}
		}
		Names.Sort();
		return Names.IsEmpty() ? TEXT("<missing>")
			: FString::Join(Names, TEXT(","));
	}

	bool HasExactReference(
		const FAngelscriptTypedASTJITSemanticUseManifest& Manifest,
		const FAngelscriptArtifactReference& Reference)
	{
		return Manifest.RequiredReferences.Contains(Reference);
	}

	const FAngelscriptTypedASTJITNativeCallEmissionPlan* FindNativePlan(
		const TConstArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan>
			Plans,
		const asTypedSemanticExpressionId Expression)
	{
		return Plans.FindByPredicate([Expression](const auto& Candidate)
		{
			return Candidate.Expression == Expression;
		});
	}

	const FAngelscriptTypedASTJITScriptCallEmissionPlan* FindScriptPlan(
		const TConstArrayView<FAngelscriptTypedASTJITScriptCallEmissionPlan>
			Plans,
		const asTypedSemanticExpressionId Expression)
	{
		return Plans.FindByPredicate([Expression](const auto& Candidate)
		{
			return Candidate.Expression == Expression;
		});
	}

	const FAngelscriptTypedASTJITGlobalDependency* FindGlobalDependency(
		const TConstArrayView<FAngelscriptTypedASTJITGlobalDependency>
			Dependencies,
		const int32 Coordinate)
	{
		const FAngelscriptTypedASTJITGlobalDependency* Match = nullptr;
		for (const FAngelscriptTypedASTJITGlobalDependency& Candidate
			: Dependencies)
		{
			if (Candidate.EngineLocalGlobalPropertyId != Coordinate)
			{
				continue;
			}
			if (Match != nullptr)
			{
				return nullptr;
			}
			Match = &Candidate;
		}
		return Match;
	}
}

const TCHAR* LexToString(
	const EAngelscriptTypedASTJITSemanticUseKind Kind)
{
	switch (Kind)
	{
	case EAngelscriptTypedASTJITSemanticUseKind::ScriptCallSignature:
		return TEXT("ScriptCallSignature");
	case EAngelscriptTypedASTJITSemanticUseKind::ScriptCallContent:
		return TEXT("ScriptCallContent");
	case EAngelscriptTypedASTJITSemanticUseKind::NativeCall:
		return TEXT("NativeCall");
	case EAngelscriptTypedASTJITSemanticUseKind::ImportedBindingSlot:
		return TEXT("ImportedBindingSlot");
	case EAngelscriptTypedASTJITSemanticUseKind::ReceiverType:
		return TEXT("ReceiverType");
	case EAngelscriptTypedASTJITSemanticUseKind::ParameterType:
		return TEXT("ParameterType");
	case EAngelscriptTypedASTJITSemanticUseKind::ReturnType:
		return TEXT("ReturnType");
	case EAngelscriptTypedASTJITSemanticUseKind::ExpressionType:
		return TEXT("ExpressionType");
	case EAngelscriptTypedASTJITSemanticUseKind::HiddenArgumentType:
		return TEXT("HiddenArgumentType");
	case EAngelscriptTypedASTJITSemanticUseKind::FoldedGlobalHardValue:
		return TEXT("FoldedGlobalHardValue");
	case EAngelscriptTypedASTJITSemanticUseKind::MutableGlobalStorage:
		return TEXT("MutableGlobalStorage");
	case EAngelscriptTypedASTJITSemanticUseKind::ProviderRoute:
		return TEXT("ProviderRoute");
	default:
		return TEXT("Invalid");
	}
}

FAngelscriptTypedASTJITDependencyAnalysis
AnalyzeAngelscriptTypedASTJITDependencies(
	const asCTypedSemanticFunction& HIR,
	const FAngelscriptTypedASTJITFunctionShape& Shape,
	const FAngelscriptStaticJITCompiledFunctionView& Function,
	const FAngelscriptStaticJITCompiledSourceGraph& Graph,
	const TConstArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan>
		NativeCalls,
	const TConstArrayView<FAngelscriptTypedASTJITScriptCallEmissionPlan>
		ScriptCalls,
	const TConstArrayView<FAngelscriptTypedASTJITGlobalDependency>
		GlobalDependencies)
{
	using namespace AngelscriptTypedASTJITDependencies_Private;
	const asSTypedSemanticSourceSpan RootSpan =
		HIR.rootStatement.IsValid()
			&& HIR.rootStatement.value < HIR.statements.GetLength()
		? HIR.statements[HIR.rootStatement.value].span
		: asSTypedSemanticSourceSpan{};
	if (Function.FunctionKey.Hash.IsZero()
		|| Function.ModuleKey.Hash.IsZero())
	{
		return Fail(RootSpan, TEXT(
			"Use=FunctionIdentity StableMapping=Missing: the dependency analyzer requires exact function and module keys."));
	}

	FAngelscriptTypedASTJITDependencyAnalysis Result;
	for (const FAngelscriptStaticJITDependencyView& Dependency
		: Graph.Dependencies)
	{
		if (Dependency.OwnerFunctionKey != Function.FunctionKey)
		{
			continue;
		}
		if (!Dependency.Reference.IsValid())
		{
			return Fail(RootSpan, TEXT(
				"Use=CompilerReference StableMapping=Invalid"));
		}
		Result.Manifest.RequiredReferences.AddUnique(Dependency.Reference);
	}
	Result.Manifest.RequiredReferences.Sort();

	for (const FAngelscriptStaticJITSemanticDependencyView& Dependency
		: Graph.SemanticDependencies)
	{
		if (Dependency.OwnerFunctionKey != Function.FunctionKey)
		{
			continue;
		}
		if (Dependency.Kind
				== EAngelscriptArtifactSemanticDependencyKind::Invalid
			|| !Dependency.Reference.IsValid())
		{
			return Fail(RootSpan, TEXT(
				"Use=CompilerArtifactDependency StableMapping=Invalid"));
		}
		if (!HasExactReference(Result.Manifest, Dependency.Reference))
		{
			return Fail(RootSpan, FString::Printf(TEXT(
				"Use=CompilerArtifactDependency StableMapping=MissingReference TargetKind=%u Target=%s"),
				static_cast<uint32>(Dependency.Reference.Kind),
				*Dependency.Reference.StableKey.ToHexString()));
		}
		FAngelscriptArtifactSemanticDependency& Copy =
			Result.Manifest.ArtifactDependencies.Emplace_GetRef();
		Copy.Kind = Dependency.Kind;
		Copy.Target = Dependency.Reference;
		Copy.ExpectedContentOrValue = Dependency.ExpectedContentOrValue;
	}
	Result.Manifest.ArtifactDependencies.Sort();

	auto AddUse = [&](const EAngelscriptTypedASTJITSemanticUseKind UseKind,
		const EAngelscriptArtifactSemanticDependencyKind RequiredKind,
		const FAngelscriptArtifactReference& Reference,
		const asSTypedSemanticSourceSpan& Span,
		const int32 Coordinate,
		const FStringView Target) -> bool
	{
		if (!Reference.IsValid()
			|| !HasExactReference(Result.Manifest, Reference))
		{
			Result = Fail(Span, FString::Printf(TEXT(
				"Use=%s RequiredKind=%u StableMapping=Missing Target=%s"),
				LexToString(UseKind), static_cast<uint32>(RequiredKind),
				Reference.StableKey.IsZero()
					? TEXT("<unmapped>")
					: *Reference.StableKey.ToHexString()));
			return false;
		}

		if (RequiredKind
			!= EAngelscriptArtifactSemanticDependencyKind::Invalid)
		{
			int32 ExactCount = 0;
			TArray<EAngelscriptArtifactSemanticDependencyKind> ActualKinds;
			for (const FAngelscriptArtifactSemanticDependency& Dependency
				: Result.Manifest.ArtifactDependencies)
			{
				if (Dependency.Target != Reference)
				{
					continue;
				}
				ActualKinds.AddUnique(Dependency.Kind);
				ExactCount += Dependency.Kind == RequiredKind ? 1 : 0;
			}
			if (ExactCount != 1)
			{
				Result = Fail(Span, FString::Printf(TEXT(
					"Use=%s RequiredKind=%u ActualKinds=%s Target=%s"),
					LexToString(UseKind),
					static_cast<uint32>(RequiredKind),
					*JoinKinds(ActualKinds),
					*Reference.StableKey.ToHexString()));
				return false;
			}
		}

		const bool bAlreadyPresent = Result.Manifest.Uses.ContainsByPredicate(
			[&](const FAngelscriptTypedASTJITSemanticUse& Existing)
			{
				return Existing.Kind == UseKind
					&& Existing.RequiredDependencyKind == RequiredKind
					&& Existing.Reference == Reference
					&& Existing.EngineLocalCoordinate == Coordinate;
			});
		if (!bAlreadyPresent)
		{
			FAngelscriptTypedASTJITSemanticUse& Use =
				Result.Manifest.Uses.Emplace_GetRef();
			Use.Kind = UseKind;
			Use.RequiredDependencyKind = RequiredKind;
			Use.Reference = Reference;
			Use.SourceSpan = Span;
			Use.EngineLocalCoordinate = Coordinate;
			Use.Target = Target;
		}
		return true;
	};

	auto AddTypeUse = [&](const asCDataType& Type,
		const EAngelscriptTypedASTJITSemanticUseKind UseKind,
		const asSTypedSemanticSourceSpan& Span) -> bool
	{
		asCTypeInfo* const TypeInfo = Type.GetTypeInfo();
		if (TypeInfo == nullptr)
		{
			return true;
		}

		const FAngelscriptStaticJITCompiledTypeView* Match = nullptr;
		bool bAmbiguous = false;
		for (const FAngelscriptStaticJITCompiledTypeView& Candidate
			: Graph.Types)
		{
			if (Candidate.EngineLocalType != TypeInfo)
			{
				continue;
			}
			if (Match != nullptr)
			{
				bAmbiguous = true;
				break;
			}
			Match = &Candidate;
		}

		FAngelscriptArtifactReference TypeReference;
		EAngelscriptArtifactSemanticDependencyKind RequiredKind =
			EAngelscriptArtifactSemanticDependencyKind::Invalid;
		if (!bAmbiguous && Match != nullptr && !Match->StableTypeKey.IsZero())
		{
			RequiredKind = (TypeInfo->flags & (asOBJ_VALUE | asOBJ_ENUM)) != 0
				? EAngelscriptArtifactSemanticDependencyKind::ValueLayout
				: EAngelscriptArtifactSemanticDependencyKind::Declaration;
			for (const FAngelscriptArtifactSemanticDependency& Dependency
				: Result.Manifest.ArtifactDependencies)
			{
				if (Dependency.Kind == RequiredKind
					&& Dependency.Target.Kind
						== EAngelscriptArtifactReferenceKind::ScriptType
					&& Dependency.Target.StableKey == Match->StableTypeKey)
				{
					if (TypeReference.IsValid()
						&& TypeReference != Dependency.Target)
					{
						return false;
					}
					TypeReference = Dependency.Target;
				}
			}
		}
		else if (!bAmbiguous
			&& FAngelscriptCacheEnvironmentIdentity::TryBuildTypeReference(
				*TypeInfo, TypeReference))
		{
			RequiredKind =
				EAngelscriptArtifactSemanticDependencyKind::EnvironmentAbi;
		}

		if (bAmbiguous || !TypeReference.IsValid())
		{
			Result = Fail(Span, FString::Printf(TEXT(
				"Use=%s StableMapping=Unmappable Type=%s"),
				LexToString(UseKind),
				UTF8_TO_TCHAR(TypeInfo->GetName())));
			return false;
		}
		return AddUse(UseKind, RequiredKind, TypeReference, Span,
			TypeInfo->GetTypeId(), UTF8_TO_TCHAR(TypeInfo->GetName()));
	};

	if (!AddTypeUse(Shape.ReturnType,
		EAngelscriptTypedASTJITSemanticUseKind::ReturnType, RootSpan))
	{
		return Result;
	}
	for (const asCDataType& ParameterType : Shape.ParameterTypes)
	{
		if (!AddTypeUse(ParameterType,
			EAngelscriptTypedASTJITSemanticUseKind::ParameterType,
			RootSpan))
		{
			return Result;
		}
	}
	if (!AddTypeUse(HIR.header.receiver.type,
		EAngelscriptTypedASTJITSemanticUseKind::ReceiverType, RootSpan))
	{
		return Result;
	}

	for (asUINT Index = 0; Index < HIR.expressions.GetLength(); ++Index)
	{
		const asSTypedSemanticExpression& Expression = HIR.expressions[Index];
		if (!AddTypeUse(Expression.type,
			EAngelscriptTypedASTJITSemanticUseKind::ExpressionType,
			Expression.span))
		{
			return Result;
		}
		if (Expression.kind
				== asETypedSemanticExpressionKind::FoldedGlobalConstant
			|| Expression.kind
				== asETypedSemanticExpressionKind::GlobalStorage)
		{
			const FAngelscriptTypedASTJITGlobalDependency* const Global =
				FindGlobalDependency(
					GlobalDependencies,
					Expression.resolvedGlobalPropertyId);
			const EAngelscriptArtifactSemanticDependencyKind RequiredKind =
				Expression.kind
					== asETypedSemanticExpressionKind::FoldedGlobalConstant
				? EAngelscriptArtifactSemanticDependencyKind::HardValue
				: EAngelscriptArtifactSemanticDependencyKind::GlobalStorage;
			const EAngelscriptTypedASTJITSemanticUseKind UseKind =
				Expression.kind
					== asETypedSemanticExpressionKind::FoldedGlobalConstant
				? EAngelscriptTypedASTJITSemanticUseKind::FoldedGlobalHardValue
				: EAngelscriptTypedASTJITSemanticUseKind::MutableGlobalStorage;
			if (Global == nullptr || Global->Kind != RequiredKind
				|| !AddUse(UseKind, RequiredKind,
					Global != nullptr ? Global->Reference
						: FAngelscriptArtifactReference{},
					Expression.span,
					Expression.resolvedGlobalPropertyId,
					TEXT("global")))
			{
				if (Result.FailureReason
					== EAngelscriptTypedASTJITFallbackReason::None)
				{
					Result = Fail(Expression.span, FString::Printf(TEXT(
						"Use=%s RequiredKind=%u StableMapping=Missing Coordinate=%d"),
						LexToString(UseKind),
						static_cast<uint32>(RequiredKind),
						Expression.resolvedGlobalPropertyId));
				}
				return Result;
			}
		}
		if (Expression.kind != asETypedSemanticExpressionKind::ResolvedCall)
		{
			continue;
		}

		for (asUINT ArgumentIndex = 0;
			ArgumentIndex < Expression.argumentProvenance.GetLength();
			++ArgumentIndex)
		{
			if (Expression.argumentProvenance[ArgumentIndex].origin
					!= asETypedSemanticArgumentOrigin::HostHidden
				|| ArgumentIndex >= Expression.operands.GetLength())
			{
				continue;
			}
			const asTypedSemanticExpressionId Operand =
				Expression.operands[ArgumentIndex];
			if (!Operand.IsValid()
				|| Operand.value >= HIR.expressions.GetLength()
				|| !AddTypeUse(HIR.expressions[Operand.value].type,
					EAngelscriptTypedASTJITSemanticUseKind::HiddenArgumentType,
					Expression.span))
			{
				return Result;
			}
		}

		if (Expression.callTargetKind
			== asETypedSemanticCallTargetKind::ScriptFunction)
		{
			const FAngelscriptTypedASTJITScriptCallEmissionPlan* const Plan =
				FindScriptPlan(ScriptCalls, Expression.id);
			if (Plan == nullptr
				|| Plan->EngineLocalTargetFunctionId
					!= Expression.resolvedFunctionId
				|| !AddUse(
					EAngelscriptTypedASTJITSemanticUseKind::ScriptCallSignature,
					EAngelscriptArtifactSemanticDependencyKind::Signature,
					Plan != nullptr ? Plan->TargetReference
						: FAngelscriptArtifactReference{},
					Expression.span,
					Expression.resolvedFunctionId,
					UTF8_TO_TCHAR(
						Expression.targetProvenance.canonicalDeclaration.AddressOf())))
			{
				return Result;
			}
			const bool bSelfRecursion =
				Plan->TargetReference.Kind
					== EAngelscriptArtifactReferenceKind::ScriptFunction
				&& Plan->TargetReference.StableKey
					== Function.FunctionKey.Hash;
			if (!bSelfRecursion)
			{
				const bool bHasFunctionContent =
					Result.Manifest.ArtifactDependencies.ContainsByPredicate(
						[Plan](const FAngelscriptArtifactSemanticDependency&
							Dependency)
						{
							return Dependency.Kind
									== EAngelscriptArtifactSemanticDependencyKind::
										FunctionContent
								&& Dependency.Target == Plan->TargetReference;
						});
				if (!bHasFunctionContent)
				{
					FAngelscriptArtifactSemanticDependency& Synthesized =
						Result.Manifest.ArtifactDependencies.Emplace_GetRef();
					Synthesized.Kind =
						EAngelscriptArtifactSemanticDependencyKind::
							FunctionContent;
					Synthesized.Target = Plan->TargetReference;
					for (const FAngelscriptStaticJITCompiledFunctionView&
						Candidate : Graph.Functions)
					{
						if (Candidate.FunctionKey.Hash
								== Plan->TargetReference.StableKey
							&& !Candidate.ExecutionHash.IsZero())
						{
							Synthesized.ExpectedContentOrValue =
								Candidate.ExecutionHash;
							break;
						}
					}
					Result.Manifest.ArtifactDependencies.Sort();
				}
				if (!AddUse(
					EAngelscriptTypedASTJITSemanticUseKind::ScriptCallContent,
					EAngelscriptArtifactSemanticDependencyKind::FunctionContent,
					Plan->TargetReference,
					Expression.span,
					Expression.resolvedFunctionId,
					UTF8_TO_TCHAR(
						Expression.targetProvenance.canonicalDeclaration.AddressOf())))
				{
					return Result;
				}
			}
			if (!AddUse(
				EAngelscriptTypedASTJITSemanticUseKind::ProviderRoute,
				EAngelscriptArtifactSemanticDependencyKind::Invalid,
				Plan->TargetReference,
				Expression.span,
				Expression.resolvedFunctionId,
				Plan->Symbol))
			{
				return Result;
			}
			continue;
		}

		if (Expression.callTargetKind
			== asETypedSemanticCallTargetKind::SystemFunction)
		{
			const FAngelscriptTypedASTJITNativeCallEmissionPlan* const Plan =
				FindNativePlan(NativeCalls, Expression.id);
			if (Plan == nullptr
				|| Plan->EngineLocalTargetFunctionId
					!= Expression.resolvedFunctionId)
			{
				return Fail(Expression.span, TEXT(
					"Use=NativeCall StableMapping=MissingPlan"));
			}
			if (Plan->TargetReference.IsValid())
			{
				Result.Manifest.RequiredReferences.AddUnique(
					Plan->TargetReference);
				Result.Manifest.RequiredReferences.Sort();
				const bool bHasEnvironmentAbi =
					Result.Manifest.ArtifactDependencies.ContainsByPredicate(
						[Plan](const FAngelscriptArtifactSemanticDependency&
							Dependency)
						{
							return Dependency.Kind
									== EAngelscriptArtifactSemanticDependencyKind::
										EnvironmentAbi
								&& Dependency.Target == Plan->TargetReference;
						});
				if (!bHasEnvironmentAbi)
				{
					FAngelscriptArtifactSemanticDependency& Synthesized =
						Result.Manifest.ArtifactDependencies.Emplace_GetRef();
					Synthesized.Kind =
						EAngelscriptArtifactSemanticDependencyKind::
							EnvironmentAbi;
					Synthesized.Target = Plan->TargetReference;
					Result.Manifest.ArtifactDependencies.Sort();
				}
			}
			if (!AddUse(
				EAngelscriptTypedASTJITSemanticUseKind::NativeCall,
				EAngelscriptArtifactSemanticDependencyKind::EnvironmentAbi,
				Plan->TargetReference,
				Expression.span,
				Expression.resolvedFunctionId,
				UTF8_TO_TCHAR(
					Expression.targetProvenance.canonicalDeclaration.AddressOf())))
			{
				return Result;
			}
			if (!Plan->Include.IsEmpty())
			{
				Result.Manifest.RequiredIncludes.AddUnique(Plan->Include);
			}
			if (!AddUse(
				EAngelscriptTypedASTJITSemanticUseKind::ProviderRoute,
				EAngelscriptArtifactSemanticDependencyKind::Invalid,
				Plan->TargetReference,
				Expression.span,
				Expression.resolvedFunctionId,
				Plan->RegisteredTarget))
			{
				return Result;
			}
			continue;
		}

		if (Expression.callTargetKind
			== asETypedSemanticCallTargetKind::ImportedFunction)
		{
			const FAngelscriptArtifactSemanticDependency* Match = nullptr;
			for (const FAngelscriptArtifactSemanticDependency& Dependency
				: Result.Manifest.ArtifactDependencies)
			{
				if (Dependency.Kind
						!= EAngelscriptArtifactSemanticDependencyKind::Import
					|| Dependency.Target.Kind
						!= EAngelscriptArtifactReferenceKind::ScriptImport)
				{
					continue;
				}
				if (Match != nullptr)
				{
					Match = nullptr;
					break;
				}
				Match = &Dependency;
			}
			if (Match == nullptr
				|| !AddUse(
					EAngelscriptTypedASTJITSemanticUseKind::ImportedBindingSlot,
					EAngelscriptArtifactSemanticDependencyKind::Import,
					Match != nullptr ? Match->Target
						: FAngelscriptArtifactReference{},
					Expression.span,
					Expression.resolvedFunctionId,
					UTF8_TO_TCHAR(Expression.targetProvenance.
						importedCanonicalSignature.AddressOf())))
			{
				return Result;
			}
		}
	}

	Result.Manifest.RequiredIncludes.Sort();
	Result.bValid = true;
	Result.FailureReason = EAngelscriptTypedASTJITFallbackReason::None;
	return Result;
}

#endif // AS_CAN_GENERATE_JIT
