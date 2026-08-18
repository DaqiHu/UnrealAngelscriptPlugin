#include "StaticJIT/AngelscriptStaticJITGenerationSnapshot.h"

#if AS_CAN_GENERATE_JIT

#include "AngelscriptEngine.h"
#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheEnvironment.h"
#include "Cache/AngelscriptCacheStableSymbolIdentity.h"
#include "StaticJIT/AngelscriptJITGeneration.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_callfunc.h"
#include "source/as_objecttype.h"
#include "source/as_property.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptStaticJITGenerationSnapshot_Private
{
	static bool Fail(FString* OutError, const FString& Detail)
	{
		if (OutError != nullptr)
		{
			*OutError = Detail;
		}
		return false;
	}

	static const FAngelscriptCacheCleanModuleArtifacts* FindArtifacts(
		const TConstArrayView<FAngelscriptCacheCleanModuleArtifacts> Artifacts,
		const FAngelscriptStableModuleKey& ModuleKey)
	{
		return Artifacts.FindByPredicate(
			[&ModuleKey](const FAngelscriptCacheCleanModuleArtifacts& Candidate)
			{
				return Candidate.ModuleKey == ModuleKey;
			});
	}

	static FAngelscriptFunctionContentHash BuildGenerationOnlyContentHash(
		const asCScriptFunction& Function)
	{
		TArray<uint8> CanonicalSourceBytes;
		if (Function.scriptData != nullptr
			&& Function.scriptData->artifactCanonicalSource.GetLength() > 0)
		{
			const asCString& CanonicalSource =
				Function.scriptData->artifactCanonicalSource;
			CanonicalSourceBytes.Append(
				reinterpret_cast<const uint8*>(CanonicalSource.AddressOf()),
				static_cast<int32>(CanonicalSource.GetLength()));
		}
		else
		{
			FTCHARToUTF8 DeclarationUtf8(UTF8_TO_TCHAR(
				Function.GetDeclaration(false, false, false)));
			CanonicalSourceBytes.Append(
				reinterpret_cast<const uint8*>(DeclarationUtf8.Get()),
				DeclarationUtf8.Length());
		}
		// Raw bytecode contains Engine-local numeric IDs. The generation-only
		// fallback identity therefore uses the compiler-retained canonical token
		// slice; bytecode remains a separate synchronous backend input.
		return FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
			CanonicalSourceBytes, CanonicalSourceBytes);
	}

	static FAngelscriptStaticJITGenerationType* FindType(
		FAngelscriptStaticJITGenerationSnapshot& Snapshot,
		const asITypeInfo* Type)
	{
		return Snapshot.Types.FindByPredicate(
			[Type](const FAngelscriptStaticJITGenerationType& Candidate)
			{
				return Candidate.EngineLocalType == Type;
			});
	}

	static FAngelscriptStaticJITGenerationFunction* FindFunction(
		FAngelscriptStaticJITGenerationSnapshot& Snapshot,
		const asIScriptFunction* Function)
	{
		return Snapshot.Functions.FindByPredicate(
			[Function](const FAngelscriptStaticJITGenerationFunction& Candidate)
			{
				return Candidate.EngineLocalFunction == Function;
			});
	}

	static bool IsRootNamespaceFunctionNamed(
		const asCScriptFunction& Function,
		const FStringView ExpectedName)
	{
		const char* const Namespace = Function.GetNamespace();
		if (Namespace != nullptr && Namespace[0] != '\0')
		{
			return false;
		}
		return FStringView(UTF8_TO_TCHAR(Function.GetName())) == ExpectedName;
	}

	static bool AssignLiteralAssetFunctionRoles(
		FAngelscriptStaticJITGenerationSnapshot& Snapshot,
		const FAngelscriptModuleDesc& ModuleDesc,
		const FAngelscriptStableModuleKey& ModuleKey,
		FString* OutError)
	{
		int32 PreviousPostInitFunctionIndex = INDEX_NONE;
		for (const FAngelscriptLiteralAssetFunctionDesc& LiteralAsset
			: ModuleDesc.LiteralAssetFunctions)
		{
			if (!ModuleDesc.PostInitFunctions.IsValidIndex(
					LiteralAsset.PostInitFunctionIndex)
				|| LiteralAsset.PostInitFunctionIndex
					<= PreviousPostInitFunctionIndex
				|| ModuleDesc.PostInitFunctions[
						LiteralAsset.PostInitFunctionIndex]
					!= LiteralAsset.GetterFunctionName
				|| LiteralAsset.GetterFunctionName.IsEmpty()
				|| LiteralAsset.InitializerFunctionName.IsEmpty())
			{
				return Fail(OutError, FString::Printf(TEXT(
					"StaticJIT literal-asset record '%s' does not exact-match the ordered PostInitFunctions authority in module '%s'."),
					*LiteralAsset.AssetName, *ModuleDesc.ModuleName));
			}
			PreviousPostInitFunctionIndex =
				LiteralAsset.PostInitFunctionIndex;

			FAngelscriptStaticJITGenerationFunction* Getter = nullptr;
			FAngelscriptStaticJITGenerationFunction* Initializer = nullptr;
			for (FAngelscriptStaticJITGenerationFunction& Candidate
				: Snapshot.Functions)
			{
				if (Candidate.ModuleKey != ModuleKey
					|| Candidate.EngineLocalFunction == nullptr)
				{
					continue;
				}
				asCScriptFunction* const Function =
					static_cast<asCScriptFunction*>(
						Candidate.EngineLocalFunction);
				if (IsRootNamespaceFunctionNamed(
						*Function, LiteralAsset.GetterFunctionName)
					&& Function->parameterTypes.GetLength() == 0)
				{
					if (Getter != nullptr)
					{
						return Fail(OutError, FString::Printf(TEXT(
							"StaticJIT literal-asset Getter '%s' is ambiguous in module '%s'."),
							*LiteralAsset.GetterFunctionName,
							*ModuleDesc.ModuleName));
					}
					Getter = &Candidate;
				}
				if (IsRootNamespaceFunctionNamed(
						*Function, LiteralAsset.InitializerFunctionName)
					&& Function->traits.GetTrait(
						asTRAIT_EXTERNAL_IMPLICIT_THIS)
					&& Function->parameterTypes.GetLength() == 1)
				{
					if (Initializer != nullptr)
					{
						return Fail(OutError, FString::Printf(TEXT(
							"StaticJIT literal-asset Initializer '%s' is ambiguous in module '%s'."),
							*LiteralAsset.InitializerFunctionName,
							*ModuleDesc.ModuleName));
					}
					Initializer = &Candidate;
				}
			}

			if (Getter == nullptr || Initializer == nullptr
				|| Getter->LiteralAssetRole
					!= EAngelscriptStaticJITLiteralAssetFunctionRole::None
				|| Initializer->LiteralAssetRole
					!= EAngelscriptStaticJITLiteralAssetFunctionRole::None)
			{
				return Fail(OutError, FString::Printf(TEXT(
					"StaticJIT literal-asset pair '%s' did not resolve to one unique root Getter and external-implicit-this Initializer in module '%s'."),
					*LiteralAsset.AssetName, *ModuleDesc.ModuleName));
			}
			Getter->LiteralAssetRole =
				EAngelscriptStaticJITLiteralAssetFunctionRole::Getter;
			Initializer->LiteralAssetRole =
				EAngelscriptStaticJITLiteralAssetFunctionRole::Initializer;
		}
		return true;
	}

	static EAngelscriptStaticJITInstalledCallableKind GetInstalledCallableKind(
		const asCScriptFunction& Function,
		const FAngelscriptGenerationFunctionProvenance& Provenance)
	{
		switch (Provenance.RegisteredBehaviour)
		{
		case asBEHAVE_CONSTRUCT:
		case asBEHAVE_LIST_CONSTRUCT:
		case asBEHAVE_FACTORY:
		case asBEHAVE_LIST_FACTORY:
			return EAngelscriptStaticJITInstalledCallableKind::Constructor;
		case asBEHAVE_DESTRUCT:
			return EAngelscriptStaticJITInstalledCallableKind::Destructor;
		case asBEHAVE_MAX:
			break;
		default:
			return EAngelscriptStaticJITInstalledCallableKind::Behavior;
		}
		if (Function.traits.GetTrait(asTRAIT_CONSTRUCTOR))
		{
			return EAngelscriptStaticJITInstalledCallableKind::Constructor;
		}
		if (Function.traits.GetTrait(asTRAIT_DESTRUCTOR))
		{
			return EAngelscriptStaticJITInstalledCallableKind::Destructor;
		}
		if (Function.sysFuncIntf != nullptr
			&& (Function.sysFuncIntf->callConv == ICC_GENERIC_FUNC
				|| Function.sysFuncIntf->callConv == ICC_GENERIC_FUNC_RETURNINMEM
				|| Function.sysFuncIntf->callConv == ICC_GENERIC_METHOD
				|| Function.sysFuncIntf->callConv == ICC_GENERIC_METHOD_RETURNINMEM))
		{
			return EAngelscriptStaticJITInstalledCallableKind::Generic;
		}
		if (Function.objectType != nullptr)
		{
			return EAngelscriptStaticJITInstalledCallableKind::Method;
		}
		return EAngelscriptStaticJITInstalledCallableKind::Global;
	}

	static bool CaptureInstalledNativeCallInventory(
		FAngelscriptStaticJITGenerationSnapshot& Snapshot,
		FAngelscriptEngine& Engine,
		asCScriptEngine& ScriptEngine,
		FString* OutError)
	{
		const FAngelscriptBindState* const BindState = Engine.GetBindState();
		if (BindState == nullptr)
		{
			return Fail(OutError,
				TEXT("StaticJIT generation Engine has no installed Bind state."));
		}
		const FAngelscriptGenerationFunctionProvenanceState* const
			GenerationProvenance =
				BindState->GenerationFunctionProvenance.Get();
		if (GenerationProvenance == nullptr)
		{
			return Fail(OutError,
				TEXT("StaticJIT generation Engine has no generation-only Bind provenance state."));
		}
		if (!GenerationProvenance->DuplicateFunctionIds.IsEmpty())
		{
			TArray<int32> DuplicateIds =
				GenerationProvenance->DuplicateFunctionIds.Array();
			DuplicateIds.Sort();
			return Fail(OutError, FString::Printf(TEXT(
				"StaticJIT generation Bind provenance contains duplicate Engine-local function id %d."),
				DuplicateIds[0]));
		}

		Snapshot.NativeCallInventory.Reserve(
			GenerationProvenance->Functions.Num());
		for (const TPair<int32, FAngelscriptGenerationFunctionProvenance>& Pair
			: GenerationProvenance->Functions)
		{
			asCScriptFunction* const Function = static_cast<asCScriptFunction*>(
				ScriptEngine.GetFunctionById(Pair.Key));
			if (Function == nullptr
				|| Function->engine != &ScriptEngine
				|| Function->funcType != asFUNC_SYSTEM)
			{
				return Fail(OutError, FString::Printf(TEXT(
					"StaticJIT installed Bind function %d is not a current system function."),
					Pair.Key));
			}

			const FAngelscriptGenerationFunctionProvenance& Provenance =
				Pair.Value;
			FAngelscriptStaticJITNativeCallInventoryEntry& Entry =
				Snapshot.NativeCallInventory.Emplace_GetRef();
			Entry.EngineLocalFunctionId = Pair.Key;
			Entry.CanonicalDeclaration = UTF8_TO_TCHAR(
				Function->GetDeclaration(false, false, false));
			Entry.Origin = Provenance.Origin;
			Entry.OwnerModule = Provenance.OwnerModule;
			Entry.Provider = Provenance.Provider;
			Entry.Phase = Provenance.Phase;
			Entry.ProviderSourceFile = Provenance.ProviderSourceFile;
			Entry.ProviderSourceLine = Provenance.ProviderSourceLine;
			Entry.CallableKind = GetInstalledCallableKind(
				*Function, Provenance);
			Entry.NamespaceCanonicalName = UTF8_TO_TCHAR(
				Function->GetNamespace());
			if (Function->objectType != nullptr)
			{
				const FString ReceiverNamespace = UTF8_TO_TCHAR(
					Function->objectType->GetNamespace());
				const FString ReceiverName = UTF8_TO_TCHAR(
					Function->objectType->GetName());
				Entry.ReceiverCanonicalName = ReceiverNamespace.IsEmpty()
					? ReceiverName
					: ReceiverNamespace + TEXT("::") + ReceiverName;
			}
			Entry.bHasStableReference =
				FAngelscriptCacheEnvironmentIdentity::TryBuildFunctionReference(
					*Function, Entry.StableReference);
			if (!Entry.bHasStableReference
				|| Entry.StableReference.Kind
					!= EAngelscriptArtifactReferenceKind::EnvironmentSymbol
				|| !Entry.StableReference.IsValid()
				|| Entry.StableReference.ExpectedAbi.IsZero())
			{
				Entry.bHasStableReference = false;
				Entry.StableReference = {};
				Entry.StableReferenceError = FString::Printf(TEXT(
					"CacheEnvironmentStableReferenceUnavailable: FunctionId=%d Declaration=%s"),
					Pair.Key, *Entry.CanonicalDeclaration);
			}
			Entry.FunctionTraitBits =
				static_cast<uint32>(Function->traits.traits);
			switch (Function->compileOutType)
			{
			case asECompileOutType::CompileCalls:
				Entry.CompileOutKind =
					EAngelscriptStaticJITInstalledCompileOutKind::CompileCalls;
				break;
			case asECompileOutType::CompileOutEntirely:
				Entry.CompileOutKind =
					EAngelscriptStaticJITInstalledCompileOutKind::CompileOutEntirely;
				break;
			case asECompileOutType::ReplaceWithFirstParam:
				Entry.CompileOutKind =
					EAngelscriptStaticJITInstalledCompileOutKind::ReplaceWithFirstParam;
				break;
			case asECompileOutType::CompileOutAsMethodChain:
				Entry.CompileOutKind =
					EAngelscriptStaticJITInstalledCompileOutKind::CompileOutAsMethodChain;
				break;
			default:
				return Fail(OutError, FString::Printf(TEXT(
					"StaticJIT installed Bind function %d has unknown compile-out kind %u."),
					Pair.Key, static_cast<uint32>(Function->compileOutType)));
			}
			for (asUINT Index = 0;
				Index < Function->defaultArgs.GetLength(); ++Index)
			{
				Entry.DefaultArgumentCount +=
					Function->defaultArgs[Index] != nullptr ? 1 : 0;
			}
			Entry.HiddenArgumentIndex = Function->hiddenArgumentIndex;
			Entry.DeterminesOutputTypeArgumentIndex =
				Function->determinesOutputTypeArgumentIndex;
			Entry.bReturnsOnStack = Function->DoesReturnOnStack();
			Entry.bUsesWorldContext = Function->traits.GetTrait(
				asEFuncTrait::asTRAIT_USES_WORLDCONTEXT);
			if (Function->sysFuncIntf != nullptr)
			{
				Entry.RegisteredCallConv = static_cast<uint32>(
					Function->sysFuncIntf->callConv);
				if (Function->sysFuncIntf->caller.IsBound())
				{
					Entry.DispatchKind =
						EAngelscriptStaticJITInstalledDispatchKind::FunctionCaller;
				}
				else if (Function->sysFuncIntf->callConv == ICC_GENERIC_FUNC)
				{
					Entry.DispatchKind =
						EAngelscriptStaticJITInstalledDispatchKind::GenericFunction;
				}
				else if (Function->sysFuncIntf->callConv == ICC_GENERIC_METHOD)
				{
					Entry.DispatchKind =
						EAngelscriptStaticJITInstalledDispatchKind::GenericMethod;
				}
				else
				{
					Entry.DispatchKind =
						EAngelscriptStaticJITInstalledDispatchKind::NativeCallingConvention;
				}
				Entry.RegisteredParameterSize =
					Function->sysFuncIntf->paramSize;
				Entry.RegisteredReturnSize =
					Function->sysFuncIntf->scriptReturnSize;
				Entry.bHasSystemCaller =
					Function->sysFuncIntf->caller.IsBound();
				switch (Function->sysFuncIntf->passFirstParamMetaData)
				{
				case asEFirstParamMetaData::None:
					Entry.FirstParamMetadata =
						EAngelscriptStaticJITInstalledFirstParamMetadata::None;
					break;
				case asEFirstParamMetaData::ScriptFunction:
					Entry.FirstParamMetadata =
						EAngelscriptStaticJITInstalledFirstParamMetadata::ScriptFunction;
					break;
				case asEFirstParamMetaData::ScriptObjectType:
					Entry.FirstParamMetadata =
						EAngelscriptStaticJITInstalledFirstParamMetadata::ScriptObjectType;
					break;
				default:
					return Fail(OutError, FString::Printf(TEXT(
						"StaticJIT installed Bind function %d has unknown first-parameter metadata %u."),
						Pair.Key, static_cast<uint32>(
							Function->sysFuncIntf->passFirstParamMetaData)));
				}
			}

			if (FScriptFunctionNativeForm* const NativeForm =
				FScriptFunctionNativeForm::GetNativeForm(Function))
			{
				Entry.bHasNativeForm = true;
				const FAngelscriptNativeFormDescription NativeInfo =
					NativeForm->Describe();
				Entry.NativeFormKind = NativeInfo.Kind;
				Entry.NativeCallableDisplay = NativeInfo.Name;
				Entry.NativeCustomForm = NativeInfo.CustomForm;
				Entry.NativeTargetType = NativeInfo.TargetType;
				Entry.NativeHeader = NativeInfo.Header;
				Entry.NativeUnrealFunctionPath =
					NativeInfo.UnrealFunctionPath;
				Entry.NativeUFunctionRouteFlags =
					NativeInfo.UFunctionRouteFlags;
				Entry.bNativeFormTrivial = NativeInfo.bTrivial;
				Entry.bNativeFormGuaranteed = NativeInfo.bGuaranteed;
				Entry.bNativeFormNeedsCompare = NativeInfo.bNeedsCompare;
				Entry.bNativeFormNeedsCopy = NativeInfo.bNeedsCopy;
			}

			if (const FAngelscriptStaticJITNativeCallDescriptor* const Descriptor =
				FAngelscriptStaticJITNativeCallRegistry::Find(Function))
			{
				Entry.bHasExternalDescriptor = true;
				Entry.ExternalDescriptor = *Descriptor;
				if (Entry.NativeCallableDisplay.IsEmpty())
				{
					Entry.NativeCallableDisplay = Descriptor->Symbol;
				}
				if (Entry.NativeHeader.IsEmpty())
				{
					Entry.NativeHeader = Descriptor->Include;
				}
			}

			Entry.bHasExpectedNativeABI =
				TryBuildAngelscriptStaticJITNativeABIIdentity(
					*Function, Entry.ExpectedNativeABI,
					&Entry.NativeABIError);
			Entry.bHasExpectedScalarABI =
				TryBuildAngelscriptStaticJITScalarABIIdentity(
					*Function, Entry.ExpectedScalarABI,
					&Entry.ScalarABIError);

			FString DispatchError;
			if (!ValidateAngelscriptStaticJITInstalledNativeCallDispatch(
					Entry, &DispatchError))
			{
				return Fail(OutError, FString::Printf(TEXT(
					"StaticJIT installed Bind function %d has an invalid dispatch contract: %s"),
					Pair.Key, *DispatchError));
			}
		}

		Snapshot.NativeCallInventory.Sort(
			[](const FAngelscriptStaticJITNativeCallInventoryEntry& Left,
				const FAngelscriptStaticJITNativeCallInventoryEntry& Right)
			{
				if (Left.ProviderSourceFile != Right.ProviderSourceFile)
				{
					return Left.ProviderSourceFile < Right.ProviderSourceFile;
				}
				if (Left.ProviderSourceLine != Right.ProviderSourceLine)
				{
					return Left.ProviderSourceLine < Right.ProviderSourceLine;
				}
				if (Left.OwnerModule != Right.OwnerModule)
				{
					return Left.OwnerModule.LexicalLess(Right.OwnerModule);
				}
				if (Left.Provider != Right.Provider)
				{
					return Left.Provider.LexicalLess(Right.Provider);
				}
				if (Left.CanonicalDeclaration != Right.CanonicalDeclaration)
				{
					return Left.CanonicalDeclaration
						< Right.CanonicalDeclaration;
				}
				if (Left.CallableKind != Right.CallableKind)
				{
					return static_cast<uint8>(Left.CallableKind)
						< static_cast<uint8>(Right.CallableKind);
				}
				return Left.EngineLocalFunctionId
					< Right.EngineLocalFunctionId;
			});

		return Snapshot.NativeCallInventory.Num()
			== GenerationProvenance->Functions.Num();
	}

	static bool IsAutomaticVMScalarBridgeCandidate(
		const FAngelscriptStaticJITNativeCallInventoryEntry& Entry,
		const asCScriptFunction& Function,
		const FAngelscriptTypedASTJITVMBridgeABIIdentity& BridgeABI)
	{
		return !Entry.bHasExternalDescriptor
			&& Entry.ExternalDescriptor.IsAbsent()
			&& Entry.CallableKind
				== EAngelscriptStaticJITInstalledCallableKind::Global
			&& Function.objectType == nullptr
			&& Function.returnType.GetTokenType() != ttVoid
			&& Entry.DispatchKind
				== EAngelscriptStaticJITInstalledDispatchKind::FunctionCaller
			&& Entry.bHasSystemCaller
			&& Entry.CompileOutKind
				== EAngelscriptStaticJITInstalledCompileOutKind::CompileCalls
			&& Entry.DefaultArgumentCount == 0
			&& Entry.HiddenArgumentIndex < 0
			&& Entry.DeterminesOutputTypeArgumentIndex < 0
			&& Entry.FirstParamMetadata
				== EAngelscriptStaticJITInstalledFirstParamMetadata::None
			&& !Entry.bReturnsOnStack
			&& !Entry.bUsesWorldContext
			&& Entry.bHasStableReference
			&& Entry.StableReference.IsValid()
			&& Entry.StableReference.Kind
				== EAngelscriptArtifactReferenceKind::EnvironmentSymbol
			&& !Entry.StableReference.ExpectedAbi.IsZero()
			&& Entry.bHasExpectedNativeABI
			&& Entry.ExpectedNativeABI.IsValid()
			&& BridgeABI.IsValid();
	}

	static bool IsReviewedCurrentNativeCandidate(
		const asCScriptFunction& Function,
		const FAngelscriptStaticJITNativeCallDescriptor& Descriptor)
	{
		if (Descriptor.Linkage
				!= EAngelscriptStaticJITNativeCallLinkage::ProviderPrivate
			|| Descriptor.ABIDomain
				!= EAngelscriptStaticJITNativeCallABIDomain::Scalar
			|| Descriptor.ExceptionPolicy
				!= EAngelscriptStaticJITNativeCallExceptionPolicy::
					DoesNotSetScriptException
			|| !EnumHasAnyFlags(
				Descriptor.RouteFlags,
				EAngelscriptStaticJITNativeCallRouteFlags::CurrentNativeCall)
			|| Function.GetFuncType() != asFUNC_SYSTEM
			|| Function.objectType != nullptr
			|| Function.sysFuncIntf == nullptr
			|| Function.sysFuncIntf->func == nullptr
			|| (Function.sysFuncIntf->callConv != ICC_CDECL
				&& Function.sysFuncIntf->callConv != ICC_STDCALL)
			|| Function.sysFuncIntf->passFirstParamMetaData
				!= asEFirstParamMetaData::None
			|| Function.DoesReturnOnStack()
			|| Function.hiddenArgumentIndex >= 0
			|| Function.determinesOutputTypeArgumentIndex >= 0
			|| Function.traits.GetTrait(
				asEFuncTrait::asTRAIT_USES_WORLDCONTEXT)
			|| Function.compileOutType != asECompileOutType::CompileCalls)
		{
			return false;
		}

		for (asUINT Index = 0;
			Index < Function.defaultArgs.GetLength(); ++Index)
		{
			if (Function.defaultArgs[Index] != nullptr)
			{
				return false;
			}
		}
		return true;
	}

	static void CaptureNativeCallTargets(
		FAngelscriptStaticJITGenerationSnapshot& Snapshot,
		asCScriptEngine& ScriptEngine,
		const asCTypedSemanticFunction& HIR)
	{
		for (asUINT ExpressionIndex = 0;
			ExpressionIndex < HIR.expressions.GetLength(); ++ExpressionIndex)
		{
			const asSTypedSemanticExpression& Expression =
				HIR.expressions[ExpressionIndex];
			if (Expression.kind
					!= asETypedSemanticExpressionKind::ResolvedCall
				|| Expression.callTargetKind
					!= asETypedSemanticCallTargetKind::SystemFunction
				|| Expression.resolvedFunctionId < 0
				|| Snapshot.NativeCallTargets.ContainsByPredicate(
					[FunctionId = Expression.resolvedFunctionId](
						const FAngelscriptStaticJITNativeCallTarget& Candidate)
					{
						return Candidate.EngineLocalFunctionId == FunctionId;
					}))
			{
				continue;
			}

			asIScriptFunction* const TargetFunction =
				ScriptEngine.GetFunctionById(Expression.resolvedFunctionId);
			const FAngelscriptStaticJITNativeCallInventoryEntry* const
				InventoryEntry = Snapshot.NativeCallInventory.FindByPredicate(
					[FunctionId = Expression.resolvedFunctionId](
						const auto& Candidate)
					{
						return Candidate.EngineLocalFunctionId == FunctionId;
					});
			const FAngelscriptStaticJITNativeCallDescriptor* const Descriptor =
				FAngelscriptStaticJITNativeCallRegistry::Find(TargetFunction);
			if (TargetFunction == nullptr
				|| TargetFunction->GetEngine() != &ScriptEngine
				|| TargetFunction->GetFuncType() != asFUNC_SYSTEM)
			{
				continue;
			}

			FAngelscriptTypedASTJITVMBridgeABIIdentity ExpectedVMBridgeABI;
			const bool bHasVMBridgeABI =
				TryBuildAngelscriptTypedASTJITVMBridgeABIIdentity(
					*TargetFunction, ExpectedVMBridgeABI);
			const auto& ScriptFunction =
				*static_cast<asCScriptFunction*>(TargetFunction);
			if (Descriptor == nullptr)
			{
				if (InventoryEntry == nullptr
					|| !IsAutomaticVMScalarBridgeCandidate(
						*InventoryEntry, ScriptFunction,
						ExpectedVMBridgeABI))
				{
					continue;
				}

				FAngelscriptStaticJITNativeCallTarget& Added =
					Snapshot.NativeCallTargets.Emplace_GetRef();
				Added.EngineLocalFunctionId = Expression.resolvedFunctionId;
				Added.CanonicalDeclaration =
					InventoryEntry->CanonicalDeclaration;
				Added.Reference = InventoryEntry->StableReference;
				Added.ExpectedNativeABI = InventoryEntry->ExpectedNativeABI;
				Added.ExpectedVMBridgeABI = MoveTemp(ExpectedVMBridgeABI);
				Added.RegisteredTargetDisplay =
					InventoryEntry->NativeCallableDisplay.IsEmpty()
						? InventoryEntry->CanonicalDeclaration
						: InventoryEntry->NativeCallableDisplay;
				Added.bScalarBridgeCallable = true;
				continue;
			}

			FAngelscriptStaticJITNativeABIIdentity ExpectedNativeABI;
			FAngelscriptStaticJITScalarABIIdentity ExpectedScalarABI;
			FAngelscriptArtifactReference StableReference;
			const bool bRequiresScalarABI = Descriptor->ABIDomain
				== EAngelscriptStaticJITNativeCallABIDomain::Scalar;
			const bool bUsesVMScalarBridgeABI = Descriptor->ABIDomain
				== EAngelscriptStaticJITNativeCallABIDomain::VMScalarBridge;
			const bool bDescriptorBridge = EnumHasAnyFlags(
				Descriptor->RouteFlags,
				EAngelscriptStaticJITNativeCallRouteFlags::ScalarBridge);
			const bool bDescriptorCurrentNative = EnumHasAnyFlags(
				Descriptor->RouteFlags,
				EAngelscriptStaticJITNativeCallRouteFlags::CurrentNativeCall);
			if (!TryBuildAngelscriptStaticJITNativeABIIdentity(
					*TargetFunction, ExpectedNativeABI)
				|| (bRequiresScalarABI
					&& !TryBuildAngelscriptStaticJITScalarABIIdentity(
						*TargetFunction, ExpectedScalarABI))
				|| (!bRequiresScalarABI && !bUsesVMScalarBridgeABI
					&& Descriptor->ABIDomain
						!= EAngelscriptStaticJITNativeCallABIDomain::BorrowedManagedPOD)
				|| (bDescriptorBridge && !bHasVMBridgeABI)
				|| (bDescriptorCurrentNative
					&& !IsReviewedCurrentNativeCandidate(
						ScriptFunction, *Descriptor))
				|| !FAngelscriptCacheEnvironmentIdentity::
					TryBuildFunctionReference(ScriptFunction, StableReference)
				|| StableReference.Kind
					!= EAngelscriptArtifactReferenceKind::EnvironmentSymbol)
			{
				// An explicit descriptor never broadens the reviewed ABI domain.
				// Unsupported shapes remain absent so CallClosure fails the exact
				// call closed instead of persisting an Engine-local approximation.
				continue;
			}

			FAngelscriptStaticJITNativeCallTarget& Added =
				Snapshot.NativeCallTargets.Emplace_GetRef();
			Added.EngineLocalFunctionId = Expression.resolvedFunctionId;
			Added.CanonicalDeclaration = UTF8_TO_TCHAR(
				TargetFunction->GetDeclaration(false, false, false));
			Added.Reference = StableReference;
			Added.ExpectedNativeABI = MoveTemp(ExpectedNativeABI);
			Added.ExpectedScalarABI = MoveTemp(ExpectedScalarABI);
			Added.ExpectedVMBridgeABI = MoveTemp(ExpectedVMBridgeABI);
			Added.RegisteredTargetDisplay = InventoryEntry != nullptr
				&& !InventoryEntry->NativeCallableDisplay.IsEmpty()
					? InventoryEntry->NativeCallableDisplay
					: Descriptor->Symbol;
			Added.bCurrentNativeCallable = bDescriptorCurrentNative;
			Added.bScalarBridgeCallable = bDescriptorBridge;
			Added.Descriptor = *Descriptor;
		}
	}

	static const asCObjectProperty* FindDescriptorProperty(
		const asCObjectType& OwnerType,
		const FAngelscriptPropertyDesc& Descriptor)
	{
		auto HasDescriptorName = [&Descriptor](
			const asCObjectProperty* Candidate)
		{
			return Candidate != nullptr
				&& Descriptor.PropertyName.Equals(
					UTF8_TO_TCHAR(Candidate->name.AddressOf()),
					ESearchCase::CaseSensitive);
		};

		if (Descriptor.ScriptPropertyIndex >= 0
			&& Descriptor.ScriptPropertyIndex
				< static_cast<int32>(OwnerType.properties.GetLength()))
		{
			const asCObjectProperty* const Indexed =
				OwnerType.properties[Descriptor.ScriptPropertyIndex];
			if (HasDescriptorName(Indexed))
			{
				return Indexed;
			}
		}

		const asCObjectProperty* Match = nullptr;
		for (asUINT Index = 0; Index < OwnerType.properties.GetLength(); ++Index)
		{
			const asCObjectProperty* const Candidate = OwnerType.properties[Index];
			if (!HasDescriptorName(Candidate))
			{
				continue;
			}
			if (Match != nullptr)
			{
				return nullptr;
			}
			Match = Candidate;
		}
		return Match;
	}

	static void AddType(
		FAngelscriptStaticJITGenerationSnapshot& Snapshot,
		const FAngelscriptStableModuleKey& ModuleKey,
		asITypeInfo* Type)
	{
		if (Type == nullptr || FindType(Snapshot, Type) != nullptr)
		{
			return;
		}
		FAngelscriptStableTypeKey TypeKey;
		if (!FAngelscriptCacheStableSymbolIdentity::TryBuildLocalTypeKey(
			ModuleKey, *static_cast<asCTypeInfo*>(Type), TypeKey))
		{
			return;
		}
		FAngelscriptStaticJITGenerationType& Added =
			Snapshot.Types.Emplace_GetRef();
		Added.EngineLocalType = Type;
		Added.ModuleKey = ModuleKey;
		Added.TypeKey = TypeKey;
		Added.CanonicalDeclaration = UTF8_TO_TCHAR(
			Type->GetEngine()->GetTypeDeclaration(Type->GetTypeId(), true));
	}

	static void AddDescriptor(
		FAngelscriptStaticJITGenerationSnapshot& Snapshot,
		const void* EngineLocalDescriptor,
		const EAngelscriptStaticJITGenerationDescriptorKind Kind,
		const FAngelscriptHash256& StableDescriptorKey,
		FString CanonicalName,
		FString CanonicalDeclaration,
		FString ReceiverCanonicalName,
		const bool bUFunctionRoot,
		const bool bAppendGeneratedWorldContext,
		const FAngelscriptStaticJITGenerationFunction* FunctionPlan)
	{
		FAngelscriptStaticJITGenerationDescriptor& Descriptor =
			Snapshot.Descriptors.Emplace_GetRef();
		Descriptor.EngineLocalDescriptor = EngineLocalDescriptor;
		Descriptor.Kind = Kind;
		Descriptor.StableDescriptorKey = StableDescriptorKey;
		Descriptor.CanonicalName = MoveTemp(CanonicalName);
		Descriptor.CanonicalDeclaration = MoveTemp(CanonicalDeclaration);
		Descriptor.ReceiverCanonicalName = MoveTemp(ReceiverCanonicalName);
		Descriptor.bUFunctionRoot = bUFunctionRoot;
		if (Kind == EAngelscriptStaticJITGenerationDescriptorKind::Function
			&& FunctionPlan != nullptr)
		{
			Descriptor.EntryPlan = FunctionPlan->EntryPlan;
			if (bAppendGeneratedWorldContext)
			{
				Descriptor.EntryPlan.ReflectedParmsLayout =
					EAngelscriptStaticJITReflectedParmsLayout::
						AppendGeneratedWorldContext;
				FinalizeAngelscriptStaticJITEntryPlan(Descriptor.EntryPlan);
			}
		}
	}
}

