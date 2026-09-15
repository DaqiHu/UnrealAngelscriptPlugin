#include "frontend/AST/as_ast_context.h"
#include "frontend/AST/as_ast_verifier.h"
#include "frontend/AST/as_ast_visitor.h"
#include "frontend/AST/as_ast_cast.h"

#include <atomic>

BEGIN_AS_NAMESPACE

static std::atomic<uint64> GNextASTContextID{1};

	asCASTContext::asCASTContext(asCSourceManager InSources)
		: asCASTContext(MoveTemp(InSources), MakeShared<asCTypeContext, ESPMode::ThreadSafe>())
	{
	}

	asCASTContext::asCASTContext(asCSourceManager InSources,
		TSharedRef<asCTypeContext, ESPMode::ThreadSafe> InTypeContext)
		: ContextID(GNextASTContextID.fetch_add(1, std::memory_order_relaxed))
		, Sources(MoveTemp(InSources))
		, TypeContext(MoveTemp(InTypeContext))
	{
	}

	asCASTContext::~asCASTContext()
	{
		for (int32 Index = Allocations.Num() - 1; Index >= 0; --Index)
		{
			if (Allocations[Index].Destroy) Allocations[Index].Destroy(Allocations[Index].Address);
		}
	}

	bool asCASTContext::ValidateTypeIdentity(const asCASTNode& Node) const
	{
		const asCType* Type = dyn_cast<asCType>(&Node);
		if (!Type || isa<asCErrorType>(Type)) return true;
		asCStableIdentityRegistry& Registry = TypeContext->GetIdentityRegistry();
		asSStableTypeUseDescriptor Descriptor;
		if (Registry.ResolveTypeUse(Type->GetStableIdentity(), Descriptor) != asETypeIdentityStatus::Succeeded) return false;
		if (const auto* Builtin = dyn_cast<asCBuiltinType>(Type))
			return Descriptor.Kind == asETypeUseKind::Primitive && Descriptor.Primitive == Builtin->GetPrimitive() && Descriptor.Qualifiers == asETypeUseQualifiers::None;
		if (const auto* Nominal = dyn_cast<asCNominalType>(Type))
			return Descriptor.Kind == asETypeUseKind::Nominal && Descriptor.Declaration == Nominal->GetDeclarationIdentity() && Descriptor.Arguments.IsEmpty() && Descriptor.Qualifiers == asETypeUseQualifiers::None;
		if (const auto* Function = dyn_cast<asCFunctionType>(Type))
		{
			if (Descriptor.Kind != asETypeUseKind::Callable || Descriptor.Qualifiers != asETypeUseQualifiers::None || Descriptor.Parameters.Num() != Function->GetParameterTypes().Num() || Descriptor.Parameters.Num() != Function->GetParameterPassingModes().Num()) return false;
			if (!Owns(Function->GetReturnType().GetType())) return false;
			asSStableKey Return;
			if (Function->GetReturnType().GetStableIdentity(Registry, Return) != asETypeIdentityStatus::Succeeded || Return != Descriptor.ReturnType) return false;
			for (int32 Index = 0; Index < Descriptor.Parameters.Num(); ++Index)
			{
				if (!Owns(Function->GetParameterTypes()[Index].GetType()) || Function->GetParameterPassingModes()[Index] != Descriptor.Parameters[Index].PassingMode) return false;
				asSStableKey Parameter;
				if (Function->GetParameterTypes()[Index].GetStableIdentity(Registry, Parameter) != asETypeIdentityStatus::Succeeded || Parameter != Descriptor.Parameters[Index].Type) return false;
			}
			return true;
		}
		if (const auto* Generic = dyn_cast<asCGenericType>(Type))
		{
			const auto* Template = dyn_cast<asCNominalType>(Generic->GetTemplateType());
			if (!Template || !Owns(Template) || Descriptor.Kind != asETypeUseKind::Nominal || Descriptor.Declaration != Template->GetDeclarationIdentity() || Descriptor.Arguments.Num() != Generic->GetArguments().Num()) return false;
			for (int32 Index = 0; Index < Descriptor.Arguments.Num(); ++Index)
			{
				if (!Owns(Generic->GetArguments()[Index].GetType())) return false;
				asSStableKey Argument;
				if (Generic->GetArguments()[Index].GetStableIdentity(Registry, Argument) != asETypeIdentityStatus::Succeeded || Argument != Descriptor.Arguments[Index]) return false;
			}
			return Descriptor.Qualifiers == asETypeUseQualifiers::None;
		}
		return false;
	}

	asCBuiltinType* asCASTContext::GetBuiltinType(asEPrimitiveType Primitive)
	{
		FScopeLock Lock(&MutationMutex);
		const uint8 Key = static_cast<uint8>(Primitive);
		if (asCBuiltinType** Existing = BuiltinTypes.Find(Key)) return *Existing;
		if (bSealed || bFrozenForTooling) return nullptr;
		asSStableKey Identity;
		if (TypeContext->InternPrimitiveTypeUse(Primitive, Identity) != asETypeIdentityStatus::Succeeded) return nullptr;
		asCBuiltinType* Created = CreateLocked<asCBuiltinType>(Primitive, Identity);
		if (Created && !Created->IsInvalid())
		{
			BuiltinTypes.Add(Key, Created);
			CanonicalTypes.Add(Identity, Created);
		}
		else Created = nullptr;
		return Created;
	}

	asCNominalType* asCASTContext::GetNominalType(
		const asSStableKey& DeclarationIdentity,
		const asSStableKey& TypeUseIdentity)
	{
		FScopeLock Lock(&MutationMutex);
		asCStableIdentityRegistry& Registry = TypeContext->GetIdentityRegistry();
		asSStableTypeUseDescriptor Descriptor;
		if (Registry.ResolveTypeUse(TypeUseIdentity, Descriptor) != asETypeIdentityStatus::Succeeded
			|| Descriptor.Kind != asETypeUseKind::Nominal
			|| Descriptor.Declaration != DeclarationIdentity
			|| !Descriptor.Arguments.IsEmpty()
			|| Descriptor.Qualifiers != asETypeUseQualifiers::None)
		{
			return nullptr;
		}
		asSNominalTypeDescriptor Declaration;
		if (Registry.ResolveNominalType(DeclarationIdentity, Declaration) != asETypeIdentityStatus::Succeeded)
			return nullptr;
		if (asCType** Existing = CanonicalTypes.Find(TypeUseIdentity))
		{
			asCNominalType* Nominal = dyn_cast<asCNominalType>(*Existing);
			return Nominal && Nominal->GetDeclarationIdentity() == DeclarationIdentity ? Nominal : nullptr;
		}
		if (bSealed || bFrozenForTooling) return nullptr;
		asCNominalType* Created = CreateLocked<asCNominalType>(DeclarationIdentity, TypeUseIdentity);
		if (!Created || Created->IsInvalid()) return nullptr;
		CanonicalTypes.Add(TypeUseIdentity, Created);
		return Created;
	}

	asCGenericType* asCASTContext::GetGenericType(
		asCType* TemplateType,
		TArray<asCQualType> Arguments,
		const asSStableKey& TypeUseIdentity)
	{
		FScopeLock Lock(&MutationMutex);
		asCStableIdentityRegistry& Registry = TypeContext->GetIdentityRegistry();
		const asCNominalType* NominalTemplate = dyn_cast<asCNominalType>(TemplateType);
		asSStableTypeUseDescriptor Descriptor;
		if (!NominalTemplate || !Owns(NominalTemplate)
			|| Registry.ResolveTypeUse(TypeUseIdentity, Descriptor) != asETypeIdentityStatus::Succeeded
			|| Descriptor.Kind != asETypeUseKind::Nominal
			|| Descriptor.Declaration != NominalTemplate->GetDeclarationIdentity()
			|| Descriptor.Arguments.IsEmpty()
			|| Descriptor.Arguments.Num() != Arguments.Num()
			|| Descriptor.Qualifiers != asETypeUseQualifiers::None)
		{
			return nullptr;
		}
		for (int32 Index = 0; Index < Arguments.Num(); ++Index)
		{
			asSStableKey ArgumentIdentity;
			if (!Arguments[Index].IsValid() || !Owns(Arguments[Index].GetType())
				|| Arguments[Index].GetStableIdentity(Registry, ArgumentIdentity) != asETypeIdentityStatus::Succeeded
				|| ArgumentIdentity != Descriptor.Arguments[Index])
			{
				return nullptr;
			}
		}
		if (asCType** Existing = CanonicalTypes.Find(TypeUseIdentity))
			return dyn_cast<asCGenericType>(*Existing);
		if (bSealed || bFrozenForTooling) return nullptr;
		asCGenericType* Created = CreateLocked<asCGenericType>(TemplateType, MoveTemp(Arguments), TypeUseIdentity);
		if (!Created || Created->IsInvalid()) return nullptr;
		CanonicalTypes.Add(TypeUseIdentity, Created);
		return Created;
	}

	asCFunctionType* asCASTContext::GetFunctionType(
		asCQualType ReturnType,
		TArray<asCQualType> Parameters,
		const asSStableKey& TypeUseIdentity,
		TArray<asEParameterPassingMode> PassingModes)
	{
		FScopeLock Lock(&MutationMutex);
		if (PassingModes.IsEmpty()) PassingModes.Init(asEParameterPassingMode::Value, Parameters.Num());
		asCStableIdentityRegistry& Registry = TypeContext->GetIdentityRegistry();
		asSStableTypeUseDescriptor Descriptor;
		asSStableKey ReturnIdentity;
		if (!ReturnType.IsValid() || !Owns(ReturnType.GetType())
			|| ReturnType.GetStableIdentity(Registry, ReturnIdentity) != asETypeIdentityStatus::Succeeded
			|| Registry.ResolveTypeUse(TypeUseIdentity, Descriptor) != asETypeIdentityStatus::Succeeded
			|| Descriptor.Kind != asETypeUseKind::Callable
			|| Descriptor.ReturnType != ReturnIdentity
			|| Descriptor.Parameters.Num() != Parameters.Num()
			|| PassingModes.Num() != Parameters.Num()
			|| Descriptor.Qualifiers != asETypeUseQualifiers::None)
		{
			return nullptr;
		}
		for (int32 Index = 0; Index < Parameters.Num(); ++Index)
		{
			asSStableKey ParameterIdentity;
			if (!Parameters[Index].IsValid() || !Owns(Parameters[Index].GetType())
				|| PassingModes[Index] > asEParameterPassingMode::InOut
				|| Parameters[Index].GetStableIdentity(Registry, ParameterIdentity) != asETypeIdentityStatus::Succeeded
				|| Descriptor.Parameters[Index].Type != ParameterIdentity
				|| Descriptor.Parameters[Index].PassingMode != PassingModes[Index])
			{
				return nullptr;
			}
		}
		if (asCType** Existing = CanonicalTypes.Find(TypeUseIdentity))
			return dyn_cast<asCFunctionType>(*Existing);
		if (bSealed || bFrozenForTooling) return nullptr;
		asCFunctionType* Created = CreateLocked<asCFunctionType>(
			ReturnType, MoveTemp(Parameters), TypeUseIdentity, MoveTemp(PassingModes));
		if (!Created || Created->IsInvalid()) return nullptr;
		CanonicalTypes.Add(TypeUseIdentity, Created);
		return Created;
	}

	asCASTNodeHandle asCASTContext::GetHandle(const asCASTNode* Node) const
	{
		return Owns(Node) ? asCASTNodeHandle{ContextID, Node->GetContextLocalID()} : asCASTNodeHandle{};
	}

	asCASTNode* asCASTContext::Resolve(asCASTNodeHandle Handle) const
	{
		FScopeLock Lock(&MutationMutex);
		if (Handle.OwnerID != ContextID || Handle.LocalID == 0 || Handle.LocalID > static_cast<uint32>(Nodes.Num())) return nullptr;
		return Nodes[static_cast<int32>(Handle.LocalID - 1)];
	}

	bool asCASTContext::IsSealed() const
	{
		FScopeLock Lock(&MutationMutex);
		return bSealed;
	}

	bool asCASTContext::IsFrozenForTooling() const
	{
		FScopeLock Lock(&MutationMutex);
		return bFrozenForTooling;
	}

	bool asCASTContext::FreezeForTooling()
	{
		FScopeLock Lock(&MutationMutex);
		if (bSealed)
		{
			return false;
		}
		if (bFrozenForTooling)
		{
			return true;
		}
		const uint64 SnapshotID = Sources.GetSnapshotID();
		for (asCASTNode* Node : Nodes)
		{
			if (!Node || Node->GetOwnerContext() != this)
			{
				return false;
			}
			if (Node->GetFamily() != asEASTNodeFamily::Type)
			{
				const asCSourceRange& Range = Node->GetSourceRange();
				TConstArrayView<uint8> Bytes;
				if (!Range.IsOrdered()
					|| Range.Begin.SnapshotID != SnapshotID
					|| !Sources.TryGetBytes(Range.Begin.FileID, Bytes)
					|| Range.End.ByteOffset > static_cast<asUINT>(Bytes.Num()))
				{
					return false;
				}
			}
			TArray<const asCASTNode*> Children;
			if (asCASTVisitor::CollectOwningChildren(*Node, Children))
			{
				for (const asCASTNode* Child : Children)
				{
					if (Child && Child->GetOwnerContext() != this)
					{
						return false;
					}
				}
			}
			TArray<asSASTCrossReference> CrossReferences;
			if (asCASTVisitor::CollectCrossReferences(*Node, CrossReferences))
			{
				for (const asSASTCrossReference& Reference : CrossReferences)
				{
					if (Reference.Target && Reference.Target->GetOwnerContext() != this)
					{
						return false;
					}
				}
			}
			TArray<asSASTSemanticTypeReference> TypeReferences;
			if (asCASTVisitor::CollectSemanticTypeReferences(*Node, TypeReferences))
			{
				for (const asSASTSemanticTypeReference& Reference : TypeReferences)
				{
					const asCType* Type = Reference.Type.GetType();
					if (Type && Type->GetOwnerContext() != this)
					{
						return false;
					}
				}
			}
		}
		bFrozenForTooling = true;
		return true;
	}

	int32 asCASTContext::GetNodeCount() const
	{
		FScopeLock Lock(&MutationMutex);
		return Nodes.Num();
	}

	bool asCASTContext::VerifyAndSeal(const asCASTNode* Root, asSASTVerificationResult& OutResult)
	{
		// Callers join all independent body workers before this exclusive barrier.
		// Per-node mutation is deliberately not made into an arbitrary shared-graph
		// concurrency contract.
		FScopeLock Lock(&MutationMutex);
		if (bSealed || !asCASTVerifier::Verify(*this, Root, OutResult)) return false;
		bSealed = true;
		for (asCASTNode* Node : Nodes) Node->SealNode();
		return true;
	}

END_AS_NAMESPACE
