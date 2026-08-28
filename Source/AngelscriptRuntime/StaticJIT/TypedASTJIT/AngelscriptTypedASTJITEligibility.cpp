#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEligibility.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCanonical.h"

#include "Core/AngelscriptEngine.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_context.h"
#include "source/as_scriptfunction.h"
#include "source/as_typeinfo.h"
#include "EndAngelscriptHeaders.h"

#if AS_CAN_GENERATE_JIT

namespace AngelscriptTypedASTJITEligibility_Private
{
	static FAngelscriptTypedASTJITEligibility Reject(
		const EAngelscriptTypedASTJITFallbackReason Reason,
		const asSSourceSpan& Span,
		const FString& Detail)
	{
		FAngelscriptTypedASTJITEligibility Result;
		Result.Reason = Reason;
		Result.SourceSpan = Span;
		Result.Detail = Detail;
		return Result;
	}

	static FString UFunctionFlagDetail(
		const EAngelscriptTypedASTJITUFunctionFlags Flags)
	{
		TArray<FString> Tokens;
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITUFunctionFlags::RpcOrNet))
			Tokens.Add(TEXT("RpcOrNet"));
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITUFunctionFlags::BlueprintEvent))
			Tokens.Add(TEXT("BlueprintEvent"));
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITUFunctionFlags::BlueprintOverride))
			Tokens.Add(TEXT("BlueprintOverride"));
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITUFunctionFlags::ThreadSafeSpecial))
			Tokens.Add(TEXT("ThreadSafeSpecial"));
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITUFunctionFlags::WorldContextGenerated))
			Tokens.Add(TEXT("WorldContextGenerated"));
		return FString::Join(Tokens, TEXT("|"));
	}

	static FString SignatureFlagDetail(
		const EAngelscriptTypedASTJITSignatureFlags Flags)
	{
		TArray<FString> Tokens;
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITSignatureFlags::Reference))
			Tokens.Add(TEXT("Reference"));
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITSignatureFlags::OutParameter))
			Tokens.Add(TEXT("OutParameter"));
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITSignatureFlags::ReturnOnStack))
			Tokens.Add(TEXT("ReturnOnStack"));
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITSignatureFlags::Object))
			Tokens.Add(TEXT("Object"));
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITSignatureFlags::Struct))
			Tokens.Add(TEXT("Struct"));
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITSignatureFlags::Container))
			Tokens.Add(TEXT("Container"));
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITSignatureFlags::Delegate))
			Tokens.Add(TEXT("Delegate"));
		return FString::Join(Tokens, TEXT("|"));
	}

	static void ClassifyType(
		const asCDataType& Type,
		EAngelscriptTypedASTJITSignatureFlags& Flags)
	{
		if (Type.IsReference())
		{
			Flags |= EAngelscriptTypedASTJITSignatureFlags::Reference;
		}
		if (!Type.IsObject() && !Type.IsFuncdef())
		{
			return;
		}
		if (Type.IsFuncdef())
		{
			Flags |= EAngelscriptTypedASTJITSignatureFlags::Delegate;
		}
		else if (Type.IsArrayType()
			|| (Type.GetTypeInfo() != nullptr
				&& (Type.GetTypeInfo()->name == "TArray"
					|| Type.GetTypeInfo()->name == "TSet"
					|| Type.GetTypeInfo()->name == "TMap")))
		{
			Flags |= EAngelscriptTypedASTJITSignatureFlags::Container;
		}
		else if (Type.IsNonPrimitiveValueType())
		{
			Flags |= EAngelscriptTypedASTJITSignatureFlags::Struct;
		}
		else
		{
			Flags |= EAngelscriptTypedASTJITSignatureFlags::Object;
		}
	}

	static bool Fail(
		FAngelscriptTypedASTJITRootIndex& OutIndex,
		FString* OutError,
		const TCHAR* Detail)
	{
		OutIndex = {};
		if (OutError != nullptr)
		{
			*OutError = Detail;
		}
		return false;
	}
}

FString FAngelscriptTypedASTJITEligibility::ToDiagnosticString() const
{
	FString Result = FString::Printf(
		TEXT("%s: %s"), LexToString(Reason), *Detail);
	if (SourceSpan.section.GetLength() != 0)
	{
		Result += FString::Printf(TEXT(" [%s:%u:%u]"),
			UTF8_TO_TCHAR(SourceSpan.section.AddressOf()),
			SourceSpan.row,
			SourceSpan.column);
	}
	return Result;
}

