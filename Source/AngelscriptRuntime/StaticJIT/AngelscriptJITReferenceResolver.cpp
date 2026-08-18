#include "StaticJIT/AngelscriptJITReferenceResolver.h"

#include "AngelscriptEngine.h"
#include "Cache/AngelscriptCacheEnvironment.h"
#include "Cache/AngelscriptCacheService.h"
#include "Cache/AngelscriptCacheStableSymbolIdentity.h"
#include "StaticJIT/AngelscriptStaticJITGenerationSnapshot.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_bytecode.h"
#include "source/as_module.h"
#include "source/as_objecttype.h"
#include "source/as_property.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_typeinfo.h"
#include "EndAngelscriptHeaders.h"

#include "UObject/UObjectIterator.h"

namespace
{
	class FScriptFunctionReferenceLifetime final
		: public FAngelscriptJITReferenceLifetime
	{
	public:
		explicit FScriptFunctionReferenceLifetime(asIScriptFunction* InFunction)
			: Function(InFunction)
		{
			check(Function != nullptr);
			Function->AddRef();
		}

		~FScriptFunctionReferenceLifetime() override
		{
			Function->Release();
		}

	private:
		asIScriptFunction* Function = nullptr;
	};

	class FTypeInfoReferenceLifetime final
		: public FAngelscriptJITReferenceLifetime
	{
	public:
		explicit FTypeInfoReferenceLifetime(asITypeInfo* InType)
			: Type(InType)
		{
			check(Type != nullptr);
			Type->AddRef();
		}

		~FTypeInfoReferenceLifetime() override
		{
			Type->Release();
		}

	private:
		asITypeInfo* Type = nullptr;
	};

	class FGlobalPropertyReferenceLifetime final
		: public FAngelscriptJITReferenceLifetime
	{
	public:
		explicit FGlobalPropertyReferenceLifetime(asCGlobalProperty* InProperty)
			: Property(InProperty)
		{
			check(Property != nullptr);
			Property->AddRef();
		}

		~FGlobalPropertyReferenceLifetime() override
		{
			Property->Release();
		}

	private:
		asCGlobalProperty* Property = nullptr;
	};

	class FStringLiteralReferenceLifetime final
		: public FAngelscriptJITReferenceLifetime
	{
	public:
		FStringLiteralReferenceLifetime(
			asIScriptEngine* InEngine,
			const void* InString)
			: Engine(InEngine)
			, String(InString)
		{
			check(Engine != nullptr && String != nullptr);
			Engine->AddRef();
		}

		~FStringLiteralReferenceLifetime() override
		{
			asCScriptEngine* const ScriptEngine =
				static_cast<asCScriptEngine*>(Engine);
			if (ScriptEngine->stringFactory != nullptr)
			{
				ScriptEngine->stringFactory->ReleaseStringConstant(String);
			}
			Engine->Release();
		}

	private:
		asIScriptEngine* Engine = nullptr;
		const void* String = nullptr;
	};

	struct FLiveTypeReference
	{
		FAngelscriptStableTypeKey Key;
		asCTypeInfo* Type = nullptr;
	};

	struct FLiveGlobalReference
	{
		FAngelscriptStableGlobalKey Key;
		asCGlobalProperty* Property = nullptr;
	};

	struct FLivePropertyReference
	{
		FAngelscriptStablePropertyKey Key;
		asCTypeInfo* OwnerType = nullptr;
		asCObjectProperty* Property = nullptr;
	};

	struct FLiveImportReference
	{
		FAngelscriptArtifactReference Reference;
		asCScriptFunction* Signature = nullptr;
	};

	static FAngelscriptArtifactReference MakeReference(
		const EAngelscriptArtifactReferenceKind Kind,
		const FAngelscriptHash256& StableKey,
		const FAngelscriptHash256& ExpectedAbi)
	{
		return {Kind, StableKey, ExpectedAbi};
	}

	static EAngelscriptArtifactMatchResult ToMatchResult(
		const EAngelscriptJITReferenceResolutionCode Code)
	{
		switch (Code)
		{
		case EAngelscriptJITReferenceResolutionCode::Missing:
			return EAngelscriptArtifactMatchResult::MissingReference;
		case EAngelscriptJITReferenceResolutionCode::Ambiguous:
			return EAngelscriptArtifactMatchResult::AmbiguousReference;
		case EAngelscriptJITReferenceResolutionCode::WrongKind:
			return EAngelscriptArtifactMatchResult::WrongReferenceKind;
		case EAngelscriptJITReferenceResolutionCode::AbiMismatch:
			return EAngelscriptArtifactMatchResult::ReferenceAbiMismatch;
		case EAngelscriptJITReferenceResolutionCode::Exact:
		default:
			return EAngelscriptArtifactMatchResult::Exact;
		}
	}
}