bool ValidateAngelscriptStaticJITInstalledNativeCallDispatch(
	const FAngelscriptStaticJITNativeCallInventoryEntry& Entry,
	FString* OutError)
{
	auto Fail = [OutError](const FString& Error)
	{
		if (OutError != nullptr)
		{
			*OutError = Error;
		}
		return false;
	};
	if (OutError != nullptr)
	{
		OutError->Reset();
	}

	if (Entry.DispatchKind
			== EAngelscriptStaticJITInstalledDispatchKind::Unknown)
	{
		return Fail(FString::Printf(TEXT(
			"InstalledDispatchUnknown: Declaration=%s"),
			*Entry.CanonicalDeclaration));
	}

	const bool bFunctionCaller = Entry.DispatchKind
		== EAngelscriptStaticJITInstalledDispatchKind::FunctionCaller;
	if (bFunctionCaller != Entry.bHasSystemCaller)
	{
		return Fail(FString::Printf(TEXT(
			"InstalledDispatchCallerMismatch: Declaration=%s Dispatch=%u HasSystemCaller=%s"),
			*Entry.CanonicalDeclaration,
			static_cast<uint8>(Entry.DispatchKind),
			Entry.bHasSystemCaller ? TEXT("true") : TEXT("false")));
	}

	if (!Entry.bHasExternalDescriptor)
	{
		if (!Entry.ExternalDescriptor.IsAbsent())
		{
			return Fail(FString::Printf(TEXT(
				"InstalledDescriptorPresenceMismatch: Declaration=%s"),
				*Entry.CanonicalDeclaration));
		}
		return true;
	}
	if (Entry.ExternalDescriptor.IsAbsent())
	{
		return Fail(FString::Printf(TEXT(
			"InstalledDescriptorPresenceMismatch: Declaration=%s"),
			*Entry.CanonicalDeclaration));
	}

	const bool bDirectRoute = EnumHasAnyFlags(
		Entry.ExternalDescriptor.RouteFlags,
		EAngelscriptStaticJITNativeCallRouteFlags::DirectCall);
	const bool bCurrentNativeRoute = EnumHasAnyFlags(
		Entry.ExternalDescriptor.RouteFlags,
		EAngelscriptStaticJITNativeCallRouteFlags::CurrentNativeCall);
	const bool bDirectLinkage = Entry.ExternalDescriptor.Linkage
		== EAngelscriptStaticJITNativeCallLinkage::ExportedSymbol
		|| Entry.ExternalDescriptor.Linkage
			== EAngelscriptStaticJITNativeCallLinkage::HeaderInline
		|| Entry.ExternalDescriptor.Linkage
			== EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeThunk
		|| Entry.ExternalDescriptor.Linkage
			== EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeCallable;
	if (bDirectRoute != bDirectLinkage)
	{
		return Fail(FString::Printf(TEXT(
			"InstalledDescriptorDirectRouteMismatch: Declaration=%s Linkage=%u DirectRoute=%s"),
			*Entry.CanonicalDeclaration,
			static_cast<uint8>(Entry.ExternalDescriptor.Linkage),
			bDirectRoute ? TEXT("true") : TEXT("false")));
	}
	if (bCurrentNativeRoute
		&& Entry.ExternalDescriptor.Linkage
			!= EAngelscriptStaticJITNativeCallLinkage::ProviderPrivate)
	{
		return Fail(FString::Printf(TEXT(
			"InstalledDescriptorCurrentNativeLinkageMismatch: Declaration=%s Linkage=%u"),
			*Entry.CanonicalDeclaration,
			static_cast<uint8>(Entry.ExternalDescriptor.Linkage)));
	}

	const bool bRawNativeRoute = bDirectRoute || bCurrentNativeRoute;
	if (bRawNativeRoute
		&& (Entry.DispatchKind
				== EAngelscriptStaticJITInstalledDispatchKind::GenericFunction
			|| Entry.DispatchKind
				== EAngelscriptStaticJITInstalledDispatchKind::GenericMethod))
	{
		return Fail(FString::Printf(TEXT(
			"DirectDescriptorUsesGenericDispatch: Declaration=%s Symbol=%s Dispatch=%u"),
			*Entry.CanonicalDeclaration,
			*Entry.ExternalDescriptor.Symbol,
			static_cast<uint8>(Entry.DispatchKind)));
	}

	if (bCurrentNativeRoute
		&& (Entry.CallableKind
				!= EAngelscriptStaticJITInstalledCallableKind::Global
			|| (Entry.RegisteredCallConv != ICC_CDECL
				&& Entry.RegisteredCallConv != ICC_STDCALL)
			|| Entry.CompileOutKind
				!= EAngelscriptStaticJITInstalledCompileOutKind::CompileCalls
			|| Entry.DefaultArgumentCount != 0
			|| Entry.HiddenArgumentIndex >= 0
			|| Entry.DeterminesOutputTypeArgumentIndex >= 0
			|| Entry.FirstParamMetadata
				!= EAngelscriptStaticJITInstalledFirstParamMetadata::None
			|| Entry.bReturnsOnStack
			|| Entry.bUsesWorldContext
			|| Entry.ExternalDescriptor.ABIDomain
				!= EAngelscriptStaticJITNativeCallABIDomain::Scalar
			|| Entry.ExternalDescriptor.ExceptionPolicy
				!= EAngelscriptStaticJITNativeCallExceptionPolicy::
					DoesNotSetScriptException))
	{
		return Fail(FString::Printf(TEXT(
			"CurrentNativeDescriptorUsesUnsupportedInstalledABI: Declaration=%s CallConv=%u CallableKind=%u Dispatch=%u"),
			*Entry.CanonicalDeclaration,
			Entry.RegisteredCallConv,
			static_cast<uint8>(Entry.CallableKind),
			static_cast<uint8>(Entry.DispatchKind)));
	}

	if (Entry.NativeUFunctionRouteFlags
			!= EAngelscriptNativeUFunctionRouteFlags::None
		&& (!Entry.bHasNativeForm
			|| Entry.NativeFormKind != EAngelscriptNativeFormKind::UFunction))
	{
		return Fail(FString::Printf(TEXT(
			"NativeUFunctionRouteFactsWithoutUFunction: Declaration=%s RouteFlags=%u"),
			*Entry.CanonicalDeclaration,
			static_cast<uint8>(Entry.NativeUFunctionRouteFlags)));
	}

	if (bRawNativeRoute
		&& Entry.NativeFormKind == EAngelscriptNativeFormKind::UFunction
		&& Entry.NativeUFunctionRouteFlags
			!= EAngelscriptNativeUFunctionRouteFlags::None)
	{
		return Fail(FString::Printf(TEXT(
			"DirectDescriptorBypassesUnrealRoute: Declaration=%s Symbol=%s RouteFlags=%u"),
			*Entry.CanonicalDeclaration,
			*Entry.ExternalDescriptor.Symbol,
			static_cast<uint8>(Entry.NativeUFunctionRouteFlags)));
	}

	return true;
}