FAngelscriptTypedASTJITEligibility EvaluateAngelscriptTypedASTJITEligibility(
	const FAngelscriptTypedASTJITEligibilityInput& Input)
{
	using namespace AngelscriptTypedASTJITEligibility_Private;
	if (Input.Purpose
			!= EAngelscriptTypedASTJITEligibilityPurpose::InternalHelperCallClosure
		&& !Input.bIsUFunctionRoot)
	{
		return Reject(EAngelscriptTypedASTJITFallbackReason::NotUFunctionRoot,
			{}, TEXT("No exact resolved FunctionDesc root exists."));
	}
	return EvaluateAngelscriptTypedASTJITEligibilityFromCanonical(Input);
}

FAngelscriptTypedASTJITEligibilityInput
BuildAngelscriptTypedASTJITEligibilityInput(
	const FAngelscriptTypedASTJITRoot* Root,
	const FAngelscriptStaticJITCompiledFunctionView& Function)
{
	using namespace AngelscriptTypedASTJITEligibility_Private;
	FAngelscriptTypedASTJITEligibilityInput Input;
	Input.bIsUFunctionRoot = Root != nullptr;
	Input.SealedAST = Function.SealedAST;
	Input.CanonicalFunctionDecl = asASTDeclId(Function.CanonicalFunctionDeclValue);
	if (Function.EngineLocalFunction == nullptr)
	{
		return Input;
	}

	const asCScriptFunction* const ScriptFunction =
		static_cast<const asCScriptFunction*>(Function.EngineLocalFunction);
	Input.RawTraitBits = ScriptFunction->traits.traits;
	Input.Shape.SymbolName = Function.FunctionKey.Hash.ToHexString();
	Input.Shape.ReturnType = ScriptFunction->returnType;
	Input.Shape.bReturnsOnStack = ScriptFunction->DoesReturnOnStack();
	for (asUINT Index = 0;
		Index < ScriptFunction->parameterTypes.GetLength(); ++Index)
	{
		Input.Shape.ParameterTypes.Add(ScriptFunction->parameterTypes[Index]);
	}
	Input.Shape.bHasNativeObjectSlot = ScriptFunction->objectType != nullptr;
	Input.Shape.InvocationKind = Input.Shape.bHasNativeObjectSlot
		? EAngelscriptTypedASTJITInvocationKind::InstanceMethod
		: EAngelscriptTypedASTJITInvocationKind::Global;
	Input.Shape.ReceiverKind = Input.Shape.bHasNativeObjectSlot
		? EAngelscriptTypedASTJITReceiverKind::NativeObjectThis
		: EAngelscriptTypedASTJITReceiverKind::None;
	Input.Shape.ReceiverParameterIndex = INDEX_NONE;
	Input.bVirtualOrNonFinal =
		ScriptFunction->funcType == asFUNC_VIRTUAL
		|| (ScriptFunction->objectType != nullptr
			&& !ScriptFunction->traits.GetTrait(asTRAIT_FINAL));

	if (Root != nullptr && Root->FunctionDescriptor != nullptr)
	{
		const FAngelscriptFunctionDesc& Desc = *Root->FunctionDescriptor;
		if (Desc.bNetMulticast || Desc.bNetClient || Desc.bNetServer
			|| Desc.bNetValidate || Desc.bUnreliable)
		{
			Input.UFunctionFlags |=
				EAngelscriptTypedASTJITUFunctionFlags::RpcOrNet;
		}
		if (Desc.bBlueprintEvent)
			Input.UFunctionFlags |= EAngelscriptTypedASTJITUFunctionFlags::BlueprintEvent;
		if (Desc.bBlueprintOverride)
			Input.UFunctionFlags |= EAngelscriptTypedASTJITUFunctionFlags::BlueprintOverride;
		if (Desc.bThreadSafe)
			Input.UFunctionFlags |= EAngelscriptTypedASTJITUFunctionFlags::ThreadSafeSpecial;
		if (Desc.Meta.Contains(FName(TEXT("WorldContext")))
			|| ScriptFunction->hiddenArgumentIndex >= 0
			|| ScriptFunction->traits.GetTrait(asTRAIT_USES_WORLDCONTEXT))
		{
			Input.UFunctionFlags |=
				EAngelscriptTypedASTJITUFunctionFlags::WorldContextGenerated;
		}
		for (const FAngelscriptArgumentDesc& Argument : Desc.Arguments)
		{
			if (Argument.bBlueprintOutRef || Argument.bBlueprintInRef)
				Input.SignatureFlags |= EAngelscriptTypedASTJITSignatureFlags::Reference;
			if (Argument.bBlueprintOutRef || Argument.bInRefForceCopyOut)
				Input.SignatureFlags |= EAngelscriptTypedASTJITSignatureFlags::OutParameter;
		}
	}
	if (ScriptFunction->DoesReturnOnStack())
		Input.SignatureFlags |= EAngelscriptTypedASTJITSignatureFlags::ReturnOnStack;
	ClassifyType(ScriptFunction->returnType, Input.SignatureFlags);
	for (asUINT Index = 0;
		Index < ScriptFunction->parameterTypes.GetLength(); ++Index)
	{
		ClassifyType(ScriptFunction->parameterTypes[Index], Input.SignatureFlags);
		if (Index < ScriptFunction->inOutFlags.GetLength()
			&& ScriptFunction->inOutFlags[Index] != asTM_NONE)
		{
			Input.SignatureFlags |= EAngelscriptTypedASTJITSignatureFlags::Reference;
			if (ScriptFunction->inOutFlags[Index] == asTM_OUTREF
				|| ScriptFunction->inOutFlags[Index] == asTM_INOUTREF)
			{
				Input.SignatureFlags |= EAngelscriptTypedASTJITSignatureFlags::OutParameter;
			}
		}
	}
	Input.CapabilityDetail = UFunctionFlagDetail(Input.UFunctionFlags);
	return Input;
}

