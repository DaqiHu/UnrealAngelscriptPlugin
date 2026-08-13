#include "StaticJIT/AngelscriptStaticJITGenerationSnapshot.h"

#if AS_CAN_GENERATE_JIT

#include "AngelscriptEngine.h"
#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheStableSymbolIdentity.h"
#include "StaticJIT/AngelscriptJITGeneration.h"
#include "StaticJIT/BytecodeJIT/AngelscriptBytecodeJIT.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_objecttype.h"
#include "source/as_scriptfunction.h"
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

	static FAngelscriptHash256 BuildEntryPlanHash(
		const FStringView Receiver,
		const FStringView Declaration,
		const bool bHasVMEntry,
		const bool bHasRawEntry,
		const bool bHasParmsEntry)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("static-jit-entry-plan"));
		Writer.WriteString(Receiver);
		Writer.WriteString(Declaration);
		Writer.WriteBool(bHasVMEntry);
		Writer.WriteBool(bHasRawEntry);
		Writer.WriteBool(bHasParmsEntry);
		return Writer.FinalizeHash();
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
			Descriptor.EntryPlan.bHasVMEntry = FunctionPlan->bHasVMEntry;
			Descriptor.EntryPlan.bHasRawEntry = FunctionPlan->bHasRawEntry;
			Descriptor.EntryPlan.bHasParmsEntry =
				FunctionPlan->bHasParmsEntry;
			Descriptor.EntryPlan.EntryPlanHash = BuildEntryPlanHash(
				Descriptor.ReceiverCanonicalName,
				Descriptor.CanonicalDeclaration,
				Descriptor.EntryPlan.bHasVMEntry,
				Descriptor.EntryPlan.bHasRawEntry,
				Descriptor.EntryPlan.bHasParmsEntry);
		}
	}
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
			bool bIsConst = false;
			if (ScriptModule->GetGlobalVar(GlobalIndex, &Name, &Namespace,
				&TypeId, &bIsConst) < 0 || Name == nullptr)
			{
				continue;
			}
			FAngelscriptGlobalIdentityDescriptor Identity;
			Identity.ModuleKey = ModuleKey;
			Identity.Namespace = Namespace != nullptr
				? UTF8_TO_TCHAR(Namespace) : FString();
			Identity.Name = UTF8_TO_TCHAR(Name);
			Identity.CanonicalType = UTF8_TO_TCHAR(
				ScriptModule->GetEngine()->GetTypeDeclaration(TypeId, true));
			if (bIsConst)
			{
				Identity.CanonicalTraits.Add(TEXT("const"));
			}
			FAngelscriptStaticJITGenerationGlobal& Global =
				Snapshot->Globals.Emplace_GetRef();
			Global.EngineLocalGlobal =
				ScriptModule->GetAddressOfGlobalVar(GlobalIndex);
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
			Added.bHasVMEntry = true;
			Added.bHasRawEntry = true;
			Added.bHasParmsEntry =
				FAngelscriptBytecodeJIT::CanGenerateParmsEntry(Function);
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
			}
		}

		for (const TSharedRef<FAngelscriptClassDesc>& ClassDesc
			: ModuleDesc->Classes)
		{
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
				FString(), false, nullptr);

			for (const TSharedRef<FAngelscriptPropertyDesc>& Property
				: ClassDesc->Properties)
			{
				FAngelscriptPropertyIdentityDescriptor Identity;
				Identity.OwnerTypeKey = Type != nullptr
					? Type->TypeKey : FAngelscriptStableTypeKey{};
				Identity.Name = Property->PropertyName;
				Identity.CanonicalType = Property->LiteralType;
				AddDescriptor(*Snapshot, &Property.Get(),
					EAngelscriptStaticJITGenerationDescriptorKind::Property,
					FAngelscriptArtifactIdentityBuilder::BuildPropertyKey(
						Identity).Hash,
					Property->PropertyName, Property->LiteralType,
					ClassDesc->ClassName, false, nullptr);
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
					true, Function);
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
				FString(), false, nullptr);
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
				FString(), false, nullptr);
		}
	}

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
	Snapshot->bDescriptorAnalysisComplete = true;
	if (!Snapshot->IsComplete())
	{
		for (const FAngelscriptStaticJITGenerationFunction& Function
			: Snapshot->Functions)
		{
			if (Function.EngineLocalFunction == nullptr
				|| Function.ModuleKey.Hash.IsZero()
				|| Function.FunctionKey.Hash.IsZero()
				|| Function.ExecutionHash.IsZero()
				|| Function.DebugHash.IsZero()
				|| Function.EntryAbiHash.IsZero())
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
					&& (Descriptor.EntryPlan.EntryPlanHash.IsZero()
						|| !Descriptor.EntryPlan.bHasVMEntry
						|| !Descriptor.EntryPlan.bHasRawEntry)))
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