bool BuildAngelscriptStaticJITGenerationSnapshot(
	FAngelscriptEngine& Engine,
	const TConstArrayView<TSharedRef<FAngelscriptModuleDesc>> Modules,
	const TConstArrayView<FAngelscriptCacheCleanModuleArtifacts> Artifacts,
	const FAngelscriptArtifactProfileKey& ArtifactProfile,
	TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
		ESPMode::ThreadSafe>& OutSnapshot,
	FString* OutError)
{
	using namespace AngelscriptStaticJITGenerationSnapshot_Private;
	OutSnapshot.Reset();
	if (Engine.GetPurpose()
			!= EAngelscriptEnginePurpose::StaticJITGeneration
		|| Modules.IsEmpty()
		|| ArtifactProfile.Hash.IsZero())
	{
		return Fail(OutError,
			TEXT("StaticJIT generation snapshot requires a generation Engine, compiled modules, and Profile."));
	}

	TSharedRef<FAngelscriptStaticJITGenerationSnapshot,
		ESPMode::ThreadSafe> Snapshot =
		MakeShared<FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe>();
	Snapshot->ArtifactProfile = ArtifactProfile;
	Snapshot->SourceCompilePassCount =
		Engine.GetStaticJITGenerationCompilePassCount();
	asCScriptEngine* const ScriptEngine =
		static_cast<asCScriptEngine*>(Engine.GetScriptEngine());
	if (ScriptEngine == nullptr
		|| !CaptureInstalledNativeCallInventory(
			*Snapshot, Engine, *ScriptEngine, OutError))
	{
		return false;
	}
	const bool bCaptureTypedHIR = ScriptEngine != nullptr
		&& ScriptEngine->IsTypedSemanticIRCaptureEnabled();
	Snapshot->CaptureProfile = bCaptureTypedHIR
		? EAngelscriptStaticJITCaptureProfile::VerifiedTypedHIR
		: EAngelscriptStaticJITCaptureProfile::Bytecode;
	Snapshot->bHasVerifiedTypedHIR = bCaptureTypedHIR;
#if WITH_DEV_AUTOMATION_TESTS
	bool bInjectedInvalidTypedHIR = false;
#endif
	for (const FAngelscriptCacheCleanModuleArtifacts& ModuleArtifacts
		: Artifacts)
	{
		Snapshot->InputCacheRecordCount += ModuleArtifacts.Records.Num();
	}

	for (const TSharedRef<FAngelscriptModuleDesc>& ModuleDesc : Modules)
	{
		asCModule* const ScriptModule = ModuleDesc->ScriptModule;
		if (ScriptModule == nullptr || ModuleDesc->bCompileError)
		{
			return Fail(OutError, FString::Printf(
				TEXT("StaticJIT generation module '%s' is not compiled."),
				*ModuleDesc->ModuleName));
		}
		FAngelscriptStableModuleKey ModuleKey;
		FString ModuleKeyError;
		if (!FAngelscriptCacheStableSymbolIdentity::TryBuildModuleKey(
			*ModuleDesc, ModuleKey, &ModuleKeyError))
		{
			return Fail(OutError, ModuleKeyError);
		}
		const FAngelscriptCacheCleanModuleArtifacts* ModuleArtifacts =
			FindArtifacts(Artifacts, ModuleKey);

		FAngelscriptStaticJITGenerationModule& Module =
			Snapshot->Modules.Emplace_GetRef();
		Module.EngineLocalModule = ScriptModule;
		Module.EngineLocalDescriptor = &ModuleDesc.Get();
		Module.ModuleKey = ModuleKey;
		Module.CanonicalModuleName = ModuleArtifacts != nullptr
			? ModuleArtifacts->CanonicalModuleName : ModuleDesc->ModuleName;
		if (ModuleDesc->Code.Num() == 1)
		{
			Module.VirtualSourcePath = ModuleDesc->Code[0].VirtualPath;
		}

		for (asUINT TypeIndex = 0;
			TypeIndex < ScriptModule->GetObjectTypeCount(); ++TypeIndex)
		{
			AddType(*Snapshot, ModuleKey,
				ScriptModule->GetObjectTypeByIndex(TypeIndex));
		}
		for (asUINT TypeIndex = 0;
			TypeIndex < ScriptModule->GetEnumCount(); ++TypeIndex)
		{
			AddType(*Snapshot, ModuleKey,
				ScriptModule->GetEnumByIndex(TypeIndex));
		}
		for (asUINT TypeIndex = 0;
			TypeIndex < ScriptModule->GetTypedefCount(); ++TypeIndex)
		{
			AddType(*Snapshot, ModuleKey,
				ScriptModule->GetTypedefByIndex(TypeIndex));
		}
		for (asUINT TypeIndex = 0;
			TypeIndex < ScriptModule->funcDefs.GetLength(); ++TypeIndex)
		{
			AddType(*Snapshot, ModuleKey,
				ScriptModule->funcDefs[TypeIndex]);
		}

		for (asUINT GlobalIndex = 0;
			GlobalIndex < ScriptModule->GetGlobalVarCount(); ++GlobalIndex)
		{
			const char* Name = nullptr;
			const char* Namespace = nullptr;
			int TypeId = 0;
			if (ScriptModule->GetGlobalVar(GlobalIndex, &Name, &Namespace,
				&TypeId, nullptr) < 0 || Name == nullptr
				|| GlobalIndex >= ScriptModule->scriptGlobalsList.GetLength()
				|| ScriptModule->scriptGlobalsList[GlobalIndex] == nullptr)
			{
				continue;
			}
			const asCGlobalProperty* const GlobalProperty =
				ScriptModule->scriptGlobalsList[GlobalIndex];
			FAngelscriptGlobalIdentityDescriptor Identity;
			Identity.ModuleKey = ModuleKey;
			Identity.Namespace = Namespace != nullptr
				? UTF8_TO_TCHAR(Namespace) : FString();
			Identity.Name = UTF8_TO_TCHAR(Name);
			Identity.CanonicalType = UTF8_TO_TCHAR(
				ScriptModule->GetEngine()->GetTypeDeclaration(TypeId, true));
			FAngelscriptStaticJITGenerationGlobal& Global =
				Snapshot->Globals.Emplace_GetRef();
			Global.EngineLocalGlobal =
				ScriptModule->GetAddressOfGlobalVar(GlobalIndex);
			Global.EngineLocalGlobalPropertyId = GlobalProperty->id;
			Global.ModuleKey = ModuleKey;
			Global.GlobalKey =
				FAngelscriptArtifactIdentityBuilder::BuildGlobalKey(Identity);
			Global.CanonicalDeclaration = UTF8_TO_TCHAR(
				ScriptModule->GetGlobalVarDeclaration(GlobalIndex, true));
		}

		for (asUINT FunctionIndex = 0;
			FunctionIndex < ScriptModule->scriptFunctions.GetLength();
			++FunctionIndex)
		{
			asCScriptFunction* const Function =
				ScriptModule->scriptFunctions[FunctionIndex];
			if (Function == nullptr || Function->module != ScriptModule)
			{
				continue;
			}
			FAngelscriptStableFunctionKey FunctionKey;
			if (!FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				ModuleKey, *Function, FunctionKey))
			{
				continue;
			}
			const FAngelscriptFunctionArtifactIdentity* FunctionIdentity =
				ModuleArtifacts != nullptr
				? ModuleArtifacts->ValidatedFunctionArtifactIdentities.
					FindByPredicate(
						[&FunctionKey](const auto& Candidate)
						{
							return Candidate.FunctionKey == FunctionKey;
						})
				: nullptr;
			FAngelscriptStaticJITGenerationFunction& Added =
				Snapshot->Functions.Emplace_GetRef();
			Added.EngineLocalFunction = Function;
			Added.ModuleKey = ModuleKey;
			Added.FunctionKey = FunctionKey;
			Added.CanonicalDeclaration = UTF8_TO_TCHAR(
				Function->GetDeclaration(false, false, false));
			if (bCaptureTypedHIR)
			{
				Added.VerifiedTypedHIR =
					Function->GetTypedSemanticFunction();
#if WITH_DEV_AUTOMATION_TESTS
				if (Added.VerifiedTypedHIR != nullptr
					&& !bInjectedInvalidTypedHIR
					&& Engine.GetRuntimeConfig().
						bInjectInvalidTypedSemanticIRForTesting)
				{
					asCTypedSemanticFunction* const MutableHIR =
						const_cast<asCTypedSemanticFunction*>(
							Added.VerifiedTypedHIR);
					MutableHIR->rootStatement =
						asTypedSemanticStatementId();
					bInjectedInvalidTypedHIR = true;
				}
#endif
				if (Added.VerifiedTypedHIR != nullptr
					&& !VerifyTypedSemanticFunction(
						*Added.VerifiedTypedHIR).IsValid())
				{
					return Fail(OutError, FString::Printf(
						TEXT("StaticJIT generation function '%s' published invalid typed HIR."),
						*Added.CanonicalDeclaration));
				}
				if (Added.VerifiedTypedHIR != nullptr)
				{
					CaptureNativeCallTargets(
						*Snapshot, *ScriptEngine, *Added.VerifiedTypedHIR);
				}
			}
			Added.bHasVMEntry = true;
			Added.bHasRawEntry = true;
			Added.bHasParmsEntry =
				CanGenerateAngelscriptStaticJITParmsEntry(Function);
			FString EntryPlanError;
			if (!BuildAngelscriptStaticJITEntryPlan(
					*Function,
					Added.bHasVMEntry,
					Added.bHasRawEntry,
					Added.bHasParmsEntry,
					Added.EntryPlan,
					&EntryPlanError))
			{
				return Fail(OutError, FString::Printf(TEXT(
					"StaticJIT generation function '%s' has no complete shared Entry Plan: %s"),
					*Added.CanonicalDeclaration,
					*EntryPlanError));
			}
			Added.EntryAbiHash =
				FAngelscriptJITGeneration::BuildEntryAbiHash(
					Added.CanonicalDeclaration,
					Added.bHasVMEntry,
					Added.bHasRawEntry,
					Added.bHasParmsEntry);
			if (Function->scriptData != nullptr
				&& Function->scriptData->byteCode.GetLength() > 0)
			{
				Added.Bytecode.Append(
					reinterpret_cast<const uint8*>(
						Function->scriptData->byteCode.AddressOf()),
					static_cast<int32>(
						Function->scriptData->byteCode.GetLength()
						* sizeof(asDWORD)));
			}
			if (FunctionIdentity != nullptr)
			{
				Added.ExecutionHash = FunctionIdentity->Content.Execution;
				Added.DebugHash = FunctionIdentity->Content.Debug;
				Added.bHasVerifiedArtifactIdentity = true;
			}
			else
			{
				const FAngelscriptFunctionContentHash Content =
					BuildGenerationOnlyContentHash(*Function);
				Added.ExecutionHash = Content.Execution;
				Added.DebugHash = Content.Debug;
			}
			const FAngelscriptFunctionArtifactReferenceSet* ReferenceSet =
				ModuleArtifacts != nullptr
				? ModuleArtifacts->ValidatedFunctionArtifactReferenceSets.
					FindByPredicate(
						[&FunctionKey](const auto& Candidate)
						{
							return Candidate.FunctionKey == FunctionKey;
						})
				: nullptr;
			if (ReferenceSet != nullptr)
			{
				Added.bHasVerifiedArtifactReferences = true;
				Added.RequiredReferences = ReferenceSet->RequiredReferences;
				for (const FAngelscriptArtifactReference& Reference
					: Added.RequiredReferences)
				{
					FAngelscriptStaticJITGenerationDependency Dependency;
					Dependency.OwnerFunctionKey = FunctionKey;
					Dependency.Reference = Reference;
					Snapshot->Dependencies.Add(Dependency);
					if (Reference.Kind
							== EAngelscriptArtifactReferenceKind::RuntimeHelper
						|| Reference.Kind
							== EAngelscriptArtifactReferenceKind::EnvironmentSymbol)
					{
						Snapshot->ExternalNativeCalls.Add(Dependency);
					}
				}
				for (const FAngelscriptArtifactSemanticDependency& Semantic
					: ReferenceSet->SemanticDependencies)
				{
					FAngelscriptStaticJITGenerationSemanticDependency&
						Dependency = Snapshot->SemanticDependencies.
							Emplace_GetRef();
					Dependency.OwnerFunctionKey = FunctionKey;
					Dependency.Kind = Semantic.Kind;
					Dependency.Reference = Semantic.Target;
					Dependency.ExpectedContentOrValue =
						Semantic.ExpectedContentOrValue;
				}
			}
		}
		if (!AssignLiteralAssetFunctionRoles(
				*Snapshot, ModuleDesc.Get(), ModuleKey, OutError))
		{
			return false;
		}

		for (const TSharedRef<FAngelscriptClassDesc>& ClassDesc
			: ModuleDesc->Classes)
		{
			const asCObjectType* const ObjectType =
				static_cast<const asCObjectType*>(ClassDesc->ScriptType);
			FAngelscriptStaticJITGenerationType* Type =
				FindType(*Snapshot, ClassDesc->ScriptType);
			if (Type != nullptr)
			{
				Type->bTargetMaterializesScriptReflection = true;
			}
			FAngelscriptHash256 TypeKey = Type != nullptr
				? Type->TypeKey.Hash : FAngelscriptHash256{};
			FString TypeDeclaration = Type != nullptr
				? Type->CanonicalDeclaration : ClassDesc->ClassName;
			if (Type == nullptr && ClassDesc->bIsStaticsClass)
			{
				// Module statics classes are reflection-only descriptors synthesized
				// for global UFUNCTIONs. They deliberately have no AngelScript
				// object type, so reproduce the same stable type identity used by
				// Cache clean capture instead of treating the missing ScriptType as
				// an incomplete generation graph.
				TypeDeclaration = FString::Printf(
					TEXT("class %s"), *ClassDesc->ClassName);
				FAngelscriptTypeIdentityDescriptor Identity;
				Identity.ModuleKey = ModuleKey;
				Identity.Namespace = ClassDesc->Namespace.Get(FString());
				Identity.Kind = EAngelscriptArtifactEntityKind::Class;
				Identity.CanonicalDeclaration = TypeDeclaration;
				TypeKey = FAngelscriptArtifactIdentityBuilder::BuildTypeKey(
					Identity).Hash;
			}
			AddDescriptor(*Snapshot, &ClassDesc.Get(),
				ClassDesc->bIsStruct
					? EAngelscriptStaticJITGenerationDescriptorKind::Struct
					: EAngelscriptStaticJITGenerationDescriptorKind::Class,
				TypeKey, ClassDesc->ClassName,
				MoveTemp(TypeDeclaration),
				FString(), false, false, nullptr);

			for (const TSharedRef<FAngelscriptPropertyDesc>& Property
				: ClassDesc->Properties)
			{
				const asCObjectProperty* const ScriptProperty =
					ObjectType != nullptr
						? FindDescriptorProperty(*ObjectType, Property.Get())
						: nullptr;
				if (Type == nullptr || ScriptProperty == nullptr)
				{
					return Fail(OutError, FString::Printf(TEXT(
						"StaticJIT generation property '%s::%s' has no exact generation-Engine type/property authority."),
						*ClassDesc->ClassName, *Property->PropertyName));
				}
				const asCString FormattedType = ScriptProperty->type.Format(
					ObjectType->nameSpace, false, false);
				const FString CanonicalType =
					UTF8_TO_TCHAR(FormattedType.AddressOf());
				if (CanonicalType.IsEmpty())
				{
					return Fail(OutError, FString::Printf(TEXT(
						"StaticJIT generation property '%s::%s' has no canonical VM type declaration."),
						*ClassDesc->ClassName, *Property->PropertyName));
				}
				FAngelscriptPropertyIdentityDescriptor Identity;
				Identity.OwnerTypeKey = Type->TypeKey;
				Identity.Kind = EAngelscriptArtifactEntityKind::Property;
				Identity.Name = Property->PropertyName;
				Identity.CanonicalType = CanonicalType;
				AddDescriptor(*Snapshot, &Property.Get(),
					EAngelscriptStaticJITGenerationDescriptorKind::Property,
					FAngelscriptArtifactIdentityBuilder::BuildPropertyKey(
						Identity).Hash,
					Property->PropertyName, CanonicalType,
					ClassDesc->ClassName, false, false, nullptr);
			}

			for (const TSharedRef<FAngelscriptFunctionDesc>& FunctionDesc
				: ClassDesc->Methods)
			{
				FAngelscriptStaticJITGenerationFunction* Function =
					FindFunction(*Snapshot, FunctionDesc->ScriptFunction);
				const FString Declaration = Function != nullptr
					? Function->CanonicalDeclaration
					: FunctionDesc->ScriptFunctionName;
				AddDescriptor(*Snapshot, &FunctionDesc.Get(),
					EAngelscriptStaticJITGenerationDescriptorKind::Function,
					Function != nullptr ? Function->FunctionKey.Hash
						: FAngelscriptHash256{},
					FunctionDesc->FunctionName, Declaration,
					ClassDesc->bIsStaticsClass ? FString()
						: ClassDesc->ClassName,
					true,
					ClassDesc->bIsStaticsClass
						&& FunctionDesc->bIsStatic
						&& [&FunctionDesc]()
						{
							const FString* const ExplicitWorldContext =
								FunctionDesc->Meta.Find(
									FName(TEXT("WorldContext")));
							return ExplicitWorldContext == nullptr
								|| ExplicitWorldContext->IsEmpty()
								|| !FunctionDesc->Arguments.ContainsByPredicate(
									[ExplicitWorldContext](
										const FAngelscriptArgumentDesc& Argument)
									{
										return Argument.ArgumentName
											== *ExplicitWorldContext;
									});
						}(),
					Function);
			}
		}

		for (const TSharedRef<FAngelscriptEnumDesc>& EnumDesc
			: ModuleDesc->Enums)
		{
			FAngelscriptStaticJITGenerationType* Type =
				FindType(*Snapshot, EnumDesc->ScriptType);
			AddDescriptor(*Snapshot, &EnumDesc.Get(),
				EAngelscriptStaticJITGenerationDescriptorKind::Enum,
				Type != nullptr ? Type->TypeKey.Hash
					: FAngelscriptHash256{},
				EnumDesc->EnumName,
				Type != nullptr ? Type->CanonicalDeclaration
					: EnumDesc->EnumName,
				FString(), false, false, nullptr);
		}

		for (const TSharedRef<FAngelscriptDelegateDesc>& DelegateDesc
			: ModuleDesc->Delegates)
		{
			// Delegate preprocessing synthesizes its callable wrapper type. It is
			// descriptor-owned and is not guaranteed to appear in the module's
			// ordinary object-type enumeration, so capture it explicitly.
			AddType(*Snapshot, ModuleKey, DelegateDesc->ScriptType);
			FAngelscriptStaticJITGenerationType* Type =
				FindType(*Snapshot, DelegateDesc->ScriptType);
			if (Type == nullptr && DelegateDesc->ScriptType != nullptr)
			{
				FAngelscriptTypeIdentityDescriptor TypeIdentity;
				TypeIdentity.ModuleKey = ModuleKey;
				TypeIdentity.Namespace = UTF8_TO_TCHAR(
					DelegateDesc->ScriptType->GetNamespace());
				TypeIdentity.Kind =
					EAngelscriptArtifactEntityKind::Delegate;
				TypeIdentity.CanonicalDeclaration = FString::Printf(
					TEXT("delegate %s"), *DelegateDesc->DelegateName);
				FAngelscriptStaticJITGenerationType& AddedType =
					Snapshot->Types.Emplace_GetRef();
				AddedType.EngineLocalType = DelegateDesc->ScriptType;
				AddedType.ModuleKey = ModuleKey;
				AddedType.TypeKey =
					FAngelscriptArtifactIdentityBuilder::BuildTypeKey(
						TypeIdentity);
				AddedType.CanonicalDeclaration =
					TypeIdentity.CanonicalDeclaration;
				AddedType.bTargetMaterializesScriptReflection = true;
				Type = &AddedType;
			}
			FAngelscriptTypeIdentityDescriptor DescriptorIdentity;
			DescriptorIdentity.ModuleKey = ModuleKey;
			DescriptorIdentity.Namespace = DelegateDesc->ScriptType != nullptr
				? UTF8_TO_TCHAR(DelegateDesc->ScriptType->GetNamespace())
				: FString();
			DescriptorIdentity.Kind =
				EAngelscriptArtifactEntityKind::Delegate;
			const FString DelegateDeclaration =
				DelegateDesc->Signature.IsValid()
				&& DelegateDesc->Signature->ScriptFunction != nullptr
				? UTF8_TO_TCHAR(DelegateDesc->Signature->ScriptFunction->
					GetDeclaration(false, false, false))
				: FString::Printf(
					TEXT("delegate %s"), *DelegateDesc->DelegateName);
			DescriptorIdentity.CanonicalDeclaration = DelegateDeclaration;
			AddDescriptor(*Snapshot, &DelegateDesc.Get(),
				EAngelscriptStaticJITGenerationDescriptorKind::Delegate,
				FAngelscriptArtifactIdentityBuilder::BuildTypeKey(
					DescriptorIdentity).Hash,
				DelegateDesc->DelegateName,
				DelegateDeclaration,
				FString(), false, false, nullptr);
		}
	}