FAngelscriptArtifactReference
FAngelscriptJITReferenceDescriptorBuilder::ForFunction(
	const FAngelscriptStableFunctionKey& FunctionKey,
	const FAngelscriptHash256& ExpectedAbi)
{
	return MakeReference(EAngelscriptArtifactReferenceKind::ScriptFunction,
		FunctionKey.Hash, ExpectedAbi);
}

FAngelscriptArtifactReference
FAngelscriptJITReferenceDescriptorBuilder::ForType(
	const FAngelscriptStableTypeKey& TypeKey,
	const FAngelscriptHash256& ExpectedAbi)
{
	return MakeReference(EAngelscriptArtifactReferenceKind::ScriptType,
		TypeKey.Hash, ExpectedAbi);
}

FAngelscriptArtifactReference
FAngelscriptJITReferenceDescriptorBuilder::ForGlobal(
	const FAngelscriptStableGlobalKey& GlobalKey,
	const FAngelscriptHash256& ExpectedAbi)
{
	return MakeReference(EAngelscriptArtifactReferenceKind::ScriptGlobal,
		GlobalKey.Hash, ExpectedAbi);
}

FAngelscriptArtifactReference
FAngelscriptJITReferenceDescriptorBuilder::ForImport(
	const FAngelscriptHash256& ImportKey,
	const FAngelscriptHash256& ExpectedAbi)
{
	return MakeReference(EAngelscriptArtifactReferenceKind::ScriptImport,
		ImportKey, ExpectedAbi);
}

FAngelscriptArtifactReference
FAngelscriptJITReferenceDescriptorBuilder::ForImportSlot(
	const FAngelscriptStableModuleKey& ConsumerModuleKey,
	const FStringView CanonicalNamespace,
	const FStringView CanonicalDeclaration,
	const FStringView DeclaredSourceModule)
{
	if (ConsumerModuleKey.Hash.IsZero() || CanonicalDeclaration.IsEmpty()
		|| DeclaredSourceModule.IsEmpty())
	{
		return {};
	}

	FAngelscriptArtifactCanonicalWriter KeyWriter(
		TEXT("jit-import-slot-reference-v1"));
	KeyWriter.WriteHash(ConsumerModuleKey.Hash);
	KeyWriter.WriteString(CanonicalNamespace);
	KeyWriter.WriteString(CanonicalDeclaration);
	KeyWriter.WriteString(DeclaredSourceModule);

	FAngelscriptArtifactCanonicalWriter AbiWriter(
		TEXT("jit-import-slot-abi-v1"));
	AbiWriter.WriteString(CanonicalNamespace);
	AbiWriter.WriteString(CanonicalDeclaration);
	return ForImport(KeyWriter.FinalizeHash(), AbiWriter.FinalizeHash());
}

FAngelscriptArtifactReference
FAngelscriptJITReferenceDescriptorBuilder::ForStringLiteral(
	const FStringView Value,
	const FAngelscriptHash256& ExpectedAbi)
{
	FAngelscriptArtifactCanonicalWriter Writer(
		TEXT("jit-string-literal-reference-v1"));
	Writer.WriteString(Value);
	return MakeReference(EAngelscriptArtifactReferenceKind::StringLiteral,
		Writer.FinalizeHash(), ExpectedAbi);
}

FAngelscriptArtifactReference
FAngelscriptJITReferenceDescriptorBuilder::ForStringLiteral(
	const FStringView Value)
{
	FAngelscriptArtifactCanonicalWriter AbiWriter(
		TEXT("jit-string-literal-abi-v1"));
	AbiWriter.WriteString(Value);
	return ForStringLiteral(Value, AbiWriter.FinalizeHash());
}

FAngelscriptArtifactReference
FAngelscriptJITReferenceDescriptorBuilder::ForRuntimeHelper(
	const FStringView CanonicalSymbol,
	const FAngelscriptHash256& ExpectedAbi)
{
	FAngelscriptArtifactCanonicalWriter Writer(
		TEXT("jit-runtime-helper-reference-v1"));
	Writer.WriteString(CanonicalSymbol);
	return MakeReference(EAngelscriptArtifactReferenceKind::RuntimeHelper,
		Writer.FinalizeHash(), ExpectedAbi);
}

FAngelscriptJITReferenceResolver::FAngelscriptJITReferenceResolver(
	const void* InEngineIdentity,
	TArray<FAngelscriptJITReferenceBinding>&& InBindings)
	: EngineIdentity(InEngineIdentity)
	, Bindings(MoveTemp(InBindings))
{
	Bindings.Sort([](
		const FAngelscriptJITReferenceBinding& Left,
		const FAngelscriptJITReferenceBinding& Right)
	{
		if (!(Left.Reference.StableKey == Right.Reference.StableKey))
		{
			return Left.Reference.StableKey < Right.Reference.StableKey;
		}
		const uint8 LeftKind = static_cast<uint8>(Left.Reference.Kind);
		const uint8 RightKind = static_cast<uint8>(Right.Reference.Kind);
		if (LeftKind != RightKind)
		{
			return LeftKind < RightKind;
		}
		return Left.Reference.ExpectedAbi < Right.Reference.ExpectedAbi;
	});
}

