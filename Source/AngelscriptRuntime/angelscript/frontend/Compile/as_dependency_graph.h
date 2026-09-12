/* Maintained Unreal AngelScript fork — typed declaration and body dependency graphs. */
#pragma once

#include "Core/AngelscriptDescriptors.h"
#include "frontend/Basic/as_source_location.h"

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"

BEGIN_AS_NAMESPACE

class asCCompilationSession;
	class asCSourceManager;

	enum class EAngelscriptDependencyReason : uint8
	{
		Base,
		Interface,
		PropertyType,
		ReturnType,
		ParameterType,
		GenericArgument,
		DelegateSignature,
	};

	enum class EAngelscriptCompletenessRequirement : uint8
	{
		DeclarationOnly,
		Signature,
		CompleteDefinition,
	};

	struct FAngelscriptDependencyEdge
	{
		FString SourceModuleKey;
		FString TargetModuleKey;
		EAngelscriptDependencyReason Reason = EAngelscriptDependencyReason::PropertyType;
		asCSourceRange SourceRange;
		FAngelscriptResolvedSourceAnchor StableSource;
		EAngelscriptCompletenessRequirement Completeness = EAngelscriptCompletenessRequirement::DeclarationOnly;

		bool IsValid() const
		{
			return !SourceModuleKey.IsEmpty()
				&& !TargetModuleKey.IsEmpty()
				&& SourceModuleKey != TargetModuleKey
				&& SourceRange.IsOrdered()
				&& StableSource.IsValid();
		}
	};

	struct FAngelscriptDependencyComponent
	{
		TArray<FString> Members;
	};

	struct FAngelscriptCondensationEdge
	{
		int32 SourceComponent = INDEX_NONE;
		int32 TargetComponent = INDEX_NONE;
	};

	class ANGELSCRIPTRUNTIME_API FAngelscriptModuleDependencyGraph
	{
	public:
		bool AddModule(FString StableModuleKey);
		bool AddEdge(FAngelscriptDependencyEdge Edge);
		bool Finalize();
		bool IsFinalized() const { return bFinalized; }
		TConstArrayView<FString> GetModules() const { return MakeArrayView(Modules); }
		TConstArrayView<FAngelscriptDependencyEdge> GetEdges() const { return MakeArrayView(Edges); }
		TConstArrayView<FAngelscriptDependencyComponent> GetComponents() const { return MakeArrayView(Components); }
		TConstArrayView<FAngelscriptCondensationEdge> GetCondensationEdges() const { return MakeArrayView(CondensationEdges); }
		FString SerializeStable() const;

	private:
		TArray<FString> Modules;
		TArray<FAngelscriptDependencyEdge> Edges;
		TArray<FAngelscriptDependencyComponent> Components;
		TArray<FAngelscriptCondensationEdge> CondensationEdges;
		bool bFinalized = false;
	};

	enum class EAngelscriptBodyReferenceReason : uint8
	{
		Call,
		Global,
		TypeReference,
	};

	struct FAngelscriptBodyInvalidationFact
	{
		FString SourceModuleKey;
		FString TargetModuleKey;
		asSStableKey StableFunctionKey;
		EAngelscriptBodyReferenceReason Reason = EAngelscriptBodyReferenceReason::Call;
		asCSourceRange SourceRange;
		FAngelscriptResolvedSourceAnchor StableSource;

		bool IsValid() const
		{
			return !SourceModuleKey.IsEmpty()
				&& !TargetModuleKey.IsEmpty()
				&& SourceModuleKey != TargetModuleKey
				&& !StableFunctionKey.IsZero()
				&& SourceRange.IsOrdered()
				&& StableSource.IsValid();
		}
	};

	class ANGELSCRIPTRUNTIME_API FAngelscriptBodyInvalidationGraph
	{
	public:
		bool AddFact(FAngelscriptBodyInvalidationFact Fact);
		bool Finalize();
		bool IsFinalized() const { return bFinalized; }
		TConstArrayView<FAngelscriptBodyInvalidationFact> GetFacts() const { return MakeArrayView(Facts); }
		FString SerializeStable() const;

	private:
		TArray<FAngelscriptBodyInvalidationFact> Facts;
		bool bFinalized = false;
	};

	ANGELSCRIPTRUNTIME_API bool BuildDeclarationDependencyGraph(
		const asCCompilationSession& Session,
		const asCSourceManager& SourceManager,
		FAngelscriptModuleDependencyGraph& OutGraph);

END_AS_NAMESPACE