#if WITH_DEV_AUTOMATION_TESTS
	if (Engine.GetRuntimeConfig().bInjectInvalidTypedSemanticIRForTesting
		&& !bInjectedInvalidTypedHIR)
	{
		return Fail(OutError, TEXT(
			"StaticJIT generation typed-HIR fault injection found no captured HIR to verify."));
	}
#endif

	Snapshot->Modules.Sort([](const auto& Left, const auto& Right)
	{
		return Left.ModuleKey.Hash < Right.ModuleKey.Hash;
	});
	Snapshot->Functions.Sort([](const auto& Left, const auto& Right)
	{
		return Left.FunctionKey.Hash < Right.FunctionKey.Hash;
	});
	Snapshot->Types.Sort([](const auto& Left, const auto& Right)
	{
		return Left.TypeKey.Hash < Right.TypeKey.Hash;
	});
	Snapshot->Globals.Sort([](const auto& Left, const auto& Right)
	{
		return Left.GlobalKey.Hash < Right.GlobalKey.Hash;
	});
	for (FAngelscriptStaticJITGenerationSemanticDependency& Dependency
		: Snapshot->SemanticDependencies)
	{
		if (Dependency.Reference.Kind
			!= EAngelscriptArtifactReferenceKind::ScriptGlobal)
		{
			continue;
		}
		const FAngelscriptStaticJITGenerationGlobal* Match = nullptr;
		bool bAmbiguous = false;
		for (const FAngelscriptStaticJITGenerationGlobal& Global
			: Snapshot->Globals)
		{
			if (Global.GlobalKey.Hash != Dependency.Reference.StableKey)
			{
				continue;
			}
			if (Match != nullptr)
			{
				bAmbiguous = true;
				break;
			}
			Match = &Global;
		}
		if (!bAmbiguous && Match != nullptr)
		{
			Dependency.EngineLocalGlobalPropertyId =
				Match->EngineLocalGlobalPropertyId;
		}
	}
	Snapshot->Dependencies.Sort([](const auto& Left, const auto& Right)
	{
		if (!(Left.OwnerFunctionKey == Right.OwnerFunctionKey))
		{
			return Left.OwnerFunctionKey.Hash < Right.OwnerFunctionKey.Hash;
		}
		return Left.Reference < Right.Reference;
	});
	Snapshot->ExternalNativeCalls.Sort([](const auto& Left, const auto& Right)
	{
		if (!(Left.OwnerFunctionKey == Right.OwnerFunctionKey))
		{
			return Left.OwnerFunctionKey.Hash < Right.OwnerFunctionKey.Hash;
		}
		return Left.Reference < Right.Reference;
	});
	Snapshot->SemanticDependencies.Sort([](const auto& Left, const auto& Right)
	{
		if (!(Left.OwnerFunctionKey == Right.OwnerFunctionKey))
		{
			return Left.OwnerFunctionKey.Hash
				< Right.OwnerFunctionKey.Hash;
		}
		FAngelscriptArtifactSemanticDependency LeftValue;
		LeftValue.Kind = Left.Kind;
		LeftValue.Target = Left.Reference;
		LeftValue.ExpectedContentOrValue = Left.ExpectedContentOrValue;
		FAngelscriptArtifactSemanticDependency RightValue;
		RightValue.Kind = Right.Kind;
		RightValue.Target = Right.Reference;
		RightValue.ExpectedContentOrValue = Right.ExpectedContentOrValue;
		return LeftValue < RightValue;
	});
	Snapshot->NativeCallTargets.Sort([](const auto& Left, const auto& Right)
	{
		return Left.EngineLocalFunctionId < Right.EngineLocalFunctionId;
	});
	Snapshot->bDescriptorAnalysisComplete = true;
	if (!Snapshot->IsComplete())
	{
		if (Snapshot->SourceCompilePassCount != 1)
		{
			return Fail(OutError, FString::Printf(TEXT(
				"StaticJIT generation requires exactly one authoritative source compile pass, observed %u."),
				Snapshot->SourceCompilePassCount));
		}
		for (const FAngelscriptStaticJITGenerationFunction& Function
			: Snapshot->Functions)
		{
			if (Function.EngineLocalFunction == nullptr
				|| Function.ModuleKey.Hash.IsZero()
				|| Function.FunctionKey.Hash.IsZero()
				|| Function.ExecutionHash.IsZero()
				|| Function.DebugHash.IsZero()
				|| Function.EntryAbiHash.IsZero()
				|| !Function.EntryPlan.IsStructurallyComplete())
			{
				return Fail(OutError, FString::Printf(TEXT(
					"StaticJIT generation function '%s' has incomplete stable identity or Entry ABI facts."),
					*Function.CanonicalDeclaration));
			}
		}
		for (const FAngelscriptStaticJITGenerationDescriptor& Descriptor
			: Snapshot->Descriptors)
		{
			if (Descriptor.StableDescriptorKey.IsZero()
				|| Descriptor.CanonicalName.IsEmpty()
				|| Descriptor.CanonicalDeclaration.IsEmpty()
				|| (Descriptor.Kind
						== EAngelscriptStaticJITGenerationDescriptorKind::Function
					&& !Descriptor.EntryPlan.IsStructurallyComplete()))
			{
				return Fail(OutError, FString::Printf(TEXT(
					"StaticJIT generation descriptor '%s' (kind=%u) has incomplete stable identity or entry-plan facts."),
					*Descriptor.CanonicalName,
					static_cast<uint32>(Descriptor.Kind)));
			}
		}
		return Fail(OutError,
			TEXT("StaticJIT generation snapshot was incomplete after descriptor capture."));
	}
	OutSnapshot = Snapshot;
	if (OutError != nullptr)
	{
		OutError->Reset();
	}
	return true;
}

#endif