FAngelscriptJITReferenceResolution FAngelscriptJITReferenceResolver::Resolve(
	const FAngelscriptArtifactReference& Reference) const
{
	if (!Reference.IsValid() || Reference.ExpectedAbi.IsZero())
	{
		return {};
	}
	bool bFoundStableKey = false;
	bool bFoundKind = false;
	const void* ExactValue = nullptr;
	TSharedPtr<const FAngelscriptJITReferenceLifetime,
		ESPMode::ThreadSafe> ExactLifetime;
	for (const FAngelscriptJITReferenceBinding& Binding : Bindings)
	{
		if (!(Binding.Reference.StableKey == Reference.StableKey))
		{
			continue;
		}
		bFoundStableKey = true;
		if (Binding.Reference.Kind != Reference.Kind)
		{
			continue;
		}
		bFoundKind = true;
		if (!(Binding.Reference.ExpectedAbi == Reference.ExpectedAbi))
		{
			continue;
		}
		if (ExactValue == nullptr)
		{
			ExactValue = Binding.Value;
			ExactLifetime = Binding.Lifetime;
		}
		else if (ExactValue != Binding.Value)
		{
			return {EAngelscriptJITReferenceResolutionCode::Ambiguous, nullptr, {}};
		}
	}
	if (ExactValue != nullptr)
	{
		return {EAngelscriptJITReferenceResolutionCode::Exact,
			ExactValue, MoveTemp(ExactLifetime)};
	}
	if (bFoundKind)
	{
		return {EAngelscriptJITReferenceResolutionCode::AbiMismatch, nullptr};
	}
	if (bFoundStableKey)
	{
		return {EAngelscriptJITReferenceResolutionCode::WrongKind, nullptr};
	}
	return {};
}

bool FAngelscriptJITReferenceIndexBuilder::Add(
	const FAngelscriptArtifactReference& Reference,
	const void* Value)

{
	return Add(Reference, Value, {});
}

bool FAngelscriptJITReferenceIndexBuilder::Add(
	const FAngelscriptArtifactReference& Reference,
	const void* Value,
	TSharedPtr<const FAngelscriptJITReferenceLifetime,
		ESPMode::ThreadSafe> Lifetime)
{
	if (!Reference.IsValid() || Reference.ExpectedAbi.IsZero() || Value == nullptr)
	{
		return false;
	}
	Bindings.Add({Reference, Value, MoveTemp(Lifetime)});
	return true;
}

bool FAngelscriptJITReferenceIndexBuilder::AddScriptFunction(
	const FAngelscriptArtifactReference& Reference,
	asIScriptFunction* Function)
{
	if (Reference.Kind != EAngelscriptArtifactReferenceKind::ScriptFunction
		|| Function == nullptr
		|| Function->GetEngine() != EngineIdentity)
	{
		return false;
	}
	FAngelscriptJITReferenceBinding& Binding = Bindings.AddDefaulted_GetRef();
	Binding.Reference = Reference;
	Binding.Value = Function;
	Binding.Lifetime = MakeShared<FScriptFunctionReferenceLifetime,
		ESPMode::ThreadSafe>(Function);
	return true;
}

TSharedRef<const FAngelscriptJITReferenceResolver,
	ESPMode::ThreadSafe> FAngelscriptJITReferenceIndexBuilder::Build() const
{
	TArray<FAngelscriptJITReferenceBinding> CopiedBindings = Bindings;
	return MakeShared<FAngelscriptJITReferenceResolver,
		ESPMode::ThreadSafe>(EngineIdentity, MoveTemp(CopiedBindings));
}

