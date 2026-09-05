#include "frontend/as_dependency_graph.h"

#include "frontend/as_ast_cast.h"
#include "frontend/as_attr.h"
#include "frontend/as_compilation_session.h"
#include "frontend/as_type.h"

#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Templates/Function.h"

BEGIN_AS_NAMESPACE

namespace
	{
		int32 CompareText(const FString& Left, const FString& Right)
		{
			return Left.Compare(Right, ESearchCase::CaseSensitive);
		}

		bool EdgeLess(const FAngelscriptDependencyEdge& Left, const FAngelscriptDependencyEdge& Right)
		{
			if (const int32 C = CompareText(Left.SourceModuleKey, Right.SourceModuleKey); C != 0) return C < 0;
			if (const int32 C = CompareText(Left.TargetModuleKey, Right.TargetModuleKey); C != 0) return C < 0;
			if (Left.Reason != Right.Reason) return Left.Reason < Right.Reason;
			if (const int32 C = CompareText(Left.StableSource.LogicalSourceKey, Right.StableSource.LogicalSourceKey); C != 0) return C < 0;
			if (Left.StableSource.ContentRevision != Right.StableSource.ContentRevision) return Left.StableSource.ContentRevision < Right.StableSource.ContentRevision;
			if (Left.StableSource.BeginByteOffset != Right.StableSource.BeginByteOffset) return Left.StableSource.BeginByteOffset < Right.StableSource.BeginByteOffset;
			if (Left.StableSource.EndByteOffset != Right.StableSource.EndByteOffset) return Left.StableSource.EndByteOffset < Right.StableSource.EndByteOffset;
			return Left.Completeness < Right.Completeness;
		}

		bool SameEdge(const FAngelscriptDependencyEdge& Left, const FAngelscriptDependencyEdge& Right)
		{
			return !EdgeLess(Left, Right) && !EdgeLess(Right, Left);
		}

		bool FactLess(const FAngelscriptBodyInvalidationFact& Left, const FAngelscriptBodyInvalidationFact& Right)
		{
			if (const int32 C = CompareText(Left.SourceModuleKey, Right.SourceModuleKey); C != 0) return C < 0;
			if (const int32 C = CompareText(Left.TargetModuleKey, Right.TargetModuleKey); C != 0) return C < 0;
			if (const int32 C = FMemory::Memcmp(Left.StableFunctionKey.GetBytes().GetData(), Right.StableFunctionKey.GetBytes().GetData(), asSStableKey::ByteCount); C != 0) return C < 0;
			if (Left.Reason != Right.Reason) return Left.Reason < Right.Reason;
			if (const int32 C = CompareText(Left.StableSource.LogicalSourceKey, Right.StableSource.LogicalSourceKey); C != 0) return C < 0;
			if (Left.StableSource.ContentRevision != Right.StableSource.ContentRevision) return Left.StableSource.ContentRevision < Right.StableSource.ContentRevision;
			if (Left.StableSource.BeginByteOffset != Right.StableSource.BeginByteOffset) return Left.StableSource.BeginByteOffset < Right.StableSource.BeginByteOffset;
			return Left.StableSource.EndByteOffset < Right.StableSource.EndByteOffset;
		}

		bool SameFact(const FAngelscriptBodyInvalidationFact& Left, const FAngelscriptBodyInvalidationFact& Right)
		{
			return !FactLess(Left, Right) && !FactLess(Right, Left);
		}

		bool MakeStableSource(
			const asCSourceManager& SourceManager,
			const asCSourceRange& Range,
			FAngelscriptResolvedSourceAnchor& Out)
		{
			asSStableSourceAnchor Stable;
			if (!SourceManager.TryCreateStableAnchor(Range, Stable)) return false;
			Out.LogicalSourceKey = MoveTemp(Stable.LogicalSourceKey);
			Out.ContentRevision = Stable.ContentRevision;
			Out.BeginByteOffset = Stable.BeginByteOffset;
			Out.EndByteOffset = Stable.EndByteOffset;
			return true;
		}

		void WalkDeclarations(
			TConstArrayView<asCDecl*> Declarations,
			TFunctionRef<void(const asCDecl&)> Visitor)
		{
			for (const asCDecl* Declaration : Declarations)
			{
				if (!Declaration) continue;
				Visitor(*Declaration);
				if (const asCNamespaceDecl* Namespace = dyn_cast<asCNamespaceDecl>(Declaration))
					WalkDeclarations(Namespace->GetDecls(), Visitor);
				else if (const asCRecordDecl* Record = dyn_cast<asCRecordDecl>(Declaration))
					WalkDeclarations(Record->GetDecls(), Visitor);
				else if (const asCCallableTypeDecl* Callable = dyn_cast<asCCallableTypeDecl>(Declaration))
					WalkDeclarations(Callable->GetDecls(), Visitor);
			}
		}
	}

	bool FAngelscriptModuleDependencyGraph::AddModule(FString StableModuleKey)
	{
		if (bFinalized || StableModuleKey.IsEmpty()) return false;
		Modules.Add(MoveTemp(StableModuleKey));
		return true;
	}

	bool FAngelscriptModuleDependencyGraph::AddEdge(FAngelscriptDependencyEdge Edge)
	{
		if (bFinalized || !Edge.IsValid()) return false;
		Edges.Add(MoveTemp(Edge));
		return true;
	}

	bool FAngelscriptModuleDependencyGraph::Finalize()
	{
		if (bFinalized) return false;
		Modules.Sort([](const FString& Left, const FString& Right) { return CompareText(Left, Right) < 0; });
		for (int32 Index = Modules.Num() - 1; Index > 0; --Index)
		{
			if (Modules[Index] == Modules[Index - 1]) Modules.RemoveAt(Index);
		}
		if (Modules.IsEmpty()) return false;

		TMap<FString, int32> ModuleIndex;
		for (int32 Index = 0; Index < Modules.Num(); ++Index) ModuleIndex.Add(Modules[Index], Index);
		for (const FAngelscriptDependencyEdge& Edge : Edges)
		{
			if (!Edge.IsValid() || !ModuleIndex.Contains(Edge.SourceModuleKey) || !ModuleIndex.Contains(Edge.TargetModuleKey)) return false;
		}
		Edges.Sort(EdgeLess);
		for (int32 Index = Edges.Num() - 1; Index > 0; --Index)
		{
			if (SameEdge(Edges[Index], Edges[Index - 1])) Edges.RemoveAt(Index);
		}

		TArray<TArray<int32>> Adjacency;
		Adjacency.SetNum(Modules.Num());
		for (const FAngelscriptDependencyEdge& Edge : Edges)
		{
			Adjacency[*ModuleIndex.Find(Edge.SourceModuleKey)].AddUnique(*ModuleIndex.Find(Edge.TargetModuleKey));
		}
		for (TArray<int32>& Neighbors : Adjacency) Neighbors.Sort();

		TArray<int32> Indices;
		TArray<int32> LowLinks;
		TArray<int32> Stack;
		TArray<bool> OnStack;
		TArray<int32> RawComponentByModule;
		Indices.Init(INDEX_NONE, Modules.Num());
		LowLinks.Init(INDEX_NONE, Modules.Num());
		OnStack.Init(false, Modules.Num());
		RawComponentByModule.Init(INDEX_NONE, Modules.Num());
		TArray<FAngelscriptDependencyComponent> RawComponents;
		int32 NextIndex = 0;
		TFunction<void(int32)> Visit = [&](int32 Vertex)
		{
			Indices[Vertex] = LowLinks[Vertex] = NextIndex++;
			Stack.Add(Vertex);
			OnStack[Vertex] = true;
			for (const int32 Neighbor : Adjacency[Vertex])
			{
				if (Indices[Neighbor] == INDEX_NONE)
				{
					Visit(Neighbor);
					LowLinks[Vertex] = FMath::Min(LowLinks[Vertex], LowLinks[Neighbor]);
				}
				else if (OnStack[Neighbor]) LowLinks[Vertex] = FMath::Min(LowLinks[Vertex], Indices[Neighbor]);
			}
			if (LowLinks[Vertex] != Indices[Vertex]) return;

			const int32 ComponentIndex = RawComponents.Num();
			FAngelscriptDependencyComponent& Component = RawComponents.AddDefaulted_GetRef();
			for (;;)
			{
				const int32 Member = Stack.Pop(EAllowShrinking::No);
				OnStack[Member] = false;
				RawComponentByModule[Member] = ComponentIndex;
				Component.Members.Add(Modules[Member]);
				if (Member == Vertex) break;
			}
			Component.Members.Sort([](const FString& Left, const FString& Right) { return CompareText(Left, Right) < 0; });
		};
		for (int32 Vertex = 0; Vertex < Modules.Num(); ++Vertex)
		{
			if (Indices[Vertex] == INDEX_NONE) Visit(Vertex);
		}

		TArray<TSet<int32>> RawDag;
		TArray<int32> Indegree;
		RawDag.SetNum(RawComponents.Num());
		Indegree.Init(0, RawComponents.Num());
		for (const FAngelscriptDependencyEdge& Edge : Edges)
		{
			const int32 Source = RawComponentByModule[*ModuleIndex.Find(Edge.SourceModuleKey)];
			const int32 Target = RawComponentByModule[*ModuleIndex.Find(Edge.TargetModuleKey)];
			if (Source != Target && !RawDag[Source].Contains(Target))
			{
				RawDag[Source].Add(Target);
				++Indegree[Target];
			}
		}

		TArray<int32> Ready;
		for (int32 Component = 0; Component < RawComponents.Num(); ++Component)
			if (Indegree[Component] == 0) Ready.Add(Component);
		auto SortReady = [&]()
		{
			Ready.Sort([&](int32 Left, int32 Right)
			{
				return CompareText(RawComponents[Left].Members[0], RawComponents[Right].Members[0]) < 0;
			});
		};
		SortReady();
		TArray<int32> OrderedRawComponents;
		while (!Ready.IsEmpty())
		{
			const int32 Current = Ready[0];
			Ready.RemoveAt(0);
			OrderedRawComponents.Add(Current);
			TArray<int32> Targets = RawDag[Current].Array();
			Targets.Sort();
			for (const int32 Target : Targets)
			{
				if (--Indegree[Target] == 0) Ready.Add(Target);
			}
			SortReady();
		}
		if (OrderedRawComponents.Num() != RawComponents.Num()) return false;

		TArray<int32> FinalComponentByRaw;
		FinalComponentByRaw.Init(INDEX_NONE, RawComponents.Num());
		for (int32 FinalIndex = 0; FinalIndex < OrderedRawComponents.Num(); ++FinalIndex)
		{
			const int32 RawIndex = OrderedRawComponents[FinalIndex];
			FinalComponentByRaw[RawIndex] = FinalIndex;
			Components.Add(MoveTemp(RawComponents[RawIndex]));
		}
		for (int32 RawSource = 0; RawSource < RawDag.Num(); ++RawSource)
		{
			for (const int32 RawTarget : RawDag[RawSource])
			{
				FAngelscriptCondensationEdge& Edge = CondensationEdges.AddDefaulted_GetRef();
				Edge.SourceComponent = FinalComponentByRaw[RawSource];
				Edge.TargetComponent = FinalComponentByRaw[RawTarget];
			}
		}
		CondensationEdges.Sort([](const FAngelscriptCondensationEdge& Left, const FAngelscriptCondensationEdge& Right)
		{
			return Left.SourceComponent != Right.SourceComponent
				? Left.SourceComponent < Right.SourceComponent
				: Left.TargetComponent < Right.TargetComponent;
		});
		bFinalized = true;
		return true;
	}

	FString FAngelscriptModuleDependencyGraph::SerializeStable() const
	{
		if (!bFinalized) return {};
		FString Result;
		for (const FString& Module : Modules) Result += TEXT("module|") + Module + TEXT("\n");
		for (const FAngelscriptDependencyEdge& Edge : Edges)
		{
			Result += FString::Printf(TEXT("edge|%s|%s|%u|%s|%llu|%u|%u|%u\n"),
				*Edge.SourceModuleKey, *Edge.TargetModuleKey, static_cast<uint8>(Edge.Reason),
				*Edge.StableSource.LogicalSourceKey,
				static_cast<unsigned long long>(Edge.StableSource.ContentRevision),
				Edge.StableSource.BeginByteOffset, Edge.StableSource.EndByteOffset,
				static_cast<uint8>(Edge.Completeness));
		}
		for (int32 Index = 0; Index < Components.Num(); ++Index)
			Result += FString::Printf(TEXT("component|%d|%s\n"), Index, *FString::Join(Components[Index].Members, TEXT(",")));
		for (const FAngelscriptCondensationEdge& Edge : CondensationEdges)
			Result += FString::Printf(TEXT("dag|%d|%d\n"), Edge.SourceComponent, Edge.TargetComponent);
		return Result;
	}

	bool FAngelscriptBodyInvalidationGraph::AddFact(FAngelscriptBodyInvalidationFact Fact)
	{
		if (bFinalized || !Fact.IsValid()) return false;
		Facts.Add(MoveTemp(Fact));
		return true;
	}

	bool FAngelscriptBodyInvalidationGraph::Finalize()
	{
		if (bFinalized) return false;
		Facts.Sort(FactLess);
		for (int32 Index = Facts.Num() - 1; Index > 0; --Index)
		{
			if (SameFact(Facts[Index], Facts[Index - 1])) Facts.RemoveAt(Index);
		}
		bFinalized = true;
		return true;
	}

	FString FAngelscriptBodyInvalidationGraph::SerializeStable() const
	{
		if (!bFinalized) return {};
		FString Result;
		for (const FAngelscriptBodyInvalidationFact& Fact : Facts)
		{
			Result += FString::Printf(TEXT("body|%s|%s|%s|%u|%s|%llu|%u|%u\n"),
				*Fact.SourceModuleKey, *Fact.TargetModuleKey, *Fact.StableFunctionKey.ToHexString(),
				static_cast<uint8>(Fact.Reason), *Fact.StableSource.LogicalSourceKey,
				static_cast<unsigned long long>(Fact.StableSource.ContentRevision),
				Fact.StableSource.BeginByteOffset, Fact.StableSource.EndByteOffset);
		}
		return Result;
	}

	bool BuildDeclarationDependencyGraph(
		const asCCompilationSession& Session,
		const asCSourceManager& SourceManager,
		FAngelscriptModuleDependencyGraph& OutGraph)
	{
		if (!Session.IsPublishable() || Session.GetPhase() != asECompilationPhase::DeclarationsResolved) return false;
		TMap<asSStableKey, FString> ModuleByTypeIdentity;
		TMap<asSStableKey, asETypeDeclKind> KindByTypeIdentity;
		for (const asCDeclarationFragment& Fragment : Session.GetFragments())
		{
			OutGraph.AddModule(Fragment.GetLogicalSourceKey());
			WalkDeclarations(Fragment.GetTopLevelDecls(), [&](const asCDecl& Declaration)
			{
				if (const asCRecordDecl* Record = dyn_cast<asCRecordDecl>(&Declaration);
					Record && !Record->GetTypeIdentity().IsZero())
				{
					const asSStableKey TypeIdentity = Record->GetTypeIdentity();
					ModuleByTypeIdentity.Add(TypeIdentity, Fragment.GetLogicalSourceKey());
					KindByTypeIdentity.Add(TypeIdentity, Record->GetDeclaredTypeKind());
				}
				else if (const asCCallableTypeDecl* Callable = dyn_cast<asCCallableTypeDecl>(&Declaration);
					Callable && !Callable->GetTypeIdentity().IsZero())
				{
					const asSStableKey TypeIdentity = Callable->GetTypeIdentity();
					ModuleByTypeIdentity.Add(TypeIdentity, Fragment.GetLogicalSourceKey());
					KindByTypeIdentity.Add(TypeIdentity, Callable->GetDeclaredTypeKind());
				}
			});
		}

		TFunction<bool(
			const asCType*,
			const FString&,
			EAngelscriptDependencyReason,
			EAngelscriptCompletenessRequirement,
			const asCSourceRange&)> AddType;
		AddType = [&](const asCType* Type, const FString& SourceModule, EAngelscriptDependencyReason Reason,
			EAngelscriptCompletenessRequirement Completeness, const asCSourceRange& Range) -> bool
		{
			if (!Type || Type->IsInvalid()) return false;
			if (const asCNominalType* Nominal = dyn_cast<asCNominalType>(Type))
			{
				const asSStableKey TypeIdentity = Nominal->GetDeclarationIdentity();
				const FString* Target = ModuleByTypeIdentity.Find(TypeIdentity);
				if (!Target || *Target == SourceModule) return true;
				FAngelscriptDependencyEdge Edge;
				Edge.SourceModuleKey = SourceModule;
				Edge.TargetModuleKey = *Target;
				const asETypeDeclKind* TargetKind = KindByTypeIdentity.Find(TypeIdentity);
				Edge.Reason = Reason == EAngelscriptDependencyReason::Base
					&& TargetKind && *TargetKind == asETypeDeclKind::Interface
					? EAngelscriptDependencyReason::Interface
					: Reason;
				Edge.SourceRange = Range;
				Edge.Completeness = Completeness;
				if (!MakeStableSource(SourceManager, Range, Edge.StableSource)) return false;
				return OutGraph.AddEdge(MoveTemp(Edge));
			}
			if (const asCGenericType* Generic = dyn_cast<asCGenericType>(Type))
			{
				if (!AddType(Generic->GetTemplateType(), SourceModule, Reason, Completeness, Range)) return false;
				for (const asCQualType& Argument : Generic->GetArguments())
				{
					if (!AddType(Argument.GetType(), SourceModule, EAngelscriptDependencyReason::GenericArgument, Completeness, Range)) return false;
				}
			}
			return true;
		};

		for (const asCDeclarationFragment& Fragment : Session.GetFragments())
		{
			const FString& Module = Fragment.GetLogicalSourceKey();
			bool bValid = true;
			WalkDeclarations(Fragment.GetTopLevelDecls(), [&](const asCDecl& Declaration)
			{
				if (!bValid || Declaration.IsInvalid()) { bValid = false; return; }
				if (const asCRecordDecl* Record = dyn_cast<asCRecordDecl>(&Declaration))
				{
					for (const asCType* Base : Record->GetResolvedBases())
						bValid &= AddType(Base, Module, EAngelscriptDependencyReason::Base, EAngelscriptCompletenessRequirement::CompleteDefinition, Record->GetSourceRange());
				}
				else if (const asCVarDecl* Variable = dyn_cast<asCVarDecl>(&Declaration))
				{
					if (Variable->GetKind() != asEDeclKind::ParmVar)
						bValid &= AddType(Variable->GetResolvedType(), Module, EAngelscriptDependencyReason::PropertyType, EAngelscriptCompletenessRequirement::CompleteDefinition, Variable->GetSourceRange());
				}
				else if (const asCFunctionDecl* Function = dyn_cast<asCFunctionDecl>(&Declaration))
				{
					bValid &= AddType(Function->GetResolvedReturnType(), Module, EAngelscriptDependencyReason::ReturnType, EAngelscriptCompletenessRequirement::Signature, Function->GetSourceRange());
					for (const asCVarDecl* Parameter : Function->GetParameters())
					{
						bValid &= Parameter && AddType(Parameter->GetResolvedType(), Module, EAngelscriptDependencyReason::ParameterType, EAngelscriptCompletenessRequirement::Signature, Parameter ? Parameter->GetSourceRange() : Function->GetSourceRange());
					}
				}
				else if (const asCCallableTypeDecl* Callable = dyn_cast<asCCallableTypeDecl>(&Declaration))
				{
					const asCFunctionType* Signature = Callable->GetResolvedSignature();
					if (!Signature) { bValid = false; return; }
					bValid &= AddType(Signature->GetReturnType().GetType(), Module, EAngelscriptDependencyReason::DelegateSignature,
						EAngelscriptCompletenessRequirement::Signature, Callable->GetReturnTypeSyntax().Range);
					for (const asCVarDecl* Parameter : Callable->GetParameters())
						bValid &= AddType(Parameter->GetResolvedType(), Module, EAngelscriptDependencyReason::DelegateSignature,
							EAngelscriptCompletenessRequirement::Signature, Parameter->GetTypeSyntax().Range);
				}
			});
			if (!bValid) return false;
		}
		return OutGraph.Finalize();
	}

END_AS_NAMESPACE