const FAngelscriptTypedASTJITRoot*
FAngelscriptTypedASTJITRootIndex::FindByFunction(
	const asIScriptFunction* Function) const
{
	return Roots.FindByPredicate(
		[Function](const FAngelscriptTypedASTJITRoot& Root)
		{
			return Root.EngineLocalFunction == Function;
		});
}

bool BuildAngelscriptTypedASTJITRootIndex(
	const FAngelscriptStaticJITCompiledSourceGraph& Graph,
	FAngelscriptTypedASTJITRootIndex& OutIndex,
	FString* OutError)
{
	using namespace AngelscriptTypedASTJITEligibility_Private;
	OutIndex = {};
	if (OutError != nullptr)
	{
		OutError->Reset();
	}

	if (Graph.ResolvedModuleDescriptors.IsEmpty()
		|| Graph.ResolvedModuleDescriptors.Num() != Graph.Modules.Num())
	{
		return Fail(OutIndex, OutError, TEXT(
			"TypedASTJITDescriptorGraphMissing: the complete generation view must carry one resolved ModuleDesc for every compiled module."));
	}

	TMap<const asIScriptFunction*,
		const FAngelscriptStaticJITCompiledFunctionView*> FunctionsByPointer;
	FunctionsByPointer.Reserve(Graph.Functions.Num());
	for (const FAngelscriptStaticJITCompiledFunctionView& Function
		: Graph.Functions)
	{
		if (Function.EngineLocalFunction == nullptr
			|| Function.FunctionKey.Hash.IsZero()
			|| FunctionsByPointer.Contains(Function.EngineLocalFunction))
		{
			return Fail(OutIndex, OutError, TEXT(
				"TypedASTJITCompiledFunctionGraphInvalid: compiled functions require unique non-null task-local pointers and stable keys."));
		}
		FunctionsByPointer.Add(Function.EngineLocalFunction, &Function);
	}

	TMap<const void*, const FAngelscriptStaticJITDescriptorView*>
		DescriptorsByPointer;
	DescriptorsByPointer.Reserve(Graph.Descriptors.Num());
	for (const FAngelscriptStaticJITDescriptorView& Descriptor
		: Graph.Descriptors)
	{
		if (Descriptor.EngineLocalDescriptor == nullptr
			|| Descriptor.StableDescriptorKey.IsZero()
			|| DescriptorsByPointer.Contains(Descriptor.EngineLocalDescriptor))
		{
			return Fail(OutIndex, OutError, TEXT(
				"TypedASTJITDescriptorGraphInvalid: descriptors require unique non-null task-local pointers and stable keys."));
		}
		DescriptorsByPointer.Add(Descriptor.EngineLocalDescriptor, &Descriptor);
	}

	TSet<const FAngelscriptModuleDesc*> SeenModules;
	TSet<const FAngelscriptClassDesc*> SeenClasses;
	TSet<const FAngelscriptFunctionDesc*> SeenFunctionDescriptors;
	TSet<const asIScriptFunction*> SeenRootFunctions;
	for (const FAngelscriptModuleDesc* ModuleDesc
		: Graph.ResolvedModuleDescriptors)
	{
		if (ModuleDesc == nullptr || SeenModules.Contains(ModuleDesc))
		{
			return Fail(OutIndex, OutError, TEXT(
				"TypedASTJITDescriptorGraphInvalid: resolved ModuleDesc pointers must be non-null and unique."));
		}
		SeenModules.Add(ModuleDesc);

		for (const TSharedRef<FAngelscriptClassDesc>& ClassRef
			: ModuleDesc->Classes)
		{
			const FAngelscriptClassDesc* const ClassDesc = &ClassRef.Get();
			if (SeenClasses.Contains(ClassDesc))
			{
				return Fail(OutIndex, OutError, TEXT(
					"TypedASTJITDescriptorGraphInvalid: a resolved class descriptor appears more than once."));
			}
			SeenClasses.Add(ClassDesc);

			const FAngelscriptStaticJITDescriptorView* const* ClassViewPtr =
				DescriptorsByPointer.Find(ClassDesc);
			const EAngelscriptStaticJITDescriptorKind ExpectedClassKind =
				ClassDesc->bIsStruct
				? EAngelscriptStaticJITDescriptorKind::Struct
				: EAngelscriptStaticJITDescriptorKind::Class;
			if (ClassViewPtr == nullptr
				|| (*ClassViewPtr)->Kind != ExpectedClassKind)
			{
				return Fail(OutIndex, OutError, TEXT(
					"TypedASTJITDescriptorGraphMismatch: resolved ClassDesc is absent from the complete descriptor view."));
			}

			for (const TSharedRef<FAngelscriptFunctionDesc>& FunctionRef
				: ClassDesc->Methods)
			{
				const FAngelscriptFunctionDesc* const FunctionDesc =
					&FunctionRef.Get();
				asIScriptFunction* const ScriptFunction =
					FunctionDesc->ScriptFunction;
				if (ScriptFunction == nullptr
					|| SeenFunctionDescriptors.Contains(FunctionDesc)
					|| SeenRootFunctions.Contains(ScriptFunction))
				{
					return Fail(OutIndex, OutError, TEXT(
						"TypedASTJITRootDescriptorInvalid: every UFUNCTION descriptor requires one unique resolved ScriptFunction pointer."));
				}
				SeenFunctionDescriptors.Add(FunctionDesc);
				SeenRootFunctions.Add(ScriptFunction);

				const FAngelscriptStaticJITCompiledFunctionView* const*
					FunctionViewPtr = FunctionsByPointer.Find(ScriptFunction);
				const FAngelscriptStaticJITDescriptorView* const*
					DescriptorViewPtr = DescriptorsByPointer.Find(FunctionDesc);
				if (FunctionViewPtr == nullptr
					|| DescriptorViewPtr == nullptr
					|| (*DescriptorViewPtr)->Kind
						!= EAngelscriptStaticJITDescriptorKind::Function
					|| !(*DescriptorViewPtr)->bUFunctionRoot)
				{
					return Fail(OutIndex, OutError, TEXT(
						"TypedASTJITRootDescriptorMismatch: a resolved ClassDesc::Methods entry must exact-match one compiled function and one UFUNCTION descriptor row."));
				}

				const FAngelscriptStaticJITCompiledFunctionView& FunctionView =
					**FunctionViewPtr;
				const FAngelscriptStaticJITDescriptorView& DescriptorView =
					**DescriptorViewPtr;
				if (DescriptorView.StableDescriptorKey
						!= FunctionView.FunctionKey.Hash)
				{
					return Fail(OutIndex, OutError, TEXT(
						"TypedASTJITRootIdentityMismatch: descriptor and compiled-function stable identities disagree."));
				}

				FAngelscriptTypedASTJITRoot& Root =
					OutIndex.Roots.Emplace_GetRef();
				Root.EngineLocalFunction = ScriptFunction;
				Root.ClassDescriptor = ClassDesc;
				Root.FunctionDescriptor = FunctionDesc;
				Root.ModuleKey = FunctionView.ModuleKey;
				Root.FunctionKey = FunctionView.FunctionKey;
				Root.StableDescriptorKey =
					DescriptorView.StableDescriptorKey;
				Root.EntryPlan = DescriptorView.EntryPlan;
				Root.bGeneratedStaticsClassSurface =
					ClassDesc->bIsStaticsClass;
			}
		}
	}

	OutIndex.Roots.Sort(
		[](const FAngelscriptTypedASTJITRoot& Left,
			const FAngelscriptTypedASTJITRoot& Right)
		{
			return Left.FunctionKey.Hash < Right.FunctionKey.Hash;
		});
	for (int32 Index = 1; Index < OutIndex.Roots.Num(); ++Index)
	{
		if (OutIndex.Roots[Index - 1].FunctionKey
			== OutIndex.Roots[Index].FunctionKey)
		{
			return Fail(OutIndex, OutError, TEXT(
				"TypedASTJITRootIdentityDuplicate: distinct roots cannot share one stable function key."));
		}
	}
	return true;
}

#endif // AS_CAN_GENERATE_JIT