FAngelscriptJITCurrentEngineReferenceBuildResult
FAngelscriptJITCurrentEngineReferenceResolverBuilder::Build(
	FAngelscriptEngine& Engine,
	const TConstArrayView<FAngelscriptArtifactReference> RequiredReferences)
{
	FAngelscriptJITCurrentEngineReferenceBuildResult Result;
	asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
		Engine.GetScriptEngine());
	if (ScriptEngine == nullptr)
	{
		Result.Detail = TEXT(
			"StaticJIT reference resolution requires a live AngelScript Engine.");
		return Result;
	}

	FAngelscriptCacheService* const CacheService = Engine.GetCacheService();
	const FAngelscriptCacheLifecyclePublications Publications =
		CacheService != nullptr
			? CacheService->GetLifecyclePublications()
			: FAngelscriptCacheLifecyclePublications();
	const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
		ESPMode::ThreadSafe> Routes = Engine.GetFunctionRouteSnapshot();
	const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
		ESPMode::ThreadSafe> GenerationSnapshot =
			Engine.GetStaticJITGenerationSnapshot();
	const bool bHasTransientReferenceAuthority = Routes.IsValid()
		&& !Routes->ValidatedFunctionArtifactReferenceSets.IsEmpty();
	const bool bHasGenerationReferenceAuthority =
		GenerationSnapshot.IsValid() && GenerationSnapshot->IsComplete()
		&& GenerationSnapshot->Functions.ContainsByPredicate(
			[](const FAngelscriptStaticJITGenerationFunction& Function)
			{
				return Function.bHasVerifiedArtifactReferences;
			});
	if (!Publications.Current.IsValid() && !bHasTransientReferenceAuthority
		&& !bHasGenerationReferenceAuthority)
	{
		Result.Code =
			EAngelscriptJITCurrentEngineReferenceBuildCode::
				MissingCurrentPublication;
		Result.Detail = TEXT(
			"StaticJIT reference resolution requires the current validated Cache V2 publication.");
		return Result;
	}

	TArray<FAngelscriptArtifactReference> Requested;
	for (const FAngelscriptArtifactReference& Reference : RequiredReferences)
	{
		if (Reference.IsValid() && !Reference.ExpectedAbi.IsZero())
		{
			Requested.AddUnique(Reference);
		}
	}
	Requested.Sort();
	Result.RequestedReferenceCount = static_cast<uint32>(Requested.Num());

	TArray<FAngelscriptHash256> RequestedKeys;
	for (const FAngelscriptArtifactReference& Reference : Requested)
	{
		RequestedKeys.AddUnique(Reference.StableKey);
	}

	// ABI authority for script-owned symbols is the current validated semantic
	// publication, not the ABI claimed by a loaded Provider.
	TArray<FAngelscriptArtifactReference> CurrentReferences;
	if (Publications.Current.IsValid())
	{
		for (const FAngelscriptCacheCleanModuleArtifacts& Module
			: Publications.Current->Modules)
		{
			for (const FAngelscriptFunctionArtifactReferenceSet& ReferenceSet
				: Module.ValidatedFunctionArtifactReferenceSets)
			{
				for (const FAngelscriptArtifactReference& Reference
					: ReferenceSet.RequiredReferences)
				{
					if (RequestedKeys.Contains(Reference.StableKey))
					{
						CurrentReferences.AddUnique(Reference);
					}
				}
			}
		}
	}
	if (Routes.IsValid())
	{
		for (const FAngelscriptFunctionArtifactReferenceSet& ReferenceSet
			: Routes->ValidatedFunctionArtifactReferenceSets)
		{
			for (const FAngelscriptArtifactReference& Reference
				: ReferenceSet.RequiredReferences)
			{
				if (RequestedKeys.Contains(Reference.StableKey))
				{
					CurrentReferences.AddUnique(Reference);
				}
			}
		}
	}
	if (bHasGenerationReferenceAuthority)
	{
		// A StaticJITGeneration Engine owns a complete same-compilation
		// snapshot instead of a reusable Cache publication. Its verified
		// reference facts are the exact transient authority for that Engine.
		for (const FAngelscriptStaticJITGenerationFunction& Function
			: GenerationSnapshot->Functions)
		{
			if (!Function.bHasVerifiedArtifactReferences)
			{
				continue;
			}
			for (const FAngelscriptArtifactReference& Reference
				: Function.RequiredReferences)
			{
				if (RequestedKeys.Contains(Reference.StableKey))
				{
					CurrentReferences.AddUnique(Reference);
				}
			}
		}
	}
	CurrentReferences.Sort();

	TMap<asIScriptModule*, FAngelscriptStableModuleKey> ModuleKeys;
	for (const TSharedRef<FAngelscriptModuleDesc>& Module
		: Engine.GetActiveModules())
	{
		FAngelscriptStableModuleKey ModuleKey;
		if (Module->ScriptModule != nullptr
			&& FAngelscriptCacheStableSymbolIdentity::TryBuildModuleKey(
				*Module, ModuleKey))
		{
			ModuleKeys.Add(Module->ScriptModule, ModuleKey);
		}
	}

	TArray<FLiveTypeReference> LiveTypes;
	const auto AddLiveType = [&](asCTypeInfo* Type)
	{
		const FAngelscriptStableModuleKey* const ModuleKey = Type != nullptr
			? ModuleKeys.Find(Type->module) : nullptr;
		FAngelscriptStableTypeKey TypeKey;
		if (ModuleKey != nullptr
			&& FAngelscriptCacheStableSymbolIdentity::TryBuildLocalTypeKey(
				*ModuleKey, *Type, TypeKey)
			&& !LiveTypes.ContainsByPredicate(
				[Type](const FLiveTypeReference& Candidate)
				{
					return Candidate.Type == Type;
				}))
		{
			LiveTypes.Add({TypeKey, Type});
		}
	};
	ScriptEngine->allRegisteredTypes.IterateAll(AddLiveType);
	// Script-declared object, enum, typedef and funcdef types are module-owned
	// and do not have to appear in the Engine's application-registration map.
	// The module-local map is the complete current-generation authority and is
	// also populated by Cache V2 restoration.
	for (const TSharedRef<FAngelscriptModuleDesc>& Module
		: Engine.GetActiveModules())
	{
		if (Module->ScriptModule != nullptr)
		{
			Module->ScriptModule->allLocalTypes.IterateAll(AddLiveType);
		}
	}

	TArray<FLiveGlobalReference> LiveGlobals;
	const auto AddLiveGlobal = [&](asCGlobalProperty* const Property)
	{
		const FAngelscriptStableModuleKey* const ModuleKey = Property != nullptr
			? ModuleKeys.Find(Property->module) : nullptr;
		if (Property == nullptr || ModuleKey == nullptr
			|| LiveGlobals.ContainsByPredicate(
				[Property](const FLiveGlobalReference& Candidate)
				{
					return Candidate.Property == Property;
				}))
		{
			return;
		}
		const char* const CanonicalType = ScriptEngine->GetTypeDeclaration(
			ScriptEngine->GetTypeIdFromDataType(Property->type), true);
		if (CanonicalType == nullptr)
		{
			return;
		}
		FAngelscriptGlobalIdentityDescriptor Identity;
		Identity.ModuleKey = *ModuleKey;
		Identity.Namespace = Property->nameSpace != nullptr
			? UTF8_TO_TCHAR(Property->nameSpace->name.AddressOf()) : FString();
		Identity.Kind = EAngelscriptArtifactEntityKind::GlobalVariable;
		Identity.Name = UTF8_TO_TCHAR(Property->name.AddressOf());
		Identity.CanonicalType = UTF8_TO_TCHAR(CanonicalType);
		LiveGlobals.Add({
			FAngelscriptArtifactIdentityBuilder::BuildGlobalKey(Identity),
			Property});
	};
	for (asUINT Index = 0;
		Index < ScriptEngine->globalProperties.GetLength(); ++Index)
	{
		AddLiveGlobal(ScriptEngine->globalProperties[Index]);
	}
	// Script-declared globals remain module-owned even when the maintained
	// Engine removes them from its application-registration availability table.
	// The active module list is therefore the complete current-generation
	// authority, matching the module-local type scan above and Cache restore.
	for (const TSharedRef<FAngelscriptModuleDesc>& Module
		: Engine.GetActiveModules())
	{
		asCModule* const ScriptModule = static_cast<asCModule*>(
			Module->ScriptModule);
		if (ScriptModule == nullptr)
		{
			continue;
		}
		for (asUINT GlobalIndex = 0;
			GlobalIndex < ScriptModule->scriptGlobalsList.GetLength();
			++GlobalIndex)
		{
			AddLiveGlobal(ScriptModule->scriptGlobalsList[GlobalIndex]);
		}
	}

	TArray<FLivePropertyReference> LiveProperties;
	for (const FLiveTypeReference& LiveType : LiveTypes)
	{
		asCObjectType* const ObjectType = CastToObjectType(LiveType.Type);
		if (ObjectType == nullptr)
		{
			continue;
		}
		for (asUINT PropertyIndex = 0;
			PropertyIndex < ObjectType->properties.GetLength(); ++PropertyIndex)
		{
			asCObjectProperty* const Property =
				ObjectType->properties[PropertyIndex];
			if (Property == nullptr)
			{
				continue;
			}
			const asCString CanonicalType = Property->type.Format(
				ObjectType->nameSpace, false, false);
			FAngelscriptPropertyIdentityDescriptor Identity;
			Identity.OwnerTypeKey = LiveType.Key;
			Identity.Kind = EAngelscriptArtifactEntityKind::Property;
			Identity.Name = UTF8_TO_TCHAR(Property->name.AddressOf());
			Identity.CanonicalType = UTF8_TO_TCHAR(CanonicalType.AddressOf());
			LiveProperties.Add({
				FAngelscriptArtifactIdentityBuilder::BuildPropertyKey(Identity),
				LiveType.Type,
				Property});
		}
	}

	TArray<FLiveImportReference> LiveImports;
	for (const TSharedRef<FAngelscriptModuleDesc>& Module
		: Engine.GetActiveModules())
	{
		asCModule* const ScriptModule = static_cast<asCModule*>(
			Module->ScriptModule);
		const FAngelscriptStableModuleKey* const ModuleKey =
			ScriptModule != nullptr ? ModuleKeys.Find(ScriptModule) : nullptr;
		if (ScriptModule == nullptr || ModuleKey == nullptr)
		{
			continue;
		}
		for (asUINT ImportIndex = 0;
			ImportIndex < ScriptModule->bindInformations.GetLength();
			++ImportIndex)
		{
			const sBindInfo* const BindInfo =
				ScriptModule->bindInformations[ImportIndex];
			asCScriptFunction* const Signature = BindInfo != nullptr
				? BindInfo->importedFunctionSignature : nullptr;
			if (Signature == nullptr || Signature->module != ScriptModule
				|| Signature->funcType != asFUNC_IMPORTED
				|| (Signature->id & FUNC_IMPORTED) == 0
				|| BindInfo->importFromModule.GetLength() == 0)
			{
				continue;
			}
			const FAngelscriptArtifactReference Reference =
				FAngelscriptJITReferenceDescriptorBuilder::ForImportSlot(
					*ModuleKey,
					UTF8_TO_TCHAR(Signature->GetNamespace()),
					UTF8_TO_TCHAR(Signature->GetDeclaration(
						false, false, false)),
					UTF8_TO_TCHAR(BindInfo->importFromModule.AddressOf()));
			if (Reference.IsValid() && !Reference.ExpectedAbi.IsZero())
			{
				LiveImports.Add({Reference, Signature});
			}
		}
	}

	FAngelscriptJITReferenceIndexBuilder Index(ScriptEngine);

	// Environment symbols intentionally share one public reference kind, but
	// resolving every requested symbol independently would rescan the complete
	// application registration surface once for types, functions and
	// properties. Build the requested Engine-local index in one pass instead.
	// The produced references still contain the current stable key and ABI, so
	// the ordinary resolver preserves exact/missing/ambiguous/ABI-mismatch
	// behavior when it compares them with Provider requirements below.
	TArray<FAngelscriptHash256> RequestedEnvironmentKeys;
	for (const FAngelscriptArtifactReference& Reference : CurrentReferences)
	{
		if (Reference.Kind
			== EAngelscriptArtifactReferenceKind::EnvironmentSymbol)
		{
			RequestedEnvironmentKeys.AddUnique(Reference.StableKey);
		}
	}
	if (!RequestedEnvironmentKeys.IsEmpty())
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			FAngelscriptArtifactReference Reference;
			if (FAngelscriptCacheEnvironmentIdentity::TryBuildCodeRootReference(
					**It, Reference)
				&& RequestedEnvironmentKeys.Contains(Reference.StableKey))
			{
				Index.Add(Reference, *It);
			}
		}

		ScriptEngine->allRegisteredTypes.IterateAll(
			[&](asCTypeInfo* Type)
			{
				if (Type == nullptr || Type->module != nullptr)
				{
					return;
				}

				FAngelscriptArtifactReference TypeReference;
				if (FAngelscriptCacheEnvironmentIdentity::TryBuildTypeReference(
						*Type, TypeReference)
					&& RequestedEnvironmentKeys.Contains(
						TypeReference.StableKey))
				{
					Index.Add(TypeReference, Type,
						MakeShared<FTypeInfoReferenceLifetime,
							ESPMode::ThreadSafe>(Type));
				}

				asCObjectType* const ObjectType = CastToObjectType(Type);
				if (ObjectType == nullptr)
				{
					return;
				}
				for (asUINT PropertyIndex = 0;
					PropertyIndex < ObjectType->properties.GetLength();
					++PropertyIndex)
				{
					asCObjectProperty* const Property =
						ObjectType->properties[PropertyIndex];
					FAngelscriptArtifactReference PropertyReference;
					if (Property != nullptr
						&& FAngelscriptCacheEnvironmentIdentity::
							TryBuildPropertyReference(
								*Type, *Property, PropertyReference)
						&& RequestedEnvironmentKeys.Contains(
							PropertyReference.StableKey))
					{
						Index.Add(PropertyReference, Property,
							MakeShared<FTypeInfoReferenceLifetime,
								ESPMode::ThreadSafe>(Type));
					}
				}
			});

		TArray<asCScriptFunction*> CurrentFunctions;
		FAngelscriptCacheEnvironmentIdentity::CollectCurrentRegisteredFunctions(
			*ScriptEngine, CurrentFunctions);
		for (asCScriptFunction* const Function : CurrentFunctions)
		{
			FAngelscriptArtifactReference FunctionReference;
			if (FAngelscriptCacheEnvironmentIdentity::TryBuildFunctionReference(
					*Function, FunctionReference)
				&& RequestedEnvironmentKeys.Contains(
					FunctionReference.StableKey))
			{
				Index.Add(FunctionReference, Function,
					MakeShared<FScriptFunctionReferenceLifetime,
						ESPMode::ThreadSafe>(Function));
			}
		}
	}

	for (const FAngelscriptArtifactReference& Reference : CurrentReferences)
	{
		switch (Reference.Kind)
		{
		case EAngelscriptArtifactReferenceKind::ScriptFunction:
			if (Routes.IsValid())
			{
				const FAngelscriptFunctionRoute* const Route =
					Routes->FunctionRoutes.FindByPredicate(
						[&Reference](const FAngelscriptFunctionRoute& Candidate)
						{
							return Candidate.Identity.FunctionKey.Hash
								== Reference.StableKey;
						});
				if (Route != nullptr && Route->Function != nullptr)
				{
					Index.AddScriptFunction(Reference, Route->Function);
				}
			}
			break;
		case EAngelscriptArtifactReferenceKind::ScriptType:
			for (const FLiveTypeReference& Type : LiveTypes)
			{
				if (Type.Key.Hash == Reference.StableKey)
				{
					Index.Add(Reference, Type.Type,
						MakeShared<FTypeInfoReferenceLifetime,
							ESPMode::ThreadSafe>(Type.Type));
				}
			}
			break;
		case EAngelscriptArtifactReferenceKind::ScriptGlobal:
			for (const FLiveGlobalReference& Global : LiveGlobals)
			{
				if (Global.Key.Hash == Reference.StableKey)
				{
					Index.Add(Reference, Global.Property->GetAddressOfValue(),
						MakeShared<FGlobalPropertyReferenceLifetime,
							ESPMode::ThreadSafe>(Global.Property));
				}
			}
			break;
		case EAngelscriptArtifactReferenceKind::ScriptProperty:
			for (const FLivePropertyReference& Property : LiveProperties)
			{
				if (Property.Key.Hash == Reference.StableKey)
				{
					Index.Add(Reference, Property.Property,
						MakeShared<FTypeInfoReferenceLifetime,
							ESPMode::ThreadSafe>(Property.OwnerType));
				}
			}
			break;
		case EAngelscriptArtifactReferenceKind::ScriptImport:
			for (const FLiveImportReference& Import : LiveImports)
			{
				if (Import.Reference.StableKey == Reference.StableKey)
				{
					Index.Add(Import.Reference, Import.Signature,
						MakeShared<FScriptFunctionReferenceLifetime,
							ESPMode::ThreadSafe>(Import.Signature));
				}
			}
			break;
		case EAngelscriptArtifactReferenceKind::EnvironmentSymbol:
			// Batched above so each current registration category is scanned
			// only once per resolver build.
			break;
		default:
			break;
		}
	}

	// String storage is VM-owned and intentionally absent from the current
	// semantic dependency set in some bytecode shapes. Recover its content from
	// current bytecode, then acquire a separately owned current-Engine constant.
	TArray<FAngelscriptArtifactReference> AddedStringReferences;
	if (ScriptEngine->stringFactory != nullptr && Routes.IsValid())
	{
		for (const FAngelscriptFunctionRoute& Route : Routes->FunctionRoutes)
		{
			asCScriptFunction* const Function =
				static_cast<asCScriptFunction*>(Route.Function);
			asUINT BytecodeLength = 0;
			const asDWORD* const Bytecode = Function != nullptr
				? Function->GetByteCode(&BytecodeLength) : nullptr;
			for (asUINT Offset = 0;
				Bytecode != nullptr && Offset < BytecodeLength;)
			{
				const asEBCInstr Opcode = static_cast<asEBCInstr>(
					*reinterpret_cast<const asBYTE*>(&Bytecode[Offset]));
				if (static_cast<int32>(Opcode)
					> static_cast<int32>(asBC_MAXBYTECODE))
				{
					break;
				}
				const int32 InstructionSize =
					asBCTypeSize[asBCInfo[Opcode].type];
				if (InstructionSize <= 0)
				{
					break;
				}
				if (Opcode == asBC_PGA || Opcode == asBC_PshGPtr)
				{
					void* const Storage = reinterpret_cast<void*>(
						asBC_PTRARG(&Bytecode[Offset]));
					if (Storage != nullptr
						&& !ScriptEngine->varAddressMap.Contains(Storage))
					{
						asUINT Utf8Length = 0;
						if (ScriptEngine->stringFactory->GetRawStringData(
								Storage, nullptr, &Utf8Length) >= 0)
						{
							TArray<char> Utf8;
							Utf8.SetNumUninitialized(Utf8Length + 1);
							if (ScriptEngine->stringFactory->GetRawStringData(
									Storage, Utf8.GetData(), &Utf8Length) >= 0)
							{
								Utf8[Utf8Length] = '\0';
								const FUTF8ToTCHAR Converted(
									Utf8.GetData(), Utf8Length);
								const FString Literal(
									Converted.Length(), Converted.Get());
								const FAngelscriptArtifactReference Reference =
									FAngelscriptJITReferenceDescriptorBuilder::
										ForStringLiteral(Literal);
								if (!AddedStringReferences.Contains(Reference))
								{
									const void* const Retained =
										ScriptEngine->stringFactory->GetStringConstant(
											Utf8.GetData(), Utf8Length);
									if (Retained != nullptr)
									{
										Index.Add(Reference, Retained,
											MakeShared<
												FStringLiteralReferenceLifetime,
												ESPMode::ThreadSafe>(
													ScriptEngine, Retained));
										AddedStringReferences.Add(Reference);
									}
								}
							}
						}
					}
				}
				Offset += static_cast<asUINT>(InstructionSize);
			}
		}
	}

	Result.Resolver = Index.Build();
	TArray<FString> Unresolved;
	for (const FAngelscriptArtifactReference& Reference : Requested)
	{
		const FAngelscriptJITReferenceResolution Resolution =
			Result.Resolver->Resolve(Reference);
		if (Resolution.Code == EAngelscriptJITReferenceResolutionCode::Exact
			&& Resolution.Value != nullptr)
		{
			++Result.ResolvedReferenceCount;
		}
		else
		{
			Unresolved.Add(FString::Printf(TEXT("%u:%s:%u"),
				static_cast<uint32>(Reference.Kind),
				*Reference.StableKey.ToHexString(),
				static_cast<uint32>(Resolution.Code)));
		}
	}
	Result.Code = EAngelscriptJITCurrentEngineReferenceBuildCode::Ready;
	Result.Detail = FString::Printf(
		TEXT("Current Engine reference index built: requested=%u resolved=%u unresolved=[%s]"),
		Result.RequestedReferenceCount,
		Result.ResolvedReferenceCount,
		*FString::Join(Unresolved, TEXT(", ")));
	return Result;
}

FAngelscriptJITReferenceTableBuildResult
FAngelscriptJITResolvedReferenceTableBuilder::Build(
	const TConstArrayView<FAngelscriptJITReferenceSlot> Slots,
	const IAngelscriptJITReferenceResolver& Resolver)
{
	FAngelscriptJITReferenceTableBuildResult Result;
	TSharedRef<FAngelscriptJITResolvedReferenceTable,
		ESPMode::ThreadSafe> Table =
		MakeShared<FAngelscriptJITResolvedReferenceTable,
			ESPMode::ThreadSafe>();
	Table->Slots.Reserve(Slots.Num());
	for (int32 SlotArrayIndex = 0;
		SlotArrayIndex < Slots.Num();
		++SlotArrayIndex)
	{
		const FAngelscriptJITReferenceSlot& Slot = Slots[SlotArrayIndex];
		if (Slot.SlotIndex != static_cast<uint32>(SlotArrayIndex))
		{
			Result.Result = EAngelscriptArtifactMatchResult::InvalidProvider;
			Result.FailedSlotIndex = SlotArrayIndex;
			return Result;
		}
		const bool bOptional = EnumHasAnyFlags(
			Slot.Flags, EAngelscriptJITReferenceSlotFlags::Optional);
		FAngelscriptJITReferenceResolution Resolution =
			Resolver.Resolve(Slot.Reference);
		if (Resolution.Code == EAngelscriptJITReferenceResolutionCode::Exact
			&& Resolution.Value == nullptr)
		{
			Resolution.Code = EAngelscriptJITReferenceResolutionCode::Missing;
		}
		if (Resolution.Code != EAngelscriptJITReferenceResolutionCode::Exact
			&& !(bOptional
				&& Resolution.Code
					== EAngelscriptJITReferenceResolutionCode::Missing))
		{
			Result.Result = ToMatchResult(Resolution.Code);
			Result.FailedSlotIndex = SlotArrayIndex;
			return Result;
		}
		FAngelscriptJITResolvedReferenceSlot& Resolved =
			Table->Slots.AddDefaulted_GetRef();
		Resolved.SlotIndex = Slot.SlotIndex;
		Resolved.Kind = Slot.Reference.Kind;
		Resolved.Value = Resolution.Code
			== EAngelscriptJITReferenceResolutionCode::Exact
				? Resolution.Value : nullptr;
		Resolved.bOptional = bOptional;
		Resolved.Lifetime = MoveTemp(Resolution.Lifetime);
	}
	Result.Result = EAngelscriptArtifactMatchResult::Exact;
	Result.Table = Table;
	return Result;
}
